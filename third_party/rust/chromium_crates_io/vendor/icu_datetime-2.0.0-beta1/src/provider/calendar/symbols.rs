// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// allowed for providers
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

use crate::size_test_macro::size_test;
use alloc::borrow::Cow;
use icu_calendar::types::MonthCode;
use icu_provider::prelude::*;
use potential_utf::PotentialUtf8;
use tinystr::{tinystr, TinyStr4};
use zerovec::ZeroMap;

size_test!(DateSymbolsV1, date_symbols_v1_size, 3792);

/// Symbol data for the months, weekdays, and eras needed to format a date.
///
/// For more information on date time symbols, see [`FieldSymbol`](crate::fields::FieldSymbol).
#[doc = date_symbols_v1_size!()]
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(
    marker(BuddhistDateSymbolsV1Marker, "datetime/buddhist/datesymbols@1"),
    marker(ChineseDateSymbolsV1Marker, "datetime/chinese/datesymbols@1"),
    marker(CopticDateSymbolsV1Marker, "datetime/coptic/datesymbols@1"),
    marker(DangiDateSymbolsV1Marker, "datetime/dangi/datesymbols@1"),
    marker(EthiopianDateSymbolsV1Marker, "datetime/ethiopic/datesymbols@1"),
    marker(GregorianDateSymbolsV1Marker, "datetime/gregory/datesymbols@1"),
    marker(HebrewDateSymbolsV1Marker, "datetime/hebrew/datesymbols@1"),
    marker(IndianDateSymbolsV1Marker, "datetime/indian/datesymbols@1"),
    marker(IslamicDateSymbolsV1Marker, "datetime/islamic/datesymbols@1"),
    marker(JapaneseDateSymbolsV1Marker, "datetime/japanese/datesymbols@1"),
    marker(JapaneseExtendedDateSymbolsV1Marker, "datetime/japanext/datesymbols@1"),
    marker(PersianDateSymbolsV1Marker, "datetime/persian/datesymbols@1"),
    marker(RocDateSymbolsV1Marker, "datetime/roc/datesymbols@1")
)]
#[derive(Debug, PartialEq, Clone, Default)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::calendar))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct DateSymbolsV1<'data> {
    /// Symbol data for months.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub months: months::ContextsV1<'data>,
    /// Symbol data for weekdays.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub weekdays: weekdays::ContextsV1<'data>,
    /// Symbol data for eras.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub eras: Eras<'data>,
}

size_test!(TimeSymbolsV1, time_symbols_v1_size, 768);

/// Symbol data for the day periods needed to format a time.
///
/// For more information on date time symbols, see [`FieldSymbol`](crate::fields::FieldSymbol).
#[doc = time_symbols_v1_size!()]
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(marker(TimeSymbolsV1Marker, "datetime/timesymbols@1",))]
#[derive(Debug, PartialEq, Clone, Default)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::calendar))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct TimeSymbolsV1<'data> {
    /// Symbol data for day periods.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub day_periods: day_periods::ContextsV1<'data>,
}

/// String data for the name, abbreviation, and narrow form of a date's era.
///
/// Keys of the map represent era codes, and the values are the display names.
///
/// Era codes are derived from CLDR data, and are calendar specific.
/// Some examples include: `"be"`, `"0"` / `"1"`, `"bce"` / `"ce"`,
/// `"heisei"` / `"meiji"` / `"reiwa"` / ...  Not all era codes are inherited as-is,
/// such as for the extended Japanese calendar.
///
/// For more information on date time symbols, see [`FieldSymbol`](crate::fields::FieldSymbol).
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, PartialEq, Clone, Default, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::calendar))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct Eras<'data> {
    /// Symbol data for era names.
    ///
    /// Keys are era codes, and values are display names. See [`Eras`].
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub names: ZeroMap<'data, PotentialUtf8, str>,
    /// Symbol data for era abbreviations.
    ///
    /// Keys are era codes, and values are display names. See [`Eras`].
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub abbr: ZeroMap<'data, PotentialUtf8, str>,
    /// Symbol data for era narrow forms.
    ///
    /// Keys are era codes, and values are display names. See [`Eras`].
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub narrow: ZeroMap<'data, PotentialUtf8, str>,
}

