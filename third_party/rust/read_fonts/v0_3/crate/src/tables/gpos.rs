//! the [GPOS] table
//!
//! [GPOS]: https://docs.microsoft.com/en-us/typography/opentype/spec/gpos

#[path = "./value_record.rs"]
mod value_record;

use crate::array::ComputedArray;

/// reexport stuff from layout that we use
pub use super::layout::{
    ClassDef, CoverageTable, Device, FeatureList, FeatureVariations, Lookup, ScriptList,
};
pub use value_record::ValueRecord;

#[cfg(test)]
#[path = "../tests/gpos.rs"]
mod tests;

include!("../../generated/generated_gpos.rs");

/// A typed GPOS [LookupList](super::layout::LookupList) table
pub type PositionLookupList<'a> = super::layout::LookupList<'a, PositionLookup<'a>>;

/// A GPOS [SequenceContext](super::layout::SequenceContext)
pub type PositionSequenceContext<'a> = super::layout::SequenceContext<'a>;

/// A GPOS [ChainedSequenceContext](super::layout::ChainedSequenceContext)
pub type PositionChainContext<'a> = super::layout::ChainedSequenceContext<'a>;
