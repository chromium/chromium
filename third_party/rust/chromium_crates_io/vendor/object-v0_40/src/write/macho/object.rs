use crate::macho;
use crate::write::macho::encoder::*;
use crate::write::string::*;
use crate::write::*;

#[derive(Default, Clone, Copy)]
struct SectionOffsets {
    index: u32,
    offset: u64,
    address: u64,
    reloc_offset: u64,
    reloc_count: u32,
}

#[derive(Default, Clone, Copy)]
struct SymbolOffsets {
    index: u32,
    str_id: Option<StringId>,
}

/// The customizable portion of a [`macho::BuildVersionCommand`].
#[derive(Debug, Default, Clone, Copy)]
#[non_exhaustive] // May want to add the tool list?
pub struct MachOBuildVersion {
    /// One of the `PLATFORM_` constants (for example,
    /// [`object::macho::PLATFORM_MACOS`](macho::PLATFORM_MACOS)).
    pub platform: macho::Platform,
    /// The minimum OS version, where `X.Y.Z` is encoded in nibbles as
    /// `xxxx.yy.zz`.
    pub minos: macho::Version,
    /// The SDK version as `X.Y.Z`, where `X.Y.Z` is encoded in nibbles as
    /// `xxxx.yy.zz`.
    pub sdk: macho::Version,
}

// Public methods.
impl<'a> Object<'a> {
    /// Specify the Mach-O CPU subtype.
    ///
    /// Requires `feature = "macho"`.
    #[inline]
    pub fn set_macho_cpu_subtype(&mut self, cpu_subtype: macho::CpuSubtype) {
        self.macho_cpu_subtype = Some(cpu_subtype);
    }

    /// Specify information for a Mach-O `LC_BUILD_VERSION` command.
    ///
    /// Requires `feature = "macho"`.
    #[inline]
    pub fn set_macho_build_version(&mut self, info: MachOBuildVersion) {
        self.macho_build_version = Some(info);
    }
}

