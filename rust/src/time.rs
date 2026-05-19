// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

use core::time::Duration;

mod ffi {
    extern "C" {
        pub fn zth_now2(s: *mut u64, ns: *mut u32);
    }
}

/// Monotonic timestamp used by Zth (`zth::now`).
///
/// This wraps the C API `zth_now2()` and converts the result to Rust
/// `core::time::Duration` relative to an unspecified monotonic epoch.
pub fn now() -> Duration {
    let mut s: u64 = 0;
    let mut ns: u32 = 0;
    unsafe {
        ffi::zth_now2(&mut s, &mut ns);
    }

    // Be defensive against any invalid ns values from foreign code.
    Duration::new(s, core::cmp::min(ns, 999_999_999))
}
