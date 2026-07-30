// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;

    #[cfg(feature = "buffer_provider")]
    use crate::unstable::{errors::ffi::DataError, provider::ffi::DataProvider};

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::normalizer::ComposingNormalizer, Struct)]
    #[diplomat::rust_link(icu::normalizer::ComposingNormalizerBorrowed, Struct, hidden)]
    pub struct ComposingNormalizer(pub icu_normalizer::ComposingNormalizer);

    impl ComposingNormalizer {
        /// Construct a new `ComposingNormalizer` instance for NFC using compiled data.
        #[diplomat::rust_link(icu::normalizer::ComposingNormalizer::new_nfc, FnInStruct)]
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizerBorrowed::new_nfc,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(auto, named_constructor = "nfc")]
        #[diplomat::demo(default_constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create_nfc() -> Box<ComposingNormalizer> {
            Box::new(ComposingNormalizer(
                icu_normalizer::ComposingNormalizer::new_nfc().static_to_owned(),
            ))
        }
        /// Construct a new `ComposingNormalizer` instance for NFC using a particular data source.
        #[diplomat::rust_link(icu::normalizer::ComposingNormalizer::new_nfc, FnInStruct)]
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizerBorrowed::new_nfc,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "nfc_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_nfc_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<ComposingNormalizer>, DataError> {
            Ok(Box::new(ComposingNormalizer(
                icu_normalizer::ComposingNormalizer::try_new_nfc_with_buffer_provider(
                    provider.get()?,
                )?,
            )))
        }
        /// Construct a new `ComposingNormalizer` instance for NFKC using compiled data.
        #[diplomat::rust_link(icu::normalizer::ComposingNormalizer::new_nfkc, FnInStruct)]
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizerBorrowed::new_nfkc,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(auto, named_constructor = "nfkc")]
        #[cfg(feature = "compiled_data")]
        pub fn create_nfkc() -> Box<ComposingNormalizer> {
            Box::new(ComposingNormalizer(
                icu_normalizer::ComposingNormalizer::new_nfkc().static_to_owned(),
            ))
        }
        /// Construct a new `ComposingNormalizer` instance for NFKC using a particular data source.
        #[diplomat::rust_link(icu::normalizer::ComposingNormalizer::new_nfkc, FnInStruct)]
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizerBorrowed::new_nfkc,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "nfkc_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_nfkc_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<ComposingNormalizer>, DataError> {
            Ok(Box::new(ComposingNormalizer(
                icu_normalizer::ComposingNormalizer::try_new_nfkc_with_buffer_provider(
                    provider.get()?,
                )?,
            )))
        }
        /// Normalize a string
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizerBorrowed::normalize_utf8,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizerBorrowed::normalize,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizerBorrowed::normalize_to,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizerBorrowed::normalize_utf8_to,
            FnInStruct,
            hidden
        )]
        pub fn normalize(&self, s: &DiplomatStr, write: &mut DiplomatWrite) {
            let _infallible = self.0.as_borrowed().normalize_utf8_to(s, write);
        }

        /// Check if a string is normalized
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizerBorrowed::is_normalized_utf8,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizerBorrowed::is_normalized,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(not(supports = utf8_strings), disable)]
        #[diplomat::attr(*, rename = "is_normalized")]
        pub fn is_normalized_utf8(&self, s: &DiplomatStr) -> bool {
            self.0.as_borrowed().is_normalized_utf8(s)
        }

        /// Check if a string is normalized
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizerBorrowed::is_normalized_utf16,
            FnInStruct
        )]
        #[diplomat::attr(not(supports = utf8_strings), rename = "is_normalized")]
        #[diplomat::attr(supports = utf8_strings, rename = "is_normalized16")]
        pub fn is_normalized_utf16(&self, s: &DiplomatStr16) -> bool {
            self.0.as_borrowed().is_normalized_utf16(s)
        }

        /// Return the index a slice of potentially-invalid UTF-8 is normalized up to
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizerBorrowed::split_normalized_utf8,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizerBorrowed::split_normalized,
            FnInStruct
        )]
        #[diplomat::attr(not(supports = utf8_strings), disable)]
        #[diplomat::attr(*, rename = "is_normalized_up_to")]
        pub fn is_normalized_utf8_up_to(&self, s: &DiplomatStr) -> usize {
            self.0.as_borrowed().split_normalized_utf8(s).0.len()
        }

        /// Return the index a slice of potentially-invalid UTF-16 is normalized up to
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizerBorrowed::split_normalized_utf16,
            FnInStruct
        )]
        #[diplomat::attr(not(supports = utf8_strings), rename = "is_normalized_up_to")]
        #[diplomat::attr(supports = utf8_strings, rename = "is_normalized16_up_to")]
        pub fn is_normalized_utf16_up_to(&self, s: &DiplomatStr16) -> usize {
            self.0.as_borrowed().split_normalized_utf16(s).0.len()
        }
    }

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::normalizer::DecomposingNormalizer, Struct)]
    #[diplomat::rust_link(icu::normalizer::DecomposingNormalizerBorrowed, Struct, hidden)]
    pub struct DecomposingNormalizer(pub icu_normalizer::DecomposingNormalizer);

    impl DecomposingNormalizer {
        /// Construct a new `DecomposingNormalizer` instance for NFD using compiled data.
        #[diplomat::rust_link(icu::normalizer::DecomposingNormalizer::new_nfd, FnInStruct)]
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizerBorrowed::new_nfd,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "nfd")]
        #[diplomat::demo(default_constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create_nfd() -> Box<DecomposingNormalizer> {
            Box::new(DecomposingNormalizer(
                icu_normalizer::DecomposingNormalizer::new_nfd().static_to_owned(),
            ))
        }

        /// Construct a new `DecomposingNormalizer` instance for NFD using a particular data source.
        #[diplomat::rust_link(icu::normalizer::DecomposingNormalizer::new_nfd, FnInStruct)]
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizerBorrowed::new_nfd,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "nfd_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_nfd_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<DecomposingNormalizer>, DataError> {
            Ok(Box::new(DecomposingNormalizer(
                icu_normalizer::DecomposingNormalizer::try_new_nfd_with_buffer_provider(
                    provider.get()?,
                )?,
            )))
        }

        /// Construct a new `DecomposingNormalizer` instance for NFKD using compiled data.
        #[diplomat::rust_link(icu::normalizer::DecomposingNormalizer::new_nfkd, FnInStruct)]
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizerBorrowed::new_nfkd,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(auto, named_constructor = "nfkd")]
        #[cfg(feature = "compiled_data")]
        pub fn create_nfkd() -> Box<DecomposingNormalizer> {
            Box::new(DecomposingNormalizer(
                icu_normalizer::DecomposingNormalizer::new_nfkd().static_to_owned(),
            ))
        }

        /// Construct a new `DecomposingNormalizer` instance for NFKD using a particular data source.
        #[diplomat::rust_link(icu::normalizer::DecomposingNormalizer::new_nfkd, FnInStruct)]
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizerBorrowed::new_nfkd,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "nfkd_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_nfkd_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<DecomposingNormalizer>, DataError> {
            Ok(Box::new(DecomposingNormalizer(
                icu_normalizer::DecomposingNormalizer::try_new_nfkd_with_buffer_provider(
                    provider.get()?,
                )?,
            )))
        }

        /// Normalize a string
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizerBorrowed::normalize_utf8,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizerBorrowed::normalize,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizerBorrowed::normalize_to,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizerBorrowed::normalize_utf8_to,
            FnInStruct,
            hidden
        )]
        pub fn normalize(&self, s: &DiplomatStr, write: &mut DiplomatWrite) {
            let _infallible = self.0.as_borrowed().normalize_utf8_to(s, write);
        }

        /// Check if a string is normalized
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizerBorrowed::is_normalized_utf8,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizerBorrowed::is_normalized,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(not(supports = utf8_strings), disable)]
        #[diplomat::attr(*, rename = "is_normalized")]
        #[diplomat::abi_rename = "icu4x_DecomposingNormalizer_is_normalized_mv1"] // TODO(3.0): remove
        pub fn is_normalized_utf8(&self, s: &DiplomatStr) -> bool {
            self.0.as_borrowed().is_normalized_utf8(s)
        }

        /// Check if a string is normalized
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizerBorrowed::is_normalized_utf16,
            FnInStruct
        )]
        #[diplomat::attr(not(supports = utf8_strings), rename = "is_normalized")]
        #[diplomat::attr(supports = utf8_strings, rename = "is_normalized16")]
        pub fn is_normalized_utf16(&self, s: &DiplomatStr16) -> bool {
            self.0.as_borrowed().is_normalized_utf16(s)
        }

        /// Return the index a slice of potentially-invalid UTF-8 is normalized up to
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizerBorrowed::split_normalized_utf8,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizerBorrowed::split_normalized,
            FnInStruct
        )]
        #[diplomat::attr(not(supports = utf8_strings), disable)]
        #[diplomat::attr(*, rename = "is_normalized_up_to")]
        #[diplomat::abi_rename = "icu4x_DecomposingNormalizer_is_normalized_up_to_mv1"] // TODO(3.0): remove
        pub fn is_normalized_utf8_up_to(&self, s: &DiplomatStr) -> usize {
            self.0.as_borrowed().split_normalized_utf8(s).0.len()
        }

        /// Return the index a slice of potentially-invalid UTF-16 is normalized up to
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizerBorrowed::split_normalized_utf16,
            FnInStruct
        )]
        #[diplomat::attr(not(supports = utf8_strings), rename = "is_normalized_up_to")]
        #[diplomat::attr(supports = utf8_strings, rename = "is_normalized16_up_to")]
        pub fn is_normalized_utf16_up_to(&self, s: &DiplomatStr16) -> usize {
            self.0.as_borrowed().split_normalized_utf16(s).0.len()
        }
    }
}
