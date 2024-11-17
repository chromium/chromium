use crate::date::Date;
use crate::error::{FendError, Interrupt};
use crate::ident::Ident;
use crate::num::{Base, Number};
use crate::result::FResult;
use crate::{Context, DecimalSeparatorStyle};
use std::{borrow, convert, fmt};

#[derive(Clone, Debug)]
pub(crate) enum Token {
	Num(Number),
	Ident(Ident),
	Symbol(Symbol),
	StringLiteral(borrow::Cow<'static, str>),
	Date(Date),
}

#[derive(PartialEq, Eq, Copy, Clone, Debug)]
pub(crate) enum Symbol {
	OpenParens,
	CloseParens,
	Add,
	Sub,
	Mul,
	Div,
	Mod,
	Pow,
	BitwiseAnd,
	BitwiseOr,
	BitwiseXor,
	UnitConversion,
	Factorial,
	Fn,
	Backslash,
	Dot,
	Of,
	ShiftLeft,
	ShiftRight,
	Semicolon,
	Equals,       // used for assignment
	DoubleEquals, // used for equality
	NotEquals,
	Combination,
	Permutation,
}

impl fmt::Display for Symbol {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> Result<(), fmt::Error> {
		let s = match self {
			Self::OpenParens => "(",
			Self::CloseParens => ")",
			Self::Add => "+",
			Self::Sub => "-",
			Self::Mul => "*",
			Self::Div => "/",
			Self::Mod => "mod",
			Self::Pow => "^",
			Self::BitwiseAnd => "&",
			Self::BitwiseOr => "|",
			Self::BitwiseXor => " xor ",
			Self::UnitConversion => "to",
			Self::Factorial => "!",
			Self::Fn => ":",
			Self::Backslash => "\"",
			Self::Dot => ".",
			Self::Of => "of",
			Self::ShiftLeft => "<<",
			Self::ShiftRight => ">>",
			Self::Semicolon => ";",
			Self::Equals => "=",
			Self::DoubleEquals => "==",
			Self::NotEquals => "!=",
			Self::Combination => "nCr",
			Self::Permutation => "nPr",
		};
		write!(f, "{s}")?;
		Ok(())
	}
}

fn parse_char(input: &str) -> FResult<(char, &str)> {
	input
		.chars()
		.next()
		.map_or(Err(FendError::ExpectedACharacter), |ch| {
			let (_, b) = input.split_at(ch.len_utf8());
			Ok((ch, b))
		})
}

fn parse_ascii_digit(input: &str, base: Base) -> FResult<(u8, &str)> {
	let (ch, input) = parse_char(input)?;
	let possible_digit = ch.to_digit(base.base_as_u8().into());
	possible_digit
		.and_then(|d| <u32 as convert::TryInto<u8>>::try_into(d).ok())
		.map_or(Err(FendError::ExpectedADigit(ch)), |digit| {
			Ok((digit, input))
		})
}

fn parse_fixed_char(input: &str, ch: char) -> FResult<((), &str)> {
	let (parsed_ch, input) = parse_char(input)?;
	if parsed_ch == ch {
		Ok(((), input))
	} else {
		Err(FendError::ExpectedChar(ch, parsed_ch))
	}
}

fn parse_digit_separator(
	input: &str,
	decimal_separator: DecimalSeparatorStyle,
) -> FResult<((), &str)> {
	let (parsed_ch, input) = parse_char(input)?;
	if parsed_ch == '_' || parsed_ch == decimal_separator.thousands_separator() {
		Ok(((), input))
	} else {
		Err(FendError::ExpectedDigitSeparator(parsed_ch))
	}
}

// Parses a plain integer with no whitespace and no base prefix.
// Leading minus sign is not allowed.
fn parse_integer<'a, E: From<FendError>>(
	input: &'a str,
	allow_digit_separator: bool,
	base: Base,
	decimal_separator: DecimalSeparatorStyle,
	process_digit: &mut impl FnMut(u8) -> Result<(), E>,
) -> Result<((), &'a str), E> {
	let (digit, mut input) = parse_ascii_digit(input, base)?;
	process_digit(digit)?;
	let mut parsed_digit_separator;
	loop {
		if let Ok(((), remaining)) = parse_digit_separator(input, decimal_separator) {
			input = remaining;
			parsed_digit_separator = true;
			if !allow_digit_separator {
				return Err(FendError::DigitSeparatorsNotAllowed.into());
			}
		} else {
			parsed_digit_separator = false;
		}
		match parse_ascii_digit(input, base) {
			Err(_) => {
				if parsed_digit_separator {
					return Err(FendError::DigitSeparatorsOnlyBetweenDigits.into());
				}
				break;
			}
			Ok((digit, next_input)) => {
				process_digit(digit)?;
				input = next_input;
			}
		}
	}
	Ok(((), input))
}

