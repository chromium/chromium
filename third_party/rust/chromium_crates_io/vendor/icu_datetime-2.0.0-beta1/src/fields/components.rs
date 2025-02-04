// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! 🚧 \[Experimental\] Types for specifying fields in a classical datetime skeleton.
//!
//! <div class="stab unstable">
//! 🚧 This code is experimental; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. It can be enabled with the `experimental` Cargo feature
//! of the icu meta-crate. Use with caution.
//! <a href="https://github.com/unicode-org/icu4x/issues/1317">#1317</a>
//! </div>
//!
//! # Examples
//!
//! ```
//! use icu::datetime::fields::components;
//!
//! let mut bag = components::Bag::default();
//! bag.year = Some(components::Year::Numeric);
//! bag.month = Some(components::Month::Long);
//! bag.day = Some(components::Day::NumericDayOfMonth);
//!
//! bag.hour = Some(components::Numeric::TwoDigit);
//! bag.minute = Some(components::Numeric::TwoDigit);
//! ```
//!
//! *Note*: The exact formatted result is a subject to change over
//! time. Formatted result should be treated as opaque and displayed to the user as-is,
//! and it is strongly recommended to never write tests that expect a particular formatted output.

use crate::{
    fields::{self, Field, FieldLength, FieldSymbol},
    options::FractionalSecondDigits,
    provider::pattern::{runtime::Pattern, PatternItem},
};

use crate::pattern::DateTimePattern;
use icu_locale_core::preferences::extensions::unicode::keywords::HourCycle;
#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};

/// See the [module-level](./index.html) docs for more information.
///
/// <div class="stab unstable">
/// 🚧 This code is experimental; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. It can be enabled with the `experimental` Cargo feature
/// of the icu meta-crate. Use with caution.
/// <a href="https://github.com/unicode-org/icu4x/issues/1317">#1317</a>
/// </div>
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default, Hash)]
#[non_exhaustive]
pub struct Bag {
    /// Include the era, such as "AD" or "CE".
    pub era: Option<Text>,
    /// Include the year, such as "1970" or "70".
    pub year: Option<Year>,
    /// Include the month, such as "April" or "Apr".
    pub month: Option<Month>,
    /// Include the week number, such as "51st" or "51" for week 51.
    pub week: Option<Week>,
    /// Include the day of the month/year, such as "07" or "7".
    pub day: Option<Day>,
    /// Include the weekday, such as "Wednesday" or "Wed".
    pub weekday: Option<Text>,

    /// Include the hour such as "2" or "14".
    pub hour: Option<Numeric>,
    /// Include the minute such as "3" or "03".
    pub minute: Option<Numeric>,
    /// Include the second such as "3" or "03".
    pub second: Option<Numeric>,
    /// Specify the number of fractional second digits such as 1 (".3") or 3 (".003").
    pub fractional_second: Option<FractionalSecondDigits>,

    /// Include the time zone, such as "GMT+05:00".
    pub time_zone_name: Option<TimeZoneName>,

    /// An override of the hour cycle.
    pub hour_cycle: Option<HourCycle>,
}

impl Bag {
    /// Creates an empty components bag
    ///
    /// Has the same behavior as the [`Default`] implementation on this type.
    pub fn empty() -> Self {
        Self::default()
    }

    /// Merges the fields of other into self if non-None. If both fields are set, `other` is kept.
    pub fn merge(self, other: Self) -> Self {
        Self {
            era: other.era.or(self.era),
            year: other.year.or(self.year),
            month: other.month.or(self.month),
            week: other.week.or(self.week),
            day: other.day.or(self.day),
            weekday: other.weekday.or(self.weekday),
            hour: other.hour.or(self.hour),
            minute: other.minute.or(self.minute),
            second: other.second.or(self.second),
            fractional_second: other.fractional_second.or(self.fractional_second),
            time_zone_name: other.time_zone_name.or(self.time_zone_name),
            hour_cycle: other.hour_cycle.or(self.hour_cycle),
        }
    }

