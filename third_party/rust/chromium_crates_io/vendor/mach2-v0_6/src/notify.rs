//! This module corresponds to `mach/notify.h`.

use crate::message::{
    mach_msg_body_t, mach_msg_header_t, mach_msg_port_descriptor_t, mach_msg_security_trailer_t,
    mach_msg_type_number_t,
};
use crate::ndr::NDR_record_t;
use crate::port::mach_port_name_t;
use crate::port::mach_port_t;
use core::ffi::c_int;

pub const MACH_NOTIFY_FIRST: c_int = 0o100;
pub const MACH_NOTIFY_PORT_DELETED: c_int = MACH_NOTIFY_FIRST + 0o01;
pub const MACH_NOTIFY_SEND_POSSIBLE: c_int = MACH_NOTIFY_FIRST + 0o02;
pub const MACH_NOTIFY_PORT_DESTROYED: c_int = MACH_NOTIFY_FIRST + 0o05;
pub const MACH_NOTIFY_NO_SENDERS: c_int = MACH_NOTIFY_FIRST + 0o06;
pub const MACH_NOTIFY_SEND_ONCE: c_int = MACH_NOTIFY_FIRST + 0o07;
pub const MACH_NOTIFY_DEAD_NAME: c_int = MACH_NOTIFY_FIRST + 0o10;
pub const MACH_NOTIFY_LAST: c_int = MACH_NOTIFY_FIRST + 0o15;

pub type notify_port_t = mach_port_t;

#[repr(C)]
#[derive(Copy, Clone, Debug)]
#[allow(non_snake_case)]
pub struct mach_port_deleted_notification_t {
    pub not_header: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub not_port: mach_port_name_t,
    pub trailer: mach_msg_security_trailer_t,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
#[allow(non_snake_case)]
pub struct mach_send_possible_notification_t {
    pub not_header: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub not_port: mach_port_name_t,
    pub trailer: mach_msg_security_trailer_t,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct mach_port_destroyed_notification_t {
    pub not_header: mach_msg_header_t,
    pub not_body: mach_msg_body_t,
    pub not_port: mach_msg_port_descriptor_t,
    pub trailer: mach_msg_security_trailer_t,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
#[allow(non_snake_case)]
pub struct mach_no_senders_notification_t {
    pub not_header: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub not_count: mach_msg_type_number_t,
    pub trailer: mach_msg_security_trailer_t,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct mach_send_once_notification_t {
    pub not_header: mach_msg_header_t,
    pub trailer: mach_msg_security_trailer_t,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
#[allow(non_snake_case)]
pub struct mach_dead_name_notification_t {
    pub not_header: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub not_port: mach_port_name_t,
    pub trailer: mach_msg_security_trailer_t,
}
