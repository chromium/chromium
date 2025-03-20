use crate::{
	error::FendError,
	result::FResult,
	serialize::{Deserialize, Serialize},
};
use std::{fmt, io};

#[derive(Clone, Copy, PartialEq, Eq)]
pub(crate) enum DayOfWeek {
	Sunday,
	Monday,
	Tuesday,
	Wednesday,
	Thursday,
	Friday,
	Saturday,
}

impl fmt::Debug for DayOfWeek {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		let s = match self {
			Self::Sunday => "Sunday",
			Self::Monday => "Monday",
			Self::Tuesday => "Tuesday",
			Self::Wednesday => "Wednesday",
			Self::Thursday => "Thursday",
			Self::Friday => "Friday",
			Self::Saturday => "Saturday",
		};
		write!(f, "{s}")
	}
}

impl DayOfWeek {
	pub(crate) fn as_u8(self) -> u8 {
		match self {
			Self::Sunday => 0,
			Self::Monday => 1,
			Self::Tuesday => 2,
			Self::Wednesday => 3,
			Self::Thursday => 4,
			Self::Friday => 5,
			Self::Saturday => 6,
		}
	}

	pub(crate) fn serialize(self, write: &mut impl io::Write) -> FResult<()> {
		self.as_u8().serialize(write)?;
		Ok(())
	}

	pub(crate) fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		Ok(match u8::deserialize(read)? {
			0 => Self::Sunday,
			1 => Self::Monday,
			2 => Self::Tuesday,
			3 => Self::Wednesday,
			4 => Self::Thursday,
			5 => Self::Friday,
			6 => Self::Saturday,
			_ => return Err(FendError::DeserializationError),
		})
	}
}

impl fmt::Display for DayOfWeek {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(f, "{self:?}")
	}
}
