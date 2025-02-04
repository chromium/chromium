// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    use crate::{
        errors::ffi::DataError, fixed_decimal::ffi::FixedDecimal, locale_core::ffi::Locale,
        provider::ffi::DataProvider,
    };
    use icu_decimal::{options::FixedDecimalFormatterOptions, FixedDecimalFormatterPreferences};

    use writeable::Writeable;

    #[diplomat::opaque]
    /// An ICU4X Fixed Decimal Format object, capable of formatting a [`FixedDecimal`] as a string.
    #[diplomat::rust_link(icu::decimal::FixedDecimalFormatter, Struct)]
    #[diplomat::rust_link(icu::datetime::FormattedFixedDecimal, Struct, hidden)]
    pub struct FixedDecimalFormatter(pub icu_decimal::FixedDecimalFormatter);

    #[diplomat::rust_link(icu::decimal::options::GroupingStrategy, Enum)]
    #[diplomat::enum_convert(icu_decimal::options::GroupingStrategy, needs_wildcard)]
    pub enum FixedDecimalGroupingStrategy {
        Auto,
        Never,
        Always,
        Min2,
    }

    impl FixedDecimalFormatter {
        /// Creates a new [`FixedDecimalFormatter`] from locale data.
        #[diplomat::rust_link(icu::decimal::FixedDecimalFormatter::try_new, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "with_grouping_strategy")]
        #[diplomat::demo(default_constructor)]
        pub fn create_with_grouping_strategy(
            provider: &DataProvider,
            locale: &Locale,
            grouping_strategy: Option<FixedDecimalGroupingStrategy>,
        ) -> Result<Box<FixedDecimalFormatter>, DataError> {
            let prefs = FixedDecimalFormatterPreferences::from(&locale.0);

            let mut options = FixedDecimalFormatterOptions::default();
            options.grouping_strategy = grouping_strategy
                .map(Into::into)
                .unwrap_or(options.grouping_strategy);
            Ok(Box::new(FixedDecimalFormatter(call_constructor!(
                icu_decimal::FixedDecimalFormatter::try_new,
                icu_decimal::FixedDecimalFormatter::try_new_with_any_provider,
                icu_decimal::FixedDecimalFormatter::try_new_with_buffer_provider,
                provider,
                prefs,
                options,
            )?)))
        }

        /// Creates a new [`FixedDecimalFormatter`] from preconstructed locale data.
        #[diplomat::rust_link(icu::decimal::provider::DecimalSymbolsV2, Struct)]
        #[allow(clippy::too_many_arguments)]
        pub fn create_with_manual_data(
            plus_sign_prefix: &DiplomatStr,
            plus_sign_suffix: &DiplomatStr,
            minus_sign_prefix: &DiplomatStr,
            minus_sign_suffix: &DiplomatStr,
            decimal_separator: &DiplomatStr,
            grouping_separator: &DiplomatStr,
            primary_group_size: u8,
            secondary_group_size: u8,
            min_group_size: u8,
            digits: &[DiplomatChar],
            grouping_strategy: Option<FixedDecimalGroupingStrategy>,
        ) -> Result<Box<FixedDecimalFormatter>, DataError> {
            use alloc::borrow::Cow;
            use icu_provider::any::AsDowncastingAnyProvider;
            use icu_provider_adapters::{fixed::FixedProvider, fork::ForkByMarkerProvider};
            use zerovec::VarZeroCow;

            fn str_to_cow(s: &'_ diplomat_runtime::DiplomatStr) -> Cow<'_, str> {
                if s.is_empty() {
                    Cow::default()
                } else if let Ok(s) = core::str::from_utf8(s) {
                    Cow::Borrowed(s)
                } else {
                    Cow::Owned(alloc::string::String::from_utf8_lossy(s).into_owned())
                }
            }

            use icu_decimal::provider::{
                DecimalDigitsV1, DecimalDigitsV1Marker, DecimalSymbolStrsBuilder, DecimalSymbolsV2,
                DecimalSymbolsV2Marker, GroupingSizesV1,
            };
            let mut new_digits = ['\0'; 10];
            for (old, new) in digits
                .iter()
                .copied()
                .chain(core::iter::repeat(char::REPLACEMENT_CHARACTER as u32))
                .zip(new_digits.iter_mut())
            {
                *new = char::from_u32(old).unwrap_or(char::REPLACEMENT_CHARACTER);
            }
            let digits = new_digits;
            let strings = DecimalSymbolStrsBuilder {
                plus_sign_prefix: str_to_cow(plus_sign_prefix),
                plus_sign_suffix: str_to_cow(plus_sign_suffix),
                minus_sign_prefix: str_to_cow(minus_sign_prefix),
                minus_sign_suffix: str_to_cow(minus_sign_suffix),
                decimal_separator: str_to_cow(decimal_separator),
                grouping_separator: str_to_cow(grouping_separator),
                numsys: "zyyy".into(),
            };

            let grouping_sizes = GroupingSizesV1 {
                primary: primary_group_size,
                secondary: secondary_group_size,
                min_grouping: min_group_size,
            };

            let mut options = FixedDecimalFormatterOptions::default();
            options.grouping_strategy = grouping_strategy
                .map(Into::into)
                .unwrap_or(options.grouping_strategy);
            let provider_symbols =
                FixedProvider::<DecimalSymbolsV2Marker>::from_owned(DecimalSymbolsV2 {
                    strings: VarZeroCow::from_encodeable(&strings),
                    grouping_sizes,
                });
            let provider_digits =
                FixedProvider::<DecimalDigitsV1Marker>::from_owned(DecimalDigitsV1 { digits });
            let provider = ForkByMarkerProvider::new(provider_symbols, provider_digits);
            Ok(Box::new(FixedDecimalFormatter(
                icu_decimal::FixedDecimalFormatter::try_new_unstable(
                    &provider.as_downcasting(),
                    Default::default(),
                    options,
                )?,
            )))
        }

        /// Formats a [`FixedDecimal`] to a string.
        #[diplomat::rust_link(icu::decimal::FixedDecimalFormatter::format, FnInStruct)]
        #[diplomat::rust_link(
            icu::decimal::FixedDecimalFormatter::format_to_string,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(icu::decimal::FormattedFixedDecimal, Struct, hidden)]
        #[diplomat::rust_link(icu::decimal::FormattedFixedDecimal::write_to, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::decimal::FormattedFixedDecimal::to_string, FnInStruct, hidden)]
        pub fn format(&self, value: &FixedDecimal, write: &mut diplomat_runtime::DiplomatWrite) {
            let _infallible = self.0.format(&value.0).write_to(write);
        }
    }
}
