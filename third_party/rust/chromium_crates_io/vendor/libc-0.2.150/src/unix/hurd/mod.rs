#![allow(dead_code)]

// types
pub type c_char = i8;

pub type __s16_type = ::c_short;
pub type __u16_type = ::c_ushort;
pub type __s32_type = ::c_int;
pub type __u32_type = ::c_uint;
pub type __slongword_type = ::c_long;
pub type __ulongword_type = ::c_ulong;

pub type __u_char = ::c_uchar;
pub type __u_short = ::c_ushort;
pub type __u_int = ::c_uint;
pub type __u_long = ::c_ulong;
pub type __int8_t = ::c_schar;
pub type __uint8_t = ::c_uchar;
pub type __int16_t = ::c_short;
pub type __uint16_t = ::c_ushort;
pub type __int32_t = ::c_int;
pub type __uint32_t = ::c_uint;
pub type __int_least8_t = __int8_t;
pub type __uint_least8_t = __uint8_t;
pub type __int_least16_t = __int16_t;
pub type __uint_least16_t = __uint16_t;
pub type __int_least32_t = __int32_t;
pub type __uint_least32_t = __uint32_t;
pub type __int_least64_t = __int64_t;
pub type __uint_least64_t = __uint64_t;

pub type __dev_t = __uword_type;
pub type __uid_t = __u32_type;
pub type __gid_t = __u32_type;
pub type __ino_t = __ulongword_type;
pub type __ino64_t = __uquad_type;
pub type __mode_t = __u32_type;
pub type __nlink_t = __uword_type;
pub type __off_t = __slongword_type;
pub type __off64_t = __squad_type;
pub type __pid_t = __s32_type;
pub type __rlim_t = __ulongword_type;
pub type __rlim64_t = __uquad_type;
pub type __blkcnt_t = __slongword_type;
pub type __blkcnt64_t = __squad_type;
pub type __fsblkcnt_t = __ulongword_type;
pub type __fsblkcnt64_t = __uquad_type;
pub type __fsfilcnt_t = __ulongword_type;
pub type __fsfilcnt64_t = __uquad_type;
pub type __fsword_t = __sword_type;
pub type __id_t = __u32_type;
pub type __clock_t = __slongword_type;
pub type __time_t = __slongword_type;
pub type __useconds_t = __u32_type;
pub type __suseconds_t = __slongword_type;
pub type __suseconds64_t = __squad_type;
pub type __daddr_t = __s32_type;
pub type __key_t = __s32_type;
pub type __clockid_t = __s32_type;
pub type __timer_t = __uword_type;
pub type __blksize_t = __slongword_type;
pub type __fsid_t = __uquad_type;
pub type __ssize_t = __sword_type;
pub type __syscall_slong_t = __slongword_type;
pub type __syscall_ulong_t = __ulongword_type;
pub type __cpu_mask = __ulongword_type;

pub type __loff_t = __off64_t;
pub type __caddr_t = *mut ::c_char;
pub type __intptr_t = __sword_type;
pub type __ptrdiff_t = __sword_type;
pub type __socklen_t = __u32_type;
pub type __sig_atomic_t = ::c_int;
pub type __time64_t = __int64_t;
pub type ssize_t = __ssize_t;
pub type size_t = ::c_ulong;
pub type wchar_t = ::c_int;
pub type wint_t = ::c_uint;
pub type gid_t = __gid_t;
pub type uid_t = __uid_t;
pub type off_t = __off_t;
pub type off64_t = __off64_t;
pub type useconds_t = __useconds_t;
pub type pid_t = __pid_t;
pub type socklen_t = __socklen_t;

pub type in_addr_t = u32;

pub type _Float32 = f32;
pub type _Float64 = f64;
pub type _Float32x = f64;
pub type _Float64x = f64;

pub type __locale_t = *mut __locale_struct;
pub type locale_t = __locale_t;

pub type u_char = __u_char;
pub type u_short = __u_short;
pub type u_int = __u_int;
pub type u_long = __u_long;
pub type quad_t = __quad_t;
pub type u_quad_t = __u_quad_t;
pub type fsid_t = __fsid_t;
pub type loff_t = __loff_t;
pub type ino_t = __ino_t;
pub type ino64_t = __ino64_t;
pub type dev_t = __dev_t;
pub type mode_t = __mode_t;
pub type nlink_t = __nlink_t;
pub type id_t = __id_t;
pub type daddr_t = __daddr_t;
pub type caddr_t = __caddr_t;
pub type key_t = __key_t;
pub type clock_t = __clock_t;
pub type clockid_t = __clockid_t;
pub type time_t = __time_t;
pub type timer_t = __timer_t;
pub type suseconds_t = __suseconds_t;
pub type ulong = ::c_ulong;
pub type ushort = ::c_ushort;
pub type uint = ::c_uint;
pub type u_int8_t = __uint8_t;
pub type u_int16_t = __uint16_t;
pub type u_int32_t = __uint32_t;
pub type u_int64_t = __uint64_t;
pub type register_t = ::c_int;
pub type __sigset_t = ::c_ulong;
pub type sigset_t = __sigset_t;

pub type __fd_mask = ::c_long;
pub type fd_mask = __fd_mask;
pub type blksize_t = __blksize_t;
pub type blkcnt_t = __blkcnt_t;
pub type fsblkcnt_t = __fsblkcnt_t;
pub type fsfilcnt_t = __fsfilcnt_t;
pub type blkcnt64_t = __blkcnt64_t;
pub type fsblkcnt64_t = __fsblkcnt64_t;
pub type fsfilcnt64_t = __fsfilcnt64_t;

pub type __pthread_spinlock_t = ::c_int;
pub type __tss_t = ::c_int;
pub type __thrd_t = ::c_long;
pub type __pthread_t = ::c_long;
pub type pthread_t = __pthread_t;
pub type __pthread_process_shared = ::c_uint;
pub type __pthread_inheritsched = ::c_uint;
pub type __pthread_contentionscope = ::c_uint;
pub type __pthread_detachstate = ::c_uint;
pub type pthread_attr_t = __pthread_attr;
pub type __pthread_mutex_protocol = ::c_uint;
pub type __pthread_mutex_type = ::c_uint;
pub type __pthread_mutex_robustness = ::c_uint;
pub type pthread_mutexattr_t = __pthread_mutexattr;
pub type pthread_mutex_t = __pthread_mutex;
pub type pthread_condattr_t = __pthread_condattr;
pub type pthread_cond_t = __pthread_cond;
pub type pthread_spinlock_t = __pthread_spinlock_t;
pub type pthread_rwlockattr_t = __pthread_rwlockattr;
pub type pthread_rwlock_t = __pthread_rwlock;
pub type pthread_barrierattr_t = __pthread_barrierattr;
pub type pthread_barrier_t = __pthread_barrier;
pub type __pthread_key = ::c_int;
pub type pthread_key_t = __pthread_key;
pub type pthread_once_t = __pthread_once;

pub type __rlimit_resource = ::c_uint;
pub type rlim_t = __rlim_t;
pub type rlim64_t = __rlim64_t;

pub type __rusage_who = ::c_int;

pub type __priority_which = ::c_uint;

pub type sa_family_t = ::c_uchar;

pub type in_port_t = u16;

pub type __sigval_t = ::sigval;

pub type sigevent_t = sigevent;

pub type nfds_t = ::c_ulong;

pub type tcflag_t = ::c_uint;
pub type cc_t = ::c_uchar;
pub type speed_t = ::c_int;

pub type sigval_t = ::sigval;

pub type greg_t = ::c_int;
pub type gregset_t = [greg_t; 19usize];

pub type __ioctl_dir = ::c_uint;

pub type __ioctl_datum = ::c_uint;

pub type __error_t_codes = ::c_int;

pub type int_least8_t = __int_least8_t;
pub type int_least16_t = __int_least16_t;
pub type int_least32_t = __int_least32_t;
pub type int_least64_t = __int_least64_t;
pub type uint_least8_t = __uint_least8_t;
pub type uint_least16_t = __uint_least16_t;
pub type uint_least32_t = __uint_least32_t;
pub type uint_least64_t = __uint_least64_t;
pub type int_fast8_t = ::c_schar;
pub type uint_fast8_t = ::c_uchar;
pub type intmax_t = __intmax_t;
pub type uintmax_t = __uintmax_t;

pub type tcp_seq = u32;

pub type tcp_ca_state = ::c_uint;

pub type idtype_t = ::c_uint;

// structs
s! {
    pub struct ip_mreq {
        pub imr_multiaddr: in_addr,
        pub imr_interface: in_addr,
    }

    pub struct ip_mreqn {
        pub imr_multiaddr: in_addr,
        pub imr_address: in_addr,
        pub imr_ifindex: ::c_int,
    }

    pub struct sockaddr {
        pub sa_len: ::c_uchar,
        pub sa_family: sa_family_t,
        pub sa_data: [::c_char; 14usize],
    }

    pub struct in_addr {
        pub s_addr: in_addr_t,
    }

    pub struct sockaddr_in {
        pub sin_len: ::c_uchar,
        pub sin_family: sa_family_t,
        pub sin_port: in_port_t,
        pub sin_addr: ::in_addr,
        pub sin_zero: [::c_uchar; 8usize],
    }

    pub struct sockaddr_in6 {
        pub sin6_len: ::c_uchar,
        pub sin6_family: sa_family_t,
        pub sin6_port: in_port_t,
        pub sin6_flowinfo: u32,
        pub sin6_addr: ::in6_addr,
        pub sin6_scope_id: u32,
    }

    pub struct sockaddr_un {
        pub sun_len: ::c_uchar,
        pub sun_family: sa_family_t,
        pub sun_path: [::c_char; 108usize],
    }

    pub struct sockaddr_storage {
        pub ss_len: ::c_uchar,
        pub ss_family: sa_family_t,
        pub __ss_padding: [::c_char; 122usize],
        pub __ss_align: __uint32_t,
    }

    pub struct sockaddr_at {
        pub _address: u8,
    }

    pub struct sockaddr_ax25 {
        pub _address: u8,
    }

    pub struct sockaddr_x25 {
        pub _address: u8,
    }

    pub struct sockaddr_dl {
        pub _address: u8,
    }
    pub struct sockaddr_eon {
        pub _address: u8,
    }
    pub struct sockaddr_inarp {
        pub _address: u8,
    }

    pub struct sockaddr_ipx {
        pub _address: u8,
    }
    pub struct sockaddr_iso {
        pub _address: u8,
    }

    pub struct sockaddr_ns {
        pub _address: u8,
    }

    pub struct addrinfo {
        pub ai_flags: ::c_int,
        pub ai_family: ::c_int,
        pub ai_socktype: ::c_int,
        pub ai_protocol: ::c_int,
        pub ai_addrlen: socklen_t,
        pub ai_addr: *mut sockaddr,
        pub ai_canonname: *mut ::c_char,
        pub ai_next: *mut addrinfo,
    }

    pub struct msghdr {
        pub msg_name: *mut ::c_void,
        pub msg_namelen: socklen_t,
        pub msg_iov: *mut ::iovec,
        pub msg_iovlen: ::c_int,
        pub msg_control: *mut ::c_void,
        pub msg_controllen: socklen_t,
        pub msg_flags: ::c_int,
    }

    pub struct dirent {
        pub d_ino: __ino_t,
        pub d_reclen: ::c_ushort,
        pub d_type: ::c_uchar,
        pub d_namlen: ::c_uchar,
        pub d_name: [::c_char; 1usize],
    }

    pub struct dirent64 {
        pub d_ino: __ino64_t,
        pub d_reclen: ::c_ushort,
        pub d_type: ::c_uchar,
        pub d_namlen: ::c_uchar,
        pub d_name: [::c_char; 1usize],
    }

    pub struct fd_set {
        pub fds_bits: [__fd_mask; 8usize],
    }

    pub struct termios {
        pub c_iflag: tcflag_t,
        pub c_oflag: tcflag_t,
        pub c_cflag: tcflag_t,
        pub c_lflag: tcflag_t,
        pub c_cc: [cc_t; 20usize],
        pub __ispeed: speed_t,
        pub __ospeed: speed_t,
    }

    pub struct sigaction {
        pub sa_sigaction: ::sighandler_t,
        pub sa_mask: __sigset_t,
        pub sa_flags: ::c_int,
    }

    pub struct sigevent {
        pub sigev_value: ::sigval,
        pub sigev_signo: ::c_int,
        pub sigev_notify: ::c_int,
        __unused1: *mut ::c_void,       //actually a function pointer
        pub sigev_notify_attributes: *mut pthread_attr_t,
    }

    pub struct siginfo_t {
        pub si_signo: ::c_int,
        pub si_errno: ::c_int,
        pub si_code: ::c_int,
        pub si_pid: __pid_t,
        pub si_uid: __uid_t,
        pub si_addr: *mut ::c_void,
        pub si_status: ::c_int,
        pub si_band: ::c_long,
        pub si_value: ::sigval,
    }

    pub struct timespec {
        pub tv_sec: __time_t,
        pub tv_nsec: __syscall_slong_t,
    }

    pub struct __locale_data {
        pub _address: u8,
    }

    pub struct stat {
        pub st_fstype: ::c_int,
        pub st_fsid: __fsid_t,
        pub st_ino: __ino_t,
        pub st_gen: ::c_uint,
        pub st_rdev: __dev_t,
        pub st_mode: __mode_t,
        pub st_nlink: __nlink_t,
        pub st_uid: __uid_t,
        pub st_gid: __gid_t,
        pub st_size: __off_t,
        pub st_atim: ::timespec,
        pub st_mtim: ::timespec,
        pub st_ctim: ::timespec,
        pub st_blksize: __blksize_t,
        pub st_blocks: __blkcnt_t,
        pub st_author: __uid_t,
        pub st_flags: ::c_uint,
        pub st_spare: [::c_int; 11usize],
    }

    pub struct stat64 {
        pub st_fstype: ::c_int,
        pub st_fsid: __fsid_t,
        pub st_ino: __ino64_t,
        pub st_gen: ::c_uint,
        pub st_rdev: __dev_t,
        pub st_mode: __mode_t,
        pub st_nlink: __nlink_t,
        pub st_uid: __uid_t,
        pub st_gid: __gid_t,
        pub st_size: __off64_t,
        pub st_atim: ::timespec,
        pub st_mtim: ::timespec,
        pub st_ctim: ::timespec,
        pub st_blksize: __blksize_t,
        pub st_blocks: __blkcnt64_t,
        pub st_author: __uid_t,
        pub st_flags: ::c_uint,
        pub st_spare: [::c_int; 8usize],
    }

    pub struct statfs {
        pub f_type: ::c_uint,
        pub f_bsize: ::c_ulong,
        pub f_blocks: __fsblkcnt_t,
        pub f_bfree: __fsblkcnt_t,
        pub f_bavail: __fsblkcnt_t,
        pub f_files: __fsblkcnt_t,
        pub f_ffree: __fsblkcnt_t,
        pub f_fsid: __fsid_t,
        pub f_namelen: ::c_ulong,
        pub f_favail: __fsfilcnt_t,
        pub f_frsize: ::c_ulong,
        pub f_flag: ::c_ulong,
        pub f_spare: [::c_uint; 3usize],
    }

    pub struct statfs64 {
        pub f_type: ::c_uint,
        pub f_bsize: ::c_ulong,
        pub f_blocks: __fsblkcnt64_t,
        pub f_bfree: __fsblkcnt64_t,
        pub f_bavail: __fsblkcnt64_t,
        pub f_files: __fsblkcnt64_t,
        pub f_ffree: __fsblkcnt64_t,
        pub f_fsid: __fsid_t,
        pub f_namelen: ::c_ulong,
        pub f_favail: __fsfilcnt64_t,
        pub f_frsize: ::c_ulong,
        pub f_flag: ::c_ulong,
        pub f_spare: [::c_uint ; 3usize],
    }

    pub struct statvfs {
        pub __f_type: ::c_uint,
        pub f_bsize: ::c_ulong,
        pub f_blocks: __fsblkcnt_t,
        pub f_bfree: __fsblkcnt_t,
        pub f_bavail: __fsblkcnt_t,
        pub f_files: __fsfilcnt_t,
        pub f_ffree: __fsfilcnt_t,
        pub f_fsid: __fsid_t,
        pub f_namemax: ::c_ulong,
        pub f_favail: __fsfilcnt_t,
        pub f_frsize: ::c_ulong,
        pub f_flag: ::c_ulong,
        pub f_spare: [::c_uint; 3usize],
    }

    pub struct statvfs64 {
        pub __f_type: ::c_uint,
        pub f_bsize: ::c_ulong,
        pub f_blocks: __fsblkcnt64_t,
        pub f_bfree: __fsblkcnt64_t,
        pub f_bavail: __fsblkcnt64_t,
        pub f_files: __fsfilcnt64_t,
        pub f_ffree: __fsfilcnt64_t,
        pub f_fsid: __fsid_t,
        pub f_namemax: ::c_ulong,
        pub f_favail: __fsfilcnt64_t,
        pub f_frsize: ::c_ulong,
        pub f_flag: ::c_ulong,
        pub f_spare: [::c_uint; 3usize],
    }

    #[cfg_attr(target_pointer_width = "32",
               repr(align(4)))]
    #[cfg_attr(target_pointer_width = "64",
               repr(align(8)))]
    pub struct sem_t {
        __size: [::c_char; 20usize],
    }

    pub struct __pthread {
        pub _address: u8,
    }

    pub struct __pthread_mutexattr {
        pub __prioceiling: ::c_int,
        pub __protocol: __pthread_mutex_protocol,
        pub __pshared: __pthread_process_shared,
        pub __mutex_type: __pthread_mutex_type,
    }
    pub struct __pthread_mutex {
        pub __lock: ::c_uint,
        pub __owner_id: ::c_uint,
        pub __cnt: ::c_uint,
        pub __shpid: ::c_int,
        pub __type: ::c_int,
        pub __flags: ::c_int,
        pub __reserved1: ::c_uint,
        pub __reserved2: ::c_uint,
    }

    pub struct __pthread_condattr {
        pub __pshared: __pthread_process_shared,
        pub __clock: __clockid_t,
    }

    pub struct __pthread_rwlockattr {
        pub __pshared: __pthread_process_shared,
    }

    pub struct __pthread_barrierattr {
        pub __pshared: __pthread_process_shared,
    }

    pub struct __pthread_once {
        pub __run: ::c_int,
        pub __lock: __pthread_spinlock_t,
    }

    pub struct __pthread_cond {
        pub __lock: __pthread_spinlock_t,
        pub __queue: *mut __pthread,
        pub __attr: *mut __pthread_condattr,
        pub __wrefs: ::c_uint,
        pub __data: *mut ::c_void,
    }

    pub struct __pthread_attr {
        pub __schedparam: __sched_param,
        pub __stackaddr: *mut ::c_void,
        pub __stacksize: size_t,
        pub __guardsize: size_t,
        pub __detachstate: __pthread_detachstate,
        pub __inheritsched: __pthread_inheritsched,
        pub __contentionscope: __pthread_contentionscope,
        pub __schedpolicy: ::c_int,
    }

    pub struct __pthread_rwlock {
        pub __held: __pthread_spinlock_t,
        pub __lock: __pthread_spinlock_t,
        pub __readers: ::c_int,
        pub __readerqueue: *mut __pthread,
        pub __writerqueue: *mut __pthread,
        pub __attr: *mut __pthread_rwlockattr,
        pub __data: *mut ::c_void,
    }

    pub struct __pthread_barrier {
        pub __lock: __pthread_spinlock_t,
        pub __queue: *mut __pthread,
        pub __pending: ::c_uint,
        pub __count: ::c_uint,
        pub __attr: *mut __pthread_barrierattr,
        pub __data: *mut ::c_void,
    }

    pub struct _IO_FILE {
        _unused: [u8; 0],
    }

    pub struct __sched_param {
        pub __sched_priority: ::c_int,
    }

    pub struct iovec {
        pub iov_base: *mut ::c_void,
        pub iov_len: size_t,
    }

    pub struct passwd {
        pub pw_name: *mut ::c_char,
        pub pw_passwd: *mut ::c_char,
        pub pw_uid: __uid_t,
        pub pw_gid: __gid_t,
        pub pw_gecos: *mut ::c_char,
        pub pw_dir: *mut ::c_char,
        pub pw_shell: *mut ::c_char,
    }

    pub struct tm {
        pub tm_sec: ::c_int,
        pub tm_min: ::c_int,
        pub tm_hour: ::c_int,
        pub tm_mday: ::c_int,
        pub tm_mon: ::c_int,
        pub tm_year: ::c_int,
        pub tm_wday: ::c_int,
        pub tm_yday: ::c_int,
        pub tm_isdst: ::c_int,
        pub tm_gmtoff: ::c_long,
        pub tm_zone: *const ::c_char,
    }

    pub struct lconv {
        pub decimal_point: *mut ::c_char,
        pub thousands_sep: *mut ::c_char,
        pub grouping: *mut ::c_char,
        pub int_curr_symbol: *mut ::c_char,
        pub currency_symbol: *mut ::c_char,
        pub mon_decimal_point: *mut ::c_char,
        pub mon_thousands_sep: *mut ::c_char,
        pub mon_grouping: *mut ::c_char,
        pub positive_sign: *mut ::c_char,
        pub negative_sign: *mut ::c_char,
        pub int_frac_digits: ::c_char,
        pub frac_digits: ::c_char,
        pub p_cs_precedes: ::c_char,
        pub p_sep_by_space: ::c_char,
        pub n_cs_precedes: ::c_char,
        pub n_sep_by_space: ::c_char,
        pub p_sign_posn: ::c_char,
        pub n_sign_posn: ::c_char,
        pub int_p_cs_precedes: ::c_char,
        pub int_p_sep_by_space: ::c_char,
        pub int_n_cs_precedes: ::c_char,
        pub int_n_sep_by_space: ::c_char,
        pub int_p_sign_posn: ::c_char,
        pub int_n_sign_posn: ::c_char,
    }

    pub struct Dl_info {
        pub dli_fname: *const ::c_char,
        pub dli_fbase: *mut ::c_void,
        pub dli_sname: *const ::c_char,
        pub dli_saddr: *mut ::c_void,
    }

    pub struct __locale_struct {
        pub __locales: [*mut __locale_data; 13usize],
        pub __ctype_b: *const ::c_ushort,
        pub __ctype_tolower: *const ::c_int,
        pub __ctype_toupper: *const ::c_int,
        pub __names: [*const ::c_char; 13usize],
    }

    pub struct utsname {
        pub sysname: [::c_char; 65],
        pub nodename: [::c_char; 65],
        pub release: [::c_char; 65],
        pub version: [::c_char; 65],
        pub machine: [::c_char; 65],
        pub domainname: [::c_char; 65]
    }

    pub struct rlimit64 {
        pub rlim_cur: rlim64_t,
        pub rlim_max: rlim64_t,
    }

    pub struct stack_t {
        pub ss_sp: * mut ::c_void,
        pub ss_size: ::size_t,
        pub ss_flags: ::c_int,
    }

    pub struct dl_phdr_info {
        pub dlpi_addr: Elf_Addr,
        pub dlpi_name: *const ::c_char,
        pub dlpi_phdr: *const Elf_Phdr,
        pub dlpi_phnum: Elf_Half,
        pub dlpi_adds: ::c_ulonglong,
        pub dlpi_subs: ::c_ulonglong,
        pub dlpi_tls_modid: ::size_t,
        pub dlpi_tls_data: *mut ::c_void,
    }

    pub struct flock {
        #[cfg(target_pointer_width = "32")]
        pub l_type : ::c_int,
        #[cfg(target_pointer_width = "32")]
        pub l_whence : ::c_int,
        #[cfg(target_pointer_width = "64")]
        pub l_type : ::c_short,
        #[cfg(target_pointer_width = "64")]
        pub l_whence : ::c_short,
        pub l_start : __off_t,
        pub l_len : __off_t,
        pub l_pid : __pid_t,
    }

    pub struct flock64 {
        #[cfg(target_pointer_width = "32")]
        pub l_type : ::c_int,
        #[cfg(target_pointer_width = "32")]
        pub l_whence : ::c_int,
        #[cfg(target_pointer_width = "64")]
        pub l_type : ::c_short,
        #[cfg(target_pointer_width = "64")]
        pub l_whence : ::c_short,
        pub l_start : __off_t,
        pub l_len : __off64_t,
        pub l_pid : __pid_t,
    }
}

