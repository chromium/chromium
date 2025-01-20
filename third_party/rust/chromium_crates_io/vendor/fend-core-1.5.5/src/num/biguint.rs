use crate::error::{FendError, Interrupt};
use crate::format::Format;
use crate::interrupt::test_int;
use crate::num::{out_of_range, Base, Exact, Range, RangeBound};
use crate::result::FResult;
use crate::serialize::{Deserialize, Serialize};
use std::cmp::{max, Ordering};
use std::{fmt, hash, io};

#[derive(Clone)]
pub(crate) enum BigUint {
	Small(u64),
	// little-endian, len >= 1
	Large(Vec<u64>),
}

impl hash::Hash for BigUint {
	fn hash<H: hash::Hasher>(&self, state: &mut H) {
		match self {
			Small(u) => u.hash(state),
			Large(v) => {
				for u in v {
					u.hash(state);
				}
			}
		}
	}
}

use BigUint::{Large, Small};

#[allow(clippy::cast_possible_truncation)]
const fn truncate(n: u128) -> u64 {
	n as u64
}

impl BigUint {
	fn bits(&self) -> u64 {
		match self {
			Small(n) => u64::from(n.ilog2()) + 1,
			Large(value) => {
				for (i, v) in value.iter().enumerate().rev() {
					if *v != 0 {
						let bits_in_v = u64::from(v.ilog2()) + 1;
						let bits_in_rest = u64::try_from(i).unwrap() * u64::from(u64::BITS);
						return bits_in_v + bits_in_rest;
					}
				}
				0
			}
		}
	}

	fn is_zero(&self) -> bool {
		match self {
			Small(n) => *n == 0,
			Large(value) => {
				for v in value {
					if *v != 0 {
						return false;
					}
				}
				true
			}
		}
	}

	fn get(&self, idx: usize) -> u64 {
		match self {
			Small(n) => {
				if idx == 0 {
					*n
				} else {
					0
				}
			}
			Large(value) => {
				if idx < value.len() {
					value[idx]
				} else {
					0
				}
			}
		}
	}

	pub(crate) fn try_as_usize<I: Interrupt>(&self, int: &I) -> FResult<usize> {
		let error = || -> FResult<_> {
			Ok(out_of_range(
				self.fm(int)?,
				Range {
					start: RangeBound::Closed(0),
					end: RangeBound::Closed(usize::MAX),
				},
			))
		};

		Ok(match self {
			Small(n) => {
				if let Ok(res) = usize::try_from(*n) {
					res
				} else {
					return Err(error()?);
				}
			}
			Large(v) => {
				// todo use correct method to get actual length excluding leading zeroes
				if v.len() == 1 {
					if let Ok(res) = usize::try_from(v[0]) {
						res
					} else {
						return Err(error()?);
					}
				} else {
					return Err(error()?);
				}
			}
		})
	}

	#[allow(clippy::cast_precision_loss)]
	pub(crate) fn as_f64(&self) -> f64 {
		match self {
			Small(n) => *n as f64,
			Large(v) => {
				let mut res = 0.0;
				for &n in v.iter().rev() {
					res *= u64::MAX as f64;
					res += n as f64;
				}
				res
			}
		}
	}

	pub(crate) fn ilog2(&self) -> u64 {
		assert!(!self.is_zero());
		self.bits() - 1
	}

	#[allow(clippy::cast_precision_loss)]
	pub(crate) fn log2<I: Interrupt>(&self, int: &I) -> FResult<f64> {
		let int_log = self.ilog2() as f64;
		let msb_position = self.bits();
		let mut fractional_value = self.clone();

		let mut divisor = Self::from(1).lshift_n(&msb_position.into(), int)?;
		fractional_value.lshift(int)?;

		while fractional_value.bits() > 1023 || divisor.bits() > 1023 {
			fractional_value.rshift(int)?;
			divisor.rshift(int)?;
		}
		let fractional_log = (fractional_value.as_f64() / divisor.as_f64()).log2();

		Ok(int_log + fractional_log)
	}

	fn make_large(&mut self) {
		match self {
			Small(n) => {
				*self = Large(vec![*n]);
			}
			Large(_) => (),
		}
	}

