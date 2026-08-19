/*
 * Copyright (c) 2023.
 *
 * This software is free software;
 *
 * You can redistribute it or modify it under terms of the MIT, Apache License or Zlib license
 */

//! Main image logic.
#![allow(clippy::doc_markdown)]

use alloc::boxed::Box;
use alloc::string::ToString;
use alloc::vec::Vec;
use alloc::sync::Arc;
use alloc::{format, vec};
use core::num::NonZeroU32;

use zune_core::bytestream::{ZByteReaderTrait, ZReader};
use zune_core::colorspace::ColorSpace;
use zune_core::log::{error, trace, warn};
use zune_core::options::DecoderOptions;

use crate::cancel::{CancelCheck, Debounced, CANCEL_POLL_INTERVAL_MCUS};

#[cfg(feature = "arith")]
use crate::bitstream::BitStream;
use crate::bitstream::{BitstreamStateSnapshot, BitStreamHuffman};
#[cfg(feature = "arith")]
use crate::bitstream_arith::{ArithACTables, ArithDCTables, BitStreamArithmetic};
use crate::color_convert::choose_ycbcr_to_rgb_convert_func;
use crate::components::{Components, SampleRatios};
use crate::errors::{DecodeErrors, UnsupportedSchemes};
#[cfg(feature = "arith")]
use crate::headers::parse_dac;
use crate::headers::{
    parse_app1, parse_app13, parse_app14, parse_app2, parse_dqt, parse_huffman, parse_sos,
    parse_start_of_frame, with_marker_body
};
use crate::huffman::HuffmanTable;
use crate::idct::{choose_idct_1x1_func, choose_idct_4x4_func, choose_idct_func};
use crate::marker::Marker;
use crate::misc::SOFMarkers;
use crate::upsampler::{
    choose_horizontal_samp_function, choose_hv_samp_function, choose_v_samp_function,
    generic_sampler, upsample_no_op
};

/// Maximum components
pub(crate) const MAX_COMPONENTS: usize = 4;

/// Maximum image dimensions supported.
pub(crate) const MAX_DIMENSIONS: usize = 1 << 27;

/// Color conversion function that can convert YCbCr colorspace to RGB(A/X) for
/// 16 values
///
/// The following are guarantees to the following functions
///
/// 1. The `&[i16]` slices passed contain 16 items
///
/// 2. The slices passed are in the following order
///    `y,cb,cr`
///
/// 3. `&mut [u8]` is zero initialized
///
/// 4. `&mut usize` points to the position in the array where new values should
///    be used
///
/// The pointer should
/// 1. Carry out color conversion
/// 2. Update `&mut usize` with the new position
pub type ColorConvert16Ptr = fn(&[i16; 16], &[i16; 16], &[i16; 16], &mut [u8], &mut usize);

/// IDCT  function prototype
///
/// This encapsulates a dequantize and IDCT function which will carry out the
/// following functions
///
/// Multiply each 64 element block of `&mut [i16]` with `&Aligned32<[i32;64]>`
/// Carry out IDCT (type 3 dct) on ach block of 64 i16's
pub type IDCTPtr = fn(&mut [i32; 64], &mut [i16], usize);

/// Scan-phase state kept so `decode_into` can retry or replay after SOS.
///
/// Full replay starts at the first SOS; `scan_checkpoint` can resume from a
/// later restart or row boundary when one is still valid.
#[derive(Clone)]
pub(crate) struct ScanDecodeState {
    pub(crate) scan_start_position:    usize,
    pub(crate) append_snapshot:        HeaderAppendStateSnapshot,
    pub(crate) sos_snapshot:           SosParamsSnapshot,
    pub(crate) header_snapshot:        ScanHeaderStateSnapshot,
    pub(crate) scan_checkpoint:        Option<Box<ScanCheckpoint>>,
    pub(crate) progressive_checkpoint: Option<Box<ProgressiveScanCheckpoint>>
}

/// Saved state at the start of a progressive scan.
///
/// Progressive refinement scans update existing coefficients in place, so a
/// retry must restart at a scan boundary using only completed scans. The
/// coefficient buffers themselves stay on `JpegDecoder`.
#[derive(Clone)]
pub(crate) struct ProgressiveScanCheckpoint {
    pub(crate) stream_position: usize,
    pub(crate) append_snapshot: HeaderAppendStateSnapshot,
    pub(crate) sos_snapshot:    SosParamsSnapshot,
    pub(crate) header_snapshot: ScanHeaderStateSnapshot,
    pub(crate) completed_scans: usize
}

/// SOS fields restored before replaying scan data.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) struct SosParamsSnapshot {
    pub(crate) z_order:         [usize; MAX_COMPONENTS],
    pub(crate) num_scans:       u8,
    pub(crate) scan_subsampled: bool,
    pub(crate) spec_start:      u8,
    pub(crate) spec_end:        u8,
    pub(crate) succ_high:       u8,
    pub(crate) succ_low:        u8,
    pub(crate) dc_huff_tables:  [usize; MAX_COMPONENTS],
    pub(crate) ac_huff_tables:  [usize; MAX_COMPONENTS]
}

/// Marker-defined decode state restored for first-SOS replay.
///
/// Inter-scan markers may redefine tables or decode configuration, so replay
/// must restore the values that were active when scan decoding first began.
#[derive(Clone)]
pub(crate) struct ScanHeaderStateSnapshot {
    pub(crate) qt_tables:        [Option<[i32; 64]>; MAX_COMPONENTS],
    pub(crate) entropy_tables:   EntropyTables,
    pub(crate) restart_interval: usize,
    pub(crate) input_colorspace: ColorSpace,
    pub(crate) adobe_transform:  Option<u8>,
    pub(crate) is_mjpeg:         bool
}

/// Saved state at a restart-interval or MCU-row boundary during scan decoding.
///
/// Coefficient buffers stay on `JpegDecoder`; the checkpoint only stores the
/// bitstream position, output position, scan state, and DC predictors.
///
/// Resume contract: the caller must pass the same output buffer to
/// `decode_into` on retry so previously-written pixels are preserved.
#[derive(Clone, Copy)]
pub(crate) struct ScanCheckpoint {
    /// Stream position immediately after the RST marker.
    pub(crate) stream_position:  usize,
    /// Next MCU row to decode.
    pub(crate) mcu_row:          usize,
    /// Next MCU column to decode in `mcu_row`.
    pub(crate) mcu_col:          usize,
    /// Restart countdown at this checkpoint.
    pub(crate) todo:             usize,
    /// Number of output bytes stable at this checkpoint.
    pub(crate) pixels_written:   usize,
    /// SOS/component table state at this checkpoint.
    pub(crate) sos_snapshot:     SosParamsSnapshot,
    /// Append-only metadata state at this checkpoint.
    pub(crate) append_snapshot:  HeaderAppendStateSnapshot,
    /// Per-component DC predictor state at the checkpoint: `(dc_pred, dc_diff)`.
    pub(crate) dc_predictions:   [(i32, i32); MAX_COMPONENTS],
    /// Bitstream decoder state at the checkpoint (for fine-grained resume).
    pub(crate) bitstream_state:  BitstreamStateSnapshot
}

// Snapshot append-only metadata so marker or scan replay can roll it back.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) struct HeaderAppendStateSnapshot {
    icc:  usize,
    xmp:  usize,
    gain: usize
}

impl HeaderAppendStateSnapshot {
    pub(crate) fn capture<T: ZByteReaderTrait>(decoder: &JpegDecoder<T>) -> Self {
        Self {
            icc:  decoder.icc_data.len(),
            xmp:  decoder.extended_xmp_segments.len(),
            gain: decoder.info.gain_map_info.len()
        }
    }

    pub(crate) fn rollback<T: ZByteReaderTrait>(self, decoder: &mut JpegDecoder<T>) {
        decoder.icc_data.truncate(self.icc);
        decoder.extended_xmp_segments.truncate(self.xmp);
        decoder.info.gain_map_info.truncate(self.gain);
    }
}

/// Result of handling a single marker inside the header loop.
enum MarkerStep {
    /// Continue reading further markers.
    Continue,
    /// Reached SOS; headers are done and scan starts at the current position.
    EnteredScan
}

/// An encapsulation of an ICC chunk
pub(crate) struct ICCChunk {
    pub(crate) seq_no:      u8,
    pub(crate) num_markers: u8,
    pub(crate) data:        Vec<u8>
}

// A separate struct to allow &borrowing tables while &mut borrowing components
#[derive(Clone)]
pub(crate) struct EntropyTables {
    /// DC Huffman Tables with a maximum of 4 tables for each  component
    pub(crate) dc_huffman:    [Option<HuffmanTable>; MAX_COMPONENTS],
    /// AC Huffman Tables with a maximum of 4 tables for each component
    pub(crate) ac_huffman:    [Option<HuffmanTable>; MAX_COMPONENTS],
    /// Arithmetic coding initial conditioning parameters and statistics (has a default value)
    #[cfg(feature = "arith")]
    pub(crate) dc_arithmetic: [ArithDCTables; MAX_COMPONENTS],
    /// Arithmetic coding initial conditioning parameters and statistics  (has a default value)
    #[cfg(feature = "arith")]
    pub(crate) ac_arithmetic: [ArithACTables; MAX_COMPONENTS]
}

