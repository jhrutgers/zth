// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

use alloc::boxed::Box;
use alloc::rc::Rc;
use alloc::vec::Vec;
use core::cell::{Cell, UnsafeCell};
use core::cmp::Eq;
use core::cmp::PartialEq;
use core::debug_assert;
use core::ffi::{c_int, c_void};
use core::fmt;
use core::fmt::Debug;
use core::hash::{Hash, Hasher};
use core::marker::PhantomData;
use core::mem;
use core::mem::{ManuallyDrop, MaybeUninit};
use core::ops::Drop;
use core::prelude::rust_2021::derive;
use core::ptr;
use core::result::Result;
use core::result::Result::{Err, Ok};
use core::stringify;

mod ffi {
    use super::{CondRaw, FutureRaw, GateRaw, MailboxRaw, MutexRaw, SemRaw};
    use core::ffi::c_int;

    extern "C" {
        pub fn zth_future_init(future: *mut FutureRaw) -> c_int;
        pub fn zth_future_destroy(future: *mut FutureRaw) -> c_int;
        pub fn zth_future_valid(future: *mut FutureRaw) -> c_int;
        pub fn zth_future_set(future: *mut FutureRaw, value: usize) -> c_int;
        pub fn zth_future_get(future: *mut FutureRaw, value: *mut usize) -> c_int;
        pub fn zth_future_wait(future: *mut FutureRaw) -> c_int;

        pub fn zth_mutex_init(mutex: *mut MutexRaw) -> c_int;
        pub fn zth_mutex_destroy(mutex: *mut MutexRaw) -> c_int;
        pub fn zth_mutex_lock(mutex: *mut MutexRaw) -> c_int;
        pub fn zth_mutex_trylock(mutex: *mut MutexRaw) -> c_int;
        pub fn zth_mutex_unlock(mutex: *mut MutexRaw) -> c_int;

        pub fn zth_sem_init(sem: *mut SemRaw, value: usize) -> c_int;
        pub fn zth_sem_destroy(sem: *mut SemRaw) -> c_int;
        pub fn zth_sem_getvalue(sem: *mut SemRaw, value: *mut usize) -> c_int;
        pub fn zth_sem_post(sem: *mut SemRaw) -> c_int;
        pub fn zth_sem_wait(sem: *mut SemRaw) -> c_int;
        pub fn zth_sem_trywait(sem: *mut SemRaw) -> c_int;

        pub fn zth_cond_init(cond: *mut CondRaw) -> c_int;
        pub fn zth_cond_destroy(cond: *mut CondRaw) -> c_int;
        pub fn zth_cond_signal(cond: *mut CondRaw) -> c_int;
        pub fn zth_cond_broadcast(cond: *mut CondRaw) -> c_int;
        pub fn zth_cond_wait(cond: *mut CondRaw) -> c_int;

        pub fn zth_gate_init(gate: *mut GateRaw, count: usize) -> c_int;
        pub fn zth_gate_destroy(gate: *mut GateRaw) -> c_int;
        pub fn zth_gate_pass(gate: *mut GateRaw) -> c_int;
        pub fn zth_gate_wait(gate: *mut GateRaw) -> c_int;

        pub fn zth_mailbox_init(mailbox: *mut MailboxRaw) -> c_int;
        pub fn zth_mailbox_destroy(mailbox: *mut MailboxRaw) -> c_int;
        pub fn zth_mailbox_can_take(mailbox: *mut MailboxRaw) -> c_int;
        pub fn zth_mailbox_can_put(mailbox: *mut MailboxRaw) -> c_int;
        pub fn zth_mailbox_put(mailbox: *mut MailboxRaw, value: usize) -> c_int;
        pub fn zth_mailbox_take(mailbox: *mut MailboxRaw, value: *mut usize) -> c_int;
    }
}

use crate::Error;

pub(crate) trait Synchronizer: core::marker::Sized {
    type Raw;

