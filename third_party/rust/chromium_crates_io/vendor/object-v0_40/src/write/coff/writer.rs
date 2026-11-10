//! Helper for writing COFF files.
use alloc::string::String;
use alloc::vec::Vec;
use core::mem;

use crate::pe;
use crate::write::string::{StringId, StringTable};
use crate::write::util;
use crate::write::{CountingBuffer, Error, Result, WritableBuffer, WritableBufferExt};

/// A helper for writing COFF files.
///
/// Writing uses a two phase approach. The first phase builds up all of the information
/// that may need to be known ahead of time:
/// - build string table
/// - reserve section indices
/// - reserve symbol indices
/// - reserve file ranges for headers and sections
///
/// Some of the information has ordering requirements. For example, strings must be added
/// to the string table before reserving the file range for the string table. There are debug
/// asserts to check some of these requirements.
///
/// The second phase writes everything out in order. Thus the caller must ensure writing
/// is in the same order that file ranges were reserved. There are debug asserts to assist
/// with checking this.
#[allow(missing_debug_implementations)]
pub struct Writer<'a> {
    buffer: CountingBuffer<&'a mut dyn WritableBuffer>,
    len: u64,

    section_num: u16,

    symtab_offset: u32,
    symtab_num: u32,

    strtab: StringTable<'a>,
    strtab_len: u32,
    strtab_offset: u32,
    strtab_data: Vec<u8>,
}

impl<'a> Writer<'a> {
    /// Create a new `Writer`.
    ///
    /// [`Writer::write_file_header`] will call [`WritableBuffer::reserve`] with the
    /// total file size.
    pub fn new(buffer: &'a mut dyn WritableBuffer) -> Self {
        Writer {
            buffer: CountingBuffer::new(buffer),
            len: 0,

            section_num: 0,

            symtab_offset: 0,
            symtab_num: 0,

            strtab: StringTable::default(),
            strtab_len: 0,
            strtab_offset: 0,
            strtab_data: Vec::new(),
        }
    }

    /// Return the current file length that has been reserved.
    pub fn reserved_len(&self) -> u64 {
        self.len
    }

    /// Return the current file length that has been written.
    #[allow(clippy::len_without_is_empty)]
    pub fn len(&self) -> u64 {
        self.buffer.count()
    }

    /// Reserve a file range with the given size and starting alignment.
    ///
    /// Returns the aligned offset of the start of the range.
    ///
    /// `align_start` must be a power of two.
    pub fn reserve(&mut self, len: u64, align_start: u64) -> u32 {
        if align_start > 1 {
            self.len = util::align(self.len, align_start);
        }
        let offset = self.len;
        self.len += len;
        offset as u32
    }

    /// Write alignment padding bytes.
    pub fn write_align(&mut self, align_start: u64) {
        if align_start > 1 {
            self.buffer.write_align(align_start);
        }
    }

    /// Write data.
    pub fn write(&mut self, data: &[u8]) {
        self.buffer.write_bytes(data);
    }

    /// Reserve the file range up to the given file offset.
    pub fn reserve_until(&mut self, offset: u64) {
        debug_assert!(self.len <= offset);
        self.len = offset;
    }

    /// Write padding up to the given file offset.
    pub fn pad_until(&mut self, offset: u64) {
        debug_assert!(self.buffer.count() <= offset);
        self.buffer.resize(offset);
    }

    /// Reserve the range for the file header.
    ///
    /// This must be at the start of the file.
    pub fn reserve_file_header(&mut self) {
        debug_assert_eq!(self.len, 0);
        self.reserve(mem::size_of::<pe::ImageFileHeader>() as u64, 1);
    }

    /// Write the file header.
    ///
    /// This must be at the start of the file.
    ///
    /// Fields that can be derived from known information are automatically set by this function.
    ///
    /// This calls [`WritableBuffer::reserve`] with the total file size.
    pub fn write_file_header(&mut self, header: FileHeader) -> Result<()> {
        debug_assert_eq!(self.buffer.count(), 0);

        // Start writing.
        self.buffer
            .reserve(self.len)
            .map_err(|_| Error(String::from("Cannot allocate buffer")))?;

        // Write file header.
        let header = pe::ImageFileHeader {
            machine: header.machine.into(),
            number_of_sections: self.section_num.into(),
            time_date_stamp: header.time_date_stamp.into(),
            pointer_to_symbol_table: self.symtab_offset.into(),
            number_of_symbols: self.symtab_num.into(),
            size_of_optional_header: 0.into(),
            characteristics: header.characteristics.into(),
        };
        self.buffer.write_pod(&header);

        Ok(())
    }

