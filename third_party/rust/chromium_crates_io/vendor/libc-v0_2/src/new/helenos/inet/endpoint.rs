//! HelenOS internet endpoint types
//!
//! * Header file: <https://github.com/HelenOS/helenos/tree/master/uspace/lib/inet/include/inet/endpoint.h>

pub use crate::inet::addr::*;
pub use crate::loc::*;

s_no_extra_traits! {
    pub struct inet_ep2_t {
        pub local_link: service_id_t,
        pub local: inet_ep_t,
        pub remote: inet_ep_t,
    }

    pub struct inet_ep_t {
        pub addr: inet_addr_t,
        pub port: u16,
    }
}
extern "C" {
    pub fn inet_ep_init(ep: *mut inet_ep_t);
    pub fn inet_ep2_init(epp: *mut inet_ep2_t);
}