/// A JPEG Decoder Instance.
#[allow(clippy::upper_case_acronyms, clippy::struct_excessive_bools)]
pub struct JpegDecoder<T> {
    /// Struct to hold image information from SOI
    pub(crate) info:             ImageInfo,
    ///  Quantization tables, will be set to none and the tables will
    /// be moved to `components` field
    pub(crate) qt_tables:        [Option<[i32; 64]>; MAX_COMPONENTS],
    // Entropy coding tables
    pub(crate) entropy_tables:   EntropyTables,
    /// Image components, holds information like DC prediction and quantization
    /// tables of a component
    pub(crate) components:       Vec<Components>,
    /// maximum horizontal component of all channels in the image
    pub(crate) h_max:            usize,
    // maximum vertical component of all channels in the image
    pub(crate) v_max:            usize,
    /// mcu's  width (interleaved scans)
    pub(crate) mcu_width:        usize,
    /// MCU height(interleaved scans
    pub(crate) mcu_height:       usize,
    /// Number of MCU's in the x plane
    pub(crate) mcu_x:            usize,
    /// Number of MCU's in the y plane
    pub(crate) mcu_y:            usize,
    /// Is the image interleaved?
    pub(crate) is_interleaved:   bool,
    /// Image input colorspace, should be YCbCr for a sane image, might be
    /// grayscale too
    pub(crate) input_colorspace: ColorSpace,
    /// Adobe APP14 transform, resolved with the SOF component count at SOS.
    pub(crate) adobe_transform:  Option<u8>,
    // Is the image using arithmetic coding?
    pub(crate) is_arithmetic:    bool,
    // Progressive image details
    /// Is the image progressive?
    pub(crate) is_progressive:   bool,

    /// Start of spectral scan
    pub(crate) spec_start:       u8,
    /// End of spectral scan
    pub(crate) spec_end:         u8,
    /// Successive approximation bit position high
    pub(crate) succ_high:        u8,
    /// Successive approximation bit position low
    pub(crate) succ_low:         u8,
    /// Number of components.
    pub(crate) num_scans:        u8,
    /// For a scan, check if any component has vertical/horizontal sampling.
    pub(crate) scan_subsampled:  bool,
    // Function pointers, for pointy stuff.
    /// Dequantize and idct function
    // This is determined at runtime which function to run, statically it's
    // initialized to a platform independent one and during initialization
    // of this struct, we check if we can switch to a faster one which
    // depend on certain CPU extensions.
    pub(crate) idct_func: IDCTPtr,
    /// Specialized IDCT when we can guarantee only few coefficients are non-zero.
    ///
    /// **The callee must uphold a contract**. See [`choose_idct_4x4_func`].
    pub(crate) idct_4x4_func: IDCTPtr,
    pub(crate) idct_1x1_func: IDCTPtr,
    // Color convert function which acts on 16 YCbCr values
    pub(crate) color_convert_16: ColorConvert16Ptr,
    pub(crate) z_order:          [usize; MAX_COMPONENTS],
    /// restart markers
    pub(crate) restart_interval: usize,
    pub(crate) todo:             usize,
    // decoder options
    pub(crate) options:          DecoderOptions,
    // cooperative cancellation check polled during decode
    pub(crate) cancel:           Option<Arc<dyn CancelCheck>>,
    // MCUs of decoding work between polls of `cancel`; see set_cancel_interval
    pub(crate) poll_interval:    usize,
    // byte-stream
    pub(crate) stream:           ZReader<T>,
    // Indicate whether headers have been decoded
    pub(crate) headers_decoded:  bool,
    pub(crate) seen_sof:         bool,

    // exif data, lifted from app2
    pub(crate) icc_data: Vec<ICCChunk>,
    pub(crate) is_mjpeg: bool,
    pub(crate) coeff:    usize, // Solves some weird bug :)
    /// Extended XMP segments
    pub(crate) extended_xmp_segments: Vec<ExtendedXmpSegment>,
    /// Stream position where the header parser should resume on a future
    /// call after a recoverable EOF. Zero means "start from SOI".
    ///
    /// This is intentionally a plain scalar (not an enum variant or boxed
    /// payload) so that one-shot decoding pays no per-call match cost.
    header_resume_position: usize,
    /// Scan-phase resume state. `Some` from SOS onward; `None` during
    /// header parsing. Boxed so the decoder struct stays compact for the
    /// common one-shot path.
    scan_state: Option<Box<ScanDecodeState>>,
    /// Number of output bytes known to be stable after the most recent
    /// `decode_into` attempt.
    pub(crate) pixels_decoded: usize,
    /// Persistent coefficient buffers for multi-SOS baseline decoding.
    ///
    /// Owned by the decoder so contents survive a recoverable EOF and the
    /// next `decode_into` retry can resume from where it stopped without
    /// copying anything. Progressive decoding also uses these as the committed
    /// coefficient buffers for completed scans. The inner `Vec`s are reused
    /// across `decode_into` calls; capacity is reclaimed only when the decoder
    /// is dropped.
    pub(crate) progressive_mcus_buffer: [Vec<i16>; MAX_COMPONENTS],
    /// Number of progressive scans committed into `progressive_mcus_buffer`.
    pub(crate) progressive_completed_scans: usize,
    /// Number of committed progressive scans currently rendered as preview
    /// pixels in the output buffer.
    pub(crate) progressive_displayed_scans: usize,
    /// Whether per-row checkpointing is enabled for the current decode.
    ///
    /// By default this becomes `true` after a previous scan attempt has run,
    /// keeping one-shot decode free of per-row overhead. `incremental_mode`
    /// enables the same checkpoints on the first scan attempt for streaming
    /// callers.
    pub(crate) mcu_checkpoints_enabled: bool,
    /// Whether row checkpoints should also be recorded on the first scan
    /// decode attempt.
    ///
    /// Disabled by default to keep one-shot decode free of checkpoint work;
    /// streaming callers can opt in before `decode_into` to avoid replaying
    /// from scan start after the first recoverable scan EOF.
    incremental_mode: bool,
    /// Whether this decoder has already attempted scan decoding.
    ///
    /// `scan_state` becomes `Some` as soon as headers reach SOS, including
    /// after an explicit `decode_headers` call. This flag tracks the narrower
    /// condition needed for default checkpoint gating: a previous
    /// `decode_into` scan attempt actually ran.
    scan_decode_attempted: bool,
    /// Scratch buffer that header marker parsers fill with the marker body
    /// before mutating decoder state.
    ///
    /// Reading the full marker body up front (length + payload) means that
    /// any `ExhaustedData` failure happens *before* any side effects are
    /// committed to the decoder; a retry replays the same marker bytes
    /// idempotently. The buffer is reused across markers so header parsing
    /// stays allocation-free in steady state.
    pub(crate) marker_body_scratch: Vec<u8>,
    /// True when the SOF header carried a height of 0, meaning the actual
    /// number of lines is defined by a DNL marker that follows the first
    /// scan's entropy data. The MCU decode loop will intercept that marker
    /// and store the real height; if it never arrives, decoding returns an
    /// error.
    pub(crate) expects_dnl: bool
}

