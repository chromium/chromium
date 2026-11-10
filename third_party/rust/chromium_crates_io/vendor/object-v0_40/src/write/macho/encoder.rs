use core::mem;

use crate::endian::*;
use crate::macho;
#[cfg(feature = "read_core")]
use crate::read;
use crate::write::util::align;
use crate::write::{Error, Result, StringTable, WritableBuffer, WritableBufferExt};

/// Native endian version of [`macho::MachHeader64`].
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub struct MachHeader {
    pub cputype: macho::CpuType,
    pub cpusubtype: macho::CpuSubtype,
    pub filetype: macho::FileType,
    pub ncmds: u32,
    pub sizeofcmds: u32,
    pub flags: macho::FileFlags,
}

#[cfg(feature = "read_core")]
impl MachHeader {
    /// Convert from a raw file header.
    pub fn from_raw<Mach: read::macho::MachHeader>(endian: Mach::Endian, header: &Mach) -> Self {
        MachHeader {
            cputype: header.cputype(endian),
            cpusubtype: header.cpusubtype(endian),
            filetype: header.filetype(endian),
            ncmds: header.ncmds(endian),
            sizeofcmds: header.sizeofcmds(endian),
            flags: header.flags(endian),
        }
    }
}

/// Native endian version of [`macho::SegmentCommand64`].
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub struct SegmentCommand {
    pub segname: [u8; 16],
    pub vmaddr: u64,
    pub vmsize: u64,
    pub fileoff: u64,
    pub filesize: u64,
    pub maxprot: macho::VmProt,
    pub initprot: macho::VmProt,
    pub nsects: u32,
    pub flags: macho::SegmentFlags,
}

#[cfg(feature = "read_core")]
impl SegmentCommand {
    /// Convert from a raw segment command.
    pub fn from_raw<S: read::macho::Segment>(endian: S::Endian, segment: &S) -> Self {
        SegmentCommand {
            segname: *segment.segname(),
            vmaddr: segment.vmaddr(endian).into(),
            vmsize: segment.vmsize(endian).into(),
            fileoff: segment.fileoff(endian).into(),
            filesize: segment.filesize(endian).into(),
            maxprot: segment.maxprot(endian),
            initprot: segment.initprot(endian),
            nsects: segment.nsects(endian),
            flags: segment.flags(endian),
        }
    }
}

/// Native endian version of [`macho::Section64`].
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub struct SectionHeader {
    pub sectname: [u8; 16],
    pub segname: [u8; 16],
    pub addr: u64,
    pub size: u64,
    pub offset: u32,
    pub align: u32,
    pub reloff: u32,
    pub nreloc: u32,
    pub flags: macho::SectionFlags,
    pub reserved1: u32,
    pub reserved2: u32,
    pub reserved3: u32,
}

#[cfg(feature = "read_core")]
impl SectionHeader {
    /// Convert from a raw section header.
    pub fn from_raw<S: read::macho::Section>(endian: S::Endian, section: &S) -> Self {
        SectionHeader {
            sectname: *section.sectname(),
            segname: *section.segname(),
            addr: section.addr(endian).into(),
            size: section.size(endian).into(),
            offset: section.offset(endian),
            align: section.align(endian),
            reloff: section.reloff(endian),
            nreloc: section.nreloc(endian),
            flags: section.flags(endian),
            reserved1: section.reserved1(endian),
            reserved2: section.reserved2(endian),
            reserved3: section.reserved3(endian),
        }
    }
}

/// Native endian version of [`macho::SymtabCommand`].
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub struct SymtabCommand {
    pub symoff: u32,
    pub nsyms: u32,
    pub stroff: u32,
    pub strsize: u32,
}

#[cfg(feature = "read_core")]
impl SymtabCommand {
    /// Convert from a raw symtab load command.
    pub fn from_raw<E: Endian>(endian: E, command: &macho::SymtabCommand<E>) -> Self {
        SymtabCommand {
            symoff: command.symoff.get(endian),
            nsyms: command.nsyms.get(endian),
            stroff: command.stroff.get(endian),
            strsize: command.strsize.get(endian),
        }
    }
}

