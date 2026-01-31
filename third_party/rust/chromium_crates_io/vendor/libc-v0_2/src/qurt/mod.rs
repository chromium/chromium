//! Interface to QuRT (Qualcomm Real-Time OS) C library

use crate::prelude::*;

// Forward declarations for opaque types
#[derive(Debug)]
pub enum DIR {}
impl Copy for DIR {}
impl Clone for DIR {
    fn clone(&self) -> DIR {
        *self
    }
}

// Network types
pub type socklen_t = c_uint;
pub type in_addr_t = u32;

// Error type
pub type errno_t = c_int;

// Resource limit type
pub type rlim_t = c_ulong;

// Terminal types
pub type speed_t = c_uint;
pub type tcflag_t = c_uint;

// File descriptor set type for select()
pub type fd_set = c_ulong;

// POSIX semaphore types
pub type sem_t = c_uint;

// Message queue types
pub type mqd_t = c_int;

// Additional file system types
pub type nfds_t = c_ulong;

// Architecture-specific modules
cfg_if! {
    if #[cfg(target_arch = "hexagon")] {
        mod hexagon;
        pub use self::hexagon::*;
    } else {
        // Add other architectures as needed
    }
}

// Structures based on QuRT headers
s! {
    pub struct sigval {
        pub sival_int: c_int,
        pub sival_ptr: *mut c_void,
    }

    pub struct sigevent {
        pub sigev_notify: c_int,
        pub sigev_signo: c_int,
        pub sigev_value: sigval,
        pub sigev_notify_function: Option<extern "C" fn(sigval)>,
        pub sigev_notify_attributes: *mut pthread_attr_t,
    }

    pub struct siginfo_t {
        pub si_signo: c_int,
        pub si_code: c_int,
        pub si_value: sigval,
    }

    pub struct sigaction {
        pub sa_handler: Option<extern "C" fn(c_int)>,
        pub sa_mask: sigset_t,
        pub sa_flags: c_int,
        pub sa_sigaction: Option<extern "C" fn(c_int, *mut siginfo_t, *mut c_void)>,
    }

    pub struct termios {
        pub c_iflag: tcflag_t,
        pub c_oflag: tcflag_t,
        pub c_cflag: tcflag_t,
        pub c_lflag: tcflag_t,
        pub c_cc: [c_uchar; 32],
        pub c_ispeed: speed_t,
        pub c_ospeed: speed_t,
    }

    pub struct dirent {
        pub d_ino: ino_t,
        pub d_type: c_uchar,
        pub d_name: [c_char; 256],
    }

    pub struct tm {
        pub tm_sec: c_int,
        pub tm_min: c_int,
        pub tm_hour: c_int,
        pub tm_mday: c_int,
        pub tm_mon: c_int,
        pub tm_year: c_int,
        pub tm_wday: c_int,
        pub tm_yday: c_int,
        pub tm_isdst: c_int,
    }

    pub struct sched_param {
        pub sched_priority: c_int,
    }

    pub struct iovec {
        pub iov_base: *mut c_void,
        pub iov_len: size_t,
    }

    pub struct rlimit {
        pub rlim_cur: rlim_t,
        pub rlim_max: rlim_t,
    }

    pub struct rusage {
        pub ru_utime: timeval,
        pub ru_stime: timeval,
        pub ru_maxrss: c_long,
        pub ru_ixrss: c_long,
        pub ru_idrss: c_long,
        pub ru_isrss: c_long,
        pub ru_minflt: c_long,
        pub ru_majflt: c_long,
        pub ru_nswap: c_long,
        pub ru_inblock: c_long,
        pub ru_oublock: c_long,
        pub ru_msgsnd: c_long,
        pub ru_msgrcv: c_long,
        pub ru_nsignals: c_long,
        pub ru_nvcsw: c_long,
        pub ru_nivcsw: c_long,
    }

    pub struct flock {
        pub l_type: c_short,
        pub l_whence: c_short,
        pub l_start: off_t,
        pub l_len: off_t,
        pub l_pid: pid_t,
    }
}

// Memory mapping constants (from sys/mman.h)
pub const PROT_NONE: c_int = 0x00;
pub const PROT_READ: c_int = 0x01;
pub const PROT_WRITE: c_int = 0x02;
pub const PROT_EXEC: c_int = 0x04;

