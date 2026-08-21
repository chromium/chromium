// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::api::{
    JxlColorType, JxlDataFormat, JxlDecoder, JxlDecoderInner, JxlDecoderOptions, JxlPixelFormat,
    JxlTransferFunction, ProcessingResult, states,
};
use crate::error::Error;
use crate::image::{Image, JxlOutputBuffer, Rect};
use std::path::Path;

use crate::tests::decode::{compare_frames, decode, decode_internal, scan_frames_with_decoder};

#[test]
fn decode_small_chunks() {
    arbtest::arbtest(|u| {
        decode_internal(
            &std::fs::read("resources/test/green_queen_vardct_e3.jxl").unwrap(),
            u.arbitrary::<u8>().unwrap() as usize + 1,
            false,
            false,
            None,
            None,
            None,
        )
        .unwrap();
        Ok(())
    });
}

// OOO jxlp boxes require any frame to start in a box that has all the logically-before
// boxes physically before it, and all the logically-after boxes physically after it.
// This test file does *not* satisfy this property.
#[test]
fn decode_ooo_jxlp_invalid_animated_container() {
    let data = std::fs::read("resources/test/invalid_animated_ooo_jxlp.jxl").unwrap();
    let res = decode(&data);
    assert!(
        matches!(res, Err(Error::InvalidBox)),
        "expected error due to frame start in non-valid checkpoint box"
    );
}

#[test]
fn test_preview_size_none_for_regular_files() {
    let file = std::fs::read("resources/test/basic.jxl").unwrap();
    let options = JxlDecoderOptions::default();
    let mut decoder = JxlDecoder::<states::Initialized>::new(options);
    let mut input = file.as_slice();
    let decoder = loop {
        match decoder.process(&mut input, None).unwrap() {
            ProcessingResult::Complete { result } => break result,
            ProcessingResult::NeedsMoreInput { fallback, .. } => decoder = fallback,
        }
    };
    assert!(decoder.basic_info().preview_size.is_none());
}

#[test]
fn test_preview_size_some_for_preview_files() {
    let file = std::fs::read("resources/test/with_preview.jxl").unwrap();
    let options = JxlDecoderOptions::default();
    let mut decoder = JxlDecoder::<states::Initialized>::new(options);
    let mut input = file.as_slice();
    let decoder = loop {
        match decoder.process(&mut input, None).unwrap() {
            ProcessingResult::Complete { result } => break result,
            ProcessingResult::NeedsMoreInput { fallback, .. } => decoder = fallback,
        }
    };
    assert_eq!(decoder.basic_info().preview_size, Some((16, 16)));
}

#[test]
fn test_set_pixel_format() {
    let file = std::fs::read("resources/test/basic.jxl").unwrap();
    let options = JxlDecoderOptions::default();
    let mut decoder = JxlDecoder::<states::Initialized>::new(options);
    let mut input = file.as_slice();
    let mut decoder = loop {
        match decoder.process(&mut input, None).unwrap() {
            ProcessingResult::Complete { result } => break result,
            ProcessingResult::NeedsMoreInput { fallback, .. } => decoder = fallback,
        }
    };
    let default_format = decoder.current_pixel_format().clone();
    assert_eq!(default_format.color_type, JxlColorType::Rgb);

    let new_format = JxlPixelFormat {
        color_type: JxlColorType::Grayscale,
        color_data_format: Some(JxlDataFormat::U8 { bit_depth: 8 }),
        extra_channel_format: vec![],
    };
    decoder.set_pixel_format(new_format.clone());
    assert_eq!(decoder.current_pixel_format(), &new_format);
}

#[test]
fn test_default_output_tf_by_pixel_format() {
    let file = std::fs::read("resources/test/lossy_with_icc.jxl").unwrap();
    let options = JxlDecoderOptions::default();
    let mut decoder = JxlDecoder::<states::Initialized>::new(options);
    let mut input = file.as_slice();
    let mut decoder = loop {
        match decoder.process(&mut input, None).unwrap() {
            ProcessingResult::Complete { result } => break result,
            ProcessingResult::NeedsMoreInput { fallback, .. } => decoder = fallback,
        }
    };

    assert_eq!(
        *decoder.output_color_profile().transfer_function().unwrap(),
        JxlTransferFunction::Linear,
    );

    decoder.set_pixel_format(JxlPixelFormat::rgba8(0));
    assert_eq!(
        *decoder.output_color_profile().transfer_function().unwrap(),
        JxlTransferFunction::SRGB,
    );

    decoder.set_pixel_format(JxlPixelFormat::rgba_f16(0));
    assert_eq!(
        *decoder.output_color_profile().transfer_function().unwrap(),
        JxlTransferFunction::Linear,
    );

    decoder.set_pixel_format(JxlPixelFormat::rgba16(0));
    assert_eq!(
        *decoder.output_color_profile().transfer_function().unwrap(),
        JxlTransferFunction::SRGB,
    );
}

