use alloc::string::String;
use alloc::vec::Vec;
use core::mem;

use crate::Wrap;
use crate::elf;
use crate::endian::*;
use crate::pod;
#[cfg(feature = "read_core")]
use crate::read;
use crate::write::{self, Error, Result, StringTable, WritableBuffer, WritableBufferExt};

/// Alignment for .symtab_shndx.
pub const ALIGN_SYMTAB_SHNDX: u64 = 4;
/// Alignment for .hash
pub const ALIGN_HASH: u64 = 4;
/// Alignment for .gnu.version
pub const ALIGN_GNU_VERSYM: u64 = 2;
/// Alignment for .gnu.version_d
pub const ALIGN_GNU_VERDEF: u64 = 4;
/// Alignment for .gnu.version_r
pub const ALIGN_GNU_VERNEED: u64 = 4;

/// Native endian version of [`elf::FileHeader64`].
#[allow(missing_docs)]
#[derive(Debug, Clone, Default)]
pub struct FileHeader {
    pub os_abi: elf::OsAbi,
    pub abi_version: u8,
    pub e_type: elf::FileType,
    pub e_machine: elf::Machine,
    pub e_entry: u64,
    pub e_flags: elf::FileFlags,
}

#[cfg(feature = "read_core")]
impl FileHeader {
    /// Convert from a raw file header.
    ///
    /// The layout-related fields are not converted. See [`FileHeaderLayout`].
    pub fn from_raw<Elf: read::elf::FileHeader>(endian: Elf::Endian, header: &Elf) -> Self {
        FileHeader {
            os_abi: header.e_ident().os_abi,
            abi_version: header.e_ident().abi_version,
            e_type: header.e_type(endian),
            e_machine: header.e_machine(endian),
            e_entry: header.e_entry(endian).into(),
            e_flags: header.e_flags(endian),
        }
    }
}

/// Native endian layout-related fields of [`elf::FileHeader64`].
#[derive(Debug, Clone, Default)]
pub struct FileHeaderLayout {
    /// The file offset of the program header table.
    pub e_phoff: u64,
    /// The number of program headers.
    ///
    /// Written to `e_phnum`. Overflow is handled during write.
    pub segment_num: u32,
    /// The file offset of the section header table.
    pub e_shoff: u64,
    /// The number of section headers.
    ///
    /// Written to `e_shnum`. Overflow is handled during write.
    pub section_num: u32,
    /// The section header string table index.
    ///
    /// Written to `e_shstrndx`. Overflow is handled during write.
    pub shstrtab_index: u32,
}

#[cfg(feature = "read_core")]
impl FileHeaderLayout {
    /// Convert from a raw file header.
    ///
    /// `data` is the entire file data; it is used to obtain the first section header if required.
    pub fn from_raw<'data, Elf: read::elf::FileHeader, R: read::ReadRef<'data>>(
        endian: Elf::Endian,
        header: &Elf,
        data: R,
    ) -> read::Result<Self> {
        let e_shstrndx = header.e_shstrndx(endian);
        let shstrtab_index = if e_shstrndx == elf::SHN_UNDEF {
            0
        } else {
            header.shstrndx(endian, data)?
        };
        Ok(FileHeaderLayout {
            e_phoff: header.e_phoff(endian).into(),
            segment_num: header.phnum(endian, data)?,
            e_shoff: header.e_shoff(endian).into(),
            section_num: header.shnum(endian, data)?,
            shstrtab_index,
        })
    }
}

/// Native endian version of [`elf::ProgramHeader64`].
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub struct ProgramHeader {
    pub p_type: elf::ProgramType,
    pub p_flags: elf::ProgramFlags,
    pub p_offset: u64,
    pub p_vaddr: u64,
    pub p_paddr: u64,
    pub p_filesz: u64,
    pub p_memsz: u64,
    pub p_align: u64,
}

#[cfg(feature = "read_core")]
impl ProgramHeader {
    /// Convert from a raw program header.
    pub fn from_raw<Phdr: read::elf::ProgramHeader>(endian: Phdr::Endian, header: &Phdr) -> Self {
        ProgramHeader {
            p_type: header.p_type(endian),
            p_flags: header.p_flags(endian),
            p_offset: header.p_offset(endian).into(),
            p_vaddr: header.p_vaddr(endian).into(),
            p_paddr: header.p_paddr(endian).into(),
            p_filesz: header.p_filesz(endian).into(),
            p_memsz: header.p_memsz(endian).into(),
            p_align: header.p_align(endian).into(),
        }
    }
}

/// Native endian version of [`elf::SectionHeader64`].
#[allow(missing_docs)]
#[derive(Debug, Clone, Default)]
pub struct SectionHeader {
    /// The string table offset of the section name, or 0.
    pub sh_name: u32,
    pub sh_type: elf::SectionType,
    pub sh_flags: elf::SectionFlags,
    pub sh_addr: u64,
    pub sh_offset: u64,
    pub sh_size: u64,
    pub sh_link: u32,
    pub sh_info: u32,
    pub sh_addralign: u64,
    pub sh_entsize: u64,
}

#[cfg(feature = "read_core")]
impl SectionHeader {
    /// Convert from a raw section header.
    pub fn from_raw<Shdr: read::elf::SectionHeader>(endian: Shdr::Endian, header: &Shdr) -> Self {
        SectionHeader {
            sh_name: header.sh_name(endian),
            sh_type: header.sh_type(endian),
            sh_flags: header.sh_flags(endian),
            sh_addr: header.sh_addr(endian).into(),
            sh_offset: header.sh_offset(endian).into(),
            sh_size: header.sh_size(endian).into(),
            sh_link: header.sh_link(endian),
            sh_info: header.sh_info(endian),
            sh_addralign: header.sh_addralign(endian).into(),
            sh_entsize: header.sh_entsize(endian).into(),
        }
    }
}

/// Native endian version of [`elf::Sym64`].
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub struct Sym {
    /// The section index for the `st_shndx` field.
    ///
    /// Overflow is handled during write.
    /// For special values, set this to `None` and set `st_shndx` instead.
    pub section: Option<u32>,
    pub st_name: u32,
    pub st_info: elf::SymbolInfo,
    pub st_other: elf::SymbolOther,
    pub st_shndx: elf::SymbolSection,
    pub st_value: u64,
    pub st_size: u64,
}

#[cfg(feature = "read_core")]
impl Sym {
    /// Convert from a raw symbol.
    ///
    /// `section` can be obtained using [`read::elf::SymbolTable::symbol_section`].
    pub fn from_raw<S: read::elf::Sym>(endian: S::Endian, sym: &S, section: Option<u32>) -> Self {
        let st_shndx = sym.st_shndx(endian);
        Sym {
            section,
            st_name: sym.st_name(endian),
            st_info: sym.st_info(),
            st_other: sym.st_other(),
            st_shndx,
            st_value: sym.st_value(endian).into(),
            st_size: sym.st_size(endian).into(),
        }
    }
}

