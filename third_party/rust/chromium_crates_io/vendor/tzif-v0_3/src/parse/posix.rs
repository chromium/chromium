// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::ensure;
use crate::data::posix::{
    DstTransitionInfo, PosixTzString, TransitionDate, TransitionDay, ZoneVariantInfo,
};
use crate::data::time::{Hours, Minutes, Seconds};
use combine::parser::byte::{byte, digit};
use combine::{between, choice, many, many1, optional, satisfy, value, ParseError, Parser, Stream};

/// Parses a byte that not a digit, a comma, a plus nor a minus signs.
fn alphabetic_zone_variant_name_value<Input>() -> impl Parser<Input, Output = u8>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    satisfy(|byte: u8| {
        byte.is_ascii_alphabetic() && !matches!(byte, b',' | b'+' | b'-' | b'0'..=b'9')
    })
}

/// Parses a string that specifies the name of the time zone variant.
/// It must not contain embedded digits, commas, nor plus and minus signs.
fn alphabetic_zone_variant_name<Input>() -> impl Parser<Input, Output = String>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    many(alphabetic_zone_variant_name_value())
        .map(|bytes: Vec<u8>| String::from_utf8_lossy(&bytes).into_owned())
}

/// Parses an arbitrary time-zone variant name. This name must be enclosed in angled brackets, e.g. `<name>`.
fn arbitrary_zone_variant_name<Input>() -> impl Parser<Input, Output = String>
where
    Input: Stream<Token = u8>,
{
    between(
        byte(b'<'),
        byte(b'>'),
        many1::<Vec<u8>, _, _>(satisfy(|byte| byte != b'>')),
    )
    .map(|name| String::from_utf8_lossy(&name).into_owned())
}

/// The string specifies the name of the time zone variant. It must be three or more characters
/// long and must not contain a leading colon.
///
/// The name must not contain embedded digits, commas, plus nor minus signs unless it is specified
/// as an arbitrary name surrounded by angled brackets, e.g. `<name>`
///
/// The angled brackets will not show up in the parsed name.
fn zone_variant_name<Input>() -> impl Parser<Input, Output = String>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    choice((
        arbitrary_zone_variant_name(),
        alphabetic_zone_variant_name(),
    ))
    .then(|name| {
        ensure(
            name,
            |name| name.len() >= 3,
            "zone variant name should be 3 or more characters long",
        )
    })
    .then(|name| {
        ensure(
            name,
            |name| name.as_bytes()[0] != b':',
            "zone variant name should never start with a leading colon",
        )
    })
}

/// Parses a plus or minus sign.
fn sign<Input>() -> impl Parser<Input, Output = u8>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    byte(b'+').or(byte(b'-'))
}

/// Parses one ore more digits.
fn digits<Input>() -> impl Parser<Input, Output = Vec<u8>>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    many1(digit())
}

/// Parses a natural number as an i64.
fn natural<Input>() -> impl Parser<Input, Output = i64>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    digits().map(|digits| {
        digits
            .into_iter()
            .map(|digit| i64::from(digit - b'0'))
            .rev()
            .zip(0u32..)
            .map(|(digit, n)| digit * 10i64.pow(n))
            .sum::<i64>()
    })
}

/// Parses a natural number as an i64 and esnures that it falls within `[lower_bound, upper_bound]`.
fn bounded_natural<Input>(lower_bound: i64, upper_bound: i64) -> impl Parser<Input, Output = i64>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    debug_assert!(
        lower_bound <= upper_bound,
        "lower bound {lower_bound} was not less than or equal, upper bound {upper_bound}"
    );
    natural().then(move |natural| {
        ensure(
            natural,
            |&natural| lower_bound <= natural && natural <= upper_bound,
            "parsed natural number is out of bounds",
        )
    })
}

