//! This module corresponds to `mach_debug/zone_info.h`.

use crate::vm_types::{integer_t, vm_size_t};
use core::ffi::c_char;

pub const ZONE_NAME_MAX_LEN: usize = 80;
pub const MACH_ZONE_NAME_MAX_LEN: usize = 80;
pub const MACH_MEMORY_INFO_NAME_MAX_LEN: usize = 80;
pub const MAX_ZTRACE_DEPTH: usize = 15;

pub const ZOP_ALLOC: u32 = 1;
pub const ZOP_FREE: u32 = 0;

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct zone_name {
    pub zn_name: [c_char; ZONE_NAME_MAX_LEN],
}

pub type zone_name_t = zone_name;
pub type zone_name_array_t = *mut zone_name_t;

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct zone_info {
    pub zi_count: integer_t,
    pub zi_cur_size: vm_size_t,
    pub zi_max_size: vm_size_t,
    pub zi_elem_size: vm_size_t,
    pub zi_alloc_size: vm_size_t,
    pub zi_pageable: integer_t,
    pub zi_sleepable: integer_t,
    pub zi_exhaustible: integer_t,
    pub zi_collectable: integer_t,
}

pub type zone_info_t = zone_info;
pub type zone_info_array_t = *mut zone_info_t;

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct mach_zone_name {
    pub mzn_name: [c_char; ZONE_NAME_MAX_LEN],
}

pub type mach_zone_name_t = mach_zone_name;
pub type mach_zone_name_array_t = *mut mach_zone_name_t;

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct mach_zone_info_data {
    pub mzi_count: u64,
    pub mzi_cur_size: u64,
    pub mzi_max_size: u64,
    pub mzi_elem_size: u64,
    pub mzi_alloc_size: u64,
    pub mzi_sum_size: u64,
    pub mzi_exhaustible: u64,
    pub mzi_collectable: u64,
}

pub type mach_zone_info_t = mach_zone_info_data;
pub type mach_zone_info_array_t = *mut mach_zone_info_t;

#[inline]
pub const fn get_mzi_collectable_bytes(val: u64) -> u64 {
    val >> 1
}

#[inline]
pub const fn get_mzi_collectable_flag(val: u64) -> u64 {
    val & 1
}

#[inline]
pub fn set_mzi_collectable_bytes(val: &mut u64, size: u64) {
    *val = (*val & 1) | (size << 1);
}

#[inline]
pub fn set_mzi_collectable_flag(val: &mut u64, flag: bool) {
    if flag {
        *val |= 1;
    } else {
        *val &= !1;
    }
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct task_zone_info_data {
    pub tzi_count: u64,
    pub tzi_cur_size: u64,
    pub tzi_max_size: u64,
    pub tzi_elem_size: u64,
    pub tzi_alloc_size: u64,
    pub tzi_sum_size: u64,
    pub tzi_exhaustible: u64,
    pub tzi_collectable: u64,
    pub tzi_caller_acct: u64,
    pub tzi_task_alloc: u64,
    pub tzi_task_free: u64,
}

pub type task_zone_info_t = task_zone_info_data;
pub type task_zone_info_array_t = *mut task_zone_info_t;

#[repr(C)]
#[derive(Copy, Clone, Debug)]
#[allow(non_snake_case)]
pub struct mach_memory_info {
    pub flags: u64,
    pub site: u64,
    pub size: u64,
    pub free: u64,
    pub largest: u64,
    pub collectable_bytes: u64,
    pub mapped: u64,
    pub peak: u64,
    pub tag: u16,
    pub zone: u16,
    pub _resvA: [u16; 2],
    pub _resv: [u64; 3],
    pub name: [c_char; MACH_MEMORY_INFO_NAME_MAX_LEN],
}

pub type mach_memory_info_t = mach_memory_info;
pub type mach_memory_info_array_t = *mut mach_memory_info_t;

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct zone_btrecord {
    pub ref_count: u32,
    pub operation_type: u32,
    pub bt: [u64; MAX_ZTRACE_DEPTH],
}

pub type zone_btrecord_t = zone_btrecord;
pub type zone_btrecord_array_t = *mut zone_btrecord_t;