	fn set(&mut self, idx: usize, new_value: u64) {
		match self {
			Small(n) => {
				if idx == 0 {
					*n = new_value;
				} else if new_value == 0 {
					// no need to do anything
				} else {
					self.make_large();
					self.set(idx, new_value);
				}
			}
			Large(value) => {
				while idx >= value.len() {
					value.push(0);
				}
				value[idx] = new_value;
			}
		}
	}

	fn value_len(&self) -> usize {
		match self {
			Small(_) => 1,
			Large(value) => value.len(),
		}
	}

	fn value_push(&mut self, new: u64) {
		if new == 0 {
			return;
		}
		self.make_large();
		match self {
			Small(_) => unreachable!(),
			Large(v) => v.push(new),
		}
	}

	pub(crate) fn gcd<I: Interrupt>(mut a: Self, mut b: Self, int: &I) -> FResult<Self> {
		while b >= 1.into() {
			let r = a.rem(&b, int)?;
			a = b;
			b = r;
		}

		Ok(a)
	}

	pub(crate) fn pow<I: Interrupt>(a: &Self, b: &Self, int: &I) -> FResult<Self> {
		if a.is_zero() && b.is_zero() {
			return Err(FendError::ZeroToThePowerOfZero);
		}
		if b.is_zero() {
			return Ok(Self::from(1));
		}
		if b.value_len() > 1 {
			return Err(FendError::ExponentTooLarge);
		}
		a.pow_internal(b.get(0), int)
	}

	// computes the exact `n`-th root if possible, otherwise the next lower integer
	pub(crate) fn root_n<I: Interrupt>(self, n: &Self, int: &I) -> FResult<Exact<Self>> {
		if self == 0.into() || self == 1.into() || n == &Self::from(1) {
			return Ok(Exact::new(self, true));
		}
		if n.value_len() > 1 {
			return Err(FendError::OutOfRange {
				value: Box::new(n.format(&FormatOptions::default(), int)?.value),
				range: Range {
					start: RangeBound::Closed(Box::new(1)),
					end: RangeBound::Closed(Box::new(u64::MAX)),
				},
			});
		}
		let max_bits = self.bits() / n.get(0) + 1;
		let mut low_guess = Self::from(1);
		let mut high_guess = Small(1).lshift_n(&(max_bits + 1).into(), int)?;
		let result = loop {
			test_int(int)?;
			let mut guess = low_guess.clone().add(&high_guess);
			guess.rshift(int)?;

			let res = Self::pow(&guess, n, int)?;
			match res.cmp(&self) {
				Ordering::Equal => break Exact::new(guess, true),
				Ordering::Greater => high_guess = guess,
				Ordering::Less => low_guess = guess,
			}
			if high_guess.clone().sub(&low_guess) <= 1.into() {
				break Exact::new(low_guess, false);
			}
		};
		Ok(result)
	}

	fn pow_internal<I: Interrupt>(&self, mut exponent: u64, int: &I) -> FResult<Self> {
		let mut result = Self::from(1);
		let mut base = self.clone();
		while exponent > 0 {
			test_int(int)?;
			if exponent % 2 == 1 {
				result = result.mul(&base, int)?;
			}
			exponent >>= 1;
			base = base.clone().mul(&base, int)?;
		}
		Ok(result)
	}

	fn lshift<I: Interrupt>(&mut self, int: &I) -> FResult<()> {
		match self {
			Small(n) => {
				if *n & 0xc000_0000_0000_0000 == 0 {
					*n <<= 1;
				} else {
					*self = Large(vec![*n << 1, *n >> 63]);
				}
			}
			Large(value) => {
				if value[value.len() - 1] & (1_u64 << 63) != 0 {
					value.push(0);
				}
				for i in (0..value.len()).rev() {
					test_int(int)?;
					value[i] <<= 1;
					if i != 0 {
						value[i] |= value[i - 1] >> 63;
					}
				}
			}
		}
		Ok(())
	}

	fn rshift<I: Interrupt>(&mut self, int: &I) -> FResult<()> {
		match self {
			Small(n) => *n >>= 1,
			Large(value) => {
				for i in 0..value.len() {
					test_int(int)?;
					value[i] >>= 1;
					let next = if i + 1 >= value.len() {
						0
					} else {
						value[i + 1]
					};
					value[i] |= next << 63;
				}
			}
		}
		Ok(())
	}

