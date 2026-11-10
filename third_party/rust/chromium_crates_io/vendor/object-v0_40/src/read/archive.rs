//! Support for archive files.
//!
//! ## Example
//!  ```no_run
//! use object::{Object, ObjectSection};
//! use std::error::Error;
//! use std::fs;
//!
//! /// Reads an archive and displays the name of each member.
//! fn main() -> Result<(), Box<dyn Error>> {
//! #   #[cfg(feature = "std")] {
//!     let data = fs::read("path/to/binary")?;
//!     let file = object::read::archive::ArchiveFile::parse(&*data)?;
//!     for member in file.members() {
//!         let member = member?;
//!         println!("{}", String::from_utf8_lossy(member.name()));
//!     }
//! #   }
//!     Ok(())
//! }
//! ```

use core::convert::TryInto;
use core::slice;

use crate::endian::{BigEndian as BE, LittleEndian as LE, U16, U32, U64};
use crate::read::{self, Bytes, Error, ReadError, ReadRef};
use crate::{SkipDebugList, archive};

/// The kind of archive format.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum ArchiveKind {
    /// There are no special files that indicate the archive format.
    Unknown,
    /// The GNU (or System V) archive format.
    Gnu,
    /// The GNU (or System V) archive format with 64-bit symbol table.
    Gnu64,
    /// The BSD archive format.
    Bsd,
    /// The BSD archive format with 64-bit symbol table.
    ///
    /// This is used for Darwin.
    Bsd64,
    /// The Windows COFF archive format.
    Coff,
    /// The AIX big archive format.
    AixBig,
    /// The z/OS archive format.
    Zos,
}

/// The list of members in the archive.
#[derive(Debug, Clone, Copy)]
enum Members<'data> {
    Common {
        offset: u64,
        end_offset: u64,
    },
    AixBig {
        index: &'data [archive::AixMemberOffset],
    },
    Zos {
        offset: u64,
        end_offset: u64,
    },
}

/// A partially parsed archive file.
#[derive(Debug, Clone, Copy)]
pub struct ArchiveFile<'data, R: ReadRef<'data> = &'data [u8]> {
    data: SkipDebugList<R>,
    kind: ArchiveKind,
    members: Members<'data>,
    symbols: (u64, u64),
    names: &'data [u8],
    thin: bool,
}

