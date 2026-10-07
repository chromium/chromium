//! This module corresponds to `mach_debug/ipc_info.h`.

use crate::port::{mach_port_name_t, mach_port_type_t, mach_port_urefs_t};
use crate::vm_types::{integer_t, natural_t};

pub type ipc_info_object_type_t = natural_t;

pub const IPC_OTYPE_NONE: ipc_info_object_type_t = 0;
pub const IPC_OTYPE_THREAD_CONTROL: ipc_info_object_type_t = 1;
pub const IPC_OTYPE_TASK_CONTROL: ipc_info_object_type_t = 2;
pub const IPC_OTYPE_HOST: ipc_info_object_type_t = 3;
pub const IPC_OTYPE_HOST_PRIV: ipc_info_object_type_t = 4;
pub const IPC_OTYPE_PROCESSOR: ipc_info_object_type_t = 5;
pub const IPC_OTYPE_PROCESSOR_SET: ipc_info_object_type_t = 6;
pub const IPC_OTYPE_PROCESSOR_SET_NAME: ipc_info_object_type_t = 7;
pub const IPC_OTYPE_TIMER: ipc_info_object_type_t = 8;
pub const IPC_OTYPE_PORT_SUBST_ONCE: ipc_info_object_type_t = 9;
pub const IPC_OTYPE_MIG: ipc_info_object_type_t = 10;
pub const IPC_OTYPE_MEMORY_OBJECT: ipc_info_object_type_t = 11;
pub const IPC_OTYPE_XMM_PAGER: ipc_info_object_type_t = 12;
pub const IPC_OTYPE_XMM_KERNEL: ipc_info_object_type_t = 13;
pub const IPC_OTYPE_XMM_REPLY: ipc_info_object_type_t = 14;
pub const IPC_OTYPE_UND_REPLY: ipc_info_object_type_t = 15;
pub const IPC_OTYPE_HOST_NOTIFY: ipc_info_object_type_t = 16;
pub const IPC_OTYPE_HOST_SECURITY: ipc_info_object_type_t = 17;
pub const IPC_OTYPE_LEDGER: ipc_info_object_type_t = 18;
pub const IPC_OTYPE_MAIN_DEVICE: ipc_info_object_type_t = 19;
pub const IPC_OTYPE_TASK_NAME: ipc_info_object_type_t = 20;
pub const IPC_OTYPE_SUBSYSTEM: ipc_info_object_type_t = 21;
pub const IPC_OTYPE_IO_DONE_QUEUE: ipc_info_object_type_t = 22;
pub const IPC_OTYPE_SEMAPHORE: ipc_info_object_type_t = 23;
pub const IPC_OTYPE_LOCK_SET: ipc_info_object_type_t = 24;
pub const IPC_OTYPE_CLOCK: ipc_info_object_type_t = 25;
pub const IPC_OTYPE_CLOCK_CTRL: ipc_info_object_type_t = 26;
pub const IPC_OTYPE_IOKIT_IDENT: ipc_info_object_type_t = 27;
pub const IPC_OTYPE_NAMED_ENTRY: ipc_info_object_type_t = 28;
pub const IPC_OTYPE_IOKIT_CONNECT: ipc_info_object_type_t = 29;
pub const IPC_OTYPE_IOKIT_OBJECT: ipc_info_object_type_t = 30;
pub const IPC_OTYPE_UPL: ipc_info_object_type_t = 31;
pub const IPC_OTYPE_MEM_OBJ_CONTROL: ipc_info_object_type_t = 32;
pub const IPC_OTYPE_AU_SESSIONPORT: ipc_info_object_type_t = 33;
pub const IPC_OTYPE_FILEPORT: ipc_info_object_type_t = 34;
pub const IPC_OTYPE_LABELH: ipc_info_object_type_t = 35;
pub const IPC_OTYPE_TASK_RESUME: ipc_info_object_type_t = 36;
pub const IPC_OTYPE_VOUCHER: ipc_info_object_type_t = 37;
pub const IPC_OTYPE_VOUCHER_ATTR_CONTROL: ipc_info_object_type_t = 38;
pub const IPC_OTYPE_WORK_INTERVAL: ipc_info_object_type_t = 39;
pub const IPC_OTYPE_UX_HANDLER: ipc_info_object_type_t = 40;
pub const IPC_OTYPE_UEXT_OBJECT: ipc_info_object_type_t = 41;
pub const IPC_OTYPE_ARCADE_REG: ipc_info_object_type_t = 42;
pub const IPC_OTYPE_EVENTLINK: ipc_info_object_type_t = 43;
pub const IPC_OTYPE_TASK_INSPECT: ipc_info_object_type_t = 44;
pub const IPC_OTYPE_TASK_READ: ipc_info_object_type_t = 45;
pub const IPC_OTYPE_THREAD_INSPECT: ipc_info_object_type_t = 46;
pub const IPC_OTYPE_THREAD_READ: ipc_info_object_type_t = 47;
pub const IPC_OTYPE_SUID_CRED: ipc_info_object_type_t = 48;
pub const IPC_OTYPE_HYPERVISOR: ipc_info_object_type_t = 49;
pub const IPC_OTYPE_TASK_ID_TOKEN: ipc_info_object_type_t = 50;
pub const IPC_OTYPE_TASK_FATAL: ipc_info_object_type_t = 51;
pub const IPC_OTYPE_KCDATA: ipc_info_object_type_t = 52;
pub const IPC_OTYPE_EXCLAVES_RESOURCE: ipc_info_object_type_t = 53;
pub const IPC_OTYPE_UNKNOWN: ipc_info_object_type_t = !0;

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct ipc_info_space {
    pub iis_genno_mask: natural_t,
    pub iis_table_size: natural_t,
    pub iis_table_next: natural_t,
    pub iis_tree_size: natural_t,
    pub iis_tree_small: natural_t,
    pub iis_tree_hash: natural_t,
}

pub type ipc_info_space_t = ipc_info_space;

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct ipc_info_space_basic {
    pub iisb_genno_mask: natural_t,
    pub iisb_table_size: natural_t,
    pub iisb_table_next: natural_t,
    pub iisb_table_inuse: natural_t,
    pub iisb_reserved: [natural_t; 2],
}

pub type ipc_info_space_basic_t = ipc_info_space_basic;

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct ipc_info_name {
    pub iin_name: mach_port_name_t,
    pub iin_collision: integer_t,
    pub iin_type: mach_port_type_t,
    pub iin_urefs: mach_port_urefs_t,
    pub iin_object: natural_t,
    pub iin_next: natural_t,
    pub iin_hash: natural_t,
}

pub type ipc_info_name_t = ipc_info_name;
pub type ipc_info_name_array_t = *mut ipc_info_name_t;

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct ipc_info_tree_name {
    pub iitn_name: ipc_info_name_t,
    pub iitn_lchild: mach_port_name_t,
    pub iitn_rchild: mach_port_name_t,
}

pub type ipc_info_tree_name_t = ipc_info_tree_name;
pub type ipc_info_tree_name_array_t = *mut ipc_info_tree_name_t;

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct ipc_info_port {
    pub iip_port_object: natural_t,
    pub iip_receiver_object: natural_t,
}

pub type ipc_info_port_t = ipc_info_port;
pub type exception_port_info_array_t = *mut ipc_info_port_t;
pub type exception_handler_info_array_t = *mut ipc_info_port_t;
