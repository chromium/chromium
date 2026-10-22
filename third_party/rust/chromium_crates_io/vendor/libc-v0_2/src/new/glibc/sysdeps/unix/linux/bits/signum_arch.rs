//! Header: `sysdeps/unix/sysv/linux/bits/signum-arch.h`

use crate::prelude::*;

pub const SIGSTKFLT: c_int = 16;
pub const SIGPWR: c_int = 30;

pub const SIGBUS: c_int = 7;
pub const SIGSYS: c_int = 31;

pub const SIGURG: c_int = 23;
pub const SIGSTOP: c_int = 19;
pub const SIGTSTP: c_int = 20;
pub const SIGCONT: c_int = 18;
pub const SIGCHLD: c_int = 17;
pub const SIGTTIN: c_int = 21;
pub const SIGTTOU: c_int = 22;
pub const SIGPOLL: c_int = 29;
pub const SIGXFSZ: c_int = 25;
pub const SIGXCPU: c_int = 24;
pub const SIGVTALRM: c_int = 26;
pub const SIGPROF: c_int = 27;
pub const SIGUSR1: c_int = 10;
pub const SIGUSR2: c_int = 12;

pub const SIGWINCH: c_int = 28;

pub const SIGIO: c_int = SIGPOLL;
