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
use alloc::sync::Arc;
use alloc::vec::Vec;
use alloc::{format, vec};
use core::num::NonZeroU32;

use zune_core::bytestream::{ZByteReaderTrait, ZReader};
use zune_core::colorspace::ColorSpace;
use zune_core::log::{error, trace, warn};
use zune_core::options::DecoderOptions;

#[cfg(feature = "arith")]
use crate::bitstream::BitStream;
use crate::bitstream::{BitStreamHuffman, BitstreamStateSnapshot};
#[cfg(feature = "arith")]
use crate::bitstream_arith::{ArithACTables, ArithDCTables, BitStreamArithmetic};
use crate::cancel::{CancelCheck, Debounced, CANCEL_POLL_INTERVAL_MCUS};
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

/// DCT block side, in samples. JPEG always uses 8x8 blocks.
pub(crate) const DCT_BLOCK_SIZE: usize = 8;

/// Maximum image dimensions supported.
pub(crate) const MAX_DIMENSIONS: usize = 1 << 27;

/// Geometry of a single component plane for raw post-IDCT output.
///
/// Mirrors the padded buffer layout libjpeg-turbo's `jpeg_read_raw_data` expects:
/// each component owns a plane sized to `stride * allocated_height` bytes,
/// with both dimensions rounded up to DCT-block boundaries
/// (`DCTSIZE = 8`). The logical `width`/`height` describe the meaningful
/// sample area inside that buffer; trailing padding columns and rows
/// contain implementation-defined data and should be ignored by the
/// caller. The sampling-factor fields preserve the exact SOF values so
/// callers do not need to infer subsampling from rounded plane dimensions.
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq)]
#[non_exhaustive]
pub struct PlaneInfo {
    /// Horizontal sampling factor from the component's SOF entry.
    pub horizontal_sampling_factor: usize,
    /// Vertical sampling factor from the component's SOF entry.
    pub vertical_sampling_factor:   usize,
    /// Logical component width in samples.
    /// `ceil(image_width * h_samp / h_max)`.
    pub width:                      usize,
    /// Logical component height in samples.
    /// `ceil(image_height * v_samp / v_max)`.
    pub height:                     usize,
    /// Allocated plane width (row stride) in bytes.
    /// `ceil(width / 8) * 8`.
    pub stride:                     usize,
    /// Allocated plane height in rows.
    /// `ceil(height / 8) * 8`.
    pub allocated_height:           usize,
    /// Required slice length for this plane: `stride * allocated_height`.
    pub byte_size:                  usize
}

/// Round `n` up to the next multiple of `align` (which must be non-zero).
/// Returns `None` on overflow.
#[inline]
fn round_up_pow2(n: usize, align: usize) -> Option<usize> {
    debug_assert!(align != 0);
    let rem = n % align;
    if rem == 0 {
        Some(n)
    } else {
        n.checked_add(align - rem)
    }
}

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
    pub(crate) scan_start_position:        usize,
    pub(crate) append_snapshot:            HeaderAppendStateSnapshot,
    pub(crate) sos_snapshot:               SosParamsSnapshot,
    pub(crate) header_snapshot:            ScanHeaderStateSnapshot,
    pub(crate) scan_checkpoint:            Option<Box<ScanCheckpoint>>,
    pub(crate) progressive_checkpoint:     Option<Box<ProgressiveScanCheckpoint>>,
    pub(crate) progressive_fine_checkpoint: Option<Box<ProgressiveFineCheckpoint>>
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

/// Saved state inside a progressive scan when mid-scan resume is safe.
///
/// This is only recorded for first DC scans. Those coefficients are assigned
/// once, so keeping the active scan scratch buffer across EOF and resuming from
/// a later MCU boundary cannot double-apply refinement data.
#[derive(Clone)]
pub(crate) struct ProgressiveFineCheckpoint {
    pub(crate) stream_position:  usize,
    pub(crate) append_snapshot:  HeaderAppendStateSnapshot,
    pub(crate) sos_snapshot:     SosParamsSnapshot,
    pub(crate) completed_scans:  usize,
    pub(crate) displayed_scans:  usize,
    pub(crate) mcu_row:          usize,
    pub(crate) mcu_col:          usize,
    pub(crate) todo:             usize,
    pub(crate) dc_predictions:   [(i32, i32); MAX_COMPONENTS],
    pub(crate) bitstream_state:  BitstreamStateSnapshot
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
    pub(crate) stream_position: usize,
    /// Next MCU row to decode.
    pub(crate) mcu_row:         usize,
    /// Next MCU column to decode in `mcu_row`.
    pub(crate) mcu_col:         usize,
    /// Restart countdown at this checkpoint.
    pub(crate) todo:            usize,
    /// Number of output bytes stable at this checkpoint.
    pub(crate) pixels_written:  usize,
    /// SOS/component table state at this checkpoint.
    pub(crate) sos_snapshot:    SosParamsSnapshot,
    /// Append-only metadata state at this checkpoint.
    pub(crate) append_snapshot: HeaderAppendStateSnapshot,
    /// Per-component DC predictor state at the checkpoint: `(dc_pred, dc_diff)`.
    pub(crate) dc_predictions:  [(i32, i32); MAX_COMPONENTS],
    /// Bitstream decoder state at the checkpoint (for fine-grained resume).
    pub(crate) bitstream_state: BitstreamStateSnapshot
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
    pub(crate) idct_4x4_func:    IDCTPtr,
    pub(crate) idct_1x1_func:    IDCTPtr,
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
    pub(crate) icc_data:                    Vec<ICCChunk>,
    pub(crate) is_mjpeg:                    bool,
    pub(crate) coeff:                       usize, // Solves some weird bug :)
    /// Extended XMP segments
    pub(crate) extended_xmp_segments:       Vec<ExtendedXmpSegment>,
    /// Stream position where the header parser should resume on a future
    /// call after a recoverable EOF. Zero means "start from SOI".
    ///
    /// This is intentionally a plain scalar (not an enum variant or boxed
    /// payload) so that one-shot decoding pays no per-call match cost.
    header_resume_position:                 usize,
    /// Scan-phase resume state. `Some` from SOS onward; `None` during
    /// header parsing. Boxed so the decoder struct stays compact for the
    /// common one-shot path.
    scan_state:                             Option<Box<ScanDecodeState>>,
    /// Number of output bytes known to be stable after the most recent
    /// `decode_into` attempt.
    pub(crate) pixels_decoded:              usize,
    /// Persistent coefficient buffers for multi-SOS baseline decoding.
    ///
    /// Owned by the decoder so contents survive a recoverable EOF and the
    /// next `decode_into` retry can resume from where it stopped without
    /// copying anything. Progressive decoding also uses these as the committed
    /// coefficient buffers for completed scans. The inner `Vec`s are reused
    /// across `decode_into` calls; capacity is reclaimed only when the decoder
    /// is dropped.
    pub(crate) progressive_mcus_buffer: [Vec<i16>; MAX_COMPONENTS],
    /// Active progressive scan scratch buffers.
    ///
    /// Storage is retained across EOF or cancellation so retries can reuse its
    /// capacity. Safe first-DC scans also keep its contents so a later retry can
    /// resume from a fine checkpoint without committing partial scan data.
    pub(crate) progressive_scan_buffer: [Vec<i16>; MAX_COMPONENTS],
    /// Reusable baseline upsampling scratch retained across pull calls.
    pub(crate) upsampler_scratch: Vec<i16>,
    /// Number of progressive scans committed into `progressive_mcus_buffer`.
    pub(crate) progressive_completed_scans: usize,
    /// Number of committed progressive scans currently rendered as preview
    /// pixels in the output buffer.
    pub(crate) progressive_displayed_scans: usize,
    /// The output buffer contains a partially rendered progressive frame and
    /// must not be advertised until rerendering succeeds.
    pub(crate) progressive_render_incomplete: bool,
    /// Whether per-row checkpointing is enabled for the current decode.
    ///
    /// By default this becomes `true` after a previous scan attempt has run,
    /// keeping one-shot decode free of per-row overhead. `incremental_mode`
    /// enables the same checkpoints on the first scan attempt for streaming
    /// callers.
    pub(crate) mcu_checkpoints_enabled:     bool,
    /// Whether row checkpoints should also be recorded on the first scan
    /// decode attempt.
    ///
    /// Disabled by default to preserve best-effort output on scan EOF in
    /// non-strict mode and keep one-shot decode free of checkpoint work.
    /// Streaming callers opt in before `decode_into` to receive recoverable
    /// scan EOF and avoid replaying from scan start.
    incremental_mode:                       bool,
    /// Whether this decoder has already attempted scan decoding.
    ///
    /// `scan_state` becomes `Some` as soon as headers reach SOS, including
    /// after an explicit `decode_headers` call. This flag tracks the narrower
    /// condition needed for default checkpoint gating: a previous
    /// `decode_into` scan attempt actually ran.
    scan_decode_attempted:                  bool,
    /// Scratch buffer that header marker parsers fill with the marker body
    /// before mutating decoder state.
    ///
    /// Reading the full marker body up front (length + payload) means that
    /// any `ExhaustedData` failure happens *before* any side effects are
    /// committed to the decoder; a retry replays the same marker bytes
    /// idempotently. The buffer is reused across markers so header parsing
    /// stays allocation-free in steady state.
    pub(crate) marker_body_scratch:         Vec<u8>,
    /// True when the SOF header carried a height of 0, meaning the actual
    /// number of lines is defined by a DNL marker that follows the first
    /// scan's entropy data. The MCU decode loop will intercept that marker
    /// and store the real height; if it never arrives, decoding returns an
    /// error.
    pub(crate) expects_dnl:                 bool,
    /// Sequential raw iMCU-row progress shared across borrowing sessions.
    raw_pull_state:                         RawPullState,
    /// Sequential converted scanline progress shared across borrowing sessions.
    scanline_state:                         ScanlineState
}