/// Unified native endian version of [`elf::Rel64`] and [`elf::Rela64`].
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub struct Rel {
    pub r_offset: u64,
    pub r_sym: u32,
    pub r_type: elf::RelocationType,
    pub r_addend: i64,
}

#[cfg(feature = "read_core")]
impl Rel {
    /// Convert from a raw relocation without an addend.
    pub fn from_rel<R: read::elf::Rel>(endian: R::Endian, rel: &R) -> Self {
        Rel {
            r_offset: rel.r_offset(endian).into(),
            r_sym: rel.r_sym(endian),
            r_type: rel.r_type(endian),
            r_addend: 0,
        }
    }

    /// Convert from a raw relocation with an addend.
    pub fn from_rela<R: read::elf::Rela>(endian: R::Endian, rela: &R, is_mips64el: bool) -> Self {
        Rel {
            r_offset: rela.r_offset(endian).into(),
            r_sym: rela.r_sym(endian, is_mips64el),
            r_type: rela.r_type(endian, is_mips64el),
            r_addend: rela.r_addend(endian).into(),
        }
    }
}

/// Information required for writing [`elf::GnuHashHeader`].
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub struct GnuHashTable {
    pub bucket_count: u32,
    pub bloom_count: u32,
    pub bloom_shift: u32,
    pub symbol_base: u32,
    pub symbol_count: u32,
}

#[cfg(feature = "read_core")]
impl GnuHashTable {
    /// Convert from a raw GNU hash header.
    ///
    /// `symbol_count` is the number of symbols in the hash. This is used in
    /// [`Encoder::gnu_hash_table`] and is not stored in the header.
    pub fn from_raw<E: Endian>(
        endian: E,
        header: &elf::GnuHashHeader<E>,
        symbol_count: u32,
    ) -> Self {
        GnuHashTable {
            bucket_count: header.bucket_count.get(endian),
            bloom_count: header.bloom_count.get(endian),
            bloom_shift: header.bloom_shift.get(endian),
            symbol_base: header.symbol_base.get(endian),
            symbol_count,
        }
    }
}

/// Information required for writing [`elf::Verdef`].
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub struct Verdef {
    pub version: u16,
    pub flags: elf::VersionFlags,
    pub index: elf::VersionIndex,
    pub aux_count: u16,
    /// The string table offset of the name for the first [`elf::Verdaux`] entry.
    ///
    /// Written to the `vda_name` field of the first verdaux.
    pub name: u32,
    /// The hash of the name for the first [`elf::Verdaux`] entry, as computed by
    /// [`elf::hash`]. Written to `vd_hash`.
    pub hash: u32,
}

#[cfg(feature = "read_core")]
impl Verdef {
    /// Convert from a raw version definition.
    ///
    /// `name` is the `vda_name` field of the first [`elf::Verdaux`] entry.
    pub fn from_raw<E: Endian>(endian: E, verdef: &elf::Verdef<E>, name: u32) -> Self {
        Verdef {
            version: verdef.vd_version.get(endian),
            flags: verdef.vd_flags.get(endian),
            index: verdef.vd_ndx.get(endian),
            aux_count: verdef.vd_cnt.get(endian),
            name,
            hash: verdef.vd_hash.get(endian),
        }
    }
}

/// Information required for writing [`elf::Verneed`].
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub struct Verneed {
    pub version: u16,
    pub aux_count: u16,
    /// The string table offset of the file name. Written to `vn_file`.
    pub file: u32,
}

#[cfg(feature = "read_core")]
impl Verneed {
    /// Convert from a raw version dependency.
    pub fn from_raw<E: Endian>(endian: E, verneed: &elf::Verneed<E>) -> Self {
        Verneed {
            version: verneed.vn_version.get(endian),
            aux_count: verneed.vn_cnt.get(endian),
            file: verneed.vn_file.get(endian),
        }
    }
}

/// Information required for writing [`elf::Vernaux`].
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub struct Vernaux {
    pub flags: elf::VersionFlags,
    pub index: elf::VersionIndex,
    /// The string table offset of the version name. Written to `vna_name`.
    pub name: u32,
    /// The hash of the version name, as computed by [`elf::hash`]. Written to `vna_hash`.
    pub hash: u32,
}

#[cfg(feature = "read_core")]
impl Vernaux {
    /// Convert from a raw version dependency auxiliary entry.
    pub fn from_raw<E: Endian>(endian: E, vernaux: &elf::Vernaux<E>) -> Self {
        Vernaux {
            flags: vernaux.vna_flags.get(endian),
            index: vernaux.vna_other.get(endian),
            name: vernaux.vna_name.get(endian),
            hash: vernaux.vna_hash.get(endian),
        }
    }
}

/// A helper for encoding headers and data when writing an ELF file.
///
/// None of the methods check for overflow when truncating addresses and file offsets for
/// 32-bit ELF. It is recommended that the caller keep track of the largest address and
/// file offset, and perform a single overflow check.
#[derive(Debug, Clone, Copy)]
pub struct Encoder<E: Endian> {
    endian: E,
    is_64: bool,
    is_mips64el: bool,
}

impl<E: Endian> Encoder<E> {
    /// Create a new `Encoder` for the given endianness and ELF class, and machine.
    ///
    /// `machine` is required to detect mips64el. If it is unknown at construction,
    /// it may be specified as `EM_NONE` here, and set later via `set_machine`.
    pub fn new(endian: E, is_64: bool, machine: elf::Machine) -> Self {
        let mut encoder = Encoder {
            endian,
            is_64,
            is_mips64el: false,
        };
        encoder.set_machine(machine);
        encoder
    }

    /// Set the machine.
    ///
    /// This is required to support the encoding of mips64el relocations.
    pub fn set_machine(&mut self, machine: elf::Machine) {
        self.is_mips64el = self.is_64 && self.endian.is_little_endian() && machine == elf::EM_MIPS;
    }

    /// Return the endianness.
    pub fn endian(self) -> E {
        self.endian
    }

    /// Return true for 64-bit ELF.
    pub fn is_64(self) -> bool {
        self.is_64
    }

    /// Return true for little-endian 64-bit MIPS.
    pub fn is_mips64el(self) -> bool {
        self.is_mips64el
    }

    /// Return the size in bytes of an address for the ELF class.
    ///
    /// This should be used as the file offset alignment for various structures
    /// such as program headers, section headers, symbols, dynamics, relocations,
    /// and .gnu.hash.
    pub fn address_size(self) -> u64 {
        if self.is_64 { 8 } else { 4 }
    }