    fn from_raw(raw: Self::Raw) -> Self;
    fn raw_ptr(&self) -> *mut Self::Raw;
    unsafe fn ffi_destroy(raw: *mut Self::Raw) -> c_int;
    fn raw_null() -> Self::Raw;
    fn raw_is_null(raw: *const Self::Raw) -> bool;
    unsafe fn raw_clear(raw: *mut Self::Raw);

    fn null() -> Self {
        Self::from_raw(Self::raw_null())
    }

    fn is_null(&self) -> bool {
        Self::raw_is_null(self.raw_ptr().cast_const())
    }

    fn destroy_in_drop(&mut self) {
        if self.is_null() {
            return;
        }

        unsafe {
            let raw = self.raw_ptr();
            let _ = Self::ffi_destroy(raw);
            Self::raw_clear(raw);
        }
    }

    fn as_result<T>(rc: c_int, ok: T) -> Result<T, Error> {
        if rc == 0 {
            Ok(ok)
        } else {
            Err(Error(rc))
        }
    }
}

macro_rules! define_synchronizer_type {
    (@impl
        type $type_name:ident $(<$($gen:ident),+>)?
        $(where $($where:tt)+)?
        raw $raw_name:ident,
        destroy $ffi_destroy:path,
        marker $marker_ty:ty
    ) => {
        #[derive(Debug)]
        #[repr(C)]
        pub(crate) struct $raw_name {
            p: *mut c_void,
        }

        pub struct $type_name $(<$($gen),+>)?
        $(where $($where)+)?
        {
            raw: UnsafeCell<$raw_name>,
            _type_marker: PhantomData<$marker_ty>,
            _not_send_sync: PhantomData<Rc<()>>,
        }

        impl $type_name $(<$($gen),+>)?
        $(where $($where)+)?
        {
            fn handle(&self) -> *mut c_void {
                unsafe { (*self.raw.get()).p }
            }
        }

        impl $(<$($gen),+>)? Synchronizer for $type_name $(<$($gen),+>)?
        $(where $($where)+)?
        {
            type Raw = $raw_name;

            fn from_raw(raw: Self::Raw) -> Self {
                Self {
                    raw: UnsafeCell::new(raw),
                    _type_marker: PhantomData,
                    _not_send_sync: PhantomData,
                }
            }

            fn raw_ptr(&self) -> *mut Self::Raw {
                self.raw.get()
            }

            unsafe fn ffi_destroy(raw: *mut Self::Raw) -> c_int {
                $ffi_destroy(raw)
            }

            fn raw_null() -> Self::Raw {
                Self::Raw {
                    p: ptr::null_mut(),
                }
            }

            fn raw_is_null(raw: *const Self::Raw) -> bool {
                unsafe { (*raw).p.is_null() }
            }

            unsafe fn raw_clear(raw: *mut Self::Raw) {
                (*raw).p = ptr::null_mut();
            }
        }

        impl $(<$($gen),+>)? fmt::Debug for $type_name $(<$($gen),+>)?
        $(where $($where)+)?
        {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                f.debug_struct(stringify!($type_name))
                    .field("p", &self.handle())
                    .finish()
            }
        }

        impl $(<$($gen),+>)? PartialEq for $type_name $(<$($gen),+>)?
        $(where $($where)+)?
        {
            fn eq(&self, other: &Self) -> bool {
                self.handle() == other.handle()
            }
        }

        impl $(<$($gen),+>)? Eq for $type_name $(<$($gen),+>)?
        $(where $($where)+)?
        {}

        impl $(<$($gen),+>)? Hash for $type_name $(<$($gen),+>)?
        $(where $($where)+)?
        {
            fn hash<H: Hasher>(&self, state: &mut H) {
                self.handle().hash(state);
            }
        }

        impl $(<$($gen),+>)? Drop for $type_name $(<$($gen),+>)?
        $(where $($where)+)?
        {
            fn drop(&mut self) {
                self.destroy_in_drop();
            }
        }
    };

    ($type_name:ident, $raw_name:ident, $ffi_destroy:path) => {
        define_synchronizer_type!(
            @impl
            type $type_name
            raw $raw_name,
            destroy $ffi_destroy,
            marker ()
        );
    };

    ($type_name:ident<$($gen:ident),+>, $raw_name:ident, $ffi_destroy:path) => {
        define_synchronizer_type!(
            @impl
            type $type_name<$($gen),+>
            raw $raw_name,
            destroy $ffi_destroy,
            marker ($($gen,)+)
        );
    };

    ($type_name:ident<$($gen:ident),+> where $($where:tt)+, $raw_name:ident, $ffi_destroy:path) => {
        define_synchronizer_type!(
            @impl
            type $type_name<$($gen),+>
            where $($where)+
            raw $raw_name,
            destroy $ffi_destroy,
            marker ($($gen,)+)
        );
    };
}

