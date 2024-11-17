use crate::error::{FendError, Interrupt};
use crate::num::real::{self, Real};
use crate::num::Exact;
use crate::num::{Base, FormattingStyle};
use crate::result::FResult;
use crate::DecimalSeparatorStyle;
use std::cmp::Ordering;
use std::ops::Neg;
use std::{fmt, io};

#[derive(Clone, Hash)]
pub(crate) struct Complex {
	real: Real,
	imag: Real,
}

impl fmt::Debug for Complex {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(f, "{:?}", self.real)?;
		if !self.imag.is_definitely_zero() {
			write!(f, " + {:?}i", self.imag)?;
		}
		Ok(())
	}
}

#[derive(Copy, Clone, Eq, PartialEq)]
pub(crate) enum UseParentheses {
	No,
	IfComplex,
	IfComplexOrFraction,
}

impl Complex {
	pub(crate) fn compare<I: Interrupt>(&self, other: &Self, int: &I) -> FResult<Option<Ordering>> {
		if self.imag().is_zero() && other.imag().is_zero() {
			Ok(Some(self.real().compare(&other.real(), int)?))
		} else {
			Ok(None)
		}
	}

	pub(crate) fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
		self.real.serialize(write)?;
		self.imag.serialize(write)?;
		Ok(())
	}

	pub(crate) fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		Ok(Self {
			real: Real::deserialize(read)?,
			imag: Real::deserialize(read)?,
		})
	}

	pub(crate) fn try_as_real(self) -> FResult<Real> {
		if !self.imag.is_zero() {
			return Err(FendError::ComplexToInteger);
		}
		Ok(self.real)
	}

	pub(crate) fn try_as_usize<I: Interrupt>(self, int: &I) -> FResult<usize> {
		if !self.imag.is_zero() {
			return Err(FendError::ComplexToInteger);
		}
		self.real.try_as_usize(int)
	}

	pub(crate) fn try_as_i64<I: Interrupt>(self, int: &I) -> FResult<i64> {
		if !self.imag.is_zero() {
			return Err(FendError::ComplexToInteger);
		}
		self.real.try_as_i64(int)
	}

	#[inline]
	pub(crate) fn real(&self) -> Real {
		self.real.clone()
	}

	#[inline]
	pub(crate) fn imag(&self) -> Real {
		self.imag.clone()
	}

	pub(crate) fn conjugate(self) -> Self {
		Self {
			real: self.real,
			imag: -self.imag,
		}
	}

	pub(crate) fn factorial<I: Interrupt>(self, int: &I) -> FResult<Self> {
		if !self.imag.is_zero() {
			return Err(FendError::FactorialComplex);
		}
		Ok(Self {
			real: self.real.factorial(int)?,
			imag: self.imag,
		})
	}

	pub(crate) fn exp<I: Interrupt>(self, int: &I) -> FResult<Exact<Self>> {
		// e^(a + bi) = e^a * e^(bi) = e^a * (cos(b) + i * sin(b))
		let r = self.real.exp(int)?;
		let exact = r.exact;

		Ok(Exact::new(
			Self {
				real: r.clone().mul(self.imag.clone().cos(int)?.re(), int)?.value,
				imag: r.mul(self.imag.sin(int)?.re(), int)?.value,
			},
			exact,
		))
	}

	fn pow_n<I: Interrupt>(self, n: usize, int: &I) -> FResult<Exact<Self>> {
		if n == 0 {
			return Ok(Exact::new(Self::from(1), true));
		}

		let mut result = Exact::new(Self::from(1), true);
		let mut base = Exact::new(self, true);
		let mut exp = n;
		while exp > 0 {
			if exp & 1 == 1 {
				result = result.mul(&base, int)?;
			}
			base = base.clone().mul(&base, int)?;
			exp >>= 1;
		}

		Ok(result)
	}

	pub(crate) fn pow<I: Interrupt>(self, rhs: Self, int: &I) -> FResult<Exact<Self>> {
		if !rhs.real.is_integer() || !rhs.imag.is_integer() {
			return self.frac_pow(rhs, int);
		}

		if self.imag.is_zero() && rhs.imag.is_zero() {
			let real = self.real.pow(rhs.real, int)?;
			Ok(Exact::new(
				Self {
					real: real.value,
					imag: 0.into(),
				},
				real.exact,
			))
		} else {
			let rem = rhs.clone().real.modulo(4.into(), int);
			// Reduced case: (ix)^y = x^y * i^y
			if self.real.is_zero() && rhs.imag.is_zero() {
				if let Ok(n) = rhs.real.try_as_usize(int) {
					return self.pow_n(n, int);
				}

				let mut result = Exact::new(
					match rem.and_then(|rem| rem.try_as_usize(int)) {
						Ok(0) => 1.into(),
						Ok(1) => Self {
							real: 0.into(),
							imag: 1.into(),
						},
						Ok(2) => Self {
							real: Real::from(1).neg(),
							imag: 0.into(),
						},
						Ok(3) => Self {
							real: 0.into(),
							imag: Real::from(1).neg(),
						},
						Ok(_) | Err(_) => unreachable!("modulo 4 should always be 0, 1, 2, or 3"),
					},
					true,
				);

				if !self.imag.is_definitely_one() {
					result = self
						.imag
						.pow(2.into(), int)?
						.apply(Self::from)
						.mul(&result, int)?;
				}

				return Ok(result);
			}

			// z^w = e^(w * ln(z))
			let exp = self.ln(int)?.mul(&Exact::new(rhs, true), int)?;
			Ok(exp.value.exp(int)?.combine(exp.exact))
		}
	}

	pub(crate) fn i() -> Self {
		Self {
			real: 0.into(),
			imag: 1.into(),
		}
	}

	pub(crate) fn pi() -> Self {
		Self {
			real: Real::pi(),
			imag: 0.into(),
		}
	}

	pub(crate) fn abs<I: Interrupt>(self, int: &I) -> FResult<Exact<Real>> {
		Ok(if self.imag.is_zero() {
			if self.real.is_neg() {
				Exact::new(-self.real, true)
			} else {
				Exact::new(self.real, true)
			}
		} else if self.real.is_zero() {
			if self.imag.is_neg() {
				Exact::new(-self.imag, true)
			} else {
				Exact::new(self.imag, true)
			}
		} else {
			let power = self.real.pow(2.into(), int)?;
			let power2 = self.imag.pow(2.into(), int)?;
			let real = power.add(power2, int)?;
			let result = real.value.root_n(&Real::from(2), int)?;
			result.combine(real.exact)
		})
	}

	pub(crate) fn floor<I: Interrupt>(self, int: &I) -> FResult<Exact<Real>> {
		Ok(Exact::new(self.expect_real()?.floor(int)?, true))
	}

	pub(crate) fn ceil<I: Interrupt>(self, int: &I) -> FResult<Exact<Real>> {
		Ok(Exact::new(self.expect_real()?.ceil(int)?, true))
	}

	pub(crate) fn round<I: Interrupt>(self, int: &I) -> FResult<Exact<Real>> {
		Ok(Exact::new(self.expect_real()?.round(int)?, true))
	}

	pub(crate) fn arg<I: Interrupt>(self, int: &I) -> FResult<Exact<Real>> {
		Ok(Exact::new(self.imag.atan2(self.real, int)?, false))
	}

	pub(crate) fn format<I: Interrupt>(
		&self,
		exact: bool,
		style: FormattingStyle,
		base: Base,
		use_parentheses: UseParentheses,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<Exact<Formatted>> {
		let style = if !exact && style == FormattingStyle::Auto {
			FormattingStyle::DecimalPlaces(10)
		} else if !self.imag.is_zero() && style == FormattingStyle::Auto {
			FormattingStyle::Exact
		} else {
			style
		};

		if self.imag.is_zero() {
			let use_parens = use_parentheses == UseParentheses::IfComplexOrFraction;
			let x = self
				.real
				.format(base, style, false, use_parens, decimal_separator, int)?;
			return Ok(Exact::new(
				Formatted {
					first_component: x.value,
					separator: "",
					second_component: None,
					use_parentheses: false,
				},
				exact && x.exact,
			));
		}

		Ok(if self.real.is_zero() {
			let use_parens = use_parentheses == UseParentheses::IfComplexOrFraction;
			let x = self
				.imag
				.format(base, style, true, use_parens, decimal_separator, int)?;
			Exact::new(
				Formatted {
					first_component: x.value,
					separator: "",
					second_component: None,
					use_parentheses: false,
				},
				exact && x.exact,
			)
		} else {
			let mut exact = exact;
			let real_part = self
				.real
				.format(base, style, false, false, decimal_separator, int)?;
			exact = exact && real_part.exact;
			let (positive, imag_part) = if self.imag.is_pos() {
				(
					true,
					self.imag
						.format(base, style, true, false, decimal_separator, int)?,
				)
			} else {
				(
					false,
					(-self.imag.clone()).format(
						base,
						style,
						true,
						false,
						decimal_separator,
						int,
					)?,
				)
			};
			exact = exact && imag_part.exact;
			let separator = if positive { " + " } else { " - " };
			Exact::new(
				Formatted {
					first_component: real_part.value,
					separator,
					second_component: Some(imag_part.value),
					use_parentheses: use_parentheses == UseParentheses::IfComplex
						|| use_parentheses == UseParentheses::IfComplexOrFraction,
				},
				exact,
			)
		})
	}
	pub(crate) fn frac_pow<I: Interrupt>(self, n: Self, int: &I) -> FResult<Exact<Self>> {
		if self.imag.is_zero() && n.imag.is_zero() && !self.real.is_neg() {
			Ok(self.real.pow(n.real, int)?.apply(Self::from))
		} else {
			let exponent = self.ln(int)?.mul(&Exact::new(n, true), int)?;
			Ok(exponent.value.exp(int)?.combine(exponent.exact))
		}
	}

	fn expect_real(self) -> FResult<Real> {
		if self.imag.is_zero() {
			Ok(self.real)
		} else {
			Err(FendError::ExpectedARealNumber)
		}
	}

	pub(crate) fn sin<I: Interrupt>(self, int: &I) -> FResult<Exact<Self>> {
		// sin(a + bi) = sin(a) * cosh(b) + i * cos(a) * sinh(b)
		if self.imag.is_zero() {
			Ok(self.real.sin(int)?.apply(Self::from))
		} else {
			let cosh = Exact::new(self.imag.clone().cosh(int)?, false);
			let sinh = Exact::new(self.imag.sinh(int)?, false);

			let real = self.real.clone().sin(int)?.mul(cosh.re(), int)?;
			let imag = self.real.cos(int)?.mul(sinh.re(), int)?;

			Ok(Exact::new(
				Self {
					real: real.value,
					imag: imag.value,
				},
				real.exact && imag.exact,
			))
		}
	}

	pub(crate) fn cos<I: Interrupt>(self, int: &I) -> FResult<Exact<Self>> {
		// cos(a + bi) = cos(a) * cosh(b) - i * sin(a) * sinh(b)
		if self.imag.is_zero() {
			Ok(self.real.cos(int)?.apply(Self::from))
		} else {
			let cosh = Exact::new(self.imag.clone().cosh(int)?, false);
			let sinh = Exact::new(self.imag.sinh(int)?, false);
			let exact_real = Exact::new(self.real, true);

			let real = exact_real.value.clone().cos(int)?.mul(cosh.re(), int)?;
			let imag = exact_real.value.sin(int)?.mul(sinh.re(), int)?.neg();
			Ok(Exact::new(
				Self {
					real: real.value,
					imag: imag.value,
				},
				real.exact && imag.exact,
			))
		}
	}

	pub(crate) fn tan<I: Interrupt>(self, int: &I) -> FResult<Exact<Self>> {
		let num = self.clone().sin(int)?;
		let den = self.cos(int)?;
		num.div(den, int)
	}

	/// Calculates ln(i * z + sqrt(1 - z^2))
	/// This is used to implement asin and acos for all complex numbers
	fn asin_ln<I: Interrupt>(self, int: &I) -> FResult<Exact<Self>> {
		let half = Exact::new(Self::from(1), true).div(Exact::new(Self::from(2), true), int)?;
		let exact = Exact::new(self, true);
		let i = Exact::new(Self::i(), true);

		let sqrt = Exact::new(Self::from(1), true)
			.add(exact.clone().mul(&exact, int)?.neg(), int)?
			.try_and_then(|x| x.frac_pow(half.value, int))?;
		i.mul(&exact, int)?
			.add(sqrt, int)?
			.try_and_then(|x| x.ln(int))
	}

	pub(crate) fn asin<I: Interrupt>(self, int: &I) -> FResult<Self> {
		// Real asin is defined for -1 <= x <= 1
		if self.imag.is_zero() && self.real.between_plus_minus_one_incl(int)? {
			Ok(Self::from(self.real.asin(int)?))
		} else {
			// asin(z) = -i * ln(i * z + sqrt(1 - z^2))
			Ok(self
				.asin_ln(int)?
				.mul(&Exact::new(Self::i(), true), int)?
				.neg()
				.value)
		}
	}

	pub(crate) fn acos<I: Interrupt>(self, int: &I) -> FResult<Self> {
		// Real acos is defined for -1 <= x <= 1
		if self.imag.is_zero() && self.real.between_plus_minus_one_incl(int)? {
			Ok(Self::from(self.real.acos(int)?))
		} else {
			// acos(z) = pi/2 + i * ln(i * z + sqrt(1 - z^2))
			let half_pi = Exact::new(Self::pi(), true).div(Exact::new(Self::from(2), true), int)?;
			Ok(half_pi
				.add(
					self.asin_ln(int)?.mul(&Exact::new(Self::i(), true), int)?,
					int,
				)?
				.value)
		}
	}

	pub(crate) fn atan<I: Interrupt>(self, int: &I) -> FResult<Self> {
		// Real atan is defined for all real numbers
		if self.imag.is_zero() {
			Ok(Self::from(self.real.atan(int)?))
		} else {
			// i/2 * (ln(-iz+1) - ln(iz+1))
			let half_i = Exact::new(Self::i(), true).div(Exact::new(Self::from(2), true), int)?;
			let z1 = Exact::new(self.clone(), true)
				.mul(&Exact::new(Self::i().neg(), true), int)?
				.add(Exact::new(Self::from(1), true), int)?;
			let z2 = Exact::new(self, true)
				.mul(&Exact::new(Self::i(), true), int)?
				.add(Exact::new(Self::from(1), true), int)?;

			Ok(half_i
				.mul(
					&z1.try_and_then(|z| z.ln(int))?
						.add(z2.try_and_then(|z| z.ln(int))?.neg(), int)?,
					int,
				)?
				.value)
		}
	}

	pub(crate) fn sinh<I: Interrupt>(self, int: &I) -> FResult<Self> {
		if self.imag.is_zero() {
			Ok(Self::from(self.real.sinh(int)?))
		} else {
			// sinh(a+bi)=sinh(a)cos(b)+icosh(a)sin(b)
			let sinh = Exact::new(self.real.clone().sinh(int)?, false);
			let cos = self.imag.clone().cos(int)?;
			let cosh = Exact::new(self.real.cosh(int)?, false);
			let sin = self.imag.sin(int)?;

			Ok(Self {
				real: sinh.mul(cos.re(), int)?.value,
				imag: cosh.mul(sin.re(), int)?.value,
			})
		}
	}

	pub(crate) fn cosh<I: Interrupt>(self, int: &I) -> FResult<Self> {
		if self.imag.is_zero() {
			Ok(Self::from(self.real.cosh(int)?))
		} else {
			// cosh(a+bi)=cosh(a)cos(b)+isinh(a)sin(b)
			let cosh = Exact::new(self.real.clone().cosh(int)?, false);
			let cos = self.imag.clone().cos(int)?;
			let sinh = Exact::new(self.real.sinh(int)?, false);
			let sin = self.imag.sin(int)?;

			Ok(Self {
				real: cosh.mul(cos.re(), int)?.value,
				imag: sinh.mul(sin.re(), int)?.value,
			})
		}
	}

	pub(crate) fn tanh<I: Interrupt>(self, int: &I) -> FResult<Self> {
		if self.imag.is_zero() {
			Ok(Self::from(self.real.tanh(int)?))
		} else {
			// tanh(a+bi)=sinh(a+bi)/cosh(a+bi)
			Exact::new(self.clone().sinh(int)?, false)
				.div(Exact::new(self.cosh(int)?, false), int)
				.map(|x| x.value)
		}
	}

	pub(crate) fn asinh<I: Interrupt>(self, int: &I) -> FResult<Self> {
		// Real asinh is defined for all real numbers
		if self.imag.is_zero() {
			Ok(Self::from(self.real.asinh(int)?))
		} else {
			// asinh(z)=ln(z+sqrt(z^2+1))
			let exact = Exact::new(self, true);
			let half = Exact::new(Self::from(1), true).div(Exact::new(Self::from(2), true), int)?;
			let sqrt = exact
				.clone()
				.mul(&exact, int)?
				.add(Exact::new(Self::from(1), true), int)?
				.try_and_then(|x| x.frac_pow(half.value, int))?;
			sqrt.add(exact, int)?
				.try_and_then(|x| x.ln(int))
				.map(|x| x.value)
		}
	}

	pub(crate) fn acosh<I: Interrupt>(self, int: &I) -> FResult<Self> {
		// Real acosh is defined for x >= 1
		if self.imag.is_zero() && self.real.compare(&1.into(), int)? != Ordering::Less {
			Ok(Self::from(self.real.acosh(int)?))
		} else {
			// acosh(z)=ln(z+sqrt(z^2-1))
			let exact = Exact::new(self, true);
			let half = Exact::new(Self::from(1), true).div(Exact::new(Self::from(2), true), int)?;
			let sqrt = exact
				.clone()
				.mul(&exact, int)?
				.add(Exact::new(Self::from(1), true).neg(), int)?
				.try_and_then(|x| x.frac_pow(half.value, int))?;
			sqrt.add(exact, int)?
				.try_and_then(|x| x.ln(int))
				.map(|x| x.value)
		}
	}

	pub(crate) fn atanh<I: Interrupt>(self, int: &I) -> FResult<Self> {
		// Real atanh is defined for -1 < x < 1
		// Undefined for x = 1, -1
		if self.imag.is_zero() && self.real.between_plus_minus_one_excl(int)? {
			Ok(Self::from(self.real.atanh(int)?))
		} else {
			// atanh(z)=ln(sqrt(-(z-1)/(z-1)))
			let exact = Exact::new(self, true);
			let one = Exact::new(Self::from(1), true);
			let half = Exact::new(Self::from(1), true).div(Exact::new(Self::from(2), true), int)?;

			exact
				.clone()
				.add(one.clone(), int)?
				.neg()
				.div(exact.add(one.neg(), int)?, int)?
				.try_and_then(|x| x.frac_pow(half.value, int))?
				.try_and_then(|z| z.ln(int))
				.map(|x| x.value)
		}
	}

	pub(crate) fn ln<I: Interrupt>(self, int: &I) -> FResult<Exact<Self>> {
		if self.imag.is_zero() && self.real.is_pos() {
			Ok(self.real.ln(int)?.apply(Self::from))
		} else {
			// ln(z) = ln(|z|) + i * arg(z)
			let abs = self.clone().abs(int)?;
			let real = abs.value.ln(int)?.combine(abs.exact);
			let arg = self.arg(int)?;

			Ok(Exact::new(
				Self {
					real: real.value,
					imag: arg.value,
				},
				real.exact && arg.exact,
			))
		}
	}

	pub(crate) fn log<I: Interrupt>(self, base: Self, int: &I) -> FResult<Self> {
		// log_n(z) = ln(z) / ln(n)
		let ln = self.ln(int)?;
		let ln2 = base.ln(int)?;
		Ok(ln.div(ln2, int)?.value)
	}

	pub(crate) fn log2<I: Interrupt>(self, int: &I) -> FResult<Self> {
		if self.imag.is_zero() && self.real.is_pos() {
			Ok(Self::from(self.real.log2(int)?))
		} else {
			self.log(Self::from(2), int)
		}
	}
	pub(crate) fn log10<I: Interrupt>(self, int: &I) -> FResult<Self> {
		if self.imag.is_zero() && self.real.is_pos() {
			Ok(Self::from(self.real.log10(int)?))
		} else {
			self.log(Self::from(10), int)
		}
	}

	pub(crate) fn is_definitely_one(&self) -> bool {
		self.real.is_definitely_one() && self.imag.is_definitely_zero()
	}

	pub(crate) fn modulo<I: Interrupt>(self, rhs: Self, int: &I) -> FResult<Self> {
		Ok(Self::from(
			self.expect_real()?.modulo(rhs.expect_real()?, int)?,
		))
	}

	pub(crate) fn bitwise<I: Interrupt>(
		self,
		rhs: Self,
		op: crate::ast::BitwiseBop,
		int: &I,
	) -> FResult<Self> {
		Ok(Self::from(self.expect_real()?.bitwise(
			rhs.expect_real()?,
			op,
			int,
		)?))
	}

	pub(crate) fn combination<I: Interrupt>(self, rhs: Self, int: &I) -> FResult<Self> {
		Ok(Self::from(
			self.expect_real()?.combination(rhs.expect_real()?, int)?,
		))
	}

	pub(crate) fn permutation<I: Interrupt>(self, rhs: Self, int: &I) -> FResult<Self> {
		Ok(Self::from(
			self.expect_real()?.permutation(rhs.expect_real()?, int)?,
		))
	}
}

