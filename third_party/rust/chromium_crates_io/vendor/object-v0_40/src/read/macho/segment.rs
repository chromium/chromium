use core::fmt::Debug;
use core::{result, slice, str};

use crate::endian::{self, Endianness};
use crate::macho;
use crate::pod::Pod;
use crate::read::{
    self, Error, ObjectSegment, Permissions, ReadError, ReadRef, Result, SegmentFlags,
};

use super::{LoadCommandData, MachHeader, MachOFile, Section};

/// An iterator for the segments in a [`MachOFile32`](super::MachOFile32).
pub type MachOSegmentIterator32<'data, 'file, Endian = Endianness, R = &'data [u8]> =
    MachOSegmentIterator<'data, 'file, macho::MachHeader32<Endian>, R>;
/// An iterator for the segments in a [`MachOFile64`](super::MachOFile64).
pub type MachOSegmentIterator64<'data, 'file, Endian = Endianness, R = &'data [u8]> =
    MachOSegmentIterator<'data, 'file, macho::MachHeader64<Endian>, R>;

/// An iterator for the segments in a [`MachOFile`].
#[derive(Debug)]
pub struct MachOSegmentIterator<'data, 'file, Mach, R = &'data [u8]>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    pub(super) file: &'file MachOFile<'data, Mach, R>,
    pub(super) iter: slice::Iter<'file, MachOSegmentInternal<'data, Mach, R>>,
}

impl<'data, 'file, Mach, R> Iterator for MachOSegmentIterator<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    type Item = MachOSegment<'data, 'file, Mach, R>;

    fn next(&mut self) -> Option<Self::Item> {
        self.iter.next().map(|internal| MachOSegment {
            file: self.file,
            internal,
        })
    }
}

/// A segment in a [`MachOFile32`](super::MachOFile32).
pub type MachOSegment32<'data, 'file, Endian = Endianness, R = &'data [u8]> =
    MachOSegment<'data, 'file, macho::MachHeader32<Endian>, R>;
/// A segment in a [`MachOFile64`](super::MachOFile64).
pub type MachOSegment64<'data, 'file, Endian = Endianness, R = &'data [u8]> =
    MachOSegment<'data, 'file, macho::MachHeader64<Endian>, R>;

/// A segment in a [`MachOFile`].
///
/// Most functionality is provided by the [`ObjectSegment`] trait implementation.
#[derive(Debug)]
pub struct MachOSegment<'data, 'file, Mach, R = &'data [u8]>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    file: &'file MachOFile<'data, Mach, R>,
    internal: &'file MachOSegmentInternal<'data, Mach, R>,
}

impl<'data, 'file, Mach, R> MachOSegment<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    /// Get the Mach-O file containing this segment.
    pub fn macho_file(&self) -> &'file MachOFile<'data, Mach, R> {
        self.file
    }

    /// Get the raw Mach-O segment structure.
    pub fn macho_segment(&self) -> &'data Mach::Segment {
        self.internal.segment
    }

    fn bytes(&self) -> Result<&'data [u8]> {
        self.internal
            .segment
            .data(self.file.endian, self.internal.data)
            .read_error("Invalid Mach-O segment size or offset")
    }
}

impl<'data, 'file, Mach, R> read::private::Sealed for MachOSegment<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
}

impl<'data, 'file, Mach, R> ObjectSegment<'data> for MachOSegment<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    #[inline]
    fn address(&self) -> u64 {
        self.internal.segment.vmaddr(self.file.endian).into()
    }

    #[inline]
    fn size(&self) -> u64 {
        self.internal.segment.vmsize(self.file.endian).into()
    }

    #[inline]
    fn align(&self) -> u64 {
        // Page size.
        0x1000
    }

    #[inline]
    fn file_range(&self) -> (u64, u64) {
        self.internal.segment.file_range(self.file.endian)
    }

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

    #[inline]
    fn name_bytes(&self) -> Result<Option<&[u8]>> {
        Ok(Some(self.internal.segment.name()))
    }

    #[inline]
    fn name(&self) -> Result<Option<&str>> {
        Ok(Some(
            str::from_utf8(self.internal.segment.name())
                .ok()
                .read_error("Non UTF-8 Mach-O segment name")?,
        ))
    }

    #[inline]
    fn flags(&self) -> SegmentFlags {
        let flags = self.internal.segment.flags(self.file.endian);
        let maxprot = self.internal.segment.maxprot(self.file.endian);
        let initprot = self.internal.segment.initprot(self.file.endian);
        SegmentFlags::MachO {
            flags,
            maxprot,
            initprot,
        }
    }

    #[inline]
    fn permissions(&self) -> Permissions {
        let maxprot = self.internal.segment.maxprot(self.file.endian);
        Permissions::new(
            maxprot.contains(macho::VM_PROT_READ),
            maxprot.contains(macho::VM_PROT_WRITE),
            maxprot.contains(macho::VM_PROT_EXECUTE),
        )
    }
}