// Usage examples:
// define_synchronizer_type!(Future, FutureRaw, ffi::zth_future_destroy);
// define_synchronizer_type!(FutureValue<T>, FutureValueRaw, ffi::zth_future_destroy);
// define_synchronizer_type!(FutureValue<T> where T: 'static, FutureValueRaw, ffi::zth_future_destroy);

define_synchronizer_type!(RawFuture, FutureRaw, ffi::zth_future_destroy);
define_synchronizer_type!(RawMailbox, MailboxRaw, ffi::zth_mailbox_destroy);

struct UintptrValueCodec<T>(PhantomData<T>);

impl<T> UintptrValueCodec<T>
where
    T: 'static,
{
    fn is_usize_mode() -> bool {
        mem::size_of::<T>() <= mem::size_of::<usize>()
            && mem::align_of::<T>() <= mem::align_of::<usize>()
    }

    fn requires_take_guard() -> bool {
        // Box-backed values and inline values with drop glue must only be
        // decoded once to avoid duplicate ownership.
        !Self::is_usize_mode() || mem::needs_drop::<T>()
    }

    fn encode_value(value: T) -> usize {
        if Self::is_usize_mode() {
            let value = ManuallyDrop::new(value);
            let mut raw = MaybeUninit::<usize>::zeroed();
            unsafe {
                ptr::copy_nonoverlapping(
                    (&*value as *const T).cast::<u8>(),
                    raw.as_mut_ptr().cast::<u8>(),
                    mem::size_of::<T>(),
                );
                raw.assume_init()
            }
        } else {
            Box::into_raw(Box::new(value)) as usize
        }
    }

    fn decode_value(raw: usize) -> T {
        if Self::is_usize_mode() {
            let mut value = MaybeUninit::<T>::uninit();
            unsafe {
                ptr::copy_nonoverlapping(
                    (&raw as *const usize).cast::<u8>(),
                    value.as_mut_ptr().cast::<u8>(),
                    mem::size_of::<T>(),
                );
                value.assume_init()
            }
        } else {
            let ptr = raw as *mut T;
            debug_assert!(!ptr.is_null());
            unsafe { *Box::from_raw(ptr) }
        }
    }

    fn drop_encoded_value(raw: usize) {
        if !Self::is_usize_mode() {
            let ptr = raw as *mut T;
            debug_assert!(!ptr.is_null());
            unsafe { core::mem::drop(Box::from_raw(ptr)) };
        } else if mem::needs_drop::<T>() {
            core::mem::drop(Self::decode_value(raw));
        }
    }
}

impl RawFuture {
    pub fn new() -> Result<Self, Error> {
        let h = Self::null();
        Self::as_result(unsafe { ffi::zth_future_init(h.raw_ptr()) }, h)
    }

    pub fn valid(&self) -> bool {
        unsafe { ffi::zth_future_valid(self.raw_ptr()) == 0 }
    }

    pub fn set(&self, value: usize) -> Result<(), Error> {
        Self::as_result(unsafe { ffi::zth_future_set(self.raw_ptr(), value) }, ())
    }

