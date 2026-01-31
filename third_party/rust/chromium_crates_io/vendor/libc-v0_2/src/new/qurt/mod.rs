//! QuRT (Qualcomm Real-Time OS) bindings
//!
//! QuRT is Qualcomm's real-time operating system for Hexagon DSP architectures.
//! Headers available via the
//! Hexagon SDK: https://softwarecenter.qualcomm.com/catalog/item/Hexagon_SDK

use crate::prelude::*;

// Basic C types for QRT
pub type intptr_t = isize;
pub type uintptr_t = usize;
pub type ptrdiff_t = isize;
pub type size_t = uintptr_t;
pub type ssize_t = intptr_t;

// Process and system types
pub type pid_t = c_int;
pub type uid_t = c_uint;
pub type gid_t = c_uint;

// Time types
pub type time_t = c_longlong;
pub type suseconds_t = c_long;
pub type useconds_t = c_ulong;
pub type clockid_t = c_int;
pub type timer_t = *mut c_void;

// File system types
pub type dev_t = c_ulong;
pub type ino_t = c_ulong;
pub type mode_t = c_uint;
pub type nlink_t = c_uint;
pub type off_t = c_long;
pub type blkcnt_t = c_long;
pub type blksize_t = c_long;

// Thread types based on QuRT pthread implementation
pub type pthread_t = c_uint;
pub type pthread_key_t = c_int;
pub type pthread_once_t = c_int;
pub type pthread_mutex_t = c_uint;
pub type pthread_mutexattr_t = c_uint;
pub type pthread_cond_t = c_uint;
pub type pthread_condattr_t = c_uint;
pub type pthread_attr_t = c_uint;
pub type pthread_rwlock_t = c_uint;
pub type pthread_rwlockattr_t = c_uint;
pub type pthread_spinlock_t = c_uint;
pub type pthread_barrier_t = c_uint;
pub type pthread_barrierattr_t = c_uint;

// Network types
pub type socklen_t = c_uint;
pub type sa_family_t = c_ushort;
pub type in_addr_t = c_uint;
pub type in_port_t = c_ushort;

// File descriptor types
pub type fd_set = c_ulong;

// Standard C library types
extern_ty! {
    pub enum FILE {}
}
pub type fpos_t = c_long;
pub type clock_t = c_long;

// POSIX semaphore types
pub type sem_t = c_uint;

// Message queue types
pub type mqd_t = c_int;

// Additional file system types
pub type nfds_t = c_ulong;

// Signal types
pub type sigset_t = c_ulong;

// Variadic argument list type
pub type va_list = *mut c_char;

// Additional missing types
pub type c_schar = i8;

// Division result types
s! {
    pub struct div_t {
        pub quot: c_int,
        pub rem: c_int,
    }

    pub struct ldiv_t {
        pub quot: c_long,
        pub rem: c_long,
    }

    pub struct lldiv_t {
        pub quot: c_longlong,
        pub rem: c_longlong,
    }

    pub struct stat {
        pub st_dev: dev_t,
        pub st_ino: ino_t,
        pub st_mode: mode_t,
        pub st_nlink: nlink_t,
        pub st_uid: uid_t,
        pub st_gid: gid_t,
        pub st_rdev: dev_t,
        pub st_size: off_t,
        pub st_blksize: blksize_t,
        pub st_blocks: blkcnt_t,
        pub st_atime: time_t,
        pub st_mtime: time_t,
        pub st_ctime: time_t,
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

    pub struct timespec {
        pub tv_sec: time_t,
        pub tv_nsec: c_long,
    }

    pub struct timeval {
        pub tv_sec: time_t,
        pub tv_usec: suseconds_t,
    }

    pub struct itimerspec {
        pub it_interval: timespec,
        pub it_value: timespec,
    }

    pub struct sigevent {
        pub sigev_notify: c_int,
        pub sigev_signo: c_int,
        pub sigev_value: c_int,
    }
}

pub(crate) mod errno;
pub(crate) mod fcntl;
pub(crate) mod limits;
pub(crate) mod pthread;
pub(crate) mod semaphore;
pub(crate) mod signal;
pub(crate) mod stdio;
pub(crate) mod stdlib;
pub(crate) mod sys;
pub(crate) mod time;
pub(crate) mod unistd;
