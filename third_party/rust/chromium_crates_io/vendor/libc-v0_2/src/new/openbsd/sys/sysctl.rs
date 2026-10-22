//! Header: `sys/sysctl.h`
//!
//! <https://github.com/openbsd/src/blob/master/sys/sys/sysctl.h>

use crate::prelude::*;

pub const HW_MACHINE: c_int = 1;
pub const HW_MODEL: c_int = 2;
pub const HW_BYTEORDER: c_int = 4;
pub const HW_PHYSMEM: c_int = 5;
pub const HW_USERMEM: c_int = 6;
pub const HW_PAGESIZE: c_int = 7;
pub const HW_DISKNAMES: c_int = 8;
pub const HW_DISKSTATS: c_int = 9;
pub const HW_DISKCOUNT: c_int = 10;
pub const HW_SENSORS: c_int = 11;
pub const HW_CPUSPEED: c_int = 12;
pub const HW_SETPERF: c_int = 13;
pub const HW_VENDOR: c_int = 14;
pub const HW_PRODUCT: c_int = 15;
pub const HW_VERSION: c_int = 16;
pub const HW_SERIALNO: c_int = 17;
pub const HW_UUID: c_int = 18;
pub const HW_PHYSMEM64: c_int = 19;
pub const HW_USERMEM64: c_int = 20;
pub const HW_NCPUFOUND: c_int = 21;
pub const HW_PERFPOLICY: c_int = 23;
pub const HW_NCPUONLINE: c_int = 25;
pub const HW_POWER: c_int = 26;
pub const HW_BATTERY: c_int = 27;
pub const HW_UCOMNAMES: c_int = 28;

pub const HW_BATTERY_CHARGEMODE: c_int = 1;
pub const HW_BATTERY_CHARGESTART: c_int = 2;
pub const HW_BATTERY_CHARGESTOP: c_int = 3;
