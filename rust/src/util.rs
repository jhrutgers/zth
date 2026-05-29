// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

use alloc::ffi::CString;
use alloc::format;
use alloc::string::String;
use core::assert;
use core::convert::Into;
#[cfg(zth_hosted_std)]
use core::ffi::{c_char, c_int};
use core::ffi::{c_void, CStr};
use core::fmt::Arguments;
use core::ptr;
#[cfg(zth_hosted_std)]
use log as log_crate;

mod ffi {
    use core::ffi::{c_char, c_int, c_void};

    extern "C" {
        pub fn zth_banner() -> *const c_char;
        pub fn zth_logv(fmt: *const c_char, ap: *mut c_void);
        pub fn zth_log_colorv(color: c_int, fmt: *const c_char, ap: *mut c_void);
        pub fn zth_err(e: c_int) -> *mut c_char;
        #[cfg(all(not(zth_hosted_std), feature = "panic-handler"))]
        pub fn zth_terminate();
    }
}

extern "C" {
    #[cfg(zth_hosted_std)]
    fn vasprintf(strp: *mut *mut c_char, fmt: *const c_char, ap: *mut c_void) -> c_int;
    fn free(ptr: *mut c_void);
}

/// Logging backend symbol consumed by Zth.
///
/// This function overrides Zth's weak `zth_logv` symbol and routes formatted output through Rust's
/// logging infrastructure at debug level.
///
/// # Safety
///
/// - `fmt` must point to a valid NUL-terminated C format string.
/// - `arg` must be a valid `va_list` matching `fmt` for the active C ABI.
/// - Both pointers must remain valid for the duration of the call.
#[cfg(zth_hosted_std)]
#[no_mangle]
pub unsafe extern "C" fn zth_logv(fmt: *const c_char, arg: *mut c_void) {
    use alloc::string::String;
    use alloc::string::ToString;
    use core::cell::RefCell;

    thread_local! {
        static LOGV_BUF: RefCell<String> = RefCell::new(String::new());
    }

    if fmt.is_null() {
        return;
    }

    let mut rendered: *mut c_char = ptr::null_mut();
    let rc = vasprintf(&mut rendered, fmt, arg);
    let msg = if rc >= 0 && !rendered.is_null() {
        let line = CStr::from_ptr(rendered).to_string_lossy().to_string();
        free(rendered.cast::<c_void>());
        line
    } else {
        CStr::from_ptr(fmt).to_string_lossy().to_string()
    };

    LOGV_BUF.with(|buf| {
        let mut buf = buf.borrow_mut();
        buf.push_str(&msg);
        while let Some(pos) = buf.find('\n') {
            let mut line = buf[..pos].to_string();
            if line.starts_with('\x1b') {
                line.push_str("\x1b[0m");
            }
            log_crate::debug!(target: "zth", "{}", line);
            buf.drain(..=pos); // Remove up to and including the newline
        }
    });
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
/// Use this function with [`format_args!`] or call the `log!` macro for ergonomic
/// `format!`-style invocation.
pub fn log(args: Arguments<'_>) {
    let rendered = alloc::fmt::format(args);
    let rendered_c = CString::new(rendered).unwrap_or_else(|_| {
        CString::new("invalid Rust log message: contains interior NUL")
            .expect("static fallback log message must not contain NUL")
    });

    unsafe {
        ffi::zth_logv(rendered_c.as_ptr(), ptr::null_mut());
    }
}

/// Logs through Zth with `format!`-style syntax.
#[macro_export]
macro_rules! log {
    ($($arg:tt)*) => {
        $crate::log(::core::format_args!($($arg)*))
    };
}

/// Logs a pre-formatted message through Zth's logging backend with a color.
///
/// Use this function with [`format_args!`] or call the `log_color!` macro for ergonomic
/// `format!`-style invocation.
pub fn log_color(color: u8, args: Arguments<'_>) {
    let rendered = alloc::fmt::format(args);
    let rendered_c = CString::new(rendered).unwrap_or_else(|_| {
        CString::new("invalid Rust log message: contains interior NUL")
            .expect("static fallback log message must not contain NUL")
    });

    unsafe {
        ffi::zth_log_colorv(color.into(), rendered_c.as_ptr(), ptr::null_mut());
    }
}

/// Prints a pre-formatted message.
///
/// Use this function with [`format_args!`] or call the `print!` macro for ergonomic
/// `format!`-style invocation.
#[cfg(zth_hosted_std)]
pub fn print(args: Arguments<'_>) {
    use std::io::Write;
    let mut stdout = std::io::stdout().lock();
    let _ = stdout.write_fmt(args);
}

#[cfg(not(zth_hosted_std))]
pub fn print(args: Arguments<'_>) {
    log(args);
}

/// Prints through Zth with `format!`-style syntax.
#[macro_export]
macro_rules! print {
    ($($arg:tt)*) => {
        $crate::print(::core::format_args!($($arg)*))
    };
}

/// Logs through Zth with `format!`-style syntax.
#[macro_export]
macro_rules! log_color {
    ($color:expr, $($arg:tt)*) => {
        $crate::log_color($color, format_args!($($arg)*))
    };
}

/// Redirect Zth termination to Rust panic!.
#[cfg(zth_hosted_std)]
#[no_mangle]
pub unsafe extern "C" fn zth_terminate() {
    core::panic!("Zth terminated");
}

#[cfg(all(not(zth_hosted_std), feature = "panic-handler"))]
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo<'_>) -> ! {
    unsafe {
        ffi::zth_terminate();
    }
    loop {}
}

/// Convert an Zth error code into a string.
pub fn err(e: i32) -> String {
    unsafe {
        let p = ffi::zth_err(e.into());
        if p.is_null() {
            return format!("{}", e);
        }

        let s = CStr::from_ptr(p).to_string_lossy().into_owned();
        free(p.cast::<c_void>());

        s
    }
}
