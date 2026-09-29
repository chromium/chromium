/*
 * Copyright (c) 2023.
 *
 * This software is free software;
 *
 * You can redistribute it or modify it under terms of the MIT, Apache License or Zlib license
 */

#![allow(
    clippy::many_single_char_names,
    clippy::similar_names,
    clippy::cast_possible_truncation,
    clippy::cast_sign_loss,
    clippy::cast_possible_wrap,
    clippy::too_many_arguments,
    clippy::doc_markdown
)]

//! Color space conversion routines
//!
//! This files exposes functions to convert one colorspace to another in a jpeg
//! image
//!
//! Currently supported conversions are
//!
//! - `YCbCr` to `RGB,RGBA,GRAYSCALE,RGBX`.
//!
//!
//! Hey there, if your reading this it means you probably need something, so let me help you.
//!
//! There are 3 supported cpu extensions here.
//! 1. Scalar
//! 2. SSE
//! 3. AVX
//!
//! There are two types of the color convert functions
//!
//! 1. Acts on 16 pixels.
//! 2. Acts on 8 pixels.
//!
//! The reason for this is because when implementing the AVX part it occurred to me that we can actually
//! do better and process 2 MCU's if we change IDCT return type to be `i16's`, since a lot of
//! CPU's these days support AVX extensions, it becomes nice if we optimize for that path ,
//! therefore AVX routines can process 16 pixels directly and SSE and Scalar just compensate.
//!
//! By compensating, I mean I wrote the 16 pixels version operating on the 8 pixel version twice.
//!
//! Therefore if your looking to optimize some routines, probably start there.

pub use scalar::ycbcr_to_grayscale;
use zune_core::colorspace::ColorSpace;
use zune_core::options::DecoderOptions;

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
#[cfg(feature = "x86")]
pub use crate::color_convert::avx::{
    ycbcr_to_bgr_avx2, ycbcr_to_bgra_avx2, ycbcr_to_rgb_avx2, ycbcr_to_rgba_avx2
};
use crate::decoder::ColorConvert16Ptr;

mod avx;
mod neon64;
mod scalar;

