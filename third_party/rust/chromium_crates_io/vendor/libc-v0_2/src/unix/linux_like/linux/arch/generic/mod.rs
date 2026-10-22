use crate::prelude::*;
use crate::Ioctl;

s! {
    pub struct termios2 {
        pub c_iflag: crate::tcflag_t,
        pub c_oflag: crate::tcflag_t,
        pub c_cflag: crate::tcflag_t,
        pub c_lflag: crate::tcflag_t,
        pub c_line: crate::cc_t,
        pub c_cc: [crate::cc_t; 19],
        pub c_ispeed: crate::speed_t,
        pub c_ospeed: crate::speed_t,
    }
}

// Ioctl Constants

pub const TCGETS: Ioctl = 0x5401;
pub const TCSETS: Ioctl = 0x5402;
pub const TCSETSW: Ioctl = 0x5403;
pub const TCSETSF: Ioctl = 0x5404;
pub const TCGETA: Ioctl = 0x5405;
pub const TCSETA: Ioctl = 0x5406;
pub const TCSETAW: Ioctl = 0x5407;
pub const TCSETAF: Ioctl = 0x5408;
pub const TCSBRK: Ioctl = 0x5409;
pub const TCXONC: Ioctl = 0x540A;
pub const TCFLSH: Ioctl = 0x540B;
pub const TIOCEXCL: Ioctl = 0x540C;
pub const TIOCNXCL: Ioctl = 0x540D;
pub const TIOCSCTTY: Ioctl = 0x540E;
pub const TIOCGPGRP: Ioctl = 0x540F;
pub const TIOCSPGRP: Ioctl = 0x5410;
pub const TIOCOUTQ: Ioctl = 0x5411;
pub const TIOCSTI: Ioctl = 0x5412;
pub const TIOCGWINSZ: Ioctl = 0x5413;
pub const TIOCSWINSZ: Ioctl = 0x5414;
pub const TIOCMGET: Ioctl = 0x5415;
pub const TIOCMBIS: Ioctl = 0x5416;
pub const TIOCMBIC: Ioctl = 0x5417;
pub const TIOCMSET: Ioctl = 0x5418;
pub const TIOCGSOFTCAR: Ioctl = 0x5419;
pub const TIOCSSOFTCAR: Ioctl = 0x541A;
pub const FIONREAD: Ioctl = 0x541B;
pub const TIOCINQ: Ioctl = FIONREAD;
pub const TIOCLINUX: Ioctl = 0x541C;
pub const TIOCCONS: Ioctl = 0x541D;
pub const TIOCGSERIAL: Ioctl = 0x541E;
pub const TIOCSSERIAL: Ioctl = 0x541F;
pub const TIOCPKT: Ioctl = 0x5420;
pub const FIONBIO: Ioctl = 0x5421;
pub const TIOCNOTTY: Ioctl = 0x5422;
pub const TIOCSETD: Ioctl = 0x5423;
pub const TIOCGETD: Ioctl = 0x5424;
pub const TCSBRKP: Ioctl = 0x5425;
pub const TIOCSBRK: Ioctl = 0x5427;
pub const TIOCCBRK: Ioctl = 0x5428;
pub const TIOCGSID: Ioctl = 0x5429;
pub const TCGETS2: Ioctl = u32_cast_ioctl(0x802c542a);
pub const TCSETS2: Ioctl = 0x402c542b;
pub const TCSETSW2: Ioctl = 0x402c542c;
pub const TCSETSF2: Ioctl = 0x402c542d;
pub const TIOCGRS485: Ioctl = 0x542E;
pub const TIOCSRS485: Ioctl = 0x542F;
pub const TIOCGPTN: Ioctl = u32_cast_ioctl(0x80045430);
pub const TIOCSPTLCK: Ioctl = 0x40045431;
pub const TIOCGDEV: Ioctl = u32_cast_ioctl(0x80045432);
pub const TCGETX: Ioctl = 0x5432;
pub const TCSETX: Ioctl = 0x5433;
pub const TCSETXF: Ioctl = 0x5434;
pub const TCSETXW: Ioctl = 0x5435;
pub const TIOCSIG: Ioctl = 0x40045436;
pub const TIOCVHANGUP: Ioctl = 0x5437;
pub const TIOCGPKT: Ioctl = u32_cast_ioctl(0x80045438);
pub const TIOCGPTLCK: Ioctl = u32_cast_ioctl(0x80045439);
pub const TIOCGEXCL: Ioctl = u32_cast_ioctl(0x80045440);
pub const TIOCGPTPEER: Ioctl = 0x5441;
// pub const TIOCGISO7816: Ioctl = 0x80285442;
// pub const TIOCSISO7816: Ioctl = 0xc0285443;
pub const FIONCLEX: Ioctl = 0x5450;
pub const FIOCLEX: Ioctl = 0x5451;
pub const FIOASYNC: Ioctl = 0x5452;
pub const TIOCSERCONFIG: Ioctl = 0x5453;
pub const TIOCSERGWILD: Ioctl = 0x5454;
pub const TIOCSERSWILD: Ioctl = 0x5455;
pub const TIOCGLCKTRMIOS: Ioctl = 0x5456;
pub const TIOCSLCKTRMIOS: Ioctl = 0x5457;
pub const TIOCSERGSTRUCT: Ioctl = 0x5458;
pub const TIOCSERGETLSR: Ioctl = 0x5459;
pub const TIOCSERGETMULTI: Ioctl = 0x545A;
pub const TIOCSERSETMULTI: Ioctl = 0x545B;
pub const TIOCMIWAIT: Ioctl = 0x545C;
pub const TIOCGICOUNT: Ioctl = 0x545D;
pub const BLKIOMIN: Ioctl = 0x1278;
pub const BLKIOOPT: Ioctl = 0x1279;
pub const BLKSSZGET: Ioctl = 0x1268;
pub const BLKPBSZGET: Ioctl = 0x127B;