impl Exact<Complex> {
	pub(crate) fn add<I: Interrupt>(self, rhs: Self, int: &I) -> FResult<Self> {
		let (self_real, self_imag) = self.apply(|x| (x.real, x.imag)).pair();
		let (rhs_real, rhs_imag) = rhs.apply(|x| (x.real, x.imag)).pair();
		let real = self_real.add(rhs_real, int)?;
		let imag = self_imag.add(rhs_imag, int)?;
		Ok(Self::new(
			Complex {
				real: real.value,
				imag: imag.value,
			},
			real.exact && imag.exact,
		))
	}

	pub(crate) fn mul<I: Interrupt>(self, rhs: &Self, int: &I) -> FResult<Self> {
		// (a + bi) * (c + di)
		//     => ac + bci + adi - bd
		//     => (ac - bd) + (bc + ad)i
		let (self_real, self_imag) = self.apply(|x| (x.real, x.imag)).pair();
		let (rhs_real, rhs_imag) = rhs.clone().apply(|x| (x.real, x.imag)).pair();

		let prod1 = self_real.clone().mul(rhs_real.re(), int)?;
		let prod2 = self_imag.clone().mul(rhs_imag.re(), int)?;
		let real_part = prod1.add(-prod2, int)?;
		let prod3 = self_real.mul(rhs_imag.re(), int)?;
		let prod4 = self_imag.mul(rhs_real.re(), int)?;
		let imag_part = prod3.add(prod4, int)?;
		Ok(Self::new(
			Complex {
				real: real_part.value,
				imag: imag_part.value,
			},
			real_part.exact && imag_part.exact,
		))
	}

