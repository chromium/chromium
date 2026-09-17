// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::cell::RefCell;
use std::path::Path;
use std::rc::Rc;

use crate::api::process::SequentialRunner;
use crate::api::{
    JxlDataFormat, JxlDecoder, JxlDecoderOptions, JxlParallelRunner, JxlPixelFormat,
    ProcessingResult, VisibleFrameInfo, states,
};
use crate::error::{Error, Result};
use crate::frame::Frame;
use crate::headers::FileHeader;
use crate::headers::frame_header::FrameHeader;
use crate::headers::toc::Toc;
use crate::image::{Image, JxlOutputBuffer, Rect};

#[allow(clippy::type_complexity)]
pub struct DecodeParams<'a> {
    pub chunk_size: usize,
    pub use_simple_pipeline: bool,
    pub do_flush: bool,
    pub callback: Option<Box<dyn FnMut(&FileHeader, &Frame, usize) -> Result<(), Error>>>,
    pub flush_callback: Option<&'a mut dyn FnMut(usize, usize, &[Image<f32>]) -> Result<(), Error>>,
    pub parallel_runner: Option<&'a mut dyn JxlParallelRunner>,
    pub disable_16bit_modular_buffers: bool,
    pub allow_partial: bool,
}

impl<'a> Default for DecodeParams<'a> {
    fn default() -> Self {
        Self {
            chunk_size: usize::MAX,
            use_simple_pipeline: false,
            do_flush: false,
            callback: None,
            flush_callback: None,
            parallel_runner: None,
            disable_16bit_modular_buffers: false,
            allow_partial: false,
        }
    }
}

#[allow(clippy::type_complexity)]
pub fn decode(input: &[u8]) -> Result<(usize, Vec<Vec<Image<f32>>>), Error> {
    decode_internal(input, DecodeParams::default())
}

#[allow(clippy::type_complexity)]
pub fn decode_32bit(input: &[u8]) -> Result<(usize, Vec<Vec<Image<f32>>>), Error> {
    decode_internal(
        input,
        DecodeParams {
            disable_16bit_modular_buffers: true,
            ..Default::default()
        },
    )
}