#[test]
fn test_fill_opaque_alpha_both_pipelines() {
    let file = std::fs::read("resources/test/basic.jxl").unwrap();
    let rgba_format = JxlPixelFormat {
        color_type: JxlColorType::Rgba,
        color_data_format: Some(JxlDataFormat::f32()),
        extra_channel_format: vec![],
    };

    for use_simple in [true, false] {
        let options = JxlDecoderOptions::default();
        let decoder = JxlDecoder::<states::Initialized>::new(options);
        let mut input = file.as_slice();

        macro_rules! advance_decoder {
            ($decoder:expr) => {
                loop {
                    match $decoder.process(&mut input, None).unwrap() {
                        ProcessingResult::Complete { result } => break result,
                        ProcessingResult::NeedsMoreInput { fallback, .. } => {
                            if input.is_empty() {
                                panic!("Unexpected end of input");
                            }
                            $decoder = fallback;
                        }
                    }
                }
            };
            ($decoder:expr, $buffers:expr) => {
                loop {
                    match $decoder.process(&mut input, $buffers, None).unwrap() {
                        ProcessingResult::Complete { result } => break result,
                        ProcessingResult::NeedsMoreInput { fallback, .. } => {
                            if input.is_empty() {
                                panic!("Unexpected end of input");
                            }
                            $decoder = fallback;
                        }
                    }
                }
            };
        }

        let mut decoder = decoder;
        let mut decoder = advance_decoder!(decoder);
        decoder.set_use_simple_pipeline(use_simple);
        decoder.set_pixel_format(rgba_format.clone());

        let basic_info = decoder.basic_info().clone();
        let (width, height) = basic_info.size;
        let mut decoder = advance_decoder!(decoder);

        let mut color_buffer = Image::<f32>::new((width * 4, height)).unwrap();
        let mut buffers: Vec<_> = vec![JxlOutputBuffer::from_image_rect_mut(
            color_buffer
                .get_rect_mut(Rect {
                    origin: (0, 0),
                    size: (width * 4, height),
                })
                .into_raw(),
        )];

        let _decoder = advance_decoder!(decoder, &mut buffers);

        for y in 0..height {
            let row = color_buffer.row(y);
            for x in 0..width {
                let alpha = row[x * 4 + 3];
                assert_eq!(
                    alpha, 1.0,
                    "Alpha at ({},{}) should be 1.0, got {} (use_simple={})",
                    x, y, alpha, use_simple
                );
            }
        }
    }
}

/// Test that premultiply_output=true produces premultiplied alpha output
/// from a source with straight (non-premultiplied) alpha.
#[test]
fn test_premultiply_output_straight_alpha() {
    let file =
        std::fs::read("resources/test/conformance_test_images/alpha_nonpremultiplied.jxl").unwrap();

    let rgba_format = JxlPixelFormat {
        color_type: JxlColorType::Rgba,
        color_data_format: Some(JxlDataFormat::f32()),
        extra_channel_format: vec![None],
    };

    for use_simple in [true, false] {
        let (straight_buffer, width, height) =
            decode_with_format::<f32>(&file, &rgba_format, use_simple, false);
        let (premul_buffer, _, _) =
            decode_with_format::<f32>(&file, &rgba_format, use_simple, true);

        let mut found_semitransparent = false;
        for y in 0..height {
            let straight_row = straight_buffer.row(y);
            let premul_row = premul_buffer.row(y);
            for x in 0..width {
                let sr = straight_row[x * 4];
                let sg = straight_row[x * 4 + 1];
                let sb = straight_row[x * 4 + 2];
                let sa = straight_row[x * 4 + 3];

                let pr = premul_row[x * 4];
                let pg = premul_row[x * 4 + 1];
                let pb = premul_row[x * 4 + 2];
                let pa = premul_row[x * 4 + 3];

                assert!(
                    (sa - pa).abs() < 1e-5,
                    "Alpha mismatch at ({},{}): straight={}, premul={} (use_simple={})",
                    x,
                    y,
                    sa,
                    pa,
                    use_simple
                );

                let expected_r = sr * sa;
                let expected_g = sg * sa;
                let expected_b = sb * sa;

                let tol = 0.01;
                assert!(
                    (expected_r - pr).abs() < tol,
                    "R mismatch at ({},{}): expected={}, got={} (use_simple={})",
                    x,
                    y,
                    expected_r,
                    pr,
                    use_simple
                );
                assert!(
                    (expected_g - pg).abs() < tol,
                    "G mismatch at ({},{}): expected={}, got={} (use_simple={})",
                    x,
                    y,
                    expected_g,
                    pg,
                    use_simple
                );
                assert!(
                    (expected_b - pb).abs() < tol,
                    "B mismatch at ({},{}): expected={}, got={} (use_simple={})",
                    x,
                    y,
                    expected_b,
                    pb,
                    use_simple
                );

                if sa > 0.01 && sa < 0.99 {
                    found_semitransparent = true;
                }
            }
        }

        assert!(
            found_semitransparent,
            "Test image should have semi-transparent pixels (use_simple={})",
            use_simple
        );
    }
}

