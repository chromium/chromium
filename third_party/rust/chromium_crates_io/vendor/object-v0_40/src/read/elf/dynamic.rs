use core::convert::TryInto;
use core::fmt::Debug;
use core::slice;

use crate::elf;
use crate::endian;
use crate::pod::Pod;
use crate::read::{self, ReadError, ReadRef, Result, SectionIndex, StringTable};

use super::{FileHeader, SectionHeader, SectionTable};

/// A table of dynamic entries in an ELF file.
///
/// Also includes the string table used for the string values.
///
/// Returned by [`SectionTable::dynamic_table`].
#[derive(Debug, Clone, Copy)]
pub struct DynamicTable<'data, Elf: FileHeader, R = &'data [u8]>
where
    R: ReadRef<'data>,
{
    endian: Elf::Endian,
    dynamics: &'data [Elf::Dyn],
    strings: StringTable<'data, R>,
}

impl<'data, Elf: FileHeader, R: ReadRef<'data>> Default for DynamicTable<'data, Elf, R> {
    fn default() -> Self {
        DynamicTable {
            endian: Default::default(),
            dynamics: &[],
            strings: Default::default(),
        }
    }
}

impl<'data, Elf: FileHeader, R: ReadRef<'data>> DynamicTable<'data, Elf, R> {
    /// Parse the given dynamic table section.
    pub(crate) fn parse(
        endian: Elf::Endian,
        data: R,
        sections: &SectionTable<'data, Elf, R>,
        section: &Elf::SectionHeader,
    ) -> Result<DynamicTable<'data, Elf, R>> {
        debug_assert!(section.sh_type(endian) == elf::SHT_DYNAMIC);

        let dynamics = section
            .data_as_array(endian, data)
            .read_error("Invalid ELF dynamic table data")?;

        let link = SectionIndex(section.sh_link(endian) as usize);
        let strings = sections.strings(endian, data, link)?;

        Ok(DynamicTable {
            endian,
            dynamics,
            strings,
        })
    }

    /// Return the string table used for the dynamic entries that have string values.
    #[inline]
    pub fn strings(&self) -> &StringTable<'data, R> {
        &self.strings
    }

    /// Return the dynamic entry slice.
    ///
    /// This includes the terminating null entry and any following entries, which you will
    /// usually need to skip.
    #[inline]
    pub fn dynamics(&self) -> &'data [Elf::Dyn] {
        self.dynamics
    }

    /// Iterate over the dynamic table entries.
    ///
    /// Excludes the terminating null entry, and any following entries.
    #[inline]
    pub fn iter(&self) -> DynamicIterator<'data, Elf> {
        DynamicIterator::new(self.endian, self.dynamics)
    }

    /// Return true if there are no dynamic entries.
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.dynamics.is_empty()
    }

    /// The number of dynamic entries.
    ///
    /// This includes the terminating null entry and any following entries.
    #[inline]
    pub fn len(&self) -> usize {
        self.dynamics.len()
    }

    /// Return the string value for the given dynamic entry.
    ///
    /// Does not check for an appropriate tag.
    pub fn string(&self, d: Dynamic) -> Result<&'data [u8]> {
        d.string(&self.strings)
    }
}

impl<'a, 'data, Elf: FileHeader, R: ReadRef<'data>> IntoIterator
    for &'a DynamicTable<'data, Elf, R>
{
    type Item = Dynamic;
    type IntoIter = DynamicIterator<'data, Elf>;

    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

/// An iterator over the dynamic entries in an ELF file.
#[derive(Debug)]
pub struct DynamicIterator<'data, Elf: FileHeader> {
    endian: Elf::Endian,
    dynamics: slice::Iter<'data, Elf::Dyn>,
}

impl<'data, Elf> DynamicIterator<'data, Elf>
where
    Elf: FileHeader,
{
    fn new(endian: Elf::Endian, dynamics: &'data [Elf::Dyn]) -> Self {
        DynamicIterator {
            endian,
            dynamics: dynamics.iter(),
        }
    }
}

impl<'data, Elf: FileHeader> Iterator for DynamicIterator<'data, Elf> {
    type Item = Dynamic;

    fn next(&mut self) -> Option<Self::Item> {
        let d = self.dynamics.next()?;
        let tag = d.d_tag(self.endian);
        if tag == elf::DT_NULL {
            self.dynamics = [].iter();
            return None;
        }
        let val = d.d_val(self.endian).into();
        Some(Dynamic { tag, val })
    }
}

/// A parsed dynamic entry in an ELF file.
#[derive(Debug, Clone, Copy)]
pub struct Dynamic {
    /// The entry tag.
    ///
    /// One of the `DT_*` constants.
    pub tag: elf::DynamicTag,

    /// The entry value.
    ///
    /// This may be an address, a string table offset, or some other value.
    pub val: u64,
}

