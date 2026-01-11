//! Header: `time.h`

use super::*;
use crate::prelude::*;

// Clock types
pub const CLOCK_REALTIME: clockid_t = 0;
pub const CLOCK_MONOTONIC: clockid_t = 1;
pub const CLOCK_PROCESS_CPUTIME_ID: clockid_t = 2;
pub const CLOCK_THREAD_CPUTIME_ID: clockid_t = 3;

// Timer flags
pub const TIMER_ABSTIME: c_int = 1;

extern "C" {
    // Time functions
    pub fn time(tloc: *mut time_t) -> time_t;
    pub fn difftime(time1: time_t, time0: time_t) -> c_double;
    pub fn mktime(tm: *mut tm) -> time_t;
    pub fn gmtime(timep: *const time_t) -> *mut tm;
    pub fn gmtime_r(timep: *const time_t, result: *mut tm) -> *mut tm;
    pub fn localtime(timep: *const time_t) -> *mut tm;
    pub fn localtime_r(timep: *const time_t, result: *mut tm) -> *mut tm;
    pub fn asctime(tm: *const tm) -> *mut c_char;
    pub fn asctime_r(tm: *const tm, buf: *mut c_char) -> *mut c_char;
    pub fn ctime(timep: *const time_t) -> *mut c_char;
    pub fn ctime_r(timep: *const time_t, buf: *mut c_char) -> *mut c_char;
    pub fn strftime(
        s: *mut c_char,
        maxsize: size_t,
        format: *const c_char,
        timeptr: *const tm,
    ) -> size_t;

    // High-resolution time functions
    pub fn clock_gettime(clk_id: clockid_t, tp: *mut timespec) -> c_int;
    pub fn clock_settime(clk_id: clockid_t, tp: *const timespec) -> c_int;
    pub fn clock_getres(clk_id: clockid_t, res: *mut timespec) -> c_int;
    pub fn nanosleep(req: *const timespec, rem: *mut timespec) -> c_int;

    // Timer functions
    pub fn timer_create(clockid: clockid_t, sevp: *mut sigevent, timerid: *mut timer_t) -> c_int;
    pub fn timer_delete(timerid: timer_t) -> c_int;
    pub fn timer_settime(
        timerid: timer_t,
        flags: c_int,
        new_value: *const itimerspec,
        old_value: *mut itimerspec,
    ) -> c_int;
    pub fn timer_gettime(timerid: timer_t, curr_value: *mut itimerspec) -> c_int;
    pub fn timer_getoverrun(timerid: timer_t) -> c_int;
}
