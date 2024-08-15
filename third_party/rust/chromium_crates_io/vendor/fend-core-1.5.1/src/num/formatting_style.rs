use std::{fmt, io};

use crate::{
	error::FendError,
	result::FResult,
	serialize::{Deserialize, Serialize},
};

#[derive(PartialEq, Eq, Clone, Copy, Default)]
#[must_use]
pub(crate) enum FormattingStyle {
	/// Print value as an improper fraction
	ImproperFraction,
	/// Print as a mixed fraction, e.g. 1 1/2
	MixedFraction,
	/// Print as a float, possibly indicating recurring digits
	/// with parentheses, e.g. 7/9 => 0.(81)
	ExactFloat,
	/// Print with the given number of decimal places
	DecimalPlaces(usize),
	/// Print with the given number of significant figures (not including any leading zeroes)
	SignificantFigures(usize),
	/// If exact and no recurring digits: `ExactFloat`, if complex/imag: `MixedFraction`,
	/// otherwise: DecimalPlaces(10)
	#[default]
	Auto,
	/// If not exact: DecimalPlaces(10). If no recurring digits: `ExactFloat`.
	/// Other numbers: `MixedFraction`, albeit possibly including fractions of pi
	Exact,
}

impl fmt::Display for FormattingStyle {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> Result<(), fmt::Error> {
		match self {
			Self::ImproperFraction => write!(f, "fraction"),
			Self::MixedFraction => write!(f, "mixed_fraction"),
			Self::ExactFloat => write!(f, "float"),
			Self::Exact => write!(f, "exact"),
			Self::DecimalPlaces(d) => write!(f, "{d} dp"),
			Self::SignificantFigures(s) => write!(f, "{s} sf"),
			Self::Auto => write!(f, "auto"),
		}
	}
}

impl fmt::Debug for FormattingStyle {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> Result<(), fmt::Error> {
		match self {
			Self::ImproperFraction => write!(f, "improper fraction"),
			Self::MixedFraction => write!(f, "mixed fraction"),
			Self::ExactFloat => write!(f, "exact float"),
			Self::Exact => write!(f, "exact"),
			Self::DecimalPlaces(d) => write!(f, "{d} dp"),
			Self::SignificantFigures(s) => write!(f, "{s} sf"),
			Self::Auto => write!(f, "auto"),
		}
	}
}

impl FormattingStyle {
	pub(crate) fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
		match self {
			Self::ImproperFraction => 1u8.serialize(write)?,
			Self::MixedFraction => 2u8.serialize(write)?,
			Self::ExactFloat => 3u8.serialize(write)?,
			Self::Exact => 4u8.serialize(write)?,
			Self::DecimalPlaces(d) => {
				5u8.serialize(write)?;
				d.serialize(write)?;
			}
			Self::SignificantFigures(s) => {
				6u8.serialize(write)?;
				s.serialize(write)?;
			}
			Self::Auto => 7u8.serialize(write)?,
		}
		Ok(())
	}

	pub(crate) fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		Ok(match u8::deserialize(read)? {
			1 => Self::ImproperFraction,
			2 => Self::MixedFraction,
			3 => Self::ExactFloat,
			4 => Self::Exact,
			5 => Self::DecimalPlaces(usize::deserialize(read)?),
			6 => Self::SignificantFigures(usize::deserialize(read)?),
			7 => Self::Auto,
			_ => return Err(FendError::DeserializationError),
		})
	}
}
