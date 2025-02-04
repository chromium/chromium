// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    use crate::errors::ffi::DataError;
    use crate::errors::ffi::FixedDecimalParseError;
    use crate::locale_core::ffi::Locale;
    use crate::provider::ffi::DataProvider;

    #[diplomat::rust_link(icu::plurals::PluralCategory, Enum)]
    #[diplomat::enum_convert(icu_plurals::PluralCategory)]
    pub enum PluralCategory {
        Zero,
        One,
        Two,
        Few,
        Many,
        Other,
    }

    impl PluralCategory {
        /// Construct from a string in the format
        /// [specified in TR35](https://unicode.org/reports/tr35/tr35-numbers.html#Language_Plural_Rules)
        #[diplomat::rust_link(icu::plurals::PluralCategory::get_for_cldr_string, FnInEnum)]
        #[diplomat::rust_link(icu::plurals::PluralCategory::get_for_cldr_bytes, FnInEnum)]
        pub fn get_for_cldr_string(s: &DiplomatStr) -> Option<PluralCategory> {
            icu_plurals::PluralCategory::get_for_cldr_bytes(s).map(Into::into)
        }
    }

    #[diplomat::rust_link(icu::plurals::PluralRules, Struct)]
    #[diplomat::opaque]
    pub struct PluralRules(icu_plurals::PluralRules);

    impl PluralRules {
        /// Construct an [`PluralRules`] for the given locale, for cardinal numbers
        #[diplomat::rust_link(icu::plurals::PluralRules::try_new_cardinal, FnInStruct)]
        #[diplomat::rust_link(icu::plurals::PluralRules::try_new, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::plurals::PluralRuleType, Enum, hidden)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "cardinal")]
        pub fn create_cardinal(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<PluralRules>, DataError> {
            let prefs = icu_plurals::PluralRulesPreferences::from(&locale.0);
            Ok(Box::new(PluralRules(call_constructor!(
                icu_plurals::PluralRules::try_new_cardinal,
                icu_plurals::PluralRules::try_new_cardinal_with_any_provider,
                icu_plurals::PluralRules::try_new_cardinal_with_buffer_provider,
                provider,
                prefs
            )?)))
        }

        /// Construct an [`PluralRules`] for the given locale, for ordinal numbers
        #[diplomat::rust_link(icu::plurals::PluralRules::try_new_ordinal, FnInStruct)]
        #[diplomat::rust_link(icu::plurals::PluralRules::try_new, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::plurals::PluralRuleType, Enum, hidden)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "ordinal")]
        pub fn create_ordinal(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<PluralRules>, DataError> {
            let prefs = icu_plurals::PluralRulesPreferences::from(&locale.0);
            Ok(Box::new(PluralRules(call_constructor!(
                icu_plurals::PluralRules::try_new_ordinal,
                icu_plurals::PluralRules::try_new_ordinal_with_any_provider,
                icu_plurals::PluralRules::try_new_ordinal_with_buffer_provider,
                provider,
                prefs
            )?)))
        }

        /// Get the category for a given number represented as operands
        #[diplomat::rust_link(icu::plurals::PluralRules::category_for, FnInStruct)]
        pub fn category_for(&self, op: &PluralOperands) -> PluralCategory {
            self.0.category_for(op.0).into()
        }

        /// Get all of the categories needed in the current locale
        #[diplomat::rust_link(icu::plurals::PluralRules::categories, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn categories(&self) -> PluralCategories {
            PluralCategories::from_iter(self.0.categories())
        }
    }

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::plurals::PluralOperands, Struct)]
    pub struct PluralOperands(pub icu_plurals::PluralOperands);

    impl PluralOperands {
        /// Construct for a given string representing a number
        #[diplomat::rust_link(icu::plurals::PluralOperands::from_str, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        pub fn from_string(s: &DiplomatStr) -> Result<Box<PluralOperands>, FixedDecimalParseError> {
            Ok(Box::new(PluralOperands(icu_plurals::PluralOperands::from(
                &fixed_decimal::FixedDecimal::try_from_utf8(s)?,
            ))))
        }

        /// Construct from a FixedDecimal
        ///
        /// Retains at most 18 digits each from the integer and fraction parts.
        #[cfg(feature = "decimal")]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        pub fn from_fixed_decimal(x: &crate::fixed_decimal::ffi::FixedDecimal) -> Box<Self> {
            Box::new(Self((&x.0).into()))
        }
    }

    #[diplomat::out]
    pub struct PluralCategories {
        pub zero: bool,
        pub one: bool,
        pub two: bool,
        pub few: bool,
        pub many: bool,
        pub other: bool,
    }

    impl PluralCategories {
        fn from_iter(i: impl Iterator<Item = icu_plurals::PluralCategory>) -> Self {
            i.fold(
                PluralCategories {
                    zero: false,
                    one: false,
                    two: false,
                    few: false,
                    many: false,
                    other: false,
                },
                |mut categories, category| {
                    match category {
                        icu_plurals::PluralCategory::Zero => categories.zero = true,
                        icu_plurals::PluralCategory::One => categories.one = true,
                        icu_plurals::PluralCategory::Two => categories.two = true,
                        icu_plurals::PluralCategory::Few => categories.few = true,
                        icu_plurals::PluralCategory::Many => categories.many = true,
                        icu_plurals::PluralCategory::Other => categories.other = true,
                    };
                    categories
                },
            )
        }
    }
}
