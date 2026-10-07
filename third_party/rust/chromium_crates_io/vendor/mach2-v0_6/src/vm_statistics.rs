//! This module roughly corresponds to `mach/vm_statistics.h`

use crate::vm_types::{integer_t, natural_t};
use core::ffi::{c_int, c_uint};

pub type vm_statistics_t = *mut vm_statistics;
pub type vm_statistics_data_t = vm_statistics;
pub type vm_statistics64_t = *mut vm_statistics64;
pub type vm_statistics64_data_t = vm_statistics64;
pub type vm_extmod_statistics_t = *mut vm_extmod_statistics;
pub type vm_extmod_statistics_data_t = vm_extmod_statistics;
pub type vm_purgeable_stat_t = vm_purgeable_stat;
pub type vm_purgeable_info_t = *mut vm_purgeable_info;

pub const VM_PAGE_QUERY_PAGE_PRESENT: integer_t = 1;
pub const VM_PAGE_QUERY_PAGE_FICTITIOUS: integer_t = 1 << 1;
pub const VM_PAGE_QUERY_PAGE_REF: integer_t = 1 << 2;
pub const VM_PAGE_QUERY_PAGE_DIRTY: integer_t = 1 << 3;
pub const VM_PAGE_QUERY_PAGE_PAGED_OUT: integer_t = 1 << 4;
pub const VM_PAGE_QUERY_PAGE_COPIED: integer_t = 1 << 5;
pub const VM_PAGE_QUERY_PAGE_SPECULATIVE: integer_t = 1 << 6;
pub const VM_PAGE_QUERY_PAGE_EXTERNAL: integer_t = 1 << 7;
pub const VM_PAGE_QUERY_PAGE_CS_VALIDATED: integer_t = 1 << 8;
pub const VM_PAGE_QUERY_PAGE_CS_TAINTED: integer_t = 1 << 9;
pub const VM_PAGE_QUERY_PAGE_CS_NX: integer_t = 1 << 10;
pub const VM_PAGE_QUERY_PAGE_REUSABLE: integer_t = 1 << 11;

