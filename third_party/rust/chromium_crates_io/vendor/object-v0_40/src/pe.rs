//! PE/COFF definitions.
//!
//! These definitions are independent of read/write support, although we do implement
//! some traits useful for those.
//!
//! This module is based heavily on "winnt.h" (10.0.28000.0).

#![allow(missing_docs)]

use core::convert::TryInto;

#[cfg(feature = "names")]
use crate::constants::{ConstantNames, FlagNames};
use crate::endian::{I32, LittleEndian as LE, U16, U32, U64};
use crate::pod::Pod;

/// Platform-specific constant names for a PE/COFF file.
///
/// Returned by [`names`] and [`machine_names`].
#[cfg(feature = "names")]
#[derive(Debug)]
#[non_exhaustive]
pub struct Names {
    /// Values for `ImageRelocation::typ`.
    pub rel: &'static FlagNames<RelocationType>,
    /// Values for the type in an `ImageBaseRelocation` block.
    pub rel_based: &'static ConstantNames<BaseRelocationType>,
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
pub const fn machine_names(machine: Machine) -> &'static Names {
    match machine {
        IMAGE_FILE_MACHINE_I386 => I386::names(),
        IMAGE_FILE_MACHINE_MIPS16 | IMAGE_FILE_MACHINE_MIPSFPU | IMAGE_FILE_MACHINE_MIPSFPU16 => {
            Mips::names()
        }
        IMAGE_FILE_MACHINE_ALPHA | IMAGE_FILE_MACHINE_ALPHA64 => Alpha::names(),
        IMAGE_FILE_MACHINE_POWERPC
        | IMAGE_FILE_MACHINE_POWERPCFP
        | IMAGE_FILE_MACHINE_POWERPCBE => Ppc::names(),
        IMAGE_FILE_MACHINE_SH3
        | IMAGE_FILE_MACHINE_SH3DSP
        | IMAGE_FILE_MACHINE_SH3E
        | IMAGE_FILE_MACHINE_SH4
        | IMAGE_FILE_MACHINE_SH5 => Sh::names(),
        IMAGE_FILE_MACHINE_ARM => Arm::names(),
        IMAGE_FILE_MACHINE_AM33 => Am::names(),
        IMAGE_FILE_MACHINE_ARM64 => Arm64::names(),
        IMAGE_FILE_MACHINE_AMD64 => Amd64::names(),
        IMAGE_FILE_MACHINE_IA64 => Ia64::names(),
        IMAGE_FILE_MACHINE_CEF => Cef::names(),
        IMAGE_FILE_MACHINE_CEE => Cee::names(),
        IMAGE_FILE_MACHINE_M32R => M32r::names(),
        IMAGE_FILE_MACHINE_EBC => Ebc::names(),
        IMAGE_FILE_MACHINE_RISCV32 | IMAGE_FILE_MACHINE_RISCV64 | IMAGE_FILE_MACHINE_RISCV128 => {
            Riscv::names()
        }
        _ => Base::names(),
    }
}

names! {
    struct Base;
    flags rel: RelocationType = {};
    consts rel_based = NAMES_REL_BASED;
}

/// MZ
pub const IMAGE_DOS_SIGNATURE: u16 = 0x5A4D;
/// NE
pub const IMAGE_OS2_SIGNATURE: u16 = 0x454E;
/// LE
pub const IMAGE_OS2_SIGNATURE_LE: u16 = 0x454C;
/// LE
pub const IMAGE_VXD_SIGNATURE: u16 = 0x454C;
/// PE00
pub const IMAGE_NT_SIGNATURE: u32 = 0x0000_4550;

/// DOS .EXE header
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageDosHeader {
    /// Magic number
    pub e_magic: U16<LE>,
    /// Bytes on last page of file
    pub e_cblp: U16<LE>,
    /// Pages in file
    pub e_cp: U16<LE>,
    /// Relocations
    pub e_crlc: U16<LE>,
    /// Size of header in paragraphs
    pub e_cparhdr: U16<LE>,
    /// Minimum extra paragraphs needed
    pub e_minalloc: U16<LE>,
    /// Maximum extra paragraphs needed
    pub e_maxalloc: U16<LE>,
    /// Initial (relative) SS value
    pub e_ss: U16<LE>,
    /// Initial SP value
    pub e_sp: U16<LE>,
    /// Checksum
    pub e_csum: U16<LE>,
    /// Initial IP value
    pub e_ip: U16<LE>,
    /// Initial (relative) CS value
    pub e_cs: U16<LE>,
    /// File address of relocation table
    pub e_lfarlc: U16<LE>,
    /// Overlay number
    pub e_ovno: U16<LE>,
    /// Reserved words
    pub e_res: [U16<LE>; 4],
    /// OEM identifier (for e_oeminfo)
    pub e_oemid: U16<LE>,
    /// OEM information; e_oemid specific
    pub e_oeminfo: U16<LE>,
    /// Reserved words
    pub e_res2: [U16<LE>; 10],
    /// File address of new exe header
    pub e_lfanew: U32<LE>,
}

/// OS/2 .EXE header
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageOs2Header {
    /// Magic number
    pub ne_magic: U16<LE>,
    /// Version number
    pub ne_ver: i8,
    /// Revision number
    pub ne_rev: i8,
    /// Offset of Entry Table
    pub ne_enttab: U16<LE>,
    /// Number of bytes in Entry Table
    pub ne_cbenttab: U16<LE>,
    /// Checksum of whole file
    pub ne_crc: I32<LE>,
    /// Flag word
    pub ne_flags: U16<LE>,
    /// Automatic data segment number
    pub ne_autodata: U16<LE>,
    /// Initial heap allocation
    pub ne_heap: U16<LE>,
    /// Initial stack allocation
    pub ne_stack: U16<LE>,
    /// Initial CS:IP setting
    pub ne_csip: I32<LE>,
    /// Initial SS:SP setting
    pub ne_sssp: I32<LE>,
    /// Count of file segments
    pub ne_cseg: U16<LE>,
    /// Entries in Module Reference Table
    pub ne_cmod: U16<LE>,
    /// Size of non-resident name table
    pub ne_cbnrestab: U16<LE>,
    /// Offset of Segment Table
    pub ne_segtab: U16<LE>,
    /// Offset of Resource Table
    pub ne_rsrctab: U16<LE>,
    /// Offset of resident name table
    pub ne_restab: U16<LE>,
    /// Offset of Module Reference Table
    pub ne_modtab: U16<LE>,
    /// Offset of Imported Names Table
    pub ne_imptab: U16<LE>,
    /// Offset of Non-resident Names Table
    pub ne_nrestab: I32<LE>,
    /// Count of movable entries
    pub ne_cmovent: U16<LE>,
    /// Segment alignment shift count
    pub ne_align: U16<LE>,
    /// Count of resource segments
    pub ne_cres: U16<LE>,
    /// Target Operating system
    pub ne_exetyp: u8,
    /// Other .EXE flags
    pub ne_flagsothers: u8,
    /// offset to return thunks
    pub ne_pretthunks: U16<LE>,
    /// offset to segment ref. bytes
    pub ne_psegrefbytes: U16<LE>,
    /// Minimum code swap area size
    pub ne_swaparea: U16<LE>,
    /// Expected Windows version number
    pub ne_expver: U16<LE>,
}

/// Windows VXD header
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageVxdHeader {
    /// Magic number
    pub e32_magic: U16<LE>,
    /// The byte ordering for the VXD
    pub e32_border: u8,
    /// The word ordering for the VXD
    pub e32_worder: u8,
    /// The EXE format level for now = 0
    pub e32_level: U32<LE>,
    /// The CPU type
    pub e32_cpu: U16<LE>,
    /// The OS type
    pub e32_os: U16<LE>,
    /// Module version
    pub e32_ver: U32<LE>,
    /// Module flags
    pub e32_mflags: U32<LE>,
    /// Module # pages
    pub e32_mpages: U32<LE>,
    /// Object # for instruction pointer
    pub e32_startobj: U32<LE>,
    /// Extended instruction pointer
    pub e32_eip: U32<LE>,
    /// Object # for stack pointer
    pub e32_stackobj: U32<LE>,
    /// Extended stack pointer
    pub e32_esp: U32<LE>,
    /// VXD page size
    pub e32_pagesize: U32<LE>,
    /// Last page size in VXD
    pub e32_lastpagesize: U32<LE>,
    /// Fixup section size
    pub e32_fixupsize: U32<LE>,
    /// Fixup section checksum
    pub e32_fixupsum: U32<LE>,
    /// Loader section size
    pub e32_ldrsize: U32<LE>,
    /// Loader section checksum
    pub e32_ldrsum: U32<LE>,
    /// Object table offset
    pub e32_objtab: U32<LE>,
    /// Number of objects in module
    pub e32_objcnt: U32<LE>,
    /// Object page map offset
    pub e32_objmap: U32<LE>,
    /// Object iterated data map offset
    pub e32_itermap: U32<LE>,
    /// Offset of Resource Table
    pub e32_rsrctab: U32<LE>,
    /// Number of resource entries
    pub e32_rsrccnt: U32<LE>,
    /// Offset of resident name table
    pub e32_restab: U32<LE>,
    /// Offset of Entry Table
    pub e32_enttab: U32<LE>,
    /// Offset of Module Directive Table
    pub e32_dirtab: U32<LE>,
    /// Number of module directives
    pub e32_dircnt: U32<LE>,
    /// Offset of Fixup Page Table
    pub e32_fpagetab: U32<LE>,
    /// Offset of Fixup Record Table
    pub e32_frectab: U32<LE>,
    /// Offset of Import Module Name Table
    pub e32_impmod: U32<LE>,
    /// Number of entries in Import Module Name Table
    pub e32_impmodcnt: U32<LE>,
    /// Offset of Import Procedure Name Table
    pub e32_impproc: U32<LE>,
    /// Offset of Per-Page Checksum Table
    pub e32_pagesum: U32<LE>,
    /// Offset of Enumerated Data Pages
    pub e32_datapage: U32<LE>,
    /// Number of preload pages
    pub e32_preload: U32<LE>,
    /// Offset of Non-resident Names Table
    pub e32_nrestab: U32<LE>,
    /// Size of Non-resident Name Table
    pub e32_cbnrestab: U32<LE>,
    /// Non-resident Name Table Checksum
    pub e32_nressum: U32<LE>,
    /// Object # for automatic data object
    pub e32_autodata: U32<LE>,
    /// Offset of the debugging information
    pub e32_debuginfo: U32<LE>,
    /// The length of the debugging info. in bytes
    pub e32_debuglen: U32<LE>,
    /// Number of instance pages in preload section of VXD file
    pub e32_instpreload: U32<LE>,
    /// Number of instance pages in demand load section of VXD file
    pub e32_instdemand: U32<LE>,
    /// Size of heap - for 16-bit apps
    pub e32_heapsize: U32<LE>,
    /// Reserved words
    pub e32_res3: [u8; 12],
    pub e32_winresoff: U32<LE>,
    pub e32_winreslen: U32<LE>,
    /// Device ID for VxD
    pub e32_devid: U16<LE>,
    /// DDK version for VxD
    pub e32_ddkver: U16<LE>,
}

/// A PE rich header entry.
///
/// Rich headers have no official documentation, but have been heavily
/// reversed-engineered and documented in the wild, e.g.:
/// * `http://www.ntcore.com/files/richsign.htm`
/// * `https://www.researchgate.net/figure/Structure-of-the-Rich-Header_fig1_318145388`
///
/// This data is "masked", i.e. XORed with a checksum derived from the file data.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct MaskedRichHeaderEntry {
    pub masked_comp_id: U32<LE>,
    pub masked_count: U32<LE>,
}

//
// File header format.
//

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageFileHeader {
    pub machine: U16<LE, Machine>,
    pub number_of_sections: U16<LE>,
    pub time_date_stamp: U32<LE>,
    pub pointer_to_symbol_table: U32<LE>,
    pub number_of_symbols: U32<LE>,
    pub size_of_optional_header: U16<LE>,
    pub characteristics: U16<LE, FileFlags>,
}

pub const IMAGE_SIZEOF_FILE_HEADER: usize = 20;

newtype!(
    /// Values for `ImageFileHeader::characteristics`.
    struct FileFlags(u16);
);

newtype_flag_names!(NAMES_FILE_FLAGS: FileFlags(u16) = {
    /// Relocation info stripped from file.
    IMAGE_FILE_RELOCS_STRIPPED = 0x0001,
    /// File is executable  (i.e. no unresolved external references).
    IMAGE_FILE_EXECUTABLE_IMAGE = 0x0002,
    /// Line numbers stripped from file.
    IMAGE_FILE_LINE_NUMS_STRIPPED = 0x0004,
    /// Local symbols stripped from file.
    IMAGE_FILE_LOCAL_SYMS_STRIPPED = 0x0008,
    /// Aggressively trim working set
    IMAGE_FILE_AGGRESIVE_WS_TRIM = 0x0010,
    /// App can handle >2gb addresses
    IMAGE_FILE_LARGE_ADDRESS_AWARE = 0x0020,
    /// Bytes of machine word are reversed.
    IMAGE_FILE_BYTES_REVERSED_LO = 0x0080,
    /// 32 bit word machine.
    IMAGE_FILE_32BIT_MACHINE = 0x0100,
    /// Debugging info stripped from file in .DBG file
    IMAGE_FILE_DEBUG_STRIPPED = 0x0200,
    /// If Image is on removable media, copy and run from the swap file.
    IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP = 0x0400,
    /// If Image is on Net, copy and run from the swap file.
    IMAGE_FILE_NET_RUN_FROM_SWAP = 0x0800,
    /// System File.
    IMAGE_FILE_SYSTEM = 0x1000,
    /// File is a DLL.
    IMAGE_FILE_DLL = 0x2000,
    /// File should only be run on a UP machine
    IMAGE_FILE_UP_SYSTEM_ONLY = 0x4000,
    /// Bytes of machine word are reversed.
    IMAGE_FILE_BYTES_REVERSED_HI = 0x8000,
});

newtype!(
    /// Values for the `machine` field in file headers.
    struct Machine(u16);
);

newtype_constant_names!(NAMES_MACHINE: Machine(u16) = {
    IMAGE_FILE_MACHINE_UNKNOWN = 0,
    /// Useful for indicating we want to interact with the host and not a WoW guest.
    IMAGE_FILE_MACHINE_TARGET_HOST = 0x0001,
    /// Intel 386.
    IMAGE_FILE_MACHINE_I386 = 0x014c,
    /// MIPS little-endian, 0x160 big-endian
    IMAGE_FILE_MACHINE_R3000 = 0x0162,
    /// MIPS little-endian
    IMAGE_FILE_MACHINE_R4000 = 0x0166,
    /// MIPS little-endian
    IMAGE_FILE_MACHINE_R10000 = 0x0168,
    /// MIPS little-endian WCE v2
    IMAGE_FILE_MACHINE_WCEMIPSV2 = 0x0169,
    /// Alpha_AXP
    IMAGE_FILE_MACHINE_ALPHA = 0x0184,
    /// SH3 little-endian
    IMAGE_FILE_MACHINE_SH3 = 0x01a2,
    IMAGE_FILE_MACHINE_SH3DSP = 0x01a3,
    /// SH3E little-endian
    IMAGE_FILE_MACHINE_SH3E = 0x01a4,
    /// SH4 little-endian
    IMAGE_FILE_MACHINE_SH4 = 0x01a6,
    /// SH5
    IMAGE_FILE_MACHINE_SH5 = 0x01a8,
    /// ARM Little-Endian
    IMAGE_FILE_MACHINE_ARM = 0x01c0,
    /// ARM Thumb/Thumb-2 Little-Endian
    IMAGE_FILE_MACHINE_THUMB = 0x01c2,
    /// ARM Thumb-2 Little-Endian
    IMAGE_FILE_MACHINE_ARMNT = 0x01c4,
    IMAGE_FILE_MACHINE_AM33 = 0x01d3,
    /// IBM PowerPC Little-Endian
    IMAGE_FILE_MACHINE_POWERPC = 0x01F0,
    IMAGE_FILE_MACHINE_POWERPCFP = 0x01f1,
    /// IBM PowerPC Big-Endian
    IMAGE_FILE_MACHINE_POWERPCBE = 0x01f2,
    /// Intel 64
    IMAGE_FILE_MACHINE_IA64 = 0x0200,
    /// MIPS
    IMAGE_FILE_MACHINE_MIPS16 = 0x0266,
    /// ALPHA64
    IMAGE_FILE_MACHINE_ALPHA64 = 0x0284,
    /// MIPS
    IMAGE_FILE_MACHINE_MIPSFPU = 0x0366,
    /// MIPS
    IMAGE_FILE_MACHINE_MIPSFPU16 = 0x0466,
    IMAGE_FILE_MACHINE_AXP64 = IMAGE_FILE_MACHINE_ALPHA64.0,
    /// Infineon
    IMAGE_FILE_MACHINE_TRICORE = 0x0520,
    IMAGE_FILE_MACHINE_CEF = 0x0CEF,
    /// EFI Byte Code
    IMAGE_FILE_MACHINE_EBC = 0x0EBC,
    /// AMD64 (K8)
    IMAGE_FILE_MACHINE_AMD64 = 0x8664,
    /// M32R little-endian
    IMAGE_FILE_MACHINE_M32R = 0x9041,
    /// ARM64 Little-Endian
    IMAGE_FILE_MACHINE_ARM64 = 0xAA64,
    /// ARM64EC ("Emulation Compatible")
    IMAGE_FILE_MACHINE_ARM64EC = 0xA641,
    IMAGE_FILE_MACHINE_CEE = 0xC0EE,
    /// RISCV32
    IMAGE_FILE_MACHINE_RISCV32 = 0x5032,
    /// RISCV64
    IMAGE_FILE_MACHINE_RISCV64 = 0x5064,
    /// RISCV128
    IMAGE_FILE_MACHINE_RISCV128 = 0x5128,
    /// ARM64X (Mixed ARM64 and ARM64EC)
    IMAGE_FILE_MACHINE_ARM64X = 0xA64E,
    /// CHPE x86 ("Compiled Hybrid Portable Executable")
    IMAGE_FILE_MACHINE_CHPE_X86 = 0x3A64,
});

//
// Directory format.
//

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageDataDirectory {
    pub virtual_address: U32<LE>,
    pub size: U32<LE>,
}

pub const IMAGE_NUMBEROF_DIRECTORY_ENTRIES: usize = 16;

