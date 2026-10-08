//! the [GPOS] table
//!
//! [GPOS]: https://docs.microsoft.com/en-us/typography/opentype/spec/gpos

#[path = "./value_record.rs"]
mod value_record;

#[cfg(feature = "std")]
mod closure;

use crate::array::ComputedArray;

/// reexport stuff from layout that we use
pub use super::layout::{
    ClassDef, CoverageTable, Device, DeviceOrVariationIndex, FeatureList, FeatureVariations,
    Lookup, ScriptList,
};
use super::layout::{ExtensionLookup, LookupFlag, Subtables};
pub use value_record::ValueRecord;

#[cfg(test)]
#[path = "../tests/test_gpos.rs"]
mod spec_tests;

include!("../../generated/generated_gpos.rs");

/// A typed GPOS [LookupList](super::layout::LookupList) table
pub type PositionLookupList<'a> = super::layout::LookupList<'a, PositionLookup<'a>>;

/// A GPOS [SequenceContext](super::layout::SequenceContext)
pub type PositionSequenceContext<'a> = super::layout::SequenceContext<'a>;

/// A GPOS [ChainedSequenceContext](super::layout::ChainedSequenceContext)
pub type PositionChainContext<'a> = super::layout::ChainedSequenceContext<'a>;

impl<'a> AnchorTable<'a> {
    /// Attempt to resolve the `Device` or `VariationIndex` table for the
    /// x_coordinate, if present
    pub fn x_device(&self) -> Option<Result<DeviceOrVariationIndex<'a>, ReadError>> {
        match self {
            AnchorTable::Format3(inner) => inner.x_device(),
            _ => None,
        }
    }

    /// Attempt to resolve the `Device` or `VariationIndex` table for the
    /// y_coordinate, if present
    pub fn y_device(&self) -> Option<Result<DeviceOrVariationIndex<'a>, ReadError>> {
        match self {
            AnchorTable::Format3(inner) => inner.y_device(),
            _ => None,
        }
    }
}

impl<'a, T: FontRead<'a, Args = ()>> ExtensionLookup<'a, T> for ExtensionPosFormat1<'a, T> {
    fn extension(&self) -> Result<T, ReadError> {
        self.extension()
    }
}

type PosSubtables<'a, T> = Subtables<'a, T, ExtensionPosFormat1<'a, T>>;

/// The subtables from a GPOS lookup.
///
/// This type is a convenience that removes the need to dig into the
/// [`PositionLookup`] enum in order to access subtables, and it also abstracts
/// away the distinction between extension and non-extension lookups.
pub enum PositionSubtables<'a> {
    Single(PosSubtables<'a, SinglePos<'a>>),
    Pair(PosSubtables<'a, PairPos<'a>>),
    Cursive(PosSubtables<'a, CursivePosFormat1<'a>>),
    MarkToBase(PosSubtables<'a, MarkBasePosFormat1<'a>>),
    MarkToLig(PosSubtables<'a, MarkLigPosFormat1<'a>>),
    MarkToMark(PosSubtables<'a, MarkMarkPosFormat1<'a>>),
    Contextual(PosSubtables<'a, PositionSequenceContext<'a>>),
    ChainContextual(PosSubtables<'a, PositionChainContext<'a>>),
    /// An extension lookup did not have any subtables
    EmptyExtension,
}

impl<'a> PositionLookup<'a> {
    pub fn lookup_flag(&self) -> LookupFlag {
        self.of_unit_type().lookup_flag()
    }

    /// Different enumerations for GSUB and GPOS
    pub fn lookup_type(&self) -> u16 {
        self.of_unit_type().lookup_type()
    }

    pub fn mark_filtering_set(&self) -> Option<u16> {
        self.of_unit_type().mark_filtering_set()
    }

