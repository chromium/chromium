// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;
    use icu_properties::props::BasicEmoji;

    use crate::errors::ffi::DataError;
    use crate::provider::ffi::DataProvider;

    #[diplomat::opaque]
    /// An ICU4X Unicode Set Property object, capable of querying whether a code point is contained in a set based on a Unicode property.
    #[diplomat::rust_link(icu::properties, Mod)]
    #[diplomat::rust_link(icu::properties::EmojiSetData, Struct)]
    #[diplomat::rust_link(icu::properties::EmojiSetData::new, FnInStruct)]
    #[diplomat::rust_link(icu::properties::EmojiSetDataBorrowed, Struct)]
    pub struct EmojiSetData(pub icu_properties::EmojiSetData);

    impl EmojiSetData {
        /// Checks whether the string is in the set.
        #[diplomat::rust_link(icu::properties::EmojiSetDataBorrowed::contains_str, FnInStruct)]
        #[diplomat::attr(supports = method_overloading, rename = "contains")]
        pub fn contains_str(&self, s: &DiplomatStr) -> bool {
            let Ok(s) = core::str::from_utf8(s) else {
                return false;
            };
            self.0.as_borrowed().contains_str(s)
        }
        /// Checks whether the code point is in the set.
        #[diplomat::rust_link(icu::properties::EmojiSetDataBorrowed::contains, FnInStruct)]
        #[diplomat::rust_link(
            icu::properties::EmojiSetDataBorrowed::contains32,
            FnInStruct,
            hidden
        )]
        pub fn contains(&self, cp: DiplomatChar) -> bool {
            self.0.as_borrowed().contains32(cp)
        }

        #[diplomat::rust_link(icu::properties::props::BasicEmoji, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "basic")]
        pub fn load_basic(provider: &DataProvider) -> Result<Box<EmojiSetData>, DataError> {
            Ok(Box::new(EmojiSetData(call_constructor_unstable!(
                icu_properties::EmojiSetData::new::<BasicEmoji> [r => Ok(r.static_to_owned())],
                icu_properties::EmojiSetData::try_new_unstable::<BasicEmoji>,
                provider,
            )?)))
        }
    }
}