/// Test that premultiply_output=true doesn't double-premultiply
/// when the source already has premultiplied alpha (alpha_associated=true).
#[test]
fn test_premultiply_output_already_premultiplied() {
    let file =
        std::fs::read("resources/test/conformance_test_images/alpha_premultiplied.jxl").unwrap();

    let rgba_format = JxlPixelFormat {
        color_type: JxlColorType::Rgba,
        color_data_format: Some(JxlDataFormat::f32()),
        extra_channel_format: vec![None],
    };

    for use_simple in [true, false] {
        let (without_flag_buffer, width, height) =
            decode_with_format::<f32>(&file, &rgba_format, use_simple, false);
        let (with_flag_buffer, _, _) =
            decode_with_format::<f32>(&file, &rgba_format, use_simple, true);

        for y in 0..height {
            let without_row = without_flag_buffer.row(y);
            let with_row = with_flag_buffer.row(y);
            for x in 0..width {
                for c in 0..4 {
                    let without_val = without_row[x * 4 + c];
                    let with_val = with_row[x * 4 + c];
                    assert!(
                        (without_val - with_val).abs() < 1e-5,
                        "Mismatch at ({},{}) channel {}: without_flag={}, with_flag={} (use_simple={})",
                        x,
                        y,
                        c,
                        without_val,
                        with_val,
                        use_simple
                    );
                }
            }
        }
    }
}

/// Test that animations with reference frames work correctly.
#[test]
fn test_animation_with_reference_frames() {
    let file =
        std::fs::read("resources/test/conformance_test_images/animation_spline.jxl").unwrap();

    let options = JxlDecoderOptions::default();
    let decoder = JxlDecoder::<states::Initialized>::new(options);
    let mut input = file.as_slice();

    let mut decoder = decoder;
    let mut decoder = loop {
        match decoder.process(&mut input, None).unwrap() {
            ProcessingResult::Complete { result } => break result,
            ProcessingResult::NeedsMoreInput { fallback, .. } => {
                decoder = fallback;
            }
        }
    };

    let rgb_format = JxlPixelFormat {
        color_type: JxlColorType::Rgb,
        color_data_format: Some(JxlDataFormat::f32()),
        extra_channel_format: vec![],
    };
    decoder.set_pixel_format(rgb_format);

    let basic_info = decoder.basic_info().clone();
    let (width, height) = basic_info.size;

    let mut frame_count = 0;

    loop {
        let mut decoder_frame = loop {
            match decoder.process(&mut input, None).unwrap() {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => {
                    decoder = fallback;
                }
            }
        };

        let mut color_buffer = Image::<f32>::new((width * 3, height)).unwrap();
        let mut buffers: Vec<_> = vec![JxlOutputBuffer::from_image_rect_mut(
            color_buffer
                .get_rect_mut(Rect {
                    origin: (0, 0),
                    size: (width * 3, height),
                })
                .into_raw(),
        )];

        decoder = loop {
            match decoder_frame
                .process(&mut input, &mut buffers, None)
                .unwrap()
            {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => {
                    decoder_frame = fallback;
                }
            }
        };

        frame_count += 1;

        if !decoder.has_more_frames() {
            break;
        }
    }

    assert!(
        frame_count > 1,
        "Expected multiple frames in animation, got {}",
        frame_count
    );
}