pub const MAP_SHARED: c_int = 0x0001;
pub const MAP_PRIVATE: c_int = 0x0002;
pub const MAP_FIXED: c_int = 0x0010;
pub const MAP_ANON: c_int = 0x1000;
pub const MAP_ANONYMOUS: c_int = MAP_ANON;
pub const MAP_FILE: c_int = 0x0000;
pub const MAP_RENAME: c_int = 0x0020;
pub const MAP_NORESERVE: c_int = 0x0040;
pub const MAP_INHERIT: c_int = 0x0080;
pub const MAP_HASSEMAPHORE: c_int = 0x0200;
pub const MAP_TRYFIXED: c_int = 0x0400;
pub const MAP_WIRED: c_int = 0x0800;

pub const MAP_FAILED: *mut c_void = !0 as *mut c_void;

pub const MS_ASYNC: c_int = 0x01;
pub const MS_INVALIDATE: c_int = 0x02;
pub const MS_SYNC: c_int = 0x04;

pub const MCL_CURRENT: c_int = 0x01;
pub const MCL_FUTURE: c_int = 0x02;

// Dynamic linking constants (from dlfcn.h)
pub const RTLD_LAZY: c_int = 1;
pub const RTLD_NOW: c_int = 2;
pub const RTLD_GLOBAL: c_int = 0x100;
pub const RTLD_LOCAL: c_int = 0x200;

// Semaphore constants
pub const SEM_FAILED: *mut sem_t = 0 as *mut sem_t;

// Additional constants
pub const PTHREAD_NAME_LEN: c_int = 16;
pub const PTHREAD_MAX_THREADS: c_uint = 512;
pub const PTHREAD_MIN_STACKSIZE: c_int = 512;
pub const PTHREAD_MAX_STACKSIZE: c_int = 1048576;
pub const PTHREAD_DEFAULT_STACKSIZE: c_int = 16384;
pub const PTHREAD_DEFAULT_PRIORITY: c_int = 1;
pub const PTHREAD_SPINLOCK_UNLOCKED: c_int = 0;
pub const PTHREAD_SPINLOCK_LOCKED: c_int = 1;
pub const TIME_CONV_SCLK_FREQ: c_int = 19200000;
pub const CLOCK_MONOTONIC_RAW: clockid_t = 4;
pub const CLOCK_REALTIME_COARSE: clockid_t = 5;
pub const CLOCK_MONOTONIC_COARSE: clockid_t = 6;
pub const CLOCK_BOOTTIME: clockid_t = 7;
pub const L_tmpnam: c_uint = 260;
pub const TMP_MAX: c_uint = 25;
pub const FOPEN_MAX: c_uint = 20;
pub const AT_FDCWD: c_int = -100;
pub const AT_EACCESS: c_int = 0x200;
pub const AT_SYMLINK_NOFOLLOW: c_int = 0x100;
pub const AT_SYMLINK_FOLLOW: c_int = 0x400;
pub const AT_REMOVEDIR: c_int = 0x200;
pub const EOK: c_int = 0;
pub const POSIX_MSG: c_int = 7;
pub const POSIX_NOTIF: c_int = 8;
pub const SIGRTMIN: c_int = 10;
pub const SIGRTMAX: c_int = 32;
pub const SIGEV_NONE: c_int = 0;
pub const SIGEV_SIGNAL: c_int = 1;
pub const SIGEV_THREAD: c_int = 2;
pub const SA_SIGINFO: c_int = 1;

