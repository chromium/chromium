//! Mach-O definitions.
//!
//! These definitions are independent of read/write support, although we do implement
//! some traits useful for those.
//!
//! This module is based heavily on header files from `MacOSX26.2.sdk`.

#![allow(missing_docs)]

#[cfg(feature = "names")]
use crate::constants::{ConstantNames, FlagNames};
use crate::endian::{BigEndian, Endian, U16, U32, U64};
use crate::pod::Pod;

/// Platform-specific constant names for a Mach-O file.
///
/// Returned by [`names`] and [`machine_names`].
#[cfg(feature = "names")]
#[derive(Debug)]
#[non_exhaustive]
pub struct Names {
    /// Values for `cpusubtype` fields.
    pub cpusubtype: &'static FlagNames<CpuSubtype>,
    /// Values for `r_type` field of `Rel*::r_info`.
    pub reloc: &'static ConstantNames<RelocationType>,
}

/// Return the platform independent names for constants.
#[cfg(feature = "names")]
pub const fn names() -> &'static Names {
    Base::names()
}

/// Return the platform specific names for constants.
///
/// Note that these also include the values returned by [`names`].
#[cfg(feature = "names")]
pub const fn machine_names(cputype: CpuType) -> &'static Names {
    match cputype {
        CPU_TYPE_X86 => X86::names(),
        CPU_TYPE_X86_64 => X86_64::names(),
        CPU_TYPE_ARM => Arm::names(),
        CPU_TYPE_ARM64 => Arm64::names(),
        CPU_TYPE_ARM64_32 => Arm64_32::names(),
        CPU_TYPE_POWERPC | CPU_TYPE_POWERPC64 => Ppc::names(),
        _ => Base::names(),
    }
}

names! {
    struct Base;
    flags cpusubtype = NAMES_CPU_SUBTYPE;
    consts reloc: RelocationType(u8) = {};
}

names! {
    struct X86(Base);
    flags cpusubtype = NAMES_CPU_SUBTYPE_X86;
    consts reloc = NAMES_GENERIC_RELOC;
}

names! {
    struct X86_64(Base);
    flags cpusubtype = NAMES_CPU_SUBTYPE_X86_64;
    consts reloc = NAMES_X86_64_RELOC;
}

names! {
    struct Arm(Base);
    flags cpusubtype = NAMES_CPU_SUBTYPE_ARM;
    consts reloc = NAMES_ARM_RELOC;
}

names! {
    struct Arm64(Base);
    flags cpusubtype = NAMES_CPU_SUBTYPE_ARM64;
    consts reloc = NAMES_ARM64_RELOC;
}

names! {
    struct Arm64_32(Base);
    flags cpusubtype = NAMES_CPU_SUBTYPE_ARM64_32;
    consts reloc = NAMES_ARM64_RELOC;
}

names! {
    struct Ppc(Base);
    flags cpusubtype = NAMES_CPU_SUBTYPE_POWERPC;
    consts reloc = NAMES_PPC_RELOC;
}

// Definitions from "/usr/include/mach/machine.h".

/*
 * Capability bits used in the definition of cpu_type.
 */

/// mask for architecture bits
pub const CPU_ARCH_MASK: u32 = 0xff00_0000;
/// 64 bit ABI
pub const CPU_ARCH_ABI64: u32 = 0x0100_0000;
/// ABI for 64-bit hardware with 32-bit types; LP32
pub const CPU_ARCH_ABI64_32: u32 = 0x0200_0000;

newtype!(
    struct CpuType(u32);
);

newtype_constant_names!(NAMES_CPU_TYPE: CpuType(u32) = {
    CPU_TYPE_ANY = !0,

    CPU_TYPE_VAX = 1,
    CPU_TYPE_MC680X0 = 6,
    CPU_TYPE_X86 = 7,
    /// Compatibility alias of [`CPU_TYPE_X86`].
    CPU_TYPE_I386 = CPU_TYPE_X86.0,
    CPU_TYPE_X86_64 = CPU_TYPE_X86.0 | CPU_ARCH_ABI64,
    CPU_TYPE_MIPS = 8,
    CPU_TYPE_MC98000 = 10,
    CPU_TYPE_HPPA = 11,
    CPU_TYPE_ARM = 12,
    CPU_TYPE_ARM64 = CPU_TYPE_ARM.0 | CPU_ARCH_ABI64,
    CPU_TYPE_ARM64_32 = CPU_TYPE_ARM.0 | CPU_ARCH_ABI64_32,
    CPU_TYPE_MC88000 = 13,
    CPU_TYPE_SPARC = 14,
    CPU_TYPE_I860 = 15,
    CPU_TYPE_ALPHA = 16,
    CPU_TYPE_POWERPC = 18,
    CPU_TYPE_POWERPC64 = CPU_TYPE_POWERPC.0 | CPU_ARCH_ABI64,
});

newtype!(
    /// The subtype identity field of [`CpuSubtype`].
    struct CpuSubtypeId(u32);
);

impl core::fmt::Debug for CpuSubtypeId {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "{:#x}", self)
    }
}

newtype!(
    /// Values for `cpusubtype` fields.
    struct CpuSubtype(u32);
);

impl CpuSubtype {
    /// Get the subtype identity field.
    pub fn id(self) -> CpuSubtypeId {
        CpuSubtypeId(self.0 & !CPU_SUBTYPE_MASK)
    }

    /// Set the subtype identity field.
    pub fn with_id(self, id: CpuSubtypeId) -> CpuSubtype {
        CpuSubtype(self.0 & CPU_SUBTYPE_MASK | id.0 & !CPU_SUBTYPE_MASK)
    }
}

impl From<CpuSubtypeId> for CpuSubtype {
    fn from(s: CpuSubtypeId) -> Self {
        CpuSubtype(s.0)
    }
}

impl From<CpuSubtype> for CpuSubtypeId {
    fn from(value: CpuSubtype) -> Self {
        value.id()
    }
}

impl core::ops::BitOr<CpuSubtype> for CpuSubtypeId {
    type Output = CpuSubtype;
    fn bitor(self, rhs: CpuSubtype) -> CpuSubtype {
        rhs.with_id(self)
    }
}

/*
 * Capability bits used in the definition of cpu_subtype.
 */
/// mask for feature flags
pub const CPU_SUBTYPE_MASK: u32 = 0xff00_0000;
newtype_flag_names!(NAMES_CPU_SUBTYPE: CpuSubtype(u32) = {
    /// 64 bit libraries
    CPU_SUBTYPE_LIB64 = 0x8000_0000,
});

/// When selecting a slice, ANY will pick the slice with the best
/// grading for the selected cpu_type_t, unlike the "ALL" subtypes,
/// which are the slices that can run on any hardware for that cpu type.
pub const CPU_SUBTYPE_ANY: CpuSubtype = CpuSubtype(!0);

/*
 *	Object files that are hand-crafted to run on any
 *	implementation of an architecture are tagged with
 *	CPU_SUBTYPE_MULTIPLE.  This functions essentially the same as
 *	the "ALL" subtype of an architecture except that it allows us
 *	to easily find object files that may need to be modified
 *	whenever a new implementation of an architecture comes out.
 *
 *	It is the responsibility of the implementor to make sure the
 *	software handles unsupported implementations elegantly.
 */
pub const CPU_SUBTYPE_MULTIPLE: CpuSubtype = CpuSubtype(!0);
newtype_consts!(CpuSubtypeId = {
    CPU_SUBTYPE_LITTLE_ENDIAN = 0,
    CPU_SUBTYPE_BIG_ENDIAN = 1,
});

/*
 *	VAX subtypes (these do *not* necessary conform to the actual cpu
 *	ID assigned by DEC available via the SID register).
 */

newtype_consts!(CpuSubtypeId = {
    CPU_SUBTYPE_VAX_ALL = 0,
    CPU_SUBTYPE_VAX780 = 1,
    CPU_SUBTYPE_VAX785 = 2,
    CPU_SUBTYPE_VAX750 = 3,
    CPU_SUBTYPE_VAX730 = 4,
    CPU_SUBTYPE_UVAXI = 5,
    CPU_SUBTYPE_UVAXII = 6,
    CPU_SUBTYPE_VAX8200 = 7,
    CPU_SUBTYPE_VAX8500 = 8,
    CPU_SUBTYPE_VAX8600 = 9,
    CPU_SUBTYPE_VAX8650 = 10,
    CPU_SUBTYPE_VAX8800 = 11,
    CPU_SUBTYPE_UVAXIII = 12,
});

/*
 *      680x0 subtypes
 *
 * The subtype definitions here are unusual for historical reasons.
 * NeXT used to consider 68030 code as generic 68000 code.  For
 * backwards compatibility:
 *
 *	CPU_SUBTYPE_MC68030 symbol has been preserved for source code
 *	compatibility.
 *
 *	CPU_SUBTYPE_MC680x0_ALL has been defined to be the same
 *	subtype as CPU_SUBTYPE_MC68030 for binary comatability.
 *
 *	CPU_SUBTYPE_MC68030_ONLY has been added to allow new object
 *	files to be tagged as containing 68030-specific instructions.
 */

newtype_consts!(CpuSubtypeId = {
    CPU_SUBTYPE_MC680X0_ALL = 1,
    // compat
    CPU_SUBTYPE_MC68030 = 1,
    CPU_SUBTYPE_MC68040 = 2,
    CPU_SUBTYPE_MC68030_ONLY = 3,
});

/*
 *	I386 subtypes
 */

#[inline]
pub const fn cpu_subtype_intel(f: u32, m: u32) -> u32 {
    f + (m << 4)
}

newtype_consts!(CpuSubtypeId = {
    CPU_SUBTYPE_I386_ALL = cpu_subtype_intel(3, 0),
    CPU_SUBTYPE_386 = cpu_subtype_intel(3, 0),
    CPU_SUBTYPE_486 = cpu_subtype_intel(4, 0),
    CPU_SUBTYPE_486SX = cpu_subtype_intel(4, 8),
    CPU_SUBTYPE_586 = cpu_subtype_intel(5, 0),
    CPU_SUBTYPE_PENT = cpu_subtype_intel(5, 0),
    CPU_SUBTYPE_PENTPRO = cpu_subtype_intel(6, 1),
    CPU_SUBTYPE_PENTII_M3 = cpu_subtype_intel(6, 3),
    CPU_SUBTYPE_PENTII_M5 = cpu_subtype_intel(6, 5),
    CPU_SUBTYPE_CELERON = cpu_subtype_intel(7, 6),
    CPU_SUBTYPE_CELERON_MOBILE = cpu_subtype_intel(7, 7),
    CPU_SUBTYPE_PENTIUM_3 = cpu_subtype_intel(8, 0),
    CPU_SUBTYPE_PENTIUM_3_M = cpu_subtype_intel(8, 1),
    CPU_SUBTYPE_PENTIUM_3_XEON = cpu_subtype_intel(8, 2),
    CPU_SUBTYPE_PENTIUM_M = cpu_subtype_intel(9, 0),
    CPU_SUBTYPE_PENTIUM_4 = cpu_subtype_intel(10, 0),
    CPU_SUBTYPE_PENTIUM_4_M = cpu_subtype_intel(10, 1),
    CPU_SUBTYPE_ITANIUM = cpu_subtype_intel(11, 0),
    CPU_SUBTYPE_ITANIUM_2 = cpu_subtype_intel(11, 1),
    CPU_SUBTYPE_XEON = cpu_subtype_intel(12, 0),
    CPU_SUBTYPE_XEON_MP = cpu_subtype_intel(12, 1),
});

#[inline]
pub const fn cpu_subtype_intel_family(x: CpuSubtypeId) -> u32 {
    x.0 & 15
}
pub const CPU_SUBTYPE_INTEL_FAMILY_MAX: u32 = 15;

#[inline]
pub const fn cpu_subtype_intel_model(x: CpuSubtypeId) -> u32 {
    x.0 >> 4
}
pub const CPU_SUBTYPE_INTEL_MODEL_ALL: u32 = 0;

/*
 *     X86 subtypes.
 */

flag_names!(NAMES_CPU_SUBTYPE_X86: CpuSubtype(u32) = NAMES_CPU_SUBTYPE + {
    _ = !CPU_SUBTYPE_MASK => NAMES_CPU_SUBTYPE_ID_X86,
});
constant_names!(NAMES_CPU_SUBTYPE_ID_X86: CpuSubtypeId(u32) = {
    CPU_SUBTYPE_X86_ALL = 3,
    CPU_SUBTYPE_X86_ARCH1 = 4,
});

flag_names!(NAMES_CPU_SUBTYPE_X86_64: CpuSubtype(u32) = NAMES_CPU_SUBTYPE + {
    _ = !CPU_SUBTYPE_MASK => NAMES_CPU_SUBTYPE_ID_X86_64,
});
constant_names!(NAMES_CPU_SUBTYPE_ID_X86_64: CpuSubtypeId(u32) = {
    CPU_SUBTYPE_X86_64_ALL = 3,
    CPU_SUBTYPE_X86_64_H = 8,
});

/*
 *	Mips subtypes.
 */

newtype_consts!(CpuSubtypeId = {
    CPU_SUBTYPE_MIPS_ALL = 0,
    CPU_SUBTYPE_MIPS_R2300 = 1,
    CPU_SUBTYPE_MIPS_R2600 = 2,
    CPU_SUBTYPE_MIPS_R2800 = 3,
    /// pmax
    CPU_SUBTYPE_MIPS_R2000A = 4,
    CPU_SUBTYPE_MIPS_R2000 = 5,
    /// 3max
    CPU_SUBTYPE_MIPS_R3000A = 6,
    CPU_SUBTYPE_MIPS_R3000 = 7,
});

/*
 *	MC98000 (PowerPC) subtypes
 */
newtype_consts!(CpuSubtypeId = {
    CPU_SUBTYPE_MC98000_ALL = 0,
    CPU_SUBTYPE_MC98601 = 1,
});

/*
 *	HPPA subtypes for Hewlett-Packard HP-PA family of
 *	risc processors. Port by NeXT to 700 series.
 */

newtype_consts!(CpuSubtypeId = {
    CPU_SUBTYPE_HPPA_ALL = 0,
    /// Compatibility alias of [`CPU_SUBTYPE_HPPA_ALL`].
    CPU_SUBTYPE_HPPA_7100 = 0,
    CPU_SUBTYPE_HPPA_7100LC = 1,
});

/*
 *	MC88000 subtypes.
 */
newtype_consts!(CpuSubtypeId = {
    CPU_SUBTYPE_MC88000_ALL = 0,
    CPU_SUBTYPE_MC88100 = 1,
    CPU_SUBTYPE_MC88110 = 2,
});

/*
 *	SPARC subtypes
 */
newtype_consts!(CpuSubtypeId = {
    CPU_SUBTYPE_SPARC_ALL = 0,
});

/*
 *	I860 subtypes
 */
newtype_consts!(CpuSubtypeId = {
    CPU_SUBTYPE_I860_ALL = 0,
    CPU_SUBTYPE_I860_860 = 1,
});

/*
 *     PowerPC subtypes
 */
flag_names!(NAMES_CPU_SUBTYPE_POWERPC: CpuSubtype(u32) = NAMES_CPU_SUBTYPE + {
    _ = !CPU_SUBTYPE_MASK => NAMES_CPU_SUBTYPE_ID_POWERPC,
});
constant_names!(NAMES_CPU_SUBTYPE_ID_POWERPC: CpuSubtypeId(u32) = {
    CPU_SUBTYPE_POWERPC_ALL = 0,
    CPU_SUBTYPE_POWERPC_601 = 1,
    CPU_SUBTYPE_POWERPC_602 = 2,
    CPU_SUBTYPE_POWERPC_603 = 3,
    CPU_SUBTYPE_POWERPC_603E = 4,
    CPU_SUBTYPE_POWERPC_603EV = 5,
    CPU_SUBTYPE_POWERPC_604 = 6,
    CPU_SUBTYPE_POWERPC_604E = 7,
    CPU_SUBTYPE_POWERPC_620 = 8,
    CPU_SUBTYPE_POWERPC_750 = 9,
    CPU_SUBTYPE_POWERPC_7400 = 10,
    CPU_SUBTYPE_POWERPC_7450 = 11,
    CPU_SUBTYPE_POWERPC_970 = 100,
});

/*
 *     ARM subtypes
 */
flag_names!(NAMES_CPU_SUBTYPE_ARM: CpuSubtype(u32) = NAMES_CPU_SUBTYPE + {
    _ = !CPU_SUBTYPE_MASK => NAMES_CPU_SUBTYPE_ID_ARM,
});
constant_names!(NAMES_CPU_SUBTYPE_ID_ARM: CpuSubtypeId(u32) = {
    CPU_SUBTYPE_ARM_ALL = 0,
    CPU_SUBTYPE_ARM_V4T = 5,
    CPU_SUBTYPE_ARM_V6 = 6,
    CPU_SUBTYPE_ARM_V5TEJ = 7,
    CPU_SUBTYPE_ARM_XSCALE = 8,
    /// ARMv7-A and ARMv7-R
    CPU_SUBTYPE_ARM_V7 = 9,
    /// Cortex A9
    CPU_SUBTYPE_ARM_V7F = 10,
    /// Swift
    CPU_SUBTYPE_ARM_V7S = 11,
    CPU_SUBTYPE_ARM_V7K = 12,
    CPU_SUBTYPE_ARM_V8 = 13,
    /// Not meant to be run under xnu
    CPU_SUBTYPE_ARM_V6M = 14,
    /// Not meant to be run under xnu
    CPU_SUBTYPE_ARM_V7M = 15,
    /// Not meant to be run under xnu
    CPU_SUBTYPE_ARM_V7EM = 16,
    /// Not meant to be run under xnu
    CPU_SUBTYPE_ARM_V8M = 17,
    /// Not meant to be run under xnu
    CPU_SUBTYPE_ARM_V8M_MAIN = CPU_SUBTYPE_ARM_V8M.0,
    /// Not meant to be run under xnu
    CPU_SUBTYPE_ARM_V8M_BASE = 18,
    /// Not meant to be run under xnu
    CPU_SUBTYPE_ARM_V8_1M_MAIN = 19,
});

/*
 *  ARM64 subtypes
 */
flag_names!(NAMES_CPU_SUBTYPE_ARM64: CpuSubtype(u32) = {
    /// pointer authentication with versioned ABI
    CPU_SUBTYPE_PTRAUTH_ABI = 0x8000_0000,
    _ = !CPU_SUBTYPE_MASK => NAMES_CPU_SUBTYPE_ID_ARM64,
});
constant_names!(NAMES_CPU_SUBTYPE_ID_ARM64: CpuSubtypeId(u32) = {
    CPU_SUBTYPE_ARM64_ALL = 0,
    CPU_SUBTYPE_ARM64_V8 = 1,
    CPU_SUBTYPE_ARM64E = 2,
});

/* CPU subtype feature flags for ptrauth on arm64e platforms */
pub const CPU_SUBTYPE_ARM64_PTR_AUTH_MASK: u32 = 0x0f000000;

impl CpuSubtype {
    /// Get the arm64 ptrauth version field.
    #[inline]
    pub const fn arm64_ptr_auth_version(self) -> u32 {
        (self.0 & CPU_SUBTYPE_ARM64_PTR_AUTH_MASK) >> 24
    }

    /// Set the arm64 ptrauth version field.
    #[inline]
    pub const fn with_arm64_ptr_auth_version(self, version: u32) -> CpuSubtype {
        CpuSubtype(
            self.0 & !CPU_SUBTYPE_ARM64_PTR_AUTH_MASK
                | (version << 24) & CPU_SUBTYPE_ARM64_PTR_AUTH_MASK,
        )
    }
}

/*
 *  ARM64_32 subtypes
 */
flag_names!(NAMES_CPU_SUBTYPE_ARM64_32: CpuSubtype(u32) = NAMES_CPU_SUBTYPE + {
    _ = !CPU_SUBTYPE_MASK => NAMES_CPU_SUBTYPE_ID_ARM64_32,
});
constant_names!(NAMES_CPU_SUBTYPE_ID_ARM64_32: CpuSubtypeId(u32) = {
    CPU_SUBTYPE_ARM64_32_ALL = 0,
    CPU_SUBTYPE_ARM64_32_V8 = 1,
});

// Definitions from "/usr/include/mach/vm_prot.h".

newtype!(
    struct VmProt(u32);
);

newtype_flag_names!(NAMES_VM_PROT: VmProt(u32) = {
    /// read permission
    VM_PROT_READ = 0x01,
    /// write permission
    VM_PROT_WRITE = 0x02,
    /// execute permission
    VM_PROT_EXECUTE = 0x04,
});

// Definitions from https://github.com/llvm/llvm-project/blob/llvmorg-22.1.3/clang/lib/Headers/ptrauth.h

/// The key used to sign a pointer for authentication.
///
/// The variant values correspond to the values used in the
/// `ptrauth_key` enum in `ptrauth.h`.
#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PtrauthKey {
    /// Instruction key A.
    IA = 0,
    /// Instruction key B.
    IB = 1,
    /// Data key A.
    DA = 2,
    /// Data key B.
    DB = 3,
}

// Definitions from https://github.com/apple-oss-distributions/dyld/blob/dyld-1340/include/mach-o/dyld_cache_format.h

/// The dyld cache header.
/// Corresponds to struct dyld_cache_header from dyld_cache_format.h.
/// This header has grown over time. Only the fields up to and including dyld_base_address
/// are guaranteed to be present. For all other fields, check the header size before
/// accessing the field. The header size is stored in mapping_offset; the mappings start
/// right after the theader.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DyldCacheHeader<E: Endian> {
    /// e.g. "dyld_v0    i386"
    pub magic: [u8; 16],
    /// file offset to first dyld_cache_mapping_info
    pub mapping_offset: U32<E>,
    /// number of dyld_cache_mapping_info entries
    pub mapping_count: U32<E>,
    /// UNUSED: moved to imagesOffset to prevent older dsc_extarctors from crashing
    pub images_offset_old: U32<E>,
    /// UNUSED: moved to imagesCount to prevent older dsc_extarctors from crashing
    pub images_count_old: U32<E>,
    /// base address of dyld when cache was built
    pub dyld_base_address: U64<E>,
    /// file offset of code signature blob
    pub code_signature_offset: U64<E>,
    /// size of code signature blob (zero means to end of file)
    pub code_signature_size: U64<E>,
    /// unused.  Used to be file offset of kernel slid info
    pub slide_info_offset_unused: U64<E>,
    /// unused.  Used to be size of kernel slid info
    pub slide_info_size_unused: U64<E>,
    /// file offset of where local symbols are stored
    pub local_symbols_offset: U64<E>,
    /// size of local symbols information
    pub local_symbols_size: U64<E>,
    /// unique value for each shared cache file
    pub uuid: [u8; 16],
    /// 0 for development, 1 for production, 2 for multi-cache
    pub cache_type: U64<E>,
    /// file offset to table of uint64_t pool addresses
    pub branch_pools_offset: U32<E>,
    /// number of uint64_t entries
    pub branch_pools_count: U32<E>,
    /// (unslid) address of mach_header of dyld in cache
    pub dyld_in_cache_mh: U64<E>,
    /// (unslid) address of entry point (_dyld_start) of dyld in cache
    pub dyld_in_cache_entry: U64<E>,
    /// file offset to first dyld_cache_image_text_info
    pub images_text_offset: U64<E>,
    /// number of dyld_cache_image_text_info entries
    pub images_text_count: U64<E>,
    /// (unslid) address of dyld_cache_patch_info
    pub patch_info_addr: U64<E>,
    /// Size of all of the patch information pointed to via the dyld_cache_patch_info
    pub patch_info_size: U64<E>,
    /// unused
    pub other_image_group_addr_unused: U64<E>,
    /// unused
    pub other_image_group_size_unused: U64<E>,
    /// (unslid) address of list of program launch closures
    pub prog_closures_addr: U64<E>,
    /// size of list of program launch closures
    pub prog_closures_size: U64<E>,
    /// (unslid) address of trie of indexes into program launch closures
    pub prog_closures_trie_addr: U64<E>,
    /// size of trie of indexes into program launch closures
    pub prog_closures_trie_size: U64<E>,
    /// platform number (macOS=1, etc)
    pub platform: U32<E>,
    /// bitfield of values:
    /// - 8: `formatVersion`, dyld3::closure::kFormatVersion
    /// - 1: `dylibsExpectedOnDisk`, dyld should expect the dylib exists on
    ///   disk and to compare inode/mtime to see if cache is valid
    /// - 1: `simulator`, for simulator of specified platform
    /// - 1: `locallyBuiltCache`, 0 for B&I built cache, 1 for locally built
    ///   cache
    /// - 1: `builtFromChainedFixups`, some dylib in cache was built using
    ///   chained fixups, so patch tables must be used for overrides
    /// - 1: `newFormatTLVs`, TLVs have been set by cache builder as new
    ///   format (not needing runtime side table)
    /// - 19: `padding`, TBD
    pub flags: U32<E>,
    /// base load address of cache if not slid
    pub shared_region_start: U64<E>,
    /// overall size required to map the cache and all subCaches, if any
    pub shared_region_size: U64<E>,
    /// runtime slide of cache can be between zero and this value
    pub max_slide: U64<E>,
    /// (unslid) address of ImageArray for dylibs in this cache
    pub dylibs_image_array_addr: U64<E>,
    /// size of ImageArray for dylibs in this cache
    pub dylibs_image_array_size: U64<E>,
    /// (unslid) address of trie of indexes of all cached dylibs
    pub dylibs_trie_addr: U64<E>,
    /// size of trie of cached dylib paths
    pub dylibs_trie_size: U64<E>,
    /// (unslid) address of ImageArray for dylibs and bundles with dlopen closures
    pub other_image_array_addr: U64<E>,
    /// size of ImageArray for dylibs and bundles with dlopen closures
    pub other_image_array_size: U64<E>,
    /// (unslid) address of trie of indexes of all dylibs and bundles with dlopen closures
    pub other_trie_addr: U64<E>,
    /// size of trie of dylibs and bundles with dlopen closures
    pub other_trie_size: U64<E>,
    /// file offset to first dyld_cache_mapping_and_slide_info
    pub mapping_with_slide_offset: U32<E>,
    /// number of dyld_cache_mapping_and_slide_info entries
    pub mapping_with_slide_count: U32<E>,
    /// unused
    pub dylibs_pbl_state_array_addr_unused: U64<E>,
    /// (unslid) address of PrebuiltLoaderSet of all cached dylibs
    pub dylibs_pbl_set_addr: U64<E>,
    /// (unslid) address of pool of PrebuiltLoaderSet for each program
    pub programs_pbl_set_pool_addr: U64<E>,
    /// size of pool of PrebuiltLoaderSet for each program
    pub programs_pbl_set_pool_size: U64<E>,
    /// (unslid) address of trie mapping program path to PrebuiltLoaderSet
    pub program_trie_addr: U64<E>,
    pub program_trie_size: U32<E>,
    /// OS Version of dylibs in this cache for the main platform
    pub os_version: U32<E>,
    /// e.g. iOSMac on macOS
    pub alt_platform: U32<E>,
    /// e.g. 14.0 for iOSMac
    pub alt_os_version: U32<E>,
    /// VM offset from cache_header* to Swift optimizations header
    pub swift_opts_offset: U64<E>,
    /// size of Swift optimizations header
    pub swift_opts_size: U64<E>,
    /// file offset to first dyld_subcache_entry
    pub sub_cache_array_offset: U32<E>,
    /// number of subCache entries
    pub sub_cache_array_count: U32<E>,
    /// unique value for the shared cache file containing unmapped local symbols
    pub symbol_file_uuid: [u8; 16],
    /// (unslid) address of the start of where Rosetta can add read-only/executable data
    pub rosetta_read_only_addr: U64<E>,
    /// maximum size of the Rosetta read-only/executable region
    pub rosetta_read_only_size: U64<E>,
    /// (unslid) address of the start of where Rosetta can add read-write data
    pub rosetta_read_write_addr: U64<E>,
    /// maximum size of the Rosetta read-write region
    pub rosetta_read_write_size: U64<E>,
    /// file offset to first dyld_cache_image_info
    pub images_offset: U32<E>,
    /// number of dyld_cache_image_info entries
    pub images_count: U32<E>,
    /// 0 for development, 1 for production, when cacheType is multi-cache(2)
    pub cache_sub_type: U32<E>,
    padding1: [u8; 4],
    /// VM offset from cache_header* to ObjC optimizations header
    pub objc_opts_offset: U64<E>,
    /// size of ObjC optimizations header
    pub objc_opts_size: U64<E>,
    /// VM offset from cache_header* to embedded cache atlas for process introspection
    pub cache_atlas_offset: U64<E>,
    /// size of embedded cache atlas
    pub cache_atlas_size: U64<E>,
    /// VM offset from cache_header* to the location of dyld_cache_dynamic_data_header
    pub dynamic_data_offset: U64<E>,
    /// maximum size of space reserved from dynamic data
    pub dynamic_data_max_size: U64<E>,
    /// file offset to first dyld_cache_tpro_mapping_info
    pub tpro_mappings_offset: U32<E>,
    /// number of dyld_cache_tpro_mapping_info entries
    pub tpro_mappings_count: U32<E>,
    /// (unslid) address of dyld_cache_function_variant_info
    pub function_variant_info_addr: U64<E>,
    /// Size of all of the variant information pointed to via the dyld_cache_function_variant_info
    pub function_variant_info_size: U64<E>,
    /// file offset to dyld_prewarming_header
    pub prewarming_data_offset: U64<E>,
    /// byte size of prewarming data
    pub prewarming_data_size: U64<E>,
}

