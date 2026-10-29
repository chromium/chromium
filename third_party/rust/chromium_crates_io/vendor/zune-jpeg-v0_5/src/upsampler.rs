/*
 * Copyright (c) 2023.
 *
 * This software is free software;
 *
 * You can redistribute it or modify it under terms of the MIT, Apache License or Zlib license
 */
#![allow(
    clippy::used_underscore_binding,
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::used_underscore_items
)]

//! Up-sampling routines
//!
//! The main upsampling method is a bi-linear interpolation or a "triangle
//! filter " or libjpeg turbo `fancy_upsampling` which is a good compromise
//! between speed and visual quality
//!
//! # The filter
//! Each output pixel is made from `(3*A+B)/4` where A is the original
//! pixel closer to the output and B is the one further.
//!
//! ```text
//!+---+---+
//! | A | B |
//! +---+---+
//! +-+-+-+-+
//! | |P| | |
//! +-+-+-+-+
//! ```
//!
//! # Horizontal Bi-linear filter
//! ```text
//! |---+-----------+---+
//! |   |           |   |
//! | A | |p1 | p2| | B |
//! |   |           |   |
//! |---+-----------+---+
//!
//! ```
//! For a horizontal bi-linear it's trivial to implement,
//!
//! `A` becomes the input closest to the output.
//!
//! `B` varies depending on output.
//!  - For odd positions, input is the `next` pixel after A
//!  - For even positions, input is the `previous` value before A.
//!
//! We iterate in a classic 1-D sliding window with a window of 3.
//! For our sliding window approach, `A` is the 1st and `B` is either the 0th term or 2nd term
//! depending on position we are writing.(see scalar code).
//!
//! For vector code see module sse for explanation.
//!
//! # Vertical bi-linear.
//! Vertical up-sampling is a bit trickier.
//!
//! ```text
//! +----+----+
//! | A1 | A2 |
//! +----+----+
//! +----+----+
//! | p1 | p2 |
//! +----+-+--+
//! +----+-+--+
//! | p3 | p4 |
//! +----+-+--+
//! +----+----+
//! | B1 | B2 |
//! +----+----+
//! ```
//!
//! For `p1`
//! - `A1` is given a weight of `3` and `B1` is given a weight of 1.
//!
//! For `p3`
//! - `B1` is given a weight of `3` and `A1` is given a weight of 1
//!
//! # Horizontal vertical downsampling/chroma quartering.
//!
//! Keep vertical weighted sums at full precision through horizontal filtering.
//! Apply the native alternating rounding biases only at the final output.
#![allow(unreachable_code)]
use zune_core::options::DecoderOptions;

use crate::components::UpSampler;

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
#[cfg(feature = "x86")]
mod avx2;
#[cfg(target_arch = "aarch64")]
#[cfg(feature = "neon")]
mod neon;
#[cfg(feature = "portable_simd")]
mod portable_simd;
mod scalar;

// choose the best possible implementation for this platform
#[allow(unused_variables)]
pub fn choose_horizontal_samp_function(options: &DecoderOptions) -> UpSampler {
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    #[cfg(feature = "x86")]
    if options.use_avx2() {
        return |a: &[i16], b: &[i16], c: &[i16], d: &mut [i16], e: &mut [i16]| {
            // SAFETY: `options.use_avx2()` only returns true if avx2 is supported.
            unsafe { avx2::upsample_horizontal_avx2(a, b, c, d, e) }
        };
    }
    #[cfg(target_arch = "aarch64")]
    #[cfg(feature = "neon")]
    if options.use_neon() {
        return |a: &[i16], b: &[i16], c: &[i16], d: &mut [i16], e: &mut [i16]| {
            // SAFETY: `options.use_neon()` only returns true if neon is supported.
            unsafe { neon::upsample_horizontal_neon(a, b, c, d, e) }
        };
    }
    #[cfg(feature = "portable_simd")]
    return portable_simd::upsample_horizontal_simd;
    return scalar::upsample_horizontal;
}

