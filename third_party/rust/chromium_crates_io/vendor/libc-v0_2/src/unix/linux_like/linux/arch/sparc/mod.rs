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

pub const TCGETS: Ioctl = 0x40245408;
pub const TCSETS: Ioctl = 0x80245409;
pub const TCSETSW: Ioctl = 0x8024540a;
pub const TCSETSF: Ioctl = 0x8024540b;
pub const TCGETA: Ioctl = 0x40125401;
pub const TCSETA: Ioctl = 0x80125402;
pub const TCSETAW: Ioctl = 0x80125403;
pub const TCSETAF: Ioctl = 0x80125404;
pub const TCSBRK: Ioctl = 0x20005405;
pub const TCXONC: Ioctl = 0x20005406;
pub const TCFLSH: Ioctl = 0x20005407;
pub const TIOCEXCL: Ioctl = 0x2000740d;
pub const TIOCNXCL: Ioctl = 0x2000740e;
pub const TIOCSCTTY: Ioctl = 0x20007484;
pub const TIOCGPGRP: Ioctl = 0x40047483;
pub const TIOCSPGRP: Ioctl = 0x80047482;
pub const TIOCOUTQ: Ioctl = 0x40047473;
pub const TIOCSTI: Ioctl = 0x80017472;
pub const TIOCGWINSZ: Ioctl = 0x40087468;
pub const TIOCSWINSZ: Ioctl = 0x80087467;
pub const TIOCMGET: Ioctl = 0x4004746a;
pub const TIOCMBIS: Ioctl = 0x8004746c;
pub const TIOCMBIC: Ioctl = 0x8004746b;
pub const TIOCMSET: Ioctl = 0x8004746d;
pub const TIOCGSOFTCAR: Ioctl = 0x40047464;
pub const TIOCSSOFTCAR: Ioctl = 0x80047465;
pub const FIONREAD: Ioctl = 0x4004667f;
pub const TIOCINQ: Ioctl = FIONREAD;
pub const TIOCLINUX: Ioctl = 0x541C;
pub const TIOCCONS: Ioctl = 0x20007424;
pub const TIOCGSERIAL: Ioctl = 0x541E;
pub const TIOCSSERIAL: Ioctl = 0x541F;
pub const TIOCPKT: Ioctl = 0x80047470;
pub const FIONBIO: Ioctl = 0x8004667e;
pub const TIOCNOTTY: Ioctl = 0x20007471;
pub const TIOCSETD: Ioctl = 0x80047401;
pub const TIOCGETD: Ioctl = 0x40047400;
pub const TCSBRKP: Ioctl = 0x5425;
pub const TIOCSBRK: Ioctl = 0x2000747b;
pub const TIOCCBRK: Ioctl = 0x2000747a;
pub const TIOCGSID: Ioctl = 0x40047485;
pub const TCGETS2: Ioctl = u32_cast_ioctl(0x402c540c);
pub const TCSETS2: Ioctl = 0x802c540d;
pub const TCSETSW2: Ioctl = 0x802c540e;
pub const TCSETSF2: Ioctl = 0x802c540f;
pub const TIOCGPTN: Ioctl = 0x40047486;
pub const TIOCSPTLCK: Ioctl = 0x80047487;
pub const TIOCGDEV: Ioctl = 0x40045432;
pub const TIOCSIG: Ioctl = 0x80047488;
pub const TIOCVHANGUP: Ioctl = 0x20005437;
pub const TIOCGPKT: Ioctl = 0x40045438;
pub const TIOCGPTLCK: Ioctl = 0x40045439;
pub const TIOCGEXCL: Ioctl = 0x40045440;
pub const TIOCGPTPEER: Ioctl = 0x20007489;
pub const FIONCLEX: Ioctl = 0x20006602;
pub const FIOCLEX: Ioctl = 0x20006601;
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
pub const TIOCSTART: Ioctl = 0x2000746e;
pub const TIOCSTOP: Ioctl = 0x2000746f;
pub const BLKIOMIN: Ioctl = 0x20001278;
pub const BLKIOOPT: Ioctl = 0x20001279;
pub const BLKSSZGET: Ioctl = 0x20001268;
pub const BLKPBSZGET: Ioctl = 0x2000127B;

//pub const FIOASYNC: Ioctl = 0x4004667d;
//pub const FIOQSIZE: Ioctl = ;
//pub const TIOCGISO7816: Ioctl = 0x40285443;
//pub const TIOCSISO7816: Ioctl = 0xc0285444;
//pub const TIOCGRS485: Ioctl = 0x40205441;
//pub const TIOCSRS485: Ioctl = 0xc0205442;

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

pub const BOTHER: crate::speed_t = 0x1000;
pub const IBSHIFT: crate::tcflag_t = 16;
pub const IUCLC: crate::tcflag_t = 0o0001000;
pub const XCASE: crate::tcflag_t = 0o0000004;

// RLIMIT Constants

pub const RLIMIT_CPU: crate::__rlimit_resource_t = 0;
pub const RLIMIT_FSIZE: crate::__rlimit_resource_t = 1;
pub const RLIMIT_DATA: crate::__rlimit_resource_t = 2;
pub const RLIMIT_STACK: crate::__rlimit_resource_t = 3;
pub const RLIMIT_CORE: crate::__rlimit_resource_t = 4;
pub const RLIMIT_RSS: crate::__rlimit_resource_t = 5;
pub const RLIMIT_NOFILE: crate::__rlimit_resource_t = 6;
pub const RLIMIT_NPROC: crate::__rlimit_resource_t = 7;
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

cfg_if! {
    if #[cfg(target_arch = "sparc64")] {
        pub const RLIM_INFINITY: crate::rlim_t = !0;
    } else if #[cfg(target_arch = "sparc")] {
        pub const RLIM_INFINITY: crate::rlim_t = 0x7fffffff;
    }
}
