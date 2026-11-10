use crate::common::{DebugInfoOffset, Format, SectionId};
use crate::constants::GdbIndexSymbolKind;
use crate::endianity::Endianity;
use crate::read::lookup::{DebugPubSet, PubSet, PubSetEntry, PubSetEntryIter, PubSetIter};
use crate::read::{EndianSlice, Reader, Result, Section, UnitOffset};

/// The `DebugPubNames` struct represents the DWARF public names information
/// found in the `.debug_pubnames` section.
#[derive(Debug, Clone)]
pub struct DebugPubNames<R: Reader>(DebugPubSet<R>);

impl<'input, Endian> DebugPubNames<EndianSlice<'input, Endian>>
where
    Endian: Endianity,
{
    /// Construct a new `DebugPubNames` instance from the data in the `.debug_pubnames`
    /// section.
    ///
    /// It is the caller's responsibility to read the `.debug_pubnames` section and
    /// present it as a `&[u8]` slice. That means using some ELF loader on
    /// Linux, a Mach-O loader on macOS, etc.
    ///
    /// ```
    /// use gimli::{DebugPubNames, LittleEndian};
    ///
    /// # let buf = [];
    /// # let read_debug_pubnames_section_somehow = || &buf;
    /// let debug_pubnames =
    ///     DebugPubNames::new(read_debug_pubnames_section_somehow(), LittleEndian);
    /// ```
    pub fn new(section: &'input [u8], endian: Endian) -> Self {
        Self::from(EndianSlice::new(section, endian))
    }
}

impl<R: Reader> DebugPubNames<R> {
    /// Iterate the pubnames in the `.debug_pubnames` section.
    ///
    /// ```
    /// use gimli::{DebugPubNames, EndianSlice, LittleEndian};
    ///
    /// # let buf = [];
    /// # let read_debug_pubnames_section_somehow = || &buf;
    /// let debug_pubnames =
    ///     DebugPubNames::new(read_debug_pubnames_section_somehow(), LittleEndian);
    ///
    /// let mut iter = debug_pubnames.items();
    /// while let Some(pubname) = iter.next().unwrap() {
    ///   println!("pubname {} found!", pubname.name().to_string_lossy());
    /// }
    /// ```
    pub fn items(&self) -> PubNamesEntryIter<R> {
        PubNamesEntryIter(self.0.items(false))
    }

    /// Iterate the sets of entries in the `.debug_pubnames` section.
    ///
    /// Each set corresponds to a single unit, and contains the header for that
    /// unit followed by its entries.
    pub fn sets(&self) -> PubNamesSetIter<R> {
        PubNamesSetIter(self.0.sets(false))
    }
}

impl<R: Reader> Section<R> for DebugPubNames<R> {
    fn id() -> SectionId {
        SectionId::DebugPubNames
    }

    fn reader(&self) -> &R {
        &self.0.section
    }
}

impl<R: Reader> From<R> for DebugPubNames<R> {
    fn from(section: R) -> Self {
        DebugPubNames(DebugPubSet { section })
    }
}

/// The `DebugGnuPubNames` struct represents the DWARF public names information
/// found in the `.debug_gnu_pubnames` section.
#[derive(Debug, Clone)]
pub struct DebugGnuPubNames<R: Reader>(DebugPubSet<R>);

impl<'input, Endian> DebugGnuPubNames<EndianSlice<'input, Endian>>
where
    Endian: Endianity,
{
    /// Construct a new `DebugGnuPubNames` instance from the data in the `.debug_gnu_pubnames`
    /// section.
    ///
    /// It is the caller's responsibility to read the `.debug_gnu_pubnames` section and
    /// present it as a `&[u8]` slice. That means using some ELF loader on
    /// Linux, a Mach-O loader on macOS, etc.
    ///
    /// ```
    /// use gimli::{DebugGnuPubNames, LittleEndian};
    ///
    /// # let buf = [];
    /// # let read_debug_gnu_pubnames_section_somehow = || &buf;
    /// let debug_gnu_pubnames =
    ///     DebugGnuPubNames::new(read_debug_gnu_pubnames_section_somehow(), LittleEndian);
    /// ```
    pub fn new(section: &'input [u8], endian: Endian) -> Self {
        Self::from(EndianSlice::new(section, endian))
    }
}