#[derive(Debug, Clone, Copy)]
pub(super) struct MachOSegmentInternal<'data, Mach: MachHeader, R: ReadRef<'data>> {
    pub segment: &'data Mach::Segment,
    /// The data for the file that contains the segment data.
    ///
    /// This is required for dyld caches, where this may be a different subcache
    /// from the file containing the Mach-O load commands.
    pub data: R,
}

/// An iterator for the header and file offset of each section in a Mach-O segment.
///
/// Returned by [`Segment::section_offsets`].
#[derive(Debug, Clone)]
pub struct SectionOffsetIterator<'data, S: Segment> {
    endian: S::Endian,
    sections: slice::Iter<'data, S::Section>,
    overflow_possible: bool,
    prev_offset: u64,
    segment_end: u64,
}

impl<'data, S: Segment> Iterator for SectionOffsetIterator<'data, S> {
    type Item = Result<(&'data S::Section, u64)>;

    fn next(&mut self) -> Option<Self::Item> {
        let section = self.sections.next()?;
        let mut offset = u64::from(section.offset(self.endian));

        // Reconstruct the full file offset by assuming that sections are ordered by
        // file offset. This is a refinement of the algorithm used by LLVM.
        if self.overflow_possible {
            if let Some(size) = section.file_size(self.endian) {
                offset |= self.prev_offset & 0xffff_ffff_0000_0000;
                if offset < self.prev_offset {
                    offset = offset.wrapping_add(0x1_0000_0000);
                }
                let section_end = offset.wrapping_add(size);
                if section_end > self.segment_end {
                    // Fuse.
                    self.sections = Default::default();
                    return Some(Err(Error("Unsupported Mach-O large section offsets")));
                }
                self.prev_offset = section_end;
            }
        }

        Some(Ok((section, offset)))
    }
}

/// A trait for generic access to [`macho::SegmentCommand32`] and [`macho::SegmentCommand64`].
#[allow(missing_docs)]
pub trait Segment: Debug + Pod + read::private::Sealed {
    type Word: Into<u64>;
    type Endian: endian::Endian;
    type Section: Section<Endian = Self::Endian>;

    fn from_command(command: LoadCommandData<'_, Self::Endian>) -> Result<Option<(&Self, &[u8])>>;

    fn cmd(&self, endian: Self::Endian) -> macho::LoadCommandType;
    fn cmdsize(&self, endian: Self::Endian) -> u32;
    fn segname(&self) -> &[u8; 16];
    fn vmaddr(&self, endian: Self::Endian) -> Self::Word;
    fn vmsize(&self, endian: Self::Endian) -> Self::Word;
    fn fileoff(&self, endian: Self::Endian) -> Self::Word;
    fn filesize(&self, endian: Self::Endian) -> Self::Word;
    fn maxprot(&self, endian: Self::Endian) -> macho::VmProt;
    fn initprot(&self, endian: Self::Endian) -> macho::VmProt;
    fn nsects(&self, endian: Self::Endian) -> u32;
    fn flags(&self, endian: Self::Endian) -> macho::SegmentFlags;

    /// Return the `segname` bytes up until the null terminator.
    fn name(&self) -> &[u8] {
        let segname = &self.segname()[..];
        match memchr::memchr(b'\0', segname) {
            Some(end) => &segname[..end],
            None => segname,
        }
    }

    /// Return the offset and size of the segment in the file.
    fn file_range(&self, endian: Self::Endian) -> (u64, u64) {
        (self.fileoff(endian).into(), self.filesize(endian).into())
    }

    /// Get the segment data from the file data.
    ///
    /// Returns `Err` for invalid values.
    fn data<'data, R: ReadRef<'data>>(
        &self,
        endian: Self::Endian,
        data: R,
    ) -> result::Result<&'data [u8], ()> {
        let (offset, size) = self.file_range(endian);
        data.read_bytes_at(offset, size)
    }