#[test]
fn test_skip_frame_then_decode_next() {
    let file =
        std::fs::read("resources/test/conformance_test_images/animation_spline.jxl").unwrap();

    let options = JxlDecoderOptions::default();
    let decoder = JxlDecoder::<states::Initialized>::new(options);
    let mut input = file.as_slice();

    let mut decoder = decoder;
    let mut decoder = loop {
        match decoder.process(&mut input, None).unwrap() {
            ProcessingResult::Complete { result } => break result,
            ProcessingResult::NeedsMoreInput { fallback, .. } => {
                decoder = fallback;
            }
        }
    };

    let rgb_format = JxlPixelFormat {
        color_type: JxlColorType::Rgb,
        color_data_format: Some(JxlDataFormat::f32()),
        extra_channel_format: vec![],
    };
    decoder.set_pixel_format(rgb_format);

    let basic_info = decoder.basic_info().clone();
    let (width, height) = basic_info.size;

    let mut decoder_frame = loop {
        match decoder.process(&mut input, None).unwrap() {
            ProcessingResult::Complete { result } => break result,
            ProcessingResult::NeedsMoreInput { fallback, .. } => {
                decoder = fallback;
            }
        }
    };

    let mut decoder = loop {
        match decoder_frame.skip_frame(&mut input).unwrap() {
            ProcessingResult::Complete { result } => break result,
            ProcessingResult::NeedsMoreInput { fallback, .. } => {
                decoder_frame = fallback;
            }
        }
    };

    assert!(
        decoder.has_more_frames(),
        "Animation should have more frames"
    );

    let mut decoder_frame = loop {
        match decoder.process(&mut input, None).unwrap() {
            ProcessingResult::Complete { result } => break result,
            ProcessingResult::NeedsMoreInput { fallback, .. } => {
                decoder = fallback;
            }
        }
    };

    let mut color_buffer = Image::<f32>::new((width * 3, height)).unwrap();
    let mut buffers: Vec<_> = vec![JxlOutputBuffer::from_image_rect_mut(
        color_buffer
            .get_rect_mut(Rect {
                origin: (0, 0),
                size: (width * 3, height),
            })
            .into_raw(),
    )];

    let decoder = loop {
        match decoder_frame
            .process(&mut input, &mut buffers, None)
            .unwrap()
        {
            ProcessingResult::Complete { result } => break result,
            ProcessingResult::NeedsMoreInput { fallback, .. } => {
                decoder_frame = fallback;
            }
        }
    };

    let _ = decoder.has_more_frames();
}

/// Test that u8 output matches f32 output within quantization tolerance.
#[test]
fn test_output_format_u8_matches_f32() {
    let file = std::fs::read("resources/test/conformance_test_images/bicycles.jxl").unwrap();

    for (color_type, num_samples) in [(JxlColorType::Rgb, 3), (JxlColorType::Bgra, 4)] {
        let f32_format = JxlPixelFormat {
            color_type,
            color_data_format: Some(JxlDataFormat::f32()),
            extra_channel_format: vec![],
        };
        let u8_format = JxlPixelFormat {
            color_type,
            color_data_format: Some(JxlDataFormat::U8 { bit_depth: 8 }),
            extra_channel_format: vec![],
        };

        for use_simple in [true, false] {
            let (f32_buffer, width, height) =
                decode_with_format::<f32>(&file, &f32_format, use_simple, false);
            let (u8_buffer, _, _) = decode_with_format::<u8>(&file, &u8_format, use_simple, false);

            let tolerance = 0.004;
            let mut max_error: f32 = 0.0;

            for y in 0..height {
                let f32_row = f32_buffer.row(y);
                let u8_row = u8_buffer.row(y);
                for x in 0..(width * num_samples) {
                    let f32_val = f32_row[x].clamp(0.0, 1.0);
                    let u8_val = u8_row[x] as f32 / 255.0;
                    let error = (f32_val - u8_val).abs();
                    max_error = max_error.max(error);
                    assert!(
                        error < tolerance,
                        "{:?} u8 mismatch at ({},{}): f32={}, u8={} (scaled={}), error={} (use_simple={})",
                        color_type,
                        x,
                        y,
                        f32_val,
                        u8_row[x],
                        u8_val,
                        error,
                        use_simple
                    );
                }
            }
        }
    }
}

/// Test that u16 output matches f32 output within quantization tolerance.
#[test]
fn test_output_format_u16_matches_f32() {
    use crate::api::Endianness;

    let file = std::fs::read("resources/test/conformance_test_images/bicycles.jxl").unwrap();

    for (color_type, num_samples) in [(JxlColorType::Rgb, 3), (JxlColorType::Bgra, 4)] {
        let f32_format = JxlPixelFormat {
            color_type,
            color_data_format: Some(JxlDataFormat::f32()),
            extra_channel_format: vec![],
        };
        let u16_format = JxlPixelFormat {
            color_type,
            color_data_format: Some(JxlDataFormat::U16 {
                endianness: Endianness::native(),
                bit_depth: 16,
            }),
            extra_channel_format: vec![],
        };

        for use_simple in [true, false] {
            let (f32_buffer, width, height) =
                decode_with_format::<f32>(&file, &f32_format, use_simple, false);
            let (u16_buffer, _, _) =
                decode_with_format::<u16>(&file, &u16_format, use_simple, false);

            let tolerance = 0.0001;

            for y in 0..height {
                let f32_row = f32_buffer.row(y);
                let u16_row = u16_buffer.row(y);
                for x in 0..(width * num_samples) {
                    let f32_val = f32_row[x].clamp(0.0, 1.0);
                    let u16_val = u16_row[x] as f32 / 65535.0;
                    let error = (f32_val - u16_val).abs();
                    assert!(
                        error < tolerance,
                        "{:?} u16 mismatch at ({},{}): f32={}, u16={} (scaled={}), error={} (use_simple={})",
                        color_type,
                        x,
                        y,
                        f32_val,
                        u16_row[x],
                        u16_val,
                        error,
                        use_simple
                    );
                }
            }
        }
    }
}

