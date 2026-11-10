//! This module provides a [`Builder`] for reading, modifying, and then writing Mach-O files.
use alloc::vec::Vec;
use core::marker::PhantomData;
use core::{fmt, mem};

use crate::build::{ByteString, Bytes, Error, Id, IdPrivate, Item, Result, Table};
use crate::endian::{Endianness, U32};
use crate::macho;
use crate::read::macho::{MachHeader, Nlist};
use crate::read::{self, FileKind, ReadRef};
use crate::write::{self, WritableBuffer};

/// A builder for reading, modifying, and then writing Mach-O files.
///
/// Public fields are available for modifying the values that will be written.
/// Methods are available to add elements to tables, and elements can be deleted
/// from tables by setting the `delete` field in the element.
#[derive(Debug)]
pub struct Builder<'data> {
    /// The endianness.
    ///
    /// Used to set the data encoding when writing the Mach-O file.
    pub endian: Endianness,
    /// Whether the file is 64-bit.
    pub is_64: bool,
    /// The page size.
    ///
    /// This is used for aligning segment file offsets.
    ///
    /// Defaults to 0x4000. When reading a Mach-O file, this is reduced if
    /// needed so that it divides every segment file offset.
    pub page_size: u32,
    /// The file header.
    pub header: Header,
    /// The load commmands.
    pub commands: Vec<LoadCommand<'data>>,
    /// The segment table.
    ///
    /// Each segment must be referenced by a load command, otherwise [`Self::write`]
    /// will return an error.
    pub segments: Segments<'data>,
    /// The section table.
    ///
    /// Each section must be referenced by a segment, otherwise [`Self::write`]
    /// will return an error.
    pub sections: Sections<'data>,
    /// The symbol table.
    ///
    /// If there are any symbols, then there must be a symtab load command,
    /// otherwise [`Self::write`] will return an error.
    pub symbols: Symbols<'data>,
    /// The libraries for imported symbols.
    ///
    /// Each dylib must be referenced by a load command, otherwise [`Self::write`]
    /// will return an error.
    pub dylibs: Dylibs<'data>,
    marker: PhantomData<()>,
}

struct ReadState<'a> {
    sections_len: usize,
    dylibs_len: usize,
    symbols_len: usize,
    indirect_symbols: &'a [U32<Endianness, macho::IndirectSymbol>],
}

impl<'data> Builder<'data> {
    /// Create a new Mach-O builder.
    pub fn new(endian: Endianness, is_64: bool) -> Self {
        Self {
            endian,
            is_64,
            page_size: 0x4000,
            header: Header::default(),
            commands: Vec::new(),
            segments: Segments::new(),
            sections: Sections::new(),
            symbols: Symbols::new(),
            dylibs: Dylibs::new(),
            marker: PhantomData,
        }
    }

