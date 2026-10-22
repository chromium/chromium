use crate::prelude::*;
use crate::Ioctl;

s! {
    pub struct termios2 {
        pub c_iflag: crate::tcflag_t,
        pub c_oflag: crate::tcflag_t,
        pub c_cflag: crate::tcflag_t,
        pub c_lflag: crate::tcflag_t,
        pub c_line: crate::cc_t,
        pub c_cc: [crate::cc_t; 23],
        pub c_ispeed: crate::speed_t,
        pub c_ospeed: crate::speed_t,
    }
}

// Ioctl Constants

pub const TCGETS: Ioctl = 0x540d;
pub const TCSETS: Ioctl = 0x540e;
pub const TCSETSW: Ioctl = 0x540f;
pub const TCSETSF: Ioctl = 0x5410;
pub const TCGETA: Ioctl = 0x5401;
pub const TCSETA: Ioctl = 0x5402;
pub const TCSETAW: Ioctl = 0x5403;
pub const TCSETAF: Ioctl = 0x5404;
pub const TCSBRK: Ioctl = 0x5405;
pub const TCXONC: Ioctl = 0x5406;
pub const TCFLSH: Ioctl = 0x5407;
pub const TIOCEXCL: Ioctl = 0x740d;
pub const TIOCNXCL: Ioctl = 0x740e;
pub const TIOCSCTTY: Ioctl = 0x5480;
pub const TIOCGPGRP: Ioctl = 0x40047477;
pub const TIOCSPGRP: Ioctl = u32_cast_ioctl(0x80047476);
pub const TIOCOUTQ: Ioctl = 0x7472;
pub const TIOCSTI: Ioctl = 0x5472;
pub const TIOCGWINSZ: Ioctl = 0x40087468;
pub const TIOCSWINSZ: Ioctl = u32_cast_ioctl(0x80087467);
pub const TIOCMGET: Ioctl = 0x741d;
pub const TIOCMBIS: Ioctl = 0x741b;
pub const TIOCMBIC: Ioctl = 0x741c;
pub const TIOCMSET: Ioctl = 0x741a;
pub const TIOCGSOFTCAR: Ioctl = 0x5481;
pub const TIOCSSOFTCAR: Ioctl = 0x5482;
pub const FIONREAD: Ioctl = 0x467f;
pub const TIOCINQ: Ioctl = FIONREAD;
pub const TIOCLINUX: Ioctl = 0x5483;
pub const TIOCCONS: Ioctl = u32_cast_ioctl(0x80047478);
pub const TIOCGSERIAL: Ioctl = 0x5484;
pub const TIOCSSERIAL: Ioctl = 0x5485;
pub const TIOCPKT: Ioctl = 0x5470;
pub const FIONBIO: Ioctl = 0x667e;
pub const TIOCNOTTY: Ioctl = 0x5471;
pub const TIOCSETD: Ioctl = 0x7401;
pub const TIOCGETD: Ioctl = 0x7400;
pub const TCSBRKP: Ioctl = 0x5486;
pub const TIOCSBRK: Ioctl = 0x5427;
pub const TIOCCBRK: Ioctl = 0x5428;
pub const TIOCGSID: Ioctl = 0x7416;
pub const TCGETS2: Ioctl = u32_cast_ioctl(0x4030542a);
pub const TCSETS2: Ioctl = u32_cast_ioctl(0x8030542b);
pub const TCSETSW2: Ioctl = u32_cast_ioctl(0x8030542c);
pub const TCSETSF2: Ioctl = u32_cast_ioctl(0x8030542d);
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
pub const FIONCLEX: Ioctl = 0x6602;
pub const FIOCLEX: Ioctl = 0x6601;
pub const FIOASYNC: Ioctl = 0x667d;
pub const TIOCSERCONFIG: Ioctl = 0x5488;
pub const TIOCSERGWILD: Ioctl = 0x5489;
pub const TIOCSERSWILD: Ioctl = 0x548a;
pub const TIOCGLCKTRMIOS: Ioctl = 0x548b;
pub const TIOCSLCKTRMIOS: Ioctl = 0x548c;
pub const TIOCSERGSTRUCT: Ioctl = 0x548d;
pub const TIOCSERGETLSR: Ioctl = 0x548e;
pub const TIOCSERGETMULTI: Ioctl = 0x548f;
pub const TIOCSERSETMULTI: Ioctl = 0x5490;
pub const TIOCMIWAIT: Ioctl = 0x5491;
pub const TIOCGICOUNT: Ioctl = 0x5492;
pub const FIOQSIZE: Ioctl = 0x667f;
pub const TIOCSLTC: Ioctl = 0x7475;
pub const TIOCGETP: Ioctl = 0x7408;
pub const TIOCSETP: Ioctl = 0x7409;
pub const TIOCSETN: Ioctl = 0x740a;
pub const BLKIOMIN: Ioctl = 0x20001278;
pub const BLKIOOPT: Ioctl = 0x20001279;
pub const BLKSSZGET: Ioctl = 0x20001268;
pub const BLKPBSZGET: Ioctl = 0x2000127B;

