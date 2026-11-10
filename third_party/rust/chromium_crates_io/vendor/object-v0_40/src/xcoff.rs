//! XCOFF definitions
//!
//! These definitions are independent of read/write support, although we do implement
//! some traits useful for those.
//!
//! This module is the equivalent of /usr/include/xcoff.h, and is based heavily on it.

#![allow(missing_docs)]

#[cfg(feature = "names")]
use crate::constants::{ConstantNames, FlagNames};
use crate::endian::{BigEndian as BE, I16, U16, U32, U64};
use crate::pod::Pod;

/// The header at the start of every 32-bit XCOFF file.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct FileHeader32 {
    /// Magic number. Must be 0x01DF.
    pub f_magic: U16<BE>,
    /// Number of sections.
    pub f_nscns: U16<BE>,
    /// Time and date of file creation.
    pub f_timdat: U32<BE>,
    /// Byte offset to symbol table start.
    pub f_symptr: U32<BE>,
    /// Number of entries in symbol table.
    pub f_nsyms: U32<BE>,
    /// Number of bytes in optional header
    pub f_opthdr: U16<BE>,
    /// Extra flags.
    pub f_flags: U16<BE, FileFlags>,
}

/// The header at the start of every 64-bit XCOFF file.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct FileHeader64 {
    /// Magic number. Must be 0x01F7.
    pub f_magic: U16<BE>,
    /// Number of sections.
    pub f_nscns: U16<BE>,
    /// Time and date of file creation
    pub f_timdat: U32<BE>,
    /// Byte offset to symbol table start.
    pub f_symptr: U64<BE>,
    /// Number of bytes in optional header
    pub f_opthdr: U16<BE>,
    /// Extra flags.
    pub f_flags: U16<BE, FileFlags>,
    /// Number of entries in symbol table.
    pub f_nsyms: U32<BE>,
}

// Values for `f_magic`.
//
/// the 64-bit mach magic number
pub const MAGIC_64: u16 = 0x01F7;
/// the 32-bit mach magic number
pub const MAGIC_32: u16 = 0x01DF;

newtype!(
    /// Values for `FileHeader*::f_flags`.
    struct FileFlags(u16);
);

newtype_flag_names!(NAMES_F: FileFlags(u16) = {
    /// Indicates that the relocation information for binding has been removed from
    /// the file.
    F_RELFLG = 0x0001,
    /// Indicates that the file is executable. No unresolved external references exist.
    F_EXEC = 0x0002,
    /// Indicates that line numbers have been stripped from the file by a utility program.
    F_LNNO = 0x0004,
    /// Indicates that the file was profiled with the fdpr command.
    F_FDPR_PROF = 0x0010,
    /// Indicates that the file was reordered with the fdpr command.
    F_FDPR_OPTI = 0x0020,
    /// Indicates that the file uses Very Large Program Support.
    F_DSA = 0x0040,
    /// Indicates that one of the members of the auxiliary header specifying the
    /// medium page sizes is non-zero.
    F_VARPG = 0x0100,
    /// Indicates the file is dynamically loadable and executable. External references
    /// are resolved by way of imports, and the file might contain exports and loader
    /// relocation.
    F_DYNLOAD = 0x1000,
    /// Indicates the file is a shared object (shared library). The file is separately
    /// loadable. That is, it is not normally bound with other objects, and its loader
    /// exports symbols are used as automatic import symbols for other object files.
    F_SHROBJ = 0x2000,
    /// If the object file is a member of an archive, it can be loaded by the system
    /// loader, but the member is ignored by the binder. If the object file is not in
    /// an archive, this flag has no effect.
    F_LOADONLY = 0x4000,
});