// Note: the SymbolsV* struct doc strings metadata are attached to `$name` in the macro invocation to
// avoid macro parsing ambiguity caused by other metadata already attached to `$symbols`.
macro_rules! symbols {
    ($(#[$symbols_attr:meta])*  $name: ident, $field_id: ident, $symbols: item) => {

        $(#[$symbols_attr])*
        #[doc = concat!("Formatting symbols for [`",
                stringify!($field_id),
                "`](crate::fields::FieldSymbol::",
                stringify!($field_id),
                ").\n\n",
                "For more information on date time symbols, see [`FieldSymbol`](crate::fields::FieldSymbol).")]
        pub mod $name {
            use super::*;

            #[derive(Debug, PartialEq, Clone, zerofrom::ZeroFrom, yoke::Yokeable)]
            #[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
            #[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::calendar::$name))]
            #[cfg_attr(feature = "serde", derive(serde::Deserialize))]
            #[yoke(prove_covariance_manually)]
            #[doc = concat!("Locale data for ", stringify!($field_id), " corresponding to the symbols.")]
            ///
            /// <div class="stab unstable">
            /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
            /// including in SemVer minor releases. While the serde representation of data structs is guaranteed
            /// to be stable, their Rust representation might not be. Use with caution.
            /// </div>
            $symbols

            // UTS 35 specifies that `format` widths are mandatory,
            // except for `short`.
            #[derive(Debug, PartialEq, Clone, Default, yoke::Yokeable, zerofrom::ZeroFrom)]
            #[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
            #[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::calendar::$name))]
            #[cfg_attr(feature = "serde", derive(serde::Deserialize))]
            #[yoke(prove_covariance_manually)]
            #[doc = concat!("Symbol data for the \"format\" style formatting of ", stringify!($field_id),
                ".\n\nThe format style is used in contexts where it is different from the stand-alone form, ex: ",
                "a case inflected form where the stand-alone form is the nominative case.")]
            ///
            /// <div class="stab unstable">
            /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
            /// including in SemVer minor releases. While the serde representation of data structs is guaranteed
            /// to be stable, their Rust representation might not be. Use with caution.
            /// </div>
            pub struct FormatWidthsV1<'data> {
                #[doc = concat!("Abbreviated length symbol for \"format\" style symbol for ", stringify!($name), ".")]
                #[cfg_attr(feature = "serde", serde(borrow))]
                pub abbreviated: SymbolsV1<'data>,
                #[doc = concat!("Narrow length symbol for \"format\" style symbol for ", stringify!($name), ".")]
                #[cfg_attr(feature = "serde", serde(borrow))]
                pub narrow: SymbolsV1<'data>,
                #[doc = concat!("Short length symbol for \"format\" style symbol for ", stringify!($name), ", if present.")]
                #[cfg_attr(feature = "serde", serde(borrow))]
                pub short: Option<SymbolsV1<'data>>,
                #[doc = concat!("Wide length symbol for \"format\" style symbol for ", stringify!($name), ".")]
                #[cfg_attr(feature = "serde", serde(borrow))]
                pub wide: SymbolsV1<'data>,
            }

            // UTS 35 specifies that `stand_alone` widths are optional
            #[derive(Debug, PartialEq, Clone, Default, yoke::Yokeable, zerofrom::ZeroFrom)]
            #[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
            #[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::calendar::$name))]
            #[cfg_attr(feature = "serde", derive(serde::Deserialize))]
            #[yoke(prove_covariance_manually)]
            #[doc = concat!("Symbol data for the \"stand-alone\" style formatting of ", stringify!($field_id),
                ".\n\nThe stand-alone style is used in contexts where the field is displayed by itself.")]
            ///
            /// <div class="stab unstable">
            /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
            /// including in SemVer minor releases. While the serde representation of data structs is guaranteed
            /// to be stable, their Rust representation might not be. Use with caution.
            /// </div>
            pub struct StandAloneWidthsV1<'data> {
                #[doc = concat!("Abbreviated length symbol for \"stand-alone\" style symbol for ", stringify!($name), ".")]
                #[cfg_attr(feature = "serde", serde(borrow))]
                pub abbreviated: Option<SymbolsV1<'data>>,
                #[doc = concat!("Narrow length symbol for \"stand-alone\" style symbol for ", stringify!($name), ".")]
                #[cfg_attr(feature = "serde", serde(borrow))]
                pub narrow: Option<SymbolsV1<'data>>,
                #[doc = concat!("Short length symbol for \"stand-alone\" style symbol for ", stringify!($name), ".")]
                #[cfg_attr(feature = "serde", serde(borrow))]
                pub short: Option<SymbolsV1<'data>>,
                #[doc = concat!("Wide length symbol for \"stand-alone\" style symbol for ", stringify!($name), ".")]
                #[cfg_attr(feature = "serde", serde(borrow))]
                pub wide: Option<SymbolsV1<'data>>,
            }

            #[derive(Debug, PartialEq, Clone, Default, yoke::Yokeable, zerofrom::ZeroFrom)]
            #[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
            #[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::calendar::$name))]
            #[cfg_attr(feature = "serde", derive(serde::Deserialize))]
            #[yoke(prove_covariance_manually)]
            #[doc = concat!("The struct containing the symbol data for ", stringify!($field_id),
                " that contains the \"format\" style symbol data ([`FormatWidthsV1`]) and \"stand-alone\" style symbol data ([`StandAloneWidthsV1`]).")]
            ///
            /// <div class="stab unstable">
            /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
            /// including in SemVer minor releases. While the serde representation of data structs is guaranteed
            /// to be stable, their Rust representation might not be. Use with caution.
            /// </div>
            pub struct ContextsV1<'data> {
                /// The symbol data for "format" style symbols.
                #[cfg_attr(feature = "serde", serde(borrow))]
                pub format: FormatWidthsV1<'data>,
                /// The symbol data for "stand-alone" style symbols.
                #[cfg_attr(feature = "serde", serde(borrow))]
                pub stand_alone: Option<StandAloneWidthsV1<'data>>,
            }

            impl<'data> ContextsV1<'data> {
                /// Convenience function to return stand-alone abbreviated as an `Option<&>`.
                pub(crate) fn stand_alone_abbreviated(&self) -> Option<&SymbolsV1<'data>> {
                    self.stand_alone.as_ref().and_then(|x| x.abbreviated.as_ref())
                }
                /// Convenience function to return stand-alone wide as an `Option<&>`.
                pub(crate) fn stand_alone_wide(&self) -> Option<&SymbolsV1<'data>> {
                    self.stand_alone.as_ref().and_then(|x| x.wide.as_ref())
                }
                /// Convenience function to return stand-alone narrow as an `Option<&>`.
                pub(crate) fn stand_alone_narrow(&self) -> Option<&SymbolsV1<'data>> {
                    self.stand_alone.as_ref().and_then(|x| x.narrow.as_ref())
                }
                /// Convenience function to return stand-alone short as an `Option<&>`.
                #[allow(dead_code)] // not all symbols have a short variant
                pub(crate) fn stand_alone_short(&self) -> Option<&SymbolsV1<'data>> {
                    self.stand_alone.as_ref().and_then(|x| x.short.as_ref())
                }
            }
        }
    };
}