    #[allow(clippy::wrong_self_convention)]
    /// Converts the components::Bag into a `Vec<Field>`. The fields will be ordered in from most
    /// significant field to least significant. This is the order the fields are listed in
    /// the UTS 35 table - <https://unicode.org/reports/tr35/tr35-dates.html#Date_Field_Symbol_Table>
    ///
    /// Arguments:
    ///
    /// - `default_hour_cycle` specifies the hour cycle to use for the hour field if not in the Bag.
    ///   `preferences::Bag::hour_cycle` takes precedence over this argument.
    pub fn to_vec_fields(&self, default_hour_cycle: HourCycle) -> alloc::vec::Vec<Field> {
        let mut fields = alloc::vec::Vec::new();
        if let Some(era) = self.era {
            fields.push(Field {
                symbol: FieldSymbol::Era,
                length: match era {
                    // Era name, format length.
                    //
                    // G..GGG  AD           Abbreviated
                    // GGGG    Anno Domini  Wide
                    // GGGGG   A            Narrow
                    Text::Short => FieldLength::Three,
                    Text::Long => FieldLength::Four,
                    Text::Narrow => FieldLength::Five,
                },
            })
        }

        if let Some(year) = self.year {
            // Unimplemented year fields:
            // u - Extended year
            // U - Cyclic year name
            // r - Related Gregorian year
            fields.push(Field {
                symbol: FieldSymbol::Year(match year {
                    Year::Numeric | Year::TwoDigit => fields::Year::Calendar,
                    Year::NumericWeekOf | Year::TwoDigitWeekOf => {
                        unimplemented!("fields::Year::WeekOf")
                    }
                }),
                length: match year {
                    // Calendar year (numeric).
                    // y       2, 20, 201, 2017, 20173
                    // yy      02, 20, 01, 17, 73
                    // yyy     002, 020, 201, 2017, 20173    (not implemented)
                    // yyyy    0002, 0020, 0201, 2017, 20173 (not implemented)
                    // yyyyy+  ...                           (not implemented)
                    Year::Numeric | Year::NumericWeekOf => FieldLength::One,
                    Year::TwoDigit | Year::TwoDigitWeekOf => FieldLength::Two,
                },
            });
        }

        // TODO(#501) - Unimplemented quarter fields:
        // Q - Quarter number/name
        // q - Stand-alone quarter

        if let Some(month) = self.month {
            fields.push(Field {
                // Always choose Month::Format as Month::StandAlone is not used in skeletons.
                symbol: FieldSymbol::Month(fields::Month::Format),
                length: match month {
                    // (intended to be used in conjunction with ‘d’ for day number).
                    // M      9, 12      Numeric: minimum digits
                    // MM     09, 12     Numeric: 2 digits, zero pad if needed
                    // MMM    Sep        Abbreviated
                    // MMMM   September  Wide
                    // MMMMM  S          Narrow
                    Month::Numeric => FieldLength::One,
                    Month::TwoDigit => FieldLength::Two,
                    Month::Long => FieldLength::Four,
                    Month::Short => FieldLength::Three,
                    Month::Narrow => FieldLength::Five,
                },
            });
        }

        if let Some(week) = self.week {
            #[allow(unreachable_code)]
            fields.push(Field {
                symbol: FieldSymbol::Week(match week {
                    Week::WeekOfMonth => unimplemented!("#5643 fields::Week::WeekOfMonth"),
                    Week::NumericWeekOfYear | Week::TwoDigitWeekOfYear => {
                        unimplemented!("#5643 fields::Week::WeekOfYear")
                    }
                }),
                length: match week {
                    Week::WeekOfMonth | Week::NumericWeekOfYear => FieldLength::One,
                    Week::TwoDigitWeekOfYear => FieldLength::Two,
                },
            });
        }

        if let Some(day) = self.day {
            // TODO(#591) Unimplemented day fields:
            // g - Modified Julian day.
            fields.push(Field {
                symbol: FieldSymbol::Day(match day {
                    Day::NumericDayOfMonth | Day::TwoDigitDayOfMonth => fields::Day::DayOfMonth,
                    Day::DayOfWeekInMonth => fields::Day::DayOfWeekInMonth,
                    Day::DayOfYear => fields::Day::DayOfYear,
                }),
                length: match day {
                    // d    1 	  Numeric day of month: minimum digits
                    // dd   01 	  Numeric day of month: 2 digits, zero pad if needed
                    // F    1  	  Numeric day of week in month: minimum digits
                    // D    1     Numeric day of year: minimum digits
                    Day::NumericDayOfMonth | Day::DayOfWeekInMonth | Day::DayOfYear => {
                        FieldLength::One
                    }
                    Day::TwoDigitDayOfMonth => FieldLength::Two,
                },
            });
        }

        if let Some(weekday) = self.weekday {
            // TODO(#593) Unimplemented fields
            // e - Local day of week.
            // c - Stand-alone local day of week.
            fields.push(Field {
                symbol: FieldSymbol::Weekday(fields::Weekday::Format),
                length: match weekday {
                    // Day of week name, format length.
                    //
                    // E..EEE   Tue      Abbreviated
                    // EEEE     Tuesday  Wide
                    // EEEEE    T 	     Narrow
                    // EEEEEE   Tu       Short
                    Text::Long => FieldLength::Four,
                    Text::Short => FieldLength::One,
                    Text::Narrow => FieldLength::Five,
                },
            });
        }

        // The period fields are not included in skeletons:
        // a - AM, PM
        // b - am, pm, noon, midnight
        // c - flexible day periods

        if let Some(hour) = self.hour {
            // fields::Hour::H11
            // fields::Hour::H12
            // fields::Hour::H23
            // fields::Hour::H24

            let hour_cycle = self.hour_cycle.unwrap_or(default_hour_cycle);

            // When used in skeleton data or in a skeleton passed in an API for flexible date
            // pattern generation, it should match the 12-hour-cycle format preferred by the
            // locale (h or K); it should not match a 24-hour-cycle format (H or k).
            fields.push(Field {
                symbol: FieldSymbol::Hour(match hour_cycle {
                    // Skeletons only contain the h12, not h11. The pattern that is matched
                    // is free to use h11 or h12.
                    HourCycle::H11 | HourCycle::H12 => {
                        // h - symbol
                        fields::Hour::H12
                    }
                    // Skeletons only contain the h23, not h24. The pattern that is matched
                    // is free to use h23 or h24.
                    HourCycle::H24 | HourCycle::H23 => {
                        // H - symbol
                        fields::Hour::H23
                    }
                    _ => unreachable!(),
                }),
                length: match hour {
                    // Example for h: (note that this is the same for k, K, and H)
                    // h     1, 12  Numeric: minimum digits
                    // hh   01, 12  Numeric: 2 digits, zero pad if needed
                    Numeric::Numeric => FieldLength::One,
                    Numeric::TwoDigit => FieldLength::Two,
                },
            });
        }

        if let Some(minute) = self.minute {
            // m   8, 59    Numeric: minimum digits
            // mm  08, 59   Numeric: 2 digits, zero pad if needed
            fields.push(Field {
                symbol: FieldSymbol::Minute,
                length: match minute {
                    Numeric::Numeric => FieldLength::One,
                    Numeric::TwoDigit => FieldLength::Two,
                },
            });
        }

        if let Some(second) = self.second {
            let symbol = match self.fractional_second {
                None => FieldSymbol::Second(fields::Second::Second),
                Some(fractional_second) => {
                    FieldSymbol::from_fractional_second_digits(fractional_second)
                }
            };
            // s    8, 12    Numeric: minimum digits
            // ss  08, 12    Numeric: 2 digits, zero pad if needed
            fields.push(Field {
                symbol,
                length: match second {
                    Numeric::Numeric => FieldLength::One,
                    Numeric::TwoDigit => FieldLength::Two,
                },
            });
            // S - Fractional seconds. Represented as DecimalSecond.
            // A - Milliseconds in day. Not used in skeletons.
        }

        if self.time_zone_name.is_some() {
            // Only the lower "v" field is used in skeletons.
            fields.push(Field {
                symbol: FieldSymbol::TimeZone(fields::TimeZone::GenericNonLocation),
                length: FieldLength::One,
            });
        }

        {
            #![allow(clippy::indexing_slicing)] // debug
            debug_assert!(
                fields.windows(2).all(|f| f[0] < f[1]),
                "The fields are sorted and unique."
            );
        }

        fields
    }
}