    /// Read the Mach-O file from file data.
    ///
    /// Detects whether the file is 32-bit or 64-bit.
    pub fn read<R: ReadRef<'data>>(data: R) -> Result<Self> {
        match FileKind::parse(data)? {
            FileKind::MachO32 => Self::read32(data),
            FileKind::MachO64 => Self::read64(data),
            // TODO: FAT
            #[allow(unreachable_patterns)]
            _ => Err(Error::new("Not a Mach-O file")),
        }
    }

    /// Read a 32-bit Mach-O file from file data.
    pub fn read32<R: ReadRef<'data>>(data: R) -> Result<Self> {
        Self::read_file::<macho::MachHeader32<Endianness>, R>(data)
    }

    /// Read a 64-bit Mach-O file from file data.
    pub fn read64<R: ReadRef<'data>>(data: R) -> Result<Self> {
        Self::read_file::<macho::MachHeader64<Endianness>, R>(data)
    }

    fn read_file<Mach, R>(data: R) -> Result<Self>
    where
        Mach: MachHeader<Endian = Endianness>,
        R: ReadRef<'data>,
    {
        let header = Mach::parse(data, 0)?;
        let endian = header.endian()?;

        let mut builder = Builder {
            endian,
            is_64: header.is_type_64(),
            page_size: 0x4000,
            header: Header {
                cputype: header.cputype(endian),
                cpusubtype: header.cpusubtype(endian),
                filetype: header.filetype(endian),
                flags: header.flags(endian),
            },
            commands: Vec::new(),
            segments: Segments::new(),
            sections: Sections::new(),
            symbols: Symbols::new(),
            dylibs: Dylibs::new(),
            marker: PhantomData,
        };

        let mut state = ReadState {
            symbols_len: 0,
            sections_len: 0,
            dylibs_len: 0,
            indirect_symbols: &[][..],
        };
        let mut page_size_bits = builder.page_size.trailing_zeros();
        let mut commands = header.load_commands(endian, data, 0)?;
        while let Some(command) = commands.next()? {
            use read::macho::LoadCommandVariant;
            match command.variant()? {
                LoadCommandVariant::Segment32(segment, _) => {
                    state.sections_len += segment.nsects.get(endian) as usize;
                    let fileoff = segment.fileoff.get(endian);
                    page_size_bits = page_size_bits.min(fileoff.trailing_zeros());
                }
                LoadCommandVariant::Segment64(segment, _) => {
                    state.sections_len += segment.nsects.get(endian) as usize;
                    let fileoff = segment.fileoff.get(endian);
                    page_size_bits = page_size_bits.min(fileoff.trailing_zeros());
                }
                LoadCommandVariant::Symtab(symtab) => {
                    state.symbols_len = symtab.nsyms.get(endian) as usize;
                }
                LoadCommandVariant::Dysymtab(dysymtab) => {
                    state.indirect_symbols = dysymtab.indirect_symbols(endian, data)?;
                }
                LoadCommandVariant::Dylib(_) => {
                    state.dylibs_len += 1;
                }
                _ => {}
            }
        }
        builder.page_size = 1 << page_size_bits;

        let mut commands = header.load_commands(endian, data, 0)?;
        while let Some(command) = commands.next()? {
            use read::macho::LoadCommandVariant;
            match command.variant()? {
                LoadCommandVariant::Segment32(segment, section_data) => {
                    let id = builder.read_segment(endian, data, segment, section_data, &state)?;
                    builder.commands.push(LoadCommand::Segment(id));
                }
                LoadCommandVariant::Segment64(segment, section_data) => {
                    let id = builder.read_segment(endian, data, segment, section_data, &state)?;
                    builder.commands.push(LoadCommand::Segment(id));
                }
                LoadCommandVariant::Symtab(symtab) => {
                    let symbols = symtab.symbols(endian, data)?;
                    builder.read_symbols::<Mach, R>(endian, symbols, &state)?;
                    builder.commands.push(LoadCommand::Symtab);
                }
                LoadCommandVariant::Dysymtab(dysymtab) => {
                    if dysymtab.ntoc.get(endian) != 0 {
                        return Err(Error("Unimplemented LC_DYSYMTAB toc".into()));
                    }
                    if dysymtab.nmodtab.get(endian) != 0 {
                        return Err(Error("Unimplemented LC_DYSYMTAB modules".into()));
                    }
                    if dysymtab.nextrefsyms.get(endian) != 0 {
                        return Err(Error(
                            "Unimplemented LC_DYSYMTAB external references".into(),
                        ));
                    }
                    if dysymtab.nextrel.get(endian) != 0 {
                        return Err(Error(
                            "Unimplemented LC_DYSYMTAB external relocations".into(),
                        ));
                    }
                    if dysymtab.nlocrel.get(endian) != 0 {
                        return Err(Error("Unimplemented LC_DYSYMTAB local relocations".into()));
                    }
                    builder.commands.push(LoadCommand::Dysymtab);
                }
                LoadCommandVariant::LinkeditData(val) => {
                    builder.commands.push(LoadCommand::LinkeditData {
                        cmd: val.cmd.get(endian),
                        data: val.data(endian, data)?.into(),
                    });
                }
                LoadCommandVariant::DyldInfo(val) if command.cmd() == macho::LC_DYLD_INFO_ONLY => {
                    builder.commands.push(LoadCommand::DyldInfo {
                        rebase: val.rebase_data(endian, data)?.into(),
                        bind: val.bind_data(endian, data)?.into(),
                        weak_bind: val.weak_bind_data(endian, data)?.into(),
                        lazy_bind: val.lazy_bind_data(endian, data)?.into(),
                        export: val.export_data(endian, data)?.into(),
                    });
                }
                LoadCommandVariant::BuildVersion(val, tools) => {
                    let tools = val
                        .tools(endian, tools)?
                        .iter()
                        .map(|val| BuildToolVersion {
                            tool: val.tool.get(endian),
                            version: val.version.get(endian),
                        })
                        .collect();
                    builder
                        .commands
                        .push(LoadCommand::BuildVersion(BuildVersion {
                            platform: val.platform.get(endian),
                            minos: val.minos.get(endian),
                            sdk: val.sdk.get(endian),
                            tools,
                        }));
                }
                LoadCommandVariant::Dylib(val) => {
                    let id = builder.dylibs.next_id();
                    builder.dylibs.push(Dylib {
                        delete: false,
                        cmd: val.cmd.get(endian),
                        name: command.string(endian, val.dylib.name)?.into(),
                        timestamp: val.dylib.timestamp.get(endian),
                        current_version: val.dylib.current_version.get(endian),
                        compatibility_version: val.dylib.compatibility_version.get(endian),
                    });
                    builder.commands.push(LoadCommand::Dylib(id));
                }
                LoadCommandVariant::IdDylib(val) => {
                    builder.commands.push(LoadCommand::IdDylib(Dylib {
                        delete: false,
                        cmd: val.cmd.get(endian),
                        name: command.string(endian, val.dylib.name)?.into(),
                        timestamp: val.dylib.timestamp.get(endian),
                        current_version: val.dylib.current_version.get(endian),
                        compatibility_version: val.dylib.compatibility_version.get(endian),
                    }));
                }
                // TODO: parse these
                LoadCommandVariant::LoadDylinker(_)
                | LoadCommandVariant::SourceVersion(_)
                | LoadCommandVariant::EntryPoint(_)
                | LoadCommandVariant::Uuid(_)
                | LoadCommandVariant::EncryptionInfo32(_)
                | LoadCommandVariant::EncryptionInfo64(_) => {
                    builder.commands.push(LoadCommand::Other {
                        cmd: command.cmd(),
                        data: command.raw_data()
                            [mem::size_of::<macho::LoadCommand<Endianness>>()..]
                            .into(),
                    });
                }
                _ => {
                    return Err(Error(format!(
                        "Unimplemented load command {:x?}",
                        command.cmd()
                    )));
                }
            }
        }

        debug_assert_eq!(state.sections_len, builder.sections.len());
        debug_assert_eq!(state.symbols_len, builder.symbols.len());

        Ok(builder)
    }

    fn read_segment<S, R>(
        &mut self,
        endian: Endianness,
        data: R,
        segment: &S,
        section_data: &[u8],
        state: &ReadState<'_>,
    ) -> Result<SegmentId>
    where
        S: read::macho::Segment<Endian = Endianness>,
        R: ReadRef<'data>,
    {
        let id = self.segments.next_id();
        let mut sections = Vec::new();
        let segment_sections = segment.sections(endian, section_data)?;
        for section_offset in segment.section_offsets(endian, segment_sections) {
            let (section, offset) = section_offset?;
            let id = self.read_section(endian, data, section, offset, state)?;
            sections.push(id);
        }
        self.segments.push(Segment {
            id,
            delete: false,
            segname: *segment.segname(),
            vmaddr: segment.vmaddr(endian).into(),
            vmsize: segment.vmsize(endian).into(),
            maxprot: segment.maxprot(endian),
            initprot: segment.initprot(endian),
            flags: segment.flags(endian),
            sections,
            marker: PhantomData,
        });
        Ok(id)
    }

    fn read_section<S, R>(
        &mut self,
        endian: Endianness,
        data: R,
        section: &S,
        offset: u64,
        state: &ReadState<'_>,
    ) -> Result<SectionId>
    where
        S: read::macho::Section<Endian = Endianness>,
        R: ReadRef<'data>,
    {
        let id = self.sections.next_id();
        let size = section.size(endian).into();
        let section_data = if section.file_size(endian).is_some() {
            SectionData::Data(section.data(endian, data, offset)?.into())
        } else {
            SectionData::UninitializedData(size as u32)
        };
        let mut relocations = Vec::new();
        for relocation in section.relocations(endian, data)? {
            let relocation = relocation.info(endian);
            let r_symbolnum = relocation.r_symbolnum as usize;
            let target = if relocation.r_extern {
                if r_symbolnum >= state.symbols_len {
                    return Err(Error(format!(
                        "Invalid symbol index {} in relocation for section at index {}",
                        r_symbolnum,
                        self.sections.len(),
                    )));
                }
                RelocationTarget::Symbol(SymbolId(r_symbolnum))
            } else {
                let section_id = r_symbolnum.wrapping_sub(1);
                if section_id >= state.sections_len {
                    return Err(Error(format!(
                        "Invalid section index {} in relocation for section at index {}",
                        r_symbolnum,
                        self.sections.len(),
                    )));
                }
                RelocationTarget::Section(SectionId(section_id))
            };
            relocations.push(Relocation {
                target,
                r_address: relocation.r_address,
                r_pcrel: relocation.r_pcrel,
                r_length: relocation.r_length,
                r_type: relocation.r_type,
            });
        }

        let mut symbols = Vec::new();
        for indirect_symbol in section.indirect_symbols(endian, state.indirect_symbols)? {
            let indirect_symbol = indirect_symbol.get(endian);
            if let Some(symbol_index) = indirect_symbol.index() {
                let symbol_index = symbol_index as usize;
                if symbol_index >= state.symbols_len {
                    return Err(Error(format!(
                        "Invalid indirect symbol index {} for section at index {}",
                        symbol_index,
                        self.sections.len(),
                    )));
                }
                symbols.push(IndirectSymbol::Symbol(SymbolId(symbol_index)));
            } else {
                symbols.push(IndirectSymbol::Flags(indirect_symbol));
            }
        }
        let symbol_stub_size = section.symbol_stub_size(endian);

        self.sections.push(Section {
            id,
            delete: false,
            sectname: *section.sectname(),
            segname: *section.segname(),
            addr: section.addr(endian).into(),
            align: section.align(endian),
            relocations,
            flags: section.flags(endian),
            indirect_symbols: symbols,
            symbol_stub_size,
            data: section_data,
        });
        Ok(id)
    }

    fn read_symbols<Mach, R>(
        &mut self,
        endian: Mach::Endian,
        symbols: read::macho::SymbolTable<'data, Mach, R>,
        state: &ReadState<'_>,
    ) -> Result<()>
    where
        Mach: MachHeader<Endian = Endianness>,
        R: ReadRef<'data>,
    {
        for nlist in symbols.iter() {
            let id = self.symbols.next_id();
            let section = if nlist.n_type().typ() == macho::N_SECT {
                let section_id = usize::from(nlist.n_sect()).wrapping_sub(1);
                if section_id >= state.sections_len {
                    return Err(Error::new("Invalid symbol section index"));
                }
                Some(SectionId(section_id))
            } else {
                None
            };
            self.symbols.push(Symbol {
                id,
                delete: false,
                name: symbols.symbol_name(endian, nlist)?.into(),
                section,
                dylib: None, // TODO
                n_type: nlist.n_type(),
                n_sect: nlist.n_sect(),
                n_desc: nlist.n_desc(endian),
                n_value: nlist.n_value(endian).into(),
            });
        }
        Ok(())
    }

    /// Write the Mach-O file to the buffer.
    ///
    /// For object files, there must be a single segment. This function will assign addresses
    /// to sections.
    ///
    /// For dylibs/executables, segment and section addresses must already be assigned.
    /// Segment sizes will be calculated based on their contents. The first segment
    /// containing data must leave space for the Mach-O header and load commands between
    /// the segment address and the address of the first section.
    ///
    /// This calls [`WritableBuffer::reserve`] with the total file size.
    pub fn write(mut self, buffer: &mut dyn write::WritableBuffer) -> Result<()> {
        struct SegmentOut {
            id: SegmentId,
            nsects: u32,
            offset: u64,
            size: u64,
            vmsize: u64,
        }
        impl SegmentOut {
            fn set_range(&mut self, range: (u64, u64)) {
                self.offset = range.0;
                self.size = range.1;
            }
        }

        struct SectionOut {
            id: SectionId,
            address: u64,
            offset: u64,
            reloc_offset: u64,
            indirect_symbols_index: usize,
        }

        struct SymbolOut {
            id: SymbolId,
            name: Option<write::StringId>,
        }

        // Overflow handling: use u64 during reservation phase, check once for
        // u32 overflow before writing. Don't bother checking for u64 overflow.
        struct Offset(u64);
        impl Offset {
            fn reserve(&mut self, size: u64, align: u64) -> (u64, u64) {
                self.0 = write::align(self.0, align);
                let offset = self.0;
                self.0 += size;
                (offset, size)
            }
        }

        self.delete_orphans();

        // Assign section indices based on the order of load commands.
        let mut out_segments = Vec::with_capacity(self.segments.len());
        let mut out_segments_index = vec![None; self.segments.len()];
        let mut out_sections = Vec::with_capacity(self.sections.len());
        let mut out_sections_index = vec![None; self.sections.len()];
        let mut indirect_symbols_count = 0;
        for command in &self.commands {
            let LoadCommand::Segment(id) = command else {
                continue;
            };
            let segment = self.segments.get(*id);
            let section_start = out_sections.len() as u32;
            for section_id in &segment.sections {
                let section = self.sections.get(*section_id);
                let indirect_symbols_index = if section.indirect_symbols.is_empty() {
                    0
                } else {
                    indirect_symbols_count
                };
                indirect_symbols_count += section.indirect_symbols.len();
                out_sections_index[section_id.0] = Some(out_sections.len() + 1);
                out_sections.push(SectionOut {
                    id: *section_id,
                    address: 0,
                    offset: 0,
                    reloc_offset: 0,
                    indirect_symbols_index,
                });
            }
            out_segments_index[id.0] = Some(out_segments.len());
            out_segments.push(SegmentOut {
                id: *id,
                nsects: out_sections.len() as u32 - section_start,
                offset: 0,
                size: 0,
                vmsize: 0,
            });
        }
        if out_sections.len() > 255 {
            return Err(Error::new("Too many sections"));
        }

        // Partition symbols.
        let mut local_symbols = vec![];
        let mut external_symbols = vec![];
        let mut undefined_symbols = vec![];
        for symbol in &self.symbols {
            if symbol.is_undefined() {
                undefined_symbols.push(symbol.id);
            } else if symbol.is_extdef() {
                external_symbols.push(symbol.id);
            } else {
                local_symbols.push(symbol.id);
            }
        }
        external_symbols.sort_by_key(|id| &*self.symbols.get(*id).name);
        undefined_symbols.sort_by_key(|id| &*self.symbols.get(*id).name);

        // Assign symbol indices and add symbol strings to strtab.
        let mut strtab = write::StringTable::new();
        let mut out_symbols = Vec::with_capacity(self.symbols.len());
        let mut out_symbols_index = vec![None; self.symbols.len()];
        for id in local_symbols
            .iter()
            .copied()
            .chain(external_symbols.iter().copied())
            .chain(undefined_symbols.iter().copied())
        {
            let symbol = self.symbols.get(id);
            let name = if symbol.name.is_empty() {
                None
            } else {
                Some(strtab.add(&symbol.name))
            };
            out_symbols_index[id.0] = Some(out_symbols.len());
            out_symbols.push(SymbolOut { id, name });
        }
        if out_symbols.len() as u64 > 1 << 24 {
            return Err(Error::new("Too many symbols"));
        }
        let nsyms = out_symbols.len() as u32;

        // Check for unreferenced items.
        for segment in &self.segments {
            if out_segments_index[segment.id().index()].is_none() {
                return Err(Error::new("Segment is not referenced by a load command"));
            }
        }
        for section in &self.sections {
            if out_sections_index[section.id().index()].is_none() {
                return Err(Error::new("Section is not referenced by a segment"));
            }
        }
        let mut have_symtab = false;
        let mut dylib_referenced = vec![false; self.dylibs.len()];
        for command in &self.commands {
            match command {
                LoadCommand::Symtab => have_symtab = true,
                LoadCommand::Dylib(id) => dylib_referenced[id.index()] = true,
                _ => {}
            }
        }
        if nsyms != 0 && !have_symtab {
            return Err(Error::new(
                "Symbols are present but there is no symtab load command",
            ));
        }
        for (index, referenced) in dylib_referenced.iter().enumerate() {
            if !referenced && !self.dylibs.get(DylibId(index)).delete {
                return Err(Error::new("Dylib is not referenced by a load command"));
            }
        }

        // Start calculating offsets of everything.
        let encoder = self.encoder();
        let address_size = encoder.address_size();
        let mut offset = Offset(encoder.mach_header_size());

        // Reserve load commands.
        let command_offset = offset.0;
        for command in &self.commands {
            let size = match command {
                LoadCommand::Segment(id) => encoder.segment_command_size(out_segments[id.0].nsects),
                LoadCommand::Symtab => encoder.symtab_command_size(),
                LoadCommand::Dysymtab => encoder.dysymtab_command_size(),
                LoadCommand::Dylib(id) => {
                    encoder.dylib_command_size(self.dylibs.get(*id).name.len())
                }
                LoadCommand::IdDylib(dylib) => encoder.dylib_command_size(dylib.name.len()),
                LoadCommand::BuildVersion(version) => {
                    encoder.build_version_command_size(version.tools.len() as u32)
                }
                LoadCommand::LinkeditData { .. } => encoder.linkedit_data_command_size(),
                LoadCommand::DyldInfo { .. } => encoder.dyld_info_command_size(),
                LoadCommand::Other { data, .. } => encoder.load_command_size(data.len() as u64),
            };
            offset.reserve(size, 1);
        }
        let sizeofcmds = offset.0 - command_offset;

        // Reserve segment/section/relocations.
        if self.header.filetype == macho::MH_OBJECT {
            let [out_segment] = &mut *out_segments else {
                return Err(Error::new("Mach-O object must have single unnamed segment"));
            };
            let segment = self.segments.get(out_segment.id);
            if segment.segname != [0; 16] || segment.vmaddr != 0 {
                return Err(Error::new("Mach-O object must have single unnamed segment"));
            }

            // Section data can immediately follow the load commands without any alignment padding.
            let segment_file_offset = offset.0;
            // Reserve segment data in the same order as `segment.sections`.
            // Addresses are also assigned in this order, except that zerofill sections come last.
            let mut address = 0;
            for out_section in &mut out_sections {
                let section = self.sections.get(out_section.id);
                if let SectionData::Data(data) = &section.data {
                    let align = 1 << section.align;
                    address = write::align(address, align);
                    out_section.address = address;
                    out_section.offset = segment_file_offset + address;
                    address += data.len() as u64;
                }
            }
            out_segment.set_range(offset.reserve(address, 1));
            for out_section in &mut out_sections {
                let section = self.sections.get(out_section.id);
                if let SectionData::UninitializedData(size) = &section.data {
                    let align = 1 << section.align;
                    address = write::align(address, align);
                    out_section.address = address;
                    address += u64::from(*size);
                }
            }
            out_segment.vmsize = address;

            for out_section in &mut out_sections {
                let section = self.sections.get(out_section.id);
                let count = section.relocations.len();
                if count != 0 {
                    out_section.reloc_offset = offset
                        .reserve(count as u64 * encoder.relocation_size(), address_size)
                        .0;
                }
            }
        } else {
            // The first segment with data must include the Mach header and load commands.
            // Other segments can start at any page aligned offset.
            let mut segment_offset = 0;
            let mut address = 0;
            for out_segment in &mut out_segments {
                let segment = self.segments.get(out_segment.id);
                if segment.vmaddr < address {
                    // We could handle this if required, but for now this is the normal order.
                    return Err(Error::new(format!(
                        "Segment load commands must be ordered by address: {:x} < {:x}",
                        segment.vmaddr, address
                    )));
                }
                address = segment.vmaddr;

                for section_id in &segment.sections {
                    let section = self.sections.get(*section_id);
                    let section_index = out_sections_index[section_id.0].unwrap();
                    let out_section = &mut out_sections[section_index - 1];
                    // Sections must already have addresses assigned.
                    out_section.address = section.addr;
                    if section.addr < address {
                        // We could handle this if required, but for now this is the normal order.
                        return Err(Error::new(format!(
                            "Sections must be ordered by address: {:x} < {:x}",
                            section.addr, address
                        )));
                    }
                    address = section.addr + section.data.size() as u64;
                    match &section.data {
                        SectionData::Data(data) => {
                            out_section.offset = (section.addr - segment.vmaddr) + segment_offset;
                            out_segment.size = (section.addr - segment.vmaddr) + data.len() as u64;
                        }
                        SectionData::UninitializedData(_) => {
                            out_section.offset = 0;
                        }
                    }

                    if !section.relocations.is_empty() {
                        return Err(Error::new("Non-MH_OBJECT section cannot have relocations"));
                    }
                }

                let segment_end = segment.vmaddr + segment.vmsize;
                if segment_end < address {
                    return Err(Error::new(format!(
                        "Segment address range must contain sections: {segment_end:#x} < {address:#x}"
                    )));
                }
                out_segment.vmsize = segment.vmsize;
                address = segment_end;

                if out_segment.size > 0 {
                    out_segment.offset = segment_offset;
                    out_segment.size = write::align(out_segment.size, self.page_size as u64);
                    segment_offset += out_segment.size;
                    offset.0 = segment_offset;
                }
            }
        }

        // Reserve linkedit.
        let mut linkedit_offsets = Vec::new();
        let linkedit_offset = offset.0;
        for command in &self.commands {
            if let LoadCommand::LinkeditData { cmd, data } = command {
                if *cmd != macho::LC_CODE_SIGNATURE {
                    // TODO: align
                    linkedit_offsets.push(offset.reserve(data.len() as u64, address_size).0);
                }
            } else if let LoadCommand::DyldInfo {
                rebase,
                bind,
                weak_bind,
                lazy_bind,
                export,
            } = command
            {
                for data in [rebase, bind, weak_bind, lazy_bind, export] {
                    if !data.is_empty() {
                        linkedit_offsets.push(offset.reserve(data.len() as u64, address_size).0);
                    }
                }
            }
        }

        // Reserve symtab and strtab. Place these in the same order as lld.
        let symoff = offset
            .reserve(u64::from(nsyms) * encoder.nlist_size(), address_size)
            .0;
        let indirect_symbols_offset = if indirect_symbols_count == 0 {
            0
        } else {
            offset
                .reserve(
                    indirect_symbols_count as u64 * encoder.indirect_symbol_size(),
                    4,
                )
                .0
        };
        let mut strtab_data = Vec::new();
        let strsize = encoder.strtab(&mut strtab_data, &mut strtab)?;
        let stroff = offset.reserve(strsize as u64, 1).0;

        // Reserve code signature. Place this last.
        let mut code_signature_offset = 0;
        for command in &self.commands {
            if let LoadCommand::LinkeditData { cmd, data } = command {
                if *cmd == macho::LC_CODE_SIGNATURE {
                    code_signature_offset = offset.reserve(data.len() as u64, 16).0;
                }
            }
        }

        let linkedit_filesize = offset.0 - linkedit_offset;
        let linkedit_vmsize = write::align(linkedit_filesize, self.page_size as u64);
        for command in &self.commands {
            if let LoadCommand::Segment(id) = command {
                let segment = self.segments.get(*id);
                if segment.segname() == b"__LINKEDIT" {
                    let out_segment = &mut out_segments[id.0];
                    out_segment.offset = linkedit_offset;
                    out_segment.size = linkedit_filesize;
                    out_segment.vmsize = linkedit_vmsize;
                }
            }
        }

        // Finished layout.
        let reserved_len = offset.0;
        // If the total length fits in 32-bit, then none of the u32 file size casts below
        // need checking.
        if reserved_len > u64::from(u32::MAX) {
            return Err(Error::new(format!("File size overflow: {reserved_len:#x}")));
        }
        buffer
            .reserve(reserved_len)
            .map_err(|_| Error(format!("Cannot allocate buffer length {reserved_len:#x}")))?;
        let buffer = &mut write::CountingBuffer::new(buffer);

        // Start writing.
        encoder.mach_header(
            buffer,
            &write::macho::MachHeader {
                cputype: self.header.cputype,
                cpusubtype: self.header.cpusubtype,
                filetype: self.header.filetype,
                ncmds: self.commands.len() as u32,
                sizeofcmds: sizeofcmds as u32,
                flags: self.header.flags,
            },
        );

        let mut linkedit_offsets_iter = linkedit_offsets.iter();
        for command in &self.commands {
            match *command {
                LoadCommand::Segment(id) => {
                    let segment = self.segments.get(id);
                    let out_segment = &out_segments[id.0];
                    let cmd = &write::macho::SegmentCommand {
                        segname: segment.segname,
                        vmaddr: segment.vmaddr,
                        vmsize: out_segment.vmsize,
                        fileoff: out_segment.offset,
                        filesize: out_segment.size,
                        maxprot: segment.maxprot,
                        initprot: segment.initprot,
                        nsects: out_segment.nsects,
                        flags: segment.flags,
                    };
                    encoder.segment_command(buffer, cmd);
                    for section_id in &segment.sections {
                        let section = self.sections.get(*section_id);
                        let section_index = out_sections_index[section_id.0].unwrap();
                        let out_section = &out_sections[section_index - 1];
                        let header = &write::macho::SectionHeader {
                            sectname: section.sectname,
                            segname: section.segname,
                            addr: section.addr,
                            size: section.data.size() as u64,
                            offset: out_section.offset as u32,
                            align: section.align,
                            reloff: out_section.reloc_offset as u32,
                            nreloc: section.relocations.len() as u32,
                            flags: section.flags,
                            reserved1: out_section.indirect_symbols_index as u32,
                            reserved2: section.symbol_stub_size,
                            reserved3: 0,
                        };
                        encoder.section_header(buffer, header);
                    }
                }
                LoadCommand::Symtab => {
                    let cmd = &write::macho::SymtabCommand {
                        symoff: symoff as u32,
                        nsyms,
                        stroff: stroff as u32,
                        strsize,
                    };
                    encoder.symtab_command(buffer, cmd);
                }
                LoadCommand::Dysymtab => {
                    let cmd = &write::macho::DysymtabCommand {
                        ilocalsym: 0,
                        nlocalsym: local_symbols.len() as u32,
                        iextdefsym: local_symbols.len() as u32,
                        nextdefsym: external_symbols.len() as u32,
                        iundefsym: (local_symbols.len() + external_symbols.len()) as u32,
                        nundefsym: undefined_symbols.len() as u32,
                        indirectsymoff: indirect_symbols_offset as u32,
                        nindirectsyms: indirect_symbols_count as u32,
                        ..Default::default()
                    };
                    encoder.dysymtab_command(buffer, cmd);
                }
                LoadCommand::Dylib(id) => {
                    let dylib = self.dylibs.get(id);
                    let cmd = &write::macho::DylibCommand {
                        cmd: dylib.cmd,
                        name: &dylib.name,
                        timestamp: dylib.timestamp,
                        current_version: dylib.current_version,
                        compatibility_version: dylib.compatibility_version,
                    };
                    encoder.dylib_command(buffer, cmd);
                }
                LoadCommand::IdDylib(ref dylib) => {
                    let cmd = &write::macho::DylibCommand {
                        cmd: dylib.cmd,
                        name: &dylib.name,
                        timestamp: dylib.timestamp,
                        current_version: dylib.current_version,
                        compatibility_version: dylib.compatibility_version,
                    };
                    encoder.dylib_command(buffer, cmd);
                }
                LoadCommand::BuildVersion(BuildVersion {
                    platform,
                    minos,
                    sdk,
                    ref tools,
                }) => {
                    let cmd = &write::macho::BuildVersionCommand {
                        platform,
                        minos,
                        sdk,
                        ntools: tools.len() as u32,
                    };
                    encoder.build_version_command(buffer, cmd);
                    for tool in tools {
                        encoder.build_tool_version(buffer, tool.tool, tool.version);
                    }
                }
                LoadCommand::LinkeditData { cmd, ref data } => {
                    let offset = if cmd == macho::LC_CODE_SIGNATURE {
                        code_signature_offset
                    } else {
                        *linkedit_offsets_iter.next().unwrap()
                    };
                    encoder.linkedit_data_command(buffer, cmd, offset as u32, data.len() as u32);
                }
                LoadCommand::DyldInfo {
                    ref rebase,
                    ref bind,
                    ref weak_bind,
                    ref lazy_bind,
                    ref export,
                } => {
                    let [
                        (rebase_off, rebase_size),
                        (bind_off, bind_size),
                        (weak_bind_off, weak_bind_size),
                        (lazy_bind_off, lazy_bind_size),
                        (export_off, export_size),
                    ] = [rebase, bind, weak_bind, lazy_bind, export].map(|data| {
                        let size = data.len() as u32;
                        if size == 0 {
                            (0, 0)
                        } else {
                            (*linkedit_offsets_iter.next().unwrap() as u32, size)
                        }
                    });
                    encoder.dyld_info_command(
                        buffer,
                        macho::LC_DYLD_INFO_ONLY,
                        &write::macho::DyldInfoCommand {
                            rebase_off,
                            rebase_size,
                            bind_off,
                            bind_size,
                            weak_bind_off,
                            weak_bind_size,
                            lazy_bind_off,
                            lazy_bind_size,
                            export_off,
                            export_size,
                        },
                    );
                }
                LoadCommand::Other { cmd, ref data } => {
                    encoder.load_command(buffer, cmd, data.as_slice());
                }
            }
        }

        // Write section data.
        for out_section in &out_sections {
            let section = self.sections.get(out_section.id);
            if let SectionData::Data(data) = &section.data {
                buffer.resize(out_section.offset);
                buffer.write_bytes(data);
            }
        }

        // Write relocations.
        for out_section in &mut out_sections {
            let section = self.sections.get(out_section.id);
            let count = section.relocations.len();
            if count != 0 {
                buffer.resize(out_section.reloc_offset);

                for relocation in &section.relocations {
                    let (r_extern, r_symbolnum) = match relocation.target {
                        RelocationTarget::Symbol(id) => {
                            (true, out_symbols_index[id.0].unwrap_or(0) as u32)
                        }
                        RelocationTarget::Section(id) => {
                            (false, out_sections_index[id.0].unwrap_or(0) as u32)
                        }
                    };

                    let reloc_info = macho::RelocationInfo {
                        r_address: relocation.r_address,
                        r_symbolnum,
                        r_pcrel: relocation.r_pcrel,
                        r_length: relocation.r_length,
                        r_extern,
                        r_type: relocation.r_type,
                    };
                    encoder.relocation(buffer, &reloc_info);
                }
            }
        }

        // Write linkedit.
        let mut linkedit_offsets_iter = linkedit_offsets.iter();
        for command in &self.commands {
            if let LoadCommand::LinkeditData { cmd, data } = command {
                if *cmd != macho::LC_CODE_SIGNATURE {
                    let offset = linkedit_offsets_iter.next().unwrap();
                    buffer.resize(*offset);
                    buffer.write_bytes(data);
                }
            } else if let LoadCommand::DyldInfo {
                rebase,
                bind,
                weak_bind,
                lazy_bind,
                export,
            } = command
            {
                for data in [rebase, bind, weak_bind, lazy_bind, export] {
                    if !data.is_empty() {
                        let offset = linkedit_offsets_iter.next().unwrap();
                        buffer.resize(*offset);
                        buffer.write_bytes(data);
                    }
                }
            }
        }

        // Write symtab and strtab.
        buffer.resize(symoff);
        for out_symbol in &out_symbols {
            let symbol = self.symbols.get(out_symbol.id);
            let n_sect = if let Some(id) = symbol.section {
                out_sections_index[id.0].unwrap_or(0) as u8
            } else {
                symbol.n_sect
            };
            let n_strx = out_symbol.name.map(|id| strtab.get_offset(id)).unwrap_or(0);
            let nlist = &write::macho::Nlist {
                n_strx,
                n_type: symbol.n_type,
                n_sect,
                n_desc: symbol.n_desc,
                n_value: symbol.n_value,
            };
            encoder.nlist(buffer, nlist);
        }

        if indirect_symbols_offset != 0 {
            buffer.resize(indirect_symbols_offset);
            for out_section in &out_sections {
                let section = self.sections.get(out_section.id);
                for indirect_symbol in &section.indirect_symbols {
                    let indirect_symbol = match indirect_symbol {
                        IndirectSymbol::Flags(flags) => *flags,
                        IndirectSymbol::Symbol(symbol_id) => {
                            let symbol_index = out_symbols_index[symbol_id.0].unwrap_or(0);
                            macho::IndirectSymbol(symbol_index as u32)
                        }
                    };
                    encoder.indirect_symbol(buffer, indirect_symbol);
                }
            }
        }

        debug_assert_eq!(stroff, buffer.count());
        buffer.write_bytes(&strtab_data);

        // Write code signature.
        for command in &self.commands {
            if let LoadCommand::LinkeditData { cmd, data } = command {
                if *cmd == macho::LC_CODE_SIGNATURE {
                    buffer.resize(code_signature_offset);
                    buffer.write_bytes(data);
                }
            }
        }

        debug_assert_eq!(reserved_len, buffer.count());
        Ok(())
    }

    /// Delete segments, symbols, relocations, and dynamics that refer
    /// to deleted items.
    ///
    /// This calls `delete_orphan_segments`, `delete_orphan_symbols`,
    /// `delete_orphan_relocations`, and `delete_orphan_load_commands`.
    pub fn delete_orphans(&mut self) {
        self.delete_orphan_segments();
        self.delete_orphan_symbols();
        self.delete_orphan_relocations();
        self.delete_orphan_load_commands();
    }

    /// Set the delete flag for segments that only refer to deleted sections.
    pub fn delete_orphan_segments(&mut self) {
        let sections = &self.sections;
        for segment in &mut self.segments {
            // We only delete segments that have become empty due to section deletions.
            if segment.sections.is_empty() {
                continue;
            }
            segment.sections.retain(|id| !sections.get(*id).delete);
            segment.delete = segment.sections.is_empty();
        }
    }

    /// Set the delete flag for symbols that refer to deleted sections.
    pub fn delete_orphan_symbols(&mut self) {
        for symbol in &mut self.symbols {
            if let Some(section) = symbol.section {
                if self.sections.get_mut(section).delete {
                    symbol.delete = true;
                }
            }
        }
    }

    /// Delete relocations that refer to deleted symbols.
    pub fn delete_orphan_relocations(&mut self) {
        let symbols = &self.symbols;
        let sections = self
            .sections
            .iter()
            .map(|section| section.delete)
            .collect::<Vec<bool>>();
        for section in &mut self.sections {
            section
                .relocations
                .retain(|relocation| match relocation.target {
                    RelocationTarget::Symbol(id) => !symbols.get(id).delete,
                    RelocationTarget::Section(id) => !sections[id.0],
                });
        }
    }

    /// Delete load commands that refer to deleted segments or dylibs.
    pub fn delete_orphan_load_commands(&mut self) {
        let segments = &self.segments;
        let dylibs = &self.dylibs;
        self.commands.retain(|command| match command {
            LoadCommand::Segment(id) => !segments.get(*id).delete,
            LoadCommand::Dylib(id) => !dylibs.get(*id).delete,
            _ => true,
        });
    }

    /// Return the Mach-O file encoder.
    ///
    /// This can be useful for calculating sizes.
    pub fn encoder(&self) -> write::macho::Encoder<Endianness> {
        write::macho::Encoder::new(self.endian, self.is_64)
    }
}

