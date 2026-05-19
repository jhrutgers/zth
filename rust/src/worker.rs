// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

mod ffi {
    use core::ffi::c_long;

    extern "C" {
        pub fn zth_yield();
        pub fn zth_outOfWork();
        pub fn zth_mnap(sleepFor_ms: c_long);
        pub fn zth_unap(sleepFor_us: c_long);
    }
}

use core::ffi::c_long;
use core::time::Duration;

/// Allow to yield execution from the current fiber to the scheduler.
///
/// When yielded too quickly, the fiber might just continue immediately.  It is safe to call this
/// function often. When you cannot continue, and a yield is required, use [`out_of_work`].
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

/// Sleep the current fiber for at least the requested duration.
///
/// This function uses microsecond precision and chunks long sleeps to fit C `long`
/// across platforms.
pub fn sleep(duration: Duration) {
    let mut sleep_for_us = duration.as_micros();
    if sleep_for_us == 0 {
        return;
    }

    let max_chunk = c_long::MAX as u128;
    while sleep_for_us > 0 {
        let chunk = core::cmp::min(sleep_for_us, max_chunk);
        unsafe {
            ffi::zth_unap(chunk as c_long);
        }
        sleep_for_us -= chunk;
    }
}

/// Sleep the current fiber for the given number of milliseconds.
pub fn sleep_ms(ms: u32) {
    let max_chunk = c_long::MAX as u32;
    let mut remaining = ms;

    while remaining > 0 {
        let chunk = core::cmp::min(remaining, max_chunk);
        unsafe {
            ffi::zth_mnap(chunk as c_long);
        }
        remaining -= chunk;
    }
}

/// Sleep the current fiber for the given number of microseconds.
pub fn sleep_us(us: u64) {
    sleep(Duration::from_micros(us));
}

/// Fixed-delay periodic helper.
///
/// Each call to [`tick`](Self::tick) sleeps one full interval from the moment it is
/// called. This is simple and works in `no_std`, but it does not compensate for work
/// time drift.
#[derive(Clone, Debug)]
pub struct Period {
    interval: Duration,
    next_deadline: Option<Duration>,
}

impl Period {
    /// Create a phase-locked periodic helper with a fixed interval.
    pub const fn new(interval: Duration) -> Self {
        Self {
            interval,
            next_deadline: None,
        }
    }

    /// Returns the configured interval.
    pub const fn interval(&self) -> Duration {
        self.interval
    }

    /// Sleep until the next phase-locked tick.
    ///
    /// This keeps cadence aligned to the initial phase and compensates for variable
    /// work time between calls. If monotonic time is unavailable, it falls back to
    /// fixed-delay behavior.
    pub fn tick(&mut self) {
        if self.interval.is_zero() {
            may_yield();
            return;
        }

        let now = crate::time::now();

        let deadline = match self.next_deadline {
            Some(t) => t,
            None => now.checked_add(self.interval).unwrap_or(Duration::MAX),
        };

        if deadline > now {
            sleep(deadline - now);
        }

        let after = crate::time::now();
        let mut next = deadline.checked_add(self.interval).unwrap_or(Duration::MAX);
        while next <= after {
            let Some(candidate) = next.checked_add(self.interval) else {
                break;
            };
            next = candidate;
        }

        self.next_deadline = Some(next);
    }
}
