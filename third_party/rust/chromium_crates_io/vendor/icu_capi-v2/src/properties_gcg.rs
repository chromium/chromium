// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use icu_properties::props;

    /// A mask that is capable of representing groups of `General_Category` values.
    #[diplomat::rust_link(icu::properties::props::GeneralCategoryGroup, Struct)]
    #[derive(Default)]
    pub struct GeneralCategoryGroup {
        pub mask: u32,
    }

    impl GeneralCategoryGroup {
        #[inline]
        pub(crate) fn into_props_group(self) -> props::GeneralCategoryGroup {
            self.mask.into()
        }

        #[diplomat::rust_link(icu::properties::props::GeneralCategoryGroup::contains, FnInStruct)]
        pub fn contains(
            self,
            val: crate::unstable::properties_enums::ffi::GeneralCategory,
        ) -> bool {
            self.into_props_group().contains(val.into())
        }
        #[diplomat::rust_link(icu::properties::props::GeneralCategoryGroup::complement, FnInStruct)]
        pub fn complement(self) -> Self {
            self.into_props_group().complement().into()
        }

        #[diplomat::rust_link(icu::properties::props::GeneralCategoryGroup::all, FnInStruct)]
        pub fn all() -> Self {
            props::GeneralCategoryGroup::all().into()
        }
        #[diplomat::rust_link(icu::properties::props::GeneralCategoryGroup::empty, FnInStruct)]
        pub fn empty() -> Self {
            props::GeneralCategoryGroup::empty().into()
        }
        #[diplomat::rust_link(icu::properties::props::GeneralCategoryGroup::union, FnInStruct)]
        #[diplomat::attr(any(c, cpp), rename = "union_")]
        pub fn union(self, other: Self) -> Self {
            self.into_props_group()
                .union(other.into_props_group())
                .into()
        }
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategoryGroup::intersection,
            FnInStruct
        )]
        pub fn intersection(self, other: Self) -> Self {
            self.into_props_group()
                .intersection(other.into_props_group())
                .into()
        }

        #[diplomat::rust_link(
            icu::properties::props::GeneralCategoryGroup::CasedLetter,
            AssociatedConstantInStruct
        )]
        pub fn cased_letter() -> Self {
            props::GeneralCategoryGroup::CasedLetter.into()
        }

        #[diplomat::rust_link(
            icu::properties::props::GeneralCategoryGroup::Letter,
            AssociatedConstantInStruct
        )]
        pub fn letter() -> Self {
            props::GeneralCategoryGroup::Letter.into()
        }

        #[diplomat::rust_link(
            icu::properties::props::GeneralCategoryGroup::Mark,
            AssociatedConstantInStruct
        )]
        pub fn mark() -> Self {
            props::GeneralCategoryGroup::Mark.into()
        }
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategoryGroup::Number,
            AssociatedConstantInStruct
        )]
        pub fn number() -> Self {
            props::GeneralCategoryGroup::Number.into()
        }
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategoryGroup::Other,
            AssociatedConstantInStruct
        )]
        pub fn separator() -> Self {
            props::GeneralCategoryGroup::Other.into()
        }
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategoryGroup::Letter,
            AssociatedConstantInStruct
        )]
        pub fn other() -> Self {
            props::GeneralCategoryGroup::Letter.into()
        }
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategoryGroup::Punctuation,
            AssociatedConstantInStruct
        )]
        pub fn punctuation() -> Self {
            props::GeneralCategoryGroup::Punctuation.into()
        }
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategoryGroup::Symbol,
            AssociatedConstantInStruct
        )]
        pub fn symbol() -> Self {
            props::GeneralCategoryGroup::Symbol.into()
        }
    }
}

impl From<icu_properties::props::GeneralCategoryGroup> for ffi::GeneralCategoryGroup {
    #[inline]
    fn from(other: icu_properties::props::GeneralCategoryGroup) -> Self {
        Self { mask: other.into() }
    }
}
