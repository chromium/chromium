//! Hermit C type definitions

cfg_if! {
    if #[cfg(any(target_arch = "aarch64", target_arch = "riscv64"))] {
        pub type c_char = u8;
    } else {
        pub type c_char = i8;
    }
}

pub type c_schar = i8;
pub type c_uchar = u8;
pub type c_short = i16;
pub type c_ushort = u16;
pub type c_int = i32;
pub type c_uint = u32;
pub type c_long = i64;
pub type c_ulong = u64;
pub type c_longlong = i64;
pub type c_ulonglong = u64;
pub type intmax_t = i64;
pub type uintmax_t = u64;
pub type intptr_t = isize;
pub type uintptr_t = usize;

pub type c_float = f32;
pub type c_double = f64;

pub type size_t = usize;
pub type ssize_t = isize;
pub type ptrdiff_t = isize;

pub type clockid_t = i32;
pub type in_addr_t = u32;
pub type in_port_t = u16;
pub type mode_t = u32;
pub type nfds_t = usize;
pub type pid_t = i32;
pub type sa_family_t = u8;
pub type socklen_t = u32;
pub type time_t = i64;

s! {
    pub struct addrinfo {
        pub ai_flags: i32,
        pub ai_family: i32,
        pub ai_socktype: i32,
        pub ai_protocol: i32,
        pub ai_addrlen: socklen_t,
        pub ai_canonname: *mut c_char,
        pub ai_addr: *mut sockaddr,
        pub ai_next: *mut addrinfo,
    }

    pub struct dirent64 {
        pub d_ino: u64,
        pub d_off: i64,
        pub d_reclen: u16,
        pub d_type: u8,
        pub d_name: [c_char; 256],
    }

    #[repr(align(4))]
    pub struct in6_addr {
        pub s6_addr: [u8; 16],
    }

    pub struct in_addr {
        pub s_addr: in_addr_t,
    }

    pub struct iovec {
        iov_base: *mut c_void,
        iov_len: usize,
    }

    pub struct pollfd {
        pub fd: i32,
        pub events: i16,
        pub revents: i16,
    }

    pub struct sockaddr {
        pub sa_len: u8,
        pub sa_family: sa_family_t,
        pub sa_data: [c_char; 14],
    }

    pub struct sockaddr_in {
        pub sin_len: u8,
        pub sin_family: sa_family_t,
        pub sin_port: in_port_t,
        pub sin_addr: in_addr,
        pub sin_zero: [c_char; 8],
    }

    pub struct sockaddr_in6 {
        pub sin6_len: u8,
        pub sin6_family: sa_family_t,
        pub sin6_port: in_port_t,
        pub sin6_flowinfo: u32,
        pub sin6_addr: in6_addr,
        pub sin6_scope_id: u32,
    }

    pub struct sockaddr_storage {
        pub ss_len: u8,
        pub ss_family: sa_family_t,
        __ss_pad1: [u8; 6],
        __ss_align: i64,
        __ss_pad2: [u8; 112],
    }

    pub struct stat {
        pub st_dev: u64,
        pub st_ino: u64,
        pub st_nlink: u64,
        pub st_mode: mode_t,
        pub st_uid: u32,
        pub st_gid: u32,
        pub st_rdev: u64,
        pub st_size: u64,
        pub st_blksize: i64,
        pub st_blocks: i64,
        pub st_atim: timespec,
        pub st_mtim: timespec,
        pub st_ctim: timespec,
    }

    pub struct timespec {
        pub tv_sec: time_t,
        pub tv_nsec: i32,
    }
}

pub const AF_INET: i32 = 0;
pub const AF_INET6: i32 = 1;

pub const CLOCK_REALTIME: clockid_t = 1;
pub const CLOCK_MONOTONIC: clockid_t = 4;

pub const DT_UNKNOWN: u8 = 0;
pub const DT_FIFO: u8 = 1;
pub const DT_CHR: u8 = 2;
pub const DT_DIR: u8 = 4;
pub const DT_BLK: u8 = 6;
pub const DT_REG: u8 = 8;
pub const DT_LNK: u8 = 10;
pub const DT_SOCK: u8 = 12;
pub const DT_WHT: u8 = 14;