/// Corresponds to struct dyld_cache_mapping_info from dyld_cache_format.h.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DyldCacheMappingInfo<E: Endian> {
    pub address: U64<E>,
    pub size: U64<E>,
    pub file_offset: U64<E>,
    pub max_prot: U32<E, VmProt>,
    pub init_prot: U32<E, VmProt>,
}

newtype!(
    /// Values for `DyldCacheMappingAndSlideInfo::flags`.
    struct DyldCacheMappingFlags(u64);
);

newtype_flag_names!(NAMES_DYLD_CACHE_MAPPING: DyldCacheMappingFlags(u64) = {
    DYLD_CACHE_MAPPING_AUTH_DATA = 1 << 0,
    DYLD_CACHE_MAPPING_DIRTY_DATA = 1 << 1,
    DYLD_CACHE_MAPPING_CONST_DATA = 1 << 2,
    DYLD_CACHE_MAPPING_TEXT_STUBS = 1 << 3,
    DYLD_CACHE_DYNAMIC_CONFIG_DATA = 1 << 4,
    DYLD_CACHE_READ_ONLY_DATA = 1 << 5,
    DYLD_CACHE_MAPPING_CONST_TPRO_DATA = 1 << 6,
});

/// Corresponds to struct dyld_cache_mapping_and_slide_info from dyld_cache_format.h.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DyldCacheMappingAndSlideInfo<E: Endian> {
    pub address: U64<E>,
    pub size: U64<E>,
    pub file_offset: U64<E>,
    pub slide_info_file_offset: U64<E>,
    pub slide_info_file_size: U64<E>,
    pub flags: U64<E, DyldCacheMappingFlags>,
    pub max_prot: U32<E, VmProt>,
    pub init_prot: U32<E, VmProt>,
}

/// Corresponds to struct dyld_cache_tpro_mapping_info from dyld_cache_format.h.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DyldCacheTproMappingInfo<E: Endian> {
    pub unslid_address: U64<E>,
    pub size: U64<E>,
}

/// Corresponds to struct dyld_cache_image_info from dyld_cache_format.h.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DyldCacheImageInfo<E: Endian> {
    pub address: U64<E>,
    pub mod_time: U64<E>,
    pub inode: U64<E>,
    pub path_file_offset: U32<E>,
    pub pad: U32<E>,
}

// Missing:
// dyld_cache_image_info_extra
// dyld_cache_accelerator_info
// dyld_cache_accelerator_initializer
// dyld_cache_range_entry
// dyld_cache_accelerator_dof
// dyld_cache_image_text_info
// dyld_cache_slide_info

/// Corresponds to struct dyld_cache_slide_info2 from dyld_cache_format.h.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DyldCacheSlideInfo2<E: Endian> {
    pub version: U32<E>,   // currently 2
    pub page_size: U32<E>, // currently 4096 (may also be 16384)
    pub page_starts_offset: U32<E>,
    pub page_starts_count: U32<E>,
    pub page_extras_offset: U32<E>,
    pub page_extras_count: U32<E>,
    pub delta_mask: U64<E>, // which (contiguous) set of bits contains the delta to the next rebase location
    pub value_add: U64<E>,
}

pub const DYLD_CACHE_SLIDE_PAGE_ATTRS: u16 = 0xC000;
// Index is into extras array (not starts array).
pub const DYLD_CACHE_SLIDE_PAGE_ATTR_EXTRA: u16 = 0x8000;
// Page has no rebasing.
pub const DYLD_CACHE_SLIDE_PAGE_ATTR_NO_REBASE: u16 = 0x4000;
// Last chain entry for page.
pub const DYLD_CACHE_SLIDE_PAGE_ATTR_END: u16 = 0x8000;

/// Corresponds to struct dyld_cache_slide_info3 from dyld_cache_format.h.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DyldCacheSlideInfo3<E: Endian> {
    pub version: U32<E>,   // currently 3
    pub page_size: U32<E>, // currently 4096 (may also be 16384)
    pub page_starts_count: U32<E>,
    reserved1: [u8; 4],
    pub auth_value_add: U64<E>,
}

/// Page has no rebasing.
pub const DYLD_CACHE_SLIDE_V3_PAGE_ATTR_NO_REBASE: u16 = 0xFFFF;

/// Corresponds to union dyld_cache_slide_pointer3 from dyld_cache_format.h.
#[derive(Debug, Clone, Copy)]
pub struct DyldCacheSlidePointer3(pub u64);

impl DyldCacheSlidePointer3 {
    /// Whether the pointer is authenticated.
    pub fn is_auth(&self) -> bool {
        ((self.0 >> 63) & 1) != 0
    }

    /// The target of the pointer.
    ///
    /// Only valid if `is_auth` is false.
    pub fn target(&self) -> u64 {
        self.0 & ((1 << 43) - 1)
    }

    /// The high 8 bits of the pointer.
    ///
    /// Only valid if `is_auth` is false.
    pub fn high8(&self) -> u64 {
        (self.0 >> 43) & 0xff
    }

    /// The target of the pointer as an offset from the start of the shared cache.
    ///
    /// Only valid if `is_auth` is true.
    pub fn runtime_offset(&self) -> u64 {
        self.0 & ((1 << 32) - 1)
    }

    /// The diversity value for authentication.
    ///
    /// Only valid if `is_auth` is true.
    pub fn diversity(&self) -> u16 {
        ((self.0 >> 32) & 0xffff) as u16
    }

    /// Whether to use address diversity for authentication.
    ///
    /// Only valid if `is_auth` is true.
    pub fn addr_div(&self) -> bool {
        ((self.0 >> 48) & 1) != 0
    }

    /// The key for authentication.
    ///
    /// Only valid if `is_auth` is true.
    pub fn key(&self) -> u8 {
        ((self.0 >> 49) & 3) as u8
    }

    /// The offset to the next slide pointer in 8-byte units.
    ///
    /// 0 if no next slide pointer.
    pub fn next(&self) -> u64 {
        (self.0 >> 51) & ((1 << 11) - 1)
    }
}

// Missing: dyld_cache_slide_info4

/// Corresponds to struct dyld_cache_slide_info5 from dyld_cache_format.h.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DyldCacheSlideInfo5<E: Endian> {
    pub version: U32<E>,   // currently 5
    pub page_size: U32<E>, // currently 4096 (may also be 16384)
    pub page_starts_count: U32<E>,
    reserved1: [u8; 4],
    pub value_add: U64<E>,
}

/// Page has no rebasing.
pub const DYLD_CACHE_SLIDE_V5_PAGE_ATTR_NO_REBASE: u16 = 0xFFFF;

/// Corresponds to struct dyld_cache_slide_pointer5 from dyld_cache_format.h.
#[derive(Debug, Clone, Copy)]
pub struct DyldCacheSlidePointer5(pub u64);

impl DyldCacheSlidePointer5 {
    /// Whether the pointer is authenticated.
    pub fn is_auth(&self) -> bool {
        ((self.0 >> 63) & 1) != 0
    }

    /// The target of the pointer as an offset from the start of the shared cache.
    pub fn runtime_offset(&self) -> u64 {
        self.0 & 0x3_ffff_ffff
    }

    /// The high 8 bits of the pointer.
    ///
    /// Only valid if `is_auth` is false.
    pub fn high8(&self) -> u64 {
        (self.0 >> 34) & 0xff
    }

    /// The diversity value for authentication.
    ///
    /// Only valid if `is_auth` is true.
    pub fn diversity(&self) -> u16 {
        ((self.0 >> 34) & 0xffff) as u16
    }

    /// Whether to use address diversity for authentication.
    ///
    /// Only valid if `is_auth` is true.
    pub fn addr_div(&self) -> bool {
        ((self.0 >> 50) & 1) != 0
    }

    /// Whether the key is IA or DA.
    ///
    /// Only valid if `is_auth` is true.
    pub fn key_is_data(&self) -> bool {
        ((self.0 >> 51) & 1) != 0
    }

    /// The offset to the next slide pointer in 8-byte units.
    ///
    /// 0 if no next slide pointer.
    pub fn next(&self) -> u64 {
        (self.0 >> 52) & 0x7ff
    }
}

// Missing:
// dyld_cache_local_symbols_info
// dyld_cache_local_symbols_entry
// dyld_cache_local_symbols_entry_64

/// Added in dyld-940, which shipped with macOS 12 / iOS 15.
/// Originally called `dyld_subcache_entry`, renamed to `dyld_subcache_entry_v1`
/// in dyld-1042.1.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DyldSubCacheEntryV1<E: Endian> {
    /// The UUID of this subcache.
    pub uuid: [u8; 16],
    /// The offset of this subcache from the main cache base address.
    pub cache_vm_offset: U64<E>,
}

/// Added in dyld-1042.1, which shipped with macOS 13 / iOS 16.
/// Called `dyld_subcache_entry` as of dyld-1042.1.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DyldSubCacheEntryV2<E: Endian> {
    /// The UUID of this subcache.
    pub uuid: [u8; 16],
    /// The offset of this subcache from the main cache base address.
    pub cache_vm_offset: U64<E>,
    /// The file name suffix of the subCache file, e.g. ".25.data" or ".03.development".
    pub file_suffix: [u8; 32],
}

// Missing:
// dyld_cache_function_variant_entry
// dyld_cache_function_variant_info
// dyld_prewarming_entry
// dyld_prewarming_header

// Definitions from "/usr/include/mach-o/fat.h".

/*
 * This header file describes the structures of the file format for "fat"
 * architecture specific file (wrapper design).  At the beginning of the file
 * there is one `FatHeader` structure followed by a number of `FatArch*`
 * structures.  For each architecture in the file, specified by a pair of
 * cputype and cpusubtype, the `FatHeader` describes the file offset, file
 * size and alignment in the file of the architecture specific member.
 * The padded bytes in the file to place each member on it's specific alignment
 * are defined to be read as zeros and can be left as "holes" if the file system
 * can support them as long as they read as zeros.
 *
 * All structures defined here are always written and read to/from disk
 * in big-endian order.
 */

pub const FAT_MAGIC: u32 = 0xcafe_babe;
/// NXSwapLong(FAT_MAGIC)
pub const FAT_CIGAM: u32 = 0xbeba_feca;

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct FatHeader {
    /// FAT_MAGIC or FAT_MAGIC_64
    pub magic: U32<BigEndian>,
    /// number of structs that follow
    pub nfat_arch: U32<BigEndian>,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct FatArch32 {
    /// cpu specifier (int)
    pub cputype: U32<BigEndian, CpuType>,
    /// machine specifier (int)
    pub cpusubtype: U32<BigEndian, CpuSubtype>,
    /// file offset to this object file
    pub offset: U32<BigEndian>,
    /// size of this object file
    pub size: U32<BigEndian>,
    /// alignment as a power of 2
    pub align: U32<BigEndian>,
}

/*
 * The support for the 64-bit fat file format described here is a work in
 * progress and not yet fully supported in all the Apple Developer Tools.
 *
 * When a slice is greater than 4mb or an offset to a slice is greater than 4mb
 * then the 64-bit fat file format is used.
 */
pub const FAT_MAGIC_64: u32 = 0xcafe_babf;
/// NXSwapLong(FAT_MAGIC_64)
pub const FAT_CIGAM_64: u32 = 0xbfba_feca;

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct FatArch64 {
    /// cpu specifier (int)
    pub cputype: U32<BigEndian, CpuType>,
    /// machine specifier (int)
    pub cpusubtype: U32<BigEndian, CpuSubtype>,
    /// file offset to this object file
    pub offset: U64<BigEndian>,
    /// size of this object file
    pub size: U64<BigEndian>,
    /// alignment as a power of 2
    pub align: U32<BigEndian>,
    /// reserved
    pub reserved: U32<BigEndian>,
}

// Definitions from "/usr/include/mach-o/loader.h".

/// The 32-bit mach header.
///
/// Appears at the very beginning of the object file for 32-bit architectures.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct MachHeader32<E: Endian> {
    /// mach magic number identifier
    pub magic: U32<BigEndian>,
    /// cpu specifier
    pub cputype: U32<E, CpuType>,
    /// machine specifier
    pub cpusubtype: U32<E, CpuSubtype>,
    /// type of file
    pub filetype: U32<E, FileType>,
    /// number of load commands
    pub ncmds: U32<E>,
    /// the size of all the load commands
    pub sizeofcmds: U32<E>,
    /// flags
    pub flags: U32<E, FileFlags>,
}

// Values for `MachHeader32::magic`.
/// the mach magic number
pub const MH_MAGIC: u32 = 0xfeed_face;
/// NXSwapInt(MH_MAGIC)
pub const MH_CIGAM: u32 = 0xcefa_edfe;

/// The 64-bit mach header.
///
/// Appears at the very beginning of object files for 64-bit architectures.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct MachHeader64<E: Endian> {
    /// mach magic number identifier
    pub magic: U32<BigEndian>,
    /// cpu specifier
    pub cputype: U32<E, CpuType>,
    /// machine specifier
    pub cpusubtype: U32<E, CpuSubtype>,
    /// type of file
    pub filetype: U32<E, FileType>,
    /// number of load commands
    pub ncmds: U32<E>,
    /// the size of all the load commands
    pub sizeofcmds: U32<E>,
    /// flags
    pub flags: U32<E, FileFlags>,
    /// reserved
    pub reserved: U32<E>,
}

// Values for `MachHeader64::magic`.
/// the 64-bit mach magic number
pub const MH_MAGIC_64: u32 = 0xfeed_facf;
/// NXSwapInt(MH_MAGIC_64)
pub const MH_CIGAM_64: u32 = 0xcffa_edfe;

/*
 * The layout of the file depends on the filetype.  For all but the MH_OBJECT
 * file type the segments are padded out and aligned on a segment alignment
 * boundary for efficient demand pageing.  The MH_EXECUTE, MH_FVMLIB, MH_DYLIB,
 * MH_DYLINKER and MH_BUNDLE file types also have the headers included as part
 * of their first segment.
 *
 * The file type MH_OBJECT is a compact format intended as output of the
 * assembler and input (and possibly output) of the link editor (the .o
 * format).  All sections are in one unnamed segment with no segment padding.
 * This format is used as an executable format when the file is so small the
 * segment padding greatly increases its size.
 *
 * The file type MH_PRELOAD is an executable format intended for things that
 * are not executed under the kernel (proms, stand alones, kernels, etc).  The
 * format can be executed under the kernel but may demand paged it and not
 * preload it before execution.
 *
 * A core file is in MH_CORE format and can be any in an arbritray legal
 * Mach-O file.
 */

newtype!(
    /// Values for `MachHeader*::filetype`.
    struct FileType(u32);
);

newtype_constant_names!(NAMES_MH_TYPE: FileType(u32) = {
    /// relocatable object file
    MH_OBJECT = 0x1,
    /// demand paged executable file
    MH_EXECUTE = 0x2,
    /// fixed VM shared library file
    MH_FVMLIB = 0x3,
    /// core file
    MH_CORE = 0x4,
    /// preloaded executable file
    MH_PRELOAD = 0x5,
    /// dynamically bound shared library
    MH_DYLIB = 0x6,
    /// dynamic link editor
    MH_DYLINKER = 0x7,
    /// dynamically bound bundle file
    MH_BUNDLE = 0x8,
    /// shared library stub for static linking only, no section contents
    MH_DYLIB_STUB = 0x9,
    /// companion file with only debug sections
    MH_DSYM = 0xa,
    /// x86_64 kexts
    MH_KEXT_BUNDLE = 0xb,
    /// a file composed of other Mach-Os to be run in the same userspace sharing a single linkedit.
    MH_FILESET = 0xc,
    /// gpu program
    MH_GPU_EXECUTE = 0xd,
    /// gpu support functions
    MH_GPU_DYLIB = 0xe,
});

newtype!(
    /// Values for `MachHeader*::flags`.
    struct FileFlags(u32);
);

newtype_flag_names!(NAMES_MH_FLAGS: FileFlags(u32) = {
    /// the object file has no undefined references
    MH_NOUNDEFS = 0x1,
    /// the object file is the output of an incremental link against a base file and can't be link edited again
    MH_INCRLINK = 0x2,
    /// the object file is input for the dynamic linker and can't be statically link edited again
    MH_DYLDLINK = 0x4,
    /// the object file's undefined references are bound by the dynamic linker when loaded.
    MH_BINDATLOAD = 0x8,
    /// the file has its dynamic undefined references prebound.
    MH_PREBOUND = 0x10,
    /// the file has its read-only and read-write segments split
    MH_SPLIT_SEGS = 0x20,
    /// the shared library init routine is to be run lazily via catching memory faults to its writeable segments (obsolete)
    MH_LAZY_INIT = 0x40,
    /// the image is using two-level name space bindings
    MH_TWOLEVEL = 0x80,
    /// the executable is forcing all images to use flat name space bindings
    MH_FORCE_FLAT = 0x100,
    /// this umbrella guarantees no multiple definitions of symbols in its sub-images so the two-level namespace hints can always be used.
    MH_NOMULTIDEFS = 0x200,
    /// do not have dyld notify the prebinding agent about this executable
    MH_NOFIXPREBINDING = 0x400,
    /// the binary is not prebound but can have its prebinding redone. only used when MH_PREBOUND is not set.
    MH_PREBINDABLE = 0x800,
    /// indicates that this binary binds to all two-level namespace modules of its dependent libraries. only used when MH_PREBINDABLE and MH_TWOLEVEL are both set.
    MH_ALLMODSBOUND = 0x1000,
    /// safe to divide up the sections into sub-sections via symbols for dead code stripping
    MH_SUBSECTIONS_VIA_SYMBOLS = 0x2000,
    /// the binary has been canonicalized via the unprebind operation
    MH_CANONICAL = 0x4000,
    /// the final linked image contains external weak symbols
    MH_WEAK_DEFINES = 0x8000,
    /// the final linked image uses weak symbols
    MH_BINDS_TO_WEAK = 0x10000,
    /// When this bit is set, all stacks in the task will be given stack execution privilege.  Only used in MH_EXECUTE filetypes.
    MH_ALLOW_STACK_EXECUTION = 0x20000,
    /// When this bit is set, the binary declares it is safe for use in processes with uid zero
    MH_ROOT_SAFE = 0x40000,
    /// When this bit is set, the binary declares it is safe for use in processes when issetugid() is true
    MH_SETUID_SAFE = 0x80000,
    /// When this bit is set on a dylib, the static linker does not need to examine dependent dylibs to see if any are re-exported
    MH_NO_REEXPORTED_DYLIBS = 0x10_0000,
    /// When this bit is set, the OS will load the main executable at a random address.  Only used in MH_EXECUTE filetypes.
    MH_PIE = 0x20_0000,
    /// Only for use on dylibs.  When linking against a dylib that has this bit set, the static linker will automatically not create a LC_LOAD_DYLIB load command to the dylib if no symbols are being referenced from the dylib.
    MH_DEAD_STRIPPABLE_DYLIB = 0x40_0000,
    /// Contains a section of type S_THREAD_LOCAL_VARIABLES
    MH_HAS_TLV_DESCRIPTORS = 0x80_0000,
    /// When this bit is set, the OS will run the main executable with a non-executable heap even on platforms (e.g. i386) that don't require it. Only used in MH_EXECUTE filetypes.
    MH_NO_HEAP_EXECUTION = 0x100_0000,
    /// The code was linked for use in an application extension.
    MH_APP_EXTENSION_SAFE = 0x0200_0000,
    /// The external symbols listed in the nlist symbol table do not include all the symbols listed in the dyld info.
    MH_NLIST_OUTOFSYNC_WITH_DYLDINFO = 0x0400_0000,
    /// Allow LC_MIN_VERSION_MACOS and LC_BUILD_VERSION load commands with
    /// the platforms macOS, iOSMac, iOSSimulator, tvOSSimulator and watchOSSimulator.
    MH_SIM_SUPPORT = 0x0800_0000,
    /// main executable has no __PAGEZERO segment.  Instead, loader (xnu) will load program high and block out all memory below it.
    MH_IMPLICIT_PAGEZERO = 0x1000_0000,
    /// Only for use on dylibs. When this bit is set, the dylib is part of the dyld
    /// shared cache, rather than loose in the filesystem.
    MH_DYLIB_IN_CACHE = 0x8000_0000,
});

/// Common fields at the start of every load command.
///
/// The load commands directly follow the mach_header.  The total size of all
/// of the commands is given by the sizeofcmds field in the mach_header.  All
/// load commands must have as their first two fields `cmd` and `cmdsize`.  The `cmd`
/// field is filled in with a constant for that command type.  Each command type
/// has a structure specifically for it.  The `cmdsize` field is the size in bytes
/// of the particular load command structure plus anything that follows it that
/// is a part of the load command (i.e. section structures, strings, etc.).  To
/// advance to the next load command the `cmdsize` can be added to the offset or
/// pointer of the current load command.  The `cmdsize` for 32-bit architectures
/// MUST be a multiple of 4 bytes and for 64-bit architectures MUST be a multiple
/// of 8 bytes (these are forever the maximum alignment of any load commands).
/// The padded bytes must be zero.  All tables in the object file must also
/// follow these rules so the file can be memory mapped.  Otherwise the pointers
/// to these tables will not work well or at all on some machines.  With all
/// padding zeroed like objects will compare byte for byte.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct LoadCommand<E: Endian> {
    /// Type of load command.
    ///
    /// One of the `LC_*` constants.
    pub cmd: U32<E, LoadCommandType>,
    /// Total size of command in bytes.
    pub cmdsize: U32<E>,
}

/*
 * After MacOS X 10.1 when a new load command is added that is required to be
 * understood by the dynamic linker for the image to execute properly the
 * LC_REQ_DYLD bit will be or'ed into the load command constant.  If the dynamic
 * linker sees such a load command it it does not understand will issue a
 * "unknown load command required for execution" error and refuse to use the
 * image.  Other load commands without this bit that are not understood will
 * simply be ignored.
 */
pub const LC_REQ_DYLD: u32 = 0x8000_0000;

newtype!(
    /// Values for `LoadCommand*::cmd`.
    struct LoadCommandType(u32);
);

