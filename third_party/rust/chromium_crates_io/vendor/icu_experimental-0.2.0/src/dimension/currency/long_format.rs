// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use fixed_decimal::FixedDecimal;

use icu_decimal::FixedDecimalFormatter;
use icu_plurals::PluralRules;
use writeable::Writeable;

use crate::dimension::provider::currency_patterns::CurrencyPatternsDataV1;
use crate::dimension::provider::extended_currency::CurrencyExtendedDataV1;

use super::CurrencyCode;

pub struct LongFormattedCurrency<'l> {
    pub(crate) value: &'l FixedDecimal,
    // TODO: use this if the display name is not exist and make the extended data optional.
    pub(crate) _currency_code: CurrencyCode,
    pub(crate) extended: &'l CurrencyExtendedDataV1<'l>,
    pub(crate) patterns: &'l CurrencyPatternsDataV1<'l>,
    pub(crate) fixed_decimal_formatter: &'l FixedDecimalFormatter,
    pub(crate) plural_rules: &'l PluralRules,
}

writeable::impl_display_with_writeable!(LongFormattedCurrency<'_>);

impl Writeable for LongFormattedCurrency<'_> {
    fn write_to<W>(&self, sink: &mut W) -> core::result::Result<(), core::fmt::Error>
    where
        W: core::fmt::Write + ?Sized,
    {
        let operands = self.value.into();
        let display_name = self.extended.display_names.get(operands, self.plural_rules);
        let pattern = self.patterns.patterns.get(operands, self.plural_rules);
        let formatted_value = self.fixed_decimal_formatter.format(self.value);
        let interpolated = pattern.interpolate((formatted_value, display_name));
        interpolated.write_to(sink)
    }
}

// TODO: add more tests for this module to cover more locales & currencies.
#[cfg(test)]
mod tests {
    use icu_locale_core::locale;
    use tinystr::*;
    use writeable::assert_writeable_eq;

    use crate::dimension::currency::{long_formatter::LongCurrencyFormatter, CurrencyCode};

    #[test]
    pub fn test_en_us() {
        let locale = locale!("en-US").into();
        let currency_code = CurrencyCode(tinystr!(3, "USD"));
        let fmt = LongCurrencyFormatter::try_new(locale, &currency_code).unwrap();

        // Positive case
        let positive_value = "12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&positive_value, currency_code);
        assert_writeable_eq!(formatted_currency, "12,345.67 US dollars");

        // Negative case
        let negative_value = "-12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&negative_value, currency_code);
        assert_writeable_eq!(formatted_currency, "-12,345.67 US dollars");
    }

    #[test]
    pub fn test_fr_fr() {
        let locale = locale!("fr-FR").into();
        let currency_code = CurrencyCode(tinystr!(3, "EUR"));
        let fmt = LongCurrencyFormatter::try_new(locale, &currency_code).unwrap();

        // Positive case
        let positive_value = "12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&positive_value, currency_code);
        assert_writeable_eq!(formatted_currency, "12\u{202f}345,67 euros");

        // Negative case
        let negative_value = "-12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&negative_value, currency_code);
        assert_writeable_eq!(formatted_currency, "-12\u{202f}345,67 euros");
    }

    #[test]
    pub fn test_ar_eg() {
        let locale = locale!("ar-EG").into();
        let currency_code = CurrencyCode(tinystr!(3, "EGP"));
        let fmt = LongCurrencyFormatter::try_new(locale, &currency_code).unwrap();

        // Positive case
        let positive_value = "12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&positive_value, currency_code);
        assert_writeable_eq!(formatted_currency, "١٢٬٣٤٥٫٦٧ جنيه مصري");

        // Negative case
        let negative_value = "-12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&negative_value, currency_code);
        assert_writeable_eq!(formatted_currency, "\u{61c}-١٢٬٣٤٥٫٦٧ جنيه مصري");
    }
}
