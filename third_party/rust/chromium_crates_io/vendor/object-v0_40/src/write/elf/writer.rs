//! Helper for writing ELF files.
use alloc::string::String;
use alloc::vec::Vec;

use crate::elf;
use crate::endian::*;
use crate::write::elf::encoder::*;
use crate::write::string::{StringId, StringTable};
use crate::write::util;
use crate::write::{CountingBuffer, Error, GrowableBuffer, Result, WritableBuffer};

/// The index of an ELF section.
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct SectionIndex(pub u32);

/// The index of an ELF symbol.
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct SymbolIndex(pub u32);

/// The writing mode of a [`Writer`].
///
/// This is a sealed trait with two implementors:
/// - [`TwoPhase`]: reserve all file ranges, then write.
/// - [`SinglePhase`]: write directly, discovering file offsets as you go.
pub trait Mode: ModeSealed {}

use private::ModeSealed;
mod private {
    use super::*;

    #[allow(private_interfaces)]
    pub trait ModeSealed {
        fn string_table() -> StringTable<'static>;
        fn reserve(
            &self,
            buffer: &mut dyn WritableBuffer,
            encoder: Encoder<Endianness>,
        ) -> Result<()>;
        fn need_offset(offset: u64) -> bool;
        fn set_offset(dest: &mut u64, offset: u64);
        fn set_size(dest: &mut u64, size: u64);
        fn set_section_index(dest: &mut u32, index: SectionIndex);
        fn set_section_name(
            dest: &mut Option<StringId>,
            shstrtab: &mut StringTable<'_>,
            name: &'static [u8],
        );
        fn set_count<T: Copy + PartialOrd>(dest: &mut T, count: T);
    }
}

/// A helper for writing ELF files.
///
/// This is a stateful layer over [`Encoder`] to keep track over bookkeeping
/// information such as strings, section ranges, section indices, and counts of items.
///
/// The writer supports two modes: [`TwoPhase`] or [`SinglePhase`].
/// See the mode documentation for a description of the use of the writer.
///
/// The default mode is two-phase. Use [`Writer::new_single_phase`] to construct
/// a single-phase writer; the [`SinglePhaseWriter`] type alias is also provided.
#[allow(missing_debug_implementations)]
pub struct Writer<'a, M: Mode = TwoPhase> {
    mode: M,
    encoder: Encoder<Endianness>,
    buffer: CountingBuffer<&'a mut dyn WritableBuffer>,

    header: FileHeader,
    layout: FileHeaderLayout,
    written_segment_num: u32,
    written_section_num: u32,

    shstrtab: StringTable<'a>,
    shstrtab_str_id: Option<StringId>,
    shstrtab_offset: u64,
    shstrtab_size: u32,

    need_strtab: bool,
    strtab: StringTable<'a>,
    strtab_str_id: Option<StringId>,
    strtab_index: SectionIndex,
    strtab_offset: u64,
    strtab_size: u32,

    symtab_str_id: Option<StringId>,
    symtab_index: SectionIndex,
    symtab_offset: u64,
    symtab_num: u32,
    written_symtab_num: u32,

    need_symtab_shndx: bool,
    symtab_shndx_str_id: Option<StringId>,
    symtab_shndx_offset: u64,
    symtab_shndx_data: Vec<u8>,

    need_dynstr: bool,
    dynstr: StringTable<'a>,
    dynstr_str_id: Option<StringId>,
    dynstr_index: SectionIndex,
    dynstr_offset: u64,
    dynstr_size: u32,

    dynsym_str_id: Option<StringId>,
    dynsym_index: SectionIndex,
    dynsym_offset: u64,
    dynsym_num: u32,
    written_dynsym_num: u32,

    dynamic_str_id: Option<StringId>,
    dynamic_offset: u64,
    dynamic_num: u64,
    written_dynamic_num: u64,

    hash_str_id: Option<StringId>,
    hash_offset: u64,
    hash_size: u64,

    gnu_hash_str_id: Option<StringId>,
    gnu_hash_offset: u64,
    gnu_hash_size: u64,

    gnu_versym_str_id: Option<StringId>,
    gnu_versym_offset: u64,
    gnu_versym_num: u32,
    written_versym_num: u32,

    gnu_verdef_str_id: Option<StringId>,
    gnu_verdef_offset: u64,
    gnu_verdef_count: u16,
    gnu_verdaux_count: usize,
    written_verdaux_count: usize,
    gnu_verdef_remaining: u16,
    gnu_verdaux_remaining: u16,

    gnu_verneed_str_id: Option<StringId>,
    gnu_verneed_offset: u64,
    gnu_verneed_count: u16,
    gnu_vernaux_count: usize,
    written_vernaux_count: usize,
    gnu_verneed_remaining: u16,
    gnu_vernaux_remaining: u16,

    gnu_attributes_str_id: Option<StringId>,
    gnu_attributes_offset: u64,
    gnu_attributes_size: u64,
}

impl<'a, M: Mode> Writer<'a, M> {
    /// Create a new `Writer` for the given endianness and ELF class.
    fn new_with_mode(
        endian: Endianness,
        is_64: bool,
        buffer: &'a mut dyn WritableBuffer,
        mode: M,
    ) -> Self {
        Writer {
            mode,
            encoder: Encoder::new(endian, is_64, elf::EM_NONE),
            buffer: CountingBuffer::new(buffer),

            header: FileHeader::default(),
            layout: FileHeaderLayout::default(),
            written_segment_num: 0,
            written_section_num: 0,

            shstrtab: M::string_table(),
            shstrtab_str_id: None,
            shstrtab_offset: 0,
            shstrtab_size: 0,

            need_strtab: false,
            strtab: M::string_table(),
            strtab_str_id: None,
            strtab_index: SectionIndex(0),
            strtab_offset: 0,
            strtab_size: 0,

            symtab_str_id: None,
            symtab_index: SectionIndex(0),
            symtab_offset: 0,
            symtab_num: 0,
            written_symtab_num: 0,

            need_symtab_shndx: false,
            symtab_shndx_str_id: None,
            symtab_shndx_offset: 0,
            symtab_shndx_data: Vec::new(),

            need_dynstr: false,
            dynstr: M::string_table(),
            dynstr_str_id: None,
            dynstr_index: SectionIndex(0),
            dynstr_offset: 0,
            dynstr_size: 0,

            dynsym_str_id: None,
            dynsym_index: SectionIndex(0),
            dynsym_offset: 0,
            dynsym_num: 0,
            written_dynsym_num: 0,

            dynamic_str_id: None,
            dynamic_offset: 0,
            dynamic_num: 0,
            written_dynamic_num: 0,

            hash_str_id: None,
            hash_offset: 0,
            hash_size: 0,

            gnu_hash_str_id: None,
            gnu_hash_offset: 0,
            gnu_hash_size: 0,

            gnu_versym_str_id: None,
            gnu_versym_offset: 0,
            gnu_versym_num: 0,
            written_versym_num: 0,

            gnu_verdef_str_id: None,
            gnu_verdef_offset: 0,
            gnu_verdef_count: 0,
            gnu_verdaux_count: 0,
            written_verdaux_count: 0,
            gnu_verdef_remaining: 0,
            gnu_verdaux_remaining: 0,

            gnu_verneed_str_id: None,
            gnu_verneed_offset: 0,
            gnu_verneed_count: 0,
            gnu_vernaux_count: 0,
            written_vernaux_count: 0,
            gnu_verneed_remaining: 0,
            gnu_vernaux_remaining: 0,

            gnu_attributes_str_id: None,
            gnu_attributes_offset: 0,
            gnu_attributes_size: 0,
        }
    }

    /// Return the current file length that has been written.
    #[allow(clippy::len_without_is_empty)]
    pub fn len(&self) -> u64 {
        self.buffer.count()
    }

    /// Return the file offset of the next write.
    pub fn offset(&self) -> u64 {
        self.buffer.count()
    }

    /// Write alignment padding bytes.
    ///
    /// Returns the file offset after the padding.
    pub fn write_align(&mut self, align_start: u64) -> u64 {
        if align_start > 1 {
            self.buffer.write_align(align_start);
        }
        self.offset()
    }

    /// Write data.
    ///
    /// This is typically used to write section data.
    pub fn write(&mut self, data: &[u8]) {
        self.buffer.write_bytes(data);
    }

    /// Write padding up to the given file offset.
    pub fn pad_until(&mut self, offset: u64) {
        debug_assert!(self.offset() <= offset);
        self.buffer.resize(offset);
    }

    /// Write the file header.
    ///
    /// This must be at the start of the file.
    ///
    /// Fields that can be derived from known information are automatically set by this function.
    ///
    /// For [`TwoPhase`] mode, this calls [`WritableBuffer::reserve`] with the total file size.
    pub fn write_file_header(&mut self, header: &FileHeader) -> Result<()> {
        debug_assert_eq!(self.buffer.count(), 0);

        self.mode.reserve(&mut self.buffer, self.encoder)?;
        self.header = header.clone();
        self.encoder.set_machine(header.e_machine);
        self.encoder
            .file_header(&mut self.buffer, &self.header, &self.layout)
    }

    /// Write alignment padding bytes prior to the program headers.
    ///
    /// Returns the file offset after the padding.
    /// In two-phase mode, returns 0 without writing if no program headers were reserved.
    pub fn write_align_program_headers(&mut self) -> u64 {
        if !M::need_offset(self.layout.e_phoff) {
            return 0;
        }
        let offset = self.write_align(self.encoder.address_size());
        M::set_offset(&mut self.layout.e_phoff, offset);
        offset
    }

