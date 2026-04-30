
// multiply the 256-bit number 'a' by the 128-bit number 'b' and return the uppermost 128 bits of the product
// ripped directly from num-biguint's long multiplication algorithm (mac3, mac_with_carry, adc), but with fixed-size arrays instead of slices
#[inline]
pub(crate) fn multiply_256_by_128_upperbits(a_hi: u128, a_lo: u128, b: u128) -> u128 {
	// Break a and b into little-endian 64-bit chunks
	let a_chunks = [
		a_lo as u64,
		(a_lo >> 64) as u64,
		a_hi as u64,
		(a_hi >> 64) as u64,
	];
	let b_chunks = [
		b as u64,
		(b >> 64) as u64,
	];

	// Multiply b by a, one chink of b at a time
	let mut product = [0; 6];
	for (b_index, &b_digit) in b_chunks.iter().enumerate() {
		multiply_256_by_64_helper(&mut product[b_index..], &a_chunks, b_digit);
	}

	// the last 2 elements of the array have the part of the productthat we care about
	((product[5] as u128) << 64) | (product[4] as u128)
}

#[inline]
fn multiply_256_by_64_helper(product: &mut [u64], a: &[u64;4], b: u64) {
	if b == 0 {
		return;
	}

	let mut carry = 0;
	let (product_lo, product_hi) = product.split_at_mut(a.len());

	// Multiply each of the digits in a by b, adding them into the 'product' value.
	// We don't zero out product, because we this will be called multiple times, so it probably contains a previous iteration's partial product, and we're adding + carrying on top of it
	for (p, &a_digit) in product_lo.iter_mut().zip(a) {
		carry += *p as u128;
		carry += (a_digit as u128) * (b as u128);

		*p = carry as u64;
		carry >>= 64;
	}

	// We're done multiplying, we just need to finish carrying through the rest of the product.
	let mut p = product_hi.iter_mut();
	while carry != 0 {
		let p = p.next().expect("carry overflow during multiplication!");
		carry += *p as u128;

		*p = carry as u64;
		carry >>= 64;
	}
}

// compute product += a * b
#[inline]
pub(crate) fn long_multiply(a: &[u64], b: u64, product: &mut [u64]) {
	if b == 0 {
		return;
	}

	let mut carry = 0;
	let (product_lo, product_hi) = product.split_at_mut(a.len());

	// Multiply each of the digits in a by b, adding them into the 'product' value.
	// We don't zero out product, because we this will be called multiple times, so it probably contains a previous iteration's partial product, and we're adding + carrying on top of it
	for (p, &a_digit) in product_lo.iter_mut().zip(a) {
		carry += *p as u128;
		carry += (a_digit as u128) * (b as u128);

		*p = carry as u64;
		carry >>= 64;
	}

	// We're done multiplying, we just need to finish carrying through the rest of the product.
	let mut p = product_hi.iter_mut();
	while carry != 0 {
		let p = p.next().expect("carry overflow during multiplication!");
		carry += *p as u128;

		*p = carry as u64;
		carry >>= 64;
	}
}
