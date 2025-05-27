// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::ensure;
use super::posix::posix_tz_string;
use crate::data::posix::PosixTzString;
use crate::data::time::Seconds;
use crate::data::tzif::{
    DataBlock, LeapSecondRecord, LocalTimeTypeRecord, StandardWallIndicator, TzifData, TzifHeader,
    UtLocalIndicator,
};
use combine::parser::byte::byte;
use combine::parser::byte::num::{be_i32, be_i64, be_u32};
use combine::{
    any, between, choice, count_min_max, one_of, skip_count, value, ParseError, Parser, Stream,
};

/// Parses the four-byte ASCII \[RFC20\] sequence `"TZif"` (0x54 0x5A 0x69 0x42),
/// which identifies the file as utilizing the Time Zone Information Format.
fn magic_sequence<Input>() -> impl Parser<Input, Output = u8>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    byte(b'T')
        .with(byte(b'Z'))
        .with(byte(b'i'))
        .with(byte(b'f'))
}

/// Parse the `TZif` version number specified by <https://datatracker.ietf.org/doc/html/rfc8536>
/// > A byte identifying the version of the file's format.
/// > The value MUST be one of the following:
/// >
/// > NUL (0x00)  Version 1
/// >
/// > '2' (0x32)  Version 2
/// >
/// > '3' (0x33)  Version 3
fn version<Input>() -> impl Parser<Input, Output = usize>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    one_of([0, b'2', b'3'])
        .map(|byte: u8| byte.saturating_sub(b'0') as usize)
        .map(|version| if version == 0 { 1 } else { version })
}

/// Parse the `TZif` `isutcnt` value specified by <https://datatracker.ietf.org/doc/html/rfc8536>
/// > A four-byte unsigned integer specifying the number of UT/
/// > local indicators contained in the data block -- MUST either be
/// > zero or equal to "typecnt".
fn isutcnt<Input>() -> impl Parser<Input, Output = usize>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    be_u32().map(|u32| u32 as usize)
}

/// Parse the `TZif` `isstdcnt` value specified by <https://datatracker.ietf.org/doc/html/rfc8536>
/// > A four-byte unsigned integer specifying the number of
/// > standard/wall indicators contained in the data block -- MUST
/// > either be zero or equal to "typecnt".
fn isstdcnt<Input>() -> impl Parser<Input, Output = usize>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    be_u32().map(|u32| u32 as usize)
}

/// Parse the `TZif` `leapcnt` value specified by <https://datatracker.ietf.org/doc/html/rfc8536>
/// > A four-byte unsigned integer specifying the number of
/// > leap-second records contained in the data block.
fn leapcnt<Input>() -> impl Parser<Input, Output = usize>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    be_u32().map(|u32| u32 as usize)
}

/// Parse the `TZif` `timecnt` value specified by <https://datatracker.ietf.org/doc/html/rfc8536>
/// > A four-byte unsigned integer specifying the number of
/// > transition times contained in the data block.
fn timecnt<Input>() -> impl Parser<Input, Output = usize>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    be_u32().map(|u32| u32 as usize)
}

/// Parse the `TZif` `typecnt` value specified by <https://datatracker.ietf.org/doc/html/rfc8536>
/// > A four-byte unsigned integer specifying the number of
/// > local time type records contained in the data block -- MUST NOT be
/// > zero. (Although local time type records convey no useful
/// > information in files that have nonempty TZ strings but no
/// > transitions, at least one such record is nevertheless required
/// > because many `TZif` readers reject files that have zero time types.)
///
/// Takes `isutcnt` and `isstdcnt` as arguments. If either of these values are
/// non-zero, then they must be equal to the parsed `typecnt`.
fn typecnt<Input>(isutcnt: usize, isstdcnt: usize) -> impl Parser<Input, Output = usize>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    be_u32()
        .map(|u32| u32 as usize)
        .then(|typecnt| {
            ensure(
                typecnt,
                |&typecnt| typecnt != 0,
                "typecnt should never be equal to zero",
            )
        })
        .then(move |typecnt| {
            ensure(
                typecnt,
                |&typecnt| isutcnt == 0 || isutcnt == typecnt,
                "if isutcnt is non-zero it should be equal to typecnt",
            )
        })
        .then(move |typecnt| {
            ensure(
                typecnt,
                |&typecnt| isstdcnt == 0 || isstdcnt == typecnt,
                "if isstdcnt is non-zero it should be equal to typecnt",
            )
        })
}

/// Parse the `TZif` `charcnt` value specified by <https://datatracker.ietf.org/doc/html/rfc8536>
/// > A four-byte unsigned integer specifying the total number
/// > of bytes used by the set of time zone designations contained in
/// > the data block - MUST NOT be zero. The count includes the
/// > trailing NUL (0x00) byte at the end of the last time zone
/// > designation.
fn charcnt<Input>() -> impl Parser<Input, Output = usize>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    be_u32().map(|u32| u32 as usize).then(|charcnt| {
        ensure(
            charcnt,
            |&charcnt| charcnt != 0,
            "charcnt should never be zero",
        )
    })
}

/// Parse a `TZif` file header specified by <https://datatracker.ietf.org/doc/html/rfc8536>
/// > A `TZif` header is structured as follows (the lengths of multi-byte
/// > fields are shown in parentheses):
/// > ```text
/// > +---------------+---+
/// > |  magic    (4) |ver|
/// > +---------------+---+---------------------------------------+
/// > |           [unused - reserved for future use] (15)         |
/// > +---------------+---------------+---------------+-----------+
/// > |  isutcnt  (4) |  isstdcnt (4) |  leapcnt  (4) |
/// > +---------------+---------------+---------------+
/// > |  timecnt  (4) |  typecnt  (4) |  charcnt  (4) |
/// > +---------------+---------------+---------------+
/// > ```
fn header<Input>() -> impl Parser<Input, Output = TzifHeader>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    magic_sequence()
        .with((
            version(),
            skip_count(15, any()).with(isutcnt()),
            isstdcnt(),
            leapcnt(),
            timecnt(),
        ))
        .then(|(version, isutcnt, isstdcnt, leapcnt, timecnt)| {
            combine::struct_parser! {
                TzifHeader {
                    version: value(version),
                    isutcnt: value(isutcnt),
                    isstdcnt: value(isstdcnt),
                    leapcnt: value(leapcnt),
                    timecnt: value(timecnt),
                    typecnt: typecnt(isutcnt, isstdcnt),
                    charcnt: charcnt(),
                }
            }
        })
}