impl<R: Reader> DebugGnuPubNames<R> {
    /// Iterate the pubnames in the `.debug_gnu_pubnames` section.
    ///
    /// ```
    /// use gimli::{DebugGnuPubNames, EndianSlice, LittleEndian};
    ///
    /// # let buf = [];
    /// # let read_debug_gnu_pubnames_section_somehow = || &buf;
    /// let debug_gnu_pubnames =
    ///     DebugGnuPubNames::new(read_debug_gnu_pubnames_section_somehow(), LittleEndian);
    ///
    /// let mut iter = debug_gnu_pubnames.items();
    /// while let Some(pubname) = iter.next().unwrap() {
    ///   println!("pubname {} found!", pubname.name().to_string_lossy());
    /// }
    /// ```
    pub fn items(&self) -> PubNamesEntryIter<R> {
        PubNamesEntryIter(self.0.items(true))
    }

    /// Iterate the sets of entries in the `.debug_gnu_pubnames` section.
    ///
    /// Each set corresponds to a single unit, and contains the header for that
    /// unit followed by its entries.
    pub fn sets(&self) -> PubNamesSetIter<R> {
        PubNamesSetIter(self.0.sets(true))
    }
}

impl<R: Reader> Section<R> for DebugGnuPubNames<R> {
    fn id() -> SectionId {
        SectionId::DebugGnuPubNames
    }

    fn reader(&self) -> &R {
        &self.0.section
    }
}

impl<R: Reader> From<R> for DebugGnuPubNames<R> {
    fn from(section: R) -> Self {
        DebugGnuPubNames(DebugPubSet { section })
    }
}

/// An iterator over the pubnames from a `.debug_pubnames` or `.debug_gnu_pubnames` section.
#[derive(Debug, Clone)]
pub struct PubNamesSetIter<R: Reader>(PubSetIter<R>);

impl<R: Reader> PubNamesSetIter<R> {
    /// Advance the iterator and return the next set.
    ///
    /// Returns the newly parsed set as `Ok(Some(set))`. Returns `Ok(None)` when
    /// iteration is complete. If an error occurs while parsing the next header,
    /// then this error is returned as `Err(e)`, and all subsequent calls return
    /// `Ok(None)`.
    pub fn next(&mut self) -> Result<Option<PubNamesSet<R>>> {
        self.0.next().map(|x| x.map(PubNamesSet))
    }
}

#[cfg(feature = "fallible-iterator")]
impl<R: Reader> fallible_iterator::FallibleIterator for PubNamesSetIter<R> {
    type Item = PubNamesSet<R>;
    type Error = crate::read::Error;

    fn next(&mut self) -> ::core::result::Result<Option<Self::Item>, Self::Error> {
        self.next()
    }
}

impl<R: Reader> Iterator for PubNamesSetIter<R> {
    type Item = Result<PubNamesSet<R>>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}

/// A set of entries that share a header in a `.debug_pubnames` or `.debug_gnu_pubnames` section.
///
/// These entries all belong to a single unit.
#[derive(Debug, Clone)]
pub struct PubNamesSet<R: Reader>(PubSet<R>);

impl<R: Reader> PubNamesSet<R> {
    /// Returns the offset into the `.debug_info` section for the header of the
    /// compilation unit which contains these names.
    pub fn unit_header_offset(&self) -> DebugInfoOffset<R::Offset> {
        self.0.header.unit_offset
    }

    /// Returns the length of the compilation unit in the `.debug_info` section
    /// which contains these names.
    pub fn unit_length(&self) -> R::Offset {
        self.0.header.unit_length
    }

    /// Returns the version of the set.
    pub fn version(&self) -> u16 {
        self.0.header.version
    }

    /// Returns the DWARF format of the set.
    pub fn format(&self) -> Format {
        self.0.header.format
    }

    /// Returns the length of the set, including the header.
    pub fn length(&self) -> R::Offset {
        self.0.header.length
    }

    /// Iterate the entries in this set.
    pub fn items(&self) -> PubNamesEntryIter<R> {
        PubNamesEntryIter(self.0.items())
    }
}

/// An iterator over the pubnames from a `.debug_pubnames` or `.debug_gnu_pubnames` section.
#[derive(Debug, Clone)]
pub struct PubNamesEntryIter<R: Reader>(PubSetEntryIter<R>);