//
// Optional header format.
//

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageOptionalHeader32 {
    // Standard fields.
    pub magic: U16<LE>,
    pub major_linker_version: u8,
    pub minor_linker_version: u8,
    pub size_of_code: U32<LE>,
    pub size_of_initialized_data: U32<LE>,
    pub size_of_uninitialized_data: U32<LE>,
    pub address_of_entry_point: U32<LE>,
    pub base_of_code: U32<LE>,
    pub base_of_data: U32<LE>,

    // NT additional fields.
    pub image_base: U32<LE>,
    pub section_alignment: U32<LE>,
    pub file_alignment: U32<LE>,
    pub major_operating_system_version: U16<LE>,
    pub minor_operating_system_version: U16<LE>,
    pub major_image_version: U16<LE>,
    pub minor_image_version: U16<LE>,
    pub major_subsystem_version: U16<LE>,
    pub minor_subsystem_version: U16<LE>,
    pub win32_version_value: U32<LE>,
    pub size_of_image: U32<LE>,
    pub size_of_headers: U32<LE>,
    pub check_sum: U32<LE>,
    pub subsystem: U16<LE, Subsystem>,
    pub dll_characteristics: U16<LE, DllFlags>,
    pub size_of_stack_reserve: U32<LE>,
    pub size_of_stack_commit: U32<LE>,
    pub size_of_heap_reserve: U32<LE>,
    pub size_of_heap_commit: U32<LE>,
    pub loader_flags: U32<LE>,
    pub number_of_rva_and_sizes: U32<LE>,
    //pub data_directory: [ImageDataDirectory; IMAGE_NUMBEROF_DIRECTORY_ENTRIES],
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageRomOptionalHeader {
    pub magic: U16<LE>,
    pub major_linker_version: u8,
    pub minor_linker_version: u8,
    pub size_of_code: U32<LE>,
    pub size_of_initialized_data: U32<LE>,
    pub size_of_uninitialized_data: U32<LE>,
    pub address_of_entry_point: U32<LE>,
    pub base_of_code: U32<LE>,
    pub base_of_data: U32<LE>,
    pub base_of_bss: U32<LE>,
    pub gpr_mask: U32<LE>,
    pub cpr_mask: [U32<LE>; 4],
    pub gp_value: U32<LE>,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageOptionalHeader64 {
    pub magic: U16<LE>,
    pub major_linker_version: u8,
    pub minor_linker_version: u8,
    pub size_of_code: U32<LE>,
    pub size_of_initialized_data: U32<LE>,
    pub size_of_uninitialized_data: U32<LE>,
    pub address_of_entry_point: U32<LE>,
    pub base_of_code: U32<LE>,
    pub image_base: U64<LE>,
    pub section_alignment: U32<LE>,
    pub file_alignment: U32<LE>,
    pub major_operating_system_version: U16<LE>,
    pub minor_operating_system_version: U16<LE>,
    pub major_image_version: U16<LE>,
    pub minor_image_version: U16<LE>,
    pub major_subsystem_version: U16<LE>,
    pub minor_subsystem_version: U16<LE>,
    pub win32_version_value: U32<LE>,
    pub size_of_image: U32<LE>,
    pub size_of_headers: U32<LE>,
    pub check_sum: U32<LE>,
    pub subsystem: U16<LE, Subsystem>,
    pub dll_characteristics: U16<LE, DllFlags>,
    pub size_of_stack_reserve: U64<LE>,
    pub size_of_stack_commit: U64<LE>,
    pub size_of_heap_reserve: U64<LE>,
    pub size_of_heap_commit: U64<LE>,
    pub loader_flags: U32<LE>,
    pub number_of_rva_and_sizes: U32<LE>,
    //pub data_directory: [ImageDataDirectory; IMAGE_NUMBEROF_DIRECTORY_ENTRIES],
}

pub const IMAGE_NT_OPTIONAL_HDR32_MAGIC: u16 = 0x10b;
pub const IMAGE_NT_OPTIONAL_HDR64_MAGIC: u16 = 0x20b;
pub const IMAGE_ROM_OPTIONAL_HDR_MAGIC: u16 = 0x107;

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageNtHeaders64 {
    pub signature: U32<LE>,
    pub file_header: ImageFileHeader,
    pub optional_header: ImageOptionalHeader64,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageNtHeaders32 {
    pub signature: U32<LE>,
    pub file_header: ImageFileHeader,
    pub optional_header: ImageOptionalHeader32,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageRomHeaders {
    pub file_header: ImageFileHeader,
    pub optional_header: ImageRomOptionalHeader,
}

newtype!(
    /// Values for `ImageOptionalHeader*::subsystem`.
    struct Subsystem(u16);
);

newtype_constant_names!(NAMES_SUBSYSTEM: Subsystem(u16) = {
    /// Unknown subsystem.
    IMAGE_SUBSYSTEM_UNKNOWN = 0,
    /// Image doesn't require a subsystem.
    IMAGE_SUBSYSTEM_NATIVE = 1,
    /// Image runs in the Windows GUI subsystem.
    IMAGE_SUBSYSTEM_WINDOWS_GUI = 2,
    /// Image runs in the Windows character subsystem.
    IMAGE_SUBSYSTEM_WINDOWS_CUI = 3,
    /// image runs in the OS/2 character subsystem.
    IMAGE_SUBSYSTEM_OS2_CUI = 5,
    /// image runs in the Posix character subsystem.
    IMAGE_SUBSYSTEM_POSIX_CUI = 7,
    /// image is a native Win9x driver.
    IMAGE_SUBSYSTEM_NATIVE_WINDOWS = 8,
    /// Image runs in the Windows CE subsystem.
    IMAGE_SUBSYSTEM_WINDOWS_CE_GUI = 9,
    IMAGE_SUBSYSTEM_EFI_APPLICATION = 10,
    IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER = 11,
    IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER = 12,
    IMAGE_SUBSYSTEM_EFI_ROM = 13,
    IMAGE_SUBSYSTEM_XBOX = 14,
    IMAGE_SUBSYSTEM_WINDOWS_BOOT_APPLICATION = 16,
    IMAGE_SUBSYSTEM_XBOX_CODE_CATALOG = 17,
});

newtype!(
    /// Values for `ImageOptionalHeader*::dll_characteristics`.
    struct DllFlags(u16);
);

newtype_flag_names!(NAMES_DLL_FLAGS: DllFlags(u16) = {
    //      IMAGE_LIBRARY_PROCESS_INIT            0x0001     // Reserved.
    //      IMAGE_LIBRARY_PROCESS_TERM            0x0002     // Reserved.
    //      IMAGE_LIBRARY_THREAD_INIT             0x0004     // Reserved.
    //      IMAGE_LIBRARY_THREAD_TERM             0x0008     // Reserved.
    /// Image can handle a high entropy 64-bit virtual address space.
    IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA = 0x0020,
    /// DLL can move.
    IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE = 0x0040,
    /// Code Integrity Image
    IMAGE_DLLCHARACTERISTICS_FORCE_INTEGRITY = 0x0080,
    /// Image is NX compatible
    IMAGE_DLLCHARACTERISTICS_NX_COMPAT = 0x0100,
    /// Image understands isolation and doesn't want it
    IMAGE_DLLCHARACTERISTICS_NO_ISOLATION = 0x0200,
    /// Image does not use SEH.  No SE handler may reside in this image
    IMAGE_DLLCHARACTERISTICS_NO_SEH = 0x0400,
    /// Do not bind this image.
    IMAGE_DLLCHARACTERISTICS_NO_BIND = 0x0800,
    /// Image should execute in an AppContainer
    IMAGE_DLLCHARACTERISTICS_APPCONTAINER = 0x1000,
    /// Driver uses WDM model
    IMAGE_DLLCHARACTERISTICS_WDM_DRIVER = 0x2000,
    /// Image supports Control Flow Guard.
    IMAGE_DLLCHARACTERISTICS_GUARD_CF = 0x4000,
    IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE = 0x8000,
});

constant_names!(
/// Indices for `ImageOptionalHeader*::data_directory`.
pub NAMES_DIRECTORY_ENTRY: usize = {
    /// Export Directory
    IMAGE_DIRECTORY_ENTRY_EXPORT = 0,
    /// Import Directory
    IMAGE_DIRECTORY_ENTRY_IMPORT = 1,
    /// Resource Directory
    IMAGE_DIRECTORY_ENTRY_RESOURCE = 2,
    /// Exception Directory
    IMAGE_DIRECTORY_ENTRY_EXCEPTION = 3,
    /// Security Directory
    IMAGE_DIRECTORY_ENTRY_SECURITY = 4,
    /// Base Relocation Table
    IMAGE_DIRECTORY_ENTRY_BASERELOC = 5,
    /// Debug Directory
    IMAGE_DIRECTORY_ENTRY_DEBUG = 6,
    /// Architecture Specific Data
    IMAGE_DIRECTORY_ENTRY_ARCHITECTURE = 7,
    /// RVA of GP
    IMAGE_DIRECTORY_ENTRY_GLOBALPTR = 8,
    /// TLS Directory
    IMAGE_DIRECTORY_ENTRY_TLS = 9,
    /// Load Configuration Directory
    IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG = 10,
    /// Bound Import Directory in headers
    IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT = 11,
    /// Import Address Table
    IMAGE_DIRECTORY_ENTRY_IAT = 12,
    /// Delay Load Import Descriptors
    IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT = 13,
    /// COM Runtime descriptor
    IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR = 14,
});

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(C)]
pub struct Guid(pub [u8; 16]);

impl Guid {
    #[inline]
    pub fn data1(self) -> U32<LE> {
        U32::from_bytes(self.0[0..4].try_into().unwrap())
    }

    #[inline]
    pub fn data2(self) -> U16<LE> {
        U16::from_bytes(self.0[4..6].try_into().unwrap())
    }

    #[inline]
    pub fn data3(self) -> U16<LE> {
        U16::from_bytes(self.0[6..8].try_into().unwrap())
    }

    #[inline]
    pub fn data4(self) -> [u8; 8] {
        self.0[8..16].try_into().unwrap()
    }
}

pub use Guid as ClsId;

/// Non-COFF Object file header
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct AnonObjectHeader {
    /// Must be IMAGE_FILE_MACHINE_UNKNOWN
    pub sig1: U16<LE, Machine>,
    /// Must be 0xffff
    pub sig2: U16<LE>,
    /// >= 1 (implies the ClsId field is present)
    pub version: U16<LE>,
    pub machine: U16<LE, Machine>,
    pub time_date_stamp: U32<LE>,
    /// Used to invoke CoCreateInstance
    pub class_id: ClsId,
    /// Size of data that follows the header
    pub size_of_data: U32<LE>,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct AnonObjectHeaderV2 {
    /// Must be IMAGE_FILE_MACHINE_UNKNOWN
    pub sig1: U16<LE, Machine>,
    /// Must be 0xffff
    pub sig2: U16<LE>,
    /// >= 2 (implies the Flags field is present - otherwise V1)
    pub version: U16<LE>,
    pub machine: U16<LE, Machine>,
    pub time_date_stamp: U32<LE>,
    /// Used to invoke CoCreateInstance
    pub class_id: ClsId,
    /// Size of data that follows the header
    pub size_of_data: U32<LE>,
    /// 0x1 -> contains metadata
    pub flags: U32<LE>,
    /// Size of CLR metadata
    pub meta_data_size: U32<LE>,
    /// Offset of CLR metadata
    pub meta_data_offset: U32<LE>,
}

/// The required value of `AnonObjectHeaderBigobj::class_id`.
pub const ANON_OBJECT_HEADER_BIGOBJ_CLASS_ID: ClsId = ClsId([
    0xC7, 0xA1, 0xBA, 0xD1, 0xEE, 0xBA, 0xA9, 0x4B, 0xAF, 0x20, 0xFA, 0xF6, 0x6A, 0xA4, 0xDC, 0xB8,
]);

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct AnonObjectHeaderBigobj {
    /* same as ANON_OBJECT_HEADER_V2 */
    /// Must be IMAGE_FILE_MACHINE_UNKNOWN
    pub sig1: U16<LE, Machine>,
    /// Must be 0xffff
    pub sig2: U16<LE>,
    /// >= 2 (implies the Flags field is present)
    pub version: U16<LE>,
    /// Actual machine - IMAGE_FILE_MACHINE_xxx
    pub machine: U16<LE, Machine>,
    pub time_date_stamp: U32<LE>,
    /// Must be `ANON_OBJECT_HEADER_BIGOBJ_CLASS_ID`.
    pub class_id: ClsId,
    /// Size of data that follows the header
    pub size_of_data: U32<LE>,
    /// 0x1 -> contains metadata
    pub flags: U32<LE>,
    /// Size of CLR metadata
    pub meta_data_size: U32<LE>,
    /// Offset of CLR metadata
    pub meta_data_offset: U32<LE>,

    /* bigobj specifics */
    /// extended from WORD
    pub number_of_sections: U32<LE>,
    pub pointer_to_symbol_table: U32<LE>,
    pub number_of_symbols: U32<LE>,
}

pub const IMAGE_SIZEOF_SHORT_NAME: usize = 8;

//
// Section header format.
//

#[derive(Debug, Default, Clone, Copy)]
#[repr(C)]
pub struct ImageSectionHeader {
    pub name: [u8; IMAGE_SIZEOF_SHORT_NAME],
    pub virtual_size: U32<LE>,
    pub virtual_address: U32<LE>,
    pub size_of_raw_data: U32<LE>,
    pub pointer_to_raw_data: U32<LE>,
    pub pointer_to_relocations: U32<LE>,
    pub pointer_to_linenumbers: U32<LE>,
    pub number_of_relocations: U16<LE>,
    pub number_of_linenumbers: U16<LE>,
    pub characteristics: U32<LE, SectionFlags>,
}

pub const IMAGE_SIZEOF_SECTION_HEADER: usize = 40;

newtype!(
    /// Values for `ImageSectionHeader::characteristics`.
    struct SectionFlags(u32);
);

impl SectionFlags {
    /// Return the alignment.
    pub fn align(self) -> SectionFlags {
        SectionFlags(self.0 & IMAGE_SCN_ALIGN_MASK)
    }

    /// Replace the alignment.
    pub fn with_align(self, align: SectionFlags) -> SectionFlags {
        SectionFlags(self.0 & !IMAGE_SCN_ALIGN_MASK | align.0 & IMAGE_SCN_ALIGN_MASK)
    }
}

newtype_flag_names!(NAMES_SCN_FLAGS: SectionFlags(u32) = {
    //      IMAGE_SCN_TYPE_REG                   0x00000000  // Reserved.
    //      IMAGE_SCN_TYPE_DSECT                 0x00000001  // Reserved.
    //      IMAGE_SCN_TYPE_NOLOAD                0x00000002  // Reserved.
    //      IMAGE_SCN_TYPE_GROUP                 0x00000004  // Reserved.
    /// Reserved.
    IMAGE_SCN_TYPE_NO_PAD = 0x0000_0008,
    //      IMAGE_SCN_TYPE_COPY                  0x00000010  // Reserved.

    /// Section contains code.
    IMAGE_SCN_CNT_CODE = 0x0000_0020,
    /// Section contains initialized data.
    IMAGE_SCN_CNT_INITIALIZED_DATA = 0x0000_0040,
    /// Section contains uninitialized data.
    IMAGE_SCN_CNT_UNINITIALIZED_DATA = 0x0000_0080,

    /// Reserved.
    IMAGE_SCN_LNK_OTHER = 0x0000_0100,
    /// Section contains comments or some other type of information.
    IMAGE_SCN_LNK_INFO = 0x0000_0200,
    //      IMAGE_SCN_TYPE_OVER                  0x00000400  // Reserved.
    /// Section contents will not become part of image.
    IMAGE_SCN_LNK_REMOVE = 0x0000_0800,
    /// Section contents comdat.
    IMAGE_SCN_LNK_COMDAT = 0x0000_1000,
    //                                           0x00002000  // Reserved.
    //      IMAGE_SCN_MEM_PROTECTED - Obsolete   0x00004000
    /// Reset speculative exceptions handling bits in the TLB entries for this section.
    IMAGE_SCN_NO_DEFER_SPEC_EXC = 0x0000_4000,
    /// Section content can be accessed relative to GP
    IMAGE_SCN_GPREL = 0x0000_8000,
    IMAGE_SCN_MEM_FARDATA = 0x0000_8000,
    //      IMAGE_SCN_MEM_SYSHEAP  - Obsolete    0x00010000
    IMAGE_SCN_MEM_PURGEABLE = 0x0002_0000,
    IMAGE_SCN_MEM_16BIT = 0x0002_0000,
    IMAGE_SCN_MEM_LOCKED = 0x0004_0000,
    IMAGE_SCN_MEM_PRELOAD = 0x0008_0000,

    IMAGE_SCN_ALIGN_MASK = 0x00F0_0000 => NAMES_SCN_ALIGN,

    /// Section contains extended relocations.
    IMAGE_SCN_LNK_NRELOC_OVFL = 0x0100_0000,
    /// Section can be discarded.
    IMAGE_SCN_MEM_DISCARDABLE = 0x0200_0000,
    /// Section is not cacheable.
    IMAGE_SCN_MEM_NOT_CACHED = 0x0400_0000,
    /// Section is not pageable.
    IMAGE_SCN_MEM_NOT_PAGED = 0x0800_0000,
    /// Section is shareable.
    IMAGE_SCN_MEM_SHARED = 0x1000_0000,
    /// Section is executable.
    IMAGE_SCN_MEM_EXECUTE = 0x2000_0000,
    /// Section is readable.
    IMAGE_SCN_MEM_READ = 0x4000_0000,
    /// Section is writeable.
    IMAGE_SCN_MEM_WRITE = 0x8000_0000,
});

constant_names!(NAMES_SCN_ALIGN: SectionFlags(u32) = {
    IMAGE_SCN_ALIGN_1BYTES = 0x0010_0000,
    IMAGE_SCN_ALIGN_2BYTES = 0x0020_0000,
    IMAGE_SCN_ALIGN_4BYTES = 0x0030_0000,
    IMAGE_SCN_ALIGN_8BYTES = 0x0040_0000,
    /// Default alignment if no others are specified.
    IMAGE_SCN_ALIGN_16BYTES = 0x0050_0000,
    IMAGE_SCN_ALIGN_32BYTES = 0x0060_0000,
    IMAGE_SCN_ALIGN_64BYTES = 0x0070_0000,
    IMAGE_SCN_ALIGN_128BYTES = 0x0080_0000,
    IMAGE_SCN_ALIGN_256BYTES = 0x0090_0000,
    IMAGE_SCN_ALIGN_512BYTES = 0x00A0_0000,
    IMAGE_SCN_ALIGN_1024BYTES = 0x00B0_0000,
    IMAGE_SCN_ALIGN_2048BYTES = 0x00C0_0000,
    IMAGE_SCN_ALIGN_4096BYTES = 0x00D0_0000,
    IMAGE_SCN_ALIGN_8192BYTES = 0x00E0_0000,
    // Unused                                    0x00F0_0000
});

//
// TLS Characteristic Flags
//
/// Tls index is scaled
pub const IMAGE_SCN_SCALE_INDEX: u32 = 0x0000_0001;

//
// Symbol format.
//

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageSymbol {
    /// If first 4 bytes are 0, then second 4 bytes are offset into string table.
    pub name: [u8; 8],
    pub value: U32<LE>,
    pub section_number: U16<LE>,
    pub typ: U16<LE, SymbolType>,
    pub storage_class: SymbolClass,
    pub number_of_aux_symbols: u8,
}

pub const IMAGE_SIZEOF_SYMBOL: usize = 18;

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageSymbolBytes(pub [u8; IMAGE_SIZEOF_SYMBOL]);

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageSymbolEx {
    /// If first 4 bytes are 0, then second 4 bytes are offset into string table.
    pub name: [u8; 8],
    pub value: U32<LE>,
    pub section_number: I32<LE, SymbolSection>,
    pub typ: U16<LE, SymbolType>,
    pub storage_class: SymbolClass,
    pub number_of_aux_symbols: u8,
}

pub const IMAGE_SIZEOF_SYMBOL_EX: usize = 20;

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageSymbolExBytes(pub [u8; IMAGE_SIZEOF_SYMBOL_EX]);

newtype!(
    /// Values for `ImageSymbol::section_number` and `ImageSymbolEx::section_number`.
    struct SymbolSection(i32);
);

impl SymbolSection {
    /// Return true if this is a reserved value.
    pub fn is_reserved(self) -> bool {
        self.0 <= 0
    }

    /// Return the 1-based section index, or `None` if this is a reserved value.
    pub fn index(self) -> Option<u32> {
        if self.0 > 0 {
            Some(self.0 as u32)
        } else {
            None
        }
    }
}

// Symbols have a section number of the section in which they are
// defined. Otherwise, section numbers have the following meanings:
newtype_constant_names!(NAMES_IMAGE_SYM: SymbolSection(i32) = {
    /// Symbol is undefined or is common.
    IMAGE_SYM_UNDEFINED = 0,
    /// Symbol is an absolute value.
    IMAGE_SYM_ABSOLUTE = -1,
    /// Symbol is a special debug item.
    IMAGE_SYM_DEBUG = -2,
});

/// Values 0xFF00-0xFFFF are special
pub const IMAGE_SYM_SECTION_MAX: u16 = 0xFEFF;
pub const IMAGE_SYM_SECTION_MAX_EX: u32 = 0x7fff_ffff;

newtype!(
    /// Values for `ImageSymbol::typ`.
    struct SymbolType(u16);
);

impl SymbolType {
    /// Construct a symbol type from base and derived types.
    ///
    /// You should generally never need to call this. In practice, only
    /// a couple of symbol types are used. Instead, use `SymbolType(0)`
    /// or `IMAGE_SYM_DTYPE_FUNCTION.into()`.
    pub fn new(base_type: SymbolBaseType, derived_type: SymbolDerivedType) -> Self {
        SymbolType(SymbolType::from(base_type).0 | SymbolType::from(derived_type).0)
    }

    /// Return the base type component.
    pub fn base_type(self) -> SymbolBaseType {
        SymbolBaseType(self.0 & N_BTMASK)
    }

    /// Return the derived type component.
    pub fn derived_type(self) -> SymbolDerivedType {
        SymbolDerivedType((self.0 & N_TMASK) >> N_BTSHFT)
    }
}

newtype_flag_names!(NAMES_SYMBOL_TYPE: SymbolType(u16) = {
    _ = N_BTMASK => NAMES_IMAGE_SYM_TYPE,
    _ = N_TMASK => NAMES_IMAGE_SYM_DTYPE,
    IMAGE_SYM_TYPE_PCODE = 0x8000,
});

newtype!(
    /// Values for `ImageSymbol::typ` (basic component).
    struct SymbolBaseType(u16);
);

impl From<SymbolBaseType> for SymbolType {
    fn from(base: SymbolBaseType) -> Self {
        SymbolType(base.0 & N_BTMASK)
    }
}

impl From<SymbolType> for SymbolBaseType {
    fn from(value: SymbolType) -> Self {
        value.base_type()
    }
}

/// no type.
pub const IMAGE_SYM_TYPE_NULL: SymbolBaseType = SymbolBaseType(0);

newtype_constant_names!(NAMES_IMAGE_SYM_TYPE: SymbolBaseType(u16) = {
    IMAGE_SYM_TYPE_VOID = 0x0001,
    /// type character.
    IMAGE_SYM_TYPE_CHAR = 0x0002,
    /// type short integer.
    IMAGE_SYM_TYPE_SHORT = 0x0003,
    IMAGE_SYM_TYPE_INT = 0x0004,
    IMAGE_SYM_TYPE_LONG = 0x0005,
    IMAGE_SYM_TYPE_FLOAT = 0x0006,
    IMAGE_SYM_TYPE_DOUBLE = 0x0007,
    IMAGE_SYM_TYPE_STRUCT = 0x0008,
    IMAGE_SYM_TYPE_UNION = 0x0009,
    /// enumeration.
    IMAGE_SYM_TYPE_ENUM = 0x000A,
    /// member of enumeration.
    IMAGE_SYM_TYPE_MOE = 0x000B,
    IMAGE_SYM_TYPE_BYTE = 0x000C,
    IMAGE_SYM_TYPE_WORD = 0x000D,
    IMAGE_SYM_TYPE_UINT = 0x000E,
    IMAGE_SYM_TYPE_DWORD = 0x000F,
});

newtype!(
    /// Values for `ImageSymbol::typ` (derived component).
    struct SymbolDerivedType(u16);
);

impl From<SymbolDerivedType> for SymbolType {
    fn from(derived: SymbolDerivedType) -> Self {
        SymbolType((derived.0 << N_BTSHFT) & N_TMASK)
    }
}

impl From<SymbolType> for SymbolDerivedType {
    fn from(value: SymbolType) -> Self {
        value.derived_type()
    }
}

/// no derived type.
pub const IMAGE_SYM_DTYPE_NULL: SymbolDerivedType = SymbolDerivedType(0);

newtype_constant_names!(NAMES_IMAGE_SYM_DTYPE: SymbolDerivedType(u16) = {
    /// pointer.
    IMAGE_SYM_DTYPE_POINTER = 1,
    /// function.
    IMAGE_SYM_DTYPE_FUNCTION = 2,
    /// array.
    IMAGE_SYM_DTYPE_ARRAY = 3,
});

newtype!(
    /// Values for `ImageSymbol::storage_class`.
    #[repr(transparent)]
    struct SymbolClass(u8);
);

newtype_constant_names!(NAMES_IMAGE_SYM_CLASS: SymbolClass(u8) = {
    IMAGE_SYM_CLASS_END_OF_FUNCTION = 0xff,
    IMAGE_SYM_CLASS_NULL = 0x00,
    IMAGE_SYM_CLASS_AUTOMATIC = 0x01,
    IMAGE_SYM_CLASS_EXTERNAL = 0x02,
    IMAGE_SYM_CLASS_STATIC = 0x03,
    IMAGE_SYM_CLASS_REGISTER = 0x04,
    IMAGE_SYM_CLASS_EXTERNAL_DEF = 0x05,
    IMAGE_SYM_CLASS_LABEL = 0x06,
    IMAGE_SYM_CLASS_UNDEFINED_LABEL = 0x07,
    IMAGE_SYM_CLASS_MEMBER_OF_STRUCT = 0x08,
    IMAGE_SYM_CLASS_ARGUMENT = 0x09,
    IMAGE_SYM_CLASS_STRUCT_TAG = 0x0A,
    IMAGE_SYM_CLASS_MEMBER_OF_UNION = 0x0B,
    IMAGE_SYM_CLASS_UNION_TAG = 0x0C,
    IMAGE_SYM_CLASS_TYPE_DEFINITION = 0x0D,
    IMAGE_SYM_CLASS_UNDEFINED_STATIC = 0x0E,
    IMAGE_SYM_CLASS_ENUM_TAG = 0x0F,
    IMAGE_SYM_CLASS_MEMBER_OF_ENUM = 0x10,
    IMAGE_SYM_CLASS_REGISTER_PARAM = 0x11,
    IMAGE_SYM_CLASS_BIT_FIELD = 0x12,
    IMAGE_SYM_CLASS_FAR_EXTERNAL = 0x44,
    IMAGE_SYM_CLASS_BLOCK = 0x64,
    IMAGE_SYM_CLASS_FUNCTION = 0x65,
    IMAGE_SYM_CLASS_END_OF_STRUCT = 0x66,
    IMAGE_SYM_CLASS_FILE = 0x67,
    IMAGE_SYM_CLASS_SECTION = 0x68,
    IMAGE_SYM_CLASS_WEAK_EXTERNAL = 0x69,
    IMAGE_SYM_CLASS_CLR_TOKEN = 0x6B,
});

// type packing constants

pub const N_BTMASK: u16 = 0x000F;
pub const N_TMASK: u16 = 0x0030;
pub const N_TMASK1: u16 = 0x00C0;
pub const N_TMASK2: u16 = 0x00F0;
pub const N_BTSHFT: usize = 4;
pub const N_TSHIFT: usize = 2;

pub const IMAGE_SYM_DTYPE_SHIFT: usize = N_BTSHFT;

//
// Auxiliary entry format.
//

// Used for both ImageSymbol and ImageSymbolEx (with padding).
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageAuxSymbolTokenDef {
    /// IMAGE_AUX_SYMBOL_TYPE
    pub aux_type: u8,
    /// Must be 0
    pub reserved1: u8,
    pub symbol_table_index: U32<LE>,
    /// Must be 0
    pub reserved2: [u8; 12],
}

pub const IMAGE_AUX_SYMBOL_TYPE_TOKEN_DEF: u16 = 1;

/// Auxiliary symbol format 1: function definitions.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageAuxSymbolFunction {
    pub tag_index: U32<LE>,
    pub total_size: U32<LE>,
    pub pointer_to_linenumber: U32<LE>,
    pub pointer_to_next_function: U32<LE>,
    pub unused: [u8; 2],
}

/// Auxiliary symbol format 2: .bf and .ef symbols.
// This struct has alignment 1.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageAuxSymbolFunctionBeginEnd {
    pub unused1: [u8; 4],
    /// declaration line number
    pub linenumber: U16<LE>,
    pub unused2: [u8; 6],
    pub pointer_to_next_function: U32<LE>,
    pub unused3: [u8; 2],
}

/// Auxiliary symbol format 3: weak externals.
///
/// Used for both `ImageSymbol` and `ImageSymbolEx` (both with padding).
// This struct has alignment 1.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageAuxSymbolWeak {
    /// the weak extern default symbol index
    pub weak_default_sym_index: U32<LE>,
    pub weak_search_type: U32<LE, WeakExternSearch>,
}

/// Auxiliary symbol format 5: sections.
///
/// Used for both `ImageSymbol` and `ImageSymbolEx` (with padding).
// This struct has alignment 1.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageAuxSymbolSection {
    /// section length
    pub length: U32<LE>,
    /// number of relocation entries
    pub number_of_relocations: U16<LE>,
    /// number of line numbers
    pub number_of_linenumbers: U16<LE>,
    /// checksum for communal
    pub check_sum: U32<LE>,
    /// section number to associate with
    pub number: U16<LE>,
    /// communal selection type
    pub selection: ComdatSelection,
    pub reserved: u8,
    /// high bits of the section number
    pub high_number: U16<LE>,
}