/// Test that f16 output matches f32 output within f16 precision tolerance.
#[test]
fn test_output_format_f16_matches_f32() {
    use crate::api::Endianness;
    use crate::util::f16;

    let file = std::fs::read("resources/test/conformance_test_images/bicycles.jxl").unwrap();

    for (color_type, num_samples) in [(JxlColorType::Rgb, 3), (JxlColorType::Bgra, 4)] {
        let f32_format = JxlPixelFormat {
            color_type,
            color_data_format: Some(JxlDataFormat::f32()),
            extra_channel_format: vec![],
        };
        let f16_format = JxlPixelFormat {
            color_type,
            color_data_format: Some(JxlDataFormat::F16 {
                endianness: Endianness::native(),
            }),
            extra_channel_format: vec![],
        };

        for use_simple in [true, false] {
            let (f32_buffer, width, height) =
                decode_with_format::<f32>(&file, &f32_format, use_simple, false);
            let (f16_buffer, _, _) =
                decode_with_format::<f16>(&file, &f16_format, use_simple, false);

            let tolerance = 0.002;

            for y in 0..height {
                let f32_row = f32_buffer.row(y);
                let f16_row = f16_buffer.row(y);
                for x in 0..(width * num_samples) {
                    let f32_val = f32_row[x];
                    let f16_val = f16_row[x].to_f32();
                    let error = (f32_val - f16_val).abs();
                    assert!(
                        error < tolerance,
                        "{:?} f16 mismatch at ({},{}): f32={}, f16={}, error={} (use_simple={})",
                        color_type,
                        x,
                        y,
                        f32_val,
                        f16_val,
                        error,
                        use_simple
                    );
                }
            }
        }
    }
}

/// Helper function to decode an image with a specific format.
fn decode_with_format<T: crate::image::ImageDataType>(
    file: &[u8],
    pixel_format: &JxlPixelFormat,
    use_simple: bool,
    premultiply: bool,
) -> (Image<T>, usize, usize) {
    let options = JxlDecoderOptions {
        premultiply_output: premultiply,
        ..Default::default()
    };
    let mut decoder = JxlDecoder::<states::Initialized>::new(options);
    let mut input = file;

    let mut decoder = loop {
        match decoder.process(&mut input, None).unwrap() {
            ProcessingResult::Complete { result } => break result,
            ProcessingResult::NeedsMoreInput { fallback, .. } => {
                if input.is_empty() {
                    panic!("Unexpected end of input");
                }
                decoder = fallback;
            }
        }
    };
    decoder.set_use_simple_pipeline(use_simple);
    decoder.set_pixel_format(pixel_format.clone());

    let basic_info = decoder.basic_info().clone();
    let (width, height) = basic_info.size;

    let num_samples = pixel_format.color_type.samples_per_pixel();

    let decoder = loop {
        match decoder.process(&mut input, None).unwrap() {
            ProcessingResult::Complete { result } => break result,
            ProcessingResult::NeedsMoreInput { fallback, .. } => {
                if input.is_empty() {
                    panic!("Unexpected end of input");
                }
                decoder = fallback;
            }
        }
    };

    let mut buffer = Image::<T>::new((width * num_samples, height)).unwrap();
    let mut buffers: Vec<_> = vec![JxlOutputBuffer::from_image_rect_mut(
        buffer
            .get_rect_mut(Rect {
                origin: (0, 0),
                size: (width * num_samples, height),
            })
            .into_raw(),
    )];

    let mut decoder = decoder;
    loop {
        match decoder.process(&mut input, &mut buffers, None).unwrap() {
            ProcessingResult::Complete { .. } => break,
            ProcessingResult::NeedsMoreInput { fallback, .. } => {
                if input.is_empty() {
                    panic!("Unexpected end of input");
                }
                decoder = fallback;
            }
        }
    }

    (buffer, width, height)
}

/// Regression test for ClusterFuzz issue 5342436251336704
#[test]
fn test_fuzzer_smallbuffer_overflow() {
    use std::panic;

    let data = include_bytes!("../../tests/testdata/fuzzer_smallbuffer_overflow.jxl");

    let result = panic::catch_unwind(|| {
        let _ = decode_internal(data, 1024, false, false, None, None, None);
    });

    if let Err(e) = result {
        let panic_msg = e
            .downcast_ref::<&str>()
            .map(|s| s.to_string())
            .or_else(|| e.downcast_ref::<String>().cloned())
            .unwrap_or_default();
        assert!(
            !panic_msg.contains("overflow"),
            "Unexpected overflow panic: {}",
            panic_msg
        );
    }
}

