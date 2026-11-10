use core::mem;

use crate::write::string::*;
use crate::write::util::*;
use crate::write::*;

use crate::xcoff;

#[derive(Default, Clone, Copy)]
struct SectionOffsets {
    address: u64,
    data_offset: u64,
    reloc_offset: u64,
}

#[derive(Default, Clone, Copy)]
struct SymbolOffsets {
    index: usize,
    str_id: Option<StringId>,
    aux_count: u8,
    n_type: xcoff::SymbolType,
    n_sclass: xcoff::SymbolClass,
    x_smtyp: xcoff::CsectAuxSmtyp,
    x_smclas: xcoff::CsectAuxClass,
    containing_csect: Option<SymbolId>,
}

impl<'a> Object<'a> {
    pub(crate) fn xcoff_section_info(
        &self,
        section: StandardSection,
    ) -> (&'static [u8], &'static [u8], SectionKind, SectionFlags) {
        match section {
            StandardSection::Text => (&[], &b".text"[..], SectionKind::Text, SectionFlags::None),
            StandardSection::Data => (&[], &b".data"[..], SectionKind::Data, SectionFlags::None),
            StandardSection::ReadOnlyData
            | StandardSection::ReadOnlyDataWithRel
            | StandardSection::ReadOnlyString => (
                &[],
                &b".rdata"[..],
                SectionKind::ReadOnlyData,
                SectionFlags::None,
            ),
            StandardSection::UninitializedData => (
                &[],
                &b".bss"[..],
                SectionKind::UninitializedData,
                SectionFlags::None,
            ),
            StandardSection::Tls => (&[], &b".tdata"[..], SectionKind::Tls, SectionFlags::None),
            StandardSection::UninitializedTls => (
                &[],
                &b".tbss"[..],
                SectionKind::UninitializedTls,
                SectionFlags::None,
            ),
            StandardSection::TlsVariables => {
                // Unsupported section.
                (&[], &[], SectionKind::Unknown, SectionFlags::None)
            }
            StandardSection::GnuProperty => {
                // Unsupported section.
                (&[], &[], SectionKind::Unknown, SectionFlags::None)
            }
            StandardSection::EhFrame => {
                // Unsupported section.
                (&[], &[], SectionKind::Unknown, SectionFlags::None)
            }
        }
    }

    pub(crate) fn xcoff_section_flags(&self, section: &Section<'_>) -> SectionFlags {
        let s_flags = match section.kind {
            SectionKind::Text
            | SectionKind::ReadOnlyData
            | SectionKind::ReadOnlyString
            | SectionKind::ReadOnlyDataWithRel => xcoff::STYP_TEXT,
            SectionKind::Data => xcoff::STYP_DATA,
            SectionKind::UninitializedData => xcoff::STYP_BSS,
            SectionKind::Tls => xcoff::STYP_TDATA,
            SectionKind::UninitializedTls => xcoff::STYP_TBSS,
            SectionKind::OtherString => xcoff::STYP_INFO,
            SectionKind::Debug | SectionKind::DebugString => xcoff::STYP_DEBUG,
            SectionKind::Other | SectionKind::Metadata => xcoff::STYP_REG,
            SectionKind::Note
            | SectionKind::Linker
            | SectionKind::Unknown
            | SectionKind::TlsVariables => {
                return SectionFlags::None;
            }
        }
        .into();
        SectionFlags::Xcoff { s_flags }
    }

