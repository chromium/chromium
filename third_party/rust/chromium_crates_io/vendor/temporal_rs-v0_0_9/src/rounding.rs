//! Implementation of increment rounding functionality

use crate::{
    options::{RoundingMode, UnsignedRoundingMode},
    TemporalResult, TemporalUnwrap,
};

use core::{
    cmp::Ordering,
    num::NonZeroU128,
    ops::{Div, Neg},
};

use num_traits::float::FloatCore;
use num_traits::{ConstZero, Euclid, FromPrimitive, NumCast, Signed, ToPrimitive};

pub(crate) trait Roundable:
    Euclid + Div + PartialOrd + Signed + FromPrimitive + ToPrimitive + NumCast + ConstZero + Copy
{
    fn is_exact(dividend: Self, divisor: Self) -> bool;
    fn compare_remainder(dividend: Self, divisor: Self) -> Option<Ordering>;
    fn is_even_cardinal(dividend: Self, divisor: Self) -> bool;
    fn result_floor(dividend: Self, divisor: Self) -> u128;
    fn result_ceil(dividend: Self, divisor: Self) -> u128;
    fn quotient_abs(dividend: Self, divisor: Self) -> Self {
        // NOTE: Sanity debugs until proper unit tests to vet the below
        // NOTE (nekevss): quotient_abs exceeds the range of u64, so u128 must be used.
        debug_assert!(<u128 as NumCast>::from((dividend / divisor).abs()) < Some(u128::MAX));
        (dividend / divisor).abs()
    }
}

pub(crate) trait Round {
    fn round(&self, mode: RoundingMode) -> i128;
}

#[derive(Debug, Clone, Copy, PartialEq, PartialOrd)]
pub(crate) struct IncrementRounder<T: Roundable> {
    sign: bool,
    dividend: T,
    divisor: T,
}

impl<T: Roundable> IncrementRounder<T> {
    #[inline]
    pub(crate) fn from_signed_num(number: T, increment: NonZeroU128) -> TemporalResult<Self> {
        let increment = <T as NumCast>::from(increment.get()).temporal_unwrap()?;
        Ok(Self {
            sign: number >= T::ZERO,
            dividend: number,
            divisor: increment,
        })
    }
}

impl<T: Roundable> Round for IncrementRounder<T> {
    #[inline]
    fn round(&self, mode: RoundingMode) -> i128 {
        let unsigned_rounding_mode = mode.get_unsigned_round_mode(self.sign);
        let mut rounded =
            apply_unsigned_rounding_mode(self.dividend, self.divisor, unsigned_rounding_mode)
                as i128;
        if !self.sign {
            rounded = rounded.neg();
        }
        // TODO: Add unit tests for the below
        rounded
            * <i128 as NumCast>::from(self.divisor).expect("increment is representable by a u64")
    }
}

impl Roundable for i128 {
    fn is_exact(dividend: Self, divisor: Self) -> bool {
        dividend.rem_euclid(divisor) == 0
    }

    fn compare_remainder(dividend: Self, divisor: Self) -> Option<Ordering> {
        Some((dividend.abs() % divisor).cmp(&(divisor / 2)))
    }

    fn is_even_cardinal(dividend: Self, divisor: Self) -> bool {
        Roundable::result_floor(dividend, divisor).rem_euclid(2) == 0
    }

    fn result_floor(dividend: Self, divisor: Self) -> u128 {
        Roundable::quotient_abs(dividend, divisor) as u128
    }

    fn result_ceil(dividend: Self, divisor: Self) -> u128 {
        Roundable::quotient_abs(dividend, divisor) as u128 + 1
    }
}

impl Roundable for f64 {
    fn is_exact(dividend: Self, divisor: Self) -> bool {
        Roundable::quotient_abs(dividend, divisor)
            == Roundable::quotient_abs(dividend, divisor).floor()
    }

    fn compare_remainder(dividend: Self, divisor: Self) -> Option<Ordering> {
        let quotient = Roundable::quotient_abs(dividend, divisor);
        let d1 = quotient - FloatCore::floor(quotient);
        let d2 = FloatCore::ceil(quotient) - quotient;
        d1.partial_cmp(&d2)
    }

    fn is_even_cardinal(dividend: Self, divisor: Self) -> bool {
        let quotient = Roundable::quotient_abs(dividend, divisor);
        (FloatCore::floor(quotient) / (FloatCore::ceil(quotient) - FloatCore::floor(quotient))
            % 2.0)
            == 0.0
    }

    fn result_floor(dividend: Self, divisor: Self) -> u128 {
        Roundable::quotient_abs(dividend, divisor).floor() as u128
    }

    fn result_ceil(dividend: Self, divisor: Self) -> u128 {
        Roundable::quotient_abs(dividend, divisor).ceil() as u128
    }
}