/// Regression test for https://issues.chromium.org/issues/541318910: flushing a
/// frame that does not support rendering before the last pass used to force an
/// eager render of an incomplete group.
#[test]
fn flush_without_partial_render_support() {
    let data = std::fs::read("resources/test/squeeze_empty_residual.jxl").unwrap();
    for chunk_size in 1..=16 {
        decode_internal(&data, chunk_size, false, true, None, None, None).unwrap();
    }
}

fn make_box(ty: &[u8; 4], content: &[u8]) -> Vec<u8> {
    let len = (8 + content.len()) as u32;
    let mut buf = Vec::new();
    buf.extend(len.to_be_bytes());
    buf.extend(ty);
    buf.extend(content);
    buf
}

fn add_container_header(container: &mut Vec<u8>) {
    let sig = [
        0x00, 0x00, 0x00, 0x0c, 0x4a, 0x58, 0x4c, 0x20, 0x0d, 0x0a, 0x87, 0x0a,
    ];
    let ftyp = make_box(b"ftyp", b"jxl \x00\x00\x00\x00jxl ");
    container.extend(&sig);
    container.extend(&ftyp);
}

fn wrap_with_jxlp_chunks(codestream: &[u8], chunk_starts: &[usize]) -> Vec<u8> {
    let mut starts = chunk_starts.to_vec();
    starts.sort_unstable();
    starts.dedup();
    if starts.first().copied() != Some(0) {
        starts.insert(0, 0);
    }
    if starts.last().copied() != Some(codestream.len()) {
        starts.push(codestream.len());
    }
    assert!(starts.len() >= 2);

    let mut container = Vec::new();
    add_container_header(&mut container);

    let num_chunks = starts.len() - 1;
    for i in 0..num_chunks {
        let begin = starts[i];
        let end = starts[i + 1];
        assert!(begin <= end && end <= codestream.len());

        let mut payload = Vec::with_capacity(4 + (end - begin));
        let mut index = i as u32;
        if i + 1 == num_chunks {
            index |= 0x8000_0000;
        }
        payload.extend(index.to_be_bytes());
        payload.extend(&codestream[begin..end]);
        container.extend(make_box(b"jxlp", &payload));
    }

    container
}

fn assert_start_new_frame_matches_sequential(data: &[u8]) {
    let scanned_frames = scan_frames_with_decoder(data, usize::MAX);

    let (_n, sequential_frames) = decode(data).unwrap();

    arbtest::arbtest(|u| {
        let initial_offset =
            u.int_in_range(scanned_frames[0].file_offset..=data.len() as u64)? as usize;

        let options = JxlDecoderOptions::default();
        let mut decoder = JxlDecoderInner::new(options);
        let mut input = &data[..initial_offset];

        while let ProcessingResult::Complete { .. } =
            decoder.process(&mut input, None, None).unwrap()
        {
            if input.is_empty() {
                break;
            }
        }

        let num_seeks = u.int_in_range(1..=3)?;
        for _ in 0..num_seeks {
            let target_visible_index =
                u.int_in_range(0..=scanned_frames.len() as u64 - 1)? as usize;
            let seek_target = scanned_frames[target_visible_index].seek_target;

            let expected = &sequential_frames[target_visible_index];

            decoder.start_new_frame(seek_target);
            let mut input = &data[seek_target.decode_start_file_offset as usize..];

            let result = decoder.process(&mut input, None, None);
            assert!(
                matches!(result, Ok(ProcessingResult::Complete { .. })),
                "decoder.process: {result:?}"
            );

            let basic_info = decoder.basic_info().unwrap().clone();
            let (width, height) = basic_info.size;

            let default_format = decoder.current_pixel_format().unwrap().clone();
            let requested_format = JxlPixelFormat {
                color_type: default_format.color_type,
                color_data_format: Some(JxlDataFormat::f32()),
                extra_channel_format: default_format
                    .extra_channel_format
                    .iter()
                    .map(|_| Some(JxlDataFormat::f32()))
                    .collect(),
            };
            decoder.set_pixel_format(requested_format.clone());

            let channels = requested_format.color_type.samples_per_pixel();
            let num_ec = requested_format.extra_channel_format.len();

            let mut color_buffer = Image::<f32>::new((width * channels, height)).unwrap();
            let mut ec_buffers: Vec<Image<f32>> = (0..num_ec)
                .map(|_| Image::<f32>::new((width, height)).unwrap())
                .collect();
            let mut buffers: Vec<JxlOutputBuffer> = vec![JxlOutputBuffer::from_image_rect_mut(
                color_buffer
                    .get_rect_mut(Rect {
                        origin: (0, 0),
                        size: (width * channels, height),
                    })
                    .into_raw(),
            )];
            for ec in ec_buffers.iter_mut() {
                buffers.push(JxlOutputBuffer::from_image_rect_mut(
                    ec.get_rect_mut(Rect {
                        origin: (0, 0),
                        size: (width, height),
                    })
                    .into_raw(),
                ));
            }

            assert!(matches!(
                decoder.process(&mut input, Some(&mut buffers), None),
                Ok(ProcessingResult::Complete { .. })
            ));

            let mut seek_decoded = Vec::with_capacity(1 + num_ec);
            seek_decoded.push(color_buffer);
            seek_decoded.extend(ec_buffers);
            compare_frames(
                Path::new("start_new_frame_seek"),
                target_visible_index,
                expected,
                &seek_decoded,
            );

            let available_bytes = input.len();
            let extra_bytes = u.int_in_range(0..=available_bytes as u64)? as usize;
            if extra_bytes == 0 {
                continue;
            }
            let mut extra_input = &input[..extra_bytes];

            while let ProcessingResult::Complete { .. } =
                decoder.process(&mut extra_input, None, None).unwrap()
            {
                if extra_input.is_empty() {
                    break;
                }
            }
        }
        Ok(())
    });
}

