use crate::ast::{BitwiseBop, Bop};
use crate::error::{FendError, Interrupt};
use crate::num::complex::{Complex, UseParentheses};
use crate::num::dist::Dist;
use crate::num::{Base, FormattingStyle};
use crate::result::FResult;
use crate::scope::Scope;
use crate::serialize::{Deserialize, Serialize};
use crate::units::{lookup_default_unit, query_unit_static};
use crate::{ast, ident::Ident};
use crate::{Attrs, DecimalSeparatorStyle, Span, SpanKind};
use std::borrow::Cow;
use std::cmp::Ordering;
use std::collections::HashMap;
use std::ops::Neg;
use std::sync::Arc;
use std::{cmp, fmt, io};

pub(crate) mod base_unit;
pub(crate) mod named_unit;
pub(crate) mod unit_exponent;

use base_unit::BaseUnit;
use named_unit::NamedUnit;
use unit_exponent::UnitExponent;

use self::named_unit::compare_hashmaps;

use super::bigrat::BigRat;
use super::biguint::BigUint;
use super::real::Real;
use super::Exact;

#[derive(Clone)]
#[allow(clippy::pedantic)]
pub(crate) struct Value {
	#[allow(clippy::struct_field_names)]
	value: Dist,
	unit: Unit,
	exact: bool,
	base: Base,
	format: FormattingStyle,
	simplifiable: bool,
}

impl Value {
	pub(crate) fn compare<I: Interrupt>(
		&self,
		other: &Self,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<Option<cmp::Ordering>> {
		match self.clone().sub(other.clone(), decimal_separator, int) {
			Err(FendError::Interrupted) => Err(FendError::Interrupted),
			Err(_) => Ok(None),
			Ok(result) => {
				if result.is_zero(int)? {
					return Ok(Some(cmp::Ordering::Equal));
				}
				let Ok(c) = result.value.one_point() else {
					return Ok(None);
				};
				c.compare(&0.into(), int)
			}
		}
	}

	pub(crate) fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
		self.value.serialize(write)?;
		self.unit.serialize(write)?;
		self.exact.serialize(write)?;
		self.base.serialize(write)?;
		self.format.serialize(write)?;
		self.simplifiable.serialize(write)?;
		Ok(())
	}