/// Mach-O file header.
///
/// This corresponds to fields in [`macho::MachHeader32`] or [`macho::MachHeader64`].
/// This only contains the Mach-O file header fields that can be modified.
/// The other fields are automatically calculated.
#[derive(Debug, Default)]
pub struct Header {
    /// CPU specifier.
    ///
    /// One of the `CPU_TYPE_*` constants.
    pub cputype: macho::CpuType,
    /// Machine specifier.
    ///
    /// One of the `CPU_SUBTYPE_*` constants. The meaning depends on the `cputype` value.
    pub cpusubtype: macho::CpuSubtype,
    /// The file type.
    ///
    /// One of the `MH_*` constants for the file type.
    pub filetype: macho::FileType,
    /// The file flags.
    ///
    /// A combination of bit flags defined by `MH_*` constants.
    pub flags: macho::FileFlags,
}

/// A Mach-O Load command.
///
/// This corresponds to [`macho::LoadCommand`] and its many variants.
#[derive(Debug)]
pub enum LoadCommand<'data> {
    /// `LC_SEGMENT` or `LC_SEGMENT64`
    Segment(SegmentId),
    /// `LC_SYMTAB`
    Symtab,
    /// `LC_DYSYMTAB`
    Dysymtab,
    /// `LC_LOAD_DYLIB`, `LC_LOAD_WEAK_DYLIB`, `LC_REEXPORT_DYLIB`,
    /// `LC_LAZY_LOAD_DYLIB`, or `LC_LOAD_UPWARD_DYLIB`
    Dylib(DylibId),
    /// `LC_ID_DYLIB`
    IdDylib(Dylib<'data>),
    /*
    /// `LC_LOAD_DYLINKER`
    LoadDylinker(Bytes<'data>),
    /// `LC_ID_DYLINKER`
    IdDylinker(Bytes<'data>),
    */
    /// `LC_BUILD_VERSION`
    BuildVersion(BuildVersion),
    /// A blob of data in the linkedit segment.
    ///
    /// Corresponds to [`macho::LinkeditDataCommand`].
    LinkeditData {
        /// Type of load command.
        ///
        /// One of the `LC_*` constants.
        cmd: macho::LoadCommandType,
        /// Data for the linkedit segment.
        data: Bytes<'data>,
    },
    /// `LC_DYLD_INFO_ONLY`.
    DyldInfo {
        /// Byte stream of rebase opcodes.
        rebase: Bytes<'data>,
        /// Byte stream of bind opcodes.
        bind: Bytes<'data>,
        /// Byte stream of weak bind opcodes.
        weak_bind: Bytes<'data>,
        /// Byte stream of lazy bind opcodes.
        lazy_bind: Bytes<'data>,
        /// Byte stream of export trie.
        export: Bytes<'data>,
    },
    /// An unrecognized or obsolete load command.
    Other {
        /// Type of load command.
        ///
        /// One of the `LC_*` constants.
        cmd: macho::LoadCommandType,
        /// Data for the command.
        data: Bytes<'data>,
    },
}

