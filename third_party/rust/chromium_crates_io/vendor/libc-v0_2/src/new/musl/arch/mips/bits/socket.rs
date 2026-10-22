use crate::prelude::*;

pub const SOCK_STREAM: c_int = 2;
pub const SOCK_DGRAM: c_int = 1;

pub const SOCK_NONBLOCK: c_int = 0o200;
pub const SOCK_CLOEXEC: c_int = 0o2000000;