pub const EAI_AGAIN: i32 = 2;
pub const EAI_BADFLAGS: i32 = 3;
pub const EAI_FAIL: i32 = 4;
pub const EAI_FAMILY: i32 = 5;
pub const EAI_MEMORY: i32 = 6;
pub const EAI_NODATA: i32 = 7;
pub const EAI_NONAME: i32 = 8;
pub const EAI_SERVICE: i32 = 9;
pub const EAI_SOCKTYPE: i32 = 10;
pub const EAI_SYSTEM: i32 = 11;
pub const EAI_OVERFLOW: i32 = 14;

pub const EFD_SEMAPHORE: i16 = 0o1;
pub const EFD_NONBLOCK: i16 = 0o4000;
pub const EFD_CLOEXEC: i16 = 0o40000;

pub const F_DUPFD: i32 = 0;
pub const F_GETFD: i32 = 1;
pub const F_SETFD: i32 = 2;
pub const F_GETFL: i32 = 3;
pub const F_SETFL: i32 = 4;

pub const FD_CLOEXEC: i32 = 1;

pub const FIONBIO: i32 = 0x8008667e;

pub const FUTEX_RELATIVE_TIMEOUT: u32 = 1;

pub const IP_TOS: i32 = 1;
pub const IP_TTL: i32 = 2;
pub const IP_ADD_MEMBERSHIP: i32 = 3;
pub const IP_DROP_MEMBERSHIP: i32 = 4;
pub const IP_MULTICAST_TTL: i32 = 5;
pub const IP_MULTICAST_LOOP: i32 = 7;

pub const IPPROTO_IP: i32 = 0;
pub const IPPROTO_TCP: i32 = 6;
pub const IPPROTO_UDP: i32 = 17;
pub const IPPROTO_IPV6: i32 = 41;

pub const IPV6_ADD_MEMBERSHIP: i32 = 12;
pub const IPV6_DROP_MEMBERSHIP: i32 = 13;
pub const IPV6_MULTICAST_LOOP: i32 = 19;
pub const IPV6_V6ONLY: i32 = 27;

pub const MSG_PEEK: i32 = 1;

pub const O_RDONLY: i32 = 0o0;
pub const O_WRONLY: i32 = 0o1;
pub const O_RDWR: i32 = 0o2;
pub const O_CREAT: i32 = 0o100;
pub const O_EXCL: i32 = 0o200;
pub const O_TRUNC: i32 = 0o1000;
pub const O_APPEND: i32 = 0o2000;
pub const O_NONBLOCK: i32 = 0o4000;
pub const O_DIRECTORY: i32 = 0o200000;

pub const POLLIN: i16 = 0x1;
pub const POLLPRI: i16 = 0x2;
pub const POLLOUT: i16 = 0x4;
pub const POLLERR: i16 = 0x8;
pub const POLLHUP: i16 = 0x10;
pub const POLLNVAL: i16 = 0x20;
pub const POLLRDNORM: i16 = 0x040;
pub const POLLRDBAND: i16 = 0x080;
pub const POLLWRNORM: i16 = 0x0100;
pub const POLLWRBAND: i16 = 0x0200;
pub const POLLRDHUP: i16 = 0x2000;

pub const S_IRWXU: mode_t = 0o0700;
pub const S_IRUSR: mode_t = 0o0400;
pub const S_IWUSR: mode_t = 0o0200;
pub const S_IXUSR: mode_t = 0o0100;
pub const S_IRWXG: mode_t = 0o0070;
pub const S_IRGRP: mode_t = 0o0040;
pub const S_IWGRP: mode_t = 0o0020;
pub const S_IXGRP: mode_t = 0o0010;
pub const S_IRWXO: mode_t = 0o0007;
pub const S_IROTH: mode_t = 0o0004;
pub const S_IWOTH: mode_t = 0o0002;
pub const S_IXOTH: mode_t = 0o0001;