// Used for both ImageSymbol and ImageSymbolEx (both with padding).
// This struct has alignment 1.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageAuxSymbolCrc {
    pub crc: U32<LE>,
}

//
// Communal selection types.
//

newtype!(
    /// Values for `ImageAuxSymbolSection::selection`.
    #[repr(transparent)]
    struct ComdatSelection(u8);
);

newtype_constant_names!(NAMES_IMAGE_COMDAT_SELECT: ComdatSelection(u8) = {
    IMAGE_COMDAT_SELECT_NODUPLICATES = 1,
    IMAGE_COMDAT_SELECT_ANY = 2,
    IMAGE_COMDAT_SELECT_SAME_SIZE = 3,
    IMAGE_COMDAT_SELECT_EXACT_MATCH = 4,
    IMAGE_COMDAT_SELECT_ASSOCIATIVE = 5,
    IMAGE_COMDAT_SELECT_LARGEST = 6,
    IMAGE_COMDAT_SELECT_NEWEST = 7,
});

newtype!(
    /// Values for `ImageAuxSymbolWeak::weak_search_type`.
    struct WeakExternSearch(u32);
);

newtype_constant_names!(NAMES_IMAGE_WEAK_EXTERN: WeakExternSearch(u32) = {
    IMAGE_WEAK_EXTERN_SEARCH_NOLIBRARY = 1,
    IMAGE_WEAK_EXTERN_SEARCH_LIBRARY = 2,
    IMAGE_WEAK_EXTERN_SEARCH_ALIAS = 3,
    IMAGE_WEAK_EXTERN_ANTI_DEPENDENCY = 4,
});

//
// Relocation format.
//

// This struct has alignment 1.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageRelocation {
    /// Also `RelocCount` when IMAGE_SCN_LNK_NRELOC_OVFL is set
    pub virtual_address: U32<LE>,
    pub symbol_table_index: U32<LE>,
    pub typ: U16<LE, RelocationType>,
}

newtype!(
    /// Values for `ImageRelocation::typ`.
    struct RelocationType(u16);
);

newtype_flag_names!(RelocationType(u16) = {});

impl RelocationType {
    /// Return the type subfield for a PPC relocation.
    pub fn ppc_type(self) -> Self {
        RelocationType(self.0 & IMAGE_REL_PPC_TYPEMASK)
    }

    /// Return the type subfield for a SH relocation.
    pub fn sh_type(self) -> Self {
        self.without(IMAGE_REL_SH_NOMODE)
    }
}

//
// I386 relocation types.
//
names! {
    struct I386(Base);
    flags rel: RelocationType(u16) = {
        _ = !0 => NAMES_IMAGE_REL_I386,
    };
}

constant_names!(
/// Values for `ImageRelocation::typ` on I386.
pub NAMES_IMAGE_REL_I386: RelocationType(u16) = {
    /// Reference is absolute, no relocation is necessary
    IMAGE_REL_I386_ABSOLUTE = 0x0000,
    /// Direct 16-bit reference to the symbols virtual address
    IMAGE_REL_I386_DIR16 = 0x0001,
    /// PC-relative 16-bit reference to the symbols virtual address
    IMAGE_REL_I386_REL16 = 0x0002,
    /// Direct 32-bit reference to the symbols virtual address
    IMAGE_REL_I386_DIR32 = 0x0006,
    /// Direct 32-bit reference to the symbols virtual address, base not included
    IMAGE_REL_I386_DIR32NB = 0x0007,
    /// Direct 16-bit reference to the segment-selector bits of a 32-bit virtual address
    IMAGE_REL_I386_SEG12 = 0x0009,
    IMAGE_REL_I386_SECTION = 0x000A,
    IMAGE_REL_I386_SECREL = 0x000B,
    /// clr token
    IMAGE_REL_I386_TOKEN = 0x000C,
    /// 7 bit offset from base of section containing target
    IMAGE_REL_I386_SECREL7 = 0x000D,
    /// PC-relative 32-bit reference to the symbols virtual address
    IMAGE_REL_I386_REL32 = 0x0014,
});

//
// MIPS relocation types.
//
names! {
    struct Mips(Base);
    flags rel: RelocationType(u16) = {
        _ = !0 => NAMES_IMAGE_REL_MIPS,
    };
    consts rel_based = NAMES_REL_BASED_MIPS;
}

constant_names!(
/// Values for `ImageRelocation::typ` on MIPS.
pub NAMES_IMAGE_REL_MIPS: RelocationType(u16) = {
    /// Reference is absolute, no relocation is necessary
    IMAGE_REL_MIPS_ABSOLUTE = 0x0000,
    IMAGE_REL_MIPS_REFHALF = 0x0001,
    IMAGE_REL_MIPS_REFWORD = 0x0002,
    IMAGE_REL_MIPS_JMPADDR = 0x0003,
    IMAGE_REL_MIPS_REFHI = 0x0004,
    IMAGE_REL_MIPS_REFLO = 0x0005,
    IMAGE_REL_MIPS_GPREL = 0x0006,
    IMAGE_REL_MIPS_LITERAL = 0x0007,
    IMAGE_REL_MIPS_SECTION = 0x000A,
    IMAGE_REL_MIPS_SECREL = 0x000B,
    /// Low 16-bit section relative reference (used for >32k TLS)
    IMAGE_REL_MIPS_SECRELLO = 0x000C,
    /// High 16-bit section relative reference (used for >32k TLS)
    IMAGE_REL_MIPS_SECRELHI = 0x000D,
    /// clr token
    IMAGE_REL_MIPS_TOKEN = 0x000E,
    IMAGE_REL_MIPS_JMPADDR16 = 0x0010,
    IMAGE_REL_MIPS_REFWORDNB = 0x0022,
    IMAGE_REL_MIPS_PAIR = 0x0025,
});

//
// Alpha Relocation types.
//
names! {
    struct Alpha(Base);
    flags rel: RelocationType(u16) = {
        _ = !0 => NAMES_IMAGE_REL_ALPHA,
    };
}

constant_names!(
/// Values for `ImageRelocation::typ` on Alpha.
pub NAMES_IMAGE_REL_ALPHA: RelocationType(u16) = {
    IMAGE_REL_ALPHA_ABSOLUTE = 0x0000,
    IMAGE_REL_ALPHA_REFLONG = 0x0001,
    IMAGE_REL_ALPHA_REFQUAD = 0x0002,
    IMAGE_REL_ALPHA_GPREL32 = 0x0003,
    IMAGE_REL_ALPHA_LITERAL = 0x0004,
    IMAGE_REL_ALPHA_LITUSE = 0x0005,
    IMAGE_REL_ALPHA_GPDISP = 0x0006,
    IMAGE_REL_ALPHA_BRADDR = 0x0007,
    IMAGE_REL_ALPHA_HINT = 0x0008,
    IMAGE_REL_ALPHA_INLINE_REFLONG = 0x0009,
    IMAGE_REL_ALPHA_REFHI = 0x000A,
    IMAGE_REL_ALPHA_REFLO = 0x000B,
    IMAGE_REL_ALPHA_PAIR = 0x000C,
    IMAGE_REL_ALPHA_MATCH = 0x000D,
    IMAGE_REL_ALPHA_SECTION = 0x000E,
    IMAGE_REL_ALPHA_SECREL = 0x000F,
    IMAGE_REL_ALPHA_REFLONGNB = 0x0010,
    /// Low 16-bit section relative reference
    IMAGE_REL_ALPHA_SECRELLO = 0x0011,
    /// High 16-bit section relative reference
    IMAGE_REL_ALPHA_SECRELHI = 0x0012,
    /// High 16 bits of 48 bit reference
    IMAGE_REL_ALPHA_REFQ3 = 0x0013,
    /// Middle 16 bits of 48 bit reference
    IMAGE_REL_ALPHA_REFQ2 = 0x0014,
    /// Low 16 bits of 48 bit reference
    IMAGE_REL_ALPHA_REFQ1 = 0x0015,
    /// Low 16-bit GP relative reference
    IMAGE_REL_ALPHA_GPRELLO = 0x0016,
    /// High 16-bit GP relative reference
    IMAGE_REL_ALPHA_GPRELHI = 0x0017,
});

//
// IBM PowerPC relocation types.
//
names! {
    struct Ppc(Base);
    flags rel: RelocationType(u16) = {
        IMAGE_REL_PPC_TYPEMASK = 0x00FF => NAMES_IMAGE_REL_PPC,
        /// subtract reloc value rather than adding it
        IMAGE_REL_PPC_NEG = 0x0100,
        /// fix branch prediction bit to predict branch taken
        IMAGE_REL_PPC_BRTAKEN = 0x0200,
        /// fix branch prediction bit to predict branch not taken
        IMAGE_REL_PPC_BRNTAKEN = 0x0400,
        /// toc slot defined in file (or, data in toc)
        IMAGE_REL_PPC_TOCDEFN = 0x0800,
    };
}