fn parse_base_prefix(
	input: &str,
	decimal_separator: DecimalSeparatorStyle,
) -> FResult<(Base, &str)> {
	// 0x -> 16
	// 0o -> 8
	// 0b -> 2
	// base# -> base (where 2 <= base <= 36)
	// case-sensitive, no whitespace allowed
	if let Ok(((), input)) = parse_fixed_char(input, '0') {
		let (ch, input) = parse_char(input)?;
		Ok((Base::from_zero_based_prefix_char(ch)?, input))
	} else {
		let mut custom_base: u8 = 0;
		let ((), input) = parse_integer(
			input,
			false,
			Base::default(),
			decimal_separator,
			&mut |digit| -> Result<(), FendError> {
				let error = FendError::BaseTooLarge;
				if custom_base > 3 {
					return Err(error);
				}
				custom_base = 10 * custom_base + digit;
				if custom_base > 36 {
					return Err(error);
				}
				Ok(())
			},
		)?;
		if custom_base < 2 {
			return Err(FendError::BaseTooSmall);
		}
		let ((), input) = parse_fixed_char(input, '#')?;
		Ok((Base::from_custom_base(custom_base)?, input))
	}
}

// Try and parse recurring digits in parentheses.
// '1.0(0)' -> success
// '1.0(a)', '1.0( 0)' -> Ok, but not parsed
// '1.0(3a)' -> FendError

fn parse_recurring_digits<'a, I: Interrupt>(
	input: &'a str,
	number: &mut Number,
	num_nonrec_digits: usize,
	base: Base,
	decimal_separator: DecimalSeparatorStyle,
	int: &I,
) -> FResult<((), &'a str)> {
	let original_input = input;
	// If there's no '(': return Ok but don't parse anything
	if parse_fixed_char(input, '(').is_err() {
		return Ok(((), original_input));
	}
	let ((), input) = parse_fixed_char(input, '(')?;
	if parse_ascii_digit(input, base).is_err() {
		// return Ok if there were no digits
		return Ok(((), original_input));
	}
	let mut recurring_number_num = Number::from(0);
	let mut recurring_number_den = Number::from(1);
	let base_as_u64 = u64::from(base.base_as_u8());
	let ((), input) = parse_integer(
		input,
		true,
		base,
		decimal_separator,
		&mut |digit| -> FResult<()> {
			let digit_as_u64 = u64::from(digit);
			recurring_number_num = recurring_number_num
				.clone()
				.mul(base_as_u64.into(), int)?
				.add(digit_as_u64.into(), decimal_separator, int)?;
			recurring_number_den = recurring_number_den.clone().mul(base_as_u64.into(), int)?;
			Ok(())
		},
	)?;
	recurring_number_den = recurring_number_den
		.clone()
		.sub(1.into(), decimal_separator, int)?;
	for _ in 0..num_nonrec_digits {
		recurring_number_den = recurring_number_den.clone().mul(base_as_u64.into(), int)?;
	}
	*number = number.clone().add(
		recurring_number_num.div(recurring_number_den, int)?,
		decimal_separator,
		int,
	)?;
	// return an error if there are any other characters before the closing parentheses
	let ((), input) = parse_fixed_char(input, ')')?;
	Ok(((), input))
}

