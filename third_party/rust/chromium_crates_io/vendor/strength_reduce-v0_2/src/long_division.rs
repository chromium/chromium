extern crate core;

const U32_MAX: u64 = core::u32::MAX as u64;
const U64_MAX: u128 = core::u64::MAX as u128;

use ::StrengthReducedU64;
use ::long_multiplication;

// divides a 128-bit number by a 64-bit divisor, returning the quotient as a 64-bit number
// assumes that the divisor and numerator have both already been bit-shifted so that divisor.leading_zeros() == 0
#[inline]
fn divide_128_by_64_preshifted(numerator_hi: u64, numerator_lo: u64, divisor: u64) -> u64 {
    let numerator_mid = (numerator_lo >> 32) as u128;
    let numerator_lo = numerator_lo as u32 as u128;
    let divisor_full_128 = divisor as u128;
    let divisor_hi = divisor >> 32;

    // To get the upper 32 bits of the quotient, we want to divide 'full_upper_numerator' by 'divisor'
    // but the problem is, full_upper_numerator is a 96-bit number, meaning we would need to use u128 to do the division all at once, and the whole point of this is that we don't want to do 128 bit divison because it's slow
	// so instead, we'll shift both the numerator and divisor right by 32, giving us a 64 bit / 32 bit division. This won't give us the exact quotient -- but it will be close.
    let full_upper_numerator = ((numerator_hi as u128) << 32) | numerator_mid;
    let mut quotient_hi = core::cmp::min(numerator_hi / divisor_hi, U32_MAX);
    let mut product_hi = quotient_hi as u128 * divisor_full_128;

    // quotient_hi contains our guess at what the quotient is! the problem is that we got this by ignoring the lower 32 bits of the divisor. when we account for that, the quotient might be slightly lower
    // we will know our quotient is too high if quotient * divisor > numerator. if it is, decrement until it's in range
    while product_hi > full_upper_numerator {
        quotient_hi -= 1;
        product_hi -= divisor_full_128;
    }
    let remainder_hi = full_upper_numerator - product_hi;


    // repeat the process using the lower half of the numerator
    let full_lower_numerator = (remainder_hi << 32) | numerator_lo;
    let mut quotient_lo = core::cmp::min((remainder_hi as u64) / divisor_hi, U32_MAX);
    let mut product_lo = quotient_lo as u128 * divisor_full_128;

    // again, quotient_lo is just a guess at this point, it might be slightly too large
    while product_lo > full_lower_numerator {
        quotient_lo -= 1;
        product_lo -= divisor_full_128;
    }

    // We now have our separate quotients, now we just have to add them together
    (quotient_hi << 32) | quotient_lo
}

// divides a 128-bit number by a 64-bit divisor, returning the quotient as a 64-bit number
// assumes that the divisor and numerator have both already been bit-shifted to maximize the number of bits in divisor_hi
// divisor_hi should be the upper 32 bits, and divisor_lo should be the lower 32 bits
#[inline]
fn divide_128_by_64_preshifted_reduced(numerator_hi: u64, numerator_lo: u64, divisor_hi: StrengthReducedU64, divisor_full: u64) -> u64 {
    let numerator_mid = (numerator_lo >> 32) as u128;
    let numerator_lo = numerator_lo as u32 as u128;
    let divisor_full_128 = divisor_full as u128;

    // To get the upper 32 bits of the quotient, we want to divide 'full_upper_numerator' by 'divisor'
    // but the problem is, full_upper_numerator is a 96-bit number, meaning we would need to use u128 to do the division all at once, and the whole point of this is that we don't want to do 128 bit divison because it's slow
	// so instead, we'll shift both the numerator and divisor right by 32, giving us a 64 bit / 32 bit division. This won't give us the exact quotient -- but it will be close.
    let full_upper_numerator = ((numerator_hi as u128) << 32) | numerator_mid;
    let mut quotient_hi = core::cmp::min(numerator_hi / divisor_hi, U32_MAX);
    let mut product_hi = quotient_hi as u128 * divisor_full_128;

    // quotient_hi contains our guess at what the quotient is! the problem is that we got this by ignoring the lower 32 bits of the divisor. when we account for that, the quotient might be slightly lower
    // we will know our quotient is too high if quotient * divisor > numerator. if it is, decrement until it's in range
    while product_hi > full_upper_numerator {
        quotient_hi -= 1;
        product_hi -= divisor_full_128;
    }
    let full_upper_remainder = full_upper_numerator - product_hi;


    // repeat the process using the lower half of the numerator
    let full_lower_numerator = (full_upper_remainder << 32) | numerator_lo;
    let mut quotient_lo = core::cmp::min((full_upper_remainder as u64) / divisor_hi, U32_MAX);
    let mut product_lo = quotient_lo as u128 * divisor_full_128;

    // again, quotient_lo is just a guess at this point, it might be slightly too large
    while product_lo > full_lower_numerator {
        quotient_lo -= 1;
        product_lo -= divisor_full_128;
    }

    // We now have our separate quotients, now we just have to add them together
    (quotient_hi << 32) | quotient_lo
}

