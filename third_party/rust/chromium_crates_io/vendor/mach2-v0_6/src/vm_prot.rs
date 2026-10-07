//! This module corresponds to `mach/vm_prot.h`.
use core::ffi::c_int;

pub type vm_prot_t = c_int;

pub const VM_PROT_NONE: vm_prot_t = 0;
pub const VM_PROT_READ: vm_prot_t = 1;
pub const VM_PROT_WRITE: vm_prot_t = 1 << 1;
pub const VM_PROT_EXECUTE: vm_prot_t = 1 << 2;
pub const VM_PROT_NO_CHANGE_LEGACY: vm_prot_t = 0x08;
pub const VM_PROT_NO_CHANGE: vm_prot_t = 0x0100_0000;
pub const VM_PROT_COPY: vm_prot_t = 0x10;
pub const VM_PROT_WANTS_COPY: vm_prot_t = VM_PROT_COPY;
pub const VM_PROT_DEFAULT: vm_prot_t = VM_PROT_READ | VM_PROT_WRITE;
pub const VM_PROT_ALL: vm_prot_t = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
pub const VM_PROT_IS_MASK: vm_prot_t = 0x40;
pub const VM_PROT_STRIP_READ: vm_prot_t = 0x80;
pub const VM_PROT_EXECUTE_ONLY: vm_prot_t = VM_PROT_EXECUTE | VM_PROT_STRIP_READ;
pub const VM_PROT_TPRO: vm_prot_t = 0x200;
#[cfg(target_arch = "x86_64")]
pub const VM_PROT_UEXEC: vm_prot_t = 0x8;
#[cfg(target_arch = "x86_64")]
pub const VM_PROT_ALLEXEC: vm_prot_t = VM_PROT_EXECUTE | VM_PROT_UEXEC;
#[cfg(not(target_arch = "x86_64"))]
pub const VM_PROT_ALLEXEC: vm_prot_t = VM_PROT_EXECUTE;
