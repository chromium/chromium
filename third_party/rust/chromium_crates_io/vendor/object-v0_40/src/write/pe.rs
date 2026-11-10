//! Helper for writing PE files.
use alloc::string::String;
use alloc::vec::Vec;
use core::mem;

use crate::endian::{LittleEndian as LE, *};
use crate::pe;
use crate::write::util;
use crate::write::{CountingBuffer, Error, Result, WritableBuffer, WritableBufferExt};

/// A helper for writing PE files.
///
/// Writing uses a two phase approach. The first phase reserves file ranges and virtual
/// address ranges for everything in the order that they will be written.
///
/// The second phase writes everything out in order. Thus the caller must ensure writing
/// is in the same order that file ranges were reserved.
#[allow(missing_debug_implementations)]
pub struct Writer<'a> {
    is_64: bool,
    section_alignment: u32,
    file_alignment: u32,

    buffer: CountingBuffer<&'a mut dyn WritableBuffer>,
    len: u64,
    virtual_len: u32,
    headers_len: u32,

    code_address: u32,
    data_address: u32,
    code_len: u32,
    data_len: u32,
    bss_len: u32,

    nt_headers_offset: u32,
    data_directories: Vec<DataDirectory>,
    section_header_num: u16,
    sections: Vec<Section>,

    symbol_offset: u32,
    symbol_num: u32,

    reloc_blocks: Vec<RelocBlock>,
    relocs: Vec<U16<LE>>,
    reloc_offset: u32,
}

impl<'a> Writer<'a> {
    /// Create a new `Writer`.
    ///
    /// The alignment values must be powers of two.
    ///
    /// Methods that write the DOS header will call [`WritableBuffer::reserve`] with the total
    /// file size.
    pub fn new(
        is_64: bool,
        section_alignment: u32,
        file_alignment: u32,
        buffer: &'a mut dyn WritableBuffer,
    ) -> Self {
        Writer {
            is_64,
            section_alignment,
            file_alignment,

            buffer: CountingBuffer::new(buffer),
            len: 0,
            virtual_len: 0,
            headers_len: 0,

            code_address: 0,
            data_address: 0,
            code_len: 0,
            data_len: 0,
            bss_len: 0,

            nt_headers_offset: 0,
            data_directories: Vec::new(),
            section_header_num: 0,
            sections: Vec::new(),

            symbol_offset: 0,
            symbol_num: 0,

            reloc_blocks: Vec::new(),
            relocs: Vec::new(),
            reloc_offset: 0,
        }
    }

    /// Return the current virtual address size that has been reserved.
    ///
    /// This is only valid after section headers have been reserved.
    pub fn virtual_len(&self) -> u32 {
        self.virtual_len
    }

    /// Reserve a virtual address range with the given size.
    ///
    /// The reserved length will be increased to match the section alignment.
    ///
    /// Returns the aligned offset of the start of the range.
    pub fn reserve_virtual(&mut self, len: u32) -> u32 {
        let offset = self.virtual_len;
        self.virtual_len += len;
        self.virtual_len = util::align_u32(self.virtual_len, self.section_alignment);
        offset
    }