// divides a 128-bit number by a 128-bit divisor
pub fn divide_128(numerator: u128, divisor: u128) -> u128 {
	if divisor <= U64_MAX {
		let divisor64 = divisor as u64;
		let upper_numerator = (numerator >> 64) as u64;
		if divisor64 > upper_numerator {
			divide_128_by_64_helper(numerator, divisor64) as u128
		}
		else {
			let upper_quotient = upper_numerator / divisor64;
			let upper_remainder = upper_numerator - upper_quotient * divisor64;

			let intermediate_numerator = ((upper_remainder as u128) << 64) | (numerator as u64 as u128);
			let lower_quotient = divide_128_by_64_helper(intermediate_numerator, divisor64);

			((upper_quotient as u128) << 64) | (lower_quotient as u128)
		}
	}
	else {
		let shift_size = divisor.leading_zeros();
		let shifted_divisor = divisor << shift_size;

		let shifted_numerator = numerator >> 1;

		let upper_quotient = divide_128_by_64_helper(shifted_numerator, (shifted_divisor >> 64) as u64);
		let mut quotient = upper_quotient >> (63 - shift_size);
		if quotient > 0 {
			quotient -= 1;
		}

		let remainder = numerator - quotient as u128 * divisor;
		if remainder >= divisor {
			quotient += 1;
		}
		quotient as u128
	}
}

// divides a 128-bit number by a 64-bit divisor, returning the quotient as a 64-bit number. Panics if the quotient doesn't fit in a 64-bit number
fn divide_128_by_64_helper(numerator: u128, divisor: u64) -> u64 {
	// Assert that the upper half of the numerator is less than the denominator. This will guarantee that the quotient fits inside the numerator.
	// Sadly this will give us some false negatives! TODO: Find a quick test we can do that doesn't have false negatives
	// false negative example: numerator = u64::MAX * u64::MAX / u64::MAX
	assert!(divisor > (numerator >> 64) as u64, "The numerator is too large for the denominator; the quotient might not fit inside a u64.");

	if divisor <= U32_MAX {
		return divide_128_by_32_helper(numerator, divisor as u32);
	}

    let shift_size = divisor.leading_zeros();
	let shifted_divisor = divisor << shift_size;
	let shifted_numerator = numerator << shift_size;
	let divisor_hi = shifted_divisor >> 32;
    let divisor_lo = shifted_divisor as u32 as u64;

    // split the numerator into 3 chunks: the top 64-bits, the next 32-bits, and the lowest 32-bits
    let numerator_hi : u64 = (shifted_numerator >> 64) as u64;
    let numerator_mid : u64 = (shifted_numerator >> 32) as u32 as u64;
    let numerator_lo : u64 = shifted_numerator as u32 as u64;

    // we're essentially going to do a long division algorithm with 2 divisions, one on numerator_hi << 32 | numerator_mid, and the second on the remainder of the first | numerator_lo
    // but numerator_hi << 32 | numerator_mid is a 96-bit number, and we only have 64 bits to work with. so instead we split the divisor into 2 chunks, and divde by the upper chunk, and then check against the lower chunk in a while loop

    // step 1: divide the top chunk of the numerator by the divisor
    // IDEALLY, we would divide (numerator_hi << 32) | numerator_mid by shifted_divisor, but that would require a 128-bit numerator, which is the whole thing we're trying to avoid
    // so instead we're going to split the second division into two sub-phases. in 1a, we divide numerator_hi by divisor_hi, and then in 1b we decrement the quotient to account for the fact that it'll be smaller when you take divisor_lo into account

    // keep in mind that for all of step 2, the full numerator we're using will be
    // complete_first_numerator  = (numerator_midbits << 32) | numerator_mid

    // step 1a: divide the upper part of the middle numerator by the upper part of the divisor
    let mut quotient_hi = core::cmp::min(numerator_hi / divisor_hi, U32_MAX);
    let mut partial_remainder_hi = numerator_hi - quotient_hi * divisor_hi;

    // step 1b: we know sort of what the quotient is, but it's slightly too large because it doesn't account for divisor_lo, nor numerator_mid, so decrement the quotient until it fits
    // note that if we do some algebra on the condition in this while loop,
    // ie "quotient_hi * divisor_lo > (partial_remainder_hi << 32) | numerator_mid"
    // we end up getting "quotient_hi * shifted_divisor < (numerator_midbits << 32) | numerator_mid". remember that the right side of the inequality sign is complete_first_numerator from above.
    // which deminstrates that we're decrementing the quotient until the quotient multipled by the complete divisor is less than the complete numerator
    while partial_remainder_hi <= U32_MAX && quotient_hi * divisor_lo > (partial_remainder_hi << 32) | numerator_mid {
        quotient_hi -= 1;
        partial_remainder_hi += divisor_hi;
    }

    // step 2: Divide the bottom part of the numerator. We're going to have the same problem as step 1, where we want the numerator to be a 96-bit number, so again we're going to split it into 2 substeps
	// the full numeratoe for step 3 will be:
	// complete_second_numerator = (first_division_remainder << 32) | numerator_lo

    // step 2a: divide the upper part of the lower numerator by the upper part of the divisor
    // To get the numerator, complate the calculation of the full remainder by subtracing the quotient times the lower bits of the divisor
    // TODO: a warpping subtract is necessary here. why does this work, and why is it necessary?
    let full_remainder_hi = ((partial_remainder_hi << 32) | numerator_mid).wrapping_sub(quotient_hi * divisor_lo);

    let mut quotient_lo = core::cmp::min(full_remainder_hi / divisor_hi, U32_MAX);
    let mut partial_remainder_lo = full_remainder_hi - quotient_lo * divisor_hi;

    // step 2b: just like step 1b, decrement the final quotient until it's correctr when accounting for the full divisor
    while partial_remainder_lo <= U32_MAX && quotient_lo * divisor_lo > (partial_remainder_lo << 32) | numerator_lo {
        quotient_lo -= 1;
        partial_remainder_lo += divisor_hi;
    }

    // We now have our separate quotients, now we just have to add them together
    (quotient_hi << 32) | quotient_lo
}


