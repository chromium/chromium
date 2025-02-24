//! the [GPOS] table
//!
//! [GPOS]: https://docs.microsoft.com/en-us/typography/opentype/spec/gpos

#[path = "./value_record.rs"]
mod value_record;

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

impl<'a, T: FontRead<'a>> ExtensionLookup<'a, T> for ExtensionPosFormat1<'a, T> {
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
                let first = offsets.first().ok_or(ReadError::OutOfBounds)?.get();
                let ext: ExtensionPosFormat1<()> = first.resolve(data)?;
                match ext.extension_lookup_type() {
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
