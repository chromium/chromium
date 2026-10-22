//! 64-bit specific Apple (ios/darwin) definitions

use crate::prelude::*;

s2! {
    #[exhaustive] // unlikely to change
    pub struct timeval32 {
        pub tv_sec: i32,
        pub tv_usec: i32,
    }

    #[exhaustive] // FIXME(exhaustive): review for 1.0
    pub struct if_data {
        pub ifi_type: c_uchar,
        pub ifi_typelen: c_uchar,
        pub ifi_physical: c_uchar,
        pub ifi_addrlen: c_uchar,
        pub ifi_hdrlen: c_uchar,
        pub ifi_recvquota: c_uchar,
        pub ifi_xmitquota: c_uchar,
        pub ifi_unused1: c_uchar,
        pub ifi_mtu: u32,
        pub ifi_metric: u32,
        pub ifi_baudrate: u32,
        pub ifi_ipackets: u32,
        pub ifi_ierrors: u32,
        pub ifi_opackets: u32,
        pub ifi_oerrors: u32,
        pub ifi_collisions: u32,
        pub ifi_ibytes: u32,
        pub ifi_obytes: u32,
        pub ifi_imcasts: u32,
        pub ifi_omcasts: u32,
        pub ifi_iqdrops: u32,
        pub ifi_noproto: u32,
        pub ifi_recvtiming: u32,
        pub ifi_xmittiming: u32,
        pub ifi_lastchange: timeval32,
        pub ifi_unused2: u32,
        pub ifi_hwassist: u32,
        pub ifi_reserved1: u32,
        pub ifi_reserved2: u32,
    }

    #[exhaustive] // FIXME(exhaustive): review for 1.0
    #[cfg(target_os = "macos")]
    pub struct bpf_hdr {
        pub bh_tstamp: crate::timeval32,
        pub bh_caplen: u32,
        pub bh_datalen: u32,
        pub bh_hdrlen: c_ushort,
    }
}

cfg_if! {
    if #[cfg(target_os = "macos")] {
        pub const BIOCSETF: c_ulong = 0x80104267;
        pub const BIOCSRTIMEOUT: c_ulong = 0x8010426d;
        pub const BIOCGRTIMEOUT: c_ulong = 0x4010426e;
        pub const BIOCSETFNR: c_ulong = 0x8010427e;
    }
}

extern "C" {
    // `exchangedata` is prohibited on tvOS and watchOS.
    #[cfg(not(any(target_os = "tvos", target_os = "watchos")))]
    pub fn exchangedata(path1: *const c_char, path2: *const c_char, options: c_uint) -> c_int;
}

cfg_if! {
    if #[cfg(target_arch = "x86_64")] {
        mod x86_64;
        pub use self::x86_64::*;
    } else if #[cfg(target_arch = "aarch64")] {
        mod aarch64;
        pub use self::aarch64::*;
    } else {
        // Unknown target_arch
    }
}