/// A numeric component for the `components::`[`Bag`]. It is used for the year, day, hour, minute,
/// and second.
///
/// <div class="stab unstable">
/// 🚧 This code is experimental; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. It can be enabled with the `experimental` Cargo feature
/// of the icu meta-crate. Use with caution.
/// <a href="https://github.com/unicode-org/icu4x/issues/1317">#1317</a>
/// </div>
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[cfg_attr(
    feature = "serde",
    derive(Serialize, Deserialize),
    serde(rename_all = "kebab-case")
)]
#[non_exhaustive]
pub enum Numeric {
    /// Display the numeric value. For instance in a year this would be "1970".
    Numeric,
    /// Display the two digit value. For instance in a year this would be "70".
    TwoDigit,
}

/// A text component for the `components::`[`Bag`]. It is used for the era and weekday.
///
/// <div class="stab unstable">
/// 🚧 This code is experimental; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. It can be enabled with the `experimental` Cargo feature
/// of the icu meta-crate. Use with caution.
/// <a href="https://github.com/unicode-org/icu4x/issues/1317">#1317</a>
/// </div>
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[cfg_attr(
    feature = "serde",
    derive(Serialize, Deserialize),
    serde(rename_all = "kebab-case")
)]
#[non_exhaustive]
pub enum Text {
    /// Display the long form of the text, such as "Wednesday" for the weekday.
    /// In UTS-35, known as "Wide" (4 letters)
    Long,
    /// Display the short form of the text, such as "Wed" for the weekday.
    /// In UTS-35, known as "Abbreviated" (3 letters)
    Short,
    /// Display the narrow form of the text, such as "W" for the weekday.
    /// In UTS-35, known as "Narrow" (5 letters)
    Narrow,
}