// Private methods.
impl<'a> Object<'a> {
    pub(crate) fn macho_segment_name(&self, segment: StandardSegment) -> &'static [u8] {
        match segment {
            StandardSegment::Text => &b"__TEXT"[..],
            StandardSegment::Data => &b"__DATA"[..],
            StandardSegment::Debug => &b"__DWARF"[..],
        }
    }

    pub(crate) fn macho_section_info(
        &self,
        section: StandardSection,
    ) -> (&'static [u8], &'static [u8], SectionKind, SectionFlags) {
        match section {
            StandardSection::Text => (
                &b"__TEXT"[..],
                &b"__text"[..],
                SectionKind::Text,
                SectionFlags::None,
            ),
            StandardSection::Data => (
                &b"__DATA"[..],
                &b"__data"[..],
                SectionKind::Data,
                SectionFlags::None,
            ),
            StandardSection::ReadOnlyData => (
                &b"__TEXT"[..],
                &b"__const"[..],
                SectionKind::ReadOnlyData,
                SectionFlags::None,
            ),
            StandardSection::ReadOnlyDataWithRel => (
                &b"__DATA"[..],
                &b"__const"[..],
                SectionKind::ReadOnlyDataWithRel,
                SectionFlags::None,
            ),
            StandardSection::ReadOnlyString => (
                &b"__TEXT"[..],
                &b"__cstring"[..],
                SectionKind::ReadOnlyString,
                SectionFlags::None,
            ),
            StandardSection::UninitializedData => (
                &b"__DATA"[..],
                &b"__bss"[..],
                SectionKind::UninitializedData,
                SectionFlags::None,
            ),
            StandardSection::Tls => (
                &b"__DATA"[..],
                &b"__thread_data"[..],
                SectionKind::Tls,
                SectionFlags::None,
            ),
            StandardSection::UninitializedTls => (
                &b"__DATA"[..],
                &b"__thread_bss"[..],
                SectionKind::UninitializedTls,
                SectionFlags::None,
            ),
            StandardSection::TlsVariables => (
                &b"__DATA"[..],
                &b"__thread_vars"[..],
                SectionKind::TlsVariables,
                SectionFlags::None,
            ),
            StandardSection::GnuProperty => {
                // Unsupported section.
                (&[], &[], SectionKind::Note, SectionFlags::None)
            }
            StandardSection::EhFrame => (
                &b"__TEXT"[..],
                &b"__eh_frame"[..],
                SectionKind::ReadOnlyData,
                SectionFlags::MachO {
                    flags: macho::S_COALESCED
                        | macho::S_ATTR_LIVE_SUPPORT
                        | macho::S_ATTR_NO_TOC
                        | macho::S_ATTR_STRIP_STATIC_SYMS,
                    reserved2: 0,
                },
            ),
        }
    }

    pub(crate) fn macho_section_flags(&self, section: &Section<'_>) -> SectionFlags {
        let flags = match section.kind {
            SectionKind::Text => {
                macho::S_REGULAR | macho::S_ATTR_PURE_INSTRUCTIONS | macho::S_ATTR_SOME_INSTRUCTIONS
            }
            SectionKind::Data | SectionKind::ReadOnlyData | SectionKind::ReadOnlyDataWithRel => {
                macho::S_REGULAR.into()
            }
            SectionKind::ReadOnlyString => macho::S_CSTRING_LITERALS.into(),
            SectionKind::UninitializedData => macho::S_ZEROFILL.into(),
            SectionKind::Tls => macho::S_THREAD_LOCAL_REGULAR.into(),
            SectionKind::UninitializedTls => macho::S_THREAD_LOCAL_ZEROFILL.into(),
            SectionKind::TlsVariables => macho::S_THREAD_LOCAL_VARIABLES.into(),
            SectionKind::Debug | SectionKind::DebugString => macho::S_REGULAR | macho::S_ATTR_DEBUG,
            SectionKind::OtherString => macho::S_CSTRING_LITERALS.into(),
            SectionKind::Other | SectionKind::Linker | SectionKind::Metadata => {
                macho::S_REGULAR.into()
            }
            SectionKind::Note | SectionKind::Unknown => {
                return SectionFlags::None;
            }
        };
        SectionFlags::MachO {
            flags,
            reserved2: 0,
        }
    }

    pub(crate) fn macho_symbol_flags(&self, symbol: &Symbol) -> SymbolFlags<SectionId, SymbolId> {
        // TODO: N_STAB
        let n_type = match symbol.section {
            SymbolSection::Undefined | SymbolSection::Common => macho::N_UNDF | macho::N_EXT,
            SymbolSection::Absolute => macho::N_ABS.into(),
            SymbolSection::Section(_) => macho::N_SECT.into(),
            SymbolSection::None => {
                return SymbolFlags::None;
            }
        } | match symbol.scope {
            SymbolScope::Unknown | SymbolScope::Compilation => macho::SymbolFlags(0),
            SymbolScope::Linkage => macho::N_EXT | macho::N_PEXT,
            SymbolScope::Dynamic => macho::N_EXT,
        };
        let mut n_desc = if symbol.weak {
            if symbol.is_undefined() {
                macho::N_WEAK_REF
            } else {
                macho::N_WEAK_DEF
            }
        } else {
            macho::SymbolDesc(0)
        };
        if symbol.is_common() {
            let align = if symbol.value > 1 {
                symbol.value.trailing_zeros().min(15) as u8
            } else {
                0
            };
            n_desc = n_desc.with_common_alignment(align);
        }
        SymbolFlags::MachO { n_type, n_desc }
    }

    fn macho_tlv_bootstrap(&mut self) -> SymbolId {
        match self.tlv_bootstrap {
            Some(id) => id,
            None => {
                let id = self.add_symbol(Symbol {
                    name: b"_tlv_bootstrap".to_vec(),
                    value: 0,
                    size: 0,
                    kind: SymbolKind::Text,
                    scope: SymbolScope::Dynamic,
                    weak: false,
                    section: SymbolSection::Undefined,
                    flags: SymbolFlags::None,
                });
                self.tlv_bootstrap = Some(id);
                id
            }
        }
    }

    /// Create the `__thread_vars` entry for a TLS variable.
    ///
    /// The symbol given by `symbol_id` will be updated to point to this entry.
    ///
    /// A new `SymbolId` will be returned. The caller must update this symbol
    /// to point to the initializer.
    ///
    /// If `symbol_id` is not for a TLS variable, then it is returned unchanged.
    pub(crate) fn macho_add_thread_var(&mut self, symbol_id: SymbolId) -> SymbolId {
        let symbol = self.symbol_mut(symbol_id);
        if symbol.kind != SymbolKind::Tls {
            return symbol_id;
        }

        // Create the initializer symbol.
        let mut name = symbol.name.clone();
        name.extend_from_slice(b"$tlv$init");
        let init_symbol_id = self.add_raw_symbol(Symbol {
            name,
            value: 0,
            size: 0,
            kind: SymbolKind::Tls,
            scope: SymbolScope::Compilation,
            weak: false,
            section: SymbolSection::Undefined,
            flags: SymbolFlags::None,
        });

        // Add the tlv entry.
        // Three pointers in size:
        //   - __tlv_bootstrap - used to make sure support exists
        //   - spare pointer - used when mapped by the runtime
        //   - pointer to symbol initializer
        let section = self.section_id(StandardSection::TlsVariables);
        let address_size = self.architecture.address_size().unwrap().bytes();
        let size = u64::from(address_size) * 3;
        let data = vec![0; size as usize];
        let offset = self.append_section_data(section, &data, u64::from(address_size));

        let tlv_bootstrap = self.macho_tlv_bootstrap();
        self.add_relocation(
            section,
            Relocation {
                offset,
                symbol: tlv_bootstrap,
                addend: 0,
                flags: RelocationFlags::Generic {
                    kind: RelocationKind::Absolute,
                    encoding: RelocationEncoding::Generic,
                    size: address_size * 8,
                },
            },
        )
        .unwrap();
        self.add_relocation(
            section,
            Relocation {
                offset: offset + u64::from(address_size) * 2,
                symbol: init_symbol_id,
                addend: 0,
                flags: RelocationFlags::Generic {
                    kind: RelocationKind::Absolute,
                    encoding: RelocationEncoding::Generic,
                    size: address_size * 8,
                },
            },
        )
        .unwrap();

        // Update the symbol to point to the tlv.
        let symbol = self.symbol_mut(symbol_id);
        symbol.value = offset;
        symbol.size = size;
        symbol.section = SymbolSection::Section(section);

        init_symbol_id
    }

    pub(crate) fn macho_translate_relocation(
        &mut self,
        section: SectionId,
        reloc: &mut RelocationInternal,
    ) -> Result<()> {
        use RelocationEncoding as E;
        use RelocationKind as K;

        let (kind, encoding, mut size) = if let RelocationFlags::Generic {
            kind,
            encoding,
            size,
        } = reloc.flags
        {
            (kind, encoding, size)
        } else {
            return Ok(());
        };
        // Aarch64 relocs of these sizes act as if they are double-word length
        if self.architecture == Architecture::Aarch64 && matches!(size, 12 | 21 | 26) {
            size = 32;
        }
        let r_length = match size {
            8 => 0,
            16 => 1,
            32 => 2,
            64 => 3,
            _ => return Err(Error(format!("unimplemented reloc size {:?}", reloc))),
        };
        let unsupported_reloc = || Err(Error(format!("unimplemented relocation {:?}", reloc)));
        let (r_pcrel, r_type) = match self.architecture {
            Architecture::I386 => match kind {
                K::Absolute => (false, macho::GENERIC_RELOC_VANILLA),
                _ => return unsupported_reloc(),
            },
            Architecture::Arm => match kind {
                K::Absolute => (false, macho::ARM_RELOC_VANILLA),
                _ => return unsupported_reloc(),
            },
            Architecture::X86_64 => match (kind, encoding) {
                (K::Absolute, E::Generic) => (false, macho::X86_64_RELOC_UNSIGNED),
                // TODO: E::Generic might need to use unsigned with a subtractor instead
                (K::Relative, E::Generic | E::X86RipRelative) => (true, macho::X86_64_RELOC_SIGNED),
                (K::Relative, E::X86Branch) => (true, macho::X86_64_RELOC_BRANCH),
                (K::PltRelative, E::Generic | E::X86Branch) => (true, macho::X86_64_RELOC_BRANCH),
                (K::GotRelative, E::Generic) => (true, macho::X86_64_RELOC_GOT),
                (K::GotRelative, E::X86RipRelativeMovq) => (true, macho::X86_64_RELOC_GOT_LOAD),
                _ => return unsupported_reloc(),
            },
            Architecture::Aarch64 | Architecture::Aarch64_Ilp32 => match (kind, encoding) {
                (K::Absolute, E::Generic) => (false, macho::ARM64_RELOC_UNSIGNED),
                (K::Relative, E::Generic) => {
                    // arm64 doesn't have PC relative relocations for data,
                    // use ARM64_RELOC_SUBTRACTOR instead.
                    reloc.subtractor = Some(self.section_symbol(section));
                    // S - (section + offset) + A = S - section + (A - offset)
                    reloc.addend -= reloc.offset as i64;
                    (false, macho::ARM64_RELOC_UNSIGNED)
                }
                (K::Relative, E::AArch64Call) => (true, macho::ARM64_RELOC_BRANCH26),
                (K::PltRelative, E::Generic | E::AArch64Call) => {
                    (true, macho::ARM64_RELOC_BRANCH26)
                }
                (K::GotRelative, E::Generic) => (true, macho::ARM64_RELOC_POINTER_TO_GOT),
                _ => return unsupported_reloc(),
            },
            Architecture::PowerPc | Architecture::PowerPc64 => match kind {
                K::Absolute => (false, macho::PPC_RELOC_VANILLA),
                _ => return unsupported_reloc(),
            },
            _ => {
                return Err(Error(format!(
                    "unimplemented architecture {:?}",
                    self.architecture
                )));
            }
        };
        reloc.flags = RelocationFlags::MachO {
            r_type,
            r_pcrel,
            r_length,
        };
        Ok(())
    }

    pub(crate) fn macho_adjust_addend(
        &mut self,
        relocation: &mut RelocationInternal,
    ) -> Result<bool> {
        let (r_type, r_pcrel) = if let RelocationFlags::MachO {
            r_type, r_pcrel, ..
        } = relocation.flags
        {
            (r_type, r_pcrel)
        } else {
            return Err(Error(format!("invalid relocation flags {:?}", relocation)));
        };
        if r_pcrel {
            // For PC relative relocations on some architectures, the
            // addend does not include the offset required due to the
            // PC being different from the place of the relocation.
            // This differs from other file formats, so adjust the
            // addend here to account for this.
            let pcrel_offset = match self.architecture {
                Architecture::I386 => 4,
                Architecture::X86_64 => match r_type {
                    macho::X86_64_RELOC_SIGNED_1 => 5,
                    macho::X86_64_RELOC_SIGNED_2 => 6,
                    macho::X86_64_RELOC_SIGNED_4 => 8,
                    _ => 4,
                },
                // TODO: maybe missing support for some architectures and relocations
                _ => 0,
            };
            relocation.addend += pcrel_offset;
        }
        // Determine if addend is implicit.
        let implicit = if self.architecture == Architecture::Aarch64 {
            match r_type {
                macho::ARM64_RELOC_BRANCH26
                | macho::ARM64_RELOC_PAGE21
                | macho::ARM64_RELOC_PAGEOFF12 => false,
                _ => true,
            }
        } else {
            true
        };
        Ok(implicit)
    }

    pub(crate) fn macho_relocation_size(&self, reloc: &RelocationInternal) -> Result<u8> {
        if let RelocationFlags::MachO { r_length, .. } = reloc.flags {
            Ok(8 << r_length)
        } else {
            Err(Error("invalid relocation flags".into()))
        }
    }

    pub(crate) fn macho_write(&self, buffer: &mut dyn WritableBuffer) -> Result<()> {
        struct Offset(u64);
        impl Offset {
            fn reserve(&mut self, size: u64, align_size: u64) -> u64 {
                self.0 = align(self.0, align_size);
                let offset = self.0;
                self.0 += size;
                offset
            }
        }
        let is_64 = match self.architecture.address_size().unwrap() {
            AddressSize::U8 | AddressSize::U16 | AddressSize::U32 => false,
            AddressSize::U64 => true,
        };
        let encoder = Encoder::new(self.endian, is_64);
        let pointer_align = encoder.address_size();

        // Calculate offsets of everything, and build strtab.
        let mut offset = Offset(encoder.mach_header_size());

        // Calculate size of commands.
        let mut ncmds = 0;
        let command_offset = offset.0;

        // Calculate size of segment command and section headers.
        let nsects = self.sections.len() as u32;
        if nsects > 255 {
            return Err(Error(format!("Too many sections: {nsects:#x}")));
        }
        let segment_command_offset = offset.reserve(encoder.segment_command_size(nsects), 1);
        ncmds += 1;

        // Calculate size of build version.
        let mut build_version_offset = 0;
        if self.macho_build_version.is_some() {
            build_version_offset = offset.reserve(encoder.build_version_command_size(0), 1);
            ncmds += 1;
        }

        // Calculate size of symtab command.
        let symtab_command_offset = offset.reserve(encoder.symtab_command_size(), 1);
        ncmds += 1;

        // Calculate size of dysymtab command.
        let dysymtab_command_offset = offset.reserve(encoder.dysymtab_command_size(), 1);
        ncmds += 1;

        let sizeofcmds = offset.0 - command_offset;

        // Calculate size of section data.
        // Section data can immediately follow the load commands without any alignment padding.
        let segment_file_offset = offset.0;
        let mut section_offsets = vec![SectionOffsets::default(); self.sections.len()];
        let mut address = 0;
        for (index, section) in self.sections.iter().enumerate() {
            section_offsets[index].index = 1 + index as u32;
            if !section.is_bss() {
                address = align(address, section.align);
                section_offsets[index].address = address;
                section_offsets[index].offset = segment_file_offset + address;
                address += section.size;
            }
        }
        let segment_file_size = address;
        offset.reserve(address, 1);
        for (index, section) in self.sections.iter().enumerate() {
            if section.is_bss() {
                debug_assert!(section.data.is_empty());
                address = align(address, section.align);
                section_offsets[index].address = address;
                address += section.size;
            }
        }
        if !is_64 && address > u64::from(u32::MAX) {
            return Err(Error(format!("Segment vmsize overflow: {address:#x}")));
        }

        // Partition symbols and add symbol strings to strtab.
        let mut strtab = StringTable::default();
        let mut symbol_offsets = vec![SymbolOffsets::default(); self.symbols.len()];
        let mut local_symbols = vec![];
        let mut external_symbols = vec![];
        let mut undefined_symbols = vec![];
        for (index, symbol) in self.symbols.iter().enumerate() {
            // The unified API allows creating symbols that we don't emit, so filter
            // them out here.
            //
            // Since we don't actually emit the symbol kind, we validate it here too.
            match symbol.kind {
                SymbolKind::Text | SymbolKind::Data | SymbolKind::Tls | SymbolKind::Unknown => {}
                SymbolKind::Section => {
                    // AArch64 needs temporary section symbols for relative relocations.
                    if !matches!(
                        self.architecture,
                        Architecture::Aarch64 | Architecture::Aarch64_Ilp32
                    ) {
                        continue;
                    }
                }
                SymbolKind::File => continue,
                SymbolKind::Label => {
                    return Err(Error(format!(
                        "unimplemented symbol `{}` kind {:?}",
                        symbol.name().unwrap_or(""),
                        symbol.kind
                    )));
                }
            }
            if !symbol.name.is_empty() {
                symbol_offsets[index].str_id = Some(strtab.add(&symbol.name));
            }
            if symbol.is_undefined() || symbol.is_common() {
                undefined_symbols.push(index);
            } else if symbol.is_local() {
                local_symbols.push(index);
            } else {
                external_symbols.push(index);
            }
        }

        external_symbols.sort_by_key(|index| &*self.symbols[*index].name);
        undefined_symbols.sort_by_key(|index| &*self.symbols[*index].name);

        // Count symbols.
        let mut nsyms = 0;
        for index in local_symbols
            .iter()
            .copied()
            .chain(external_symbols.iter().copied())
            .chain(undefined_symbols.iter().copied())
        {
            symbol_offsets[index].index = nsyms;
            nsyms += 1;
        }
        if nsyms > 1 << 24 {
            return Err(Error(format!("Too many symbols: {nsyms:#x}")));
        }

        // Calculate size of relocations.
        for (index, section) in self.sections.iter().enumerate() {
            let count: u32 = section
                .relocations
                .iter()
                .map(|reloc| {
                    1 + u32::from(reloc.addend != 0) + u32::from(reloc.subtractor.is_some())
                })
                .sum();
            if count != 0 {
                section_offsets[index].reloc_offset =
                    offset.reserve(count as u64 * encoder.relocation_size(), pointer_align);
                section_offsets[index].reloc_count = count;
            }
        }

        // Calculate size of symtab/strtab.
        let symtab_offset = offset.reserve(u64::from(nsyms) * encoder.nlist_size(), pointer_align);
        let mut strtab_data = Vec::new();
        let strsize = encoder.strtab(&mut strtab_data, &mut strtab)?;
        let strtab_offset = offset.reserve(u64::from(strsize), 1);

        // Start writing.
        let reserved_len = offset.0;
        if reserved_len > u64::from(u32::MAX) {
            return Err(Error(format!("File size overflow: {reserved_len:#x}")));
        }
        buffer
            .reserve(reserved_len)
            .map_err(|_| Error(String::from("Cannot allocate buffer")))?;
        let buffer = &mut CountingBuffer::new(buffer);

        // Write file header.
        let (cputype, cpusubtype_id) = match (self.architecture, self.sub_architecture) {
            (Architecture::Arm, None) => (macho::CPU_TYPE_ARM, macho::CPU_SUBTYPE_ARM_ALL),
            (Architecture::Aarch64, None) => (macho::CPU_TYPE_ARM64, macho::CPU_SUBTYPE_ARM64_ALL),
            (Architecture::Aarch64, Some(SubArchitecture::Arm64E)) => {
                (macho::CPU_TYPE_ARM64, macho::CPU_SUBTYPE_ARM64E)
            }
            (Architecture::Aarch64_Ilp32, None) => {
                (macho::CPU_TYPE_ARM64_32, macho::CPU_SUBTYPE_ARM64_32_V8)
            }
            (Architecture::I386, None) => (macho::CPU_TYPE_X86, macho::CPU_SUBTYPE_I386_ALL),
            (Architecture::X86_64, None) => (macho::CPU_TYPE_X86_64, macho::CPU_SUBTYPE_X86_64_ALL),
            (Architecture::PowerPc, None) => {
                (macho::CPU_TYPE_POWERPC, macho::CPU_SUBTYPE_POWERPC_ALL)
            }
            (Architecture::PowerPc64, None) => {
                (macho::CPU_TYPE_POWERPC64, macho::CPU_SUBTYPE_POWERPC_ALL)
            }
            _ => {
                return Err(Error(format!(
                    "unimplemented architecture {:?} with sub-architecture {:?}",
                    self.architecture, self.sub_architecture
                )));
            }
        };
        let mut cpusubtype: macho::CpuSubtype = cpusubtype_id.into();

        if let Some(cpu_subtype) = self.macho_cpu_subtype {
            cpusubtype = cpu_subtype;
        }

        let mut flags = match self.flags {
            FileFlags::MachO { flags } => flags,
            _ => macho::FileFlags(0),
        };
        if self.macho_subsections_via_symbols {
            flags |= macho::MH_SUBSECTIONS_VIA_SYMBOLS;
        }
        let mach_header = &MachHeader {
            cputype,
            cpusubtype,
            filetype: macho::MH_OBJECT,
            ncmds,
            sizeofcmds: sizeofcmds as u32,
            flags,
        };
        encoder.mach_header(buffer, mach_header);

        // Write segment command.
        debug_assert_eq!(segment_command_offset, buffer.count());
        let segment_command = &SegmentCommand {
            segname: [0; 16],
            vmaddr: 0,
            vmsize: address,
            fileoff: segment_file_offset,
            filesize: segment_file_size,
            maxprot: macho::VM_PROT_READ | macho::VM_PROT_WRITE | macho::VM_PROT_EXECUTE,
            initprot: macho::VM_PROT_READ | macho::VM_PROT_WRITE | macho::VM_PROT_EXECUTE,
            nsects,
            flags: macho::SegmentFlags(0),
        };
        encoder.segment_command(buffer, segment_command);

        // Write section headers.
        for (index, section) in self.sections.iter().enumerate() {
            let mut sectname = [0; 16];
            sectname
                .get_mut(..section.name.len())
                .ok_or_else(|| {
                    Error(format!(
                        "section name `{}` is too long",
                        section.name().unwrap_or(""),
                    ))
                })?
                .copy_from_slice(&section.name);
            let mut segname = [0; 16];
            segname
                .get_mut(..section.segment.len())
                .ok_or_else(|| {
                    Error(format!(
                        "segment name `{}` is too long",
                        section.segment().unwrap_or(""),
                    ))
                })?
                .copy_from_slice(&section.segment);
            let SectionFlags::MachO { flags, reserved2 } = self.section_flags(section) else {
                return Err(Error(format!(
                    "unimplemented section `{}` kind {:?}",
                    section.name().unwrap_or(""),
                    section.kind
                )));
            };
            let section_header = &SectionHeader {
                sectname,
                segname,
                addr: section_offsets[index].address,
                size: section.size,
                offset: section_offsets[index].offset as u32,
                align: section.align.trailing_zeros(),
                reloff: section_offsets[index].reloc_offset as u32,
                nreloc: section_offsets[index].reloc_count as u32,
                flags,
                reserved1: 0,
                reserved2,
                reserved3: 0,
            };
            encoder.section_header(buffer, section_header);
        }

        // Write build version.
        if let Some(version) = &self.macho_build_version {
            debug_assert_eq!(build_version_offset, buffer.count());
            let build_version_command = &BuildVersionCommand {
                platform: version.platform,
                minos: version.minos,
                sdk: version.sdk,
                ntools: 0,
            };
            encoder.build_version_command(buffer, build_version_command);
        }

        // Write symtab command.
        debug_assert_eq!(symtab_command_offset, buffer.count());
        let symtab_command = &SymtabCommand {
            symoff: symtab_offset as u32,
            nsyms,
            stroff: strtab_offset as u32,
            strsize,
        };
        encoder.symtab_command(buffer, symtab_command);

        // Write dysymtab command.
        debug_assert_eq!(dysymtab_command_offset, buffer.count());
        let dysymtab_command = &DysymtabCommand {
            ilocalsym: 0,
            nlocalsym: local_symbols.len() as u32,
            iextdefsym: local_symbols.len() as u32,
            nextdefsym: external_symbols.len() as u32,
            iundefsym: local_symbols.len() as u32 + external_symbols.len() as u32,
            nundefsym: undefined_symbols.len() as u32,
            ..Default::default()
        };
        encoder.dysymtab_command(buffer, dysymtab_command);

        // Write section data.
        debug_assert_eq!(segment_file_offset, buffer.count());
        for (index, section) in self.sections.iter().enumerate() {
            if !section.is_bss() {
                buffer.resize(section_offsets[index].offset);
                buffer.write_bytes(&section.data);
            }
        }
        debug_assert_eq!(segment_file_offset + segment_file_size, buffer.count());

        // Write relocations.
        for (index, section) in self.sections.iter().enumerate() {
            if !section.relocations.is_empty() {
                buffer.resize(section_offsets[index].reloc_offset);

                let mut write_reloc = |reloc: &RelocationInternal| {
                    let (r_type, r_pcrel, r_length) = if let RelocationFlags::MachO {
                        r_type,
                        r_pcrel,
                        r_length,
                    } = reloc.flags
                    {
                        (r_type, r_pcrel, r_length)
                    } else {
                        return Err(Error("invalid relocation flags".into()));
                    };

                    if let Some(subtractor) = reloc.subtractor {
                        let r_type = match self.architecture {
                            Architecture::Aarch64 | Architecture::Aarch64_Ilp32 => {
                                macho::ARM64_RELOC_SUBTRACTOR
                            }
                            Architecture::X86_64 => macho::X86_64_RELOC_SUBTRACTOR,
                            _ => {
                                return Err(Error(format!("unimplemented relocation {:?}", reloc)));
                            }
                        };
                        let reloc_info = macho::RelocationInfo {
                            r_address: reloc.offset as u32,
                            r_symbolnum: symbol_offsets[subtractor.0].index,
                            r_pcrel: false,
                            r_length,
                            r_extern: true,
                            r_type,
                        };
                        encoder.relocation(buffer, &reloc_info);
                    }

                    // Write explicit addend.
                    if reloc.addend != 0 {
                        let r_type = match self.architecture {
                            Architecture::Aarch64 | Architecture::Aarch64_Ilp32 => {
                                macho::ARM64_RELOC_ADDEND
                            }
                            _ => {
                                return Err(Error(format!("unimplemented relocation {:?}", reloc)));
                            }
                        };

                        let reloc_info = macho::RelocationInfo {
                            r_address: reloc.offset as u32,
                            r_symbolnum: reloc.addend as u32,
                            r_pcrel: false,
                            r_length,
                            r_extern: false,
                            r_type,
                        };
                        encoder.relocation(buffer, &reloc_info);
                    }

                    let r_extern;
                    let r_symbolnum;
                    let symbol = &self.symbols[reloc.symbol.0];
                    if symbol.kind == SymbolKind::Section {
                        r_symbolnum = section_offsets[symbol.section.id().unwrap().0].index;
                        r_extern = false;
                    } else {
                        r_symbolnum = symbol_offsets[reloc.symbol.0].index;
                        r_extern = true;
                    }

                    let reloc_info = macho::RelocationInfo {
                        r_address: reloc.offset as u32,
                        r_symbolnum,
                        r_pcrel,
                        r_length,
                        r_extern,
                        r_type,
                    };
                    encoder.relocation(buffer, &reloc_info);
                    Ok(())
                };

                // Relocations are emitted in descending order as otherwise Apple's
                // new linker crashes. This matches LLVM's behavior too:
                // https://github.com/llvm/llvm-project/blob/e9b8cd0c8/llvm/lib/MC/MachObjectWriter.cpp#L1001-L1002
                let need_reverse = |relocs: &[RelocationInternal]| {
                    let Some(first) = relocs.first() else {
                        return false;
                    };
                    let Some(last) = relocs.last() else {
                        return false;
                    };
                    first.offset < last.offset
                };
                if need_reverse(&section.relocations) {
                    for reloc in section.relocations.iter().rev() {
                        write_reloc(reloc)?;
                    }
                } else {
                    for reloc in &section.relocations {
                        write_reloc(reloc)?;
                    }
                }
            }
        }

        // Write symtab.
        buffer.resize(symtab_offset);
        for index in local_symbols
            .iter()
            .copied()
            .chain(external_symbols.iter().copied())
            .chain(undefined_symbols.iter().copied())
        {
            let symbol = &self.symbols[index];
            let SymbolFlags::MachO { n_type, n_desc } = self.symbol_flags(symbol) else {
                return Err(Error(format!(
                    "unimplemented symbol `{}` kind {:?}",
                    symbol.name().unwrap_or(""),
                    symbol.kind
                )));
            };

            let n_sect = match symbol.section {
                SymbolSection::Section(id) => id.0 + 1,
                _ => 0,
            };

            let n_value = match symbol.section {
                SymbolSection::Common => symbol.size,
                SymbolSection::Section(section) => {
                    section_offsets[section.0].address + symbol.value
                }
                _ => symbol.value,
            };

            let n_strx = symbol_offsets[index]
                .str_id
                .map(|id| strtab.get_offset(id))
                .unwrap_or(0);

            let nlist = &Nlist {
                n_strx,
                n_type,
                n_sect: n_sect as u8,
                n_desc,
                n_value,
            };
            encoder.nlist(buffer, nlist);
        }

        // Write strtab.
        debug_assert_eq!(strtab_offset, buffer.count());
        buffer.write_bytes(&strtab_data);

        debug_assert_eq!(reserved_len, buffer.count());

        Ok(())
    }
}
