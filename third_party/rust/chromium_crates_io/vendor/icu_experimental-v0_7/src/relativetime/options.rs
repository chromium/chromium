// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Options for configuring [`RelativeTimeFormatter`](crate::relativetime::RelativeTimeFormatter).

use icu_decimal::options::DecimalFormatterOptions;
pub use icu_decimal::options::GroupingStrategy;

/// A bag of options for defining how to format time using
/// [`RelativeTimeFormatter`](crate::relativetime::RelativeTimeFormatter).
///
/// # Examples
///
/// ```
/// use icu::experimental::relativetime::options::{
///     GroupingStrategy, Numeric, RelativeTimeFormatterOptions,
/// };
///
/// let options = RelativeTimeFormatterOptions::default()
///     .with_numeric(Numeric::Auto)
///     .with_grouping_strategy(GroupingStrategy::Min2);
/// ```
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub struct RelativeTimeFormatterOptions {
    /// Whether to always use numeric formatting for time.
    ///
    /// Default is [`Numeric::Always`].
    pub numeric: Option<Numeric>,

    /// When to render grouping separators in the formatted number.
    ///
    /// Default is [`GroupingStrategy::Auto`].
    pub grouping_strategy: Option<GroupingStrategy>,
}

impl Default for RelativeTimeFormatterOptions {
    fn default() -> Self {
        Self::default()
    }
}

impl RelativeTimeFormatterOptions {
    /// Constructs a new [`RelativeTimeFormatterOptions`] with default values.
    pub const fn default() -> Self {
        Self {
            numeric: None,
            grouping_strategy: None,
        }
    }

    /// Sets the [`Numeric`] formatting mode.
    pub const fn with_numeric(mut self, numeric: Numeric) -> Self {
        self.numeric = Some(numeric);
        self
    }

    /// Sets the [`GroupingStrategy`] for formatting numbers.
    pub const fn with_grouping_strategy(mut self, grouping_strategy: GroupingStrategy) -> Self {
        self.grouping_strategy = Some(grouping_strategy);
        self
    }
}

impl From<RelativeTimeFormatterOptions> for DecimalFormatterOptions {
    fn from(options: RelativeTimeFormatterOptions) -> Self {
        let mut df_options = Self::default();
        df_options.grouping_strategy = options.grouping_strategy;
        df_options
    }
}

/// Configures whether to always use numeric formatting even when special formatting is available.
#[derive(Debug, Copy, Clone, PartialEq, Eq, Default, Hash)]
#[non_exhaustive]
pub enum Numeric {
    /// Always use numeric formatting.
    #[default]
    Always,

    /// Automatically select special formatting if available else fallback to numeric formatting.
    Auto,
}