	pub(crate) fn divmod<I: Interrupt>(&self, other: &Self, int: &I) -> FResult<(Self, Self)> {
		if let (Small(a), Small(b)) = (self, other) {
			if let (Some(div_res), Some(mod_res)) = (a.checked_div(*b), a.checked_rem(*b)) {
				return Ok((Small(div_res), Small(mod_res)));
			}
			return Err(FendError::DivideByZero);
		}
		if other.is_zero() {
			return Err(FendError::DivideByZero);
		}
		if other == &Self::from(1) {
			return Ok((self.clone(), Self::from(0)));
		}
		if self.is_zero() {
			return Ok((Self::from(0), Self::from(0)));
		}
		if self < other {
			return Ok((Self::from(0), self.clone()));
		}
		if self == other {
			return Ok((Self::from(1), Self::from(0)));
		}
		if other == &Self::from(2) {
			let mut div_result = self.clone();
			div_result.rshift(int)?;
			let modulo = self.get(0) & 1;
			return Ok((div_result, Self::from(modulo)));
		}
		// binary long division
		let mut q = Self::from(0);
		let mut r = Self::from(0);
		for i in (0..self.value_len()).rev() {
			test_int(int)?;
			for j in (0..64).rev() {
				r.lshift(int)?;
				let bit_of_self = u64::from((self.get(i) & (1 << j)) != 0);
				r.set(0, r.get(0) | bit_of_self);
				if &r >= other {
					r = r.sub(other);
					q.set(i, q.get(i) | (1 << j));
				}
			}
		}
		Ok((q, r))
	}

	/// computes self *= other
	fn mul_internal<I: Interrupt>(&mut self, other: &Self, int: &I) -> FResult<()> {
		if self.is_zero() || other.is_zero() {
			*self = Self::from(0);
			return Ok(());
		}
		let self_clone = self.clone();
		self.make_large();
		match self {
			Small(_) => unreachable!(),
			Large(v) => {
				v.clear();
				v.push(0);
			}
		}
		for i in 0..other.value_len() {
			test_int(int)?;
			self.add_assign_internal(&self_clone, other.get(i), i);
		}
		Ok(())
	}

	/// computes `self += (other * mul_digit) << (64 * shift)`
	fn add_assign_internal(&mut self, other: &Self, mul_digit: u64, shift: usize) {
		let mut carry = 0;
		for i in 0..max(self.value_len(), other.value_len() + shift) {
			let a = self.get(i);
			let b = if i >= shift { other.get(i - shift) } else { 0 };
			let sum = u128::from(a) + (u128::from(b) * u128::from(mul_digit)) + u128::from(carry);
			self.set(i, truncate(sum));
			carry = truncate(sum >> 64);
		}
		if carry != 0 {
			self.value_push(carry);
		}
	}

	// Note: 0! = 1, 1! = 1
	pub(crate) fn factorial<I: Interrupt>(mut self, int: &I) -> FResult<Self> {
		let mut res = Self::from(1);
		while self > 1.into() {
			test_int(int)?;
			res = res.mul(&self, int)?;
			self = self.sub(&1.into());
		}
		Ok(res)
	}

	pub(crate) fn fibonacci<I: Interrupt>(mut n: usize, int: &I) -> FResult<Self> {
		if n == 0 {
			return Ok(0.into());
		}
		if n == 1 {
			return Ok(1.into());
		}
		let mut a: Self = 0.into();
		let mut b: Self = 1.into();
		while n > 1 {
			test_int(int)?;
			(b, a) = (a.add(&b), b);
			n -= 1;
		}
		Ok(b)
	}

	pub(crate) fn mul<I: Interrupt>(mut self, other: &Self, int: &I) -> FResult<Self> {
		if let (Small(a), Small(b)) = (&self, &other) {
			if let Some(res) = a.checked_mul(*b) {
				return Ok(Self::from(res));
			}
		}
		self.mul_internal(other, int)?;
		Ok(self)
	}

	fn rem<I: Interrupt>(&self, other: &Self, int: &I) -> FResult<Self> {
		Ok(self.divmod(other, int)?.1)
	}

	pub(crate) fn is_even<I: Interrupt>(&self, int: &I) -> FResult<bool> {
		Ok(self.divmod(&Self::from(2), int)?.1 == 0.into())
	}

