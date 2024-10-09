use fend_core::{evaluate, Context};

#[track_caller]
fn test_serialization_roundtrip(context: &mut Context) {
	let mut v = vec![];
	context.serialize_variables(&mut v).unwrap();
	let ctx_debug_repr = format!("{context:#?}");
	match context.deserialize_variables(&mut v.as_slice()) {
		Ok(()) => (),
		Err(s) => {
			eprintln!("Data: {:?}", &v);
			eprintln!("Context: {ctx_debug_repr}");
			panic!("Failed to deserialize: {s}");
		}
	}
}

#[track_caller]
fn test_eval_simple(input: &str, expected: &str) {
	let mut context = Context::new();
	context.set_exchange_rate_handler_v1(fend_core::test_utils::dummy_currency_handler);
	assert_eq!(
		evaluate(input, &mut context).unwrap().get_main_result(),
		expected
	);
	if let Ok(res) = evaluate(expected, &mut context) {
		assert_ne!(res.get_main_result(), expected);
	}
	test_serialization_roundtrip(&mut context);
}

#[track_caller]
fn test_eval(input: &str, expected: &str) {
	let mut context = Context::new();
	context.set_exchange_rate_handler_v1(fend_core::test_utils::dummy_currency_handler);
	assert_eq!(
		evaluate(input, &mut context).unwrap().get_main_result(),
		expected
	);
	// try parsing the output again, and make sure it matches
	assert_eq!(
		evaluate(expected, &mut context).unwrap().get_main_result(),
		expected
	);
	test_serialization_roundtrip(&mut context);
}

#[track_caller]
fn expect_error(input: &str, error_message: Option<&str>) {
	let mut context = Context::new();
	if let Some(error_message) = error_message {
		assert_eq!(
			evaluate(input, &mut context),
			Err(error_message.to_string())
		);
	} else {
		assert!(evaluate(input, &mut context).is_err());
	}
}

#[test]
fn two() {
	test_eval("2", "2");
}

#[test]
fn nine() {
	test_eval("9", "9");
}

#[test]
fn ten() {
	test_eval("10", "10");
}

#[test]
fn large_integer() {
	test_eval("39456720983475234523452345", "39456720983475234523452345");
}

#[test]
fn ten_whitespace_after() {
	test_eval("10 ", "10");
}

#[test]
fn ten_whitespace_before() {
	test_eval(" 10", "10");
}

#[test]
fn ten_whitespace_both() {
	test_eval(" 10\n\r\n", "10");
}

#[test]
fn blank_input() {
	test_eval("", "");
}

#[test]
fn version() {
	let mut ctx = Context::new();
	let result = evaluate("version", &mut ctx).unwrap();
	for c in result.get_main_result().chars() {
		assert!(c.is_ascii_digit() || c == '.');
	}
}

#[test]
fn pi() {
	test_eval("pi", "approx. 3.1415926535");
}

#[test]
fn pi_times_two() {
	test_eval("pi * 2", "approx. 6.2831853071");
}

#[test]
fn two_pi() {
	test_eval("2 pi", "approx. 6.2831853071");
}

#[test]
fn pi_to_fraction() {
	let mut ctx = Context::new();
	assert!(evaluate("pi to fraction", &mut ctx)
		.unwrap()
		.get_main_result()
		.starts_with("approx."));
}

const DIVISION_BY_ZERO_ERROR: &str = "division by zero";
#[test]
fn one_over_zero() {
	expect_error("1/0", Some(DIVISION_BY_ZERO_ERROR));
}

#[test]
fn zero_over_zero() {
	expect_error("0/0", Some(DIVISION_BY_ZERO_ERROR));
}

#[test]
fn minus_one_over_zero() {
	expect_error("-1/0", Some(DIVISION_BY_ZERO_ERROR));
}

#[test]
fn three_pi_over_zero_pi() {
	expect_error("(3pi) / (0pi)", Some(DIVISION_BY_ZERO_ERROR));
}

#[test]
fn minus_one_over_zero_indirect() {
	expect_error("-1/(2-2)", Some(DIVISION_BY_ZERO_ERROR));
}

#[test]
fn two_zeroes() {
	test_eval("00", "0");
}

#[test]
fn six_zeroes() {
	test_eval("000000", "0");
}

#[test]
fn multiple_zeroes_with_decimal_point() {
	test_eval("000000.01", "0.01");
}

#[test]
fn leading_zeroes_and_decimal_point() {
	test_eval("0000001.01", "1.01");
}

#[test]
fn binary_leading_zeroes() {
	test_eval("0b01", "0b1");
}

#[test]
fn hex_leading_zeroes() {
	test_eval("0x0000_00ff", "0xff");
}

#[test]
fn explicit_base_10_leading_zeroes() {
	test_eval("10#04", "10#4");
}

#[test]
fn leading_zeroes_after_decimal_point() {
	test_eval("1.001", "1.001");
}

#[test]
fn leading_zeroes_in_exponent() {
	test_eval("1e01", "10");
}

#[test]
fn leading_zeroes_in_negative_exponent() {
	test_eval("1e-01", "0.1");
}

#[test]
fn upper_case_exponent() {
	test_eval("1E3", "1000");
}

#[test]
fn upper_case_big_exponent() {
	test_eval("1E10", "10000000000");
}

#[test]
fn upper_case_neg_exponent() {
	test_eval("1E-3", "0.001");
}

#[test]
fn upper_case_binary_exponent() {
	test_eval("0b10E100 to decimal", "32");
}

#[test]
fn no_recurring_digits() {
	expect_error("0.()", None);
}

#[test]
fn to_float_1() {
	test_eval_simple("0.(3) to float", "0.(3)");
}

#[test]
fn to_float_2() {
	test_eval_simple("0.(33) to float", "0.(3)");
}

#[test]
fn to_float_3() {
	test_eval_simple("0.(34) to float", "0.(34)");
}

#[test]
fn to_float_4() {
	test_eval_simple("0.(12345) to float", "0.(12345)");
}

#[test]
fn to_float_5() {
	test_eval("0.(0) to float", "0");
}

#[test]
fn to_float_6() {
	test_eval("0.123(00) to float", "0.123");
}

#[test]
fn to_float_7() {
	test_eval_simple("0.0(34) to float", "0.0(34)");
}

#[test]
fn to_float_8() {
	test_eval_simple("0.00(34) to float", "0.00(34)");
}

#[test]
fn to_float_9() {
	test_eval_simple("0.0000(34) to float", "0.0000(34)");
}

#[test]
fn to_float_10() {
	test_eval_simple("0.123434(34) to float", "0.12(34)");
}

#[test]
fn to_float_11() {
	test_eval_simple("0.123434(34)i to float", "0.12(34)i");
}

#[test]
fn to_float_12() {
	test_eval_simple("0.(3) + 0.123434(34)i to float", "0.(3) + 0.12(34)i");
}

#[test]
fn to_float_13() {
	test_eval_simple("6#0.(1) to float", "6#0.(1)");
}

#[test]
fn to_float_14() {
	test_eval("6#0.(1) to float in base 10", "0.2");
}

#[test]
fn two_times_two() {
	test_eval("2*2", "4");
}

#[test]
fn two_times_two_whitespace() {
	test_eval("\n2\n*\n2\n", "4");
}

#[test]
fn large_multiplication() {
	test_eval(
		"315427679023453451289740 * 927346502937456234523452",
		"292510755072077978255166497050046859223676982480",
	);
}

#[test]
fn pi_times_pi() {
	test_eval("pi * pi", "approx. 9.869604401");
}

#[test]
fn four_pi_plus_one() {
	test_eval("4pi + 1", "approx. 13.5663706143");
}

#[test]
fn implicit_lambda_1() {
	test_eval("-sin (-pi/2)", "1");
}

#[test]
fn implicit_lambda_2() {
	test_eval("+sin (-pi/2)", "-1");
}

#[test]
fn implicit_lambda_3() {
	test_eval("/sin (-pi/2)", "-1");
}

#[test]
fn implicit_lambda_4() {
	test_eval("cos! 0", "1");
}

#[test]
fn implicit_lambda_5() {
	test_eval("sqrt! 16", "24");
}

#[test]
fn implicit_lambda_6() {
	test_eval("///sqrt! 16", "approx. 0.0416666666");
}

#[test]
fn implicit_lambda_7() {
	test_eval("(x: sin^2 x + cos^2 x) 1", "approx. 1");
}

#[test]
fn implicit_lambda_8() {
	test_eval("cos^2 pi", "1");
}

#[test]
fn implicit_lambda_9() {
	test_eval("sin pi/cos pi", "0");
}

#[test]
fn implicit_lambda_10() {
	test_eval("sin + 1) pi", "1");
}

#[test]
fn implicit_lambda_11() {
	test_eval("3sin pi", "0");
}

#[test]
fn implicit_lambda_12() {
	test_eval("(-sqrt) 4", "-2");
}

#[test]
#[ignore]
fn implicit_lambda_13() {
	test_eval("-sqrt 4", "-2");
}

#[test]
fn inverse_sin() {
	test_eval("sin^-1", "asin");
}

#[test]
fn inverse_sin_point_five() {
	test_eval("sin^-1 0.5", "approx. 0.5235987755");
}

#[test]
fn inverse_sin_nested() {
	test_eval("sin^-1 (sin 0.5", "approx. 0.5");
}

#[test]
fn inverse_sin_nested_2() {
	test_eval("(sin^-1)^-1", "sin");
}

#[test]
fn inverse_cos() {
	test_eval("cos^-1", "acos");
}

#[test]
fn inverse_tan() {
	test_eval("tan^-1", "atan");
}

#[test]
fn inverse_asin() {
	test_eval("asin^-1", "sin");
}

#[test]
fn inverse_acos() {
	test_eval("acos^-1", "cos");
}

#[test]
fn inverse_atan() {
	test_eval("atan^-1", "tan");
}

#[test]
fn inverse_sinh() {
	test_eval("sinh^-1", "asinh");
}

#[test]
fn inverse_cosh() {
	test_eval("cosh^-1", "acosh");
}

#[test]
fn inverse_tanh() {
	test_eval("tanh^-1", "atanh");
}

#[test]
fn inverse_asinh() {
	test_eval("asinh^-1", "sinh");
}

#[test]
fn inverse_acosh() {
	test_eval("acosh^-1", "cosh");
}

#[test]
fn inverse_atanh() {
	test_eval("atanh^-1", "tanh");
}

#[test]
fn two_plus_two() {
	test_eval("2+2", "4");
}

#[test]
fn two_plus_two_whitespace() {
	test_eval("\n2\n+\n2\n", "4");
}

#[test]
fn plus_two() {
	test_eval("+2", "2");
}

#[test]
fn unary_pluses_two() {
	test_eval("++++2", "2");
}

#[test]
fn large_simple_addition() {
	test_eval(
		"315427679023453451289740 + 927346502937456234523452",
		"1242774181960909685813192",
	);
}

#[test]
fn minus_zero() {
	test_eval("-0", "0");
}

#[test]
fn two_minus_two() {
	test_eval("2-2", "0");
}

#[test]
fn three_minus_two() {
	test_eval("3-2", "1");
}

#[test]
fn two_minus_three() {
	test_eval("2-3", "-1");
}

#[test]
fn minus_two() {
	test_eval("-2", "-2");
}

#[test]
fn minus_minus_two() {
	test_eval("--2", "2");
}

#[test]
fn minus_minus_minus_two() {
	test_eval("---2", "-2");
}

#[test]
fn minus_minus_minus_two_parens() {
	test_eval("-(--2)", "-2");
}

#[test]
fn two_minus_64() {
	test_eval("\n2\n-\n64\n", "-62");
}

#[test]
fn large_simple_subtraction() {
	test_eval(
		"315427679023453451289740 - 927346502937456234523452",
		"-611918823914002783233712",
	);
}

#[test]
fn three_pi_minus_two_pi() {
	test_eval("3pi - 2pi", "approx. 3.1415926535");
}

#[test]
fn four_pi_plus_one_over_pi() {
	test_eval("4pi-1)/pi", "approx. 3.6816901138");
}

#[test]
fn large_simple_subtraction_2() {
	test_eval(
		"36893488123704996004 - 18446744065119617025",
		"18446744058585378979",
	);
}

#[test]
fn sqrt_half() {
	test_eval("sqrt (1/2)", "approx. 0.7071067814");
}

#[test]
fn sqrt_0() {
	test_eval("sqrt 0", "0");
}

#[test]
fn sqrt_1() {
	test_eval("sqrt 1", "1");
}

#[test]
fn sqrt_2() {
	test_eval("sqrt 2", "approx. 1.4142135619");
}

#[test]
fn sqrt_pi() {
	test_eval("sqrt pi", "approx. 1.7724538509");
}

#[test]
fn sqrt_4() {
	test_eval("sqrt 4", "2");
}

#[test]
fn sqrt_9() {
	test_eval("sqrt 9", "3");
}

#[test]
fn sqrt_16() {
	test_eval("sqrt 16", "4");
}

#[test]
fn sqrt_25() {
	test_eval("sqrt 25", "5");
}

#[test]
fn sqrt_36() {
	test_eval("sqrt 36", "6");
}

#[test]
fn sqrt_49() {
	test_eval("sqrt 49", "7");
}

#[test]
fn sqrt_64() {
	test_eval("sqrt 64", "8");
}

#[test]
fn sqrt_81() {
	test_eval("sqrt 81", "9");
}

#[test]
fn sqrt_100() {
	test_eval("sqrt 100", "10");
}

#[test]
fn sqrt_10000() {
	test_eval("sqrt 10000", "100");
}

#[test]
fn sqrt_1000000() {
	test_eval("sqrt 1000000", "1000");
}

#[test]
fn sqrt_quarter() {
	test_eval("sqrt 0.25", "0.5");
}

#[test]
fn sqrt_sixteenth() {
	test_eval("sqrt 0.0625", "0.25");
}

#[test]
fn cbrt_0() {
	test_eval("cbrt 0", "0");
}

#[test]
fn cbrt_1() {
	test_eval("cbrt 1", "1");
}

#[test]
fn cbrt_8() {
	test_eval("cbrt 8", "2");
}

#[test]
fn cbrt_27() {
	test_eval("cbrt 27", "3");
}

#[test]
fn cbrt_64() {
	test_eval("cbrt 64", "4");
}

#[test]
fn cbrt_eighth() {
	test_eval("cbrt (1/8)", "0.5");
}

#[test]
fn cbrt_1_over_125() {
	test_eval("cbrt (125/8)", "2.5");
}

#[test]
fn sqrt_kg_squared_1() {
	test_eval("sqrt(kg^2)", "1 kg");
}

#[test]
fn sqrt_kg_squared_2() {
	test_eval("(sqrt kg)^2", "1 kg");
}

#[test]
fn lightyear_to_parsecs() {
	test_eval("1 lightyear to parsecs", "approx. 0.3066013937 parsecs");
}

#[test]
fn order_of_operations_1() {
	test_eval("2+2*3", "8");
}

#[test]
fn order_of_operations_2() {
	test_eval("2*2+3", "7");
}

#[test]
fn order_of_operations_3() {
	test_eval("2+2+3", "7");
}

#[test]
fn order_of_operations_4() {
	test_eval("2+2-3", "1");
}

#[test]
fn order_of_operations_5() {
	test_eval("2-2+3", "3");
}

#[test]
fn order_of_operations_6() {
	test_eval("2-2-3", "-3");
}

#[test]
fn order_of_operations_7() {
	test_eval("2*2*3", "12");
}

#[test]
fn order_of_operations_8() {
	test_eval("2*2*-3", "-12");
}

#[test]
fn order_of_operations_9() {
	test_eval("2*-2*3", "-12");
}

#[test]
fn order_of_operations_10() {
	test_eval("-2*2*3", "-12");
}

#[test]
fn order_of_operations_11() {
	test_eval("-2*-2*3", "12");
}

#[test]
fn order_of_operations_12() {
	test_eval("-2*2*-3", "12");
}

#[test]
fn order_of_operations_13() {
	test_eval("2*-2*-3", "12");
}

#[test]
fn order_of_operations_14() {
	test_eval("-2*-2*-3", "-12");
}

