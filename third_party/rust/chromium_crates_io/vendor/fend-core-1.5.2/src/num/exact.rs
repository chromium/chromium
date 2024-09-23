// helper struct for keeping track of which values are exact

use std::fmt;
use std::ops::Neg;

#[derive(Copy, Clone)]
pub(crate) struct Exact<T: fmt::Debug> {
	pub(crate) value: T,
	pub(crate) exact: bool,
}

impl<T: fmt::Debug> fmt::Debug for Exact<T> {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		if self.exact {
			write!(f, "exactly ")?;
		} else {
			write!(f, "approx. ")?;
		}
		write!(f, "{:?}", self.value)?;
		Ok(())
	}
}

impl<T: fmt::Debug> Exact<T> {
	pub(crate) fn new(value: T, exact: bool) -> Self {
		Self { value, exact }
	}

	pub(crate) fn apply<R: fmt::Debug, F: FnOnce(T) -> R>(self, f: F) -> Exact<R> {
		Exact::<R> {
			value: f(self.value),
			exact: self.exact,
		}
	}

	pub(crate) fn try_and_then<
		R: fmt::Debug,
		E: fmt::Debug,
		F: FnOnce(T) -> Result<Exact<R>, E>,
	>(
		self,
		f: F,
	) -> Result<Exact<R>, E> {
		Ok(f(self.value)?.combine(self.exact))
	}

	pub(crate) fn combine(self, x: bool) -> Self {
		Self {
			value: self.value,
			exact: self.exact && x,
		}
	}

	pub(crate) fn re<'a>(&'a self) -> Exact<&'a T> {
		Exact::<&'a T> {
			value: &self.value,
			exact: self.exact,
		}
	}
}

impl<A: fmt::Debug, B: fmt::Debug> Exact<(A, B)> {
	pub(crate) fn pair(self) -> (Exact<A>, Exact<B>) {
		(
			Exact {
				value: self.value.0,
				exact: self.exact,
			},
			Exact {
				value: self.value.1,
				exact: self.exact,
			},
		)
	}
}

impl<T: fmt::Debug + Neg<Output = T>> Neg for Exact<T> {
	type Output = Self;
	fn neg(self) -> Self {
		Self {
			value: -self.value,
			exact: self.exact,
		}
	}
}
