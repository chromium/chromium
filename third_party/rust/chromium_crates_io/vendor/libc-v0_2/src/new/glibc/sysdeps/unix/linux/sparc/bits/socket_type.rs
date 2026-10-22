//! Header: `sysdeps/unix/sysv/linux/mips/bits/socket_type.h`

use crate::prelude::*;

c_enum! {
    // Actually called __socket_type but that causes test issues
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
        pub SOCK_CLOEXEC = 0x400000,
        pub SOCK_NONBLOCK = 0x004000,
    }
}