pub const VM_MEMORY_MALLOC: c_uint = 1;
pub const VM_MEMORY_MALLOC_SMALL: c_uint = 2;
pub const VM_MEMORY_MALLOC_LARGE: c_uint = 3;
pub const VM_MEMORY_MALLOC_HUGE: c_uint = 4;
pub const VM_MEMORY_SBRK: c_uint = 5;
pub const VM_MEMORY_REALLOC: c_uint = 6;
pub const VM_MEMORY_MALLOC_TINY: c_uint = 7;
pub const VM_MEMORY_MALLOC_LARGE_REUSABLE: c_uint = 8;
pub const VM_MEMORY_MALLOC_LARGE_REUSED: c_uint = 9;
pub const VM_MEMORY_ANALYSIS_TOOL: c_uint = 10;
pub const VM_MEMORY_MALLOC_NANO: c_uint = 11;
pub const VM_MEMORY_MALLOC_MEDIUM: c_uint = 12;
pub const VM_MEMORY_MALLOC_PROB_GUARD: c_uint = 13;
pub const VM_MEMORY_MACH_MSG: c_uint = 20;
pub const VM_MEMORY_IOKIT: c_uint = 21;
pub const VM_MEMORY_STACK: c_uint = 30;
pub const VM_MEMORY_GUARD: c_uint = 31;
pub const VM_MEMORY_SHARED_PMAP: c_uint = 32;
pub const VM_MEMORY_DYLIB: c_uint = 33;
pub const VM_MEMORY_OBJC_DISPATCHERS: c_uint = 34;
pub const VM_MEMORY_UNSHARED_PMAP: c_uint = 35;
pub const VM_MEMORY_LIBCHANNEL: c_uint = 36;
pub const VM_MEMORY_APPKIT: c_uint = 40;
pub const VM_MEMORY_FOUNDATION: c_uint = 41;
pub const VM_MEMORY_COREGRAPHICS: c_uint = 42;
pub const VM_MEMORY_CORESERVICES: c_uint = 43;
pub const VM_MEMORY_CARBON: c_uint = VM_MEMORY_CORESERVICES;
pub const VM_MEMORY_JAVA: c_uint = 44;
pub const VM_MEMORY_COREDATA: c_uint = 45;
pub const VM_MEMORY_COREDATA_OBJECTIDS: c_uint = 46;
pub const VM_MEMORY_ATS: c_uint = 50;
pub const VM_MEMORY_LAYERKIT: c_uint = 51;
pub const VM_MEMORY_CGIMAGE: c_uint = 52;
pub const VM_MEMORY_TCMALLOC: c_uint = 53;
pub const VM_MEMORY_COREGRAPHICS_DATA: c_uint = 54;
pub const VM_MEMORY_COREGRAPHICS_SHARED: c_uint = 55;
pub const VM_MEMORY_COREGRAPHICS_FRAMEBUFFERS: c_uint = 56;
pub const VM_MEMORY_COREGRAPHICS_BACKINGSTORES: c_uint = 57;
pub const VM_MEMORY_COREGRAPHICS_XALLOC: c_uint = 58;
pub const VM_MEMORY_COREGRAPHICS_MISC: c_uint = VM_MEMORY_COREGRAPHICS;
pub const VM_MEMORY_DYLD: c_uint = 60;
pub const VM_MEMORY_DYLD_MALLOC: c_uint = 61;
pub const VM_MEMORY_SQLITE: c_uint = 62;
pub const VM_MEMORY_JAVASCRIPT_CORE: c_uint = 63;
pub const VM_MEMORY_WEBASSEMBLY: c_uint = VM_MEMORY_JAVASCRIPT_CORE;
pub const VM_MEMORY_JAVASCRIPT_JIT_EXECUTABLE_ALLOCATOR: c_uint = 64;
pub const VM_MEMORY_JAVASCRIPT_JIT_REGISTER_FILE: c_uint = 65;
pub const VM_MEMORY_GLSL: c_uint = 66;
pub const VM_MEMORY_OPENCL: c_uint = 67;
pub const VM_MEMORY_COREIMAGE: c_uint = 68;
pub const VM_MEMORY_WEBCORE_PURGEABLE_BUFFERS: c_uint = 69;
pub const VM_MEMORY_IMAGEIO: c_uint = 70;
pub const VM_MEMORY_COREPROFILE: c_uint = 71;
pub const VM_MEMORY_ASSETSD: c_uint = 72;
pub const VM_MEMORY_OS_ALLOC_ONCE: c_uint = 73;
pub const VM_MEMORY_LIBDISPATCH: c_uint = 74;
pub const VM_MEMORY_ACCELERATE: c_uint = 75;
pub const VM_MEMORY_COREUI: c_uint = 76;
pub const VM_MEMORY_COREUIFILE: c_uint = 77;
pub const VM_MEMORY_GENEALOGY: c_uint = 78;
pub const VM_MEMORY_RAWCAMERA: c_uint = 79;
pub const VM_MEMORY_CORPSEINFO: c_uint = 80;
pub const VM_MEMORY_ASL: c_uint = 81;
pub const VM_MEMORY_SWIFT_RUNTIME: c_uint = 82;
pub const VM_MEMORY_SWIFT_METADATA: c_uint = 83;
pub const VM_MEMORY_DHMM: c_uint = 84;
pub const VM_MEMORY_DFR: c_uint = 85;
pub const VM_MEMORY_SCENEKIT: c_uint = 86;
pub const VM_MEMORY_SKYWALK: c_uint = 87;
pub const VM_MEMORY_IOSURFACE: c_uint = 88;
pub const VM_MEMORY_LIBNETWORK: c_uint = 89;
pub const VM_MEMORY_AUDIO: c_uint = 90;
pub const VM_MEMORY_VIDEOBITSTREAM: c_uint = 91;
pub const VM_MEMORY_CM_XPC: c_uint = 92;
pub const VM_MEMORY_CM_RPC: c_uint = 93;
pub const VM_MEMORY_CM_MEMORYPOOL: c_uint = 94;
pub const VM_MEMORY_CM_READCACHE: c_uint = 95;
pub const VM_MEMORY_CM_CRABS: c_uint = 96;
pub const VM_MEMORY_QUICKLOOK_THUMBNAILS: c_uint = 97;
pub const VM_MEMORY_ACCOUNTS: c_uint = 98;
pub const VM_MEMORY_SANITIZER: c_uint = 99;
pub const VM_MEMORY_IOACCELERATOR: c_uint = 100;
pub const VM_MEMORY_CM_REGWARP: c_uint = 101;
pub const VM_MEMORY_EAR_DECODER: c_uint = 102;
pub const VM_MEMORY_COREUI_CACHED_IMAGE_DATA: c_uint = 103;
pub const VM_MEMORY_COLORSYNC: c_uint = 104;
pub const VM_MEMORY_BTINFO: c_uint = 105;
pub const VM_MEMORY_CM_HLS: c_uint = 106;
pub const VM_MEMORY_COMPOSITOR_SERVICES: c_uint = 107;
pub const VM_MEMORY_ROSETTA: c_uint = 230;
pub const VM_MEMORY_ROSETTA_THREAD_CONTEXT: c_uint = 231;
pub const VM_MEMORY_ROSETTA_INDIRECT_BRANCH_MAP: c_uint = 232;
pub const VM_MEMORY_ROSETTA_RETURN_STACK: c_uint = 233;
pub const VM_MEMORY_ROSETTA_EXECUTABLE_HEAP: c_uint = 234;
pub const VM_MEMORY_ROSETTA_USER_LDT: c_uint = 235;
pub const VM_MEMORY_ROSETTA_ARENA: c_uint = 236;
pub const VM_MEMORY_ROSETTA_10: c_uint = 239;
pub const VM_MEMORY_APPLICATION_SPECIFIC_1: c_uint = 240;
pub const VM_MEMORY_APPLICATION_SPECIFIC_2: c_uint = 241;
pub const VM_MEMORY_APPLICATION_SPECIFIC_3: c_uint = 242;
pub const VM_MEMORY_APPLICATION_SPECIFIC_4: c_uint = 243;
pub const VM_MEMORY_APPLICATION_SPECIFIC_5: c_uint = 244;
pub const VM_MEMORY_APPLICATION_SPECIFIC_6: c_uint = 245;
pub const VM_MEMORY_APPLICATION_SPECIFIC_7: c_uint = 246;
pub const VM_MEMORY_APPLICATION_SPECIFIC_8: c_uint = 247;
pub const VM_MEMORY_APPLICATION_SPECIFIC_9: c_uint = 248;
pub const VM_MEMORY_APPLICATION_SPECIFIC_10: c_uint = 249;
pub const VM_MEMORY_APPLICATION_SPECIFIC_11: c_uint = 250;
pub const VM_MEMORY_APPLICATION_SPECIFIC_12: c_uint = 251;
pub const VM_MEMORY_APPLICATION_SPECIFIC_13: c_uint = 252;
pub const VM_MEMORY_APPLICATION_SPECIFIC_14: c_uint = 253;
pub const VM_MEMORY_APPLICATION_SPECIFIC_15: c_uint = 254;
pub const VM_MEMORY_APPLICATION_SPECIFIC_16: c_uint = 255;
pub const VM_MEMORY_COUNT: c_uint = 256;

