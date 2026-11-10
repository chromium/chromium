use core::fmt;
use core::ops::{Deref, DerefMut};

/// A CPU architecture.
#[allow(missing_docs)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum Architecture {
    Unknown,
    Aarch64,
    #[allow(non_camel_case_types)]
    Aarch64_Ilp32,
    Alpha,
    Arm,
    Avr,
    Bpf,
    Csky,
    E2K32,
    E2K64,
    I386,
    X86_64,
    #[allow(non_camel_case_types)]
    X86_64_X32,
    Hexagon,
    Hppa,
    Ia64,
    LoongArch32,
    LoongArch64,
    M68k,
    Mips,
    Mips64,
    #[allow(non_camel_case_types)]
    Mips64_N32,
    Msp430,
    PowerPc,
    PowerPc64,
    Riscv32,
    Riscv64,
    S390x,
    Sbf,
    Sharc,
    Sparc,
    Sparc32Plus,
    Sparc64,
    SuperH,
    Wasm32,
    Wasm64,
    Xtensa,
}

/// A CPU sub-architecture.
#[allow(missing_docs)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum SubArchitecture {
    Arm64E,
    Arm64EC,
}

impl Architecture {
    /// The size of an address value for this architecture.
    ///
    /// Returns `None` for unknown architectures.
    pub fn address_size(self) -> Option<AddressSize> {
        match self {
            Architecture::Unknown => None,
            Architecture::Aarch64 => Some(AddressSize::U64),
            Architecture::Aarch64_Ilp32 => Some(AddressSize::U32),
            Architecture::Alpha => Some(AddressSize::U64),
            Architecture::Arm => Some(AddressSize::U32),
            Architecture::Avr => Some(AddressSize::U8),
            Architecture::Bpf => Some(AddressSize::U64),
            Architecture::Csky => Some(AddressSize::U32),
            Architecture::E2K32 => Some(AddressSize::U32),
            Architecture::E2K64 => Some(AddressSize::U64),
            Architecture::I386 => Some(AddressSize::U32),
            Architecture::X86_64 => Some(AddressSize::U64),
            Architecture::X86_64_X32 => Some(AddressSize::U32),
            Architecture::Hexagon => Some(AddressSize::U32),
            Architecture::Hppa => Some(AddressSize::U32),
            Architecture::Ia64 => Some(AddressSize::U64),
            Architecture::LoongArch32 => Some(AddressSize::U32),
            Architecture::LoongArch64 => Some(AddressSize::U64),
            Architecture::M68k => Some(AddressSize::U32),
            Architecture::Mips => Some(AddressSize::U32),
            Architecture::Mips64 => Some(AddressSize::U64),
            Architecture::Mips64_N32 => Some(AddressSize::U32),
            Architecture::Msp430 => Some(AddressSize::U16),
            Architecture::PowerPc => Some(AddressSize::U32),
            Architecture::PowerPc64 => Some(AddressSize::U64),
            Architecture::Riscv32 => Some(AddressSize::U32),
            Architecture::Riscv64 => Some(AddressSize::U64),
            Architecture::S390x => Some(AddressSize::U64),
            Architecture::Sbf => Some(AddressSize::U64),
            Architecture::Sharc => Some(AddressSize::U32),
            Architecture::Sparc => Some(AddressSize::U32),
            Architecture::Sparc32Plus => Some(AddressSize::U32),
            Architecture::Sparc64 => Some(AddressSize::U64),
            Architecture::Wasm32 => Some(AddressSize::U32),
            Architecture::Wasm64 => Some(AddressSize::U64),
            Architecture::Xtensa => Some(AddressSize::U32),
            Architecture::SuperH => Some(AddressSize::U32),
        }
    }
}

/// The size of an address value for an architecture.
///
/// This may differ from the address size supported by the file format (such as for COFF).
#[allow(missing_docs)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
#[repr(u8)]
pub enum AddressSize {
    U8 = 1,
    U16 = 2,
    U32 = 4,
    U64 = 8,
}

