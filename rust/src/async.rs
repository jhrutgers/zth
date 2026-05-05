// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

use std::ffi::{c_void, CStr, CString};

use crate::fiber::Fiber;
use crate::Error;

mod ffi {
    use crate::fiber::Fiber;
    use std::ffi::{c_char, c_int, c_void};

    extern "C" {
        pub fn zth_fiber_create(
            h: *mut Fiber,
            f: extern "C" fn(*mut c_void),
            arg: *mut c_void,
            stack: usize,
            name: *const c_char,
        ) -> c_int;
    }
}

/// Trait implemented by callable fiber entry points.
///
/// This trait is implemented for `FnOnce` callables that take either no
/// arguments or tuples up to arity 8.
pub trait FiberEntry<Args> {
    /// Invokes the entry point with the provided argument tuple.
    fn call(self, args: Args);
}

impl<F> FiberEntry<()> for F
where
    F: FnOnce(),
{
    fn call(self, (): ()) {
        self();
    }
}

macro_rules! impl_fiber_entry_tuple {
    ($($name:ident),+ $(,)?) => {
        impl<F, $($name),+> FiberEntry<($($name,)+)> for F
        where
            F: FnOnce($($name),+),
        {
            #[allow(non_snake_case)]
            fn call(self, args: ($($name,)+)) {
                let ($($name,)+) = args;
                self($($name),+);
            }
        }
    };
}

impl_fiber_entry_tuple!(A0);
impl_fiber_entry_tuple!(A0, A1);
impl_fiber_entry_tuple!(A0, A1, A2);
impl_fiber_entry_tuple!(A0, A1, A2, A3);
impl_fiber_entry_tuple!(A0, A1, A2, A3, A4);
impl_fiber_entry_tuple!(A0, A1, A2, A3, A4, A5);
impl_fiber_entry_tuple!(A0, A1, A2, A3, A4, A5, A6);
impl_fiber_entry_tuple!(A0, A1, A2, A3, A4, A5, A6, A7);

pub(crate) struct FiberStart<F, Args> {
    pub entry: F,
    pub args: Args,
}

/// Optional configuration for spawning a fiber.
#[derive(Debug, Clone, Default)]
pub struct FiberOptions {
    /// Requested fiber stack size in bytes. `0` uses the Zth default.
    pub stack: usize,
    /// Optional C-compatible fiber name for tracing/debug output.
    pub name: Option<CString>,
}

impl FiberOptions {
    /// Sets the requested stack size in bytes.
    pub fn stack(mut self, stack: usize) -> Self {
        self.stack = stack;
        self
    }

    /// Sets a fiber name used by Zth logging and diagnostics.
    ///
    /// Panics when the provided name contains an interior NUL byte.
    pub fn name(mut self, name: impl AsRef<str>) -> Self {
        self.name =
            Some(CString::new(name.as_ref()).expect("fiber name must not contain NUL bytes"));
        self
    }

    fn name_cstr(&self) -> Option<&CStr> {
        self.name.as_deref()
    }
}

pub(crate) extern "C" fn fiber_start_trampoline<F, Args>(arg: *mut c_void)
where
    F: FiberEntry<Args>,
{
    let start = unsafe { Box::from_raw(arg.cast::<FiberStart<F, Args>>()) };
    start.entry.call(start.args);
}

fn fiber_impl<F, Args>(
    entry: F,
    args: Args,
    stack: usize,
    name: Option<&CStr>,
) -> Result<Fiber, Error>
where
    F: FiberEntry<Args> + 'static,
    Args: 'static,
{
    let start = Box::new(FiberStart { entry, args });
    let start_ptr = Box::into_raw(start);
    let mut h = Fiber::null();

    let rc = unsafe {
        ffi::zth_fiber_create(
            h.to_ptr(),
            fiber_start_trampoline::<F, Args>,
            start_ptr.cast::<c_void>(),
            stack,
            name.map_or(std::ptr::null(), CStr::as_ptr),
        )
    };

    if rc == 0 {
        Ok(h)
    } else {
        unsafe {
            drop(Box::from_raw(start_ptr));
        }
        Err(Error(rc))
    }
}

/// Spawns a new fiber using default options.
///
/// Equivalent to calling [`fiber_with`] with `FiberOptions::default()`.
pub fn fiber<F, Args>(entry: F, args: Args) -> Result<Fiber, Error>
where
    F: FiberEntry<Args> + 'static,
    Args: 'static,
{
    fiber_impl(entry, args, 0, None)
}

/// Spawns a new fiber with explicit options.
///
/// `entry` and `args` are moved into heap storage and executed by Zth when
/// the scheduled fiber starts.
pub fn fiber_with<F, Args>(entry: F, args: Args, options: FiberOptions) -> Result<Fiber, Error>
where
    F: FiberEntry<Args> + 'static,
    Args: 'static,
{
    fiber_impl(entry, args, options.stack, options.name_cstr())
}
