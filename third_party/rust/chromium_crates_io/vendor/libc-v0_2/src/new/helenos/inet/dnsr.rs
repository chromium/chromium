//! HelenOS DNS resolver API
//!
//! * Header file: <https://github.com/HelenOS/helenos/tree/master/uspace/lib/inet/include/inet/dnsr.h>

use crate::errno_t;
pub use crate::inet::addr::*;
use crate::prelude::*;

s_no_extra_traits! {
    pub struct dnsr_hostinfo_t {
        pub cname: *mut c_char,
        pub addr: inet_addr_t,
    }
}

extern "C" {
    pub fn dnsr_name2host(
        name: *const c_char,
        info: *mut *mut dnsr_hostinfo_t,
        ipver: ip_ver_t,
    ) -> errno_t;
    pub fn dnsr_hostinfo_destroy(info: *mut dnsr_hostinfo_t);
}