	pub(crate) fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		Ok(Self {
			value: Dist::deserialize(read)?,
			unit: Unit::deserialize(read)?,
			exact: bool::deserialize(read)?,
			base: Base::deserialize(read)?,
			format: FormattingStyle::deserialize(read)?,
			simplifiable: bool::deserialize(read)?,
		})
	}

	pub(crate) fn try_as_usize<I: Interrupt>(
		self,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<usize> {
		self.into_unitless_complex(decimal_separator, int)?
			.try_as_usize(int)
	}

	pub(crate) fn try_as_usize_unit<I: Interrupt>(self, int: &I) -> FResult<usize> {
		if !self.exact {
			return Err(FendError::InexactNumberToInt);
		}
		self.value.one_point()?.try_as_usize(int)
	}

	pub(crate) fn create_unit_value_from_value<I: Interrupt>(
		value: &Self,
		prefix: Cow<'static, str>,
		alias: bool,
		singular_name: Cow<'static, str>,
		plural_name: Cow<'static, str>,
		int: &I,
	) -> FResult<Self> {
		let (hashmap, scale) = value.unit.to_hashmap_and_scale(int)?;
		let scale = scale.mul(&Exact::new(value.value.one_point_ref()?.clone(), true), int)?;
		let resulting_unit = NamedUnit::new(
			prefix,
			singular_name,
			plural_name,
			alias,
			hashmap,
			scale.value,
		);
		let mut result = Self::new(1, vec![UnitExponent::new(resulting_unit, 1)]);
		result.exact = result.exact && value.exact && scale.exact;
		Ok(result)
	}

	pub(crate) fn new_base_unit(
		singular_name: Cow<'static, str>,
		plural_name: Cow<'static, str>,
	) -> Self {
		let base_unit = BaseUnit::new(singular_name.clone());
		let mut hashmap = HashMap::new();
		hashmap.insert(base_unit, 1.into());
		let unit = NamedUnit::new(
			Cow::Borrowed(""),
			singular_name,
			plural_name,
			false,
			hashmap,
			1,
		);
		Self::new(1, vec![UnitExponent::new(unit, 1)])
	}

	pub(crate) fn with_format(self, format: FormattingStyle) -> Self {
		Self {
			value: self.value,
			unit: self.unit,
			exact: self.exact,
			base: self.base,
			simplifiable: self.simplifiable,
			format,
		}
	}

	pub(crate) fn with_base(self, base: Base) -> Self {
		Self {
			value: self.value,
			unit: self.unit,
			exact: self.exact,
			format: self.format,
			simplifiable: self.simplifiable,
			base,
		}
	}

	pub(crate) fn factorial<I: Interrupt>(
		self,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<Self> {
		Ok(Self {
			unit: Unit::unitless(),
			exact: self.exact,
			base: self.base,
			format: self.format,
			simplifiable: self.simplifiable,
			value: Dist::from(
				self.into_unitless_complex(decimal_separator, int)?
					.factorial(int)?,
			),
		})
	}

	fn new(value: impl Into<Dist>, unit_components: Vec<UnitExponent>) -> Self {
		Self {
			value: value.into(),
			unit: Unit {
				components: unit_components,
			},
			exact: true,
			base: Base::default(),
			format: FormattingStyle::default(),
			simplifiable: true,
		}
	}

	pub(crate) fn add<I: Interrupt>(
		self,
		rhs: Self,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<Self> {
		if rhs.is_zero(int)? {
			return Ok(self);
		}
		let scale_factor =
			Unit::compute_scale_factor(&rhs.unit, &self.unit, decimal_separator, int)?;
		let scaled = Exact::new(rhs.value, rhs.exact)
			.mul(&scale_factor.scale_1.apply(Dist::from), int)?
			.div(&scale_factor.scale_2.apply(Dist::from), int)?;
		let value =
			Exact::new(self.value, self.exact).add(&Exact::new(scaled.value, scaled.exact), int)?;
		Ok(Self {
			value: value.value,
			unit: self.unit,
			exact: self.exact && rhs.exact && value.exact,
			base: self.base,
			format: self.format,
			simplifiable: self.simplifiable,
		})
	}

	/// Called for implicit addition to modify the second operand.
	/// For example, when evaluating `5'0`, this function can change the second
	/// operand's unit from `unitless` to `"`.
	fn fudge_implicit_rhs_unit<I: Interrupt>(
		&self,
		rhs: Self,
		attrs: Attrs,
		context: &mut crate::Context,
		int: &I,
	) -> FResult<Self> {
		for (lhs_unit, rhs_unit) in crate::units::IMPLICIT_UNIT_MAP {
			if self.unit.equal_to(lhs_unit, int)? && rhs.is_unitless(int)? {
				let inches =
					ast::resolve_identifier(&Ident::new_str(rhs_unit), None, attrs, context, int)?
						.expect_num()?;
				return rhs.mul(inches, int);
			}
		}
		Ok(rhs)
	}

	pub(crate) fn convert_to<I: Interrupt>(
		self,
		rhs: Self,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<Self> {
		if rhs.value.one_point()?.compare(&1.into(), int)? != Some(Ordering::Equal) {
			return Err(FendError::ConversionRhsNumerical);
		}
		let scale_factor =
			Unit::compute_scale_factor(&self.unit, &rhs.unit, decimal_separator, int)?;
		let new_value = Exact::new(self.value, self.exact)
			.mul(&scale_factor.scale_1.apply(Dist::from), int)?
			.add(&scale_factor.offset.apply(Dist::from), int)?
			.div(&scale_factor.scale_2.apply(Dist::from), int)?;
		Ok(Self {
			value: new_value.value,
			unit: rhs.unit,
			exact: self.exact && rhs.exact && new_value.exact,
			base: self.base,
			format: self.format,
			simplifiable: false,
		})
	}

	pub(crate) fn sub<I: Interrupt>(
		self,
		rhs: Self,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<Self> {
		self.add(-rhs, decimal_separator, int)
	}

	pub(crate) fn div<I: Interrupt>(self, rhs: Self, int: &I) -> FResult<Self> {
		let mut components = self.unit.components.clone();
		for rhs_component in rhs.unit.components {
			components.push(UnitExponent::new(
				rhs_component.unit,
				-rhs_component.exponent,
			));
		}
		let value =
			Exact::new(self.value, self.exact).div(&Exact::new(rhs.value, rhs.exact), int)?;
		Ok(Self {
			value: value.value,
			unit: Unit { components },
			exact: value.exact && self.exact && rhs.exact,
			base: self.base,
			format: self.format,
			simplifiable: self.simplifiable,
		})
	}

	fn modulo<I: Interrupt>(
		self,
		rhs: Self,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<Self> {
		Ok(Self {
			unit: Unit::unitless(),
			exact: self.exact && rhs.exact,
			base: self.base,
			format: self.format,
			simplifiable: self.simplifiable,
			value: Dist::from(
				self.into_unitless_complex(decimal_separator, int)?
					.modulo(rhs.into_unitless_complex(decimal_separator, int)?, int)?,
			),
		})
	}

	fn bitwise<I: Interrupt>(
		self,
		rhs: Self,
		op: BitwiseBop,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<Self> {
		Ok(Self {
			unit: Unit::unitless(),
			exact: self.exact && rhs.exact,
			base: self.base,
			format: self.format,
			simplifiable: self.simplifiable,
			value: Dist::from(
				self.into_unitless_complex(decimal_separator, int)?
					.bitwise(rhs.into_unitless_complex(decimal_separator, int)?, op, int)?,
			),
		})
	}

	pub(crate) fn combination<I: Interrupt>(
		self,
		rhs: Self,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<Self> {
		Ok(Self {
			unit: Unit::unitless(),
			exact: self.exact && rhs.exact,
			base: self.base,
			format: self.format,
			simplifiable: self.simplifiable,
			value: Dist::from(
				self.into_unitless_complex(decimal_separator, int)?
					.combination(rhs.into_unitless_complex(decimal_separator, int)?, int)?,
			),
		})
	}

	pub(crate) fn permutation<I: Interrupt>(
		self,
		rhs: Self,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<Self> {
		Ok(Self {
			unit: Unit::unitless(),
			exact: self.exact && rhs.exact,
			base: self.base,
			format: self.format,
			simplifiable: self.simplifiable,
			value: Dist::from(
				self.into_unitless_complex(decimal_separator, int)?
					.permutation(rhs.into_unitless_complex(decimal_separator, int)?, int)?,
			),
		})
	}

	pub(crate) fn bop<I: Interrupt>(
		self,
		op: Bop,
		rhs: Self,
		attrs: Attrs,
		context: &mut crate::Context,
		int: &I,
	) -> FResult<Self> {
		match op {
			Bop::Plus => self.add(rhs, context.decimal_separator, int),
			Bop::ImplicitPlus => {
				let rhs = self.fudge_implicit_rhs_unit(rhs, attrs, context, int)?;
				self.add(rhs, context.decimal_separator, int)
			}
			Bop::Minus => self.sub(rhs, context.decimal_separator, int),
			Bop::Mul => self.mul(rhs, int),
			Bop::Div => self.div(rhs, int),
			Bop::Mod => self.modulo(rhs, context.decimal_separator, int),
			Bop::Pow => self.pow(rhs, context.decimal_separator, int),
			Bop::Bitwise(bitwise_bop) => {
				self.bitwise(rhs, bitwise_bop, context.decimal_separator, int)
			}
			Bop::Combination => self.combination(rhs, context.decimal_separator, int),
			Bop::Permutation => self.permutation(rhs, context.decimal_separator, int),
		}
	}

	pub(crate) fn is_unitless<I: Interrupt>(&self, int: &I) -> FResult<bool> {
		// todo this is broken for unitless components
		if self.unit.components.is_empty() {
			return Ok(true);
		}
		let (hashmap, _scale) = self.unit.to_hashmap_and_scale(int)?;
		if hashmap.is_empty() {
			return Ok(true);
		}
		Ok(false)
	}

	pub(crate) fn is_unitless_one<I: Interrupt>(&self, int: &I) -> FResult<bool> {
		Ok(self.exact && self.value.equals_int(1, int)? && self.is_unitless(int)?)
	}

	pub(crate) fn pow<I: Interrupt>(
		self,
		rhs: Self,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<Self> {
		let rhs_exact = rhs.exact;
		let rhs = rhs.into_unitless_complex(decimal_separator, int)?;
		let mut new_components = vec![];
		let mut exact_res = true;
		for unit_exp in self.unit.components {
			let exponent = Exact::new(unit_exp.exponent, self.exact)
				.mul(&Exact::new(rhs.clone(), rhs_exact), int)?;
			exact_res = exact_res && exponent.exact;
			new_components.push(UnitExponent {
				unit: unit_exp.unit,
				exponent: exponent.value,
			});
		}
		let new_unit = Unit {
			components: new_components,
		};
		let value = self.value.one_point()?.pow(rhs, int)?;
		Ok(Self {
			value: value.value.into(),
			unit: new_unit,
			exact: self.exact && rhs_exact && exact_res && value.exact,
			base: self.base,
			format: self.format,
			simplifiable: self.simplifiable,
		})
	}

	pub(crate) fn i() -> Self {
		Self {
			value: Complex::i().into(),
			unit: Unit { components: vec![] },
			exact: true,
			base: Base::default(),
			format: FormattingStyle::default(),
			simplifiable: true,
		}
	}

	pub(crate) fn pi() -> Self {
		Self {
			value: Complex::pi().into(),
			unit: Unit { components: vec![] },
			exact: true,
			base: Base::default(),
			format: FormattingStyle::default(),
			simplifiable: true,
		}
	}

	pub(crate) fn abs<I: Interrupt>(self, int: &I) -> FResult<Self> {
		let value = self.value.one_point()?.abs(int)?;
		Ok(Self {
			value: Complex::from(value.value).into(),
			unit: self.unit,
			exact: self.exact && value.exact,
			base: self.base,
			format: self.format,
			simplifiable: self.simplifiable,
		})
	}

	pub(crate) fn make_approximate(self) -> Self {
		Self {
			value: self.value,
			unit: self.unit,
			exact: false,
			base: self.base,
			format: self.format,
			simplifiable: self.simplifiable,
		}
	}

	pub(crate) fn zero_with_base(base: Base) -> Self {
		Self {
			value: Dist::from(0),
			unit: Unit::unitless(),
			exact: true,
			base,
			format: FormattingStyle::default(),
			simplifiable: true,
		}
	}

	pub(crate) fn is_zero<I: Interrupt>(&self, int: &I) -> FResult<bool> {
		self.value.equals_int(0, int)
	}

	pub(crate) fn new_die<I: Interrupt>(count: u32, faces: u32, int: &I) -> FResult<Self> {
		Ok(Self::new(Dist::new_die(count, faces, int)?, vec![]))
	}

	fn remove_unit_scaling<I: Interrupt>(
		self,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<Self> {
		self.convert_to(Self::unitless(), decimal_separator, int)
	}

	pub(crate) fn into_unitless_complex<I: Interrupt>(
		mut self,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<Complex> {
		self = self.remove_unit_scaling(decimal_separator, int)?;
		if !self.is_unitless(int)? {
			return Err(FendError::ExpectedAUnitlessNumber);
		}
		self.value.one_point()
	}

	fn apply_fn_exact<I: Interrupt>(
		mut self,
		f: impl FnOnce(Complex, &I) -> FResult<Exact<Complex>>,
		require_unitless: bool,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<Self> {
		self = self.remove_unit_scaling(decimal_separator, int)?;
		if require_unitless && !self.is_unitless(int)? {
			return Err(FendError::ExpectedAUnitlessNumber);
		}
		let exact = f(self.value.one_point()?, int)?;
		Ok(Self {
			value: exact.value.into(),
			unit: self.unit,
			exact: self.exact && exact.exact,
			base: self.base,
			format: self.format,
			simplifiable: self.simplifiable,
		})
	}

	fn apply_fn<I: Interrupt>(
		mut self,
		f: impl FnOnce(Complex, &I) -> FResult<Complex>,
		require_unitless: bool,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<Self> {
		self = self.remove_unit_scaling(decimal_separator, int)?;
		if require_unitless && !self.is_unitless(int)? {
			return Err(FendError::ExpectedAUnitlessNumber);
		}
		Ok(Self {
			value: f(self.value.one_point()?, int)?.into(),
			unit: self.unit,
			exact: false,
			base: self.base,
			format: self.format,
			simplifiable: self.simplifiable,
		})
	}

	pub(crate) fn sample<I: Interrupt>(self, ctx: &crate::Context, int: &I) -> FResult<Self> {
		Ok(Self {
			value: self.value.sample(ctx, int)?,
			..self
		})
	}

	pub(crate) fn mean<I: Interrupt>(self, int: &I) -> FResult<Self> {
		Ok(Self {
			value: self.value.mean(int)?,
			..self
		})
	}

	fn convert_angle_to_rad<I: Interrupt>(
		self,
		scope: Option<Arc<Scope>>,
		attrs: Attrs,
		context: &mut crate::Context,
		int: &I,
	) -> FResult<Self> {
		let radians =
			ast::resolve_identifier(&Ident::new_str("radians"), scope, attrs, context, int)?
				.expect_num()?;
		self.convert_to(radians, context.decimal_separator, int)
	}

	fn unitless() -> Self {
		Self {
			value: 1.into(),
			unit: Unit::unitless(),
			exact: true,
			base: Base::default(),
			format: FormattingStyle::default(),
			simplifiable: true,
		}
	}

	pub(crate) fn floor<I: Interrupt>(self, int: &I) -> FResult<Self> {
		let value = self.value.one_point()?.floor(int)?;
		Ok(Self {
			value: Complex::from(value.value).into(),
			unit: self.unit,
			exact: self.exact && value.exact,
			base: self.base,
			format: self.format,
			simplifiable: self.simplifiable,
		})
	}

	pub(crate) fn ceil<I: Interrupt>(self, int: &I) -> FResult<Self> {
		let value = self.value.one_point()?.ceil(int)?;
		Ok(Self {
			value: Complex::from(value.value).into(),
			unit: self.unit,
			exact: self.exact && value.exact,
			base: self.base,
			format: self.format,
			simplifiable: self.simplifiable,
		})
	}

	pub(crate) fn round<I: Interrupt>(self, int: &I) -> FResult<Self> {
		let value = self.value.one_point()?.round(int)?;
		Ok(Self {
			value: Complex::from(value.value).into(),
			unit: self.unit,
			exact: self.exact && value.exact,
			base: self.base,
			format: self.format,
			simplifiable: self.simplifiable,
		})
	}

	pub(crate) fn fibonacci<I: Interrupt>(
		self,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<Self> {
		Ok(Self {
			unit: Unit::unitless(),
			exact: self.exact,
			base: self.base,
			format: self.format,
			simplifiable: self.simplifiable,
			value: Complex::from(Real::from(BigRat::from(BigUint::fibonacci(
				self.try_as_usize(decimal_separator, int)?,
				int,
			)?)))
			.into(),
		})
	}

	pub(crate) fn real(self) -> FResult<Self> {
		Ok(Self {
			value: Complex::from(self.value.one_point()?.real()).into(),
			..self
		})
	}

	pub(crate) fn imag(self) -> FResult<Self> {
		Ok(Self {
			value: Complex::from(self.value.one_point()?.imag()).into(),
			..self
		})
	}

	pub(crate) fn arg<I: Interrupt>(
		self,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<Self> {
		self.apply_fn_exact(
			|c, int| c.arg(int).map(|c| c.apply(Complex::from)),
			false,
			decimal_separator,
			int,
		)
	}

	pub(crate) fn conjugate(self) -> FResult<Self> {
		Ok(Self {
			value: self.value.one_point()?.conjugate().into(),
			..self
		})
	}

	pub(crate) fn sin<I: Interrupt>(
		self,
		scope: Option<Arc<Scope>>,
		attrs: Attrs,
		context: &mut crate::Context,
		int: &I,
	) -> FResult<Self> {
		if let Ok(rad) = self
			.clone()
			.convert_angle_to_rad(scope, attrs, context, int)
		{
			Ok(rad
				.apply_fn_exact(Complex::sin, false, context.decimal_separator, int)?
				.convert_to(Self::unitless(), context.decimal_separator, int)?)
		} else {
			self.apply_fn_exact(Complex::sin, false, context.decimal_separator, int)
		}
	}

	pub(crate) fn cos<I: Interrupt>(
		self,
		scope: Option<Arc<Scope>>,
		attrs: Attrs,
		context: &mut crate::Context,
		int: &I,
	) -> FResult<Self> {
		if let Ok(rad) = self
			.clone()
			.convert_angle_to_rad(scope, attrs, context, int)
		{
			rad.apply_fn_exact(Complex::cos, false, context.decimal_separator, int)?
				.convert_to(Self::unitless(), context.decimal_separator, int)
		} else {
			self.apply_fn_exact(Complex::cos, false, context.decimal_separator, int)
		}
	}

	pub(crate) fn tan<I: Interrupt>(
		self,
		scope: Option<Arc<Scope>>,
		attrs: Attrs,
		context: &mut crate::Context,
		int: &I,
	) -> FResult<Self> {
		if let Ok(rad) = self
			.clone()
			.convert_angle_to_rad(scope, attrs, context, int)
		{
			rad.apply_fn_exact(Complex::tan, false, context.decimal_separator, int)?
				.convert_to(Self::unitless(), context.decimal_separator, int)
		} else {
			self.apply_fn_exact(Complex::tan, false, context.decimal_separator, int)
		}
	}

	pub(crate) fn asin<I: Interrupt>(self, context: &mut crate::Context, int: &I) -> FResult<Self> {
		self.apply_fn(Complex::asin, false, context.decimal_separator, int)
	}

	pub(crate) fn acos<I: Interrupt>(self, context: &mut crate::Context, int: &I) -> FResult<Self> {
		self.apply_fn(Complex::acos, false, context.decimal_separator, int)
	}

	pub(crate) fn atan<I: Interrupt>(self, context: &mut crate::Context, int: &I) -> FResult<Self> {
		self.apply_fn(Complex::atan, false, context.decimal_separator, int)
	}

	pub(crate) fn sinh<I: Interrupt>(self, context: &mut crate::Context, int: &I) -> FResult<Self> {
		self.apply_fn(Complex::sinh, false, context.decimal_separator, int)
	}

	pub(crate) fn cosh<I: Interrupt>(self, context: &mut crate::Context, int: &I) -> FResult<Self> {
		self.apply_fn(Complex::cosh, false, context.decimal_separator, int)
	}

	pub(crate) fn tanh<I: Interrupt>(self, context: &mut crate::Context, int: &I) -> FResult<Self> {
		self.apply_fn(Complex::tanh, false, context.decimal_separator, int)
	}

	pub(crate) fn asinh<I: Interrupt>(
		self,
		context: &mut crate::Context,
		int: &I,
	) -> FResult<Self> {
		self.apply_fn(Complex::asinh, false, context.decimal_separator, int)
	}

	pub(crate) fn acosh<I: Interrupt>(
		self,
		context: &mut crate::Context,
		int: &I,
	) -> FResult<Self> {
		self.apply_fn(Complex::acosh, false, context.decimal_separator, int)
	}

	pub(crate) fn atanh<I: Interrupt>(
		self,
		context: &mut crate::Context,
		int: &I,
	) -> FResult<Self> {
		self.apply_fn(Complex::atanh, false, context.decimal_separator, int)
	}

	pub(crate) fn ln<I: Interrupt>(self, context: &mut crate::Context, int: &I) -> FResult<Self> {
		self.apply_fn_exact(Complex::ln, true, context.decimal_separator, int)
	}

	pub(crate) fn log2<I: Interrupt>(self, context: &mut crate::Context, int: &I) -> FResult<Self> {
		self.apply_fn(Complex::log2, true, context.decimal_separator, int)
	}

	pub(crate) fn log10<I: Interrupt>(
		self,
		context: &mut crate::Context,
		int: &I,
	) -> FResult<Self> {
		self.apply_fn(Complex::log10, true, context.decimal_separator, int)
	}

	pub(crate) fn format<I: Interrupt>(
		&self,
		ctx: &crate::Context,
		int: &I,
	) -> FResult<FormattedValue> {
		let use_parentheses = if self.unit.components.is_empty() {
			UseParentheses::No
		} else {
			UseParentheses::IfComplex
		};
		let mut formatted_value = String::new();
		let mut exact = self
			.value
			.format(
				self.exact,
				self.format,
				self.base,
				use_parentheses,
				&mut formatted_value,
				ctx,
				int,
			)?
			.exact;
		let unit_string = self.unit.format(
			"",
			self.value.equals_int(1, int)?,
			self.base,
			self.format,
			true,
			ctx.decimal_separator,
			int,
		)?;
		exact = exact && unit_string.exact;
		Ok(FormattedValue {
			number: formatted_value,
			exact,
			unit_str: unit_string.value,
		})
	}

	pub(crate) fn mul<I: Interrupt>(self, rhs: Self, int: &I) -> FResult<Self> {
		let components = [self.unit.components, rhs.unit.components].concat();
		let value =
			Exact::new(self.value, self.exact).mul(&Exact::new(rhs.value, rhs.exact), int)?;
		Ok(Self {
			value: value.value,
			unit: Unit { components },
			exact: self.exact && rhs.exact && value.exact,
			base: self.base,
			format: self.format,
			simplifiable: self.simplifiable,
		})
	}

	#[allow(clippy::too_many_lines)]
	pub(crate) fn simplify<I: Interrupt>(
		self,
		attrs: Attrs,
		ctx: &mut crate::Context,
		int: &I,
	) -> FResult<Self> {
		if !self.simplifiable {
			return Ok(self);
		}

		let mut res_components: Vec<UnitExponent> = vec![];
		let mut res_exact = self.exact;
		let mut res_value = self.value;

		// remove alias units and combine identical or compatible units
		// by summing their exponents and potentially adjusting the value
		// percentages should be merged to be handled below
		'outer: for comp in self.unit.components {
			if comp.is_alias() && !comp.is_percentage_unit() {
				// remove alias units
				let adjusted_res = Exact {
					value: res_value,
					exact: res_exact,
				}
				.mul(
					&comp.unit.scale.pow(comp.exponent, int)?.apply(Dist::from),
					int,
				)?;
				res_value = adjusted_res.value;
				res_exact = adjusted_res.exact;
				continue;
			}
			for res_comp in &mut res_components {
				if comp.unit.has_no_base_units()
					&& !(comp.is_percentage_unit() && res_comp.is_percentage_unit())
					&& !comp.unit.compare(&res_comp.unit, int)?
				{
					continue;
				}
				let conversion = Unit::compute_scale_factor(
					&Unit {
						components: vec![UnitExponent {
							unit: comp.unit.clone(),
							exponent: 1.into(),
						}],
					},
					&Unit {
						components: vec![UnitExponent {
							unit: res_comp.unit.clone(),
							exponent: 1.into(),
						}],
					},
					ctx.decimal_separator,
					int,
				);
				match conversion {
					Ok(scale_factor) => {
						if scale_factor.offset.value.compare(&0.into(), int)?
							!= Some(Ordering::Equal)
						{
							// don't merge units that have offsets
							break;
						}
						let scale = scale_factor.scale_1.div(scale_factor.scale_2, int)?;

						let lhs = Exact {
							value: res_comp.exponent.clone(),
							exact: res_exact,
						};
						let rhs = Exact {
							value: comp.exponent.clone(),
							exact: res_exact,
						};
						let sum = lhs.add(rhs, int)?;
						res_comp.exponent = sum.value;
						res_exact = res_exact && sum.exact && scale.exact;

						let scale = scale.value.pow(comp.exponent, int)?;
						let adjusted_value = Exact {
							value: res_value.one_point()?,
							exact: res_exact,
						}
						.mul(&scale, int)?;
						res_value = Dist::from(adjusted_value.value);
						res_exact = res_exact && adjusted_value.exact;

						continue 'outer;
					}
					Err(FendError::Interrupted) => return Err(FendError::Interrupted),
					Err(_) => (),
				};
			}
			res_components.push(comp.clone());
		}

		// remove units with exponent == 0
		{
			let mut res_components2 = Vec::with_capacity(res_components.len());
			for c in res_components {
				if c.exponent.compare(&0.into(), int)? != Some(Ordering::Equal) {
					res_components2.push(c);
				}
			}
			res_components = res_components2;
		}

		// percentages are now merged into a single component,
		// and all other units have been simplified
		if let Some(percents_i) = res_components
			.iter()
			.position(unit_exponent::UnitExponent::is_percentage_unit)
		{
			// adjust the exponent in the percentages to either
			// 1 (keeping it), if its exponent is a positive integer
			// and there are no other units (like `80kg * 5%`), or
			// 0 (removing it) otherwise
			let scale = match res_components[..] {
				[UnitExponent {
					ref unit,
					ref mut exponent,
				}] if exponent.imag().is_zero()
					&& matches!(exponent.real().try_as_usize(int), Ok(1..)) =>
				{
					let new_exponent = Complex::from(1);
					let old_exponent = std::mem::replace(exponent, new_exponent.clone());
					let scale_exponent = Exact::new(old_exponent, true)
						.add(Exact::new(-new_exponent, true), int)?
						.value;

					unit.scale.clone().pow(scale_exponent, int)?
				}
				_ => {
					let UnitExponent { unit, exponent } = res_components.remove(percents_i);

					unit.scale.pow(exponent, int)?
				}
			};

			Exact {
				value: res_value,
				exact: res_exact,
			} = Exact::new(res_value, res_exact).mul(&scale.apply(Dist::from), int)?;
		}

		let result = Self {
			value: res_value,
			unit: Unit {
				components: res_components,
			},
			exact: res_exact,
			base: self.base,
			format: self.format,
			simplifiable: self.simplifiable,
		};

		if result.unit.components.len() > 1
			&& !result
				.unit
				.components
				.iter()
				.any(|c| c.unit.singular_name == "rad" || c.unit.singular_name == "radian")
		{
			// try and replace unit with a default one, e.g. `kilogram` or `ampere`
			let (hashmap, _) = result.unit.to_hashmap_and_scale(int)?;
			if let Ok(mut base_units) = hashmap
				.into_iter()
				.map(|(k, v)| v.try_as_i64(int).map(|v| format!("{}^{v}", k.name())))
				.collect::<Result<Vec<String>, _>>()
			{
				base_units.sort();
				if let Some(new_unit) = lookup_default_unit(&base_units.join(" ")) {
					let rhs = query_unit_static(new_unit, attrs, ctx, int)?.expect_num()?;
					return result.convert_to(rhs, ctx.decimal_separator, int);
				}
			}
		}

		Ok(result)
	}

	pub(crate) fn unit_equal_to<I: Interrupt>(&self, rhs: &str, int: &I) -> FResult<bool> {
		self.unit.equal_to(rhs, int)
	}
}

impl Neg for Value {
	type Output = Self;
	fn neg(self) -> Self {
		Self {
			value: -self.value,
			unit: self.unit,
			exact: self.exact,
			base: self.base,
			format: self.format,
			simplifiable: self.simplifiable,
		}
	}
}

impl From<u64> for Value {
	fn from(i: u64) -> Self {
		Self {
			value: i.into(),
			unit: Unit::unitless(),
			exact: true,
			base: Base::default(),
			format: FormattingStyle::default(),
			simplifiable: true,
		}
	}
}

impl fmt::Debug for Value {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		if !self.exact {
			write!(f, "approx. ")?;
		}
		let simplifiable = if self.simplifiable { "" } else { "not " };
		write!(
			f,
			"{:?} {:?} ({:?}, {:?}, {simplifiable}simplifiable)",
			self.value, self.unit, self.base, self.format
		)?;
		Ok(())
	}
}

#[derive(Debug)]
pub(crate) struct FormattedValue {
	exact: bool,
	number: String,
	unit_str: String,
}

impl FormattedValue {
	pub(crate) fn spans(self, spans: &mut Vec<Span>, attrs: Attrs) {
		if !self.exact && attrs.show_approx && !attrs.plain_number {
			spans.push(Span {
				string: "approx. ".to_string(),
				kind: SpanKind::Ident,
			});
		}
		if ["$", "\u{a3}", "\u{a5}"].contains(&self.unit_str.as_str()) && !attrs.plain_number {
			spans.push(Span {
				string: self.unit_str,
				kind: SpanKind::Ident,
			});
			spans.push(Span {
				string: self.number,
				kind: SpanKind::Number,
			});
			return;
		}
		spans.push(Span {
			string: self.number.to_string(),
			kind: SpanKind::Number,
		});
		if !attrs.plain_number {
			spans.push(Span {
				string: self.unit_str,
				kind: SpanKind::Ident,
			});
		}
	}
}

impl fmt::Display for FormattedValue {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		if !self.exact {
			write!(f, "approx. ")?;
		}
		write!(f, "{}{}", self.number, self.unit_str)?;
		Ok(())
	}
}

// TODO: equality comparisons should not depend on order
#[derive(Clone)]
struct Unit {
	components: Vec<UnitExponent>,
}

type HashmapScale = (HashMap<BaseUnit, Complex>, Exact<Complex>);
type HashmapScaleOffset = (HashMap<BaseUnit, Complex>, Exact<Complex>, Exact<Complex>);

struct ScaleFactor {
	scale_1: Exact<Complex>,
	offset: Exact<Complex>,
	scale_2: Exact<Complex>,
}

impl Unit {
	pub(crate) fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
		self.components.len().serialize(write)?;
		for c in &self.components {
			c.serialize(write)?;
		}
		Ok(())
	}

	pub(crate) fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		let len = usize::deserialize(read)?;
		let mut cs = Vec::with_capacity(len);
		for _ in 0..len {
			cs.push(UnitExponent::deserialize(read)?);
		}
		Ok(Self { components: cs })
	}

	pub(crate) fn equal_to<I: Interrupt>(&self, rhs: &str, int: &I) -> FResult<bool> {
		if self.components.len() != 1 {
			return Ok(false);
		}
		let unit = &self.components[0];
		if unit.exponent.compare(&1.into(), int)? != Some(Ordering::Equal) {
			return Ok(false);
		}
		let (prefix, name) = unit.unit.prefix_and_name(false);
		Ok(prefix.is_empty() && name == rhs)
	}

	/// base units with cancelled exponents do not appear in the hashmap
	fn to_hashmap_and_scale<I: Interrupt>(&self, int: &I) -> FResult<HashmapScale> {
		let mut hashmap = HashMap::<BaseUnit, Complex>::new();
		let mut scale = Complex::from(1);
		let mut exact = true;
		for named_unit_exp in &self.components {
			named_unit_exp.add_to_hashmap(&mut hashmap, &mut scale, &mut exact, int)?;
		}
		Ok((hashmap, Exact::new(scale, exact)))
	}

	fn reduce_hashmap<I: Interrupt>(
		hashmap: HashMap<BaseUnit, Complex>,
		int: &I,
	) -> FResult<HashmapScaleOffset> {
		let check = |s: &'static str| -> FResult<bool> {
			Ok(hashmap.len() == 1
				&& match hashmap.get(&BaseUnit::new(Cow::Borrowed(s))) {
					None => false,
					Some(c) => c.compare(&1.into(), int)? == Some(Ordering::Equal),
				})
		};
		if check("celsius")? {
			let mut result_hashmap = HashMap::new();
			result_hashmap.insert(BaseUnit::new(Cow::Borrowed("kelvin")), 1.into());
			return Ok((
				result_hashmap,
				Exact::new(1.into(), true),
				Exact::new(Complex::from(27315), true)
					.div(Exact::new(Complex::from(100), true), int)?,
			));
		}
		if check("fahrenheit")? {
			let mut result_hashmap = HashMap::new();
			result_hashmap.insert(BaseUnit::new(Cow::Borrowed("kelvin")), 1.into());
			return Ok((
				result_hashmap,
				Exact::new(Complex::from(5), true).div(Exact::new(Complex::from(9), true), int)?,
				Exact::new(Complex::from(45967), true)
					.div(Exact::new(Complex::from(180), true), int)?,
			));
		}
		let mut scale_adjustment = Exact::new(Complex::from(1), true);
		let mut result_hashmap = HashMap::new();
		for (mut base_unit, exponent) in hashmap {
			if base_unit.name() == "celsius" {
				base_unit = BaseUnit::new_static("kelvin");
			} else if base_unit.name() == "fahrenheit" {
				base_unit = BaseUnit::new_static("kelvin");
				scale_adjustment = scale_adjustment.mul(
					&Exact::new(Complex::from(5), true)
						.div(Exact::new(Complex::from(9), true), int)?
						.value
						.pow(exponent.clone(), int)?,
					int,
				)?;
			}
			result_hashmap.insert(base_unit.clone(), exponent.clone());
		}
		Ok((result_hashmap, scale_adjustment, Exact::new(0.into(), true)))
	}

	fn print_base_units<I: Interrupt>(
		hash: HashMap<BaseUnit, Complex>,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<String> {
		let from_base_units: Vec<_> = hash
			.into_iter()
			.map(|(base_unit, exponent)| {
				UnitExponent::new(NamedUnit::new_from_base(base_unit), exponent)
			})
			.collect();
		Ok(Self {
			components: from_base_units,
		}
		.format(
			"unitless",
			false,
			Base::default(),
			FormattingStyle::Auto,
			false,
			decimal_separator,
			int,
		)?
		.value)
	}

	/// Returns the combined scale factor if successful
	fn compute_scale_factor<I: Interrupt>(
		from: &Self,
		into: &Self,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<ScaleFactor> {
		let (hash_a, scale_a) = from.to_hashmap_and_scale(int)?;
		let (hash_b, scale_b) = into.to_hashmap_and_scale(int)?;
		let (hash_a, adj_a, offset_a) = Self::reduce_hashmap(hash_a, int)?;
		let (hash_b, adj_b, offset_b) = Self::reduce_hashmap(hash_b, int)?;
		if compare_hashmaps(&hash_a, &hash_b, int)? {
			Ok(ScaleFactor {
				scale_1: scale_a.mul(&adj_a, int)?,
				offset: offset_a.add(-offset_b, int)?,
				scale_2: scale_b.mul(&adj_b, int)?,
			})
		} else {
			let from_formatted = from
				.format(
					"unitless",
					false,
					Base::default(),
					FormattingStyle::Auto,
					false,
					decimal_separator,
					int,
				)?
				.value;
			let into_formatted = into
				.format(
					"unitless",
					false,
					Base::default(),
					FormattingStyle::Auto,
					false,
					decimal_separator,
					int,
				)?
				.value;
			Err(FendError::IncompatibleConversion {
				from: from_formatted,
				to: into_formatted,
				from_base: Self::print_base_units(hash_a, decimal_separator, int)?,
				to_base: Self::print_base_units(hash_b, decimal_separator, int)?,
			})
		}
	}

	const fn unitless() -> Self {
		Self { components: vec![] }
	}

	#[allow(clippy::too_many_arguments)]
	fn format<I: Interrupt>(
		&self,
		unitless: &str,
		value_is_one: bool,
		base: Base,
		format: FormattingStyle,
		consider_printing_space: bool,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<Exact<String>> {
		let mut unit_string = String::new();
		if self.components.is_empty() {
			unit_string.push_str(unitless);
			return Ok(Exact::new(unit_string, true));
		}
		// Pluralisation:
		// All units should be singular, except for the last unit
		// that has a positive exponent, iff the number is not equal to 1
		let mut exact = true;
		let mut positive_components = vec![];
		let mut negative_components = vec![];
		let mut first = true;
		for unit_exponent in &self.components {
			if unit_exponent.exponent.compare(&0.into(), int)? == Some(Ordering::Less) {
				negative_components.push(unit_exponent);
			} else {
				positive_components.push(unit_exponent);
			}
		}
		let invert_negative_component =
			!positive_components.is_empty() && negative_components.len() == 1;
		let mut merged_components = vec![];
		let pluralised_idx = if positive_components.is_empty() {
			usize::MAX
		} else {
			positive_components.len() - 1
		};
		for pos_comp in positive_components {
			merged_components.push((pos_comp, false));
		}
		for neg_comp in negative_components {
			merged_components.push((neg_comp, invert_negative_component));
		}
		let last_component_plural = !value_is_one;
		for (i, (unit_exponent, invert)) in merged_components.into_iter().enumerate() {
			if !first || (consider_printing_space && unit_exponent.unit.print_with_space()) {
				unit_string.push(' ');
			}
			first = false;
			if invert {
				unit_string.push('/');
				unit_string.push(' ');
			}
			let plural = last_component_plural && i == pluralised_idx;
			let exp_format = if format == FormattingStyle::Auto {
				FormattingStyle::Exact
			} else {
				format
			};
			let formatted_exp =
				unit_exponent.format(base, exp_format, plural, invert, decimal_separator, int)?;
			unit_string.push_str(formatted_exp.value.to_string().as_str());
			exact = exact && formatted_exp.exact;
		}
		Ok(Exact::new(unit_string, true))
	}
}

impl fmt::Debug for Unit {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		if self.components.is_empty() {
			write!(f, "(unitless)")?;
		}
		let mut first = true;
		for component in &self.components {
			if !first {
				write!(f, " * ")?;
			}
			write!(f, "{component:?}")?;
			first = false;
		}
		Ok(())
	}
}

#[cfg(test)]
mod tests {
	use super::*;
	use crate::interrupt::Never;

	fn to_string(n: &Value) -> String {
		let int = &crate::interrupt::Never;
		n.format(&crate::Context::new(), int).unwrap().to_string()
	}

	#[test]
	fn test_basic_kg() {
		let base_kg = BaseUnit::new("kilogram".into());
		let mut hashmap = HashMap::new();
		hashmap.insert(base_kg, 1.into());
		let kg = NamedUnit::new("k".into(), "g".into(), "g".into(), false, hashmap, 1);
		let one_kg = Value::new(1, vec![UnitExponent::new(kg.clone(), 1)]);
		let two_kg = Value::new(2, vec![UnitExponent::new(kg, 1)]);
		let sum = one_kg
			.add(two_kg, DecimalSeparatorStyle::Dot, &Never)
			.unwrap();
		assert_eq!(to_string(&sum), "3 kg");
	}

	#[test]
	fn test_basic_kg_and_g() {
		let int = &Never;
		let base_kg = BaseUnit::new("kilogram".into());
		let mut hashmap = HashMap::new();
		hashmap.insert(base_kg, 1.into());
		let kg = NamedUnit::new(
			"k".into(),
			"g".into(),
			"g".into(),
			false,
			hashmap.clone(),
			1,
		);
		let g = NamedUnit::new(
			Cow::Borrowed(""),
			Cow::Borrowed("g"),
			Cow::Borrowed("g"),
			false,
			hashmap,
			Exact::new(Complex::from(1), true)
				.div(Exact::new(1000.into(), true), int)
				.unwrap()
				.value,
		);
		let one_kg = Value::new(1, vec![UnitExponent::new(kg, 1)]);
		let twelve_g = Value::new(12, vec![UnitExponent::new(g, 1)]);
		assert_eq!(
			to_string(
				&one_kg
					.clone()
					.add(twelve_g.clone(), DecimalSeparatorStyle::Dot, int)
					.unwrap()
			),
			"1.012 kg"
		);
		assert_eq!(
			to_string(
				&twelve_g
					.add(one_kg, DecimalSeparatorStyle::Comma, int)
					.unwrap()
			),
			"1012 g"
		);
	}
}