impl<'data, R: ReadRef<'data>> ArchiveFile<'data, R> {
    /// Parse the archive header and special members.
    pub fn parse(data: R) -> read::Result<Self> {
        let len = data.len().read_error("Unknown archive length")?;
        let mut tail = 0;
        let magic = data
            .read_bytes(&mut tail, archive::MAGIC.len() as u64)
            .read_error("Invalid archive size")?;

        if magic == archive::AIX_BIG_MAGIC {
            return Self::parse_aixbig(data);
        } else if magic == archive::ZOS_MAGIC {
            return Self::parse_zos(data);
        }

        let thin = if magic == archive::THIN_MAGIC {
            true
        } else if magic == archive::MAGIC {
            false
        } else {
            return Err(Error("Unsupported archive identifier"));
        };

        let mut members_offset = tail;
        let members_end_offset = len;

        let mut file = ArchiveFile {
            data: SkipDebugList(data),
            kind: ArchiveKind::Unknown,
            members: Members::Common {
                offset: 0,
                end_offset: 0,
            },
            symbols: (0, 0),
            names: &[],
            thin,
        };

        let parse_member = |file: &ArchiveFile<'data, R>, tail: &mut u64| {
            if *tail < len {
                ArchiveMember::parse(file.data.0, tail, file.names, file.thin).map(Some)
            } else {
                Ok(None)
            }
        };

        // The first few members may be special, so parse them.
        // GNU has:
        // - "/" or "/SYM64/": symbol table (optional)
        // - "//": names table (optional)
        // COFF has:
        // - "/": first linker member
        // - "/": second linker member
        // - "//": names table (optional)
        // - "/<ECSYMBOLS>/": ARM EC symbol table (optional)
        // BSD has:
        // - "__.SYMDEF" or "__.SYMDEF SORTED": symbol table (optional)
        // BSD 64-bit has:
        // - "__.SYMDEF_64" or "__.SYMDEF_64 SORTED": symbol table (optional)
        // BSD may use the extended name for the symbol table. This is handled
        // by `ArchiveMember::parse`.
        if let Some(member) = parse_member(&file, &mut tail)? {
            if member.name == b"/" {
                // GNU symbol table (unless we later determine this is COFF).
                file.kind = ArchiveKind::Gnu;
                file.symbols = member.file_range();
                members_offset = tail;
                if let Some(member) = parse_member(&file, &mut tail)? {
                    if member.name == b"//" {
                        // GNU names table.
                        file.names = member.data(data)?;
                        members_offset = tail;
                    } else if member.name == b"/" {
                        // COFF linker member.
                        file.kind = ArchiveKind::Coff;
                        file.symbols = member.file_range();
                        members_offset = tail;
                        let mut next_member = parse_member(&file, &mut tail)?;
                        if let Some(member) = &next_member {
                            if member.name == b"//" {
                                // COFF names table (optional if no long names).
                                file.names = member.data(data)?;
                                members_offset = tail;
                                next_member = parse_member(&file, &mut tail)?;
                            }
                        }
                        if let Some(member) = &next_member {
                            if member.name == b"/<ECSYMBOLS>/" {
                                // COFF EC symbol table.
                                members_offset = tail;
                            }
                        }
                    }
                }
            } else if member.name == b"/SYM64/" {
                // GNU 64-bit symbol table.
                file.kind = ArchiveKind::Gnu64;
                file.symbols = member.file_range();
                members_offset = tail;
                if let Some(member) = parse_member(&file, &mut tail)? {
                    if member.name == b"//" {
                        // GNU names table.
                        file.names = member.data(data)?;
                        members_offset = tail;
                    }
                }
            } else if member.name == b"//" {
                // GNU names table.
                file.kind = ArchiveKind::Gnu;
                file.names = member.data(data)?;
                members_offset = tail;
            } else if member.name == b"__.SYMDEF" || member.name == b"__.SYMDEF SORTED" {
                // BSD symbol table.
                file.kind = ArchiveKind::Bsd;
                file.symbols = member.file_range();
                members_offset = tail;
            } else if member.name == b"__.SYMDEF_64" || member.name == b"__.SYMDEF_64 SORTED" {
                // BSD 64-bit symbol table.
                file.kind = ArchiveKind::Bsd64;
                file.symbols = member.file_range();
                members_offset = tail;
            } else {
                // TODO: This could still be a BSD file. We leave this as unknown for now.
            }
        }
        file.members = Members::Common {
            offset: members_offset,
            end_offset: members_end_offset,
        };
        Ok(file)
    }

    fn parse_aixbig(data: R) -> read::Result<Self> {
        let mut tail = 0;

        let file_header = data
            .read::<archive::AixFileHeader>(&mut tail)
            .read_error("Invalid AIX big archive file header")?;
        // Caller already validated this.
        debug_assert_eq!(file_header.magic, archive::AIX_BIG_MAGIC);

        let mut file = ArchiveFile {
            data: SkipDebugList(data),
            kind: ArchiveKind::AixBig,
            members: Members::AixBig { index: &[] },
            symbols: (0, 0),
            names: &[],
            thin: false,
        };

        // Read the span of symbol table.
        // TODO: an archive may have both 32-bit and 64-bit symbol tables.
        let symtbl64 = parse_u64_digits(&file_header.gst64off, 10)
            .read_error("Invalid offset to 64-bit symbol table in AIX big archive")?;
        if symtbl64 > 0 {
            // The symbol table is also a file with header.
            let member = ArchiveMember::parse_aixbig(data, symtbl64)?;
            file.symbols = member.file_range();
        } else {
            let symtbl = parse_u64_digits(&file_header.gstoff, 10)
                .read_error("Invalid offset to symbol table in AIX big archive")?;
            if symtbl > 0 {
                // The symbol table is also a file with header.
                let member = ArchiveMember::parse_aixbig(data, symtbl)?;
                file.symbols = member.file_range();
            }
        }

        // Big archive member index table lists file entries with offsets and names.
        // To avoid potential infinite loop (members are double-linked list), the
        // iterator goes through the index instead of real members.
        let member_table_offset = parse_u64_digits(&file_header.memoff, 10)
            .read_error("Invalid offset for member table of AIX big archive")?;
        if member_table_offset == 0 {
            // The offset would be zero if archive contains no file.
            return Ok(file);
        }

        // The member index table is also a file with header.
        let member = ArchiveMember::parse_aixbig(data, member_table_offset)?;
        let mut member_data = Bytes(member.data(data)?);

        // Structure of member index table:
        // Number of entries (20 bytes)
        // Offsets of each entry (20*N bytes)
        // Names string table (the rest of bytes to fill size defined in header)
        let members_count_bytes = member_data
            .read_slice::<u8>(20)
            .read_error("Missing member count in AIX big archive")?;
        let members_count = parse_u64_digits(members_count_bytes, 10)
            .and_then(|size| size.try_into().ok())
            .read_error("Invalid member count in AIX big archive")?;
        let index = member_data
            .read_slice::<archive::AixMemberOffset>(members_count)
            .read_error("Member count overflow in AIX big archive")?;
        file.members = Members::AixBig { index };

        Ok(file)
    }

    /// Parse the z/OS archive header and special __.SYMDEF member.
    fn parse_zos(data: R) -> read::Result<Self> {
        let len = data.len().read_error("Unknown archive length")?;
        let mut tail = 0;
        let magic = data
            .read_bytes(&mut tail, archive::MAGIC.len() as u64)
            .read_error("Invalid z/OS archive size")?;
        debug_assert_eq!(magic, archive::ZOS_MAGIC);

        let mut file = ArchiveFile {
            data: SkipDebugList(data),
            kind: ArchiveKind::Zos,
            members: Members::Zos {
                offset: 0,
                end_offset: 0,
            },
            symbols: (0, 0),
            names: &[],
            thin: false,
        };

        let mut members_offset = tail;
        let members_end_offset = len;

        if tail < len {
            let member = ArchiveMember::parse_zos(data, &mut tail)?;
            // __.SYMDEF
            if member.name() == [0x6D, 0x6D, 0x4B, 0xE2, 0xE8, 0xD4, 0xC4, 0xC5, 0xC6] {
                // Symbol table member.
                file.symbols = member.file_range();
                members_offset = tail;
            }
        }
        file.members = Members::Zos {
            offset: members_offset,
            end_offset: members_end_offset,
        };
        Ok(file)
    }

    /// Return the archive format.
    #[inline]
    pub fn kind(&self) -> ArchiveKind {
        self.kind
    }

    /// Return true if the archive is a thin archive.
    pub fn is_thin(&self) -> bool {
        self.thin
    }

    /// Iterate over the members of the archive.
    ///
    /// This does not return special members.
    #[inline]
    pub fn members(&self) -> ArchiveMemberIterator<'data, R> {
        ArchiveMemberIterator {
            data: self.data.0,
            members: self.members,
            names: self.names,
            thin: self.thin,
        }
    }

    /// Return the member at the given offset.
    pub fn member(&self, member: ArchiveOffset) -> read::Result<ArchiveMember<'data>> {
        match self.members {
            Members::Common { offset, end_offset } => {
                if member.0 < offset || member.0 >= end_offset {
                    return Err(Error("Invalid archive member offset"));
                }
                let mut offset = member.0;
                ArchiveMember::parse(self.data.0, &mut offset, self.names, self.thin)
            }
            Members::AixBig { .. } => {
                let offset = member.0;
                ArchiveMember::parse_aixbig(self.data.0, offset)
            }
            Members::Zos { offset, end_offset } => {
                if member.0 < offset || member.0 >= end_offset {
                    return Err(Error("Invalid archive member offset"));
                }
                let mut offset = member.0;
                ArchiveMember::parse_zos(self.data.0, &mut offset)
            }
        }
    }

    /// Iterate over the symbols in the archive.
    pub fn symbols(&self) -> read::Result<Option<ArchiveSymbolIterator<'data>>> {
        if self.symbols == (0, 0) {
            return Ok(None);
        }
        let (offset, size) = self.symbols;
        ArchiveSymbolIterator::new(self.kind, self.data.0, offset, size)
            .read_error("Invalid archive symbol table")
            .map(Some)
    }
}

