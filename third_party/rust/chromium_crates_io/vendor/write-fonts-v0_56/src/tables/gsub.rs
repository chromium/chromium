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
mod extended;
#[cfg(test)]
mod spec_tests;

/// A GSUB lookup list table.
pub type SubstitutionLookupList = LookupList<SubstitutionLookup>;

/// A lookup list with the original 16-bit offsets.
pub type OriginalSubstitutionLookupList = SubstitutionLookupList;

/// A lookup list with 32-bit offsets.
pub type SubstitutionLookupList2 = super::layout::LookupList2<SubstitutionLookup>;

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
    /// Creates a GSUB table using the original 16-bit header offsets.
    pub fn new(
        script_list: ScriptList,
        feature_list: FeatureList,
        lookup_list: SubstitutionLookupList,
    ) -> Self {
        Self {
            script_list: Some(script_list).into(),
            feature_list: Some(feature_list).into(),
            lookup_list: Some(lookup_list).into(),
            ..Default::default()
        }
    }

    fn compute_version(&self) -> MajorMinor {
        if self.script_list2.is_some()
            || self.feature_list2.is_some()
            || self.lookup_list2.is_some()
        {
            MajorMinor::new(1, 2)
        } else if self.feature_variations.is_none() {
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
impl ReadArgs for SubstitutionLookup {
    type Args = ();
}

impl<'a> FontRead<'a> for SubstitutionLookup {
    fn read_with_args(data: FontData<'a>, _: ()) -> Result<Self, ReadError> {
        read_fonts::tables::gsub::SubstitutionLookup::read(data).map(|x| x.to_owned_table())
    }
}

impl ReadArgs for SubstitutionLookupList {
    type Args = ();
}

impl<'a> FontRead<'a> for SubstitutionLookupList {
    fn read_with_args(data: FontData<'a>, _: ()) -> Result<Self, ReadError> {
        read_fonts::tables::gsub::OriginalSubstitutionLookupList::read(data)
            .map(|x| x.to_owned_table())
    }
}

super::layout::lookup_type!(gsub, MultipleSubst, 2);
super::layout::lookup_type!(gsub, MultipleSubstFormat2, 2);
super::layout::original_lookup!(
    SubstitutionLookup,
    Multiple,
    MultipleSubstFormat1,
    MultipleSubst
);

super::layout::lookup_type!(gsub, AlternateSubst, 3);
super::layout::lookup_type!(gsub, AlternateSubstFormat2, 3);
super::layout::original_lookup!(
    SubstitutionLookup,
    Alternate,
    AlternateSubstFormat1,
    AlternateSubst
);

super::layout::lookup_type!(gsub, LigatureSubst, 4);
super::layout::lookup_type!(gsub, LigatureSubstFormat2, 4);
super::layout::original_lookup!(
    SubstitutionLookup,
    Ligature,
    LigatureSubstFormat1,
    LigatureSubst
);

super::layout::lookup_type!(gsub, ReverseChainSingleSubst, 8);
super::layout::lookup_type!(gsub, ReverseChainSingleSubstFormat2, 8);
super::layout::original_lookup!(
    SubstitutionLookup,
    Reverse,
    ReverseChainSingleSubstFormat1,
    ReverseChainSingleSubst
);

impl ReadArgs for SubstitutionLookupList2 {
    type Args = ();
}

impl<'a> FontRead<'a> for SubstitutionLookupList2 {
    fn read_with_args(data: FontData<'a>, _: ()) -> Result<Self, ReadError> {
        read_fonts::tables::gsub::SubstitutionLookupList2::read(data).map(|x| x.to_owned_table())
    }
}