    /// Reserve the range for the section headers.
    pub fn reserve_section_headers(&mut self, section_num: u16) {
        debug_assert_eq!(self.section_num, 0);
        self.section_num = section_num;
        self.reserve(
            section_num as u64 * mem::size_of::<pe::ImageSectionHeader>() as u64,
            1,
        );
    }

    /// Write a section header.
    pub fn write_section_header(&mut self, section: SectionHeader) {
        let mut coff_section = pe::ImageSectionHeader {
            name: [0; 8],
            virtual_size: 0.into(),
            virtual_address: 0.into(),
            size_of_raw_data: section.size_of_raw_data.into(),
            pointer_to_raw_data: section.pointer_to_raw_data.into(),
            pointer_to_relocations: section.pointer_to_relocations.into(),
            pointer_to_linenumbers: section.pointer_to_linenumbers.into(),
            number_of_relocations: if section.number_of_relocations > 0xffff {
                0xffff.into()
            } else {
                (section.number_of_relocations as u16).into()
            },
            number_of_linenumbers: 0.into(),
            characteristics: section.characteristics.into(),
        };
        match section.name {
            Name::Short(name) => coff_section.name = name,
            Name::Long(str_id) => {
                let mut str_offset = self.strtab.get_offset(str_id);
                if str_offset <= 9_999_999 {
                    let mut name = [0; 7];
                    let mut len = 0;
                    if str_offset == 0 {
                        name[6] = b'0';
                        len = 1;
                    } else {
                        while str_offset != 0 {
                            let rem = (str_offset % 10) as u8;
                            str_offset /= 10;
                            name[6 - len] = b'0' + rem;
                            len += 1;
                        }
                    }
                    coff_section.name = [0; 8];
                    coff_section.name[0] = b'/';
                    coff_section.name[1..][..len].copy_from_slice(&name[7 - len..]);
                } else {
                    coff_section.name[0] = b'/';
                    coff_section.name[1] = b'/';
                    for i in 0..6 {
                        let rem = (str_offset % 64) as u8;
                        str_offset /= 64;
                        let c = match rem {
                            0..=25 => b'A' + rem,
                            26..=51 => b'a' + rem - 26,
                            52..=61 => b'0' + rem - 52,
                            62 => b'+',
                            63 => b'/',
                            _ => unreachable!(),
                        };
                        coff_section.name[7 - i] = c;
                    }
                }
            }
        }
        self.buffer.write_pod(&coff_section);
    }

    /// Reserve the range for the section data.
    ///
    /// Returns the aligned offset of the start of the range.
    /// Does nothing and returns 0 if the length is zero.
    pub fn reserve_section(&mut self, len: usize) -> u32 {
        if len == 0 {
            return 0;
        }
        // TODO: not sure what alignment is required here, but this seems to match LLVM
        self.reserve(len as u64, 4)
    }

    /// Write the alignment bytes prior to section data.
    ///
    /// This is unneeded if you are using `write_section` or `write_section_zeroes`
    /// for the data.
    pub fn write_section_align(&mut self) {
        self.buffer.write_align(4);
    }

    /// Write the section data.
    ///
    /// Writes alignment bytes prior to the data.
    /// Does nothing if the data is empty.
    pub fn write_section(&mut self, data: &[u8]) {
        if data.is_empty() {
            return;
        }
        self.write_section_align();
        self.buffer.write_bytes(data);
    }

    /// Write the section data using zero bytes.
    ///
    /// Writes alignment bytes prior to the data.
    /// Does nothing if the length is zero.
    pub fn write_section_zeroes(&mut self, len: usize) {
        if len == 0 {
            return;
        }
        self.write_section_align();
        self.buffer.write_zeros(len as u64);
    }

    /// Reserve a file range for the given number of relocations.
    ///
    /// This will automatically reserve an extra relocation if there are more than 0xffff.
    ///
    /// Returns the offset of the range.
    /// Does nothing and returns 0 if the count is zero.
    pub fn reserve_relocations(&mut self, mut count: usize) -> u32 {
        if count == 0 {
            return 0;
        }
        if count > 0xffff {
            count += 1;
        }
        self.reserve(
            count as u64 * mem::size_of::<pe::ImageRelocation>() as u64,
            1,
        )
    }

    /// Write a relocation containing the count if required.
    ///
    /// This should be called before writing the first relocation for a section.
    pub fn write_relocations_count(&mut self, count: usize) {
        if count > 0xffff {
            let coff_relocation = pe::ImageRelocation {
                virtual_address: (count as u32 + 1).into(),
                symbol_table_index: 0.into(),
                typ: pe::RelocationType(0).into(),
            };
            self.buffer.write_pod(&coff_relocation);
        }
    }