/// Parses an integer as an i64 and esnures that it falls within `[lower_bound, upper_bound]`.
fn bounded_integer<Input>(lower_bound: i64, upper_bound: i64) -> impl Parser<Input, Output = i64>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    assert!(
        lower_bound <= upper_bound,
        "lower bound {lower_bound} was not less than or equal, upper bound {upper_bound}"
    );
    (optional(sign()), natural())
        .map(|(sign, natural)| natural * if matches!(sign, Some(b'-')) { -1 } else { 1 })
        .then(move |integer| {
            ensure(
                integer,
                |&integer| lower_bound <= integer && integer <= upper_bound,
                "parsed bounded integer is out of bounds",
            )
        })
}

/// Parses an integer as [`Hours`] and ensures that it falls within `[-bound, bound]`.
fn hours<Input>(bound: i64) -> impl Parser<Input, Output = Hours>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    bounded_integer(-bound, bound).map(Hours)
}

/// Parses an integer as [`Minutes`] and ensures that it falls within `[0, 59]`.
fn minutes<Input>() -> impl Parser<Input, Output = Minutes>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    bounded_natural(0, 59).map(Minutes)
}

/// Parses an integer as [`Seconds`] and ensures that it falls within `[0, 59]`.
fn seconds<Input>() -> impl Parser<Input, Output = Seconds>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    bounded_natural(0, 59).map(Seconds)
}

/// Parses a minutes segment (`:mm`) as part of an `hh:mm:ss` time stamp.
/// The parsed [`Minutes`] must be within range `[0, 59]`.
///
/// If there is no leading colon, defaults to zero minutes.
fn mm_segment<Input>() -> impl Parser<Input, Output = Minutes>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    optional(byte(b':')).then(|colon| {
        if colon.is_some() {
            minutes().left()
        } else {
            value(Minutes(0)).right()
        }
    })
}

/// Parses a seconds segment (`:ss`) as part of an `hh:mm:ss` time stamp.
/// The parsed [`Seconds`] must be within range `[0, 59]`.
///
/// If there is no leading colon, defaults to zero minutes.
fn ss_segment<Input>() -> impl Parser<Input, Output = Seconds>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    optional(byte(b':')).then(|colon| {
        if colon.is_some() {
            seconds().left()
        } else {
            value(Seconds(0)).right()
        }
    })
}

/// Parses a time value of the form `\[+|-\]hh\[:mm\[:ss\]\]`.
///
/// Returns the total time in [`Seconds`].
fn time<Input>(hour_bound: i64) -> impl Parser<Input, Output = Seconds>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    (hours(hour_bound), mm_segment(), ss_segment()).map(|(hours, minutes, seconds)| {
        if hours < Hours(0) {
            hours.as_seconds() - minutes.as_seconds() - seconds
        } else {
            hours.as_seconds() + minutes.as_seconds() + seconds
        }
    })
}

/// Parses a time value of the form `\[+|-\]hh\[:mm\[:ss\]\]`.
///
/// This is positive if the local time zone is west of the Prime Meridian and negative if it is east.
/// The hour must be in range `[0, 24]`, and the minute and seconds must be in range `[0, 59]`.
///
/// Returns the total time in [`Seconds`].
fn offset_time<Input>() -> impl Parser<Input, Output = Seconds>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    time(24)
}

/// Parses the STD time-zone variant info including the variant name and the offset in seconds.
///
/// See [`zone_variant_name`] and [`offset_time`] for more information.
fn std_variant_info<Input>() -> impl Parser<Input, Output = ZoneVariantInfo>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    combine::struct_parser! {
        ZoneVariantInfo {
            name: zone_variant_name(),
            offset: offset_time(),
        }
    }
}

/// Parses the DST time-zone variant info including the variant name and the offset in seconds.
///
/// This differs from [`std_variant_info`] in that it takes a predetermined STD offset
/// offset as an argument. If no explicit DST offset is parsed, it will default to the
/// STD offset minus one hour.
fn dst_variant_info<Input>(std_offset: Seconds) -> impl Parser<Input, Output = ZoneVariantInfo>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    combine::struct_parser! {
        ZoneVariantInfo {
            name: zone_variant_name(),
            offset: optional(offset_time()).map(move |time| time.unwrap_or(std_offset - Hours(1).as_seconds())),
        }
    }
}