constant_names!(
/// Values for `ImageRelocation::typ` on PowerPC.
pub NAMES_IMAGE_REL_PPC: RelocationType(u16) = {
    /// NOP
    IMAGE_REL_PPC_ABSOLUTE = 0x0000,
    /// 64-bit address
    IMAGE_REL_PPC_ADDR64 = 0x0001,
    /// 32-bit address
    IMAGE_REL_PPC_ADDR32 = 0x0002,
    /// 26-bit address, shifted left 2 (branch absolute)
    IMAGE_REL_PPC_ADDR24 = 0x0003,
    /// 16-bit address
    IMAGE_REL_PPC_ADDR16 = 0x0004,
    /// 16-bit address, shifted left 2 (load doubleword)
    IMAGE_REL_PPC_ADDR14 = 0x0005,
    /// 26-bit PC-relative offset, shifted left 2 (branch relative)
    IMAGE_REL_PPC_REL24 = 0x0006,
    /// 16-bit PC-relative offset, shifted left 2 (br cond relative)
    IMAGE_REL_PPC_REL14 = 0x0007,
    /// 16-bit offset from TOC base
    IMAGE_REL_PPC_TOCREL16 = 0x0008,
    /// 16-bit offset from TOC base, shifted left 2 (load doubleword)
    IMAGE_REL_PPC_TOCREL14 = 0x0009,

    /// 32-bit addr w/o image base
    IMAGE_REL_PPC_ADDR32NB = 0x000A,
    /// va of containing section (as in an image sectionhdr)
    IMAGE_REL_PPC_SECREL = 0x000B,
    /// sectionheader number
    IMAGE_REL_PPC_SECTION = 0x000C,
    /// substitute TOC restore instruction iff symbol is glue code
    IMAGE_REL_PPC_IFGLUE = 0x000D,
    /// symbol is glue code; virtual address is TOC restore instruction
    IMAGE_REL_PPC_IMGLUE = 0x000E,
    /// va of containing section (limited to 16 bits)
    IMAGE_REL_PPC_SECREL16 = 0x000F,
    IMAGE_REL_PPC_REFHI = 0x0010,
    IMAGE_REL_PPC_REFLO = 0x0011,
    IMAGE_REL_PPC_PAIR = 0x0012,
    /// Low 16-bit section relative reference (used for >32k TLS)
    IMAGE_REL_PPC_SECRELLO = 0x0013,
    /// High 16-bit section relative reference (used for >32k TLS)
    IMAGE_REL_PPC_SECRELHI = 0x0014,
    IMAGE_REL_PPC_GPREL = 0x0015,
    /// clr token
    IMAGE_REL_PPC_TOKEN = 0x0016,
});

//
// Hitachi SH3 relocation types.
//
names! {
    struct Sh(Base);
    flags rel: RelocationType(u16) = {
        _ = !IMAGE_REL_SH_NOMODE.0 => NAMES_IMAGE_REL_SH,
        /// relocation ignores section mode
        IMAGE_REL_SH_NOMODE = 0x8000,
    };
}

constant_names!(
/// Values for `ImageRelocation::typ` on SH3.
pub NAMES_IMAGE_REL_SH: RelocationType(u16) = {
    /// No relocation
    IMAGE_REL_SH3_ABSOLUTE = 0x0000,
    /// 16 bit direct
    IMAGE_REL_SH3_DIRECT16 = 0x0001,
    /// 32 bit direct
    IMAGE_REL_SH3_DIRECT32 = 0x0002,
    /// 8 bit direct, -128..255
    IMAGE_REL_SH3_DIRECT8 = 0x0003,
    /// 8 bit direct .W (0 ext.)
    IMAGE_REL_SH3_DIRECT8_WORD = 0x0004,
    /// 8 bit direct .L (0 ext.)
    IMAGE_REL_SH3_DIRECT8_LONG = 0x0005,
    /// 4 bit direct (0 ext.)
    IMAGE_REL_SH3_DIRECT4 = 0x0006,
    /// 4 bit direct .W (0 ext.)
    IMAGE_REL_SH3_DIRECT4_WORD = 0x0007,
    /// 4 bit direct .L (0 ext.)
    IMAGE_REL_SH3_DIRECT4_LONG = 0x0008,
    /// 8 bit PC relative .W
    IMAGE_REL_SH3_PCREL8_WORD = 0x0009,
    /// 8 bit PC relative .L
    IMAGE_REL_SH3_PCREL8_LONG = 0x000A,
    /// 12 LSB PC relative .W
    IMAGE_REL_SH3_PCREL12_WORD = 0x000B,
    /// Start of EXE section
    IMAGE_REL_SH3_STARTOF_SECTION = 0x000C,
    /// Size of EXE section
    IMAGE_REL_SH3_SIZEOF_SECTION = 0x000D,
    /// Section table index
    IMAGE_REL_SH3_SECTION = 0x000E,
    /// Offset within section
    IMAGE_REL_SH3_SECREL = 0x000F,
    /// 32 bit direct not based
    IMAGE_REL_SH3_DIRECT32_NB = 0x0010,
    /// GP-relative addressing
    IMAGE_REL_SH3_GPREL4_LONG = 0x0011,
    /// clr token
    IMAGE_REL_SH3_TOKEN = 0x0012,
    /// Offset from current instruction in longwords
    /// if not NOMODE, insert the inverse of the low bit at bit 32 to select PTA/PTB
    IMAGE_REL_SHM_PCRELPT = 0x0013,
    /// Low bits of 32-bit address
    IMAGE_REL_SHM_REFLO = 0x0014,
    /// High bits of 32-bit address
    IMAGE_REL_SHM_REFHALF = 0x0015,
    /// Low bits of relative reference
    IMAGE_REL_SHM_RELLO = 0x0016,
    /// High bits of relative reference
    IMAGE_REL_SHM_RELHALF = 0x0017,
    /// offset operand for relocation
    IMAGE_REL_SHM_PAIR = 0x0018,
});

names! {
    struct Arm(Base);
    flags rel: RelocationType(u16) = {
        _ = !0 => NAMES_IMAGE_REL_ARM,
    };
    consts rel_based = NAMES_REL_BASED_ARM;
}

constant_names!(
/// Values for `ImageRelocation::typ` on ARM.
pub NAMES_IMAGE_REL_ARM: RelocationType(u16) = {
    /// No relocation required
    IMAGE_REL_ARM_ABSOLUTE = 0x0000,
    /// 32 bit address
    IMAGE_REL_ARM_ADDR32 = 0x0001,
    /// 32 bit address w/o image base
    IMAGE_REL_ARM_ADDR32NB = 0x0002,
    /// 24 bit offset << 2 & sign ext.
    IMAGE_REL_ARM_BRANCH24 = 0x0003,
    /// Thumb: 2 11 bit offsets
    IMAGE_REL_ARM_BRANCH11 = 0x0004,
    /// clr token
    IMAGE_REL_ARM_TOKEN = 0x0005,
    /// GP-relative addressing (ARM)
    IMAGE_REL_ARM_GPREL12 = 0x0006,
    /// GP-relative addressing (Thumb)
    IMAGE_REL_ARM_GPREL7 = 0x0007,
    IMAGE_REL_ARM_BLX24 = 0x0008,
    IMAGE_REL_ARM_BLX11 = 0x0009,
    /// 32-bit relative address from byte following reloc
    IMAGE_REL_ARM_REL32 = 0x000A,
    /// Section table index
    IMAGE_REL_ARM_SECTION = 0x000E,
    /// Offset within section
    IMAGE_REL_ARM_SECREL = 0x000F,
    /// ARM: MOVW/MOVT
    IMAGE_REL_ARM_MOV32A = 0x0010,
    /// ARM: MOVW/MOVT (deprecated)
    IMAGE_REL_ARM_MOV32 = 0x0010,
    /// Thumb: MOVW/MOVT
    IMAGE_REL_ARM_MOV32T = 0x0011,
    /// Thumb: MOVW/MOVT (deprecated)
    IMAGE_REL_THUMB_MOV32 = 0x0011,
    /// Thumb: 32-bit conditional B
    IMAGE_REL_ARM_BRANCH20T = 0x0012,
    /// Thumb: 32-bit conditional B (deprecated)
    IMAGE_REL_THUMB_BRANCH20 = 0x0012,
    /// Thumb: 32-bit B or BL
    IMAGE_REL_ARM_BRANCH24T = 0x0014,
    /// Thumb: 32-bit B or BL (deprecated)
    IMAGE_REL_THUMB_BRANCH24 = 0x0014,
    /// Thumb: BLX immediate
    IMAGE_REL_ARM_BLX23T = 0x0015,
    /// Thumb: BLX immediate (deprecated)
    IMAGE_REL_THUMB_BLX23 = 0x0015,
});

names! {
    struct Am(Base);
    flags rel: RelocationType(u16) = {
        _ = !0 => NAMES_IMAGE_REL_AM,
    };
}

constant_names!(
/// Values for `ImageRelocation::typ` on AM33.
pub NAMES_IMAGE_REL_AM: RelocationType(u16) = {
    IMAGE_REL_AM_ABSOLUTE = 0x0000,
    IMAGE_REL_AM_ADDR32 = 0x0001,
    IMAGE_REL_AM_ADDR32NB = 0x0002,
    IMAGE_REL_AM_CALL32 = 0x0003,
    IMAGE_REL_AM_FUNCINFO = 0x0004,
    IMAGE_REL_AM_REL32_1 = 0x0005,
    IMAGE_REL_AM_REL32_2 = 0x0006,
    IMAGE_REL_AM_SECREL = 0x0007,
    IMAGE_REL_AM_SECTION = 0x0008,
    IMAGE_REL_AM_TOKEN = 0x0009,
});

//
// ARM64 relocations types.
//

names! {
    struct Arm64(Base);
    flags rel: RelocationType(u16) = {
        _ = !0 => NAMES_IMAGE_REL_ARM64,
    };
}

constant_names!(
/// Values for `ImageRelocation::typ` on ARM64.
pub NAMES_IMAGE_REL_ARM64: RelocationType(u16) = {
    /// No relocation required
    IMAGE_REL_ARM64_ABSOLUTE = 0x0000,
    /// 32 bit address. Review! do we need it?
    IMAGE_REL_ARM64_ADDR32 = 0x0001,
    /// 32 bit address w/o image base (RVA: for Data/PData/XData)
    IMAGE_REL_ARM64_ADDR32NB = 0x0002,
    /// 26 bit offset << 2 & sign ext. for B & BL
    IMAGE_REL_ARM64_BRANCH26 = 0x0003,
    /// ADRP
    IMAGE_REL_ARM64_PAGEBASE_REL21 = 0x0004,
    /// ADR
    IMAGE_REL_ARM64_REL21 = 0x0005,
    /// ADD/ADDS (immediate) with zero shift, for page offset
    IMAGE_REL_ARM64_PAGEOFFSET_12A = 0x0006,
    /// LDR (indexed, unsigned immediate), for page offset
    IMAGE_REL_ARM64_PAGEOFFSET_12L = 0x0007,
    /// Offset within section
    IMAGE_REL_ARM64_SECREL = 0x0008,
    /// ADD/ADDS (immediate) with zero shift, for bit 0:11 of section offset
    IMAGE_REL_ARM64_SECREL_LOW12A = 0x0009,
    /// ADD/ADDS (immediate) with zero shift, for bit 12:23 of section offset
    IMAGE_REL_ARM64_SECREL_HIGH12A = 0x000A,
    /// LDR (indexed, unsigned immediate), for bit 0:11 of section offset
    IMAGE_REL_ARM64_SECREL_LOW12L = 0x000B,
    IMAGE_REL_ARM64_TOKEN = 0x000C,
    /// Section table index
    IMAGE_REL_ARM64_SECTION = 0x000D,
    /// 64 bit address
    IMAGE_REL_ARM64_ADDR64 = 0x000E,
    /// 19 bit offset << 2 & sign ext. for conditional B
    IMAGE_REL_ARM64_BRANCH19 = 0x000F,
    /// TBZ/TBNZ
    IMAGE_REL_ARM64_BRANCH14 = 0x0010,
    /// 32-bit relative address from byte following reloc
    IMAGE_REL_ARM64_REL32 = 0x0011,
});

//
// x64 relocations
//
names! {
    struct Amd64(Base);
    flags rel: RelocationType(u16) = {
        _ = !0 => NAMES_IMAGE_REL_AMD64,
    };
}

constant_names!(
/// Values for `ImageRelocation::typ` on x64.
pub NAMES_IMAGE_REL_AMD64: RelocationType(u16) = {
    /// Reference is absolute, no relocation is necessary
    IMAGE_REL_AMD64_ABSOLUTE = 0x0000,
    /// 64-bit address (VA).
    IMAGE_REL_AMD64_ADDR64 = 0x0001,
    /// 32-bit address (VA).
    IMAGE_REL_AMD64_ADDR32 = 0x0002,
    /// 32-bit address w/o image base (RVA).
    IMAGE_REL_AMD64_ADDR32NB = 0x0003,
    /// 32-bit relative address from byte following reloc
    IMAGE_REL_AMD64_REL32 = 0x0004,
    /// 32-bit relative address from byte distance 1 from reloc
    IMAGE_REL_AMD64_REL32_1 = 0x0005,
    /// 32-bit relative address from byte distance 2 from reloc
    IMAGE_REL_AMD64_REL32_2 = 0x0006,
    /// 32-bit relative address from byte distance 3 from reloc
    IMAGE_REL_AMD64_REL32_3 = 0x0007,
    /// 32-bit relative address from byte distance 4 from reloc
    IMAGE_REL_AMD64_REL32_4 = 0x0008,
    /// 32-bit relative address from byte distance 5 from reloc
    IMAGE_REL_AMD64_REL32_5 = 0x0009,
    /// Section index
    IMAGE_REL_AMD64_SECTION = 0x000A,
    /// 32 bit offset from base of section containing target
    IMAGE_REL_AMD64_SECREL = 0x000B,
    /// 7 bit unsigned offset from base of section containing target
    IMAGE_REL_AMD64_SECREL7 = 0x000C,
    /// 32 bit metadata token
    IMAGE_REL_AMD64_TOKEN = 0x000D,
    /// 32 bit signed span-dependent value emitted into object
    IMAGE_REL_AMD64_SREL32 = 0x000E,
    IMAGE_REL_AMD64_PAIR = 0x000F,
    /// 32 bit signed span-dependent value applied at link time
    IMAGE_REL_AMD64_SSPAN32 = 0x0010,
    IMAGE_REL_AMD64_EHANDLER = 0x0011,
    /// Indirect branch to an import
    IMAGE_REL_AMD64_IMPORT_BR = 0x0012,
    /// Indirect call to an import
    IMAGE_REL_AMD64_IMPORT_CALL = 0x0013,
    /// Indirect branch to a CFG check
    IMAGE_REL_AMD64_CFG_BR = 0x0014,
    /// Indirect branch to a CFG check, with REX.W prefix
    IMAGE_REL_AMD64_CFG_BR_REX = 0x0015,
    /// Indirect call to a CFG check
    IMAGE_REL_AMD64_CFG_CALL = 0x0016,
    /// Indirect branch to a target in RAX (no CFG)
    IMAGE_REL_AMD64_INDIR_BR = 0x0017,
    /// Indirect branch to a target in RAX, with REX.W prefix (no CFG)
    IMAGE_REL_AMD64_INDIR_BR_REX = 0x0018,
    /// Indirect call to a target in RAX (no CFG)
    IMAGE_REL_AMD64_INDIR_CALL = 0x0019,
    /// Indirect branch for a switch table using Reg 0 (RAX)
    IMAGE_REL_AMD64_INDIR_BR_SWITCHTABLE_FIRST = 0x0020,
    /// Indirect branch for a switch table using Reg 15 (R15)
    IMAGE_REL_AMD64_INDIR_BR_SWITCHTABLE_LAST = 0x002F,
});

//
// IA64 relocation types.
//
names! {
    struct Ia64(Base);
    flags rel: RelocationType(u16) = {
        _ = !0 => NAMES_IMAGE_REL_IA64,
    };
    consts rel_based = NAMES_REL_BASED_IA64;
}

constant_names!(
/// Values for `ImageRelocation::typ` on IA64.
pub NAMES_IMAGE_REL_IA64: RelocationType(u16) = {
    IMAGE_REL_IA64_ABSOLUTE = 0x0000,
    IMAGE_REL_IA64_IMM14 = 0x0001,
    IMAGE_REL_IA64_IMM22 = 0x0002,
    IMAGE_REL_IA64_IMM64 = 0x0003,
    IMAGE_REL_IA64_DIR32 = 0x0004,
    IMAGE_REL_IA64_DIR64 = 0x0005,
    IMAGE_REL_IA64_PCREL21B = 0x0006,
    IMAGE_REL_IA64_PCREL21M = 0x0007,
    IMAGE_REL_IA64_PCREL21F = 0x0008,
    IMAGE_REL_IA64_GPREL22 = 0x0009,
    IMAGE_REL_IA64_LTOFF22 = 0x000A,
    IMAGE_REL_IA64_SECTION = 0x000B,
    IMAGE_REL_IA64_SECREL22 = 0x000C,
    IMAGE_REL_IA64_SECREL64I = 0x000D,
    IMAGE_REL_IA64_SECREL32 = 0x000E,
    //
    IMAGE_REL_IA64_DIR32NB = 0x0010,
    IMAGE_REL_IA64_SREL14 = 0x0011,
    IMAGE_REL_IA64_SREL22 = 0x0012,
    IMAGE_REL_IA64_SREL32 = 0x0013,
    IMAGE_REL_IA64_UREL32 = 0x0014,
    /// This is always a BRL and never converted
    IMAGE_REL_IA64_PCREL60X = 0x0015,
    /// If possible, convert to MBB bundle with NOP.B in slot 1
    IMAGE_REL_IA64_PCREL60B = 0x0016,
    /// If possible, convert to MFB bundle with NOP.F in slot 1
    IMAGE_REL_IA64_PCREL60F = 0x0017,
    /// If possible, convert to MIB bundle with NOP.I in slot 1
    IMAGE_REL_IA64_PCREL60I = 0x0018,
    /// If possible, convert to MMB bundle with NOP.M in slot 1
    IMAGE_REL_IA64_PCREL60M = 0x0019,
    IMAGE_REL_IA64_IMMGPREL64 = 0x001A,
    /// clr token
    IMAGE_REL_IA64_TOKEN = 0x001B,
    IMAGE_REL_IA64_GPREL32 = 0x001C,
    IMAGE_REL_IA64_ADDEND = 0x001F,
});

//
// CEF relocation types.
//
names! {
    struct Cef(Base);
    flags rel: RelocationType(u16) = {
        _ = !0 => NAMES_IMAGE_REL_CEF,
    };
}

constant_names!(
/// Values for `ImageRelocation::typ` on CEF.
pub NAMES_IMAGE_REL_CEF: RelocationType(u16) = {
    /// Reference is absolute, no relocation is necessary
    IMAGE_REL_CEF_ABSOLUTE = 0x0000,
    /// 32-bit address (VA).
    IMAGE_REL_CEF_ADDR32 = 0x0001,
    /// 64-bit address (VA).
    IMAGE_REL_CEF_ADDR64 = 0x0002,
    /// 32-bit address w/o image base (RVA).
    IMAGE_REL_CEF_ADDR32NB = 0x0003,
    /// Section index
    IMAGE_REL_CEF_SECTION = 0x0004,
    /// 32 bit offset from base of section containing target
    IMAGE_REL_CEF_SECREL = 0x0005,
    /// 32 bit metadata token
    IMAGE_REL_CEF_TOKEN = 0x0006,
});

//
// clr relocation types.
//
names! {
    struct Cee(Base);
    flags rel: RelocationType(u16) = {
        _ = !0 => NAMES_IMAGE_REL_CEE,
    };
}

