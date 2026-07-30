// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;

    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use crate::unstable::locale_core::ffi::Locale;
    #[cfg(feature = "buffer_provider")]
    use crate::unstable::provider::ffi::DataProvider;
    use crate::unstable::{errors::ffi::DataError, fixed_decimal::ffi::Decimal};
    use icu_decimal::options::DecimalFormatterOptions;
    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use icu_decimal::DecimalFormatterPreferences;

    use writeable::Writeable;

    #[diplomat::opaque]
    /// An ICU4X Decimal Format object, capable of formatting a [`Decimal`] as a string.
    #[diplomat::rust_link(icu::decimal::DecimalFormatter, Struct)]
    #[diplomat::rust_link(icu::decimal::FormattedDecimal, Struct, hidden)]
    pub struct DecimalFormatter(pub icu_decimal::DecimalFormatter);

    #[diplomat::rust_link(icu::decimal::options::GroupingStrategy, Enum)]
    #[diplomat::enum_convert(icu_decimal::options::GroupingStrategy, needs_wildcard)]
    #[non_exhaustive]
    pub enum DecimalGroupingStrategy {
        #[diplomat::attr(auto, default)]
        Auto,
        Never,
        Always,
        Min2,
    }

    impl DecimalFormatter {
        /// Creates a new [`DecimalFormatter`], using compiled data
        #[diplomat::rust_link(icu::decimal::DecimalFormatter::try_new, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_grouping_strategy")]
        #[diplomat::demo(default_constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create_with_grouping_strategy(
            locale: &Locale,
            grouping_strategy: Option<DecimalGroupingStrategy>,
        ) -> Result<Box<DecimalFormatter>, DataError> {
            let prefs = DecimalFormatterPreferences::from(&locale.0);

            let mut options = DecimalFormatterOptions::default();
            options.grouping_strategy = grouping_strategy.map(Into::into);
            Ok(Box::new(DecimalFormatter(
                icu_decimal::DecimalFormatter::try_new(prefs, options)?,
            )))
        }

        /// Creates a new [`DecimalFormatter`], using a particular data source.
        #[diplomat::rust_link(icu::decimal::DecimalFormatter::try_new, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_grouping_strategy_and_provider")]
        #[diplomat::demo(default_constructor)]
        #[cfg(feature = "buffer_provider")]
        pub fn create_with_grouping_strategy_and_provider(
            provider: &DataProvider,
            locale: &Locale,
            grouping_strategy: Option<DecimalGroupingStrategy>,
        ) -> Result<Box<DecimalFormatter>, DataError> {
            let prefs = DecimalFormatterPreferences::from(&locale.0);

            let mut options = DecimalFormatterOptions::default();
            options.grouping_strategy = grouping_strategy.map(Into::into);
            Ok(Box::new(DecimalFormatter(
                icu_decimal::DecimalFormatter::try_new_with_buffer_provider(
                    provider.get()?,
                    prefs,
                    options,
                )?,
            )))
        }

        /// Creates a new [`DecimalFormatter`] from preconstructed locale data.
        #[diplomat::rust_link(icu::decimal::provider::DecimalSymbolsV1, Struct)]
        #[expect(clippy::too_many_arguments)]
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
            grouping_strategy: Option<DecimalGroupingStrategy>,
        ) -> Result<Box<DecimalFormatter>, DataError> {
            use core::cell::RefCell;
            use icu_provider::prelude::*;
            use zerovec::VarZeroCow;

            fn str_to_cow(s: &'_ diplomat_runtime::DiplomatStr) -> VarZeroCow<'_, str> {
                if let Ok(s) = core::str::from_utf8(s) {
                    VarZeroCow::new_borrowed(s)
                } else {
                    VarZeroCow::new_owned(
                        alloc::string::String::from_utf8_lossy(s)
                            .into_owned()
                            .into_boxed_str(),
                    )
                }
            }

            use icu_decimal::provider::{
                DecimalDigitsV1, DecimalSymbolStrsBuilder, DecimalSymbols, DecimalSymbolsV1,
                GroupingSizes,
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

            let grouping_sizes = GroupingSizes {
                primary: primary_group_size,
                secondary: secondary_group_size,
                min_grouping: min_group_size,
            };

            let mut options = DecimalFormatterOptions::default();
            options.grouping_strategy = grouping_strategy.map(Into::into);

            struct Provider(RefCell<Option<DecimalSymbols<'static>>>, [char; 10]);
            impl DataProvider<DecimalSymbolsV1> for Provider {
                fn load(
                    &self,
                    _req: icu_provider::DataRequest,
                ) -> Result<icu_provider::DataResponse<DecimalSymbolsV1>, icu_provider::DataError>
                {
                    Ok(DataResponse {
                        metadata: Default::default(),
                        payload: DataPayload::from_owned(
                            self.0
                                .borrow_mut()
                                .take()
                                // We only have one payload
                                .ok_or(DataErrorKind::Custom.into_error())?,
                        ),
                    })
                }
            }
            impl DataProvider<DecimalDigitsV1> for Provider {
                fn load(
                    &self,
                    _req: icu_provider::DataRequest,
                ) -> Result<icu_provider::DataResponse<DecimalDigitsV1>, icu_provider::DataError>
                {
                    Ok(DataResponse {
                        metadata: Default::default(),
                        payload: DataPayload::from_owned(self.1),
                    })
                }
            }
            let provider = Provider(
                RefCell::new(Some(DecimalSymbols {
                    strings: VarZeroCow::from_encodeable(&strings),
                    grouping_sizes,
                })),
                digits,
            );
            Ok(Box::new(DecimalFormatter(
                icu_decimal::DecimalFormatter::try_new_unstable(
                    &provider,
                    Default::default(),
                    options,
                )?,
            )))
        }

        /// Formats a [`Decimal`] to a string.
        #[diplomat::rust_link(icu::decimal::DecimalFormatter::format, FnInStruct)]
        #[diplomat::rust_link(icu::decimal::DecimalFormatter::format_to_string, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::decimal::FormattedDecimal, Struct, hidden)]
        #[diplomat::rust_link(icu::decimal::FormattedDecimal::to_string, FnInStruct, hidden)]
        pub fn format(&self, value: &Decimal, write: &mut diplomat_runtime::DiplomatWrite) {
            let _infallible = self.0.format(&value.0).write_to(write);
        }
    }
}
