// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    use crate::properties_sets::ffi::CodePointSetData;

    #[diplomat::opaque]
    #[diplomat::rust_link(
        icu::collections::codepointinvlist::CodePointInversionListBuilder,
        Struct
    )]
    pub struct CodePointSetBuilder(
        pub icu_collections::codepointinvlist::CodePointInversionListBuilder,
    );

    impl CodePointSetBuilder {
        /// Make a new set builder containing nothing
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::new,
            FnInStruct
        )]
        #[diplomat::attr(auto, constructor)]
        pub fn create() -> Box<Self> {
            Box::new(Self(
                icu_collections::codepointinvlist::CodePointInversionListBuilder::new(),
            ))
        }

        /// Build this into a set
        ///
        /// This object is repopulated with an empty builder
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::build,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::properties::CodePointSetData::from_code_point_inversion_list,
            FnInStruct,
            hidden
        )]
        pub fn build(&mut self) -> Box<CodePointSetData> {
            let inner = core::mem::take(&mut self.0);
            let built = inner.build();
            let set = icu_properties::CodePointSetData::from_code_point_inversion_list(built);
            Box::new(CodePointSetData(set))
        }

        /// Complements this set
        ///
        /// (Elements in this set are removed and vice versa)
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::complement,
            FnInStruct
        )]
        pub fn complement(&mut self) {
            self.0.complement()
        }

        /// Returns whether this set is empty
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::is_empty,
            FnInStruct
        )]
        #[diplomat::attr(auto, getter)]
        pub fn is_empty(&self) -> bool {
            self.0.is_empty()
        }

        /// Add a single character to the set
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::add_char,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::add32,
            FnInStruct,
            hidden
        )]
        pub fn add_char(&mut self, ch: DiplomatChar) {
            self.0.add32(ch)
        }

        /// Add an inclusive range of characters to the set
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::add_range,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::add_range32,
            FnInStruct,
            hidden
        )]
        pub fn add_inclusive_range(&mut self, start: DiplomatChar, end: DiplomatChar) {
            self.0.add_range32(start..=end)
        }

        /// Add all elements that belong to the provided set to the set
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::add_set,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::properties::CodePointSetData::as_code_point_inversion_list,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::properties::CodePointSetData::to_code_point_inversion_list,
            FnInStruct,
            hidden
        )]
        pub fn add_set(&mut self, data: &CodePointSetData) {
            // This is a ZeroFrom and always cheap for a CPIL, may be expensive
            // for other impls. In the future we can make this builder support multiple impls
            // if we ever add them
            let list = data.0.to_code_point_inversion_list();
            self.0.add_set(&list);
        }

        /// Remove a single character to the set
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::remove_char,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::remove32,
            FnInStruct,
            hidden
        )]
        pub fn remove_char(&mut self, ch: DiplomatChar) {
            self.0.remove32(ch)
        }

        /// Remove an inclusive range of characters from the set
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::remove_range,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::remove_range32,
            FnInStruct,
            hidden
        )]
        pub fn remove_inclusive_range(&mut self, start: DiplomatChar, end: DiplomatChar) {
            self.0.remove_range32(start..=end)
        }

        /// Remove all elements that belong to the provided set from the set
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::remove_set,
            FnInStruct
        )]
        pub fn remove_set(&mut self, data: &CodePointSetData) {
            // (see comment in add_set)
            let list = data.0.to_code_point_inversion_list();
            self.0.remove_set(&list);
        }

        /// Removes all elements from the set except a single character
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::retain_char,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::retain32,
            FnInStruct,
            hidden
        )]
        pub fn retain_char(&mut self, ch: DiplomatChar) {
            self.0.retain32(ch)
        }

        /// Removes all elements from the set except an inclusive range of characters f
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::retain_range,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::retain_range32,
            FnInStruct,
            hidden
        )]
        pub fn retain_inclusive_range(&mut self, start: DiplomatChar, end: DiplomatChar) {
            self.0.retain_range32(start..=end)
        }

        /// Removes all elements from the set except all elements in the provided set
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::retain_set,
            FnInStruct
        )]
        pub fn retain_set(&mut self, data: &CodePointSetData) {
            // (see comment in add_set)
            let list = data.0.to_code_point_inversion_list();
            self.0.retain_set(&list);
        }

        /// Complement a single character to the set
        ///
        /// (Characters which are in this set are removed and vice versa)
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::complement_char,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::complement32,
            FnInStruct,
            hidden
        )]
        pub fn complement_char(&mut self, ch: DiplomatChar) {
            self.0.complement32(ch)
        }

        /// Complement an inclusive range of characters from the set
        ///
        /// (Characters which are in this set are removed and vice versa)
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::complement_range,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::complement_range32,
            FnInStruct,
            hidden
        )]
        pub fn complement_inclusive_range(&mut self, start: DiplomatChar, end: DiplomatChar) {
            self.0.complement_range32(start..=end)
        }

        /// Complement all elements that belong to the provided set from the set
        ///
        /// (Characters which are in this set are removed and vice versa)
        #[diplomat::rust_link(
            icu::collections::codepointinvlist::CodePointInversionListBuilder::complement_set,
            FnInStruct
        )]
        pub fn complement_set(&mut self, data: &CodePointSetData) {
            // (see comment in add_set)
            let list = data.0.to_code_point_inversion_list();
            self.0.complement_set(&list);
        }
    }
}
