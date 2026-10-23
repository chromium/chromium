// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::dimension::provider::units::display_names::UnitsDisplayNames;
use fixed_decimal::Decimal;
use icu_decimal::DecimalFormatter;
use icu_plurals::PluralRules;
use writeable::{Writeable, impl_display_with_writeable};

#[derive(Debug)]
pub struct FormattedUnit<'l> {
    pub(crate) value: &'l Decimal,
    // TODO: review using options and essentials.
    // pub(crate) _options: &'l UnitsFormatterOptions,
    // pub(crate) essential: &'l UnitsEssentials<'l>,
    pub(crate) display_name: &'l UnitsDisplayNames<'l>,
    pub(crate) decimal_formatter: &'l DecimalFormatter,
    pub(crate) plural_rules: &'l PluralRules,
}

impl Writeable for FormattedUnit<'_> {
    fn write_to_parts<W>(&self, sink: &mut W) -> Result<(), core::fmt::Error>
    where
        W: writeable::PartsWrite + ?Sized,
    {
        self.display_name
            .get(self.value.into(), self.plural_rules)
            .interpolate((self.decimal_formatter.format(self.value),))
            .write_to_parts(sink)
    }
}

impl_display_with_writeable!(FormattedUnit<'_>);
