use crate::DecimalSeparatorStyle;
use crate::error::{FendError, Interrupt};
use crate::format::Format;
use crate::num::Exact;
use crate::num::bigrat::{BigRat, FormattedBigRat};
use crate::num::{Base, FormattingStyle};
use crate::result::FResult;
use crate::serialize::CborValue;
use std::cmp::Ordering;
use std::ops::Neg;
use std::{fmt, hash};

use super::bigrat;
use super::biguint::BigUint;

#[derive(Clone)]
pub(crate) struct Real {
	pattern: Pattern,
}

impl fmt::Debug for Real {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		match &self.pattern {
			Pattern::Simple(x) => write!(f, "{x:?}"),
			Pattern::Pi(x) => {
				if x.is_definitely_one() {
					write!(f, "pi")
				} else {
					write!(f, "{x:?} * pi")
				}
			}
		}
	}
}

#[derive(Clone, Debug)]
pub(crate) enum Pattern {
	/// a simple fraction
	Simple(BigRat),
	// n * pi
	Pi(BigRat),
}

impl hash::Hash for Real {
	fn hash<H: hash::Hasher>(&self, state: &mut H) {
		match &self.pattern {
			Pattern::Simple(r) | Pattern::Pi(r) => r.hash(state),
		}
	}
}

impl Real {
	pub(crate) fn compare<I: Interrupt>(&self, other: &Self, int: &I) -> FResult<Ordering> {
		Ok(match (&self.pattern, &other.pattern) {
			(Pattern::Simple(a), Pattern::Simple(b)) | (Pattern::Pi(a), Pattern::Pi(b)) => a.cmp(b),
			_ => {
				let a = self.clone().approximate(int)?;
				let b = other.clone().approximate(int)?;
				a.cmp(&b)
			}
		})
	}

	pub(crate) fn serialize(&self) -> CborValue {
		match &self.pattern {
			Pattern::Simple(s) => s.serialize(),
			Pattern::Pi(n) => {
				// private use tag
				CborValue::Tag(80000, Box::new(n.serialize()))
			}
		}
	}

	pub(crate) fn deserialize(value: CborValue) -> FResult<Self> {
		Ok(Self {
			pattern: match value {
				CborValue::Tag(80000, inner) => Pattern::Pi(BigRat::deserialize(*inner)?),
				value => Pattern::Simple(BigRat::deserialize(value)?),
			},
		})
	}

	pub(crate) fn is_integer(&self) -> bool {
		match &self.pattern {
			Pattern::Simple(s) => s.is_integer(),
			Pattern::Pi(_) => false,
		}
	}

	fn approximate<I: Interrupt>(self, int: &I) -> FResult<BigRat> {
		match self.pattern {
			Pattern::Simple(s) => Ok(s),
			Pattern::Pi(n) => {
				// This fraction is the 33th convergent of pi, (in the sense of continued fraction),
				// and as such, is a "best approximation of the second kind" of pi (see e.g. the
				// book of Khinchin about continued fractions).
				// The 33th convergent is the last one with both numerators and denominators
				// fitting in a u64-container.
				// The difference between this approximation and pi is less than 10^-37.
				let num = BigRat::from(2_646_693_125_139_304_345);
				let den = BigRat::from(842_468_587_426_513_207);
				let pi = num.div(&den, int)?;
				Ok(n.mul(&pi, int)?)
			}
		}
	}

	pub(crate) fn try_as_biguint<I: Interrupt>(self, int: &I) -> FResult<BigUint> {
		match self.pattern {
			Pattern::Simple(s) => s.try_as_biguint(int),
			Pattern::Pi(n) => {
				if n == 0.into() {
					Ok(BigUint::Small(0))
				} else {
					Err(FendError::CannotConvertToInteger)
				}
			}
		}
	}

	pub(crate) fn try_as_i64<I: Interrupt>(self, int: &I) -> FResult<i64> {
		match self.pattern {
			Pattern::Simple(s) => s.try_as_i64(int),
			Pattern::Pi(n) => {
				if n == 0.into() {
					Ok(0)
				} else {
					Err(FendError::CannotConvertToInteger)
				}
			}
		}
	}