/// An iterator over the members of an archive.
#[derive(Debug)]
pub struct ArchiveMemberIterator<'data, R: ReadRef<'data> = &'data [u8]> {
    data: R,
    members: Members<'data>,
    names: &'data [u8],
    thin: bool,
}

impl<'data, R: ReadRef<'data>> Iterator for ArchiveMemberIterator<'data, R> {
    type Item = read::Result<ArchiveMember<'data>>;

    fn next(&mut self) -> Option<Self::Item> {
        match &mut self.members {
            Members::Common { offset, end_offset } => {
                if *offset >= *end_offset {
                    return None;
                }
                let member = ArchiveMember::parse(self.data, offset, self.names, self.thin);
                if member.is_err() {
                    *offset = *end_offset;
                }
                Some(member)
            }
            Members::AixBig { index } => match **index {
                [] => None,
                [ref first, ref rest @ ..] => {
                    *index = rest;
                    let member = ArchiveMember::parse_aixbig_index(self.data, first);
                    if member.is_err() {
                        *index = &[];
                    }
                    Some(member)
                }
            },
            Members::Zos { offset, end_offset } => {
                if *offset >= *end_offset {
                    return None;
                }
                let member = ArchiveMember::parse_zos(self.data, offset);
                if member.is_err() {
                    *offset = *end_offset;
                }
                Some(member)
            }
        }
    }
}