/// Native endian version of [`macho::DysymtabCommand`].
#[allow(missing_docs)]
#[derive(Debug, Default, Clone)]
pub struct DysymtabCommand {
    pub ilocalsym: u32,
    pub nlocalsym: u32,
    pub iextdefsym: u32,
    pub nextdefsym: u32,
    pub iundefsym: u32,
    pub nundefsym: u32,
    pub tocoff: u32,
    pub ntoc: u32,
    pub modtaboff: u32,
    pub nmodtab: u32,
    pub extrefsymoff: u32,
    pub nextrefsyms: u32,
    pub indirectsymoff: u32,
    pub nindirectsyms: u32,
    pub extreloff: u32,
    pub nextrel: u32,
    pub locreloff: u32,
    pub nlocrel: u32,
}

#[cfg(feature = "read_core")]
impl DysymtabCommand {
    /// Convert from a raw dynamic symtab load command.
    pub fn from_raw<E: Endian>(endian: E, command: &macho::DysymtabCommand<E>) -> Self {
        DysymtabCommand {
            ilocalsym: command.ilocalsym.get(endian),
            nlocalsym: command.nlocalsym.get(endian),
            iextdefsym: command.iextdefsym.get(endian),
            nextdefsym: command.nextdefsym.get(endian),
            iundefsym: command.iundefsym.get(endian),
            nundefsym: command.nundefsym.get(endian),
            tocoff: command.tocoff.get(endian),
            ntoc: command.ntoc.get(endian),
            modtaboff: command.modtaboff.get(endian),
            nmodtab: command.nmodtab.get(endian),
            extrefsymoff: command.extrefsymoff.get(endian),
            nextrefsyms: command.nextrefsyms.get(endian),
            indirectsymoff: command.indirectsymoff.get(endian),
            nindirectsyms: command.nindirectsyms.get(endian),
            extreloff: command.extreloff.get(endian),
            nextrel: command.nextrel.get(endian),
            locreloff: command.locreloff.get(endian),
            nlocrel: command.nlocrel.get(endian),
        }
    }
}

/// Native endian version of [`macho::Nlist64`].
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub struct Nlist {
    pub n_strx: u32,
    pub n_type: macho::SymbolFlags,
    pub n_sect: u8,
    pub n_desc: macho::SymbolDesc,
    pub n_value: u64,
}

#[cfg(feature = "read_core")]
impl Nlist {
    /// Convert from a raw symbol.
    pub fn from_raw<N: read::macho::Nlist>(endian: N::Endian, nlist: &N) -> Self {
        Nlist {
            n_strx: nlist.n_strx(endian),
            n_type: nlist.n_type(),
            n_sect: nlist.n_sect(),
            n_desc: nlist.n_desc(endian),
            n_value: nlist.n_value(endian).into(),
        }
    }
}

/// Native endian version of [`macho::DylibCommand`].
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub struct DylibCommand<'data> {
    pub cmd: macho::LoadCommandType,
    pub name: &'data [u8],
    pub timestamp: u32,
    pub current_version: macho::Version,
    pub compatibility_version: macho::Version,
}

#[cfg(feature = "read_core")]
impl<'data> DylibCommand<'data> {
    /// Convert from a raw dylib load command.
    ///
    /// `name` is the string referenced by the `dylib.name` field. It can be obtained
    /// using [`read::macho::LoadCommandData::string`].
    pub fn from_raw<E: Endian>(
        endian: E,
        command: &macho::DylibCommand<E>,
        name: &'data [u8],
    ) -> Self {
        DylibCommand {
            cmd: command.cmd.get(endian),
            name,
            timestamp: command.dylib.timestamp.get(endian),
            current_version: command.dylib.current_version.get(endian),
            compatibility_version: command.dylib.compatibility_version.get(endian),
        }
    }
}

/// Native endian version of [`macho::BuildVersionCommand`].
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub struct BuildVersionCommand {
    pub platform: macho::Platform,
    pub minos: macho::Version,
    pub sdk: macho::Version,
    pub ntools: u32,
}

#[cfg(feature = "read_core")]
impl BuildVersionCommand {
    /// Convert from a raw build version load command.
    pub fn from_raw<E: Endian>(endian: E, command: &macho::BuildVersionCommand<E>) -> Self {
        BuildVersionCommand {
            platform: command.platform.get(endian),
            minos: command.minos.get(endian),
            sdk: command.sdk.get(endian),
            ntools: command.ntools.get(endian),
        }
    }
}

/// Native endian version of [`macho::DyldInfoCommand`].
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub struct DyldInfoCommand {
    pub rebase_off: u32,
    pub rebase_size: u32,
    pub bind_off: u32,
    pub bind_size: u32,
    pub weak_bind_off: u32,
    pub weak_bind_size: u32,
    pub lazy_bind_off: u32,
    pub lazy_bind_size: u32,
    pub export_off: u32,
    pub export_size: u32,
}