#[inline]
pub const fn vm_make_tag(tag: c_uint) -> c_int {
    (tag as c_int) << 24
}

pub const VM_FLAGS_FIXED: c_int = 0x0;
pub const VM_FLAGS_ANYWHERE: c_int = 0x1;
pub const VM_FLAGS_PURGABLE: c_int = 0x2;
pub const VM_FLAGS_4GB_CHUNK: c_int = 0x4;
pub const VM_FLAGS_RANDOM_ADDR: c_int = 0x8;
pub const VM_FLAGS_NO_CACHE: c_int = 0x10;
pub const VM_FLAGS_RESILIENT_CODESIGN: c_int = 0x20;
pub const VM_FLAGS_RESILIENT_MEDIA: c_int = 0x40;
pub const VM_FLAGS_PERMANENT: c_int = 0x80;
pub const VM_FLAGS_TPRO: c_int = 0x1000;
pub const VM_FLAGS_MTE: c_int = 0x2000;
pub const VM_FLAGS_OVERWRITE: c_int = 0x4000;
pub const VM_FLAGS_SUPERPAGE_MASK: c_int = 0x0007_0000;
pub const VM_FLAGS_RETURN_DATA_ADDR: c_int = 0x0010_0000;
pub const VM_FLAGS_RETURN_4K_DATA_ADDR: c_int = 0x0080_0000;
pub const VM_FLAGS_ALIAS_MASK: c_int = -16_777_216; // 0xFF000000