	pub(crate) fn try_as_usize<I: Interrupt>(self, int: &I) -> FResult<usize> {
		match self.pattern {
			Pattern::Simple(s) => s.try_as_usize(int),
			Pattern::Pi(n) => {
				if n == 0.into() {
					Ok(0)
				} else {
					Err(FendError::CannotConvertToInteger)
				}
			}
		}
	}

	// sin works for all real numbers
	pub(crate) fn sin<I: Interrupt>(self, int: &I) -> FResult<Exact<Self>> {
		Ok(match self.pattern {
			Pattern::Simple(s) => s.sin(int)?.apply(Self::from),
			Pattern::Pi(n) => {
				if n < 0.into() {
					let s = Self {
						pattern: Pattern::Pi(n),
					};
					// sin(-x) == -sin(x)
					return Ok(-Self::sin(-s, int)?);
				}
				if let Ok(n_times_6) = n.clone().mul(&6.into(), int)?.try_as_usize(int) {
					// values from https://en.wikipedia.org/wiki/Exact_trigonometric_values
					// sin(n pi) == sin( (6n/6) pi +/- 2 pi )
					//           == sin( (6n +/- 12)/6 pi ) == sin( (6n%12)/6 pi )
					match n_times_6 % 12 {
						// sin(0) == sin(pi) == 0
						0 | 6 => return Ok(Exact::new(Self::from(0), true)),
						// sin(pi/6) == sin(5pi/6) == 1/2
						1 | 5 => {
							return Exact::new(Self::from(1), true)
								.div(&Exact::new(2.into(), true), int);
						}
						// sin(pi/2) == 1
						3 => return Ok(Exact::new(Self::from(1), true)),
						// sin(7pi/6) == sin(11pi/6) == -1/2
						7 | 11 => {
							return Exact::new(-Self::from(1), true)
								.div(&Exact::new(2.into(), true), int);
						}
						// sin(3pi/2) == -1
						9 => return Ok(Exact::new(-Self::from(1), true)),
						_ => {}
					}
				}
				let s = Self {
					pattern: Pattern::Pi(n),
				};
				s.approximate(int)?.sin(int)?.apply(Self::from)
			}
		})
	}

	// cos works for all real numbers
	pub(crate) fn cos<I: Interrupt>(self, int: &I) -> FResult<Exact<Self>> {
		Ok(match self.pattern {
			Pattern::Simple(c) => c.cos(int)?.apply(Self::from),
			Pattern::Pi(n) => {
				if n < 0.into() {
					let c = Self {
						pattern: Pattern::Pi(n),
					};
					// cos(-x) == cos(x)
					return Self::cos(-c, int);
				}
				if let Ok(n_times_6) = n.clone().mul(&6.into(), int)?.try_as_usize(int) {
					// values from https://en.wikipedia.org/wiki/Exact_trigonometric_values
					// cos(n pi) == cos( (6n/6) pi +/- 2 pi )
					//           == cos( (6n +/- 12)/6 pi ) == cos( (6n%12)/6 pi )
					match n_times_6 % 12 {
						// cos(0) == 1
						0 => return Ok(Exact::new(Self::from(1), true)),
						// cos(pi/3) == cos(5pi/3) == 1/2
						2 | 10 => {
							return Exact::new(Self::from(1), true)
								.div(&Exact::new(2.into(), true), int);
						}
						// cos(pi/2) == cos(3pi/2) == 0
						3 | 9 => return Ok(Exact::new(Self::from(0), true)),
						// cos(2pi/3) == cos(4pi/3) == -1/2
						4 | 8 => {
							return Exact::new(-Self::from(1), true)
								.div(&Exact::new(2.into(), true), int);
						}
						// cos(pi) == -1
						6 => return Ok(Exact::new(-Self::from(1), true)),
						_ => {}
					}
				}
				let c = Self {
					pattern: Pattern::Pi(n),
				};
				c.approximate(int)?.cos(int)?.apply(Self::from)
			}
		})
	}

