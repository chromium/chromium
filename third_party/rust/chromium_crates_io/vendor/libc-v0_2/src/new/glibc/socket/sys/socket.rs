//! Header: `socket/sys/socket.h`

pub use crate::new::glibc::bits::socket::*;
use crate::prelude::*;

c_enum! {
    #[repr(c_int)]
    enum #anon {
        pub SHUT_RD = 0,
        pub SHUT_WR,
        pub SHUT_RDWR,
    }
}

s! {
    pub struct mmsghdr {
        pub msg_hdr: crate::msghdr,
        pub msg_len: c_uint,
    }
}