// Same as divide_128_by_64_into_64, but optimized for scenarios where the divisor fits in a u32. Still panics if the quotient doesn't fit in a u64
fn divide_128_by_32_helper(numerator: u128, divisor: u32) -> u64 {
	// Assert that the upper half of the numerator is less than the denominator. This will guarantee that the quotient fits inside the numerator.
	// Sadly this will give us some false negatives! TODO: Find a quick test we can do that doesn't have false negatives
	// false negative example: numerator = u64::MAX * u64::MAX / u64::MAX
	assert!(divisor as u64 > (numerator >> 64) as u64, "The numerator is too large for the denominator; the quotient might not fit inside a u64.");

    let shift_size = divisor.leading_zeros();
	let shifted_divisor = (divisor << shift_size) as u64;
	let shifted_numerator = numerator << (shift_size + 32);

    // split the numerator into 3 chunks: the top 64-bits, the next 32-bits, and the lowest 32-bits
    let numerator_hi : u64 = (shifted_numerator >> 64) as u64;
    let numerator_mid : u64 = (shifted_numerator >> 32) as u32 as u64;

    // we're essentially going to do a long division algorithm with 2 divisions, one on numerator_hi << 32 | numerator_mid, and the second on the remainder of the first | numerator_lo
    // but numerator_hi << 32 | numerator_mid is a 96-bit number, and we only have 64 bits to work with. so instead we split the divisor into 2 chunks, and divde by the upper chunk, and then check against the lower chunk in a while loop

    // step 1: divide the top chunk of the numerator by the divisor
    // IDEALLY, we would divide (numerator_hi << 32) | numerator_mid by shifted_divisor, but that would require a 128-bit numerator, which is the whole thing we're trying to avoid
    // so instead we're going to split the second division into two sub-phases. in 1a, we divide numerator_hi by divisor_hi, and then in 1b we decrement the quotient to account for the fact that it'll be smaller when you take divisor_lo into account

    // keep in mind that for all of step 1, the full numerator we're using will be
    // complete_first_numerator  = (numerator_hi << 32) | numerator_mid

    // step 1a: divide the upper part of the middle numerator by the upper part of the divisor
    let quotient_hi = numerator_hi / shifted_divisor;
    let remainder_hi = numerator_hi - quotient_hi * shifted_divisor;

    // step 2: Divide the bottom part of the numerator. We're going to have the same problem as step 1, where we want the numerator to be a 96-bit number, so again we're going to split it into 2 substeps
	// the full numeratoe for step 3 will be:
	// complete_second_numerator = (first_division_remainder << 32) | numerator_lo

    // step 2a: divide the upper part of the lower numerator by the upper part of the divisor
    // To get the numerator, complate the calculation of the full remainder by subtracing the quotient times the lower bits of the divisor
    // TODO: a warpping subtract is necessary here. why does this work, and why is it necessary?
    let final_numerator = (remainder_hi) << 32 | numerator_mid;
    let quotient_lo = final_numerator / shifted_divisor;

    // We now have our separate quotients, now we just have to add them together
    (quotient_hi << 32) | quotient_lo
}