	pub(crate) fn asin<I: Interrupt>(self, int: &I) -> FResult<Self> {
		Ok(Self::from(self.approximate(int)?.asin(int)?))
	}

	pub(crate) fn acos<I: Interrupt>(self, int: &I) -> FResult<Self> {
		Ok(Self::from(self.approximate(int)?.acos(int)?))
	}

	pub(crate) fn atan<I: Interrupt>(self, int: &I) -> FResult<Self> {
		Ok(Self::from(self.approximate(int)?.atan(int)?))
	}

	pub(crate) fn atan2<I: Interrupt>(self, rhs: Self, int: &I) -> FResult<Self> {
		Ok(Self::from(
			self.approximate(int)?.atan2(rhs.approximate(int)?, int)?,
		))
	}

	pub(crate) fn sinh<I: Interrupt>(self, int: &I) -> FResult<Self> {
		Ok(Self::from(self.approximate(int)?.sinh(int)?))
	}

	pub(crate) fn cosh<I: Interrupt>(self, int: &I) -> FResult<Self> {
		Ok(Self::from(self.approximate(int)?.cosh(int)?))
	}

	pub(crate) fn tanh<I: Interrupt>(self, int: &I) -> FResult<Self> {
		Ok(Self::from(self.approximate(int)?.tanh(int)?))
	}

	pub(crate) fn asinh<I: Interrupt>(self, int: &I) -> FResult<Self> {
		Ok(Self::from(self.approximate(int)?.asinh(int)?))
	}

	pub(crate) fn acosh<I: Interrupt>(self, int: &I) -> FResult<Self> {
		Ok(Self::from(self.approximate(int)?.acosh(int)?))
	}

	pub(crate) fn atanh<I: Interrupt>(self, int: &I) -> FResult<Self> {
		Ok(Self::from(self.approximate(int)?.atanh(int)?))
	}

	// For all logs: value must be greater than 0
	pub(crate) fn ln<I: Interrupt>(self, int: &I) -> FResult<Exact<Self>> {
		Ok(self.approximate(int)?.ln(int)?.apply(Self::from))
	}

	pub(crate) fn log2<I: Interrupt>(self, int: &I) -> FResult<Self> {
		Ok(Self::from(self.approximate(int)?.log2(int)?))
	}

	pub(crate) fn log10<I: Interrupt>(self, int: &I) -> FResult<Self> {
		Ok(Self::from(self.approximate(int)?.log10(int)?))
	}

	pub(crate) fn factorial<I: Interrupt>(self, int: &I) -> FResult<Self> {
		Ok(Self::from(self.approximate(int)?.factorial(int)?))
	}

	pub(crate) fn floor<I: Interrupt>(self, int: &I) -> FResult<Self> {
		Ok(Self::from(self.approximate(int)?.floor(int)?))
	}

	pub(crate) fn ceil<I: Interrupt>(self, int: &I) -> FResult<Self> {
		Ok(Self::from(self.approximate(int)?.ceil(int)?))
	}

	pub(crate) fn round<I: Interrupt>(self, int: &I) -> FResult<Self> {
		Ok(Self::from(self.approximate(int)?.round(int)?))
	}

