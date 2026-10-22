//! Header: `include/sys/socket.h`

use crate::prelude::*;

cfg_if! {
    if #[cfg(target_arch = "mips")] {
        // Header: `libc/sysdeps/linux/mips/bits/socket_type.h`
        //
        // Inlined for simplicity.
        // Actually called __socket_type but that causes test issues
        c_enum! {
            #[repr(c_int)]
            enum #anon {
                pub SOCK_DGRAM = 1,
                pub SOCK_STREAM = 2,
                pub SOCK_RAW = 3,
                pub SOCK_RDM = 4,
                pub SOCK_SEQPACKET = 5,
                pub SOCK_DCCP = 6,
                #[deprecated(since = "0.2.70", note = "AF_PACKET must be used instead")]
                pub SOCK_PACKET = 10,
                pub SOCK_CLOEXEC = 0o2000000,
                pub SOCK_NONBLOCK = 0o0000200,
            }
        }
    } else {
        // Header: `libc/sysdeps/linux/common/bits/socket_type.h`
        //
        // Inlined for simplicity.
        // Actually called __socket_type but that causes test issues
        c_enum! {
            #[repr(c_int)]
            enum #anon {
                pub SOCK_STREAM = 1,
                pub SOCK_DGRAM = 2,
                pub SOCK_RAW = 3,
                pub SOCK_RDM = 4,
                pub SOCK_SEQPACKET = 5,
                pub SOCK_DCCP = 6,
                #[deprecated(since = "0.2.70", note = "AF_PACKET must be used instead")]
                pub SOCK_PACKET = 10,
                pub SOCK_CLOEXEC = 0o2000000,
                pub SOCK_NONBLOCK = 0o0004000,
            }
        }
    }
}

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