newtype_constant_names!(NAMES_LC_TYPE: LoadCommandType(u32) = {
    /// segment of this file to be mapped
    LC_SEGMENT = 0x1,
    /// link-edit stab symbol table info
    LC_SYMTAB = 0x2,
    /// link-edit gdb symbol table info (obsolete)
    LC_SYMSEG = 0x3,
    /// thread
    LC_THREAD = 0x4,
    /// unix thread (includes a stack)
    LC_UNIXTHREAD = 0x5,
    /// load a specified fixed VM shared library
    LC_LOADFVMLIB = 0x6,
    /// fixed VM shared library identification
    LC_IDFVMLIB = 0x7,
    /// object identification info (obsolete)
    LC_IDENT = 0x8,
    /// fixed VM file inclusion (internal use)
    LC_FVMFILE = 0x9,
    /// prepage command (internal use)
    LC_PREPAGE = 0xa,
    /// dynamic link-edit symbol table info
    LC_DYSYMTAB = 0xb,
    /// load a dynamically linked shared library
    LC_LOAD_DYLIB = 0xc,
    /// dynamically linked shared lib ident
    LC_ID_DYLIB = 0xd,
    /// load a dynamic linker
    LC_LOAD_DYLINKER = 0xe,
    /// dynamic linker identification
    LC_ID_DYLINKER = 0xf,
    /// modules prebound for a dynamically linked shared library
    LC_PREBOUND_DYLIB = 0x10,
    /// image routines
    LC_ROUTINES = 0x11,
    /// sub framework
    LC_SUB_FRAMEWORK = 0x12,
    /// sub umbrella
    LC_SUB_UMBRELLA = 0x13,
    /// sub client
    LC_SUB_CLIENT = 0x14,
    /// sub library
    LC_SUB_LIBRARY = 0x15,
    /// two-level namespace lookup hints
    LC_TWOLEVEL_HINTS = 0x16,
    /// prebind checksum
    LC_PREBIND_CKSUM = 0x17,
    /// load a dynamically linked shared library that is allowed to be missing
    /// (all symbols are weak imported).
    LC_LOAD_WEAK_DYLIB = 0x18 | LC_REQ_DYLD,
    /// 64-bit segment of this file to be mapped
    LC_SEGMENT_64 = 0x19,
    /// 64-bit image routines
    LC_ROUTINES_64 = 0x1a,
    /// the uuid
    LC_UUID = 0x1b,
    /// runpath additions
    LC_RPATH = 0x1c | LC_REQ_DYLD,
    /// local of code signature
    LC_CODE_SIGNATURE = 0x1d,
    /// local of info to split segments
    LC_SEGMENT_SPLIT_INFO = 0x1e,
    /// load and re-export dylib
    LC_REEXPORT_DYLIB = 0x1f | LC_REQ_DYLD,
    /// delay load of dylib until first use
    LC_LAZY_LOAD_DYLIB = 0x20,
    /// encrypted segment information
    LC_ENCRYPTION_INFO = 0x21,
    /// compressed dyld information
    LC_DYLD_INFO = 0x22,
    /// compressed dyld information only
    LC_DYLD_INFO_ONLY = 0x22 | LC_REQ_DYLD,
    /// load upward dylib
    LC_LOAD_UPWARD_DYLIB = 0x23 | LC_REQ_DYLD,
    /// build for MacOSX min OS version
    LC_VERSION_MIN_MACOSX = 0x24,
    /// build for iPhoneOS min OS version
    LC_VERSION_MIN_IPHONEOS = 0x25,
    /// compressed table of function start addresses
    LC_FUNCTION_STARTS = 0x26,
    /// string for dyld to treat like environment variable
    LC_DYLD_ENVIRONMENT = 0x27,
    /// replacement for LC_UNIXTHREAD
    LC_MAIN = 0x28 | LC_REQ_DYLD,
    /// table of non-instructions in __text
    LC_DATA_IN_CODE = 0x29,
    /// source version used to build binary
    LC_SOURCE_VERSION = 0x2A,
    /// Code signing DRs copied from linked dylibs
    LC_DYLIB_CODE_SIGN_DRS = 0x2B,
    /// 64-bit encrypted segment information
    LC_ENCRYPTION_INFO_64 = 0x2C,
    /// linker options in MH_OBJECT files
    LC_LINKER_OPTION = 0x2D,
    /// optimization hints in MH_OBJECT files
    LC_LINKER_OPTIMIZATION_HINT = 0x2E,
    /// build for AppleTV min OS version
    LC_VERSION_MIN_TVOS = 0x2F,
    /// build for Watch min OS version
    LC_VERSION_MIN_WATCHOS = 0x30,
    /// arbitrary data included within a Mach-O file
    LC_NOTE = 0x31,
    /// build for platform min OS version
    LC_BUILD_VERSION = 0x32,
    /// used with `LinkeditDataCommand`, payload is trie
    LC_DYLD_EXPORTS_TRIE = 0x33 | LC_REQ_DYLD,
    /// used with `LinkeditDataCommand`
    LC_DYLD_CHAINED_FIXUPS = 0x34 | LC_REQ_DYLD,
    /// used with `FilesetEntryCommand`
    LC_FILESET_ENTRY = 0x35 | LC_REQ_DYLD,
    /// used with linkedit_data_command
    LC_ATOM_INFO = 0x36,
    /// used with linkedit_data_command
    LC_FUNCTION_VARIANTS = 0x37,
    /// used with linkedit_data_command
    LC_FUNCTION_VARIANT_FIXUPS = 0x38,
    /// target triple used to compile
    LC_TARGET_TRIPLE = 0x39,
});

/// A variable length string in a load command.
///
/// The strings are stored just after the load command structure and
/// the offset is from the start of the load command structure.  The size
/// of the string is reflected in the `cmdsize` field of the load command.
/// Once again any padded bytes to bring the `cmdsize` field to a multiple
/// of 4 bytes must be zero.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct LcStr<E: Endian> {
    /// offset to the string
    pub offset: U32<E>,
}

/// 32-bit segment load command.
///
/// The segment load command indicates that a part of this file is to be
/// mapped into the task's address space.  The size of this segment in memory,
/// vmsize, maybe equal to or larger than the amount to map from this file,
/// filesize.  The file is mapped starting at fileoff to the beginning of
/// the segment in memory, vmaddr.  The rest of the memory of the segment,
/// if any, is allocated zero fill on demand.  The segment's maximum virtual
/// memory protection and initial virtual memory protection are specified
/// by the maxprot and initprot fields.  If the segment has sections then the
/// `Section32` structures directly follow the segment command and their size is
/// reflected in `cmdsize`.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct SegmentCommand32<E: Endian> {
    /// LC_SEGMENT
    pub cmd: U32<E, LoadCommandType>,
    /// includes sizeof section structs
    pub cmdsize: U32<E>,
    /// segment name
    pub segname: [u8; 16],
    /// memory address of this segment
    pub vmaddr: U32<E>,
    /// memory size of this segment
    pub vmsize: U32<E>,
    /// file offset of this segment
    pub fileoff: U32<E>,
    /// amount to map from the file
    pub filesize: U32<E>,
    /// maximum VM protection
    pub maxprot: U32<E, VmProt>,
    /// initial VM protection
    pub initprot: U32<E, VmProt>,
    /// number of sections in segment
    pub nsects: U32<E>,
    /// flags
    pub flags: U32<E, SegmentFlags>,
}

/// 64-bit segment load command.
///
/// The 64-bit segment load command indicates that a part of this file is to be
/// mapped into a 64-bit task's address space.  If the 64-bit segment has
/// sections then `Section64` structures directly follow the 64-bit segment
/// command and their size is reflected in `cmdsize`.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct SegmentCommand64<E: Endian> {
    /// LC_SEGMENT_64
    pub cmd: U32<E, LoadCommandType>,
    /// includes sizeof section_64 structs
    pub cmdsize: U32<E>,
    /// segment name
    pub segname: [u8; 16],
    /// memory address of this segment
    pub vmaddr: U64<E>,
    /// memory size of this segment
    pub vmsize: U64<E>,
    /// file offset of this segment
    pub fileoff: U64<E>,
    /// amount to map from the file
    pub filesize: U64<E>,
    /// maximum VM protection
    pub maxprot: U32<E, VmProt>,
    /// initial VM protection
    pub initprot: U32<E, VmProt>,
    /// number of sections in segment
    pub nsects: U32<E>,
    /// flags
    pub flags: U32<E, SegmentFlags>,
}

newtype!(
    /// Values for `SegmentCommand*::flags`.
    struct SegmentFlags(u32);
);

newtype_flag_names!(NAMES_SG: SegmentFlags(u32) = {
    /// the file contents for this segment is for the high part of the VM space, the low part is zero filled (for stacks in core files)
    SG_HIGHVM = 0x1,
    /// this segment is the VM that is allocated by a fixed VM library, for overlap checking in the link editor
    SG_FVMLIB = 0x2,
    /// this segment has nothing that was relocated in it and nothing relocated to it, that is it maybe safely replaced without relocation
    SG_NORELOC = 0x4,
    /// This segment is protected.  If the segment starts at file offset 0, the first page of the segment is not protected.  All other pages of the segment are protected.
    SG_PROTECTED_VERSION_1 = 0x8,
    /// This segment is made read-only after fixups
    SG_READ_ONLY = 0x10,
});

/*
 * A segment is made up of zero or more sections.  Non-MH_OBJECT files have
 * all of their segments with the proper sections in each, and padded to the
 * specified segment alignment when produced by the link editor.  The first
 * segment of a MH_EXECUTE and MH_FVMLIB format file contains the mach_header
 * and load commands of the object file before its first section.  The zero
 * fill sections are always last in their segment (in all formats).  This
 * allows the zeroed segment padding to be mapped into memory where zero fill
 * sections might be. The gigabyte zero fill sections, those with the section
 * type S_GB_ZEROFILL, can only be in a segment with sections of this type.
 * These segments are then placed after all other segments.
 *
 * The MH_OBJECT format has all of its sections in one segment for
 * compactness.  There is no padding to a specified segment boundary and the
 * mach_header and load commands are not part of the segment.
 *
 * Sections with the same section name, sectname, going into the same segment,
 * segname, are combined by the link editor.  The resulting section is aligned
 * to the maximum alignment of the combined sections and is the new section's
 * alignment.  The combined sections are aligned to their original alignment in
 * the combined section.  Any padded bytes to get the specified alignment are
 * zeroed.
 *
 * The format of the relocation entries referenced by the reloff and nreloc
 * fields of the section structure for mach object files is described in the
 * header file <reloc.h>.
 */
/// 32-bit section.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Section32<E: Endian> {
    /// name of this section
    pub sectname: [u8; 16],
    /// segment this section goes in
    pub segname: [u8; 16],
    /// memory address of this section
    pub addr: U32<E>,
    /// size in bytes of this section
    pub size: U32<E>,
    /// file offset of this section
    pub offset: U32<E>,
    /// section alignment (power of 2)
    pub align: U32<E>,
    /// file offset of relocation entries
    pub reloff: U32<E>,
    /// number of relocation entries
    pub nreloc: U32<E>,
    /// flags (section type and attributes)
    pub flags: U32<E, SectionFlags>,
    /// reserved (for offset or index)
    pub reserved1: U32<E>,
    /// reserved (for count or sizeof)
    pub reserved2: U32<E>,
}

/// 64-bit section.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Section64<E: Endian> {
    /// name of this section
    pub sectname: [u8; 16],
    /// segment this section goes in
    pub segname: [u8; 16],
    /// memory address of this section
    pub addr: U64<E>,
    /// size in bytes of this section
    pub size: U64<E>,
    /// file offset of this section
    pub offset: U32<E>,
    /// section alignment (power of 2)
    pub align: U32<E>,
    /// file offset of relocation entries
    pub reloff: U32<E>,
    /// number of relocation entries
    pub nreloc: U32<E>,
    /// flags (section type and attributes)
    pub flags: U32<E, SectionFlags>,
    /// reserved (for offset or index)
    pub reserved1: U32<E>,
    /// reserved (for count or sizeof)
    pub reserved2: U32<E>,
    /// reserved
    pub reserved3: U32<E>,
}

/*
 * The flags field of a section structure is separated into two parts a section
 * type and section attributes.  The section types are mutually exclusive (it
 * can only have one type) but the section attributes are not (it may have more
 * than one attribute).
 */
/// 256 section types
pub const SECTION_TYPE: u32 = 0x0000_00ff;
/// 24 section attributes
pub const SECTION_ATTRIBUTES: u32 = 0xffff_ff00;

newtype!(
    /// Values for `Section*::flags`.
    struct SectionFlags(u32);
);

impl SectionFlags {
    /// Get the section type field.
    pub fn typ(self) -> SectionType {
        SectionType((self.0 & SECTION_TYPE) as u8)
    }

    /// Set the section type field.
    pub const fn with_type(self, typ: SectionType) -> SectionFlags {
        SectionFlags(self.0 & !SECTION_TYPE | typ.0 as u32)
    }
}

newtype!(
    /// Constants for the type of a section
    struct SectionType(u8);
);

impl SectionType {
    /// Convert to a `SectionFlags` with no attributes, for use in const expressions.
    pub const fn to_flags(self) -> SectionFlags {
        SectionFlags(self.0 as u32)
    }
}

impl From<SectionType> for SectionFlags {
    fn from(typ: SectionType) -> Self {
        SectionFlags(u32::from(typ.0))
    }
}

impl From<SectionFlags> for SectionType {
    fn from(flags: SectionFlags) -> Self {
        flags.typ()
    }
}

impl core::ops::BitOr<SectionFlags> for SectionType {
    type Output = SectionFlags;
    fn bitor(self, attrs: SectionFlags) -> SectionFlags {
        attrs.with_type(self)
    }
}

newtype_constant_names!(NAMES_S_TYPE: SectionType(u8) = {
    /// regular section
    S_REGULAR = 0x0,
    /// zero fill on demand section
    S_ZEROFILL = 0x1,
    /// section with only literal C strings
    S_CSTRING_LITERALS = 0x2,
    /// section with only 4 byte literals
    S_4BYTE_LITERALS = 0x3,
    /// section with only 8 byte literals
    S_8BYTE_LITERALS = 0x4,
    /// section with only pointers to literals
    S_LITERAL_POINTERS = 0x5,
    /*
     * For the two types of symbol pointers sections and the symbol stubs section
     * they have indirect symbol table entries.  For each of the entries in the
     * section the indirect symbol table entries, in corresponding order in the
     * indirect symbol table, start at the index stored in the reserved1 field
     * of the section structure.  Since the indirect symbol table entries
     * correspond to the entries in the section the number of indirect symbol table
     * entries is inferred from the size of the section divided by the size of the
     * entries in the section.  For symbol pointers sections the size of the entries
     * in the section is 4 bytes and for symbol stubs sections the byte size of the
     * stubs is stored in the reserved2 field of the section structure.
     */
    /// section with only non-lazy symbol pointers
    S_NON_LAZY_SYMBOL_POINTERS = 0x6,
    /// section with only lazy symbol pointers
    S_LAZY_SYMBOL_POINTERS = 0x7,
    /// section with only symbol stubs, byte size of stub in the reserved2 field
    S_SYMBOL_STUBS = 0x8,
    /// section with only function pointers for initialization
    S_MOD_INIT_FUNC_POINTERS = 0x9,
    /// section with only function pointers for termination
    S_MOD_TERM_FUNC_POINTERS = 0xa,
    /// section contains symbols that are to be coalesced
    S_COALESCED = 0xb,
    /// zero fill on demand section (that can be larger than 4 gigabytes)
    S_GB_ZEROFILL = 0xc,
    /// section with only pairs of function pointers for interposing
    S_INTERPOSING = 0xd,
    /// section with only 16 byte literals
    S_16BYTE_LITERALS = 0xe,
    /// section contains DTrace Object Format
    S_DTRACE_DOF = 0xf,
    /// section with only lazy symbol pointers to lazy loaded dylibs
    S_LAZY_DYLIB_SYMBOL_POINTERS = 0x10,
    /*
     * Section types to support thread local variables
     */
    /// template of initial values for TLVs
    S_THREAD_LOCAL_REGULAR = 0x11,
    /// template of initial values for TLVs
    S_THREAD_LOCAL_ZEROFILL = 0x12,
    /// TLV descriptors
    S_THREAD_LOCAL_VARIABLES = 0x13,
    /// pointers to TLV descriptors
    S_THREAD_LOCAL_VARIABLE_POINTERS = 0x14,
    /// functions to call to initialize TLV values
    S_THREAD_LOCAL_INIT_FUNCTION_POINTERS = 0x15,
    /// 32-bit offsets to initializers
    S_INIT_FUNC_OFFSETS = 0x16,
});

/*
 * Constants for the section attributes part of the flags field of a section
 * structure.
 */
newtype_flag_names!(NAMES_S: SectionFlags(u32) = {
    _ = SECTION_TYPE => NAMES_S_TYPE,
    /// section contains only true machine instructions
    S_ATTR_PURE_INSTRUCTIONS = 0x8000_0000,
    /// section contains coalesced symbols that are not to be in a ranlib table of contents
    S_ATTR_NO_TOC = 0x4000_0000,
    /// ok to strip static symbols in this section in files with the MH_DYLDLINK flag
    S_ATTR_STRIP_STATIC_SYMS = 0x2000_0000,
    /// no dead stripping
    S_ATTR_NO_DEAD_STRIP = 0x1000_0000,
    /// blocks are live if they reference live blocks
    S_ATTR_LIVE_SUPPORT = 0x0800_0000,
    /// Used with i386 code stubs written on by dyld
    S_ATTR_SELF_MODIFYING_CODE = 0x0400_0000,
    /*
     * If a segment contains any sections marked with S_ATTR_DEBUG then all
     * sections in that segment must have this attribute.  No section other than
     * a section marked with this attribute may reference the contents of this
     * section.  A section with this attribute may contain no symbols and must have
     * a section type S_REGULAR.  The static linker will not copy section contents
     * from sections with this attribute into its output file.  These sections
     * generally contain DWARF debugging info.
     */
    /// a debug section
    S_ATTR_DEBUG = 0x0200_0000,
    /// section contains some machine instructions
    S_ATTR_SOME_INSTRUCTIONS = 0x0000_0400,
    /// section has external relocation entries
    S_ATTR_EXT_RELOC = 0x0000_0200,
    /// section has local relocation entries
    S_ATTR_LOC_RELOC = 0x0000_0100,
});

/// User setable attributes
pub const SECTION_ATTRIBUTES_USR: u32 = 0xff00_0000;
/// system setable attributes
pub const SECTION_ATTRIBUTES_SYS: u32 = 0x00ff_ff00;

/*
 * The names of segments and sections in them are mostly meaningless to the
 * link-editor.  But there are few things to support traditional UNIX
 * executables that require the link-editor and assembler to use some names
 * agreed upon by convention.
 *
 * The initial protection of the "__TEXT" segment has write protection turned
 * off (not writeable).
 *
 * The link-editor will allocate common symbols at the end of the "__common"
 * section in the "__DATA" segment.  It will create the section and segment
 * if needed.
 */

/* The currently known segment names and the section names in those segments */

/// the pagezero segment which has no protections and catches NULL references for MH_EXECUTE files
pub const SEG_PAGEZERO: &str = "__PAGEZERO";

/// the tradition UNIX text segment
pub const SEG_TEXT: &str = "__TEXT";
/// the real text part of the text section no headers, and no padding
pub const SECT_TEXT: &str = "__text";
/// the fvmlib initialization section
pub const SECT_FVMLIB_INIT0: &str = "__fvmlib_init0";
/// the section following the fvmlib initialization section
pub const SECT_FVMLIB_INIT1: &str = "__fvmlib_init1";

/// the tradition UNIX data segment
pub const SEG_DATA: &str = "__DATA";
/// the real initialized data section no padding, no bss overlap
pub const SECT_DATA: &str = "__data";
/// the real uninitialized data section no padding
pub const SECT_BSS: &str = "__bss";
/// the section common symbols are allocated in by the link editor
pub const SECT_COMMON: &str = "__common";

/// objective-C runtime segment
pub const SEG_OBJC: &str = "__OBJC";
/// symbol table
pub const SECT_OBJC_SYMBOLS: &str = "__symbol_table";
/// module information
pub const SECT_OBJC_MODULES: &str = "__module_info";
/// string table
pub const SECT_OBJC_STRINGS: &str = "__selector_strs";
/// string table
pub const SECT_OBJC_REFS: &str = "__selector_refs";

/// the icon segment
pub const SEG_ICON: &str = "__ICON";
/// the icon headers
pub const SECT_ICON_HEADER: &str = "__header";
/// the icons in tiff format
pub const SECT_ICON_TIFF: &str = "__tiff";

/// the segment containing all structs created and maintained by the link editor.  Created with -seglinkedit option to ld(1) for MH_EXECUTE and FVMLIB file types only
pub const SEG_LINKEDIT: &str = "__LINKEDIT";

/// the segment overlapping with linkedit containing linking information
pub const SEG_LINKINFO: &str = "__LINKINFO";

/// the unix stack segment
pub const SEG_UNIXSTACK: &str = "__UNIXSTACK";

/// the segment for the self (dyld) modifying code stubs that has read, write and execute permissions
pub const SEG_IMPORT: &str = "__IMPORT";

/*
 * Fixed virtual memory shared libraries are identified by two things.  The
 * target pathname (the name of the library as found for execution), and the
 * minor version number.  The address of where the headers are loaded is in
 * header_addr. (THIS IS OBSOLETE and no longer supported).
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Fvmlib<E: Endian> {
    /// library's target pathname
    pub name: LcStr<E>,
    /// library's minor version number
    pub minor_version: U32<E>,
    /// library's header address
    pub header_addr: U32<E>,
}

/*
 * A fixed virtual shared library (filetype == MH_FVMLIB in the mach header)
 * contains a `FvmlibCommand` (cmd == LC_IDFVMLIB) to identify the library.
 * An object that uses a fixed virtual shared library also contains a
 * `FvmlibCommand` (cmd == LC_LOADFVMLIB) for each library it uses.
 * (THIS IS OBSOLETE and no longer supported).
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct FvmlibCommand<E: Endian> {
    /// LC_IDFVMLIB or LC_LOADFVMLIB
    pub cmd: U32<E, LoadCommandType>,
    /// includes pathname string
    pub cmdsize: U32<E>,
    /// the library identification
    pub fvmlib: Fvmlib<E>,
}

/*
 * Dynamically linked shared libraries are identified by two things.  The
 * pathname (the name of the library as found for execution), and the
 * compatibility version number.  The pathname must match and the compatibility
 * number in the user of the library must be greater than or equal to the
 * library being used.  The time stamp is used to record the time a library was
 * built and copied into user so it can be use to determined if the library used
 * at runtime is exactly the same as used to built the program.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Dylib<E: Endian> {
    /// library's path name
    pub name: LcStr<E>,
    /// library's build time stamp
    pub timestamp: U32<E>,
    /// library's current version number
    pub current_version: U32<E, Version>,
    /// library's compatibility vers number
    pub compatibility_version: U32<E, Version>,
}

/*
 * A dynamically linked shared library (filetype == MH_DYLIB in the mach header)
 * contains a `DylibCommand` (cmd == LC_ID_DYLIB) to identify the library.
 * An object that uses a dynamically linked shared library also contains a
 * `DylibCommand` (cmd == LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB, or
 * LC_REEXPORT_DYLIB) for each library it uses.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DylibCommand<E: Endian> {
    /// LC_ID_DYLIB, LC_LOAD_{,WEAK_}DYLIB, LC_REEXPORT_DYLIB
    pub cmd: U32<E, LoadCommandType>,
    /// includes pathname string
    pub cmdsize: U32<E>,
    /// the library identification
    pub dylib: Dylib<E>,
}

/*
 * An alternate encoding for: LC_LOAD_DYLIB.
 * The flags field contains independent flags DYLIB_USE_*
 * First supported in macOS 15, iOS 18.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DylibUseCommand<E: Endian> {
    /// LC_LOAD_DYLIB or LC_LOAD_WEAK_DYLIB
    pub cmd: U32<E, LoadCommandType>,
    /// overall size, including path
    pub cmdsize: U32<E>,
    /// == 28, dylibs's path offset
    pub nameoff: U32<E>,
    /// == DYLIB_USE_MARKER
    pub marker: U32<E>,
    /// dylib's current version number
    pub current_version: U32<E, Version>,
    /// dylib's compatibility version number
    pub compat_version: U32<E, Version>,
    /// DYLIB_USE_... flags
    pub flags: U32<E, DylibUseFlags>,
}

newtype!(
    /// Values for `DylibUseCommand::flags`.
    struct DylibUseFlags(u32);
);

newtype_flag_names!(NAMES_DYLIB_USE: DylibUseFlags(u32) = {
    DYLIB_USE_WEAK_LINK = 0x01,
    DYLIB_USE_REEXPORT = 0x02,
    DYLIB_USE_UPWARD = 0x04,
    DYLIB_USE_DELAYED_INIT = 0x08,
});

pub const DYLIB_USE_MARKER: u32 = 0x1a741800;

/*
 * A dynamically linked shared library may be a subframework of an umbrella
 * framework.  If so it will be linked with "-umbrella umbrella_name" where
 * Where "umbrella_name" is the name of the umbrella framework. A subframework
 * can only be linked against by its umbrella framework or other subframeworks
 * that are part of the same umbrella framework.  Otherwise the static link
 * editor produces an error and states to link against the umbrella framework.
 * The name of the umbrella framework for subframeworks is recorded in the
 * following structure.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct SubFrameworkCommand<E: Endian> {
    /// LC_SUB_FRAMEWORK
    pub cmd: U32<E, LoadCommandType>,
    /// includes umbrella string
    pub cmdsize: U32<E>,
    /// the umbrella framework name
    pub umbrella: LcStr<E>,
}

/*
 * For dynamically linked shared libraries that are subframework of an umbrella
 * framework they can allow clients other than the umbrella framework or other
 * subframeworks in the same umbrella framework.  To do this the subframework
 * is built with "-allowable_client client_name" and an LC_SUB_CLIENT load
 * command is created for each -allowable_client flag.  The client_name is
 * usually a framework name.  It can also be a name used for bundles clients
 * where the bundle is built with "-client_name client_name".
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct SubClientCommand<E: Endian> {
    /// LC_SUB_CLIENT
    pub cmd: U32<E, LoadCommandType>,
    /// includes client string
    pub cmdsize: U32<E>,
    /// the client name
    pub client: LcStr<E>,
}

/*
 * A dynamically linked shared library may be a sub_umbrella of an umbrella
 * framework.  If so it will be linked with "-sub_umbrella umbrella_name" where
 * Where "umbrella_name" is the name of the sub_umbrella framework.  When
 * statically linking when -twolevel_namespace is in effect a twolevel namespace
 * umbrella framework will only cause its subframeworks and those frameworks
 * listed as sub_umbrella frameworks to be implicited linked in.  Any other
 * dependent dynamic libraries will not be linked it when -twolevel_namespace
 * is in effect.  The primary library recorded by the static linker when
 * resolving a symbol in these libraries will be the umbrella framework.
 * Zero or more sub_umbrella frameworks may be use by an umbrella framework.
 * The name of a sub_umbrella framework is recorded in the following structure.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct SubUmbrellaCommand<E: Endian> {
    /// LC_SUB_UMBRELLA
    pub cmd: U32<E, LoadCommandType>,
    /// includes sub_umbrella string
    pub cmdsize: U32<E>,
    /// the sub_umbrella framework name
    pub sub_umbrella: LcStr<E>,
}

/*
 * A dynamically linked shared library may be a sub_library of another shared
 * library.  If so it will be linked with "-sub_library library_name" where
 * Where "library_name" is the name of the sub_library shared library.  When
 * statically linking when -twolevel_namespace is in effect a twolevel namespace
 * shared library will only cause its subframeworks and those frameworks
 * listed as sub_umbrella frameworks and libraries listed as sub_libraries to
 * be implicited linked in.  Any other dependent dynamic libraries will not be
 * linked it when -twolevel_namespace is in effect.  The primary library
 * recorded by the static linker when resolving a symbol in these libraries
 * will be the umbrella framework (or dynamic library). Zero or more sub_library
 * shared libraries may be use by an umbrella framework or (or dynamic library).
 * The name of a sub_library framework is recorded in the following structure.
 * For example /usr/lib/libobjc_profile.A.dylib would be recorded as "libobjc".
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct SubLibraryCommand<E: Endian> {
    /// LC_SUB_LIBRARY
    pub cmd: U32<E, LoadCommandType>,
    /// includes sub_library string
    pub cmdsize: U32<E>,
    /// the sub_library name
    pub sub_library: LcStr<E>,
}

/*
 * A program (filetype == MH_EXECUTE) that is
 * prebound to its dynamic libraries has one of these for each library that
 * the static linker used in prebinding.  It contains a bit vector for the
 * modules in the library.  The bits indicate which modules are bound (1) and
 * which are not (0) from the library.  The bit for module 0 is the low bit
 * of the first byte.  So the bit for the Nth module is:
 * (linked_modules[N/8] >> N%8) & 1
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct PreboundDylibCommand<E: Endian> {
    /// LC_PREBOUND_DYLIB
    pub cmd: U32<E, LoadCommandType>,
    /// includes strings
    pub cmdsize: U32<E>,
    /// library's path name
    pub name: LcStr<E>,
    /// number of modules in library
    pub nmodules: U32<E>,
    /// bit vector of linked modules
    pub linked_modules: LcStr<E>,
}

/*
 * A program that uses a dynamic linker contains a `DylinkerCommand` to identify
 * the name of the dynamic linker (LC_LOAD_DYLINKER).  And a dynamic linker
 * contains a `DylinkerCommand` to identify the dynamic linker (LC_ID_DYLINKER).
 * A file can have at most one of these.
 * This struct is also used for the LC_DYLD_ENVIRONMENT load command and
 * contains string for dyld to treat like environment variable.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DylinkerCommand<E: Endian> {
    /// LC_ID_DYLINKER, LC_LOAD_DYLINKER or LC_DYLD_ENVIRONMENT
    pub cmd: U32<E, LoadCommandType>,
    /// includes pathname string
    pub cmdsize: U32<E>,
    /// dynamic linker's path name
    pub name: LcStr<E>,
}

/*
 * Thread commands contain machine-specific data structures suitable for
 * use in the thread state primitives.  The machine specific data structures
 * follow the struct `ThreadCommand` as follows.
 * Each flavor of machine specific data structure is preceded by an uint32_t
 * constant for the flavor of that data structure, an uint32_t that is the
 * count of uint32_t's of the size of the state data structure and then
 * the state data structure follows.  This triple may be repeated for many
 * flavors.  The constants for the flavors, counts and state data structure
 * definitions are expected to be in the header file <machine/thread_status.h>.
 * These machine specific data structures sizes must be multiples of
 * 4 bytes.  The `cmdsize` reflects the total size of the `ThreadCommand`
 * and all of the sizes of the constants for the flavors, counts and state
 * data structures.
 *
 * For executable objects that are unix processes there will be one
 * `ThreadCommand` (cmd == LC_UNIXTHREAD) created for it by the link-editor.
 * This is the same as a LC_THREAD, except that a stack is automatically
 * created (based on the shell's limit for the stack size).  Command arguments
 * and environment variables are copied onto that stack.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ThreadCommand<E: Endian> {
    /// LC_THREAD or  LC_UNIXTHREAD
    pub cmd: U32<E, LoadCommandType>,
    /// total size of this command
    pub cmdsize: U32<E>,
    /* uint32_t flavor		   flavor of thread state */
    /* uint32_t count		   count of uint32_t's in thread state */
    /* struct XXX_thread_state state   thread state for this flavor */
    /* ... */
}

