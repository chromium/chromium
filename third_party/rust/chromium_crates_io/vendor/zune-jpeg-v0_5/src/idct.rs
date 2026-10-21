/*
 * Copyright (c) 2023.
 *
 * This software is free software;
 *
 * You can redistribute it or modify it under terms of the MIT, Apache License or Zlib license
 */

//! Routines for IDCT
//!
//! Essentially we provide 2 routines for IDCT, a scalar implementation and a not super optimized
//! AVX2 one, i'll talk about them here.
//!
//! There are 2 reasons why we have the avx one
//! 1. No one compiles with -C target-features=avx2 hence binaries won't probably take advantage(even
//!    if it exists).
//! 2. AVX employs zero short circuit in a way the scalar code cannot employ it.
//!     - AVX does this by checking for MCU's whose 63 AC coefficients are zero and if true, it writes
//!       values directly, if false, it goes the long way of calculating.
//!     - Although this can be trivially implemented in the scalar version, it  generates code
//!       I'm not happy width(scalar version that basically loops and that is too many branches for me)
//!       The avx one does a better job of using bitwise or's with (`_mm256_or_si256`) which is magnitudes of faster
//!       than anything I could come up with
//!
//! The AVX code also has some cool transpose_u16 instructions which look so complicated to be cool
//! (spoiler alert, i barely understand how it works, that's why I credited the owner).
//!
#![allow(
    clippy::excessive_precision,
    clippy::unreadable_literal,
    clippy::module_name_repetitions,
    unused_parens,
    clippy::wildcard_imports
)]

use zune_core::log::debug;
use zune_core::options::DecoderOptions;

use crate::decoder::IDCTPtr;
use crate::idct::scalar::{idct_int, idct_int_1x1};

// Match libjpeg's JDCT_ISLOW: 13-bit constants and two fractional bits
// between passes. Round once per pass; the final bias also level-shifts by 128.
const CONST_BITS: i32 = 13;
const PASS1_SHIFT: i32 = CONST_BITS - 2;
const PASS1_ROUND: i32 = 1 << (PASS1_SHIFT - 1);
const OUTPUT_SHIFT: i32 = CONST_BITS + 2 + 3;
const SCALE_BITS: i32 = (1 << (OUTPUT_SHIFT - 1)) + (128 << OUTPUT_SHIFT);

#[cfg(feature = "x86")]
pub mod avx2;
#[cfg(feature = "neon")]
pub mod neon;

pub mod scalar;

/// Choose an appropriate IDCT function
#[allow(unused_variables)]
pub fn choose_idct_func(options: &DecoderOptions) -> IDCTPtr {
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    #[cfg(feature = "x86")]
    {
        if options.use_avx2() {
            debug!("Using vector integer IDCT");
            return |a: &mut [i32; 64], b: &mut [i16], c: usize| {
                // SAFETY: `options.use_avx2()` only returns true if avx2 is supported.
                unsafe { avx2::idct_avx2(a,b,c) }
            };
        }
    }
    #[cfg(target_arch = "aarch64")]
    #[cfg(feature = "neon")]
    {
        if options.use_neon() {
            debug!("Using vector integer IDCT");
            return |a: &mut [i32; 64], b: &mut [i16], c: usize| {
                // SAFETY: `options.use_neon()` only returns true if neon is supported.
                unsafe { neon::idct_neon(a,b,c) }
            };
        }
    }
    debug!("Using scalar integer IDCT");
    // use generic one
    return idct_int;
}

/// Choose a function to implement 4x4 IDCT.
///
/// These functions get the same input but have an extra contract: Only the first 4x4 block of
/// coefficients are non-zero. All other entries are zeroed.
///
/// **The callee must uphold that contract on return**
pub fn choose_idct_4x4_func(_options: &DecoderOptions) -> IDCTPtr {
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    #[cfg(feature = "x86")]
    {
        if _options.use_avx2() {
            debug!("Using vector integer IDCT");
            return |a: &mut [i32; 64], b: &mut [i16], c: usize| {
                // SAFETY: `options.use_avx2()` only returns true if avx2 is supported.
                unsafe { avx2::idct_avx2_4x4(a,b,c) }
            };
        }
    }

    scalar::idct4x4
}

pub fn choose_idct_1x1_func(_: &DecoderOptions) -> IDCTPtr {
    // These are simple stores, no alternative implementation for now
    idct_int_1x1
}

#[cfg(test)]
#[allow(unreachable_code)]
#[allow(dead_code)]
mod tests {
    use super::*;

