//! This module corresponds to `mach/mig.h`.

use crate::kern_return::kern_return_t;
use crate::message::{
    mach_msg_header_t, mach_msg_id_t, mach_msg_size_t, mach_msg_type_descriptor_t,
};
use crate::port::mach_port_t;
use crate::vm_types::{vm_address_t, vm_size_t};
use core::ffi::{c_char, c_int, c_uint};
use core::ptr;

pub type mig_stub_routine_t =
    Option<unsafe extern "C" fn(*mut mach_msg_header_t, *mut mach_msg_header_t)>;
pub type mig_routine_t = mig_stub_routine_t;
pub type mig_server_routine_t =
    Option<unsafe extern "C" fn(*mut mach_msg_header_t) -> mig_routine_t>;
pub type mig_impl_routine_t = Option<unsafe extern "C" fn() -> kern_return_t>;

pub type routine_arg_descriptor_t = *mut mach_msg_type_descriptor_t;
pub type mig_routine_arg_descriptor_t = *mut mach_msg_type_descriptor_t;

pub const MIG_ROUTINE_ARG_DESCRIPTOR_NULL: mig_routine_arg_descriptor_t = ptr::null_mut();

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct routine_descriptor {
    pub impl_routine: mig_impl_routine_t,
    pub stub_routine: mig_stub_routine_t,
    pub argc: c_uint,
    pub descr_count: c_uint,
    pub arg_descr: routine_arg_descriptor_t,
    pub max_reply_msg: c_uint,
}

pub type routine_descriptor_t = *mut routine_descriptor;
pub type mig_routine_descriptor = routine_descriptor;
pub type mig_routine_descriptor_t = *mut mig_routine_descriptor;

pub const MIG_ROUTINE_DESCRIPTOR_NULL: mig_routine_descriptor_t = ptr::null_mut();

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct mig_subsystem {
    pub server: mig_server_routine_t,
    pub start: mach_msg_id_t,
    pub end: mach_msg_id_t,
    pub maxsize: mach_msg_size_t,
    pub reserved: vm_address_t,
    pub routine: [mig_routine_descriptor; 1],
}

pub type mig_subsystem_t = *mut mig_subsystem;

pub const MIG_SUBSYSTEM_NULL: mig_subsystem_t = ptr::null_mut();

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct mig_symtab {
    pub ms_routine_name: *mut c_char,
    pub ms_routine_number: c_int,
    pub ms_routine: Option<unsafe extern "C" fn()>,
}

pub type mig_symtab_t = mig_symtab;

unsafe extern "C" {
    pub fn mig_get_reply_port() -> mach_port_t;
    pub fn mig_dealloc_reply_port(reply_port: mach_port_t);
    pub fn mig_put_reply_port(reply_port: mach_port_t);
    pub fn mig_strncpy(dest: *mut c_char, src: *const c_char, len: c_int) -> c_int;
    pub fn mig_strncpy_zerofill(dest: *mut c_char, src: *const c_char, len: c_int) -> c_int;
    pub fn mig_allocate(address: *mut vm_address_t, size: vm_size_t);
    pub fn mig_deallocate(address: vm_address_t, size: vm_size_t);
}