symbols!(
    months,
    Month,
    #[allow(clippy::large_enum_variant)]
    pub enum SymbolsV1<'data> {
        /// Twelve symbols for a solar calendar
        ///
        /// This is an optimization to reduce data size.
        SolarTwelve(
            #[cfg_attr(
                feature = "serde",
                serde(
                    borrow,
                    deserialize_with = "icu_provider::serde_borrow_de_utils::array_of_cow"
                )
            )]
            [Cow<'data, str>; 12],
        ),
        /// A calendar with an arbitrary number of months, potentially including leap months
        #[cfg_attr(feature = "serde", serde(borrow))]
        Other(ZeroMap<'data, MonthCode, str>),
    }
);

impl months::SymbolsV1<'_> {
    /// Get the symbol for the given month code
    pub fn get(&self, code: MonthCode) -> Option<&str> {
        match *self {
            Self::SolarTwelve(ref arr) => {
                // The tinystr macro doesn't work in match patterns
                // so we use consts first
                const CODE_1: TinyStr4 = tinystr!(4, "M01");
                const CODE_2: TinyStr4 = tinystr!(4, "M02");
                const CODE_3: TinyStr4 = tinystr!(4, "M03");
                const CODE_4: TinyStr4 = tinystr!(4, "M04");
                const CODE_5: TinyStr4 = tinystr!(4, "M05");
                const CODE_6: TinyStr4 = tinystr!(4, "M06");
                const CODE_7: TinyStr4 = tinystr!(4, "M07");
                const CODE_8: TinyStr4 = tinystr!(4, "M08");
                const CODE_9: TinyStr4 = tinystr!(4, "M09");
                const CODE_10: TinyStr4 = tinystr!(4, "M10");
                const CODE_11: TinyStr4 = tinystr!(4, "M11");
                const CODE_12: TinyStr4 = tinystr!(4, "M12");
                let idx = match code.0 {
                    CODE_1 => 0,
                    CODE_2 => 1,
                    CODE_3 => 2,
                    CODE_4 => 3,
                    CODE_5 => 4,
                    CODE_6 => 5,
                    CODE_7 => 6,
                    CODE_8 => 7,
                    CODE_9 => 8,
                    CODE_10 => 9,
                    CODE_11 => 10,
                    CODE_12 => 11,
                    _ => return None,
                };
                arr.get(idx).map(|x| &**x)
            }
            Self::Other(ref map) => map.get(&code),
        }
    }
}