#[allow(unused_variables)]
pub fn choose_hv_samp_function(options: &DecoderOptions) -> UpSampler {
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    #[cfg(feature = "x86")]
    if options.use_avx2() {
        return |a: &[i16], b: &[i16], c: &[i16], d: &mut [i16], e: &mut [i16]| {
            // SAFETY: `options.use_avx2()` only returns true if avx2 is supported.
            unsafe { avx2::upsample_hv_avx2(a, b, c, d, e) }
        };
    }
    #[cfg(target_arch = "aarch64")]
    #[cfg(feature = "neon")]
    if options.use_neon() {
        return |a: &[i16], b: &[i16], c: &[i16], d: &mut [i16], e: &mut [i16]| {
            // SAFETY: `options.use_neon()` only returns true if neon is supported.
            unsafe { neon::upsample_hv_neon(a, b, c, d, e) }
        };
    }
    #[cfg(feature = "portable_simd")]
    return portable_simd::upsample_hv_simd;
    return scalar::upsample_hv;
}

#[allow(unused_variables)]
pub fn choose_v_samp_function(options: &DecoderOptions) -> UpSampler {
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    #[cfg(feature = "x86")]
    if options.use_avx2() {
        return |a: &[i16], b: &[i16], c: &[i16], d: &mut [i16], e: &mut [i16]| {
            // SAFETY: `options.use_avx2()` only returns true if avx2 is supported.
            unsafe { avx2::upsample_vertical_avx2(a, b, c, d, e) }
        };
    }
    #[cfg(target_arch = "aarch64")]
    #[cfg(feature = "neon")]
    if options.use_neon() {
        return |a: &[i16], b: &[i16], c: &[i16], d: &mut [i16], e: &mut [i16]| {
            // SAFETY: `options.use_neon()` only returns true if neon is supported.
            unsafe { neon::upsample_vertical_neon(a, b, c, d, e) }
        };
    }
    #[cfg(feature = "portable_simd")]
    return portable_simd::upsample_vertical_simd;
    return scalar::upsample_vertical;
}

/// Upsample nothing
pub fn upsample_no_op(
    _input: &[i16], _in_ref: &[i16], _in_near: &[i16], _scratch_space: &mut [i16],
    _output: &mut [i16],
) {
}