cfg_if! {
    if #[cfg(any(target_arch = "arm", target_arch = "s390x"))] {
        pub const FIOQSIZE: Ioctl = 0x545E;
    } else {
        pub const FIOQSIZE: Ioctl = 0x5460;
    }
}

pub const TIOCM_LE: c_int = 0x001;
pub const TIOCM_DTR: c_int = 0x002;
pub const TIOCM_RTS: c_int = 0x004;
pub const TIOCM_ST: c_int = 0x008;
pub const TIOCM_SR: c_int = 0x010;
pub const TIOCM_CTS: c_int = 0x020;
pub const TIOCM_CAR: c_int = 0x040;
pub const TIOCM_CD: c_int = TIOCM_CAR;
pub const TIOCM_RNG: c_int = 0x080;
pub const TIOCM_RI: c_int = TIOCM_RNG;
pub const TIOCM_DSR: c_int = 0x100;

pub const BOTHER: crate::speed_t = 0o010000;
pub const IBSHIFT: crate::tcflag_t = 16;
pub const IUCLC: crate::tcflag_t = 0o0001000;
pub const XCASE: crate::tcflag_t = 0o0000004;

// RLIMIT Constants

cfg_if! {
    if #[cfg(any(target_env = "gnu", target_env = "uclibc"))] {
        pub const RLIMIT_CPU: crate::__rlimit_resource_t = 0;
        pub const RLIMIT_FSIZE: crate::__rlimit_resource_t = 1;
        pub const RLIMIT_DATA: crate::__rlimit_resource_t = 2;
        pub const RLIMIT_STACK: crate::__rlimit_resource_t = 3;
        pub const RLIMIT_CORE: crate::__rlimit_resource_t = 4;
        pub const RLIMIT_RSS: crate::__rlimit_resource_t = 5;
        pub const RLIMIT_NPROC: crate::__rlimit_resource_t = 6;
        pub const RLIMIT_NOFILE: crate::__rlimit_resource_t = 7;
        pub const RLIMIT_MEMLOCK: crate::__rlimit_resource_t = 8;
        pub const RLIMIT_AS: crate::__rlimit_resource_t = 9;
        pub const RLIMIT_LOCKS: crate::__rlimit_resource_t = 10;
        pub const RLIMIT_SIGPENDING: crate::__rlimit_resource_t = 11;
        pub const RLIMIT_MSGQUEUE: crate::__rlimit_resource_t = 12;
        pub const RLIMIT_NICE: crate::__rlimit_resource_t = 13;
        pub const RLIMIT_RTPRIO: crate::__rlimit_resource_t = 14;
        pub const RLIMIT_RTTIME: crate::__rlimit_resource_t = 15;
        #[allow(deprecated)]
        #[deprecated(since = "0.2.64", note = "Not stable across OS versions")]
        pub const RLIMIT_NLIMITS: crate::__rlimit_resource_t = RLIM_NLIMITS;
    } else if #[cfg(any(target_env = "musl", target_env = "ohos"))] {
        pub const RLIMIT_CPU: c_int = 0;
        pub const RLIMIT_FSIZE: c_int = 1;
        pub const RLIMIT_DATA: c_int = 2;
        pub const RLIMIT_STACK: c_int = 3;
        pub const RLIMIT_CORE: c_int = 4;
        pub const RLIMIT_RSS: c_int = 5;
        pub const RLIMIT_NPROC: c_int = 6;
        pub const RLIMIT_NOFILE: c_int = 7;
        pub const RLIMIT_MEMLOCK: c_int = 8;
        pub const RLIMIT_AS: c_int = 9;
        pub const RLIMIT_LOCKS: c_int = 10;
        pub const RLIMIT_SIGPENDING: c_int = 11;
        pub const RLIMIT_MSGQUEUE: c_int = 12;
        pub const RLIMIT_NICE: c_int = 13;
        pub const RLIMIT_RTPRIO: c_int = 14;
        pub const RLIMIT_RTTIME: c_int = 15;
        #[deprecated(since = "0.2.64", note = "Not stable across OS versions")]
        pub const RLIM_NLIMITS: c_int = 16;
        #[allow(deprecated)]
        #[deprecated(since = "0.2.64", note = "Not stable across OS versions")]
        pub const RLIMIT_NLIMITS: c_int = RLIM_NLIMITS;
    }
}

cfg_if! {
    if #[cfg(target_env = "gnu")] {
        #[deprecated(since = "0.2.64", note = "Not stable across OS versions")]
        pub const RLIM_NLIMITS: crate::__rlimit_resource_t = 16;
    } else if #[cfg(target_env = "uclibc")] {
        #[deprecated(since = "0.2.64", note = "Not stable across OS versions")]
        pub const RLIM_NLIMITS: crate::__rlimit_resource_t = 15;
    }
}

pub const RLIM_INFINITY: crate::rlim_t = !0;
