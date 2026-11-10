use core::convert::TryInto;
use core::fmt::Debug;
use core::mem;

use crate::endian::{self, Endian, Endianness, NativeEndian, U32};
use crate::pod::Pod;
use crate::read::{
    self, Architecture, Bytes, Error, FileFlags, Object, ObjectKind, ReadError, ReadRef,
    SectionIndex, StringTable, SymbolIndex, util,
};
use crate::{SkipDebugList, elf};

use super::{
    CompressionHeader, Dyn, DynamicTable, ElfComdat, ElfComdatIterator,
    ElfDynamicRelocationIterator, ElfExportIterator, ElfImportIterator, ElfImportLibraryIterator,
    ElfSection, ElfSectionIterator, ElfSegment, ElfSegmentIterator, ElfSymbol, ElfSymbolIterator,
    ElfSymbolTable, NoteHeader, ProgramHeader, Rel, Rela, RelocationSections, Relr, SectionHeader,
    SectionTable, Sym, SymbolTable,
};

/// A 32-bit ELF object file.
///
/// This is a file that starts with [`elf::FileHeader32`], and corresponds
/// to [`crate::FileKind::Elf32`].
pub type ElfFile32<'data, Endian = Endianness, R = &'data [u8]> =
    ElfFile<'data, elf::FileHeader32<Endian>, R>;

/// A 64-bit ELF object file.
///
/// This is a file that starts with [`elf::FileHeader64`], and corresponds
/// to [`crate::FileKind::Elf64`].
pub type ElfFile64<'data, Endian = Endianness, R = &'data [u8]> =
    ElfFile<'data, elf::FileHeader64<Endian>, R>;

/// The ELF file format that matches the pointer width and endianness of the target platform.
#[cfg(target_pointer_width = "32")]
pub type NativeElfFile<'data, R = &'data [u8]> = ElfFile32<'data, NativeEndian, R>;

/// The ELF file format that matches the pointer width and endianness of the target platform.
#[cfg(target_pointer_width = "64")]
pub type NativeElfFile<'data, R = &'data [u8]> = ElfFile64<'data, NativeEndian, R>;

/// A partially parsed ELF file.
///
/// Most functionality is provided by the [`Object`] trait implementation.
#[derive(Debug)]
pub struct ElfFile<'data, Elf, R = &'data [u8]>
where
    Elf: FileHeader,
    R: ReadRef<'data>,
{
    pub(super) endian: Elf::Endian,
    pub(super) data: SkipDebugList<R>,
    pub(super) header: &'data Elf,
    pub(super) segments: &'data [Elf::ProgramHeader],
    pub(super) sections: SectionTable<'data, Elf, R>,
    pub(super) relocations: RelocationSections,
    pub(super) symbols: SymbolTable<'data, Elf, R>,
    pub(super) dynamic_symbols: SymbolTable<'data, Elf, R>,
}

