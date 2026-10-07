//! This module roughly corresponds to `mach/vm_region.h`.

use crate::boolean::boolean_t;
use crate::memory_object_types::{memory_object_offset_t, vm_object_id_t};
use crate::message::mach_msg_type_number_t;
use crate::vm_behavior::vm_behavior_t;
use crate::vm_inherit::vm_inherit_t;
use crate::vm_prot::vm_prot_t;
use crate::vm_types::{mach_vm_address_t, mach_vm_size_t, natural_t};
use core::ffi::{c_int, c_uchar, c_uint, c_ushort};
use core::mem;

pub type vm32_object_id_t = u32;

pub type vm_region_info_t = *mut c_int;
pub type vm_region_info_64_t = *mut c_int;
pub type vm_region_recurse_info_t = *mut c_int;
pub type vm_region_recurse_info_64_t = *mut c_int;
pub type vm_region_flavor_t = c_int;
pub type vm_region_info_data_t = [c_int; VM_REGION_INFO_MAX as usize];

pub type vm_region_basic_info_64_t = *mut vm_region_basic_info_64;
pub type vm_region_basic_info_data_64_t = vm_region_basic_info_64;
pub type vm_region_basic_info_t = *mut vm_region_basic_info;
pub type vm_region_basic_info_data_t = vm_region_basic_info;
pub const VM_REGION_BASIC_INFO_COUNT_64: mach_msg_type_number_t =
    (mem::size_of::<vm_region_basic_info_data_64_t>() / mem::size_of::<c_int>())
        as mach_msg_type_number_t;
pub const VM_REGION_BASIC_INFO_COUNT: mach_msg_type_number_t =
    (mem::size_of::<vm_region_basic_info_data_t>() / mem::size_of::<c_int>())
        as mach_msg_type_number_t;
pub type vm_region_extended_info_t = *mut vm_region_extended_info;
pub type vm_region_extended_info_data_t = vm_region_extended_info;
pub type vm_region_top_info_t = *mut vm_region_top_info;
pub type vm_region_top_info_data_t = vm_region_top_info;
pub const VM_REGION_EXTENDED_INFO_COUNT: mach_msg_type_number_t =
    (mem::size_of::<vm_region_extended_info_data_t>() / mem::size_of::<natural_t>())
        as mach_msg_type_number_t;
pub const VM_REGION_TOP_INFO_COUNT: mach_msg_type_number_t =
    (mem::size_of::<vm_region_top_info_data_t>() / mem::size_of::<natural_t>())
        as mach_msg_type_number_t;
pub type vm_region_submap_info_t = *mut vm_region_submap_info;
pub type vm_region_submap_info_data_t = vm_region_submap_info;
pub type vm_region_submap_info_64_t = *mut vm_region_submap_info_64;
pub type vm_region_submap_info_data_64_t = vm_region_submap_info_64;
pub type vm_region_submap_short_info_64_t = *mut vm_region_submap_short_info_64;
pub type vm_region_submap_short_info_data_64_t = vm_region_submap_short_info_64;
pub type vm_page_info_t = *mut c_int;
pub type vm_page_info_flavor_t = c_int;
pub type vm_page_info_basic_t = *mut vm_page_info_basic;
pub type vm_page_info_basic_data_t = vm_page_info_basic;
pub type mach_vm_read_entry_t = [mach_vm_read_entry; VM_MAP_ENTRY_MAX as usize];
pub const VM_REGION_SUBMAP_INFO_COUNT: mach_msg_type_number_t =
    (mem::size_of::<vm_region_submap_info_data_t>() / mem::size_of::<natural_t>())
        as mach_msg_type_number_t;
pub const VM_REGION_SUBMAP_SHORT_INFO_COUNT_64: mach_msg_type_number_t =
    (mem::size_of::<vm_region_submap_short_info_data_64_t>() / mem::size_of::<natural_t>())
        as mach_msg_type_number_t;
pub const VM_PAGE_INFO_BASIC_COUNT: mach_msg_type_number_t =
    (mem::size_of::<vm_page_info_basic_data_t>() / mem::size_of::<c_int>())
        as mach_msg_type_number_t;
pub const VM_REGION_FLAG_JIT_ENABLED: c_ushort = 0x1;
pub const VM_REGION_FLAG_TPRO_ENABLED: c_ushort = 0x2;

pub const VM_REGION_INFO_MAX: c_int = 1 << 10;
pub const VM_MAP_ENTRY_MAX: c_int = 1 << 8;

pub const VM_PAGE_INFO_BASIC: vm_page_info_flavor_t = 1;

pub const VM_REGION_BASIC_INFO_64: vm_region_flavor_t = 9;
pub const VM_REGION_BASIC_INFO: vm_region_flavor_t = 10;
pub const VM_REGION_EXTENDED_INFO: vm_region_flavor_t = 13;
pub const VM_REGION_TOP_INFO: vm_region_flavor_t = 12;

pub const SM_COW: c_uchar = 1;
pub const SM_PRIVATE: c_uchar = 2;
pub const SM_EMPTY: c_uchar = 3;
pub const SM_SHARED: c_uchar = 4;
pub const SM_TRUESHARED: c_uchar = 5;
pub const SM_PRIVATE_ALIASED: c_uchar = 6;
pub const SM_SHARED_ALIASED: c_uchar = 7;
pub const SM_LARGE_PAGE: c_uchar = 8;

