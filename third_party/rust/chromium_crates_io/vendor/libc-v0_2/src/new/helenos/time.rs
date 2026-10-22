//! HelenOS time handling
//!
//! * Header file: <https://github.com/HelenOS/helenos/tree/master/uspace/lib/c/include/time.h>

use crate::prelude::*;

pub type time_t = c_longlong;
pub type usec_t = c_longlong;

s2! {
    #[exhaustive]
    pub struct timespec {
        pub tv_sec: time_t,
        pub tv_nsec: c_long,
    }
}

extern "C" {
    pub fn getuptime(tp: *mut timespec);
}
