// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

use std::ffi::c_void;
use std::rc::Rc;

use crate::r#async::{fiber_start_trampoline, FiberStart};
use crate::Error;
use crate::Fiber;
use crate::FiberEntry;
use crate::Future;

mod ffi {
    use std::ffi::{c_int, c_void};

    extern "C" {
        pub fn zth_preinit();
        pub fn zth_run(f: extern "C" fn(*mut c_void), arg: *mut c_void) -> c_int;
        pub fn zth_postdeinit() -> c_int;
    }
}

pub fn run<F, Args>(entry: F, args: Args) -> Result<F::Output, Error>
where
    F: FiberEntry<Args> + 'static,
    Args: 'static,
{
    assert!(!Fiber::current().is_valid());

    unsafe { ffi::zth_preinit() }

    let f = Rc::new(Future::<F::Output>::new()?);

    let start = Box::new(FiberStart {
        entry,
        args,
        future: Some(f.clone()),
    });
    let start_ptr = Box::into_raw(start);

    let rc = unsafe {
        ffi::zth_run(
            fiber_start_trampoline::<F, Args>,
            start_ptr.cast::<c_void>(),
        )
    };

    unsafe { ffi::zth_postdeinit() };

    if rc == 0 {
        Ok(f.get()?)
    } else {
        unsafe {
            drop(Box::from_raw(start_ptr));
        }
        Err(Error(rc))
    }
}
