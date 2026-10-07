//! This module corresponds to `mach/task_special_ports.h`.

use crate::kern_return::kern_return_t;
use crate::mach_types::task_t;
use crate::port::mach_port_t;
use core::ffi::c_int;

pub type task_special_port_t = c_int;

pub const TASK_KERNEL_PORT: task_special_port_t = 1;
pub const TASK_HOST_PORT: task_special_port_t = 2;
pub const TASK_NAME_PORT: task_special_port_t = 3;
pub const TASK_BOOTSTRAP_PORT: task_special_port_t = 4;
pub const TASK_INSPECT_PORT: task_special_port_t = 5;
pub const TASK_READ_PORT: task_special_port_t = 6;
pub const TASK_ACCESS_PORT: task_special_port_t = 9;
pub const TASK_DEBUG_CONTROL_PORT: task_special_port_t = 10;
pub const TASK_RESOURCE_NOTIFY_PORT: task_special_port_t = 11;

pub const TASK_MAX_SPECIAL_PORT: task_special_port_t = TASK_RESOURCE_NOTIFY_PORT;

#[inline]
#[allow(clippy::missing_safety_doc)]
pub unsafe fn task_get_kernel_port(task: task_t, port: *mut mach_port_t) -> kern_return_t {
    unsafe { crate::task::task_get_special_port(task, TASK_KERNEL_PORT, port) }
}

#[inline]
#[allow(clippy::missing_safety_doc)]
pub unsafe fn task_set_kernel_port(task: task_t, port: mach_port_t) -> kern_return_t {
    unsafe { crate::task::task_set_special_port(task, TASK_KERNEL_PORT, port) }
}

#[inline]
#[allow(clippy::missing_safety_doc)]
pub unsafe fn task_get_host_port(task: task_t, port: *mut mach_port_t) -> kern_return_t {
    unsafe { crate::task::task_get_special_port(task, TASK_HOST_PORT, port) }
}

#[inline]
#[allow(clippy::missing_safety_doc)]
pub unsafe fn task_set_host_port(task: task_t, port: mach_port_t) -> kern_return_t {
    unsafe { crate::task::task_set_special_port(task, TASK_HOST_PORT, port) }
}

#[inline]
#[allow(clippy::missing_safety_doc)]
pub unsafe fn task_get_bootstrap_port(task: task_t, port: *mut mach_port_t) -> kern_return_t {
    unsafe { crate::task::task_get_special_port(task, TASK_BOOTSTRAP_PORT, port) }
}

#[inline]
#[allow(clippy::missing_safety_doc)]
pub unsafe fn task_set_bootstrap_port(task: task_t, port: mach_port_t) -> kern_return_t {
    unsafe { crate::task::task_set_special_port(task, TASK_BOOTSTRAP_PORT, port) }
}

#[inline]
#[allow(clippy::missing_safety_doc)]
pub unsafe fn task_get_debug_control_port(task: task_t, port: *mut mach_port_t) -> kern_return_t {
    unsafe { crate::task::task_get_special_port(task, TASK_DEBUG_CONTROL_PORT, port) }
}

#[inline]
#[allow(clippy::missing_safety_doc)]
pub unsafe fn task_set_task_debug_control_port(task: task_t, port: mach_port_t) -> kern_return_t {
    unsafe { crate::task::task_set_special_port(task, TASK_DEBUG_CONTROL_PORT, port) }
}

#[inline]
#[allow(clippy::missing_safety_doc)]
pub unsafe fn task_get_task_access_port(task: task_t, port: *mut mach_port_t) -> kern_return_t {
    unsafe { crate::task::task_get_special_port(task, TASK_ACCESS_PORT, port) }
}

#[inline]
#[allow(clippy::missing_safety_doc)]
pub unsafe fn task_set_task_access_port(task: task_t, port: mach_port_t) -> kern_return_t {
    unsafe { crate::task::task_set_special_port(task, TASK_ACCESS_PORT, port) }
}