/// The auxiliary header immediately following file header. If the value of the
/// f_opthdr field in the file header is 0, the auxiliary header does not exist.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct AuxHeader32 {
    /// Flags.
    pub o_mflag: U16<BE>,
    /// Version.
    pub o_vstamp: U16<BE>,
    /// Text size in bytes.
    pub o_tsize: U32<BE>,
    /// Initialized data size in bytes.
    pub o_dsize: U32<BE>,
    /// Uninitialized data size in bytes.
    pub o_bsize: U32<BE>,
    /// Entry point descriptor (virtual address).
    pub o_entry: U32<BE>,
    /// Base address of text (virtual address).
    pub o_text_start: U32<BE>,
    /// Base address of data (virtual address).
    pub o_data_start: U32<BE>,
    /// Address of TOC anchor.
    pub o_toc: U32<BE>,
    /// Section number for entry point.
    pub o_snentry: U16<BE>,
    /// Section number for .text.
    pub o_sntext: U16<BE>,
    /// Section number for .data.
    pub o_sndata: U16<BE>,
    /// Section number for TOC.
    pub o_sntoc: U16<BE>,
    /// Section number for loader data.
    pub o_snloader: U16<BE>,
    /// Section number for .bss.
    pub o_snbss: U16<BE>,
    /// Maximum alignment for .text.
    pub o_algntext: U16<BE>,
    /// Maximum alignment for .data.
    pub o_algndata: U16<BE>,
    /// Module type field.
    pub o_modtype: U16<BE>,
    /// Bit flags - cpu types of objects.
    pub o_cpuflag: u8,
    /// Reserved for CPU type.
    pub o_cputype: u8,
    /// Maximum stack size allowed (bytes).
    pub o_maxstack: U32<BE>,
    /// Maximum data size allowed (bytes).
    pub o_maxdata: U32<BE>,
    /// Reserved for debuggers.
    pub o_debugger: U32<BE>,
    /// Requested text page size.
    pub o_textpsize: u8,
    /// Requested data page size.
    pub o_datapsize: u8,
    /// Requested stack page size.
    pub o_stackpsize: u8,
    /// Flags and thread-local storage alignment.
    pub o_flags: u8,
    /// Section number for .tdata.
    pub o_sntdata: U16<BE>,
    /// Section number for .tbss.
    pub o_sntbss: U16<BE>,
}

/// The auxiliary header immediately following file header. If the value of the
/// f_opthdr field in the file header is 0, the auxiliary header does not exist.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct AuxHeader64 {
    /// Flags.
    pub o_mflag: U16<BE>,
    /// Version.
    pub o_vstamp: U16<BE>,
    /// Reserved for debuggers.
    pub o_debugger: U32<BE>,
    /// Base address of text (virtual address).
    pub o_text_start: U64<BE>,
    /// Base address of data (virtual address).
    pub o_data_start: U64<BE>,
    /// Address of TOC anchor.
    pub o_toc: U64<BE>,
    /// Section number for entry point.
    pub o_snentry: U16<BE>,
    /// Section number for .text.
    pub o_sntext: U16<BE>,
    /// Section number for .data.
    pub o_sndata: U16<BE>,
    /// Section number for TOC.
    pub o_sntoc: U16<BE>,
    /// Section number for loader data.
    pub o_snloader: U16<BE>,
    /// Section number for .bss.
    pub o_snbss: U16<BE>,
    /// Maximum alignment for .text.
    pub o_algntext: U16<BE>,
    /// Maximum alignment for .data.
    pub o_algndata: U16<BE>,
    /// Module type field.
    pub o_modtype: U16<BE>,
    /// Bit flags - cpu types of objects.
    pub o_cpuflag: u8,
    /// Reserved for CPU type.
    pub o_cputype: u8,
    /// Requested text page size.
    pub o_textpsize: u8,
    /// Requested data page size.
    pub o_datapsize: u8,
    /// Requested stack page size.
    pub o_stackpsize: u8,
    /// Flags and thread-local storage alignment.
    pub o_flags: u8,
    /// Text size in bytes.
    pub o_tsize: U64<BE>,
    /// Initialized data size in bytes.
    pub o_dsize: U64<BE>,
    /// Uninitialized data size in bytes.
    pub o_bsize: U64<BE>,
    /// Entry point descriptor (virtual address).
    pub o_entry: U64<BE>,
    /// Maximum stack size allowed (bytes).
    pub o_maxstack: U64<BE>,
    /// Maximum data size allowed (bytes).
    pub o_maxdata: U64<BE>,
    /// Section number for .tdata.
    pub o_sntdata: U16<BE>,
    /// Section number for .tbss.
    pub o_sntbss: U16<BE>,
    /// XCOFF64 flags.
    pub o_x64flags: U16<BE>,
    /// Reserved.
    pub o_resv3a: U16<BE>,
    /// Reserved.
    pub o_resv3: [U32<BE>; 2],
}

/// Some AIX programs generate auxiliary headers for 32-bit object files that
/// end after the data_start field.
pub const AOUTHSZ_SHORT: u16 = 28;

/// Section header.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct SectionHeader32 {
    /// Section name.
    pub s_name: [u8; 8],
    /// Physical address.
    pub s_paddr: U32<BE>,
    /// Virtual address (same as physical address).
    pub s_vaddr: U32<BE>,
    /// Section size.
    pub s_size: U32<BE>,
    /// Offset in file to raw data for section.
    pub s_scnptr: U32<BE>,
    /// Offset in file to relocation entries for section.
    pub s_relptr: U32<BE>,
    /// Offset in file to line number entries for section.
    pub s_lnnoptr: U32<BE>,
    /// Number of relocation entries.
    pub s_nreloc: U16<BE>,
    /// Number of line number entries.
    pub s_nlnno: U16<BE>,
    /// Flags to define the section type.
    pub s_flags: U32<BE, SectionFlags>,
}