impl<R: Reader> PubNamesEntryIter<R> {
    /// Advance the iterator and return the next pubname.
    ///
    /// Returns the newly parsed pubname as `Ok(Some(pubname))`. Returns
    /// `Ok(None)` when iteration is complete and all pubnames have already been
    /// parsed and yielded. If an error occurs while parsing the next pubname,
    /// then this error is returned as `Err(e)`, and all subsequent calls return
    /// `Ok(None)`.
    pub fn next(&mut self) -> Result<Option<PubNamesEntry<R>>> {
        self.0.next().map(|x| x.map(PubNamesEntry))
    }
}

#[cfg(feature = "fallible-iterator")]
impl<R: Reader> fallible_iterator::FallibleIterator for PubNamesEntryIter<R> {
    type Item = PubNamesEntry<R>;
    type Error = crate::read::Error;

    fn next(&mut self) -> ::core::result::Result<Option<Self::Item>, Self::Error> {
        self.next()
    }
}

impl<R: Reader> Iterator for PubNamesEntryIter<R> {
    type Item = Result<PubNamesEntry<R>>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}

/// A single parsed pubname.
#[derive(Debug, Clone)]
pub struct PubNamesEntry<R: Reader>(PubSetEntry<R>);

impl<R: Reader> PubNamesEntry<R> {
    /// Returns the name this entry refers to.
    pub fn name(&self) -> &R {
        &self.0.name
    }

    /// Returns the offset into the .debug_info section for the header of the compilation unit
    /// which contains this name.
    pub fn unit_header_offset(&self) -> DebugInfoOffset<R::Offset> {
        self.0.unit_header_offset
    }

    /// Returns the offset into the compilation unit for the debugging information entry which
    /// has this name.
    pub fn die_offset(&self) -> UnitOffset<R::Offset> {
        self.0.die_offset
    }

    /// Return the symbol kind.
    ///
    /// The compiler derives this from the tag of the DIE.
    ///
    /// Only .debug_gnu_pubnames entries contain this value.
    /// Always returns `GDB_INDEX_SYMBOL_KIND_NONE` for a .debug_pubnames entry.
    pub fn kind(&self) -> GdbIndexSymbolKind {
        self.0.kind()
    }

    /// Return true if the symbol is static.
    ///
    /// Always returns false for a .debug_pubnames entry.
    pub fn is_static(&self) -> bool {
        self.0.is_static()
    }