impl<'data, Elf, R> ElfFile<'data, Elf, R>
where
    Elf: FileHeader,
    R: ReadRef<'data>,
{
    /// Parse the raw ELF file data.
    pub fn parse(data: R) -> read::Result<Self> {
        let header = Elf::parse(data)?;
        let endian = header.endian()?;
        let segments = header.program_headers(endian, data)?;
        let sections = header.sections(endian, data)?;
        let symbols = sections.symbols(endian, data, elf::SHT_SYMTAB)?;
        // TODO: get dynamic symbols from DT_SYMTAB if there are no sections
        let dynamic_symbols = sections.symbols(endian, data, elf::SHT_DYNSYM)?;
        // The API we provide requires a mapping from section to relocations, so build it now.
        let relocations = sections.relocation_sections(endian, symbols.section())?;

        Ok(ElfFile {
            endian,
            data: SkipDebugList(data),
            header,
            segments,
            sections,
            relocations,
            symbols,
            dynamic_symbols,
        })
    }

    /// Returns the endianness.
    pub fn endian(&self) -> Elf::Endian {
        self.endian
    }

    /// Returns the raw data.
    pub fn data(&self) -> R {
        self.data.0
    }

    /// Returns the raw ELF file header.
    #[deprecated(note = "Use `elf_header` instead")]
    pub fn raw_header(&self) -> &'data Elf {
        self.header
    }

    /// Returns the raw ELF segments.
    #[deprecated(note = "Use `elf_program_headers` instead")]
    pub fn raw_segments(&self) -> &'data [Elf::ProgramHeader] {
        self.segments
    }

    /// Get the raw ELF file header.
    pub fn elf_header(&self) -> &'data Elf {
        self.header
    }

    /// Get the raw ELF program headers.
    ///
    /// Returns an empty slice if the file has no program headers.
    pub fn elf_program_headers(&self) -> &'data [Elf::ProgramHeader] {
        self.segments
    }

    /// Get the ELF section table.
    ///
    /// Returns an empty section table if the file has no section headers.
    pub fn elf_section_table(&self) -> &SectionTable<'data, Elf, R> {
        &self.sections
    }

    /// Get the ELF symbol table.
    ///
    /// Returns an empty symbol table if the file has no symbol table.
    pub fn elf_symbol_table(&self) -> &SymbolTable<'data, Elf, R> {
        &self.symbols
    }

    /// Get the ELF dynamic symbol table.
    ///
    /// Returns an empty symbol table if the file has no dynamic symbol table.
    pub fn elf_dynamic_symbol_table(&self) -> &SymbolTable<'data, Elf, R> {
        &self.dynamic_symbols
    }

    /// Get a mapping for linked relocation sections.
    pub fn elf_relocation_sections(&self) -> &RelocationSections {
        &self.relocations
    }

    /// Get the first `SHT_DYNAMIC` section.
    ///
    /// Returns an empty dynamic table if there is no `SHT_DYNAMIC` section.
    pub fn elf_dynamic_table(&self) -> read::Result<DynamicTable<'data, Elf, R>> {
        self.sections.dynamic_table(self.endian, self.data.0)
    }

    fn raw_section_by_name<'file>(
        &'file self,
        section_name: &[u8],
    ) -> Option<ElfSection<'data, 'file, Elf, R>> {
        self.sections
            .section_by_name(self.endian, section_name)
            .map(|(index, section)| ElfSection {
                file: self,
                index,
                section,
            })
    }

    #[cfg(feature = "compression")]
    fn zdebug_section_by_name<'file>(
        &'file self,
        section_name: &[u8],
    ) -> Option<ElfSection<'data, 'file, Elf, R>> {
        if !section_name.starts_with(b".debug_") {
            return None;
        }
        let mut name = alloc::vec::Vec::with_capacity(section_name.len() + 1);
        name.extend_from_slice(b".zdebug_");
        name.extend_from_slice(&section_name[7..]);
        self.raw_section_by_name(&name)
    }

    #[cfg(not(feature = "compression"))]
    fn zdebug_section_by_name<'file>(
        &'file self,
        _section_name: &[u8],
    ) -> Option<ElfSection<'data, 'file, Elf, R>> {
        None
    }
}

impl<'data, Elf, R> read::private::Sealed for ElfFile<'data, Elf, R>
where
    Elf: FileHeader,
    R: ReadRef<'data>,
{
}