impl AddressSize {
    /// The size in bytes of an address value.
    #[inline]
    pub fn bytes(self) -> u8 {
        self as u8
    }
}

/// A binary file format.
#[allow(missing_docs)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum BinaryFormat {
    Coff,
    Elf,
    MachO,
    Pe,
    Wasm,
    Xcoff,
}

impl BinaryFormat {
    /// The target's native binary format for relocatable object files.
    ///
    /// Defaults to `Elf` for unknown platforms.
    pub fn native_object() -> BinaryFormat {
        if cfg!(target_os = "windows") {
            BinaryFormat::Coff
        } else if cfg!(target_os = "macos") {
            BinaryFormat::MachO
        } else {
            BinaryFormat::Elf
        }
    }
}

/// The kind of a section.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum SectionKind {
    /// The section kind is unknown.
    Unknown,
    /// An executable code section.
    ///
    /// Example ELF sections: `.text`
    ///
    /// Example Mach-O sections: `__TEXT/__text`
    Text,
    /// A data section.
    ///
    /// Example ELF sections: `.data`
    ///
    /// Example Mach-O sections: `__DATA/__data`
    Data,
    /// A read only data section.
    ///
    /// Example ELF sections: `.rodata`
    ///
    /// Example Mach-O sections: `__TEXT/__const`, `__TEXT/__literal4`
    ReadOnlyData,
    /// A read only data section with relocations.
    ///
    /// This is the same as either `Data` or `ReadOnlyData`, depending on the file format.
    /// This value is only used in the API for writing files. It is never returned when reading files.
    ReadOnlyDataWithRel,
    /// A loadable string section.
    ///
    /// Example ELF sections: `.rodata.str`
    ///
    /// Example Mach-O sections: `__TEXT/__cstring`
    ReadOnlyString,
    /// An uninitialized data section.
    ///
    /// Example ELF sections: `.bss`
    ///
    /// Example Mach-O sections: `__DATA/__bss`, `__DATA/__common`
    UninitializedData,
    /// A TLS data section.
    ///
    /// Example ELF sections: `.tdata`
    ///
    /// Example Mach-O sections: `__DATA/__thread_data`
    Tls,
    /// An uninitialized TLS data section.
    ///
    /// Example ELF sections: `.tbss`
    ///
    /// Example Mach-O sections: `__DATA/__thread_bss`
    UninitializedTls,
    /// A TLS variables section.
    ///
    /// This contains TLS variable structures, rather than the variable initializers.
    ///
    /// Example Mach-O sections: `__DATA/__thread_vars`
    TlsVariables,
    /// A non-loadable string section.
    ///
    /// Example ELF sections: `.comment`, `.debug_str`
    OtherString,
    /// Some other non-loadable section.
    ///
    /// Example ELF sections: `.debug_info`
    Other,
    /// Debug information.
    ///
    /// Example Mach-O sections: `__DWARF/__debug_info`
    Debug,
    /// Debug strings.
    ///
    /// This is the same as either `Debug` or `OtherString`, depending on the file format.
    /// This value is only used in the API for writing files. It is never returned when reading files.
    DebugString,
    /// Information for the linker.
    ///
    /// Example COFF sections: `.drectve`
    Linker,
    /// ELF note section.
    Note,
    /// Metadata such as symbols or relocations.
    ///
    /// Example ELF sections: `.symtab`, `.strtab`, `.group`
    Metadata,
}

impl SectionKind {
    /// Return true if this section contains zerofill data.
    pub fn is_bss(self) -> bool {
        self == SectionKind::UninitializedData || self == SectionKind::UninitializedTls
    }
}

