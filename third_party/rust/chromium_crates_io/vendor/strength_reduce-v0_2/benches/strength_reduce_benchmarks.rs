#![feature(test)]
extern crate test;
extern crate strength_reduce;
extern crate rand;

// rustc incorrectly says these are unused
#[allow(unused_imports)]
use rand::{rngs::StdRng, SeedableRng, distributions::Distribution, distributions::Uniform};

const REPETITIONS: usize = 1000;

macro_rules! bench_unsigned {
	($struct_name:ident, $primitive_type:ident) => (
		#[inline(never)]
		fn compute_repeated_division_primitive(numerators: &[$primitive_type], divisor: $primitive_type) -> $primitive_type {
			let mut sum = 0;
			for numerator in numerators {
				sum += *numerator / divisor;
			}
			sum
		}

		#[inline(never)]
		fn compute_repeated_division(numerators: &[$primitive_type], divisor: strength_reduce::$struct_name) -> $primitive_type {
			let mut sum = 0;
			for numerator in numerators {
				sum += *numerator / divisor;
			}
			sum
		}

		#[inline(never)]
		fn compute_single_division(divisors: &[$primitive_type]) -> $primitive_type {
			let mut sum = 0;
			for divisor in divisors {
				let reduced_divisor = strength_reduce::$struct_name::new(*divisor);
				sum += 100 / reduced_divisor;
			}
			sum
		}

		#[inline(never)]
		fn compute_repeated_modulo_primitive(numerators: &[$primitive_type], divisor: $primitive_type) -> $primitive_type {
			let mut sum = 0;
			for numerator in numerators {
				sum += *numerator % divisor;
			}
			sum
		}

		#[inline(never)]
		fn compute_repeated_modulo(numerators: &[$primitive_type], divisor: strength_reduce::$struct_name) -> $primitive_type {
			let mut sum = 0;
			for numerator in numerators {
				sum += *numerator % divisor;
			}
			sum
		}

		#[inline(never)]
		fn compute_repeated_divrem(numerators: &[$primitive_type], divisor: strength_reduce::$struct_name) -> ($primitive_type, $primitive_type) {
			let mut div_sum = 0;
			let mut rem_sum = 0;
			for numerator in numerators {
				let (div_value, rem_value) = strength_reduce::$struct_name::div_rem(*numerator, divisor);
				div_sum += div_value;
				rem_sum += rem_value;
			}
			(div_sum, rem_sum)
		}

		fn gen_numerators() -> Vec<$primitive_type> {
			test::black_box((0..std::$primitive_type::MAX).rev().cycle().take(REPETITIONS).collect::<Vec<$primitive_type>>())
		}

		#[bench]
		fn division_standard(b: &mut test::Bencher) {
			let numerators = gen_numerators();
			let divisor = 6;
			b.iter(|| { test::black_box(compute_repeated_division_primitive(&numerators, divisor)); });
		}

		#[bench]
		fn repeated_division_reduced_power2(b: &mut test::Bencher) {
			let reduced_divisor = strength_reduce::$struct_name::new(8);
			let numerators = gen_numerators();
			b.iter(|| { test::black_box(compute_repeated_division(&numerators, reduced_divisor)); });
		}

		#[bench]
		fn repeated_division_reduced(b: &mut test::Bencher) {
			let reduced_divisor = strength_reduce::$struct_name::new(6);
			let numerators = gen_numerators();
			b.iter(|| { test::black_box(compute_repeated_division(&numerators, reduced_divisor)); });
		}

		#[bench]
		fn modulo_standard(b: &mut test::Bencher) {
		    let numerators = gen_numerators();
		    let divisor = 6;
			b.iter(|| { test::black_box(compute_repeated_modulo_primitive(&numerators, divisor)); });
		}

		#[bench]
		fn repeated_modulo_reduced_power2(b: &mut test::Bencher) {
			let reduced_divisor = strength_reduce::$struct_name::new(8);
			let numerators = gen_numerators();
			b.iter(|| { test::black_box(compute_repeated_modulo(&numerators, reduced_divisor)); });
		}

		#[bench]
		fn repeated_modulo_reduced(b: &mut test::Bencher) {
			let reduced_divisor = strength_reduce::$struct_name::new(6);
			let numerators = gen_numerators();
			b.iter(|| { test::black_box(compute_repeated_modulo(&numerators, reduced_divisor)); });
		}

		#[bench]
		fn repeated_divrem_reduced_power2(b: &mut test::Bencher) {
			let reduced_divisor = strength_reduce::$struct_name::new(8);
			let numerators = gen_numerators();
			b.iter(|| { test::black_box(compute_repeated_divrem(&numerators, reduced_divisor)); });
		}

		#[bench]
		fn repeated_divrem_reduced(b: &mut test::Bencher) {
			let reduced_divisor = strength_reduce::$struct_name::new(6);
			let numerators = gen_numerators();
			b.iter(|| { test::black_box(compute_repeated_divrem(&numerators, reduced_divisor)); });
		}
		
		#[bench]
		fn single_division_reduced_power2(b: &mut test::Bencher) {
			let divisors = test::black_box(vec![8; REPETITIONS]);
			b.iter(|| { test::black_box(compute_single_division(&divisors)); });
		}

		#[bench]
		fn single_division_reduced(b: &mut test::Bencher) {
			let divisors = test::black_box(vec![core::$primitive_type::MAX; REPETITIONS]);
			b.iter(|| { test::black_box(compute_single_division(&divisors)); });
		}
	)
}