	pub(crate) fn format<I: Interrupt>(
		&self,
		base: Base,
		mut style: FormattingStyle,
		imag: bool,
		use_parens_if_fraction: bool,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<Exact<Formatted>> {
		let mut pi = false;
		if style == FormattingStyle::Exact
			&& !self.is_zero()
			&& let Pattern::Pi(_) = self.pattern
		{
			pi = true;
		}

		let term = match (imag, pi) {
			(false, false) => "",
			(false, true) => "\u{3c0}", // pi symbol
			(true, false) => "i",
			(true, true) => "\u{3c0}i",
		};

		let mut override_exact = true;

		let rat = match &self.pattern {
			Pattern::Simple(f) => f,
			Pattern::Pi(f) => {
				if pi {
					f
				} else {
					override_exact = false;
					if style == FormattingStyle::Auto {
						style = FormattingStyle::DecimalPlaces(10);
					}
					&self.clone().approximate(int)?
				}
			}
		};

		let formatted = rat.format(
			&bigrat::FormatOptions {
				base,
				style,
				term,
				use_parens_if_fraction,
				decimal_separator,
			},
			int,
		)?;
		let exact = formatted.exact && override_exact;
		Ok(Exact::new(
			Formatted {
				num: formatted.value,
			},
			exact,
		))
	}

	pub(crate) fn exp<I: Interrupt>(self, int: &I) -> FResult<Exact<Self>> {
		Ok(self.approximate(int)?.exp(int)?.apply(Self::from))
	}

	pub(crate) fn pow<I: Interrupt>(self, rhs: Self, int: &I) -> FResult<Exact<Self>> {
		// x^1 == x
		if let Pattern::Simple(n) = &rhs.pattern
			&& n == &1.into()
		{
			return Ok(Exact::new(self, true));
		}

		// 1^x == 1
		if let Pattern::Simple(n) = &self.pattern
			&& n == &1.into()
		{
			return Ok(Exact::new(1.into(), true));
		}

		if let (Pattern::Simple(a), Pattern::Simple(b)) =
			(self.clone().pattern, rhs.clone().pattern)
		{
			Ok(a.pow(b, int)?.apply(Self::from))
		} else {
			Ok(self
				.approximate(int)?
				.pow(rhs.approximate(int)?, int)?
				.combine(false)
				.apply(Self::from))
		}
	}

	pub(crate) fn root_n<I: Interrupt>(self, n: &Self, int: &I) -> FResult<Exact<Self>> {
		// TODO: Combining these match blocks is not currently possible because
		// 'binding by-move and by-ref in the same pattern is unstable'
		// https://github.com/rust-lang/rust/pull/76119
		Ok(match self.pattern {
			Pattern::Simple(a) => match &n.pattern {
				Pattern::Simple(b) => a.root_n(b, int)?.apply(Self::from),
				Pattern::Pi(_) => {
					let b = n.clone().approximate(int)?;
					a.root_n(&b, int)?.apply(Self::from).combine(false)
				}
			},
			Pattern::Pi(_) => {
				let a = self.clone().approximate(int)?;
				let b = n.clone().approximate(int)?;
				a.root_n(&b, int)?.apply(Self::from).combine(false)
			}
		})
	}

	pub(crate) fn pi() -> Self {
		Self {
			pattern: Pattern::Pi(1.into()),
		}
	}

	pub(crate) fn is_zero(&self) -> bool {
		match &self.pattern {
			Pattern::Simple(a) | Pattern::Pi(a) => a.is_definitely_zero() || a == &0.into(),
		}
	}

	pub(crate) fn is_pos(&self) -> bool {
		match &self.pattern {
			Pattern::Simple(a) | Pattern::Pi(a) => !a.is_definitely_zero() && a > &0.into(),
		}
	}

	pub(crate) fn is_neg(&self) -> bool {
		match &self.pattern {
			Pattern::Simple(a) | Pattern::Pi(a) => !a.is_definitely_zero() && a < &0.into(),
		}
	}

	pub(crate) fn between_plus_minus_one_incl<I: Interrupt>(&self, int: &I) -> FResult<bool> {
		// -1 <= x <= 1
		Ok(Self::from(1).neg().compare(self, int)? != Ordering::Greater
			&& self.compare(&1.into(), int)? != Ordering::Greater)
	}

	pub(crate) fn between_plus_minus_one_excl<I: Interrupt>(&self, int: &I) -> FResult<bool> {
		// -1 < x < 1
		Ok(Self::from(1).neg().compare(self, int)? == Ordering::Less
			&& self.compare(&1.into(), int)? == Ordering::Less)
	}

	pub(crate) fn is_definitely_zero(&self) -> bool {
		match &self.pattern {
			Pattern::Simple(a) | Pattern::Pi(a) => a.is_definitely_zero(),
		}
	}

	pub(crate) fn is_definitely_one(&self) -> bool {
		match &self.pattern {
			Pattern::Simple(a) => a.is_definitely_one(),
			Pattern::Pi(_) => false,
		}
	}

	pub(crate) fn expect_rational(self) -> FResult<BigRat> {
		match self.pattern {
			Pattern::Simple(a) => Ok(a),
			Pattern::Pi(_) => Err(FendError::ExpectedARationalNumber),
		}
	}

	pub(crate) fn modulo<I: Interrupt>(self, rhs: Self, int: &I) -> FResult<Self> {
		Ok(Self::from(
			self.expect_rational()?
				.modulo(rhs.expect_rational()?, int)?,
		))
	}

	pub(crate) fn bitwise<I: Interrupt>(
		self,
		rhs: Self,
		op: crate::ast::BitwiseBop,
		int: &I,
	) -> FResult<Self> {
		Ok(Self::from(self.expect_rational()?.bitwise(
			rhs.expect_rational()?,
			op,
			int,
		)?))
	}

	pub(crate) fn combination<I: Interrupt>(self, rhs: Self, int: &I) -> FResult<Self> {
		Ok(Self::from(
			self.expect_rational()?
				.combination(rhs.expect_rational()?, int)?,
		))
	}

	pub(crate) fn permutation<I: Interrupt>(self, rhs: Self, int: &I) -> FResult<Self> {
		Ok(Self::from(
			self.expect_rational()?
				.permutation(rhs.expect_rational()?, int)?,
		))
	}
}

impl Exact<Real> {
	pub(crate) fn add<I: Interrupt>(self, rhs: Self, int: &I) -> FResult<Self> {
		if self.exact && self.value.is_zero() {
			return Ok(rhs);
		} else if rhs.exact && rhs.value.is_zero() {
			return Ok(self);
		}
		let args_exact = self.exact && rhs.exact;
		Ok(
			match (self.clone().value.pattern, rhs.clone().value.pattern) {
				(Pattern::Simple(a), Pattern::Simple(b)) => {
					Self::new(a.add(b, int)?.into(), args_exact)
				}
				(Pattern::Pi(a), Pattern::Pi(b)) => Self::new(
					Real {
						pattern: Pattern::Pi(a.add(b, int)?),
					},
					args_exact,
				),
				_ => {
					let a = self.value.approximate(int)?;
					let b = rhs.value.approximate(int)?;
					Self::new(a.add(b, int)?.into(), false)
				}
			},
		)
	}