#[test]
fn order_of_operations_15() {
	test_eval("-2*-2*-3/2", "-6");
}

#[test]
fn order_of_operations_16() {
	test_eval("-2*-2*-3/-2", "6");
}

#[test]
fn order_of_operations_17() {
	test_eval("-3 -1/2", "-3.5");
}

#[test]
fn yobibyte() {
	test_eval("1 YiB to bytes", "1208925819614629174706176 bytes");
}

#[test]
fn div_1_over_1() {
	test_eval("1/1", "1");
}

#[test]
fn div_1_over_2() {
	test_eval("1/2", "0.5");
}

#[test]
fn div_1_over_4() {
	test_eval("1/4", "0.25");
}

#[test]
fn div_1_over_8() {
	test_eval("1/8", "0.125");
}

#[test]
fn div_1_over_16() {
	test_eval("1/16", "0.0625");
}

#[test]
fn div_1_over_32() {
	test_eval("1/32", "0.03125");
}

#[test]
fn div_1_over_64() {
	test_eval("1/64", "0.015625");
}

#[test]
fn div_2_over_64() {
	test_eval("2/64", "0.03125");
}

#[test]
fn div_4_over_64() {
	test_eval("4/64", "0.0625");
}

#[test]
fn div_8_over_64() {
	test_eval("8/64", "0.125");
}

#[test]
fn div_16_over_64() {
	test_eval("16/64", "0.25");
}

#[test]
fn div_32_over_64() {
	test_eval("32/64", "0.5");
}

#[test]
fn div_64_over_64() {
	test_eval("64/64", "1");
}

#[test]
fn div_2_over_1() {
	test_eval("2/1", "2");
}

#[test]
fn div_27_over_3() {
	test_eval("27/3", "9");
}

#[test]
fn div_100_over_4() {
	test_eval("100/4", "25");
}

#[test]
fn div_100_over_5() {
	test_eval("100/5", "20");
}

#[test]
fn div_large_1() {
	test_eval("18446744073709551616/2", "9223372036854775808");
}

#[test]
fn div_large_2() {
	test_eval(
		"184467440737095516160000000000000/2",
		"92233720368547758080000000000000",
	);
}

#[test]
fn div_exact_pi() {
	test_eval("(3pi) / (2pi)", "1.5");
}

#[test]
fn zero_point_zero() {
	test_eval("0.0", "0");
}

#[test]
fn zero_point_multiple_zeroes() {
	test_eval("0.000000", "0");
}

#[test]
fn zero_point_zero_one() {
	test_eval("0.01", "0.01");
}

#[test]
fn zero_point_zero_one_zeroes() {
	test_eval("0.01000", "0.01");
}

#[test]
fn zero_point_two_five() {
	test_eval("0.25", "0.25");
}

#[test]
fn one_point() {
	expect_error("1.", None);
}

#[test]
fn point_one() {
	test_eval(".1", "0.1");
}

#[test]
fn point_one_e_minus_one() {
	test_eval(".1e-1", "0.01");
}

#[test]
fn leading_zeroes_with_dp() {
	test_eval("001.01000", "1.01");
}

#[test]
fn very_long_decimal() {
	test_eval(
		"0.251974862348971623412341534273261435",
		"0.251974862348971623412341534273261435",
	);
}

#[test]
fn one_point_zeroes_1_as_1_dp() {
	test_eval("1.00000001 as 1 dp", "approx. 1");
}

#[test]
fn one_point_zeroes_1_as_2_dp() {
	test_eval("1.00000001 as 2 dp", "approx. 1");
}

#[test]
fn one_point_zeroes_1_as_3_dp() {
	test_eval("1.00000001 as 3 dp", "approx. 1");
}

#[test]
fn one_point_zeroes_1_as_4_dp() {
	test_eval("1.00000001 as 4 dp", "approx. 1");
}

#[test]
fn one_point_zeroes_1_as_10_dp() {
	test_eval("1.00000001 as 10 dp", "1.00000001");
}

#[test]
fn one_point_zeroes_1_as_30_dp() {
	test_eval("1.00000001 as 30 dp", "1.00000001");
}

#[test]
fn one_point_zeroes_1_as_1000_dp() {
	test_eval("1.00000001 as 1000 dp", "1.00000001");
}

#[test]
fn one_point_zeroes_1_as_0_dp() {
	test_eval("1.00000001 as 0 dp", "approx. 1");
}

#[test]
fn point_1_zero_recurring() {
	test_eval(".1(0)", "0.1");
}

#[test]
fn recurring_product_whitespace_1() {
	test_eval(".1( 0)", "0");
}

#[test]
fn recurring_product_whitespace_2() {
	test_eval(".1 ( 0)", "0");
}

#[test]
fn point_1_zero_recurring_whitespace_error() {
	expect_error(".1(0 )", None);
}

#[test]
fn point_1_zero_recurring_letters_error() {
	expect_error(".1(0a)", None);
}

#[test]
fn recurring_product_with_e() {
	test_eval("2.0(e)", "approx. 5.4365636569");
}

#[test]
fn recurring_product_with_function() {
	test_eval("2.0(ln 5)", "approx. 3.2188758248");
}

#[test]
fn integer_product_whitespace_1() {
	test_eval("2 (5)", "10");
}

#[test]
fn integer_product_whitespace_2() {
	test_eval("2( 5)", "10");
}

#[test]
fn large_division() {
	test_eval(
		"60153992292001127921539815855494266880 / 9223372036854775808",
		"6521908912666391110",
	);
}

#[test]
fn parentheses_1() {
	test_eval("(1)", "1");
}

#[test]
fn parentheses_2() {
	test_eval("(0.0)", "0");
}

#[test]
fn parentheses_3() {
	test_eval("(1+-2)", "-1");
}

#[test]
fn parentheses_4() {
	test_eval("1+2*3", "7");
}

#[test]
fn parentheses_5() {
	test_eval("(1+2)*3", "9");
}

#[test]
fn parentheses_6() {
	test_eval("((1+2))*3", "9");
}

#[test]
fn parentheses_7() {
	test_eval("((1)+2)*3", "9");
}

#[test]
fn parentheses_8() {
	test_eval("(1+(2))*3", "9");
}

#[test]
fn parentheses_9() {
	test_eval("(1+(2)*3)", "7");
}

#[test]
fn parentheses_10() {
	test_eval("1+(2*3)", "7");
}

#[test]
fn parentheses_11() {
	test_eval("1+((2 )*3)", "7");
}

#[test]
fn parentheses_12() {
	test_eval(" 1 + ( (\r\n2 ) * 3 ) ", "7");
}

#[test]
fn parentheses_13() {
	test_eval("2*(1+3", "8");
}

#[test]
fn parentheses_14() {
	test_eval("4+5+6)*(1+2", "45");
}

#[test]
fn parentheses_15() {
	test_eval("4+5+6))*(1+2", "45");
}

#[test]
fn powers_1() {
	test_eval("1^1", "1");
}

#[test]
fn powers_2() {
	test_eval("1**1", "1");
}

#[test]
fn powers_3() {
	test_eval("1**1.0", "1");
}

#[test]
fn powers_4() {
	test_eval("1.0**1", "1");
}

#[test]
fn powers_5() {
	test_eval("2^4", "16");
}

#[test]
fn powers_6() {
	test_eval("4^2", "16");
}

#[test]
fn powers_7() {
	test_eval("4^3", "64");
}

#[test]
fn powers_8() {
	test_eval("4^(3^1)", "64");
}

#[test]
fn powers_9() {
	test_eval("4^3^1", "64");
}

#[test]
fn powers_10() {
	test_eval("(4^3)^1", "64");
}

#[test]
fn powers_11() {
	test_eval("(2^3)^4", "4096");
}

#[test]
fn powers_12() {
	test_eval("2^3^2", "512");
}

#[test]
fn powers_13() {
	test_eval("(2^3)^2", "64");
}

#[test]
fn powers_14() {
	test_eval("4^0.5", "2");
}

#[test]
fn powers_15() {
	test_eval("4^(1/2)", "2");
}

#[test]
fn powers_16() {
	test_eval("4^(1/4)", "approx. 1.4142135619");
}

#[test]
fn powers_17() {
	test_eval("(2/3)^(4/5)", "approx. 0.7229811807");
}

#[test]
fn powers_18() {
	test_eval(
		"5.2*10^15*300^(3/2)",
		"approx. 27019992598076723515.9873962402",
	);
}

#[test]
fn pi_to_the_power_of_ten() {
	test_eval("pi^10", "approx. 93648.047476083");
}

#[test]
fn zero_to_the_power_of_zero() {
	expect_error("0^0", None);
}

#[test]
fn zero_to_the_power_of_one() {
	test_eval("0^1", "0");
}

#[test]
fn one_to_the_power_of_zero() {
	test_eval("1^0", "1");
}

#[test]
fn one_to_the_power_of_huge_exponent() {
	test_eval("1^1e1000", "1");
}

#[test]
fn exponent_too_large() {
	expect_error("2^1e1000", None);
}

#[test]
fn i_powers() {
	for (i, result) in (0..=100).zip(["1", "i", "-1", "-i"].iter().cycle()) {
		test_eval(&format!("i^{}", i), result);
	}
}

#[test]
fn four_to_the_power_of_i() {
	test_eval("4^i", "approx. 0.1834569747 + 0.9830277404i");
}

#[test]
fn i_to_the_power_of_i() {
	test_eval("i^i", "approx. 0.2078795763");
}

#[test]
fn unit_to_approx_power() {
	test_eval("kg^(approx. 1)", "approx. 1 kg");
}

#[test]
fn negative_decimal() {
	test_eval("-0.125", "-0.125");
}

#[test]
fn two_pow_one_pow_two() {
	test_eval("2^1^2", "2");
}

#[test]
fn two_pow_one_parens_one_pow_two() {
	test_eval("2^(1^2)", "2");
}

#[test]
fn two_pow_parens_one() {
	test_eval("2^(1)", "2");
}

#[test]
fn negative_power_1() {
	test_eval("2 * (-2^3)", "-16");
}

#[test]
fn negative_power_2() {
	test_eval("2 * -2^3", "-16");
}

#[test]
fn negative_power_3() {
	test_eval("2^-3 * 4", "0.5");
}

#[test]
fn negative_power_4() {
	test_eval("2^3 * 4", "32");
}

#[test]
fn negative_power_5() {
	test_eval("-2^-3", "-0.125");
}

#[test]
fn negative_product() {
	test_eval("2 * -3 * 4", "-24");
}

#[test]
fn negative_power_6() {
	test_eval("4^-1^2", "0.25");
}

#[test]
fn negative_power_7() {
	test_eval(
		"2^-3^4",
		"0.000000000000000000000000413590306276513837435704346034981426782906055450439453125",
	);
}

#[test]
fn i() {
	test_eval("i", "i");
}

#[test]
fn three_i() {
	test_eval("3i", "3i");
}

#[test]
fn three_i_plus_four() {
	test_eval("3i+4", "4 + 3i");
}

#[test]
fn three_i_plus_four_plus_i() {
	test_eval("(3i+4) + i", "4 + 4i");
}

#[test]
fn three_i_plus_four_plus_i_2() {
	test_eval("3i+(4 + i)", "4 + 4i");
}

#[test]
fn minus_three_i() {
	test_eval("-3i", "-3i");
}

#[test]
fn i_over_i() {
	test_eval("i/i", "1");
}

#[test]
fn i_times_i() {
	test_eval("i*i", "-1");
}

#[test]
fn i_times_i_times_i() {
	test_eval("i*i*i", "-i");
}

#[test]
fn i_times_i_times_i_times_i() {
	test_eval("i*i*i*i", "1");
}

#[test]
fn minus_three_plus_i() {
	test_eval("-3+i", "-3 + i");
}

#[test]
fn i_plus_i() {
	test_eval("1+i", "1 + i");
}

#[test]
fn i_minus_i() {
	test_eval("1-i", "1 - i");
}

#[test]
fn minus_one_plus_i() {
	test_eval("-1 + i", "-1 + i");
}

#[test]
fn minus_one_minus_i() {
	test_eval("-1 - i", "-1 - i");
}

#[test]
fn minus_one_minus_two_i() {
	test_eval("-1 - 2i", "-1 - 2i");
}

#[test]
fn minus_one_minus_half_i() {
	test_eval("-1 - 0.5i", "-1 - 0.5i");
}

#[test]
fn minus_one_minus_half_i_plus_half_i() {
	test_eval("-1 - 0.5i + 1.5i", "-1 + i");
}

#[test]
fn minus_i() {
	test_eval("-i", "-i");
}

#[test]
fn plus_i() {
	test_eval("+i", "i");
}

#[test]
fn two_i() {
	test_eval("2i", "2i");
}

#[test]
fn i_over_3() {
	test_eval("i/3", "i/3");
}

#[test]
fn two_i_over_three() {
	test_eval("2i/3", "2i/3");
}

#[test]
fn two_i_over_minus_three_minus_one() {
	test_eval("2i/-3-1", "-1 - 2i/3");
}

#[test]
fn i_is_not_a_binary_digit() {
	expect_error("2#i", None);
}

#[test]
fn digit_separators_1() {
	test_eval("1_1", "11");
}

#[test]
fn digit_separators_2() {
	test_eval("11_1", "111");
}

#[test]
fn digit_separators_3() {
	test_eval("1_1_1", "111");
}

#[test]
fn digit_separators_4() {
	test_eval("123_456_789_123", "123456789123");
}

#[test]
fn digit_separators_5() {
	test_eval("1_2_3_4_5_6", "123456");
}

#[test]
fn digit_separators_6() {
	test_eval("1.1_1", "1.11");
}

#[test]
fn digit_separators_7() {
	test_eval("1_1.1_1", "11.11");
}

#[test]
fn digit_separators_8() {
	expect_error("_1", None);
}

#[test]
fn digit_separators_9() {
	expect_error("1_", None);
}

#[test]
fn digit_separators_10() {
	expect_error("1__1", None);
}

#[test]
fn digit_separators_11() {
	expect_error("_", None);
}

#[test]
fn digit_separators_12() {
	expect_error("1_.1", None);
}

#[test]
fn digit_separators_13() {
	expect_error("1._1", None);
}

#[test]
fn digit_separators_14() {
	expect_error("1.1_", None);
}

#[test]
fn digit_separators_15() {
	test_eval("1,1", "11");
}

#[test]
fn digit_separators_16() {
	test_eval("11,1", "111");
}

#[test]
fn digit_separators_17() {
	test_eval("1,1,1", "111");
}

#[test]
fn digit_separators_18() {
	test_eval("123,456,789,123", "123456789123");
}

#[test]
fn digit_separators_19() {
	test_eval("1,2,3,4,5,6", "123456");
}

#[test]
fn digit_separators_20() {
	test_eval("1.1,1", "1.11");
}

#[test]
fn digit_separators_21() {
	test_eval("1,1.1,1", "11.11");
}

#[test]
fn digit_separators_22() {
	expect_error(",1", None);
}

#[test]
fn digit_separators_23() {
	expect_error("1,", None);
}

#[test]
fn digit_separators_24() {
	expect_error("1,,1", None);
}

#[test]
fn digit_separators_25() {
	expect_error(",", None);
}

#[test]
fn digit_separators_26() {
	expect_error("1,.1", None);
}

#[test]
fn digit_separators_27() {
	expect_error("1.,1", None);
}

#[test]
fn digit_separators_28() {
	expect_error("1.1,", None);
}

#[test]
fn different_base_1() {
	test_eval("0x10", "0x10");
}

#[test]
fn different_base_2() {
	test_eval("0o10", "0o10");
}

#[test]
fn different_base_3() {
	test_eval("0b10", "0b10");
}

#[test]
fn different_base_4() {
	test_eval("0x10 - 1", "0xf");
}

#[test]
fn different_base_5() {
	test_eval("0x0 + sqrt 16", "0x4");
}

#[test]
fn different_base_6() {
	test_eval("16#0 + sqrt 16", "16#4");
}

#[test]
fn different_base_7() {
	test_eval("0 + 6#100", "36");
}

#[test]
fn different_base_8() {
	test_eval("0 + 36#z", "35");
}