/// A four- or eight-byte UNIX leap-time value.
/// Each value is used as a transition time at which the rules for
/// computing local time may change. Each time value SHOULD be at least -2**59.
///
/// (-2**59 is the greatest negated power of 2 that predates the Big
/// Bang, and avoiding earlier timestamps works around known `TZif`
/// reader bugs relating to outlandishly negative timestamps.)
fn historic_transition_time<const V: usize, Input>() -> impl Parser<Input, Output = Seconds>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    match V {
        1 => be_i32().map(i64::from).left(),
        _ => be_i64().right(),
    }
    .then(|time| {
        ensure(
            time,
            |&time| time >= (-2_i64).pow(59),
            "transition time should not be less than -2.pow(59)",
        )
    })
    .map(Seconds)
}

/// Parse a series of transition times sorted in strictly ascending order.
/// The number of time values is specified by the `timecnt`
/// field in the header.
fn historic_transition_times<const V: usize, Input>(
    timecnt: usize,
) -> impl Parser<Input, Output = Vec<Seconds>>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    count_min_max(timecnt, timecnt, historic_transition_time::<V, _>()).then(
        |times: Vec<Seconds>| {
            ensure(
                times,
                |times| {
                    times
                        .iter()
                        .zip(times.iter().skip(1))
                        .all(|(lhs, rhs)| lhs <= rhs)
                },
                "historic transition times should be in ascenting order",
            )
        },
    )
}

/// A series of one-byte unsigned integers specifying
/// the type of local time of the corresponding transition time.
///
/// These values serve as zero-based indices into the array of local
/// time type records. The number of type indices is specified by the
/// `timecnt` field in the header. Each type index MUST be in the
/// range `[0, typecnt - 1]`
fn transition_types<Input>(
    timecnt: usize,
    typecnt: usize,
) -> impl Parser<Input, Output = Vec<usize>>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    count_min_max(timecnt, timecnt, any().map(|byte| byte as usize)).then(
        move |types: Vec<usize>| {
            ensure(
                types,
                |types| types.iter().all(|&t| t < typecnt),
                "all transition types should be in range [0, typecnt - 1]",
            )
        },
    )
}

/// A four-byte signed integer specifying the number of
/// seconds to be added to UT in order to determine local time.
/// The value MUST NOT be -2**31 and SHOULD be in the range
/// [-89999, 93599] (i.e., its value SHOULD be more than -25 hours
/// and less than 26 hours). Avoiding -2**31 allows 32-bit clients
/// to negate the value without overflow. Restricting it to
/// [-89999, 93599] allows easy support by implementations that
/// already support the POSIX-required range [-24:59:59, 25:59:59].
fn utoff<Input>() -> impl Parser<Input, Output = Seconds>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    be_i32()
        .then(|utoff| {
            ensure(
                utoff,
                |&utoff| utoff != (-2i32).pow(31),
                "utoff should never be equal to -2.pow(31)",
            )
        })
        .map(|utoff| Seconds(i64::from(utoff)))
}

/// Parses a byte as a boolean value. The value must be exactly
/// the numeric digit 0 (false) or the numeric digit 1 (true).
fn boolean<Input>() -> impl Parser<Input, Output = bool>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    choice((byte(b'\x00').map(|_| false), byte(b'\x01').map(|_| true)))
}

/// A one-byte value indicating whether local time should
/// be considered Daylight Saving Time (DST). The value MUST be 0
/// or 1. A value of one (1) indicates that this type of time is
/// DST. A value of zero (0) indicates that this time type is
/// standard time.
fn is_dst<Input>() -> impl Parser<Input, Output = bool>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    boolean()
}

/// A one-byte unsigned integer specifying a zero-based
/// index into the series of time zone designation bytes, thereby
/// selecting a particular designation string. Each index MUST be
/// in the range [0, charcnt - 1]; it designates the
/// NUL-terminated string of bytes starting at position "idx" in
/// the time zone designations. (This string MAY be empty.) A NUL
/// byte MUST exist in the time zone designations at or after
/// position "idx".
fn idx<Input>(charcnt: usize) -> impl Parser<Input, Output = usize>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    any()
        .map(|byte| byte as usize)
        .then(move |idx| ensure(idx, |&idx| idx < charcnt, "idx should be less than charcnt"))
}

/// A series of six-byte records specifying a
/// local time type. Each record has the following
/// format (the lengths of multi-byte fields are shown in
/// parentheses):
///
/// > ```text
/// > +---------------+---+---+
/// > |  utoff (4)    |dst|idx|
/// > +---------------+---+---+
/// > ```
fn local_time_type_record<Input>(charcnt: usize) -> impl Parser<Input, Output = LocalTimeTypeRecord>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    combine::struct_parser! {
        LocalTimeTypeRecord {
            utoff: utoff(),
            is_dst: is_dst(),
            idx: idx(charcnt),
        }
    }
}

/// A series of local time type records.
/// The number of records is specified by the "typecnt" field in the header.
fn local_time_type_records<Input>(
    typecnt: usize,
    charcnt: usize,
) -> impl Parser<Input, Output = Vec<LocalTimeTypeRecord>>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    count_min_max(typecnt, typecnt, local_time_type_record(charcnt))
}

