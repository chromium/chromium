//! This module roughly corresponds to `mach/clock.h`.
use crate::clock_types::{alarm_type_t, clock_attr_t, clock_flavor_t, mach_timespec_t};
use crate::kern_return::kern_return_t;
use crate::mach_types::{clock_reply_t, clock_serv_t};
use crate::message::mach_msg_type_number_t;
use core::ffi::c_uint;

pub const clock_MSG_COUNT: c_uint = 3;

unsafe extern "C" {
    pub fn clock_get_time(
        clock_serv: clock_serv_t,
        cur_time: *mut mach_timespec_t,
    ) -> kern_return_t;
    pub fn clock_get_attributes(
        clock_serv: clock_serv_t,
        flavor: clock_flavor_t,
        clock_attr: clock_attr_t,
        clock_attrCnt: *mut mach_msg_type_number_t,
    ) -> kern_return_t;
    pub fn clock_alarm(
        clock_serv: clock_serv_t,
        alarm_type: alarm_type_t,
        alarm_time: mach_timespec_t,
        alarm_port: clock_reply_t,
    ) -> kern_return_t;
}
