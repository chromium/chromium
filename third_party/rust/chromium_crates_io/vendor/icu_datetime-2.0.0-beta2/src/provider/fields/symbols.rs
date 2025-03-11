// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::options::SubsecondDigits;
#[cfg(feature = "datagen")]
use crate::provider::fields::FieldLength;
use core::{cmp::Ordering, convert::TryFrom};
use displaydoc::Display;
use icu_locale_core::preferences::extensions::unicode::keywords::HourCycle;
use icu_provider::prelude::*;
use zerovec::ule::{AsULE, UleError, ULE};

/// An error relating to the field symbol for a date pattern field.
#[derive(Display, Debug, PartialEq, Copy, Clone)]
#[non_exhaustive]
pub enum SymbolError {
    /// Invalid field symbol index.
    #[displaydoc("Invalid field symbol index: {0}")]
    InvalidIndex(u8),
    /// Unknown field symbol.
    #[displaydoc("Unknown field symbol: {0}")]
    Unknown(char),
    /// Invalid character for a field symbol.
    #[displaydoc("Invalid character for a field symbol: {0}")]
    Invalid(u8),
}

impl core::error::Error for SymbolError {}

/// A field symbol for a date formatting pattern.
///
/// Field symbols are a more granular distinction
/// for a pattern field within the category of a field type. Examples of field types are:
/// `Year`, `Month`, `Hour`.  Within the [`Hour`] field type, examples of field symbols are: [`Hour::H12`],
/// [`Hour::H24`]. Each field symbol is represented within the date formatting pattern string
/// by a distinct character from the set of `A..Z` and `a..z`.
#[derive(Debug, Eq, PartialEq, Clone, Copy)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::fields))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[allow(clippy::exhaustive_enums)] // part of data struct
pub enum FieldSymbol {
    /// Era name.
    Era,
    /// Year number or year name.
    Year(Year),
    /// Month number or month name.
    Month(Month),
    /// Week number or week name.
    Week(Week),
    /// Day number relative to a time period longer than a week (ex: month, year).
    Day(Day),
    /// Day number or day name relative to a week.
    Weekday(Weekday),
    /// Name of a period within a day.
    DayPeriod(DayPeriod),
    /// Hour number within a day, possibly with day period.
    Hour(Hour),
    /// Minute number within an hour.
    Minute,
    /// Seconds integer within a minute or milliseconds within a day.
    Second(Second),
    /// Time zone as a name, a zone ID, or a ISO 8601 numerical offset.
    TimeZone(TimeZone),
    /// Seconds with fractional digits. If seconds are an integer,
    /// [`FieldSymbol::Second`] is used.
    DecimalSecond(DecimalSecond),
}

impl FieldSymbol {
    /// Symbols are necessary components of `Pattern` struct which
    /// uses efficient byte serialization and deserialization via `zerovec`.
    ///
    /// The `FieldSymbol` impl provides non-public methods that can be used to efficiently
    /// convert between `u8` and the symbol variant.
    ///
    /// The serialization model packages the variant in one byte.
    ///
    /// 1) The top four bits are used to determine the type of the field
    ///    using that type's `idx()/from_idx()` for the mapping.
    ///    (Examples: `Year`, `Month`, `Hour`)
    ///
    /// 2) The bottom four bits are used to determine the symbol of the type.
    ///    (Examples: `Year::Calendar`, `Hour::H11`)
    ///
    /// # Diagram
    ///
    /// ```text
    /// ┌─┬─┬─┬─┬─┬─┬─┬─┐
    /// ├─┴─┴─┴─┼─┴─┴─┴─┤
    /// │ Type  │Symbol │
    /// └───────┴───────┘
    /// ```
    ///
    /// # Optimization
    ///
    /// This model is optimized to package data efficiently when `FieldSymbol`
    /// is used as a variant of `PatternItem`. See the documentation of `PatternItemULE`
    /// for details on how it is composed.
    ///
    /// # Constraints
    ///
    /// This model limits the available number of possible types and symbols to 16 each.
    #[inline]
    pub(crate) fn idx(self) -> u8 {
        let (high, low) = match self {
            FieldSymbol::Era => (0, 0),
            FieldSymbol::Year(year) => (1, year.idx()),
            FieldSymbol::Month(month) => (2, month.idx()),
            FieldSymbol::Week(w) => (3, w.idx()),
            FieldSymbol::Day(day) => (4, day.idx()),
            FieldSymbol::Weekday(wd) => (5, wd.idx()),
            FieldSymbol::DayPeriod(dp) => (6, dp.idx()),
            FieldSymbol::Hour(hour) => (7, hour.idx()),
            FieldSymbol::Minute => (8, 0),
            FieldSymbol::Second(second) => (9, second.idx()),
            FieldSymbol::TimeZone(tz) => (10, tz.idx()),
            FieldSymbol::DecimalSecond(second) => (11, second.idx()),
        };
        let result = high << 4;
        result | low
    }