/// A series of bytes constituting an array of
/// NUL-terminated (0x00) time zone designation strings. The total
/// number of bytes is specified by the "charcnt" field in the header.
fn raw_time_zone_designations<Input>(charcnt: usize) -> impl Parser<Input, Output = String>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    count_min_max(charcnt, charcnt, any())
        .map(|bytes: Vec<u8>| String::from_utf8_lossy(&bytes).into_owned())
}

/// A series of bytes constituting an array of
/// NUL-terminated (0x00) time zone designation strings. The total
/// number of bytes is specified by the "charcnt" field in the
/// header.
///
/// Splits each designation into a vector of [`String`] where each string
/// starts at an index defined by a local time type record and ends at a
/// NUL-terminator (0x00)
///
/// > e.g.
/// > ```text
/// > "LMT\u{0}HMT\u{0}MMT\u{0}IST\u{0}+0630\u{0}"
/// > ```
///
/// Note that two designations MAY overlap if one is a suffix
/// of the other. The character encoding of time zone designation
/// strings is not specified.
///
/// However, time zone designations SHOULD consist of at least three (3) and no
/// more than six (6) ASCII characters from the set of alphanumerics,
/// '-', and '+'. This is for compatibility with POSIX requirements
/// for time zone abbreviations, so this parser enforces a UTF-8 ASCII encoding,
/// to ensure compatability with Rust strings.
fn time_zone_designations<Input>(
    charcnt: usize,
    local_time_type_records: Vec<LocalTimeTypeRecord>,
) -> impl Parser<Input, Output = Vec<String>>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    raw_time_zone_designations(charcnt).map(move |raw_time_zone_designations| {
        let mut time_zone_designations = Vec::with_capacity(local_time_type_records.len());
        for record in &local_time_type_records {
            for end_idx in record.idx..charcnt {
                if raw_time_zone_designations.as_bytes()[end_idx] == b'\0' {
                    time_zone_designations.push(
                        String::from_utf8_lossy(
                            raw_time_zone_designations[record.idx..end_idx].as_bytes(),
                        )
                        .into_owned(),
                    );
                    break;
                }
            }
        }
        time_zone_designations
    })
}

/// A four- or eight-byte UNIX leap time value specifying the time at which a leap-second
/// correction occurs.
fn leap_second_occurrence<const V: usize, Input>() -> impl Parser<Input, Output = Seconds>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    match V {
        1 => be_i32().map(i64::from).left(),
        _ => be_i64().right(),
    }
    .map(Seconds)
}

/// A four-byte signed integer specifying the value of LEAPCORR on or after the
/// occurrence. The correction value in the first leap-second record, if present,
/// MUST be either one (1) or minus one (-1).
fn leap_second_correction<Input>() -> impl Parser<Input, Output = i32>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    be_i32()
}

/// A series of eight- or twelve-byte records
/// specifying the corrections that need to be applied to UTC in order
/// to determine TAI. The records are sorted by the occurrence time
/// in strictly ascending order. The number of records is specified
/// by the "leapcnt" field in the header. Each record has one of the
/// following structures (the lengths of multi-byte fields are shown
/// in parentheses):
///
/// > Version 1 Data Block:
/// >
/// > ```text
/// > +---------------+---------------+
/// > |  occur (4)    |  corr (4)     |
/// > +---------------+---------------+
/// > ```
/// >
/// > version-2+ Data Block:
/// >
/// > ```text
/// > +---------------+---------------+---------------+
/// > |  occur (8)                    |  corr (4)     |
/// > +---------------+---------------+---------------+
/// > ```
fn leap_second_record<const V: usize, Input>() -> impl Parser<Input, Output = LeapSecondRecord>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    combine::struct_parser! {
        LeapSecondRecord {
            occurrence: leap_second_occurrence::<V, _>(),
            correction: leap_second_correction(),
        }
    }
}

/// A series of leap second records.
///
/// Regarding the "occurence" value:
/// > The first value, if present, MUST be nonnegative, and each
/// > later value MUST be at least 2419199 greater than the previous
/// > value. (This is 28 days' worth of seconds, minus a potential
/// > negative leap second.)
///
/// Regarding the "correction" value:
/// > The correction value in the first leap-second record, if present,
/// > MUST be either one (1) or minus one (-1).
/// >
/// > The correction values in adjacent leap-second
/// > records MUST differ by exactly one (1). The value of
/// > LEAPCORR is zero for timestamps that occur before the
/// > occurrence time in the first leap-second record (or for all
/// > timestamps if there are no leap-second records).
fn leap_second_records<const V: usize, Input>(
    leapcnt: usize,
) -> impl Parser<Input, Output = Vec<LeapSecondRecord>>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    count_min_max(leapcnt, leapcnt, leap_second_record::<V, _>())
        .then(|records: Vec<LeapSecondRecord>| {
            ensure(
                records,
                |records| {
                    records
                        .first()
                        .map_or(true, |first| first.occurrence >= Seconds(0))
                },
                "The first leap-second occurrence, if present, must be non-negative",
            )
        })
        .then(|records: Vec<LeapSecondRecord>| {
            ensure(
                records,
                |records| {
                    records
                        .first()
                        .map_or(true, |first| first.correction == 1 || first.correction == -1)
                },
                "The first leap-second correction, if present, must be 1 or -1",
            )
        })
        .then(|records: Vec<LeapSecondRecord>| {
            ensure(
                records,
                |records| {
                    records
                        .iter()
                        .zip(records.iter().skip(1))
                        .all(|(prev, next)| next.occurrence - prev.occurrence >= Seconds(2_419_199))
                },
                "Each subsequent leap-second occurrence must be at least 2419199 greater than the previous value",
            )
        })
        .then(|records: Vec<LeapSecondRecord>| {
            ensure(
                records,
                |records| {
                    records
                        .iter()
                        .zip(records.iter().skip(1))
                        .all(|(prev, next)| (next.correction - prev.correction).abs() == 1)
                },
                "Adjacent leap-second corrections must differ by exactly 1",
            )
        })
}