    /// Write a relocation.
    pub fn write_relocation(&mut self, reloc: Relocation) {
        let coff_relocation = pe::ImageRelocation {
            virtual_address: reloc.virtual_address.into(),
            symbol_table_index: reloc.symbol.into(),
            typ: reloc.typ.into(),
        };
        self.buffer.write_pod(&coff_relocation);
    }

    /// Reserve a symbol table entry.
    ///
    /// This must be called before [`Self::reserve_symtab_strtab`].
    pub fn reserve_symbol_index(&mut self) -> u32 {
        debug_assert_eq!(self.symtab_offset, 0);
        let index = self.symtab_num;
        self.symtab_num += 1;
        index
    }

    /// Reserve a number of symbol table entries.
    pub fn reserve_symbol_indices(&mut self, count: u32) {
        debug_assert_eq!(self.symtab_offset, 0);
        self.symtab_num += count;
    }

    /// Write a symbol table entry.
    pub fn write_symbol(&mut self, symbol: Symbol) {
        let mut coff_symbol = pe::ImageSymbol {
            name: [0; 8],
            value: symbol.value.into(),
            section_number: (symbol.section_number.0 as u16).into(),
            typ: symbol.typ.into(),
            storage_class: symbol.storage_class,
            number_of_aux_symbols: symbol.number_of_aux_symbols,
        };
        match symbol.name {
            Name::Short(name) => coff_symbol.name = name,
            Name::Long(str_id) => {
                let str_offset = self.strtab.get_offset(str_id);
                coff_symbol.name[4..8].copy_from_slice(&u32::to_le_bytes(str_offset));
            }
        }
        self.buffer.write_pod(&coff_symbol);
    }

    /// Reserve auxiliary symbols for a file name.
    ///
    /// Returns the number of auxiliary symbols required.
    ///
    /// This must be called before [`Self::reserve_symtab_strtab`].
    pub fn reserve_aux_file_name(&mut self, name: &[u8]) -> u8 {
        debug_assert_eq!(self.symtab_offset, 0);
        let aux_count = name.len().div_ceil(pe::IMAGE_SIZEOF_SYMBOL);
        self.symtab_num += aux_count as u32;
        aux_count as u8
    }

    /// Write auxiliary symbols for a file name.
    pub fn write_aux_file_name(&mut self, name: &[u8], aux_count: u8) {
        let aux_len = aux_count as usize * pe::IMAGE_SIZEOF_SYMBOL;
        debug_assert!(aux_len >= name.len());
        let old_len = self.buffer.count();
        self.buffer.write_bytes(name);
        self.buffer.resize(old_len + aux_len as u64);
    }

    /// Reserve an auxiliary symbol for a section.
    ///
    /// Returns the number of auxiliary symbols required.
    ///
    /// This must be called before [`Self::reserve_symtab_strtab`].
    pub fn reserve_aux_section(&mut self) -> u8 {
        debug_assert_eq!(self.symtab_offset, 0);
        self.symtab_num += 1;
        1
    }

    /// Write an auxiliary symbol for a section.
    pub fn write_aux_section(&mut self, section: AuxSymbolSection) {
        let aux = pe::ImageAuxSymbolSection {
            length: section.length.into(),
            number_of_relocations: if section.number_of_relocations > 0xffff {
                0xffff.into()
            } else {
                (section.number_of_relocations as u16).into()
            },
            number_of_linenumbers: section.number_of_linenumbers.into(),
            check_sum: section.check_sum.into(),
            number: (section.number as u16).into(),
            selection: section.selection,
            reserved: 0,
            high_number: ((section.number >> 16) as u16).into(),
        };
        self.buffer.write_pod(&aux);
    }

    /// Reserve an auxiliary symbol for a weak external.
    ///
    /// Returns the number of auxiliary symbols required.
    ///
    /// This must be called before [`Self::reserve_symtab_strtab`].
    pub fn reserve_aux_weak_external(&mut self) -> u8 {
        debug_assert_eq!(self.symtab_offset, 0);
        self.symtab_num += 1;
        1
    }

    /// Write an auxiliary symbol for a weak external.
    pub fn write_aux_weak_external(&mut self, weak: AuxSymbolWeak) {
        let aux = pe::ImageAuxSymbolWeak {
            weak_default_sym_index: weak.weak_default_sym_index.into(),
            weak_search_type: weak.weak_search_type.into(),
        };
        self.buffer.write_pod(&aux);
        // write padding for the unused field
        const PAD_LEN: usize = pe::IMAGE_SIZEOF_SYMBOL - mem::size_of::<pe::ImageAuxSymbolWeak>();
        self.buffer.write_bytes(&[0u8; PAD_LEN]);
    }