impl siginfo_t {
    pub unsafe fn si_addr(&self) -> *mut ::c_void {
        self.si_addr
    }

    pub unsafe fn si_value(&self) -> ::sigval {
        self.si_value
    }

    pub unsafe fn si_pid(&self) -> ::pid_t {
        self.si_pid
    }

    pub unsafe fn si_uid(&self) -> ::uid_t {
        self.si_uid
    }

    pub unsafe fn si_status(&self) -> ::c_int {
        self.si_status
    }
}

// const
pub const IPOPT_COPY: u8 = 0x80;
pub const IPOPT_NUMBER_MASK: u8 = 0x1f;
pub const IPOPT_CLASS_MASK: u8 = 0x60;
pub const IPTOS_ECN_MASK: u8 = 0x03;
pub const MSG_CMSG_CLOEXEC: ::c_int = 0x40000000;

// unistd.h
pub const STDIN_FILENO: c_long = 0;
pub const STDOUT_FILENO: c_long = 1;
pub const STDERR_FILENO: c_long = 2;
pub const __FD_SETSIZE: usize = 256;
pub const R_OK: ::c_int = 4;
pub const W_OK: ::c_int = 2;
pub const X_OK: ::c_int = 1;
pub const F_OK: ::c_int = 0;
pub const SEEK_SET: ::c_int = 0;
pub const SEEK_CUR: ::c_int = 1;
pub const SEEK_END: ::c_int = 2;
pub const SEEK_DATA: ::c_int = 3;
pub const SEEK_HOLE: ::c_int = 4;
pub const L_SET: ::c_int = 0;
pub const L_INCR: ::c_int = 1;
pub const L_XTND: ::c_int = 2;
pub const F_ULOCK: ::c_int = 0;
pub const F_LOCK: ::c_int = 1;
pub const F_TLOCK: ::c_int = 2;
pub const F_TEST: ::c_int = 3;
pub const CLOSE_RANGE_CLOEXEC: ::c_int = 4;

// stdlib.h
pub const WNOHANG: ::c_int = 1;
pub const WUNTRACED: ::c_int = 2;
pub const WSTOPPED: ::c_int = 2;
pub const WCONTINUED: ::c_int = 4;
pub const WNOWAIT: ::c_int = 8;
pub const WEXITED: ::c_int = 16;
pub const __W_CONTINUED: ::c_int = 65535;
pub const __WCOREFLAG: ::c_int = 128;
pub const RAND_MAX: ::c_int = 2147483647;
pub const EXIT_FAILURE: ::c_int = 1;
pub const EXIT_SUCCESS: ::c_int = 0;
pub const __LITTLE_ENDIAN: usize = 1234;
pub const __BIG_ENDIAN: usize = 4321;
pub const __PDP_ENDIAN: usize = 3412;
pub const __BYTE_ORDER: usize = 1234;
pub const __FLOAT_WORD_ORDER: usize = 1234;
pub const LITTLE_ENDIAN: usize = 1234;
pub const BIG_ENDIAN: usize = 4321;
pub const PDP_ENDIAN: usize = 3412;
pub const BYTE_ORDER: usize = 1234;

// sys/select.h
pub const FD_SETSIZE: usize = 256;
pub const __SIZEOF_PTHREAD_MUTEX_T: usize = 32;
pub const __SIZEOF_PTHREAD_ATTR_T: usize = 32;
pub const __SIZEOF_PTHREAD_RWLOCK_T: usize = 28;
pub const __SIZEOF_PTHREAD_BARRIER_T: usize = 24;
pub const __SIZEOF_PTHREAD_MUTEXATTR_T: usize = 16;
pub const __SIZEOF_PTHREAD_COND_T: usize = 20;
pub const __SIZEOF_PTHREAD_CONDATTR_T: usize = 8;
pub const __SIZEOF_PTHREAD_RWLOCKATTR_T: usize = 4;
pub const __SIZEOF_PTHREAD_BARRIERATTR_T: usize = 4;
pub const __SIZEOF_PTHREAD_ONCE_T: usize = 8;
pub const __PTHREAD_SPIN_LOCK_INITIALIZER: ::c_int = 0;
pub const PTHREAD_MUTEX_NORMAL: ::c_int = 0;

// sys/resource.h
pub const RLIM_INFINITY: ::rlim_t = 2147483647;
pub const RLIM64_INFINITY: ::rlim64_t = 9223372036854775807;
pub const RLIM_SAVED_MAX: ::rlim_t = RLIM_INFINITY;
pub const RLIM_SAVED_CUR: ::rlim_t = RLIM_INFINITY;
pub const PRIO_MIN: ::c_int = -20;
pub const PRIO_MAX: ::c_int = 20;

// pwd.h
pub const NSS_BUFLEN_PASSWD: usize = 1024;

// sys/socket.h
pub const SOCK_TYPE_MASK: usize = 15;
pub const PF_UNSPEC: ::c_int = 0;
pub const PF_LOCAL: ::c_int = 1;
pub const PF_UNIX: ::c_int = 1;
pub const PF_FILE: ::c_int = 1;
pub const PF_INET: ::c_int = 2;
pub const PF_IMPLINK: ::c_int = 3;
pub const PF_PUP: ::c_int = 4;
pub const PF_CHAOS: ::c_int = 5;
pub const PF_NS: ::c_int = 6;
pub const PF_ISO: ::c_int = 7;
pub const PF_OSI: ::c_int = 7;
pub const PF_ECMA: ::c_int = 8;
pub const PF_DATAKIT: ::c_int = 9;
pub const PF_CCITT: ::c_int = 10;
pub const PF_SNA: ::c_int = 11;
pub const PF_DECnet: ::c_int = 12;
pub const PF_DLI: ::c_int = 13;
pub const PF_LAT: ::c_int = 14;
pub const PF_HYLINK: ::c_int = 15;
pub const PF_APPLETALK: ::c_int = 16;
pub const PF_ROUTE: ::c_int = 17;
pub const PF_XTP: ::c_int = 19;
pub const PF_COIP: ::c_int = 20;
pub const PF_CNT: ::c_int = 21;
pub const PF_RTIP: ::c_int = 22;
pub const PF_IPX: ::c_int = 23;
pub const PF_SIP: ::c_int = 24;
pub const PF_PIP: ::c_int = 25;
pub const PF_INET6: ::c_int = 26;
pub const PF_MAX: ::c_int = 27;
pub const AF_UNSPEC: ::c_int = 0;
pub const AF_LOCAL: ::c_int = 1;
pub const AF_UNIX: ::c_int = 1;
pub const AF_FILE: ::c_int = 1;
pub const AF_INET: ::c_int = 2;
pub const AF_IMPLINK: ::c_int = 3;
pub const AF_PUP: ::c_int = 4;
pub const AF_CHAOS: ::c_int = 5;
pub const AF_NS: ::c_int = 6;
pub const AF_ISO: ::c_int = 7;
pub const AF_OSI: ::c_int = 7;
pub const AF_ECMA: ::c_int = 8;
pub const AF_DATAKIT: ::c_int = 9;
pub const AF_CCITT: ::c_int = 10;
pub const AF_SNA: ::c_int = 11;
pub const AF_DECnet: ::c_int = 12;
pub const AF_DLI: ::c_int = 13;
pub const AF_LAT: ::c_int = 14;
pub const AF_HYLINK: ::c_int = 15;
pub const AF_APPLETALK: ::c_int = 16;
pub const AF_ROUTE: ::c_int = 17;
pub const pseudo_AF_XTP: ::c_int = 19;
pub const AF_COIP: ::c_int = 20;
pub const AF_CNT: ::c_int = 21;
pub const pseudo_AF_RTIP: ::c_int = 22;
pub const AF_IPX: ::c_int = 23;
pub const AF_SIP: ::c_int = 24;
pub const pseudo_AF_PIP: ::c_int = 25;
pub const AF_INET6: ::c_int = 26;
pub const AF_MAX: ::c_int = 27;
pub const SOMAXCONN: ::c_int = 4096;
pub const _SS_SIZE: usize = 128;
pub const CMGROUP_MAX: usize = 16;
pub const SOL_SOCKET: ::c_int = 65535;

// netinet/in.h
pub const SOL_IP: ::c_int = 0;
pub const IP_OPTIONS: ::c_int = 1;
pub const IP_HDRINCL: ::c_int = 2;
pub const IP_TOS: ::c_int = 3;
pub const IP_TTL: ::c_int = 4;
pub const IP_RECVOPTS: ::c_int = 5;
pub const IP_RECVRETOPTS: ::c_int = 6;
pub const IP_RECVDSTADDR: ::c_int = 7;
pub const IP_RETOPTS: ::c_int = 8;
pub const IP_MULTICAST_IF: ::c_int = 9;
pub const IP_MULTICAST_TTL: ::c_int = 10;
pub const IP_MULTICAST_LOOP: ::c_int = 11;
pub const IP_ADD_MEMBERSHIP: ::c_int = 12;
pub const IP_DROP_MEMBERSHIP: ::c_int = 13;
pub const SOL_IPV6: ::c_int = 41;
pub const SOL_ICMPV6: ::c_int = 58;
pub const IPV6_ADDRFORM: ::c_int = 1;
pub const IPV6_2292PKTINFO: ::c_int = 2;
pub const IPV6_2292HOPOPTS: ::c_int = 3;
pub const IPV6_2292DSTOPTS: ::c_int = 4;
pub const IPV6_2292RTHDR: ::c_int = 5;
pub const IPV6_2292PKTOPTIONS: ::c_int = 6;
pub const IPV6_CHECKSUM: ::c_int = 7;
pub const IPV6_2292HOPLIMIT: ::c_int = 8;
pub const IPV6_RXINFO: ::c_int = 2;
pub const IPV6_TXINFO: ::c_int = 2;
pub const SCM_SRCINFO: ::c_int = 2;
pub const IPV6_UNICAST_HOPS: ::c_int = 16;
pub const IPV6_MULTICAST_IF: ::c_int = 17;
pub const IPV6_MULTICAST_HOPS: ::c_int = 18;
pub const IPV6_MULTICAST_LOOP: ::c_int = 19;
pub const IPV6_JOIN_GROUP: ::c_int = 20;
pub const IPV6_LEAVE_GROUP: ::c_int = 21;
pub const IPV6_ROUTER_ALERT: ::c_int = 22;
pub const IPV6_MTU_DISCOVER: ::c_int = 23;
pub const IPV6_MTU: ::c_int = 24;
pub const IPV6_RECVERR: ::c_int = 25;
pub const IPV6_V6ONLY: ::c_int = 26;
pub const IPV6_JOIN_ANYCAST: ::c_int = 27;
pub const IPV6_LEAVE_ANYCAST: ::c_int = 28;
pub const IPV6_RECVPKTINFO: ::c_int = 49;
pub const IPV6_PKTINFO: ::c_int = 50;
pub const IPV6_RECVHOPLIMIT: ::c_int = 51;
pub const IPV6_HOPLIMIT: ::c_int = 52;
pub const IPV6_RECVHOPOPTS: ::c_int = 53;
pub const IPV6_HOPOPTS: ::c_int = 54;
pub const IPV6_RTHDRDSTOPTS: ::c_int = 55;
pub const IPV6_RECVRTHDR: ::c_int = 56;
pub const IPV6_RTHDR: ::c_int = 57;
pub const IPV6_RECVDSTOPTS: ::c_int = 58;
pub const IPV6_DSTOPTS: ::c_int = 59;
pub const IPV6_RECVPATHMTU: ::c_int = 60;
pub const IPV6_PATHMTU: ::c_int = 61;
pub const IPV6_DONTFRAG: ::c_int = 62;
pub const IPV6_RECVTCLASS: ::c_int = 66;
pub const IPV6_TCLASS: ::c_int = 67;
pub const IPV6_ADDR_PREFERENCES: ::c_int = 72;
pub const IPV6_MINHOPCOUNT: ::c_int = 73;
pub const IPV6_ADD_MEMBERSHIP: ::c_int = 20;
pub const IPV6_DROP_MEMBERSHIP: ::c_int = 21;
pub const IPV6_RXHOPOPTS: ::c_int = 3;
pub const IPV6_RXDSTOPTS: ::c_int = 4;
pub const IPV6_RTHDR_LOOSE: ::c_int = 0;
pub const IPV6_RTHDR_STRICT: ::c_int = 1;
pub const IPV6_RTHDR_TYPE_0: ::c_int = 0;
pub const IN_CLASSA_NET: u32 = 4278190080;
pub const IN_CLASSA_NSHIFT: usize = 24;
pub const IN_CLASSA_HOST: u32 = 16777215;
pub const IN_CLASSA_MAX: u32 = 128;
pub const IN_CLASSB_NET: u32 = 4294901760;
pub const IN_CLASSB_NSHIFT: usize = 16;
pub const IN_CLASSB_HOST: u32 = 65535;
pub const IN_CLASSB_MAX: u32 = 65536;
pub const IN_CLASSC_NET: u32 = 4294967040;
pub const IN_CLASSC_NSHIFT: usize = 8;
pub const IN_CLASSC_HOST: u32 = 255;
pub const IN_LOOPBACKNET: u32 = 127;
pub const INET_ADDRSTRLEN: usize = 16;
pub const INET6_ADDRSTRLEN: usize = 46;

