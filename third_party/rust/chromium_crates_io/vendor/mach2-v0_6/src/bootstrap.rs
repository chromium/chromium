//! This module corresponds to `bootstrap.h`

use crate::boolean::boolean_t;
use crate::kern_return::kern_return_t;
use crate::port::mach_port_t;
use core::ffi::{c_char, c_int, c_uint};

pub const BOOTSTRAP_MAX_NAME_LEN: c_uint = 128;
pub const BOOTSTRAP_MAX_CMD_LEN: c_uint = 512;

pub const BOOTSTRAP_MAX_LOOKUP_COUNT: c_uint = 20;

pub const BOOTSTRAP_SUCCESS: c_uint = 0;
pub const BOOTSTRAP_NOT_PRIVILEGED: c_uint = 1100;
pub const BOOTSTRAP_NAME_IN_USE: c_uint = 1101;
pub const BOOTSTRAP_UNKNOWN_SERVICE: c_uint = 1102;
pub const BOOTSTRAP_SERVICE_ACTIVE: c_uint = 1103;
pub const BOOTSTRAP_BAD_COUNT: c_uint = 1104;
pub const BOOTSTRAP_NO_MEMORY: c_uint = 1105;
pub const BOOTSTRAP_NO_CHILDREN: c_uint = 1106;

pub const BOOTSTRAP_STATUS_INACTIVE: c_uint = 0;
pub const BOOTSTRAP_STATUS_ACTIVE: c_uint = 1;
pub const BOOTSTRAP_STATUS_ON_DEMAND: c_uint = 2;

pub type name_t = [c_char; 128];
pub type cmd_t = [c_char; 512];
pub type name_array_t = *mut name_t;
pub type bootstrap_status_t = c_int;
pub type bootstrap_status_array_t = *mut bootstrap_status_t;
pub type bootstrap_property_t = c_uint;
pub type bootstrap_property_array_t = *mut bootstrap_property_t;
pub type bool_array_t = *mut boolean_t;

unsafe extern "C" {
    pub static bootstrap_port: mach_port_t;
    pub fn bootstrap_create_server(
        bp: mach_port_t,
        server_cmd: *mut c_char,
        server_uid: c_uint,
        on_demand: boolean_t,
        server_port: *mut mach_port_t,
    ) -> kern_return_t;
    pub fn bootstrap_subset(
        bp: mach_port_t,
        requestor_port: mach_port_t,
        subset_port: *mut mach_port_t,
    ) -> kern_return_t;
    pub fn bootstrap_unprivileged(bp: mach_port_t, unpriv_port: *mut mach_port_t) -> kern_return_t;
    pub fn bootstrap_parent(bp: mach_port_t, parent_port: *mut mach_port_t) -> kern_return_t;
    pub fn bootstrap_register(
        bp: mach_port_t,
        service_name: *mut c_char,
        sp: mach_port_t,
    ) -> kern_return_t;
    pub fn bootstrap_create_service(
        bp: mach_port_t,
        service_name: *mut c_char,
        sp: *mut mach_port_t,
    ) -> kern_return_t;
    pub fn bootstrap_check_in(
        bp: mach_port_t,
        service_name: *const c_char,
        sp: *mut mach_port_t,
    ) -> kern_return_t;
    pub fn bootstrap_look_up(
        bp: mach_port_t,
        service_name: *const c_char,
        sp: *mut mach_port_t,
    ) -> kern_return_t;
    pub fn bootstrap_status(
        bp: mach_port_t,
        service_name: *mut c_char,
        service_active: *mut bootstrap_status_t,
    ) -> kern_return_t;
    pub fn bootstrap_strerror(r: kern_return_t) -> *const c_char;
}
