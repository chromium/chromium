//! This module roughly corresponds to `mach/vm_page_size.h`
use crate::vm_types::{mach_vm_offset_t, mach_vm_size_t, vm_size_t};
use core::ffi::c_int;

unsafe extern "C" {
    pub static vm_page_size: vm_size_t;
    pub static vm_page_mask: vm_size_t;
    pub static vm_page_shift: c_int;
    pub static vm_kernel_page_size: vm_size_t;
    pub static vm_kernel_page_mask: vm_size_t;
    pub static vm_kernel_page_shift: c_int;
}

#[allow(clippy::missing_safety_doc)] // FIXME
pub unsafe fn vm_trunc_page(x: vm_size_t) -> vm_size_t {
    unsafe { x & !(vm_page_size - 1) }
}

#[allow(clippy::missing_safety_doc)] // FIXME
pub unsafe fn vm_round_page(x: vm_size_t) -> vm_size_t {
    unsafe { vm_trunc_page(x + (vm_page_size - 1)) }
}

#[allow(clippy::missing_safety_doc)] // FIXME
pub unsafe fn mach_vm_trunc_page(x: mach_vm_offset_t) -> mach_vm_offset_t {
    unsafe { x & !(vm_page_mask as mach_vm_size_t) }
}

#[allow(clippy::missing_safety_doc)] // FIXME
pub unsafe fn mach_vm_round_page(x: mach_vm_offset_t) -> mach_vm_offset_t {
    unsafe { (x + vm_page_mask as mach_vm_size_t) & !(vm_page_mask as mach_vm_size_t) }
}

#[cfg(test)]
mod tests {
    use crate::vm_page_size::*;
    use crate::vm_types::mach_vm_size_t;

    #[test]
    fn page_size() {
        unsafe {
            assert!(vm_page_size > 0);
            assert!(vm_page_size % 2 == 0);
            assert_eq!(mach_vm_round_page(1), vm_page_size as mach_vm_size_t);

            #[cfg(target_arch = "aarch64")]
            assert_eq!(vm_page_size, 16384);

            #[cfg(not(target_arch = "aarch64"))]
            assert_eq!(vm_page_size, 4096);
        }
    }
}

#[allow(clippy::missing_safety_doc)] // FIXME
pub unsafe fn trunc_page_kernel(x: vm_size_t) -> vm_size_t {
    unsafe { x & !vm_kernel_page_mask }
}

#[allow(clippy::missing_safety_doc)] // FIXME
pub unsafe fn round_page_kernel(x: vm_size_t) -> vm_size_t {
    unsafe { trunc_page_kernel(x + vm_kernel_page_mask) }
}
