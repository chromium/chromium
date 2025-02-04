// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Options for [`FixedDecimalFormatter`](crate::FixedDecimalFormatter).

/// A bag of options defining how numbers will be formatted by
/// [`FixedDecimalFormatter`](crate::FixedDecimalFormatter).
#[derive(Debug, Eq, PartialEq, Clone, Copy, Default, Hash)]
#[non_exhaustive]
pub struct FixedDecimalFormatterOptions {
    /// When to render grouping separators.
    pub grouping_strategy: GroupingStrategy,
}

impl From<GroupingStrategy> for FixedDecimalFormatterOptions {
    fn from(grouping_strategy: GroupingStrategy) -> Self {
        Self { grouping_strategy }
    }
}

/// Configuration for how often to render grouping separators.
///
/// # Examples
///
/// ```
/// use icu::decimal::options;
/// use icu::decimal::FixedDecimalFormatter;
/// use icu::locale::Locale;
/// use writeable::assert_writeable_eq;
///
/// let locale = Default::default();
/// let mut options: options::FixedDecimalFormatterOptions = Default::default();
/// options.grouping_strategy = options::GroupingStrategy::Min2;
/// let fdf = FixedDecimalFormatter::try_new(locale, options)
///     .expect("locale should be present");
///
/// let one_thousand = 1000.into();
/// assert_writeable_eq!(fdf.format(&one_thousand), "1000");
///
/// let ten_thousand = 10000.into();
/// assert_writeable_eq!(fdf.format(&ten_thousand), "10,000");
/// ```
#[non_exhaustive]
#[derive(Debug, Eq, PartialEq, Clone, Copy, Hash)]
pub enum GroupingStrategy {
    /// Render grouping separators according to locale preferences.
    Auto,

    /// Never render grouping separators.
    Never,

    /// Always render grouping separators.
    ///
    /// For [`FixedDecimalFormatter`](crate::FixedDecimalFormatter), [`GroupingStrategy::Always`]
    /// has the same behavior as [`GroupingStrategy::Auto`].
    Always,

    /// Render grouping separators only if there are at least 2 digits before the final grouping
    /// separator. In most locales, this means that numbers between 1000 and 9999 do not get
    /// grouping separators, but numbers 10,000 and above will.
    Min2,
}

impl Default for GroupingStrategy {
    fn default() -> Self {
        Self::Auto
    }
}
