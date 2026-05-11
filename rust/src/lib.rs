// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

//! Rust bindings for Zth.
//!
//! This crate exposes the core Zth primitives for running fibers.
//!
//! # Quick Start
//!
//! ```no_run
//! fn fiber2() {
//!     // Do work.
//!     zth::may_yield();
//!     // Do more work.
//! }
//!
//! fn fiber() {
//!     zth::fiber(fiber2, ());
//! }
//!
//! fn main() -> Result<(), zth::Error> {
//!     // Start a worker and execute fiber().
//!     zth::run(fiber, ())
//! }
//! ```
//!
//! # Native Library Discovery
//!
//! The crate links against native `libzth` and discovers it through
//! `build.rs`.
//!
//! Common setup:
//! - Set `ZTH_INSTALL` to an installed prefix containing Zth headers/libs.
//! - Set `ZTH_REPO` to a Zth repository root (uses `dist/<target>/build`), if not detected automatically.
//! - Optionally set `ZTH_DIST` to override target dir selection.

mod r#async;
mod fiber;
mod init;
mod sync;
mod util;
mod worker;

use std::ffi::{c_char, c_int};
use std::fmt;

pub use fiber::Fiber;
pub use init::run;
pub use r#async::fiber;
pub use r#async::fiber_with;
pub use r#async::FiberEntry;
pub use r#async::FiberOptions;
pub use sync::Future;
pub use util::banner;
pub use util::err;
pub use util::log;
pub use util::log_color;
pub use worker::may_yield;
pub use worker::out_of_work;

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

impl std::error::Error for Error {}

/// Signature of the `%main_fiber` entry point used by Zth's default `main()`.
///
/// Applications that rely on Zth-provided process startup export a function
/// with this signature (usually as `#[no_mangle] pub extern "C" fn main_fiber(...)`).
pub type MainFiber = extern "C" fn(c_int, *mut *mut c_char) -> c_int;