    pub fn get(&self) -> Result<usize, Error> {
        let mut value: usize = 0;
        Self::as_result(
            unsafe { ffi::zth_future_get(self.raw_ptr(), &mut value) },
            value,
        )
    }

    pub fn wait(&self) -> Result<(), Error> {
        Self::as_result(unsafe { ffi::zth_future_wait(self.raw_ptr()) }, ())
    }
}

impl RawMailbox {
    const EAGAIN: c_int = 11;

    pub fn new() -> Result<Self, Error> {
        let h = Self::null();
        Self::as_result(unsafe { ffi::zth_mailbox_init(h.raw_ptr()) }, h)
    }

    pub fn can_take(&self) -> Result<bool, Error> {
        match unsafe { ffi::zth_mailbox_can_take(self.raw_ptr()) } {
            0 => Ok(true),
            rc if rc == Self::EAGAIN => Ok(false),
            rc => Err(Error::from_raw_os_error(rc)),
        }
    }

    pub fn can_put(&self) -> Result<bool, Error> {
        match unsafe { ffi::zth_mailbox_can_put(self.raw_ptr()) } {
            0 => Ok(true),
            rc if rc == Self::EAGAIN => Ok(false),
            rc => Err(Error::from_raw_os_error(rc)),
        }
    }

    pub fn put(&self, value: usize) -> Result<(), Error> {
        Self::as_result(unsafe { ffi::zth_mailbox_put(self.raw_ptr(), value) }, ())
    }

    pub fn take(&self) -> Result<usize, Error> {
        let mut value: usize = 0;
        Self::as_result(
            unsafe { ffi::zth_mailbox_take(self.raw_ptr(), &mut value) },
            value,
        )
    }
}

/// Typed future wrapper.
///
/// - For values with size and alignment that fit in a `usize`, bytes are
///   stored directly in the native future.
/// - For larger values, a boxed pointer is stored as `usize` in the native
///   future.
pub struct Future<T = usize>
where
    T: 'static,
{
    raw: RawFuture,
    taken: Cell<bool>,
    _marker: PhantomData<T>,
}

impl<T> Future<T>
where
    T: 'static,
{
    #[cfg(all(zth_hosted_std, test))]
    fn is_usize_mode() -> bool {
        UintptrValueCodec::<T>::is_usize_mode()
    }

    fn requires_take_guard() -> bool {
        UintptrValueCodec::<T>::requires_take_guard()
    }

    fn encode_value(value: T) -> usize {
        UintptrValueCodec::<T>::encode_value(value)
    }

    fn decode_value(raw: usize) -> T {
        UintptrValueCodec::<T>::decode_value(raw)
    }

    pub fn new() -> Result<Self, Error> {
        Ok(Self {
            raw: RawFuture::new()?,
            taken: Cell::new(false),
            _marker: PhantomData,
        })
    }

    pub fn valid(&self) -> bool {
        self.raw.valid()
    }

    pub fn wait(&self) -> Result<(), Error> {
        self.raw.wait()
    }

    pub fn set(&self, value: T) -> Result<(), Error> {
        let encoded = Self::encode_value(value);
        match self.raw.set(encoded) {
            Ok(()) => Ok(()),
            Err(e) => {
                UintptrValueCodec::<T>::drop_encoded_value(encoded);
                Err(e)
            }
        }
    }

    pub fn get(&self) -> Result<T, Error> {
        if Self::requires_take_guard() && self.taken.replace(true) {
            return Err(Error::from_raw_os_error(22));
        }

        self.raw.get().map(Self::decode_value)
    }
}

/// Typed mailbox wrapper.
///
/// Like [`Future`], values are encoded as a single `usize` when possible,
/// otherwise boxed and passed as pointer-sized integer.
pub struct Mailbox<T = usize>
where
    T: 'static,
{
    raw: RawMailbox,
    boxed_pool: UnsafeCell<Vec<*mut T>>,
    _marker: PhantomData<T>,
}