    #[inline]
    pub(crate) fn from_idx(idx: u8) -> Result<Self, SymbolError> {
        // extract the top four bits to determine the symbol.
        let low = idx & 0b0000_1111;
        // use the bottom four bits out of `u8` to disriminate the field type.
        let high = idx >> 4;

        Ok(match high {
            0 if low == 0 => Self::Era,
            1 => Self::Year(Year::from_idx(low)?),
            2 => Self::Month(Month::from_idx(low)?),
            3 => Self::Week(Week::from_idx(low)?),
            4 => Self::Day(Day::from_idx(low)?),
            5 => Self::Weekday(Weekday::from_idx(low)?),
            6 => Self::DayPeriod(DayPeriod::from_idx(low)?),
            7 => Self::Hour(Hour::from_idx(low)?),
            8 if low == 0 => Self::Minute,
            9 => Self::Second(Second::from_idx(low)?),
            10 => Self::TimeZone(TimeZone::from_idx(low)?),
            11 => Self::DecimalSecond(DecimalSecond::from_idx(low)?),
            _ => return Err(SymbolError::InvalidIndex(idx)),
        })
    }

    /// Returns the index associated with this FieldSymbol.
    #[cfg(feature = "datagen")]
    fn idx_for_skeleton(self) -> u8 {
        match self {
            FieldSymbol::Era => 0,
            FieldSymbol::Year(_) => 1,
            FieldSymbol::Month(_) => 2,
            FieldSymbol::Week(_) => 3,
            FieldSymbol::Day(_) => 4,
            FieldSymbol::Weekday(_) => 5,
            FieldSymbol::DayPeriod(_) => 6,
            FieldSymbol::Hour(_) => 7,
            FieldSymbol::Minute => 8,
            FieldSymbol::Second(_) | FieldSymbol::DecimalSecond(_) => 9,
            FieldSymbol::TimeZone(_) => 10,
        }
    }

    /// Compares this enum with other solely based on the enum variant,
    /// ignoring the enum's data.
    ///
    /// Second and DecimalSecond are considered equal.
    #[cfg(feature = "datagen")]
    pub(crate) fn skeleton_cmp(self, other: Self) -> Ordering {
        self.idx_for_skeleton().cmp(&other.idx_for_skeleton())
    }

    pub(crate) fn from_subsecond_digits(subsecond_digits: SubsecondDigits) -> Self {
        use SubsecondDigits::*;
        match subsecond_digits {
            S1 => FieldSymbol::DecimalSecond(DecimalSecond::Subsecond1),
            S2 => FieldSymbol::DecimalSecond(DecimalSecond::Subsecond2),
            S3 => FieldSymbol::DecimalSecond(DecimalSecond::Subsecond3),
            S4 => FieldSymbol::DecimalSecond(DecimalSecond::Subsecond4),
            S5 => FieldSymbol::DecimalSecond(DecimalSecond::Subsecond5),
            S6 => FieldSymbol::DecimalSecond(DecimalSecond::Subsecond6),
            S7 => FieldSymbol::DecimalSecond(DecimalSecond::Subsecond7),
            S8 => FieldSymbol::DecimalSecond(DecimalSecond::Subsecond8),
            S9 => FieldSymbol::DecimalSecond(DecimalSecond::Subsecond9),
        }
    }

