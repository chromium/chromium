//! This module roughly corresponds to `mach/memory_object_types.h`.

use crate::vm_types::natural_t;
use core::ffi::c_ulonglong;

pub type memory_object_offset_t = c_ulonglong;
pub type memory_object_size_t = c_ulonglong;
pub type memory_object_cluster_size_t = natural_t;
pub type memory_object_fault_info_t = *mut natural_t;
pub type vm_object_id_t = c_ulonglong;