/// Section header.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct SectionHeader64 {
    /// Section name.
    pub s_name: [u8; 8],
    /// Physical address.
    pub s_paddr: U64<BE>,
    /// Virtual address (same as physical address).
    pub s_vaddr: U64<BE>,
    /// Section size.
    pub s_size: U64<BE>,
    /// Offset in file to raw data for section.
    pub s_scnptr: U64<BE>,
    /// Offset in file to relocation entries for section.
    pub s_relptr: U64<BE>,
    /// Offset in file to line number entries for section.
    pub s_lnnoptr: U64<BE>,
    /// Number of relocation entries.
    pub s_nreloc: U32<BE>,
    /// Number of line number entries.
    pub s_nlnno: U32<BE>,
    /// Flags to define the section type.
    pub s_flags: U32<BE, SectionFlags>,
    /// Reserved.
    pub s_reserve: U32<BE>,
}

newtype!(
    /// Values for `SectionHeader*::s_flags`.
    struct SectionFlags(u32);
);

const SFLAGS_TYPE_MASK: u32 = 0x0000_ffff;
const SFLAGS_SUBTYPE_MASK: u32 = 0xffff_0000;

impl core::fmt::Debug for SectionFlags {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        self.typ().fmt(f)?;
        let subtype = self.subtype();
        if subtype.0 != 0 {
            #[cfg(feature = "names")]
            if self.typ() == STYP_DWARF {
                if let Some(name) = SectionFlags::NAMES_DWARF.name(subtype) {
                    return write!(f, " | {}", name);
                }
            }
            write!(f, " | {:x}", subtype.0)?;
        }
        Ok(())
    }
}

impl SectionFlags {
    /// Values for the DWARF subtype field of `SectionHeader*::s_flags`.
    #[cfg(feature = "names")]
    pub const NAMES_DWARF: &ConstantNames<SectionFlags> = &NAMES_SSUBTYP_DWARF;

    /// Get the type field.
    pub fn typ(self) -> SectionType {
        SectionType(self.0 as u16)
    }

    /// Set the type field.
    pub fn with_type(self, typ: SectionType) -> SectionFlags {
        SectionFlags(self.0 & !SFLAGS_TYPE_MASK | u32::from(typ.0))
    }

    /// Get the subtype field.
    pub fn subtype(self) -> SectionFlags {
        SectionFlags(self.0 & SFLAGS_SUBTYPE_MASK)
    }

    /// Set the subtype field.
    pub fn with_subtype(self, subtype: SectionFlags) -> SectionFlags {
        SectionFlags(self.0 & !SFLAGS_SUBTYPE_MASK | subtype.0 & SFLAGS_SUBTYPE_MASK)
    }
}

impl From<SectionType> for SectionFlags {
    fn from(value: SectionType) -> Self {
        SectionFlags(u32::from(value.0))
    }
}

impl core::ops::BitOr<SectionFlags> for SectionType {
    type Output = SectionFlags;
    fn bitor(self, subtype: SectionFlags) -> SectionFlags {
        subtype.with_type(self)
    }
}

newtype!(
    /// Values for the type field of `SectionHeader*::s_flags`.
    ///
    /// There is a bit flag for each type, but only one bit will be set.
    struct SectionType(u16);
);

/// "regular" section
///
/// Unlike other `STYP_*` constants, this is not a bit flag.
pub const STYP_REG: SectionType = SectionType(0x00);

newtype_flag_names!(NAMES_STYP: SectionType(u16) = {
    /// Specifies a pad section. A section of this type is used to provide alignment
    /// padding between sections within an XCOFF executable object file. This section
    /// header type is obsolete since padding is allowed in an XCOFF file without a
    /// corresponding pad section header.
    STYP_PAD = 0x08,
    /// Specifies a DWARF debugging section, which provide source file and symbol
    /// information for the symbolic debugger.
    STYP_DWARF = 0x10,
    /// Specifies an executable text (code) section. A section of this type contains
    /// the executable instructions of a program.
    STYP_TEXT = 0x20,
    /// Specifies an initialized data section. A section of this type contains the
    /// initialized data and the TOC of a program.
    STYP_DATA = 0x40,
    /// Specifies an uninitialized data section. A section header of this type
    /// defines the uninitialized data of a program.
    STYP_BSS = 0x80,
    /// Specifies an exception section. A section of this type provides information
    /// to identify the reason that a trap or exception occurred within an executable
    /// object program.
    STYP_EXCEPT = 0x0100,
    /// Specifies a comment section. A section of this type provides comments or data
    /// to special processing utility programs.
    STYP_INFO = 0x0200,
    /// Specifies an initialized thread-local data section.
    STYP_TDATA = 0x0400,
    /// Specifies an uninitialized thread-local data section.
    STYP_TBSS = 0x0800,
    /// Specifies a loader section. A section of this type contains object file
    /// information for the system loader to load an XCOFF executable. The information
    /// includes imported symbols, exported symbols, relocation data, type-check
    /// information, and shared object names.
    STYP_LOADER = 0x1000,
    /// Specifies a debug section. A section of this type contains stabstring
    /// information used by the symbolic debugger.
    STYP_DEBUG = 0x2000,
    /// Specifies a type-check section. A section of this type contains
    /// parameter/argument type-check strings used by the binder.
    STYP_TYPCHK = 0x4000,
    /// Specifies a relocation or line-number field overflow section. A section
    /// header of this type contains the count of relocation entries and line
    /// number entries for some other section. This section header is required
    /// when either of the counts exceeds 65,534.
    STYP_OVRFLO = 0x8000,
});

