/*
 * Copyright (c) 2023.
 *
 * This software is free software;
 *
 * You can redistribute it or modify it under terms of the MIT, Apache License or Zlib license
 */

use alloc::vec::Vec;
use alloc::{format, vec};
use core::cmp::min;

use zune_core::bytestream::ZByteReaderTrait;
use zune_core::colorspace::ColorSpace;
use zune_core::colorspace::ColorSpace::Luma;
use zune_core::log::{error, trace, warn};

use crate::bitstream::BitStream;
use crate::components::SampleRatios;
use crate::decoder::{HeaderAppendStateSnapshot, McuDecodeOutput, MAX_COMPONENTS};
use crate::errors::DecodeErrors;
use crate::marker::Marker;
use crate::mcu_prog::get_marker_with_discarded;
use crate::misc::{calculate_padded_width, setup_component_params};
use crate::worker::{color_convert, upsample};
use crate::JpegDecoder;

/// The size of a DC block for a MCU.
pub const DCT_BLOCK: usize = 64;

fn baseline_coefficient_count(
    mcu_width: usize, mcu_height: usize, horizontal_sample: usize, vertical_sample: usize
) -> Result<usize, DecodeErrors> {
    let len = mcu_width
        .checked_mul(vertical_sample)
        .and_then(|len| len.checked_mul(horizontal_sample))
        .and_then(|len| len.checked_mul(mcu_height))
        .and_then(|len| len.checked_mul(DCT_BLOCK))
        .ok_or(DecodeErrors::FormatStatic(
            "Baseline coefficient buffer size overflow"
        ))?;
    // Vec's byte extent, including alignment, must fit in isize as well as usize.
    core::alloc::Layout::array::<i32>(len)
        .map_err(|_| DecodeErrors::FormatStatic("Baseline coefficient buffer size overflow"))?;
    Ok(len)
}

fn prepare_baseline_coefficients(
    buffer: &mut Vec<i32>, mcu_width: usize, mcu_height: usize, horizontal_sample: usize,
    vertical_sample: usize, resuming: bool
) -> Result<(), DecodeErrors> {
    let len =
        baseline_coefficient_count(mcu_width, mcu_height, horizontal_sample, vertical_sample)?;
    if !resuming || buffer.len() != len {
        // Reserve before clearing so failure preserves the previous coefficients.
        buffer
            .try_reserve_exact(len.saturating_sub(buffer.len()))
            .map_err(|_| {
                DecodeErrors::FormatStatic("Could not allocate baseline coefficient buffer")
            })?;
        buffer.clear();
        buffer.resize(len, 0);
    }
    Ok(())
}

#[cfg(test)]
mod coefficient_buffer_tests {
    use alloc::vec;

    use zune_core::bytestream::ZCursor;

    use super::{
        baseline_coefficient_count, prepare_baseline_coefficients, JpegDecoder, DCT_BLOCK
    };

    #[test]
    fn native_fast_input_window_requires_its_budget_and_no_marker() {
        let mut stuffed = vec![0; 512];
        stuffed[0] = 0xff;
        let mut marker = stuffed.clone();
        marker[1] = 0xd9;
        let mut trailing_prefix = vec![0; 512];
        trailing_prefix[511] = 0xff;
        for (data, blocks, expected) in [
            (vec![0; 511], 1, false),
            (vec![0; 512], 1, true),
            (vec![0; 512], 2, false),
            (vec![0; 1024], 2, true),
            (stuffed, 1, true),
            (marker, 1, false),
            (trailing_prefix, 1, false)
        ] {
            let mut decoder = JpegDecoder::new(ZCursor::new(&data));
            assert_eq!(
                decoder.native_fast_huffman_window(blocks).unwrap(),
                expected
            );
            assert_eq!(decoder.stream.position().unwrap(), 0);
        }
        let mut decoder = JpegDecoder::new(ZCursor::new(&[]));
        assert!(decoder.native_fast_huffman_window(usize::MAX).is_err());
    }

    #[test]
    fn checked_geometry_counts_all_sampling_factors() {
        assert_eq!(
            baseline_coefficient_count(2, 3, 2, 4).unwrap(),
            48 * DCT_BLOCK
        );
    }

    #[test]
    fn element_and_i32_byte_extent_overflows_are_rejected() {
        for (width, height, horizontal, vertical) in [
            (usize::MAX, 1, 1, 2),
            (usize::MAX, 1, 2, 1),
            (2, usize::MAX, 1, 1),
            (usize::MAX / DCT_BLOCK + 1, 1, 1, 1),
            // The element count fits usize, but its i32 byte extent does not.
            (usize::MAX / DCT_BLOCK, 1, 1, 1)
        ] {
            let error =
                baseline_coefficient_count(width, height, horizontal, vertical).unwrap_err();
            assert!(!error.is_recoverable_eof());
        }
    }

    #[test]
    fn allocation_layout_boundary_is_checked_before_touching_existing_storage() {
        let max_bytes = usize::try_from(isize::MAX).unwrap();
        let max_mcus = max_bytes / core::mem::size_of::<i32>() / DCT_BLOCK;
        assert_eq!(
            baseline_coefficient_count(max_mcus, 1, 1, 1).unwrap(),
            max_mcus * DCT_BLOCK
        );
        // This byte extent fits usize but exceeds Vec's isize allocation limit.
        let mut buffer = vec![7; DCT_BLOCK];
        let capacity = buffer.capacity();
        assert!(prepare_baseline_coefficients(&mut buffer, max_mcus + 1, 1, 1, 1, false).is_err());
        assert_eq!(buffer, [7; DCT_BLOCK]);
        assert_eq!(buffer.capacity(), capacity);
    }

    #[test]
    fn reservations_preserve_retries_and_reinitialize_fresh_or_changed_geometry() {
        let mut buffer = vec![7; DCT_BLOCK];
        let capacity = buffer.capacity();
        prepare_baseline_coefficients(&mut buffer, 1, 1, 1, 1, true).unwrap();
        assert_eq!(buffer, [7; DCT_BLOCK]);
        assert_eq!(buffer.capacity(), capacity);

        prepare_baseline_coefficients(&mut buffer, 1, 1, 1, 1, false).unwrap();
        assert_eq!(buffer, [0; DCT_BLOCK]);
        assert_eq!(buffer.capacity(), capacity);

        buffer.fill(7);
        prepare_baseline_coefficients(&mut buffer, 2, 1, 1, 1, true).unwrap();
        assert_eq!(buffer, [0; 2 * DCT_BLOCK]);
        assert!(buffer.capacity() >= 2 * DCT_BLOCK);

        let capacity = buffer.capacity();
        buffer.fill(7);
        prepare_baseline_coefficients(&mut buffer, 1, 1, 1, 1, true).unwrap();
        assert_eq!(buffer, [0; DCT_BLOCK]);
        assert_eq!(buffer.capacity(), capacity);
    }
}

struct McuWidthContext<'a, B: BitStream> {
    mcu_width:      usize,
    // Current MCU row, used when indexing progressive scratch buffers and checkpoints.
    mcu_row:        usize,
    // First MCU column to decode in this row; non-zero when resuming from a restart checkpoint.
    start_col:      usize,
    // Number of output bytes already committed before this MCU row.
    pixels_written: usize,
    // Shared coefficient scratch block reused for each decoded data unit.
    tmp:            &'a mut [i32; 64],
    // Entropy decoder state for the current scan.
    stream:         &'a mut B,
    // Full-image dequantized coefficient buffers for multi-SOS baseline scans.
    progressive:    &'a mut [Vec<i32>; MAX_COMPONENTS],
    // True when decoding the final data-unit row of the current scan.
    is_last_row:    bool,
    scaled:        Option<&'a mut crate::scaled::ScaledOutput>
}

impl<T: ZByteReaderTrait> JpegDecoder<T> {
    /// Check for existence of DC and AC Huffman Tables
    pub(crate) fn check_tables<B: BitStream>(&mut self) -> Result<(), DecodeErrors> {
        // check that dc and AC tables exist outside the hot path
        let z_order = self.z_order;
        for &component_index in &z_order[..usize::from(self.num_scans)] {
            let Some(component) = self.components.get(component_index) else {
                return Err(DecodeErrors::Format(format!(
                    "Invalid component index {component_index} in scan"
                )));
            };
            let _ = B::get_dc_ac_tables(
                &mut self.entropy_tables,
                component.dc_huff_table,
                component.ac_huff_table
            )?;
        }
        Ok(())
    }

    /// Decode MCUs and carry out post processing.
    ///
    /// This is the main decoder loop for the library, the hot path.
    pub(crate) fn decode_mcu_ycbcr_baseline<B: BitStream>(
        &mut self, output: &mut McuDecodeOutput<'_, '_>
    ) -> Result<(), DecodeErrors> {
        // Move the persistent multi-SOS coefficient buffer out of `self` so
        // the inner decoder can borrow it mutably while still calling methods
        // on `self`. The buffer is put back on return and survives across
        // `decode_into` retries, which is what lets scan checkpoints stay
        // allocation-free.
        let mut progressive_mcus = core::mem::take(&mut self.baseline_coefficients);
        let mut upsampler_scratch = core::mem::take(&mut self.upsampler_scratch);
        let result = self.decode_mcu_ycbcr_baseline_inner::<B>(
            output,
            &mut progressive_mcus,
            &mut upsampler_scratch,
        );
        self.baseline_coefficients = progressive_mcus;
        self.upsampler_scratch = upsampler_scratch;
        result
    }