    /// Return the size of the file header.
    pub fn file_header_size(self) -> u64 {
        if self.is_64 {
            mem::size_of::<elf::FileHeader64<Endianness>>() as u64
        } else {
            mem::size_of::<elf::FileHeader32<Endianness>>() as u64
        }
    }

    /// Write the file header.
    ///
    /// The buffer should be at the start of the file.
    ///
    /// `layout.segment_num`, `layout.section_num`, and `layout.shstrtab_index`
    /// are replaced with the appropriate sentinel values if overflow occurs.
    ///
    /// No overflow check is performed when truncating `e_entry`, `e_phoff`,
    /// and `e_shoff` for 32-bit ELF.
    pub fn file_header<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        header: &FileHeader,
        layout: &FileHeaderLayout,
    ) -> Result<()> {
        let endian = self.endian;
        let e_ident = elf::Ident {
            magic: elf::ELFMAG,
            class: if self.is_64 {
                elf::ELFCLASS64
            } else {
                elf::ELFCLASS32
            },
            data: if self.endian.is_little_endian() {
                elf::ELFDATA2LSB
            } else {
                elf::ELFDATA2MSB
            },
            version: elf::EV_CURRENT,
            os_abi: header.os_abi,
            abi_version: header.abi_version,
            padding: [0; 7],
        };

        let e_ehsize = self.file_header_size() as u16;

        let e_phentsize = if layout.segment_num == 0 {
            0
        } else {
            self.program_header_size() as u16
        };
        let e_phnum = if layout.segment_num >= elf::PN_XNUM.into() {
            if layout.section_num == 0 {
                return Err(Error(String::from(
                    "e_phnum overflow requires section headers",
                )));
            }
            elf::PN_XNUM
        } else {
            layout.segment_num as u16
        };

        let e_shentsize = if layout.section_num == 0 {
            0
        } else {
            self.section_header_size() as u16
        };
        let e_shnum = if layout.section_num >= elf::SHN_LORESERVE.into() {
            0
        } else {
            layout.section_num as u16
        };
        let e_shstrndx = elf::SymbolSection::new(layout.shstrtab_index);

        if self.is_64 {
            let data = &elf::FileHeader64 {
                e_ident,
                e_type: U16::new(endian, header.e_type),
                e_machine: U16::new(endian, header.e_machine),
                e_version: U32::new(endian, elf::EV_CURRENT.0.into()),
                e_entry: U64::new(endian, header.e_entry),
                e_phoff: U64::new(endian, layout.e_phoff),
                e_shoff: U64::new(endian, layout.e_shoff),
                e_flags: U32::new(endian, header.e_flags),
                e_ehsize: U16::new(endian, e_ehsize),
                e_phentsize: U16::new(endian, e_phentsize),
                e_phnum: U16::new(endian, e_phnum),
                e_shentsize: U16::new(endian, e_shentsize),
                e_shnum: U16::new(endian, e_shnum),
                e_shstrndx: U16::new(endian, e_shstrndx),
            };
            buffer.write_pod(data);
        } else {
            let data = &elf::FileHeader32 {
                e_ident,
                e_type: U16::new(endian, header.e_type),
                e_machine: U16::new(endian, header.e_machine),
                e_version: U32::new(endian, elf::EV_CURRENT.0.into()),
                e_entry: U32::new(endian, header.e_entry as u32),
                e_phoff: U32::new(endian, layout.e_phoff as u32),
                e_shoff: U32::new(endian, layout.e_shoff as u32),
                e_flags: U32::new(endian, header.e_flags),
                e_ehsize: U16::new(endian, e_ehsize),
                e_phentsize: U16::new(endian, e_phentsize),
                e_phnum: U16::new(endian, e_phnum),
                e_shentsize: U16::new(endian, e_shentsize),
                e_shnum: U16::new(endian, e_shnum),
                e_shstrndx: U16::new(endian, e_shstrndx),
            };
            buffer.write_pod(data);
        }

        Ok(())
    }

    /// Return the size of a program header.
    pub fn program_header_size(self) -> u64 {
        if self.is_64 {
            mem::size_of::<elf::ProgramHeader64<Endianness>>() as u64
        } else {
            mem::size_of::<elf::ProgramHeader32<Endianness>>() as u64
        }
    }

    /// Write a program header.
    ///
    /// The buffer should already be aligned to `address_size`.
    ///
    /// No overflow check is performed when truncating `p_offset`, `p_vaddr`,
    /// `p_paddr`, `p_filesz`, `p_memsz` and `p_align` for 32-bit ELF.
    pub fn program_header<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        header: &ProgramHeader,
    ) {
        let endian = self.endian;
        if self.is_64 {
            let data = &elf::ProgramHeader64 {
                p_type: U32::new(endian, header.p_type),
                p_flags: U32::new(endian, header.p_flags),
                p_offset: U64::new(endian, header.p_offset),
                p_vaddr: U64::new(endian, header.p_vaddr),
                p_paddr: U64::new(endian, header.p_paddr),
                p_filesz: U64::new(endian, header.p_filesz),
                p_memsz: U64::new(endian, header.p_memsz),
                p_align: U64::new(endian, header.p_align),
            };
            buffer.write_pod(data);
        } else {
            let data = &elf::ProgramHeader32 {
                p_type: U32::new(endian, header.p_type),
                p_offset: U32::new(endian, header.p_offset as u32),
                p_vaddr: U32::new(endian, header.p_vaddr as u32),
                p_paddr: U32::new(endian, header.p_paddr as u32),
                p_filesz: U32::new(endian, header.p_filesz as u32),
                p_memsz: U32::new(endian, header.p_memsz as u32),
                p_flags: U32::new(endian, header.p_flags),
                p_align: U32::new(endian, header.p_align as u32),
            };
            buffer.write_pod(data);
        }
    }

    /// Return the size of a section header.
    pub fn section_header_size(self) -> u64 {
        if self.is_64 {
            mem::size_of::<elf::SectionHeader64<Endianness>>() as u64
        } else {
            mem::size_of::<elf::SectionHeader32<Endianness>>() as u64
        }
    }

    /// Write the section header at index 0.
    ///
    /// The layout is used to set fields if file header values overflowed.
    ///
    /// The buffer should already be aligned to `address_size`.
    pub fn null_section_header<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        layout: &FileHeaderLayout,
    ) {
        let sh_size = if layout.section_num >= elf::SHN_LORESERVE.into() {
            layout.section_num
        } else {
            0
        };
        let sh_link = if layout.shstrtab_index >= elf::SHN_LORESERVE.into() {
            layout.shstrtab_index
        } else {
            0
        };
        let sh_info = if layout.segment_num >= elf::PN_XNUM.into() {
            layout.segment_num
        } else {
            0
        };
        let endian = self.endian;
        if self.is_64 {
            let data = &elf::SectionHeader64 {
                sh_name: U32::new(endian, 0),
                sh_type: U32::new(endian, elf::SHT_NULL),
                sh_flags: U64::new(endian, elf::SectionFlags(0)),
                sh_addr: U64::new(endian, 0),
                sh_offset: U64::new(endian, 0),
                sh_size: U64::new(endian, sh_size.into()),
                sh_link: U32::new(endian, sh_link),
                sh_info: U32::new(endian, sh_info),
                sh_addralign: U64::new(endian, 0),
                sh_entsize: U64::new(endian, 0),
            };
            buffer.write_pod(data);
        } else {
            let data = &elf::SectionHeader32 {
                sh_name: U32::new(endian, 0),
                sh_type: U32::new(endian, elf::SHT_NULL),
                sh_flags: U32::new_u64_truncate(endian, elf::SectionFlags(0)),
                sh_addr: U32::new(endian, 0),
                sh_offset: U32::new(endian, 0),
                sh_size: U32::new(endian, sh_size),
                sh_link: U32::new(endian, sh_link),
                sh_info: U32::new(endian, sh_info),
                sh_addralign: U32::new(endian, 0),
                sh_entsize: U32::new(endian, 0),
            };
            buffer.write_pod(data);
        }
    }

    /// Write a section header.
    ///
    /// The buffer should already be aligned to `address_size`.
    ///
    /// No overflow check is performed when truncating `sh_addr`, `sh_offset`,
    /// `sh_size`, `sh_addralign`, and `sh_entsize` for 32-bit ELF.
    pub fn section_header<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        section: &SectionHeader,
    ) {
        let endian = self.endian;
        if self.is_64 {
            let data = &elf::SectionHeader64 {
                sh_name: U32::new(endian, section.sh_name),
                sh_type: U32::new(endian, section.sh_type),
                sh_flags: U64::new(endian, section.sh_flags),
                sh_addr: U64::new(endian, section.sh_addr),
                sh_offset: U64::new(endian, section.sh_offset),
                sh_size: U64::new(endian, section.sh_size),
                sh_link: U32::new(endian, section.sh_link),
                sh_info: U32::new(endian, section.sh_info),
                sh_addralign: U64::new(endian, section.sh_addralign),
                sh_entsize: U64::new(endian, section.sh_entsize),
            };
            buffer.write_pod(data);
        } else {
            let data = &elf::SectionHeader32 {
                sh_name: U32::new(endian, section.sh_name),
                sh_type: U32::new(endian, section.sh_type),
                sh_flags: U32::new_u64_truncate(endian, section.sh_flags),
                sh_addr: U32::new(endian, section.sh_addr as u32),
                sh_offset: U32::new(endian, section.sh_offset as u32),
                sh_size: U32::new(endian, section.sh_size as u32),
                sh_link: U32::new(endian, section.sh_link),
                sh_info: U32::new(endian, section.sh_info),
                sh_addralign: U32::new(endian, section.sh_addralign as u32),
                sh_entsize: U32::new(endian, section.sh_entsize as u32),
            };
            buffer.write_pod(data);
        }
    }

    /// Return a section header for `elf::SHT_STRTAB`.
    ///
    /// The caller must set `sh_name`, `sh_offset`, and `sh_size`.
    pub fn strtab_section_header(self) -> SectionHeader {
        SectionHeader {
            sh_type: elf::SHT_STRTAB,
            sh_addralign: 1,
            ..SectionHeader::default()
        }
    }

    /// Return a section header for `elf::SHT_STRTAB` with `elf::SHF_ALLOC`.
    ///
    /// The caller must set `sh_name`, `sh_addr`, `sh_offset`, and `sh_size`.
    pub fn dynstr_section_header(self) -> SectionHeader {
        SectionHeader {
            sh_type: elf::SHT_STRTAB,
            sh_flags: elf::SHF_ALLOC,
            sh_addralign: 1,
            ..SectionHeader::default()
        }
    }

    /// Return a section header for `elf::SHT_SYMTAB`.
    ///
    /// `sh_link` is set to `strtab`. `sh_info` is set to `num_local`.
    /// The caller must set `sh_name`, `sh_offset`, and `sh_size`.
    pub fn symtab_section_header(self, strtab: u32, num_local: u32) -> SectionHeader {
        SectionHeader {
            sh_type: elf::SHT_SYMTAB,
            sh_link: strtab,
            sh_info: num_local,
            sh_addralign: self.address_size(),
            sh_entsize: self.sym_size(),
            ..SectionHeader::default()
        }
    }

    /// Return a section header for `elf::SHT_SYMTAB_SHNDX`.
    ///
    /// `sh_link` is set to `symtab`.
    /// The caller must set `sh_name`, `sh_offset`, `sh_size`.
    pub fn symtab_shndx_section_header(self, symtab: u32) -> SectionHeader {
        SectionHeader {
            sh_type: elf::SHT_SYMTAB_SHNDX,
            sh_link: symtab,
            sh_addralign: ALIGN_SYMTAB_SHNDX,
            sh_entsize: 4,
            ..SectionHeader::default()
        }
    }

    /// Return a section header for `elf::SHT_DYNSYM`.
    ///
    /// `sh_link` is set to `dynstr`. `sh_info` is set to `num_local`.
    /// The caller must set `sh_name`, `sh_addr`, `sh_offset`, and `sh_size`.
    pub fn dynsym_section_header(self, dynstr: u32, num_local: u32) -> SectionHeader {
        SectionHeader {
            sh_type: elf::SHT_DYNSYM,
            sh_flags: elf::SHF_ALLOC,
            sh_link: dynstr,
            sh_info: num_local,
            sh_addralign: self.address_size(),
            sh_entsize: self.sym_size(),
            ..SectionHeader::default()
        }
    }

    /// Return a section header for `elf::SHT_DYNAMIC`.
    ///
    /// `sh_link` is set to `dynstr`.
    /// The caller must set `sh_name`, `sh_addr`, `sh_offset`, and `sh_size`.
    pub fn dynamic_section_header(self, dynstr: u32) -> SectionHeader {
        SectionHeader {
            sh_type: elf::SHT_DYNAMIC,
            sh_flags: elf::SHF_WRITE | elf::SHF_ALLOC,
            sh_link: dynstr,
            sh_addralign: self.address_size(),
            sh_entsize: self.dyn_size(),
            ..SectionHeader::default()
        }
    }

    /// Return a section header for `elf::SHT_HASH`.
    ///
    /// `sh_link` is set to `dynsym`.
    /// The caller must set `sh_name`, `sh_addr`, `sh_offset`, and `sh_size`.
    pub fn hash_section_header(self, dynsym: u32) -> SectionHeader {
        SectionHeader {
            sh_type: elf::SHT_HASH,
            sh_flags: elf::SHF_ALLOC,
            sh_link: dynsym,
            sh_addralign: ALIGN_HASH,
            sh_entsize: 4,
            ..SectionHeader::default()
        }
    }

    /// Return a section header for `elf::SHT_GNU_HASH`.
    ///
    /// `sh_link` is set to `dynsym`.
    /// The caller must set `sh_name`, `sh_addr`, `sh_offset`, and `sh_size`.
    pub fn gnu_hash_section_header(self, dynsym: u32) -> SectionHeader {
        SectionHeader {
            sh_type: elf::SHT_GNU_HASH,
            sh_flags: elf::SHF_ALLOC,
            sh_link: dynsym,
            sh_addralign: self.address_size(),
            sh_entsize: if self.is_64 { 0 } else { 4 },
            ..SectionHeader::default()
        }
    }

    /// Return a section header for `elf::SHT_GNU_VERSYM`.
    ///
    /// `sh_link` is set to `dynsym`.
    /// The caller must set `sh_name`, `sh_addr`, `sh_offset`, and `sh_size`.
    pub fn gnu_versym_section_header(self, dynsym: u32) -> SectionHeader {
        SectionHeader {
            sh_type: elf::SHT_GNU_VERSYM,
            sh_flags: elf::SHF_ALLOC,
            sh_link: dynsym,
            sh_addralign: ALIGN_GNU_VERSYM,
            sh_entsize: 2,
            ..SectionHeader::default()
        }
    }

    /// Return a section header for `elf::SHT_GNU_VERDEF`.
    ///
    /// `sh_link` is set to `dynstr`. `sh_info` is set to `verdef_count`.
    /// The caller must set `sh_name`, `sh_addr`, `sh_offset`, and `sh_size`.
    pub fn gnu_verdef_section_header(self, dynstr: u32, verdef_count: u16) -> SectionHeader {
        SectionHeader {
            sh_type: elf::SHT_GNU_VERDEF,
            sh_flags: elf::SHF_ALLOC,
            sh_link: dynstr,
            sh_info: verdef_count.into(),
            sh_addralign: ALIGN_GNU_VERDEF,
            ..SectionHeader::default()
        }
    }

    /// Return a section header for `elf::SHT_GNU_VERNEED`.
    ///
    /// `sh_link` is set to `dynstr`. `sh_info` is set to `verneed_count`.
    /// The caller must set `sh_name`, `sh_addr`, `sh_offset`, and `sh_size`.
    pub fn gnu_verneed_section_header(self, dynstr: u32, verneed_count: u16) -> SectionHeader {
        SectionHeader {
            sh_type: elf::SHT_GNU_VERNEED,
            sh_flags: elf::SHF_ALLOC,
            sh_link: dynstr,
            sh_info: verneed_count.into(),
            sh_addralign: ALIGN_GNU_VERNEED,
            ..SectionHeader::default()
        }
    }

    /// Return a section header for `elf::SHT_GNU_ATTRIBUTES`.
    ///
    /// The caller must set `sh_name`, `sh_offset`, `sh_size`, and `sh_link`.
    pub fn gnu_attributes_section_header(self) -> SectionHeader {
        SectionHeader {
            sh_type: elf::SHT_GNU_ATTRIBUTES,
            sh_addralign: 1,
            ..SectionHeader::default()
        }
    }

    /// Return a section header for a relocation section.
    ///
    /// `is_rela` determines `sh_type` (`elf::SHT_RELA` or `elf::SHT_REL`) and
    /// `sh_entsize`. The caller must set `sh_name`, `sh_offset`, `sh_size`,
    /// `sh_link` (the symbol table section index), and `sh_info` (the section
    /// index the relocations apply to).
    pub fn relocation_section_header(self, is_rela: bool) -> SectionHeader {
        SectionHeader {
            sh_type: if is_rela { elf::SHT_RELA } else { elf::SHT_REL },
            sh_flags: elf::SHF_INFO_LINK,
            sh_addralign: self.address_size(),
            sh_entsize: self.rel_size(is_rela),
            ..SectionHeader::default()
        }
    }

    /// Return a section header for a relative relocation section.
    ///
    /// The caller must set `sh_name`, `sh_offset`, and `sh_size`.
    pub fn relative_relocation_section_header(self) -> SectionHeader {
        SectionHeader {
            sh_type: elf::SHT_RELA,
            sh_addralign: self.address_size(),
            sh_entsize: self.relr_size(),
            ..SectionHeader::default()
        }
    }

    /// Return a section header for a COMDAT group section (`elf::SHT_GROUP`).
    ///
    /// The caller must set `sh_name`, `sh_offset`, `sh_size`, `sh_link` (the
    /// symbol table section index), and `sh_info` (the group signature symbol
    /// index).
    pub fn comdat_section_header(self) -> SectionHeader {
        SectionHeader {
            sh_type: elf::SHT_GROUP,
            sh_addralign: 4,
            sh_entsize: 4,
            ..SectionHeader::default()
        }
    }

    /// Write the data for a string table.
    ///
    /// Returns the length of the written data.
    pub fn strtab<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        strtab: &mut StringTable<'_>,
    ) -> Result<u32> {
        buffer.write_bytes(&[0]);
        strtab.write(buffer, 1)
    }

    /// Return the size of a symbol.
    pub fn sym_size(self) -> u64 {
        if self.is_64 {
            mem::size_of::<elf::Sym64<Endianness>>() as u64
        } else {
            mem::size_of::<elf::Sym32<Endianness>>() as u64
        }
    }

    /// Write the null symbol.
    ///
    /// The buffer should already be aligned to `address_size`.
    pub fn null_symbol<W: WritableBuffer + ?Sized>(self, buffer: &mut W) {
        if self.is_64 {
            buffer.write_pod(&elf::Sym64::<Endianness>::default());
        } else {
            buffer.write_pod(&elf::Sym32::<Endianness>::default());
        }
    }

    /// Write a symbol.
    ///
    /// Returns the extended symbol index if overflow occurred.
    ///
    /// The buffer should already be aligned to `address_size`.
    ///
    /// No overflow check is performed when truncating `st_value` and `st_size`
    /// for 32-bit ELF.
    pub fn symbol<W: WritableBuffer + ?Sized>(self, buffer: &mut W, sym: &Sym) -> Option<u32> {
        let st_shndx = if let Some(section) = sym.section {
            elf::SymbolSection::new(section)
        } else {
            sym.st_shndx
        };

        let endian = self.endian;
        if self.is_64 {
            let data = &elf::Sym64 {
                st_name: U32::new(endian, sym.st_name),
                st_info: sym.st_info,
                st_other: sym.st_other,
                st_shndx: U16::new(endian, st_shndx),
                st_value: U64::new(endian, sym.st_value),
                st_size: U64::new(endian, sym.st_size),
            };
            buffer.write_pod(data);
        } else {
            let data = &elf::Sym32 {
                st_name: U32::new(endian, sym.st_name),
                st_info: sym.st_info,
                st_other: sym.st_other,
                st_shndx: U16::new(endian, st_shndx),
                st_value: U32::new(endian, sym.st_value as u32),
                st_size: U32::new(endian, sym.st_size as u32),
            };
            buffer.write_pod(data);
        }

        if st_shndx == elf::SHN_XINDEX {
            Some(sym.section.unwrap_or(0))
        } else {
            None
        }
    }

    /// Write a `u32` value.
    ///
    /// Useful for .symtab_shndx or COMDAT section groups.
    pub(crate) fn u32<W: WritableBuffer + ?Sized, T: Wrap<Inner = u32> + Copy + 'static>(
        self,
        buffer: &mut W,
        value: T,
    ) {
        buffer.write_u32(self.endian, value);
    }

    /// Return the size of a relocation entry.
    pub fn rel_size(self, is_rela: bool) -> u64 {
        if self.is_64 {
            if is_rela {
                mem::size_of::<elf::Rela64<Endianness>>() as u64
            } else {
                mem::size_of::<elf::Rel64<Endianness>>() as u64
            }
        } else {
            if is_rela {
                mem::size_of::<elf::Rela32<Endianness>>() as u64
            } else {
                mem::size_of::<elf::Rel32<Endianness>>() as u64
            }
        }
    }

    /// Return the size of a relative relocation entry.
    pub fn relr_size(self) -> u64 {
        if self.is_64 {
            mem::size_of::<elf::Relr64<Endianness>>() as u64
        } else {
            mem::size_of::<elf::Relr32<Endianness>>() as u64
        }
    }

    /// Write a relocation.
    ///
    /// The buffer should already be aligned to `address_size`.
    ///
    /// No overflow check is performed when truncating `r_offset`, `r_sym`, `r_type`,
    /// and `r_addend` for 32-bit ELF.
    pub fn relocation<W: WritableBuffer + ?Sized>(self, buffer: &mut W, is_rela: bool, rel: &Rel) {
        let endian = self.endian;
        if self.is_64 {
            if is_rela {
                let data = &elf::Rela64 {
                    r_offset: U64::new(endian, rel.r_offset),
                    r_info: elf::Rela64::r_info(endian, self.is_mips64el, rel.r_sym, rel.r_type),
                    r_addend: I64::new(endian, rel.r_addend),
                };
                buffer.write_pod(data);
            } else {
                let data = &elf::Rel64 {
                    r_offset: U64::new(endian, rel.r_offset),
                    r_info: elf::Rel64::r_info(endian, rel.r_sym, rel.r_type),
                };
                buffer.write_pod(data);
            }
        } else {
            if is_rela {
                let data = &elf::Rela32 {
                    r_offset: U32::new(endian, rel.r_offset as u32),
                    r_info: elf::Rel32::r_info(endian, rel.r_sym, rel.r_type),
                    r_addend: I32::new(endian, rel.r_addend as i32),
                };
                buffer.write_pod(data);
            } else {
                let data = &elf::Rel32 {
                    r_offset: U32::new(endian, rel.r_offset as u32),
                    r_info: elf::Rel32::r_info(endian, rel.r_sym, rel.r_type),
                };
                buffer.write_pod(data);
            }
        }
    }

    /// Return the size of a dynamic entry.
    pub fn dyn_size(self) -> u64 {
        if self.is_64 {
            mem::size_of::<elf::Dyn64<Endianness>>() as u64
        } else {
            mem::size_of::<elf::Dyn32<Endianness>>() as u64
        }
    }

    /// Write a dynamic value entry.
    ///
    /// Returns an error for 32-bit ELF overflows.
    ///
    /// The buffer should already be aligned to `address_size`.
    pub fn dynamic<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        d_tag: elf::DynamicTag,
        d_val: u64,
    ) -> Result<()> {
        let endian = self.endian;
        if self.is_64 {
            let data = &elf::Dyn64 {
                d_tag: I64::new(endian, d_tag),
                d_val: U64::new(endian, d_val),
            };
            buffer.write_pod(data);
        } else {
            let d_tag = I32::new_i64(endian, d_tag)
                .map_err(|_| Error(format!("d_tag overflow: 0x{:x}", d_tag)))?;
            let d_val = d_val
                .try_into()
                .map_err(|_| Error(format!("d_val overflow: 0x{:x}", d_val)))?;
            let data = &elf::Dyn32 {
                d_tag,
                d_val: U32::new(endian, d_val),
            };
            buffer.write_pod(data);
        }
        Ok(())
    }

    /// Return the size of a hash table.
    pub fn hash_size(self, bucket_count: u32, chain_count: u32) -> u64 {
        mem::size_of::<elf::HashHeader<Endianness>>() as u64
            + u64::from(bucket_count) * 4
            + u64::from(chain_count) * 4
    }

    /// Write a SysV hash table.
    ///
    /// `chain_count` is the number of symbols in the hash.
    /// The argument to `hash` will be in the range `0..chain_count`.
    ///
    /// The buffer should already be aligned to 4.
    pub fn hash_table<W, F>(self, buffer: &mut W, bucket_count: u32, chain_count: u32, hash: F)
    where
        W: WritableBuffer + ?Sized,
        F: Fn(u32) -> Option<u32>,
    {
        let mut buckets = vec![U32::new(self.endian, 0); bucket_count as usize];
        let mut chains = vec![U32::new(self.endian, 0); chain_count as usize];
        for i in 0..chain_count {
            if let Some(hash) = hash(i) {
                let bucket = hash % bucket_count;
                chains[i as usize] = buckets[bucket as usize];
                buckets[bucket as usize] = U32::new(self.endian, i);
            }
        }

        let data = &elf::HashHeader {
            bucket_count: U32::new(self.endian, bucket_count),
            chain_count: U32::new(self.endian, chain_count),
        };
        buffer.write_pod(data);
        buffer.write_pod_slice(&buckets);
        buffer.write_pod_slice(&chains);
    }

    /// Return the size of a GNU hash table.
    pub fn gnu_hash_size(self, bloom_count: u32, bucket_count: u32, symbol_count: u32) -> u64 {
        let bloom_size = if self.is_64 { 8 } else { 4 };
        mem::size_of::<elf::GnuHashHeader<Endianness>>() as u64
            + u64::from(bloom_count) * bloom_size
            + u64::from(bucket_count) * 4
            + u64::from(symbol_count) * 4
    }

    /// Write a GNU hash section.
    ///
    /// `symbol_count` is the number of symbols in the hash.
    /// The argument to `hash` will be in the range `0..symbol_count`.
    ///
    /// This requires that symbols are already sorted by bucket.
    pub fn gnu_hash_table<W, F>(self, buffer: &mut W, table: &GnuHashTable, hash: F)
    where
        W: WritableBuffer + ?Sized,
        F: Fn(u32) -> u32,
    {
        let GnuHashTable {
            bucket_count,
            bloom_count,
            bloom_shift,
            symbol_base,
            symbol_count,
        } = *table;

        let data = &elf::GnuHashHeader {
            bucket_count: U32::new(self.endian, bucket_count),
            symbol_base: U32::new(self.endian, symbol_base),
            bloom_count: U32::new(self.endian, bloom_count),
            bloom_shift: U32::new(self.endian, bloom_shift),
        };
        buffer.write_pod(data);

        // Calculate and write bloom filter.
        if self.is_64 {
            let mut bloom_filters = vec![0u64; bloom_count as usize];
            for i in 0..symbol_count {
                let h = hash(i);
                bloom_filters[((h / 64) & (bloom_count - 1)) as usize] |=
                    1 << (h % 64) | 1 << ((h >> bloom_shift) % 64);
            }
            for bloom_filter in bloom_filters {
                buffer.write_u64(self.endian, bloom_filter);
            }
        } else {
            let mut bloom_filters = vec![0u32; bloom_count as usize];
            for i in 0..symbol_count {
                let h = hash(i);
                bloom_filters[((h / 32) & (bloom_count - 1)) as usize] |=
                    1 << (h % 32) | 1 << ((h >> bloom_shift) % 32);
            }
            for bloom_filter in bloom_filters {
                buffer.write_u32(self.endian, bloom_filter);
            }
        }

        // Write buckets.
        //
        // This requires that symbols are already sorted by bucket.
        let mut bucket = 0;
        for i in 0..symbol_count {
            let symbol_bucket = hash(i) % bucket_count;
            while bucket < symbol_bucket {
                buffer.write_u32(self.endian, 0u32);
                bucket += 1;
            }
            if bucket == symbol_bucket {
                buffer.write_u32(self.endian, symbol_base + i);
                bucket += 1;
            }
        }
        while bucket < bucket_count {
            buffer.write_u32(self.endian, 0u32);
            bucket += 1;
        }

        // Write hash values.
        for i in 0..symbol_count {
            let mut h = hash(i);
            if i == symbol_count - 1 || h % bucket_count != hash(i + 1) % bucket_count {
                h |= 1;
            } else {
                h &= !1;
            }
            buffer.write_u32(self.endian, h);
        }
    }

    /// Return the size of a GNU symbol version section.
    pub fn gnu_versym_size(self, symbol_count: u32) -> u64 {
        u64::from(symbol_count) * 2
    }

    /// Write a symbol version entry.
    pub fn gnu_versym<W: WritableBuffer + ?Sized>(self, buffer: &mut W, versym: elf::VersymIndex) {
        buffer.write_u16(self.endian, versym);
    }

    /// Return the size of a GNU version definition section.
    pub fn gnu_verdef_size(self, verdef_count: u16, verdaux_count: usize) -> u64 {
        u64::from(verdef_count) * mem::size_of::<elf::Verdef<Endianness>>() as u64
            + verdaux_count as u64 * mem::size_of::<elf::Verdaux<Endianness>>() as u64
    }

    /// Write a version definition entry.
    pub fn gnu_verdef<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        next: bool,
        verdef: &Verdef,
    ) {
        let vd_next = if next {
            mem::size_of::<elf::Verdef<Endianness>>() as u32
                + u32::from(verdef.aux_count) * mem::size_of::<elf::Verdaux<Endianness>>() as u32
        } else {
            0
        };
        let vd_aux = mem::size_of::<elf::Verdef<Endianness>>() as u32;
        let data = &elf::Verdef {
            vd_version: U16::new(self.endian, verdef.version),
            vd_flags: U16::new(self.endian, verdef.flags),
            vd_ndx: U16::new(self.endian, verdef.index),
            vd_cnt: U16::new(self.endian, verdef.aux_count),
            vd_hash: U32::new(self.endian, verdef.hash),
            vd_aux: U32::new(self.endian, vd_aux),
            vd_next: U32::new(self.endian, vd_next),
        };
        buffer.write_pod(data);
        self.gnu_verdaux(buffer, verdef.aux_count > 1, verdef.name);
    }

    /// Write a version definition entry that shares the names of the next definition.
    ///
    /// This is typically useful when there are only two versions (including the base)
    /// and they have the same name.
    pub fn gnu_verdef_shared<W: WritableBuffer + ?Sized>(self, buffer: &mut W, verdef: &Verdef) {
        let vd_next = mem::size_of::<elf::Verdef<Endianness>>() as u32;
        let vd_aux = 2 * mem::size_of::<elf::Verdef<Endianness>>() as u32;
        let data = &elf::Verdef {
            vd_version: U16::new(self.endian, verdef.version),
            vd_flags: U16::new(self.endian, verdef.flags),
            vd_ndx: U16::new(self.endian, verdef.index),
            vd_cnt: U16::new(self.endian, verdef.aux_count),
            vd_hash: U32::new(self.endian, verdef.hash),
            vd_aux: U32::new(self.endian, vd_aux),
            vd_next: U32::new(self.endian, vd_next),
        };
        buffer.write_pod(data);
    }

    /// Write a version definition auxiliary entry.
    ///
    /// `name` is the offset of the name in the dynamic string table.
    pub fn gnu_verdaux<W: WritableBuffer + ?Sized>(self, buffer: &mut W, next: bool, name: u32) {
        let vda_next = if next {
            mem::size_of::<elf::Verdaux<Endianness>>() as u32
        } else {
            0
        };
        let data = &elf::Verdaux {
            vda_name: U32::new(self.endian, name),
            vda_next: U32::new(self.endian, vda_next),
        };
        buffer.write_pod(data);
    }

    /// Return the size of a GNU version dependency section.
    pub fn gnu_verneed_size(self, verneed_count: u16, vernaux_count: usize) -> u64 {
        u64::from(verneed_count) * mem::size_of::<elf::Verneed<Endianness>>() as u64
            + vernaux_count as u64 * mem::size_of::<elf::Vernaux<Endianness>>() as u64
    }

    /// Write a version needed entry.
    pub fn gnu_verneed<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        next: bool,
        verneed: &Verneed,
    ) {
        let vn_next = if next {
            mem::size_of::<elf::Verneed<Endianness>>() as u32
                + u32::from(verneed.aux_count) * mem::size_of::<elf::Vernaux<Endianness>>() as u32
        } else {
            0
        };
        let vn_aux = if verneed.aux_count != 0 {
            mem::size_of::<elf::Verneed<Endianness>>() as u32
        } else {
            0
        };
        let data = &elf::Verneed {
            vn_version: U16::new(self.endian, verneed.version),
            vn_cnt: U16::new(self.endian, verneed.aux_count),
            vn_file: U32::new(self.endian, verneed.file),
            vn_aux: U32::new(self.endian, vn_aux),
            vn_next: U32::new(self.endian, vn_next),
        };
        buffer.write_pod(data);
    }

    /// Write a version needed auxiliary entry.
    pub fn gnu_vernaux<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        next: bool,
        vernaux: &Vernaux,
    ) {
        let vna_next = if next {
            mem::size_of::<elf::Vernaux<Endianness>>() as u32
        } else {
            0
        };
        let data = &elf::Vernaux {
            vna_hash: U32::new(self.endian, vernaux.hash),
            vna_flags: U16::new(self.endian, vernaux.flags),
            vna_other: U16::new(self.endian, vernaux.index),
            vna_name: U32::new(self.endian, vernaux.name),
            vna_next: U32::new(self.endian, vna_next),
        };
        buffer.write_pod(data);
    }
}