    /// Write a program header.
    ///
    /// Must be called after [`Self::write_align_program_headers`].
    pub fn write_program_header(&mut self, header: &ProgramHeader) {
        self.encoder.program_header(&mut self.buffer, header);
        self.written_segment_num += 1;
        M::set_count(&mut self.layout.segment_num, self.written_segment_num);
    }

    /// Write the null section header.
    ///
    /// This must be the first section header that is written.
    ///
    /// Returns the file offset of the header.
    /// In two-phase mode, returns 0 without writing if no sections were reserved.
    pub fn write_null_section_header(&mut self) -> u64 {
        if !M::need_offset(self.layout.e_shoff) {
            return 0;
        }
        let offset = self.write_align(self.encoder.address_size());
        M::set_offset(&mut self.layout.e_shoff, offset);
        self.encoder
            .null_section_header(&mut self.buffer, &self.layout);
        self.written_section_num = 1;
        M::set_count(&mut self.layout.section_num, self.written_section_num);
        offset
    }

    /// Write a section header.
    ///
    /// Must be called after [`Self::write_null_section_header`].
    pub fn write_section_header(&mut self, section: &SectionHeader) -> SectionIndex {
        self.encoder.section_header(&mut self.buffer, section);
        let index = SectionIndex(self.written_section_num);
        self.written_section_num += 1;
        M::set_count(&mut self.layout.section_num, self.written_section_num);
        index
    }

    /// Add a section name to the section header string table.
    ///
    /// This will be stored in the `.shstrtab` section.
    ///
    /// Must be called before [`Self::reserve_shstrtab`] in two-phase mode,
    /// or before [`Self::write_shstrtab`] in single-phase mode.
    pub fn add_section_name(&mut self, name: &'a [u8]) -> StringId {
        debug_assert_eq!(self.shstrtab_offset, 0);
        self.shstrtab.add(name)
    }

    /// Return the offset of a section name in the section header string table.
    ///
    /// Returns 0 if `id` is `None`. Pairs with [`Self::add_section_name`].
    ///
    /// In two-phase mode, must be called after [`Self::reserve_shstrtab`].
    pub fn section_name_offset(&self, id: Option<StringId>) -> u32 {
        id.map_or(0, |id| self.shstrtab.get_offset(id))
    }

    /// Write the section header for the section header string table.
    ///
    /// The header is only written if the section index was reserved in two-phase
    /// mode, or [`Self::write_shstrtab`] was called in single-phase mode.
    pub fn write_shstrtab_section_header(&mut self) {
        if self.shstrtab_str_id.is_none() {
            return;
        }
        debug_assert_ne!(self.shstrtab_offset, 0);
        let index = self.write_section_header(&SectionHeader {
            sh_name: self.section_name_offset(self.shstrtab_str_id),
            sh_offset: self.shstrtab_offset,
            sh_size: self.shstrtab_size.into(),
            ..self.encoder.strtab_section_header()
        });
        M::set_section_index(&mut self.layout.shstrtab_index, index);
    }

    /// Add a string to the string table.
    ///
    /// This will be stored in the `.strtab` section.
    ///
    /// Must be called before [`Self::reserve_strtab`] in two-phase mode,
    /// or before [`Self::write_strtab`] in single-phase mode.
    pub fn add_string(&mut self, name: &'a [u8]) -> StringId {
        debug_assert_eq!(self.strtab_offset, 0);
        self.strtab.add(name)
    }

    /// Return the offset of a string in the string table.
    ///
    /// Returns 0 if `id` is `None`. Pairs with [`Self::add_string`].
    ///
    /// In two-phase mode, must be called after [`Self::reserve_strtab`].
    pub fn string_offset(&self, id: Option<StringId>) -> u32 {
        id.map_or(0, |id| self.strtab.get_offset(id))
    }

    /// Write the section header for the string table.
    ///
    /// Only writes the header if the section index was reserved in two-phase mode,
    /// or [`Self::write_strtab`] wrote the string table in single-phase mode.
    pub fn write_strtab_section_header(&mut self) {
        if self.strtab_str_id.is_none() {
            return;
        }
        let index = self.write_section_header(&SectionHeader {
            sh_name: self.section_name_offset(self.strtab_str_id),
            sh_offset: self.strtab_offset,
            sh_size: self.strtab_size.into(),
            ..self.encoder.strtab_section_header()
        });
        M::set_section_index(&mut self.strtab_index.0, index);
    }

    /// Write the null symbol.
    ///
    /// This must be the first symbol that is written.
    ///
    /// Returns the file offset of the symbol.
    /// In two-phase mode, returns 0 without writing if no symbols were reserved.
    pub fn write_null_symbol(&mut self) -> u64 {
        if !M::need_offset(self.symtab_offset) {
            return 0;
        }
        let offset = self.write_align(self.encoder.address_size());
        M::set_offset(&mut self.symtab_offset, offset);
        M::set_section_name(&mut self.symtab_str_id, &mut self.shstrtab, b".symtab");

        self.encoder.null_symbol(&mut self.buffer);
        if M::need_offset(self.symtab_shndx_offset) {
            self.encoder.u32(&mut self.symtab_shndx_data, 0u32);
        }

        self.written_symtab_num = 1;
        M::set_count(&mut self.symtab_num, self.written_symtab_num);
        // The symtab must link to a strtab.
        self.need_strtab = true;
        offset
    }

    /// Write a symbol.
    ///
    /// Must be called after [`Self::write_null_symbol`].
    pub fn write_symbol(&mut self, sym: &Sym) -> SymbolIndex {
        let section = self.encoder.symbol(&mut self.buffer, sym);
        if M::need_offset(self.symtab_shndx_offset) {
            let section_index = if let Some(section) = section {
                self.need_symtab_shndx = true;
                section
            } else {
                0
            };
            self.encoder.u32(&mut self.symtab_shndx_data, section_index);
        }

        let index = SymbolIndex(self.written_symtab_num);
        self.written_symtab_num += 1;
        M::set_count(&mut self.symtab_num, self.written_symtab_num);
        index
    }

    /// Write the section header for the symbol table.
    ///
    /// Only writes the header if the section index was reserved in two-phase mode,
    /// or [`Self::write_null_symbol`] was called in single-phase mode.
    pub fn write_symtab_section_header(&mut self, num_local: u32) {
        if self.symtab_str_id.is_none() {
            return;
        }
        let index = self.write_section_header(&SectionHeader {
            sh_name: self.section_name_offset(self.symtab_str_id),
            sh_offset: self.symtab_offset,
            sh_size: self.symtab_num as u64 * self.encoder.sym_size(),
            ..self
                .encoder
                .symtab_section_header(self.strtab_index.0, num_local)
        });
        M::set_section_index(&mut self.symtab_index.0, index);
    }

    /// Write the extended section indices for the symbol table.
    ///
    /// Returns the file offset of the start of the data.
    /// Returns 0 without writing if extended section indices are not needed.
    pub fn write_symtab_shndx(&mut self) -> u64 {
        if !self.need_symtab_shndx {
            self.symtab_shndx_data = Vec::new();
            return 0;
        }
        let offset = self.write_align(ALIGN_SYMTAB_SHNDX);
        M::set_offset(&mut self.symtab_shndx_offset, offset);
        M::set_section_name(
            &mut self.symtab_shndx_str_id,
            &mut self.shstrtab,
            b".symtab_shndx",
        );
        self.buffer.write_bytes(&self.symtab_shndx_data);
        self.symtab_shndx_data = Vec::new();
        offset
    }

    /// Write the section header for the extended section indices for the symbol table.
    ///
    /// Only writes the header if the section index was reserved in two-phase mode,
    /// or [`Self::write_symtab_shndx`] was called in single-phase mode.
    pub fn write_symtab_shndx_section_header(&mut self) {
        if self.symtab_shndx_str_id.is_none() {
            return;
        }
        let sh_size = if self.symtab_shndx_offset == 0 {
            0
        } else {
            (self.symtab_num * 4) as u64
        };
        self.write_section_header(&SectionHeader {
            sh_name: self.section_name_offset(self.symtab_shndx_str_id),
            sh_offset: self.symtab_shndx_offset,
            sh_size,
            ..self
                .encoder
                .symtab_shndx_section_header(self.symtab_index.0)
        });
    }

    /// Add a string to the dynamic string table.
    ///
    /// This will be stored in the `.dynstr` section.
    ///
    /// Must be called before [`Self::reserve_dynstr`] in two-phase mode,
    /// or before [`Self::write_dynstr`] in single-phase mode.
    pub fn add_dynamic_string(&mut self, name: &'a [u8]) -> StringId {
        debug_assert_eq!(self.dynstr_offset, 0);
        self.dynstr.add(name)
    }

    /// Get a string that was previously added to the dynamic string table.
    ///
    /// Panics if the string was not added.
    pub fn get_dynamic_string(&self, name: &'a [u8]) -> StringId {
        self.dynstr.get_id(name)
    }

    /// Return the offset of a string in the dynamic string table.
    ///
    /// Returns 0 if `id` is `None`. Pairs with [`Self::add_dynamic_string`].
    ///
    /// In two-phase mode, must be called after [`Self::reserve_dynstr`].
    pub fn dynamic_string_offset(&self, id: Option<StringId>) -> u32 {
        id.map_or(0, |id| self.dynstr.get_offset(id))
    }

    /// Write the section header for the dynamic string table.
    ///
    /// Only writes the header if the section index was reserved in two-phase mode,
    /// or [`Self::write_dynstr`] wrote the string table in single-phase mode.
    pub fn write_dynstr_section_header(&mut self, sh_addr: u64) {
        if self.dynstr_str_id.is_none() {
            return;
        }
        let index = self.write_section_header(&SectionHeader {
            sh_name: self.section_name_offset(self.dynstr_str_id),
            sh_addr,
            sh_offset: self.dynstr_offset,
            sh_size: self.dynstr_size.into(),
            ..self.encoder.dynstr_section_header()
        });
        M::set_section_index(&mut self.dynstr_index.0, index);
    }