#[test]
fn different_base_9() {
	test_eval("16#dead_beef", "16#deadbeef");
}

#[test]
fn different_base_10() {
	test_eval("16#DEAD_BEEF", "16#deadbeef");
}

#[test]
fn different_base_potential_dice_overlap() {
	test_eval("16#D3AD_BEEF", "16#d3adbeef");
}

#[test]
fn different_base_11() {
	expect_error("#", None);
}

#[test]
fn different_base_12() {
	expect_error("0#0", None);
}

#[test]
fn different_base_13() {
	expect_error("1#0", None);
}

#[test]
fn different_base_14() {
	expect_error("2_2#0", None);
}

#[test]
fn different_base_15() {
	expect_error("22 #0", None);
}

#[test]
fn different_base_16() {
	expect_error("22# 0", None);
}

#[test]
fn different_base_17() {
	test_eval("36#i i", "36#i i");
}

#[test]
fn different_base_18() {
	test_eval("16#1 i", "16#1 i");
}

#[test]
fn different_base_19() {
	test_eval("16#f i", "16#f i");
}

#[test]
fn different_base_20() {
	test_eval("0 + 36#ii", "666");
}

#[test]
fn different_base_21() {
	expect_error("18#i/i", None);
}

#[test]
fn different_base_22() {
	test_eval("19#i/i", "-19#i i");
}

// verified using a ruby program
#[test]
fn different_base_23() {
	test_eval(
		"0+36#0123456789abcdefghijklmnopqrstuvwxyz",
		"86846823611197163108337531226495015298096208677436155",
	);
}

#[test]
fn different_base_24() {
	test_eval(
		"36#0 + 86846823611197163108337531226495015298096208677436155",
		"36#123456789abcdefghijklmnopqrstuvwxyz",
	);
}

#[test]
fn different_base_25() {
	test_eval("18#100/65537 i", "18#100i/18#b44h");
}

#[test]
fn different_base_26() {
	test_eval("19#100/65537 i", "19#100 i/19#9aa6");
}

#[test]
fn different_base_27() {
	expect_error("5 to base 1.5", None);
}

#[test]
fn different_base_28() {
	expect_error("5 to base pi", None);
}

#[test]
fn different_base_29() {
	expect_error("5 to base (0pi)", None);
}

#[test]
fn different_base_30() {
	expect_error("5 to base 1", None);
}

#[test]
fn different_base_31() {
	expect_error("5 to base (-5)", None);
}

#[test]
fn different_base_32() {
	expect_error("5 to base 1000000000", None);
}

#[test]
fn different_base_33() {
	expect_error("5 to base 100", None);
}

#[test]
fn different_base_34() {
	expect_error("5 to base i", None);
}

#[test]
fn different_base_35() {
	expect_error("5 to base kg", None);
}

#[test]
fn different_base_36() {
	expect_error("6#3e9", None);
}

#[test]
fn different_base_37() {
	expect_error("6#3e39", None);
}

#[test]
fn different_base_38() {
	test_eval("9#5i", "9#5i");
}

#[test]
fn three_electroncharge() {
	test_eval(
		"3 electron_charge",
		"0.0000000000000000004806529902 coulomb",
	);
}

#[test]
fn e_to_1() {
	test_eval("e to 1", "approx. 2.7182818284");
}

#[test]
fn e_in_binary() {
	test_eval("e in binary", "approx. 10.1011011111");
}

#[test]
fn base_conversion_1() {
	test_eval("16 to base 2", "10000");
}

#[test]
fn base_conversion_2() {
	test_eval("0x10ffff to decimal", "1114111");
}

#[test]
fn base_conversion_3() {
	test_eval("0o400 to decimal", "256");
}

#[test]
fn base_conversion_4() {
	test_eval("100 to base 6", "244");
}

#[test]
fn base_conversion_5() {
	test_eval("65536 to hex", "10000");
}

#[test]
fn base_conversion_6() {
	test_eval("65536 to octal", "200000");
}

#[test]
fn exponents_1() {
	test_eval("1e10", "10000000000");
}

#[test]
fn exponents_2() {
	test_eval("1.5e10", "15000000000");
}

#[test]
fn exponents_3() {
	test_eval("0b1e10", "0b100");
}

#[test]
fn exponents_4() {
	test_eval("0b1e+10", "0b100");
}

#[test]
fn exponents_5() {
	test_eval("0 + 0b1e100", "16");
}

#[test]
fn exponents_6() {
	test_eval("0 + 0b1e1000", "256");
}

#[test]
fn exponents_7() {
	test_eval("0 + 0b1e10000", "65536");
}

#[test]
fn exponents_8() {
	test_eval("0 + 0b1e100000", "4294967296");
}

#[test]
fn exponents_9() {
	test_eval("16#1e10", "16#1e10");
}

#[test]
fn exponents_10() {
	test_eval("10#1e10", "10#10000000000");
}

#[test]
fn exponents_11() {
	expect_error("11#1e10", None);
}

#[test]
fn binary_exponent() {
	test_eval(
		"0 + 0b1e10000000",
		"340282366920938463463374607431768211456",
	);
}

#[test]
fn exponents_12() {
	test_eval("1.5e-1", "0.15");
}

#[test]
fn exponents_13() {
	test_eval("1.5e0", "1.5");
}

#[test]
fn exponents_14() {
	test_eval("1.5e-0", "1.5");
}

#[test]
fn exponents_15() {
	test_eval("1.5e+0", "1.5");
}

#[test]
fn exponents_16() {
	test_eval("1.5e1", "15");
}

#[test]
fn exponents_17() {
	test_eval("1.5e+1", "15");
}

#[test]
fn exponents_18() {
	expect_error("1e- 1", None);
}

#[test]
fn exponents_19() {
	test_eval("0 + 0b1e-110", "0.015625");
}

#[test]
fn exponents_20() {
	test_eval("e", "approx. 2.7182818284");
}

#[test]
fn exponents_21() {
	test_eval("2 e", "approx. 5.4365636569");
}

#[test]
fn exponents_22() {
	test_eval("2e", "approx. 5.4365636569");
}

#[test]
fn exponents_23() {
	test_eval("2e/2", "approx. 2.7182818284");
}

#[test]
fn exponents_24() {
	test_eval("2e / 2", "approx. 2.7182818284");
}

#[test]
fn exponents_25() {
	expect_error("2e+", None);
}

#[test]
fn exponents_26() {
	expect_error("2e-", None);
}

#[test]
fn exponents_27() {
	expect_error("2ehello", None);
}

#[test]
fn exponents_28() {
	test_eval("e^10", "approx. 22026.4657948067");
}

#[test]
fn exponents_29() {
	test_eval("e^2.72", "approx. 15.1803222449");
}

#[test]
fn one_kg() {
	test_eval("1kg", "1 kg");
}

#[test]
fn one_g() {
	test_eval("1g", "1 g");
}

#[test]
fn one_kg_plus_one_g() {
	test_eval("1kg + 1g", "1.001 kg");
}

#[test]
fn one_kg_plus_100_g() {
	test_eval("1kg + 100g", "1.1 kg");
}

#[test]
fn zero_g_plus_1_kg_plus_100_g() {
	test_eval("0g + 1kg + 100g", "1100 g");
}

#[test]
fn zero_g_plus_1_kg() {
	test_eval("0g + 1kg", "1000 g");
}

#[test]
fn one_over_half_kg() {
	test_eval("1/0.5 kg", "2 kg");
}

#[test]
fn one_over_one_over_half_kg() {
	test_eval("1/(1/0.5 kg)", "0.5 kg^-1");
}

#[test]
fn cbrt_kg() {
	test_eval("cbrt (1kg)", "1 kg^(1/3)");
}

#[test]
fn one_kg_plug_i_g() {
	test_eval("1 kg + i g", "(1 + 0.001i) kg");
}

#[test]
fn abs_2() {
	test_eval("abs 2", "2");
}

#[test]
fn five_meters() {
	test_eval("5 m", "5 m");
}

#[test]
fn parentheses_multiplication() {
	test_eval("(4)(6)", "24");
}

#[test]
fn parentheses_multiplication_2() {
	test_eval("5(6)", "30");
}

#[test]
fn multiply_number_without_parentheses() {
	expect_error("(5)6", None);
}

#[test]
fn simple_adjacent_numbers() {
	expect_error("7165928\t761528765", None);
}

#[test]
fn three_feet_six_inches() {
	test_eval("3’6”", "3.5’");
}

#[test]
fn five_feet_twelve_inches() {
	test_eval("5 feet 12 inch", "6 feet");
}

#[test]
fn three_feet_six_inches_ascii() {
	test_eval("3'6\"", "3.5'");
}

#[test]
fn three_meters_15_cm() {
	test_eval("3 m 15 cm", "3.15 m");
}

#[test]
fn five_percent() {
	test_eval("5%", "5%");
}

#[test]
fn five_percent_to_percent() {
	test_eval("5% to %", "5%");
}

#[test]
fn five_percent_plus_point_one() {
	test_eval("5% + 0.1", "15%");
}

#[test]
fn five_percent_plus_one() {
	test_eval("5% + 1", "105%");
}

#[test]
fn point_one_plus_five_percent() {
	test_eval("0.1 + 5%", "0.15");
}

#[test]
fn one_plus_five_percent() {
	test_eval("1 + 5%", "1.05");
}

#[test]
fn five_percent_times_five_percent() {
	test_eval("5% * 5%", "0.25%");
}

#[test]
fn five_percent_times_kg() {
	test_eval("5% * 8 kg", "0.4 kg");
}

#[test]
fn five_percent_times_100() {
	test_eval("5% * 100", "500%");
}

#[test]
fn five_percent_of_100() {
	test_eval("5% of 100", "5");
}

#[test]
fn five_percent_of_200() {
	test_eval("2 + 5% of 200", "12");
}

#[test]
fn five_percent_of_200_2() {
	test_eval("(2 + 5)% of 200", "14");
}

#[test]
fn units_1() {
	test_eval("0m + 1kph * 1 hr", "1000 m");
}

#[test]
fn units_2() {
	test_eval("0GiB + 1GB", "0.931322574615478515625 GiB");
}

#[test]
fn units_3() {
	test_eval("0m/s + 1 km/hr", "approx. 0.2777777777 m / s");
}

#[test]
fn units_4() {
	test_eval("0m/s + i km/hr", "5i/18 m / s");
}

#[test]
fn units_5() {
	test_eval("0m/s + i kilometers per hour", "5i/18 m / s");
}

#[test]
fn units_6() {
	test_eval("0m/s + (1 + i) km/hr", "(5/18 + 5i/18) m / s");
}

#[test]
fn units_9() {
	test_eval("365.25 light days to ly", "1 ly");
}

#[test]
fn units_10() {
	test_eval("365.25 light days as ly", "1 ly");
}

#[test]
fn units_11() {
	test_eval("1 light year", "1 light_year");
}

#[test]
fn units_12() {
	expect_error("1 2 m", None);
}

#[test]
fn units_13() {
	test_eval("5pi", "approx. 15.7079632679");
}

#[test]
fn units_14() {
	test_eval("5 pi/2", "approx. 7.8539816339");
}

#[test]
fn units_15() {
	test_eval("5 i/2", "2.5i");
}

#[test]
fn units_22() {
	test_eval("1psi as kPa as 5dp", "approx. 6.89475 kPa");
}

#[test]
fn units_23() {
	test_eval("1NM to m", "1852 m");
}

#[test]
fn units_24() {
	test_eval("1NM + 1cm as m", "1852.01 m");
}

#[test]
fn units_25() {
	test_eval("1 m / (s kg cd)", "1 m s^-1 kg^-1 cd^-1");
}

#[test]
fn units_26() {
	test_eval("1 watt hour / lb", "1 watt hour / lb");
}

#[test]
fn units_27() {
	test_eval("4 watt hours / lb", "4 watt hours / lb");
}

#[test]
fn units_28() {
	test_eval("1 second second", "1 second^2");
}

#[test]
fn units_29() {
	test_eval("2 second seconds", "2 seconds^2");
}

#[test]
fn units_30() {
	test_eval("1 lb^-1", "1 lb^-1");
}

#[test]
fn units_31() {
	test_eval("2 lb^-1", "2 lb^-1");
}

#[test]
fn units_32() {
	test_eval("2 lb^-1 kg^-1", "0.90718474 lb^-2");
}

#[test]
fn units_33() {
	test_eval("1 lb^-1 kg^-1", "0.45359237 lb^-2");
}

#[test]
fn units_34() {
	test_eval("0.5 light year", "0.5 light_years");
}

#[test]
fn units_35() {
	test_eval("1 lightyear / second", "1 lightyear / second");
}

#[test]
fn units_36() {
	test_eval("2 lightyears / second", "2 lightyears / second");
}

#[test]
fn units_37() {
	test_eval(
		"2 lightyears second^-1 lb^-1",
		"2 lightyears second^-1 lb^-1",
	);
}

#[test]
fn units_38() {
	test_eval("1 feet", "1 foot");
}

#[test]
fn units_39() {
	test_eval("5 foot", "5 feet");
}

#[test]
fn units_40() {
	test_eval("5 foot 2 inches", "approx. 5.1666666666 feet");
}

#[test]
fn units_41() {
	test_eval("5 foot 1 inch 1 inch", "approx. 5.1666666666 feet");
}

#[test]
fn plain_adjacent_numbers() {
	expect_error("1 2", None);
}

#[test]
fn multiple_plain_adjacent_numbers() {
	expect_error("1 2 3 4 5", None);
}

#[test]
fn implicit_sum_missing_unit() {
	expect_error("1 inch 5", None);
}

#[test]
fn implicit_sum_incompatible_unit() {
	expect_error("1 inch 5 kg", None);
}

#[test]
fn too_many_args() {
	expect_error("abs 1 2", None);
}

#[test]
fn abs_4_with_coefficient() {
	test_eval("5 (abs 4)", "20");
}

#[test]
fn mixed_fraction_to_improper_fraction() {
	test_eval_simple("1 2/3 to fraction", "5/3");
}

#[test]
fn mixed_fractions_1() {
	test_eval("5/3", "approx. 1.6666666666");
}

#[test]
fn mixed_fractions_2() {
	test_eval("4 + 1 2/3", "approx. 5.6666666666");
}

#[test]
fn mixed_fractions_3() {
	test_eval("-8 1/2", "-8.5");
}

#[test]
fn mixed_fractions_4() {
	test_eval("-8 1/2'", "-8.5'");
}

#[test]
fn mixed_fractions_5() {
	test_eval("1.(3)i", "1 1/3 i");
}

#[test]
fn mixed_fractions_6() {
	test_eval("1*1 1/2", "1.5");
}

#[test]
fn mixed_fractions_7() {
	test_eval("2*1 1/2", "3");
}

#[test]
fn mixed_fractions_8() {
	test_eval("3*2*1 1/2", "9");
}

#[test]
fn mixed_fractions_9() {
	test_eval("3 + 2*1 1/2", "6");
}

#[test]
fn mixed_fractions_10() {
	test_eval("abs 2*1 1/2", "3");
}

#[test]
fn mixed_fractions_11() {
	expect_error("1/1 1/2", None);
}

#[test]
fn mixed_fractions_12() {
	expect_error("2/1 1/2", None);
}

#[test]
fn mixed_fractions_13() {
	test_eval("1 1/2 m/s^2", "1.5 m / s^2");
}

#[test]
fn mixed_fractions_14() {
	expect_error("(x:2x) 1 1/2", None);
}

#[test]
fn mixed_fractions_15() {
	expect_error("pi 1 1/2", None);
}

#[test]
fn lone_conversion_arrow() {
	expect_error("->", None);
}

#[test]
fn conversion_arrow_no_rhs() {
	expect_error("1m->", None);
}

#[test]
fn conversion_arrow_with_space_in_the_middle() {
	expect_error("1m - >", None);
}

#[test]
fn conversion_arrow_no_lhs() {
	expect_error("->1ft", None);
}

#[test]
fn meter_to_feet() {
	expect_error("1m -> 45ft", None);
}

#[test]
fn meter_to_kg_ft() {
	expect_error("1m -> 45 kg ft", None);
}