    /// UTS 35 defines several 1 and 2 symbols to be the same as 3 symbols (abbreviated).
    /// For example, 'a' represents an abbreviated day period, the same as 'aaa'.
    ///
    /// This function maps field lengths 1 and 2 to field length 3.
    #[cfg(feature = "datagen")]
    pub(crate) fn is_at_least_abbreviated(self) -> bool {
        matches!(
            self,
            FieldSymbol::Era
                | FieldSymbol::Year(Year::Cyclic)
                | FieldSymbol::Weekday(Weekday::Format)
                | FieldSymbol::DayPeriod(_)
                | FieldSymbol::TimeZone(TimeZone::SpecificNonLocation)
        )
    }
}

/// [`ULE`](zerovec::ule::ULE) type for [`FieldSymbol`]
#[repr(transparent)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub struct FieldSymbolULE(u8);

impl AsULE for FieldSymbol {
    type ULE = FieldSymbolULE;
    fn to_unaligned(self) -> Self::ULE {
        FieldSymbolULE(self.idx())
    }
    fn from_unaligned(unaligned: Self::ULE) -> Self {
        #[allow(clippy::unwrap_used)] // OK because the ULE is pre-validated
        Self::from_idx(unaligned.0).unwrap()
    }
}

impl FieldSymbolULE {
    #[inline]
    pub(crate) fn validate_byte(byte: u8) -> Result<(), UleError> {
        FieldSymbol::from_idx(byte)
            .map(|_| ())
            .map_err(|_| UleError::parse::<FieldSymbol>())
    }
}

// Safety checklist for ULE:
//
// 1. Must not include any uninitialized or padding bytes (true since transparent over a ULE).
// 2. Must have an alignment of 1 byte (true since transparent over a ULE).
// 3. ULE::validate_bytes() checks that the given byte slice represents a valid slice.
// 4. ULE::validate_bytes() checks that the given byte slice has a valid length
//    (true since transparent over a type of size 1).
// 5. All other methods must be left with their default impl.
// 6. Byte equality is semantic equality.
unsafe impl ULE for FieldSymbolULE {
    fn validate_bytes(bytes: &[u8]) -> Result<(), UleError> {
        for byte in bytes {
            Self::validate_byte(*byte)?;
        }
        Ok(())
    }
}

#[derive(Debug, Eq, PartialEq, Clone, Copy)]
#[allow(clippy::exhaustive_enums)] // used in data struct
#[cfg(feature = "datagen")]
pub(crate) enum TextOrNumeric {
    Text,
    Numeric,
}

/// [`FieldSymbols`](FieldSymbol) can be either text or numeric. This categorization is important
/// when matching skeletons with a components [`Bag`](crate::options::components::Bag).
#[cfg(feature = "datagen")]
pub(crate) trait LengthType {
    fn get_length_type(self, length: FieldLength) -> TextOrNumeric;
}