/// A helper for writing an attributes section.
///
/// Attributes have a variable length encoding, so it is awkward to write them in a
/// single pass. Instead, we build the entire attributes section data in memory, using
/// placeholders for unknown lengths that are filled in later.
#[allow(missing_debug_implementations)]
pub struct AttributesWriter {
    endian: Endianness,
    data: Vec<u8>,
    subsection_offset: u32,
    subsubsection_offset: u32,
}

impl AttributesWriter {
    /// Create a new `AttributesWriter` for the given endianness.
    pub fn new(endian: Endianness) -> Self {
        AttributesWriter {
            endian,
            data: vec![0x41],
            subsection_offset: 0,
            subsubsection_offset: 0,
        }
    }

    fn offset(&self) -> u32 {
        self.data.len() as u32
    }

    /// Start a new subsection with the given vendor name.
    pub fn start_subsection(&mut self, vendor: &[u8]) {
        debug_assert_eq!(self.subsection_offset, 0);
        debug_assert_eq!(self.subsubsection_offset, 0);
        self.subsection_offset = self.offset();
        self.data.extend_from_slice(&[0; 4]);
        self.data.extend_from_slice(vendor);
        self.data.push(0);
    }

    /// End the subsection.
    ///
    /// The subsection length is automatically calculated and written.
    pub fn end_subsection(&mut self) {
        debug_assert_ne!(self.subsection_offset, 0);
        debug_assert_eq!(self.subsubsection_offset, 0);
        let length = self.offset() - self.subsection_offset;
        self.data[self.subsection_offset as usize..][..4]
            .copy_from_slice(pod::bytes_of(&U32::new(self.endian, length)));
        self.subsection_offset = 0;
    }

