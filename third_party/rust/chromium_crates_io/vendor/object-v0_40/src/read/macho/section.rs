use core::fmt::Debug;
use core::{fmt, mem, slice, str};

use crate::endian::{self, Endianness, U32};
use crate::macho;
use crate::pod::Pod;
use crate::read::{
    self, CompressedData, CompressedFileRange, Error, ObjectSection, ReadError, ReadRef,
    RelocationMap, Result, SectionFlags, SectionIndex, SectionKind, gnu_compression,
};

use super::{MachHeader, MachOFile, MachORelocationIterator};

/// An iterator for the sections in a [`MachOFile32`](super::MachOFile32).
pub type MachOSectionIterator32<'data, 'file, Endian = Endianness, R = &'data [u8]> =
    MachOSectionIterator<'data, 'file, macho::MachHeader32<Endian>, R>;
/// An iterator for the sections in a [`MachOFile64`](super::MachOFile64).
pub type MachOSectionIterator64<'data, 'file, Endian = Endianness, R = &'data [u8]> =
    MachOSectionIterator<'data, 'file, macho::MachHeader64<Endian>, R>;

/// An iterator for the sections in a [`MachOFile`].
pub struct MachOSectionIterator<'data, 'file, Mach, R = &'data [u8]>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    pub(super) file: &'file MachOFile<'data, Mach, R>,
    pub(super) iter: slice::Iter<'file, MachOSectionInternal<'data, Mach, R>>,
}

impl<'data, 'file, Mach, R> fmt::Debug for MachOSectionIterator<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // It's painful to do much better than this
        f.debug_struct("MachOSectionIterator").finish()
    }
}

impl<'data, 'file, Mach, R> Iterator for MachOSectionIterator<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    type Item = MachOSection<'data, 'file, Mach, R>;

    fn next(&mut self) -> Option<Self::Item> {
        self.iter.next().map(|&internal| MachOSection {
            file: self.file,
            internal,
        })
    }
}

/// A section in a [`MachOFile32`](super::MachOFile32).
pub type MachOSection32<'data, 'file, Endian = Endianness, R = &'data [u8]> =
    MachOSection<'data, 'file, macho::MachHeader32<Endian>, R>;
/// A section in a [`MachOFile64`](super::MachOFile64).
pub type MachOSection64<'data, 'file, Endian = Endianness, R = &'data [u8]> =
    MachOSection<'data, 'file, macho::MachHeader64<Endian>, R>;

/// A section in a [`MachOFile`].
///
/// Most functionality is provided by the [`ObjectSection`] trait implementation.
#[derive(Debug)]
pub struct MachOSection<'data, 'file, Mach, R = &'data [u8]>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    pub(super) file: &'file MachOFile<'data, Mach, R>,
    pub(super) internal: MachOSectionInternal<'data, Mach, R>,
}

impl<'data, 'file, Mach, R> MachOSection<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    /// Get the Mach-O file containing this section.
    pub fn macho_file(&self) -> &'file MachOFile<'data, Mach, R> {
        self.file
    }

    /// Get the raw Mach-O section structure.
    pub fn macho_section(&self) -> &'data Mach::Section {
        self.internal.section
    }

    /// Get the raw Mach-O relocation entries.
    pub fn macho_relocations(&self) -> Result<&'data [macho::Relocation<Mach::Endian>]> {
        self.internal
            .section
            .relocations(self.file.endian, self.internal.data)
    }

    fn bytes(&self) -> Result<&'data [u8]> {
        self.internal
            .section
            .data(self.file.endian, self.internal.data, self.internal.offset)
            .read_error("Invalid Mach-O section size or offset")
    }

    // Try GNU-style "ZLIB" header decompression.
    fn maybe_compressed_gnu(&self) -> Result<Option<CompressedFileRange>> {
        if !self.name().is_ok_and(|name| name.starts_with("__zdebug_")) {
            return Ok(None);
        }
        let (section_offset, section_size) = self
            .file_range()
            .read_error("Invalid ELF GNU compressed section type")?;
        gnu_compression::compressed_file_range(self.internal.data, section_offset, section_size)
            .map(Some)
    }
}

impl<'data, 'file, Mach, R> read::private::Sealed for MachOSection<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
}

