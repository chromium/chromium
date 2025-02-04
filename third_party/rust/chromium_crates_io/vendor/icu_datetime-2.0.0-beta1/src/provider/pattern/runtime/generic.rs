// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::{
    super::{reference, PatternError},
    super::{GenericPatternItem, PatternItem},
    Pattern,
};
use alloc::vec::Vec;
use core::str::FromStr;
use icu_provider::prelude::*;
use zerovec::ZeroVec;

/// A raw, low-level pattern with literals and placeholders.
///
/// This is a datetime-specific type designed to be binary-compatible with
/// [`Pattern`]. ICU4X developers looking for this sort of type should use
/// the `icu_pattern` crate instead.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, PartialEq, Eq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[allow(clippy::exhaustive_structs)] // this type is stable
#[cfg_attr(feature = "datagen", derive(databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::pattern::runtime))]
pub struct GenericPattern<'data> {
    /// The list of [`GenericPatternItem`]s.
    pub items: ZeroVec<'data, GenericPatternItem>,
}

/// A ZeroSlice containing a 0, 1, and 2 placeholder with no spaces
pub(crate) const ZERO_ONE_TWO_SLICE: &zerovec::ZeroSlice<GenericPatternItem> = zerovec::zeroslice!(
    GenericPatternItem;
    GenericPatternItem::to_unaligned_const;
    [
        GenericPatternItem::Placeholder(0),
        GenericPatternItem::Placeholder(1),
        GenericPatternItem::Placeholder(2),
    ]
);

impl<'data> GenericPattern<'data> {
    /// The function allows for creation of new DTF pattern from a generic pattern
    /// and replacement patterns.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::datetime::provider::pattern::runtime::{GenericPattern, Pattern};
    ///
    /// let date: Pattern = "y-M-d".parse().expect("Failed to parse pattern");
    /// let time: Pattern = "HH:mm".parse().expect("Failed to parse pattern");
    ///
    /// let glue: GenericPattern = "{1} 'at' {0}"
    ///     .parse()
    ///     .expect("Failed to parse generic pattern");
    /// assert_eq!(
    ///     glue.combined(date, time)
    ///         .expect("Failed to combine patterns")
    ///         .to_string(),
    ///     "y-M-d 'at' HH:mm"
    /// );
    /// ```
    pub fn combined(
        self,
        date: Pattern<'data>,
        time: Pattern<'data>,
    ) -> Result<Pattern<'static>, PatternError> {
        let size = date.items.len() + time.items.len();
        let mut result = Vec::with_capacity(self.items.len() + size);

        for item in self.items.iter() {
            match item {
                GenericPatternItem::Placeholder(0) => {
                    result.extend(time.items.iter());
                }
                GenericPatternItem::Placeholder(1) => {
                    result.extend(date.items.iter());
                }
                GenericPatternItem::Placeholder(idx) => {
                    #[allow(clippy::unwrap_used)] // idx is a valid base-10 digit
                    return Err(PatternError::UnknownSubstitution(
                        char::from_digit(idx as u32, 10).unwrap(),
                    ));
                }
                GenericPatternItem::Literal(ch) => result.push(PatternItem::Literal(ch)),
            }
        }

        Ok(Pattern::from(result))
    }
}

impl Default for GenericPattern<'_> {
    fn default() -> Self {
        Self {
            items: ZeroVec::new(),
        }
    }
}

impl From<&reference::GenericPattern> for GenericPattern<'_> {
    fn from(input: &reference::GenericPattern) -> Self {
        Self {
            items: ZeroVec::alloc_from_slice(&input.items),
        }
    }
}

impl From<&GenericPattern<'_>> for reference::GenericPattern {
    fn from(input: &GenericPattern<'_>) -> Self {
        Self {
            items: input.items.to_vec(),
        }
    }
}

impl FromStr for GenericPattern<'_> {
    type Err = PatternError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let reference = reference::GenericPattern::from_str(s)?;
        Ok(Self::from(&reference))
    }
}

#[cfg(test)]
#[cfg(feature = "datagen")]
mod test {
    use super::*;

    #[test]
    fn test_runtime_generic_pattern_combine() {
        let pattern: GenericPattern = "{1} 'at' {0}"
            .parse()
            .expect("Failed to parse a generic pattern.");

        let date = "y/M/d".parse().expect("Failed to parse a date pattern.");

        let time = "HH:mm".parse().expect("Failed to parse a time pattern.");

        let pattern = pattern
            .combined(date, time)
            .expect("Failed to combine date and time.");

        assert_eq!(pattern.to_string(), "y/M/d 'at' HH:mm");
    }
}
