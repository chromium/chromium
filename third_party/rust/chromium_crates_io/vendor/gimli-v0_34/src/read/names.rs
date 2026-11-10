//! Functions for parsing DWARF 5 `.debug_names` sections.
//!
//! The `.debug_names` section provides an accelerated access table for debugging
//! information entries (DIEs) organized by name. This section is defined in
//! DWARF 5 Section 6.1.1 and enables efficient lookup of symbols without
//! scanning the entire `.debug_info` section.
//!
//! # DWARF 5 Name Index
//!
//! A name index in the `.debug_names` section contains:
//! - **Header**: Format, version, and table counts
//! - **CU/TU Lists**: Lists of compilation and type units
//! - **Hash Table**: Bucket-based hash table for name lookup
//! - **Name Table**: String and entry offsets for each name
//! - **Abbreviation Table**: Describes entry structure and attributes
//! - **Entry Pool**: Series of entries with abbreviation codes and attributes
//!
//! Per DWARF 5 Section 6.1.1.3, a `.debug_names` section can contain multiple
//! name indexes. There are two strategies:
//! - **Per-module index**: Single index covering all compilation units (most common)
//! - **Per-CU indexes**: Separate indexes for individual compilation units
//!
//! The choice depends on the compiler/linker. When looking up names, all indexes
//! must be searched since a name could appear in any index.
//!
use crate::common::{
    DebugInfoOffset, DebugNamesOffset, DebugStrOffset, DebugTypeSignature, Format, SectionId,
};
use crate::constants;
use crate::endianity::Endianity;
use crate::read::{
    DebugStr, EndianSlice, Error, Reader, ReaderOffset, Result, Section, UnitOffset,
};
use alloc::vec::Vec;

/// The `DebugNames` struct represents the DWARF 5 name index information
/// found in the `.debug_names` section.
///
/// The `.debug_names` section provides an index for efficiently finding
/// debugging information entries (DIEs) by name. It contains hash tables
/// that map names to DIE offsets, allowing debuggers to quickly locate
/// functions, variables, types, and other named entities.
#[derive(Debug, Default, Clone, Copy)]
pub struct DebugNames<R> {
    section: R,
}

impl<'input, Endian> DebugNames<EndianSlice<'input, Endian>>
where
    Endian: Endianity,
{
    /// Construct a new `DebugNames` instance from the data in the `.debug_names`
    /// section.
    ///
    /// It is the caller's responsibility to read the `.debug_names` section and
    /// present it as a `&[u8]` slice. That means using some ELF loader on
    /// Linux, a Mach-O loader on macOS, etc.
    ///
    /// ```
    /// use gimli::{DebugNames, LittleEndian};
    ///
    /// # let buf = [0x00, 0x01, 0x02, 0x03];
    /// # let read_debug_names_section_somehow = || &buf;
    /// let debug_names =
    ///     DebugNames::new(read_debug_names_section_somehow(), LittleEndian);
    /// ```
    pub fn new(debug_names_section: &'input [u8], endian: Endian) -> Self {
        Self::from(EndianSlice::new(debug_names_section, endian))
    }
}

impl<T> DebugNames<T> {
    /// Create a `DebugNames` section that references the data in `self`.
    ///
    /// This is useful when `R` implements `Reader` but `T` does not.
    ///
    /// Used by `DwarfSections::borrow`.
    pub fn borrow<'a, F, R>(&'a self, mut borrow: F) -> DebugNames<R>
    where
        F: FnMut(&'a T) -> R,
    {
        borrow(&self.section).into()
    }
}

impl<R: Reader> DebugNames<R> {
    /// Iterate over all name indexes in the `.debug_names` section.
    pub fn headers(&self) -> NameIndexHeaderIter<R> {
        NameIndexHeaderIter {
            input: self.section.clone(),
            end_offset: self.section.len(),
        }
    }
}

impl<R> Section<R> for DebugNames<R> {
    fn id() -> SectionId {
        SectionId::DebugNames
    }

    fn reader(&self) -> &R {
        &self.section
    }
}

impl<R> From<R> for DebugNames<R> {
    fn from(debug_names_section: R) -> Self {
        DebugNames {
            section: debug_names_section,
        }
    }
}

/// An iterator over the name index headers in the `.debug_names` section.
#[derive(Debug, Clone)]
pub struct NameIndexHeaderIter<R: Reader> {
    input: R,
    end_offset: R::Offset,
}

impl<R: Reader> NameIndexHeaderIter<R> {
    /// Advance the iterator and return the next name index header.
    ///
    /// Returns `Ok(None)` when iteration is complete.
    pub fn next(&mut self) -> Result<Option<NameIndexHeader<R>>> {
        if self.input.is_empty() {
            return Ok(None);
        }

        let offset = DebugNamesOffset(self.end_offset - self.input.len());
        let result = NameIndexHeader::parse(&mut self.input, offset).map(Some);
        if result.is_err() {
            self.input.empty();
        }
        result
    }
}

impl<R: Reader> Iterator for NameIndexHeaderIter<R> {
    type Item = Result<NameIndexHeader<R>>;

    fn next(&mut self) -> Option<Self::Item> {
        NameIndexHeaderIter::next(self).transpose()
    }
}

/// The header of a name index in the `.debug_names` section.
#[derive(Debug, Clone)]
pub struct NameIndexHeader<R: Reader> {
    /// The section offset of the header.
    offset: DebugNamesOffset<R::Offset>,
    /// The length of this name index.
    length: R::Offset,
    /// The format of the unit.
    format: Format,
    /// Version of the name index format (should be 5 for DWARF 5).
    version: u16,
    /// Number of compilation units in the CU list.
    compile_unit_count: u32,
    /// Number of type units in the local TU list.
    local_type_unit_count: u32,
    /// Number of type units in the foreign TU list.
    foreign_type_unit_count: u32,
    /// Number of buckets in the hash table.
    bucket_count: u32,
    /// Number of unique name entries.
    name_count: u32,
    /// Size of the abbreviations table in bytes.
    abbrev_table_size: u32,
    /// The augmentation string.
    augmentation_string: Option<R>,
    /// The remaining unparsed contents of the index.
    content: R,
}

impl<R: Reader> NameIndexHeader<R> {
    /// Convert the header into a `NameIndex`.
    pub fn index(self) -> Result<NameIndex<R>> {
        NameIndex::new(self)
    }

    /// Return the section offset of this name index.
    #[inline]
    pub fn offset(&self) -> DebugNamesOffset<R::Offset> {
        self.offset
    }

    /// Return the index length.
    #[inline]
    pub fn length(&self) -> R::Offset {
        self.length
    }

    /// Return the format (DWARF32 or DWARF64).
    #[inline]
    pub fn format(&self) -> Format {
        self.format
    }

    /// Return the version of this name index.
    #[inline]
    pub fn version(&self) -> u16 {
        self.version
    }

    /// Return the number of compilation units in this index.
    #[inline]
    pub fn compile_unit_count(&self) -> u32 {
        self.compile_unit_count
    }

    /// Return the number of local type units in this index.
    #[inline]
    pub fn local_type_unit_count(&self) -> u32 {
        self.local_type_unit_count
    }

    /// Return the number of foreign type units in this index.
    #[inline]
    pub fn foreign_type_unit_count(&self) -> u32 {
        self.foreign_type_unit_count
    }

    /// Return the number of buckets in the hash table.
    #[inline]
    pub fn bucket_count(&self) -> u32 {
        self.bucket_count
    }

    /// Return the number of unique name entries.
    #[inline]
    pub fn name_count(&self) -> u32 {
        self.name_count
    }

    /// Return the size of the abbreviations table in bytes.
    #[inline]
    pub fn abbrev_table_size(&self) -> u32 {
        self.abbrev_table_size
    }

    /// Return the augmentation string.
    #[inline]
    pub fn augmentation_string(&self) -> Option<&R> {
        self.augmentation_string.as_ref()
    }