// bits/posix1_lim.h
pub const _POSIX_AIO_LISTIO_MAX: usize = 2;
pub const _POSIX_AIO_MAX: usize = 1;
pub const _POSIX_ARG_MAX: usize = 4096;
pub const _POSIX_CHILD_MAX: usize = 25;
pub const _POSIX_DELAYTIMER_MAX: usize = 32;
pub const _POSIX_HOST_NAME_MAX: usize = 255;
pub const _POSIX_LINK_MAX: usize = 8;
pub const _POSIX_LOGIN_NAME_MAX: usize = 9;
pub const _POSIX_MAX_CANON: usize = 255;
pub const _POSIX_MAX_INPUT: usize = 255;
pub const _POSIX_MQ_OPEN_MAX: usize = 8;
pub const _POSIX_MQ_PRIO_MAX: usize = 32;
pub const _POSIX_NAME_MAX: usize = 14;
pub const _POSIX_NGROUPS_MAX: usize = 8;
pub const _POSIX_OPEN_MAX: usize = 20;
pub const _POSIX_FD_SETSIZE: usize = 20;
pub const _POSIX_PATH_MAX: usize = 256;
pub const _POSIX_PIPE_BUF: usize = 512;
pub const _POSIX_RE_DUP_MAX: usize = 255;
pub const _POSIX_RTSIG_MAX: usize = 8;
pub const _POSIX_SEM_NSEMS_MAX: usize = 256;
pub const _POSIX_SEM_VALUE_MAX: usize = 32767;
pub const _POSIX_SIGQUEUE_MAX: usize = 32;
pub const _POSIX_SSIZE_MAX: usize = 32767;
pub const _POSIX_STREAM_MAX: usize = 8;
pub const _POSIX_SYMLINK_MAX: usize = 255;
pub const _POSIX_SYMLOOP_MAX: usize = 8;
pub const _POSIX_TIMER_MAX: usize = 32;
pub const _POSIX_TTY_NAME_MAX: usize = 9;
pub const _POSIX_TZNAME_MAX: usize = 6;
pub const _POSIX_QLIMIT: usize = 1;
pub const _POSIX_HIWAT: usize = 512;
pub const _POSIX_UIO_MAXIOV: usize = 16;
pub const _POSIX_CLOCKRES_MIN: usize = 20000000;
pub const NAME_MAX: usize = 255;
pub const NGROUPS_MAX: usize = 256;
pub const _POSIX_THREAD_KEYS_MAX: usize = 128;
pub const _POSIX_THREAD_DESTRUCTOR_ITERATIONS: usize = 4;
pub const _POSIX_THREAD_THREADS_MAX: usize = 64;
pub const SEM_VALUE_MAX: ::c_int = 2147483647;
pub const MAXNAMLEN: usize = 255;

// netdb.h
pub const _PATH_HEQUIV: &'static [u8; 17usize] = b"/etc/hosts.equiv\0";
pub const _PATH_HOSTS: &'static [u8; 11usize] = b"/etc/hosts\0";
pub const _PATH_NETWORKS: &'static [u8; 14usize] = b"/etc/networks\0";
pub const _PATH_NSSWITCH_CONF: &'static [u8; 19usize] = b"/etc/nsswitch.conf\0";
pub const _PATH_PROTOCOLS: &'static [u8; 15usize] = b"/etc/protocols\0";
pub const _PATH_SERVICES: &'static [u8; 14usize] = b"/etc/services\0";
pub const HOST_NOT_FOUND: ::c_int = 1;
pub const TRY_AGAIN: ::c_int = 2;
pub const NO_RECOVERY: ::c_int = 3;
pub const NO_DATA: ::c_int = 4;
pub const NETDB_INTERNAL: ::c_int = -1;
pub const NETDB_SUCCESS: ::c_int = 0;
pub const NO_ADDRESS: ::c_int = 4;
pub const IPPORT_RESERVED: ::c_int = 1024;
pub const SCOPE_DELIMITER: u8 = 37u8;
pub const GAI_WAIT: ::c_int = 0;
pub const GAI_NOWAIT: ::c_int = 1;
pub const AI_PASSIVE: ::c_int = 1;
pub const AI_CANONNAME: ::c_int = 2;
pub const AI_NUMERICHOST: ::c_int = 4;
pub const AI_V4MAPPED: ::c_int = 8;
pub const AI_ALL: ::c_int = 16;
pub const AI_ADDRCONFIG: ::c_int = 32;
pub const AI_IDN: ::c_int = 64;
pub const AI_CANONIDN: ::c_int = 128;
pub const AI_NUMERICSERV: ::c_int = 1024;
pub const EAI_BADFLAGS: ::c_int = -1;
pub const EAI_NONAME: ::c_int = -2;
pub const EAI_AGAIN: ::c_int = -3;
pub const EAI_FAIL: ::c_int = -4;
pub const EAI_FAMILY: ::c_int = -6;
pub const EAI_SOCKTYPE: ::c_int = -7;
pub const EAI_SERVICE: ::c_int = -8;
pub const EAI_MEMORY: ::c_int = -10;
pub const EAI_SYSTEM: ::c_int = -11;
pub const EAI_OVERFLOW: ::c_int = -12;
pub const EAI_NODATA: ::c_int = -5;
pub const EAI_ADDRFAMILY: ::c_int = -9;
pub const EAI_INPROGRESS: ::c_int = -100;
pub const EAI_CANCELED: ::c_int = -101;
pub const EAI_NOTCANCELED: ::c_int = -102;
pub const EAI_ALLDONE: ::c_int = -103;
pub const EAI_INTR: ::c_int = -104;
pub const EAI_IDN_ENCODE: ::c_int = -105;
pub const NI_MAXHOST: usize = 1025;
pub const NI_MAXSERV: usize = 32;
pub const NI_NUMERICHOST: ::c_int = 1;
pub const NI_NUMERICSERV: ::c_int = 2;
pub const NI_NOFQDN: ::c_int = 4;
pub const NI_NAMEREQD: ::c_int = 8;
pub const NI_DGRAM: ::c_int = 16;
pub const NI_IDN: ::c_int = 32;

// time.h
pub const CLOCK_REALTIME: clockid_t = 0;
pub const CLOCK_MONOTONIC: clockid_t = 1;
pub const CLOCK_PROCESS_CPUTIME_ID: clockid_t = 2;
pub const CLOCK_THREAD_CPUTIME_ID: clockid_t = 3;
pub const CLOCK_MONOTONIC_RAW: clockid_t = 4;
pub const CLOCK_REALTIME_COARSE: clockid_t = 5;
pub const CLOCK_MONOTONIC_COARSE: clockid_t = 6;
pub const TIMER_ABSTIME: ::c_int = 1;
pub const TIME_UTC: ::c_int = 1;

// sys/poll.h
pub const POLLIN: i16 = 1;
pub const POLLPRI: i16 = 2;
pub const POLLOUT: i16 = 4;
pub const POLLRDNORM: i16 = 1;
pub const POLLRDBAND: i16 = 2;
pub const POLLWRNORM: i16 = 4;
pub const POLLWRBAND: i16 = 4;
pub const POLLERR: i16 = 8;
pub const POLLHUP: i16 = 16;
pub const POLLNVAL: i16 = 32;

// locale.h
pub const __LC_CTYPE: usize = 0;
pub const __LC_NUMERIC: usize = 1;
pub const __LC_TIME: usize = 2;
pub const __LC_COLLATE: usize = 3;
pub const __LC_MONETARY: usize = 4;
pub const __LC_MESSAGES: usize = 5;
pub const __LC_ALL: usize = 6;
pub const __LC_PAPER: usize = 7;
pub const __LC_NAME: usize = 8;
pub const __LC_ADDRESS: usize = 9;
pub const __LC_TELEPHONE: usize = 10;
pub const __LC_MEASUREMENT: usize = 11;
pub const __LC_IDENTIFICATION: usize = 12;
pub const LC_CTYPE: ::c_int = 0;
pub const LC_NUMERIC: ::c_int = 1;
pub const LC_TIME: ::c_int = 2;
pub const LC_COLLATE: ::c_int = 3;
pub const LC_MONETARY: ::c_int = 4;
pub const LC_MESSAGES: ::c_int = 5;
pub const LC_ALL: ::c_int = 6;
pub const LC_PAPER: ::c_int = 7;
pub const LC_NAME: ::c_int = 8;
pub const LC_ADDRESS: ::c_int = 9;
pub const LC_TELEPHONE: ::c_int = 10;
pub const LC_MEASUREMENT: ::c_int = 11;
pub const LC_IDENTIFICATION: ::c_int = 12;
pub const LC_CTYPE_MASK: ::c_int = 1;
pub const LC_NUMERIC_MASK: ::c_int = 2;
pub const LC_TIME_MASK: ::c_int = 4;
pub const LC_COLLATE_MASK: ::c_int = 8;
pub const LC_MONETARY_MASK: ::c_int = 16;
pub const LC_MESSAGES_MASK: ::c_int = 32;
pub const LC_PAPER_MASK: ::c_int = 128;
pub const LC_NAME_MASK: ::c_int = 256;
pub const LC_ADDRESS_MASK: ::c_int = 512;
pub const LC_TELEPHONE_MASK: ::c_int = 1024;
pub const LC_MEASUREMENT_MASK: ::c_int = 2048;
pub const LC_IDENTIFICATION_MASK: ::c_int = 4096;
pub const LC_ALL_MASK: ::c_int = 8127;

// semaphore.h
pub const __SIZEOF_SEM_T: usize = 20;

// termios.h
pub const IGNBRK: tcflag_t = 1;
pub const BRKINT: tcflag_t = 2;
pub const IGNPAR: tcflag_t = 4;
pub const PARMRK: tcflag_t = 8;
pub const INPCK: tcflag_t = 16;
pub const ISTRIP: tcflag_t = 32;
pub const INLCR: tcflag_t = 64;
pub const IGNCR: tcflag_t = 128;
pub const ICRNL: tcflag_t = 256;
pub const IXON: tcflag_t = 512;
pub const IXOFF: tcflag_t = 1024;
pub const IXANY: tcflag_t = 2048;
pub const IMAXBEL: tcflag_t = 8192;
pub const IUCLC: tcflag_t = 16384;
pub const OPOST: tcflag_t = 1;
pub const ONLCR: tcflag_t = 2;
pub const ONOEOT: tcflag_t = 8;
pub const OCRNL: tcflag_t = 16;
pub const ONOCR: tcflag_t = 32;
pub const ONLRET: tcflag_t = 64;
pub const NLDLY: tcflag_t = 768;
pub const NL0: tcflag_t = 0;
pub const NL1: tcflag_t = 256;
pub const TABDLY: tcflag_t = 3076;
pub const TAB0: tcflag_t = 0;
pub const TAB1: tcflag_t = 1024;
pub const TAB2: tcflag_t = 2048;
pub const TAB3: tcflag_t = 4;
pub const CRDLY: tcflag_t = 12288;
pub const CR0: tcflag_t = 0;
pub const CR1: tcflag_t = 4096;
pub const CR2: tcflag_t = 8192;
pub const CR3: tcflag_t = 12288;
pub const FFDLY: tcflag_t = 16384;
pub const FF0: tcflag_t = 0;
pub const FF1: tcflag_t = 16384;
pub const BSDLY: tcflag_t = 32768;
pub const BS0: tcflag_t = 0;
pub const BS1: tcflag_t = 32768;
pub const VTDLY: tcflag_t = 65536;
pub const VT0: tcflag_t = 0;
pub const VT1: tcflag_t = 65536;
pub const OLCUC: tcflag_t = 131072;
pub const OFILL: tcflag_t = 262144;
pub const OFDEL: tcflag_t = 524288;
pub const CIGNORE: tcflag_t = 1;
pub const CSIZE: tcflag_t = 768;
pub const CS5: tcflag_t = 0;
pub const CS6: tcflag_t = 256;
pub const CS7: tcflag_t = 512;
pub const CS8: tcflag_t = 768;
pub const CSTOPB: tcflag_t = 1024;
pub const CREAD: tcflag_t = 2048;
pub const PARENB: tcflag_t = 4096;
pub const PARODD: tcflag_t = 8192;
pub const HUPCL: tcflag_t = 16384;
pub const CLOCAL: tcflag_t = 32768;
pub const CRTSCTS: tcflag_t = 65536;
pub const CRTS_IFLOW: tcflag_t = 65536;
pub const CCTS_OFLOW: tcflag_t = 65536;
pub const CDTRCTS: tcflag_t = 131072;
pub const MDMBUF: tcflag_t = 1048576;
pub const CHWFLOW: tcflag_t = 1245184;
pub const ECHOKE: tcflag_t = 1;
pub const _ECHOE: tcflag_t = 2;
pub const ECHOE: tcflag_t = 2;
pub const _ECHOK: tcflag_t = 4;
pub const ECHOK: tcflag_t = 4;
pub const _ECHO: tcflag_t = 8;
pub const ECHO: tcflag_t = 8;
pub const _ECHONL: tcflag_t = 16;
pub const ECHONL: tcflag_t = 16;
pub const ECHOPRT: tcflag_t = 32;
pub const ECHOCTL: tcflag_t = 64;
pub const _ISIG: tcflag_t = 128;
pub const ISIG: tcflag_t = 128;
pub const _ICANON: tcflag_t = 256;
pub const ICANON: tcflag_t = 256;
pub const ALTWERASE: tcflag_t = 512;
pub const _IEXTEN: tcflag_t = 1024;
pub const IEXTEN: tcflag_t = 1024;
pub const EXTPROC: tcflag_t = 2048;
pub const _TOSTOP: tcflag_t = 4194304;
pub const TOSTOP: tcflag_t = 4194304;
pub const FLUSHO: tcflag_t = 8388608;
pub const NOKERNINFO: tcflag_t = 33554432;
pub const PENDIN: tcflag_t = 536870912;
pub const _NOFLSH: tcflag_t = 2147483648;
pub const NOFLSH: tcflag_t = 2147483648;
pub const VEOF: cc_t = 0;
pub const VEOL: cc_t = 1;
pub const VEOL2: cc_t = 2;
pub const VERASE: cc_t = 3;
pub const VWERASE: cc_t = 4;
pub const VKILL: cc_t = 5;
pub const VREPRINT: cc_t = 6;
pub const VINTR: cc_t = 8;
pub const VQUIT: cc_t = 9;
pub const VSUSP: cc_t = 10;
pub const VDSUSP: cc_t = 11;
pub const VSTART: cc_t = 12;
pub const VSTOP: cc_t = 13;
pub const VLNEXT: cc_t = 14;
pub const VDISCARD: cc_t = 15;
pub const VMIN: cc_t = 16;
pub const VTIME: cc_t = 17;
pub const VSTATUS: cc_t = 18;
pub const NCCS: usize = 20;
pub const B0: speed_t = 0;
pub const B50: speed_t = 50;
pub const B75: speed_t = 75;
pub const B110: speed_t = 110;
pub const B134: speed_t = 134;
pub const B150: speed_t = 150;
pub const B200: speed_t = 200;
pub const B300: speed_t = 300;
pub const B600: speed_t = 600;
pub const B1200: speed_t = 1200;
pub const B1800: speed_t = 1800;
pub const B2400: speed_t = 2400;
pub const B4800: speed_t = 4800;
pub const B9600: speed_t = 9600;
pub const B7200: speed_t = 7200;
pub const B14400: speed_t = 14400;
pub const B19200: speed_t = 19200;
pub const B28800: speed_t = 28800;
pub const B38400: speed_t = 38400;
pub const EXTA: speed_t = 19200;
pub const EXTB: speed_t = 38400;
pub const B57600: speed_t = 57600;
pub const B76800: speed_t = 76800;
pub const B115200: speed_t = 115200;
pub const B230400: speed_t = 230400;
pub const B460800: speed_t = 460800;
pub const B500000: speed_t = 500000;
pub const B576000: speed_t = 576000;
pub const B921600: speed_t = 921600;
pub const B1000000: speed_t = 1000000;
pub const B1152000: speed_t = 1152000;
pub const B1500000: speed_t = 1500000;
pub const B2000000: speed_t = 2000000;
pub const B2500000: speed_t = 2500000;
pub const B3000000: speed_t = 3000000;
pub const B3500000: speed_t = 3500000;
pub const B4000000: speed_t = 4000000;
pub const TCSANOW: ::c_int = 0;
pub const TCSADRAIN: ::c_int = 1;
pub const TCSAFLUSH: ::c_int = 2;
pub const TCSASOFT: ::c_int = 16;
pub const TCIFLUSH: ::c_int = 1;
pub const TCOFLUSH: ::c_int = 2;
pub const TCIOFLUSH: ::c_int = 3;
pub const TCOOFF: ::c_int = 1;
pub const TCOON: ::c_int = 2;
pub const TCIOFF: ::c_int = 3;
pub const TCION: ::c_int = 4;
pub const TTYDEF_IFLAG: tcflag_t = 11042;
pub const TTYDEF_LFLAG: tcflag_t = 1483;
pub const TTYDEF_CFLAG: tcflag_t = 23040;
pub const TTYDEF_SPEED: tcflag_t = 9600;
pub const CEOL: u8 = 0u8;
pub const CERASE: u8 = 127;
pub const CMIN: u8 = 1;
pub const CQUIT: u8 = 28;
pub const CTIME: u8 = 0;
pub const CBRK: u8 = 0u8;

// dlfcn.h
pub const RTLD_DEFAULT: *mut ::c_void = 0i64 as *mut ::c_void;
pub const RTLD_LAZY: ::c_int = 1;
pub const RTLD_NOW: ::c_int = 2;
pub const RTLD_BINDING_MASK: ::c_int = 3;
pub const RTLD_NOLOAD: ::c_int = 4;
pub const RTLD_DEEPBIND: ::c_int = 8;
pub const RTLD_GLOBAL: ::c_int = 256;
pub const RTLD_LOCAL: ::c_int = 0;
pub const RTLD_NODELETE: ::c_int = 4096;
pub const DLFO_STRUCT_HAS_EH_DBASE: usize = 1;
pub const DLFO_STRUCT_HAS_EH_COUNT: usize = 0;
pub const LM_ID_BASE: c_long = 0;
pub const LM_ID_NEWLM: c_long = -1;

// bits/signum_generic.h
pub const SIGINT: ::c_int = 2;
pub const SIGILL: ::c_int = 4;
pub const SIGABRT: ::c_int = 6;
pub const SIGFPE: ::c_int = 8;
pub const SIGSEGV: ::c_int = 11;
pub const SIGTERM: ::c_int = 15;
pub const SIGHUP: ::c_int = 1;
pub const SIGQUIT: ::c_int = 3;
pub const SIGTRAP: ::c_int = 5;
pub const SIGKILL: ::c_int = 9;
pub const SIGPIPE: ::c_int = 13;
pub const SIGALRM: ::c_int = 14;
pub const SIGIOT: ::c_int = 6;
pub const SIGBUS: ::c_int = 10;
pub const SIGSYS: ::c_int = 12;
pub const SIGEMT: ::c_int = 7;
pub const SIGINFO: ::c_int = 29;
pub const SIGLOST: ::c_int = 32;
pub const SIGURG: ::c_int = 16;
pub const SIGSTOP: ::c_int = 17;
pub const SIGTSTP: ::c_int = 18;
pub const SIGCONT: ::c_int = 19;
pub const SIGCHLD: ::c_int = 20;
pub const SIGTTIN: ::c_int = 21;
pub const SIGTTOU: ::c_int = 22;
pub const SIGPOLL: ::c_int = 23;
pub const SIGXCPU: ::c_int = 24;
pub const SIGVTALRM: ::c_int = 26;
pub const SIGPROF: ::c_int = 27;
pub const SIGXFSZ: ::c_int = 25;
pub const SIGUSR1: ::c_int = 30;
pub const SIGUSR2: ::c_int = 31;
pub const SIGWINCH: ::c_int = 28;
pub const SIGIO: ::c_int = 23;
pub const SIGCLD: ::c_int = 20;
pub const __SIGRTMIN: usize = 32;
pub const __SIGRTMAX: usize = 32;
pub const _NSIG: usize = 33;
pub const NSIG: usize = 33;

// bits/sigaction.h
pub const SA_ONSTACK: ::c_int = 1;
pub const SA_RESTART: ::c_int = 2;
pub const SA_NODEFER: ::c_int = 16;
pub const SA_RESETHAND: ::c_int = 4;
pub const SA_NOCLDSTOP: ::c_int = 8;
pub const SA_SIGINFO: ::c_int = 64;
pub const SA_INTERRUPT: ::c_int = 0;
pub const SA_NOMASK: ::c_int = 16;
pub const SA_ONESHOT: ::c_int = 4;
pub const SA_STACK: ::c_int = 1;
pub const SIG_BLOCK: ::c_int = 1;
pub const SIG_UNBLOCK: ::c_int = 2;
pub const SIG_SETMASK: ::c_int = 3;