impl FieldSymbol {
    /// Skeletons are a Vec<Field>, and represent the Fields that can be used to match to a
    /// specific pattern. The order of the Vec does not affect the Pattern that is output.
    /// However, it's more performant when matching these fields, and it's more deterministic
    /// when serializing them to present them in a consistent order.
    ///
    /// This ordering is taken by the order of the fields listed in the [UTS 35 Date Field Symbol Table]
    /// (https://unicode.org/reports/tr35/tr35-dates.html#Date_Field_Symbol_Table), and are generally
    /// ordered most significant to least significant.
    fn get_canonical_order(self) -> u8 {
        match self {
            Self::Era => 0,
            Self::Year(Year::Calendar) => 1,
            // Self::Year(Year::WeekOf) => 2,
            Self::Year(Year::Cyclic) => 3,
            Self::Year(Year::RelatedIso) => 4,
            Self::Month(Month::Format) => 5,
            Self::Month(Month::StandAlone) => 6,
            // TODO(#5643): Add week fields back
            // Self::Week(Week::WeekOfYear) => 7,
            // Self::Week(Week::WeekOfMonth) => 8,
            Self::Week(_) => unreachable!(), // ZST references aren't uninhabited
            Self::Day(Day::DayOfMonth) => 9,
            Self::Day(Day::DayOfYear) => 10,
            Self::Day(Day::DayOfWeekInMonth) => 11,
            // Self::Day(Day::ModifiedJulianDay) => 12,
            Self::Weekday(Weekday::Format) => 13,
            Self::Weekday(Weekday::Local) => 14,
            Self::Weekday(Weekday::StandAlone) => 15,
            Self::DayPeriod(DayPeriod::AmPm) => 16,
            Self::DayPeriod(DayPeriod::NoonMidnight) => 17,
            Self::Hour(Hour::H11) => 18,
            Self::Hour(Hour::H12) => 19,
            Self::Hour(Hour::H23) => 20,
            Self::Hour(Hour::H24) => 21,
            Self::Minute => 22,
            Self::Second(Second::Second) => 23,
            Self::Second(Second::MillisInDay) => 24,
            Self::DecimalSecond(DecimalSecond::Subsecond1) => 31,
            Self::DecimalSecond(DecimalSecond::Subsecond2) => 32,
            Self::DecimalSecond(DecimalSecond::Subsecond3) => 33,
            Self::DecimalSecond(DecimalSecond::Subsecond4) => 34,
            Self::DecimalSecond(DecimalSecond::Subsecond5) => 35,
            Self::DecimalSecond(DecimalSecond::Subsecond6) => 36,
            Self::DecimalSecond(DecimalSecond::Subsecond7) => 37,
            Self::DecimalSecond(DecimalSecond::Subsecond8) => 38,
            Self::DecimalSecond(DecimalSecond::Subsecond9) => 39,
            Self::TimeZone(TimeZone::SpecificNonLocation) => 100,
            Self::TimeZone(TimeZone::LocalizedOffset) => 102,
            Self::TimeZone(TimeZone::GenericNonLocation) => 103,
            Self::TimeZone(TimeZone::Location) => 104,
            Self::TimeZone(TimeZone::Iso) => 105,
            Self::TimeZone(TimeZone::IsoWithZ) => 106,
        }
    }
}

impl TryFrom<char> for FieldSymbol {
    type Error = SymbolError;
    fn try_from(ch: char) -> Result<Self, Self::Error> {
        if !ch.is_ascii_alphanumeric() {
            return Err(SymbolError::Invalid(ch as u8));
        }

        (if ch == 'G' {
            Ok(Self::Era)
        } else {
            Err(SymbolError::Unknown(ch))
        })
        .or_else(|_| Year::try_from(ch).map(Self::Year))
        .or_else(|_| Month::try_from(ch).map(Self::Month))
        .or_else(|_| Week::try_from(ch).map(Self::Week))
        .or_else(|_| Day::try_from(ch).map(Self::Day))
        .or_else(|_| Weekday::try_from(ch).map(Self::Weekday))
        .or_else(|_| DayPeriod::try_from(ch).map(Self::DayPeriod))
        .or_else(|_| Hour::try_from(ch).map(Self::Hour))
        .or({
            if ch == 'm' {
                Ok(Self::Minute)
            } else {
                Err(SymbolError::Unknown(ch))
            }
        })
        .or_else(|_| Second::try_from(ch).map(Self::Second))
        .or_else(|_| TimeZone::try_from(ch).map(Self::TimeZone))
        // Note: char-to-enum conversion for DecimalSecond is handled directly in the parser
    }
}