    /// Return flags.
    ///
    /// Only .debug_gnu_pubnames entries contain this value.
    pub fn flags(&self) -> u8 {
        self.0.flags()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::common::DebugInfoOffset;
    use crate::constants::*;
    use crate::test_util::GimliSectionMethods;
    use crate::{Format, LittleEndian};
    use test_assembler::{Endian, Label, LabelMaker, Section};

    #[test]
    fn test_pubnames() {
        for format in [Format::Dwarf32, Format::Dwarf64] {
            let size = format.word_size();

            // Set 1, with two entries.
            let length = Label::new();
            let start = Label::new();
            let section = Section::with_endian(Endian::Little)
                .initial_length(format, &length, &start)
                .D16(2) // Version
                .word(size, 0x10) // Unit header offset
                .word(size, 0x20) // Unit length
                // Entry 1
                .word(size, 0x02) // DIE offset
                .append_bytes(b"foo\0")
                // Entry 2
                .word(size, 0x04) // DIE offset
                .append_bytes(b"bar\0")
                // Null entry
                .word(size, 0)
                .set_initial_length(&length, &start);

            // Set 2, with one entry.
            let length = Label::new();
            let start = Label::new();
            let section = section
                .initial_length(format, &length, &start)
                .D16(2) // Version
                .word(size, 0x40) // Unit header offset
                .word(size, 0x30) // Unit length
                // Entry 1
                .word(size, 0x06) // DIE offset
                .append_bytes(b"baz\0")
                // Null entry
                .word(size, 0)
                .set_initial_length(&length, &start);

            let section = section.get_contents().unwrap();
            let debug_pubnames = DebugPubNames::new(&section, LittleEndian);

            // Iterate all entries.
            let mut items = debug_pubnames.items();

            let entry = items.next().unwrap().unwrap();
            assert_eq!(entry.name(), &EndianSlice::new(b"foo", LittleEndian));
            assert_eq!(entry.unit_header_offset(), DebugInfoOffset(0x10));
            assert_eq!(entry.die_offset(), UnitOffset(0x02));
            assert_eq!(entry.kind(), GDB_INDEX_SYMBOL_KIND_NONE);
            assert!(!entry.is_static());

            let entry = items.next().unwrap().unwrap();
            assert_eq!(entry.name(), &EndianSlice::new(b"bar", LittleEndian));
            assert_eq!(entry.unit_header_offset(), DebugInfoOffset(0x10));
            assert_eq!(entry.die_offset(), UnitOffset(0x04));

            // Iteration continues into the next set.
            let entry = items.next().unwrap().unwrap();
            assert_eq!(entry.name(), &EndianSlice::new(b"baz", LittleEndian));
            assert_eq!(entry.unit_header_offset(), DebugInfoOffset(0x40));
            assert_eq!(entry.die_offset(), UnitOffset(0x06));

            assert!(matches!(items.next(), Ok(None)));

            // Iterate entries within sets.
            let mut sets = debug_pubnames.sets();

            // Set 1.
            let set = sets.next().unwrap().unwrap();
            assert_eq!(set.version(), 2);
            assert_eq!(set.format(), format);
            assert_eq!(set.unit_header_offset(), DebugInfoOffset(0x10));
            assert_eq!(set.unit_length(), 0x20);

            let mut items = set.items();
            let entry = items.next().unwrap().unwrap();
            assert_eq!(entry.name(), &EndianSlice::new(b"foo", LittleEndian));
            assert_eq!(entry.die_offset(), UnitOffset(0x02));
            let entry = items.next().unwrap().unwrap();
            assert_eq!(entry.name(), &EndianSlice::new(b"bar", LittleEndian));
            assert_eq!(entry.die_offset(), UnitOffset(0x04));
            // Iteration stops at the end of this set.
            assert!(matches!(items.next(), Ok(None)));

            // Set 2.
            let set = sets.next().unwrap().unwrap();
            assert_eq!(set.unit_header_offset(), DebugInfoOffset(0x40));
            assert_eq!(set.unit_length(), 0x30);

            let mut items = set.items();
            let entry = items.next().unwrap().unwrap();
            assert_eq!(entry.name(), &EndianSlice::new(b"baz", LittleEndian));
            assert_eq!(entry.die_offset(), UnitOffset(0x06));
            assert!(matches!(items.next(), Ok(None)));

            assert!(matches!(sets.next(), Ok(None)));
        }
    }

    #[test]
    fn test_gnu_pubnames() {
        for format in [Format::Dwarf32, Format::Dwarf64] {
            let size = format.word_size();

            let length = Label::new();
            let start = Label::new();
            let section = Section::with_endian(Endian::Little)
                .initial_length(format, &length, &start)
                .D16(2) // Version
                .word(size, 0x10) // Unit header offset
                .word(size, 0x20) // Unit length
                // Entry 1
                .word(size, 0x02)
                .D8(0xb0) // static (0x80) | function (3 << 4) = 0xb0.
                .append_bytes(b"foo\0")
                // Entry 2
                .word(size, 0x04)
                .D8(0x20) // global (0x00) | variable (2 << 4) = 0x20.
                .append_bytes(b"bar\0")
                // Null entry
                .word(size, 0)
                .set_initial_length(&length, &start);

            let section = section.get_contents().unwrap();
            let debug_gnu_pubnames = DebugGnuPubNames::new(&section, LittleEndian);
            let mut items = debug_gnu_pubnames.items();

            let entry = items.next().unwrap().unwrap();
            assert_eq!(entry.name(), &EndianSlice::new(b"foo", LittleEndian));
            assert_eq!(entry.unit_header_offset(), DebugInfoOffset(0x10));
            assert_eq!(entry.die_offset(), UnitOffset(0x02));
            assert_eq!(entry.kind(), GDB_INDEX_SYMBOL_KIND_FUNCTION);
            assert!(entry.is_static());
            assert_eq!(entry.flags(), 0xb0);

            let entry = items.next().unwrap().unwrap();
            assert_eq!(entry.name(), &EndianSlice::new(b"bar", LittleEndian));
            assert_eq!(entry.kind(), GDB_INDEX_SYMBOL_KIND_VARIABLE);
            assert!(!entry.is_static());
            assert_eq!(entry.flags(), 0x20);

            assert!(matches!(items.next(), Ok(None)));
        }
    }
}
