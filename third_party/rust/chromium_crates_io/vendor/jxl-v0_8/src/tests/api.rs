// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::path::Path;

use crate::api::{
    AlphaMode, Event, JxlAuxBoxType, JxlColorType, JxlDataFormat, JxlDecoder, JxlDecoderOptions,
    JxlPixelFormat, JxlTransferFunction, VisibleFrameInfo,
};
use crate::error::Error;
use crate::image::{Image, ImageDataType};
use crate::tests::decode::{DecodeParams, as_output_buffers, compare_frames, decode, scan_frames};

// OOO jxlp boxes require any frame to start in a box that has all the logically-before
// boxes physically before it, and all the logically-after boxes physically after it.
// This test file does *not* satisfy this property.
#[test]
fn decode_ooo_jxlp_invalid_animated_container() {
    let data = std::fs::read("resources/test/invalid_animated_ooo_jxlp.jxl").unwrap();
    let res = decode::<f32>(&data, Default::default());
    assert!(
        matches!(res, Err(Error::InvalidBox)),
        "expected error due to frame start in non-valid checkpoint box"
    );
}

#[test]
fn test_preview_size_none_for_regular_files() {
    let file = std::fs::read("resources/test/basic.jxl").unwrap();
    let options = JxlDecoderOptions::default();
    let mut decoder = JxlDecoder::new(options);
    let mut input = file.as_slice();
    assert_eq!(
        decoder.process(&mut input, None, None).unwrap(),
        Event::BasicInfo
    );
    assert!(decoder.basic_info().unwrap().preview_size.is_none());
}

#[test]
fn test_preview_size_some_for_preview_files() {
    let file = std::fs::read("resources/test/with_preview.jxl").unwrap();
    let options = JxlDecoderOptions::default();
    let mut decoder = JxlDecoder::new(options);
    let mut input = file.as_slice();
    assert_eq!(
        decoder.process(&mut input, None, None).unwrap(),
        Event::BasicInfo
    );
    assert_eq!(decoder.basic_info().unwrap().preview_size, Some((16, 16)));
}

#[test]
fn test_set_pixel_format() {
    let file = std::fs::read("resources/test/basic.jxl").unwrap();
    let options = JxlDecoderOptions::default();
    let mut decoder = JxlDecoder::new(options);
    let mut input = file.as_slice();
    assert_eq!(
        decoder.process(&mut input, None, None).unwrap(),
        Event::BasicInfo
    );
    let default_format = decoder.current_pixel_format().unwrap().clone();
    assert_eq!(default_format.color_type, JxlColorType::Rgb);

    let new_format = JxlPixelFormat {
        color_type: JxlColorType::Grayscale,
        color_data_format: Some(JxlDataFormat::U8 { bit_depth: 8 }),
        extra_channel_format: vec![],
    };
    decoder.set_pixel_format(new_format.clone()).unwrap();
    assert_eq!(decoder.current_pixel_format().unwrap(), &new_format);
}

#[test]
fn test_default_output_tf_by_pixel_format() {
    let file = std::fs::read("resources/test/lossy_with_icc.jxl").unwrap();
    let options = JxlDecoderOptions::default();
    let mut decoder = JxlDecoder::new(options);
    let mut input = file.as_slice();
    assert_eq!(
        decoder.process(&mut input, None, None).unwrap(),
        Event::BasicInfo
    );

    assert_eq!(
        *decoder
            .output_color_profile()
            .unwrap()
            .transfer_function()
            .unwrap(),
        JxlTransferFunction::Linear,
    );

    decoder.set_pixel_format(JxlPixelFormat::rgba8(0)).unwrap();
    assert_eq!(
        *decoder
            .output_color_profile()
            .unwrap()
            .transfer_function()
            .unwrap(),
        JxlTransferFunction::SRGB,
    );

    decoder
        .set_pixel_format(JxlPixelFormat::rgba_f16(0))
        .unwrap();
    assert_eq!(
        *decoder
            .output_color_profile()
            .unwrap()
            .transfer_function()
            .unwrap(),
        JxlTransferFunction::Linear,
    );

    decoder.set_pixel_format(JxlPixelFormat::rgba16(0)).unwrap();
    assert_eq!(
        *decoder
            .output_color_profile()
            .unwrap()
            .transfer_function()
            .unwrap(),
        JxlTransferFunction::SRGB,
    );
}