    /// Return the subtables for this lookup.
    ///
    /// This method handles both extension and non-extension lookups, and saves
    /// the caller needing to dig into the `PositionLookup` enum itself.
    pub fn subtables(&self) -> Result<PositionSubtables<'a>, ReadError> {
        let raw_lookup = self.of_unit_type();
        let offsets = raw_lookup.subtable_offsets();
        let data = raw_lookup.offset_data();
        match raw_lookup.lookup_type() {
            1 => Ok(PositionSubtables::Single(Subtables::new(offsets, data))),
            2 => Ok(PositionSubtables::Pair(Subtables::new(offsets, data))),
            3 => Ok(PositionSubtables::Cursive(Subtables::new(offsets, data))),
            4 => Ok(PositionSubtables::MarkToBase(Subtables::new(offsets, data))),
            5 => Ok(PositionSubtables::MarkToLig(Subtables::new(offsets, data))),
            6 => Ok(PositionSubtables::MarkToMark(Subtables::new(offsets, data))),
            7 => Ok(PositionSubtables::Contextual(Subtables::new(offsets, data))),
            8 => Ok(PositionSubtables::ChainContextual(Subtables::new(
                offsets, data,
            ))),
            9 => {
                // look through subtable offsets to try and find a lookup type.
                // this is robust in the case where the first subtable offset is
                // malformed, but a later one is okay.
                let Some(lookup_type) = offsets.iter().find_map(|off| {
                    off.get()
                        .resolve::<ExtensionPosFormat1<()>>(data)
                        .ok()
                        .map(|ext| ext.extension_lookup_type())
                }) else {
                    return Ok(PositionSubtables::EmptyExtension);
                };

                match lookup_type {
                    1 => Ok(PositionSubtables::Single(Subtables::new_ext(offsets, data))),
                    2 => Ok(PositionSubtables::Pair(Subtables::new_ext(offsets, data))),
                    3 => Ok(PositionSubtables::Cursive(Subtables::new_ext(
                        offsets, data,
                    ))),
                    4 => Ok(PositionSubtables::MarkToBase(Subtables::new_ext(
                        offsets, data,
                    ))),
                    5 => Ok(PositionSubtables::MarkToLig(Subtables::new_ext(
                        offsets, data,
                    ))),
                    6 => Ok(PositionSubtables::MarkToMark(Subtables::new_ext(
                        offsets, data,
                    ))),
                    7 => Ok(PositionSubtables::Contextual(Subtables::new_ext(
                        offsets, data,
                    ))),
                    8 => Ok(PositionSubtables::ChainContextual(Subtables::new_ext(
                        offsets, data,
                    ))),
                    other => Err(ReadError::InvalidFormat(other as _)),
                }
            }
            other => Err(ReadError::InvalidFormat(other as _)),
        }
    }
}

/// Extends the generated pair value records with a binary search.
impl<'a> PairSet<'a> {
    /// Returns the pair value record for `second_glyph`, using a binary search.
    ///
    /// Pair value records are ordered by second glyph, and are located rather
    /// than read, so this reads only the glyph id of each probed record.
    pub fn find_pair_value_record(&self, second_glyph: GlyphId16) -> Option<PairValueRecord<'a>> {
        let records = self.pair_value_records();
        let (mut lo, mut hi) = (0usize, records.len());
        while lo < hi {
            // deliberately not usize::midpoint, which widens to u128; these
            // values are bounded by a u16 count
            #[allow(clippy::manual_midpoint)]
            let mid = (lo + hi) / 2;
            let record = records.get(mid).ok()?;
            match record.second_glyph().cmp(&second_glyph) {
                core::cmp::Ordering::Less => lo = mid + 1,
                core::cmp::Ordering::Greater => hi = mid,
                core::cmp::Ordering::Equal => return Some(record),
            }
        }
        None
    }
}

/// The two-dimensional array of value records in a [`PairPosFormat2`], located
/// but not read.
///
/// The class counts, value formats and record stride are read once when this is
/// built, so addressing a record afterwards is pure arithmetic. Callers walking
/// many class pairs should build this once and reuse it rather than going
/// through [`PairPosFormat2::value_record_refs`], which rebuilds it every call.
#[derive(Copy, Clone)]
pub struct ClassValueRecords<'a> {
    data: FontData<'a>,
    start: u32,
    record_size: u32,
    format1_len: u32,
    class1_count: u16,
    class2_count: u16,
    format1: ValueFormat,
    format2: ValueFormat,
}

impl<'a> ClassValueRecords<'a> {
    /// Number of classes in classDef1, including class 0.
    #[inline]
    pub fn class1_count(&self) -> u16 {
        self.class1_count
    }

    /// Number of classes in classDef2, including class 0.
    #[inline]
    pub fn class2_count(&self) -> u16 {
        self.class2_count
    }

    /// Returns the pair of value records for the given classes.
    ///
    /// Returns `None` if either class is out of range.
    #[inline]
    pub fn get(&self, class1: u16, class2: u16) -> Option<[ValueRecord<'a>; 2]> {
        if class1 >= self.class1_count || class2 >= self.class2_count {
            return None;
        }
        // Compute an offset into the 2D array of positioning records. Every
        // term is bounded by a u16 count times a u16-derived stride, so this
        // cannot overflow usize on any supported target.
        let record_offset = self.start as usize
            + class1 as usize * self.record_size as usize * self.class2_count as usize
            + class2 as usize * self.record_size as usize;
        Some([
            ValueRecord::new(self.data, record_offset, self.format1),
            ValueRecord::new(
                self.data,
                record_offset + self.format1_len as usize,
                self.format2,
            ),
        ])
    }
}