pub fn generic_sampler() -> UpSampler {
    scalar::upsample_generic
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn native_fancy_upsampling_rounding() {
        let mut horizontal = [0; 6];
        scalar::upsample_horizontal(&[2, 0, 2], &[], &[], &mut [], &mut horizontal);
        assert_eq!(horizontal, [2, 2, 0, 1, 1, 2]);

        let mut vertical = [0; 6];
        scalar::upsample_vertical(&[0, 1, 2], &[2, 0, 0], &[2, 0, 0], &mut [], &mut vertical);
        assert_eq!(vertical, [0, 1, 1, 1, 1, 2]);

        let mut hv = [0; 12];
        scalar::upsample_hv(&[2, 0, 2], &[0, 2, 0], &[1, 1, 1], &mut [0; 6], &mut hv);
        assert_eq!(hv, [2, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 2]);
    }

    #[test]
    fn partial_mcu_rgb_matches_native() {
        // A 14x10 4:2:0 image with AC chroma in the partial right/bottom MCU.
        // The RGB hash is the native JDCT_ISLOW + fancy-upsampling result.
        const JPEG: &[u8] = &[
            0xff, 0xd8, 0xff, 0xdb, 0x00, 0x43, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
            0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
            0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
            0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
            0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
            0x01, 0xff, 0xc0, 0x00, 0x11, 0x08, 0x00, 0x0a, 0x00, 0x0e, 0x03, 0x01, 0x22, 0x00,
            0x02, 0x11, 0x00, 0x03, 0x11, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xff, 0xc4, 0x00, 0x16, 0x10, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x15, 0xff, 0xda, 0x00, 0x0c,
            0x03, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x5f, 0x37, 0xdf,
            0xff, 0xd9
        ];
        for options in [DecoderOptions::new_safe(), DecoderOptions::new_fast()] {
            let mut decoder = crate::JpegDecoder::new_with_options(
                zune_core::bytestream::ZCursor::new(JPEG),
                options
            );
            let pixels = decoder.decode().unwrap();
            assert_eq!(pixels.len(), 14 * 10 * 3);
            let hash = pixels.iter().fold(0xcbf2_9ce4_8422_2325_u64, |hash, byte| {
                (hash ^ u64::from(*byte)).wrapping_mul(0x100_0000_01b3)
            });
            assert_eq!(hash, 0x17be_0800_0e74_5511);
        }
    }

    #[test]
    fn native_fancy_upsampling_dispatch_and_boundaries() {
        let options = DecoderOptions::default();
        for width in (3..68).chain([128, 511, 1024]) {
            let input: Vec<i16> = (0..width)
                .map(|x| i16::try_from((x * 73 + x / 3) % 256).unwrap())
                .collect();
            let near: Vec<i16> = (0..width)
                .map(|x| i16::try_from((x * 19 + 255) % 256).unwrap())
                .collect();
            let far: Vec<i16> = (0..width)
                .map(|x| i16::try_from((x * 151 + 1) % 256).unwrap())
                .collect();
            for (horizontal, vertical, sampler) in [
                (true, false, choose_horizontal_samp_function(&options)),
                (false, true, choose_v_samp_function(&options)),
                (true, true, choose_hv_samp_function(&options))
            ] {
                let row_width = width * if horizontal { 2 } else { 1 };
                let height = if vertical { 2 } else { 1 };
                let length = row_width * height;
                let mut output = vec![-1; length + 5];
                sampler(
                    &input,
                    &near,
                    &far,
                    &mut vec![0; width * 2],
                    &mut output[..length]
                );
                for y in 0..height {
                    for x in 0..row_width {
                        let source = x / if horizontal { 2 } else { 1 };
                        let adjacent = if x % 2 == 0 {
                            source.saturating_sub(1)
                        } else {
                            (source + 1).min(width - 1)
                        };
                        let current = i32::from(input[source]);
                        let neighbor = if y == 0 { &near } else { &far };
                        let expected = match (horizontal, vertical) {
                            (true, true) => {
                                let a = 3 * current + i32::from(neighbor[source]);
                                let b =
                                    3 * i32::from(input[adjacent]) + i32::from(neighbor[adjacent]);
                                (3 * a + b + if x % 2 == 0 { 8 } else { 7 }) >> 4
                            }
                            (true, false) => {
                                (3 * current
                                    + i32::from(input[adjacent])
                                    + if x % 2 == 0 { 1 } else { 2 })
                                    >> 2
                            }
                            (false, true) => {
                                (3 * current
                                    + i32::from(neighbor[source])
                                    + if y == 0 { 1 } else { 2 })
                                    >> 2
                            }
                            _ => unreachable!()
                        };
                        assert_eq!(
                            output[y * row_width + x],
                            i16::try_from(expected).unwrap(),
                            "width={width} x={x} y={y} horizontal={horizontal} vertical={vertical}"
                        );
                    }
                }
                assert_eq!(&output[length..], &[-1; 5]);
            }
        }
    }

    #[cfg(feature = "portable_simd")]
    mod portable_simd_impl {
        use super::*;

        #[test]
        fn portable_simd_vertical() {
            _test_vertical(portable_simd::upsample_vertical_simd)
        }

        #[test]
        fn portable_simd_horizontal() {
            _test_horizontal(portable_simd::upsample_horizontal_simd)
        }

        #[test]
        fn portable_simd_hv() {
            _test_hv(portable_simd::upsample_hv_simd)
        }
    }

    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    #[cfg(feature = "x86")]
    #[cfg(target_feature = "avx2")]
    mod avx2_impl {
        use super::*;

        #[test]
        fn avx2_vertical() {
            _test_vertical(
                |a: &[i16], b: &[i16], c: &[i16], d: &mut [i16], e: &mut [i16]| {
                    // SAFETY: Test guarded behind `target_feature`
                    unsafe { avx2::upsample_vertical_avx2(a, b, c, d, e) }
                },
            )
        }

        #[test]
        fn avx2_horizontal() {
            _test_horizontal(
                |a: &[i16], b: &[i16], c: &[i16], d: &mut [i16], e: &mut [i16]| {
                    // SAFETY: Test guarded behind `target_feature`
                    unsafe { avx2::upsample_horizontal_avx2(a, b, c, d, e) }
                },
            )
        }

        #[test]
        fn avx2_hv() {
            _test_hv(
                |a: &[i16], b: &[i16], c: &[i16], d: &mut [i16], e: &mut [i16]| {
                    // SAFETY: Test guarded behind `target_feature`
                    unsafe { avx2::upsample_hv_avx2(a, b, c, d, e) }
                },
            )
        }
    }

    #[cfg(target_arch = "aarch64")]
    #[cfg(feature = "neon")]
    #[cfg(target_feature = "neon")]
    mod neon_impl {
        use super::*;

        #[test]
        fn neon_vertical() {
            _test_vertical(
                |a: &[i16], b: &[i16], c: &[i16], d: &mut [i16], e: &mut [i16]| {
                    // SAFETY: Test guarded behind `target_feature`
                    unsafe { neon::upsample_vertical_neon(a, b, c, d, e) }
                },
            );
        }

        #[test]
        fn neon_horizontal() {
            _test_horizontal(
                |a: &[i16], b: &[i16], c: &[i16], d: &mut [i16], e: &mut [i16]| {
                    // SAFETY: Test guarded behind `target_feature`
                    unsafe { neon::upsample_horizontal_neon(a, b, c, d, e) }
                },
            );
        }

        #[test]
        fn neon_hv() {
            _test_hv(
                |a: &[i16], b: &[i16], c: &[i16], d: &mut [i16], e: &mut [i16]| {
                    // SAFETY: Test guarded behind `target_feature`
                    unsafe { neon::upsample_hv_neon(a, b, c, d, e) }
                },
            );
        }
    }

    fn _test_vertical(upsampler: UpSampler) {
        let width = 1024;
        let input: Vec<i16> = (0..width).map(|x| ((x + 10) % 256) as i16).collect();
        let in_near: Vec<i16> = (0..width).map(|x| ((x + 20) % 256) as i16).collect();
        let in_far: Vec<i16> = (0..width).map(|x| ((x + 30) % 256) as i16).collect();
        let mut scratch = vec![0i16; width];

        let mut output_scalar = vec![0i16; width * 2];
        let mut output_fast = vec![0i16; width * 2];

        scalar::upsample_vertical(&input, &in_near, &in_far, &mut scratch, &mut output_scalar);
        upsampler(&input, &in_near, &in_far, &mut scratch, &mut output_fast);

        assert_eq!(output_scalar, output_fast);
    }

    fn _test_horizontal(upsampler: UpSampler) {
        _test_horizontal_even_width(upsampler);
        _test_horizontal_odd_width(upsampler);
    }

    fn _test_horizontal_even_width(upsampler: UpSampler) {
        let width = 1024;
        let input: Vec<i16> = (0..width).map(|x| ((x + 10) % 256) as i16).collect();

        let mut scratch = vec![0i16; width];

        let mut output_scalar = vec![0i16; width * 2];
        let mut output_fast = vec![0i16; width * 2];

        scalar::upsample_horizontal(&input, &[], &[], &mut scratch, &mut output_scalar);
        upsampler(&input, &[], &[], &mut scratch, &mut output_fast);

        assert_eq!(output_scalar, output_fast);
    }

    fn _test_horizontal_odd_width(upsampler: UpSampler) {
        let width = 33;
        let input: Vec<i16> = (0..width).map(|x| ((x + 10) % 256) as i16).collect();
        let mut scratch = vec![0i16; width];
        let mut output_scalar = vec![0i16; width * 2];
        let mut output_fast = vec![0i16; width * 2];

        scalar::upsample_horizontal(&input, &[], &[], &mut scratch, &mut output_scalar);
        upsampler(&input, &[], &[], &mut scratch, &mut output_fast);

        assert_eq!(output_scalar, output_fast);
    }

    fn _test_hv(upsampler: UpSampler) {
        let width = 512;
        let input: Vec<i16> = (0..width).map(|x| ((x + 10) % 256) as i16).collect();
        let in_near: Vec<i16> = (0..width).map(|x| ((x + 20) % 256) as i16).collect();
        let in_far: Vec<i16> = (0..width).map(|x| ((x + 30) % 256) as i16).collect();

        // Output len is width * 4 for HV (vertical * 2, then horizontal * 2 for each row)
        // scratch is width * 2
        let mut scratch_scalar = vec![0i16; width * 2];
        let mut scratch_fast = vec![0i16; width * 2];
        let mut output_scalar = vec![0i16; width * 4];
        let mut output_fast = vec![0i16; width * 4];

        scalar::upsample_hv(
            &input,
            &in_near,
            &in_far,
            &mut scratch_scalar,
            &mut output_scalar,
        );
        upsampler(
            &input,
            &in_near,
            &in_far,
            &mut scratch_fast,
            &mut output_fast,
        );

        assert_eq!(output_scalar, output_fast);
    }
}
