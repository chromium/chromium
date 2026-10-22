//! HelenOS fibril synchronization primitives
//!
//! * Header file: <https://github.com/HelenOS/helenos/tree/master/uspace/lib/c/include/fibril_synch.h>

pub use crate::fibril::*;
use crate::prelude::*;
use crate::{
    errno_t,
    list_initialize,
    list_t,
};

s! {
    pub struct fibril_mutex_t {
        pub oi: fibril_owner_info_t,
        pub counter: c_int,
        pub waiters: list_t,
    }

    pub struct fibril_rwlock_t {
        pub oi: fibril_owner_info_t,
        pub writers: c_uint,
        pub readers: c_uint,
        pub waiters: list_t,
    }

    pub struct fibril_condvar_t {
        pub waiters: list_t,
    }
}

f! {
    pub safe fn fibril_mutex_initialize(fm: &mut fibril_mutex_t) -> () {
        fm.oi.owned_by = ptr::null_mut();
        fm.counter = 1;
        list_initialize(&mut fm.waiters);
    }
}

extern "C" {
    pub fn fibril_mutex_lock(mutex: *mut fibril_mutex_t);
    pub fn fibril_mutex_unlock(mutex: *mut fibril_mutex_t);
    pub fn fibril_mutex_trylock(mutex: *mut fibril_mutex_t) -> bool;
    pub fn fibril_mutex_is_locked(mutex: *mut fibril_mutex_t) -> bool;

    pub fn fibril_rwlock_initialize(rwlock: *mut fibril_rwlock_t);
    pub fn fibril_rwlock_read_lock(rwlock: *mut fibril_rwlock_t);
    pub fn fibril_rwlock_write_lock(rwlock: *mut fibril_rwlock_t);
    pub fn fibril_rwlock_read_unlock(rwlock: *mut fibril_rwlock_t);
    pub fn fibril_rwlock_write_unlock(rwlock: *mut fibril_rwlock_t);
    pub fn fibril_rwlock_is_read_locked(rwlock: *mut fibril_rwlock_t) -> bool;
    pub fn fibril_rwlock_is_write_locked(rwlock: *mut fibril_rwlock_t) -> bool;
    pub fn fibril_rwlock_is_locked(rwlock: *mut fibril_rwlock_t) -> bool;

    pub fn fibril_condvar_initialize(condvar: *mut fibril_condvar_t);
    pub fn fibril_condvar_wait(condvar: *mut fibril_condvar_t, mutex: *mut fibril_mutex_t);
    pub fn fibril_condvar_wait_timeout(
        condvar: *mut fibril_condvar_t,
        mutex: *mut fibril_mutex_t,
        timeout: usec_t,
    ) -> errno_t;
    pub fn fibril_condvar_signal(condvar: *mut fibril_condvar_t);
    pub fn fibril_condvar_broadcast(condvar: *mut fibril_condvar_t);
}