pub const S_IFMT: mode_t = 0o17_0000;
pub const S_IFSOCK: mode_t = 0o14_0000;
pub const S_IFLNK: mode_t = 0o12_0000;
pub const S_IFREG: mode_t = 0o10_0000;
pub const S_IFBLK: mode_t = 0o6_0000;
pub const S_IFDIR: mode_t = 0o4_0000;
pub const S_IFCHR: mode_t = 0o2_0000;
pub const S_IFIFO: mode_t = 0o1_0000;

pub const SHUT_RD: i32 = 0;
pub const SHUT_WR: i32 = 1;
pub const SHUT_RDWR: i32 = 2;

pub const SO_REUSEADDR: i32 = 0x0004;
pub const SO_KEEPALIVE: i32 = 0x0008;
pub const SO_BROADCAST: i32 = 0x0020;
pub const SO_LINGER: i32 = 0x0080;
pub const SO_SNDBUF: i32 = 0x1001;
pub const SO_RCVBUF: i32 = 0x1002;
pub const SO_SNDTIMEO: i32 = 0x1005;
pub const SO_RCVTIMEO: i32 = 0x1006;
pub const SO_ERROR: i32 = 0x1007;

pub const SOCK_STREAM: i32 = 1;
pub const SOCK_DGRAM: i32 = 2;
pub const SOCK_NONBLOCK: i32 = 0o4000;
pub const SOCK_CLOEXEC: i32 = 0o40000;

pub const SOL_SOCKET: i32 = 4095;

pub const STDIN_FILENO: c_int = 0;
pub const STDOUT_FILENO: c_int = 1;
pub const STDERR_FILENO: c_int = 2;

pub const TCP_NODELAY: i32 = 1;