// bits/sigcontext.h
pub const FPC_IE: u16 = 1;
pub const FPC_IM: u16 = 1;
pub const FPC_DE: u16 = 2;
pub const FPC_DM: u16 = 2;
pub const FPC_ZE: u16 = 4;
pub const FPC_ZM: u16 = 4;
pub const FPC_OE: u16 = 8;
pub const FPC_OM: u16 = 8;
pub const FPC_UE: u16 = 16;
pub const FPC_PE: u16 = 32;
pub const FPC_PC: u16 = 768;
pub const FPC_PC_24: u16 = 0;
pub const FPC_PC_53: u16 = 512;
pub const FPC_PC_64: u16 = 768;
pub const FPC_RC: u16 = 3072;
pub const FPC_RC_RN: u16 = 0;
pub const FPC_RC_RD: u16 = 1024;
pub const FPC_RC_RU: u16 = 2048;
pub const FPC_RC_CHOP: u16 = 3072;
pub const FPC_IC: u16 = 4096;
pub const FPC_IC_PROJ: u16 = 0;
pub const FPC_IC_AFF: u16 = 4096;
pub const FPS_IE: u16 = 1;
pub const FPS_DE: u16 = 2;
pub const FPS_ZE: u16 = 4;
pub const FPS_OE: u16 = 8;
pub const FPS_UE: u16 = 16;
pub const FPS_PE: u16 = 32;
pub const FPS_SF: u16 = 64;
pub const FPS_ES: u16 = 128;
pub const FPS_C0: u16 = 256;
pub const FPS_C1: u16 = 512;
pub const FPS_C2: u16 = 1024;
pub const FPS_TOS: u16 = 14336;
pub const FPS_TOS_SHIFT: u16 = 11;
pub const FPS_C3: u16 = 16384;
pub const FPS_BUSY: u16 = 32768;
pub const FPE_INTOVF_TRAP: ::c_int = 1;
pub const FPE_INTDIV_FAULT: ::c_int = 2;
pub const FPE_FLTOVF_FAULT: ::c_int = 3;
pub const FPE_FLTDIV_FAULT: ::c_int = 4;
pub const FPE_FLTUND_FAULT: ::c_int = 5;
pub const FPE_SUBRNG_FAULT: ::c_int = 7;
pub const FPE_FLTDNR_FAULT: ::c_int = 8;
pub const FPE_FLTINX_FAULT: ::c_int = 9;
pub const FPE_EMERR_FAULT: ::c_int = 10;
pub const FPE_EMBND_FAULT: ::c_int = 11;
pub const ILL_INVOPR_FAULT: ::c_int = 1;
pub const ILL_STACK_FAULT: ::c_int = 2;
pub const ILL_FPEOPR_FAULT: ::c_int = 3;
pub const DBG_SINGLE_TRAP: ::c_int = 1;
pub const DBG_BRKPNT_FAULT: ::c_int = 2;
pub const __NGREG: usize = 19;
pub const NGREG: usize = 19;

// bits/sigstack.h
pub const MINSIGSTKSZ: usize = 8192;
pub const SIGSTKSZ: usize = 40960;

// sys/stat.h
pub const __S_IFMT: mode_t = 61440;
pub const __S_IFDIR: mode_t = 16384;
pub const __S_IFCHR: mode_t = 8192;
pub const __S_IFBLK: mode_t = 24576;
pub const __S_IFREG: mode_t = 32768;
pub const __S_IFLNK: mode_t = 40960;
pub const __S_IFSOCK: mode_t = 49152;
pub const __S_IFIFO: mode_t = 4096;
pub const __S_ISUID: mode_t = 2048;
pub const __S_ISGID: mode_t = 1024;
pub const __S_ISVTX: mode_t = 512;
pub const __S_IREAD: mode_t = 256;
pub const __S_IWRITE: mode_t = 128;
pub const __S_IEXEC: mode_t = 64;
pub const S_INOCACHE: mode_t = 65536;
pub const S_IUSEUNK: mode_t = 131072;
pub const S_IUNKNOWN: mode_t = 1835008;
pub const S_IUNKSHIFT: mode_t = 12;
pub const S_IPTRANS: mode_t = 2097152;
pub const S_IATRANS: mode_t = 4194304;
pub const S_IROOT: mode_t = 8388608;
pub const S_ITRANS: mode_t = 14680064;
pub const S_IMMAP0: mode_t = 16777216;
pub const CMASK: mode_t = 18;
pub const UF_SETTABLE: ::c_uint = 65535;
pub const UF_NODUMP: ::c_uint = 1;
pub const UF_IMMUTABLE: ::c_uint = 2;
pub const UF_APPEND: ::c_uint = 4;
pub const UF_OPAQUE: ::c_uint = 8;
pub const UF_NOUNLINK: ::c_uint = 16;
pub const SF_SETTABLE: ::c_uint = 4294901760;
pub const SF_ARCHIVED: ::c_uint = 65536;
pub const SF_IMMUTABLE: ::c_uint = 131072;
pub const SF_APPEND: ::c_uint = 262144;
pub const SF_NOUNLINK: ::c_uint = 1048576;
pub const SF_SNAPSHOT: ::c_uint = 2097152;
pub const UTIME_NOW: ::c_long = -1;
pub const UTIME_OMIT: ::c_long = -2;
pub const S_IFMT: mode_t = 61440;
pub const S_IFDIR: mode_t = 16384;
pub const S_IFCHR: mode_t = 8192;
pub const S_IFBLK: mode_t = 24576;
pub const S_IFREG: mode_t = 32768;
pub const S_IFIFO: mode_t = 4096;
pub const S_IFLNK: mode_t = 40960;
pub const S_IFSOCK: mode_t = 49152;
pub const S_ISUID: mode_t = 2048;
pub const S_ISGID: mode_t = 1024;
pub const S_ISVTX: mode_t = 512;
pub const S_IRUSR: mode_t = 256;
pub const S_IWUSR: mode_t = 128;
pub const S_IXUSR: mode_t = 64;
pub const S_IRWXU: mode_t = 448;
pub const S_IREAD: mode_t = 256;
pub const S_IWRITE: mode_t = 128;
pub const S_IEXEC: mode_t = 64;
pub const S_IRGRP: mode_t = 32;
pub const S_IWGRP: mode_t = 16;
pub const S_IXGRP: mode_t = 8;
pub const S_IRWXG: mode_t = 56;
pub const S_IROTH: mode_t = 4;
pub const S_IWOTH: mode_t = 2;
pub const S_IXOTH: mode_t = 1;
pub const S_IRWXO: mode_t = 7;
pub const ACCESSPERMS: mode_t = 511;
pub const ALLPERMS: mode_t = 4095;
pub const DEFFILEMODE: mode_t = 438;
pub const S_BLKSIZE: usize = 512;
pub const STATX_TYPE: ::c_uint = 1;
pub const STATX_MODE: ::c_uint = 2;
pub const STATX_NLINK: ::c_uint = 4;
pub const STATX_UID: ::c_uint = 8;
pub const STATX_GID: ::c_uint = 16;
pub const STATX_ATIME: ::c_uint = 32;
pub const STATX_MTIME: ::c_uint = 64;
pub const STATX_CTIME: ::c_uint = 128;
pub const STATX_INO: ::c_uint = 256;
pub const STATX_SIZE: ::c_uint = 512;
pub const STATX_BLOCKS: ::c_uint = 1024;
pub const STATX_BASIC_STATS: ::c_uint = 2047;
pub const STATX_ALL: ::c_uint = 4095;
pub const STATX_BTIME: ::c_uint = 2048;
pub const STATX_MNT_ID: ::c_uint = 4096;
pub const STATX_DIOALIGN: ::c_uint = 8192;
pub const STATX__RESERVED: ::c_uint = 2147483648;
pub const STATX_ATTR_COMPRESSED: ::c_uint = 4;
pub const STATX_ATTR_IMMUTABLE: ::c_uint = 16;
pub const STATX_ATTR_APPEND: ::c_uint = 32;
pub const STATX_ATTR_NODUMP: ::c_uint = 64;
pub const STATX_ATTR_ENCRYPTED: ::c_uint = 2048;
pub const STATX_ATTR_AUTOMOUNT: ::c_uint = 4096;
pub const STATX_ATTR_MOUNT_ROOT: ::c_uint = 8192;
pub const STATX_ATTR_VERITY: ::c_uint = 1048576;
pub const STATX_ATTR_DAX: ::c_uint = 2097152;

// sys/ioctl.h
pub const TIOCM_LE: ::c_int = 1;
pub const TIOCM_DTR: ::c_int = 2;
pub const TIOCM_RTS: ::c_int = 4;
pub const TIOCM_ST: ::c_int = 8;
pub const TIOCM_SR: ::c_int = 16;
pub const TIOCM_CTS: ::c_int = 32;
pub const TIOCM_CAR: ::c_int = 64;
pub const TIOCM_CD: ::c_int = 64;
pub const TIOCM_RNG: ::c_int = 128;
pub const TIOCM_RI: ::c_int = 128;
pub const TIOCM_DSR: ::c_int = 256;
pub const TIOCPKT_DATA: ::c_int = 0;
pub const TIOCPKT_FLUSHREAD: ::c_int = 1;
pub const TIOCPKT_FLUSHWRITE: ::c_int = 2;
pub const TIOCPKT_STOP: ::c_int = 4;
pub const TIOCPKT_START: ::c_int = 8;
pub const TIOCPKT_NOSTOP: ::c_int = 16;
pub const TIOCPKT_DOSTOP: ::c_int = 32;
pub const TIOCPKT_IOCTL: ::c_int = 64;
pub const TTYDISC: ::c_int = 0;
pub const TABLDISC: ::c_int = 3;
pub const SLIPDISC: ::c_int = 4;
pub const TANDEM: tcflag_t = 1;
pub const CBREAK: tcflag_t = 2;
pub const LCASE: tcflag_t = 4;
pub const CRMOD: tcflag_t = 16;
pub const RAW: tcflag_t = 32;
pub const ODDP: tcflag_t = 64;
pub const EVENP: tcflag_t = 128;
pub const ANYP: tcflag_t = 192;
pub const NLDELAY: tcflag_t = 768;
pub const NL2: tcflag_t = 512;
pub const NL3: tcflag_t = 768;
pub const TBDELAY: tcflag_t = 3072;
pub const XTABS: tcflag_t = 3072;
pub const CRDELAY: tcflag_t = 12288;
pub const VTDELAY: tcflag_t = 16384;
pub const BSDELAY: tcflag_t = 32768;
pub const ALLDELAY: tcflag_t = 65280;
pub const CRTBS: tcflag_t = 65536;
pub const PRTERA: tcflag_t = 131072;
pub const CRTERA: tcflag_t = 262144;
pub const TILDE: tcflag_t = 524288;
pub const LITOUT: tcflag_t = 2097152;
pub const NOHANG: tcflag_t = 16777216;
pub const L001000: tcflag_t = 33554432;
pub const CRTKIL: tcflag_t = 67108864;
pub const PASS8: tcflag_t = 134217728;
pub const CTLECH: tcflag_t = 268435456;
pub const DECCTQ: tcflag_t = 1073741824;

pub const FIONBIO: ::c_ulong = 0xa008007e;
pub const FIONREAD: ::c_ulong = 0x6008007f;
pub const TIOCSWINSZ: ::c_ulong = 0x90200767;
pub const TIOCGWINSZ: ::c_ulong = 0x50200768;
pub const TIOCEXCL: ::c_ulong = 0x70d;
pub const TIOCNXCL: ::c_ulong = 0x70e;
pub const TIOCSCTTY: ::c_ulong = 0x761;

pub const FIOCLEX: ::c_ulong = 1;

// fcntl.h
pub const O_EXEC: ::c_int = 4;
pub const O_NORW: ::c_int = 0;
pub const O_RDONLY: ::c_int = 1;
pub const O_WRONLY: ::c_int = 2;
pub const O_RDWR: ::c_int = 3;
pub const O_ACCMODE: ::c_int = 3;
pub const O_LARGEFILE: ::c_int = 0;
pub const O_CREAT: ::c_int = 16;
pub const O_EXCL: ::c_int = 32;
pub const O_NOLINK: ::c_int = 64;
pub const O_NOTRANS: ::c_int = 128;
pub const O_NOFOLLOW: ::c_int = 1048576;
pub const O_DIRECTORY: ::c_int = 2097152;
pub const O_APPEND: ::c_int = 256;
pub const O_ASYNC: ::c_int = 512;
pub const O_FSYNC: ::c_int = 1024;
pub const O_SYNC: ::c_int = 1024;
pub const O_NOATIME: ::c_int = 2048;
pub const O_SHLOCK: ::c_int = 131072;
pub const O_EXLOCK: ::c_int = 262144;
pub const O_DSYNC: ::c_int = 1024;
pub const O_RSYNC: ::c_int = 1024;
pub const O_NONBLOCK: ::c_int = 8;
pub const O_NDELAY: ::c_int = 8;
pub const O_HURD: ::c_int = 458751;
pub const O_TRUNC: ::c_int = 65536;
pub const O_CLOEXEC: ::c_int = 4194304;
pub const O_IGNORE_CTTY: ::c_int = 524288;
pub const O_TMPFILE: ::c_int = 8388608;
pub const O_NOCTTY: ::c_int = 0;
pub const FREAD: ::c_int = 1;
pub const FWRITE: ::c_int = 2;
pub const FASYNC: ::c_int = 512;
pub const FCREAT: ::c_int = 16;
pub const FEXCL: ::c_int = 32;
pub const FTRUNC: ::c_int = 65536;
pub const FNOCTTY: ::c_int = 0;
pub const FFSYNC: ::c_int = 1024;
pub const FSYNC: ::c_int = 1024;
pub const FAPPEND: ::c_int = 256;
pub const FNONBLOCK: ::c_int = 8;
pub const FNDELAY: ::c_int = 8;
pub const F_DUPFD: ::c_int = 0;
pub const F_GETFD: ::c_int = 1;
pub const F_SETFD: ::c_int = 2;
pub const F_GETFL: ::c_int = 3;
pub const F_SETFL: ::c_int = 4;
pub const F_GETOWN: ::c_int = 5;
pub const F_SETOWN: ::c_int = 6;
pub const F_GETLK: ::c_int = 7;
pub const F_SETLK: ::c_int = 8;
pub const F_SETLKW: ::c_int = 9;
pub const F_GETLK64: ::c_int = 10;
pub const F_SETLK64: ::c_int = 11;
pub const F_SETLKW64: ::c_int = 12;
pub const F_DUPFD_CLOEXEC: ::c_int = 1030;
pub const FD_CLOEXEC: ::c_int = 1;
pub const F_RDLCK: ::c_int = 1;
pub const F_WRLCK: ::c_int = 2;
pub const F_UNLCK: ::c_int = 3;
pub const POSIX_FADV_NORMAL: ::c_int = 0;
pub const POSIX_FADV_RANDOM: ::c_int = 1;
pub const POSIX_FADV_SEQUENTIAL: ::c_int = 2;
pub const POSIX_FADV_WILLNEED: ::c_int = 3;
pub const POSIX_FADV_DONTNEED: ::c_int = 4;
pub const POSIX_FADV_NOREUSE: ::c_int = 5;
pub const AT_FDCWD: ::c_int = -100;
pub const AT_SYMLINK_NOFOLLOW: ::c_int = 256;
pub const AT_REMOVEDIR: ::c_int = 512;
pub const AT_SYMLINK_FOLLOW: ::c_int = 1024;
pub const AT_NO_AUTOMOUNT: ::c_int = 2048;
pub const AT_EMPTY_PATH: ::c_int = 4096;
pub const AT_STATX_SYNC_TYPE: ::c_int = 24576;
pub const AT_STATX_SYNC_AS_STAT: ::c_int = 0;
pub const AT_STATX_FORCE_SYNC: ::c_int = 8192;
pub const AT_STATX_DONT_SYNC: ::c_int = 16384;
pub const AT_RECURSIVE: ::c_int = 32768;
pub const AT_EACCESS: ::c_int = 512;

// sys/uio.h
pub const RWF_HIPRI: ::c_int = 1;
pub const RWF_DSYNC: ::c_int = 2;
pub const RWF_SYNC: ::c_int = 4;
pub const RWF_NOWAIT: ::c_int = 8;
pub const RWF_APPEND: ::c_int = 16;