/// Output target for one MCU decode call.
pub(crate) enum McuDecodeOutput<'planes, 'buf> {
    Pixels(&'planes mut [u8]),
    RawPlanes(RawPlanesSink<'planes, 'buf>),
    Scanlines(ConvertedScanlineSink<'planes>)
}

impl McuDecodeOutput<'_, '_> {
    pub(crate) const fn is_raw(&self) -> bool {
        matches!(self, Self::RawPlanes(_))
    }

    pub(crate) fn pixels_mut(&mut self) -> Option<&mut [u8]> {
        match self {
            Self::Pixels(pixels) => Some(pixels),
            Self::Scanlines(sink) => Some(sink.pixels),
            Self::RawPlanes(_) => None
        }
    }

    pub(crate) const fn requested_output_stripe(&self) -> Option<usize> {
        match self {
            Self::RawPlanes(sink) => sink.requested_stripe,
            Self::Scanlines(sink) => Some(sink.requested_stripe),
            Self::Pixels(_) => None
        }
    }

    pub(crate) const fn requests_raw_output(&self) -> bool {
        match self {
            Self::RawPlanes(_) => true,
            Self::Pixels(_) | Self::Scanlines(_) => false
        }
    }

    pub(crate) fn mark_source_complete(&mut self) {
        if let Self::RawPlanes(sink) = self {
            sink.source_complete = true;
        }
        if let Self::Scanlines(sink) = self {
            sink.source_complete = true;
        }
    }
}

pub(crate) struct ConvertedScanlineSink<'pixels> {
    pub(crate) pixels:           &'pixels mut [u8],
    pub(crate) stride:           usize,
    pub(crate) requested_stripe: usize,
    pub(crate) rows_written:     usize,
    pub(crate) source_complete:  bool
}

/// Caller-provided raw planar buffers for one decode call.
pub(crate) struct RawPlanesSink<'planes, 'buf> {
    /// Caller plane buffers, one per component.
    pub(crate) planes:           &'planes mut [&'buf mut [u8]],
    /// Total byte length of each caller buffer.
    pub(crate) lengths:          [usize; MAX_COMPONENTS],
    /// Bytes between successive destination rows (used for strided output).
    pub(crate) target_strides:   [usize; MAX_COMPONENTS],
    /// Maximum bytes to write per destination row.
    pub(crate) target_widths:    [usize; MAX_COMPONENTS],
    /// Number of destination rows per plane.
    pub(crate) target_heights:   [usize; MAX_COMPONENTS],
    /// Number of valid components.
    pub(crate) n_components:     usize,
    /// Pull mode requests exactly this sequential iMCU row.
    pub(crate) requested_stripe: Option<usize>,
    /// Logical rows copied for each component in pull mode.
    pub(crate) rows_written:     [usize; MAX_COMPONENTS],
    /// Whether a complete requested stripe was copied.
    pub(crate) stripe_ready:     bool,
    /// Whether entropy/coefficient production has completed.
    pub(crate) source_complete:  bool
}

/// Result of requesting the next sequential raw iMCU row.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[non_exhaustive]
pub enum RawImcuRowStatus {
    /// One complete iMCU row was written to the caller's planes.
    RowReady {
        /// Logical rows written for each component in SOF declaration order.
        /// Entries after [`RawDecodeSession::num_components`] are zero.
        rows_written: [usize; MAX_COMPONENTS]
    },
    /// The next iMCU row could not be completed with the currently visible input.
    NeedMoreInput,
    /// All raw component rows have been emitted.
    Complete
}

/// State returned by scanline session lifecycle operations.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[non_exhaustive]
pub enum ScanlineStatus {
    /// The session is ready to read converted output rows.
    Ready,
    /// More compressed input is required to continue.
    NeedMoreInput,
    /// All converted rows have been consumed and the image is finished.
    Complete
}

/// Result of reading or skipping converted output rows.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[non_exhaustive]
pub enum ScanlineReadStatus {
    /// The requested operation committed `rows` output rows.
    RowsProcessed {
        /// Number of rows read into caller storage or skipped.
        rows: usize
    },
    /// No row was committed because more compressed input is required.
    NeedMoreInput,
    /// No output rows remain.
    Complete
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum RawPullPhase {
    Start,
    StreamingBaseline,
    BufferedRows,
    Complete
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum RawPullOwner {
    None,
    Pull,
    WholeImage
}

struct RawPullState {
    owner:                    RawPullOwner,
    phase:                    RawPullPhase,
    next_stripe:              usize,
    entropy_tables:           Option<EntropyTables>,
    layout:                   Option<[PlaneInfo; MAX_COMPONENTS]>,
    rows_per_stripe:          [usize; MAX_COMPONENTS],
    buffered_source_complete: bool
}

struct RawPullTargets {
    lengths: [usize; MAX_COMPONENTS],
    strides: [usize; MAX_COMPONENTS],
    widths:  [usize; MAX_COMPONENTS],
    heights: [usize; MAX_COMPONENTS]
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum ScanlinePhase {
    Start,
    StreamingBaseline,
    BufferedRows,
    Complete
}

struct ScanlineState {
    phase:           ScanlinePhase,
    started:         bool,
    output_scanline: usize,
    next_mcu_stripe: usize,
    staged_row:      usize,
    staged_rows:     usize,
    staging:         Vec<u8>,
    source_complete: bool,
    entropy_tables:  Option<EntropyTables>
}

impl Default for ScanlineState {
    fn default() -> Self {
        Self {
            phase:           ScanlinePhase::Start,
            started:         false,
            output_scanline: 0,
            next_mcu_stripe: 0,
            staged_row:      0,
            staged_rows:     0,
            staging:         Vec::new(),
            source_complete: false,
            entropy_tables:  None
        }
    }
}

impl Default for RawPullState {
    fn default() -> Self {
        Self {
            owner:                    RawPullOwner::None,
            phase:                    RawPullPhase::Start,
            next_stripe:              0,
            entropy_tables:           None,
            layout:                   None,
            rows_per_stripe:          [0; MAX_COMPONENTS],
            buffered_source_complete: false
        }
    }
}

/// Exclusive borrowing session for whole-image raw component output.
///
/// Create a session with [`JpegDecoder::raw_output`] after decoding headers.
/// While the session exists, its exclusive borrow prevents switching to pixel
/// output or changing decoder options during a raw retry sequence. Entropy,
/// MCU, checkpoint, and replay state remain owned by the borrowed decoder.
///
/// Unlike [`JpegDecoder::decode`] and [`JpegDecoder::decode_into`], raw output
/// skips upsampling and color conversion and returns one post-IDCT plane per
/// JPEG component. The configured output colorspace is therefore ignored.
pub struct RawDecodeSession<'decoder, T> {
    decoder: &'decoder mut JpegDecoder<T>,
    previous_incremental_mode: bool,
}

/// Stateful converted scanline output session.
///
/// Obtain a session with [`JpegDecoder::scanline_output`], call [`Self::start`],
/// then read or skip rows sequentially before finishing. The session uses the
/// decoder's configured output colorspace and the same upsampling and color
/// conversion pipeline as [`JpegDecoder::decode_into`].
///
/// Progressive and multi-SOS images do not expose provisional preview rows
/// through this API. They return [`ScanlineReadStatus::NeedMoreInput`] until
/// all coefficients needed for stable final rows are available. Progressive
/// preview reporting on [`JpegDecoder`] remains a separate full-frame API.
/// Output dimensions always match the JPEG frame dimensions. This API does not
/// provide libjpeg's scaled-IDCT or horizontal crop operations; callers must
/// perform scaling or cropping after row conversion.
pub struct ScanlineDecodeSession<'decoder, T> {
    decoder: &'decoder mut JpegDecoder<T>,
    previous_incremental_mode: bool,
}

impl<T> Drop for RawDecodeSession<'_, T> {
    fn drop(&mut self) {
        self.decoder.incremental_mode = self.previous_incremental_mode;
    }
}

impl<T> Drop for ScanlineDecodeSession<'_, T> {
    fn drop(&mut self) {
        self.decoder.incremental_mode = self.previous_incremental_mode;
    }
}