#[inline(never)]
fn long_division(numerator_slice: &[u64], reduced_divisor: &StrengthReducedU64, quotient: &mut [u64]) {
	let mut remainder = 0;
	for (numerator_element, quotient_element) in numerator_slice.iter().zip(quotient.iter_mut()).rev() {
		if remainder > 0 {
			// Do one division that includes the running remainder and the upper half of this numerator element, 
			// then a second division for the first division's remainder combinedwith the lower half
			let upper_numerator = (remainder << 32) | (*numerator_element >> 32);
			let (upper_quotient, upper_remainder) = StrengthReducedU64::div_rem(upper_numerator, *reduced_divisor);

			let lower_numerator = (upper_remainder << 32) | (*numerator_element as u32 as u64);
			let (lower_quotient, lower_remainder) = StrengthReducedU64::div_rem(lower_numerator, *reduced_divisor);

			*quotient_element = (upper_quotient << 32) | lower_quotient;
			remainder = lower_remainder;
		} else {
			// The remainder is zero, which means we can take a shortcut and only do a single division!
			let (digit_quotient, digit_remainder) = StrengthReducedU64::div_rem(*numerator_element, *reduced_divisor);

			*quotient_element = digit_quotient;
			remainder = digit_remainder;
		}
	}
}

#[inline]
fn normalize_slice(input: &mut [u64]) -> &mut [u64] {
	let input_len = input.len();
	let trailing_zero_chunks = input.iter().rev().take_while(|e| **e == 0).count();
	&mut input[..input_len - trailing_zero_chunks]
}

#[inline]
fn is_slice_greater(a: &[u64], b: &[u64]) -> bool {
    if a.len() > b.len() {
        return true;
    }
    if b.len() > a.len() {
    	return false;
    }

    for (&ai, &bi) in a.iter().zip(b.iter()).rev() {
        if ai < bi {
            return false;
        }
        if ai > bi {
            return true;
        }
    }
   	false
}
// subtract b from a, and store the result in a
#[inline]
fn sub_assign(a: &mut [u64], b: &[u64]) {
	let mut borrow: i128 = 0;

	// subtract b from a, keeping track of borrows as we go
	let (a_lo, a_hi) = a.split_at_mut(b.len());

	for (a, b) in a_lo.iter_mut().zip(b) {
		borrow += *a as i128;
		borrow -= *b as i128;
		*a = borrow as u64;
		borrow >>= 64;
	}

	// We're done subtracting, we just need to finish carrying
	let mut a_element = a_hi.iter_mut();
	while borrow != 0 {
		let a_element = a_element.next().expect("borrow underflow during sub_assign");
		borrow += *a_element as i128;

		*a_element = borrow as u64;
		borrow >>= 64;
	}
}

pub(crate) fn divide_128_max_by_64(divisor: u64) -> u128 {
	let quotient_hi = core::u64::MAX / divisor;
	let remainder_hi = core::u64::MAX - quotient_hi * divisor;

	let leading_zeros = divisor.leading_zeros();
	let quotient_lo = if leading_zeros >= 32 {
		let numerator_mid = (remainder_hi << 32) | core::u32::MAX as u64;
		let quotient_mid = numerator_mid / divisor;
		let remainder_mid = numerator_mid - quotient_mid * divisor;

		let numerator_lo = (remainder_mid << 32) | core::u32::MAX as u64;
		let quotient_lo = numerator_lo / divisor;

		(quotient_mid << 32) | quotient_lo
	}
	else {
		let numerator_hi = if leading_zeros > 0 { (remainder_hi << leading_zeros) | (core::u64::MAX >> (64 - leading_zeros)) } else { remainder_hi };
		let numerator_lo = core::u64::MAX << leading_zeros;

		divide_128_by_64_preshifted(numerator_hi, numerator_lo, divisor << leading_zeros)
	};
	((quotient_hi as u128) << 64) | (quotient_lo as u128)
}

