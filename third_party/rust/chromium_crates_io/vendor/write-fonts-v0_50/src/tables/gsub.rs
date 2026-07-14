//! the [GSUB] table
//!
//! [GSUB]: https://docs.microsoft.com/en-us/typography/opentype/spec/gsub

include!("../../generated/generated_gsub.rs");

use super::layout::{
    ChainedSequenceContext, CoverageTable, FeatureList, FeatureVariations, Lookup, LookupList,
    LookupSubtable, LookupType, ScriptList, SequenceContext,
};

pub mod builders;
#[cfg(test)]
mod spec_tests;

/// A GSUB lookup list table.
pub type SubstitutionLookupList = LookupList<SubstitutionLookup>;

super::layout::table_newtype!(
    SubstitutionSequenceContext,
    SequenceContext,
    read_fonts::tables::layout::SequenceContext<'a>
);

super::layout::table_newtype!(
    SubstitutionChainContext,
    ChainedSequenceContext,
    read_fonts::tables::layout::ChainedSequenceContext<'a>
);

impl Gsub {
    fn compute_version(&self) -> MajorMinor {
        if self.feature_variations.is_none() {
            MajorMinor::VERSION_1_0
        } else {
            MajorMinor::VERSION_1_1
        }
    }
}

super::layout::lookup_type!(gsub, SingleSubst, 1);
super::layout::lookup_type!(gsub, MultipleSubstFormat1, 2);
super::layout::lookup_type!(gsub, AlternateSubstFormat1, 3);
super::layout::lookup_type!(gsub, LigatureSubstFormat1, 4);
super::layout::lookup_type!(gsub, SubstitutionSequenceContext, 5);
super::layout::lookup_type!(gsub, SubstitutionChainContext, 6);
super::layout::lookup_type!(gsub, ExtensionSubtable, 7);
super::layout::lookup_type!(gsub, ReverseChainSingleSubstFormat1, 8);

impl<T: LookupSubtable + FontWrite> FontWrite for ExtensionSubstFormat1<T> {
    fn write_into(&self, writer: &mut TableWriter) {
        1u16.write_into(writer);
        T::TYPE.write_into(writer);
        self.extension.write_into(writer);
    }
}

// these can't have auto impls because the traits don't support generics
impl<'a> FontRead<'a> for SubstitutionLookup {
    fn read(data: FontData<'a>) -> Result<Self, ReadError> {
        read_fonts::tables::gsub::SubstitutionLookup::read(data).map(|x| x.to_owned_table())
    }
}

impl<'a> FontRead<'a> for SubstitutionLookupList {
    fn read(data: FontData<'a>) -> Result<Self, ReadError> {
        read_fonts::tables::gsub::SubstitutionLookupList::read(data).map(|x| x.to_owned_table())
    }
}
