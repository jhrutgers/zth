// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#![doc = include_str!("../README.md")]
#![cfg_attr(not(zth_hosted_std), no_std)]

extern crate alloc;

mod allocator;
mod r#async;
mod fiber;
mod init;
mod sync;
mod time;
mod util;
mod worker;

use core::clone::Clone;
use core::cmp::Eq;
use core::cmp::PartialEq;
use core::ffi::c_int;
use core::fmt;
use core::fmt::Debug;
use core::marker::Copy;
use core::prelude::rust_2021::derive;

pub use fiber::Fiber;
pub use init::run;
pub use r#async::fiber;
pub use r#async::fiber_with;
pub use r#async::FiberEntry;
pub use r#async::FiberOptions;
pub use sync::Future;
pub use time::now;
pub use util::banner;
pub use util::err;
pub use util::log;
pub use util::log_color;
pub use worker::may_yield;
pub use worker::out_of_work;
pub use worker::sleep;
pub use worker::sleep_ms;
pub use worker::sleep_us;
pub use worker::Period;
pub use zth_macros::main;
pub use zth_macros::main_fiber;

/// Error returned by Zth C API wrappers.
///
/// The inner value is a platform errno-style error code.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Error(c_int);

impl Error {
    /// Returns the raw errno value reported by the underlying C API.
    pub fn raw_os_error(self) -> c_int {
        self.0
    }

    pub fn from_raw_os_error(e: c_int) -> Self {
        Error(e)
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&err(self.0))
    }
}

#[cfg(zth_hosted_std)]
impl std::error::Error for Error {}

#[doc(hidden)]
pub mod __private {
    use super::Error;
    use core::ffi::c_int;

    pub trait MainReturn {
        fn into_exit_code(self) -> c_int;
    }

    impl MainReturn for () {
        fn into_exit_code(self) -> c_int {
            0
        }
    }

    impl MainReturn for c_int {
        fn into_exit_code(self) -> c_int {
            self
        }
    }

    impl MainReturn for Result<(), Error> {
        fn into_exit_code(self) -> c_int {
            match self {
                Ok(()) => 0,
                Err(_) => 1,
            }
        }
    }

    impl MainReturn for Result<c_int, Error> {
        fn into_exit_code(self) -> c_int {
            match self {
                Ok(code) => code,
                Err(_) => 1,
            }
        }
    }

    pub fn to_exit_code<T>(value: T) -> c_int
    where
        T: MainReturn,
    {
        value.into_exit_code()
    }
}