fn divide_256_max_by_32(divisor: u32) -> (u128, u128) {
	let reduced_divisor = StrengthReducedU64::new(divisor as u64);
	let mut numerator_chunks = [core::u64::MAX; 4];
	let mut quotient_chunks = [0; 4];
	long_division(&mut numerator_chunks, &reduced_divisor, &mut quotient_chunks);

	// quotient_chunks now contains the quotient! all we have to do is recombine it into u128s
	let quotient_lo = (quotient_chunks[0] as u128) | ((quotient_chunks[1] as u128) << 64);
	let quotient_hi = (quotient_chunks[2] as u128) | ((quotient_chunks[3] as u128) << 64);

	(quotient_hi, quotient_lo)
}

pub(crate) fn divide_256_max_by_128(divisor: u128) -> (u128, u128) {
	let leading_zeros = divisor.leading_zeros();

	// if the divisor fits inside a u32, we can use a much faster algorithm
	if leading_zeros >= 96 {
		return divide_256_max_by_32(divisor as u32);
	}

	let empty_divisor_chunks = (leading_zeros / 64) as usize;
	let shift_amount = leading_zeros % 64;

	// Shift the divisor and chunk it up into U32s
	let divisor_shifted = divisor << shift_amount;
	let divisor_chunks = [
		divisor_shifted as u64,
		(divisor_shifted >> 64) as u64,
	];
	let divisor_slice = &divisor_chunks[..(divisor_chunks.len() - empty_divisor_chunks)];

	// We're gonna be doing a ton of u64/u64 divisions, so we're gonna eat our own dog food and set up a strength-reduced division instance
	// the only actual **divisions* we'll be doing will be with the largest 32 bits of the full divisor, not the full divisor
	let reduced_divisor_hi = StrengthReducedU64::new(*divisor_slice.last().unwrap() >> 32);
	let divisor_hi = *divisor_slice.last().unwrap();

	// Build our numerator, represented by u32 chunks. at first it will be full of u32::MAX, but we will iteratively take chunks out of it as we divide
	let mut numerator_chunks = [core::u64::MAX; 5];
	let mut numerator_max_idx = if shift_amount > 0 {
		numerator_chunks[4] >>= 64 - shift_amount;
		numerator_chunks[0] <<= shift_amount;
		5
	}
	else {
		4
	};

	// allocate the biggest-possible quotient, even if it might be smaller -- we just won't fill out the biggest parts
	let num_quotient_chunks = 3 + empty_divisor_chunks;
	let mut quotient_chunks = [0; 4];
	for quotient_idx in (0..num_quotient_chunks).rev() {
		/*
         * When calculating our next guess q0, we don't need to consider the digits below j
         * + b.data.len() - 1: we're guessing digit j of the quotient (i.e. q0 << j) from
         * digit bn of the divisor (i.e. bn << (b.data.len() - 1) - so the product of those
         * two numbers will be zero in all digits up to (j + b.data.len() - 1).
         */

        let numerator_slice = &mut numerator_chunks[..numerator_max_idx];
     	let numerator_start_idx = quotient_idx + divisor_slice.len() - 1;
     	if numerator_start_idx >= numerator_slice.len() {
            continue;
		}
		

		// scope for borrow checker
		{
			// divide the uppermost bits of the remaining numerator to get "sub_quotient" which will be our guess for this quotient element
			let numerator_hi = if numerator_slice.len() - numerator_start_idx > 1 { numerator_slice[numerator_start_idx + 1] } else { 0 };
			let numerator_lo = numerator_slice[numerator_start_idx];
			let mut sub_quotient = divide_128_by_64_preshifted_reduced(numerator_hi, numerator_lo, reduced_divisor_hi, divisor_hi);

			let mut tmp_product = [0; 3];
			long_multiplication::long_multiply(&divisor_slice, sub_quotient, &mut tmp_product);
			let sub_product = normalize_slice(&mut tmp_product);

			// our sub_quotient is just a guess at the quotient -- it only accounts for the topmost bits of the divisor. when we take the bottom bits of the divisor into account, the actual quotient will be smaller
			// we will know if our guess is too large if (quotient_guess * full_divisor) (aka sub_product) is greater than this iteration's numerator slice. ifthat's the case, decrement it until it's less than or equal.
			while is_slice_greater(sub_product, &numerator_slice[quotient_idx..]) {
				sub_assign(sub_product, &divisor_slice);
				sub_quotient -= 1;
			}

			// sub_quotient is now the correct sub-quotient for this iteration. add it to the full quotient, and subtract the product from the full numerator, so that what remains in the numerator is the remainder of this division
			quotient_chunks[quotient_idx] = sub_quotient;
			sub_assign(&mut numerator_slice[quotient_idx..], sub_product);
		}


		// slice off any zeroes at the end of the numerator. we're not calling normalize_slice here because of borrow checker obnoxiousness
		numerator_max_idx -= numerator_slice.iter().rev().take_while(|e| **e == 0).count();
	}

	
	// quotient_chunks now contains the quotient! all we have to do is recombine it into u128s
	let quotient_lo = (quotient_chunks[0] as u128)
		| ((quotient_chunks[1] as u128) << 64);
	let quotient_hi = (quotient_chunks[2] as u128)
		| ((quotient_chunks[3] as u128) << 64);

	(quotient_hi, quotient_lo)
}



