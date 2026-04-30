// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

use std::ffi::{c_void, CStr, CString};

use crate::Error;

mod ffi {
    use std::ffi::{c_char, c_int, c_void};

    extern "C" {
        pub fn zth_fiber_create(
            f: extern "C" fn(*mut c_void),
            arg: *mut c_void,
            stack: usize,
            name: *const c_char,
        ) -> c_int;
    }
}

pub trait FiberEntry<Args> {
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

struct FiberStart<F, Args> {
    entry: F,
    args: Args,
}

#[derive(Debug, Clone, Default)]
pub struct FiberOptions {
    pub stack: usize,
    pub name: Option<CString>,
}

impl FiberOptions {
    pub fn stack(mut self, stack: usize) -> Self {
        self.stack = stack;
        self
    }

    pub fn name(mut self, name: impl AsRef<str>) -> Self {
        self.name =
            Some(CString::new(name.as_ref()).expect("fiber name must not contain NUL bytes"));
        self
    }

    fn name_cstr(&self) -> Option<&CStr> {
        self.name.as_deref()
    }
}

extern "C" fn fiber_start_trampoline<F, Args>(arg: *mut c_void)
where
    F: FiberEntry<Args>,
{
    let start = unsafe { Box::from_raw(arg.cast::<FiberStart<F, Args>>()) };
    start.entry.call(start.args);
}

fn fiber_impl<F, Args>(entry: F, args: Args, stack: usize, name: Option<&CStr>) -> Result<(), Error>
where
    F: FiberEntry<Args> + 'static,
    Args: 'static,
{
    let start = Box::new(FiberStart { entry, args });
    let start_ptr = Box::into_raw(start);

    let rc = unsafe {
        ffi::zth_fiber_create(
            fiber_start_trampoline::<F, Args>,
            start_ptr.cast::<c_void>(),
            stack,
            name.map_or(std::ptr::null(), CStr::as_ptr),
        )
    };

    if rc == 0 {
        Ok(())
    } else {
        unsafe {
            drop(Box::from_raw(start_ptr));
        }
        Err(Error(rc))
    }
}

pub fn fiber<F, Args>(entry: F, args: Args) -> Result<(), Error>
where
    F: FiberEntry<Args> + 'static,
    Args: 'static,
{
    fiber_impl(entry, args, 0, None)
}

pub fn fiber_with<F, Args>(entry: F, args: Args, options: FiberOptions) -> Result<(), Error>
where
    F: FiberEntry<Args> + 'static,
    Args: 'static,
{
    fiber_impl(entry, args, options.stack, options.name_cstr())
}