/*
 * The routines command contains the address of the dynamic shared library
 * initialization routine and an index into the module table for the module
 * that defines the routine.  Before any modules are used from the library the
 * dynamic linker fully binds the module that defines the initialization routine
 * and then calls it.  This gets called before any module initialization
 * routines (used for C++ static constructors) in the library.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct RoutinesCommand32<E: Endian> {
    /* for 32-bit architectures */
    /// LC_ROUTINES
    pub cmd: U32<E, LoadCommandType>,
    /// total size of this command
    pub cmdsize: U32<E>,
    /// address of initialization routine
    pub init_address: U32<E>,
    /// index into the module table that the init routine is defined in
    pub init_module: U32<E>,
    pub reserved1: U32<E>,
    pub reserved2: U32<E>,
    pub reserved3: U32<E>,
    pub reserved4: U32<E>,
    pub reserved5: U32<E>,
    pub reserved6: U32<E>,
}

/*
 * The 64-bit routines command.  Same use as above.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct RoutinesCommand64<E: Endian> {
    /* for 64-bit architectures */
    /// LC_ROUTINES_64
    pub cmd: U32<E, LoadCommandType>,
    /// total size of this command
    pub cmdsize: U32<E>,
    /// address of initialization routine
    pub init_address: U64<E>,
    /// index into the module table that the init routine is defined in
    pub init_module: U64<E>,
    pub reserved1: U64<E>,
    pub reserved2: U64<E>,
    pub reserved3: U64<E>,
    pub reserved4: U64<E>,
    pub reserved5: U64<E>,
    pub reserved6: U64<E>,
}

/*
 * The `SymtabCommand` contains the offsets and sizes of the link-edit 4.3BSD
 * "stab" style symbol table information as described in the header files
 * <nlist.h> and <stab.h>.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct SymtabCommand<E: Endian> {
    /// LC_SYMTAB
    pub cmd: U32<E, LoadCommandType>,
    /// sizeof(struct SymtabCommand)
    pub cmdsize: U32<E>,
    /// symbol table offset
    pub symoff: U32<E>,
    /// number of symbol table entries
    pub nsyms: U32<E>,
    /// string table offset
    pub stroff: U32<E>,
    /// string table size in bytes
    pub strsize: U32<E>,
}

/*
 * This is the second set of the symbolic information which is used to support
 * the data structures for the dynamically link editor.
 *
 * The original set of symbolic information in the `SymtabCommand` which contains
 * the symbol and string tables must also be present when this load command is
 * present.  When this load command is present the symbol table is organized
 * into three groups of symbols:
 *	local symbols (static and debugging symbols) - grouped by module
 *	defined external symbols - grouped by module (sorted by name if not lib)
 *	undefined external symbols (sorted by name if MH_BINDATLOAD is not set,
 *	     			    and in order the were seen by the static
 *				    linker if MH_BINDATLOAD is set)
 * In this load command there are offsets and counts to each of the three groups
 * of symbols.
 *
 * This load command contains a the offsets and sizes of the following new
 * symbolic information tables:
 *	table of contents
 *	module table
 *	reference symbol table
 *	indirect symbol table
 * The first three tables above (the table of contents, module table and
 * reference symbol table) are only present if the file is a dynamically linked
 * shared library.  For executable and object modules, which are files
 * containing only one module, the information that would be in these three
 * tables is determined as follows:
 * 	table of contents - the defined external symbols are sorted by name
 *	module table - the file contains only one module so everything in the
 *		       file is part of the module.
 *	reference symbol table - is the defined and undefined external symbols
 *
 * For dynamically linked shared library files this load command also contains
 * offsets and sizes to the pool of relocation entries for all sections
 * separated into two groups:
 *	external relocation entries
 *	local relocation entries
 * For executable and object modules the relocation entries continue to hang
 * off the section structures.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DysymtabCommand<E: Endian> {
    /// LC_DYSYMTAB
    pub cmd: U32<E, LoadCommandType>,
    /// sizeof(struct DysymtabCommand)
    pub cmdsize: U32<E>,

    /*
     * The symbols indicated by symoff and nsyms of the LC_SYMTAB load command
     * are grouped into the following three groups:
     *    local symbols (further grouped by the module they are from)
     *    defined external symbols (further grouped by the module they are from)
     *    undefined symbols
     *
     * The local symbols are used only for debugging.  The dynamic binding
     * process may have to use them to indicate to the debugger the local
     * symbols for a module that is being bound.
     *
     * The last two groups are used by the dynamic binding process to do the
     * binding (indirectly through the module table and the reference symbol
     * table when this is a dynamically linked shared library file).
     */
    /// index to local symbols
    pub ilocalsym: U32<E>,
    /// number of local symbols
    pub nlocalsym: U32<E>,

    /// index to externally defined symbols
    pub iextdefsym: U32<E>,
    /// number of externally defined symbols
    pub nextdefsym: U32<E>,

    /// index to undefined symbols
    pub iundefsym: U32<E>,
    /// number of undefined symbols
    pub nundefsym: U32<E>,

    /*
     * For the for the dynamic binding process to find which module a symbol
     * is defined in the table of contents is used (analogous to the ranlib
     * structure in an archive) which maps defined external symbols to modules
     * they are defined in.  This exists only in a dynamically linked shared
     * library file.  For executable and object modules the defined external
     * symbols are sorted by name and is use as the table of contents.
     */
    /// file offset to table of contents
    pub tocoff: U32<E>,
    /// number of entries in table of contents
    pub ntoc: U32<E>,

    /*
     * To support dynamic binding of "modules" (whole object files) the symbol
     * table must reflect the modules that the file was created from.  This is
     * done by having a module table that has indexes and counts into the merged
     * tables for each module.  The module structure that these two entries
     * refer to is described below.  This exists only in a dynamically linked
     * shared library file.  For executable and object modules the file only
     * contains one module so everything in the file belongs to the module.
     */
    /// file offset to module table
    pub modtaboff: U32<E>,
    /// number of module table entries
    pub nmodtab: U32<E>,

    /*
     * To support dynamic module binding the module structure for each module
     * indicates the external references (defined and undefined) each module
     * makes.  For each module there is an offset and a count into the
     * reference symbol table for the symbols that the module references.
     * This exists only in a dynamically linked shared library file.  For
     * executable and object modules the defined external symbols and the
     * undefined external symbols indicates the external references.
     */
    /// offset to referenced symbol table
    pub extrefsymoff: U32<E>,
    /// number of referenced symbol table entries
    pub nextrefsyms: U32<E>,

    /*
     * The sections that contain "symbol pointers" and "routine stubs" have
     * indexes and (implied counts based on the size of the section and fixed
     * size of the entry) into the "indirect symbol" table for each pointer
     * and stub.  For every section of these two types the index into the
     * indirect symbol table is stored in the section header in the field
     * reserved1.  An indirect symbol table entry is simply a 32bit index into
     * the symbol table to the symbol that the pointer or stub is referring to.
     * The indirect symbol table is ordered to match the entries in the section.
     */
    /// file offset to the indirect symbol table
    pub indirectsymoff: U32<E>,
    /// number of indirect symbol table entries
    pub nindirectsyms: U32<E>,

    /*
     * To support relocating an individual module in a library file quickly the
     * external relocation entries for each module in the library need to be
     * accessed efficiently.  Since the relocation entries can't be accessed
     * through the section headers for a library file they are separated into
     * groups of local and external entries further grouped by module.  In this
     * case the presents of this load command who's extreloff, nextrel,
     * locreloff and nlocrel fields are non-zero indicates that the relocation
     * entries of non-merged sections are not referenced through the section
     * structures (and the reloff and nreloc fields in the section headers are
     * set to zero).
     *
     * Since the relocation entries are not accessed through the section headers
     * this requires the r_address field to be something other than a section
     * offset to identify the item to be relocated.  In this case r_address is
     * set to the offset from the vmaddr of the first LC_SEGMENT command.
     * For MH_SPLIT_SEGS images r_address is set to the the offset from the
     * vmaddr of the first read-write LC_SEGMENT command.
     *
     * The relocation entries are grouped by module and the module table
     * entries have indexes and counts into them for the group of external
     * relocation entries for that the module.
     *
     * For sections that are merged across modules there must not be any
     * remaining external relocation entries for them (for merged sections
     * remaining relocation entries must be local).
     */
    /// offset to external relocation entries
    pub extreloff: U32<E>,
    /// number of external relocation entries
    pub nextrel: U32<E>,

    /*
     * All the local relocation entries are grouped together (they are not
     * grouped by their module since they are only used if the object is moved
     * from it statically link edited address).
     */
    /// offset to local relocation entries
    pub locreloff: U32<E>,
    /// number of local relocation entries
    pub nlocrel: U32<E>,
}

/*
 * An indirect symbol table entry is simply a 32bit index into the symbol table
 * to the symbol that the pointer or stub is referring to.  Unless it is for a
 * non-lazy symbol pointer section for a defined symbol which strip(1) as
 * removed.  In which case it has the value INDIRECT_SYMBOL_LOCAL.  If the
 * symbol was also absolute INDIRECT_SYMBOL_ABS is or'ed with that.
 */
newtype!(
    /// An entry in the indirect symbol table.
    ///
    /// Either a symbol table index or a sentinel with the LOCAL/ABS flags set.
    struct IndirectSymbol(u32);
);

newtype_flag_names!(NAMES_INDIRECT_SYMBOL: IndirectSymbol(u32) = {
    INDIRECT_SYMBOL_LOCAL = 0x8000_0000,
    INDIRECT_SYMBOL_ABS = 0x4000_0000,
});

impl IndirectSymbol {
    /// Whether this is a local symbol.
    pub fn is_local(self) -> bool {
        self.contains(INDIRECT_SYMBOL_LOCAL)
    }

    /// Whether this is an absolute symbol.
    pub fn is_abs(self) -> bool {
        self.contains(INDIRECT_SYMBOL_ABS)
    }

    /// Returns the symbol table index, or `None` if the symbol is local or absolute.
    pub fn index(self) -> Option<u32> {
        if self.intersects(INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS) {
            None
        } else {
            Some(self.0)
        }
    }
}

/* a table of contents entry */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DylibTableOfContents<E: Endian> {
    /// the defined external symbol (index into the symbol table)
    pub symbol_index: U32<E>,
    /// index into the module table this symbol is defined in
    pub module_index: U32<E>,
}

/* a module table entry */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DylibModule32<E: Endian> {
    /// the module name (index into string table)
    pub module_name: U32<E>,

    /// index into externally defined symbols
    pub iextdefsym: U32<E>,
    /// number of externally defined symbols
    pub nextdefsym: U32<E>,
    /// index into reference symbol table
    pub irefsym: U32<E>,
    /// number of reference symbol table entries
    pub nrefsym: U32<E>,
    /// index into symbols for local symbols
    pub ilocalsym: U32<E>,
    /// number of local symbols
    pub nlocalsym: U32<E>,

    /// index into external relocation entries
    pub iextrel: U32<E>,
    /// number of external relocation entries
    pub nextrel: U32<E>,

    /// low 16 bits are the index into the init section, high 16 bits are the index into the term section
    pub iinit_iterm: U32<E>,
    /// low 16 bits are the number of init section entries, high 16 bits are the number of term section entries
    pub ninit_nterm: U32<E>,

    /// for this module address of the start of the (__OBJC,__module_info) section
    pub objc_module_info_addr: U32<E>,
    /// for this module size of the (__OBJC,__module_info) section
    pub objc_module_info_size: U32<E>,
}

/* a 64-bit module table entry */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DylibModule64<E: Endian> {
    /// the module name (index into string table)
    pub module_name: U32<E>,

    /// index into externally defined symbols
    pub iextdefsym: U32<E>,
    /// number of externally defined symbols
    pub nextdefsym: U32<E>,
    /// index into reference symbol table
    pub irefsym: U32<E>,
    /// number of reference symbol table entries
    pub nrefsym: U32<E>,
    /// index into symbols for local symbols
    pub ilocalsym: U32<E>,
    /// number of local symbols
    pub nlocalsym: U32<E>,

    /// index into external relocation entries
    pub iextrel: U32<E>,
    /// number of external relocation entries
    pub nextrel: U32<E>,

    /// low 16 bits are the index into the init section, high 16 bits are the index into the term section
    pub iinit_iterm: U32<E>,
    /// low 16 bits are the number of init section entries, high 16 bits are the number of term section entries
    pub ninit_nterm: U32<E>,

    /// for this module size of the (__OBJC,__module_info) section
    pub objc_module_info_size: U32<E>,
    /// for this module address of the start of the (__OBJC,__module_info) section
    pub objc_module_info_addr: U64<E>,
}

/*
 * The entries in the reference symbol table are used when loading the module
 * (both by the static and dynamic link editors) and if the module is unloaded
 * or replaced.  Therefore all external symbols (defined and undefined) are
 * listed in the module's reference table.  The flags describe the type of
 * reference that is being made.  The constants for the flags are defined in
 * <mach-o/nlist.h> as they are also used for symbol table entries.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DylibReference<E: Endian> {
    /* TODO:
    uint32_t isym:24,		/* index into the symbol table */
              flags:8;	/* flags to indicate the type of reference */
    */
    pub bitfield: U32<E>,
}

/*
 * The TwolevelHintsCommand contains the offset and number of hints in the
 * two-level namespace lookup hints table.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct TwolevelHintsCommand<E: Endian> {
    /// LC_TWOLEVEL_HINTS
    pub cmd: U32<E, LoadCommandType>,
    /// sizeof(struct TwolevelHintsCommand)
    pub cmdsize: U32<E>,
    /// offset to the hint table
    pub offset: U32<E>,
    /// number of hints in the hint table
    pub nhints: U32<E>,
}

/*
 * The entries in the two-level namespace lookup hints table are TwolevelHint
 * structs.  These provide hints to the dynamic link editor where to start
 * looking for an undefined symbol in a two-level namespace image.  The
 * isub_image field is an index into the sub-images (sub-frameworks and
 * sub-umbrellas list) that made up the two-level image that the undefined
 * symbol was found in when it was built by the static link editor.  If
 * isub-image is 0 the the symbol is expected to be defined in library and not
 * in the sub-images.  If isub-image is non-zero it is an index into the array
 * of sub-images for the umbrella with the first index in the sub-images being
 * 1. The array of sub-images is the ordered list of sub-images of the umbrella
 * that would be searched for a symbol that has the umbrella recorded as its
 * primary library.  The table of contents index is an index into the
 * library's table of contents.  This is used as the starting point of the
 * binary search or a directed linear search.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct TwolevelHint<E: Endian> {
    /* TODO:
    uint32_t
    isub_image:8,	/* index into the sub images */
    itoc:24;	/* index into the table of contents */
    */
    pub bitfield: U32<E>,
}

/*
 * The PrebindCksumCommand contains the value of the original check sum for
 * prebound files or zero.  When a prebound file is first created or modified
 * for other than updating its prebinding information the value of the check sum
 * is set to zero.  When the file has it prebinding re-done and if the value of
 * the check sum is zero the original check sum is calculated and stored in
 * cksum field of this load command in the output file.  If when the prebinding
 * is re-done and the cksum field is non-zero it is left unchanged from the
 * input file.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct PrebindCksumCommand<E: Endian> {
    /// LC_PREBIND_CKSUM
    pub cmd: U32<E, LoadCommandType>,
    /// sizeof(struct PrebindCksumCommand)
    pub cmdsize: U32<E>,
    /// the check sum or zero
    pub cksum: U32<E>,
}

/*
 * The uuid load command contains a single 128-bit unique random number that
 * identifies an object produced by the static link editor.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct UuidCommand<E: Endian> {
    /// LC_UUID
    pub cmd: U32<E, LoadCommandType>,
    /// sizeof(struct UuidCommand)
    pub cmdsize: U32<E>,
    /// the 128-bit uuid
    pub uuid: [u8; 16],
}

/*
 * The RpathCommand contains a path which at runtime should be added to
 * the current run path used to find @rpath prefixed dylibs.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct RpathCommand<E: Endian> {
    /// LC_RPATH
    pub cmd: U32<E, LoadCommandType>,
    /// includes string
    pub cmdsize: U32<E>,
    /// path to add to run path
    pub path: LcStr<E>,
}

/*
 * The target_triple_command contains a string which specifies the
 * target triple (e.g. "arm64e-apple-macosx15.0.0") used to compile the code.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct TargetTripleCommand<E: Endian> {
    /// LC_TARGET_TRIPLE
    pub cmd: U32<E, LoadCommandType>,
    /// including string
    pub cmdsize: U32<E>,
    /// target triple string
    pub triple: LcStr<E>,
}

/*
 * The LinkeditDataCommand contains the offsets and sizes of a blob
 * of data in the __LINKEDIT segment.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct LinkeditDataCommand<E: Endian> {
    /// `LC_CODE_SIGNATURE`, `LC_SEGMENT_SPLIT_INFO`, `LC_FUNCTION_STARTS`,
    /// `LC_DATA_IN_CODE`, `LC_DYLIB_CODE_SIGN_DRS`, `LC_LINKER_OPTIMIZATION_HINT`,
    /// `LC_DYLD_EXPORTS_TRIE`, or `LC_DYLD_CHAINED_FIXUPS`.
    pub cmd: U32<E, LoadCommandType>,
    /// sizeof(struct LinkeditDataCommand)
    pub cmdsize: U32<E>,
    /// file offset of data in __LINKEDIT segment
    pub dataoff: U32<E>,
    /// file size of data in __LINKEDIT segment
    pub datasize: U32<E>,
}

/*
 * The EncryptionInfoCommand32 contains the file offset and size of an
 * of an encrypted segment.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct EncryptionInfoCommand32<E: Endian> {
    /// LC_ENCRYPTION_INFO
    pub cmd: U32<E, LoadCommandType>,
    /// sizeof(struct EncryptionInfoCommand32)
    pub cmdsize: U32<E>,
    /// file offset of encrypted range
    pub cryptoff: U32<E>,
    /// file size of encrypted range
    pub cryptsize: U32<E>,
    /// which enryption system, 0 means not-encrypted yet
    pub cryptid: U32<E>,
}

/*
 * The EncryptionInfoCommand64 contains the file offset and size of an
 * of an encrypted segment (for use in x86_64 targets).
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct EncryptionInfoCommand64<E: Endian> {
    /// LC_ENCRYPTION_INFO_64
    pub cmd: U32<E, LoadCommandType>,
    /// sizeof(struct EncryptionInfoCommand64)
    pub cmdsize: U32<E>,
    /// file offset of encrypted range
    pub cryptoff: U32<E>,
    /// file size of encrypted range
    pub cryptsize: U32<E>,
    /// which enryption system, 0 means not-encrypted yet
    pub cryptid: U32<E>,
    /// padding to make this struct's size a multiple of 8 bytes
    pub pad: U32<E>,
}

/*
 * The VersionMinCommand contains the min OS version on which this
 * binary was built to run.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct VersionMinCommand<E: Endian> {
    /// LC_VERSION_MIN_MACOSX or LC_VERSION_MIN_IPHONEOS or LC_VERSION_MIN_WATCHOS or LC_VERSION_MIN_TVOS
    pub cmd: U32<E, LoadCommandType>,
    /// sizeof(struct VersionMinCommand)
    pub cmdsize: U32<E>,
    /// X.Y.Z is encoded in nibbles xxxx.yy.zz
    pub version: U32<E, Version>,
    /// X.Y.Z is encoded in nibbles xxxx.yy.zz
    pub sdk: U32<E, Version>,
}

/*
 * The BuildVersionCommand contains the min OS version on which this
 * binary was built to run for its platform.  The list of known platforms and
 * tool values following it.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct BuildVersionCommand<E: Endian> {
    /// LC_BUILD_VERSION
    pub cmd: U32<E, LoadCommandType>,
    /// sizeof(struct BuildVersionCommand) plus ntools * sizeof(struct BuildToolVersion)
    pub cmdsize: U32<E>,
    /// platform
    pub platform: U32<E, Platform>,
    /// X.Y.Z is encoded in nibbles xxxx.yy.zz
    pub minos: U32<E, Version>,
    /// X.Y.Z is encoded in nibbles xxxx.yy.zz
    pub sdk: U32<E, Version>,
    /// number of tool entries following this
    pub ntools: U32<E>,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct BuildToolVersion<E: Endian> {
    /// enum for the tool
    pub tool: U32<E, Tool>,
    /// version number of the tool
    pub version: U32<E, Version>,
}

newtype!(
    struct Version(u32);
);

impl Version {
    pub fn new(major: u16, minor: u8, update: u8) -> Self {
        Version(u32::from(major) << 16 | u32::from(minor) << 8 | u32::from(update))
    }

    pub fn major(self) -> u16 {
        (self.0 >> 16) as u16
    }

    pub fn minor(self) -> u8 {
        (self.0 >> 8) as u8
    }

    pub fn update(self) -> u8 {
        self.0 as u8
    }
}

impl core::fmt::Debug for Version {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "{}.{}.{}", self.major(), self.minor(), self.update())
    }
}

impl core::fmt::Display for Version {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "{}.{}.{}", self.major(), self.minor(), self.update())
    }
}

newtype!(
    /// Value for `BuildVersionCommand::platform`.
    struct Platform(u32);
);

newtype_constant_names!(NAMES_PLATFORM: Platform(u32) = {
    PLATFORM_UNKNOWN = 0,
    PLATFORM_ANY = 0xFFFFFFFF,
    PLATFORM_MACOS = 1,
    PLATFORM_IOS = 2,
    PLATFORM_TVOS = 3,
    PLATFORM_WATCHOS = 4,
    PLATFORM_BRIDGEOS = 5,
    PLATFORM_MACCATALYST = 6,
    PLATFORM_IOSSIMULATOR = 7,
    PLATFORM_TVOSSIMULATOR = 8,
    PLATFORM_WATCHOSSIMULATOR = 9,
    PLATFORM_DRIVERKIT = 10,
    PLATFORM_VISIONOS = 11,
    PLATFORM_VISIONOSSIMULATOR = 12,
    /// Compatibility alias for [`PLATFORM_VISIONOS`].
    PLATFORM_XROS = PLATFORM_VISIONOS.0,
    /// Compatibility alias for [`PLATFORM_VISIONOSSIMULATOR`].
    PLATFORM_XROSSIMULATOR = PLATFORM_VISIONOSSIMULATOR.0,

    PLATFORM_FIRMWARE = 13,
    PLATFORM_SEPOS = 14,

    PLATFORM_MACOS_EXCLAVECORE = 15,
    PLATFORM_MACOS_EXCLAVEKIT = 16,
    PLATFORM_IOS_EXCLAVECORE = 17,
    PLATFORM_IOS_EXCLAVEKIT = 18,
    PLATFORM_TVOS_EXCLAVECORE = 19,
    PLATFORM_TVOS_EXCLAVEKIT = 20,
    PLATFORM_WATCHOS_EXCLAVECORE = 21,
    PLATFORM_WATCHOS_EXCLAVEKIT = 22,
    PLATFORM_VISIONOS_EXCLAVECORE = 23,
    PLATFORM_VISIONOS_EXCLAVEKIT = 24,
});

newtype!(
    /// Value for `BuildToolVersion::tool`.
    struct Tool(u32);
);

newtype_constant_names!(NAMES_TOOL: Tool(u32) = {
    TOOL_CLANG = 1,
    TOOL_SWIFT = 2,
    TOOL_LD = 3,

    /* values for gpu tools (1024 to 1048) */
    TOOL_METAL = 1024,
    TOOL_AIRLLD = 1025,
    TOOL_AIRNT = 1026,
    TOOL_AIRNT_PLUGIN = 1027,
    TOOL_AIRPACK = 1028,
    TOOL_GPUARCHIVER = 1031,
    TOOL_METAL_FRAMEWORK = 1032,
});