#[cfg(feature = "read_core")]
impl DyldInfoCommand {
    /// Convert from a raw dyld info load command.
    pub fn from_raw<E: Endian>(endian: E, command: &macho::DyldInfoCommand<E>) -> Self {
        DyldInfoCommand {
            rebase_off: command.rebase_off.get(endian),
            rebase_size: command.rebase_size.get(endian),
            bind_off: command.bind_off.get(endian),
            bind_size: command.bind_size.get(endian),
            weak_bind_off: command.weak_bind_off.get(endian),
            weak_bind_size: command.weak_bind_size.get(endian),
            lazy_bind_off: command.lazy_bind_off.get(endian),
            lazy_bind_size: command.lazy_bind_size.get(endian),
            export_off: command.export_off.get(endian),
            export_size: command.export_size.get(endian),
        }
    }
}

/// A helper for encoding headers and data when writing a Mach-O file.
///
/// None of the methods check for overflow when truncating file offsets, or when
/// truncating addresses for 32-bit Mach-O. It is recommended that the caller keep
/// track of the largest address and file offset, and perform a single overflow check.
#[derive(Debug, Clone, Copy)]
pub struct Encoder<E: Endian> {
    endian: E,
    is_64: bool,
}

impl<E: Endian> Encoder<E> {
    /// Create a new `Encoder` for the given endianness and pointer width.
    pub fn new(endian: E, is_64: bool) -> Self {
        Encoder { endian, is_64 }
    }

    /// Return the endianness.
    pub fn endian(self) -> E {
        self.endian
    }

    /// Return true for 64-bit Mach-O.
    pub fn is_64(self) -> bool {
        self.is_64
    }

    /// Return the size in bytes of an address.
    ///
    /// This should be used as the file offset alignment for various structures such as
    /// load commands, symbols, and relocations.
    pub fn address_size(self) -> u64 {
        if self.is_64 { 8 } else { 4 }
    }

    /// Return the size of the file header.
    pub fn mach_header_size(self) -> u64 {
        if self.is_64 {
            mem::size_of::<macho::MachHeader64<Endianness>>() as u64
        } else {
            mem::size_of::<macho::MachHeader32<Endianness>>() as u64
        }
    }

    /// Write the file header.
    ///
    /// The buffer should be at the start of the file.
    pub fn mach_header<W: WritableBuffer + ?Sized>(self, buffer: &mut W, header: &MachHeader) {
        let endian = self.endian;
        if self.is_64 {
            let magic = if endian.is_big_endian() {
                macho::MH_MAGIC_64
            } else {
                macho::MH_CIGAM_64
            };
            let data = &macho::MachHeader64 {
                magic: U32::new(BigEndian, magic),
                cputype: U32::new(endian, header.cputype),
                cpusubtype: U32::new(endian, header.cpusubtype),
                filetype: U32::new(endian, header.filetype),
                ncmds: U32::new(endian, header.ncmds),
                sizeofcmds: U32::new(endian, header.sizeofcmds),
                flags: U32::new(endian, header.flags),
                reserved: U32::default(),
            };
            buffer.write_pod(data);
        } else {
            let magic = if endian.is_big_endian() {
                macho::MH_MAGIC
            } else {
                macho::MH_CIGAM
            };
            let data = &macho::MachHeader32 {
                magic: U32::new(BigEndian, magic),
                cputype: U32::new(endian, header.cputype),
                cpusubtype: U32::new(endian, header.cpusubtype),
                filetype: U32::new(endian, header.filetype),
                ncmds: U32::new(endian, header.ncmds),
                sizeofcmds: U32::new(endian, header.sizeofcmds),
                flags: U32::new(endian, header.flags),
            };
            buffer.write_pod(data);
        }
    }

    /// Return the size of a raw load command.
    ///
    /// `data_size` is the number of bytes following the common load command header.
    pub fn load_command_size(self, data_size: u64) -> u64 {
        mem::size_of::<macho::LoadCommand<Endianness>>() as u64 + data_size
    }

