// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;
    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use icu_properties::props::BasicEmoji;

    #[cfg(feature = "buffer_provider")]
    use crate::unstable::{errors::ffi::DataError, provider::ffi::DataProvider};

    #[diplomat::opaque]
    /// An ICU4X Unicode Set Property object, capable of querying whether a code point is contained in a set based on a Unicode property.
    #[diplomat::rust_link(icu::properties, Mod)]
    #[diplomat::rust_link(icu::properties::EmojiSetData, Struct)]
    #[diplomat::rust_link(icu::properties::EmojiSetData::new, FnInStruct)]
    #[diplomat::rust_link(icu::properties::EmojiSetDataBorrowed::new, FnInStruct, hidden)]
    #[diplomat::rust_link(icu::properties::EmojiSetDataBorrowed, Struct)]
    pub struct EmojiSetData(pub icu_properties::EmojiSetData);

    impl EmojiSetData {
        /// Checks whether the string is in the set.
        #[diplomat::rust_link(icu::properties::EmojiSetDataBorrowed::contains_str, FnInStruct)]
        #[diplomat::rust_link(
            icu::properties::EmojiSetDataBorrowed::contains_utf8,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(supports = method_overloading, rename = "contains")]
        pub fn contains_str(&self, s: &DiplomatStr) -> bool {
            self.0.as_borrowed().contains_utf8(s)
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

        /// Create a map for the `Basic_Emoji` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::BasicEmoji, Struct)]
        #[diplomat::attr(auto, named_constructor = "basic")]
        #[cfg(feature = "compiled_data")]
        #[diplomat::demo(default_constructor)]
        pub fn create_basic() -> Box<EmojiSetData> {
            Box::new(EmojiSetData(
                icu_properties::EmojiSetData::new::<BasicEmoji>().static_to_owned(),
            ))
        }
        /// Create a map for the `Basic_Emoji` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::BasicEmoji, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "basic_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_basic_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<EmojiSetData>, DataError> {
            Ok(Box::new(EmojiSetData(
                icu_properties::EmojiSetData::try_new_unstable::<BasicEmoji>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Basic_Emoji` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::EmojiSet::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn basic_emoji_for_char(ch: DiplomatChar) -> bool {
            icu_properties::EmojiSetData::new::<BasicEmoji>().contains32(ch)
        }

        /// Get the `Basic_Emoji` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::EmojiSet::for_str, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn basic_emoji_for_str(s: &DiplomatStr) -> bool {
            icu_properties::EmojiSetData::new::<BasicEmoji>().contains_utf8(s)
        }
    }
}
