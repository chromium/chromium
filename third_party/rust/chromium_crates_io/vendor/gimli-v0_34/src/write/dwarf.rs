use alloc::vec::Vec;

use crate::common::Encoding;
use crate::write::{
    AbbreviationTable, LineProgram, LineString, LineStringTable, Result, Sections, StringTable,
    Unit, UnitTable, Writer,
};

/// Writable DWARF information for more than one unit.
#[derive(Debug, Default)]
pub struct Dwarf {
    /// A table of units. These are primarily stored in the `.debug_info` section,
    /// but they also contain information that is stored in other sections.
    pub units: UnitTable,

    /// Extra line number programs that are not associated with a unit.
    ///
    /// These should only be used when generating DWARF5 line-only debug
    /// information.
    pub line_programs: Vec<LineProgram>,

    /// A table of strings that will be stored in the `.debug_line_str` section.
    pub line_strings: LineStringTable,

    /// A table of strings that will be stored in the `.debug_str` section.
    pub strings: StringTable,
}

impl Dwarf {
    /// Create a new `Dwarf` instance.
    #[inline]
    pub fn new() -> Self {
        Self::default()
    }

    /// Write the DWARF information to the given sections.
    pub fn write<W: Writer>(&mut self, sections: &mut Sections<W>) -> Result<()> {
        self.units
            .write(sections, &mut self.line_strings, &mut self.strings)?;
        for line_program in &self.line_programs {
            line_program.write(
                &mut sections.debug_line,
                line_program.encoding(),
                &mut self.line_strings,
                &mut self.strings,
            )?;
        }
        self.line_strings.write(&mut sections.debug_line_str)?;
        self.strings.write(&mut sections.debug_str)?;
        Ok(())
    }

    /// Get a reference to the data for a line string.
    pub fn get_line_string<'a>(&'a self, string: &'a LineString) -> &'a [u8] {
        string.get(&self.strings, &self.line_strings)
    }
}

/// Writable DWARF information for a single unit.
#[derive(Debug)]
pub struct DwarfUnit {
    /// A unit. This is primarily stored in the `.debug_info` section,
    /// but also contains information that is stored in other sections.
    pub unit: Unit,

    /// A table of strings that will be stored in the `.debug_line_str` section.
    pub line_strings: LineStringTable,

    /// A table of strings that will be stored in the `.debug_str` section.
    pub strings: StringTable,
}

impl DwarfUnit {
    /// Create a new `DwarfUnit`.
    ///
    /// Note: you should set `self.unit.line_program` after creation.
    /// This cannot be done earlier because it may need to reference
    /// `self.line_strings`.
    pub fn new(encoding: Encoding) -> Self {
        let unit = Unit::new(encoding, LineProgram::none());
        DwarfUnit {
            unit,
            line_strings: LineStringTable::default(),
            strings: StringTable::default(),
        }
    }

    /// Write the DWARf information to the given sections.
    pub fn write<W: Writer>(&mut self, sections: &mut Sections<W>) -> Result<()> {
        let abbrev_offset = sections.debug_abbrev.offset();
        let mut abbrevs = AbbreviationTable::default();

        self.unit.write(
            sections,
            abbrev_offset,
            &mut abbrevs,
            &mut self.line_strings,
            &mut self.strings,
        )?;
        // None should exist because we didn't give out any UnitId.
        assert!(sections.debug_info_fixups.is_empty());
        assert!(sections.debug_loc_fixups.is_empty());
        assert!(sections.debug_loclists_fixups.is_empty());

        abbrevs.write(&mut sections.debug_abbrev)?;
        self.line_strings.write(&mut sections.debug_line_str)?;
        self.strings.write(&mut sections.debug_str)?;
        Ok(())
    }

    /// Get a reference to the data for a line string.
    pub fn get_line_string<'a>(&'a self, string: &'a LineString) -> &'a [u8] {
        string.get(&self.strings, &self.line_strings)
    }
}

#[cfg(feature = "read")]
pub(crate) mod convert {
    use super::*;
    use crate::common::LineEncoding;
    use crate::read::{self, Reader};
    use crate::write::{
        Address, ConvertLineProgram, ConvertResult, ConvertUnitSection, FilterUnitSection,
    };

    impl Dwarf {
        /// Create a `write::Dwarf` by converting a `read::Dwarf`.
        ///
        /// `convert_address` is a function to convert addresses read by
        /// `Reader::read_address` into the `Address` type. For executable files,
        /// it is sufficient to simply map the address to `Address::Constant`.
        ///
        /// Relocatable object files are more complicated because there are relocations
        /// associated with the address. To handle this, you can use a `Reader`
        /// implementation for which `Reader::read_address` stores the relocation
        /// information in a map and returns the map key instead of an address. Then
        /// `convert_address` can look up the mapping to produce an `Address`.
        ///
        /// Note that `convert_address` is also used for address and offset pairs in
        /// DWARF v2-v4 range lists and location lists. In order for the parser to
        /// correctly handle these, `Reader::read_address` must return the values 0 and -1
        /// unchanged.
        ///
        /// `convert_address` should not be used for complex address transformations, as it
        /// will not be called for address offsets (such as in `DW_AT_high_pc`, line programs,
        /// location lists, or range lists). If you need complex transformations, then you
        /// need to use [`Dwarf::convert`] to enable you to transform the address offsets too.
        ///
        /// ## Example
        ///
        /// Convert DWARF sections using `Dwarf::from`.
        ///
        /// ```rust,no_run
        /// # fn example() -> Result<(), gimli::write::ConvertError> {
        /// # let loader = |name| -> Result<gimli::EndianSlice<gimli::RunTimeEndian>, gimli::Error> { unimplemented!() };
        /// let read_dwarf = gimli::read::Dwarf::load(loader)?;
        /// let write_dwarf = gimli::write::Dwarf::from(
        ///     &read_dwarf,
        ///     &|address| Some(gimli::write::Address::Constant(address)),
        /// )?;
        /// # unreachable!()
        /// # }
        /// ```
        pub fn from<R: Reader<Offset = usize>>(
            from_dwarf: &read::Dwarf<R>,
            convert_address: &dyn Fn(u64) -> Option<Address>,
        ) -> ConvertResult<Dwarf> {
            let mut dwarf = Dwarf::default();
            let mut convert = dwarf.convert(from_dwarf)?;
            while let Some((mut unit, root_entry)) = convert.read_unit()? {
                unit.convert(root_entry, convert_address)?;
            }
            // TODO: convert the line programs that were not referenced by a unit.
            Ok(dwarf)
        }