    /// Inner implementation of [`Self::decode_mcu_ycbcr_baseline`].
    ///
    /// `progressive_mcus` is owned by the decoder across calls so its
    /// contents survive recoverable EOFs and don't need to be copied into a
    /// `ScanCheckpoint` at restart boundaries.
    ///
    /// Because of this, we pull in some very crazy optimization tricks hence readability is a pinch
    /// here.
    #[allow(
        clippy::similar_names,
        clippy::too_many_lines,
        clippy::cast_possible_truncation,
        clippy::used_underscore_binding
    )]
    #[inline(never)]
    fn decode_mcu_ycbcr_baseline_inner<B: BitStream>(
        &mut self, output: &mut McuDecodeOutput<'_, '_>,
        progressive_mcus: &mut [Vec<i32>; MAX_COMPONENTS], upsampler_scratch_space: &mut Vec<i16>
    ) -> Result<(), DecodeErrors> {
        setup_component_params(self)?;

        let (mut mcu_width, mut mcu_height);

        if self.is_interleaved {
            // set upsampling functions
            self.set_upsampling();

            mcu_width = self.mcu_x;
            mcu_height = self.mcu_y;
        } else {
            // For non-interleaved images( (1*1) subsampling)
            // number of MCU's are the widths (+7 to account for paddings) divided bu 8.
            mcu_width = (self.info.width as usize).div_ceil(8);
            mcu_height = if self.expects_dnl {
                self.options.max_height().div_ceil(8)
            } else {
                (self.info.height as usize).div_ceil(8)
            };
        }
        if self.is_interleaved
            && self.input_colorspace.num_components() > 1
            && self.options.jpeg_get_out_colorspace().num_components() == 1
            && (self.info.sample_ratio == SampleRatios::V
                || self.info.sample_ratio == SampleRatios::HV)
        {
            // For a specific set of images, e.g interleaved,
            // when converting from YcbCr to grayscale, we need to
            // take into account mcu height since the MCU decoding needs to take
            // it into account for padding purposes and the post processor
            // parses two rows per mcu width.
            //
            // set coeff to be 2 to ensure that we increment two rows
            // for every mcu processed also
            mcu_height *= self.v_max;
            mcu_height /= self.h_max;
            self.coeff = 2;
        }

        if self.input_colorspace == ColorSpace::Luma && self.is_interleaved {
            warn!("Grayscale image with down-sampled component, resetting component details");

            self.reset_params();

            mcu_width = self.info.width.div_ceil(8) as usize;
            mcu_height = if self.expects_dnl {
                self.options.max_height().div_ceil(8)
            } else {
                self.info.height.div_ceil(8) as usize
            };
        }
        let width = usize::from(self.info.width);

        let padded_width = calculate_padded_width(width, self.info.sample_ratio);

        let mut stream = B::new();
        stream.set_strict(self.options.strict_mode());

        let mut tmp = [0_i32; DCT_BLOCK];

        let comp_len = self.components.len();
        // True when we are resuming from a previously-saved scan checkpoint;
        // in that case the per-component `raw_coeff` and `progressive_mcus`
        // buffers still hold the data from the prior `decode_into` call and
        // must not be re-zeroed. On a fresh decode they are (re-)allocated.
        let resuming = self.scan_checkpoint().is_some();

        let raw_mode = output.is_raw();
        if !resuming {
            self.baseline_seen_components.fill(false);
            self.baseline_eoi_seen = false;
            self.baseline_corrupt = false;
        }

        for (pos, comp) in self.components.iter_mut().enumerate() {
            // Allocate only needed components.
            //
            // For special colorspaces i.e YCCK and CMYK, just allocate all of the needed
            // components.
            //
            // Raw output needs every component regardless of output colorspace.
            if raw_mode
                || min(
                    self.options.jpeg_get_out_colorspace().num_components() - 1,
                    pos
                ) == pos
                || comp_len == 4
            // Special colorspace
            {
                // allocate enough space to hold a whole MCU width
                // this means we should take into account sampling ratios
                // `*8` is because each MCU spans 8 widths.
                let len = comp.width_stride * comp.vertical_sample * 8;

                comp.needed = true;
                if !resuming || comp.raw_coeff.len() != len {
                    // Reuse capacity across decodes; zero contents.
                    comp.raw_coeff.clear();
                    comp.raw_coeff.resize(len, 0);
                }
            } else {
                comp.needed = false;
            }
        }

        // If all components are contained in the first scan of MCUs, then we can process into
        // (upsampled) pixels immediately after each MCU, for convenience we use each row of MCUS.
        // Otherwise, we must first wait until following SOS provide the remaining components.
        // A resumed later scan must keep the first scan's buffering strategy.
        let all_components_in_first_scan = usize::from(self.num_scans) == self.components.len()
            && !(resuming && progressive_mcus.iter().any(|buffer| !buffer.is_empty()));
        if self.expects_dnl && !all_components_in_first_scan {
            return Err(DecodeErrors::FormatStatic(
                "baseline multi-scan JPEG with DNL is not supported",
            ));
        }

        if all_components_in_first_scan {
            // No multi-SOS scratch is needed for this call; release any
            // retained `baseline_coefficients` capacity so it doesn't pin
            // memory between decodes.
            for mcu in progressive_mcus.iter_mut() {
                *mcu = Vec::new();
            }
        } else {
            for (component, mcu) in self.components.iter().zip(progressive_mcus.iter_mut()) {
                prepare_baseline_coefficients(
                    mcu,
                    mcu_width,
                    mcu_height,
                    component.horizontal_sample,
                    component.vertical_sample,
                    resuming
                )?;
            }
        }

        let checkpoint = self.scan_checkpoint().copied();
        let mut resume_row = 0;
        let mut resume_col = 0;
        let mut pixels_written = 0;

        if let Some(checkpoint) = checkpoint {
            resume_row = checkpoint.mcu_row;
            resume_col = checkpoint.mcu_col;
            pixels_written = checkpoint.pixels_written;
            // Coefficient and progressive buffers are persistent across
            // `decode_into` calls and already contain the data from the
            // prior attempt; we don't need to copy anything back. Only the
            // per-component DC predictor state needs to be restored, which
            // is already handled in `decode_into` from the checkpoint.

            // Restore bitstream decoder state for fine-grained resume.
            stream.restore_snapshot(checkpoint.bitstream_state);
        }

        let is_hv = usize::from(self.is_interleaved && !raw_mode);
        let upsampler_scratch_size = is_hv
            * self
                .components
                .iter()
                .map(|x| x.width_stride)
                .max()
                .unwrap_or(0)
            * 8;
        upsampler_scratch_space.clear();
        upsampler_scratch_space.resize(upsampler_scratch_size, 0);

        // Every scan walks the whole image, so bound the number of scans as the
        // progressive decoder does (jpeg_get_max_scans).
        let mut scans_seen = self.baseline_scans_seen();
        let mut resuming_current_scan = resuming && scans_seen != 0;
        let mut all_required_components_seen = self.baseline_components_complete();
        let mut last_scan_rows_decoded: usize;
        'sos: loop {
            last_scan_rows_decoded = 0;
            if resuming_current_scan {
                resuming_current_scan = false;
            } else {
                scans_seen = scans_seen.saturating_add(1);
                self.set_baseline_scans_seen(scans_seen);
            }
            if scans_seen > self.options.jpeg_get_max_scans() {
                return Err(DecodeErrors::Format(format!(
                    "Too many scans, exceeded limit of {}",
                    self.options.jpeg_get_max_scans()
                )));
            }
            // Later scans may use Huffman tables defined by inter-scan DHT markers.
            self.check_tables::<B>()?;

            let scan_components = self.z_order;
            let scan_component_count = usize::from(self.num_scans);
            // DNL height is unresolved until entropy decoding reaches its marker.
            let scan_mcu_height = if self.num_scans > 1 || self.expects_dnl {
                mcu_height
            } else {
                let component_index = self.z_order.first().copied().unwrap_or(0);
                if let Some(component) = self.components.get(component_index) {
                    (self.info.height as usize * component.vertical_sample).div_ceil(self.v_max * 8)
                } else {
                    mcu_height
                }
            };

            trace!(
                "Baseline decoding of components: {:?}",
                &self.z_order[..usize::from(self.num_scans)]
            );

            trace!("Decoding MCU width: {mcu_width}, height: {scan_mcu_height}");

            let scan_start_row = resume_row;
            let scan_start_col = resume_col;
            resume_row = 0;
            resume_col = 0;

            let mut cancel = self.cancel_debounced(mcu_width);
            for i in scan_start_row..scan_mcu_height {
                let start_col = if i == scan_start_row { scan_start_col } else { 0 };
                if cancel.is_cancelled() {
                    return Err(DecodeErrors::Cancelled);
                }
                if stream.consumed_past_end() {
                    if self.scan_eof_is_error() {
                        return Err(DecodeErrors::ExhaustedData);
                    }
                    if all_components_in_first_scan {
                        if let Some(remaining) =
                            output.pixels_mut().and_then(|pixels| pixels.get_mut(pixels_written..))
                        {
                            remaining.fill(128);
                        }
                        return Ok(());
                    }
                    break 'sos;
                }

                // Per-row checkpoint: save bitstream state at the start of
                // each MCU row so we can resume from here (rather than
                // replaying from scan start) if ExhaustedData fires mid-row.
                // Active on retry calls, or on the first attempt when the
                // caller explicitly opted into incremental mode. Default
                // one-shot decode keeps this disabled.
                //
                // Multi-SOS baseline scans keep their full-image coefficient
                // buffers on the decoder across retries, so row checkpoints
                // are safe there too. They still report no stable output
                // until final assembly has all component scans.
                if self.mcu_checkpoints_enabled
                    && !self.is_progressive
                    && B::supports_mcu_checkpoint()
                    && self.pending_dnl_height.is_none()
                    // A checkpoint saved past the end of the visible input cannot be
                    // resumed: restoring its bitstream state would go on claiming the
                    // data ended at the saved cursor, which stops being true the
                    // moment the caller grows the input. Rows decoded from bits that
                    // were buffered when the input ended are not stable either
                    // (`pixels_decoded` is not advanced for them below), so resuming
                    // from the last clean row loses nothing.
                    && stream.overread_by() == 0
                {
                    let dc_predictions = core::array::from_fn(|idx| {
                        self.components
                            .get(idx)
                            .map_or((0, 0), |component| (component.dc_pred, component.dc_diff))
                    });
                    let bs_state = stream.snapshot_state();
                    self.checkpoint_scan_with_bitstream(
                        i,
                        start_col,
                        pixels_written,
                        dc_predictions,
                        bs_state
                    )?;
                }

                // A truncated refill can speculatively decode zero-padded
                // symbols before detecting EOF. Replaying a repeated scan
                // must start with the original coefficients, not those writes.
                let row_backup: Vec<_> = if !all_components_in_first_scan
                    && self.mcu_checkpoints_enabled
                {
                    self.z_order[..usize::from(self.num_scans)]
                        .iter()
                        .map(|&index| {
                            let component = &self.components[index];
                            let rows = if self.num_scans > 1 {
                                component.vertical_sample
                            } else {
                                1
                            };
                            let start = i * rows * component.width_stride * 8;
                            let end = start + rows * component.width_stride * 8;
                            (index, start, progressive_mcus[index][start..end].to_vec())
                        })
                        .collect()
                } else {
                    Vec::new()
                };
                let seen_components = self.baseline_seen_components;
                let baseline_corrupt = self.baseline_corrupt;

                // decode a whole MCU width,
                // this takes into account interleaved components.
                if cancel.is_cancelled() {
                    return Err(DecodeErrors::Cancelled);
                }
                let terminate_result = {
                    let mut mcu_width_context = McuWidthContext {
                        mcu_width,
                        mcu_row: i,
                        start_col,
                        pixels_written,
                        tmp: &mut tmp,
                        stream: &mut stream,
                        progressive: &mut *progressive_mcus,
                        is_last_row: i + 1 >= scan_mcu_height,
                        scaled: match output {
                            McuDecodeOutput::Scaled(scaled) => Some(&mut **scaled),
                            _ => None,
                        },
                    };
                    if all_components_in_first_scan {
                        self.decode_mcu_width::<false, B>(&mut mcu_width_context)
                    } else {
                    /* NB: (cae). This code was added due to the issue at https://github.com/etemesi254/zune-image/issues/277
                    *
                    * There is a particular set of images that interleave the start of scan (SOS) with the MCU,
                    * E.g if it's a three component image, we have SOS->MCU ->SOS->MCU ->SOS->MCU
                    * which presents a problem on decoding, we need to buffer the whole image before continuing since
                    * we won't have a row containing all the component data which will be needed e.g for color conversion.
                    *
                    * The mechanisms is that we decode the whole image upfront, which goes against the normal
                    * routine of decoding MCU width , so this requires more memory upfront than initial routines
                    * but it is a single image out of the many corpuses that exist, so its fine.
                    * (image in test-images/jpeg/sos_news.jpeg)

                    * Code contributed by  Aurelia Molzer (https://github.com/197g)

                    *
                    */

                        self.decode_mcu_width::<true, B>(&mut mcu_width_context)
                    }
                };

                if terminate_result.as_ref().is_err_and(|error| {
                    !error.is_recoverable_eof() || self.scan_eof_is_error()
                })
                    || (stream.overread_by() > 0 && self.scan_eof_is_error())
                {
                    for (index, start, coefficients) in &row_backup {
                        progressive_mcus[*index][*start..][..coefficients.len()]
                            .copy_from_slice(coefficients);
                    }
                    self.baseline_seen_components = seen_components;
                    self.baseline_corrupt = baseline_corrupt;
                }

                let terminate = match terminate_result {
                    Ok(terminate) => terminate,
                    Err(e) if e.is_recoverable_eof() && !self.scan_eof_is_error() => {
                        error!("{e}");
                        if all_components_in_first_scan {
                            match output {
                                McuDecodeOutput::Pixels(pixels) => {
                                    // A row that ran out of data mid-way and consumed
                                    // the zero padding the reader appends past the end
                                    // is not publishable even in part: which of its
                                    // MCUs came from the data and which from padding
                                    // is not recoverable, so the row and everything
                                    // after it stay neutral, as a row that begins
                                    // past the end does.
                                    if !stream.consumed_past_end() {
                                        let output_stride = width
                                            * self.options.jpeg_get_out_colorspace().num_components();
                                        self.post_process(
                                            pixels,
                                            output_stride,
                                            i,
                                            mcu_height,
                                            width,
                                            padded_width,
                                            &mut pixels_written,
                                            upsampler_scratch_space,
                                        )?;
                                    }
                                    self.pixels_decoded = pixels_written;
                                    if let Some(remaining) = pixels.get_mut(pixels_written..) {
                                        remaining.fill(128);
                                    }
                                }
                                McuDecodeOutput::RawPlanes(raw_planes) => {
                                    self.copy_raw_planes_for_mcu_stripe(i, raw_planes)?;
                                }
                                McuDecodeOutput::Scanlines(_) => return Err(e),
                                McuDecodeOutput::Scaled(_) => {},
                            }
                        } else {
                            break 'sos;
                        }
                        return Ok(());
                    }
                    Err(e) => return Err(e)
                };
                last_scan_rows_decoded = i + 1;
                let effective_mcu_height =
                    if let McuContinuation::DnlFound(height) = &terminate {
                        self.validate_dnl_scan_rows(
                            *height,
                            last_scan_rows_decoded,
                            all_components_in_first_scan,
                        )?;
                        self.resolve_dnl_height(*height)?;
                        last_scan_rows_decoded
                    } else {
                        mcu_height
                    };

                // A row that began on bits the data held can finish on bits it did
                // not: once the reader's lookahead crosses the end of the data, the
                // zeros appended after it are synthesised, and a row whose last
                // MCUs were decoded from them is not publishable — those MCUs carry
                // values the data never stated. Strict and incremental callers
                // return a recoverable EOF and resume from this row's checkpoint,
                // which was saved while the row still started on real bits;
                // lenient ones leave the row and everything after it neutral.
                if stream.consumed_past_end() {
                    if self.scan_eof_is_error() {
                        return Err(DecodeErrors::ExhaustedData);
                    }
                    if all_components_in_first_scan {
                        if let Some(remaining) =
                            output.pixels_mut().and_then(|pixels| pixels.get_mut(pixels_written..))
                        {
                            remaining.fill(128);
                        }
                        return Ok(());
                    }
                    break 'sos;
                }

                // Neither a pull request nor the stable packed prefix may
                // publish a row assembled after exhausting growing input.
                if stream.overread_by() > 0
                    && (self.scan_eof_is_error() || output.requested_output_stripe().is_some())
                {
                    return Err(DecodeErrors::ExhaustedData);
                }
                if output.requested_output_stripe().is_some() {
                    if let McuContinuation::Terminate(reason) = &terminate {
                        return Err(reason.raw_error());
                    }
                }

                if i + 1 == scan_mcu_height
                    || matches!(
                        &terminate,
                        McuContinuation::AnotherSos
                            | McuContinuation::InterScanMarker(_)
                            | McuContinuation::DnlFound(_)
                    )
                {
                    all_required_components_seen = self.mark_baseline_scan_components_complete(
                        &scan_components[..scan_component_count],
                    );
                }

                // process that width up until it's impossible. This is faster than allocation the
                // full components, which we skipped earlier.
                if all_components_in_first_scan {
                    let pull_row_ready = output.requested_output_stripe().is_some();
                    let final_pull_row = output.requested_raw_stripe().is_some()
                        && i + 1 == scan_mcu_height;
                    if pull_row_ready && self.pending_dnl_height.is_none() {
                        if final_pull_row {
                            if let McuContinuation::InterScanMarker(marker) = &terminate {
                                // `check_stream_marker_after_mcu_width` removes the marker before
                                // returning it. Keep it in the final-row checkpoint so a suspended
                                // trailing marker sequence can replay from this exact boundary.
                                *stream.marker() = Some(*marker);
                            }
                        }
                        let dc_predictions = core::array::from_fn(|idx| {
                            self.components
                                .get(idx)
                                .map_or((0, 0), |component| (component.dc_pred, component.dc_diff))
                        });
                        self.checkpoint_scan_with_bitstream(
                            i + 1,
                            0,
                            pixels_written,
                            dc_predictions,
                            stream.snapshot_state()
                        )?;
                    }

                    match output {
                        McuDecodeOutput::Scaled(_) => {},
                        McuDecodeOutput::Pixels(pixels) => {
                            let output_stride =
                                width * self.options.jpeg_get_out_colorspace().num_components();
                            self.post_process(
                                pixels,
                                output_stride,
                                i,
                                effective_mcu_height,
                                width,
                                padded_width,
                                &mut pixels_written,
                                upsampler_scratch_space,
                            )?;
                            // A row finished from zero padding past the end of the
                            // input is not stable: a retry with more input decodes it
                            // again with the real bits.
                            if stream.overread_by() == 0 {
                                self.pixels_decoded = pixels_written;
                            }
                        }
                        McuDecodeOutput::RawPlanes(raw_planes) => {
                            if !final_pull_row {
                                self.copy_raw_planes_for_mcu_stripe(i, raw_planes)?;
                            }
                        }
                        McuDecodeOutput::Scanlines(scanlines) => {
                            if scanlines.requested_stripe != i {
                                return Err(DecodeErrors::FormatStatic(
                                    "converted scanline output advanced out of sequence"
                                ));
                            }
                            let mut stripe_written = 0;
                            self.post_process(
                                scanlines.pixels,
                                scanlines.stride,
                                i,
                                mcu_height,
                                width,
                                padded_width,
                                &mut stripe_written,
                                upsampler_scratch_space,
                            )?;
                            scanlines.rows_written = stripe_written / scanlines.stride;
                        }
                    }
                    if pull_row_ready && !final_pull_row {
                        return Ok(());
                    }
                    // This row's coefficient buffers can be reused next, so
                    // any checkpoint inside the row is no longer valid.
                    if !final_pull_row && self.pending_dnl_height.is_none() {
                        self.invalidate_scan_checkpoint();
                    }
                }

                match terminate {
                    McuContinuation::Ok => {}
                    McuContinuation::AnotherSos if self.dnl_image => {
                        return Err(DecodeErrors::FormatStatic(
                            "JPEG with DNL cannot contain multiple scans",
                        ));
                    }
                    McuContinuation::AnotherSos if all_components_in_first_scan => {
                        if self.scan_eof_is_error() {
                            return Err(DecodeErrors::FormatStatic(
                                "Unexpected SOS after completed scan"
                            ));
                        }
                        warn!("More than one SOS despite already having all components");
                        if let McuDecodeOutput::RawPlanes(raw_planes) = output {
                            self.copy_raw_planes_for_mcu_stripe(i, raw_planes)?;
                            self.invalidate_scan_checkpoint();
                        }
                        return Ok(());
                    }
                    McuContinuation::AnotherSos => continue 'sos,
                    McuContinuation::InterScanMarker(marker) => {
                        // Handle inter-scan markers (DHT/DQT/etc) uniformly here.
                        // This keeps all marker handling in the outer loop.
                        if self.advance_to_next_sos(
                            marker,
                            &mut stream,
                            all_required_components_seen,
                        )? {
                            if self.dnl_image {
                                return Err(DecodeErrors::FormatStatic(
                                    "JPEG with DNL cannot contain multiple scans",
                                ));
                            }
                            if all_components_in_first_scan {
                                if self.scan_eof_is_error() {
                                    return Err(DecodeErrors::FormatStatic(
                                        "Unexpected SOS after completed scan"
                                    ));
                                }
                                warn!("More than one SOS despite already having all components");
                                if let McuDecodeOutput::RawPlanes(raw_planes) = output {
                                    self.copy_raw_planes_for_mcu_stripe(i, raw_planes)?;
                                    self.invalidate_scan_checkpoint();
                                }
                                return Ok(());
                            }
                            continue 'sos;
                        }
                        // Hit EOI
                        break;
                    }
                    McuContinuation::Terminate(_) => {
                        warn!("Got terminate signal, will not process further");
                        if !all_components_in_first_scan {
                            break 'sos;
                        }
                        if let Some(v) = output
                            .pixels_mut()
                            .and_then(|p| p.get_mut(pixels_written..))
                        {
                            v.fill(128);
                        }
                        return Ok(());
                    }
                    McuContinuation::DnlFound(height) => {
                        debug_assert_eq!(self.resolved_dnl_height(), Some(height));
                        // DNL marker was consumed and info.height is now set.
                        // The image is complete; break out cleanly without
                        // grey-filling any remaining buffer space.
                        break 'sos;
                    }
                }
            }

            // A non-interleaved baseline image may place the next SOS right
            // after the final MCU row of the current scan. If the bitstream
            // did not discover that marker while decoding coefficients, look
            // for it before treating the image as complete.
            if !all_components_in_first_scan && !*stream.seen_eoi() {
                match self.get_baseline_marker(&mut stream) {
                    Ok(Marker::EOI) => {
                        *stream.seen_eoi() = true;
                        break;
                    }
                    Ok(Marker::SOS) => {
                        self.parse_marker_inner(Marker::SOS)?;
                        self.invalidate_scan_checkpoint();
                        stream.reset();
                        B::reset_arith_tables(&mut self.entropy_tables);
                        continue 'sos;
                    }
                    Ok(marker) => {
                        if self.advance_to_next_sos(
                            marker,
                            &mut stream,
                            all_required_components_seen,
                        )? {
                            continue 'sos;
                        }
                        break;
                    }
                    Err(e) if e.is_recoverable_eof() => {
                        if self.scan_eof_is_error() {
                            return Err(e);
                        }
                        break;
                    }
                    Err(e) => {
                        if self.options.strict_mode() {
                            return Err(e);
                        }
                        error!("{e}");
                        break;
                    }
                }
            }

            // Breaks if we get here, looping only if we have restarted, i.e. found another SOS and
            // continued at `'sos'.
            break;
        }

        // After the scan loop completes, check if the bitstream was over-read.
        // This catches the case where the final MCU row consumed data past EOF
        // (e.g. a 0xFF at the boundary was misinterpreted as byte-stuffing)
        // without a subsequent row to detect it.
        if stream.overread_by() > 0 && self.scan_eof_is_error() {
            return Err(DecodeErrors::ExhaustedData);
        }

        // If DNL is expected, try to consume the marker now. The bitstream
        // refiller reads ahead opportunistically, so we use get_marker() which
        // handles both the latched-marker case and raw-stream scanning.
        if self.expects_dnl {
            let scan_components = self.z_order;
            let scan_component_count = usize::from(self.num_scans);
            match self.get_baseline_marker(&mut stream) {
                Ok(Marker::DNL) => {
                    let length = self.stream.get_u16_be_err()?;
                    if length != 4 {
                        return Err(DecodeErrors::FormatStatic(
                            "Malformed DNL segment: length must be 4",
                        ));
                    }
                    let height = self.stream.get_u16_be_err()?;
                    self.validate_dnl_height(height)?;
                    self.validate_dnl_scan_rows(
                        height,
                        last_scan_rows_decoded,
                        all_components_in_first_scan,
                    )?;
                    self.resolve_dnl_height(height)?;
                    all_required_components_seen = self.mark_baseline_scan_components_complete(
                        &scan_components[..scan_component_count],
                    );
                    trace!("DNL marker: actual image height = {height}");
                }
                Ok(other) => {
                    return Err(DecodeErrors::Format(format!(
                        "Expected DNL marker after height-0 scan, got {other:?}"
                    )));
                }
                Err(_e) => {
                    return Err(DecodeErrors::FormatStatic(
                        "DNL marker expected (SOF height was 0) but not found in scan data"
                    ));
                }
            }
        }

        if !all_components_in_first_scan && !matches!(output, McuDecodeOutput::Scaled(_)) {
            self.finish_baseline_decoding(progressive_mcus, mcu_width, output)?;
        }

        // it may happen that some images don't have the whole buffer
        // so we can't panic in case of that
        // assert_eq!(pixels_written, pixels.len());

        // For UHD usecases that tie two images separating them with EOI and
        // SOI markers, it may happen that we do not reach this image end of image
        // So this ensures we reach it
        // Ensure we read EOI
        if !*stream.seen_eoi() {
            let marker = self.get_baseline_marker(&mut stream);
            match marker {
                Ok(Marker::EOI) => {
                    self.reassemble_extended_xmp();
                    *stream.seen_eoi() = true;
                    trace!("Found EOI marker");
                }
                Ok(
                    marker @ (Marker::DAC
                    | Marker::DHT
                    | Marker::DQT
                    | Marker::DRI
                    | Marker::COM
                    | Marker::APP(_)
                    | Marker::DNL),
                ) => {
                    if self.advance_to_next_sos(
                        marker,
                        &mut stream,
                        all_required_components_seen,
                    )? {
                        if self.dnl_image {
                            return Err(DecodeErrors::FormatStatic(
                                "JPEG with DNL cannot contain multiple scans",
                            ));
                        }
                        warn!("More than one SOS despite already having all components");
                    }
                }
                Ok(Marker::SOS) if self.dnl_image => {
                    return Err(DecodeErrors::FormatStatic(
                        "JPEG with DNL cannot contain multiple scans",
                    ));
                }
                Ok(marker) => {
                    if self.scan_eof_is_error() {
                        return Err(DecodeErrors::Format(format!(
                            "Unexpected marker {marker:?} after completed scan"
                        )));
                    }
                    // Preserve the previous lenient behavior for standalone
                    // markers such as an unexpected additional SOS.
                    trace!("Found marker {marker:?}");
                }
                Err(e) if e.is_recoverable_eof() => {
                    if self.scan_eof_is_error() {
                        return Err(e);
                    }
                    error!("{e}");
                }
                Err(e) => {
                    if self.options.strict_mode() {
                        return Err(e);
                    }
                    error!("{e}");
                }
            }
        }

        if all_components_in_first_scan {
            if let Some(final_stripe) = mcu_height.checked_sub(1) {
                if let McuDecodeOutput::RawPlanes(raw_planes) = output {
                    if raw_planes.requested_stripe == Some(final_stripe) {
                        self.copy_raw_planes_for_mcu_stripe(final_stripe, raw_planes)?;
                        self.invalidate_scan_checkpoint();
                    }
                }
            }
        }

        trace!("Finished decoding image");

        self.baseline_corrupt |= stream.has_corrupt_data();
        self.baseline_eoi_seen |= *stream.seen_eoi();
        output.mark_source_complete();

        Ok(())
    }

    fn get_baseline_marker<B: BitStream>(
        &mut self, stream: &mut B
    ) -> Result<Marker, DecodeErrors> {
        self.baseline_corrupt |= stream.has_corrupt_data();
        let (marker, discarded) = get_marker_with_discarded(&mut self.stream, stream)?;
        if discarded != 0 {
            // A warning raised while reaching EOI does not halt native output.
            if marker != Marker::EOI {
                self.baseline_corrupt = true;
            }
            warn!("Discarded {discarded} bytes before JPEG scan marker");
        }
        Ok(marker)
    }

    fn native_fast_huffman_window(&mut self, blocks: usize) -> Result<bool, DecodeErrors> {
        let length = blocks
            .checked_mul(DCT_BLOCK * 8)
            .ok_or(DecodeErrors::FormatStatic(
                "Huffman MCU input budget overflow"
            ))?;
        let bytes = match self.stream.peek_at(0, length) {
            Ok(bytes) => bytes,
            Err(error) if error.is_recoverable_eof() => return Ok(false),
            Err(error) => return Err(error.into())
        };
        // Native's fast MCU decoder does not warn for recovered Huffman codes.
        // Its 512-byte-per-block guard and an uninterrupted entropy window
        // guarantee that it does not fall back to the warning-producing path.
        Ok(bytes.last() != Some(&0xff)
            && !bytes.windows(2).any(|pair| pair[0] == 0xff && pair[1] != 0))
    }

    /// Process all MCUs when baseline decoding has been processing them component-after-component.
    /// Transform buffered coefficient blocks into the component stripes expected
    /// by the interleaved post processor.
    #[allow(clippy::too_many_lines)]
    #[allow(clippy::cast_sign_loss)]
    pub(crate) fn finish_baseline_decoding(
        &mut self, block: &[Vec<i32>; MAX_COMPONENTS], _mcu_width: usize,
        output: &mut McuDecodeOutput<'_, '_>
    ) -> Result<(), DecodeErrors> {
        let mcu_height = self.mcu_y;
        if output.is_raw()
            && self.baseline_seen_components[..self.components.len()].contains(&false)
        {
            return Err(DecodeErrors::ExhaustedData);
        }

        // Size of our output image(width*height)
        let raw_mode = output.is_raw();
        let is_hv = usize::from(self.is_interleaved && !raw_mode);
        let upsampler_scratch_size = is_hv
            * self
                .components
                .iter()
                .map(|x| x.width_stride)
                .max()
                .unwrap_or(0)
            * 8;
        let width = usize::from(self.info.width);
        let padded_width = calculate_padded_width(width, self.info.sample_ratio);

        let mut upsampler_scratch_space = vec![0; upsampler_scratch_size];

        for (pos, comp) in self.components.iter_mut().enumerate() {
            // Mark only needed components for computing output colors.
            //
            // Raw output needs every component regardless of output colorspace.
            comp.needed = raw_mode
                || min(
                    self.options.jpeg_get_out_colorspace().num_components() - 1,
                    pos
                ) == pos
                || self.input_colorspace == ColorSpace::YCCK
                || self.input_colorspace == ColorSpace::CMYK;
        }

        let mut pixels_written = 0;
        let stripe_start = output.requested_output_stripe().unwrap_or(0);
        let stripe_end = if output.requested_output_stripe().is_some() {
            core::cmp::min(stripe_start + 1, mcu_height)
        } else {
            mcu_height
        };

        let mut cancel = self.cancel_debounced(self.mcu_x);
        for i in stripe_start..stripe_end {
            if cancel.is_cancelled() {
                return Err(DecodeErrors::Cancelled);
            }
            // All the data is already in the right order, we just need to be able to pass it to
            // the post_process & upsample method. That expects all the data to be stored as one
            // row of MCUs in each component's `raw_coeff`.
            'component: for (position, component) in &mut self.components.iter_mut().enumerate() {
                if !component.needed {
                    continue 'component;
                }

                let blocks_wide = component.width_stride / 8;
                let blocks_high = component.vertical_sample;
                let mut coefficients = [0; 64];
                for row in 0..blocks_high {
                    for col in 0..blocks_wide {
                        let offset = ((i * blocks_high + row) * blocks_wide + col) * 64;
                        coefficients.copy_from_slice(&block[position][offset..][..64]);
                        let offset = row * 8 * component.width_stride + col * 8;
                        (self.idct_func)(
                            &mut coefficients,
                            &mut component.raw_coeff[offset..],
                            component.width_stride,
                        );
                    }
                }
            }

            // process that whole stripe of MCUs
            match output {
                McuDecodeOutput::Scaled(_) => {},
                McuDecodeOutput::Pixels(pixels) => self.post_process(
                    pixels,
                    width * self.options.jpeg_get_out_colorspace().num_components(),
                    i,
                    mcu_height,
                    width,
                    padded_width,
                    &mut pixels_written,
                    &mut upsampler_scratch_space
                )?,
                McuDecodeOutput::RawPlanes(raw_planes) => {
                    self.copy_raw_planes_for_mcu_stripe(i, raw_planes)?;
                }
                McuDecodeOutput::Scanlines(scanlines) => {
                    let mut stripe_written = 0;
                    self.post_process(
                        scanlines.pixels,
                        scanlines.stride,
                        i,
                        mcu_height,
                        width,
                        padded_width,
                        &mut stripe_written,
                        &mut upsampler_scratch_space
                    )?;
                    scanlines.rows_written = stripe_written / scanlines.stride;
                }
            }
        }

        output.mark_source_complete();

        return Ok(());
    }

    pub(crate) fn render_buffered_baseline_stripe(
        &mut self, output: &mut McuDecodeOutput<'_, '_>
    ) -> Result<(), DecodeErrors> {
        let block = core::mem::take(&mut self.baseline_coefficients);
        let result = self.finish_baseline_decoding(&block, 0, output);
        self.baseline_coefficients = block;
        result
    }

    fn decode_mcu_width<const PROGRESSIVE: bool, B: BitStream>(
        &mut self, context: &mut McuWidthContext<'_, B>
    ) -> Result<McuContinuation, DecodeErrors> {
        let is_one_by_one = self.num_scans == 1 || !self.scan_subsampled;

        // The definition of MCU depends on the sampling factor of involved scans. When components
        // have different factors then each Minimal-Coding-Unit is the least common multiple such
        // that we have an integer number of blocks from each component. But the decoding of these
        // components differs from it otherwise, we need an inner loop with a dynamic amount of
        // coefficients per component, whereas otherwise we have exactly one block of coefficients
        // encoded for each component in the bitstream order.
        //
        // We statically specialize on this to improve code generation of the common case a little
        // bit. We could also special case common sub-sampling cases but be mindful of code bloat.
        if is_one_by_one {
            self.inner_decode_mcu_width::<PROGRESSIVE, false, B>(context)
        } else {
            self.inner_decode_mcu_width::<PROGRESSIVE, true, B>(context)
        }
    }

    // Inline-never ensures we do get this function optimize on its own, into two different
    // versions, without the optimizer tripping up over the complexity that comes with the
    // constant folding. And constant folding is quite important for performance here as
    // when `not SAMPLED` then the inner loop has exactly one iteration per component in
    // the scan. The difference was ~1% or a bit more.
    #[allow(clippy::too_many_lines)]
    fn inner_decode_mcu_width<const PROGRESSIVE: bool, const SAMPLED: bool, B: BitStream>(
        &mut self, context: &mut McuWidthContext<'_, B>
    ) -> Result<McuContinuation, DecodeErrors> {
        // Destructure the context into local bindings up front. Reading the
        // hot loop through `context.<field>` keeps the optimizer from
        // treating the per-field mutable borrows as `noalias` and was
        // observed to cost ~1-2% on sub-sampled baseline benchmarks; pulling
        // them out here restores parity with the pre-refactor signature.
        let mcu_width = context.mcu_width;
        let mcu_row = context.mcu_row;
        let start_col = context.start_col;
        let ctx_pixels_written = context.pixels_written;
        let tmp: &mut [i32; 64] = &mut *context.tmp;
        let stream: &mut B = &mut *context.stream;
        let progressive: &mut [Vec<i32>; MAX_COMPONENTS] = &mut *context.progressive;
        let is_last_row = context.is_last_row;

        let z_order = self.z_order;
        let z_scans = &z_order[..usize::from(self.num_scans)];
        let blocks_in_mcu = z_scans
            .iter()
            .map(|&index| {
                if SAMPLED {
                    self.components[index].horizontal_sample
                        * self.components[index].vertical_sample
                } else {
                    1
                }
            })
            .sum();

        // How much of the head of `tmp` was written by the last MCU decoding? We only check for
        // two different cases and not all possible outcomes as this is only used to optimize the
        // bytes written in `fill`. Since the clobber happens in UNZIGZAG order we'd be straddling
        // most cache lines anyways even if we did a partial write with the exact length of the
        // coefficient data which was written into `tmp`.
        let mut clobber_more_than_4x4 = true;

        // A one-component scan consists of individual data units, regardless
        // of whether full-image buffering (PROGRESSIVE) is required.
        let scan_du_width = if self.num_scans == 1 {
            let k = z_scans[0];
            let comp = &self.components[k];
            // Calculate actual data units for this component: ceil(width / (8 * subsampling_ratio))
            (self.info.width as usize * comp.horizontal_sample).div_ceil(self.h_max * 8)
        } else {
            mcu_width
        };

        for j in start_col..scan_du_width {
            let native_fast_window = PROGRESSIVE && !self.baseline_corrupt
                && self.restart_interval == 0 && stream.marker().is_none()
                && self.native_fast_huffman_window(blocks_in_mcu)?;
            stream.begin_mcu();
            // iterate over components
            for &k in z_scans {
                // we made this loop body massive due to several different paths that depend on
                // static conditions. Note we (potentially) call into other functions so the
                // compiler will not unroll anything here anyways. The gains from separating
                // differently optimized loop bodies are much greater than a single additional jump
                // here.
                let component = &mut self.components[k];

                let (dc_table, ac_table) = B::get_dc_ac_tables(
                    &mut self.entropy_tables,
                    component.dc_huff_table % MAX_COMPONENTS,
                    component.ac_huff_table % MAX_COMPONENTS
                )?;

                let qt_table = &component.quantization_table;
                let component_samples_needed = component.needed;

                // If image is interleaved iterate over scan components,
                // otherwise if it-s non-interleaved, these routines iterate in
                // trivial scanline order(Y,Cb,Cr)
                //
                // Turn the bounds into a compile time constant for a common special case. This
                // allows the compiler to unroll the loop and then do a bunch of interleaving.
                //
                // A multi-component scan uses each component's sampling factors,
                // even when other frame components are absent from this scan.
                let v_step =
                    if SAMPLED { 0..component.vertical_sample } else { 0..1 };

                for v_samp in v_step {
                    let h_step =
                        if SAMPLED { 0..component.horizontal_sample } else { 0..1 };

                    for h_samp in h_step {
                        let (block_x, block_y) = if SAMPLED {
                            (
                                j * component.horizontal_sample + h_samp,
                                mcu_row * component.vertical_sample + v_samp,
                            )
                        } else {
                            (j, mcu_row)
                        };
                        let block_offset =
                            (block_y * (component.width_stride / 8) + block_x) * 64;
                        let result = if component_samples_needed {
                            // Fill the array with zeroes, decode_mcu_block expects
                            // a zero based array. Clobber is in zig-zag order though.
                            // Writing consecutive entries is basically free in terms
                            // of memory throughput so we opt for a larger power of
                            // two which lets the compiler turn this into a repeated
                            // write of a zeroed vector register, which does not have
                            // any branches, instead of a more difficult pattern where
                            // we attempt to overwrite exactly one coefficient.
                            let clobber_len = if clobber_more_than_4x4 { 64 } else { 32 };

                            if PROGRESSIVE {
                                // Sequential decoders do not clear coefficients
                                // between repeated component scans. EOB leaves
                                // previously transmitted AC coefficients intact.
                                let previous = progressive[k].get(block_offset..block_offset + 64)
                                    .ok_or(DecodeErrors::FormatStatic(
                                        "Invalid baseline block geometry"
                                    ))?;
                                tmp.copy_from_slice(previous);
                            } else {
                                tmp[..clobber_len].fill(0);
                            }

                            stream.decode_mcu_block(
                                &mut self.stream,
                                dc_table,
                                ac_table,
                                qt_table,
                                tmp,
                                &mut component.dc_pred,
                                &mut component.dc_diff
                            )
                        } else {
                            // We do not touch tmp so there is no need to reset it.
                            stream.discard_mcu_block(
                                &mut self.stream,
                                dc_table,
                                ac_table,
                                &mut component.dc_diff
                            )
                        };

                        // If an error occurs we can either propagate it
                        // as an error or print it and call terminate.
                        //
                        // This allows even corrupt images to render something,
                        // even if its bad, matching browsers.
                        //
                        // See example in https://github.com/etemesi254/zune-image/issues/293
                        let Ok(len) = result else {
                            let err = result.err().unwrap();
                            let moved_past_dnl = if let Some(position) = self.pending_dnl_stream_position {
                                self.stream.position()? != position
                            } else {
                                false
                            };
                            if self.pending_dnl_height.is_some()
                                && (moved_past_dnl || stream.marker().is_some() || stream.overread_by() > 0)
                            {
                                return Err(DecodeErrors::FormatStatic(
                                    "Entropy data ended before the height declared by DNL"
                                ));
                            }
                            // Always propagate ExhaustedData for incremental
                            // decoding support — the caller can retry with more
                            // data.
                            if err.is_recoverable_eof() {
                                return Err(err);
                            }
                            if self.stream.eof()? {
                                return Err(DecodeErrors::ExhaustedData);
                            }
                            self.baseline_corrupt = true;
                            return if self.options.strict_mode() {
                                Err(err)
                            } else {
                                error!("{}", err);
                                Ok(McuContinuation::Terminate(
                                    TerminationReason::CorruptEntropy,
                                ))
                            };
                        };

                        if component_samples_needed {
                            // tmp was only written partially, note that len is in ZigZag order.
                            clobber_more_than_4x4 = len > 10;

                            if PROGRESSIVE {
                                progressive[k][block_offset..][..64].copy_from_slice(tmp);
                                self.baseline_seen_components[k] = true;
                            }
                            if let Some(scaled) = context.scaled.as_deref_mut() {
                                scaled.write_block(k, block_x, block_y, tmp)?;
                                continue;
                            }
                            if PROGRESSIVE {
                                continue;
                            }

                            let idct_position = if !SAMPLED {
                                // For non-interleaved, j indexes data units directly
                                j * 8
                            } else {
                                // derived from stb and rewritten for my tastes
                                let c2 = v_samp * 8;
                                let c3 = ((j * component.horizontal_sample) + h_samp) * 8;

                                component.width_stride * c2 + c3
                            };

                            let idct_pos = component.raw_coeff.get_mut(idct_position..).unwrap();

                            if len <= 1 {
                                (self.idct_1x1_func)(tmp, idct_pos, component.width_stride);
                            } else if len <= 10 {
                                (self.idct_4x4_func)(tmp, idct_pos, component.width_stride);
                            } else {
                                //  call idct.
                                (self.idct_func)(tmp, idct_pos, component.width_stride);
                            }
                        }
                    }
                }
            }

            if self.pending_dnl_height.is_some() {
                let stream_moved = if let Some(position) = self.pending_dnl_stream_position {
                    self.stream.position()? != position
                } else {
                    false
                };
                if stream_moved || stream.marker().is_some() || stream.overread_by() > 0 {
                    return Err(DecodeErrors::FormatStatic(
                        "Entropy data ended before the height declared by DNL",
                    ));
                }
            }

            if native_fast_window && stream.marker().is_none() {
                stream.clear_huffman_code_warning();
            }
            self.baseline_corrupt |= stream.has_corrupt_data();
            self.todo = self.todo.wrapping_sub(1);

            if self.todo == 0 {
                let handled_restart = self.handle_rst_main_with_status(stream)?;
                // Buffered scans roll back whole rows on suspension, so their
                // coefficient writes and checkpoint must share that boundary.
                if handled_restart && !PROGRESSIVE && self.pending_dnl_height.is_none() {
                    // Coefficient buffers (`raw_coeff` and `progressive`)
                    // live on the decoder across `decode_into` calls, so
                    // they don't need to be snapshotted here. We only
                    // capture the per-component DC predictor state, which is
                    // zero immediately after `handle_rst` resets it.
                    let dc_predictions = core::array::from_fn(|idx| {
                        self.components
                            .get(idx)
                            .map_or((0, 0), |component| (component.dc_pred, component.dc_diff))
                    });
                    self.checkpoint_scan(mcu_row, j + 1, ctx_pixels_written, dc_predictions)?;
                }
                if !handled_restart && stream.marker().is_some() {
                    if matches!(stream.marker(), Some(Marker::DNL)) && j + 1 < mcu_width {
                        return Err(DecodeErrors::FormatStatic(
                            "DNL marker found before the end of an MCU row",
                        ));
                    }
                    break;
                }
                continue;
            }

            if !B::marker_ends_data() && matches!(stream.marker(), Some(Marker::DNL)) {
                if j + 1 < mcu_width {
                    return Err(DecodeErrors::FormatStatic(
                        "DNL marker found before the end of an MCU row",
                    ));
                }
                break;
            }

            if B::marker_ends_data() && stream.marker().is_some() && stream.bits_left() == 0 {
                if matches!(stream.marker(), Some(Marker::DNL)) && j + 1 < mcu_width {
                    return Err(DecodeErrors::FormatStatic(
                        "DNL marker found before the end of an MCU row",
                    ));
                }
                break;
            }
        }

        self.check_stream_marker_after_mcu_width(stream, is_last_row, mcu_row)
    }

    fn dnl_scan_rows(
        &self, height: u16, all_components_in_scan: bool,
    ) -> Result<usize, DecodeErrors> {
        let height = usize::from(height);
        let rows = if all_components_in_scan {
            height.div_ceil(self.mcu_height)
        } else {
            let component_index = self.z_order.first().copied().unwrap_or(0);
            let component = self.components.get(component_index).ok_or(
                DecodeErrors::FormatStatic("DNL scan references an unknown component"),
            )?;
            height
                .checked_mul(component.vertical_sample)
                .ok_or(DecodeErrors::FormatStatic("DNL scan height overflow"))?
                .div_ceil(self.v_max * 8)
        };
        Ok(rows)
    }

    fn validate_dnl_scan_rows(
        &self, height: u16, decoded_rows: usize, all_components_in_scan: bool,
    ) -> Result<(), DecodeErrors> {
        let expected_rows = self.dnl_scan_rows(height, all_components_in_scan)?;
        if decoded_rows != expected_rows {
            return Err(DecodeErrors::Format(format!(
                "DNL height {height} requires {expected_rows} MCU rows, decoded {decoded_rows}"
            )));
        }
        Ok(())
    }

    fn handle_pending_dnl(
        &mut self, mcu_row: usize,
    ) -> Result<McuContinuation, DecodeErrors> {
        let height = self
            .pending_dnl_height
            .ok_or(DecodeErrors::FormatStatic("Missing pending DNL height"))?;
        let decoded_rows = mcu_row + 1;
        let expected_rows = self.dnl_scan_rows(height, true)?;
        if decoded_rows < expected_rows {
            return Ok(McuContinuation::Ok);
        }
        if decoded_rows > expected_rows {
            return Err(DecodeErrors::Format(format!(
                "DNL height {height} requires {expected_rows} MCU rows, decoded {decoded_rows}"
            )));
        }
        self.pending_dnl_height = None;
        self.pending_dnl_stream_position = None;
        Ok(McuContinuation::DnlFound(height))
    }

    fn check_stream_marker_after_mcu_width<B: BitStream>(
        &mut self, stream: &mut B, is_last_row: bool, mcu_row: usize
    ) -> Result<McuContinuation, DecodeErrors> {
        // After all interleaved components, that's an MCU
        // handle stream markers
        //
        // In some corrupt images, it may occur that header markers occur in the stream.
        // The spec EXPLICITLY FORBIDS this, specifically, in
        // routine F.2.2.5  it says
        // `The only valid marker which may occur within the Huffman coded data is the RSTm marker.`
        //
        // But libjpeg-turbo allows it because of some weird reason. so I'll also
        // allow it because of some weird reason.
        if self.pending_dnl_height.is_some() {
            return self.handle_pending_dnl(mcu_row);
        }
        if let Some(m) = stream.marker() {
            if *m == Marker::DNL && self.dnl_image {
                self.check_cancelled()?;
                stream.marker().take();
                let length = self.stream.get_u16_be_err()?;
                if length != 4 {
                    return Err(DecodeErrors::FormatStatic(
                        "Malformed DNL segment: length must be 4",
                    ));
                }
                let height = self.stream.get_u16_be_err()?;
                if self.expects_dnl {
                    self.resolve_dnl_height(height)?;
                } else {
                    self.validate_dnl_height(height)?;
                }
                self.pending_dnl_height = Some(height);
                self.pending_dnl_stream_position = Some(self.stream.position()?);
                return self.handle_pending_dnl(mcu_row);
            }
            let ends_scan = matches!(
                m,
                Marker::SOS
                    | Marker::DAC
                    | Marker::DHT
                    | Marker::DQT
                    | Marker::DRI
                    | Marker::COM
                    | Marker::APP(_)
            );
            if (ends_scan || *m == Marker::EOI) && !is_last_row {
                return Ok(McuContinuation::Ok);
            }
            if *m == Marker::EOI {
                // acknowledge and ignore EOI marker.
                stream.marker().take();
                trace!("Found EOI marker");
                // Google Introduced the Ultra-HD image format which is basically
                // stitching two images into one container.
                // They basically separate two images via a EOI and SOI marker
                // so let's just ensure if we ever see EOI, we never read past that
                // ever.
                // https://github.com/google/libultrahdr
                *stream.seen_eoi() = true;
            } else if let Marker::RST(_) = m {
                // A latched RST means the entropy segment is already exhausted.
                self.handle_rst(stream)?;
            } else if let Marker::SOS = m {
                self.parse_marker_inner(Marker::SOS)?;
                self.invalidate_scan_checkpoint();
                stream.marker().take();
                stream.reset();
                B::reset_arith_tables(&mut self.entropy_tables);
                trace!("Found SOS marker");
                return Ok(McuContinuation::AnotherSos);
            } else if matches!(
                m,
                Marker::DAC | Marker::DHT | Marker::DQT | Marker::DRI | Marker::COM
            ) || matches!(m, Marker::APP(_))
            {
                // For non-interleaved images, setup markers can appear between scans.
                // Signal the caller to handle this marker and find the next SOS.
                // This keeps all marker parsing in the caller's loop.
                let m = stream.marker().take().unwrap();
                trace!("Found inter-scan marker {m:?}");
                return Ok(McuContinuation::InterScanMarker(m));
            } else if let Marker::DNL = m {
                // DNL on a non-DNL image remains invalid; the expected-DNL
                // path was consumed above before other marker dispatch.
                if self.options.strict_mode() {
                    return Err(DecodeErrors::Format(format!(
                        "Marker {:?} found where not expected", m
                    )));
                }
                error!("Unexpected DNL marker in Huffman stream, possibly corrupt jpeg");
                stream.marker().take();
                let length = self.stream.get_u16_be_err()?;
                let skip = usize::from(length).saturating_sub(2);
                self.stream.skip(skip)?;
                stream.reset();
                B::reset_arith_tables(&mut self.entropy_tables);
                return Ok(McuContinuation::Terminate(
                    TerminationReason::UnexpectedDnl,
                ));
            } else {
                if self.options.strict_mode() {
                    return Err(DecodeErrors::Format(format!(
                        "Marker {m:?} found where not expected"
                    )));
                }
                error!("Marker `{m:?}` Found within Huffman Stream, possibly corrupt jpeg");

                self.parse_marker_inner(*m)?;
                self.reassemble_extended_xmp();
                stream.marker().take();
                stream.reset();
                B::reset_arith_tables(&mut self.entropy_tables);
                return Ok(McuContinuation::Terminate(
                    TerminationReason::UnexpectedMarker,
                ));
            }
        }

        Ok(McuContinuation::Ok)
    }

    /// Scan for the next SOS marker, parsing setup markers along the way.
    ///
    /// This is the unified marker scanning function used after encountering an
    /// inter-scan marker. It handles DHT, DQT, DRI, COM, and APP markers that
    /// can appear between scans in non-interleaved images.
    ///
    /// # Arguments
    /// * `first_marker` - The first marker that was already detected (not yet parsed)
    /// * `stream` - The bitstream state
    /// * `final_scan` - Preserve lenient completion when no further scan is expected
    ///
    /// # Returns
    /// * `Ok(true)` - Found SOS, ready to continue decoding
    /// * `Ok(false)` - Found EOI, decoding complete
    /// * `Err(_)` - Error (too many markers, unexpected marker in strict mode, etc.)
    fn advance_to_next_sos<B: BitStream>(
        &mut self, first_marker: Marker, stream: &mut B, final_scan: bool
    ) -> Result<bool, DecodeErrors> {
        // Limit iterations to prevent DoS from malicious files.
        const MAX_INTER_SCAN_MARKERS: usize = 64;

        let inter_scan_snapshot = if self.scan_checkpoint().is_some() {
            Some((
                HeaderAppendStateSnapshot::capture(self),
                self.capture_scan_header_state()
            ))
        } else {
            None
        };

        // Keep the previous scan checkpoint valid until the next SOS is fully
        // parsed. If input ends between scans, rollback marker side effects so
        // the retry can resume from that checkpoint and parse the markers again.
        macro_rules! restore_inter_scan_on_suspend {
            ($result:expr) => {
                match $result {
                    Ok(value) => value,
                    Err(e) => {
                        if final_scan && e.is_recoverable_eof() && !self.scan_eof_is_error() {
                            self.reassemble_extended_xmp();
                            return Ok(false);
                        }
                        if e.is_recoverable_eof() || matches!(e, DecodeErrors::Cancelled) {
                            if let Some((append_snapshot, header_snapshot)) = &inter_scan_snapshot {
                                append_snapshot.rollback(self);
                                self.restore_scan_header_state(header_snapshot);
                            }
                        }
                        return Err(e);
                    }
                }
            };
        }

        // Parse the first marker that triggered this call
        restore_inter_scan_on_suspend!(self.parse_marker_inner(first_marker));
        stream.reset();
        B::reset_arith_tables(&mut self.entropy_tables);

        let mut markers_seen = 0;
        loop {
            if !final_scan && markers_seen >= MAX_INTER_SCAN_MARKERS {
                return Err(DecodeErrors::FormatStatic(
                    "Too many markers between scans (exceeded limit of 64)",
                ));
            }
            markers_seen += 1;
            restore_inter_scan_on_suspend!(self.check_cancelled());
            let marker = restore_inter_scan_on_suspend!(self.get_baseline_marker(stream));

            match marker {
                Marker::SOS => {
                    if markers_seen > MAX_INTER_SCAN_MARKERS {
                        return Err(DecodeErrors::FormatStatic(
                            "Too many markers between scans (exceeded limit of 64)",
                        ));
                    }
                    restore_inter_scan_on_suspend!(self.parse_marker_inner(Marker::SOS));
                    self.reassemble_extended_xmp();
                    self.invalidate_scan_checkpoint();
                    stream.reset();
                    B::reset_arith_tables(&mut self.entropy_tables);
                    trace!("Found SOS marker, continuing decode");
                    return Ok(true);
                }
                Marker::EOI => {
                    self.reassemble_extended_xmp();
                    *stream.seen_eoi() = true;
                    trace!("Found EOI marker");
                    return Ok(false);
                }
                Marker::DAC
                | Marker::DHT
                | Marker::DQT
                | Marker::DRI
                | Marker::COM
                | Marker::DNL => {
                    trace!("Parsing inter-scan marker {marker:?}");
                    restore_inter_scan_on_suspend!(self.parse_marker_inner(marker));
                }
                Marker::APP(_) => {
                    trace!("Parsing inter-scan APP marker {marker:?}");
                    restore_inter_scan_on_suspend!(self.parse_marker_inner(marker));
                }
                other => {
                    if final_scan {
                        if !self.scan_eof_is_error() {
                            warn!("Stopping after unexpected final marker {other:?}");
                            self.reassemble_extended_xmp();
                            return Ok(false);
                        }
                        if let Some((append_snapshot, header_snapshot)) = &inter_scan_snapshot {
                            append_snapshot.rollback(self);
                            self.restore_scan_header_state(header_snapshot);
                        }
                        return Err(DecodeErrors::Format(format!(
                            "Unexpected marker {other:?} after completed scan"
                        )));
                    }
                    if self.options.strict_mode() {
                        return Err(DecodeErrors::Format(format!(
                            "Unexpected marker {other:?} while scanning for SOS between scans"
                        )));
                    }
                    // Non-strict: skip unknown marker
                    warn!("Skipping unexpected marker {other:?} between scans");
                    let length = restore_inter_scan_on_suspend!(
                        self.stream.get_u16_be_err().map_err(DecodeErrors::IoErrors)
                    );
                    if length >= 2 {
                        restore_inter_scan_on_suspend!(
                            self.stream
                                .skip((length - 2) as usize)
                                .map_err(DecodeErrors::IoErrors)
                        );
                    }
                }
            }
        }
    }

    // handle RST markers.
    // No-op if not using restarts
    // this routine is shared with mcu_prog
    #[cold]
    pub(crate) fn handle_rst<B: BitStream>(&mut self, stream: &mut B) -> Result<(), DecodeErrors> {
        self.todo = self.restart_interval;

        if let Some(marker) = stream.marker() {
            // Found a marker
            // Read stream and see what marker is stored there
            match marker {
                Marker::RST(_) => {
                    // reset stream
                    stream.reset();
                    B::reset_arith_tables(&mut self.entropy_tables);
                    // Initialize dc predictions to zero for all components
                    self.components.iter_mut().for_each(|x| {
                        x.dc_pred = 0;
                        x.dc_diff = 0;
                    });
                    // Start iterating again. from position.
                }
                // Valid markers that can appear between scans at a restart boundary
                // (restart interval aligns with end of scan). Leave for caller.
                Marker::SOS
                | Marker::DAC
                | Marker::DHT
                | Marker::DQT
                | Marker::DRI
                | Marker::COM
                | Marker::APP(_)
                | Marker::DNL
                | Marker::EOI => {}
                _ => {
                    if self.options.strict_mode() {
                        return Err(DecodeErrors::MCUError(format!(
                            "Unexpected marker {marker:?} at restart boundary"
                        )));
                    }
                    warn!("Unexpected marker {marker:?} at restart boundary");
                }
            }
        }
        Ok(())
    }
    #[allow(clippy::too_many_lines, clippy::too_many_arguments)]
    pub(crate) fn post_process(
        &mut self, pixels: &mut [u8], output_stride: usize, i: usize, mcu_height: usize,
        width: usize, padded_width: usize, pixels_written: &mut usize,
        upsampler_scratch_space: &mut [i16]
    ) -> Result<(), DecodeErrors> {
        let out_colorspace_components = self.options.jpeg_get_out_colorspace().num_components();
        let row_bytes = width
            .checked_mul(out_colorspace_components)
            .ok_or(DecodeErrors::FormatStatic("output row size overflow"))?;
        if output_stride < row_bytes {
            return Err(DecodeErrors::FormatStatic(
                "output stride is smaller than the converted row width"
            ));
        }

        let mut px = *pixels_written;
        if self.h_max == 0 || self.v_max == 0 {
            return Err(DecodeErrors::FormatStatic("Invalid component sampling"));
        }
        for component in &mut self.components {
            if component.raw_coeff.is_empty() {
                continue;
            }
            let visible_width = (width * component.horizontal_sample).div_ceil(self.h_max);
            let visible_height =
                (usize::from(self.info.height) * component.vertical_sample).div_ceil(self.v_max);
            let stride = component.width_stride;
            if visible_width == 0
                || visible_width > stride
                || component.raw_coeff.len() % stride != 0
            {
                return Err(DecodeErrors::FormatStatic(
                    "Invalid component reconstruction dimensions"
                ));
            }
            let rows = component.raw_coeff.len() / stride;
            // DNL leaves the height unknown, but horizontal edges are already known.
            let valid_rows = if self.expects_dnl {
                rows
            } else {
                visible_height.saturating_sub(i * rows).min(rows)
            };
            if valid_rows == 0 {
                continue;
            }
            for row in component
                .raw_coeff
                .chunks_exact_mut(stride)
                .take(valid_rows)
            {
                let edge = row[visible_width - 1];
                row[visible_width..].fill(edge);
            }
            for row in valid_rows..rows {
                component
                    .raw_coeff
                    .copy_within((valid_rows - 1) * stride..valid_rows * stride, row * stride);
            }
        }
        // indicates whether image is vertically up-sampled
        let is_vertically_sampled = self
            .components
            .iter()
            .any(|c| c.sample_ratio == SampleRatios::HV || c.sample_ratio == SampleRatios::V);

        let mut comp_len = self.components.len();

        // If we are moving from YCbCr -> Luma, we do not allocate storage for other components, so we
        // will panic when we are trying to read samples, so for that case,
        // hardcode it so that we  don't panic when doing
        //   *samp = &samples[j][pos * padded_width..(pos + 1) * padded_width]
        if out_colorspace_components < comp_len && self.options.jpeg_get_out_colorspace() == Luma {
            comp_len = out_colorspace_components;
        }
        let mut color_conv_function =
            |num_iters: usize, samples: [&[i16]; 4]| -> Result<(), DecodeErrors> {
                for (pos, output) in pixels[px..]
                    .chunks_exact_mut(output_stride)
                    .take(num_iters)
                    .enumerate()
                {
                    let mut raw_samples: [&[i16]; 4] = [&[], &[], &[], &[]];

                    // iterate over each line, since color-convert needs only
                    // one line
                    for (j, samp) in raw_samples.iter_mut().enumerate().take(comp_len) {
                        let temp = &samples[j].get(pos * padded_width..(pos + 1) * padded_width);
                        if temp.is_none() {
                            return Err(DecodeErrors::FormatStatic("Missing samples"));
                        }
                        *samp = temp.unwrap();
                    }
                    color_convert(
                        &raw_samples,
                        self.color_convert_16,
                        self.input_colorspace,
                        self.options.jpeg_get_out_colorspace(),
                        &mut output[..row_bytes],
                        width,
                        padded_width
                    )?;
                    px += output_stride;
                }
                Ok(())
            };

        let comps = &mut self.components[..];

        if self.is_interleaved && self.options.jpeg_get_out_colorspace() != ColorSpace::Luma {
            for comp in comps.iter_mut() {
                upsample(
                    comp,
                    mcu_height,
                    i,
                    upsampler_scratch_space,
                    is_vertically_sampled
                )?;
            }

            if is_vertically_sampled {
                if i > 0 {
                    // write the last line, it wasn't  up-sampled as we didn't have row_down
                    // yet
                    let mut samples: [&[i16]; 4] = [&[], &[], &[], &[]];

                    for (samp, component) in samples.iter_mut().zip(comps.iter()) {
                        *samp = &component.first_row_upsample_dest;
                    }

                    // ensure length matches for all samples
                    let _first_len = samples[0].len();

                    // This was a good check, but can be caused to panic, esp on invalid/corrupt images.
                    // See one in issue https://github.com/etemesi254/zune-image/issues/262, so for now
                    // we just ignore and generate invalid images at the end.

                    //
                    //
                    // for samp in samples.iter().take(comp_len) {
                    //     assert_eq!(first_len, samp.len());
                    // }
                    let num_iters = self.coeff * self.v_max;

                    color_conv_function(num_iters, samples)?;
                }

                // After up-sampling the last row, save  any row that can be used for
                // a later up-sampling,
                //
                // E.g the Y sample is not sampled but we haven't finished upsampling the last row of
                // the previous mcu, since we don't have the down row, so save it
                for component in comps.iter_mut() {
                    // Components the output colorspace does not use are not decoded, so
                    // they have no rows to carry over.
                    if !matches!(
                        component.sample_ratio,
                        SampleRatios::H | SampleRatios::Generic(_, _)
                    ) && component.needed
                    {
                        // H and Generic sampling preserve their already-upsampled rows in worker.

                        // copy last row to be used for the  next color conversion
                        let size = component.vertical_sample
                            * component.width_stride
                            * component.sample_ratio.sample();

                        let last_bytes =
                            component.raw_coeff.rchunks_exact_mut(size).next().unwrap();

                        component
                            .first_row_upsample_dest
                            .copy_from_slice(last_bytes);
                    }
                }
            }

            let mut samples: [&[i16]; 4] = [&[], &[], &[], &[]];

            for (samp, component) in samples.iter_mut().zip(comps.iter()) {
                *samp = if component.sample_ratio == SampleRatios::None {
                    &component.raw_coeff
                } else {
                    &component.upsample_dest
                };
            }

            // we either do 7 or 8 MCU's depending on the state, this only applies to
            // vertically sampled images
            //
            // for rows up until the last MCU, we do not upsample the last stride of the MCU
            // which means that the number of iterations should take that into account is one less the
            // up-sampled size
            //
            // For the last MCU, we upsample the last stride, meaning that if we hit the last MCU, we
            // should sample full raw coeffs
            let is_last_considered = is_vertically_sampled && (i != mcu_height.saturating_sub(1));

            let num_iters = (8 - usize::from(is_last_considered)) * self.coeff * self.v_max;

            color_conv_function(num_iters, samples)?;
        } else {
            let mut channels_ref: [&[i16]; MAX_COMPONENTS] = [&[]; MAX_COMPONENTS];

            self.components
                .iter()
                .enumerate()
                .for_each(|(pos, x)| channels_ref[pos] = &x.raw_coeff);

            if let SampleRatios::Generic(_, v) = self.info.sample_ratio {
                color_conv_function(8 * v * self.coeff, channels_ref)?;
            } else {
                color_conv_function(8 * self.coeff, channels_ref)?;
            }
        }

        *pixels_written = px;
        Ok(())
    }
}

enum McuContinuation {
    Ok,
    AnotherSos,
    /// Found an inter-scan marker (DHT/DQT/DRI/COM/APP) that needs handling.
    /// The caller should parse it and scan for the next SOS.
    InterScanMarker(Marker),
    Terminate(TerminationReason),
    /// The DNL marker was found and parsed. The scan is complete and
    /// `info.height` has been updated to the actual number of lines.
    DnlFound(u16)
}

enum TerminationReason {
    CorruptEntropy,
    UnexpectedDnl,
    UnexpectedMarker,
}

impl TerminationReason {
    fn raw_error(&self) -> DecodeErrors {
        let message = match self {
            Self::CorruptEntropy => "Terminal entropy corruption during raw output",
            Self::UnexpectedDnl => "Unexpected DNL marker during raw output",
            Self::UnexpectedMarker => "Unexpected marker during raw output",
        };
        DecodeErrors::FormatStatic(message)
    }
}
