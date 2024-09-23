use std::cmp::Ordering;
use std::{borrow::Cow, collections::HashMap, fmt, io};

use super::base_unit::BaseUnit;
use crate::num::complex::Complex;
use crate::result::FResult;
use crate::serialize::{Deserialize, Serialize};
use crate::Interrupt;

/// A named unit, like kilogram, megabyte or percent.
#[derive(Clone)]
pub(crate) struct NamedUnit {
	prefix: Cow<'static, str>,
	pub(super) singular_name: Cow<'static, str>,
	plural_name: Cow<'static, str>,
	alias: bool,
	pub(super) base_units: HashMap<BaseUnit, Complex>,
	pub(super) scale: Complex,
}

pub(crate) fn compare_hashmaps<I: Interrupt>(
	a: &HashMap<BaseUnit, Complex>,
	b: &HashMap<BaseUnit, Complex>,
	int: &I,
) -> FResult<bool> {
	if a.len() != b.len() {
		return Ok(false);
	}
	for (k, v) in a {
		match b.get(k) {
			None => return Ok(false),
			Some(o) => {
				if v.compare(o, int)? != Some(Ordering::Equal) {
					return Ok(false);
				}
			}
		}
	}
	Ok(true)
}

impl NamedUnit {
	pub(crate) fn new(
		prefix: Cow<'static, str>,
		singular_name: Cow<'static, str>,
		plural_name: Cow<'static, str>,
		alias: bool,
		base_units: HashMap<BaseUnit, Complex>,
		scale: impl Into<Complex>,
	) -> Self {
		Self {
			prefix,
			singular_name,
			plural_name,
			alias,
			base_units,
			scale: scale.into(),
		}
	}

	pub(crate) fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
		self.prefix.as_ref().serialize(write)?;
		self.singular_name.as_ref().serialize(write)?;
		self.plural_name.as_ref().serialize(write)?;
		self.alias.serialize(write)?;

		self.base_units.len().serialize(write)?;
		for (a, b) in &self.base_units {
			a.serialize(write)?;
			b.serialize(write)?;
		}

		self.scale.serialize(write)?;
		Ok(())
	}

	pub(crate) fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		let prefix = String::deserialize(read)?;
		let singular_name = String::deserialize(read)?;
		let plural_name = String::deserialize(read)?;
		let alias = bool::deserialize(read)?;

		let len = usize::deserialize(read)?;
		let mut hashmap = HashMap::with_capacity(len);
		for _ in 0..len {
			let k = BaseUnit::deserialize(read)?;
			let v = Complex::deserialize(read)?;
			hashmap.insert(k, v);
		}
		Ok(Self {
			prefix: Cow::Owned(prefix),
			singular_name: Cow::Owned(singular_name),
			plural_name: Cow::Owned(plural_name),
			alias,
			base_units: hashmap,
			scale: Complex::deserialize(read)?,
		})
	}

	pub(crate) fn compare<I: Interrupt>(&self, other: &Self, int: &I) -> FResult<bool> {
		Ok(self.prefix == other.prefix
			&& self.singular_name == other.singular_name
			&& self.plural_name == other.plural_name
			&& self.alias == other.alias
			&& self.scale.compare(&other.scale, int)? == Some(Ordering::Equal)
			&& compare_hashmaps(&self.base_units, &other.base_units, int)?)
	}

	pub(crate) fn new_from_base(base_unit: BaseUnit) -> Self {
		Self {
			prefix: "".into(),
			singular_name: base_unit.name().to_string().into(),
			plural_name: base_unit.name().to_string().into(),
			alias: false,
			base_units: {
				let mut base_units = HashMap::new();
				base_units.insert(base_unit, 1.into());
				base_units
			},
			scale: 1.into(),
		}
	}

	pub(crate) fn prefix_and_name(&self, plural: bool) -> (&str, &str) {
		(
			self.prefix.as_ref(),
			if plural {
				self.plural_name.as_ref()
			} else {
				self.singular_name.as_ref()
			},
		)
	}

	pub(crate) fn has_no_base_units(&self) -> bool {
		self.base_units.is_empty()
	}

	pub(crate) fn is_alias(&self) -> bool {
		self.alias && self.has_no_base_units()
	}

	/// Returns whether or not this unit should be printed with a
	/// space (between the number and the unit). This should be true for most
	/// units like kg or m, but not for % or °
	pub(crate) fn print_with_space(&self) -> bool {
		// Alphabetic names like kg or m should have a space,
		// while non-alphabetic names like % or ' shouldn't.
		// Empty names shouldn't really exist, but they might as well have a space.

		// degree symbol
		if self.singular_name == "\u{b0}" {
			return false;
		}

		// if it starts with a quote and is more than one character long, print it with a space
		if (self.singular_name.starts_with('\'') || self.singular_name.starts_with('\"'))
			&& self.singular_name.len() > 1
		{
			return true;
		}

		self.singular_name
			.chars()
			.next()
			.map_or(true, |first_char| {
				char::is_alphabetic(first_char) || first_char == '\u{b0}'
			})
	}
}

impl fmt::Debug for NamedUnit {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		if self.prefix.is_empty() {
			write!(f, "{}", self.singular_name)?;
		} else {
			write!(f, "{}-{}", self.prefix, self.singular_name)?;
		}
		write!(f, " (")?;
		if self.plural_name != self.singular_name {
			if self.prefix.is_empty() {
				write!(f, "{}, ", self.plural_name)?;
			} else {
				write!(f, "{}-{}, ", self.prefix, self.plural_name)?;
			}
		}
		write!(f, "= {:?}", self.scale)?;
		let mut it = self.base_units.iter().collect::<Vec<_>>();
		it.sort_by_key(|(k, _v)| k.name());
		for (base_unit, exponent) in &it {
			write!(f, " {base_unit:?}")?;
			if !exponent.is_definitely_one() {
				write!(f, "^{exponent:?}")?;
			}
		}
		write!(f, ")")?;
		Ok(())
	}
}