impl<T> Mailbox<T>
where
    T: 'static,
{
    pub fn new() -> Result<Self, Error> {
        Ok(Self {
            raw: RawMailbox::new()?,
            boxed_pool: UnsafeCell::new(Vec::new()),
            _marker: PhantomData,
        })
    }

    fn is_usize_mode() -> bool {
        UintptrValueCodec::<T>::is_usize_mode()
    }

    fn boxed_pool_mut(&self) -> &mut Vec<*mut T> {
        unsafe { &mut *self.boxed_pool.get() }
    }

    fn boxed_alloc_slot(&self) -> *mut T {
        if let Some(slot) = self.boxed_pool_mut().pop() {
            slot
        } else {
            Box::into_raw(Box::<MaybeUninit<T>>::new_uninit()).cast::<T>()
        }
    }

    fn boxed_recycle_slot(&self, slot: *mut T) {
        debug_assert!(!slot.is_null());
        self.boxed_pool_mut().push(slot);
    }

    pub fn can_take(&self) -> Result<bool, Error> {
        self.raw.can_take()
    }

    pub fn can_put(&self) -> Result<bool, Error> {
        self.raw.can_put()
    }

    pub fn put(&self, value: T) -> Result<(), Error> {
        if Self::is_usize_mode() {
            let encoded = UintptrValueCodec::<T>::encode_value(value);
            return match self.raw.put(encoded) {
                Ok(()) => Ok(()),
                Err(e) => {
                    UintptrValueCodec::<T>::drop_encoded_value(encoded);
                    Err(e)
                }
            };
        }

        let slot = self.boxed_alloc_slot();
        unsafe { ptr::write(slot, value) };
        let encoded = slot as usize;

        match self.raw.put(encoded) {
            Ok(()) => Ok(()),
            Err(e) => {
                unsafe { ptr::drop_in_place(slot) };
                self.boxed_recycle_slot(slot);
                Err(e)
            }
        }
    }

    pub fn take(&self) -> Result<T, Error> {
        if Self::is_usize_mode() {
            return self.raw.take().map(UintptrValueCodec::<T>::decode_value);
        }

        let slot = self.raw.take()? as *mut T;
        debug_assert!(!slot.is_null());
        let value = unsafe { ptr::read(slot) };
        self.boxed_recycle_slot(slot);
        Ok(value)
    }
}

impl<T> core::ops::Drop for Mailbox<T>
where
    T: 'static,
{
    fn drop(&mut self) {
        if !UintptrValueCodec::<T>::requires_take_guard() {
            return;
        }

        if Self::is_usize_mode() {
            if let Ok(true) = self.raw.can_take() {
                if let Ok(raw) = self.raw.take() {
                    core::mem::drop(UintptrValueCodec::<T>::decode_value(raw));
                }
            }
        } else {
            if let Ok(true) = self.raw.can_take() {
                if let Ok(raw) = self.raw.take() {
                    let slot = raw as *mut T;
                    if !slot.is_null() {
                        unsafe { ptr::drop_in_place(slot) };
                        self.boxed_pool.get_mut().push(slot);
                    }
                }
            }

            let pool = self.boxed_pool.get_mut();
            for slot in pool.drain(..) {
                if !slot.is_null() {
                    unsafe {
                        core::mem::drop(Box::from_raw(slot.cast::<MaybeUninit<T>>()));
                    }
                }
            }
        }
    }
}

impl<T> core::ops::Drop for Future<T>
where
    T: 'static,
{
    fn drop(&mut self) {
        if Self::requires_take_guard() && self.raw.valid() && !self.taken.get() {
            if let Ok(raw) = self.raw.get() {
                core::mem::drop(Self::decode_value(raw));
            }
        }
    }
}

#[cfg(all(zth_hosted_std, test))]
mod tests {
    use super::Future;