impl From<FieldSymbol> for char {
    fn from(symbol: FieldSymbol) -> Self {
        match symbol {
            FieldSymbol::Era => 'G',
            FieldSymbol::Year(year) => year.into(),
            FieldSymbol::Month(month) => month.into(),
            FieldSymbol::Week(week) => week.into(),
            FieldSymbol::Day(day) => day.into(),
            FieldSymbol::Weekday(weekday) => weekday.into(),
            FieldSymbol::DayPeriod(dayperiod) => dayperiod.into(),
            FieldSymbol::Hour(hour) => hour.into(),
            FieldSymbol::Minute => 'm',
            FieldSymbol::Second(second) => second.into(),
            FieldSymbol::TimeZone(time_zone) => time_zone.into(),
            // Note: This is only used for representing the integer portion
            FieldSymbol::DecimalSecond(_) => 's',
        }
    }
}

impl PartialOrd for FieldSymbol {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for FieldSymbol {
    fn cmp(&self, other: &Self) -> Ordering {
        self.get_canonical_order().cmp(&other.get_canonical_order())
    }
}

macro_rules! field_type {
    ($(#[$enum_attr:meta])* $i:ident; { $( $(#[$variant_attr:meta])* $key:literal => $val:ident = $idx:expr,)* }; $length_type:ident; $($ule_name:ident)?) => (
        field_type!($(#[$enum_attr])* $i; {$( $(#[$variant_attr])* $key => $val = $idx,)*}; $($ule_name)?);

        #[cfg(feature = "datagen")]
        impl LengthType for $i {
            fn get_length_type(self, _length: FieldLength) -> TextOrNumeric {
                TextOrNumeric::$length_type
            }
        }
    );
    ($(#[$enum_attr:meta])* $i:ident; { $( $(#[$variant_attr:meta])* $key:literal => $val:ident = $idx:expr,)* }; $($ule_name:ident)?) => (
        #[derive(Debug, Eq, PartialEq, Ord, PartialOrd, Clone, Copy, yoke::Yokeable, zerofrom::ZeroFrom)]
        // FIXME: This should be replaced with a custom derive.
        // See: https://github.com/unicode-org/icu4x/issues/1044
        #[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
        #[cfg_attr(feature = "datagen", databake(path = icu_datetime::fields))]
        #[cfg_attr(feature = "serde", derive(serde::Deserialize))]
        #[allow(clippy::enum_variant_names)]
        $(
            #[repr(u8)]
            #[zerovec::make_ule($ule_name)]
            #[zerovec::derive(Debug)]
        )?
        #[allow(clippy::exhaustive_enums)] // used in data struct
        $(#[$enum_attr])*
        pub enum $i {
            $(
                $(#[$variant_attr])*
                #[doc = core::concat!("\n\nThis field symbol is represented by the character `", $key, "` in a date formatting pattern string.")]
                #[doc = "\n\nFor more details, see documentation on [date field symbols](https://unicode.org/reports/tr35/tr35-dates.html#table-date-field-symbol-table)."]
                $val = $idx,
            )*
        }

        $(
            #[allow(path_statements)] // #5643 impl conditional on $ule_name
            const _: () = { $ule_name; };

        impl $i {
            /// Retrieves an index of the field variant.
            ///
            /// # Examples
            ///
            /// ```ignore
            /// use icu::datetime::fields::Month;
            ///
            /// assert_eq!(Month::StandAlone::idx(), 1);
            /// ```
            ///
            /// # Stability
            ///
            /// This is mostly useful for serialization,
            /// and does not guarantee index stability between ICU4X
            /// versions.
            #[inline]
            pub(crate) fn idx(self) -> u8 {
                self as u8
            }

            /// Retrieves a field variant from an index.
            ///
            /// # Examples
            ///
            /// ```ignore
            /// use icu::datetime::fields::Month;
            ///
            /// assert_eq!(Month::from_idx(0), Month::Format);
            /// ```
            ///
            /// # Stability
            ///
            /// This is mostly useful for serialization,
            /// and does not guarantee index stability between ICU4X
            /// versions.
            #[inline]
            pub(crate) fn from_idx(idx: u8) -> Result<Self, SymbolError> {
                Self::new_from_u8(idx)
                    .ok_or(SymbolError::InvalidIndex(idx))
            }
        }
        )?

        impl TryFrom<char> for $i {
            type Error = SymbolError;

            fn try_from(ch: char) -> Result<Self, Self::Error> {
                match ch {
                    $(
                        $key => Ok(Self::$val),
                    )*
                    _ => Err(SymbolError::Unknown(ch)),
                }
            }
        }

        impl From<$i> for FieldSymbol {
            fn from(input: $i) -> Self {
                Self::$i(input)
            }
        }

        impl From<$i> for char {
            fn from(input: $i) -> char {
                match input {
                    $(
                        $i::$val => $key,
                    )*
                }
            }
        }
    );
}

field_type! (
    /// An enum for the possible symbols of a year field in a date pattern.
    Year; {
        /// Field symbol for calendar year (numeric).
        ///
        /// In most cases the length of this field specifies the minimum number of digits to display, zero-padded as necessary. For most use cases, [`Year::Calendar`] or `Year::WeekOf` should be adequate.
        'y' => Calendar = 0,
        /// Field symbol for cyclic year; used in calendars where years are tracked in cycles, such as the Chinese or Dangi calendars.
        'U' => Cyclic = 1,
        /// Field symbol for related ISO; some calendars which use different year numbering than ISO, or no year numbering, may express years in an ISO year corresponding to a calendar year.
        'r' => RelatedIso = 2,
        // /// Field symbol for year in "week of year".
        // ///
        // /// This works for “week of year” based calendars in which the year transition occurs on a week boundary; may differ from calendar year [`Year::Calendar`] near a year transition. This numeric year designation is used in conjunction with [`Week::WeekOfYear`], but can be used in non-Gregorian based calendar systems where week date processing is desired. The field length is interpreted in the same way as for [`Year::Calendar`].
        // 'Y' => WeekOf = 3,
    };
    YearULE
);

#[cfg(feature = "datagen")]
impl LengthType for Year {
    fn get_length_type(self, _length: FieldLength) -> TextOrNumeric {
        // https://unicode.org/reports/tr35/tr35-dates.html#dfst-year
        match self {
            Year::Cyclic => TextOrNumeric::Text,
            _ => TextOrNumeric::Numeric,
        }
    }
}

field_type!(
    /// An enum for the possible symbols of a month field in a date pattern.
    Month; {
        /// Field symbol for month number or name in a pattern that contains multiple fields.
        'M' => Format = 0,
        /// Field symbol for a "stand-alone" month number or name.
        /// 
        /// The stand-alone month name is used when the month is displayed by itself. This may differ from the standard form based on the language and context.
        'L' => StandAlone = 1,
}; MonthULE);

#[cfg(feature = "datagen")]
impl LengthType for Month {
    fn get_length_type(self, length: FieldLength) -> TextOrNumeric {
        match length {
            FieldLength::One => TextOrNumeric::Numeric,
            FieldLength::NumericOverride(_) => TextOrNumeric::Numeric,
            FieldLength::Two => TextOrNumeric::Numeric,
            FieldLength::Three => TextOrNumeric::Text,
            FieldLength::Four => TextOrNumeric::Text,
            FieldLength::Five => TextOrNumeric::Text,
            FieldLength::Six => TextOrNumeric::Text,
        }
    }
}

field_type!(
    /// An enum for the possible symbols of a day field in a date pattern.
    Day; {
        /// Field symbol for day of month (numeric).
        'd' => DayOfMonth = 0,
        /// Field symbol for day of year (numeric).
        'D' => DayOfYear = 1,
        /// Field symbol for the day of week occurrence relative to the month (numeric).
        ///
        /// For the example `"2nd Wed in July"`, this field would provide `"2"`.  Should likely be paired with the [`Weekday`] field.
        'F' => DayOfWeekInMonth = 2,
        // /// Field symbol for the modified Julian day (numeric).
        // ///
        // /// The value of this field differs from the conventional Julian day number in a couple of ways, which are based on measuring relative to the local time zone.
        // 'g' => ModifiedJulianDay = 3,
    };
    Numeric;
    DayULE
);

field_type!(
    /// An enum for the possible symbols of an hour field in a date pattern.
    Hour; {
        /// Field symbol for numeric hour [0-11].
        'K' => H11 = 0,
        /// Field symbol for numeric hour [1-12].
        'h' => H12 = 1,
        /// Field symbol for numeric hour [0-23].
        'H' => H23 = 2,
        /// Field symbol for numeric hour [1-24].
        'k' => H24 = 3,
    };
    Numeric;
    HourULE
);

impl Hour {
    pub(crate) fn from_hour_cycle(hour_cycle: HourCycle) -> Self {
        match hour_cycle {
            HourCycle::H11 => Self::H11,
            HourCycle::H12 => Self::H12,
            HourCycle::H23 => Self::H23,
            HourCycle::H24 => Self::H24,
            _ => unreachable!(),
        }
    }
}

// NOTE: 'S' Subsecond is represented via DecimalSecond,
// so it is not included in the Second enum.

field_type!(
    /// An enum for the possible symbols of a second field in a date pattern.
    Second; {
        /// Field symbol for second (numeric).
        's' => Second = 0,
        /// Field symbol for milliseconds in day (numeric).
        ///
        /// This field behaves exactly like a composite of all time-related fields, not including the zone fields.
        'A' => MillisInDay = 1,
    };
    Numeric;
    SecondULE
);

field_type!(
    /// An enum for the possible symbols of a week field in a date pattern.
    Week; {
        // /// Field symbol for week of year (numeric).
        // ///
        // /// When used in a pattern with year, use [`Year::WeekOf`] for the year field instead of [`Year::Calendar`].
        // 'w' => WeekOfYear = 0,
        // /// Field symbol for week of month (numeric).
        // 'W' => WeekOfMonth = 1,
    };
    Numeric;
    // TODO(#5643): Recover ULE once the type is inhabited
    // WeekULE
);

impl Week {
    /// Retrieves an index of the field variant.
    ///
    /// # Examples
    ///
    /// ```ignore
    /// use icu::datetime::fields::Month;
    ///
    /// assert_eq!(Month::StandAlone::idx(), 1);
    /// ```
    ///
    /// # Stability
    ///
    /// This is mostly useful for serialization,
    /// and does not guarantee index stability between ICU4X
    /// versions.
    #[inline]
    pub(crate) fn idx(self) -> u8 {
        0
    }

    /// Retrieves a field variant from an index.
    ///
    /// # Examples
    ///
    /// ```ignore
    /// use icu::datetime::fields::Month;
    ///
    /// assert_eq!(Month::from_idx(0), Month::Format);
    /// ```
    ///
    /// # Stability
    ///
    /// This is mostly useful for serialization,
    /// and does not guarantee index stability between ICU4X
    /// versions.
    #[inline]
    pub(crate) fn from_idx(idx: u8) -> Result<Self, SymbolError> {
        Err(SymbolError::InvalidIndex(idx))
    }
}

field_type!(
    /// An enum for the possible symbols of a weekday field in a date pattern.
    Weekday;  {
        /// Field symbol for day of week (text format only).
        'E' => Format = 0,
        /// Field symbol for day of week; numeric formats produce a locale-dependent ordinal weekday number.
        ///
        /// For example, in de-DE, Monday is the 1st day of the week.
        'e' => Local = 1,
        /// Field symbol for stand-alone local day of week number/name.
        ///
        /// The stand-alone weekday name is used when the weekday is displayed by itself. This may differ from the standard form based on the language and context.
        'c' => StandAlone = 2,
    };
    WeekdayULE
);

#[cfg(feature = "datagen")]
impl LengthType for Weekday {
    fn get_length_type(self, length: FieldLength) -> TextOrNumeric {
        match self {
            Self::Format => TextOrNumeric::Text,
            Self::Local | Self::StandAlone => match length {
                FieldLength::One | FieldLength::Two => TextOrNumeric::Numeric,
                _ => TextOrNumeric::Text,
            },
        }
    }
}

impl Weekday {
    /// UTS 35 says that "e" (local weekday) and "E" (format weekday) have the same non-numeric names.
    ///
    /// This function normalizes "e" to "E".
    pub(crate) fn to_format_symbol(self) -> Self {
        match self {
            Weekday::Local => Weekday::Format,
            other => other,
        }
    }
}

field_type!(
    /// An enum for the possible symbols of a day period field in a date pattern.
    DayPeriod; {
        /// Field symbol for the AM, PM day period.  (Does not include noon, midnight.)
        'a' => AmPm = 0,
        /// Field symbol for the am, pm, noon, midnight day period.
        'b' => NoonMidnight = 1,
    };
    Text;
    DayPeriodULE
);

field_type!(
    /// An enum for the possible symbols of a time zone field in a date pattern.
    TimeZone; {
        /// Field symbol for the specific non-location format of a time zone.
        ///
        /// For example: "Pacific Standard Time"
        'z' => SpecificNonLocation = 0,
        /// Field symbol for the localized offset format of a time zone.
        ///
        /// For example: "GMT-07:00"
        'O' => LocalizedOffset = 1,
        /// Field symbol for the generic non-location format of a time zone.
        ///
        /// For example: "Pacific Time"
        'v' => GenericNonLocation = 2,
        /// Field symbol for any of: the time zone id, time zone exemplar city, or generic location format.
        'V' => Location = 3,
        /// Field symbol for either the ISO-8601 basic format or ISO-8601 extended format. This does not use an
        /// optional ISO-8601 UTC indicator `Z`, whereas [`TimeZone::IsoWithZ`] produces `Z`.
        'x' => Iso = 4,
        /// Field symbol for either the ISO-8601 basic format or ISO-8601 extended format, with the ISO-8601 UTC indicator `Z`.
        'X' => IsoWithZ = 5,
    };
    TimeZoneULE
);

#[cfg(feature = "datagen")]
impl LengthType for TimeZone {
    fn get_length_type(self, _: FieldLength) -> TextOrNumeric {
        use TextOrNumeric::*;
        match self {
            Self::Iso | Self::IsoWithZ => Numeric,
            Self::LocalizedOffset
            | Self::SpecificNonLocation
            | Self::GenericNonLocation
            | Self::Location => Text,
        }
    }
}

/// A second field with fractional digits.
#[derive(
    Debug, Eq, PartialEq, Ord, PartialOrd, Clone, Copy, yoke::Yokeable, zerofrom::ZeroFrom,
)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::fields))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[allow(clippy::enum_variant_names)]
#[repr(u8)]
#[zerovec::make_ule(DecimalSecondULE)]
#[zerovec::derive(Debug)]
#[allow(clippy::exhaustive_enums)] // used in data struct
pub enum DecimalSecond {
    /// A second with 1 fractional digit: "1.0"
    Subsecond1 = 1,
    /// A second with 2 fractional digits: "1.00"
    Subsecond2 = 2,
    /// A second with 3 fractional digits: "1.000"
    Subsecond3 = 3,
    /// A second with 4 fractional digits: "1.0000"
    Subsecond4 = 4,
    /// A second with 5 fractional digits: "1.00000"
    Subsecond5 = 5,
    /// A second with 6 fractional digits: "1.000000"
    Subsecond6 = 6,
    /// A second with 7 fractional digits: "1.0000000"
    Subsecond7 = 7,
    /// A second with 8 fractional digits: "1.00000000"
    Subsecond8 = 8,
    /// A second with 9 fractional digits: "1.000000000"
    Subsecond9 = 9,
}

impl DecimalSecond {
    #[inline]
    pub(crate) fn idx(self) -> u8 {
        self as u8
    }
    #[inline]
    pub(crate) fn from_idx(idx: u8) -> Result<Self, SymbolError> {
        Self::new_from_u8(idx).ok_or(SymbolError::InvalidIndex(idx))
    }
}
impl From<DecimalSecond> for FieldSymbol {
    fn from(input: DecimalSecond) -> Self {
        Self::DecimalSecond(input)
    }
}
