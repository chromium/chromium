// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Localized formatting of dates, times, and time zones.
//!
//! This module is published as its own crate ([`icu_datetime`](https://docs.rs/icu_datetime/latest/icu_datetime/))
//! and as part of the [`icu`](https://docs.rs/icu/latest/icu/) crate. See the latter for more details on the ICU4X project.
//!
//! ICU4X datetime formatting follows the Unicode UTS 35 standard for [Semantic Skeletons](https://unicode.org/reports/tr35/tr35-dates.html#Semantic_Skeletons).
//! First you choose a _field set_, then you configure the formatting _options_ to your desired context.
//!
//! 1. Field Sets: [`icu::datetime::fieldsets`](fieldsets)
//! 2. Options: [`icu::datetime::options`](options)
//!
//! ICU4X supports formatting in over one dozen _calendar systems_, including Gregorian, Buddhist,
//! Islamic, and more. The calendar system is usually derived from the locale, but it can also be
//! specified explicitly.
//!
//! The main formatter in this crate is [`DateTimeFormatter`], which supports all field sets,
//! options, and calendar systems. Additional formatter types are available to developers in
//! resource-constrained environments.
//!
//! The formatters accept input types from the [`calendar`](icu_calendar) and
//! [`timezone`](icu_time) crates (Also reexported from the [`input`] module of this crate):
//!
//! 1. [`Date`](icu_calendar::Date)
//! 2. [`DateTime`](icu_time::DateTime)
//! 3. [`Time`](icu_time::Time)
//! 4. [`UtcOffset`](icu_time::zone::UtcOffset)
//! 5. [`TimeZoneInfo`](icu_time::TimeZoneInfo)
//! 6. [`ZonedDateTime`](icu_time::ZonedDateTime)
//!
//! Not all inputs are valid for all field sets.
//!
//! # Binary Size Tradeoffs
//!
//! The datetime crate has been engineered with a focus on giving developers the ability to
//! tune binary size to their needs. The table illustrates the two main tradeoffs, field sets
//! and calendar systems:
//!
//! | Factor | Static (Lower Binary Size) | Dynamic (Greater Binary Size) |
//! |---|---|---|
//! | Field Sets | Specific [`fieldsets`] types | Enumerations from [`fieldsets::enums`] |
//! | Calendar Systems | [`FixedCalendarDateTimeFormatter`] | [`DateTimeFormatter`] |
//!
//! If formatting times and time zones without dates, consider using [`NoCalendarFormatter`].
//!
//! # Examples
//!
//! ```
//! use icu::datetime::fieldsets;
//! use icu::datetime::input::Date;
//! use icu::datetime::input::{DateTime, Time};
//! use icu::datetime::DateTimeFormatter;
//! use icu::locale::{locale, Locale};
//! use writeable::assert_writeable_eq;
//!
//! // Field set for year, month, day, hour, and minute with a medium length:
//! let field_set = fieldsets::YMDT::medium().hm();
//!
//! // Create a formatter for Argentinian Spanish:
//! let locale = locale!("es-AR");
//! let dtf = DateTimeFormatter::try_new(locale.into(), field_set).unwrap();
//!
//! // Format something:
//! let datetime = DateTime {
//!     date: Date::try_new_iso(2025, 1, 15).unwrap(),
//!     time: Time::try_new(16, 9, 35, 0).unwrap(),
//! };
//! let formatted_date = dtf.format(&datetime);
//!
//! assert_writeable_eq!(formatted_date, "15 de ene de 2025, 4:09 p. m.");
//! ```

// https://github.com/unicode-org/icu4x/blob/main/documents/process/boilerplate.md#library-annotations
#![cfg_attr(not(any(test, doc)), no_std)]
#![cfg_attr(
    not(test),
    deny(
        clippy::indexing_slicing,
        clippy::unwrap_used,
        clippy::expect_used,
        clippy::panic,
        clippy::exhaustive_structs,
        clippy::exhaustive_enums,
        clippy::trivially_copy_pass_by_ref,
        missing_debug_implementations,
    )
)]
#![warn(missing_docs)]

extern crate alloc;

mod combo;
mod error;
mod external_loaders;
pub mod fieldsets;
mod format;
mod neo;
pub mod options;
pub mod parts;
pub mod pattern;
pub mod provider;
pub(crate) mod raw;
pub mod scaffold;
pub(crate) mod size_test_macro;

pub use error::{DateTimeFormatterLoadError, DateTimeWriteError, MismatchedCalendarError};

pub use neo::DateTimeFormatter;
pub use neo::DateTimeFormatterPreferences;
pub use neo::FixedCalendarDateTimeFormatter;
pub use neo::FormattedDateTime;
pub use neo::NoCalendarFormatter;
pub use options::Length;

/// Locale preferences used by this crate
pub mod preferences {
    /// **This is a reexport of a type in [`icu::locale`](icu_locale_core::preferences::extensions::unicode::keywords)**.
    #[doc = "\n"] // prevent autoformatting
    pub use icu_locale_core::preferences::extensions::unicode::keywords::CalendarAlgorithm;
    /// **This is a reexport of a type in [`icu::locale`](icu_locale_core::preferences::extensions::unicode::keywords)**.
    #[doc = "\n"] // prevent autoformatting
    pub use icu_locale_core::preferences::extensions::unicode::keywords::HourCycle;
    /// **This is a reexport of a type in [`icu::locale`](icu_locale_core::preferences::extensions::unicode::keywords)**.
    #[doc = "\n"] // prevent autoformatting
    pub use icu_locale_core::preferences::extensions::unicode::keywords::NumberingSystem;
}

/// Types that can be fed to [`DateTimeFormatter`]/[`FixedCalendarDateTimeFormatter`].
pub mod input {
    /// **This is a reexport of a type in [`icu_calendar`]**.
    #[doc = "\n"] // prevent autoformatting
    pub use icu_calendar::Date;
    /// **This is a reexport of a type in [`icu_time`]**.
    #[doc = "\n"] // prevent autoformatting
    pub use icu_time::zone::UtcOffset;
    /// **This is a reexport of a type in [`icu_time`]**.
    #[doc = "\n"] // prevent autoformatting
    pub use icu_time::DateTime;
    /// **This is a reexport of a type in [`icu_time`]**.
    #[doc = "\n"] // prevent autoformatting
    pub use icu_time::Time;
    /// **This is a reexport of a type in [`icu_time`]**.
    #[doc = "\n"] // prevent autoformatting
    pub use icu_time::TimeZone;
    /// **This is a reexport of a type in [`icu_time`]**.
    #[doc = "\n"] // prevent autoformatting
    pub use icu_time::TimeZoneInfo;
    /// **This is a reexport of a type in [`icu_time`]**.
    #[doc = "\n"] // prevent autoformatting
    pub use icu_time::ZonedDateTime;
}