constant_names!(
/// Values for `ImageRelocation::typ` on CLR.
pub NAMES_IMAGE_REL_CEE: RelocationType(u16) = {
    /// Reference is absolute, no relocation is necessary
    IMAGE_REL_CEE_ABSOLUTE = 0x0000,
    /// 32-bit address (VA).
    IMAGE_REL_CEE_ADDR32 = 0x0001,
    /// 64-bit address (VA).
    IMAGE_REL_CEE_ADDR64 = 0x0002,
    /// 32-bit address w/o image base (RVA).
    IMAGE_REL_CEE_ADDR32NB = 0x0003,
    /// Section index
    IMAGE_REL_CEE_SECTION = 0x0004,
    /// 32 bit offset from base of section containing target
    IMAGE_REL_CEE_SECREL = 0x0005,
    /// 32 bit metadata token
    IMAGE_REL_CEE_TOKEN = 0x0006,
});

names! {
    struct M32r(Base);
    flags rel: RelocationType(u16) = {
        _ = !0 => NAMES_IMAGE_REL_M32R,
    };
}

constant_names!(
/// Values for `ImageRelocation::typ` on M32R.
pub NAMES_IMAGE_REL_M32R: RelocationType(u16) = {
    /// No relocation required
    IMAGE_REL_M32R_ABSOLUTE = 0x0000,
    /// 32 bit address
    IMAGE_REL_M32R_ADDR32 = 0x0001,
    /// 32 bit address w/o image base
    IMAGE_REL_M32R_ADDR32NB = 0x0002,
    /// 24 bit address
    IMAGE_REL_M32R_ADDR24 = 0x0003,
    /// GP relative addressing
    IMAGE_REL_M32R_GPREL16 = 0x0004,
    /// 24 bit offset << 2 & sign ext.
    IMAGE_REL_M32R_PCREL24 = 0x0005,
    /// 16 bit offset << 2 & sign ext.
    IMAGE_REL_M32R_PCREL16 = 0x0006,
    /// 8 bit offset << 2 & sign ext.
    IMAGE_REL_M32R_PCREL8 = 0x0007,
    /// 16 MSBs
    IMAGE_REL_M32R_REFHALF = 0x0008,
    /// 16 MSBs; adj for LSB sign ext.
    IMAGE_REL_M32R_REFHI = 0x0009,
    /// 16 LSBs
    IMAGE_REL_M32R_REFLO = 0x000A,
    /// Link HI and LO
    IMAGE_REL_M32R_PAIR = 0x000B,
    /// Section table index
    IMAGE_REL_M32R_SECTION = 0x000C,
    /// 32 bit section relative reference
    IMAGE_REL_M32R_SECREL32 = 0x000D,
    /// clr token
    IMAGE_REL_M32R_TOKEN = 0x000E,
});

names! {
    struct Ebc(Base);
    flags rel: RelocationType(u16) = {
        _ = !0 => NAMES_IMAGE_REL_EBC,
    };
}

constant_names!(
/// Values for `ImageRelocation::typ` on EBC.
pub NAMES_IMAGE_REL_EBC: RelocationType(u16) = {
    /// No relocation required
    IMAGE_REL_EBC_ABSOLUTE = 0x0000,
    /// 32 bit address w/o image base
    IMAGE_REL_EBC_ADDR32NB = 0x0001,
    /// 32-bit relative address from byte following reloc
    IMAGE_REL_EBC_REL32 = 0x0002,
    /// Section table index
    IMAGE_REL_EBC_SECTION = 0x0003,
    /// Offset within section
    IMAGE_REL_EBC_SECREL = 0x0004,
});

names! {
    struct Riscv(Base);
    consts rel_based = NAMES_REL_BASED_RISCV;
}

/*
// TODO?
#define EXT_IMM64(Value, Address, Size, InstPos, ValPos)  /* Intel-IA64-Filler */           \
    Value |= (((ULONGLONG)((*(Address) >> InstPos) & (((ULONGLONG)1 << Size) - 1))) << ValPos)  // Intel-IA64-Filler

#define INS_IMM64(Value, Address, Size, InstPos, ValPos)  /* Intel-IA64-Filler */\
    *(PDWORD)Address = (*(PDWORD)Address & ~(((1 << Size) - 1) << InstPos)) | /* Intel-IA64-Filler */\
          ((DWORD)((((ULONGLONG)Value >> ValPos) & (((ULONGLONG)1 << Size) - 1))) << InstPos)  // Intel-IA64-Filler
*/

/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM7B_INST_WORD_X: u16 = 3;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM7B_SIZE_X: u16 = 7;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM7B_INST_WORD_POS_X: u16 = 4;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM7B_VAL_POS_X: u16 = 0;

/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM9D_INST_WORD_X: u16 = 3;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM9D_SIZE_X: u16 = 9;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM9D_INST_WORD_POS_X: u16 = 18;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM9D_VAL_POS_X: u16 = 7;

/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM5C_INST_WORD_X: u16 = 3;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM5C_SIZE_X: u16 = 5;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM5C_INST_WORD_POS_X: u16 = 13;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM5C_VAL_POS_X: u16 = 16;

/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IC_INST_WORD_X: u16 = 3;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IC_SIZE_X: u16 = 1;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IC_INST_WORD_POS_X: u16 = 12;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IC_VAL_POS_X: u16 = 21;

/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM41A_INST_WORD_X: u16 = 1;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM41A_SIZE_X: u16 = 10;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM41A_INST_WORD_POS_X: u16 = 14;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM41A_VAL_POS_X: u16 = 22;

/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM41B_INST_WORD_X: u16 = 1;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM41B_SIZE_X: u16 = 8;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM41B_INST_WORD_POS_X: u16 = 24;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM41B_VAL_POS_X: u16 = 32;

/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM41C_INST_WORD_X: u16 = 2;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM41C_SIZE_X: u16 = 23;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM41C_INST_WORD_POS_X: u16 = 0;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_IMM41C_VAL_POS_X: u16 = 40;

/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_SIGN_INST_WORD_X: u16 = 3;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_SIGN_SIZE_X: u16 = 1;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_SIGN_INST_WORD_POS_X: u16 = 27;
/// Intel-IA64-Filler
pub const EMARCH_ENC_I17_SIGN_VAL_POS_X: u16 = 63;

/// Intel-IA64-Filler
pub const X3_OPCODE_INST_WORD_X: u16 = 3;
/// Intel-IA64-Filler
pub const X3_OPCODE_SIZE_X: u16 = 4;
/// Intel-IA64-Filler
pub const X3_OPCODE_INST_WORD_POS_X: u16 = 28;
/// Intel-IA64-Filler
pub const X3_OPCODE_SIGN_VAL_POS_X: u16 = 0;

/// Intel-IA64-Filler
pub const X3_I_INST_WORD_X: u16 = 3;
/// Intel-IA64-Filler
pub const X3_I_SIZE_X: u16 = 1;
/// Intel-IA64-Filler
pub const X3_I_INST_WORD_POS_X: u16 = 27;
/// Intel-IA64-Filler
pub const X3_I_SIGN_VAL_POS_X: u16 = 59;

/// Intel-IA64-Filler
pub const X3_D_WH_INST_WORD_X: u16 = 3;
/// Intel-IA64-Filler
pub const X3_D_WH_SIZE_X: u16 = 3;
/// Intel-IA64-Filler
pub const X3_D_WH_INST_WORD_POS_X: u16 = 24;
/// Intel-IA64-Filler
pub const X3_D_WH_SIGN_VAL_POS_X: u16 = 0;

/// Intel-IA64-Filler
pub const X3_IMM20_INST_WORD_X: u16 = 3;
/// Intel-IA64-Filler
pub const X3_IMM20_SIZE_X: u16 = 20;
/// Intel-IA64-Filler
pub const X3_IMM20_INST_WORD_POS_X: u16 = 4;
/// Intel-IA64-Filler
pub const X3_IMM20_SIGN_VAL_POS_X: u16 = 0;

/// Intel-IA64-Filler
pub const X3_IMM39_1_INST_WORD_X: u16 = 2;
/// Intel-IA64-Filler
pub const X3_IMM39_1_SIZE_X: u16 = 23;
/// Intel-IA64-Filler
pub const X3_IMM39_1_INST_WORD_POS_X: u16 = 0;
/// Intel-IA64-Filler
pub const X3_IMM39_1_SIGN_VAL_POS_X: u16 = 36;

/// Intel-IA64-Filler
pub const X3_IMM39_2_INST_WORD_X: u16 = 1;
/// Intel-IA64-Filler
pub const X3_IMM39_2_SIZE_X: u16 = 16;
/// Intel-IA64-Filler
pub const X3_IMM39_2_INST_WORD_POS_X: u16 = 16;
/// Intel-IA64-Filler
pub const X3_IMM39_2_SIGN_VAL_POS_X: u16 = 20;

/// Intel-IA64-Filler
pub const X3_P_INST_WORD_X: u16 = 3;
/// Intel-IA64-Filler
pub const X3_P_SIZE_X: u16 = 4;
/// Intel-IA64-Filler
pub const X3_P_INST_WORD_POS_X: u16 = 0;
/// Intel-IA64-Filler
pub const X3_P_SIGN_VAL_POS_X: u16 = 0;

/// Intel-IA64-Filler
pub const X3_TMPLT_INST_WORD_X: u16 = 0;
/// Intel-IA64-Filler
pub const X3_TMPLT_SIZE_X: u16 = 4;
/// Intel-IA64-Filler
pub const X3_TMPLT_INST_WORD_POS_X: u16 = 0;
/// Intel-IA64-Filler
pub const X3_TMPLT_SIGN_VAL_POS_X: u16 = 0;

/// Intel-IA64-Filler
pub const X3_BTYPE_QP_INST_WORD_X: u16 = 2;
/// Intel-IA64-Filler
pub const X3_BTYPE_QP_SIZE_X: u16 = 9;
/// Intel-IA64-Filler
pub const X3_BTYPE_QP_INST_WORD_POS_X: u16 = 23;
/// Intel-IA64-Filler
pub const X3_BTYPE_QP_INST_VAL_POS_X: u16 = 0;

/// Intel-IA64-Filler
pub const X3_EMPTY_INST_WORD_X: u16 = 1;
/// Intel-IA64-Filler
pub const X3_EMPTY_SIZE_X: u16 = 2;
/// Intel-IA64-Filler
pub const X3_EMPTY_INST_WORD_POS_X: u16 = 14;
/// Intel-IA64-Filler
pub const X3_EMPTY_INST_VAL_POS_X: u16 = 0;

//
// Line number format.
//

// This struct has alignment 1.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageLinenumber {
    /// Symbol table index of function name if Linenumber is 0.
    /// Otherwise virtual address of line number.
    pub symbol_table_index_or_virtual_address: U32<LE>,
    /// Line number.
    pub linenumber: U16<LE>,
}

//
// Based relocation format.
//

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageBaseRelocation {
    pub virtual_address: U32<LE>,
    pub size_of_block: U32<LE>,
    //  pub type_offset[1]: U16<LE>,
}

//
// Based relocation types.
//

newtype!(
    /// Values for `ImageBaseRelocation` `type_offset` entries.
    struct BaseRelocationType(u16);
);

newtype_constant_names!(NAMES_REL_BASED: BaseRelocationType(u16) = {
    IMAGE_REL_BASED_ABSOLUTE = 0,
    IMAGE_REL_BASED_HIGH = 1,
    IMAGE_REL_BASED_LOW = 2,
    IMAGE_REL_BASED_HIGHLOW = 3,
    IMAGE_REL_BASED_HIGHADJ = 4,
    IMAGE_REL_BASED_MACHINE_SPECIFIC_5 = 5,
    IMAGE_REL_BASED_RESERVED = 6,
    IMAGE_REL_BASED_MACHINE_SPECIFIC_7 = 7,
    IMAGE_REL_BASED_MACHINE_SPECIFIC_8 = 8,
    IMAGE_REL_BASED_MACHINE_SPECIFIC_9 = 9,
    IMAGE_REL_BASED_DIR64 = 10,
});

//
// Platform-specific based relocation types.
//

constant_names!(
/// Values for `ImageBaseRelocation` `type_offset` entries on IA64.
pub NAMES_REL_BASED_IA64: BaseRelocationType(u16) = NAMES_REL_BASED + {
    IMAGE_REL_BASED_IA64_IMM64 = 9,
});

constant_names!(
/// Values for `ImageBaseRelocation` `type_offset` entries on MIPS.
pub NAMES_REL_BASED_MIPS: BaseRelocationType(u16) = NAMES_REL_BASED + {
    IMAGE_REL_BASED_MIPS_JMPADDR = 5,
    IMAGE_REL_BASED_MIPS_JMPADDR16 = 9,
});

constant_names!(
/// Values for `ImageBaseRelocation` `type_offset` entries on ARM.
pub NAMES_REL_BASED_ARM: BaseRelocationType(u16) = NAMES_REL_BASED + {
    IMAGE_REL_BASED_ARM_MOV32 = 5,
    IMAGE_REL_BASED_THUMB_MOV32 = 7,
});

constant_names!(
/// Values for `ImageBaseRelocation` `type_offset` entries on RISC-V.
pub NAMES_REL_BASED_RISCV: BaseRelocationType(u16) = NAMES_REL_BASED + {
    IMAGE_REL_BASED_RISCV_HIGH20 = 5,
    IMAGE_REL_BASED_RISCV_LOW12I = 7,
    IMAGE_REL_BASED_RISCV_LOW12S = 8,
});

//
// Archive format.
//

pub const IMAGE_ARCHIVE_START_SIZE: usize = 8;
pub const IMAGE_ARCHIVE_START: &[u8; 8] = b"!<arch>\n";
pub const IMAGE_ARCHIVE_END: &[u8] = b"`\n";
pub const IMAGE_ARCHIVE_PAD: &[u8] = b"\n";
pub const IMAGE_ARCHIVE_LINKER_MEMBER: &[u8; 16] = b"/               ";
pub const IMAGE_ARCHIVE_LONGNAMES_MEMBER: &[u8; 16] = b"//              ";
pub const IMAGE_ARCHIVE_HYBRIDMAP_MEMBER: &[u8; 16] = b"/<HYBRIDMAP>/   ";

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageArchiveMemberHeader {
    /// File member name - `/' terminated.
    pub name: [u8; 16],
    /// File member date - decimal.
    pub date: [u8; 12],
    /// File member user id - decimal.
    pub user_id: [u8; 6],
    /// File member group id - decimal.
    pub group_id: [u8; 6],
    /// File member mode - octal.
    pub mode: [u8; 8],
    /// File member size - decimal.
    pub size: [u8; 10],
    /// String to end header.
    pub end_header: [u8; 2],
}

pub const IMAGE_SIZEOF_ARCHIVE_MEMBER_HDR: u16 = 60;

//
// DLL support.
//

//
// Export Format
//

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageExportDirectory {
    pub characteristics: U32<LE>,
    pub time_date_stamp: U32<LE>,
    pub major_version: U16<LE>,
    pub minor_version: U16<LE>,
    pub name: U32<LE>,
    pub base: U32<LE>,
    pub number_of_functions: U32<LE>,
    pub number_of_names: U32<LE>,
    /// RVA from base of image
    pub address_of_functions: U32<LE>,
    /// RVA from base of image
    pub address_of_names: U32<LE>,
    /// RVA from base of image
    pub address_of_name_ordinals: U32<LE>,
}