#[allow(clippy::too_many_lines)]
fn parse_basic_number<'a, I: Interrupt>(
	mut input: &'a str,
	base: Base,
	decimal_separator: DecimalSeparatorStyle,
	int: &I,
) -> FResult<(Number, &'a str)> {
	let mut is_dice_with_no_count = false;
	if input.starts_with('d') && base.base_as_u8() <= 10 {
		let mut chars = input.chars();
		chars.next();
		let following = chars.next();
		if following.is_some() && following.unwrap().is_ascii_digit() {
			is_dice_with_no_count = true;
		}
	}

	// parse integer component
	let mut res = Number::zero_with_base(base);
	let base_as_u64 = u64::from(base.base_as_u8());
	let mut is_integer = true;

	let decimal_point_char = decimal_separator.decimal_separator();

	if parse_fixed_char(input, decimal_point_char).is_err() && !is_dice_with_no_count {
		let ((), remaining) = parse_integer(
			input,
			true,
			base,
			decimal_separator,
			&mut |digit| -> FResult<()> {
				res = res.clone().mul(base_as_u64.into(), int)?.add(
					u64::from(digit).into(),
					decimal_separator,
					int,
				)?;
				Ok(())
			},
		)?;
		input = remaining;
	}

	// parse decimal point and at least one digit
	if let Ok(((), remaining)) = parse_fixed_char(input, decimal_point_char) {
		is_integer = false;
		let mut num_nonrec_digits = 0;
		let mut numerator = Number::zero_with_base(base);
		let mut denominator = Number::zero_with_base(base).add(1.into(), decimal_separator, int)?;
		if parse_fixed_char(remaining, '(').is_err() {
			let ((), remaining) = parse_integer(
				remaining,
				true,
				base,
				decimal_separator,
				&mut |digit| -> Result<(), FendError> {
					numerator = numerator.clone().mul(base_as_u64.into(), int)?.add(
						u64::from(digit).into(),
						decimal_separator,
						int,
					)?;
					denominator = denominator.clone().mul(base_as_u64.into(), int)?;
					num_nonrec_digits += 1;
					Ok(())
				},
			)?;
			input = remaining;
		} else {
			input = remaining;
		}
		res = res.add(numerator.div(denominator, int)?, decimal_separator, int)?;

		// try parsing recurring decimals
		let ((), remaining) = parse_recurring_digits(
			input,
			&mut res,
			num_nonrec_digits,
			base,
			decimal_separator,
			int,
		)?;
		input = remaining;
	}

	// parse dice syntax
	if is_integer && base.base_as_u8() <= 10 {
		if let Ok(((), remaining)) = parse_fixed_char(input, 'd') {
			// peek to see if there's a digit immediately after the `d`:
			if parse_ascii_digit(remaining, base).is_ok() {
				let dice_count: u32 = if is_dice_with_no_count {
					1
				} else {
					convert::TryFrom::try_from(res.try_as_usize(decimal_separator, int)?)
						.map_err(|_| FendError::InvalidDiceSyntax)?
				};
				let mut face_count = 0_u32;
				let ((), remaining2) = parse_integer(
					remaining,
					false,
					base,
					decimal_separator,
					&mut |digit| -> FResult<()> {
						face_count = face_count
							.checked_mul(base.base_as_u8().into())
							.ok_or(FendError::InvalidDiceSyntax)?
							.checked_add(digit.into())
							.ok_or(FendError::InvalidDiceSyntax)?;
						Ok(())
					},
				)?;
				if dice_count == 0 || face_count == 0 {
					return Err(FendError::InvalidDiceSyntax);
				}
				res = Number::new_die(dice_count, face_count, int)?;
				res = res.with_base(base);
				return Ok((res, remaining2));
			}
		}
	}

	// parse optional exponent, but only for base 10 and below
	if base.base_as_u8() <= 10 {
		let (parsed_exponent, remaining) = if let Ok(((), remaining)) = parse_fixed_char(input, 'e')
		{
			(true, remaining)
		} else if let Ok(((), remaining)) = parse_fixed_char(input, 'E') {
			(true, remaining)
		} else {
			(false, "")
		};

		if parsed_exponent {
			// peek ahead to the next char to determine if we should continue parsing an exponent
			let abort = if let Ok((ch, _)) = parse_char(remaining) {
				// abort if there is a non-digit non-plus or minus char after 'e',
				// such as '(', '/' or 'a'. Note that this is only parsed in base <= 10,
				// so letters can never be digits. We do want to include all digits even for
				// base < 10 though to avoid 6#3e9 from being valid.
				!(ch.is_ascii_digit() || ch == '+' || ch == '-')
			} else {
				// if there is no more input after the 'e', abort
				true
			};
			if !abort {
				input = remaining;
				let mut negative_exponent = false;
				if let Ok(((), remaining)) = parse_fixed_char(input, '-') {
					negative_exponent = true;
					input = remaining;
				} else if let Ok(((), remaining)) = parse_fixed_char(input, '+') {
					input = remaining;
				}
				let mut exp = Number::zero_with_base(base);
				let base_num = Number::from(u64::from(base.base_as_u8()));
				let ((), remaining2) = parse_integer(
					input,
					true,
					base,
					decimal_separator,
					&mut |digit| -> FResult<()> {
						exp = (exp.clone().mul(base_num.clone(), int)?).add(
							u64::from(digit).into(),
							decimal_separator,
							int,
						)?;
						Ok(())
					},
				)?;
				if negative_exponent {
					exp = -exp;
				}
				let base_as_number: Number = base_as_u64.into();
				res = res.mul(base_as_number.pow(exp, decimal_separator, int)?, int)?;
				input = remaining2;
			}
		}
	}

	// parse exponentiation via unicode superscript digits
	if base.base_as_u8() <= 10
		&& input
			.chars()
			.next()
			.is_some_and(|c| SUPERSCRIPT_DIGITS.contains(&c))
	{
		if let Ok((mut power_digits, remaining)) = parse_power_number(input) {
			let mut exponent = Number::zero_with_base(base);

			power_digits.reverse();

			for (i, digit) in power_digits.into_iter().enumerate() {
				let num = digit * 10u64.pow(u32::try_from(i).unwrap());
				exponent = exponent.add(num.into(), decimal_separator, int)?;
			}

			res = res.pow(exponent, decimal_separator, int)?;
			input = remaining;
		}
	}

	Ok((res, input))
}