    fn check_idct(input: [i32; 64], expected: [i16; 64], reduced: bool) {
        const STRIDE: usize = 13;
        let mut functions: Vec<(&str, IDCTPtr)> = vec![
            ("scalar", idct_int),
            ("native", choose_idct_func(&DecoderOptions::new_fast())),
        ];
        if reduced {
            functions.push(("scalar 4x4", scalar::idct4x4));
            functions.push(("native 4x4", choose_idct_4x4_func(&DecoderOptions::new_fast())));
        }
        if input[1..].iter().all(|&coefficient| coefficient == 0) {
            functions.push(("DC only", idct_int_1x1));
        }
        for (name, function) in functions {
            let mut coefficients = input;
            let mut output = [-1; 8 * STRIDE];
            function(&mut coefficients, &mut output, STRIDE);
            for (row, expected) in output.chunks_exact(STRIDE).zip(expected.chunks_exact(8)) {
                assert_eq!(&row[..8], expected, "{name}");
                assert_eq!(&row[8..], &[-1; STRIDE - 8], "{name} overwrote padding");
            }
            if name.ends_with("4x4") {
                for (index, coefficient) in coefficients.iter().enumerate() {
                    if index / 8 >= 4 || index % 8 >= 4 {
                        assert_eq!(*coefficient, 0, "{name} violated the 4x4 contract");
                    }
                }
            }
        }
    }

    #[test]
    fn islow_rounding() {
        // Golden samples from libjpeg's jidctint.c, with unit quantization.
        let mut coefficients = [0; 64];
        coefficients[9] = -255;
        #[rustfmt::skip]
        let expected = [
            67, 76, 93, 116, 140, 163, 180, 189,
            76, 84, 99, 118, 138, 157, 172, 180,
            93, 99, 108, 121, 135, 148, 157, 163,
            116, 118, 121, 126, 130, 135, 138, 140,
            140, 138, 135, 130, 126, 121, 118, 116,
            163, 157, 148, 135, 121, 108, 99, 93,
            180, 172, 157, 138, 118, 99, 84, 76,
            189, 180, 163, 140, 116, 93, 76, 67,
        ];
        check_idct(coefficients, expected, true);

        coefficients[9] = 0;
        coefficients[0] = 255;
        coefficients[63] = -256;
        #[rustfmt::skip]
        let expected = [
            157, 167, 150, 172, 148, 170, 153, 162,
            167, 140, 189, 125, 195, 130, 180, 153,
            149, 189, 116, 212, 108, 204, 130, 170,
            172, 125, 212, 98, 221, 108, 195, 148,
            148, 195, 108, 221, 98, 212, 125, 172,
            170, 130, 204, 108, 212, 116, 189, 149,
            153, 180, 130, 195, 125, 189, 140, 167,
            162, 153, 170, 148, 172, 149, 167, 157,
        ];
        check_idct(coefficients, expected, false);
    }

    #[test]
    fn islow_4x4_rounding() {
        #[rustfmt::skip]
        let coefficients = [
            -254, -7, 0, 0, 0, 0, 0, 0,
            7, 0, -30, 32, 0, 0, 0, 0,
            7, 0, -30, 32, 0, 0, 0, 0,
            7, 0, -30, 32, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
        ];
        #[rustfmt::skip]
        let expected = [
            98, 86, 85, 106, 131, 130, 97, 64,
            96, 92, 92, 100, 109, 109, 97, 85,
            94, 98, 99, 93, 86, 87, 97, 107,
            94, 99, 100, 91, 80, 82, 97, 113,
            95, 97, 98, 94, 89, 90, 97, 104,
            95, 95, 95, 96, 97, 97, 97, 97,
            95, 96, 97, 95, 93, 94, 97, 101,
            94, 98, 99, 93, 85, 86, 97, 108,
        ];
        check_idct(coefficients, expected, true);
    }

    #[test]
    fn islow_final_rounding_and_clipping() {
        // An extra first-pass rounding bias in the final pass changes 227 to 228.
        let mut row = [0, 0, 29, 93, 163, 227, 255, 255];
        for coefficient in [-1013, 1013] {
            let mut coefficients = [0; 64];
            coefficients[1] = coefficient;
            let mut expected = [0; 64];
            for output_row in expected.chunks_exact_mut(8) {
                output_row.copy_from_slice(&row);
            }
            check_idct(coefficients, expected, true);
            row.reverse();
        }
    }

