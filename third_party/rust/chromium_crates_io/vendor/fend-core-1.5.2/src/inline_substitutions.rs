use crate::{Context, Interrupt};

pub enum InlineFendResultComponent {
	Unprocessed(String),
	FendOutput(String),
	FendError(String),
}

impl InlineFendResultComponent {
	pub fn get_contents(&self) -> &str {
		match self {
			Self::Unprocessed(s) | Self::FendOutput(s) | Self::FendError(s) => s.as_str(),
		}
	}

	fn to_json(&self, out: &mut String) {
		out.push_str("{\"type\": ");
		match self {
			Self::Unprocessed(_) => out.push_str("\"unprocessed\""),
			Self::FendOutput(_) => out.push_str("\"fend_output\""),
			Self::FendError(_) => out.push_str("\"fend_error\""),
		}
		out.push_str(", \"contents\": \"");
		crate::json::escape_string(self.get_contents(), out);
		out.push_str("\"}");
	}
}

pub struct InlineFendResult {
	parts: Vec<InlineFendResultComponent>,
}

impl InlineFendResult {
	pub fn get_parts(&self) -> &[InlineFendResultComponent] {
		self.parts.as_slice()
	}

	pub fn to_json(&self) -> String {
		let mut res = String::new();
		res.push('[');
		for (i, part) in self.get_parts().iter().enumerate() {
			if i > 0 {
				res.push(',');
			}
			part.to_json(&mut res);
		}
		res.push(']');
		res
	}
}

/// Evaluates fend syntax embedded in Markdown or similarly-formatted strings.
///
/// Any calculations in `[[` and `]]` are evaluated and replaced with their
/// results.
///
/// This function implements a subset of markdown parsing, ensuring that
/// backtick-escaped notation will not be evaluated.
///
/// # Examples
/// ```
/// let mut ctx = fend_core::Context::new();
/// struct NeverInterrupt;
/// impl fend_core::Interrupt for NeverInterrupt {
/// 	fn should_interrupt(&self) -> bool {
/// 		false
/// 	}
/// }
/// let int = NeverInterrupt;
///
/// let result = fend_core::substitute_inline_fend_expressions(
/// 	"The answer is [[1+1]].", &mut ctx, &int);
///
/// assert_eq!(result.get_parts().len(), 3);
/// assert_eq!(result.get_parts()[0].get_contents(), "The answer is ");
/// assert_eq!(result.get_parts()[1].get_contents(), "2");
/// assert_eq!(result.get_parts()[2].get_contents(), ".");
/// ```
pub fn substitute_inline_fend_expressions(
	input: &str,
	context: &mut Context,
	int: &impl Interrupt,
) -> InlineFendResult {
	let mut result = InlineFendResult { parts: vec![] };
	let mut current_component = String::new();
	let mut inside_fend_expr = false;
	let mut inside_backticks = false;
	for ch in input.chars() {
		current_component.push(ch);
		if ch == '`' {
			inside_backticks = !inside_backticks;
		}
		if !inside_fend_expr && !inside_backticks && current_component.ends_with("[[") {
			current_component.truncate(current_component.len() - 2);
			result
				.parts
				.push(InlineFendResultComponent::Unprocessed(current_component));
			current_component = String::new();
			inside_fend_expr = true;
		} else if inside_fend_expr && !inside_backticks && current_component.ends_with("]]") {
			current_component.truncate(current_component.len() - 2);
			match crate::evaluate_with_interrupt(&current_component, context, int) {
				Ok(res) => result.parts.push(InlineFendResultComponent::FendOutput(
					res.get_main_result().to_string(),
				)),
				Err(msg) => result.parts.push(InlineFendResultComponent::FendError(msg)),
			}
			current_component = String::new();
			inside_fend_expr = false;
		}
	}
	if inside_fend_expr {
		current_component.insert_str(0, "[[");
	}
	result
		.parts
		.push(InlineFendResultComponent::Unprocessed(current_component));
	result
}

#[cfg(test)]
mod tests {
	use super::*;

	#[track_caller]
	fn simple_test(input: &str, expected: &str) {
		let mut ctx = crate::Context::new();
		let int = crate::interrupt::Never;
		let mut result = String::new();
		for part in substitute_inline_fend_expressions(input, &mut ctx, &int).parts {
			result.push_str(part.get_contents());
		}
		if expected == "=" {
			assert_eq!(result, input);
		} else {
			assert_eq!(result, expected);
		}
	}

	#[test]
	fn trivial_tests() {
		simple_test("", "");
		simple_test("a", "a");
	}

	#[test]
	fn longer_unprocessed_test() {
		simple_test(
			"auidhwiaudb   \n\naiusdfba!!! `code`\n\n\n```rust\nfn foo() {}\n```",
			"=",
		);
	}

	#[test]
	fn simple_fend_expr() {
		simple_test("[[1+1]]", "2");
		simple_test("[[2+2]][[6*6]]", "436");
		simple_test("[[a = 5; 3a]]\n[[6a]]", "15\n30");
		simple_test("[[2+\n\r\n2\n\n\r\n]][[1]]", "41");
		simple_test(
			"The answer is [[\n  # let's work out 40 + 2:\n  40+2\n]].",
			"The answer is 42.",
		);
		simple_test("[[]]", "");
		simple_test("[[", "[[");
		simple_test("]]", "]]");
	}

	#[test]
	fn escaped_exprs() {
		simple_test("`[[1+1]]` = [[1+1]]", "`[[1+1]]` = 2");
		simple_test("`[[1+1]]` = [[1+1\n\n]]", "`[[1+1]]` = 2");
		simple_test("```\n[[2+2]]\n```", "=");
	}
}