constant_names!(NAMES_SSUBTYP_DWARF: SectionFlags(u32) = {
    SSUBTYP_DWINFO = 0x10000,
    SSUBTYP_DWLINE = 0x20000,
    SSUBTYP_DWPBNMS = 0x30000,
    SSUBTYP_DWPBTYP = 0x40000,
    SSUBTYP_DWARNGE = 0x50000,
    SSUBTYP_DWABREV = 0x60000,
    SSUBTYP_DWSTR = 0x70000,
    SSUBTYP_DWRNGES = 0x80000,
    SSUBTYP_DWLOC = 0x90000,
    SSUBTYP_DWFRAME = 0xA0000,
    SSUBTYP_DWMAC = 0xB0000,
});

pub const SIZEOF_SYMBOL: usize = 18;

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct SymbolBytes(pub [u8; SIZEOF_SYMBOL]);

/// Symbol table entry.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Symbol32 {
    /// Symbol name.
    ///
    /// If first 4 bytes are 0, then second 4 bytes are offset into string table.
    pub n_name: [u8; 8],
    /// Symbol value; storage class-dependent.
    pub n_value: U32<BE>,
    /// Section number of symbol.
    pub n_scnum: I16<BE, SymbolSection>,
    /// Basic and derived type specification.
    pub n_type: U16<BE, SymbolType>,
    /// Storage class of symbol.
    pub n_sclass: SymbolClass,
    /// Number of auxiliary entries.
    pub n_numaux: u8,
}

/// Symbol table entry.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Symbol64 {
    /// Symbol value; storage class-dependent.
    pub n_value: U64<BE>,
    /// Offset of the name in string table or .debug section.
    pub n_offset: U32<BE>,
    /// Section number of symbol.
    pub n_scnum: I16<BE, SymbolSection>,
    /// Basic and derived type specification.
    pub n_type: U16<BE, SymbolType>,
    /// Storage class of symbol.
    pub n_sclass: SymbolClass,
    /// Number of auxiliary entries.
    pub n_numaux: u8,
}

newtype!(
    /// Values for `Symbol*::n_scnum`.
    struct SymbolSection(i16);
);

impl SymbolSection {
    /// Return true if this is a reserved value.
    pub fn is_reserved(self) -> bool {
        self.0 <= 0
    }

    /// Return the 1-based section index, or `None` if this is a reserved value.
    pub fn index(self) -> Option<u16> {
        if self.0 > 0 {
            Some(self.0 as u16)
        } else {
            None
        }
    }
}

newtype_constant_names!(NAMES_N: SymbolSection(i16) = {
    /// A special symbolic debugging symbol.
    N_DEBUG = -2,
    /// An absolute symbol. The symbol has a value but is not relocatable.
    N_ABS = -1,
    /// An undefined external symbol.
    N_UNDEF = 0,
});

newtype!(
    /// Values for `Symbol*::n_type`.
    struct SymbolType(u16);
);

// Names depend on storage class.
newtype_flag_names!(SymbolType(u16) = {});

flag_names!(NAMES_SYM_T_EXT: SymbolType(u16) = {
    _ = SYM_V_MASK => NAMES_SYM_V,
    SYM_DT_FCN = DT_FCN << N_BTSHFT,
});

const DT_FCN: u16 = 2;
const N_BTSHFT: usize = 4;

impl SymbolType {
    /// Value names for C_EXT, C_HIDEXT, and C_WEAKEXT symbols.
    #[cfg(feature = "names")]
    pub const NAMES_EXT: &FlagNames<SymbolType> = &NAMES_SYM_T_EXT;

