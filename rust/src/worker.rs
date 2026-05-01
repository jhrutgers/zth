// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

mod ffi {
    extern "C" {
        pub fn zth_yield();
    }
}

/// Yields execution from the current fiber to the scheduler.
///
/// This is a direct wrapper around the C API `zth_yield()`.
pub fn yield_now() {
    unsafe {
        ffi::zth_yield();
    }
}