pub const VM_FLAGS_HW: c_int = VM_FLAGS_TPRO | VM_FLAGS_MTE;

pub const VM_FLAGS_USER_ALLOCATE: c_int = VM_FLAGS_FIXED
    | VM_FLAGS_ANYWHERE
    | VM_FLAGS_PURGABLE
    | VM_FLAGS_4GB_CHUNK
    | VM_FLAGS_RANDOM_ADDR
    | VM_FLAGS_NO_CACHE
    | VM_FLAGS_PERMANENT
    | VM_FLAGS_OVERWRITE
    | VM_FLAGS_SUPERPAGE_MASK
    | VM_FLAGS_HW
    | VM_FLAGS_ALIAS_MASK;
pub const VM_FLAGS_USER_MAP: c_int =
    VM_FLAGS_USER_ALLOCATE | VM_FLAGS_RETURN_4K_DATA_ADDR | VM_FLAGS_RETURN_DATA_ADDR;
pub const VM_FLAGS_USER_REMAP: c_int = VM_FLAGS_FIXED
    | VM_FLAGS_ANYWHERE
    | VM_FLAGS_RANDOM_ADDR
    | VM_FLAGS_OVERWRITE
    | VM_FLAGS_RETURN_DATA_ADDR
    | VM_FLAGS_RESILIENT_CODESIGN
    | VM_FLAGS_RESILIENT_MEDIA;

pub const VM_FLAGS_SUPERPAGE_SHIFT: c_int = 16;
pub const SUPERPAGE_NONE: c_int = 0;
pub const SUPERPAGE_SIZE_ANY: c_int = 1;
pub const VM_FLAGS_SUPERPAGE_NONE: c_int = SUPERPAGE_NONE << VM_FLAGS_SUPERPAGE_SHIFT;
pub const VM_FLAGS_SUPERPAGE_SIZE_ANY: c_int = SUPERPAGE_SIZE_ANY << VM_FLAGS_SUPERPAGE_SHIFT;
#[cfg(target_arch = "x86_64")]
pub const SUPERPAGE_SIZE_2MB: c_int = 2;
#[cfg(target_arch = "x86_64")]
pub const VM_FLAGS_SUPERPAGE_SIZE_2MB: c_int = SUPERPAGE_SIZE_2MB << VM_FLAGS_SUPERPAGE_SHIFT;

pub const GUARD_TYPE_VIRT_MEMORY: u32 = 5;

pub type virtual_memory_guard_exception_code_t = u32;
pub const kGUARD_EXC_DEALLOC_GAP: u32 = 1;
pub const kGUARD_EXC_RECLAIM_COPYIO_FAILURE: u32 = 2;
pub const kGUARD_EXC_RECLAIM_INDEX_FAILURE: u32 = 4;
pub const kGUARD_EXC_RECLAIM_DEALLOCATE_FAILURE: u32 = 8;
pub const kGUARD_EXC_RECLAIM_ACCOUNTING_FAILURE: u32 = 9;
pub const kGUARD_EXC_SEC_IOPL_ON_EXEC_PAGE: u32 = 10;
pub const kGUARD_EXC_SEC_EXEC_ON_IOPL_PAGE: u32 = 11;
pub const kGUARD_EXC_SEC_UPL_WRITE_ON_EXEC_REGION: u32 = 12;

pub const kGUARD_EXC_SEC_ACCESS_FAULT: u32 = 98;
pub const kGUARD_EXC_SEC_ASYNC_ACCESS_FAULT: u32 = 99;
pub const kGUARD_EXC_SEC_COPY_DENIED: u32 = 100;
pub const kGUARD_EXC_SEC_SHARING_DENIED: u32 = 101;
pub const kGUARD_EXC_MTE_SYNC_FAULT: u32 = 200;
pub const kGUARD_EXC_MTE_ASYNC_USER_FAULT: u32 = 201;
pub const kGUARD_EXC_MTE_ASYNC_KERN_FAULT: u32 = 202;

pub const kGUARD_EXC_MTE_SOFT_MODE: u32 = 0x100000;