    /// Write the null dynamic symbol.
    ///
    /// This must be the first dynamic symbol that is written.
    ///
    /// Returns the file offset of the symbol.
    /// In two-phase mode, returns 0 without writing if no dynamic symbols were reserved.
    pub fn write_null_dynamic_symbol(&mut self) -> u64 {
        if !M::need_offset(self.dynsym_offset) {
            return 0;
        }
        let offset = self.write_align(self.encoder.address_size());
        M::set_offset(&mut self.dynsym_offset, offset);
        M::set_section_name(&mut self.dynsym_str_id, &mut self.shstrtab, b".dynsym");

        self.encoder.null_symbol(&mut self.buffer);

        self.written_dynsym_num = 1;
        M::set_count(&mut self.dynsym_num, self.written_dynsym_num);
        // The symbol table must link to a string table.
        self.need_dynstr = true;
        offset
    }

    /// Write a dynamic symbol.
    ///
    /// Must be called after [`Self::write_null_dynamic_symbol`].
    pub fn write_dynamic_symbol(&mut self, sym: &Sym) -> SymbolIndex {
        // TODO: we don't write out .dynsym_shndx yet.
        // This is unlikely to be needed though.
        let _ = self.encoder.symbol(&mut self.buffer, sym);

        let index = SymbolIndex(self.written_dynsym_num);
        self.written_dynsym_num += 1;
        M::set_count(&mut self.dynsym_num, self.written_dynsym_num);
        index
    }

    /// Write the section header for the dynamic symbol table.
    ///
    /// Only writes the header if the section index was reserved in two-phase mode,
    /// or [`Self::write_null_dynamic_symbol`] was called in single-phase mode.
    pub fn write_dynsym_section_header(&mut self, sh_addr: u64, num_local: u32) {
        if self.dynsym_str_id.is_none() {
            return;
        }
        let index = self.write_section_header(&SectionHeader {
            sh_name: self.section_name_offset(self.dynsym_str_id),
            sh_addr,
            sh_offset: self.dynsym_offset,
            sh_size: self.dynsym_num as u64 * self.encoder.sym_size(),
            ..self
                .encoder
                .dynsym_section_header(self.dynstr_index.0, num_local)
        });
        M::set_section_index(&mut self.dynsym_index.0, index);
    }

    /// Write alignment padding bytes prior to the `.dynamic` section.
    ///
    /// Returns the file offset after the padding.
    /// In two-phase mode, returns 0 without writing if the section was not reserved.
    pub fn write_align_dynamic(&mut self) -> u64 {
        if !M::need_offset(self.dynamic_offset) {
            return 0;
        }
        let offset = self.write_align(self.encoder.address_size());
        M::set_offset(&mut self.dynamic_offset, offset);
        M::set_section_name(&mut self.dynamic_str_id, &mut self.shstrtab, b".dynamic");
        offset
    }

    /// Write a dynamic string entry.
    ///
    /// Must be called after [`Self::write_align_dynamic`].
    pub fn write_dynamic_string(&mut self, tag: elf::DynamicTag, id: StringId) -> Result<()> {
        self.write_dynamic(tag, self.dynstr.get_offset(id).into())
    }

    /// Write a dynamic value entry.
    ///
    /// Must be called after [`Self::write_align_dynamic`].
    pub fn write_dynamic(&mut self, d_tag: elf::DynamicTag, d_val: u64) -> Result<()> {
        self.encoder.dynamic(&mut self.buffer, d_tag, d_val)?;
        self.written_dynamic_num += 1;
        M::set_count(&mut self.dynamic_num, self.written_dynamic_num);
        Ok(())
    }

    /// Write the section header for the dynamic table.
    ///
    /// Only writes the header if the section index was reserved in two-phase mode,
    /// or [`Self::write_align_dynamic`] was called in single-phase mode.
    pub fn write_dynamic_section_header(&mut self, sh_addr: u64) {
        if self.dynamic_str_id.is_none() {
            return;
        }
        self.write_section_header(&SectionHeader {
            sh_name: self.section_name_offset(self.dynamic_str_id),
            sh_addr,
            sh_offset: self.dynamic_offset,
            sh_size: self.dynamic_num * self.encoder.dyn_size(),
            ..self.encoder.dynamic_section_header(self.dynstr_index.0)
        });
    }

    /// Write a SysV hash section.
    ///
    /// `symbol_count` is the number of symbols in the hash.
    /// The argument to `hash` will be in the range `0..symbol_count`.
    ///
    /// In two-phase mode, [`Self::reserve_hash`] must be called before this.
    ///
    /// Returns the file offset of the hash table data.
    pub fn write_hash<F>(&mut self, bucket_count: u32, symbol_count: u32, hash: F) -> u64
    where
        F: Fn(u32) -> Option<u32>,
    {
        let offset = self.write_align(ALIGN_HASH);
        M::set_offset(&mut self.hash_offset, offset);
        M::set_section_name(&mut self.hash_str_id, &mut self.shstrtab, b".hash");
        self.encoder
            .hash_table(&mut self.buffer, bucket_count, symbol_count, hash);
        let size = self.offset() - self.hash_offset;
        M::set_size(&mut self.hash_size, size);
        offset
    }

    /// Write the section header for the SysV hash table.
    ///
    /// Only writes the header if the section index was reserved in two-phase mode,
    /// or [`Self::write_hash`] was called in single-phase mode.
    pub fn write_hash_section_header(&mut self, sh_addr: u64) {
        if self.hash_str_id.is_none() {
            return;
        }
        self.write_section_header(&SectionHeader {
            sh_name: self.section_name_offset(self.hash_str_id),
            sh_addr,
            sh_offset: self.hash_offset,
            sh_size: self.hash_size,
            ..self.encoder.hash_section_header(self.dynsym_index.0)
        });
    }

    /// Write a GNU hash section.
    ///
    /// `symbol_count` is the number of symbols in the hash.
    /// The argument to `hash` will be in the range `0..symbol_count`.
    ///
    /// This requires that symbols are already sorted by bucket.
    ///
    /// In two-phase mode, [`Self::reserve_gnu_hash`] must be called before this.
    ///
    /// Returns the file offset of the hash table data.
    pub fn write_gnu_hash<F>(
        &mut self,
        symbol_base: u32,
        bloom_shift: u32,
        bloom_count: u32,
        bucket_count: u32,
        symbol_count: u32,
        hash: F,
    ) -> u64
    where
        F: Fn(u32) -> u32,
    {
        let offset = self.write_align(self.encoder.address_size());
        M::set_offset(&mut self.gnu_hash_offset, offset);
        M::set_section_name(&mut self.gnu_hash_str_id, &mut self.shstrtab, b".gnu.hash");

        self.encoder.gnu_hash_table(
            &mut self.buffer,
            &GnuHashTable {
                bloom_shift,
                bloom_count,
                bucket_count,
                symbol_base,
                symbol_count,
            },
            hash,
        );

        let size = self.offset() - self.gnu_hash_offset;
        M::set_size(&mut self.gnu_hash_size, size);
        offset
    }

    /// Write the section header for the GNU hash table.
    ///
    /// Only writes the header if the section index was reserved in two-phase mode,
    /// or [`Self::write_gnu_hash`] was called in single-phase mode.
    pub fn write_gnu_hash_section_header(&mut self, sh_addr: u64) {
        if self.gnu_hash_str_id.is_none() {
            return;
        }
        self.write_section_header(&SectionHeader {
            sh_name: self.section_name_offset(self.gnu_hash_str_id),
            sh_addr,
            sh_offset: self.gnu_hash_offset,
            sh_size: self.gnu_hash_size,
            ..self.encoder.gnu_hash_section_header(self.dynsym_index.0)
        });
    }

    /// Write the null symbol version entry.
    ///
    /// This must be the first symbol version that is written.
    ///
    /// Returns the file offset of the entry.
    /// In two-phase mode, returns 0 without writing if no dynamic symbols were reserved.
    pub fn write_null_gnu_versym(&mut self) -> u64 {
        if !M::need_offset(self.gnu_versym_offset) {
            return 0;
        }
        let offset = self.write_align(ALIGN_GNU_VERSYM);
        M::set_offset(&mut self.gnu_versym_offset, offset);
        M::set_section_name(
            &mut self.gnu_versym_str_id,
            &mut self.shstrtab,
            b".gnu.version",
        );
        self.write_gnu_versym(elf::VER_NDX_LOCAL.into());
        offset
    }

    /// Write a symbol version entry.
    ///
    /// Must be called after [`Self::write_null_gnu_versym`].
    pub fn write_gnu_versym(&mut self, versym: elf::VersymIndex) {
        self.encoder.gnu_versym(&mut self.buffer, versym);
        self.written_versym_num += 1;
        M::set_count(&mut self.gnu_versym_num, self.written_versym_num);
    }

    /// Write the section header for the `.gnu.version` section.
    ///
    /// Only writes the header if the section index was reserved in two-phase mode,
    /// or [`Self::write_null_gnu_versym`] was called in single-phase mode.
    pub fn write_gnu_versym_section_header(&mut self, sh_addr: u64) {
        if self.gnu_versym_str_id.is_none() {
            return;
        }
        self.write_section_header(&SectionHeader {
            sh_name: self.section_name_offset(self.gnu_versym_str_id),
            sh_addr,
            sh_offset: self.gnu_versym_offset,
            sh_size: self.encoder.gnu_versym_size(self.gnu_versym_num),
            ..self.encoder.gnu_versym_section_header(self.dynsym_index.0)
        });
    }