impl<T> ScanlineDecodeSession<'_, T>
where
    T: ZByteReaderTrait
{
    /// Parse enough input to make converted output geometry available.
    ///
    /// A suspended start does not advance output and can be retried on the
    /// same session after exposing more input through the underlying reader.
    pub fn start(&mut self) -> Result<ScanlineStatus, DecodeErrors> {
        if self.decoder.scanline_state.phase == ScanlinePhase::Complete {
            return Ok(ScanlineStatus::Complete);
        }
        if self.decoder.scanline_state.started {
            return Ok(ScanlineStatus::Ready);
        }
        match self.decoder.decode_headers() {
            Ok(()) => {
                if self.decoder.expects_dnl {
                    return Err(DecodeErrors::FormatStatic(
                        "converted scanline output does not support DNL images"
                    ));
                }
                self.decoder.scanline_state.started = true;
                Ok(ScanlineStatus::Ready)
            }
            Err(error) if error.is_recoverable_eof() => Ok(ScanlineStatus::NeedMoreInput),
            Err(error) => Err(error)
        }
    }

    /// Number of converted rows already returned or skipped.
    #[must_use]
    pub const fn output_scanline(&self) -> usize {
        self.decoder.scanline_state.output_scanline
    }

    /// Logical converted bytes in one output row.
    #[must_use]
    pub fn output_row_bytes(&self) -> Option<usize> {
        if !self.decoder.scanline_state.started {
            return None;
        }
        usize::from(self.decoder.width()).checked_mul(
            self.decoder
                .options
                .jpeg_get_out_colorspace()
                .num_components()
        )
    }

    /// Logical number of converted output rows, when known from the frame header.
    #[must_use]
    pub fn output_height(&self) -> Option<usize> {
        if !self.decoder.scanline_state.started || self.decoder.expects_dnl {
            return None;
        }
        Some(usize::from(self.decoder.height()))
    }

    /// Read as many converted rows as fit in `output` using `stride` bytes per row.
    ///
    /// Stride padding is left untouched. An empty output slice is a successful
    /// zero-row operation. If input suspends after some staged rows were copied,
    /// those stable rows are returned first and suspension is reported on a
    /// later call. `Complete` means no logical rows remain; call [`Self::finish`]
    /// to complete the compressed stream.
    pub fn read_scanlines(
        &mut self, output: &mut [u8], stride: usize
    ) -> Result<ScanlineReadStatus, DecodeErrors> {
        if !self.decoder.scanline_state.started {
            return Err(DecodeErrors::FormatStatic(
                "scanline output must be started before reading rows"
            ));
        }
        let row_bytes = self.output_row_bytes().ok_or(DecodeErrors::FormatStatic(
            "converted output row size overflow"
        ))?;
        if output.is_empty() {
            return Ok(ScanlineReadStatus::RowsProcessed { rows: 0 });
        }
        if stride < row_bytes {
            return Err(DecodeErrors::Format(format!(
                "scanline stride {stride} is smaller than row width {row_bytes}"
            )));
        }
        if output.len() < stride {
            return Err(DecodeErrors::TooSmallOutput(stride, output.len()));
        }
        let capacity = output.len() / stride;
        let height = usize::from(self.decoder.height());
        if self.decoder.scanline_state.output_scanline >= height {
            return Ok(ScanlineReadStatus::Complete);
        }

        let mut rows = 0;
        while rows < capacity && self.decoder.scanline_state.output_scanline < height {
            if self.decoder.scanline_state.staged_row == self.decoder.scanline_state.staged_rows {
                if rows != 0 {
                    break;
                }
                let remaining_capacity = capacity - rows;
                let maximum_stripe_rows = self.max_scanline_stripe_rows()?;
                let remaining_rows = height - self.decoder.scanline_state.output_scanline;
                if remaining_capacity >= maximum_stripe_rows
                    && remaining_rows >= maximum_stripe_rows
                {
                    let destination_start = rows * stride;
                    match self.decode_scanline_stripe(
                        &mut output[destination_start..capacity * stride],
                        stride
                    )? {
                        ScanlineReadStatus::RowsProcessed { rows: direct_rows }
                            if direct_rows != 0 =>
                        {
                            self.decoder.scanline_state.output_scanline += direct_rows;
                            rows += direct_rows;
                            break;
                        }
                        ScanlineReadStatus::NeedMoreInput => {
                            return Ok(ScanlineReadStatus::NeedMoreInput)
                        }
                        ScanlineReadStatus::RowsProcessed { .. } | ScanlineReadStatus::Complete => {
                            break
                        }
                    }
                }
                match self.produce_scanline_stripe()? {
                    ScanlineReadStatus::RowsProcessed { .. } => {}
                    ScanlineReadStatus::NeedMoreInput => {
                        return Ok(ScanlineReadStatus::NeedMoreInput)
                    }
                    ScanlineReadStatus::Complete => break
                }
            }
            if self.decoder.scanline_state.staged_row == self.decoder.scanline_state.staged_rows {
                break;
            }

            let source_start = self.decoder.scanline_state.staged_row * row_bytes;
            let destination_start = rows * stride;
            output[destination_start..destination_start + row_bytes].copy_from_slice(
                &self.decoder.scanline_state.staging[source_start..source_start + row_bytes]
            );
            self.decoder.scanline_state.staged_row += 1;
            self.decoder.scanline_state.output_scanline += 1;
            rows += 1;
        }

        if rows != 0 {
            Ok(ScanlineReadStatus::RowsProcessed { rows })
        } else if self.decoder.scanline_state.output_scanline >= height {
            Ok(ScanlineReadStatus::Complete)
        } else {
            Ok(ScanlineReadStatus::NeedMoreInput)
        }
    }

    /// Skip up to `rows` converted output rows.
    ///
    /// Skipped rows advance [`Self::output_scanline`] exactly like rows returned
    /// by [`Self::read_scanlines`]. A zero-row skip is a successful no-op.
    pub fn skip_scanlines(&mut self, rows: usize) -> Result<ScanlineReadStatus, DecodeErrors> {
        if !self.decoder.scanline_state.started {
            return Err(DecodeErrors::FormatStatic(
                "scanline output must be started before skipping rows"
            ));
        }
        if rows == 0 {
            return Ok(ScanlineReadStatus::RowsProcessed { rows: 0 });
        }
        let height = usize::from(self.decoder.height());
        if self.decoder.scanline_state.output_scanline >= height {
            return Ok(ScanlineReadStatus::Complete);
        }

        let target = rows.min(height - self.decoder.scanline_state.output_scanline);
        let mut skipped = 0;
        while skipped < target {
            if self.decoder.scanline_state.staged_row == self.decoder.scanline_state.staged_rows {
                if skipped != 0 {
                    break;
                }
                match self.produce_scanline_stripe()? {
                    ScanlineReadStatus::RowsProcessed { .. } => {}
                    ScanlineReadStatus::NeedMoreInput => {
                        return Ok(ScanlineReadStatus::NeedMoreInput)
                    }
                    ScanlineReadStatus::Complete => break
                }
            }
            let available = self
                .decoder
                .scanline_state
                .staged_rows
                .saturating_sub(self.decoder.scanline_state.staged_row);
            if available == 0 {
                break;
            }
            let consume = available.min(target - skipped);
            self.decoder.scanline_state.staged_row += consume;
            self.decoder.scanline_state.output_scanline += consume;
            skipped += consume;
        }

        if skipped != 0 {
            Ok(ScanlineReadStatus::RowsProcessed { rows: skipped })
        } else if self.decoder.scanline_state.output_scanline >= height {
            Ok(ScanlineReadStatus::Complete)
        } else {
            Ok(ScanlineReadStatus::NeedMoreInput)
        }
    }

    /// Consume all unread output and finish the compressed image.
    ///
    /// Calling this before reading every row discards the unread converted rows
    /// while continuing entropy decode and marker processing to completion.
    /// `NeedMoreInput` leaves the session resumable. `Complete` is sticky for
    /// the current session.
    pub fn finish(&mut self) -> Result<ScanlineStatus, DecodeErrors> {
        if !self.decoder.scanline_state.started {
            return Err(DecodeErrors::FormatStatic(
                "scanline output must be started before finishing"
            ));
        }
        if self.decoder.scanline_state.phase == ScanlinePhase::Complete {
            return Ok(ScanlineStatus::Complete);
        }
        let height = usize::from(self.decoder.height());
        while self.decoder.scanline_state.output_scanline < height {
            match self.skip_scanlines(height - self.decoder.scanline_state.output_scanline)? {
                ScanlineReadStatus::RowsProcessed { .. } => {}
                ScanlineReadStatus::NeedMoreInput => return Ok(ScanlineStatus::NeedMoreInput),
                ScanlineReadStatus::Complete => break
            }
        }
        if !self.decoder.scanline_state.source_complete {
            match self.produce_scanline_stripe()? {
                ScanlineReadStatus::NeedMoreInput => return Ok(ScanlineStatus::NeedMoreInput),
                ScanlineReadStatus::RowsProcessed { .. } | ScanlineReadStatus::Complete => {}
            }
        }
        self.decoder.finish_output_source();
        self.decoder.scanline_state.phase = ScanlinePhase::Complete;
        Ok(ScanlineStatus::Complete)
    }

    fn produce_scanline_stripe(&mut self) -> Result<ScanlineReadStatus, DecodeErrors> {
        let row_bytes = self.output_row_bytes().ok_or(DecodeErrors::FormatStatic(
            "converted output row size overflow"
        ))?;
        let max_rows = self.max_scanline_stripe_rows()?;
        let staging_len = row_bytes
            .checked_mul(max_rows)
            .ok_or(DecodeErrors::FormatStatic(
                "converted scanline staging size overflow"
            ))?;
        let mut staging = core::mem::take(&mut self.decoder.scanline_state.staging);
        if staging.len() != staging_len {
            staging.resize(staging_len, 0);
        }
        let result = self.decode_scanline_stripe(&mut staging, row_bytes);
        self.decoder.scanline_state.staging = staging;
        let status = result?;
        if let ScanlineReadStatus::RowsProcessed { rows } = status {
            self.decoder.scanline_state.staged_row = 0;
            self.decoder.scanline_state.staged_rows = rows;
        }
        Ok(status)
    }

    fn max_scanline_stripe_rows(&self) -> Result<usize, DecodeErrors> {
        let max_v = self
            .decoder
            .components
            .iter()
            .map(|component| component.vertical_sample)
            .max()
            .unwrap_or(1);
        // Vertical interpolation can defer one sample group until the next
        // MCU stripe, which then emits that group plus its regular 8 groups.
        (DCT_BLOCK_SIZE + 1)
            .checked_mul(max_v)
            .ok_or(DecodeErrors::FormatStatic(
                "converted scanline staging height overflow"
            ))
    }

    fn decode_scanline_stripe(
        &mut self, pixels: &mut [u8], stride: usize
    ) -> Result<ScanlineReadStatus, DecodeErrors> {
        let prepared = if self.decoder.scanline_state.phase == ScanlinePhase::Start {
            self.decoder.prepare_for_scan_decode()?;
            let buffered = self.decoder.is_progressive
                || usize::from(self.decoder.num_scans) != self.decoder.components.len();
            self.decoder.scanline_state.phase = if buffered {
                ScanlinePhase::BufferedRows
            } else {
                ScanlinePhase::StreamingBaseline
            };
            true
        } else {
            false
        };
        let requested_stripe = self.decoder.scanline_state.next_mcu_stripe;
        let sink = ConvertedScanlineSink {
            pixels,
            stride,
            requested_stripe,
            rows_written: 0,
            source_complete: false
        };
        let mut output = McuDecodeOutput::Scanlines(sink);

        let decode_result = match self.decoder.scanline_state.phase {
            ScanlinePhase::StreamingBaseline => {
                if !prepared {
                    self.decoder.prepare_for_scan_decode()?;
                }
                #[cfg(feature = "arith")]
                self.restore_arithmetic_context()?;
                self.decoder.decode_mcu_output(&mut output)
            }
            ScanlinePhase::BufferedRows if self.decoder.scanline_state.source_complete => {
                self.decoder.render_buffered_output_stripe(&mut output)
            }
            ScanlinePhase::BufferedRows => {
                if !prepared {
                    self.decoder.prepare_for_scan_decode()?;
                }
                self.decoder.decode_mcu_output(&mut output)
            }
            ScanlinePhase::Start | ScanlinePhase::Complete => unreachable!()
        };

        let (rows_written, source_complete) = match output {
            McuDecodeOutput::Scanlines(sink) => (sink.rows_written, sink.source_complete),
            McuDecodeOutput::Pixels(_) | McuDecodeOutput::RawPlanes(_) => unreachable!()
        };
        if let Err(error) = decode_result {
            if error.is_recoverable_eof() {
                return Ok(ScanlineReadStatus::NeedMoreInput);
            }
            return Err(error);
        }

        let remaining = usize::from(self.decoder.height())
            .saturating_sub(self.decoder.scanline_state.output_scanline);
        let rows_written = rows_written.min(remaining);
        self.decoder.scanline_state.next_mcu_stripe += 1;
        self.decoder.scanline_state.source_complete |= source_complete;
        if self.decoder.scanline_state.phase == ScanlinePhase::StreamingBaseline
            && self.decoder.is_arithmetic
        {
            self.decoder.scanline_state.entropy_tables = Some(self.decoder.entropy_tables.clone());
        }
        Ok(ScanlineReadStatus::RowsProcessed { rows: rows_written })
    }

    #[cfg(feature = "arith")]
    fn restore_arithmetic_context(&mut self) -> Result<(), DecodeErrors> {
        if !self.decoder.is_arithmetic {
            return Ok(());
        }
        let resumes_completed_row = match self.decoder.scan_checkpoint() {
            Some(checkpoint) => match checkpoint.bitstream_state {
                BitstreamStateSnapshot::Arithmetic(_) => true,
                BitstreamStateSnapshot::Huffman(_) | BitstreamStateSnapshot::None => false
            },
            None => false
        };
        if resumes_completed_row {
            let tables = self.decoder.scanline_state.entropy_tables.as_ref().ok_or(
                DecodeErrors::FormatStatic(
                    "missing arithmetic contexts for converted scanline resume"
                )
            )?;
            self.decoder.entropy_tables = tables.clone();
        }
        Ok(())
    }
}