/// The selection kind for a COMDAT section group.
///
/// This determines the way in which the linker resolves multiple definitions of the COMDAT
/// sections.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum ComdatKind {
    /// The selection kind is unknown.
    Unknown,
    /// Multiple definitions are allowed.
    ///
    /// An arbitrary definition is selected, and the rest are removed.
    ///
    /// This is the only supported selection kind for ELF.
    Any,
    /// Multiple definitions are not allowed.
    ///
    /// This is used to group sections without allowing duplicates.
    NoDuplicates,
    /// Multiple definitions must have the same size.
    ///
    /// An arbitrary definition is selected, and the rest are removed.
    SameSize,
    /// Multiple definitions must match exactly.
    ///
    /// An arbitrary definition is selected, and the rest are removed.
    ExactMatch,
    /// Multiple definitions are allowed, and the largest is selected.
    ///
    /// An arbitrary definition with the largest size is selected, and the rest are removed.
    Largest,
    /// Multiple definitions are allowed, and the newest is selected.
    Newest,
}

/// The kind of a symbol.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum SymbolKind {
    /// The symbol kind is unknown.
    Unknown,
    /// The symbol is for executable code.
    Text,
    /// The symbol is for a data object.
    Data,
    /// The symbol is for a section.
    Section,
    /// The symbol is the name of a file. It precedes symbols within that file.
    File,
    /// The symbol is for a code label.
    Label,
    /// The symbol is for a thread local storage entity.
    Tls,
}

/// A symbol scope.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum SymbolScope {
    /// Unknown scope.
    Unknown,
    /// Symbol is visible to the compilation unit.
    Compilation,
    /// Symbol is visible to the static linkage unit.
    Linkage,
    /// Symbol is visible to dynamically linked objects.
    Dynamic,
}

/// The operation used to calculate the result of the relocation.
///
/// The relocation descriptions use the following definitions. Note that
/// these definitions probably don't match any ELF ABI.
///
/// * A - The value of the addend.
/// * G - The address of the symbol's entry within the global offset table.
/// * L - The address of the symbol's entry within the procedure linkage table.
/// * P - The address of the place of the relocation.
/// * S - The address of the symbol.
/// * GotBase - The address of the global offset table.
/// * Image - The base address of the image.
/// * Section - The address of the section containing the symbol.
///
/// 'XxxRelative' means 'Xxx + A - P'.  'XxxOffset' means 'S + A - Xxx'.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum RelocationKind {
    /// The operation is unknown.
    Unknown,
    /// No relocation.
    ///
    /// Example: `elf::R_X86_64_NONE`
    None,
    /// S + A
    ///
    /// Example: `elf::R_X86_64_64`
    Absolute,
    /// S + A - P
    ///
    /// Example: `elf::R_X86_64_PC32`
    Relative,
    /// G + A - GotBase
    ///
    /// Example: `elf::R_X86_64_GOT32`
    Got,
    /// G + A - P
    ///
    /// Example: `elf::R_X86_64_GOTPCREL`
    GotRelative,
    /// GotBase + A - P
    ///
    /// Example: `elf::R_386_GOTPC`
    GotBaseRelative,
    /// S + A - GotBase
    ///
    /// Example: `elf::R_386_GOTOFF`
    GotBaseOffset,
    /// L + A - P
    ///
    /// Example: `elf::R_X86_64_PLT32`
    PltRelative,
    /// S + A - Image
    ///
    /// Example: `pe::IMAGE_REL_AMD64_ADDR32NB`
    ImageOffset,
    /// S + A - Section
    ///
    /// Example: `pe::IMAGE_REL_AMD64_SECREL`
    SectionOffset,
    /// The index of the section containing the symbol.
    ///
    /// Example: `pe::IMAGE_REL_AMD64_SECTION`
    SectionIndex,
}