impl<'data, Elf, R> Object<'data> for ElfFile<'data, Elf, R>
where
    Elf: FileHeader,
    R: ReadRef<'data>,
{
    type Segment<'file>
        = ElfSegment<'data, 'file, Elf, R>
    where
        Self: 'file,
        'data: 'file;
    type SegmentIterator<'file>
        = ElfSegmentIterator<'data, 'file, Elf, R>
    where
        Self: 'file,
        'data: 'file;
    type Section<'file>
        = ElfSection<'data, 'file, Elf, R>
    where
        Self: 'file,
        'data: 'file;
    type SectionIterator<'file>
        = ElfSectionIterator<'data, 'file, Elf, R>
    where
        Self: 'file,
        'data: 'file;
    type Comdat<'file>
        = ElfComdat<'data, 'file, Elf, R>
    where
        Self: 'file,
        'data: 'file;
    type ComdatIterator<'file>
        = ElfComdatIterator<'data, 'file, Elf, R>
    where
        Self: 'file,
        'data: 'file;
    type Symbol<'file>
        = ElfSymbol<'data, 'file, Elf, R>
    where
        Self: 'file,
        'data: 'file;
    type SymbolIterator<'file>
        = ElfSymbolIterator<'data, 'file, Elf, R>
    where
        Self: 'file,
        'data: 'file;
    type SymbolTable<'file>
        = ElfSymbolTable<'data, 'file, Elf, R>
    where
        Self: 'file,
        'data: 'file;
    type DynamicRelocationIterator<'file>
        = ElfDynamicRelocationIterator<'data, 'file, Elf, R>
    where
        Self: 'file,
        'data: 'file;
    type ImportLibraryIterator<'file>
        = ElfImportLibraryIterator<'data, 'file, Elf, R>
    where
        Self: 'file,
        'data: 'file;
    type ImportIterator<'file>
        = ElfImportIterator<'data, 'file, Elf, R>
    where
        Self: 'file,
        'data: 'file;
    type ExportIterator<'file>
        = ElfExportIterator<'data, 'file, Elf, R>
    where
        Self: 'file,
        'data: 'file;

    fn architecture(&self) -> Architecture {
        match (
            self.header.e_machine(self.endian),
            self.header.is_class_64(),
        ) {
            (elf::EM_AARCH64, true) => Architecture::Aarch64,
            (elf::EM_AARCH64, false) => Architecture::Aarch64_Ilp32,
            (elf::EM_ALPHA, true) => Architecture::Alpha,
            (elf::EM_ARM, _) => Architecture::Arm,
            (elf::EM_AVR, _) => Architecture::Avr,
            (elf::EM_BPF, _) => Architecture::Bpf,
            (elf::EM_CSKY, _) => Architecture::Csky,
            (elf::EM_MCST_ELBRUS, false) => Architecture::E2K32,
            (elf::EM_MCST_ELBRUS, true) => Architecture::E2K64,
            (elf::EM_386, _) => Architecture::I386,
            (elf::EM_X86_64, false) => Architecture::X86_64_X32,
            (elf::EM_X86_64, true) => Architecture::X86_64,
            (elf::EM_HEXAGON, _) => Architecture::Hexagon,
            (elf::EM_IA_64, _) => Architecture::Ia64,
            (elf::EM_LOONGARCH, false) => Architecture::LoongArch32,
            (elf::EM_LOONGARCH, true) => Architecture::LoongArch64,
            (elf::EM_68K, false) => Architecture::M68k,
            (elf::EM_MIPS, false) => {
                if self.header.e_flags(self.endian).contains(elf::EF_MIPS_ABI2) {
                    Architecture::Mips64_N32
                } else {
                    Architecture::Mips
                }
            }
            (elf::EM_MIPS, true) => Architecture::Mips64,
            (elf::EM_MSP430, _) => Architecture::Msp430,
            (elf::EM_PARISC, _) => Architecture::Hppa,
            (elf::EM_PPC, _) => Architecture::PowerPc,
            (elf::EM_PPC64, _) => Architecture::PowerPc64,
            (elf::EM_RISCV, false) => Architecture::Riscv32,
            (elf::EM_RISCV, true) => Architecture::Riscv64,
            // This is either s390 or s390x, depending on the ELF class.
            // We only support the 64-bit variant s390x here.
            (elf::EM_S390, true) => Architecture::S390x,
            (elf::EM_SBF, _) => Architecture::Sbf,
            (elf::EM_SHARC, false) => Architecture::Sharc,
            (elf::EM_SPARC, false) => Architecture::Sparc,
            (elf::EM_SPARC32PLUS, false) => Architecture::Sparc32Plus,
            (elf::EM_SPARCV9, true) => Architecture::Sparc64,
            (elf::EM_XTENSA, false) => Architecture::Xtensa,
            (elf::EM_SH, false) => Architecture::SuperH,
            _ => Architecture::Unknown,
        }
    }

    #[inline]
    fn is_little_endian(&self) -> bool {
        self.header.is_little_endian()
    }

    #[inline]
    fn is_64(&self) -> bool {
        self.header.is_class_64()
    }

    fn kind(&self) -> ObjectKind {
        match self.header.e_type(self.endian) {
            elf::ET_REL => ObjectKind::Relocatable,
            elf::ET_EXEC => ObjectKind::Executable,
            // TODO: check for `DF_1_PIE`?
            elf::ET_DYN => ObjectKind::Dynamic,
            elf::ET_CORE => ObjectKind::Core,
            _ => ObjectKind::Unknown,
        }
    }

    fn segments(&self) -> ElfSegmentIterator<'data, '_, Elf, R> {
        ElfSegmentIterator {
            file: self,
            iter: self.segments.iter(),
        }
    }

    fn section_by_name_bytes<'file>(
        &'file self,
        section_name: &[u8],
    ) -> Option<ElfSection<'data, 'file, Elf, R>> {
        self.raw_section_by_name(section_name)
            .or_else(|| self.zdebug_section_by_name(section_name))
    }

    fn section_by_index(&self, index: SectionIndex) -> read::Result<ElfSection<'data, '_, Elf, R>> {
        let section = self.sections.section(index)?;
        Ok(ElfSection {
            file: self,
            index,
            section,
        })
    }

    fn sections(&self) -> ElfSectionIterator<'data, '_, Elf, R> {
        ElfSectionIterator::new(self)
    }

    fn comdats(&self) -> ElfComdatIterator<'data, '_, Elf, R> {
        ElfComdatIterator::new(self)
    }

    fn symbol_by_index(&self, index: SymbolIndex) -> read::Result<ElfSymbol<'data, '_, Elf, R>> {
        let symbol = self.symbols.symbol(index)?;
        Ok(ElfSymbol {
            endian: self.endian,
            symbols: &self.symbols,
            index,
            symbol,
        })
    }

    fn symbols(&self) -> ElfSymbolIterator<'data, '_, Elf, R> {
        ElfSymbolIterator::new(self.endian, &self.symbols)
    }

    fn symbol_table(&self) -> Option<ElfSymbolTable<'data, '_, Elf, R>> {
        if self.symbols.is_empty() {
            return None;
        }
        Some(ElfSymbolTable {
            endian: self.endian,
            symbols: &self.symbols,
        })
    }

    fn dynamic_symbols(&self) -> ElfSymbolIterator<'data, '_, Elf, R> {
        ElfSymbolIterator::new(self.endian, &self.dynamic_symbols)
    }

    fn dynamic_symbol_table(&self) -> Option<ElfSymbolTable<'data, '_, Elf, R>> {
        if self.dynamic_symbols.is_empty() {
            return None;
        }
        Some(ElfSymbolTable {
            endian: self.endian,
            symbols: &self.dynamic_symbols,
        })
    }

    fn dynamic_relocations<'file>(
        &'file self,
    ) -> Option<ElfDynamicRelocationIterator<'data, 'file, Elf, R>> {
        Some(ElfDynamicRelocationIterator {
            section_index: SectionIndex(1),
            file: self,
            relocations: None,
        })
    }

    fn import_libraries(&self) -> read::Result<ElfImportLibraryIterator<'data, '_, Elf, R>> {
        let dynamic = self.sections.dynamic_table(self.endian, self.data.0)?;
        Ok(ElfImportLibraryIterator::new(dynamic))
    }

    fn imports(&self) -> read::Result<ElfImportIterator<'data, '_, Elf, R>> {
        let versions = self.sections.versions(self.endian, self.data.0)?;
        Ok(ElfImportIterator::new(
            self.endian,
            versions,
            &self.dynamic_symbols,
        ))
    }

    fn exports(&self) -> read::Result<ElfExportIterator<'data, '_, Elf, R>> {
        let versions = self.sections.versions(self.endian, self.data.0)?;
        Ok(ElfExportIterator::new(
            self.endian,
            versions,
            &self.dynamic_symbols,
        ))
    }

    fn has_debug_symbols(&self) -> bool {
        for section in self.sections.iter() {
            if let Ok(name) = self.sections.section_name(self.endian, section) {
                if name == b".debug_info" || name == b".zdebug_info" {
                    return true;
                }
            }
        }
        false
    }

    fn build_id(&self) -> read::Result<Option<&'data [u8]>> {
        let endian = self.endian;
        // Use section headers if present, otherwise use program headers.
        if !self.sections.is_empty() {
            for section in self.sections.iter() {
                if let Some(mut notes) = section.notes(endian, self.data.0)? {
                    while let Some(note) = notes.next()? {
                        if note.name() == elf::ELF_NOTE_GNU
                            && note.n_type(endian) == elf::NT_GNU_BUILD_ID
                        {
                            return Ok(Some(note.desc()));
                        }
                    }
                }
            }
        } else {
            for segment in self.segments {
                if let Some(mut notes) = segment.notes(endian, self.data.0)? {
                    while let Some(note) = notes.next()? {
                        if note.name() == elf::ELF_NOTE_GNU
                            && note.n_type(endian) == elf::NT_GNU_BUILD_ID
                        {
                            return Ok(Some(note.desc()));
                        }
                    }
                }
            }
        }
        Ok(None)
    }

    fn gnu_debuglink(&self) -> read::Result<Option<(&'data [u8], u32)>> {
        let section = match self.raw_section_by_name(b".gnu_debuglink") {
            Some(section) => section,
            None => return Ok(None),
        };
        let data = section
            .section
            .data(self.endian, self.data.0)
            .read_error("Invalid ELF .gnu_debuglink section offset or size")
            .map(Bytes)?;
        let filename = data
            .read_string_at(0)
            .read_error("Missing ELF .gnu_debuglink filename")?;
        let crc_offset = util::align(filename.len() + 1, 4);
        let crc = data
            .read_at::<U32<_>>(crc_offset)
            .read_error("Missing ELF .gnu_debuglink crc")?
            .get(self.endian);
        Ok(Some((filename, crc)))
    }

    fn gnu_debugaltlink(&self) -> read::Result<Option<(&'data [u8], &'data [u8])>> {
        let section = match self.raw_section_by_name(b".gnu_debugaltlink") {
            Some(section) => section,
            None => return Ok(None),
        };
        let mut data = section
            .section
            .data(self.endian, self.data.0)
            .read_error("Invalid ELF .gnu_debugaltlink section offset or size")
            .map(Bytes)?;
        let filename = data
            .read_string()
            .read_error("Missing ELF .gnu_debugaltlink filename")?;
        let build_id = data.0;
        Ok(Some((filename, build_id)))
    }

    fn relative_address_base(&self) -> u64 {
        0
    }

    fn entry(&self) -> u64 {
        self.header.e_entry(self.endian).into()
    }

    fn flags(&self) -> FileFlags {
        FileFlags::Elf {
            os_abi: self.header.e_ident().os_abi,
            abi_version: self.header.e_ident().abi_version,
            e_flags: self.header.e_flags(self.endian),
        }
    }
}