#[repr(C, packed(4))]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct vm_region_basic_info_64 {
    pub protection: vm_prot_t,
    pub max_protection: vm_prot_t,
    pub inheritance: vm_inherit_t,
    pub shared: boolean_t,
    pub reserved: boolean_t,
    pub offset: memory_object_offset_t,
    pub behavior: vm_behavior_t,
    pub user_wired_count: c_ushort,
}

impl vm_region_basic_info_64 {
    pub fn count() -> mach_msg_type_number_t {
        (mem::size_of::<Self>() / mem::size_of::<c_int>()) as mach_msg_type_number_t
    }
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct vm_region_basic_info {
    pub protection: vm_prot_t,
    pub max_protection: vm_prot_t,
    pub inheritance: vm_inherit_t,
    pub shared: boolean_t,
    pub reserved: boolean_t,
    pub offset: u32,
    pub behavior: vm_behavior_t,
    pub user_wired_count: c_ushort,
}

impl vm_region_basic_info {
    pub fn count() -> mach_msg_type_number_t {
        (mem::size_of::<Self>() / mem::size_of::<c_int>()) as mach_msg_type_number_t
    }
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct vm_region_extended_info {
    pub protection: vm_prot_t,
    pub user_tag: c_uint,
    pub pages_resident: c_uint,
    pub pages_shared_now_private: c_uint,
    pub pages_swapped_out: c_uint,
    pub pages_dirtied: c_uint,
    pub ref_count: c_uint,
    pub shadow_depth: c_ushort,
    pub external_pager: c_uchar,
    pub share_mode: c_uchar,
    pub pages_reusable: c_uint,
}

impl vm_region_extended_info {
    pub fn count() -> mach_msg_type_number_t {
        (mem::size_of::<Self>() / mem::size_of::<c_int>()) as mach_msg_type_number_t
    }
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct vm_region_top_info {
    pub obj_id: c_uint,
    pub ref_count: c_uint,
    pub private_pages_resident: c_uint,
    pub shared_pages_resident: c_uint,
    pub share_mode: c_uchar,
}

impl vm_region_top_info {
    pub fn count() -> mach_msg_type_number_t {
        (mem::size_of::<Self>() / mem::size_of::<c_int>()) as mach_msg_type_number_t
    }
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct vm_region_submap_info {
    pub protection: vm_prot_t,
    pub max_protection: vm_prot_t,
    pub inheritance: vm_inherit_t,
    pub offset: u32,
    pub user_tag: c_uint,
    pub pages_resident: c_uint,
    pub pages_shared_now_private: c_uint,
    pub pages_swapped_out: c_uint,
    pub pages_dirtied: c_uint,
    pub ref_count: c_uint,
    pub shadow_depth: c_ushort,
    pub external_pager: c_uchar,
    pub share_mode: c_uchar,
    pub is_submap: boolean_t,
    pub behavior: vm_behavior_t,
    pub object_id: vm32_object_id_t,
    pub user_wired_count: c_ushort,
}

impl vm_region_submap_info {
    pub fn count() -> mach_msg_type_number_t {
        (mem::size_of::<Self>() / mem::size_of::<c_int>()) as mach_msg_type_number_t
    }
}

#[repr(C, packed(4))]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct vm_region_submap_info_64 {
    pub protection: vm_prot_t,
    pub max_protection: vm_prot_t,
    pub inheritance: vm_inherit_t,
    pub offset: memory_object_offset_t,
    pub user_tag: c_uint,
    pub pages_resident: c_uint,
    pub pages_shared_now_private: c_uint,
    pub pages_swapped_out: c_uint,
    pub pages_dirtied: c_uint,
    pub ref_count: c_uint,
    pub shadow_depth: c_ushort,
    pub external_pager: c_uchar,
    pub share_mode: c_uchar,
    pub is_submap: boolean_t,
    pub behavior: vm_behavior_t,
    pub object_id: vm32_object_id_t,
    pub user_wired_count: c_ushort,
    pub flags: c_ushort,
    pub pages_reusable: c_uint,
    pub object_id_full: vm_object_id_t,
}

impl vm_region_submap_info_64 {
    pub fn count() -> mach_msg_type_number_t {
        (mem::size_of::<Self>() / mem::size_of::<c_int>()) as mach_msg_type_number_t
    }
}

#[repr(C, packed(4))]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct vm_region_submap_short_info_64 {
    pub protection: vm_prot_t,
    pub max_protection: vm_prot_t,
    pub inheritance: vm_inherit_t,
    pub offset: memory_object_offset_t,
    pub user_tag: c_uint,
    pub ref_count: c_uint,
    pub shadow_depth: c_ushort,
    pub external_pager: c_uchar,
    pub share_mode: c_uchar,
    pub is_submap: boolean_t,
    pub behavior: vm_behavior_t,
    pub object_id: vm32_object_id_t,
    pub user_wired_count: c_ushort,
    pub flags: c_ushort,
}

impl vm_region_submap_short_info_64 {
    pub fn count() -> mach_msg_type_number_t {
        (mem::size_of::<Self>() / mem::size_of::<c_int>()) as mach_msg_type_number_t
    }
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct vm_page_info_basic {
    pub disposition: c_int,
    pub ref_count: c_int,
    pub object_id: vm_object_id_t,
    pub offset: memory_object_offset_t,
    pub depth: c_int,
    pub __pad: c_int,
}

impl vm_page_info_basic {
    pub fn count() -> mach_msg_type_number_t {
        (mem::size_of::<Self>() / mem::size_of::<c_int>()) as mach_msg_type_number_t
    }
}

#[repr(C, packed(4))]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct mach_vm_read_entry {
    pub address: mach_vm_address_t,
    pub size: mach_vm_size_t,
}