    /// Write alignment padding bytes prior to a `.gnu.version_d` section.
    ///
    /// Returns the file offset after the padding.
    /// In two-phase mode, returns 0 without writing if the section was not reserved.
    fn write_align_gnu_verdef_impl(&mut self) -> u64 {
        if !M::need_offset(self.gnu_verdef_offset) {
            return 0;
        }
        let offset = self.write_align(ALIGN_GNU_VERDEF);
        M::set_offset(&mut self.gnu_verdef_offset, offset);
        M::set_section_name(
            &mut self.gnu_verdef_str_id,
            &mut self.shstrtab,
            b".gnu.version_d",
        );
        offset
    }

    /// Write a version definition entry.
    ///
    /// Must be called after [`Self::write_align_gnu_verdef`].
    pub fn write_gnu_verdef(&mut self, verdef: &Verdef) {
        debug_assert_ne!(self.gnu_verdef_remaining, 0);
        self.gnu_verdef_remaining -= 1;

        debug_assert_ne!(verdef.aux_count, 0);
        self.gnu_verdaux_remaining = verdef.aux_count;
        self.written_verdaux_count += usize::from(verdef.aux_count);
        M::set_count(&mut self.gnu_verdaux_count, self.written_verdaux_count);

        self.encoder
            .gnu_verdef(&mut self.buffer, self.gnu_verdef_remaining != 0, verdef);
    }

    /// Write a version definition entry that shares the names of the next definition.
    ///
    /// This is typically useful when there are only two versions (including the base)
    /// and they have the same name.
    ///
    /// Must be called after [`Self::write_align_gnu_verdef`].
    pub fn write_gnu_verdef_shared(&mut self, verdef: &Verdef) {
        debug_assert_ne!(self.gnu_verdef_remaining, 0);
        self.gnu_verdef_remaining -= 1;
        debug_assert_ne!(self.gnu_verdef_remaining, 0);

        debug_assert_ne!(verdef.aux_count, 0);
        self.gnu_verdaux_remaining = 0;

        self.encoder.gnu_verdef_shared(&mut self.buffer, verdef);
    }

    /// Write a version definition auxiliary entry.
    ///
    /// Must be called inside a version definition started by [`Self::write_gnu_verdef`].
    pub fn write_gnu_verdaux(&mut self, name: StringId) {
        debug_assert_ne!(self.gnu_verdaux_remaining, 0);
        self.gnu_verdaux_remaining -= 1;
        self.encoder.gnu_verdaux(
            &mut self.buffer,
            self.gnu_verdaux_remaining != 0,
            self.dynstr.get_offset(name),
        );
    }

    /// Write the section header for the `.gnu.version_d` section.
    ///
    /// Only writes the header if the section index was reserved in two-phase mode,
    /// or [`Self::write_align_gnu_verdef`] was called in single-phase mode.
    pub fn write_gnu_verdef_section_header(&mut self, sh_addr: u64) {
        if self.gnu_verdef_str_id.is_none() {
            return;
        }
        let sh_size = self
            .encoder
            .gnu_verdef_size(self.gnu_verdef_count, self.gnu_verdaux_count);
        self.write_section_header(&SectionHeader {
            sh_name: self.section_name_offset(self.gnu_verdef_str_id),
            sh_addr,
            sh_offset: self.gnu_verdef_offset,
            sh_size,
            ..self
                .encoder
                .gnu_verdef_section_header(self.dynstr_index.0, self.gnu_verdef_count)
        });
    }

    /// Write alignment padding bytes prior to a `.gnu.version_r` section.
    ///
    /// Returns the file offset after the padding.
    /// In two-phase mode, returns 0 without writing if the section was not reserved.
    fn write_align_gnu_verneed_impl(&mut self) -> u64 {
        if !M::need_offset(self.gnu_verneed_offset) {
            return 0;
        }
        let offset = self.write_align(ALIGN_GNU_VERNEED);
        M::set_offset(&mut self.gnu_verneed_offset, offset);
        M::set_section_name(
            &mut self.gnu_verneed_str_id,
            &mut self.shstrtab,
            b".gnu.version_r",
        );
        offset
    }

    /// Write a version needed entry.
    ///
    /// Must be called after [`Self::write_align_gnu_verneed`].
    pub fn write_gnu_verneed(&mut self, verneed: &Verneed) {
        debug_assert_ne!(self.gnu_verneed_remaining, 0);
        self.gnu_verneed_remaining -= 1;

        if verneed.aux_count != 0 {
            self.gnu_vernaux_remaining = verneed.aux_count;
            self.written_vernaux_count += usize::from(verneed.aux_count);
            M::set_count(&mut self.gnu_vernaux_count, self.written_vernaux_count);
        };

        self.encoder
            .gnu_verneed(&mut self.buffer, self.gnu_verneed_remaining != 0, verneed);
    }

    /// Write a version needed auxiliary entry.
    ///
    /// Must be called inside a version needed entry started by [`Self::write_gnu_verneed`].
    pub fn write_gnu_vernaux(&mut self, vernaux: &Vernaux) {
        debug_assert_ne!(self.gnu_vernaux_remaining, 0);
        self.gnu_vernaux_remaining -= 1;
        self.encoder
            .gnu_vernaux(&mut self.buffer, self.gnu_vernaux_remaining != 0, vernaux);
    }

    /// Write the section header for the `.gnu.version_r` section.
    ///
    /// Only writes the header if the section index was reserved in two-phase mode,
    /// or [`Self::write_align_gnu_verneed`] was called in single-phase mode.
    pub fn write_gnu_verneed_section_header(&mut self, sh_addr: u64) {
        if self.gnu_verneed_str_id.is_none() {
            return;
        }
        let sh_size = self
            .encoder
            .gnu_verneed_size(self.gnu_verneed_count, self.gnu_vernaux_count);
        self.write_section_header(&SectionHeader {
            sh_name: self.section_name_offset(self.gnu_verneed_str_id),
            sh_addr,
            sh_offset: self.gnu_verneed_offset,
            sh_size,
            ..self
                .encoder
                .gnu_verneed_section_header(self.dynstr_index.0, self.gnu_verneed_count)
        });
    }

    /// Write the section header for the `.gnu.attributes` section.
    ///
    /// Only writes the header if the section index was reserved in two-phase mode,
    /// or [`Self::write_gnu_attributes`] was called in single-phase mode.
    pub fn write_gnu_attributes_section_header(&mut self) {
        if self.gnu_attributes_str_id.is_none() {
            return;
        }
        self.write_section_header(&SectionHeader {
            sh_name: self.section_name_offset(self.gnu_attributes_str_id),
            sh_offset: self.gnu_attributes_offset,
            sh_size: self.gnu_attributes_size,
            sh_link: self.dynstr_index.0,
            ..self.encoder.gnu_attributes_section_header()
        });
    }

    /// Write the data for the `.gnu.attributes` section.
    ///
    /// Returns the file offset of the data.
    /// In two-phase mode, returns 0 without writing if the section was not reserved.
    pub fn write_gnu_attributes(&mut self, data: &[u8]) -> u64 {
        if !M::need_offset(self.gnu_attributes_offset) {
            return 0;
        }
        let offset = self.offset();
        M::set_offset(&mut self.gnu_attributes_offset, offset);
        M::set_section_name(
            &mut self.gnu_attributes_str_id,
            &mut self.shstrtab,
            b".gnu.attributes",
        );
        self.buffer.write_bytes(data);
        let size = self.offset() - self.gnu_attributes_offset;
        M::set_size(&mut self.gnu_attributes_size, size);
        offset
    }

    /// Write alignment padding bytes prior to a relocation section.
    ///
    /// Returns the file offset after the padding.
    pub fn write_align_relocation(&mut self) -> u64 {
        self.write_align(self.encoder.address_size())
    }

    /// Write a relocation.
    ///
    /// [`Self::write_align_relocation`] should be called before the first relocation
    /// of a section. In two-phase mode, the file range must have been reserved with
    /// [`Self::reserve_relocations`].
    pub fn write_relocation(&mut self, is_rela: bool, rel: &Rel) {
        self.encoder.relocation(&mut self.buffer, is_rela, rel);
    }

    /// Write the section header for a relocation section.
    ///
    /// `section` is the index of the section the relocations apply to,
    /// or 0 if none.
    ///
    /// `symtab` is the index of the symbol table the relocations refer to,
    /// or 0 if none.
    ///
    /// `offset` is the file offset of the relocations.
    pub fn write_relocation_section_header(
        &mut self,
        name: StringId,
        section: SectionIndex,
        symtab: SectionIndex,
        offset: u64,
        count: usize,
        is_rela: bool,
    ) {
        self.write_section_header(&SectionHeader {
            sh_name: self.section_name_offset(Some(name)),
            sh_offset: offset,
            sh_size: count as u64 * self.encoder.rel_size(is_rela),
            sh_link: symtab.0,
            sh_info: section.0,
            ..self.encoder.relocation_section_header(is_rela)
        });
    }

    /// Write the section header for a relative relocation section.
    ///
    /// `offset` is the file offset of the relocations.
    /// `size` is the size of the section in bytes.
    pub fn write_relative_relocation_section_header(
        &mut self,
        name: StringId,
        offset: u64,
        size: u64,
    ) {
        self.write_section_header(&SectionHeader {
            sh_name: self.section_name_offset(Some(name)),
            sh_offset: offset,
            sh_size: size,
            ..self.encoder.relative_relocation_section_header()
        });
    }

    /// Write `GRP_COMDAT` at the start of the COMDAT section.
    pub fn write_comdat_header(&mut self) {
        self.buffer.write_align(4);
        self.encoder.u32(&mut self.buffer, elf::GRP_COMDAT);
    }

