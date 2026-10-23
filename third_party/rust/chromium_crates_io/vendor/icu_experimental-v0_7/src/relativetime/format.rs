// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::fmt::Write;

use fixed_decimal::Decimal;
use writeable::Writeable;

use crate::relativetime::{
    options::{Numeric, RelativeTimeFormatterOptions},
    relativetime::RelativeTimeFormatter,
};

/// [`Part`](writeable::Part)s used by [`FormattedRelativeTime`].
pub mod parts {
    use writeable::Part;

    /// The [`Part`] used by [`FormattedRelativeTime`](crate::relativetime::FormattedRelativeTime) to mark the
    /// part of the string that is without a placeholder.
    pub const LITERAL: Part = Part {
        category: "relativetime",
        value: "literal",
    };
}

/// An intermediate structure returned by [`RelativeTimeFormatter`](crate::relativetime::RelativeTimeFormatter).
/// This structure can be consumed via [`Writeable`](Writeable) trait to a string or buffer.
#[derive(Debug)]
pub struct FormattedRelativeTime<'a> {
    pub(crate) formatter: &'a RelativeTimeFormatter,
    pub(crate) options: &'a RelativeTimeFormatterOptions,
    pub(crate) value: Decimal,
    pub(crate) is_negative: bool,
}

impl Writeable for FormattedRelativeTime<'_> {
    fn write_to_parts<S: writeable::PartsWrite + ?Sized>(&self, sink: &mut S) -> core::fmt::Result {
        if self.options.numeric.unwrap_or_default() == Numeric::Auto
            && self.value.absolute.magnitude_range() == (0..=0)
        {
            // Can be cast without overflow as it is a single digit.
            let digit = self.value.absolute.digit_at(0) as i8;
            // -0 is not handled by special relatives strings
            if !(self.is_negative && digit == 0) {
                let i8_value = if self.is_negative { -digit } else { digit };
                if let Some(v) = self.formatter.rt.get().get_relative(i8_value) {
                    return sink.with_part(parts::LITERAL, |s| s.write_str(v));
                }
            }
        }

        if self.is_negative {
            &self.formatter.rt.get().past
        } else {
            &self.formatter.rt.get().future
        }
        .get((&self.value).into(), &self.formatter.plural_rules)
        .interpolate((self.formatter.decimal_formatter.format(&self.value),))
        .write_to_parts(sink)
    }
}

writeable::impl_display_with_writeable!(FormattedRelativeTime<'_>);