// errno.h
pub const EPERM: ::c_int = 1073741825;
pub const ENOENT: ::c_int = 1073741826;
pub const ESRCH: ::c_int = 1073741827;
pub const EINTR: ::c_int = 1073741828;
pub const EIO: ::c_int = 1073741829;
pub const ENXIO: ::c_int = 1073741830;
pub const E2BIG: ::c_int = 1073741831;
pub const ENOEXEC: ::c_int = 1073741832;
pub const EBADF: ::c_int = 1073741833;
pub const ECHILD: ::c_int = 1073741834;
pub const EDEADLK: ::c_int = 1073741835;
pub const ENOMEM: ::c_int = 1073741836;
pub const EACCES: ::c_int = 1073741837;
pub const EFAULT: ::c_int = 1073741838;
pub const ENOTBLK: ::c_int = 1073741839;
pub const EBUSY: ::c_int = 1073741840;
pub const EEXIST: ::c_int = 1073741841;
pub const EXDEV: ::c_int = 1073741842;
pub const ENODEV: ::c_int = 1073741843;
pub const ENOTDIR: ::c_int = 1073741844;
pub const EISDIR: ::c_int = 1073741845;
pub const EINVAL: ::c_int = 1073741846;
pub const EMFILE: ::c_int = 1073741848;
pub const ENFILE: ::c_int = 1073741847;
pub const ENOTTY: ::c_int = 1073741849;
pub const ETXTBSY: ::c_int = 1073741850;
pub const EFBIG: ::c_int = 1073741851;
pub const ENOSPC: ::c_int = 1073741852;
pub const ESPIPE: ::c_int = 1073741853;
pub const EROFS: ::c_int = 1073741854;
pub const EMLINK: ::c_int = 1073741855;
pub const EPIPE: ::c_int = 1073741856;
pub const EDOM: ::c_int = 1073741857;
pub const ERANGE: ::c_int = 1073741858;
pub const EAGAIN: ::c_int = 1073741859;
pub const EWOULDBLOCK: ::c_int = 1073741859;
pub const EINPROGRESS: ::c_int = 1073741860;
pub const EALREADY: ::c_int = 1073741861;
pub const ENOTSOCK: ::c_int = 1073741862;
pub const EMSGSIZE: ::c_int = 1073741864;
pub const EPROTOTYPE: ::c_int = 1073741865;
pub const ENOPROTOOPT: ::c_int = 1073741866;
pub const EPROTONOSUPPORT: ::c_int = 1073741867;
pub const ESOCKTNOSUPPORT: ::c_int = 1073741868;
pub const EOPNOTSUPP: ::c_int = 1073741869;
pub const EPFNOSUPPORT: ::c_int = 1073741870;
pub const EAFNOSUPPORT: ::c_int = 1073741871;
pub const EADDRINUSE: ::c_int = 1073741872;
pub const EADDRNOTAVAIL: ::c_int = 1073741873;
pub const ENETDOWN: ::c_int = 1073741874;
pub const ENETUNREACH: ::c_int = 1073741875;
pub const ENETRESET: ::c_int = 1073741876;
pub const ECONNABORTED: ::c_int = 1073741877;
pub const ECONNRESET: ::c_int = 1073741878;
pub const ENOBUFS: ::c_int = 1073741879;
pub const EISCONN: ::c_int = 1073741880;
pub const ENOTCONN: ::c_int = 1073741881;
pub const EDESTADDRREQ: ::c_int = 1073741863;
pub const ESHUTDOWN: ::c_int = 1073741882;
pub const ETOOMANYREFS: ::c_int = 1073741883;
pub const ETIMEDOUT: ::c_int = 1073741884;
pub const ECONNREFUSED: ::c_int = 1073741885;
pub const ELOOP: ::c_int = 1073741886;
pub const ENAMETOOLONG: ::c_int = 1073741887;
pub const EHOSTDOWN: ::c_int = 1073741888;
pub const EHOSTUNREACH: ::c_int = 1073741889;
pub const ENOTEMPTY: ::c_int = 1073741890;
pub const EPROCLIM: ::c_int = 1073741891;
pub const EUSERS: ::c_int = 1073741892;
pub const EDQUOT: ::c_int = 1073741893;
pub const ESTALE: ::c_int = 1073741894;
pub const EREMOTE: ::c_int = 1073741895;
pub const EBADRPC: ::c_int = 1073741896;
pub const ERPCMISMATCH: ::c_int = 1073741897;
pub const EPROGUNAVAIL: ::c_int = 1073741898;
pub const EPROGMISMATCH: ::c_int = 1073741899;
pub const EPROCUNAVAIL: ::c_int = 1073741900;
pub const ENOLCK: ::c_int = 1073741901;
pub const EFTYPE: ::c_int = 1073741903;
pub const EAUTH: ::c_int = 1073741904;
pub const ENEEDAUTH: ::c_int = 1073741905;
pub const ENOSYS: ::c_int = 1073741902;
pub const ELIBEXEC: ::c_int = 1073741907;
pub const ENOTSUP: ::c_int = 1073741942;
pub const EILSEQ: ::c_int = 1073741930;
pub const EBACKGROUND: ::c_int = 1073741924;
pub const EDIED: ::c_int = 1073741925;
pub const EGREGIOUS: ::c_int = 1073741927;
pub const EIEIO: ::c_int = 1073741928;
pub const EGRATUITOUS: ::c_int = 1073741929;
pub const EBADMSG: ::c_int = 1073741931;
pub const EIDRM: ::c_int = 1073741932;
pub const EMULTIHOP: ::c_int = 1073741933;
pub const ENODATA: ::c_int = 1073741934;
pub const ENOLINK: ::c_int = 1073741935;
pub const ENOMSG: ::c_int = 1073741936;
pub const ENOSR: ::c_int = 1073741937;
pub const ENOSTR: ::c_int = 1073741938;
pub const EOVERFLOW: ::c_int = 1073741939;
pub const EPROTO: ::c_int = 1073741940;
pub const ETIME: ::c_int = 1073741941;
pub const ECANCELED: ::c_int = 1073741943;
pub const EOWNERDEAD: ::c_int = 1073741944;
pub const ENOTRECOVERABLE: ::c_int = 1073741945;
pub const EMACH_SEND_IN_PROGRESS: ::c_int = 268435457;
pub const EMACH_SEND_INVALID_DATA: ::c_int = 268435458;
pub const EMACH_SEND_INVALID_DEST: ::c_int = 268435459;
pub const EMACH_SEND_TIMED_OUT: ::c_int = 268435460;
pub const EMACH_SEND_WILL_NOTIFY: ::c_int = 268435461;
pub const EMACH_SEND_NOTIFY_IN_PROGRESS: ::c_int = 268435462;
pub const EMACH_SEND_INTERRUPTED: ::c_int = 268435463;
pub const EMACH_SEND_MSG_TOO_SMALL: ::c_int = 268435464;
pub const EMACH_SEND_INVALID_REPLY: ::c_int = 268435465;
pub const EMACH_SEND_INVALID_RIGHT: ::c_int = 268435466;
pub const EMACH_SEND_INVALID_NOTIFY: ::c_int = 268435467;
pub const EMACH_SEND_INVALID_MEMORY: ::c_int = 268435468;
pub const EMACH_SEND_NO_BUFFER: ::c_int = 268435469;
pub const EMACH_SEND_NO_NOTIFY: ::c_int = 268435470;
pub const EMACH_SEND_INVALID_TYPE: ::c_int = 268435471;
pub const EMACH_SEND_INVALID_HEADER: ::c_int = 268435472;
pub const EMACH_RCV_IN_PROGRESS: ::c_int = 268451841;
pub const EMACH_RCV_INVALID_NAME: ::c_int = 268451842;
pub const EMACH_RCV_TIMED_OUT: ::c_int = 268451843;
pub const EMACH_RCV_TOO_LARGE: ::c_int = 268451844;
pub const EMACH_RCV_INTERRUPTED: ::c_int = 268451845;
pub const EMACH_RCV_PORT_CHANGED: ::c_int = 268451846;
pub const EMACH_RCV_INVALID_NOTIFY: ::c_int = 268451847;
pub const EMACH_RCV_INVALID_DATA: ::c_int = 268451848;
pub const EMACH_RCV_PORT_DIED: ::c_int = 268451849;
pub const EMACH_RCV_IN_SET: ::c_int = 268451850;
pub const EMACH_RCV_HEADER_ERROR: ::c_int = 268451851;
pub const EMACH_RCV_BODY_ERROR: ::c_int = 268451852;
pub const EKERN_INVALID_ADDRESS: ::c_int = 1;
pub const EKERN_PROTECTION_FAILURE: ::c_int = 2;
pub const EKERN_NO_SPACE: ::c_int = 3;
pub const EKERN_INVALID_ARGUMENT: ::c_int = 4;
pub const EKERN_FAILURE: ::c_int = 5;
pub const EKERN_RESOURCE_SHORTAGE: ::c_int = 6;
pub const EKERN_NOT_RECEIVER: ::c_int = 7;
pub const EKERN_NO_ACCESS: ::c_int = 8;
pub const EKERN_MEMORY_FAILURE: ::c_int = 9;
pub const EKERN_MEMORY_ERROR: ::c_int = 10;
pub const EKERN_NOT_IN_SET: ::c_int = 12;
pub const EKERN_NAME_EXISTS: ::c_int = 13;
pub const EKERN_ABORTED: ::c_int = 14;
pub const EKERN_INVALID_NAME: ::c_int = 15;
pub const EKERN_INVALID_TASK: ::c_int = 16;
pub const EKERN_INVALID_RIGHT: ::c_int = 17;
pub const EKERN_INVALID_VALUE: ::c_int = 18;
pub const EKERN_UREFS_OVERFLOW: ::c_int = 19;
pub const EKERN_INVALID_CAPABILITY: ::c_int = 20;
pub const EKERN_RIGHT_EXISTS: ::c_int = 21;
pub const EKERN_INVALID_HOST: ::c_int = 22;
pub const EKERN_MEMORY_PRESENT: ::c_int = 23;
pub const EKERN_WRITE_PROTECTION_FAILURE: ::c_int = 24;
pub const EKERN_TERMINATED: ::c_int = 26;
pub const EKERN_TIMEDOUT: ::c_int = 27;
pub const EKERN_INTERRUPTED: ::c_int = 28;
pub const EMIG_TYPE_ERROR: ::c_int = -300;
pub const EMIG_REPLY_MISMATCH: ::c_int = -301;
pub const EMIG_REMOTE_ERROR: ::c_int = -302;
pub const EMIG_BAD_ID: ::c_int = -303;
pub const EMIG_BAD_ARGUMENTS: ::c_int = -304;
pub const EMIG_NO_REPLY: ::c_int = -305;
pub const EMIG_EXCEPTION: ::c_int = -306;
pub const EMIG_ARRAY_TOO_LARGE: ::c_int = -307;
pub const EMIG_SERVER_DIED: ::c_int = -308;
pub const EMIG_DESTROY_REQUEST: ::c_int = -309;
pub const ED_IO_ERROR: ::c_int = 2500;
pub const ED_WOULD_BLOCK: ::c_int = 2501;
pub const ED_NO_SUCH_DEVICE: ::c_int = 2502;
pub const ED_ALREADY_OPEN: ::c_int = 2503;
pub const ED_DEVICE_DOWN: ::c_int = 2504;
pub const ED_INVALID_OPERATION: ::c_int = 2505;
pub const ED_INVALID_RECNUM: ::c_int = 2506;
pub const ED_INVALID_SIZE: ::c_int = 2507;
pub const ED_NO_MEMORY: ::c_int = 2508;
pub const ED_READ_ONLY: ::c_int = 2509;
pub const _HURD_ERRNOS: usize = 122;

// sched.h
pub const SCHED_OTHER: ::c_int = 0;
pub const SCHED_FIFO: ::c_int = 1;
pub const SCHED_RR: ::c_int = 2;
pub const _BITS_TYPES_STRUCT_SCHED_PARAM: usize = 1;
pub const __CPU_SETSIZE: usize = 1024;
pub const CPU_SETSIZE: usize = 1024;

// pthread.h
pub const PTHREAD_SPINLOCK_INITIALIZER: ::c_int = 0;
pub const PTHREAD_CANCEL_DISABLE: ::c_int = 0;
pub const PTHREAD_CANCEL_ENABLE: ::c_int = 1;
pub const PTHREAD_CANCEL_DEFERRED: ::c_int = 0;
pub const PTHREAD_CANCEL_ASYNCHRONOUS: ::c_int = 1;
pub const PTHREAD_BARRIER_SERIAL_THREAD: ::c_int = -1;

// netinet/tcp.h
pub const TCP_NODELAY: ::c_int = 1;
pub const TCP_MAXSEG: ::c_int = 2;
pub const TCP_CORK: ::c_int = 3;
pub const TCP_KEEPIDLE: ::c_int = 4;
pub const TCP_KEEPINTVL: ::c_int = 5;
pub const TCP_KEEPCNT: ::c_int = 6;
pub const TCP_SYNCNT: ::c_int = 7;
pub const TCP_LINGER2: ::c_int = 8;
pub const TCP_DEFER_ACCEPT: ::c_int = 9;
pub const TCP_WINDOW_CLAMP: ::c_int = 10;
pub const TCP_INFO: ::c_int = 11;
pub const TCP_QUICKACK: ::c_int = 12;
pub const TCP_CONGESTION: ::c_int = 13;
pub const TCP_MD5SIG: ::c_int = 14;
pub const TCP_COOKIE_TRANSACTIONS: ::c_int = 15;
pub const TCP_THIN_LINEAR_TIMEOUTS: ::c_int = 16;
pub const TCP_THIN_DUPACK: ::c_int = 17;
pub const TCP_USER_TIMEOUT: ::c_int = 18;
pub const TCP_REPAIR: ::c_int = 19;
pub const TCP_REPAIR_QUEUE: ::c_int = 20;
pub const TCP_QUEUE_SEQ: ::c_int = 21;
pub const TCP_REPAIR_OPTIONS: ::c_int = 22;
pub const TCP_FASTOPEN: ::c_int = 23;
pub const TCP_TIMESTAMP: ::c_int = 24;
pub const TCP_NOTSENT_LOWAT: ::c_int = 25;
pub const TCP_CC_INFO: ::c_int = 26;
pub const TCP_SAVE_SYN: ::c_int = 27;
pub const TCP_SAVED_SYN: ::c_int = 28;
pub const TCP_REPAIR_WINDOW: ::c_int = 29;
pub const TCP_FASTOPEN_CONNECT: ::c_int = 30;
pub const TCP_ULP: ::c_int = 31;
pub const TCP_MD5SIG_EXT: ::c_int = 32;
pub const TCP_FASTOPEN_KEY: ::c_int = 33;
pub const TCP_FASTOPEN_NO_COOKIE: ::c_int = 34;
pub const TCP_ZEROCOPY_RECEIVE: ::c_int = 35;
pub const TCP_INQ: ::c_int = 36;
pub const TCP_CM_INQ: ::c_int = 36;
pub const TCP_TX_DELAY: ::c_int = 37;
pub const TCP_REPAIR_ON: ::c_int = 1;
pub const TCP_REPAIR_OFF: ::c_int = 0;
pub const TCP_REPAIR_OFF_NO_WP: ::c_int = -1;

// stdint.h
pub const INT8_MIN: i8 = -128;
pub const INT16_MIN: i16 = -32768;
pub const INT32_MIN: i32 = -2147483648;
pub const INT8_MAX: i8 = 127;
pub const INT16_MAX: i16 = 32767;
pub const INT32_MAX: i32 = 2147483647;
pub const UINT8_MAX: u8 = 255;
pub const UINT16_MAX: u16 = 65535;
pub const UINT32_MAX: u32 = 4294967295;
pub const INT_LEAST8_MIN: int_least8_t = -128;
pub const INT_LEAST16_MIN: int_least16_t = -32768;
pub const INT_LEAST32_MIN: int_least32_t = -2147483648;
pub const INT_LEAST8_MAX: int_least8_t = 127;
pub const INT_LEAST16_MAX: int_least16_t = 32767;
pub const INT_LEAST32_MAX: int_least32_t = 2147483647;
pub const UINT_LEAST8_MAX: uint_least8_t = 255;
pub const UINT_LEAST16_MAX: uint_least16_t = 65535;
pub const UINT_LEAST32_MAX: uint_least32_t = 4294967295;
pub const INT_FAST8_MIN: int_fast8_t = -128;
pub const INT_FAST16_MIN: int_fast16_t = -2147483648;
pub const INT_FAST32_MIN: int_fast32_t = -2147483648;
pub const INT_FAST8_MAX: int_fast8_t = 127;
pub const INT_FAST16_MAX: int_fast16_t = 2147483647;
pub const INT_FAST32_MAX: int_fast32_t = 2147483647;
pub const UINT_FAST8_MAX: uint_fast8_t = 255;
pub const UINT_FAST16_MAX: uint_fast16_t = 4294967295;
pub const UINT_FAST32_MAX: uint_fast32_t = 4294967295;
pub const INTPTR_MIN: __intptr_t = -2147483648;
pub const INTPTR_MAX: __intptr_t = 2147483647;
pub const UINTPTR_MAX: usize = 4294967295;
pub const PTRDIFF_MIN: __ptrdiff_t = -2147483648;
pub const PTRDIFF_MAX: __ptrdiff_t = 2147483647;
pub const SIG_ATOMIC_MIN: __sig_atomic_t = -2147483648;
pub const SIG_ATOMIC_MAX: __sig_atomic_t = 2147483647;
pub const SIZE_MAX: usize = 4294967295;
pub const WINT_MIN: wint_t = 0;
pub const WINT_MAX: wint_t = 4294967295;
pub const INT8_WIDTH: usize = 8;
pub const UINT8_WIDTH: usize = 8;
pub const INT16_WIDTH: usize = 16;
pub const UINT16_WIDTH: usize = 16;
pub const INT32_WIDTH: usize = 32;
pub const UINT32_WIDTH: usize = 32;
pub const INT64_WIDTH: usize = 64;
pub const UINT64_WIDTH: usize = 64;
pub const INT_LEAST8_WIDTH: usize = 8;
pub const UINT_LEAST8_WIDTH: usize = 8;
pub const INT_LEAST16_WIDTH: usize = 16;
pub const UINT_LEAST16_WIDTH: usize = 16;
pub const INT_LEAST32_WIDTH: usize = 32;
pub const UINT_LEAST32_WIDTH: usize = 32;
pub const INT_LEAST64_WIDTH: usize = 64;
pub const UINT_LEAST64_WIDTH: usize = 64;
pub const INT_FAST8_WIDTH: usize = 8;
pub const UINT_FAST8_WIDTH: usize = 8;
pub const INT_FAST16_WIDTH: usize = 32;
pub const UINT_FAST16_WIDTH: usize = 32;
pub const INT_FAST32_WIDTH: usize = 32;
pub const UINT_FAST32_WIDTH: usize = 32;
pub const INT_FAST64_WIDTH: usize = 64;
pub const UINT_FAST64_WIDTH: usize = 64;
pub const INTPTR_WIDTH: usize = 32;
pub const UINTPTR_WIDTH: usize = 32;
pub const INTMAX_WIDTH: usize = 64;
pub const UINTMAX_WIDTH: usize = 64;
pub const PTRDIFF_WIDTH: usize = 32;
pub const SIG_ATOMIC_WIDTH: usize = 32;
pub const SIZE_WIDTH: usize = 32;
pub const WCHAR_WIDTH: usize = 32;
pub const WINT_WIDTH: usize = 32;

pub const TH_FIN: u8 = 1;
pub const TH_SYN: u8 = 2;
pub const TH_RST: u8 = 4;
pub const TH_PUSH: u8 = 8;
pub const TH_ACK: u8 = 16;
pub const TH_URG: u8 = 32;
pub const TCPOPT_EOL: u8 = 0;
pub const TCPOPT_NOP: u8 = 1;
pub const TCPOPT_MAXSEG: u8 = 2;
pub const TCPOLEN_MAXSEG: u8 = 4;
pub const TCPOPT_WINDOW: u8 = 3;
pub const TCPOLEN_WINDOW: u8 = 3;
pub const TCPOPT_SACK_PERMITTED: u8 = 4;
pub const TCPOLEN_SACK_PERMITTED: u8 = 2;
pub const TCPOPT_SACK: u8 = 5;
pub const TCPOPT_TIMESTAMP: u8 = 8;
pub const TCPOLEN_TIMESTAMP: u8 = 10;
pub const TCPOLEN_TSTAMP_APPA: u8 = 12;
pub const TCPOPT_TSTAMP_HDR: u32 = 16844810;
pub const TCP_MSS: usize = 512;
pub const TCP_MAXWIN: usize = 65535;
pub const TCP_MAX_WINSHIFT: usize = 14;
pub const SOL_TCP: ::c_int = 6;
pub const TCPI_OPT_TIMESTAMPS: u8 = 1;
pub const TCPI_OPT_SACK: u8 = 2;
pub const TCPI_OPT_WSCALE: u8 = 4;
pub const TCPI_OPT_ECN: u8 = 8;
pub const TCPI_OPT_ECN_SEEN: u8 = 16;
pub const TCPI_OPT_SYN_DATA: u8 = 32;
pub const TCP_MD5SIG_MAXKEYLEN: usize = 80;
pub const TCP_MD5SIG_FLAG_PREFIX: usize = 1;
pub const TCP_COOKIE_MIN: usize = 8;
pub const TCP_COOKIE_MAX: usize = 16;
pub const TCP_COOKIE_PAIR_SIZE: usize = 32;
pub const TCP_COOKIE_IN_ALWAYS: ::c_int = 1;
pub const TCP_COOKIE_OUT_NEVER: ::c_int = 2;
pub const TCP_S_DATA_IN: ::c_int = 4;
pub const TCP_S_DATA_OUT: ::c_int = 8;
pub const TCP_MSS_DEFAULT: usize = 536;
pub const TCP_MSS_DESIRED: usize = 1220;

// sys/wait.h
pub const WCOREFLAG: ::c_int = 128;
pub const WAIT_ANY: pid_t = -1;
pub const WAIT_MYPGRP: pid_t = 0;