    fn parse(input: &mut R, offset: DebugNamesOffset<R::Offset>) -> Result<Self> {
        let (length, format) = input.read_initial_length()?;
        let mut input = input.split(length)?;

        let version = input.read_u16()?;

        if version != 5 {
            return Err(Error::UnknownVersion(version as u64));
        }

        input.skip(R::Offset::from_u8(2))?; // Padding
        let compile_unit_count = input.read_u32()?;
        let local_type_unit_count = input.read_u32()?;
        let foreign_type_unit_count = input.read_u32()?;
        let bucket_count = input.read_u32()?;
        let name_count = input.read_u32()?;
        let abbrev_table_size = input.read_u32()?;
        let augmentation_string_size = input.read_u32()?;
        let augmentation_string = if augmentation_string_size > 0 {
            let val = input.split(R::Offset::from_u32(augmentation_string_size))?;
            input.skip(R::Offset::from_u32(
                (4 - (augmentation_string_size & 3)) & 3,
            ))?;
            Some(val)
        } else {
            None
        };

        Ok(NameIndexHeader {
            offset,
            length,
            format,
            version,
            compile_unit_count,
            local_type_unit_count,
            foreign_type_unit_count,
            bucket_count,
            name_count,
            abbrev_table_size,
            augmentation_string,
            content: input,
        })
    }
}

/// An index into the name table of a `NameIndex`.
///
/// This is used as an index into the list of string offsets, the list of entry
/// offsets, and the list of hashes.
///
/// Note that while the DWARF standard specifies that indexes in the DWARF data
/// start at 1, we use a zero based index here. Functions that read an index from
/// the data will automatically adjust the index to start at 0.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct NameTableIndex(pub u32);

/// A reference to a type unit.
///
/// This is the result of looking up a type unit index obtained from a `DW_IDX_type_unit`
/// attribute.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NameTypeUnit<T> {
    /// The offset of a local type unit in the `.debug_info` section.
    Local(DebugInfoOffset<T>),
    /// The type signature of a foreign type unit.
    Foreign(DebugTypeSignature),
}

/// A single name index from the `.debug_names` section.
///
/// It provides access to the compilation unit table, type unit tables, hash table, name
/// table, and entry pool that make up the accelerated lookup structure.
#[derive(Debug)]
pub struct NameIndex<R: Reader> {
    format: Format,
    compile_unit_count: u32,
    local_type_unit_count: u32,
    foreign_type_unit_count: u32,
    bucket_count: u32,
    name_count: u32,

    // Pre-sliced readers for each section
    compile_unit_list: R,
    local_type_unit_list: R,
    foreign_type_unit_list: R,
    bucket_data: R,
    hash_table_data: R,
    name_table_data: R,
    entry_offset_data: R,
    entry_pool: R,

    abbreviations: NameAbbreviations,
}

impl<R: Reader> NameIndex<R> {
    /// Create a new name index from a header.
    pub fn new(header: NameIndexHeader<R>) -> Result<Self> {
        let mut reader = header.content;

        // Calculate section sizes once
        let offset_size = header.format.word_size() as u64;

        let cu_list_size = header.compile_unit_count as u64 * offset_size;
        let local_tu_size = header.local_type_unit_count as u64 * offset_size;
        let foreign_tu_size = header.foreign_type_unit_count as u64 * 8; // Always 8 bytes per signature
        let buckets_size = header.bucket_count as u64 * 4;
        let hash_table_size = if header.bucket_count == 0 {
            0
        } else {
            header.name_count as u64 * 4
        };
        let name_table_size = header.name_count as u64 * offset_size;
        let abbrev_size = header.abbrev_table_size as u64;

        // Slice each section once (split() advances the reader automatically)
        let compile_unit_list = reader.split(R::Offset::from_u64(cu_list_size)?)?;
        let local_type_unit_list = reader.split(R::Offset::from_u64(local_tu_size)?)?;
        let foreign_type_unit_list = reader.split(R::Offset::from_u64(foreign_tu_size)?)?;
        let bucket_data = reader.split(R::Offset::from_u64(buckets_size)?)?;
        let hash_table_data = reader.split(R::Offset::from_u64(hash_table_size)?)?;
        let name_table_data = reader.split(R::Offset::from_u64(name_table_size)?)?;
        let entry_offset_data = reader.split(R::Offset::from_u64(name_table_size)?)?;
        let abbreviation_table = reader.split(R::Offset::from_u64(abbrev_size)?)?;

        let abbreviations = NameAbbreviations::parse(abbreviation_table)?;

        // Remaining data is the entry pool
        let entry_pool = reader;

        Ok(NameIndex {
            format: header.format,
            compile_unit_count: header.compile_unit_count,
            local_type_unit_count: header.local_type_unit_count,
            foreign_type_unit_count: header.foreign_type_unit_count,
            bucket_count: header.bucket_count,
            name_count: header.name_count,
            compile_unit_list,
            local_type_unit_list,
            foreign_type_unit_list,
            bucket_data,
            hash_table_data,
            name_table_data,
            entry_offset_data,
            entry_pool,
            abbreviations,
        })
    }

    /// Return the number of compilation units in this index.
    pub fn compile_unit_count(&self) -> u32 {
        self.compile_unit_count
    }

    /// Get the `.debug_info` offset of a compilation unit.
    ///
    /// `index` must be less than [`Self::compile_unit_count`].
    ///
    /// Returns an error if `index` is invalid.
    pub fn compile_unit(&self, index: u32) -> Result<DebugInfoOffset<R::Offset>> {
        let mut reader = self.compile_unit_list.clone();
        reader.skip(R::Offset::from_u64(
            u64::from(index) * u64::from(self.format.word_size()),
        )?)?;
        reader.read_offset(self.format).map(DebugInfoOffset)
    }

    /// Return the `.debug_info` offset of the default compilation unit, if any.
    ///
    /// If there is only one compilation unit, then entries may omit the `DW_IDX_compile_unit`
    /// attribute.
    pub fn default_compile_unit(&self) -> Result<Option<DebugInfoOffset<R::Offset>>> {
        if self.compile_unit_count == 1 {
            self.compile_unit(0).map(Some)
        } else {
            Ok(None)
        }
    }

    /// Return the number of local type units in this index.
    pub fn local_type_unit_count(&self) -> u32 {
        self.local_type_unit_count
    }

    /// Get the `.debug_info` offset of a local type unit.
    ///
    /// `index` must be less than [`Self::local_type_unit_count`].
    ///
    /// Returns an error if `index` is invalid.
    pub fn local_type_unit(&self, index: u32) -> Result<DebugInfoOffset<R::Offset>> {
        let mut reader = self.local_type_unit_list.clone();
        reader.skip(R::Offset::from_u64(
            u64::from(index) * u64::from(self.format.word_size()),
        )?)?;
        reader.read_offset(self.format).map(DebugInfoOffset)
    }

    /// Return the number of foreign type units in this index.
    pub fn foreign_type_unit_count(&self) -> u32 {
        self.foreign_type_unit_count
    }

    /// Get the signature of a foreign type unit.
    ///
    /// `index` must be less than [`Self::foreign_type_unit_count`].
    ///
    /// Returns an error if `index` is invalid.
    pub fn foreign_type_unit(&self, index: u32) -> Result<DebugTypeSignature> {
        let mut reader = self.foreign_type_unit_list.clone();
        reader.skip(R::Offset::from_u64(u64::from(index) * 8)?)?;
        reader.read_u64().map(DebugTypeSignature)
    }

    /// Return the number of type units in this index, both local and foreign.
    pub fn type_unit_count(&self) -> u32 {
        self.local_type_unit_count + self.foreign_type_unit_count
    }

    /// Get a type unit reference.
    ///
    /// `index` must be less than [`Self::type_unit_count`], and normally is
    /// obtained from a `DW_IDX_type_unit` attribute.
    ///
    /// Returns an error if `index` is invalid.
    pub fn type_unit(&self, index: u32) -> Result<NameTypeUnit<R::Offset>> {
        if let Some(foreign_index) = index.checked_sub(self.local_type_unit_count) {
            self.foreign_type_unit(foreign_index)
                .map(NameTypeUnit::Foreign)
        } else {
            self.local_type_unit(index).map(NameTypeUnit::Local)
        }
    }

    /// Return true if the name index contains a hash table.
    pub fn has_hash_table(&self) -> bool {
        self.bucket_count != 0
    }

    /// Return the number of buckets in the hash table.
    pub fn bucket_count(&self) -> u32 {
        self.bucket_count
    }