    /// Write an entry in a COMDAT section.
    pub fn write_comdat_entry(&mut self, entry: SectionIndex) {
        self.encoder.u32(&mut self.buffer, entry.0);
    }

    /// Write the section header for a COMDAT section.
    pub fn write_comdat_section_header(
        &mut self,
        name: StringId,
        symtab: SectionIndex,
        symbol: SymbolIndex,
        offset: u64,
        count: usize,
    ) {
        self.write_section_header(&SectionHeader {
            sh_name: self.section_name_offset(Some(name)),
            sh_offset: offset,
            sh_size: ((count + 1) * 4) as u64,
            sh_link: symtab.0,
            sh_info: symbol.0,
            ..self.encoder.comdat_section_header()
        });
    }

    /// Return a helper for writing an attributes section.
    pub fn attributes_writer(&self) -> AttributesWriter {
        AttributesWriter::new(self.encoder.endian())
    }
}

/// Single-phase writer for ELF files.
///
/// See [`SinglePhase`].
pub type SinglePhaseWriter<'a> = Writer<'a, SinglePhase>;

/// Single-phase writing mode for [`Writer`].
///
/// Construct a writer with this mode using [`Writer::new_single_phase`].
///
/// There is no reservation phase: items are written directly and their file offsets are
/// discovered as they are written. The `reserve_*` methods are not available in this mode.
///
/// Items must be written before they are referenced:
/// - write section data before section headers
/// - write metadata section headers before they are referenced (e.g. `.strtab` before `.symtab`)
///
/// Strings in strings tables are written in the order they are added, without suffix merging.
/// This means that string offsets are known before the string table is written.
/// For example, [`Writer::write_symbol`] may be called before [`Writer::write_strtab`].
///
/// The one exception is the file header, which must be written first, but needs to
/// reference the program header table and section header table. To support this, you can
/// call [`Writer::write_file_header`] to write placeholders, then after everything
/// is written call [`Writer::write_file_header_to`] using a new buffer, and manually copy
/// the header to the start.
///
/// The `Writer` will assign section indices to metadata sections as the headers are
/// written. However, for text/data sections you will need to manually determine
/// the section indices ahead of time if you need to reference sections from
/// symbols. Similarly, for symbol indices if you need to reference symbols from
/// relocations.
#[derive(Debug)]
pub struct SinglePhase(());

impl Mode for SinglePhase {}
#[allow(private_interfaces)]
impl ModeSealed for SinglePhase {
    fn string_table() -> StringTable<'static> {
        StringTable::new_in_order(1)
    }

    fn reserve(
        &self,
        _buffer: &mut dyn WritableBuffer,
        _encoder: Encoder<Endianness>,
    ) -> Result<()> {
        Ok(())
    }

    fn need_offset(offset: u64) -> bool {
        debug_assert_eq!(offset, 0);
        true
    }

    fn set_offset(dest: &mut u64, offset: u64) {
        debug_assert_eq!(*dest, 0);
        *dest = offset;
    }

    fn set_size(dest: &mut u64, size: u64) {
        debug_assert_eq!(*dest, 0);
        *dest = size;
    }

    fn set_section_index(dest: &mut u32, index: SectionIndex) {
        debug_assert_eq!(*dest, 0);
        *dest = index.0;
    }

    fn set_section_name(
        dest: &mut Option<StringId>,
        shstrtab: &mut StringTable<'_>,
        name: &'static [u8],
    ) {
        debug_assert!(dest.is_none());
        *dest = Some(shstrtab.add(name));
    }

    fn set_count<T: Copy + PartialOrd>(dest: &mut T, count: T) {
        debug_assert!(*dest < count);
        *dest = count;
    }
}

impl<'a> Writer<'a, SinglePhase> {
    /// Create a new single-phase `Writer` for the given endianness and ELF class.
    pub fn new_single_phase(
        endian: Endianness,
        is_64: bool,
        buffer: &'a mut dyn GrowableBuffer,
    ) -> Self {
        Writer::new_with_mode(endian, is_64, buffer.as_writable(), SinglePhase(()))
    }

    /// Write the file header to the given buffer.
    ///
    /// This is needed to write fields such as the section header offset into the file
    /// header. You must manually copy the resulting `buffer` contents to the start of the
    /// original buffer after dropping the `Writer`.
    ///
    /// This does not call [`WritableBuffer::reserve`].
    pub fn write_file_header_to(&mut self, buffer: &mut dyn WritableBuffer) -> Result<()> {
        let len = self.offset();
        if !self.encoder.is_64() && len > u64::from(u32::MAX) {
            return Err(Error(format!("File size overflow: {len:#x}")));
        }
        if self.layout.section_num >= elf::SHN_LORESERVE.into() {
            // The null section header was written before section_num was known,
            // so it won't contain the overflow value.
            return Err(Error(String::from(
                "single phase write doesn't support e_shnum overflow",
            )));
        }
        self.encoder.file_header(buffer, &self.header, &self.layout)
    }

    /// Write a placeholder for the program header table immediately after the file header.
    ///
    /// Must be called immediately after [`Self::write_file_header`] and before any
    /// other data is written. This pads the buffer by `count` program header entries.
    /// Use [`Self::write_headers_to`] to write the program headers once their contents
    /// are known.
    ///
    /// Returns the file offset and size of the program headers.
    /// Returns `(0, 0)` if `count` is zero.
    pub fn write_program_headers_placeholder(&mut self, count: u32) -> (u64, u64) {
        if count == 0 {
            return (0, 0);
        }
        // file_header_size is always a multiple of address size, so no alignment
        // padding is required.
        let offset = self.offset();
        debug_assert_eq!(offset, self.encoder.file_header_size());
        SinglePhase::set_offset(&mut self.layout.e_phoff, offset);
        SinglePhase::set_count(&mut self.layout.segment_num, count);
        self.written_segment_num = count;
        let size = u64::from(count) * self.encoder.program_header_size();
        self.buffer.write_zeros(size);
        (offset, size)
    }

    /// Write the file header and program headers to the given buffer.
    ///
    /// This serves the same purpose as [`Self::write_file_header_to`], but additionally
    /// writes the program headers immediately following the file header.  You must
    /// manually copy the resulting `buffer` contents to the start of the original buffer
    /// after dropping the `Writer`.
    ///
    /// The number of program headers must match the count passed to
    /// [`Self::write_program_headers_placeholder`].
    ///
    /// This does not call [`WritableBuffer::reserve`].
    pub fn write_headers_to(
        &mut self,
        buffer: &mut dyn WritableBuffer,
        program_headers: &[ProgramHeader],
    ) -> Result<()> {
        debug_assert_eq!(program_headers.len(), self.layout.segment_num as usize);
        self.write_file_header_to(buffer)?;
        for header in program_headers {
            self.encoder.program_header(buffer, header);
        }
        Ok(())
    }

    /// Write a program header to the given buffer.
    ///
    /// This serves the same purpose as [`Self::write_headers_to`], but allows you to use
    /// separate calls for writing the file header and each program header.
    ///
    /// This does not call [`WritableBuffer::reserve`].
    pub fn write_program_header_to(
        &mut self,
        buffer: &mut dyn WritableBuffer,
        header: &ProgramHeader,
    ) {
        self.encoder.program_header(buffer, header);
    }

    /// Write the section header string table.
    ///
    /// Always writes the string table, even if it is empty.
    ///
    /// Returns the file offset and size of the data.
    pub fn write_shstrtab(&mut self) -> Result<(u64, u32)> {
        // This must be written before the .shstrtab section header, so
        // we can't use write_null_section_header to determine if it is needed.
        // Thus we always write this if called.
        debug_assert_eq!(self.shstrtab_offset, 0);
        self.shstrtab_str_id = Some(self.add_section_name(b".shstrtab"));
        self.shstrtab_offset = self.offset();
        self.shstrtab_size = self.encoder.strtab(&mut self.buffer, &mut self.shstrtab)?;
        Ok((self.shstrtab_offset, self.shstrtab_size))
    }

    /// Write the string table.
    ///
    /// Returns the file offset and size of the data.
    /// Returns `(0, 0)` without writing anything if the string table is empty and
    /// there is no symbol table.
    pub fn write_strtab(&mut self) -> Result<(u64, u32)> {
        if !self.need_strtab && self.strtab.is_empty() {
            return Ok((0, 0));
        }
        debug_assert_eq!(self.strtab_offset, 0);
        self.strtab_str_id = Some(self.add_section_name(b".strtab"));
        self.strtab_offset = self.offset();
        self.strtab_size = self.encoder.strtab(&mut self.buffer, &mut self.strtab)?;
        Ok((self.strtab_offset, self.strtab_size))
    }

    /// Write the dynamic string table.
    ///
    /// Returns the file offset and size of the data.
    /// Returns `(0, 0)` without writing anything if the string table is empty and
    /// there is no dynamic symbol table.
    pub fn write_dynstr(&mut self) -> Result<(u64, u32)> {
        if !self.need_dynstr && self.dynstr.is_empty() {
            return Ok((0, 0));
        }
        debug_assert_eq!(self.dynstr_offset, 0);
        self.dynstr_str_id = Some(self.add_section_name(b".dynstr"));
        self.dynstr_offset = self.offset();
        self.dynstr_size = self.encoder.strtab(&mut self.buffer, &mut self.dynstr)?;
        Ok((self.dynstr_offset, self.dynstr_size))
    }