// sys/file.h
pub const LOCK_SH: ::c_int = 1;
pub const LOCK_EX: ::c_int = 2;
pub const LOCK_UN: ::c_int = 8;
pub const LOCK_NB: ::c_int = 4;

// sys/mman.h
pub const PROT_NONE: ::c_int = 0;
pub const PROT_READ: ::c_int = 4;
pub const PROT_WRITE: ::c_int = 2;
pub const PROT_EXEC: ::c_int = 1;
pub const MAP_PRIVATE: ::c_int = 0;
pub const MAP_FILE: ::c_int = 1;
pub const MAP_ANON: ::c_int = 2;
pub const MAP_SHARED: ::c_int = 16;
pub const MAP_COPY: ::c_int = 32;
pub const MAP_FIXED: ::c_int = 256;
pub const MAP_FAILED: *mut ::c_void = !0 as *mut ::c_void;
pub const MS_SYNC: ::c_int = 0;
pub const MS_ASYNC: ::c_int = 1;
pub const MS_INVALIDATE: ::c_int = 2;
pub const MADV_NORMAL: ::c_int = 0;
pub const MADV_RANDOM: ::c_int = 1;
pub const MADV_SEQUENTIAL: ::c_int = 2;
pub const MADV_WILLNEED: ::c_int = 3;
pub const MADV_DONTNEED: ::c_int = 4;

// random.h
pub const GRND_NONBLOCK: ::c_uint = 1;
pub const GRND_RANDOM: ::c_uint = 2;
pub const GRND_INSECURE: ::c_uint = 4;

pub const _PC_LINK_MAX: ::c_int = 0;
pub const _PC_MAX_CANON: ::c_int = 1;
pub const _PC_MAX_INPUT: ::c_int = 2;
pub const _PC_NAME_MAX: ::c_int = 3;
pub const _PC_PATH_MAX: ::c_int = 4;
pub const _PC_PIPE_BUF: ::c_int = 5;
pub const _PC_CHOWN_RESTRICTED: ::c_int = 6;
pub const _PC_NO_TRUNC: ::c_int = 7;
pub const _PC_VDISABLE: ::c_int = 8;
pub const _PC_SYNC_IO: ::c_int = 9;
pub const _PC_ASYNC_IO: ::c_int = 10;
pub const _PC_PRIO_IO: ::c_int = 11;
pub const _PC_SOCK_MAXBUF: ::c_int = 12;
pub const _PC_FILESIZEBITS: ::c_int = 13;
pub const _PC_REC_INCR_XFER_SIZE: ::c_int = 14;
pub const _PC_REC_MAX_XFER_SIZE: ::c_int = 15;
pub const _PC_REC_MIN_XFER_SIZE: ::c_int = 16;
pub const _PC_REC_XFER_ALIGN: ::c_int = 17;
pub const _PC_ALLOC_SIZE_MIN: ::c_int = 18;
pub const _PC_SYMLINK_MAX: ::c_int = 19;
pub const _PC_2_SYMLINKS: ::c_int = 20;
pub const _SC_ARG_MAX: ::c_int = 0;
pub const _SC_CHILD_MAX: ::c_int = 1;
pub const _SC_CLK_TCK: ::c_int = 2;
pub const _SC_NGROUPS_MAX: ::c_int = 3;
pub const _SC_OPEN_MAX: ::c_int = 4;
pub const _SC_STREAM_MAX: ::c_int = 5;
pub const _SC_TZNAME_MAX: ::c_int = 6;
pub const _SC_JOB_CONTROL: ::c_int = 7;
pub const _SC_SAVED_IDS: ::c_int = 8;
pub const _SC_REALTIME_SIGNALS: ::c_int = 9;
pub const _SC_PRIORITY_SCHEDULING: ::c_int = 10;
pub const _SC_TIMERS: ::c_int = 11;
pub const _SC_ASYNCHRONOUS_IO: ::c_int = 12;
pub const _SC_PRIORITIZED_IO: ::c_int = 13;
pub const _SC_SYNCHRONIZED_IO: ::c_int = 14;
pub const _SC_FSYNC: ::c_int = 15;
pub const _SC_MAPPED_FILES: ::c_int = 16;
pub const _SC_MEMLOCK: ::c_int = 17;
pub const _SC_MEMLOCK_RANGE: ::c_int = 18;
pub const _SC_MEMORY_PROTECTION: ::c_int = 19;
pub const _SC_MESSAGE_PASSING: ::c_int = 20;
pub const _SC_SEMAPHORES: ::c_int = 21;
pub const _SC_SHARED_MEMORY_OBJECTS: ::c_int = 22;
pub const _SC_AIO_LISTIO_MAX: ::c_int = 23;
pub const _SC_AIO_MAX: ::c_int = 24;
pub const _SC_AIO_PRIO_DELTA_MAX: ::c_int = 25;
pub const _SC_DELAYTIMER_MAX: ::c_int = 26;
pub const _SC_MQ_OPEN_MAX: ::c_int = 27;
pub const _SC_MQ_PRIO_MAX: ::c_int = 28;
pub const _SC_VERSION: ::c_int = 29;
pub const _SC_PAGESIZE: ::c_int = 30;
pub const _SC_PAGE_SIZE: ::c_int = 30;
pub const _SC_RTSIG_MAX: ::c_int = 31;
pub const _SC_SEM_NSEMS_MAX: ::c_int = 32;
pub const _SC_SEM_VALUE_MAX: ::c_int = 33;
pub const _SC_SIGQUEUE_MAX: ::c_int = 34;
pub const _SC_TIMER_MAX: ::c_int = 35;
pub const _SC_BC_BASE_MAX: ::c_int = 36;
pub const _SC_BC_DIM_MAX: ::c_int = 37;
pub const _SC_BC_SCALE_MAX: ::c_int = 38;
pub const _SC_BC_STRING_MAX: ::c_int = 39;
pub const _SC_COLL_WEIGHTS_MAX: ::c_int = 40;
pub const _SC_EQUIV_CLASS_MAX: ::c_int = 41;
pub const _SC_EXPR_NEST_MAX: ::c_int = 42;
pub const _SC_LINE_MAX: ::c_int = 43;
pub const _SC_RE_DUP_MAX: ::c_int = 44;
pub const _SC_CHARCLASS_NAME_MAX: ::c_int = 45;
pub const _SC_2_VERSION: ::c_int = 46;
pub const _SC_2_C_BIND: ::c_int = 47;
pub const _SC_2_C_DEV: ::c_int = 48;
pub const _SC_2_FORT_DEV: ::c_int = 49;
pub const _SC_2_FORT_RUN: ::c_int = 50;
pub const _SC_2_SW_DEV: ::c_int = 51;
pub const _SC_2_LOCALEDEF: ::c_int = 52;
pub const _SC_PII: ::c_int = 53;
pub const _SC_PII_XTI: ::c_int = 54;
pub const _SC_PII_SOCKET: ::c_int = 55;
pub const _SC_PII_INTERNET: ::c_int = 56;
pub const _SC_PII_OSI: ::c_int = 57;
pub const _SC_POLL: ::c_int = 58;
pub const _SC_SELECT: ::c_int = 59;
pub const _SC_UIO_MAXIOV: ::c_int = 60;
pub const _SC_IOV_MAX: ::c_int = 60;
pub const _SC_PII_INTERNET_STREAM: ::c_int = 61;
pub const _SC_PII_INTERNET_DGRAM: ::c_int = 62;
pub const _SC_PII_OSI_COTS: ::c_int = 63;
pub const _SC_PII_OSI_CLTS: ::c_int = 64;
pub const _SC_PII_OSI_M: ::c_int = 65;
pub const _SC_T_IOV_MAX: ::c_int = 66;
pub const _SC_THREADS: ::c_int = 67;
pub const _SC_THREAD_SAFE_FUNCTIONS: ::c_int = 68;
pub const _SC_GETGR_R_SIZE_MAX: ::c_int = 69;
pub const _SC_GETPW_R_SIZE_MAX: ::c_int = 70;
pub const _SC_LOGIN_NAME_MAX: ::c_int = 71;
pub const _SC_TTY_NAME_MAX: ::c_int = 72;
pub const _SC_THREAD_DESTRUCTOR_ITERATIONS: ::c_int = 73;
pub const _SC_THREAD_KEYS_MAX: ::c_int = 74;
pub const _SC_THREAD_STACK_MIN: ::c_int = 75;
pub const _SC_THREAD_THREADS_MAX: ::c_int = 76;
pub const _SC_THREAD_ATTR_STACKADDR: ::c_int = 77;
pub const _SC_THREAD_ATTR_STACKSIZE: ::c_int = 78;
pub const _SC_THREAD_PRIORITY_SCHEDULING: ::c_int = 79;
pub const _SC_THREAD_PRIO_INHERIT: ::c_int = 80;
pub const _SC_THREAD_PRIO_PROTECT: ::c_int = 81;
pub const _SC_THREAD_PROCESS_SHARED: ::c_int = 82;
pub const _SC_NPROCESSORS_CONF: ::c_int = 83;
pub const _SC_NPROCESSORS_ONLN: ::c_int = 84;
pub const _SC_PHYS_PAGES: ::c_int = 85;
pub const _SC_AVPHYS_PAGES: ::c_int = 86;
pub const _SC_ATEXIT_MAX: ::c_int = 87;
pub const _SC_PASS_MAX: ::c_int = 88;
pub const _SC_XOPEN_VERSION: ::c_int = 89;
pub const _SC_XOPEN_XCU_VERSION: ::c_int = 90;
pub const _SC_XOPEN_UNIX: ::c_int = 91;
pub const _SC_XOPEN_CRYPT: ::c_int = 92;
pub const _SC_XOPEN_ENH_I18N: ::c_int = 93;
pub const _SC_XOPEN_SHM: ::c_int = 94;
pub const _SC_2_CHAR_TERM: ::c_int = 95;
pub const _SC_2_C_VERSION: ::c_int = 96;
pub const _SC_2_UPE: ::c_int = 97;
pub const _SC_XOPEN_XPG2: ::c_int = 98;
pub const _SC_XOPEN_XPG3: ::c_int = 99;
pub const _SC_XOPEN_XPG4: ::c_int = 100;
pub const _SC_CHAR_BIT: ::c_int = 101;
pub const _SC_CHAR_MAX: ::c_int = 102;
pub const _SC_CHAR_MIN: ::c_int = 103;
pub const _SC_INT_MAX: ::c_int = 104;
pub const _SC_INT_MIN: ::c_int = 105;
pub const _SC_LONG_BIT: ::c_int = 106;
pub const _SC_WORD_BIT: ::c_int = 107;
pub const _SC_MB_LEN_MAX: ::c_int = 108;
pub const _SC_NZERO: ::c_int = 109;
pub const _SC_SSIZE_MAX: ::c_int = 110;
pub const _SC_SCHAR_MAX: ::c_int = 111;
pub const _SC_SCHAR_MIN: ::c_int = 112;
pub const _SC_SHRT_MAX: ::c_int = 113;
pub const _SC_SHRT_MIN: ::c_int = 114;
pub const _SC_UCHAR_MAX: ::c_int = 115;
pub const _SC_UINT_MAX: ::c_int = 116;
pub const _SC_ULONG_MAX: ::c_int = 117;
pub const _SC_USHRT_MAX: ::c_int = 118;
pub const _SC_NL_ARGMAX: ::c_int = 119;
pub const _SC_NL_LANGMAX: ::c_int = 120;
pub const _SC_NL_MSGMAX: ::c_int = 121;
pub const _SC_NL_NMAX: ::c_int = 122;
pub const _SC_NL_SETMAX: ::c_int = 123;
pub const _SC_NL_TEXTMAX: ::c_int = 124;
pub const _SC_XBS5_ILP32_OFF32: ::c_int = 125;
pub const _SC_XBS5_ILP32_OFFBIG: ::c_int = 126;
pub const _SC_XBS5_LP64_OFF64: ::c_int = 127;
pub const _SC_XBS5_LPBIG_OFFBIG: ::c_int = 128;
pub const _SC_XOPEN_LEGACY: ::c_int = 129;
pub const _SC_XOPEN_REALTIME: ::c_int = 130;
pub const _SC_XOPEN_REALTIME_THREADS: ::c_int = 131;
pub const _SC_ADVISORY_INFO: ::c_int = 132;
pub const _SC_BARRIERS: ::c_int = 133;
pub const _SC_BASE: ::c_int = 134;
pub const _SC_C_LANG_SUPPORT: ::c_int = 135;
pub const _SC_C_LANG_SUPPORT_R: ::c_int = 136;
pub const _SC_CLOCK_SELECTION: ::c_int = 137;
pub const _SC_CPUTIME: ::c_int = 138;
pub const _SC_THREAD_CPUTIME: ::c_int = 139;
pub const _SC_DEVICE_IO: ::c_int = 140;
pub const _SC_DEVICE_SPECIFIC: ::c_int = 141;
pub const _SC_DEVICE_SPECIFIC_R: ::c_int = 142;
pub const _SC_FD_MGMT: ::c_int = 143;
pub const _SC_FIFO: ::c_int = 144;
pub const _SC_PIPE: ::c_int = 145;
pub const _SC_FILE_ATTRIBUTES: ::c_int = 146;
pub const _SC_FILE_LOCKING: ::c_int = 147;
pub const _SC_FILE_SYSTEM: ::c_int = 148;
pub const _SC_MONOTONIC_CLOCK: ::c_int = 149;
pub const _SC_MULTI_PROCESS: ::c_int = 150;
pub const _SC_SINGLE_PROCESS: ::c_int = 151;
pub const _SC_NETWORKING: ::c_int = 152;
pub const _SC_READER_WRITER_LOCKS: ::c_int = 153;
pub const _SC_SPIN_LOCKS: ::c_int = 154;
pub const _SC_REGEXP: ::c_int = 155;
pub const _SC_REGEX_VERSION: ::c_int = 156;
pub const _SC_SHELL: ::c_int = 157;
pub const _SC_SIGNALS: ::c_int = 158;
pub const _SC_SPAWN: ::c_int = 159;
pub const _SC_SPORADIC_SERVER: ::c_int = 160;
pub const _SC_THREAD_SPORADIC_SERVER: ::c_int = 161;
pub const _SC_SYSTEM_DATABASE: ::c_int = 162;
pub const _SC_SYSTEM_DATABASE_R: ::c_int = 163;
pub const _SC_TIMEOUTS: ::c_int = 164;
pub const _SC_TYPED_MEMORY_OBJECTS: ::c_int = 165;
pub const _SC_USER_GROUPS: ::c_int = 166;
pub const _SC_USER_GROUPS_R: ::c_int = 167;
pub const _SC_2_PBS: ::c_int = 168;
pub const _SC_2_PBS_ACCOUNTING: ::c_int = 169;
pub const _SC_2_PBS_LOCATE: ::c_int = 170;
pub const _SC_2_PBS_MESSAGE: ::c_int = 171;
pub const _SC_2_PBS_TRACK: ::c_int = 172;
pub const _SC_SYMLOOP_MAX: ::c_int = 173;
pub const _SC_STREAMS: ::c_int = 174;
pub const _SC_2_PBS_CHECKPOINT: ::c_int = 175;
pub const _SC_V6_ILP32_OFF32: ::c_int = 176;
pub const _SC_V6_ILP32_OFFBIG: ::c_int = 177;
pub const _SC_V6_LP64_OFF64: ::c_int = 178;
pub const _SC_V6_LPBIG_OFFBIG: ::c_int = 179;
pub const _SC_HOST_NAME_MAX: ::c_int = 180;
pub const _SC_TRACE: ::c_int = 181;
pub const _SC_TRACE_EVENT_FILTER: ::c_int = 182;
pub const _SC_TRACE_INHERIT: ::c_int = 183;
pub const _SC_TRACE_LOG: ::c_int = 184;
pub const _SC_LEVEL1_ICACHE_SIZE: ::c_int = 185;
pub const _SC_LEVEL1_ICACHE_ASSOC: ::c_int = 186;
pub const _SC_LEVEL1_ICACHE_LINESIZE: ::c_int = 187;
pub const _SC_LEVEL1_DCACHE_SIZE: ::c_int = 188;
pub const _SC_LEVEL1_DCACHE_ASSOC: ::c_int = 189;
pub const _SC_LEVEL1_DCACHE_LINESIZE: ::c_int = 190;
pub const _SC_LEVEL2_CACHE_SIZE: ::c_int = 191;
pub const _SC_LEVEL2_CACHE_ASSOC: ::c_int = 192;
pub const _SC_LEVEL2_CACHE_LINESIZE: ::c_int = 193;
pub const _SC_LEVEL3_CACHE_SIZE: ::c_int = 194;
pub const _SC_LEVEL3_CACHE_ASSOC: ::c_int = 195;
pub const _SC_LEVEL3_CACHE_LINESIZE: ::c_int = 196;
pub const _SC_LEVEL4_CACHE_SIZE: ::c_int = 197;
pub const _SC_LEVEL4_CACHE_ASSOC: ::c_int = 198;
pub const _SC_LEVEL4_CACHE_LINESIZE: ::c_int = 199;
pub const _SC_IPV6: ::c_int = 235;
pub const _SC_RAW_SOCKETS: ::c_int = 236;
pub const _SC_V7_ILP32_OFF32: ::c_int = 237;
pub const _SC_V7_ILP32_OFFBIG: ::c_int = 238;
pub const _SC_V7_LP64_OFF64: ::c_int = 239;
pub const _SC_V7_LPBIG_OFFBIG: ::c_int = 240;
pub const _SC_SS_REPL_MAX: ::c_int = 241;
pub const _SC_TRACE_EVENT_NAME_MAX: ::c_int = 242;
pub const _SC_TRACE_NAME_MAX: ::c_int = 243;
pub const _SC_TRACE_SYS_MAX: ::c_int = 244;
pub const _SC_TRACE_USER_EVENT_MAX: ::c_int = 245;
pub const _SC_XOPEN_STREAMS: ::c_int = 246;
pub const _SC_THREAD_ROBUST_PRIO_INHERIT: ::c_int = 247;
pub const _SC_THREAD_ROBUST_PRIO_PROTECT: ::c_int = 248;
pub const _SC_MINSIGSTKSZ: ::c_int = 249;
pub const _SC_SIGSTKSZ: ::c_int = 250;