	pub(crate) fn mul<I: Interrupt>(self, rhs: Exact<&Real>, int: &I) -> FResult<Self> {
		if self.exact && self.value.is_zero() {
			return Ok(self);
		} else if rhs.exact && rhs.value.is_zero() {
			return Ok(Self::new(rhs.value.clone(), rhs.exact));
		}
		let args_exact = self.exact && rhs.exact;
		Ok(match self.value.pattern {
			Pattern::Simple(a) => match &rhs.value.pattern {
				Pattern::Simple(b) => Self::new(a.mul(b, int)?.into(), args_exact),
				Pattern::Pi(b) => Self::new(
					Real {
						pattern: Pattern::Pi(a.mul(b, int)?),
					},
					args_exact,
				),
			},
			Pattern::Pi(a) => match &rhs.value.pattern {
				Pattern::Simple(b) => Self::new(
					Real {
						pattern: Pattern::Pi(a.mul(b, int)?),
					},
					args_exact,
				),
				Pattern::Pi(_) => Self::new(
					Real {
						pattern: Pattern::Pi(a.mul(&rhs.value.clone().approximate(int)?, int)?),
					},
					false,
				),
			},
		})
	}

	pub(crate) fn div<I: Interrupt>(self, rhs: &Self, int: &I) -> FResult<Self> {
		if rhs.value.is_zero() {
			return Err(FendError::DivideByZero);
		}
		if self.exact && self.value.is_zero() {
			return Ok(self);
		}
		Ok(match self.value.pattern {
			Pattern::Simple(a) => match &rhs.value.pattern {
				Pattern::Simple(b) => Self::new(a.div(b, int)?.into(), self.exact && rhs.exact),
				Pattern::Pi(_) => Self::new(
					a.div(&rhs.value.clone().approximate(int)?, int)?.into(),
					false,
				),
			},
			Pattern::Pi(a) => match &rhs.value.pattern {
				Pattern::Simple(b) => Self::new(
					Real {
						pattern: Pattern::Pi(a.div(b, int)?),
					},
					self.exact && rhs.exact,
				),
				Pattern::Pi(b) => Self::new(a.div(b, int)?.into(), self.exact && rhs.exact),
			},
		})
	}
}

impl Neg for Real {
	type Output = Self;