/// Parses transition day with no specified leading character, e.g. `15`.
/// This is a value in range `[0, 365]` in which the leap day is considered for leap years.
fn transition_day_n<Input>() -> impl Parser<Input, Output = TransitionDay>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    bounded_natural(0, 365).map(|natural| TransitionDay::WithLeap(natural as u16))
}

/// Parses transition day specified by a leading `J`, e.g. `J15`.
/// This is a value in range `[1, 365]` in which the leap day is never considered.
fn transition_day_jn<Input>() -> impl Parser<Input, Output = TransitionDay>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    byte(b'J').with(bounded_natural(1, 365).map(|natural| TransitionDay::NoLeap(natural as u16)))
}

/// Parses a transition date specified by a leading `M`, e.g. `Mm.w.d`
/// This specifies day `d` of week `w` of month `m`. The day `d` must be between
/// 0 (Sunday) and 6 (Saturday). The week `w` must be in range `[1, 5]`;
/// The week that corresponds to `1` is the first week in which day `d` occurs,
/// and week 5 specifies the last `d` day in the month. The month `m` should
/// be between 1 and 12.
fn transition_day_mwd<Input>() -> impl Parser<Input, Output = TransitionDay>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    byte(b'M')
        .with((
            bounded_natural(1, 12),
            byte(b'.').with(bounded_natural(1, 5)),
            byte(b'.').with(bounded_natural(0, 6)),
        ))
        .map(|(m, w, d)| TransitionDay::Mwd(m as u16, w as u16, d as u16))
}

/// Parses a transition day in any of three unambiguous formats.
///
/// See [`transition_day_mwd`], [`transition_day_jn`], and [`transition_day_n`]
/// for more information.
fn transition_day<Input>() -> impl Parser<Input, Output = TransitionDay>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    choice((
        transition_day_mwd(),
        transition_day_jn(),
        transition_day_n(),
    ))
}

/// Parses a time value of the form `\[+|-\]hh\[:mm\[:ss\]\]`.
///
/// This is positive if the local time zone is west of the Prime Meridian and negative if it is east.
/// The hour must be in range `[-167, 167]`, and the minute and seconds must be in range `[0, 59]`.
/// This is an extension to POSIX.1, which only allows hours to be in range `[0, 24]`.
fn transition_time<Input>() -> impl Parser<Input, Output = Seconds>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    optional(byte(b'/')).then(|slash| {
        if slash.is_some() {
            time(167).left()
        } else {
            value(Hours(2).as_seconds()).right()
        }
    })
}

/// Parses a DST transition date including the transition day and the transition time in seconds.
///
/// See [`transition_day`] and [`transition_time`] for more information.
fn transition_date<Input>() -> impl Parser<Input, Output = TransitionDate>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    combine::struct_parser! {
        TransitionDate {
            day: transition_day(),
            time: transition_time(),
        }
    }
}

/// Parses DST transition information including the variant info, and the transition dates.
///
/// See [`dst_variant_info`], [`transition_date`] for more information.
fn dst_transition_info<Input>(std_offset: Seconds) -> impl Parser<Input, Output = DstTransitionInfo>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    combine::struct_parser! {
        DstTransitionInfo {
            variant_info: dst_variant_info(std_offset),
            start_date: byte(b',').with(transition_date()),
            end_date: byte(b',').with(transition_date()),
        }
    }
}