/// Options for displaying a Year for the `components::`[`Bag`].
///
/// <div class="stab unstable">
/// 🚧 This code is experimental; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. It can be enabled with the `experimental` Cargo feature
/// of the icu meta-crate. Use with caution.
/// <a href="https://github.com/unicode-org/icu4x/issues/1317">#1317</a>
/// </div>
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[cfg_attr(
    feature = "serde",
    derive(Serialize, Deserialize),
    serde(rename_all = "kebab-case")
)]
#[non_exhaustive]
pub enum Year {
    /// The numeric value of the year, such as "2018" for 2018-12-31.
    Numeric,
    /// The two-digit value of the year, such as "18" for 2018-12-31.
    TwoDigit,
    /// The numeric value of the year in "week-of-year", such as "2019" in
    /// "week 01 of 2019" for the week of 2018-12-31 according to the ISO calendar.
    NumericWeekOf,
    /// The numeric value of the year in "week-of-year", such as "19" in
    /// "week 01 '19" for the week of 2018-12-31 according to the ISO calendar.
    TwoDigitWeekOf,
}

/// Options for displaying a Month for the `components::`[`Bag`].
///
/// <div class="stab unstable">
/// 🚧 This code is experimental; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. It can be enabled with the `experimental` Cargo feature
/// of the icu meta-crate. Use with caution.
/// <a href="https://github.com/unicode-org/icu4x/issues/1317">#1317</a>
/// </div>
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[cfg_attr(
    feature = "serde",
    derive(Serialize, Deserialize),
    serde(rename_all = "kebab-case")
)]
#[non_exhaustive]
pub enum Month {
    /// The numeric value of the month, such as "4".
    Numeric,
    /// The two-digit value of the month, such as "04".
    TwoDigit,
    /// The long value of the month, such as "April".
    Long,
    /// The short value of the month, such as "Apr".
    Short,
    /// The narrow value of the month, such as "A".
    Narrow,
}

