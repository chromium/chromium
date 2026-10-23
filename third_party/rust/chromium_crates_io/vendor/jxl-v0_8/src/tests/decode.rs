// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::path::Path;

use crate::api::{
    AlphaMode, Event, JxlDecoder, JxlDecoderOptions, JxlParallelRunner, JxlPixelFormat,
    ProfileLevel, TestOptions, VisibleFrameInfo,
};
use crate::error::{Error, Result};
use crate::headers::FileHeader;
use crate::headers::frame_header::FrameHeader;
use crate::headers::toc::Toc;
use crate::image::{Image, ImageDataType, JxlOutputBuffer, Rect};

#[allow(clippy::type_complexity)]
pub struct DecodeParams<'a, T: ImageDataType = f32> {
    pub chunk_size: usize,
    pub use_simple_pipeline: bool,
    pub do_flush: bool,
    pub flush_callback: Option<&'a mut dyn FnMut(usize, usize, &[Image<T>]) -> Result<(), Error>>,
    pub parallel_runner: Option<&'a mut dyn JxlParallelRunner>,
    pub disable_16bit_modular_buffers: bool,
    pub allow_partial: bool,
    pub pixel_format: Option<JxlPixelFormat>,
    pub alpha_mode: AlphaMode,
    pub adjust_orientation: bool,
}

impl<'a, T: ImageDataType> Default for DecodeParams<'a, T> {
    fn default() -> Self {
        Self {
            chunk_size: usize::MAX,
            use_simple_pipeline: false,
            do_flush: false,
            flush_callback: None,
            parallel_runner: None,
            disable_16bit_modular_buffers: false,
            allow_partial: false,
            pixel_format: None,
            alpha_mode: AlphaMode::AsIs,
            adjust_orientation: true,
        }
    }
}

pub fn as_output_buffers<T: ImageDataType>(bufs: &mut [Image<T>]) -> Vec<JxlOutputBuffer<'_>> {
    bufs.iter_mut()
        .map(|b| {
            JxlOutputBuffer::from_image_rect_mut(
                b.get_rect_mut(Rect {
                    origin: (0, 0),
                    size: b.size(),
                })
                .into_raw(),
            )
        })
        .collect()
}

pub fn decode<'a, T: ImageDataType>(
    mut input: &[u8],
    params: DecodeParams<'a, T>,
) -> Result<Vec<Vec<Image<T>>>, Error> {
    let mut parallel_runner = params.parallel_runner;
    let options = JxlDecoderOptions {
        adjust_orientation: params.adjust_orientation,
        alpha_mode: params.alpha_mode,
        max_profile_level: ProfileLevel::Main10,
        test_options: TestOptions {
            use_simple_pipeline: params.use_simple_pipeline,
            disable_16bit_modular_buffers: params.disable_16bit_modular_buffers,
        },
        ..Default::default()
    };
    let mut decoder = JxlDecoder::new(options);

    let original_input_len = input.len();
    let mut chunk_input = &input[0..0];
    let chunk_size = params.chunk_size;
    let do_flush = params.do_flush;
    let mut flush_callback = params.flush_callback;
    let allow_partial = params.allow_partial;
    let mut frames = vec![];

    let init = T::from_f64(f64::NAN);
    let make_buffers = |decoder: &JxlDecoder| -> Result<Vec<Image<T>>, Error> {
        let (w, h) = decoder.basic_info().unwrap().size;
        let pixel_format = decoder.current_pixel_format().unwrap();
        let num_channels = pixel_format.color_type.samples_per_pixel();
        assert!(num_channels > 0);
        let mut buffers = vec![];
        if pixel_format.color_data_format.is_some() {
            // First channel is interleaved.
            buffers.push(Image::new_with_value((w * num_channels, h), init)?);
        }
        for ecf in pixel_format.extra_channel_format.iter() {
            if ecf.is_none() {
                continue;
            }
            buffers.push(Image::new_with_value((w, h), init)?);
        }
        Ok(buffers)
    };

    let mut buffers: Option<Vec<Image<T>>> = None;

    loop {
        chunk_input = &input[..(chunk_input.len().saturating_add(chunk_size)).min(input.len())];
        let available_before = chunk_input.len();
        let mut out_bufs = buffers.as_deref_mut().map(as_output_buffers);
        let process_result = decoder.process(
            &mut chunk_input,
            out_bufs.as_deref_mut(),
            parallel_runner.as_deref_mut(),
        );
        input = &input[(available_before - chunk_input.len())..];

        match process_result? {
            Event::BasicInfo => {
                let basic_info = decoder.basic_info().unwrap();
                assert!(basic_info.bit_depth.bits_per_sample() > 0);
                let (buffer_width, buffer_height) = basic_info.size;
                assert!(buffer_width > 0);
                assert!(buffer_height > 0);

                if let Some(ref fmt) = params.pixel_format {
                    decoder.set_pixel_format(fmt.clone())?;
                }
                buffers = Some(make_buffers(&decoder)?);
            }
            Event::FrameHeader => {}
            Event::FrameComplete { has_more_frames } => {
                let completed = buffers.take().unwrap();
                if !allow_partial {
                    // All pixels should have been overwritten, so they should no longer be NaNs.
                    for buf in completed.iter() {
                        let (xs, ys) = buf.size();
                        for y in 0..ys {
                            let row = buf.row(y);
                            for (x, v) in row.iter().enumerate() {
                                assert!(
                                    !v.to_f64().is_nan(),
                                    "NaN at {x} {y} (image size {xs}x{ys})"
                                );
                            }
                        }
                    }
                }
                frames.push(completed);
                if has_more_frames {
                    buffers = Some(make_buffers(&decoder)?);
                }
            }
            Event::Complete => {
                if !allow_partial {
                    assert!(!frames.is_empty(), "No frames were decoded");
                }
                return Ok(frames);
            }
            Event::NeedMoreInput { size_hint } => {
                if !input.is_empty() {
                    if do_flush
                        && let Some(ref mut out_bufs) = out_bufs
                        && decoder.flush_pixels(out_bufs, parallel_runner.as_deref_mut())?
                        && let Some(ref mut cb) = flush_callback
                    {
                        let consumed_bytes = original_input_len - input.len();
                        cb(consumed_bytes, frames.len(), buffers.as_ref().unwrap())?;
                    }
                } else if allow_partial {
                    if let Some(ref mut out_bufs) = out_bufs {
                        let _ = decoder.flush_pixels(out_bufs, parallel_runner.as_deref_mut())?;
                        frames.push(buffers.unwrap());
                    }
                    return Ok(frames);
                } else {
                    panic!("Unexpected end of input ({size_hint})");
                }
            }
        }
    }
}

