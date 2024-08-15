use crate::result::FResult;
use crate::FendError;
use crate::{Deserialize, Serialize};
use std::fmt;
use std::io;

#[derive(Copy, Clone, Eq, PartialEq)]
pub(crate) struct Day(u8);

impl Day {
	pub(crate) fn value(self) -> u8 {
		self.0
	}

	pub(crate) fn new(day: u8) -> Self {
		assert!(day != 0 && day < 32, "day value {day} is out of range");
		Self(day)
	}

	pub(crate) fn serialize(self, write: &mut impl io::Write) -> FResult<()> {
		self.value().serialize(write)?;
		Ok(())
	}

	pub(crate) fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		let n = u8::deserialize(read)?;
		if n == 0 || n >= 32 {
			return Err(FendError::DeserializationError);
		}
		Ok(Self::new(n))
	}
}

impl fmt::Debug for Day {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(f, "{}", self.0)
	}
}

impl fmt::Display for Day {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(f, "{}", self.0)
	}
}

#[cfg(test)]
mod tests {
	use super::*;

	#[test]
	#[should_panic(expected = "day value 0 is out of range")]
	fn day_0() {
		Day::new(0);
	}

	#[test]
	#[should_panic(expected = "day value 32 is out of range")]
	fn day_32() {
		Day::new(32);
	}

	#[test]
	fn day_to_string() {
		assert_eq!(Day::new(1).to_string(), "1");
	}
}