impl<T> JpegDecoder<T>
where
    T: ZByteReaderTrait
{
    // Mark the current stream position as a safe resume point at a marker
    // boundary; on a future retry decode_headers_internal will seek here
    // instead of restarting from SOI.
    fn stream_position(&mut self) -> Result<usize, DecodeErrors> {
        let position = self.stream.position()?;
        usize::try_from(position).map_err(|_| {
            DecodeErrors::FormatStatic("Stream position does not fit in usize")
        })
    }

    fn checkpoint_headers(&mut self) -> Result<(), DecodeErrors> {
        let resume_position = self.stream_position()?;
        self.header_resume_position = resume_position;
        Ok(())
    }

    fn capture_sos_params(&self) -> SosParamsSnapshot {
        SosParamsSnapshot {
            z_order:         self.z_order,
            num_scans:       self.num_scans,
            scan_subsampled: self.scan_subsampled,
            spec_start:      self.spec_start,
            spec_end:        self.spec_end,
            succ_high:       self.succ_high,
            succ_low:        self.succ_low,
            dc_huff_tables:  core::array::from_fn(|i| {
                self.components
                    .get(i)
                    .map_or(0, |component| component.dc_huff_table)
            }),
            ac_huff_tables:  core::array::from_fn(|i| {
                self.components
                    .get(i)
                    .map_or(0, |component| component.ac_huff_table)
            })
        }
    }

    pub(crate) fn capture_scan_header_state(&self) -> ScanHeaderStateSnapshot {
        ScanHeaderStateSnapshot {
            qt_tables:        self.qt_tables,
            entropy_tables:   self.entropy_tables.clone(),
            restart_interval: self.restart_interval,
            input_colorspace: self.input_colorspace,
            adobe_transform:  self.adobe_transform,
            is_mjpeg:         self.is_mjpeg
        }
    }

    pub(crate) fn restore_scan_header_state(&mut self, snapshot: &ScanHeaderStateSnapshot) {
        self.qt_tables = snapshot.qt_tables;
        self.entropy_tables = snapshot.entropy_tables.clone();
        self.restart_interval = snapshot.restart_interval;
        self.input_colorspace = snapshot.input_colorspace;
        self.adobe_transform = snapshot.adobe_transform;
        self.is_mjpeg = snapshot.is_mjpeg;
    }

    fn enter_scan_state(&mut self) -> Result<(), DecodeErrors> {
        let scan_start_position = self.stream_position()?;
        let append_snapshot = HeaderAppendStateSnapshot::capture(self);
        let sos_snapshot = self.capture_sos_params();
        let header_snapshot = self.capture_scan_header_state();
        self.scan_state = Some(Box::new(ScanDecodeState {
            scan_start_position,
            append_snapshot,
            sos_snapshot,
            header_snapshot,
            scan_checkpoint: None,
            progressive_checkpoint: None
        }));
        Ok(())
    }

    pub(crate) fn scan_checkpoint(&self) -> Option<&ScanCheckpoint> {
        self.scan_state
            .as_deref()
            .and_then(|state| state.scan_checkpoint.as_deref())
    }

    pub(crate) fn progressive_scan_checkpoint(&self) -> Option<&ProgressiveScanCheckpoint> {
        self.scan_state
            .as_deref()
            .and_then(|state| state.progressive_checkpoint.as_deref())
    }

    pub(crate) fn checkpoint_progressive_scan(
        &mut self, completed_scans: usize
    ) -> Result<(), DecodeErrors> {
        let stream_position = self.stream_position()?;
        let append_snapshot = HeaderAppendStateSnapshot::capture(self);
        let sos_snapshot = self.capture_sos_params();
        let header_snapshot = self.capture_scan_header_state();

        if let Some(state) = self.scan_state.as_mut() {
            state.progressive_checkpoint = Some(Box::new(ProgressiveScanCheckpoint {
                stream_position,
                append_snapshot,
                sos_snapshot,
                header_snapshot,
                completed_scans
            }));
        }
        Ok(())
    }

    pub(crate) fn invalidate_progressive_scan_checkpoint(&mut self) {
        if let Some(state) = self.scan_state.as_mut() {
            state.progressive_checkpoint = None;
        }
    }

    // Save a scan checkpoint at the current restart or MCU-row boundary.
    // Allocation-free: this only writes `Copy` scalars and fixed-size
    // arrays into the existing `Box<ScanCheckpoint>` (or allocates the box
    // exactly once at the first RST in a scan). The decoded coefficient and
    // component buffers themselves are *not* copied here — they live on the
    // decoder (`Components::raw_coeff`, `progressive_mcus_buffer`) and
    // persist across `decode_into` retries.
    pub(crate) fn checkpoint_scan(
        &mut self, mcu_row: usize, mcu_col: usize, pixels_written: usize,
        dc_predictions: [(i32, i32); MAX_COMPONENTS]
    ) -> Result<(), DecodeErrors> {
        self.checkpoint_scan_with_bitstream(
            mcu_row,
            mcu_col,
            pixels_written,
            dc_predictions,
            BitstreamStateSnapshot::None
        )
    }

    /// Like `checkpoint_scan` but also saves the bitstream decoder state for
    /// row-granularity resume.
    pub(crate) fn checkpoint_scan_with_bitstream(
        &mut self, mcu_row: usize, mcu_col: usize, pixels_written: usize,
        dc_predictions: [(i32, i32); MAX_COMPONENTS],
        bitstream_state: BitstreamStateSnapshot
    ) -> Result<(), DecodeErrors> {
        let stream_position = self.stream_position()?;
        let sos_snapshot = self.capture_sos_params();
        let append_snapshot = HeaderAppendStateSnapshot::capture(self);

        if let Some(state) = self.scan_state.as_mut() {
            let snapshot = ScanCheckpoint {
                stream_position,
                mcu_row,
                mcu_col,
                todo: self.todo,
                pixels_written,
                sos_snapshot,
                append_snapshot,
                dc_predictions,
                bitstream_state
            };
            match &mut state.scan_checkpoint {
                Some(existing) => **existing = snapshot,
                None => state.scan_checkpoint = Some(Box::new(snapshot))
            }
        }
        Ok(())
    }

    /// Drop the active scan checkpoint, if any.
    ///
    /// Called from the single-SOS baseline path after each row's
    /// `post_process` succeeds, and after a later SOS is fully parsed in the
    /// multi-SOS path (`advance_to_next_sos`). In the single-SOS case the next
    /// iteration of the outer loop will overwrite `Components::raw_coeff`, so
    /// any checkpoint that pointed at the just-processed row is no longer safe
    /// to resume to. New checkpoints get recorded as RSTs fire in the next row;
    /// if EOF happens before the next row's first RST, the scan falls back to
    /// replaying from scan start.
    pub(crate) fn invalidate_scan_checkpoint(&mut self) {
        if let Some(state) = self.scan_state.as_mut() {
            state.scan_checkpoint = None;
        }
    }

    // Match output colorspace; we only care for ycbcr to rgb/rgba here, in
    // case one is using another colorspace may god help you.
    fn set_color_convert_from_options(&mut self) {
        let out_colorspace = self.options.jpeg_get_out_colorspace();
        if matches!(
            out_colorspace,
            ColorSpace::BGR | ColorSpace::BGRA | ColorSpace::RGB | ColorSpace::RGBA
        ) {
            self.color_convert_16 = choose_ycbcr_to_rgb_convert_func(
                self.options.jpeg_get_out_colorspace(),
                &self.options
            )
            .unwrap();
        }
    }

    #[allow(clippy::redundant_field_names)]
    fn default(options: DecoderOptions, buffer: T) -> Self {
        let color_convert = choose_ycbcr_to_rgb_convert_func(ColorSpace::RGB, &options).unwrap();
        JpegDecoder {
            info:                  ImageInfo::default(),
            qt_tables:             [None, None, None, None],
            entropy_tables:        EntropyTables {
                dc_huffman: [None, None, None, None],
                ac_huffman: [None, None, None, None],
                #[cfg(feature = "arith")]
                dc_arithmetic: [
                    ArithDCTables::default(),
                    ArithDCTables::default(),
                    ArithDCTables::default(),
                    ArithDCTables::default()
                ],
                #[cfg(feature = "arith")]
                ac_arithmetic: [
                    ArithACTables::default(),
                    ArithACTables::default(),
                    ArithACTables::default(),
                    ArithACTables::default()
                ]
            },
            components:        vec![],
            // Interleaved information
            h_max:                       1,
            v_max:                       1,
            mcu_height:                  0,
            mcu_width:                   0,
            mcu_x:                       0,
            mcu_y:                       0,
            is_interleaved:              false,
            is_arithmetic:               false,
            is_progressive:              false,
            spec_start:                  0,
            spec_end:                    0,
            succ_high:                   0,
            succ_low:                    0,
            num_scans:                   0,
            scan_subsampled:             false,
            idct_func:                   choose_idct_func(&options),
            idct_4x4_func:               choose_idct_4x4_func(&options),
            idct_1x1_func:               choose_idct_1x1_func(&options),
            color_convert_16:            color_convert,
            input_colorspace:            ColorSpace::YCbCr,
            adobe_transform:             None,
            z_order:                     [0; MAX_COMPONENTS],
            restart_interval:            0,
            todo:                        0x7fff_ffff,
            options:                     options,
            cancel:                      None,
            poll_interval:               CANCEL_POLL_INTERVAL_MCUS,
            stream:                      ZReader::new(buffer),
            headers_decoded:             false,
            seen_sof:                    false,
            icc_data:                    vec![],
            is_mjpeg:                    false,
            coeff:                       1,
            extended_xmp_segments:       vec![],
            header_resume_position:      0,
            scan_state:                  None,
            pixels_decoded:              0,
            mcu_checkpoints_enabled:     false,
            incremental_mode:            false,
            scan_decode_attempted:       false,
            progressive_mcus_buffer:     core::array::from_fn(|_| Vec::new()),
            progressive_completed_scans: 0,
            progressive_displayed_scans: 0,
            marker_body_scratch:         Vec::new(),
            expects_dnl:                 false
        }
    }
    /// Decode a buffer already in memory
    ///
    /// The buffer should be a valid jpeg file, perhaps created by the command
    /// `std:::fs::read()` or a JPEG file downloaded from the internet.
    ///
    /// # Errors
    /// See DecodeErrors for an explanation
    pub fn decode(&mut self) -> Result<Vec<u8>, DecodeErrors> {
        self.decode_headers()?;
        self.ensure_supported_sample_precision()?;
        self.ensure_supported_encoding()?;

        if self.expects_dnl {
            // Height is unknown until DNL is encountered during entropy
            // decoding. Pre-allocate a buffer large enough for the worst case
            // (the configured max height), run decode_into normally — the MCU
            // loop will intercept the DNL marker and set info.height — then
            // truncate to the actual decoded size.
            let max_size = self.options.max_height()
                .checked_mul(usize::from(self.info.width))
                .and_then(|v| v.checked_mul(self.options.jpeg_get_out_colorspace().num_components()))
                .ok_or(DecodeErrors::FormatStatic("DNL image dimensions overflow usize"))?;
            let mut out = vec![0u8; max_size];
            self.decode_into(&mut out)?;
            // After decode_into, info.height has been set by the DNL handler.
            let actual_size = self.output_buffer_size()
                .ok_or(DecodeErrors::FormatStatic("DNL image: output size unavailable after decode"))?;
            out.truncate(actual_size);
            return Ok(out);
        }

        let size = self.output_buffer_size().unwrap();
        let mut out = vec![0; size];
        self.decode_into(&mut out)?;
        Ok(out)
    }

    /// Create a new Decoder instance
    ///
    /// # Arguments
    ///  - `stream`: The raw bytes of a jpeg file.
    #[must_use]
    #[allow(clippy::new_without_default)]
    pub fn new(stream: T) -> JpegDecoder<T> {
        JpegDecoder::default(DecoderOptions::default(), stream)
    }
    /// Return the inner stream
    pub fn into_inner(self) -> T {
        self.stream.consume()
    }
    pub fn inner_reader(&mut self) -> &mut ZReader<T> {
        &mut self.stream
    }

    /// Returns the image information
    ///
    /// This **must** be called after a subsequent call to [`decode`] or [`decode_headers`]
    /// it will return `None`
    ///
    /// # Returns
    /// - `Some(info)`: Image information,width, height, number of components
    /// - None: Indicates image headers haven't been decoded
    ///
    /// [`decode`]: JpegDecoder::decode
    /// [`decode_headers`]: JpegDecoder::decode_headers
    #[must_use]
    pub fn info(&self) -> Option<ImageInfo> {
        // we check for fails to that call by comparing what we have to the default, if
        // it's default we assume that the caller failed to uphold the
        // guarantees. We can be sure that an image cannot be the default since
        // its a hard panic in-case width or height are set to zero.
        if !self.headers_decoded {
            return None;
        }

        return Some(self.info.clone());
    }

    /// Return the number of bytes required to hold a decoded image frame
    /// decoded using the given input transformations
    ///
    /// # Returns
    ///  - `Some(usize)`: Minimum size for a buffer needed to decode the image
    ///  - `None`: Indicates headers are unavailable or image dimensions overflow `usize`
    ///
    #[must_use]
    pub fn output_buffer_size(&self) -> Option<usize> {
        return if self.headers_decoded {
            Some(
                usize::from(self.width())
                    .checked_mul(usize::from(self.height()))?
                    .checked_mul(self.options.jpeg_get_out_colorspace().num_components())?
            )
        } else {
            None
        };
    }

    /// Return the number of output bytes known to be stable after the most
    /// recent `decode_into` attempt.
    ///
    /// On recoverable EOF this is the prefix the caller may display or copy,
    /// provided the next retry uses the same decoder and output buffer. It is
    /// `None` until headers are complete and the output layout is known.
    #[must_use]
    pub fn decoded_output_bytes(&self) -> Option<usize> {
        Some(self.pixels_decoded.min(self.output_buffer_size()?))
    }

    /// Return the number of progressive scans whose coefficients are safely
    /// committed.
    ///
    /// Returns `None` until headers are decoded, and for non-progressive
    /// images. For progressive images, the value is the number of completed
    /// scans committed into the decoder-owned coefficient buffers. If the
    /// value does not change across retries, the currently rendered preview has
    /// not advanced to a newer scan.
    #[must_use]
    pub fn decoded_scans(&self) -> Option<usize> {
        if !self.headers_decoded || !self.is_progressive {
            return None;
        }
        Some(self.progressive_completed_scans)
    }

    /// Return the number of output bytes currently holding a progressive
    /// preview image.
    ///
    /// Progressive previews are replaceable full-frame renders assembled from
    /// completed scans. The bytes are already IDCT-processed, upsampled, and
    /// color-converted into the caller's output buffer; raw coefficient planes
    /// are not exposed. On the first scan attempt, preview preservation is
    /// enabled only if [`set_incremental_mode`](Self::set_incremental_mode) was
    /// called before decoding began.
    ///
    /// Returns `None` until headers are decoded, and for non-progressive
    /// images. For progressive images, returns `Some(0)` until the first
    /// completed scan has been rendered, then returns the full output buffer
    /// size for the current preview.
    #[must_use]
    pub fn decoded_preview_output_bytes(&self) -> Option<usize> {
        if !self.headers_decoded || !self.is_progressive {
            return None;
        }
        if self.progressive_displayed_scans == 0 {
            return Some(0);
        }
        self.output_buffer_size()
    }

    /// Return the number of scanlines currently holding a progressive preview.
    ///
    /// Progressive previews are full-frame renders, so this returns image height
    /// once a preview has been rendered and `Some(0)` before then.
    #[must_use]
    pub fn decoded_preview_scanlines(&self) -> Option<usize> {
        let preview_bytes = self.decoded_preview_output_bytes()?;
        let row_stride = usize::from(self.width())
            .checked_mul(self.options.jpeg_get_out_colorspace().num_components())?;
        if row_stride == 0 || preview_bytes == 0 {
            return Some(0);
        }
        Some((preview_bytes / row_stride).min(usize::from(self.height())))
    }

    /// Return whether incremental mode is enabled.
    ///
    /// Incremental mode records per-row checkpoints during the first scan
    /// decode attempt, allowing a later retry after recoverable EOF to resume
    /// from the latest stable row instead of replaying from scan start.
    ///
    /// It is disabled by default so one-shot decoding keeps the lowest
    /// overhead path.
    #[must_use]
    pub const fn incremental_mode(&self) -> bool {
        self.incremental_mode
    }

    /// Enable or disable incremental mode.
    ///
    /// Call this before the first `decode_into` scan attempt when the caller
    /// expects input to arrive incrementally. For baseline images, Huffman scans
    /// save row checkpoints on the first attempt. For progressive images, the
    /// active scan decodes through scratch coefficient storage so completed
    /// scans can be rendered as previews if that first attempt reaches EOF.
    ///
    /// The default is `false`. A first-attempt one-shot progressive decode then
    /// updates its existing coefficient buffers directly and does not clone them.
    /// Incremental mode clones only the coefficient buffers touched by the active
    /// progressive scan. After any previous scan decode attempt, later attempts
    /// enable the same preservation automatically so retries remain idempotent.
    pub fn set_incremental_mode(&mut self, enabled: bool) {
        self.incremental_mode = enabled;
    }

    /// Return the number of output scanlines known to be stable after the
    /// most recent `decode_into` attempt.
    ///
    /// This is useful after a recoverable EOF: callers can keep the same
    /// output buffer, display the stable prefix, grow the input stream, and
    /// call `decode_into` again to continue decoding.
    ///
    #[must_use]
    pub fn decoded_scanlines(&self) -> Option<usize> {
        let decoded_output_bytes = self.decoded_output_bytes()?;
        let row_stride = usize::from(self.width())
            .checked_mul(self.options.jpeg_get_out_colorspace().num_components())?;
        if row_stride == 0 {
            return Some(0);
        }

        Some((decoded_output_bytes / row_stride).min(usize::from(self.height())))
    }

    /// Get an immutable reference to the decoder options
    /// for the decoder instance
    ///
    /// This can be used to modify options before actual decoding
    /// but after initial creation
    ///
    /// # Example
    /// ```no_run
    /// use zune_core::bytestream::ZCursor;
    /// use zune_jpeg::JpegDecoder;
    ///
    /// let mut decoder = JpegDecoder::new(ZCursor::new(&[]));
    /// // get current options
    /// let mut options = decoder.options();
    /// // modify it
    ///  let new_options = options.set_max_width(10);
    /// // set it back
    /// decoder.set_options(new_options);
    ///
    /// ```
    #[must_use]
    pub const fn options(&self) -> &DecoderOptions {
        &self.options
    }
    /// Return the input colorspace of the image
    ///
    /// This indicates the colorspace that is present in
    /// the image, but this may be different to the colorspace that
    /// the output will be transformed to
    ///
    /// # Returns
    /// -`Some(Colorspace)`: Input colorspace
    /// - None : Indicates the headers weren't decoded
    #[must_use]
    pub fn input_colorspace(&self) -> Option<ColorSpace> {
        return if self.headers_decoded { Some(self.input_colorspace) } else { None };
    }
    /// Set decoder options
    ///
    /// This can be used to set new options even after initialization
    /// but before decoding.
    ///
    /// This does not bear any significance after decoding an image
    ///
    /// # Arguments
    /// - `options`: New decoder options
    ///
    /// # Example
    /// Set maximum jpeg progressive passes to be 4
    ///
    /// ```no_run
    /// use zune_core::bytestream::ZCursor;
    /// use zune_jpeg::JpegDecoder;
    /// let mut decoder =JpegDecoder::new(ZCursor::new(&[]));
    /// // this works also because DecoderOptions implements `Copy`
    /// let options = decoder.options().jpeg_set_max_scans(4);
    /// // set the new options
    /// decoder.set_options(options);
    /// // now decode
    /// decoder.decode().unwrap();
    /// ```
    /// Set a cooperative cancellation check that is polled during decoding,
    /// about every 1024 MCUs of decoding work by default (see
    /// [`set_cancel_interval`](Self::set_cancel_interval) to change the rate).
    ///
    /// Any `Fn() -> bool` that is `Send + Sync` works as the check (e.g. a
    /// closure over an `Arc<AtomicBool>` or a deadline). If it fires, decoding
    /// returns
    /// [`DecodeErrors::Cancelled`](crate::errors::DecodeErrors::Cancelled).
    /// Passing [`NeverCancel`](crate::NeverCancel) (or any check whose
    /// [`may_cancel`](crate::CancelCheck::may_cancel) is `false`) clears it; the
    /// default is no check, which costs a single predicted branch per poll.
    pub fn set_cancel(&mut self, cancel: impl CancelCheck + 'static) {
        self.cancel = if cancel.may_cancel() {
            Some(Arc::new(cancel) as Arc<dyn CancelCheck>)
        } else {
            None
        };
    }

    /// Set how many MCUs of decoding work pass between polls of the cancel
    /// check set with [`set_cancel`](Self::set_cancel). Defaults to 1024.
    ///
    /// Smaller values poll more often — more responsive cancellation for a
    /// marginally higher polling cost — while larger values poll less often.
    /// The decoder rounds the interval down to whole MCU rows, so the finest
    /// effective granularity is one poll per MCU row; `1` selects it. Zero is
    /// treated as one.
    pub fn set_cancel_interval(&mut self, mcus: usize) {
        self.poll_interval = mcus.max(1);
    }

    /// MCUs of decoding work between polls of the cancel check; see
    /// [`set_cancel_interval`](Self::set_cancel_interval).
    #[must_use]
    pub fn cancel_interval(&self) -> usize {
        self.poll_interval
    }

    /// A stack-local [`Debounced`] view of the cancel check for the current
    /// scan, with the poll interval scaled from MCUs to the scan's MCU-row
    /// width. Owns a clone of the check, so it can live in a `&mut self` loop.
    pub(crate) fn cancel_debounced(&self, mcu_width: usize) -> Debounced {
        Debounced::new(self.cancel.clone(), self.poll_interval / mcu_width.max(1))
    }

    pub fn set_options(&mut self, options: DecoderOptions) {
        self.options = options;
    }
    #[allow(clippy::cast_possible_truncation)]
    fn reassemble_extended_xmp(&mut self) {
        if self.extended_xmp_segments.is_empty() {
            return;
        }

        // Sort by offset
        self.extended_xmp_segments.sort_by_key(|a| a.offset);

        let guid = &self.extended_xmp_segments[0].guid;
        let total_size = self.extended_xmp_segments[0].total_size;

        // Check for consistency
        for segment in &self.extended_xmp_segments {
            if &segment.guid != guid || segment.total_size != total_size {
                error!("Inconsistent Extended XMP segments");
                self.extended_xmp_segments.clear();
                return;
            }
        }

        let mut rolling_offset = 0;
        let mut complete = true;

        for segment in &self.extended_xmp_segments {
            if segment.offset != rolling_offset {
                // Gap or overlap
                complete = false;
                break;
            }
            rolling_offset += segment.data.len() as u32;
        }

        if complete && rolling_offset == total_size {
            let mut result = Vec::with_capacity(total_size as usize);
            for segment in &self.extended_xmp_segments {
                result.extend_from_slice(&segment.data);
            }
            self.info.extended_xmp = Some(result);
            self.info.extended_xmp_guid = Some(guid.clone());
            self.extended_xmp_segments.clear();
        } else if rolling_offset > total_size {
            error!("Extended XMP overflow");
            self.extended_xmp_segments.clear();
        }
        // Else: Incomplete, wait for more.
    }
    /// Decode Decoder headers
    ///
    /// This routine takes care of parsing supported headers from a Decoder
    /// image
    ///
    /// # Supported Headers
    ///  - APP(0)
    ///  - SOF(O)
    ///  - DQT -> Quantization tables
    ///  - DHT -> Huffman tables
    ///  - SOS -> Start of Scan
    /// # Unsupported Headers
    ///  - SOF(n) -> Decoder images which are not baseline/progressive
    ///  - DAC -> Images using Arithmetic tables
    ///  - JPG(n)
    fn decode_headers_internal(&mut self) -> Result<(), DecodeErrors> {
        // Idempotent: once headers are complete (which today implies we
        // have also entered the scan phase) further calls are no-ops.
        // `header_resume_position` is intentionally not reset; callers
        // are not expected to drive header parsing again.
        if self.headers_decoded || self.scan_state.is_some() {
            return Ok(());
        }
        let resume_position = self.header_resume_position;
        if resume_position == 0 {
            // First two bytes should be jpeg soi marker
            let magic_bytes = self.stream.get_u16_be_err()?;

            if magic_bytes != 0xffd8 {
                return Err(DecodeErrors::IllegalMagicBytes(magic_bytes));
            }

            // Color convert depends only on options, so pick it once
            // on a fresh decode rather than on every resume.
            self.set_color_convert_from_options();
            self.checkpoint_headers()?;
        } else {
            self.stream.set_position(resume_position)?;
        }

        let mut last_byte = 0;
        let mut bytes_before_marker = 0;

        loop {
            // read a byte
            let mut m = self.stream.read_u8_err()?;

            // AND OF COURSE some images will have fill bytes in their marker
            // bitstreams because why not.
            //
            // I am disappointed as a man.
            if (m == 0xFF || m == 0) && last_byte == 0xFF {
                // This handles the edge case where
                // images have markers with fill bytes(0xFF)
                // or byte stuffing (0)
                // I.e 0xFF 0xFF 0xDA
                // and
                // 0xFF 0 0xDA
                // It should ignore those fill bytes and take 0xDA
                // I don't know why such images exist
                // but they do.
                // so this is for you (with love)
                while m == 0xFF || m == 0x0 {
                    last_byte = m;
                    m = self.stream.read_u8_err()?;
                }
            }
            // Last byte should be 0xFF to confirm existence of a marker since markers look
            // like OxFF(some marker data)
            if last_byte == 0xFF {
                let marker = Marker::from_u8(m);
                if let Some(n) = marker {
                    if bytes_before_marker > 3 {
                        if self.options.strict_mode()
                        /*No reason to use this*/
                        {
                            return Err(DecodeErrors::FormatStatic(
                                "[strict-mode]: Extra bytes between headers"
                            ));
                        }

                        error!(
                            "Extra bytes {} before marker 0xFF{:X}",
                            bytes_before_marker - 3,
                            m
                        );
                    }

                    bytes_before_marker = 0;

                    if let MarkerStep::EnteredScan = self.handle_known_marker(n)? {
                        return Ok(());
                    }
                } else {
                    bytes_before_marker = 0;
                    warn!("Marker 0xFF{m:X} not known");
                    self.skip_unknown_marker()?;
                }
            }
            last_byte = m;
            bytes_before_marker += 1;
        }
    }

    // Parse a recognised marker and update the resume checkpoint. Rollback of
    // append-only metadata on parser error lives inside `parse_marker_inner`
    // itself so every caller (including the inline-marker path in `mcu.rs`)
    // is protected uniformly.
    fn handle_known_marker(&mut self, n: Marker) -> Result<MarkerStep, DecodeErrors> {
        self.parse_marker_inner(n)?;

        if !self.extended_xmp_segments.is_empty() {
            self.reassemble_extended_xmp();
        }

        // break after reading the start of scan.
        // what follows is the image data
        if n == Marker::SOS {
            self.resolve_input_colorspace()?;
            trace!("Input colorspace {:?}", self.input_colorspace);
            self.headers_decoded = true;

            self.enter_scan_state()?;
            return Ok(MarkerStep::EnteredScan);
        }

        self.checkpoint_headers()?;
        Ok(MarkerStep::Continue)
    }

    fn resolve_input_colorspace(&mut self) -> Result<(), DecodeErrors> {
        self.input_colorspace = match self.adobe_transform {
            Some(0) if self.components.len() == 3 => ColorSpace::RGB,
            Some(0) => ColorSpace::CMYK,
            Some(1) => ColorSpace::YCbCr,
            Some(2) => ColorSpace::YCCK,
            Some(_) => unreachable!("APP14 parser rejects unknown transforms"),
            None if self.components.len() == 1 => ColorSpace::Luma,
            None if self.components.len() == 4 => ColorSpace::CMYK,
            None
                if self.components.len() == 3
                    && self
                        .components
                        .iter()
                        .zip(b"RGB")
                        .all(|(component, id)| component.id == *id) =>
            {
                ColorSpace::RGB
            }
            None => ColorSpace::YCbCr
        };

        if self.input_colorspace.num_components() > self.components.len() {
            if self.options.strict_mode() {
                return Err(DecodeErrors::Format(format!(
                    "Expected {} number of components but found {}",
                    self.input_colorspace.num_components(),
                    self.components.len()
                )));
            }

            if self.input_colorspace == ColorSpace::YCCK && self.components.len() == 3 {
                warn!("Treating YCCK colorspace as YCbCr because component count is 3");
                self.input_colorspace = ColorSpace::YCbCr;
            } else if let Some(component_count) = u32::try_from(self.components.len())
                .ok()
                .and_then(NonZeroU32::new)
            {
                warn!(
                    "Expected {} number of components but found {}; defaulting to multiband",
                    self.input_colorspace.num_components(),
                    self.components.len()
                );
                self.input_colorspace = ColorSpace::MultiBand(component_count);
            }
        }
        Ok(())
    }

    // Read a length-prefixed marker payload and discard its body. Shared by
    // the unknown-marker path in `decode_headers_internal` and the catch-all
    // arm in `parse_marker_inner`. The full body is buffered (and immediately
    // dropped) so this remains atomic for resumability purposes: an EOF mid-
    // payload surfaces before any decoder state is mutated.
    fn skip_marker_payload(&mut self) -> Result<(), DecodeErrors> {
        with_marker_body(self, |_, _body| {
            warn!("Skipping {} bytes", _body.body().len());
            Ok(())
        })
    }

    // Skip a marker we don't recognise, then checkpoint past it so we don't
    // need to re-skip on retry.
    fn skip_unknown_marker(&mut self) -> Result<(), DecodeErrors> {
        self.skip_marker_payload()?;
        self.checkpoint_headers()?;
        Ok(())
    }
    pub(crate) fn parse_marker_inner(&mut self, m: Marker) -> Result<(), DecodeErrors> {
        // Marker parsers are atomic: they read the full marker body into the
        // scratch buffer before mutating any decoder state, so a parser that
        // returns an error has already left the decoder in the same shape as
        // before the marker started. No explicit rollback is needed here.
        self.parse_marker_dispatch(m)
    }

    #[allow(clippy::too_many_lines)]
    fn parse_marker_dispatch(&mut self, m: Marker) -> Result<(), DecodeErrors> {
        match m {
            Marker::SOF(0..=2) => {
                // choose marker
                let (marker, is_progressive) =
                    match m {
                        Marker::SOF(0) => (SOFMarkers::BaselineDct, false),
                        Marker::SOF(1) =>
                            (SOFMarkers::ExtendedSequentialHuffman, false),
                        Marker::SOF(2) =>
                            (SOFMarkers::ProgressiveDctHuffman, true),
                        _ => unreachable!(),
                    };

                trace!("Image encoding scheme =`{marker:?}`");
                // get components
                parse_start_of_frame(marker, self)?;
                self.is_progressive = is_progressive;
            }
            Marker::SOF(3 | 11) => {
                let (marker, is_arithmetic) = match m {
                    Marker::SOF(3) => (SOFMarkers::LosslessHuffman, false),
                    Marker::SOF(11) => (SOFMarkers::LosslessArithmetic, true),
                    _ => unreachable!()
                };

                trace!("Image encoding scheme =`{marker:?}`");
                parse_start_of_frame(marker, self)?;
                self.is_progressive = false;
                self.is_arithmetic = is_arithmetic;
            }
            #[cfg(feature = "arith")]
            Marker::SOF(9..=10) => {
                // choose marker
                let (marker, is_progressive) = match m {
                    Marker::SOF(9) => (SOFMarkers::ExtendedSequentialDctArithmetic, false),
                    Marker::SOF(10) => (SOFMarkers::ProgressiveDctArithmetic, true),
                    _ => unreachable!()
                };

                trace!("Image encoding scheme =`{marker:?}`");
                // get components
                parse_start_of_frame(marker, self)?;
                self.is_arithmetic = true;
                self.is_progressive = is_progressive;
            }
            // Start of Frame Segments not supported
            Marker::SOF(v) => {
                let feature = UnsupportedSchemes::from_int(v);

                if let Some(feature) = feature {
                    return Err(DecodeErrors::Unsupported(feature));
                }

                return Err(DecodeErrors::Format(format!(
                    "Unsupported image format (SOF_{v})"
                )));
            }
            //APP(0) segment
            Marker::APP(0) => {
                // APP0 is normally the JFIF identifier (`b"JFIF\0"`), which
                // carries pixel-density metadata we currently ignore. The
                // single thing we care about here is the Motion-JPEG marker
                // — Microsoft's AVI/AVI2 container stores per-frame JPEGs
                // with an `b"AVI1\0"` identifier in APP0 instead of JFIF.
                // When we see it, we tag the decoder as MJPEG so downstream
                // logic can apply MJPEG-specific concessions (e.g. missing
                // DHT segments fall back to the standard tables). The
                // body-length guard tolerates JFIF bodies shorter than five
                // bytes by simply not matching them — anything that isn't
                // exactly the AVI1 signature is silently skipped.
                //
                // Atomic read: full body buffered first, then inspected.
                with_marker_body(self, |decoder, body| {
                    if body.body().len() >= 5 && &body.body()[..5] == b"AVI1\0" {
                        decoder.is_mjpeg = true;
                    }
                    Ok(())
                })?;
            }
            Marker::APP(1) => {
                parse_app1(self)?;
            }

            Marker::APP(2) => {
                parse_app2(self)?;
            }
            // Quantization tables
            Marker::DQT => {
                parse_dqt(self)?;
            }
            // Huffman tables
            Marker::DHT => {
                parse_huffman(self)?;
            }
            // Start of Scan Data
            Marker::SOS => {
                parse_sos(self)?;
            }
            Marker::EOI => return Err(DecodeErrors::FormatStatic("Premature End of image")),

            #[cfg(feature = "arith")]
            Marker::DAC => {
                parse_dac(self)?;
            }

            Marker::DNL => {
                // DNL before SOS is spec-illegal but tolerated by some encoders.
                // Parse the height value; if we were expecting DNL (height was 0
                // in SOF) store it, otherwise just swallow the segment.
                with_marker_body(self, |decoder, body| {
                    // DNL body must be exactly 2 bytes (the line count).
                    let b = body.body();
                    if b.len() != 2 {
                        return Err(DecodeErrors::FormatStatic(
                            "Malformed DNL segment: expected 2-byte body"
                        ));
                    }
                    let height = u16::from_be_bytes([b[0], b[1]]);
                    if decoder.expects_dnl {
                        decoder.info.set_height(height);
                        decoder.expects_dnl = false;
                    }
                    Ok(())
                })?;
            }
            Marker::DRI => {
                with_marker_body(self, |decoder, body| {
                    let body = body.body();
                    if body.len() != 2 {
                        return Err(DecodeErrors::Format(
                            "Bad DRI length, Corrupt JPEG".to_string()
                        ));
                    }
                    let restart_interval = usize::from(u16::from_be_bytes([body[0], body[1]]));
                    trace!("DRI marker present ({restart_interval})");
                    // Commit phase.
                    decoder.restart_interval = restart_interval;
                    decoder.todo = restart_interval;
                    Ok(())
                })?;
            }
            Marker::APP(14) => {
                parse_app14(self)?;
            }
            Marker::APP(13) => {
                parse_app13(self)?;
            }
            _ => {
                warn!(
                    "Capabilities for processing marker \"{m:?}\" not implemented"
                );
                self.skip_marker_payload()?;
            }
        }
        Ok(())
    }

    /// Get the embedded ICC profile if it exists
    /// and is correct
    ///
    /// One needs not to decode the whole image to extract this,
    /// calling [`decode_headers`] for an image with an ICC profile
    /// allows you to decode this
    ///
    /// # Returns
    /// - `Some(Vec<u8>)`: The raw ICC profile of the image
    /// - `None`: May indicate an error  in the ICC profile , non-existence of
    ///   an ICC profile, or that the headers weren't decoded.
    ///
    /// [`decode_headers`]:Self::decode_headers
    #[must_use]
    pub fn icc_profile(&self) -> Option<Vec<u8>> {
        let mut marker_present: [Option<&ICCChunk>; 256] = [None; 256];

        if !self.headers_decoded {
            return None;
        }
        let num_markers = self.icc_data.len();

        if num_markers == 0 || num_markers >= 255 {
            return None;
        }
        // check validity
        for chunk in &self.icc_data {
            if usize::from(chunk.num_markers) != num_markers {
                // all the lengths must match
                return None;
            }
            if chunk.seq_no == 0 {
                warn!("Zero sequence number in ICC, corrupt ICC chunk");
                return None;
            }
            if marker_present[usize::from(chunk.seq_no)].is_some() {
                // duplicate seq_no
                warn!("Duplicate sequence number in ICC, corrupt chunk");
                return None;
            }

            marker_present[usize::from(chunk.seq_no)] = Some(chunk);
        }
        let mut data = Vec::with_capacity(1000);
        // assemble the data now
        for chunk in marker_present.get(1..=num_markers).unwrap() {
            if let Some(ch) = chunk {
                data.extend_from_slice(&ch.data);
            } else {
                warn!("Missing icc sequence number, corrupt ICC chunk ");
                return None;
            }
        }

        Some(data)
    }
    /// Return the exif data for the file
    ///
    /// This returns the raw exif data starting at the
    /// TIFF header
    ///
    /// # Returns
    /// -`Some(data)`: The raw exif data, if present in the image
    /// - None: May indicate the following
    ///
    ///    1. The image doesn't have exif data
    ///    2. The image headers haven't been decoded
    #[must_use]
    pub fn exif(&self) -> Option<&Vec<u8>> {
        return self.info.exif_data.as_ref();
    }
    /// Return the XMP data for the file
    ///
    /// This returns raw XMP data starting at the XML header
    /// One needs an XML/XMP decoder to extract valuable metadata
    ///
    ///
    /// # Returns
    ///  - `Some(data)`: Raw xmp data
    ///  - `None`: May indicate the following
    ///     1. The image does not have xmp data
    ///     2. The image headers have not been decoded
    ///
    /// # Example
    ///
    /// ```no_run
    /// use zune_core::bytestream::ZCursor;
    /// use zune_jpeg::JpegDecoder;
    /// let mut decoder = JpegDecoder::new(ZCursor::new(&[]));
    /// // decode headers to extract xmp metadata if present
    /// decoder.decode_headers().unwrap();
    /// if let Some(data) = decoder.xmp(){
    ///     let stringified = String::from_utf8_lossy(data);
    ///     println!("XMP")
    /// } else{
    ///     println!("No XMP Found")
    /// }
    ///
    /// ```
    pub fn xmp(&self) -> Option<&Vec<u8>> {
        return self.info.xmp_data.as_ref();
    }
    /// Return the IPTC data for the file
    ///
    /// This returns the raw IPTC data.
    ///
    /// # Returns
    /// -`Some(data)`: The raw IPTC data, if present in the image
    /// - None: May indicate the following
    ///
    ///    1. The image doesn't have IPTC data
    ///    2. The image headers haven't been decoded
    #[must_use]
    pub fn iptc(&self) -> Option<&Vec<u8>> {
        return self.info.iptc_data.as_ref();
    }
    /// Get the output colorspace the image pixels will be decoded into
    ///
    ///
    /// # Note.
    /// This field can only be regarded after decoding headers,
    /// as markers such as Adobe APP14 may dictate different colorspaces
    /// than requested.
    ///
    /// Calling `decode_headers` is sufficient to know what colorspace the
    /// output is, if this is called after `decode` it indicates the colorspace
    /// the output is currently in
    ///
    /// Additionally, not all input->output colorspace mappings are supported
    /// but all input colorspaces can map to RGB colorspace, so that's a safe bet
    /// if one is handling image formats
    ///
    ///# Returns
    /// - `Some(Colorspace)`: If headers have been decoded, the colorspace the
    ///   output array will be in
    ///- `None
    #[must_use]
    pub fn output_colorspace(&self) -> Option<ColorSpace> {
        return if self.headers_decoded {
            Some(self.options.jpeg_get_out_colorspace())
        } else {
            None
        };
    }

    fn ensure_supported_sample_precision(&self) -> Result<(), DecodeErrors> {
        if self.info.pixel_density == 12 {
            return Err(DecodeErrors::FormatStatic(
                "12-bit JPEG pixel decoding is not supported"
            ));
        }
        Ok(())
    }

    fn ensure_supported_encoding(&self) -> Result<(), DecodeErrors> {
        let unsupported = match self.info.sof {
            SOFMarkers::LosslessHuffman => Some(UnsupportedSchemes::LosslessHuffman),
            SOFMarkers::LosslessArithmetic => Some(UnsupportedSchemes::LosslessArithmetic),
            _ => None
        };
        if let Some(unsupported) = unsupported {
            return Err(DecodeErrors::Unsupported(unsupported));
        }
        Ok(())
    }

    /// Decode into a pre-allocated buffer
    ///
    /// It is an error if the buffer size is smaller than
    /// [`output_buffer_size()`](Self::output_buffer_size)
    ///
    /// If the buffer is bigger than expected, we ignore the end padding bytes
    ///
    /// # Resumability
    ///
    /// On a recoverable EOF (`DecodeErrors::is_recoverable_eof()`) the
    /// decoder keeps enough state to resume; the caller can grow the input
    /// stream and call `decode_into` again. The caller must keep using the
    /// same decoder and output buffer for retries. After a recoverable scan
    /// EOF, [`decoded_output_bytes`](Self::decoded_output_bytes) and
    /// [`decoded_scanlines`](Self::decoded_scanlines) describe the stable
    /// prefix in that output buffer. For progressive images, use
    /// [`decoded_preview_output_bytes`](Self::decoded_preview_output_bytes),
    /// [`decoded_preview_scanlines`](Self::decoded_preview_scanlines), and
    /// [`decoded_scans`](Self::decoded_scans) to inspect any rendered preview.
    ///
    /// Embedders should use the returned error to distinguish retryable EOF
    /// from hard failures: `Err(e)` where `e.is_recoverable_eof()` means feed
    /// more input and retry, while any other `Err` is non-recoverable.
    ///
    /// By default, row checkpoints are enabled after a previous scan decode
    /// attempt, so the first one-shot decode avoids checkpoint overhead. Call
    /// [`set_incremental_mode`](Self::set_incremental_mode) before the first
    /// scan attempt to record row checkpoints immediately when input is
    /// expected to arrive incrementally.
    ///
    /// On success the decoder keeps scan-start replay state, so a later
    /// `decode_into` call is well-defined and produces bit-identical pixels.
    /// Replay re-runs entropy decoding from the first SOS.
    ///
    /// # Example
    ///
    /// - Read  headers and then alloc a buffer big enough to hold the image
    ///
    /// ```no_run
    /// use zune_core::bytestream::ZCursor;
    /// use zune_jpeg::JpegDecoder;
    /// let mut decoder = JpegDecoder::new(ZCursor::new(&[]));
    /// // before we get output, we must decode the headers to get width
    /// // height, and input colorspace
    /// decoder.decode_headers().unwrap();
    ///
    /// let mut out = vec![0;decoder.output_buffer_size().unwrap()];
    /// // write into out
    /// decoder.decode_into(&mut out).unwrap();
    /// ```
    ///
    ///
    #[allow(clippy::too_many_lines)]
    pub fn decode_into(&mut self, out: &mut [u8]) -> Result<(), DecodeErrors> {
        // Pull the scan-resume state out into owned locals so the restore
        // below can freely mutate `self`. When headers haven't completed
        // yet, `scan_plan` is `None` and we just run header decoding below.
        struct ScanPlan {
            scan_start_position:   usize,
            outer_append_snapshot: HeaderAppendStateSnapshot,
            outer_sos_snapshot:    SosParamsSnapshot,
            outer_header_snapshot: ScanHeaderStateSnapshot,
            /// Snapshots taken from the checkpoint (if any) so the seek and
            /// SOS-restore steps below do not need to touch `scan_state`.
            checkpoint_view:       Option<CheckpointView>,
            progressive_view:      Option<ProgressiveCheckpointView>
        }
        #[derive(Clone, Copy)]
        struct CheckpointView {
            append_snapshot: HeaderAppendStateSnapshot,
            sos_snapshot:    SosParamsSnapshot,
            stream_position: usize,
            todo:            usize,
            pixels_written:  usize,
            dc_predictions:  [(i32, i32); MAX_COMPONENTS]
        }
        #[derive(Clone)]
        struct ProgressiveCheckpointView {
            append_snapshot: HeaderAppendStateSnapshot,
            sos_snapshot:    SosParamsSnapshot,
            header_snapshot: ScanHeaderStateSnapshot,
            stream_position: usize,
            completed_scans: usize
        }
        let scan_plan = self.scan_state.as_deref().map(|state| ScanPlan {
            scan_start_position:   state.scan_start_position,
            outer_append_snapshot: state.append_snapshot,
            outer_sos_snapshot:    state.sos_snapshot,
            outer_header_snapshot: state.header_snapshot.clone(),
            checkpoint_view:       state.scan_checkpoint.as_deref().map(|checkpoint| {
                CheckpointView {
                    append_snapshot: checkpoint.append_snapshot,
                    sos_snapshot:    checkpoint.sos_snapshot,
                    stream_position: checkpoint.stream_position,
                    todo:            checkpoint.todo,
                    pixels_written:  checkpoint.pixels_written,
                    dc_predictions:  checkpoint.dc_predictions
                }
            }),
            progressive_view:      state.progressive_checkpoint.as_deref().map(|checkpoint| {
                ProgressiveCheckpointView {
                    append_snapshot: checkpoint.append_snapshot,
                    sos_snapshot:    checkpoint.sos_snapshot,
                    header_snapshot: checkpoint.header_snapshot.clone(),
                    stream_position: checkpoint.stream_position,
                    completed_scans: checkpoint.completed_scans
                }
            })
        });
        if let Some(plan) = scan_plan {
            let ScanPlan {
                scan_start_position,
                outer_append_snapshot,
                outer_sos_snapshot,
                outer_header_snapshot,
                checkpoint_view,
                progressive_view
            } = plan;
            // Roll back inline metadata from a previous scan attempt.
            let resume_append_snapshot = progressive_view.as_ref().map_or_else(
                || checkpoint_view.map_or(outer_append_snapshot, |view| view.append_snapshot),
                |view| view.append_snapshot
            );
            resume_append_snapshot.rollback(self);

            // Restore the SOS state for the chosen resume point.
            let resume_sos_snapshot = progressive_view.as_ref().map_or_else(
                || checkpoint_view.map_or(outer_sos_snapshot, |view| view.sos_snapshot),
                |view| view.sos_snapshot
            );
            self.z_order = resume_sos_snapshot.z_order;
            self.num_scans = resume_sos_snapshot.num_scans;
            self.scan_subsampled = resume_sos_snapshot.scan_subsampled;
            self.spec_start = resume_sos_snapshot.spec_start;
            self.spec_end = resume_sos_snapshot.spec_end;
            self.succ_high = resume_sos_snapshot.succ_high;
            self.succ_low = resume_sos_snapshot.succ_low;
            debug_assert!(
                self.components.len() <= MAX_COMPONENTS,
                "components vector exceeds MAX_COMPONENTS; SOS restore would index out of bounds"
            );
            for (i, component) in self.components.iter_mut().take(MAX_COMPONENTS).enumerate() {
                component.dc_huff_table = resume_sos_snapshot.dc_huff_tables[i];
                component.ac_huff_table = resume_sos_snapshot.ac_huff_tables[i];
            }

            let had_progressive_view = progressive_view.is_some();
            if let Some(view) = progressive_view {
                self.restore_scan_header_state(&view.header_snapshot);
                self.stream.set_position(view.stream_position)?;
                self.progressive_completed_scans = view.completed_scans;
                self.progressive_displayed_scans = view.completed_scans;
                self.pixels_decoded = 0;
                for comp in &mut self.components {
                    comp.dc_pred = 0;
                    comp.dc_diff = 0;
                }
            } else if let Some(view) = checkpoint_view {
                self.stream.set_position(view.stream_position)?;
                self.todo = view.todo;
                self.pixels_decoded = view.pixels_written;
                // Restore DC predictor state from the checkpoint.
                for (i, comp) in
                    self.components.iter_mut().enumerate().take(MAX_COMPONENTS)
                {
                    let (dc_pred, dc_diff) = view.dc_predictions[i];
                    comp.dc_pred = dc_pred;
                    comp.dc_diff = dc_diff;
                }
            } else {
                self.restore_scan_header_state(&outer_header_snapshot);
                self.stream.set_position(scan_start_position)?;
                self.pixels_decoded = 0;
                if self.is_progressive {
                    self.progressive_completed_scans = 0;
                    self.progressive_displayed_scans = 0;
                }
                // Full replay restores first-SOS tables/config and predictors.
                for comp in &mut self.components {
                    comp.dc_pred = 0;
                    comp.dc_diff = 0;
                }
            }
            if checkpoint_view.is_none() && !had_progressive_view {
                // First-SOS replay restarts progressive coefficient assembly;
                // scan-boundary retries restore `todo` in `parse_entropy_coded_data`.
                self.todo =
                    if self.restart_interval == 0 { 0x7fff_ffff } else { self.restart_interval };
            }

            if self.is_arithmetic {
                #[cfg(feature = "arith")]
                BitStreamArithmetic::reset_arith_tables(&mut self.entropy_tables);
            }
        } else {
            self.decode_headers_internal()?;
        }

        self.ensure_supported_sample_precision()?;
        self.ensure_supported_encoding()?;

        let expected_size = self.output_buffer_size().unwrap();

        if out.len() < expected_size {
            // too small of a size
            return Err(DecodeErrors::TooSmallOutput(expected_size, out.len()));
        }

        // ensure we don't touch anyone else's scratch space
        let out_len = core::cmp::min(out.len(), expected_size);
        let out = &mut out[0..out_len];

        // By default, enable per-row checkpointing only after a previous
        // scan decode attempt has run. Incremental mode opts into the same
        // checkpoints on the first scan attempt so a streaming caller avoids
        // one scan-start replay.
        let previous_scan_attempt = self.scan_decode_attempted;
        self.mcu_checkpoints_enabled = previous_scan_attempt || self.incremental_mode;
        if !previous_scan_attempt {
            self.pixels_decoded = 0;
            if self.is_progressive {
                self.progressive_displayed_scans = 0;
            }
        }
        self.scan_decode_attempted = true;

        let result: Result<(), DecodeErrors>;
        if self.is_arithmetic {
            #[cfg(feature = "arith")]
            {
                result = if self.is_progressive {
                    self.decode_mcu_ycbcr_progressive::<BitStreamArithmetic>(out)
                } else {
                    self.decode_mcu_ycbcr_baseline::<BitStreamArithmetic>(out)
                };
            }
            #[cfg(not(feature = "arith"))]
            unreachable!();
        } else if self.is_progressive {
            result = self.decode_mcu_ycbcr_progressive::<BitStreamHuffman>(out);
        } else {
            result = self.decode_mcu_ycbcr_baseline::<BitStreamHuffman>(out);
        }

        match result {
            Ok(()) => {
                // Drop the scan checkpoint so a post-success replay starts
                // from scan-start with zeroed DC predictors instead of
                // pointing at stale entropy data.
                debug_assert!(
                    self.scan_state.is_some(),
                    "scan_state should be Some after a successful scan decode"
                );
                if let Some(state) = self.scan_state.as_deref_mut() {
                    state.scan_checkpoint = None;
                    state.progressive_checkpoint = None;
                }
                self.pixels_decoded = expected_size;
                if self.is_progressive {
                    self.progressive_displayed_scans = self.progressive_completed_scans;
                }
                Ok(())
            }
            Err(e) => Err(e)
        }
    }

    /// Read only headers from a jpeg image buffer
    ///
    /// This allows you to extract important information like
    /// image width and height without decoding the full image
    ///
    /// # Examples
    /// ```no_run
    /// use zune_core::bytestream::ZCursor;
    /// use zune_jpeg::{JpegDecoder};
    ///
    /// let img_data = std::fs::read("a_valid.jpeg").unwrap();
    /// let mut decoder = JpegDecoder::new(ZCursor::new(&img_data));
    /// decoder.decode_headers().unwrap();
    ///
    /// println!("Total decoder dimensions are : {:?} pixels",decoder.dimensions());
    /// println!("Number of components in the image are {}", decoder.info().unwrap().components);
    /// ```
    /// # Errors
    /// See DecodeErrors enum for list of possible errors during decoding.
    ///
    /// If the reader runs out of data the error will satisfy
    /// [`is_recoverable_eof()`](crate::errors::DecodeErrors::is_recoverable_eof);
    /// the caller may retry after providing more data. After success,
    /// [`output_buffer_size`](Self::output_buffer_size) and [`info`](Self::info)
    /// are available.
    pub fn decode_headers(&mut self) -> Result<(), DecodeErrors> {
        self.decode_headers_internal()?;
        // For DNL images (SOF height == 0), the true line count is carried by
        // a DNL marker that appears after the entropy data of the first scan.
        // We leave info.height as 0 here; the MCU decode loop will intercept
        // the DNL marker and update it. Callers that only call decode_headers
        // will see height == 0 as an accurate reflection of the stream state.
        Ok(())
    }



    /// Create a new decoder with the specified options to be used for decoding
    /// an image
    ///
    /// # Arguments
    /// - `buf`: The input buffer from where we will pull in compressed jpeg bytes from
    /// - `options`: Options specific to this decoder instance
    #[must_use]
    pub fn new_with_options(buf: T, options: DecoderOptions) -> JpegDecoder<T> {
        JpegDecoder::default(options, buf)
    }

    /// Set up-sampling routines in case an image is down sampled
    pub(crate) fn set_upsampling(&mut self) {
        // no sampling, return early
        // check if horizontal max ==1
        if self.h_max == self.v_max && self.h_max == 1 {
            return ;
        }

        for comp in &mut self.components {
            let hs = self.h_max / comp.horizontal_sample;
            let vs = self.v_max / comp.vertical_sample;

            let samp_factor = match (hs, vs) {
                (1, 1) => {
                    comp.sample_ratio = SampleRatios::None;
                    upsample_no_op
                }
                (2, 1) => {
                    comp.sample_ratio = SampleRatios::H;
                    choose_horizontal_samp_function(&self.options)
                }
                (1, 2) => {
                    comp.sample_ratio = SampleRatios::V;
                    choose_v_samp_function(&self.options)
                }
                (2, 2) => {
                    comp.sample_ratio = SampleRatios::HV;
                    choose_hv_samp_function(&self.options)
                }
                (hs, vs) => {
                    comp.sample_ratio = SampleRatios::Generic(hs, vs);
                    generic_sampler()
                }
            };
            comp.setup_upsample_scanline();
            comp.up_sampler = samp_factor;
        }

    }
    #[must_use]
    /// Get the width of the image as a u16
    ///
    /// The width lies between 1 and 65535
    pub(crate) fn width(&self) -> u16 {
        self.info.width
    }

    /// Get the height of the image as a u16
    ///
    /// The height lies between 1 and 65535
    #[must_use]
    pub(crate) fn height(&self) -> u16 {
        self.info.height
    }

    /// Get image dimensions as a tuple of width and height
    /// or `None` if the image hasn't been decoded.
    ///
    /// # Returns
    /// - `Some(width,height)`: Image dimensions
    /// -  None : The image headers haven't been decoded
    #[must_use]
    pub const fn dimensions(&self) -> Option<(usize, usize)> {
        return if self.headers_decoded {
            Some((self.info.width as usize, self.info.height as usize))
        } else {
            None
        };
    }
}