	pub(crate) fn div<I: Interrupt>(self, other: &Self, int: &I) -> FResult<Self> {
		Ok(self.divmod(other, int)?.0)
	}

	pub(crate) fn add(mut self, other: &Self) -> Self {
		self.add_assign_internal(other, 1, 0);
		self
	}

	pub(crate) fn sub(self, other: &Self) -> Self {
		if let (Small(a), Small(b)) = (&self, &other) {
			return Self::from(a - b);
		}
		match self.cmp(other) {
			Ordering::Equal => return Self::from(0),
			Ordering::Less => unreachable!("number would be less than 0"),
			Ordering::Greater => (),
		};
		if other.is_zero() {
			return self;
		}
		let mut carry = 0; // 0 or 1
		let mut res = match self {
			Large(x) => x,
			Small(v) => vec![v],
		};
		if res.len() < other.value_len() {
			res.resize(other.value_len(), 0);
		}
		for (i, a) in res.iter_mut().enumerate() {
			let b = other.get(i);
			if !(b == u64::MAX && carry == 1) && *a >= b + carry {
				*a = *a - b - carry;
				carry = 0;
			} else {
				let next_digit =
					u128::from(*a) + ((1_u128) << 64) - u128::from(b) - u128::from(carry);
				*a = truncate(next_digit);
				carry = 1;
			}
		}
		assert_eq!(carry, 0);
		Large(res)
	}

	pub(crate) const fn is_definitely_zero(&self) -> bool {
		match self {
			Small(x) => *x == 0,
			Large(_) => false,
		}
	}

	pub(crate) const fn is_definitely_one(&self) -> bool {
		match self {
			Small(x) => *x == 1,
			Large(_) => false,
		}
	}

	pub(crate) fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
		match self {
			Small(x) => {
				1u8.serialize(write)?;
				x.serialize(write)?;
			}
			Large(v) => {
				2u8.serialize(write)?;
				v.len().serialize(write)?;
				for b in v {
					b.serialize(write)?;
				}
			}
		}
		Ok(())
	}

	pub(crate) fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		let kind = u8::deserialize(read)?;
		Ok(match kind {
			1 => Self::Small(u64::deserialize(read)?),
			2 => {
				let len = usize::deserialize(read)?;
				let mut v = Vec::with_capacity(len);
				for _ in 0..len {
					v.push(u64::deserialize(read)?);
				}
				Self::Large(v)
			}
			_ => return Err(FendError::DeserializationError),
		})
	}

	pub(crate) fn bitwise_and(self, rhs: &Self) -> Self {
		match (self, rhs) {
			(Small(a), Small(b)) => Small(a & *b),
			(Large(a), Small(b)) => Small(a[0] & *b),
			(Small(a), Large(b)) => Small(a & b[0]),
			(Large(a), Large(b)) => {
				let mut result = b.clone();
				for (i, res_i) in result.iter_mut().enumerate() {
					*res_i &= a.get(i).unwrap_or(&0);
				}
				Large(result)
			}
		}
	}

	pub(crate) fn bitwise_or(self, rhs: &Self) -> Self {
		match (self, rhs) {
			(Small(a), Small(b)) => Small(a | *b),
			(Large(mut a), Small(b)) => {
				a[0] |= b;
				Large(a)
			}
			(Small(a), Large(b)) => {
				let mut result = b.clone();
				result[0] |= a;
				Large(result)
			}
			(Large(mut a), Large(b)) => {
				while a.len() < b.len() {
					a.push(0);
				}
				for i in 0..b.len() {
					a[i] |= b[i];
				}
				Large(a)
			}
		}
	}

	pub(crate) fn bitwise_xor(self, rhs: &Self) -> Self {
		match (self, rhs) {
			(Small(a), Small(b)) => Small(a ^ *b),
			(Large(mut a), Small(b)) => {
				a[0] ^= b;
				Large(a)
			}
			(Small(a), Large(b)) => {
				let mut result = b.clone();
				result[0] ^= a;
				Large(result)
			}
			(Large(mut a), Large(b)) => {
				while a.len() < b.len() {
					a.push(0);
				}
				for i in 0..b.len() {
					a[i] ^= b[i];
				}
				Large(a)
			}
		}
	}

	pub(crate) fn lshift_n<I: Interrupt>(mut self, rhs: &Self, int: &I) -> FResult<Self> {
		let mut rhs = rhs.try_as_usize(int)?;
		if rhs > 64 {
			self.make_large();
			match &mut self {
				Large(v) => {
					while rhs >= 64 {
						v.insert(0, 0);
						rhs -= 64;
					}
				}
				Small(_) => unreachable!(),
			}
		}
		for _ in 0..rhs {
			self.lshift(int)?;
		}
		Ok(self)
	}

	pub(crate) fn rshift_n<I: Interrupt>(mut self, rhs: &Self, int: &I) -> FResult<Self> {
		let rhs = rhs.try_as_usize(int)?;
		for _ in 0..rhs {
			if self.is_zero() {
				break;
			}
			self.rshift(int)?;
		}
		Ok(self)
	}

	pub(crate) fn to_words<I: Interrupt>(&self, int: &I) -> FResult<String> {
		// it would be nice to implement https://www.mrob.com/pub/math/largenum.html at some point
		let num = self
			.format(
				&FormatOptions {
					base: Base::from_plain_base(10)?,
					sf_limit: None,
					write_base_prefix: false,
				},
				int,
			)?
			.value
			.to_string();

		if num == "0" {
			return Ok("zero".to_string());
		}

		let mut result = String::new();
		let mut chunks = Vec::new();

		let mut i = num.len();
		while i > 0 {
			let start = if i >= 3 { i - 3 } else { 0 };
			chunks.push(&num[start..i]);
			i = start;
		}

		for (i, chunk) in chunks.iter().enumerate().rev() {
			let part = chunk.parse::<usize>().unwrap_or(0);
			if part != 0 {
				if !result.is_empty() {
					result.push(' ');
				}
				convert_below_1000(part, &mut result);
				if i > 0 {
					result.push(' ');
					result.push_str(SCALE_NUMBERS.get(i).ok_or_else(|| FendError::OutOfRange {
						value: Box::new(num.clone()),
						range: Range {
							start: RangeBound::Closed(Box::new("0")),
							end: RangeBound::Closed(Box::new("10^66 - 1")),
						},
					})?);
				}
			}
		}

		Ok(result.trim().to_string())
	}
}