// Function declarations for QuRT POSIX API
extern "C" {
    // Signal functions
    pub fn sigwait(set: *const sigset_t, sig: *mut c_int) -> c_int;
    pub fn _sigaction(sig: c_int, act: *const sigaction, oact: *mut sigaction) -> c_int;
    pub fn sigtimedwait(
        set: *const sigset_t,
        info: *mut siginfo_t,
        timeout: *const timespec,
    ) -> c_int;

    // Additional pthread functions
    pub fn pthread_attr_getstack(
        attr: *const pthread_attr_t,
        stackaddr: *mut *mut c_void,
        stacksize: *mut size_t,
    ) -> c_int;
    pub fn pthread_attr_setstack(
        attr: *mut pthread_attr_t,
        stackaddr: *mut c_void,
        stacksize: size_t,
    ) -> c_int;
    pub fn pthread_mutexattr_gettype(attr: *const pthread_mutexattr_t, type_: *mut c_int) -> c_int;
    pub fn pthread_mutexattr_settype(attr: *mut pthread_mutexattr_t, type_: c_int) -> c_int;

    // Additional time functions
    pub fn clock_getcpuclockid(pid: pid_t, clock_id: *mut clockid_t) -> c_int;

    // POSIX semaphore functions
    pub fn sem_open(name: *const c_char, oflag: c_int, ...) -> *mut sem_t;
    pub fn sem_close(sem: *mut sem_t) -> c_int;
    pub fn sem_unlink(name: *const c_char) -> c_int;

    // Additional stdlib functions
    pub fn aligned_alloc(alignment: size_t, size: size_t) -> *mut c_void;

    // String functions
    pub fn strlen(s: *const c_char) -> size_t;
    pub fn strcpy(dest: *mut c_char, src: *const c_char) -> *mut c_char;
    pub fn strncpy(dest: *mut c_char, src: *const c_char, n: size_t) -> *mut c_char;
    pub fn strcat(dest: *mut c_char, src: *const c_char) -> *mut c_char;
    pub fn strncat(dest: *mut c_char, src: *const c_char, n: size_t) -> *mut c_char;
    pub fn strcmp(s1: *const c_char, s2: *const c_char) -> c_int;
    pub fn strncmp(s1: *const c_char, s2: *const c_char, n: size_t) -> c_int;
    pub fn strcoll(s1: *const c_char, s2: *const c_char) -> c_int;
    pub fn strxfrm(dest: *mut c_char, src: *const c_char, n: size_t) -> size_t;
    pub fn strchr(s: *const c_char, c: c_int) -> *mut c_char;
    pub fn strrchr(s: *const c_char, c: c_int) -> *mut c_char;
    pub fn strspn(s: *const c_char, accept: *const c_char) -> size_t;
    pub fn strcspn(s: *const c_char, reject: *const c_char) -> size_t;
    pub fn strpbrk(s: *const c_char, accept: *const c_char) -> *mut c_char;
    pub fn strstr(haystack: *const c_char, needle: *const c_char) -> *mut c_char;
    pub fn strtok(s: *mut c_char, delim: *const c_char) -> *mut c_char;
    pub fn strerror(errnum: c_int) -> *mut c_char;
    pub fn memchr(s: *const c_void, c: c_int, n: size_t) -> *mut c_void;
    pub fn memcmp(s1: *const c_void, s2: *const c_void, n: size_t) -> c_int;
    pub fn memcpy(dest: *mut c_void, src: *const c_void, n: size_t) -> *mut c_void;
    pub fn memmove(dest: *mut c_void, src: *const c_void, n: size_t) -> *mut c_void;
    pub fn memset(s: *mut c_void, c: c_int, n: size_t) -> *mut c_void;

    // Additional unistd functions
    pub fn fork() -> pid_t;
    pub fn execve(
        filename: *const c_char,
        argv: *const *const c_char,
        envp: *const *const c_char,
    ) -> c_int;

    // Directory functions
    pub fn opendir(name: *const c_char) -> *mut DIR;
    pub fn closedir(dirp: *mut DIR) -> c_int;
    pub fn readdir(dirp: *mut DIR) -> *mut dirent;
    pub fn rewinddir(dirp: *mut DIR);
    pub fn telldir(dirp: *mut DIR) -> c_long;
    pub fn seekdir(dirp: *mut DIR, loc: c_long);

    // Memory mapping functions
    pub fn mmap(
        addr: *mut c_void,
        len: size_t,
        prot: c_int,
        flags: c_int,
        fd: c_int,
        offset: off_t,
    ) -> *mut c_void;
    pub fn munmap(addr: *mut c_void, len: size_t) -> c_int;
    pub fn mprotect(addr: *mut c_void, len: size_t, prot: c_int) -> c_int;
    pub fn mlock(addr: *const c_void, len: size_t) -> c_int;
    pub fn munlock(addr: *const c_void, len: size_t) -> c_int;
    pub fn mlockall(flags: c_int) -> c_int;
    pub fn munlockall() -> c_int;
    pub fn msync(addr: *mut c_void, len: size_t, flags: c_int) -> c_int;

    // Dynamic linking functions
    pub fn dlopen(filename: *const c_char, flag: c_int) -> *mut c_void;
    pub fn dlclose(handle: *mut c_void) -> c_int;
    pub fn dlsym(handle: *mut c_void, symbol: *const c_char) -> *mut c_void;
    pub fn dlerror() -> *mut c_char;

    // Character classification functions
    pub fn isalnum(c: c_int) -> c_int;
    pub fn isalpha(c: c_int) -> c_int;
    pub fn iscntrl(c: c_int) -> c_int;
    pub fn isdigit(c: c_int) -> c_int;
    pub fn isgraph(c: c_int) -> c_int;
    pub fn islower(c: c_int) -> c_int;
    pub fn isprint(c: c_int) -> c_int;
    pub fn ispunct(c: c_int) -> c_int;
    pub fn isspace(c: c_int) -> c_int;
    pub fn isupper(c: c_int) -> c_int;
    pub fn isxdigit(c: c_int) -> c_int;
    pub fn tolower(c: c_int) -> c_int;
    pub fn toupper(c: c_int) -> c_int;
}

// Re-export common prelude items
pub use crate::*;
