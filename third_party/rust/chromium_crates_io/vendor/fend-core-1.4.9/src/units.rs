use std::borrow::Cow;

use crate::error::{FendError, Interrupt};
use crate::eval::evaluate_to_value;
use crate::num::Number;
use crate::result::FResult;
use crate::value::Value;
use crate::Attrs;

mod builtin;

pub(crate) use builtin::lookup_default_unit;
pub(crate) use builtin::IMPLICIT_UNIT_MAP;

#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub(crate) enum PrefixRule {
	NoPrefixesAllowed,
	LongPrefixAllowed,
	LongPrefix,
	ShortPrefixAllowed,
	ShortPrefix,
}

#[derive(Debug)]
pub(crate) struct UnitDef {
	singular: Cow<'static, str>,
	plural: Cow<'static, str>,
	prefix_rule: PrefixRule,
	alias: bool,
	value: Value,
}

fn expr_unit<I: Interrupt>(
	unit_def: (Cow<'static, str>, Cow<'static, str>, Cow<'static, str>),
	attrs: Attrs,
	context: &mut crate::Context,
	int: &I,
) -> FResult<UnitDef> {
	let (singular, plural, definition) = unit_def;
	let mut definition = definition.trim();
	if definition == "$CURRENCY" {
		let Some(exchange_rate_fn) = &context.get_exchange_rate else {
			return Err(FendError::NoExchangeRatesAvailable);
		};
		let one_base_in_currency = exchange_rate_fn.relative_to_base_currency(&singular)?;
		let value = evaluate_to_value(
			format!("(1/{one_base_in_currency}) BASE_CURRENCY").as_str(),
			None,
			attrs,
			context,
			int,
		)?
		.expect_num()?;
		let value = Number::create_unit_value_from_value(
			&value,
			Cow::Borrowed(""),
			false,
			singular.clone(),
			plural.clone(),
			int,
		)?;
		return Ok(UnitDef {
			singular,
			plural,
			prefix_rule: PrefixRule::LongPrefixAllowed,
			alias: false,
			value: Value::Num(Box::new(value)),
		});
	}
	let mut rule = PrefixRule::NoPrefixesAllowed;
	if let Some(remaining) = definition.strip_prefix("l@") {
		definition = remaining;
		rule = PrefixRule::LongPrefixAllowed;
	}
	if let Some(remaining) = definition.strip_prefix("lp@") {
		definition = remaining;
		rule = PrefixRule::LongPrefix;
	}
	if let Some(remaining) = definition.strip_prefix("s@") {
		definition = remaining;
		rule = PrefixRule::ShortPrefixAllowed;
	}
	if let Some(remaining) = definition.strip_prefix("sp@") {
		definition = remaining;
		rule = PrefixRule::ShortPrefix;
	}
	if definition == "!" {
		return Ok(UnitDef {
			value: Value::Num(Box::new(Number::new_base_unit(
				singular.clone(),
				plural.clone(),
			))),
			prefix_rule: rule,
			singular,
			plural,
			alias: false,
		});
	}
	let (alias, definition) = definition
		.strip_prefix('=')
		.map_or((false, definition), |remaining| (true, remaining));
	// long prefixes like `hecto` are always treated as aliases
	let alias = alias || rule == PrefixRule::LongPrefix;
	let mut num = evaluate_to_value(definition, None, attrs, context, int)?.expect_num()?;

	// There are three cases to consider:
	//   1. Unitless aliases (e.g. `million` or `mega`) should be treated as an
	//      actual unit, but with the `alias` flag set so it can be simplified
	//      when possible.
	//   2. Aliases with units (e.g. `sqft`) should be a pure alias (not a unit)
	//      so it can always be replaced. We can't convert this like unitless
	//      aliases since we would be simplifying it to base units (scaled m^2),
	//      so the precise unit we are aliasing to would be lost.
	//   3. Units that aren't aliased (e.g. `meter`) should be converted to a
	//      normal unit.
	//
	// One exception to these cases is `unitless`, which should always be
	// replaced with `1` even when we aren't simplifying, so it is defined
	// manually instead of as a normal unit definition.

	#[allow(clippy::nonminimal_bool)]
	if !alias || (alias && num.is_unitless(int)?) {
		// convert to an actual unit (cases 1 and 3)
		num = Number::create_unit_value_from_value(
			&num,
			Cow::Borrowed(""),
			alias,
			singular.clone(),
			plural.clone(),
			int,
		)?;
	}
	Ok(UnitDef {
		value: Value::Num(Box::new(num)),
		prefix_rule: rule,
		singular,
		plural,
		alias,
	})
}

fn construct_prefixed_unit<I: Interrupt>(a: UnitDef, b: UnitDef, int: &I) -> FResult<Value> {
	let product = a.value.expect_num()?.mul(b.value.expect_num()?, int)?;
	assert_eq!(a.singular, a.plural);
	let unit = Number::create_unit_value_from_value(
		&product, a.singular, b.alias, b.singular, b.plural, int,
	)?;
	Ok(Value::Num(Box::new(unit)))
}

pub(crate) fn query_unit<I: Interrupt>(
	ident: &str,
	attrs: Attrs,
	context: &mut crate::Context,
	int: &I,
) -> FResult<Value> {
	if ident.starts_with('\'') && ident.ends_with('\'') && ident.len() >= 3 {
		let ident = ident.split_at(1).1;
		let ident = ident.split_at(ident.len() - 1).0;
		return Ok(Value::Num(Box::new(Number::new_base_unit(
			ident.to_string().into(),
			ident.to_string().into(),
		))));
	}
	query_unit_static(ident, attrs, context, int)
}

pub(crate) fn query_unit_static<I: Interrupt>(
	ident: &str,
	attrs: Attrs,
	context: &mut crate::Context,
	int: &I,
) -> FResult<Value> {
	match query_unit_case_sensitive(ident, true, attrs, context, int) {
		Err(FendError::IdentifierNotFound(_)) => (),
		Err(e) => return Err(e),
		Ok(value) => {
			return Ok(value);
		}
	}
	query_unit_case_sensitive(ident, false, attrs, context, int)
}

fn query_unit_case_sensitive<I: Interrupt>(
	ident: &str,
	case_sensitive: bool,
	attrs: Attrs,
	context: &mut crate::Context,
	int: &I,
) -> FResult<Value> {
	match query_unit_internal(ident, false, case_sensitive, true, context) {
		Err(FendError::IdentifierNotFound(_)) => (),
		Err(e) => return Err(e),
		Ok(unit_def) => {
			// Return value without prefix. Note that lone short prefixes
			// won't be returned here.
			return Ok(expr_unit(unit_def, attrs, context, int)?.value);
		}
	}
	let mut split_idx = ident.chars().next().unwrap().len_utf8();
	while split_idx < ident.len() {
		let (prefix, remaining_ident) = ident.split_at(split_idx);
		split_idx += remaining_ident.chars().next().unwrap().len_utf8();
		let a = match query_unit_internal(prefix, true, case_sensitive, false, context) {
			Err(FendError::IdentifierNotFound(_)) => continue,
			Err(e) => {
				return Err(e);
			}
			Ok(a) => a,
		};
		match query_unit_internal(remaining_ident, false, case_sensitive, false, context) {
			Err(FendError::IdentifierNotFound(_)) => continue,
			Err(e) => return Err(e),
			Ok(b) => {
				let (a, b) = (
					expr_unit(a, attrs, context, int)?,
					expr_unit(b, attrs, context, int)?,
				);
				if (a.prefix_rule == PrefixRule::LongPrefix
					&& b.prefix_rule == PrefixRule::LongPrefixAllowed)
					|| (a.prefix_rule == PrefixRule::ShortPrefix
						&& b.prefix_rule == PrefixRule::ShortPrefixAllowed)
				{
					// now construct a new unit!
					return construct_prefixed_unit(a, b, int);
				}
				return Err(FendError::IdentifierNotFound(ident.to_string().into()));
			}
		};
	}
	Err(FendError::IdentifierNotFound(ident.to_string().into()))
}

#[allow(clippy::type_complexity)]
fn query_unit_internal(
	ident: &str,
	short_prefixes: bool,
	case_sensitive: bool,
	whole_unit: bool,
	context: &crate::Context,
) -> FResult<(Cow<'static, str>, Cow<'static, str>, Cow<'static, str>)> {
	if !short_prefixes {
		for (s, p, d) in &context.custom_units {
			let p = if p.is_empty() { s } else { p };
			if (ident == s || ident == p)
				|| (!case_sensitive
					&& (s.eq_ignore_ascii_case(ident) || p.eq_ignore_ascii_case(ident)))
			{
				return Ok((
					s.to_string().into(),
					p.to_string().into(),
					d.to_string().into(),
				));
			}
		}
	}
	if whole_unit && context.fc_mode == crate::FCMode::CelsiusFahrenheit {
		if ident == "C" {
			return Ok((
				Cow::Borrowed("C"),
				Cow::Borrowed("C"),
				Cow::Borrowed("=\u{b0}C"),
			));
		} else if ident == "F" {
			return Ok((
				Cow::Borrowed("F"),
				Cow::Borrowed("F"),
				Cow::Borrowed("=\u{b0}F"),
			));
		}
	}
	if let Some(unit_def) = builtin::query_unit(ident, short_prefixes, case_sensitive) {
		Ok(unit_def)
	} else {
		Err(FendError::IdentifierNotFound(ident.to_string().into()))
	}
}

pub(crate) fn get_completions_for_prefix(prefix: &str) -> Vec<crate::Completion> {
	use crate::Completion;

	let mut result = vec![];

	let mut add = |name: &str| {
		if name.starts_with(prefix) && name != prefix {
			result.push(Completion {
				display: name.to_string(),
				insert: name.split_at(prefix.len()).1.to_string(),
			});
		}
	};

	for group in builtin::ALL_UNIT_DEFS {
		for (s, _, _, _) in *group {
			// only add singular name, since plurals
			// unnecessarily clutter autocompletions
			add(s);
		}
	}

	result.sort_by(|a, b| a.display().cmp(b.display()));

	result
}
