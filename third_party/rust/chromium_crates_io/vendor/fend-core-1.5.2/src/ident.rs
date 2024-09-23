use std::{borrow::Cow, fmt, io};

use crate::{
	result::FResult,
	serialize::{Deserialize, Serialize},
};

#[derive(Clone, Debug, PartialEq, Eq)]
pub(crate) struct Ident(Cow<'static, str>);

impl Ident {
	pub(crate) fn new_str(s: &'static str) -> Self {
		Self(Cow::Borrowed(s))
	}

	pub(crate) fn new_string(s: String) -> Self {
		Self(Cow::Owned(s))
	}

	pub(crate) fn as_str(&self) -> &str {
		self.0.as_ref()
	}

	pub(crate) fn is_prefix_unit(&self) -> bool {
		// when changing this also make sure to change number output formatting
		// lexer identifier splitting
		["$", "\u{a3}", "\u{a5}"].contains(&&*self.0)
	}

	pub(crate) fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
		self.0.as_ref().serialize(write)
	}

	pub(crate) fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		Ok(Self(Cow::Owned(String::deserialize(read)?)))
	}
}

impl From<String> for Ident {
	fn from(value: String) -> Self {
		Self(Cow::Owned(value))
	}
}

impl From<&'static str> for Ident {
	fn from(value: &'static str) -> Self {
		Self(Cow::Borrowed(value))
	}
}

impl fmt::Display for Ident {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(f, "{}", self.0)
	}
}
