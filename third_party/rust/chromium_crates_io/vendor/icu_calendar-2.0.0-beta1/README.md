# icu_calendar [![crates.io](https://img.shields.io/crates/v/icu_calendar)](https://crates.io/crates/icu_calendar)

<!-- cargo-rdme start -->

Types for dealing with dates, times, and custom calendars.

This module is published as its own crate ([`icu_calendar`](https://docs.rs/icu_calendar/latest/icu_calendar/))
and as part of the [`icu`](https://docs.rs/icu/latest/icu/) crate. See the latter for more details on the ICU4X project.
The [`types`] module has a lot of common types for dealing with dates and times.

[`Calendar`] is a trait that allows one to define custom calendars, and [`Date`]
can represent dates for arbitrary calendars.

The [`Iso`] and [`Gregorian`] types are implementations for the ISO and
Gregorian calendars respectively. Further calendars can be found in the [`cal`] module.

Most interaction with this crate will be done via the [`Date`] and [`DateTime`] types.

Some of the algorithms implemented here are based on
Dershowitz, Nachum, and Edward M. Reingold. _Calendrical calculations_. Cambridge University Press, 2008.
with associated Lisp code found at <https://github.com/EdReingold/calendar-code2>.

## Examples

Examples of date manipulation using `Date` object. `Date` objects are useful
for working with dates, encompassing information about the day, month, year,
as well as the calendar type.

```rust
use icu::calendar::{types::IsoWeekday, Date};

// Creating ISO date: 1992-09-02.
let mut date_iso = Date::try_new_iso(1992, 9, 2)
    .expect("Failed to initialize ISO Date instance.");

assert_eq!(date_iso.day_of_week(), IsoWeekday::Wednesday);
assert_eq!(date_iso.year().era_year_or_extended(), 1992);
assert_eq!(date_iso.month().ordinal, 9);
assert_eq!(date_iso.day_of_month().0, 2);

// Answering questions about days in month and year.
assert_eq!(date_iso.days_in_year(), 366);
assert_eq!(date_iso.days_in_month(), 30);
```

Example of converting an ISO date across Indian and Buddhist calendars.

```rust
use icu::calendar::cal::{Buddhist, Indian};
use icu::calendar::Date;

// Creating ISO date: 1992-09-02.
let mut date_iso = Date::try_new_iso(1992, 9, 2)
    .expect("Failed to initialize ISO Date instance.");

assert_eq!(date_iso.year().era_year_or_extended(), 1992);
assert_eq!(date_iso.month().ordinal, 9);
assert_eq!(date_iso.day_of_month().0, 2);

// Conversion into Indian calendar: 1914-08-02.
let date_indian = date_iso.to_calendar(Indian);
assert_eq!(date_indian.year().era_year_or_extended(), 1914);
assert_eq!(date_indian.month().ordinal, 6);
assert_eq!(date_indian.day_of_month().0, 11);

// Conversion into Buddhist calendar: 2535-09-02.
let date_buddhist = date_iso.to_calendar(Buddhist);
assert_eq!(date_buddhist.year().era_year_or_extended(), 2535);
assert_eq!(date_buddhist.month().ordinal, 9);
assert_eq!(date_buddhist.day_of_month().0, 2);
```

Example using `DateTime` object. Similar to `Date` objects, `DateTime` objects
contain an accessible `Date` object containing information about the day, month,
year, and calendar type. Additionally, `DateTime` objects contain an accessible
`Time` object, including granularity of hour, minute, second, and nanosecond.

```rust
use icu::calendar::{types::IsoWeekday, DateTime, Time};

// Creating ISO date: 1992-09-02 8:59
let mut datetime_iso = DateTime::try_new_iso(1992, 9, 2, 8, 59, 0)
    .expect("Failed to initialize ISO DateTime instance.");

assert_eq!(datetime_iso.date.day_of_week(), IsoWeekday::Wednesday);
assert_eq!(datetime_iso.date.year().era_year_or_extended(), 1992);
assert_eq!(datetime_iso.date.month().ordinal, 9);
assert_eq!(datetime_iso.date.day_of_month().0, 2);
assert_eq!(datetime_iso.time.hour.number(), 8);
assert_eq!(datetime_iso.time.minute.number(), 59);
assert_eq!(datetime_iso.time.second.number(), 0);
assert_eq!(datetime_iso.time.nanosecond.number(), 0);
```
[`ICU4X`]: ../icu/index.html

<!-- cargo-rdme end -->

## More Information

For more information on development, authorship, contributing etc. please visit [`ICU4X home page`](https://github.com/unicode-org/icu4x).
