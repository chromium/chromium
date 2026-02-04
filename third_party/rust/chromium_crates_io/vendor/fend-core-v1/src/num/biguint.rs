use crate::error::{FendError, Interrupt};
use crate::format::Format;
use crate::interrupt::test_int;
use crate::num::bigrat::sign::Sign;
use crate::num::{Base, Exact, Range, RangeBound, out_of_range};
use crate::result::FResult;
use crate::serialize::CborValue;
use std::cmp::{Ordering, max};
use std::{fmt, hash};

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

	fn reduce(&mut self) {
		let Large(v) = self else {
			return;
		};
		while v.len() > 1 && v[v.len() - 1] == 0 {
			v.pop();
		}
		if v.len() == 1 {
			*self = Small(v[0]);
		}
	}

	fn value_len(&self) -> usize {
		match self {
			Small(_) => 1,
			Large(value) => value.len(),
		}
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
		if self.is_zero() {
			return Ok(Exact::new(self, true));
		}
		if n.is_zero() {
			return Err(FendError::DivideByZero);
		}
		if n == &Self::from(1) {
			return Ok(Exact::new(self, true));
		}

		let Ok(n_usize) = n.try_as_usize(int) else {
			// n is huge (larger than usize::MAX).
			// root is 1 (unless self is 0 or 1, handled above).
			return Ok(Exact::new(Self::from(1), false));
		};

		let self_bits = self.bits();
		if self_bits < n_usize as u64 {
			if self == Self::from(1) {
				return Ok(Exact::new(Self::from(1), true));
			}
			return Ok(Exact::new(Self::from(1), false));
		}

		// Initial guess: 2 ^ ceil(bits / n)
		let guess_bits = self_bits.div_ceil(n_usize as u64);
		let mut x = Small(1).lshift_n(&guess_bits.into(), int)?;

		let n_minus_1 = Self::from((n_usize - 1) as u64);

		loop {
			test_int(int)?;

			// Newton iteration: x_new = ( (n-1)x + self/x^(n-1) ) / n

			// 1. Calculate x^(n-1)
			let x_pow_n_minus_1 = Self::pow(&x, &n_minus_1, int)?;

			// 2. Calculate self / x^(n-1)
			// We discard the remainder (the .1 tuple element).
			let quotient = self.divmod(&x_pow_n_minus_1, int)?.0;

			// 3. Calculate numerator: (n-1)x + quotient
			let term1 = x.clone().mul(&n_minus_1, int)?;
			let numerator = term1.add(&quotient);

			// 4. Calculate x_new: numerator / n
			let x_new = numerator.div(n, int)?;

			// 5. Check convergence
			// For integer Newton method starting from above, we stop when x_new >= x
			if x_new >= x {
				let check_pow = Self::pow(&x, n, int)?;
				return Ok(Exact::new(x, check_pow == self));
			}

			x = x_new;
		}
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
		// Check for interrupts once per call, not per limb.
		test_int(int)?;

		match self {
			Small(n) => {
				if *n & 0xc000_0000_0000_0000 == 0 {
					*n <<= 1;
				} else {
					*self = Large(vec![*n << 1, *n >> 63]);
				}
			}
			Large(value) => {
				// Use a forward pass.
				// This is cache-friendly and allows the compiler to vectorise.
				let mut carry = 0;
				for elem in value.iter_mut() {
					let new_carry = *elem >> 63;
					*elem = (*elem << 1) | carry;
					carry = new_carry;
				}
				if carry != 0 {
					value.push(carry);
				}
			}
		}
		Ok(())
	}

	fn rshift<I: Interrupt>(&mut self, int: &I) -> FResult<()> {
		// Check for interrupts once per call, not per limb.
		test_int(int)?;

		match self {
			Small(n) => *n >>= 1,
			Large(value) => {
				let len = value.len();
				if len > 0 {
					// Forward pass: value[i] takes the high bit of value[i+1]
					for i in 0..len - 1 {
						value[i] = (value[i] >> 1) | (value[i + 1] << 63);
					}
					// Handle the most significant limb
					value[len - 1] >>= 1;
				}
			}
		}
		Ok(())
	}

	pub(crate) fn divmod<I: Interrupt>(&self, other: &Self, int: &I) -> FResult<(Self, Self)> {
		if other.is_zero() {
			return Err(FendError::DivideByZero);
		}
		if self.is_zero() {
			return Ok((0.into(), 0.into()));
		}
		if self < other {
			return Ok((0.into(), self.clone()));
		}
		if other == &Self::from(1) {
			return Ok((self.clone(), 0.into()));
		}

		if let (Small(a), Small(b)) = (self, other) {
			if let (Some(div_res), Some(mod_res)) = (a.checked_div(*b), a.checked_rem(*b)) {
				return Ok((Small(div_res), Small(mod_res)));
			}
			return Err(FendError::DivideByZero);
		}

		self.div_rem_knuth(other, int)
	}

	fn trailing_zeros(&self) -> u64 {
		match self {
			Small(n) => u64::from(n.trailing_zeros()),
			Large(v) => {
				let mut count = 0;
				for &digit in v {
					if digit == 0 {
						count += 64;
					} else {
						count += u64::from(digit.trailing_zeros());
						break;
					}
				}
				count
			}
		}
	}

	fn rshift_u64(&mut self, n: u64) {
		if n == 0 {
			return;
		}
		match self {
			Small(v) => {
				if n >= 64 {
					*v = 0;
				} else {
					*v >>= n;
				}
			}
			Large(v) => {
				let shift_limbs = (n / 64) as usize;
				let shift_bits = (n % 64) as u32;

				if shift_limbs >= v.len() {
					*self = Small(0);
					return;
				}

				// Shift limbs (whole words) - remove LSBs
				if shift_limbs > 0 {
					v.drain(0..shift_limbs);
				}

				// Shift bits within limbs
				if shift_bits > 0 {
					let len = v.len();
					let inv_shift = 64 - shift_bits;
					for i in 0..len - 1 {
						v[i] = (v[i] >> shift_bits) | (v[i + 1] << inv_shift);
					}
					v[len - 1] >>= shift_bits;
				}

				// Clean up
				while v.last() == Some(&0) {
					v.pop();
				}
				if v.is_empty() {
					*self = Small(0);
				}
			}
		}
	}

	fn lshift_u64(&mut self, n: u64) {
		if n == 0 {
			return;
		}
		match self {
			Small(v) => {
				// If it fits in Small, stay Small
				if n < 64 && u64::from(v.leading_zeros()) >= n {
					*v <<= n;
				} else {
					// promote to Large
					let mut large = Large(vec![*v]);
					large.lshift_u64(n);
					*self = large;
				}
			}
			Large(v) => {
				let shift_limbs = (n / 64) as usize;
				let shift_bits = (n % 64) as u32;

				// 1. Shift bits
				if shift_bits > 0 {
					let mut carry = 0;
					let inv_shift = 64 - shift_bits;
					for x in v.iter_mut() {
						let next_carry = *x >> inv_shift;
						*x = (*x << shift_bits) | carry;
						carry = next_carry;
					}
					if carry != 0 {
						v.push(carry);
					}
				}

				// 2. Prepend zero limbs (LSBs)
				if shift_limbs > 0 {
					let mut new_vec = vec![0; shift_limbs];
					new_vec.append(v);
					*v = new_vec;
				}
			}
		}
	}

	// Stein's Algorithm (Binary GCD)
	pub(crate) fn gcd<I: Interrupt>(mut self, mut other: Self, int: &I) -> FResult<Self> {
		// 1. Handle base cases
		if self.is_zero() {
			return Ok(other);
		}
		if other.is_zero() {
			return Ok(self);
		}

		// 2. Find common power of 2
		let u_zeros = self.trailing_zeros();
		let v_zeros = other.trailing_zeros();
		let common_shift = std::cmp::min(u_zeros, v_zeros);

		// 3. Remove factors of 2 from u and v
		self.rshift_u64(u_zeros);
		other.rshift_u64(v_zeros);

		// 4. Main Loop
		loop {
			// Invariant: self and other are both odd at the start of loop body
			test_int(int)?;

			match self.cmp(&other) {
				Ordering::Equal => break,
				Ordering::Greater => {
					self.sub_assign(&other);
					let zeros = self.trailing_zeros();
					self.rshift_u64(zeros);
				}
				Ordering::Less => {
					std::mem::swap(&mut self, &mut other);
					self.sub_assign(&other);
					let zeros = self.trailing_zeros();
					self.rshift_u64(zeros);
				}
			}
		}

		// 5. Restore common power of 2
		self.lshift_u64(common_shift);
		Ok(self)
	}

	// Knuth's Algorithm D (The Art of Computer Programming, Vol 2, 4.3.1)
	#[allow(clippy::cast_possible_truncation, clippy::too_many_lines)]
	fn div_rem_knuth<I: Interrupt>(&self, other: &Self, int: &I) -> FResult<(Self, Self)> {
		let mut dividend_digits = match self {
			Small(n) => vec![*n],
			Large(v) => v.clone(),
		};
		let mut divisor_digits = match other {
			Small(n) => vec![*n],
			Large(v) => v.clone(),
		};

		// Sanitize inputs by removing leading zero limbs (trailing in LE vector).
		while dividend_digits.len() > 1 && dividend_digits.last() == Some(&0) {
			dividend_digits.pop();
		}
		while divisor_digits.len() > 1 && divisor_digits.last() == Some(&0) {
			divisor_digits.pop();
		}

		let divisor_len = divisor_digits.len();
		// shift = number of leading zeros in the most significant digit of the divisor
		let shift = divisor_digits.last().unwrap().leading_zeros();

		// Normalize divisor (shift left so MSB is 1)
		if shift > 0 {
			let mut carry = 0;
			for digit in &mut divisor_digits {
				let next_carry = *digit >> (64 - shift);
				*digit = (*digit << shift) | carry;
				carry = next_carry;
			}
		}

		// Normalize dividend (shift left by same amount)
		if shift > 0 {
			let mut carry = 0;
			for digit in &mut dividend_digits {
				let next_carry = *digit >> (64 - shift);
				*digit = (*digit << shift) | carry;
				carry = next_carry;
			}
			if carry != 0 {
				dividend_digits.push(carry);
			}
		}

		// Ensure dividend has a leading zero digit for the algorithm to work comfortably
		dividend_digits.push(0);

		let delta_len = dividend_digits.len() - divisor_len - 1;
		let mut quotient_digits = vec![0; delta_len + 1];
		let divisor_msb = divisor_digits[divisor_len - 1];

		// Iterate 'j' from m down to 0 -> offset
		for offset in (0..=delta_len).rev() {
			test_int(int)?;

			let dividend_high = dividend_digits[offset + divisor_len];
			let dividend_low = dividend_digits[offset + divisor_len - 1];

			// Estimate q_hat (quotient digit)
			let mut quotient_estimate = if dividend_high == divisor_msb {
				u64::MAX
			} else {
				let numerator = (u128::from(dividend_high) << 64) | u128::from(dividend_low);
				(numerator / u128::from(divisor_msb)) as u64
			};

			// Refine q_hat
			let divisor_second = if divisor_len >= 2 {
				divisor_digits[divisor_len - 2]
			} else {
				0
			};
			let dividend_third = if offset + divisor_len >= 2 {
				dividend_digits[offset + divisor_len - 2]
			} else {
				0
			};

			let mut remainder = ((u128::from(dividend_high) << 64) | u128::from(dividend_low))
				.wrapping_sub(u128::from(quotient_estimate) * u128::from(divisor_msb));

			while u128::from(divisor_second) * u128::from(quotient_estimate)
				> (remainder << 64) | u128::from(dividend_third)
			{
				quotient_estimate -= 1;
				remainder = remainder.wrapping_add(u128::from(divisor_msb));
				if remainder > u128::from(u64::MAX) {
					break;
				}
			}

			// Multiply and subtract: dividend[offset..] -= quotient_estimate * divisor
			let mut borrow: u128 = 0;
			for (i, &divisor_digit) in divisor_digits.iter().enumerate() {
				let product = u128::from(divisor_digit) * u128::from(quotient_estimate) + borrow;
				let sub_val = product as u64;
				borrow = product >> 64;

				let (res, borrowed) = dividend_digits[offset + i].overflowing_sub(sub_val);
				dividend_digits[offset + i] = res;
				if borrowed {
					borrow += 1;
				}
			}
			// Propagate borrow to the top digit
			let (res, borrowed) =
				dividend_digits[offset + divisor_len].overflowing_sub(borrow as u64);
			dividend_digits[offset + divisor_len] = res;

			// If we subtracted too much (borrowed from top), correct by adding back divisor
			if borrowed {
				quotient_estimate -= 1;
				let mut carry: u128 = 0;
				for (i, &divisor_digit) in divisor_digits.iter().enumerate() {
					let sum =
						u128::from(dividend_digits[offset + i]) + u128::from(divisor_digit) + carry;
					dividend_digits[offset + i] = sum as u64;
					carry = sum >> 64;
				}
				dividend_digits[offset + divisor_len] =
					dividend_digits[offset + divisor_len].wrapping_add(carry as u64);
			}

			quotient_digits[offset] = quotient_estimate;
		}

		// Denormalize remainder (shift right)
		if shift > 0 {
			let mut next_high_bits = 0;
			// Iterate from MSB to LSB
			for digit in dividend_digits.iter_mut().rev() {
				let val = *digit;
				*digit = (val >> shift) | next_high_bits;
				next_high_bits = val << (64 - shift);
			}
		}

		let mut final_quotient = Self::Large(quotient_digits);
		final_quotient.reduce();
		let mut final_remainder = Self::Large(dividend_digits);
		final_remainder.reduce();

		Ok((final_quotient, final_remainder))
	}

	/// computes `self += (other * mul_digit) << (64 * shift)`
	#[allow(clippy::cast_possible_truncation)]
	fn add_assign_internal(&mut self, other: &Self, mul_digit: u64, shift: usize) {
		if mul_digit == 0 {
			return;
		}

		self.make_large();
		let self_vec = match self {
			Large(v) => v,
			Small(_) => unreachable!(),
		};

		let other_slice = match other {
			Large(v) => v.as_slice(),
			Small(v) => std::slice::from_ref(v),
		};

		let min_len = shift + other_slice.len();
		if self_vec.len() < min_len {
			self_vec.resize(min_len, 0);
		}

		let mut carry: u128 = 0;
		let mul_digit = u128::from(mul_digit);

		// Use zip to iterate specified slices.
		let target_slice = &mut self_vec[shift..];
		for (target, &source) in target_slice.iter_mut().zip(other_slice) {
			let a = u128::from(*target);
			let b = u128::from(source);

			let sum = a + (b * mul_digit) + carry;
			*target = sum as u64;
			carry = sum >> 64;
		}

		// Propagate carry
		let mut i = min_len;
		while carry != 0 {
			if i < self_vec.len() {
				let sum = u128::from(self_vec[i]) + carry;
				self_vec[i] = sum as u64;
				carry = sum >> 64;
				i += 1;
			} else {
				self_vec.push(carry as u64);
				break;
			}
		}
	}

	/// computes `self += other << (64 * shift)`
	#[allow(clippy::cast_possible_truncation)]
	fn add_assign_shifted(&mut self, other: &Self, shift: usize) {
		self.make_large();
		let self_vec = match self {
			Large(v) => v,
			Small(_) => unreachable!(),
		};

		let other_slice = match other {
			Large(v) => v.as_slice(),
			Small(v) => std::slice::from_ref(v),
		};

		let min_len = shift + other_slice.len();
		if self_vec.len() < min_len {
			self_vec.resize(min_len, 0);
		}

		let mut carry: u128 = 0;
		let target_slice = &mut self_vec[shift..];

		for (target, &source) in target_slice.iter_mut().zip(other_slice) {
			let a = u128::from(*target);
			let b = u128::from(source);

			let sum = a + b + carry;
			*target = sum as u64;
			carry = sum >> 64;
		}

		let mut i = min_len;
		while carry != 0 {
			if i < self_vec.len() {
				let sum = u128::from(self_vec[i]) + carry;
				self_vec[i] = sum as u64;
				carry = sum >> 64;
				i += 1;
			} else {
				self_vec.push(carry as u64);
				break;
			}
		}
	}

	#[allow(clippy::cast_possible_truncation)]
	fn mul_internal_slice(a: &[u64], b: &[u64]) -> Self {
		if a.is_empty() || b.is_empty() {
			return Self::from(0);
		}
		// Pre-allocate result
		let mut res = vec![0; a.len() + b.len()];

		for (i, &digit) in b.iter().enumerate() {
			if digit == 0 {
				continue;
			}
			let mut carry: u128 = 0;
			let mul_digit = u128::from(digit);

			// Optimized inner loop on slices
			let target = &mut res[i..];
			for (dst, &src) in target.iter_mut().zip(a) {
				let sum = u128::from(*dst) + (u128::from(src) * mul_digit) + carry;
				*dst = sum as u64;
				carry = sum >> 64;
			}

			// Propagate carry
			let mut k = i + a.len();
			while carry > 0 {
				let sum = u128::from(res[k]) + carry;
				res[k] = sum as u64;
				carry = sum >> 64;
				k += 1;
			}
		}

		let mut big = Large(res);
		big.reduce();
		big
	}

	#[allow(clippy::cast_possible_truncation)]
	// Helper to add two slices and return BigUint
	fn add_slices(a: &[u64], b: &[u64]) -> Self {
		let max_len = max(a.len(), b.len());
		let mut res = vec![0; max_len];
		let mut carry: u128 = 0;

		for i in 0..max_len {
			let va = if i < a.len() { u128::from(a[i]) } else { 0 };
			let vb = if i < b.len() { u128::from(b[i]) } else { 0 };
			let sum = va + vb + carry;
			res[i] = sum as u64;
			carry = sum >> 64;
		}
		if carry > 0 {
			res.push(carry as u64);
		}
		Large(res)
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

	#[allow(clippy::cast_possible_truncation)]
	pub(crate) fn sub(self, other: &Self) -> Self {
		if let (Small(a), Small(b)) = (&self, &other) {
			return Self::from(a - b);
		}
		match self.cmp(other) {
			Ordering::Equal => return Self::from(0),
			Ordering::Less => unreachable!("number would be less than 0"),
			Ordering::Greater => (),
		}
		if other.is_zero() {
			return self;
		}

		let mut res = match self {
			Large(x) => x,
			Small(v) => vec![v],
		};

		let other_slice = match other {
			Large(v) => v.as_slice(),
			Small(v) => std::slice::from_ref(v),
		};

		let mut borrow: u128 = 0;
		let mut i = 0;

		let len = std::cmp::min(res.len(), other_slice.len());

		while i < len {
			let a = res[i];
			let b = other_slice[i];

			let sub_val = u128::from(b) + borrow;
			let (val, new_borrow) = u128::from(a).overflowing_sub(sub_val);

			res[i] = val as u64;
			borrow = u128::from(new_borrow);
			i += 1;
		}

		while borrow > 0 && i < res.len() {
			let a = res[i];
			let (val, new_borrow) = a.overflowing_sub(borrow as u64);
			res[i] = val;
			borrow = u128::from(new_borrow);
			i += 1;
		}

		let mut result = Large(res);
		result.reduce();
		result
	}

	#[allow(clippy::cast_possible_truncation)]
	fn sub_assign(&mut self, other: &Self) {
		let other_slice = match other {
			Large(v) => v.as_slice(),
			Small(s) => std::slice::from_ref(s),
		};

		if other_slice.is_empty() {
			return;
		}

		self.make_large();
		let self_vec = match self {
			Large(v) => v,
			Small(_) => unreachable!(),
		};

		let mut borrow: u128 = 0;
		let len = std::cmp::min(self_vec.len(), other_slice.len());

		for i in 0..len {
			let val = u128::from(self_vec[i]);
			let sub = u128::from(other_slice[i]) + borrow;
			let (res, b) = val.overflowing_sub(sub);
			self_vec[i] = res as u64;
			borrow = u128::from(b);
		}

		let mut i = len;
		while borrow > 0 && i < self_vec.len() {
			let val = u128::from(self_vec[i]);
			let (res, b) = val.overflowing_sub(borrow);
			self_vec[i] = res as u64;
			borrow = u128::from(b);
			i += 1;
		}
	}

	pub(crate) fn mul<I: Interrupt>(self, other: &Self, int: &I) -> FResult<Self> {
		if self.is_zero() || other.is_zero() {
			return Ok(Self::from(0));
		}

		let a_slice = match &self {
			Large(v) => v.as_slice(),
			Small(s) => std::slice::from_ref(s),
		};
		let b_slice = match other {
			Large(v) => v.as_slice(),
			Small(s) => std::slice::from_ref(s),
		};

		// 64 limbs (4096 bits) is the optimal cut-off.
		if a_slice.len() >= 64 && b_slice.len() >= 64 {
			return Self::mul_karatsuba_slice(a_slice, b_slice, int);
		}

		Ok(Self::mul_internal_slice(a_slice, b_slice))
	}

	fn mul_karatsuba_slice<I: Interrupt>(a: &[u64], b: &[u64], int: &I) -> FResult<Self> {
		test_int(int)?;

		// Base case: 64 limbs
		if a.len() < 64 || b.len() < 64 {
			return Ok(Self::mul_internal_slice(a, b));
		}

		let m = max(a.len(), b.len()) / 2;

		let a_len = a.len();
		let a_low = if m < a_len { &a[..m] } else { a };
		let a_high = if m < a_len { &a[m..] } else { &[] };

		let b_len = b.len();
		let b_low = if m < b_len { &b[..m] } else { b };
		let b_high = if m < b_len { &b[m..] } else { &[] };

		let z0 = Self::mul_karatsuba_slice(a_low, b_low, int)?;
		let z2 = Self::mul_karatsuba_slice(a_high, b_high, int)?;

		let a_sum = Self::add_slices(a_low, a_high);
		let b_sum = Self::add_slices(b_low, b_high);

		let a_sum_slice = match &a_sum {
			Large(v) => v.as_slice(),
			Small(s) => std::slice::from_ref(s),
		};
		let b_sum_slice = match &b_sum {
			Large(v) => v.as_slice(),
			Small(s) => std::slice::from_ref(s),
		};

		let z1_raw = Self::mul_karatsuba_slice(a_sum_slice, b_sum_slice, int)?;

		// Use in-place subtraction
		let mut z1 = z1_raw;
		z1.sub_assign(&z2);
		z1.sub_assign(&z0);

		let mut res = z0;
		res.add_assign_shifted(&z1, m);
		res.add_assign_shifted(&z2, 2 * m);

		Ok(res)
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

	pub(crate) fn serialize(&self, mut sign: Sign) -> CborValue {
		let mut x = self.clone();
		x.reduce();
		if sign == Sign::Negative {
			if x.is_zero() {
				sign = Sign::Positive;
			} else {
				x = x.sub(&1.into());
			}
		}
		match x {
			Small(x) => match sign {
				Sign::Positive => CborValue::Uint(x),
				Sign::Negative => CborValue::Negative(x),
			},
			Large(v) => {
				let bytes: Vec<_> = v
					.iter()
					.rev()
					.flat_map(|u| u.to_be_bytes())
					.skip_while(|&b| b == 0)
					.collect();
				let tag = match sign {
					Sign::Negative => 3,
					Sign::Positive => 2,
				};
				CborValue::Tag(tag, Box::new(CborValue::Bytes(bytes)))
			}
		}
	}

	pub(crate) fn deserialize(value: CborValue) -> FResult<(Self, Sign)> {
		Ok(match value {
			CborValue::Uint(n) => (Self::Small(n), Sign::Positive),
			CborValue::Negative(n) => (Self::Small(n.checked_add(1).unwrap()), Sign::Negative),
			CborValue::Tag(tag @ (2 | 3), inner) => {
				let CborValue::Bytes(mut b) = *inner else {
					return Err(FendError::DeserializationError(
						"biguint with tag 2 or 3 does not contain bytes",
					));
				};
				b.reverse();
				while b.len() % 8 != 0 {
					b.push(0);
				}
				let (chunks, rem) = b.as_chunks::<8>();
				assert!(rem.is_empty());
				let mut v = Self::Large(chunks.iter().map(|&c| u64::from_le_bytes(c)).collect());
				let sign = match tag {
					2 => Sign::Positive,
					3 => {
						v = v.add(&1.into());
						Sign::Negative
					}
					_ => unreachable!(),
				};
				(v, sign)
			}
			_ => {
				return Err(FendError::DeserializationError(
					"biguint must have major type 0, 1 or 6",
				));
			}
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
			let start = i.saturating_sub(3);
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
		if !num.is_multiple_of(100) {
			result.push_str(" and ");
		}
	}

	let remainder = num % 100;

	if remainder < 20 && remainder > 0 {
		result.push_str(SMALL_NUMBERS[remainder]);
	} else if remainder >= 20 {
		result.push_str(TENS[remainder / 10]);
		if !remainder.is_multiple_of(10) {
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
						(u128::from(divmod_res.1.get(1)) << 64) | u128::from(divmod_res.1.get(0));
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
					.is_none_or(|sf| sf >= output.len() - num_leading_zeroes);
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
	use std::io;

	use crate::{
		format::Format,
		interrupt::Never,
		num::biguint::FormatOptions,
		serialize::{CborValue, Deserialize},
	};

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
			BigUint::Large(vec![
				0,
				0x0e3c_bb5a_c574_1c64,
				0x1cfd_a3a5_6977_58bf,
				0x097e_dd87
			])
			.to_words(int)
			.unwrap_err()
			.to_string(),
			"1000000000000000000000000000000000000000000000000000000000000000000 must lie in the interval [0, 10^66 - 1]"
		);
		Ok(())
	}

	#[test]
	fn serialisation() {
		let bytes = b"\xc2\x49\x01\x00\x00\x00\x00\x00\x00\x00\x00";
		let dec =
			BigUint::deserialize(CborValue::deserialize(&mut io::Cursor::new(bytes)).unwrap())
				.unwrap()
				.0
				.format(&FormatOptions::default(), &Never)
				.unwrap()
				.value
				.to_string();
		assert_eq!(dec, "18446744073709551616");
	}
}
