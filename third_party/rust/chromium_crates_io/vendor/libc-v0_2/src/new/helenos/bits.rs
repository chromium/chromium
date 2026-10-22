//! HelenOS ABI bits
//!
//! * Headers: <https://github.com/HelenOS/helenos/tree/master/abi/include/_bits>

use crate::prelude::*;

// `errno.h`
pub type errno_t = c_int;

// `native.h`
pub type sysarg_t = uintptr_t;
