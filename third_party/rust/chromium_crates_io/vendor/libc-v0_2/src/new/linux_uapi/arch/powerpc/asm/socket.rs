//! Header: `arch/powerpc/include/uapi/asm/socket.h`

use crate::prelude::*;

pub const SO_RCVLOWAT: c_int = 16;
pub const SO_SNDLOWAT: c_int = 17;
const SO_RCVTIMEO_OLD: c_int = 18;
const SO_SNDTIMEO_OLD: c_int = 19;
pub const SO_PASSCRED: c_int = 20;
pub const SO_PEERCRED: c_int = 21;

#[path = "../../../asm/socket.rs"]
mod asm_generic;

pub use asm_generic::*;