	fn neg(self) -> Self {
		match self.pattern {
			Pattern::Simple(s) => Self::from(-s),
			Pattern::Pi(n) => Self {
				pattern: Pattern::Pi(-n),
			},
		}
	}
}

impl From<u64> for Real {
	fn from(i: u64) -> Self {
		Self {
			pattern: Pattern::Simple(i.into()),
		}
	}
}

impl From<BigRat> for Real {
	fn from(n: BigRat) -> Self {
		Self {
			pattern: Pattern::Simple(n),
		}
	}
}

#[derive(Debug)]
pub(crate) struct Formatted {
	num: FormattedBigRat,
}

impl fmt::Display for Formatted {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(f, "{}", self.num)
	}
}

#[cfg(test)]
mod tests {
	use super::*;

	#[test]
	fn test_pi() {
		let int = &crate::interrupt::Never;
		let pi = Real {
			pattern: Pattern::Pi(BigRat::from(1u64)),
		};
		let pi = pi.approximate(int).unwrap();
		let pi_f64 = pi.clone().into_f64(int).unwrap();

		assert!((pi_f64 - std::f64::consts::PI).abs() < 3. * f64::EPSILON);

		// = 31_415_926_535_897_932_384_626_433_832_795_028_841 / 1O^37
		let ten_power_37_pi_floor = BigRat::from(BigUint::Large(vec![
			12_315_011_256_901_221_737,
			1_703_060_790_043_277_294,
		]));
		// = 31_415_926_535_897_932_384_626_433_832_795_028_842 / 1O^37
		let ten_power_37_pi_ceil = BigRat::from(BigUint::Large(vec![
			12_315_011_256_901_221_738,
			1_703_060_790_043_277_294,
		]));
		// = 10^37
		let ten_power_37 = BigRat::from(BigUint::Large(vec![
			68_739_955_140_067_328,
			542_101_086_242_752_217,
		]));
		let ten_power_minus_37 = BigRat::from(1).div(&ten_power_37, int).unwrap();

		let pi_floor = ten_power_37_pi_floor.mul(&ten_power_minus_37, int).unwrap();
		let pi_ceil = ten_power_37_pi_ceil.mul(&ten_power_minus_37, int).unwrap();

		// pi_ceil - pi_floor = 10^37
		assert_eq!(
			pi_ceil.clone().add(pi_floor.clone().neg(), int).unwrap(),
			ten_power_minus_37
		);

		let pi_minus_pi_floor = pi.clone().add(pi_floor.clone().neg(), int).unwrap();
		let pi_ceil_minus_pi = pi_ceil.clone().add(pi.clone().neg(), int).unwrap();

		// pi_floor < pi < pi_ceil
		assert!(pi_floor < pi && pi < pi_ceil);
		// 0 < pi - pi_floor < 10^-37
		assert!(BigRat::from(0) < pi_minus_pi_floor && pi_minus_pi_floor < ten_power_minus_37);
		// 0 < pi_ceil - pi < 10^-37
		assert!(BigRat::from(0) < pi_ceil_minus_pi && pi_ceil_minus_pi < ten_power_minus_37);
	}