    /// Write a raw load command.
    ///
    /// `data` is the bytes following the common load command header.
    ///
    /// No overflow check is performed for the command size.
    pub fn load_command<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        cmd: macho::LoadCommandType,
        data: &[u8],
    ) {
        let endian = self.endian;
        let cmdsize = (mem::size_of::<macho::LoadCommand<Endianness>>() + data.len()) as u32;
        let command = &macho::LoadCommand {
            cmd: U32::new(endian, cmd),
            cmdsize: U32::new(endian, cmdsize),
        };
        buffer.write_pod(command);
        buffer.write_bytes(data);
    }

    /// Return the size of a segment command.
    pub fn segment_command_size(self, nsects: u32) -> u64 {
        (if self.is_64 {
            mem::size_of::<macho::SegmentCommand64<Endianness>>() as u64
        } else {
            mem::size_of::<macho::SegmentCommand32<Endianness>>() as u64
        }) + u64::from(nsects) * self.section_header_size()
    }

    /// Write a segment command.
    ///
    /// No overflow check is performed when truncating `vmaddr`, `vmsize`,
    /// `fileoffset`, and `filesize` for 32-bit Mach-O.
    pub fn segment_command<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        segment: &SegmentCommand,
    ) {
        let endian = self.endian;
        let cmdsize = self.segment_command_size(segment.nsects) as u32;
        if self.is_64 {
            let data = &macho::SegmentCommand64 {
                cmd: U32::new(endian, macho::LC_SEGMENT_64),
                cmdsize: U32::new(endian, cmdsize),
                segname: segment.segname,
                vmaddr: U64::new(endian, segment.vmaddr),
                vmsize: U64::new(endian, segment.vmsize),
                fileoff: U64::new(endian, segment.fileoff),
                filesize: U64::new(endian, segment.filesize),
                maxprot: U32::new(endian, segment.maxprot),
                initprot: U32::new(endian, segment.initprot),
                nsects: U32::new(endian, segment.nsects),
                flags: U32::new(endian, segment.flags),
            };
            buffer.write_pod(data);
        } else {
            let data = &macho::SegmentCommand32 {
                cmd: U32::new(endian, macho::LC_SEGMENT),
                cmdsize: U32::new(endian, cmdsize),
                segname: segment.segname,
                vmaddr: U32::new(endian, segment.vmaddr as u32),
                vmsize: U32::new(endian, segment.vmsize as u32),
                fileoff: U32::new(endian, segment.fileoff as u32),
                filesize: U32::new(endian, segment.filesize as u32),
                maxprot: U32::new(endian, segment.maxprot),
                initprot: U32::new(endian, segment.initprot),
                nsects: U32::new(endian, segment.nsects),
                flags: U32::new(endian, segment.flags),
            };
            buffer.write_pod(data);
        }
    }

    /// Return the size of a section header.
    pub fn section_header_size(self) -> u64 {
        if self.is_64 {
            mem::size_of::<macho::Section64<Endianness>>() as u64
        } else {
            mem::size_of::<macho::Section32<Endianness>>() as u64
        }
    }

    /// Write a section header.
    ///
    /// No overflow check is performed when truncating `addr` and `size`
    /// for 32-bit Mach-O.
    pub fn section_header<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        section: &SectionHeader,
    ) {
        let endian = self.endian;
        if self.is_64 {
            let data = &macho::Section64 {
                sectname: section.sectname,
                segname: section.segname,
                addr: U64::new(endian, section.addr),
                size: U64::new(endian, section.size),
                offset: U32::new(endian, section.offset),
                align: U32::new(endian, section.align),
                reloff: U32::new(endian, section.reloff),
                nreloc: U32::new(endian, section.nreloc),
                flags: U32::new(endian, section.flags),
                reserved1: U32::new(endian, section.reserved1),
                reserved2: U32::new(endian, section.reserved2),
                reserved3: U32::new(endian, section.reserved3),
            };
            buffer.write_pod(data);
        } else {
            let data = &macho::Section32 {
                sectname: section.sectname,
                segname: section.segname,
                addr: U32::new(endian, section.addr as u32),
                size: U32::new(endian, section.size as u32),
                offset: U32::new(endian, section.offset),
                align: U32::new(endian, section.align),
                reloff: U32::new(endian, section.reloff),
                nreloc: U32::new(endian, section.nreloc),
                flags: U32::new(endian, section.flags),
                reserved1: U32::new(endian, section.reserved1),
                reserved2: U32::new(endian, section.reserved2),
            };
            buffer.write_pod(data);
        }
    }

    /// Return the size of a relocation.
    pub fn relocation_size(self) -> u64 {
        mem::size_of::<macho::Relocation<Endianness>>() as u64
    }

    /// Write a relocation.
    ///
    /// No overflow check is performed when truncating `r_symbolnum` to 24 bits.
    pub fn relocation<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        rel: &macho::RelocationInfo,
    ) {
        buffer.write_pod(&rel.relocation(self.endian));
    }

    /// Return the size of a symtab load command.
    pub fn symtab_command_size(self) -> u64 {
        mem::size_of::<macho::SymtabCommand<Endianness>>() as u64
    }

    /// Write a symtab load command.
    pub fn symtab_command<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        symtab: &SymtabCommand,
    ) {
        let endian = self.endian;
        let data = &macho::SymtabCommand {
            cmd: U32::new(endian, macho::LC_SYMTAB),
            cmdsize: U32::new(endian, self.symtab_command_size() as u32),
            symoff: U32::new(endian, symtab.symoff),
            nsyms: U32::new(endian, symtab.nsyms),
            stroff: U32::new(endian, symtab.stroff),
            strsize: U32::new(endian, symtab.strsize),
        };
        buffer.write_pod(data);
    }

    /// Return the size of a symbol.
    pub fn nlist_size(self) -> u64 {
        if self.is_64 {
            mem::size_of::<macho::Nlist64<Endianness>>() as u64
        } else {
            mem::size_of::<macho::Nlist32<Endianness>>() as u64
        }
    }

    /// Write a symbol.
    ///
    /// No overflow check is performed when truncating `n_value` for 32-bit Mach-O.
    pub fn nlist<W: WritableBuffer + ?Sized>(self, buffer: &mut W, nlist: &Nlist) {
        let endian = self.endian;
        if self.is_64 {
            let data = &macho::Nlist64 {
                n_strx: U32::new(endian, nlist.n_strx),
                n_type: nlist.n_type,
                n_sect: nlist.n_sect,
                n_desc: U16::new(endian, nlist.n_desc),
                n_value: U64::new(endian, nlist.n_value),
            };
            buffer.write_pod(data);
        } else {
            let data = &macho::Nlist32 {
                n_strx: U32::new(endian, nlist.n_strx),
                n_type: nlist.n_type,
                n_sect: nlist.n_sect,
                n_desc: U16::new(endian, nlist.n_desc),
                n_value: U32::new(endian, nlist.n_value as u32),
            };
            buffer.write_pod(data);
        }
    }

    /// Write the data for a string table.
    ///
    /// The string table data is padded to a multiple of the address size.
    ///
    /// Returns the length of the written data.
    pub fn strtab<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        strtab: &mut StringTable<'_>,
    ) -> Result<u32> {
        buffer.write_bytes(&[0]);
        let len = strtab.write(buffer, 1)? as u64;
        let aligned_len = align(len, self.address_size());
        buffer.write_zeros(aligned_len - len);
        u32::try_from(aligned_len).map_err(|_| Error("string table size overflow".into()))
    }

    /// Return the size of a dynamic symtab load command.
    pub fn dysymtab_command_size(self) -> u64 {
        mem::size_of::<macho::DysymtabCommand<Endianness>>() as u64
    }

    /// Write a dynamic symtab load command.
    pub fn dysymtab_command<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        dysymtab: &DysymtabCommand,
    ) {
        let endian = self.endian;
        let data = &macho::DysymtabCommand {
            cmd: U32::new(endian, macho::LC_DYSYMTAB),
            cmdsize: U32::new(endian, self.dysymtab_command_size() as u32),
            ilocalsym: U32::new(endian, dysymtab.ilocalsym),
            nlocalsym: U32::new(endian, dysymtab.nlocalsym),
            iextdefsym: U32::new(endian, dysymtab.iextdefsym),
            nextdefsym: U32::new(endian, dysymtab.nextdefsym),
            iundefsym: U32::new(endian, dysymtab.iundefsym),
            nundefsym: U32::new(endian, dysymtab.nundefsym),
            tocoff: U32::new(endian, dysymtab.tocoff),
            ntoc: U32::new(endian, dysymtab.ntoc),
            modtaboff: U32::new(endian, dysymtab.modtaboff),
            nmodtab: U32::new(endian, dysymtab.nmodtab),
            extrefsymoff: U32::new(endian, dysymtab.extrefsymoff),
            nextrefsyms: U32::new(endian, dysymtab.nextrefsyms),
            indirectsymoff: U32::new(endian, dysymtab.indirectsymoff),
            nindirectsyms: U32::new(endian, dysymtab.nindirectsyms),
            extreloff: U32::new(endian, dysymtab.extreloff),
            nextrel: U32::new(endian, dysymtab.nextrel),
            locreloff: U32::new(endian, dysymtab.locreloff),
            nlocrel: U32::new(endian, dysymtab.nlocrel),
        };
        buffer.write_pod(data);
    }

    /// Return the size of an indirect symbol for a dynamic symtab load command.
    pub fn indirect_symbol_size(self) -> u64 {
        mem::size_of::<U32<Endianness>>() as u64
    }

    /// Write an indirect symbol for a dynamic symtab load command.
    pub fn indirect_symbol<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        symbol: macho::IndirectSymbol,
    ) {
        buffer.write_u32(self.endian, symbol);
    }

    /// Return the size of a dylib command.
    ///
    /// `dylib_len` is the length of the dylib name excluding the null terminator.
    pub fn dylib_command_size(self, dylib_len: usize) -> u64 {
        align(
            mem::size_of::<macho::DylibCommand<Endianness>>() as u64 + dylib_len as u64 + 1,
            self.address_size(),
        )
    }

    /// Write a dylib command.
    pub fn dylib_command<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        dylib: &DylibCommand<'_>,
    ) {
        let cmdsize = self.dylib_command_size(dylib.name.len());
        let name_offset = mem::size_of::<macho::DylibCommand<Endianness>>() as u32;
        let endian = self.endian;
        let data = &macho::DylibCommand {
            cmd: U32::new(endian, dylib.cmd),
            cmdsize: U32::new(endian, cmdsize as u32),
            dylib: macho::Dylib {
                name: macho::LcStr {
                    offset: U32::new(endian, name_offset),
                },
                timestamp: U32::new(endian, dylib.timestamp),
                current_version: U32::new(endian, dylib.current_version),
                compatibility_version: U32::new(endian, dylib.compatibility_version),
            },
        };
        buffer.write_pod(data);
        buffer.write_bytes(dylib.name);
        let written = name_offset as u64 + dylib.name.len() as u64;
        buffer.write_zeros(cmdsize - written);
    }

    /// Return the size of a build version load command.
    pub fn build_version_command_size(self, ntools: u32) -> u64 {
        mem::size_of::<macho::BuildVersionCommand<Endianness>>() as u64
            + u64::from(ntools) * mem::size_of::<macho::BuildToolVersion<Endianness>>() as u64
    }

    /// Write a build version load command.
    pub fn build_version_command<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        version: &BuildVersionCommand,
    ) {
        let endian = self.endian;
        let data = &macho::BuildVersionCommand {
            cmd: U32::new(endian, macho::LC_BUILD_VERSION),
            cmdsize: U32::new(
                endian,
                self.build_version_command_size(version.ntools) as u32,
            ),
            platform: U32::new(endian, version.platform),
            minos: U32::new(endian, version.minos),
            sdk: U32::new(endian, version.sdk),
            ntools: U32::new(endian, version.ntools),
        };
        buffer.write_pod(data);
    }

    /// Write a build tool version.
    ///
    /// These should be written immediately following the load command.
    pub fn build_tool_version<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        tool: macho::Tool,
        version: macho::Version,
    ) {
        let endian = self.endian;
        let data = &macho::BuildToolVersion {
            tool: U32::new(endian, tool),
            version: U32::new(endian, version),
        };
        buffer.write_pod(data);
    }

    /// Return the size of a load command referencing linkedit data.
    pub fn linkedit_data_command_size(self) -> u64 {
        mem::size_of::<macho::LinkeditDataCommand<Endianness>>() as u64
    }

    /// Write a load command referencing linkedit data.
    pub fn linkedit_data_command<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        cmd: macho::LoadCommandType,
        dataoff: u32,
        datasize: u32,
    ) {
        let endian = self.endian;
        let data = &macho::LinkeditDataCommand {
            cmd: U32::new(endian, cmd),
            cmdsize: U32::new(endian, self.linkedit_data_command_size() as u32),
            dataoff: U32::new(endian, dataoff),
            datasize: U32::new(endian, datasize),
        };
        buffer.write_pod(data);
    }

    /// Return the size of a load command referencing dyld information.
    pub fn dyld_info_command_size(self) -> u64 {
        mem::size_of::<macho::DyldInfoCommand<Endianness>>() as u64
    }

    /// Write a load command referencing dyld information.
    pub fn dyld_info_command<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        cmd: macho::LoadCommandType,
        info: &DyldInfoCommand,
    ) {
        let endian = self.endian;
        let data = &macho::DyldInfoCommand {
            cmd: U32::new(endian, cmd),
            cmdsize: U32::new(endian, self.dyld_info_command_size() as u32),
            rebase_off: U32::new(endian, info.rebase_off),
            rebase_size: U32::new(endian, info.rebase_size),
            bind_off: U32::new(endian, info.bind_off),
            bind_size: U32::new(endian, info.bind_size),
            weak_bind_off: U32::new(endian, info.weak_bind_off),
            weak_bind_size: U32::new(endian, info.weak_bind_size),
            lazy_bind_off: U32::new(endian, info.lazy_bind_off),
            lazy_bind_size: U32::new(endian, info.lazy_bind_size),
            export_off: U32::new(endian, info.export_off),
            export_size: U32::new(endian, info.export_size),
        };
        buffer.write_pod(data);
    }
}