impl<'data, 'file, Mach, R> ObjectSection<'data> for MachOSection<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    type RelocationIterator = MachORelocationIterator<'data, 'file, Mach, R>;

    #[inline]
    fn index(&self) -> SectionIndex {
        self.internal.index
    }

    #[inline]
    fn address(&self) -> u64 {
        self.internal.section.addr(self.file.endian).into()
    }

    #[inline]
    fn size(&self) -> u64 {
        self.internal.section.size(self.file.endian).into()
    }

    #[inline]
    fn align(&self) -> u64 {
        let align = self.internal.section.align(self.file.endian);
        if align < 64 { 1 << align } else { 0 }
    }

    #[inline]
    fn file_range(&self) -> Option<(u64, u64)> {
        self.internal
            .section
            .file_range(self.file.endian, self.internal.offset)
    }

    #[inline]
    fn data(&self) -> Result<&'data [u8]> {
        self.bytes()
    }

    fn data_range(&self, address: u64, size: u64) -> Result<Option<&'data [u8]>> {
        Ok(read::util::data_range(
            self.bytes()?,
            self.address(),
            address,
            size,
        ))
    }

    fn compressed_file_range(&self) -> Result<CompressedFileRange> {
        Ok(if let Some(data) = self.maybe_compressed_gnu()? {
            data
        } else {
            CompressedFileRange::none(self.file_range())
        })
    }

    fn compressed_data(&self) -> read::Result<CompressedData<'data>> {
        self.compressed_file_range()?.data(self.file.data.0)
    }

    #[inline]
    fn name_bytes(&self) -> Result<&'data [u8]> {
        Ok(self.internal.section.name())
    }

    #[inline]
    fn name(&self) -> Result<&'data str> {
        str::from_utf8(self.internal.section.name())
            .ok()
            .read_error("Non UTF-8 Mach-O section name")
    }

    #[inline]
    fn segment_name_bytes(&self) -> Result<Option<&[u8]>> {
        Ok(Some(self.internal.section.segment_name()))
    }

    #[inline]
    fn segment_name(&self) -> Result<Option<&str>> {
        Ok(Some(
            str::from_utf8(self.internal.section.segment_name())
                .ok()
                .read_error("Non UTF-8 Mach-O segment name")?,
        ))
    }

    fn kind(&self) -> SectionKind {
        self.internal.kind
    }

    fn relocations(&self) -> MachORelocationIterator<'data, 'file, Mach, R> {
        MachORelocationIterator {
            file: self.file,
            relocations: self.macho_relocations().unwrap_or(&[]).iter(),
        }
    }

    fn relocation_map(&self) -> read::Result<RelocationMap> {
        RelocationMap::new(self.file, self)
    }

    fn flags(&self) -> SectionFlags {
        SectionFlags::MachO {
            flags: self.internal.section.flags(self.file.endian),
            reserved2: self.internal.section.reserved2(self.file.endian),
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub(super) struct MachOSectionInternal<'data, Mach: MachHeader, R: ReadRef<'data>> {
    pub index: SectionIndex,
    pub kind: SectionKind,
    pub section: &'data Mach::Section,
    /// The data for the file that contains the section data.
    ///
    /// This is required for dyld caches, where this may be a different subcache
    /// from the file containing the Mach-O load commands.
    pub data: R,
    /// The file offset of the section.
    ///
    /// Used instead of `section.offset()` to handle file offsets greater than 32-bit.
    pub offset: u64,
}

impl<'data, Mach: MachHeader, R: ReadRef<'data>> MachOSectionInternal<'data, Mach, R> {
    pub(super) fn parse(
        endian: Mach::Endian,
        index: SectionIndex,
        readonly: Option<bool>,
        section: &'data Mach::Section,
        data: R,
        offset: u64,
    ) -> Self {
        let kind = Self::parse_kind(endian, readonly, section);
        MachOSectionInternal {
            index,
            kind,
            section,
            data,
            offset,
        }
    }

    fn parse_kind(
        endian: Mach::Endian,
        readonly: Option<bool>,
        section: &Mach::Section,
    ) -> SectionKind {
        let flags = section.flags(endian);
        let segment_name = section.segment_name();

        // Don't require the S_ATTR_DEBUG flag for DWARF, but treat its presence
        // in any other segment as unknown.
        if segment_name == b"__DWARF" {
            return SectionKind::Debug;
        }
        if flags.contains(macho::S_ATTR_DEBUG) {
            return SectionKind::Unknown;
        }

        match flags.typ() {
            macho::S_ZEROFILL | macho::S_GB_ZEROFILL => SectionKind::UninitializedData,
            macho::S_CSTRING_LITERALS => SectionKind::ReadOnlyString,
            macho::S_4BYTE_LITERALS
            | macho::S_8BYTE_LITERALS
            | macho::S_16BYTE_LITERALS
            | macho::S_LITERAL_POINTERS => SectionKind::ReadOnlyData,
            macho::S_SYMBOL_STUBS => SectionKind::Text,
            macho::S_THREAD_LOCAL_REGULAR => SectionKind::Tls,
            macho::S_THREAD_LOCAL_ZEROFILL => SectionKind::UninitializedTls,
            macho::S_THREAD_LOCAL_VARIABLES => SectionKind::TlsVariables,
            macho::S_REGULAR
            | macho::S_COALESCED
            | macho::S_NON_LAZY_SYMBOL_POINTERS
            | macho::S_LAZY_SYMBOL_POINTERS
            | macho::S_MOD_INIT_FUNC_POINTERS
            | macho::S_MOD_TERM_FUNC_POINTERS
            | macho::S_INTERPOSING
            | macho::S_LAZY_DYLIB_SYMBOL_POINTERS
            | macho::S_THREAD_LOCAL_VARIABLE_POINTERS
            | macho::S_THREAD_LOCAL_INIT_FUNCTION_POINTERS
            | macho::S_INIT_FUNC_OFFSETS => {
                if flags
                    .intersects(macho::S_ATTR_PURE_INSTRUCTIONS | macho::S_ATTR_SOME_INSTRUCTIONS)
                {
                    SectionKind::Text
                } else if readonly.unwrap_or(segment_name == b"__TEXT") {
                    SectionKind::ReadOnlyData
                } else {
                    SectionKind::Data
                }
            }
            _ => SectionKind::Unknown,
        }
    }
}

/// A trait for generic access to [`macho::Section32`] and [`macho::Section64`].
#[allow(missing_docs)]
pub trait Section: Debug + Pod + read::private::Sealed {
    type Word: Into<u64>;
    type Endian: endian::Endian;

    fn sectname(&self) -> &[u8; 16];
    fn segname(&self) -> &[u8; 16];
    fn addr(&self, endian: Self::Endian) -> Self::Word;
    fn size(&self, endian: Self::Endian) -> Self::Word;
    /// The file offset of the section.
    ///
    /// Note that for offsets larger than `u32::MAX` this will be truncated, so you will
    /// need to determine the offset in another way.
    /// `(section.addr - segment.vmaddr) + segment.fileoff` is equivalent for well-formed files.
    /// Alternatively, you can use [`Segment::section_offsets`](super::Segment::section_offsets)
    /// to track the file offset of consecutive sections to determine when overflow occurs.
    fn offset(&self, endian: Self::Endian) -> u32;
    fn align(&self, endian: Self::Endian) -> u32;
    fn reloff(&self, endian: Self::Endian) -> u32;
    fn nreloc(&self, endian: Self::Endian) -> u32;
    fn flags(&self, endian: Self::Endian) -> macho::SectionFlags;
    fn reserved1(&self, endian: Self::Endian) -> u32;
    fn reserved2(&self, endian: Self::Endian) -> u32;
    fn reserved3(&self, endian: Self::Endian) -> u32;

    /// Return the `sectname` bytes up until the null terminator.
    fn name(&self) -> &[u8] {
        let sectname = &self.sectname()[..];
        match memchr::memchr(b'\0', sectname) {
            Some(end) => &sectname[..end],
            None => sectname,
        }
    }

    /// Return the `segname` bytes up until the null terminator.
    fn segment_name(&self) -> &[u8] {
        let segname = &self.segname()[..];
        match memchr::memchr(b'\0', segname) {
            Some(end) => &segname[..end],
            None => segname,
        }
    }

    /// Return the section type from the flags field.
    fn section_type(&self, endian: Self::Endian) -> macho::SectionType {
        self.flags(endian).typ()
    }

    /// Return the size of the section in the file.
    ///
    /// Returns `None` for sections that have no data in the file.
    fn file_size(&self, endian: Self::Endian) -> Option<u64> {
        match self.section_type(endian) {
            macho::S_ZEROFILL | macho::S_GB_ZEROFILL | macho::S_THREAD_LOCAL_ZEROFILL => None,
            _ => Some(self.size(endian).into()),
        }
    }

    /// Return the offset and size of the section in the file.
    ///
    /// `offset` must be the section file offset. See [`Self::offset`] and
    /// [`Segment::section_offsets`](super::Segment::section_offsets).
    ///
    /// Returns `None` for sections that have no data in the file.
    fn file_range(&self, endian: Self::Endian, offset: u64) -> Option<(u64, u64)> {
        let size = self.file_size(endian)?;
        Some((offset, size))
    }

    /// Return the section data.
    ///
    /// `offset` must be the section file offset. See [`Self::offset`] and
    /// [`Segment::section_offsets`](super::Segment::section_offsets).
    ///
    /// Returns `Ok(&[])` if the section has no data.
    /// Returns `Err` for invalid values.
    fn data<'data, R: ReadRef<'data>>(
        &self,
        endian: Self::Endian,
        data: R,
        offset: u64,
    ) -> Result<&'data [u8]> {
        if let Some(size) = self.file_size(endian) {
            data.read_bytes_at(offset, size)
                .read_error("Invalid Mach-O section size or offset")
        } else {
            Ok(&[])
        }
    }

    /// Return the relocation array.
    ///
    /// Returns `Err` for invalid values.
    fn relocations<'data, R: ReadRef<'data>>(
        &self,
        endian: Self::Endian,
        data: R,
    ) -> Result<&'data [macho::Relocation<Self::Endian>]> {
        data.read_slice_at(self.reloff(endian).into(), self.nreloc(endian) as usize)
            .read_error("Invalid Mach-O relocations offset or number")
    }

    /// Return the size of symbol stubs in this section.
    ///
    /// Returns 0 if this section does not contain symbol stubs.
    fn symbol_stub_size(&self, endian: Self::Endian) -> u32 {
        if self.section_type(endian) == macho::S_SYMBOL_STUBS {
            self.reserved2(endian)
        } else {
            0
        }
    }

    /// Return the indirect symbols referenced by this section.
    ///
    /// Returns an empty slice if this section does not reference indirect symbols.
    fn indirect_symbols<'data>(
        &self,
        endian: Self::Endian,
        indirect_symbols: &'data [U32<Self::Endian, macho::IndirectSymbol>],
    ) -> Result<&'data [U32<Self::Endian, macho::IndirectSymbol>]> {
        let entry_size = match self.section_type(endian) {
            macho::S_NON_LAZY_SYMBOL_POINTERS
            | macho::S_LAZY_SYMBOL_POINTERS
            | macho::S_LAZY_DYLIB_SYMBOL_POINTERS
            | macho::S_THREAD_LOCAL_VARIABLE_POINTERS => mem::size_of::<Self::Word>(),
            macho::S_SYMBOL_STUBS => {
                let reserved2 = self.reserved2(endian);
                if reserved2 == 0 {
                    return Err(Error("Invalid Mach-O stub size"));
                }
                reserved2 as usize
            }
            _ => return Ok(&[]),
        };
        let start = self.reserved1(endian) as usize;
        let count = self.size(endian).into() as usize / entry_size;
        indirect_symbols
            .get(start..)
            .and_then(|symbols| symbols.get(..count))
            .read_error("Invalid Mach-O indirect symbol index or count")
    }
}