pub const __VM_LEDGER_ACCOUNTING_POSTMARK: u64 = 2_019_032_600;
pub const VM_LEDGER_TAG_NONE: c_int = 0x0000_0000;
pub const VM_LEDGER_TAG_DEFAULT: c_int = 0x0000_0001;
pub const VM_LEDGER_TAG_NETWORK: c_int = 0x0000_0002;
pub const VM_LEDGER_TAG_MEDIA: c_int = 0x0000_0003;
pub const VM_LEDGER_TAG_GRAPHICS: c_int = 0x0000_0004;
pub const VM_LEDGER_TAG_NEURAL: c_int = 0x0000_0005;
pub const VM_LEDGER_TAG_MAX: c_int = VM_LEDGER_TAG_NEURAL;
pub const VM_LEDGER_TAG_UNCHANGED: c_int = -1;
pub const VM_LEDGER_FLAG_NO_FOOTPRINT: c_int = 1 << 0;
pub const VM_LEDGER_FLAG_NO_FOOTPRINT_FOR_DEBUG: c_int = 1 << 1;
pub const VM_LEDGER_FLAG_FROM_KERNEL: c_int = 1 << 2;
pub const VM_LEDGER_FLAGS_USER: c_int =
    VM_LEDGER_FLAG_NO_FOOTPRINT | VM_LEDGER_FLAG_NO_FOOTPRINT_FOR_DEBUG;
pub const VM_LEDGER_FLAGS_ALL: c_int = VM_LEDGER_FLAGS_USER | VM_LEDGER_FLAG_FROM_KERNEL;

#[inline]
pub fn vm_statistics_truncate_to_32_bit(value: u64) -> u32 {
    if value > u32::MAX as u64 {
        u32::MAX
    } else {
        value as u32
    }
}

#[inline]
pub fn vm_get_flags_alias(flags: c_int) -> u8 {
    ((flags >> 24) & 0xff) as u8
}

#[inline]
pub fn vm_set_flags_alias(flags: &mut c_int, alias: u8) {
    *flags = (*flags & !VM_FLAGS_ALIAS_MASK) | ((alias as c_int) << 24);
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct vm_statistics {
    pub free_count: natural_t,
    pub active_count: natural_t,
    pub inactive_count: natural_t,
    pub wire_count: natural_t,
    pub zero_fill_count: natural_t,
    pub reactivations: natural_t,
    pub pageins: natural_t,
    pub pageouts: natural_t,
    pub faults: natural_t,
    pub cow_faults: natural_t,
    pub lookups: natural_t,
    pub hits: natural_t,
    pub purgeable_count: natural_t,
    pub purges: natural_t,
    pub speculative_count: natural_t,
}

#[repr(C, packed(8))]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct vm_statistics64 {
    pub free_count: natural_t,
    pub active_count: natural_t,
    pub inactive_count: natural_t,
    pub wire_count: natural_t,
    pub zero_fill_count: u64,
    pub reactivations: u64,
    pub pageins: u64,
    pub pageouts: u64,
    pub faults: u64,
    pub cow_faults: u64,
    pub lookups: u64,
    pub hits: u64,
    pub purges: u64,
    pub purgeable_count: natural_t,
    pub speculative_count: natural_t,
    pub decompressions: u64,
    pub compressions: u64,
    pub swapins: u64,
    pub swapouts: u64,
    pub compressor_page_count: natural_t,
    pub throttled_count: natural_t,
    pub external_page_count: natural_t,
    pub internal_page_count: natural_t,
    pub total_uncompressed_pages_in_compressor: u64,
    pub swapped_count: u64,
}

#[repr(C, packed(8))]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct vm_extmod_statistics {
    pub task_for_pid_count: i64,
    pub task_for_pid_caller_count: i64,
    pub thread_creation_count: i64,
    pub thread_creation_caller_count: i64,
    pub thread_set_state_count: i64,
    pub thread_set_state_caller_count: i64,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct vm_purgeable_stat {
    pub count: u64,
    pub size: u64,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, Hash, PartialOrd, PartialEq, Eq, Ord)]
pub struct vm_purgeable_info {
    pub fifo_data: [vm_purgeable_stat_t; 8usize],
    pub obsolete_data: vm_purgeable_stat_t,
    pub lifo_data: [vm_purgeable_stat_t; 8usize],
}
