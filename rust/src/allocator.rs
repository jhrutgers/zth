// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#[cfg(feature = "bare-metal")]
mod baremetal {
    use core::alloc::{GlobalAlloc, Layout};
    use core::ffi::c_void;

    struct Allocator;

    unsafe impl GlobalAlloc for Allocator {
        unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
            malloc(layout.size().max(1)).cast()
        }

        unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
            if !ptr.is_null() {
                free(ptr.cast());
            }
        }
    }

    #[global_allocator]
    static ALLOCATOR: Allocator = Allocator;

    unsafe extern "C" {
        fn malloc(size: usize) -> *mut c_void;
        fn free(ptr: *mut c_void);
    }
}