impl Dynamic {
    /// Return true if the value is an address.
    pub fn is_address(&self) -> bool {
        tag_is_address(self.tag)
    }

    /// Return true if the value is an offset in the dynamic string table.
    pub fn is_string(&self) -> bool {
        tag_is_string(self.tag)
    }

    /// Use the value to get a string in a string table.
    ///
    /// Does not check for an appropriate tag.
    pub fn string<'data, R: ReadRef<'data>>(
        &self,
        strings: &StringTable<'data, R>,
    ) -> Result<&'data [u8]> {
        self.val
            .try_into()
            .ok()
            .and_then(|val| strings.get(val).ok())
            .read_error("Invalid ELF dyn string")
    }
}

/// A trait for generic access to [`elf::Dyn32`] and [`elf::Dyn64`].
#[allow(missing_docs)]
pub trait Dyn: Debug + Pod + read::private::Sealed {
    type Word: Into<u64>;
    type Endian: endian::Endian;

    fn d_tag(&self, endian: Self::Endian) -> elf::DynamicTag;
    fn d_val(&self, endian: Self::Endian) -> Self::Word;

    /// Get the tag as a `DynamicTag`.
    ///
    /// This will sign-extend for 32-bit ELF.
    fn tag(&self, endian: Self::Endian) -> elf::DynamicTag {
        self.d_tag(endian)
    }

    /// Get the value as a `u64`.
    ///
    /// This will zero-extend for 32-bit ELF.
    fn val(&self, endian: Self::Endian) -> u64 {
        self.d_val(endian).into()
    }

    /// Try to convert the tag to an `i32`.
    fn tag32(&self, endian: Self::Endian) -> Option<i32> {
        self.d_tag(endian).0.try_into().ok()
    }

    /// Try to convert the value to a `u32`.
    fn val32(&self, endian: Self::Endian) -> Option<u32> {
        self.d_val(endian).into().try_into().ok()
    }

    /// Return true if the value is an offset in the dynamic string table.
    fn is_string(&self, endian: Self::Endian) -> bool {
        tag_is_string(self.tag(endian))
    }

    /// Use the value to get a string in a string table.
    ///
    /// Does not check for an appropriate tag.
    fn string<'data, R: ReadRef<'data>>(
        &self,
        endian: Self::Endian,
        strings: StringTable<'data, R>,
    ) -> Result<&'data [u8]> {
        self.val32(endian)
            .and_then(|val| strings.get(val).ok())
            .read_error("Invalid ELF dyn string")
    }

    /// Return true if the value is an address.
    fn is_address(&self, endian: Self::Endian) -> bool {
        tag_is_address(self.tag(endian))
    }
}

impl<Endian: endian::Endian> read::private::Sealed for elf::Dyn32<Endian> {}

impl<Endian: endian::Endian> Dyn for elf::Dyn32<Endian> {
    type Word = u32;
    type Endian = Endian;

    #[inline]
    fn d_tag(&self, endian: Self::Endian) -> elf::DynamicTag {
        self.d_tag.get_i64(endian)
    }

    #[inline]
    fn d_val(&self, endian: Self::Endian) -> Self::Word {
        self.d_val.get(endian)
    }
}

impl<Endian: endian::Endian> read::private::Sealed for elf::Dyn64<Endian> {}

impl<Endian: endian::Endian> Dyn for elf::Dyn64<Endian> {
    type Word = u64;
    type Endian = Endian;

    #[inline]
    fn d_tag(&self, endian: Self::Endian) -> elf::DynamicTag {
        self.d_tag.get(endian)
    }

    #[inline]
    fn d_val(&self, endian: Self::Endian) -> Self::Word {
        self.d_val.get(endian)
    }
}

fn tag_is_string(tag: elf::DynamicTag) -> bool {
    match tag {
        elf::DT_NEEDED
        | elf::DT_SONAME
        | elf::DT_RPATH
        | elf::DT_RUNPATH
        | elf::DT_AUXILIARY
        | elf::DT_FILTER => true,
        _ => false,
    }
}

fn tag_is_address(tag: elf::DynamicTag) -> bool {
    // TODO: check architecture specific values. This requires the e_machine value.
    match tag {
        elf::DT_PLTGOT
        | elf::DT_HASH
        | elf::DT_STRTAB
        | elf::DT_SYMTAB
        | elf::DT_RELA
        | elf::DT_INIT
        | elf::DT_FINI
        | elf::DT_REL
        | elf::DT_DEBUG
        | elf::DT_JMPREL
        | elf::DT_FINI_ARRAY
        | elf::DT_INIT_ARRAY
        | elf::DT_PREINIT_ARRAY
        | elf::DT_SYMTAB_SHNDX
        | elf::DT_VERDEF
        | elf::DT_VERNEED
        | elf::DT_VERSYM => true,
        _ => tag.is_address(),
    }
}