#[test]
fn one_foot_to_inches() {
	test_eval("1' to inches", "12 inches");
}

#[test]
fn abs_1() {
	test_eval("abs 1", "1");
}

#[test]
fn abs_i() {
	test_eval("abs i", "1");
}

#[test]
fn abs_minus_1() {
	test_eval("abs (-1)", "1");
}

#[test]
fn abs_minus_i() {
	test_eval("abs (-i)", "1");
}

#[test]
fn abs_2_i() {
	test_eval("abs (2i)", "2");
}

#[test]
fn abs_1_plus_i() {
	test_eval("abs (1 + i)", "approx. 1.4142135619");
}

#[test]
fn two_kg_squared() {
	test_eval("2 kg^2", "2 kg^2");
}

#[test]
fn quarter_kg_pow_minus_two() {
	test_eval("((1/4) kg)^-2", "16 kg^-2");
}

#[test]
fn newton_subtraction() {
	test_eval("1 N - 1 kg m s^-2", "0 N");
}

#[test]
fn joule_subtraction() {
	test_eval("1 J - 1 kg m^2 s^-2 + 1 kg / (m^-2 s^2)", "1 J");
}

#[test]
fn two_to_the_power_of_abs_one() {
	test_eval("2^abs 1", "2");
}

#[test]
fn adjacent_numbers_rhs_cubed() {
	expect_error("2 4^3", None);
}

#[test]
fn negative_adjacent_numbers_rhs_cubed() {
	expect_error("-2 4^3", None);
}

#[test]
fn product_with_unary_minus_1() {
	test_eval("3*-2", "-6");
}

#[test]
fn product_with_unary_minus_2() {
	test_eval("-3*-2", "6");
}

#[test]
fn product_with_unary_minus_3() {
	test_eval("-3*2", "-6");
}

#[test]
fn illegal_mixed_fraction_with_pow_1() {
	expect_error("1 2/3^2", None);
}

#[test]
fn illegal_mixed_fraction_with_pow_2() {
	expect_error("1 2^2/3", None);
}

#[test]
fn illegal_mixed_fraction_with_pow_3() {
	expect_error("1^2 2/3", None);
}

#[test]
fn illegal_mixed_fraction_with_pow_4() {
	expect_error("1 2/-3", None);
}

#[test]
fn positive_mixed_fraction_sum() {
	test_eval("1 2/3 + 4 5/6", "6.5");
}

#[test]
fn negative_mixed_fraction_sum() {
	test_eval("1 2/3 + -4 5/6", "approx. -3.1666666666");
}

#[test]
fn positive_mixed_fraction_subtraction() {
	test_eval("1 2/3 - 4 5/6", "approx. -3.1666666666");
}

#[test]
fn negative_mixed_fraction_subtraction() {
	test_eval("1 2/3 - 4 + 5/6", "-1.5");
}

#[test]
fn barn_to_meters_squared() {
	test_eval("1 barn to m^2", "0.0000000000000000000000000001 m^2");
}

#[test]
fn liter_to_cubic_meters() {
	test_eval("1L to m^3", "0.001 m^3");
}

#[test]
fn five_feet_to_meters() {
	test_eval("5 ft to m", "1.524 m");
}

#[test]
fn log10_4() {
	test_eval("log10 4", "approx. 0.6020599913");
}

#[test]
fn log_4_as_log10() {
	test_eval("log 4", "approx. 0.6020599913");
}

#[test]
fn factorial_of_0() {
	test_eval("0!", "1");
}

#[test]
fn factorial_of_1() {
	test_eval("1!", "1");
}

#[test]
fn factorial_of_2() {
	test_eval("2!", "2");
}

#[test]
fn factorial_of_3() {
	test_eval("3!", "6");
}

#[test]
fn factorial_of_4() {
	test_eval("4!", "24");
}

#[test]
fn factorial_of_5() {
	test_eval("5!", "120");
}

#[test]
fn factorial_of_6() {
	test_eval("6!", "720");
}

#[test]
fn factorial_of_7() {
	test_eval("7!", "5040");
}

#[test]
fn factorial_of_8() {
	test_eval("8!", "40320");
}

#[test]
fn factorial_of_half() {
	expect_error("0.5!", None);
}

#[test]
fn factorial_of_minus_two() {
	expect_error("(-2)!", None);
}

#[test]
fn factorial_of_three_i() {
	expect_error("3i!", None);
}

#[test]
fn factorial_of_three_kg() {
	expect_error("(3 kg)!", None);
}

#[test]
fn test_floor() {
	test_eval("floor(3)", "3");
	test_eval("floor(3.9)", "3");
	test_eval("floor(-3)", "-3");
	test_eval("floor(-3.1)", "-4");
}

#[test]
fn test_ceil() {
	test_eval("ceil(3)", "3");
	test_eval("ceil(3.3)", "4");
	test_eval("ceil(-3)", "-3");
	test_eval("ceil(-3.3)", "-3");
}

#[test]
fn test_round() {
	test_eval("round(3)", "3");

	test_eval("round(3.3)", "3");
	test_eval("round(3.7)", "4");

	test_eval("round(-3.3)", "-3");
	test_eval("round(-3.7)", "-4");
}

#[test]
fn recurring_digits_1() {
	test_eval_simple("9/11 to float", "0.(81)");
}

#[test]
fn recurring_digits_2() {
	test_eval_simple("6#1 / 11 to float", "6#0.(0313452421)");
}

#[test]
fn recurring_digits_3() {
	test_eval_simple("6#0 + 6#1 / 7 to float", "6#0.(05)");
}

#[test]
fn recurring_digits_4() {
	test_eval_simple("0.25 as fraction", "1/4");
}

#[test]
fn recurring_digits_5() {
	test_eval("0.21 as 1 dp", "approx. 0.2");
}

#[test]
fn recurring_digits_6() {
	test_eval("0.21 to 1 dp to auto", "0.21");
}

#[test]
fn recurring_digits_7() {
	test_eval_simple("502938/700 to float", "718.48(285714)");
}

#[test]
fn builtin_function_name_abs() {
	test_eval("abs", "abs");
}

#[test]
fn builtin_function_name_sin() {
	test_eval("sin", "sin");
}

#[test]
fn builtin_function_name_cos() {
	test_eval("cos", "cos");
}

#[test]
fn builtin_function_name_tan() {
	test_eval("tan", "tan");
}

#[test]
fn builtin_function_name_asin() {
	test_eval("asin", "asin");
}

#[test]
fn builtin_function_name_acos() {
	test_eval("acos", "acos");
}

#[test]
fn builtin_function_name_atan() {
	test_eval("atan", "atan");
}

#[test]
fn builtin_function_name_sinh() {
	test_eval("sinh", "sinh");
}

#[test]
fn builtin_function_name_cosh() {
	test_eval("cosh", "cosh");
}

#[test]
fn builtin_function_name_tanh() {
	test_eval("tanh", "tanh");
}

#[test]
fn builtin_function_name_asinh() {
	test_eval("asinh", "asinh");
}

#[test]
fn builtin_function_name_acosh() {
	test_eval("acosh", "acosh");
}

#[test]
fn builtin_function_name_atanh() {
	test_eval("atanh", "atanh");
}

#[test]
fn builtin_function_name_ln() {
	test_eval("ln", "ln");
}

#[test]
fn builtin_function_name_log2() {
	test_eval("log2", "log2");
}

#[test]
fn builtin_function_name_log10() {
	test_eval("log10", "log10");
}

#[test]
fn builtin_function_name_log_is_log10() {
	test_eval("log", "log10");
}

#[test]
fn builtin_function_name_base() {
	test_eval("base", "base");
}

// values from https://en.wikipedia.org/wiki/Exact_trigonometric_values
#[test]
fn sin_0() {
	test_eval("sin 0", "0");
}

#[test]
fn sin_1() {
	test_eval("sin 1", "approx. 0.8414709848");
}

#[test]
fn sin_1_percent() {
	test_eval("sin (1%)", "approx. 0.0099998333");
}

#[test]
fn atan_1_percent() {
	test_eval("atan (1%)", "approx. 0.0099996666");
}

#[test]
fn sin_pi() {
	test_eval("sin pi", "0");
}

#[test]
fn sin_2_pi() {
	test_eval("sin (2pi)", "0");
}

#[test]
fn sin_minus_pi() {
	test_eval("sin (-pi)", "0");
}

#[test]
fn sin_minus_1000_pi() {
	test_eval("sin (-1000pi)", "0");
}

#[test]
fn sin_pi_over_2() {
	test_eval("sin (pi/2)", "1");
}

#[test]
fn sin_3_pi_over_2() {
	test_eval("sin (3pi/2)", "-1");
}

#[test]
fn sin_5_pi_over_2() {
	test_eval("sin (5pi/2)", "1");
}

#[test]
fn sin_7_pi_over_2() {
	test_eval("sin (7pi/2)", "-1");
}

#[test]
fn sin_minus_pi_over_2() {
	test_eval("sin (-pi/2)", "-1");
}

#[test]
fn sin_minus_3_pi_over_2() {
	test_eval("sin (-3pi/2)", "1");
}

#[test]
fn sin_minus_5_pi_over_2() {
	test_eval("sin (-5pi/2)", "-1");
}

#[test]
fn sin_minus_7_pi_over_2() {
	test_eval("sin (-7pi/2)", "1");
}

#[test]
fn sin_minus_1023_pi_over_2() {
	test_eval("sin (-1023pi/2)", "1");
}

#[test]
fn sin_pi_over_6() {
	test_eval("sin (pi/6)", "0.5");
}

#[test]
fn sin_5_pi_over_6() {
	test_eval("sin (5pi/6)", "0.5");
}

#[test]
fn sin_7_pi_over_6() {
	test_eval("sin (7pi/6)", "-0.5");
}

#[test]
fn sin_11_pi_over_6() {
	test_eval("sin (11pi/6)", "-0.5");
}

#[test]
fn sin_minus_pi_over_6() {
	test_eval("sin (-pi/6)", "-0.5");
}

#[test]
fn sin_minus_5_pi_over_6() {
	test_eval("sin (-5pi/6)", "-0.5");
}

#[test]
fn sin_minus_7_pi_over_6() {
	test_eval("sin (-7pi/6)", "0.5");
}

#[test]
fn sin_minus_11_pi_over_6() {
	test_eval("sin (-11pi/6)", "0.5");
}

#[test]
fn sin_180_degrees() {
	test_eval("sin (180°)", "0");
}

#[test]
fn sin_30_degrees() {
	test_eval("sin (30°)", "0.5");
}

#[test]
fn sin_one_degree() {
	test_eval("sin (1°)", "approx. 0.0174524064");
}

#[test]
fn cos_0() {
	test_eval("cos 0", "1");
}

#[test]
fn cos_1() {
	test_eval("cos 1", "approx. 0.5403023058");
}

#[test]
fn cos_pi() {
	test_eval("cos pi", "-1");
}

#[test]
fn cos_2_pi() {
	test_eval("cos (2pi)", "1");
}

#[test]
fn cos_minus_pi() {
	test_eval("cos (-pi)", "-1");
}

#[test]
fn cos_minus_1000_pi() {
	test_eval("cos (-1000pi)", "1");
}

#[test]
fn cos_pi_over_2() {
	test_eval("cos (pi/2)", "0");
}

#[test]
fn cos_3_pi_over_2() {
	test_eval("cos (3pi/2)", "0");
}

#[test]
fn cos_5_pi_over_2() {
	test_eval("cos (5pi/2)", "0");
}

#[test]
fn cos_7_pi_over_2() {
	test_eval("cos (7pi/2)", "0");
}

#[test]
fn cos_minus_pi_over_2() {
	test_eval("cos (-pi/2)", "0");
}

#[test]
fn cos_minus_3_pi_over_2() {
	test_eval("cos (-3pi/2)", "0");
}

#[test]
fn cos_minus_5_pi_over_2() {
	test_eval("cos (-5pi/2)", "0");
}

#[test]
fn cos_minus_7_pi_over_2() {
	test_eval("cos (-7pi/2)", "0");
}

#[test]
fn cos_minus_1023_pi_over_2() {
	test_eval("cos (-1023pi/2)", "0");
}

#[test]
fn cos_pi_over_3() {
	test_eval("cos (pi/3)", "0.5");
}

#[test]
fn cos_2_pi_over_3() {
	test_eval("cos (2pi/3)", "-0.5");
}

#[test]
fn cos_4_pi_over_3() {
	test_eval("cos (4pi/3)", "-0.5");
}

#[test]
fn cos_5_pi_over_3() {
	test_eval("cos (5pi/3)", "0.5");
}

#[test]
fn cos_minus_pi_over_3() {
	test_eval("cos (-pi/3)", "0.5");
}

#[test]
fn cos_minus_2_pi_over_3() {
	test_eval("cos (-2pi/3)", "-0.5");
}

#[test]
fn cos_minus_4_pi_over_3() {
	test_eval("cos (-4pi/3)", "-0.5");
}

#[test]
fn cos_minus_5_pi_over_3() {
	test_eval("cos (-5pi/3)", "0.5");
}

#[test]
fn tau() {
	test_eval("tau", "approx. 6.2831853071");
}

#[test]
fn sin_tau_over_two() {
	test_eval("sin (tau / 2)", "0");
}

#[test]
fn greek_pi_symbol() {
	test_eval("π", "approx. 3.1415926535");
}

#[test]
fn greek_tau_symbol() {
	test_eval("τ", "approx. 6.2831853071");
}

#[test]
fn tan_0() {
	test_eval("tan 0", "0");
}

#[test]
fn tan_pi() {
	test_eval("tan pi", "0");
}

#[test]
fn tan_2pi() {
	test_eval("tan (2pi)", "0");
}

#[test]
fn asin_1() {
	test_eval("asin 1", "approx. 1.5707963267");
}

#[test]
fn asin_3() {
	test_eval("asin 3", "approx. 1.5707963267 - 1.762747174i");
}

#[test]
fn asin_minus_3() {
	test_eval("asin (-3)", "approx. -1.5707963267 + 1.762747174i");
}

#[test]
fn asin_one_point_zero_one() {
	test_eval("asin 1.01", "approx. 1.5707963267 - 0.1413037694i");
}

#[test]
fn asin_minus_one_point_zero_one() {
	test_eval("asin (-1.01)", "approx. -1.5707963267 + 0.1413037694i");
}

#[test]
fn acos_0() {
	test_eval("acos 0", "approx. 1.5707963267");
}

#[test]
fn acos_3() {
	test_eval_simple("acos 3", "approx. 0 + 1.762747174i");
}

#[test]
fn acos_minus_3() {
	test_eval("acos (-3)", "approx. 3.1415926535 - 1.762747174i");
}

#[test]
fn acos_one_point_zero_one() {
	test_eval_simple("acos 1.01", "approx. 0 + 0.1413037694i");
}

#[test]
fn acos_minus_one_point_zero_one() {
	test_eval("acos (-1.01)", "approx. 3.1415926535 - 0.1413037694i");
}

#[test]
fn acos_one() {
	test_eval("acos 1", "approx. 0");
}

#[test]
fn acos_minus_one() {
	test_eval("acos (-1)", "approx. 3.1415926535");
}

#[test]
fn atan_1() {
	test_eval("atan 1", "approx. 0.7853981633");
}

#[test]
fn sinh_0() {
	test_eval("sinh 0", "approx. 0");
}

#[test]
fn cosh_0() {
	test_eval("cosh 0", "approx. 1");
}

#[test]
fn tanh_0() {
	test_eval("tanh 0", "approx. 0");
}

#[test]
fn asinh_0() {
	test_eval("asinh 0", "approx. 0");
}

#[test]
fn acosh_0() {
	test_eval("acosh 0", "approx. 1.5707963267i");
}

#[test]
fn acosh_2() {
	test_eval("acosh 2", "approx. 1.3169578969");
}

#[test]
fn atanh_0() {
	test_eval("atanh 0", "approx. 0");
}

#[test]
fn atanh_3() {
	test_eval("atanh 3", "approx. 0.3465735902 + 1.5707963267i");
}

#[test]
fn atanh_minus_3() {
	test_eval("atanh (-3)", "approx. -0.3465735902 + 1.5707963267i");
}