/// A trait for generic access to [`elf::FileHeader32`] and [`elf::FileHeader64`].
#[allow(missing_docs)]
pub trait FileHeader: Debug + Pod + read::private::Sealed {
    // Ideally this would be a `u64: From<Word>`, but can't express that.
    type Word: Into<u64> + Default + Copy;
    type Sword: Into<i64>;
    type Endian: endian::Endian;
    type ProgramHeader: ProgramHeader<Elf = Self, Endian = Self::Endian, Word = Self::Word>;
    type SectionHeader: SectionHeader<Elf = Self, Endian = Self::Endian, Word = Self::Word>;
    type CompressionHeader: CompressionHeader<Endian = Self::Endian, Word = Self::Word>;
    type NoteHeader: NoteHeader<Endian = Self::Endian>;
    type Dyn: Dyn<Endian = Self::Endian, Word = Self::Word>;
    type Sym: Sym<Endian = Self::Endian, Word = Self::Word>;
    type Rel: Rel<Endian = Self::Endian, Word = Self::Word>;
    type Rela: Rela<Endian = Self::Endian, Word = Self::Word> + From<Self::Rel>;
    type Relr: Relr<Endian = Self::Endian, Word = Self::Word>;

    /// Return true if this type is a 64-bit header.
    ///
    /// This is a property of the type, not a value in the header data.
    fn is_type_64(&self) -> bool;

