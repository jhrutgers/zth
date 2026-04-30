// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

mod r#async;
mod util;
mod worker;

pub use r#async::{fiber, fiber_with, FiberEntry, FiberOptions};
use std::ffi::{c_char, c_int};
use std::fmt;
pub use util::zth_logv;
pub use worker::yield_now;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Error(c_int);

impl Error {
    pub fn raw_os_error(self) -> c_int {
        self.0
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Zth returned errno {}", self.0)
    }
}

impl std::error::Error for Error {}

pub type MainFiber = extern "C" fn(c_int, *mut *mut c_char) -> c_int;