    /// Return the number of reserved symbol table entries.
    pub fn symbol_count(&self) -> u32 {
        self.symtab_num
    }

    /// Add a string to the string table.
    ///
    /// This must be called before [`Self::reserve_symtab_strtab`].
    pub fn add_string(&mut self, name: &'a [u8]) -> StringId {
        debug_assert_eq!(self.strtab_offset, 0);
        self.strtab.add(name)
    }

    /// Add a section or symbol name to the string table if required.
    ///
    /// This must be called before [`Self::reserve_symtab_strtab`].
    pub fn add_name(&mut self, name: &'a [u8]) -> Name {
        if name.len() > 8 {
            Name::Long(self.add_string(name))
        } else {
            let mut short_name = [0; 8];
            short_name[..name.len()].copy_from_slice(name);
            Name::Short(short_name)
        }
    }

    /// Reserve the range for the symbol table and string table.
    ///
    /// This must be called after functions that reserve symbol
    /// indices or add strings.
    ///
    /// Returns an error if the string table could not be finalized.
    pub fn reserve_symtab_strtab(&mut self) -> Result<()> {
        debug_assert_eq!(self.symtab_offset, 0);
        self.symtab_offset =
            self.reserve(self.symtab_num as u64 * pe::IMAGE_SIZEOF_SYMBOL as u64, 1);

        debug_assert_eq!(self.strtab_offset, 0);
        // First 4 bytes of strtab are the length.
        self.strtab_len = self.strtab.write(&mut self.strtab_data, 4)?;
        self.strtab_offset = self.reserve(self.strtab_len as u64, 1);
        Ok(())
    }

    /// Write the string table.
    pub fn write_strtab(&mut self) {
        debug_assert_eq!(self.strtab_offset as u64, self.buffer.count());
        self.buffer.write_bytes(&u32::to_le_bytes(self.strtab_len));
        self.buffer.write_bytes(&self.strtab_data);
    }
}

/// Shortened and native endian version of [`pe::ImageFileHeader`].
#[allow(missing_docs)]
#[derive(Debug, Default, Clone)]
pub struct FileHeader {
    pub machine: pe::Machine,
    pub time_date_stamp: u32,
    pub characteristics: pe::FileFlags,
}

/// A section or symbol name.
#[derive(Debug, Clone, Copy)]
pub enum Name {
    /// An inline name.
    Short([u8; 8]),
    /// An id of a string table entry.
    Long(StringId),
}

impl Default for Name {
    fn default() -> Name {
        Name::Short([0; 8])
    }
}

// From isn't useful.
#[allow(clippy::from_over_into)]
impl<'a> Into<Name> for &'a [u8; 8] {
    fn into(self) -> Name {
        Name::Short(*self)
    }
}

/// Native endian version of [`pe::ImageSectionHeader`].
#[allow(missing_docs)]
#[derive(Debug, Default, Clone)]
pub struct SectionHeader {
    pub name: Name,
    pub size_of_raw_data: u32,
    pub pointer_to_raw_data: u32,
    pub pointer_to_relocations: u32,
    pub pointer_to_linenumbers: u32,
    /// This will automatically be clamped if there are more than 0xffff.
    pub number_of_relocations: u32,
    pub number_of_linenumbers: u16,
    pub characteristics: pe::SectionFlags,
}

/// Native endian version of [`pe::ImageSymbol`].
#[allow(missing_docs)]
#[derive(Debug, Default, Clone)]
pub struct Symbol {
    pub name: Name,
    pub value: u32,
    pub section_number: pe::SymbolSection,
    pub typ: pe::SymbolType,
    pub storage_class: pe::SymbolClass,
    pub number_of_aux_symbols: u8,
}

/// Native endian version of [`pe::ImageAuxSymbolSection`].
#[allow(missing_docs)]
#[derive(Debug, Default, Clone)]
pub struct AuxSymbolSection {
    pub length: u32,
    /// This will automatically be clamped if there are more than 0xffff.
    pub number_of_relocations: u32,
    pub number_of_linenumbers: u16,
    pub check_sum: u32,
    pub number: u32,
    pub selection: pe::ComdatSelection,
}

/// Native endian version of [`pe::ImageAuxSymbolWeak`].
#[allow(missing_docs)]
#[derive(Debug, Default, Clone)]
pub struct AuxSymbolWeak {
    pub weak_default_sym_index: u32,
    pub weak_search_type: pe::WeakExternSearch,
}

/// Native endian version of [`pe::ImageRelocation`].
#[allow(missing_docs)]
#[derive(Debug, Default, Clone)]
pub struct Relocation {
    pub virtual_address: u32,
    pub symbol: u32,
    pub typ: pe::RelocationType,
}