/// Information about how the result of the relocation operation is encoded in the place.
///
/// This is usually architecture specific, such as specifying an addressing mode or
/// a specific instruction.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum RelocationEncoding {
    /// The relocation encoding is unknown.
    Unknown,
    /// Generic encoding.
    Generic,

    /// x86 sign extension at runtime.
    ///
    /// Used with `RelocationKind::Absolute`.
    X86Signed,
    /// x86 rip-relative addressing.
    ///
    /// The `RelocationKind` must be PC relative.
    X86RipRelative,
    /// x86 rip-relative addressing in movq instruction.
    ///
    /// The `RelocationKind` must be PC relative.
    X86RipRelativeMovq,
    /// x86 call target.
    ///
    /// The `RelocationKind` must be PC relative.
    // This name matches Mach-O, but might have been better named `X86Call`.
    X86Branch,

    /// s390x PC-relative offset shifted right by one bit.
    ///
    /// The `RelocationKind` must be PC relative.
    S390xDbl,

    /// AArch64 call target.
    ///
    /// The `RelocationKind` must be PC relative.
    AArch64Call,

    /// LoongArch branch offset with two trailing zeros.
    ///
    /// The `RelocationKind` must be PC relative.
    LoongArchBranch,

    /// SHARC+ 48-bit Type A instruction
    ///
    /// Represents these possible variants, each with a corresponding
    /// `R_SHARC_*` constant:
    ///
    /// * 24-bit absolute address
    /// * 32-bit absolute address
    /// * 6-bit relative address
    /// * 24-bit relative address
    /// * 6-bit absolute address in the immediate value field
    /// * 16-bit absolute address in the immediate value field
    SharcTypeA,

    /// SHARC+ 32-bit Type B instruction
    ///
    /// Represents these possible variants, each with a corresponding
    /// `R_SHARC_*` constant:
    ///
    /// * 6-bit absolute address in the immediate value field
    /// * 7-bit absolute address in the immediate value field
    /// * 16-bit absolute address
    /// * 6-bit relative address
    SharcTypeB,

    /// E2K 64-bit value stored in two LTS
    ///
    /// Memory representation:
    /// ```text
    /// 0: LTS1 = value[63:32]
    /// 4: LTS0 = value[31:0]
    /// ```
    E2KLit,

    /// E2K 28-bit value stored in CS0
    E2KDisp,
}

/// File flags that are specific to each file format.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum FileFlags {
    /// No file flags.
    None,
    /// ELF file flags.
    #[cfg(feature = "elf")]
    Elf {
        /// `os_abi` field in the ELF file header.
        os_abi: crate::elf::OsAbi,
        /// `abi_version` field in the ELF file header.
        abi_version: u8,
        /// `e_flags` field in the ELF file header.
        e_flags: crate::elf::FileFlags,
    },
    /// Mach-O file flags.
    #[cfg(feature = "macho")]
    MachO {
        /// `flags` field in the Mach-O file header.
        flags: crate::macho::FileFlags,
    },
    /// COFF file flags.
    #[cfg(feature = "coff")]
    Coff {
        /// `Characteristics` field in the COFF file header.
        characteristics: crate::pe::FileFlags,
    },
    /// XCOFF file flags.
    #[cfg(feature = "xcoff")]
    Xcoff {
        /// `f_flags` field in the XCOFF file header.
        f_flags: crate::xcoff::FileFlags,
    },
}

/// Segment flags that are specific to each file format.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum SegmentFlags {
    /// No segment flags.
    None,
    /// ELF segment flags.
    #[cfg(feature = "elf")]
    Elf {
        /// `p_type` field in the segment header.
        p_type: crate::elf::ProgramType,
        /// `p_flags` field in the segment header.
        p_flags: crate::elf::ProgramFlags,
    },
    /// Mach-O segment flags.
    #[cfg(feature = "macho")]
    MachO {
        /// `flags` field in the segment header.
        flags: crate::macho::SegmentFlags,
        /// `maxprot` field in the segment header.
        maxprot: crate::macho::VmProt,
        /// `initprot` field in the segment header.
        initprot: crate::macho::VmProt,
    },
    /// COFF segment flags.
    #[cfg(feature = "coff")]
    Coff {
        /// `Characteristics` field in the segment header.
        characteristics: crate::pe::SectionFlags,
    },
}

/// Memory permissions for a segment.
///
/// This is a simplified representation of segment permissions that abstracts
/// over format-specific flags.
#[derive(Clone, Copy, PartialEq, Eq, Hash, Default)]
pub struct Permissions {
    bits: u8,
}