#[test]
fn atanh_one_point_zero_one() {
	test_eval("atanh 1.01", "approx. 2.651652454 + 1.5707963267i");
}

#[test]
fn atanh_minus_one_point_zero_one() {
	test_eval("atanh (-1.01)", "approx. -2.651652454 + 1.5707963267i");
}

#[test]
fn atanh_1() {
	expect_error("atanh 1", None);
}

#[test]
fn atanh_minus_1() {
	expect_error("atanh (-1)", None);
}

#[test]
fn ln_2() {
	test_eval("ln 2", "approx. 0.6931471805");
}

#[test]
fn ln_0() {
	expect_error("ln 0", None);
}

#[test]
fn exp_2() {
	test_eval("exp 2", "approx. 7.3890560989");
}

#[test]
fn log10_100() {
	test_eval("log10 100", "approx. 2");
}

#[test]
fn log10_1000() {
	test_eval("log10 1000", "approx. 3");
}

#[test]
fn log10_10000() {
	test_eval("log10 10000", "approx. 3.9999999999");
}

#[test]
fn log10_100000() {
	test_eval("log10 100000", "approx. 5");
}

#[test]
fn log_100() {
	test_eval("log 100", "approx. 2");
}

#[test]
fn log_1000() {
	test_eval("log 1000", "approx. 3");
}

#[test]
fn log_10000() {
	test_eval("log 10000", "approx. 3.9999999999");
}

#[test]
fn log_100000() {
	test_eval("log 100000", "approx. 5");
}

#[test]
fn log2_65536() {
	test_eval("log2 65536", "approx. 16");
}

#[test]
fn log2_2_2048() {
	test_eval("log2(2^2048)", "approx. 2048");
}

#[test]
fn log2_3_2_2048() {
	test_eval("log2(3*2^2048)", "approx. 2049.5849625007");
}

#[test]
fn log10_minus_1() {
	test_eval("log10 (-1)", "approx. 1.3643763538i");
}

#[test]
fn log2_minus_1() {
	test_eval("log2 (-1)", "approx. 4.5323601418i");
}

#[test]
fn sqrt_minus_two() {
	test_eval_simple("sqrt(-2)", "approx. 0 + 1.4142135623i");
}

#[test]
fn minus_two_cubed() {
	test_eval("(-2)^3", "-8");
}

#[test]
fn minus_two_pow_five() {
	test_eval("(-2)^5", "-32");
}

#[test]
fn two_pow_minus_two() {
	test_eval("2^-2", "0.25");
}

#[test]
fn minus_two_to_the_power_of_minus_two() {
	test_eval("(-2)^-2", "0.25");
}

#[test]
fn minus_two_to_the_power_of_minus_three() {
	test_eval("(-2)^-3", "-0.125");
}

#[test]
fn minus_two_to_the_power_of_minus_four() {
	test_eval("(-2)^-4", "0.0625");
}

#[test]
fn invalid_function_call() {
	expect_error("oishfod 3", None);
}

#[test]
fn ln() {
	test_eval("ln", "ln");
}

#[test]
fn dp() {
	test_eval("dp", "dp");
}

#[test]
fn ten_dp() {
	test_eval("10 dp", "10 dp");
}

#[test]
fn float() {
	test_eval("float", "float");
}

#[test]
fn fraction() {
	test_eval("fraction", "fraction");
}

#[test]
fn auto() {
	test_eval("auto", "auto");
}

#[test]
fn sqrt_i() {
	test_eval("sqrt i", "approx. 0.7071067811 + 0.7071067811i");
}

#[test]
fn sqrt_minus_two_i() {
	// FIXME: exactly 1 - i
	test_eval("sqrt (-2i)", "approx. 0.9999999999 - 0.9999999999i");
}

#[test]
fn cbrt_i() {
	// FIXME: exactly 0.8660 + 0.5i
	test_eval("cbrt i", "approx. 0.8660254037 + 0.4999999999i");
}

#[test]
fn cbrt_minus_two_i() {
	test_eval("cbrt (-2i)", "approx. 1.0911236359 - 0.6299605249i");
}

#[test]
fn sin_i() {
	test_eval("sin i", "approx. 1.1752011936i");
}

#[test]
fn cos_i() {
	test_eval("cos i", "approx. 1.5430806348");
}

#[test]
fn tan_i() {
	test_eval("tan i", "approx. 0.7615941559i");
}

#[test]
fn ln_i() {
	test_eval("ln i", "approx. 1.5707963267i");
}

#[test]
fn log2_i() {
	test_eval("log2 i", "approx. 2.2661800709i");
}

#[test]
fn log10_i() {
	test_eval("log10 i", "approx. 0.6821881769i");
}

#[test]
fn dp_1() {
	expect_error("dp 1", None);
}

#[test]
fn unary_div_seconds() {
	test_eval("/s", "1 s^-1");
}

#[test]
fn per_second() {
	test_eval("per second", "1 second^-1");
}

#[test]
fn hertz_plus_unary_div_seconds() {
	test_eval("1 Hz + /s", "2 Hz");
}

#[test]
fn lambda_1() {
	test_eval("(x: x) 1", "1");
}

#[test]
fn lambda_2() {
	test_eval("(x: y: x) 1 2", "1");
}

#[test]
fn lambda_3() {
	test_eval(
		"(cis: (cis (pi/3))) (x: cos x + i * (sin x))",
		"approx. 0.5 + 0.8660254037i",
	);
}

#[test]
fn lambda_4() {
	test_eval("(x: iuwhe)", "\\x.iuwhe");
}

#[test]
fn lambda_5() {
	test_eval("(b: 5 + b) 1", "6");
}

#[test]
fn lambda_6() {
	test_eval("(addFive: 4)(b: 5 + b)", "4");
}

#[test]
fn lambda_7() {
	test_eval("(addFive: addFive 4)(b: 5 + b)", "9");
}

#[test]
fn lambda_8() {
	test_eval("(x: y: z: x) 1 2 3", "1");
}

#[test]
fn lambda_9() {
	test_eval("(x: y: z: y) 1 2 3", "2");
}

#[test]
fn lambda_10() {
	test_eval("(x: y: z: z) 1 2 3", "3");
}

#[test]
fn lambda_11() {
	test_eval("(one: one + 4) 1", "5");
}

#[test]
fn lambda_12() {
	test_eval("(one: one + one) 1", "2");
}

#[test]
fn lambda_13() {
	test_eval("(x: x to kg) (5 g)", "0.005 kg");
}

#[test]
fn lambda_14() {
	test_eval("(p: q: p p q) (x: y: y) (x: y: y) 1 0", "0");
}

#[test]
fn lambda_15() {
	test_eval("(p: q: p p q) (x: y: y) (x: y: x) 1 0", "1");
}

#[test]
fn lambda_16() {
	test_eval("(p: q: p p q) (x: y: x) (x: y: y) 1 0", "1");
}

#[test]
fn lambda_17() {
	test_eval("(p: q: p p q) (x: y: x) (x: y: x) 1 0", "1");
}

#[test]
fn lambda_18() {
	test_eval("(x => x) 1", "1");
}

#[test]
fn lambda_19() {
	test_eval("(x: y => x) 1 2", "1");
}

#[test]
fn lambda_20() {
	test_eval("(\\x. y => x) 1 2", "1");
}

#[test]
fn lambda_21() {
	test_eval("(\\x.\\y.x)1 2", "1");
}

#[test]
fn lambda_22() {
	test_eval("a. => 0", "a.:0");
}

#[test]
fn unit_to_the_power_of_pi() {
	test_eval("kg^pi", "1 kg^π");
}

#[test]
fn more_complex_unit_power_of_pi() {
	test_eval("kg^(2pi) / kg^(2pi) to 1", "1");
}

#[test]
fn cis_0() {
	test_eval("cis 0", "1");
}

#[test]
fn cis_pi() {
	test_eval("cis pi", "-1");
}

#[test]
fn cis_half_pi() {
	test_eval("cis (pi/2)", "i");
}

#[test]
fn cis_three_pi_over_two() {
	test_eval("cis (3pi/2)", "-i");
}

#[test]
fn cis_two_pi() {
	test_eval("cis (2pi)", "1");
}

#[test]
fn cis_minus_two_pi() {
	test_eval("cis -(2pi)", "1");
}

#[test]
fn cis_pi_over_six() {
	test_eval("cis (pi/6)", "approx. 0.8660254037 + 0.5i");
}

#[test]
fn name_one() {
	test_eval("one", "1");
}

#[test]
fn name_two() {
	test_eval("two", "2");
}

#[test]
fn name_three() {
	test_eval("three", "3");
}

#[test]
fn name_four() {
	test_eval("four", "4");
}

#[test]
fn name_five() {
	test_eval("five", "5");
}

#[test]
fn name_six() {
	test_eval("six", "6");
}

#[test]
fn name_seven() {
	test_eval("seven", "7");
}

#[test]
fn name_eight() {
	test_eval("eight", "8");
}

#[test]
fn name_nine() {
	test_eval("nine", "9");
}

#[test]
fn name_ten() {
	test_eval("ten", "10");
}

#[test]
fn name_eleven() {
	test_eval("eleven", "11");
}

#[test]
fn name_twelve() {
	test_eval("twelve", "12");
}

#[test]
fn name_thirteen() {
	test_eval("thirteen", "13");
}

#[test]
fn name_fourteen() {
	test_eval("fourteen", "14");
}

#[test]
fn name_fifteen() {
	test_eval("fifteen", "15");
}

#[test]
fn name_sixteen() {
	test_eval("sixteen", "16");
}

#[test]
fn name_seventeen() {
	test_eval("seventeen", "17");
}

#[test]
fn name_eighteen() {
	test_eval("eighteen", "18");
}

#[test]
fn name_nineteen() {
	test_eval("nineteen", "19");
}

#[test]
fn name_twenty() {
	test_eval("twenty", "20");
}

#[test]
fn name_thirty() {
	test_eval("thirty", "30");
}

#[test]
fn name_forty() {
	test_eval("forty", "40");
}

#[test]
fn name_fifty() {
	test_eval("fifty", "50");
}

#[test]
fn name_sixty() {
	test_eval("sixty", "60");
}

#[test]
fn name_seventy() {
	test_eval("seventy", "70");
}

#[test]
fn name_eighty() {
	test_eval("eighty", "80");
}

#[test]
fn name_ninety() {
	test_eval("ninety", "90");
}

#[test]
fn name_hundred() {
	test_eval("hundred", "100");
}

#[test]
fn name_thousand() {
	test_eval("thousand", "1000");
}

#[test]
fn name_million() {
	test_eval("million", "1000000");
}

#[test]
fn name_dozen() {
	test_eval("dozen", "12");
}

#[test]
fn name_one_dozen() {
	test_eval("one dozen", "12");
}

#[test]
fn name_two_dozen() {
	test_eval("two dozen", "24");
}

#[test]
fn name_three_dozen() {
	test_eval("three dozen", "36");
}

#[test]
fn name_four_dozen() {
	test_eval("four dozen", "48");
}

#[test]
fn name_five_dozen() {
	test_eval("five dozen", "60");
}

#[test]
fn name_six_dozen() {
	test_eval("six dozen", "72");
}

#[test]
fn name_seven_dozen() {
	test_eval("seven dozen", "84");
}

#[test]
fn name_eight_dozen() {
	test_eval("eight dozen", "96");
}

#[test]
fn name_nine_dozen() {
	test_eval("nine dozen", "108");
}

#[test]
fn name_ten_dozen() {
	test_eval("ten dozen", "120");
}

#[test]
fn name_eleven_dozen() {
	test_eval("eleven dozen", "132");
}

#[test]
fn name_twelve_dozen() {
	test_eval("twelve dozen", "144");
}

#[test]
fn name_gross() {
	test_eval("gross", "144");
}

#[test]
fn name_thirteen_dozen() {
	test_eval("thirteen dozen", "156");
}

#[test]
fn name_fourteen_dozen() {
	test_eval("fourteen dozen", "168");
}

#[test]
fn name_fifteen_dozen() {
	test_eval("fifteen dozen", "180");
}

#[test]
fn name_sixteen_dozen() {
	test_eval("sixteen dozen", "192");
}

#[test]
fn name_seventeen_dozen() {
	test_eval("seventeen dozen", "204");
}

#[test]
fn name_eighteen_dozen() {
	test_eval("eighteen dozen", "216");
}

#[test]
fn name_nineteen_dozen() {
	test_eval("nineteen dozen", "228");
}

#[test]
fn name_twenty_dozen() {
	test_eval("twenty dozen", "240");
}

#[test]
fn name_thirty_dozen() {
	test_eval("thirty dozen", "360");
}

#[test]
fn name_forty_dozen() {
	test_eval("forty dozen", "480");
}

#[test]
fn name_fifty_dozen() {
	test_eval("fifty dozen", "600");
}

#[test]
fn name_sixty_dozen() {
	test_eval("sixty dozen", "720");
}

#[test]
fn name_seventy_dozen() {
	test_eval("seventy dozen", "840");
}

#[test]
fn name_eighty_dozen() {
	test_eval("eighty dozen", "960");
}

#[test]
fn name_ninety_dozen() {
	test_eval("ninety dozen", "1080");
}

#[test]
fn name_hundred_dozen() {
	test_eval("hundred dozen", "1200");
}

#[test]
fn name_thousand_dozen() {
	test_eval("thousand dozen", "12000");
}

#[test]
fn name_million_dozen() {
	test_eval("million dozen", "12000000");
}

#[test]
fn lone_prefix_yotta() {
	test_eval("yotta", "1000000000000000000000000");
}

#[test]
fn lone_prefix_zetta() {
	test_eval("zetta", "1000000000000000000000");
}

#[test]
fn lone_prefix_exa() {
	test_eval("exa", "1000000000000000000");
}

#[test]
fn lone_prefix_peta() {
	test_eval("peta", "1000000000000000");
}

#[test]
fn lone_prefix_tera() {
	test_eval("tera", "1000000000000");
}

#[test]
fn lone_prefix_giga() {
	test_eval("giga", "1000000000");
}

#[test]
fn lone_prefix_mega() {
	test_eval("mega", "1000000");
}

#[test]
fn lone_prefix_myria() {
	test_eval("myria", "10000");
}

#[test]
fn lone_prefix_kilo() {
	test_eval("kilo", "1000");
}

#[test]
fn lone_prefix_hecto() {
	test_eval("hecto", "100");
}

#[test]
fn lone_prefix_deca() {
	test_eval("deca", "10");
}

#[test]
fn lone_prefix_deka() {
	test_eval("deka", "10");
}

#[test]
fn lone_prefix_deci() {
	test_eval("deci", "0.1");
}

#[test]
fn lone_prefix_centi() {
	test_eval("centi", "0.01");
}

#[test]
fn lone_prefix_milli() {
	test_eval("milli", "0.001");
}

#[test]
fn lone_prefix_micro() {
	test_eval("micro", "0.000001");
}

#[test]
fn lone_prefix_nano() {
	test_eval("nano", "0.000000001");
}

#[test]
fn lone_prefix_pico() {
	test_eval("pico", "0.000000000001");
}

#[test]
fn lone_prefix_femto() {
	test_eval("femto", "0.000000000000001");
}

#[test]
fn lone_prefix_atto() {
	test_eval("atto", "0.000000000000000001");
}

#[test]
fn lone_prefix_zepto() {
	test_eval("zepto", "0.000000000000000000001");
}

#[test]
fn lone_prefix_yocto() {
	test_eval("yocto", "0.000000000000000000000001");
}

#[test]
fn billion() {
	test_eval("billion", "1000000000");
}

#[test]
fn trillion() {
	test_eval("trillion", "1000000000000");
}

#[test]
fn quadrillion() {
	test_eval("quadrillion", "1000000000000000");
}

#[test]
fn quintillion() {
	test_eval("quintillion", "1000000000000000000");
}

#[test]
fn sextillion() {
	test_eval("sextillion", "1000000000000000000000");
}

#[test]
fn septillion() {
	test_eval("septillion", "1000000000000000000000000");
}

#[test]
fn octillion() {
	test_eval("octillion", "1000000000000000000000000000");
}

