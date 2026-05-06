// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

use std::cell::{Cell, UnsafeCell};
use std::ffi::{c_int, c_void};
use std::fmt;
use std::hash::{Hash, Hasher};
use std::marker::PhantomData;
use std::mem;
use std::mem::{ManuallyDrop, MaybeUninit};
use std::rc::Rc;

mod ffi {
    use super::FutureRaw;
    use std::ffi::c_int;

    extern "C" {
        pub fn zth_future_init(future: *mut FutureRaw) -> c_int;
        pub fn zth_future_destroy(future: *mut FutureRaw) -> c_int;
        pub fn zth_future_valid(future: *mut FutureRaw) -> c_int;
        pub fn zth_future_set(future: *mut FutureRaw, value: usize) -> c_int;
        pub fn zth_future_get(future: *mut FutureRaw, value: *mut usize) -> c_int;
        pub fn zth_future_wait(future: *mut FutureRaw) -> c_int;
    }
}

use crate::Error;

pub(crate) trait Synchronizer: Sized {
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
                    p: std::ptr::null_mut(),
                }
            }

            fn raw_is_null(raw: *const Self::Raw) -> bool {
                unsafe { (*raw).p.is_null() }
            }

            unsafe fn raw_clear(raw: *mut Self::Raw) {
                (*raw).p = std::ptr::null_mut();
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
                std::ptr::copy_nonoverlapping(
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
                std::ptr::copy_nonoverlapping(
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
                if !Self::is_usize_mode() {
                    let ptr = encoded as *mut T;
                    unsafe { drop(Box::from_raw(ptr)) };
                } else if mem::needs_drop::<T>() {
                    drop(Self::decode_value(encoded));
                }
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

impl<T> Drop for Future<T>
where
    T: 'static,
{
    fn drop(&mut self) {
        if Self::requires_take_guard() && self.raw.valid() && !self.taken.get() {
            if let Ok(raw) = self.raw.get() {
                drop(Self::decode_value(raw));
            }
        }
    }
}
