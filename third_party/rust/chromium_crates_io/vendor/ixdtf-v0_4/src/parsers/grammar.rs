// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! ISO8601 specific grammar checks.

/// Checks if char is a `AKeyLeadingChar`.
#[inline]
pub(crate) const fn is_a_key_leading_char(ch: char) -> bool {
    ch.is_ascii_lowercase() || ch == '_'
}

/// Checks if char is an `AKeyChar`.
#[inline]
pub(crate) const fn is_a_key_char(ch: char) -> bool {
    is_a_key_leading_char(ch) || ch.is_ascii_digit() || ch == '-'
}

/// Checks if char is an `AnnotationValueComponent`.
pub(crate) const fn is_annotation_value_component(ch: char) -> bool {
    ch.is_ascii_digit() || ch.is_ascii_alphabetic()
}

/// Checks if char is a `TZLeadingChar`.
#[inline]
pub(crate) const fn is_tz_leading_char(ch: char) -> bool {
    ch.is_ascii_alphabetic() || ch == '_' || ch == '.'
}

/// Checks if char is a `TZChar`.
#[inline]
pub(crate) const fn is_tz_char(ch: char) -> bool {
    is_tz_leading_char(ch) || ch.is_ascii_digit() || ch == '-' || ch == '+'
}

/// Checks if char is a `TimeZoneIANAName` Separator.
#[inline]
pub(crate) const fn is_tz_name_separator(ch: char) -> bool {
    ch == '/'
}

/// Checks if char is an ascii sign.
#[inline]
pub(crate) const fn is_ascii_sign(ch: char) -> bool {
    ch == '+' || ch == '-'
}

/// Checks if char is an ascii sign or U+2212
#[inline]
pub(crate) const fn is_sign(ch: char) -> bool {
    is_ascii_sign(ch) || ch == '\u{2212}'
}

/// Checks if char is a `TimeSeparator`.
#[inline]
pub(crate) const fn is_time_separator(ch: char) -> bool {
    ch == ':'
}

/// Checks if char is a `TimeDesignator`.
#[inline]
pub(crate) const fn is_time_designator(ch: char) -> bool {
    ch == 'T' || ch == 't'
}

#[inline]
/// Checks if char is a space.
pub(crate) const fn is_space(ch: char) -> bool {
    ch == ' '
}

/// Checks if char is a `DateTimeSeparator`.
#[inline]
pub(crate) const fn is_date_time_separator(ch: char) -> bool {
    is_time_designator(ch) || is_space(ch)
}

/// Checks if char is a `UtcDesignator`.
#[inline]
pub(crate) const fn is_utc_designator(ch: char) -> bool {
    ch == 'Z' || ch == 'z'
}

/// Checks if char is a `DurationDesignator`.
#[inline]
#[cfg(feature = "duration")]
pub(crate) const fn is_duration_designator(ch: char) -> bool {
    ch == 'P' || ch == 'p'
}

/// Checks if char is a `YearDesignator`.
#[inline]
#[cfg(feature = "duration")]
pub(crate) const fn is_year_designator(ch: char) -> bool {
    ch == 'Y' || ch == 'y'
}

/// Checks if char is a `MonthsDesignator`.
#[inline]
#[cfg(feature = "duration")]
pub(crate) const fn is_month_designator(ch: char) -> bool {
    ch == 'M' || ch == 'm'
}

/// Checks if char is a `WeekDesignator`.
#[inline]
#[cfg(feature = "duration")]
pub(crate) const fn is_week_designator(ch: char) -> bool {
    ch == 'W' || ch == 'w'
}

/// Checks if char is a `DayDesignator`.
#[inline]
#[cfg(feature = "duration")]
pub(crate) const fn is_day_designator(ch: char) -> bool {
    ch == 'D' || ch == 'd'
}

/// checks if char is a `DayDesignator`.
#[inline]
#[cfg(feature = "duration")]
pub(crate) const fn is_hour_designator(ch: char) -> bool {
    ch == 'H' || ch == 'h'
}

/// Checks if char is a `MinuteDesignator`.
#[inline]
#[cfg(feature = "duration")]
pub(crate) const fn is_minute_designator(ch: char) -> bool {
    is_month_designator(ch)
}

/// checks if char is a `SecondDesignator`.
#[inline]
#[cfg(feature = "duration")]
pub(crate) const fn is_second_designator(ch: char) -> bool {
    ch == 'S' || ch == 's'
}

/// Checks if char is a `DecimalSeparator`.
#[inline]
pub(crate) const fn is_decimal_separator(ch: char) -> bool {
    ch == '.' || ch == ','
}

/// Checks if char is an `AnnotationOpen`.
#[inline]
pub(crate) const fn is_annotation_open(ch: char) -> bool {
    ch == '['
}

/// Checks if char is an `AnnotationClose`.
#[inline]
pub(crate) const fn is_annotation_close(ch: char) -> bool {
    ch == ']'
}

/// Checks if char is an `CriticalFlag`.
#[inline]
pub(crate) const fn is_critical_flag(ch: char) -> bool {
    ch == '!'
}

/// Checks if char is the `AnnotationKeyValueSeparator`.
#[inline]
pub(crate) const fn is_annotation_key_value_separator(ch: char) -> bool {
    ch == '='
}

/// Checks if char is a hyphen. Hyphens are used as a Date separator
/// and as a `AttributeValueComponent` separator.
#[inline]
pub(crate) const fn is_hyphen(ch: char) -> bool {
    ch == '-'
}