#[derive(Default, Clone, Eq, PartialEq, Debug)]
pub struct GainMapInfo {
    pub data: Vec<u8>
}

#[derive(Default, Clone, Eq, PartialEq, Debug)]
pub(crate) struct ExtendedXmpSegment {
    pub(crate) offset: u32,
    pub(crate) total_size: u32,
    pub(crate) guid: Vec<u8>,
    pub(crate) data: Vec<u8>,
}

/// A struct representing Image Information
#[derive(Default, Clone, Eq, PartialEq)]
#[allow(clippy::module_name_repetitions)]
pub struct ImageInfo {
    /// Width of the image
    pub width: u16,
    /// Height of image
    pub height: u16,
    /// Sample precision in bits.
    pub pixel_density: u8,
    /// Start of frame markers
    pub sof: SOFMarkers,
    /// Horizontal sample
    pub x_density: u16,
    /// Vertical sample
    pub y_density: u16,
    /// Number of components
    pub components: u8,
    /// Gain Map information, useful for
    /// UHDR images
    pub gain_map_info: Vec<GainMapInfo>,
    /// Multi picture information, useful for
    /// UHDR images
    pub multi_picture_information: Option<Vec<u8>>,
    /// Exif Data
    pub exif_data: Option<Vec<u8>>,
    /// XMP Data
    pub xmp_data: Option<Vec<u8>>,
    /// IPTC Data
    pub iptc_data: Option<Vec<u8>>,
    /// Extended XMP Data
    pub extended_xmp: Option<Vec<u8>>,
    /// Extended XMP Guid
    pub extended_xmp_guid: Option<Vec<u8>>,
    /// Image sub-sampling ratio
    pub sample_ratio: SampleRatios,
    /// The offset at which Multi picture information was found
    pub multi_picture_information_offset: Option<u64>,
}

impl ImageInfo {
    /// Set width of the image
    ///
    /// Found in the start of frame
    pub(crate) fn set_width(&mut self, width: u16) {
        self.width = width;
    }

    /// Set height of the image
    ///
    /// Found in the start of frame
    pub(crate) fn set_height(&mut self, height: u16) {
        self.height = height;
    }

    /// Set the image density
    ///
    /// Found in the start of frame
    pub(crate) fn set_density(&mut self, density: u8) {
        self.pixel_density = density;
    }

    /// Set image Start of frame marker
    ///
    /// found in the Start of frame header
    pub(crate) fn set_sof_marker(&mut self, marker: SOFMarkers) {
        self.sof = marker;
    }

    /// Set image x-density(dots per pixel)
    ///
    /// Found in the APP(0) marker
    #[allow(dead_code)]
    pub(crate) fn set_x(&mut self, sample: u16) {
        self.x_density = sample;
    }

    /// Set image y-density
    ///
    /// Found in the APP(0) marker
    #[allow(dead_code)]
    pub(crate) fn set_y(&mut self, sample: u16) {
        self.y_density = sample;
    }
}