    /// Return true if this type is a 64-bit header.
    ///
    /// This is a property of the type, not a value in the header data.
    ///
    /// This is the same as [`Self::is_type_64`], but is non-dispatchable.
    fn is_type_64_sized() -> bool
    where
        Self: Sized;

    fn e_ident(&self) -> &elf::Ident;
    fn e_type(&self, endian: Self::Endian) -> elf::FileType;
    fn e_machine(&self, endian: Self::Endian) -> elf::Machine;
    fn e_version(&self, endian: Self::Endian) -> u32;
    fn e_entry(&self, endian: Self::Endian) -> Self::Word;
    fn e_phoff(&self, endian: Self::Endian) -> Self::Word;
    fn e_shoff(&self, endian: Self::Endian) -> Self::Word;
    fn e_flags(&self, endian: Self::Endian) -> elf::FileFlags;
    fn e_ehsize(&self, endian: Self::Endian) -> u16;
    fn e_phentsize(&self, endian: Self::Endian) -> u16;
    fn e_phnum(&self, endian: Self::Endian) -> u16;
    fn e_shentsize(&self, endian: Self::Endian) -> u16;
    fn e_shnum(&self, endian: Self::Endian) -> u16;
    fn e_shstrndx(&self, endian: Self::Endian) -> elf::SymbolSection;

    // Provided methods.