pub const _CS_PATH: ::c_int = 0;
pub const _CS_V6_WIDTH_RESTRICTED_ENVS: ::c_int = 1;
pub const _CS_GNU_LIBC_VERSION: ::c_int = 2;
pub const _CS_GNU_LIBPTHREAD_VERSION: ::c_int = 3;
pub const _CS_V5_WIDTH_RESTRICTED_ENVS: ::c_int = 4;
pub const _CS_V7_WIDTH_RESTRICTED_ENVS: ::c_int = 5;
pub const _CS_LFS_CFLAGS: ::c_int = 1000;
pub const _CS_LFS_LDFLAGS: ::c_int = 1001;
pub const _CS_LFS_LIBS: ::c_int = 1002;
pub const _CS_LFS_LINTFLAGS: ::c_int = 1003;
pub const _CS_LFS64_CFLAGS: ::c_int = 1004;
pub const _CS_LFS64_LDFLAGS: ::c_int = 1005;
pub const _CS_LFS64_LIBS: ::c_int = 1006;
pub const _CS_LFS64_LINTFLAGS: ::c_int = 1007;
pub const _CS_XBS5_ILP32_OFF32_CFLAGS: ::c_int = 1100;
pub const _CS_XBS5_ILP32_OFF32_LDFLAGS: ::c_int = 1101;
pub const _CS_XBS5_ILP32_OFF32_LIBS: ::c_int = 1102;
pub const _CS_XBS5_ILP32_OFF32_LINTFLAGS: ::c_int = 1103;
pub const _CS_XBS5_ILP32_OFFBIG_CFLAGS: ::c_int = 1104;
pub const _CS_XBS5_ILP32_OFFBIG_LDFLAGS: ::c_int = 1105;
pub const _CS_XBS5_ILP32_OFFBIG_LIBS: ::c_int = 1106;
pub const _CS_XBS5_ILP32_OFFBIG_LINTFLAGS: ::c_int = 1107;
pub const _CS_XBS5_LP64_OFF64_CFLAGS: ::c_int = 1108;
pub const _CS_XBS5_LP64_OFF64_LDFLAGS: ::c_int = 1109;
pub const _CS_XBS5_LP64_OFF64_LIBS: ::c_int = 1110;
pub const _CS_XBS5_LP64_OFF64_LINTFLAGS: ::c_int = 1111;
pub const _CS_XBS5_LPBIG_OFFBIG_CFLAGS: ::c_int = 1112;
pub const _CS_XBS5_LPBIG_OFFBIG_LDFLAGS: ::c_int = 1113;
pub const _CS_XBS5_LPBIG_OFFBIG_LIBS: ::c_int = 1114;
pub const _CS_XBS5_LPBIG_OFFBIG_LINTFLAGS: ::c_int = 1115;
pub const _CS_POSIX_V6_ILP32_OFF32_CFLAGS: ::c_int = 1116;
pub const _CS_POSIX_V6_ILP32_OFF32_LDFLAGS: ::c_int = 1117;
pub const _CS_POSIX_V6_ILP32_OFF32_LIBS: ::c_int = 1118;
pub const _CS_POSIX_V6_ILP32_OFF32_LINTFLAGS: ::c_int = 1119;
pub const _CS_POSIX_V6_ILP32_OFFBIG_CFLAGS: ::c_int = 1120;
pub const _CS_POSIX_V6_ILP32_OFFBIG_LDFLAGS: ::c_int = 1121;
pub const _CS_POSIX_V6_ILP32_OFFBIG_LIBS: ::c_int = 1122;
pub const _CS_POSIX_V6_ILP32_OFFBIG_LINTFLAGS: ::c_int = 1123;
pub const _CS_POSIX_V6_LP64_OFF64_CFLAGS: ::c_int = 1124;
pub const _CS_POSIX_V6_LP64_OFF64_LDFLAGS: ::c_int = 1125;
pub const _CS_POSIX_V6_LP64_OFF64_LIBS: ::c_int = 1126;
pub const _CS_POSIX_V6_LP64_OFF64_LINTFLAGS: ::c_int = 1127;
pub const _CS_POSIX_V6_LPBIG_OFFBIG_CFLAGS: ::c_int = 1128;
pub const _CS_POSIX_V6_LPBIG_OFFBIG_LDFLAGS: ::c_int = 1129;
pub const _CS_POSIX_V6_LPBIG_OFFBIG_LIBS: ::c_int = 1130;
pub const _CS_POSIX_V6_LPBIG_OFFBIG_LINTFLAGS: ::c_int = 1131;
pub const _CS_POSIX_V7_ILP32_OFF32_CFLAGS: ::c_int = 1132;
pub const _CS_POSIX_V7_ILP32_OFF32_LDFLAGS: ::c_int = 1133;
pub const _CS_POSIX_V7_ILP32_OFF32_LIBS: ::c_int = 1134;
pub const _CS_POSIX_V7_ILP32_OFF32_LINTFLAGS: ::c_int = 1135;
pub const _CS_POSIX_V7_ILP32_OFFBIG_CFLAGS: ::c_int = 1136;
pub const _CS_POSIX_V7_ILP32_OFFBIG_LDFLAGS: ::c_int = 1137;
pub const _CS_POSIX_V7_ILP32_OFFBIG_LIBS: ::c_int = 1138;
pub const _CS_POSIX_V7_ILP32_OFFBIG_LINTFLAGS: ::c_int = 1139;
pub const _CS_POSIX_V7_LP64_OFF64_CFLAGS: ::c_int = 1140;
pub const _CS_POSIX_V7_LP64_OFF64_LDFLAGS: ::c_int = 1141;
pub const _CS_POSIX_V7_LP64_OFF64_LIBS: ::c_int = 1142;
pub const _CS_POSIX_V7_LP64_OFF64_LINTFLAGS: ::c_int = 1143;
pub const _CS_POSIX_V7_LPBIG_OFFBIG_CFLAGS: ::c_int = 1144;
pub const _CS_POSIX_V7_LPBIG_OFFBIG_LDFLAGS: ::c_int = 1145;
pub const _CS_POSIX_V7_LPBIG_OFFBIG_LIBS: ::c_int = 1146;
pub const _CS_POSIX_V7_LPBIG_OFFBIG_LINTFLAGS: ::c_int = 1147;
pub const _CS_V6_ENV: ::c_int = 1148;
pub const _CS_V7_ENV: ::c_int = 1149;

pub const PTHREAD_PROCESS_PRIVATE: __pthread_process_shared = 0;
pub const PTHREAD_PROCESS_SHARED: __pthread_process_shared = 1;

pub const PTHREAD_EXPLICIT_SCHED: __pthread_inheritsched = 0;
pub const PTHREAD_INHERIT_SCHED: __pthread_inheritsched = 1;

pub const PTHREAD_SCOPE_SYSTEM: __pthread_contentionscope = 0;
pub const PTHREAD_SCOPE_PROCESS: __pthread_contentionscope = 1;

pub const PTHREAD_CREATE_JOINABLE: __pthread_detachstate = 0;
pub const PTHREAD_CREATE_DETACHED: __pthread_detachstate = 1;

pub const PTHREAD_PRIO_NONE: __pthread_mutex_protocol = 0;
pub const PTHREAD_PRIO_INHERIT: __pthread_mutex_protocol = 1;
pub const PTHREAD_PRIO_PROTECT: __pthread_mutex_protocol = 2;

pub const PTHREAD_MUTEX_TIMED: __pthread_mutex_type = 0;
pub const PTHREAD_MUTEX_ERRORCHECK: __pthread_mutex_type = 1;
pub const PTHREAD_MUTEX_RECURSIVE: __pthread_mutex_type = 2;

pub const PTHREAD_MUTEX_STALLED: __pthread_mutex_robustness = 0;
pub const PTHREAD_MUTEX_ROBUST: __pthread_mutex_robustness = 256;

pub const RLIMIT_CPU: __rlimit_resource = 0;
pub const RLIMIT_FSIZE: __rlimit_resource = 1;
pub const RLIMIT_DATA: __rlimit_resource = 2;
pub const RLIMIT_STACK: __rlimit_resource = 3;
pub const RLIMIT_CORE: __rlimit_resource = 4;
pub const RLIMIT_RSS: __rlimit_resource = 5;
pub const RLIMIT_MEMLOCK: __rlimit_resource = 6;
pub const RLIMIT_NPROC: __rlimit_resource = 7;
pub const RLIMIT_OFILE: __rlimit_resource = 8;
pub const RLIMIT_NOFILE: __rlimit_resource = 8;
pub const RLIMIT_SBSIZE: __rlimit_resource = 9;
pub const RLIMIT_AS: __rlimit_resource = 10;
pub const RLIMIT_VMEM: __rlimit_resource = 10;
pub const RLIMIT_NLIMITS: __rlimit_resource = 11;
pub const RLIM_NLIMITS: __rlimit_resource = 11;

pub const RUSAGE_SELF: __rusage_who = 0;
pub const RUSAGE_CHILDREN: __rusage_who = -1;

pub const PRIO_PROCESS: __priority_which = 0;
pub const PRIO_PGRP: __priority_which = 1;
pub const PRIO_USER: __priority_which = 2;

pub const SOCK_STREAM: ::c_int = 1;
pub const SOCK_DGRAM: ::c_int = 2;
pub const SOCK_RAW: ::c_int = 3;
pub const SOCK_RDM: ::c_int = 4;
pub const SOCK_SEQPACKET: ::c_int = 5;
pub const SOCK_CLOEXEC: ::c_int = 4194304;
pub const SOCK_NONBLOCK: ::c_int = 2048;

pub const MSG_OOB: ::c_int = 1;
pub const MSG_PEEK: ::c_int = 2;
pub const MSG_DONTROUTE: ::c_int = 4;
pub const MSG_EOR: ::c_int = 8;
pub const MSG_TRUNC: ::c_int = 16;
pub const MSG_CTRUNC: ::c_int = 32;
pub const MSG_WAITALL: ::c_int = 64;
pub const MSG_DONTWAIT: ::c_int = 128;
pub const MSG_NOSIGNAL: ::c_int = 1024;

pub const SCM_RIGHTS: ::c_int = 1;
pub const SCM_TIMESTAMP: ::c_int = 2;
pub const SCM_CREDS: ::c_int = 3;

pub const SO_DEBUG: ::c_int = 1;
pub const SO_ACCEPTCONN: ::c_int = 2;
pub const SO_REUSEADDR: ::c_int = 4;
pub const SO_KEEPALIVE: ::c_int = 8;
pub const SO_DONTROUTE: ::c_int = 16;
pub const SO_BROADCAST: ::c_int = 32;
pub const SO_USELOOPBACK: ::c_int = 64;
pub const SO_LINGER: ::c_int = 128;
pub const SO_OOBINLINE: ::c_int = 256;
pub const SO_REUSEPORT: ::c_int = 512;
pub const SO_SNDBUF: ::c_int = 4097;
pub const SO_RCVBUF: ::c_int = 4098;
pub const SO_SNDLOWAT: ::c_int = 4099;
pub const SO_RCVLOWAT: ::c_int = 4100;
pub const SO_SNDTIMEO: ::c_int = 4101;
pub const SO_RCVTIMEO: ::c_int = 4102;
pub const SO_ERROR: ::c_int = 4103;
pub const SO_STYLE: ::c_int = 4104;
pub const SO_TYPE: ::c_int = 4104;

pub const IPPROTO_IP: ::c_int = 0;
pub const IPPROTO_ICMP: ::c_int = 1;
pub const IPPROTO_IGMP: ::c_int = 2;
pub const IPPROTO_IPIP: ::c_int = 4;
pub const IPPROTO_TCP: ::c_int = 6;
pub const IPPROTO_EGP: ::c_int = 8;
pub const IPPROTO_PUP: ::c_int = 12;
pub const IPPROTO_UDP: ::c_int = 17;
pub const IPPROTO_IDP: ::c_int = 22;
pub const IPPROTO_TP: ::c_int = 29;
pub const IPPROTO_DCCP: ::c_int = 33;
pub const IPPROTO_IPV6: ::c_int = 41;
pub const IPPROTO_RSVP: ::c_int = 46;
pub const IPPROTO_GRE: ::c_int = 47;
pub const IPPROTO_ESP: ::c_int = 50;
pub const IPPROTO_AH: ::c_int = 51;
pub const IPPROTO_MTP: ::c_int = 92;
pub const IPPROTO_BEETPH: ::c_int = 94;
pub const IPPROTO_ENCAP: ::c_int = 98;
pub const IPPROTO_PIM: ::c_int = 103;
pub const IPPROTO_COMP: ::c_int = 108;
pub const IPPROTO_L2TP: ::c_int = 115;
pub const IPPROTO_SCTP: ::c_int = 132;
pub const IPPROTO_UDPLITE: ::c_int = 136;
pub const IPPROTO_MPLS: ::c_int = 137;
pub const IPPROTO_ETHERNET: ::c_int = 143;
pub const IPPROTO_RAW: ::c_int = 255;
pub const IPPROTO_MPTCP: ::c_int = 262;
pub const IPPROTO_MAX: ::c_int = 263;

pub const IPPROTO_HOPOPTS: ::c_int = 0;
pub const IPPROTO_ROUTING: ::c_int = 43;
pub const IPPROTO_FRAGMENT: ::c_int = 44;
pub const IPPROTO_ICMPV6: ::c_int = 58;
pub const IPPROTO_NONE: ::c_int = 59;
pub const IPPROTO_DSTOPTS: ::c_int = 60;
pub const IPPROTO_MH: ::c_int = 135;

pub const IPPORT_ECHO: in_port_t = 7;
pub const IPPORT_DISCARD: in_port_t = 9;
pub const IPPORT_SYSTAT: in_port_t = 11;
pub const IPPORT_DAYTIME: in_port_t = 13;
pub const IPPORT_NETSTAT: in_port_t = 15;
pub const IPPORT_FTP: in_port_t = 21;
pub const IPPORT_TELNET: in_port_t = 23;
pub const IPPORT_SMTP: in_port_t = 25;
pub const IPPORT_TIMESERVER: in_port_t = 37;
pub const IPPORT_NAMESERVER: in_port_t = 42;
pub const IPPORT_WHOIS: in_port_t = 43;
pub const IPPORT_MTP: in_port_t = 57;
pub const IPPORT_TFTP: in_port_t = 69;
pub const IPPORT_RJE: in_port_t = 77;
pub const IPPORT_FINGER: in_port_t = 79;
pub const IPPORT_TTYLINK: in_port_t = 87;
pub const IPPORT_SUPDUP: in_port_t = 95;
pub const IPPORT_EXECSERVER: in_port_t = 512;
pub const IPPORT_LOGINSERVER: in_port_t = 513;
pub const IPPORT_CMDSERVER: in_port_t = 514;
pub const IPPORT_EFSSERVER: in_port_t = 520;
pub const IPPORT_BIFFUDP: in_port_t = 512;
pub const IPPORT_WHOSERVER: in_port_t = 513;
pub const IPPORT_ROUTESERVER: in_port_t = 520;
pub const IPPORT_USERRESERVED: in_port_t = 5000;

pub const DT_UNKNOWN: ::c_uchar = 0;
pub const DT_FIFO: ::c_uchar = 1;
pub const DT_CHR: ::c_uchar = 2;
pub const DT_DIR: ::c_uchar = 4;
pub const DT_BLK: ::c_uchar = 6;
pub const DT_REG: ::c_uchar = 8;
pub const DT_LNK: ::c_uchar = 10;
pub const DT_SOCK: ::c_uchar = 12;
pub const DT_WHT: ::c_uchar = 14;

pub const ST_RDONLY: ::c_ulong = 1;
pub const ST_NOSUID: ::c_ulong = 2;
pub const ST_NOEXEC: ::c_ulong = 8;
pub const ST_SYNCHRONOUS: ::c_ulong = 16;
pub const ST_NOATIME: ::c_ulong = 32;
pub const ST_RELATIME: ::c_ulong = 64;

pub const RTLD_DI_LMID: ::c_int = 1;
pub const RTLD_DI_LINKMAP: ::c_int = 2;
pub const RTLD_DI_CONFIGADDR: ::c_int = 3;
pub const RTLD_DI_SERINFO: ::c_int = 4;
pub const RTLD_DI_SERINFOSIZE: ::c_int = 5;
pub const RTLD_DI_ORIGIN: ::c_int = 6;
pub const RTLD_DI_PROFILENAME: ::c_int = 7;
pub const RTLD_DI_PROFILEOUT: ::c_int = 8;
pub const RTLD_DI_TLS_MODID: ::c_int = 9;
pub const RTLD_DI_TLS_DATA: ::c_int = 10;
pub const RTLD_DI_PHDR: ::c_int = 11;
pub const RTLD_DI_MAX: ::c_int = 11;

pub const SI_ASYNCIO: ::c_int = -4;
pub const SI_MESGQ: ::c_int = -3;
pub const SI_TIMER: ::c_int = -2;
pub const SI_QUEUE: ::c_int = -1;
pub const SI_USER: ::c_int = 0;

pub const ILL_ILLOPC: ::c_int = 1;
pub const ILL_ILLOPN: ::c_int = 2;
pub const ILL_ILLADR: ::c_int = 3;
pub const ILL_ILLTRP: ::c_int = 4;
pub const ILL_PRVOPC: ::c_int = 5;
pub const ILL_PRVREG: ::c_int = 6;
pub const ILL_COPROC: ::c_int = 7;
pub const ILL_BADSTK: ::c_int = 8;

pub const FPE_INTDIV: ::c_int = 1;
pub const FPE_INTOVF: ::c_int = 2;
pub const FPE_FLTDIV: ::c_int = 3;
pub const FPE_FLTOVF: ::c_int = 4;
pub const FPE_FLTUND: ::c_int = 5;
pub const FPE_FLTRES: ::c_int = 6;
pub const FPE_FLTINV: ::c_int = 7;
pub const FPE_FLTSUB: ::c_int = 8;

pub const SEGV_MAPERR: ::c_int = 1;
pub const SEGV_ACCERR: ::c_int = 2;

pub const BUS_ADRALN: ::c_int = 1;
pub const BUS_ADRERR: ::c_int = 2;
pub const BUS_OBJERR: ::c_int = 3;

pub const TRAP_BRKPT: ::c_int = 1;
pub const TRAP_TRACE: ::c_int = 2;

pub const CLD_EXITED: ::c_int = 1;
pub const CLD_KILLED: ::c_int = 2;
pub const CLD_DUMPED: ::c_int = 3;
pub const CLD_TRAPPED: ::c_int = 4;
pub const CLD_STOPPED: ::c_int = 5;
pub const CLD_CONTINUED: ::c_int = 6;

pub const POLL_IN: ::c_int = 1;
pub const POLL_OUT: ::c_int = 2;
pub const POLL_MSG: ::c_int = 3;
pub const POLL_ERR: ::c_int = 4;
pub const POLL_PRI: ::c_int = 5;
pub const POLL_HUP: ::c_int = 6;

pub const SIGEV_SIGNAL: ::c_int = 0;
pub const SIGEV_NONE: ::c_int = 1;
pub const SIGEV_THREAD: ::c_int = 2;

pub const REG_GS: ::c_uint = 0;
pub const REG_FS: ::c_uint = 1;
pub const REG_ES: ::c_uint = 2;
pub const REG_DS: ::c_uint = 3;
pub const REG_EDI: ::c_uint = 4;
pub const REG_ESI: ::c_uint = 5;
pub const REG_EBP: ::c_uint = 6;
pub const REG_ESP: ::c_uint = 7;
pub const REG_EBX: ::c_uint = 8;
pub const REG_EDX: ::c_uint = 9;
pub const REG_ECX: ::c_uint = 10;
pub const REG_EAX: ::c_uint = 11;
pub const REG_TRAPNO: ::c_uint = 12;
pub const REG_ERR: ::c_uint = 13;
pub const REG_EIP: ::c_uint = 14;
pub const REG_CS: ::c_uint = 15;
pub const REG_EFL: ::c_uint = 16;
pub const REG_UESP: ::c_uint = 17;
pub const REG_SS: ::c_uint = 18;

pub const IOC_VOID: __ioctl_dir = 0;
pub const IOC_OUT: __ioctl_dir = 1;
pub const IOC_IN: __ioctl_dir = 2;
pub const IOC_INOUT: __ioctl_dir = 3;

pub const IOC_8: __ioctl_datum = 0;
pub const IOC_16: __ioctl_datum = 1;
pub const IOC_32: __ioctl_datum = 2;
pub const IOC_64: __ioctl_datum = 3;

