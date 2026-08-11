/*
 * Copyright (c) 2023.
 *
 * This software is free software;
 *
 * You can redistribute it or modify it under terms of the MIT, Apache License or Zlib license
 */

//!This crate provides a library for decoding valid
//! ITU-T Rec. T.851 (09/2005) ITU-T T.81 (JPEG-1) or JPEG images.
//!
//!
//!
//! # Features
//!  - SSE and AVX accelerated functions to speed up certain decoding operations
//!  - FAST and accurate 32 bit IDCT algorithm
//!  - Fast color convert functions
//!  - RGBA and RGBX (4-Channel) color conversion functions
//!  - YCbCr to Luma(Grayscale) conversion.
//!
//! # Usage
//! Add zune-jpeg to the dependencies in the project Cargo.toml
//!
//! ```toml
//! [dependencies]
//! zune_jpeg = "0.5"
//! ```
//! # Examples
//!
//! ## Decode a JPEG file with default arguments.
//!```no_run
//! use std::fs::read;
//! use std::io::BufReader;
//! use zune_jpeg::JpegDecoder;
//! let file_contents = BufReader::new(std::fs::File::open("a_jpeg.file").unwrap());
//! let mut decoder = JpegDecoder::new(file_contents);
//! let mut pixels = decoder.decode().unwrap();
//! ```
//!
//! ## Incremental input
//!
//! `JpegDecoder` can be retried on the same decoder when the underlying reader can
//! see more bytes later. Callers should treat `DecodeErrors::is_recoverable_eof()`
//! as the signal to feed more input and retry; any other error is a hard decode
//! failure.
//!
//! After `decode_headers()` succeeds, `info()` and `output_buffer_size()` are
//! available. During `decode_into()`, the same decoder and output buffer must be
//! kept across retries. If scan decoding returns recoverable EOF,
//! `decoded_output_bytes()` and `decoded_scanlines()` report the stable prefix of
//! the output buffer that can be displayed or copied before retrying.
//!
//! By default, row checkpoints are recorded only after a previous scan decode
//! attempt, so one-shot decoding keeps the lowest-overhead path. Call
//! `set_incremental_mode(true)` before the first `decode_into()` attempt when the
//! caller expects input to arrive incrementally; this records checkpoints during
//! baseline Huffman scans and enables progressive preview preservation on the
//! first progressive decode attempt.
//!
//! Fine-grained row checkpoints currently apply within baseline Huffman scan
//! bodies, including baseline multi-SOS / non-interleaved images. Those images may
//! still report no stable output rows until the later component scans have been
//! decoded and final assembly has run.
//!
//! Scan checkpoints store only scalar resume state: stream position, next MCU
//! row/column, restart countdown, SOS parameters, DC predictors, and bitstream
//! state. Coefficients for already-decoded component scans stay on the decoder
//! across retries. For multi-SOS images this means a retry can continue inside the
//! current component scan, then decode later component scans, but output rows are
//! not considered stable until final assembly has all component data.
//!
//! For progressive JPEGs, incremental scan preservation can render completed
//! scans as full-frame previews from committed coefficient data. The preview APIs
//! report that replaceable frame separately from the stable final-output prefix,
//! which remains zero until all scans complete. When preservation is enabled, the
//! active scan decodes into scratch coefficient storage and is committed only
//! after the scan completes. No decoder-owned preview pixel buffer is kept;
//! recoverable EOF may render committed coefficients into the caller-provided
//! output slice. Internal buffers are raw DCT coefficient planes, while preview
//! bytes are already IDCT-processed, upsampled, and converted into the requested
//! output colorspace.
//!
//! On recoverable EOF, callers can use the same output buffer in two ways:
//!
//! - `decoded_output_bytes()` / `decoded_scanlines()` describe stable final
//!   pixels. These bytes will not need to be corrected by later retries. For
//!   progressive JPEGs, this stable prefix remains zero until all scans complete.
//! - `decoded_preview_output_bytes()` / `decoded_preview_scanlines()` describe a
//!   progressive preview assembled from completed scans. These methods return
//!   `None` for non-progressive images and `Some(0)` for progressive images before
//!   the first preview is rendered.
//! - `decoded_scans()` reports how many progressive scans are represented in the
//!   current preview. It returns `None` for non-progressive images; if the value
//!   is unchanged across retries, no newer preview has been produced.
//!
//! Use the preview methods only after `decode_into()` returns a recoverable EOF.
//! A positive preview byte count means the same output buffer now contains a
//! complete provisional frame in `pixels[..preview_bytes]`. Treat that frame as
//! replaceable: repaint it when `decoded_scans()` increases, then keep retrying
//! with the same decoder and output buffer until `decode_into()` returns `Ok(())`.
//! Do not append preview bytes to the final output stream; they are a display
//! surface separate from the stable prefix reported by `decoded_output_bytes()`.
//!
//! ```no_run
//! use zune_core::bytestream::ZCursor;
//! use zune_jpeg::errors::DecodeErrors;
//! use zune_jpeg::JpegDecoder;
//!
//! fn decode_incremental(jpeg_bytes: &[u8]) -> Result<Vec<u8>, DecodeErrors> {
//!     let mut decoder = JpegDecoder::new(ZCursor::new(jpeg_bytes));
//!
//!     loop {
//!         match decoder.decode_headers() {
//!             Ok(()) => break,
//!             Err(error) if error.is_recoverable_eof() => {
//!                 // Make more input bytes visible to the same reader, then retry.
//!             }
//!             Err(error) => return Err(error)
//!         }
//!     }
//!
//!     let mut pixels = vec![0; decoder.output_buffer_size().unwrap()];
//!     decoder.set_incremental_mode(true);
//!     let mut displayed_preview_scans = 0;
//!
//!     loop {
//!         match decoder.decode_into(&mut pixels) {
//!             Ok(()) => break,
//!             Err(error) if error.is_recoverable_eof() => {
//!                 let stable_bytes = decoder.decoded_output_bytes().unwrap_or(0);
//!                 let stable_scanlines = decoder.decoded_scanlines().unwrap_or(0);
//!                 let preview_bytes = decoder.decoded_preview_output_bytes().unwrap_or(0);
//!                 let preview_scanlines = decoder.decoded_preview_scanlines().unwrap_or(0);
//!                 let preview_scans = decoder.decoded_scans().unwrap_or(0);
//!
//!                 if stable_bytes > 0 && stable_scanlines > 0 {
//!                     // Display or copy the stable prefix in `pixels[..stable_bytes]`.
//!                 } else if preview_bytes > 0
//!                     && preview_scanlines > 0
//!                     && preview_scans > displayed_preview_scans
//!                 {
//!                     // Repaint the replaceable preview in `pixels[..preview_bytes]`.
//!                     displayed_preview_scans = preview_scans;
//!                 }
//!
//!                 // Feed more input, then retry with the same decoder and `pixels` buffer.
//!             }
//!             Err(error) => return Err(error)
//!         }
//!     }
//!
//!     Ok(pixels)
//! }
//! ```
//!
//! ## Migrating from version 0.4--
//!
//! ### Motivation
//! zune v 0.5 reworks mainly the internal architecture of how we perform I/O
//! ,before the decoder accepted byte slices that represent the whole data as contiguous
//! but that was not ideal for all use cases, increasing memory e.g on massive files that had
//! to be read to memory.
//!
//! With v 0.5 a new I/O system is introduced, which generally introduces mechanisms to process
//! `std::io::Read + std::io::Seek` type of data feeds, (but which works in no-std), which means...
//!
//! ### What changes
//!
//! I/O code that looked like this
//!
//!```ignore
//! use zune_core::colorspace::ColorSpace;
//! use zune_jpeg::JpegDecoder;
//! // Read file into memory
//! let image = std::fs::read("image.jpg").unwrap();
//! // Make a decoder from the slice
//! let mut decoder = JpegDecoder::new(&image);
//! // decode
//! decoder.decode().unwrap();
//! ```
//!
//! Now can be rewritten in two ways.
//!
//! 1. File I/O (Using bufreader)
//!
//!```no_run
//! use std::io::BufReader;
//! use zune_core::colorspace::ColorSpace;
//! use zune_jpeg::JpegDecoder;
//!
//! let image = BufReader::new(std::fs::File::open("image.jpg").unwrap());
//! let mut decoder = JpegDecoder::new(image);
//! // decode
//! decoder.decode().unwrap();
//! ```
//!
//! 2. Reading to memory (but wrapping it in a Cursor like object)
//!```no_run
//! use zune_core::bytestream::ZCursor;
//! use zune_jpeg::JpegDecoder;
//!
//! let image_data =std::fs::read("image.jpg").unwrap();
//! // Alternatively, you can use std::io::Cursor,
//! // but it is better speed wise to use ZCursor, and it also works in
//! // no-std environments
//! let mut cursor = ZCursor::new(image_data);
//! // use the wrapped item
//! let mut decoder = JpegDecoder::new(cursor);
//! // decode
//! decoder.decode().unwrap();
//! ```
//!
//! 3. Anything that implements [ZByteReaderTrait](zune_core::bytestream::traits::ZByteReaderTrait)
//!
//! ## Decode a JPEG file to RGBA format
//!
//! - Other (limited) supported formats are and  BGR, BGRA
//!
//!```no_run
//! use zune_core::bytestream::ZCursor;
//! use zune_core::colorspace::ColorSpace;
//! use zune_core::options::DecoderOptions;
//! use zune_jpeg::JpegDecoder;
//!
//! let mut options = DecoderOptions::default().jpeg_set_out_colorspace(ColorSpace::RGBA);
//!
//! let mut decoder = JpegDecoder::new_with_options(ZCursor::new(&[]),options);
//! let pixels = decoder.decode().unwrap();
//! ```
//!
//! ## Decode an image and get its width and height.
//!```no_run
//! use zune_core::bytestream::ZCursor;
//! use zune_jpeg::JpegDecoder;
//!
//! let mut decoder = JpegDecoder::new(ZCursor::new(&[]));
//! decoder.decode_headers().unwrap();
//! let image_info = decoder.info().unwrap();
//! println!("{},{}",image_info.width,image_info.height)
//! ```
//! # Crate features.
//! This crate tries to be as minimal as possible while being extensible
//! enough to handle the complexities arising from parsing different types
//! of jpeg images.
//!
//! Safety is a top concern that is why we provide both static ways to disable unsafe code,
//! disabling x86 feature, and dynamic ,by using [`DecoderOptions::set_use_unsafe(false)`],
//! both of these disable platform specific optimizations, which reduce the speed of decompression.
//!
//! Please do note that careful consideration has been taken to ensure that the unsafe paths
//! are only unsafe because they depend on platform specific intrinsics, hence no need to disable them
//!
//! The crate tries to decode as many images as possible, as a best effort, even those violating the standard
//! , this means a lot of images may  get silent warnings and wrong output, but if you are sure you will be handling
//! images that follow the spec, set `ZuneJpegOptions::set_strict` to true.
//!
//![`DecoderOptions::set_use_unsafe(false)`]:  https://docs.rs/zune-core/latest/zune_core/options/struct.DecoderOptions.html#method.set_use_unsafe

