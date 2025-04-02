// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// An example application which uses icu_datetime to format entries
// from a log into human readable dates and times.

#![no_main] // https://github.com/unicode-org/icu4x/issues/395
icu_benchmark_macros::instrument!();
use icu_benchmark_macros::println;

use icu_calendar::Date;

const DATES_ISO: &[(i32, u8, u8)] = &[
    (1970, 1, 1),
    (1982, 3, 11),
    (1999, 2, 21),
    (2000, 12, 29),
    (2001, 9, 8),
    (2017, 7, 12),
    (2020, 2, 29),
    (2021, 3, 21),
    (2021, 6, 10),
    (2021, 9, 2),
    (2022, 10, 8),
    (2022, 2, 9),
    (2033, 6, 10),
];

fn main() {
    for &(year, month, day) in DATES_ISO {
        let date = Date::try_new_iso(year, month, day).expect("date should parse");
        println!(
            "Year: {}, Month: {}, Day: {}",
            date.year().era_year_or_extended(),
            date.month().ordinal,
            date.day_of_month().0,
        );
    }
}