impl<Endian: endian::Endian> read::private::Sealed for macho::Section32<Endian> {}

impl<Endian: endian::Endian> Section for macho::Section32<Endian> {
    type Word = u32;
    type Endian = Endian;

    fn sectname(&self) -> &[u8; 16] {
        &self.sectname
    }
    fn segname(&self) -> &[u8; 16] {
        &self.segname
    }
    fn addr(&self, endian: Self::Endian) -> Self::Word {
        self.addr.get(endian)
    }
    fn size(&self, endian: Self::Endian) -> Self::Word {
        self.size.get(endian)
    }
    fn offset(&self, endian: Self::Endian) -> u32 {
        self.offset.get(endian)
    }
    fn align(&self, endian: Self::Endian) -> u32 {
        self.align.get(endian)
    }
    fn reloff(&self, endian: Self::Endian) -> u32 {
        self.reloff.get(endian)
    }
    fn nreloc(&self, endian: Self::Endian) -> u32 {
        self.nreloc.get(endian)
    }
    fn flags(&self, endian: Self::Endian) -> macho::SectionFlags {
        self.flags.get(endian)
    }
    fn reserved1(&self, endian: Self::Endian) -> u32 {
        self.reserved1.get(endian)
    }
    fn reserved2(&self, endian: Self::Endian) -> u32 {
        self.reserved2.get(endian)
    }
    fn reserved3(&self, _endian: Self::Endian) -> u32 {
        0
    }
}

