// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

use std::ffi::{c_char, c_int, c_void, CStr};

extern "C" {
    fn vasprintf(strp: *mut *mut c_char, fmt: *const c_char, ap: *mut c_void) -> c_int;
    fn free(ptr: *mut c_void);
}

#[no_mangle]
pub unsafe extern "C" fn zth_logv(fmt: *const c_char, arg: *mut c_void) {
    if fmt.is_null() {
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