const SMALL_NUMBERS: &[&str] = &[
	"zero",
	"one",
	"two",
	"three",
	"four",
	"five",
	"six",
	"seven",
	"eight",
	"nine",
	"ten",
	"eleven",
	"twelve",
	"thirteen",
	"fourteen",
	"fifteen",
	"sixteen",
	"seventeen",
	"eighteen",
	"nineteen",
];
const TENS: &[&str] = &[
	"", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety",
];
const SCALE_NUMBERS: &[&str] = &[
	"",
	"thousand",
	"million",
	"billion",
	"trillion",
	"quadrillion",
	"quintillion",
	"sextillion",
	"septillion",
	"octillion",
	"nonillion",
	"decillion",
	"undecillion",
	"duodecillion",
	"tredecillion",
	"quattuordecillion",
	"quindecillion",
	"sexdecillion",
	"septendecillion",
	"octodecillion",
	"novemdecillion",
	"vigintillion",
];

fn convert_below_1000(num: usize, result: &mut String) {
	if num >= 100 {
		result.push_str(SMALL_NUMBERS[num / 100]);
		result.push_str(" hundred");
		if num % 100 != 0 {
			result.push_str(" and ");
		}
	}

	let remainder = num % 100;

	if remainder < 20 && remainder > 0 {
		result.push_str(SMALL_NUMBERS[remainder]);
	} else if remainder >= 20 {
		result.push_str(TENS[remainder / 10]);
		if remainder % 10 != 0 {
			result.push('-');
			result.push_str(SMALL_NUMBERS[remainder % 10]);
		}
	}
}

impl Ord for BigUint {
	fn cmp(&self, other: &Self) -> Ordering {
		if let (Small(a), Small(b)) = (self, other) {
			return a.cmp(b);
		}
		let mut i = std::cmp::max(self.value_len(), other.value_len());
		while i != 0 {
			let v1 = self.get(i - 1);
			let v2 = other.get(i - 1);
			match v1.cmp(&v2) {
				Ordering::Less => return Ordering::Less,
				Ordering::Greater => return Ordering::Greater,
				Ordering::Equal => (),
			}
			i -= 1;
		}

		Ordering::Equal
	}
}

