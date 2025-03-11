// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Parts of a formatted date/time.
//!
//! # Examples
//!
//! ```
//! use icu::calendar::Gregorian;
//! use icu::calendar::Date;
//! use icu::datetime::parts as datetime_parts;
//! use icu::datetime::fieldsets;
//! use icu::datetime::options::SubsecondDigits;
//! use icu::datetime::options::TimePrecision;
//! use icu::datetime::DateTimeFormatter;
//! use icu::decimal::parts as decimal_parts;
//! use icu::locale::locale;
//! use icu::datetime::input::{ZonedDateTime, Time};
//! use icu::time::zone::{IanaParser, UtcOffsetCalculator};
//! use writeable::assert_writeable_parts_eq;
//!
//! let dtf = DateTimeFormatter::try_new(
//!     locale!("en-u-ca-buddhist").into(),
//!     fieldsets::YMDT::medium().with_time_precision(TimePrecision::Subsecond(SubsecondDigits::S2)).zone(fieldsets::zone::SpecificShort),
//! )
//! .unwrap();
//!
//! let dtz = ZonedDateTime::try_from_str("2023-11-20T11:35:03.5+00:00[Europe/London]", dtf.calendar(), IanaParser::new(), &UtcOffsetCalculator::new()).unwrap();
//!
//! // Missing data is filled in on a best-effort basis, and an error is signaled.
//! assert_writeable_parts_eq!(
//!     dtf.format(&dtz),
//!     "Nov 20, 2566 BE, 11:35:03.50 AM GMT",
//!     [
//!         (0, 3, datetime_parts::MONTH),
//!         (4, 6, decimal_parts::INTEGER),
//!         (4, 6, datetime_parts::DAY),
//!         (8, 12, decimal_parts::INTEGER),
//!         (8, 12, datetime_parts::YEAR),
//!         (13, 15, datetime_parts::ERA),
//!         (17, 19, decimal_parts::INTEGER),
//!         (17, 19, datetime_parts::HOUR),
//!         (20, 22, decimal_parts::INTEGER),
//!         (20, 22, datetime_parts::MINUTE),
//!         (23, 28, datetime_parts::SECOND),
//!         (23, 25, decimal_parts::INTEGER),
//!         (25, 26, decimal_parts::DECIMAL),
//!         (26, 28, decimal_parts::FRACTION),
//!         // note: from 28 to 31 is a NNBSP
//!         (31, 33, datetime_parts::DAY_PERIOD),
//!         (34, 37, datetime_parts::TIME_ZONE_NAME),
//!     ]
//! );
//! ```

use writeable::Part;

/// A [`Part`] used by [`FormattedDateTime`](super::FormattedDateTime).
pub const ERA: Part = Part {
    category: "datetime",
    value: "era",
};

/// A [`Part`] used by [`FormattedDateTime`](super::FormattedDateTime).
pub const YEAR: Part = Part {
    category: "datetime",
    value: "year",
};

/// A [`Part`] used by [`FormattedDateTime`](super::FormattedDateTime).
pub const RELATED_YEAR: Part = Part {
    category: "datetime",
    value: "relatedYear",
};

/// A [`Part`] used by [`FormattedDateTime`](super::FormattedDateTime).
pub const YEAR_NAME: Part = Part {
    category: "datetime",
    value: "yearName",
};

/// A [`Part`] used by [`FormattedDateTime`](super::FormattedDateTime).
pub const MONTH: Part = Part {
    category: "datetime",
    value: "month",
};

/// A [`Part`] used by [`FormattedDateTime`](super::FormattedDateTime).
pub const DAY: Part = Part {
    category: "datetime",
    value: "day",
};

/// A [`Part`] used by [`FormattedDateTime`](super::FormattedDateTime).
pub const WEEKDAY: Part = Part {
    category: "datetime",
    value: "weekday",
};

/// A [`Part`] used by [`FormattedDateTime`](super::FormattedDateTime).
pub const DAY_PERIOD: Part = Part {
    category: "datetime",
    value: "dayPeriod",
};

/// A [`Part`] used by [`FormattedDateTime`](super::FormattedDateTime).
pub const HOUR: Part = Part {
    category: "datetime",
    value: "hour",
};

/// A [`Part`] used by [`FormattedDateTime`](super::FormattedDateTime).
pub const MINUTE: Part = Part {
    category: "datetime",
    value: "minute",
};

/// A [`Part`] used by [`FormattedDateTime`](super::FormattedDateTime).
pub const SECOND: Part = Part {
    category: "datetime",
    value: "second",
};

/// A [`Part`] used by [`FormattedDateTime`](super::FormattedDateTime).
pub const TIME_ZONE_NAME: Part = Part {
    category: "datetime",
    value: "timeZoneName",
};