#[test]
fn nonillion() {
	test_eval("nonillion", "1000000000000000000000000000000");
}

#[test]
fn decillion() {
	test_eval("decillion", "1000000000000000000000000000000000");
}

#[test]
fn undecillion() {
	test_eval("undecillion", "1000000000000000000000000000000000000");
}

#[test]
fn duodecillion() {
	test_eval("duodecillion", "1000000000000000000000000000000000000000");
}

#[test]
fn tredecillion() {
	test_eval(
		"tredecillion",
		"1000000000000000000000000000000000000000000",
	);
}

#[test]
fn quattuordecillion() {
	test_eval(
		"quattuordecillion",
		"1000000000000000000000000000000000000000000000",
	);
}

#[test]
fn quindecillion() {
	test_eval(
		"quindecillion",
		"1000000000000000000000000000000000000000000000000",
	);
}

#[test]
fn sexdecillion() {
	test_eval(
		"sexdecillion",
		"1000000000000000000000000000000000000000000000000000",
	);
}

#[test]
fn septendecillion() {
	test_eval(
		"septendecillion",
		"1000000000000000000000000000000000000000000000000000000",
	);
}

#[test]
fn octodecillion() {
	test_eval(
		"octodecillion",
		"1000000000000000000000000000000000000000000000000000000000",
	);
}

#[test]
fn novemdecillion() {
	test_eval(
		"novemdecillion",
		"1000000000000000000000000000000000000000000000000000000000000",
	);
}

#[test]
fn vigintillion() {
	test_eval(
		"vigintillion",
		"1000000000000000000000000000000000000000000000000000000000000000",
	);
}

#[test]
fn one_cent() {
	test_eval("cent", "1 cent");
}

#[test]
fn two_cent() {
	test_eval("2 cent", "2 cents");
}

#[test]
fn to_dp() {
	expect_error("1 to dp", None);
}

#[test]
fn to_sf() {
	expect_error("1 to sf", None);
}

#[test]
fn sf() {
	test_eval("sf", "sf");
}

#[test]
fn one_sf() {
	test_eval("1 sf", "1 sf");
}

#[test]
fn ten_sf() {
	test_eval("10 sf", "10 sf");
}

#[test]
fn one_over_sin() {
	test_eval_simple("1/sin", "\\x.(1/(sin x))");
}

#[test]
fn zero_sf() {
	expect_error("0 sf", None);
}

#[test]
fn sf_1() {
	test_eval("1234567.55645 to 1 sf", "approx. 1000000");
}

#[test]
fn sf_2() {
	test_eval("1234567.55645 to 2 sf", "approx. 1200000");
}

#[test]
fn sf_3() {
	test_eval("1234567.55645 to 3 sf", "approx. 1230000");
}

#[test]
fn sf_4() {
	test_eval("1234567.55645 to 4 sf", "approx. 1234000");
}

#[test]
fn sf_5() {
	test_eval("1234567.55645 to 5 sf", "approx. 1234500");
}

#[test]
fn sf_6() {
	test_eval("1234567.55645 to 6 sf", "approx. 1234560");
}

#[test]
fn sf_7() {
	test_eval("1234567.55645 to 7 sf", "approx. 1234567");
}

#[test]
fn sf_8() {
	test_eval("1234567.55645 to 8 sf", "approx. 1234567.5");
}

#[test]
fn sf_9() {
	test_eval("1234567.55645 to 9 sf", "approx. 1234567.55");
}

#[test]
fn sf_10() {
	test_eval("1234567.55645 to 10 sf", "approx. 1234567.556");
}

#[test]
fn sf_11() {
	test_eval("1234567.55645 to 11 sf", "approx. 1234567.5564");
}

#[test]
fn sf_12() {
	test_eval("1234567.55645 to 12 sf", "1234567.55645");
}

#[test]
fn sf_13() {
	test_eval("1234567.55645 to 13 sf", "1234567.55645");
}

#[test]
fn sf_small_1() {
	test_eval("pi / 1000000 to 1 sf", "approx. 0.000003");
}

#[test]
fn sf_small_2() {
	test_eval("pi / 1000000 to 2 sf", "approx. 0.0000031");
}

#[test]
fn sf_small_3() {
	test_eval("pi / 1000000 to 3 sf", "approx. 0.00000314");
}

#[test]
fn sf_small_4() {
	test_eval("pi / 1000000 to 4 sf", "approx. 0.000003141");
}

#[test]
fn sf_small_5() {
	test_eval("pi / 1000000 to 5 sf", "approx. 0.0000031415");
}

#[test]
fn sf_small_6() {
	test_eval_simple("pi / 1000000 to 6 sf", "approx. 0.00000314159");
}

#[test]
fn sf_small_7() {
	test_eval_simple("pi / 1000000 to 7 sf", "approx. 0.000003141592");
}

#[test]
fn sf_small_8() {
	test_eval_simple("pi / 1000000 to 8 sf", "approx. 0.0000031415926");
}

#[test]
fn sf_small_9() {
	test_eval_simple("pi / 1000000 to 9 sf", "approx. 0.00000314159265");
}

#[test]
fn sf_small_10() {
	test_eval_simple("pi / 1000000 to 10 sf", "approx. 0.000003141592653");
}

#[test]
fn sf_small_11() {
	test_eval_simple("pi / 1000000 to 11 sf", "approx. 0.0000031415926535");
}

#[test]
fn no_prefixes_for_speed_of_light() {
	expect_error("mc", None);
}

#[test]
fn quarter() {
	test_eval("quarter", "0.25");
}

#[test]
fn million_pi_1_sf() {
	test_eval("1e6 pi to 1 sf", "approx. 3000000");
}

#[test]
fn million_pi_2_sf() {
	test_eval("1e6 pi to 2 sf", "approx. 3100000");
}

#[test]
fn million_pi_3_sf() {
	test_eval("1e6 pi to 3 sf", "approx. 3140000");
}

#[test]
fn million_pi_4_sf() {
	test_eval("1e6 pi to 4 sf", "approx. 3141000");
}

#[test]
fn million_pi_5_sf() {
	test_eval("1e6 pi to 5 sf", "approx. 3141500");
}

#[test]
fn million_pi_6_sf() {
	test_eval("1e6 pi to 6 sf", "approx. 3141590");
}

#[test]
fn million_pi_7_sf() {
	test_eval("1e6 pi to 7 sf", "approx. 3141592");
}

#[test]
fn million_pi_8_sf() {
	test_eval("1e6 pi to 8 sf", "approx. 3141592.6");
}

#[test]
fn million_pi_9_sf() {
	test_eval("1e6 pi to 9 sf", "approx. 3141592.65");
}

#[test]
fn million_pi_10_sf() {
	test_eval("1e6 pi to 10 sf", "approx. 3141592.653");
}

#[test]
fn large_integer_to_1_sf() {
	test_eval("1234567 to 1 sf", "approx. 1000000");
}

#[test]
fn large_integer_to_2_sf() {
	test_eval("1234567 to 2 sf", "approx. 1200000");
}

#[test]
fn large_integer_to_3_sf() {
	test_eval("1234567 to 3 sf", "approx. 1230000");
}

#[test]
fn large_integer_to_4_sf() {
	test_eval("1234567 to 4 sf", "approx. 1234000");
}

#[test]
fn large_integer_to_5_sf() {
	test_eval("1234567 to 5 sf", "approx. 1234500");
}

#[test]
fn large_integer_to_6_sf() {
	test_eval("1234567 to 6 sf", "approx. 1234560");
}

#[test]
fn large_integer_to_7_sf() {
	test_eval("1234567 to 7 sf", "1234567");
}

#[test]
fn large_integer_to_8_sf() {
	test_eval("1234567 to 8 sf", "1234567");
}

#[test]
fn large_integer_to_9_sf() {
	test_eval("1234567 to 9 sf", "1234567");
}

#[test]
fn large_integer_to_10_sf() {
	test_eval("1234567 to 10 sf", "1234567");
}

#[test]
fn trailing_zeroes_sf_1() {
	test_eval("1234560 to 5sf", "approx. 1234500");
}

#[test]
fn trailing_zeroes_sf_2() {
	test_eval("1234560 to 6sf", "1234560");
}

#[test]
fn trailing_zeroes_sf_3() {
	test_eval("1234560 to 7sf", "1234560");
}

#[test]
fn trailing_zeroes_sf_4() {
	test_eval("1234560.1 to 6sf", "approx. 1234560");
}

#[test]
fn trailing_zeroes_sf_5() {
	test_eval("12345601 to 6sf", "approx. 12345600");
}

#[test]
fn trailing_zeroes_sf_6() {
	test_eval("12345601 to 7sf", "approx. 12345600");
}

#[test]
fn trailing_zeroes_sf_7() {
	test_eval("12345601 to 8sf", "12345601");
}

#[test]
fn kwh_conversion() {
	test_eval("100 kWh/yr to watt", "approx. 11.4079552707 watts");
}

#[test]
fn debug_pi_n() {
	test_eval_simple(
		"@debug pi N",
		"pi N (= 1000/1000 kilogram meter second^-2) (base 10, auto, simplifiable)",
	);
}

#[test]
fn square_m_to_sqft() {
	test_eval("3 square feet to square meters", "0.27870912 meters^2");
}

#[test]
fn test_hex_unit_conversion() {
	test_eval_simple("1 yard lb to hex to kg m to 3sf", "approx. 0.6a2 kg m");
}

#[test]
fn test_hex_unit_conversion_complex() {
	test_eval_simple("i yard lb to hex to kg m to 3sf", "approx. 0.6a2 i kg m");
}

#[test]
fn convert_to_billion() {
	test_eval_simple("1000000000 to billion", "1 billion");
}

#[test]
fn acres_to_sqmi() {
	test_eval("640 acre to mi^2", "1 mi^2");
}

#[test]
fn one_square_mile_to_acres() {
	test_eval("1 mile^2 to acre", "640 acres");
}

#[test]
fn one_hectare_to_km_sq() {
	test_eval("1 hectare to km^2", "0.01 km^2");
}

#[test]
fn two_km_sq_to_hectares() {
	test_eval("2 km^2 to hectare", "200 hectares");
}

#[test]
fn kg_to_unitless() {
	expect_error(
		"kg to unitless",
		Some(
			"cannot convert from kg to unitless: units 'kilogram' and 'unitless' are incompatible",
		),
	);
}

#[test]
fn percent_to_unitless() {
	test_eval("1% to unitless", "0.01");
}

#[test]
fn convert_to_numerical_product() {
	expect_error("550Mbit/s to GB/s * 12000s", None);
}

#[test]
fn unit_simplification() {
	test_eval("0.18mL * 40 mg/mL", "7.2 mg");
}

#[test]
fn unit_simplification_kg_1() {
	test_eval("kg g", "0.001 kg^2");
}

#[test]
fn unit_simplification_kg_2() {
	test_eval("kg g^0", "1 kg");
}

#[test]
fn unit_simplification_kg_3() {
	test_eval("kg g^-1", "1000");
}

#[test]
fn unit_simplification_kg_4() {
	test_eval("kg^2 g", "0.001 kg^3");
}

#[test]
fn unit_simplification_kg_5() {
	test_eval("kg^2 g^0", "1 kg^2");
}

#[test]
fn unit_simplification_kg_6() {
	test_eval("kg^2 g^-1", "1000 kg");
}

#[test]
fn unit_simplification_kg_7() {
	test_eval("kg^2 g^-2", "1000000");
}

#[test]
fn eccentricity_of_earth() {
	test_eval("eccentricity of earth", "0.0167086");
}

#[test]
fn mass_of_earth() {
	test_eval("mass of earth", "5972370000000000000000000 kg");
}

#[test]
fn maths_with_earth_properties() {
	test_eval(
		"escape_velocity of earth / gravity of earth",
		"approx. 1140.6545558371 s",
	);
}

#[test]
fn kelvin_to_rankine() {
	test_eval("273K to °R", "491.4 °R");
}

#[test]
fn joule_per_kelvin_to_joule_per_celsius() {
	test_eval("1J/K to J/°C", "1 J / °C");
}

#[test]
fn kelvin_plus_celsius() {
	test_eval("1K+1°C", "2 K");
}

#[test]
fn kelvin_plus_fahrenheit() {
	test_eval("1K+1°F", "approx. 1.5555555555 K");
}

#[test]
fn celsius_plus_kelvin() {
	test_eval("1°C+1K", "2 °C");
}

#[test]
fn fahrenheit_plus_kelvin() {
	test_eval("1°F+1K", "2.8 °F");
}

#[test]
fn celsius_plus_fahrenheit() {
	test_eval("1°C+1°F", "approx. 1.5555555555 °C");
}

#[test]
fn celsius_plus_rankine() {
	test_eval("1°C+1°R", "approx. 1.5555555555 °C");
}

#[test]
fn fahrenheit_plus_celsius() {
	test_eval("1°F+1°C", "2.8 °F");
}

#[test]
fn fahrenheit_plus_kilokelvin() {
	test_eval("1°F+10kK", "18001 °F");
}

#[test]
fn celsius_plus_millikelvin() {
	test_eval("-273.15°C+1mK", "-273.149 °C");
}

#[test]
fn joule_per_kelvin_to_joule_per_fahrenheit() {
	test_eval("1J/K to J/°F", "approx. 0.5555555555 J / °F");
}

#[test]
fn ice_melting_point_to_kelvin() {
	test_eval("0°C to K", "273.15 K");
}

#[test]
fn degrees_kelvin_as_alias() {
	test_eval("6°K", "6 K");
}

#[test]
fn fahrenheit_squared_plus_kelvin_squared() {
	test_eval("(1°F)^2 + 1 K^2", "4.24 °F^2");
}

#[test]
fn fahrenheit_squared_to_kelvin_squared() {
	test_eval("(1°F)^2 to 1 K^2", "approx. 0.3086419753 K^2");
}

#[test]
fn hundred_celsius_to_fahrenheit() {
	test_eval("100°C to °F", "212 °F");
}

#[test]
fn zero_celsius_to_fahrenheit() {
	test_eval("0°C to °F", "32 °F");
}

#[test]
fn zero_kelvin_to_fahrenheit() {
	test_eval("0K to °F", "-459.67 °F");
}

#[test]
fn zero_millicelsius_to_fahrenheit() {
	test_eval("0 millicelsius to °F", "32 °F");
}

#[test]
fn zero_kilocelsius_to_fahrenheit() {
	test_eval("0 kilocelsius to °F", "32 °F");
}

#[test]
fn zero_kilocelsius_to_millifahrenheit() {
	test_eval("0 kilocelsius to millifahrenheit", "32000 millifahrenheit");
}

#[test]
#[ignore]
fn five_percent_celsius_to_fahrenheit() {
	test_eval("5% °C to °F", "32.09 °F");
}

#[test]
fn five_celsius_to_fahrenheit() {
	test_eval("5°C to °F", "41 °F");
}

#[test]
fn fifteen_celsius_to_rankine() {
	test_eval("15°C to °R", "518.67 °R");
}

#[test]
fn fifteen_celsius_to_kelvin() {
	test_eval("15°C to K", "288.15 K");
}

#[test]
fn celsius_as_c() {
	test_eval("4C", "4 °C");
}

#[test]
fn fahrenheit_as_f() {
	test_eval("4C to F", "39.2 °F");
}

#[test]
fn radians_to_degrees() {
	test_eval("pi radians to °", "180°");
}

#[test]
fn minus_40_fahrenheit_to_celsius() {
	test_eval("-40 F to C", "-40 °C");
}

#[test]
fn gigabits_to_gigabytes() {
	test_eval("25Gib/s to GB/s", "3.3554432 GB / s");
}

#[test]
fn one_plus_one() {
	test_eval("1 + 1", "2");
}

#[test]
fn unterminated_raw_empty_string() {
	expect_error("#\"", Some("unterminated string literal"));
}

#[test]
fn unterminated_raw_string() {
	expect_error("#\"hello", Some("unterminated string literal"));
}

#[test]
fn empty_raw_string() {
	test_eval("#\"\"#", "");
}

#[test]
fn hello_world_raw_string() {
	test_eval_simple("#\"Hello, world!\"#", "Hello, world!");
}