/// An archive member header.
#[derive(Debug, Clone, Copy)]
enum MemberHeader<'data> {
    /// Common header used by many formats.
    Common(&'data archive::Header),
    /// AIX big archive header
    AixBig(&'data archive::AixHeader),
    /// ZOS archive header.
    /// Same format as Common but fields are EBCDIC.
    Zos(&'data archive::Header),
}

/// A partially parsed archive member.
#[derive(Debug)]
pub struct ArchiveMember<'data> {
    header: MemberHeader<'data>,
    name: &'data [u8],
    // May be zero for thin members.
    offset: u64,
    size: u64,
}

impl<'data> ArchiveMember<'data> {
    /// Parse the member header, name, and file data in an archive with the common format.
    ///
    /// This reads the extended name (if any) and adjusts the file size.
    fn parse<R: ReadRef<'data>>(
        data: R,
        offset: &mut u64,
        names: &'data [u8],
        thin: bool,
    ) -> read::Result<Self> {
        let header = data
            .read::<archive::Header>(offset)
            .read_error("Invalid archive member header")?;
        if header.terminator != archive::TERMINATOR {
            return Err(Error("Invalid archive terminator"));
        }

        let header_file_size =
            parse_u64_digits(&header.size, 10).read_error("Invalid archive member size")?;
        let mut file_offset = *offset;
        let mut file_size = header_file_size;

        let name = if header.name[0] == b'/' && (header.name[1] as char).is_ascii_digit() {
            // Read file name from the names table.
            parse_sysv_extended_name(&header.name[1..], names)
                .read_error("Invalid archive extended name offset")?
        } else if &header.name[..3] == b"#1/" && (header.name[3] as char).is_ascii_digit() {
            // Read file name from the start of the file data.
            parse_bsd_extended_name(&header.name[3..], data, &mut file_offset, &mut file_size)
                .read_error("Invalid archive extended name length")?
        } else if header.name[0] == b'/' {
            let name_len = memchr::memchr(b' ', &header.name).unwrap_or(header.name.len());
            &header.name[..name_len]
        } else {
            // Name is terminated by slash or space.
            // Slash allows embedding spaces in the name, so only look
            // for space if there is no slash.
            let name_len = memchr::memchr(b'/', &header.name)
                .or_else(|| memchr::memchr(b' ', &header.name))
                .unwrap_or(header.name.len());
            &header.name[..name_len]
        };

        // Members in thin archives don't have data unless they are special members.
        if thin && name != b"/" && name != b"//" && name != b"/SYM64/" {
            return Ok(ArchiveMember {
                header: MemberHeader::Common(header),
                name,
                offset: 0,
                size: file_size,
            });
        }

        // Skip the file data.
        *offset = offset
            .checked_add(header_file_size)
            .read_error("Archive member size is too large")?;
        // Entries are padded to an even number of bytes.
        if (header_file_size & 1) != 0 {
            *offset = offset.saturating_add(1);
        }

        Ok(ArchiveMember {
            header: MemberHeader::Common(header),
            name,
            offset: file_offset,
            size: file_size,
        })
    }

    /// Parse the member header, name, and file data in an archive with the z/OS format.
    ///
    /// This reads the extended name (if any) and adjusts the file size.
    fn parse_zos<R: ReadRef<'data>>(data: R, offset: &mut u64) -> read::Result<Self> {
        let header = data
            .read::<archive::Header>(offset)
            .read_error("Invalid z/OS archive member header")?;
        if header.terminator != archive::ZOS_TERMINATOR {
            return Err(Error("Invalid z/OS archive header terminator"));
        }

        let header_file_size =
            parse_u64_ebcdic_digits(&header.size, 10).read_error("Invalid archive member size")?;
        let mut file_offset = *offset;
        let mut file_size = header_file_size;

        // Check for "#1/" in EBCDIC followed by EBCDIC length.
        let name = if header.name[..3] == [0x7B, 0xF1, 0x61] && is_ebcdic_digit(header.name[3]) {
            // BSD-style extended name at start of file data.
            // Note: This is unlikely for z/OS archives, but we handle it for completeness
            zos_parse_bsd_extended_name(&header.name[3..], data, &mut file_offset, &mut file_size)
                .read_error("Invalid archive extended name length")?
        } else {
            let name_len = memchr::memchr(EBCDIC_SPACE, &header.name).unwrap_or(header.name.len());
            &header.name[..name_len]
        };

        // Skip the file data.
        *offset = offset
            .checked_add(header_file_size)
            .read_error("Archive member size is too large")?;
        // Entries are padded to an even number of bytes.
        if (header_file_size & 1) != 0 {
            *offset = offset.saturating_add(1);
        }

        Ok(ArchiveMember {
            header: MemberHeader::Zos(header),
            name,
            offset: file_offset,
            size: file_size,
        })
    }

    /// Parse a member index entry in an AIX big archive,
    /// and then parse the member header, name, and file data.
    fn parse_aixbig_index<R: ReadRef<'data>>(
        data: R,
        index: &archive::AixMemberOffset,
    ) -> read::Result<Self> {
        let offset = parse_u64_digits(&index.0, 10)
            .read_error("Invalid AIX big archive file member offset")?;
        Self::parse_aixbig(data, offset)
    }

    /// Parse the member header, name, and file data in an AIX big archive.
    fn parse_aixbig<R: ReadRef<'data>>(data: R, mut offset: u64) -> read::Result<Self> {
        // The format was described at
        // https://www.ibm.com/docs/en/aix/7.3?topic=formats-ar-file-format-big
        let header = data
            .read::<archive::AixHeader>(&mut offset)
            .read_error("Invalid AIX big archive member header")?;
        let name_length = parse_u64_digits(&header.namlen, 10)
            .read_error("Invalid AIX big archive member name length")?;
        let name = data
            .read_bytes(&mut offset, name_length)
            .read_error("Invalid AIX big archive member name")?;

        // The actual data for a file member begins at the first even-byte boundary beyond the
        // member header and continues for the number of bytes specified by the ar_size field. The
        // ar command inserts null bytes for padding where necessary.
        if offset & 1 != 0 {
            offset = offset.saturating_add(1);
        }
        // Because of the even-byte boundary, we have to read and check terminator after header.
        let terminator = data
            .read_bytes(&mut offset, 2)
            .read_error("Invalid AIX big archive terminator")?;
        if terminator != archive::TERMINATOR {
            return Err(Error("Invalid AIX big archive terminator"));
        }

        let size = parse_u64_digits(&header.size, 10)
            .read_error("Invalid archive member size in AIX big archive")?;
        Ok(ArchiveMember {
            header: MemberHeader::AixBig(header),
            name,
            offset,
            size,
        })
    }

    /// Return the raw header that is common to many archive formats.
    ///
    /// Returns `None` if this archive does not use the common header format.
    #[inline]
    pub fn header(&self) -> Option<&'data archive::Header> {
        match self.header {
            MemberHeader::Common(header) => Some(header),
            _ => None,
        }
    }

    /// Return the raw header for AIX big archives.
    ///
    /// Returns `None` if this is not an AIX big archive.
    #[inline]
    pub fn aix_header(&self) -> Option<&'data archive::AixHeader> {
        match self.header {
            MemberHeader::AixBig(header) => Some(header),
            _ => None,
        }
    }

    /// Return the parsed file name.
    ///
    /// This may be an extended file name.
    #[inline]
    pub fn name(&self) -> &'data [u8] {
        self.name
    }

    /// Parse the file modification timestamp from the header.
    #[inline]
    pub fn date(&self) -> Option<u64> {
        match &self.header {
            MemberHeader::Common(header) => parse_u64_digits(&header.date, 10),
            MemberHeader::AixBig(header) => parse_u64_digits(&header.date, 10),
            MemberHeader::Zos(header) => parse_u64_ebcdic_digits(&header.date, 10),
        }
    }

    /// Parse the user ID from the header.
    #[inline]
    pub fn uid(&self) -> Option<u64> {
        match &self.header {
            MemberHeader::Common(header) => parse_u64_digits(&header.uid, 10),
            MemberHeader::AixBig(header) => parse_u64_digits(&header.uid, 10),
            MemberHeader::Zos(header) => parse_u64_ebcdic_digits(&header.uid, 10),
        }
    }

    /// Parse the group ID from the header.
    #[inline]
    pub fn gid(&self) -> Option<u64> {
        match &self.header {
            MemberHeader::Common(header) => parse_u64_digits(&header.gid, 10),
            MemberHeader::AixBig(header) => parse_u64_digits(&header.gid, 10),
            MemberHeader::Zos(header) => parse_u64_ebcdic_digits(&header.gid, 10),
        }
    }

    /// Parse the file mode from the header.
    #[inline]
    pub fn mode(&self) -> Option<u64> {
        match &self.header {
            MemberHeader::Common(header) => parse_u64_digits(&header.mode, 8),
            MemberHeader::AixBig(header) => parse_u64_digits(&header.mode, 8),
            MemberHeader::Zos(header) => parse_u64_ebcdic_digits(&header.mode, 8),
        }
    }

    /// Return the size of the file data.
    pub fn size(&self) -> u64 {
        self.size
    }

    /// Return the offset and size of the file data.
    pub fn file_range(&self) -> (u64, u64) {
        (self.offset, self.size)
    }

    /// Return true if the member is a thin member.
    ///
    /// Thin members have no file data.
    pub fn is_thin(&self) -> bool {
        self.offset == 0
    }

    /// Return the file data.
    ///
    /// This is an empty slice for thin members.
    #[inline]
    pub fn data<R: ReadRef<'data>>(&self, data: R) -> read::Result<&'data [u8]> {
        if self.is_thin() {
            return Ok(&[]);
        }
        data.read_bytes_at(self.offset, self.size)
            .read_error("Archive member size is too large")
    }
}

/// An offset of a member in an archive.
#[derive(Debug, Clone, Copy)]
pub struct ArchiveOffset(pub u64);

/// An iterator over the symbols in the archive symbol table.
#[derive(Debug, Clone)]
pub struct ArchiveSymbolIterator<'data>(SymbolIteratorInternal<'data>);

