//! This module corresponds to `mach/semaphore.h`

use crate::clock_types::mach_timespec_t;
use crate::kern_return::kern_return_t;
use crate::mach_types::{semaphore_t, task_t};
use crate::sync_policy::sync_policy_t;
use core::ffi::c_int;

unsafe extern "C" {
    pub fn semaphore_create(
        task: task_t,
        semaphore: *mut semaphore_t,
        policy: sync_policy_t,
        value: c_int,
    ) -> kern_return_t;
    pub fn semaphore_signal(semaphore: *mut semaphore_t) -> kern_return_t;
    pub fn semaphore_wait(semaphore: *mut semaphore_t) -> kern_return_t;
    pub fn semaphore_timedwait(
        semaphore: *mut semaphore_t,
        timeout: mach_timespec_t,
    ) -> kern_return_t;
    pub fn semaphore_destroy(task: task_t, semaphore: *mut semaphore_t) -> kern_return_t;
}
