// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;

    #[cfg(feature = "buffer_provider")]
    use crate::unstable::{errors::ffi::DataError, provider::ffi::DataProvider};

    /// Lookup of the `Canonical_Combining_Class` Unicode property
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::normalizer::properties::CanonicalCombiningClassMap, Struct)]
    #[diplomat::rust_link(
        icu::normalizer::properties::CanonicalCombiningClassMapBorrowed,
        Struct,
        hidden
    )]
    pub struct CanonicalCombiningClassMap(
        pub icu_normalizer::properties::CanonicalCombiningClassMap,
    );

    impl CanonicalCombiningClassMap {
        /// Construct a new `CanonicalCombiningClassMap` instance for NFC using compiled data.
        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalCombiningClassMap::new,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalCombiningClassMapBorrowed::new,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(auto, constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create() -> Box<CanonicalCombiningClassMap> {
            Box::new(CanonicalCombiningClassMap(
                icu_normalizer::properties::CanonicalCombiningClassMap::new().static_to_owned(),
            ))
        }

        /// Construct a new `CanonicalCombiningClassMap` instance for NFC using a particular data source.
        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalCombiningClassMap::new,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalCombiningClassMapBorrowed::new,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CanonicalCombiningClassMap>, DataError> {
            Ok(Box::new(CanonicalCombiningClassMap(icu_normalizer::properties::CanonicalCombiningClassMap::try_new_with_buffer_provider(provider.get()?)?)))
        }

        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalCombiningClassMapBorrowed::get,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalCombiningClassMapBorrowed::get32,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalCombiningClassMapBorrowed::get32_u8,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalCombiningClassMapBorrowed::get_u8,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass, Struct, compact)]
        #[diplomat::attr(all(supports = indexing, not(kotlin)), indexer)] // Kotlin doesn't support non-integral indexers
        pub fn get(&self, ch: DiplomatChar) -> u8 {
            self.0.as_borrowed().get32_u8(ch)
        }
    }

    /// The raw canonical composition operation.
    ///
    /// Callers should generally use `ComposingNormalizer` unless they specifically need raw composition operations
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::normalizer::properties::CanonicalComposition, Struct)]
    #[diplomat::rust_link(
        icu::normalizer::properties::CanonicalCompositionBorrowed,
        Struct,
        hidden
    )]
    pub struct CanonicalComposition(pub icu_normalizer::properties::CanonicalComposition);

    impl CanonicalComposition {
        /// Construct a new `CanonicalComposition` instance for NFC using compiled data.
        #[diplomat::rust_link(icu::normalizer::properties::CanonicalComposition::new, FnInStruct)]
        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalCompositionBorrowed::new,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(auto, constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create() -> Box<CanonicalComposition> {
            Box::new(CanonicalComposition(
                icu_normalizer::properties::CanonicalComposition::new().static_to_owned(),
            ))
        }

        /// Construct a new `CanonicalComposition` instance for NFC using a particular data source.
        #[diplomat::rust_link(icu::normalizer::properties::CanonicalComposition::new, FnInStruct)]
        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalCompositionBorrowed::new,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CanonicalComposition>, DataError> {
            Ok(Box::new(CanonicalComposition(
                icu_normalizer::properties::CanonicalComposition::try_new_with_buffer_provider(
                    provider.get()?,
                )?,
            )))
        }

        /// Performs canonical composition (including Hangul) on a pair of characters
        /// or returns NUL if these characters don’t compose. Composition exclusions are taken into account.
        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalCompositionBorrowed::compose,
            FnInStruct
        )]
        pub fn compose(&self, starter: DiplomatChar, second: DiplomatChar) -> DiplomatChar {
            match (char::from_u32(starter), char::from_u32(second)) {
                (Some(starter), Some(second)) => self.0.as_borrowed().compose(starter, second),
                _ => None,
            }
            .unwrap_or('\0') as DiplomatChar
        }
    }

    /// The outcome of non-recursive canonical decomposition of a character.
    /// `second` will be NUL when the decomposition expands to a single character
    /// (which may or may not be the original one)
    #[diplomat::rust_link(icu::normalizer::properties::Decomposed, Enum)]
    #[diplomat::out]
    pub struct Decomposed {
        first: DiplomatChar,
        second: DiplomatChar,
    }

    /// The raw (non-recursive) canonical decomposition operation.
    ///
    /// Callers should generally use `DecomposingNormalizer` unless they specifically need raw composition operations
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::normalizer::properties::CanonicalDecomposition, Struct)]
    #[diplomat::rust_link(
        icu::normalizer::properties::CanonicalDecompositionBorrowed,
        Struct,
        hidden
    )]
    pub struct CanonicalDecomposition(pub icu_normalizer::properties::CanonicalDecomposition);

    impl CanonicalDecomposition {
        /// Construct a new `CanonicalDecomposition` instance for NFC using compiled data.
        #[diplomat::rust_link(icu::normalizer::properties::CanonicalDecomposition::new, FnInStruct)]
        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalDecompositionBorrowed::new,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(auto, constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create() -> Box<CanonicalDecomposition> {
            Box::new(CanonicalDecomposition(
                icu_normalizer::properties::CanonicalDecomposition::new().static_to_owned(),
            ))
        }

        /// Construct a new `CanonicalDecomposition` instance for NFC using a particular data source.
        #[diplomat::rust_link(icu::normalizer::properties::CanonicalDecomposition::new, FnInStruct)]
        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalDecompositionBorrowed::new,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CanonicalDecomposition>, DataError> {
            Ok(Box::new(CanonicalDecomposition(
                icu_normalizer::properties::CanonicalDecomposition::try_new_with_buffer_provider(
                    provider.get()?,
                )?,
            )))
        }

        /// Performs non-recursive canonical decomposition (including for Hangul).
        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalDecompositionBorrowed::decompose,
            FnInStruct
        )]
        pub fn decompose(&self, c: DiplomatChar) -> Decomposed {
            match char::from_u32(c) {
                Some(c) => match self.0.as_borrowed().decompose(c) {
                    icu_normalizer::properties::Decomposed::Default => Decomposed {
                        first: c as DiplomatChar,
                        second: '\0' as DiplomatChar,
                    },
                    icu_normalizer::properties::Decomposed::Singleton(s) => Decomposed {
                        first: s as DiplomatChar,
                        second: '\0' as DiplomatChar,
                    },
                    icu_normalizer::properties::Decomposed::Expansion(first, second) => {
                        Decomposed {
                            first: first as DiplomatChar,
                            second: second as DiplomatChar,
                        }
                    }
                },
                _ => Decomposed {
                    first: c,
                    second: '\0' as DiplomatChar,
                },
            }
        }
    }
}