#[derive(Debug, Clone)]
enum SymbolIteratorInternal<'data> {
    /// There is no symbol table.
    None,
    /// A GNU symbol table.
    ///
    /// Contains:
    /// - the number of symbols as a 32-bit big-endian integer
    /// - the offsets of the member headers as 32-bit big-endian integers
    /// - the symbol names as null-terminated strings
    Gnu {
        offsets: slice::Iter<'data, U32<BE>>,
        names: Bytes<'data>,
    },
    /// A GNU 64-bit symbol table
    ///
    /// Contains:
    /// - the number of symbols as a 64-bit big-endian integer
    /// - the offsets of the member headers as 64-bit big-endian integers
    /// - the symbol names as null-terminated strings
    Gnu64 {
        offsets: slice::Iter<'data, U64<BE>>,
        names: Bytes<'data>,
    },
    /// A BSD symbol table.
    ///
    /// Contains:
    /// - the size in bytes of the offsets array as a 32-bit little-endian integer
    /// - the offsets array, for which each entry is a pair of 32-bit little-endian integers
    ///   for the offset of the member header and the offset of the symbol name
    /// - the size in bytes of the symbol names as a 32-bit little-endian integer
    /// - the symbol names as null-terminated strings
    Bsd {
        offsets: slice::Iter<'data, [U32<LE>; 2]>,
        names: Bytes<'data>,
    },
    /// A BSD 64-bit symbol table.
    ///
    /// Contains:
    /// - the size in bytes of the offsets array as a 64-bit little-endian integer
    /// - the offsets array, for which each entry is a pair of 64-bit little-endian integers
    ///   for the offset of the member header and the offset of the symbol name
    /// - the size in bytes of the symbol names as a 64-bit little-endian integer
    /// - the symbol names as null-terminated strings
    Bsd64 {
        offsets: slice::Iter<'data, [U64<LE>; 2]>,
        names: Bytes<'data>,
    },
    /// A Windows COFF symbol table.
    ///
    /// Contains:
    /// - the number of members as a 32-bit little-endian integer
    /// - the offsets of the member headers as 32-bit little-endian integers
    /// - the number of symbols as a 32-bit little-endian integer
    /// - the member index for each symbol as a 16-bit little-endian integer
    /// - the symbol names as null-terminated strings in lexical order
    Coff {
        members: &'data [U32<LE>],
        indices: slice::Iter<'data, U16<LE>>,
        names: Bytes<'data>,
    },
}

impl<'data> ArchiveSymbolIterator<'data> {
    fn new<R: ReadRef<'data>>(
        kind: ArchiveKind,
        data: R,
        offset: u64,
        size: u64,
    ) -> Result<Self, ()> {
        let mut data = data.read_bytes_at(offset, size).map(Bytes)?;
        match kind {
            ArchiveKind::Unknown => Ok(ArchiveSymbolIterator(SymbolIteratorInternal::None)),
            ArchiveKind::Gnu => {
                let offsets_count = data.read::<U32<BE>>()?.get(BE);
                let offsets = data.read_slice::<U32<BE>>(offsets_count as usize)?;
                Ok(ArchiveSymbolIterator(SymbolIteratorInternal::Gnu {
                    offsets: offsets.iter(),
                    names: data,
                }))
            }
            ArchiveKind::Gnu64 => {
                let offsets_count = data.read::<U64<BE>>()?.get(BE);
                let offsets = data.read_slice::<U64<BE>>(offsets_count as usize)?;
                Ok(ArchiveSymbolIterator(SymbolIteratorInternal::Gnu64 {
                    offsets: offsets.iter(),
                    names: data,
                }))
            }
            ArchiveKind::Bsd => {
                let offsets_size = data.read::<U32<LE>>()?.get(LE);
                let offsets = data.read_slice::<[U32<LE>; 2]>(offsets_size as usize / 8)?;
                let names_size = data.read::<U32<LE>>()?.get(LE);
                let names = data.read_bytes(names_size as usize)?;
                Ok(ArchiveSymbolIterator(SymbolIteratorInternal::Bsd {
                    offsets: offsets.iter(),
                    names,
                }))
            }
            ArchiveKind::Bsd64 => {
                let offsets_size = data.read::<U64<LE>>()?.get(LE);
                let offsets = data.read_slice::<[U64<LE>; 2]>(offsets_size as usize / 16)?;
                let names_size = data.read::<U64<LE>>()?.get(LE);
                let names = data.read_bytes(names_size as usize)?;
                Ok(ArchiveSymbolIterator(SymbolIteratorInternal::Bsd64 {
                    offsets: offsets.iter(),
                    names,
                }))
            }
            ArchiveKind::Coff => {
                let members_count = data.read::<U32<LE>>()?.get(LE);
                let members = data.read_slice::<U32<LE>>(members_count as usize)?;
                let indices_count = data.read::<U32<LE>>()?.get(LE);
                let indices = data.read_slice::<U16<LE>>(indices_count as usize)?;
                Ok(ArchiveSymbolIterator(SymbolIteratorInternal::Coff {
                    members,
                    indices: indices.iter(),
                    names: data,
                }))
            }
            // TODO: Implement AIX big archive symbol table.
            ArchiveKind::AixBig => Ok(ArchiveSymbolIterator(SymbolIteratorInternal::None)),
            // TODO: Implement z/OS archive symbol table.
            ArchiveKind::Zos => Ok(ArchiveSymbolIterator(SymbolIteratorInternal::None)),
        }
    }
}

impl<'data> Iterator for ArchiveSymbolIterator<'data> {
    type Item = read::Result<ArchiveSymbol<'data>>;