extern "C" {
    #[link_name = "sys_alloc"]
    pub fn alloc(size: usize, align: usize) -> *mut u8;

    #[link_name = "sys_alloc_zeroed"]
    pub fn alloc_zeroed(size: usize, align: usize) -> *mut u8;

    #[link_name = "sys_realloc"]
    pub fn realloc(ptr: *mut u8, size: usize, align: usize, new_size: usize) -> *mut u8;

    #[link_name = "sys_dealloc"]
    pub fn dealloc(ptr: *mut u8, size: usize, align: usize);

    #[link_name = "sys_exit"]
    pub fn exit(status: i32) -> !;

    #[link_name = "sys_abort"]
    pub fn abort() -> !;

    #[link_name = "sys_errno"]
    pub fn errno() -> i32;

    #[link_name = "sys_clock_gettime"]
    pub fn clock_gettime(clockid: clockid_t, tp: *mut timespec) -> i32;

    #[link_name = "sys_nanosleep"]
    pub fn nanosleep(req: *const timespec) -> i32;

    #[link_name = "sys_available_parallelism"]
    pub fn available_parallelism() -> usize;

    #[link_name = "sys_futex_wait"]
    pub fn futex_wait(
        address: *mut u32,
        expected: u32,
        timeout: *const timespec,
        flags: u32,
    ) -> i32;

    #[link_name = "sys_futex_wake"]
    pub fn futex_wake(address: *mut u32, count: i32) -> i32;

    #[link_name = "sys_stat"]
    pub fn stat(path: *const c_char, stat: *mut stat) -> i32;

    #[link_name = "sys_fstat"]
    pub fn fstat(fd: i32, stat: *mut stat) -> i32;

    #[link_name = "sys_lstat"]
    pub fn lstat(path: *const c_char, stat: *mut stat) -> i32;

    #[link_name = "sys_open"]
    pub fn open(path: *const c_char, flags: i32, mode: mode_t) -> i32;

    #[link_name = "sys_unlink"]
    pub fn unlink(path: *const c_char) -> i32;

    #[link_name = "sys_mkdir"]
    pub fn mkdir(path: *const c_char, mode: mode_t) -> i32;

    #[link_name = "sys_rmdir"]
    pub fn rmdir(path: *const c_char) -> i32;

    #[link_name = "sys_read"]
    pub fn read(fd: i32, buf: *mut u8, len: usize) -> isize;

    #[link_name = "sys_write"]
    pub fn write(fd: i32, buf: *const u8, len: usize) -> isize;

    #[link_name = "sys_readv"]
    pub fn readv(fd: i32, iov: *const iovec, iovcnt: usize) -> isize;

    #[link_name = "sys_writev"]
    pub fn writev(fd: i32, iov: *const iovec, iovcnt: usize) -> isize;

    #[link_name = "sys_close"]
    pub fn close(fd: i32) -> i32;

    #[link_name = "sys_dup"]
    pub fn dup(fd: i32) -> i32;

    #[link_name = "sys_fcntl"]
    pub fn fcntl(fd: i32, cmd: i32, arg: i32) -> i32;

    #[link_name = "sys_getdents64"]
    pub fn getdents64(fd: i32, dirp: *mut dirent64, count: usize) -> isize;

    #[link_name = "sys_getaddrinfo"]
    pub fn getaddrinfo(
        nodename: *const c_char,
        servname: *const c_char,
        hints: *const addrinfo,
        res: *mut *mut addrinfo,
    ) -> i32;

    #[link_name = "sys_freeaddrinfo"]
    pub fn freeaddrinfo(ai: *mut addrinfo);

    #[link_name = "sys_socket"]
    pub fn socket(domain: i32, ty: i32, protocol: i32) -> i32;

    #[link_name = "sys_bind"]
    pub fn bind(sockfd: i32, addr: *const sockaddr, addrlen: socklen_t) -> i32;

    #[link_name = "sys_listen"]
    pub fn listen(sockfd: i32, backlog: i32) -> i32;

    #[link_name = "sys_accept"]
    pub fn accept(sockfd: i32, addr: *mut sockaddr, addrlen: *mut socklen_t) -> i32;

    #[link_name = "sys_connect"]
    pub fn connect(sockfd: i32, addr: *const sockaddr, addrlen: socklen_t) -> i32;

    #[link_name = "sys_recv"]
    pub fn recv(sockfd: i32, buf: *mut u8, len: usize, flags: i32) -> isize;

    #[link_name = "sys_recvfrom"]
    pub fn recvfrom(
        sockfd: i32,
        buf: *mut c_void,
        len: usize,
        flags: i32,
        addr: *mut sockaddr,
        addrlen: *mut socklen_t,
    ) -> isize;

    #[link_name = "sys_send"]
    pub fn send(sockfd: i32, buf: *const c_void, len: usize, flags: i32) -> isize;

    #[link_name = "sys_sendto"]
    pub fn sendto(
        sockfd: i32,
        buf: *const c_void,
        len: usize,
        flags: i32,
        to: *const sockaddr,
        tolen: socklen_t,
    ) -> isize;

    #[link_name = "sys_getpeername"]
    pub fn getpeername(sockfd: i32, addr: *mut sockaddr, addrlen: *mut socklen_t) -> i32;

    #[link_name = "sys_getsockname"]
    pub fn getsockname(sockfd: i32, addr: *mut sockaddr, addrlen: *mut socklen_t) -> i32;

    #[link_name = "sys_getsockopt"]
    pub fn getsockopt(
        sockfd: i32,
        level: i32,
        optname: i32,
        optval: *mut c_void,
        optlen: *mut socklen_t,
    ) -> i32;

    #[link_name = "sys_setsockopt"]
    pub fn setsockopt(
        sockfd: i32,
        level: i32,
        optname: i32,
        optval: *const c_void,
        optlen: socklen_t,
    ) -> i32;

    #[link_name = "sys_ioctl"]
    pub fn ioctl(sockfd: i32, cmd: i32, argp: *mut c_void) -> i32;

    #[link_name = "sys_shutdown"]
    pub fn shutdown(sockfd: i32, how: i32) -> i32;

    #[link_name = "sys_eventfd"]
    pub fn eventfd(initval: u64, flags: i16) -> i32;

    #[link_name = "sys_poll"]
    pub fn poll(fds: *mut pollfd, nfds: nfds_t, timeout: i32) -> i32;
}

pub use ffi::c_void;
