// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_casemap::options::TitlecaseOptions;

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use crate::errors::ffi::DataError;
    use crate::locale_core::ffi::Locale;
    #[cfg(feature = "buffer_provider")]
    use crate::provider::ffi::DataProvider;
    use diplomat_runtime::DiplomatOption;

    use writeable::Writeable;

    #[diplomat::enum_convert(icu_casemap::options::LeadingAdjustment, needs_wildcard)]
    #[diplomat::rust_link(icu::casemap::options::LeadingAdjustment, Enum)]
    pub enum LeadingAdjustment {
        Auto,
        None,
        ToCased,
    }

    #[diplomat::enum_convert(icu_casemap::options::TrailingCase, needs_wildcard)]
    #[diplomat::rust_link(icu::casemap::options::TrailingCase, Enum)]
    pub enum TrailingCase {
        Lower,
        Unchanged,
    }

    #[diplomat::rust_link(icu::casemap::options::TitlecaseOptions, Struct)]
    #[diplomat::attr(supports = non_exhaustive_structs, rename = "TitlecaseOptions")]
    pub struct TitlecaseOptionsV1 {
        pub leading_adjustment: DiplomatOption<LeadingAdjustment>,
        pub trailing_case: DiplomatOption<TrailingCase>,
    }

    impl TitlecaseOptionsV1 {
        #[diplomat::rust_link(icu::casemap::options::TitlecaseOptions::default, FnInStruct)]
        #[diplomat::attr(auto, constructor)]
        #[diplomat::attr(any(cpp, js), rename = "default_options")]
        pub fn default() -> TitlecaseOptionsV1 {
            Self {
                leading_adjustment: None.into(),
                trailing_case: None.into(),
            }
        }
    }

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::casemap::CaseMapper, Struct)]
    #[diplomat::rust_link(icu::casemap::CaseMapperBorrowed, Struct, hidden)]
    pub struct CaseMapper(pub icu_casemap::CaseMapper);

    impl CaseMapper {
        /// Construct a new CaseMapper instance using compiled data.
        #[diplomat::rust_link(icu::casemap::CaseMapper::new, FnInStruct)]
        #[diplomat::rust_link(icu::casemap::CaseMapperBorrowed::new, FnInStruct, hidden)]
        #[diplomat::attr(auto, constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create() -> Box<CaseMapper> {
            Box::new(CaseMapper(icu_casemap::CaseMapper::new().static_to_owned()))
        }

        /// Construct a new CaseMapper instance using a particular data source.
        #[diplomat::rust_link(icu::casemap::CaseMapper::new, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_with_provider(provider: &DataProvider) -> Result<Box<CaseMapper>, DataError> {
            Ok(Box::new(CaseMapper(
                icu_casemap::CaseMapper::try_new_with_buffer_provider(provider.get()?)?,
            )))
        }
        /// Returns the full lowercase mapping of the given string
        #[diplomat::rust_link(icu::casemap::CaseMapperBorrowed::lowercase, FnInStruct)]
        #[diplomat::rust_link(
            icu::casemap::CaseMapperBorrowed::lowercase_to_string,
            FnInStruct,
            hidden
        )]
        pub fn lowercase(&self, s: &str, locale: &Locale, write: &mut DiplomatWrite) {
            let _infallible = self
                .0
                .as_borrowed()
                .lowercase(s, &locale.0.id)
                .write_to(write);
        }

        /// Returns the full uppercase mapping of the given string
        #[diplomat::rust_link(icu::casemap::CaseMapperBorrowed::uppercase, FnInStruct)]
        #[diplomat::rust_link(
            icu::casemap::CaseMapperBorrowed::uppercase_to_string,
            FnInStruct,
            hidden
        )]
        pub fn uppercase(&self, s: &str, locale: &Locale, write: &mut DiplomatWrite) {
            let _infallible = self
                .0
                .as_borrowed()
                .uppercase(s, &locale.0.id)
                .write_to(write);
        }

        /// Returns the full titlecase mapping of the given string, performing head adjustment without
        /// loading additional data.
        /// (if head adjustment is enabled in the options)
        ///
        /// The `v1` refers to the version of the options struct, which may change as we add more options
        #[diplomat::rust_link(
            icu::casemap::CaseMapperBorrowed::titlecase_segment_with_only_case_data,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::casemap::CaseMapperBorrowed::titlecase_segment_with_only_case_data_to_string,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(supports = non_exhaustive_structs, rename = "titlecase_segment_with_only_case_data")]
        pub fn titlecase_segment_with_only_case_data_v1(
            &self,
            s: &str,
            locale: &Locale,
            options: TitlecaseOptionsV1,
            write: &mut DiplomatWrite,
        ) {
            let _infallible = self
                .0
                .as_borrowed()
                .titlecase_segment_with_only_case_data(s, &locale.0.id, options.into())
                .write_to(write);
        }

        /// Case-folds the characters in the given string
        #[diplomat::rust_link(icu::casemap::CaseMapperBorrowed::fold, FnInStruct)]
        #[diplomat::rust_link(icu::casemap::CaseMapperBorrowed::fold_string, FnInStruct, hidden)]
        pub fn fold(&self, s: &str, write: &mut DiplomatWrite) {
            let _infallible = self.0.as_borrowed().fold(s).write_to(write);
        }
        /// Case-folds the characters in the given string
        /// using Turkic (T) mappings for dotted/dotless I.
        #[diplomat::rust_link(icu::casemap::CaseMapperBorrowed::fold_turkic, FnInStruct)]
        #[diplomat::rust_link(
            icu::casemap::CaseMapperBorrowed::fold_turkic_string,
            FnInStruct,
            hidden
        )]
        pub fn fold_turkic(&self, s: &str, write: &mut DiplomatWrite) {
            let _infallible = self.0.as_borrowed().fold_turkic(s).write_to(write);
        }

        /// Adds all simple case mappings and the full case folding for `c` to `builder`.
        /// Also adds special case closure mappings.
        ///
        /// In other words, this adds all characters that this casemaps to, as
        /// well as all characters that may casemap to this one.
        ///
        /// Note that since CodePointSetBuilder does not contain strings, this will
        /// ignore string mappings.
        ///
        /// Identical to the similarly named method on `CaseMapCloser`, use that if you
        /// plan on using string case closure mappings too.
        #[cfg(feature = "properties")]
        #[diplomat::rust_link(icu::casemap::CaseMapperBorrowed::add_case_closure_to, FnInStruct)]
        #[diplomat::rust_link(icu::casemap::ClosureSink, Trait, hidden)]
        #[diplomat::rust_link(icu::casemap::ClosureSink::add_char, FnInTrait, hidden)]
        #[diplomat::rust_link(icu::casemap::ClosureSink::add_string, FnInTrait, hidden)]
        pub fn add_case_closure_to(
            &self,
            c: DiplomatChar,
            builder: &mut crate::collections_sets::ffi::CodePointSetBuilder,
        ) {
            if let Some(ch) = char::from_u32(c) {
                self.0.as_borrowed().add_case_closure_to(ch, &mut builder.0)
            }
        }

        /// Returns the simple lowercase mapping of the given character.
        ///
        /// This function only implements simple and common mappings.
        /// Full mappings, which can map one char to a string, are not included.
        /// For full mappings, use `CaseMapperBorrowed::lowercase`.
        #[diplomat::rust_link(icu::casemap::CaseMapperBorrowed::simple_lowercase, FnInStruct)]
        pub fn simple_lowercase(&self, ch: DiplomatChar) -> DiplomatChar {
            char::from_u32(ch)
                .map(|ch| self.0.as_borrowed().simple_lowercase(ch) as DiplomatChar)
                .unwrap_or(ch)
        }

        /// Returns the simple uppercase mapping of the given character.
        ///
        /// This function only implements simple and common mappings.
        /// Full mappings, which can map one char to a string, are not included.
        /// For full mappings, use `CaseMapperBorrowed::uppercase`.
        #[diplomat::rust_link(icu::casemap::CaseMapperBorrowed::simple_uppercase, FnInStruct)]
        pub fn simple_uppercase(&self, ch: DiplomatChar) -> DiplomatChar {
            char::from_u32(ch)
                .map(|ch| self.0.as_borrowed().simple_uppercase(ch) as DiplomatChar)
                .unwrap_or(ch)
        }

        /// Returns the simple titlecase mapping of the given character.
        ///
        /// This function only implements simple and common mappings.
        /// Full mappings, which can map one char to a string, are not included.
        /// For full mappings, use `CaseMapperBorrowed::titlecase_segment`.
        #[diplomat::rust_link(icu::casemap::CaseMapperBorrowed::simple_titlecase, FnInStruct)]
        pub fn simple_titlecase(&self, ch: DiplomatChar) -> DiplomatChar {
            char::from_u32(ch)
                .map(|ch| self.0.as_borrowed().simple_titlecase(ch) as DiplomatChar)
                .unwrap_or(ch)
        }

        /// Returns the simple casefolding of the given character.
        ///
        /// This function only implements simple folding.
        /// For full folding, use `CaseMapperBorrowed::fold`.
        #[diplomat::rust_link(icu::casemap::CaseMapperBorrowed::simple_fold, FnInStruct)]
        pub fn simple_fold(&self, ch: DiplomatChar) -> DiplomatChar {
            char::from_u32(ch)
                .map(|ch| self.0.as_borrowed().simple_fold(ch) as DiplomatChar)
                .unwrap_or(ch)
        }
        /// Returns the simple casefolding of the given character in the Turkic locale
        ///
        /// This function only implements simple folding.
        /// For full folding, use `CaseMapperBorrowed::fold_turkic`.
        #[diplomat::rust_link(icu::casemap::CaseMapperBorrowed::simple_fold_turkic, FnInStruct)]
        pub fn simple_fold_turkic(&self, ch: DiplomatChar) -> DiplomatChar {
            char::from_u32(ch)
                .map(|ch| self.0.as_borrowed().simple_fold_turkic(ch) as DiplomatChar)
                .unwrap_or(ch)
        }
    }

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::casemap::CaseMapCloser, Struct)]
    #[diplomat::rust_link(icu::casemap::CaseMapCloserBorrowed, Struct, hidden)]
    pub struct CaseMapCloser(pub icu_casemap::CaseMapCloser<icu_casemap::CaseMapper>);

    impl CaseMapCloser {
        /// Construct a new CaseMapCloser instance using compiled data.
        #[diplomat::rust_link(icu::casemap::CaseMapCloser::new, FnInStruct)]
        #[diplomat::rust_link(icu::casemap::CaseMapCloserBorrowed::new, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::casemap::CaseMapCloser::new_with_mapper, FnInStruct, hidden)]
        #[diplomat::attr(supports = "fallible_constructors", constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create() -> Result<Box<CaseMapCloser>, DataError> {
            Ok(Box::new(CaseMapCloser(
                icu_casemap::CaseMapCloser::new().static_to_owned(),
            )))
        }
        /// Construct a new CaseMapCloser instance using a particular data source.
        #[diplomat::rust_link(icu::casemap::CaseMapCloser::new, FnInStruct)]
        #[diplomat::rust_link(icu::casemap::CaseMapCloser::new_with_mapper, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CaseMapCloser>, DataError> {
            Ok(Box::new(CaseMapCloser(
                icu_casemap::CaseMapCloser::try_new_with_buffer_provider(provider.get()?)?,
            )))
        }
        /// Adds all simple case mappings and the full case folding for `c` to `builder`.
        /// Also adds special case closure mappings.
        #[cfg(feature = "properties")]
        #[diplomat::rust_link(icu::casemap::CaseMapCloserBorrowed::add_case_closure_to, FnInStruct)]
        pub fn add_case_closure_to(
            &self,
            c: DiplomatChar,
            builder: &mut crate::collections_sets::ffi::CodePointSetBuilder,
        ) {
            if let Some(ch) = char::from_u32(c) {
                self.0.as_borrowed().add_case_closure_to(ch, &mut builder.0)
            }
        }

        /// Finds all characters and strings which may casemap to `s` as their full case folding string
        /// and adds them to the set.
        ///
        /// Returns true if the string was found
        #[cfg(feature = "properties")]
        #[diplomat::rust_link(
            icu::casemap::CaseMapCloserBorrowed::add_string_case_closure_to,
            FnInStruct
        )]
        pub fn add_string_case_closure_to(
            &self,
            s: &DiplomatStr,
            builder: &mut crate::collections_sets::ffi::CodePointSetBuilder,
        ) -> bool {
            let s = core::str::from_utf8(s).unwrap_or("");
            self.0
                .as_borrowed()
                .add_string_case_closure_to(s, &mut builder.0)
        }
    }

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::casemap::TitlecaseMapper, Struct)]
    #[diplomat::rust_link(icu::casemap::TitlecaseMapperBorrowed, Struct, hidden)]
    pub struct TitlecaseMapper(pub icu_casemap::TitlecaseMapper<icu_casemap::CaseMapper>);

    impl TitlecaseMapper {
        /// Construct a new `TitlecaseMapper` instance using compiled data.
        #[diplomat::rust_link(icu::casemap::TitlecaseMapper::new, FnInStruct)]
        #[diplomat::rust_link(icu::casemap::TitlecaseMapperBorrowed::new, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::casemap::TitlecaseMapper::new_with_mapper, FnInStruct, hidden)]
        #[diplomat::attr(supports = "fallible_constructors", constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create() -> Result<Box<TitlecaseMapper>, DataError> {
            Ok(Box::new(TitlecaseMapper(
                icu_casemap::TitlecaseMapper::new().static_to_owned(),
            )))
        }
        /// Construct a new `TitlecaseMapper` instance using a particular data source.
        #[diplomat::rust_link(icu::casemap::TitlecaseMapper::new, FnInStruct)]
        #[diplomat::rust_link(icu::casemap::TitlecaseMapper::new_with_mapper, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<TitlecaseMapper>, DataError> {
            Ok(Box::new(TitlecaseMapper(
                icu_casemap::TitlecaseMapper::try_new_with_buffer_provider(provider.get()?)?,
            )))
        }
        /// Returns the full titlecase mapping of the given string
        ///
        /// The `v1` refers to the version of the options struct, which may change as we add more options
        #[diplomat::rust_link(icu::casemap::TitlecaseMapperBorrowed::titlecase_segment, FnInStruct)]
        #[diplomat::rust_link(
            icu::casemap::TitlecaseMapperBorrowed::titlecase_segment_to_string,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(supports = non_exhaustive_structs, rename = "titlecase_segment")]
        pub fn titlecase_segment_v1(
            &self,
            s: &str,
            locale: &Locale,
            options: TitlecaseOptionsV1,
            write: &mut DiplomatWrite,
        ) {
            let _infallible = self
                .0
                .as_borrowed()
                .titlecase_segment(s, &locale.0.id, options.into())
                .write_to(write);
        }
    }
}

impl From<ffi::TitlecaseOptionsV1> for TitlecaseOptions {
    fn from(other: ffi::TitlecaseOptionsV1) -> Self {
        let mut ret = Self::default();

        ret.leading_adjustment = other.leading_adjustment.into_converted_option();

        ret.trailing_case = other.trailing_case.into_converted_option();

        ret
    }
}