    fn next(&mut self) -> Option<Self::Item> {
        match &mut self.0 {
            SymbolIteratorInternal::None => None,
            SymbolIteratorInternal::Gnu { offsets, names } => {
                let offset = offsets.next()?.get(BE);
                Some(
                    names
                        .read_string()
                        .read_error("Missing archive symbol name")
                        .map(|name| ArchiveSymbol {
                            name,
                            offset: ArchiveOffset(offset.into()),
                        }),
                )
            }
            SymbolIteratorInternal::Gnu64 { offsets, names } => {
                let offset = offsets.next()?.get(BE);
                Some(
                    names
                        .read_string()
                        .read_error("Missing archive symbol name")
                        .map(|name| ArchiveSymbol {
                            name,
                            offset: ArchiveOffset(offset),
                        }),
                )
            }
            SymbolIteratorInternal::Bsd { offsets, names } => {
                let entry = offsets.next()?;
                Some(
                    names
                        .read_string_at(entry[0].get(LE) as usize)
                        .read_error("Invalid archive symbol name offset")
                        .map(|name| ArchiveSymbol {
                            name,
                            offset: ArchiveOffset(entry[1].get(LE).into()),
                        }),
                )
            }
            SymbolIteratorInternal::Bsd64 { offsets, names } => {
                let entry = offsets.next()?;
                Some(
                    names
                        .read_string_at(entry[0].get(LE) as usize)
                        .read_error("Invalid archive symbol name offset")
                        .map(|name| ArchiveSymbol {
                            name,
                            offset: ArchiveOffset(entry[1].get(LE)),
                        }),
                )
            }
            SymbolIteratorInternal::Coff {
                members,
                indices,
                names,
            } => {
                let index = indices.next()?.get(LE).wrapping_sub(1);
                let member = members
                    .get(index as usize)
                    .read_error("Invalid archive symbol member index");
                let name = names
                    .read_string()
                    .read_error("Missing archive symbol name");
                Some(member.and_then(|member| {
                    name.map(|name| ArchiveSymbol {
                        name,
                        offset: ArchiveOffset(member.get(LE).into()),
                    })
                }))
            }
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        match &self.0 {
            SymbolIteratorInternal::None => (0, None),
            SymbolIteratorInternal::Gnu { offsets, .. } => offsets.size_hint(),
            SymbolIteratorInternal::Gnu64 { offsets, .. } => offsets.size_hint(),
            SymbolIteratorInternal::Bsd { offsets, .. } => offsets.size_hint(),
            SymbolIteratorInternal::Bsd64 { offsets, .. } => offsets.size_hint(),
            SymbolIteratorInternal::Coff { indices, .. } => {
                // The `slice::Iter` is in the indices field for this variant
                indices.size_hint()
            }
        }
    }
}

/// A symbol in the archive symbol table.
///
/// This is used to find the member containing the symbol.
#[derive(Debug, Clone, Copy)]
pub struct ArchiveSymbol<'data> {
    name: &'data [u8],
    offset: ArchiveOffset,
}

impl<'data> ArchiveSymbol<'data> {
    /// Return the symbol name.
    #[inline]
    pub fn name(&self) -> &'data [u8] {
        self.name
    }

    /// Return the offset of the header for the member containing the symbol.
    #[inline]
    pub fn offset(&self) -> ArchiveOffset {
        self.offset
    }
}

// Ignores bytes starting from the first space.
fn parse_u64_digits(digits: &[u8], radix: u32) -> Option<u64> {
    if let [b' ', ..] = digits {
        return None;
    }
    let mut result: u64 = 0;
    for &c in digits {
        if c == b' ' {
            return Some(result);
        } else {
            let x = (c as char).to_digit(radix)?;
            result = result
                .checked_mul(u64::from(radix))?
                .checked_add(u64::from(x))?;
        }
    }
    Some(result)
}

/// Parse EBCDIC-encoded numeric digits.
fn parse_u64_ebcdic_digits(digits: &[u8], radix: u32) -> Option<u64> {
    if let [EBCDIC_SPACE, ..] = digits {
        return None;
    }
    let mut result: u64 = 0;
    for &c in digits {
        if c == EBCDIC_SPACE {
            return Some(result);
        } else {
            let x = ebcdic_to_digit(c, radix)?;
            result = result
                .checked_mul(u64::from(radix))?
                .checked_add(u64::from(x))?;
        }
    }
    Some(result)
}

const EBCDIC_SPACE: u8 = 0x40;

fn is_ebcdic_digit(byte: u8) -> bool {
    matches!(byte, 0xF0..=0xF9)
}

fn ebcdic_to_digit(byte: u8, radix: u32) -> Option<u8> {
    if radix == 8 && matches!(byte, 0xF0..=0xF7) || radix == 10 && matches!(byte, 0xF0..=0xF9) {
        Some(byte - 0xF0)
    } else {
        None
    }
}

/// Digits are a decimal offset into the extended name table.
/// Name is terminated by "/\n" (for GNU) or a null byte (for COFF).
fn parse_sysv_extended_name<'data>(digits: &[u8], names: &'data [u8]) -> Result<&'data [u8], ()> {
    let offset = parse_u64_digits(digits, 10).ok_or(())?;
    let offset = offset.try_into().map_err(|_| ())?;
    let name_data = names.get(offset..).ok_or(())?;
    let len = memchr::memchr2(b'\n', b'\0', name_data).ok_or(())?;
    if name_data[len] == b'\n' {
        if len < 1 || name_data[len - 1] != b'/' {
            Err(())
        } else {
            Ok(&name_data[..len - 1])
        }
    } else {
        Ok(&name_data[..len])
    }
}

/// Digits are a decimal length of the extended name, which is contained
/// in `data` at `offset`.
/// Modifies `offset` and `size` to start after the extended name.
fn parse_bsd_extended_name<'data, R: ReadRef<'data>>(
    digits: &[u8],
    data: R,
    offset: &mut u64,
    size: &mut u64,
) -> Result<&'data [u8], ()> {
    let len = parse_u64_digits(digits, 10).ok_or(())?;
    *size = size.checked_sub(len).ok_or(())?;
    let name_data = data.read_bytes(offset, len)?;
    let name = match memchr::memchr(b'\0', name_data) {
        Some(len) => &name_data[..len],
        None => name_data,
    };
    Ok(name)
}