const SUPERSCRIPT_DIGITS: [char; 10] = ['⁰', '¹', '²', '³', '⁴', '⁵', '⁶', '⁷', '⁸', '⁹'];

fn parse_power_number(input: &str) -> FResult<(Vec<u64>, &str)> {
	let mut digits: Vec<u64> = Vec::new();

	let (mut ch, mut input) = parse_char(input)?;
	while let Some((idx, _)) = SUPERSCRIPT_DIGITS
		.iter()
		.enumerate()
		.find(|(_, x)| **x == ch)
	{
		digits.push(idx as u64);
		if input.is_empty() {
			break;
		}
		(ch, input) = parse_char(input)?;
	}

	Ok((digits, input))
}

fn parse_number<'a, I: Interrupt>(
	input: &'a str,
	decimal_separator: DecimalSeparatorStyle,
	int: &I,
) -> FResult<(Number, &'a str)> {
	let (base, input) =
		parse_base_prefix(input, decimal_separator).unwrap_or((Base::default(), input));
	let (res, input) = parse_basic_number(input, base, decimal_separator, int)?;
	Ok((res, input))
}

fn is_valid_in_ident(ch: char, prev: Option<char>) -> bool {
	let allowed_chars = [
		',', '_', '⅛', '¼', '⅜', '½', '⅝', '¾', '⅞', '⅙', '⅓', '⅔', '⅚', '⅕', '⅖', '⅗', '⅘', '°',
		'$', '℃', '℉', '℧', '℈', '℥', '℔', '¢', '£', '¥', '€', '₩', '₪', '₤', '₨', '฿', '₡', '₣',
		'₦', '₧', '₫', '₭', '₮', '₯', '₱', '﷼', '﹩', '￠', '￡', '￥', '￦', '㍱', '㍲', '㍳',
		'㍴', '㍶', '㎀', '㎁', '㎂', '㎃', '㎄', '㎅', '㎆', '㎇', '㎈', '㎉', '㎊', '㎋', '㎌',
		'㎍', '㎎', '㎏', '㎐', '㎑', '㎒', '㎓', '㎔', '㎕', '㎖', '㎗', '㎘', '㎙', '㎚', '㎛',
		'㎜', '㎝', '㎞', '㎟', '㎠', '㎡', '㎢', '㎣', '㎤', '㎥', '㎦', '㎧', '㎨', '㎩', '㎪',
		'㎫', '㎬', '㎭', '㎮', '㎯', '㎰', '㎱', '㎲', '㎳', '㎴', '㎵', '㎶', '㎷', '㎸', '㎹',
		'㎺', '㎻', '㎼', '㎽', '㎾', '㎿', '㏀', '㏁', '㏃', '㏄', '㏅', '㏆', '㏈', '㏉', '㏊',
		'㏌', '㏏', '㏐', '㏓', '㏔', '㏕', '㏖', '㏗', '㏙', '㏛', '㏜', '㏝',
	];
	let only_valid_by_themselves = ['%', '‰', '‱', '′', '″', '’', '”', 'π'];
	let split_on_subsequent_digit = ['$', '£', '¥'];
	let always_invalid = ['λ'];
	if always_invalid.contains(&ch) {
		false
	} else if only_valid_by_themselves.contains(&ch) {
		// these are only valid if there was no previous char
		prev.is_none()
	} else if only_valid_by_themselves.contains(&prev.unwrap_or('a')) {
		// if prev was a char that's only valid by itself, then this next
		// char cannot be part of an identifier
		false
	} else if ch.is_alphabetic() || allowed_chars.contains(&ch) {
		true
	} else {
		// these are valid only if there was a previous non-$ char in this identifier
		prev.is_some()
			&& !(split_on_subsequent_digit.contains(&prev.unwrap_or('a')))
			&& ".0123456789'\"".contains(ch)
	}
}