	#[test]
	fn test_sin_exact_values() {
		let int = &crate::interrupt::Never;
		let angles = [
			(2, 1, -1),  // -2π
			(11, 6, -1), // -11π/6
			(3, 2, -1),  // -3π/2
			(7, 6, -1),  // -7π/6
			(1, 1, -1),  // -π
			(5, 6, -1),  // -5π/6
			(1, 2, -1),  // -π/2
			(1, 6, -1),  // -π/6
			(0, 1, 1),   // 0
			(1, 6, 1),   // π/6
			(1, 2, 1),   // π/2
			(5, 6, 1),   // 5π/6
			(1, 1, 1),   // π
			(7, 6, 1),   // 7π/6
			(3, 2, 1),   // 3π/2
			(11, 6, 1),  // 11π/6
			(2, 1, 1),   // 2π
		]
		.map(|(num, den, sign)| {
			(
				Real {
					pattern: Pattern::Pi(BigRat::from(num).div(&BigRat::from(den), int).unwrap()),
				},
				sign,
			)
		})
		.map(|(br, sign)| if sign > 0 { br } else { -br });
		let expected_sinuses = [
			(0, 1, 1),  // 0
			(1, 2, 1),  // 1/2
			(1, 1, 1),  // 1
			(1, 2, 1),  // 1/2
			(0, 1, 1),  // 0
			(1, 2, -1), // -1/2
			(1, 1, -1), // -1
			(1, 2, -1), // -1/2
			(0, 1, 1),  // 0
			(1, 2, 1),  // 1/2
			(1, 1, 1),  // 1
			(1, 2, 1),  // 1/2
			(0, 1, 1),  // 0
			(1, 2, -1), // -1/2
			(1, 1, -1), // -1
			(1, 2, -1), // -1/2
			(0, 1, 1),  // 0
		]
		.map(|(num, den, sign)| {
			(
				BigRat::from(num).div(&BigRat::from(den), int).unwrap(),
				sign,
			)
		})
		.map(|(br, sign)| if sign > 0 { br } else { -br });

		let actual_sinuses = angles.map(|r| r.sin(int).unwrap().value);

		for (actual, expected) in actual_sinuses.into_iter().zip(expected_sinuses) {
			assert_eq!(actual.approximate(int).unwrap(), expected);
		}
	}

	#[test]
	fn test_cos_exact_values() {
		let int = &crate::interrupt::Never;
		let angles = [
			(2, 1, -1), // -2π
			(5, 3, -1), // -5π/3
			(3, 2, -1), // -3π/2
			(4, 3, -1), // -4π/3
			(1, 1, -1), // -π
			(2, 3, -1), // -2π/3
			(1, 2, -1), // -π/2
			(1, 3, -1), // -π/3
			(0, 1, 1),  // 0
			(1, 3, 1),  // π/3
			(1, 2, 1),  // π/2
			(2, 3, 1),  // 2π/3
			(1, 1, 1),  // π
			(4, 3, 1),  // 4π/3
			(3, 2, 1),  // 3π/2
			(5, 3, 1),  // 5π/3
			(2, 1, 1),  // 2π
		]
		.map(|(num, den, sign)| {
			(
				Real {
					pattern: Pattern::Pi(BigRat::from(num).div(&BigRat::from(den), int).unwrap()),
				},
				sign,
			)
		})
		.map(|(br, sign)| if sign > 0 { br } else { -br });
		let expected_cosines = [
			(1, 1, 1),  // 1
			(1, 2, 1),  // 1/2
			(0, 1, 1),  // 0
			(1, 2, -1), // -1/2
			(1, 1, -1), // -1
			(1, 2, -1), // -1/2
			(0, 1, 1),  // 0
			(1, 2, 1),  // 1/2
			(1, 1, 1),  // 1
			(1, 2, 1),  // 1/2
			(0, 1, 1),  // 0
			(1, 2, -1), // -1/2
			(1, 1, -1), // -1
			(1, 2, -1), // -1/2
			(0, 1, 1),  // 0
			(1, 2, 1),  // 1/2
			(1, 1, 1),  // 1
		]
		.map(|(num, den, sign)| {
			(
				BigRat::from(num).div(&BigRat::from(den), int).unwrap(),
				sign,
			)
		})
		.map(|(br, sign)| if sign > 0 { br } else { -br });

		let actual_cosines = angles.map(|r| r.cos(int).unwrap().value);

		for (actual, expected) in actual_cosines.into_iter().zip(expected_cosines) {
			assert_eq!(actual.approximate(int).unwrap(), expected);
		}
	}
}