    pub(crate) fn xcoff_symbol_flags(&self, symbol: &Symbol) -> SymbolFlags<SectionId, SymbolId> {
        let n_sclass = match symbol.kind {
            SymbolKind::File => xcoff::C_FILE,
            SymbolKind::Text | SymbolKind::Data | SymbolKind::Tls => {
                if symbol.is_local() {
                    xcoff::C_STAT
                } else if symbol.weak {
                    xcoff::C_WEAKEXT
                } else {
                    xcoff::C_EXT
                }
            }
            SymbolKind::Section | SymbolKind::Label | SymbolKind::Unknown => {
                return SymbolFlags::None;
            }
        };
        let n_type = if (symbol.scope == SymbolScope::Linkage)
            && (n_sclass == xcoff::C_EXT
                || n_sclass == xcoff::C_WEAKEXT
                || n_sclass == xcoff::C_HIDEXT)
        {
            xcoff::SYM_V_HIDDEN.into()
        } else {
            xcoff::SymbolType(0)
        };
        let (x_smtyp, x_smclas) = if n_sclass == xcoff::C_EXT
            || n_sclass == xcoff::C_WEAKEXT
            || n_sclass == xcoff::C_HIDEXT
        {
            let section_kind = if let SymbolSection::Section(id) = symbol.section {
                self.sections[id.0].kind
            } else {
                SectionKind::Unknown
            };
            match symbol.kind {
                SymbolKind::Text => (xcoff::XTY_SD, xcoff::XMC_PR),
                SymbolKind::Data => {
                    if section_kind == SectionKind::UninitializedData {
                        (xcoff::XTY_CM, xcoff::XMC_BS)
                    } else if section_kind == SectionKind::ReadOnlyData {
                        (xcoff::XTY_SD, xcoff::XMC_RO)
                    } else {
                        (xcoff::XTY_SD, xcoff::XMC_RW)
                    }
                }
                SymbolKind::Tls => {
                    if section_kind == SectionKind::UninitializedTls {
                        (xcoff::XTY_CM, xcoff::XMC_UL)
                    } else {
                        (xcoff::XTY_SD, xcoff::XMC_TL)
                    }
                }
                _ => {
                    return SymbolFlags::None;
                }
            }
        } else {
            (xcoff::CsectAuxType(0), xcoff::CsectAuxClass(0))
        };
        SymbolFlags::Xcoff {
            n_type,
            n_sclass,
            x_smtyp: x_smtyp.into(),
            x_smclas,
            containing_csect: None,
        }
    }

    pub(crate) fn xcoff_translate_relocation(
        &mut self,
        reloc: &mut RelocationInternal,
    ) -> Result<()> {
        let (kind, _encoding, size) = if let RelocationFlags::Generic {
            kind,
            encoding,
            size,
        } = reloc.flags
        {
            (kind, encoding, size)
        } else {
            return Ok(());
        };

        let r_rtype = match kind {
            RelocationKind::Absolute => xcoff::R_POS,
            RelocationKind::Relative => xcoff::R_REL,
            RelocationKind::Got => xcoff::R_TOC,
            _ => {
                return Err(Error(format!("unimplemented relocation {:?}", reloc)));
            }
        };
        let r_rsize = size - 1;
        reloc.flags = RelocationFlags::Xcoff { r_rtype, r_rsize };
        Ok(())
    }

    pub(crate) fn xcoff_adjust_addend(
        &mut self,
        relocation: &mut RelocationInternal,
    ) -> Result<bool> {
        let r_rtype = if let RelocationFlags::Xcoff { r_rtype, .. } = relocation.flags {
            r_rtype
        } else {
            return Err(Error(format!("invalid relocation flags {:?}", relocation)));
        };
        if r_rtype == xcoff::R_REL {
            relocation.addend += 4;
        }
        Ok(true)
    }

    pub(crate) fn xcoff_relocation_size(&self, reloc: &RelocationInternal) -> Result<u8> {
        let r_rsize = if let RelocationFlags::Xcoff { r_rsize, .. } = reloc.flags {
            r_rsize
        } else {
            return Err(Error(format!("unexpected relocation {:?}", reloc)));
        };
        Ok(r_rsize + 1)
    }