/// A build version load command.
///
/// Corresponds to [`macho::BuildVersionCommand`].
#[derive(Debug)]
pub struct BuildVersion {
    /// One of the `PLATFORM_` constants (for example,
    /// [`object::macho::PLATFORM_MACOS`](macho::PLATFORM_MACOS)).
    pub platform: macho::Platform,
    /// The minimum OS version, where `X.Y.Z` is encoded in nibbles as
    /// `xxxx.yy.zz`.
    pub minos: macho::Version,
    /// The SDK version as `X.Y.Z`, where `X.Y.Z` is encoded in nibbles as
    /// `xxxx.yy.zz`.
    pub sdk: macho::Version,
    /// The build tool versions.
    pub tools: Vec<BuildToolVersion>,
}

/// The version of a build tool.
///
/// Corresponds to [`macho::BuildToolVersion`].
#[derive(Debug)]
pub struct BuildToolVersion {
    /// One of the `TOOL_*` constants.
    pub tool: macho::Tool,
    /// Version number of the tool.
    ///
    /// X.Y.Z is encoded in nibbles xxxx.yy.zz
    pub version: macho::Version,
}

/// An ID for referring to a segment in [`Segments`].
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct SegmentId(usize);

impl fmt::Debug for SegmentId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "SegmentId({})", self.0)
    }
}