/*
 * The DyldInfoCommand contains the file offsets and sizes of
 * the new compressed form of the information dyld needs to
 * load the image.  This information is used by dyld on Mac OS X
 * 10.6 and later.  All information pointed to by this command
 * is encoded using byte streams, so no endian swapping is needed
 * to interpret it.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DyldInfoCommand<E: Endian> {
    /// LC_DYLD_INFO or LC_DYLD_INFO_ONLY
    pub cmd: U32<E, LoadCommandType>,
    /// sizeof(struct DyldInfoCommand)
    pub cmdsize: U32<E>,

    /*
     * Dyld rebases an image whenever dyld loads it at an address different
     * from its preferred address.  The rebase information is a stream
     * of byte sized opcodes whose symbolic names start with REBASE_OPCODE_.
     * Conceptually the rebase information is a table of tuples:
     *    <seg-index, seg-offset, type>
     * The opcodes are a compressed way to encode the table by only
     * encoding when a column changes.  In addition simple patterns
     * like "every n'th offset for m times" can be encoded in a few
     * bytes.
     */
    /// file offset to rebase info
    pub rebase_off: U32<E>,
    /// size of rebase info
    pub rebase_size: U32<E>,

    /*
     * Dyld binds an image during the loading process, if the image
     * requires any pointers to be initialized to symbols in other images.
     * The bind information is a stream of byte sized
     * opcodes whose symbolic names start with BIND_OPCODE_.
     * Conceptually the bind information is a table of tuples:
     *    <seg-index, seg-offset, type, symbol-library-ordinal, symbol-name, addend>
     * The opcodes are a compressed way to encode the table by only
     * encoding when a column changes.  In addition simple patterns
     * like for runs of pointers initialized to the same value can be
     * encoded in a few bytes.
     */
    /// file offset to binding info
    pub bind_off: U32<E>,
    /// size of binding info
    pub bind_size: U32<E>,

    /*
     * Some C++ programs require dyld to unique symbols so that all
     * images in the process use the same copy of some code/data.
     * This step is done after binding. The content of the weak_bind
     * info is an opcode stream like the bind_info.  But it is sorted
     * alphabetically by symbol name.  This enable dyld to walk
     * all images with weak binding information in order and look
     * for collisions.  If there are no collisions, dyld does
     * no updating.  That means that some fixups are also encoded
     * in the bind_info.  For instance, all calls to "operator new"
     * are first bound to libstdc++.dylib using the information
     * in bind_info.  Then if some image overrides operator new
     * that is detected when the weak_bind information is processed
     * and the call to operator new is then rebound.
     */
    /// file offset to weak binding info
    pub weak_bind_off: U32<E>,
    /// size of weak binding info
    pub weak_bind_size: U32<E>,

    /*
     * Some uses of external symbols do not need to be bound immediately.
     * Instead they can be lazily bound on first use.  The lazy_bind
     * are contains a stream of BIND opcodes to bind all lazy symbols.
     * Normal use is that dyld ignores the lazy_bind section when
     * loading an image.  Instead the static linker arranged for the
     * lazy pointer to initially point to a helper function which
     * pushes the offset into the lazy_bind area for the symbol
     * needing to be bound, then jumps to dyld which simply adds
     * the offset to lazy_bind_off to get the information on what
     * to bind.
     */
    /// file offset to lazy binding info
    pub lazy_bind_off: U32<E>,
    /// size of lazy binding infs
    pub lazy_bind_size: U32<E>,

    /*
     * The symbols exported by a dylib are encoded in a trie.  This
     * is a compact representation that factors out common prefixes.
     * It also reduces LINKEDIT pages in RAM because it encodes all
     * information (name, address, flags) in one small, contiguous range.
     * The export area is a stream of nodes.  The first node sequentially
     * is the start node for the trie.
     *
     * Nodes for a symbol start with a uleb128 that is the length of
     * the exported symbol information for the string so far.
     * If there is no exported symbol, the node starts with a zero byte.
     * If there is exported info, it follows the length.
     *
     * First is a uleb128 containing flags. Normally, it is followed by
     * a uleb128 encoded offset which is location of the content named
     * by the symbol from the mach_header for the image.  If the flags
     * is EXPORT_SYMBOL_FLAGS_REEXPORT, then following the flags is
     * a uleb128 encoded library ordinal, then a zero terminated
     * UTF8 string.  If the string is zero length, then the symbol
     * is re-export from the specified dylib with the same name.
     * If the flags is EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER, then following
     * the flags is two uleb128s: the stub offset and the resolver offset.
     * The stub is used by non-lazy pointers.  The resolver is used
     * by lazy pointers and must be called to get the actual address to use.
     *
     * After the optional exported symbol information is a byte of
     * how many edges (0-255) that this node has leaving it,
     * followed by each edge.
     * Each edge is a zero terminated UTF8 of the addition chars
     * in the symbol, followed by a uleb128 offset for the node that
     * edge points to.
     *
     */
    /// file offset to lazy binding info
    pub export_off: U32<E>,
    /// size of lazy binding infs
    pub export_size: U32<E>,
}

/*
 * The following are used to encode rebasing information
 */

newtype!(
    struct RebaseType(u8);
);

newtype_constant_names!(NAMES_REBASE_TYPE: RebaseType(u8) = {
    REBASE_TYPE_POINTER = 1,
    REBASE_TYPE_TEXT_ABSOLUTE32 = 2,
    REBASE_TYPE_TEXT_PCREL32 = 3,
});

newtype!(
    struct RebaseOpcode(u8);
);

pub const REBASE_OPCODE_MASK: u8 = 0xF0;
pub const REBASE_IMMEDIATE_MASK: u8 = 0x0F;

newtype_constant_names!(NAMES_REBASE_OPCODE: RebaseOpcode(u8) = {
    REBASE_OPCODE_DONE = 0x00,
    REBASE_OPCODE_SET_TYPE_IMM = 0x10,
    REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB = 0x20,
    REBASE_OPCODE_ADD_ADDR_ULEB = 0x30,
    REBASE_OPCODE_ADD_ADDR_IMM_SCALED = 0x40,
    REBASE_OPCODE_DO_REBASE_IMM_TIMES = 0x50,
    REBASE_OPCODE_DO_REBASE_ULEB_TIMES = 0x60,
    REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB = 0x70,
    REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB = 0x80,
});

/*
 * The following are used to encode binding information
 */

newtype!(
    struct BindType(u8);
);

newtype_constant_names!(NAMES_BIND_TYPE: BindType(u8) = {
    BIND_TYPE_POINTER = 1,
    BIND_TYPE_TEXT_ABSOLUTE32 = 2,
    BIND_TYPE_TEXT_PCREL32 = 3,
});

newtype!(
    /// The library ordinal for a bind.
    ///
    /// A positive value is the 1-based index of the dylib containing the symbol.
    /// Zero and negative values are the special `BIND_SPECIAL_DYLIB_*` constants.
    struct BindDylib(i32);
);

impl BindDylib {
    /// Whether this is a reserved constant, rather than a library ordinal.
    pub fn is_special(self) -> bool {
        self.0 <= 0
    }

    /// Get the library ordinal.
    ///
    /// Returns `None` for reserved constants.
    pub fn index(self) -> Option<u32> {
        if self.0 <= 0 {
            None
        } else {
            Some(self.0 as u32)
        }
    }
}

newtype_constant_names!(NAMES_BIND_DYLIB: BindDylib(i32) = {
    BIND_SPECIAL_DYLIB_SELF = 0,
    BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE = -1,
    BIND_SPECIAL_DYLIB_FLAT_LOOKUP = -2,
    BIND_SPECIAL_DYLIB_WEAK_LOOKUP = -3,
});

newtype!(
    struct BindSymbolFlags(u8);
);

newtype_flag_names!(NAMES_BIND_SYMBOL_FLAGS: BindSymbolFlags(u8) = {
    BIND_SYMBOL_FLAGS_WEAK_IMPORT = 0x1,
    BIND_SYMBOL_FLAGS_NON_WEAK_DEFINITION = 0x8,
});

newtype!(
    struct BindOpcode(u8);
);

pub const BIND_OPCODE_MASK: u8 = 0xF0;
pub const BIND_IMMEDIATE_MASK: u8 = 0x0F;

newtype_constant_names!(NAMES_BIND_OPCODE: BindOpcode(u8) = {
    BIND_OPCODE_DONE = 0x00,
    BIND_OPCODE_SET_DYLIB_ORDINAL_IMM = 0x10,
    BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB = 0x20,
    BIND_OPCODE_SET_DYLIB_SPECIAL_IMM = 0x30,
    BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM = 0x40,
    BIND_OPCODE_SET_TYPE_IMM = 0x50,
    BIND_OPCODE_SET_ADDEND_SLEB = 0x60,
    BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB = 0x70,
    BIND_OPCODE_ADD_ADDR_ULEB = 0x80,
    BIND_OPCODE_DO_BIND = 0x90,
    BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB = 0xA0,
    BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED = 0xB0,
    BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB = 0xC0,
    BIND_OPCODE_THREADED = 0xD0,
});

newtype!(
    struct BindSubopcodeThreaded(u8);
);

newtype_constant_names!(NAMES_BIND_SUBOPCODE_THREADED: BindSubopcodeThreaded(u8) = {
    BIND_SUBOPCODE_THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB = 0x00,
    BIND_SUBOPCODE_THREADED_APPLY = 0x01,
});

/*
 * The following are used on the flags byte of a terminal node
 * in the export information.
 */
newtype!(
    struct ExportSymbolKind(u8);
);

newtype_constant_names!(NAMES_EXPORT_SYMBOL_KIND: ExportSymbolKind(u8) = {
    EXPORT_SYMBOL_FLAGS_KIND_REGULAR = 0x00,
    EXPORT_SYMBOL_FLAGS_KIND_THREAD_LOCAL = 0x01,
    EXPORT_SYMBOL_FLAGS_KIND_ABSOLUTE = 0x02,
});

newtype!(
    struct ExportSymbolFlags(u64);
);

newtype_flag_names!(NAMES_EXPORT_SYMBOL_FLAGS: ExportSymbolFlags(u64) = {
    EXPORT_SYMBOL_FLAGS_KIND_MASK = 0x03 => NAMES_EXPORT_SYMBOL_KIND,
    EXPORT_SYMBOL_FLAGS_WEAK_DEFINITION = 0x04,
    EXPORT_SYMBOL_FLAGS_REEXPORT = 0x08,
    EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER = 0x10,
    EXPORT_SYMBOL_FLAGS_STATIC_RESOLVER = 0x20,
});

impl ExportSymbolFlags {
    /// Whether any unknown bits are set in the flags.
    pub fn has_unknown_bits(self) -> bool {
        self.0 & !0x3f != 0
    }

    /// Get the kind subfield.
    pub fn kind(self) -> ExportSymbolKind {
        ExportSymbolKind((self.0 & EXPORT_SYMBOL_FLAGS_KIND_MASK) as u8)
    }

    /// Set the kind subfield.
    pub fn with_kind(self, kind: ExportSymbolKind) -> ExportSymbolFlags {
        ExportSymbolFlags(
            self.0 & !EXPORT_SYMBOL_FLAGS_KIND_MASK
                | u64::from(kind.0) & EXPORT_SYMBOL_FLAGS_KIND_MASK,
        )
    }
}

impl From<ExportSymbolKind> for ExportSymbolFlags {
    fn from(value: ExportSymbolKind) -> Self {
        ExportSymbolFlags(u64::from(value.0))
    }
}

impl From<ExportSymbolFlags> for ExportSymbolKind {
    fn from(value: ExportSymbolFlags) -> Self {
        value.kind()
    }
}

impl core::ops::BitOr<ExportSymbolFlags> for ExportSymbolKind {
    type Output = ExportSymbolFlags;
    fn bitor(self, flags: ExportSymbolFlags) -> ExportSymbolFlags {
        flags.with_kind(self)
    }
}

/*
 * The LinkerOptionCommand contains linker options embedded in object files.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct LinkerOptionCommand<E: Endian> {
    /// LC_LINKER_OPTION only used in MH_OBJECT filetypes
    pub cmd: U32<E, LoadCommandType>,
    pub cmdsize: U32<E>,
    /// number of strings
    pub count: U32<E>,
    /* concatenation of zero terminated UTF8 strings.
    Zero filled at end to align */
}

/*
 * The SymsegCommand contains the offset and size of the GNU style
 * symbol table information as described in the header file <symseg.h>.
 * The symbol roots of the symbol segments must also be aligned properly
 * in the file.  So the requirement of keeping the offsets aligned to a
 * multiple of a 4 bytes translates to the length field of the symbol
 * roots also being a multiple of a long.  Also the padding must again be
 * zeroed. (THIS IS OBSOLETE and no longer supported).
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct SymsegCommand<E: Endian> {
    /// LC_SYMSEG
    pub cmd: U32<E, LoadCommandType>,
    /// sizeof(struct SymsegCommand)
    pub cmdsize: U32<E>,
    /// symbol segment offset
    pub offset: U32<E>,
    /// symbol segment size in bytes
    pub size: U32<E>,
}

/*
 * The IdentCommand contains a free format string table following the
 * IdentCommand structure.  The strings are null terminated and the size of
 * the command is padded out with zero bytes to a multiple of 4 bytes/
 * (THIS IS OBSOLETE and no longer supported).
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct IdentCommand<E: Endian> {
    /// LC_IDENT
    pub cmd: U32<E, LoadCommandType>,
    /// strings that follow this command
    pub cmdsize: U32<E>,
}

/*
 * The FvmfileCommand contains a reference to a file to be loaded at the
 * specified virtual address.  (Presently, this command is reserved for
 * internal use.  The kernel ignores this command when loading a program into
 * memory).
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct FvmfileCommand<E: Endian> {
    /// LC_FVMFILE
    pub cmd: U32<E, LoadCommandType>,
    /// includes pathname string
    pub cmdsize: U32<E>,
    /// files pathname
    pub name: LcStr<E>,
    /// files virtual address
    pub header_addr: U32<E>,
}

/*
 * The EntryPointCommand is a replacement for thread_command.
 * It is used for main executables to specify the location (file offset)
 * of main().  If -stack_size was used at link time, the stacksize
 * field will contain the stack size need for the main thread.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct EntryPointCommand<E: Endian> {
    /// LC_MAIN only used in MH_EXECUTE filetypes
    pub cmd: U32<E, LoadCommandType>,
    /// 24
    pub cmdsize: U32<E>,
    /// file (__TEXT) offset of main()
    pub entryoff: U64<E>,
    /// if not zero, initial stack size
    pub stacksize: U64<E>,
}

/*
 * The SourceVersionCommand is an optional load command containing
 * the version of the sources used to build the binary.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct SourceVersionCommand<E: Endian> {
    /// LC_SOURCE_VERSION
    pub cmd: U32<E, LoadCommandType>,
    /// 16
    pub cmdsize: U32<E>,
    /// A.B.C.D.E packed as a24.b10.c10.d10.e10
    pub version: U64<E>,
}

/*
 * The LC_DATA_IN_CODE load commands uses a LinkeditDataCommand
 * to point to an array of DataInCodeEntry entries. Each entry
 * describes a range of data in a code section.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DataInCodeEntry<E: Endian> {
    /// from mach_header to start of data range
    pub offset: U32<E>,
    /// number of bytes in data range
    pub length: U16<E>,
    /// a DICE_KIND_* value
    pub kind: U16<E, DiceKind>,
}

newtype!(
    struct DiceKind(u16);
);

newtype_constant_names!(NAMES_DICE_KIND: DiceKind(u16) = {
    DICE_KIND_DATA = 0x0001,
    DICE_KIND_JUMP_TABLE8 = 0x0002,
    DICE_KIND_JUMP_TABLE16 = 0x0003,
    DICE_KIND_JUMP_TABLE32 = 0x0004,
    DICE_KIND_ABS_JUMP_TABLE32 = 0x0005,
});

/*
 * Sections of type S_THREAD_LOCAL_VARIABLES contain an array
 * of TlvDescriptor structures.
 */
/* TODO:
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct TlvDescriptor<E: Endian>
{
    void*		(*thunk)(struct TlvDescriptor*);
    unsigned long	key;
    unsigned long	offset;
}
*/

/*
 * LC_NOTE commands describe a region of arbitrary data included in a Mach-O
 * file.  Its initial use is to record extra data in MH_CORE files.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct NoteCommand<E: Endian> {
    /// LC_NOTE
    pub cmd: U32<E, LoadCommandType>,
    /// sizeof(struct NoteCommand)
    pub cmdsize: U32<E>,
    /// owner name for this LC_NOTE
    pub data_owner: [u8; 16],
    /// file offset of this data
    pub offset: U64<E>,
    /// length of data region
    pub size: U64<E>,
}

/*
 * LC_FILESET_ENTRY commands describe constituent Mach-O files that are part
 * of a fileset. In one implementation, entries are dylibs with individual
 * mach headers and repositionable text and data segments. Each entry is
 * further described by its own mach header.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct FilesetEntryCommand<E: Endian> {
    // LC_FILESET_ENTRY
    pub cmd: U32<E, LoadCommandType>,
    /// includes id string
    pub cmdsize: U32<E>,
    /// memory address of the dylib
    pub vmaddr: U64<E>,
    /// file offset of the dylib
    pub fileoff: U64<E>,
    /// contained entry id
    pub entry_id: LcStr<E>,
    /// entry_id is 32-bits long, so this is the reserved padding
    pub reserved: U32<E>,
}

// Definitions from "/usr/include/mach-o/nlist.h".

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Nlist32<E: Endian> {
    /// index into the string table
    pub n_strx: U32<E>,
    /// type flag
    pub n_type: SymbolFlags,
    /// section number or NO_SECT
    pub n_sect: u8,
    /// see <mach-o/stab.h>
    pub n_desc: U16<E, SymbolDesc>,
    /// value of this symbol (or stab offset)
    pub n_value: U32<E>,
}

/*
 * This is the symbol table entry structure for 64-bit architectures.
 */
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Nlist64<E: Endian> {
    /// index into the string table
    pub n_strx: U32<E>,
    /// type flag
    pub n_type: SymbolFlags,
    /// section number or NO_SECT
    pub n_sect: u8,
    /// see <mach-o/stab.h>
    pub n_desc: U16<E, SymbolDesc>,
    /// value of this symbol (or stab offset)
    pub n_value: U64<E>,
}

/*
 * Symbols with a index into the string table of zero (n_un.n_strx == 0) are
 * defined to have a null, "", name.  Therefore all string indexes to non null
 * names must not have a zero string index.  This is bit historical information
 * that has never been well documented.
 */

/*
 * The n_type field really contains four fields:
 *	unsigned char N_STAB:3,
 *		      N_PEXT:1,
 *		      N_TYPE:3,
 *		      N_EXT:1;
 * which are used via the following masks.
 */
newtype!(
    /// Values for `Nlist*::n_type`.
    #[repr(transparent)]
    struct SymbolFlags(u8);
);

impl SymbolFlags {
    /// Whether this is a symbolic debugging entry.
    pub fn is_stab(self) -> bool {
        self.0 & N_STAB != 0
    }

    /// The symbol debugging entry type.
    pub fn stab(self) -> Option<SymbolStab> {
        if self.is_stab() {
            Some(SymbolStab(self.0))
        } else {
            None
        }
    }

    /// Get the N_TYPE subfield.
    ///
    /// This method is only valid if `Self::is_stab` is false.
    pub fn typ(self) -> SymbolType {
        SymbolType(self.0 & N_TYPE)
    }

    /// Set the N_TYPE subfield.
    pub fn with_type(self, typ: SymbolType) -> SymbolFlags {
        SymbolFlags(self.0 & !N_TYPE | typ.0 & N_TYPE)
    }

    /// Whether this is an external symbol.
    ///
    /// This method is only valid if `Self::is_stab` is false.
    pub fn is_ext(self) -> bool {
        self.contains(N_EXT)
    }

    /// Whether this is a private external symbol.
    ///
    /// This checks that both `N_EXT` and `N_PEXT` are set.
    ///
    /// This method is only valid if `Self::is_stab` is false.
    pub fn is_pext(self) -> bool {
        self.contains(N_EXT | N_PEXT)
    }
}

/// if any of these bits set, a symbolic debugging entry
pub const N_STAB: u8 = 0xe0;

newtype_flag_names!(NAMES_N: SymbolFlags(u8) = {
    /// private external symbol bit
    N_PEXT = 0x10,
    /// mask for the type bits
    N_TYPE = 0x0e => NAMES_N_TYPE,
    /// external symbol bit, set for external symbols
    N_EXT = 0x01,
});

/*
 * Only symbolic debugging entries have some of the N_STAB bits set and if any
 * of these bits are set then it is a symbolic debugging entry (a stab).  In
 * which case then the values of the n_type field (the entire field) are given
 * in <mach-o/stab.h>
 */

newtype!(
    /// Values for `N_TYPE` bits of `Nlist*::n_type`.
    struct SymbolType(u8);
);

impl From<SymbolType> for SymbolFlags {
    fn from(value: SymbolType) -> Self {
        SymbolFlags(value.0)
    }
}

impl From<SymbolFlags> for SymbolType {
    fn from(value: SymbolFlags) -> Self {
        value.typ()
    }
}

impl core::ops::BitOr<SymbolFlags> for SymbolType {
    type Output = SymbolFlags;
    fn bitor(self, flags: SymbolFlags) -> SymbolFlags {
        flags.with_type(self)
    }
}

newtype_constant_names!(NAMES_N_TYPE: SymbolType(u8) = {
    /// undefined, n_sect == NO_SECT
    N_UNDF = 0x0,
    /// absolute, n_sect == NO_SECT
    N_ABS = 0x2,
    /// defined in section number n_sect
    N_SECT = 0xe,
    /// prebound undefined (defined in a dylib)
    N_PBUD = 0xc,
    /// indirect
    N_INDR = 0xa,
});

/*
 * If the type is N_INDR then the symbol is defined to be the same as another
 * symbol.  In this case the n_value field is an index into the string table
 * of the other symbol's name.  When the other symbol is defined then they both
 * take on the defined type and value.
 */

/*
 * If the type is N_SECT then the n_sect field contains an ordinal of the
 * section the symbol is defined in.  The sections are numbered from 1 and
 * refer to sections in order they appear in the load commands for the file
 * they are in.  This means the same ordinal may very well refer to different
 * sections in different files.
 *
 * The n_value field for all symbol table entries (including N_STAB's) gets
 * updated by the link editor based on the value of it's n_sect field and where
 * the section n_sect references gets relocated.  If the value of the n_sect
 * field is NO_SECT then it's n_value field is not changed by the link editor.
 */
/// symbol is not in any section
pub const NO_SECT: u8 = 0;
/// 1 thru 255 inclusive
pub const MAX_SECT: u8 = 255;

newtype!(
    /// Value for the `Nlist*::n_desc`.
    struct SymbolDesc(u16);
);

// Names are context-dependent.
newtype_flag_names!(SymbolDesc(u16) = {});

impl SymbolDesc {
    /// All possible flag value names for defined symbols.
    #[cfg(feature = "names")]
    pub const NAMES_DEFINED: &'static FlagNames<SymbolDesc> = &NAMES_N_DESC_DEFINED;

    /// All possible flag value names for undefined symbols.
    #[cfg(feature = "names")]
    pub const NAMES_UNDEFINED: &'static FlagNames<SymbolDesc> = &NAMES_N_DESC_UNDEFINED;

    /// Get the reference type subfield.
    ///
    /// Valid for undefined symbols.
    pub fn reference(self) -> SymbolReference {
        SymbolReference((self.0 & REFERENCE_TYPE) as u8)
    }

    /// Set the reference type subfield.
    ///
    /// Valid for undefined symbols.
    pub fn with_reference(self, reference: SymbolReference) -> SymbolDesc {
        SymbolDesc((self.0 & !REFERENCE_TYPE) | (u16::from(reference.0)))
    }

    /// Get the library ordinal subfield.
    ///
    /// Valid for undefined symbols in a file using `MH_TWOLEVEL`.
    ///
    /// This overlaps with the common alignment subfield and some `MH_OBJECT` flags.
    pub fn library(self) -> SymbolLibrary {
        SymbolLibrary((self.0 >> 8) as u8)
    }