// Each enum variant is documented with the UTS 35 field information from:
// https://unicode.org/reports/tr35/tr35-dates.html#Date_Field_Symbol_Table

/// Options for displaying the current week number for the `components::`[`Bag`].
///
/// Week numbers are relative to either a month or year, e.g. 'week 3 of January' or 'week 40 of 2000'.
///
/// <div class="stab unstable">
/// 🚧 This code is experimental; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. It can be enabled with the `experimental` Cargo feature
/// of the icu meta-crate. Use with caution.
/// <a href="https://github.com/unicode-org/icu4x/issues/1317">#1317</a>
/// </div>
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[cfg_attr(
    feature = "serde",
    derive(Serialize, Deserialize),
    serde(rename_all = "kebab-case")
)]
#[non_exhaustive]
pub enum Week {
    /// The week of the month, such as the "3" in "week 3 of January".
    WeekOfMonth,
    /// The numeric value of the week of the year, such as the "8" in "week 8 of 2000".
    NumericWeekOfYear,
    /// The two-digit value of the week of the year, such as the "08" in "2000-W08".
    TwoDigitWeekOfYear,
}

/// Options for displaying the current day of the month or year.
///
/// <div class="stab unstable">
/// 🚧 This code is experimental; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. It can be enabled with the `experimental` Cargo feature
/// of the icu meta-crate. Use with caution.
/// <a href="https://github.com/unicode-org/icu4x/issues/1317">#1317</a>
/// </div>
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[cfg_attr(
    feature = "serde",
    derive(Serialize, Deserialize),
    serde(rename_all = "kebab-case")
)]
#[non_exhaustive]
pub enum Day {
    /// The numeric value of the day of month, such as the "2" in July 2 1984.
    NumericDayOfMonth,
    /// The two digit value of the day of month, such as the "02" in 1984-07-02.
    TwoDigitDayOfMonth,
    /// The day of week in this month, such as the "2" in 2nd Wednesday of July.
    DayOfWeekInMonth,
    /// The day of year (numeric).
    DayOfYear,
}

/// Options for displaying a time zone for the `components::`[`Bag`].
///
/// Note that the initial implementation is focusing on only supporting ECMA-402 compatible
/// options.
///
/// <div class="stab unstable">
/// 🚧 This code is experimental; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. It can be enabled with the `experimental` Cargo feature
/// of the icu meta-crate. Use with caution.
/// <a href="https://github.com/unicode-org/icu4x/issues/1317">#1317</a>
/// </div>
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[cfg_attr(
    feature = "serde",
    derive(Serialize, Deserialize),
    serde(rename_all = "kebab-case")
)]
#[non_exhaustive]
pub enum TimeZoneName {
    // UTS-35 fields: z..zzz
    /// Short localized form, without the location. (e.g.: PST, GMT-8)
    ShortSpecific,

    // UTS-35 fields: zzzz
    // Per UTS-35: [long form] specific non-location (falling back to long localized offset)
    /// Long localized form, without the location (e.g., Pacific Standard Time, Nordamerikanische Westküsten-Normalzeit)
    LongSpecific,

    // UTS-35 fields: OOOO
    // Per UTS-35: The long localized offset format. This is equivalent to the "OOOO" specifier
    /// Long localized offset form, e.g. GMT-08:00
    LongOffset,

