use std::{borrow::Cow, fmt, io};

use crate::result::FResult;
use crate::serialize::{Deserialize, Serialize};

/// Represents a base unit, identified solely by its name. The name is not exposed to the user.
#[derive(Clone, PartialEq, Eq, Hash)]
pub(crate) struct BaseUnit {
	name: Cow<'static, str>,
}

impl fmt::Debug for BaseUnit {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(f, "{}", self.name)
	}
}

impl BaseUnit {
	pub(crate) const fn new(name: Cow<'static, str>) -> Self {
		Self { name }
	}

	pub(crate) const fn new_static(name: &'static str) -> Self {
		Self {
			name: Cow::Borrowed(name),
		}
	}

	pub(crate) fn name(&self) -> &str {
		self.name.as_ref()
	}

	pub(crate) fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
		self.name.as_ref().serialize(write)?;
		Ok(())
	}

	pub(crate) fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		Ok(Self {
			name: Cow::Owned(String::deserialize(read)?),
		})
	}
}
