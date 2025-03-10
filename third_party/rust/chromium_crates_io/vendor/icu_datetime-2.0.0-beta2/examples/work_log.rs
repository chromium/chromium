// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// An example application which uses icu_datetime to format entries
// from a work log into human readable dates and times.

#![no_main] // https://github.com/unicode-org/icu4x/issues/395
icu_benchmark_macros::instrument!();
use icu_benchmark_macros::println;

use icu_calendar::Date;
use icu_datetime::{fieldsets::YMDT, FixedCalendarDateTimeFormatter};
use icu_locale_core::locale;
use icu_time::{DateTime, Time};

const DATES_ISO: &[(i32, u8, u8, u8, u8, u8)] = &[
    (2001, 9, 8, 18, 46, 40),
    (2017, 7, 13, 19, 40, 0),
    (2020, 9, 13, 5, 26, 40),
    (2021, 1, 6, 22, 13, 20),
    (2021, 5, 2, 17, 0, 0),
    (2021, 8, 26, 10, 46, 40),
    (2021, 11, 20, 3, 33, 20),
    (2022, 4, 14, 22, 20, 0),
    (2022, 8, 8, 16, 6, 40),
    (2033, 5, 17, 20, 33, 20),
];

fn main() {
    let dtf = FixedCalendarDateTimeFormatter::try_new(locale!("en").into(), YMDT::medium())
        .expect("Failed to create FixedCalendarDateTimeFormatter instance.");

    println!("\n====== Work Log (en) example ============");

    for (idx, &(year, month, day, hour, minute, second)) in DATES_ISO.iter().enumerate() {
        let date = DateTime {
            date: Date::try_new_gregorian(year, month, day).expect("date should parse"),
            time: Time::try_new(hour, minute, second, 0).expect("time should parse"),
        };
        let fdt = dtf.format(&date);
        println!("{idx}) {}", fdt);
    }
}
