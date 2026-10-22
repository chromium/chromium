//! Header: `netinet/tcp.h`
//!
//! <https://github.com/apple-oss-distributions/xnu/blob/main/bsd/netinet/tcp.h>

use crate::prelude::*;

// TCP header flags. These are numbered as if `th_x2` and `th_flags` were one
// field, with `th_flags` as the low octet, so `TH_AE` (0x100) and `TH_ACE`
// (which spans both) do not fit in `th_flags` alone.
pub const TH_FIN: c_int = 0x01;
pub const TH_SYN: c_int = 0x02;
pub const TH_RST: c_int = 0x04;
pub const TH_PUSH: c_int = 0x08;
pub const TH_ACK: c_int = 0x10;
pub const TH_URG: c_int = 0x20;
pub const TH_ECE: c_int = 0x40;
pub const TH_CWR: c_int = 0x80;
pub const TH_AE: c_int = 0x100;

pub const TH_FLAGS: c_int = TH_FIN | TH_SYN | TH_RST | TH_ACK | TH_URG | TH_ECE | TH_CWR;
pub const TH_FLAGS_ALL: c_int = TH_FLAGS | TH_PUSH;
pub const TH_ACCEPT: c_int = TH_FIN | TH_SYN | TH_RST | TH_ACK;
pub const TH_ACE: c_int = TH_AE | TH_CWR | TH_ECE;

// TCP option kinds and their lengths.
pub const TCPOPT_EOL: c_int = 0;
pub const TCPOLEN_EOL: c_int = 1;
pub const TCPOPT_NOP: c_int = 1;
pub const TCPOLEN_NOP: c_int = 1;
pub const TCPOPT_MAXSEG: c_int = 2;
pub const TCPOLEN_MAXSEG: c_int = 4;
pub const TCPOPT_WINDOW: c_int = 3;
pub const TCPOLEN_WINDOW: c_int = 3;
/// SACK capability in SYN.
pub const TCPOPT_SACK_PERMITTED: c_int = 4;
pub const TCPOLEN_SACK_PERMITTED: c_int = 2;
pub const TCPOPT_SACK: c_int = 5;
pub const TCPOLEN_SACKHDR: c_int = 2;
/// Length of one SACK block.
pub const TCPOLEN_SACK: c_int = 8;
pub const TCPOPT_TIMESTAMP: c_int = 8;
pub const TCPOLEN_TIMESTAMP: c_int = 10;
/// RFC 1323 appendix A.
pub const TCPOLEN_TSTAMP_APPA: c_int = TCPOLEN_TIMESTAMP + 2;
pub const TCPOPT_TSTAMP_HDR: c_int =
    (TCPOPT_NOP << 24) | (TCPOPT_NOP << 16) | (TCPOPT_TIMESTAMP << 8) | TCPOLEN_TIMESTAMP;

/// Absolute maximum TCP options length.
pub const MAX_TCPOPTLEN: c_int = 40;

/// CC options: RFC 1644.
pub const TCPOPT_CC: c_int = 11;
pub const TCPOPT_CCNEW: c_int = 12;
pub const TCPOPT_CCECHO: c_int = 13;
pub const TCPOLEN_CC: c_int = 6;
pub const TCPOLEN_CC_APPA: c_int = TCPOLEN_CC + 2;

/// Keyed MD5: RFC 2385.
pub const TCPOPT_SIGNATURE: c_int = 19;
pub const TCPOLEN_SIGNATURE: c_int = 18;

pub const TCPOPT_FASTOPEN: c_int = 34;
pub const TCPOLEN_FASTOPEN_REQ: c_int = 2;

/// AccECN order 0.
pub const TCPOPT_ACCECN0: c_int = 172;
/// AccECN order 1.
pub const TCPOPT_ACCECN1: c_int = 174;
/// Empty option contains kind and length.
pub const TCPOLEN_ACCECN_EMPTY: c_int = 2;
/// Length of each AccECN counter.
pub const TCPOLEN_ACCECN_COUNTER: c_int = 3;

pub const TCPOPT_SACK_PERMIT_HDR: c_int =
    (TCPOPT_NOP << 24) | (TCPOPT_NOP << 16) | (TCPOPT_SACK_PERMITTED << 8) | TCPOLEN_SACK_PERMITTED;
pub const TCPOPT_SACK_HDR: c_int = (TCPOPT_NOP << 24) | (TCPOPT_NOP << 16) | (TCPOPT_SACK << 8);

/// Maximum number of SACK blocks stored at the sender.
///
/// Constants may change across releases. See the [usage guidelines](crate#usage-guidelines)
/// for details.
pub const MAX_SACK_BLKS: c_int = 6;
/// Maximum number of SACKs sent in any segment.
pub const TCP_MAX_SACK: c_int = 4;
