//! Header: `arch/sparc/include/uapi/asm/socket.h`

use crate::prelude::*;

pub const SOL_SOCKET: c_int = 0xffff;

pub const SO_DEBUG: c_int = 0x0001;
pub const SO_PASSCRED: c_int = 0x0002;
pub const SO_REUSEADDR: c_int = 0x0004;
pub const SO_KEEPALIVE: c_int = 0x0008;
pub const SO_DONTROUTE: c_int = 0x0010;
pub const SO_BROADCAST: c_int = 0x0020;
pub const SO_PEERCRED: c_int = 0x0040;
pub const SO_LINGER: c_int = 0x0080;
pub const SO_OOBINLINE: c_int = 0x0100;
pub const SO_REUSEPORT: c_int = 0x0200;
pub const SO_BSDCOMPAT: c_int = 0x0400;
pub const SO_RCVLOWAT: c_int = 0x0800;
pub const SO_SNDLOWAT: c_int = 0x1000;
pub const SO_RCVTIMEO_OLD: c_int = 0x2000;
pub const SO_SNDTIMEO_OLD: c_int = 0x4000;
pub const SO_ACCEPTCONN: c_int = 0x8000;

pub const SO_SNDBUF: c_int = 0x1001;
pub const SO_RCVBUF: c_int = 0x1002;
pub const SO_SNDBUFFORCE: c_int = 0x100a;
pub const SO_RCVBUFFORCE: c_int = 0x100b;
pub const SO_ERROR: c_int = 0x1007;
pub const SO_TYPE: c_int = 0x1008;
pub const SO_PROTOCOL: c_int = 0x1028;
pub const SO_DOMAIN: c_int = 0x1029;

/* Linux specific, keep the same. */
pub const SO_NO_CHECK: c_int = 0x000b;
pub const SO_PRIORITY: c_int = 0x000c;

pub const SO_BINDTODEVICE: c_int = 0x000d;

pub const SO_ATTACH_FILTER: c_int = 0x001a;
pub const SO_DETACH_FILTER: c_int = 0x001b;
pub const SO_GET_FILTER: c_int = SO_ATTACH_FILTER;

pub const SO_PEERNAME: c_int = 0x001c;

pub const SO_PEERSEC: c_int = 0x001e;
pub const SO_PASSSEC: c_int = 0x001f;

pub const SO_MARK: c_int = 0x0022;

pub const SO_RXQ_OVFL: c_int = 0x0024;

pub const SO_WIFI_STATUS: c_int = 0x0025;
pub const SCM_WIFI_STATUS: c_int = SO_WIFI_STATUS;
pub const SO_PEEK_OFF: c_int = 0x0026;

pub const SO_NOFCS: c_int = 0x0027;

pub const SO_LOCK_FILTER: c_int = 0x0028;

pub const SO_SELECT_ERR_QUEUE: c_int = 0x0029;

pub const SO_BUSY_POLL: c_int = 0x0030;

pub const SO_MAX_PACING_RATE: c_int = 0x0031;

pub const SO_BPF_EXTENSIONS: c_int = 0x0032;

pub const SO_INCOMING_CPU: c_int = 0x0033;

pub const SO_ATTACH_BPF: c_int = 0x0034;
pub const SO_DETACH_BPF: c_int = SO_DETACH_FILTER;

pub const SO_ATTACH_REUSEPORT_CBPF: c_int = 0x0035;
pub const SO_ATTACH_REUSEPORT_EBPF: c_int = 0x0036;

pub const SO_CNX_ADVICE: c_int = 0x0037;

pub const SCM_TIMESTAMPING_OPT_STATS: c_int = 0x0038;

pub const SO_MEMINFO: c_int = 0x0039;

pub const SO_INCOMING_NAPI_ID: c_int = 0x003a;

pub const SO_COOKIE: c_int = 0x003b;

pub const SCM_TIMESTAMPING_PKTINFO: c_int = 0x003c;

pub const SO_PEERGROUPS: c_int = 0x003d;

pub const SO_ZEROCOPY: c_int = 0x003e;

pub const SO_TXTIME: c_int = 0x003f;
pub const SCM_TXTIME: c_int = SO_TXTIME;

pub const SO_BINDTOIFINDEX: c_int = 0x0041;