/// Native endian version of [`macho::CsCodeDirectoryV0`].
#[allow(missing_docs)]
#[derive(Debug, Default, Clone)]
pub struct CodeDirectory {
    pub length: u32,
    pub version: macho::CsVersion,
    pub flags: macho::CsFlags,
    pub hash_offset: u32,
    pub ident_offset: u32,
    pub n_special_slots: u32,
    pub n_code_slots: u32,
    pub code_limit: u64,
    pub hash_size: u8,
    pub hash_type: macho::CsHashType,
    pub platform: u8,
    pub page_size: u8,
    pub scatter_offset: u32,
    pub team_offset: u32,
    pub exec_seg_base: u64,
    pub exec_seg_limit: u64,
    pub exec_seg_flags: macho::CsExecSegFlags,
}

#[cfg(feature = "read_core")]
impl CodeDirectory {
    /// Convert from a raw code directory.
    ///
    /// Fields that are not present for the version of the code directory are set to 0.
    pub fn from_raw(code_directory: &read::macho::CodeDirectory<'_>) -> Self {
        let header = code_directory.header();
        let exec_seg = code_directory.exec_seg();
        CodeDirectory {
            length: header.length.get(BigEndian),
            version: header.version.get(BigEndian),
            flags: header.flags.get(BigEndian),
            hash_offset: header.hash_offset.get(BigEndian),
            ident_offset: header.ident_offset.get(BigEndian),
            n_special_slots: header.n_special_slots.get(BigEndian),
            n_code_slots: header.n_code_slots.get(BigEndian),
            code_limit: code_directory.code_limit(),
            hash_size: header.hash_size,
            hash_type: header.hash_type,
            platform: header.platform,
            page_size: header.page_size,
            scatter_offset: code_directory.scatter_offset().unwrap_or(0),
            team_offset: code_directory.team_offset().unwrap_or(0),
            exec_seg_base: exec_seg.map_or(0, |v| v.exec_seg_base.get(BigEndian)),
            exec_seg_limit: exec_seg.map_or(0, |v| v.exec_seg_limit.get(BigEndian)),
            exec_seg_flags: exec_seg.map_or(macho::CsExecSegFlags(0), |v| {
                v.exec_seg_flags.get(BigEndian)
            }),
        }
    }
}