impl core::fmt::Debug for Permissions {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(
            f,
            "{}{}{}",
            if self.readable() { 'R' } else { '-' },
            if self.writable() { 'W' } else { '-' },
            if self.executable() { 'X' } else { '-' },
        )
    }
}

impl Permissions {
    /// Permission bit for readable.
    const R: u8 = 1 << 0;
    /// Permission bit for writable.
    const W: u8 = 1 << 1;
    /// Permission bit for executable.
    const X: u8 = 1 << 2;

    /// Creates a new `Permissions` with the given flags.
    pub fn new(readable: bool, writable: bool, executable: bool) -> Self {
        let mut bits = 0;
        if readable {
            bits |= Self::R;
        }
        if writable {
            bits |= Self::W;
        }
        if executable {
            bits |= Self::X;
        }
        Permissions { bits }
    }

    /// Returns true if the segment is readable.
    #[inline]
    pub fn readable(&self) -> bool {
        self.bits & Self::R != 0
    }

    /// Returns true if the segment is writable.
    #[inline]
    pub fn writable(&self) -> bool {
        self.bits & Self::W != 0
    }

    /// Returns true if the segment is executable.
    #[inline]
    pub fn executable(&self) -> bool {
        self.bits & Self::X != 0
    }

    /// Returns true if the segment is readable but not writable.
    #[inline]
    pub fn readonly(&self) -> bool {
        self.readable() && !self.writable()
    }
}

/// Section flags that are specific to each file format.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum SectionFlags {
    /// No section flags.
    None,
    /// ELF section flags.
    #[cfg(feature = "elf")]
    Elf {
        /// `sh_type` field in the section header.
        sh_type: crate::elf::SectionType,
        /// `sh_flags` field in the section header.
        sh_flags: crate::elf::SectionFlags,
    },
    /// Mach-O section flags.
    #[cfg(feature = "macho")]
    MachO {
        /// `flags` field in the section header.
        flags: crate::macho::SectionFlags,
        /// `reserved2` field in the section header.
        ///
        /// This is the size of a stub in a section with type `S_SYMBOL_STUBS`.
        reserved2: u32,
    },
    /// COFF section flags.
    #[cfg(feature = "coff")]
    Coff {
        /// `Characteristics` field in the section header.
        characteristics: crate::pe::SectionFlags,
    },
    /// XCOFF section flags.
    #[cfg(feature = "xcoff")]
    Xcoff {
        /// `s_flags` field in the section header.
        s_flags: crate::xcoff::SectionFlags,
    },
}

