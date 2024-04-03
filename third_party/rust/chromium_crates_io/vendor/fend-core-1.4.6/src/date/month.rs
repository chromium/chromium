use crate::result::FResult;
use crate::FendError;
use crate::{
	date::Year,
	serialize::{Deserialize, Serialize},
};
use std::{convert, fmt, io};

#[derive(Copy, Clone, Eq, PartialEq)]
pub(crate) enum Month {
	January = 1,
	February = 2,
	March = 3,
	April = 4,
	May = 5,
	June = 6,
	July = 7,
	August = 8,
	September = 9,
	October = 10,
	November = 11,
	December = 12,
}

impl Month {
	pub(crate) fn number_of_days(self, year: Year) -> u8 {
		match self {
			Self::February => {
				if year.is_leap_year() {
					29
				} else {
					28
				}
			}
			Self::April | Self::June | Self::September | Self::November => 30,
			_ => 31,
		}
	}

	pub(crate) fn next(self) -> Self {
		match self {
			Self::January => Self::February,
			Self::February => Self::March,
			Self::March => Self::April,
			Self::April => Self::May,
			Self::May => Self::June,
			Self::June => Self::July,
			Self::July => Self::August,
			Self::August => Self::September,
			Self::September => Self::October,
			Self::October => Self::November,
			Self::November => Self::December,
			Self::December => Self::January,
		}
	}

	pub(crate) fn prev(self) -> Self {
		match self {
			Self::January => Self::December,
			Self::February => Self::January,
			Self::March => Self::February,
			Self::April => Self::March,
			Self::May => Self::April,
			Self::June => Self::May,
			Self::July => Self::June,
			Self::August => Self::July,
			Self::September => Self::August,
			Self::October => Self::September,
			Self::November => Self::October,
			Self::December => Self::November,
		}
	}

	fn as_str(self) -> &'static str {
		match self {
			Self::January => "January",
			Self::February => "February",
			Self::March => "March",
			Self::April => "April",
			Self::May => "May",
			Self::June => "June",
			Self::July => "July",
			Self::August => "August",
			Self::September => "September",
			Self::October => "October",
			Self::November => "November",
			Self::December => "December",
		}
	}

	pub(crate) fn serialize(self, write: &mut impl io::Write) -> FResult<()> {
		self.as_u8().serialize(write)?;
		Ok(())
	}

	pub(crate) fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		Self::try_from(u8::deserialize(read)?).map_err(|_| FendError::DeserializationError)
	}

	fn as_u8(self) -> u8 {
		match self {
			Self::January => 1,
			Self::February => 2,
			Self::March => 3,
			Self::April => 4,
			Self::May => 5,
			Self::June => 6,
			Self::July => 7,
			Self::August => 8,
			Self::September => 9,
			Self::October => 10,
			Self::November => 11,
			Self::December => 12,
		}
	}
}

impl fmt::Debug for Month {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(f, "{}", self.as_str())
	}
}

impl fmt::Display for Month {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(f, "{}", self.as_str())
	}
}

pub(crate) struct InvalidMonthError;

impl convert::TryFrom<u8> for Month {
	type Error = InvalidMonthError;

	fn try_from(month: u8) -> Result<Self, Self::Error> {
		Ok(match month {
			1 => Self::January,
			2 => Self::February,
			3 => Self::March,
			4 => Self::April,
			5 => Self::May,
			6 => Self::June,
			7 => Self::July,
			8 => Self::August,
			9 => Self::September,
			10 => Self::October,
			11 => Self::November,
			12 => Self::December,
			_ => return Err(InvalidMonthError),
		})
	}
}