    /// Set the library ordinal subfield.
    ///
    /// See [`Self::library`].
    pub fn with_library(self, ordinal: SymbolLibrary) -> SymbolDesc {
        SymbolDesc((self.0 & 0x00ff) | (u16::from(ordinal.0) << 8))
    }

    /// Get the alignment for common symbols.
    ///
    /// This is a power of 2 from 1 to 15. A value of 0 means that the natural
    /// alignment based on the size is used.
    ///
    /// Common symbols are represented by undefined (`N_UNDF`) external (`N_EXT`) symbols
    /// whose values (`n_value`) are non-zero.
    ///
    /// This overlaps with the library ordinal subfield and some `MH_OBJECT` flags.
    pub fn common_alignment(self) -> u8 {
        ((self.0 >> 8) & 0x0f) as u8
    }

    /// Set the alignment for common symbols.
    ///
    /// See [`Self::common_alignment`].
    pub fn with_common_alignment(self, alignment: u8) -> SymbolDesc {
        debug_assert_eq!(alignment & !0xf, 0);
        SymbolDesc((self.0 & 0xf0ff) | (u16::from(alignment & 0xf) << 8))
    }
}

/*
 * Common symbols are represented by undefined (N_UNDF) external (N_EXT) types
 * who's values (n_value) are non-zero.  In which case the value of the n_value
 * field is the size (in bytes) of the common symbol.  The n_sect field is set
 * to NO_SECT.  The alignment of a common symbol may be set as a power of 2
 * between 2^1 and 2^15 as part of the n_desc field using the macros below. If
 * the alignment is not set (a value of zero) then natural alignment based on
 * the size is used.
 */

/*
 * To support the lazy binding of undefined symbols in the dynamic link-editor,
 * the undefined symbols in the symbol table (the nlist structures) are marked
 * with the indication if the undefined reference is a lazy reference or
 * non-lazy reference.  If both a non-lazy reference and a lazy reference is
 * made to the same symbol the non-lazy reference takes precedence.  A reference
 * is lazy only when all references to that symbol are made through a symbol
 * pointer in a lazy symbol pointer section.
 *
 * The implementation of marking nlist structures in the symbol table for
 * undefined symbols will be to use some of the bits of the n_desc field as a
 * reference type.  The mask REFERENCE_TYPE will be applied to the n_desc field
 * of an nlist structure for an undefined symbol to determine the type of
 * undefined reference (lazy or non-lazy).
 *
 * The constants for the REFERENCE FLAGS are propagated to the reference table
 * in a shared library file.  In that case the constant for a defined symbol,
 * REFERENCE_FLAG_DEFINED, is also used.
 */
newtype!(
    /// Reference type bits of the n_desc field of undefined symbols.
    struct SymbolReference(u8);
);

impl From<SymbolReference> for SymbolDesc {
    fn from(value: SymbolReference) -> Self {
        SymbolDesc(u16::from(value.0))
    }
}

pub const REFERENCE_TYPE: u16 = 0x7;
/* types of references */
newtype_constant_names!(NAMES_REFERENCE: SymbolReference(u8) = {
    REFERENCE_FLAG_UNDEFINED_NON_LAZY = 0,
    REFERENCE_FLAG_UNDEFINED_LAZY = 1,
    REFERENCE_FLAG_DEFINED = 2,
    REFERENCE_FLAG_PRIVATE_DEFINED = 3,
    REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY = 4,
    REFERENCE_FLAG_PRIVATE_UNDEFINED_LAZY = 5,
});

/*
 * For images created by the static link editor with the -twolevel_namespace
 * option in effect the flags field of the mach header is marked with
 * MH_TWOLEVEL.  And the binding of the undefined references of the image are
 * determined by the static link editor.  Which library an undefined symbol is
 * bound to is recorded by the static linker in the high 8 bits of the n_desc
 * field using the SET_LIBRARY_ORDINAL macro below.  The ordinal recorded
 * references the libraries listed in the Mach-O's LC_LOAD_DYLIB,
 * LC_LOAD_WEAK_DYLIB, LC_REEXPORT_DYLIB, LC_LOAD_UPWARD_DYLIB, and
 * LC_LAZY_LOAD_DYLIB, etc. load commands in the order they appear in the
 * headers.   The library ordinals start from 1.
 * For a dynamic library that is built as a two-level namespace image the
 * undefined references from module defined in another use the same nlist struct
 * an in that case SELF_LIBRARY_ORDINAL is used as the library ordinal.  For
 * defined symbols in all images they also must have the library ordinal set to
 * SELF_LIBRARY_ORDINAL.  The EXECUTABLE_ORDINAL refers to the executable
 * image for references from plugins that refer to the executable that loads
 * them.
 *
 * The DYNAMIC_LOOKUP_ORDINAL is for undefined symbols in a two-level namespace
 * image that are looked up by the dynamic linker with flat namespace semantics.
 * This ordinal was added as a feature in Mac OS X 10.3 by reducing the
 * value of MAX_LIBRARY_ORDINAL by one.  So it is legal for existing binaries
 * or binaries built with older tools to have 0xfe (254) dynamic libraries.  In
 * this case the ordinal value 0xfe (254) must be treated as a library ordinal
 * for compatibility.
 */
newtype!(
    /// Library ordinal bits of the n_desc field of undefined symbols.
    ///
    /// May be a 1-based index of the libraries listed in the load commands,
    /// or a reserved constant.
    struct SymbolLibrary(u8);
);

impl SymbolLibrary {
    /// Whether this is a reserved constant, rather than an index into the
    /// libraries in the load commands.
    pub fn is_reserved(self) -> bool {
        self == SELF_LIBRARY_ORDINAL || self.0 > MAX_LIBRARY_ORDINAL
    }

    /// Get the index of the library in the load commands.
    ///
    /// Returns `None` for reserved constants.
    pub fn index(self) -> Option<u8> {
        if self.is_reserved() {
            None
        } else {
            Some(self.0)
        }
    }
}

impl From<SymbolLibrary> for SymbolDesc {
    fn from(value: SymbolLibrary) -> Self {
        SymbolDesc(u16::from(value.0) << 8)
    }
}

pub const MAX_LIBRARY_ORDINAL: u8 = 0xfd;

newtype_constant_names!(NAMES_ORDINAL: SymbolLibrary(u8) = {
    SELF_LIBRARY_ORDINAL = 0x0,
    DYNAMIC_LOOKUP_ORDINAL = 0xfe,
    EXECUTABLE_ORDINAL = 0xff,
});

flag_names!(NAMES_N_DESC_DEFINED: SymbolDesc(u16) = {
    /*
     * The N_ARM_THUMB_DEF bit of the n_desc field indicates that the symbol is
     * a definition of a Thumb function.
     */
    /// symbol is a Thumb function (ARM)
    N_ARM_THUMB_DEF = 0x0008,

    /*
     * To simplify stripping of objects that use are used with the dynamic link
     * editor, the static link editor marks the symbols defined an object that are
     * referenced by a dynamically bound object (dynamic shared libraries, bundles).
     * With this marking strip knows not to strip these symbols.
     */
    REFERENCED_DYNAMICALLY = 0x0010,

    /*
     * The bit 0x0020 of the n_desc field is used for two non-overlapping purposes
     * and has two different symbolic names, N_NO_DEAD_STRIP and N_DESC_DISCARDED.
     */

    /*
     * The N_NO_DEAD_STRIP bit of the n_desc field only ever appears in a
     * relocatable .o file (MH_OBJECT filetype). And is used to indicate to the
     * static link editor it is never to dead strip the symbol.
     */
    /// symbol is not to be dead stripped
    N_NO_DEAD_STRIP = 0x0020,

    /*
     * The N_DESC_DISCARDED bit of the n_desc field never appears in linked image.
     * But is used in very rare cases by the dynamic link editor to mark an in
     * memory symbol as discared and longer used for linking.
     */
    /// symbol is discarded
    N_DESC_DISCARDED = 0x0020,

    /*
     * The N_WEAK_DEF bit of the n_desc field indicates to the static and dynamic
     * linkers that the symbol definition is weak, allowing a non-weak symbol to
     * also be used which causes the weak definition to be discared.  Currently this
     * is only supported for symbols in coalesced sections.
     */
    /// coalesced symbol is a weak definition
    N_WEAK_DEF = 0x0080,

    /*
     * The N_SYMBOL_RESOLVER bit of the n_desc field indicates that the
     * that the function is actually a resolver function and should
     * be called to get the address of the real function to use.
     * This bit is only available in .o files (MH_OBJECT filetype)
     */
    N_SYMBOL_RESOLVER = 0x0100,

    /*
     * The N_ALT_ENTRY bit of the n_desc field indicates that the
     * symbol is pinned to the previous content.
     */
    N_ALT_ENTRY = 0x0200,

    /*
     * The N_COLD_FUNC bit of the n_desc field indicates that the symbol is used
     * infrequently and the linker should order it towards the end of the section.
     */
    N_COLD_FUNC = 0x0400,
});

flag_names!(NAMES_N_DESC_UNDEFINED: SymbolDesc(u16) = {
    /*
     * The N_WEAK_REF bit of the n_desc field indicates to the dynamic linker that
     * the undefined symbol is allowed to be missing and is to have the address of
     * zero when missing.
     */
    /// symbol is weak referenced
    N_WEAK_REF = 0x0040,

    /*
     * The N_REF_TO_WEAK bit of the n_desc field indicates to the dynamic linker
     * that the undefined symbol should be resolved using flat namespace searching.
     */
    /// reference to a weak symbol
    N_REF_TO_WEAK = 0x0080,
});

// Definitions from "/usr/include/mach-o/stab.h".

/*
 * This file gives definitions supplementing <nlist.h> for permanent symbol
 * table entries of Mach-O files.  Modified from the BSD definitions.  The
 * modifications from the original definitions were changing what the values of
 * what was the n_other field (an unused field) which is now the n_sect field.
 * These modifications are required to support symbols in an arbitrary number of
 * sections not just the three sections (text, data and bss) in a BSD file.
 * The values of the defined constants have NOT been changed.
 *
 * These must have one of the N_STAB bits on.  The n_value fields are subject
 * to relocation according to the value of their n_sect field.  So for types
 * that refer to things in sections the n_sect field must be filled in with the
 * proper section ordinal.  For types that are not to have their n_value field
 * relocatated the n_sect field must be NO_SECT.
 */

newtype!(
    /// Values for `Nlist*::n_type` when any bits in `N_STAB` are set.
    struct SymbolStab(u8);
);

/*
 * Symbolic debugger symbols.  The comments give the conventional use for
 *
 * 	.stabs "n_name", n_type, n_sect, n_desc, n_value
 *
 * where n_type is the defined constant and not listed in the comment.  Other
 * fields not listed are zero. n_sect is the section ordinal the entry is
 * referring to.
 */
newtype_constant_names!(NAMES_N_STAB: SymbolStab(u8) = {
    /// global symbol: name,,NO_SECT,type,0
    N_GSYM = 0x20,
    /// procedure name (f77 kludge): name,,NO_SECT,0,0
    N_FNAME = 0x22,
    /// procedure: name,,n_sect,linenumber,address
    N_FUN = 0x24,
    /// static symbol: name,,n_sect,type,address
    N_STSYM = 0x26,
    /// .lcomm symbol: name,,n_sect,type,address
    N_LCSYM = 0x28,
    /// begin nsect sym: 0,,n_sect,0,address
    N_BNSYM = 0x2e,
    /// AST file path: name,,NO_SECT,0,0
    N_AST = 0x32,
    /// emitted with gcc2_compiled and in gcc source
    N_OPT = 0x3c,
    /// register sym: name,,NO_SECT,type,register
    N_RSYM = 0x40,
    /// src line: 0,,n_sect,linenumber,address
    N_SLINE = 0x44,
    /// end nsect sym: 0,,n_sect,0,address
    N_ENSYM = 0x4e,
    /// structure elt: name,,NO_SECT,type,struct_offset
    N_SSYM = 0x60,
    /// source file name: name,,n_sect,0,address
    N_SO = 0x64,
    /// object file name: name,,0,0,st_mtime
    ///
    /// historically N_OSO set n_sect to 0. The N_OSO
    /// n_sect may instead hold the low byte of the
    /// cpusubtype value from the Mach-O header.
    N_OSO = 0x66,
    /// dynamic library file name: name,,NO_SECT,0,0
    N_LIB = 0x68,
    /// local sym: name,,NO_SECT,type,offset
    N_LSYM = 0x80,
    /// include file beginning: name,,NO_SECT,0,sum
    N_BINCL = 0x82,
    /// #included file name: name,,n_sect,0,address
    N_SOL = 0x84,
    /// compiler parameters: name,,NO_SECT,0,0
    N_PARAMS = 0x86,
    /// compiler version: name,,NO_SECT,0,0
    N_VERSION = 0x88,
    /// compiler -O level: name,,NO_SECT,0,0
    N_OLEVEL = 0x8A,
    /// parameter: name,,NO_SECT,type,offset
    N_PSYM = 0xa0,
    /// include file end: name,,NO_SECT,0,0
    N_EINCL = 0xa2,
    /// alternate entry: name,,n_sect,linenumber,address
    N_ENTRY = 0xa4,
    /// left bracket: 0,,NO_SECT,nesting level,address
    N_LBRAC = 0xc0,
    /// deleted include file: name,,NO_SECT,0,sum
    N_EXCL = 0xc2,
    /// right bracket: 0,,NO_SECT,nesting level,address
    N_RBRAC = 0xe0,
    /// begin common: name,,NO_SECT,0,0
    N_BCOMM = 0xe2,
    /// end common: name,,n_sect,0,0
    N_ECOMM = 0xe4,
    /// end common (local name): 0,,n_sect,0,address
    N_ECOML = 0xe8,
    /// second stab entry with length information
    N_LENG = 0xfe,

    /*
     * for the berkeley pascal compiler, pc(1):
     */
    /// global pascal symbol: name,,NO_SECT,subtype,line
    N_PC = 0x30,
});

// Definitions from "/usr/include/mach-o/reloc.h".

/// A relocation entry.
///
/// Mach-O relocations have plain and scattered variants, with the
/// meaning of the fields depending on the variant.
///
/// This type provides functions for determining whether the relocation
/// is scattered, and for accessing the fields of each variant.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Relocation<E: Endian> {
    pub r_word0: U32<E>,
    pub r_word1: U32<E>,
}

impl<E: Endian> Relocation<E> {
    /// Determine whether this is a scattered relocation.
    #[inline]
    pub fn r_scattered(self, endian: E, cputype: CpuType) -> bool {
        if cputype == CPU_TYPE_X86_64 {
            false
        } else {
            self.r_word0.get(endian) & R_SCATTERED != 0
        }
    }

    /// Return the fields of a plain relocation.
    pub fn info(self, endian: E) -> RelocationInfo {
        let r_address = self.r_word0.get(endian);
        let r_word1 = self.r_word1.get(endian);
        if endian.is_little_endian() {
            RelocationInfo {
                r_address,
                r_symbolnum: r_word1 & 0x00ff_ffff,
                r_pcrel: ((r_word1 >> 24) & 0x1) != 0,
                r_length: ((r_word1 >> 25) & 0x3) as u8,
                r_extern: ((r_word1 >> 27) & 0x1) != 0,
                r_type: RelocationType((r_word1 >> 28) as u8),
            }
        } else {
            RelocationInfo {
                r_address,
                r_symbolnum: r_word1 >> 8,
                r_pcrel: ((r_word1 >> 7) & 0x1) != 0,
                r_length: ((r_word1 >> 5) & 0x3) as u8,
                r_extern: ((r_word1 >> 4) & 0x1) != 0,
                r_type: RelocationType((r_word1 & 0xf) as u8),
            }
        }
    }

    /// Return the fields of a scattered relocation.
    pub fn scattered_info(self, endian: E) -> ScatteredRelocationInfo {
        let r_word0 = self.r_word0.get(endian);
        let r_value = self.r_word1.get(endian);
        ScatteredRelocationInfo {
            r_address: r_word0 & 0x00ff_ffff,
            r_type: RelocationType(((r_word0 >> 24) & 0xf) as u8),
            r_length: ((r_word0 >> 28) & 0x3) as u8,
            r_pcrel: ((r_word0 >> 30) & 0x1) != 0,
            r_value,
        }
    }
}

/*
 * Format of a relocation entry of a Mach-O file.  Modified from the 4.3BSD
 * format.  The modifications from the original format were changing the value
 * of the r_symbolnum field for "local" (r_extern == 0) relocation entries.
 * This modification is required to support symbols in an arbitrary number of
 * sections not just the three sections (text, data and bss) in a 4.3BSD file.
 * Also the last 4 bits have had the r_type tag added to them.
 */

#[derive(Debug, Clone, Copy)]
pub struct RelocationInfo {
    /// offset in the section to what is being relocated
    pub r_address: u32,
    /// symbol index if r_extern == 1 or section ordinal if r_extern == 0
    pub r_symbolnum: u32,
    /// was relocated pc relative already
    pub r_pcrel: bool,
    /// 0=byte, 1=word, 2=long, 3=quad
    pub r_length: u8,
    /// does not include value of sym referenced
    pub r_extern: bool,
    /// if not 0, machine specific relocation type
    pub r_type: RelocationType,
}

impl RelocationInfo {
    /// Combine the fields into a `Relocation`.
    pub fn relocation<E: Endian>(self, endian: E) -> Relocation<E> {
        let r_word0 = U32::new(endian, self.r_address);
        let r_word1 = U32::new(
            endian,
            if endian.is_little_endian() {
                self.r_symbolnum & 0x00ff_ffff
                    | u32::from(self.r_pcrel) << 24
                    | u32::from(self.r_length & 0x3) << 25
                    | u32::from(self.r_extern) << 27
                    | u32::from(self.r_type.0) << 28
            } else {
                self.r_symbolnum >> 8
                    | u32::from(self.r_pcrel) << 7
                    | u32::from(self.r_length & 0x3) << 5
                    | u32::from(self.r_extern) << 4
                    | u32::from(self.r_type.0) & 0xf
            },
        );
        Relocation { r_word0, r_word1 }
    }
}

/// Value for `RelocationInfo::r_symbolnum` for absolute symbols.
pub const R_ABS: u32 = 0;

/*
 * The r_address is not really the address as it's name indicates but an offset.
 * In 4.3BSD a.out objects this offset is from the start of the "segment" for
 * which relocation entry is for (text or data).  For Mach-O object files it is
 * also an offset but from the start of the "section" for which the relocation
 * entry is for.  See comments in <mach-o/loader.h> about the r_address feild
 * in images for used with the dynamic linker.
 *
 * In 4.3BSD a.out objects if r_extern is zero then r_symbolnum is an ordinal
 * for the segment the symbol being relocated is in.  These ordinals are the
 * symbol types N_TEXT, N_DATA, N_BSS or N_ABS.  In Mach-O object files these
 * ordinals refer to the sections in the object file in the order their section
 * structures appear in the headers of the object file they are in.  The first
 * section has the ordinal 1, the second 2, and so on.  This means that the
 * same ordinal in two different object files could refer to two different
 * sections.  And further could have still different ordinals when combined
 * by the link-editor.  The value R_ABS is used for relocation entries for
 * absolute symbols which need no further relocation.
 */

/*
 * For RISC machines some of the references are split across two instructions
 * and the instruction does not contain the complete value of the reference.
 * In these cases a second, or paired relocation entry, follows each of these
 * relocation entries, using a PAIR r_type, which contains the other part of the
 * reference not contained in the instruction.  This other part is stored in the
 * pair's r_address field.  The exact number of bits of the other part of the
 * reference store in the r_address field is dependent on the particular
 * relocation type for the particular architecture.
 */

/*
 * To make scattered loading by the link editor work correctly "local"
 * relocation entries can't be used when the item to be relocated is the value
 * of a symbol plus an offset (where the resulting expression is outside the
 * block the link editor is moving, a blocks are divided at symbol addresses).
 * In this case. where the item is a symbol value plus offset, the link editor
 * needs to know more than just the section the symbol was defined.  What is
 * needed is the actual value of the symbol without the offset so it can do the
 * relocation correctly based on where the value of the symbol got relocated to
 * not the value of the expression (with the offset added to the symbol value).
 * So for the NeXT 2.0 release no "local" relocation entries are ever used when
 * there is a non-zero offset added to a symbol.  The "external" and "local"
 * relocation entries remain unchanged.
 *
 * The implementation is quite messy given the compatibility with the existing
 * relocation entry format.  The ASSUMPTION is that a section will never be
 * bigger than 2**24 - 1 (0x00ffffff or 16,777,215) bytes.  This assumption
 * allows the r_address (which is really an offset) to fit in 24 bits and high
 * bit of the r_address field in the relocation_info structure to indicate
 * it is really a scattered_relocation_info structure.  Since these are only
 * used in places where "local" relocation entries are used and not where
 * "external" relocation entries are used the r_extern field has been removed.
 *
 * For scattered loading to work on a RISC machine where some of the references
 * are split across two instructions the link editor needs to be assured that
 * each reference has a unique 32 bit reference (that more than one reference is
 * NOT sharing the same high 16 bits for example) so it move each referenced
 * item independent of each other.  Some compilers guarantees this but the
 * compilers don't so scattered loading can be done on those that do guarantee
 * this.
 */

/// Bit set in `Relocation::r_word0` for scattered relocations.
pub const R_SCATTERED: u32 = 0x8000_0000;

#[derive(Debug, Clone, Copy)]
pub struct ScatteredRelocationInfo {
    /// offset in the section to what is being relocated
    pub r_address: u32,
    /// if not 0, machine specific relocation type
    pub r_type: RelocationType,
    /// 0=byte, 1=word, 2=long, 3=quad
    pub r_length: u8,
    /// was relocated pc relative already
    pub r_pcrel: bool,
    /// the value the item to be relocated is referring to (without any offset added)
    pub r_value: u32,
}

impl ScatteredRelocationInfo {
    /// Combine the fields into a `Relocation`.
    pub fn relocation<E: Endian>(self, endian: E) -> Relocation<E> {
        let r_word0 = U32::new(
            endian,
            self.r_address & 0x00ff_ffff
                | u32::from(self.r_type.0 & 0xf) << 24
                | u32::from(self.r_length & 0x3) << 28
                | u32::from(self.r_pcrel) << 30
                | R_SCATTERED,
        );
        let r_word1 = U32::new(endian, self.r_value);
        Relocation { r_word0, r_word1 }
    }
}

newtype!(
    /// Values for `RelocationInfo::r_type` and `ScatteredRelocationInfo::r_type`.
    struct RelocationType(u8);
);

newtype_constant_names!(RelocationType(u8) = {});

/*
 * Relocation types used in a generic implementation.  Relocation entries for
 * normal things use the generic relocation as described above and their r_type
 * is GENERIC_RELOC_VANILLA (a value of zero).
 *
 * Another type of generic relocation, GENERIC_RELOC_SECTDIFF, is to support
 * the difference of two symbols defined in different sections.  That is the
 * expression "symbol1 - symbol2 + constant" is a relocatable expression when
 * both symbols are defined in some section.  For this type of relocation the
 * both relocations entries are scattered relocation entries.  The value of
 * symbol1 is stored in the first relocation entry's r_value field and the
 * value of symbol2 is stored in the pair's r_value field.
 *
 * A special case for a prebound lazy pointer is needed to beable to set the
 * value of the lazy pointer back to its non-prebound state.  This is done
 * using the GENERIC_RELOC_PB_LA_PTR r_type.  This is a scattered relocation
 * entry where the r_value feild is the value of the lazy pointer not prebound.
 */
constant_names!(
/// Values for `Relocation::r_type` for generic Mach-O architectures.
pub NAMES_GENERIC_RELOC: RelocationType(u8) = {
    /// generic relocation as described above
    GENERIC_RELOC_VANILLA = 0,
    /// Only follows a GENERIC_RELOC_SECTDIFF
    GENERIC_RELOC_PAIR = 1,
    GENERIC_RELOC_SECTDIFF = 2,
    /// prebound lazy pointer
    GENERIC_RELOC_PB_LA_PTR = 3,
    GENERIC_RELOC_LOCAL_SECTDIFF = 4,
    /// thread local variables
    GENERIC_RELOC_TLV = 5,
});

// Definitions from "/usr/include/mach-o/arm/reloc.h".

/*
 * Relocation types used in the arm implementation.  Relocation entries for
 * things other than instructions use the same generic relocation as described
 * in <mach-o/reloc.h> and their r_type is ARM_RELOC_VANILLA, one of the
 * *_SECTDIFF or the *_PB_LA_PTR types.  The rest of the relocation types are
 * for instructions.  Since they are for instructions the r_address field
 * indicates the 32 bit instruction that the relocation is to be performed on.
 */
constant_names!(
/// Values for `Relocation::r_type` on ARM.
pub NAMES_ARM_RELOC: RelocationType(u8) = {
    /// generic relocation as described above
    ARM_RELOC_VANILLA = 0,
    /// the second relocation entry of a pair
    ARM_RELOC_PAIR = 1,
    /// a PAIR follows with subtract symbol value
    ARM_RELOC_SECTDIFF = 2,
    /// like ARM_RELOC_SECTDIFF, but the symbol referenced was local.
    ARM_RELOC_LOCAL_SECTDIFF = 3,
    /// prebound lazy pointer
    ARM_RELOC_PB_LA_PTR = 4,
    /// 24 bit branch displacement (to a word address)
    ARM_RELOC_BR24 = 5,
    /// 22 bit branch displacement (to a half-word address)
    ARM_THUMB_RELOC_BR22 = 6,
    /// obsolete - a thumb 32-bit branch instruction possibly needing page-spanning branch workaround
    ARM_THUMB_32BIT_BRANCH = 7,

    /*
     * For these two r_type relocations they always have a pair following them
     * and the r_length bits are used differently.  The encoding of the
     * r_length is as follows:
     * low bit of r_length:
     *  0 - :lower16: for movw instructions
     *  1 - :upper16: for movt instructions
     * high bit of r_length:
     *  0 - arm instructions
     *  1 - thumb instructions
     * the other half of the relocated expression is in the following pair
     * relocation entry in the the low 16 bits of r_address field.
     */
    ARM_RELOC_HALF = 8,
    ARM_RELOC_HALF_SECTDIFF = 9,
});

