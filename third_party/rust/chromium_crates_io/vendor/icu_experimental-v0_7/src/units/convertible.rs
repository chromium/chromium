// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use num_bigint::BigInt;
use num_rational::Ratio;
use num_traits::ToPrimitive;

/// A trait for types that can be converted between two units.
pub trait Convertible: Clone {
    type Factor: core::fmt::Debug + Clone;
    type Addend: core::fmt::Debug + Clone;
    type Result: core::fmt::Debug;

    /// Computes `self * factor + addend`.
    fn mul_add(self, factor: &Self::Factor, addend: &Self::Addend) -> Self::Result;

    /// Computes `self * factor`
    fn mul(self, factor: &Self::Factor) -> Self::Result;

    /// Computes `1/(self * factor)`
    fn reciprocal_mul(self, factor: &Self::Factor) -> Self::Result;

    /// Converts a [`Ratio<BigInt>`] to a [`Self::Factor`].
    fn factor_from_ratio_bigint(factor: Ratio<BigInt>) -> Self::Factor;

    /// Converts a [`Ratio<BigInt>`] to a [`Self::Addend`].
    fn addend_from_ratio_bigint(addend: Ratio<BigInt>) -> Self::Addend;
}

impl Convertible for &'_ Ratio<BigInt> {
    type Factor = Ratio<BigInt>;
    type Addend = Ratio<BigInt>;
    type Result = Ratio<BigInt>;

    // Exact
    fn mul(self, factor: &Self::Factor) -> Self::Result {
        self * factor
    }

    // Exact
    fn mul_add(self, factor: &Self::Factor, addend: &Self::Addend) -> Self::Result {
        self * factor + addend
    }

    // Exact
    fn reciprocal_mul(self, factor: &Self::Factor) -> Self::Result {
        (self * factor).recip()
    }

    fn factor_from_ratio_bigint(factor: Ratio<BigInt>) -> Self::Factor {
        factor
    }

    fn addend_from_ratio_bigint(addend: Ratio<BigInt>) -> Self::Addend {
        addend
    }
}

impl Convertible for f64 {
    type Factor = f64;
    type Addend = f64;
    type Result = f64;

    // TODO: reduce error
    fn mul(self, factor: &Self::Factor) -> Self::Result {
        self * factor
    }

    // TODO: reduce error
    fn mul_add(self, factor: &Self::Factor, addend: &Self::Addend) -> Self::Result {
        self * factor + addend
    }

    // TODO: reduce error
    fn reciprocal_mul(self, factor: &Self::Factor) -> Self::Result {
        1.0 / (self * factor)
    }

    fn factor_from_ratio_bigint(factor: Ratio<BigInt>) -> Self::Factor {
        // Ratio::<BigInt>::to_f64 is infallible
        factor.to_f64().unwrap_or(f64::NAN)
    }

    fn addend_from_ratio_bigint(addend: Ratio<BigInt>) -> Self::Addend {
        // Ratio::<BigInt>::to_f64 is infallible
        addend.to_f64().unwrap_or(f64::NAN)
    }
}