#[test]
fn test_fill_opaque_alpha_both_pipelines() {
    let file = std::fs::read("resources/test/basic.jxl").unwrap();

    for use_simple in [true, false] {
        let frames = decode::<f32>(
            &file,
            DecodeParams {
                pixel_format: Some(JxlPixelFormat::rgba_f32(0)),
                use_simple_pipeline: use_simple,
                ..Default::default()
            },
        )
        .unwrap();
        let color_buffer = &frames[0][0];
        let (xs, height) = color_buffer.size();
        let width = xs / 4;

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

fn assert_alpha_mode_conversion(
    path: &str,
    target_mode: AlphaMode,
    factor_fn: impl Fn(f32) -> f32,
    tol: f32,
) {
    let file = std::fs::read(path).unwrap();

    for use_simple in [true, false] {
        let as_is_frames = decode::<f32>(
            &file,
            DecodeParams {
                pixel_format: Some(JxlPixelFormat::rgba_f32(1)),
                use_simple_pipeline: use_simple,
                alpha_mode: AlphaMode::AsIs,
                ..Default::default()
            },
        )
        .unwrap();
        let as_is_buffer = &as_is_frames[0][0];
        let converted_frames = decode::<f32>(
            &file,
            DecodeParams {
                pixel_format: Some(JxlPixelFormat::rgba_f32(1)),
                use_simple_pipeline: use_simple,
                alpha_mode: target_mode,
                ..Default::default()
            },
        )
        .unwrap();
        let converted_buffer = &converted_frames[0][0];
        let (xs, height) = as_is_buffer.size();
        let width = xs / 4;

        let mut found_semitransparent = false;
        for y in 0..height {
            let as_is_row = as_is_buffer.row(y);
            let converted_row = converted_buffer.row(y);
            for x in 0..width {
                let [sr, sg, sb, sa] = as_is_row[x * 4..x * 4 + 4] else {
                    unreachable!()
                };
                let [cr, cg, cb, ca] = converted_row[x * 4..x * 4 + 4] else {
                    unreachable!()
                };

                assert!(
                    (sa - ca).abs() < 1e-5,
                    "Alpha mismatch at ({x},{y}): as_is={sa}, converted={ca} (use_simple={use_simple})"
                );

                let mul = factor_fn(sa);
                for (ch, expected, got) in [
                    ('R', sr * mul, cr),
                    ('G', sg * mul, cg),
                    ('B', sb * mul, cb),
                ] {
                    assert!(
                        (expected - got).abs() < tol,
                        "{ch} mismatch at ({x},{y}): expected={expected}, got={got} (use_simple={use_simple})"
                    );
                }

                if sa > 0.01 && sa < 0.99 {
                    found_semitransparent = true;
                }
            }
        }

        assert!(
            found_semitransparent,
            "Test image should have semi-transparent pixels (use_simple={use_simple})"
        );
    }
}

fn assert_alpha_mode_noop(path: &str, mode: AlphaMode) {
    let file = std::fs::read(path).unwrap();

    for use_simple in [true, false] {
        let as_is = decode::<f32>(
            &file,
            DecodeParams {
                pixel_format: Some(JxlPixelFormat::rgba_f32(1)),
                use_simple_pipeline: use_simple,
                alpha_mode: AlphaMode::AsIs,
                ..Default::default()
            },
        )
        .unwrap();
        let with_mode = decode::<f32>(
            &file,
            DecodeParams {
                pixel_format: Some(JxlPixelFormat::rgba_f32(1)),
                use_simple_pipeline: use_simple,
                alpha_mode: mode,
                ..Default::default()
            },
        )
        .unwrap();
        let (xs, height) = as_is[0][0].size();
        for y in 0..height {
            let as_is_row = as_is[0][0].row(y);
            let with_mode_row = with_mode[0][0].row(y);
            for x in 0..xs {
                assert!(
                    (as_is_row[x] - with_mode_row[x]).abs() < 1e-5,
                    "Mismatch at ({x},{y}): as_is={}, with_mode={} (use_simple={use_simple})",
                    as_is_row[x],
                    with_mode_row[x],
                );
            }
        }
    }
}

fn assert_planar_matches_rgba(path: &str, alpha_mode: AlphaMode) {
    let file = std::fs::read(path).unwrap();
    let planar_format = JxlPixelFormat {
        color_type: JxlColorType::Rgb,
        color_data_format: Some(JxlDataFormat::f32()),
        extra_channel_format: vec![Some(JxlDataFormat::f32())],
    };
    let planar = decode::<f32>(
        &file,
        DecodeParams {
            pixel_format: Some(planar_format),
            alpha_mode,
            ..Default::default()
        },
    )
    .unwrap();
    let rgba = decode::<f32>(
        &file,
        DecodeParams {
            pixel_format: Some(JxlPixelFormat::rgba_f32(1)),
            alpha_mode,
            ..Default::default()
        },
    )
    .unwrap();
    let (rgb_xs, rgb_h) = planar[0][0].size();
    let rgb_w = rgb_xs / 3;
    for y in 0..rgb_h {
        let rgb_row = planar[0][0].row(y);
        let alpha_row = planar[0][1].row(y);
        let rgba_row = rgba[0][0].row(y);
        for x in 0..rgb_w {
            assert!((rgb_row[x * 3] - rgba_row[x * 4]).abs() < 1e-5);
            assert!((rgb_row[x * 3 + 1] - rgba_row[x * 4 + 1]).abs() < 1e-5);
            assert!((rgb_row[x * 3 + 2] - rgba_row[x * 4 + 2]).abs() < 1e-5);
            assert!((alpha_row[x] - rgba_row[x * 4 + 3]).abs() < 1e-5);
        }
    }
}

#[test]
fn test_premultiply_output_straight_alpha() {
    assert_alpha_mode_conversion(
        "resources/test/conformance_test_images/alpha_nonpremultiplied.jxl",
        AlphaMode::Premultiplied,
        |a| a,
        0.01,
    );
}

#[test]
fn test_premultiply_output_grayscale_as_rgba() {
    let file = std::fs::read("resources/test/gray_alpha_lossless.jxl").unwrap();
    let frames = decode::<f32>(
        &file,
        DecodeParams {
            pixel_format: Some(JxlPixelFormat::rgba_f32(1)),
            alpha_mode: AlphaMode::Premultiplied,
            ..Default::default()
        },
    )
    .unwrap();
    let rgba = &frames[0][0];
    let (xs, height) = rgba.size();
    let width = xs / 4;

    for y in 0..height {
        let row = rgba.row(y);
        for x in 0..width {
            assert_eq!(row[x * 4], row[x * 4 + 1]);
            assert_eq!(row[x * 4 + 1], row[x * 4 + 2]);
        }
    }
}

#[test]
fn test_premultiply_output_already_premultiplied() {
    assert_alpha_mode_noop(
        "resources/test/conformance_test_images/alpha_premultiplied.jxl",
        AlphaMode::Premultiplied,
    );
}

#[test]
fn test_unpremultiply_output_associated_alpha() {
    let premul_path = "resources/test/conformance_test_images/alpha_premultiplied.jxl";
    let straight_path = "resources/test/conformance_test_images/alpha_nonpremultiplied.jxl";

    assert_alpha_mode_conversion(
        premul_path,
        AlphaMode::Unpremultiplied,
        |a| 1.0 / a.max(1.0 / ((1u32 << 26) as f32)),
        1e-4,
    );
    assert_alpha_mode_noop(straight_path, AlphaMode::Unpremultiplied);
    assert_planar_matches_rgba(premul_path, AlphaMode::Unpremultiplied);
    assert_planar_matches_rgba(straight_path, AlphaMode::Premultiplied);
}

#[test]
fn test_skip_frame_then_decode_next() {
    let file =
        std::fs::read("resources/test/conformance_test_images/animation_spline.jxl").unwrap();

    let mut decoder = JxlDecoder::new(JxlDecoderOptions::default());
    let mut input = file.as_slice();

    for expected in [
        Event::BasicInfo,
        Event::FrameHeader,
        Event::FrameComplete {
            has_more_frames: true,
        },
        Event::FrameHeader,
    ] {
        assert_eq!(decoder.process(&mut input, None, None).unwrap(), expected);
    }

    let (width, height) = decoder.basic_info().unwrap().size;
    let mut color_buffer = [Image::<f32>::new((width * 3, height)).unwrap()];
    assert!(matches!(
        decoder.process(
            &mut input,
            Some(&mut as_output_buffers(&mut color_buffer)),
            None
        ),
        Ok(Event::FrameComplete { .. })
    ));
}

fn check_output_format_matches_f32<T: ImageDataType>() {
    use crate::api::Endianness;
    use crate::image::DataTypeTag;

    let (data_format, scale, clamp, tolerance) = match T::DATA_TYPE_ID {
        DataTypeTag::U8 => (JxlDataFormat::U8 { bit_depth: 8 }, 1.0 / 255.0, true, 0.004),
        DataTypeTag::U16 => (
            JxlDataFormat::U16 {
                endianness: Endianness::native(),
                bit_depth: 16,
            },
            1.0 / 65535.0,
            true,
            0.0001,
        ),
        DataTypeTag::F16 => (
            JxlDataFormat::F16 {
                endianness: Endianness::native(),
            },
            1.0,
            false,
            0.002,
        ),
        _ => unreachable!(),
    };

    let file = std::fs::read("resources/test/conformance_test_images/bicycles.jxl").unwrap();

    for color_type in [JxlColorType::Rgb, JxlColorType::Bgra] {
        let f32_format = JxlPixelFormat {
            color_type,
            color_data_format: Some(JxlDataFormat::f32()),
            extra_channel_format: vec![],
        };
        let format = JxlPixelFormat {
            color_type,
            color_data_format: Some(data_format),
            extra_channel_format: vec![],
        };

        let f32_frames = decode::<f32>(
            &file,
            DecodeParams {
                pixel_format: Some(f32_format),
                ..Default::default()
            },
        )
        .unwrap();
        let f32_buffer = &f32_frames[0][0];
        let (xs, height) = f32_buffer.size();

        for use_simple in [true, false] {
            let frames = decode::<T>(
                &file,
                DecodeParams {
                    pixel_format: Some(format.clone()),
                    use_simple_pipeline: use_simple,
                    ..Default::default()
                },
            )
            .unwrap();
            let buffer = &frames[0][0];

            for y in 0..height {
                let f32_row = f32_buffer.row(y);
                let row = buffer.row(y);
                for x in 0..xs {
                    let f32_val = if clamp {
                        f32_row[x].clamp(0.0, 1.0)
                    } else {
                        f32_row[x]
                    };
                    let val = (row[x].to_f64() * scale) as f32;
                    let error = (f32_val - val).abs();
                    assert!(
                        error < tolerance,
                        "{color_type:?} {:?} mismatch at ({x},{y}): f32={f32_val}, got={:?} (scaled={val}), error={error} (use_simple={use_simple})",
                        T::DATA_TYPE_ID,
                        row[x],
                    );
                }
            }
        }
    }
}

/// Test that u8 output matches f32 output within quantization tolerance.
#[test]
fn test_output_format_u8_matches_f32() {
    check_output_format_matches_f32::<u8>();
}

/// Test that u16 output matches f32 output within quantization tolerance.
#[test]
fn test_output_format_u16_matches_f32() {
    check_output_format_matches_f32::<u16>();
}

/// Test that f16 output matches f32 output within f16 precision tolerance.
#[test]
fn test_output_format_f16_matches_f32() {
    check_output_format_matches_f32::<crate::util::f16>();
}

/// CMYK interleaved output matches the RGB color channels for C, M and Y, and
/// the Black extra channel plane for K.
#[test]
fn test_cmyk_pixel_format() {
    let file = std::fs::read("resources/test/conformance_test_images/cmyk_layers.jxl").unwrap();

    // cmyk_layers.jxl has two extra channels: Black (index 0) and Alpha
    // (index 1).
    let cmyk_format = JxlPixelFormat::cmyk8(2);
    let reference_format = JxlPixelFormat {
        color_type: JxlColorType::Rgb,
        color_data_format: Some(JxlDataFormat::U8 { bit_depth: 8 }),
        extra_channel_format: vec![Some(JxlDataFormat::U8 { bit_depth: 8 }), None],
    };

    for use_simple in [true, false] {
        let cmyk_frames = decode::<u8>(
            &file,
            DecodeParams {
                pixel_format: Some(cmyk_format.clone()),
                use_simple_pipeline: use_simple,
                ..Default::default()
            },
        )
        .unwrap();
        let reference_frames = decode::<u8>(
            &file,
            DecodeParams {
                pixel_format: Some(reference_format.clone()),
                use_simple_pipeline: use_simple,
                ..Default::default()
            },
        )
        .unwrap();
        let cmyk = &cmyk_frames[0][0];
        let rgb = &reference_frames[0][0];
        let black = &reference_frames[0][1];
        let (width, height) = black.size();

        for y in 0..height {
            let cmyk_row = cmyk.row(y);
            let rgb_row = rgb.row(y);
            let black_row = black.row(y);
            for x in 0..width {
                for c in 0..3 {
                    assert_eq!(
                        cmyk_row[x * 4 + c],
                        rgb_row[x * 3 + c],
                        "CMY mismatch at ({x},{y}) channel {c} (use_simple={use_simple})"
                    );
                }
                assert_eq!(
                    cmyk_row[x * 4 + 3],
                    black_row[x],
                    "K mismatch at ({x},{y}) (use_simple={use_simple})"
                );
            }
        }
    }
}

/// Requesting CMYK output for a non-CMYK image fails.
#[test]
fn test_cmyk_pixel_format_requires_cmyk_image() {
    let file = std::fs::read("resources/test/basic.jxl").unwrap();
    let result = decode::<f32>(
        &file,
        DecodeParams {
            pixel_format: Some(JxlPixelFormat::cmyk8(0)),
            ..Default::default()
        },
    );
    assert!(
        matches!(result, Err(Error::NotCmyk)),
        "expected NotCmyk, got {result:?}"
    );
}

/// Regression test for ClusterFuzz issue 5342436251336704
#[test]
fn test_fuzzer_smallbuffer_overflow() {
    use std::panic;

    let data = include_bytes!("../../tests/testdata/fuzzer_smallbuffer_overflow.jxl");

    let result = panic::catch_unwind(|| {
        let _ = decode::<f32>(
            data,
            DecodeParams {
                chunk_size: 1024,
                ..Default::default()
            },
        );
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
        decode::<f32>(
            &data,
            DecodeParams {
                chunk_size,
                do_flush: true,
                ..Default::default()
            },
        )
        .unwrap();
    }
}

/// Regression test for https://issues.chromium.org/issues/562761172: flushing
/// a truncated image used to panic when a smooth-squeeze upsample step read a
/// tile whose channel had not been decoded yet.
#[test]
fn flush_truncated_squeeze_missing_tiles() {
    let data = include_bytes!("../../tests/testdata/truncated_squeeze_flush_missing_tiles.jxl");
    for chunk_size in [64, 256, usize::MAX] {
        decode::<f32>(
            data,
            DecodeParams {
                chunk_size,
                do_flush: true,
                allow_partial: true,
                ..Default::default()
            },
        )
        .unwrap();
    }
}

#[test]
fn flush_truncated_squeeze_missing_avg() {
    let data = include_bytes!("../../tests/testdata/truncated_squeeze_missing_avg.jxl");
    for chunk_size in [64, 256, usize::MAX] {
        decode::<f32>(
            data,
            DecodeParams {
                chunk_size,
                do_flush: true,
                allow_partial: true,
                ..Default::default()
            },
        )
        .unwrap();
    }
}

/// Regression test: flushing a truncated image with tiled channels whose tile
/// dimension is <= 1 used to panic during smooth-squeeze upsampling.
#[test]
fn flush_truncated_squeeze_small_tiles() {
    let data = include_bytes!("../../tests/testdata/truncated_squeeze_flush_small_tiles.jxl");
    for chunk_size in [64, 256, usize::MAX] {
        decode::<f32>(
            data,
            DecodeParams {
                chunk_size,
                do_flush: true,
                allow_partial: true,
                ..Default::default()
            },
        )
        .unwrap();
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
    let scanned_frames = scan_frames(data, usize::MAX);

    let sequential_frames = decode(data, Default::default()).unwrap();

    arbtest::arbtest(|u| {
        let initial_offset =
            u.int_in_range(scanned_frames[0].file_offset..=data.len() as u64)? as usize;

        let options = JxlDecoderOptions::default();
        let mut decoder = JxlDecoder::new(options);
        let mut input = &data[..initial_offset];

        while !matches!(
            decoder.process(&mut input, None, None).unwrap(),
            Event::NeedMoreInput { .. } | Event::Complete
        ) {}

        let num_seeks = u.int_in_range(1..=3)?;
        for _ in 0..num_seeks {
            let target_visible_index =
                u.int_in_range(0..=scanned_frames.len() as u64 - 1)? as usize;
            let seek_target = scanned_frames[target_visible_index].seek_target;

            let expected = &sequential_frames[target_visible_index];

            let offset = decoder.start_new_frame(seek_target).unwrap();
            let mut input = &data[offset as usize..];

            assert_eq!(
                decoder.process(&mut input, None, None).unwrap(),
                Event::FrameHeader
            );

            let mut seek_decoded: Vec<Image<f32>> = expected
                .iter()
                .map(|img| Image::new(img.size()).unwrap())
                .collect();
            let mut buffers = as_output_buffers(&mut seek_decoded);

            assert!(matches!(
                decoder.process(&mut input, Some(&mut buffers), None),
                Ok(Event::FrameComplete { .. })
            ));

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

            while !matches!(
                decoder.process(&mut extra_input, None, None).unwrap(),
                Event::NeedMoreInput { .. } | Event::Complete
            ) {}
        }
        Ok(())
    });
}

// Seeks to every frame of an animation with noise on every frame; displayed frames that are saved
// as references (some of them used by later frames' blending), displayed frames that are not,
// cropped frames, patches, and keyframes after the start. Each frame's noise must be seeded as in
// a sequential decode.
#[test]
fn test_seek_every_frame_noise_references() {
    let data = std::fs::read("resources/test/animation_seek_noise_references.jxl").unwrap();
    let scanned_frames = scan_frames(&data, usize::MAX);
    let sequential_frames = decode(&data, Default::default()).unwrap();

    let mut decoder = JxlDecoder::new(JxlDecoderOptions::default());
    let mut input = &data[..];
    while !matches!(
        decoder.process(&mut input, None, None).unwrap(),
        Event::NeedMoreInput { .. } | Event::Complete
    ) {}

    for target in (0..scanned_frames.len()).rev() {
        let seek_target = scanned_frames[target].seek_target;
        let expected = &sequential_frames[target];

        let offset = decoder.start_new_frame(seek_target).unwrap();
        let mut input = &data[offset as usize..];
        assert_eq!(
            decoder.process(&mut input, None, None).unwrap(),
            Event::FrameHeader
        );

        let mut seek_decoded: Vec<Image<f32>> = expected
            .iter()
            .map(|img| Image::new(img.size()).unwrap())
            .collect();
        let mut buffers = as_output_buffers(&mut seek_decoded);
        assert!(matches!(
            decoder.process(&mut input, Some(&mut buffers), None),
            Ok(Event::FrameComplete { .. })
        ));

        compare_frames(
            Path::new("seek_every_frame"),
            target,
            expected,
            &seek_decoded,
        );
    }
}

/// Decodes the next frame and compares it with `expected`.
fn decode_next_and_compare(
    decoder: &mut JxlDecoder,
    input: &mut &[u8],
    expected: &[Image<f32>],
    name: &str,
    index: usize,
) {
    let event = decoder.process(input, None, None).unwrap();
    assert_eq!(event, Event::FrameHeader, "{name}: frame {index}");
    let mut decoded: Vec<Image<f32>> = expected
        .iter()
        .map(|img| Image::new(img.size()).unwrap())
        .collect();
    let mut buffers = as_output_buffers(&mut decoded);
    assert!(matches!(
        decoder.process(input, Some(&mut buffers), None),
        Ok(Event::FrameComplete { .. })
    ));
    compare_frames(Path::new(name), index, expected, &decoded);
}

fn decode_whole_file(data: &[u8]) -> JxlDecoder {
    let mut decoder = JxlDecoder::new(JxlDecoderOptions::default());
    let mut input = data;
    while decoder.process(&mut input, None, None).unwrap() != Event::Complete {}
    decoder
}

/// Seeks to `target` with `start_new_frame` and returns the input to continue from.
fn seek<'a>(decoder: &mut JxlDecoder, data: &'a [u8], target: &VisibleFrameInfo) -> &'a [u8] {
    let offset = decoder.start_new_frame(target.seek_target).unwrap();
    &data[offset as usize..]
}

fn new_decoder_at_basic_info(data: &[u8]) -> JxlDecoder {
    let mut decoder = JxlDecoder::new(JxlDecoderOptions::default());
    let mut input = data;
    while decoder.process(&mut input, None, None).unwrap() != Event::BasicInfo {}
    decoder
}

// Seeks to frames in different orders, each followed by some frames in order: all must match a
// sequential decode. The decoder keeps its state where it can: seeking back after three frames,
// seeking forward over a frame, and (as a reference) seeking with a new decoder. The files have
// noise, saved and unsaved displayed frames, blending, cropped frames, patches from sprite sheets,
// and a frame that reads no slot followed by frames that read a sheet saved before it.
#[test]
fn test_seek_matches_sequential() {
    for path in [
        "animation_seek_noise_references.jxl",
        "animation_seek_sprite_sheets.jxl",
        "animation_seek_keyframe_then_patches.jxl",
        "animation_seek_empty.jxl",
        "animation_seek_interrupted.jxl",
        "conformance_test_images/animation_icos4d.jxl",
        "conformance_test_images/animation_newtons_cradle.jxl",
        "conformance_test_images/animation_spline.jxl",
        "cropped_traffic_light.jxl",
    ] {
        let data = std::fs::read(format!("resources/test/{path}")).unwrap();
        let frames = scan_frames(&data, usize::MAX);
        let sequential = decode(&data, Default::default()).unwrap();
        let backward = (0..frames.len()).rev().map(|target| (target, 3));
        let forward = (0..frames.len()).step_by(2).map(|target| (target, 1));
        let mut decoder = decode_whole_file(&data);
        for (target, count) in backward.chain(forward) {
            let mut fresh = new_decoder_at_basic_info(&data);
            for (decoder, how) in [
                (&mut decoder, "seek"),
                (&mut fresh, "seek with a new decoder"),
            ] {
                let mut input = seek(decoder, &data, &frames[target]);
                for (i, expected) in sequential.iter().enumerate().skip(target).take(count) {
                    let name = format!("{path}: {how}");
                    decode_next_and_compare(decoder, &mut input, expected, &name, i);
                }
            }
        }
    }
}

// A seek that is interrupted after decoding a frame that it does not get right (X, which adds A,
// but the seek to the last frame starts after A), then a seek to the frame that shows X: it must
// not take X from the interrupted seek.
#[test]
fn test_seek_after_interrupted_seek() {
    let data = std::fs::read("resources/test/animation_seek_interrupted.jxl").unwrap();
    let frames = scan_frames(&data, usize::MAX);
    let sequential = decode(&data, Default::default()).unwrap();
    let mut decoder = new_decoder_at_basic_info(&data);
    let offset = decoder.start_new_frame(frames[1].seek_target).unwrap();
    let mut input = &data[offset as usize..frames[0].file_offset as usize];
    while !matches!(
        decoder.process(&mut input, None, None).unwrap(),
        Event::NeedMoreInput { .. }
    ) {}
    let mut input = seek(&mut decoder, &data, &frames[0]);
    decode_next_and_compare(&mut decoder, &mut input, &sequential[0], "interrupted", 0);
}

// Sprite sheet A in slot 1, frames 0-5 patch from it, sheet B replaces it, frames 6-11 patch from
// that (file order: A, 0-5, B, 6-11). A seek decodes only the target and the sheet it needs, and
// not even the sheet when the decoder still has it.
#[test]
fn test_seek_decodes_only_needed_frames() {
    let data = std::fs::read("resources/test/animation_seek_sprite_sheets.jxl").unwrap();
    let frames = scan_frames(&data, usize::MAX);
    let frames_decoded_by_seek = |decoder: &mut JxlDecoder, target: usize| {
        let before = decoder.frames_decoded();
        let mut input = seek(decoder, &data, &frames[target]);
        while !matches!(
            decoder.process(&mut input, None, None).unwrap(),
            Event::FrameComplete { .. }
        ) {}
        decoder.frames_decoded() - before
    };
    let mut decoder = decode_whole_file(&data);
    assert_eq!(decoder.frames_decoded(), 14);
    // The animation loops, so the decoder keeps its state after the last frame, with sheet B. It
    // needs sheet A again for 3; 5 continues after 3, and 6 continues through sheet B.
    for (target, cost) in [(9, 1), (7, 1), (3, 2), (1, 1), (5, 1), (6, 2)] {
        assert_eq!(
            frames_decoded_by_seek(&mut decoder, target),
            cost,
            "frame {target}"
        );
    }
    let mut fresh = new_decoder_at_basic_info(&data);
    assert_eq!(frames_decoded_by_seek(&mut fresh, 9), 2);
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

    let scanned_frames = scan_frames(&codestream, usize::MAX);
    assert!(scanned_frames.len() > 1, "need multiple frames");

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
fn test_start_new_frame_animation_newtons_cradle() {
    let data = std::fs::read("resources/test/conformance_test_images/animation_newtons_cradle.jxl")
        .unwrap();
    assert_start_new_frame_matches_sequential(&data);
}

#[test]
fn test_start_new_frame_animation_spline() {
    let data =
        std::fs::read("resources/test/conformance_test_images/animation_spline.jxl").unwrap();
    assert_start_new_frame_matches_sequential(&data);
}

#[test]
fn test_scan_still_image() {
    let data = std::fs::read("resources/test/green_queen_vardct_e3.jxl").unwrap();
    let frames = scan_frames(&data, usize::MAX);

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
    let frames = scan_frames(&data, usize::MAX);

    assert!(frames.len() > 1, "expected multiple frames");

    for (i, frame) in frames.iter().enumerate() {
        assert_eq!(frame.index, i);
    }

    assert!(frames.last().unwrap().is_last);
    assert!(frames[0].is_keyframe);
    assert_eq!(
        new_decoder_at_basic_info(&data)
            .start_new_frame(frames[0].seek_target)
            .unwrap(),
        frames[0].file_offset as u64
    );
}

#[test]
fn test_scan_animation_offsets_increase() {
    let data =
        std::fs::read("resources/test/conformance_test_images/animation_icos4d_5.jxl").unwrap();
    let frames = scan_frames(&data, usize::MAX);

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

    let frames = scan_frames(&data, 128);
    assert!(frames.len() > 1);
    assert!(frames.last().unwrap().is_last);
}

#[test]
fn test_scan_keyframe_detection_still() {
    let data = std::fs::read("resources/test/green_queen_vardct_e3.jxl").unwrap();
    let frames = scan_frames(&data, usize::MAX);

    assert_eq!(frames.len(), 1);
    let f = &frames[0];
    assert!(f.is_keyframe);
    assert_eq!(
        new_decoder_at_basic_info(&data)
            .start_new_frame(f.seek_target)
            .unwrap(),
        f.file_offset as u64
    );
}

#[test]
fn test_scan_decode_start_file_offset_consistency() {
    let data =
        std::fs::read("resources/test/conformance_test_images/animation_icos4d_5.jxl").unwrap();

    let frames = scan_frames(&data, usize::MAX);
    let mut decoder = new_decoder_at_basic_info(&data);

    for frame in &frames {
        let decode_start_offset = decoder.start_new_frame(frame.seek_target).unwrap();
        assert!(
            decode_start_offset <= frame.file_offset,
            "frame {}: decode_start_file_offset {} > file_offset {}",
            frame.index,
            decode_start_offset,
            frame.file_offset,
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
    let frames = scan_frames(&data, usize::MAX);

    assert!(frames.len() <= 1);
}

#[test]
fn test_scan_patches_not_keyframe() {
    let data = std::fs::read("resources/test/grayscale_patches_var_dct.jxl");
    if data.is_err() {
        return;
    }
    let data = data.unwrap();
    let frames = scan_frames(&data, usize::MAX);

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

    let mut decoder = JxlDecoder::new(Default::default());
    let mut input = data;

    if let Ok(Event::BasicInfo) = decoder.process(&mut input, None, None)
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
    let mut decoder = JxlDecoder::new(opts);

    let mut input = data;
    while !matches!(
        decoder.process(&mut input, None, None).unwrap(),
        Event::NeedMoreInput { .. } | Event::Complete
    ) {}
}

/// Small regression test for issue #728: squeeze transform boundary bug.
#[test]
fn test_squeeze_boundary_minimal() {
    let frames = decode::<f32>(
        &std::fs::read("resources/test/issue728_minimal.jxl").unwrap(),
        Default::default(),
    )
    .unwrap();
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
    let frames = decode::<f32>(
        &std::fs::read("resources/test/strategic_solid_blue.jxl").unwrap(),
        Default::default(),
    )
    .unwrap();
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

/// Regression test: a grayscale, non-XYB VarDCT frame has no stage consuming colour
/// channels 1 and 2, so those channels end up with no type in the pipeline. VarDCT still
/// decodes all three channels, and asking the pipeline for their scratch buffers used to
/// panic.
#[test]
fn test_fuzzer_vardct_grayscale_unused_channel() {
    let data = include_bytes!("../../tests/testdata/vardct_grayscale_unused_channel.jxl");
    let frames = decode(data, Default::default()).unwrap();
    let simple_frames = decode(
        data,
        DecodeParams {
            use_simple_pipeline: true,
            ..Default::default()
        },
    )
    .unwrap();
    assert_eq!(frames.len(), 1);
    assert_eq!(frames[0].len(), 1);
    assert_eq!(frames[0][0].size(), (1, 1));
    compare_frames(
        Path::new("vardct_grayscale_unused_channel.jxl"),
        0,
        &frames[0],
        &simple_frames[0],
    );
    // Streaming input with flushing exercises the low-memory pipeline's partial renders.
    decode::<f32>(
        data,
        DecodeParams {
            chunk_size: 1,
            do_flush: true,
            ..Default::default()
        },
    )
    .unwrap();
}

/// Regression test: a context map with cluster index 255. This shouldn't panic.
#[test]
fn test_fuzzer_context_map_num_histograms_overflow() {
    let data = include_bytes!("../../tests/testdata/context_map_num_histograms_overflow.jxl");
    let _ = decode::<f32>(data, Default::default());
    let _ = decode::<f32>(
        data,
        DecodeParams {
            chunk_size: 1024,
            do_flush: true,
            ..Default::default()
        },
    );
}

/// Regression test: two nested palette transforms, where the inner one has no colors and no
/// deltas and so produces a 0x1 palette channel. `Image` allocates such a channel as 0x0, and
/// applying the outer palette on top of it used to compare the declared 0x1 size against the
/// allocated 0x0 one and panic. The file is malformed further on, so decoding it must fail --
/// but with an error rather than a panic.
#[test]
fn test_fuzzer_modular_palette_empty_meta_channel() {
    let data = include_bytes!("../../tests/testdata/modular_palette_empty_meta_channel.jxl");
    assert!(decode::<f32>(data, Default::default()).is_err());
}

/// `dim_shift = 3` with `ec_upsampling = 2` is a cumulative extra channel upsampling of 16, which is
/// rejected (at most 8) also when the frame itself is not upsampled.
#[test]
fn test_ec_upsampling_too_large_without_upsampling() {
    let data = include_bytes!("../../tests/testdata/ec_upsampling16_no_upsampling.jxl");
    let result = decode::<f32>(data, Default::default());
    assert!(
        matches!(result, Err(Error::InvalidEcUpsampling(1, 3, 16))),
        "expected an extra channel upsampling error, got {:?}",
        result.map(|_| "a decoded image")
    );
}

/// Regression test: a frame with patches that declares `upsampling = 4` and `ec_upsampling = [4]`
/// for an extra channel with `dim_shift = 1`. The declared amounts match, so the guard against
/// mixing patches with differing upsampling used to pass, and `postprocess` then shifted the
/// extra channel to an effective 8x. The extra channel is upsampled before the patches stage
/// while the color channels are upsampled after it, so the patches stage (which uses both)
/// saw channels at two different resolutions and tripped an assertion in the low-memory
/// pipeline.
#[test]
fn test_fuzzer_patches_ec_upsampling_dim_shift() {
    let data = include_bytes!("../../tests/testdata/patches_ec_upsampling_dim_shift.jxl");
    let result = decode::<f32>(data, Default::default());
    assert!(
        matches!(result, Err(Error::PatchesUnsupportedMixedUpsampling(..))),
        "expected a mixed upsampling error, got {:?}",
        result.map(|_| "a decoded image")
    );
}

/// Regression test: a Modular stream that disables LZ77, but whose pixel histogram codes the
/// constant symbol 1 with a split-exponent-zero uint config -- the shape `Histograms::is_rle()`
/// used to accept, since without LZ77 it inspected cluster 0 instead of the (nonexistent)
/// distance cluster. Together with a single Gradient leaf and prefix codes, that made
/// `decode_modular_subbitstream()` take the RLE fast path, where `decode_fast_lossless()`
/// unwraps the LZ77 parameters -- all `None` here -- and panicked. The stream is valid and
/// decodes on the normal path, so it must decode rather than merely not panic.
#[test]
fn test_fuzzer_modular_rle_fast_path_without_lz77() {
    let data = include_bytes!("../../tests/testdata/modular_rle_fast_path_without_lz77.jxl");
    let frames = decode::<f32>(data, Default::default()).unwrap();
    assert_eq!(frames.len(), 1);
    // A single 8x8 frame, with its three colour channels interleaved.
    assert_eq!(frames[0][0].size(), (3 * 8, 8));
    // Streaming input with flushing exercises the low-memory pipeline as well.
    decode::<f32>(
        data,
        DecodeParams {
            chunk_size: 1,
            do_flush: true,
            ..Default::default()
        },
    )
    .unwrap();
}

/// The other direction: a stream that is genuinely RLE-coded (LZ77 enabled, every copy at
/// distance 1) still takes the fast path. It codes the same image as
/// `modular_rle_fast_path_without_lz77.jxl`, so the two must decode to the same pixels.
#[test]
fn test_modular_rle_fast_path() {
    let data = include_bytes!("../../tests/testdata/modular_rle_fast_path.jxl");
    let frames = decode(data, Default::default()).unwrap();
    let no_lz77 = include_bytes!("../../tests/testdata/modular_rle_fast_path_without_lz77.jxl");
    let no_lz77_frames = decode(no_lz77, Default::default()).unwrap();
    compare_frames(
        Path::new("modular_rle_fast_path.jxl"),
        0,
        &frames[0],
        &no_lz77_frames[0],
    );
}

/// Decoding with `adjust_orientation: false` must output pixels in
/// codestream order and report the codestream size in the basic info;
/// re-applying the orientation must reproduce the default (oriented) output.
#[test]
fn test_adjust_orientation_disabled() {
    use crate::headers::Orientation;

    let files = [
        ("orientation1_identity.jxl", Orientation::Identity),
        (
            "orientation2_flip_horizontal.jxl",
            Orientation::FlipHorizontal,
        ),
        ("orientation3_rotate_180.jxl", Orientation::Rotate180),
        ("orientation4_flip_vertical.jxl", Orientation::FlipVertical),
        ("orientation5_transpose.jxl", Orientation::Transpose),
        ("orientation6_rotate_90_cw.jxl", Orientation::Rotate90Cw),
        (
            "orientation7_anti_transpose.jxl",
            Orientation::AntiTranspose,
        ),
        ("orientation8_rotate_90_ccw.jxl", Orientation::Rotate90Ccw),
    ];
    const NUM_SAMPLES: usize = 4;

    for (name, orientation) in files {
        let file = std::fs::read(format!("resources/test/{name}")).unwrap();
        for use_simple in [true, false] {
            let oriented_frames = decode::<f32>(
                &file,
                DecodeParams {
                    pixel_format: Some(JxlPixelFormat::rgba_f32(0)),
                    use_simple_pipeline: use_simple,
                    ..Default::default()
                },
            )
            .unwrap();
            let raw_frames = decode::<f32>(
                &file,
                DecodeParams {
                    pixel_format: Some(JxlPixelFormat::rgba_f32(0)),
                    use_simple_pipeline: use_simple,
                    adjust_orientation: false,
                    ..Default::default()
                },
            )
            .unwrap();

            let oriented = &oriented_frames[0][0];
            let raw = &raw_frames[0][0];
            let (ow, oh) = (oriented.size().0 / NUM_SAMPLES, oriented.size().1);
            let (rw, rh) = (raw.size().0 / NUM_SAMPLES, raw.size().1);
            assert_eq!((ow, oh), orientation.map_size((rw, rh)), "{name}");

            for y in 0..rh {
                for x in 0..rw {
                    let (dx, dy) = orientation.display_pixel((x, y), (rw, rh));
                    for s in 0..NUM_SAMPLES {
                        assert_eq!(
                            raw.row(y)[x * NUM_SAMPLES + s],
                            oriented.row(dy)[dx * NUM_SAMPLES + s],
                            "{name} mismatch at ({x},{y}) sample {s} (use_simple={use_simple})"
                        );
                    }
                }
            }
        }
    }
}

#[test]
fn basic_info_not_visible_before_embedded_profile() {
    let data = std::fs::read("resources/test/conformance_test_images/cmyk_layers.jxl").unwrap();
    let mut decoder = JxlDecoder::new(JxlDecoderOptions::default());

    for chunk in data.chunks(64) {
        let mut input = chunk;
        let _ = decoder.process(&mut input, None, None);

        if decoder.embedded_color_profile().is_none() {
            assert!(decoder.basic_info().is_none());
        }

        if decoder.basic_info().is_some() {
            assert!(decoder.embedded_color_profile().is_some());
            return;
        }
    }

    panic!("failed to reach image-info state while parsing cmyk_layers.jxl");
}

#[test]
fn incomplete_ooo_jxlp() {
    let data = include_bytes!("../../tests/testdata/incomplete_ooo_jxlp.jxl");

    let mut decoder = JxlDecoder::new(JxlDecoderOptions::default());
    let mut input = data.as_slice();
    let result = decoder.process(&mut input, None, None);
    assert!(
        matches!(result, Err(Error::UnexpectedCodestreamBoxEnd)),
        "{result:?}"
    );
}

fn process_to_complete(decoder: &mut JxlDecoder, buf: &mut &[u8]) {
    decoder.close_input();
    loop {
        match decoder.process(buf, None, None).unwrap() {
            Event::Complete => break,
            Event::NeedMoreInput { .. } => panic!("unexpected NeedMoreInput"),
            _ => {}
        }
    }
}

#[test]
fn aux_box_finite() {
    let data = [
        (&include_bytes!("../../tests/testdata/exif.jxl")[..], 170),
        (
            &include_bytes!("../../tests/testdata/exif_brob.jxl")[..],
            120,
        ),
        (
            &include_bytes!("../../tests/testdata/exif_trailing_finite.jxl")[..],
            170,
        ),
        (
            &include_bytes!("../../tests/testdata/exif_brob_trailing_finite.jxl")[..],
            120,
        ),
    ];

    for (mut buf, expected_size) in data {
        let options = JxlDecoderOptions {
            request_aux_boxes: vec![JxlAuxBoxType::EXIF],
            scan_frames_only: true,
            ..Default::default()
        };
        let mut decoder = JxlDecoder::new(options);
        process_to_complete(&mut decoder, &mut buf);

        let exif = &decoder.aux_boxes(JxlAuxBoxType::EXIF)[0];
        assert_eq!(exif.raw_data().len(), expected_size);
        assert!(decoder.trailing_box().is_none());
    }
}

#[test]
fn aux_box_incomplete_trailing_box() {
    let data = include_bytes!("../../tests/testdata/exif_trailing_finite.jxl");
    // Supply the codestream box and only the first byte of the Exif box header.
    let mut input = &data[..53];
    let options = JxlDecoderOptions {
        scan_frames_only: true,
        request_aux_boxes: vec![JxlAuxBoxType::EXIF],
        ..Default::default()
    };
    let mut decoder = JxlDecoder::new(options);

    let expected_events = [
        Event::BasicInfo,
        Event::FrameHeader,
        Event::FrameComplete {
            has_more_frames: false,
        },
    ];
    for expected in expected_events {
        assert_eq!(decoder.process(&mut input, None, None).unwrap(), expected);
    }

    assert!(matches!(
        decoder.process(&mut input, None, None).unwrap(),
        Event::NeedMoreInput { .. }
    ));
}

#[test]
fn aux_box_empty_input_at_box_boundary() {
    let data = include_bytes!("../../tests/testdata/exif_trailing_finite.jxl");
    // Stop at the box boundary after the codestream.
    let mut input = &data[..52];
    let options = JxlDecoderOptions {
        scan_frames_only: true,
        request_aux_boxes: vec![JxlAuxBoxType::EXIF],
        ..Default::default()
    };
    let mut decoder = JxlDecoder::new(options);

    let expected_events = [
        Event::BasicInfo,
        Event::FrameHeader,
        Event::FrameComplete {
            has_more_frames: false,
        },
    ];
    for expected in expected_events {
        assert_eq!(decoder.process(&mut input, None, None).unwrap(), expected);
    }

    // No more bytes are available yet, but the stream has not ended.
    assert!(input.is_empty());
    assert_eq!(
        decoder.process(&mut input, None, None).unwrap(),
        Event::NeedMoreInput { size_hint: 8 }
    );

    let mut input = &data[52..];
    decoder.close_input();
    assert_eq!(
        decoder.process(&mut input, None, None).unwrap(),
        Event::Complete
    );
    assert_eq!(decoder.aux_boxes(JxlAuxBoxType::EXIF).len(), 1);
}

#[test]
fn aux_box_trailing_infinite() {
    let data = [
        (
            &include_bytes!("../../tests/testdata/exif_trailing_infinite.jxl")[..],
            170,
        ),
        (
            &include_bytes!("../../tests/testdata/exif_brob_trailing_infinite.jxl")[..],
            120,
        ),
    ];

    for (mut buf, expected_size) in data {
        let options = JxlDecoderOptions {
            request_aux_boxes: vec![JxlAuxBoxType::EXIF],
            scan_frames_only: true,
            ..Default::default()
        };
        let mut decoder = JxlDecoder::new(options);
        process_to_complete(&mut decoder, &mut buf);

        let trailing = decoder.trailing_box().unwrap();
        assert_eq!(trailing.box_type(), JxlAuxBoxType::EXIF);
        assert_eq!(trailing.raw_data().len() + buf.len(), expected_size);
    }
}

#[test]
fn aux_box_seek() {
    let data = include_bytes!("../../tests/testdata/multiple_aux.jxl");

    let ty_foo = JxlAuxBoxType(*b"foo ");
    let ty_bar = JxlAuxBoxType(*b"bar ");

    let options = JxlDecoderOptions {
        request_aux_boxes: vec![ty_bar],
        scan_frames_only: true,
        ..Default::default()
    };
    let mut decoder = JxlDecoder::new(options);

    let mut buf = &data[..];
    process_to_complete(&mut decoder, &mut buf);
    assert!(decoder.aux_boxes(ty_foo).is_empty());
    assert_eq!(decoder.aux_boxes(ty_bar).len(), 2);

    let seek_target = decoder.scanned_frames()[0].seek_target;
    let offset = decoder.start_new_frame(seek_target).unwrap();

    let mut buf = &data[(offset as usize)..];
    process_to_complete(&mut decoder, &mut buf);
    assert!(decoder.aux_boxes(ty_foo).is_empty());
    assert_eq!(decoder.aux_boxes(ty_bar).len(), 2);
}

#[test]
fn test_fuzzer_icc_lz77_restore_empty_rewind() {
    let data = include_bytes!("../../tests/testdata/icc_lz77_restore_empty_rewind.jxl");
    for chunk_size in 1..=data.len() {
        let _ = decode::<f32>(
            data,
            DecodeParams {
                chunk_size,
                do_flush: true,
                ..Default::default()
            },
        );
    }
}

#[test]
fn test_decoder_reset() {
    let paths = [
        "resources/test/basic.jxl",
        "resources/test/with_preview.jxl",
        "resources/test/3x3_srgb_lossless.jxl",
        "resources/test/has_permutation_with_container.jxl",
        "resources/test/lossy_with_icc.jxl",
        "resources/test/conformance_test_images/animation_spline.jxl",
        "resources/test/conformance_test_images/bicycles.jxl",
    ];
    let files: Vec<_> = paths
        .into_iter()
        .map(|path| {
            let data = std::fs::read(path).unwrap();
            let expected = decode::<f32>(&data, Default::default()).unwrap();
            (path, data, expected)
        })
        .collect();

    arbtest::arbtest(|u| {
        let mut decoder = JxlDecoder::new(JxlDecoderOptions::default());

        let num_aborts = u.int_in_range(0..=3)?;
        for _ in 0..num_aborts {
            let (_, data, _) = u.choose(&files)?;
            let prefix_len = u.int_in_range(0..=data.len())?;
            let chunk_size = u.int_in_range(1..=data.len().max(1))?;
            let do_flush: bool = u.arbitrary()?;

            decoder.reset();
            let mut remaining = &data[..prefix_len];
            let mut size = (0, 0);
            let mut bufs: Option<Vec<Image<f32>>> = None;

            loop {
                let take = remaining.len().min(chunk_size);
                let mut chunk = &remaining[..take];
                let prev_len = chunk.len();
                let mut out_bufs = bufs.as_mut().map(|b| as_output_buffers(b));
                let event = decoder.process(&mut chunk, out_bufs.as_deref_mut(), None);
                let consumed = prev_len - chunk.len();
                remaining = &remaining[consumed..];

                match event {
                    Ok(Event::BasicInfo) => {
                        size = decoder.basic_info().unwrap().size;
                    }
                    Ok(Event::FrameHeader) => {
                        bufs = Some(vec![Image::<f32>::new((size.0 * 3, size.1)).unwrap()]);
                    }
                    Ok(Event::FrameComplete { .. }) => {
                        bufs = None;
                    }
                    Ok(Event::NeedMoreInput { .. }) => {
                        if do_flush && let Some(ref mut b) = bufs {
                            let _ = decoder.flush_pixels(&mut as_output_buffers(b), None);
                        }
                        if remaining.is_empty() {
                            break;
                        }
                    }
                    Ok(Event::Complete) | Err(_) => break,
                }
            }
        }

        let num_full = u.int_in_range(1..=2)?;
        for _ in 0..num_full {
            let (path, data, expected) = u.choose(&files)?;
            let chunk_size = u.int_in_range(1..=data.len())?;

            decoder.reset();
            let mut remaining = data.as_slice();
            let mut current = &remaining[..0];
            let mut size = (0, 0);
            let mut bufs: Option<Vec<Image<f32>>> = None;
            let mut frames = Vec::new();

            loop {
                let mut out_bufs = bufs.as_mut().map(|b| as_output_buffers(b));
                match decoder
                    .process(&mut current, out_bufs.as_deref_mut(), None)
                    .unwrap()
                {
                    Event::BasicInfo => {
                        size = decoder.basic_info().unwrap().size;
                    }
                    Event::FrameHeader => {
                        bufs = Some(vec![Image::<f32>::new((size.0 * 3, size.1)).unwrap()]);
                    }
                    Event::FrameComplete { .. } => {
                        frames.push(bufs.take().unwrap());
                    }
                    Event::NeedMoreInput { .. } => {
                        assert!(!remaining.is_empty(), "Unexpected NeedMoreInput at EOF");
                        let take = remaining.len().min(chunk_size);
                        let (next, rest) = remaining.split_at(take);
                        current = next;
                        remaining = rest;
                    }
                    Event::Complete => break,
                }
            }

            assert_eq!(expected.len(), frames.len());
            for (fc, (f, sf)) in expected.iter().zip(frames.iter()).enumerate() {
                compare_frames(Path::new(path), fc, f, sf);
            }
        }
        Ok(())
    });
}
