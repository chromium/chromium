//! This module roughly corresponds to `mach/clock_types.h`.
use core::ffi::{c_int, c_uint, c_ulonglong};

pub type alarm_type_t = c_int;
pub type sleep_type_t = c_int;
pub type clock_id_t = c_int;
pub type clock_flavor_t = c_int;
pub type clock_attr_t = *mut c_int;
pub type clock_res_t = c_int;

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct mach_timespec {
    pub tv_sec: c_uint,
    pub tv_nsec: clock_res_t,
}
pub type mach_timespec_t = mach_timespec;

pub const SYSTEM_CLOCK: c_uint = 0;
pub const CALENDAR_CLOCK: c_uint = 1;
pub const REALTIME_CLOCK: c_uint = 0;

pub const CLOCK_GET_TIME_RES: c_uint = 1;
pub const CLOCK_ALARM_CURRES: c_uint = 3;
pub const CLOCK_ALARM_MINRES: c_uint = 4;
pub const CLOCK_ALARM_MAXRES: c_uint = 5;

pub const NSEC_PER_USEC: c_ulonglong = 1000;
pub const USEC_PER_SEC: c_ulonglong = 1_000_000;
pub const NSEC_PER_SEC: c_ulonglong = 1_000_000_000;
pub const NSEC_PER_MSEC: c_ulonglong = 1_000_000;

#[allow(non_snake_case)]
pub fn BAD_MACH_TIMESPEC(t: mach_timespec) -> bool {
    t.tv_nsec < 0 || (t.tv_nsec as c_ulonglong) >= NSEC_PER_SEC
}

#[allow(non_snake_case)]
pub fn CMP_MACH_TIMESPEC(t1: &mach_timespec, t2: &mach_timespec) -> c_ulonglong {
    if t1.tv_sec > t2.tv_sec {
        return NSEC_PER_SEC;
    }
    if t1.tv_sec < t2.tv_sec {
        return !NSEC_PER_SEC;
    }
    (t1.tv_nsec as c_ulonglong) - (t2.tv_nsec as c_ulonglong)
}

#[allow(non_snake_case)]
pub fn ADD_MACH_TIMESPEC(t1: &mut mach_timespec, t2: &mach_timespec) {
    t1.tv_nsec += t2.tv_nsec;
    if (t1.tv_nsec as c_ulonglong) >= NSEC_PER_SEC {
        t1.tv_nsec = (t1.tv_nsec as c_ulonglong - NSEC_PER_SEC) as clock_res_t;
        t1.tv_sec += 1;
    }
    t1.tv_sec += t2.tv_sec;
}

#[allow(non_snake_case)]
pub fn SUB_MACH_TIMESPEC(t1: &mut mach_timespec, t2: &mach_timespec) {
    t1.tv_nsec -= t2.tv_nsec;
    if t1.tv_nsec < 0 {
        t1.tv_nsec = (t1.tv_nsec as c_ulonglong + NSEC_PER_SEC) as clock_res_t;
        t1.tv_sec -= 1;
    }
    t1.tv_sec -= t2.tv_sec;
}

pub const ALRMTYPE: c_uint = 0xff;
pub const TIME_ABSOLUTE: c_uint = 0x00;
pub const TIME_RELATIVE: c_uint = 0x01;

#[allow(non_snake_case)]
pub fn BAD_ALRMTYPE(t: c_uint) -> bool {
    t & (!TIME_RELATIVE) != 0
}