#[allow(unused_variables)]
pub fn choose_ycbcr_to_rgb_convert_func(
    type_need: ColorSpace, options: &DecoderOptions
) -> Option<ColorConvert16Ptr> {
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    #[cfg(feature = "x86")]
    {
        use zune_core::log::debug;
        if options.use_avx2() {
            debug!("Using AVX optimised color conversion functions");

            // I believe avx2 means sse4 is also available
            // match colorspace
            match type_need {
                ColorSpace::RGB => return Some(ycbcr_to_rgb_avx2),
                ColorSpace::RGBA => return Some(ycbcr_to_rgba_avx2),
                ColorSpace::BGR => return Some(ycbcr_to_bgr_avx2),
                ColorSpace::BGRA => return Some(ycbcr_to_bgra_avx2),
                _ => () // fall through to scalar, which has more types
            };
        }
    }
    #[cfg(all(feature = "neon", target_arch = "aarch64"))]
    {
        if options.use_neon() {
            use crate::color_convert::neon64::{
                ycbcr_to_bgr_neon, ycbcr_to_bgra_neon, ycbcr_to_rgb_neon, ycbcr_to_rgba_neon
            };
            match type_need {
                ColorSpace::RGB => return Some(ycbcr_to_rgb_neon),
                ColorSpace::RGBA => return Some(ycbcr_to_rgba_neon),
                ColorSpace::BGR => return Some(ycbcr_to_bgr_neon),
                ColorSpace::BGRA => return Some(ycbcr_to_bgra_neon),
                _ => () // fall through to scalar, which has more types
            }
        }
    }
    // when there is no x86 or we haven't returned by here, resort to scalar
    return match type_need {
        ColorSpace::RGB => Some(scalar::ycbcr_to_rgb_inner_16_scalar::<false>),
        ColorSpace::RGBA => Some(scalar::ycbcr_to_rgba_inner_16_scalar::<false>),
        ColorSpace::BGRA => Some(scalar::ycbcr_to_rgba_inner_16_scalar::<true>),
        ColorSpace::BGR => Some(scalar::ycbcr_to_rgb_inner_16_scalar::<true>),
        _ => None
    };
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::color_convert::scalar::{
        ycbcr_to_rgb_inner_16_scalar, ycbcr_to_rgba_inner_16_scalar
    };

    fn input_vectors() -> Vec<([i16; 16], [i16; 16], [i16; 16])> {
        let mut vectors = vec![
            ([0; 16], [0; 16], [0; 16]),
            ([255; 16], [255; 16], [255; 16]),
            ([128; 16], [128; 16], [128; 16]),
            (
                core::array::from_fn(|index| (index * 17) as i16),
                core::array::from_fn(|index| (255 - index * 13) as i16),
                core::array::from_fn(|index| (index * 11) as i16)
            )
        ];
        let mut state = 0xC0FF_EE12_u32;
        for _ in 0..256 {
            let mut next = || {
                state = state.wrapping_mul(1_664_525).wrapping_add(1_013_904_223);
                (state >> 24) as i16
            };
            vectors.push((
                core::array::from_fn(|_| next()),
                core::array::from_fn(|_| next()),
                core::array::from_fn(|_| next())
            ));
        }
        vectors
    }

    fn assert_converter_matches_scalar(
        vector: ColorConvert16Ptr, scalar: ColorConvert16Ptr, output_size: usize,
        has_alpha: bool
    ) {
        const OFFSET: usize = 7;
        const GUARD: usize = 11;
        const SENTINEL: u8 = 0xCD;

        for (y, cb, cr) in input_vectors() {
            let mut expected = vec![SENTINEL; OFFSET + output_size + GUARD];
            let mut actual = expected.clone();
            let mut expected_offset = OFFSET;
            let mut actual_offset = OFFSET;
            scalar(&y, &cb, &cr, &mut expected, &mut expected_offset);
            vector(&y, &cb, &cr, &mut actual, &mut actual_offset);
            assert_eq!(actual_offset, OFFSET + output_size);
            assert_eq!(actual, expected);
            assert!(actual[..OFFSET].iter().all(|byte| *byte == SENTINEL));
            assert!(actual[OFFSET + output_size..]
                .iter()
                .all(|byte| *byte == SENTINEL));
            if has_alpha {
                assert!(actual[OFFSET + 3..OFFSET + output_size]
                    .iter()
                    .step_by(4)
                    .all(|alpha| *alpha == 255));
            }
        }

        let y = [128; 16];
        let cb = [128; 16];
        let cr = [128; 16];
        let mut exact = vec![0; output_size];
        let mut offset = 0;
        vector(&y, &cb, &cr, &mut exact, &mut offset);
        assert_eq!(offset, output_size);

        let mut short = vec![SENTINEL; output_size - 1];
        let mut short_offset = 0;
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            vector(&y, &cb, &cr, &mut short, &mut short_offset);
        }));
        assert!(result.is_err());
        assert_eq!(short_offset, 0);
        assert!(short.iter().all(|byte| *byte == SENTINEL));
    }

    #[test]
    #[cfg(all(feature = "x86", any(target_arch = "x86", target_arch = "x86_64")))]
    fn avx2_bgr_and_bgra_match_scalar() {
        if !std::is_x86_feature_detected!("avx2") {
            return;
        }
        assert_converter_matches_scalar(
            ycbcr_to_bgr_avx2,
            ycbcr_to_rgb_inner_16_scalar::<true>,
            48,
            false
        );
        assert_converter_matches_scalar(
            ycbcr_to_bgra_avx2,
            ycbcr_to_rgba_inner_16_scalar::<true>,
            64,
            true
        );

        let options = DecoderOptions::default();
        assert!(options.use_avx2());
        assert_converter_matches_scalar(
            choose_ycbcr_to_rgb_convert_func(ColorSpace::BGR, &options).unwrap(),
            ycbcr_to_rgb_inner_16_scalar::<true>,
            48,
            false
        );
        assert_converter_matches_scalar(
            choose_ycbcr_to_rgb_convert_func(ColorSpace::BGRA, &options).unwrap(),
            ycbcr_to_rgba_inner_16_scalar::<true>,
            64,
            true
        );
    }

    #[test]
    #[cfg(all(feature = "neon", target_arch = "aarch64"))]
    fn neon_bgr_and_bgra_match_scalar() {
        use crate::color_convert::neon64::{ycbcr_to_bgr_neon, ycbcr_to_bgra_neon};

        assert_converter_matches_scalar(
            ycbcr_to_bgr_neon,
            ycbcr_to_rgb_inner_16_scalar::<true>,
            48,
            false
        );
        assert_converter_matches_scalar(
            ycbcr_to_bgra_neon,
            ycbcr_to_rgba_inner_16_scalar::<true>,
            64,
            true
        );

        let options = DecoderOptions::default();
        assert!(options.use_neon());
        assert_converter_matches_scalar(
            choose_ycbcr_to_rgb_convert_func(ColorSpace::BGR, &options).unwrap(),
            ycbcr_to_rgb_inner_16_scalar::<true>,
            48,
            false
        );
        assert_converter_matches_scalar(
            choose_ycbcr_to_rgb_convert_func(ColorSpace::BGRA, &options).unwrap(),
            ycbcr_to_rgba_inner_16_scalar::<true>,
            64,
            true
        );
    }

    #[test]
    #[cfg(not(any(
        all(feature = "x86", any(target_arch = "x86", target_arch = "x86_64")),
        all(feature = "neon", target_arch = "aarch64")
    )))]
    fn no_simd_build_dispatches_bgr_and_bgra_to_scalar() {
        let options = DecoderOptions::default();
        assert_eq!(
            choose_ycbcr_to_rgb_convert_func(ColorSpace::BGR, &options).unwrap() as usize,
            ycbcr_to_rgb_inner_16_scalar::<true> as usize
        );
        assert_eq!(
            choose_ycbcr_to_rgb_convert_func(ColorSpace::BGRA, &options).unwrap() as usize,
            ycbcr_to_rgba_inner_16_scalar::<true> as usize
        );
    }
}