fn parse_ident(input: &str, allow_dots: bool) -> FResult<(Token, &str)> {
	let (first_char, _) = parse_char(input)?;
	if !is_valid_in_ident(first_char, None) || first_char == '.' && !allow_dots {
		return Err(FendError::InvalidCharAtBeginningOfIdent(first_char));
	}
	let mut byte_idx = first_char.len_utf8();
	let (_, mut remaining) = input.split_at(byte_idx);
	let mut prev_char = first_char;
	while let Ok((next_char, remaining_input)) = parse_char(remaining) {
		if !is_valid_in_ident(next_char, Some(prev_char)) || next_char == '.' && !allow_dots {
			break;
		}
		remaining = remaining_input;
		byte_idx += next_char.len_utf8();
		prev_char = next_char;
	}
	let (ident, input) = input.split_at(byte_idx);
	Ok((
		match ident {
			"to" | "as" | "in" => Token::Symbol(Symbol::UnitConversion),
			"per" => Token::Symbol(Symbol::Div),
			"of" => Token::Symbol(Symbol::Of),
			"mod" => Token::Symbol(Symbol::Mod),
			"xor" | "XOR" => Token::Symbol(Symbol::BitwiseXor),
			"and" | "AND" => Token::Symbol(Symbol::BitwiseAnd),
			"or" | "OR" => Token::Symbol(Symbol::BitwiseOr),
			"nCr" | "choose" => Token::Symbol(Symbol::Combination),
			"nPr" | "permute" => Token::Symbol(Symbol::Permutation),
			_ => Token::Ident(Ident::new_string(ident.to_string())),
		},
		input,
	))
}

