use std::{fmt, io};

mod day;
mod day_of_week;
mod month;
mod parser;
mod year;

use day::Day;
pub(crate) use day_of_week::DayOfWeek;
pub(crate) use month::Month;
use year::Year;

use crate::{error::FendError, ident::Ident, result::FResult, value::Value, Interrupt};

#[derive(Copy, Clone, Eq, PartialEq)]
pub(crate) struct Date {
	year: Year,
	month: Month,
	day: Day,
}

impl Date {
	pub(crate) fn today(context: &crate::Context) -> FResult<Self> {
		let Some(current_time_info) = &context.current_time else {
			return Err(FendError::UnableToGetCurrentDate);
		};
		let mut ms_since_epoch: i64 = current_time_info.elapsed_unix_time_ms.try_into().unwrap();
		ms_since_epoch -= current_time_info.timezone_offset_secs * 1000;
		let mut days = ms_since_epoch / 86_400_000; // no leap seconds
		let mut year = Year::new(1970);
		while days >= year.number_of_days().into() {
			year = year.next();
			days -= i64::from(year.number_of_days());
		}
		let mut month = Month::January;
		while days >= month.number_of_days(year).into() {
			month = month.next();
			days -= i64::from(month.number_of_days(year));
		}
		Ok(Self {
			year,
			month,
			day: Day::new(days.try_into().unwrap()),
		})
	}

	fn day_of_week(self) -> DayOfWeek {
		let d1 = (1
			+ 5 * ((self.year.value() - 1) % 4)
			+ 4 * ((self.year.value() - 1) % 100)
			+ 6 * ((self.year.value() - 1) % 400))
			% 7;
		let ms = match self.month {
			Month::January => (0, 0),
			Month::February => (3, 3),
			Month::March | Month::November => (3, 4),
			Month::April | Month::July => (6, 0),
			Month::May => (1, 2),
			Month::June => (4, 5),
			Month::August => (2, 3),
			Month::September | Month::December => (5, 6),
			Month::October => (0, 1),
		};
		let m = if self.year.is_leap_year() { ms.1 } else { ms.0 };
		match (d1 + m + i32::from(self.day.value() - 1)) % 7 {
			0 => DayOfWeek::Sunday,
			1 => DayOfWeek::Monday,
			2 => DayOfWeek::Tuesday,
			3 => DayOfWeek::Wednesday,
			4 => DayOfWeek::Thursday,
			5 => DayOfWeek::Friday,
			6 => DayOfWeek::Saturday,
			_ => unreachable!(),
		}
	}

	pub(crate) fn next(self) -> Self {
		if self.day.value() < Month::number_of_days(self.month, self.year) {
			Self {
				day: Day::new(self.day.value() + 1),
				month: self.month,
				year: self.year,
			}
		} else if self.month == Month::December {
			Self {
				day: Day::new(1),
				month: Month::January,
				year: self.year.next(),
			}
		} else {
			Self {
				day: Day::new(1),
				month: self.month.next(),
				year: self.year,
			}
		}
	}

	pub(crate) fn prev(self) -> Self {
		if self.day.value() > 1 {
			Self {
				day: Day::new(self.day.value() - 1),
				month: self.month,
				year: self.year,
			}
		} else if self.month == Month::January {
			Self {
				day: Day::new(31),
				month: Month::December,
				year: self.year.prev(),
			}
		} else {
			let month = self.month.prev();
			Self {
				day: Day::new(Month::number_of_days(month, self.year)),
				month,
				year: self.year,
			}
		}
	}

	pub(crate) fn diff_months(self, mut months: i64) -> FResult<Self> {
		let mut result = self;
		while months >= 12 {
			result.year = result.year.next();
			months -= 12;
		}
		while months <= -12 {
			result.year = result.year.prev();
			months += 12;
		}
		while months > 0 {
			if result.month == Month::December {
				result.month = Month::January;
				result.year = result.year.next();
			} else {
				result.month = result.month.next();
			}
			months -= 1;
		}
		while months < 0 {
			if result.month == Month::January {
				result.month = Month::December;
				result.year = result.year.prev();
			} else {
				result.month = result.month.prev();
			}
			months += 1;
		}
		if result.day.value() > Month::number_of_days(result.month, result.year) {
			let mut before = result;
			before.day = Day::new(Month::number_of_days(before.month, before.year));
			let mut after = result;
			if after.month == Month::December {
				after.month = Month::January;
				after.year = after.year.next();
			} else {
				after.month = after.month.next();
			}
			after.day = Day::new(1);
			return Err(FendError::NonExistentDate {
				year: result.year.value(),
				month: result.month,
				expected_day: result.day.value(),
				before,
				after,
			});
		}
		Ok(result)
	}

	pub(crate) fn parse(s: &str) -> FResult<Self> {
		parser::parse_date(s)
	}

	pub(crate) fn serialize(self, write: &mut impl io::Write) -> FResult<()> {
		self.year.serialize(write)?;
		self.month.serialize(write)?;
		self.day.serialize(write)?;
		Ok(())
	}

	pub(crate) fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		Ok(Self {
			year: Year::deserialize(read)?,
			month: Month::deserialize(read)?,
			day: Day::deserialize(read)?,
		})
	}

	pub(crate) fn get_object_member(self, key: &Ident) -> FResult<crate::value::Value> {
		Ok(match key.as_str() {
			"month" => Value::Month(self.month),
			"day_of_week" => Value::DayOfWeek(self.day_of_week()),
			_ => return Err(FendError::CouldNotFindKey(key.to_string())),
		})
	}

	pub(crate) fn add<I: Interrupt>(self, rhs: Value, int: &I) -> FResult<Value> {
		let rhs = rhs.expect_num()?;
		if rhs.unit_equal_to("day", int)? {
			let num_days = rhs.try_as_usize_unit(int)?;
			let mut result = self;
			for _ in 0..num_days {
				result = result.next();
			}
			Ok(Value::Date(result))
		} else {
			Err(FendError::ExpectedANumber)
		}
	}

	pub(crate) fn sub<I: Interrupt>(self, rhs: Value, int: &I) -> FResult<Value> {
		let rhs = rhs.expect_num()?;

		if rhs.unit_equal_to("day", int)? {
			let num_days = rhs.try_as_usize_unit(int)?;
			let mut result = self;
			for _ in 0..num_days {
				result = result.prev();
			}
			Ok(Value::Date(result))
		} else if rhs.unit_equal_to("week", int)? {
			let num_weeks = rhs.try_as_usize_unit(int)?;
			let mut result = self;
			for _ in 0..num_weeks {
				for _ in 0..7 {
					result = result.prev();
				}
			}
			Ok(Value::Date(result))
		} else if rhs.unit_equal_to("month", int)? {
			let num_months = rhs.try_as_usize_unit(int)?;
			let result = self
				.diff_months(-i64::try_from(num_months).map_err(|_| FendError::ValueTooLarge)?)?;
			Ok(Value::Date(result))
		} else if rhs.unit_equal_to("year", int)? {
			let num_years = rhs.try_as_usize_unit(int)?;
			let num_months = num_years * 12;
			let result = self
				.diff_months(-i64::try_from(num_months).map_err(|_| FendError::ValueTooLarge)?)?;
			Ok(Value::Date(result))
		} else {
			Err(FendError::ExpectedANumber)
		}
	}
}

impl fmt::Debug for Date {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(
			f,
			"{}, {} {} {}",
			self.day_of_week(),
			self.day,
			self.month,
			self.year
		)
	}
}

impl fmt::Display for Date {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(f, "{self:?}")
	}
}
