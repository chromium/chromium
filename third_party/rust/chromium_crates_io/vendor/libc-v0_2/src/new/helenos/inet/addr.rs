//! HelenOS internet address types
//!
//! * Header file: <https://github.com/HelenOS/helenos/tree/master/uspace/lib/inet/include/inet/addr.h>

pub type addr32_t = u32;
pub type addr128_t = [u8; 16];

c_enum! {
    #[repr(u32)]
    pub enum ip_ver_t {
        pub ip_any,
        pub ip_v4,
        pub ip_v6,
    }
}
s_no_extra_traits! {
    pub union __inet_addr_t_addr_union {
        pub addr: addr32_t,
        pub addr6: addr128_t,
    }

    pub struct inet_addr_t {
        pub version: ip_ver_t,
        pub addr: __inet_addr_t_addr_union,
    }
}
