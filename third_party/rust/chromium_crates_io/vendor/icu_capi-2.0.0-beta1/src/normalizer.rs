// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    use crate::{errors::ffi::DataError, provider::ffi::DataProvider};

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::normalizer::ComposingNormalizer, Struct)]
    #[diplomat::rust_link(icu::normalizer::ComposingNormalizerBorrowed, Struct, hidden)]
    pub struct ComposingNormalizer(pub icu_normalizer::ComposingNormalizer);

    impl ComposingNormalizer {
        /// Construct a new ComposingNormalizer instance for NFC
        #[diplomat::rust_link(icu::normalizer::ComposingNormalizer::new_nfc, FnInStruct)]
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizerBorrowed::new_nfc,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "nfc")]
        #[diplomat::demo(default_constructor)]
        pub fn create_nfc(provider: &DataProvider) -> Result<Box<ComposingNormalizer>, DataError> {
            Ok(Box::new(ComposingNormalizer(call_constructor!(
                icu_normalizer::ComposingNormalizer::new_nfc [r => Ok(r.static_to_owned())],
                icu_normalizer::ComposingNormalizer::try_new_nfc_with_any_provider,
                icu_normalizer::ComposingNormalizer::try_new_nfc_with_buffer_provider,
                provider,
            )?)))
        }

        /// Construct a new ComposingNormalizer instance for NFKC
        #[diplomat::rust_link(icu::normalizer::ComposingNormalizer::new_nfkc, FnInStruct)]
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizerBorrowed::new_nfkc,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "nfkc")]
        pub fn create_nfkc(provider: &DataProvider) -> Result<Box<ComposingNormalizer>, DataError> {
            Ok(Box::new(ComposingNormalizer(call_constructor!(
                icu_normalizer::ComposingNormalizer::new_nfkc [r => Ok(r.static_to_owned())],
                icu_normalizer::ComposingNormalizer::try_new_nfkc_with_any_provider,
                icu_normalizer::ComposingNormalizer::try_new_nfkc_with_buffer_provider,
                provider,
            )?)))
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
            icu::normalizer::ComposingNormalizerBorrowed::is_normalized_utf8_up_to,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizerBorrowed::is_normalized_up_to,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(not(supports = utf8_strings), disable)]
        #[diplomat::attr(*, rename = "is_normalized_up_to")]
        pub fn is_normalized_utf8_up_to(&self, s: &DiplomatStr) -> usize {
            self.0.as_borrowed().is_normalized_utf8_up_to(s)
        }

        /// Return the index a slice of potentially-invalid UTF-8 is normalized up to
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizerBorrowed::is_normalized_utf16_up_to,
            FnInStruct
        )]
        #[diplomat::attr(not(supports = utf8_strings), rename = "is_normalized_up_to")]
        #[diplomat::attr(supports = utf8_strings, rename = "is_normalized16_up_to")]
        pub fn is_normalized_utf16_up_to(&self, s: &DiplomatStr16) -> usize {
            self.0.as_borrowed().is_normalized_utf16_up_to(s)
        }
    }

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::normalizer::DecomposingNormalizer, Struct)]
    #[diplomat::rust_link(icu::normalizer::DecomposingNormalizerBorrowed, Struct, hidden)]
    pub struct DecomposingNormalizer(pub icu_normalizer::DecomposingNormalizer);

    impl DecomposingNormalizer {
        /// Construct a new DecomposingNormalizer instance for NFD
        #[diplomat::rust_link(icu::normalizer::DecomposingNormalizer::new_nfd, FnInStruct)]
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizerBorrowed::new_nfd,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "nfd")]
        #[diplomat::demo(default_constructor)]
        pub fn create_nfd(
            provider: &DataProvider,
        ) -> Result<Box<DecomposingNormalizer>, DataError> {
            Ok(Box::new(DecomposingNormalizer(call_constructor!(
                icu_normalizer::DecomposingNormalizer::new_nfd [r => Ok(r.static_to_owned())],
                icu_normalizer::DecomposingNormalizer::try_new_nfd_with_any_provider,
                icu_normalizer::DecomposingNormalizer::try_new_nfd_with_buffer_provider,
                provider,
            )?)))
        }

        /// Construct a new DecomposingNormalizer instance for NFKD
        #[diplomat::rust_link(icu::normalizer::DecomposingNormalizer::new_nfkd, FnInStruct)]
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizerBorrowed::new_nfkd,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "nfkd")]
        pub fn create_nfkd(
            provider: &DataProvider,
        ) -> Result<Box<DecomposingNormalizer>, DataError> {
            Ok(Box::new(DecomposingNormalizer(call_constructor!(
                icu_normalizer::DecomposingNormalizer::new_nfkd [r => Ok(r.static_to_owned())],
                icu_normalizer::DecomposingNormalizer::try_new_nfkd_with_any_provider,
                icu_normalizer::DecomposingNormalizer::try_new_nfkd_with_buffer_provider,
                provider,
            )?)))
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
        pub fn is_normalized(&self, s: &DiplomatStr) -> bool {
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
        pub fn is_normalized_utf16(&self, s: &DiplomatStr16) -> bool {
            self.0.as_borrowed().is_normalized_utf16(s)
        }

        /// Return the index a slice of potentially-invalid UTF-8 is normalized up to
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizerBorrowed::is_normalized_utf8_up_to,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizerBorrowed::is_normalized_up_to,
            FnInStruct,
            hidden
        )]
        pub fn is_normalized_up_to(&self, s: &DiplomatStr) -> usize {
            self.0.as_borrowed().is_normalized_utf8_up_to(s)
        }

        /// Return the index a slice of potentially-invalid UTF-8 is normalized up to
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizerBorrowed::is_normalized_utf16_up_to,
            FnInStruct
        )]
        pub fn is_normalized_utf16_up_to(&self, s: &DiplomatStr16) -> usize {
            self.0.as_borrowed().is_normalized_utf16_up_to(s)
        }
    }
}
