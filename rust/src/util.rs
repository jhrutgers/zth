// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

use std::ffi::{c_char, c_int, c_void, CStr, CString};
use std::fmt::Arguments;

mod ffi {
    use std::ffi::{c_char, c_int, c_void};

    extern "C" {
        pub fn zth_banner() -> *const c_char;
        pub fn zth_logv(fmt: *const c_char, ap: *mut c_void);
        pub fn zth_log_colorv(color: c_int, fmt: *const c_char, ap: *mut c_void);
        pub fn zth_err(e: c_int) -> *mut c_char;
    }
}

extern "C" {
    fn vasprintf(strp: *mut *mut c_char, fmt: *const c_char, ap: *mut c_void) -> c_int;
    fn free(ptr: *mut c_void);
}

/// Logging backend symbol consumed by Zth.
///
/// This function overrides Zth's weak `zth_logv` symbol and routes formatted
/// output through Rust's standard output buffering.
///
/// # Safety
///
/// - `fmt` must point to a valid NUL-terminated C format string.
/// - `arg` must be a valid `va_list` matching `fmt` for the active C ABI.
/// - Both pointers must remain valid for the duration of the call.
#[no_mangle]
pub unsafe extern "C" fn zth_logv(fmt: *const c_char, arg: *mut c_void) {
    if fmt.is_null() {
        return;
    }

    if arg.is_null() {
        let line = CStr::from_ptr(fmt).to_string_lossy();
        print!("{}", line);
        return;
    }

    let mut rendered: *mut c_char = std::ptr::null_mut();
    let rc = vasprintf(&mut rendered, fmt, arg);
    if rc >= 0 && !rendered.is_null() {
        let line = CStr::from_ptr(rendered).to_string_lossy();
        print!("{}", line);
        free(rendered.cast::<c_void>());
        return;
    }

    let line = CStr::from_ptr(fmt).to_string_lossy();
    print!("{}", line);
}

/// Returns a banner line with version and configuration information.
pub fn banner() -> String {
    unsafe {
        let ptr = ffi::zth_banner();
        assert!(!ptr.is_null());

        CStr::from_ptr(ptr).to_string_lossy().into_owned()
    }
}

/// Logs a pre-formatted message through Zth's logging backend.
///
/// Use this function with [`format_args!`] or call [`log!`] for ergonomic
/// `format!`-style invocation.
pub fn log(args: Arguments<'_>) {
    let rendered = std::fmt::format(args);
    let rendered_c = CString::new(rendered).unwrap_or_else(|_| {
        CString::new("invalid Rust log message: contains interior NUL")
            .expect("static fallback log message must not contain NUL")
    });

    unsafe {
        // Pass a fully rendered message; zth_logv() handles null va_list by
        // printing fmt directly.
        ffi::zth_logv(rendered_c.as_ptr(), std::ptr::null_mut());
    }
}

/// Logs through Zth with `format!`-style syntax.
#[macro_export]
macro_rules! log {
    ($($arg:tt)*) => {
        $crate::log(format_args!($($arg)*))
    };
}

/// Logs a pre-formatted message through Zth's logging backend with a color.
///
/// Use this function with [`format_args!`] or call [`log!`] for ergonomic
/// `format!`-style invocation.
pub fn log_color(color: u8, args: Arguments<'_>) {
    let rendered = std::fmt::format(args);
    let rendered_c = CString::new(rendered).unwrap_or_else(|_| {
        CString::new("invalid Rust log message: contains interior NUL")
            .expect("static fallback log message must not contain NUL")
    });

    unsafe {
        // Pass a fully rendered message; zth_logv() handles null va_list by
        // printing fmt directly.
        ffi::zth_log_colorv(color.into(), rendered_c.as_ptr(), std::ptr::null_mut());
    }
}

/// Logs through Zth with `format!`-style syntax.
#[macro_export]
macro_rules! log_color {
    ($color:expr, $($arg:tt)*) => {
        $crate::log_color($color, format_args!($($arg)*))
    };
}

/// Redirect Zth termination to Rust panic!.
#[no_mangle]
pub unsafe extern "C" fn zth_terminate() {
    panic!("Zth terminated");
}

/// Convert an Zth error code into a string.
pub fn err(e: i32) -> String {
    unsafe {
        let p = ffi::zth_err(e.into());
        if p.is_null() {
            return format!("{}", e);
        }

        let s = CStr::from_ptr(p).to_string_lossy().into_owned();
        free(p.cast());

        s
    }
}
