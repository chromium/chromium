//! This module corresponds to `mach/task.h`.

use crate::boolean::boolean_t;
use crate::dyld_kernel::{
    dyld_kernel_image_info_array_t, dyld_kernel_image_info_t, dyld_kernel_process_info_t,
};
use crate::exception_types::{
    exception_behavior_array_t, exception_behavior_t, exception_flavor_array_t,
    exception_mask_array_t, exception_mask_t,
};
use crate::kern_return::kern_return_t;
use crate::mach_debug::ipc_info::exception_handler_info_array_t;
use crate::mach_debug::zone_info::{mach_zone_name_array_t, task_zone_info_array_t};
use crate::mach_types::{
    exception_handler_array_t, kcdata_object_t, ledger_array_t, lock_set_t, processor_set_name_t,
    processor_set_t, task_id_token_t, task_inspect_t, task_name_t, task_read_t,
    task_suspension_token_t, task_t, thread_act_array_t, thread_act_t,
};
use crate::mach_voucher_types::{ipc_voucher_t, mach_voucher_selector_t};
use crate::message::{
    mach_msg_body_t, mach_msg_header_t, mach_msg_ool_descriptor_t, mach_msg_ool_ports_descriptor_t,
    mach_msg_port_descriptor_t, mach_msg_type_number_t,
};
use crate::ndr::NDR_record_t;
use crate::policy::{policy_base_t, policy_limit_t, policy_t};
use crate::port::{mach_port_array_t, mach_port_name_t, mach_port_t};
use crate::task_info::{
    task_corpse_forking_behavior_t, task_exc_guard_behavior_t, task_flavor_t, task_info_t,
    task_purgable_info_t,
};
use crate::task_inspect::{task_inspect_flavor_t, task_inspect_info_t};
use crate::task_special_ports::task_special_port_t;
use crate::thread_status::{thread_state_flavor_t, thread_state_t};
use crate::vm_types::{integer_t, mach_vm_address_t, mach_vm_size_t, natural_t, vm_address_t};
use core::ffi::{c_int, c_uint};

pub const task_MSG_COUNT: c_uint = 66;

pub type task_policy_flavor_t = natural_t;
pub type task_policy_t = *mut integer_t;
pub type emulation_vector_t = *mut integer_t;