impl PartialOrd for BigUint {
	fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
		Some(self.cmp(other))
	}
}

impl PartialEq for BigUint {
	fn eq(&self, other: &Self) -> bool {
		self.cmp(other) == Ordering::Equal
	}
}

impl Eq for BigUint {}

impl From<u64> for BigUint {
	fn from(val: u64) -> Self {
		Small(val)
	}
}

impl fmt::Debug for BigUint {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		match self {
			Small(n) => write!(f, "{n}")?,
			Large(value) => {
				write!(f, "[")?;
				let mut first = true;
				for v in value.iter().rev() {
					if !first {
						write!(f, ", ")?;
					}
					write!(f, "{v}")?;
					first = false;
				}
				write!(f, "]")?;
			}
		}
		Ok(())
	}
}

#[derive(Default)]
pub(crate) struct FormatOptions {
	pub(crate) base: Base,
	pub(crate) write_base_prefix: bool,
	pub(crate) sf_limit: Option<usize>,
}

impl Format for BigUint {
	type Params = FormatOptions;
	type Out = FormattedBigUint;

	fn format<I: Interrupt>(&self, params: &Self::Params, int: &I) -> FResult<Exact<Self::Out>> {
		let base_prefix = if params.write_base_prefix {
			Some(params.base)
		} else {
			None
		};

		if self.is_zero() {
			return Ok(Exact::new(
				FormattedBigUint {
					base: base_prefix,
					ty: FormattedBigUintType::Zero,
				},
				true,
			));
		}

		let mut num = self.clone();
		Ok(
			if num.value_len() == 1 && params.base.base_as_u8() == 10 && params.sf_limit.is_none() {
				Exact::new(
					FormattedBigUint {
						base: base_prefix,
						ty: FormattedBigUintType::Simple(num.get(0)),
					},
					true,
				)
			} else {
				let base_as_u128: u128 = params.base.base_as_u8().into();
				let mut divisor = base_as_u128;
				let mut rounds = 1;
				// note that the string is reversed: this is the number of trailing zeroes while
				// printing, but actually the number of leading zeroes in the final number
				let mut num_trailing_zeroes = 0;
				let mut num_leading_zeroes = 0;
				let mut finished_counting_leading_zeroes = false;
				while divisor
					< u128::MAX
						.checked_div(base_as_u128)
						.expect("base appears to be 0")
				{
					divisor *= base_as_u128;
					rounds += 1;
				}
				let divisor = Self::Large(vec![truncate(divisor), truncate(divisor >> 64)]);
				let mut output = String::with_capacity(rounds);
				while !num.is_zero() {
					test_int(int)?;
					let divmod_res = num.divmod(&divisor, int)?;
					let mut digit_group_value =
						u128::from(divmod_res.1.get(1)) << 64 | u128::from(divmod_res.1.get(0));
					for _ in 0..rounds {
						let digit_value = digit_group_value % base_as_u128;
						digit_group_value /= base_as_u128;
						let ch = Base::digit_as_char(truncate(digit_value)).unwrap();
						if ch == '0' {
							num_trailing_zeroes += 1;
						} else {
							for _ in 0..num_trailing_zeroes {
								output.push('0');
								if !finished_counting_leading_zeroes {
									num_leading_zeroes += 1;
								}
							}
							finished_counting_leading_zeroes = true;
							num_trailing_zeroes = 0;
							output.push(ch);
						}
					}
					num = divmod_res.0;
				}
				let exact = params
					.sf_limit
					.map_or(true, |sf| sf >= output.len() - num_leading_zeroes);
				Exact::new(
					FormattedBigUint {
						base: base_prefix,
						ty: FormattedBigUintType::Complex(output, params.sf_limit),
					},
					exact,
				)
			},
		)
	}
}

#[derive(Debug)]
enum FormattedBigUintType {
	Zero,
	Simple(u64),
	Complex(String, Option<usize>),
}

#[must_use]
#[derive(Debug)]
pub(crate) struct FormattedBigUint {
	base: Option<Base>,
	ty: FormattedBigUintType,
}

