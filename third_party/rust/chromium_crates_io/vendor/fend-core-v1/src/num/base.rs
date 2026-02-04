use std::{fmt, io};

use crate::{
	error::FendError,
	result::FResult,
	serialize::{Deserialize, Serialize},
};

#[derive(Copy, Clone, PartialEq, Eq)]
pub(crate) struct Base(BaseEnum);

#[derive(Copy, Clone, PartialEq, Eq)]
enum BaseEnum {
	/// Binary with 0b prefix
	Binary,
	/// Octal with 0o prefix
	Octal,
	/// Hex with 0x prefix
	Hex,
	/// Custom base between 2 and 36 (inclusive), written as base#number
	Custom(u8),
	/// Plain (no prefix)
	Plain(u8),
}

impl Base {
	pub(crate) const HEX: Self = Self(BaseEnum::Hex);

	pub(crate) const fn base_as_u8(self) -> u8 {
		match self.0 {
			BaseEnum::Binary => 2,
			BaseEnum::Octal => 8,
			BaseEnum::Hex => 16,
			BaseEnum::Custom(b) | BaseEnum::Plain(b) => b,
		}
	}

	pub(crate) const fn from_zero_based_prefix_char(ch: char) -> FResult<Self> {
		Ok(match ch {
			'x' => Self(BaseEnum::Hex),
			'o' => Self(BaseEnum::Octal),
			'b' => Self(BaseEnum::Binary),
			_ => return Err(FendError::InvalidBasePrefix),
		})
	}

	pub(crate) const fn from_plain_base(base: u8) -> FResult<Self> {
		if base < 2 {
			return Err(FendError::BaseTooSmall);
		} else if base > 36 {
			return Err(FendError::BaseTooLarge);
		}
		Ok(Self(BaseEnum::Plain(base)))
	}

	pub(crate) const fn from_custom_base(base: u8) -> FResult<Self> {
		if base < 2 {
			return Err(FendError::BaseTooSmall);
		} else if base > 36 {
			return Err(FendError::BaseTooLarge);
		}
		Ok(Self(BaseEnum::Custom(base)))
	}

	pub(crate) fn write_prefix(self, f: &mut fmt::Formatter<'_>) -> Result<(), fmt::Error> {
		match self.0 {
			BaseEnum::Binary => write!(f, "0b")?,
			BaseEnum::Octal => write!(f, "0o")?,
			BaseEnum::Hex => write!(f, "0x")?,
			BaseEnum::Custom(b) => write!(f, "{b}#")?,
			BaseEnum::Plain(_) => (),
		}
		Ok(())
	}

	pub(crate) const fn has_prefix(self) -> bool {
		!matches!(self.0, BaseEnum::Plain(_))
	}

	pub(crate) const fn digit_as_char(digit: u64) -> Option<char> {
		Some(match digit {
			0 => '0',
			1 => '1',
			2 => '2',
			3 => '3',
			4 => '4',
			5 => '5',
			6 => '6',
			7 => '7',
			8 => '8',
			9 => '9',
			10 => 'a',
			11 => 'b',
			12 => 'c',
			13 => 'd',
			14 => 'e',
			15 => 'f',
			16 => 'g',
			17 => 'h',
			18 => 'i',
			19 => 'j',
			20 => 'k',
			21 => 'l',
			22 => 'm',
			23 => 'n',
			24 => 'o',
			25 => 'p',
			26 => 'q',
			27 => 'r',
			28 => 's',
			29 => 't',
			30 => 'u',
			31 => 'v',
			32 => 'w',
			33 => 'x',
			34 => 'y',
			35 => 'z',
			_ => return None,
		})
	}

	pub(crate) fn serialize(self, write: &mut impl io::Write) -> FResult<()> {
		match self.0 {
			BaseEnum::Binary => 1u8.serialize(write)?,
			BaseEnum::Octal => 2u8.serialize(write)?,
			BaseEnum::Hex => 3u8.serialize(write)?,
			BaseEnum::Custom(b) => {
				4u8.serialize(write)?;
				b.serialize(write)?;
			}
			BaseEnum::Plain(b) => {
				5u8.serialize(write)?;
				b.serialize(write)?;
			}
		}
		Ok(())
	}

	pub(crate) fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		Ok(Self(match u8::deserialize(read)? {
			1 => BaseEnum::Binary,
			2 => BaseEnum::Octal,
			3 => BaseEnum::Hex,
			4 => BaseEnum::Custom(u8::deserialize(read)?),
			5 => BaseEnum::Plain(u8::deserialize(read)?),
			_ => return Err(FendError::DeserializationError),
		}))
	}
}

impl Default for Base {
	fn default() -> Self {
		Self(BaseEnum::Plain(10))
	}
}

impl fmt::Debug for Base {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		match self.0 {
			BaseEnum::Binary => write!(f, "binary"),
			BaseEnum::Octal => write!(f, "octal"),
			BaseEnum::Hex => write!(f, "hex"),
			BaseEnum::Custom(b) => write!(f, "base {b} (with prefix)"),
			BaseEnum::Plain(b) => write!(f, "base {b}"),
		}
	}
}