#[allow(clippy::type_complexity)]
pub fn decode_internal<'a>(
    mut input: &[u8],
    params: DecodeParams<'a>,
) -> Result<(usize, Vec<Vec<Image<f32>>>), Error> {
    let s = &mut SequentialRunner;
    let parallel_runner = params.parallel_runner.unwrap_or(s);
    let options = JxlDecoderOptions::default();
    let mut initialized_decoder = JxlDecoder::<states::Initialized>::new(options);

    if let Some(callback) = params.callback {
        initialized_decoder.set_frame_callback(callback);
    }

    let original_input_len = input.len();
    let mut chunk_input = &input[0..0];
    let chunk_size = params.chunk_size;
    let do_flush = params.do_flush;
    let mut flush_callback = params.flush_callback;
    let allow_partial = params.allow_partial;
    let mut frames = vec![];
    let mut f_idx = 0;

    macro_rules! advance_decoder {
        ($decoder: ident, $process_call: expr) => {{
            loop {
                chunk_input =
                    &input[..(chunk_input.len().saturating_add(chunk_size)).min(input.len())];
                let available_before = chunk_input.len();
                let process_result = $process_call;
                input = &input[(available_before - chunk_input.len())..];
                match process_result? {
                    ProcessingResult::Complete { result } => break result,
                    ProcessingResult::NeedsMoreInput {
                        fallback,
                        size_hint,
                    } => {
                        if input.is_empty() {
                            if allow_partial {
                                return Ok((0, vec![]));
                            }
                            panic!("Unexpected end of input ({size_hint})");
                        }
                        $decoder = fallback;
                    }
                }
            }
        }};
        ($decoder: ident, $process_call: expr; flush: $buffers: ident, $f_idx: ident) => {{
            loop {
                chunk_input =
                    &input[..(chunk_input.len().saturating_add(chunk_size)).min(input.len())];
                let available_before = chunk_input.len();
                let process_result = $process_call;
                input = &input[(available_before - chunk_input.len())..];
                match process_result? {
                    ProcessingResult::Complete { result } => break result,
                    ProcessingResult::NeedsMoreInput {
                        fallback,
                        size_hint,
                    } => {
                        let mut fallback = fallback;
                        let mut flushed = false;
                        if do_flush && !input.is_empty() {
                            let mut api_buffers: Vec<_> = $buffers
                                .iter_mut()
                                .map(|b| {
                                    JxlOutputBuffer::from_image_rect_mut(
                                        b.get_rect_mut(Rect {
                                            origin: (0, 0),
                                            size: b.size(),
                                        })
                                        .into_raw(),
                                    )
                                })
                                .collect();
                            flushed =
                                fallback.flush_pixels(&mut api_buffers, Some(parallel_runner))?;
                        }
                        if flushed {
                            if let Some(ref mut cb) = flush_callback {
                                let consumed_bytes = original_input_len - input.len();
                                cb(consumed_bytes, $f_idx, &$buffers)?;
                            }
                        }
                        if input.is_empty() {
                            if allow_partial {
                                let mut api_buffers: Vec<_> = $buffers
                                    .iter_mut()
                                    .map(|b| {
                                        JxlOutputBuffer::from_image_rect_mut(
                                            b.get_rect_mut(Rect {
                                                origin: (0, 0),
                                                size: b.size(),
                                            })
                                            .into_raw(),
                                        )
                                    })
                                    .collect();
                                let _ = fallback
                                    .flush_pixels(&mut api_buffers, Some(parallel_runner))?;
                                frames.push($buffers);
                                return Ok((frames.len(), frames));
                            }
                            panic!("Unexpected end of input ({size_hint})");
                        }
                        $decoder = fallback;
                    }
                }
            }
        }};
    }

    // Process until we have image info
    let mut decoder_with_image_info = advance_decoder!(
        initialized_decoder,
        initialized_decoder.process(&mut chunk_input, Some(parallel_runner))
    );
    decoder_with_image_info.set_use_simple_pipeline(params.use_simple_pipeline);
    if params.disable_16bit_modular_buffers {
        decoder_with_image_info.disable_16bit_modular_buffers();
    }

    // Get basic info
    let basic_info = decoder_with_image_info.basic_info().clone();
    assert!(basic_info.bit_depth.bits_per_sample() > 0);

    // Get image dimensions (after upsampling, which is the actual output size)
    let (buffer_width, buffer_height) = basic_info.size;
    assert!(buffer_width > 0);
    assert!(buffer_height > 0);

    // Explicitly request F32 pixel format (test helper returns Image<f32>)
    let default_format = decoder_with_image_info.current_pixel_format();
    let requested_format = JxlPixelFormat {
        color_type: default_format.color_type,
        color_data_format: Some(JxlDataFormat::f32()),
        extra_channel_format: default_format
            .extra_channel_format
            .iter()
            .map(|_| Some(JxlDataFormat::f32()))
            .collect(),
    };
    decoder_with_image_info
        .set_pixel_format(requested_format)
        .unwrap();

    // Get the configured pixel format
    let pixel_format = decoder_with_image_info.current_pixel_format().clone();

    let num_channels = pixel_format.color_type.samples_per_pixel();
    assert!(num_channels > 0);

    loop {
        // First channel is interleaved.
        let mut buffers = vec![Image::new_with_value(
            (buffer_width * num_channels, buffer_height),
            f32::NAN,
        )?];

        for ecf in pixel_format.extra_channel_format.iter() {
            if ecf.is_none() {
                continue;
            }
            buffers.push(Image::new_with_value(
                (buffer_width, buffer_height),
                f32::NAN,
            )?);
        }

        // Process until we have frame info
        let mut decoder_with_frame_info = advance_decoder!(
            decoder_with_image_info,
            decoder_with_image_info.process(&mut chunk_input, Some(parallel_runner));
            flush: buffers,
            f_idx
        );
        decoder_with_image_info = advance_decoder!(
            decoder_with_frame_info,
            {
                let mut api_buffers: Vec<_> = buffers
                    .iter_mut()
                    .map(|b| {
                        JxlOutputBuffer::from_image_rect_mut(
                            b.get_rect_mut(Rect {
                                origin: (0, 0),
                                size: b.size(),
                            })
                            .into_raw(),
                        )
                    })
                    .collect();
                let res = decoder_with_frame_info.process(&mut chunk_input, &mut api_buffers, Some(parallel_runner));
                drop(api_buffers);
                res
            };
            flush: buffers,
            f_idx
        );

        if !allow_partial {
            // All pixels should have been overwritten, so they should no longer be NaNs.
            for buf in buffers.iter() {
                let (xs, ys) = buf.size();
                for y in 0..ys {
                    let row = buf.row(y);
                    for (x, v) in row.iter().enumerate() {
                        assert!(!v.is_nan(), "NaN at {x} {y} (image size {xs}x{ys})");
                    }
                }
            }
        }

        frames.push(buffers);

        // Check if there are more frames
        if !decoder_with_image_info.has_more_frames() {
            let decoded_frames = decoder_with_image_info.scanned_frames().len();

            if !allow_partial {
                // Ensure we decoded at least one frame
                assert!(decoded_frames > 0, "No frames were decoded");
            }

            return Ok((decoded_frames, frames));
        }
        f_idx += 1;
    }
}