// Definitions from "/usr/include/mach-o/arm64/reloc.h".

/*
 * Relocation types used in the arm64 implementation.
 */
constant_names!(
/// Values for `Relocation::r_type` on ARM64.
pub NAMES_ARM64_RELOC: RelocationType(u8) = {
    /// for pointers
    ARM64_RELOC_UNSIGNED = 0,
    /// must be followed by a ARM64_RELOC_UNSIGNED
    ARM64_RELOC_SUBTRACTOR = 1,
    /// a B/BL instruction with 26-bit displacement
    ARM64_RELOC_BRANCH26 = 2,
    /// pc-rel distance to page of target
    ARM64_RELOC_PAGE21 = 3,
    /// offset within page, scaled by r_length
    ARM64_RELOC_PAGEOFF12 = 4,
    /// pc-rel distance to page of GOT slot
    ARM64_RELOC_GOT_LOAD_PAGE21 = 5,
    /// offset within page of GOT slot, scaled by r_length
    ARM64_RELOC_GOT_LOAD_PAGEOFF12 = 6,
    /// for pointers to GOT slots
    ARM64_RELOC_POINTER_TO_GOT = 7,
    /// pc-rel distance to page of TLVP slot
    ARM64_RELOC_TLVP_LOAD_PAGE21 = 8,
    /// offset within page of TLVP slot, scaled by r_length
    ARM64_RELOC_TLVP_LOAD_PAGEOFF12 = 9,
    /// must be followed by PAGE21 or PAGEOFF12
    ARM64_RELOC_ADDEND = 10,

    // An arm64e authenticated pointer.
    //
    // Represents a pointer to a symbol (like ARM64_RELOC_UNSIGNED).
    // Additionally, the resulting pointer is signed.  The signature is
    // specified in the target location: the addend is restricted to the lower
    // 32 bits (instead of the full 64 bits for ARM64_RELOC_UNSIGNED):
    //
    //   |63|62|61-51|50-49|  48  |47     -     32|31  -  0|
    //   | 1| 0|  0  | key | addr | discriminator | addend |
    //
    // The key is one of:
    //   IA: 00 IB: 01
    //   DA: 10 DB: 11
    //
    // The discriminator field is used as extra signature diversification.
    //
    // The addr field indicates whether the target address should be blended
    // into the discriminator.
    //
    ARM64_RELOC_AUTHENTICATED_POINTER = 11,
});

// Definitions from "/usr/include/mach-o/ppc/reloc.h".

/*
 * Relocation types used in the ppc implementation.  Relocation entries for
 * things other than instructions use the same generic relocation as described
 * above and their r_type is RELOC_VANILLA.  The rest of the relocation types
 * are for instructions.  Since they are for instructions the r_address field
 * indicates the 32 bit instruction that the relocation is to be performed on.
 * The fields r_pcrel and r_length are ignored for non-RELOC_VANILLA r_types
 * except for PPC_RELOC_BR14.
 *
 * For PPC_RELOC_BR14 if the r_length is the unused value 3, then the branch was
 * statically predicted setting or clearing the Y-bit based on the sign of the
 * displacement or the opcode.  If this is the case the static linker must flip
 * the value of the Y-bit if the sign of the displacement changes for non-branch
 * always conditions.
 */
constant_names!(
/// Values for `Relocation::r_type` on PowerPC.
pub NAMES_PPC_RELOC: RelocationType(u8) = {
    /// generic relocation as described above
    PPC_RELOC_VANILLA = 0,
    /// the second relocation entry of a pair
    PPC_RELOC_PAIR = 1,
    /// 14 bit branch displacement (to a word address)
    PPC_RELOC_BR14 = 2,
    /// 24 bit branch displacement (to a word address)
    PPC_RELOC_BR24 = 3,
    /// a PAIR follows with the low half
    PPC_RELOC_HI16 = 4,
    /// a PAIR follows with the high half
    PPC_RELOC_LO16 = 5,
    /// Same as the RELOC_HI16 except the low 16 bits and the high 16 bits are added together
    /// with the low 16 bits sign extended first.  This means if bit 15 of the low 16 bits is
    /// set the high 16 bits stored in the instruction will be adjusted.
    PPC_RELOC_HA16 = 6,
    /// Same as the LO16 except that the low 2 bits are not stored in the instruction and are
    /// always zero.  This is used in double word load/store instructions.
    PPC_RELOC_LO14 = 7,
    /// a PAIR follows with subtract symbol value
    PPC_RELOC_SECTDIFF = 8,
    /// prebound lazy pointer
    PPC_RELOC_PB_LA_PTR = 9,
    /// section difference forms of above.  a PAIR
    PPC_RELOC_HI16_SECTDIFF = 10,
    /// follows these with subtract symbol value
    PPC_RELOC_LO16_SECTDIFF = 11,
    PPC_RELOC_HA16_SECTDIFF = 12,
    PPC_RELOC_JBSR = 13,
    PPC_RELOC_LO14_SECTDIFF = 14,
    /// like PPC_RELOC_SECTDIFF, but the symbol referenced was local.
    PPC_RELOC_LOCAL_SECTDIFF = 15,
});

// Definitions from "/usr/include/mach-o/x86_64/reloc.h".

/*
 * Relocations for x86_64 are a bit different than for other architectures in
 * Mach-O: Scattered relocations are not used.  Almost all relocations produced
 * by the compiler are external relocations.  An external relocation has the
 * r_extern bit set to 1 and the r_symbolnum field contains the symbol table
 * index of the target label.
 *
 * When the assembler is generating relocations, if the target label is a local
 * label (begins with 'L'), then the previous non-local label in the same
 * section is used as the target of the external relocation.  An addend is used
 * with the distance from that non-local label to the target label.  Only when
 * there is no previous non-local label in the section is an internal
 * relocation used.
 *
 * The addend (i.e. the 4 in _foo+4) is encoded in the instruction (Mach-O does
 * not have RELA relocations).  For PC-relative relocations, the addend is
 * stored directly in the instruction.  This is different from other Mach-O
 * architectures, which encode the addend minus the current section offset.
 *
 * The relocation types are:
 *
 * 	X86_64_RELOC_UNSIGNED	// for absolute addresses
 * 	X86_64_RELOC_SIGNED		// for signed 32-bit displacement
 * 	X86_64_RELOC_BRANCH		// a CALL/JMP instruction with 32-bit displacement
 * 	X86_64_RELOC_GOT_LOAD	// a MOVQ load of a GOT entry
 * 	X86_64_RELOC_GOT		// other GOT references
 * 	X86_64_RELOC_SUBTRACTOR	// must be followed by a X86_64_RELOC_UNSIGNED
 *
 * The following are sample assembly instructions, followed by the relocation
 * and section content they generate in an object file:
 *
 * 	call _foo
 * 		r_type=X86_64_RELOC_BRANCH, r_length=2, r_extern=1, r_pcrel=1, r_symbolnum=_foo
 * 		E8 00 00 00 00
 *
 * 	call _foo+4
 * 		r_type=X86_64_RELOC_BRANCH, r_length=2, r_extern=1, r_pcrel=1, r_symbolnum=_foo
 * 		E8 04 00 00 00
 *
 * 	movq _foo@GOTPCREL(%rip), %rax
 * 		r_type=X86_64_RELOC_GOT_LOAD, r_length=2, r_extern=1, r_pcrel=1, r_symbolnum=_foo
 * 		48 8B 05 00 00 00 00
 *
 * 	pushq _foo@GOTPCREL(%rip)
 * 		r_type=X86_64_RELOC_GOT, r_length=2, r_extern=1, r_pcrel=1, r_symbolnum=_foo
 * 		FF 35 00 00 00 00
 *
 * 	movl _foo(%rip), %eax
 * 		r_type=X86_64_RELOC_SIGNED, r_length=2, r_extern=1, r_pcrel=1, r_symbolnum=_foo
 * 		8B 05 00 00 00 00
 *
 * 	movl _foo+4(%rip), %eax
 * 		r_type=X86_64_RELOC_SIGNED, r_length=2, r_extern=1, r_pcrel=1, r_symbolnum=_foo
 * 		8B 05 04 00 00 00
 *
 * 	movb  $0x12, _foo(%rip)
 * 		r_type=X86_64_RELOC_SIGNED, r_length=2, r_extern=1, r_pcrel=1, r_symbolnum=_foo
 * 		C6 05 FF FF FF FF 12
 *
 * 	movl  $0x12345678, _foo(%rip)
 * 		r_type=X86_64_RELOC_SIGNED, r_length=2, r_extern=1, r_pcrel=1, r_symbolnum=_foo
 * 		C7 05 FC FF FF FF 78 56 34 12
 *
 * 	.quad _foo
 * 		r_type=X86_64_RELOC_UNSIGNED, r_length=3, r_extern=1, r_pcrel=0, r_symbolnum=_foo
 * 		00 00 00 00 00 00 00 00
 *
 * 	.quad _foo+4
 * 		r_type=X86_64_RELOC_UNSIGNED, r_length=3, r_extern=1, r_pcrel=0, r_symbolnum=_foo
 * 		04 00 00 00 00 00 00 00
 *
 * 	.quad _foo - _bar
 * 		r_type=X86_64_RELOC_SUBTRACTOR, r_length=3, r_extern=1, r_pcrel=0, r_symbolnum=_bar
 * 		r_type=X86_64_RELOC_UNSIGNED, r_length=3, r_extern=1, r_pcrel=0, r_symbolnum=_foo
 * 		00 00 00 00 00 00 00 00
 *
 * 	.quad _foo - _bar + 4
 * 		r_type=X86_64_RELOC_SUBTRACTOR, r_length=3, r_extern=1, r_pcrel=0, r_symbolnum=_bar
 * 		r_type=X86_64_RELOC_UNSIGNED, r_length=3, r_extern=1, r_pcrel=0, r_symbolnum=_foo
 * 		04 00 00 00 00 00 00 00
 *
 * 	.long _foo - _bar
 * 		r_type=X86_64_RELOC_SUBTRACTOR, r_length=2, r_extern=1, r_pcrel=0, r_symbolnum=_bar
 * 		r_type=X86_64_RELOC_UNSIGNED, r_length=2, r_extern=1, r_pcrel=0, r_symbolnum=_foo
 * 		00 00 00 00
 *
 * 	lea L1(%rip), %rax
 * 		r_type=X86_64_RELOC_SIGNED, r_length=2, r_extern=1, r_pcrel=1, r_symbolnum=_prev
 * 		48 8d 05 12 00 00 00
 * 		// assumes _prev is the first non-local label 0x12 bytes before L1
 *
 * 	lea L0(%rip), %rax
 * 		r_type=X86_64_RELOC_SIGNED, r_length=2, r_extern=0, r_pcrel=1, r_symbolnum=3
 * 		48 8d 05 56 00 00 00
 *		// assumes L0 is in third section and there is no previous non-local label.
 *		// The rip-relative-offset of 0x00000056 is L0-address_of_next_instruction.
 *		// address_of_next_instruction is the address of the relocation + 4.
 *
 *     add     $6,L0(%rip)
 *             r_type=X86_64_RELOC_SIGNED_1, r_length=2, r_extern=0, r_pcrel=1, r_symbolnum=3
 *		83 05 18 00 00 00 06
 *		// assumes L0 is in third section and there is no previous non-local label.
 *		// The rip-relative-offset of 0x00000018 is L0-address_of_next_instruction.
 *		// address_of_next_instruction is the address of the relocation + 4 + 1.
 *		// The +1 comes from SIGNED_1.  This is used because the relocation is not
 *		// at the end of the instruction.
 *
 * 	.quad L1
 * 		r_type=X86_64_RELOC_UNSIGNED, r_length=3, r_extern=1, r_pcrel=0, r_symbolnum=_prev
 * 		12 00 00 00 00 00 00 00
 * 		// assumes _prev is the first non-local label 0x12 bytes before L1
 *
 * 	.quad L0
 * 		r_type=X86_64_RELOC_UNSIGNED, r_length=3, r_extern=0, r_pcrel=0, r_symbolnum=3
 * 		56 00 00 00 00 00 00 00
 * 		// assumes L0 is in third section, has an address of 0x00000056 in .o
 * 		// file, and there is no previous non-local label
 *
 * 	.quad _foo - .
 * 		r_type=X86_64_RELOC_SUBTRACTOR, r_length=3, r_extern=1, r_pcrel=0, r_symbolnum=_prev
 * 		r_type=X86_64_RELOC_UNSIGNED, r_length=3, r_extern=1, r_pcrel=0, r_symbolnum=_foo
 * 		EE FF FF FF FF FF FF FF
 * 		// assumes _prev is the first non-local label 0x12 bytes before this
 * 		// .quad
 *
 * 	.quad _foo - L1
 * 		r_type=X86_64_RELOC_SUBTRACTOR, r_length=3, r_extern=1, r_pcrel=0, r_symbolnum=_prev
 * 		r_type=X86_64_RELOC_UNSIGNED, r_length=3, r_extern=1, r_pcrel=0, r_symbolnum=_foo
 * 		EE FF FF FF FF FF FF FF
 * 		// assumes _prev is the first non-local label 0x12 bytes before L1
 *
 * 	.quad L1 - _prev
 * 		// No relocations.  This is an assembly time constant.
 * 		12 00 00 00 00 00 00 00
 * 		// assumes _prev is the first non-local label 0x12 bytes before L1
 *
 *
 *
 * In final linked images, there are only two valid relocation kinds:
 *
 *     r_type=X86_64_RELOC_UNSIGNED, r_length=3, r_pcrel=0, r_extern=1, r_symbolnum=sym_index
 *	This tells dyld to add the address of a symbol to a pointer sized (8-byte)
 *  piece of data (i.e on disk the 8-byte piece of data contains the addend). The
 *  r_symbolnum contains the index into the symbol table of the target symbol.
 *
 *     r_type=X86_64_RELOC_UNSIGNED, r_length=3, r_pcrel=0, r_extern=0, r_symbolnum=0
 * This tells dyld to adjust the pointer sized (8-byte) piece of data by the amount
 * the containing image was loaded from its base address (e.g. slide).
 *
 */
constant_names!(
/// Values for `Relocation::r_type` on x86_64.
pub NAMES_X86_64_RELOC: RelocationType(u8) = {
    /// for absolute addresses
    X86_64_RELOC_UNSIGNED = 0,
    /// for signed 32-bit displacement
    X86_64_RELOC_SIGNED = 1,
    /// a CALL/JMP instruction with 32-bit displacement
    X86_64_RELOC_BRANCH = 2,
    /// a MOVQ load of a GOT entry
    X86_64_RELOC_GOT_LOAD = 3,
    /// other GOT references
    X86_64_RELOC_GOT = 4,
    /// must be followed by a X86_64_RELOC_UNSIGNED
    X86_64_RELOC_SUBTRACTOR = 5,
    /// for signed 32-bit displacement with a -1 addend
    X86_64_RELOC_SIGNED_1 = 6,
    /// for signed 32-bit displacement with a -2 addend
    X86_64_RELOC_SIGNED_2 = 7,
    /// for signed 32-bit displacement with a -4 addend
    X86_64_RELOC_SIGNED_4 = 8,
    /// for thread local variables
    X86_64_RELOC_TLV = 9,
});

// Definitions from https://github.com/apple-oss-distributions/dyld/blob/dyld-1376.6/include/mach-o/fixup-chains.h

// Header of the `LC_DYLD_CHAINED_FIXUPS` payload.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DyldChainedFixupsHeader<E: Endian> {
    /// 0
    pub fixups_version: U32<E>,
    /// offset of `DyldChainedStartsInImage` in chain_data
    pub starts_offset: U32<E>,
    /// offset of imports table in chain_data
    pub imports_offset: U32<E>,
    /// offset of symbol strings in chain_data
    pub symbols_offset: U32<E>,
    /// number of imported symbol names
    pub imports_count: U32<E>,
    /// `DYLD_CHAINED_IMPORT*`
    pub imports_format: U32<E, DyldChainedImportFormat>,
    /// 0 => uncompressed, 1 => zlib compressed
    pub symbols_format: U32<E>,
}

/// Holds the chain starts for each segment in the image.
///
/// This struct is embedded in `LC_DYLD_CHAINED_FIXUPS` payload.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DyldChainedStartsInImage<E: Endian> {
    pub seg_count: U32<E>,
    // Each entry is offset into this struct for that segment
    // followed by pool of `DyldChainedStartsInSegment` data.
    //pub seg_info_offset: [U32<E>; 1],
}

/// Holds the chain starts for each page in a segment.
///
/// This struct is embedded in `DyldChainedStartsInImage`.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DyldChainedStartsInSegment<E: Endian> {
    /// size of this (amount kernel needs to copy)
    pub size: U32<E>,
    /// 0x1000 or 0x4000
    pub page_size: U16<E>,
    /// `DYLD_CHAINED_PTR_*`
    pub pointer_format: U16<E, DyldChainedPtrFormat>,
    /// offset in memory to start of segment
    pub segment_offset: U64<E>,
    /// for 32-bit OS, any value beyond this is not a pointer
    pub max_valid_pointer: U32<E>,
    /// how many pages are in array
    pub page_count: U16<E>,
    // each entry is offset in each page of first element in chain
    // or DYLD_CHAINED_PTR_START_NONE if no fixups on page
    //pub page_start: [U16<E>; 1],
    // some 32-bit formats may require multiple starts per page.
    // for those, if high bit is set in page_starts[], then it
    // is index into chain_starts[] which is a list of starts
    // the last of which has the high bit set
    //pub chain_starts: [U16<E>; 1],
}

/// Used in `DyldChainedStartsInSegment::page_start[]` to denote a page with no fixups.
pub const DYLD_CHAINED_PTR_START_NONE: u16 = 0xFFFF;
/// Used in `DyldChainedStartsInSegment::page_start[]` to denote a page which has multiple starts.
pub const DYLD_CHAINED_PTR_START_MULTI: u16 = 0x8000;
/// Used in `DyldChainedStartsInSegment::chain_starts[]` to denote last start in list for page.
pub const DYLD_CHAINED_PTR_START_LAST: u16 = 0x8000;

// these values are set in the reserved1 field of the __chain_starts section
/*
enum {
    /// denotes chain starts linked with -fixup_chains_section
    DYLD_CHAINED_STARTS_USE_FILE_OFFSET = 0x1,
    /// denotes chain starts linked with -fixup_chains_section_vm
    DYLD_CHAINED_STARTS_USE_VM_OFFSET   = 0x2,
};
*/

/// Holds the chain starts in the `__TEXT,__chain_starts` section in firmware.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DyldChainedStartsOffsets<E: Endian> {
    /// `DYLD_CHAINED_PTR_32_FIRMWARE` or `DYLD_CHAINED_PTR_ARM64E_FIRMWARE`
    pub pointer_format: U32<E, DyldChainedPtrFormat>,
    /// number of starts in array
    pub starts_count: U32<E>,
    // array chain start offsets
    //pub chain_starts: [U32<E>; 1],
}

newtype!(
    /// Value for `DyldChainedStartsInSegment::pointer_format`.
    struct DyldChainedPtrFormat(u16);
);

newtype_constant_names!(NAMES_DYLD_CHAINED_PTR: DyldChainedPtrFormat(u16) = {
    /// stride 8, unauth target is vmaddr
    DYLD_CHAINED_PTR_ARM64E                 =  1,
    /// target is vmaddr
    DYLD_CHAINED_PTR_64                     =  2,
    /// target is vmaddr
    DYLD_CHAINED_PTR_32                     =  3,
    DYLD_CHAINED_PTR_32_CACHE               =  4,
    DYLD_CHAINED_PTR_32_FIRMWARE            =  5,
    /// target is vm offset
    DYLD_CHAINED_PTR_64_OFFSET              =  6,
    /// stride 4, unauth target is vm offset
    DYLD_CHAINED_PTR_ARM64E_KERNEL          =  7,
    /// old name
    DYLD_CHAINED_PTR_ARM64E_OFFSET          =  7,
    DYLD_CHAINED_PTR_64_KERNEL_CACHE        =  8,
    /// stride 8, unauth target is vm offset
    DYLD_CHAINED_PTR_ARM64E_USERLAND        =  9,
    /// stride 4, unauth target is vmaddr
    DYLD_CHAINED_PTR_ARM64E_FIRMWARE        = 10,
    /// stride 1, x86_64 kernel caches
    DYLD_CHAINED_PTR_X86_64_KERNEL_CACHE    = 11,
    /// stride 8, unauth target is vm offset, 24-bit bind
    DYLD_CHAINED_PTR_ARM64E_USERLAND24      = 12,
    /// stride 8, regular/auth targets both vm offsets
    DYLD_CHAINED_PTR_ARM64E_SHARED_CACHE    = 13,
    /// stride 4, rebase offsets use segIndex and segOffset
    DYLD_CHAINED_PTR_ARM64E_SEGMENTED       = 14,
});

/// A chained pointer for `DYLD_CHAINED_PTR_ARM64E` and `DYLD_CHAINED_PTR_ARM64E_USERLAND24`.
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedPtrArm64e(pub u64);

impl DyldChainedPtrArm64e {
    /// The offset to the next chained pointer, in units of the stride.
    pub fn next(self) -> u64 {
        (self.0 >> 51) & ((1 << 11) - 1)
    }

    /// Whether this is a bind or a rebase.
    pub fn is_bind(self) -> bool {
        (self.0 >> 62) & 1 != 0
    }

    /// Whether the pointer is authenticated.
    pub fn is_auth(self) -> bool {
        (self.0 >> 63) & 1 != 0
    }

    /// Get the bind fields.
    pub fn bind(self) -> DyldChainedPtrArm64eBind {
        DyldChainedPtrArm64eBind(self.0)
    }

    /// Get the authenticated bind fields.
    pub fn auth_bind(self) -> DyldChainedPtrArm64eAuthBind {
        DyldChainedPtrArm64eAuthBind(self.0)
    }

    /// Get the rebase fields.
    pub fn rebase(self) -> DyldChainedPtrArm64eRebase {
        DyldChainedPtrArm64eRebase(self.0)
    }

    /// Get the authenticated rebase fields.
    pub fn auth_rebase(self) -> DyldChainedPtrArm64eAuthRebase {
        DyldChainedPtrArm64eAuthRebase(self.0)
    }

    /// Get the 24-bit bind fields.
    pub fn bind24(self) -> DyldChainedPtrArm64eBind24 {
        DyldChainedPtrArm64eBind24(self.0)
    }

    /// Get the authenticated 24-bit bind fields.
    pub fn auth_bind24(self) -> DyldChainedPtrArm64eAuthBind24 {
        DyldChainedPtrArm64eAuthBind24(self.0)
    }
}

/// The unauthenticated rebase fields for [`DyldChainedPtrArm64e`].
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedPtrArm64eRebase(pub u64);

impl DyldChainedPtrArm64eRebase {
    /// The unauthenticated target.
    pub fn target(self) -> u64 {
        self.0 & ((1 << 43) - 1)
    }

    /// The top 8 bits of the pointer.
    pub fn high8(self) -> u64 {
        (self.0 >> 43) & 0xff
    }
}

/// The authenticated rebase fields for [`DyldChainedPtrArm64e`].
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedPtrArm64eAuthRebase(pub u64);

impl DyldChainedPtrArm64eAuthRebase {
    /// The runtime offset target.
    pub fn runtime_offset(self) -> u64 {
        self.0 & ((1 << 32) - 1)
    }

    /// The diversity value for authentication.
    pub fn diversity(self) -> u16 {
        ((self.0 >> 32) & 0xffff) as u16
    }

    /// Whether to use address diversity for authentication.
    pub fn addr_div(self) -> bool {
        (self.0 >> 48) & 1 != 0
    }

    /// The key for authentication.
    pub fn key(self) -> u8 {
        ((self.0 >> 49) & 3) as u8
    }
}

/// The unauthenticated bind fields for [`DyldChainedPtrArm64e`].
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedPtrArm64eBind(pub u64);

impl DyldChainedPtrArm64eBind {
    /// The import ordinal.
    pub fn ordinal(self) -> u32 {
        (self.0 & 0xffff) as u32
    }

    /// The signed 19-bit addend.
    pub fn addend(self) -> i32 {
        // Sign extend.
        ((self.0 >> 19) as i32) >> 13
    }
}

/// The authenticated bind fields for [`DyldChainedPtrArm64e`].
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedPtrArm64eAuthBind(pub u64);

impl DyldChainedPtrArm64eAuthBind {
    /// The import ordinal.
    pub fn ordinal(self) -> u32 {
        (self.0 & 0xffff) as u32
    }

    /// The diversity value for authentication.
    pub fn diversity(self) -> u16 {
        ((self.0 >> 32) & 0xffff) as u16
    }

    /// Whether to use address diversity for authentication.
    pub fn addr_div(self) -> bool {
        (self.0 >> 48) & 1 != 0
    }

    /// The key for authentication.
    pub fn key(self) -> u8 {
        ((self.0 >> 49) & 3) as u8
    }
}

/// The unauthenticated bind fields for `DYLD_CHAINED_PTR_ARM64E_USERLAND24`.
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedPtrArm64eBind24(pub u64);

impl DyldChainedPtrArm64eBind24 {
    /// The 24-bit import ordinal.
    pub fn ordinal(self) -> u32 {
        (self.0 & ((1 << 24) - 1)) as u32
    }

    /// The signed 19-bit addend.
    pub fn addend(self) -> i32 {
        // Sign extend.
        ((self.0 >> 19) as i32) >> 13
    }
}

/// The authenticated bind fields for `DYLD_CHAINED_PTR_ARM64E_USERLAND24`.
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedPtrArm64eAuthBind24(pub u64);

impl DyldChainedPtrArm64eAuthBind24 {
    /// The 24-bit import ordinal.
    pub fn ordinal(self) -> u32 {
        (self.0 & ((1 << 24) - 1)) as u32
    }

    /// The diversity value for authentication.
    pub fn diversity(self) -> u16 {
        ((self.0 >> 32) & 0xffff) as u16
    }

    /// Whether to use address diversity for authentication.
    pub fn addr_div(self) -> bool {
        (self.0 >> 48) & 1 != 0
    }