impl<Endian: endian::Endian> read::private::Sealed for macho::Section64<Endian> {}

impl<Endian: endian::Endian> Section for macho::Section64<Endian> {
    type Word = u64;
    type Endian = Endian;

    fn sectname(&self) -> &[u8; 16] {
        &self.sectname
    }
    fn segname(&self) -> &[u8; 16] {
        &self.segname
    }
    fn addr(&self, endian: Self::Endian) -> Self::Word {
        self.addr.get(endian)
    }
    fn size(&self, endian: Self::Endian) -> Self::Word {
        self.size.get(endian)
    }
    fn offset(&self, endian: Self::Endian) -> u32 {
        self.offset.get(endian)
    }
    fn align(&self, endian: Self::Endian) -> u32 {
        self.align.get(endian)
    }
    fn reloff(&self, endian: Self::Endian) -> u32 {
        self.reloff.get(endian)
    }
    fn nreloc(&self, endian: Self::Endian) -> u32 {
        self.nreloc.get(endian)
    }
    fn flags(&self, endian: Self::Endian) -> macho::SectionFlags {
        self.flags.get(endian)
    }
    fn reserved1(&self, endian: Self::Endian) -> u32 {
        self.reserved1.get(endian)
    }
    fn reserved2(&self, endian: Self::Endian) -> u32 {
        self.reserved2.get(endian)
    }
    fn reserved3(&self, endian: Self::Endian) -> u32 {
        self.reserved3.get(endian)
    }
}