    /// Iterate over the hash entries for a bucket in the hash table.
    ///
    /// This function is only for diagnostic uses. Usually [`Self::find_by_hash`] should be
    /// called instead.
    ///
    /// The given bucket index is 0 based, and must be less than [`Self::bucket_count`].
    ///
    /// Returns an error if there is no hash table or the bucket index is invalid.
    /// Returns `Ok(None)` if the bucket is empty.
    pub fn find_by_bucket(&self, bucket_index: u32) -> Result<Option<NameBucketIter<R>>> {
        NameBucketIter::new(self, bucket_index)
    }

    /// Iterate over the indexes of the names with the given hash value.
    ///
    /// The user must then check each name to see if it matches the desired name.
    ///
    /// Returns an error if there is no hash table.
    pub fn find_by_hash(&self, hash_value: u32) -> Result<NameHashIter<R>> {
        NameHashIter::new(self, hash_value)
    }

    /// Get the number of names in the name index.
    ///
    /// This is 1 greater than the maximum valid [`NameTableIndex`].
    pub fn name_count(&self) -> u32 {
        self.name_count
    }

    /// Iterate over the indexes of all names in the name table.
    pub fn names(&self) -> NameTableIter {
        NameTableIter::new(self)
    }

    /// Get the string table offset for the name at the given index.
    ///
    /// Returns an error if `index` is invalid.
    pub fn name_string_offset(&self, index: NameTableIndex) -> Result<DebugStrOffset<R::Offset>> {
        let mut reader = self.name_table_data.clone();
        reader.skip(R::Offset::from_u64(
            u64::from(index.0) * u64::from(self.format.word_size()),
        )?)?;
        reader.read_offset(self.format).map(DebugStrOffset)
    }

    /// Get the name at the given index using the provided `.debug_str` section.
    ///
    /// Returns an error if `index` is invalid, or the string table offset is invalid.
    pub fn name_string(&self, index: NameTableIndex, debug_str: &DebugStr<R>) -> Result<R> {
        let offset = self.name_string_offset(index)?;
        debug_str.get_str(offset)
    }

    /// Iterate over the series of entries for the given name table index.
    ///
    /// Each name in the name table has a corresponding series of entries
    /// with that name in the entry pool.
    ///
    /// Returns an error if `index` is invalid, or the entry pool offset is invalid.
    pub fn name_entries(&self, index: NameTableIndex) -> Result<NameEntryIter<'_, R>> {
        NameEntryIter::new(self, index)
    }

    /// Parse the entry at the given entry pool offset.
    ///
    /// This is useful for reading the entry referenced by a `DW_IDX_parent` attribute.
    pub fn name_entry(&self, offset: NameEntryOffset<R::Offset>) -> Result<NameEntry<R>> {
        let mut entries = self.entry_pool.clone();
        entries.skip(offset.0)?;
        NameEntry::parse(&mut entries, offset, &self.abbreviations)?
            .ok_or(Error::NoEntryAtGivenOffset(offset.0.into_u64()))
    }

    /// Get the abbreviation table for name entries in this name index.
    pub fn abbreviations(&self) -> &NameAbbreviations {
        &self.abbreviations
    }
}

/// An iterator over the indexes of all names in a name index.
#[derive(Debug)]
pub struct NameTableIter {
    name_table_index: NameTableIndex,
    name_count: u32,
}

impl NameTableIter {
    fn new<R: Reader>(name_index: &NameIndex<R>) -> Self {
        NameTableIter {
            name_table_index: NameTableIndex(0),
            name_count: name_index.name_count,
        }
    }
}

impl Iterator for NameTableIter {
    type Item = NameTableIndex;

    fn next(&mut self) -> Option<Self::Item> {
        let name_table_index = self.name_table_index;
        if name_table_index.0 >= self.name_count {
            return None;
        }
        self.name_table_index.0 += 1;
        Some(name_table_index)
    }
}

/// An iterator over the hash entries for a bucket in a name index hash table.
#[derive(Debug)]
pub struct NameBucketIter<R: Reader> {
    reader: R,
    name_table_index: NameTableIndex,
    name_count: u32,
    bucket_index: u32,
    bucket_count: u32,
}

impl<R: Reader> NameBucketIter<R> {
    fn new(name_index: &NameIndex<R>, bucket_index: u32) -> Result<Option<Self>> {
        let mut bucket_reader = name_index.bucket_data.clone();
        bucket_reader.skip(R::Offset::from_u64(u64::from(bucket_index) * 4)?)?;
        let start = bucket_reader.read_u32()?;
        if start == 0 {
            return Ok(None);
        }
        let name_table_index = NameTableIndex(start - 1);

        let mut reader = name_index.hash_table_data.clone();
        reader.skip(R::Offset::from_u64(u64::from(name_table_index.0) * 4)?)?;

        Ok(Some(NameBucketIter {
            reader,
            name_table_index,
            name_count: name_index.name_count,
            bucket_index,
            bucket_count: name_index.bucket_count,
        }))
    }

    /// Advance the iterator and return the next name table index and hash.
    pub fn next(&mut self) -> Result<Option<(NameTableIndex, u32)>> {
        let name_table_index = self.name_table_index;
        if name_table_index.0 >= self.name_count {
            return Ok(None);
        }
        let hash = self.reader.read_u32()?;
        self.name_table_index.0 += 1;
        if hash % self.bucket_count != self.bucket_index {
            return Ok(None);
        }
        Ok(Some((name_table_index, hash)))
    }
}

#[cfg(feature = "fallible-iterator")]
impl<R: Reader> fallible_iterator::FallibleIterator for NameBucketIter<R> {
    type Item = (NameTableIndex, u32);
    type Error = Error;

    fn next(&mut self) -> ::core::result::Result<Option<Self::Item>, Self::Error> {
        NameBucketIter::next(self)
    }
}

impl<R: Reader> Iterator for NameBucketIter<R> {
    type Item = Result<(NameTableIndex, u32)>;

    fn next(&mut self) -> Option<Self::Item> {
        NameBucketIter::next(self).transpose()
    }
}

/// An iterator over the indexes of the names in a name index hash table that match a hash
/// value.
#[derive(Debug)]
pub struct NameHashIter<R: Reader> {
    bucket_iter: Option<NameBucketIter<R>>,
    hash: u32,
}

impl<R: Reader> NameHashIter<R> {
    fn new(name_index: &NameIndex<R>, hash: u32) -> Result<Self> {
        let bucket_index = if name_index.bucket_count == 0 {
            0
        } else {
            hash % name_index.bucket_count
        };
        let bucket_iter = NameBucketIter::new(name_index, bucket_index)?;

        Ok(NameHashIter { bucket_iter, hash })
    }

    /// Advance the iterator and return the next name table index.
    pub fn next(&mut self) -> Result<Option<NameTableIndex>> {
        let Some(bucket_iter) = &mut self.bucket_iter else {
            return Ok(None);
        };
        while let Some((name_table_index, hash)) = bucket_iter.next()? {
            if hash == self.hash {
                return Ok(Some(name_table_index));
            }
        }
        Ok(None)
    }
}

#[cfg(feature = "fallible-iterator")]
impl<R: Reader> fallible_iterator::FallibleIterator for NameHashIter<R> {
    type Item = NameTableIndex;
    type Error = Error;

    fn next(&mut self) -> ::core::result::Result<Option<Self::Item>, Self::Error> {
        NameHashIter::next(self)
    }
}

impl<R: Reader> Iterator for NameHashIter<R> {
    type Item = Result<NameTableIndex>;

    fn next(&mut self) -> Option<Self::Item> {
        NameHashIter::next(self).transpose()
    }
}

/// An iterator for a series of name entries in a name index entry pool.
///
/// Each name in a name index corresponds to a series of entries
/// with that name.
#[derive(Debug)]
pub struct NameEntryIter<'a, R: Reader> {
    entries: R,
    end_offset: R::Offset,
    abbreviations: &'a NameAbbreviations,
}