impl Id for SegmentId {
    fn index(&self) -> usize {
        self.0
    }
}

impl IdPrivate for SegmentId {
    fn new(id: usize) -> Self {
        SegmentId(id)
    }
}

/// A segment in [`Segments`].
///
/// This corresponds to [`macho::SegmentCommand32`] or [`macho::SegmentCommand64`].
#[derive(Debug)]
pub struct Segment<'data> {
    id: SegmentId,
    /// Ignore this segment when writing the Mach-O file.
    pub delete: bool,
    /// Segment name
    pub segname: [u8; 16],
    /// memory address of this segment
    pub vmaddr: u64,
    /// memory size of this segment
    pub vmsize: u64,
    /// Maximum VM protection.
    ///
    /// A combination of `VM_PROT_*` flags.
    pub maxprot: macho::VmProt,
    /// Initial VM protection.
    ///
    /// A combination of `VM_PROT_*` flags.
    pub initprot: macho::VmProt,
    /// The `flags` field in the segment command.
    ///
    /// A combination of `SG_*` flags.
    pub flags: macho::SegmentFlags,
    /// The sections contained in this segment.
    pub sections: Vec<SectionId>,
    marker: PhantomData<&'data ()>,
}

impl<'data> Item for Segment<'data> {
    type Id = SegmentId;