/// Parses a POSIX time-zone string according to the following specification:
/// <https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html>
#[must_use]
pub fn posix_tz_string<Input>() -> impl Parser<Input, Output = PosixTzString>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    std_variant_info().then(|std_info| {
        let std_offset = std_info.offset;
        combine::struct_parser! {
            PosixTzString {
                std_info: value(std_info),
                dst_info: optional(dst_transition_info(std_offset)),
            }
        }
    })
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::{assert_parse_eq, assert_parse_err};
    use combine::EasyParser;

    #[test]
    fn parse_zone_variant_name() {
        // invalid alphabetic names
        assert_parse_err!(zone_variant_name(), "");
        assert_parse_err!(zone_variant_name(), "a");
        assert_parse_err!(zone_variant_name(), "ab");
        assert_parse_err!(zone_variant_name(), "ab,");
        assert_parse_err!(zone_variant_name(), "ab+");
        assert_parse_err!(zone_variant_name(), "ab-");
        assert_parse_err!(zone_variant_name(), "ab0");
        assert_parse_err!(zone_variant_name(), "ab1");
        assert_parse_err!(zone_variant_name(), "ab2");
        assert_parse_err!(zone_variant_name(), "ab3");
        assert_parse_err!(zone_variant_name(), "ab4");
        assert_parse_err!(zone_variant_name(), "ab5");
        assert_parse_err!(zone_variant_name(), "ab6");
        assert_parse_err!(zone_variant_name(), "ab7");
        assert_parse_err!(zone_variant_name(), "ab8");
        assert_parse_err!(zone_variant_name(), "ab9");
        assert_parse_err!(zone_variant_name(), ":bc");
        assert_parse_err!(zone_variant_name(), ":abc");
        assert_parse_err!(zone_variant_name(), ":PDT");

        // invalid arbitrary names
        assert_parse_err!(zone_variant_name(), "<>");
        assert_parse_err!(zone_variant_name(), "<a>");
        assert_parse_err!(zone_variant_name(), "<ab>");
        assert_parse_err!(zone_variant_name(), "<:abc>");
        assert_parse_err!(zone_variant_name(), "<:PDT>");

        // valid alphabetic names
        assert_parse_eq!(zone_variant_name(), "abc", "abc");
        assert_parse_eq!(zone_variant_name(), "PST", "PST");
        assert_parse_eq!(zone_variant_name(), "PDT", "PDT");
        assert_parse_eq!(zone_variant_name(), "GMT", "GMT");
        assert_parse_eq!(zone_variant_name(), "PST8PDT", "PST");

        // valid arbitrary names
        assert_parse_eq!(zone_variant_name(), "<1,bc>", "1,bc");
        assert_parse_eq!(zone_variant_name(), "<+800>", "+800");
        assert_parse_eq!(zone_variant_name(), "<-800>", "-800");
        assert_parse_eq!(zone_variant_name(), "<PST8PDT>", "PST8PDT");
    }

    #[test]
    fn parse_natural() {
        // invalid integers
        assert_parse_err!(natural(), "");
        assert_parse_err!(natural(), "+");
        assert_parse_err!(natural(), "-");
        assert_parse_err!(natural(), "a");
        assert_parse_err!(natural(), "ab");
        assert_parse_err!(natural(), "-1");

        // out of bounds
        assert_parse_err!(bounded_natural(1, 9), "0");
        assert_parse_err!(bounded_natural(2, 9), "1");
        assert_parse_err!(bounded_natural(3, 9), "2");

        assert_parse_err!(bounded_natural(0, 9), "10");
        assert_parse_err!(bounded_natural(0, 8), "9");
        assert_parse_err!(bounded_natural(0, 7), "8");

        // valid single digits
        assert_parse_eq!(bounded_natural(0, 9), "0", 0);
        assert_parse_eq!(bounded_natural(0, 9), "1", 1);
        assert_parse_eq!(bounded_natural(0, 9), "2", 2);
        assert_parse_eq!(bounded_natural(0, 9), "3", 3);
        assert_parse_eq!(bounded_natural(0, 9), "4", 4);
        assert_parse_eq!(bounded_natural(0, 9), "5", 5);
        assert_parse_eq!(bounded_natural(0, 9), "6", 6);
        assert_parse_eq!(bounded_natural(0, 9), "7", 7);
        assert_parse_eq!(bounded_natural(0, 9), "8", 8);
        assert_parse_eq!(bounded_natural(0, 9), "9", 9);

        // valid naturals
        assert_parse_eq!(natural(), "01", 1);
        assert_parse_eq!(natural(), "002", 2);
        assert_parse_eq!(natural(), "13", 13);
        assert_parse_eq!(natural(), "4321", 4321);
        assert_parse_eq!(natural(), "0543-21", 543);
        assert_parse_eq!(natural(), "0543+21", 543);
        assert_parse_eq!(natural(), "12345abc", 12345);
    }

    #[test]
    fn parse_integer() {
        // invalid integers
        assert_parse_err!(bounded_integer(i64::MIN, i64::MAX), "");
        assert_parse_err!(bounded_integer(i64::MIN, i64::MAX), "+");
        assert_parse_err!(bounded_integer(i64::MIN, i64::MAX), "-");
        assert_parse_err!(bounded_integer(i64::MIN, i64::MAX), "a");
        assert_parse_err!(bounded_integer(i64::MIN, i64::MAX), "ab");
        assert_parse_err!(bounded_integer(i64::MIN, i64::MAX), "--1");
        assert_parse_err!(bounded_integer(i64::MIN, i64::MAX), "++1");
        assert_parse_err!(bounded_integer(i64::MIN, i64::MAX), "+-1");
        assert_parse_err!(bounded_integer(i64::MIN, i64::MAX), "-+1");

        // out of bounds
        assert_parse_err!(bounded_integer(0, 9), "-1");
        assert_parse_err!(bounded_integer(1, 9), "0");
        assert_parse_err!(bounded_integer(2, 9), "1");

        assert_parse_err!(bounded_integer(0, 7), "8");
        assert_parse_err!(bounded_integer(0, 8), "9");
        assert_parse_err!(bounded_integer(0, 9), "10");

        // valid single digits
        assert_parse_eq!(bounded_integer(0, 9), "0", 0);
        assert_parse_eq!(bounded_integer(0, 9), "1", 1);
        assert_parse_eq!(bounded_integer(0, 9), "2", 2);
        assert_parse_eq!(bounded_integer(0, 9), "3", 3);
        assert_parse_eq!(bounded_integer(0, 9), "4", 4);
        assert_parse_eq!(bounded_integer(0, 9), "5", 5);
        assert_parse_eq!(bounded_integer(0, 9), "6", 6);
        assert_parse_eq!(bounded_integer(0, 9), "7", 7);
        assert_parse_eq!(bounded_integer(0, 9), "8", 8);
        assert_parse_eq!(bounded_integer(0, 9), "9", 9);

        // valid integers
        assert_parse_eq!(bounded_integer(i64::MIN, i64::MAX), "+1", 1);
        assert_parse_eq!(bounded_integer(i64::MIN, i64::MAX), "-5", -5);
        assert_parse_eq!(bounded_integer(i64::MIN, i64::MAX), "01", 1);
        assert_parse_eq!(bounded_integer(i64::MIN, i64::MAX), "002", 2);
        assert_parse_eq!(bounded_integer(i64::MIN, i64::MAX), "13", 13);
        assert_parse_eq!(bounded_integer(i64::MIN, i64::MAX), "4321", 4321);
        assert_parse_eq!(bounded_integer(i64::MIN, i64::MAX), "+0543-21", 543);
        assert_parse_eq!(bounded_integer(i64::MIN, i64::MAX), "-0543+21", -543);
        assert_parse_eq!(bounded_integer(i64::MIN, i64::MAX), "-12345abc", -12345);
    }

    #[test]
    fn parse_hours() {
        // out of bounds [-2, 2]
        assert_parse_err!(hours(2), "4");
        assert_parse_err!(hours(2), "3");
        assert_parse_err!(hours(2), "-4");
        assert_parse_err!(hours(2), "-3");

        // within bounds [-2, 2]
        assert_parse_eq!(hours(2), "2", Hours(2));
        assert_parse_eq!(hours(2), "1", Hours(1));
        assert_parse_eq!(hours(2), "0", Hours(0));
        assert_parse_eq!(hours(2), "+2", Hours(2));
        assert_parse_eq!(hours(2), "-2", Hours(-2));
        assert_parse_eq!(hours(2), "-1", Hours(-1));
        assert_parse_eq!(hours(2), "-0", Hours(0));
        assert_parse_eq!(hours(2), "+0", Hours(0));
    }

    #[test]
    fn parse_minutes() {
        // out of bounds [0, 60]
        assert_parse_err!(minutes(), "-2");
        assert_parse_err!(minutes(), "-1");
        assert_parse_err!(minutes(), "61");
        assert_parse_err!(minutes(), "60");

        // within bounds [0, 60]
        assert_parse_eq!(minutes(), "00", Minutes(0));
        assert_parse_eq!(minutes(), "01", Minutes(1));
        assert_parse_eq!(minutes(), "30", Minutes(30));
        assert_parse_eq!(minutes(), "58", Minutes(58));
        assert_parse_eq!(minutes(), "59", Minutes(59));
    }

    #[test]
    fn parse_seconds() {
        // out of bounds [0, 60]
        assert_parse_err!(seconds(), "-2");
        assert_parse_err!(seconds(), "-1");
        assert_parse_err!(seconds(), "61");
        assert_parse_err!(seconds(), "60");

        // within bounds [0, 60]
        assert_parse_eq!(seconds(), "00", Seconds(0));
        assert_parse_eq!(seconds(), "01", Seconds(1));
        assert_parse_eq!(seconds(), "30", Seconds(30));
        assert_parse_eq!(seconds(), "58", Seconds(58));
        assert_parse_eq!(seconds(), "59", Seconds(59));
    }

    #[test]
    fn parse_time() {
        // hours out of bounds
        assert_parse_err!(time(12), "13");
        assert_parse_err!(time(12), "-13");
        assert_parse_err!(time(12), "13:00");
        assert_parse_err!(time(12), "-13:00");
        assert_parse_err!(time(12), "13:00:00");
        assert_parse_err!(time(12), "-13:00:00");

        // minutes out of bounds
        assert_parse_err!(time(12), "00:60");
        assert_parse_err!(time(12), "00:60:00");

        // seconds out of bounds
        assert_parse_err!(time(12), "00:00:60");

        // valid times
        assert_parse_eq!(time(12), "12", Seconds(12 * 60 * 60));
        assert_parse_eq!(time(12), "+12", Seconds(12 * 60 * 60));
        assert_parse_eq!(time(12), "-12", Seconds(-12 * 60 * 60));
        assert_parse_eq!(time(12), "12:15", Seconds(12 * 60 * 60 + 15 * 60));
        assert_parse_eq!(time(12), "-12:15", Seconds(-12 * 60 * 60 - 15 * 60));
        assert_parse_eq!(time(12), "12:30:15", Seconds(12 * 60 * 60 + 30 * 60 + 15));
        assert_parse_eq!(time(12), "-12:30:15", Seconds(-12 * 60 * 60 - 30 * 60 - 15));
    }

    #[test]
    fn parse_std_variant_info() {
        // invalid zone variant info name
        assert_parse_err!(std_variant_info(), "");
        assert_parse_err!(std_variant_info(), "P");
        assert_parse_err!(std_variant_info(), "PS");
        assert_parse_err!(std_variant_info(), ":PST8");

        // invalid zone variant info hour
        assert_parse_err!(std_variant_info(), "PST25");
        assert_parse_err!(std_variant_info(), "PST-25");

        // invalid zone variant info minute
        assert_parse_err!(std_variant_info(), "PST8:60");
        assert_parse_err!(std_variant_info(), "PST8:60");

        // invalid zone variant info second
        assert_parse_err!(std_variant_info(), "PST8:00:60");
        assert_parse_err!(std_variant_info(), "PST8:00:60");

        // valid zone variant infos
        assert_parse_eq!(
            std_variant_info(),
            "EST+5",
            ZoneVariantInfo {
                name: "EST".to_owned(),
                offset: Hours(5).as_seconds(),
            }
        );
        assert_parse_eq!(
            std_variant_info(),
            "IST-2",
            ZoneVariantInfo {
                name: "IST".to_owned(),
                offset: Hours(-2).as_seconds(),
            }
        );
        assert_parse_eq!(
            std_variant_info(),
            "<0made+up0>-24:59:59",
            ZoneVariantInfo {
                name: "0made+up0".to_owned(),
                offset: Hours(-24).as_seconds() - Minutes(59).as_seconds() - Seconds(59)
            }
        );
    }

    #[test]
    fn parse_dst_variant_info() {
        // invalid zone variant info name
        assert_parse_err!(dst_variant_info(Seconds(0)), "");
        assert_parse_err!(dst_variant_info(Seconds(0)), "P");
        assert_parse_err!(dst_variant_info(Seconds(0)), "PD");
        assert_parse_err!(dst_variant_info(Seconds(0)), ":PDT8");

        // invalid zone variant info hour
        assert_parse_err!(dst_variant_info(Seconds(0)), "PDT25");
        assert_parse_err!(dst_variant_info(Seconds(0)), "PDT-25");

        // invalid zone variant info minute
        assert_parse_err!(dst_variant_info(Seconds(0)), "PDT8:60");
        assert_parse_err!(dst_variant_info(Seconds(0)), "PDT8:60");

        // invalid zone variant info second
        assert_parse_err!(dst_variant_info(Seconds(0)), "PDT8:00:60");
        assert_parse_err!(dst_variant_info(Seconds(0)), "PDT8:00:60");

        // valid zone variant infos
        assert_parse_eq!(
            dst_variant_info(Hours(5).as_seconds()),
            "EDT",
            ZoneVariantInfo {
                name: "EDT".to_owned(),
                offset: Hours(4).as_seconds(),
            }
        );
        assert_parse_eq!(
            dst_variant_info(Hours(-2).as_seconds()),
            "IDT",
            ZoneVariantInfo {
                name: "IDT".to_owned(),
                offset: Hours(-3).as_seconds(),
            }
        );
        assert_parse_eq!(
            dst_variant_info(Seconds(0)),
            "<0made+up0>-24:59:59",
            ZoneVariantInfo {
                name: "0made+up0".to_owned(),
                offset: Hours(-24).as_seconds() - Minutes(59).as_seconds() - Seconds(59)
            }
        );
    }

    #[test]
    fn parse_transition_day() {
        // invalid transition days
        assert_parse_err!(transition_day(), "");
        assert_parse_err!(transition_day(), ":5");
        assert_parse_err!(transition_day(), "A65");

        // invalid out of bounds
        assert_parse_err!(transition_day(), "-1");
        assert_parse_err!(transition_day(), "366");
        assert_parse_err!(transition_day(), "J0");
        assert_parse_err!(transition_day(), "J366");
        assert_parse_err!(transition_day(), "M0.1.0");
        assert_parse_err!(transition_day(), "M13.1.0");
        assert_parse_err!(transition_day(), "M1.0.0");
        assert_parse_err!(transition_day(), "M1.6.0");
        assert_parse_err!(transition_day(), "M1.1.7");

        // valid transition days
        assert_parse_eq!(transition_day(), "0", TransitionDay::WithLeap(0));
        assert_parse_eq!(transition_day(), "365", TransitionDay::WithLeap(365));
        assert_parse_eq!(transition_day(), "J1", TransitionDay::NoLeap(1));
        assert_parse_eq!(transition_day(), "J365", TransitionDay::NoLeap(365));
        assert_parse_eq!(transition_day(), "M1.1.0", TransitionDay::Mwd(1, 1, 0));
        assert_parse_eq!(transition_day(), "M12.5.6", TransitionDay::Mwd(12, 5, 6));
    }

    #[test]
    fn parse_transition_date() {
        assert_parse_eq!(
            transition_date(),
            "0",
            TransitionDate {
                day: TransitionDay::WithLeap(0),
                time: Hours(2).as_seconds(),
            }
        );
        assert_parse_eq!(
            transition_date(),
            "M3.4.4/26",
            TransitionDate {
                day: TransitionDay::Mwd(3, 4, 4),
                time: Hours(26).as_seconds(),
            }
        );
        assert_parse_eq!(
            transition_date(),
            "J365/25",
            TransitionDate {
                day: TransitionDay::NoLeap(365),
                time: Hours(25).as_seconds(),
            }
        );
    }

    #[test]
    fn parse_dst_transition_info() {
        assert_parse_eq!(
            dst_transition_info(Hours(4).as_seconds()),
            "WARST,J1/0,J365/25",
            DstTransitionInfo {
                variant_info: ZoneVariantInfo {
                    name: "WARST".to_owned(),
                    offset: Hours(3).as_seconds()
                },
                start_date: TransitionDate {
                    day: TransitionDay::NoLeap(1),
                    time: Seconds(0)
                },
                end_date: TransitionDate {
                    day: TransitionDay::NoLeap(365),
                    time: Hours(25).as_seconds()
                },
            }
        );
        assert_parse_eq!(
            dst_transition_info(Hours(-2).as_seconds()),
            "IDT,M3.4.4/26,M10.5.0",
            DstTransitionInfo {
                variant_info: ZoneVariantInfo {
                    name: "IDT".to_owned(),
                    offset: Hours(-3).as_seconds()
                },
                start_date: TransitionDate {
                    day: TransitionDay::Mwd(3, 4, 4),
                    time: Hours(26).as_seconds(),
                },
                end_date: TransitionDate {
                    day: TransitionDay::Mwd(10, 5, 0),
                    time: Hours(2).as_seconds()
                },
            }
        );
    }

    #[test]
    fn parse_posix_tz_string() {
        assert_parse_eq!(
            posix_tz_string(),
            "WGT3WGST,M3.5.0/-2,M10.5.0/-1",
            PosixTzString {
                std_info: ZoneVariantInfo {
                    name: "WGT".to_owned(),
                    offset: Hours(3).as_seconds(),
                },
                dst_info: Some(DstTransitionInfo {
                    variant_info: ZoneVariantInfo {
                        name: "WGST".to_owned(),
                        offset: Hours(2).as_seconds()
                    },
                    start_date: TransitionDate {
                        day: TransitionDay::Mwd(3, 5, 0),
                        time: Hours(-2).as_seconds(),
                    },
                    end_date: TransitionDate {
                        day: TransitionDay::Mwd(10, 5, 0),
                        time: Hours(-1).as_seconds(),
                    },
                })
            }
        );
        assert_parse_eq!(
            posix_tz_string(),
            "WART4WARST,J1/0,J365/25",
            PosixTzString {
                std_info: ZoneVariantInfo {
                    name: "WART".to_owned(),
                    offset: Hours(4).as_seconds(),
                },
                dst_info: Some(DstTransitionInfo {
                    variant_info: ZoneVariantInfo {
                        name: "WARST".to_owned(),
                        offset: Hours(3).as_seconds()
                    },
                    start_date: TransitionDate {
                        day: TransitionDay::NoLeap(1),
                        time: Seconds(0),
                    },
                    end_date: TransitionDate {
                        day: TransitionDay::NoLeap(365),
                        time: Hours(25).as_seconds(),
                    },
                })
            }
        );
    }
}
