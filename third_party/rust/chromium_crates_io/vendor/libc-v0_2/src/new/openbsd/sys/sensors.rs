//! Header: `sys/sensors.h`
//!
//! <https://github.com/openbsd/src/blob/master/sys/sys/sensors.h>

use crate::prelude::*;
use crate::timeval;

pub const SENSOR_FINVALID: c_int = 0x0001;
pub const SENSOR_FUNKNOWN: c_int = 0x0002;

c_enum! {
    pub enum sensor_status {
        pub SENSOR_S_UNSPEC,
        pub SENSOR_S_OK,
        pub SENSOR_S_WARN,
        pub SENSOR_S_CRIT,
        pub SENSOR_S_UNKNOWN,
    }
}

c_enum! {
    pub enum sensor_type {
        pub SENSOR_TEMP,
        pub SENSOR_FANRPM,
        pub SENSOR_VOLTS_DC,
        pub SENSOR_VOLTS_AC,
        pub SENSOR_OHMS,
        pub SENSOR_WATTS,
        pub SENSOR_AMPS,
        pub SENSOR_WATTHOUR,
        pub SENSOR_AMPHOUR,
        pub SENSOR_INDICATOR,
        pub SENSOR_INTEGER,
        pub SENSOR_PERCENT,
        pub SENSOR_LUX,
        pub SENSOR_DRIVE,
        pub SENSOR_TIMEDELTA,
        pub SENSOR_HUMIDITY,
        pub SENSOR_FREQ,
        pub SENSOR_ANGLE,
        pub SENSOR_DISTANCE,
        pub SENSOR_PRESSURE,
        pub SENSOR_ACCEL,
        pub SENSOR_VELOCITY,
        pub SENSOR_ENERGY,
        pub SENSOR_MAX_TYPES,
    }
}

pub const SENSOR_DRIVE_EMPTY: c_int = 1;
pub const SENSOR_DRIVE_READY: c_int = 2;
pub const SENSOR_DRIVE_POWERUP: c_int = 3;
pub const SENSOR_DRIVE_ONLINE: c_int = 4;
pub const SENSOR_DRIVE_IDLE: c_int = 5;
pub const SENSOR_DRIVE_ACTIVE: c_int = 6;
pub const SENSOR_DRIVE_REBUILD: c_int = 7;
pub const SENSOR_DRIVE_POWERDOWN: c_int = 8;
pub const SENSOR_DRIVE_FAIL: c_int = 9;
pub const SENSOR_DRIVE_PFAIL: c_int = 10;

s! {
    pub struct sensor {
        pub desc: [c_char; 32],
        pub tv: timeval,
        pub value: i64,
        pub type_: sensor_type,
        pub status: sensor_status,
        pub numt: c_int,
        pub flags: c_int,
    }
}
