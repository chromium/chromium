// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;

    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use crate::unstable::errors::ffi::DataError;
    use crate::unstable::errors::ffi::DecimalParseError;
    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use crate::unstable::locale_core::ffi::Locale;
    #[cfg(feature = "buffer_provider")]
    use crate::unstable::provider::ffi::DataProvider;

    #[diplomat::rust_link(icu::plurals::PluralCategory, Enum)]
    #[diplomat::enum_convert(icu_plurals::PluralCategory)]
    #[non_exhaustive]
    pub enum PluralCategory {
        Zero,
        One,
        Two,
        Few,
        Many,
        // This is an output type, so the default mostly impacts deferred initialization.
        #[diplomat::attr(auto, default)]
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
        /// Construct an [`PluralRules`] for the given locale, for cardinal numbers, using compiled data.
        #[diplomat::rust_link(icu::plurals::PluralRules::try_new_cardinal, FnInStruct)]
        #[diplomat::rust_link(icu::plurals::PluralRules::try_new, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::plurals::PluralRuleType, Enum, hidden)]
        #[diplomat::rust_link(icu::plurals::PluralRulesOptions, Struct, hidden)]
        #[diplomat::rust_link(icu::plurals::PluralRulesOptions::default, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::plurals::PluralRulesOptions::with_type, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "cardinal")]
        #[cfg(feature = "compiled_data")]
        #[diplomat::demo(default_constructor)]
        pub fn create_cardinal(locale: &Locale) -> Result<Box<PluralRules>, DataError> {
            let prefs = icu_plurals::PluralRulesPreferences::from(&locale.0);
            Ok(Box::new(PluralRules(
                icu_plurals::PluralRules::try_new_cardinal(prefs)?,
            )))
        }
        /// Construct an [`PluralRules`] for the given locale, for cardinal numbers, using a particular data source.
        #[diplomat::rust_link(icu::plurals::PluralRules::try_new_cardinal, FnInStruct)]
        #[diplomat::rust_link(icu::plurals::PluralRules::try_new, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::plurals::PluralRuleType, Enum, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "cardinal_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_cardinal_with_provider(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<PluralRules>, DataError> {
            let prefs = icu_plurals::PluralRulesPreferences::from(&locale.0);
            Ok(Box::new(PluralRules(
                icu_plurals::PluralRules::try_new_cardinal_with_buffer_provider(
                    provider.get()?,
                    prefs,
                )?,
            )))
        }
        /// Construct an [`PluralRules`] for the given locale, for ordinal numbers, using compiled data.
        #[diplomat::rust_link(icu::plurals::PluralRules::try_new_ordinal, FnInStruct)]
        #[diplomat::rust_link(icu::plurals::PluralRules::try_new, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::plurals::PluralRuleType, Enum, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "ordinal")]
        #[cfg(feature = "compiled_data")]
        pub fn create_ordinal(locale: &Locale) -> Result<Box<PluralRules>, DataError> {
            let prefs = icu_plurals::PluralRulesPreferences::from(&locale.0);
            Ok(Box::new(PluralRules(
                icu_plurals::PluralRules::try_new_ordinal(prefs)?,
            )))
        }
        /// Construct an [`PluralRules`] for the given locale, for ordinal numbers, using a particular data source.
        #[diplomat::rust_link(icu::plurals::PluralRules::try_new_ordinal, FnInStruct)]
        #[diplomat::rust_link(icu::plurals::PluralRules::try_new, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::plurals::PluralRuleType, Enum, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "ordinal_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_ordinal_with_provider(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<PluralRules>, DataError> {
            let prefs = icu_plurals::PluralRulesPreferences::from(&locale.0);
            Ok(Box::new(PluralRules(
                icu_plurals::PluralRules::try_new_ordinal_with_buffer_provider(
                    provider.get()?,
                    prefs,
                )?,
            )))
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
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn from_string(s: &DiplomatStr) -> Result<Box<PluralOperands>, DecimalParseError> {
            Ok(Box::new(PluralOperands(icu_plurals::PluralOperands::from(
                &fixed_decimal::Decimal::try_from_utf8(s)?,
            ))))
        }

        /// Construct for a given integer
        #[diplomat::attr(auto, named_constructor)]
        #[diplomat::attr(dart, rename = "from_int")]
        #[diplomat::attr(js, rename = "from_big_int")]
        #[diplomat::attr(supports = method_overloading, rename = "from")]
        pub fn from_int64(i: i64) -> Box<PluralOperands> {
            Box::new(PluralOperands(icu_plurals::PluralOperands::from(i)))
        }

        /// Construct from a `FixedDecimal`
        ///
        /// Retains at most 18 digits each from the integer and fraction parts.
        #[cfg(feature = "decimal")]
        #[diplomat::attr(auto, named_constructor)]
        #[diplomat::demo(default_constructor)]
        pub fn from_fixed_decimal(x: &crate::unstable::fixed_decimal::ffi::Decimal) -> Box<Self> {
            Box::new(Self((&x.0).into()))
        }
    }

    #[diplomat::out]
    #[diplomat::rust_link(icu::plurals::PluralRules::categories, FnInStruct)]
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

    #[diplomat::rust_link(icu::plurals::PluralRulesWithRanges, Struct)]
    #[diplomat::opaque]
    #[cfg(feature = "unstable")]
    pub struct PluralRulesWithRanges(icu_plurals::PluralRulesWithRanges<icu_plurals::PluralRules>);

    #[cfg(feature = "unstable")]
    impl PluralRulesWithRanges {
        /// construct a [`PluralRulesWithRanges`] for the given locale, for cardinal numbers, using compiled data.
        #[diplomat::rust_link(icu::plurals::PluralRulesWithRanges::try_new_cardinal, FnInStruct)]
        #[diplomat::rust_link(icu::plurals::PluralRulesWithRanges::try_new, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "cardinal")]
        #[cfg(feature = "compiled_data")]
        #[diplomat::demo(default_constructor)]
        pub fn create_cardinal(locale: &Locale) -> Result<Box<PluralRulesWithRanges>, DataError> {
            let prefs = icu_plurals::PluralRulesPreferences::from(&locale.0);
            Ok(Box::new(PluralRulesWithRanges(
                icu_plurals::PluralRulesWithRanges::try_new_cardinal(prefs)?,
            )))
        }

        /// construct a [`PluralRulesWithRanges`] for the given locale, for cardinal numbers, using a particular data source.
        #[diplomat::rust_link(icu::plurals::PluralRulesWithRanges::try_new_cardinal, FnInStruct)]
        #[diplomat::rust_link(icu::plurals::PluralRulesWithRanges::try_new, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "cardinal_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_cardinal_with_provider(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<PluralRulesWithRanges>, DataError> {
            let prefs = icu_plurals::PluralRulesPreferences::from(&locale.0);
            Ok(Box::new(PluralRulesWithRanges(
                icu_plurals::PluralRulesWithRanges::try_new_cardinal_with_buffer_provider(
                    provider.get()?,
                    prefs,
                )?,
            )))
        }

        /// Construct a [`PluralRulesWithRanges`] for the given locale, for ordinal numbers, using compiled data.
        #[diplomat::rust_link(icu::plurals::PluralRulesWithRanges::try_new_ordinal, FnInStruct)]
        #[diplomat::rust_link(icu::plurals::PluralRulesWithRanges::try_new, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "ordinal")]
        #[cfg(feature = "compiled_data")]
        pub fn create_ordinal(locale: &Locale) -> Result<Box<PluralRulesWithRanges>, DataError> {
            let prefs = icu_plurals::PluralRulesPreferences::from(&locale.0);
            Ok(Box::new(PluralRulesWithRanges(
                icu_plurals::PluralRulesWithRanges::try_new_ordinal(prefs)?,
            )))
        }

        /// Construct a [`PluralRulesWithRanges`] for the given locale, for ordinal numbers, using a particular data source.
        #[diplomat::rust_link(icu::plurals::PluralRulesWithRanges::try_new_ordinal, FnInStruct)]
        #[diplomat::rust_link(icu::plurals::PluralRulesWithRanges::try_new, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "ordinal_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_ordinal_with_provider(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<PluralRulesWithRanges>, DataError> {
            let prefs = icu_plurals::PluralRulesPreferences::from(&locale.0);
            Ok(Box::new(PluralRulesWithRanges(
                icu_plurals::PluralRulesWithRanges::try_new_ordinal_with_buffer_provider(
                    provider.get()?,
                    prefs,
                )?,
            )))
        }

        /// Get the category for a given number represented as operands
        #[diplomat::rust_link(icu::plurals::PluralRulesWithRanges::category_for_range, FnInStruct)]
        pub fn category_for_range(
            &self,
            start: &PluralOperands,
            end: &PluralOperands,
        ) -> PluralCategory {
            self.0.category_for_range(start.0, end.0).into()
        }
    }
}