    /// Read the file header.
    ///
    /// Also checks that the ident field in the file header is a supported format.
    fn parse<'data, R: ReadRef<'data>>(data: R) -> read::Result<&'data Self> {
        let header = data
            .read_at::<Self>(0)
            .read_error("Invalid ELF header size or alignment")?;
        if !header.is_supported() {
            return Err(Error("Unsupported ELF header"));
        }
        // TODO: Check self.e_ehsize?
        Ok(header)
    }

    /// Check that the ident field in the file header is a supported format.
    ///
    /// This checks the magic number, version, class, and endianness.
    fn is_supported(&self) -> bool {
        let ident = self.e_ident();
        // TODO: Check self.e_version too? Requires endian though.
        ident.magic == elf::ELFMAG
            && (self.is_type_64() || self.is_class_32())
            && (!self.is_type_64() || self.is_class_64())
            && (self.is_little_endian() || self.is_big_endian())
            && ident.version == elf::EV_CURRENT
    }

    fn is_class_32(&self) -> bool {
        self.e_ident().class == elf::ELFCLASS32
    }

    fn is_class_64(&self) -> bool {
        self.e_ident().class == elf::ELFCLASS64
    }

    fn is_little_endian(&self) -> bool {
        self.e_ident().data == elf::ELFDATA2LSB
    }

    fn is_big_endian(&self) -> bool {
        self.e_ident().data == elf::ELFDATA2MSB
    }

    fn endian(&self) -> read::Result<Self::Endian> {
        Self::Endian::from_big_endian(self.is_big_endian()).read_error("Unsupported ELF endian")
    }

    /// Return the first section header, if present.
    ///
    /// Section 0 is a special case because getting the section headers normally
    /// requires `shnum`, but `shnum` may be in the first section header.
    fn section_0<'data, R: ReadRef<'data>>(
        &self,
        endian: Self::Endian,
        data: R,
    ) -> read::Result<Option<&'data Self::SectionHeader>> {
        let shoff: u64 = self.e_shoff(endian).into();
        if shoff == 0 {
            // No section headers is ok.
            return Ok(None);
        }
        let shentsize = usize::from(self.e_shentsize(endian));
        if shentsize != mem::size_of::<Self::SectionHeader>() {
            // Section header size must match.
            return Err(Error("Invalid ELF section header entry size"));
        }
        data.read_at(shoff)
            .map(Some)
            .read_error("Invalid ELF section header offset or size")
    }

    /// Return the `e_phnum` field of the header. Handles extended values.
    ///
    /// Returns `Err` for invalid values.
    fn phnum<'data, R: ReadRef<'data>>(&self, endian: Self::Endian, data: R) -> read::Result<u32> {
        let e_phnum = self.e_phnum(endian);
        if e_phnum < elf::PN_XNUM {
            Ok(e_phnum.into())
        } else if let Some(section_0) = self.section_0(endian, data)? {
            Ok(section_0.sh_info(endian))
        } else {
            // Section 0 must exist if e_phnum overflows.
            Err(Error("Missing ELF section headers for e_phnum overflow"))
        }
    }

    /// Return the `e_shnum` field of the header. Handles extended values.
    ///
    /// Returns `Err` for invalid values.
    fn shnum<'data, R: ReadRef<'data>>(&self, endian: Self::Endian, data: R) -> read::Result<u32> {
        let e_shnum = self.e_shnum(endian);
        if e_shnum > 0 {
            Ok(e_shnum.into())
        } else if let Some(section_0) = self.section_0(endian, data)? {
            section_0
                .sh_size(endian)
                .into()
                .try_into()
                .ok()
                .read_error("Invalid ELF extended e_shnum")
        } else {
            // No section headers is ok.
            Ok(0)
        }
    }

    /// Return the `e_shstrndx` field of the header. Handles extended values.
    ///
    /// Returns `Err` for invalid values (including if the index is 0).
    fn shstrndx<'data, R: ReadRef<'data>>(
        &self,
        endian: Self::Endian,
        data: R,
    ) -> read::Result<u32> {
        let e_shstrndx = self.e_shstrndx(endian);
        if let Some(index) = e_shstrndx.index() {
            Ok(u32::from(index))
        } else if e_shstrndx == elf::SHN_XINDEX {
            // Section 0 must exist if we're trying to read e_shstrndx.
            let section_0 = self
                .section_0(endian, data)?
                .read_error("Missing ELF section headers for e_shstrndx overflow")?;
            Ok(section_0.sh_link(endian))
        } else if e_shstrndx == elf::SHN_UNDEF {
            // Valid, but we're only called if we expect a section index.
            Err(Error("Missing ELF e_shstrndx"))
        } else {
            Err(Error("Invalid ELF e_shstrndx"))
        }
    }

    /// Return the slice of program headers.
    ///
    /// Returns `Ok(&[])` if there are no program headers.
    /// Returns `Err` for invalid values.
    fn program_headers<'data, R: ReadRef<'data>>(
        &self,
        endian: Self::Endian,
        data: R,
    ) -> read::Result<&'data [Self::ProgramHeader]> {
        let phoff: u64 = self.e_phoff(endian).into();
        if phoff == 0 {
            // No program headers is ok.
            return Ok(&[]);
        }
        let phnum = self.phnum(endian, data)?;
        if phnum == 0 {
            // No program headers is ok.
            return Ok(&[]);
        }
        let phentsize = self.e_phentsize(endian) as usize;
        if phentsize != mem::size_of::<Self::ProgramHeader>() {
            // Program header size must match.
            return Err(Error("Invalid ELF program header entry size"));
        }
        data.read_slice_at(phoff, phnum as usize)
            .read_error("Invalid ELF program header size or alignment")
    }

    /// Return the slice of section headers.
    ///
    /// Returns `Ok(&[])` if there are no section headers.
    /// Returns `Err` for invalid values.
    fn section_headers<'data, R: ReadRef<'data>>(
        &self,
        endian: Self::Endian,
        data: R,
    ) -> read::Result<&'data [Self::SectionHeader]> {
        let shoff: u64 = self.e_shoff(endian).into();
        if shoff == 0 {
            // No section headers is ok.
            return Ok(&[]);
        }
        let shnum = self.shnum(endian, data)?;
        if shnum == 0 {
            // No section headers is ok.
            return Ok(&[]);
        }
        let shentsize = usize::from(self.e_shentsize(endian));
        if shentsize != mem::size_of::<Self::SectionHeader>() {
            // Section header size must match.
            return Err(Error("Invalid ELF section header entry size"));
        }
        data.read_slice_at(shoff, shnum as usize)
            .read_error("Invalid ELF section header offset/size/alignment")
    }

    /// Get the section index of the section header string table.
    ///
    /// Returns `Err` for invalid values (including if the index is 0).
    fn section_strings_index<'data, R: ReadRef<'data>>(
        &self,
        endian: Self::Endian,
        data: R,
    ) -> read::Result<SectionIndex> {
        self.shstrndx(endian, data)
            .map(|index| SectionIndex(index as usize))
    }

    /// Return the string table for the section headers.
    fn section_strings<'data, R: ReadRef<'data>>(
        &self,
        endian: Self::Endian,
        data: R,
        sections: &[Self::SectionHeader],
    ) -> read::Result<StringTable<'data, R>> {
        if sections.is_empty() {
            return Ok(StringTable::default());
        }
        let index = self.section_strings_index(endian, data)?;
        let shstrtab = sections.get(index.0).read_error("Invalid ELF e_shstrndx")?;
        let strings = if let Some((shstrtab_offset, shstrtab_size)) = shstrtab.file_range(endian) {
            let shstrtab_end = shstrtab_offset
                .checked_add(shstrtab_size)
                .read_error("Invalid ELF shstrtab size")?;
            StringTable::new(data, shstrtab_offset, shstrtab_end)
        } else {
            StringTable::default()
        };
        Ok(strings)
    }

    /// Return the section table.
    fn sections<'data, R: ReadRef<'data>>(
        &self,
        endian: Self::Endian,
        data: R,
    ) -> read::Result<SectionTable<'data, Self, R>> {
        let sections = self.section_headers(endian, data)?;
        let strings = self.section_strings(endian, data, sections)?;
        Ok(SectionTable::new(sections, strings))
    }

    /// Returns whether this is a mips64el elf file.
    fn is_mips64el(&self, endian: Self::Endian) -> bool {
        self.is_class_64() && self.is_little_endian() && self.e_machine(endian) == elf::EM_MIPS
    }
}