    // UTS-35 fields: O
    // Per UTS-35: Short localized offset format
    /// Short localized offset form, e.g. GMT-8
    ShortOffset,

    // UTS-35 fields: v
    //   * falling back to generic location (See UTS 35 for more specific rules)
    //   * falling back to short localized offset
    /// Short generic non-location format (e.g.: PT, Los Angeles, Zeit).
    ShortGeneric,

    // UTS-35 fields: vvvv
    //  * falling back to generic location (See UTS 35 for more specific rules)
    //  * falling back to long localized offset
    /// Long generic non-location format (e.g.: Pacific Time, Nordamerikanische Westküstenzeit),
    LongGeneric,
}

impl From<TimeZoneName> for Field {
    fn from(time_zone_name: TimeZoneName) -> Self {
        match time_zone_name {
            TimeZoneName::ShortSpecific => Field {
                symbol: FieldSymbol::TimeZone(fields::TimeZone::SpecificNonLocation),
                length: FieldLength::One,
            },
            TimeZoneName::LongSpecific => Field {
                symbol: FieldSymbol::TimeZone(fields::TimeZone::SpecificNonLocation),
                length: FieldLength::Four,
            },
            TimeZoneName::LongOffset => Field {
                symbol: FieldSymbol::TimeZone(fields::TimeZone::LocalizedOffset),
                length: FieldLength::Four,
            },
            TimeZoneName::ShortOffset => Field {
                symbol: FieldSymbol::TimeZone(fields::TimeZone::LocalizedOffset),
                length: FieldLength::One,
            },
            TimeZoneName::ShortGeneric => Field {
                symbol: FieldSymbol::TimeZone(fields::TimeZone::GenericNonLocation),
                length: FieldLength::One,
            },
            TimeZoneName::LongGeneric => Field {
                symbol: FieldSymbol::TimeZone(fields::TimeZone::GenericNonLocation),
                length: FieldLength::Four,
            },
        }
    }
}

impl From<&DateTimePattern> for Bag {
    fn from(value: &DateTimePattern) -> Self {
        Self::from(value.as_borrowed().0)
    }
}

