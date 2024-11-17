use crate::error::{FendError, Interrupt};
use crate::format::Format;
use crate::num::bigrat::{BigRat, FormattedBigRat};
use crate::num::Exact;
use crate::num::{Base, FormattingStyle};
use crate::result::FResult;
use crate::serialize::{Deserialize, Serialize};
use crate::DecimalSeparatorStyle;
use std::cmp::Ordering;
use std::ops::Neg;
use std::{fmt, hash, io};

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

	pub(crate) fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
		match &self.pattern {
			Pattern::Simple(s) => {
				1u8.serialize(write)?;
				s.serialize(write)?;
			}
			Pattern::Pi(n) => {
				2u8.serialize(write)?;
				n.serialize(write)?;
			}
		}
		Ok(())
	}

	pub(crate) fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		Ok(Self {
			pattern: match u8::deserialize(read)? {
				1 => Pattern::Simple(BigRat::deserialize(read)?),
				2 => Pattern::Pi(BigRat::deserialize(read)?),
				_ => return Err(FendError::DeserializationError),
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
				let num = BigRat::from(3_141_592_653_589_793_238);
				let den = BigRat::from(1_000_000_000_000_000_000);
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
				if let Ok(integer) = n.clone().mul(&6.into(), int)?.try_as_usize(int) {
					// values from https://en.wikipedia.org/wiki/Exact_trigonometric_values
					if integer % 6 == 0 {
						return Ok(Exact::new(Self::from(0), true));
					} else if integer % 12 == 3 {
						return Ok(Exact::new(Self::from(1), true));
					} else if integer % 12 == 9 {
						return Ok(Exact::new(-Self::from(1), true));
					} else if integer % 12 == 1 || integer % 12 == 5 {
						return Exact::new(Self::from(1), true)
							.div(&Exact::new(2.into(), true), int);
					} else if integer % 12 == 7 || integer % 12 == 11 {
						return Exact::new(-Self::from(1), true)
							.div(&Exact::new(2.into(), true), int);
					}
				}
				let s = Self {
					pattern: Pattern::Pi(n),
				};
				s.approximate(int)?.sin(int)?.apply(Self::from)
			}
		})
	}

	pub(crate) fn cos<I: Interrupt>(self, int: &I) -> FResult<Exact<Self>> {
		// cos(x) = sin(x + pi/2)
		let half_pi = Exact::new(Self::pi(), true).div(&Exact::new(Self::from(2), true), int)?;
		Exact::new(self, true).add(half_pi, int)?.value.sin(int)
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
		if style == FormattingStyle::Exact && !self.is_zero() {
			if let Pattern::Pi(_) = self.pattern {
				pi = true;
			}
		}

		let term = match (imag, pi) {
			(false, false) => "",
			(false, true) => "\u{3c0}", // pi symbol
			(true, false) => "i",
			(true, true) => "\u{3c0}i",
		};

		let mut override_exact = true;

		let rat = match &self.pattern {
			Pattern::Simple(f) => f.clone(),
			Pattern::Pi(f) => {
				if pi {
					f.clone()
				} else {
					override_exact = false;
					if style == FormattingStyle::Auto {
						style = FormattingStyle::DecimalPlaces(10);
					}
					self.clone().approximate(int)?
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
		if let Pattern::Simple(n) = &rhs.pattern {
			if n == &1.into() {
				return Ok(Exact::new(self, true));
			}
		}

		// 1^x == 1
		if let Pattern::Simple(n) = &self.pattern {
			if n == &1.into() {
				return Ok(Exact::new(1.into(), true));
			}
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