	pub(crate) fn div<I: Interrupt>(self, rhs: Self, int: &I) -> FResult<Self> {
		// (u + vi) / (x + yi) = (1/(x^2 + y^2)) * ((ux + vy) + (vx - uy)i)
		let (u, v) = self.apply(|x| (x.real, x.imag)).pair();
		let (x, y) = rhs.apply(|x| (x.real, x.imag)).pair();
		// if both numbers are real, use this simplified algorithm
		if v.exact && v.value.is_zero() && y.exact && y.value.is_zero() {
			return Ok(u.div(&x, int)?.apply(|real| Complex {
				real,
				imag: 0.into(),
			}));
		}
		let prod1 = x.clone().mul(x.re(), int)?;
		let prod2 = y.clone().mul(y.re(), int)?;
		let sum = prod1.add(prod2, int)?;
		let real_part = Exact::new(Real::from(1), true).div(&sum, int)?;
		let prod3 = u.clone().mul(x.re(), int)?;
		let prod4 = v.clone().mul(y.re(), int)?;
		let real2 = prod3.add(prod4, int)?;
		let prod5 = v.mul(x.re(), int)?;
		let prod6 = u.mul(y.re(), int)?;
		let imag2 = prod5.add(-prod6, int)?;
		let multiplicand = Self::new(
			Complex {
				real: real2.value,
				imag: imag2.value,
			},
			real2.exact && imag2.exact,
		);
		let result = Self::new(
			Complex {
				real: real_part.value,
				imag: 0.into(),
			},
			real_part.exact,
		)
		.mul(&multiplicand, int)?;
		Ok(result)
	}
}

impl Neg for Complex {
	type Output = Self;

	fn neg(self) -> Self {
		Self {
			real: -self.real,
			imag: -self.imag,
		}
	}
}

impl Neg for &Complex {
	type Output = Complex;

	fn neg(self) -> Complex {
		-self.clone()
	}
}

impl From<u64> for Complex {
	fn from(i: u64) -> Self {
		Self {
			real: i.into(),
			imag: 0.into(),
		}
	}
}

impl From<Real> for Complex {
	fn from(i: Real) -> Self {
		Self {
			real: i,
			imag: 0.into(),
		}
	}
}

#[derive(Debug)]
pub(crate) struct Formatted {
	first_component: real::Formatted,
	separator: &'static str,
	second_component: Option<real::Formatted>,
	use_parentheses: bool,
}

impl fmt::Display for Formatted {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		if self.use_parentheses {
			write!(f, "(")?;
		}
		write!(f, "{}{}", self.first_component, self.separator)?;
		if let Some(second_component) = &self.second_component {
			write!(f, "{second_component}")?;
		}
		if self.use_parentheses {
			write!(f, ")")?;
		}
		Ok(())
	}
}
