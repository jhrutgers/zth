// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

use std::ffi::c_void;

mod ffi {
    use super::Future;
    use std::ffi::c_int;

    extern "C" {
        pub fn zth_future_init(future: *mut Future) -> c_int;
        pub fn zth_future_destroy(future: *mut Future) -> c_int;
        pub fn zth_future_valid(future: *mut Future) -> c_int;
        pub fn zth_future_set(future: *mut Future, value: usize) -> c_int;
        pub fn zth_future_get(future: *mut Future, value: *mut usize) -> c_int;
        pub fn zth_future_wait(future: *mut Future) -> c_int;
    }
}

#[derive(Debug, PartialEq, Eq, Hash)]
#[repr(C)]
pub struct Future {
    p: *const c_void,
}

use crate::Error;

impl Future {
    pub fn new() -> Result<Self, Error> {
        let mut h = Self::null();
        let rc = unsafe { ffi::zth_future_init(h.to_ptr()) };

        if rc == 0 {
            Ok(h)
        } else {
            Err(Error(rc))
        }
    }

    pub fn to_ptr(&mut self) -> *mut Future {
        self
    }

    pub fn null() -> Self {
        Self {
            p: std::ptr::null(),
        }
    }

    pub fn is_null(&self) -> bool {
        self.p.is_null()
    }

    pub fn valid(&mut self) -> bool {
        unsafe { ffi::zth_future_valid(self.to_ptr()) == 0 }
    }

    pub fn set(&mut self, value: usize) -> Result<(), Error> {
        let rc = unsafe { ffi::zth_future_set(self.to_ptr(), value) };
        if rc == 0 {
            Ok(())
        } else {
            Err(Error(rc))
        }
    }

    pub fn get(&mut self) -> Result<usize, Error> {
        let mut value: usize = 0;
        let rc = unsafe { ffi::zth_future_get(self.to_ptr(), &mut value) };
        if rc == 0 {
            Ok(value)
        } else {
            Err(Error(rc))
        }
    }

    pub fn wait(&mut self) -> Result<(), Error> {
        let rc = unsafe { ffi::zth_future_wait(self.to_ptr()) };
        if rc == 0 {
            Ok(())
        } else {
            Err(Error(rc))
        }
    }
}

impl Drop for Future {
    fn drop(&mut self) {
        if self.is_null() {
            return;
        }

        unsafe {
            let _ = ffi::zth_future_destroy(self.to_ptr());
        }

        self.p = std::ptr::null();
    }
}
