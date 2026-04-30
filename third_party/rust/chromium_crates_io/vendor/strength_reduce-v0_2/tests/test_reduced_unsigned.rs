#[macro_use]
extern crate proptest;
extern crate strength_reduce;

use proptest::test_runner::Config;
use strength_reduce::{StrengthReducedU8, StrengthReducedU16, StrengthReducedU32, StrengthReducedU64, StrengthReducedUsize, StrengthReducedU128};


macro_rules! reduction_proptest {
    ($test_name:ident, $struct_name:ident, $primitive_type:ident) => (
        mod $test_name {
            use super::*;
            use proptest::sample::select;

            fn assert_div_rem_equivalence(divisor: $primitive_type, numerator: $primitive_type) {
                let reduced_divisor = $struct_name::new(divisor);
                let expected_div = numerator / divisor;
                let expected_rem = numerator % divisor;
                let reduced_div = numerator / reduced_divisor;
                let reduced_rem = numerator % reduced_divisor;
                assert_eq!(expected_div, reduced_div, "Divide failed with numerator: {}, divisor: {}", numerator, divisor);
                assert_eq!(expected_rem, reduced_rem, "Modulo failed with numerator: {}, divisor: {}", numerator, divisor);
                let (reduced_combined_div, reduced_combined_rem) = $struct_name::div_rem(numerator, reduced_divisor);
                assert_eq!(expected_div, reduced_combined_div, "div_rem divide failed with numerator: {}, divisor: {}", numerator, divisor);
                assert_eq!(expected_rem, reduced_combined_rem, "div_rem modulo failed with numerator: {}, divisor: {}", numerator, divisor);
            }



            proptest! {
                #![proptest_config(Config::with_cases(100_000))]

                #[test]
                fn fully_generated_inputs_are_div_rem_equivalent(divisor in 1..core::$primitive_type::MAX, numerator in 0..core::$primitive_type::MAX) {
                    assert_div_rem_equivalence(divisor, numerator);
                }

                #[test]
                fn generated_divisors_with_edge_case_numerators_are_div_rem_equivalent(
                        divisor in 1..core::$primitive_type::MAX,
                        numerator in select(vec![0 as $primitive_type, 1 as $primitive_type, core::$primitive_type::MAX - 1, core::$primitive_type::MAX])) {
                    assert_div_rem_equivalence(divisor, numerator);
                }

                #[test]
                fn generated_numerators_with_edge_case_divisors_are_div_rem_equivalent(
                        divisor in select(vec![1 as $primitive_type, 2 as $primitive_type, core::$primitive_type::MAX - 1, core::$primitive_type::MAX]),
                        numerator in 0..core::$primitive_type::MAX) {
                    assert_div_rem_equivalence(divisor, numerator);
                }
            }
        }
    )
}
reduction_proptest!(strength_reduced_u08, StrengthReducedU8, u8);
reduction_proptest!(strength_reduced_u16, StrengthReducedU16, u16);
reduction_proptest!(strength_reduced_u32, StrengthReducedU32, u32);
reduction_proptest!(strength_reduced_u64, StrengthReducedU64, u64);
reduction_proptest!(strength_reduced_usize, StrengthReducedUsize, usize);
reduction_proptest!(strength_reduced_u128, StrengthReducedU128, u128);

macro_rules! exhaustive_test {
    ($test_name:ident, $struct_name:ident, $primitive_type:ident) => (
    	#[test]
    	#[ignore]
    	fn $test_name() {
    		for divisor in 1..=std::$primitive_type::MAX {
    			let reduced_divisor = $struct_name::new(divisor);

    			for numerator in 0..=std::$primitive_type::MAX {
    				let expected_div = numerator / divisor;
	                let expected_rem = numerator % divisor;

	                let reduced_div = numerator / reduced_divisor;
	                assert_eq!(expected_div, reduced_div, "Divide failed with numerator: {}, divisor: {}", numerator, divisor);

	                let reduced_rem = numerator % reduced_divisor;
	                assert_eq!(expected_rem, reduced_rem, "Modulo failed with numerator: {}, divisor: {}", numerator, divisor);

	                let (reduced_combined_div, reduced_combined_rem) = $struct_name::div_rem(numerator, reduced_divisor);
	                assert_eq!(expected_div, reduced_combined_div, "div_rem divide failed with numerator: {}, divisor: {}", numerator, divisor);
	                assert_eq!(expected_rem, reduced_combined_rem, "div_rem modulo failed with numerator: {}, divisor: {}", numerator, divisor);
    			}
    		}
    	}
    )
}

exhaustive_test!(test_strength_reduced_u08_exhaustive, StrengthReducedU8, u8);
exhaustive_test!(test_strength_reduced_u16_exhaustive, StrengthReducedU16, u16);