#![warn(
    clippy::correctness,
    clippy::perf,
    clippy::pedantic,
    clippy::inline_always,
    clippy::missing_errors_doc,
    clippy::panic
)]
#![allow(
    clippy::needless_return,
    clippy::similar_names,
    clippy::inline_always,
    clippy::doc_markdown,
    clippy::module_name_repetitions,
    clippy::missing_panics_doc,
    clippy::missing_errors_doc
)]
// no_std compatibility
#![deny(clippy::std_instead_of_alloc, clippy::alloc_instead_of_core)]
#![cfg_attr(not(any(feature = "x86", feature = "neon")), forbid(unsafe_code))]
#![cfg_attr(not(feature = "std"), no_std)]
#![cfg_attr(feature = "portable_simd", feature(portable_simd))]
#![macro_use]
extern crate alloc;
extern crate core;

pub use zune_core;

pub use crate::components::SampleRatios;
pub use crate::decoder::{ImageInfo, JpegDecoder};
pub use crate::marker::Marker;
pub use crate::cancel::{CancelCheck, NeverCancel};
mod bitstream;
#[cfg(feature = "arith")]
mod bitstream_arith;
mod color_convert;
mod components;
mod decoder;
pub mod errors;
mod headers;
mod huffman;
#[cfg(not(fuzzing))]
mod idct;
#[cfg(fuzzing)]
pub mod idct;
mod marker;
mod mcu;
mod mcu_prog;
mod misc;
mod cancel;
mod unsafe_utils;
mod unsafe_utils_avx2;
mod unsafe_utils_neon;
mod upsampler;
mod worker;