    fn is_deleted(&self) -> bool {
        self.delete
    }
}

impl<'data> Segment<'data> {
    /// The ID used for referring to this segment.
    pub fn id(&self) -> SegmentId {
        self.id
    }

    /// Return the segment name up until the null terminator.
    pub fn segname(&self) -> &[u8] {
        let segname = &self.segname[..];
        match memchr::memchr(b'\0', segname) {
            Some(end) => &segname[..end],
            None => segname,
        }
    }
}

/// A segment table.
pub type Segments<'data> = Table<Segment<'data>>;

impl<'data> Segments<'data> {
    /// Add a new segment to the table.
    pub fn add(&mut self) -> &mut Segment<'data> {
        let id = self.next_id();
        self.push(Segment {
            id,
            delete: false,
            segname: [0; 16],
            vmaddr: 0,
            vmsize: 0,
            maxprot: macho::VmProt(0),
            initprot: macho::VmProt(0),
            flags: macho::SegmentFlags(0),
            sections: Vec::new(),
            marker: PhantomData,
        });
        self.get_mut(id)
    }
}

/// An ID for referring to a section in [`Sections`].
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct SectionId(usize);

impl fmt::Debug for SectionId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "SectionId({})", self.0)
    }
}

impl Id for SectionId {
    fn index(&self) -> usize {
        self.0
    }
}