impl<Endian: endian::Endian> read::private::Sealed for elf::FileHeader32<Endian> {}

impl<Endian: endian::Endian> FileHeader for elf::FileHeader32<Endian> {
    type Word = u32;
    type Sword = i32;
    type Endian = Endian;
    type ProgramHeader = elf::ProgramHeader32<Endian>;
    type SectionHeader = elf::SectionHeader32<Endian>;
    type CompressionHeader = elf::CompressionHeader32<Endian>;
    type NoteHeader = elf::NoteHeader32<Endian>;
    type Dyn = elf::Dyn32<Endian>;
    type Sym = elf::Sym32<Endian>;
    type Rel = elf::Rel32<Endian>;
    type Rela = elf::Rela32<Endian>;
    type Relr = elf::Relr32<Endian>;

    #[inline]
    fn is_type_64(&self) -> bool {
        false
    }

    #[inline]
    fn is_type_64_sized() -> bool
    where
        Self: Sized,
    {
        false
    }

    #[inline]
    fn e_ident(&self) -> &elf::Ident {
        &self.e_ident
    }

    #[inline]
    fn e_type(&self, endian: Self::Endian) -> elf::FileType {
        self.e_type.get(endian)
    }

    #[inline]
    fn e_machine(&self, endian: Self::Endian) -> elf::Machine {
        self.e_machine.get(endian)
    }

