// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

use std::ffi::c_void;
use std::fmt;

mod ffi {
    use super::Fiber;

    extern "C" {
        pub fn zth_current_fiber() -> Fiber;
    }
}

#[derive(Debug, PartialEq, Eq, Hash, Clone, Copy)]
#[repr(C)]
pub struct Fiber {
    p: *const c_void,
}

/// A Fiber handle.
///
/// The fiber lives in C++ space, this is only a wrapper for the handle.
impl Fiber {
    /// Returns the fiber handle.
    pub fn addr(&self) -> *const c_void {
        self.p
    }

    /// Converts this object to be passed to `zth_fiber_create()`.
    pub(crate) fn to_ptr(&mut self) -> *mut Self {
        self
    }

    /// Returns an invalid Fiber object.
    pub fn null() -> Self {
        Self {
            p: std::ptr::null(),
        }
    }

    /// Get the currently running Fiber.
    ///
    /// Returns [`Fiber::null()`] when there is no fiber currently running.
    pub fn current() -> Self {
        unsafe { ffi::zth_current_fiber() }
    }

    pub fn is_valid(&self) -> bool {
        !self.p.is_null()
    }
}

impl Default for Fiber {
    fn default() -> Self {
        Self::null()
    }
}

impl std::fmt::Display for Fiber {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{:x}", self.addr().addr())
    }
}