    /// Get the visibility field.
    ///
    /// Valid for C_EXT, C_HIDEXT, and C_WEAKEXT symbols.
    /// Valid in XCOFF32 only when `o_vstamp` is greater than 1.
    pub fn visibility(self) -> SymbolVisibility {
        SymbolVisibility(self.0 & SYM_V_MASK)
    }

    /// Whether the symbol has `SYM_DT_FCN` set.
    ///
    /// Valid only for C_EXT, C_HIDEXT, and C_WEAKEXT symbols in XCOFF32 when
    /// `o_vstamp` is 0 or 1.
    pub fn is_function(self) -> bool {
        self.contains(SYM_DT_FCN)
    }
}

newtype!(
    /// Values for the visibility field of `Symbol*::n_type`.
    ///
    /// Valid for 32-bit XCOFF only when the o_vstamp in the auxiliary header is greater than 1.
    struct SymbolVisibility(u16);
);

pub const SYM_V_MASK: u16 = 0xF000;

newtype_constant_names!(NAMES_SYM_V: SymbolVisibility(u16) = {
    SYM_V_INTERNAL = 0x1000,
    SYM_V_HIDDEN = 0x2000,
    SYM_V_PROTECTED = 0x3000,
    SYM_V_EXPORTED = 0x4000,
});

impl From<SymbolVisibility> for SymbolType {
    fn from(value: SymbolVisibility) -> Self {
        SymbolType(value.0 & SYM_V_MASK)
    }
}

impl From<SymbolType> for SymbolVisibility {
    fn from(value: SymbolType) -> Self {
        value.visibility()
    }
}

newtype!(
    /// Storage classes used for symbolic debugging symbols.
    ///
    /// Values for `Symbol*::n_sclass`.
    #[repr(transparent)]
    struct SymbolClass(u8);
);

newtype_constant_names!(NAMES_C: SymbolClass(u8) = {
    /// Source file name and compiler information.
    C_FILE = 103,
    /// Beginning of include file.
    C_BINCL = 108,
    /// Ending of include file.
    C_EINCL = 109,
    /// Global variable.
    C_GSYM = 128,
    /// Statically allocated symbol.
    C_STSYM = 133,
    /// Beginning of common block.
    C_BCOMM = 135,
    /// End of common block.
    C_ECOMM = 137,
    /// Alternate entry.
    C_ENTRY = 141,
    /// Beginning of static block.
    C_BSTAT = 143,
    /// End of static block.
    C_ESTAT = 144,
    /// Global thread-local variable.
    C_GTLS = 145,
    /// Static thread-local variable.
    C_STTLS = 146,
    /// DWARF section symbol.
    C_DWARF = 112,
    //
    // Storage classes used for absolute symbols.
    //
    /// Automatic variable allocated on stack.
    C_LSYM = 129,
    /// Argument to subroutine allocated on stack.
    C_PSYM = 130,
    /// Register variable.
    C_RSYM = 131,
    /// Argument to function or procedure stored in register.
    C_RPSYM = 132,
    /// Local member of common block.
    C_ECOML = 136,
    /// Function or procedure.
    C_FUN = 142,
    //
    // Storage classes used for undefined external symbols or symbols of general sections.
    //
    /// External symbol.
    C_EXT = 2,
    /// Weak external symbol.
    C_WEAKEXT = 111,
    //
    // Storage classes used for symbols of general sections.
    //
    /// Symbol table entry marked for deletion.
    C_NULL = 0,
    /// Static.
    C_STAT = 3,
    /// Beginning or end of inner block.
    C_BLOCK = 100,
    /// Beginning or end of function.
    C_FCN = 101,
    /// Un-named external symbol.
    C_HIDEXT = 107,
    /// Comment string in .info section.
    C_INFO = 110,
    /// Declaration of object (type).
    C_DECL = 140,
    //
    // Storage classes - Obsolete/Undocumented.
    //
    /// Automatic variable.
    C_AUTO = 1,
    /// Register variable.
    C_REG = 4,
    /// External definition.
    C_EXTDEF = 5,
    /// Label.
    C_LABEL = 6,
    /// Undefined label.
    C_ULABEL = 7,
    /// Member of structure.
    C_MOS = 8,
    /// Function argument.
    C_ARG = 9,
    /// Structure tag.
    C_STRTAG = 10,
    /// Member of union.
    C_MOU = 11,
    /// Union tag.
    C_UNTAG = 12,
    /// Type definition.
    C_TPDEF = 13,
    /// Undefined static.
    C_USTATIC = 14,
    /// Enumeration tag.
    C_ENTAG = 15,
    /// Member of enumeration.
    C_MOE = 16,
    /// Register parameter.
    C_REGPARM = 17,
    /// Bit field.
    C_FIELD = 18,
    /// End of structure.
    C_EOS = 102,
    /// Duplicate tag.
    C_ALIAS = 105,
    /// Special storage class for external.
    C_HIDDEN = 106,
    /// Physical end of function.
    C_EFCN = 255,
    /// Reserved.
    C_TCSYM = 134,
});