impl fmt::Display for FormattedBigUint {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> Result<(), fmt::Error> {
		if let Some(base) = self.base {
			base.write_prefix(f)?;
		}
		match &self.ty {
			FormattedBigUintType::Zero => write!(f, "0")?,
			FormattedBigUintType::Simple(i) => write!(f, "{i}")?,
			FormattedBigUintType::Complex(s, sf_limit) => {
				for (i, ch) in s.chars().rev().enumerate() {
					if sf_limit.is_some() && &Some(i) >= sf_limit {
						write!(f, "0")?;
					} else {
						write!(f, "{ch}")?;
					}
				}
			}
		}
		Ok(())
	}
}

impl FormattedBigUint {
	pub(crate) fn num_digits(&self) -> usize {
		match &self.ty {
			FormattedBigUintType::Zero => 1,
			FormattedBigUintType::Simple(i) => {
				if *i <= 9 {
					1
				} else {
					i.to_string().len()
				}
			}
			FormattedBigUintType::Complex(s, _) => s.len(),
		}
	}
}

#[cfg(test)]
mod tests {
	use super::BigUint;
	type Res = Result<(), crate::error::FendError>;

	#[test]
	fn test_sqrt() -> Res {
		let two = &BigUint::from(2);
		let int = crate::interrupt::Never;
		let test_sqrt_inner = |n, expected_root, exact| -> Res {
			let actual = BigUint::from(n).root_n(two, &int)?;
			assert_eq!(actual.value, BigUint::from(expected_root));
			assert_eq!(actual.exact, exact);
			Ok(())
		};
		test_sqrt_inner(0, 0, true)?;
		test_sqrt_inner(1, 1, true)?;
		test_sqrt_inner(2, 1, false)?;
		test_sqrt_inner(3, 1, false)?;
		test_sqrt_inner(4, 2, true)?;
		test_sqrt_inner(5, 2, false)?;
		test_sqrt_inner(6, 2, false)?;
		test_sqrt_inner(7, 2, false)?;
		test_sqrt_inner(8, 2, false)?;
		test_sqrt_inner(9, 3, true)?;
		test_sqrt_inner(10, 3, false)?;
		test_sqrt_inner(11, 3, false)?;
		test_sqrt_inner(12, 3, false)?;
		test_sqrt_inner(13, 3, false)?;
		test_sqrt_inner(14, 3, false)?;
		test_sqrt_inner(15, 3, false)?;
		test_sqrt_inner(16, 4, true)?;
		test_sqrt_inner(17, 4, false)?;
		test_sqrt_inner(18, 4, false)?;
		test_sqrt_inner(19, 4, false)?;
		test_sqrt_inner(20, 4, false)?;
		test_sqrt_inner(200_000, 447, false)?;
		test_sqrt_inner(1_740_123_984_719_364_372, 1_319_137_591, false)?;

		// sqrt(3_260_954_456_333_195_555 * 2^64)
		let val = BigUint::Large(vec![0, 3_260_954_456_333_195_555]).root_n(two, &int)?;
		assert_eq!(val.value, BigUint::from(7_755_900_482_342_532_476));
		assert!(!val.exact);
		Ok(())
	}

	#[test]
	fn test_cmp() {
		assert_eq!(BigUint::from(0), BigUint::from(0));
		assert!(BigUint::from(0) < BigUint::from(1));
		assert!(BigUint::from(100) > BigUint::from(1));
		assert!(BigUint::from(10_000_000) > BigUint::from(1));
		assert!(BigUint::from(10_000_000) > BigUint::from(9_999_999));
	}

	#[test]
	fn test_addition() {
		assert_eq!(BigUint::from(2).add(&BigUint::from(2)), BigUint::from(4));
		assert_eq!(BigUint::from(5).add(&BigUint::from(3)), BigUint::from(8));
		assert_eq!(
			BigUint::from(0).add(&BigUint::Large(vec![0, 9_223_372_036_854_775_808, 0])),
			BigUint::Large(vec![0, 9_223_372_036_854_775_808, 0])
		);
	}

	#[test]
	fn test_sub() {
		assert_eq!(BigUint::from(5).sub(&BigUint::from(3)), BigUint::from(2));
		assert_eq!(BigUint::from(0).sub(&BigUint::from(0)), BigUint::from(0));
	}

	#[test]
	fn test_multiplication() -> Res {
		let int = &crate::interrupt::Never;
		assert_eq!(
			BigUint::from(20).mul(&BigUint::from(3), int)?,
			BigUint::from(60)
		);
		Ok(())
	}

