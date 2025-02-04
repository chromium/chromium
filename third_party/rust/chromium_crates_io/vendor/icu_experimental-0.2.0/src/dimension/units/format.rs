// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Experimental.

use crate::dimension::provider::units::UnitsDisplayNameV1;
use fixed_decimal::FixedDecimal;
use icu_decimal::FixedDecimalFormatter;
use icu_plurals::PluralRules;
use writeable::{impl_display_with_writeable, Writeable};

pub struct FormattedUnit<'l> {
    pub(crate) value: &'l FixedDecimal,
    // TODO: review using options and essentials.
    // pub(crate) _options: &'l UnitsFormatterOptions,
    // pub(crate) essential: &'l UnitsEssentialsV1<'l>,
    pub(crate) display_name: &'l UnitsDisplayNameV1<'l>,
    pub(crate) fixed_decimal_formatter: &'l FixedDecimalFormatter,
    pub(crate) plural_rules: &'l PluralRules,
}

impl Writeable for FormattedUnit<'_> {
    fn write_to<W>(&self, sink: &mut W) -> core::result::Result<(), core::fmt::Error>
    where
        W: core::fmt::Write + ?Sized,
    {
        self.display_name
            .patterns
            .get(self.value.into(), self.plural_rules)
            .interpolate((self.fixed_decimal_formatter.format(self.value),))
            .write_to(sink)
    }
}

impl_display_with_writeable!(FormattedUnit<'_>);

#[test]
fn test_basic() {
    use icu_locale_core::locale;
    use writeable::assert_writeable_eq;

    use crate::dimension::units::formatter::UnitsFormatter;
    use crate::dimension::units::options::{UnitsFormatterOptions, Width};

    let test_cases = [
        (
            locale!("en-US"),
            "meter",
            "1",
            UnitsFormatterOptions {
                width: Width::Long,
                ..Default::default()
            },
            "1 meter",
        ),
        (
            locale!("en-US"),
            "meter",
            "12345.67",
            UnitsFormatterOptions::default(),
            "12,345.67 m",
        ),
        (
            locale!("en-US"),
            "century",
            "12345.67",
            UnitsFormatterOptions {
                width: Width::Long,
                ..Default::default()
            },
            "12,345.67 centuries",
        ),
        (
            locale!("de-DE"),
            "meter",
            "12345.67",
            UnitsFormatterOptions::default(),
            "12.345,67 m",
        ),
        (
            locale!("ar-EG"),
            "meter",
            "12345.67",
            UnitsFormatterOptions {
                width: Width::Long,
                ..Default::default()
            },
            "١٢٬٣٤٥٫٦٧ متر",
        ),
    ];

    for (locale, unit, value, options, expected) in test_cases {
        let fmt = UnitsFormatter::try_new(locale.into(), unit, options).unwrap();
        let value = value.parse().unwrap();
        assert_writeable_eq!(fmt.format_fixed_decimal(&value), expected);
    }
}