#[test]
fn backslash_in_raw_string_literal() {
	test_eval_simple("#\"\\\"#", "\\");
}

#[test]
fn double_quote_in_raw_string() {
	test_eval_simple("#\"A quote: \"\"#", "A quote: \"");
}

#[test]
fn raw_string_debug_representation() {
	test_eval_simple("@debug #\"hi\"#", "\"hi\"");
}

#[test]
fn string_debug_representation() {
	test_eval_simple("@debug \"hi\"", "\"hi\"");
}

#[test]
fn cis_4() {
	test_eval("cis 4", "approx. -0.6536436208 - 0.7568024953i");
}

#[test]
fn a_prime() {
	expect_error("a'", Some("unknown identifier 'a''"));
}

#[test]
fn a_double_prime() {
	expect_error("a\"", Some("unknown identifier 'a\"'"));
}

#[test]
fn one_inch_with_space() {
	test_eval("1 \"", "1\"");
}

#[test]
fn empty_string() {
	test_eval("\"\"", "");
}

#[test]
fn unterminated_empty_string() {
	expect_error("\"", Some("unterminated string literal"));
}

#[test]
fn unterminated_string() {
	expect_error("\"hello", Some("unterminated string literal"));
}

#[test]
fn hello_world_string() {
	test_eval_simple("\"Hello, world!\"", "Hello, world!");
}