impl<'a> PairPosFormat2<'a> {
    /// Returns the two-dimensional array of class value records, located but
    /// not read.
    pub fn class_value_records(&self) -> ClassValueRecords<'a> {
        let format1 = self.value_format1();
        let format2 = self.value_format2();
        let format1_len = format1.record_byte_len();
        ClassValueRecords {
            data: self.offset_data(),
            start: self.class1_records_byte_range().start as u32,
            record_size: (format1_len + format2.record_byte_len()) as u32,
            format1_len: format1_len as u32,
            class1_count: self.class1_count(),
            class2_count: self.class2_count(),
            format1,
            format2,
        }
    }

    /// Returns the pair of value records for the given classes, located but
    /// not read.
    ///
    /// Returns `None` if either class is out of range. When reading more than
    /// one class pair, prefer [`class_value_records`][Self::class_value_records],
    /// which reads the table's counts and formats only once.
    #[inline]
    pub fn value_record_refs(&self, class1: u16, class2: u16) -> Option<[ValueRecord<'a>; 2]> {
        self.class_value_records().get(class1, class2)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A value record is located, not read, so two ways of arriving at the
    /// same record must land on the same bytes.
    fn assert_same_record(a: ValueRecord, b: ValueRecord) {
        assert_eq!(a.format(), b.format());
        assert_eq!(a.bytes(), b.bytes());
        assert_eq!(a, b);
    }

    /// `class_value_records` reads the counts and formats once and addresses
    /// the 2D array arithmetically; walking `class1_records` reads them per
    /// row. Both must reach the same records.
    #[test]
    fn pair_pos2_hoisted_matches_nested() {
        let data = FontData::new(font_test_data::gpos::PAIRPOSFORMAT2);
        let table = PairPosFormat2::read(data).unwrap();
        let class1_count = table.class1_count();
        let class2_count = table.class2_count();
        assert!(class1_count > 0 && class2_count > 0);

        let hoisted = table.class_value_records();
        let rows = table.class1_records();
        for class1 in 0..class1_count {
            let row = rows.get(class1 as usize).unwrap();
            let cells = row.class2_records();
            for class2 in 0..class2_count {
                let cell = cells.get(class2 as usize).unwrap();
                let [a, b] = hoisted.get(class1, class2).unwrap();
                assert_same_record(a, cell.value_record1);
                assert_same_record(b, cell.value_record2);
            }
        }
        assert!(hoisted.get(class1_count, 0).is_none());
        assert!(hoisted.get(0, class2_count).is_none());
    }

    /// The binary search must find exactly what a linear scan finds.
    #[test]
    fn pair_set_find_matches_scan() {
        let data = FontData::new(font_test_data::gpos::PAIRPOSFORMAT1);
        let table = PairPosFormat1::read(data).unwrap();
        let mut seen = 0;
        for set_offset in table.pair_set_offsets() {
            let pair_set = set_offset
                .get()
                .resolve_with_args::<PairSet>(data, (table.value_format1(), table.value_format2()))
                .unwrap();
            let records = pair_set.pair_value_records();
            assert!(!records.is_empty());
            for i in 0..records.len() {
                let want = records.get(i).unwrap();
                let found = pair_set
                    .find_pair_value_record(want.second_glyph())
                    .unwrap();
                assert_eq!(found.second_glyph(), want.second_glyph());
                assert_same_record(found.value_record1, want.value_record1);
                assert_same_record(found.value_record2, want.value_record2);
                seen += 1;
            }
            assert!(pair_set
                .find_pair_value_record(GlyphId16::new(0xFFFF))
                .is_none());
        }
        assert!(seen > 0);
    }

    /// Device offsets in a value record resolve against the table containing
    /// the record, so a located record must carry that table's data.
    #[test]
    fn value_record_resolves_devices_against_its_table() {
        let data = FontData::new(font_test_data::gpos::VALUEFORMATTABLE);
        let table = SinglePosFormat1::read(data).unwrap();
        let record = table.value_record();
        assert!(record
            .format()
            .intersects(ValueFormat::ANY_DEVICE_OR_VARIDX));
        // the offsets are measured from the start of the table, not the record,
        // which begins six bytes in
        assert!(record.offset() > 0);
        assert!(record.x_placement_device().unwrap().is_ok());
        assert!(record.y_advance_device().unwrap().is_ok());
    }

    #[test]
    fn single_pos2_records_are_evenly_spaced() {
        let data = FontData::new(font_test_data::gpos::SINGLEPOSFORMAT2);
        let table = SinglePosFormat2::read(data).unwrap();
        let records = table.value_records();
        let stride = table.value_format().record_byte_len();
        assert!(records.len() > 1);
        let first = records.get(0).unwrap();
        for i in 0..records.len() {
            let rec = records.get(i).unwrap();
            assert_eq!(rec.offset(), first.offset() + i * stride);
            assert_eq!(rec.format(), table.value_format());
        }
        assert!(records.get(records.len()).is_err());
    }

    #[test]
    fn default_for_generics() {
        let ExtensionSubtable::Single(inner) = ExtensionSubtable::default() else {
            panic!("this is quite bad");
        };

        // this is invalid, but we the default impl for the extension offset
        // will be the first variant of the enum anyway
        assert_eq!(inner.extension_lookup_type(), 0);

        let SinglePos::Format1(_hmm) = inner.extension().unwrap_or_default() else {
            panic!("unexpected");
        };
    }
}