pub fn scan_frames_with_decoder(mut input: &[u8], chunk_size: usize) -> Vec<VisibleFrameInfo> {
    let mut chunk_input = &input[0..0];
    let options = JxlDecoderOptions {
        scan_frames_only: true,
        skip_preview: false,
        ..Default::default()
    };
    let mut initialized_decoder = JxlDecoder::<states::Initialized>::new(options);

    macro_rules! advance_process {
        ($decoder: ident) => {
            loop {
                chunk_input =
                    &input[..(chunk_input.len().saturating_add(chunk_size)).min(input.len())];
                let available_before = chunk_input.len();
                let process_result = $decoder.process(&mut chunk_input, None);
                input = &input[(available_before - chunk_input.len())..];
                match process_result.unwrap() {
                    ProcessingResult::Complete { result } => break result,
                    ProcessingResult::NeedsMoreInput {
                        fallback,
                        size_hint,
                    } => {
                        if input.is_empty() {
                            panic!("Unexpected end of input ({size_hint})");
                        }
                        $decoder = fallback;
                    }
                }
            }
        };
    }

    macro_rules! advance_skip {
        ($decoder: ident) => {
            loop {
                chunk_input =
                    &input[..(chunk_input.len().saturating_add(chunk_size)).min(input.len())];
                let available_before = chunk_input.len();
                let process_result = $decoder.skip_frame(&mut chunk_input);
                input = &input[(available_before - chunk_input.len())..];
                match process_result.unwrap() {
                    ProcessingResult::Complete { result } => break result,
                    ProcessingResult::NeedsMoreInput {
                        fallback,
                        size_hint,
                    } => {
                        if input.is_empty() {
                            panic!("Unexpected end of input ({size_hint})");
                        }
                        $decoder = fallback;
                    }
                }
            }
        };
    }

    let mut decoder_with_image_info = advance_process!(initialized_decoder);

    if !decoder_with_image_info.has_more_frames() {
        return decoder_with_image_info.scanned_frames().to_vec();
    }

    loop {
        let mut decoder_with_frame_info = advance_process!(decoder_with_image_info);
        decoder_with_image_info = advance_skip!(decoder_with_frame_info);
        if !decoder_with_image_info.has_more_frames() {
            break;
        }
    }

    decoder_with_image_info.scanned_frames().to_vec()
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

pub fn image_size(input: &[u8]) -> Result<(usize, usize)> {
    let mut decoder = JxlDecoder::<states::Initialized>::new(JxlDecoderOptions::default());
    let mut chunk_input = input;
    loop {
        match decoder.process(&mut chunk_input, None)? {
            ProcessingResult::Complete { result } => return Ok(result.basic_info().size),
            ProcessingResult::NeedsMoreInput { fallback, .. } => {
                decoder = fallback;
                if chunk_input.is_empty() {
                    panic!("Unexpected end of input before image info");
                }
            }
        }
    }
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

pub fn read_headers_and_toc(data: &[u8]) -> Result<(FileHeader, FrameHeader, Toc)> {
    let result = Rc::new(RefCell::new(None));

    let r = result.clone();
    decode_internal(
        data,
        DecodeParams {
            callback: Some(Box::new(move |fh, f, _| {
                let mut r = r.borrow_mut();
                if r.is_none() {
                    *r = Some((fh.clone(), f.header().clone(), f.toc().clone()));
                }
                Ok(())
            })),
            ..Default::default()
        },
    )?;

    Ok(result.take().unwrap())
}