fn parse_symbol(ch: char, input: &mut &str) -> FResult<Token> {
	let mut test_next = |next: char| {
		if input.starts_with(next) {
			let (_, remaining) = input.split_at(next.len_utf8());
			*input = remaining;
			true
		} else {
			false
		}
	};
	Ok(Token::Symbol(match ch {
		'(' => Symbol::OpenParens,
		')' => Symbol::CloseParens,
		'+' => Symbol::Add,
		'!' => {
			if test_next('=') {
				Symbol::NotEquals
			} else {
				Symbol::Factorial
			}
		}
		// unicode minus sign
		'-' | '\u{2212}' => Symbol::Sub,
		'*' | '\u{d7}' | '\u{2715}' => {
			if test_next('*') {
				Symbol::Pow
			} else {
				Symbol::Mul
			}
		}
		'/' | '\u{f7}' | '\u{2215}' => Symbol::Div, // unicode division symbol and slash
		'^' => Symbol::Pow,
		'&' => Symbol::BitwiseAnd,
		'|' => Symbol::BitwiseOr,
		':' => Symbol::Fn,
		'=' => {
			if test_next('>') {
				Symbol::Fn
			} else if test_next('=') {
				Symbol::DoubleEquals
			} else {
				Symbol::Equals
			}
		}
		'\u{2260}' => Symbol::NotEquals, // unicode not equal to symbol
		'\\' | '\u{3bb}' => Symbol::Backslash, // lambda symbol
		'.' => Symbol::Dot,
		'<' => {
			if test_next('<') {
				Symbol::ShiftLeft
			} else if test_next('>') {
				Symbol::NotEquals
			} else {
				return Err(FendError::UnexpectedChar(ch));
			}
		}
		'>' => {
			if test_next('>') {
				Symbol::ShiftRight
			} else {
				return Err(FendError::UnexpectedChar(ch));
			}
		}
		';' => Symbol::Semicolon,
		_ => return Err(FendError::UnexpectedChar(ch)),
	}))
}

fn parse_unicode_escape(chars_iter: &mut std::str::CharIndices<'_>) -> FResult<char> {
	if chars_iter
		.next()
		.ok_or(FendError::UnterminatedStringLiteral)?
		.1 != '{'
	{
		return Err(FendError::InvalidUnicodeEscapeSequence);
	}
	let mut result_value = 0;
	let mut zero_length = true;
	loop {
		let (_, ch) = chars_iter
			.next()
			.ok_or(FendError::UnterminatedStringLiteral)?;
		if ch.is_ascii_hexdigit() {
			zero_length = false;
			result_value *= 16;
			result_value += ch
				.to_digit(16)
				.ok_or(FendError::InvalidUnicodeEscapeSequence)?;
			if result_value > 0x10_ffff {
				return Err(FendError::InvalidUnicodeEscapeSequence);
			}
		} else if ch == '}' {
			break;
		} else {
			return Err(FendError::InvalidUnicodeEscapeSequence);
		}
	}
	if zero_length {
		return Err(FendError::InvalidUnicodeEscapeSequence);
	}
	if let Ok(ch) = <char as convert::TryFrom<u32>>::try_from(result_value) {
		Ok(ch)
	} else {
		Err(FendError::InvalidUnicodeEscapeSequence)
	}
}