impl<'a, R: Reader> NameEntryIter<'a, R> {
    fn new(name_index: &'a NameIndex<R>, index: NameTableIndex) -> Result<Self> {
        let mut offsets = name_index.entry_offset_data.clone();
        offsets.skip(R::Offset::from_u64(
            u64::from(index.0) * u64::from(name_index.format.word_size()),
        )?)?;
        let offset = offsets
            .read_offset(name_index.format)
            .map(NameEntryOffset)?;

        let mut entries = name_index.entry_pool.clone();
        let end_offset = entries.len();
        entries.skip(offset.0)?;
        Ok(NameEntryIter {
            entries,
            end_offset,
            abbreviations: &name_index.abbreviations,
        })
    }

    /// Advance the iterator and return the next name entry.
    pub fn next(&mut self) -> Result<Option<NameEntry<R>>> {
        if self.entries.is_empty() {
            return Ok(None);
        }

        let offset = NameEntryOffset(self.end_offset - self.entries.len());
        match NameEntry::parse(&mut self.entries, offset, self.abbreviations) {
            Ok(Some(entry)) => Ok(Some(entry)),
            Ok(None) => {
                // Series end.
                self.entries.empty();
                Ok(None)
            }
            Err(e) => {
                // On error, prevent further iteration
                self.entries.empty();
                Err(e)
            }
        }
    }
}

#[cfg(feature = "fallible-iterator")]
impl<'a, R: Reader> fallible_iterator::FallibleIterator for NameEntryIter<'a, R> {
    type Item = NameEntry<R>;
    type Error = Error;

    fn next(&mut self) -> ::core::result::Result<Option<Self::Item>, Self::Error> {
        NameEntryIter::next(self)
    }
}

impl<'a, R: Reader> Iterator for NameEntryIter<'a, R> {
    type Item = Result<NameEntry<R>>;

    fn next(&mut self) -> Option<Self::Item> {
        NameEntryIter::next(self).transpose()
    }
}

/// An offset into the entry pool of a name index.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Ord, PartialOrd, Hash)]
pub struct NameEntryOffset<T = usize>(pub T);

/// A parsed entry from the `.debug_names` section.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct NameEntry<R: Reader> {
    /// The offset of the entry in the entries pool.
    pub offset: NameEntryOffset<R::Offset>,

    /// The abbreviation code for this entry.
    pub abbrev_code: u64,

    /// The DIE tag for this entry.
    pub tag: constants::DwTag,

    /// The attributes for this entry.
    pub attrs: Vec<NameAttribute<R>>,
}

impl<R: Reader> NameEntry<R> {
    /// Get the value of the `DW_IDX_compile_unit` attribute, if any.
    ///
    /// If neither `DW_IDX_compile_unit` nor `DW_IDX_type_unit` exist then you should use
    /// [`NameIndex::default_compile_unit`].
    ///
    /// If both `DW_IDX_compile_unit` and `DW_IDX_type_unit` exist then this value is for
    /// a skeleton CU that may be used to locate a split DWARF object file containing
    /// the type unit.
    pub fn compile_unit(&self, names: &NameIndex<R>) -> Result<Option<DebugInfoOffset<R::Offset>>> {
        for attr in &self.attrs {
            if attr.name == constants::DW_IDX_compile_unit {
                return attr.compile_unit(names).map(Some);
            }
        }
        Ok(None)
    }

    /// Get the value of the `DW_IDX_type_unit` attribute, if any.
    pub fn type_unit(&self, names: &NameIndex<R>) -> Result<Option<NameTypeUnit<R::Offset>>> {
        for attr in &self.attrs {
            if attr.name == constants::DW_IDX_type_unit {
                return attr.type_unit(names).map(Some);
            }
        }
        Ok(None)
    }

    /// Get the value of the `DW_IDX_die_offset` attribute, if any.
    ///
    /// This is the offset of the DIE within the compile unit or type unit.
    pub fn die_offset(&self) -> Result<Option<UnitOffset<R::Offset>>> {
        for attr in &self.attrs {
            if attr.name == constants::DW_IDX_die_offset {
                return attr.die_offset().map(Some);
            }
        }
        Ok(None)
    }

    /// Get the value of the `DW_IDX_parent` attribute, if any.
    ///
    /// Returns `Ok(Some(Some(offset)))` if the DIE parent is indexed.
    /// Returns `Ok(Some(None))` if the DIE parent is not indexed.
    /// Returns `Ok(None)` if it is unknown whether the DIE parent is indexed
    /// because the producer did not generate a `DW_IDX_parent` attribute.
    pub fn parent(&self) -> Result<Option<Option<NameEntryOffset<R::Offset>>>> {
        for attr in &self.attrs {
            if attr.name == constants::DW_IDX_parent {
                return attr.parent().map(Some);
            }
        }
        Ok(None)
    }

    /// Get the value of the `DW_IDX_type_hash` attribute, if any.
    pub fn type_hash(&self) -> Result<Option<u64>> {
        for attr in &self.attrs {
            if attr.name == constants::DW_IDX_type_hash {
                return attr.type_hash().map(Some);
            }
        }
        Ok(None)
    }

    /// Parse a single entry from the entry pool.
    fn parse(
        entry_reader: &mut R,
        offset: NameEntryOffset<R::Offset>,
        abbreviations: &NameAbbreviations,
    ) -> Result<Option<NameEntry<R>>> {
        let abbrev_code = entry_reader.read_uleb128()?;
        if abbrev_code == 0 {
            return Ok(None);
        }
        let Some(abbrev) = abbreviations.get(abbrev_code) else {
            return Err(Error::InvalidAbbreviationCode(abbrev_code));
        };
        let tag = abbrev.tag();
        let specs = abbrev.attributes();
        let mut attrs = Vec::with_capacity(specs.len());
        for spec in specs {
            let name = spec.name();
            let form = spec.form();
            let value = read_debug_names_form_value(entry_reader, form)?;
            attrs.push(NameAttribute { name, form, value });
        }

        Ok(Some(NameEntry {
            offset,
            abbrev_code,
            tag,
            attrs,
        }))
    }
}

/// A parsed attribute for a [`NameEntry`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct NameAttribute<R: Reader> {
    name: constants::DwIdx,
    form: constants::DwForm,
    value: NameAttributeValue<R>,
}

impl<R: Reader> NameAttribute<R> {
    /// Get the attribute name.
    pub fn name(&self) -> constants::DwIdx {
        self.name
    }

    /// Get the attribute form.
    pub fn form(&self) -> constants::DwForm {
        self.form
    }

    /// Get the attribute value.
    ///
    /// Interpretation of this value depends on the name and form.
    pub fn value(&self) -> &NameAttributeValue<R> {
        &self.value
    }

    /// Get the value of a `DW_IDX_compile_unit` attribute.
    pub fn compile_unit(&self, names: &NameIndex<R>) -> Result<DebugInfoOffset<R::Offset>> {
        match self.value {
            NameAttributeValue::Unsigned(val) => {
                let index =
                    u32::try_from(val).map_err(|_| Error::InvalidNameAttributeIndex(val))?;
                names.compile_unit(index)
            }
            _ => Err(Error::UnsupportedAttributeForm(self.form)),
        }
    }

    /// Get the value of a `DW_IDX_type_unit` attribute.
    pub fn type_unit(&self, names: &NameIndex<R>) -> Result<NameTypeUnit<R::Offset>> {
        match self.value {
            NameAttributeValue::Unsigned(val) => {
                let index =
                    u32::try_from(val).map_err(|_| Error::InvalidNameAttributeIndex(val))?;
                names.type_unit(index)
            }
            _ => Err(Error::UnsupportedAttributeForm(self.form)),
        }
    }

    /// Get the value of a `DW_IDX_die_offset` attribute.
    pub fn die_offset(&self) -> Result<UnitOffset<R::Offset>> {
        match self.value {
            NameAttributeValue::Offset(val) => Ok(UnitOffset(val)),
            _ => Err(Error::UnsupportedAttributeForm(self.form)),
        }
    }

    /// Get the value of a `DW_IDX_parent` attribute.
    ///
    /// Returns `Ok(Some(offset))` if the DIE parent is indexed.
    /// Returns `Ok(None)` if the DIE parent is not indexed.
    pub fn parent(&self) -> Result<Option<NameEntryOffset<R::Offset>>> {
        match self.value {
            NameAttributeValue::Offset(val) => Ok(Some(NameEntryOffset(val))),
            NameAttributeValue::Flag(true) => Ok(None),
            _ => Err(Error::UnsupportedAttributeForm(self.form)),
        }
    }