	#[test]
	fn test_small_division_by_two() -> Res {
		let int = &crate::interrupt::Never;
		let two = BigUint::from(2);
		assert_eq!(BigUint::from(0).div(&two, int)?, BigUint::from(0));
		assert_eq!(BigUint::from(1).div(&two, int)?, BigUint::from(0));
		assert_eq!(BigUint::from(2).div(&two, int)?, BigUint::from(1));
		assert_eq!(BigUint::from(3).div(&two, int)?, BigUint::from(1));
		assert_eq!(BigUint::from(4).div(&two, int)?, BigUint::from(2));
		assert_eq!(BigUint::from(5).div(&two, int)?, BigUint::from(2));
		assert_eq!(BigUint::from(6).div(&two, int)?, BigUint::from(3));
		assert_eq!(BigUint::from(7).div(&two, int)?, BigUint::from(3));
		assert_eq!(BigUint::from(8).div(&two, int)?, BigUint::from(4));
		Ok(())
	}

	#[test]
	fn test_rem() -> Res {
		let int = &crate::interrupt::Never;
		let three = BigUint::from(3);
		assert_eq!(BigUint::from(20).rem(&three, int)?, BigUint::from(2));
		assert_eq!(BigUint::from(21).rem(&three, int)?, BigUint::from(0));
		assert_eq!(BigUint::from(22).rem(&three, int)?, BigUint::from(1));
		assert_eq!(BigUint::from(23).rem(&three, int)?, BigUint::from(2));
		assert_eq!(BigUint::from(24).rem(&three, int)?, BigUint::from(0));
		Ok(())
	}

	#[test]
	fn test_lshift() -> Res {
		let int = &crate::interrupt::Never;
		let mut n = BigUint::from(1);
		for _ in 0..100 {
			n.lshift(int)?;
			assert_eq!(n.get(0) & 1, 0);
		}
		Ok(())
	}

	#[test]
	fn test_gcd() -> Res {
		let int = &crate::interrupt::Never;
		assert_eq!(BigUint::gcd(2.into(), 4.into(), int)?, 2.into());
		assert_eq!(BigUint::gcd(4.into(), 2.into(), int)?, 2.into());
		assert_eq!(BigUint::gcd(37.into(), 43.into(), int)?, 1.into());
		assert_eq!(BigUint::gcd(43.into(), 37.into(), int)?, 1.into());
		assert_eq!(BigUint::gcd(215.into(), 86.into(), int)?, 43.into());
		assert_eq!(BigUint::gcd(86.into(), 215.into(), int)?, 43.into());
		Ok(())
	}

	#[test]
	fn test_add_assign_internal() {
		// 0 += (1 * 1) << (64 * 1)
		let mut x = BigUint::from(0);
		x.add_assign_internal(&BigUint::from(1), 1, 1);
		assert_eq!(x, BigUint::Large(vec![0, 1]));
	}

	#[test]
	fn test_large_lshift() -> Res {
		let int = &crate::interrupt::Never;
		let mut a = BigUint::from(9_223_372_036_854_775_808);
		a.lshift(int)?;
		assert!(!a.is_zero());
		Ok(())
	}

	#[test]
	fn test_big_multiplication() -> Res {
		let int = &crate::interrupt::Never;
		assert_eq!(
			BigUint::from(1).mul(&BigUint::Large(vec![0, 1]), int)?,
			BigUint::Large(vec![0, 1])
		);
		Ok(())
	}

	#[test]
	fn words() -> Res {
		let int = &crate::interrupt::Never;
		assert_eq!(
			BigUint::from(123).to_words(int)?,
			"one hundred and twenty-three"
		);
		assert_eq!(
			BigUint::from(10_000_347_001_023_000_002).to_words(int)?,
			"ten quintillion three hundred and forty-seven trillion one billion twenty-three million two"
		);
		assert_eq!(
			BigUint::Large(vec![0, 0x0e3c_bb5a_c574_1c64, 0x1cfd_a3a5_6977_58bf, 0x097e_dd87]).to_words(int).unwrap_err().to_string(),
			"1000000000000000000000000000000000000000000000000000000000000000000 must lie in the interval [0, 10^66 - 1]"
		);
		Ok(())
	}
}