unsafe extern "C" {
    pub fn task_create(
        target_task: task_t,
        ledgers: ledger_array_t,
        ledgersCnt: mach_msg_type_number_t,
        inherit_memory: boolean_t,
        child_task: *mut task_t,
    ) -> kern_return_t;
    pub fn task_terminate(target_task: task_t) -> kern_return_t;
    pub fn task_threads(
        target_task: task_t,
        act_list: *mut thread_act_array_t,
        act_list_cnt: *mut mach_msg_type_number_t,
    ) -> kern_return_t;
    pub fn mach_ports_register(
        target_task: task_t,
        init_port_set: mach_port_array_t,
        init_port_setCnt: mach_msg_type_number_t,
    ) -> kern_return_t;
    pub fn mach_ports_lookup(
        target_task: task_t,
        init_port_set: *mut mach_port_array_t,
        init_port_setCnt: *mut mach_msg_type_number_t,
    ) -> kern_return_t;
    pub fn task_info(
        target_task: task_name_t,
        flavor: task_flavor_t,
        task_info_out: task_info_t,
        task_info_outCnt: *mut mach_msg_type_number_t,
    ) -> kern_return_t;
    pub fn task_set_info(
        target_task: task_t,
        flavor: task_flavor_t,
        task_info_in: task_info_t,
        task_info_inCnt: mach_msg_type_number_t,
    ) -> kern_return_t;
    pub fn task_suspend(target_task: task_t) -> kern_return_t;
    pub fn task_resume(target_task: task_t) -> kern_return_t;
    pub fn task_get_special_port(
        task: task_t,
        which_port: task_special_port_t,
        special_port: *mut mach_port_t,
    ) -> kern_return_t;
    pub fn task_set_special_port(
        task: task_t,
        which_port: task_special_port_t,
        special_port: mach_port_t,
    ) -> kern_return_t;
    pub fn thread_create(parent_task: task_t, child_act: *mut thread_act_t) -> kern_return_t;
    pub fn thread_create_running(
        parent_task: task_t,
        flavor: thread_state_flavor_t,
        new_state: thread_state_t,
        new_stateCnt: mach_msg_type_number_t,
        child_act: *mut thread_act_t,
    ) -> kern_return_t;
    pub fn task_set_exception_ports(
        task: task_t,
        exception_mask: exception_mask_t,
        new_port: mach_port_t,
        behavior: exception_behavior_t,
        new_flavor: thread_state_flavor_t,
    ) -> kern_return_t;
    pub fn task_get_exception_ports(
        task: task_t,
        exception_mask: exception_mask_t,
        masks: exception_mask_array_t,
        masksCnt: *mut mach_msg_type_number_t,
        old_handlers: exception_handler_array_t,
        old_behaviors: exception_behavior_array_t,
        old_flavors: exception_flavor_array_t,
    ) -> kern_return_t;
    pub fn task_swap_exception_ports(
        task: task_t,
        exception_mask: exception_mask_t,
        new_port: mach_port_t,
        behavior: exception_behavior_t,
        new_flavor: thread_state_flavor_t,
        masks: exception_mask_array_t,
        masksCnt: *mut mach_msg_type_number_t,
        old_handlers: exception_handler_array_t,
        old_behaviors: exception_behavior_array_t,
        old_flavors: exception_flavor_array_t,
    ) -> kern_return_t;
    pub fn lock_set_create(
        task: task_t,
        new_lock_set: *mut lock_set_t,
        n_ulocks: c_int,
        policy: c_int,
    ) -> kern_return_t;
    pub fn lock_set_destroy(task: task_t, lock_set: lock_set_t) -> kern_return_t;
    pub fn task_policy_set(
        task: task_t,
        flavor: task_policy_flavor_t,
        policy_info: task_policy_t,
        policy_infoCnt: mach_msg_type_number_t,
    ) -> kern_return_t;
    pub fn task_policy_get(
        task: task_t,
        flavor: task_policy_flavor_t,
        policy_info: task_policy_t,
        policy_infoCnt: *mut mach_msg_type_number_t,
        get_default: *mut boolean_t,
    ) -> kern_return_t;
    pub fn task_sample(task: task_t, reply: mach_port_t) -> kern_return_t;
    pub fn task_policy(
        task: task_t,
        policy: policy_t,
        base: policy_base_t,
        baseCnt: mach_msg_type_number_t,
        set_limit: boolean_t,
        change: boolean_t,
    ) -> kern_return_t;
    pub fn task_set_emulation(
        target_port: task_t,
        routine_entry_pt: vm_address_t,
        routine_number: c_int,
    ) -> kern_return_t;
    pub fn task_get_emulation_vector(
        task: task_t,
        vector_start: *mut c_int,
        emulation_vector: *mut emulation_vector_t,
        emulation_vectorCnt: *mut mach_msg_type_number_t,
    ) -> kern_return_t;
    pub fn task_set_emulation_vector(
        task: task_t,
        vector_start: c_int,
        emulation_vector: emulation_vector_t,
        emulation_vectorCnt: mach_msg_type_number_t,
    ) -> kern_return_t;
    pub fn task_set_ras_pc(
        target_task: task_t,
        basepc: vm_address_t,
        boundspc: vm_address_t,
    ) -> kern_return_t;
    pub fn task_assign(
        task: task_t,
        new_set: processor_set_t,
        assign_threads: boolean_t,
    ) -> kern_return_t;
    pub fn task_assign_default(task: task_t, assign_threads: boolean_t) -> kern_return_t;
    pub fn task_get_assignment(
        task: task_t,
        assigned_set: *mut processor_set_name_t,
    ) -> kern_return_t;
    pub fn task_set_policy(
        task: task_t,
        pset: processor_set_t,
        policy: policy_t,
        base: policy_base_t,
        baseCnt: mach_msg_type_number_t,
        limit: policy_limit_t,
        limitCnt: mach_msg_type_number_t,
        change: boolean_t,
    ) -> kern_return_t;
    pub fn task_get_state(
        task: task_t,
        flavor: thread_state_flavor_t,
        old_state: thread_state_t,
        old_stateCnt: *mut mach_msg_type_number_t,
    ) -> kern_return_t;
    pub fn task_set_state(
        task: task_t,
        flavor: thread_state_flavor_t,
        new_state: thread_state_t,
        new_stateCnt: mach_msg_type_number_t,
    ) -> kern_return_t;
    pub fn task_zone_info(
        target_task: task_inspect_t,
        names: *mut mach_zone_name_array_t,
        namesCnt: *mut mach_msg_type_number_t,
        info: *mut task_zone_info_array_t,
        infoCnt: *mut mach_msg_type_number_t,
    ) -> kern_return_t;
    pub fn task_set_phys_footprint_limit(
        task: task_t,
        new_limit: c_int,
        old_limit: *mut c_int,
    ) -> kern_return_t;
    pub fn task_suspend2(
        target_task: task_read_t,
        suspend_token: *mut task_suspension_token_t,
    ) -> kern_return_t;
    pub fn task_resume2(suspend_token: task_suspension_token_t) -> kern_return_t;
    pub fn task_purgable_info(
        task: task_inspect_t,
        stats: *mut task_purgable_info_t,
    ) -> kern_return_t;
    pub fn task_get_mach_voucher(
        task: task_read_t,
        which: mach_voucher_selector_t,
        voucher: *mut ipc_voucher_t,
    ) -> kern_return_t;
    pub fn task_set_mach_voucher(task: task_t, voucher: ipc_voucher_t) -> kern_return_t;
    pub fn task_swap_mach_voucher(
        task: task_t,
        new_voucher: ipc_voucher_t,
        old_voucher: *mut ipc_voucher_t,
    ) -> kern_return_t;
    pub fn task_generate_corpse(
        task: task_read_t,
        corpse_task_port: *mut mach_port_t,
    ) -> kern_return_t;
    pub fn task_map_corpse_info(
        task: task_t,
        corspe_task: task_read_t,
        kcd_addr_begin: *mut vm_address_t,
        kcd_size: *mut u32,
    ) -> kern_return_t;
    pub fn task_register_dyld_image_infos(
        task: task_t,
        dyld_images: dyld_kernel_image_info_array_t,
        dyld_imagesCnt: mach_msg_type_number_t,
    ) -> kern_return_t;
    pub fn task_unregister_dyld_image_infos(
        task: task_t,
        dyld_images: dyld_kernel_image_info_array_t,
        dyld_imagesCnt: mach_msg_type_number_t,
    ) -> kern_return_t;
    pub fn task_get_dyld_image_infos(
        task: task_read_t,
        dyld_images: *mut dyld_kernel_image_info_array_t,
        dyld_imagesCnt: *mut mach_msg_type_number_t,
    ) -> kern_return_t;
    pub fn task_register_dyld_shared_cache_image_info(
        task: task_t,
        dyld_cache_image: dyld_kernel_image_info_t,
        no_cache: boolean_t,
        private_cache: boolean_t,
    ) -> kern_return_t;
    pub fn task_register_dyld_set_dyld_state(task: task_t, dyld_state: u8) -> kern_return_t;
    pub fn task_register_dyld_get_process_state(
        task: task_t,
        dyld_process_state: *mut dyld_kernel_process_info_t,
    ) -> kern_return_t;
    pub fn task_map_corpse_info_64(
        task: task_t,
        corspe_task: task_read_t,
        kcd_addr_begin: *mut mach_vm_address_t,
        kcd_size: *mut mach_vm_size_t,
    ) -> kern_return_t;
    pub fn task_inspect(
        task: task_inspect_t,
        flavor: task_inspect_flavor_t,
        info_out: task_inspect_info_t,
        info_outCnt: *mut mach_msg_type_number_t,
    ) -> kern_return_t;
    pub fn task_get_exc_guard_behavior(
        task: task_inspect_t,
        behavior: *mut task_exc_guard_behavior_t,
    ) -> kern_return_t;
    pub fn task_set_exc_guard_behavior(
        task: task_t,
        behavior: task_exc_guard_behavior_t,
    ) -> kern_return_t;
    pub fn task_dyld_process_info_notify_register(
        target_task: task_read_t,
        notify: mach_port_t,
    ) -> kern_return_t;
    pub fn task_create_identity_token(task: task_t, token: *mut task_id_token_t) -> kern_return_t;
    pub fn task_identity_token_get_task_port(
        token: task_id_token_t,
        flavor: task_flavor_t,
        task_port: *mut mach_port_t,
    ) -> kern_return_t;
    pub fn task_dyld_process_info_notify_deregister(
        target_task: task_read_t,
        notify: mach_port_name_t,
    ) -> kern_return_t;
    pub fn task_get_exception_ports_info(
        port: mach_port_t,
        exception_mask: exception_mask_t,
        masks: exception_mask_array_t,
        masksCnt: *mut mach_msg_type_number_t,
        old_handlers_info: exception_handler_info_array_t,
        old_behaviors: exception_behavior_array_t,
        old_flavors: exception_flavor_array_t,
    ) -> kern_return_t;
    pub fn task_test_sync_upcall(task: task_t, port: mach_port_t) -> kern_return_t;
    pub fn task_set_corpse_forking_behavior(
        task: task_t,
        behavior: task_corpse_forking_behavior_t,
    ) -> kern_return_t;
    pub fn task_test_async_upcall_propagation(
        task: task_t,
        port: mach_port_t,
        qos: c_int,
        iotier: c_int,
    ) -> kern_return_t;
    pub fn task_map_kcdata_object_64(
        task: task_t,
        kcdata_object: kcdata_object_t,
        kcd_addr_begin: *mut mach_vm_address_t,
        kcd_size: *mut mach_vm_size_t,
    ) -> kern_return_t;
    pub fn task_register_hardened_exception_handler(
        task: task_t,
        signed_pc_key: u32,
        exceptions_allowed: exception_mask_t,
        behaviors_allowed: exception_behavior_t,
        flavors_allowed: thread_state_flavor_t,
        new_exception_port: mach_port_t,
    ) -> kern_return_t;
}