    pub(crate) fn xcoff_write(&self, buffer: &mut dyn WritableBuffer) -> Result<()> {
        let is_64 = match self.architecture.address_size().unwrap() {
            AddressSize::U8 | AddressSize::U16 | AddressSize::U32 => false,
            AddressSize::U64 => true,
        };

        let (hdr_size, sechdr_size, rel_size, sym_size) = if is_64 {
            (
                mem::size_of::<xcoff::FileHeader64>() as u64,
                mem::size_of::<xcoff::SectionHeader64>() as u64,
                mem::size_of::<xcoff::Rel64>() as u64,
                mem::size_of::<xcoff::Symbol64>() as u64,
            )
        } else {
            (
                mem::size_of::<xcoff::FileHeader32>() as u64,
                mem::size_of::<xcoff::SectionHeader32>() as u64,
                mem::size_of::<xcoff::Rel32>() as u64,
                mem::size_of::<xcoff::Symbol32>() as u64,
            )
        };

        // Calculate offsets and build strtab.
        let mut offset = 0;
        let mut strtab = StringTable::default();
        // We place the shared address 0 immediately after the section header table.
        let mut address = 0;

        // XCOFF file header.
        offset += hdr_size;
        // Section headers.
        offset += self.sections.len() as u64 * sechdr_size;

        // Calculate size of section data.
        let mut section_offsets = vec![SectionOffsets::default(); self.sections.len()];
        for (index, section) in self.sections.iter().enumerate() {
            let len = section.data.len() as u64;
            let sectype = section.kind;
            // Section address should be 0 for all sections except the .text, .data, and .bss sections.
            if sectype == SectionKind::Data
                || sectype == SectionKind::Text
                || sectype == SectionKind::UninitializedData
            {
                section_offsets[index].address = address;
                address += len;
                address = align(address, 4);
            } else {
                section_offsets[index].address = 0;
            }
            if len != 0 {
                // Set the default section alignment as 4.
                offset = align(offset, 4);
                section_offsets[index].data_offset = offset;
                offset += len;
            } else {
                section_offsets[index].data_offset = 0;
            }
        }

        // Calculate size of relocations.
        for (index, section) in self.sections.iter().enumerate() {
            let count = section.relocations.len() as u64;
            if count != 0 {
                section_offsets[index].reloc_offset = offset;
                offset += count * rel_size;
            } else {
                section_offsets[index].reloc_offset = 0;
            }
        }

        // Calculate size of symbols.
        let mut file_str_id = None;
        let mut symbol_offsets = vec![SymbolOffsets::default(); self.symbols.len()];
        let mut symtab_count = 0;
        for (index, symbol) in self.symbols.iter().enumerate() {
            symbol_offsets[index].index = symtab_count;
            symtab_count += 1;

            let SymbolFlags::Xcoff {
                n_type,
                n_sclass,
                x_smtyp,
                x_smclas,
                containing_csect,
            } = self.symbol_flags(symbol)
            else {
                return Err(Error(format!(
                    "unimplemented symbol `{}` kind {:?}",
                    symbol.name().unwrap_or(""),
                    symbol.kind
                )));
            };
            symbol_offsets[index].n_type = n_type;
            symbol_offsets[index].n_sclass = n_sclass;
            symbol_offsets[index].x_smtyp = x_smtyp;
            symbol_offsets[index].x_smclas = x_smclas;
            symbol_offsets[index].containing_csect = containing_csect;

            if n_sclass == xcoff::C_FILE {
                if is_64 && file_str_id.is_none() {
                    file_str_id = Some(strtab.add(b".file"));
                }
                if symbol.name.len() > 8 {
                    symbol_offsets[index].str_id = Some(strtab.add(&symbol.name));
                }
            } else if is_64 || symbol.name.len() > 8 {
                symbol_offsets[index].str_id = Some(strtab.add(&symbol.name));
            }

            symbol_offsets[index].aux_count = 0;
            match n_sclass {
                xcoff::C_FILE => {
                    symbol_offsets[index].aux_count = 1;
                    symtab_count += 1;
                }
                xcoff::C_EXT | xcoff::C_WEAKEXT | xcoff::C_HIDEXT => {
                    symbol_offsets[index].aux_count = 1;
                    symtab_count += 1;
                }
                // TODO: support auxiliary entry for other types of symbol.
                _ => {}
            }
        }
        let symtab_offset = offset;
        let symtab_len = symtab_count as u64 * sym_size;
        offset += symtab_len;

        // Calculate size of strtab.
        let strtab_offset = offset;
        let mut strtab_data = Vec::new();
        // First 4 bytes of strtab are the length.
        let strtab_len = strtab.write(&mut strtab_data, 4)?;
        offset += strtab_len as u64;

        // Start writing.
        buffer
            .reserve(offset)
            .map_err(|_| Error(String::from("Cannot allocate buffer")))?;
        let buffer = &mut CountingBuffer::new(buffer);

        // Write file header.
        let f_flags = match self.flags {
            FileFlags::Xcoff { f_flags } => f_flags,
            _ => xcoff::FileFlags(0),
        };
        if is_64 {
            let header = xcoff::FileHeader64 {
                f_magic: xcoff::MAGIC_64.into(),
                f_nscns: (self.sections.len() as u16).into(),
                f_timdat: 0.into(),
                f_symptr: symtab_offset.into(),
                f_nsyms: (symtab_count as u32).into(),
                f_opthdr: 0.into(),
                f_flags: f_flags.into(),
            };
            buffer.write_pod(&header);
        } else {
            let header = xcoff::FileHeader32 {
                f_magic: xcoff::MAGIC_32.into(),
                f_nscns: (self.sections.len() as u16).into(),
                f_timdat: 0.into(),
                f_symptr: (symtab_offset as u32).into(),
                f_nsyms: (symtab_count as u32).into(),
                f_opthdr: 0.into(),
                f_flags: f_flags.into(),
            };
            buffer.write_pod(&header);
        }

        // Write section headers.
        for (index, section) in self.sections.iter().enumerate() {
            let mut sectname = [0; 8];
            sectname
                .get_mut(..section.name.len())
                .ok_or_else(|| {
                    Error(format!(
                        "section name `{}` is too long",
                        section.name().unwrap_or(""),
                    ))
                })?
                .copy_from_slice(&section.name);
            let SectionFlags::Xcoff { s_flags } = self.section_flags(section) else {
                return Err(Error(format!(
                    "unimplemented section `{}` kind {:?}",
                    section.name().unwrap_or(""),
                    section.kind
                )));
            };
            if is_64 {
                let section_header = xcoff::SectionHeader64 {
                    s_name: sectname,
                    s_paddr: section_offsets[index].address.into(),
                    // This field has the same value as the s_paddr field.
                    s_vaddr: section_offsets[index].address.into(),
                    s_size: (section.data.len() as u64).into(),
                    s_scnptr: (section_offsets[index].data_offset as u64).into(),
                    s_relptr: (section_offsets[index].reloc_offset as u64).into(),
                    s_lnnoptr: 0.into(),
                    s_nreloc: (section.relocations.len() as u32).into(),
                    s_nlnno: 0.into(),
                    s_flags: s_flags.into(),
                    s_reserve: 0.into(),
                };
                buffer.write_pod(&section_header);
            } else {
                let section_header = xcoff::SectionHeader32 {
                    s_name: sectname,
                    s_paddr: (section_offsets[index].address as u32).into(),
                    // This field has the same value as the s_paddr field.
                    s_vaddr: (section_offsets[index].address as u32).into(),
                    s_size: (section.data.len() as u32).into(),
                    s_scnptr: (section_offsets[index].data_offset as u32).into(),
                    s_relptr: (section_offsets[index].reloc_offset as u32).into(),
                    s_lnnoptr: 0.into(),
                    // TODO: If more than 65,534 relocation entries are required, the field
                    // value will be 65535, and an STYP_OVRFLO section header will contain
                    // the actual count of relocation entries in the s_paddr field.
                    s_nreloc: (section.relocations.len() as u16).into(),
                    s_nlnno: 0.into(),
                    s_flags: s_flags.into(),
                };
                buffer.write_pod(&section_header);
            }
        }

        // Write section data.
        for (index, section) in self.sections.iter().enumerate() {
            let len = section.data.len();
            if len != 0 {
                buffer.write_align(4);
                debug_assert_eq!(section_offsets[index].data_offset, buffer.count());
                buffer.write_bytes(&section.data);
            }
        }

        // Write relocations.
        for (index, section) in self.sections.iter().enumerate() {
            if !section.relocations.is_empty() {
                debug_assert_eq!(section_offsets[index].reloc_offset, buffer.count());
                for reloc in &section.relocations {
                    let (r_rtype, r_rsize) =
                        if let RelocationFlags::Xcoff { r_rtype, r_rsize } = reloc.flags {
                            (r_rtype, r_rsize)
                        } else {
                            return Err(Error("invalid relocation flags".into()));
                        };
                    if is_64 {
                        let xcoff_rel = xcoff::Rel64 {
                            r_vaddr: reloc.offset.into(),
                            r_symndx: (symbol_offsets[reloc.symbol.0].index as u32).into(),
                            r_rsize,
                            r_rtype,
                        };
                        buffer.write_pod(&xcoff_rel);
                    } else {
                        let xcoff_rel = xcoff::Rel32 {
                            r_vaddr: (reloc.offset as u32).into(),
                            r_symndx: (symbol_offsets[reloc.symbol.0].index as u32).into(),
                            r_rsize,
                            r_rtype,
                        };
                        buffer.write_pod(&xcoff_rel);
                    }
                }
            }
        }

        // Write symbols.
        debug_assert_eq!(symtab_offset, buffer.count());
        for (index, symbol) in self.symbols.iter().enumerate() {
            let n_value = if let SymbolSection::Section(id) = symbol.section {
                section_offsets[id.0].address + symbol.value
            } else {
                symbol.value
            };
            let n_scnum = match symbol.section {
                SymbolSection::None => {
                    debug_assert_eq!(symbol.kind, SymbolKind::File);
                    xcoff::N_DEBUG
                }
                SymbolSection::Undefined | SymbolSection::Common => xcoff::N_UNDEF,
                SymbolSection::Absolute => xcoff::N_ABS,
                SymbolSection::Section(id) => xcoff::SymbolSection(id.0 as i16 + 1),
            };
            let n_type = symbol_offsets[index].n_type;
            let n_sclass = symbol_offsets[index].n_sclass;
            let n_numaux = symbol_offsets[index].aux_count;
            if is_64 {
                let str_id = if n_sclass == xcoff::C_FILE {
                    file_str_id.unwrap()
                } else {
                    symbol_offsets[index].str_id.unwrap()
                };
                let xcoff_sym = xcoff::Symbol64 {
                    n_value: n_value.into(),
                    n_offset: strtab.get_offset(str_id).into(),
                    n_scnum: n_scnum.into(),
                    n_type: n_type.into(),
                    n_sclass,
                    n_numaux,
                };
                buffer.write_pod(&xcoff_sym);
            } else {
                let mut sym_name = [0; 8];
                if n_sclass == xcoff::C_FILE {
                    sym_name[..5].copy_from_slice(b".file");
                } else if symbol.name.len() <= 8 {
                    sym_name[..symbol.name.len()].copy_from_slice(&symbol.name[..]);
                } else {
                    let str_offset = strtab.get_offset(symbol_offsets[index].str_id.unwrap());
                    sym_name[4..8].copy_from_slice(&u32::to_be_bytes(str_offset));
                }
                let xcoff_sym = xcoff::Symbol32 {
                    n_name: sym_name,
                    n_value: (n_value as u32).into(),
                    n_scnum: n_scnum.into(),
                    n_type: n_type.into(),
                    n_sclass,
                    n_numaux,
                };
                buffer.write_pod(&xcoff_sym);
            }
            // Generate auxiliary entries.
            if n_sclass == xcoff::C_FILE {
                debug_assert_eq!(n_numaux, 1);
                let mut x_fname = [0; 8];
                if symbol.name.len() <= 8 {
                    x_fname[..symbol.name.len()].copy_from_slice(&symbol.name[..]);
                } else {
                    let str_offset = strtab.get_offset(symbol_offsets[index].str_id.unwrap());
                    x_fname[4..8].copy_from_slice(&u32::to_be_bytes(str_offset));
                }
                if is_64 {
                    let file_aux = xcoff::FileAux64 {
                        x_fname,
                        x_fpad: Default::default(),
                        x_ftype: xcoff::XFT_FN,
                        x_freserve: Default::default(),
                        x_auxtype: xcoff::AUX_FILE,
                    };
                    buffer.write_pod(&file_aux);
                } else {
                    let file_aux = xcoff::FileAux32 {
                        x_fname,
                        x_fpad: Default::default(),
                        x_ftype: xcoff::XFT_FN,
                        x_freserve: Default::default(),
                    };
                    buffer.write_pod(&file_aux);
                }
            } else if n_sclass == xcoff::C_EXT
                || n_sclass == xcoff::C_WEAKEXT
                || n_sclass == xcoff::C_HIDEXT
            {
                debug_assert_eq!(n_numaux, 1);
                let x_smtyp = symbol_offsets[index].x_smtyp;
                let x_smclas = symbol_offsets[index].x_smclas;
                let scnlen = if let Some(containing_csect) = symbol_offsets[index].containing_csect
                {
                    symbol_offsets[containing_csect.0].index as u64
                } else {
                    symbol.size
                };
                if is_64 {
                    let csect_aux = xcoff::CsectAux64 {
                        x_scnlen_lo: ((scnlen & 0xFFFFFFFF) as u32).into(),
                        x_scnlen_hi: (((scnlen >> 32) & 0xFFFFFFFF) as u32).into(),
                        x_parmhash: 0.into(),
                        x_snhash: 0.into(),
                        x_smtyp,
                        x_smclas,
                        pad: 0,
                        x_auxtype: xcoff::AUX_CSECT,
                    };
                    buffer.write_pod(&csect_aux);
                } else {
                    let csect_aux = xcoff::CsectAux32 {
                        x_scnlen: (scnlen as u32).into(),
                        x_parmhash: 0.into(),
                        x_snhash: 0.into(),
                        x_smtyp,
                        x_smclas,
                        x_stab: 0.into(),
                        x_snstab: 0.into(),
                    };
                    buffer.write_pod(&csect_aux);
                }
            }
        }

        // Write string table.
        debug_assert_eq!(strtab_offset, buffer.count());
        buffer.write_bytes(&u32::to_be_bytes(strtab_len));
        buffer.write_bytes(&strtab_data);

        debug_assert_eq!(offset, buffer.count());
        Ok(())
    }
}