    #[inline]
    fn e_version(&self, endian: Self::Endian) -> u32 {
        self.e_version.get(endian)
    }

    #[inline]
    fn e_entry(&self, endian: Self::Endian) -> Self::Word {
        self.e_entry.get(endian)
    }

    #[inline]
    fn e_phoff(&self, endian: Self::Endian) -> Self::Word {
        self.e_phoff.get(endian)
    }

    #[inline]
    fn e_shoff(&self, endian: Self::Endian) -> Self::Word {
        self.e_shoff.get(endian)
    }

    #[inline]
    fn e_flags(&self, endian: Self::Endian) -> elf::FileFlags {
        self.e_flags.get(endian)
    }

    #[inline]
    fn e_ehsize(&self, endian: Self::Endian) -> u16 {
        self.e_ehsize.get(endian)
    }

    #[inline]
    fn e_phentsize(&self, endian: Self::Endian) -> u16 {
        self.e_phentsize.get(endian)
    }

    #[inline]
    fn e_phnum(&self, endian: Self::Endian) -> u16 {
        self.e_phnum.get(endian)
    }

    #[inline]
    fn e_shentsize(&self, endian: Self::Endian) -> u16 {
        self.e_shentsize.get(endian)
    }

    #[inline]
    fn e_shnum(&self, endian: Self::Endian) -> u16 {
        self.e_shnum.get(endian)
    }

    #[inline]
    fn e_shstrndx(&self, endian: Self::Endian) -> elf::SymbolSection {
        self.e_shstrndx.get(endian)
    }
}

impl<Endian: endian::Endian> read::private::Sealed for elf::FileHeader64<Endian> {}

impl<Endian: endian::Endian> FileHeader for elf::FileHeader64<Endian> {
    type Word = u64;
    type Sword = i64;
    type Endian = Endian;
    type ProgramHeader = elf::ProgramHeader64<Endian>;
    type SectionHeader = elf::SectionHeader64<Endian>;
    type CompressionHeader = elf::CompressionHeader64<Endian>;
    type NoteHeader = elf::NoteHeader32<Endian>;
    type Dyn = elf::Dyn64<Endian>;
    type Sym = elf::Sym64<Endian>;
    type Rel = elf::Rel64<Endian>;
    type Rela = elf::Rela64<Endian>;
    type Relr = elf::Relr64<Endian>;

    #[inline]
    fn is_type_64(&self) -> bool {
        true
    }

    #[inline]
    fn is_type_64_sized() -> bool
    where
        Self: Sized,
    {
        true
    }

    #[inline]
    fn e_ident(&self) -> &elf::Ident {
        &self.e_ident
    }

    #[inline]
    fn e_type(&self, endian: Self::Endian) -> elf::FileType {
        self.e_type.get(endian)
    }

    #[inline]
    fn e_machine(&self, endian: Self::Endian) -> elf::Machine {
        self.e_machine.get(endian)
    }

    #[inline]
    fn e_version(&self, endian: Self::Endian) -> u32 {
        self.e_version.get(endian)
    }

    #[inline]
    fn e_entry(&self, endian: Self::Endian) -> Self::Word {
        self.e_entry.get(endian)
    }

    #[inline]
    fn e_phoff(&self, endian: Self::Endian) -> Self::Word {
        self.e_phoff.get(endian)
    }

    #[inline]
    fn e_shoff(&self, endian: Self::Endian) -> Self::Word {
        self.e_shoff.get(endian)
    }

    #[inline]
    fn e_flags(&self, endian: Self::Endian) -> elf::FileFlags {
        self.e_flags.get(endian)
    }

    #[inline]
    fn e_ehsize(&self, endian: Self::Endian) -> u16 {
        self.e_ehsize.get(endian)
    }

    #[inline]
    fn e_phentsize(&self, endian: Self::Endian) -> u16 {
        self.e_phentsize.get(endian)
    }

    #[inline]
    fn e_phnum(&self, endian: Self::Endian) -> u16 {
        self.e_phnum.get(endian)
    }

    #[inline]
    fn e_shentsize(&self, endian: Self::Endian) -> u16 {
        self.e_shentsize.get(endian)
    }

    #[inline]
    fn e_shnum(&self, endian: Self::Endian) -> u16 {
        self.e_shnum.get(endian)
    }

    #[inline]
    fn e_shstrndx(&self, endian: Self::Endian) -> elf::SymbolSection {
        self.e_shstrndx.get(endian)
    }
}
