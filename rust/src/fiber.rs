// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

use std::ffi::c_void;
use std::fmt;

mod ffi {
    use std::ffi::c_void;

    extern "C" {
        pub fn zth_current_fiber() -> *const c_void;
    }
}

#[derive(Debug, PartialEq, Eq, Hash, Clone, Copy)]
pub struct Fiber {
    h: *const c_void,
}

/// A Fiber handle.
///
/// The fiber lives in C++ space, this is only a wrapper for the handle.
impl Fiber {
    /// Returns the fiber handle.
    pub fn handle(&self) -> *const c_void {
        self.h
    }

    /// Converts this object to be passed to `zth_fiber_create()`.
    pub(crate) fn to_handle_ptr(&mut self) -> *mut *const c_void {
        &mut self.h
    }

    /// Returns an invalid Fiber object.
    pub fn null() -> Self {
        Self {
            h: std::ptr::null(),
        }
    }

    /// Get the currently running Fiber.
    ///
    /// Returns [`Fiber::null()`] when there is no fiber currently running.
    pub fn current() -> Self {
        Self {
            h: unsafe { ffi::zth_current_fiber() },
        }
    }
}

impl Default for Fiber {
    fn default() -> Self {
        Self::null()
    }
}

impl std::fmt::Display for Fiber {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{:x}", self.handle().addr())
    }
}
