use std::cmp::Ordering;
use std::{collections::HashMap, fmt, io};

use crate::interrupt::test_int;
use crate::num::complex::{self, Complex, UseParentheses};
use crate::num::{Base, Exact, FormattingStyle};
use crate::result::FResult;
use crate::{DecimalSeparatorStyle, Interrupt};

use super::{base_unit::BaseUnit, named_unit::NamedUnit};

#[derive(Clone)]
pub(crate) struct UnitExponent {
	pub(crate) unit: NamedUnit,
	pub(crate) exponent: Complex,
}

impl UnitExponent {
	pub(crate) fn new(unit: NamedUnit, exponent: impl Into<Complex>) -> Self {
		Self {
			unit,
			exponent: exponent.into(),
		}
	}

	pub(crate) fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
		self.unit.serialize(write)?;
		self.exponent.serialize(write)?;
		Ok(())
	}

	pub(crate) fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		Ok(Self {
			unit: NamedUnit::deserialize(read)?,
			exponent: Complex::deserialize(read)?,
		})
	}

	pub(crate) fn is_alias(&self) -> bool {
		self.unit.is_alias()
	}

	pub(crate) fn is_percentage_unit(&self) -> bool {
		let (prefix, name) = self.unit.prefix_and_name(false);
		prefix.is_empty() && ["%", "percent"].contains(&name)
	}

	pub(crate) fn add_to_hashmap<I: Interrupt>(
		&self,
		hashmap: &mut HashMap<BaseUnit, Complex>,
		scale: &mut Complex,
		exact: &mut bool,
		int: &I,
	) -> FResult<()> {
		test_int(int)?;
		let overall_exp = &Exact::new(self.exponent.clone(), true);
		for (base_unit, base_exp) in &self.unit.base_units {
			test_int(int)?;
			let base_exp = Exact::new(base_exp.clone(), true);
			let product = overall_exp.clone().mul(&base_exp, int)?;
			if let Some(exp) = hashmap.get_mut(base_unit) {
				let new_exp = Exact::new(exp.clone(), true).add(product, int)?;
				*exact = *exact && new_exp.exact;
				if new_exp.value.compare(&0.into(), int)? == Some(Ordering::Equal) {
					hashmap.remove(base_unit);
				} else {
					*exp = new_exp.value;
				}
			} else {
				*exact = *exact && product.exact;
				if product.value.compare(&0.into(), int)? != Some(Ordering::Equal) {
					let adj_exp = overall_exp.clone().mul(&base_exp, int)?;
					hashmap.insert(base_unit.clone(), adj_exp.value);
					*exact = *exact && adj_exp.exact;
				}
			}
		}
		let pow_result = self
			.unit
			.scale
			.clone()
			.pow(overall_exp.value.clone(), int)?;
		*scale = Exact::new(scale.clone(), true).mul(&pow_result, int)?.value;
		*exact = *exact && pow_result.exact;
		Ok(())
	}

	pub(crate) fn format<I: Interrupt>(
		&self,
		base: Base,
		format: FormattingStyle,
		plural: bool,
		invert_exp: bool,
		decimal_separator: DecimalSeparatorStyle,
		int: &I,
	) -> FResult<Exact<FormattedExponent<'_>>> {
		let (prefix, name) = self.unit.prefix_and_name(plural);
		let exp = if invert_exp {
			-self.exponent.clone()
		} else {
			self.exponent.clone()
		};
		let (exact, exponent) = if exp.compare(&1.into(), int)? == Some(Ordering::Equal) {
			(true, None)
		} else {
			let formatted = exp.format(
				true,
				format,
				base,
				UseParentheses::IfComplexOrFraction,
				decimal_separator,
				int,
			)?;
			(formatted.exact, Some(formatted.value))
		};
		Ok(Exact::new(
			FormattedExponent {
				prefix,
				name,
				number: exponent,
			},
			exact,
		))
	}
}

impl fmt::Debug for UnitExponent {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(f, "{:?}", self.unit)?;
		if !self.exponent.is_definitely_one() {
			write!(f, "^{:?}", self.exponent)?;
		}
		Ok(())
	}
}

#[derive(Debug)]
pub(crate) struct FormattedExponent<'a> {
	prefix: &'a str,
	name: &'a str,
	number: Option<complex::Formatted>,
}

impl<'a> fmt::Display for FormattedExponent<'a> {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(f, "{}{}", self.prefix, self.name)?;
		if let Some(number) = &self.number {
			write!(f, "^{number}")?;
		}
		Ok(())
	}
}
