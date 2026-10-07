//! This module corresponds to `mach/policy.h`.

use crate::boolean::boolean_t;
use crate::message::mach_msg_type_number_t;
use crate::vm_types::integer_t;
use core::ffi::c_int;
use core::mem;

pub type policy_t = c_int;
pub type policy_info_t = *mut integer_t;
pub type policy_base_t = *mut integer_t;
pub type policy_limit_t = *mut integer_t;

pub const POLICY_NULL: policy_t = 0;
pub const POLICY_TIMESHARE: policy_t = 1;
pub const POLICY_RR: policy_t = 2;
pub const POLICY_FIFO: policy_t = 4;

pub const POLICYCLASS_FIXEDPRI: policy_t = POLICY_RR | POLICY_FIFO;

#[inline]
pub const fn invalid_policy(policy: policy_t) -> bool {
    !(policy == POLICY_TIMESHARE || policy == POLICY_RR || policy == POLICY_FIFO)
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq)]
pub struct policy_timeshare_base {
    pub base_priority: integer_t,
}

pub type policy_timeshare_base_t = *mut policy_timeshare_base;
pub type policy_timeshare_base_data_t = policy_timeshare_base;

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq)]
pub struct policy_timeshare_limit {
    pub max_priority: integer_t,
}

pub type policy_timeshare_limit_t = *mut policy_timeshare_limit;
pub type policy_timeshare_limit_data_t = policy_timeshare_limit;

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq)]
pub struct policy_timeshare_info {
    pub max_priority: integer_t,
    pub base_priority: integer_t,
    pub cur_priority: integer_t,
    pub depressed: boolean_t,
    pub depress_priority: integer_t,
}

pub type policy_timeshare_info_t = *mut policy_timeshare_info;
pub type policy_timeshare_info_data_t = policy_timeshare_info;

pub const POLICY_TIMESHARE_BASE_COUNT: mach_msg_type_number_t =
    (mem::size_of::<policy_timeshare_base_data_t>() / mem::size_of::<integer_t>())
        as mach_msg_type_number_t;
pub const POLICY_TIMESHARE_LIMIT_COUNT: mach_msg_type_number_t =
    (mem::size_of::<policy_timeshare_limit_data_t>() / mem::size_of::<integer_t>())
        as mach_msg_type_number_t;
pub const POLICY_TIMESHARE_INFO_COUNT: mach_msg_type_number_t =
    (mem::size_of::<policy_timeshare_info_data_t>() / mem::size_of::<integer_t>())
        as mach_msg_type_number_t;

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq)]
pub struct policy_rr_base {
    pub base_priority: integer_t,
    pub quantum: integer_t,
}

pub type policy_rr_base_t = *mut policy_rr_base;
pub type policy_rr_base_data_t = policy_rr_base;

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq)]
pub struct policy_rr_limit {
    pub max_priority: integer_t,
}

pub type policy_rr_limit_t = *mut policy_rr_limit;
pub type policy_rr_limit_data_t = policy_rr_limit;

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq)]
pub struct policy_rr_info {
    pub max_priority: integer_t,
    pub base_priority: integer_t,
    pub quantum: integer_t,
    pub depressed: boolean_t,
    pub depress_priority: integer_t,
}

pub type policy_rr_info_t = *mut policy_rr_info;
pub type policy_rr_info_data_t = policy_rr_info;

pub const POLICY_RR_BASE_COUNT: mach_msg_type_number_t = (mem::size_of::<policy_rr_base_data_t>()
    / mem::size_of::<integer_t>())
    as mach_msg_type_number_t;
pub const POLICY_RR_LIMIT_COUNT: mach_msg_type_number_t = (mem::size_of::<policy_rr_limit_data_t>()
    / mem::size_of::<integer_t>())
    as mach_msg_type_number_t;
pub const POLICY_RR_INFO_COUNT: mach_msg_type_number_t = (mem::size_of::<policy_rr_info_data_t>()
    / mem::size_of::<integer_t>())
    as mach_msg_type_number_t;

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq)]
pub struct policy_fifo_base {
    pub base_priority: integer_t,
}

pub type policy_fifo_base_t = *mut policy_fifo_base;
pub type policy_fifo_base_data_t = policy_fifo_base;

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq)]
pub struct policy_fifo_limit {
    pub max_priority: integer_t,
}

pub type policy_fifo_limit_t = *mut policy_fifo_limit;
pub type policy_fifo_limit_data_t = policy_fifo_limit;

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq)]
pub struct policy_fifo_info {
    pub max_priority: integer_t,
    pub base_priority: integer_t,
    pub depressed: boolean_t,
    pub depress_priority: integer_t,
}

pub type policy_fifo_info_t = *mut policy_fifo_info;
pub type policy_fifo_info_data_t = policy_fifo_info;

pub const POLICY_FIFO_BASE_COUNT: mach_msg_type_number_t =
    (mem::size_of::<policy_fifo_base_data_t>() / mem::size_of::<integer_t>())
        as mach_msg_type_number_t;
pub const POLICY_FIFO_LIMIT_COUNT: mach_msg_type_number_t =
    (mem::size_of::<policy_fifo_limit_data_t>() / mem::size_of::<integer_t>())
        as mach_msg_type_number_t;
pub const POLICY_FIFO_INFO_COUNT: mach_msg_type_number_t =
    (mem::size_of::<policy_fifo_info_data_t>() / mem::size_of::<integer_t>())
        as mach_msg_type_number_t;