fn parse_string_literal(input: &str, terminator: char) -> FResult<(Token, &str)> {
	let (_, input) = input.split_at(1);
	let mut chars_iter = input.char_indices();
	let mut literal_length = None;
	let mut literal_string = String::new();
	let mut skip_whitespace = false;
	while let Some((idx, ch)) = chars_iter.next() {
		if skip_whitespace {
			if ch.is_ascii_whitespace() {
				continue;
			}
			skip_whitespace = false;
		}
		if ch == terminator {
			literal_length = Some(idx);
			break;
		}
		if ch == '\\' {
			let (_, next) = chars_iter
				.next()
				.ok_or(FendError::UnterminatedStringLiteral)?;
			let escaped_char = match next {
				'\\' => Some('\\'),
				'"' => Some('"'),
				'\'' => Some('\''),
				'a' => Some('\u{7}'),  // bell
				'b' => Some('\u{8}'),  // backspace
				'e' => Some('\u{1b}'), // escape
				'f' => Some('\u{c}'),  // form feed
				'n' => Some('\n'),     // line feed
				'r' => Some('\r'),     // carriage return
				't' => Some('\t'),     // tab
				'v' => Some('\u{0b}'), // vertical tab
				'x' => {
					// two-character hex code
					let (_, hex1) = chars_iter
						.next()
						.ok_or(FendError::UnterminatedStringLiteral)?;
					let (_, hex2) = chars_iter
						.next()
						.ok_or(FendError::UnterminatedStringLiteral)?;
					let hex1: u8 = convert::TryInto::try_into(
						hex1.to_digit(8).ok_or(FendError::BackslashXOutOfRange)?,
					)
					.unwrap();
					let hex2: u8 = convert::TryInto::try_into(
						hex2.to_digit(16).ok_or(FendError::BackslashXOutOfRange)?,
					)
					.unwrap();
					Some((hex1 * 16 + hex2) as char)
				}
				'u' => Some(parse_unicode_escape(&mut chars_iter)?),
				'z' => {
					skip_whitespace = true;
					None
				}
				'^' => {
					// control character escapes
					let (_, letter) = chars_iter
						.next()
						.ok_or(FendError::UnterminatedStringLiteral)?;
					let code = letter as u8;
					if !(63..=95).contains(&code) {
						return Err(FendError::ExpectedALetterOrCode);
					}
					Some(if code == b'?' {
						'\x7f'
					} else {
						(code - 64) as char
					})
				}
				_ => return Err(FendError::UnknownBackslashEscapeSequence(next)),
			};
			if let Some(escaped_char) = escaped_char {
				literal_string.push(escaped_char);
			}
		} else {
			literal_string.push(ch);
		}
	}
	let literal_length = literal_length.ok_or(FendError::UnterminatedStringLiteral)?;
	let (_, remaining) = input.split_at(literal_length + 1);
	Ok((Token::StringLiteral(literal_string.into()), remaining))
}

// parses a unit beginning with ' or "
fn parse_quote_unit(input: &str) -> (Token, &str) {
	let mut split_idx = 1;
	if let Some(ch) = input.split_at(1).1.chars().next() {
		if ch.is_alphabetic() {
			split_idx += ch.len_utf8();
			let mut prev = ch;
			let (_, mut remaining) = input.split_at(split_idx);
			while let Some(next) = remaining.chars().next() {
				if !is_valid_in_ident(next, Some(prev)) {
					break;
				}
				split_idx += next.len_utf8();
				prev = next;
				let (_, remaining2) = input.split_at(split_idx);
				remaining = remaining2;
			}
		}
	}
	let (a, b) = input.split_at(split_idx);
	(Token::Ident(Ident::new_string(a.to_string())), b)
}

pub(crate) struct Lexer<'a, 'b, I: Interrupt> {
	input: &'a str,
	// normally 0; 1 after backslash; 2 after ident after backslash
	after_backslash_state: u8,
	after_number_or_to: bool,
	decimal_separator: DecimalSeparatorStyle,
	int: &'b I,
}

fn skip_whitespace_and_comments(input: &mut &str) {
	while !input.is_empty() {
		if input.starts_with("# ") || input.starts_with("#!") {
			if let Some(idx) = input.find('\n') {
				let (_, remaining) = input.split_at(idx);
				*input = remaining;
				continue;
			}
			*input = "";
			return;
		} else if let Some(ch) = input.chars().next() {
			if ch.is_whitespace() {
				let (_, remaining) = input.split_at(ch.len_utf8());
				*input = remaining;
				continue;
			}
		}
		break;
	}
}