/// A one-byte value indicating whether the
/// transition times associated with local time types were
/// specified as standard time or wall-clock time. Each value MUST be
/// 0 or 1. A value of one (1) indicates standard time. The value
/// MUST be set to one (1) if the corresponding UT/local indicator is
/// set to one (1). A value of zero (0) indicates wall time.
fn standard_wall_indicator<Input>() -> impl Parser<Input, Output = StandardWallIndicator>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    boolean().map(|bool| {
        if bool {
            StandardWallIndicator::Standard
        } else {
            StandardWallIndicator::Wall
        }
    })
}

/// A series of standard/wall indicators.
/// The number of values is specified by the "isstdcnt" field in the
/// header. If "isstdcnt" is zero (0), all transition times
/// associated with local time types are assumed to be specified as
/// wall time.
fn standard_wall_indicators<Input>(
    isstdcnt: usize,
) -> impl Parser<Input, Output = Vec<StandardWallIndicator>>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    count_min_max(isstdcnt, isstdcnt, standard_wall_indicator())
}

/// A one-byte value indicating whether the
/// transition times associated with local time types were
/// specified as UT or local time. Each value MUST be 0 or 1. A
/// value of one (1) indicates UT, and the corresponding standard/wall
/// indicator MUST also be set to one (1). A value of zero (0)
/// indicates local time.
fn ut_local_indicator<Input>() -> impl Parser<Input, Output = UtLocalIndicator>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    boolean().map(|bool| {
        if bool {
            UtLocalIndicator::Ut
        } else {
            UtLocalIndicator::Local
        }
    })
}

/// A series of ut/local indicators
/// The number of values is specified by the
/// "isutcnt" field in the header. If "isutcnt" is zero (0), all
/// transition times associated with local time types are assumed to
/// be specified as local time.
fn ut_local_indicators<Input>(isstdcnt: usize) -> impl Parser<Input, Output = Vec<UtLocalIndicator>>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    count_min_max(isstdcnt, isstdcnt, ut_local_indicator())
}

/// Parses a `TZif` data block.
/// A `TZif` data block consists of seven variable-length elements, each of
/// which is a series of items.  The number of items in each series is
/// determined by the corresponding count field in the header.  The total
/// length of each element is calculated by multiplying the number of
/// items by the size of each item.  Therefore, implementations that do
/// not wish to parse or use the version 1 data block can calculate its
/// total length and skip directly to the header of the version-2+ data
/// block.
///
/// In the version 1 data block, time values are 32 bits (`TIME_SIZE` = 4
/// bytes).  In the version-2+ data block, present only in version 2 and
/// 3 files, time values are 64 bits (`TIME_SIZE` = 8 bytes).
///
/// The data block is structured as follows (the lengths of multi-byte
/// fields are shown in parentheses):
/// > ```text
/// >    +---------------------------------------------------------+
/// >    |  transition times          (timecnt x TIME_SIZE)        |
/// >    +---------------------------------------------------------+
/// >    |  transition types          (timecnt)                    |
/// >    +---------------------------------------------------------+
/// >    |  local time type records   (typecnt x 6)                |
/// >    +---------------------------------------------------------+
/// >    |  time zone designations    (charcnt)                    |
/// >    +---------------------------------------------------------+
/// >    |  leap-second records       (leapcnt x (TIME_SIZE + 4))  |
/// >    +---------------------------------------------------------+
/// >    |  standard/wall indicators  (isstdcnt)                   |
/// >    +---------------------------------------------------------+
/// >    |  UT/local indicators       (isutcnt)                    |
/// >    +---------------------------------------------------------+
/// > ```
fn data_block<const V: usize, Input>(header: TzifHeader) -> impl Parser<Input, Output = DataBlock>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    (
        historic_transition_times::<V, _>(header.timecnt),
        transition_types(header.timecnt, header.typecnt),
        local_time_type_records(header.typecnt, header.charcnt),
    )
        .then(
            move |(transition_times, transition_types, local_time_type_records)| {
                (
                    value(transition_times),
                    value(transition_types),
                    value(local_time_type_records.clone()),
                    time_zone_designations(header.charcnt, local_time_type_records),
                    leap_second_records::<V, _>(header.leapcnt),
                    standard_wall_indicators(header.isstdcnt),
                )
            },
        )
        .then(
            move |(
                transition_times,
                transition_types,
                local_time_type_records,
                time_zone_designations,
                leap_second_records,
                standard_wall_indicators,
            )| {
                combine::struct_parser! {
                    DataBlock {
                        transition_times: value(transition_times),
                        transition_types: value(transition_types),
                        local_time_type_records: value(local_time_type_records),
                        time_zone_designations: value(time_zone_designations),
                        leap_second_records: value(leap_second_records),
                        standard_wall_indicators: value(standard_wall_indicators),
                        ut_local_indicators: ut_local_indicators(header.isutcnt),
                    }
                }
            },
        )
}

/// Parses a `TZif` footer.
/// The `TZif` footer is structured as follows (the lengths of multi-byte
/// fields are shown in parentheses):
///
/// > ```text
/// > +---+--------------------+---+
/// > | NL|  TZ string (0...)  |NL |
/// > +---+--------------------+---+
/// >
/// >           `TZif` Footer
/// > ```
///
/// The elements of the footer are defined as follows:
///
/// > NL:
/// > > An ASCII new line character (0x0A).
/// >
/// > TZ string:
/// > > A rule for computing local time changes after the last
/// > > transition time stored in the version-2+ data block.  The string
/// > > is either empty or uses the expanded format of the "TZ"
/// > > environment variable as defined in Section 8.3 of the "Base
/// > > Definitions" volume of \[POSIX\] with ASCII encoding, possibly
/// > > utilizing extensions described below (Section 3.3.1) in version 3
/// > > files.  If the string is empty, the corresponding information is
/// > > not available.  If the string is nonempty and one or more
/// > > transitions appear in the version-2+ data, the string MUST be
/// > > consistent with the last version-2+ transition.  In other words,
/// > > evaluating the TZ string at the time of the last transition should
/// > > yield the same time type as was specified in the last transition.
/// > > The string MUST NOT contain NUL bytes or be NUL-terminated, and
/// > > it SHOULD NOT begin with the ':' (colon) character.
///
/// The `TZif` footer is present only in version 2 and 3 files, as the
/// obsolescent version 1 format was designed before the need for a
/// footer was apparent.
fn footer<Input>() -> impl Parser<Input, Output = PosixTzString>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    between(byte(b'\n'), byte(b'\n'), posix_tz_string())
}

