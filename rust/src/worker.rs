// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

mod ffi {
    extern "C" {
        pub fn zth_yield();
        pub fn zth_outOfWork();
    }
}

/// Allow to yield execution from the current fiber to the scheduler.
///
/// When yielded too quickly, the fiber might just continue immediately.  It is safe to call this
/// function often. When you cannot continue, and a yield is required, use [`outOfWork`].
///
/// This is a direct wrapper around the C API `zth_yield()`.
pub fn may_yield() {
    unsafe {
        ffi::zth_yield();
    }
}

/// Yields execution from the current fiber to the scheduler.
///
/// This is a direct wrapper around the C API `zth_outOfWork()`.
pub fn out_of_work() {
    unsafe {
        ffi::zth_outOfWork();
    }
}