//
// Import Format
//

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageImportByName {
    pub hint: U16<LE>,
    //pub name: [i8; 1],
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageThunkData64(pub U64<LE>);
/*
    union {
/// PBYTE
        pub forwarder_string: U64<LE>,
/// PDWORD
        pub function: U64<LE>,
        pub ordinal: U64<LE>,
/// PIMAGE_IMPORT_BY_NAME
        pub address_of_data: U64<LE>,
    } u1;
*/

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageThunkData32(pub U32<LE>);
/*
    union {
/// PBYTE
        pub forwarder_string: U32<LE>,
/// PDWORD
        pub function: U32<LE>,
        pub ordinal: U32<LE>,
/// PIMAGE_IMPORT_BY_NAME
        pub address_of_data: U32<LE>,
    } u1;
}
*/

pub const IMAGE_ORDINAL_FLAG64: u64 = 0x8000000000000000;
pub const IMAGE_ORDINAL_FLAG32: u32 = 0x80000000;

/*
#define IMAGE_ORDINAL64(Ordinal) (Ordinal & 0xffff)
#define IMAGE_ORDINAL32(Ordinal) (Ordinal & 0xffff)
#define IMAGE_SNAP_BY_ORDINAL64(Ordinal) ((Ordinal & IMAGE_ORDINAL_FLAG64) != 0)
#define IMAGE_SNAP_BY_ORDINAL32(Ordinal) ((Ordinal & IMAGE_ORDINAL_FLAG32) != 0)

*/

//
// Thread Local Storage
//

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageTlsDirectory64 {
    pub start_address_of_raw_data: U64<LE>,
    pub end_address_of_raw_data: U64<LE>,
    /// PDWORD
    pub address_of_index: U64<LE>,
    /// PIMAGE_TLS_CALLBACK *;
    pub address_of_call_backs: U64<LE>,
    pub size_of_zero_fill: U32<LE>,
    pub characteristics: U32<LE>,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageTlsDirectory32 {
    pub start_address_of_raw_data: U32<LE>,
    pub end_address_of_raw_data: U32<LE>,
    /// PDWORD
    pub address_of_index: U32<LE>,
    /// PIMAGE_TLS_CALLBACK *
    pub address_of_call_backs: U32<LE>,
    pub size_of_zero_fill: U32<LE>,
    pub characteristics: U32<LE>,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageImportDescriptor {
    /// RVA to original unbound IAT (`ImageThunkData32`/`ImageThunkData64`)
    /// 0 for terminating null import descriptor
    pub original_first_thunk: U32<LE>,
    /// 0 if not bound,
    /// -1 if bound, and real date\time stamp
    ///     in IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT (new BIND)
    /// O.W. date/time stamp of DLL bound to (Old BIND)
    pub time_date_stamp: U32<LE>,
    /// -1 if no forwarders
    pub forwarder_chain: U32<LE>,
    pub name: U32<LE>,
    /// RVA to IAT (if bound this IAT has actual addresses)
    pub first_thunk: U32<LE>,
}

impl ImageImportDescriptor {
    /// Tell whether this import descriptor is the null descriptor
    /// (used to mark the end of the iterator array in a PE)
    pub fn is_null(&self) -> bool {
        self.original_first_thunk.get(LE) == 0
            && self.time_date_stamp.get(LE) == 0
            && self.forwarder_chain.get(LE) == 0
            && self.name.get(LE) == 0
            && self.first_thunk.get(LE) == 0
    }
}

//
// New format import descriptors pointed to by DataDirectory[ IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT ]
//

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageBoundImportDescriptor {
    pub time_date_stamp: U32<LE>,
    pub offset_module_name: U16<LE>,
    pub number_of_module_forwarder_refs: U16<LE>,
    // Array of zero or more IMAGE_BOUND_FORWARDER_REF follows
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageBoundForwarderRef {
    pub time_date_stamp: U32<LE>,
    pub offset_module_name: U16<LE>,
    pub reserved: U16<LE>,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageDelayloadDescriptor {
    pub attributes: U32<LE>,

    /// RVA to the name of the target library (NULL-terminate ASCII string)
    pub dll_name_rva: U32<LE>,
    /// RVA to the HMODULE caching location (PHMODULE)
    pub module_handle_rva: U32<LE>,
    /// RVA to the start of the IAT (PIMAGE_THUNK_DATA)
    pub import_address_table_rva: U32<LE>,
    /// RVA to the start of the name table (PIMAGE_THUNK_DATA::AddressOfData)
    pub import_name_table_rva: U32<LE>,
    /// RVA to an optional bound IAT
    pub bound_import_address_table_rva: U32<LE>,
    /// RVA to an optional unload info table
    pub unload_information_table_rva: U32<LE>,
    /// 0 if not bound, otherwise, date/time of the target DLL
    pub time_date_stamp: U32<LE>,
}

impl ImageDelayloadDescriptor {
    /// Tell whether this delay-load import descriptor is the null descriptor
    /// (used to mark the end of the iterator array in a PE)
    pub fn is_null(&self) -> bool {
        self.attributes.get(LE) == 0
            && self.dll_name_rva.get(LE) == 0
            && self.module_handle_rva.get(LE) == 0
            && self.import_address_table_rva.get(LE) == 0
            && self.import_name_table_rva.get(LE) == 0
            && self.bound_import_address_table_rva.get(LE) == 0
            && self.unload_information_table_rva.get(LE) == 0
            && self.time_date_stamp.get(LE) == 0
    }
}

/// Delay load version 2 flag for `ImageDelayloadDescriptor::attributes`.
pub const IMAGE_DELAYLOAD_RVA_BASED: u32 = 0x1;

//
// Resource Format.
//

//
// Resource directory consists of two counts, following by a variable length
// array of directory entries.  The first count is the number of entries at
// beginning of the array that have actual names associated with each entry.
// The entries are in ascending order, case insensitive strings.  The second
// count is the number of entries that immediately follow the named entries.
// This second count identifies the number of entries that have 16-bit integer
// Ids as their name.  These entries are also sorted in ascending order.
//
// This structure allows fast lookup by either name or number, but for any
// given resource entry only one form of lookup is supported, not both.
// This is consistent with the syntax of the .RC file and the .RES file.
//

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageResourceDirectory {
    pub characteristics: U32<LE>,
    pub time_date_stamp: U32<LE>,
    pub major_version: U16<LE>,
    pub minor_version: U16<LE>,
    pub number_of_named_entries: U16<LE>,
    pub number_of_id_entries: U16<LE>,
}

pub const IMAGE_RESOURCE_NAME_IS_STRING: u32 = 0x8000_0000;
pub const IMAGE_RESOURCE_DATA_IS_DIRECTORY: u32 = 0x8000_0000;
//
// Each directory contains the 32-bit Name of the entry and an offset,
// relative to the beginning of the resource directory of the data associated
// with this directory entry.  If the name of the entry is an actual text
// string instead of an integer Id, then the high order bit of the name field
// is set to one and the low order 31-bits are an offset, relative to the
// beginning of the resource directory of the string, which is of type
// IMAGE_RESOURCE_DIRECTORY_STRING.  Otherwise the high bit is clear and the
// low-order 16-bits are the integer Id that identify this resource directory
// entry. If the directory entry is yet another resource directory (i.e. a
// subdirectory), then the high order bit of the offset field will be
// set to indicate this.  Otherwise the high bit is clear and the offset
// field points to a resource data entry.
//

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageResourceDirectoryEntry {
    pub name_or_id: U32<LE>,
    pub offset_to_data_or_directory: U32<LE>,
}

//
// For resource directory entries that have actual string names, the Name
// field of the directory entry points to an object of the following type.
// All of these string objects are stored together after the last resource
// directory entry and before the first resource data object.  This minimizes
// the impact of these variable length objects on the alignment of the fixed
// size directory entry objects.
//

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageResourceDirectoryString {
    pub length: U16<LE>,
    //pub name_string: [i8; 1],
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageResourceDirStringU {
    pub length: U16<LE>,
    //pub name_string: [U16<LE>; 1],
}

//
// Each resource data entry describes a leaf node in the resource directory
// tree.  It contains an offset, relative to the beginning of the resource
// directory of the data for the resource, a size field that gives the number
// of bytes of data at that offset, a CodePage that should be used when
// decoding code point values within the resource data.  Typically for new
// applications the code page would be the unicode code page.
//

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageResourceDataEntry {
    /// RVA of the data.
    pub offset_to_data: U32<LE>,
    pub size: U32<LE>,
    pub code_page: U32<LE>,
    pub reserved: U32<LE>,
}

// Resource type: https://docs.microsoft.com/en-us/windows/win32/menurc/resource-types

/// Values for resource types.
///
/// These are used in `ImageResourceDirectoryEntry::name_or_id` at level 0 of the tree.
#[cfg(feature = "names")]
pub const NAMES_RT: &ConstantNames<u16> = &_NAMES_RT;

constant_names!(_NAMES_RT: u16 = {
    /// ID for: Hardware-dependent cursor resource.
    RT_CURSOR = 1,
    /// ID for: Bitmap resource.
    RT_BITMAP = 2,
    /// ID for: Hardware-dependent icon resource.
    RT_ICON = 3,
    /// ID for: Menu resource.
    RT_MENU = 4,
    /// ID for: Dialog box.
    RT_DIALOG = 5,
    /// ID for: String-table entry.
    RT_STRING = 6,
    /// ID for: Font directory resource.
    RT_FONTDIR = 7,
    /// ID for: Font resource.
    RT_FONT = 8,
    /// ID for: Accelerator table.
    RT_ACCELERATOR = 9,
    /// ID for: Application-defined resource (raw data).
    RT_RCDATA = 10,
    /// ID for: Message-table entry.
    RT_MESSAGETABLE = 11,
    /// ID for: Hardware-independent cursor resource.
    RT_GROUP_CURSOR = 12,
    /// ID for: Hardware-independent icon resource.
    RT_GROUP_ICON = 14,
    /// ID for: Version resource.
    RT_VERSION = 16,
    /// ID for: Allows a resource editing tool to associate a string with an .rc file.
    RT_DLGINCLUDE = 17,
    /// ID for: Plug and Play resource.
    RT_PLUGPLAY = 19,
    /// ID for: VXD.
    RT_VXD = 20,
    /// ID for: Animated cursor.
    RT_ANICURSOR = 21,
    /// ID for: Animated icon.
    RT_ANIICON = 22,
    /// ID for: HTML resource.
    RT_HTML = 23,
    /// ID for: Side-by-Side Assembly Manifest.
    RT_MANIFEST = 24,
});

//
// Code Integrity in loadconfig (CI)
//

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageLoadConfigCodeIntegrity {
    /// Flags to indicate if CI information is available, etc.
    pub flags: U16<LE>,
    /// 0xFFFF means not available
    pub catalog: U16<LE>,
    pub catalog_offset: U32<LE>,
    /// Additional bitmask to be defined later
    pub reserved: U32<LE>,
}

//
// Dynamic value relocation table in loadconfig
//

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageDynamicRelocationTable {
    pub version: U32<LE>,
    pub size: U32<LE>,
    // DynamicRelocations: [ImageDynamicRelocation; 0],
}

//
// Dynamic value relocation entries following IMAGE_DYNAMIC_RELOCATION_TABLE
//

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageDynamicRelocation32 {
    pub symbol: U32<LE, DynamicRelocationSymbol>,
    pub base_reloc_size: U32<LE>,
    // BaseRelocations: [ImageBaseRelocation; 0],
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageDynamicRelocation64 {
    pub symbol: U64<LE, DynamicRelocationSymbol>,
    pub base_reloc_size: U32<LE>,
    // BaseRelocations: [ImageBaseRelocation; 0],
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageDynamicRelocation32V2 {
    pub header_size: U32<LE>,
    pub fixup_info_size: U32<LE>,
    pub symbol: U32<LE, DynamicRelocationSymbol>,
    pub symbol_group: U32<LE>,
    pub flags: U32<LE>,
    // ...     variable length header fields
    // pub     fixup_info: [u8; fixup_info_size]
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageDynamicRelocation64V2 {
    pub header_size: U32<LE>,
    pub fixup_info_size: U32<LE>,
    pub symbol: U64<LE, DynamicRelocationSymbol>,
    pub symbol_group: U32<LE>,
    pub flags: U32<LE>,
    // ...     variable length header fields
    // pub     fixup_info[u8; fixup_info_size]
}

//
// Defined symbolic dynamic relocation entries.
//
newtype!(
    /// Values for `ImageDynamicRelocation*::symbol`.
    ///
    /// Small values have special meaning. Other values are RVAs.
    struct DynamicRelocationSymbol(u64);
);

newtype_constant_names!(NAMES_DYNAMIC_RELOCATION: DynamicRelocationSymbol(u64) = {
    IMAGE_DYNAMIC_RELOCATION_GUARD_RF_PROLOGUE = 0x0000_0001,
    IMAGE_DYNAMIC_RELOCATION_GUARD_RF_EPILOGUE = 0x0000_0002,
    IMAGE_DYNAMIC_RELOCATION_GUARD_IMPORT_CONTROL_TRANSFER = 0x0000_0003,
    IMAGE_DYNAMIC_RELOCATION_GUARD_INDIR_CONTROL_TRANSFER = 0x0000_0004,
    IMAGE_DYNAMIC_RELOCATION_GUARD_SWITCHTABLE_BRANCH = 0x0000_0005,
    IMAGE_DYNAMIC_RELOCATION_FUNCTION_OVERRIDE = 0x0000_0007,
    IMAGE_DYNAMIC_RELOCATION_ARM64_KERNEL_IMPORT_CALL_TRANSFER = 0x0000_0008,
});

// This struct has alignment 1.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImagePrologueDynamicRelocationHeader {
    pub prologue_byte_count: u8,
    // pub prologue_bytes: [u8; prologue_byte_count],
}

// This struct has alignment 1.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageEpilogueDynamicRelocationHeader {
    pub epilogue_count: U32<LE>,
    pub epilogue_byte_count: u8,
    pub branch_descriptor_element_size: u8,
    pub branch_descriptor_count: U16<LE>,
    // pub branch_descriptors[...],
    // pub branch_descriptor_bit_map[...],
}

/*
// TODO? bitfields
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageImportControlTransferDynamicRelocation {
    DWORD       PageRelativeOffset : 12;
    DWORD       IndirectCall       : 1;
    DWORD       IATIndex           : 19;
}

// On ARM64, an optimized imported function uses the following data structure
// insted of a `ImageImportControlTransferDynamicRelocation`.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageImportControlTransferArm64Relocation {
    DWORD PageRelativeOffset : 10;  // Offset to the call instruction shifted right by 2 (4-byte aligned instruction)
    DWORD IndirectCall       :  1;  // 0 if target instruction is a BR, 1 if BLR.
    DWORD RegisterIndex      :  5;  // Register index used for the indirect call/jump.
    DWORD ImportType         :  1;  // 0 if this refers to a static import, 1 for delayload import
    DWORD IATIndex           : 15;  // IAT index of the corresponding import.
                                    // 0x7FFF is a special value indicating no index.
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageIndirControlTransferDynamicRelocation {
    WORD        PageRelativeOffset : 12;
    WORD        IndirectCall       : 1;
    WORD        RexWPrefix         : 1;
    WORD        CfgCheck           : 1;
    WORD        Reserved           : 1;
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageSwitchtableBranchDynamicRelocation {
    WORD        PageRelativeOffset : 12;
    WORD        RegisterNumber     : 4;
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageFunctionOverrideHeader {
    pub func_override_size: U32<LE>,

    // FuncOverrideSize bytes in size
    // pub func_override_info: [ImageFunctionOverrideDynamicRelocation; 0],

    // BDD region, size in bytes: DVRTEntrySize - sizeof(IMAGE_FUNCTION_OVERRIDE_HEADER) - FuncOverrideSize
    // pub bdd_info: ImageBddInfo,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageFunctionOverrideDynamicRelocation {
    /// RVA of original function
    pub original_rva: U32<LE>,
    /// Offset into the BDD region
    pub bdd_offset: U32<LE>,
    /// Size in bytes taken by RVAs. Must be multiple of sizeof(DWORD).
    pub rva_size: U32<LE>,
    /// Size in bytes taken by BaseRelocs
    pub base_reloc_size: U32<LE>,

    // Array containing overriding func RVAs.
    // pub rvas: [U32<LE>; rva_size / sizeof(DWORD)],

    // Base relocations (RVA + Size + TO)
    //  Padded with extra TOs for 4B alignment
    // BaseRelocSize size in bytes
    // pub base_relocs: [ImageBaseRelocation; 0],
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageBddInfo {
    // decides the semantics of serialized BDD
    pub version: U32<LE>,
    pub bdd_size: U32<LE>,
    // bdd_size size in bytes.
    //pub bdd_nodes: [ImageBddDynamicRelocation; 0],
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageBddDynamicRelocation {
    /// Index of FALSE edge in BDD array
    pub left: U16<LE>,
    /// Index of TRUE edge in BDD array
    pub right: U16<LE>,
    /// Either FeatureNumber or Index into RVAs array
    pub value: U32<LE>,
}

// Function override relocation types in DVRT records.

constant_names!(NAMES_FUNCTION_OVERRIDE: FunctionOverride() = {
    IMAGE_FUNCTION_OVERRIDE_INVALID = 0,
    // 32-bit relative address from byte following reloc
    IMAGE_FUNCTION_OVERRIDE_X64_REL32 = 1,
    // 26 bit offset << 2 & sign ext. for B & BL
    IMAGE_FUNCTION_OVERRIDE_ARM64_BRANCH26 = 2,
    IMAGE_FUNCTION_OVERRIDE_ARM64_THUNK = 3,
});

*/

//
// Load Configuration Directory Entry
//

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageLoadConfigDirectory32 {
    pub size: U32<LE>,
    pub time_date_stamp: U32<LE>,
    pub major_version: U16<LE>,
    pub minor_version: U16<LE>,
    pub global_flags_clear: U32<LE>,
    pub global_flags_set: U32<LE>,
    pub critical_section_default_timeout: U32<LE>,
    pub de_commit_free_block_threshold: U32<LE>,
    pub de_commit_total_free_threshold: U32<LE>,
    /// VA
    pub lock_prefix_table: U32<LE>,
    pub maximum_allocation_size: U32<LE>,
    pub virtual_memory_threshold: U32<LE>,
    pub process_heap_flags: U32<LE>,
    pub process_affinity_mask: U32<LE>,
    pub csd_version: U16<LE>,
    pub dependent_load_flags: U16<LE>,
    /// VA
    pub edit_list: U32<LE>,
    /// VA
    pub security_cookie: U32<LE>,
    /// VA
    pub sehandler_table: U32<LE>,
    pub sehandler_count: U32<LE>,
    /// VA
    pub guard_cf_check_function_pointer: U32<LE>,
    /// VA
    pub guard_cf_dispatch_function_pointer: U32<LE>,
    /// VA
    pub guard_cf_function_table: U32<LE>,
    pub guard_cf_function_count: U32<LE>,
    pub guard_flags: U32<LE, GuardFlags>,
    pub code_integrity: ImageLoadConfigCodeIntegrity,
    /// VA
    pub guard_address_taken_iat_entry_table: U32<LE>,
    pub guard_address_taken_iat_entry_count: U32<LE>,
    /// VA
    pub guard_long_jump_target_table: U32<LE>,
    pub guard_long_jump_target_count: U32<LE>,
    /// VA
    pub dynamic_value_reloc_table: U32<LE>,
    pub chpe_metadata_pointer: U32<LE>,
    /// VA
    pub guard_rf_failure_routine: U32<LE>,
    /// VA
    pub guard_rf_failure_routine_function_pointer: U32<LE>,
    pub dynamic_value_reloc_table_offset: U32<LE>,
    pub dynamic_value_reloc_table_section: U16<LE>,
    pub reserved2: U16<LE>,
    /// VA
    pub guard_rf_verify_stack_pointer_function_pointer: U32<LE>,
    pub hot_patch_table_offset: U32<LE>,
    pub reserved3: U32<LE>,
    /// VA
    pub enclave_configuration_pointer: U32<LE>,
    /// VA
    pub volatile_metadata_pointer: U32<LE>,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageLoadConfigDirectory64 {
    pub size: U32<LE>,
    pub time_date_stamp: U32<LE>,
    pub major_version: U16<LE>,
    pub minor_version: U16<LE>,
    pub global_flags_clear: U32<LE>,
    pub global_flags_set: U32<LE>,
    pub critical_section_default_timeout: U32<LE>,
    pub de_commit_free_block_threshold: U64<LE>,
    pub de_commit_total_free_threshold: U64<LE>,
    /// VA
    pub lock_prefix_table: U64<LE>,
    pub maximum_allocation_size: U64<LE>,
    pub virtual_memory_threshold: U64<LE>,
    pub process_affinity_mask: U64<LE>,
    pub process_heap_flags: U32<LE>,
    pub csd_version: U16<LE>,
    pub dependent_load_flags: U16<LE>,
    /// VA
    pub edit_list: U64<LE>,
    /// VA
    pub security_cookie: U64<LE>,
    /// VA
    pub sehandler_table: U64<LE>,
    pub sehandler_count: U64<LE>,
    /// VA
    pub guard_cf_check_function_pointer: U64<LE>,
    /// VA
    pub guard_cf_dispatch_function_pointer: U64<LE>,
    /// VA
    pub guard_cf_function_table: U64<LE>,
    pub guard_cf_function_count: U64<LE>,
    pub guard_flags: U32<LE, GuardFlags>,
    pub code_integrity: ImageLoadConfigCodeIntegrity,
    /// VA
    pub guard_address_taken_iat_entry_table: U64<LE>,
    pub guard_address_taken_iat_entry_count: U64<LE>,
    /// VA
    pub guard_long_jump_target_table: U64<LE>,
    pub guard_long_jump_target_count: U64<LE>,
    /// VA
    pub dynamic_value_reloc_table: U64<LE>,
    /// VA
    pub chpe_metadata_pointer: U64<LE>,
    /// VA
    pub guard_rf_failure_routine: U64<LE>,
    /// VA
    pub guard_rf_failure_routine_function_pointer: U64<LE>,
    pub dynamic_value_reloc_table_offset: U32<LE>,
    pub dynamic_value_reloc_table_section: U16<LE>,
    pub reserved2: U16<LE>,
    /// VA
    pub guard_rf_verify_stack_pointer_function_pointer: U64<LE>,
    pub hot_patch_table_offset: U32<LE>,
    pub reserved3: U32<LE>,
    /// VA
    pub enclave_configuration_pointer: U64<LE>,
    /// VA
    pub volatile_metadata_pointer: U64<LE>,
}

/*
flag_names!(NAMES_HOT_PATCH_INFO_FLAG: HotPatchInfoFlags(u32) = {
    IMAGE_HOT_PATCH_INFO_FLAG_PATCHORDERCRITICAL = 0x0000_0001,
    IMAGE_HOT_PATCH_INFO_FLAG_HOTSWAP = 0x0000_0002,
});
*/

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageHotPatchInfo {
    pub version: U32<LE>,
    pub size: U32<LE>,
    pub sequence_number: U32<LE>,
    pub base_image_list: U32<LE>,
    pub base_image_count: U32<LE>,
    /// Version 2 and later
    pub buffer_offset: U32<LE>,
    /// Version 3 and later
    pub extra_patch_size: U32<LE>,
    // Version 4 and later
    //pub min_sequence_number: U32<LE>,
    // Version 4 and later
    //pub flags: U32<LE, HotPatchInfoFlags>,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageHotPatchBase {
    pub sequence_number: U32<LE>,
    pub flags: U32<LE, HotPatchBaseFlags>,
    pub original_time_date_stamp: U32<LE>,
    pub original_check_sum: U32<LE>,
    pub code_integrity_info: U32<LE>,
    pub code_integrity_size: U32<LE>,
    pub patch_table: U32<LE>,
    /// Version 2 and later
    pub buffer_offset: U32<LE>,
}

/*
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageHotPatchMachine {
        DWORD _x86     :  1;
        DWORD Amd64    :  1;
        DWORD Arm64    :  1;
        DWORD Amd64EC  :  1;
}
*/

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageHotPatchHashes {
    pub sha256: [u8; 32],
    pub sha1: [u8; 20],
}

newtype!(
    /// Values for `ImageHotPatchBase::flags`.
    struct HotPatchBaseFlags(u32);
);

newtype_flag_names!(NAMES_HOT_PATCH_BASE: HotPatchBaseFlags(u32) = {
    IMAGE_HOT_PATCH_BASE_OBLIGATORY = 0x0000_0001,
    IMAGE_HOT_PATCH_BASE_CAN_ROLL_BACK = 0x0000_0002,

    IMAGE_HOT_PATCH_BASE_MACHINE_I386 = 0x0000_0004,
    IMAGE_HOT_PATCH_BASE_MACHINE_ARM64 = 0x0000_0008,
    IMAGE_HOT_PATCH_BASE_MACHINE_AMD64 = 0x0000_0010,
});

newtype!(
    struct HotPatchChunkFlags(u32);
);

// TODO: convenience methods for subfields

newtype_flag_names!(NAMES_HOT_PATCH_CHUNK: HotPatchChunkFlags(u32) = {
    IMAGE_HOT_PATCH_CHUNK_INVERSE = 0x8000_0000,
    IMAGE_HOT_PATCH_CHUNK_OBLIGATORY = 0x4000_0000,
    _ = IMAGE_HOT_PATCH_CHUNK_TYPE => NAMES_HOT_PATCH_CHUNK_TYPE,
});

pub const IMAGE_HOT_PATCH_CHUNK_RESERVED: u32 = 0x3FF0_3000;
pub const IMAGE_HOT_PATCH_CHUNK_TYPE: u32 = 0x000F_C000;
pub const IMAGE_HOT_PATCH_CHUNK_SOURCE_RVA: u32 = 0x0000_8000;
pub const IMAGE_HOT_PATCH_CHUNK_TARGET_RVA: u32 = 0x0000_4000;
pub const IMAGE_HOT_PATCH_CHUNK_SIZE: u32 = 0x0000_0FFF;

constant_names!(NAMES_HOT_PATCH_CHUNK_TYPE: HotPatchChunkFlags(u32) = {
    IMAGE_HOT_PATCH_NONE = 0x0000_0000,
    IMAGE_HOT_PATCH_FUNCTION = 0x0001_C000,
    IMAGE_HOT_PATCH_ABSOLUTE = 0x0002_C000,
    IMAGE_HOT_PATCH_REL32 = 0x0003_C000,
    IMAGE_HOT_PATCH_CALL_TARGET = 0x0004_4000,
    IMAGE_HOT_PATCH_INDIRECT = 0x0005_C000,
    IMAGE_HOT_PATCH_NO_CALL_TARGET = 0x0006_4000,
    IMAGE_HOT_PATCH_DYNAMIC_VALUE = 0x0007_8000,
});

//
// Hot-Swap Image Info
//

/*
pub const IMAGE_HOTSWAP_ENDPOINT_TABLE_SECTION: &str = ".shsept";

constant_names!(NAMES_ENDPOINT_RETURN_TYPE: HotswapArm64EndpointInfoCcReturn(u32) = {
    EndpointReturnTypeNone = 0,
    EndpointReturnTypeX0,
    EndpointReturnTypeX0_X1,
    EndpointReturnTypeX0_X2,
    EndpointReturnTypeX0_X3,
    EndpointReturnTypeQ0,
    EndpointReturnTypeQ0_Q1,
    EndpointReturnTypeQ0_Q2,
    EndpointReturnTypeQ0_Q3,
    EndpointReturnTypeX8
});

constant_names!(NAMES_ENDPOINT_PARAM_REG: HotswapX64EndpointInfoCcReg(u32) = {
    EndpointParamRegNone = 0x00,
    EndpointParamRegRAX = 0x01,
    EndpointParamRegRCX = 0x02,
    EndpointParamRegRDX = 0x03,
    EndpointParamRegR8 = 0x09,
    EndpointParamRegR9 = 0x0A,
    EndpointParamRegXMM0 = 0xC8,
    EndpointParamRegXMM1 = 0xC9,
    EndpointParamRegXMM2 = 0xCA,
    EndpointParamRegXMM3 = 0xCB,
});


#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageHotswapEndpointInfoHeaderCommon {
    pub version: U32<LE>,
    pub size: U32<LE>,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageHotswapEndpointInfoEntryCommon {
    // Size of this current entry structure.
    pub size: U32<LE>,
    // RVA of the endpoint within the image.
    pub rva: U32<LE>,
    pub name_size: U32<LE>,
    pub name_offset: U32<LE>,
}

pub const IMAGE_HOTSWAP_ENDPOINT_INFO_V2: u32 = 2;

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageHotswapX64EndpointInfoEntryV2 {
    pub common: ImageHotswapEndpointInfoEntryCommon,
    pub arg_regs: [U32<LE, HotswapX64EndpointInfoCcReg>; 4],
    pub arg_stack_size: U32<LE>,
    pub ret_reg: U32<LE, HotswapX64EndpointInfoCcReg>,
    // name: [u8; 0],
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageHotswapArm64EndpointInfoEntryV2 {
    pub common: ImageHotswapEndpointInfoEntryCommon,
    pub int_args: U32<LE>,
    pub float_args: U32<LE>,
    pub arg_stack_size: U32<LE>,
    pub return_type: U32<LE, HotswapArm64EndpointInfoCcReturn>,
    // name: [u8; 0],
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageHotswapEndpointInfoHeaderV2 {
    pub common: ImageHotswapEndpointInfoHeaderCommon,
    pub count: U32<LE>,
}
*/

newtype!(
    /// Values for `ImageLoadConfigDirectory*::guard_flags`.
    struct GuardFlags(u32);
);

newtype_flag_names!(NAMES_GUARD: GuardFlags(u32) = {
    /// Module performs control flow integrity checks using system-supplied support
    IMAGE_GUARD_CF_INSTRUMENTED = 0x0000_0100,
    /// Module performs control flow and write integrity checks
    IMAGE_GUARD_CFW_INSTRUMENTED = 0x0000_0200,
    /// Module contains valid control flow target metadata
    IMAGE_GUARD_CF_FUNCTION_TABLE_PRESENT = 0x0000_0400,
    /// Module does not make use of the /GS security cookie
    IMAGE_GUARD_SECURITY_COOKIE_UNUSED = 0x0000_0800,
    /// Module supports read only delay load IAT
    IMAGE_GUARD_PROTECT_DELAYLOAD_IAT = 0x0000_1000,
    /// Delayload import table in its own .didat section (with nothing else in it) that can be freely reprotected
    IMAGE_GUARD_DELAYLOAD_IAT_IN_ITS_OWN_SECTION = 0x0000_2000,
    /// Module contains suppressed export information.
    ///
    /// This also infers that the address taken taken IAT table is also present in the load config.
    IMAGE_GUARD_CF_EXPORT_SUPPRESSION_INFO_PRESENT = 0x0000_4000,
    /// Module enables suppression of exports
    IMAGE_GUARD_CF_ENABLE_EXPORT_SUPPRESSION = 0x0000_8000,
    /// Module contains longjmp target information
    IMAGE_GUARD_CF_LONGJUMP_TABLE_PRESENT = 0x0001_0000,
    /// Module contains return flow instrumentation and metadata
    IMAGE_GUARD_RF_INSTRUMENTED = 0x0002_0000,
    /// Module requests that the OS enable return flow protection
    IMAGE_GUARD_RF_ENABLE = 0x0004_0000,
    /// Module requests that the OS enable return flow protection in strict mode
    IMAGE_GUARD_RF_STRICT = 0x0008_0000,
    /// Module was built with retpoline support
    IMAGE_GUARD_RETPOLINE_PRESENT = 0x0010_0000,
    // Was EHCont flag on VB (20H1)
    // DO_NOT_USE = 0x00200000,
    /// Module contains EH continuation target information
    IMAGE_GUARD_EH_CONTINUATION_TABLE_PRESENT = 0x0040_0000,
    /// Module was built with xfg (deprecated)
    IMAGE_GUARD_XFG_ENABLED = 0x0080_0000,
    /// Module has CastGuard instrumentation present
    IMAGE_GUARD_CASTGUARD_PRESENT = 0x0100_0000,
    /// Module has Guarded Memcpy instrumentation present
    IMAGE_GUARD_MEMCPY_PRESENT = 0x0200_0000,
});

/// Stride of Guard CF function table encoded in these bits (additional count of bytes per element)
pub const IMAGE_GUARD_CF_FUNCTION_TABLE_SIZE_MASK: u32 = 0xF000_0000;
/// Shift to right-justify Guard CF function table stride
pub const IMAGE_GUARD_CF_FUNCTION_TABLE_SIZE_SHIFT: u32 = 28;

//
// GFIDS table entry flags.
//

newtype!(
    /// Values for flags in the Guard CF function table entries.
    struct GuardFunctionFlags(u16);
);

newtype_flag_names!(NAMES_GUARD_FLAG: GuardFunctionFlags(u16) = {
    /// The containing GFID entry is suppressed
    IMAGE_GUARD_FLAG_FID_SUPPRESSED = 0x01,
    /// The containing GFID entry is export suppressed
    IMAGE_GUARD_FLAG_EXPORT_SUPPRESSED = 0x02,
    IMAGE_GUARD_FLAG_FID_LANGEXCPTHANDLER = 0x04,
    IMAGE_GUARD_FLAG_FID_XFG = 0x08,
});

//
// WIN CE Exception table format
//

//
// Function table entry format.  Function table is pointed to by the
// IMAGE_DIRECTORY_ENTRY_EXCEPTION directory entry.
//

/*
// TODO? bitfields
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageCeRuntimeFunctionEntry {
    pub func_start: U32<LE>,
    DWORD PrologLen : 8;
    DWORD FuncLen : 22;
    DWORD ThirtyTwoBit : 1;
    DWORD ExceptionFlag : 1;
}
*/

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageArmRuntimeFunctionEntry {
    pub begin_address: U32<LE>,
    pub unwind_data: U32<LE>,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageArm64RuntimeFunctionEntry {
    pub begin_address: U32<LE>,
    pub unwind_data: U32<LE>,
    /* unwind_data bits:
            DWORD Flag : 2;
            DWORD FunctionLength : 11;
            DWORD RegF : 3;
            DWORD RegI : 4;
            DWORD H : 1;
            DWORD CR : 2;
            DWORD FrameSize : 9;
    */
}

/*
constant_names!(ARM64_FNPDATA_FLAGS = {
    PdataRefToFullXdata = 0,
    PdataPackedUnwindFunction = 1,
    PdataPackedUnwindFragment = 2,
});

constant_names!(ARM64_FNPDATA_CR = {
    PdataCrUnchained = 0,
    PdataCrUnchainedSavedLr = 1,
    PdataCrChainedWithPac = 2,
    PdataCrChained = 3,
});

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageArm64RuntimeFunctionEntryXdata {
    pub header_data: U32<LE>,
/* header_data bits:
        DWORD FunctionLength : 18;      // in words (2 bytes)
        DWORD Version : 2;
        DWORD ExceptionDataPresent : 1;
        DWORD EpilogInHeader : 1;
        DWORD EpilogCount : 5;          // number of epilogs or byte index of the first unwind code for the one only epilog
        DWORD CodeWords : 5;            // number of dwords with unwind codes
*/
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageArm64RuntimeFunctionEntryXdataExtended {
    pub extended_header_data: U32<LE>,
/* extended_header_data bits:
    struct {
        DWORD ExtendedEpilogCount : 16;
        DWORD ExtendedCodeWords : 8;
    } DUMMYSTRUCTNAME;
*/
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageArm64RuntimeFunctionEntryXdataEpilogScope {
    pub epilog_scope_data: U32<LE>,
/* epilog_scope_data bits:
        DWORD EpilogStartOffset : 18;   // offset in bytes, divided by 4, of the epilog relative to the start of the function.
        DWORD Res0: 4;
        DWORD EpilogStartIndex : 10;    // byte index of the first unwind code that describes this epilog.
*/
}
*/

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageAlpha64RuntimeFunctionEntry {
    pub begin_address: U64<LE>,
    pub end_address: U64<LE>,
    pub exception_handler: U64<LE>,
    pub handler_data: U64<LE>,
    pub prolog_end_address: U64<LE>,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageAlphaRuntimeFunctionEntry {
    pub begin_address: U32<LE>,
    pub end_address: U32<LE>,
    pub exception_handler: U32<LE>,
    pub handler_data: U32<LE>,
    pub prolog_end_address: U32<LE>,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageRuntimeFunctionEntry {
    pub begin_address: U32<LE>,
    pub end_address: U32<LE>,
    pub unwind_info_address_or_data: U32<LE>,
}

//
// Software enclave information
//

pub const IMAGE_ENCLAVE_LONG_ID_LENGTH: usize = 32;
pub const IMAGE_ENCLAVE_SHORT_ID_LENGTH: usize = 16;

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageEnclaveConfig32 {
    pub size: U32<LE>,
    pub minimum_required_config_size: U32<LE>,
    pub policy_flags: U32<LE, EnclavePolicyFlags>,
    pub number_of_imports: U32<LE>,
    pub import_list: U32<LE>,
    pub import_entry_size: U32<LE>,
    pub family_id: [u8; IMAGE_ENCLAVE_SHORT_ID_LENGTH],
    pub image_id: [u8; IMAGE_ENCLAVE_SHORT_ID_LENGTH],
    pub image_version: U32<LE>,
    pub security_version: U32<LE>,
    pub enclave_size: U32<LE>,
    pub number_of_threads: U32<LE>,
    pub enclave_flags: U32<LE, EnclaveFlags>,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageEnclaveConfig64 {
    pub size: U32<LE>,
    pub minimum_required_config_size: U32<LE>,
    pub policy_flags: U32<LE, EnclavePolicyFlags>,
    pub number_of_imports: U32<LE>,
    pub import_list: U32<LE>,
    pub import_entry_size: U32<LE>,
    pub family_id: [u8; IMAGE_ENCLAVE_SHORT_ID_LENGTH],
    pub image_id: [u8; IMAGE_ENCLAVE_SHORT_ID_LENGTH],
    pub image_version: U32<LE>,
    pub security_version: U32<LE>,
    pub enclave_size: U64<LE>,
    pub number_of_threads: U32<LE>,
    pub enclave_flags: U32<LE, EnclaveFlags>,
}

//pub const IMAGE_ENCLAVE_MINIMUM_CONFIG_SIZE: usize = FIELD_OFFSET(IMAGE_ENCLAVE_CONFIG, EnclaveFlags);

newtype!(
    struct EnclavePolicyFlags(u32);
);
newtype_flag_names!(NAMES_IMAGE_ENCLAVE_POLICY: EnclavePolicyFlags(u32) = {
    IMAGE_ENCLAVE_POLICY_DEBUGGABLE = 0x0000_0001,
    IMAGE_ENCLAVE_POLICY_STRICT_MEMORY = 0x0000_0002,
});

newtype!(
    struct EnclaveFlags(u32);
);
newtype_flag_names!(NAMES_IMAGE_ENCLAVE_FLAG: EnclaveFlags(u32) = {
    IMAGE_ENCLAVE_FLAG_PRIMARY_IMAGE = 0x0000_0001,
});

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageEnclaveImport {
    pub match_type: U32<LE, EnclaveImportMatchType>,
    pub minimum_security_version: U32<LE>,
    pub unique_or_author_id: [u8; IMAGE_ENCLAVE_LONG_ID_LENGTH],
    pub family_id: [u8; IMAGE_ENCLAVE_SHORT_ID_LENGTH],
    pub image_id: [u8; IMAGE_ENCLAVE_SHORT_ID_LENGTH],
    pub import_name: U32<LE>,
    pub reserved: U32<LE>,
}

newtype!(
    /// Values for `ImageEnclaveImport::match_type`.
    struct EnclaveImportMatchType(u32);
);
newtype_constant_names!(NAMES_ENCLAVE_IMPORT_MATCH: EnclaveImportMatchType(u32) = {
    IMAGE_ENCLAVE_IMPORT_MATCH_NONE = 0x0000_0000,
    IMAGE_ENCLAVE_IMPORT_MATCH_UNIQUE_ID = 0x0000_0001,
    IMAGE_ENCLAVE_IMPORT_MATCH_AUTHOR_ID = 0x0000_0002,
    IMAGE_ENCLAVE_IMPORT_MATCH_FAMILY_ID = 0x0000_0003,
    IMAGE_ENCLAVE_IMPORT_MATCH_IMAGE_ID = 0x0000_0004,
});

//
// Debug Format
//

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageDebugDirectory {
    pub characteristics: U32<LE>,
    pub time_date_stamp: U32<LE>,
    pub major_version: U16<LE>,
    pub minor_version: U16<LE>,
    pub typ: U32<LE, DebugType>,
    pub size_of_data: U32<LE>,
    pub address_of_raw_data: U32<LE>,
    pub pointer_to_raw_data: U32<LE>,
}

newtype!(
    /// Values for `ImageDebugDirectory::typ`.
    struct DebugType(u32);
);

newtype_constant_names!(NAMES_DEBUG_TYPE: DebugType(u32) = {
    IMAGE_DEBUG_TYPE_UNKNOWN = 0,
    IMAGE_DEBUG_TYPE_COFF = 1,
    IMAGE_DEBUG_TYPE_CODEVIEW = 2,
    IMAGE_DEBUG_TYPE_FPO = 3,
    IMAGE_DEBUG_TYPE_MISC = 4,
    IMAGE_DEBUG_TYPE_EXCEPTION = 5,
    IMAGE_DEBUG_TYPE_FIXUP = 6,
    IMAGE_DEBUG_TYPE_OMAP_TO_SRC = 7,
    IMAGE_DEBUG_TYPE_OMAP_FROM_SRC = 8,
    IMAGE_DEBUG_TYPE_BORLAND = 9,
    IMAGE_DEBUG_TYPE_BBT = IMAGE_DEBUG_TYPE_RESERVED10.0,
    IMAGE_DEBUG_TYPE_RESERVED10 = 10,
    IMAGE_DEBUG_TYPE_CLSID = 11,
    IMAGE_DEBUG_TYPE_VC_FEATURE = 12,
    IMAGE_DEBUG_TYPE_POGO = 13,
    IMAGE_DEBUG_TYPE_ILTCG = 14,
    IMAGE_DEBUG_TYPE_MPX = 15,
    IMAGE_DEBUG_TYPE_REPRO = 16,
    IMAGE_DEBUG_TYPE_SPGO = 18,
    IMAGE_DEBUG_TYPE_EX_DLLCHARACTERISTICS = 20,
});

/*
flag_names!(NAMES_DLLCHARACTERISTICS_EX: DllFlagsEx() = {
    IMAGE_DLLCHARACTERISTICS_EX_CET_COMPAT = 0x01,
    IMAGE_DLLCHARACTERISTICS_EX_CET_COMPAT_STRICT_MODE = 0x02,
    IMAGE_DLLCHARACTERISTICS_EX_CET_SET_CONTEXT_IP_VALIDATION_RELAXED_MODE = 0x04,
    IMAGE_DLLCHARACTERISTICS_EX_CET_DYNAMIC_APIS_ALLOW_IN_PROC = 0x08,
    // Reserved for CET policy *downgrade* only!
    IMAGE_DLLCHARACTERISTICS_EX_CET_RESERVED_1 = 0x10,
    // Reserved for CET policy *downgrade* only!
    IMAGE_DLLCHARACTERISTICS_EX_CET_RESERVED_2 = 0x20,
    IMAGE_DLLCHARACTERISTICS_EX_FORWARD_CFI_COMPAT = 0x40,
    IMAGE_DLLCHARACTERISTICS_EX_HOTPATCH_COMPATIBLE = 0x80,
});
*/

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageCoffSymbolsHeader {
    pub number_of_symbols: U32<LE>,
    pub lva_to_first_symbol: U32<LE>,
    pub number_of_linenumbers: U32<LE>,
    pub lva_to_first_linenumber: U32<LE>,
    pub rva_to_first_byte_of_code: U32<LE>,
    pub rva_to_last_byte_of_code: U32<LE>,
    pub rva_to_first_byte_of_data: U32<LE>,
    pub rva_to_last_byte_of_data: U32<LE>,
}

newtype!(
    /// Values for `FpoData::cbFrame`.
    struct FrameType(u8);
);
newtype_constant_names!(NAMES_FRAME: FrameType(u8) = {
    FRAME_FPO = 0,
    FRAME_TRAP = 1,
    FRAME_TSS = 2,
    FRAME_NONFPO = 3,
});

/*
// TODO? bitfields
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct FpoData {
/// offset 1st byte of function code
    pub ul_off_start: U32<LE>,
/// # bytes in function
    pub cb_proc_size: U32<LE>,
/// # bytes in locals/4
    pub cdw_locals: U32<LE>,
/// # bytes in params/4
    pub cdw_params: U16<LE>,
/// # bytes in prolog
    WORD        cbProlog : 8;
/// # regs saved
    WORD        cbRegs   : 3;
/// TRUE if SEH in func
    WORD        fHasSEH  : 1;
/// TRUE if EBP has been allocated
    WORD        fUseBP   : 1;
/// reserved for future use
    WORD        reserved : 1;
/// frame type
    WORD        cbFrame  : 2;
}
pub const SIZEOF_RFPO_DATA: usize = 16;
*/

pub const IMAGE_DEBUG_MISC_EXENAME: u16 = 1;

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageDebugMisc {
    /// type of misc data, see defines
    pub data_type: U32<LE>,
    /// total length of record, rounded to four byte multiple.
    pub length: U32<LE>,
    /// TRUE if data is unicode string
    pub unicode: u8,
    pub reserved: [u8; 3],
    // Actual data
    //pub data: [u8; 1],
}

//
// Function table extracted from MIPS/ALPHA/IA64 images.  Does not contain
// information needed only for runtime support.  Just those fields for
// each entry needed by a debugger.
//

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageFunctionEntry {
    pub starting_address: U32<LE>,
    pub ending_address: U32<LE>,
    pub end_of_prologue: U32<LE>,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageFunctionEntry64 {
    pub starting_address: U64<LE>,
    pub ending_address: U64<LE>,
    pub end_of_prologue_or_unwind_info_address: U64<LE>,
}

//
// Debugging information can be stripped from an image file and placed
// in a separate .DBG file, whose file name part is the same as the
// image file name part (e.g. symbols for CMD.EXE could be stripped
// and placed in CMD.DBG).  This is indicated by the IMAGE_FILE_DEBUG_STRIPPED
// flag in the Characteristics field of the file header.  The beginning of
// the .DBG file contains the following structure which captures certain
// information from the image file.  This allows a debug to proceed even if
// the original image file is not accessible.  This header is followed by
// zero of more IMAGE_SECTION_HEADER structures, followed by zero or more
// IMAGE_DEBUG_DIRECTORY structures.  The latter structures and those in
// the image file contain file offsets relative to the beginning of the
// .DBG file.
//
// If symbols have been stripped from an image, the IMAGE_DEBUG_MISC structure
// is left in the image file, but not mapped.  This allows a debugger to
// compute the name of the .DBG file, from the name of the image in the
// IMAGE_DEBUG_MISC structure.
//

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageSeparateDebugHeader {
    pub signature: U16<LE>,
    pub flags: U16<LE>,
    pub machine: U16<LE, Machine>,
    pub characteristics: U16<LE>,
    pub time_date_stamp: U32<LE>,
    pub check_sum: U32<LE>,
    pub image_base: U32<LE>,
    pub size_of_image: U32<LE>,
    pub number_of_sections: U32<LE>,
    pub exported_names_size: U32<LE>,
    pub debug_directory_size: U32<LE>,
    pub section_alignment: U32<LE>,
    pub reserved: [U32<LE>; 2],
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct NonPagedDebugInfo {
    pub signature: U16<LE>,
    pub flags: U16<LE>,
    pub size: U32<LE>,
    pub machine: U16<LE, Machine>,
    pub characteristics: U16<LE>,
    pub time_date_stamp: U32<LE>,
    pub check_sum: U32<LE>,
    pub size_of_image: U32<LE>,
    pub image_base: U64<LE>,
    //debug_directory_size
    //ImageDebugDirectory
}

pub const IMAGE_SEPARATE_DEBUG_SIGNATURE: u16 = 0x4944;
pub const NON_PAGED_DEBUG_SIGNATURE: u16 = 0x494E;

pub const IMAGE_SEPARATE_DEBUG_FLAGS_MASK: u16 = 0x8000;
/// when DBG was updated, the old checksum didn't match.
pub const IMAGE_SEPARATE_DEBUG_MISMATCH: u16 = 0x8000;

//
//  The .arch section is made up of headers, each describing an amask position/value
//  pointing to an array of IMAGE_ARCHITECTURE_ENTRY's.  Each "array" (both the header
//  and entry arrays) are terminiated by a quadword of 0xffffffffL.
//
//  NOTE: There may be quadwords of 0 sprinkled around and must be skipped.
//

/*
// TODO? bitfields
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageArchitectureHeader {
    /// 1 -> code section depends on mask bit
    /// 0 -> new instruction depends on mask bit
    unsigned int AmaskValue: 1;
    /// MBZ
    int :7;
    /// Amask bit in question for this fixup
    unsigned int AmaskShift: 8;
    /// MBZ
    int :16;
    /// RVA into .arch section to array of ARCHITECTURE_ENTRY's
    pub first_entry_rva: U32<LE>,
}
*/

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageArchitectureEntry {
    /// RVA of instruction to fixup
    pub fixup_inst_rva: U32<LE>,
    /// fixup instruction (see alphaops.h)
    pub new_inst: U32<LE>,
}

// The following structure defines the new import object.  Note the values of the first two fields,
// which must be set as stated in order to differentiate old and new import members.
// Following this structure, the linker emits two null-terminated strings used to recreate the
// import at the time of use.  The first string is the import's name, the second is the dll's name.

pub const IMPORT_OBJECT_HDR_SIG2: u16 = 0xffff;

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImportObjectHeader {
    /// Must be IMAGE_FILE_MACHINE_UNKNOWN
    pub sig1: U16<LE, Machine>,
    /// Must be IMPORT_OBJECT_HDR_SIG2.
    pub sig2: U16<LE>,
    pub version: U16<LE>,
    pub machine: U16<LE, Machine>,
    /// Time/date stamp
    pub time_date_stamp: U32<LE>,
    /// particularly useful for incremental links
    pub size_of_data: U32<LE>,

    /// if grf & IMPORT_OBJECT_ORDINAL
    pub ordinal_or_hint: U16<LE>,

    // WORD    Type : 2;
    // WORD    NameType : 3;
    // WORD    Reserved : 11;
    pub name_type: U16<LE, ImportObjectFlags>,
}

newtype!(
    /// Values for `ImportObjectHeader::name_type`.
    struct ImportObjectFlags(u16);
);

newtype_flag_names!(NAMES_IMPORT_OBJECT: ImportObjectFlags(u16) = {
    _ = IMPORT_OBJECT_TYPE_MASK << IMPORT_OBJECT_TYPE_SHIFT => NAMES_IMPORT_OBJECT_TYPE,
    _ = IMPORT_OBJECT_NAME_MASK << IMPORT_OBJECT_NAME_SHIFT => NAMES_IMPORT_OBJECT_NAME,
});

impl ImportObjectFlags {
    /// Construct the flags from its subfields.
    pub fn new(import_type: ImportObjectType, name_type: ImportObjectNameType) -> Self {
        ImportObjectFlags(
            ImportObjectFlags::from(import_type).0 | ImportObjectFlags::from(name_type).0,
        )
    }

    /// Return the import type field.
    pub fn import_type(self) -> ImportObjectType {
        ImportObjectType(self.0 & IMPORT_OBJECT_TYPE_MASK)
    }

    /// Return the name type field.
    pub fn name_type(self) -> ImportObjectNameType {
        ImportObjectNameType((self.0 >> IMPORT_OBJECT_NAME_SHIFT) & IMPORT_OBJECT_NAME_MASK)
    }
}

newtype!(
    /// Values for the import type subfield of `ImportObjectHeader::name_type`.
    struct ImportObjectType(u16);
);

impl From<ImportObjectType> for ImportObjectFlags {
    fn from(import_type: ImportObjectType) -> Self {
        ImportObjectFlags(import_type.0 & IMPORT_OBJECT_TYPE_MASK)
    }
}

impl From<ImportObjectFlags> for ImportObjectType {
    fn from(value: ImportObjectFlags) -> Self {
        value.import_type()
    }
}

pub const IMPORT_OBJECT_TYPE_MASK: u16 = 0b11;
pub const IMPORT_OBJECT_TYPE_SHIFT: u16 = 0;

newtype_constant_names!(NAMES_IMPORT_OBJECT_TYPE: ImportObjectType(u16) = {
    IMPORT_OBJECT_CODE = 0,
    IMPORT_OBJECT_DATA = 1,
    IMPORT_OBJECT_CONST = 2,
});

newtype!(
    /// Values for the name type subfield of `ImportObjectHeader::name_type`.
    struct ImportObjectNameType(u16);
);

impl From<ImportObjectNameType> for ImportObjectFlags {
    fn from(name_type: ImportObjectNameType) -> Self {
        ImportObjectFlags((name_type.0 & IMPORT_OBJECT_NAME_MASK) << IMPORT_OBJECT_NAME_SHIFT)
    }
}

impl From<ImportObjectFlags> for ImportObjectNameType {
    fn from(value: ImportObjectFlags) -> Self {
        value.name_type()
    }
}

pub const IMPORT_OBJECT_NAME_MASK: u16 = 0b111;
pub const IMPORT_OBJECT_NAME_SHIFT: u16 = 2;

newtype_constant_names!(NAMES_IMPORT_OBJECT_NAME: ImportObjectNameType(u16) = {
    /// Import by ordinal
    IMPORT_OBJECT_ORDINAL = 0,
    /// Import name == public symbol name.
    IMPORT_OBJECT_NAME = 1,
    /// Import name == public symbol name skipping leading ?, @, or optionally _.
    IMPORT_OBJECT_NAME_NO_PREFIX = 2,
    /// Import name == public symbol name skipping leading ?, @, or optionally _ and truncating at first @.
    IMPORT_OBJECT_NAME_UNDECORATE = 3,
    /// Import name == a name is explicitly provided after the DLL name.
    IMPORT_OBJECT_NAME_EXPORTAS = 4,
});

newtype!(
    /// Values for `ImageCor20Header::flags`.
    struct CorFlags(u32);
);
newtype_flag_names!(NAMES_COMIMAGE_FLAGS: CorFlags(u32) = {
    COMIMAGE_FLAGS_ILONLY = 0x0000_0001,
    COMIMAGE_FLAGS_32BITREQUIRED = 0x0000_0002,
    COMIMAGE_FLAGS_IL_LIBRARY = 0x0000_0004,
    COMIMAGE_FLAGS_STRONGNAMESIGNED = 0x0000_0008,
    COMIMAGE_FLAGS_NATIVE_ENTRYPOINT = 0x0000_0010,
    COMIMAGE_FLAGS_TRACKDEBUGDATA = 0x0001_0000,
    COMIMAGE_FLAGS_32BITPREFERRED = 0x0002_0000,
});

// Version flags for image.
pub const COR_VERSION_MAJOR_V2: u16 = 2;
pub const COR_VERSION_MAJOR: u16 = COR_VERSION_MAJOR_V2;
pub const COR_VERSION_MINOR: u16 = 5;
pub const COR_DELETED_NAME_LENGTH: usize = 8;
pub const COR_VTABLEGAP_NAME_LENGTH: usize = 8;

// Maximum size of a NativeType descriptor.
pub const NATIVE_TYPE_MAX_CB: u16 = 1;
pub const COR_ILMETHOD_SECT_SMALL_MAX_DATASIZE: u16 = 0xFF;

// Consts for the MIH FLAGS
pub const IMAGE_COR_MIH_METHODRVA: u16 = 0x01;
pub const IMAGE_COR_MIH_EHRVA: u16 = 0x02;
pub const IMAGE_COR_MIH_BASICBLOCK: u16 = 0x08;

// V-table constants
newtype!(
    struct CorVtableFlags(u16);
);
newtype_flag_names!(NAMES_COR_VTABLE: CorVtableFlags(u16) = {
    /// V-table slots are 32-bits in size.
    COR_VTABLE_32BIT = 0x01,
    /// V-table slots are 64-bits in size.
    COR_VTABLE_64BIT = 0x02,
    /// If set, transition from unmanaged.
    COR_VTABLE_FROM_UNMANAGED = 0x04,
    /// If set, transition from unmanaged with keeping the current appdomain.
    COR_VTABLE_FROM_UNMANAGED_RETAIN_APPDOMAIN = 0x08,
    /// Call most derived method described by
    COR_VTABLE_CALL_MOST_DERIVED = 0x10,
});

// EATJ constants
/// Size of a jump thunk reserved range.
pub const IMAGE_COR_EATJ_THUNK_SIZE: usize = 32;

// Max name lengths
pub const MAX_CLASS_NAME: usize = 1024;
pub const MAX_PACKAGE_NAME: usize = 1024;

// CLR 2.0 header structure.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ImageCor20Header {
    // Header versioning
    pub cb: U32<LE>,
    pub major_runtime_version: U16<LE>,
    pub minor_runtime_version: U16<LE>,

    // Symbol table and startup information
    pub meta_data: ImageDataDirectory,
    pub flags: U32<LE, CorFlags>,

    // If COMIMAGE_FLAGS_NATIVE_ENTRYPOINT is not set, EntryPointToken represents a managed entrypoint.
    // If COMIMAGE_FLAGS_NATIVE_ENTRYPOINT is set, EntryPointRVA represents an RVA to a native entrypoint.
    pub entry_point_token_or_rva: U32<LE>,

    // Binding information
    pub resources: ImageDataDirectory,
    pub strong_name_signature: ImageDataDirectory,

    // Regular fixup and binding information
    pub code_manager_table: ImageDataDirectory,
    pub vtable_fixups: ImageDataDirectory,
    pub export_address_table_jumps: ImageDataDirectory,

    // Precompiled image info (internal use only - set to zero)
    pub managed_native_header: ImageDataDirectory,
}

unsafe_impl_pod!(
    ImageDosHeader,
    ImageOs2Header,
    ImageVxdHeader,
    ImageFileHeader,
    ImageDataDirectory,
    ImageOptionalHeader32,
    ImageRomOptionalHeader,
    ImageOptionalHeader64,
    ImageNtHeaders64,
    ImageNtHeaders32,
    ImageRomHeaders,
    Guid,
    AnonObjectHeader,
    AnonObjectHeaderV2,
    AnonObjectHeaderBigobj,
    ImageSectionHeader,
    ImageSymbol,
    ImageSymbolBytes,
    ImageSymbolEx,
    ImageSymbolExBytes,
    ImageAuxSymbolTokenDef,
    ImageAuxSymbolFunction,
    ImageAuxSymbolFunctionBeginEnd,
    ImageAuxSymbolWeak,
    ImageAuxSymbolSection,
    ImageAuxSymbolCrc,
    ImageRelocation,
    ImageLinenumber,
    ImageBaseRelocation,
    ImageArchiveMemberHeader,
    ImageExportDirectory,
    ImageImportByName,
    ImageThunkData64,
    ImageThunkData32,
    ImageTlsDirectory64,
    ImageTlsDirectory32,
    ImageImportDescriptor,
    ImageBoundImportDescriptor,
    ImageBoundForwarderRef,
    ImageDelayloadDescriptor,
    ImageResourceDirectory,
    ImageResourceDirectoryEntry,
    ImageResourceDirectoryString,
    ImageResourceDirStringU,
    ImageResourceDataEntry,
    ImageLoadConfigCodeIntegrity,
    ImageDynamicRelocationTable,
    ImageDynamicRelocation32,
    ImageDynamicRelocation64,
    ImageDynamicRelocation32V2,
    ImageDynamicRelocation64V2,
    ImagePrologueDynamicRelocationHeader,
    ImageEpilogueDynamicRelocationHeader,
    //ImageImportControlTransferDynamicRelocation,
    //ImageImportControlTransferArm64Relocation,
    //ImageIndirControlTransferDynamicRelocation,
    //ImageSwitchtableBranchDynamicRelocation,
    //ImageFunctionOverrideHeader,
    //ImageFunctionOverrideDynamicRelocation,
    //ImageBddInfo,
    //ImageBddDynamicRelocation,
    ImageLoadConfigDirectory32,
    ImageLoadConfigDirectory64,
    ImageHotPatchInfo,
    ImageHotPatchBase,
    //ImageHotPatchMachine,
    ImageHotPatchHashes,
    //ImageHotswapEndpointInfoHeaderCommon,
    //ImageHotswapEndpointInfoEntryCommon,
    //ImageHotswapX64EndpointInfoEntryV2,
    //ImageHotswapArm64EndpointInfoEntryV2,
    //ImageHotswapEndpointInfoHeaderV2,
    //ImageCeRuntimeFunctionEntry,
    ImageArmRuntimeFunctionEntry,
    ImageArm64RuntimeFunctionEntry,
    //ImageArm64RuntimeFunctionEntryXdata,
    //ImageArm64RuntimeFunctionEntryXdataExtended,
    //ImageArm64RuntimeFunctionEntryXdataEpilogScope,
    ImageAlpha64RuntimeFunctionEntry,
    ImageAlphaRuntimeFunctionEntry,
    ImageRuntimeFunctionEntry,
    ImageEnclaveConfig32,
    ImageEnclaveConfig64,
    ImageEnclaveImport,
    ImageDebugDirectory,
    ImageCoffSymbolsHeader,
    //FpoData,
    ImageDebugMisc,
    ImageFunctionEntry,
    ImageFunctionEntry64,
    ImageSeparateDebugHeader,
    NonPagedDebugInfo,
    //ImageArchitectureHeader,
    ImageArchitectureEntry,
    ImportObjectHeader,
    ImageCor20Header,
    MaskedRichHeaderEntry,
);