#[test]
fn test_start_new_frame_bare_codestream() {
    let data =
        std::fs::read("resources/test/conformance_test_images/animation_icos4d.jxl").unwrap();
    assert_start_new_frame_matches_sequential(&data);
}

#[test]
fn test_start_new_frame_boxed_jxlp_per_visible_frame() {
    let codestream =
        std::fs::read("resources/test/conformance_test_images/animation_icos4d.jxl").unwrap();

    let scanned_frames = scan_frames_with_decoder(&codestream, usize::MAX);
    assert!(scanned_frames.len() > 1, "need multiple frames");

    let (decoded_frames, _) = decode(&codestream).unwrap();
    assert_eq!(
        decoded_frames,
        scanned_frames.len(),
        "test file should have one codestream frame per visible frame",
    );

    let mut chunk_starts: Vec<usize> = scanned_frames
        .iter()
        .map(|f| f.file_offset as usize)
        .collect();
    chunk_starts.sort_unstable();
    chunk_starts.dedup();
    assert_eq!(chunk_starts.len(), scanned_frames.len());

    let container = wrap_with_jxlp_chunks(&codestream, &chunk_starts);
    assert_start_new_frame_matches_sequential(&container);
}

#[test]
fn test_start_new_frame_cropped_traffic_light() {
    let data = std::fs::read("resources/test/cropped_traffic_light.jxl").unwrap();
    assert_start_new_frame_matches_sequential(&data);
}

#[test]
fn test_scan_still_image() {
    let data = std::fs::read("resources/test/green_queen_vardct_e3.jxl").unwrap();
    let frames = scan_frames_with_decoder(&data, usize::MAX);

    assert_eq!(frames.len(), 1);
    assert!(frames[0].is_last);
    assert!(frames[0].is_keyframe);
    let total_duration_ms: f64 = frames.iter().map(|f| f.duration_ms).sum();
    assert_eq!(total_duration_ms, 0.0);
}

#[test]
fn test_scan_bare_animation() {
    let data =
        std::fs::read("resources/test/conformance_test_images/animation_icos4d_5.jxl").unwrap();
    let frames = scan_frames_with_decoder(&data, usize::MAX);

    assert!(frames.len() > 1, "expected multiple frames");

    for (i, frame) in frames.iter().enumerate() {
        assert_eq!(frame.index, i);
    }

    assert!(frames.last().unwrap().is_last);
    assert!(frames[0].is_keyframe);
    assert_eq!(
        frames[0].seek_target.decode_start_file_offset,
        frames[0].file_offset as u64
    );
}

#[test]
fn test_scan_animation_offsets_increase() {
    let data =
        std::fs::read("resources/test/conformance_test_images/animation_icos4d_5.jxl").unwrap();
    let frames = scan_frames_with_decoder(&data, usize::MAX);

    for i in 1..frames.len() {
        assert!(
            frames[i].file_offset > frames[i - 1].file_offset,
            "frame {} offset {} should be > frame {} offset {}",
            i,
            frames[i].file_offset,
            i - 1,
            frames[i - 1].file_offset,
        );
    }
}

#[test]
fn test_scan_incremental() {
    let data =
        std::fs::read("resources/test/conformance_test_images/animation_icos4d_5.jxl").unwrap();

    let frames = scan_frames_with_decoder(&data, 128);
    assert!(frames.len() > 1);
    assert!(frames.last().unwrap().is_last);
}