#[test]
fn backslash_in_string_literal() {
	test_eval_simple(r#""\\""#, "\\");
}

#[test]
fn add_string_to_number() {
	expect_error("\"hi\" + 2", Some("expected a number"));
}

#[test]
fn simple_string_concatenation() {
	test_eval_simple(r#""hi" + "a""#, "hia");
}

#[test]
fn triple_string_concatenation() {
	test_eval_simple(r#""hi" + "a" + "3""#, "hia3");
}

#[test]
fn number_to_string() {
	test_eval_simple("\"pi = \" + (pi to string)", "pi = approx. 3.1415926535");
}

#[test]
fn escape_sequence_backslashes() {
	test_eval_simple(r#""\\\\ \\""#, "\\\\ \\");
}

#[test]
fn escape_sequence_linebreak() {
	test_eval_simple(r#""\n""#, "\n");
}

#[test]
fn escape_sequence_tab() {
	test_eval_simple(r#""\t""#, "\t");
}

#[test]
fn escape_sequence_quote() {
	test_eval_simple(r#""\"""#, "\"");
}

#[test]
fn escape_sequence_vertical_tab() {
	test_eval_simple(r#""\v""#, "\u{0b}");
}

#[test]
fn escape_sequence_escape() {
	test_eval_simple(r#""\e""#, "\u{1b}");
}

#[test]
fn escape_sequence_backspace() {
	test_eval_simple(r#""\b""#, "\u{8}");
}

#[test]
fn escape_sequence_tilde() {
	test_eval_simple(r#""\x7e""#, "~");
}

#[test]
fn escape_sequence_first_char_out_of_range() {
	expect_error(
		r#""\x9e""#,
		Some("expected an escape sequence between \\x00 and \\x7f"),
	);
}

#[test]
fn escape_sequence_second_char_out_of_range() {
	expect_error(
		r#""\x7g""#,
		Some("expected an escape sequence between \\x00 and \\x7f"),
	);
}

#[test]
fn multiple_escape_sequences() {
	test_eval_simple(r#""\\\n\e\v\b\t\x00\x7F""#, "\\\n\u{1b}\u{0b}\u{8}\t\0\x7f");
}

#[test]
fn bell_in_string() {
	test_eval_simple("\"\\a\"", "\u{7}");
}

#[test]
fn form_feed_in_string() {
	test_eval_simple("\"\\f\"", "\u{c}");
}

#[test]
fn escaped_single_quote_in_string() {
	test_eval_simple("\" \\' \"", " ' ");
}

#[test]
fn skip_whitespace_in_string() {
	test_eval_simple("\" hi \\z  \n\t  \r\n  \' \\z\\za\\z :\"", " hi ' a:");
}

#[test]
fn single_quote_string() {
	test_eval_simple("'hi'", "hi");
}

#[test]
fn single_quote_string_unterminated() {
	expect_error(r#"'hi\"\'"#, Some("unterminated string literal"));
}

#[test]
fn single_quote_string_with_escapes() {
	test_eval_simple(r#"'hi\"\''"#, "hi\"'");
}

#[test]
fn control_char_escape_question_mark() {
	test_eval_simple("'\\^?'", "\x7f");
}

#[test]
fn control_char_escape_at_symbol() {
	test_eval_simple("'\\^@'", "\0");
}

#[test]
fn control_char_escape_a() {
	test_eval_simple("'\\^A'", "\x01");
}

#[test]
fn control_char_escape_b() {
	test_eval_simple("'\\^B'", "\x02");
}

#[test]
fn control_char_escape_c() {
	test_eval_simple("'\\^C'", "\x03");
}

#[test]
fn control_char_escape_d() {
	test_eval_simple("'\\^D'", "\x04");
}

#[test]
fn control_char_escape_e() {
	test_eval_simple("'\\^E'", "\x05");
}

#[test]
fn control_char_escape_f() {
	test_eval_simple("'\\^F'", "\x06");
}

#[test]
fn control_char_escape_g() {
	test_eval_simple("'\\^G'", "\x07");
}

#[test]
fn control_char_escape_h() {
	test_eval_simple("'\\^H'", "\x08");
}

#[test]
fn control_char_escape_i() {
	test_eval_simple("'\\^I'", "\x09");
}

#[test]
fn control_char_escape_j() {
	test_eval_simple("'\\^J'", "\x0a");
}

#[test]
fn control_char_escape_k() {
	test_eval_simple("'\\^K'", "\x0b");
}

#[test]
fn control_char_escape_l() {
	test_eval_simple("'\\^L'", "\x0c");
}

#[test]
fn control_char_escape_p() {
	test_eval_simple("'\\^P'", "\x10");
}

#[test]
fn control_char_escape_x() {
	test_eval_simple("'\\^X'", "\x18");
}

#[test]
fn control_char_escape_y() {
	test_eval_simple("'\\^Y'", "\x19");
}

#[test]
fn control_char_escape_z() {
	test_eval_simple("'\\^Z'", "\x1a");
}

#[test]
fn control_char_escape_opening_square_bracket() {
	test_eval_simple("'\\^['", "\x1b");
}

#[test]
fn control_char_escape_backslash() {
	test_eval_simple("'\\^\\'", "\x1c");
}

#[test]
fn control_char_escape_closing_square_bracket() {
	test_eval_simple("'\\^]'", "\x1d");
}

#[test]
fn control_char_escape_caret() {
	test_eval_simple("'\\^^'", "\x1e");
}

#[test]
fn control_char_escape_underscore() {
	test_eval_simple("'\\^_'", "\x1f");
}

#[test]
fn control_char_escape_lowercase() {
	expect_error("'\\^a'", None);
}

#[test]
fn control_char_escape_gt() {
	expect_error("'\\^>'", None);
}

#[test]
fn control_char_escape_backtick() {
	expect_error("'\\^`'", None);
}

#[test]
fn unicode_escape_7e() {
	test_eval_simple("'\\u{7e}'", "~");
}

#[test]
fn unicode_escape_696969() {
	expect_error(
		"'\\u{696969}'",
		Some("invalid Unicode escape sequence, expected e.g. \\u{7e}"),
	);
}

#[test]
fn unicode_escape_69() {
	test_eval("'\\u{69}'", "i");
}

#[test]
fn unicode_escape_69x() {
	expect_error(
		"'\\u{69x}'",
		Some("invalid Unicode escape sequence, expected e.g. \\u{7e}"),
	);
}

#[test]
fn unicode_escape_empty() {
	expect_error(
		"'\\u{}'",
		Some("invalid Unicode escape sequence, expected e.g. \\u{7e}"),
	);
}

#[test]
fn unicode_escape_5437() {
	test_eval_simple("'\\u{5437}'", "\u{5437}");
}

#[test]
fn unicode_escape_5() {
	test_eval_simple("'\\u{5}'", "\u{5}");
}

#[test]
fn unicode_escape_0() {
	test_eval_simple("'\\u{0}'", "\0");
}

#[test]
fn unicode_escape_1() {
	test_eval_simple("'\\u{1}'", "\u{1}");
}

#[test]
fn unicode_escape_10ffff() {
	test_eval_simple("'\\u{10ffff}'", "\u{10ffff}");
}

#[test]
fn unicode_escape_aaa_uppercase() {
	test_eval_simple("'\\u{AAA}'", "\u{aaa}");
}

#[test]
#[ignore]
fn today() {
	let mut context = Context::new();
	context.set_current_time_v1(1617517099000, 0);
	assert_eq!(
		evaluate("today", &mut context).unwrap().get_main_result(),
		"Sunday, 4 April 2021"
	);
}

#[test]
#[ignore]
fn today_with_tz() {
	let mut context = Context::new();
	context.set_current_time_v1(1619943083155, 43200);
	assert_eq!(
		evaluate("today", &mut context).unwrap().get_main_result(),
		"Sunday, 2 May 2021"
	);
}

#[test]
fn acre_foot_to_m_3() {
	test_eval("acre foot to m^3", "1233.48183754752 m^3");
}

#[test]
fn cm_to_double_quote() {
	test_eval("2.54cm to \"", "1\"");
}

#[test]
fn cm_to_single_quote() {
	test_eval("30.48cm to \'", "1'");
}

#[test]
fn single_line_comment() {
	test_eval("30.48cm to \' # converting cm to feet", "1'");
}

#[test]
fn single_line_comment_and_linebreak() {
	test_eval("30.48cm to \' # converting cm to feet\n", "1'");
}

#[test]
fn single_line_comment_and_linebreak_2() {
	test_eval("30.48cm to # converting cm\n feet # to feet", "1 foot");
}

#[test]
fn single_line_comment_and_linebreak_3() {
	test_eval("30.48cm to # converting cm\n ' # to feet", "1'");
}

#[test]
fn percent_plus_per_mille() {
	test_eval("4% + 3\u{2030}", "4.3%");
}

#[test]
fn custom_base_unit() {
	test_eval_simple("5 'tests'", "5 tests");
}

#[test]
fn custom_base_unit_in_calculation() {
	test_eval_simple("5 'pigeons' per meter", "5 pigeons / meter");
}

#[test]
#[ignore]
fn custom_base_unit_in_calculation_2() {
	test_eval("5 'pigeons' per meter / 'pigeons'", "5 meters");
}

#[test]
fn five_k() {
	test_eval("5k", "5000");
}

#[test]
fn upper_case_meter() {
	test_eval("4 Metres", "4 metres");
}

#[test]
fn mixed_case_meter() {
	test_eval("4 mEtRes", "4 metres");
}

#[test]
fn asin_minus_1_1() {
	test_eval("asin -1.1", "approx. -1.5707963267 + 0.4435682543i");
}

#[test]
fn custom_unit_test() {
	test_eval_simple("15*3*50/1000 'cases'", "2.25 cases");
}

#[test]
fn simplify_ms_per_year() {
	test_eval("ms/year", "approx. 0");
}

#[test]
fn not_simplify_explicit_to() {
	test_eval_simple(
		"1.550519768*10^-8 to ms/year",
		"489.296375410515266426112 ms / year",
	);
}

#[test]
fn unicode_operators() {
	test_eval("5 − 2 ✕ 3 × 1 ÷ 1 ∕ 3", "3");
}

#[test]
fn bool_true() {
	test_eval("true", "true");
}

#[test]
fn bool_false() {
	test_eval("false", "false");
}

#[test]
fn zero_to_bool() {
	test_eval("0 to bool", "false");
}

#[test]
fn zero_to_boolean() {
	test_eval("0 to boolean", "false");
}

#[test]
fn one_to_bool() {
	test_eval("1 to bool", "true");
}

#[test]
fn minus_one_to_bool() {
	test_eval("-1 to bool", "true");
}

#[test]
fn not_true() {
	test_eval("not true", "false");
}

#[test]
fn not_false() {
	test_eval("not false", "true");
}

#[test]
fn not_one() {
	expect_error("not 1", None);
}

#[test]
fn sqm() {
	test_eval("5 sqm", "5 m^2");
}

#[test]
fn sqft() {
	test_eval("5 sqft", "5 ft^2");
}

#[test]
fn modulo() {
	for a in 0..30 {
		for b in 1..30 {
			let input = format!("{a} mod {b}");
			let output = (a % b).to_string();
			test_eval(&input, &output);
		}
	}
}

#[test]
fn modulo_zero() {
	expect_error("5 mod 0", Some("modulo by zero"));
}

#[test]
fn binary_modulo() {
	test_eval("0b1001010 mod 5", "0b100");
}

#[test]
fn huge_modulo() {
	test_eval(
		"9283749283460298374027364928736492873469287354267354 mod 4",
		"2",
	);
}

#[test]
fn month_of_date() {
	test_eval_simple("month of ('2020-03-04' to date)", "March");
}

#[test]
fn weekday_of_date() {
	test_eval_simple("day_of_week of ('2020-05-08' to date)", "Friday");
}

#[test]
fn day_of_week_type_name() {
	expect_error(
		"5 to (day_of_week of ('2020-05-08' to date)",
		Some("cannot convert value to day of week"),
	);
}

#[test]
fn phi() {
	test_eval("phi", "approx. 1.6180339886");
}

#[test]
fn five_dollars() {
	test_eval("$5", "$5");
}

#[test]
fn dollar_prefix() {
	test_eval_simple("$200/3 to 2dp", "approx. $66.66");
}

#[test]
fn dollar_multiplication() {
	test_eval("$3 * 7", "$21");
}

#[test]
#[ignore]
fn dollar_multiplication_reverse() {
	test_eval("7 * $3", "$21");
}

#[test]
fn gbp_symbol() {
	test_eval("£5 + £3", "£8");
}

#[test]
fn jpy_symbol() {
	test_eval("¥5 + ¥3", "¥8");
}

#[test]
fn two_statements() {
	test_eval("2; 4", "4");
}

#[test]
fn five_statements() {
	test_eval("2; 4; 8kg; c:2c; a = 2", "2");
}

#[test]
fn variable_assignment() {
	test_eval("a = b = 2; b", "2");
}

#[test]
fn overwrite_variable() {
	test_eval("a = 3; a = a + 4a; a", "15");
}

#[test]
fn multiple_variables() {
	test_eval("a = 3; b = 2a; c = a * b; c + a", "21");
}

#[test]
fn mixed_frac() {
	test_eval_simple("4/3 to mixed_frac", "1 1/3");
}

#[test]
fn farad_conversion() {
	test_eval("1 farad to A^2 kg^-1 m^-2 s^4", "1 A^2 s^4 kg^-1 m^-2");
}

#[test]
fn coulomb_farad_mode() {
	let mut ctx = Context::new();
	ctx.use_coulomb_and_farad();
	assert_eq!(
		evaluate("5C to coulomb", &mut ctx)
			.unwrap()
			.get_main_result(),
		"5 coulomb"
	);
	assert_eq!(
		evaluate("5uF to farad", &mut ctx)
			.unwrap()
			.get_main_result(),
		"0.000005 farad"
	);
}

#[test]
fn test_rolling_dice() {
	let mut ctx = Context::new();
	ctx.set_random_u32_fn(|| 5);
	evaluate("roll d20", &mut ctx).unwrap();
}

#[test]
fn test_d6() {
	test_eval_simple(
		"d6",
		"{ 1: 16.67%, 2: 16.67%, 3: 16.67%, 4: 16.67%, 5: 16.67%, 6: 16.67% }",
	);
}

#[test]
fn test_2d6() {
	test_eval_simple(
		"2d6",
		"{ 2: 2.78%, 3: 5.56%, 4: 8.33%, 5: 11.11%, \
		6: 13.89%, 7: 16.67%, 8: 13.89%, 9: 11.11%, 10: 8.33%, 11: 5.56%, 12: 2.78% }",
	)
}

#[test]
fn test_invalid_dice_syntax_1() {
	expect_error("0d6", Some("invalid dice syntax, try e.g. `4d6`"));
}

#[test]
fn test_invalid_dice_syntax_2() {
	expect_error("1d0", Some("invalid dice syntax, try e.g. `4d6`"));
}

#[test]
fn test_invalid_dice_syntax_3() {
	expect_error("0d0", Some("invalid dice syntax, try e.g. `4d6`"));
}

#[test]
fn test_invalid_dice_syntax_4() {
	expect_error("d30000000000000000", None);
}

#[test]
fn test_invalid_dice_syntax_5() {
	// this produces different error messages on 32-bit vs 64-bit platforms
	expect_error("30000000000000000d2", None);
}

#[test]
fn unit_literal() {
	test_eval("()", "()");
}

#[test]
fn empty_statements() {
	test_eval("1234;", "1234");
	test_eval(";432", "432");
	test_eval(";", "()");
	test_eval(";;3", "3");
	test_eval("34;;;", "34");
	test_eval(";2;;3;a=4;;4a", "16");
	test_eval(";2;;3;a=4;;4a;;;()", "()");
}

#[test]
fn add_days_to_date() {
	test_eval_simple(
		"('2020-05-04' to date) + 500 days",
		"Thursday, 16 September 2021",
	);
}

#[test]
fn fancy_syntax() {
	test_eval("(\u{3bb}x.x) 5", "5");
}

#[test]
fn kmh() {
	test_eval("25146 kmh to mph", "15625 mph");
}

#[test]
fn km_slash_h() {
	test_eval("25146 km/h to mph", "15625 mph");
}

#[test]
fn planck() {
	test_eval("planck", "0.000000000000000000000000000000000662607015 J s");
}

#[test]
fn implicit_unit_fudging() {
	test_eval("5'1 to m to 2dp", "approx. 1.54 m");
}

#[test]
fn implicit_unit_fudging_2() {
	test_eval("0'1 to m to 2dp", "approx. 0.02 m");
}

#[test]
fn implicit_unit_fudging_3() {
	expect_error(
		"0'1 + 5",
		Some("cannot convert from unitless to ': units 'unitless' and 'meter' are incompatible"),
	);
}

#[test]
fn implicit_unit_fudging_4() {
	test_eval("5'1 + 5m", "approx. 21.4875328083'");
}

#[test]
fn implicit_unit_fudging_5() {
	expect_error(
		"5'1 + 5kg",
		Some("cannot convert from kg to ': units 'kilogram' and 'meter' are incompatible"),
	);
}

#[test]
fn millicoulomb() {
	test_eval("5 coulomb to mC", "5000 mC");
}

#[test]
fn millifarad() {
	test_eval("5 farad to mF", "5000 mF");
}

#[test]
fn point() {
	test_eval("0.5 points to mm", "approx. 0.1763888888 mm");
}

#[test]
fn rad_per_sec() {
	test_eval("10 RPM to rad/s", "approx. 1.0471975511 rad / s")
}

#[test]
fn fudged_rhs_feet_conv() {
	test_eval("6 foot 4 in cm", "193.04 cm");
}

#[test]
fn trivial_fn() {
	// used for testing fn serialization
	test_eval("x:()", "\\x.()");
}

#[test]
fn unknown_argument_error_msg() {
	expect_error("sqrt(aiusbdla)", Some("unknown identifier 'aiusbdla'"));
}

#[test]
fn log10_cancelled_units() {
	test_eval("log10 (1m / (1m", "approx. 0");
}

#[test]
fn shebang() {
	test_eval("#!/usr/bin/env fend\n1 + 1", "2");
}

#[test]
fn bitwise_and_1() {
	test_eval("0 & 0", "0");
	test_eval("0 & 1", "0");
	test_eval("1 & 0", "0");
	test_eval("1 & 1", "1");
}

#[test]
fn bitwise_and_2() {
	test_eval("0 & 91802367489176234987162938461829374691238641", "0");
}

#[test]
fn bitwise_and_3() {
	test_eval(
		"912834710927364108273648927346788234682764 &
        98123740918263740896274873648273642342534252",
		"207742386994266479278471200397877100888076",
	);
}

#[test]
fn bitwise_or_1() {
	test_eval("0 | 0", "0");
	test_eval("0 | 1", "1");
	test_eval("1 | 0", "1");
	test_eval("1 | 1", "1");
}

#[test]
fn bitwise_or_2() {
	test_eval("3 | 4", "7");
}

#[test]
fn bitwise_or_3() {
	test_eval("255 | 34", "255");
}

#[test]
fn bitwise_or_4() {
	test_eval("0b0011 | 0b0101", "0b111");
}

#[test]
fn bitwise_xor_1() {
	test_eval("0 xor 0", "0");
	test_eval("0 xor 1", "1");
	test_eval("1 xor 0", "1");
	test_eval("1 xor 1", "0");
}

#[test]
fn bitwise_xor_2() {
	test_eval(
		"019278364182374698123476928376459726354982 xor
		387294658347659283475689347659823745692837465",
		"387286275339643142048939049610868709852535935",
	);
}

#[test]
fn lshift() {
	test_eval("0 << 10", "0");
	test_eval("54 << 1", "108");
	test_eval("54 << 2", "216");
	test_eval("54 << 3", "432");
}

#[test]
fn rshift() {
	test_eval("54 >> 12", "0");
	test_eval("54 >> 1", "27");
	test_eval("54 >> 2", "13");
	test_eval("54 >> 3", "6");
}

#[test]
fn shift_and_and() {
	test_eval("54 << 1 & 54 >> 1", "8");
}

#[test]
fn combination_test() {
	test_eval("5 nCr 2", "10");
	test_eval("5 choose 2", "10");
	test_eval("10 nCr 3", "120");
	test_eval("10 choose 3", "120");
}

#[test]
fn permutation_test() {
	test_eval("5 nPr 2", "20");
	test_eval("5 permute 2", "20");
	test_eval("10 nPr 3", "720");
	test_eval("10 permute 3", "720");
}

// ERROR
#[test]
fn date_literals() {
	test_eval_simple("@1970-01-01", "Thursday, 1 January 1970");
}

// ERROR
#[test]
fn date_literal_subtraction() {
	test_eval_simple("@2022-11-29 - 2 days", "Sunday, 27 November 2022");
	test_eval_simple("@2022-11-29 - 2 weeks", "Tuesday, 15 November 2022");
	test_eval_simple("@2022-11-29 - 2 months", "Thursday, 29 September 2022");
	test_eval_simple("@2022-11-29 - 2 years", "Sunday, 29 November 2020");

	test_eval_simple("@2022-03-01 - 1 month", "Tuesday, 1 February 2022");
	test_eval_simple("@2020-02-28 - 1 year", "Thursday, 28 February 2019");
	expect_error(
		"@2020-02-29 - 1 year",
		"February 29, 2019 does not exist, did you mean Thursday, 28 February 2019 or Friday, 1 March 2019?".into(),
	);
	expect_error(
        "@2020-02-29 - 12 month",
        "February 29, 2019 does not exist, did you mean Thursday, 28 February 2019 or Friday, 1 March 2019?".into(),
    );
	test_eval_simple("@2020-08-01 - 1 year", "Thursday, 1 August 2019");
}

#[test]
fn atan_meter() {
	test_eval("atan((30 centi meter) / (2 meter))", "approx. 0.1488899476");
	test_eval("atan((30 centimeter) / (2 meter))", "approx. 0.1488899476");
}

#[test]
fn centimeter_no_currencies() {
	let mut context = Context::new();
	assert_eq!(
		evaluate("1 centimeter", &mut context)
			.unwrap()
			.get_main_result(),
		"1 centimeter"
	);
}

#[test]
fn underscore_variables() {
	test_eval("test_a = 5; test_a", "5");
}

#[test]
fn light_year_1() {
	test_eval("light_year / light to days", "365.25 days");
}

#[test]
fn light_year_2() {
	test_eval("light year / light to days", "365.25 days");
}

#[test]
fn light_year_3() {
	test_eval("lightyear / light to days", "365.25 days");
}

#[test]
fn light_year_4() {
	test_eval("light day / light to days", "1 day");
}

#[test]
fn mixed_case_abbreviations_1() {
	test_eval("5 KB", "5 kB");
}

#[test]
fn mixed_case_abbreviations_2() {
	test_eval("5 Kb", "5 kb");
}

#[test]
fn mixed_case_abbreviations_3() {
	test_eval("5 gb", "5 Gb");
}

#[test]
fn mixed_case_abbreviations_4() {
	test_eval("5 kiwh", "5 KiWh");
}

#[test]
fn thou() {
	test_eval("2 thou to mm", "0.0508 mm");
}

#[test]
fn hubble_constant() {
	test_eval_simple(
		"70 km s^-1 Mpc^-1 to 25dp",
		"approx. 0.0000000000000000022685455 s^-1",
	);
}

#[test]
fn oc() {
	test_eval("5 oC", "5 °C");
}

#[test]
fn to_million() {
	test_eval_simple("5 to million", "0.000005 million");
}

#[test]
fn ohms_law() {
	test_eval("(5 volts) / (2 ohms)", "2.5 amperes");
}

#[test]
fn simplification_sec_hz() {
	test_eval("c/(145MHz)", "approx. 2.0675341931 meters");
}

#[test]
fn simplification_ohms() {
	test_eval("4556 ohm * ampere", "4556 volts");
}

#[test]
fn simplification_ohms_2() {
	test_eval("4556 volt / ampere", "4556 ohms");
}

#[test]
fn alias_sqrt() {
	test_eval(
		"partial_result = 2*(0.84 femto meter) / (1.35e-22 m/s^2); sqrt(partial_result)",
		"approx. 3527.6684147527 s",
	);
}

#[test]
fn test_superscript() {
	test_eval("200²", "40000");
	test_eval("13¹³ days", "302875106592253 days");
}

#[test]
fn test_equality() {
	test_eval("1 + 2 == 3", "true");
	test_eval("1 + 2 != 4", "true");
	test_eval("1 + 2 ≠ 4", "true");
	test_eval("1 + 2 <> 4", "true");
	test_eval("true == false", "false");
	test_eval("true != false", "true");
	test_eval("true ≠ false", "true");
	test_eval("2m == 200cm", "true");
	test_eval("2kg == 200cm", "false");
	test_eval("2kg == true", "false");
	test_eval("2.010m == 200cm", "false");
	test_eval("2.000m == approx. 200cm", "true");
}

#[test]
fn test_roman() {
	expect_error(
		"0 to roman",
		Some("zero cannot be represented as a roman numeral"),
	);
	test_eval_simple("1 to roman", "I");
	test_eval_simple("2 to roman", "II");
	test_eval_simple("3 to roman", "III");
	test_eval_simple("4 to roman", "IV");
	test_eval_simple("5 to roman", "V");
	test_eval_simple("6 to roman", "VI");
	test_eval_simple("7 to roman", "VII");
	test_eval_simple("8 to roman", "VIII");
	test_eval_simple("9 to roman", "IX");
	test_eval_simple("10 to roman", "X");
	test_eval_simple("11 to roman", "XI");
	test_eval_simple("12 to roman", "XII");
	test_eval_simple("13 to roman", "XIII");
	test_eval_simple("14 to roman", "XIV");
	test_eval_simple("15 to roman", "XV");
	test_eval_simple("16 to roman", "XVI");
	test_eval_simple("17 to roman", "XVII");
	test_eval_simple("18 to roman", "XVIII");
	test_eval_simple("19 to roman", "XIX");
	test_eval_simple("20 to roman", "XX");
	test_eval_simple("21 to roman", "XXI");
	test_eval_simple("22 to roman", "XXII");
	test_eval_simple("45 to roman", "XLV");
	test_eval_simple("134 to roman", "CXXXIV");
	test_eval_simple("1965 to roman", "MCMLXV");
	test_eval_simple("2020 to roman", "MMXX");
	test_eval_simple("3456 to roman", "MMMCDLVI");
	test_eval_simple("1452 to roman", "MCDLII");
	test_eval_simple("20002 to roman", "X\u{305}X\u{305}II");
	expect_error(
		"1000000001 to roman",
		Some("1000000001 must lie in the interval [1, 1000000000]"),
	);
}

#[test]
fn rack_unit() {
	test_eval("4U to cm", "17.78 cm");
}

#[test]
fn test_mean() {
	test_eval("mean d1", "1");
	test_eval("mean d2", "1.5");
	test_eval("mean d500", "250.5");

	test_eval("mean (d1 + d1)", "2");
	test_eval("mean (d2 + d500)", "252");

	test_eval("mean (d6 / d2)", "2.625");
	test_eval("mean (d10 / d2)", "4.125");

	test_eval("average d500", "250.5");
}

#[test]
fn modulo_percent() {
	test_eval("5%4", "1");
	test_eval("(104857566-103811072+1) % (1024*1024/512)", "2015");
}

#[test]
fn modulo_unitless() {
	test_eval("5 mod (4k)", "5");
	test_eval("(4k)^2", "16000000");
}

#[test]
fn fibonacci() {
	test_eval("fib 0", "0");
	test_eval("fib 1", "1");
	test_eval("fib 2", "1");
	test_eval("fib 3", "2");
	test_eval("fib 4", "3");
	test_eval("fib 5", "5");
	test_eval("fib 6", "8");
	test_eval("fib 7", "13");
	test_eval("fib 8", "21");
	test_eval("fib 9", "34");
	test_eval("fib 10", "55");
	test_eval("fib 11", "89");
}

#[test]
fn uppercase_identifiers() {
	test_eval("SIN PI", "0");
	test_eval("COS TAU", "1");
	test_eval("LOG 1", "approx. 0");
	test_eval("LOG10 1", "approx. 0");
	test_eval("EXP 0", "approx. 1");

	expect_error("foo = 1; FOO", Some("unknown identifier 'FOO'"));
}

#[test]
fn test_words() {
	test_eval_simple("1 to words", "one");
	test_eval_simple("9 to words", "nine");
	test_eval_simple("15 to words", "fifteen");
	test_eval_simple("20 to words", "twenty");
	test_eval_simple("99 to words", "ninety-nine");
	test_eval_simple("154 to words", "one hundred and fifty-four");
	test_eval_simple("500 to words", "five hundred");
	test_eval_simple("999 to words", "nine hundred and ninety-nine");
	test_eval_simple("1000 to words", "one thousand");
	test_eval_simple(
		"4321 to words",
		"four thousand three hundred and twenty-one",
	);
	test_eval_simple("1000000 to words", "one million");
	test_eval_simple(
		"1234567 to words",
		"one million two hundred and thirty-four thousand five hundred and sixty-seven",
	);
	test_eval_simple("1000000000 to words", "one billion");
	test_eval_simple("9876543210 to words", "nine billion eight hundred and seventy-six million five hundred and forty-three thousand two hundred and ten");
	test_eval_simple("1000000000000 to words", "one trillion");
	test_eval_simple("1234567890123456 to words", "one quadrillion two hundred and thirty-four trillion five hundred and sixty-seven billion eight hundred and ninety million one hundred and twenty-three thousand four hundred and fifty-six");
	test_eval_simple("1000000000000000000000 to words", "one sextillion");
	test_eval_simple("1000000000000000000000000 to words", "one septillion");
}

#[test]
fn test_plus_zero_ignore_units() {
	test_eval("4m + 0", "4 m");
	test_eval("4m + 0kg", "4 m");
	test_eval("4m + (sin pi) kg", "4 m");
	expect_error(
		"4m + (sin (pi/2)) kg",
		Some("cannot convert from kg to m: units 'kilogram' and 'meter' are incompatible"),
	);
}

#[test]
fn european_formatting() {
	let mut ctx = Context::new();
	ctx.set_decimal_separator_style(fend_core::DecimalSeparatorStyle::Comma);
	assert_eq!(
		fend_core::evaluate("1.234,56 * 1000", &mut ctx)
			.unwrap()
			.get_main_result(),
		"1234560"
	);
	assert_eq!(
		fend_core::evaluate("100-1,9", &mut ctx)
			.unwrap()
			.get_main_result(),
		"98,1"
	);
	assert_eq!(
		fend_core::evaluate("sin(1,2)", &mut ctx)
			.unwrap()
			.get_main_result(),
		"approx. 0,9320390859"
	);
}
