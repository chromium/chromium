use std::{convert, fmt, io};

use crate::{
	error::FendError,
	result::FResult,
	serialize::{Deserialize, Serialize},
};

#[derive(Copy, Clone, Eq, PartialEq)]
pub(crate) struct Year(i32);

impl Year {
	pub(crate) fn new(year: i32) -> Self {
		assert!(year != 0, "year 0 is invalid");
		Self(year)
	}

	#[inline]
	pub(crate) fn value(self) -> i32 {
		self.0
	}

	pub(crate) fn next(self) -> Self {
		if self.value() == -1 {
			Self::new(1)
		} else {
			Self::new(self.value() + 1)
		}
	}

	pub(crate) fn prev(self) -> Self {
		if self.value() == 1 {
			Self::new(-1)
		} else {
			Self::new(self.value() - 1)
		}
	}

	pub(crate) fn is_leap_year(self) -> bool {
		if self.value() % 400 == 0 {
			true
		} else if self.value() % 100 == 0 {
			false
		} else {
			self.value() % 4 == 0
		}
	}

	pub(crate) fn number_of_days(self) -> u16 {
		if self.is_leap_year() {
			366
		} else {
			365
		}
	}

	pub(crate) fn serialize(self, write: &mut impl io::Write) -> FResult<()> {
		self.value().serialize(write)?;
		Ok(())
	}

	pub(crate) fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		Self::try_from(i32::deserialize(read)?).map_err(|_| FendError::DeserializationError)
	}
}

pub(crate) struct InvalidYearError;

impl convert::TryFrom<i32> for Year {
	type Error = InvalidYearError;

	fn try_from(year: i32) -> Result<Self, Self::Error> {
		if year == 0 {
			Err(InvalidYearError)
		} else {
			Ok(Self(year))
		}
	}
}

impl fmt::Debug for Year {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		if self.value() < 0 {
			write!(f, "{} BC", -self.0)
		} else {
			write!(f, "{}", self.0)
		}
	}
}

impl fmt::Display for Year {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		if self.value() < 0 {
			write!(f, "{} BC", -self.0)
		} else {
			write!(f, "{}", self.0)
		}
	}
}

#[cfg(test)]
mod tests {
	use super::*;

	#[test]
	#[should_panic(expected = "year 0 is invalid")]
	fn year_0() {
		Year::new(0);
	}

	#[test]
	fn negative_year_string() {
		assert_eq!(Year::new(-823).to_string(), "823 BC");
	}
}