    /// Get the array of sections from the data following the segment command.
    ///
    /// Returns `Err` for invalid values.
    fn sections<'data, R: ReadRef<'data>>(
        &self,
        endian: Self::Endian,
        section_data: R,
    ) -> Result<&'data [Self::Section]> {
        section_data
            .read_slice_at(0, self.nsects(endian) as usize)
            .read_error("Invalid Mach-O number of sections")
    }

    /// Return an iterator for the header and file offset of each section in this segment.
    ///
    /// Section headers only have 32-bit file offsets, which can overflow in files
    /// larger than 4GB. This iterator reconstructs the full offset by assuming that
    /// sections are ordered by file offset. Use this instead of [`Section::offset`]
    /// if you may need to parse large files.
    ///
    /// `sections` must be the sections for this segment, such as those returned by
    /// [`Self::sections`].
    fn section_offsets<'data>(
        &self,
        endian: Self::Endian,
        sections: &'data [Self::Section],
    ) -> SectionOffsetIterator<'data, Self> {
        let segment_fileoff: u64 = self.fileoff(endian).into();
        let segment_end = segment_fileoff.wrapping_add(self.filesize(endian).into());
        SectionOffsetIterator {
            endian,
            sections: sections.iter(),
            overflow_possible: segment_end > u32::MAX as u64,
            prev_offset: segment_fileoff,
            segment_end,
        }
    }
}

impl<Endian: endian::Endian> read::private::Sealed for macho::SegmentCommand32<Endian> {}

impl<Endian: endian::Endian> Segment for macho::SegmentCommand32<Endian> {
    type Word = u32;
    type Endian = Endian;
    type Section = macho::Section32<Self::Endian>;

    fn from_command(command: LoadCommandData<'_, Self::Endian>) -> Result<Option<(&Self, &[u8])>> {
        command.segment_32()
    }

    fn cmd(&self, endian: Self::Endian) -> macho::LoadCommandType {
        self.cmd.get(endian)
    }
    fn cmdsize(&self, endian: Self::Endian) -> u32 {
        self.cmdsize.get(endian)
    }
    fn segname(&self) -> &[u8; 16] {
        &self.segname
    }
    fn vmaddr(&self, endian: Self::Endian) -> Self::Word {
        self.vmaddr.get(endian)
    }
    fn vmsize(&self, endian: Self::Endian) -> Self::Word {
        self.vmsize.get(endian)
    }
    fn fileoff(&self, endian: Self::Endian) -> Self::Word {
        self.fileoff.get(endian)
    }
    fn filesize(&self, endian: Self::Endian) -> Self::Word {
        self.filesize.get(endian)
    }
    fn maxprot(&self, endian: Self::Endian) -> macho::VmProt {
        self.maxprot.get(endian)
    }
    fn initprot(&self, endian: Self::Endian) -> macho::VmProt {
        self.initprot.get(endian)
    }
    fn nsects(&self, endian: Self::Endian) -> u32 {
        self.nsects.get(endian)
    }
    fn flags(&self, endian: Self::Endian) -> macho::SegmentFlags {
        self.flags.get(endian)
    }
}

impl<Endian: endian::Endian> read::private::Sealed for macho::SegmentCommand64<Endian> {}

impl<Endian: endian::Endian> Segment for macho::SegmentCommand64<Endian> {
    type Word = u64;
    type Endian = Endian;
    type Section = macho::Section64<Self::Endian>;

    fn from_command(command: LoadCommandData<'_, Self::Endian>) -> Result<Option<(&Self, &[u8])>> {
        command.segment_64()
    }