    /// Reserve up to the given virtual address.
    ///
    /// The reserved length will be increased to match the section alignment.
    pub fn reserve_virtual_until(&mut self, address: u32) {
        debug_assert!(self.virtual_len <= address);
        self.virtual_len = util::align_u32(address, self.section_alignment);
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
    pub fn reserve(&mut self, len: u32, align_start: u32) -> u32 {
        if len == 0 {
            return self.len as u32;
        }
        self.reserve_align(align_start);
        let offset = self.len as u32;
        self.len += len as u64;
        offset
    }

    /// Reserve a file range with the given size and using the file alignment.
    ///
    /// Returns the aligned offset of the start of the range.
    pub fn reserve_file(&mut self, len: u32) -> u32 {
        self.reserve(len, self.file_alignment)
    }

    /// Write data.
    pub fn write(&mut self, data: &[u8]) {
        self.buffer.write_bytes(data);
    }

    /// Reserve alignment padding bytes.
    pub fn reserve_align(&mut self, align_start: u32) {
        self.len = util::align(self.len, align_start as u64);
    }

    /// Write alignment padding bytes.
    pub fn write_align(&mut self, align_start: u32) {
        self.buffer.write_align(align_start as u64);
    }

    /// Write padding up to the next multiple of file alignment.
    pub fn write_file_align(&mut self) {
        self.write_align(self.file_alignment);
    }

    /// Reserve the file range up to the given file offset.
    pub fn reserve_until(&mut self, offset: u32) {
        debug_assert!(self.len <= offset as u64);
        self.len = offset as u64;
    }

    /// Write padding up to the given file offset.
    pub fn pad_until(&mut self, offset: u32) {
        debug_assert!(self.buffer.count() <= offset as u64);
        self.buffer.resize(offset as u64);
    }

    /// Reserve the range for the DOS header.
    ///
    /// This must be at the start of the file.
    ///
    /// When writing, you may use `write_custom_dos_header` or `write_empty_dos_header`.
    pub fn reserve_dos_header(&mut self) {
        debug_assert_eq!(self.len, 0);
        self.reserve(mem::size_of::<pe::ImageDosHeader>() as u32, 1);
    }

    /// Write a custom DOS header.
    ///
    /// This must be at the start of the file.
    ///
    /// This calls [`WritableBuffer::reserve`] with the total file size.
    pub fn write_custom_dos_header(&mut self, dos_header: &pe::ImageDosHeader) -> Result<()> {
        debug_assert_eq!(self.buffer.count(), 0);

        // Start writing.
        self.buffer
            .reserve(self.len)
            .map_err(|_| Error(String::from("Cannot allocate buffer")))?;

        self.buffer.write_pod(dos_header);
        Ok(())
    }

    /// Write the DOS header for a file without a stub.
    ///
    /// This must be at the start of the file.
    ///
    /// Uses default values for all fields.
    ///
    /// This calls [`WritableBuffer::reserve`] with the total file size.
    pub fn write_empty_dos_header(&mut self) -> Result<()> {
        self.write_custom_dos_header(&pe::ImageDosHeader {
            e_magic: pe::IMAGE_DOS_SIGNATURE.into(),
            e_cblp: 0.into(),
            e_cp: 0.into(),
            e_crlc: 0.into(),
            e_cparhdr: 0.into(),
            e_minalloc: 0.into(),
            e_maxalloc: 0.into(),
            e_ss: 0.into(),
            e_sp: 0.into(),
            e_csum: 0.into(),
            e_ip: 0.into(),
            e_cs: 0.into(),
            e_lfarlc: 0.into(),
            e_ovno: 0.into(),
            e_res: [0.into(); 4],
            e_oemid: 0.into(),
            e_oeminfo: 0.into(),
            e_res2: [0.into(); 10],
            e_lfanew: self.nt_headers_offset.into(),
        })
    }

    /// Reserve a fixed DOS header and stub.
    ///
    /// Use `reserve_dos_header` and `reserve` if you need a custom stub.
    pub fn reserve_dos_header_and_stub(&mut self) {
        self.reserve_dos_header();
        self.reserve(64, 1);
    }

    /// Write a fixed DOS header and stub.
    ///
    /// Use `write_custom_dos_header` and `write` if you need a custom stub.
    ///
    /// This calls [`WritableBuffer::reserve`] with the total file size.
    pub fn write_dos_header_and_stub(&mut self) -> Result<()> {
        self.write_custom_dos_header(&pe::ImageDosHeader {
            e_magic: pe::IMAGE_DOS_SIGNATURE.into(),
            e_cblp: 0x90.into(),
            e_cp: 3.into(),
            e_crlc: 0.into(),
            e_cparhdr: 4.into(),
            e_minalloc: 0.into(),
            e_maxalloc: 0xffff.into(),
            e_ss: 0.into(),
            e_sp: 0xb8.into(),
            e_csum: 0.into(),
            e_ip: 0.into(),
            e_cs: 0.into(),
            e_lfarlc: 0x40.into(),
            e_ovno: 0.into(),
            e_res: [0.into(); 4],
            e_oemid: 0.into(),
            e_oeminfo: 0.into(),
            e_res2: [0.into(); 10],
            e_lfanew: self.nt_headers_offset.into(),
        })?;

        #[rustfmt::skip]
        self.buffer.write_bytes(&[
            0x0e, 0x1f, 0xba, 0x0e, 0x00, 0xb4, 0x09, 0xcd,
            0x21, 0xb8, 0x01, 0x4c, 0xcd, 0x21, 0x54, 0x68,
            0x69, 0x73, 0x20, 0x70, 0x72, 0x6f, 0x67, 0x72,
            0x61, 0x6d, 0x20, 0x63, 0x61, 0x6e, 0x6e, 0x6f,
            0x74, 0x20, 0x62, 0x65, 0x20, 0x72, 0x75, 0x6e,
            0x20, 0x69, 0x6e, 0x20, 0x44, 0x4f, 0x53, 0x20,
            0x6d, 0x6f, 0x64, 0x65, 0x2e, 0x0d, 0x0d, 0x0a,
            0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        ]);

        Ok(())
    }

    fn nt_headers_size(&self) -> u32 {
        if self.is_64 {
            mem::size_of::<pe::ImageNtHeaders64>() as u32
        } else {
            mem::size_of::<pe::ImageNtHeaders32>() as u32
        }
    }

    fn optional_header_size(&self) -> u32 {
        let size = if self.is_64 {
            mem::size_of::<pe::ImageOptionalHeader64>() as u32
        } else {
            mem::size_of::<pe::ImageOptionalHeader32>() as u32
        };
        size + self.data_directories.len() as u32 * mem::size_of::<pe::ImageDataDirectory>() as u32
    }

    /// Return the offset of the NT headers, if reserved.
    pub fn nt_headers_offset(&self) -> u32 {
        self.nt_headers_offset
    }

    /// Reserve the range for the NT headers.
    pub fn reserve_nt_headers(&mut self, data_directory_num: usize) {
        debug_assert_eq!(self.nt_headers_offset, 0);
        self.nt_headers_offset = self.reserve(self.nt_headers_size(), 8);
        self.data_directories = vec![DataDirectory::default(); data_directory_num];
        self.reserve(
            data_directory_num as u32 * mem::size_of::<pe::ImageDataDirectory>() as u32,
            1,
        );
    }

    /// Set the virtual address and size of a data directory.
    pub fn set_data_directory(&mut self, index: usize, virtual_address: u32, size: u32) {
        self.data_directories[index] = DataDirectory {
            virtual_address,
            size,
        }
    }

    /// Write the NT headers.
    pub fn write_nt_headers(&mut self, nt_headers: NtHeaders) {
        self.pad_until(self.nt_headers_offset);
        self.buffer.write_u32(LE, pe::IMAGE_NT_SIGNATURE);
        let file_header = pe::ImageFileHeader {
            machine: nt_headers.machine.into(),
            number_of_sections: self.section_header_num.into(),
            time_date_stamp: nt_headers.time_date_stamp.into(),
            pointer_to_symbol_table: self.symbol_offset.into(),
            number_of_symbols: self.symbol_num.into(),
            size_of_optional_header: (self.optional_header_size() as u16).into(),
            characteristics: nt_headers.characteristics.into(),
        };
        self.buffer.write_pod(&file_header);
        if self.is_64 {
            let optional_header = pe::ImageOptionalHeader64 {
                magic: pe::IMAGE_NT_OPTIONAL_HDR64_MAGIC.into(),
                major_linker_version: nt_headers.major_linker_version,
                minor_linker_version: nt_headers.minor_linker_version,
                size_of_code: self.code_len.into(),
                size_of_initialized_data: self.data_len.into(),
                size_of_uninitialized_data: self.bss_len.into(),
                address_of_entry_point: nt_headers.address_of_entry_point.into(),
                base_of_code: self.code_address.into(),
                image_base: nt_headers.image_base.into(),
                section_alignment: self.section_alignment.into(),
                file_alignment: self.file_alignment.into(),
                major_operating_system_version: U16::new(
                    LE,
                    nt_headers.major_operating_system_version,
                ),
                minor_operating_system_version: U16::new(
                    LE,
                    nt_headers.minor_operating_system_version,
                ),
                major_image_version: nt_headers.major_image_version.into(),
                minor_image_version: nt_headers.minor_image_version.into(),
                major_subsystem_version: nt_headers.major_subsystem_version.into(),
                minor_subsystem_version: nt_headers.minor_subsystem_version.into(),
                win32_version_value: 0.into(),
                size_of_image: self.virtual_len.into(),
                size_of_headers: self.headers_len.into(),
                check_sum: 0.into(),
                subsystem: nt_headers.subsystem.into(),
                dll_characteristics: nt_headers.dll_characteristics.into(),
                size_of_stack_reserve: nt_headers.size_of_stack_reserve.into(),
                size_of_stack_commit: nt_headers.size_of_stack_commit.into(),
                size_of_heap_reserve: nt_headers.size_of_heap_reserve.into(),
                size_of_heap_commit: nt_headers.size_of_heap_commit.into(),
                loader_flags: 0.into(),
                number_of_rva_and_sizes: (self.data_directories.len() as u32).into(),
            };
            self.buffer.write_pod(&optional_header);
        } else {
            let optional_header = pe::ImageOptionalHeader32 {
                magic: pe::IMAGE_NT_OPTIONAL_HDR32_MAGIC.into(),
                major_linker_version: nt_headers.major_linker_version,
                minor_linker_version: nt_headers.minor_linker_version,
                size_of_code: self.code_len.into(),
                size_of_initialized_data: self.data_len.into(),
                size_of_uninitialized_data: self.bss_len.into(),
                address_of_entry_point: nt_headers.address_of_entry_point.into(),
                base_of_code: self.code_address.into(),
                base_of_data: self.data_address.into(),
                image_base: (nt_headers.image_base as u32).into(),
                section_alignment: self.section_alignment.into(),
                file_alignment: self.file_alignment.into(),
                major_operating_system_version: U16::new(
                    LE,
                    nt_headers.major_operating_system_version,
                ),
                minor_operating_system_version: U16::new(
                    LE,
                    nt_headers.minor_operating_system_version,
                ),
                major_image_version: nt_headers.major_image_version.into(),
                minor_image_version: nt_headers.minor_image_version.into(),
                major_subsystem_version: nt_headers.major_subsystem_version.into(),
                minor_subsystem_version: nt_headers.minor_subsystem_version.into(),
                win32_version_value: 0.into(),
                size_of_image: self.virtual_len.into(),
                size_of_headers: self.headers_len.into(),
                check_sum: 0.into(),
                subsystem: nt_headers.subsystem.into(),
                dll_characteristics: nt_headers.dll_characteristics.into(),
                size_of_stack_reserve: (nt_headers.size_of_stack_reserve as u32).into(),
                size_of_stack_commit: (nt_headers.size_of_stack_commit as u32).into(),
                size_of_heap_reserve: (nt_headers.size_of_heap_reserve as u32).into(),
                size_of_heap_commit: (nt_headers.size_of_heap_commit as u32).into(),
                loader_flags: 0.into(),
                number_of_rva_and_sizes: (self.data_directories.len() as u32).into(),
            };
            self.buffer.write_pod(&optional_header);
        }

        for dir in &self.data_directories {
            self.buffer.write_pod(&pe::ImageDataDirectory {
                virtual_address: dir.virtual_address.into(),
                size: dir.size.into(),
            })
        }
    }

    /// Reserve the section headers.
    ///
    /// The number of reserved section headers must be the same as the number of sections that
    /// are later reserved.
    // TODO: change this to a maximum number of sections?
    pub fn reserve_section_headers(&mut self, section_header_num: u16) {
        debug_assert_eq!(self.section_header_num, 0);
        self.section_header_num = section_header_num;
        self.reserve(
            u32::from(section_header_num) * mem::size_of::<pe::ImageSectionHeader>() as u32,
            1,
        );
        // Padding before sections must be included in headers_len.
        self.reserve_align(self.file_alignment);
        self.headers_len = self.len as u32;
        self.reserve_virtual(self.len as u32);
    }

    /// Write the section headers.
    ///
    /// This uses information that was recorded when the sections were reserved.
    pub fn write_section_headers(&mut self) {
        debug_assert_eq!(self.section_header_num as usize, self.sections.len());
        for section in &self.sections {
            let section_header = pe::ImageSectionHeader {
                name: section.name,
                virtual_size: section.range.virtual_size.into(),
                virtual_address: section.range.virtual_address.into(),
                size_of_raw_data: section.range.file_size.into(),
                pointer_to_raw_data: section.range.file_offset.into(),
                pointer_to_relocations: 0.into(),
                pointer_to_linenumbers: 0.into(),
                number_of_relocations: 0.into(),
                number_of_linenumbers: 0.into(),
                characteristics: section.characteristics.into(),
            };
            self.buffer.write_pod(&section_header);
        }
    }

    /// Reserve a section.
    ///
    /// Returns the file range and virtual address range that are reserved
    /// for the section.
    pub fn reserve_section(
        &mut self,
        name: [u8; 8],
        characteristics: pe::SectionFlags,
        virtual_size: u32,
        data_size: u32,
    ) -> SectionRange {
        let virtual_address = self.reserve_virtual(virtual_size);

        // Padding after section must be included in section file size.
        let file_size = util::align_u32(data_size, self.file_alignment);
        let file_offset = if file_size != 0 {
            self.reserve(file_size, self.file_alignment)
        } else {
            0
        };

        // Sizes in optional header use the virtual size with the file alignment.
        let aligned_virtual_size = util::align_u32(virtual_size, self.file_alignment);
        if characteristics.contains(pe::IMAGE_SCN_CNT_CODE) {
            if self.code_address == 0 {
                self.code_address = virtual_address;
            }
            self.code_len += aligned_virtual_size;
        } else if characteristics.contains(pe::IMAGE_SCN_CNT_INITIALIZED_DATA) {
            if self.data_address == 0 {
                self.data_address = virtual_address;
            }
            self.data_len += aligned_virtual_size;
        } else if characteristics.contains(pe::IMAGE_SCN_CNT_UNINITIALIZED_DATA) {
            if self.data_address == 0 {
                self.data_address = virtual_address;
            }
            self.bss_len += aligned_virtual_size;
        }

        let range = SectionRange {
            virtual_address,
            virtual_size,
            file_offset,
            file_size,
        };
        self.sections.push(Section {
            name,
            characteristics,
            range,
        });
        range
    }

    /// Write the data for a section.
    pub fn write_section(&mut self, offset: u32, data: &[u8]) {
        if data.is_empty() {
            return;
        }
        self.pad_until(offset);
        self.write(data);
        self.write_align(self.file_alignment);
    }

    /// Reserve a `.text` section.
    ///
    /// Contains executable code.
    pub fn reserve_text_section(&mut self, size: u32) -> SectionRange {
        self.reserve_section(
            *b".text\0\0\0",
            pe::IMAGE_SCN_CNT_CODE | pe::IMAGE_SCN_MEM_EXECUTE | pe::IMAGE_SCN_MEM_READ,
            size,
            size,
        )
    }

    /// Reserve a `.data` section.
    ///
    /// Contains initialized data.
    ///
    /// May also contain uninitialized data if `virtual_size` is greater than `data_size`.
    pub fn reserve_data_section(&mut self, virtual_size: u32, data_size: u32) -> SectionRange {
        self.reserve_section(
            *b".data\0\0\0",
            pe::IMAGE_SCN_CNT_INITIALIZED_DATA | pe::IMAGE_SCN_MEM_READ | pe::IMAGE_SCN_MEM_WRITE,
            virtual_size,
            data_size,
        )
    }

    /// Reserve a `.rdata` section.
    ///
    /// Contains read-only initialized data.
    pub fn reserve_rdata_section(&mut self, size: u32) -> SectionRange {
        self.reserve_section(
            *b".rdata\0\0",
            pe::IMAGE_SCN_CNT_INITIALIZED_DATA | pe::IMAGE_SCN_MEM_READ,
            size,
            size,
        )
    }

    /// Reserve a `.bss` section.
    ///
    /// Contains uninitialized data.
    pub fn reserve_bss_section(&mut self, size: u32) -> SectionRange {
        self.reserve_section(
            *b".bss\0\0\0\0",
            pe::IMAGE_SCN_CNT_UNINITIALIZED_DATA | pe::IMAGE_SCN_MEM_READ | pe::IMAGE_SCN_MEM_WRITE,
            size,
            0,
        )
    }

    /// Reserve an `.idata` section.
    ///
    /// Contains import tables. Note that it is permissible to store import tables in a different
    /// section.
    ///
    /// This also sets the `pe::IMAGE_DIRECTORY_ENTRY_IMPORT` data directory.
    pub fn reserve_idata_section(&mut self, size: u32) -> SectionRange {
        let range = self.reserve_section(
            *b".idata\0\0",
            pe::IMAGE_SCN_CNT_INITIALIZED_DATA | pe::IMAGE_SCN_MEM_READ | pe::IMAGE_SCN_MEM_WRITE,
            size,
            size,
        );
        let dir = &mut self.data_directories[pe::IMAGE_DIRECTORY_ENTRY_IMPORT];
        debug_assert_eq!(dir.virtual_address, 0);
        *dir = DataDirectory {
            virtual_address: range.virtual_address,
            size,
        };
        range
    }

    /// Reserve an `.edata` section.
    ///
    /// Contains export tables.
    ///
    /// This also sets the `pe::IMAGE_DIRECTORY_ENTRY_EXPORT` data directory.
    pub fn reserve_edata_section(&mut self, size: u32) -> SectionRange {
        let range = self.reserve_section(
            *b".edata\0\0",
            pe::IMAGE_SCN_CNT_INITIALIZED_DATA | pe::IMAGE_SCN_MEM_READ,
            size,
            size,
        );
        let dir = &mut self.data_directories[pe::IMAGE_DIRECTORY_ENTRY_EXPORT];
        debug_assert_eq!(dir.virtual_address, 0);
        *dir = DataDirectory {
            virtual_address: range.virtual_address,
            size,
        };
        range
    }

    /// Reserve a `.pdata` section.
    ///
    /// Contains exception information.
    ///
    /// This also sets the `pe::IMAGE_DIRECTORY_ENTRY_EXCEPTION` data directory.
    pub fn reserve_pdata_section(&mut self, size: u32) -> SectionRange {
        let range = self.reserve_section(
            *b".pdata\0\0",
            pe::IMAGE_SCN_CNT_INITIALIZED_DATA | pe::IMAGE_SCN_MEM_READ,
            size,
            size,
        );
        let dir = &mut self.data_directories[pe::IMAGE_DIRECTORY_ENTRY_EXCEPTION];
        debug_assert_eq!(dir.virtual_address, 0);
        *dir = DataDirectory {
            virtual_address: range.virtual_address,
            size,
        };
        range
    }

    /// Reserve a `.xdata` section.
    ///
    /// Contains exception information.
    pub fn reserve_xdata_section(&mut self, size: u32) -> SectionRange {
        self.reserve_section(
            *b".xdata\0\0",
            pe::IMAGE_SCN_CNT_INITIALIZED_DATA | pe::IMAGE_SCN_MEM_READ,
            size,
            size,
        )
    }

    /// Reserve a `.rsrc` section.
    ///
    /// Contains the resource directory.
    ///
    /// This also sets the `pe::IMAGE_DIRECTORY_ENTRY_RESOURCE` data directory.
    pub fn reserve_rsrc_section(&mut self, size: u32) -> SectionRange {
        let range = self.reserve_section(
            *b".rsrc\0\0\0",
            pe::IMAGE_SCN_CNT_INITIALIZED_DATA | pe::IMAGE_SCN_MEM_READ,
            size,
            size,
        );
        let dir = &mut self.data_directories[pe::IMAGE_DIRECTORY_ENTRY_RESOURCE];
        debug_assert_eq!(dir.virtual_address, 0);
        *dir = DataDirectory {
            virtual_address: range.virtual_address,
            size,
        };
        range
    }

    /// Add a base relocation.
    ///
    /// `typ` must be one of the `IMAGE_REL_BASED_*` constants.
    pub fn add_reloc(&mut self, mut virtual_address: u32, typ: pe::BaseRelocationType) {
        let reloc = (typ.0 << 12 | (virtual_address & 0xfff) as u16).into();
        virtual_address &= !0xfff;
        if let Some(block) = self.reloc_blocks.last_mut() {
            if block.virtual_address == virtual_address {
                self.relocs.push(reloc);
                block.count += 1;
                return;
            }
            // Blocks must have an even number of relocations.
            if block.count & 1 != 0 {
                self.relocs.push(0.into());
                block.count += 1;
            }
            debug_assert!(block.virtual_address < virtual_address);
        }
        self.relocs.push(reloc);
        self.reloc_blocks.push(RelocBlock {
            virtual_address,
            count: 1,
        });
    }

    /// Return true if a base relocation has been added.
    pub fn has_relocs(&mut self) -> bool {
        !self.relocs.is_empty()
    }

    /// Reserve a `.reloc` section.
    ///
    /// This contains the base relocations that were added with `add_reloc`.
    ///
    /// This also sets the `pe::IMAGE_DIRECTORY_ENTRY_BASERELOC` data directory.
    pub fn reserve_reloc_section(&mut self) -> SectionRange {
        if let Some(block) = self.reloc_blocks.last_mut() {
            // Blocks must have an even number of relocations.
            if block.count & 1 != 0 {
                self.relocs.push(0.into());
                block.count += 1;
            }
        }
        let size = self.reloc_blocks.iter().map(RelocBlock::size).sum();
        let range = self.reserve_section(
            *b".reloc\0\0",
            pe::IMAGE_SCN_CNT_INITIALIZED_DATA
                | pe::IMAGE_SCN_MEM_READ
                | pe::IMAGE_SCN_MEM_DISCARDABLE,
            size,
            size,
        );
        let dir = &mut self.data_directories[pe::IMAGE_DIRECTORY_ENTRY_BASERELOC];
        debug_assert_eq!(dir.virtual_address, 0);
        *dir = DataDirectory {
            virtual_address: range.virtual_address,
            size,
        };
        self.reloc_offset = range.file_offset;
        range
    }

    /// Write a `.reloc` section.
    ///
    /// This contains the base relocations that were added with `add_reloc`.
    pub fn write_reloc_section(&mut self) {
        if self.reloc_offset == 0 {
            return;
        }
        self.pad_until(self.reloc_offset);

        let mut total = 0;
        for block in &self.reloc_blocks {
            self.buffer.write_pod(&pe::ImageBaseRelocation {
                virtual_address: block.virtual_address.into(),
                size_of_block: block.size().into(),
            });
            self.buffer
                .write_pod_slice(&self.relocs[total..][..block.count as usize]);
            total += block.count as usize;
        }
        debug_assert_eq!(total, self.relocs.len());

        self.write_align(self.file_alignment);
    }

    /// Reserve the certificate table.
    ///
    /// This also sets the `pe::IMAGE_DIRECTORY_ENTRY_SECURITY` data directory.
    // TODO: reserve individual certificates
    pub fn reserve_certificate_table(&mut self, size: u32) {
        let size = util::align_u32(size, 8);
        let offset = self.reserve(size, 8);
        let dir = &mut self.data_directories[pe::IMAGE_DIRECTORY_ENTRY_SECURITY];
        debug_assert_eq!(dir.virtual_address, 0);
        *dir = DataDirectory {
            virtual_address: offset,
            size,
        };
    }

    /// Write the certificate table.
    // TODO: write individual certificates
    pub fn write_certificate_table(&mut self, data: &[u8]) {
        let dir = self.data_directories[pe::IMAGE_DIRECTORY_ENTRY_SECURITY];
        self.pad_until(dir.virtual_address);
        self.write(data);
        self.pad_until(dir.virtual_address + dir.size);
    }
}

/// Information required for writing [`pe::ImageNtHeaders32`] or [`pe::ImageNtHeaders64`].
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub struct NtHeaders {
    // ImageFileHeader
    pub machine: pe::Machine,
    pub time_date_stamp: u32,
    pub characteristics: pe::FileFlags,
    // ImageOptionalHeader
    pub major_linker_version: u8,
    pub minor_linker_version: u8,
    pub address_of_entry_point: u32,
    pub image_base: u64,
    pub major_operating_system_version: u16,
    pub minor_operating_system_version: u16,
    pub major_image_version: u16,
    pub minor_image_version: u16,
    pub major_subsystem_version: u16,
    pub minor_subsystem_version: u16,
    pub subsystem: pe::Subsystem,
    pub dll_characteristics: pe::DllFlags,
    pub size_of_stack_reserve: u64,
    pub size_of_stack_commit: u64,
    pub size_of_heap_reserve: u64,
    pub size_of_heap_commit: u64,
}

#[derive(Default, Clone, Copy)]
struct DataDirectory {
    virtual_address: u32,
    size: u32,
}

/// Information required for writing [`pe::ImageSectionHeader`].
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub struct Section {
    pub name: [u8; pe::IMAGE_SIZEOF_SHORT_NAME],
    pub characteristics: pe::SectionFlags,
    pub range: SectionRange,
}

/// The file range and virtual address range for a section.
#[allow(missing_docs)]
#[derive(Debug, Default, Clone, Copy)]
pub struct SectionRange {
    pub virtual_address: u32,
    pub virtual_size: u32,
    pub file_offset: u32,
    pub file_size: u32,
}

struct RelocBlock {
    virtual_address: u32,
    count: u32,
}

impl RelocBlock {
    fn size(&self) -> u32 {
        mem::size_of::<pe::ImageBaseRelocation>() as u32 + self.count * mem::size_of::<u16>() as u32
    }
}