pub fn scan_frames(mut input: &[u8], chunk_size: usize) -> Vec<VisibleFrameInfo> {
    let mut chunk_input = &input[0..0];
    let options = JxlDecoderOptions {
        scan_frames_only: true,
        skip_preview: false,
        max_profile_level: ProfileLevel::Main10,
        ..Default::default()
    };
    let mut decoder = JxlDecoder::new(options);

    loop {
        chunk_input = &input[..(chunk_input.len().saturating_add(chunk_size)).min(input.len())];
        let available_before = chunk_input.len();
        let event = decoder.process(&mut chunk_input, None, None).unwrap();
        input = &input[(available_before - chunk_input.len())..];
        match event {
            Event::Complete => break,
            Event::NeedMoreInput { size_hint } => {
                if input.is_empty() {
                    panic!("Unexpected end of input ({size_hint})");
                }
            }
            Event::BasicInfo | Event::FrameHeader | Event::FrameComplete { .. } => {}
        }
    }

    decoder.scanned_frames().to_vec()
}

pub fn compute_mse(actual: &[Image<f32>], reference: &[Image<f32>]) -> f32 {
    assert_eq!(actual.len(), reference.len());
    let mut sum_sq_diff = 0.0f64;
    let mut total_pixels = 0;
    for (act_chan, ref_chan) in actual.iter().zip(reference.iter()) {
        let size = act_chan.size();
        assert_eq!(size, ref_chan.size());
        for y in 0..size.1 {
            let act_row = act_chan.row(y);
            let ref_row = ref_chan.row(y);
            for x in 0..size.0 {
                let act_val = if act_row[x].is_nan() { 0.0 } else { act_row[x] };
                let ref_val = ref_row[x];
                let diff = act_val - ref_val;
                sum_sq_diff += (diff * diff) as f64;
                total_pixels += 1;
            }
        }
    }
    if total_pixels == 0 {
        0.0
    } else {
        (sum_sq_diff / total_pixels as f64) as f32
    }
}

pub fn image_size(mut input: &[u8]) -> Result<(usize, usize)> {
    let options = JxlDecoderOptions {
        max_profile_level: ProfileLevel::Main10,
        ..Default::default()
    };
    let mut decoder = JxlDecoder::new(options);
    assert_eq!(decoder.process(&mut input, None, None)?, Event::BasicInfo);
    Ok(decoder.basic_info().unwrap().size)
}