    #[test]
    fn inline_values_round_trip() {
        assert!(Future::<u32>::is_usize_mode());
        assert!(!Future::<u32>::requires_take_guard());

        let encoded = Future::<u32>::encode_value(0x1234_5678);
        let decoded = Future::<u32>::decode_value(encoded);

        assert_eq!(decoded, 0x1234_5678);
    }

    #[test]
    fn bool_round_trip() {
        assert!(Future::<bool>::is_usize_mode());
        assert!(!Future::<bool>::requires_take_guard());

        let encoded = Future::<bool>::encode_value(true);
        let decoded = Future::<bool>::decode_value(encoded);

        assert!(decoded);
    }

    #[test]
    fn boxed_values_round_trip() {
        assert!(!Future::<String>::is_usize_mode());
        assert!(Future::<String>::requires_take_guard());

        let encoded = Future::<String>::encode_value("zth".to_string());
        let decoded = Future::<String>::decode_value(encoded);

        assert_eq!(decoded, "zth");
    }
}

define_synchronizer_type!(Mutex, MutexRaw, ffi::zth_mutex_destroy);

/// Fiber-aware mutex with RAII guard semantics.
///
/// This wrapper behaves similarly to Rust's standard mutex API:
/// - [`lock`](Self::lock) blocks until the lock is acquired and returns a guard.
/// - [`try_lock`](Self::try_lock) returns `Ok(None)` when lock is busy.
/// - The lock is released when the guard is dropped.
impl Mutex {
    const EBUSY: c_int = 16;

    pub fn new() -> Result<Self, Error> {
        let h = Self::null();
        Self::as_result(unsafe { ffi::zth_mutex_init(h.raw_ptr()) }, h)
    }

    fn lock_ffi(&self) -> Result<(), Error> {
        Self::as_result(unsafe { ffi::zth_mutex_lock(self.raw_ptr()) }, ())
    }

    fn try_lock_ffi(&self) -> Result<bool, Error> {
        match unsafe { ffi::zth_mutex_trylock(self.raw_ptr()) } {
            0 => Ok(true),
            rc if rc == Self::EBUSY => Ok(false),
            rc => Err(Error::from_raw_os_error(rc)),
        }
    }

    fn unlock_ffi(&self) -> Result<(), Error> {
        Self::as_result(unsafe { ffi::zth_mutex_unlock(self.raw_ptr()) }, ())
    }

    pub fn lock(&self) -> Result<MutexGuard<'_>, Error> {
        self.lock_ffi()?;
        Ok(MutexGuard { mutex: self })
    }

    pub fn try_lock(&self) -> Result<Option<MutexGuard<'_>>, Error> {
        if self.try_lock_ffi()? {
            Ok(Some(MutexGuard { mutex: self }))
        } else {
            Ok(None)
        }
    }
}

pub struct MutexGuard<'a> {
    mutex: &'a Mutex,
}

impl Drop for MutexGuard<'_> {
    fn drop(&mut self) {
        let rc = self.mutex.unlock_ffi();
        debug_assert!(rc.is_ok());
    }
}

define_synchronizer_type!(Semaphore, SemRaw, ffi::zth_sem_destroy);

/// Fiber-aware counting semaphore with RAII permit.
///
/// This wrapper follows Rust semaphore-style APIs:
/// - [`acquire`](Self::acquire) blocks and returns a permit.
/// - [`try_acquire`](Self::try_acquire) returns `Ok(None)` if no permit is available.
/// - Dropping the permit releases it back to the semaphore.
impl Semaphore {
    const EAGAIN: c_int = 11;

    pub fn new(value: usize) -> Result<Self, Error> {
        let h = Self::null();
        Self::as_result(unsafe { ffi::zth_sem_init(h.raw_ptr(), value) }, h)
    }

    pub fn available_permits(&self) -> Result<usize, Error> {
        let mut value: usize = 0;
        Self::as_result(
            unsafe { ffi::zth_sem_getvalue(self.raw_ptr(), &mut value) },
            value,
        )
    }