    #[test]
    fn dc_rounding_and_extremes() {
        for dc in [-2048, -1024, -1023, -4, -1, 0, 3, 4, 1015, 1016, 1023, 2047] {
            let mut coefficients = [0; 64];
            coefficients[0] = dc;
            let sample = (((dc + 4) >> 3) + 128).clamp(0, 255) as i16;
            check_idct(coefficients, [sample; 64], true);
        }
    }

    #[test]
    fn scalar_simd_consistency() {
        let mut state = 0x12345678_u32;
        for reduced in [false, true] {
            for _ in 0..256 {
                let mut coefficients = [0; 64];
                for (index, coefficient) in coefficients.iter_mut().enumerate() {
                    state = state.wrapping_mul(1664525).wrapping_add(1013904223);
                    if !reduced || (index / 8 < 4 && index % 8 < 4) {
                        *coefficient = (state >> 16) as i32 - 32768;
                    }
                }
                let mut expected = [0; 64];
                let mut reference_coefficients = coefficients;
                idct_int(&mut reference_coefficients, &mut expected, 8);
                check_idct(coefficients, expected, reduced);
            }
        }
    }

    #[test]
    fn idct_test0() {
        let stride = 8;
        let mut coeff = [10; 64];
        let mut coeff2 = [10; 64];
        let mut output_scalar = [0; 64];
        let mut output_vector = [0; 64];
        let idct_func = choose_idct_func(&DecoderOptions::new_fast());
        idct_func(&mut coeff, &mut output_vector, stride);
        idct_int(&mut coeff2, &mut output_scalar, stride);
        assert_eq!(output_scalar, output_vector, "IDCT and scalar do not match");
    }

    #[test]
    fn do_idct_test1() {
        let stride = 8;
        let mut coeff = [14; 64];
        let mut coeff2 = [14; 64];
        let mut output_scalar = [0; 64];
        let mut output_vector = [0; 64];
        let idct_func = choose_idct_func(&DecoderOptions::new_fast());
        idct_func(&mut coeff, &mut output_vector, stride);
        idct_int(&mut coeff2, &mut output_scalar, stride);
        assert_eq!(output_scalar, output_vector, "IDCT and scalar do not match");
    }

    #[test]
    fn do_idct_test2() {
        let stride = 8;
        let mut coeff = [0; 64];
        coeff[0] = 255;
        coeff[63] = -256;
        let mut coeff2 = coeff;
        let mut output_scalar = [0; 64];
        let mut output_vector = [0; 64];
        let idct_func = choose_idct_func(&DecoderOptions::new_fast());
        idct_func(&mut coeff, &mut output_vector, stride);
        idct_int(&mut coeff2, &mut output_scalar, stride);
        assert_eq!(output_scalar, output_vector, "IDCT and scalar do not match");
    }

    #[test]
    fn do_idct_zeros() {
        let stride = 8;
        let mut coeff = [0; 64];
        let mut coeff2 = [0; 64];
        let mut output_scalar = [0; 64];
        let mut output_vector = [0; 64];
        let idct_func = choose_idct_func(&DecoderOptions::new_fast());
        idct_func(&mut coeff, &mut output_vector, stride);
        idct_int(&mut coeff2, &mut output_scalar, stride);
        assert_eq!(output_scalar, output_vector, "IDCT and scalar do not match");
    }

    #[test]
    fn idct_4x4() {
        #[rustfmt::skip]
        const A: [i32; 32] = [
            -254, -7, 0, 0, 0, 0, 0, 0,
            7, 0, -30, 32,  0, 0, 0, 0,
            7, 0, -30, 32,  0, 0, 0, 0,
            7, 0, -30, 32,  0, 0, 0, 0,
        ];

        let v: Vec<IDCTPtr> = vec![
            choose_idct_func(&DecoderOptions::new_safe()),
            choose_idct_4x4_func(&DecoderOptions::new_safe()),
            choose_idct_func(&DecoderOptions::new_fast()),
            choose_idct_4x4_func(&DecoderOptions::new_fast()),
        ];
        let dct_names = vec![
            "safe idct",
            "safe idct 4x4",
            "fast idct",
            "fast idct 4x4",
        ];

        let mut color = vec![];

        for idct in v {
            let mut a = [0i32; 64];
            a[..32].copy_from_slice(&A);
            let mut b = [0i16; 64];

            idct(&mut a, &mut b, 8);

            color.push(b);
        }

        for (wnd, name) in color.windows(2).zip(&dct_names) {
            let [a, b] = wnd else { unreachable!() };
            assert_eq!(a, b, "{name}");
        }
    }
}