    /// Start a new sub-subsection with the given tag.
    pub fn start_subsubsection(&mut self, tag: elf::AttributeTag) {
        debug_assert_ne!(self.subsection_offset, 0);
        debug_assert_eq!(self.subsubsection_offset, 0);
        self.subsubsection_offset = self.offset();
        self.data.push(tag.0);
        self.data.extend_from_slice(&[0; 4]);
    }

    /// Write a section or symbol index to the sub-subsection.
    ///
    /// The user must also call this function to write the terminating 0 index.
    pub fn write_subsubsection_index(&mut self, index: u32) {
        debug_assert_ne!(self.subsection_offset, 0);
        debug_assert_ne!(self.subsubsection_offset, 0);
        write::write_uleb128(&mut self.data, u64::from(index));
    }

    /// Write raw index data to the sub-subsection.
    ///
    /// The terminating 0 index is automatically written.
    pub fn write_subsubsection_indices(&mut self, indices: &[u8]) {
        debug_assert_ne!(self.subsection_offset, 0);
        debug_assert_ne!(self.subsubsection_offset, 0);
        self.data.extend_from_slice(indices);
        self.data.push(0);
    }

    /// Write an attribute tag to the sub-subsection.
    pub fn write_attribute_tag(&mut self, tag: u64) {
        debug_assert_ne!(self.subsection_offset, 0);
        debug_assert_ne!(self.subsubsection_offset, 0);
        write::write_uleb128(&mut self.data, tag);
    }