cfg_if! {
    if #[cfg(target_env = "musl")] {
        pub const TIOCGRS485: Ioctl = 0x4020542e;
        pub const TIOCSRS485: Ioctl = u32_cast_ioctl(0xc020542f);
    }
}

pub const TIOCM_LE: c_int = 0x001;
pub const TIOCM_DTR: c_int = 0x002;
pub const TIOCM_RTS: c_int = 0x004;
pub const TIOCM_ST: c_int = 0x010;
pub const TIOCM_SR: c_int = 0x020;
pub const TIOCM_CTS: c_int = 0x040;
pub const TIOCM_CAR: c_int = 0x100;
pub const TIOCM_CD: c_int = TIOCM_CAR;
pub const TIOCM_RNG: c_int = 0x200;
pub const TIOCM_RI: c_int = TIOCM_RNG;
pub const TIOCM_DSR: c_int = 0x400;

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
        pub const RLIMIT_NOFILE: crate::__rlimit_resource_t = 5;
        pub const RLIMIT_AS: crate::__rlimit_resource_t = 6;
        pub const RLIMIT_RSS: crate::__rlimit_resource_t = 7;
        pub const RLIMIT_NPROC: crate::__rlimit_resource_t = 8;
        pub const RLIMIT_MEMLOCK: crate::__rlimit_resource_t = 9;
        pub const RLIMIT_LOCKS: crate::__rlimit_resource_t = 10;
        pub const RLIMIT_SIGPENDING: crate::__rlimit_resource_t = 11;
        pub const RLIMIT_MSGQUEUE: crate::__rlimit_resource_t = 12;
        pub const RLIMIT_NICE: crate::__rlimit_resource_t = 13;
        pub const RLIMIT_RTPRIO: crate::__rlimit_resource_t = 14;
        pub const RLIMIT_RTTIME: crate::__rlimit_resource_t = 15;
        #[allow(deprecated)]
        #[deprecated(since = "0.2.64", note = "Not stable across OS versions")]
        pub const RLIMIT_NLIMITS: crate::__rlimit_resource_t = RLIM_NLIMITS;
    } else if #[cfg(target_env = "musl")] {
        pub const RLIMIT_CPU: c_int = 0;
        pub const RLIMIT_FSIZE: c_int = 1;
        pub const RLIMIT_DATA: c_int = 2;
        pub const RLIMIT_STACK: c_int = 3;
        pub const RLIMIT_CORE: c_int = 4;
        pub const RLIMIT_NOFILE: c_int = 5;
        pub const RLIMIT_AS: c_int = 6;
        pub const RLIMIT_RSS: c_int = 7;
        pub const RLIMIT_NPROC: c_int = 8;
        pub const RLIMIT_MEMLOCK: c_int = 9;
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
        pub const RLIM_INFINITY: crate::rlim_t = !0;
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

cfg_if! {
    if #[cfg(
        any(target_arch = "mips64", target_arch = "mips64r6"),
        any(target_env = "gnu", target_env = "uclibc")
    )] {
        pub const RLIM_INFINITY: crate::rlim_t = !0;
    }
}

cfg_if! {
    if #[cfg(all(
        any(target_arch = "mips", target_arch = "mips32r6"),
        any(
            all(target_env = "uclibc", linux_time_bits64),
            all(
                target_env = "gnu",
                any(linux_time_bits64, gnu_file_offset_bits64)
            )
        )
    ))] {
        pub const RLIM_INFINITY: crate::rlim_t = !0;
    } else if #[cfg(all(
        any(target_arch = "mips", target_arch = "mips32r6"),
        any(target_env = "uclibc", target_env = "gnu"),
        not(linux_time_bits64)
    ))] {
        pub const RLIM_INFINITY: crate::rlim_t = 0x7fffffff;
    }
}
