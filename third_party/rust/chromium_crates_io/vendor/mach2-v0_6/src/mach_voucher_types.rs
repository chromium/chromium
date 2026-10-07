//! This module corresponds to `mach/mach_voucher_types.h`.

use crate::message::mach_msg_type_number_t;
use crate::port::{mach_port_name_t, mach_port_t};

pub type mach_voucher_t = mach_port_t;
pub const MACH_VOUCHER_NULL: mach_voucher_t = 0;

pub type mach_voucher_name_t = mach_port_name_t;
pub const MACH_VOUCHER_NAME_NULL: mach_voucher_name_t = 0;

pub type mach_voucher_name_array_t = *mut mach_voucher_name_t;
pub const MACH_VOUCHER_NAME_ARRAY_NULL: mach_voucher_name_array_t = core::ptr::null_mut();

pub type ipc_voucher_t = mach_voucher_t;
pub const IPC_VOUCHER_NULL: ipc_voucher_t = 0;

pub type mach_voucher_selector_t = u32;
pub const MACH_VOUCHER_SELECTOR_CURRENT: mach_voucher_selector_t = 0;
pub const MACH_VOUCHER_SELECTOR_EFFECTIVE: mach_voucher_selector_t = 1;

pub type mach_voucher_attr_key_t = u32;
pub type mach_voucher_attr_key_array_t = *mut mach_voucher_attr_key_t;

pub const MACH_VOUCHER_ATTR_KEY_ALL: mach_voucher_attr_key_t = !0;
pub const MACH_VOUCHER_ATTR_KEY_NONE: mach_voucher_attr_key_t = 0;
pub const MACH_VOUCHER_ATTR_KEY_ATM: mach_voucher_attr_key_t = 1;
pub const MACH_VOUCHER_ATTR_KEY_IMPORTANCE: mach_voucher_attr_key_t = 2;
pub const MACH_VOUCHER_ATTR_KEY_BANK: mach_voucher_attr_key_t = 3;
pub const MACH_VOUCHER_ATTR_KEY_PTHPRIORITY: mach_voucher_attr_key_t = 4;
pub const MACH_VOUCHER_ATTR_KEY_USER_DATA: mach_voucher_attr_key_t = 7;
pub const MACH_VOUCHER_ATTR_KEY_BITS: mach_voucher_attr_key_t = MACH_VOUCHER_ATTR_KEY_USER_DATA;
pub const MACH_VOUCHER_ATTR_KEY_TEST: mach_voucher_attr_key_t = 8;
pub const MACH_VOUCHER_ATTR_KEY_NUM_WELL_KNOWN: mach_voucher_attr_key_t =
    MACH_VOUCHER_ATTR_KEY_TEST;

pub type mach_voucher_attr_content_t = *mut u8;
pub type mach_voucher_attr_content_size_t = u32;

pub type mach_voucher_attr_command_t = u32;
pub type mach_voucher_attr_recipe_command_t = u32;
pub type mach_voucher_attr_recipe_command_array_t = *mut mach_voucher_attr_recipe_command_t;

pub const MACH_VOUCHER_ATTR_NOOP: mach_voucher_attr_recipe_command_t = 0;
pub const MACH_VOUCHER_ATTR_COPY: mach_voucher_attr_recipe_command_t = 1;
pub const MACH_VOUCHER_ATTR_REMOVE: mach_voucher_attr_recipe_command_t = 2;
pub const MACH_VOUCHER_ATTR_SET_VALUE_HANDLE: mach_voucher_attr_recipe_command_t = 3;
pub const MACH_VOUCHER_ATTR_AUTO_REDEEM: mach_voucher_attr_recipe_command_t = 4;
pub const MACH_VOUCHER_ATTR_SEND_PREPROCESS: mach_voucher_attr_recipe_command_t = 5;
pub const MACH_VOUCHER_ATTR_REDEEM: mach_voucher_attr_recipe_command_t = 10;
pub const MACH_VOUCHER_ATTR_IMPORTANCE_SELF: mach_voucher_attr_recipe_command_t = 200;
pub const MACH_VOUCHER_ATTR_USER_DATA_STORE: mach_voucher_attr_recipe_command_t = 211;
pub const MACH_VOUCHER_ATTR_BITS_STORE: mach_voucher_attr_recipe_command_t =
    MACH_VOUCHER_ATTR_USER_DATA_STORE;
pub const MACH_VOUCHER_ATTR_TEST_STORE: mach_voucher_attr_recipe_command_t =
    MACH_VOUCHER_ATTR_USER_DATA_STORE;

pub type mach_voucher_attr_recipe_size_t = mach_msg_type_number_t;

pub type mach_voucher_attr_raw_recipe_t = *mut u8;
pub type mach_voucher_attr_raw_recipe_array_t = mach_voucher_attr_raw_recipe_t;
pub type mach_voucher_attr_raw_recipe_size_t = mach_msg_type_number_t;
pub type mach_voucher_attr_raw_recipe_array_size_t = mach_msg_type_number_t;

pub const MACH_VOUCHER_ATTR_MAX_RAW_RECIPE_ARRAY_SIZE: mach_msg_type_number_t = 5120;
pub const MACH_VOUCHER_TRAP_STACK_LIMIT: mach_msg_type_number_t = 256;

pub type mach_voucher_attr_manager_t = mach_port_t;
pub const MACH_VOUCHER_ATTR_MANAGER_NULL: mach_voucher_attr_manager_t = 0;

pub type mach_voucher_attr_control_t = mach_port_t;
pub const MACH_VOUCHER_ATTR_CONTROL_NULL: mach_voucher_attr_control_t = 0;

pub type ipc_voucher_attr_manager_t = mach_port_t;
pub type ipc_voucher_attr_control_t = mach_port_t;
pub const IPC_VOUCHER_ATTR_MANAGER_NULL: ipc_voucher_attr_manager_t = 0;
pub const IPC_VOUCHER_ATTR_CONTROL_NULL: ipc_voucher_attr_control_t = 0;

pub type mach_voucher_attr_value_handle_t = u64;
pub type mach_voucher_attr_value_handle_array_t = *mut mach_voucher_attr_value_handle_t;
pub type mach_voucher_attr_value_handle_array_size_t = mach_msg_type_number_t;

pub const MACH_VOUCHER_ATTR_VALUE_MAX_NESTED: mach_voucher_attr_value_handle_array_size_t = 4;

pub type mach_voucher_attr_value_reference_t = u32;
pub type mach_voucher_attr_value_flags_t = u32;
pub const MACH_VOUCHER_ATTR_VALUE_FLAGS_NONE: mach_voucher_attr_value_flags_t = 0;
pub const MACH_VOUCHER_ATTR_VALUE_FLAGS_PERSIST: mach_voucher_attr_value_flags_t = 1;

pub type mach_voucher_attr_control_flags_t = u32;
pub const MACH_VOUCHER_ATTR_CONTROL_FLAGS_NONE: mach_voucher_attr_control_flags_t = 0;

pub const MACH_VOUCHER_IMPORTANCE_ATTR_ADD_EXTERNAL: u32 = 1;
pub const MACH_VOUCHER_IMPORTANCE_ATTR_DROP_EXTERNAL: u32 = 2;
pub type mach_voucher_attr_importance_refs = u32;

pub const MACH_ACTIVITY_ID_COUNT_MAX: u32 = 16;