/// File Auxiliary Entry for C_FILE Symbols.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct FileAux32 {
    /// The source file name or compiler-related string.
    ///
    /// If first 4 bytes are 0, then second 4 bytes are offset into string table.
    pub x_fname: [u8; 8],
    /// Pad size for file name.
    pub x_fpad: [u8; 6],
    /// The source-file string type.
    pub x_ftype: FileAuxType,
    /// Reserved.
    pub x_freserve: [u8; 3],
}

/// File Auxiliary Entry for C_FILE Symbols.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct FileAux64 {
    /// The source file name or compiler-related string.
    ///
    /// If first 4 bytes are 0, then second 4 bytes are offset into string table.
    pub x_fname: [u8; 8],
    /// Pad size for file name.
    pub x_fpad: [u8; 6],
    /// The source-file string type.
    pub x_ftype: FileAuxType,
    /// Reserved.
    pub x_freserve: [u8; 2],
    /// Specifies the type of auxiliary entry. Contains `AUX_FILE` for this auxiliary entry.
    pub x_auxtype: AuxType,
}

newtype!(
    /// Values for `FileAux*::x_ftype`.
    #[repr(transparent)]
    struct FileAuxType(u8);
);

newtype_constant_names!(NAMES_XFT: FileAuxType(u8) = {
    /// Specifies the source-file name.
    XFT_FN = 0,
    /// Specifies the compiler time stamp.
    XFT_CT = 1,
    /// Specifies the compiler version number.
    XFT_CV = 2,
    /// Specifies compiler-defined information.
    XFT_CD = 128,
});

/// Csect auxiliary entry for C_EXT, C_WEAKEXT, and C_HIDEXT symbols.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct CsectAux32 {
    /// Section length.
    pub x_scnlen: U32<BE>,
    /// Offset of parameter type-check hash in .typchk section.
    pub x_parmhash: U32<BE>,
    /// .typchk section number.
    pub x_snhash: U16<BE>,
    /// Symbol alignment and type.
    pub x_smtyp: CsectAuxSmtyp,
    /// Storage mapping class.
    pub x_smclas: CsectAuxClass,
    /// Reserved.
    pub x_stab: U32<BE>,
    /// x_snstab.
    pub x_snstab: U16<BE>,
}

/// Csect auxiliary entry for C_EXT, C_WEAKEXT, and C_HIDEXT symbols.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct CsectAux64 {
    /// Low 4 bytes of section length.
    pub x_scnlen_lo: U32<BE>,
    /// Offset of parameter type-check hash in .typchk section.
    pub x_parmhash: U32<BE>,
    /// .typchk section number.
    pub x_snhash: U16<BE>,
    /// Symbol alignment and type.
    pub x_smtyp: CsectAuxSmtyp,
    /// Storage mapping class.
    pub x_smclas: CsectAuxClass,
    /// High 4 bytes of section length.
    pub x_scnlen_hi: U32<BE>,
    /// Reserved.
    pub pad: u8,
    /// Contains `AUX_CSECT`; indicates type of auxiliary entry.
    pub x_auxtype: AuxType,
}

newtype!(
    /// Values for `CsectAux*::x_smtyp`.
    #[repr(transparent)]
    struct CsectAuxSmtyp(u8);
);

impl core::fmt::Debug for CsectAuxSmtyp {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        self.typ().fmt(f)?;
        let alignment = self.alignment();
        if alignment != 0 {
            write!(f, " | {}", alignment)?;
        }
        Ok(())
    }
}

impl CsectAuxSmtyp {
    /// Construct `CsectAuxSmtype` from its subfields.
    pub fn new(typ: CsectAuxType, alignment: u8) -> Self {
        CsectAuxSmtyp(typ.0 & 0x7 | alignment << 3)
    }

    /// Get the alignment field.
    pub fn alignment(self) -> u8 {
        self.0 >> 3
    }

    /// Get the typ field.
    pub fn typ(self) -> CsectAuxType {
        CsectAuxType(self.0 & 0x7)
    }
}

impl From<CsectAuxType> for CsectAuxSmtyp {
    fn from(value: CsectAuxType) -> Self {
        CsectAuxSmtyp(value.0 & 0x7)
    }
}

newtype!(
    /// Values for type field of `CsectAux*::x_smtyp`.
    struct CsectAuxType(u8);
);

