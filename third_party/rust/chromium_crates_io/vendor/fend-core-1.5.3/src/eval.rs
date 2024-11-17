use std::sync::Arc;

use crate::{
	ast, error::Interrupt, lexer, parser, result::FResult, scope::Scope, value::Value, Span,
};

pub(crate) fn evaluate_to_value<I: Interrupt>(
	input: &str,
	scope: Option<Arc<Scope>>,
	attrs: Attrs,
	context: &mut crate::Context,
	int: &I,
) -> FResult<Value> {
	let lex = lexer::lex(input, context, int);
	let mut tokens = vec![];
	let mut missing_open_parens: i32 = 0;
	for token in lex {
		let token = token?;
		if matches!(token, lexer::Token::Symbol(lexer::Symbol::CloseParens)) {
			missing_open_parens += 1;
		}
		tokens.push(token);
	}
	for _ in 0..missing_open_parens {
		tokens.insert(0, lexer::Token::Symbol(lexer::Symbol::OpenParens));
	}
	let parsed = parser::parse_tokens(&tokens)?;
	let result = ast::evaluate(parsed, scope, attrs, context, int)?;
	Ok(result)
}

#[derive(Clone, Copy, Eq, PartialEq, Debug)]
#[allow(clippy::struct_excessive_bools)]
pub(crate) struct Attrs {
	pub(crate) debug: bool,
	pub(crate) show_approx: bool,
	pub(crate) plain_number: bool,
	pub(crate) trailing_newline: bool,
}

impl Default for Attrs {
	fn default() -> Self {
		Self {
			debug: false,
			show_approx: true,
			plain_number: false,
			trailing_newline: true,
		}
	}
}

fn parse_attrs(mut input: &str) -> (Attrs, &str) {
	let mut attrs = Attrs::default();
	while input.starts_with('@') {
		if let Some(remaining) = input.strip_prefix("@debug ") {
			attrs.debug = true;
			input = remaining;
		} else if let Some(remaining) = input.strip_prefix("@noapprox ") {
			attrs.show_approx = false;
			input = remaining;
		} else if let Some(remaining) = input.strip_prefix("@plain_number ") {
			attrs.plain_number = true;
			input = remaining;
		} else if let Some(remaining) = input.strip_prefix("@no_trailing_newline ") {
			attrs.trailing_newline = false;
			input = remaining;
		} else {
			break;
		}
	}
	(attrs, input)
}

/// This also saves the calculation result in a variable `_` and `ans`
pub(crate) fn evaluate_to_spans<I: Interrupt>(
	input: &str,
	scope: Option<Arc<Scope>>,
	context: &mut crate::Context,
	int: &I,
) -> FResult<(Vec<Span>, bool, Attrs)> {
	let (attrs, input) = parse_attrs(input);
	let value = evaluate_to_value(input, scope, attrs, context, int)?;
	context.variables.insert("_".to_string(), value.clone());
	context.variables.insert("ans".to_string(), value.clone());
	Ok((
		if attrs.debug {
			vec![Span::from_string(format!("{value:?}"))]
		} else {
			let mut spans = vec![];
			value.format(0, &mut spans, attrs, context, int)?;
			spans
		},
		value.is_unit(),
		attrs,
	))
}