pub fn compute_tile_quartiles(
    actual: &[Image<f32>],
    reference: &[Image<f32>],
    image_size: (usize, usize),
) -> [f32; 4] {
    assert_eq!(actual.len(), reference.len());
    let (width, height) = image_size;
    let num_channels = actual[0].size().0 / width;
    assert_eq!(actual[0].size().0, width * num_channels);
    assert_eq!(actual[0].size().1, height);

    const TILE_SIZE: usize = 64;
    let mut tile_mses = Vec::new();

    // Evaluate 64x64 tiles on natural alignment (full tiles only)
    let mut y0 = 0;
    while y0 + TILE_SIZE <= height {
        let mut x0 = 0;
        while x0 + TILE_SIZE <= width {
            let mut sum_sq_diff = 0.0f64;
            let mut tile_samples = 0usize;

            for y in y0..y0 + TILE_SIZE {
                let act_row0 = actual[0].row(y);
                let ref_row0 = reference[0].row(y);
                for x in x0..x0 + TILE_SIZE {
                    let base_idx = x * num_channels;
                    for c in 0..num_channels {
                        let act_val = act_row0[base_idx + c];
                        let ref_val = ref_row0[base_idx + c];
                        let act_val = if act_val.is_nan() { 0.0 } else { act_val };
                        let diff = act_val - ref_val;
                        sum_sq_diff += (diff * diff) as f64;
                        tile_samples += 1;
                    }
                }

                for (act_chan, ref_chan) in actual[1..].iter().zip(reference[1..].iter()) {
                    let act_row = act_chan.row(y);
                    let ref_row = ref_chan.row(y);
                    for x in x0..x0 + TILE_SIZE {
                        let act_val = act_row[x];
                        let ref_val = ref_row[x];
                        let act_val = if act_val.is_nan() { 0.0 } else { act_val };
                        let diff = act_val - ref_val;
                        sum_sq_diff += (diff * diff) as f64;
                        tile_samples += 1;
                    }
                }
            }

            if tile_samples > 0 {
                let tile_mse = (sum_sq_diff / tile_samples as f64) as f32;
                tile_mses.push(tile_mse);
            }

            x0 += TILE_SIZE;
        }
        y0 += TILE_SIZE;
    }

    if tile_mses.is_empty() {
        let mse = compute_mse(actual, reference);
        [mse, mse, mse, mse]
    } else {
        tile_mses.sort_by(|a, b| a.partial_cmp(b).unwrap_or(std::cmp::Ordering::Equal));
        let n = tile_mses.len();
        let q = |p: f64| -> f32 {
            let idx = p * (n - 1) as f64;
            let i = idx.floor() as usize;
            let frac = (idx - i as f64) as f32;
            if i + 1 < n {
                tile_mses[i] * (1.0 - frac) + tile_mses[i + 1] * frac
            } else {
                tile_mses[n - 1]
            }
        };
        [q(0.25), q(0.50), q(0.75), q(1.00)]
    }
}

pub fn compare_frames(path: &Path, fc: usize, f: &[Image<f32>], sf: &[Image<f32>]) {
    assert_eq!(f.len(), sf.len());
    for (c, (b, sb)) in f.iter().zip(sf.iter()).enumerate() {
        crate::tests::assert_image_eq!(b, sb, "channel {} frame {} for {:?}", c, fc, path);
    }
}

pub fn compare_frames_close(
    path: &Path,
    fc: usize,
    f: &[Image<f32>],
    sf: &[Image<f32>],
    max_abs_diff: f32,
) {
    assert_eq!(f.len(), sf.len());
    for (c, (chan_a, chan_b)) in f.iter().zip(sf.iter()).enumerate() {
        assert_eq!(chan_a.size(), chan_b.size(), "Size mismatch");
        let (w, h) = chan_a.size();
        for y in 0..h {
            let row_a = chan_a.row(y);
            let row_b = chan_b.row(y);
            for x in 0..w {
                let val_a = row_a[x];
                let val_b = row_b[x];
                if val_a.is_nan() && val_b.is_nan() {
                    continue;
                }
                let diff = (val_a - val_b).abs();
                if diff > max_abs_diff || val_a.is_nan() != val_b.is_nan() {
                    panic!(
                        "channel {} frame {} mismatch at ({}, {}) for {:?}: left={}, right={}, diff={}",
                        c, fc, x, y, path, val_a, val_b, diff
                    );
                }
            }
        }
    }
}

pub fn has_decoded_pixels(frames: &[Vec<Image<f32>>]) -> bool {
    frames.iter().any(|f| {
        f.iter().any(|c| {
            let (_, h) = c.size();
            (0..h).any(|y| c.row(y).iter().any(|&v| !v.is_nan()))
        })
    })
}

pub fn read_headers_and_toc(mut input: &[u8]) -> Result<(FileHeader, FrameHeader, Toc)> {
    let options = JxlDecoderOptions {
        max_profile_level: ProfileLevel::Main10,
        ..Default::default()
    };
    let mut decoder = JxlDecoder::new(options);

    for expected in [Event::BasicInfo, Event::FrameHeader] {
        assert_eq!(decoder.process(&mut input, None, None)?, expected);
    }

    let fh = decoder.file_header().unwrap().clone();
    let fr = decoder.raw_frame_header().unwrap().clone();
    let toc = decoder.toc().unwrap().clone();

    Ok((fh, fr, toc))
}
