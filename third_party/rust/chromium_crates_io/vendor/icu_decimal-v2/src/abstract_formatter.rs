// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use fixed_decimal::UnsignedDecimal;
use icu_plurals::PluralOperands;
use writeable::Writeable;

use crate::{
    CompactDecimalFormatter, DecimalFormatter, FormattedSign, FormattedUnsignedCompactDecimal,
    FormattedUnsignedDecimal,
};

pub trait Sealed {}

/// A trait representing an abstract number formatter.
///
/// This is a building block for more complicated formatters, like currency or units.
///
/// <div class="stab unstable">
/// 🚫 This trait is sealed; it cannot be implemented by user code. If an API requests an item that implements this
/// trait, please consider using a type from the implementors listed below.
/// </div>
pub trait AbstractFormatter: core::fmt::Debug + Sealed {
    #[doc(hidden)]
    type FormattedUnsigned<'a>: Writeable
    where
        Self: 'a;

    #[doc(hidden)]
    fn format_unsigned<'a>(&'a self, value: UnsignedDecimal) -> Self::FormattedUnsigned<'a>;

    #[doc(hidden)]
    fn format_sign<'a, W: Writeable>(
        &'a self,
        value: W,
        sign: fixed_decimal::Sign,
    ) -> FormattedSign<'a, W>;

    #[doc(hidden)]
    fn plural_operands(value: &Self::FormattedUnsigned<'_>) -> PluralOperands;
}

impl Sealed for DecimalFormatter {}
impl AbstractFormatter for DecimalFormatter {
    type FormattedUnsigned<'a> = FormattedUnsignedDecimal<'a>;

    fn format_unsigned<'a>(&'a self, value: UnsignedDecimal) -> Self::FormattedUnsigned<'a> {
        self.format_unsigned(crate::Cow::Owned(value))
    }

    fn format_sign<'a, W: Writeable>(
        &'a self,
        value: W,
        sign: fixed_decimal::Sign,
    ) -> FormattedSign<'a, W> {
        self.format_sign(sign, value)
    }

    fn plural_operands(value: &Self::FormattedUnsigned<'_>) -> PluralOperands {
        value.plural_operands()
    }
}

impl Sealed for CompactDecimalFormatter {}
impl AbstractFormatter for CompactDecimalFormatter {
    type FormattedUnsigned<'a> = FormattedUnsignedCompactDecimal<'a>;

    fn format_unsigned<'a>(&'a self, value: UnsignedDecimal) -> Self::FormattedUnsigned<'a> {
        self.format_unsigned(&value)
    }

    fn format_sign<'a, W: Writeable>(
        &'a self,
        value: W,
        sign: fixed_decimal::Sign,
    ) -> FormattedSign<'a, W> {
        self.decimal_formatter.format_sign(sign, value)
    }

    fn plural_operands(value: &Self::FormattedUnsigned<'_>) -> PluralOperands {
        value.plural_operands()
    }
}
