#![allow(non_camel_case_types)]
#![allow(non_upper_case_globals)]
#![deny(missing_debug_implementations)]
#![deny(missing_copy_implementations)]
#![allow(
    clippy::module_name_repetitions,
    clippy::cast_sign_loss,
    clippy::cast_possible_truncation,
    clippy::trivially_copy_pass_by_ref
)]
#![no_std]

#[cfg(not(target_vendor = "apple"))]
compile_error!("mach requires macOS, iOS, tvOS, watchOS or visionOS");

#[allow(unused_imports)]
use core::{clone, cmp, default, fmt, hash, marker, mem, option};

pub mod boolean;
pub mod bootstrap;
pub mod clock;
pub mod clock_priv;
pub mod clock_reply;
pub mod clock_types;
pub mod dyld;
pub mod dyld_kernel;
pub mod error;
pub mod exc;
pub mod exception_types;
pub mod kern_return;
pub mod loader;
pub mod mach_debug;
pub mod mach_init;
pub mod mach_port;
pub mod mach_time;
pub mod mach_types;
pub mod mach_voucher_types;
pub mod memory_object_types;
pub mod message;
pub mod mig;
pub mod ndr;
pub mod notify;
pub mod policy;
pub mod port;
pub mod semaphore;
pub mod structs;
pub mod sync_policy;
pub mod task;
pub mod task_info;
pub mod task_inspect;
pub mod task_special_ports;
pub mod thread_act;
pub mod thread_policy;
pub mod thread_special_ports;
pub mod thread_state;
pub mod thread_status;
pub mod thread_switch;
pub mod time_value;
pub mod traps;
pub mod vm;
pub mod vm_attributes;
pub mod vm_behavior;
pub mod vm_inherit;
pub mod vm_page_size;
pub mod vm_prot;
pub mod vm_purgable;
pub mod vm_region;
pub mod vm_statistics;
pub mod vm_sync;
pub mod vm_types;