    /// Write an attribute integer value to the sub-subsection.
    pub fn write_attribute_integer(&mut self, value: u64) {
        debug_assert_ne!(self.subsection_offset, 0);
        debug_assert_ne!(self.subsubsection_offset, 0);
        write::write_uleb128(&mut self.data, value);
    }

    /// Write an attribute string value to the sub-subsection.
    ///
    /// The value must not include the null terminator.
    pub fn write_attribute_string(&mut self, value: &[u8]) {
        debug_assert_ne!(self.subsection_offset, 0);
        debug_assert_ne!(self.subsubsection_offset, 0);
        self.data.extend_from_slice(value);
        self.data.push(0);
    }

    /// Write raw attribute data to the sub-subsection.
    pub fn write_subsubsection_attributes(&mut self, attributes: &[u8]) {
        debug_assert_ne!(self.subsection_offset, 0);
        debug_assert_ne!(self.subsubsection_offset, 0);
        self.data.extend_from_slice(attributes);
    }

    /// End the sub-subsection.
    ///
    /// The sub-subsection length is automatically calculated and written.
    pub fn end_subsubsection(&mut self) {
        debug_assert_ne!(self.subsection_offset, 0);
        debug_assert_ne!(self.subsubsection_offset, 0);
        let length = self.offset() - self.subsubsection_offset;
        self.data[self.subsubsection_offset as usize + 1..][..4]
            .copy_from_slice(pod::bytes_of(&U32::new(self.endian, length)));
        self.subsubsection_offset = 0;
    }

    /// Return the completed section data.
    pub fn data(self) -> Vec<u8> {
        debug_assert_eq!(self.subsection_offset, 0);
        debug_assert_eq!(self.subsubsection_offset, 0);
        debug_assert!((self.data.len() as u64) < (u32::MAX as u64));
        self.data
    }
}