    fn cmd(&self, endian: Self::Endian) -> macho::LoadCommandType {
        self.cmd.get(endian)
    }
    fn cmdsize(&self, endian: Self::Endian) -> u32 {
        self.cmdsize.get(endian)
    }
    fn segname(&self) -> &[u8; 16] {
        &self.segname
    }
    fn vmaddr(&self, endian: Self::Endian) -> Self::Word {
        self.vmaddr.get(endian)
    }
    fn vmsize(&self, endian: Self::Endian) -> Self::Word {
        self.vmsize.get(endian)
    }
    fn fileoff(&self, endian: Self::Endian) -> Self::Word {
        self.fileoff.get(endian)
    }
    fn filesize(&self, endian: Self::Endian) -> Self::Word {
        self.filesize.get(endian)
    }
    fn maxprot(&self, endian: Self::Endian) -> macho::VmProt {
        self.maxprot.get(endian)
    }
    fn initprot(&self, endian: Self::Endian) -> macho::VmProt {
        self.initprot.get(endian)
    }
    fn nsects(&self, endian: Self::Endian) -> u32 {
        self.nsects.get(endian)
    }
    fn flags(&self, endian: Self::Endian) -> macho::SegmentFlags {
        self.flags.get(endian)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::LittleEndian as LE;
    use crate::endian::{U32, U64};

    fn segment(fileoff: u64, filesize: u64) -> macho::SegmentCommand64<LE> {
        macho::SegmentCommand64 {
            cmd: U32::new(LE, macho::LC_SEGMENT_64),
            cmdsize: U32::new(LE, 0),
            segname: [0; 16],
            vmaddr: U64::new(LE, 0),
            vmsize: U64::new(LE, filesize),
            fileoff: U64::new(LE, fileoff),
            filesize: U64::new(LE, filesize),
            maxprot: U32::new(LE, macho::VmProt(0)),
            initprot: U32::new(LE, macho::VmProt(0)),
            nsects: U32::new(LE, 0),
            flags: U32::new(LE, macho::SegmentFlags(0)),
        }
    }

    fn section(offset: u32, size: u64, typ: macho::SectionType) -> macho::Section64<LE> {
        macho::Section64 {
            sectname: [0; 16],
            segname: [0; 16],
            addr: U64::new(LE, 0),
            size: U64::new(LE, size),
            offset: U32::new(LE, offset),
            align: U32::new(LE, 0),
            reloff: U32::new(LE, 0),
            nreloc: U32::new(LE, 0),
            flags: U32::new(LE, typ.to_flags()),
            reserved1: U32::new(LE, 0),
            reserved2: U32::new(LE, 0),
            reserved3: U32::new(LE, 0),
        }
    }

    fn assert_offsets(
        segment: &macho::SegmentCommand64<LE>,
        sections: &[macho::Section64<LE>],
        expected: &[u64],
    ) {
        let mut iter = segment.section_offsets(LE, sections);
        for expected in expected {
            let (_, offset) = iter.next().unwrap().unwrap();
            assert_eq!(offset, *expected);
        }
        assert!(iter.next().is_none());
    }

    #[test]
    fn section_offsets_small() {
        // Overflow not possible, order does not matter.
        let segment = segment(0x1000, 0x2000);
        let sections = [
            section(0x2000, 0x1000, macho::S_REGULAR),
            section(0x1000, 0x1000, macho::S_REGULAR),
        ];
        assert_offsets(&segment, &sections, &[0x2000, 0x1000]);
    }

    #[test]
    fn section_offsets_large() {
        // Overflow possible.
        let segment = segment(0, 0x2_0000_1000);
        let sections = [
            section(0x1000, 0xffff_e000, macho::S_REGULAR),
            section(0xffff_f000, 0x2000, macho::S_REGULAR),
            // Truncated from 0x1_0000_1000.
            section(0x1000, 0x1000, macho::S_REGULAR),
            // Zerofill sections have no file data, so they are left alone.
            section(0, 0x1000, macho::S_ZEROFILL),
            // Truncated from 0x1_0000_2000.
            section(0x2000, 0xffff_d000, macho::S_REGULAR),
            // Truncated from 0x2_0000_0000, tests `offset < self.prev_offset` branch.
            section(0, 0x1000, macho::S_REGULAR),
        ];
        assert_offsets(
            &segment,
            &sections,
            &[
                0x1000,
                0xffff_f000,
                0x1_0000_1000,
                0,
                0x1_0000_2000,
                0x2_0000_0000,
            ],
        );
    }

    #[test]
    fn section_offsets_invalid() {
        // Overflow possible, but total section size is invalid.
        let segment = segment(0, 0x1_0000_1000);
        let sections = [
            section(0x1000, 0x1000, macho::S_REGULAR),
            section(0x2000, 0x1_0000_0000, macho::S_REGULAR),
            // Fused before here.
            section(0x3000, 0x1000, macho::S_REGULAR),
        ];
        let mut iter = segment.section_offsets(LE, &sections);
        assert_eq!(iter.next().unwrap().unwrap().1, 0x1000);
        assert!(iter.next().unwrap().is_err());
        assert!(iter.next().is_none());
    }
}
