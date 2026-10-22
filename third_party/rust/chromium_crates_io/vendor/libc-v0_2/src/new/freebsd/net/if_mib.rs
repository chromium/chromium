//! Header: `net/if_mib.h`
//!
//! <https://github.com/freebsd/freebsd-src/blob/86691d52a6d3796ad36ba474cf0a9493f6d99202/sys/net/if_mib.h>

use crate::prelude::*;

/// non-interface-specific
pub const IFMIB_SYSTEM: c_int = 1;
/// per-interface data table
pub const IFMIB_IFDATA: c_int = 2;

/// generic stats for all kinds of ifaces
pub const IFDATA_GENERAL: c_int = 1;
/// specific to the type of interface
pub const IFDATA_LINKSPECIFIC: c_int = 2;
/// driver name and unit
pub const IFDATA_DRIVERNAME: c_int = 3;

/// number of interfaces configured
pub const IFMIB_IFCOUNT: c_int = 1;

/// functions not specific to a type of iface
pub const NETLINK_GENERIC: c_int = 0;

pub const DOT3COMPLIANCE_STATS: c_int = 1;
pub const DOT3COMPLIANCE_COLLS: c_int = 2;

pub const dot3ChipSetAMD7990: c_int = 1;
pub const dot3ChipSetAMD79900: c_int = 2;
pub const dot3ChipSetAMD79C940: c_int = 3;

pub const dot3ChipSetIntel82586: c_int = 1;
pub const dot3ChipSetIntel82596: c_int = 2;
pub const dot3ChipSetIntel82557: c_int = 3;

pub const dot3ChipSetNational8390: c_int = 1;
pub const dot3ChipSetNationalSonic: c_int = 2;

pub const dot3ChipSetFujitsu86950: c_int = 1;

pub const dot3ChipSetDigitalDC21040: c_int = 1;
pub const dot3ChipSetDigitalDC21140: c_int = 2;
pub const dot3ChipSetDigitalDC21041: c_int = 3;
pub const dot3ChipSetDigitalDC21140A: c_int = 4;
pub const dot3ChipSetDigitalDC21142: c_int = 5;

pub const dot3ChipSetWesternDigital83C690: c_int = 1;
pub const dot3ChipSetWesternDigital83C790: c_int = 2;