impl Default for months::SymbolsV1<'_> {
    fn default() -> Self {
        Self::Other(Default::default())
    }
}

symbols!(
    weekdays,
    Weekday,
    #[derive(Default)]
    pub struct SymbolsV1<'data>(
        #[cfg_attr(
            feature = "serde",
            serde(
                borrow,
                deserialize_with = "icu_provider::serde_borrow_de_utils::array_of_cow"
            )
        )]
        pub [Cow<'data, str>; 7],
    );
);

symbols!(
    day_periods,
    DayPeriod,
    #[derive(Default)]
    pub struct SymbolsV1<'data> {
        /// Day period for AM (before noon).
        #[cfg_attr(feature = "serde", serde(borrow))]
        pub am: Cow<'data, str>,
        /// Day period for PM (after noon).
        #[cfg_attr(feature = "serde", serde(borrow))]
        pub pm: Cow<'data, str>,
        #[cfg_attr(
            feature = "serde",
            serde(
                borrow,
                deserialize_with = "icu_provider::serde_borrow_de_utils::option_of_cow"
            )
        )]
        /// Day period for noon, in locales that support it.
        pub noon: Option<Cow<'data, str>>,
        #[cfg_attr(
            feature = "serde",
            serde(
                borrow,
                deserialize_with = "icu_provider::serde_borrow_de_utils::option_of_cow"
            )
        )]
        /// Day period for midnight, in locales that support it.
        pub midnight: Option<Cow<'data, str>>,
    }
);

#[cfg(all(test, feature = "datagen"))]
mod test {
    use super::*;
    use tinystr::tinystr;

