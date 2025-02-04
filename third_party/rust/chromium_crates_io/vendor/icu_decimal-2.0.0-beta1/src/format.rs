// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Lower-level types for decimal formatting.

use crate::grouper;
use crate::options::*;
use crate::provider::*;
use fixed_decimal::FixedDecimal;
use fixed_decimal::Sign;
use writeable::Writeable;

/// An intermediate structure returned by [`FixedDecimalFormatter`](crate::FixedDecimalFormatter).
/// Use [`Writeable`][Writeable] to render the formatted decimal to a string or buffer.
#[derive(Debug, PartialEq, Clone)]
pub struct FormattedFixedDecimal<'l> {
    pub(crate) value: &'l FixedDecimal,
    pub(crate) options: &'l FixedDecimalFormatterOptions,
    pub(crate) symbols: &'l DecimalSymbolsV2<'l>,
    pub(crate) digits: &'l DecimalDigitsV1,
}

impl FormattedFixedDecimal<'_> {
    /// Returns the affixes needed for the current sign, as (prefix, suffix)
    fn get_affixes(&self) -> Option<(&str, &str)> {
        match self.value.sign() {
            Sign::None => None,
            Sign::Negative => Some(self.symbols.minus_sign_affixes()),
            Sign::Positive => Some(self.symbols.plus_sign_affixes()),
        }
    }
}

impl Writeable for FormattedFixedDecimal<'_> {
    fn write_to<W>(&self, sink: &mut W) -> core::result::Result<(), core::fmt::Error>
    where
        W: core::fmt::Write + ?Sized,
    {
        let affixes = self.get_affixes();
        if let Some(affixes) = affixes {
            sink.write_str(affixes.0)?;
        }
        let range = self.value.magnitude_range();
        let upper_magnitude = *range.end();
        for m in range.rev() {
            if m == -1 {
                sink.write_str(self.symbols.decimal_separator())?;
            }
            #[allow(clippy::indexing_slicing)] // digit_at in 0..=9
            sink.write_char(self.digits.digits[self.value.digit_at(m) as usize])?;
            if grouper::check(
                upper_magnitude,
                m,
                self.options.grouping_strategy,
                &self.symbols.grouping_sizes,
            ) {
                sink.write_str(self.symbols.grouping_separator())?;
            }
        }
        if let Some(affixes) = affixes {
            sink.write_str(affixes.1)?;
        }
        Ok(())
    }
}

writeable::impl_display_with_writeable!(FormattedFixedDecimal<'_>);

#[cfg(test)]
mod tests {
    use icu_locale_core::locale;
    use writeable::assert_writeable_eq;

    use crate::FixedDecimalFormatter;

    #[test]
    pub fn test_es_mx() {
        let locale = locale!("es-MX").into();
        let fmt = FixedDecimalFormatter::try_new(locale, Default::default()).unwrap();
        let fd = "12345.67".parse().unwrap();
        assert_writeable_eq!(fmt.format(&fd), "12,345.67");
    }
}