    /// Write alignment padding bytes prior to a `.gnu.version_d` section.
    ///
    /// Also sets the number of definition entries that will be written.
    ///
    /// Returns the file offset after the padding.
    /// Returns 0 without writing if count is zero.
    pub fn write_align_gnu_verdef(&mut self, count: u16) -> u64 {
        if count == 0 {
            return 0;
        }
        self.gnu_verdef_count = count;
        self.gnu_verdef_remaining = count;
        self.write_align_gnu_verdef_impl()
    }

    /// Write alignment padding bytes prior to a `.gnu.version_r` section.
    ///
    /// Also sets the number of version needed entries that will be written.
    ///
    /// Returns the file offset after the padding.
    /// Returns 0 without writing if count is zero.
    pub fn write_align_gnu_verneed(&mut self, count: u16) -> u64 {
        if count == 0 {
            return 0;
        }
        self.gnu_verneed_count = count;
        self.gnu_verneed_remaining = count;
        self.write_align_gnu_verneed_impl()
    }
}

/// Two-phase writing mode for [`Writer`].
///
/// Construct a writer with this mode using [`Writer::new`].
///
/// File ranges and indices are reserved up front with the `reserve_*` methods,
/// then everything is written in the same order. This is the default mode.
///
/// The first phase uses the `reserve_*` methods to build up all of the information
/// that may need to be known ahead of time:
/// - string table offsets
/// - section indices
/// - symbol indices
/// - file ranges for headers and sections
///
/// Some of the information has ordering requirements. For example, strings must be added
/// to string tables before reserving the file range for the string table. Symbol indices
/// must be reserved after reserving the section indices they reference. There are debug
/// asserts to check some of these requirements.
///
/// The second phase writes everything out in order. Thus the caller must ensure writing
/// is in the same order that file ranges were reserved. There are debug asserts to assist
/// with checking this.
#[derive(Debug)]
pub struct TwoPhase {
    len: u64,
    shstrtab_data: Vec<u8>,
    strtab_data: Vec<u8>,
    dynstr_data: Vec<u8>,
}

impl Mode for TwoPhase {}
#[allow(private_interfaces)]
impl ModeSealed for TwoPhase {
    fn string_table() -> StringTable<'static> {
        StringTable::new()
    }

    fn reserve(&self, buffer: &mut dyn WritableBuffer, encoder: Encoder<Endianness>) -> Result<()> {
        let len = self.len;
        if !encoder.is_64() && len > u64::from(u32::MAX) {
            return Err(Error(format!("File size overflow: {len:#x}")));
        }
        buffer
            .reserve(len)
            .map_err(|_| Error(String::from("Cannot allocate buffer")))
    }

    fn need_offset(offset: u64) -> bool {
        offset != 0
    }

    fn set_offset(dest: &mut u64, offset: u64) {
        debug_assert_eq!(*dest, offset);
    }

    fn set_size(dest: &mut u64, size: u64) {
        debug_assert_eq!(*dest, size);
    }

    fn set_section_index(dest: &mut u32, index: SectionIndex) {
        debug_assert_eq!(*dest, index.0);
    }

    fn set_section_name(
        dest: &mut Option<StringId>,
        shstrtab: &mut StringTable<'_>,
        name: &'static [u8],
    ) {
        debug_assert_eq!(*dest, Some(shstrtab.get_id(name)));
    }

    fn set_count<T: Copy + PartialOrd>(dest: &mut T, count: T) {
        debug_assert!(*dest >= count);
    }
}

impl<'a> Writer<'a, TwoPhase> {
    /// Create a new two-phase `Writer` for the given endianness and ELF class.
    ///
    /// [`Writer::write_file_header`] will call [`WritableBuffer::reserve`] with the
    /// total file size.
    pub fn new(endian: Endianness, is_64: bool, buffer: &'a mut dyn WritableBuffer) -> Self {
        let mode = TwoPhase {
            len: 0,
            shstrtab_data: Vec::new(),
            strtab_data: Vec::new(),
            dynstr_data: Vec::new(),
        };
        Writer::new_with_mode(endian, is_64, buffer, mode)
    }

    /// Return the current file length that has been reserved.
    pub fn reserved_len(&self) -> u64 {
        self.mode.len
    }

    /// Reserve a file range with the given size and starting alignment.
    ///
    /// Returns the aligned offset of the start of the range.
    ///
    /// `align_start` must be a power of two.
    pub fn reserve(&mut self, len: u64, align_start: u64) -> u64 {
        if align_start > 1 {
            self.mode.len = util::align(self.mode.len, align_start);
        }
        let offset = self.mode.len;
        self.mode.len += len;
        offset
    }

    /// Reserve the file range up to the given file offset.
    pub fn reserve_until(&mut self, offset: u64) {
        debug_assert!(self.mode.len <= offset);
        self.mode.len = offset;
    }

    /// Reserve the range for the file header.
    ///
    /// This must be at the start of the file.
    pub fn reserve_file_header(&mut self) {
        debug_assert_eq!(self.mode.len, 0);
        self.reserve(self.encoder.file_header_size(), 1);
    }

    /// Reserve the range for the program headers.
    ///
    /// If `num` is >= `elf::PN_XNUM`, you must also reserve and write
    /// the section table; this is checked in `write_file_header`.
    ///
    /// Does nothing if `num` is zero.
    pub fn reserve_program_headers(&mut self, num: u32) {
        debug_assert_eq!(self.layout.e_phoff, 0);
        if num == 0 {
            return;
        }
        self.layout.segment_num = num;
        self.layout.e_phoff = self.reserve(
            num as u64 * self.encoder.program_header_size(),
            self.encoder.address_size(),
        );
    }

    /// Reserve the section index for the null section header.
    ///
    /// The null section header is usually automatically reserved,
    /// but this can be used to force an empty section table.
    ///
    /// Must be called before [`Self::reserve_section_index`] and
    /// [`Self::reserve_section_headers`].
    pub fn reserve_null_section_index(&mut self) -> SectionIndex {
        debug_assert_eq!(self.layout.section_num, 0);
        if self.layout.section_num == 0 {
            self.layout.section_num = 1;
        }
        SectionIndex(0)
    }

    /// Reserve a section table index.
    ///
    /// Automatically also reserves the null section header if required.
    ///
    /// Must be called before [`Self::reserve_section_headers`].
    pub fn reserve_section_index(&mut self) -> SectionIndex {
        debug_assert_eq!(self.layout.e_shoff, 0);
        if self.layout.section_num == 0 {
            self.layout.section_num = 1;
        }
        let index = self.layout.section_num;
        self.layout.section_num += 1;
        SectionIndex(index)
    }

    /// Reserve the range for the section headers.
    ///
    /// Does nothing if no sections were reserved.
    /// Must be called after [`Self::reserve_section_index`]
    /// and other functions that reserve section indices.
    pub fn reserve_section_headers(&mut self) {
        debug_assert_eq!(self.layout.e_shoff, 0);
        if self.layout.section_num == 0 {
            return;
        }
        self.layout.e_shoff = self.reserve(
            self.layout.section_num as u64 * self.encoder.section_header_size(),
            self.encoder.address_size(),
        );
    }

    /// Reserve the range for the section header string table.
    ///
    /// This range is used for a section named `.shstrtab`.
    ///
    /// Does nothing if no sections were reserved.
    /// Must be called after [`Self::add_section_name`]
    /// and other functions that add section names and reserve indices.
    ///
    /// Returns an error if the string table could not be finalized.
    pub fn reserve_shstrtab(&mut self) -> Result<()> {
        debug_assert_eq!(self.shstrtab_offset, 0);
        if self.layout.section_num == 0 {
            return Ok(());
        }
        self.shstrtab_size = self
            .encoder
            .strtab(&mut self.mode.shstrtab_data, &mut self.shstrtab)?;
        self.shstrtab_offset = self.reserve(self.shstrtab_size as u64, 1);
        Ok(())
    }

    /// Write the section header string table.
    ///
    /// Does nothing if the section was not reserved.
    pub fn write_shstrtab(&mut self) {
        if self.shstrtab_offset == 0 {
            return;
        }
        debug_assert_eq!(self.shstrtab_offset, self.offset());
        self.buffer.write_bytes(&self.mode.shstrtab_data);
    }

    /// Reserve the section index for the section header string table.
    ///
    /// Must be called before [`Self::reserve_shstrtab`]
    /// and [`Self::reserve_section_headers`].
    pub fn reserve_shstrtab_section_index(&mut self) -> SectionIndex {
        debug_assert_eq!(self.layout.shstrtab_index, 0);
        self.shstrtab_str_id = Some(self.add_section_name(b".shstrtab"));
        self.layout.shstrtab_index = self.reserve_section_index().0;
        SectionIndex(self.layout.shstrtab_index)
    }

    /// Return true if `.strtab` is needed.
    pub fn strtab_needed(&self) -> bool {
        self.need_strtab || !self.strtab.is_empty()
    }

    /// Require the string table even if no strings were added.
    pub fn require_strtab(&mut self) {
        self.need_strtab = true;
    }

    /// Reserve the range for the string table.
    ///
    /// This range is used for a section named `.strtab`.
    ///
    /// The range is only reserved if the string table is needed (either `require_strtab`
    /// was called, or at least one string or symbol was added).
    /// Must be called after [`Self::add_string`].
    ///
    /// Returns an error if the string table could not be finalized.
    pub fn reserve_strtab(&mut self) -> Result<()> {
        debug_assert_eq!(self.strtab_offset, 0);
        if !self.strtab_needed() {
            return Ok(());
        }
        self.strtab_size = self
            .encoder
            .strtab(&mut self.mode.strtab_data, &mut self.strtab)?;
        self.strtab_offset = self.reserve(self.strtab_size as u64, 1);
        Ok(())
    }

