// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::str::FromStr;

use writeable::{impl_display_with_writeable, Writeable};

use crate::provider::pattern::{runtime, PatternError, PatternItem};
use crate::size_test_macro::size_test;

size_test!(DateTimePattern, date_time_pattern_size, 32);

/// A pattern for formatting a datetime in a calendar.
///
/// ❗ This type forgoes most internationalization functionality of the datetime crate.
/// It assumes that the pattern is already localized for the customer's locale. Most clients
/// should use [`DateTimeFormatter`] instead of directly formatting with patterns.
///
/// There are two ways to make one of these:
///
/// 1. From a custom pattern string: [`DateTimePattern::try_from_pattern_str`]
/// 2. From a formatted datetime: [`FormattedDateTime::pattern`]
///
/// Things you can do with one of these:
///
/// 1. Use it to directly format a datetime via [`TypedDateTimeNames`]
/// 2. Convert it to a string pattern via [`Writeable`]
/// 3. Get the resolved components
#[doc = date_time_pattern_size!()]
///
/// # Examples
///
/// Create a pattern from a custom string and compare it to one from data,
/// then check the resolved components:
///
/// ```
/// use icu::calendar::Date;
/// use icu::calendar::Gregorian;
/// use icu::datetime::fields::components;
/// use icu::datetime::fieldsets::YMD;
/// use icu::datetime::pattern::DateTimePattern;
/// use icu::datetime::FixedCalendarDateTimeFormatter;
/// use icu::locale::locale;
/// use writeable::assert_writeable_eq;
///
/// // Create the pattern from a string:
/// let pattern_str = "d MMM y";
/// let custom_pattern =
///     DateTimePattern::try_from_pattern_str(pattern_str).unwrap();
/// assert_writeable_eq!(custom_pattern, pattern_str);
///
/// // Load data that resolves to the same pattern:
/// let data_pattern = FixedCalendarDateTimeFormatter::<Gregorian, _>::try_new(
///     locale!("es-MX").into(),
///     YMD::medium(),
/// )
/// .unwrap()
/// // The pattern can depend on the datetime being formatted.
/// .format(
///     &Date::try_new_iso(2024, 1, 1)
///         .unwrap()
///         .to_calendar(Gregorian),
/// )
/// .pattern();
/// assert_writeable_eq!(data_pattern, pattern_str);
/// assert_eq!(custom_pattern, data_pattern);
///
/// // Check the resolved components:
/// let mut expected_components_bag = components::Bag::default();
/// expected_components_bag.year = Some(components::Year::Numeric);
/// expected_components_bag.month = Some(components::Month::Short);
/// expected_components_bag.day = Some(components::Day::NumericDayOfMonth);
/// let actual_components_bag = components::Bag::from(&data_pattern);
/// assert_eq!(actual_components_bag, expected_components_bag);
/// ```
///
/// [`DateTimeFormatter`]: crate::DateTimeFormatter
/// [`FormattedDateTime::pattern`]: crate::FormattedDateTime::pattern
/// [`TypedDateTimeNames`]: crate::pattern::TypedDateTimeNames
#[derive(Debug)]
pub struct DateTimePattern {
    pattern: runtime::Pattern<'static>,
}

impl DateTimePattern {
    /// Creates a [`DateTimePattern`] from a pattern string.
    ///
    /// For more details on the syntax, see UTS 35:
    /// <https://unicode.org/reports/tr35/tr35-dates.html#Date_Format_Patterns>
    pub fn try_from_pattern_str(pattern_str: &str) -> Result<Self, PatternError> {
        let pattern = runtime::Pattern::from_str(pattern_str)?;
        Ok(Self { pattern })
    }

    #[doc(hidden)] // TODO(#4467): Internal API
    pub fn from_runtime_pattern(pattern: runtime::Pattern<'static>) -> Self {
        Self { pattern }
    }

    pub(crate) fn iter_items(&self) -> impl Iterator<Item = PatternItem> + '_ {
        self.pattern.items.iter()
    }

    pub(crate) fn as_borrowed(&self) -> DateTimePatternBorrowed {
        DateTimePatternBorrowed(&self.pattern)
    }
}

impl FromStr for DateTimePattern {
    type Err = PatternError;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Self::try_from_pattern_str(s)
    }
}

// Check equality on the borrowed variant since it flattens the difference
// between the three `Single` pattern variants, which is not public-facing
impl PartialEq for DateTimePattern {
    fn eq(&self, other: &Self) -> bool {
        self.as_borrowed().eq(&other.as_borrowed())
    }
}

impl Eq for DateTimePattern {}

impl Writeable for DateTimePattern {
    fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
        self.pattern.write_to(sink)
    }
}

impl_display_with_writeable!(DateTimePattern);

// Not clear if this needs to be public. For now, make it private.
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub(crate) struct DateTimePatternBorrowed<'a>(pub(crate) &'a runtime::Pattern<'a>);
