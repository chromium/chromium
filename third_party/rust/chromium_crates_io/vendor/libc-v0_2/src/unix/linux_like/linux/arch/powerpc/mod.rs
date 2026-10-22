use crate::prelude::*;
use crate::Ioctl;

// Ioctl Constants

cfg_if! {
    if #[cfg(target_env = "gnu")] {
        pub const TCGETS: Ioctl = 0x403c7413;
        pub const TCSETS: Ioctl = u32_cast_ioctl(0x803c7414);
        pub const TCSETSW: Ioctl = u32_cast_ioctl(0x803c7415);
        pub const TCSETSF: Ioctl = u32_cast_ioctl(0x803c7416);
    } else if #[cfg(target_env = "musl")] {
        pub const TCGETS: Ioctl = 0x402c7413;
        pub const TCSETS: Ioctl = u32_cast_ioctl(0x802c7414);
        pub const TCSETSW: Ioctl = u32_cast_ioctl(0x802c7415);
        pub const TCSETSF: Ioctl = u32_cast_ioctl(0x802c7416);
    }
}

pub const TCGETA: Ioctl = 0x40147417;
pub const TCSETA: Ioctl = u32_cast_ioctl(0x80147418);
pub const TCSETAW: Ioctl = u32_cast_ioctl(0x80147419);
pub const TCSETAF: Ioctl = u32_cast_ioctl(0x8014741C);
pub const TCSBRK: Ioctl = 0x2000741D;
pub const TCXONC: Ioctl = 0x2000741E;
pub const TCFLSH: Ioctl = 0x2000741F;
pub const TIOCEXCL: Ioctl = 0x540C;
pub const TIOCNXCL: Ioctl = 0x540D;
pub const TIOCSCTTY: Ioctl = 0x540E;
pub const TIOCGPGRP: Ioctl = 0x40047477;
pub const TIOCSPGRP: Ioctl = u32_cast_ioctl(0x80047476);
pub const TIOCOUTQ: Ioctl = 0x40047473;
pub const TIOCSTI: Ioctl = 0x5412;
pub const TIOCGWINSZ: Ioctl = 0x40087468;
pub const TIOCSWINSZ: Ioctl = u32_cast_ioctl(0x80087467);
pub const TIOCMGET: Ioctl = 0x5415;
pub const TIOCMBIS: Ioctl = 0x5416;
pub const TIOCMBIC: Ioctl = 0x5417;
pub const TIOCMSET: Ioctl = 0x5418;
pub const TIOCGSOFTCAR: Ioctl = 0x5419;
pub const TIOCSSOFTCAR: Ioctl = 0x541A;
pub const FIONREAD: Ioctl = 0x4004667F;
pub const TIOCINQ: Ioctl = FIONREAD;
pub const TIOCLINUX: Ioctl = 0x541C;
pub const TIOCCONS: Ioctl = 0x541D;
pub const TIOCGSERIAL: Ioctl = 0x541E;
pub const TIOCSSERIAL: Ioctl = 0x541F;
pub const TIOCPKT: Ioctl = 0x5420;
pub const FIONBIO: Ioctl = u32_cast_ioctl(0x8004667e);
pub const TIOCNOTTY: Ioctl = 0x5422;
pub const TIOCSETD: Ioctl = 0x5423;
pub const TIOCGETD: Ioctl = 0x5424;
pub const TCSBRKP: Ioctl = 0x5425;
pub const TIOCSBRK: Ioctl = 0x5427;
pub const TIOCCBRK: Ioctl = 0x5428;
pub const TIOCGSID: Ioctl = 0x5429;
pub const TIOCGRS485: Ioctl = 0x542e;
pub const TIOCSRS485: Ioctl = 0x542f;
pub const TIOCGPTN: Ioctl = 0x40045430;
pub const TIOCSPTLCK: Ioctl = u32_cast_ioctl(0x80045431);
pub const TIOCGDEV: Ioctl = 0x40045432;
pub const TIOCSIG: Ioctl = u32_cast_ioctl(0x80045436);
pub const TIOCVHANGUP: Ioctl = 0x5437;
pub const TIOCGPKT: Ioctl = 0x40045438;
pub const TIOCGPTLCK: Ioctl = 0x40045439;
pub const TIOCGEXCL: Ioctl = 0x40045440;
pub const TIOCGPTPEER: Ioctl = 0x20005441;
//pub const TIOCGISO7816: Ioctl = 0x40285442;
//pub const TIOCSISO7816: Ioctl = 0xc0285443;
pub const FIONCLEX: Ioctl = 0x20006602;
pub const FIOCLEX: Ioctl = 0x20006601;
pub const FIOASYNC: Ioctl = u32_cast_ioctl(0x8004667d);
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
pub const BLKIOMIN: Ioctl = 0x20001278;
pub const BLKIOOPT: Ioctl = 0x20001279;
pub const BLKSSZGET: Ioctl = 0x20001268;
pub const BLKPBSZGET: Ioctl = 0x2000127B;
//pub const FIOQSIZE: Ioctl = 0x40086680;

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

pub const BOTHER: crate::speed_t = 0o0037;
pub const IBSHIFT: crate::tcflag_t = 16;
pub const IUCLC: crate::tcflag_t = 0o0010000;
pub const XCASE: crate::tcflag_t = 0o0040000;

// RLIMIT Constants

cfg_if! {
    if #[cfg(target_env = "gnu")] {
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
        #[deprecated(since = "0.2.64", note = "Not stable across OS versions")]
        pub const RLIM_NLIMITS: crate::__rlimit_resource_t = 16;
        #[allow(deprecated)]
        #[deprecated(since = "0.2.64", note = "Not stable across OS versions")]
        pub const RLIMIT_NLIMITS: crate::__rlimit_resource_t = RLIM_NLIMITS;
    } else if #[cfg(target_env = "musl")] {
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
pub const RLIM_INFINITY: crate::rlim_t = !0;