/// Parses a `TZif` binary file according to the following specification:
/// <https://datatracker.ietf.org/doc/html/rfc8536>
#[must_use]
pub fn tzif<Input>() -> impl Parser<Input, Output = TzifData>
where
    Input: Stream<Token = u8>,
    Input::Error: ParseError<Input::Token, Input::Range, Input::Position>,
{
    header()
        .then(|header1| {
            if header1.version() == 1 {
                (
                    value(header1),
                    data_block::<1, _>(header1),
                    value(None).left(),
                )
            } else {
                (
                    value(header1),
                    data_block::<1, _>(header1),
                    header().map(Some).right(),
                )
            }
        })
        .then(|(header1, block1, header2)| match header2 {
            None => combine::struct_parser! {
                TzifData {
                    header1: value(header1),
                    data_block1: value(block1),
                    header2: value(header2),
                    data_block2: value(None),
                    footer: value(None),
                }
            }
            .left(),
            Some(header) => (match header.version() {
                2 => combine::struct_parser! {
                    TzifData {
                        header1: value(header1),
                        data_block1: value(block1),
                        header2: value(header2),
                        data_block2: data_block::<2, _>(header).map(Some),
                        footer: footer().map(Some),
                    }
                }
                .left(),
                _ => combine::struct_parser! {
                    TzifData {
                        header1: value(header1),
                        data_block1: value(block1),
                        header2: value(header2),
                        data_block2: data_block::<3, _>(header).map(Some),
                        footer: footer().map(Some),
                    }
                }
                .right(),
            })
            .right(),
        })
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::data::posix::{DstTransitionInfo, TransitionDate, TransitionDay, ZoneVariantInfo};
    use crate::data::time::Hours;
    use crate::{assert_parse_eq, assert_parse_err, assert_parse_ok};
    use combine::EasyParser;

    // Test constants
    const A: usize = b'A' as usize;
    const B: usize = b'B' as usize;
    const C: usize = b'C' as usize;
    const D: usize = b'D' as usize;

    #[test]
    fn parse_magic_sequence() {
        // emtpy
        assert_parse_err!(magic_sequence(), "");

        // invalid magic sequences
        assert_parse_err!(magic_sequence(), "asdf");
        assert_parse_err!(magic_sequence(), "tzif");
        assert_parse_err!(magic_sequence(), "TZIF");

        // valid magic sequence
        assert_parse_ok!(magic_sequence(), "TZif");
    }

    #[test]
    fn parse_version() {
        // emtpy
        assert_parse_err!(version(), "");

        // invalid versions
        assert_parse_err!(version(), "0");
        assert_parse_err!(version(), "1");
        assert_parse_err!(version(), "4");

        // valid versions
        assert_parse_eq!(version(), "\x00", 1);
        assert_parse_eq!(version(), "2", 2);
        assert_parse_eq!(version(), "3", 3);
    }

    #[test]
    fn parse_trivial_count_values() {
        // empty
        assert_parse_err!(isutcnt(), "");
        assert_parse_err!(isstdcnt(), "");
        assert_parse_err!(leapcnt(), "");
        assert_parse_err!(timecnt(), "");

        // invalid count
        assert_parse_err!(isutcnt(), "\x00");
        assert_parse_err!(isutcnt(), "\x00\x00");
        assert_parse_err!(isutcnt(), "\x00\x00\x00");

        assert_parse_err!(isstdcnt(), "\x00");
        assert_parse_err!(isstdcnt(), "\x00\x00");
        assert_parse_err!(isstdcnt(), "\x00\x00\x00");

        assert_parse_err!(leapcnt(), "\x00");
        assert_parse_err!(leapcnt(), "\x00\x00");
        assert_parse_err!(leapcnt(), "\x00\x00\x00");

        assert_parse_err!(timecnt(), "\x00");
        assert_parse_err!(timecnt(), "\x00\x00");
        assert_parse_err!(timecnt(), "\x00\x00\x00");

        // valid count
        assert_parse_eq!(isutcnt(), "\x00\x00\x00\x41", A);
        assert_parse_eq!(isutcnt(), "\x00\x00\x00\x00", 0);

        assert_parse_eq!(isstdcnt(), "\x00\x00\x00\x42", B);
        assert_parse_eq!(isstdcnt(), "\x00\x00\x00\x00", 0);

        assert_parse_eq!(leapcnt(), "\x00\x00\x00\x43", C);
        assert_parse_eq!(leapcnt(), "\x00\x00\x00\x00", 0);

        assert_parse_eq!(timecnt(), "\x00\x00\x00\x44", D);
        assert_parse_eq!(timecnt(), "\x00\x00\x00\x00", 0);
    }

    #[test]
    fn parse_typecnt() {
        // empty
        assert_parse_err!(typecnt(0, 0), "");

        // invalid count
        assert_parse_err!(typecnt(0, 0), "\x00");
        assert_parse_err!(typecnt(0, 0), "\x00\x00");
        assert_parse_err!(typecnt(0, 0), "\x00\x00\x00");

        // invalid must be non-zero
        assert_parse_err!(typecnt(0, 0), "\x00\x00\x00\x00");

        // invalid if params are non-zero, must be equal to count.
        assert_parse_err!(typecnt(B, 0), "\x00\x00\x00\x41");
        assert_parse_err!(typecnt(0, B), "\x00\x00\x00\x41");
        assert_parse_err!(typecnt(A, B), "\x00\x00\x00\x41");
        assert_parse_err!(typecnt(B, A), "\x00\x00\x00\x41");

        // valid typecnt
        assert_parse_eq!(typecnt(0, 0), "\x00\x00\x00\x41", A);
        assert_parse_eq!(typecnt(B, B), "\x00\x00\x00\x42", B);
    }

    #[test]
    fn parse_charcnt() {
        // empty
        assert_parse_err!(charcnt(), "");

        // invalid count
        assert_parse_err!(charcnt(), "\x00");
        assert_parse_err!(charcnt(), "\x00\x00");
        assert_parse_err!(charcnt(), "\x00\x00\x00");

        // invalid must be non-zero
        assert_parse_err!(charcnt(), "\x00\x00\x00\x00");

        // valid count
        assert_parse_eq!(charcnt(), "\x00\x00\x00\x41", A);
        assert_parse_eq!(charcnt(), "\x00\x00\x00\x42", B);
    }

    #[test]
    fn parse_header() {
        // empty
        assert_parse_err!(header(), "");

        // invalid magic number
        assert_parse_err!(
            header(),
            "TZif1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x01\0\0\0\x01\0\0\0\0\0\0\0",
        );

        // invalid typecnt
        assert_parse_err!(
            header(),
            "TZif2\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x01\0\0\0\0\0\0\0",
        );

        // invalid charcnt
        assert_parse_err!(
            header(),
            "TZif2\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x01\0\0\0\0\0\0\0\0\0\0\0",
        );

        // invalid isutcnt
        assert_parse_err!(
            header(),
            "TZif2\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x02\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x01\0\0\0\x01\0\0\0\0\0\0\0",
        );

        // invalid isstdcnt
        assert_parse_err!(
            header(),
            "TZif2\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x02\0\0\0\0\0\0\0\0\0\0\0\x01\0\0\0\x01\0\0\0\0\0\0\0",
        );

        // valid header
        assert_parse_eq!(
            header(),
            "TZif2\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x01\0\0\0\x01\0\0\0\0\0\0\0",
            TzifHeader {
                version: 2,
                isutcnt: 0,
                isstdcnt: 0,
                leapcnt: 0,
                timecnt: 0,
                typecnt: 1,
                charcnt:1,
            }
        );
    }

    #[test]
    fn parse_historic_transition_time() {
        const ONE: &[u8] = 1i64.to_be_bytes().as_slice();
        const AT_BOUNDARY: &[u8] = (-2_i64).pow(59).to_be_bytes().as_slice();
        const OUT_OF_BOUNDS: &[u8] = ((-2_i64).pow(59) - 1).to_be_bytes().as_slice();

        // invalid transition time
        assert_parse_err!(
            historic_transition_time::<2, _>(),
            bytes OUT_OF_BOUNDS,
        );

        // valid transition times
        assert_parse_eq!(
            historic_transition_time::<2, _>(),
            bytes ONE,
            Seconds(1),
        );

        // version 1 reads only the first 32 bits
        // so this will read all zeros
        assert_parse_eq!(
            historic_transition_time::<1, _>(),
            bytes ONE,
            Seconds(0),
        );

        // reading the last 32 bits of the 64-bit ONE
        // should still parse to 1.
        assert_parse_eq!(
            historic_transition_time::<1, _>(),
            bytes ONE[ONE.len() / 2..].as_ref(),
            Seconds(1),
        );

        assert_parse_eq!(
            historic_transition_time::<3, _>(),
            bytes AT_BOUNDARY,
            Seconds((-2_i64).pow(59)),
        );
    }

    #[test]
    fn parse_historic_transition_times() {
        const ONE: &[u8] = 1i64.to_be_bytes().as_slice();
        const TWO: &[u8] = 2i64.to_be_bytes().as_slice();
        const SIX: &[u8] = 6i64.to_be_bytes().as_slice();
        let ascending = ONE
            .iter()
            .chain(TWO)
            .chain(SIX)
            .copied()
            .collect::<Vec<u8>>();
        let descending = SIX
            .iter()
            .chain(TWO)
            .chain(ONE)
            .copied()
            .collect::<Vec<u8>>();

        // invalid descending order
        assert_parse_err!(
            historic_transition_times::<2, _>(3),
            bytes descending.as_slice(),
        );

        // invalid wrong count
        assert_parse_err!(
            historic_transition_times::<2, _>(4),
            bytes ascending.as_slice(),
        );

        // valid times
        assert_parse_eq!(
            historic_transition_times::<2, _>(3),
            bytes ascending.as_slice(),
            vec![
                Seconds(1),
                Seconds(2),
                Seconds(6),
            ]
        );
    }

    #[test]
    fn parse_transition_types() {
        // empty
        assert_parse_err!(transition_types(3, 3), "");

        // invalid wrong count
        assert_parse_err!(transition_types(3, 3), "\x00\x01");

        // invalid ranges
        assert_parse_err!(transition_types(3, 3), "\x00\x01\x03");

        // valid types
        assert_parse_eq!(transition_types(3, 3), "\x00\x01\x02", vec![0, 1, 2],);
        assert_parse_eq!(transition_types(3, 3), "\x02\x01\x01", vec![2, 1, 1],);
    }

    #[test]
    fn parse_utoff() {
        const ONE: &[u8] = 1i32.to_be_bytes().as_slice();
        const TWO: &[u8] = 2i32.to_be_bytes().as_slice();
        const SIX: &[u8] = 6i32.to_be_bytes().as_slice();
        const INVALID: &[u8] = ((-2i32).pow(31)).to_be_bytes().as_slice();

        // invalid utoff
        assert_parse_err!(utoff(), bytes INVALID);

        // valid utoff
        assert_parse_eq!(utoff(), bytes ONE, Seconds(1));
        assert_parse_eq!(utoff(), bytes TWO, Seconds(2));
        assert_parse_eq!(utoff(), bytes SIX, Seconds(6));
    }

    #[test]
    fn parse_is_dst() {
        // empty
        assert_parse_err!(is_dst(), "");

        // invalid boolean byte
        assert_parse_err!(is_dst(), "0");
        assert_parse_err!(is_dst(), "1");

        // valid boolean bytes
        assert_parse_eq!(is_dst(), "\x00", false);
        assert_parse_eq!(is_dst(), "\x01", true);
    }

    #[test]
    fn parse_idx() {
        // empty
        assert_parse_err!(idx(0), "");

        // invalid index too large
        assert_parse_err!(idx(3), "\x03");

        // valid indices
        assert_parse_eq!(idx(3), "\x00", 0);
        assert_parse_eq!(idx(3), "\x01", 1);
        assert_parse_eq!(idx(3), "\x02", 2);
    }

    #[test]
    fn parse_local_time_type_record() {
        assert_parse_eq!(
            local_time_type_record(3),
            "\x00\x00\x00\x10\x01\x02",
            LocalTimeTypeRecord {
                utoff: Seconds(16),
                is_dst: true,
                idx: 2
            }
        );
        assert_parse_eq!(
            local_time_type_record(3),
            "\x00\x00\x10\x10\x00\x01",
            LocalTimeTypeRecord {
                utoff: Seconds(16 * 16 * 16 + 16),
                is_dst: false,
                idx: 1,
            }
        );
    }

    #[test]
    fn parse_local_time_type_records() {
        // invalid count
        assert_parse_err!(
            local_time_type_records(3, 3),
            "\x00\x00\x00\x10\x01\x02\x00\x00\x10\x10\x00\x01",
        );

        // valid records
        assert_parse_eq!(
            local_time_type_records(2, 3),
            "\x00\x00\x00\x10\x01\x02\x00\x00\x10\x10\x00\x01",
            vec![
                LocalTimeTypeRecord {
                    utoff: Seconds(16),
                    is_dst: true,
                    idx: 2
                },
                LocalTimeTypeRecord {
                    utoff: Seconds(16 * 16 * 16 + 16),
                    is_dst: false,
                    idx: 1,
                },
            ]
        );
    }

    #[test]
    fn parse_time_zone_designations() {
        assert_parse_eq!(
            time_zone_designations(
                14,
                vec![
                    LocalTimeTypeRecord {
                        utoff: Seconds(35356),
                        is_dst: false,
                        idx: 0,
                    },
                    LocalTimeTypeRecord {
                        utoff: Seconds(39600),
                        is_dst: true,
                        idx: 4,
                    },
                    LocalTimeTypeRecord {
                        utoff: Seconds(36000),
                        is_dst: false,
                        idx: 9,
                    },
                ]
            ),
            "LMT\0AEDT\0AEST\0",
            vec!["LMT".to_owned(), "AEDT".to_owned(), "AEST".to_owned()],
        );
    }

    #[test]
    fn parse_leap_second_occurrence() {
        const FIVE: &[u8] = 5i64.to_be_bytes().as_slice();

        // version 1 reads only the first 32 bits
        // so this will read all zeros
        assert_parse_eq!(
            historic_transition_time::<1, _>(),
            bytes FIVE,
            Seconds(0),
        );

        // reading the last 32 bits of the 64-bit ONE
        // should still parse to 5.
        assert_parse_eq!(
            historic_transition_time::<1, _>(),
            bytes FIVE[FIVE.len() / 2..].as_ref(),
            Seconds(5),
        );

        // version 2
        assert_parse_eq!(
            historic_transition_time::<2, _>(),
            bytes FIVE,
            Seconds(5),
        );

        // version 3
        assert_parse_eq!(
            historic_transition_time::<3, _>(),
            bytes FIVE,
            Seconds(5),
        );
    }

    #[test]
    fn parse_leap_second_record() {
        const ONE_64BIT: &[u8] = 1i64.to_be_bytes().as_slice();
        const ONE_32BIT: &[u8] = 1i32.to_be_bytes().as_slice();

        let record_v1 = ONE_32BIT
            .iter()
            .chain(ONE_32BIT)
            .copied()
            .collect::<Vec<u8>>();
        let record_v2p = ONE_64BIT
            .iter()
            .chain(ONE_32BIT)
            .copied()
            .collect::<Vec<u8>>();

        // version 1
        assert_parse_eq!(
            leap_second_record::<1, _>(),
            bytes record_v1.as_slice(),
            LeapSecondRecord {
                occurrence: Seconds(1),
                correction: 1,
            }
        );

        // version 2
        assert_parse_eq!(
            leap_second_record::<2, _>(),
            bytes record_v2p.as_slice(),
            LeapSecondRecord {
                occurrence: Seconds(1),
                correction: 1,
            }
        );

        // version 3
        assert_parse_eq!(
            leap_second_record::<3, _>(),
            bytes record_v2p.as_slice(),
            LeapSecondRecord {
                occurrence: Seconds(1),
                correction: 1,
            }
        );
    }

    #[test]
    fn parse_leap_second_records() {
        let invalid_first_occurrence = (-5i64)
            .to_be_bytes()
            .iter()
            .copied()
            .chain(1i32.to_be_bytes().iter().copied())
            .collect::<Vec<u8>>();
        let invalid_first_correction = 0i64
            .to_be_bytes()
            .iter()
            .copied()
            .chain(0i32.to_be_bytes().iter().copied())
            .collect::<Vec<u8>>();
        let invalid_second_occurrence = 0i64
            .to_be_bytes()
            .iter()
            .copied()
            .chain(1i32.to_be_bytes().iter().copied())
            .chain(2419198i64.to_be_bytes().iter().copied())
            .chain(2i32.to_be_bytes().iter().copied())
            .chain((2 * 2419199i64).to_be_bytes().iter().copied())
            .chain(3i32.to_be_bytes().iter().copied())
            .collect::<Vec<u8>>();
        let invalid_second_correction = 0i64
            .to_be_bytes()
            .iter()
            .copied()
            .chain(1i32.to_be_bytes().iter().copied())
            .chain(2419199i64.to_be_bytes().iter().copied())
            .chain(3i32.to_be_bytes().iter().copied())
            .chain((2 * 2419199i64).to_be_bytes().iter().copied())
            .chain(4i32.to_be_bytes().iter().copied())
            .collect::<Vec<u8>>();
        let valid_v1 = 0i32
            .to_be_bytes()
            .iter()
            .copied()
            .chain(1i32.to_be_bytes().iter().copied())
            .chain(2419199i32.to_be_bytes().iter().copied())
            .chain(2i32.to_be_bytes().iter().copied())
            .chain((2 * 2419199i32).to_be_bytes().iter().copied())
            .chain(3i32.to_be_bytes().iter().copied())
            .collect::<Vec<u8>>();
        let valid_v2p = 0i64
            .to_be_bytes()
            .iter()
            .copied()
            .chain(1i32.to_be_bytes().iter().copied())
            .chain(2419199i64.to_be_bytes().iter().copied())
            .chain(2i32.to_be_bytes().iter().copied())
            .chain((2 * 2419199i64).to_be_bytes().iter().copied())
            .chain(3i32.to_be_bytes().iter().copied())
            .collect::<Vec<u8>>();

        // invalid count
        assert_parse_err!(
            leap_second_records::<2, _>(4),
            bytes valid_v2p.as_slice(),
        );

        // invalid records
        assert_parse_err!(
            leap_second_records::<2, _>(1),
            bytes invalid_first_correction.as_slice(),
        );

        assert_parse_err!(
            leap_second_records::<2, _>(1),
            bytes invalid_first_occurrence.as_slice(),
        );

        assert_parse_err!(
            leap_second_records::<2, _>(2),
            bytes invalid_second_correction.as_slice(),
        );

        assert_parse_err!(
            leap_second_records::<2, _>(2),
            bytes invalid_second_occurrence.as_slice(),
        );

        // valid records
        assert_parse_eq!(
            leap_second_records::<1, _>(2),
            bytes valid_v1.as_slice(),
            vec![
                LeapSecondRecord {
                    occurrence: Seconds(0),
                    correction: 1,
                },
                LeapSecondRecord {
                    occurrence: Seconds(2419199),
                    correction: 2,
                },
            ],
        );

        assert_parse_eq!(
            leap_second_records::<2, _>(2),
            bytes valid_v2p.as_slice(),
            vec![
                LeapSecondRecord {
                    occurrence: Seconds(0),
                    correction: 1,
                },
                LeapSecondRecord {
                    occurrence: Seconds(2419199),
                    correction: 2,
                },
            ],
        );
    }

    #[test]
    fn parse_standard_wall_indicators() {
        // empty
        assert_parse_err!(standard_wall_indicators(3), "");

        // invalid count
        assert_parse_err!(standard_wall_indicators(3), "\x00\x01");

        // invalid standard-wall indicator
        assert_parse_err!(standard_wall_indicators(3), "\x00\x01\x02");

        // valid standard-wall indicators
        assert_parse_eq!(
            standard_wall_indicators(0),
            "",
            Vec::<StandardWallIndicator>::new()
        );
        assert_parse_eq!(
            standard_wall_indicators(4),
            "\x00\x01\x01\x00",
            vec![
                StandardWallIndicator::Wall,
                StandardWallIndicator::Standard,
                StandardWallIndicator::Standard,
                StandardWallIndicator::Wall,
            ]
        );
    }

    #[test]
    fn parse_ut_local_indicators() {
        // empty
        assert_parse_err!(ut_local_indicators(3), "");

        // invalid count
        assert_parse_err!(ut_local_indicators(3), "\x00\x01");

        // invalid standard-wall indicator
        assert_parse_err!(ut_local_indicators(3), "\x00\x01\x02");

        // valid standard-wall indicators
        assert_parse_eq!(ut_local_indicators(0), "", Vec::<UtLocalIndicator>::new());
        assert_parse_eq!(
            ut_local_indicators(4),
            "\x01\x00\x00\x01",
            vec![
                UtLocalIndicator::Ut,
                UtLocalIndicator::Local,
                UtLocalIndicator::Local,
                UtLocalIndicator::Ut,
            ]
        );
    }

    #[test]
    fn parse_footer() {
        // missing leading newline
        assert_parse_err!(footer(), "EST+5EDT,M3.2.0/2,M11.1.0/2\n");

        // missing final newline
        assert_parse_err!(footer(), "\nEST+5EDT,M3.2.0/2,M11.1.0/2");

        // valid footer
        assert_parse_eq!(
            footer(),
            "\nEST+5EDT,M3.2.0/2,M11.1.0/2\n",
            PosixTzString {
                std_info: ZoneVariantInfo {
                    name: "EST".to_owned(),
                    offset: Hours(5).as_seconds(),
                },
                dst_info: Some(DstTransitionInfo {
                    variant_info: ZoneVariantInfo {
                        name: "EDT".to_owned(),
                        offset: Hours(4).as_seconds()
                    },
                    start_date: TransitionDate {
                        day: TransitionDay::Mwd(3, 2, 0),
                        time: Hours(2).as_seconds(),
                    },
                    end_date: TransitionDate {
                        day: TransitionDay::Mwd(11, 1, 0),
                        time: Hours(2).as_seconds(),
                    },
                })
            }
        );
    }
}
