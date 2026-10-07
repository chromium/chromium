//! This module roughly corresponds to `mach-o/dyld.h`.

use crate::loader::mach_header;
use core::ffi::c_char;

unsafe extern "C" {
    pub fn _dyld_image_count() -> u32;
    pub fn _dyld_get_image_header(image_index: u32) -> *const mach_header;
    pub fn _dyld_get_image_vmaddr_slide(image_index: u32) -> isize;
    pub fn _dyld_get_image_name(image_index: u32) -> *const c_char;
}