pub const TCP_ESTABLISHED: ::c_uint = 1;
pub const TCP_SYN_SENT: ::c_uint = 2;
pub const TCP_SYN_RECV: ::c_uint = 3;
pub const TCP_FIN_WAIT1: ::c_uint = 4;
pub const TCP_FIN_WAIT2: ::c_uint = 5;
pub const TCP_TIME_WAIT: ::c_uint = 6;
pub const TCP_CLOSE: ::c_uint = 7;
pub const TCP_CLOSE_WAIT: ::c_uint = 8;
pub const TCP_LAST_ACK: ::c_uint = 9;
pub const TCP_LISTEN: ::c_uint = 10;
pub const TCP_CLOSING: ::c_uint = 11;

pub const TCP_CA_Open: tcp_ca_state = 0;
pub const TCP_CA_Disorder: tcp_ca_state = 1;
pub const TCP_CA_CWR: tcp_ca_state = 2;
pub const TCP_CA_Recovery: tcp_ca_state = 3;
pub const TCP_CA_Loss: tcp_ca_state = 4;

pub const TCP_NO_QUEUE: ::c_uint = 0;
pub const TCP_RECV_QUEUE: ::c_uint = 1;
pub const TCP_SEND_QUEUE: ::c_uint = 2;
pub const TCP_QUEUES_NR: ::c_uint = 3;

pub const P_ALL: idtype_t = 0;
pub const P_PID: idtype_t = 1;
pub const P_PGID: idtype_t = 2;

pub const SS_ONSTACK: ::c_int = 1;
pub const SS_DISABLE: ::c_int = 4;

pub const SHUT_RD: ::c_int = 0;
pub const SHUT_WR: ::c_int = 1;
pub const SHUT_RDWR: ::c_int = 2;
pub const PTHREAD_MUTEX_INITIALIZER: pthread_mutex_t = pthread_mutex_t {
    __lock: 0,
    __owner_id: 0,
    __cnt: 0,
    __shpid: 0,
    __type: PTHREAD_MUTEX_TIMED as ::c_int,
    __flags: 0,
    __reserved1: 0,
    __reserved2: 0,
};
pub const PTHREAD_COND_INITIALIZER: pthread_cond_t = pthread_cond_t {
    __lock: __PTHREAD_SPIN_LOCK_INITIALIZER,
    __queue: 0i64 as *mut __pthread,
    __attr: 0i64 as *mut __pthread_condattr,
    __wrefs: 0,
    __data: 0i64 as *mut ::c_void,
};
pub const PTHREAD_RWLOCK_INITIALIZER: pthread_rwlock_t = pthread_rwlock_t {
    __held: __PTHREAD_SPIN_LOCK_INITIALIZER,
    __lock: __PTHREAD_SPIN_LOCK_INITIALIZER,
    __readers: 0,
    __readerqueue: 0i64 as *mut __pthread,
    __writerqueue: 0i64 as *mut __pthread,
    __attr: 0i64 as *mut __pthread_rwlockattr,
    __data: 0i64 as *mut ::c_void,
};
pub const PTHREAD_STACK_MIN: ::size_t = 0;

// functions
f! {
    pub fn major(dev: ::dev_t) -> ::c_uint {
         ((dev >> 8) & 0xff) as ::c_uint
    }

    pub fn minor(dev: ::dev_t) -> ::c_uint {
        (dev & 0xffff00ff) as ::c_uint
    }

    pub fn FD_CLR(fd: ::c_int, set: *mut fd_set) -> () {
        let fd = fd as usize;
        let size = ::mem::size_of_val(&(*set).fds_bits[0]) * 8;
        (*set).fds_bits[fd / size] &= !(1 << (fd % size));
        return
    }

    pub fn FD_ISSET(fd: ::c_int, set: *const fd_set) -> bool {
        let fd = fd as usize;
        let size = ::mem::size_of_val(&(*set).fds_bits[0]) * 8;
        return ((*set).fds_bits[fd / size] & (1 << (fd % size))) != 0
    }

    pub fn FD_SET(fd: ::c_int, set: *mut fd_set) -> () {
        let fd = fd as usize;
        let size = ::mem::size_of_val(&(*set).fds_bits[0]) * 8;
        (*set).fds_bits[fd / size] |= 1 << (fd % size);
        return
    }

    pub fn FD_ZERO(set: *mut fd_set) -> () {
        for slot in (*set).fds_bits.iter_mut() {
            *slot = 0;
        }
    }
}

extern "C" {
    pub fn lutimes(file: *const ::c_char, times: *const ::timeval) -> ::c_int;

    pub fn futimes(fd: ::c_int, times: *const ::timeval) -> ::c_int;
    pub fn futimens(__fd: ::c_int, __times: *const ::timespec) -> ::c_int;

    pub fn utimensat(
        dirfd: ::c_int,
        path: *const ::c_char,
        times: *const ::timespec,
        flag: ::c_int,
    ) -> ::c_int;

    pub fn mkfifoat(__fd: ::c_int, __path: *const ::c_char, __mode: __mode_t) -> ::c_int;

    pub fn mknodat(
        dirfd: ::c_int,
        pathname: *const ::c_char,
        mode: ::mode_t,
        dev: dev_t,
    ) -> ::c_int;

    pub fn __libc_current_sigrtmin() -> ::c_int;

    pub fn __libc_current_sigrtmax() -> ::c_int;

    pub fn waitid(idtype: idtype_t, id: id_t, infop: *mut ::siginfo_t, options: ::c_int)
        -> ::c_int;

    pub fn sigwait(__set: *const sigset_t, __sig: *mut ::c_int) -> ::c_int;

    pub fn sigaltstack(ss: *const stack_t, oss: *mut stack_t) -> ::c_int;

    pub fn ioctl(__fd: ::c_int, __request: ::c_ulong, ...) -> ::c_int;

    pub fn pipe2(fds: *mut ::c_int, flags: ::c_int) -> ::c_int;

    pub fn dup3(oldfd: ::c_int, newfd: ::c_int, flags: ::c_int) -> ::c_int;

    pub fn pread64(fd: ::c_int, buf: *mut ::c_void, count: ::size_t, offset: off64_t) -> ::ssize_t;
    pub fn pwrite64(
        fd: ::c_int,
        buf: *const ::c_void,
        count: ::size_t,
        offset: off64_t,
    ) -> ::ssize_t;

    pub fn readv(__fd: ::c_int, __iovec: *const ::iovec, __count: ::c_int) -> ::ssize_t;
    pub fn writev(__fd: ::c_int, __iovec: *const ::iovec, __count: ::c_int) -> ::ssize_t;

    pub fn preadv(
        __fd: ::c_int,
        __iovec: *const ::iovec,
        __count: ::c_int,
        __offset: __off_t,
    ) -> ssize_t;
    pub fn pwritev(
        __fd: ::c_int,
        __iovec: *const ::iovec,
        __count: ::c_int,
        __offset: __off_t,
    ) -> ssize_t;

    pub fn preadv64(
        fd: ::c_int,
        iov: *const ::iovec,
        iovcnt: ::c_int,
        offset: ::off64_t,
    ) -> ::ssize_t;
    pub fn pwritev64(
        fd: ::c_int,
        iov: *const ::iovec,
        iovcnt: ::c_int,
        offset: ::off64_t,
    ) -> ::ssize_t;

    pub fn lseek64(__fd: ::c_int, __offset: __off64_t, __whence: ::c_int) -> __off64_t;

    pub fn lseek(__fd: ::c_int, __offset: __off_t, __whence: ::c_int) -> __off_t;

    pub fn bind(__fd: ::c_int, __addr: *const sockaddr, __len: socklen_t) -> ::c_int;

    pub fn accept4(
        fd: ::c_int,
        addr: *mut ::sockaddr,
        len: *mut ::socklen_t,
        flg: ::c_int,
    ) -> ::c_int;

    pub fn recvmsg(__fd: ::c_int, __message: *mut msghdr, __flags: ::c_int) -> ::ssize_t;

    pub fn sendmsg(__fd: ::c_int, __message: *const msghdr, __flags: ::c_int) -> ssize_t;

    pub fn recvfrom(
        socket: ::c_int,
        buf: *mut ::c_void,
        len: ::size_t,
        flags: ::c_int,
        addr: *mut ::sockaddr,
        addrlen: *mut ::socklen_t,
    ) -> ::ssize_t;

    pub fn shutdown(__fd: ::c_int, __how: ::c_int) -> ::c_int;

    pub fn sethostname(name: *const ::c_char, len: ::size_t) -> ::c_int;

    pub fn uname(buf: *mut ::utsname) -> ::c_int;

    pub fn getpwuid_r(
        uid: ::uid_t,
        pwd: *mut passwd,
        buf: *mut ::c_char,
        buflen: ::size_t,
        result: *mut *mut passwd,
    ) -> ::c_int;

    pub fn setgroups(ngroups: ::size_t, ptr: *const ::gid_t) -> ::c_int;

    pub fn pthread_create(
        native: *mut ::pthread_t,
        attr: *const ::pthread_attr_t,
        f: extern "C" fn(*mut ::c_void) -> *mut ::c_void,
        value: *mut ::c_void,
    ) -> ::c_int;
    pub fn pthread_kill(__threadid: pthread_t, __signo: ::c_int) -> ::c_int;
    pub fn __pthread_equal(__t1: __pthread_t, __t2: __pthread_t) -> ::c_int;

    pub fn pthread_getattr_np(__thr: pthread_t, __attr: *mut pthread_attr_t) -> ::c_int;

    pub fn pthread_attr_getguardsize(
        __attr: *const pthread_attr_t,
        __guardsize: *mut ::size_t,
    ) -> ::c_int;

    pub fn pthread_attr_getstack(
        __attr: *const pthread_attr_t,
        __stackaddr: *mut *mut ::c_void,
        __stacksize: *mut ::size_t,
    ) -> ::c_int;

    pub fn pthread_condattr_setclock(
        __attr: *mut pthread_condattr_t,
        __clock_id: __clockid_t,
    ) -> ::c_int;

    pub fn pthread_atfork(
        prepare: ::Option<unsafe extern "C" fn()>,
        parent: ::Option<unsafe extern "C" fn()>,
        child: ::Option<unsafe extern "C" fn()>,
    ) -> ::c_int;

    pub fn pthread_sigmask(
        __how: ::c_int,
        __newmask: *const __sigset_t,
        __oldmask: *mut __sigset_t,
    ) -> ::c_int;

    pub fn clock_getres(__clock_id: clockid_t, __res: *mut ::timespec) -> ::c_int;
    pub fn clock_gettime(__clock_id: clockid_t, __tp: *mut ::timespec) -> ::c_int;
    pub fn clock_settime(__clock_id: clockid_t, __tp: *const ::timespec) -> ::c_int;

    pub fn fstat(__fd: ::c_int, __buf: *mut stat) -> ::c_int;
    pub fn fstat64(__fd: ::c_int, __buf: *mut stat64) -> ::c_int;

    pub fn fstatat(
        __fd: ::c_int,
        __file: *const ::c_char,
        __buf: *mut stat,
        __flag: ::c_int,
    ) -> ::c_int;
    pub fn fstatat64(
        __fd: ::c_int,
        __file: *const ::c_char,
        __buf: *mut stat64,
        __flag: ::c_int,
    ) -> ::c_int;

    pub fn ftruncate(__fd: ::c_int, __length: __off_t) -> ::c_int;
    pub fn ftruncate64(__fd: ::c_int, __length: __off64_t) -> ::c_int;
    pub fn truncate64(__file: *const ::c_char, __length: __off64_t) -> ::c_int;

    pub fn lstat(__file: *const ::c_char, __buf: *mut stat) -> ::c_int;
    pub fn lstat64(__file: *const ::c_char, __buf: *mut stat64) -> ::c_int;

    pub fn statfs(path: *const ::c_char, buf: *mut statfs) -> ::c_int;
    pub fn statfs64(__file: *const ::c_char, __buf: *mut statfs64) -> ::c_int;
    pub fn fstatfs(fd: ::c_int, buf: *mut statfs) -> ::c_int;
    pub fn fstatfs64(__fildes: ::c_int, __buf: *mut statfs64) -> ::c_int;

    pub fn statvfs(__file: *const ::c_char, __buf: *mut statvfs) -> ::c_int;
    pub fn statvfs64(__file: *const ::c_char, __buf: *mut statvfs64) -> ::c_int;
    pub fn fstatvfs(__fildes: ::c_int, __buf: *mut statvfs) -> ::c_int;
    pub fn fstatvfs64(__fildes: ::c_int, __buf: *mut statvfs64) -> ::c_int;

    pub fn open(__file: *const ::c_char, __oflag: ::c_int, ...) -> ::c_int;
    pub fn open64(__file: *const ::c_char, __oflag: ::c_int, ...) -> ::c_int;

    pub fn openat(__fd: ::c_int, __file: *const ::c_char, __oflag: ::c_int, ...) -> ::c_int;
    pub fn openat64(__fd: ::c_int, __file: *const ::c_char, __oflag: ::c_int, ...) -> ::c_int;

    pub fn faccessat(
        dirfd: ::c_int,
        pathname: *const ::c_char,
        mode: ::c_int,
        flags: ::c_int,
    ) -> ::c_int;

    pub fn stat(__file: *const ::c_char, __buf: *mut stat) -> ::c_int;
    pub fn stat64(__file: *const ::c_char, __buf: *mut stat64) -> ::c_int;

    pub fn readdir(dirp: *mut ::DIR) -> *mut ::dirent;
    pub fn readdir64(dirp: *mut ::DIR) -> *mut ::dirent64;
    pub fn readdir_r(dirp: *mut ::DIR, entry: *mut ::dirent, result: *mut *mut ::dirent)
        -> ::c_int;

    pub fn dirfd(dirp: *mut ::DIR) -> ::c_int;

    #[link_name = "__xpg_strerror_r"]
    pub fn strerror_r(__errnum: ::c_int, __buf: *mut ::c_char, __buflen: ::size_t) -> ::c_int;

    pub fn __errno_location() -> *mut ::c_int;

    pub fn mmap64(
        __addr: *mut ::c_void,
        __len: size_t,
        __prot: ::c_int,
        __flags: ::c_int,
        __fd: ::c_int,
        __offset: __off64_t,
    ) -> *mut ::c_void;

    pub fn mprotect(__addr: *mut ::c_void, __len: ::size_t, __prot: ::c_int) -> ::c_int;

    pub fn msync(__addr: *mut ::c_void, __len: ::size_t, __flags: ::c_int) -> ::c_int;
    pub fn sync();
    pub fn syncfs(fd: ::c_int) -> ::c_int;
    pub fn fdatasync(fd: ::c_int) -> ::c_int;

    pub fn fallocate64(fd: ::c_int, mode: ::c_int, offset: ::off64_t, len: ::off64_t) -> ::c_int;
    pub fn posix_fallocate(fd: ::c_int, offset: ::off_t, len: ::off_t) -> ::c_int;
    pub fn posix_fallocate64(fd: ::c_int, offset: ::off64_t, len: ::off64_t) -> ::c_int;

    pub fn posix_fadvise(fd: ::c_int, offset: ::off_t, len: ::off_t, advise: ::c_int) -> ::c_int;

    pub fn posix_fadvise64(
        fd: ::c_int,
        offset: ::off64_t,
        len: ::off64_t,
        advise: ::c_int,
    ) -> ::c_int;

    pub fn madvise(__addr: *mut ::c_void, __len: ::size_t, __advice: ::c_int) -> ::c_int;

    pub fn getrlimit(resource: ::__rlimit_resource, rlim: *mut ::rlimit) -> ::c_int;
    pub fn getrlimit64(resource: ::__rlimit_resource, rlim: *mut ::rlimit64) -> ::c_int;
    pub fn setrlimit(resource: ::__rlimit_resource, rlim: *const ::rlimit) -> ::c_int;
    pub fn setrlimit64(resource: ::__rlimit_resource, rlim: *const ::rlimit64) -> ::c_int;

    pub fn getpriority(which: ::__priority_which, who: ::id_t) -> ::c_int;
    pub fn setpriority(which: ::__priority_which, who: ::id_t, prio: ::c_int) -> ::c_int;

    pub fn getrandom(__buffer: *mut ::c_void, __length: ::size_t, __flags: ::c_uint) -> ::ssize_t;
    pub fn getentropy(__buffer: *mut ::c_void, __length: ::size_t) -> ::c_int;

    pub fn backtrace(buf: *mut *mut ::c_void, sz: ::c_int) -> ::c_int;
    pub fn dl_iterate_phdr(
        callback: ::Option<
            unsafe extern "C" fn(
                info: *mut ::dl_phdr_info,
                size: ::size_t,
                data: *mut ::c_void,
            ) -> ::c_int,
        >,
        data: *mut ::c_void,
    ) -> ::c_int;
}

safe_f! {
    pub {const} fn makedev(major: ::c_uint, minor: ::c_uint) -> ::dev_t {
        let major = major as ::dev_t;
        let minor = minor as ::dev_t;
        let mut dev = 0;
        dev |= major << 8;
        dev |= minor;
        dev
    }

    pub fn SIGRTMAX() -> ::c_int {
        unsafe { __libc_current_sigrtmax() }
    }

    pub fn SIGRTMIN() -> ::c_int {
        unsafe { __libc_current_sigrtmin() }
    }

    pub {const} fn WIFSTOPPED(status: ::c_int) -> bool {
        (status & 0xff) == 0x7f
    }

    pub {const} fn WSTOPSIG(status: ::c_int) -> ::c_int {
        (status >> 8) & 0xff
    }

    pub {const} fn WIFCONTINUED(status: ::c_int) -> bool {
        status == 0xffff
    }

    pub {const} fn WIFSIGNALED(status: ::c_int) -> bool {
        ((status & 0x7f) + 1) as i8 >= 2
    }

    pub {const} fn WTERMSIG(status: ::c_int) -> ::c_int {
        status & 0x7f
    }

    pub {const} fn WIFEXITED(status: ::c_int) -> bool {
        (status & 0x7f) == 0
    }

    pub {const} fn WEXITSTATUS(status: ::c_int) -> ::c_int {
        (status >> 8) & 0xff
    }

    pub {const} fn WCOREDUMP(status: ::c_int) -> bool {
        (status & 0x80) != 0
    }

    pub {const} fn W_EXITCODE(ret: ::c_int, sig: ::c_int) -> ::c_int {
        (ret << 8) | sig
    }

    pub {const} fn W_STOPCODE(sig: ::c_int) -> ::c_int {
        (sig << 8) | 0x7f
    }

    pub {const} fn QCMD(cmd: ::c_int, type_: ::c_int) -> ::c_int {
        (cmd << 8) | (type_ & 0x00ff)
    }

    pub {const} fn IPOPT_COPIED(o: u8) -> u8 {
        o & IPOPT_COPY
    }

    pub {const} fn IPOPT_CLASS(o: u8) -> u8 {
        o & IPOPT_CLASS_MASK
    }

    pub {const} fn IPOPT_NUMBER(o: u8) -> u8 {
        o & IPOPT_NUMBER_MASK
    }

    pub {const} fn IPTOS_ECN(x: u8) -> u8 {
        x & ::IPTOS_ECN_MASK
    }
}

cfg_if! {
    if #[cfg(libc_align)] {
        mod align;
        pub use self::align::*;
    } else {
        mod no_align;
        pub use self::no_align::*;
    }
}

cfg_if! {
    if #[cfg(target_pointer_width = "64")] {
        mod b64;
        pub use self::b64::*;
    } else {
        mod b32;
        pub use self::b32::*;
    }
}
