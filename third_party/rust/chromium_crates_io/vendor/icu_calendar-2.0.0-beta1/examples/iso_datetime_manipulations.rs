// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// An example application which uses icu_datetime to format entries
// from a log into human readable dates and times.

#![no_main] // https://github.com/unicode-org/icu4x/issues/395
icu_benchmark_macros::instrument!();
use icu_benchmark_macros::println;

use icu_calendar::DateTime;

const DATETIMES_ISO: &[(i32, u8, u8, u8, u8, u8)] = &[
    (1970, 1, 1, 3, 5, 12),
    (1982, 3, 11, 2, 25, 59),
    (1999, 2, 21, 13, 12, 23),
    (2000, 12, 29, 10, 50, 23),
    (2001, 9, 8, 11, 5, 5),
    (2017, 7, 12, 3, 1, 1),
    (2020, 2, 29, 23, 12, 23),
    (2021, 3, 21, 18, 35, 34),
    (2021, 6, 10, 13, 12, 23),
    (2021, 9, 2, 5, 50, 22),
    (2022, 10, 8, 9, 45, 32),
    (2022, 2, 9, 10, 32, 45),
    (2033, 6, 10, 17, 22, 22),
];

fn main() {
    for &(year, month, day, hour, minute, second) in DATETIMES_ISO {
        let datetime = DateTime::try_new_iso(year, month, day, hour, minute, second)
            .expect("datetime should parse");
        println!(
            "Year: {}, Month: {}, Day: {}, Hour: {}, Minute: {}, Second: {}",
            datetime.date.year().era_year_or_extended(),
            datetime.date.month().ordinal,
            datetime.date.day_of_month().0,
            u8::from(datetime.time.hour),
            u8::from(datetime.time.minute),
            u8::from(datetime.time.second),
        );
    }
}