impl<T> RawDecodeSession<'_, T>
where
    T: ZByteReaderTrait
{
    /// Number of components in SOF declaration order.
    ///
    /// Returns `None` when headers have not been decoded yet.
    #[must_use]
    pub fn num_components(&self) -> Option<usize> {
        self.decoder.raw_num_components()
    }

    /// Component selector bytes from the SOF marker, in declaration order.
    ///
    /// Common YCbCr files usually use `[1, 2, 3]`, while RGB files may use
    /// `[b'R', b'G', b'B']`. Selectors are returned exactly as encoded so
    /// callers can identify nonstandard component layouts.
    ///
    /// Returns `None` when headers have not been decoded yet or contain no
    /// components.
    #[must_use]
    pub fn component_ids(&self) -> Option<Vec<u8>> {
        self.decoder.raw_component_ids()
    }

    /// Per-component raw plane geometry and sampling metadata.
    ///
    /// Indices `0..num_components()` are populated in SOF declaration order;
    /// trailing entries are [`PlaneInfo::default`]. The sampling-factor fields
    /// preserve the exact SOF values, so callers can identify subsampling
    /// without inferring it from rounded dimensions.
    ///
    /// ```text
    /// width            = ceil(image_width  * h_samp / h_max)
    /// height           = ceil(image_height * v_samp / v_max)
    /// stride           = ceil(width  / 8) * 8
    /// allocated_height = ceil(height / 8) * 8
    /// byte_size        = stride * allocated_height
    /// ```
    ///
    /// Returns `None` when headers have not been decoded or layout arithmetic
    /// overflows `usize`.
    #[must_use]
    pub fn layout(&self) -> Option<[PlaneInfo; MAX_COMPONENTS]> {
        self.decoder
            .raw_pull_state
            .layout
            .or_else(|| self.decoder.raw_planar_layout())
    }

    /// Decode the next sequential raw iMCU row into caller-provided planes.
    ///
    /// Interleaved baseline images decode and deliver one iMCU row at a time.
    /// Progressive and multi-SOS images must first buffer all coefficient data
    /// needed for final samples; their rows are still pulled sequentially, but
    /// compressed input consumption is not row-streaming.
    ///
    /// Each stride must be at least the corresponding logical plane width. For
    /// a component that will return `rows` rows, its plane must contain at least
    /// `(rows - 1) * stride + width` bytes, or zero bytes when `rows == 0`.
    /// Padding is addressable only between logical rows; no padding is required
    /// after the final row. The returned `rows_written` count identifies the
    /// meaningful prefix; the final iMCU row is clipped to the logical component
    /// height. Components that exhaust their logical height before other
    /// components report zero rows on later calls.
    ///
    /// A recoverable input suspension returns [`RawImcuRowStatus::NeedMoreInput`]
    /// and leaves caller planes untouched. Expose more input through the same
    /// decoder stream, then retry with the same session and plane layout. The
    /// caller cannot select a stripe index.
    ///
    /// Images whose SOF height is zero and whose line count is supplied later
    /// by a DNL marker are currently rejected because the caller-visible plane
    /// geometry is not known when the pull sequence begins.
    ///
    /// # Example
    /// ```no_run
    /// use zune_core::bytestream::ZCursor;
    /// use zune_jpeg::{JpegDecoder, RawImcuRowStatus};
    ///
    /// let data = std::fs::read("photo.jpg").unwrap();
    /// let mut decoder = JpegDecoder::new(ZCursor::new(&data));
    /// decoder.decode_headers().unwrap();
    /// let mut raw = decoder.raw_output();
    /// let layout = raw.layout().unwrap();
    /// let count = raw.num_components().unwrap();
    /// let strides: Vec<usize> = layout[..count]
    ///     .iter()
    ///     .map(|plane| plane.width)
    ///     .collect();
    ///
    /// loop {
    ///     let mut storage: Vec<Vec<u8>> = layout[..count]
    ///         .iter()
    ///         .map(|plane| vec![0; plane.width * plane.vertical_sampling_factor * 8])
    ///         .collect();
    ///     let mut planes: Vec<&mut [u8]> =
    ///         storage.iter_mut().map(Vec::as_mut_slice).collect();
    ///     match raw.decode_next_imcu_row(&mut planes, &strides).unwrap() {
    ///         RawImcuRowStatus::RowReady { rows_written } => {
    ///             // Consume `rows_written[index]` rows from each plane.
    ///             let _ = rows_written;
    ///         }
    ///         RawImcuRowStatus::NeedMoreInput => break,
    ///         RawImcuRowStatus::Complete => break,
    ///         _ => unreachable!()
    ///     }
    /// }
    /// ```
    ///
    /// # Errors
    /// Returns [`DecodeErrors::Format`] for invalid plane counts or strides,
    /// [`DecodeErrors::TooSmallOutput`] for an undersized plane, or a
    /// non-recoverable decode error.
    pub fn decode_next_imcu_row(
        &mut self, planes: &mut [&mut [u8]], strides: &[usize]
    ) -> Result<RawImcuRowStatus, DecodeErrors> {
        self.decode_next_imcu_row_for(RawPullOwner::Pull, planes, strides)
    }

    fn decode_next_imcu_row_for(
        &mut self, owner: RawPullOwner, planes: &mut [&mut [u8]], strides: &[usize]
    ) -> Result<RawImcuRowStatus, DecodeErrors> {
        if self.decoder.raw_pull_state.phase == RawPullPhase::Complete {
            return Ok(RawImcuRowStatus::Complete);
        }

        let layout = self.pull_layout()?;
        let n = self.decoder.components.len();
        if self.decoder.expects_dnl {
            return Err(DecodeErrors::FormatStatic(
                "raw output does not support DNL images"
            ));
        }
        if planes.len() != n || strides.len() != n {
            return Err(DecodeErrors::Format(format!(
                "raw iMCU-row output expected {n} plane(s) and stride(s), got {} plane(s) and {} stride(s)",
                planes.len(),
                strides.len()
            )));
        }

        let total_stripes = self.decoder.raw_imcu_row_count(&layout, n);
        if self.decoder.raw_pull_state.next_stripe >= total_stripes {
            self.decoder.finish_output_source();
            self.decoder.raw_pull_state.phase = RawPullPhase::Complete;
            return Ok(RawImcuRowStatus::Complete);
        }

        let targets = self.pull_targets(planes, strides, &layout, n)?;
        match self.decoder.raw_pull_state.owner {
            RawPullOwner::None => self.decoder.raw_pull_state.owner = owner,
            active if active == owner => {}
            _ => {
                return Err(DecodeErrors::FormatStatic(
                    "cannot switch raw output API during an active decode sequence"
                ))
            }
        }
        let requested_stripe = self.decoder.raw_pull_state.next_stripe;
        let raw_planes = RawPlanesSink {
            planes,
            lengths: targets.lengths,
            target_strides: targets.strides,
            target_widths: targets.widths,
            target_heights: targets.heights,
            n_components: n,
            requested_stripe: Some(requested_stripe),
            rows_written: [0; MAX_COMPONENTS],
            stripe_ready: false,
            source_complete: false
        };
        let mut output = McuDecodeOutput::RawPlanes(raw_planes);

        if let Err(error) = self.decode_pull_phase(&mut output, &layout, n) {
            if error.is_recoverable_eof() {
                return Ok(RawImcuRowStatus::NeedMoreInput);
            }
            return Err(error);
        }

        let sink = match output {
            McuDecodeOutput::RawPlanes(sink) => sink,
            McuDecodeOutput::Pixels(_) | McuDecodeOutput::Scanlines(_) => unreachable!()
        };
        if sink.source_complete {
            self.decoder.raw_pull_state.buffered_source_complete = true;
            self.decoder.finish_output_source();
        }
        if !sink.stripe_ready {
            self.decoder.raw_pull_state.phase = RawPullPhase::Complete;
            return Ok(RawImcuRowStatus::Complete);
        }

        self.decoder.raw_pull_state.next_stripe += 1;
        for (index, rows) in sink.rows_written.iter().copied().enumerate().take(n) {
            if rows != 0 && self.decoder.raw_pull_state.rows_per_stripe[index] == 0 {
                self.decoder.raw_pull_state.rows_per_stripe[index] = rows;
            }
        }
        if self.decoder.raw_pull_state.phase == RawPullPhase::StreamingBaseline
            && self.decoder.is_arithmetic
        {
            self.decoder.raw_pull_state.entropy_tables = Some(self.decoder.entropy_tables.clone());
        }
        Ok(RawImcuRowStatus::RowReady {
            rows_written: sink.rows_written
        })
    }

    fn pull_layout(&mut self) -> Result<[PlaneInfo; MAX_COMPONENTS], DecodeErrors> {
        if let Some(layout) = self.decoder.raw_pull_state.layout {
            return Ok(layout);
        }
        let layout = self
            .decoder
            .raw_planar_layout()
            .ok_or(DecodeErrors::FormatStatic(
                "raw layout unavailable before headers are decoded"
            ))?;
        self.decoder.raw_pull_state.layout = Some(layout);
        Ok(layout)
    }

    fn pull_targets(
        &self, planes: &[&mut [u8]], strides: &[usize], layout: &[PlaneInfo; MAX_COMPONENTS],
        n: usize
    ) -> Result<RawPullTargets, DecodeErrors> {
        let mut targets = RawPullTargets {
            lengths: [0; MAX_COMPONENTS],
            strides: [0; MAX_COMPONENTS],
            widths:  [0; MAX_COMPONENTS],
            heights: [0; MAX_COMPONENTS]
        };
        for i in 0..n {
            if strides[i] < layout[i].width {
                return Err(DecodeErrors::Format(format!(
                    "stride[{i}] = {} is smaller than logical width {}",
                    strides[i], layout[i].width
                )));
            }
            let configured_rows = self.decoder.raw_pull_state.rows_per_stripe[i];
            let stripe_rows = if configured_rows == 0 {
                layout[i].vertical_sampling_factor * DCT_BLOCK_SIZE
            } else {
                configured_rows
            };
            let row_start = self.decoder.raw_pull_state.next_stripe * stripe_rows;
            let rows = layout[i].height.saturating_sub(row_start).min(stripe_rows);
            let need = if rows == 0 {
                0
            } else {
                (rows - 1)
                    .checked_mul(strides[i])
                    .and_then(|offset| offset.checked_add(layout[i].width))
                    .ok_or(DecodeErrors::FormatStatic(
                        "raw iMCU-row plane size overflow",
                    ))?
            };
            if planes[i].len() < need {
                return Err(DecodeErrors::TooSmallOutput(need, planes[i].len()));
            }
            targets.lengths[i] = planes[i].len();
            targets.strides[i] = strides[i];
            targets.widths[i] = layout[i].width;
            targets.heights[i] = rows;
        }
        Ok(targets)
    }

    fn decode_pull_phase(
        &mut self, output: &mut McuDecodeOutput<'_, '_>, layout: &[PlaneInfo; MAX_COMPONENTS],
        n: usize
    ) -> Result<(), DecodeErrors> {
        match self.decoder.raw_pull_state.phase {
            RawPullPhase::Start => self.start_pull(output, layout, n),
            RawPullPhase::StreamingBaseline => {
                self.prepare_pull_scan()?;
                #[cfg(feature = "arith")]
                self.restore_arithmetic_pull_context()?;
                self.decoder.decode_mcu_output(output)
            }
            RawPullPhase::BufferedRows => {
                if self.decoder.raw_pull_state.buffered_source_complete {
                    self.decoder.render_buffered_output_stripe(output)
                } else {
                    self.prepare_pull_scan()?;
                    self.decode_buffered_source_and_render(output, layout, n)
                }
            }
            RawPullPhase::Complete => unreachable!()
        }
    }

    fn start_pull(
        &mut self, output: &mut McuDecodeOutput<'_, '_>, layout: &[PlaneInfo; MAX_COMPONENTS],
        n: usize
    ) -> Result<(), DecodeErrors> {
        self.prepare_pull_scan()?;
        let buffered = self.decoder.is_progressive
            || usize::from(self.decoder.num_scans) != self.decoder.components.len();
        self.decoder.raw_pull_state.phase = if buffered {
            RawPullPhase::BufferedRows
        } else {
            RawPullPhase::StreamingBaseline
        };
        if buffered {
            self.decode_buffered_source_and_render(output, layout, n)
        } else {
            self.decoder.decode_mcu_output(output)
        }
    }

    fn prepare_pull_scan(&mut self) -> Result<(), DecodeErrors> {
        self.decoder.prepare_for_scan_decode()?;
        self.decoder.mcu_checkpoints_enabled = true;
        Ok(())
    }

    fn decode_buffered_source_and_render(
        &mut self, output: &mut McuDecodeOutput<'_, '_>, layout: &[PlaneInfo; MAX_COMPONENTS],
        n: usize
    ) -> Result<(), DecodeErrors> {
        let stripe = self.decoder.raw_pull_state.next_stripe;
        self.decoder.decode_buffered_raw_source(stripe, layout, n)?;
        self.decoder.raw_pull_state.buffered_source_complete = true;
        self.decoder.render_buffered_output_stripe(output)
    }

    #[cfg(feature = "arith")]
    fn restore_arithmetic_pull_context(&mut self) -> Result<(), DecodeErrors> {
        if !self.decoder.is_arithmetic {
            return Ok(());
        }
        let resumes_completed_row = match self.decoder.scan_checkpoint() {
            Some(checkpoint) => matches!(
                checkpoint.bitstream_state,
                BitstreamStateSnapshot::Arithmetic(_)
            ),
            None => false
        };
        if resumes_completed_row {
            let tables = self.decoder.raw_pull_state.entropy_tables.as_ref().ok_or(
                DecodeErrors::FormatStatic("missing arithmetic contexts for raw iMCU-row resume")
            )?;
            self.decoder.entropy_tables = tables.clone();
        }
        Ok(())
    }

    /// Decode the complete image into DCT-block-padded component planes.
    /// This whole-image convenience method is not operationally
    /// equivalent to one call to `jpeg_read_raw_data`, which returns one iMCU
    /// row at a time.
    ///
    /// Supply one mutable plane per component in SOF declaration order. Each
    /// plane must contain at least the corresponding [`PlaneInfo::byte_size`]
    /// bytes from [`Self::layout`]. The configured output colorspace is
    /// ignored. Samples within each plane's logical `width * height` area are
    /// meaningful; trailing DCT padding is implementation-defined.
    ///
    /// On a recoverable EOF, call this method again on the same session after
    /// exposing more input. Successful calls can also be replayed and produce
    /// bit-identical planes.
    ///
    /// # Example
    /// ```no_run
    /// use zune_core::bytestream::ZCursor;
    /// use zune_jpeg::JpegDecoder;
    ///
    /// let data = std::fs::read("photo.jpg").unwrap();
    /// let mut decoder = JpegDecoder::new(ZCursor::new(&data));
    /// decoder.decode_headers().unwrap();
    /// let mut raw = decoder.raw_output();
    /// let layout = raw.layout().unwrap();
    /// let count = raw.num_components().unwrap();
    /// let mut buffers: Vec<Vec<u8>> = (0..count)
    ///     .map(|index| vec![0; layout[index].byte_size])
    ///     .collect();
    /// let mut planes: Vec<&mut [u8]> =
    ///     buffers.iter_mut().map(Vec::as_mut_slice).collect();
    /// raw.decode_into_planes(&mut planes).unwrap();
    /// ```
    ///
    /// # Errors
    /// Returns [`DecodeErrors::TooSmallOutput`] for an undersized plane,
    /// [`DecodeErrors::Format`] for the wrong plane count, or an error from
    /// the underlying decode pipeline.
    pub fn decode_into_planes(&mut self, planes: &mut [&mut [u8]]) -> Result<(), DecodeErrors> {
        if self.decoder.expects_dnl {
            return Err(DecodeErrors::FormatStatic(
                "raw output does not support DNL images"
            ));
        }
        if self.decoder.raw_pull_state.phase == RawPullPhase::Complete {
            self.decoder.raw_pull_state = RawPullState::default();
        }
        let layout = self.layout().ok_or(DecodeErrors::FormatStatic(
            "raw layout unavailable before headers are decoded"
        ))?;
        let count = self.num_components().ok_or(DecodeErrors::FormatStatic(
            "raw components unavailable before headers are decoded"
        ))?;
        if planes.len() != count {
            return Err(DecodeErrors::Format(format!(
                "RawDecodeSession::decode_into_planes expected {count} plane buffer(s), got {}",
                planes.len()
            )));
        }
        for (index, plane) in planes.iter().enumerate() {
            if plane.len() < layout[index].byte_size {
                return Err(DecodeErrors::TooSmallOutput(
                    layout[index].byte_size,
                    plane.len()
                ));
            }
        }
        let strides: Vec<usize> = layout[..count].iter().map(|plane| plane.stride).collect();
        self.decode_whole_with_strides(planes, &strides, &layout)
    }

    /// Decode the complete image into component planes with caller row strides.
    ///
    /// Logical plane contents and geometry are compatible with libjpeg-turbo
    /// raw output. This whole-image convenience method is not operationally
    /// equivalent to one call to `jpeg_read_raw_data`, which returns one iMCU
    /// row at a time. Only each plane's logical `width * height` area is
    /// written; stride padding is left untouched.
    ///
    /// Supply one mutable plane and stride per component in SOF declaration
    /// order. Each stride must be at least the corresponding logical
    /// [`PlaneInfo::width`], and each plane must contain at least
    /// `stride * PlaneInfo::height` bytes.
    ///
    /// On a recoverable EOF, call this method again on the same session after
    /// exposing more input. Successful calls can also be replayed and produce
    /// bit-identical logical samples.
    ///
    /// # Example
    /// ```no_run
    /// use zune_core::bytestream::ZCursor;
    /// use zune_jpeg::JpegDecoder;
    ///
    /// let data = std::fs::read("photo.jpg").unwrap();
    /// let mut decoder = JpegDecoder::new(ZCursor::new(&data));
    /// decoder.decode_headers().unwrap();
    /// let mut raw = decoder.raw_output();
    /// let layout = raw.layout().unwrap();
    /// let count = raw.num_components().unwrap();
    /// let strides: Vec<usize> = (0..count)
    ///     .map(|index| layout[index].width.div_ceil(64) * 64)
    ///     .collect();
    /// let mut buffers: Vec<Vec<u8>> = (0..count)
    ///     .map(|index| vec![0; strides[index] * layout[index].height])
    ///     .collect();
    /// let mut planes: Vec<&mut [u8]> =
    ///     buffers.iter_mut().map(Vec::as_mut_slice).collect();
    /// raw.decode_into_planes_strided(&mut planes, &strides).unwrap();
    /// ```
    ///
    /// # Errors
    /// Returns [`DecodeErrors::TooSmallOutput`] for an undersized plane,
    /// [`DecodeErrors::Format`] for invalid plane counts or strides, or an
    /// error from the underlying decode pipeline.
    pub fn decode_into_planes_strided(
        &mut self, planes: &mut [&mut [u8]], strides: &[usize]
    ) -> Result<(), DecodeErrors> {
        if self.decoder.expects_dnl {
            return Err(DecodeErrors::FormatStatic(
                "raw output does not support DNL images"
            ));
        }
        if self.decoder.raw_pull_state.phase == RawPullPhase::Complete {
            self.decoder.raw_pull_state = RawPullState::default();
        }
        let layout = self.layout().ok_or(DecodeErrors::FormatStatic(
            "raw layout unavailable before headers are decoded"
        ))?;
        let count = self.num_components().ok_or(DecodeErrors::FormatStatic(
            "raw components unavailable before headers are decoded"
        ))?;
        if planes.len() != count || strides.len() != count {
            return Err(DecodeErrors::Format(format!(
                "RawDecodeSession::decode_into_planes_strided expected {count} plane(s) and stride(s), got {} plane(s) and {} stride(s)",
                planes.len(),
                strides.len()
            )));
        }
        for index in 0..count {
            if strides[index] < layout[index].width {
                return Err(DecodeErrors::Format(format!(
                    "stride[{index}] = {} is smaller than logical width {}",
                    strides[index], layout[index].width
                )));
            }
            let need = strides[index]
                .checked_mul(layout[index].height)
                .ok_or(DecodeErrors::FormatStatic("plane size overflow"))?;
            if planes[index].len() < need {
                return Err(DecodeErrors::TooSmallOutput(need, planes[index].len()));
            }
        }
        self.decode_whole_with_strides(planes, strides, &layout)
    }

    fn decode_whole_with_strides(
        &mut self, planes: &mut [&mut [u8]], strides: &[usize],
        layout: &[PlaneInfo; MAX_COMPONENTS]
    ) -> Result<(), DecodeErrors> {
        let count = planes.len();
        loop {
            let stripe = self.decoder.raw_pull_state.next_stripe;
            if self.decoder.raw_pull_state.phase == RawPullPhase::Complete {
                self.decoder.finish_output_source();
                return Ok(());
            }
            let mut stripe_planes: Vec<&mut [u8]> = planes
                .iter_mut()
                .enumerate()
                .map(|(index, plane)| {
                    let configured_rows = self.decoder.raw_pull_state.rows_per_stripe[index];
                    let stripe_rows = if configured_rows == 0 {
                        layout[index].vertical_sampling_factor * DCT_BLOCK_SIZE
                    } else {
                        configured_rows
                    };
                    let row_start = stripe * stripe_rows;
                    let rows = layout[index]
                        .height
                        .saturating_sub(row_start)
                        .min(stripe_rows);
                    let start = (row_start * strides[index]).min(plane.len());
                    let end = start + rows * strides[index];
                    &mut plane[start..end]
                })
                .collect();

            match self.decode_next_imcu_row_for(
                RawPullOwner::WholeImage,
                &mut stripe_planes,
                &strides[..count]
            )? {
                RawImcuRowStatus::RowReady { .. } => {}
                RawImcuRowStatus::NeedMoreInput => return Err(DecodeErrors::ExhaustedData),
                RawImcuRowStatus::Complete => return Ok(())
            }
        }
    }
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
        usize::try_from(position)
            .map_err(|_| DecodeErrors::FormatStatic("Stream position does not fit in usize"))
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
            progressive_checkpoint: None,
            progressive_fine_checkpoint: None
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

    pub(crate) fn progressive_fine_checkpoint(&self) -> Option<&ProgressiveFineCheckpoint> {
        self.scan_state
            .as_deref()
            .and_then(|state| state.progressive_fine_checkpoint.as_deref())
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
            state.progressive_fine_checkpoint = None;
        }
        Ok(())
    }

    pub(crate) fn checkpoint_progressive_fine_scan(
        &mut self, mcu_row: usize, mcu_col: usize, bitstream_state: BitstreamStateSnapshot
    ) -> Result<(), DecodeErrors> {
        let stream_position = self.stream_position()?;
        let append_snapshot = HeaderAppendStateSnapshot::capture(self);
        let sos_snapshot = self.capture_sos_params();
        let dc_predictions = core::array::from_fn(|idx| {
            self.components
                .get(idx)
                .map_or((0, 0), |component| (component.dc_pred, component.dc_diff))
        });

        if let Some(state) = self.scan_state.as_mut() {
            state.progressive_fine_checkpoint = Some(Box::new(ProgressiveFineCheckpoint {
                stream_position,
                append_snapshot,
                sos_snapshot,
                completed_scans: self.progressive_completed_scans,
                displayed_scans: self.progressive_displayed_scans,
                mcu_row,
                mcu_col,
                todo: self.todo,
                dc_predictions,
                bitstream_state
            }));
        }
        Ok(())
    }

    pub(crate) fn invalidate_progressive_scan_checkpoint(&mut self) {
        if let Some(state) = self.scan_state.as_mut() {
            state.progressive_checkpoint = None;
            state.progressive_fine_checkpoint = None;
        }
    }

    pub(crate) fn invalidate_progressive_fine_checkpoint(&mut self) {
        if let Some(state) = self.scan_state.as_mut() {
            state.progressive_fine_checkpoint = None;
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
        dc_predictions: [(i32, i32); MAX_COMPONENTS], bitstream_state: BitstreamStateSnapshot
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

    // Refresh the conversion function selected by the configured output colorspace.
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
            info:                        ImageInfo::default(),
            qt_tables:                   [None, None, None, None],
            entropy_tables:              EntropyTables {
                dc_huffman:                              [None, None, None, None],
                ac_huffman:                              [None, None, None, None],
                #[cfg(feature = "arith")]
                dc_arithmetic:                           [
                    ArithDCTables::default(),
                    ArithDCTables::default(),
                    ArithDCTables::default(),
                    ArithDCTables::default()
                ],
                #[cfg(feature = "arith")]
                ac_arithmetic:                           [
                    ArithACTables::default(),
                    ArithACTables::default(),
                    ArithACTables::default(),
                    ArithACTables::default()
                ]
            },
            components:                  vec![],
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
            progressive_scan_buffer: core::array::from_fn(|_| Vec::new()),
            upsampler_scratch: Vec::new(),
            progressive_completed_scans: 0,
            progressive_displayed_scans: 0,
            progressive_render_incomplete: false,
            marker_body_scratch:         Vec::new(),
            expects_dnl:                 false,
            raw_pull_state:              RawPullState::default(),
            scanline_state:              ScanlineState::default()
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
        self.abort_raw_pull_sequence();
        self.decode_headers()?;
        self.ensure_supported_encoding()?;

        if self.expects_dnl {
            // Height is unknown until DNL is encountered during entropy
            // decoding. Pre-allocate a buffer large enough for the worst case
            // (the configured max height), run decode_into normally — the MCU
            // loop will intercept the DNL marker and set info.height — then
            // truncate to the actual decoded size.
            let max_size = self
                .options
                .max_height()
                .checked_mul(usize::from(self.info.width))
                .and_then(|v| {
                    v.checked_mul(self.options.jpeg_get_out_colorspace().num_components())
                })
                .ok_or(DecodeErrors::FormatStatic(
                    "DNL image dimensions overflow usize"
                ))?;
            let mut out = vec![0u8; max_size];
            self.decode_into(&mut out)?;
            // After decode_into, info.height has been set by the DNL handler.
            let actual_size = self.output_buffer_size().ok_or(DecodeErrors::FormatStatic(
                "DNL image: output size unavailable after decode"
            ))?;
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
    /// Incremental mode makes scan EOF recoverable in non-strict mode and
    /// records per-row checkpoints during the first scan decode attempt,
    /// allowing a later retry to resume from the latest stable row instead of
    /// replaying from scan start.
    ///
    /// It is disabled by default so one-shot decoding keeps the lowest
    /// overhead path.
    #[must_use]
    pub const fn incremental_mode(&self) -> bool {
        self.incremental_mode
    }

    /// Enable or disable incremental mode.
    ///
    /// Call this before the first `decode_into` scan attempt. When enabled,
    /// scan EOF is recoverable and decoding can be retried with more input.
    /// Otherwise, non-strict decoding returns best-effort output on scan EOF.
    /// Strict mode always treats scan EOF as an error. The default is `false`.
    pub fn set_incremental_mode(&mut self, enabled: bool) {
        self.incremental_mode = enabled;
    }

    /// Whether scan EOF must be returned as an error.
    /// Strict mode rejects truncated scans, while incremental mode uses the
    /// error to signal that the caller should provide more input.
    pub(crate) fn scan_eof_is_error(&self) -> bool {
        self.options.strict_mode() || self.incremental_mode
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

    /// Begin an exclusive raw component-output session.
    ///
    /// Decode headers first when the caller needs to query
    /// [`RawDecodeSession::layout`] or component metadata before allocating
    /// output planes. The session borrows this decoder mutably, preventing
    /// pixel output or option changes until the session is dropped.
    pub fn raw_output(&mut self) -> RawDecodeSession<'_, T> {
        self.abort_scanline_sequence();
        if self.raw_pull_state.phase == RawPullPhase::Complete {
            self.raw_pull_state = RawPullState::default();
        }
        if self.raw_pull_state.owner == RawPullOwner::None {
            self.clear_scan_checkpoints();
        }
        let previous_incremental_mode = self.incremental_mode;
        self.incremental_mode = true;
        RawDecodeSession {
            decoder: self,
            previous_incremental_mode,
        }
    }

    /// Begin an exclusive converted scanline output session.
    ///
    /// Call [`ScanlineDecodeSession::start`] before reading rows. Recreating a
    /// session after input suspension preserves decoder-owned scanline state.
    pub fn scanline_output(&mut self) -> ScanlineDecodeSession<'_, T> {
        self.abort_raw_pull_sequence();
        if self.scanline_state.phase == ScanlinePhase::Complete {
            self.abort_scanline_sequence();
        }
        if !self.scanline_state.started {
            self.clear_scan_checkpoints();
        }
        let previous_incremental_mode = self.incremental_mode;
        self.incremental_mode = true;
        ScanlineDecodeSession {
            decoder: self,
            previous_incremental_mode,
        }
    }

    #[cfg(test)]
    pub(crate) fn raw_buffer_capacities(
        &self
    ) -> ([usize; MAX_COMPONENTS], [usize; MAX_COMPONENTS]) {
        (
            core::array::from_fn(|index| {
                self.components
                    .get(index)
                    .map_or(0, |component| component.raw_coeff.capacity())
            }),
            core::array::from_fn(|index| self.progressive_mcus_buffer[index].capacity())
        )
    }

    /// Number of components present in the JPEG scan (1..=4).
    ///
    /// Valid only after [`decode_headers`](Self::decode_headers).
    ///
    /// # Returns
    /// - `Some(n)`: number of components in the input scan
    /// - `None`: headers have not been decoded yet
    #[must_use]
    fn raw_num_components(&self) -> Option<usize> {
        if self.headers_decoded {
            Some(self.components.len())
        } else {
            None
        }
    }

    /// Per-component plane geometry for raw planar output.
    ///
    /// Returns one [`PlaneInfo`] per component in declaration order
    /// (Y, Cb, Cr for YCbCr; Y for grayscale; C, M, Y, K for CMYK; etc.).
    /// Indices `0..num_components()` are populated; trailing entries are
    /// the default zero-sized [`PlaneInfo`].
    /// [`PlaneInfo::horizontal_sampling_factor`] and
    /// [`PlaneInfo::vertical_sampling_factor`] expose each component's exact
    /// SOF sampling factors.
    ///
    /// Plane dimensions are computed using the same rules as libjpeg-turbo's
    /// `jpeg_read_raw_data`:
    ///
    /// ```text
    /// comp_width       = ceil(image_width  * h_samp / h_max)
    /// comp_height      = ceil(image_height * v_samp / v_max)
    /// stride           = ceil(comp_width  / 8) * 8
    /// allocated_height = ceil(comp_height / 8) * 8
    /// byte_size        = stride * allocated_height
    /// ```
    ///
    /// Valid only after [`decode_headers`](Self::decode_headers).
    ///
    /// # Returns
    /// - `Some([PlaneInfo; MAX_COMPONENTS])`: per-component layout
    /// - `None`: headers have not been decoded yet, or layout overflows `usize`
    #[must_use]
    fn raw_planar_layout(&self) -> Option<[PlaneInfo; MAX_COMPONENTS]> {
        if !self.headers_decoded || self.components.is_empty() {
            return None;
        }
        let img_w = usize::from(self.width());
        let img_h = usize::from(self.height());
        // `self.h_max` / `self.v_max` aren't populated until `decode()` runs
        // `setup_component_params`, so derive the maxima directly from the
        // parsed component list (which is available right after
        // `decode_headers`).
        let mut h_max = 1usize;
        let mut v_max = 1usize;
        for comp in &self.components {
            if comp.horizontal_sample > h_max {
                h_max = comp.horizontal_sample;
            }
            if comp.vertical_sample > v_max {
                v_max = comp.vertical_sample;
            }
        }
        let mut out = [PlaneInfo::default(); MAX_COMPONENTS];
        for (slot, comp) in out.iter_mut().zip(self.components.iter()) {
            let comp_w = img_w.checked_mul(comp.horizontal_sample)?.div_ceil(h_max);
            let comp_h = img_h.checked_mul(comp.vertical_sample)?.div_ceil(v_max);
            let stride = round_up_pow2(comp_w, DCT_BLOCK_SIZE)?;
            let allocated_height = round_up_pow2(comp_h, DCT_BLOCK_SIZE)?;
            let byte_size = stride.checked_mul(allocated_height)?;
            *slot = PlaneInfo {
                horizontal_sampling_factor: comp.horizontal_sample,
                vertical_sampling_factor: comp.vertical_sample,
                width: comp_w,
                height: comp_h,
                stride,
                allocated_height,
                byte_size
            };
        }
        Some(out)
    }

    fn raw_imcu_row_count(&self, layout: &[PlaneInfo; MAX_COMPONENTS], n: usize) -> usize {
        layout[..n]
            .iter()
            .enumerate()
            .map(|(index, plane)| {
                let configured_rows = self.raw_pull_state.rows_per_stripe[index];
                let rows = if configured_rows == 0 {
                    plane.vertical_sampling_factor * DCT_BLOCK_SIZE
                } else {
                    configured_rows
                };
                plane.height.div_ceil(rows)
            })
            .max()
            .unwrap_or(0)
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
    /// This can be used after initialization or a partial decode. Active raw
    /// and scanline sequences are aborted, and the next output operation
    /// replays from scan start with the new options.
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
    /// Cancellation is propagated even in non-strict mode and is distinct from
    /// recoverable EOF. To retry, replace or clear the check and call
    /// [`decode_into`](Self::decode_into) again with the same output buffer.
    /// Stable baseline rows remain valid. If cancellation interrupts a
    /// progressive render, preview queries return zero until rerendering
    /// completes. Header parsing checks cancellation at marker boundaries and
    /// while buffering large marker bodies.
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

    /// Check cancellation at a non-MCU boundary, such as before parsing a
    /// marker body. Marker parsing is atomic, so callers can retry safely from
    /// the previously committed header or scan checkpoint.
    pub(crate) fn check_cancelled(&self) -> Result<(), DecodeErrors> {
        if self.cancel.as_ref().is_some_and(|cancel| cancel.is_cancelled()) {
            return Err(DecodeErrors::Cancelled);
        }
        Ok(())
    }

    /// Replace decoder options and restart any active raw or scanline sequence.
    ///
    /// Changes rebuild cached output and dispatch state, so the next decode
    /// replays from scan start using the new options.
    pub fn set_options(&mut self, options: DecoderOptions) {
        self.abort_raw_pull_sequence();
        self.abort_scanline_sequence();
        self.clear_scan_checkpoints();
        self.options = options;
        self.coeff = 1;
        self.pixels_decoded = 0;
        self.progressive_displayed_scans = 0;
        self.set_color_convert_from_options();
        self.idct_func = choose_idct_func(&self.options);
        self.idct_4x4_func = choose_idct_4x4_func(&self.options);
        self.idct_1x1_func = choose_idct_1x1_func(&self.options);
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
        self.check_cancelled()?;
        let resume_position = self.header_resume_position;
        if resume_position == 0 {
            // First two bytes should be jpeg soi marker
            let magic_bytes = self.stream.get_u16_be_err()?;

            if magic_bytes != 0xffd8 {
                return Err(DecodeErrors::IllegalMagicBytes(magic_bytes));
            }

            // Select the initial color converter. `set_options` refreshes it
            // when output options change after headers have been parsed.
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
            Some(0) if self.components.len() == 1 => ColorSpace::Luma,
            Some(0) if self.components.len() == 3 => ColorSpace::RGB,
            Some(0) => ColorSpace::CMYK,
            Some(1) => ColorSpace::YCbCr,
            Some(2) => ColorSpace::YCCK,
            Some(_) => unreachable!("APP14 parser rejects unknown transforms"),
            None if self.components.len() == 1 => ColorSpace::Luma,
            None if self.components.len() == 4 => ColorSpace::CMYK,
            None if self.components.len() == 3
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
        self.check_cancelled()?;
        self.skip_marker_payload()?;
        self.checkpoint_headers()?;
        Ok(())
    }
    pub(crate) fn parse_marker_inner(&mut self, m: Marker) -> Result<(), DecodeErrors> {
        self.check_cancelled()?;
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
                warn!("Capabilities for processing marker \"{m:?}\" not implemented");
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

    fn ensure_supported_encoding(&self) -> Result<(), DecodeErrors> {
        let unsupported = match self.info.sof {
            SOFMarkers::LosslessHuffman => Some(UnsupportedSchemes::LosslessHuffman),
            SOFMarkers::LosslessArithmetic => Some(UnsupportedSchemes::LosslessArithmetic),
            _ => None
        };
        if let Some(unsupported) = unsupported {
            return Err(DecodeErrors::Unsupported(unsupported));
        }
        if self.info.pixel_density == 12 {
            return Err(DecodeErrors::FormatStatic(
                "12-bit JPEG pixel decoding is not supported"
            ));
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
    /// Call [`set_incremental_mode`](Self::set_incremental_mode) before the
    /// first scan attempt when input is expected to arrive incrementally. This
    /// makes scan EOF recoverable in non-strict mode and records row
    /// checkpoints immediately. Without incremental mode, non-strict scan EOF
    /// completes with best-effort output for compatibility with truncated JPEGs.
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
    fn prepare_for_scan_decode(&mut self) -> Result<(), DecodeErrors> {
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
            progressive_view:      Option<ProgressiveCheckpointView>,
            progressive_fine_view: Option<ProgressiveFineCheckpointView>
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
        #[derive(Clone)]
        struct ProgressiveFineCheckpointView {
            append_snapshot: HeaderAppendStateSnapshot,
            sos_snapshot:    SosParamsSnapshot,
            stream_position: usize,
            completed_scans: usize,
            displayed_scans: usize,
            todo:            usize,
            dc_predictions:  [(i32, i32); MAX_COMPONENTS]
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
            }),
            progressive_fine_view: state.progressive_fine_checkpoint.as_deref().map(|checkpoint| {
                ProgressiveFineCheckpointView {
                    append_snapshot: checkpoint.append_snapshot,
                    sos_snapshot:    checkpoint.sos_snapshot,
                    stream_position: checkpoint.stream_position,
                    completed_scans: checkpoint.completed_scans,
                    displayed_scans: checkpoint.displayed_scans,
                    todo:            checkpoint.todo,
                    dc_predictions:  checkpoint.dc_predictions
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
                progressive_view,
                progressive_fine_view
            } = plan;
            // Roll back inline metadata from a previous scan attempt.
            let resume_append_snapshot = progressive_fine_view.as_ref().map_or_else(
                || {
                    progressive_view.as_ref().map_or_else(
                        || {
                            checkpoint_view
                                .map_or(outer_append_snapshot, |view| view.append_snapshot)
                        },
                        |view| view.append_snapshot
                    )
                },
                |view| view.append_snapshot
            );
            resume_append_snapshot.rollback(self);

            // Restore the SOS state for the chosen resume point.
            let resume_sos_snapshot = progressive_fine_view.as_ref().map_or_else(
                || {
                    progressive_view.as_ref().map_or_else(
                        || checkpoint_view.map_or(outer_sos_snapshot, |view| view.sos_snapshot),
                        |view| view.sos_snapshot
                    )
                },
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

            let had_progressive_view =
                progressive_view.is_some() || progressive_fine_view.is_some();
            if let Some(view) = progressive_fine_view {
                let header_snapshot = progressive_view
                    .as_ref()
                    .map_or(&outer_header_snapshot, |view| &view.header_snapshot);
                self.restore_scan_header_state(header_snapshot);
                self.stream.set_position(view.stream_position)?;
                self.progressive_completed_scans = view.completed_scans;
                self.progressive_displayed_scans =
                    if self.progressive_render_incomplete { 0 } else { view.displayed_scans };
                self.todo = view.todo;
                self.pixels_decoded = 0;
                for (i, comp) in self.components.iter_mut().enumerate().take(MAX_COMPONENTS) {
                    let (dc_pred, dc_diff) = view.dc_predictions[i];
                    comp.dc_pred = dc_pred;
                    comp.dc_diff = dc_diff;
                }
            } else if let Some(view) = progressive_view {
                self.restore_scan_header_state(&view.header_snapshot);
                self.stream.set_position(view.stream_position)?;
                self.progressive_completed_scans = view.completed_scans;
                self.progressive_displayed_scans =
                    if self.progressive_render_incomplete { 0 } else { view.completed_scans };
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
                for (i, comp) in self.components.iter_mut().enumerate().take(MAX_COMPONENTS) {
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

        self.ensure_supported_encoding()?;

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

        Ok(())
    }

    fn decode_mcu_output_with_success_cleanup(
        &mut self, output: &mut McuDecodeOutput<'_, '_>
    ) -> Result<(), DecodeErrors> {
        match self.decode_mcu_output(output) {
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
                if let Some(pixels) = output.pixels_mut() {
                    self.pixels_decoded = pixels.len();
                }
                if self.is_progressive {
                    self.progressive_displayed_scans = self.progressive_completed_scans;
                }
                Ok(())
            }
            Err(e) => Err(e)
        }
    }

    pub fn decode_into(&mut self, out: &mut [u8]) -> Result<(), DecodeErrors> {
        self.abort_raw_pull_sequence();
        self.abort_scanline_sequence();
        self.prepare_for_scan_decode()?;

        let expected_size = self.output_buffer_size().unwrap();

        if out.len() < expected_size {
            // too small of a size
            return Err(DecodeErrors::TooSmallOutput(expected_size, out.len()));
        }

        // ensure we don't touch anyone else's scratch space
        let out_len = core::cmp::min(out.len(), expected_size);
        let out = &mut out[0..out_len];

        let mut output = McuDecodeOutput::Pixels(out);
        self.decode_mcu_output_with_success_cleanup(&mut output)
    }

    fn decode_mcu_output(
        &mut self, output: &mut McuDecodeOutput<'_, '_>
    ) -> Result<(), DecodeErrors> {
        if self.is_arithmetic {
            #[cfg(feature = "arith")]
            {
                if self.is_progressive {
                    self.decode_mcu_ycbcr_progressive::<BitStreamArithmetic>(output)
                } else {
                    self.decode_mcu_ycbcr_baseline::<BitStreamArithmetic>(output)
                }
            }
            #[cfg(not(feature = "arith"))]
            unreachable!();
        } else if self.is_progressive {
            self.decode_mcu_ycbcr_progressive::<BitStreamHuffman>(output)
        } else {
            self.decode_mcu_ycbcr_baseline::<BitStreamHuffman>(output)
        }
    }

    fn render_buffered_output_stripe(
        &mut self, output: &mut McuDecodeOutput<'_, '_>
    ) -> Result<(), DecodeErrors> {
        if self.is_progressive {
            self.render_buffered_progressive_stripe(output)
        } else {
            self.render_buffered_baseline_stripe(output)
        }
    }

    fn decode_buffered_raw_source(
        &mut self, requested_stripe: usize, layout: &[PlaneInfo; MAX_COMPONENTS], n: usize
    ) -> Result<(), DecodeErrors> {
        let mut buffers: Vec<Vec<u8>> = layout[..n]
            .iter()
            .map(|plane| {
                let rows = plane.vertical_sampling_factor * DCT_BLOCK_SIZE;
                vec![0; plane.width * rows]
            })
            .collect();
        let mut planes: Vec<&mut [u8]> = buffers.iter_mut().map(Vec::as_mut_slice).collect();
        let mut lengths = [0usize; MAX_COMPONENTS];
        let mut target_strides = [0usize; MAX_COMPONENTS];
        let mut target_widths = [0usize; MAX_COMPONENTS];
        let mut target_heights = [0usize; MAX_COMPONENTS];
        for index in 0..n {
            lengths[index] = planes[index].len();
            target_strides[index] = layout[index].width;
            target_widths[index] = layout[index].width;
            target_heights[index] = layout[index].vertical_sampling_factor * DCT_BLOCK_SIZE;
        }
        let sink = RawPlanesSink {
            planes: &mut planes,
            lengths,
            target_strides,
            target_widths,
            target_heights,
            n_components: n,
            requested_stripe: Some(requested_stripe),
            rows_written: [0; MAX_COMPONENTS],
            stripe_ready: false,
            source_complete: false
        };
        let mut output = McuDecodeOutput::RawPlanes(sink);
        self.decode_mcu_output(&mut output)
    }

    fn finish_output_source(&mut self) {
        self.clear_scan_checkpoints();
        if self.is_progressive {
            self.progressive_displayed_scans = self.progressive_completed_scans;
        }
    }

    fn clear_scan_checkpoints(&mut self) {
        if let Some(state) = self.scan_state.as_deref_mut() {
            state.scan_checkpoint = None;
            state.progressive_checkpoint = None;
            state.progressive_fine_checkpoint = None;
        }
    }

    fn abort_raw_pull_sequence(&mut self) {
        if self.raw_pull_state.owner == RawPullOwner::None {
            return;
        }
        self.raw_pull_state = RawPullState::default();
        self.clear_scan_checkpoints();
    }

    fn abort_scanline_sequence(&mut self) {
        if !self.scanline_state.started {
            return;
        }
        self.scanline_state = ScanlineState::default();
        self.clear_scan_checkpoints();
    }

    fn raw_component_ids(&self) -> Option<Vec<u8>> {
        if !self.headers_decoded || self.components.is_empty() {
            return None;
        }
        Some(
            self.components
                .iter()
                .map(|component| component.id)
                .collect()
        )
    }

    /// Copy one MCU stripe (`mcu_stripe_index`) of post-IDCT samples from
    /// each component's `raw_coeff` into the caller-provided plane buffers.
    ///
    /// Called from the baseline / progressive paths in place of
    /// `post_process()` when decoding raw planes.
    ///
    /// The IDCT outputs are already clamped to `[0, 255]` so the `i16 -> u8`
    /// truncation here is exact.
    pub(crate) fn copy_raw_planes_for_mcu_stripe(
        &self, mcu_stripe_index: usize, sink: &mut RawPlanesSink<'_, '_>
    ) -> Result<(), DecodeErrors> {
        if let Some(requested_stripe) = sink.requested_stripe {
            if requested_stripe != mcu_stripe_index {
                return Err(DecodeErrors::FormatStatic(
                    "raw iMCU-row output advanced out of sequence"
                ));
            }
        }

        for (idx, comp) in self.components.iter().enumerate() {
            if idx >= sink.n_components {
                break;
            }
            let target_stride = sink.target_strides[idx];
            let target_width = sink.target_widths[idx];
            let target_height = sink.target_heights[idx];

            let stripe_rows = comp.vertical_sample * DCT_BLOCK_SIZE;
            let source_row_start = mcu_stripe_index * stripe_rows;
            let row_start = if sink.requested_stripe.is_some() { 0 } else { source_row_start };
            // Clip rows to the plane height.
            if row_start >= target_height {
                continue;
            }
            let src_stride = comp.width_stride;
            let available_source_rows = comp.raw_coeff.len() / src_stride;
            let logical_rows = if let Some(requested_stripe) = sink.requested_stripe {
                if self.expects_dnl {
                    target_height
                } else {
                    self.raw_planar_layout().map_or(target_height, |layout| {
                        layout[idx]
                            .height
                            .saturating_sub(requested_stripe * available_source_rows)
                            .min(target_height)
                    })
                }
            } else {
                target_height - row_start
            };
            let rows_to_copy = if sink.requested_stripe.is_some() {
                core::cmp::min(logical_rows, available_source_rows)
            } else {
                core::cmp::min(
                    core::cmp::min(stripe_rows, target_height - row_start),
                    available_source_rows
                )
            };
            sink.rows_written[idx] = rows_to_copy;

            let copy_w = core::cmp::min(src_stride, core::cmp::min(target_width, target_stride));
            let len = sink.lengths[idx];

            for r in 0..rows_to_copy {
                let src_row_start = r * src_stride;
                let src_row_end = src_row_start + copy_w;
                if src_row_end > comp.raw_coeff.len() {
                    return Err(DecodeErrors::FormatStatic(
                        "raw_coeff shorter than expected for MCU stripe"
                    ));
                }
                let src = &comp.raw_coeff[src_row_start..src_row_end];

                let dst_offset = (row_start + r) * target_stride;
                let dst_end = dst_offset + copy_w;
                if dst_end > len {
                    return Err(DecodeErrors::TooSmallOutput(dst_end, len));
                }
                let dst = &mut sink.planes[idx][dst_offset..dst_end];
                for (sample, dst) in src.iter().zip(dst.iter_mut()) {
                    #[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
                    {
                        *dst = *sample as u8;
                    }
                }
            }
        }
        sink.stripe_ready = true;
        Ok(())
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
            return;
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
    pub(crate) offset:     u32,
    pub(crate) total_size: u32,
    pub(crate) guid:       Vec<u8>,
    pub(crate) data:       Vec<u8>
}

/// A struct representing Image Information
#[derive(Default, Clone, Eq, PartialEq)]
#[allow(clippy::module_name_repetitions)]
pub struct ImageInfo {
    /// Width of the image
    pub width:                            u16,
    /// Height of image
    pub height:                           u16,
    /// Sample precision in bits.
    pub pixel_density:                    u8,
    /// Start of frame markers
    pub sof:                              SOFMarkers,
    /// Horizontal sample
    pub x_density:                        u16,
    /// Vertical sample
    pub y_density:                        u16,
    /// Number of components
    pub components:                       u8,
    /// Gain Map information, useful for
    /// UHDR images
    pub gain_map_info:                    Vec<GainMapInfo>,
    /// Multi picture information, useful for
    /// UHDR images
    pub multi_picture_information:        Option<Vec<u8>>,
    /// Exif Data
    pub exif_data:                        Option<Vec<u8>>,
    /// XMP Data
    pub xmp_data:                         Option<Vec<u8>>,
    /// IPTC Data
    pub iptc_data:                        Option<Vec<u8>>,
    /// Extended XMP Data
    pub extended_xmp:                     Option<Vec<u8>>,
    /// Extended XMP Guid
    pub extended_xmp_guid:                Option<Vec<u8>>,
    /// Image sub-sampling ratio
    pub sample_ratio:                     SampleRatios,
    /// The offset at which Multi picture information was found
    pub multi_picture_information_offset: Option<u64>
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

#[cfg(test)]
mod planar_layout_helpers {
    use zune_core::bytestream::ZCursor;
    use zune_core::colorspace::ColorSpace;
    use zune_core::options::DecoderOptions;

    use crate::color_convert::choose_ycbcr_to_rgb_convert_func;
    use super::{round_up_pow2, JpegDecoder, RawImcuRowStatus, ScanlineReadStatus, ScanlineStatus};
    use crate::idct::{choose_idct_1x1_func, choose_idct_4x4_func, choose_idct_func};

    #[test]
    fn div_ceil_basic() {
        assert_eq!(0usize.div_ceil(8), 0);
        assert_eq!(1usize.div_ceil(8), 1);
        assert_eq!(8usize.div_ceil(8), 1);
        assert_eq!(9usize.div_ceil(8), 2);
        assert_eq!(64usize.div_ceil(8), 8);
        assert_eq!(65usize.div_ceil(8), 9);
        assert_eq!(101usize.div_ceil(8), 13);
    }

    #[test]
    fn round_up_pow2_basic() {
        assert_eq!(round_up_pow2(0, 8), Some(0));
        assert_eq!(round_up_pow2(1, 8), Some(8));
        assert_eq!(round_up_pow2(8, 8), Some(8));
        assert_eq!(round_up_pow2(9, 8), Some(16));
        assert_eq!(round_up_pow2(64, 8), Some(64));
        assert_eq!(round_up_pow2(101, 8), Some(104));
    }

    #[test]
    fn round_up_pow2_overflow() {
        assert_eq!(round_up_pow2(usize::MAX, 8), None);
    }

    #[test]
    fn set_options_refreshes_cached_dispatch() {
        for (initial, replacement) in [
            (DecoderOptions::new_fast(), DecoderOptions::new_safe()),
            (DecoderOptions::new_safe(), DecoderOptions::new_fast())
        ] {
            let mut decoder = JpegDecoder::new_with_options(ZCursor::new(&[]), initial);
            decoder.set_options(replacement);
            assert_eq!(
                decoder.idct_func as usize,
                choose_idct_func(&replacement) as usize
            );
            assert_eq!(
                decoder.idct_4x4_func as usize,
                choose_idct_4x4_func(&replacement) as usize
            );
            assert_eq!(
                decoder.idct_1x1_func as usize,
                choose_idct_1x1_func(&replacement) as usize
            );
        }

        for colorspace in [
            ColorSpace::RGB,
            ColorSpace::BGR,
            ColorSpace::RGBA,
            ColorSpace::BGRA
        ] {
            let replacement = DecoderOptions::default().jpeg_set_out_colorspace(colorspace);
            let mut decoder = JpegDecoder::new(ZCursor::new(&[]));
            decoder.set_options(replacement);
            assert_eq!(
                decoder.color_convert_16 as usize,
                choose_ycbcr_to_rgb_convert_func(colorspace, &replacement).unwrap() as usize
            );
        }
    }

    #[test]
    fn baseline_pull_memory_is_stripe_bounded() {
        let data = include_bytes!("../../../test-images/jpeg/2029.jpg");
        let mut decoder = JpegDecoder::new(ZCursor::new(data));
        decoder.decode_headers().unwrap();
        let mut raw = decoder.raw_output();
        let layout = raw.layout().unwrap();
        let count = raw.num_components().unwrap();
        let strides: Vec<usize> = layout[..count].iter().map(|plane| plane.width).collect();
        let mut buffers: Vec<Vec<u8>> = layout[..count]
            .iter()
            .map(|plane| vec![0; plane.width * plane.vertical_sampling_factor * 8])
            .collect();
        let mut planes: Vec<&mut [u8]> = buffers.iter_mut().map(Vec::as_mut_slice).collect();
        assert!(matches!(
            raw.decode_next_imcu_row(&mut planes, &strides).unwrap(),
            RawImcuRowStatus::RowReady { .. }
        ));
        drop(raw);

        let (stripe_buffers, full_buffers) = decoder.raw_buffer_capacities();
        assert!(full_buffers[..count].iter().all(|length| *length == 0));
        for index in 0..count {
            let rows = layout[index].vertical_sampling_factor * 8;
            assert!(
                stripe_buffers[index] <= (layout[index].stride + 8) * rows,
                "component {index} exceeded one-iMCU-row scratch bound"
            );
            assert!(stripe_buffers[index] < layout[index].byte_size);
        }
    }

    #[test]
    fn baseline_scanline_memory_is_stripe_bounded() {
        let data = include_bytes!("../../../test-images/jpeg/2029.jpg");
        let mut decoder = JpegDecoder::new(ZCursor::new(data));
        let (row_bytes, height) = {
            let mut scanlines = decoder.scanline_output();
            assert_eq!(scanlines.start().unwrap(), ScanlineStatus::Ready);
            let row_bytes = scanlines.output_row_bytes().unwrap();
            let height = scanlines.output_height().unwrap();
            let mut row = vec![0; row_bytes];
            assert_eq!(
                scanlines.read_scanlines(&mut row, row_bytes).unwrap(),
                ScanlineReadStatus::RowsProcessed { rows: 1 }
            );
            (row_bytes, height)
        };

        assert!(decoder
            .progressive_mcus_buffer
            .iter()
            .all(|buffer| buffer.capacity() == 0));
        assert!(decoder.scanline_state.staging.capacity() <= row_bytes * 36);
        assert!(decoder.scanline_state.staging.capacity() < row_bytes * height);
        for component in &decoder.components {
            assert!(
                component.raw_coeff.capacity()
                    <= (component.width_stride + 8) * component.vertical_sample * 8
            );
        }
    }
}
