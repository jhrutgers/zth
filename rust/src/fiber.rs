// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

use alloc::rc::Rc;
use core::clone::Clone;
use core::cmp::Eq;
use core::cmp::PartialEq;
use core::default::Default;
use core::ffi::c_void;
use core::fmt;
use core::fmt::Debug;
use core::hash::{Hash, Hasher};
use core::marker::Copy;
use core::option::Option;
use core::option::Option::None;
use core::prelude::rust_2021::derive;
use core::ptr;
use core::result::Result;

use crate::sync::Future;

mod ffi {
    use super::FiberRaw;

    extern "C" {
        pub fn zth_current_fiber() -> FiberRaw;
    }
}

#[derive(Debug, PartialEq, Eq, Hash, Clone, Copy)]
#[repr(C)]
pub struct FiberRaw {
    p: *const c_void,
}

#[derive(Clone)]
pub struct Fiber<T: 'static = ()> {
    raw: FiberRaw,
    future: Option<Rc<Future<T>>>,
}

/// A Fiber handle.
///
/// The fiber lives in C++ space, this is only a wrapper for the handle.
impl<T> Fiber<T> {
    /// Returns the fiber handle.
    pub fn handle(&self) -> *const c_void {
        self.raw.p
    }

    /// Converts this object to be passed to `zth_fiber_create()`.
    pub(crate) fn raw_ptr(&mut self) -> *mut FiberRaw {
        &mut self.raw
    }

    /// Returns an invalid Fiber object.
    pub fn null() -> Self {
        Self {
            raw: FiberRaw { p: ptr::null() },
            future: None,
        }
    }

    pub fn is_valid(&self) -> bool {
        !self.handle().is_null()
    }

    pub(crate) fn with_future(&mut self, future: Rc<Future<T>>) {
        self.future.replace(future);
    }

    pub fn future(&self) -> Option<Rc<Future<T>>> {
        self.future.clone()
    }
}

impl Fiber<()> {
    /// Get the currently running Fiber.
    ///
    /// Returns [`Fiber::null()`] when there is no fiber currently running.
    pub fn current() -> Self {
        Self {
            raw: unsafe { ffi::zth_current_fiber() },
            future: None,
        }
    }
}

impl<T> fmt::Debug for Fiber<T> {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> Result<(), fmt::Error> {
        self.raw.fmt(formatter)
    }
}

impl<T> Default for Fiber<T> {
    fn default() -> Self {
        Self::null()
    }
}

impl<T> PartialEq for Fiber<T> {
    fn eq(&self, other: &Self) -> bool {
        self.handle() == other.handle()
    }
}

impl<T> Eq for Fiber<T> {}

impl<T> Hash for Fiber<T> {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.handle().hash(state);
    }
}

impl<T> fmt::Display for Fiber<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        core::write!(f, "{:x}", self.handle().addr())
    }
}