#[cfg(test)]
mod unit_tests {
	use num_bigint::BigUint;

	#[test]
	fn test_divide_128_by_64() {
		for divisor in core::u64::MAX..=core::u64::MAX {
			let divisor_128 = core::u64::MAX as u128;

			let numerator = divisor_128 * divisor_128 + (divisor_128 - 1);
			//for numerator in core::u128::MAX - 10..core::u128::MAX {
		        let expected_quotient = numerator / divisor as u128;
		        assert!(expected_quotient == core::u64::MAX as u128);

		        let actual_quotient = super::divide_128_by_64_helper(numerator as u128, divisor);

		        

		        let expected_upper = (expected_quotient >> 32) as u64;
		        let expected_lower = expected_quotient as u32 as u64;
		        let actual_upper = (actual_quotient >> 32) as u64;
		        let actual_lower = actual_quotient as u32 as u64;

		        assert_eq!(expected_upper, actual_upper, "wrong quotient for {}/{}", numerator, divisor);
		        assert_eq!(expected_lower, actual_lower, "wrong quotient for {}/{}", numerator, divisor);
		    //}
	    }
	}

	fn test_divisor_128(divisor: u128) {
		let big_numerator = BigUint::from_slice(&[core::u32::MAX; 8]);
		let big_quotient = big_numerator / divisor;

		//let (actual_hi, actual_lo) = super::divide_256_max_by_128_direct(divisor);
		let (actual64_hi, actual64_lo) = super::divide_256_max_by_128(divisor);

		//let actual_big = (BigUint::from(actual_hi) << 128) | BigUint::from(actual_lo);
		let actual64_big = (BigUint::from(actual64_hi) << 128) | BigUint::from(actual64_lo);

		//assert_eq!(big_quotient, actual_big, "Actual quotient didn't match expected quotient for max/{}", divisor);
		assert_eq!(big_quotient, actual64_big, "Actual64 quotient didn't match expected quotient for max/{}", divisor);
	}

	#[allow(unused_imports)]
	use rand::{rngs::StdRng, SeedableRng, distributions::Distribution, distributions::Uniform};

	#[test]
	fn test_max_256() {
		let log2_tests_per_bit = 6;

		for divisor in 1..(1 << log2_tests_per_bit) {
			test_divisor_128(divisor);
		}

		let mut gen = StdRng::seed_from_u64(5673573);
		for bits in log2_tests_per_bit..128 {
			let lower_start = 1 << bits;
			let lower_stop = lower_start + (1 << (log2_tests_per_bit - 3));
			let upper_stop = 1u128.checked_shl(bits + 1).map_or(core::u128::MAX, |v| v - 1);
			let upper_start = upper_stop - (1 << (log2_tests_per_bit - 3)) + 1;

			for divisor in lower_start..lower_stop {
				test_divisor_128(divisor);
			}
			for divisor in upper_start..=upper_stop {
				test_divisor_128(divisor);
			}

			let random_count = 1 << log2_tests_per_bit;
			let dist = Uniform::new(lower_stop + 1, upper_start);
			for _ in 0..random_count {
				let divisor = dist.sample(&mut gen);
				test_divisor_128(divisor);
			}
		}
	}
}