pub const SO_SECURITY_AUTHENTICATION: c_int = 0x5001;
pub const SO_SECURITY_ENCRYPTION_TRANSPORT: c_int = 0x5002;
pub const SO_SECURITY_ENCRYPTION_NETWORK: c_int = 0x5004;

const SO_TIMESTAMP_OLD: c_int = 0x001d;
const SO_TIMESTAMPNS_OLD: c_int = 0x0021;
const SO_TIMESTAMPING_OLD: c_int = 0x0023;

// `SO_*_NEW` constants defined in `cfg` block

pub const SO_DETACH_REUSEPORT_BPF: c_int = 0x0047;

pub const SO_PREFER_BUSY_POLL: c_int = 0x0048;
pub const SO_BUSY_POLL_BUDGET: c_int = 0x0049;

pub const SO_NETNS_COOKIE: c_int = 0x0050;

pub const SO_BUF_LOCK: c_int = 0x0051;

pub const SO_RESERVE_MEM: c_int = 0x0052;

pub const SO_TXREHASH: c_int = 0x0053;

pub const SO_RCVMARK: c_int = 0x0054;

pub const SO_PASSPIDFD: c_int = 0x0055;
pub const SO_PEERPIDFD: c_int = 0x0056;

pub const SO_DEVMEM_LINEAR: c_int = 0x0057;
pub const SCM_DEVMEM_LINEAR: c_int = SO_DEVMEM_LINEAR;
pub const SO_DEVMEM_DMABUF: c_int = 0x0058;
pub const SCM_DEVMEM_DMABUF: c_int = SO_DEVMEM_DMABUF;
pub const SO_DEVMEM_DONTNEED: c_int = 0x0059;

pub const SCM_TS_OPT_ID: c_int = 0x005a;

pub const SO_RCVPRIORITY: c_int = 0x005b;

pub const SO_PASSRIGHTS: c_int = 0x005c;

pub const SO_INQ: c_int = 0x005d;
pub const SCM_INQ: c_int = SO_INQ;

cfg_if! {
    if #[cfg(target_pointer_width = "64")] {
        pub const SO_TIMESTAMP: c_int = SO_TIMESTAMP_OLD;
        pub const SO_TIMESTAMPNS: c_int = SO_TIMESTAMPNS_OLD;
        pub const SO_TIMESTAMPING: c_int = SO_TIMESTAMPING_OLD;

        pub const SO_RCVTIMEO: c_int = SO_RCVTIMEO_OLD;
        pub const SO_SNDTIMEO: c_int = SO_SNDTIMEO_OLD;
    } else {
        #[allow(deprecated)]
        const TIME_T_LONG: bool = size_of::<crate::time_t>() == size_of::<c_long>();

        const SO_TIMESTAMP_NEW: c_int = 0x0046;
        const SO_TIMESTAMPNS_NEW: c_int = 0x0042;
        const SO_TIMESTAMPING_NEW: c_int = 0x0043;

        const SO_RCVTIMEO_NEW: c_int = 0x0044;
        const SO_SNDTIMEO_NEW: c_int = 0x0045;

        pub const SO_TIMESTAMP: c_int = if TIME_T_LONG {
            SO_TIMESTAMP_OLD
        } else {
            SO_TIMESTAMP_NEW
        };
        pub const SO_TIMESTAMPNS: c_int = if TIME_T_LONG {
            SO_TIMESTAMPNS_OLD
        } else {
            SO_TIMESTAMPNS_NEW
        };
        pub const SO_TIMESTAMPING: c_int = if TIME_T_LONG {
            SO_TIMESTAMPING_OLD
        } else {
            SO_TIMESTAMPING_NEW
        };

        pub const SO_RCVTIMEO: c_int = if TIME_T_LONG {
            SO_RCVTIMEO_OLD
        } else {
            SO_RCVTIMEO_NEW
        };
        pub const SO_SNDTIMEO: c_int = if TIME_T_LONG {
            SO_SNDTIMEO_OLD
        } else {
            SO_SNDTIMEO_NEW
        };
    }
}

pub const SCM_TIMESTAMP: c_int = SO_TIMESTAMP;
pub const SCM_TIMESTAMPNS: c_int = SO_TIMESTAMPNS;
pub const SCM_TIMESTAMPING: c_int = SO_TIMESTAMPING;