    /// Get the value of a `DW_IDX_type_hash` attribute.
    pub fn type_hash(&self) -> Result<u64> {
        match self.value {
            NameAttributeValue::Unsigned(val) => Ok(val),
            _ => Err(Error::UnsupportedAttributeForm(self.form)),
        }
    }
}

/// A parsed attribute value for a [`NameEntry`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NameAttributeValue<R: Reader> {
    /// An unsigned integer.
    ///
    /// This can be from the following forms:
    /// `DW_FORM_data1`, `DW_FORM_data2`, `DW_FORM_data4`, `DW_FORM_data8`, `DW_FORM_udata`
    Unsigned(u64),
    /// An offset within a DWARF section or part thereof.
    ///
    /// This can be from the following forms:
    /// `DW_FORM_ref1`, `DW_FORM_ref2`, `DW_FORM_ref4`, `DW_FORM_ref8`, `DW_FORM_ref_udata`
    Offset(R::Offset),
    /// A boolean flag.
    ///
    /// This can be from the following forms:
    /// `DW_FORM_flag`, `DW_FORM_flag_present`
    Flag(bool),
}

/// Read an attribute value.
///
/// This handles the subset of DWARF forms used in `.debug_names` entry pools
/// (`DW_IDX_*` attributes).
fn read_debug_names_form_value<R: Reader>(
    input: &mut R,
    form: constants::DwForm,
) -> Result<NameAttributeValue<R>> {
    Ok(match form {
        constants::DW_FORM_flag => {
            let present = input.read_u8()?;
            NameAttributeValue::Flag(present != 0)
        }
        constants::DW_FORM_flag_present => NameAttributeValue::Flag(true),
        constants::DW_FORM_data1 => {
            let data = input.read_u8()?;
            NameAttributeValue::Unsigned(u64::from(data))
        }
        constants::DW_FORM_data2 => {
            let data = input.read_u16()?;
            NameAttributeValue::Unsigned(u64::from(data))
        }
        constants::DW_FORM_data4 => {
            let data = input.read_u32()?;
            NameAttributeValue::Unsigned(u64::from(data))
        }
        constants::DW_FORM_data8 => {
            let data = input.read_u64()?;
            NameAttributeValue::Unsigned(data)
        }
        constants::DW_FORM_udata => {
            let data = input.read_uleb128()?;
            NameAttributeValue::Unsigned(data)
        }
        constants::DW_FORM_ref1 => {
            let reference = input.read_u8().map(R::Offset::from_u8)?;
            NameAttributeValue::Offset(reference)
        }
        constants::DW_FORM_ref2 => {
            let reference = input.read_u16().map(R::Offset::from_u16)?;
            NameAttributeValue::Offset(reference)
        }
        constants::DW_FORM_ref4 => {
            let reference = input.read_u32().map(R::Offset::from_u32)?;
            NameAttributeValue::Offset(reference)
        }
        constants::DW_FORM_ref8 => {
            let reference = input.read_u64().and_then(R::Offset::from_u64)?;
            NameAttributeValue::Offset(reference)
        }
        constants::DW_FORM_ref_udata => {
            let reference = input.read_uleb128().and_then(R::Offset::from_u64)?;
            NameAttributeValue::Offset(reference)
        }
        form => return Err(Error::UnknownForm(form)),
    })
}

/// A table of name entry abbreviations.
#[derive(Debug, Default, Clone)]
pub struct NameAbbreviations {
    /// The abbreviations in this table.
    abbreviations: Vec<NameAbbreviation>,
}

impl NameAbbreviations {
    /// Get an abbreviation by its code.
    pub fn get(&self, code: u64) -> Option<&NameAbbreviation> {
        self.abbreviations.iter().find(|abbrev| abbrev.code == code)
    }

    /// Get all abbreviations.
    pub fn abbreviations(&self) -> &[NameAbbreviation] {
        &self.abbreviations
    }

    /// Parse the abbreviation table from a reader.
    fn parse<R: Reader>(mut reader: R) -> Result<NameAbbreviations> {
        let mut abbreviations = Vec::new();

        // Allow missing null terminator.
        while !reader.is_empty() {
            let code = reader.read_uleb128()?;
            if code == 0 {
                break; // End of abbreviation table
            }

            let tag = reader.read_uleb128_u16()?;
            if tag == 0 {
                return Err(Error::AbbreviationTagZero);
            }
            let tag = constants::DwTag(tag);

            let mut attributes = Vec::new();
            loop {
                let name = reader.read_uleb128_u16()?;
                let form = reader.read_uleb128_u16()?;
                match (name, form) {
                    (0, 0) => break,
                    (0, _) => return Err(Error::AttributeNameZero),
                    (_, 0) => return Err(Error::AttributeFormZero),
                    (_, _) => {}
                }
                attributes.push(NameAbbreviationAttribute {
                    name: constants::DwIdx(name),
                    form: constants::DwForm(form),
                });
            }

            abbreviations.push(NameAbbreviation {
                code,
                tag,
                attributes,
            });
        }

        Ok(NameAbbreviations { abbreviations })
    }
}

/// A name abbreviation entry defines how name entries are encoded.
#[derive(Debug, Clone)]
pub struct NameAbbreviation {
    /// The abbreviation code.
    code: u64,
    /// The DIE tag.
    tag: constants::DwTag,
    /// The list of attribute specifications.
    attributes: Vec<NameAbbreviationAttribute>,
}

impl NameAbbreviation {
    /// Get the abbreviation code.
    pub fn code(&self) -> u64 {
        self.code
    }

    /// Get the DIE tag.
    pub fn tag(&self) -> constants::DwTag {
        self.tag
    }

    /// Get the attribute specifications.
    pub fn attributes(&self) -> &[NameAbbreviationAttribute] {
        &self.attributes
    }
}

/// An attribute specification in a name abbreviation.
#[derive(Debug, Clone)]
pub struct NameAbbreviationAttribute {
    /// The attribute name (index type).
    name: constants::DwIdx,
    /// The attribute form.
    form: constants::DwForm,
}

impl NameAbbreviationAttribute {
    /// Get the attribute name (index type).
    pub fn name(&self) -> constants::DwIdx {
        self.name
    }