/// Symbol flags that are specific to each file format.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum SymbolFlags<Section, Symbol> {
    /// No symbol flags.
    None,
    /// ELF symbol flags.
    #[cfg(feature = "elf")]
    Elf {
        /// `st_info` field in the ELF symbol.
        st_info: crate::elf::SymbolInfo,
        /// `st_other` field in the ELF symbol.
        st_other: crate::elf::SymbolOther,
    },
    /// Mach-O symbol flags.
    #[cfg(feature = "macho")]
    MachO {
        /// `n_type` field in the Mach-O symbol.
        n_type: crate::macho::SymbolFlags,
        /// `n_desc` field in the Mach-O symbol.
        n_desc: crate::macho::SymbolDesc,
    },
    /// COFF flags for a symbol.
    #[cfg(feature = "coff")]
    Coff {
        /// `Type` field in the COFF symbol.
        typ: crate::pe::SymbolType,
        /// `StorageClass` field in the COFF symbol.
        storage_class: crate::pe::SymbolClass,
    },
    /// COFF flags for a section symbol with an auxiliary symbol.
    #[cfg(feature = "coff")]
    CoffSection {
        /// `Type` field in the COFF symbol.
        typ: crate::pe::SymbolType,
        /// `StorageClass` field in the COFF symbol.
        storage_class: crate::pe::SymbolClass,
        /// `Selection` field in the auxiliary symbol for the section.
        selection: crate::pe::ComdatSelection,
        /// `Number` field in the auxiliary symbol for the section.
        associative_section: Option<Section>,
    },
    /// XCOFF symbol flags.
    #[cfg(feature = "xcoff")]
    Xcoff {
        /// `n_type` field in the XCOFF symbol.
        n_type: crate::xcoff::SymbolType,
        /// `n_sclass` field in the XCOFF symbol.
        n_sclass: crate::xcoff::SymbolClass,
        /// `x_smtyp` field in the CSECT auxiliary symbol.
        ///
        /// Only valid if `n_sclass` is `C_EXT`, `C_WEAKEXT`, or `C_HIDEXT`.
        x_smtyp: crate::xcoff::CsectAuxSmtyp,
        /// `x_smclas` field in the CSECT auxiliary symbol.
        ///
        /// Only valid if `n_sclass` is `C_EXT`, `C_WEAKEXT`, or `C_HIDEXT`.
        x_smclas: crate::xcoff::CsectAuxClass,
        /// The containing csect for the symbol.
        ///
        /// Only valid if `x_smtyp` is `XTY_LD`.
        containing_csect: Option<Symbol>,
    },
    #[doc(hidden)]
    #[cfg(not(all(feature = "coff", feature = "xcoff")))]
    _Phantom(core::marker::PhantomData<(Section, Symbol)>),
}

impl<Section, Symbol> SymbolFlags<Section, Symbol> {
    /// Returns the ELF symbol visibility.
    ///
    /// This corresponds to the lower 2 bits of the `st_other` field,
    /// and will be a value such as `elf::STV_DEFAULT`.
    #[cfg(feature = "elf")]
    pub fn elf_visibility(&self) -> Option<crate::elf::SymbolVisibility> {
        match self {
            SymbolFlags::Elf { st_other, .. } => Some(st_other.visibility()),
            _ => None,
        }
    }
}

/// Relocation fields that are specific to each file format and architecture.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum RelocationFlags {
    /// Format independent representation.
    Generic {
        /// The operation used to calculate the result of the relocation.
        kind: RelocationKind,
        /// Information about how the result of the relocation operation is encoded in the place.
        encoding: RelocationEncoding,
        /// The size in bits of the place of relocation.
        size: u8,
    },
    /// ELF relocation fields.
    #[cfg(feature = "elf")]
    Elf {
        /// `r_type` field in the ELF relocation.
        r_type: crate::elf::RelocationType,
    },
    /// Mach-O relocation fields.
    #[cfg(feature = "macho")]
    MachO {
        /// `r_type` field in the Mach-O relocation.
        r_type: crate::macho::RelocationType,
        /// `r_pcrel` field in the Mach-O relocation.
        r_pcrel: bool,
        /// `r_length` field in the Mach-O relocation.
        r_length: u8,
    },
    /// COFF relocation fields.
    #[cfg(feature = "coff")]
    Coff {
        /// `typ` field in the COFF relocation.
        typ: crate::pe::RelocationType,
    },
    /// XCOFF relocation fields.
    #[cfg(feature = "xcoff")]
    Xcoff {
        /// `r_rtype` field in the XCOFF relocation.
        r_rtype: crate::xcoff::RelocationType,
        /// `r_rsize` field in the XCOFF relocation.
        r_rsize: u8,
    },
    /// Wasm relocation fields.
    #[cfg(feature = "wasm")]
    Wasm {
        /// Relocation type (the `R_WASM_*` constant).
        r_type: u8,
    },
}

/// Wrapper to print as `[..]` without a manual `Debug` implementation, rather than dumping an
/// entire byte array.
#[allow(unused)]
#[derive(Clone, Copy, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub(crate) struct SkipDebugList<T>(pub T);

impl<T> fmt::Debug for SkipDebugList<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "[..]")
    }
}

impl<T> Deref for SkipDebugList<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl<T> DerefMut for SkipDebugList<T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}