        /// Create a converter for all units in the `.debug_info` section of the given
        /// DWARF object.
        ///
        /// ## Example
        ///
        /// Convert DWARF sections using `convert`.
        /// See [`ConvertUnit`](crate::write::ConvertUnit) for an example of the unit
        /// conversion.
        ///
        /// ```rust,no_run
        /// # fn example() -> Result<(), gimli::write::ConvertError> {
        /// # let loader = |name| -> Result<gimli::EndianSlice<gimli::RunTimeEndian>, gimli::Error> { unimplemented!() };
        /// let read_dwarf = gimli::read::Dwarf::load(loader)?;
        /// let mut write_dwarf = gimli::write::Dwarf::new();
        /// let mut convert = write_dwarf.convert(&read_dwarf)?;
        /// while let Some((mut unit, root_entry)) = convert.read_unit()? {
        ///     // Now you can convert the root DIE attributes, and other DIEs.
        /// }
        /// # unreachable!()
        /// # }
        /// ```
        pub fn convert<'a, R: Reader<Offset = usize>>(
            &'a mut self,
            dwarf: &'a read::Dwarf<R>,
        ) -> ConvertResult<ConvertUnitSection<'a, R>> {
            ConvertUnitSection::new(dwarf, self)
        }

        /// Create a converter for some of the DIEs in the `.debug_info` section of the
        /// given DWARF object.
        ///
        /// `filter` determines which DIEs are converted. This can be created using
        /// [`FilterUnitSection::new`].
        ///
        /// ## Example
        ///
        /// Convert a DWARF section using `convert_with_filter`.
        /// See [`ConvertUnit`](crate::write::ConvertUnit) for an example of the unit
        /// conversion.
        ///
        /// ```rust,no_run
        /// # fn example() -> Result<(), gimli::write::ConvertError> {
        /// # let loader = |name| -> Result<gimli::EndianSlice<gimli::RunTimeEndian>, gimli::Error> { unimplemented!() };
        /// # let need_entry = &|entry: &gimli::write::FilterUnitEntry<_>| -> Result<bool, gimli::write::ConvertError> { Ok(false) };
        /// let read_dwarf = gimli::read::Dwarf::load(loader)?;
        /// let mut filter = gimli::write::FilterUnitSection::new(&read_dwarf)?;
        /// while let Some(mut unit) = filter.read_unit()? {
        ///     let mut entry = unit.null_entry();
        ///     while unit.read_entry(&mut entry)? {
        ///         if need_entry(&entry)? {
        ///             unit.require_entry(entry.offset);
        ///         }
        ///     }
        /// }
        /// let mut write_dwarf = gimli::write::Dwarf::new();
        /// let mut convert = write_dwarf.convert_with_filter(filter)?;
        /// while let Some((mut unit, root_entry)) = convert.read_unit()? {
        ///     // Now you can convert the root DIE attributes, and other DIEs.
        /// }
        /// # unreachable!()
        /// # }
        /// ```
        pub fn convert_with_filter<'a, R: Reader<Offset = usize>>(
            &'a mut self,
            filter: FilterUnitSection<'a, R>,
        ) -> ConvertResult<ConvertUnitSection<'a, R>> {
            ConvertUnitSection::new_with_filter(self, filter)
        }

        /// Start a new conversion of a line number program.
        ///
        /// This is intended for line number programs that do not have an associated
        /// [`read::Unit`]. If the line number program has an associated [`read::Unit`]
        /// that you are converting, then you should use
        /// [`ConvertUnit::read_line_program`](crate::write::ConvertUnit::read_line_program)
        /// instead.
        ///
        /// `encoding` and `line_encoding` apply to the converted program, and
        /// may be different from the source program. If `None`, the encoding from
        /// the source program is used.
        ///
        /// See [`ConvertLineProgram`] for an example.
        pub fn read_line_program<'a, R: Reader<Offset = usize>>(
            &'a mut self,
            dwarf: &'a read::Dwarf<R>,
            program: read::IncompleteLineProgram<R>,
            encoding: Option<Encoding>,
            line_encoding: Option<LineEncoding>,
        ) -> ConvertResult<ConvertLineProgram<'a, R>> {
            ConvertLineProgram::new(
                dwarf,
                program,
                None,
                encoding,
                line_encoding,
                &mut self.line_strings,
                &mut self.strings,
            )
        }
    }
}