impl From<&Pattern<'_>> for Bag {
    fn from(pattern: &Pattern) -> Self {
        let mut bag: Bag = Default::default();

        // Transform the fields into components per:
        // https://unicode.org/reports/tr35/tr35-dates.html#Date_Field_Symbol_Table
        for item in pattern.items.iter() {
            let field = match item {
                PatternItem::Field(ref field) => field,
                PatternItem::Literal(_) => continue,
            };
            match field.symbol {
                FieldSymbol::Era => {
                    bag.era = Some(match field.length {
                        FieldLength::One
                        | FieldLength::NumericOverride(_)
                        | FieldLength::Two
                        | FieldLength::Three => Text::Short,
                        FieldLength::Four => Text::Long,
                        FieldLength::Five | FieldLength::Six => Text::Narrow,
                    });
                }
                FieldSymbol::Year(year) => {
                    bag.year = Some(match year {
                        fields::Year::Calendar => match field.length {
                            FieldLength::Two => Year::TwoDigit,
                            _ => Year::Numeric,
                        },
                        // fields::Year::WeekOf => match field.length {
                        //     FieldLength::TwoDigit => Year::TwoDigitWeekOf,
                        //     _ => Year::NumericWeekOf,
                        // },
                        // TODO(#3762): Add support for U and r
                        _ => Year::Numeric,
                    });
                }
                FieldSymbol::Month(_) => {
                    // `Month::StandAlone` is only relevant in the pattern, so only differentiate
                    // on the field length.
                    bag.month = Some(match field.length {
                        FieldLength::One => Month::Numeric,
                        FieldLength::NumericOverride(_) => Month::Numeric,
                        FieldLength::Two => Month::TwoDigit,
                        FieldLength::Three => Month::Short,
                        FieldLength::Four => Month::Long,
                        FieldLength::Five | FieldLength::Six => Month::Narrow,
                    });
                }
                FieldSymbol::Week(_week) => {
                    // TODO(#5643): Add week fields back
                    // bag.week = Some(match week {
                    //     fields::Week::WeekOfYear => match field.length {
                    //         FieldLength::TwoDigit => Week::TwoDigitWeekOfYear,
                    //         _ => Week::NumericWeekOfYear,
                    //     },
                    //     fields::Week::WeekOfMonth => Week::WeekOfMonth,
                    // });
                }
                FieldSymbol::Day(day) => {
                    bag.day = Some(match day {
                        fields::Day::DayOfMonth => match field.length {
                            FieldLength::Two => Day::TwoDigitDayOfMonth,
                            _ => Day::NumericDayOfMonth,
                        },
                        fields::Day::DayOfYear => Day::DayOfYear,
                        fields::Day::DayOfWeekInMonth => Day::DayOfWeekInMonth,
                    });
                }
                FieldSymbol::Weekday(weekday) => {
                    bag.weekday = Some(match weekday {
                        fields::Weekday::Format => match field.length {
                            FieldLength::One | FieldLength::Two | FieldLength::Three => Text::Short,
                            FieldLength::Four => Text::Long,
                            _ => Text::Narrow,
                        },
                        fields::Weekday::StandAlone => match field.length {
                            FieldLength::One
                            | FieldLength::Two
                            | FieldLength::NumericOverride(_) => {
                                // Stand-alone fields also support a numeric 1 digit per UTS-35, but there is
                                // no way to request it using the current system. As of 2021-12-06
                                // no skeletons resolve to patterns containing this symbol.
                                //
                                // All resolved patterns from cldr-json:
                                // https://github.com/gregtatum/cldr-json/blob/d4779f9611a4cc1e3e6a0a4597e92ead32d9f118/stand-alone-week.js
                                //     'ccc',
                                //     'ccc d. MMM',
                                //     'ccc d. MMMM',
                                //     'cccc d. MMMM y',
                                //     'd, ccc',
                                //     'cccနေ့',
                                //     'ccc, d MMM',
                                //     "ccc, d 'de' MMMM",
                                //     "ccc, d 'de' MMMM 'de' y",
                                //     'ccc, h:mm B',
                                //     'ccc, h:mm:ss B',
                                //     'ccc, d',
                                //     "ccc, dd.MM.y 'г'.",
                                //     'ccc, d.MM.y',
                                //     'ccc, MMM d. y'
                                unimplemented!("Numeric stand-alone fields are not supported.")
                            }
                            FieldLength::Three => Text::Short,
                            FieldLength::Four => Text::Long,
                            FieldLength::Five | FieldLength::Six => Text::Narrow,
                        },
                        fields::Weekday::Local => unimplemented!("fields::Weekday::Local"),
                    });
                }
                FieldSymbol::DayPeriod(_) => {
                    // Day period does not affect the resolved components.
                }
                FieldSymbol::Hour(hour) => {
                    bag.hour = Some(match field.length {
                        FieldLength::Two => Numeric::TwoDigit,
                        _ => Numeric::Numeric,
                    });
                    bag.hour_cycle = Some(match hour {
                        fields::Hour::H11 => HourCycle::H11,
                        fields::Hour::H12 => HourCycle::H12,
                        fields::Hour::H23 => HourCycle::H23,
                        fields::Hour::H24 => HourCycle::H24,
                    });
                }
                FieldSymbol::Minute => {
                    bag.minute = Some(match field.length {
                        FieldLength::Two => Numeric::TwoDigit,
                        _ => Numeric::Numeric,
                    });
                }
                FieldSymbol::Second(second) => match second {
                    fields::Second::Second => {
                        bag.second = Some(match field.length {
                            FieldLength::Two => Numeric::TwoDigit,
                            _ => Numeric::Numeric,
                        });
                    }
                    fields::Second::MillisInDay => unimplemented!("fields::Second::MillisInDay"),
                },
                FieldSymbol::DecimalSecond(decimal_second) => {
                    use FractionalSecondDigits::*;
                    bag.second = Some(match field.length {
                        FieldLength::Two => Numeric::TwoDigit,
                        _ => Numeric::Numeric,
                    });
                    bag.fractional_second = Some(match decimal_second {
                        fields::DecimalSecond::SecondF1 => F1,
                        fields::DecimalSecond::SecondF2 => F2,
                        fields::DecimalSecond::SecondF3 => F3,
                        fields::DecimalSecond::SecondF4 => F4,
                        fields::DecimalSecond::SecondF5 => F5,
                        fields::DecimalSecond::SecondF6 => F6,
                        fields::DecimalSecond::SecondF7 => F7,
                        fields::DecimalSecond::SecondF8 => F8,
                        fields::DecimalSecond::SecondF9 => F9,
                    });
                }
                FieldSymbol::TimeZone(time_zone_name) => {
                    bag.time_zone_name = Some(match time_zone_name {
                        fields::TimeZone::SpecificNonLocation => match field.length {
                            FieldLength::One => TimeZoneName::ShortSpecific,
                            _ => TimeZoneName::LongSpecific,
                        },
                        fields::TimeZone::GenericNonLocation => match field.length {
                            FieldLength::One => TimeZoneName::ShortGeneric,
                            _ => TimeZoneName::LongGeneric,
                        },
                        fields::TimeZone::LocalizedOffset => match field.length {
                            FieldLength::One => TimeZoneName::ShortOffset,
                            _ => TimeZoneName::LongOffset,
                        },
                        fields::TimeZone::Location => unimplemented!("fields::TimeZone::Location"),
                        fields::TimeZone::Iso => unimplemented!("fields::TimeZone::IsoZ"),
                        fields::TimeZone::IsoWithZ => unimplemented!("fields::TimeZone::Iso"),
                    });
                }
            }
        }

        bag
    }
}