#[test]
fn test_scan_keyframe_detection_still() {
    let data = std::fs::read("resources/test/green_queen_vardct_e3.jxl").unwrap();
    let frames = scan_frames_with_decoder(&data, usize::MAX);

    assert_eq!(frames.len(), 1);
    let f = &frames[0];
    assert!(f.is_keyframe);
    assert_eq!(f.seek_target.decode_start_file_offset, f.file_offset as u64);
    assert_eq!(f.seek_target.visible_frames_to_skip, 0);
}

#[test]
fn test_scan_decode_start_file_offset_consistency() {
    let data =
        std::fs::read("resources/test/conformance_test_images/animation_icos4d_5.jxl").unwrap();

    let frames = scan_frames_with_decoder(&data, usize::MAX);

    for frame in &frames {
        assert!(
            frame.seek_target.decode_start_file_offset <= frame.file_offset,
            "frame {}: decode_start_file_offset {} > file_offset {}",
            frame.index,
            frame.seek_target.decode_start_file_offset,
            frame.file_offset,
        );
        assert_eq!(
            frame.is_keyframe,
            frame.seek_target.visible_frames_to_skip == 0,
            "frame {}: keyframe flag should match visible_frames_to_skip",
            frame.index,
        );
    }
}

#[test]
fn test_scan_with_preview() {
    let data = std::fs::read("resources/test/with_preview.jxl");
    if data.is_err() {
        return;
    }
    let data = data.unwrap();
    let frames = scan_frames_with_decoder(&data, usize::MAX);

    assert!(frames.len() <= 1);
}

#[test]
fn test_scan_patches_not_keyframe() {
    let data = std::fs::read("resources/test/grayscale_patches_var_dct.jxl");
    if data.is_err() {
        return;
    }
    let data = data.unwrap();
    let frames = scan_frames_with_decoder(&data, usize::MAX);

    assert!(!frames.is_empty());
}

/// Regression test for Chromium ClusterFuzz issue 474401148.
#[test]
fn test_fuzzer_xyb_icc_no_panic() {
    #[rustfmt::skip]
    let data: &[u8] = &[
        0xff, 0x0a, 0x01, 0x00, 0x00, 0x04, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x25, 0x00,
    ];

    let mut decoder = JxlDecoderInner::new(Default::default());
    let mut input = data;

    if let Ok(ProcessingResult::Complete { .. }) = decoder.process(&mut input, None, None)
        && let Some(profile) = decoder.output_color_profile()
    {
        let _ = profile.try_as_icc();
    }
}

/// Regression test for Chromium ClusterFuzz issue 502853162.
#[test]
fn test_scan_frames_only_empty_followup_no_panic_502853162() {
    #[rustfmt::skip]
    let data: &[u8] = &[
        0xff, 0x0a, 0x31, 0xbd, 0xa2, 0xd0, 0x2a, 0x18,
        0x07, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x0f, 0xa0, 0x26, 0x00, 0xff,
    ];

    let opts = JxlDecoderOptions {
        scan_frames_only: true,
        ..Default::default()
    };
    let mut decoder = JxlDecoderInner::new(opts);

    let mut input = data;
    while decoder.has_more_frames() && !input.is_empty() {
        let _ = decoder.process(&mut input, None, None).unwrap();
    }
}

/// Small regression test for issue #728: squeeze transform boundary bug.
#[test]
fn test_squeeze_boundary_minimal() {
    let (_, frames) =
        decode(&std::fs::read("resources/test/issue728_minimal.jxl").unwrap()).unwrap();
    assert_eq!(frames.len(), 1);
    let frame = &frames[0];
    let buf = &frame[0];
    let (xs, ys) = buf.size();
    for y in 0..ys {
        let row = buf.row(y);
        for (x, &v) in row.iter().enumerate().take(xs) {
            assert!(
                v == 0.0 || v == 1.0,
                "pixel ({}, {}) has value {v}, expected 0.0 or 1.0 \
                 (issue #728 squeeze boundary bug - minimal test)",
                x / 3,
                y,
            );
        }
    }
}

/// Regression test for grid boundary bug with odd-width images (issue #728 variant).
#[test]
fn decode_test_strategic_solid_blue_grid_boundary() {
    let (_, frames) =
        decode(&std::fs::read("resources/test/strategic_solid_blue.jxl").unwrap()).unwrap();
    assert_eq!(frames.len(), 1);
    let frame = &frames[0];

    let buf = &frame[0];
    let (xs, ys) = buf.size();

    assert_eq!(xs, 257 * 3);
    assert_eq!(ys, 256);

    for y in 0..ys {
        for x in 0..257 {
            let row = buf.row(y);
            let (r, g, b) = (row[x * 3], row[x * 3 + 1], row[x * 3 + 2]);
            assert_eq!(
                (r, g, b),
                (0.0, 0.0, 1.0),
                "pixel ({}, {}) has value ({}, {}, {}), expected (0.0, 0.0, 1.0)",
                x,
                y,
                r,
                g,
                b,
            );
        }
    }
}