/// A helper for encoding code signatures.
#[derive(Debug, Clone, Copy)]
pub struct CodeSignatureEncoder;

impl CodeSignatureEncoder {
    /// Return the size of a super blob header.
    pub fn super_blob_size(self) -> u32 {
        mem::size_of::<macho::CsSuperBlob>() as u32
    }

    /// Write a code signature super blob header.
    pub fn signature_super_blob<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        length: u32,
        count: u32,
    ) {
        let data = &macho::CsSuperBlob {
            magic: U32::new(BigEndian, macho::CSMAGIC_EMBEDDED_SIGNATURE),
            length: U32::new(BigEndian, length),
            count: U32::new(BigEndian, count),
        };
        buffer.write_pod(data);
    }

    /// Write a requirements super blob header.
    pub fn requirements_super_blob<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        length: u32,
        count: u32,
    ) {
        let data = &macho::CsSuperBlob {
            magic: U32::new(BigEndian, macho::CSMAGIC_REQUIREMENTS),
            length: U32::new(BigEndian, length),
            count: U32::new(BigEndian, count),
        };
        buffer.write_pod(data);
    }

    /// Return the size of a blob index entry.
    pub fn blob_index_size(self) -> u32 {
        mem::size_of::<macho::CsBlobIndex>() as u32
    }

    /// Write a code signature blob index entry.
    pub fn blob_index<W: WritableBuffer + ?Sized>(
        self,
        buffer: &mut W,
        slot: macho::CsSlot,
        offset: u32,
    ) {
        let data = &macho::CsBlobIndex {
            slot: U32::new(BigEndian, slot),
            offset: U32::new(BigEndian, offset),
        };
        buffer.write_pod(data);
    }

    /// Return the size of a generic blob header.
    pub fn generic_blob_size(self) -> u32 {
        mem::size_of::<macho::CsGenericBlob>() as u32
    }

    /// Write a generic blob header.
    pub fn generic_blob<W: WritableBuffer + ?Sized>(self, buffer: &mut W, magic: u32, length: u32) {
        let header = &macho::CsGenericBlob {
            magic: U32::new(BigEndian, magic),
            length: U32::new(BigEndian, length),
        };
        buffer.write_pod(header);
    }

    /// Return the size of a code directory header for the given version.
    ///
    /// This does not include data such as the identifier, team identifier,
    /// hash slots, or scatter vector.
    ///
    /// Only versions <= [`macho::CS_SUPPORTSEXECSEG`] are supported.
    pub fn code_directory_size(self, version: macho::CsVersion) -> u32 {
        debug_assert!(version <= macho::CS_SUPPORTSEXECSEG);
        let mut size = mem::size_of::<macho::CsCodeDirectoryV0>();
        if version >= macho::CS_SUPPORTSSCATTER {
            size += mem::size_of::<macho::CsCodeDirectoryV1>();
        }
        if version >= macho::CS_SUPPORTSTEAMID {
            size += mem::size_of::<macho::CsCodeDirectoryV2>();
        }
        if version >= macho::CS_SUPPORTSCODELIMIT64 {
            size += mem::size_of::<macho::CsCodeDirectoryV3>();
        }
        if version >= macho::CS_SUPPORTSEXECSEG {
            size += mem::size_of::<macho::CsCodeDirectoryV4>();
        }
        size as u32
    }

    /// Write a code directory header.
    ///
    /// The version is used to determine which fields to write.
    /// Only versions <= [`macho::CS_SUPPORTSEXECSEG`] are supported.
    pub fn code_directory<W: WritableBuffer + ?Sized>(self, buffer: &mut W, cd: &CodeDirectory) {
        debug_assert!(cd.version <= macho::CS_SUPPORTSEXECSEG);
        let (code_limit, code_limit64) = if cd.code_limit > u64::from(u32::MAX) {
            debug_assert!(cd.version >= macho::CS_SUPPORTSCODELIMIT64);
            (u32::MAX, cd.code_limit)
        } else {
            (cd.code_limit as u32, 0)
        };
        let v0 = macho::CsCodeDirectoryV0 {
            magic: U32::new(BigEndian, macho::CSMAGIC_CODEDIRECTORY),
            length: U32::new(BigEndian, cd.length),
            version: U32::new(BigEndian, cd.version),
            flags: U32::new(BigEndian, cd.flags),
            hash_offset: U32::new(BigEndian, cd.hash_offset),
            ident_offset: U32::new(BigEndian, cd.ident_offset),
            n_special_slots: U32::new(BigEndian, cd.n_special_slots),
            n_code_slots: U32::new(BigEndian, cd.n_code_slots),
            code_limit: U32::new(BigEndian, code_limit),
            hash_size: cd.hash_size,
            hash_type: cd.hash_type,
            platform: cd.platform,
            page_size: cd.page_size,
            spare2: U32::new(BigEndian, 0),
        };
        buffer.write_pod(&v0);
        if cd.version >= macho::CS_SUPPORTSSCATTER {
            let v1 = macho::CsCodeDirectoryV1 {
                scatter_offset: U32::new(BigEndian, cd.scatter_offset),
            };
            buffer.write_pod(&v1);
        }
        if cd.version >= macho::CS_SUPPORTSTEAMID {
            let v2 = macho::CsCodeDirectoryV2 {
                team_offset: U32::new(BigEndian, cd.team_offset),
            };
            buffer.write_pod(&v2);
        }
        if cd.version >= macho::CS_SUPPORTSCODELIMIT64 {
            let v3 = macho::CsCodeDirectoryV3 {
                spare3: U32::new(BigEndian, 0),
                code_limit64: U64::new(BigEndian, code_limit64),
            };
            buffer.write_pod(&v3);
        }
        if cd.version >= macho::CS_SUPPORTSEXECSEG {
            let v4 = macho::CsCodeDirectoryV4 {
                exec_seg_base: U64::new(BigEndian, cd.exec_seg_base),
                exec_seg_limit: U64::new(BigEndian, cd.exec_seg_limit),
                exec_seg_flags: U64::new(BigEndian, cd.exec_seg_flags),
            };
            buffer.write_pod(&v4);
        }
    }
}
