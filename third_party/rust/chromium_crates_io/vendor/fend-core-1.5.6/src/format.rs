use crate::error::Interrupt;
use crate::num::Exact;
use crate::result::FResult;
use std::fmt;

pub(crate) trait Format {
	type Params: Default;
	type Out: fmt::Display + fmt::Debug;

	fn format<I: Interrupt>(&self, params: &Self::Params, int: &I) -> FResult<Exact<Self::Out>>;

	/// Simpler alternative to calling format
	fn fm<I: Interrupt>(&self, int: &I) -> FResult<Self::Out> {
		Ok(self.format(&Default::default(), int)?.value)
	}
}

pub(crate) trait DisplayDebug: fmt::Display + fmt::Debug {}

impl<T: fmt::Display + fmt::Debug> DisplayDebug for T {}
