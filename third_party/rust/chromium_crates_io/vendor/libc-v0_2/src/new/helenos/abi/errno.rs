//! HelenOS errno codes
//!
//! * Header file: <https://github.com/HelenOS/helenos/tree/master/abi/include/abi/errno.h>

use crate::errno_t;

pub const EOK: errno_t = 0;
pub const ENOENT: errno_t = 1;
pub const ENOMEM: errno_t = 2;
pub const ELIMIT: errno_t = 3;
pub const EREFUSED: errno_t = 4;
pub const EFORWARD: errno_t = 5;
pub const EPERM: errno_t = 6;
pub const EHANGUP: errno_t = 7;
pub const EPARTY: errno_t = 8;
pub const EEXIST: errno_t = 9;
pub const EBADMEM: errno_t = 10;
pub const ENOTSUP: errno_t = 11;
pub const EADDRNOTAVAIL: errno_t = 12;
pub const ETIMEOUT: errno_t = 13;
pub const EINVAL: errno_t = 14;
pub const EBUSY: errno_t = 15;
pub const EOVERFLOW: errno_t = 16;
pub const EINTR: errno_t = 17;
pub const EMFILE: errno_t = 18;
pub const ENAMETOOLONG: errno_t = 19;
pub const EISDIR: errno_t = 20;
pub const ENOTDIR: errno_t = 21;
pub const ENOSPC: errno_t = 22;
pub const ENOTEMPTY: errno_t = 23;
pub const EBADF: errno_t = 24;
pub const EDOM: errno_t = 25;
pub const ERANGE: errno_t = 26;
pub const EXDEV: errno_t = 27;
pub const EIO: errno_t = 28;
pub const EMLINK: errno_t = 29;
pub const ENXIO: errno_t = 30;
pub const ENOFS: errno_t = 31;
pub const EBADCHECKSUM: errno_t = 32;
pub const ESTALL: errno_t = 33;
pub const EEMPTY: errno_t = 34;
pub const ENAK: errno_t = 35;
pub const EAGAIN: errno_t = 36;