    /// Write the string table.
    ///
    /// Does nothing if the section was not reserved.
    pub fn write_strtab(&mut self) {
        if self.strtab_offset == 0 {
            return;
        }
        debug_assert_eq!(self.strtab_offset, self.offset());
        self.buffer.write_bytes(&self.mode.strtab_data);
    }

    /// Reserve the section index for the string table.
    ///
    /// You should check [`Self::strtab_needed`] before calling this
    /// unless you have other means of knowing if this section is needed.
    ///
    /// Must be called before [`Self::reserve_section_headers`].
    pub fn reserve_strtab_section_index(&mut self) -> SectionIndex {
        debug_assert_eq!(self.strtab_index, SectionIndex(0));
        self.strtab_str_id = Some(self.add_section_name(b".strtab"));
        self.strtab_index = self.reserve_section_index();
        self.strtab_index
    }

    /// Reserve the null symbol table entry.
    ///
    /// This will be stored in the `.symtab` section.
    ///
    /// The null symbol table entry is usually automatically reserved,
    /// but this can be used to force an empty symbol table.
    ///
    /// Must be called before [`Self::reserve_symbol_index`] and
    /// [`Self::reserve_symtab`].
    pub fn reserve_null_symbol_index(&mut self) -> SymbolIndex {
        debug_assert_eq!(self.symtab_offset, 0);
        debug_assert_eq!(self.symtab_num, 0);
        self.symtab_num = 1;
        // The symtab must link to a strtab.
        self.need_strtab = true;
        SymbolIndex(0)
    }

    /// Reserve a symbol table entry.
    ///
    /// This will be stored in the `.symtab` section.
    ///
    /// `section_index` is used to determine whether `.symtab_shndx` is required.
    ///
    /// Automatically also reserves the null symbol if required.
    /// Callers may assume that the returned indices will be sequential
    /// starting at 1.
    ///
    /// Must be called before [`Self::reserve_symtab`] and
    /// [`Self::reserve_symtab_shndx`].
    pub fn reserve_symbol_index(&mut self, section_index: Option<SectionIndex>) -> SymbolIndex {
        debug_assert_eq!(self.symtab_offset, 0);
        debug_assert_eq!(self.symtab_shndx_offset, 0);
        if self.symtab_num == 0 {
            self.symtab_num = 1;
            // The symtab must link to a strtab.
            self.need_strtab = true;
        }
        let index = self.symtab_num;
        self.symtab_num += 1;
        if let Some(section_index) = section_index {
            if section_index.0 >= elf::SHN_LORESERVE.into() {
                self.need_symtab_shndx = true;
            }
        }
        SymbolIndex(index)
    }

    /// Return the number of reserved symbol table entries.
    ///
    /// Includes the null symbol.
    pub fn symbol_count(&self) -> u32 {
        self.symtab_num
    }

    /// Reserve the range for the symbol table.
    ///
    /// This range is used for a section named `.symtab`.
    /// Does nothing if no symbols were reserved.
    /// Must be called after [`Self::reserve_symbol_index`].
    pub fn reserve_symtab(&mut self) {
        debug_assert_eq!(self.symtab_offset, 0);
        if self.symtab_num == 0 {
            return;
        }
        self.symtab_offset = self.reserve(
            self.symtab_num as u64 * self.encoder.sym_size(),
            self.encoder.address_size(),
        );
    }

    /// Reserve the section index for the symbol table.
    ///
    /// Must be called before [`Self::reserve_section_headers`].
    pub fn reserve_symtab_section_index(&mut self) -> SectionIndex {
        debug_assert_eq!(self.symtab_index, SectionIndex(0));
        self.symtab_str_id = Some(self.add_section_name(b".symtab"));
        self.symtab_index = self.reserve_section_index();
        self.symtab_index
    }

    /// Return the section index of the symbol table.
    pub fn symtab_index(&mut self) -> SectionIndex {
        self.symtab_index
    }

    /// Return true if `.symtab_shndx` is needed.
    pub fn symtab_shndx_needed(&self) -> bool {
        self.need_symtab_shndx
    }

    /// Require the extended section indices for the symbol table even
    /// if no section indices are too large.
    pub fn require_symtab_shndx(&mut self) {
        self.need_symtab_shndx = true;
    }

    /// Reserve the range for the extended section indices for the symbol table.
    ///
    /// This range is used for a section named `.symtab_shndx`.
    /// This also reserves a section index.
    ///
    /// Does nothing if extended section indices are not needed.
    /// Must be called after [`Self::reserve_symbol_index`].
    pub fn reserve_symtab_shndx(&mut self) {
        debug_assert_eq!(self.symtab_shndx_offset, 0);
        if !self.symtab_shndx_needed() {
            return;
        }
        self.symtab_shndx_offset = self.reserve(self.symtab_num as u64 * 4, ALIGN_SYMTAB_SHNDX);
        self.symtab_shndx_data.reserve(self.symtab_num as usize * 4);
    }

    /// Reserve the section index for the extended section indices symbol table.
    ///
    /// You should check [`Self::symtab_shndx_needed`] before calling this
    /// unless you have other means of knowing if this section is needed.
    ///
    /// Must be called before [`Self::reserve_section_headers`].
    pub fn reserve_symtab_shndx_section_index(&mut self) -> SectionIndex {
        debug_assert!(self.symtab_shndx_str_id.is_none());
        self.symtab_shndx_str_id = Some(self.add_section_name(b".symtab_shndx"));
        self.reserve_section_index()
    }

    /// Return true if `.dynstr` is needed.
    pub fn dynstr_needed(&self) -> bool {
        self.need_dynstr || !self.dynstr.is_empty()
    }

    /// Require the dynamic string table even if no strings were added.
    pub fn require_dynstr(&mut self) {
        self.need_dynstr = true;
    }

    /// Reserve the range for the dynamic string table.
    ///
    /// This range is used for a section named `.dynstr`.
    ///
    /// The range is only reserved if the string table is needed (either `require_dynstr`
    /// was called, or dynamic symbols or dynamic strings are present).
    /// Must be called after [`Self::add_dynamic_string`].
    ///
    /// Returns the file offset of the data.
    /// Returns 0 if the range was not reserved.
    /// Returns an error if the string table could not be finalized.
    pub fn reserve_dynstr(&mut self) -> Result<u64> {
        debug_assert_eq!(self.dynstr_offset, 0);
        if !self.dynstr_needed() {
            return Ok(0);
        }
        self.dynstr_size = self
            .encoder
            .strtab(&mut self.mode.dynstr_data, &mut self.dynstr)?;
        self.dynstr_offset = self.reserve(self.dynstr_size as u64, 1);
        Ok(self.dynstr_offset)
    }

    /// Return the size of the dynamic string table.
    ///
    /// Must be called after [`Self::reserve_dynstr`].
    /// Returns 0 if the dynamic string table is not needed.
    pub fn dynstr_len(&self) -> u32 {
        if !self.dynstr_needed() {
            return 0;
        }
        debug_assert_ne!(self.dynstr_offset, 0);
        self.dynstr_size
    }

    /// Write the dynamic string table.
    ///
    /// Does nothing if the section was not reserved.
    pub fn write_dynstr(&mut self) {
        if self.dynstr_offset == 0 {
            return;
        }
        debug_assert_eq!(self.dynstr_offset, self.offset());
        self.buffer.write_bytes(&self.mode.dynstr_data);
    }

    /// Reserve the section index for the dynamic string table.
    ///
    /// You should check [`Self::dynstr_needed`] before calling this
    /// unless you have other means of knowing if this section is needed.
    ///
    /// Must be called before [`Self::reserve_section_headers`].
    pub fn reserve_dynstr_section_index(&mut self) -> SectionIndex {
        debug_assert_eq!(self.dynstr_index, SectionIndex(0));
        self.dynstr_str_id = Some(self.add_section_name(b".dynstr"));
        self.dynstr_index = self.reserve_section_index();
        self.dynstr_index
    }

    /// Return the section index of the dynamic string table.
    pub fn dynstr_index(&mut self) -> SectionIndex {
        self.dynstr_index
    }

    /// Reserve the null dynamic symbol table entry.
    ///
    /// This will be stored in the `.dynsym` section.
    ///
    /// The null dynamic symbol table entry is usually automatically reserved,
    /// but this can be used to force an empty dynamic symbol table.
    ///
    /// Must be called before [`Self::reserve_dynamic_symbol_index`] and
    /// [`Self::reserve_dynsym`].
    pub fn reserve_null_dynamic_symbol_index(&mut self) -> SymbolIndex {
        debug_assert_eq!(self.dynsym_offset, 0);
        debug_assert_eq!(self.dynsym_num, 0);
        self.dynsym_num = 1;
        // The symbol table must link to a string table.
        self.need_dynstr = true;
        SymbolIndex(0)
    }

    /// Reserve a dynamic symbol table entry.
    ///
    /// This will be stored in the `.dynsym` section.
    ///
    /// Automatically also reserves the null symbol if required.
    /// Callers may assume that the returned indices will be sequential
    /// starting at 1.
    ///
    /// Must be called before [`Self::reserve_dynsym`].
    pub fn reserve_dynamic_symbol_index(&mut self) -> SymbolIndex {
        debug_assert_eq!(self.dynsym_offset, 0);
        if self.dynsym_num == 0 {
            self.dynsym_num = 1;
            // The symbol table must link to a string table.
            self.need_dynstr = true;
        }
        let index = self.dynsym_num;
        self.dynsym_num += 1;
        SymbolIndex(index)
    }

    /// Return the number of reserved dynamic symbols.
    ///
    /// Includes the null symbol.
    pub fn dynamic_symbol_count(&mut self) -> u32 {
        self.dynsym_num
    }