fn parse_date(input: &str) -> FResult<(Date, &str)> {
	let (_, input) = input.split_at(1); // skip '@' symbol
	let mut input2 = input;
	let mut split_idx = 0;
	for i in 0..3 {
		let mut n = 0;
		while matches!(input2.chars().next(), Some('0'..='9')) {
			let (_, remaining) = input2.split_at(1);
			input2 = remaining;
			n += 1;
			split_idx += 1;
		}
		if n == 0 {
			return Err(FendError::ExpectedADateLiteral);
		}
		if i == 2 {
			break;
		}
		if !input2.starts_with('-') {
			return Err(FendError::ExpectedADateLiteral);
		}
		let (_, remaining) = input2.split_at(1);
		input2 = remaining;
		split_idx += 1;
	}
	let (date_str, result_remaining) = input.split_at(split_idx);
	let res = Date::parse(date_str)?;
	Ok((res, result_remaining))
}

impl<'a, 'b, I: Interrupt> Lexer<'a, 'b, I> {
	fn next_token(&mut self) -> FResult<Option<Token>> {
		skip_whitespace_and_comments(&mut self.input);
		let (ch, following) = {
			let mut chars = self.input.chars();
			let ch = chars.next();
			let following = chars.next();
			(ch, following)
		};
		Ok(Some(match ch {
			Some(ch) => {
				if ch.is_ascii_digit()
					|| (ch == self.decimal_separator.decimal_separator()
						&& self.after_backslash_state == 0)
					|| (ch == 'd' && following.is_some() && following.unwrap().is_ascii_digit())
				{
					let (num, remaining) =
						parse_number(self.input, self.decimal_separator, self.int)?;
					self.input = remaining;
					Token::Num(num)
				} else if ch == '\'' || ch == '"' {
					if self.after_number_or_to {
						let (token, remaining) = parse_quote_unit(self.input);
						self.input = remaining;
						token
					} else {
						// normal string literal, with possible escape sequences
						let (token, remaining) = parse_string_literal(self.input, ch)?;
						self.input = remaining;
						token
					}
				} else if ch == '@' {
					// date literal, e.g. @1970-01-01
					let (date, remaining) = parse_date(self.input)?;
					self.input = remaining;
					Token::Date(date)
				} else if self.input.starts_with("#\"") {
					// raw string literal
					let (_, remaining) = self.input.split_at(2);
					let literal_length = remaining
						.match_indices("\"#")
						.next()
						.ok_or(FendError::UnterminatedStringLiteral)?
						.0;
					let (literal, remaining) = remaining.split_at(literal_length);
					let (_terminator, remaining) = remaining.split_at(2);
					self.input = remaining;
					Token::StringLiteral(literal.to_string().into())
				} else if is_valid_in_ident(ch, None) {
					// dots aren't allowed in idents after a backslash
					let (ident, remaining) =
						parse_ident(self.input, self.after_backslash_state != 1)?;
					self.input = remaining;
					ident
				} else {
					let (_, remaining) = self.input.split_at(ch.len_utf8());
					self.input = remaining;
					parse_symbol(ch, &mut self.input)?
				}
			}
			None => return Ok(None),
		}))
	}
}

impl<'a, I: Interrupt> Iterator for Lexer<'a, '_, I> {
	type Item = FResult<Token>;

	fn next(&mut self) -> Option<Self::Item> {
		let res = match self.next_token() {
			Err(e) => Some(Err(e)),
			Ok(None) => None,
			Ok(Some(t)) => Some(Ok(t)),
		};
		self.after_number_or_to = matches!(
			res,
			Some(Ok(Token::Num(_) | Token::Symbol(Symbol::UnitConversion)))
		);
		if matches!(res, Some(Ok(Token::Symbol(Symbol::Backslash)))) {
			self.after_backslash_state = 1;
		} else if self.after_backslash_state == 1 {
			if let Some(Ok(Token::Ident(_))) = res {
				self.after_backslash_state = 2;
			} else {
				self.after_backslash_state = 0;
			}
		} else {
			self.after_backslash_state = 0;
		}
		res
	}
}

pub(crate) fn lex<'a, 'b, I: Interrupt>(
	input: &'a str,
	ctx: &Context,
	int: &'b I,
) -> Lexer<'a, 'b, I> {
	Lexer {
		input,
		after_backslash_state: 0,
		after_number_or_to: false,
		decimal_separator: ctx.decimal_separator,
		int,
	}
}