#[cfg(test)]
mod test {
    use super::*;

    // Shorten these for terser tests.
    type Symbol = FieldSymbol;
    type Length = FieldLength;

    #[test]
    fn test_component_bag_to_vec_field() {
        let bag = Bag {
            year: Some(Year::Numeric),
            month: Some(Month::Long),
            // TODO(#5643): Add week fields back
            week: None,
            day: Some(Day::NumericDayOfMonth),

            hour: Some(Numeric::Numeric),
            minute: Some(Numeric::Numeric),
            second: Some(Numeric::Numeric),
            fractional_second: Some(FractionalSecondDigits::F3),

            ..Default::default()
        };
        assert_eq!(
            bag.to_vec_fields(HourCycle::H23),
            [
                (Symbol::Year(fields::Year::Calendar), Length::One).into(),
                (Symbol::Month(fields::Month::Format), Length::Four).into(),
                (Symbol::Day(fields::Day::DayOfMonth), Length::One).into(),
                (Symbol::Hour(fields::Hour::H23), Length::One).into(),
                (Symbol::Minute, Length::One).into(),
                (
                    Symbol::DecimalSecond(fields::DecimalSecond::SecondF3),
                    Length::One
                )
                    .into(),
            ]
        );
    }

    #[test]
    fn test_component_bag_to_vec_field2() {
        let bag = Bag {
            year: Some(Year::Numeric),
            month: Some(Month::TwoDigit),
            day: Some(Day::NumericDayOfMonth),
            ..Default::default()
        };
        assert_eq!(
            bag.to_vec_fields(HourCycle::H23),
            [
                (Symbol::Year(fields::Year::Calendar), Length::One).into(),
                (Symbol::Month(fields::Month::Format), Length::Two).into(),
                (Symbol::Day(fields::Day::DayOfMonth), Length::One).into(),
            ]
        );
    }
}
