//! Header: `net/route.h`
//!
//! <https://github.com/freebsd/freebsd-src/blob/main/sys/net/route.h>

use crate::prelude::*;

s2! {
    pub struct rt_metrics {
        pub rmx_locks: c_ulong,
        pub rmx_mtu: c_ulong,
        pub rmx_hopcount: c_ulong,
        pub rmx_expire: c_ulong,
        pub rmx_recvpipe: c_ulong,
        pub rmx_sendpipe: c_ulong,
        pub rmx_ssthresh: c_ulong,
        pub rmx_rtt: c_ulong,
        pub rmx_rttvar: c_ulong,
        pub rmx_pksent: c_ulong,
        pub rmx_weight: c_ulong,
        pub rmx_nhidx: c_ulong,
        rmx_filler: Padding<[c_ulong; 2]>,
    }

    pub struct rt_msghdr {
        pub rtm_msglen: c_ushort,
        pub rtm_version: c_uchar,
        pub rtm_type: c_uchar,
        pub rtm_index: c_ushort,
        _rtm_spare1: Padding<c_ushort>,
        pub rtm_flags: c_int,
        pub rtm_addrs: c_int,
        pub rtm_pid: crate::pid_t,
        pub rtm_seq: c_int,
        pub rtm_errno: c_int,
        pub rtm_fmask: c_int,
        pub rtm_inits: c_ulong,
        pub rtm_rmx: rt_metrics,
    }
}