impl IdPrivate for SectionId {
    fn new(id: usize) -> Self {
        SectionId(id)
    }
}

/// A section in [`Sections`].
///
/// This corresponds to [`macho::Section32`] or [`macho::Section64`].
#[derive(Debug)]
pub struct Section<'data> {
    id: SectionId,
    /// Ignore this section when writing the Mach-O file.
    pub delete: bool,
    /// The name of this section.
    pub sectname: [u8; 16],
    /// The segment this section goes in.
    pub segname: [u8; 16],
    /// Virtual memory address of this section.
    pub addr: u64,
    /// Section alignment (power of 2)
    pub align: u32,
    /// Relocation entries.
    pub relocations: Vec<Relocation>,
    /// Flags (section type and attributes)
    pub flags: macho::SectionFlags,
    /// Symbols referenced by pointers or stubs in the section data.
    ///
    /// This is only valid for some section types.
    pub indirect_symbols: Vec<IndirectSymbol>,
    /// The size of a stub in a section with type [`macho::S_SYMBOL_STUBS`].
    pub symbol_stub_size: u32,
    /// The section data.
    pub data: SectionData<'data>,
}

impl<'data> Item for Section<'data> {
    type Id = SectionId;

    fn is_deleted(&self) -> bool {
        self.delete
    }
}

impl<'data> Section<'data> {
    /// The ID used for referring to this section.
    pub fn id(&self) -> SectionId {
        self.id
    }

