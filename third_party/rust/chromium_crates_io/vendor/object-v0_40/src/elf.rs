//! ELF definitions.
//!
//! These definitions are independent of read/write support, although we do implement
//! some traits useful for those.
//!
//! This module is the equivalent of /usr/include/elf.h, and is based heavily on it.

#![allow(missing_docs)]
#![allow(clippy::identity_op)]

#[cfg(feature = "names")]
use crate::constants::{ConstantNames, FlagNames};
use crate::endian::{Endian, I32, I64, U16, U32, U64};
use crate::pod::Pod;

/// Platform-specific constant names for an ELF file.
///
/// Returned by [`names`] and [`machine_names`].
#[cfg(feature = "names")]
#[derive(Debug)]
#[non_exhaustive]
pub struct Names {
    /// Values for `FileHeader*::e_type`.
    pub et: &'static ConstantNames<FileType>,
    /// Values for `FileHeader*::e_flags`.
    pub ef: &'static FlagNames<FileFlags>,
    /// Special values for `Sym*::st_shndx` and `FileHeader*::e_shstrndx`.
    pub shn: &'static ConstantNames<SymbolSection>,
    /// Values for `SectionHeader*::sh_type`.
    pub sht: &'static ConstantNames<SectionType>,
    /// Values for `SectionHeader*::sh_flags`.
    pub shf: &'static FlagNames<SectionFlags>,
    /// Values for `st_bind` field of `Sym*::st_info`.
    pub stb: &'static ConstantNames<SymbolBind>,
    /// Values for `st_type` field of `Sym*::st_info`.
    pub stt: &'static ConstantNames<SymbolType>,
    /// Values for `Sym*::st_other`.
    pub sto: &'static FlagNames<SymbolOther>,
    /// Values for `ProgramHeader*::p_type`.
    pub pt: &'static ConstantNames<ProgramType>,
    /// Values for `ProgramHeader*::p_flags`.
    pub pf: &'static FlagNames<ProgramFlags>,
    /// Values for `Dyn*::d_tag`.
    pub dt: &'static ConstantNames<DynamicTag>,
    /// Values for `r_type` field of `Rel*::r_info`.
    pub r: &'static ConstantNames<RelocationType>,
}

names! {
    struct Base;
    consts et = NAMES_ET;
    flags ef: FileFlags(u32) = {};
    consts shn = NAMES_SHN;
    consts sht = NAMES_SHT;
    flags shf = NAMES_SHF;
    consts stb = NAMES_STB;
    consts stt = NAMES_STT;
    flags sto = NAMES_STO;
    consts pt = NAMES_PT;
    flags pf = NAMES_PF;
    consts dt = NAMES_DT;
    consts r: RelocationType = {};
}

/// Return the platform independent names for constants.
#[cfg(feature = "names")]
pub const fn names() -> &'static Names {
    Base::names()
}

/// Return the platform specific names for constants.
///
/// Note that these also include the values returned by [`names`].
///
/// `machine` corresponds to the `FileHeader*::e_machine` field.
#[cfg(feature = "names")]
pub const fn machine_names(machine: Machine) -> &'static Names {
    match machine {
        EM_386 => I386::names(),
        EM_68K => M68k::names(),
        EM_AARCH64 => Aarch64::names(),
        EM_ALPHA => Alpha::names(),
        EM_ALTERA_NIOS2 => Nios2::names(),
        EM_ARM => Arm::names(),
        EM_AVR => Avr::names(),
        EM_BPF => Bpf::names(),
        EM_CRIS => Cris::names(),
        EM_CSKY => Csky::names(),
        EM_HEXAGON => Hex::names(),
        EM_MCST_ELBRUS => E2k::names(),
        EM_IA_64 => Ia64::names(),
        EM_LOONGARCH => Larch::names(),
        EM_M32R => M32r::names(),
        EM_METAG => Metag::names(),
        EM_MICROBLAZE => Microblaze::names(),
        EM_MIPS => Mips::names(),
        EM_MN10300 => Mn10300::names(),
        EM_MSP430 => Msp430::names(),
        EM_NDS32 => Nds32::names(),
        EM_PARISC => Parisc::names(),
        EM_PPC => Ppc::names(),
        EM_PPC64 => Ppc64::names(),
        EM_RISCV => Riscv::names(),
        EM_S390 => S390::names(),
        EM_SBF => Sbf::names(),
        EM_SH => Sh::names(),
        EM_SHARC => Sharc::names(),
        // TODO: might need to be separated
        EM_SPARC | EM_SPARC32PLUS => Sparc::names(),
        EM_SPARCV9 => SparcV9::names(),
        EM_TILEGX => Tilegx::names(),
        EM_TILEPRO => Tilepro::names(),
        EM_X86_64 => X86_64::names(),
        EM_XTENSA => Xtensa::names(),
        _ => Base::names(),
    }
}

/// The header at the start of every 32-bit ELF file.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct FileHeader32<E: Endian> {
    /// Magic number and other information.
    pub e_ident: Ident,
    /// Object file type. One of the `ET_*` constants.
    pub e_type: U16<E, FileType>,
    /// Architecture. One of the `EM_*` constants.
    pub e_machine: U16<E, Machine>,
    /// Object file version. Must be `EV_CURRENT`.
    pub e_version: U32<E>,
    /// Entry point virtual address.
    pub e_entry: U32<E>,
    /// Program header table file offset.
    pub e_phoff: U32<E>,
    /// Section header table file offset.
    pub e_shoff: U32<E>,
    /// Processor-specific flags.
    ///
    /// A combination of the `EF_*` constants.
    pub e_flags: U32<E, FileFlags>,
    /// Size in bytes of this header.
    pub e_ehsize: U16<E>,
    /// Program header table entry size.
    pub e_phentsize: U16<E>,
    /// Program header table entry count.
    ///
    /// If the count is greater than or equal to `PN_XNUM` then this field is set to
    /// `PN_XNUM` and the count is stored in the `sh_info` field of section 0.
    pub e_phnum: U16<E>,
    /// Section header table entry size.
    pub e_shentsize: U16<E>,
    /// Section header table entry count.
    ///
    /// If the count is greater than or equal to `SHN_LORESERVE` then this field is set to
    /// `0` and the count is stored in the `sh_size` field of section 0.
    /// first section header.
    pub e_shnum: U16<E>,
    /// Section header string table index.
    ///
    /// If the index is greater than or equal to `SHN_LORESERVE` then this field is set to
    /// `SHN_XINDEX` and the index is stored in the `sh_link` field of section 0.
    pub e_shstrndx: U16<E, SymbolSection>,
}

/// The header at the start of every 64-bit ELF file.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct FileHeader64<E: Endian> {
    /// Magic number and other information.
    pub e_ident: Ident,
    /// Object file type. One of the `ET_*` constants.
    pub e_type: U16<E, FileType>,
    /// Architecture. One of the `EM_*` constants.
    pub e_machine: U16<E, Machine>,
    /// Object file version. Must be `EV_CURRENT`.
    pub e_version: U32<E>,
    /// Entry point virtual address.
    pub e_entry: U64<E>,
    /// Program header table file offset.
    pub e_phoff: U64<E>,
    /// Section header table file offset.
    pub e_shoff: U64<E>,
    /// Processor-specific flags.
    ///
    /// A combination of the `EF_*` constants.
    pub e_flags: U32<E, FileFlags>,
    /// Size in bytes of this header.
    pub e_ehsize: U16<E>,
    /// Program header table entry size.
    pub e_phentsize: U16<E>,
    /// Program header table entry count.
    ///
    /// If the count is greater than or equal to `PN_XNUM` then this field is set to
    /// `PN_XNUM` and the count is stored in the `sh_info` field of section 0.
    pub e_phnum: U16<E>,
    /// Section header table entry size.
    pub e_shentsize: U16<E>,
    /// Section header table entry count.
    ///
    /// If the count is greater than or equal to `SHN_LORESERVE` then this field is set to
    /// `0` and the count is stored in the `sh_size` field of section 0.
    /// first section header.
    pub e_shnum: U16<E>,
    /// Section header string table index.
    ///
    /// If the index is greater than or equal to `SHN_LORESERVE` then this field is set to
    /// `SHN_XINDEX` and the index is stored in the `sh_link` field of section 0.
    pub e_shstrndx: U16<E, SymbolSection>,
}

/// Magic number and other information.
///
/// Contained in the file header.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Ident {
    /// Magic number. Must be `ELFMAG`.
    pub magic: [u8; 4],
    /// File class. One of the `ELFCLASS*` constants.
    pub class: FileClass,
    /// Data encoding. One of the `ELFDATA*` constants.
    pub data: DataEncoding,
    /// ELF version. Must be `EV_CURRENT`.
    pub version: FileVersion,
    /// OS ABI identification. One of the `ELFOSABI*` constants.
    pub os_abi: OsAbi,
    /// ABI version.
    ///
    /// The meaning of this field depends on the `os_abi` value.
    pub abi_version: u8,
    /// Padding bytes.
    pub padding: [u8; 7],
}

/// File identification bytes stored in `Ident::magic`.
pub const ELFMAG: [u8; 4] = [0x7f, b'E', b'L', b'F'];

newtype!(
    /// Values for `Ident::class`.
    #[repr(transparent)]
    struct FileClass(u8);
);

newtype_constant_names!(NAMES_CLASS: FileClass(u8) = {
    /// Invalid class.
    ELFCLASSNONE = 0,
    /// 32-bit object.
    ELFCLASS32 = 1,
    /// 64-bit object.
    ELFCLASS64 = 2,
});

newtype!(
    /// Values for `Ident::data`.
    #[repr(transparent)]
    struct DataEncoding(u8);
);

newtype_constant_names!(NAMES_DATA: DataEncoding(u8) = {
    /// Invalid data encoding.
    ELFDATANONE = 0,
    /// 2's complement, little endian.
    ELFDATA2LSB = 1,
    /// 2's complement, big endian.
    ELFDATA2MSB = 2,
});

newtype!(
    /// Values for `Ident::os_abi`.
    #[repr(transparent)]
    struct OsAbi(u8);
);

newtype_constant_names!(NAMES_ELFOSABI: OsAbi(u8) = {
    /// UNIX System V ABI.
    ELFOSABI_SYSV = 0,
    /// UNIX System V ABI.
    ///
    /// Alias.
    ELFOSABI_NONE = 0,
    /// HP-UX.
    ELFOSABI_HPUX = 1,
    /// NetBSD.
    ELFOSABI_NETBSD = 2,
    /// Object uses GNU ELF extensions.
    ELFOSABI_GNU = 3,
    /// Object uses GNU ELF extensions.
    ///
    /// Compatibility alias.
    ELFOSABI_LINUX = ELFOSABI_GNU.0,
    /// GNU/Hurd.
    ELFOSABI_HURD = 4,
    /// Sun Solaris.
    ELFOSABI_SOLARIS = 6,
    /// IBM AIX.
    ELFOSABI_AIX = 7,
    /// SGI Irix.
    ELFOSABI_IRIX = 8,
    /// FreeBSD.
    ELFOSABI_FREEBSD = 9,
    /// Compaq TRU64 UNIX.
    ELFOSABI_TRU64 = 10,
    /// Novell Modesto.
    ELFOSABI_MODESTO = 11,
    /// OpenBSD.
    ELFOSABI_OPENBSD = 12,
    /// OpenVMS.
    ELFOSABI_OPENVMS = 13,
    /// Hewlett-Packard Non-Stop Kernel.
    ELFOSABI_NSK = 14,
    /// AROS
    ELFOSABI_AROS = 15,
    /// FenixOS
    ELFOSABI_FENIXOS = 16,
    /// Nuxi CloudABI
    ELFOSABI_CLOUDABI = 17,
    /// ARM EABI.
    ELFOSABI_ARM_AEABI = 64,
    /// ARM.
    ELFOSABI_ARM = 97,
    /// Standalone (embedded) application.
    ELFOSABI_STANDALONE = 255,
});

newtype!(
    /// Values for `FileHeader*::e_type`.
    struct FileType(u16);
);

impl FileType {
    /// Return true if the type is in the OS-specific range.
    pub fn is_os(self) -> bool {
        self.0 >= ET_LOOS && self.0 <= ET_HIOS
    }
    /// Return true if the type is in the processor-specific range.
    pub fn is_proc(self) -> bool {
        debug_assert_eq!(ET_HIPROC, !0);
        self.0 >= ET_LOPROC
    }
}

newtype_constant_names!(NAMES_ET: FileType(u16) = {
    /// No file type.
    ET_NONE = 0,
    /// Relocatable file.
    ET_REL = 1,
    /// Executable file.
    ET_EXEC = 2,
    /// Shared object file.
    ET_DYN = 3,
    /// Core file.
    ET_CORE = 4,
});

/// OS-specific range start.
pub const ET_LOOS: u16 = 0xfe00;
/// OS-specific range end.
pub const ET_HIOS: u16 = 0xfeff;
/// Processor-specific range start.
pub const ET_LOPROC: u16 = 0xff00;
/// Processor-specific range end.
pub const ET_HIPROC: u16 = 0xffff;

newtype!(
    /// Values for `FileHeader*::e_machine`.
    struct Machine(u16);
);

newtype_constant_names!(NAMES_EM: Machine(u16) = {
    /// No machine
    EM_NONE = 0,
    /// AT&T WE 32100
    EM_M32 = 1,
    /// SUN SPARC
    EM_SPARC = 2,
    /// Intel 80386
    EM_386 = 3,
    /// Motorola m68k family
    EM_68K = 4,
    /// Motorola m88k family
    EM_88K = 5,
    /// Intel MCU
    EM_IAMCU = 6,
    /// Intel 80860
    EM_860 = 7,
    /// MIPS R3000 big-endian
    EM_MIPS = 8,
    /// IBM System/370
    EM_S370 = 9,
    /// MIPS R3000 little-endian
    EM_MIPS_RS3_LE = 10,
    /// HPPA
    EM_PARISC = 15,
    /// Fujitsu VPP500
    EM_VPP500 = 17,
    /// Sun's "v8plus"
    EM_SPARC32PLUS = 18,
    /// Intel 80960
    EM_960 = 19,
    /// PowerPC
    EM_PPC = 20,
    /// PowerPC 64-bit
    EM_PPC64 = 21,
    /// IBM S390
    EM_S390 = 22,
    /// IBM SPU/SPC
    EM_SPU = 23,
    /// NEC V800 series
    EM_V800 = 36,
    /// Fujitsu FR20
    EM_FR20 = 37,
    /// TRW RH-32
    EM_RH32 = 38,
    /// Motorola RCE
    EM_RCE = 39,
    /// ARM
    EM_ARM = 40,
    /// Digital Alpha
    EM_FAKE_ALPHA = 41,
    /// Hitachi SH
    EM_SH = 42,
    /// SPARC v9 64-bit
    EM_SPARCV9 = 43,
    /// Siemens Tricore
    EM_TRICORE = 44,
    /// Argonaut RISC Core
    EM_ARC = 45,
    /// Hitachi H8/300
    EM_H8_300 = 46,
    /// Hitachi H8/300H
    EM_H8_300H = 47,
    /// Hitachi H8S
    EM_H8S = 48,
    /// Hitachi H8/500
    EM_H8_500 = 49,
    /// Intel Merced
    EM_IA_64 = 50,
    /// Stanford MIPS-X
    EM_MIPS_X = 51,
    /// Motorola Coldfire
    EM_COLDFIRE = 52,
    /// Motorola M68HC12
    EM_68HC12 = 53,
    /// Fujitsu MMA Multimedia Accelerator
    EM_MMA = 54,
    /// Siemens PCP
    EM_PCP = 55,
    /// Sony nCPU embeeded RISC
    EM_NCPU = 56,
    /// Denso NDR1 microprocessor
    EM_NDR1 = 57,
    /// Motorola Start*Core processor
    EM_STARCORE = 58,
    /// Toyota ME16 processor
    EM_ME16 = 59,
    /// STMicroelectronic ST100 processor
    EM_ST100 = 60,
    /// Advanced Logic Corp. Tinyj emb.fam
    EM_TINYJ = 61,
    /// AMD x86-64 architecture
    EM_X86_64 = 62,
    /// Sony DSP Processor
    EM_PDSP = 63,
    /// Digital PDP-10
    EM_PDP10 = 64,
    /// Digital PDP-11
    EM_PDP11 = 65,
    /// Siemens FX66 microcontroller
    EM_FX66 = 66,
    /// STMicroelectronics ST9+ 8/16 mc
    EM_ST9PLUS = 67,
    /// STmicroelectronics ST7 8 bit mc
    EM_ST7 = 68,
    /// Motorola MC68HC16 microcontroller
    EM_68HC16 = 69,
    /// Motorola MC68HC11 microcontroller
    EM_68HC11 = 70,
    /// Motorola MC68HC08 microcontroller
    EM_68HC08 = 71,
    /// Motorola MC68HC05 microcontroller
    EM_68HC05 = 72,
    /// Silicon Graphics SVx
    EM_SVX = 73,
    /// STMicroelectronics ST19 8 bit mc
    EM_ST19 = 74,
    /// Digital VAX
    EM_VAX = 75,
    /// Axis Communications 32-bit emb.proc
    EM_CRIS = 76,
    /// Infineon Technologies 32-bit emb.proc
    EM_JAVELIN = 77,
    /// Element 14 64-bit DSP Processor
    EM_FIREPATH = 78,
    /// LSI Logic 16-bit DSP Processor
    EM_ZSP = 79,
    /// Donald Knuth's educational 64-bit proc
    EM_MMIX = 80,
    /// Harvard University machine-independent object files
    EM_HUANY = 81,
    /// SiTera Prism
    EM_PRISM = 82,
    /// Atmel AVR 8-bit microcontroller
    EM_AVR = 83,
    /// Fujitsu FR30
    EM_FR30 = 84,
    /// Mitsubishi D10V
    EM_D10V = 85,
    /// Mitsubishi D30V
    EM_D30V = 86,
    /// NEC v850
    EM_V850 = 87,
    /// Mitsubishi M32R
    EM_M32R = 88,
    /// Matsushita MN10300
    EM_MN10300 = 89,
    /// Matsushita MN10200
    EM_MN10200 = 90,
    /// picoJava
    EM_PJ = 91,
    /// OpenRISC 32-bit embedded processor
    EM_OPENRISC = 92,
    /// ARC International ARCompact
    EM_ARC_COMPACT = 93,
    /// Tensilica Xtensa Architecture
    EM_XTENSA = 94,
    /// Alphamosaic VideoCore
    EM_VIDEOCORE = 95,
    /// Thompson Multimedia General Purpose Proc
    EM_TMM_GPP = 96,
    /// National Semi. 32000
    EM_NS32K = 97,
    /// Tenor Network TPC
    EM_TPC = 98,
    /// Trebia SNP 1000
    EM_SNP1K = 99,
    /// STMicroelectronics ST200
    EM_ST200 = 100,
    /// Ubicom IP2xxx
    EM_IP2K = 101,
    /// MAX processor
    EM_MAX = 102,
    /// National Semi. CompactRISC
    EM_CR = 103,
    /// Fujitsu F2MC16
    EM_F2MC16 = 104,
    /// Texas Instruments msp430
    EM_MSP430 = 105,
    /// Analog Devices Blackfin DSP
    EM_BLACKFIN = 106,
    /// Seiko Epson S1C33 family
    EM_SE_C33 = 107,
    /// Sharp embedded microprocessor
    EM_SEP = 108,
    /// Arca RISC
    EM_ARCA = 109,
    /// PKU-Unity & MPRC Peking Uni. mc series
    EM_UNICORE = 110,
    /// eXcess configurable cpu
    EM_EXCESS = 111,
    /// Icera Semi. Deep Execution Processor
    EM_DXP = 112,
    /// Altera Nios II
    EM_ALTERA_NIOS2 = 113,
    /// National Semi. CompactRISC CRX
    EM_CRX = 114,
    /// Motorola XGATE
    EM_XGATE = 115,
    /// Infineon C16x/XC16x
    EM_C166 = 116,
    /// Renesas M16C
    EM_M16C = 117,
    /// Microchip Technology dsPIC30F
    EM_DSPIC30F = 118,
    /// Freescale Communication Engine RISC
    EM_CE = 119,
    /// Renesas M32C
    EM_M32C = 120,
    /// Altium TSK3000
    EM_TSK3000 = 131,
    /// Freescale RS08
    EM_RS08 = 132,
    /// Analog Devices SHARC family
    EM_SHARC = 133,
    /// Cyan Technology eCOG2
    EM_ECOG2 = 134,
    /// Sunplus S+core7 RISC
    EM_SCORE7 = 135,
    /// New Japan Radio (NJR) 24-bit DSP
    EM_DSP24 = 136,
    /// Broadcom VideoCore III
    EM_VIDEOCORE3 = 137,
    /// RISC for Lattice FPGA
    EM_LATTICEMICO32 = 138,
    /// Seiko Epson C17
    EM_SE_C17 = 139,
    /// Texas Instruments TMS320C6000 DSP
    EM_TI_C6000 = 140,
    /// Texas Instruments TMS320C2000 DSP
    EM_TI_C2000 = 141,
    /// Texas Instruments TMS320C55x DSP
    EM_TI_C5500 = 142,
    /// Texas Instruments App. Specific RISC
    EM_TI_ARP32 = 143,
    /// Texas Instruments Prog. Realtime Unit
    EM_TI_PRU = 144,
    /// STMicroelectronics 64bit VLIW DSP
    EM_MMDSP_PLUS = 160,
    /// Cypress M8C
    EM_CYPRESS_M8C = 161,
    /// Renesas R32C
    EM_R32C = 162,
    /// NXP Semi. TriMedia
    EM_TRIMEDIA = 163,
    /// QUALCOMM Hexagon
    EM_HEXAGON = 164,
    /// Intel 8051 and variants
    EM_8051 = 165,
    /// STMicroelectronics STxP7x
    EM_STXP7X = 166,
    /// Andes Tech. compact code emb. RISC
    EM_NDS32 = 167,
    /// Cyan Technology eCOG1X
    EM_ECOG1X = 168,
    /// Dallas Semi. MAXQ30 mc
    EM_MAXQ30 = 169,
    /// New Japan Radio (NJR) 16-bit DSP
    EM_XIMO16 = 170,
    /// M2000 Reconfigurable RISC
    EM_MANIK = 171,
    /// Cray NV2 vector architecture
    EM_CRAYNV2 = 172,
    /// Renesas RX
    EM_RX = 173,
    /// Imagination Tech. META
    EM_METAG = 174,
    /// MCST Elbrus
    EM_MCST_ELBRUS = 175,
    /// Cyan Technology eCOG16
    EM_ECOG16 = 176,
    /// National Semi. CompactRISC CR16
    EM_CR16 = 177,
    /// Freescale Extended Time Processing Unit
    EM_ETPU = 178,
    /// Infineon Tech. SLE9X
    EM_SLE9X = 179,
    /// Intel L10M
    EM_L10M = 180,
    /// Intel K10M
    EM_K10M = 181,
    /// ARM AARCH64
    EM_AARCH64 = 183,
    /// Amtel 32-bit microprocessor
    EM_AVR32 = 185,
    /// STMicroelectronics STM8
    EM_STM8 = 186,
    /// Tileta TILE64
    EM_TILE64 = 187,
    /// Tilera TILEPro
    EM_TILEPRO = 188,
    /// Xilinx MicroBlaze
    EM_MICROBLAZE = 189,
    /// NVIDIA CUDA
    EM_CUDA = 190,
    /// Tilera TILE-Gx
    EM_TILEGX = 191,
    /// CloudShield
    EM_CLOUDSHIELD = 192,
    /// KIPO-KAIST Core-A 1st gen.
    EM_COREA_1ST = 193,
    /// KIPO-KAIST Core-A 2nd gen.
    EM_COREA_2ND = 194,
    /// Synopsys ARCompact V2
    EM_ARC_COMPACT2 = 195,
    /// Open8 RISC
    EM_OPEN8 = 196,
    /// Renesas RL78
    EM_RL78 = 197,
    /// Broadcom VideoCore V
    EM_VIDEOCORE5 = 198,
    /// Renesas 78KOR
    EM_78KOR = 199,
    /// Freescale 56800EX DSC
    EM_56800EX = 200,
    /// Beyond BA1
    EM_BA1 = 201,
    /// Beyond BA2
    EM_BA2 = 202,
    /// XMOS xCORE
    EM_XCORE = 203,
    /// Microchip 8-bit PIC(r)
    EM_MCHP_PIC = 204,
    /// KM211 KM32
    EM_KM32 = 210,
    /// KM211 KMX32
    EM_KMX32 = 211,
    /// KM211 KMX16
    EM_EMX16 = 212,
    /// KM211 KMX8
    EM_EMX8 = 213,
    /// KM211 KVARC
    EM_KVARC = 214,
    /// Paneve CDP
    EM_CDP = 215,
    /// Cognitive Smart Memory Processor
    EM_COGE = 216,
    /// Bluechip CoolEngine
    EM_COOL = 217,
    /// Nanoradio Optimized RISC
    EM_NORC = 218,
    /// CSR Kalimba
    EM_CSR_KALIMBA = 219,
    /// Zilog Z80
    EM_Z80 = 220,
    /// Controls and Data Services VISIUMcore
    EM_VISIUM = 221,
    /// FTDI Chip FT32
    EM_FT32 = 222,
    /// Moxie processor
    EM_MOXIE = 223,
    /// AMD GPU
    EM_AMDGPU = 224,
    /// RISC-V
    EM_RISCV = 243,
    /// Linux BPF -- in-kernel virtual machine
    EM_BPF = 247,
    /// C-SKY
    EM_CSKY = 252,
    /// Loongson LoongArch
    EM_LOONGARCH = 258,
    /// Solana Binary Format
    EM_SBF = 263,
    /// Digital Alpha
    EM_ALPHA = 0x9026,
});

newtype!(
    /// Values for `FileHeader*::e_version` and `Ident::version`.
    #[repr(transparent)]
    struct FileVersion(u8);
);

newtype_constant_names!(NAMES_VERSION: FileVersion(u8) = {
    /// Invalid ELF version.
    EV_NONE = 0,
    /// Current ELF version.
    EV_CURRENT = 1,
});

newtype!(
    /// Values for `FileHeader*::e_flags`.
    struct FileFlags(u32);
);

// Names depend on machine.
newtype_flag_names!(FileFlags(u32) = {});

impl FileFlags {
    /// Get the MIPS ABI field.
    ///
    /// One of the `EF_MIPS_ABI_*` constants.
    pub fn mips_abi(self) -> FileFlags {
        FileFlags(self.0 & EF_MIPS_ABI)
    }

    /// Set the MIPS ABI field.
    ///
    /// One of the `EF_MIPS_ABI_*` constants.
    pub fn with_mips_abi(self, abi: FileFlags) -> FileFlags {
        FileFlags(self.0 & !EF_MIPS_ABI | abi.0 & EF_MIPS_ABI)
    }

    /// Get the MIPS architecture field.
    ///
    /// One of the `EF_MIPS_ARCH_*` constants.
    pub fn mips_arch(self) -> FileFlags {
        FileFlags(self.0 & EF_MIPS_ARCH)
    }

    /// Set the MIPS architecture field.
    ///
    /// One of the `EF_MIPS_ARCH_*` constants.
    pub fn with_mips_arch(self, arch: FileFlags) -> FileFlags {
        FileFlags(self.0 & !EF_MIPS_ARCH | arch.0 & EF_MIPS_ARCH)
    }

    /// Get the PA-RISC architecture field.
    ///
    /// One of the `EFA_PARISC_*` constants.
    pub fn parisc_arch(self) -> FileFlags {
        FileFlags(self.0 & EF_PARISC_ARCH)
    }

    /// Set the PA-RISC architecture field.
    ///
    /// One of the `EFA_PARISC_*` constants.
    pub fn with_parisc_arch(self, arch: FileFlags) -> FileFlags {
        FileFlags(self.0 & !EF_PARISC_ARCH | arch.0 & EF_PARISC_ARCH)
    }

    /// Get the PPC64 ABI field.
    ///
    /// See [`EF_PPC64_ABI`] for values.
    pub fn ppc64_abi(self) -> u32 {
        self.0 & EF_PPC64_ABI
    }

    /// Set the PPC64 ABI field.
    ///
    /// See [`EF_PPC64_ABI`] for values.
    pub fn with_ppc64_abi(self, abi: u32) -> FileFlags {
        FileFlags(self.0 & !EF_PPC64_ABI | abi & EF_PPC64_ABI)
    }

    /// Get the ARM EABI field.
    ///
    /// One of the `EF_ARM_EABI_*` values.
    pub fn arm_eabi(self) -> FileFlags {
        FileFlags(self.0 & EF_ARM_EABIMASK)
    }

    /// Set the ARM EABI field.
    ///
    /// One of the `EF_ARM_EABI_*` values.
    pub fn with_arm_eabi(self, eabi: FileFlags) -> FileFlags {
        FileFlags(self.0 & !EF_ARM_EABIMASK | eabi.0 & EF_ARM_EABIMASK)
    }

    /// Get the AVR architecture field.
    ///
    /// One of the `EF_AVR_ARCH_*` values.
    pub fn avr_arch(self) -> FileFlags {
        FileFlags(self.0 & EF_AVR_ARCH)
    }

    /// Set the AVR architecture field.
    ///
    /// One of the `EF_AVR_ARCH_*` values.
    pub fn with_avr_arch(self, arch: FileFlags) -> FileFlags {
        FileFlags(self.0 & !EF_AVR_ARCH | arch.0 & EF_AVR_ARCH)
    }

    /// Get the SH machine field.
    ///
    /// One of the `EF_SH*` values.
    pub fn sh_mach(self) -> FileFlags {
        FileFlags(self.0 & EF_SH_MACH_MASK)
    }

    /// Set the SH machine field.
    ///
    /// One of the `EF_SH*` values.
    pub fn with_sh_mach(self, mach: FileFlags) -> FileFlags {
        FileFlags(self.0 & !EF_SH_MACH_MASK | mach.0 & EF_SH_MACH_MASK)
    }

    /// Get the RISC-V float ABI field.
    ///
    /// One of the `EF_RISCV_FLOAT_ABI_*` values.
    pub fn riscv_float_abi(self) -> FileFlags {
        FileFlags(self.0 & EF_RISCV_FLOAT_ABI)
    }

    /// Set the RISC-V float ABI field.
    ///
    /// One of the `EF_RISCV_FLOAT_ABI_*` values.
    pub fn with_riscv_float_abi(self, float_abi: FileFlags) -> FileFlags {
        FileFlags(self.0 & !EF_RISCV_FLOAT_ABI | float_abi.0 & EF_RISCV_FLOAT_ABI)
    }

    /// Get the LoongArch ABI modifier field.
    ///
    /// One of the `EF_LARCH_ABI_* ` values.
    pub fn larch_abi_modifier(self) -> FileFlags {
        FileFlags(self.0 & EF_LARCH_ABI_MODIFIER_MASK)
    }

    /// Set the LoongArch ABI modifier field.
    ///
    /// One of the `EF_LARCH_ABI_* ` values.
    pub fn with_larch_abi_modifier(self, modifier: FileFlags) -> FileFlags {
        FileFlags(self.0 & !EF_LARCH_ABI_MODIFIER_MASK | modifier.0 & EF_LARCH_ABI_MODIFIER_MASK)
    }
}

/// Section header.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct SectionHeader32<E: Endian> {
    /// Section name.
    ///
    /// This is an offset into the section header string table.
    pub sh_name: U32<E>,
    /// Section type. One of the `SHT_*` constants.
    pub sh_type: U32<E, SectionType>,
    /// Section flags. A combination of the `SHF_*` constants.
    pub sh_flags: U32<E, SectionFlags>,
    /// Section virtual address at execution.
    pub sh_addr: U32<E>,
    /// Section file offset.
    pub sh_offset: U32<E>,
    /// Section size in bytes.
    pub sh_size: U32<E>,
    /// Link to another section.
    ///
    /// The section relationship depends on the `sh_type` value.
    pub sh_link: U32<E>,
    /// Additional section information.
    ///
    /// The meaning of this field depends on the `sh_type` value.
    pub sh_info: U32<E>,
    /// Section alignment.
    pub sh_addralign: U32<E>,
    /// Entry size if the section holds a table.
    pub sh_entsize: U32<E>,
}

/// Section header.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct SectionHeader64<E: Endian> {
    /// Section name.
    ///
    /// This is an offset into the section header string table.
    pub sh_name: U32<E>,
    /// Section type. One of the `SHT_*` constants.
    pub sh_type: U32<E, SectionType>,
    /// Section flags. A combination of the `SHF_*` constants.
    pub sh_flags: U64<E, SectionFlags>,
    /// Section virtual address at execution.
    pub sh_addr: U64<E>,
    /// Section file offset.
    pub sh_offset: U64<E>,
    /// Section size in bytes.
    pub sh_size: U64<E>,
    /// Link to another section.
    ///
    /// The section relationship depends on the `sh_type` value.
    pub sh_link: U32<E>,
    /// Additional section information.
    ///
    /// The meaning of this field depends on the `sh_type` value.
    pub sh_info: U32<E>,
    /// Section alignment.
    pub sh_addralign: U64<E>,
    /// Entry size if the section holds a table.
    pub sh_entsize: U64<E>,
}

newtype!(
    /// Section header index.
    ///
    /// May be a reserved value with special meaning.
    ///
    /// This is primarily used for `Sym*::st_shndx`.
    /// However, `FileHeader*::e_shstrndx` may be set to `SHN_UNDEF`, `SHN_XINDEX`, or an index.
    struct SymbolSection(u16);
);

impl SymbolSection {
    /// Construct a `SymbolSection` from a `u32` section index.
    ///
    /// Returns `SHN_XINDEX` if the index is >= `SHN_LORESERVE`. The caller must write the
    /// extended index separately.
    pub fn new(index: u32) -> Self {
        if index >= u32::from(SHN_LORESERVE) {
            SHN_XINDEX
        } else {
            SymbolSection(index as u16)
        }
    }

    /// Return the section index, or `None` if this is `SHN_UNDEF` or in the reserved range.
    pub fn index(self) -> Option<u16> {
        if self.is_special() {
            None
        } else {
            Some(self.0)
        }
    }

    /// Return true if the number is `SHN_UNDEF` or in the reserved range.
    pub fn is_special(self) -> bool {
        self == SHN_UNDEF || self.is_reserved()
    }

    /// Return true if the number is in the reserved range.
    pub fn is_reserved(self) -> bool {
        debug_assert_eq!(SHN_HIRESERVE, !0);
        self.0 >= SHN_LORESERVE
    }
    /// Return true if the number is in the OS-specific range.
    pub fn is_os(self) -> bool {
        self.0 >= SHN_LOOS && self.0 <= SHN_HIOS
    }
    /// Return true if the number is in the processor-specific range.
    pub fn is_proc(self) -> bool {
        self.0 >= SHN_LOPROC && self.0 <= SHN_HIPROC
    }
}

newtype_constant_names!(NAMES_SHN: SymbolSection(u16) = {
    /// Undefined section.
    SHN_UNDEF = 0,
    /// Associated symbol is absolute.
    SHN_ABS = 0xfff1,
    /// Associated symbol is common.
    SHN_COMMON = 0xfff2,
    /// Section index is in the `SHT_SYMTAB_SHNDX` section.
    SHN_XINDEX = 0xffff,
});

/// Start of reserved section indices.
pub const SHN_LORESERVE: u16 = 0xff00;
/// Start of processor-specific section indices.
pub const SHN_LOPROC: u16 = 0xff00;
/// End of processor-specific section indices.
pub const SHN_HIPROC: u16 = 0xff1f;
/// Start of OS-specific section indices.
pub const SHN_LOOS: u16 = 0xff20;
/// End of OS-specific section indices.
pub const SHN_HIOS: u16 = 0xff3f;
/// End of reserved section indices.
pub const SHN_HIRESERVE: u16 = 0xffff;

newtype!(
    /// Values for `SectionHeader*::sh_type`.
    struct SectionType(u32);
);

impl SectionType {
    /// Return true if the type is in the OS-specific range.
    pub fn is_os(self) -> bool {
        self.0 >= SHT_LOOS && self.0 <= SHT_HIOS
    }
    /// Return true if the type is in the Sun-specific range.
    pub fn is_sun(self) -> bool {
        self.0 >= SHT_LOSUNW && self.0 <= SHT_HISUNW
    }
    /// Return true if the type is in the processor-specific range.
    pub fn is_proc(self) -> bool {
        self.0 >= SHT_LOPROC && self.0 <= SHT_HIPROC
    }
    /// Return true if the type is in the application-specific range.
    pub fn is_user(self) -> bool {
        self.0 >= SHT_LOUSER && self.0 <= SHT_HIUSER
    }
}

newtype_constant_names!(NAMES_SHT: SectionType(u32) = {
    /// Section header table entry is unused.
    SHT_NULL = 0,
    /// Program data.
    SHT_PROGBITS = 1,
    /// Symbol table.
    SHT_SYMTAB = 2,
    /// String table.
    SHT_STRTAB = 3,
    /// Relocation entries with explicit addends.
    SHT_RELA = 4,
    /// Symbol hash table.
    SHT_HASH = 5,
    /// Dynamic linking information.
    SHT_DYNAMIC = 6,
    /// Notes.
    SHT_NOTE = 7,
    /// Program space with no data (bss).
    SHT_NOBITS = 8,
    /// Relocation entries without explicit addends.
    SHT_REL = 9,
    /// Reserved section type.
    SHT_SHLIB = 10,
    /// Dynamic linker symbol table.
    SHT_DYNSYM = 11,
    /// Array of constructors.
    SHT_INIT_ARRAY = 14,
    /// Array of destructors.
    SHT_FINI_ARRAY = 15,
    /// Array of pre-constructors.
    SHT_PREINIT_ARRAY = 16,
    /// Section group.
    SHT_GROUP = 17,
    /// Extended section indices for a symbol table.
    SHT_SYMTAB_SHNDX = 18,
    /// Relocation entries; only offsets.
    SHT_RELR = 19,
    /// Experimental CREL relocations. LLVM will change the value and
    /// break compatibility in the future.
    SHT_CREL = 0x40000014,
    /// Android-specific compressed version of `SHT_REL`.
    SHT_ANDROID_REL = 0x60000001,
    /// Android-specific compressed version of `SHT_RELA`.
    SHT_ANDROID_RELA = 0x60000002,
    /// LLVM-style dependent libraries.
    SHT_LLVM_DEPENDENT_LIBRARIES = 0x6fff4c04,
    /// Android-specific precursor of `SHT_RELR`; differs only by constants and required API level.
    SHT_ANDROID_RELR = 0x6fff_ff00,
    /// GNU SFrame stack trace format.
    SHT_GNU_SFRAME = 0x6fff_fff4,
    /// Object attributes.
    SHT_GNU_ATTRIBUTES = 0x6fff_fff5,
    /// GNU-style hash table.
    SHT_GNU_HASH = 0x6fff_fff6,
    /// Prelink library list
    SHT_GNU_LIBLIST = 0x6fff_fff7,
    /// Checksum for DSO content.
    SHT_CHECKSUM = 0x6fff_fff8,
    #[allow(non_upper_case_globals)]
    SHT_SUNW_move = 0x6fff_fffa,
    SHT_SUNW_COMDAT = 0x6fff_fffb,
    #[allow(non_upper_case_globals)]
    SHT_SUNW_syminfo = 0x6fff_fffc,
    /// Version definition section.
    SHT_GNU_VERDEF = 0x6fff_fffd,
    /// Version needs section.
    SHT_GNU_VERNEED = 0x6fff_fffe,
    /// Version symbol table.
    SHT_GNU_VERSYM = 0x6fff_ffff,
});

/// Start of OS-specific section types.
pub const SHT_LOOS: u32 = 0x6000_0000;
/// Sun-specific low bound.
pub const SHT_LOSUNW: u32 = 0x6fff_fffa;
/// Sun-specific high bound.
pub const SHT_HISUNW: u32 = 0x6fff_ffff;
/// End of OS-specific section types.
pub const SHT_HIOS: u32 = 0x6fff_ffff;
/// Start of processor-specific section types.
pub const SHT_LOPROC: u32 = 0x7000_0000;
/// End of processor-specific section types.
pub const SHT_HIPROC: u32 = 0x7fff_ffff;
/// Start of application-specific section types.
pub const SHT_LOUSER: u32 = 0x8000_0000;
/// End of application-specific section types.
pub const SHT_HIUSER: u32 = 0x8fff_ffff;

newtype!(
    /// Values for `SectionHeader*::sh_flags`.
    struct SectionFlags(u64);
);

impl SectionFlags {
    /// Returns OS-specific flags.
    pub fn os_bits(self) -> Self {
        SectionFlags(self.0 & SHF_MASKOS)
    }

    /// Returns processor-specific flags.
    pub fn proc_bits(self) -> Self {
        SectionFlags(self.0 & SHF_MASKPROC)
    }
}

newtype_flag_names!(NAMES_SHF: SectionFlags(u64) = {
    /// Section is writable.
    SHF_WRITE = 1 << 0,
    /// Section occupies memory during execution.
    SHF_ALLOC = 1 << 1,
    /// Section is executable.
    SHF_EXECINSTR = 1 << 2,
    /// Section may be be merged to eliminate duplication.
    SHF_MERGE = 1 << 4,
    /// Section contains nul-terminated strings.
    SHF_STRINGS = 1 << 5,
    /// The `sh_info` field contains a section header table index.
    SHF_INFO_LINK = 1 << 6,
    /// Section has special ordering requirements when combining sections.
    SHF_LINK_ORDER = 1 << 7,
    /// Section requires special OS-specific handling.
    SHF_OS_NONCONFORMING = 1 << 8,
    /// Section is a member of a group.
    SHF_GROUP = 1 << 9,
    /// Section holds thread-local storage.
    SHF_TLS = 1 << 10,
    /// Section is compressed.
    ///
    /// Compressed sections begin with one of the `CompressionHeader*` headers.
    SHF_COMPRESSED = 1 << 11,
    /// Section should not be garbage collected by the linker.
    SHF_GNU_RETAIN = 1 << 21,
    /// Mbind section.
    SHF_GNU_MBIND = 1 << 24,
    /// This section is excluded from the final executable or shared library.
    SHF_EXCLUDE = 0x8000_0000,
});

/// OS-specific section flags.
pub const SHF_MASKOS: u64 = 0x0ff0_0000;
/// Processor-specific section flags.
pub const SHF_MASKPROC: u64 = 0xf000_0000;

/// Section compression header.
///
/// Used when `SHF_COMPRESSED` is set.
///
/// Note: this type currently allows for misaligned headers, but that may be
/// changed in a future version.
#[derive(Debug, Default, Clone, Copy)]
#[repr(C)]
pub struct CompressionHeader32<E: Endian> {
    /// Compression format. One of the `ELFCOMPRESS_*` values.
    pub ch_type: U32<E, CompressionType>,
    /// Uncompressed data size.
    pub ch_size: U32<E>,
    /// Uncompressed data alignment.
    pub ch_addralign: U32<E>,
}

/// Section compression header.
///
/// Used when `SHF_COMPRESSED` is set.
///
/// Note: this type currently allows for misaligned headers, but that may be
/// changed in a future version.
#[derive(Debug, Default, Clone, Copy)]
#[repr(C)]
pub struct CompressionHeader64<E: Endian> {
    /// Compression format. One of the `ELFCOMPRESS_*` values.
    pub ch_type: U32<E, CompressionType>,
    /// Reserved.
    pub ch_reserved: U32<E>,
    /// Uncompressed data size.
    pub ch_size: U64<E>,
    /// Uncompressed data alignment.
    pub ch_addralign: U64<E>,
}

newtype!(
    /// Values for `CompressionHeader*::ch_type`.
    struct CompressionType(u32);
);

impl CompressionType {
    /// Return true if the type is in the OS-specific range.
    pub fn is_os(self) -> bool {
        self.0 >= ELFCOMPRESS_LOOS && self.0 <= ELFCOMPRESS_HIOS
    }
    /// Return true if the type is in the processor-specific range.
    pub fn is_proc(self) -> bool {
        self.0 >= ELFCOMPRESS_LOPROC && self.0 <= ELFCOMPRESS_HIPROC
    }
}

newtype_constant_names!(NAMES_COMPRESS: CompressionType(u32) = {
    /// ZLIB/DEFLATE algorithm.
    ELFCOMPRESS_ZLIB = 1,
    /// Zstandard algorithm.
    ELFCOMPRESS_ZSTD = 2,
});

/// Start of OS-specific compression types.
pub const ELFCOMPRESS_LOOS: u32 = 0x6000_0000;
/// End of OS-specific compression types.
pub const ELFCOMPRESS_HIOS: u32 = 0x6fff_ffff;
/// Start of processor-specific compression types.
pub const ELFCOMPRESS_LOPROC: u32 = 0x7000_0000;
/// End of processor-specific compression types.
pub const ELFCOMPRESS_HIPROC: u32 = 0x7fff_ffff;

newtype!(
    /// Values for the flag entry for section groups.
    struct GroupFlags(u32);
);

newtype_flag_names!(NAMES_GRP: GroupFlags(u32) = {
    /// Mark group as COMDAT.
    GRP_COMDAT = 1,
});

/// Symbol table entry.
#[derive(Debug, Default, Clone, Copy)]
#[repr(C)]
pub struct Sym32<E: Endian> {
    /// Symbol name.
    ///
    /// This is an offset into the symbol string table.
    pub st_name: U32<E>,
    /// Symbol value.
    pub st_value: U32<E>,
    /// Symbol size.
    pub st_size: U32<E>,
    /// Symbol type and binding.
    ///
    /// Use the `st_type` and `st_bind` methods to access this value.
    pub st_info: SymbolInfo,
    /// Symbol visibility.
    ///
    /// Use the `st_visibility` method to access this value.
    pub st_other: SymbolOther,
    /// Section index or one of the `SHN_*` values.
    pub st_shndx: U16<E, SymbolSection>,
}

impl<E: Endian> Sym32<E> {
    /// Get the `st_bind` subfield of the `st_info` field.
    #[inline]
    pub fn st_bind(&self) -> SymbolBind {
        self.st_info.st_bind()
    }

    /// Get the `st_type` subfield of the `st_info` field.
    #[inline]
    pub fn st_type(&self) -> SymbolType {
        self.st_info.st_type()
    }

    /// Set the `st_info` field given the `st_bind` and `st_type` subfields.
    #[inline]
    pub fn set_st_info(&mut self, st_bind: SymbolBind, st_type: SymbolType) {
        self.st_info = SymbolInfo::new(st_bind, st_type);
    }

    /// Get the `st_visibility` subfield of the `st_other` field.
    #[inline]
    pub fn st_visibility(&self) -> SymbolVisibility {
        self.st_other.visibility()
    }
}

/// Symbol table entry.
#[derive(Debug, Default, Clone, Copy)]
#[repr(C)]
pub struct Sym64<E: Endian> {
    /// Symbol name.
    ///
    /// This is an offset into the symbol string table.
    pub st_name: U32<E>,
    /// Symbol type and binding.
    ///
    /// Use the `st_bind` and `st_type` methods to access this value.
    pub st_info: SymbolInfo,
    /// Symbol visibility.
    ///
    /// Use the `st_visibility` method to access this value.
    pub st_other: SymbolOther,
    /// Section index or one of the `SHN_*` values.
    pub st_shndx: U16<E, SymbolSection>,
    /// Symbol value.
    pub st_value: U64<E>,
    /// Symbol size.
    pub st_size: U64<E>,
}

impl<E: Endian> Sym64<E> {
    /// Get the `st_bind` subfield of the `st_info` field.
    #[inline]
    pub fn st_bind(&self) -> SymbolBind {
        self.st_info.st_bind()
    }

    /// Get the `st_type` subfield of the `st_info` field.
    #[inline]
    pub fn st_type(&self) -> SymbolType {
        self.st_info.st_type()
    }

    /// Set the `st_info` field given the `st_bind` and `st_type` subfields.
    #[inline]
    pub fn set_st_info(&mut self, st_bind: SymbolBind, st_type: SymbolType) {
        self.st_info = SymbolInfo::new(st_bind, st_type);
    }

    /// Get the `st_visibility` subfield of the `st_other` field.
    #[inline]
    pub fn st_visibility(&self) -> SymbolVisibility {
        self.st_other.visibility()
    }
}

/// Additional information about a `Sym32`.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Syminfo32<E: Endian> {
    /// Direct bindings, symbol bound to.
    pub si_boundto: U16<E, SyminfoBoundto>,
    /// Per symbol flags.
    pub si_flags: U16<E, SyminfoFlags>,
}

/// Additional information about a `Sym64`.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Syminfo64<E: Endian> {
    /// Direct bindings, symbol bound to.
    pub si_boundto: U16<E, SyminfoBoundto>,
    /// Per symbol flags.
    pub si_flags: U16<E, SyminfoFlags>,
}

newtype!(
    /// Values for `Syminfo*::si_boundto`.
    struct SyminfoBoundto(u16);
);

impl SyminfoBoundto {
    /// Return true if the value is in the reserved range.
    pub fn is_reserved(self) -> bool {
        self.0 >= SYMINFO_BT_LOWRESERVE
    }

    /// Return the symbol index, or `None` if it is a reserved value.
    pub fn index(self) -> Option<u16> {
        if self.is_reserved() {
            None
        } else {
            Some(self.0)
        }
    }

    /// Values for `Syminfo*::si_boundto` for the first entry.
    #[cfg(feature = "names")]
    pub fn version_names() -> &'static ConstantNames<SyminfoBoundto> {
        &NAMES_SYMINFO_VERSION
    }
}

newtype_constant_names!(NAMES_SYMINFO_BT: SyminfoBoundto(u16) = {
    /// Symbol bound to self
    SYMINFO_BT_SELF = 0xffff,
    /// Symbol bound to parent
    SYMINFO_BT_PARENT = 0xfffe,
});

/// Beginning of reserved entries
pub const SYMINFO_BT_LOWRESERVE: u16 = 0xff00;

newtype!(
    /// Values for `Syminfo*::si_flags`.
    struct SyminfoFlags(u16);
);

newtype_flag_names!(NAMES_SYMINFO_FLG: SyminfoFlags(u16) = {
    /// Direct bound symbol
    SYMINFO_FLG_DIRECT = 0x0001,
    /// Pass-thru symbol for translator
    SYMINFO_FLG_PASSTHRU = 0x0002,
    /// Symbol is a copy-reloc
    SYMINFO_FLG_COPY = 0x0004,
    /// Symbol bound to object to be lazy loaded
    SYMINFO_FLG_LAZYLOAD = 0x0008,
});

constant_names!(NAMES_SYMINFO_VERSION: SyminfoBoundto(u16) = {
    SYMINFO_NONE = 0,
    SYMINFO_CURRENT = 1,
    SYMINFO_NUM = 2,
});

newtype!(
    /// Values for `Sym*::st_info`.
    #[repr(transparent)]
    struct SymbolInfo(u8);
);

impl core::fmt::Debug for SymbolInfo {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "{:?} | {:?}", self.st_bind(), self.st_type())
    }
}

impl SymbolInfo {
    /// Construct a value for the `st_info` field given the `st_bind` and `st_type` fields.
    pub fn new(st_bind: SymbolBind, st_type: SymbolType) -> Self {
        SymbolInfo((st_bind.0 << 4) | (st_type.0 & 0xf))
    }

    /// Get the `st_bind` field.
    pub fn st_bind(self) -> SymbolBind {
        SymbolBind(self.0 >> 4)
    }

    /// Get the `st_type` field.
    pub fn st_type(self) -> SymbolType {
        SymbolType(self.0 & 0xf)
    }
}

impl core::ops::BitOr<SymbolBind> for SymbolType {
    type Output = SymbolInfo;
    fn bitor(self, st_bind: SymbolBind) -> Self::Output {
        SymbolInfo::new(st_bind, self)
    }
}

impl core::ops::BitOr<SymbolType> for SymbolBind {
    type Output = SymbolInfo;
    fn bitor(self, st_type: SymbolType) -> Self::Output {
        SymbolInfo::new(self, st_type)
    }
}

newtype!(
    /// Values for `st_bind` field of `Sym*::st_info`.
    struct SymbolBind(u8);
);

impl SymbolBind {
    /// Return true if the binding is in the OS-specific range.
    pub fn is_os(self) -> bool {
        self.0 >= STB_LOOS && self.0 <= STB_HIOS
    }
    /// Return true if the binding is in the processor-specific range.
    pub fn is_proc(self) -> bool {
        self.0 >= STB_LOPROC && self.0 <= STB_HIPROC
    }
}

newtype_constant_names!(NAMES_STB: SymbolBind(u8) = {
    /// Local symbol.
    STB_LOCAL = 0,
    /// Global symbol.
    STB_GLOBAL = 1,
    /// Weak symbol.
    STB_WEAK = 2,
    /// Unique symbol.
    STB_GNU_UNIQUE = 10,
});

/// Start of OS-specific symbol binding.
pub const STB_LOOS: u8 = 10;
/// End of OS-specific symbol binding.
pub const STB_HIOS: u8 = 12;
/// Start of processor-specific symbol binding.
pub const STB_LOPROC: u8 = 13;
/// End of processor-specific symbol binding.
pub const STB_HIPROC: u8 = 15;

newtype!(
    /// Values for `st_type` field of `Sym*::st_info`.
    struct SymbolType(u8);
);

impl SymbolType {
    /// Return true if the type is in the OS-specific range.
    pub fn is_os(self) -> bool {
        self.0 >= STT_LOOS && self.0 <= STT_HIOS
    }
    /// Return true if the type is in the processor-specific range.
    pub fn is_proc(self) -> bool {
        self.0 >= STT_LOPROC && self.0 <= STT_HIPROC
    }
}

newtype_constant_names!(NAMES_STT: SymbolType(u8) = {
    /// Symbol type is unspecified.
    STT_NOTYPE = 0,
    /// Symbol is a data object.
    STT_OBJECT = 1,
    /// Symbol is a code object.
    STT_FUNC = 2,
    /// Symbol is associated with a section.
    STT_SECTION = 3,
    /// Symbol's name is a file name.
    STT_FILE = 4,
    /// Symbol is a common data object.
    STT_COMMON = 5,
    /// Symbol is a thread-local storage object.
    STT_TLS = 6,
    /// Symbol is an indirect code object.
    STT_GNU_IFUNC = 10,
});

/// Start of OS-specific symbol types.
pub const STT_LOOS: u8 = 10;
/// End of OS-specific symbol types.
pub const STT_HIOS: u8 = 12;
/// Start of processor-specific symbol types.
pub const STT_LOPROC: u8 = 13;
/// End of processor-specific symbol types.
pub const STT_HIPROC: u8 = 15;

newtype!(
    /// Values for `Sym*::st_other`.
    #[repr(transparent)]
    struct SymbolOther(u8);
);

impl SymbolOther {
    /// Get the `st_visibility` field.
    pub fn visibility(self) -> SymbolVisibility {
        SymbolVisibility(self.0 & STV_MASK)
    }

    /// Set the `st_visibility` field.
    pub fn with_visibility(self, vis: SymbolVisibility) -> Self {
        SymbolOther(self.0 & !STV_MASK | vis.0 & STV_MASK)
    }

    /// Get the PPC64 `local` field.
    pub fn ppc64_local(self) -> u8 {
        (self.0 & STO_PPC64_LOCAL_MASK) >> STO_PPC64_LOCAL_BIT
    }

    /// Set the PPC64 `local` field.
    pub fn ppc64_with_local(self, local: u8) -> Self {
        SymbolOther(
            self.0 & !STO_PPC64_LOCAL_MASK | (local << STO_PPC64_LOCAL_BIT) & STO_PPC64_LOCAL_MASK,
        )
    }
}

newtype_flag_names!(NAMES_STO: SymbolOther(u8) = {
    STV_MASK = 3 => NAMES_STV,
});

newtype!(
    struct SymbolVisibility(u8);
);

impl From<SymbolVisibility> for SymbolOther {
    fn from(value: SymbolVisibility) -> Self {
        SymbolOther(value.0 & STV_MASK)
    }
}

impl From<SymbolOther> for SymbolVisibility {
    fn from(value: SymbolOther) -> Self {
        value.visibility()
    }
}

newtype_constant_names!(NAMES_STV: SymbolVisibility(u8) = {
    /// Default symbol visibility rules.
    STV_DEFAULT = 0,
    /// Processor specific hidden class.
    STV_INTERNAL = 1,
    /// Symbol is not visible to other components.
    STV_HIDDEN = 2,
    /// Symbol is visible to other components, but is not preemptible.
    STV_PROTECTED = 3,
});

/// Relocation table entry without explicit addend.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Rel32<E: Endian> {
    /// Relocation address.
    pub r_offset: U32<E>,
    /// Relocation type and symbol index.
    pub r_info: U32<E>,
}

impl<E: Endian> Rel32<E> {
    /// Get the `r_sym` subfield of the `r_info` field.
    #[inline]
    pub fn r_sym(&self, endian: E) -> u32 {
        self.r_info.get(endian) >> 8
    }

    /// Get the `r_type` subfield of the `r_info` field.
    #[inline]
    pub fn r_type(&self, endian: E) -> RelocationType {
        RelocationType(self.r_info.get(endian) & 0xff)
    }

    /// Calculate the `r_info` field given the `r_sym` and `r_type` subfields.
    ///
    /// No overflow check is performed when truncating `r_sym` and `r_type`.
    pub fn r_info(endian: E, r_sym: u32, r_type: RelocationType) -> U32<E> {
        U32::new(endian, (r_sym << 8) | (r_type.0 & 0xff))
    }

    /// Set the `r_info` field given the `r_sym` and `r_type` subfields.
    ///
    /// No overflow check is performed when truncating `r_sym` and `r_type`.
    pub fn set_r_info(&mut self, endian: E, r_sym: u32, r_type: RelocationType) {
        self.r_info = Self::r_info(endian, r_sym, r_type)
    }
}

/// Relocation table entry with explicit addend.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Rela32<E: Endian> {
    /// Relocation address.
    pub r_offset: U32<E>,
    /// Relocation type and symbol index.
    pub r_info: U32<E>,
    /// Explicit addend.
    pub r_addend: I32<E>,
}

impl<E: Endian> Rela32<E> {
    /// Get the `r_sym` subfield of the `r_info` field.
    #[inline]
    pub fn r_sym(&self, endian: E) -> u32 {
        self.r_info.get(endian) >> 8
    }

    /// Get the `r_type` subfield of the `r_info` field.
    #[inline]
    pub fn r_type(&self, endian: E) -> RelocationType {
        RelocationType(self.r_info.get(endian) & 0xff)
    }

    /// Calculate the `r_info` field given the `r_sym` and `r_type` subfields.
    ///
    /// No overflow check is performed when truncating `r_sym` and `r_type`.
    pub fn r_info(endian: E, r_sym: u32, r_type: RelocationType) -> U32<E> {
        U32::new(endian, (r_sym << 8) | u32::from(r_type.0 as u8))
    }

    /// Set the `r_info` field given the `r_sym` and `r_type` subfields.
    ///
    /// No overflow check is performed when truncating `r_sym` and `r_type`.
    pub fn set_r_info(&mut self, endian: E, r_sym: u32, r_type: RelocationType) {
        self.r_info = Self::r_info(endian, r_sym, r_type)
    }
}

impl<E: Endian> From<Rel32<E>> for Rela32<E> {
    fn from(rel: Rel32<E>) -> Self {
        Rela32 {
            r_offset: rel.r_offset,
            r_info: rel.r_info,
            r_addend: I32::default(),
        }
    }
}

/// Relocation table entry without explicit addend.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Rel64<E: Endian> {
    /// Relocation address.
    pub r_offset: U64<E>,
    /// Relocation type and symbol index.
    pub r_info: U64<E>,
}

impl<E: Endian> Rel64<E> {
    /// Get the `r_sym` subfield of the `r_info` field.
    #[inline]
    pub fn r_sym(&self, endian: E) -> u32 {
        (self.r_info.get(endian) >> 32) as u32
    }

    /// Get the `r_type` subfield of the `r_info` field.
    #[inline]
    pub fn r_type(&self, endian: E) -> RelocationType {
        RelocationType((self.r_info.get(endian) & 0xffff_ffff) as u32)
    }

    /// Calculate the `r_info` field given the `r_sym` and `r_type` subfields.
    pub fn r_info(endian: E, r_sym: u32, r_type: RelocationType) -> U64<E> {
        U64::new(endian, (u64::from(r_sym) << 32) | u64::from(r_type.0))
    }

    /// Set the `r_info` field given the `r_sym` and `r_type` subfields.
    pub fn set_r_info(&mut self, endian: E, r_sym: u32, r_type: RelocationType) {
        self.r_info = Self::r_info(endian, r_sym, r_type)
    }
}

impl<E: Endian> From<Rel64<E>> for Rela64<E> {
    fn from(rel: Rel64<E>) -> Self {
        Rela64 {
            r_offset: rel.r_offset,
            r_info: rel.r_info,
            r_addend: I64::default(),
        }
    }
}

/// Relocation table entry with explicit addend.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Rela64<E: Endian> {
    /// Relocation address.
    pub r_offset: U64<E>,
    /// Relocation type and symbol index.
    pub r_info: U64<E>,
    /// Explicit addend.
    pub r_addend: I64<E>,
}

impl<E: Endian> Rela64<E> {
    pub(crate) fn get_r_info(&self, endian: E, is_mips64el: bool) -> u64 {
        let mut t = self.r_info.get(endian);
        if is_mips64el {
            t = (t << 32)
                | ((t >> 8) & 0xff000000)
                | ((t >> 24) & 0x00ff0000)
                | ((t >> 40) & 0x0000ff00)
                | ((t >> 56) & 0x000000ff);
        }
        t
    }

    /// Get the `r_sym` subfield of the `r_info` field.
    #[inline]
    pub fn r_sym(&self, endian: E, is_mips64el: bool) -> u32 {
        (self.get_r_info(endian, is_mips64el) >> 32) as u32
    }

    /// Get the `r_type` subfield of the `r_info` field.
    #[inline]
    pub fn r_type(&self, endian: E, is_mips64el: bool) -> RelocationType {
        RelocationType((self.get_r_info(endian, is_mips64el) & 0xffff_ffff) as u32)
    }

    /// Calculate the `r_info` field given the `r_sym` and `r_type` subfields.
    pub fn r_info(endian: E, is_mips64el: bool, r_sym: u32, r_type: RelocationType) -> U64<E> {
        let mut t = (u64::from(r_sym) << 32) | u64::from(r_type.0);
        if is_mips64el {
            t = (t >> 32)
                | ((t & 0xff000000) << 8)
                | ((t & 0x00ff0000) << 24)
                | ((t & 0x0000ff00) << 40)
                | ((t & 0x000000ff) << 56);
        }
        U64::new(endian, t)
    }

    /// Set the `r_info` field given the `r_sym` and `r_type` subfields.
    pub fn set_r_info(&mut self, endian: E, is_mips64el: bool, r_sym: u32, r_type: RelocationType) {
        self.r_info = Self::r_info(endian, is_mips64el, r_sym, r_type);
    }
}

/// 32-bit relative relocation table entry.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Relr32<E: Endian>(pub U32<E>);

/// 64-bit relative relocation table entry.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Relr64<E: Endian>(pub U64<E>);

newtype!(
    /// Values for `Rel*::r_type`.
    struct RelocationType(u32);
);

newtype_constant_names!(RelocationType(u32) = {});

/// Program segment header.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ProgramHeader32<E: Endian> {
    /// Segment type. One of the `PT_*` constants.
    pub p_type: U32<E, ProgramType>,
    /// Segment file offset.
    pub p_offset: U32<E>,
    /// Segment virtual address.
    pub p_vaddr: U32<E>,
    /// Segment physical address.
    pub p_paddr: U32<E>,
    /// Segment size in the file.
    pub p_filesz: U32<E>,
    /// Segment size in memory.
    pub p_memsz: U32<E>,
    /// Segment flags. A combination of the `PF_*` constants.
    pub p_flags: U32<E, ProgramFlags>,
    /// Segment alignment.
    pub p_align: U32<E>,
}

/// Program segment header.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ProgramHeader64<E: Endian> {
    /// Segment type. One of the `PT_*` constants.
    pub p_type: U32<E, ProgramType>,
    /// Segment flags. A combination of the `PF_*` constants.
    pub p_flags: U32<E, ProgramFlags>,
    /// Segment file offset.
    pub p_offset: U64<E>,
    /// Segment virtual address.
    pub p_vaddr: U64<E>,
    /// Segment physical address.
    pub p_paddr: U64<E>,
    /// Segment size in the file.
    pub p_filesz: U64<E>,
    /// Segment size in memory.
    pub p_memsz: U64<E>,
    /// Segment alignment.
    pub p_align: U64<E>,
}

/// Special value for `FileHeader*::e_phnum`.
///
/// This indicates that the real number of program headers is too large to fit into e_phnum.
/// Instead the real value is in the field `sh_info` of section 0.
pub const PN_XNUM: u16 = 0xffff;

newtype!(
    /// Values for `ProgramHeader*::p_type`.
    struct ProgramType(u32);
);

impl ProgramType {
    /// Return true if the type is in the OS-specific range.
    pub fn is_os(self) -> bool {
        self.0 >= PT_LOOS && self.0 <= PT_HIOS
    }
    /// Return true if the type is in the processor-specific range.
    pub fn is_proc(self) -> bool {
        self.0 >= PT_LOPROC && self.0 <= PT_HIPROC
    }
}

newtype_constant_names!(NAMES_PT: ProgramType(u32) = {
    /// Program header table entry is unused.
    PT_NULL = 0,
    /// Loadable program segment.
    PT_LOAD = 1,
    /// Dynamic linking information.
    PT_DYNAMIC = 2,
    /// Program interpreter.
    PT_INTERP = 3,
    /// Auxiliary information.
    PT_NOTE = 4,
    /// Reserved.
    PT_SHLIB = 5,
    /// Segment contains the program header table.
    PT_PHDR = 6,
    /// Thread-local storage segment.
    PT_TLS = 7,
    /// GCC `.eh_frame_hdr` segment.
    PT_GNU_EH_FRAME = 0x6474_e550,
    /// Indicates stack executability.
    PT_GNU_STACK = 0x6474_e551,
    /// Read-only after relocation.
    PT_GNU_RELRO = 0x6474_e552,
    /// Segment containing `.note.gnu.property` section.
    PT_GNU_PROPERTY = 0x6474_e553,
    /// GNU SFrame stack trace format.
    PT_GNU_SFRAME = 0x6474_e554,
});

/// Start of OS-specific segment types.
pub const PT_LOOS: u32 = 0x6000_0000;
/// End of OS-specific segment types.
pub const PT_HIOS: u32 = 0x6fff_ffff;
/// Start of processor-specific segment types.
pub const PT_LOPROC: u32 = 0x7000_0000;
/// End of processor-specific segment types.
pub const PT_HIPROC: u32 = 0x7fff_ffff;

newtype!(
    /// Values for `ProgramHeader*::p_flags`.
    struct ProgramFlags(u32);
);

newtype_flag_names!(NAMES_PF: ProgramFlags(u32) = {
    /// Segment is executable.
    PF_X = 1 << 0,
    /// Segment is writable.
    PF_W = 1 << 1,
    /// Segment is readable.
    PF_R = 1 << 2,
});

/// OS-specific segment flags.
pub const PF_MASKOS: u32 = 0x0ff0_0000;
/// Processor-specific segment flags.
pub const PF_MASKPROC: u32 = 0xf000_0000;

/// Note name for core files.
pub const ELF_NOTE_CORE: &[u8] = b"CORE";
/// Note name for linux core files.
///
/// Notes in linux core files may also use `ELF_NOTE_CORE`.
pub const ELF_NOTE_LINUX: &[u8] = b"LINUX";

// Values for `NoteHeader*::n_type` in core files.
constant_names!(NAMES_NT_CORE: NoteType(u32) = {
    /// Contains copy of prstatus struct.
    NT_PRSTATUS = 1,
    /// Contains copy of fpregset struct.
    NT_PRFPREG = 2,
    /// Contains copy of fpregset struct.
    NT_FPREGSET = 2,
    /// Contains copy of prpsinfo struct.
    NT_PRPSINFO = 3,
    /// Contains copy of prxregset struct.
    NT_PRXREG = 4,
    /// Contains copy of task structure.
    NT_TASKSTRUCT = 4,
    /// String from sysinfo(SI_PLATFORM).
    NT_PLATFORM = 5,
    /// Contains copy of auxv array.
    NT_AUXV = 6,
    /// Contains copy of gwindows struct.
    NT_GWINDOWS = 7,
    /// Contains copy of asrset struct.
    NT_ASRS = 8,
    /// Contains copy of pstatus struct.
    NT_PSTATUS = 10,
    /// Contains copy of psinfo struct.
    NT_PSINFO = 13,
    /// Contains copy of prcred struct.
    NT_PRCRED = 14,
    /// Contains copy of utsname struct.
    NT_UTSNAME = 15,
    /// Contains copy of lwpstatus struct.
    NT_LWPSTATUS = 16,
    /// Contains copy of lwpinfo struct.
    NT_LWPSINFO = 17,
    /// Contains copy of fprxregset struct.
    NT_PRFPXREG = 20,
    /// Contains copy of siginfo_t, size might increase.
    NT_SIGINFO = 0x5349_4749,
    /// Contains information about mapped files.
    NT_FILE = 0x4649_4c45,
    /// Contains copy of user_fxsr_struct.
    NT_PRXFPREG = 0x46e6_2b7f,
    /// PowerPC Altivec/VMX registers.
    NT_PPC_VMX = 0x100,
    /// PowerPC SPE/EVR registers.
    NT_PPC_SPE = 0x101,
    /// PowerPC VSX registers.
    NT_PPC_VSX = 0x102,
    /// Target Address Register.
    NT_PPC_TAR = 0x103,
    /// Program Priority Register.
    NT_PPC_PPR = 0x104,
    /// Data Stream Control Register.
    NT_PPC_DSCR = 0x105,
    /// Event Based Branch Registers.
    NT_PPC_EBB = 0x106,
    /// Performance Monitor Registers.
    NT_PPC_PMU = 0x107,
    /// TM checkpointed GPR Registers.
    NT_PPC_TM_CGPR = 0x108,
    /// TM checkpointed FPR Registers.
    NT_PPC_TM_CFPR = 0x109,
    /// TM checkpointed VMX Registers.
    NT_PPC_TM_CVMX = 0x10a,
    /// TM checkpointed VSX Registers.
    NT_PPC_TM_CVSX = 0x10b,
    /// TM Special Purpose Registers.
    NT_PPC_TM_SPR = 0x10c,
    /// TM checkpointed Target Address Register.
    NT_PPC_TM_CTAR = 0x10d,
    /// TM checkpointed Program Priority Register.
    NT_PPC_TM_CPPR = 0x10e,
    /// TM checkpointed Data Stream Control Register.
    NT_PPC_TM_CDSCR = 0x10f,
    /// Memory Protection Keys registers.
    NT_PPC_PKEY = 0x110,
    /// i386 TLS slots (struct user_desc).
    NT_386_TLS = 0x200,
    /// x86 io permission bitmap (1=deny).
    NT_386_IOPERM = 0x201,
    /// x86 extended state using xsave.
    NT_X86_XSTATE = 0x202,
    /// s390 upper register halves.
    NT_S390_HIGH_GPRS = 0x300,
    /// s390 timer register.
    NT_S390_TIMER = 0x301,
    /// s390 TOD clock comparator register.
    NT_S390_TODCMP = 0x302,
    /// s390 TOD programmable register.
    NT_S390_TODPREG = 0x303,
    /// s390 control registers.
    NT_S390_CTRS = 0x304,
    /// s390 prefix register.
    NT_S390_PREFIX = 0x305,
    /// s390 breaking event address.
    NT_S390_LAST_BREAK = 0x306,
    /// s390 system call restart data.
    NT_S390_SYSTEM_CALL = 0x307,
    /// s390 transaction diagnostic block.
    NT_S390_TDB = 0x308,
    /// s390 vector registers 0-15 upper half.
    NT_S390_VXRS_LOW = 0x309,
    /// s390 vector registers 16-31.
    NT_S390_VXRS_HIGH = 0x30a,
    /// s390 guarded storage registers.
    NT_S390_GS_CB = 0x30b,
    /// s390 guarded storage broadcast control block.
    NT_S390_GS_BC = 0x30c,
    /// s390 runtime instrumentation.
    NT_S390_RI_CB = 0x30d,
    /// ARM VFP/NEON registers.
    NT_ARM_VFP = 0x400,
    /// ARM TLS register.
    NT_ARM_TLS = 0x401,
    /// ARM hardware breakpoint registers.
    NT_ARM_HW_BREAK = 0x402,
    /// ARM hardware watchpoint registers.
    NT_ARM_HW_WATCH = 0x403,
    /// ARM system call number.
    NT_ARM_SYSTEM_CALL = 0x404,
    /// ARM Scalable Vector Extension registers.
    NT_ARM_SVE = 0x405,
    /// Vmcore Device Dump Note.
    NT_VMCOREDD = 0x700,
    /// MIPS DSP ASE registers.
    NT_MIPS_DSP = 0x800,
    /// MIPS floating-point mode.
    NT_MIPS_FP_MODE = 0x801,
});

/// Note type for version string.
///
/// This note may appear in object files.
///
/// It must be handled as a special case because it has no descriptor, and instead
/// uses the note name as the version string.
pub const NT_VERSION: u32 = 1;

/// Dynamic section entry.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Dyn32<E: Endian> {
    /// Dynamic entry type.
    pub d_tag: I32<E, DynamicTag>,
    /// Value (integer or address).
    pub d_val: U32<E>,
}

/// Dynamic section entry.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Dyn64<E: Endian> {
    /// Dynamic entry type.
    pub d_tag: I64<E, DynamicTag>,
    /// Value (integer or address).
    pub d_val: U64<E>,
}

newtype!(
    /// Values for `Dyn*::d_tag`.
    struct DynamicTag(i64);
);

impl DynamicTag {
    /// Return true if the tag is in the range for values.
    ///
    /// Note that some tags outside of this range are also used for values.
    pub fn is_value(self) -> bool {
        self.0 >= DT_VALRNGLO && self.0 <= DT_VALRNGHI
    }
    /// Return true if the tag is in the range for addresses.
    ///
    /// Note that some tags outside of this range are also used for addresses.
    pub fn is_address(self) -> bool {
        self.0 >= DT_ADDRRNGLO && self.0 <= DT_ADDRRNGHI
    }
    /// Return true if the tag is in the OS-specific range.
    pub fn is_os(self) -> bool {
        self.0 >= DT_LOOS && self.0 <= DT_HIOS
    }
    /// Return true if the tag is in the processor-specific range.
    pub fn is_proc(self) -> bool {
        self.0 >= DT_LOPROC && self.0 <= DT_HIPROC
    }
}

newtype_constant_names!(NAMES_DT: DynamicTag(i64) = {
    /// Marks end of dynamic section
    DT_NULL = 0,
    /// Name of needed library
    DT_NEEDED = 1,
    /// Size in bytes of PLT relocs
    DT_PLTRELSZ = 2,
    /// Processor defined value
    DT_PLTGOT = 3,
    /// Address of symbol hash table
    DT_HASH = 4,
    /// Address of string table
    DT_STRTAB = 5,
    /// Address of symbol table
    DT_SYMTAB = 6,
    /// Address of Rela relocs
    DT_RELA = 7,
    /// Total size of Rela relocs
    DT_RELASZ = 8,
    /// Size of one Rela reloc
    DT_RELAENT = 9,
    /// Size of string table
    DT_STRSZ = 10,
    /// Size of one symbol table entry
    DT_SYMENT = 11,
    /// Address of init function
    DT_INIT = 12,
    /// Address of termination function
    DT_FINI = 13,
    /// Name of shared object
    DT_SONAME = 14,
    /// Library search path (deprecated)
    DT_RPATH = 15,
    /// Start symbol search here
    DT_SYMBOLIC = 16,
    /// Address of Rel relocs
    DT_REL = 17,
    /// Total size of Rel relocs
    DT_RELSZ = 18,
    /// Size of one Rel reloc
    DT_RELENT = 19,
    /// Type of reloc in PLT
    DT_PLTREL = 20,
    /// For debugging; unspecified
    DT_DEBUG = 21,
    /// Reloc might modify .text
    DT_TEXTREL = 22,
    /// Address of PLT relocs
    DT_JMPREL = 23,
    /// Process relocations of object
    DT_BIND_NOW = 24,
    /// Array with addresses of init fct
    DT_INIT_ARRAY = 25,
    /// Array with addresses of fini fct
    DT_FINI_ARRAY = 26,
    /// Size in bytes of DT_INIT_ARRAY
    DT_INIT_ARRAYSZ = 27,
    /// Size in bytes of DT_FINI_ARRAY
    DT_FINI_ARRAYSZ = 28,
    /// Library search path
    DT_RUNPATH = 29,
    /// Flags for the object being loaded
    DT_FLAGS = 30,
    /// Start of encoded range
    DT_ENCODING = 32,
    /// Array with addresses of preinit fct
    DT_PREINIT_ARRAY = 32,
    /// size in bytes of DT_PREINIT_ARRAY
    DT_PREINIT_ARRAYSZ = 33,
    /// Address of SYMTAB_SHNDX section
    DT_SYMTAB_SHNDX = 34,
    /// Address of Relr relocs
    DT_RELR = 36,
    /// Total size of Relr relocs
    DT_RELRSZ = 35,
    /// Size of one Relr reloc
    DT_RELRENT = 37,

    /// Address of Android-specific compressed Rel relocs
    DT_ANDROID_REL = 0x6000000f,
    /// Total size of Android-specific compressed Rel relocs
    DT_ANDROID_RELSZ = 0x60000010,
    /// Address of Android-specific compressed Rela relocs
    DT_ANDROID_RELA = 0x60000011,
    /// Total size of Android-specific compressed Rela relocs
    DT_ANDROID_RELASZ = 0x60000012,
    /// Address of Android-specific Relr relocs
    DT_ANDROID_RELR = 0x6fff_e000,
    /// Total size of Android-specific Relr relocs
    DT_ANDROID_RELRSZ = 0x6fff_e001,
    /// Size of one Android-specific Relr reloc
    DT_ANDROID_RELRENT = 0x6fff_e003,

    /// Prelinking timestamp
    DT_GNU_PRELINKED = 0x6fff_fdf5,
    /// Size of conflict section
    DT_GNU_CONFLICTSZ = 0x6fff_fdf6,
    /// Size of library list
    DT_GNU_LIBLISTSZ = 0x6fff_fdf7,
    DT_CHECKSUM = 0x6fff_fdf8,
    DT_PLTPADSZ = 0x6fff_fdf9,
    DT_MOVEENT = 0x6fff_fdfa,
    DT_MOVESZ = 0x6fff_fdfb,
    /// Feature selection (DTF_*).
    DT_FEATURE_1 = 0x6fff_fdfc,
    /// Flags for DT_* entries, affecting the following DT_* entry.
    DT_POSFLAG_1 = 0x6fff_fdfd,
    /// Size of syminfo table (in bytes)
    DT_SYMINSZ = 0x6fff_fdfe,
    /// Entry size of syminfo
    DT_SYMINENT = 0x6fff_fdff,

    /// GNU-style hash table.
    DT_GNU_HASH = 0x6fff_fef5,
    DT_TLSDESC_PLT = 0x6fff_fef6,
    DT_TLSDESC_GOT = 0x6fff_fef7,
    /// Start of conflict section
    DT_GNU_CONFLICT = 0x6fff_fef8,
    /// Library list
    DT_GNU_LIBLIST = 0x6fff_fef9,
    /// Configuration information.
    DT_CONFIG = 0x6fff_fefa,
    /// Dependency auditing.
    DT_DEPAUDIT = 0x6fff_fefb,
    /// Object auditing.
    DT_AUDIT = 0x6fff_fefc,
    /// PLT padding.
    DT_PLTPAD = 0x6fff_fefd,
    /// Move table.
    DT_MOVETAB = 0x6fff_fefe,
    /// Syminfo table.
    DT_SYMINFO = 0x6fff_feff,

    // The versioning entry types.  The next are defined as part of the
    // GNU extension.
    DT_VERSYM = 0x6fff_fff0,
    DT_RELACOUNT = 0x6fff_fff9,
    DT_RELCOUNT = 0x6fff_fffa,
    /// State flags, see DF_1_* below.
    DT_FLAGS_1 = 0x6fff_fffb,
    /// Address of version definition table
    DT_VERDEF = 0x6fff_fffc,
    /// Number of version definitions
    DT_VERDEFNUM = 0x6fff_fffd,
    /// Address of table with needed versions
    DT_VERNEED = 0x6fff_fffe,
    /// Number of needed versions
    DT_VERNEEDNUM = 0x6fff_ffff,

    // Machine-independent extensions in the "processor-specific" range.
    /// Shared object to load before self
    DT_AUXILIARY = 0x7fff_fffd,
    /// Shared object to get values from
    DT_FILTER = 0x7fff_ffff,
});

/// Start of OS-specific
pub const DT_LOOS: i64 = 0x6000_000d;
/// End of OS-specific
pub const DT_HIOS: i64 = 0x6fff_f000;
/// Start of processor-specific
pub const DT_LOPROC: i64 = 0x7000_0000;
/// End of processor-specific
pub const DT_HIPROC: i64 = 0x7fff_ffff;

// `DT_*` entries between `DT_VALRNGHI` & `DT_VALRNGLO` use `d_val` as a value.
pub const DT_VALRNGLO: i64 = 0x6fff_fd00;
pub const DT_VALRNGHI: i64 = 0x6fff_fdff;

// `DT_*` entries between `DT_ADDRRNGHI` & `DT_ADDRRNGLO` use `d_val` as an address.
//
// If any adjustment is made to the ELF object after it has been
// built these entries will need to be adjusted.
pub const DT_ADDRRNGLO: i64 = 0x6fff_fe00;
pub const DT_ADDRRNGHI: i64 = 0x6fff_feff;

newtype!(
    /// Values of `Dyn*::d_val` in the `DT_FLAGS` entry.
    struct DynamicFlags(u64);
);

newtype_flag_names!(NAMES_DF: DynamicFlags(u64) = {
    /// Object may use DF_ORIGIN
    DF_ORIGIN = 0x0000_0001,
    /// Symbol resolutions starts here
    DF_SYMBOLIC = 0x0000_0002,
    /// Object contains text relocations
    DF_TEXTREL = 0x0000_0004,
    /// No lazy binding for this object
    DF_BIND_NOW = 0x0000_0008,
    /// Module uses the static TLS model
    DF_STATIC_TLS = 0x0000_0010,
});

newtype!(
    /// Values of `Dyn*::d_val` in the `DT_FLAGS_1` entry.
    struct DynamicFlags1(u64);
);

newtype_flag_names!(NAMES_DF_1: DynamicFlags1(u64) = {
    /// Set RTLD_NOW for this object.
    DF_1_NOW = 0x0000_0001,
    /// Set RTLD_GLOBAL for this object.
    DF_1_GLOBAL = 0x0000_0002,
    /// Set RTLD_GROUP for this object.
    DF_1_GROUP = 0x0000_0004,
    /// Set RTLD_NODELETE for this object.
    DF_1_NODELETE = 0x0000_0008,
    /// Trigger filtee loading at runtime.
    DF_1_LOADFLTR = 0x0000_0010,
    /// Set RTLD_INITFIRST for this object.
    DF_1_INITFIRST = 0x0000_0020,
    /// Set RTLD_NOOPEN for this object.
    DF_1_NOOPEN = 0x0000_0040,
    /// $ORIGIN must be handled.
    DF_1_ORIGIN = 0x0000_0080,
    /// Direct binding enabled.
    DF_1_DIRECT = 0x0000_0100,
    DF_1_TRANS = 0x0000_0200,
    /// Object is used to interpose.
    DF_1_INTERPOSE = 0x0000_0400,
    /// Ignore default lib search path.
    DF_1_NODEFLIB = 0x0000_0800,
    /// Object can't be dldump'ed.
    DF_1_NODUMP = 0x0000_1000,
    /// Configuration alternative created.
    DF_1_CONFALT = 0x0000_2000,
    /// Filtee terminates filters search.
    DF_1_ENDFILTEE = 0x0000_4000,
    /// Disp reloc applied at build time.
    DF_1_DISPRELDNE = 0x0000_8000,
    /// Disp reloc applied at run-time.
    DF_1_DISPRELPND = 0x0001_0000,
    /// Object has no-direct binding.
    DF_1_NODIRECT = 0x0002_0000,
    DF_1_IGNMULDEF = 0x0004_0000,
    DF_1_NOKSYMS = 0x0008_0000,
    DF_1_NOHDR = 0x0010_0000,
    /// Object is modified after built.
    DF_1_EDITED = 0x0020_0000,
    DF_1_NORELOC = 0x0040_0000,
    /// Object has individual interposers.
    DF_1_SYMINTPOSE = 0x0080_0000,
    /// Global auditing required.
    DF_1_GLOBAUDIT = 0x0100_0000,
    /// Singleton symbols are used.
    DF_1_SINGLETON = 0x0200_0000,
    DF_1_STUB = 0x0400_0000,
    DF_1_PIE = 0x0800_0000,
});

/// Version symbol information
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Versym<E: Endian>(pub U16<E, VersymIndex>);

newtype!(
    /// Version index for a symbol.
    ///
    /// This is a `VersionIndex` plus the `VERSYM_HIDDEN` flag.
    struct VersymIndex(u16);
);

impl VersymIndex {
    /// Return the version index.
    pub fn index(&self) -> VersionIndex {
        VersionIndex(self.0 & VERSYM_VERSION)
    }

    /// Return true if it is the local index.
    pub fn is_local(&self) -> bool {
        self.index() == VER_NDX_LOCAL
    }

    /// Return true if it is the global index.
    pub fn is_global(&self) -> bool {
        self.index() == VER_NDX_GLOBAL
    }

    /// Return the hidden flag.
    pub fn is_hidden(&self) -> bool {
        self.contains(VERSYM_HIDDEN)
    }
}

impl From<VersionIndex> for VersymIndex {
    fn from(value: VersionIndex) -> Self {
        VersymIndex(value.0)
    }
}

impl From<VersymIndex> for VersionIndex {
    fn from(value: VersymIndex) -> Self {
        value.index()
    }
}

newtype_flag_names!(NAMES_VERSYM: VersymIndex(u16) = {
    /// Symbol is hidden.
    VERSYM_HIDDEN = 0x8000,
    /// Symbol version index.
    VERSYM_VERSION = 0x7fff => NAMES_VER_NDX,
});

/// Version definition sections
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Verdef<E: Endian> {
    /// Version revision
    pub vd_version: U16<E>,
    /// Version information
    pub vd_flags: U16<E, VersionFlags>,
    /// Version Index
    pub vd_ndx: U16<E, VersionIndex>,
    /// Number of associated aux entries
    pub vd_cnt: U16<E>,
    /// Version name hash value
    pub vd_hash: U32<E>,
    /// Offset in bytes to verdaux array
    pub vd_aux: U32<E>,
    /// Offset in bytes to next verdef entry
    pub vd_next: U32<E>,
}

// Legal values for vd_version (version revision).
/// No version
pub const VER_DEF_NONE: u16 = 0;
/// Current version
pub const VER_DEF_CURRENT: u16 = 1;

newtype!(
    /// Legal values for vd_flags and vna_flags (version information flags).
    struct VersionFlags(u16);
);

newtype_flag_names!(NAMES_VER_FLG: VersionFlags(u16) = {
    // Legal values for vd_flags (version information flags).
    /// Version definition of file itself
    VER_FLG_BASE = 0x1,
    // Legal values for vd_flags and vna_flags (version information flags).
    /// Weak version identifier
    VER_FLG_WEAK = 0x2,
});

newtype!(
    /// Version index.
    ///
    /// This is the index value stored in [`VersymIndex`], [`Verdef::vd_ndx`] or
    /// [`field@Vernaux::vna_other`].
    struct VersionIndex(u16);
);

impl VersionIndex {
    /// Construct a `VersymIndex` from an index and a hidden flag.
    pub fn versym(self, hidden: bool) -> VersymIndex {
        if hidden {
            VersymIndex(self.0).with(VERSYM_HIDDEN)
        } else {
            VersymIndex(self.0)
        }
    }

    /// Add an offset to the index.
    ///
    /// This is useful when constructing a `VersionIndex` from a base index
    /// such as `VER_NDX_GLOBAL`.
    ///
    /// Panics in debug builds if the resulting index >= `VERSYM_HIDDEN`.
    /// In release builds, the result wraps and the bounds check is skipped.
    pub fn offset(self, offset: u16) -> Self {
        let index = self.0 + offset;
        debug_assert!(index < VERSYM_HIDDEN.0);
        VersionIndex(index)
    }

    /// Add an offset to the index, checking for overflow.
    ///
    /// This is useful when constructing a `VersionIndex` from a base index
    /// such as `VER_NDX_GLOBAL`.
    ///
    /// Returns `None` if the resulting index >= `VERSYM_HIDDEN`.
    pub fn checked_offset(self, offset: u16) -> Option<Self> {
        let index = self.0.checked_add(offset)?;
        if index >= VERSYM_HIDDEN.0 {
            return None;
        }
        Some(VersionIndex(index))
    }

    /// Compute the offset relative to another index.
    ///
    /// This is useful for obtaining an array index relative to a base index
    /// such as `VER_NDX_GLOBAL`.
    ///
    /// Panics in debug builds if `self.0 < base.0`.
    /// In release builds, the result wraps.
    pub fn offset_from(self, base: VersionIndex) -> u16 {
        self.0 - base.0
    }

    /// Compute the offset relative to another index, checking for underflow.
    ///
    /// This is useful for obtaining an array index relative to a base index
    /// such as `VER_NDX_GLOBAL`.
    ///
    /// Returns `None` if `self.0 < base.0`.
    pub fn checked_offset_from(self, base: VersionIndex) -> Option<u16> {
        self.0.checked_sub(base.0)
    }

    /// Return true if it is the local index.
    pub fn is_local(&self) -> bool {
        *self == VER_NDX_LOCAL
    }

    /// Return true if it is the global index.
    pub fn is_global(&self) -> bool {
        *self == VER_NDX_GLOBAL
    }

    /// Return true if it is the local or global index.
    pub fn is_special(&self) -> bool {
        self.0 <= VER_NDX_GLOBAL.0
    }
}

impl core::ops::Add<u16> for VersionIndex {
    type Output = Self;

    fn add(self, rhs: u16) -> Self::Output {
        self.offset(rhs)
    }
}

impl core::ops::AddAssign<u16> for VersionIndex {
    fn add_assign(&mut self, rhs: u16) {
        *self = self.offset(rhs);
    }
}

impl core::ops::Sub for VersionIndex {
    type Output = u16;

    fn sub(self, rhs: VersionIndex) -> Self::Output {
        self.offset_from(rhs)
    }
}

impl From<VersionIndex> for usize {
    fn from(value: VersionIndex) -> Self {
        usize::from(value.0)
    }
}

newtype_constant_names!(NAMES_VER_NDX: VersionIndex(u16) = {
    /// Symbol is local.
    VER_NDX_LOCAL = 0,
    /// Symbol is global.
    VER_NDX_GLOBAL = 1,
});

/// Auxiliary version information.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Verdaux<E: Endian> {
    /// Version or dependency names
    pub vda_name: U32<E>,
    /// Offset in bytes to next verdaux
    pub vda_next: U32<E>,
}

/// Version dependency.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Verneed<E: Endian> {
    /// Version of structure
    pub vn_version: U16<E>,
    /// Number of associated aux entries
    pub vn_cnt: U16<E>,
    /// Offset of filename for this dependency
    pub vn_file: U32<E>,
    /// Offset in bytes to vernaux array
    pub vn_aux: U32<E>,
    /// Offset in bytes to next verneed entry
    pub vn_next: U32<E>,
}

// Legal values for vn_version (version revision).
/// No version
pub const VER_NEED_NONE: u16 = 0;
/// Current version
pub const VER_NEED_CURRENT: u16 = 1;

/// Auxiliary needed version information.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Vernaux<E: Endian> {
    /// Hash value of dependency name
    pub vna_hash: U32<E>,
    /// Dependency specific information
    pub vna_flags: U16<E, VersionFlags>,
    /// Version Index
    ///
    /// LSB documents that this supports `VERSYM_HIDDEN`, but no linker sets it, so we
    /// keep this as a `VersionIndex`. Use [`method@Self::vna_other`] if you wish to handle
    /// the hidden bit when parsing.
    pub vna_other: U16<E, VersionIndex>,
    /// Dependency name string offset
    pub vna_name: U32<E>,
    /// Offset in bytes to next vernaux entry
    pub vna_next: U32<E>,
}

impl<E: Endian> Vernaux<E> {
    /// Return the `vna_other` field as a `VersymIndex`.
    pub fn vna_other(self, endian: E) -> VersymIndex {
        VersymIndex(self.vna_other.get(endian).0)
    }
}

// TODO: Elf*_auxv_t, AT_*

/// Note section entry header.
///
/// A note consists of a header followed by a variable length name and descriptor.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct NoteHeader32<E: Endian> {
    /// Length of the note's name.
    ///
    /// Some known names are defined by the `ELF_NOTE_*` constants.
    pub n_namesz: U32<E>,
    /// Length of the note's descriptor.
    ///
    /// The content of the descriptor depends on the note name and type.
    pub n_descsz: U32<E>,
    /// Type of the note.
    ///
    /// One of the `NT_*` constants. The note name determines which
    /// `NT_*` constants are valid.
    pub n_type: U32<E, NoteType>,
}

/// Note section entry header.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct NoteHeader64<E: Endian> {
    /// Length of the note's name.
    ///
    /// Some known names are defined by the `ELF_NOTE_*` constants.
    pub n_namesz: U32<E>,
    /// Length of the note's descriptor.
    ///
    /// The content of the descriptor depends on the note name and type.
    pub n_descsz: U32<E>,
    /// Type of the note.
    ///
    /// One of the `NT_*` constants. The note name determines which
    /// `NT_*` constants are valid.
    pub n_type: U32<E, NoteType>,
}

newtype!(
    /// Values for `NoteHeader*::n_type`.
    struct NoteType(u32);
);

// Types depend on n_name.
newtype_constant_names!(NoteType(u32) = {});

#[cfg(feature = "names")]
impl NoteType {
    /// Get `NT_*` constants for `NoteHeader*::n_type`.
    ///
    /// `name` is the note name (as indicated by `NoteHeader*::n_namesz`).
    pub fn names(name: &[u8]) -> &'static ConstantNames<NoteType> {
        match name {
            ELF_NOTE_CORE | ELF_NOTE_LINUX => &NAMES_NT_CORE,
            ELF_NOTE_GNU => &NAMES_NT_GNU,
            ELF_NOTE_SOLARIS => &NAMES_NT_SOLARIS,
            ELF_NOTE_GO => &NAMES_NT_GO,
            _ => {
                constant_names!(EMPTY: NoteType = {});
                &EMPTY
            }
        }
    }
}

/// Solaris entries in the note section have this name.
pub const ELF_NOTE_SOLARIS: &[u8] = b"SUNW Solaris";

// Values for `n_type` when the name is `ELF_NOTE_SOLARIS`.
constant_names!(NAMES_NT_SOLARIS: NoteType(u32) = {
    /// Desired pagesize for the binary.
    NT_SOLARIS_PAGESIZE_HINT = 1,
});

/// GNU entries in the note section have this name.
pub const ELF_NOTE_GNU: &[u8] = b"GNU";

/// Go entries in the note section have this name.
// See https://go-review.googlesource.com/9520 and https://go-review.googlesource.com/10704.
pub const ELF_NOTE_GO: &[u8] = b"Go";

// Note types for `ELF_NOTE_GNU`.
constant_names!(NAMES_NT_GNU: NoteType(u32) = {
    /// ABI information.
    ///
    /// The descriptor consists of words:
    /// - word 0: OS descriptor
    /// - word 1: major version of the ABI
    /// - word 2: minor version of the ABI
    /// - word 3: subminor version of the ABI
    NT_GNU_ABI_TAG = 1,
    /// Synthetic hwcap information.
    ///
    /// The descriptor begins with two words:
    /// - word 0: number of entries
    /// - word 1: bitmask of enabled entries
    ///
    /// Then follow variable-length entries, one byte followed by a
    /// '\0'-terminated hwcap name string.  The byte gives the bit
    /// number to test if enabled, (1U << bit) & bitmask.
    NT_GNU_HWCAP = 2,
    /// Build ID bits as generated by `ld --build-id`.
    ///
    /// The descriptor consists of any nonzero number of bytes.
    NT_GNU_BUILD_ID = 3,
    /// Version note generated by GNU gold containing a version string.
    NT_GNU_GOLD_VERSION = 4,
    /// Program property.
    NT_GNU_PROPERTY_TYPE_0 = 5,
});

/// OS descriptor for `NT_GNU_ABI_TAG`.
pub const ELF_NOTE_OS_LINUX: u32 = 0;
/// OS descriptor for `NT_GNU_ABI_TAG`.
pub const ELF_NOTE_OS_GNU: u32 = 1;
/// OS descriptor for `NT_GNU_ABI_TAG`.
pub const ELF_NOTE_OS_SOLARIS2: u32 = 2;
/// OS descriptor for `NT_GNU_ABI_TAG`.
pub const ELF_NOTE_OS_FREEBSD: u32 = 3;

newtype!(
    /// Values used in GNU .note.gnu.property notes (NT_GNU_PROPERTY_TYPE_0).
    struct GnuPropertyType(u32);
);

impl GnuPropertyType {
    /// A 4-byte unsigned integer property: A bit is set if it is set in all
    /// relocatable inputs.
    pub fn is_uint32_and(self) -> bool {
        self.0 >= GNU_PROPERTY_UINT32_AND_LO && self.0 <= GNU_PROPERTY_UINT32_AND_HI
    }
    /// A 4-byte unsigned integer property: A bit is set if it is set in any
    /// relocatable inputs.
    pub fn is_uint32_or(self) -> bool {
        self.0 >= GNU_PROPERTY_UINT32_OR_LO && self.0 <= GNU_PROPERTY_UINT32_OR_HI
    }
    /// Return true if the property is in the processor-specific range.
    pub fn is_proc(self) -> bool {
        self.0 >= GNU_PROPERTY_LOPROC && self.0 <= GNU_PROPERTY_HIPROC
    }
    /// Return true if the property is in the application-specific range.
    pub fn is_user(self) -> bool {
        debug_assert_eq!(GNU_PROPERTY_HIUSER, !0);
        self.0 >= GNU_PROPERTY_LOUSER
    }
    /// A 4-byte unsigned integer property: A bit is set if it is set in all
    /// relocatable inputs.
    pub fn is_x86_uint32_and(self) -> bool {
        self.0 >= GNU_PROPERTY_X86_UINT32_AND_LO && self.0 <= GNU_PROPERTY_X86_UINT32_AND_HI
    }
    /// A 4-byte unsigned integer property: A bit is set if it is set in any
    /// relocatable inputs.
    pub fn is_x86_uint32_or(self) -> bool {
        self.0 >= GNU_PROPERTY_X86_UINT32_OR_LO && self.0 <= GNU_PROPERTY_X86_UINT32_OR_HI
    }
    /// A 4-byte unsigned integer property: A bit is set if it is set in any
    /// relocatable inputs and the property is present in all relocatable
    /// inputs.
    pub fn is_x86_uint32_or_and(self) -> bool {
        self.0 >= GNU_PROPERTY_X86_UINT32_OR_AND_LO && self.0 <= GNU_PROPERTY_X86_UINT32_OR_AND_HI
    }

    /// Return the property type names for the given machine.
    #[cfg(feature = "names")]
    pub fn type_names(machine: Machine) -> &'static ConstantNames<Self> {
        match machine {
            EM_386 | EM_X86_64 => &NAMES_GNU_PROPERTY_X86,
            EM_AARCH64 => &NAMES_GNU_PROPERTY_AARCH64,
            _ => &NAMES_GNU_PROPERTY,
        }
    }

    /// Return the property values names for the given machine if the property
    /// data is a u32.
    #[cfg(feature = "names")]
    pub fn u32_value_names(self, machine: Machine) -> Option<&'static FlagNames<u32>> {
        match self {
            GNU_PROPERTY_1_NEEDED => {
                return Some(&NAMES_GNU_PROPERTY_1_NEEDED);
            }
            _ => {}
        }
        match machine {
            EM_386 | EM_X86_64 => match self {
                GNU_PROPERTY_X86_ISA_1_USED | GNU_PROPERTY_X86_ISA_1_NEEDED => {
                    return Some(&NAMES_GNU_PROPERTY_X86_ISA_1);
                }
                GNU_PROPERTY_X86_FEATURE_1_AND => {
                    return Some(&NAMES_GNU_PROPERTY_X86_FEATURE_1);
                }
                _ => {}
            },
            EM_AARCH64 => match self {
                GNU_PROPERTY_AARCH64_FEATURE_1_AND => {
                    return Some(&NAMES_GNU_PROPERTY_AARCH64_FEATURE_1);
                }
                _ => {}
            },
            _ => {}
        }
        None
    }
}

newtype_constant_names!(NAMES_GNU_PROPERTY: GnuPropertyType(u32) = {
    /// Stack size.
    GNU_PROPERTY_STACK_SIZE = 1,
    /// No copy relocation on protected data symbol.
    GNU_PROPERTY_NO_COPY_ON_PROTECTED = 2,
    /// The needed properties by the object file.  */
    GNU_PROPERTY_1_NEEDED = GNU_PROPERTY_UINT32_OR_LO,
});

flag_names!(NAMES_GNU_PROPERTY_1_NEEDED: u32 = {
    /// Set if the object file requires canonical function pointers and
    /// cannot be used with copy relocation.
    GNU_PROPERTY_1_NEEDED_INDIRECT_EXTERN_ACCESS = 1 << 0,
});

// A 4-byte unsigned integer property: A bit is set if it is set in all
// relocatable inputs.
pub const GNU_PROPERTY_UINT32_AND_LO: u32 = 0xb0000000;
pub const GNU_PROPERTY_UINT32_AND_HI: u32 = 0xb0007fff;

// A 4-byte unsigned integer property: A bit is set if it is set in any
// relocatable inputs.
pub const GNU_PROPERTY_UINT32_OR_LO: u32 = 0xb0008000;
pub const GNU_PROPERTY_UINT32_OR_HI: u32 = 0xb000ffff;

/// Processor-specific semantics, lo
pub const GNU_PROPERTY_LOPROC: u32 = 0xc0000000;
/// Processor-specific semantics, hi
pub const GNU_PROPERTY_HIPROC: u32 = 0xdfffffff;
/// Application-specific semantics, lo
pub const GNU_PROPERTY_LOUSER: u32 = 0xe0000000;
/// Application-specific semantics, hi
pub const GNU_PROPERTY_HIUSER: u32 = 0xffffffff;

constant_names!(NAMES_GNU_PROPERTY_AARCH64: GnuPropertyType(u32) = NAMES_GNU_PROPERTY + {
    GNU_PROPERTY_AARCH64_FEATURE_1_AND = 0xc0000000,
    GNU_PROPERTY_AARCH64_FEATURE_PAUTH = 0xc0000001,
});

flag_names!(NAMES_GNU_PROPERTY_AARCH64_FEATURE_1: u32 = {
    GNU_PROPERTY_AARCH64_FEATURE_1_BTI = 1 << 0,
    GNU_PROPERTY_AARCH64_FEATURE_1_PAC = 1 << 1,
});

// A 4-byte unsigned integer property: A bit is set if it is set in all
// relocatable inputs.
pub const GNU_PROPERTY_X86_UINT32_AND_LO: u32 = 0xc0000002;
pub const GNU_PROPERTY_X86_UINT32_AND_HI: u32 = 0xc0007fff;

// A 4-byte unsigned integer property: A bit is set if it is set in any
// relocatable inputs.
pub const GNU_PROPERTY_X86_UINT32_OR_LO: u32 = 0xc0008000;
pub const GNU_PROPERTY_X86_UINT32_OR_HI: u32 = 0xc000ffff;

// A 4-byte unsigned integer property: A bit is set if it is set in any
// relocatable inputs and the property is present in all relocatable
// inputs.
pub const GNU_PROPERTY_X86_UINT32_OR_AND_LO: u32 = 0xc0010000;
pub const GNU_PROPERTY_X86_UINT32_OR_AND_HI: u32 = 0xc0017fff;

constant_names!(NAMES_GNU_PROPERTY_X86: GnuPropertyType(u32) = NAMES_GNU_PROPERTY + {
    /// The x86 instruction sets indicated by the corresponding bits are
    /// used in program.  Their support in the hardware is optional.
    GNU_PROPERTY_X86_ISA_1_USED = 0xc0010002,
    /// The x86 instruction sets indicated by the corresponding bits are
    /// used in program and they must be supported by the hardware.
    GNU_PROPERTY_X86_ISA_1_NEEDED = 0xc0008002,
    /// X86 processor-specific features used in program.
    GNU_PROPERTY_X86_FEATURE_1_AND = 0xc0000002,
});

flag_names!(NAMES_GNU_PROPERTY_X86_ISA_1: u32 = {
    /// GNU_PROPERTY_X86_ISA_1_BASELINE: CMOV, CX8 (cmpxchg8b), FPU (fld),
    /// MMX, OSFXSR (fxsave), SCE (syscall), SSE and SSE2.
    GNU_PROPERTY_X86_ISA_1_BASELINE = 1 << 0,
    /// GNU_PROPERTY_X86_ISA_1_V2: GNU_PROPERTY_X86_ISA_1_BASELINE,
    /// CMPXCHG16B (cmpxchg16b), LAHF-SAHF (lahf), POPCNT (popcnt), SSE3,
    /// SSSE3, SSE4.1 and SSE4.2.
    GNU_PROPERTY_X86_ISA_1_V2 = 1 << 1,
    /// GNU_PROPERTY_X86_ISA_1_V3: GNU_PROPERTY_X86_ISA_1_V2, AVX, AVX2, BMI1,
    /// BMI2, F16C, FMA, LZCNT, MOVBE, XSAVE.
    GNU_PROPERTY_X86_ISA_1_V3 = 1 << 2,
    /// GNU_PROPERTY_X86_ISA_1_V4: GNU_PROPERTY_X86_ISA_1_V3, AVX512F,
    /// AVX512BW, AVX512CD, AVX512DQ and AVX512VL.
    GNU_PROPERTY_X86_ISA_1_V4 = 1 << 3,
});

flag_names!(NAMES_GNU_PROPERTY_X86_FEATURE_1: u32 = {
    /// This indicates that all executable sections are compatible with IBT.
    GNU_PROPERTY_X86_FEATURE_1_IBT = 1 << 0,
    /// This indicates that all executable sections are compatible with SHSTK.
    GNU_PROPERTY_X86_FEATURE_1_SHSTK = 1 << 1,
});

// Note types for `ELF_NOTE_GO`.
constant_names!(NAMES_NT_GO: NoteType(u32) = {
    /// Build ID bits as generated by Go's gc compiler.
    ///
    /// The descriptor consists of any nonzero number of bytes.
    // See https://go-review.googlesource.com/10707.
    NT_GO_BUILD_ID = 4,
});

// TODO: Elf*_Move

/// Header of `SHT_HASH` section.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct HashHeader<E: Endian> {
    /// The number of hash buckets.
    pub bucket_count: U32<E>,
    /// The number of chain values.
    pub chain_count: U32<E>,
    // Array of hash bucket start indices.
    // buckets: U32<E>[bucket_count]
    // Array of hash chain links. An index of 0 terminates the chain.
    // chains: U32<E>[chain_count]
}

/// Calculate the SysV hash for a symbol name.
///
/// Used for `SHT_HASH`.
pub fn hash(name: &[u8]) -> u32 {
    let mut hash = 0u32;
    for byte in name {
        hash = hash.wrapping_mul(16).wrapping_add(u32::from(*byte));
        hash ^= (hash >> 24) & 0xf0;
    }
    hash & 0xfff_ffff
}

/// Header of `SHT_GNU_HASH` section.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct GnuHashHeader<E: Endian> {
    /// The number of hash buckets.
    pub bucket_count: U32<E>,
    /// The symbol table index of the first symbol in the hash.
    pub symbol_base: U32<E>,
    /// The number of words in the bloom filter.
    ///
    /// Must be a non-zero power of 2.
    pub bloom_count: U32<E>,
    /// The bit shift count for the bloom filter.
    pub bloom_shift: U32<E>,
    // Array of bloom filter words.
    // bloom_filters: U32<E>[bloom_count] or U64<E>[bloom_count]
    // Array of hash bucket start indices.
    // buckets: U32<E>[bucket_count]
    // Array of hash values, one for each symbol starting at symbol_base.
    // values: U32<E>[symbol_count]
}

/// Calculate the GNU hash for a symbol name.
///
/// Used for `SHT_GNU_HASH`.
pub fn gnu_hash(name: &[u8]) -> u32 {
    let mut hash = 5381u32;
    for byte in name {
        hash = hash.wrapping_mul(33).wrapping_add(u32::from(*byte));
    }
    hash
}

// Motorola 68k specific definitions.

names! {
    struct M68k(Base);
    /// `Rel*::r_type` values for `EM_68K`.
    consts r = pub NAMES_R_68K: RelocationType(u32) = {
        /// No reloc
        R_68K_NONE = 0,
        /// Direct 32 bit
        R_68K_32 = 1,
        /// Direct 16 bit
        R_68K_16 = 2,
        /// Direct 8 bit
        R_68K_8 = 3,
        /// PC relative 32 bit
        R_68K_PC32 = 4,
        /// PC relative 16 bit
        R_68K_PC16 = 5,
        /// PC relative 8 bit
        R_68K_PC8 = 6,
        /// 32 bit PC relative GOT entry
        R_68K_GOT32 = 7,
        /// 16 bit PC relative GOT entry
        R_68K_GOT16 = 8,
        /// 8 bit PC relative GOT entry
        R_68K_GOT8 = 9,
        /// 32 bit GOT offset
        R_68K_GOT32O = 10,
        /// 16 bit GOT offset
        R_68K_GOT16O = 11,
        /// 8 bit GOT offset
        R_68K_GOT8O = 12,
        /// 32 bit PC relative PLT address
        R_68K_PLT32 = 13,
        /// 16 bit PC relative PLT address
        R_68K_PLT16 = 14,
        /// 8 bit PC relative PLT address
        R_68K_PLT8 = 15,
        /// 32 bit PLT offset
        R_68K_PLT32O = 16,
        /// 16 bit PLT offset
        R_68K_PLT16O = 17,
        /// 8 bit PLT offset
        R_68K_PLT8O = 18,
        /// Copy symbol at runtime
        R_68K_COPY = 19,
        /// Create GOT entry
        R_68K_GLOB_DAT = 20,
        /// Create PLT entry
        R_68K_JMP_SLOT = 21,
        /// Adjust by program base
        R_68K_RELATIVE = 22,
        /// 32 bit GOT offset for GD
        R_68K_TLS_GD32 = 25,
        /// 16 bit GOT offset for GD
        R_68K_TLS_GD16 = 26,
        /// 8 bit GOT offset for GD
        R_68K_TLS_GD8 = 27,
        /// 32 bit GOT offset for LDM
        R_68K_TLS_LDM32 = 28,
        /// 16 bit GOT offset for LDM
        R_68K_TLS_LDM16 = 29,
        /// 8 bit GOT offset for LDM
        R_68K_TLS_LDM8 = 30,
        /// 32 bit module-relative offset
        R_68K_TLS_LDO32 = 31,
        /// 16 bit module-relative offset
        R_68K_TLS_LDO16 = 32,
        /// 8 bit module-relative offset
        R_68K_TLS_LDO8 = 33,
        /// 32 bit GOT offset for IE
        R_68K_TLS_IE32 = 34,
        /// 16 bit GOT offset for IE
        R_68K_TLS_IE16 = 35,
        /// 8 bit GOT offset for IE
        R_68K_TLS_IE8 = 36,
        /// 32 bit offset relative to static TLS block
        R_68K_TLS_LE32 = 37,
        /// 16 bit offset relative to static TLS block
        R_68K_TLS_LE16 = 38,
        /// 8 bit offset relative to static TLS block
        R_68K_TLS_LE8 = 39,
        /// 32 bit module number
        R_68K_TLS_DTPMOD32 = 40,
        /// 32 bit module-relative offset
        R_68K_TLS_DTPREL32 = 41,
        /// 32 bit TP-relative offset
        R_68K_TLS_TPREL32 = 42,
    };
}

// Intel 80386 specific definitions.

names! {
    struct I386(Base);
    /// `Rel*::r_type` values for `EM_386`.
    consts r = pub NAMES_R_386: RelocationType(u32) = {
        /// No reloc
        R_386_NONE = 0,
        /// Direct 32 bit
        R_386_32 = 1,
        /// PC relative 32 bit
        R_386_PC32 = 2,
        /// 32 bit GOT entry
        R_386_GOT32 = 3,
        /// 32 bit PLT address
        R_386_PLT32 = 4,
        /// Copy symbol at runtime
        R_386_COPY = 5,
        /// Create GOT entry
        R_386_GLOB_DAT = 6,
        /// Create PLT entry
        R_386_JMP_SLOT = 7,
        /// Adjust by program base
        R_386_RELATIVE = 8,
        /// 32 bit offset to GOT
        R_386_GOTOFF = 9,
        /// 32 bit PC relative offset to GOT
        R_386_GOTPC = 10,
        /// Direct 32 bit PLT address
        R_386_32PLT = 11,
        /// Offset in static TLS block
        R_386_TLS_TPOFF = 14,
        /// Address of GOT entry for static TLS block offset
        R_386_TLS_IE = 15,
        /// GOT entry for static TLS block offset
        R_386_TLS_GOTIE = 16,
        /// Offset relative to static TLS block
        R_386_TLS_LE = 17,
        /// Direct 32 bit for GNU version of general dynamic thread local data
        R_386_TLS_GD = 18,
        /// Direct 32 bit for GNU version of local dynamic thread local data in LE code
        R_386_TLS_LDM = 19,
        /// Direct 16 bit
        R_386_16 = 20,
        /// PC relative 16 bit
        R_386_PC16 = 21,
        /// Direct 8 bit
        R_386_8 = 22,
        /// PC relative 8 bit
        R_386_PC8 = 23,
        /// Direct 32 bit for general dynamic thread local data
        R_386_TLS_GD_32 = 24,
        /// Tag for pushl in GD TLS code
        R_386_TLS_GD_PUSH = 25,
        /// Relocation for call to __tls_get_addr()
        R_386_TLS_GD_CALL = 26,
        /// Tag for popl in GD TLS code
        R_386_TLS_GD_POP = 27,
        /// Direct 32 bit for local dynamic thread local data in LE code
        R_386_TLS_LDM_32 = 28,
        /// Tag for pushl in LDM TLS code
        R_386_TLS_LDM_PUSH = 29,
        /// Relocation for call to __tls_get_addr() in LDM code
        R_386_TLS_LDM_CALL = 30,
        /// Tag for popl in LDM TLS code
        R_386_TLS_LDM_POP = 31,
        /// Offset relative to TLS block
        R_386_TLS_LDO_32 = 32,
        /// GOT entry for negated static TLS block offset
        R_386_TLS_IE_32 = 33,
        /// Negated offset relative to static TLS block
        R_386_TLS_LE_32 = 34,
        /// ID of module containing symbol
        R_386_TLS_DTPMOD32 = 35,
        /// Offset in TLS block
        R_386_TLS_DTPOFF32 = 36,
        /// Negated offset in static TLS block
        R_386_TLS_TPOFF32 = 37,
        /// 32-bit symbol size
        R_386_SIZE32 = 38,
        /// GOT offset for TLS descriptor.
        R_386_TLS_GOTDESC = 39,
        /// Marker of call through TLS descriptor for relaxation.
        R_386_TLS_DESC_CALL = 40,
        /// TLS descriptor containing pointer to code and to argument, returning the TLS offset for the symbol.
        R_386_TLS_DESC = 41,
        /// Adjust indirectly by program base
        R_386_IRELATIVE = 42,
        /// Load from 32 bit GOT entry, relaxable.
        R_386_GOT32X = 43,
    };
}

// ADI SHARC specific definitions

names! {
    struct Sharc(Base);
    /// `Rel*::r_type` values for `EM_SHARC`.
    consts r = pub NAMES_R_SHARC: RelocationType(u32) = {
        /// 24-bit absolute address in bits 23:0 of a 48-bit instr
        ///
        /// Targets:
        ///
        /// * Type 25a (PC_DIRECT)
        R_SHARC_ADDR24_V3 = 0x0b,

        /// 32-bit absolute address in bits 31:0 of a 48-bit instr
        ///
        /// Targets:
        ///
        /// * Type 14a
        /// * Type 14d
        /// * Type 15a
        /// * Type 16a
        /// * Type 17a
        /// * Type 18a
        /// * Type 19a
        R_SHARC_ADDR32_V3 = 0x0c,

        /// 32-bit absolute address in bits 31:0 of a 32-bit data location
        ///
        /// Represented with `RelocationEncoding::Generic`
        R_SHARC_ADDR_VAR_V3 = 0x0d,

        /// 6-bit PC-relative address in bits 32:27 of a 48-bit instr
        ///
        /// Targets:
        ///
        /// * Type 9a
        /// * Type 10a
        R_SHARC_PCRSHORT_V3 = 0x0e,

        /// 24-bit PC-relative address in bits 23:0 of a 48-bit instr
        ///
        /// Targets:
        ///
        /// * Type 8a
        /// * Type 12a (truncated to 23 bits after relocation)
        /// * Type 13a (truncated to 23 bits after relocation)
        /// * Type 25a (PC Relative)
        R_SHARC_PCRLONG_V3 = 0x0f,

        /// 6-bit absolute address in bits 32:27 of a 48-bit instr
        ///
        /// Targets:
        ///
        /// * Type 4a
        /// * Type 4b
        /// * Type 4d
        R_SHARC_DATA6_V3 = 0x10,

        /// 16-bit absolute address in bits 39:24 of a 48-bit instr
        ///
        /// Targets:
        ///
        /// * Type 12a
        R_SHARC_DATA16_V3 = 0x11,

        /// 6-bit absolute address into bits 16:11 of a 32-bit instr
        ///
        /// Targets:
        ///
        /// * Type 4b
        R_SHARC_DATA6_VISA_V3 = 0x12,

        /// 7-bit absolute address into bits 6:0 of a 32-bit instr
        R_SHARC_DATA7_VISA_V3 = 0x13,

        /// 16-bit absolute address into bits 15:0 of a 32-bit instr
        R_SHARC_DATA16_VISA_V3 = 0x14,

        /// 6-bit PC-relative address into bits 16:11 of a Type B
        ///
        /// Targets:
        ///
        /// * Type 9b
        R_SHARC_PCR6_VISA_V3 = 0x17,

        /// 16-bit absolute address into bits 15:0 of a 16-bit location.
        ///
        /// Represented with `RelocationEncoding::Generic`
        R_SHARC_ADDR_VAR16_V3 = 0x19,

        R_SHARC_CALC_PUSH_ADDR = 0xe0,
        R_SHARC_CALC_PUSH_ADDEND = 0xe1,
        R_SHARC_CALC_ADD = 0xe2,
        R_SHARC_CALC_SUB = 0xe3,
        R_SHARC_CALC_MUL = 0xe4,
        R_SHARC_CALC_DIV = 0xe5,
        R_SHARC_CALC_MOD = 0xe6,
        R_SHARC_CALC_LSHIFT = 0xe7,
        R_SHARC_CALC_RSHIFT = 0xe8,
        R_SHARC_CALC_AND = 0xe9,
        R_SHARC_CALC_OR = 0xea,
        R_SHARC_CALC_XOR = 0xeb,
        R_SHARC_CALC_PUSH_LEN = 0xec,
        R_SHARC_CALC_NOT = 0xf6,
    };
    consts sht: SectionType(u32) = {
        /// .adi.attributes
        SHT_SHARC_ADI_ATTRIBUTES = SHT_LOPROC + 0x2,
    };
}

// SUN SPARC specific definitions.

names! {
    struct Sparc(Base);
    consts stt: SymbolType(u8) = {
        /// Global register reserved to app.
        STT_SPARC_REGISTER = 13,
    };
    flags ef: FileFlags(u32) = {
        /// little endian data
        EF_SPARC_LEDATA = 0x80_0000,
        /// generic V8+ features
        EF_SPARC_32PLUS = 0x00_0100,
        /// Sun UltraSPARC1 extensions
        EF_SPARC_SUN_US1 = 0x00_0200,
        /// HAL R1 extensions
        EF_SPARC_HAL_R1 = 0x00_0400,
        /// Sun UltraSPARCIII extensions
        EF_SPARC_SUN_US3 = 0x00_0800,
    };
    /// `Rel*::r_type` values for `EM_SPARC` and `EM_SPARC32PLUS`.
    consts r = pub NAMES_R_SPARC: RelocationType(u32) = {
        /// No reloc
        R_SPARC_NONE = 0,
        /// Direct 8 bit
        R_SPARC_8 = 1,
        /// Direct 16 bit
        R_SPARC_16 = 2,
        /// Direct 32 bit
        R_SPARC_32 = 3,
        /// PC relative 8 bit
        R_SPARC_DISP8 = 4,
        /// PC relative 16 bit
        R_SPARC_DISP16 = 5,
        /// PC relative 32 bit
        R_SPARC_DISP32 = 6,
        /// PC relative 30 bit shifted
        R_SPARC_WDISP30 = 7,
        /// PC relative 22 bit shifted
        R_SPARC_WDISP22 = 8,
        /// High 22 bit
        R_SPARC_HI22 = 9,
        /// Direct 22 bit
        R_SPARC_22 = 10,
        /// Direct 13 bit
        R_SPARC_13 = 11,
        /// Truncated 10 bit
        R_SPARC_LO10 = 12,
        /// Truncated 10 bit GOT entry
        R_SPARC_GOT10 = 13,
        /// 13 bit GOT entry
        R_SPARC_GOT13 = 14,
        /// 22 bit GOT entry shifted
        R_SPARC_GOT22 = 15,
        /// PC relative 10 bit truncated
        R_SPARC_PC10 = 16,
        /// PC relative 22 bit shifted
        R_SPARC_PC22 = 17,
        /// 30 bit PC relative PLT address
        R_SPARC_WPLT30 = 18,
        /// Copy symbol at runtime
        R_SPARC_COPY = 19,
        /// Create GOT entry
        R_SPARC_GLOB_DAT = 20,
        /// Create PLT entry
        R_SPARC_JMP_SLOT = 21,
        /// Adjust by program base
        R_SPARC_RELATIVE = 22,
        /// Direct 32 bit unaligned
        R_SPARC_UA32 = 23,

        /// Direct 32 bit ref to PLT entry
        R_SPARC_PLT32 = 24,
        /// High 22 bit PLT entry
        R_SPARC_HIPLT22 = 25,
        /// Truncated 10 bit PLT entry
        R_SPARC_LOPLT10 = 26,
        /// PC rel 32 bit ref to PLT entry
        R_SPARC_PCPLT32 = 27,
        /// PC rel high 22 bit PLT entry
        R_SPARC_PCPLT22 = 28,
        /// PC rel trunc 10 bit PLT entry
        R_SPARC_PCPLT10 = 29,
        /// Direct 10 bit
        R_SPARC_10 = 30,
        /// Direct 11 bit
        R_SPARC_11 = 31,
        /// Direct 64 bit
        R_SPARC_64 = 32,
        /// 10bit with secondary 13bit addend
        R_SPARC_OLO10 = 33,
        /// Top 22 bits of direct 64 bit
        R_SPARC_HH22 = 34,
        /// High middle 10 bits of ...
        R_SPARC_HM10 = 35,
        /// Low middle 22 bits of ...
        R_SPARC_LM22 = 36,
        /// Top 22 bits of pc rel 64 bit
        R_SPARC_PC_HH22 = 37,
        /// High middle 10 bit of ...
        R_SPARC_PC_HM10 = 38,
        /// Low miggle 22 bits of ...
        R_SPARC_PC_LM22 = 39,
        /// PC relative 16 bit shifted
        R_SPARC_WDISP16 = 40,
        /// PC relative 19 bit shifted
        R_SPARC_WDISP19 = 41,
        /// was part of v9 ABI but was removed
        R_SPARC_GLOB_JMP = 42,
        /// Direct 7 bit
        R_SPARC_7 = 43,
        /// Direct 5 bit
        R_SPARC_5 = 44,
        /// Direct 6 bit
        R_SPARC_6 = 45,
        /// PC relative 64 bit
        R_SPARC_DISP64 = 46,
        /// Direct 64 bit ref to PLT entry
        R_SPARC_PLT64 = 47,
        /// High 22 bit complemented
        R_SPARC_HIX22 = 48,
        /// Truncated 11 bit complemented
        R_SPARC_LOX10 = 49,
        /// Direct high 12 of 44 bit
        R_SPARC_H44 = 50,
        /// Direct mid 22 of 44 bit
        R_SPARC_M44 = 51,
        /// Direct low 10 of 44 bit
        R_SPARC_L44 = 52,
        /// Global register usage
        R_SPARC_REGISTER = 53,
        /// Direct 64 bit unaligned
        R_SPARC_UA64 = 54,
        /// Direct 16 bit unaligned
        R_SPARC_UA16 = 55,
        R_SPARC_TLS_GD_HI22 = 56,
        R_SPARC_TLS_GD_LO10 = 57,
        R_SPARC_TLS_GD_ADD = 58,
        R_SPARC_TLS_GD_CALL = 59,
        R_SPARC_TLS_LDM_HI22 = 60,
        R_SPARC_TLS_LDM_LO10 = 61,
        R_SPARC_TLS_LDM_ADD = 62,
        R_SPARC_TLS_LDM_CALL = 63,
        R_SPARC_TLS_LDO_HIX22 = 64,
        R_SPARC_TLS_LDO_LOX10 = 65,
        R_SPARC_TLS_LDO_ADD = 66,
        R_SPARC_TLS_IE_HI22 = 67,
        R_SPARC_TLS_IE_LO10 = 68,
        R_SPARC_TLS_IE_LD = 69,
        R_SPARC_TLS_IE_LDX = 70,
        R_SPARC_TLS_IE_ADD = 71,
        R_SPARC_TLS_LE_HIX22 = 72,
        R_SPARC_TLS_LE_LOX10 = 73,
        R_SPARC_TLS_DTPMOD32 = 74,
        R_SPARC_TLS_DTPMOD64 = 75,
        R_SPARC_TLS_DTPOFF32 = 76,
        R_SPARC_TLS_DTPOFF64 = 77,
        R_SPARC_TLS_TPOFF32 = 78,
        R_SPARC_TLS_TPOFF64 = 79,
        R_SPARC_GOTDATA_HIX22 = 80,
        R_SPARC_GOTDATA_LOX10 = 81,
        R_SPARC_GOTDATA_OP_HIX22 = 82,
        R_SPARC_GOTDATA_OP_LOX10 = 83,
        R_SPARC_GOTDATA_OP = 84,
        R_SPARC_H34 = 85,
        R_SPARC_SIZE32 = 86,
        R_SPARC_SIZE64 = 87,
        R_SPARC_WDISP10 = 88,
        R_SPARC_JMP_IREL = 248,
        R_SPARC_IRELATIVE = 249,
        R_SPARC_GNU_VTINHERIT = 250,
        R_SPARC_GNU_VTENTRY = 251,
        R_SPARC_REV32 = 252,
    };
    consts dt: DynamicTag(i64) = {
        DT_SPARC_REGISTER = 0x7000_0001,
    };
}

pub const EF_SPARC_EXT_MASK: u32 = 0xFF_FF00;

names! {
    struct SparcV9(Sparc);
    flags ef: FileFlags(u32) = {
        EF_SPARCV9_MM = 3 => NAMES_EF_SPARC_MM,
    };
}

constant_names!(NAMES_EF_SPARC_MM: FileFlags(u32) = {
    EF_SPARCV9_TSO = 0,
    EF_SPARCV9_PSO = 1,
    EF_SPARCV9_RMO = 2,
});

// MIPS R3000 specific definitions.

names! {
    struct Mips(Base);
    flags ef: FileFlags(u32) = {
        /// A .noreorder directive was used.
        EF_MIPS_NOREORDER = 1,
        /// Contains PIC code.
        EF_MIPS_PIC = 2,
        /// Uses PIC calling sequence.
        EF_MIPS_CPIC = 4,
        EF_MIPS_XGOT = 8,
        EF_MIPS_64BIT_WHIRL = 16,
        EF_MIPS_ABI2 = 32,
        EF_MIPS_ABI_ON32 = 64,
        /// Uses FP64 (12 callee-saved).
        EF_MIPS_FP64 = 512,
        /// Uses IEEE 754-2008 NaN encoding.
        EF_MIPS_NAN2008 = 1024,
        /// Mask for selecting EF_MIPS_ABI_ variant
        EF_MIPS_ABI = 0x0000_f000 => NAMES_EF_MIPS_ABI,
        /// MIPS architecture level.
        EF_MIPS_ARCH = 0xf000_0000 => NAMES_EF_MIPS_ARCH,
    };
    consts shn: SymbolSection(u16) = {
        /// Allocated common symbols.
        SHN_MIPS_ACOMMON = 0xff00,
        /// Allocated test symbols.
        SHN_MIPS_TEXT = 0xff01,
        /// Allocated data symbols.
        SHN_MIPS_DATA = 0xff02,
        /// Small common symbols.
        SHN_MIPS_SCOMMON = 0xff03,
        /// Small undefined symbols.
        SHN_MIPS_SUNDEFINED = 0xff04,
    };
    consts sht: SectionType(u32) = {
        /// Shared objects used in link.
        SHT_MIPS_LIBLIST = 0x7000_0000,
        SHT_MIPS_MSYM = 0x7000_0001,
        /// Conflicting symbols.
        SHT_MIPS_CONFLICT = 0x7000_0002,
        /// Global data area sizes.
        SHT_MIPS_GPTAB = 0x7000_0003,
        /// Reserved for SGI/MIPS compilers
        SHT_MIPS_UCODE = 0x7000_0004,
        /// MIPS ECOFF debugging info.
        SHT_MIPS_DEBUG = 0x7000_0005,
        /// Register usage information.
        SHT_MIPS_REGINFO = 0x7000_0006,
        SHT_MIPS_PACKAGE = 0x7000_0007,
        SHT_MIPS_PACKSYM = 0x7000_0008,
        SHT_MIPS_RELD = 0x7000_0009,
        SHT_MIPS_IFACE = 0x7000_000b,
        SHT_MIPS_CONTENT = 0x7000_000c,
        /// Miscellaneous options.
        SHT_MIPS_OPTIONS = 0x7000_000d,
        SHT_MIPS_SHDR = 0x7000_0010,
        SHT_MIPS_FDESC = 0x7000_0011,
        SHT_MIPS_EXTSYM = 0x7000_0012,
        SHT_MIPS_DENSE = 0x7000_0013,
        SHT_MIPS_PDESC = 0x7000_0014,
        SHT_MIPS_LOCSYM = 0x7000_0015,
        SHT_MIPS_AUXSYM = 0x7000_0016,
        SHT_MIPS_OPTSYM = 0x7000_0017,
        SHT_MIPS_LOCSTR = 0x7000_0018,
        SHT_MIPS_LINE = 0x7000_0019,
        SHT_MIPS_RFDESC = 0x7000_001a,
        SHT_MIPS_DELTASYM = 0x7000_001b,
        SHT_MIPS_DELTAINST = 0x7000_001c,
        SHT_MIPS_DELTACLASS = 0x7000_001d,
        /// DWARF debugging information.
        SHT_MIPS_DWARF = 0x7000_001e,
        SHT_MIPS_DELTADECL = 0x7000_001f,
        SHT_MIPS_SYMBOL_LIB = 0x7000_0020,
        /// Event section.
        SHT_MIPS_EVENTS = 0x7000_0021,
        SHT_MIPS_TRANSLATE = 0x7000_0022,
        SHT_MIPS_PIXIE = 0x7000_0023,
        SHT_MIPS_XLATE = 0x7000_0024,
        SHT_MIPS_XLATE_DEBUG = 0x7000_0025,
        SHT_MIPS_WHIRL = 0x7000_0026,
        SHT_MIPS_EH_REGION = 0x7000_0027,
        SHT_MIPS_XLATE_OLD = 0x7000_0028,
        SHT_MIPS_PDR_EXCEPTION = 0x7000_0029,
    };
    flags shf: SectionFlags(u64) = {
        /// Must be in global data area.
        SHF_MIPS_GPREL = 0x1000_0000,
        SHF_MIPS_MERGE = 0x2000_0000,
        SHF_MIPS_ADDR = 0x4000_0000,
        SHF_MIPS_STRINGS = 0x8000_0000,
        SHF_MIPS_NOSTRIP = 0x0800_0000,
        SHF_MIPS_LOCAL = 0x0400_0000,
        SHF_MIPS_NAMES = 0x0200_0000,
        SHF_MIPS_NODUPE = 0x0100_0000,
    };
    flags sto: SymbolOther(u8) = {
        STO_MIPS_PLT = 0x8,
    };
    consts stb: SymbolBind(u8) = {
        STB_MIPS_SPLIT_COMMON = 13,
    };
    /// `Rel*::r_type` values for `EM_MIPS`.
    consts r = pub NAMES_R_MIPS: RelocationType(u32) = {
        /// No reloc
        R_MIPS_NONE = 0,
        /// Direct 16 bit
        R_MIPS_16 = 1,
        /// Direct 32 bit
        R_MIPS_32 = 2,
        /// PC relative 32 bit
        R_MIPS_REL32 = 3,
        /// Direct 26 bit shifted
        R_MIPS_26 = 4,
        /// High 16 bit
        R_MIPS_HI16 = 5,
        /// Low 16 bit
        R_MIPS_LO16 = 6,
        /// GP relative 16 bit
        R_MIPS_GPREL16 = 7,
        /// 16 bit literal entry
        R_MIPS_LITERAL = 8,
        /// 16 bit GOT entry
        R_MIPS_GOT16 = 9,
        /// PC relative 16 bit
        R_MIPS_PC16 = 10,
        /// 16 bit GOT entry for function
        R_MIPS_CALL16 = 11,
        /// GP relative 32 bit
        R_MIPS_GPREL32 = 12,

        R_MIPS_SHIFT5 = 16,
        R_MIPS_SHIFT6 = 17,
        R_MIPS_64 = 18,
        R_MIPS_GOT_DISP = 19,
        R_MIPS_GOT_PAGE = 20,
        R_MIPS_GOT_OFST = 21,
        R_MIPS_GOT_HI16 = 22,
        R_MIPS_GOT_LO16 = 23,
        R_MIPS_SUB = 24,
        R_MIPS_INSERT_A = 25,
        R_MIPS_INSERT_B = 26,
        R_MIPS_DELETE = 27,
        R_MIPS_HIGHER = 28,
        R_MIPS_HIGHEST = 29,
        R_MIPS_CALL_HI16 = 30,
        R_MIPS_CALL_LO16 = 31,
        R_MIPS_SCN_DISP = 32,
        R_MIPS_REL16 = 33,
        R_MIPS_ADD_IMMEDIATE = 34,
        R_MIPS_PJUMP = 35,
        R_MIPS_RELGOT = 36,
        R_MIPS_JALR = 37,
        /// Module number 32 bit
        R_MIPS_TLS_DTPMOD32 = 38,
        /// Module-relative offset 32 bit
        R_MIPS_TLS_DTPREL32 = 39,
        /// Module number 64 bit
        R_MIPS_TLS_DTPMOD64 = 40,
        /// Module-relative offset 64 bit
        R_MIPS_TLS_DTPREL64 = 41,
        /// 16 bit GOT offset for GD
        R_MIPS_TLS_GD = 42,
        /// 16 bit GOT offset for LDM
        R_MIPS_TLS_LDM = 43,
        /// Module-relative offset, high 16 bits
        R_MIPS_TLS_DTPREL_HI16 = 44,
        /// Module-relative offset, low 16 bits
        R_MIPS_TLS_DTPREL_LO16 = 45,
        /// 16 bit GOT offset for IE
        R_MIPS_TLS_GOTTPREL = 46,
        /// TP-relative offset, 32 bit
        R_MIPS_TLS_TPREL32 = 47,
        /// TP-relative offset, 64 bit
        R_MIPS_TLS_TPREL64 = 48,
        /// TP-relative offset, high 16 bits
        R_MIPS_TLS_TPREL_HI16 = 49,
        /// TP-relative offset, low 16 bits
        R_MIPS_TLS_TPREL_LO16 = 50,
        R_MIPS_GLOB_DAT = 51,
        R_MIPS_COPY = 126,
        R_MIPS_JUMP_SLOT = 127,
    };
    consts pt: ProgramType(u32) = {
        /// Register usage information.
        PT_MIPS_REGINFO = 0x7000_0000,
        /// Runtime procedure table.
        PT_MIPS_RTPROC = 0x7000_0001,
        PT_MIPS_OPTIONS = 0x7000_0002,
        /// FP mode requirement.
        PT_MIPS_ABIFLAGS = 0x7000_0003,
    };
    flags pf: ProgramFlags(u32) = {
        PF_MIPS_LOCAL = 0x1000_0000,
    };
    consts dt: DynamicTag(i64) = {
        /// Runtime linker interface version
        DT_MIPS_RLD_VERSION = 0x7000_0001,
        /// Timestamp
        DT_MIPS_TIME_STAMP = 0x7000_0002,
        /// Checksum
        DT_MIPS_ICHECKSUM = 0x7000_0003,
        /// Version string (string tbl index)
        DT_MIPS_IVERSION = 0x7000_0004,
        /// Flags
        DT_MIPS_FLAGS = 0x7000_0005,
        /// Base address
        DT_MIPS_BASE_ADDRESS = 0x7000_0006,
        DT_MIPS_MSYM = 0x7000_0007,
        /// Address of CONFLICT section
        DT_MIPS_CONFLICT = 0x7000_0008,
        /// Address of LIBLIST section
        DT_MIPS_LIBLIST = 0x7000_0009,
        /// Number of local GOT entries
        DT_MIPS_LOCAL_GOTNO = 0x7000_000a,
        /// Number of CONFLICT entries
        DT_MIPS_CONFLICTNO = 0x7000_000b,
        /// Number of LIBLIST entries
        DT_MIPS_LIBLISTNO = 0x7000_0010,
        /// Number of DYNSYM entries
        DT_MIPS_SYMTABNO = 0x7000_0011,
        /// First external DYNSYM
        DT_MIPS_UNREFEXTNO = 0x7000_0012,
        /// First GOT entry in DYNSYM
        DT_MIPS_GOTSYM = 0x7000_0013,
        /// Number of GOT page table entries
        DT_MIPS_HIPAGENO = 0x7000_0014,
        /// Address of run time loader map.
        DT_MIPS_RLD_MAP = 0x7000_0016,
        /// Delta C++ class definition.
        DT_MIPS_DELTA_CLASS = 0x7000_0017,
        /// Number of entries in DT_MIPS_DELTA_CLASS.
        DT_MIPS_DELTA_CLASS_NO = 0x7000_0018,
        /// Delta C++ class instances.
        DT_MIPS_DELTA_INSTANCE = 0x7000_0019,
        /// Number of entries in DT_MIPS_DELTA_INSTANCE.
        DT_MIPS_DELTA_INSTANCE_NO = 0x7000_001a,
        /// Delta relocations.
        DT_MIPS_DELTA_RELOC = 0x7000_001b,
        /// Number of entries in DT_MIPS_DELTA_RELOC.
        DT_MIPS_DELTA_RELOC_NO = 0x7000_001c,
        /// Delta symbols that Delta relocations refer to.
        DT_MIPS_DELTA_SYM = 0x7000_001d,
        /// Number of entries in DT_MIPS_DELTA_SYM.
        DT_MIPS_DELTA_SYM_NO = 0x7000_001e,
        /// Delta symbols that hold the class declaration.
        DT_MIPS_DELTA_CLASSSYM = 0x7000_0020,
        /// Number of entries in DT_MIPS_DELTA_CLASSSYM.
        DT_MIPS_DELTA_CLASSSYM_NO = 0x7000_0021,
        /// Flags indicating for C++ flavor.
        DT_MIPS_CXX_FLAGS = 0x7000_0022,
        DT_MIPS_PIXIE_INIT = 0x7000_0023,
        DT_MIPS_SYMBOL_LIB = 0x7000_0024,
        DT_MIPS_LOCALPAGE_GOTIDX = 0x7000_0025,
        DT_MIPS_LOCAL_GOTIDX = 0x7000_0026,
        DT_MIPS_HIDDEN_GOTIDX = 0x7000_0027,
        DT_MIPS_PROTECTED_GOTIDX = 0x7000_0028,
        /// Address of .options.
        DT_MIPS_OPTIONS = 0x7000_0029,
        /// Address of .interface.
        DT_MIPS_INTERFACE = 0x7000_002a,
        DT_MIPS_DYNSTR_ALIGN = 0x7000_002b,
        /// Size of the .interface section.
        DT_MIPS_INTERFACE_SIZE = 0x7000_002c,
        /// Address of rld_text_rsolve function stored in GOT.
        DT_MIPS_RLD_TEXT_RESOLVE_ADDR = 0x7000_002d,
        /// Default suffix of dso to be added by rld on dlopen() calls.
        DT_MIPS_PERF_SUFFIX = 0x7000_002e,
        /// (O32)Size of compact rel section.
        DT_MIPS_COMPACT_SIZE = 0x7000_002f,
        /// GP value for aux GOTs.
        DT_MIPS_GP_VALUE = 0x7000_0030,
        /// Address of aux .dynamic.
        DT_MIPS_AUX_DYNAMIC = 0x7000_0031,
        /// The address of .got.plt in an executable using the new non-PIC ABI.
        DT_MIPS_PLTGOT = 0x7000_0032,
        /// The base of the PLT in an executable using the new non-PIC ABI if that PLT is writable.  For a non-writable PLT, this is omitted or has a zero value.
        DT_MIPS_RWPLT = 0x7000_0034,
        /// An alternative description of the classic MIPS RLD_MAP that is usable in a PIE as it stores a relative offset from the address of the tag rather than an absolute address.
        DT_MIPS_RLD_MAP_REL = 0x7000_0035,
    };
}

constant_names!(NAMES_EF_MIPS_ABI: FileFlags(u32) = {
    /// The first MIPS 32 bit ABI
    EF_MIPS_ABI_O32 = 0x0000_1000,
    /// O32 ABI extended for 64-bit architectures
    EF_MIPS_ABI_O64 = 0x0000_2000,
    /// EABI in 32-bit mode
    EF_MIPS_ABI_EABI32 = 0x0000_3000,
    /// EABI in 64-bit mode
    EF_MIPS_ABI_EABI64 = 0x0000_4000,
});

constant_names!(NAMES_EF_MIPS_ARCH: FileFlags(u32) = {
    /// -mips1 code.
    EF_MIPS_ARCH_1 = 0x0000_0000,
    /// -mips2 code.
    EF_MIPS_ARCH_2 = 0x1000_0000,
    /// -mips3 code.
    EF_MIPS_ARCH_3 = 0x2000_0000,
    /// -mips4 code.
    EF_MIPS_ARCH_4 = 0x3000_0000,
    /// -mips5 code.
    EF_MIPS_ARCH_5 = 0x4000_0000,
    /// MIPS32 code.
    EF_MIPS_ARCH_32 = 0x5000_0000,
    /// MIPS64 code.
    EF_MIPS_ARCH_64 = 0x6000_0000,
    /// MIPS32r2 code.
    EF_MIPS_ARCH_32R2 = 0x7000_0000,
    /// MIPS64r2 code.
    EF_MIPS_ARCH_64R2 = 0x8000_0000,
    /// MIPS32r6 code
    EF_MIPS_ARCH_32R6 = 0x9000_0000,
    /// MIPS64r6 code
    EF_MIPS_ARCH_64R6 = 0xa000_0000,
});

// MIPS values for `Sym32::st_other`.

/// Only valid for `STB_MIPS_SPLIT_COMMON`.
pub const STO_MIPS_SC_ALIGN_UNUSED: u8 = 0xff;

// Entries found in sections of type `SHT_MIPS_GPTAB`.

// TODO: Elf32_gptab, Elf32_RegInfo, Elf_Options

// Values for `Elf_Options::kind`.

/// Undefined.
pub const ODK_NULL: u32 = 0;
/// Register usage information.
pub const ODK_REGINFO: u32 = 1;
/// Exception processing options.
pub const ODK_EXCEPTIONS: u32 = 2;
/// Section padding options.
pub const ODK_PAD: u32 = 3;
/// Hardware workarounds performed
pub const ODK_HWPATCH: u32 = 4;
/// record the fill value used by the linker.
pub const ODK_FILL: u32 = 5;
/// reserve space for desktop tools to write.
pub const ODK_TAGS: u32 = 6;
/// HW workarounds.  'AND' bits when merging.
pub const ODK_HWAND: u32 = 7;
/// HW workarounds.  'OR' bits when merging.
pub const ODK_HWOR: u32 = 8;

// Values for `Elf_Options::info` for `ODK_EXCEPTIONS` entries.

/// FPE's which MUST be enabled.
pub const OEX_FPU_MIN: u32 = 0x1f;
/// FPE's which MAY be enabled.
pub const OEX_FPU_MAX: u32 = 0x1f00;
/// page zero must be mapped.
pub const OEX_PAGE0: u32 = 0x10000;
/// Force sequential memory mode?
pub const OEX_SMM: u32 = 0x20000;
/// Force floating point debug mode?
pub const OEX_FPDBUG: u32 = 0x40000;
pub const OEX_PRECISEFP: u32 = OEX_FPDBUG;
/// Dismiss invalid address faults?
pub const OEX_DISMISS: u32 = 0x80000;

pub const OEX_FPU_INVAL: u32 = 0x10;
pub const OEX_FPU_DIV0: u32 = 0x08;
pub const OEX_FPU_OFLO: u32 = 0x04;
pub const OEX_FPU_UFLO: u32 = 0x02;
pub const OEX_FPU_INEX: u32 = 0x01;

// Masks for `Elf_Options::info` for an `ODK_HWPATCH` entry.  */
/// R4000 end-of-page patch.
pub const OHW_R4KEOP: u32 = 0x1;
/// may need R8000 prefetch patch.
pub const OHW_R8KPFETCH: u32 = 0x2;
/// R5000 end-of-page patch.
pub const OHW_R5KEOP: u32 = 0x4;
/// R5000 cvt.\[ds\].l bug.  clean=1.
pub const OHW_R5KCVTL: u32 = 0x8;

pub const OPAD_PREFIX: u32 = 0x1;
pub const OPAD_POSTFIX: u32 = 0x2;
pub const OPAD_SYMBOL: u32 = 0x4;

// Entries found in sections of type `SHT_MIPS_OPTIONS`.

// TODO: Elf_Options_Hw

// Masks for `ElfOptions::info` for `ODK_HWAND` and `ODK_HWOR` entries.

pub const OHWA0_R4KEOP_CHECKED: u32 = 0x0000_0001;
pub const OHWA1_R4KEOP_CLEAN: u32 = 0x0000_0002;

// Values for `DT_MIPS_FLAGS` `Dyn32` entry.

/// No flags
pub const RHF_NONE: u32 = 0;
/// Use quickstart
pub const RHF_QUICKSTART: u32 = 1 << 0;
/// Hash size not power of 2
pub const RHF_NOTPOT: u32 = 1 << 1;
/// Ignore LD_LIBRARY_PATH
pub const RHF_NO_LIBRARY_REPLACEMENT: u32 = 1 << 2;
pub const RHF_NO_MOVE: u32 = 1 << 3;
pub const RHF_SGI_ONLY: u32 = 1 << 4;
pub const RHF_GUARANTEE_INIT: u32 = 1 << 5;
pub const RHF_DELTA_C_PLUS_PLUS: u32 = 1 << 6;
pub const RHF_GUARANTEE_START_INIT: u32 = 1 << 7;
pub const RHF_PIXIE: u32 = 1 << 8;
pub const RHF_DEFAULT_DELAY_LOAD: u32 = 1 << 9;
pub const RHF_REQUICKSTART: u32 = 1 << 10;
pub const RHF_REQUICKSTARTED: u32 = 1 << 11;
pub const RHF_CORD: u32 = 1 << 12;
pub const RHF_NO_UNRES_UNDEF: u32 = 1 << 13;
pub const RHF_RLD_ORDER_SAFE: u32 = 1 << 14;

// Entries found in sections of type `SHT_MIPS_LIBLIST`.

// TODO: Elf32_Lib, Elf64_Lib

// Values for `Lib*::l_flags`.

pub const LL_NONE: u32 = 0;
/// Require exact match
pub const LL_EXACT_MATCH: u32 = 1 << 0;
/// Ignore interface version
pub const LL_IGNORE_INT_VER: u32 = 1 << 1;
pub const LL_REQUIRE_MINOR: u32 = 1 << 2;
pub const LL_EXPORTS: u32 = 1 << 3;
pub const LL_DELAY_LOAD: u32 = 1 << 4;
pub const LL_DELTA: u32 = 1 << 5;

// TODO: MIPS ABI flags

// PA-RISC specific definitions.

names! {
    struct Parisc(Base);
    flags ef: FileFlags(u32) = {
        /// Trap nil pointer dereference.
        EF_PARISC_TRAPNIL = 0x0001_0000,
        /// Program uses arch. extensions.
        EF_PARISC_EXT = 0x0002_0000,
        /// Program expects little endian.
        EF_PARISC_LSB = 0x0004_0000,
        /// Program expects wide mode.
        EF_PARISC_WIDE = 0x0008_0000,
        /// No kernel assisted branch prediction.
        EF_PARISC_NO_KABP = 0x0010_0000,
        /// Allow lazy swapping.
        EF_PARISC_LAZYSWAP = 0x0040_0000,
        /// Architecture version.
        EF_PARISC_ARCH = 0x0000_ffff => NAMES_EFA_PARISC,
    };
    consts shn: SymbolSection(u16) = {
        /// Section for tentatively declared symbols in ANSI C.
        SHN_PARISC_ANSI_COMMON = 0xff00,
        /// Common blocks in huge model.
        SHN_PARISC_HUGE_COMMON = 0xff01,
    };
    consts sht: SectionType(u32) = {
        /// Contains product specific ext.
        SHT_PARISC_EXT = 0x7000_0000,
        /// Unwind information.
        SHT_PARISC_UNWIND = 0x7000_0001,
        /// Debug info for optimized code.
        SHT_PARISC_DOC = 0x7000_0002,
    };
    flags shf: SectionFlags(u64) = {
        /// Section with short addressing.
        SHF_PARISC_SHORT = 0x2000_0000,
        /// Section far from gp.
        SHF_PARISC_HUGE = 0x4000_0000,
        /// Static branch prediction code.
        SHF_PARISC_SBP = 0x8000_0000,
    };
    consts stt: SymbolType(u8) = {
        /// Millicode function entry point.
        STT_PARISC_MILLICODE = 13,
        STT_HP_OPAQUE = STT_LOOS + 0x1,
        STT_HP_STUB = STT_LOOS + 0x2,
    };
    /// `Rel*::r_type` values for `EM_PARISC`.
    consts r = pub NAMES_R_PARISC: RelocationType(u32) = {
        /// No reloc.
        R_PARISC_NONE = 0,
        /// Direct 32-bit reference.
        R_PARISC_DIR32 = 1,
        /// Left 21 bits of eff. address.
        R_PARISC_DIR21L = 2,
        /// Right 17 bits of eff. address.
        R_PARISC_DIR17R = 3,
        /// 17 bits of eff. address.
        R_PARISC_DIR17F = 4,
        /// Right 14 bits of eff. address.
        R_PARISC_DIR14R = 6,
        /// 32-bit rel. address.
        R_PARISC_PCREL32 = 9,
        /// Left 21 bits of rel. address.
        R_PARISC_PCREL21L = 10,
        /// Right 17 bits of rel. address.
        R_PARISC_PCREL17R = 11,
        /// 17 bits of rel. address.
        R_PARISC_PCREL17F = 12,
        /// Right 14 bits of rel. address.
        R_PARISC_PCREL14R = 14,
        /// Left 21 bits of rel. address.
        R_PARISC_DPREL21L = 18,
        /// Right 14 bits of rel. address.
        R_PARISC_DPREL14R = 22,
        /// GP-relative, left 21 bits.
        R_PARISC_GPREL21L = 26,
        /// GP-relative, right 14 bits.
        R_PARISC_GPREL14R = 30,
        /// LT-relative, left 21 bits.
        R_PARISC_LTOFF21L = 34,
        /// LT-relative, right 14 bits.
        R_PARISC_LTOFF14R = 38,
        /// 32 bits section rel. address.
        R_PARISC_SECREL32 = 41,
        /// No relocation, set segment base.
        R_PARISC_SEGBASE = 48,
        /// 32 bits segment rel. address.
        R_PARISC_SEGREL32 = 49,
        /// PLT rel. address, left 21 bits.
        R_PARISC_PLTOFF21L = 50,
        /// PLT rel. address, right 14 bits.
        R_PARISC_PLTOFF14R = 54,
        /// 32 bits LT-rel. function pointer.
        R_PARISC_LTOFF_FPTR32 = 57,
        /// LT-rel. fct ptr, left 21 bits.
        R_PARISC_LTOFF_FPTR21L = 58,
        /// LT-rel. fct ptr, right 14 bits.
        R_PARISC_LTOFF_FPTR14R = 62,
        /// 64 bits function address.
        R_PARISC_FPTR64 = 64,
        /// 32 bits function address.
        R_PARISC_PLABEL32 = 65,
        /// Left 21 bits of fdesc address.
        R_PARISC_PLABEL21L = 66,
        /// Right 14 bits of fdesc address.
        R_PARISC_PLABEL14R = 70,
        /// 64 bits PC-rel. address.
        R_PARISC_PCREL64 = 72,
        /// 22 bits PC-rel. address.
        R_PARISC_PCREL22F = 74,
        /// PC-rel. address, right 14 bits.
        R_PARISC_PCREL14WR = 75,
        /// PC rel. address, right 14 bits.
        R_PARISC_PCREL14DR = 76,
        /// 16 bits PC-rel. address.
        R_PARISC_PCREL16F = 77,
        /// 16 bits PC-rel. address.
        R_PARISC_PCREL16WF = 78,
        /// 16 bits PC-rel. address.
        R_PARISC_PCREL16DF = 79,
        /// 64 bits of eff. address.
        R_PARISC_DIR64 = 80,
        /// 14 bits of eff. address.
        R_PARISC_DIR14WR = 83,
        /// 14 bits of eff. address.
        R_PARISC_DIR14DR = 84,
        /// 16 bits of eff. address.
        R_PARISC_DIR16F = 85,
        /// 16 bits of eff. address.
        R_PARISC_DIR16WF = 86,
        /// 16 bits of eff. address.
        R_PARISC_DIR16DF = 87,
        /// 64 bits of GP-rel. address.
        R_PARISC_GPREL64 = 88,
        /// GP-rel. address, right 14 bits.
        R_PARISC_GPREL14WR = 91,
        /// GP-rel. address, right 14 bits.
        R_PARISC_GPREL14DR = 92,
        /// 16 bits GP-rel. address.
        R_PARISC_GPREL16F = 93,
        /// 16 bits GP-rel. address.
        R_PARISC_GPREL16WF = 94,
        /// 16 bits GP-rel. address.
        R_PARISC_GPREL16DF = 95,
        /// 64 bits LT-rel. address.
        R_PARISC_LTOFF64 = 96,
        /// LT-rel. address, right 14 bits.
        R_PARISC_LTOFF14WR = 99,
        /// LT-rel. address, right 14 bits.
        R_PARISC_LTOFF14DR = 100,
        /// 16 bits LT-rel. address.
        R_PARISC_LTOFF16F = 101,
        /// 16 bits LT-rel. address.
        R_PARISC_LTOFF16WF = 102,
        /// 16 bits LT-rel. address.
        R_PARISC_LTOFF16DF = 103,
        /// 64 bits section rel. address.
        R_PARISC_SECREL64 = 104,
        /// 64 bits segment rel. address.
        R_PARISC_SEGREL64 = 112,
        /// PLT-rel. address, right 14 bits.
        R_PARISC_PLTOFF14WR = 115,
        /// PLT-rel. address, right 14 bits.
        R_PARISC_PLTOFF14DR = 116,
        /// 16 bits LT-rel. address.
        R_PARISC_PLTOFF16F = 117,
        /// 16 bits PLT-rel. address.
        R_PARISC_PLTOFF16WF = 118,
        /// 16 bits PLT-rel. address.
        R_PARISC_PLTOFF16DF = 119,
        /// 64 bits LT-rel. function ptr.
        R_PARISC_LTOFF_FPTR64 = 120,
        /// LT-rel. fct. ptr., right 14 bits.
        R_PARISC_LTOFF_FPTR14WR = 123,
        /// LT-rel. fct. ptr., right 14 bits.
        R_PARISC_LTOFF_FPTR14DR = 124,
        /// 16 bits LT-rel. function ptr.
        R_PARISC_LTOFF_FPTR16F = 125,
        /// 16 bits LT-rel. function ptr.
        R_PARISC_LTOFF_FPTR16WF = 126,
        /// 16 bits LT-rel. function ptr.
        R_PARISC_LTOFF_FPTR16DF = 127,
        R_PARISC_LORESERVE = 128,
        /// Copy relocation.
        R_PARISC_COPY = 128,
        /// Dynamic reloc, imported PLT
        R_PARISC_IPLT = 129,
        /// Dynamic reloc, exported PLT
        R_PARISC_EPLT = 130,
        /// 32 bits TP-rel. address.
        R_PARISC_TPREL32 = 153,
        /// TP-rel. address, left 21 bits.
        R_PARISC_TPREL21L = 154,
        /// TP-rel. address, right 14 bits.
        R_PARISC_TPREL14R = 158,
        /// LT-TP-rel. address, left 21 bits.
        R_PARISC_LTOFF_TP21L = 162,
        /// LT-TP-rel. address, right 14 bits.
        R_PARISC_LTOFF_TP14R = 166,
        /// 14 bits LT-TP-rel. address.
        R_PARISC_LTOFF_TP14F = 167,
        /// 64 bits TP-rel. address.
        R_PARISC_TPREL64 = 216,
        /// TP-rel. address, right 14 bits.
        R_PARISC_TPREL14WR = 219,
        /// TP-rel. address, right 14 bits.
        R_PARISC_TPREL14DR = 220,
        /// 16 bits TP-rel. address.
        R_PARISC_TPREL16F = 221,
        /// 16 bits TP-rel. address.
        R_PARISC_TPREL16WF = 222,
        /// 16 bits TP-rel. address.
        R_PARISC_TPREL16DF = 223,
        /// 64 bits LT-TP-rel. address.
        R_PARISC_LTOFF_TP64 = 224,
        /// LT-TP-rel. address, right 14 bits.
        R_PARISC_LTOFF_TP14WR = 227,
        /// LT-TP-rel. address, right 14 bits.
        R_PARISC_LTOFF_TP14DR = 228,
        /// 16 bits LT-TP-rel. address.
        R_PARISC_LTOFF_TP16F = 229,
        /// 16 bits LT-TP-rel. address.
        R_PARISC_LTOFF_TP16WF = 230,
        /// 16 bits LT-TP-rel. address.
        R_PARISC_LTOFF_TP16DF = 231,
        R_PARISC_GNU_VTENTRY = 232,
        R_PARISC_GNU_VTINHERIT = 233,
        /// GD 21-bit left.
        R_PARISC_TLS_GD21L = 234,
        /// GD 14-bit right.
        R_PARISC_TLS_GD14R = 235,
        /// GD call to __t_g_a.
        R_PARISC_TLS_GDCALL = 236,
        /// LD module 21-bit left.
        R_PARISC_TLS_LDM21L = 237,
        /// LD module 14-bit right.
        R_PARISC_TLS_LDM14R = 238,
        /// LD module call to __t_g_a.
        R_PARISC_TLS_LDMCALL = 239,
        /// LD offset 21-bit left.
        R_PARISC_TLS_LDO21L = 240,
        /// LD offset 14-bit right.
        R_PARISC_TLS_LDO14R = 241,
        /// DTP module 32-bit.
        R_PARISC_TLS_DTPMOD32 = 242,
        /// DTP module 64-bit.
        R_PARISC_TLS_DTPMOD64 = 243,
        /// DTP offset 32-bit.
        R_PARISC_TLS_DTPOFF32 = 244,
        /// DTP offset 32-bit.
        R_PARISC_TLS_DTPOFF64 = 245,
        R_PARISC_TLS_LE21L = R_PARISC_TPREL21L.0,
        R_PARISC_TLS_LE14R = R_PARISC_TPREL14R.0,
        R_PARISC_TLS_IE21L = R_PARISC_LTOFF_TP21L.0,
        R_PARISC_TLS_IE14R = R_PARISC_LTOFF_TP14R.0,
        R_PARISC_TLS_TPREL32 = R_PARISC_TPREL32.0,
        R_PARISC_TLS_TPREL64 = R_PARISC_TPREL64.0,
        R_PARISC_HIRESERVE = 255,
    };
    consts pt: ProgramType(u32) = {
        PT_HP_TLS = PT_LOOS + 0x0,
        PT_HP_CORE_NONE = PT_LOOS + 0x1,
        PT_HP_CORE_VERSION = PT_LOOS + 0x2,
        PT_HP_CORE_KERNEL = PT_LOOS + 0x3,
        PT_HP_CORE_COMM = PT_LOOS + 0x4,
        PT_HP_CORE_PROC = PT_LOOS + 0x5,
        PT_HP_CORE_LOADABLE = PT_LOOS + 0x6,
        PT_HP_CORE_STACK = PT_LOOS + 0x7,
        PT_HP_CORE_SHM = PT_LOOS + 0x8,
        PT_HP_CORE_MMF = PT_LOOS + 0x9,
        PT_HP_PARALLEL = PT_LOOS + 0x10,
        PT_HP_FASTBIND = PT_LOOS + 0x11,
        PT_HP_OPT_ANNOT = PT_LOOS + 0x12,
        PT_HP_HSL_ANNOT = PT_LOOS + 0x13,
        PT_HP_STACK = PT_LOOS + 0x14,

        PT_PARISC_ARCHEXT = 0x7000_0000,
        PT_PARISC_UNWIND = 0x7000_0001,
    };
    flags pf: ProgramFlags(u32) = {
        PF_PARISC_SBP = 0x0800_0000,

        PF_HP_PAGE_SIZE = 0x0010_0000,
        PF_HP_FAR_SHARED = 0x0020_0000,
        PF_HP_NEAR_SHARED = 0x0040_0000,
        PF_HP_CODE = 0x0100_0000,
        PF_HP_MODIFY = 0x0200_0000,
        PF_HP_LAZYSWAP = 0x0400_0000,
        PF_HP_SBP = 0x0800_0000,
    };
}

constant_names!(NAMES_EFA_PARISC: FileFlags(u32) = {
    /// PA-RISC 1.0 big-endian.
    EFA_PARISC_1_0 = 0x020b,
    /// PA-RISC 1.1 big-endian.
    EFA_PARISC_1_1 = 0x0210,
    /// PA-RISC 2.0 big-endian.
    EFA_PARISC_2_0 = 0x0214,
});

// Alpha specific definitions.

names! {
    struct Alpha(Base);
    flags ef: FileFlags(u32) = {
        /// All addresses must be < 2GB.
        EF_ALPHA_32BIT = 1,
        /// Relocations for relaxing exist.
        EF_ALPHA_CANRELAX = 2,
    };
    consts sht: SectionType(u32) = {
        // These two are primarily concerned with ECOFF debugging info.
        SHT_ALPHA_DEBUG = 0x7000_0001,
        SHT_ALPHA_REGINFO = 0x7000_0002,
    };
    flags shf: SectionFlags(u64) = {
        SHF_ALPHA_GPREL = 0x1000_0000,
    };
    flags sto: SymbolOther(u8) = {
        /// No PV required.
        STO_ALPHA_NOPV = 0x80,
        /// PV only used for initial ldgp.
        STO_ALPHA_STD_GPLOAD = 0x88,
    };
    /// `Rel*::r_type` values for `EM_ALPHA`.
    consts r = pub NAMES_R_ALPHA: RelocationType(u32) = {
        /// No reloc
        R_ALPHA_NONE = 0,
        /// Direct 32 bit
        R_ALPHA_REFLONG = 1,
        /// Direct 64 bit
        R_ALPHA_REFQUAD = 2,
        /// GP relative 32 bit
        R_ALPHA_GPREL32 = 3,
        /// GP relative 16 bit w/optimization
        R_ALPHA_LITERAL = 4,
        /// Optimization hint for LITERAL
        R_ALPHA_LITUSE = 5,
        /// Add displacement to GP
        R_ALPHA_GPDISP = 6,
        /// PC+4 relative 23 bit shifted
        R_ALPHA_BRADDR = 7,
        /// PC+4 relative 16 bit shifted
        R_ALPHA_HINT = 8,
        /// PC relative 16 bit
        R_ALPHA_SREL16 = 9,
        /// PC relative 32 bit
        R_ALPHA_SREL32 = 10,
        /// PC relative 64 bit
        R_ALPHA_SREL64 = 11,
        /// GP relative 32 bit, high 16 bits
        R_ALPHA_GPRELHIGH = 17,
        /// GP relative 32 bit, low 16 bits
        R_ALPHA_GPRELLOW = 18,
        /// GP relative 16 bit
        R_ALPHA_GPREL16 = 19,
        /// Copy symbol at runtime
        R_ALPHA_COPY = 24,
        /// Create GOT entry
        R_ALPHA_GLOB_DAT = 25,
        /// Create PLT entry
        R_ALPHA_JMP_SLOT = 26,
        /// Adjust by program base
        R_ALPHA_RELATIVE = 27,
        R_ALPHA_TLS_GD_HI = 28,
        R_ALPHA_TLSGD = 29,
        R_ALPHA_TLS_LDM = 30,
        R_ALPHA_DTPMOD64 = 31,
        R_ALPHA_GOTDTPREL = 32,
        R_ALPHA_DTPREL64 = 33,
        R_ALPHA_DTPRELHI = 34,
        R_ALPHA_DTPRELLO = 35,
        R_ALPHA_DTPREL16 = 36,
        R_ALPHA_GOTTPREL = 37,
        R_ALPHA_TPREL64 = 38,
        R_ALPHA_TPRELHI = 39,
        R_ALPHA_TPRELLO = 40,
        R_ALPHA_TPREL16 = 41,
    };
    consts dt: DynamicTag(i64) = {
        DT_ALPHA_PLTRO = DT_LOPROC + 0,
    };
}

// Magic values of the `R_ALPHA_LITUSE` relocation addend.
pub const LITUSE_ALPHA_ADDR: u32 = 0;
pub const LITUSE_ALPHA_BASE: u32 = 1;
pub const LITUSE_ALPHA_BYTOFF: u32 = 2;
pub const LITUSE_ALPHA_JSR: u32 = 3;
pub const LITUSE_ALPHA_TLS_GD: u32 = 4;
pub const LITUSE_ALPHA_TLS_LDM: u32 = 5;

// PowerPC specific declarations.

names! {
    struct Ppc(Base);
    flags ef: FileFlags(u32) = {
        /// PowerPC embedded flag
        EF_PPC_EMB = 0x8000_0000,

        // Cygnus local bits below .
        /// PowerPC -mrelocatable flag
        EF_PPC_RELOCATABLE = 0x0001_0000,
        /// PowerPC -mrelocatable-lib flag
        EF_PPC_RELOCATABLE_LIB = 0x0000_8000,
    };
    /// `Rel*::r_type` values for `EM_PPC`.
    consts r = pub NAMES_R_PPC: RelocationType(u32) = {
        // PowerPC values for `Rel*::r_type` defined by the ABIs.
        R_PPC_NONE = 0,
        /// 32bit absolute address
        R_PPC_ADDR32 = 1,
        /// 26bit address, 2 bits ignored.
        R_PPC_ADDR24 = 2,
        /// 16bit absolute address
        R_PPC_ADDR16 = 3,
        /// lower 16bit of absolute address
        R_PPC_ADDR16_LO = 4,
        /// high 16bit of absolute address
        R_PPC_ADDR16_HI = 5,
        /// adjusted high 16bit
        R_PPC_ADDR16_HA = 6,
        /// 16bit address, 2 bits ignored
        R_PPC_ADDR14 = 7,
        R_PPC_ADDR14_BRTAKEN = 8,
        R_PPC_ADDR14_BRNTAKEN = 9,
        /// PC relative 26 bit
        R_PPC_REL24 = 10,
        /// PC relative 16 bit
        R_PPC_REL14 = 11,
        R_PPC_REL14_BRTAKEN = 12,
        R_PPC_REL14_BRNTAKEN = 13,
        R_PPC_GOT16 = 14,
        R_PPC_GOT16_LO = 15,
        R_PPC_GOT16_HI = 16,
        R_PPC_GOT16_HA = 17,
        R_PPC_PLTREL24 = 18,
        R_PPC_COPY = 19,
        R_PPC_GLOB_DAT = 20,
        R_PPC_JMP_SLOT = 21,
        R_PPC_RELATIVE = 22,
        R_PPC_LOCAL24PC = 23,
        R_PPC_UADDR32 = 24,
        R_PPC_UADDR16 = 25,
        R_PPC_REL32 = 26,
        R_PPC_PLT32 = 27,
        R_PPC_PLTREL32 = 28,
        R_PPC_PLT16_LO = 29,
        R_PPC_PLT16_HI = 30,
        R_PPC_PLT16_HA = 31,
        R_PPC_SDAREL16 = 32,
        R_PPC_SECTOFF = 33,
        R_PPC_SECTOFF_LO = 34,
        R_PPC_SECTOFF_HI = 35,
        R_PPC_SECTOFF_HA = 36,

        // PowerPC values for `Rel*::r_type` defined for the TLS access ABI.
        /// none    (sym+add)@tls
        R_PPC_TLS = 67,
        /// word32  (sym+add)@dtpmod
        R_PPC_DTPMOD32 = 68,
        /// half16* (sym+add)@tprel
        R_PPC_TPREL16 = 69,
        /// half16  (sym+add)@tprel@l
        R_PPC_TPREL16_LO = 70,
        /// half16  (sym+add)@tprel@h
        R_PPC_TPREL16_HI = 71,
        /// half16  (sym+add)@tprel@ha
        R_PPC_TPREL16_HA = 72,
        /// word32  (sym+add)@tprel
        R_PPC_TPREL32 = 73,
        /// half16*(sym+add)@dtprel
        R_PPC_DTPREL16 = 74,
        /// half16  (sym+add)@dtprel@l
        R_PPC_DTPREL16_LO = 75,
        /// half16  (sym+add)@dtprel@h
        R_PPC_DTPREL16_HI = 76,
        /// half16  (sym+add)@dtprel@ha
        R_PPC_DTPREL16_HA = 77,
        /// word32  (sym+add)@dtprel
        R_PPC_DTPREL32 = 78,
        /// half16* (sym+add)@got@tlsgd
        R_PPC_GOT_TLSGD16 = 79,
        /// half16  (sym+add)@got@tlsgd@l
        R_PPC_GOT_TLSGD16_LO = 80,
        /// half16  (sym+add)@got@tlsgd@h
        R_PPC_GOT_TLSGD16_HI = 81,
        /// half16  (sym+add)@got@tlsgd@ha
        R_PPC_GOT_TLSGD16_HA = 82,
        /// half16* (sym+add)@got@tlsld
        R_PPC_GOT_TLSLD16 = 83,
        /// half16  (sym+add)@got@tlsld@l
        R_PPC_GOT_TLSLD16_LO = 84,
        /// half16  (sym+add)@got@tlsld@h
        R_PPC_GOT_TLSLD16_HI = 85,
        /// half16  (sym+add)@got@tlsld@ha
        R_PPC_GOT_TLSLD16_HA = 86,
        /// half16* (sym+add)@got@tprel
        R_PPC_GOT_TPREL16 = 87,
        /// half16  (sym+add)@got@tprel@l
        R_PPC_GOT_TPREL16_LO = 88,
        /// half16  (sym+add)@got@tprel@h
        R_PPC_GOT_TPREL16_HI = 89,
        /// half16  (sym+add)@got@tprel@ha
        R_PPC_GOT_TPREL16_HA = 90,
        /// half16* (sym+add)@got@dtprel
        R_PPC_GOT_DTPREL16 = 91,
        /// half16* (sym+add)@got@dtprel@l
        R_PPC_GOT_DTPREL16_LO = 92,
        /// half16* (sym+add)@got@dtprel@h
        R_PPC_GOT_DTPREL16_HI = 93,
        /// half16* (sym+add)@got@dtprel@ha
        R_PPC_GOT_DTPREL16_HA = 94,
        /// none    (sym+add)@tlsgd
        R_PPC_TLSGD = 95,
        /// none    (sym+add)@tlsld
        R_PPC_TLSLD = 96,

        // PowerPC values for `Rel*::r_type` from the Embedded ELF ABI.
        R_PPC_EMB_NADDR32 = 101,
        R_PPC_EMB_NADDR16 = 102,
        R_PPC_EMB_NADDR16_LO = 103,
        R_PPC_EMB_NADDR16_HI = 104,
        R_PPC_EMB_NADDR16_HA = 105,
        R_PPC_EMB_SDAI16 = 106,
        R_PPC_EMB_SDA2I16 = 107,
        R_PPC_EMB_SDA2REL = 108,
        /// 16 bit offset in SDA
        R_PPC_EMB_SDA21 = 109,
        R_PPC_EMB_MRKREF = 110,
        R_PPC_EMB_RELSEC16 = 111,
        R_PPC_EMB_RELST_LO = 112,
        R_PPC_EMB_RELST_HI = 113,
        R_PPC_EMB_RELST_HA = 114,
        R_PPC_EMB_BIT_FLD = 115,
        /// 16 bit relative offset in SDA
        R_PPC_EMB_RELSDA = 116,

        // Diab tool values for `Rel*::r_type`.
        /// like EMB_SDA21, but lower 16 bit
        R_PPC_DIAB_SDA21_LO = 180,
        /// like EMB_SDA21, but high 16 bit
        R_PPC_DIAB_SDA21_HI = 181,
        /// like EMB_SDA21, adjusted high 16
        R_PPC_DIAB_SDA21_HA = 182,
        /// like EMB_RELSDA, but lower 16 bit
        R_PPC_DIAB_RELSDA_LO = 183,
        /// like EMB_RELSDA, but high 16 bit
        R_PPC_DIAB_RELSDA_HI = 184,
        /// like EMB_RELSDA, adjusted high 16
        R_PPC_DIAB_RELSDA_HA = 185,

        /// GNU extension to support local ifunc.
        R_PPC_IRELATIVE = 248,

        // GNU relocs used in PIC code sequences.
        /// half16   (sym+add-.)
        R_PPC_REL16 = 249,
        /// half16   (sym+add-.)@l
        R_PPC_REL16_LO = 250,
        /// half16   (sym+add-.)@h
        R_PPC_REL16_HI = 251,
        /// half16   (sym+add-.)@ha
        R_PPC_REL16_HA = 252,

        /// This is a phony reloc to handle any old fashioned TOC16 references that may
        /// still be in object files.
        R_PPC_TOC16 = 255,
    };
    consts dt: DynamicTag(i64) = {
        DT_PPC_GOT = DT_LOPROC + 0,
        DT_PPC_OPT = DT_LOPROC + 1,
    };
}

// PowerPC specific values for the `DT_PPC_OPT` entry.
pub const PPC_OPT_TLS: u32 = 1;

names! {
    struct Ppc64(Base);
    /// `Rel*::r_type` values for `EM_PPC64`.
    consts r = pub NAMES_R_PPC64: RelocationType(u32) = {
        // PowerPC64 values for `Rel*::r_type` defined by the ABIs.
        R_PPC64_NONE = R_PPC_NONE.0,
        /// 32bit absolute address
        R_PPC64_ADDR32 = R_PPC_ADDR32.0,
        /// 26bit address, word aligned
        R_PPC64_ADDR24 = R_PPC_ADDR24.0,
        /// 16bit absolute address
        R_PPC64_ADDR16 = R_PPC_ADDR16.0,
        /// lower 16bits of address
        R_PPC64_ADDR16_LO = R_PPC_ADDR16_LO.0,
        /// high 16bits of address.
        R_PPC64_ADDR16_HI = R_PPC_ADDR16_HI.0,
        /// adjusted high 16bits.
        R_PPC64_ADDR16_HA = R_PPC_ADDR16_HA.0,
        /// 16bit address, word aligned
        R_PPC64_ADDR14 = R_PPC_ADDR14.0,
        R_PPC64_ADDR14_BRTAKEN = R_PPC_ADDR14_BRTAKEN.0,
        R_PPC64_ADDR14_BRNTAKEN = R_PPC_ADDR14_BRNTAKEN.0,
        /// PC-rel. 26 bit, word aligned
        R_PPC64_REL24 = R_PPC_REL24.0,
        /// PC relative 16 bit
        R_PPC64_REL14 = R_PPC_REL14.0,
        R_PPC64_REL14_BRTAKEN = R_PPC_REL14_BRTAKEN.0,
        R_PPC64_REL14_BRNTAKEN = R_PPC_REL14_BRNTAKEN.0,
        R_PPC64_GOT16 = R_PPC_GOT16.0,
        R_PPC64_GOT16_LO = R_PPC_GOT16_LO.0,
        R_PPC64_GOT16_HI = R_PPC_GOT16_HI.0,
        R_PPC64_GOT16_HA = R_PPC_GOT16_HA.0,

        R_PPC64_COPY = R_PPC_COPY.0,
        R_PPC64_GLOB_DAT = R_PPC_GLOB_DAT.0,
        R_PPC64_JMP_SLOT = R_PPC_JMP_SLOT.0,
        R_PPC64_RELATIVE = R_PPC_RELATIVE.0,

        R_PPC64_UADDR32 = R_PPC_UADDR32.0,
        R_PPC64_UADDR16 = R_PPC_UADDR16.0,
        R_PPC64_REL32 = R_PPC_REL32.0,
        R_PPC64_PLT32 = R_PPC_PLT32.0,
        R_PPC64_PLTREL32 = R_PPC_PLTREL32.0,
        R_PPC64_PLT16_LO = R_PPC_PLT16_LO.0,
        R_PPC64_PLT16_HI = R_PPC_PLT16_HI.0,
        R_PPC64_PLT16_HA = R_PPC_PLT16_HA.0,

        R_PPC64_SECTOFF = R_PPC_SECTOFF.0,
        R_PPC64_SECTOFF_LO = R_PPC_SECTOFF_LO.0,
        R_PPC64_SECTOFF_HI = R_PPC_SECTOFF_HI.0,
        R_PPC64_SECTOFF_HA = R_PPC_SECTOFF_HA.0,
        /// word30 (S + A - P) >> 2
        R_PPC64_ADDR30 = 37,
        /// doubleword64 S + A
        R_PPC64_ADDR64 = 38,
        /// half16 #higher(S + A)
        R_PPC64_ADDR16_HIGHER = 39,
        /// half16 #highera(S + A)
        R_PPC64_ADDR16_HIGHERA = 40,
        /// half16 #highest(S + A)
        R_PPC64_ADDR16_HIGHEST = 41,
        /// half16 #highesta(S + A)
        R_PPC64_ADDR16_HIGHESTA = 42,
        /// doubleword64 S + A
        R_PPC64_UADDR64 = 43,
        /// doubleword64 S + A - P
        R_PPC64_REL64 = 44,
        /// doubleword64 L + A
        R_PPC64_PLT64 = 45,
        /// doubleword64 L + A - P
        R_PPC64_PLTREL64 = 46,
        /// half16* S + A - .TOC
        R_PPC64_TOC16 = 47,
        /// half16 #lo(S + A - .TOC.)
        R_PPC64_TOC16_LO = 48,
        /// half16 #hi(S + A - .TOC.)
        R_PPC64_TOC16_HI = 49,
        /// half16 #ha(S + A - .TOC.)
        R_PPC64_TOC16_HA = 50,
        /// doubleword64 .TOC
        R_PPC64_TOC = 51,
        /// half16* M + A
        R_PPC64_PLTGOT16 = 52,
        /// half16 #lo(M + A)
        R_PPC64_PLTGOT16_LO = 53,
        /// half16 #hi(M + A)
        R_PPC64_PLTGOT16_HI = 54,
        /// half16 #ha(M + A)
        R_PPC64_PLTGOT16_HA = 55,

        /// half16ds* (S + A) >> 2
        R_PPC64_ADDR16_DS = 56,
        /// half16ds  #lo(S + A) >> 2
        R_PPC64_ADDR16_LO_DS = 57,
        /// half16ds* (G + A) >> 2
        R_PPC64_GOT16_DS = 58,
        /// half16ds  #lo(G + A) >> 2
        R_PPC64_GOT16_LO_DS = 59,
        /// half16ds  #lo(L + A) >> 2
        R_PPC64_PLT16_LO_DS = 60,
        /// half16ds* (R + A) >> 2
        R_PPC64_SECTOFF_DS = 61,
        /// half16ds  #lo(R + A) >> 2
        R_PPC64_SECTOFF_LO_DS = 62,
        /// half16ds* (S + A - .TOC.) >> 2
        R_PPC64_TOC16_DS = 63,
        /// half16ds  #lo(S + A - .TOC.) >> 2
        R_PPC64_TOC16_LO_DS = 64,
        /// half16ds* (M + A) >> 2
        R_PPC64_PLTGOT16_DS = 65,
        /// half16ds  #lo(M + A) >> 2
        R_PPC64_PLTGOT16_LO_DS = 66,

        // PowerPC64 values for `Rel*::r_type` defined for the TLS access ABI.
        /// none    (sym+add)@tls
        R_PPC64_TLS = 67,
        /// doubleword64 (sym+add)@dtpmod
        R_PPC64_DTPMOD64 = 68,
        /// half16* (sym+add)@tprel
        R_PPC64_TPREL16 = 69,
        /// half16  (sym+add)@tprel@l
        R_PPC64_TPREL16_LO = 70,
        /// half16  (sym+add)@tprel@h
        R_PPC64_TPREL16_HI = 71,
        /// half16  (sym+add)@tprel@ha
        R_PPC64_TPREL16_HA = 72,
        /// doubleword64 (sym+add)@tprel
        R_PPC64_TPREL64 = 73,
        /// half16* (sym+add)@dtprel
        R_PPC64_DTPREL16 = 74,
        /// half16  (sym+add)@dtprel@l
        R_PPC64_DTPREL16_LO = 75,
        /// half16  (sym+add)@dtprel@h
        R_PPC64_DTPREL16_HI = 76,
        /// half16  (sym+add)@dtprel@ha
        R_PPC64_DTPREL16_HA = 77,
        /// doubleword64 (sym+add)@dtprel
        R_PPC64_DTPREL64 = 78,
        /// half16* (sym+add)@got@tlsgd
        R_PPC64_GOT_TLSGD16 = 79,
        /// half16  (sym+add)@got@tlsgd@l
        R_PPC64_GOT_TLSGD16_LO = 80,
        /// half16  (sym+add)@got@tlsgd@h
        R_PPC64_GOT_TLSGD16_HI = 81,
        /// half16  (sym+add)@got@tlsgd@ha
        R_PPC64_GOT_TLSGD16_HA = 82,
        /// half16* (sym+add)@got@tlsld
        R_PPC64_GOT_TLSLD16 = 83,
        /// half16  (sym+add)@got@tlsld@l
        R_PPC64_GOT_TLSLD16_LO = 84,
        /// half16  (sym+add)@got@tlsld@h
        R_PPC64_GOT_TLSLD16_HI = 85,
        /// half16  (sym+add)@got@tlsld@ha
        R_PPC64_GOT_TLSLD16_HA = 86,
        /// half16ds* (sym+add)@got@tprel
        R_PPC64_GOT_TPREL16_DS = 87,
        /// half16ds (sym+add)@got@tprel@l
        R_PPC64_GOT_TPREL16_LO_DS = 88,
        /// half16  (sym+add)@got@tprel@h
        R_PPC64_GOT_TPREL16_HI = 89,
        /// half16  (sym+add)@got@tprel@ha
        R_PPC64_GOT_TPREL16_HA = 90,
        /// half16ds* (sym+add)@got@dtprel
        R_PPC64_GOT_DTPREL16_DS = 91,
        /// half16ds (sym+add)@got@dtprel@l
        R_PPC64_GOT_DTPREL16_LO_DS = 92,
        /// half16  (sym+add)@got@dtprel@h
        R_PPC64_GOT_DTPREL16_HI = 93,
        /// half16  (sym+add)@got@dtprel@ha
        R_PPC64_GOT_DTPREL16_HA = 94,
        /// half16ds* (sym+add)@tprel
        R_PPC64_TPREL16_DS = 95,
        /// half16ds (sym+add)@tprel@l
        R_PPC64_TPREL16_LO_DS = 96,
        /// half16  (sym+add)@tprel@higher
        R_PPC64_TPREL16_HIGHER = 97,
        /// half16  (sym+add)@tprel@highera
        R_PPC64_TPREL16_HIGHERA = 98,
        /// half16  (sym+add)@tprel@highest
        R_PPC64_TPREL16_HIGHEST = 99,
        /// half16  (sym+add)@tprel@highesta
        R_PPC64_TPREL16_HIGHESTA = 100,
        /// half16ds* (sym+add)@dtprel
        R_PPC64_DTPREL16_DS = 101,
        /// half16ds (sym+add)@dtprel@l
        R_PPC64_DTPREL16_LO_DS = 102,
        /// half16  (sym+add)@dtprel@higher
        R_PPC64_DTPREL16_HIGHER = 103,
        /// half16  (sym+add)@dtprel@highera
        R_PPC64_DTPREL16_HIGHERA = 104,
        /// half16  (sym+add)@dtprel@highest
        R_PPC64_DTPREL16_HIGHEST = 105,
        /// half16  (sym+add)@dtprel@highesta
        R_PPC64_DTPREL16_HIGHESTA = 106,
        /// none    (sym+add)@tlsgd
        R_PPC64_TLSGD = 107,
        /// none    (sym+add)@tlsld
        R_PPC64_TLSLD = 108,
        /// none
        R_PPC64_TOCSAVE = 109,

        // Added when HA and HI relocs were changed to report overflows.
        R_PPC64_ADDR16_HIGH = 110,
        R_PPC64_ADDR16_HIGHA = 111,
        R_PPC64_TPREL16_HIGH = 112,
        R_PPC64_TPREL16_HIGHA = 113,
        R_PPC64_DTPREL16_HIGH = 114,
        R_PPC64_DTPREL16_HIGHA = 115,

        /// GNU extension to support local ifunc.
        R_PPC64_JMP_IREL = 247,
        /// GNU extension to support local ifunc.
        R_PPC64_IRELATIVE = 248,
        /// half16   (sym+add-.)
        R_PPC64_REL16 = 249,
        /// half16   (sym+add-.)@l
        R_PPC64_REL16_LO = 250,
        /// half16   (sym+add-.)@h
        R_PPC64_REL16_HI = 251,
        /// half16   (sym+add-.)@ha
        R_PPC64_REL16_HA = 252,
    };
    consts dt: DynamicTag(i64) = {
        DT_PPC64_GLINK = DT_LOPROC + 0,
        DT_PPC64_OPD = DT_LOPROC + 1,
        DT_PPC64_OPDSZ = DT_LOPROC + 2,
        DT_PPC64_OPT = DT_LOPROC + 3,
    };
}

/// PowerPC64 bits specifying ABI.
///
/// 1 for original function descriptor using ABI,
/// 2 for revised ABI without function descriptors,
/// 0 for unspecified or not using any features affected by the differences.
pub const EF_PPC64_ABI: u32 = 3;

// PowerPC64 bits for `DT_PPC64_OPT` entry.
pub const PPC64_OPT_TLS: u32 = 1;
pub const PPC64_OPT_MULTI_TOC: u32 = 2;
pub const PPC64_OPT_LOCALENTRY: u32 = 4;

// PowerPC64 values for `Sym64::st_other.
pub const STO_PPC64_LOCAL_BIT: u8 = 5;
pub const STO_PPC64_LOCAL_MASK: u8 = 7 << STO_PPC64_LOCAL_BIT;

// ARM specific declarations.

names! {
    struct Arm(Base);
    flags ef: FileFlags(u32) = {
        EF_ARM_RELEXEC = 0x01,
        EF_ARM_HASENTRY = 0x02,
        EF_ARM_INTERWORK = 0x04,
        EF_ARM_APCS_26 = 0x08,
        EF_ARM_APCS_FLOAT = 0x10,
        EF_ARM_PIC = 0x20,
        /// 8-bit structure alignment is in use
        EF_ARM_ALIGN8 = 0x40,
        EF_ARM_NEW_ABI = 0x80,
        EF_ARM_OLD_ABI = 0x100,
        EF_ARM_SOFT_FLOAT = 0x200,
        EF_ARM_VFP_FLOAT = 0x400,
        EF_ARM_MAVERICK_FLOAT = 0x800,

        // Constants defined in AAELF.
        EF_ARM_BE8 = 0x0080_0000,
        EF_ARM_LE8 = 0x0040_0000,

        EF_ARM_EABIMASK = 0xff00_0000 => NAMES_EF_ARM_EABI,
    };
    consts stt: SymbolType(u8) = {
        /// A Thumb function.
        STT_ARM_TFUNC = STT_LOPROC,
        /// A Thumb label.
        STT_ARM_16BIT = STT_HIPROC,
    };
    flags shf: SectionFlags(u64) = {
        /// Section contains an entry point
        SHF_ARM_ENTRYSECT = 0x1000_0000,
        /// Section may be multiply defined in the input to a link step.
        SHF_ARM_COMDEF = 0x8000_0000,
    };
    flags pf: ProgramFlags(u32) = {
        /// Segment contains the location addressed by the static base.
        PF_ARM_SB = 0x1000_0000,
        /// Position-independent segment.
        PF_ARM_PI = 0x2000_0000,
        /// Absolute segment.
        PF_ARM_ABS = 0x4000_0000,
    };
    consts pt: ProgramType(u32) = {
        /// ARM unwind segment.
        PT_ARM_EXIDX = PT_LOPROC + 1,
    };
    consts sht: SectionType(u32) = {
        /// ARM unwind section.
        SHT_ARM_EXIDX = SHT_LOPROC + 1,
        /// Preemption details.
        SHT_ARM_PREEMPTMAP = SHT_LOPROC + 2,
        /// ARM attributes section.
        SHT_ARM_ATTRIBUTES = SHT_LOPROC + 3,
    };
    /// `Rel*::r_type` values for `EM_ARM`.
    consts r = pub NAMES_R_ARM: RelocationType(u32) = {
        /// No reloc
        R_ARM_NONE = 0,
        /// Deprecated PC relative 26 bit branch.
        R_ARM_PC24 = 1,
        /// Direct 32 bit
        R_ARM_ABS32 = 2,
        /// PC relative 32 bit
        R_ARM_REL32 = 3,
        R_ARM_PC13 = 4,
        /// Direct 16 bit
        R_ARM_ABS16 = 5,
        /// Direct 12 bit
        R_ARM_ABS12 = 6,
        /// Direct & 0x7C (`LDR`, `STR`).
        R_ARM_THM_ABS5 = 7,
        /// Direct 8 bit
        R_ARM_ABS8 = 8,
        R_ARM_SBREL32 = 9,
        /// PC relative 24 bit (Thumb32 `BL`).
        R_ARM_THM_PC22 = 10,
        /// PC relative & 0x3FC (Thumb16 `LDR`, `ADD`, `ADR`).
        R_ARM_THM_PC8 = 11,
        R_ARM_AMP_VCALL9 = 12,
        /// Obsolete static relocation.
        R_ARM_SWI24 = 13,
        /// Dynamic relocation.
        R_ARM_TLS_DESC = 13,
        /// Reserved.
        R_ARM_THM_SWI8 = 14,
        /// Reserved.
        R_ARM_XPC25 = 15,
        /// Reserved.
        R_ARM_THM_XPC22 = 16,
        /// ID of module containing symbol
        R_ARM_TLS_DTPMOD32 = 17,
        /// Offset in TLS block
        R_ARM_TLS_DTPOFF32 = 18,
        /// Offset in static TLS block
        R_ARM_TLS_TPOFF32 = 19,
        /// Copy symbol at runtime
        R_ARM_COPY = 20,
        /// Create GOT entry
        R_ARM_GLOB_DAT = 21,
        /// Create PLT entry
        R_ARM_JUMP_SLOT = 22,
        /// Adjust by program base
        R_ARM_RELATIVE = 23,
        /// 32 bit offset to GOT
        R_ARM_GOTOFF = 24,
        /// 32 bit PC relative offset to GOT
        R_ARM_GOTPC = 25,
        /// 32 bit GOT entry
        R_ARM_GOT32 = 26,
        /// Deprecated, 32 bit PLT address.
        R_ARM_PLT32 = 27,
        /// PC relative 24 bit (`BL`, `BLX`).
        R_ARM_CALL = 28,
        /// PC relative 24 bit (`B`, `BL<cond>`).
        R_ARM_JUMP24 = 29,
        /// PC relative 24 bit (Thumb32 `B.W`).
        R_ARM_THM_JUMP24 = 30,
        /// Adjust by program base.
        R_ARM_BASE_ABS = 31,
        /// Obsolete.
        R_ARM_ALU_PCREL_7_0 = 32,
        /// Obsolete.
        R_ARM_ALU_PCREL_15_8 = 33,
        /// Obsolete.
        R_ARM_ALU_PCREL_23_15 = 34,
        /// Deprecated, prog. base relative.
        R_ARM_LDR_SBREL_11_0 = 35,
        /// Deprecated, prog. base relative.
        R_ARM_ALU_SBREL_19_12 = 36,
        /// Deprecated, prog. base relative.
        R_ARM_ALU_SBREL_27_20 = 37,
        R_ARM_TARGET1 = 38,
        /// Program base relative.
        R_ARM_SBREL31 = 39,
        R_ARM_V4BX = 40,
        R_ARM_TARGET2 = 41,
        /// 32 bit PC relative.
        R_ARM_PREL31 = 42,
        /// Direct 16-bit (`MOVW`).
        R_ARM_MOVW_ABS_NC = 43,
        /// Direct high 16-bit (`MOVT`).
        R_ARM_MOVT_ABS = 44,
        /// PC relative 16-bit (`MOVW`).
        R_ARM_MOVW_PREL_NC = 45,
        /// PC relative (MOVT).
        R_ARM_MOVT_PREL = 46,
        /// Direct 16 bit (Thumb32 `MOVW`).
        R_ARM_THM_MOVW_ABS_NC = 47,
        /// Direct high 16 bit (Thumb32 `MOVT`).
        R_ARM_THM_MOVT_ABS = 48,
        /// PC relative 16 bit (Thumb32 `MOVW`).
        R_ARM_THM_MOVW_PREL_NC = 49,
        /// PC relative high 16 bit (Thumb32 `MOVT`).
        R_ARM_THM_MOVT_PREL = 50,
        /// PC relative 20 bit (Thumb32 `B<cond>.W`).
        R_ARM_THM_JUMP19 = 51,
        /// PC relative X & 0x7E (Thumb16 `CBZ`, `CBNZ`).
        R_ARM_THM_JUMP6 = 52,
        /// PC relative 12 bit (Thumb32 `ADR.W`).
        R_ARM_THM_ALU_PREL_11_0 = 53,
        /// PC relative 12 bit (Thumb32 `LDR{D,SB,H,SH}`).
        R_ARM_THM_PC12 = 54,
        /// Direct 32-bit.
        R_ARM_ABS32_NOI = 55,
        /// PC relative 32-bit.
        R_ARM_REL32_NOI = 56,
        /// PC relative (`ADD`, `SUB`).
        R_ARM_ALU_PC_G0_NC = 57,
        /// PC relative (`ADD`, `SUB`).
        R_ARM_ALU_PC_G0 = 58,
        /// PC relative (`ADD`, `SUB`).
        R_ARM_ALU_PC_G1_NC = 59,
        /// PC relative (`ADD`, `SUB`).
        R_ARM_ALU_PC_G1 = 60,
        /// PC relative (`ADD`, `SUB`).
        R_ARM_ALU_PC_G2 = 61,
        /// PC relative (`LDR`,`STR`,`LDRB`,`STRB`).
        R_ARM_LDR_PC_G1 = 62,
        /// PC relative (`LDR`,`STR`,`LDRB`,`STRB`).
        R_ARM_LDR_PC_G2 = 63,
        /// PC relative (`STR{D,H}`, `LDR{D,SB,H,SH}`).
        R_ARM_LDRS_PC_G0 = 64,
        /// PC relative (`STR{D,H}`, `LDR{D,SB,H,SH}`).
        R_ARM_LDRS_PC_G1 = 65,
        /// PC relative (`STR{D,H}`, `LDR{D,SB,H,SH}`).
        R_ARM_LDRS_PC_G2 = 66,
        /// PC relative (`LDC`, `STC`).
        R_ARM_LDC_PC_G0 = 67,
        /// PC relative (`LDC`, `STC`).
        R_ARM_LDC_PC_G1 = 68,
        /// PC relative (`LDC`, `STC`).
        R_ARM_LDC_PC_G2 = 69,
        /// Program base relative (`ADD`,`SUB`).
        R_ARM_ALU_SB_G0_NC = 70,
        /// Program base relative (`ADD`,`SUB`).
        R_ARM_ALU_SB_G0 = 71,
        /// Program base relative (`ADD`,`SUB`).
        R_ARM_ALU_SB_G1_NC = 72,
        /// Program base relative (`ADD`,`SUB`).
        R_ARM_ALU_SB_G1 = 73,
        /// Program base relative (`ADD`,`SUB`).
        R_ARM_ALU_SB_G2 = 74,
        /// Program base relative (`LDR`, `STR`, `LDRB`, `STRB`).
        R_ARM_LDR_SB_G0 = 75,
        /// Program base relative (`LDR`, `STR`, `LDRB`, `STRB`).
        R_ARM_LDR_SB_G1 = 76,
        /// Program base relative (`LDR`, `STR`, `LDRB`, `STRB`).
        R_ARM_LDR_SB_G2 = 77,
        /// Program base relative (`LDR`, `STR`, `LDRB`, `STRB`).
        R_ARM_LDRS_SB_G0 = 78,
        /// Program base relative (`LDR`, `STR`, `LDRB`, `STRB`).
        R_ARM_LDRS_SB_G1 = 79,
        /// Program base relative (`LDR`, `STR`, `LDRB`, `STRB`).
        R_ARM_LDRS_SB_G2 = 80,
        /// Program base relative (`LDC`,`STC`).
        R_ARM_LDC_SB_G0 = 81,
        /// Program base relative (`LDC`,`STC`).
        R_ARM_LDC_SB_G1 = 82,
        /// Program base relative (`LDC`,`STC`).
        R_ARM_LDC_SB_G2 = 83,
        /// Program base relative 16 bit (`MOVW`).
        R_ARM_MOVW_BREL_NC = 84,
        /// Program base relative high 16 bit (`MOVT`).
        R_ARM_MOVT_BREL = 85,
        /// Program base relative 16 bit (`MOVW`).
        R_ARM_MOVW_BREL = 86,
        /// Program base relative 16 bit (Thumb32 `MOVW`).
        R_ARM_THM_MOVW_BREL_NC = 87,
        /// Program base relative high 16 bit (Thumb32 `MOVT`).
        R_ARM_THM_MOVT_BREL = 88,
        /// Program base relative 16 bit (Thumb32 `MOVW`).
        R_ARM_THM_MOVW_BREL = 89,
        R_ARM_TLS_GOTDESC = 90,
        R_ARM_TLS_CALL = 91,
        /// TLS relaxation.
        R_ARM_TLS_DESCSEQ = 92,
        R_ARM_THM_TLS_CALL = 93,
        R_ARM_PLT32_ABS = 94,
        /// GOT entry.
        R_ARM_GOT_ABS = 95,
        /// PC relative GOT entry.
        R_ARM_GOT_PREL = 96,
        /// GOT entry relative to GOT origin (`LDR`).
        R_ARM_GOT_BREL12 = 97,
        /// 12 bit, GOT entry relative to GOT origin (`LDR`, `STR`).
        R_ARM_GOTOFF12 = 98,
        R_ARM_GOTRELAX = 99,
        R_ARM_GNU_VTENTRY = 100,
        R_ARM_GNU_VTINHERIT = 101,
        /// PC relative & 0xFFE (Thumb16 `B`).
        R_ARM_THM_PC11 = 102,
        /// PC relative & 0x1FE (Thumb16 `B`/`B<cond>`).
        R_ARM_THM_PC9 = 103,
        /// PC-rel 32 bit for global dynamic thread local data
        R_ARM_TLS_GD32 = 104,
        /// PC-rel 32 bit for local dynamic thread local data
        R_ARM_TLS_LDM32 = 105,
        /// 32 bit offset relative to TLS block
        R_ARM_TLS_LDO32 = 106,
        /// PC-rel 32 bit for GOT entry of static TLS block offset
        R_ARM_TLS_IE32 = 107,
        /// 32 bit offset relative to static TLS block
        R_ARM_TLS_LE32 = 108,
        /// 12 bit relative to TLS block (`LDR`, `STR`).
        R_ARM_TLS_LDO12 = 109,
        /// 12 bit relative to static TLS block (`LDR`, `STR`).
        R_ARM_TLS_LE12 = 110,
        /// 12 bit GOT entry relative to GOT origin (`LDR`).
        R_ARM_TLS_IE12GP = 111,
        /// Obsolete.
        R_ARM_ME_TOO = 128,
        R_ARM_THM_TLS_DESCSEQ = 129,
        R_ARM_THM_TLS_DESCSEQ16 = 129,
        R_ARM_THM_TLS_DESCSEQ32 = 130,
        /// GOT entry relative to GOT origin, 12 bit (Thumb32 `LDR`).
        R_ARM_THM_GOT_BREL12 = 131,
        R_ARM_IRELATIVE = 160,
        R_ARM_RXPC25 = 249,
        R_ARM_RSBREL32 = 250,
        R_ARM_THM_RPC22 = 251,
        R_ARM_RREL32 = 252,
        R_ARM_RABS22 = 253,
        R_ARM_RPC24 = 254,
        R_ARM_RBASE = 255,
    };
}

constant_names!(NAMES_EF_ARM_EABI: FileFlags(u32) = {
    EF_ARM_EABI_UNKNOWN = 0x0000_0000,
    EF_ARM_EABI_VER1 = 0x0100_0000,
    EF_ARM_EABI_VER2 = 0x0200_0000,
    EF_ARM_EABI_VER3 = 0x0300_0000,
    EF_ARM_EABI_VER4 = 0x0400_0000,
    EF_ARM_EABI_VER5 = 0x0500_0000,
});

/// NB conflicts with EF_ARM_SOFT_FLOAT
pub const EF_ARM_ABI_FLOAT_SOFT: u32 = 0x200;
/// NB conflicts with EF_ARM_VFP_FLOAT
pub const EF_ARM_ABI_FLOAT_HARD: u32 = 0x400;

// Other constants defined in the ARM ELF spec. version B-01.
// NB. These conflict with values defined above.
pub const EF_ARM_SYMSARESORTED: u32 = 0x04;
pub const EF_ARM_DYNSYMSUSESEGIDX: u32 = 0x08;
pub const EF_ARM_MAPSYMSFIRST: u32 = 0x10;

names! {
    struct Aarch64(Base);
    consts sht: SectionType(u32) = {
        /// AArch64 attributes section.
        SHT_AARCH64_ATTRIBUTES = SHT_LOPROC + 3,
    };
    flags sto: SymbolOther(u8) = {
        // AArch64 values for `Sym64::st_other`.
        STO_AARCH64_VARIANT_PCS = 0x80,
    };
    consts dt: DynamicTag(i64) = {
        DT_AARCH64_BTI_PLT = DT_LOPROC + 1,
        DT_AARCH64_PAC_PLT = DT_LOPROC + 3,
        DT_AARCH64_VARIANT_PCS = DT_LOPROC + 5,
    };
    /// `Rel*::r_type` values for `EM_AARCH64`.
    consts r = pub NAMES_R_AARCH64: RelocationType(u32) = {
        /// No relocation.
        R_AARCH64_NONE = 0,

        // ILP32 AArch64 relocs.
        /// Direct 32 bit.
        R_AARCH64_P32_ABS32 = 1,
        /// Copy symbol at runtime.
        R_AARCH64_P32_COPY = 180,
        /// Create GOT entry.
        R_AARCH64_P32_GLOB_DAT = 181,
        /// Create PLT entry.
        R_AARCH64_P32_JUMP_SLOT = 182,
        /// Adjust by program base.
        R_AARCH64_P32_RELATIVE = 183,
        /// Module number, 32 bit.
        R_AARCH64_P32_TLS_DTPMOD = 184,
        /// Module-relative offset, 32 bit.
        R_AARCH64_P32_TLS_DTPREL = 185,
        /// TP-relative offset, 32 bit.
        R_AARCH64_P32_TLS_TPREL = 186,
        /// TLS Descriptor.
        R_AARCH64_P32_TLSDESC = 187,
        /// STT_GNU_IFUNC relocation.
        R_AARCH64_P32_IRELATIVE = 188,

        // LP64 AArch64 relocs.
        /// Direct 64 bit.
        R_AARCH64_ABS64 = 257,
        /// Direct 32 bit.
        R_AARCH64_ABS32 = 258,
        /// Direct 16-bit.
        R_AARCH64_ABS16 = 259,
        /// PC-relative 64-bit.
        R_AARCH64_PREL64 = 260,
        /// PC-relative 32-bit.
        R_AARCH64_PREL32 = 261,
        /// PC-relative 16-bit.
        R_AARCH64_PREL16 = 262,
        /// Dir. MOVZ imm. from bits 15:0.
        R_AARCH64_MOVW_UABS_G0 = 263,
        /// Likewise for MOVK; no check.
        R_AARCH64_MOVW_UABS_G0_NC = 264,
        /// Dir. MOVZ imm. from bits 31:16.
        R_AARCH64_MOVW_UABS_G1 = 265,
        /// Likewise for MOVK; no check.
        R_AARCH64_MOVW_UABS_G1_NC = 266,
        /// Dir. MOVZ imm. from bits 47:32.
        R_AARCH64_MOVW_UABS_G2 = 267,
        /// Likewise for MOVK; no check.
        R_AARCH64_MOVW_UABS_G2_NC = 268,
        /// Dir. MOV{K,Z} imm. from 63:48.
        R_AARCH64_MOVW_UABS_G3 = 269,
        /// Dir. MOV{N,Z} imm. from 15:0.
        R_AARCH64_MOVW_SABS_G0 = 270,
        /// Dir. MOV{N,Z} imm. from 31:16.
        R_AARCH64_MOVW_SABS_G1 = 271,
        /// Dir. MOV{N,Z} imm. from 47:32.
        R_AARCH64_MOVW_SABS_G2 = 272,
        /// PC-rel. LD imm. from bits 20:2.
        R_AARCH64_LD_PREL_LO19 = 273,
        /// PC-rel. ADR imm. from bits 20:0.
        R_AARCH64_ADR_PREL_LO21 = 274,
        /// Page-rel. ADRP imm. from 32:12.
        R_AARCH64_ADR_PREL_PG_HI21 = 275,
        /// Likewise; no overflow check.
        R_AARCH64_ADR_PREL_PG_HI21_NC = 276,
        /// Dir. ADD imm. from bits 11:0.
        R_AARCH64_ADD_ABS_LO12_NC = 277,
        /// Likewise for LD/ST; no check.
        R_AARCH64_LDST8_ABS_LO12_NC = 278,
        /// PC-rel. TBZ/TBNZ imm. from 15:2.
        R_AARCH64_TSTBR14 = 279,
        /// PC-rel. cond. br. imm. from 20:2.
        R_AARCH64_CONDBR19 = 280,
        /// PC-rel. B imm. from bits 27:2.
        R_AARCH64_JUMP26 = 282,
        /// Likewise for CALL.
        R_AARCH64_CALL26 = 283,
        /// Dir. ADD imm. from bits 11:1.
        R_AARCH64_LDST16_ABS_LO12_NC = 284,
        /// Likewise for bits 11:2.
        R_AARCH64_LDST32_ABS_LO12_NC = 285,
        /// Likewise for bits 11:3.
        R_AARCH64_LDST64_ABS_LO12_NC = 286,
        /// PC-rel. MOV{N,Z} imm. from 15:0.
        R_AARCH64_MOVW_PREL_G0 = 287,
        /// Likewise for MOVK; no check.
        R_AARCH64_MOVW_PREL_G0_NC = 288,
        /// PC-rel. MOV{N,Z} imm. from 31:16.
        R_AARCH64_MOVW_PREL_G1 = 289,
        /// Likewise for MOVK; no check.
        R_AARCH64_MOVW_PREL_G1_NC = 290,
        /// PC-rel. MOV{N,Z} imm. from 47:32.
        R_AARCH64_MOVW_PREL_G2 = 291,
        /// Likewise for MOVK; no check.
        R_AARCH64_MOVW_PREL_G2_NC = 292,
        /// PC-rel. MOV{N,Z} imm. from 63:48.
        R_AARCH64_MOVW_PREL_G3 = 293,
        /// Dir. ADD imm. from bits 11:4.
        R_AARCH64_LDST128_ABS_LO12_NC = 299,
        /// GOT-rel. off. MOV{N,Z} imm. 15:0.
        R_AARCH64_MOVW_GOTOFF_G0 = 300,
        /// Likewise for MOVK; no check.
        R_AARCH64_MOVW_GOTOFF_G0_NC = 301,
        /// GOT-rel. o. MOV{N,Z} imm. 31:16.
        R_AARCH64_MOVW_GOTOFF_G1 = 302,
        /// Likewise for MOVK; no check.
        R_AARCH64_MOVW_GOTOFF_G1_NC = 303,
        /// GOT-rel. o. MOV{N,Z} imm. 47:32.
        R_AARCH64_MOVW_GOTOFF_G2 = 304,
        /// Likewise for MOVK; no check.
        R_AARCH64_MOVW_GOTOFF_G2_NC = 305,
        /// GOT-rel. o. MOV{N,Z} imm. 63:48.
        R_AARCH64_MOVW_GOTOFF_G3 = 306,
        /// GOT-relative 64-bit.
        R_AARCH64_GOTREL64 = 307,
        /// GOT-relative 32-bit.
        R_AARCH64_GOTREL32 = 308,
        /// PC-rel. GOT off. load imm. 20:2.
        R_AARCH64_GOT_LD_PREL19 = 309,
        /// GOT-rel. off. LD/ST imm. 14:3.
        R_AARCH64_LD64_GOTOFF_LO15 = 310,
        /// P-page-rel. GOT off. ADRP 32:12.
        R_AARCH64_ADR_GOT_PAGE = 311,
        /// Dir. GOT off. LD/ST imm. 11:3.
        R_AARCH64_LD64_GOT_LO12_NC = 312,
        /// GOT-page-rel. GOT off. LD/ST 14:3
        R_AARCH64_LD64_GOTPAGE_LO15 = 313,
        /// PC-relative 32-bit.
        R_AARCH64_PLT32 = 314,
        /// GOT-relative PC-relative.
        R_AARCH64_GOTPCREL32 = 315,
        /// PC-relative ADR imm. 20:0.
        R_AARCH64_TLSGD_ADR_PREL21 = 512,
        /// page-rel. ADRP imm. 32:12.
        R_AARCH64_TLSGD_ADR_PAGE21 = 513,
        /// direct ADD imm. from 11:0.
        R_AARCH64_TLSGD_ADD_LO12_NC = 514,
        /// GOT-rel. MOV{N,Z} 31:16.
        R_AARCH64_TLSGD_MOVW_G1 = 515,
        /// GOT-rel. MOVK imm. 15:0.
        R_AARCH64_TLSGD_MOVW_G0_NC = 516,
        /// Like 512; local dynamic model.
        R_AARCH64_TLSLD_ADR_PREL21 = 517,
        /// Like 513; local dynamic model.
        R_AARCH64_TLSLD_ADR_PAGE21 = 518,
        /// Like 514; local dynamic model.
        R_AARCH64_TLSLD_ADD_LO12_NC = 519,
        /// Like 515; local dynamic model.
        R_AARCH64_TLSLD_MOVW_G1 = 520,
        /// Like 516; local dynamic model.
        R_AARCH64_TLSLD_MOVW_G0_NC = 521,
        /// TLS PC-rel. load imm. 20:2.
        R_AARCH64_TLSLD_LD_PREL19 = 522,
        /// TLS DTP-rel. MOV{N,Z} 47:32.
        R_AARCH64_TLSLD_MOVW_DTPREL_G2 = 523,
        /// TLS DTP-rel. MOV{N,Z} 31:16.
        R_AARCH64_TLSLD_MOVW_DTPREL_G1 = 524,
        /// Likewise; MOVK; no check.
        R_AARCH64_TLSLD_MOVW_DTPREL_G1_NC = 525,
        /// TLS DTP-rel. MOV{N,Z} 15:0.
        R_AARCH64_TLSLD_MOVW_DTPREL_G0 = 526,
        /// Likewise; MOVK; no check.
        R_AARCH64_TLSLD_MOVW_DTPREL_G0_NC = 527,
        /// DTP-rel. ADD imm. from 23:12.
        R_AARCH64_TLSLD_ADD_DTPREL_HI12 = 528,
        /// DTP-rel. ADD imm. from 11:0.
        R_AARCH64_TLSLD_ADD_DTPREL_LO12 = 529,
        /// Likewise; no ovfl. check.
        R_AARCH64_TLSLD_ADD_DTPREL_LO12_NC = 530,
        /// DTP-rel. LD/ST imm. 11:0.
        R_AARCH64_TLSLD_LDST8_DTPREL_LO12 = 531,
        /// Likewise; no check.
        R_AARCH64_TLSLD_LDST8_DTPREL_LO12_NC = 532,
        /// DTP-rel. LD/ST imm. 11:1.
        R_AARCH64_TLSLD_LDST16_DTPREL_LO12 = 533,
        /// Likewise; no check.
        R_AARCH64_TLSLD_LDST16_DTPREL_LO12_NC = 534,
        /// DTP-rel. LD/ST imm. 11:2.
        R_AARCH64_TLSLD_LDST32_DTPREL_LO12 = 535,
        /// Likewise; no check.
        R_AARCH64_TLSLD_LDST32_DTPREL_LO12_NC = 536,
        /// DTP-rel. LD/ST imm. 11:3.
        R_AARCH64_TLSLD_LDST64_DTPREL_LO12 = 537,
        /// Likewise; no check.
        R_AARCH64_TLSLD_LDST64_DTPREL_LO12_NC = 538,
        /// GOT-rel. MOV{N,Z} 31:16.
        R_AARCH64_TLSIE_MOVW_GOTTPREL_G1 = 539,
        /// GOT-rel. MOVK 15:0.
        R_AARCH64_TLSIE_MOVW_GOTTPREL_G0_NC = 540,
        /// Page-rel. ADRP 32:12.
        R_AARCH64_TLSIE_ADR_GOTTPREL_PAGE21 = 541,
        /// Direct LD off. 11:3.
        R_AARCH64_TLSIE_LD64_GOTTPREL_LO12_NC = 542,
        /// PC-rel. load imm. 20:2.
        R_AARCH64_TLSIE_LD_GOTTPREL_PREL19 = 543,
        /// TLS TP-rel. MOV{N,Z} 47:32.
        R_AARCH64_TLSLE_MOVW_TPREL_G2 = 544,
        /// TLS TP-rel. MOV{N,Z} 31:16.
        R_AARCH64_TLSLE_MOVW_TPREL_G1 = 545,
        /// Likewise; MOVK; no check.
        R_AARCH64_TLSLE_MOVW_TPREL_G1_NC = 546,
        /// TLS TP-rel. MOV{N,Z} 15:0.
        R_AARCH64_TLSLE_MOVW_TPREL_G0 = 547,
        /// Likewise; MOVK; no check.
        R_AARCH64_TLSLE_MOVW_TPREL_G0_NC = 548,
        /// TP-rel. ADD imm. 23:12.
        R_AARCH64_TLSLE_ADD_TPREL_HI12 = 549,
        /// TP-rel. ADD imm. 11:0.
        R_AARCH64_TLSLE_ADD_TPREL_LO12 = 550,
        /// Likewise; no ovfl. check.
        R_AARCH64_TLSLE_ADD_TPREL_LO12_NC = 551,
        /// TP-rel. LD/ST off. 11:0.
        R_AARCH64_TLSLE_LDST8_TPREL_LO12 = 552,
        /// Likewise; no ovfl. check.
        R_AARCH64_TLSLE_LDST8_TPREL_LO12_NC = 553,
        /// TP-rel. LD/ST off. 11:1.
        R_AARCH64_TLSLE_LDST16_TPREL_LO12 = 554,
        /// Likewise; no check.
        R_AARCH64_TLSLE_LDST16_TPREL_LO12_NC = 555,
        /// TP-rel. LD/ST off. 11:2.
        R_AARCH64_TLSLE_LDST32_TPREL_LO12 = 556,
        /// Likewise; no check.
        R_AARCH64_TLSLE_LDST32_TPREL_LO12_NC = 557,
        /// TP-rel. LD/ST off. 11:3.
        R_AARCH64_TLSLE_LDST64_TPREL_LO12 = 558,
        /// Likewise; no check.
        R_AARCH64_TLSLE_LDST64_TPREL_LO12_NC = 559,
        /// PC-rel. load immediate 20:2.
        R_AARCH64_TLSDESC_LD_PREL19 = 560,
        /// PC-rel. ADR immediate 20:0.
        R_AARCH64_TLSDESC_ADR_PREL21 = 561,
        /// Page-rel. ADRP imm. 32:12.
        R_AARCH64_TLSDESC_ADR_PAGE21 = 562,
        /// Direct LD off. from 11:3.
        R_AARCH64_TLSDESC_LD64_LO12 = 563,
        /// Direct ADD imm. from 11:0.
        R_AARCH64_TLSDESC_ADD_LO12 = 564,
        /// GOT-rel. MOV{N,Z} imm. 31:16.
        R_AARCH64_TLSDESC_OFF_G1 = 565,
        /// GOT-rel. MOVK imm. 15:0; no ck.
        R_AARCH64_TLSDESC_OFF_G0_NC = 566,
        /// Relax LDR.
        R_AARCH64_TLSDESC_LDR = 567,
        /// Relax ADD.
        R_AARCH64_TLSDESC_ADD = 568,
        /// Relax BLR.
        R_AARCH64_TLSDESC_CALL = 569,
        /// TP-rel. LD/ST off. 11:4.
        R_AARCH64_TLSLE_LDST128_TPREL_LO12 = 570,
        /// Likewise; no check.
        R_AARCH64_TLSLE_LDST128_TPREL_LO12_NC = 571,
        /// DTP-rel. LD/ST imm. 11:4.
        R_AARCH64_TLSLD_LDST128_DTPREL_LO12 = 572,
        /// Likewise; no check.
        R_AARCH64_TLSLD_LDST128_DTPREL_LO12_NC = 573,
        /// Copy symbol at runtime.
        R_AARCH64_COPY = 1024,
        /// Create GOT entry.
        R_AARCH64_GLOB_DAT = 1025,
        /// Create PLT entry.
        R_AARCH64_JUMP_SLOT = 1026,
        /// Adjust by program base.
        R_AARCH64_RELATIVE = 1027,
        /// Module number, 64 bit.
        R_AARCH64_TLS_DTPMOD = 1028,
        /// Module-relative offset, 64 bit.
        R_AARCH64_TLS_DTPREL = 1029,
        /// TP-relative offset, 64 bit.
        R_AARCH64_TLS_TPREL = 1030,
        /// TLS Descriptor.
        R_AARCH64_TLSDESC = 1031,
        /// STT_GNU_IFUNC relocation.
        R_AARCH64_IRELATIVE = 1032,
    };
}

pub const DT_AARCH64_NUM: i64 = 6;

names! {
    struct Avr(Base);
    flags ef: FileFlags(u32) = {
        /// If set, it is assumed that the elf file uses local symbols as reference
        /// for the relocations so that linker relaxation is possible.
        EF_AVR_LINKRELAX_PREPARED = 0x80,

        /// Bitmask for `EF_AVR_ARCH_*`.
        EF_AVR_ARCH = 0x7F => NAMES_EF_AVR_ARCH,
    };
    /// `Rel*::r_type` values for `EM_AVR`.
    consts r = pub NAMES_R_AVR: RelocationType(u32) = {
        R_AVR_NONE = 0,
        /// Direct 32 bit
        R_AVR_32 = 1,
        R_AVR_7_PCREL = 2,
        R_AVR_13_PCREL = 3,
        /// Direct 16 bit
        R_AVR_16 = 4,
        R_AVR_16_PM = 5,
        R_AVR_LO8_LDI = 6,
        R_AVR_HI8_LDI = 7,
        R_AVR_HH8_LDI = 8,
        R_AVR_LO8_LDI_NEG = 9,
        R_AVR_HI8_LDI_NEG = 10,
        R_AVR_HH8_LDI_NEG = 11,
        R_AVR_LO8_LDI_PM = 12,
        R_AVR_HI8_LDI_PM = 13,
        R_AVR_HH8_LDI_PM = 14,
        R_AVR_LO8_LDI_PM_NEG = 15,
        R_AVR_HI8_LDI_PM_NEG = 16,
        R_AVR_HH8_LDI_PM_NEG = 17,
        R_AVR_CALL = 18,
        R_AVR_LDI = 19,
        R_AVR_6 = 20,
        R_AVR_6_ADIW = 21,
        R_AVR_MS8_LDI = 22,
        R_AVR_MS8_LDI_NEG = 23,
        R_AVR_LO8_LDI_GS = 24,
        R_AVR_HI8_LDI_GS = 25,
        R_AVR_8 = 26,
        R_AVR_8_LO8 = 27,
        R_AVR_8_HI8 = 28,
        R_AVR_8_HLO8 = 29,
        R_AVR_DIFF8 = 30,
        R_AVR_DIFF16 = 31,
        R_AVR_DIFF32 = 32,
        R_AVR_LDS_STS_16 = 33,
        R_AVR_PORT6 = 34,
        R_AVR_PORT5 = 35,
        R_AVR_32_PCREL = 36,
    };
}

constant_names!(NAMES_EF_AVR_ARCH: FileFlags(u32) = {
    EF_AVR_ARCH_AVR1 = 1,
    EF_AVR_ARCH_AVR2 = 2,
    EF_AVR_ARCH_AVR25 = 25,
    EF_AVR_ARCH_AVR3 = 3,
    EF_AVR_ARCH_AVR31 = 31,
    EF_AVR_ARCH_AVR35 = 35,
    EF_AVR_ARCH_AVR4 = 4,
    EF_AVR_ARCH_AVR5 = 5,
    EF_AVR_ARCH_AVR51 = 51,
    EF_AVR_ARCH_AVR6 = 6,
    EF_AVR_ARCH_AVRTINY = 100,
    EF_AVR_ARCH_XMEGA1 = 101,
    EF_AVR_ARCH_XMEGA2 = 102,
    EF_AVR_ARCH_XMEGA3 = 103,
    EF_AVR_ARCH_XMEGA4 = 104,
    EF_AVR_ARCH_XMEGA5 = 105,
    EF_AVR_ARCH_XMEGA6 = 106,
    EF_AVR_ARCH_XMEGA7 = 107,
});

names! {
    struct Msp430(Base);
    /// `Rel*::r_type` values for `EM_MSP430`.
    consts r = pub NAMES_R_MSP430: RelocationType(u32) = {
        /// No reloc
        R_MSP430_NONE = 0,
        /// Direct 32 bit
        R_MSP430_32 = 1,
        /// Direct 16 bit
        R_MSP430_16_BYTE = 5,
    };
}

names! {
    struct Hex(Base);
    /// `Rel*::r_type` values for `EM_HEXAGON`.
    consts r = pub NAMES_R_HEX: RelocationType(u32) = {
        /// No reloc
        R_HEX_NONE = 0,
        /// Direct 32 bit
        R_HEX_32 = 6,
    };
}

names! {
    struct Csky(Base);
    /// `Rel*::r_type` values for `EM_CSKY`.
    consts r = pub NAMES_R_CKCORE: RelocationType(u32) = {
        /// no reloc
        R_CKCORE_NONE = 0,
        /// direct 32 bit (S + A)
        R_CKCORE_ADDR32 = 1,
        /// disp ((S + A - P) >> 2) & 0xff
        R_CKCORE_PCRELIMM8BY4 = 2,
        /// disp ((S + A - P) >> 1) & 0x7ff
        R_CKCORE_PCRELIMM11BY2 = 3,
        /// 32-bit rel (S + A - P)
        R_CKCORE_PCREL32 = 5,
        /// disp ((S + A - P) >>1) & 0x7ff
        R_CKCORE_PCRELJSR_IMM11BY2 = 6,
        /// 32 bit adjust program base(B + A)
        R_CKCORE_RELATIVE = 9,
        /// 32 bit adjust by program base
        R_CKCORE_COPY = 10,
        /// off between got and sym (S)
        R_CKCORE_GLOB_DAT = 11,
        /// PLT entry (S)
        R_CKCORE_JUMP_SLOT = 12,
        /// offset to GOT (S + A - GOT)
        R_CKCORE_GOTOFF = 13,
        /// PC offset to GOT (GOT + A - P)
        R_CKCORE_GOTPC = 14,
        /// 32 bit GOT entry (G)
        R_CKCORE_GOT32 = 15,
        /// 32 bit PLT entry (G)
        R_CKCORE_PLT32 = 16,
        /// GOT entry in GLOB_DAT (GOT + G)
        R_CKCORE_ADDRGOT = 17,
        /// PLT entry in GLOB_DAT (GOT + G)
        R_CKCORE_ADDRPLT = 18,
        /// ((S + A - P) >> 1) & 0x3ff_ffff
        R_CKCORE_PCREL_IMM26BY2 = 19,
        /// disp ((S + A - P) >> 1) & 0xffff
        R_CKCORE_PCREL_IMM16BY2 = 20,
        /// disp ((S + A - P) >> 2) & 0xffff
        R_CKCORE_PCREL_IMM16BY4 = 21,
        /// disp ((S + A - P) >> 1) & 0x3ff
        R_CKCORE_PCREL_IMM10BY2 = 22,
        /// disp ((S + A - P) >> 2) & 0x3ff
        R_CKCORE_PCREL_IMM10BY4 = 23,
        /// high & low 16 bit ADDR, ((S + A) >> 16) & 0xffff
        R_CKCORE_ADDR_HI16 = 24,
        /// (S + A) & 0xffff
        R_CKCORE_ADDR_LO16 = 25,
        /// high & low 16 bit GOTPC, ((GOT + A - P) >> 16) & 0xffff
        R_CKCORE_GOTPC_HI16 = 26,
        /// (GOT + A - P) & 0xffff
        R_CKCORE_GOTPC_LO16 = 27,
        /// high & low 16 bit GOTOFF, ((S + A - GOT) >> 16) & 0xffff
        R_CKCORE_GOTOFF_HI16 = 28,
        /// (S + A - GOT) & 0xffff
        R_CKCORE_GOTOFF_LO16 = 29,
        /// 12 bit disp GOT entry (G)
        R_CKCORE_GOT12 = 30,
        /// high & low 16 bit GOT, (G >> 16) & 0xffff
        R_CKCORE_GOT_HI16 = 31,
        /// (G & 0xffff)
        R_CKCORE_GOT_LO16 = 32,
        /// 12 bit disp PLT entry (G)
        R_CKCORE_PLT12 = 33,
        /// high & low 16 bit PLT, (G >> 16) & 0xffff
        R_CKCORE_PLT_HI16 = 34,
        /// G & 0xffff
        R_CKCORE_PLT_LO16 = 35,
        /// high & low 16 bit ADDRGOT, (GOT + G * 4) & 0xffff
        R_CKCORE_ADDRGOT_HI16 = 36,
        /// (GOT + G * 4) & 0xffff
        R_CKCORE_ADDRGOT_LO16 = 37,
        /// high & low 16 bit ADDRPLT, ((GOT + G * 4) >> 16) & 0xFFFF
        R_CKCORE_ADDRPLT_HI16 = 38,
        /// (GOT+G*4) & 0xffff
        R_CKCORE_ADDRPLT_LO16 = 39,
        /// disp ((S+A-P) >>1) & x3ff_ffff
        R_CKCORE_PCREL_JSR_IMM26BY2 = 40,
        /// (S+A-BTEXT) & 0xffff
        R_CKCORE_TOFFSET_LO16 = 41,
        /// (S+A-BTEXT) & 0xffff
        R_CKCORE_DOFFSET_LO16 = 42,
        /// disp ((S+A-P) >>1) & 0x3ffff
        R_CKCORE_PCREL_IMM18BY2 = 43,
        /// disp (S+A-BDATA) & 0x3ffff
        R_CKCORE_DOFFSET_IMM18 = 44,
        /// disp ((S+A-BDATA)>>1) & 0x3ffff
        R_CKCORE_DOFFSET_IMM18BY2 = 45,
        /// disp ((S+A-BDATA)>>2) & 0x3ffff
        R_CKCORE_DOFFSET_IMM18BY4 = 46,
        /// disp (G >> 2)
        R_CKCORE_GOT_IMM18BY4 = 48,
        /// disp (G >> 2)
        R_CKCORE_PLT_IMM18BY4 = 49,
        /// disp ((S+A-P) >>2) & 0x7f
        R_CKCORE_PCREL_IMM7BY4 = 50,
        /// 32 bit offset to TLS block
        R_CKCORE_TLS_LE32 = 51,
        R_CKCORE_TLS_IE32 = 52,
        R_CKCORE_TLS_GD32 = 53,
        R_CKCORE_TLS_LDM32 = 54,
        R_CKCORE_TLS_LDO32 = 55,
        R_CKCORE_TLS_DTPMOD32 = 56,
        R_CKCORE_TLS_DTPOFF32 = 57,
        R_CKCORE_TLS_TPOFF32 = 58,
    };
    flags ef: FileFlags(u32) = {
        _ = EF_CSKY_ABIMASK => NAMES_EF_CSKY_ABI,
    };
    consts sht: SectionType(u32) = {
        /// C-SKY attributes section.
        SHT_CSKY_ATTRIBUTES = SHT_LOPROC + 1,
    };
}

pub const EF_CSKY_ABIMASK: u32 = 0xF000_0000;
pub const EF_CSKY_OTHER: u32 = 0x0FFF_0000;
pub const EF_CSKY_PROCESSOR: u32 = 0x0000_FFFF;

constant_names!(NAMES_EF_CSKY_ABI: FileFlags(u32) = {
    EF_CSKY_ABIV1 = 0x1000_0000,
    EF_CSKY_ABIV2 = 0x2000_0000,
});

// IA-64 specific declarations.

names! {
    struct Ia64(Base);
    flags ef: FileFlags(u32) = {
        /// 64-bit ABI
        EF_IA_64_ABI64 = 0x0000_0010,
    };
    consts pt: ProgramType(u32) = {
        /// arch extension bits
        PT_IA_64_ARCHEXT = PT_LOPROC + 0,
        /// ia64 unwind bits
        PT_IA_64_UNWIND = PT_LOPROC + 1,
        PT_IA_64_HP_OPT_ANOT = PT_LOOS + 0x12,
        PT_IA_64_HP_HSL_ANOT = PT_LOOS + 0x13,
        PT_IA_64_HP_STACK = PT_LOOS + 0x14,
    };
    flags pf: ProgramFlags(u32) = {
        /// spec insns w/o recovery
        PF_IA_64_NORECOV = 0x8000_0000,
    };
    consts sht: SectionType(u32) = {
        /// extension bits
        SHT_IA_64_EXT = SHT_LOPROC + 0,
        /// unwind bits
        SHT_IA_64_UNWIND = SHT_LOPROC + 1,
    };
    flags shf: SectionFlags(u64) = {
        /// section near gp
        SHF_IA_64_SHORT = 0x1000_0000,
        /// spec insns w/o recovery
        SHF_IA_64_NORECOV = 0x2000_0000,
    };
    consts dt: DynamicTag(i64) = {
        DT_IA_64_PLT_RESERVE = DT_LOPROC + 0,
    };
    /// `Rel*::r_type` values for `EM_IA_64`.
    consts r = pub NAMES_R_IA64: RelocationType(u32) = {
        /// none
        R_IA64_NONE = 0x00,
        /// symbol + addend, add imm14
        R_IA64_IMM14 = 0x21,
        /// symbol + addend, add imm22
        R_IA64_IMM22 = 0x22,
        /// symbol + addend, mov imm64
        R_IA64_IMM64 = 0x23,
        /// symbol + addend, data4 MSB
        R_IA64_DIR32MSB = 0x24,
        /// symbol + addend, data4 LSB
        R_IA64_DIR32LSB = 0x25,
        /// symbol + addend, data8 MSB
        R_IA64_DIR64MSB = 0x26,
        /// symbol + addend, data8 LSB
        R_IA64_DIR64LSB = 0x27,
        /// @gprel(sym + add), add imm22
        R_IA64_GPREL22 = 0x2a,
        /// @gprel(sym + add), mov imm64
        R_IA64_GPREL64I = 0x2b,
        /// @gprel(sym + add), data4 MSB
        R_IA64_GPREL32MSB = 0x2c,
        /// @gprel(sym + add), data4 LSB
        R_IA64_GPREL32LSB = 0x2d,
        /// @gprel(sym + add), data8 MSB
        R_IA64_GPREL64MSB = 0x2e,
        /// @gprel(sym + add), data8 LSB
        R_IA64_GPREL64LSB = 0x2f,
        /// @ltoff(sym + add), add imm22
        R_IA64_LTOFF22 = 0x32,
        /// @ltoff(sym + add), mov imm64
        R_IA64_LTOFF64I = 0x33,
        /// @pltoff(sym + add), add imm22
        R_IA64_PLTOFF22 = 0x3a,
        /// @pltoff(sym + add), mov imm64
        R_IA64_PLTOFF64I = 0x3b,
        /// @pltoff(sym + add), data8 MSB
        R_IA64_PLTOFF64MSB = 0x3e,
        /// @pltoff(sym + add), data8 LSB
        R_IA64_PLTOFF64LSB = 0x3f,
        /// @fptr(sym + add), mov imm64
        R_IA64_FPTR64I = 0x43,
        /// @fptr(sym + add), data4 MSB
        R_IA64_FPTR32MSB = 0x44,
        /// @fptr(sym + add), data4 LSB
        R_IA64_FPTR32LSB = 0x45,
        /// @fptr(sym + add), data8 MSB
        R_IA64_FPTR64MSB = 0x46,
        /// @fptr(sym + add), data8 LSB
        R_IA64_FPTR64LSB = 0x47,
        /// @pcrel(sym + add), brl
        R_IA64_PCREL60B = 0x48,
        /// @pcrel(sym + add), ptb, call
        R_IA64_PCREL21B = 0x49,
        /// @pcrel(sym + add), chk.s
        R_IA64_PCREL21M = 0x4a,
        /// @pcrel(sym + add), fchkf
        R_IA64_PCREL21F = 0x4b,
        /// @pcrel(sym + add), data4 MSB
        R_IA64_PCREL32MSB = 0x4c,
        /// @pcrel(sym + add), data4 LSB
        R_IA64_PCREL32LSB = 0x4d,
        /// @pcrel(sym + add), data8 MSB
        R_IA64_PCREL64MSB = 0x4e,
        /// @pcrel(sym + add), data8 LSB
        R_IA64_PCREL64LSB = 0x4f,
        /// @ltoff(@fptr(s+a)), imm22
        R_IA64_LTOFF_FPTR22 = 0x52,
        /// @ltoff(@fptr(s+a)), imm64
        R_IA64_LTOFF_FPTR64I = 0x53,
        /// @ltoff(@fptr(s+a)), data4 MSB
        R_IA64_LTOFF_FPTR32MSB = 0x54,
        /// @ltoff(@fptr(s+a)), data4 LSB
        R_IA64_LTOFF_FPTR32LSB = 0x55,
        /// @ltoff(@fptr(s+a)), data8 MSB
        R_IA64_LTOFF_FPTR64MSB = 0x56,
        /// @ltoff(@fptr(s+a)), data8 LSB
        R_IA64_LTOFF_FPTR64LSB = 0x57,
        /// @segrel(sym + add), data4 MSB
        R_IA64_SEGREL32MSB = 0x5c,
        /// @segrel(sym + add), data4 LSB
        R_IA64_SEGREL32LSB = 0x5d,
        /// @segrel(sym + add), data8 MSB
        R_IA64_SEGREL64MSB = 0x5e,
        /// @segrel(sym + add), data8 LSB
        R_IA64_SEGREL64LSB = 0x5f,
        /// @secrel(sym + add), data4 MSB
        R_IA64_SECREL32MSB = 0x64,
        /// @secrel(sym + add), data4 LSB
        R_IA64_SECREL32LSB = 0x65,
        /// @secrel(sym + add), data8 MSB
        R_IA64_SECREL64MSB = 0x66,
        /// @secrel(sym + add), data8 LSB
        R_IA64_SECREL64LSB = 0x67,
        /// data 4 + REL
        R_IA64_REL32MSB = 0x6c,
        /// data 4 + REL
        R_IA64_REL32LSB = 0x6d,
        /// data 8 + REL
        R_IA64_REL64MSB = 0x6e,
        /// data 8 + REL
        R_IA64_REL64LSB = 0x6f,
        /// symbol + addend, data4 MSB
        R_IA64_LTV32MSB = 0x74,
        /// symbol + addend, data4 LSB
        R_IA64_LTV32LSB = 0x75,
        /// symbol + addend, data8 MSB
        R_IA64_LTV64MSB = 0x76,
        /// symbol + addend, data8 LSB
        R_IA64_LTV64LSB = 0x77,
        /// @pcrel(sym + add), 21bit inst
        R_IA64_PCREL21BI = 0x79,
        /// @pcrel(sym + add), 22bit inst
        R_IA64_PCREL22 = 0x7a,
        /// @pcrel(sym + add), 64bit inst
        R_IA64_PCREL64I = 0x7b,
        /// dynamic reloc, imported PLT, MSB
        R_IA64_IPLTMSB = 0x80,
        /// dynamic reloc, imported PLT, LSB
        R_IA64_IPLTLSB = 0x81,
        /// copy relocation
        R_IA64_COPY = 0x84,
        /// Addend and symbol difference
        R_IA64_SUB = 0x85,
        /// LTOFF22, relaxable.
        R_IA64_LTOFF22X = 0x86,
        /// Use of LTOFF22X.
        R_IA64_LDXMOV = 0x87,
        /// @tprel(sym + add), imm14
        R_IA64_TPREL14 = 0x91,
        /// @tprel(sym + add), imm22
        R_IA64_TPREL22 = 0x92,
        /// @tprel(sym + add), imm64
        R_IA64_TPREL64I = 0x93,
        /// @tprel(sym + add), data8 MSB
        R_IA64_TPREL64MSB = 0x96,
        /// @tprel(sym + add), data8 LSB
        R_IA64_TPREL64LSB = 0x97,
        /// @ltoff(@tprel(s+a)), imm2
        R_IA64_LTOFF_TPREL22 = 0x9a,
        /// @dtpmod(sym + add), data8 MSB
        R_IA64_DTPMOD64MSB = 0xa6,
        /// @dtpmod(sym + add), data8 LSB
        R_IA64_DTPMOD64LSB = 0xa7,
        /// @ltoff(@dtpmod(sym + add)), imm22
        R_IA64_LTOFF_DTPMOD22 = 0xaa,
        /// @dtprel(sym + add), imm14
        R_IA64_DTPREL14 = 0xb1,
        /// @dtprel(sym + add), imm22
        R_IA64_DTPREL22 = 0xb2,
        /// @dtprel(sym + add), imm64
        R_IA64_DTPREL64I = 0xb3,
        /// @dtprel(sym + add), data4 MSB
        R_IA64_DTPREL32MSB = 0xb4,
        /// @dtprel(sym + add), data4 LSB
        R_IA64_DTPREL32LSB = 0xb5,
        /// @dtprel(sym + add), data8 MSB
        R_IA64_DTPREL64MSB = 0xb6,
        /// @dtprel(sym + add), data8 LSB
        R_IA64_DTPREL64LSB = 0xb7,
        /// @ltoff(@dtprel(s+a)), imm22
        R_IA64_LTOFF_DTPREL22 = 0xba,
    };
}

/// os-specific flags
pub const EF_IA_64_MASKOS: u32 = 0x0000_000f;
/// arch. version mask
pub const EF_IA_64_ARCH: u32 = 0xff00_0000;

// SH specific declarations.

names! {
    struct Sh(Base);
    flags ef: FileFlags(u32) = {
        EF_SH_MACH_MASK = 0x1f => NAMES_EF_SH_MACH,
    };
    /// `Rel*::r_type` values for `EM_SH`.
    consts r = pub NAMES_R_SH: RelocationType(u32) = {
        R_SH_NONE = 0,
        R_SH_DIR32 = 1,
        R_SH_REL32 = 2,
        R_SH_DIR8WPN = 3,
        R_SH_IND12W = 4,
        R_SH_DIR8WPL = 5,
        R_SH_DIR8WPZ = 6,
        R_SH_DIR8BP = 7,
        R_SH_DIR8W = 8,
        R_SH_DIR8L = 9,
        R_SH_SWITCH16 = 25,
        R_SH_SWITCH32 = 26,
        R_SH_USES = 27,
        R_SH_COUNT = 28,
        R_SH_ALIGN = 29,
        R_SH_CODE = 30,
        R_SH_DATA = 31,
        R_SH_LABEL = 32,
        R_SH_SWITCH8 = 33,
        R_SH_GNU_VTINHERIT = 34,
        R_SH_GNU_VTENTRY = 35,
        R_SH_TLS_GD_32 = 144,
        R_SH_TLS_LD_32 = 145,
        R_SH_TLS_LDO_32 = 146,
        R_SH_TLS_IE_32 = 147,
        R_SH_TLS_LE_32 = 148,
        R_SH_TLS_DTPMOD32 = 149,
        R_SH_TLS_DTPOFF32 = 150,
        R_SH_TLS_TPOFF32 = 151,
        R_SH_GOT32 = 160,
        R_SH_PLT32 = 161,
        R_SH_COPY = 162,
        R_SH_GLOB_DAT = 163,
        R_SH_JMP_SLOT = 164,
        R_SH_RELATIVE = 165,
        R_SH_GOTOFF = 166,
        R_SH_GOTPC = 167,
    };
}

// S/390 specific definitions.

names! {
    struct S390(Base);
    flags ef: FileFlags(u32) = {
        /// High GPRs kernel facility needed.
        EF_S390_HIGH_GPRS = 0x0000_0001,
    };
    /// `Rel*::r_type` values for `EM_S390`.
    consts r = pub NAMES_R_390: RelocationType(u32) = {
        /// No reloc.
        R_390_NONE = 0,
        /// Direct 8 bit.
        R_390_8 = 1,
        /// Direct 12 bit.
        R_390_12 = 2,
        /// Direct 16 bit.
        R_390_16 = 3,
        /// Direct 32 bit.
        R_390_32 = 4,
        /// PC relative 32 bit.
        R_390_PC32 = 5,
        /// 12 bit GOT offset.
        R_390_GOT12 = 6,
        /// 32 bit GOT offset.
        R_390_GOT32 = 7,
        /// 32 bit PC relative PLT address.
        R_390_PLT32 = 8,
        /// Copy symbol at runtime.
        R_390_COPY = 9,
        /// Create GOT entry.
        R_390_GLOB_DAT = 10,
        /// Create PLT entry.
        R_390_JMP_SLOT = 11,
        /// Adjust by program base.
        R_390_RELATIVE = 12,
        /// 32 bit offset to GOT.
        R_390_GOTOFF32 = 13,
        /// 32 bit PC relative offset to GOT.
        R_390_GOTPC = 14,
        /// 16 bit GOT offset.
        R_390_GOT16 = 15,
        /// PC relative 16 bit.
        R_390_PC16 = 16,
        /// PC relative 16 bit shifted by 1.
        R_390_PC16DBL = 17,
        /// 16 bit PC rel. PLT shifted by 1.
        R_390_PLT16DBL = 18,
        /// PC relative 32 bit shifted by 1.
        R_390_PC32DBL = 19,
        /// 32 bit PC rel. PLT shifted by 1.
        R_390_PLT32DBL = 20,
        /// 32 bit PC rel. GOT shifted by 1.
        R_390_GOTPCDBL = 21,
        /// Direct 64 bit.
        R_390_64 = 22,
        /// PC relative 64 bit.
        R_390_PC64 = 23,
        /// 64 bit GOT offset.
        R_390_GOT64 = 24,
        /// 64 bit PC relative PLT address.
        R_390_PLT64 = 25,
        /// 32 bit PC rel. to GOT entry >> 1.
        R_390_GOTENT = 26,
        /// 16 bit offset to GOT.
        R_390_GOTOFF16 = 27,
        /// 64 bit offset to GOT.
        R_390_GOTOFF64 = 28,
        /// 12 bit offset to jump slot.
        R_390_GOTPLT12 = 29,
        /// 16 bit offset to jump slot.
        R_390_GOTPLT16 = 30,
        /// 32 bit offset to jump slot.
        R_390_GOTPLT32 = 31,
        /// 64 bit offset to jump slot.
        R_390_GOTPLT64 = 32,
        /// 32 bit rel. offset to jump slot.
        R_390_GOTPLTENT = 33,
        /// 16 bit offset from GOT to PLT.
        R_390_PLTOFF16 = 34,
        /// 32 bit offset from GOT to PLT.
        R_390_PLTOFF32 = 35,
        /// 16 bit offset from GOT to PLT.
        R_390_PLTOFF64 = 36,
        /// Tag for load insn in TLS code.
        R_390_TLS_LOAD = 37,
        /// Tag for function call in general dynamic TLS code.
        R_390_TLS_GDCALL = 38,
        /// Tag for function call in local dynamic TLS code.
        R_390_TLS_LDCALL = 39,
        /// Direct 32 bit for general dynamic thread local data.
        R_390_TLS_GD32 = 40,
        /// Direct 64 bit for general dynamic thread local data.
        R_390_TLS_GD64 = 41,
        /// 12 bit GOT offset for static TLS block offset.
        R_390_TLS_GOTIE12 = 42,
        /// 32 bit GOT offset for static TLS block offset.
        R_390_TLS_GOTIE32 = 43,
        /// 64 bit GOT offset for static TLS block offset.
        R_390_TLS_GOTIE64 = 44,
        /// Direct 32 bit for local dynamic thread local data in LE code.
        R_390_TLS_LDM32 = 45,
        /// Direct 64 bit for local dynamic thread local data in LE code.
        R_390_TLS_LDM64 = 46,
        /// 32 bit address of GOT entry for negated static TLS block offset.
        R_390_TLS_IE32 = 47,
        /// 64 bit address of GOT entry for negated static TLS block offset.
        R_390_TLS_IE64 = 48,
        /// 32 bit rel. offset to GOT entry for negated static TLS block offset.
        R_390_TLS_IEENT = 49,
        /// 32 bit negated offset relative to static TLS block.
        R_390_TLS_LE32 = 50,
        /// 64 bit negated offset relative to static TLS block.
        R_390_TLS_LE64 = 51,
        /// 32 bit offset relative to TLS block.
        R_390_TLS_LDO32 = 52,
        /// 64 bit offset relative to TLS block.
        R_390_TLS_LDO64 = 53,
        /// ID of module containing symbol.
        R_390_TLS_DTPMOD = 54,
        /// Offset in TLS block.
        R_390_TLS_DTPOFF = 55,
        /// Negated offset in static TLS block.
        R_390_TLS_TPOFF = 56,
        /// Direct 20 bit.
        R_390_20 = 57,
        /// 20 bit GOT offset.
        R_390_GOT20 = 58,
        /// 20 bit offset to jump slot.
        R_390_GOTPLT20 = 59,
        /// 20 bit GOT offset for static TLS block offset.
        R_390_TLS_GOTIE20 = 60,
        /// STT_GNU_IFUNC relocation.
        R_390_IRELATIVE = 61,
    };
}

constant_names!(NAMES_EF_SH_MACH: FileFlags(u32) = {
    EF_SH_UNKNOWN = 0x0,
    EF_SH1 = 0x1,
    EF_SH2 = 0x2,
    EF_SH3 = 0x3,
    EF_SH_DSP = 0x4,
    EF_SH3_DSP = 0x5,
    EF_SH4AL_DSP = 0x6,
    EF_SH3E = 0x8,
    EF_SH4 = 0x9,
    EF_SH2E = 0xb,
    EF_SH4A = 0xc,
    EF_SH2A = 0xd,
    EF_SH4_NOFPU = 0x10,
    EF_SH4A_NOFPU = 0x11,
    EF_SH4_NOMMU_NOFPU = 0x12,
    EF_SH2A_NOFPU = 0x13,
    EF_SH3_NOMMU = 0x14,
    EF_SH2A_SH4_NOFPU = 0x15,
    EF_SH2A_SH3_NOFPU = 0x16,
    EF_SH2A_SH4 = 0x17,
    EF_SH2A_SH3E = 0x18,
});

names! {
    struct Cris(Base);
    /// `Rel*::r_type` values for `EM_CRIS`.
    consts r = pub NAMES_R_CRIS: RelocationType(u32) = {
        R_CRIS_NONE = 0,
        R_CRIS_8 = 1,
        R_CRIS_16 = 2,
        R_CRIS_32 = 3,
        R_CRIS_8_PCREL = 4,
        R_CRIS_16_PCREL = 5,
        R_CRIS_32_PCREL = 6,
        R_CRIS_GNU_VTINHERIT = 7,
        R_CRIS_GNU_VTENTRY = 8,
        R_CRIS_COPY = 9,
        R_CRIS_GLOB_DAT = 10,
        R_CRIS_JUMP_SLOT = 11,
        R_CRIS_RELATIVE = 12,
        R_CRIS_16_GOT = 13,
        R_CRIS_32_GOT = 14,
        R_CRIS_16_GOTPLT = 15,
        R_CRIS_32_GOTPLT = 16,
        R_CRIS_32_GOTREL = 17,
        R_CRIS_32_PLT_GOTREL = 18,
        R_CRIS_32_PLT_PCREL = 19,
    };
}

names! {
    struct X86_64(Base);
    /// `Rel*::r_type` values for `EM_X86_64`.
    consts r = pub NAMES_R_X86_64: RelocationType(u32) = {
        /// No reloc
        R_X86_64_NONE = 0,
        /// Direct 64 bit
        R_X86_64_64 = 1,
        /// PC relative 32 bit signed
        R_X86_64_PC32 = 2,
        /// 32 bit GOT entry
        R_X86_64_GOT32 = 3,
        /// 32 bit PLT address
        R_X86_64_PLT32 = 4,
        /// Copy symbol at runtime
        R_X86_64_COPY = 5,
        /// Create GOT entry
        R_X86_64_GLOB_DAT = 6,
        /// Create PLT entry
        R_X86_64_JUMP_SLOT = 7,
        /// Adjust by program base
        R_X86_64_RELATIVE = 8,
        /// 32 bit signed PC relative offset to GOT
        R_X86_64_GOTPCREL = 9,
        /// Direct 32 bit zero extended
        R_X86_64_32 = 10,
        /// Direct 32 bit sign extended
        R_X86_64_32S = 11,
        /// Direct 16 bit zero extended
        R_X86_64_16 = 12,
        /// 16 bit sign extended pc relative
        R_X86_64_PC16 = 13,
        /// Direct 8 bit sign extended
        R_X86_64_8 = 14,
        /// 8 bit sign extended pc relative
        R_X86_64_PC8 = 15,
        /// ID of module containing symbol
        R_X86_64_DTPMOD64 = 16,
        /// Offset in module's TLS block
        R_X86_64_DTPOFF64 = 17,
        /// Offset in initial TLS block
        R_X86_64_TPOFF64 = 18,
        /// 32 bit signed PC relative offset to two GOT entries for GD symbol
        R_X86_64_TLSGD = 19,
        /// 32 bit signed PC relative offset to two GOT entries for LD symbol
        R_X86_64_TLSLD = 20,
        /// Offset in TLS block
        R_X86_64_DTPOFF32 = 21,
        /// 32 bit signed PC relative offset to GOT entry for IE symbol
        R_X86_64_GOTTPOFF = 22,
        /// Offset in initial TLS block
        R_X86_64_TPOFF32 = 23,
        /// PC relative 64 bit
        R_X86_64_PC64 = 24,
        /// 64 bit offset to GOT
        R_X86_64_GOTOFF64 = 25,
        /// 32 bit signed pc relative offset to GOT
        R_X86_64_GOTPC32 = 26,
        /// 64-bit GOT entry offset
        R_X86_64_GOT64 = 27,
        /// 64-bit PC relative offset to GOT entry
        R_X86_64_GOTPCREL64 = 28,
        /// 64-bit PC relative offset to GOT
        R_X86_64_GOTPC64 = 29,
        /// like GOT64, says PLT entry needed
        R_X86_64_GOTPLT64 = 30,
        /// 64-bit GOT relative offset to PLT entry
        R_X86_64_PLTOFF64 = 31,
        /// Size of symbol plus 32-bit addend
        R_X86_64_SIZE32 = 32,
        /// Size of symbol plus 64-bit addend
        R_X86_64_SIZE64 = 33,
        /// GOT offset for TLS descriptor.
        R_X86_64_GOTPC32_TLSDESC = 34,
        /// Marker for call through TLS descriptor.
        R_X86_64_TLSDESC_CALL = 35,
        /// TLS descriptor.
        R_X86_64_TLSDESC = 36,
        /// Adjust indirectly by program base
        R_X86_64_IRELATIVE = 37,
        /// 64-bit adjust by program base
        R_X86_64_RELATIVE64 = 38,
        // 39 Reserved was R_X86_64_PC32_BND
        // 40 Reserved was R_X86_64_PLT32_BND
        /// Load from 32 bit signed pc relative offset to GOT entry without REX prefix, relaxable.
        R_X86_64_GOTPCRELX = 41,
        /// Load from 32 bit signed pc relative offset to GOT entry with REX prefix, relaxable.
        R_X86_64_REX_GOTPCRELX = 42,
        /// 32 bit signed PC relative offset to GOT if the instruction starts at 4 bytes before the relocation offset, relaxable.
        R_X86_64_CODE_4_GOTPCRELX = 43,
        /// 32 bit signed PC relative offset to GOT entry for IE symbol if the instruction starts at 4 bytes before the relocation offset.
        R_X86_64_CODE_4_GOTTPOFF = 44,
        /// 32-bit PC relative to TLS descriptor in GOT if the instruction starts at 4 bytes before the relocation offset.
        R_X86_64_CODE_4_GOTPC32_TLSDESC = 45,
        /// 32 bit signed PC relative offset to GOT if the instruction starts at 5 bytes before the relocation offset, relaxable.
        R_X86_64_CODE_5_GOTPCRELX = 46,
        /// 32 bit signed PC relative offset to GOT entry for IE symbol if the instruction starts at 5 bytes before the relocation offset.
        R_X86_64_CODE_5_GOTTPOFF = 47,
        /// 32-bit PC relative to TLS descriptor in GOT if the instruction starts at 5 bytes before the relocation offset.
        R_X86_64_CODE_5_GOTPC32_TLSDESC = 48,
        /// 32 bit signed PC relative offset to GOT if the instruction starts at 6 bytes before the relocation offset, relaxable.
        R_X86_64_CODE_6_GOTPCRELX = 49,
        /// 32 bit signed PC relative offset to GOT entry for IE symbol if the instruction starts at 6 bytes before the relocation offset.
        R_X86_64_CODE_6_GOTTPOFF = 50,
        /// 32-bit PC relative to TLS descriptor in GOT if the instruction starts at 6 bytes before the relocation offset.
        R_X86_64_CODE_6_GOTPC32_TLSDESC = 51,
    };
    consts sht: SectionType(u32) = {
        /// Unwind information.
        SHT_X86_64_UNWIND = 0x7000_0001,
    };
}

names! {
    struct Mn10300(Base);
    /// `Rel*::r_type` values for `EM_MN10300`.
    consts r = pub NAMES_R_MN10300: RelocationType(u32) = {
        /// No reloc.
        R_MN10300_NONE = 0,
        /// Direct 32 bit.
        R_MN10300_32 = 1,
        /// Direct 16 bit.
        R_MN10300_16 = 2,
        /// Direct 8 bit.
        R_MN10300_8 = 3,
        /// PC-relative 32-bit.
        R_MN10300_PCREL32 = 4,
        /// PC-relative 16-bit signed.
        R_MN10300_PCREL16 = 5,
        /// PC-relative 8-bit signed.
        R_MN10300_PCREL8 = 6,
        /// Ancient C++ vtable garbage...
        R_MN10300_GNU_VTINHERIT = 7,
        /// ... collection annotation.
        R_MN10300_GNU_VTENTRY = 8,
        /// Direct 24 bit.
        R_MN10300_24 = 9,
        /// 32-bit PCrel offset to GOT.
        R_MN10300_GOTPC32 = 10,
        /// 16-bit PCrel offset to GOT.
        R_MN10300_GOTPC16 = 11,
        /// 32-bit offset from GOT.
        R_MN10300_GOTOFF32 = 12,
        /// 24-bit offset from GOT.
        R_MN10300_GOTOFF24 = 13,
        /// 16-bit offset from GOT.
        R_MN10300_GOTOFF16 = 14,
        /// 32-bit PCrel to PLT entry.
        R_MN10300_PLT32 = 15,
        /// 16-bit PCrel to PLT entry.
        R_MN10300_PLT16 = 16,
        /// 32-bit offset to GOT entry.
        R_MN10300_GOT32 = 17,
        /// 24-bit offset to GOT entry.
        R_MN10300_GOT24 = 18,
        /// 16-bit offset to GOT entry.
        R_MN10300_GOT16 = 19,
        /// Copy symbol at runtime.
        R_MN10300_COPY = 20,
        /// Create GOT entry.
        R_MN10300_GLOB_DAT = 21,
        /// Create PLT entry.
        R_MN10300_JMP_SLOT = 22,
        /// Adjust by program base.
        R_MN10300_RELATIVE = 23,
        /// 32-bit offset for global dynamic.
        R_MN10300_TLS_GD = 24,
        /// 32-bit offset for local dynamic.
        R_MN10300_TLS_LD = 25,
        /// Module-relative offset.
        R_MN10300_TLS_LDO = 26,
        /// GOT offset for static TLS block offset.
        R_MN10300_TLS_GOTIE = 27,
        /// GOT address for static TLS block offset.
        R_MN10300_TLS_IE = 28,
        /// Offset relative to static TLS block.
        R_MN10300_TLS_LE = 29,
        /// ID of module containing symbol.
        R_MN10300_TLS_DTPMOD = 30,
        /// Offset in module TLS block.
        R_MN10300_TLS_DTPOFF = 31,
        /// Offset in static TLS block.
        R_MN10300_TLS_TPOFF = 32,
        /// Adjustment for next reloc as needed by linker relaxation.
        R_MN10300_SYM_DIFF = 33,
        /// Alignment requirement for linker relaxation.
        R_MN10300_ALIGN = 34,
    };
}

names! {
    struct M32r(Base);
    /// `Rel*::r_type` values for `EM_M32R`.
    consts r = pub NAMES_R_M32R: RelocationType(u32) = {
        /// No reloc.
        R_M32R_NONE = 0,
        /// Direct 16 bit.
        R_M32R_16 = 1,
        /// Direct 32 bit.
        R_M32R_32 = 2,
        /// Direct 24 bit.
        R_M32R_24 = 3,
        /// PC relative 10 bit shifted.
        R_M32R_10_PCREL = 4,
        /// PC relative 18 bit shifted.
        R_M32R_18_PCREL = 5,
        /// PC relative 26 bit shifted.
        R_M32R_26_PCREL = 6,
        /// High 16 bit with unsigned low.
        R_M32R_HI16_ULO = 7,
        /// High 16 bit with signed low.
        R_M32R_HI16_SLO = 8,
        /// Low 16 bit.
        R_M32R_LO16 = 9,
        /// 16 bit offset in SDA.
        R_M32R_SDA16 = 10,
        R_M32R_GNU_VTINHERIT = 11,
        R_M32R_GNU_VTENTRY = 12,
        // M32R values `Rela32::r_type`.
        /// Direct 16 bit.
        R_M32R_16_RELA = 33,
        /// Direct 32 bit.
        R_M32R_32_RELA = 34,
        /// Direct 24 bit.
        R_M32R_24_RELA = 35,
        /// PC relative 10 bit shifted.
        R_M32R_10_PCREL_RELA = 36,
        /// PC relative 18 bit shifted.
        R_M32R_18_PCREL_RELA = 37,
        /// PC relative 26 bit shifted.
        R_M32R_26_PCREL_RELA = 38,
        /// High 16 bit with unsigned low
        R_M32R_HI16_ULO_RELA = 39,
        /// High 16 bit with signed low
        R_M32R_HI16_SLO_RELA = 40,
        /// Low 16 bit
        R_M32R_LO16_RELA = 41,
        /// 16 bit offset in SDA
        R_M32R_SDA16_RELA = 42,
        R_M32R_RELA_GNU_VTINHERIT = 43,
        R_M32R_RELA_GNU_VTENTRY = 44,
        /// PC relative 32 bit.
        R_M32R_REL32 = 45,

        /// 24 bit GOT entry
        R_M32R_GOT24 = 48,
        /// 26 bit PC relative to PLT shifted
        R_M32R_26_PLTREL = 49,
        /// Copy symbol at runtime
        R_M32R_COPY = 50,
        /// Create GOT entry
        R_M32R_GLOB_DAT = 51,
        /// Create PLT entry
        R_M32R_JMP_SLOT = 52,
        /// Adjust by program base
        R_M32R_RELATIVE = 53,
        /// 24 bit offset to GOT
        R_M32R_GOTOFF = 54,
        /// 24 bit PC relative offset to GOT
        R_M32R_GOTPC24 = 55,
        /// High 16 bit GOT entry with unsigned low
        R_M32R_GOT16_HI_ULO = 56,
        /// High 16 bit GOT entry with signed low
        R_M32R_GOT16_HI_SLO = 57,
        /// Low 16 bit GOT entry
        R_M32R_GOT16_LO = 58,
        /// High 16 bit PC relative offset to GOT with unsigned low
        R_M32R_GOTPC_HI_ULO = 59,
        /// High 16 bit PC relative offset to GOT with signed low
        R_M32R_GOTPC_HI_SLO = 60,
        /// Low 16 bit PC relative offset to GOT
        R_M32R_GOTPC_LO = 61,
        /// High 16 bit offset to GOT with unsigned low
        R_M32R_GOTOFF_HI_ULO = 62,
        /// High 16 bit offset to GOT with signed low
        R_M32R_GOTOFF_HI_SLO = 63,
        /// Low 16 bit offset to GOT
        R_M32R_GOTOFF_LO = 64,
        /// Keep this the last entry.
        R_M32R_NUM = 256,
    };
}

names! {
    struct Microblaze(Base);
    /// `Rel*::r_type` values for `EM_MICROBLAZE`.
    consts r = pub NAMES_R_MICROBLAZE: RelocationType(u32) = {
        /// No reloc.
        R_MICROBLAZE_NONE = 0,
        /// Direct 32 bit.
        R_MICROBLAZE_32 = 1,
        /// PC relative 32 bit.
        R_MICROBLAZE_32_PCREL = 2,
        /// PC relative 64 bit.
        R_MICROBLAZE_64_PCREL = 3,
        /// Low 16 bits of PCREL32.
        R_MICROBLAZE_32_PCREL_LO = 4,
        /// Direct 64 bit.
        R_MICROBLAZE_64 = 5,
        /// Low 16 bit.
        R_MICROBLAZE_32_LO = 6,
        /// Read-only small data area.
        R_MICROBLAZE_SRO32 = 7,
        /// Read-write small data area.
        R_MICROBLAZE_SRW32 = 8,
        /// No reloc.
        R_MICROBLAZE_64_NONE = 9,
        /// Symbol Op Symbol relocation.
        R_MICROBLAZE_32_SYM_OP_SYM = 10,
        /// GNU C++ vtable hierarchy.
        R_MICROBLAZE_GNU_VTINHERIT = 11,
        /// GNU C++ vtable member usage.
        R_MICROBLAZE_GNU_VTENTRY = 12,
        /// PC-relative GOT offset.
        R_MICROBLAZE_GOTPC_64 = 13,
        /// GOT entry offset.
        R_MICROBLAZE_GOT_64 = 14,
        /// PLT offset (PC-relative).
        R_MICROBLAZE_PLT_64 = 15,
        /// Adjust by program base.
        R_MICROBLAZE_REL = 16,
        /// Create PLT entry.
        R_MICROBLAZE_JUMP_SLOT = 17,
        /// Create GOT entry.
        R_MICROBLAZE_GLOB_DAT = 18,
        /// 64 bit offset to GOT.
        R_MICROBLAZE_GOTOFF_64 = 19,
        /// 32 bit offset to GOT.
        R_MICROBLAZE_GOTOFF_32 = 20,
        /// Runtime copy.
        R_MICROBLAZE_COPY = 21,
        /// TLS Reloc.
        R_MICROBLAZE_TLS = 22,
        /// TLS General Dynamic.
        R_MICROBLAZE_TLSGD = 23,
        /// TLS Local Dynamic.
        R_MICROBLAZE_TLSLD = 24,
        /// TLS Module ID.
        R_MICROBLAZE_TLSDTPMOD32 = 25,
        /// TLS Offset Within TLS Block.
        R_MICROBLAZE_TLSDTPREL32 = 26,
        /// TLS Offset Within TLS Block.
        R_MICROBLAZE_TLSDTPREL64 = 27,
        /// TLS Offset From Thread Pointer.
        R_MICROBLAZE_TLSGOTTPREL32 = 28,
        /// TLS Offset From Thread Pointer.
        R_MICROBLAZE_TLSTPREL32 = 29,
    };
}

// Nios II
names! {
    struct Nios2(Base);
    consts dt: DynamicTag(i64) = {
        /// Address of _gp.
        DT_NIOS2_GP = 0x7000_0002,
    };
    /// `Rel*::r_type` values for `EM_ALTERA_NIOS2`.
    consts r = pub NAMES_R_NIOS2: RelocationType(u32) = {
        /// No reloc.
        R_NIOS2_NONE = 0,
        /// Direct signed 16 bit.
        R_NIOS2_S16 = 1,
        /// Direct unsigned 16 bit.
        R_NIOS2_U16 = 2,
        /// PC relative 16 bit.
        R_NIOS2_PCREL16 = 3,
        /// Direct call.
        R_NIOS2_CALL26 = 4,
        /// 5 bit constant expression.
        R_NIOS2_IMM5 = 5,
        /// 5 bit expression, shift 22.
        R_NIOS2_CACHE_OPX = 6,
        /// 6 bit constant expression.
        R_NIOS2_IMM6 = 7,
        /// 8 bit constant expression.
        R_NIOS2_IMM8 = 8,
        /// High 16 bit.
        R_NIOS2_HI16 = 9,
        /// Low 16 bit.
        R_NIOS2_LO16 = 10,
        /// High 16 bit, adjusted.
        R_NIOS2_HIADJ16 = 11,
        /// 32 bit symbol value + addend.
        R_NIOS2_BFD_RELOC_32 = 12,
        /// 16 bit symbol value + addend.
        R_NIOS2_BFD_RELOC_16 = 13,
        /// 8 bit symbol value + addend.
        R_NIOS2_BFD_RELOC_8 = 14,
        /// 16 bit GP pointer offset.
        R_NIOS2_GPREL = 15,
        /// GNU C++ vtable hierarchy.
        R_NIOS2_GNU_VTINHERIT = 16,
        /// GNU C++ vtable member usage.
        R_NIOS2_GNU_VTENTRY = 17,
        /// Unconditional branch.
        R_NIOS2_UJMP = 18,
        /// Conditional branch.
        R_NIOS2_CJMP = 19,
        /// Indirect call through register.
        R_NIOS2_CALLR = 20,
        /// Alignment requirement for linker relaxation.
        R_NIOS2_ALIGN = 21,
        /// 16 bit GOT entry.
        R_NIOS2_GOT16 = 22,
        /// 16 bit GOT entry for function.
        R_NIOS2_CALL16 = 23,
        /// %lo of offset to GOT pointer.
        R_NIOS2_GOTOFF_LO = 24,
        /// %hiadj of offset to GOT pointer.
        R_NIOS2_GOTOFF_HA = 25,
        /// %lo of PC relative offset.
        R_NIOS2_PCREL_LO = 26,
        /// %hiadj of PC relative offset.
        R_NIOS2_PCREL_HA = 27,
        /// 16 bit GOT offset for TLS GD.
        R_NIOS2_TLS_GD16 = 28,
        /// 16 bit GOT offset for TLS LDM.
        R_NIOS2_TLS_LDM16 = 29,
        /// 16 bit module relative offset.
        R_NIOS2_TLS_LDO16 = 30,
        /// 16 bit GOT offset for TLS IE.
        R_NIOS2_TLS_IE16 = 31,
        /// 16 bit LE TP-relative offset.
        R_NIOS2_TLS_LE16 = 32,
        /// Module number.
        R_NIOS2_TLS_DTPMOD = 33,
        /// Module-relative offset.
        R_NIOS2_TLS_DTPREL = 34,
        /// TP-relative offset.
        R_NIOS2_TLS_TPREL = 35,
        /// Copy symbol at runtime.
        R_NIOS2_COPY = 36,
        /// Create GOT entry.
        R_NIOS2_GLOB_DAT = 37,
        /// Create PLT entry.
        R_NIOS2_JUMP_SLOT = 38,
        /// Adjust by program base.
        R_NIOS2_RELATIVE = 39,
        /// 16 bit offset to GOT pointer.
        R_NIOS2_GOTOFF = 40,
        /// Direct call in .noat section.
        R_NIOS2_CALL26_NOAT = 41,
        /// %lo() of GOT entry.
        R_NIOS2_GOT_LO = 42,
        /// %hiadj() of GOT entry.
        R_NIOS2_GOT_HA = 43,
        /// %lo() of function GOT entry.
        R_NIOS2_CALL_LO = 44,
        /// %hiadj() of function GOT entry.
        R_NIOS2_CALL_HA = 45,
    };
}

// TILEPro
names! {
    struct Tilepro(Base);
    /// `Rel*::r_type` values for `EM_TILEPRO`.
    consts r = pub NAMES_R_TILEPRO: RelocationType(u32) = {
        /// No reloc
        R_TILEPRO_NONE = 0,
        /// Direct 32 bit
        R_TILEPRO_32 = 1,
        /// Direct 16 bit
        R_TILEPRO_16 = 2,
        /// Direct 8 bit
        R_TILEPRO_8 = 3,
        /// PC relative 32 bit
        R_TILEPRO_32_PCREL = 4,
        /// PC relative 16 bit
        R_TILEPRO_16_PCREL = 5,
        /// PC relative 8 bit
        R_TILEPRO_8_PCREL = 6,
        /// Low 16 bit
        R_TILEPRO_LO16 = 7,
        /// High 16 bit
        R_TILEPRO_HI16 = 8,
        /// High 16 bit, adjusted
        R_TILEPRO_HA16 = 9,
        /// Copy relocation
        R_TILEPRO_COPY = 10,
        /// Create GOT entry
        R_TILEPRO_GLOB_DAT = 11,
        /// Create PLT entry
        R_TILEPRO_JMP_SLOT = 12,
        /// Adjust by program base
        R_TILEPRO_RELATIVE = 13,
        /// X1 pipe branch offset
        R_TILEPRO_BROFF_X1 = 14,
        /// X1 pipe jump offset
        R_TILEPRO_JOFFLONG_X1 = 15,
        /// X1 pipe jump offset to PLT
        R_TILEPRO_JOFFLONG_X1_PLT = 16,
        /// X0 pipe 8-bit
        R_TILEPRO_IMM8_X0 = 17,
        /// Y0 pipe 8-bit
        R_TILEPRO_IMM8_Y0 = 18,
        /// X1 pipe 8-bit
        R_TILEPRO_IMM8_X1 = 19,
        /// Y1 pipe 8-bit
        R_TILEPRO_IMM8_Y1 = 20,
        /// X1 pipe mtspr
        R_TILEPRO_MT_IMM15_X1 = 21,
        /// X1 pipe mfspr
        R_TILEPRO_MF_IMM15_X1 = 22,
        /// X0 pipe 16-bit
        R_TILEPRO_IMM16_X0 = 23,
        /// X1 pipe 16-bit
        R_TILEPRO_IMM16_X1 = 24,
        /// X0 pipe low 16-bit
        R_TILEPRO_IMM16_X0_LO = 25,
        /// X1 pipe low 16-bit
        R_TILEPRO_IMM16_X1_LO = 26,
        /// X0 pipe high 16-bit
        R_TILEPRO_IMM16_X0_HI = 27,
        /// X1 pipe high 16-bit
        R_TILEPRO_IMM16_X1_HI = 28,
        /// X0 pipe high 16-bit, adjusted
        R_TILEPRO_IMM16_X0_HA = 29,
        /// X1 pipe high 16-bit, adjusted
        R_TILEPRO_IMM16_X1_HA = 30,
        /// X0 pipe PC relative 16 bit
        R_TILEPRO_IMM16_X0_PCREL = 31,
        /// X1 pipe PC relative 16 bit
        R_TILEPRO_IMM16_X1_PCREL = 32,
        /// X0 pipe PC relative low 16 bit
        R_TILEPRO_IMM16_X0_LO_PCREL = 33,
        /// X1 pipe PC relative low 16 bit
        R_TILEPRO_IMM16_X1_LO_PCREL = 34,
        /// X0 pipe PC relative high 16 bit
        R_TILEPRO_IMM16_X0_HI_PCREL = 35,
        /// X1 pipe PC relative high 16 bit
        R_TILEPRO_IMM16_X1_HI_PCREL = 36,
        /// X0 pipe PC relative ha() 16 bit
        R_TILEPRO_IMM16_X0_HA_PCREL = 37,
        /// X1 pipe PC relative ha() 16 bit
        R_TILEPRO_IMM16_X1_HA_PCREL = 38,
        /// X0 pipe 16-bit GOT offset
        R_TILEPRO_IMM16_X0_GOT = 39,
        /// X1 pipe 16-bit GOT offset
        R_TILEPRO_IMM16_X1_GOT = 40,
        /// X0 pipe low 16-bit GOT offset
        R_TILEPRO_IMM16_X0_GOT_LO = 41,
        /// X1 pipe low 16-bit GOT offset
        R_TILEPRO_IMM16_X1_GOT_LO = 42,
        /// X0 pipe high 16-bit GOT offset
        R_TILEPRO_IMM16_X0_GOT_HI = 43,
        /// X1 pipe high 16-bit GOT offset
        R_TILEPRO_IMM16_X1_GOT_HI = 44,
        /// X0 pipe ha() 16-bit GOT offset
        R_TILEPRO_IMM16_X0_GOT_HA = 45,
        /// X1 pipe ha() 16-bit GOT offset
        R_TILEPRO_IMM16_X1_GOT_HA = 46,
        /// X0 pipe mm "start"
        R_TILEPRO_MMSTART_X0 = 47,
        /// X0 pipe mm "end"
        R_TILEPRO_MMEND_X0 = 48,
        /// X1 pipe mm "start"
        R_TILEPRO_MMSTART_X1 = 49,
        /// X1 pipe mm "end"
        R_TILEPRO_MMEND_X1 = 50,
        /// X0 pipe shift amount
        R_TILEPRO_SHAMT_X0 = 51,
        /// X1 pipe shift amount
        R_TILEPRO_SHAMT_X1 = 52,
        /// Y0 pipe shift amount
        R_TILEPRO_SHAMT_Y0 = 53,
        /// Y1 pipe shift amount
        R_TILEPRO_SHAMT_Y1 = 54,
        /// X1 pipe destination 8-bit
        R_TILEPRO_DEST_IMM8_X1 = 55,
        // Relocs 56-59 are currently not defined.
        /// "jal" for TLS GD
        R_TILEPRO_TLS_GD_CALL = 60,
        /// X0 pipe "addi" for TLS GD
        R_TILEPRO_IMM8_X0_TLS_GD_ADD = 61,
        /// X1 pipe "addi" for TLS GD
        R_TILEPRO_IMM8_X1_TLS_GD_ADD = 62,
        /// Y0 pipe "addi" for TLS GD
        R_TILEPRO_IMM8_Y0_TLS_GD_ADD = 63,
        /// Y1 pipe "addi" for TLS GD
        R_TILEPRO_IMM8_Y1_TLS_GD_ADD = 64,
        /// "lw_tls" for TLS IE
        R_TILEPRO_TLS_IE_LOAD = 65,
        /// X0 pipe 16-bit TLS GD offset
        R_TILEPRO_IMM16_X0_TLS_GD = 66,
        /// X1 pipe 16-bit TLS GD offset
        R_TILEPRO_IMM16_X1_TLS_GD = 67,
        /// X0 pipe low 16-bit TLS GD offset
        R_TILEPRO_IMM16_X0_TLS_GD_LO = 68,
        /// X1 pipe low 16-bit TLS GD offset
        R_TILEPRO_IMM16_X1_TLS_GD_LO = 69,
        /// X0 pipe high 16-bit TLS GD offset
        R_TILEPRO_IMM16_X0_TLS_GD_HI = 70,
        /// X1 pipe high 16-bit TLS GD offset
        R_TILEPRO_IMM16_X1_TLS_GD_HI = 71,
        /// X0 pipe ha() 16-bit TLS GD offset
        R_TILEPRO_IMM16_X0_TLS_GD_HA = 72,
        /// X1 pipe ha() 16-bit TLS GD offset
        R_TILEPRO_IMM16_X1_TLS_GD_HA = 73,
        /// X0 pipe 16-bit TLS IE offset
        R_TILEPRO_IMM16_X0_TLS_IE = 74,
        /// X1 pipe 16-bit TLS IE offset
        R_TILEPRO_IMM16_X1_TLS_IE = 75,
        /// X0 pipe low 16-bit TLS IE offset
        R_TILEPRO_IMM16_X0_TLS_IE_LO = 76,
        /// X1 pipe low 16-bit TLS IE offset
        R_TILEPRO_IMM16_X1_TLS_IE_LO = 77,
        /// X0 pipe high 16-bit TLS IE offset
        R_TILEPRO_IMM16_X0_TLS_IE_HI = 78,
        /// X1 pipe high 16-bit TLS IE offset
        R_TILEPRO_IMM16_X1_TLS_IE_HI = 79,
        /// X0 pipe ha() 16-bit TLS IE offset
        R_TILEPRO_IMM16_X0_TLS_IE_HA = 80,
        /// X1 pipe ha() 16-bit TLS IE offset
        R_TILEPRO_IMM16_X1_TLS_IE_HA = 81,
        /// ID of module containing symbol
        R_TILEPRO_TLS_DTPMOD32 = 82,
        /// Offset in TLS block
        R_TILEPRO_TLS_DTPOFF32 = 83,
        /// Offset in static TLS block
        R_TILEPRO_TLS_TPOFF32 = 84,
        /// X0 pipe 16-bit TLS LE offset
        R_TILEPRO_IMM16_X0_TLS_LE = 85,
        /// X1 pipe 16-bit TLS LE offset
        R_TILEPRO_IMM16_X1_TLS_LE = 86,
        /// X0 pipe low 16-bit TLS LE offset
        R_TILEPRO_IMM16_X0_TLS_LE_LO = 87,
        /// X1 pipe low 16-bit TLS LE offset
        R_TILEPRO_IMM16_X1_TLS_LE_LO = 88,
        /// X0 pipe high 16-bit TLS LE offset
        R_TILEPRO_IMM16_X0_TLS_LE_HI = 89,
        /// X1 pipe high 16-bit TLS LE offset
        R_TILEPRO_IMM16_X1_TLS_LE_HI = 90,
        /// X0 pipe ha() 16-bit TLS LE offset
        R_TILEPRO_IMM16_X0_TLS_LE_HA = 91,
        /// X1 pipe ha() 16-bit TLS LE offset
        R_TILEPRO_IMM16_X1_TLS_LE_HA = 92,

        /// GNU C++ vtable hierarchy
        R_TILEPRO_GNU_VTINHERIT = 128,
        /// GNU C++ vtable member usage
        R_TILEPRO_GNU_VTENTRY = 129,
    };
}

// TILE-Gx
names! {
    struct Tilegx(Base);
    /// `Rel*::r_type` values for `EM_TILEGX`.
    consts r = pub NAMES_R_TILEGX: RelocationType(u32) = {
        /// No reloc
        R_TILEGX_NONE = 0,
        /// Direct 64 bit
        R_TILEGX_64 = 1,
        /// Direct 32 bit
        R_TILEGX_32 = 2,
        /// Direct 16 bit
        R_TILEGX_16 = 3,
        /// Direct 8 bit
        R_TILEGX_8 = 4,
        /// PC relative 64 bit
        R_TILEGX_64_PCREL = 5,
        /// PC relative 32 bit
        R_TILEGX_32_PCREL = 6,
        /// PC relative 16 bit
        R_TILEGX_16_PCREL = 7,
        /// PC relative 8 bit
        R_TILEGX_8_PCREL = 8,
        /// hword 0 16-bit
        R_TILEGX_HW0 = 9,
        /// hword 1 16-bit
        R_TILEGX_HW1 = 10,
        /// hword 2 16-bit
        R_TILEGX_HW2 = 11,
        /// hword 3 16-bit
        R_TILEGX_HW3 = 12,
        /// last hword 0 16-bit
        R_TILEGX_HW0_LAST = 13,
        /// last hword 1 16-bit
        R_TILEGX_HW1_LAST = 14,
        /// last hword 2 16-bit
        R_TILEGX_HW2_LAST = 15,
        /// Copy relocation
        R_TILEGX_COPY = 16,
        /// Create GOT entry
        R_TILEGX_GLOB_DAT = 17,
        /// Create PLT entry
        R_TILEGX_JMP_SLOT = 18,
        /// Adjust by program base
        R_TILEGX_RELATIVE = 19,
        /// X1 pipe branch offset
        R_TILEGX_BROFF_X1 = 20,
        /// X1 pipe jump offset
        R_TILEGX_JUMPOFF_X1 = 21,
        /// X1 pipe jump offset to PLT
        R_TILEGX_JUMPOFF_X1_PLT = 22,
        /// X0 pipe 8-bit
        R_TILEGX_IMM8_X0 = 23,
        /// Y0 pipe 8-bit
        R_TILEGX_IMM8_Y0 = 24,
        /// X1 pipe 8-bit
        R_TILEGX_IMM8_X1 = 25,
        /// Y1 pipe 8-bit
        R_TILEGX_IMM8_Y1 = 26,
        /// X1 pipe destination 8-bit
        R_TILEGX_DEST_IMM8_X1 = 27,
        /// X1 pipe mtspr
        R_TILEGX_MT_IMM14_X1 = 28,
        /// X1 pipe mfspr
        R_TILEGX_MF_IMM14_X1 = 29,
        /// X0 pipe mm "start"
        R_TILEGX_MMSTART_X0 = 30,
        /// X0 pipe mm "end"
        R_TILEGX_MMEND_X0 = 31,
        /// X0 pipe shift amount
        R_TILEGX_SHAMT_X0 = 32,
        /// X1 pipe shift amount
        R_TILEGX_SHAMT_X1 = 33,
        /// Y0 pipe shift amount
        R_TILEGX_SHAMT_Y0 = 34,
        /// Y1 pipe shift amount
        R_TILEGX_SHAMT_Y1 = 35,
        /// X0 pipe hword 0
        R_TILEGX_IMM16_X0_HW0 = 36,
        /// X1 pipe hword 0
        R_TILEGX_IMM16_X1_HW0 = 37,
        /// X0 pipe hword 1
        R_TILEGX_IMM16_X0_HW1 = 38,
        /// X1 pipe hword 1
        R_TILEGX_IMM16_X1_HW1 = 39,
        /// X0 pipe hword 2
        R_TILEGX_IMM16_X0_HW2 = 40,
        /// X1 pipe hword 2
        R_TILEGX_IMM16_X1_HW2 = 41,
        /// X0 pipe hword 3
        R_TILEGX_IMM16_X0_HW3 = 42,
        /// X1 pipe hword 3
        R_TILEGX_IMM16_X1_HW3 = 43,
        /// X0 pipe last hword 0
        R_TILEGX_IMM16_X0_HW0_LAST = 44,
        /// X1 pipe last hword 0
        R_TILEGX_IMM16_X1_HW0_LAST = 45,
        /// X0 pipe last hword 1
        R_TILEGX_IMM16_X0_HW1_LAST = 46,
        /// X1 pipe last hword 1
        R_TILEGX_IMM16_X1_HW1_LAST = 47,
        /// X0 pipe last hword 2
        R_TILEGX_IMM16_X0_HW2_LAST = 48,
        /// X1 pipe last hword 2
        R_TILEGX_IMM16_X1_HW2_LAST = 49,
        /// X0 pipe PC relative hword 0
        R_TILEGX_IMM16_X0_HW0_PCREL = 50,
        /// X1 pipe PC relative hword 0
        R_TILEGX_IMM16_X1_HW0_PCREL = 51,
        /// X0 pipe PC relative hword 1
        R_TILEGX_IMM16_X0_HW1_PCREL = 52,
        /// X1 pipe PC relative hword 1
        R_TILEGX_IMM16_X1_HW1_PCREL = 53,
        /// X0 pipe PC relative hword 2
        R_TILEGX_IMM16_X0_HW2_PCREL = 54,
        /// X1 pipe PC relative hword 2
        R_TILEGX_IMM16_X1_HW2_PCREL = 55,
        /// X0 pipe PC relative hword 3
        R_TILEGX_IMM16_X0_HW3_PCREL = 56,
        /// X1 pipe PC relative hword 3
        R_TILEGX_IMM16_X1_HW3_PCREL = 57,
        /// X0 pipe PC-rel last hword 0
        R_TILEGX_IMM16_X0_HW0_LAST_PCREL = 58,
        /// X1 pipe PC-rel last hword 0
        R_TILEGX_IMM16_X1_HW0_LAST_PCREL = 59,
        /// X0 pipe PC-rel last hword 1
        R_TILEGX_IMM16_X0_HW1_LAST_PCREL = 60,
        /// X1 pipe PC-rel last hword 1
        R_TILEGX_IMM16_X1_HW1_LAST_PCREL = 61,
        /// X0 pipe PC-rel last hword 2
        R_TILEGX_IMM16_X0_HW2_LAST_PCREL = 62,
        /// X1 pipe PC-rel last hword 2
        R_TILEGX_IMM16_X1_HW2_LAST_PCREL = 63,
        /// X0 pipe hword 0 GOT offset
        R_TILEGX_IMM16_X0_HW0_GOT = 64,
        /// X1 pipe hword 0 GOT offset
        R_TILEGX_IMM16_X1_HW0_GOT = 65,
        /// X0 pipe PC-rel PLT hword 0
        R_TILEGX_IMM16_X0_HW0_PLT_PCREL = 66,
        /// X1 pipe PC-rel PLT hword 0
        R_TILEGX_IMM16_X1_HW0_PLT_PCREL = 67,
        /// X0 pipe PC-rel PLT hword 1
        R_TILEGX_IMM16_X0_HW1_PLT_PCREL = 68,
        /// X1 pipe PC-rel PLT hword 1
        R_TILEGX_IMM16_X1_HW1_PLT_PCREL = 69,
        /// X0 pipe PC-rel PLT hword 2
        R_TILEGX_IMM16_X0_HW2_PLT_PCREL = 70,
        /// X1 pipe PC-rel PLT hword 2
        R_TILEGX_IMM16_X1_HW2_PLT_PCREL = 71,
        /// X0 pipe last hword 0 GOT offset
        R_TILEGX_IMM16_X0_HW0_LAST_GOT = 72,
        /// X1 pipe last hword 0 GOT offset
        R_TILEGX_IMM16_X1_HW0_LAST_GOT = 73,
        /// X0 pipe last hword 1 GOT offset
        R_TILEGX_IMM16_X0_HW1_LAST_GOT = 74,
        /// X1 pipe last hword 1 GOT offset
        R_TILEGX_IMM16_X1_HW1_LAST_GOT = 75,
        /// X0 pipe PC-rel PLT hword 3
        R_TILEGX_IMM16_X0_HW3_PLT_PCREL = 76,
        /// X1 pipe PC-rel PLT hword 3
        R_TILEGX_IMM16_X1_HW3_PLT_PCREL = 77,
        /// X0 pipe hword 0 TLS GD offset
        R_TILEGX_IMM16_X0_HW0_TLS_GD = 78,
        /// X1 pipe hword 0 TLS GD offset
        R_TILEGX_IMM16_X1_HW0_TLS_GD = 79,
        /// X0 pipe hword 0 TLS LE offset
        R_TILEGX_IMM16_X0_HW0_TLS_LE = 80,
        /// X1 pipe hword 0 TLS LE offset
        R_TILEGX_IMM16_X1_HW0_TLS_LE = 81,
        /// X0 pipe last hword 0 LE off
        R_TILEGX_IMM16_X0_HW0_LAST_TLS_LE = 82,
        /// X1 pipe last hword 0 LE off
        R_TILEGX_IMM16_X1_HW0_LAST_TLS_LE = 83,
        /// X0 pipe last hword 1 LE off
        R_TILEGX_IMM16_X0_HW1_LAST_TLS_LE = 84,
        /// X1 pipe last hword 1 LE off
        R_TILEGX_IMM16_X1_HW1_LAST_TLS_LE = 85,
        /// X0 pipe last hword 0 GD off
        R_TILEGX_IMM16_X0_HW0_LAST_TLS_GD = 86,
        /// X1 pipe last hword 0 GD off
        R_TILEGX_IMM16_X1_HW0_LAST_TLS_GD = 87,
        /// X0 pipe last hword 1 GD off
        R_TILEGX_IMM16_X0_HW1_LAST_TLS_GD = 88,
        /// X1 pipe last hword 1 GD off
        R_TILEGX_IMM16_X1_HW1_LAST_TLS_GD = 89,
        // Relocs 90-91 are currently not defined.
        /// X0 pipe hword 0 TLS IE offset
        R_TILEGX_IMM16_X0_HW0_TLS_IE = 92,
        /// X1 pipe hword 0 TLS IE offset
        R_TILEGX_IMM16_X1_HW0_TLS_IE = 93,
        /// X0 pipe PC-rel PLT last hword 0
        R_TILEGX_IMM16_X0_HW0_LAST_PLT_PCREL = 94,
        /// X1 pipe PC-rel PLT last hword 0
        R_TILEGX_IMM16_X1_HW0_LAST_PLT_PCREL = 95,
        /// X0 pipe PC-rel PLT last hword 1
        R_TILEGX_IMM16_X0_HW1_LAST_PLT_PCREL = 96,
        /// X1 pipe PC-rel PLT last hword 1
        R_TILEGX_IMM16_X1_HW1_LAST_PLT_PCREL = 97,
        /// X0 pipe PC-rel PLT last hword 2
        R_TILEGX_IMM16_X0_HW2_LAST_PLT_PCREL = 98,
        /// X1 pipe PC-rel PLT last hword 2
        R_TILEGX_IMM16_X1_HW2_LAST_PLT_PCREL = 99,
        /// X0 pipe last hword 0 IE off
        R_TILEGX_IMM16_X0_HW0_LAST_TLS_IE = 100,
        /// X1 pipe last hword 0 IE off
        R_TILEGX_IMM16_X1_HW0_LAST_TLS_IE = 101,
        /// X0 pipe last hword 1 IE off
        R_TILEGX_IMM16_X0_HW1_LAST_TLS_IE = 102,
        /// X1 pipe last hword 1 IE off
        R_TILEGX_IMM16_X1_HW1_LAST_TLS_IE = 103,
        // Relocs 104-105 are currently not defined.
        /// 64-bit ID of symbol's module
        R_TILEGX_TLS_DTPMOD64 = 106,
        /// 64-bit offset in TLS block
        R_TILEGX_TLS_DTPOFF64 = 107,
        /// 64-bit offset in static TLS block
        R_TILEGX_TLS_TPOFF64 = 108,
        /// 32-bit ID of symbol's module
        R_TILEGX_TLS_DTPMOD32 = 109,
        /// 32-bit offset in TLS block
        R_TILEGX_TLS_DTPOFF32 = 110,
        /// 32-bit offset in static TLS block
        R_TILEGX_TLS_TPOFF32 = 111,
        /// "jal" for TLS GD
        R_TILEGX_TLS_GD_CALL = 112,
        /// X0 pipe "addi" for TLS GD
        R_TILEGX_IMM8_X0_TLS_GD_ADD = 113,
        /// X1 pipe "addi" for TLS GD
        R_TILEGX_IMM8_X1_TLS_GD_ADD = 114,
        /// Y0 pipe "addi" for TLS GD
        R_TILEGX_IMM8_Y0_TLS_GD_ADD = 115,
        /// Y1 pipe "addi" for TLS GD
        R_TILEGX_IMM8_Y1_TLS_GD_ADD = 116,
        /// "ld_tls" for TLS IE
        R_TILEGX_TLS_IE_LOAD = 117,
        /// X0 pipe "addi" for TLS GD/IE
        R_TILEGX_IMM8_X0_TLS_ADD = 118,
        /// X1 pipe "addi" for TLS GD/IE
        R_TILEGX_IMM8_X1_TLS_ADD = 119,
        /// Y0 pipe "addi" for TLS GD/IE
        R_TILEGX_IMM8_Y0_TLS_ADD = 120,
        /// Y1 pipe "addi" for TLS GD/IE
        R_TILEGX_IMM8_Y1_TLS_ADD = 121,

        /// GNU C++ vtable hierarchy
        R_TILEGX_GNU_VTINHERIT = 128,
        /// GNU C++ vtable member usage
        R_TILEGX_GNU_VTENTRY = 129,
    };
}

names! {
    struct Riscv(Base);
    flags ef: FileFlags(u32) = {
        EF_RISCV_RVC = 0x0001,
        EF_RISCV_FLOAT_ABI = 0x0006 => NAMES_EF_RISCV_FLOAT_ABI,
        EF_RISCV_RVE = 0x0008,
        EF_RISCV_TSO = 0x0010,
        EF_RISCV_RV64ILP32 = 0x0020,
    };
    flags sto: SymbolOther(u8) = {
        /// Function uses variant calling convention.
        STO_RISCV_VARIANT_CC = 0x80,
    };
    consts sht: SectionType(u32) = {
        /// RISC-V attributes section.
        SHT_RISCV_ATTRIBUTES = SHT_LOPROC + 3,
    };
    consts pt: ProgramType(u32) = {
        PT_RISCV_ATTRIBUTES = PT_LOPROC + 3,
    };
    consts dt: DynamicTag(i64) = {
        DT_RISCV_VARIANT_CC = DT_LOPROC + 1,
    };
    /// `Rel*::r_type` values for `EM_RISCV`.
    consts r = pub NAMES_R_RISCV: RelocationType(u32) = {
        R_RISCV_NONE = 0,
        R_RISCV_32 = 1,
        R_RISCV_64 = 2,
        R_RISCV_RELATIVE = 3,
        R_RISCV_COPY = 4,
        R_RISCV_JUMP_SLOT = 5,
        R_RISCV_TLS_DTPMOD32 = 6,
        R_RISCV_TLS_DTPMOD64 = 7,
        R_RISCV_TLS_DTPREL32 = 8,
        R_RISCV_TLS_DTPREL64 = 9,
        R_RISCV_TLS_TPREL32 = 10,
        R_RISCV_TLS_TPREL64 = 11,
        R_RISCV_TLSDESC = 12,
        R_RISCV_BRANCH = 16,
        R_RISCV_JAL = 17,
        R_RISCV_CALL = 18,
        R_RISCV_CALL_PLT = 19,
        R_RISCV_GOT_HI20 = 20,
        R_RISCV_TLS_GOT_HI20 = 21,
        R_RISCV_TLS_GD_HI20 = 22,
        R_RISCV_PCREL_HI20 = 23,
        R_RISCV_PCREL_LO12_I = 24,
        R_RISCV_PCREL_LO12_S = 25,
        R_RISCV_HI20 = 26,
        R_RISCV_LO12_I = 27,
        R_RISCV_LO12_S = 28,
        R_RISCV_TPREL_HI20 = 29,
        R_RISCV_TPREL_LO12_I = 30,
        R_RISCV_TPREL_LO12_S = 31,
        R_RISCV_TPREL_ADD = 32,
        R_RISCV_ADD8 = 33,
        R_RISCV_ADD16 = 34,
        R_RISCV_ADD32 = 35,
        R_RISCV_ADD64 = 36,
        R_RISCV_SUB8 = 37,
        R_RISCV_SUB16 = 38,
        R_RISCV_SUB32 = 39,
        R_RISCV_SUB64 = 40,
        R_RISCV_GOT32_PCREL = 41,
        // 42 Reserved was R_RISCV_GNU_VTENTRY
        R_RISCV_ALIGN = 43,
        R_RISCV_RVC_BRANCH = 44,
        R_RISCV_RVC_JUMP = 45,
        R_RISCV_RVC_LUI = 46,
        R_RISCV_GPREL_I = 47,
        R_RISCV_GPREL_S = 48,
        R_RISCV_TPREL_I = 49,
        R_RISCV_TPREL_S = 50,
        R_RISCV_RELAX = 51,
        R_RISCV_SUB6 = 52,
        R_RISCV_SET6 = 53,
        R_RISCV_SET8 = 54,
        R_RISCV_SET16 = 55,
        R_RISCV_SET32 = 56,
        R_RISCV_32_PCREL = 57,
        R_RISCV_IRELATIVE = 58,
        R_RISCV_PLT32 = 59,
        R_RISCV_SET_ULEB128 = 60,
        R_RISCV_SUB_ULEB128 = 61,
        R_RISCV_TLSDESC_HI20 = 62,
        R_RISCV_TLSDESC_LOAD_LO12 = 63,
        R_RISCV_TLSDESC_ADD_LO12 = 64,
        R_RISCV_TLSDESC_CALL = 65,
    };
}

constant_names!(NAMES_EF_RISCV_FLOAT_ABI: FileFlags(u32) = {
    EF_RISCV_FLOAT_ABI_SOFT = 0x0000,
    EF_RISCV_FLOAT_ABI_SINGLE = 0x0002,
    EF_RISCV_FLOAT_ABI_DOUBLE = 0x0004,
    EF_RISCV_FLOAT_ABI_QUAD = 0x0006,
});

names! {
    struct Bpf(Base);
    /// `Rel*::r_type` values for `EM_BPF`.
    consts r = pub NAMES_R_BPF: RelocationType(u32) = {
        /// No reloc
        R_BPF_NONE = 0,
        R_BPF_64_64 = 1,
        R_BPF_64_32 = 10,
    };
}

names! {
    struct Sbf(Base);
    /// `Rel*::r_type` values for `EM_SBF`.
    consts r = pub NAMES_R_SBF: RelocationType(u32) = {
        /// No reloc
        R_SBF_NONE = 0,
        R_SBF_64_64 = 1,
        R_SBF_64_32 = 10,
    };
}

// Imagination Meta

names! {
    struct Metag(Base);
    /// `Rel*::r_type` values for `EM_METAG`.
    consts r = pub NAMES_R_METAG: RelocationType(u32) = {
        R_METAG_HIADDR16 = 0,
        R_METAG_LOADDR16 = 1,
        /// 32bit absolute address
        R_METAG_ADDR32 = 2,
        /// No reloc
        R_METAG_NONE = 3,
        R_METAG_RELBRANCH = 4,
        R_METAG_GETSETOFF = 5,

        // Backward compatibility
        R_METAG_REG32OP1 = 6,
        R_METAG_REG32OP2 = 7,
        R_METAG_REG32OP3 = 8,
        R_METAG_REG16OP1 = 9,
        R_METAG_REG16OP2 = 10,
        R_METAG_REG16OP3 = 11,
        R_METAG_REG32OP4 = 12,

        R_METAG_HIOG = 13,
        R_METAG_LOOG = 14,

        R_METAG_REL8 = 15,
        R_METAG_REL16 = 16,

        R_METAG_GNU_VTINHERIT = 30,
        R_METAG_GNU_VTENTRY = 31,

        // PIC relocations
        R_METAG_HI16_GOTOFF = 32,
        R_METAG_LO16_GOTOFF = 33,
        R_METAG_GETSET_GOTOFF = 34,
        R_METAG_GETSET_GOT = 35,
        R_METAG_HI16_GOTPC = 36,
        R_METAG_LO16_GOTPC = 37,
        R_METAG_HI16_PLT = 38,
        R_METAG_LO16_PLT = 39,
        R_METAG_RELBRANCH_PLT = 40,
        R_METAG_GOTOFF = 41,
        R_METAG_PLT = 42,
        R_METAG_COPY = 43,
        R_METAG_JMP_SLOT = 44,
        R_METAG_RELATIVE = 45,
        R_METAG_GLOB_DAT = 46,

        // TLS relocations
        R_METAG_TLS_GD = 47,
        R_METAG_TLS_LDM = 48,
        R_METAG_TLS_LDO_HI16 = 49,
        R_METAG_TLS_LDO_LO16 = 50,
        R_METAG_TLS_LDO = 51,
        R_METAG_TLS_IE = 52,
        R_METAG_TLS_IENONPIC = 53,
        R_METAG_TLS_IENONPIC_HI16 = 54,
        R_METAG_TLS_IENONPIC_LO16 = 55,
        R_METAG_TLS_TPOFF = 56,
        R_METAG_TLS_DTPMOD = 57,
        R_METAG_TLS_DTPOFF = 58,
        R_METAG_TLS_LE = 59,
        R_METAG_TLS_LE_HI16 = 60,
        R_METAG_TLS_LE_LO16 = 61,
    };
}

names! {
    struct Nds32(Base);
    /// `Rel*::r_type` values for `EM_NDS32`.
    consts r = pub NAMES_R_NDS32: RelocationType(u32) = {
        R_NDS32_NONE = 0,
        R_NDS32_32_RELA = 20,
        R_NDS32_COPY = 39,
        R_NDS32_GLOB_DAT = 40,
        R_NDS32_JMP_SLOT = 41,
        R_NDS32_RELATIVE = 42,
        R_NDS32_TLS_TPOFF = 102,
        R_NDS32_TLS_DESC = 119,
    };
}

names! {
    struct Larch(Base);
    flags ef: FileFlags(u32) = {
        /// Additional properties of the base ABI type, including the FP calling
        /// convention.
        EF_LARCH_ABI_MODIFIER_MASK = 0x7 => NAMES_EF_LARCH_ABI,
        /// Uses relocation types directly writing to immediate slots
        EF_LARCH_OBJABI_V1 = 0x40,
    };
    /// `Rel*::r_type` values for `EM_LOONGARCH`.
    consts r = pub NAMES_R_LARCH: RelocationType(u32) = {
        /// No reloc
        R_LARCH_NONE = 0,
        /// Runtime address resolving
        R_LARCH_32 = 1,
        /// Runtime address resolving
        R_LARCH_64 = 2,
        /// Runtime fixup for load-address
        R_LARCH_RELATIVE = 3,
        /// Runtime memory copy in executable
        R_LARCH_COPY = 4,
        /// Runtime PLT supporting
        R_LARCH_JUMP_SLOT = 5,
        /// Runtime relocation for TLS-GD
        R_LARCH_TLS_DTPMOD32 = 6,
        /// Runtime relocation for TLS-GD
        R_LARCH_TLS_DTPMOD64 = 7,
        /// Runtime relocation for TLS-GD
        R_LARCH_TLS_DTPREL32 = 8,
        /// Runtime relocation for TLS-GD
        R_LARCH_TLS_DTPREL64 = 9,
        /// Runtime relocation for TLE-IE
        R_LARCH_TLS_TPREL32 = 10,
        /// Runtime relocation for TLE-IE
        R_LARCH_TLS_TPREL64 = 11,
        /// Runtime local indirect function resolving
        R_LARCH_IRELATIVE = 12,
        /// Runtime relocation for TLS descriptors
        R_LARCH_TLS_DESC32 = 13,
        /// Runtime relocation for TLS descriptors
        R_LARCH_TLS_DESC64 = 14,
        /// Mark la.abs: load absolute address for static link.
        R_LARCH_MARK_LA = 20,
        /// Mark external label branch: access PC relative address for static link.
        R_LARCH_MARK_PCREL = 21,
        /// Push PC-relative offset
        R_LARCH_SOP_PUSH_PCREL = 22,
        /// Push constant or absolute address
        R_LARCH_SOP_PUSH_ABSOLUTE = 23,
        /// Duplicate stack top
        R_LARCH_SOP_PUSH_DUP = 24,
        /// Push for access GOT entry
        R_LARCH_SOP_PUSH_GPREL = 25,
        /// Push for TLS-LE
        R_LARCH_SOP_PUSH_TLS_TPREL = 26,
        /// Push for TLS-IE
        R_LARCH_SOP_PUSH_TLS_GOT = 27,
        /// Push for TLS-GD
        R_LARCH_SOP_PUSH_TLS_GD = 28,
        /// Push for external function calling
        R_LARCH_SOP_PUSH_PLT_PCREL = 29,
        /// Assert stack top
        R_LARCH_SOP_ASSERT = 30,
        /// Stack top logical not (unary)
        R_LARCH_SOP_NOT = 31,
        /// Stack top subtraction (binary)
        R_LARCH_SOP_SUB = 32,
        /// Stack top left shift (binary)
        R_LARCH_SOP_SL = 33,
        /// Stack top right shift (binary)
        R_LARCH_SOP_SR = 34,
        /// Stack top addition (binary)
        R_LARCH_SOP_ADD = 35,
        /// Stack top bitwise and (binary)
        R_LARCH_SOP_AND = 36,
        /// Stack top selection (tertiary)
        R_LARCH_SOP_IF_ELSE = 37,
        /// Pop stack top to fill 5-bit signed immediate operand
        R_LARCH_SOP_POP_32_S_10_5 = 38,
        /// Pop stack top to fill 12-bit unsigned immediate operand
        R_LARCH_SOP_POP_32_U_10_12 = 39,
        /// Pop stack top to fill 12-bit signed immediate operand
        R_LARCH_SOP_POP_32_S_10_12 = 40,
        /// Pop stack top to fill 16-bit signed immediate operand
        R_LARCH_SOP_POP_32_S_10_16 = 41,
        /// Pop stack top to fill 18-bit signed immediate operand with two trailing
        /// zeros implied
        R_LARCH_SOP_POP_32_S_10_16_S2 = 42,
        /// Pop stack top to fill 20-bit signed immediate operand
        R_LARCH_SOP_POP_32_S_5_20 = 43,
        /// Pop stack top to fill 23-bit signed immediate operand with two trailing
        /// zeros implied
        R_LARCH_SOP_POP_32_S_0_5_10_16_S2 = 44,
        /// Pop stack top to fill 28-bit signed immediate operand with two trailing
        /// zeros implied
        R_LARCH_SOP_POP_32_S_0_10_10_16_S2 = 45,
        /// Pop stack top to fill an instruction
        R_LARCH_SOP_POP_32_U = 46,
        /// 8-bit in-place addition
        R_LARCH_ADD8 = 47,
        /// 16-bit in-place addition
        R_LARCH_ADD16 = 48,
        /// 24-bit in-place addition
        R_LARCH_ADD24 = 49,
        /// 32-bit in-place addition
        R_LARCH_ADD32 = 50,
        /// 64-bit in-place addition
        R_LARCH_ADD64 = 51,
        /// 8-bit in-place subtraction
        R_LARCH_SUB8 = 52,
        /// 16-bit in-place subtraction
        R_LARCH_SUB16 = 53,
        /// 24-bit in-place subtraction
        R_LARCH_SUB24 = 54,
        /// 32-bit in-place subtraction
        R_LARCH_SUB32 = 55,
        /// 64-bit in-place subtraction
        R_LARCH_SUB64 = 56,
        /// GNU C++ vtable hierarchy
        R_LARCH_GNU_VTINHERIT = 57,
        /// GNU C++ vtable member usage
        R_LARCH_GNU_VTENTRY = 58,
        /// 18-bit PC-relative jump offset with two trailing zeros
        R_LARCH_B16 = 64,
        /// 23-bit PC-relative jump offset with two trailing zeros
        R_LARCH_B21 = 65,
        /// 28-bit PC-relative jump offset with two trailing zeros
        R_LARCH_B26 = 66,
        /// 12..=31 bits of 32/64-bit absolute address
        R_LARCH_ABS_HI20 = 67,
        /// 0..=11 bits of 32/64-bit absolute address
        R_LARCH_ABS_LO12 = 68,
        /// 32..=51 bits of 64-bit absolute address
        R_LARCH_ABS64_LO20 = 69,
        /// 52..=63 bits of 64-bit absolute address
        R_LARCH_ABS64_HI12 = 70,
        /// The signed 32-bit offset `offs` from `PC & 0xfffff000` to
        /// `(S + A + 0x800) & 0xfffff000`, with 12 trailing zeros removed.
        ///
        /// We define the *PC relative anchor* for `S + A` as `PC + offs` (`offs`
        /// is sign-extended to VA bits).
        R_LARCH_PCALA_HI20 = 71,
        /// Same as R_LARCH_ABS_LO12.  0..=11 bits of the 32/64-bit offset from the
        /// [PC relative anchor][R_LARCH_PCALA_HI20].
        R_LARCH_PCALA_LO12 = 72,
        /// 32..=51 bits of the 64-bit offset from the
        /// [PC relative anchor][R_LARCH_PCALA_HI20].
        R_LARCH_PCALA64_LO20 = 73,
        /// 52..=63 bits of the 64-bit offset from the
        /// [PC relative anchor][R_LARCH_PCALA_HI20].
        R_LARCH_PCALA64_HI12 = 74,
        /// The signed 32-bit offset `offs` from `PC & 0xfffff000` to
        /// `(GP + G + 0x800) & 0xfffff000`, with 12 trailing zeros removed.
        ///
        /// We define the *PC relative anchor* for the GOT entry at `GP + G` as
        /// `PC + offs` (`offs` is sign-extended to VA bits).
        R_LARCH_GOT_PC_HI20 = 75,
        /// 0..=11 bits of the 32/64-bit offset from the
        /// [PC relative anchor][R_LARCH_GOT_PC_HI20] to the GOT entry.
        R_LARCH_GOT_PC_LO12 = 76,
        /// 32..=51 bits of the 64-bit offset from the
        /// [PC relative anchor][R_LARCH_GOT_PC_HI20] to the GOT entry.
        R_LARCH_GOT64_PC_LO20 = 77,
        /// 52..=63 bits of the 64-bit offset from the
        /// [PC relative anchor][R_LARCH_GOT_PC_HI20] to the GOT entry.
        R_LARCH_GOT64_PC_HI12 = 78,
        /// 12..=31 bits of 32/64-bit GOT entry absolute address
        R_LARCH_GOT_HI20 = 79,
        /// 0..=11 bits of 32/64-bit GOT entry absolute address
        R_LARCH_GOT_LO12 = 80,
        /// 32..=51 bits of 64-bit GOT entry absolute address
        R_LARCH_GOT64_LO20 = 81,
        /// 52..=63 bits of 64-bit GOT entry absolute address
        R_LARCH_GOT64_HI12 = 82,
        /// 12..=31 bits of TLS LE 32/64-bit offset from thread pointer
        R_LARCH_TLS_LE_HI20 = 83,
        /// 0..=11 bits of TLS LE 32/64-bit offset from thread pointer
        R_LARCH_TLS_LE_LO12 = 84,
        /// 32..=51 bits of TLS LE 64-bit offset from thread pointer
        R_LARCH_TLS_LE64_LO20 = 85,
        /// 52..=63 bits of TLS LE 64-bit offset from thread pointer
        R_LARCH_TLS_LE64_HI12 = 86,
        /// The signed 32-bit offset `offs` from `PC & 0xfffff000` to
        /// `(GP + IE + 0x800) & 0xfffff000`, with 12 trailing zeros removed.
        ///
        /// We define the *PC relative anchor* for the TLS IE GOT entry at
        /// `GP + IE` as `PC + offs` (`offs` is sign-extended to VA bits).
        R_LARCH_TLS_IE_PC_HI20 = 87,
        /// 0..=12 bits of the 32/64-bit offset from the
        /// [PC-relative anchor][R_LARCH_TLS_IE_PC_HI20] to the TLS IE GOT entry.
        R_LARCH_TLS_IE_PC_LO12 = 88,
        /// 32..=51 bits of the 64-bit offset from the
        /// [PC-relative anchor][R_LARCH_TLS_IE_PC_HI20] to the TLS IE GOT entry.
        R_LARCH_TLS_IE64_PC_LO20 = 89,
        /// 52..=63 bits of the 64-bit offset from the
        /// [PC-relative anchor][R_LARCH_TLS_IE_PC_HI20] to the TLS IE GOT entry.
        R_LARCH_TLS_IE64_PC_HI12 = 90,
        /// 12..=31 bits of TLS IE GOT entry 32/64-bit absolute address
        R_LARCH_TLS_IE_HI20 = 91,
        /// 0..=11 bits of TLS IE GOT entry 32/64-bit absolute address
        R_LARCH_TLS_IE_LO12 = 92,
        /// 32..=51 bits of TLS IE GOT entry 64-bit absolute address
        R_LARCH_TLS_IE64_LO20 = 93,
        /// 51..=63 bits of TLS IE GOT entry 64-bit absolute address
        R_LARCH_TLS_IE64_HI12 = 94,
        /// 12..=31 bits of the offset from `PC` to `GP + GD + 0x800`, where
        /// `GP + GD` is a TLS LD GOT entry
        R_LARCH_TLS_LD_PC_HI20 = 95,
        /// 12..=31 bits of TLS LD GOT entry 32/64-bit absolute address
        R_LARCH_TLS_LD_HI20 = 96,
        /// 12..=31 bits of the 32/64-bit PC-relative offset to the PC-relative
        /// anchor for the TLE GD GOT entry.
        R_LARCH_TLS_GD_PC_HI20 = 97,
        /// 12..=31 bits of TLS GD GOT entry 32/64-bit absolute address
        R_LARCH_TLS_GD_HI20 = 98,
        /// 32-bit PC relative
        R_LARCH_32_PCREL = 99,
        /// Paired with a normal relocation at the same address to indicate the
        /// instruction can be relaxed
        R_LARCH_RELAX = 100,
        /// Reserved
        R_LARCH_DELETE = 101,
        /// Delete some bytes to ensure the instruction at PC + A aligned to
        /// `A.next_power_of_two()`-byte boundary
        R_LARCH_ALIGN = 102,
        /// 22-bit PC-relative offset with two trailing zeros
        R_LARCH_PCREL20_S2 = 103,
        /// Reserved
        R_LARCH_CFA = 104,
        /// 6-bit in-place addition
        R_LARCH_ADD6 = 105,
        /// 6-bit in-place subtraction
        R_LARCH_SUB6 = 106,
        /// LEB128 in-place addition
        R_LARCH_ADD_ULEB128 = 107,
        /// LEB128 in-place subtraction
        R_LARCH_SUB_ULEB128 = 108,
        /// 64-bit PC relative
        R_LARCH_64_PCREL = 109,
        /// 18..=37 bits of `S + A - PC` into the `pcaddu18i` instruction at `PC`,
        /// and 2..=17 bits of `S + A - PC` into the `jirl` instruction at `PC + 4`
        R_LARCH_CALL36 = 110,
        /// 12..=31 bits of 32/64-bit PC-relative offset to TLS DESC GOT entry
        R_LARCH_TLS_DESC_PC_HI20 = 111,
        /// 0..=11 bits of 32/64-bit TLS DESC GOT entry address
        R_LARCH_TLS_DESC_PC_LO12 = 112,
        /// 32..=51 bits of 64-bit PC-relative offset to TLS DESC GOT entry
        R_LARCH_TLS_DESC64_PC_LO20 = 113,
        /// 52..=63 bits of 64-bit PC-relative offset to TLS DESC GOT entry
        R_LARCH_TLS_DESC64_PC_HI12 = 114,
        /// 12..=31 bits of 32/64-bit TLS DESC GOT entry absolute address
        R_LARCH_TLS_DESC_HI20 = 115,
        /// 0..=11 bits of 32/64-bit TLS DESC GOT entry absolute address
        R_LARCH_TLS_DESC_LO12 = 116,
        /// 32..=51 bits of 64-bit TLS DESC GOT entry absolute address
        R_LARCH_TLS_DESC64_LO20 = 117,
        /// 52..=63 bits of 64-bit TLS DESC GOT entry absolute address
        R_LARCH_TLS_DESC64_HI12 = 118,
        /// Used on ld.{w,d} for TLS DESC to get the resolve function address
        /// from GOT entry
        R_LARCH_TLS_DESC_LD = 119,
        /// Used on jirl for TLS DESC to call the resolve function
        R_LARCH_TLS_DESC_CALL = 120,
        /// 12..=31 bits of TLS LE 32/64-bit offset from TP register, can be relaxed
        R_LARCH_TLS_LE_HI20_R = 121,
        /// TLS LE thread pointer usage, can be relaxed
        R_LARCH_TLS_LE_ADD_R = 122,
        /// 0..=11 bits of TLS LE 32/64-bit offset from TP register, sign-extended,
        /// can be relaxed.
        R_LARCH_TLS_LE_LO12_R = 123,
        /// 22-bit PC-relative offset to TLS LD GOT entry
        R_LARCH_TLS_LD_PCREL20_S2 = 124,
        /// 22-bit PC-relative offset to TLS GD GOT entry
        R_LARCH_TLS_GD_PCREL20_S2 = 125,
        /// 22-bit PC-relative offset to TLS DESC GOT entry
        R_LARCH_TLS_DESC_PCREL20_S2 = 126,
        /// 12..=31 bits of `S + A - PC` into the `pcaddu12i` instruction at `PC`,
        /// and 2..=11 bits of `S + A - PC` into the `jirl` instruction at `PC + 4`
        R_LARCH_CALL30 = 127,
        /// The signed 32-bit offset `offs` from `PC` to `(S + A + 0x800) & 0xfffff000`.
        ///
        /// We define the *PC relative anchor* for `S + A` as `PC + offs` (`offs`
        /// is sign-extended to VA bits).
        R_LARCH_PCADD_HI20 = 128,
        /// 0..=11 bits of the 32-bit offset from the
        /// [PC relative anchor][R_LARCH_PCADD_HI20].
        R_LARCH_PCADD_LO12 = 129,
        /// The signed 32-bit offset `offs` from `PC` to
        /// `(GP + G + 0x800) & 0xfffff000`.
        ///
        /// We define the *PC relative anchor* for the GOT entry at `GP + G` as
        /// `PC + offs` (`offs` is sign-extended to VA bits).
        R_LARCH_GOT_PCADD_HI20 = 130,
        /// 0..=11 bits of the 32-bit offset from the
        /// [PC relative anchor][R_LARCH_GOT_PCADD_HI20] to the GOT entry.
        R_LARCH_GOT_PCADD_LO12 = 131,
        /// The signed 32-bit offset `offs` from `PC` to
        /// `(GP + IE + 0x800) & 0xfffff000`.
        ///
        /// We define the *PC relative anchor* for the TLS IE GOT entry at
        /// `GP + IE` as `PC + offs` (`offs` is sign-extended to VA bits).
        R_LARCH_TLS_IE_PCADD_HI20 = 132,
        /// 0..=11 bits of the 32-bit offset from the
        /// [PC-relative anchor][R_LARCH_TLS_IE_PCADD_HI20] to the TLS IE GOT entry.
        R_LARCH_TLS_IE_PCADD_LO12 = 133,
        /// The signed 32-bit offset `offs` from `PC` to
        /// `(GP + GD + 0x800) & 0xfffff000`.
        ///
        /// We define the *PC relative anchor* for the TLS LD GOT entry at
        /// `GP + GD` as `PC + offs` (`offs` is sign-extended to VA bits).
        R_LARCH_TLS_LD_PCADD_HI20 = 134,
        /// 0..=11 bits of the 32-bit offset from the
        /// [PC-relative anchor][R_LARCH_TLS_LD_PCADD_HI20] to the TLS LD GOT entry.
        R_LARCH_TLS_LD_PCADD_LO12 = 135,
        /// The signed 32-bit offset `offs` from `PC` to
        /// `(GP + GD + 0x800) & 0xfffff000`.
        ///
        /// We define the *PC relative anchor* for the TLS GD GOT entry at
        /// `GP + GD` as `PC + offs` (`offs` is sign-extended to VA bits).
        R_LARCH_TLS_GD_PCADD_HI20 = 136,
        /// 0..=11 bits of the 32-bit offset from the
        /// [PC-relative anchor][R_LARCH_TLS_GD_PCADD_HI20] to the TLS GD GOT entry.
        R_LARCH_TLS_GD_PCADD_LO12 = 137,
        /// The signed 32-bit offset `offs` from `PC` to
        /// `(GP + GD + 0x800) & 0xfffff000`.
        ///
        /// We define the *PC relative anchor* for the TLS DESC GOT entry at
        /// `GP + GD` as `PC + offs` (`offs` is sign-extended to VA bits).
        R_LARCH_TLS_DESC_PCADD_HI20 = 138,
        /// 0..=11 bits of the 32-bit offset from the
        /// [PC-relative anchor][R_LARCH_TLS_DESC_PCADD_HI20] to the TLS DESC GOT entry.
        R_LARCH_TLS_DESC_PCADD_LO12 = 139,
    };
}

constant_names!(NAMES_EF_LARCH_ABI: FileFlags(u32) = {
    /// Uses GPRs and the stack for parameter passing
    EF_LARCH_ABI_SOFT_FLOAT = 0x1,
    /// Uses GPRs, 32-bit FPRs and the stack for parameter passing
    EF_LARCH_ABI_SINGLE_FLOAT = 0x2,
    /// Uses GPRs, 64-bit FPRs and the stack for parameter passing
    EF_LARCH_ABI_DOUBLE_FLOAT = 0x3,
});

names! {
    struct Xtensa(Base);
    /// `Rel*::r_type` values for `EM_XTENSA`.
    consts r = pub NAMES_R_XTENSA: RelocationType(u32) = {
        R_XTENSA_NONE = 0,
        R_XTENSA_32 = 1,
        R_XTENSA_RTLD = 2,
        R_XTENSA_GLOB_DAT = 3,
        R_XTENSA_JMP_SLOT = 4,
        R_XTENSA_RELATIVE = 5,
        R_XTENSA_PLT = 6,
        R_XTENSA_OP0 = 8,
        R_XTENSA_OP1 = 9,
        R_XTENSA_OP2 = 10,
        R_XTENSA_ASM_EXPAND = 11,
        R_XTENSA_ASM_SIMPLIFY = 12,
        R_XTENSA_32_PCREL = 14,
        R_XTENSA_GNU_VTINHERIT = 15,
        R_XTENSA_GNU_VTENTRY = 16,
        R_XTENSA_DIFF8 = 17,
        R_XTENSA_DIFF16 = 18,
        R_XTENSA_DIFF32 = 19,
        R_XTENSA_SLOT0_OP = 20,
        R_XTENSA_SLOT1_OP = 21,
        R_XTENSA_SLOT2_OP = 22,
        R_XTENSA_SLOT3_OP = 23,
        R_XTENSA_SLOT4_OP = 24,
        R_XTENSA_SLOT5_OP = 25,
        R_XTENSA_SLOT6_OP = 26,
        R_XTENSA_SLOT7_OP = 27,
        R_XTENSA_SLOT8_OP = 28,
        R_XTENSA_SLOT9_OP = 29,
        R_XTENSA_SLOT10_OP = 30,
        R_XTENSA_SLOT11_OP = 31,
        R_XTENSA_SLOT12_OP = 32,
        R_XTENSA_SLOT13_OP = 33,
        R_XTENSA_SLOT14_OP = 34,
        R_XTENSA_SLOT0_ALT = 35,
        R_XTENSA_SLOT1_ALT = 36,
        R_XTENSA_SLOT2_ALT = 37,
        R_XTENSA_SLOT3_ALT = 38,
        R_XTENSA_SLOT4_ALT = 39,
        R_XTENSA_SLOT5_ALT = 40,
        R_XTENSA_SLOT6_ALT = 41,
        R_XTENSA_SLOT7_ALT = 42,
        R_XTENSA_SLOT8_ALT = 43,
        R_XTENSA_SLOT9_ALT = 44,
        R_XTENSA_SLOT10_ALT = 45,
        R_XTENSA_SLOT11_ALT = 46,
        R_XTENSA_SLOT12_ALT = 47,
        R_XTENSA_SLOT13_ALT = 48,
        R_XTENSA_SLOT14_ALT = 49,
        R_XTENSA_TLSDESC_FN = 50,
        R_XTENSA_TLSDESC_ARG = 51,
        R_XTENSA_TLS_DTPOFF = 52,
        R_XTENSA_TLS_TPOFF = 53,
        R_XTENSA_TLS_FUNC = 54,
        R_XTENSA_TLS_ARG = 55,
        R_XTENSA_TLS_CALL = 56,
        R_XTENSA_PDIFF8 = 57,
        R_XTENSA_PDIFF16 = 58,
        R_XTENSA_PDIFF32 = 59,
        R_XTENSA_NDIFF8 = 60,
        R_XTENSA_NDIFF16 = 61,
        R_XTENSA_NDIFF32 = 62,
    };
}

/// Encode `E_E2K_MACH_*` into `FileHeader*::e_flags`.
pub const fn ef_e2k_mach_to_flag(e_flags: u32, x: u32) -> u32 {
    (e_flags & 0xffffff) | (x << 24)
}

/// Decode `E_E2K_MACH_*` from `FileHeader*::e_flags`.
pub const fn ef_e2k_flag_to_mach(e_flags: u32) -> u32 {
    e_flags >> 24
}

// Codes of supported E2K machines.

/// -march=generic code.
///
/// Legacy. Shouldn't be created nowadays.
pub const E_E2K_MACH_BASE: u32 = 0;
/// -march=elbrus-v1 code.
///
/// Legacy. Shouldn't be created nowadays.
pub const E_E2K_MACH_EV1: u32 = 1;
/// -march=elbrus-v2 code.
pub const E_E2K_MACH_EV2: u32 = 2;
/// -march=elbrus-v3 code.
pub const E_E2K_MACH_EV3: u32 = 3;
/// -march=elbrus-v4 code.
pub const E_E2K_MACH_EV4: u32 = 4;
/// -march=elbrus-v5 code.
pub const E_E2K_MACH_EV5: u32 = 5;
/// -march=elbrus-v6 code.
pub const E_E2K_MACH_EV6: u32 = 6;
/// -march=elbrus-v7 code.
pub const E_E2K_MACH_EV7: u32 = 7;
/// -mtune=elbrus-8c code.
pub const E_E2K_MACH_8C: u32 = 19;
/// -mtune=elbrus-1c+ code.
pub const E_E2K_MACH_1CPLUS: u32 = 20;
/// -mtune=elbrus-12c code.
pub const E_E2K_MACH_12C: u32 = 21;
/// -mtune=elbrus-16c code.
pub const E_E2K_MACH_16C: u32 = 22;
/// -mtune=elbrus-2c3 code.
pub const E_E2K_MACH_2C3: u32 = 23;
/// -mtune=elbrus-48c code.
pub const E_E2K_MACH_48C: u32 = 24;
/// -mtune=elbrus-8v7 code.
pub const E_E2K_MACH_8V7: u32 = 25;

names! {
    struct E2k(Base);
    flags ef: FileFlags(u32) = {
        EF_E2K_IPD = 3,
        EF_E2K_X86APP = 4,
        EF_E2K_4MB_PAGES = 8,
        EF_E2K_INCOMPAT = 16,
        EF_E2K_PM = 32,
        EF_E2K_PACK_SEGMENTS = 64,
    };
    /// `Rel*::r_type` values for `EM_MCST_ELBRUS`.
    consts r = pub NAMES_R_E2K: RelocationType(u32) = {
        /// Direct 32 bit.
        R_E2K_32_ABS = 0,
        /// PC relative 32 bit.
        R_E2K_32_PC = 2,
        /// 32-bit offset of AP GOT entry.
        R_E2K_AP_GOT = 3,
        /// 32-bit offset of PL GOT entry.
        R_E2K_PL_GOT = 4,
        /// Create PLT entry.
        R_E2K_32_JMP_SLOT = 8,
        /// Copy relocation, 32-bit case.
        R_E2K_32_COPY = 9,
        /// Adjust by program base, 32-bit case.
        R_E2K_32_RELATIVE = 10,
        /// Adjust indirectly by program base, 32-bit case.
        R_E2K_32_IRELATIVE = 11,
        /// Size of symbol plus 32-bit addend.
        R_E2K_32_SIZE = 12,
        /// Symbol value if resolved by the definition in the same
        /// compilation unit or NULL otherwise, 32-bit case.
        R_E2K_32_DYNOPT = 13,
        /// Direct 64 bit.
        R_E2K_64_ABS = 50,
        /// Direct 64 bit for literal.
        R_E2K_64_ABS_LIT = 51,
        /// PC relative 64 bit for literal.
        R_E2K_64_PC_LIT = 54,
        /// Create PLT entry, 64-bit case.
        R_E2K_64_JMP_SLOT = 63,
        /// Copy relocation, 64-bit case.
        R_E2K_64_COPY = 64,
        /// Adjust by program base, 64-bit case.
        R_E2K_64_RELATIVE = 65,
        /// Adjust by program base for literal, 64-bit case.
        R_E2K_64_RELATIVE_LIT = 66,
        /// Adjust indirectly by program base, 64-bit case.
        R_E2K_64_IRELATIVE = 67,
        /// Size of symbol plus 64-bit addend.
        R_E2K_64_SIZE = 68,
        /// 64-bit offset of the symbol from GOT.
        R_E2K_64_GOTOFF = 69,

        /// GOT entry for ID of module containing symbol.
        R_E2K_TLS_GDMOD = 70,
        /// GOT entry for offset in module TLS block.
        R_E2K_TLS_GDREL = 71,
        /// Static TLS block offset GOT entry.
        R_E2K_TLS_IE = 74,
        /// Offset relative to static TLS block, 32-bit case.
        R_E2K_32_TLS_LE = 75,
        /// Offset relative to static TLS block, 64-bit case.
        R_E2K_64_TLS_LE = 76,
        /// ID of module containing symbol, 32-bit case.
        R_E2K_TLS_32_DTPMOD = 80,
        /// Offset in module TLS block, 32-bit case.
        R_E2K_TLS_32_DTPREL = 81,
        /// ID of module containing symbol, 64-bit case.
        R_E2K_TLS_64_DTPMOD = 82,
        /// Offset in module TLS block, 64-bit case.
        R_E2K_TLS_64_DTPREL = 83,
        /// Offset in static TLS block, 32-bit case.
        R_E2K_TLS_32_TPREL = 84,
        /// Offset in static TLS block, 64-bit case.
        R_E2K_TLS_64_TPREL = 85,

        /// Direct AP.
        R_E2K_AP = 100,
        /// Direct PL.
        R_E2K_PL = 101,

        /// 32-bit offset of the symbol's entry in GOT.
        R_E2K_GOT = 108,
        /// 32-bit offset of the symbol from GOT.
        R_E2K_GOTOFF = 109,
        /// PC relative 28 bit for DISP.
        R_E2K_DISP = 110,
        /// Prefetch insn line containing the label (symbol).
        R_E2K_PREF = 111,
        /// No reloc.
        R_E2K_NONE = 112,
        /// 32-bit offset of the symbol's entry in .got.plt.
        R_E2K_GOTPLT = 114,
        /// Is symbol resolved locally during the link.
        /// The result is encoded in 5-bit ALS.src1.
        R_E2K_ISLOCAL = 115,
        /// Is symbol resloved locally during the link.
        /// The result is encoded in a long 32-bit LTS.
        R_E2K_ISLOCAL32 = 118,
        /// The symbol's offset from GOT encoded within a 64-bit literal.
        R_E2K_64_GOTOFF_LIT = 256,
        /// Symbol value if resolved by the definition in the same
        /// compilation unit or NULL otherwise, 64-bit case.
        R_E2K_64_DYNOPT = 257,
        /// PC relative 64 bit in data.
        R_E2K_64_PC = 258,
    };
    consts dt: DynamicTag(i64) = {
        DT_E2K_LAZY = DT_LOPROC + 1,
        DT_E2K_LAZY_GOT = DT_LOPROC + 3,

        DT_E2K_INIT_GOT = DT_LOPROC + 0x101c,
        DT_E2K_EXPORT_PL = DT_LOPROC + 0x101d,
        DT_E2K_EXPORT_PLSZ = DT_LOPROC + 0x101e,
        DT_E2K_REAL_PLTGOT = DT_LOPROC + 0x101f,
        DT_E2K_NO_SELFINIT = DT_LOPROC + 0x1020,
    };
}

pub const DT_E2K_NUM: i64 = 0x1021;

newtype!(
    /// Value for the subsubsection tag in an attributes section.
    struct AttributeTag(u8);
);

newtype_constant_names!(NAMES_TAG: AttributeTag(u8) = {
    #[allow(non_upper_case_globals)]
    Tag_File = 1,
    #[allow(non_upper_case_globals)]
    Tag_Section = 2,
    #[allow(non_upper_case_globals)]
    Tag_Symbol = 3,
});

unsafe_impl_endian_pod!(
    FileHeader32,
    FileHeader64,
    SectionHeader32,
    SectionHeader64,
    CompressionHeader32,
    CompressionHeader64,
    Sym32,
    Sym64,
    Syminfo32,
    Syminfo64,
    Rel32,
    Rel64,
    Rela32,
    Rela64,
    Relr32,
    Relr64,
    ProgramHeader32,
    ProgramHeader64,
    Dyn32,
    Dyn64,
    Versym,
    Verdef,
    Verdaux,
    Verneed,
    Vernaux,
    NoteHeader32,
    NoteHeader64,
    HashHeader,
    GnuHashHeader,
);
