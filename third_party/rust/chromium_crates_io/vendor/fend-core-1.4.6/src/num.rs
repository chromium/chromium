use std::fmt;

mod base;
mod bigrat;
mod biguint;
mod complex;
#[allow(unused_macros, unused_variables, dead_code)]
mod continued_fraction;
mod dist;
mod exact;
mod formatting_style;
mod real;
mod unit;

pub(crate) use formatting_style::FormattingStyle;

use crate::error::FendError;

pub(crate) type Number = unit::Value;
pub(crate) type Base = base::Base;
pub(crate) type Exact<T> = exact::Exact<T>;

#[derive(Debug)]
pub(crate) enum RangeBound<T> {
	None,
	Open(T),
	Closed(T),
}

impl<T: fmt::Display + fmt::Debug + 'static> RangeBound<T> {
	fn into_dyn(self) -> RangeBound<Box<dyn crate::format::DisplayDebug>> {
		match self {
			Self::None => RangeBound::None,
			Self::Open(v) => RangeBound::Open(Box::new(v)),
			Self::Closed(v) => RangeBound::Closed(Box::new(v)),
		}
	}
}

#[derive(Debug)]
pub(crate) struct Range<T> {
	pub(crate) start: RangeBound<T>,
	pub(crate) end: RangeBound<T>,
}

impl<T> Range<T> {
	pub(crate) fn open(start: T, end: T) -> Self {
		Self {
			start: RangeBound::Open(start),
			end: RangeBound::Open(end),
		}
	}
}

impl Range<i32> {
	const ZERO_OR_GREATER: Self = Self {
		start: RangeBound::Closed(0),
		end: RangeBound::None,
	};
}

impl<T: fmt::Display> fmt::Display for Range<T> {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> Result<(), fmt::Error> {
		match &self.start {
			RangeBound::None => write!(f, "(-\u{221e}, ")?, // infinity symbol
			RangeBound::Open(v) => write!(f, "({v}, ")?,
			RangeBound::Closed(v) => write!(f, "[{v}, ")?,
		}
		match &self.end {
			RangeBound::None => write!(f, "\u{221e})")?,
			RangeBound::Open(v) => write!(f, "{v})")?,
			RangeBound::Closed(v) => write!(f, "{v}]")?,
		}
		Ok(())
	}
}

fn out_of_range<T: fmt::Display + fmt::Debug + 'static, U: fmt::Display + fmt::Debug + 'static>(
	value: T,
	range: Range<U>,
) -> FendError {
	FendError::OutOfRange {
		value: Box::new(value),
		range: Range {
			start: range.start.into_dyn(),
			end: range.end.into_dyn(),
		},
	}
}