    /// Reserve the range for the dynamic symbol table.
    ///
    /// This range is used for a section named `.dynsym`.
    ///
    /// Must be called after [`Self::reserve_dynamic_symbol_index`].
    ///
    /// Returns the file offset of the symbol table.
    /// Returns 0 if no dynamic symbols were reserved.
    pub fn reserve_dynsym(&mut self) -> u64 {
        debug_assert_eq!(self.dynsym_offset, 0);
        if self.dynsym_num == 0 {
            return 0;
        }
        self.dynsym_offset = self.reserve(
            self.dynsym_num as u64 * self.encoder.sym_size(),
            self.encoder.address_size(),
        );
        self.dynsym_offset
    }

    /// Reserve the section index for the dynamic symbol table.
    ///
    /// Must be called before [`Self::reserve_section_headers`].
    pub fn reserve_dynsym_section_index(&mut self) -> SectionIndex {
        debug_assert_eq!(self.dynsym_index, SectionIndex(0));
        self.dynsym_str_id = Some(self.add_section_name(b".dynsym"));
        self.dynsym_index = self.reserve_section_index();
        self.dynsym_index
    }

    /// Return the section index of the dynamic symbol table.
    pub fn dynsym_index(&mut self) -> SectionIndex {
        self.dynsym_index
    }

    /// Reserve the range for the `.dynamic` section.
    ///
    /// Returns the file offset of the reserved range.
    /// Returns 0 if `dynamic_num` is zero.
    pub fn reserve_dynamic(&mut self, dynamic_num: usize) -> u64 {
        debug_assert_eq!(self.dynamic_offset, 0);
        if dynamic_num == 0 {
            return 0;
        }
        self.dynamic_num = dynamic_num as u64;
        self.dynamic_offset = self.reserve(
            self.dynamic_num * self.encoder.dyn_size(),
            self.encoder.address_size(),
        );
        self.dynamic_offset
    }

    /// Reserve the section index for the dynamic table.
    ///
    /// Must be called before [`Self::reserve_section_headers`].
    pub fn reserve_dynamic_section_index(&mut self) -> SectionIndex {
        debug_assert!(self.dynamic_str_id.is_none());
        self.dynamic_str_id = Some(self.add_section_name(&b".dynamic"[..]));
        self.reserve_section_index()
    }

    /// Reserve a file range for a SysV hash section.
    ///
    /// `symbol_count` is the number of symbols in the hash,
    /// not the total number of symbols.
    pub fn reserve_hash(&mut self, bucket_count: u32, symbol_count: u32) -> u64 {
        let size = self.encoder.hash_size(bucket_count, symbol_count);
        self.hash_offset = self.reserve(size, ALIGN_HASH);
        self.hash_size = size;
        self.hash_offset
    }

    /// Reserve the section index for the SysV hash table.
    ///
    /// Must be called before [`Self::reserve_section_headers`].
    pub fn reserve_hash_section_index(&mut self) -> SectionIndex {
        debug_assert!(self.hash_str_id.is_none());
        self.hash_str_id = Some(self.add_section_name(b".hash"));
        self.reserve_section_index()
    }

    /// Reserve a file range for a GNU hash section.
    ///
    /// `symbol_count` is the number of symbols in the hash,
    /// not the total number of symbols.
    pub fn reserve_gnu_hash(
        &mut self,
        bloom_count: u32,
        bucket_count: u32,
        symbol_count: u32,
    ) -> u64 {
        let size = self
            .encoder
            .gnu_hash_size(bloom_count, bucket_count, symbol_count);
        self.gnu_hash_offset = self.reserve(size, self.encoder.address_size());
        self.gnu_hash_size = size;
        self.gnu_hash_offset
    }

    /// Reserve the section index for the GNU hash table.
    ///
    /// Must be called before [`Self::reserve_section_headers`].
    pub fn reserve_gnu_hash_section_index(&mut self) -> SectionIndex {
        debug_assert!(self.gnu_hash_str_id.is_none());
        self.gnu_hash_str_id = Some(self.add_section_name(b".gnu.hash"));
        self.reserve_section_index()
    }

    /// Reserve the range for the `.gnu.version` section.
    ///
    /// Returns the file offset of the reserved range.
    /// Returns 0 if no dynamic symbols were reserved.
    pub fn reserve_gnu_versym(&mut self) -> u64 {
        debug_assert_eq!(self.gnu_versym_offset, 0);
        if self.dynsym_num == 0 {
            return 0;
        }
        self.gnu_versym_num = self.dynsym_num;
        let size = self.encoder.gnu_versym_size(self.gnu_versym_num);
        self.gnu_versym_offset = self.reserve(size, ALIGN_GNU_VERSYM);
        self.gnu_versym_offset
    }

    /// Reserve the section index for the `.gnu.version` section.
    ///
    /// Must be called before [`Self::reserve_section_headers`].
    pub fn reserve_gnu_versym_section_index(&mut self) -> SectionIndex {
        debug_assert!(self.gnu_versym_str_id.is_none());
        self.gnu_versym_str_id = Some(self.add_section_name(b".gnu.version"));
        self.reserve_section_index()
    }

    /// Reserve the range for the `.gnu.version_d` section.
    ///
    /// Returns the file offset of the reserved range.
    /// Returns 0 if `verdef_count` is zero.
    pub fn reserve_gnu_verdef(&mut self, verdef_count: u16, verdaux_count: usize) -> u64 {
        debug_assert_eq!(self.gnu_verdef_offset, 0);
        if verdef_count == 0 {
            return 0;
        }
        let size = self.encoder.gnu_verdef_size(verdef_count, verdaux_count);
        self.gnu_verdef_offset = self.reserve(size, ALIGN_GNU_VERDEF);
        self.gnu_verdef_count = verdef_count;
        self.gnu_verdaux_count = verdaux_count;
        self.gnu_verdef_remaining = self.gnu_verdef_count;
        self.gnu_verdef_offset
    }

    /// Write alignment padding bytes prior to a `.gnu.version_d` section.
    ///
    /// Does nothing if the section was not reserved with [`Self::reserve_gnu_verdef`].
    pub fn write_align_gnu_verdef(&mut self) {
        self.write_align_gnu_verdef_impl();
    }

    /// Reserve the section index for the `.gnu.version_d` section.
    ///
    /// Must be called before [`Self::reserve_section_headers`].
    pub fn reserve_gnu_verdef_section_index(&mut self) -> SectionIndex {
        debug_assert!(self.gnu_verdef_str_id.is_none());
        self.gnu_verdef_str_id = Some(self.add_section_name(b".gnu.version_d"));
        self.reserve_section_index()
    }

    /// Reserve the range for the `.gnu.version_r` section.
    ///
    /// Returns the file offset of the reserved range.
    /// Returns 0 if `verneed_count` is zero.
    pub fn reserve_gnu_verneed(&mut self, verneed_count: u16, vernaux_count: usize) -> u64 {
        debug_assert_eq!(self.gnu_verneed_offset, 0);
        if verneed_count == 0 {
            return 0;
        }
        let size = self.encoder.gnu_verneed_size(verneed_count, vernaux_count);
        self.gnu_verneed_offset = self.reserve(size, ALIGN_GNU_VERNEED);
        self.gnu_verneed_count = verneed_count;
        self.gnu_vernaux_count = vernaux_count;
        self.gnu_verneed_remaining = self.gnu_verneed_count;
        self.gnu_verneed_offset
    }

    /// Write alignment padding bytes prior to a `.gnu.version_r` section.
    ///
    /// Does nothing if the section was not reserved with [`Self::reserve_gnu_verneed`].
    pub fn write_align_gnu_verneed(&mut self) {
        self.write_align_gnu_verneed_impl();
    }

    /// Reserve the section index for the `.gnu.version_r` section.
    ///
    /// Must be called before [`Self::reserve_section_headers`].
    pub fn reserve_gnu_verneed_section_index(&mut self) -> SectionIndex {
        debug_assert!(self.gnu_verneed_str_id.is_none());
        self.gnu_verneed_str_id = Some(self.add_section_name(b".gnu.version_r"));
        self.reserve_section_index()
    }

    /// Reserve the section index for the `.gnu.attributes` section.
    ///
    /// Must be called before [`Self::reserve_section_headers`].
    pub fn reserve_gnu_attributes_section_index(&mut self) -> SectionIndex {
        debug_assert!(self.gnu_attributes_str_id.is_none());
        self.gnu_attributes_str_id = Some(self.add_section_name(b".gnu.attributes"));
        self.reserve_section_index()
    }

    /// Reserve the range for the `.gnu.attributes` section.
    ///
    /// Returns the file offset of the reserved range.
    /// Returns 0 if `gnu_attributes_size` is zero.
    pub fn reserve_gnu_attributes(&mut self, gnu_attributes_size: u64) -> u64 {
        debug_assert_eq!(self.gnu_attributes_offset, 0);
        if gnu_attributes_size == 0 {
            return 0;
        }
        self.gnu_attributes_size = gnu_attributes_size;
        self.gnu_attributes_offset = self.reserve(gnu_attributes_size, 1);
        self.gnu_attributes_offset
    }

    /// Reserve a file range for the given number of relocations.
    ///
    /// Returns the offset of the range.
    pub fn reserve_relocations(&mut self, count: usize, is_rela: bool) -> u64 {
        self.reserve(
            count as u64 * self.encoder.rel_size(is_rela),
            self.encoder.address_size(),
        )
    }

    /// Reserve a file range for a COMDAT section.
    ///
    /// `count` is the number of sections in the COMDAT group.
    ///
    /// Returns the offset of the range.
    pub fn reserve_comdat(&mut self, count: usize) -> u64 {
        self.reserve((count as u64 + 1) * 4, 4)
    }
}