impl Roundable for i64 {
    fn is_exact(dividend: Self, divisor: Self) -> bool {
        dividend.rem_euclid(divisor) == 0
    }

    fn compare_remainder(dividend: Self, divisor: Self) -> Option<Ordering> {
        Some((dividend.abs() % divisor).cmp(&(divisor / 2)))
    }

    fn is_even_cardinal(dividend: Self, divisor: Self) -> bool {
        Roundable::result_floor(dividend, divisor).rem_euclid(2) == 0
    }

    fn result_floor(dividend: Self, divisor: Self) -> u128 {
        Roundable::quotient_abs(dividend, divisor) as u128
    }

    fn result_ceil(dividend: Self, divisor: Self) -> u128 {
        Roundable::quotient_abs(dividend, divisor) as u128 + 1
    }
}

/// Applies the unsigned rounding mode.
fn apply_unsigned_rounding_mode<T: Roundable>(
    dividend: T,
    divisor: T,
    unsigned_rounding_mode: UnsignedRoundingMode,
) -> u128 {
    // is_floor
    // 1. If x is equal to r1, return r1.
    if Roundable::is_exact(dividend, divisor) {
        return Roundable::result_floor(dividend, divisor);
    }
    // 2. Assert: r1 < x < r2.
    // 3. Assert: unsignedRoundingMode is not undefined.

    // 4. If unsignedRoundingMode is zero, return r1.
    if unsigned_rounding_mode == UnsignedRoundingMode::Zero {
        return Roundable::result_floor(dividend, divisor);
    };
    // 5. If unsignedRoundingMode is infinity, return r2.
    if unsigned_rounding_mode == UnsignedRoundingMode::Infinity {
        return Roundable::result_ceil(dividend, divisor);
    };

    // 6. Let d1 be x – r1.
    // 7. Let d2 be r2 – x.
    // 8. If d1 < d2, return r1.
    // 9. If d2 < d1, return r2.
    match Roundable::compare_remainder(dividend, divisor) {
        Some(Ordering::Less) => Roundable::result_floor(dividend, divisor),
        Some(Ordering::Greater) => Roundable::result_ceil(dividend, divisor),
        Some(Ordering::Equal) => {
            // 10. Assert: d1 is equal to d2.
            // 11. If unsignedRoundingMode is half-zero, return r1.
            if unsigned_rounding_mode == UnsignedRoundingMode::HalfZero {
                return Roundable::result_floor(dividend, divisor);
            };
            // 12. If unsignedRoundingMode is half-infinity, return r2.
            if unsigned_rounding_mode == UnsignedRoundingMode::HalfInfinity {
                return Roundable::result_ceil(dividend, divisor);
            };
            // 13. Assert: unsignedRoundingMode is half-even.
            debug_assert!(unsigned_rounding_mode == UnsignedRoundingMode::HalfEven);
            // 14. Let cardinality be (r1 / (r2 – r1)) modulo 2.
            // 15. If cardinality is 0, return r1.
            if Roundable::is_even_cardinal(dividend, divisor) {
                return Roundable::result_floor(dividend, divisor);
            }
            // 16. Return r2.
            Roundable::result_ceil(dividend, divisor)
        }
        None => unreachable!(),
    }
}

#[cfg(test)]
mod tests {
    use core::num::NonZeroU128;

    use super::{IncrementRounder, Round, RoundingMode};

    #[test]
    fn neg_i128_rounding() {
        let result = IncrementRounder::<i128>::from_signed_num(-9, NonZeroU128::new(2).unwrap())
            .unwrap()
            .round(RoundingMode::Ceil);
        assert_eq!(result, -8);

        let result = IncrementRounder::<i128>::from_signed_num(-9, NonZeroU128::new(2).unwrap())
            .unwrap()
            .round(RoundingMode::Floor);
        assert_eq!(result, -10);

        let result = IncrementRounder::<i128>::from_signed_num(-14, NonZeroU128::new(3).unwrap())
            .unwrap()
            .round(RoundingMode::HalfExpand);
        assert_eq!(result, -15);
    }

    #[test]
    fn neg_f64_rounding() {
        let result = IncrementRounder::<f64>::from_signed_num(-8.5, NonZeroU128::new(1).unwrap())
            .unwrap()
            .round(RoundingMode::Ceil);
        assert_eq!(result, -8);

        let result = IncrementRounder::<f64>::from_signed_num(-8.5, NonZeroU128::new(1).unwrap())
            .unwrap()
            .round(RoundingMode::Floor);
        assert_eq!(result, -9);
    }

    #[test]
    fn dt_since_basic_rounding() {
        let result = IncrementRounder::<i128>::from_signed_num(
            -84082624864197532,
            NonZeroU128::new(1800000000000).unwrap(),
        )
        .unwrap()
        .round(RoundingMode::HalfExpand);

        assert_eq!(result, -84083400000000000);
    }
}