#[repr(C, packed(4))]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_create_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub ledgers: mach_msg_ool_ports_descriptor_t,
    pub NDR: NDR_record_t,
    pub ledgersCnt: mach_msg_type_number_t,
    pub inherit_memory: boolean_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_terminate_t {
    pub Head: mach_msg_header_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_threads_t {
    pub Head: mach_msg_header_t,
}

#[repr(C, packed(4))]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__mach_ports_register_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub init_port_set: mach_msg_ool_ports_descriptor_t,
    pub NDR: NDR_record_t,
    pub init_port_setCnt: mach_msg_type_number_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__mach_ports_lookup_t {
    pub Head: mach_msg_header_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_info_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub flavor: task_flavor_t,
    pub task_info_outCnt: mach_msg_type_number_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_set_info_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub flavor: task_flavor_t,
    pub task_info_inCnt: mach_msg_type_number_t,
    pub task_info_in: [integer_t; 94],
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_suspend_t {
    pub Head: mach_msg_header_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_resume_t {
    pub Head: mach_msg_header_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_get_special_port_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub which_port: task_special_port_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_set_special_port_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub special_port: mach_msg_port_descriptor_t,
    pub NDR: NDR_record_t,
    pub which_port: task_special_port_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__thread_create_t {
    pub Head: mach_msg_header_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__thread_create_running_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub flavor: thread_state_flavor_t,
    pub new_stateCnt: mach_msg_type_number_t,
    pub new_state: [natural_t; 1296],
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_set_exception_ports_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub new_port: mach_msg_port_descriptor_t,
    pub NDR: NDR_record_t,
    pub exception_mask: exception_mask_t,
    pub behavior: exception_behavior_t,
    pub new_flavor: thread_state_flavor_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_get_exception_ports_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub exception_mask: exception_mask_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_swap_exception_ports_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub new_port: mach_msg_port_descriptor_t,
    pub NDR: NDR_record_t,
    pub exception_mask: exception_mask_t,
    pub behavior: exception_behavior_t,
    pub new_flavor: thread_state_flavor_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__lock_set_create_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub n_ulocks: c_int,
    pub policy: c_int,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__lock_set_destroy_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub lock_set: mach_msg_port_descriptor_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__semaphore_create_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub policy: c_int,
    pub value: c_int,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_policy_set_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub flavor: task_policy_flavor_t,
    pub policy_infoCnt: mach_msg_type_number_t,
    pub policy_info: [integer_t; 16],
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_policy_get_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub flavor: task_policy_flavor_t,
    pub policy_infoCnt: mach_msg_type_number_t,
    pub get_default: boolean_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_sample_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub reply: mach_msg_port_descriptor_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_policy_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub policy: policy_t,
    pub baseCnt: mach_msg_type_number_t,
    pub base: [integer_t; 5],
    pub set_limit: boolean_t,
    pub change: boolean_t,
}

#[repr(C, packed(4))]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_set_emulation_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub routine_entry_pt: vm_address_t,
    pub routine_number: c_int,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_get_emulation_vector_t {
    pub Head: mach_msg_header_t,
}

#[repr(C, packed(4))]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_set_emulation_vector_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub emulation_vector: mach_msg_ool_descriptor_t,
    pub NDR: NDR_record_t,
    pub vector_start: c_int,
    pub emulation_vectorCnt: mach_msg_type_number_t,
}

#[repr(C, packed(4))]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_set_ras_pc_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub basepc: vm_address_t,
    pub boundspc: vm_address_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_assign_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub new_set: mach_msg_port_descriptor_t,
    pub NDR: NDR_record_t,
    pub assign_threads: boolean_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_assign_default_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub assign_threads: boolean_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_get_assignment_t {
    pub Head: mach_msg_header_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_set_policy_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub pset: mach_msg_port_descriptor_t,
    pub NDR: NDR_record_t,
    pub policy: policy_t,
    pub baseCnt: mach_msg_type_number_t,
    pub base: [integer_t; 5],
    pub limitCnt: mach_msg_type_number_t,
    pub limit: [integer_t; 1],
    pub change: boolean_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_get_state_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub flavor: thread_state_flavor_t,
    pub old_stateCnt: mach_msg_type_number_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Request__task_set_state_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub flavor: thread_state_flavor_t,
    pub new_stateCnt: mach_msg_type_number_t,
    pub new_state: [natural_t; 1296],
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_create_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub child_task: mach_msg_port_descriptor_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_terminate_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
}

#[repr(C, packed(4))]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_threads_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub act_list: mach_msg_ool_ports_descriptor_t,
    pub NDR: NDR_record_t,
    pub act_listCnt: mach_msg_type_number_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__mach_ports_register_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
}

#[repr(C, packed(4))]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__mach_ports_lookup_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub init_port_set: mach_msg_ool_ports_descriptor_t,
    pub NDR: NDR_record_t,
    pub init_port_setCnt: mach_msg_type_number_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_info_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
    pub task_info_outCnt: mach_msg_type_number_t,
    pub task_info_out: [integer_t; 94],
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_set_info_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_suspend_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_resume_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_get_special_port_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub special_port: mach_msg_port_descriptor_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_set_special_port_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__thread_create_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub child_act: mach_msg_port_descriptor_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__thread_create_running_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub child_act: mach_msg_port_descriptor_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_set_exception_ports_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_get_exception_ports_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub old_handlers: [mach_msg_port_descriptor_t; 32],
    pub NDR: NDR_record_t,
    pub masksCnt: mach_msg_type_number_t,
    pub masks: [exception_mask_t; 32],
    pub old_behaviors: [exception_behavior_t; 32],
    pub old_flavors: [thread_state_flavor_t; 32],
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_swap_exception_ports_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub old_handlers: [mach_msg_port_descriptor_t; 32],
    pub NDR: NDR_record_t,
    pub masksCnt: mach_msg_type_number_t,
    pub masks: [exception_mask_t; 32],
    pub old_behaviors: [exception_behavior_t; 32],
    pub old_flavors: [thread_state_flavor_t; 32],
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__lock_set_create_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub new_lock_set: mach_msg_port_descriptor_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__lock_set_destroy_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__semaphore_create_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub semaphore: mach_msg_port_descriptor_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_policy_set_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_policy_get_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
    pub policy_infoCnt: mach_msg_type_number_t,
    pub policy_info: [integer_t; 16],
    pub get_default: boolean_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_sample_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_policy_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_set_emulation_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
}

#[repr(C, packed(4))]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_get_emulation_vector_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub emulation_vector: mach_msg_ool_descriptor_t,
    pub NDR: NDR_record_t,
    pub vector_start: c_int,
    pub emulation_vectorCnt: mach_msg_type_number_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_set_emulation_vector_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_set_ras_pc_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_assign_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_assign_default_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_get_assignment_t {
    pub Head: mach_msg_header_t,
    pub msgh_body: mach_msg_body_t,
    pub assigned_set: mach_msg_port_descriptor_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_set_policy_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_get_state_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
    pub old_stateCnt: mach_msg_type_number_t,
    pub old_state: [natural_t; 1296],
}

#[repr(C)]
#[allow(dead_code, non_snake_case)]
#[derive(Copy, Clone, Debug)]
pub struct __Reply__task_set_state_t {
    pub Head: mach_msg_header_t,
    pub NDR: NDR_record_t,
    pub RetCode: kern_return_t,
}