newtype_constant_names!(NAMES_XTY: CsectAuxType(u8) = {
    /// External reference.
    XTY_ER = 0,
    /// Csect definition for initialized storage.
    XTY_SD = 1,
    /// Defines an entry point to an initialized csect.
    XTY_LD = 2,
    /// Common csect definition. For uninitialized storage.
    XTY_CM = 3,
});

newtype!(
    /// Values for `CsectAux*::x_smclas`.
    #[repr(transparent)]
    struct CsectAuxClass(u8);
);

newtype_constant_names!(NAMES_XMC: CsectAuxClass(u8) = {
    //
    // READ ONLY CLASSES
    //
    /// Program Code
    XMC_PR = 0,
    /// Read Only Constant
    XMC_RO = 1,
    /// Debug Dictionary Table
    XMC_DB = 2,
    /// Global Linkage (Interfile Interface Code)
    XMC_GL = 6,
    /// Extended Operation (Pseudo Machine Instruction)
    XMC_XO = 7,
    /// Supervisor Call (32-bit process only)
    XMC_SV = 8,
    /// Supervisor Call for 64-bit process
    XMC_SV64 = 17,
    /// Supervisor Call for both 32- and 64-bit processes
    XMC_SV3264 = 18,
    /// Traceback Index csect
    XMC_TI = 12,
    /// Traceback Table csect
    XMC_TB = 13,
    //
    // READ WRITE CLASSES
    //
    /// Read Write Data
    XMC_RW = 5,
    /// TOC Anchor for TOC Addressability
    XMC_TC0 = 15,
    /// General TOC item
    XMC_TC = 3,
    /// Scalar data item in the TOC
    XMC_TD = 16,
    /// Descriptor csect
    XMC_DS = 10,
    /// Unclassified - Treated as Read Write
    XMC_UA = 4,
    /// BSS class (uninitialized static internal)
    XMC_BS = 9,
    /// Un-named Fortran Common
    XMC_UC = 11,
    /// Initialized thread-local variable
    XMC_TL = 20,
    /// Uninitialized thread-local variable
    XMC_UL = 21,
    /// Symbol mapped at the end of TOC
    XMC_TE = 22,
});

/// Function auxiliary entry.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct FunAux32 {
    /// File offset to exception table entry.
    pub x_exptr: U32<BE>,
    /// Size of function in bytes.
    pub x_fsize: U32<BE>,
    /// File pointer to line number
    pub x_lnnoptr: U32<BE>,
    /// Symbol table index of next entry beyond this function.
    pub x_endndx: U32<BE>,
    /// Pad
    pub pad: U16<BE>,
}

/// Function auxiliary entry.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct FunAux64 {
    /// File pointer to line number
    pub x_lnnoptr: U64<BE>,
    /// Size of function in bytes.
    pub x_fsize: U32<BE>,
    /// Symbol table index of next entry beyond this function.
    pub x_endndx: U32<BE>,
    /// Pad
    pub pad: u8,
    /// Contains `AUX_FCN`; Type of auxiliary entry.
    pub x_auxtype: AuxType,
}

/// Exception auxiliary entry. (XCOFF64 only)
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ExpAux {
    /// File offset to exception table entry.
    pub x_exptr: U64<BE>,
    /// Size of function in bytes.
    pub x_fsize: U32<BE>,
    /// Symbol table index of next entry beyond this function.
    pub x_endndx: U32<BE>,
    /// Pad
    pub pad: u8,
    /// Contains `AUX_EXCEPT`; Type of auxiliary entry
    pub x_auxtype: AuxType,
}

/// Block auxiliary entry for the C_BLOCK and C_FCN Symbols.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct BlockAux32 {
    /// Reserved.
    pub pad: [u8; 2],
    /// High-order 2 bytes of the source line number.
    pub x_lnnohi: U16<BE>,
    /// Low-order 2 bytes of the source line number.
    pub x_lnnolo: U16<BE>,
    /// Reserved.
    pub pad2: [u8; 12],
}

/// Block auxiliary entry for the C_BLOCK and C_FCN Symbols.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct BlockAux64 {
    /// Source line number.
    pub x_lnno: U32<BE>,
    /// Reserved.
    pub pad: [u8; 13],
    /// Contains `AUX_SYM`; Type of auxiliary entry.
    pub x_auxtype: AuxType,
}

/// Section auxiliary entry for the C_STAT Symbol. (XCOFF32 Only)
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct StatAux {
    /// Section length.
    pub x_scnlen: U32<BE>,
    /// Number of relocation entries.
    pub x_nreloc: U16<BE>,
    /// Number of line numbers.
    pub x_nlinno: U16<BE>,
    /// Reserved.
    pub pad: [u8; 10],
}