    /// Return the section name up until the null terminator.
    pub fn sectname(&self) -> &[u8] {
        let sectname = &self.sectname[..];
        match memchr::memchr(b'\0', sectname) {
            Some(end) => &sectname[..end],
            None => sectname,
        }
    }
    /// Return the segment name up until the null terminator.
    pub fn segname(&self) -> &[u8] {
        let segname = &self.segname[..];
        match memchr::memchr(b'\0', segname) {
            Some(end) => &segname[..end],
            None => segname,
        }
    }
}

/// The data for a [`Section`].
#[derive(Debug, Clone)]
pub enum SectionData<'data> {
    /// The section contains the given raw data bytes.
    Data(Bytes<'data>),
    /// The section contains uninitialised data bytes of the given length.
    UninitializedData(u32),
}

impl<'data> SectionData<'data> {
    /// The virtual memory size of the section.
    pub fn size(&self) -> usize {
        match self {
            SectionData::Data(bytes) => bytes.len(),
            SectionData::UninitializedData(len) => *len as usize,
        }
    }
}

/// A section table.
pub type Sections<'data> = Table<Section<'data>>;

impl<'data> Sections<'data> {
    /// Add a new section to the table.
    pub fn add(&mut self) -> &mut Section<'data> {
        let id = self.next_id();
        self.push(Section {
            id,
            delete: false,
            sectname: [0; 16],
            segname: [0; 16],
            addr: 0,
            align: 0,
            relocations: Vec::new(),
            flags: macho::SectionFlags(0),
            indirect_symbols: Vec::new(),
            symbol_stub_size: 0,
            data: SectionData::Data(Bytes::default()),
        })
    }
}

/// A relocation stored in a [`Section`].
///
/// This corresponds to [`macho::RelocationInfo`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Relocation {
    /// The symbol or section referenced by the Mach-O relocation.
    ///
    /// Used to set `r_symbolnum`.
    pub target: RelocationTarget,
    /// The `r_address` field in the Mach-O relocation.
    pub r_address: u32,
    /// The `r_pcrel` field in the Mach-O relocation.
    pub r_pcrel: bool,
    /// The `r_length` field in the Mach-O relocation.
    pub r_length: u8,
    /// The `r_type` field in the Mach-O relocation.
    pub r_type: macho::RelocationType,
}

/// The symbol or section referenced by a Mach-O relocation.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RelocationTarget {
    /// A symbol.
    Symbol(SymbolId),
    /// A section.
    Section(SectionId),
}

/// An ID for referring to a symbol in [`Symbols`].
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct SymbolId(usize);

impl fmt::Debug for SymbolId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.0.fmt(f)
    }
}

impl Id for SymbolId {
    fn index(&self) -> usize {
        self.0
    }
}

impl IdPrivate for SymbolId {
    fn new(id: usize) -> Self {
        SymbolId(id)
    }
}

/// An indirect symbol.
#[derive(Debug)]
pub enum IndirectSymbol {
    /// A sentinel value.
    Flags(macho::IndirectSymbol),
    /// A symbol ID.
    Symbol(SymbolId),
}

/// A symbol in [`Symbols`].
///
/// This corresponds to [`macho::Nlist32`] or [`macho::Nlist64`].
#[derive(Debug)]
pub struct Symbol<'data> {
    id: SymbolId,
    /// Ignore this symbol when writing the Mach-O file.
    pub delete: bool,
    /// The name of the symbol.
    ///
    /// Used to set the `n_strx` field in the Mach-O symbol.
    pub name: ByteString<'data>,
    /// The section referenced by the symbol.
    ///
    /// Used to set the `n_sect` field in the Mach-O symbol.
    pub section: Option<SectionId>,
    /// The library referenced by the symbol.
    ///
    /// Used to set dylib ordinal subfield of the `n_desc` field in the Mach-O symbol.
    pub dylib: Option<DylibId>,
    /// The `n_type` field in the Mach-O symbol.
    pub n_type: macho::SymbolFlags,
    /// The `n_sect` field in the Mach-O symbol, if `section` is `None`.
    pub n_sect: u8,
    /// The `n_desc` field in the Mach-O symbol.
    pub n_desc: macho::SymbolDesc,
    /// The `n_value` field in the Mach-O symbol.
    pub n_value: u64,
}

impl<'data> Item for Symbol<'data> {
    type Id = SymbolId;

    fn is_deleted(&self) -> bool {
        self.delete
    }
}

impl<'data> Symbol<'data> {
    /// The ID used for referring to this symbol.
    pub fn id(&self) -> SymbolId {
        self.id
    }

    /// Return true if this is an undefined symbol.
    pub fn is_undefined(&self) -> bool {
        !self.n_type.is_stab() && self.n_type.typ() == macho::N_UNDF
    }

    /// Return true if this is a dynamically visible symbol.
    ///
    /// Returns true if `N_EXT` is set and `N_PEXT` is not set.
    pub fn is_extdef(&self) -> bool {
        self.n_type & (macho::N_EXT | macho::N_PEXT) == macho::N_EXT
    }
}

/// A symbol table.
pub type Symbols<'data> = Table<Symbol<'data>>;

impl<'data> Symbols<'data> {
    /// Add a new symbol to the table.
    pub fn add(&mut self) -> &mut Symbol<'data> {
        let id = self.next_id();
        self.push(Symbol {
            id,
            delete: false,
            name: ByteString::default(),
            section: None,
            dylib: None,
            n_type: macho::SymbolFlags(0),
            n_sect: 0,
            n_desc: macho::SymbolDesc(0),
            n_value: 0,
        })
    }
}

/// An ID for referring to a library in [`Dylibs`].
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct DylibId(usize);

impl fmt::Debug for DylibId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "DylibId({})", self.0)
    }
}

impl Id for DylibId {
    fn index(&self) -> usize {
        self.0
    }
}

impl IdPrivate for DylibId {
    fn new(id: usize) -> Self {
        DylibId(id)
    }
}

/// A library specified in a load command.
///
/// This correspeonds to [`macho::Dylib`].
#[derive(Debug, Clone)]
pub struct Dylib<'data> {
    /// Ignore this dylib when writing the Mach-O file.
    ///
    /// This has no effect for [`LoadCommand::IdDylib`].
    pub delete: bool,
    /// `LC_ID_DYLIB, `LC_LOAD_DYLIB`, `LC_LOAD_WEAK_DYLIB`, `LC_REEXPORT_DYLIB`,
    /// `LC_LAZY_LOAD_DYLIB`, or `LC_LOAD_UPWARD_DYLIB`
    pub cmd: macho::LoadCommandType,
    /// The library's path name.
    pub name: ByteString<'data>,
    /// The library's build time stamp.
    pub timestamp: u32,
    /// The library's current version number.
    pub current_version: macho::Version,
    /// The library's compatibility version number.
    pub compatibility_version: macho::Version,
}

impl<'data> Item for Dylib<'data> {
    type Id = DylibId;

    fn is_deleted(&self) -> bool {
        self.delete
    }
}

/// The libraries for imported symbols.
pub type Dylibs<'data> = Table<Dylib<'data>>;
