//! This module corresponds to `mach/task_inspect.h`.

use crate::vm_types::{integer_t, natural_t};
use core::mem;

pub type task_inspect_flavor_t = natural_t;
pub const TASK_INSPECT_BASIC_COUNTS: task_inspect_flavor_t = 1;
pub const TASK_INSPECT_BASIC_COUNTS_COUNT: u32 =
    (mem::size_of::<task_inspect_basic_counts>() / mem::size_of::<natural_t>()) as u32;

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct task_inspect_basic_counts {
    pub instructions: u64,
    pub cycles: u64,
}

pub type task_inspect_basic_counts_data_t = task_inspect_basic_counts;
pub type task_inspect_basic_counts_t = *mut task_inspect_basic_counts;

pub type task_inspect_info_t = *mut integer_t;