mod bench_u08 {
	use super::*;
	bench_unsigned!(StrengthReducedU8, u8);
}
mod bench_u16 {
	use super::*;
	bench_unsigned!(StrengthReducedU16, u16);
}
mod bench_u32 {
	use super::*;
	bench_unsigned!(StrengthReducedU32, u32);
}
mod bench_u64 {
	use super::*;
	bench_unsigned!(StrengthReducedU64, u64);

	// generates random divisors with values in the range [1<<bit_min, 1<<bit_max)
	fn generate_random_divisors(bit_min: u32, bit_max: u32, count: usize) -> Vec<u64> {
		assert!(bit_min < bit_max);
		assert!(bit_max <= 64);

		let min_value = 1u64 << bit_min;
		let max_value = 1u64.checked_shl(bit_max).map_or(core::u64::MAX, |v| v - 1);

		let mut gen = StdRng::seed_from_u64(5673573);
		let dist = Uniform::new_inclusive(min_value, max_value);
		
		(0..count).map(|_| dist.sample(&mut gen)).collect()
	}

	// since the constructor for StrengthReducedU64 is so dependent on input size, we're going to do a few more targeted "single division" tests at specific sizes, so we can measure each "size class" separately
	#[bench]
	fn targeted_single_division_32bit(b: &mut test::Bencher) {
		let divisors = test::black_box(generate_random_divisors(0, 32, REPETITIONS));
		b.iter(|| { test::black_box(compute_single_division(&divisors)); });
	}
	#[bench]
	fn targeted_single_division_64bit(b: &mut test::Bencher) {
		let divisors = test::black_box(generate_random_divisors(32, 64, REPETITIONS));
		b.iter(|| { test::black_box(compute_single_division(&divisors)); });
	}
}
mod bench_u128 {
	use super::*;
	bench_unsigned!(StrengthReducedU128, u128);

	// generates random divisors with values in the range [1<<bit_min, 1<<bit_max)
	fn generate_random_divisors(bit_min: u32, bit_max: u32, count: usize) -> Vec<u128> {
		assert!(bit_min < bit_max);
		assert!(bit_max <= 128);

		let min_value = 1u128 << bit_min;
		let max_value = 1u128.checked_shl(bit_max).map_or(core::u128::MAX, |v| v - 1);

		let mut gen = StdRng::seed_from_u64(5673573);
		let dist = Uniform::new_inclusive(min_value, max_value);
		
		(0..count).map(|_| dist.sample(&mut gen)).collect()
	}

	// since the constructor for StrengthReducedU128 is so dependent on input size, we're going to do a few more targeted "single division" tests at specific sizes, so we can measure each "size class" separately
	#[bench]
	fn targeted_single_division_032bit(b: &mut test::Bencher) {
		let divisors = test::black_box(generate_random_divisors(0, 32, REPETITIONS));
		b.iter(|| { test::black_box(compute_single_division(&divisors)); });
	}
	#[bench]
	fn targeted_single_division_064bit(b: &mut test::Bencher) {
		let divisors = test::black_box(generate_random_divisors(32, 64, REPETITIONS));
		b.iter(|| { test::black_box(compute_single_division(&divisors)); });
	}
	#[bench]
	fn targeted_single_division_096bit(b: &mut test::Bencher) {
		let divisors = test::black_box(generate_random_divisors(64, 96, REPETITIONS));
		b.iter(|| { test::black_box(compute_single_division(&divisors)); });
	}
	#[bench]
	fn targeted_single_division_128bit(b: &mut test::Bencher) {
		let divisors = test::black_box(generate_random_divisors(96, 128, REPETITIONS));
		b.iter(|| { test::black_box(compute_single_division(&divisors)); });
	}
}