    /// The key for authentication.
    pub fn key(self) -> u8 {
        ((self.0 >> 49) & 3) as u8
    }
}

/// A chained pointer for `DYLD_CHAINED_PTR_ARM64E_SEGMENTED`.
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedPtrArm64eSegmentedRebase(pub u64);

impl DyldChainedPtrArm64eSegmentedRebase {
    /// The offset to the next chained pointer, in units of the stride.
    pub fn next(self) -> u64 {
        (self.0 >> 51) & ((1 << 12) - 1)
    }

    /// Whether the pointer is authenticated.
    pub fn is_auth(self) -> bool {
        (self.0 >> 63) & 1 != 0
    }

    /// The offset in the segment.
    pub fn target_seg_offset(self) -> u64 {
        self.0 & ((1 << 28) - 1)
    }

    /// The index into the segment address table.
    pub fn target_seg_index(self) -> u8 {
        ((self.0 >> 28) & 0xf) as u8
    }

    /// The diversity value for authentication.
    ///
    /// Only valid if `is_auth` is true.
    pub fn diversity(self) -> u16 {
        ((self.0 >> 32) & 0xffff) as u16
    }

    /// Whether to use address diversity for authentication.
    ///
    /// Only valid if `is_auth` is true.
    pub fn addr_div(self) -> bool {
        (self.0 >> 48) & 1 != 0
    }

    /// The key for authentication.
    ///
    /// Only valid if `is_auth` is true.
    pub fn key(self) -> u8 {
        ((self.0 >> 49) & 3) as u8
    }
}

/// A chained pointer for `DYLD_CHAINED_PTR_64` and `DYLD_CHAINED_PTR_64_OFFSET`.
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedPtr64(pub u64);

impl DyldChainedPtr64 {
    /// The offset to the next chained pointer, in units of the stride.
    pub fn next(self) -> u64 {
        (self.0 >> 51) & ((1 << 12) - 1)
    }

    /// Whether this is a bind or a rebase.
    pub fn is_bind(self) -> bool {
        (self.0 >> 63) & 1 != 0
    }

    /// Get the bind fields.
    pub fn bind(self) -> DyldChainedPtr64Bind {
        DyldChainedPtr64Bind(self.0)
    }

    /// Get the rebase fields.
    pub fn rebase(self) -> DyldChainedPtr64Rebase {
        DyldChainedPtr64Rebase(self.0)
    }
}

/// The rebase fields for [`DyldChainedPtr64`].
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedPtr64Rebase(pub u64);

impl DyldChainedPtr64Rebase {
    /// The target.
    ///
    /// A vmaddr for `DYLD_CHAINED_PTR_64`, or a runtime offset for
    /// `DYLD_CHAINED_PTR_64_OFFSET`.
    pub fn target(self) -> u64 {
        self.0 & ((1 << 36) - 1)
    }

    /// The top 8 bits of the pointer.
    pub fn high8(self) -> u64 {
        (self.0 >> 36) & 0xff
    }
}

/// The bind fields for [`DyldChainedPtr64`].
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedPtr64Bind(pub u64);

impl DyldChainedPtr64Bind {
    /// The 24-bit import ordinal.
    pub fn ordinal(self) -> u32 {
        (self.0 & ((1 << 24) - 1)) as u32
    }

    /// The unsigned 8-bit addend.
    pub fn addend(self) -> i32 {
        // No sign extend.
        ((self.0 >> 24) & 0xff) as i32
    }
}

/// A chained pointer for `DYLD_CHAINED_PTR_64_KERNEL_CACHE` and
/// `DYLD_CHAINED_PTR_X86_64_KERNEL_CACHE`.
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedPtr64KernelCacheRebase(pub u64);

impl DyldChainedPtr64KernelCacheRebase {
    /// The offset to the next chained pointer, in units of the stride.
    pub fn next(self) -> u64 {
        (self.0 >> 51) & ((1 << 12) - 1)
    }

    /// Whether the pointer is authenticated.
    pub fn is_auth(self) -> bool {
        (self.0 >> 63) & 1 != 0
    }

    /// The target.
    pub fn target(self) -> u64 {
        self.0 & ((1 << 30) - 1)
    }

    /// The cache level to bind to.
    pub fn cache_level(self) -> u8 {
        ((self.0 >> 30) & 3) as u8
    }

    /// The diversity value for authentication.
    ///
    /// Only valid if `is_auth` is true.
    pub fn diversity(self) -> u16 {
        ((self.0 >> 32) & 0xffff) as u16
    }

    /// Whether to use address diversity for authentication.
    ///
    /// Only valid if `is_auth` is true.
    pub fn addr_div(self) -> bool {
        (self.0 >> 48) & 1 != 0
    }

    /// The key for authentication.
    ///
    /// Only valid if `is_auth` is true.
    pub fn key(self) -> u8 {
        ((self.0 >> 49) & 3) as u8
    }
}

/// A chained pointer for `DYLD_CHAINED_PTR_32`.
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedPtr32(pub u32);

impl DyldChainedPtr32 {
    /// The offset to the next chained pointer, in units of the stride.
    pub fn next(self) -> u32 {
        (self.0 >> 26) & ((1 << 5) - 1)
    }

    /// Whether this is a bind or a rebase.
    pub fn is_bind(self) -> bool {
        (self.0 >> 31) & 1 != 0
    }

    /// Get the bind fields.
    pub fn bind(self) -> DyldChainedPtr32Bind {
        DyldChainedPtr32Bind(self.0)
    }

    /// Get the rebase fields.
    pub fn rebase(self) -> DyldChainedPtr32Rebase {
        DyldChainedPtr32Rebase(self.0)
    }
}

/// The rebase fields for [`DyldChainedPtr32`].
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedPtr32Rebase(pub u32);

impl DyldChainedPtr32Rebase {
    /// The target vmaddr.
    pub fn target(self) -> u32 {
        self.0 & ((1 << 26) - 1)
    }
}

/// The bind fields for [`DyldChainedPtr32`].
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedPtr32Bind(pub u32);

impl DyldChainedPtr32Bind {
    /// The import ordinal.
    pub fn ordinal(self) -> u32 {
        self.0 & ((1 << 20) - 1)
    }

    /// The unsigned 6-bit addend.
    pub fn addend(self) -> i32 {
        // No sign extend.
        ((self.0 >> 20) & ((1 << 6) - 1)) as i32
    }
}

/// A chained pointer for `DYLD_CHAINED_PTR_32_CACHE`.
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedPtr32CacheRebase(pub u32);

impl DyldChainedPtr32CacheRebase {
    /// The target.
    pub fn target(self) -> u32 {
        self.0 & ((1 << 30) - 1)
    }

    /// The offset to the next chained pointer, in units of the stride.
    pub fn next(self) -> u32 {
        (self.0 >> 30) & ((1 << 2) - 1)
    }
}

/// A chained pointer for `DYLD_CHAINED_PTR_32_FIRMWARE`.
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedPtr32FirmwareRebase(pub u32);

impl DyldChainedPtr32FirmwareRebase {
    /// The target.
    pub fn target(self) -> u32 {
        self.0 & ((1 << 26) - 1)
    }

    /// The offset to the next chained pointer, in units of the stride.
    pub fn next(self) -> u32 {
        (self.0 >> 26) & ((1 << 6) - 1)
    }
}

/// A chained pointer for `DYLD_CHAINED_PTR_ARM64E_SHARED_CACHE`.
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedPtrArm64eSharedCache(pub u64);

impl DyldChainedPtrArm64eSharedCache {
    /// The offset to the next chained pointer, in units of the stride.
    pub fn next(self) -> u64 {
        (self.0 >> 52) & ((1 << 11) - 1)
    }
    /// Whether the pointer is authenticated.
    pub fn is_auth(self) -> bool {
        (self.0 >> 63) & 1 != 0
    }

    /// Get the rebase fields.
    pub fn rebase(self) -> DyldChainedPtrArm64eSharedCacheRebase {
        DyldChainedPtrArm64eSharedCacheRebase(self.0)
    }

    /// Get the authenticated rebase fields.
    pub fn auth_rebase(self) -> DyldChainedPtrArm64eSharedCacheAuthRebase {
        DyldChainedPtrArm64eSharedCacheAuthRebase(self.0)
    }
}

/// The unauthenticated rebase fields for [`DyldChainedPtrArm64eSharedCache`].
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedPtrArm64eSharedCacheRebase(pub u64);

impl DyldChainedPtrArm64eSharedCacheRebase {
    /// The offset from the start of the shared cache.
    pub fn runtime_offset(self) -> u64 {
        self.0 & ((1 << 34) - 1)
    }

    /// The top 8 bits of the pointer.
    ///
    /// Only valid if `is_auth` is false.
    pub fn high8(self) -> u64 {
        (self.0 >> 34) & 0xff
    }
}

/// The authenticated rebase fields for [`DyldChainedPtrArm64eSharedCache`].
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedPtrArm64eSharedCacheAuthRebase(pub u64);

impl DyldChainedPtrArm64eSharedCacheAuthRebase {
    /// The offset from the start of the shared cache.
    pub fn runtime_offset(self) -> u64 {
        self.0 & ((1 << 34) - 1)
    }

    /// The diversity value for authentication.
    pub fn diversity(self) -> u16 {
        ((self.0 >> 34) & 0xffff) as u16
    }

    /// Whether to use address diversity for authentication.
    pub fn addr_div(self) -> bool {
        (self.0 >> 50) & 1 != 0
    }

    /// Whether the key is IA (false) or DA (true).
    pub fn key_is_data(self) -> bool {
        (self.0 >> 51) & 1 != 0
    }
}

newtype!(
    /// Value for `DyldChainedFixupsHeader::imports_format`.
    struct DyldChainedImportFormat(u32);
);

newtype_constant_names!(NAMES_DYLD_CHAINED_IMPORT_FORMAT: DyldChainedImportFormat(u32) = {
    DYLD_CHAINED_IMPORT = 1,
    DYLD_CHAINED_IMPORT_ADDEND = 2,
    DYLD_CHAINED_IMPORT_ADDEND64 = 3,
});

newtype!(
    /// An entry in the imports table, for `DYLD_CHAINED_IMPORT` and `DYLD_CHAINED_IMPORT_ADDEND`.
    ///
    /// For `DYLD_CHAINED_IMPORT_ADDEND`, this is followed by an i32 addend.
    #[derive(Debug)]
    struct DyldChainedImport32(u32);
);

impl DyldChainedImport32 {
    /// The ordinal of the library that the symbol is imported from.
    ///
    /// `0` and `0xF1..` are special `BindDylib` values.
    pub fn lib_ordinal(self) -> u8 {
        (self.0 & 0xff) as u8
    }

    /// Return `lib_ordinal` as a `BindDylib`.
    pub fn dylib(self) -> BindDylib {
        let lib_ordinal = self.lib_ordinal();
        if lib_ordinal > 0xf0 {
            BindDylib((lib_ordinal as i8).into())
        } else {
            BindDylib(lib_ordinal.into())
        }
    }

    /// Whether this is a weak import.
    pub fn weak_import(self) -> bool {
        (self.0 >> 8) & 1 != 0
    }

    /// The offset of the symbol name in the symbol string pool.
    pub fn name_offset(self) -> u32 {
        self.0 >> 9
    }
}

newtype!(
    /// An entry in the imports table, for `DYLD_CHAINED_IMPORT_ADDEND64`.
    ///
    /// This is followed by a u64 addend.
    #[derive(Debug)]
    struct DyldChainedImport64(u64);
);

impl DyldChainedImport64 {
    /// The ordinal of the library that the symbol is imported from.
    ///
    /// `0` and `0xFFF1..` are special `BindDylib` values.
    pub fn lib_ordinal(self) -> u16 {
        (self.0 & 0xffff) as u16
    }

    /// Return `lib_ordinal` as a `BindDylib`.
    pub fn dylib(self) -> BindDylib {
        let lib_ordinal = self.lib_ordinal();
        if lib_ordinal > 0xfff0 {
            BindDylib((lib_ordinal as i16).into())
        } else {
            BindDylib(lib_ordinal.into())
        }
    }

    /// Whether this is a weak import.
    pub fn weak_import(self) -> bool {
        (self.0 >> 16) & 1 != 0
    }

    /// The offset of the symbol name in the symbol string pool.
    pub fn name_offset(self) -> u32 {
        (self.0 >> 32) as u32
    }
}

// Definitions from:
// https://github.com/apple-oss-distributions/xnu/blob/rel/xnu-12377/osfmk/kern/cs_blobs.h

newtype!(
    struct CsFlags(u32);
);

newtype_flag_names!(NAMES_CS: CsFlags(u32) = {
    /// dynamically valid
    CS_VALID = 0x00000001,
    /// ad hoc signed
    CS_ADHOC = 0x00000002,
    /// has get-task-allow entitlement
    CS_GET_TASK_ALLOW = 0x00000004,
    /// has installer entitlement
    CS_INSTALLER = 0x00000008,

    /// Library Validation required by Hardened System Policy
    CS_FORCED_LV = 0x00000010,
    /// (macOS Only) Page invalidation allowed by task port policy
    CS_INVALID_ALLOWED = 0x00000020,

    /// don't load invalid pages
    CS_HARD = 0x00000100,
    /// kill process if it becomes invalid
    CS_KILL = 0x00000200,
    /// force expiration checking
    CS_CHECK_EXPIRATION = 0x00000400,
    /// tell dyld to treat restricted
    CS_RESTRICT = 0x00000800,

    /// require enforcement
    CS_ENFORCEMENT = 0x00001000,
    /// require library validation
    CS_REQUIRE_LV = 0x00002000,
    /// code signature permits restricted entitlements
    CS_ENTITLEMENTS_VALIDATED = 0x00004000,
    /// has com.apple.rootless.restricted-nvram-variables.heritable entitlement
    CS_NVRAM_UNRESTRICTED = 0x00008000,

    /// Apply hardened runtime policies
    CS_RUNTIME = 0x00010000,
    /// Automatically signed by the linker
    CS_LINKER_SIGNED = 0x00020000,

    /// set CS_HARD on any exec'ed process
    CS_EXEC_SET_HARD = 0x00100000,
    /// set CS_KILL on any exec'ed process
    CS_EXEC_SET_KILL = 0x00200000,
    /// set CS_ENFORCEMENT on any exec'ed process
    CS_EXEC_SET_ENFORCEMENT = 0x00400000,
    /// set CS_INSTALLER on any exec'ed process
    CS_EXEC_INHERIT_SIP = 0x00800000,

    /// was killed by kernel for invalidity
    CS_KILLED = 0x01000000,
    /// kernel did not load a non-platform-binary dyld or Rosetta runtime
    CS_NO_UNTRUSTED_HELPERS = 0x02000000,
    /// old name
    CS_DYLD_PLATFORM = CS_NO_UNTRUSTED_HELPERS.0,
    /// this is a platform binary
    CS_PLATFORM_BINARY = 0x04000000,
    /// platform binary by the fact of path (osx only)
    CS_PLATFORM_PATH = 0x08000000,

    /// process is currently or has previously been debugged and allowed to run with invalid pages
    CS_DEBUGGED = 0x10000000,
    /// process has a signature (may have gone invalid)
    CS_SIGNED = 0x20000000,
    /// code is dev signed, cannot be loaded into prod signed code (will go away with rdar://problem/28322552)
    CS_DEV_CODE = 0x40000000,
    /// has Data Vault controller entitlement
    CS_DATAVAULT_CONTROLLER = 0x80000000,
});

pub const CS_ALLOWED_MACHO: CsFlags = CS_ADHOC
    .with(CS_HARD)
    .with(CS_KILL)
    .with(CS_CHECK_EXPIRATION)
    .with(CS_RESTRICT)
    .with(CS_ENFORCEMENT)
    .with(CS_REQUIRE_LV)
    .with(CS_RUNTIME)
    .with(CS_LINKER_SIGNED);

pub const CS_ENTITLEMENT_FLAGS: CsFlags = CS_GET_TASK_ALLOW
    .with(CS_INSTALLER)
    .with(CS_DATAVAULT_CONTROLLER)
    .with(CS_NVRAM_UNRESTRICTED);

newtype!(
    struct CsExecSegFlags(u64);
);

newtype_flag_names!(NAMES_CS_EXECSEG: CsExecSegFlags(u64) = {
    /// executable segment denotes main binary
    CS_EXECSEG_MAIN_BINARY = 0x1,
    /// allow unsigned pages (for debugging)
    CS_EXECSEG_ALLOW_UNSIGNED = 0x10,
    /// main binary is debugger
    CS_EXECSEG_DEBUGGER = 0x20,
    /// JIT enabled
    CS_EXECSEG_JIT = 0x40,
    /// OBSOLETE: skip library validation
    CS_EXECSEG_SKIP_LV = 0x80,
    /// can bless cdhash for execution
    CS_EXECSEG_CAN_LOAD_CDHASH = 0x100,
    /// can execute blessed cdhash
    CS_EXECSEG_CAN_EXEC_CDHASH = 0x200,
});

/// single Requirement blob
pub const CSMAGIC_REQUIREMENT: u32 = 0xfade0c00;
/// Requirements vector (internal requirements)
pub const CSMAGIC_REQUIREMENTS: u32 = 0xfade0c01;
/// CodeDirectory blob
pub const CSMAGIC_CODEDIRECTORY: u32 = 0xfade0c02;
/// embedded form of signature data
pub const CSMAGIC_EMBEDDED_SIGNATURE: u32 = 0xfade0cc0;
pub const CSMAGIC_EMBEDDED_SIGNATURE_OLD: u32 = 0xfade0b02;
/// embedded entitlements
pub const CSMAGIC_EMBEDDED_ENTITLEMENTS: u32 = 0xfade7171;
/// embedded DER encoded entitlements
pub const CSMAGIC_EMBEDDED_DER_ENTITLEMENTS: u32 = 0xfade7172;
/// multi-arch collection of embedded signatures
pub const CSMAGIC_DETACHED_SIGNATURE: u32 = 0xfade0cc1;
/// CMS Signature, among other things
pub const CSMAGIC_BLOBWRAPPER: u32 = 0xfade0b01;
/// Light weight code requirement
pub const CSMAGIC_EMBEDDED_LAUNCH_CONSTRAINT: u32 = 0xfade8181;

newtype!(
    struct CsVersion(u32);
);

newtype_constant_names!(NAMES_CS_SUPPORT: CsVersion(u32) = {
    CS_SUPPORTSSCATTER = 0x20100,
    CS_SUPPORTSTEAMID = 0x20200,
    CS_SUPPORTSCODELIMIT64 = 0x20300,
    CS_SUPPORTSEXECSEG = 0x20400,
    CS_SUPPORTSRUNTIME = 0x20500,
    CS_SUPPORTSLINKAGE = 0x20600,
});

newtype!(
    struct CsSlot(u32);
);

newtype_constant_names!(NAMES_CSSLOT: CsSlot(u32) = {
    CSSLOT_CODEDIRECTORY = 0,
    CSSLOT_INFOSLOT = 1,
    CSSLOT_REQUIREMENTS = 2,
    CSSLOT_RESOURCEDIR = 3,
    CSSLOT_APPLICATION = 4,
    CSSLOT_ENTITLEMENTS = 5,
    CSSLOT_DER_ENTITLEMENTS = 7,
    CSSLOT_LAUNCH_CONSTRAINT_SELF = 8,
    CSSLOT_LAUNCH_CONSTRAINT_PARENT = 9,
    CSSLOT_LAUNCH_CONSTRAINT_RESPONSIBLE = 10,
    CSSLOT_LIBRARY_CONSTRAINT = 11,


    CSSLOT_SIGNATURESLOT = 0x10000,
    CSSLOT_IDENTIFICATIONSLOT = 0x10001,
    CSSLOT_TICKETSLOT = 0x10002,
});

impl CsSlot {
    pub fn is_alternate_codedirectory(self) -> bool {
        matches!(
            self.0,
            CSSLOT_ALTERNATE_CODEDIRECTORIES..CSSLOT_ALTERNATE_CODEDIRECTORY_LIMIT
        )
    }
}

/// first alternate CodeDirectory, if any
pub const CSSLOT_ALTERNATE_CODEDIRECTORIES: u32 = 0x1000;
/// max number of alternate CD slots
pub const CSSLOT_ALTERNATE_CODEDIRECTORY_MAX: u32 = 5;
pub const CSSLOT_ALTERNATE_CODEDIRECTORY_LIMIT: u32 =
    CSSLOT_ALTERNATE_CODEDIRECTORIES + CSSLOT_ALTERNATE_CODEDIRECTORY_MAX;

newtype!(
    #[repr(C)]
    struct CsHashType(u8);
);

newtype_constant_names!(NAMES_CS_HASHTYPE: CsHashType(u8) = {
    CS_HASHTYPE_SHA1              = 1,
    CS_HASHTYPE_SHA256            = 2,
    CS_HASHTYPE_SHA256_TRUNCATED  = 3,
    CS_HASHTYPE_SHA384 = 4,
});

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct CsCodeDirectoryV0 {
    /// magic number (CSMAGIC_CODEDIRECTORY)
    pub magic: U32<BigEndian>,
    /// total length of CodeDirectory blob
    pub length: U32<BigEndian>,
    /// compatibility version
    pub version: U32<BigEndian, CsVersion>,
    /// setup and mode flags
    pub flags: U32<BigEndian, CsFlags>,
    /// offset of hash slot element at index zero
    pub hash_offset: U32<BigEndian>,
    /// offset of identifier string
    pub ident_offset: U32<BigEndian>,
    /// number of special hash slots
    pub n_special_slots: U32<BigEndian>,
    /// number of ordinary (code) hash slots
    pub n_code_slots: U32<BigEndian>,
    /// limit to main image signature range
    pub code_limit: U32<BigEndian>,
    /// size of each hash in bytes
    pub hash_size: u8,
    /// type of hash
    pub hash_type: CsHashType,
    /// platform identifier; zero if not platform binary
    pub platform: u8,
    /// log2(page size in bytes); 0 => infinite
    pub page_size: u8,
    /// unused (must be zero)
    pub spare2: U32<BigEndian>,
    //char end_earliest[0];
}

// Version 0x20100
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct CsCodeDirectoryV1 {
    /// offset of optional scatter vector
    pub scatter_offset: U32<BigEndian>,
}

// Version 0x20200
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct CsCodeDirectoryV2 {
    /// offset of optional team identifier
    pub team_offset: U32<BigEndian>,
}

// Version 0x20300
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct CsCodeDirectoryV3 {
    /// unused (must be zero)
    pub spare3: U32<BigEndian>,
    /// limit to main image signature range, 64 bits
    pub code_limit64: U64<BigEndian>,
}

// Version 0x20400
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct CsCodeDirectoryV4 {
    /// offset of executable segment
    pub exec_seg_base: U64<BigEndian>,
    /// limit of executable segment
    pub exec_seg_limit: U64<BigEndian>,
    /// exec segment flags
    pub exec_seg_flags: U64<BigEndian, CsExecSegFlags>,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct CsBlobIndex {
    /// type of entry
    pub slot: U32<BigEndian, CsSlot>,
    /// offset of entry
    pub offset: U32<BigEndian>,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct CsSuperBlob {
    /// magic number
    pub magic: U32<BigEndian>,
    /// total length of SuperBlob
    pub length: U32<BigEndian>,
    /// number of index entries following
    pub count: U32<BigEndian>,
    // (count) entries
    // index: [CsBlobIndex]
    // followed by Blobs in no particular order as indicated by offsets in index
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct CsGenericBlob {
    /// magic number
    pub magic: U32<BigEndian>,
    /// total length of blob
    pub length: U32<BigEndian>,
    // data: [u8],
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct CsScatter {
    /// number of pages; zero for sentinel (only)
    pub count: U32<BigEndian>,
    /// first page number
    pub base: U32<BigEndian>,
    /// byte offset in target
    pub target_offset: U64<BigEndian>,
    /// reserved (must be zero)
    pub spare: U64<BigEndian>,
}

unsafe_impl_pod!(FatHeader, FatArch32, FatArch64,);
unsafe_impl_endian_pod!(
    DyldCacheHeader,
    DyldCacheMappingInfo,
    DyldCacheMappingAndSlideInfo,
    DyldCacheTproMappingInfo,
    DyldCacheImageInfo,
    DyldCacheSlideInfo2,
    DyldCacheSlideInfo3,
    DyldCacheSlideInfo5,
    DyldSubCacheEntryV1,
    DyldSubCacheEntryV2,
    MachHeader32,
    MachHeader64,
    LoadCommand,
    LcStr,
    SegmentCommand32,
    SegmentCommand64,
    Section32,
    Section64,
    Fvmlib,
    FvmlibCommand,
    Dylib,
    DylibCommand,
    DylibUseCommand,
    SubFrameworkCommand,
    SubClientCommand,
    SubUmbrellaCommand,
    SubLibraryCommand,
    PreboundDylibCommand,
    DylinkerCommand,
    ThreadCommand,
    RoutinesCommand32,
    RoutinesCommand64,
    SymtabCommand,
    DysymtabCommand,
    DylibTableOfContents,
    DylibModule32,
    DylibModule64,
    DylibReference,
    TwolevelHintsCommand,
    TwolevelHint,
    PrebindCksumCommand,
    UuidCommand,
    RpathCommand,
    TargetTripleCommand,
    LinkeditDataCommand,
    EncryptionInfoCommand32,
    EncryptionInfoCommand64,
    VersionMinCommand,
    BuildVersionCommand,
    BuildToolVersion,
    DyldInfoCommand,
    DyldChainedFixupsHeader,
    DyldChainedStartsInImage,
    DyldChainedStartsInSegment,
    DyldChainedStartsOffsets,
    LinkerOptionCommand,
    SymsegCommand,
    IdentCommand,
    FvmfileCommand,
    EntryPointCommand,
    SourceVersionCommand,
    DataInCodeEntry,
    //TlvDescriptor,
    NoteCommand,
    FilesetEntryCommand,
    Nlist32,
    Nlist64,
    Relocation,
);
unsafe_impl_pod!(
    CsBlobIndex,
    CsSuperBlob,
    CsCodeDirectoryV0,
    CsCodeDirectoryV1,
    CsCodeDirectoryV2,
    CsCodeDirectoryV3,
    CsCodeDirectoryV4,
    CsGenericBlob,
    CsScatter,
);
