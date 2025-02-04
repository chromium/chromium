// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Parsing of Time Values

use crate::{
    assert_syntax,
    parsers::{
        grammar::{
            is_annotation_open, is_decimal_separator, is_sign, is_time_designator,
            is_time_separator, is_utc_designator,
        },
        records::{Annotation, TimeRecord},
        timezone::parse_date_time_utc,
        Cursor,
    },
    ParseError, ParserResult,
};

use super::{annotations, records::IxdtfParseRecord};

/// Parse annotated time record is silently fallible returning None in the case that the
/// value does not align
pub(crate) fn parse_annotated_time_record<'a>(
    cursor: &mut Cursor<'a>,
    handler: impl FnMut(Annotation<'a>) -> Option<Annotation<'a>>,
) -> ParserResult<IxdtfParseRecord<'a>> {
    let designator = cursor.check_or(false, is_time_designator);
    cursor.advance_if(designator);

    let time = parse_time_record(cursor)?;

    // If Time was successfully parsed, assume from this point that this IS a
    // valid AnnotatedTimeRecord.

    let offset = if cursor.check_or(false, |ch| is_sign(ch) || is_utc_designator(ch)) {
        Some(parse_date_time_utc(cursor)?)
    } else {
        None
    };

    // Check if annotations exist.
    if !cursor.check_or(false, is_annotation_open) {
        cursor.close()?;

        return Ok(IxdtfParseRecord {
            date: None,
            time: Some(time),
            offset,
            tz: None,
            calendar: None,
        });
    }

    let annotations = annotations::parse_annotation_set(cursor, handler)?;

    cursor.close()?;

    Ok(IxdtfParseRecord {
        date: None,
        time: Some(time),
        offset,
        tz: annotations.tz,
        calendar: annotations.calendar,
    })
}

/// Parse `TimeRecord`
pub(crate) fn parse_time_record(cursor: &mut Cursor) -> ParserResult<TimeRecord> {
    let hour = parse_hour(cursor)?;

    if !cursor.check_or(false, |ch| is_time_separator(ch) || ch.is_ascii_digit()) {
        return Ok(TimeRecord {
            hour,
            minute: 0,
            second: 0,
            nanosecond: 0,
        });
    }

    let separator_present = cursor.check_or(false, is_time_separator);
    cursor.advance_if(separator_present);

    let minute = parse_minute_second(cursor, false)?;

    if !cursor.check_or(false, |ch| is_time_separator(ch) || ch.is_ascii_digit()) {
        return Ok(TimeRecord {
            hour,
            minute,
            second: 0,
            nanosecond: 0,
        });
    }

    let second_separator = cursor.check_or(false, is_time_separator);
    assert_syntax!(separator_present == second_separator, TimeSeparator);
    cursor.advance_if(second_separator);

    let second = parse_minute_second(cursor, true)?;

    let nanosecond = parse_fraction(cursor)?.unwrap_or(0);

    Ok(TimeRecord {
        hour,
        minute,
        second,
        nanosecond,
    })
}

/// Parse an hour value.
#[inline]
pub(crate) fn parse_hour(cursor: &mut Cursor) -> ParserResult<u8> {
    let first = cursor.next_digit()?.ok_or(ParseError::TimeHour)?;
    let hour_value = first * 10 + cursor.next_digit()?.ok_or(ParseError::TimeHour)?;
    if !(0..=23).contains(&hour_value) {
        return Err(ParseError::TimeHour);
    }
    Ok(hour_value)
}

/// Parses `MinuteSecond` value.
#[inline]
pub(crate) fn parse_minute_second(cursor: &mut Cursor, is_second: bool) -> ParserResult<u8> {
    let (valid_range, err) = if is_second {
        (0..=60, ParseError::TimeSecond)
    } else {
        (0..=59, ParseError::TimeMinute)
    };
    let first = cursor.next_digit()?.ok_or(err)?;
    let min_sec_value = first * 10 + cursor.next_digit()?.ok_or(err)?;
    if !valid_range.contains(&min_sec_value) {
        return Err(err);
    }
    Ok(min_sec_value)
}

/// Parse a `Fraction` value
///
/// This is primarily used in ISO8601 to add percision past
/// a second.
#[inline]
pub(crate) fn parse_fraction(cursor: &mut Cursor) -> ParserResult<Option<u32>> {
    // Assert that the first char provided is a decimal separator.
    if !cursor.check_or(false, is_decimal_separator) {
        return Ok(None);
    }
    cursor.next_or(ParseError::FractionPart)?;

    let mut result = 0;
    let mut fraction_len = 0;
    while cursor.check_or(false, |ch| ch.is_ascii_digit()) {
        if fraction_len > 9 {
            return Err(ParseError::FractionPart);
        }
        result = result * 10 + u32::from(cursor.next_digit()?.ok_or(ParseError::FractionPart)?);
        fraction_len += 1;
    }

    // Assert: 10^9-1 should always be a valid u32.
    let result = result
        * 10u32
            .checked_pow(9 - fraction_len)
            .ok_or(ParseError::ImplAssert)?;

    Ok(Some(result))
}