    fn acquire_ffi(&self) -> Result<(), Error> {
        Self::as_result(unsafe { ffi::zth_sem_wait(self.raw_ptr()) }, ())
    }

    fn try_acquire_ffi(&self) -> Result<bool, Error> {
        match unsafe { ffi::zth_sem_trywait(self.raw_ptr()) } {
            0 => Ok(true),
            rc if rc == Self::EAGAIN => Ok(false),
            rc => Err(Error::from_raw_os_error(rc)),
        }
    }

    fn release_ffi(&self) -> Result<(), Error> {
        Self::as_result(unsafe { ffi::zth_sem_post(self.raw_ptr()) }, ())
    }

    pub fn acquire(&self) -> Result<SemaphorePermit<'_>, Error> {
        self.acquire_ffi()?;
        Ok(SemaphorePermit { sem: self })
    }

    pub fn try_acquire(&self) -> Result<Option<SemaphorePermit<'_>>, Error> {
        if self.try_acquire_ffi()? {
            Ok(Some(SemaphorePermit { sem: self }))
        } else {
            Ok(None)
        }
    }

    pub fn release(&self) -> Result<(), Error> {
        self.release_ffi()
    }
}

pub struct SemaphorePermit<'a> {
    sem: &'a Semaphore,
}

impl Drop for SemaphorePermit<'_> {
    fn drop(&mut self) {
        let rc = self.sem.release_ffi();
        debug_assert!(rc.is_ok());
    }
}

define_synchronizer_type!(Condition, CondRaw, ffi::zth_cond_destroy);
define_synchronizer_type!(Gate, GateRaw, ffi::zth_gate_destroy);

/// Fiber-aware condition variable.
///
/// This mirrors the C API and provides wait/notify operations:
/// - [`wait`](Self::wait) blocks the current fiber until signaled.
/// - [`signal`](Self::signal) wakes one waiter.
/// - [`broadcast`](Self::broadcast) wakes all waiters.
impl Condition {
    pub fn new() -> Result<Self, Error> {
        let h = Self::null();
        Self::as_result(unsafe { ffi::zth_cond_init(h.raw_ptr()) }, h)
    }

    pub fn signal(&self) -> Result<(), Error> {
        Self::as_result(unsafe { ffi::zth_cond_signal(self.raw_ptr()) }, ())
    }

    pub fn notify_one(&self) -> Result<(), Error> {
        self.signal()
    }

    pub fn broadcast(&self) -> Result<(), Error> {
        Self::as_result(unsafe { ffi::zth_cond_broadcast(self.raw_ptr()) }, ())
    }

    pub fn notify_all(&self) -> Result<(), Error> {
        self.broadcast()
    }

    pub fn wait(&self) -> Result<(), Error> {
        Self::as_result(unsafe { ffi::zth_cond_wait(self.raw_ptr()) }, ())
    }
}

/// Fiber-aware gate/barrier-style synchronizer.
///
/// This mirrors the C gate API:
/// - [`pass`](Self::pass) attempts to pass without blocking.
/// - [`wait`](Self::wait) blocks until the gate opens.
impl Gate {
    const EBUSY: c_int = 16;

    pub fn new(count: usize) -> Result<Self, Error> {
        let h = Self::null();
        Self::as_result(unsafe { ffi::zth_gate_init(h.raw_ptr(), count) }, h)
    }

    pub fn pass(&self) -> Result<bool, Error> {
        match unsafe { ffi::zth_gate_pass(self.raw_ptr()) } {
            0 => Ok(true),
            rc if rc == Self::EBUSY => Ok(false),
            rc => Err(Error::from_raw_os_error(rc)),
        }
    }

    pub fn wait(&self) -> Result<(), Error> {
        Self::as_result(unsafe { ffi::zth_gate_wait(self.raw_ptr()) }, ())
    }
}
