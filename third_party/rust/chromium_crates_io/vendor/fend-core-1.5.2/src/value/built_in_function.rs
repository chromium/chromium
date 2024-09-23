use crate::result::FResult;
use crate::value::Expr;
use crate::value::Ident;
use crate::value::Scope;
use crate::FendError;
use crate::{Deserialize, Serialize};
use std::{fmt, io};

use std::sync::Arc;

use crate::value::Value;

#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub(crate) enum BuiltInFunction {
	Approximately,
	Abs,
	Sin,
	Cos,
	Tan,
	Asin,
	Acos,
	Atan,
	Sinh,
	Cosh,
	Tanh,
	Asinh,
	Acosh,
	Atanh,
	Ln,
	Log2,
	Log10,
	Base,
	Sample,
	Mean,
	Not,
	Conjugate,
	Real,
	Imag,
	Arg,
	Floor,
	Ceil,
	Round,
	Fibonacci,
}

impl BuiltInFunction {
	pub(crate) fn wrap_with_expr(
		self,
		lazy_fn: impl FnOnce(Box<Expr>) -> Expr,
		scope: Option<Arc<Scope>>,
	) -> Value {
		Value::Fn(
			Ident::new_str("x"),
			Box::new(lazy_fn(Box::new(Expr::ApplyFunctionCall(
				Box::new(Expr::Ident(Ident::new_str(self.as_str()))),
				Box::new(Expr::Ident(Ident::new_str("x"))),
			)))),
			scope,
		)
	}

	pub(crate) fn invert(self) -> FResult<Value> {
		Ok(match self {
			Self::Sin => Value::BuiltInFunction(Self::Asin),
			Self::Cos => Value::BuiltInFunction(Self::Acos),
			Self::Tan => Value::BuiltInFunction(Self::Atan),
			Self::Asin => Value::BuiltInFunction(Self::Sin),
			Self::Acos => Value::BuiltInFunction(Self::Cos),
			Self::Atan => Value::BuiltInFunction(Self::Tan),
			Self::Sinh => Value::BuiltInFunction(Self::Asinh),
			Self::Cosh => Value::BuiltInFunction(Self::Acosh),
			Self::Tanh => Value::BuiltInFunction(Self::Atanh),
			Self::Asinh => Value::BuiltInFunction(Self::Sinh),
			Self::Acosh => Value::BuiltInFunction(Self::Cosh),
			Self::Atanh => Value::BuiltInFunction(Self::Tanh),
			_ => return Err(FendError::UnableToInvertFunction(self.as_str())),
		})
	}

	pub(crate) const fn as_str(self) -> &'static str {
		match self {
			Self::Approximately => "approximately",
			Self::Abs => "abs",
			Self::Sin => "sin",
			Self::Cos => "cos",
			Self::Tan => "tan",
			Self::Asin => "asin",
			Self::Acos => "acos",
			Self::Atan => "atan",
			Self::Sinh => "sinh",
			Self::Cosh => "cosh",
			Self::Tanh => "tanh",
			Self::Asinh => "asinh",
			Self::Acosh => "acosh",
			Self::Atanh => "atanh",
			Self::Ln => "ln",
			Self::Log2 => "log2",
			Self::Log10 => "log10",
			Self::Base => "base",
			Self::Sample => "sample",
			Self::Mean => "mean",
			Self::Not => "not",
			Self::Conjugate => "conjugate",
			Self::Real => "real",
			Self::Imag => "imag",
			Self::Arg => "arg",
			Self::Floor => "floor",
			Self::Ceil => "ceil",
			Self::Round => "round",
			Self::Fibonacci => "fibonacci",
		}
	}

	fn try_from_str(s: &str) -> FResult<Self> {
		Ok(match s {
			"approximately" => Self::Approximately,
			"abs" => Self::Abs,
			"sin" => Self::Sin,
			"cos" => Self::Cos,
			"tan" => Self::Tan,
			"asin" => Self::Asin,
			"acos" => Self::Acos,
			"atan" => Self::Atan,
			"sinh" => Self::Sinh,
			"cosh" => Self::Cosh,
			"tanh" => Self::Tanh,
			"asinh" => Self::Asinh,
			"acosh" => Self::Acosh,
			"atanh" => Self::Atanh,
			"ln" => Self::Ln,
			"log2" => Self::Log2,
			"log10" => Self::Log10,
			"base" => Self::Base,
			"sample" => Self::Sample,
			"not" => Self::Not,
			"conjugate" => Self::Conjugate,
			"real" => Self::Real,
			"imag" => Self::Imag,
			"fibonacci" => Self::Fibonacci,
			_ => return Err(FendError::DeserializationError),
		})
	}

	pub(crate) fn serialize(self, write: &mut impl io::Write) -> FResult<()> {
		self.as_str().serialize(write)
	}

	pub(crate) fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		Self::try_from_str(String::deserialize(read)?.as_str())
	}
}

impl fmt::Display for BuiltInFunction {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(f, "{}", self.as_str())
	}
}
