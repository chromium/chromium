// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    use crate::{errors::ffi::DataError, locale_core::ffi::Locale, provider::ffi::DataProvider};
    use diplomat_runtime::DiplomatOption;

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::collator::Collator, Struct)]
    #[diplomat::rust_link(icu::collator::CollatorBorrowed, Struct, hidden)]
    pub struct Collator(pub icu_collator::Collator);

    #[diplomat::rust_link(icu::collator::CollatorOptions, Struct)]
    #[diplomat::rust_link(icu::collator::CollatorOptions::default, FnInStruct, hidden)]
    #[diplomat::attr(supports = non_exhaustive_structs, rename = "CollatorOptions")]
    pub struct CollatorOptionsV1 {
        pub strength: DiplomatOption<CollatorStrength>,
        pub alternate_handling: DiplomatOption<CollatorAlternateHandling>,
        pub case_first: DiplomatOption<CollatorCaseFirst>,
        pub max_variable: DiplomatOption<CollatorMaxVariable>,
        pub case_level: DiplomatOption<CollatorCaseLevel>,
        pub numeric: DiplomatOption<CollatorNumeric>,
        pub backward_second_level: DiplomatOption<CollatorBackwardSecondLevel>,
    }

    // Note the flipped order of the words `Collator` and `Resolved`, because
    // in FFI `Collator` is part of the `Collator` prefix, but in Rust,
    // `ResolvedCollatorOptions` makes more sense as English.
    #[diplomat::rust_link(icu::collator::ResolvedCollatorOptions, Struct)]
    #[diplomat::out]
    #[diplomat::attr(supports = non_exhaustive_structs, rename = "CollatorResolvedOptions")]
    pub struct CollatorResolvedOptionsV1 {
        pub strength: CollatorStrength,
        pub alternate_handling: CollatorAlternateHandling,
        pub case_first: CollatorCaseFirst,
        pub max_variable: CollatorMaxVariable,
        pub case_level: CollatorCaseLevel,
        pub numeric: CollatorNumeric,
        pub backward_second_level: CollatorBackwardSecondLevel,
    }

    #[diplomat::rust_link(icu::collator::Strength, Enum)]
    #[derive(Eq, PartialEq, Debug, PartialOrd, Ord)]
    #[diplomat::enum_convert(icu_collator::Strength, needs_wildcard)]
    pub enum CollatorStrength {
        Primary = 0,
        Secondary = 1,
        Tertiary = 2,
        Quaternary = 3,
        Identical = 4,
    }

    #[diplomat::rust_link(icu::collator::AlternateHandling, Enum)]
    #[derive(Eq, PartialEq, Debug, PartialOrd, Ord)]
    #[diplomat::enum_convert(icu_collator::AlternateHandling, needs_wildcard)]
    pub enum CollatorAlternateHandling {
        NonIgnorable = 0,
        Shifted = 1,
    }

    #[diplomat::rust_link(icu::collator::CaseFirst, Enum)]
    #[derive(Eq, PartialEq, Debug, PartialOrd, Ord)]
    #[diplomat::enum_convert(icu_collator::CaseFirst, needs_wildcard)]
    pub enum CollatorCaseFirst {
        Off = 0,
        LowerFirst = 1,
        UpperFirst = 2,
    }

    #[diplomat::rust_link(icu::collator::MaxVariable, Enum)]
    #[derive(Eq, PartialEq, Debug, PartialOrd, Ord)]
    #[diplomat::enum_convert(icu_collator::MaxVariable, needs_wildcard)]
    pub enum CollatorMaxVariable {
        Space = 0,
        Punctuation = 1,
        Symbol = 2,
        Currency = 3,
    }

    #[diplomat::rust_link(icu::collator::CaseLevel, Enum)]
    #[derive(Eq, PartialEq, Debug, PartialOrd, Ord)]
    #[diplomat::enum_convert(icu_collator::CaseLevel, needs_wildcard)]
    pub enum CollatorCaseLevel {
        Off = 0,
        On = 1,
    }

    #[diplomat::rust_link(icu::collator::Numeric, Enum)]
    #[derive(Eq, PartialEq, Debug, PartialOrd, Ord)]
    #[diplomat::enum_convert(icu_collator::Numeric, needs_wildcard)]
    pub enum CollatorNumeric {
        Off = 0,
        On = 1,
    }

    #[diplomat::rust_link(icu::collator::BackwardSecondLevel, Enum)]
    #[derive(Eq, PartialEq, Debug, PartialOrd, Ord)]
    #[diplomat::enum_convert(icu_collator::BackwardSecondLevel, needs_wildcard)]
    pub enum CollatorBackwardSecondLevel {
        Off = 0,
        On = 1,
    }

    impl Collator {
        /// Construct a new Collator instance.
        #[diplomat::rust_link(icu::collator::Collator::try_new, FnInStruct)]
        #[diplomat::rust_link(icu::collator::CollatorBorrowed::try_new, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::collator::CollatorPreferences, Struct, hidden)]
        #[diplomat::attr(supports = fallible_constructors, constructor)]
        #[diplomat::attr(supports = non_exhaustive_structs, rename = "create")]
        pub fn create_v1(
            provider: &DataProvider,
            locale: &Locale,
            options: CollatorOptionsV1,
        ) -> Result<Box<Collator>, DataError> {
            Ok(Box::new(Collator(call_constructor!(
                icu_collator::Collator::try_new [r => Ok(r?.static_to_owned())],
                icu_collator::Collator::try_new_with_any_provider,
                icu_collator::Collator::try_new_with_buffer_provider,
                provider,
                icu_collator::CollatorPreferences::from(&locale.0),
                icu_collator::CollatorOptions::from(options),
            )?)))
        }

        /// Compare two strings.
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(icu::collator::CollatorBorrowed::compare_utf8, FnInStruct)]
        #[diplomat::rust_link(icu::collator::CollatorBorrowed::compare, FnInStruct, hidden)]
        #[diplomat::attr(not(supports = utf8_strings), disable)]
        #[diplomat::attr(supports = utf8_strings, rename = "compare")]
        pub fn compare_utf8(&self, left: &DiplomatStr, right: &DiplomatStr) -> core::cmp::Ordering {
            self.0.as_borrowed().compare_utf8(left, right)
        }

        /// Compare two strings.
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(icu::collator::CollatorBorrowed::compare_utf16, FnInStruct)]
        #[diplomat::attr(not(supports = utf8_strings), rename = "compare")]
        #[diplomat::attr(supports = utf8_strings, rename = "compare16")]
        pub fn compare_utf16(
            &self,
            left: &DiplomatStr16,
            right: &DiplomatStr16,
        ) -> core::cmp::Ordering {
            self.0.as_borrowed().compare_utf16(left, right)
        }

        /// The resolved options showing how the default options, the requested options,
        /// and the options from locale data were combined. None of the struct fields
        /// will have `Auto` as the value.
        #[diplomat::rust_link(icu::collator::CollatorBorrowed::resolved_options, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        #[diplomat::attr(supports = non_exhaustive_structs, rename = "resolved_options")]
        pub fn resolved_options_v1(&self) -> CollatorResolvedOptionsV1 {
            self.0.as_borrowed().resolved_options().into()
        }
    }
}

impl From<ffi::CollatorOptionsV1> for icu_collator::CollatorOptions {
    fn from(options: ffi::CollatorOptionsV1) -> icu_collator::CollatorOptions {
        let mut result = icu_collator::CollatorOptions::default();
        result.strength = options.strength.into_converted_option();
        result.alternate_handling = options.alternate_handling.into_converted_option();
        result.case_first = options.case_first.into_converted_option();
        result.max_variable = options.max_variable.into_converted_option();
        result.case_level = options.case_level.into_converted_option();
        result.numeric = options.numeric.into_converted_option();
        result.backward_second_level = options.backward_second_level.into_converted_option();

        result
    }
}

impl From<icu_collator::ResolvedCollatorOptions> for ffi::CollatorResolvedOptionsV1 {
    fn from(options: icu_collator::ResolvedCollatorOptions) -> ffi::CollatorResolvedOptionsV1 {
        Self {
            strength: options.strength.into(),
            alternate_handling: options.alternate_handling.into(),
            case_first: options.case_first.into(),
            max_variable: options.max_variable.into(),
            case_level: options.case_level.into(),
            numeric: options.numeric.into(),
            backward_second_level: options.backward_second_level.into(),
        }
    }
}