    /// Get the attribute form.
    pub fn form(&self) -> constants::DwForm {
        self.form
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::constants::*;
    use crate::endianity::LittleEndian;
    use crate::test_util::GimliSectionMethods;
    use test_assembler::{Endian, Label, LabelMaker, Section};

    /// Debug names section builder methods for testing
    pub trait DebugNamesSectionMethods {
        fn debug_names_header(
            self,
            header: &mut NameIndexHeader<EndianSlice<'_, LittleEndian>>,
        ) -> Self;
        fn debug_names_abbrev(self, code: u64, tag: DwTag, idx_attrs: &[(DwIdx, DwForm)]) -> Self;
        fn debug_names_abbrev_null(self) -> Self;
        fn debug_names_entry_null(self) -> Self;
        fn offset(self, offset: usize, format: Format) -> Self;
        fn offset_label(self, offset: &Label, format: Format) -> Self;
    }

    impl DebugNamesSectionMethods for Section {
        fn debug_names_header(
            self,
            header: &mut NameIndexHeader<EndianSlice<'_, LittleEndian>>,
        ) -> Self {
            let length = Label::new();
            let start = Label::new();
            let end = Label::new();

            let section = match header.format {
                Format::Dwarf32 => self.D32(&length),
                Format::Dwarf64 => self.D32(0xffffffff).D64(&length),
            };
            let section = section
                .mark(&start)
                .D16(header.version)
                .D16(0) // Padding
                .D32(header.compile_unit_count)
                .D32(header.local_type_unit_count)
                .D32(header.foreign_type_unit_count)
                .D32(header.bucket_count)
                .D32(header.name_count)
                .D32(header.abbrev_table_size);
            let section = if let Some(augmentation_string) = header.augmentation_string {
                section
                    .D32(augmentation_string.len() as u32)
                    .append_bytes(augmentation_string.slice())
                    .append_repeated(0, (4 - (augmentation_string.len() & 3)) & 3)
            } else {
                section.D32(0)
            };
            let section = section.append_bytes(header.content.slice()).mark(&end);
            header.length = (&end - &start) as usize;
            length.set_const(header.length as u64);
            section
        }

        fn debug_names_abbrev(self, code: u64, tag: DwTag, idx_attrs: &[(DwIdx, DwForm)]) -> Self {
            let mut section = self.uleb(code).uleb(tag.0.into());
            for &(idx, form) in idx_attrs {
                section = section.uleb(idx.0.into()).uleb(form.0.into());
            }
            section.D8(0).D8(0) // Null terminator
        }

        fn debug_names_abbrev_null(self) -> Self {
            self.D8(0)
        }

        fn offset(self, offset: usize, format: Format) -> Self {
            match format {
                Format::Dwarf32 => self.D32(offset as u32),
                Format::Dwarf64 => self.D64(offset as u64),
            }
        }

        fn offset_label(self, offset: &Label, format: Format) -> Self {
            match format {
                Format::Dwarf32 => self.D32(offset),
                Format::Dwarf64 => self.D64(offset),
            }
        }

        fn debug_names_entry_null(self) -> Self {
            self.D8(0)
        }
    }

    #[test]
    fn test_debug_names() {
        for format in [Format::Dwarf32, Format::Dwarf64] {
            let offset_form = match format {
                Format::Dwarf32 => DW_FORM_ref4,
                Format::Dwarf64 => DW_FORM_ref8,
            };
            let abbrev_start = Label::new();
            let abbrev_end = Label::new();
            let entries_1 = Label::new();
            let entries_2 = Label::new();
            let entries_3 = Label::new();
            let entries_4 = Label::new();
            let entries_2_offset = Label::new();
            let entries_3_offset = Label::new();
            let entries_4_offset = Label::new();

            let debug_str = Section::new()
                .append_repeated(0, 0xd9)
                .append_bytes(b"char\0")
                .append_bytes(b"__ARRAY_SIZE_TYPE__\0")
                .append_bytes(b"main\0")
                .append_bytes(b"int\0")
                .get_contents()
                .unwrap();
            let debug_str = DebugStr::new(&debug_str, LittleEndian);

            let section = Section::with_endian(Endian::Little)
                // CU offsets
                .offset(0x101, format)
                // Local TU offsets
                .offset(0x201, format)
                .offset(0x202, format)
                // Foreign TU signatures
                .D64(0x1234_5678) // Hash for "int"
                // Hash buckets
                .D32(1) // Bucket 0: points to name 1 ("int")
                .D32(0) // Bucket 1: empty
                .D32(2) // Bucket 2: points to name 2 ("main")
                .D32(3) // Bucket 3: points to name 3 ("__ARRAY_SIZE_TYPE__")
                // Hash array
                .D32(0x0b88_8030) // Hash for "int"
                .D32(0x7c9a_7f6a) // Hash for "main"
                .D32(0x0cef_4cfb) // Hash for "__ARRAY_SIZE_TYPE__"
                .D32(0x7c95_2063) // Hash for "char"
                // String offsets
                .offset(0xf7, format) // String offset for "int"
                .offset(0xf2, format) // String offset for "main"
                .offset(0xde, format) // String offset for "__ARRAY_SIZE_TYPE__"
                .offset(0xd9, format) // String offset for "char"
                // Entry series offsets
                .offset(0x00, format)
                .offset_label(&entries_2_offset, format)
                .offset_label(&entries_3_offset, format)
                .offset_label(&entries_4_offset, format)
                .mark(&abbrev_start)
                .debug_names_abbrev(
                    1,
                    DW_TAG_base_type,
                    &[
                        (DW_IDX_type_unit, DW_FORM_udata),
                        (DW_IDX_die_offset, offset_form),
                        (DW_IDX_parent, DW_FORM_flag_present),
                    ],
                )
                .debug_names_abbrev(2, DW_TAG_subprogram, &[(DW_IDX_die_offset, offset_form)])
                .debug_names_abbrev(
                    3,
                    DW_TAG_subprogram,
                    &[
                        (DW_IDX_compile_unit, DW_FORM_data1),
                        (DW_IDX_die_offset, offset_form),
                        (DW_IDX_parent, offset_form),
                    ],
                )
                .debug_names_abbrev_null()
                .mark(&abbrev_end)
                // Entries series for name 1
                .mark(&entries_1)
                // Local TU
                .uleb(1)
                .uleb(0)
                .offset(0x10, format)
                // Foreign TU
                .uleb(1)
                .uleb(2)
                .offset(0x20, format)
                // Invalid TU
                .uleb(1)
                .uleb(4)
                .offset(0x30, format)
                .debug_names_entry_null()
                // Entries series for name 2
                .mark(&entries_2)
                // No CU
                .uleb(2)
                .offset(0x40, format)
                // CU
                .uleb(3)
                .D8(0)
                .offset(0x50, format)
                .offset((&entries_2 - &entries_1) as usize, format)
                .debug_names_entry_null()
                // Entries series for name 3
                .mark(&entries_3)
                .uleb(4) // Invalid
                .debug_names_entry_null()
                // Entries series for name 4
                .mark(&entries_4)
                .debug_names_entry_null();
            let abbrev_table_size = (&abbrev_end - &abbrev_start) as u32;
            entries_2_offset.set_const((&entries_2 - &entries_1) as u64);
            entries_3_offset.set_const((&entries_3 - &entries_1) as u64);
            entries_4_offset.set_const((&entries_4 - &entries_1) as u64);
            let content = section.get_contents().unwrap();

            let mut header = NameIndexHeader {
                offset: DebugNamesOffset(0),
                length: 0,
                format,
                version: 5,
                compile_unit_count: 1,
                local_type_unit_count: 2,
                foreign_type_unit_count: 1,
                bucket_count: 4,
                name_count: 4,
                abbrev_table_size,
                augmentation_string: Some(EndianSlice::new(b"LLVM0700", LittleEndian)),
                content: EndianSlice::new(&content, LittleEndian),
            };

            let buf = Section::with_endian(Endian::Little)
                .debug_names_header(&mut header)
                .get_contents()
                .unwrap();

            let debug_names = DebugNames::new(&buf, LittleEndian);
            let mut headers = debug_names.headers();
            let header = headers.next().unwrap().unwrap();
            assert_eq!(header.offset(), DebugNamesOffset(0));
            assert_eq!(header.version(), 5);
            assert_eq!(header.format(), format);
            assert_eq!(header.compile_unit_count(), 1);
            assert_eq!(header.local_type_unit_count(), 2);
            assert_eq!(header.foreign_type_unit_count(), 1);
            assert_eq!(header.bucket_count(), 4);
            assert_eq!(header.name_count(), 4);
            assert_eq!(header.abbrev_table_size(), abbrev_table_size);
            assert_eq!(header.augmentation_string().unwrap().slice(), b"LLVM0700");

            let name_index = header.index().unwrap();

            assert_eq!(name_index.compile_unit_count(), 1);
            assert_eq!(name_index.compile_unit(0), Ok(DebugInfoOffset(0x101)));
            assert!(name_index.compile_unit(1).is_err());
            assert_eq!(
                name_index.default_compile_unit(),
                Ok(Some(DebugInfoOffset(0x101)))
            );

            assert_eq!(name_index.local_type_unit_count(), 2);
            assert_eq!(name_index.local_type_unit(0), Ok(DebugInfoOffset(0x201)));
            assert_eq!(name_index.local_type_unit(1), Ok(DebugInfoOffset(0x202)));
            assert!(name_index.local_type_unit(2).is_err());

            assert_eq!(name_index.foreign_type_unit_count(), 1);
            assert_eq!(
                name_index.foreign_type_unit(0),
                Ok(DebugTypeSignature(0x1234_5678))
            );
            assert!(name_index.foreign_type_unit(1).is_err());

            assert_eq!(name_index.type_unit_count(), 3);
            assert_eq!(
                name_index.type_unit(0),
                Ok(NameTypeUnit::Local(DebugInfoOffset(0x201)))
            );
            assert_eq!(
                name_index.type_unit(1),
                Ok(NameTypeUnit::Local(DebugInfoOffset(0x202)))
            );
            assert_eq!(
                name_index.type_unit(2),
                Ok(NameTypeUnit::Foreign(DebugTypeSignature(0x1234_5678)))
            );
            assert!(name_index.type_unit(3).is_err());

            assert!(name_index.has_hash_table());
            assert_eq!(name_index.bucket_count(), 4);

            // Bucket 0 contains 1 name
            let mut names = name_index.find_by_bucket(0).unwrap().unwrap();
            assert_eq!(names.next(), Ok(Some((NameTableIndex(0), 0x0b88_8030))));
            assert_eq!(names.next(), Ok(None));

            // Bucket 1 is empty
            assert!(matches!(name_index.find_by_bucket(1), Ok(None)));

            // Bucket 3 contains 2 names
            let mut names = name_index.find_by_bucket(3).unwrap().unwrap();
            assert_eq!(names.next(), Ok(Some((NameTableIndex(2), 0x0cef_4cfb))));
            assert_eq!(names.next(), Ok(Some((NameTableIndex(3), 0x7c95_2063))));
            assert_eq!(names.next(), Ok(None));

            // Bucket 4 is invalid
            assert!(name_index.find_by_bucket(4).is_err());

            // Hash present
            for (i, hash) in [0x0b88_8030, 0x7c9a_7f6a, 0x0cef_4cfb, 0x7c95_2063]
                .into_iter()
                .enumerate()
            {
                let mut names = name_index.find_by_hash(hash).unwrap();
                assert_eq!(names.next(), Ok(Some(NameTableIndex(i as u32))));
                assert_eq!(names.next(), Ok(None));
            }

            // No bucket for hash
            let mut names = name_index.find_by_hash(0x0b88_8031).unwrap();
            assert!(matches!(names.next(), Ok(None)));

            // Bucket for hash, but hash not present
            let mut names = name_index.find_by_hash(0x0b88_8034).unwrap();
            assert!(matches!(names.next(), Ok(None)));

            assert_eq!(name_index.name_count(), 4);
            let mut names = name_index.names();

            // TU entries
            let name = names.next().unwrap();
            assert_eq!(
                name_index.name_string_offset(name),
                Ok(DebugStrOffset(0xf7))
            );
            assert_eq!(
                name_index.name_string(name, &debug_str).unwrap().slice(),
                b"int"
            );

            let mut entries = name_index.name_entries(name).unwrap();
            let entry = entries.next().unwrap().unwrap();
            assert_eq!(entry, name_index.name_entry(entry.offset).unwrap());
            assert_eq!(entry.tag, DW_TAG_base_type);
            assert_eq!(entry.compile_unit(&name_index), Ok(None));
            assert_eq!(
                entry.type_unit(&name_index),
                Ok(Some(NameTypeUnit::Local(DebugInfoOffset(0x201))))
            );
            assert_eq!(entry.die_offset(), Ok(Some(UnitOffset(0x10))));
            assert_eq!(entry.parent(), Ok(Some(None)));
            assert_eq!(entry.type_hash(), Ok(None));

            let entry = entries.next().unwrap().unwrap();
            assert_eq!(entry, name_index.name_entry(entry.offset).unwrap());
            assert_eq!(entry.tag, DW_TAG_base_type);
            assert_eq!(entry.compile_unit(&name_index), Ok(None));
            assert_eq!(
                entry.type_unit(&name_index),
                Ok(Some(NameTypeUnit::Foreign(DebugTypeSignature(0x1234_5678))))
            );
            assert_eq!(entry.die_offset(), Ok(Some(UnitOffset(0x20))));
            assert_eq!(entry.parent(), Ok(Some(None)));
            assert_eq!(entry.type_hash(), Ok(None));

            let entry = entries.next().unwrap().unwrap();
            assert_eq!(entry, name_index.name_entry(entry.offset).unwrap());
            assert_eq!(entry.tag, DW_TAG_base_type);
            assert_eq!(entry.compile_unit(&name_index), Ok(None));
            assert!(entry.type_unit(&name_index).is_err());
            assert_eq!(entry.die_offset(), Ok(Some(UnitOffset(0x30))));
            assert_eq!(entry.parent(), Ok(Some(None)));
            assert_eq!(entry.type_hash(), Ok(None));

            assert!(matches!(entries.next(), Ok(None)));

            // CU entries
            let name = names.next().unwrap();
            assert_eq!(
                name_index.name_string_offset(name),
                Ok(DebugStrOffset(0xf2))
            );
            assert_eq!(
                name_index.name_string(name, &debug_str).unwrap().slice(),
                b"main"
            );

            let mut entries = name_index.name_entries(name).unwrap();
            let entry = entries.next().unwrap().unwrap();
            assert_eq!(entry, name_index.name_entry(entry.offset).unwrap());
            assert_eq!(entry.tag, DW_TAG_subprogram);
            assert_eq!(entry.compile_unit(&name_index), Ok(None));
            assert_eq!(entry.type_unit(&name_index), Ok(None));
            assert_eq!(entry.die_offset(), Ok(Some(UnitOffset(0x40))));
            assert_eq!(entry.parent(), Ok(None));

            let entry = entries.next().unwrap().unwrap();
            assert_eq!(entry, name_index.name_entry(entry.offset).unwrap());
            assert_eq!(entry.tag, DW_TAG_subprogram);
            assert_eq!(
                entry.compile_unit(&name_index),
                Ok(Some(DebugInfoOffset(0x101)))
            );
            assert_eq!(entry.type_unit(&name_index), Ok(None));
            assert_eq!(entry.die_offset(), Ok(Some(UnitOffset(0x50))));
            assert_eq!(
                entry.parent(),
                Ok(Some(Some(NameEntryOffset(
                    (&entries_2 - &entries_1) as usize
                ))))
            );

            assert!(matches!(entries.next(), Ok(None)));

            // Invalid entry
            let name = names.next().unwrap();
            assert_eq!(
                name_index.name_string_offset(name),
                Ok(DebugStrOffset(0xde))
            );
            assert_eq!(
                name_index.name_string(name, &debug_str).unwrap().slice(),
                b"__ARRAY_SIZE_TYPE__"
            );

            let mut entries = name_index.name_entries(name).unwrap();
            assert!(matches!(
                entries.next(),
                Err(Error::InvalidAbbreviationCode(4))
            ));
            assert!(matches!(entries.next(), Ok(None)));

            // No entries
            let name = names.next().unwrap();
            assert_eq!(
                name_index.name_string_offset(name),
                Ok(DebugStrOffset(0xd9))
            );
            assert_eq!(
                name_index.name_string(name, &debug_str).unwrap().slice(),
                b"char"
            );

            let mut entries = name_index.name_entries(name).unwrap();
            assert!(matches!(entries.next(), Ok(None)));

            assert_eq!(names.next(), None);

            assert!(matches!(headers.next(), Ok(None)));
        }
    }

    // Tests:
    // - no hash table
    // - no augmentation string
    // - no default compile unit
    #[test]
    fn test_debug_names_no_hash_table() {
        for format in [Format::Dwarf32, Format::Dwarf64] {
            let word_size = usize::from(format.word_size());
            let abbrev_start = Label::new();
            let abbrev_end = Label::new();

            let debug_str = Section::new()
                .append_bytes(b"main\0")
                .get_contents()
                .unwrap();
            let debug_str = DebugStr::new(&debug_str, LittleEndian);

            let content = Section::with_endian(Endian::Little)
                // CU offsets
                .offset(0x101, format)
                .offset(0x102, format)
                // Local TU offset
                .offset(0x201, format)
                // Foreign TU signatures
                .D64(0x1234_5678) // Hash for "int"
                // String offsets
                .offset(0x0, format) // String offset for "main"
                // Entry offsets
                .offset(0x00, format)
                .mark(&abbrev_start)
                .debug_names_abbrev(
                    1,
                    DW_TAG_subprogram,
                    &[
                        (DW_IDX_die_offset, DW_FORM_ref4),
                        (DW_IDX_parent, DW_FORM_flag_present),
                    ],
                )
                .debug_names_abbrev_null()
                .mark(&abbrev_end)
                // Entry 0: abbrev=1, die_offset=0x3c
                .uleb(0x01)
                .offset(0x3c, format)
                .debug_names_entry_null()
                .get_contents()
                .unwrap();

            let mut header = NameIndexHeader {
                offset: DebugNamesOffset(0),
                length: 0,
                format,
                version: 5,
                compile_unit_count: 2,
                local_type_unit_count: 1,
                foreign_type_unit_count: 1,
                bucket_count: 0,
                name_count: 1,
                abbrev_table_size: (&abbrev_end - &abbrev_start) as u32,
                augmentation_string: None,
                content: EndianSlice::new(&content, LittleEndian),
            };

            let buf = Section::with_endian(Endian::Little)
                .debug_names_header(&mut header)
                .get_contents()
                .unwrap();

            let debug_names = DebugNames::new(&buf, LittleEndian);
            let mut headers = debug_names.headers();
            let header = headers.next().unwrap().unwrap();
            assert_eq!(header.offset(), DebugNamesOffset(0));
            assert_eq!(header.length(), 51 + 6 * word_size);
            assert_eq!(header.version(), 5);
            assert_eq!(header.format(), format);
            assert_eq!(header.compile_unit_count(), 2);
            assert_eq!(header.local_type_unit_count(), 1);
            assert_eq!(header.foreign_type_unit_count(), 1);
            assert_eq!(header.bucket_count(), 0);
            assert_eq!(header.name_count(), 1);
            assert_eq!(header.abbrev_table_size(), 9);
            assert!(header.augmentation_string().is_none());

            let name_index = header.index().unwrap();

            assert_eq!(name_index.compile_unit_count(), 2);
            assert_eq!(name_index.compile_unit(0), Ok(DebugInfoOffset(0x101)));
            assert_eq!(name_index.compile_unit(1), Ok(DebugInfoOffset(0x102)));
            assert_eq!(name_index.default_compile_unit(), Ok(None));

            assert_eq!(name_index.local_type_unit_count(), 1);
            assert_eq!(name_index.local_type_unit(0), Ok(DebugInfoOffset(0x201)));

            assert_eq!(name_index.foreign_type_unit_count(), 1);
            assert_eq!(
                name_index.foreign_type_unit(0),
                Ok(DebugTypeSignature(0x1234_5678))
            );

            assert_eq!(name_index.type_unit_count(), 2);
            assert_eq!(
                name_index.type_unit(0),
                Ok(NameTypeUnit::Local(DebugInfoOffset(0x201)))
            );
            assert_eq!(
                name_index.type_unit(1),
                Ok(NameTypeUnit::Foreign(DebugTypeSignature(0x1234_5678)))
            );

            // Hash table is not present
            assert!(!name_index.has_hash_table());
            assert_eq!(name_index.bucket_count(), 0);
            assert!(name_index.find_by_bucket(0).is_err());
            assert!(name_index.find_by_hash(0).is_err());

            // Names and entries are still accessible
            assert_eq!(name_index.name_count(), 1);
            let mut names = name_index.names();

            let name = names.next().unwrap();
            assert_eq!(name_index.name_string_offset(name), Ok(DebugStrOffset(0x0)));
            assert_eq!(
                name_index.name_string(name, &debug_str).unwrap().slice(),
                b"main"
            );
            let mut entries = name_index.name_entries(name).unwrap();
            let entry = entries.next().unwrap().unwrap();
            assert_eq!(entry, name_index.name_entry(entry.offset).unwrap());

            assert!(headers.next().unwrap().is_none());
        }
    }

    #[test]
    fn test_debug_names_invalid_version() {
        for format in [Format::Dwarf32, Format::Dwarf64] {
            let mut header = NameIndexHeader {
                offset: DebugNamesOffset(0),
                length: 0,
                format,
                version: 4,
                compile_unit_count: 0,
                local_type_unit_count: 0,
                foreign_type_unit_count: 0,
                bucket_count: 0,
                name_count: 0,
                abbrev_table_size: 0,
                augmentation_string: None,
                content: EndianSlice::new(&[], LittleEndian),
            };

            let buf = Section::with_endian(Endian::Little)
                .debug_names_header(&mut header)
                .get_contents()
                .unwrap();

            let debug_names = DebugNames::new(&buf, LittleEndian);
            let mut headers = debug_names.headers();
            let result = headers.next();
            assert_eq!(result.unwrap_err(), Error::UnknownVersion(4));
            assert!(headers.next().unwrap().is_none());
        }
    }

    #[test]
    fn test_debug_names_truncated() {
        for format in [Format::Dwarf32, Format::Dwarf64] {
            let mut header = NameIndexHeader {
                offset: DebugNamesOffset(0),
                length: 0,
                format,
                version: 5,
                compile_unit_count: 0,
                local_type_unit_count: 0,
                foreign_type_unit_count: 0,
                bucket_count: 0,
                name_count: 0,
                abbrev_table_size: 0,
                augmentation_string: None,
                content: EndianSlice::new(&[], LittleEndian),
            };

            let buf = Section::with_endian(Endian::Little)
                .debug_names_header(&mut header)
                .get_contents()
                .unwrap();

            let debug_names = DebugNames::new(&buf[..buf.len() - 1], LittleEndian);
            let mut headers = debug_names.headers();
            assert!(headers.next().is_err());
            assert!(headers.next().unwrap().is_none());
        }
    }

    #[test]
    fn test_debug_names_abbrev_table_empty() {
        let reader = EndianSlice::new(&[], LittleEndian);
        let abbrevs = NameAbbreviations::parse(reader).unwrap();
        assert!(abbrevs.abbreviations.is_empty());

        let reader = EndianSlice::new(&[0], LittleEndian);
        let abbrevs = NameAbbreviations::parse(reader).unwrap();
        assert!(abbrevs.abbreviations.is_empty());
    }

    #[test]
    fn test_debug_names_abbrev_table_invalid() {
        let input = Section::with_endian(Endian::Little)
            .uleb(1) // code
            .uleb(0) // invalid tag
            .uleb(2) // name
            .uleb(3) // form
            .D8(0) // name terminator
            .D8(0) // form terminator
            .D8(0) // code terminator
            .get_contents()
            .unwrap();
        let reader = EndianSlice::new(&input, LittleEndian);
        assert!(matches!(
            NameAbbreviations::parse(reader),
            Err(Error::AbbreviationTagZero)
        ));

        let input = Section::with_endian(Endian::Little)
            .uleb(1) // code
            .uleb(2) // tag
            .uleb(0) // invalid name
            .uleb(3) // form
            .D8(0) // name terminator
            .D8(0) // form terminator
            .D8(0) // code terminator
            .get_contents()
            .unwrap();
        let reader = EndianSlice::new(&input, LittleEndian);
        assert!(matches!(
            NameAbbreviations::parse(reader),
            Err(Error::AttributeNameZero)
        ));

        let input = Section::with_endian(Endian::Little)
            .uleb(1) // code
            .uleb(2) // tag
            .uleb(3) // name
            .uleb(0) // invalid form
            .D8(0) // name terminator
            .D8(0) // form terminator
            .D8(0) // code terminator
            .get_contents()
            .unwrap();
        let reader = EndianSlice::new(&input, LittleEndian);
        assert!(matches!(
            NameAbbreviations::parse(reader),
            Err(Error::AttributeFormZero)
        ));
    }

    #[test]
    fn test_debug_names_augmentation() {
        let augmentation = b"LLVM0700";
        let content = [0x12, 0x23];
        for i in 1..augmentation.len() {
            let augmentation_string = &augmentation[..i];
            let mut header = NameIndexHeader {
                offset: DebugNamesOffset(0),
                length: 0,
                format: Format::Dwarf32,
                version: 5,
                compile_unit_count: 0,
                local_type_unit_count: 0,
                foreign_type_unit_count: 0,
                bucket_count: 0,
                name_count: 0,
                abbrev_table_size: 0,
                augmentation_string: Some(EndianSlice::new(augmentation_string, LittleEndian)),
                content: EndianSlice::new(&content, LittleEndian),
            };

            let buf = Section::with_endian(Endian::Little)
                .debug_names_header(&mut header)
                .get_contents()
                .unwrap();

            let debug_names = DebugNames::new(&buf, LittleEndian);
            let mut headers = debug_names.headers();
            let header = headers.next().unwrap().unwrap();
            assert_eq!(
                header.augmentation_string().unwrap().slice(),
                augmentation_string
            );
            assert_eq!(header.content.slice(), content);
        }
    }
}