/// Section auxiliary entry Format for C_DWARF symbols.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DwarfAux32 {
    /// Length of portion of section represented by symbol.
    pub x_scnlen: U32<BE>,
    /// Reserved.
    pub pad: [u8; 4],
    /// Number of relocation entries in section.
    pub x_nreloc: U32<BE>,
    /// Reserved.
    pub pad2: [u8; 6],
}

/// Section auxiliary entry Format for C_DWARF symbols.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct DwarfAux64 {
    /// Length of portion of section represented by symbol.
    pub x_scnlen: U64<BE>,
    /// Number of relocation entries in section.
    pub x_nreloc: U64<BE>,
    /// Reserved.
    pub pad: u8,
    /// Contains `AUX_SECT`; Type of Auxiliary entry.
    pub x_auxtype: AuxType,
}

newtype!(
    /// Values for `x_auxtype` field in auxiliary entries.
    #[repr(transparent)]
    struct AuxType(u8);
);

newtype_constant_names!(NAMES_AUX: AuxType(u8) = {
    /// Identifies an exception auxiliary entry.
    AUX_EXCEPT = 255,
    /// Identifies a function auxiliary entry.
    AUX_FCN = 254,
    /// Identifies a symbol auxiliary entry.
    AUX_SYM = 253,
    /// Identifies a file auxiliary entry.
    AUX_FILE = 252,
    /// Identifies a csect auxiliary entry.
    AUX_CSECT = 251,
    /// Identifies a SECT auxiliary entry.
    AUX_SECT = 250,
});

/// Relocation table entry
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Rel32 {
    /// Virtual address (position) in section to be relocated.
    pub r_vaddr: U32<BE>,
    /// Symbol table index of item that is referenced.
    pub r_symndx: U32<BE>,
    /// Relocation size and information.
    pub r_rsize: u8,
    /// Relocation type.
    pub r_rtype: RelocationType,
}

/// Relocation table entry
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Rel64 {
    /// Virtual address (position) in section to be relocated.
    pub r_vaddr: U64<BE>,
    /// Symbol table index of item that is referenced.
    pub r_symndx: U32<BE>,
    /// Relocation size and information.
    pub r_rsize: u8,
    /// Relocation type.
    pub r_rtype: RelocationType,
}

newtype!(
    /// Values for `Rel*::r_rtype`.
    struct RelocationType(u8);
);

newtype_constant_names!(NAMES_REL_TYPE: RelocationType(u8) = {
    /// Positive relocation.
    R_POS = 0x00,
    /// Positive indirect load relocation.
    R_RL = 0x0c,
    /// Positive load address relocation. Modifiable instruction.
    R_RLA = 0x0d,
    /// Negative relocation.
    R_NEG = 0x01,
    /// Relative to self relocation.
    R_REL = 0x02,
    /// Relative to the TOC relocation.
    R_TOC = 0x03,
    /// TOC relative indirect load relocation.
    R_TRL = 0x12,
    /// Relative to the TOC or to the thread-local storage base relocation.
    R_TRLA = 0x13,
    /// Global linkage-external TOC address relocation.
    R_GL = 0x05,
    /// Local object TOC address relocation.
    R_TCL = 0x06,
    /// A non-relocating relocation.
    R_REF = 0x0f,
    /// Branch absolute relocation. References a non-modifiable instruction.
    R_BA = 0x08,
    /// Branch relative to self relocation. References a non-modifiable instruction.
    R_BR = 0x0a,
    /// Branch absolute relocation. References a modifiable instruction.
    R_RBA = 0x18,
    /// Branch relative to self relocation. References a modifiable instruction.
    R_RBR = 0x1a,
    /// General-dynamic reference to TLS symbol.
    R_TLS = 0x20,
    /// Initial-exec reference to TLS symbol.
    R_TLS_IE = 0x21,
    /// Local-dynamic reference to TLS symbol.
    R_TLS_LD = 0x22,
    /// Local-exec reference to TLS symbol.
    R_TLS_LE = 0x23,
    /// Module reference to TLS.
    R_TLSM = 0x24,
    /// Module reference to the local TLS storage.
    R_TLSML = 0x25,
    /// Relative to TOC upper.
    R_TOCU = 0x30,
    /// Relative to TOC lower.
    R_TOCL = 0x31,
});

unsafe_impl_pod!(
    FileHeader32,
    FileHeader64,
    AuxHeader32,
    AuxHeader64,
    SectionHeader32,
    SectionHeader64,
    SymbolBytes,
    Symbol32,
    Symbol64,
    FileAux32,
    FileAux64,
    CsectAux32,
    CsectAux64,
    FunAux32,
    FunAux64,
    ExpAux,
    BlockAux32,
    BlockAux64,
    StatAux,
    DwarfAux32,
    DwarfAux64,
    Rel32,
    Rel64,
);
