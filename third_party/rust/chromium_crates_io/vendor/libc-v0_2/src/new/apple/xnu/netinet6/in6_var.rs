//! Header: `netinet6/in6_var.h`
//!
//! <https://github.com/apple-oss-distributions/xnu/blob/main/bsd/netinet6/in6_var.h>

use crate::prelude::*;
use crate::sys::ioccom::*;

s2! {
    pub struct in6_aliasreq {
        pub ifra_name: [c_char; crate::IFNAMSIZ],
        pub ifra_addr: crate::sockaddr_in6,
        pub ifra_dstaddr: crate::sockaddr_in6,
        pub ifra_prefixmask: crate::sockaddr_in6,
        pub ifra_flags: c_int,
        pub ifra_lifetime: crate::in6_addrlifetime,
    }
}

pub const IN6_IFF_ANYCAST: c_int = 0x0001;
pub const IN6_IFF_TENTATIVE: c_int = 0x0002;
pub const IN6_IFF_DUPLICATED: c_int = 0x0004;
pub const IN6_IFF_DETACHED: c_int = 0x0008;
pub const IN6_IFF_DEPRECATED: c_int = 0x0010;
pub const IN6_IFF_NODAD: c_int = 0x0020;
pub const IN6_IFF_AUTOCONF: c_int = 0x0040;
pub const IN6_IFF_TEMPORARY: c_int = 0x0080;
pub const IN6_IFF_DYNAMIC: c_int = 0x0100;
pub const IN6_IFF_OPTIMISTIC: c_int = 0x0200;
pub const IN6_IFF_SECURED: c_int = 0x0400;
pub const IN6_IFF_CLAT46: c_int = 0x1000;
pub const IN6_IFF_NOPFX: c_int = 0x8000;

pub const SIOCDIFADDR_IN6: c_ulong = _IOW::<crate::in6_ifreq>('i' as c_ulong, 25);
pub const SIOCAIFADDR_IN6: c_ulong = _IOW::<in6_aliasreq>('i' as c_ulong, 26);
pub const SIOCSIFPHYADDR_IN6: c_ulong = _IOW::<in6_aliasreq>('i' as c_ulong, 62);
pub const SIOCGIFAFLAG_IN6: c_ulong = _IOWR::<crate::in6_ifreq>('i' as c_ulong, 73);