/// Digits are an EBCDIC decimal length of the extended name, which is contained
/// in `data` at `offset`.
/// Modifies `offset` and `size` to start after the extended name.
fn zos_parse_bsd_extended_name<'data, R: ReadRef<'data>>(
    digits: &[u8],
    data: R,
    offset: &mut u64,
    size: &mut u64,
) -> Result<&'data [u8], ()> {
    let len = parse_u64_ebcdic_digits(digits, 10).ok_or(())?;
    *size = size.checked_sub(len).ok_or(())?;
    let name_data = data.read_bytes(offset, len)?;
    let name = match memchr::memchr(b'\0', name_data) {
        Some(len) => &name_data[..len],
        None => name_data,
    };
    Ok(name)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn kind() {
        let data = b"!<arch>\n";
        let archive = ArchiveFile::parse(&data[..]).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::Unknown);

        let data = b"\
            !<arch>\n\
            /                                               4         `\n\
            0000";
        let archive = ArchiveFile::parse(&data[..]).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::Gnu);

        let data = b"\
            !<arch>\n\
            //                                              4         `\n\
            0000";
        let archive = ArchiveFile::parse(&data[..]).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::Gnu);

        let data = b"\
            !<arch>\n\
            /                                               4         `\n\
            0000\
            //                                              4         `\n\
            0000";
        let archive = ArchiveFile::parse(&data[..]).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::Gnu);

        let data = b"\
            !<arch>\n\
            /SYM64/                                         4         `\n\
            0000";
        let archive = ArchiveFile::parse(&data[..]).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::Gnu64);

        let data = b"\
            !<arch>\n\
            /SYM64/                                         4         `\n\
            0000\
            //                                              4         `\n\
            0000";
        let archive = ArchiveFile::parse(&data[..]).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::Gnu64);

        let data = b"\
            !<arch>\n\
            __.SYMDEF                                       4         `\n\
            0000";
        let archive = ArchiveFile::parse(&data[..]).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::Bsd);

        let data = b"\
            !<arch>\n\
            #1/9                                            13        `\n\
            __.SYMDEF0000";
        let archive = ArchiveFile::parse(&data[..]).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::Bsd);

        let data = b"\
            !<arch>\n\
            #1/16                                           20        `\n\
            __.SYMDEF SORTED0000";
        let archive = ArchiveFile::parse(&data[..]).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::Bsd);

        let data = b"\
            !<arch>\n\
            __.SYMDEF_64                                    4         `\n\
            0000";
        let archive = ArchiveFile::parse(&data[..]).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::Bsd64);

        let data = b"\
            !<arch>\n\
            #1/12                                           16        `\n\
            __.SYMDEF_640000";
        let archive = ArchiveFile::parse(&data[..]).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::Bsd64);

        let data = b"\
            !<arch>\n\
            #1/19                                           23        `\n\
            __.SYMDEF_64 SORTED0000";
        let archive = ArchiveFile::parse(&data[..]).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::Bsd64);

        let data = b"\
            !<arch>\n\
            /                                               4         `\n\
            0000\
            /                                               4         `\n\
            0000\
            //                                              4         `\n\
            0000";
        let archive = ArchiveFile::parse(&data[..]).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::Coff);

        let data = b"\
            <bigaf>\n\
            0                   0                   \
            0                   0                   \
            0                   128                 \
            6                   0                   \
            0                   \0\0\0\0\0\0\0\0\0\0\0\0\
            \0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\
            \0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\
            \0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
        let archive = ArchiveFile::parse(&data[..]).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::AixBig);

        let data = b"\
            !<thin>\n\
            /                                               4         `\n\
            0000";
        let archive = ArchiveFile::parse(&data[..]).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::Gnu);
        assert!(archive.is_thin());
    }

    #[test]
    fn coff_ec_symbols() {
        use alloc::vec::Vec;

        // Append a COFF archive member (header followed by data) to `archive`.
        fn add_member(archive: &mut Vec<u8>, name: &str, data: &[u8]) {
            let mut header = [b' '; 60];
            header[..name.len()].copy_from_slice(name.as_bytes());
            let size = format!("{}", data.len());
            header[48..48 + size.len()].copy_from_slice(size.as_bytes());
            header[58] = b'`';
            header[59] = b'\n';
            archive.extend_from_slice(&header);
            archive.extend_from_slice(data);
            // Members are padded to an even number of bytes.
            if data.len() % 2 != 0 {
                archive.push(b'\n');
            }
        }

        // A COFF archive with an EC symbol table but no `//` names table, which
        // `llvm-ar` omits when all member names fit in the header. The
        // `/<ECSYMBOLS>/` member must be skipped when iterating members.
        let mut data = Vec::new();
        data.extend_from_slice(b"!<arch>\n");
        add_member(&mut data, "/", &[0, 0, 0, 0]);
        add_member(&mut data, "/", &[0, 0, 0, 0]);
        add_member(&mut data, "/<ECSYMBOLS>/", &[1, 0, 1, 0]);
        add_member(&mut data, "foo.obj/", b"data");

        let archive = ArchiveFile::parse(&data[..]).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::Coff);
        let mut members = archive.members();
        let member = members.next().unwrap().unwrap();
        assert_eq!(member.name(), b"foo.obj");
        assert_eq!(member.data(&data[..]).unwrap(), &b"data"[..]);
        assert!(members.next().is_none());

        // The same archive, but with a `//` names table before the EC symbol
        // table. The `/<ECSYMBOLS>/` member must still be skipped.
        let mut data = Vec::new();
        data.extend_from_slice(b"!<arch>\n");
        add_member(&mut data, "/", &[0, 0, 0, 0]);
        add_member(&mut data, "/", &[0, 0, 0, 0]);
        add_member(&mut data, "//", b"foo.obj/\n");
        add_member(&mut data, "/<ECSYMBOLS>/", &[1, 0, 1, 0]);
        add_member(&mut data, "/0", b"data");

        let archive = ArchiveFile::parse(&data[..]).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::Coff);
        let mut members = archive.members();
        let member = members.next().unwrap().unwrap();
        assert_eq!(member.name(), b"foo.obj");
        assert_eq!(member.data(&data[..]).unwrap(), &b"data"[..]);
        assert!(members.next().is_none());
    }

    #[test]
    fn gnu_names() {
        let data = b"\
            !<arch>\n\
            //                                              18        `\n\
            0123456789abcdef/\n\
            s p a c e/      0           0     0     644     4         `\n\
            0000\
            0123456789abcde/0           0     0     644     3         `\n\
            odd\n\
            /0              0           0     0     644     4         `\n\
            even";
        let data = &data[..];
        let archive = ArchiveFile::parse(data).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::Gnu);
        let mut members = archive.members();

        let member = members.next().unwrap().unwrap();
        assert_eq!(member.name(), b"s p a c e");
        assert_eq!(member.data(data).unwrap(), &b"0000"[..]);

        let member = members.next().unwrap().unwrap();
        assert_eq!(member.name(), b"0123456789abcde");
        assert_eq!(member.data(data).unwrap(), &b"odd"[..]);

        let member = members.next().unwrap().unwrap();
        assert_eq!(member.name(), b"0123456789abcdef");
        assert_eq!(member.data(data).unwrap(), &b"even"[..]);

        assert!(members.next().is_none());
    }

    #[test]
    fn thin_gnu_names() {
        let data = b"\
            !<thin>\n\
            //                                              18        `\n\
            0123456789/abcde/\n\
            s p a c e/      0           0     0     644     4         `\n\
            0123456789abcde/0           0     0     644     3         `\n\
            /0              0           0     0     644     4         `\n\
            ";
        let data = &data[..];
        let archive = ArchiveFile::parse(data).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::Gnu);
        let mut members = archive.members();

        let member = members.next().unwrap().unwrap();
        assert_eq!(member.name(), b"s p a c e");
        assert!(member.is_thin());
        assert_eq!(member.size(), 4);
        assert_eq!(member.data(data).unwrap(), &[]);

        let member = members.next().unwrap().unwrap();
        assert_eq!(member.name(), b"0123456789abcde");
        assert!(member.is_thin());
        assert_eq!(member.size(), 3);
        assert_eq!(member.data(data).unwrap(), &[]);

        let member = members.next().unwrap().unwrap();
        assert_eq!(member.name(), b"0123456789/abcde");
        assert!(member.is_thin());
        assert_eq!(member.size(), 4);
        assert_eq!(member.data(data).unwrap(), &[]);

        assert!(members.next().is_none());
    }

    #[test]
    fn bsd_names() {
        let data = b"\
            !<arch>\n\
            0123456789abcde 0           0     0     644     3         `\n\
            odd\n\
            #1/16           0           0     0     644     20        `\n\
            0123456789abcdefeven";
        let data = &data[..];
        let archive = ArchiveFile::parse(data).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::Unknown);
        let mut members = archive.members();

        let member = members.next().unwrap().unwrap();
        assert_eq!(member.name(), b"0123456789abcde");
        assert_eq!(member.data(data).unwrap(), &b"odd"[..]);

        let member = members.next().unwrap().unwrap();
        assert_eq!(member.name(), b"0123456789abcdef");
        assert_eq!(member.data(data).unwrap(), &b"even"[..]);

        assert!(members.next().is_none());
    }

    #[test]
    fn aix_names() {
        let data = b"\
            <bigaf>\n\
            396                 0                   0                   \
            128                 262                 0                   \
            4                   262                 0                   \
            1662610370  223         1           644         16  \
            0123456789abcdef`\nord\n\
            4                   396                 128                 \
            1662610374  223         1           644         16  \
            fedcba9876543210`\nrev\n\
            94                  0                   262                 \
            0           0           0           0           0   \
            `\n2                   128                 \
            262                 0123456789abcdef\0fedcba9876543210\0";
        let data = &data[..];
        let archive = ArchiveFile::parse(data).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::AixBig);
        let mut members = archive.members();

        let member = members.next().unwrap().unwrap();
        assert_eq!(member.name(), b"0123456789abcdef");
        assert_eq!(member.data(data).unwrap(), &b"ord\n"[..]);

        let member = members.next().unwrap().unwrap();
        assert_eq!(member.name(), b"fedcba9876543210");
        assert_eq!(member.data(data).unwrap(), &b"rev\n"[..]);

        assert!(members.next().is_none());
    }

    #[test]
    fn zos_names() {
        let data = b"\
            \x5A\x4C\x81\x99\x83\x88\x6E\x15\
            \xA3\x85\xA2\xA3\xF1\x4B\x96\x40\x40\x40\x40\x40\x40\x40\x40\x40\
            \xF1\xF6\xF6\xF2\xF6\xF1\xF0\xF3\xF7\xF0\x40\x40\
            \xF2\xF2\xF3\x40\x40\x40\
            \xF1\x40\x40\x40\x40\x40\
            \xF6\xF4\xF4\x40\x40\x40\x40\x40\
            \xF6\x40\x40\x40\x40\x40\x40\x40\x40\x40\
            \x79\x15\
            first\0\
            \xA3\x85\xA2\xA3\xF2\x4B\x96\x40\x40\x40\x40\x40\x40\x40\x40\x40\
            \xF1\xF6\xF6\xF2\xF6\xF1\xF0\xF3\xF7\xF0\x40\x40\
            \xF2\xF2\xF3\x40\x40\x40\
            \xF1\x40\x40\x40\x40\x40\
            \xF6\xF4\xF4\x40\x40\x40\x40\x40\
            \xF7\x40\x40\x40\x40\x40\x40\x40\x40\x40\
            \x79\x15\
            second\0";
        let data = &data[..];
        let archive = ArchiveFile::parse(data).unwrap();
        assert_eq!(archive.kind(), ArchiveKind::Zos);
        let mut members = archive.members();

        let member = members.next().unwrap().unwrap();
        // Names are returned in raw EBCDIC encoding
        // "test1.o" in EBCDIC = \xA3\x85\xA2\xA3\xF1\x4B\x96
        assert_eq!(member.name(), b"\xA3\x85\xA2\xA3\xF1\x4B\x96");
        assert_eq!(member.data(data).unwrap(), &b"first\0"[..]);

        let member = members.next().unwrap().unwrap();
        // "test2.o" in EBCDIC = \xA3\x85\xA2\xA3\xF2\x4B\x96
        assert_eq!(member.name(), b"\xA3\x85\xA2\xA3\xF2\x4B\x96");
        assert_eq!(member.data(data).unwrap(), &b"second\0"[..]);

        assert!(members.next().is_none());
    }
}