    fn serialize_date() -> Vec<u8> {
        let months = [
            (&MonthCode(tinystr!(4, "M01")), "January"),
            (&MonthCode(tinystr!(4, "M02")), "February"),
            (&MonthCode(tinystr!(4, "M03")), "March"),
            (&MonthCode(tinystr!(4, "M04")), "April"),
            (&MonthCode(tinystr!(4, "M05")), "May"),
            (&MonthCode(tinystr!(4, "M06")), "June"),
            (&MonthCode(tinystr!(4, "M07")), "July"),
            (&MonthCode(tinystr!(4, "M08")), "August"),
            (&MonthCode(tinystr!(4, "M09")), "September"),
            (&MonthCode(tinystr!(4, "M10")), "October"),
            (&MonthCode(tinystr!(4, "M11")), "November"),
            (&MonthCode(tinystr!(4, "M12")), "December"),
        ];
        let months = months::SymbolsV1::Other(months.iter().copied().collect());

        let weekdays = weekdays::SymbolsV1([
            Cow::Borrowed("Monday"),
            Cow::Borrowed("Tuesday"),
            Cow::Borrowed("Wednesday"),
            Cow::Borrowed("Thursday"),
            Cow::Borrowed("Friday"),
            Cow::Borrowed("Saturday"),
            Cow::Borrowed("Sunday"),
        ]);

        bincode::serialize(&DateSymbolsV1 {
            months: months::ContextsV1 {
                format: months::FormatWidthsV1 {
                    abbreviated: months.clone(),
                    narrow: months.clone(),
                    short: Some(months.clone()),
                    wide: months.clone(),
                },
                stand_alone: Some(months::StandAloneWidthsV1 {
                    abbreviated: Some(months.clone()),
                    narrow: Some(months.clone()),
                    short: Some(months.clone()),
                    wide: Some(months.clone()),
                }),
            },
            weekdays: weekdays::ContextsV1 {
                format: weekdays::FormatWidthsV1 {
                    abbreviated: weekdays.clone(),
                    narrow: weekdays.clone(),
                    short: Some(weekdays.clone()),
                    wide: weekdays.clone(),
                },
                stand_alone: Some(weekdays::StandAloneWidthsV1 {
                    abbreviated: Some(weekdays.clone()),
                    narrow: Some(weekdays.clone()),
                    short: Some(weekdays.clone()),
                    wide: Some(weekdays.clone()),
                }),
            },
            eras: Eras {
                names: ZeroMap::new(),
                abbr: ZeroMap::new(),
                narrow: ZeroMap::new(),
            },
        })
        .unwrap()
    }

    fn serialize_time() -> Vec<u8> {
        let day_periods = day_periods::SymbolsV1 {
            am: Cow::Borrowed("am"),
            pm: Cow::Borrowed("pm"),
            noon: Some(Cow::Borrowed("noon")),
            midnight: None,
        };

        bincode::serialize(&TimeSymbolsV1 {
            day_periods: day_periods::ContextsV1 {
                format: day_periods::FormatWidthsV1 {
                    abbreviated: day_periods.clone(),
                    narrow: day_periods.clone(),
                    short: Some(day_periods.clone()),
                    wide: day_periods.clone(),
                },
                stand_alone: Some(day_periods::StandAloneWidthsV1 {
                    abbreviated: Some(day_periods.clone()),
                    narrow: Some(day_periods.clone()),
                    short: Some(day_periods.clone()),
                    wide: Some(day_periods.clone()),
                }),
            },
        })
        .unwrap()
    }

    #[test]
    fn weekdays_borrows() {
        let bytes = serialize_date();
        let de = bincode::deserialize::<DateSymbolsV1>(&bytes).unwrap();

        assert!(matches!(de.weekdays.format.narrow.0[2], Cow::Borrowed(_)));
        assert!(matches!(
            de.weekdays.format.short.as_ref().unwrap().0[4],
            Cow::Borrowed(_)
        ));
    }

    #[test]
    fn day_periods_borrows() {
        let bytes = serialize_time();
        let de = bincode::deserialize::<TimeSymbolsV1>(&bytes).unwrap();

        assert!(matches!(
            de.day_periods.format.narrow.noon,
            Some(Cow::Borrowed(_))
        ));
        assert!(matches!(
            de.day_periods.format.short.as_ref().unwrap().noon,
            Some(Cow::Borrowed(_))
        ));

        assert!(matches!(de.day_periods.format.narrow.am, Cow::Borrowed(_)));
        assert!(matches!(
            de.day_periods.format.short.as_ref().unwrap().am,
            Cow::Borrowed(_)
        ));
    }
}
