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
use crate::decoder::{HeaderAppendStateSnapshot, MAX_COMPONENTS};
use crate::errors::DecodeErrors;
use crate::marker::Marker;
use crate::mcu_prog::get_marker;
use crate::misc::{calculate_padded_width, setup_component_params};
use crate::worker::{color_convert, upsample};
use crate::JpegDecoder;

/// The size of a DC block for a MCU.
pub const DCT_BLOCK: usize = 64;

struct McuWidthContext<'a, B: BitStream> {
    mcu_width: usize,
    // Current MCU row, used when indexing progressive scratch buffers and checkpoints.
    mcu_row: usize,
    // First MCU column to decode in this row; non-zero when resuming from a restart checkpoint.
    start_col: usize,
    // Number of output bytes already committed before this MCU row.
    pixels_written: usize,
    // Shared coefficient scratch block reused for each decoded data unit.
    tmp: &'a mut [i32; 64],
    // Entropy decoder state for the current scan.
    stream: &'a mut B,
    // Full-image coefficient buffers for multi-SOS baseline scans.
    progressive: &'a mut [Vec<i16>; MAX_COMPONENTS],
}

impl<T: ZByteReaderTrait> JpegDecoder<T> {
    /// Check for existence of DC and AC Huffman Tables
    pub(crate) fn check_tables<B: BitStream>(&mut self) -> Result<(), DecodeErrors> {
        // check that dc and AC tables exist outside the hot path
        for component in &self.components {
            let _ = B::get_dc_ac_tables(
                &mut self.entropy_tables,
                component.dc_huff_table,
                component.ac_huff_table,
            )?;
        }
        Ok(())
    }

    /// Decode MCUs and carry out post processing.
    ///
    /// This is the main decoder loop for the library, the hot path.
    pub(crate) fn decode_mcu_ycbcr_baseline<B: BitStream>(
        &mut self, pixels: &mut [u8],
    ) -> Result<(), DecodeErrors> {
        // Move the persistent multi-SOS coefficient buffer out of `self` so
        // the inner decoder can borrow it mutably while still calling methods
        // on `self`. The buffer is put back on return and survives across
        // `decode_into` retries, which is what lets scan checkpoints stay
        // allocation-free.
        let mut progressive_mcus = core::mem::take(&mut self.progressive_mcus_buffer);
        let result = self.decode_mcu_ycbcr_baseline_inner::<B>(pixels, &mut progressive_mcus);
        self.progressive_mcus_buffer = progressive_mcus;
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
        &mut self, pixels: &mut [u8], progressive_mcus: &mut [Vec<i16>; MAX_COMPONENTS],
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
            mcu_height = (self.info.height as usize).div_ceil(8);
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
            mcu_height = self.info.height.div_ceil(8) as usize;
        }
        let width = usize::from(self.info.width);

        let padded_width = calculate_padded_width(width, self.info.sample_ratio);

        let mut stream = B::new();

        self.check_tables::<B>()?;

        let mut tmp = [0_i32; DCT_BLOCK];

        let comp_len = self.components.len();
        // True when we are resuming from a previously-saved scan checkpoint;
        // in that case the per-component `raw_coeff` and `progressive_mcus`
        // buffers still hold the data from the prior `decode_into` call and
        // must not be re-zeroed. On a fresh decode they are (re-)allocated.
        let resuming = self.scan_checkpoint().is_some();

        for (pos, comp) in self.components.iter_mut().enumerate() {
            // Allocate only needed components.
            //
            // For special colorspaces i.e YCCK and CMYK, just allocate all of the needed
            // components.
            if min(
                self.options.jpeg_get_out_colorspace().num_components() - 1,
                pos,
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
        let all_components_in_first_scan = usize::from(self.num_scans) == self.components.len();

        if all_components_in_first_scan {
            // No multi-SOS scratch is needed for this call; release any
            // retained `progressive_mcus_buffer` capacity so it doesn't pin
            // memory between decodes.
            for mcu in progressive_mcus.iter_mut() {
                *mcu = Vec::new();
            }
        } else {
            for (component, mcu) in self.components.iter().zip(progressive_mcus.iter_mut()) {
                let len = mcu_width
                    * component.vertical_sample
                    * component.horizontal_sample
                    * mcu_height
                    * 64;
                if !resuming || mcu.len() != len {
                    mcu.clear();
                    mcu.resize(len, 0);
                }
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

        let is_hv = usize::from(self.is_interleaved);
        let upsampler_scratch_size = is_hv
            * self
                .components
                .iter()
                .map(|x| x.width_stride)
                .max()
                .unwrap_or(0)
            * 8;
        let mut upsampler_scratch_space = vec![0; upsampler_scratch_size];

        'sos: loop {
            trace!(
                "Baseline decoding of components: {:?}",
                &self.z_order[..usize::from(self.num_scans)]
            );

            trace!("Decoding MCU width: {mcu_width}, height: {mcu_height}");

            // Consume the resume position once into locals so later SOS scans
            // start at row 0, and so we don't mutate the loop's range bound
            // from inside the loop below.
            let current_resume_row = resume_row;
            let current_resume_col = resume_col;
            resume_row = 0;
            resume_col = 0;

            let scan_du_height = if all_components_in_first_scan {
                mcu_height
            } else {
                let k = self.z_order.first().copied().unwrap_or(0);
                if let Some(comp) = self.components.get(k) {
                    (self.info.height as usize * comp.vertical_sample).div_ceil(self.v_max * 8)
                } else {
                    mcu_height
                }
            };

            let mut cancel = self.cancel_debounced(mcu_width);
            for i in current_resume_row..scan_du_height {
                let start_col = if i == current_resume_row {
                    current_resume_col
                } else {
                    0
                };
                if stream.overread_by() > 0 {
                    // The bitstream reader has exhausted available data.
                    // Return a recoverable error so the caller can feed more
                    // bytes and retry (incremental decoding).
                    return Err(DecodeErrors::ExhaustedData);
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
                        bs_state,
                    )?;
                }

                // decode a whole MCU width,
                // this takes into account interleaved components.
                let mut mcu_width_context = McuWidthContext {
                    mcu_width,
                    mcu_row: i,
                    start_col,
                    pixels_written,
                    tmp: &mut tmp,
                    stream: &mut stream,
                    progressive: &mut *progressive_mcus,
                };
                if cancel.is_cancelled() {
                    return Err(DecodeErrors::Cancelled);
                }
                let terminate = if all_components_in_first_scan {
                    self.decode_mcu_width::<false, B>(&mut mcu_width_context)?
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

                    self.decode_mcu_width::<true, B>(&mut mcu_width_context)?
                };

                // process that width up until it's impossible. This is faster than allocation the
                // full components, which we skipped earlier.
                if all_components_in_first_scan {
                    self.post_process(
                        pixels,
                        i,
                        mcu_height,
                        width,
                        padded_width,
                        &mut pixels_written,
                        &mut upsampler_scratch_space,
                    )?;
                    self.pixels_decoded = pixels_written;
                    // This row's coefficient buffers can be reused next, so
                    // any checkpoint inside the row is no longer valid.
                    self.invalidate_scan_checkpoint();
                }

                match terminate {
                    McuContinuation::Ok => {}
                    McuContinuation::AnotherSos if all_components_in_first_scan => {
                        warn!("More than one SOS despite already having all components");
                        return Ok(());
                    }
                    McuContinuation::AnotherSos => continue 'sos,
                    McuContinuation::InterScanMarker(marker) => {
                        // Handle inter-scan markers (DHT/DQT/etc) uniformly here.
                        // This keeps all marker handling in the outer loop.
                        if self.advance_to_next_sos(marker, &mut stream)? {
                            continue 'sos;
                        }
                        // Hit EOI
                        break;
                    }
                    McuContinuation::Terminate => {
                        warn!("Got terminate signal, will not process further");
                        if let Some(v) = pixels.get_mut(pixels_written..) {
                            v.fill(128);
                        }
                        return Ok(());
                    }
                    McuContinuation::DnlFound => {
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
                match get_marker(&mut self.stream, &mut stream) {
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
                        if self.advance_to_next_sos(marker, &mut stream)? {
                            continue 'sos;
                        }
                        break;
                    }
                    Err(e) if e.is_recoverable_eof() => {
                        return Err(e);
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
        if stream.overread_by() > 0 {
            return Err(DecodeErrors::ExhaustedData);
        }

        // If DNL is expected, try to consume the marker now. The bitstream
        // refiller reads ahead opportunistically, so we use get_marker() which
        // handles both the latched-marker case and raw-stream scanning.
        if self.expects_dnl {
            match get_marker(&mut self.stream, &mut stream) {
                Ok(Marker::DNL) => {
                    let _length = self.stream.get_u16_be_err()?;
                    let height = self.stream.get_u16_be_err()?;
                    self.info.set_height(height);
                    self.expects_dnl = false;
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

        if !all_components_in_first_scan {
            self.finish_baseline_decoding(progressive_mcus, mcu_width, pixels)?;
        }

        // it may happen that some images don't have the whole buffer
        // so we can't panic in case of that
        // assert_eq!(pixels_written, pixels.len());

        // For UHD usecases that tie two images separating them with EOI and
        // SOI markers, it may happen that we do not reach this image end of image
        // So this ensures we reach it
        // Ensure we read EOI
        if !*stream.seen_eoi() {
            let marker = get_marker(&mut self.stream, &mut stream);
            match marker {
                Ok(_m) => {
                    trace!("Found marker {_m:?}");
                }
                Err(e) if e.is_recoverable_eof() => {
                    return Err(e);
                }
                Err(e) => {
                    if self.options.strict_mode() {
                        return Err(e);
                    }
                    error!("{e}");
                }
            }
        }

        trace!("Finished decoding image");

        Ok(())
    }

    /// Process all MCUs when baseline decoding has been processing them component-after-component.
    /// For simplicity this assembles the dequantized blocks in the order that the post processing
    /// of an interleaved baseline decoding would use.
    #[allow(clippy::too_many_lines)]
    #[allow(clippy::cast_sign_loss)]
    pub(crate) fn finish_baseline_decoding(
        &mut self, block: &[Vec<i16>; MAX_COMPONENTS], _mcu_width: usize, pixels: &mut [u8],
    ) -> Result<(), DecodeErrors> {
        let mcu_height = self.mcu_y;

        // Size of our output image(width*height)
        let is_hv = usize::from(self.is_interleaved);
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
            comp.needed = min(
                self.options.jpeg_get_out_colorspace().num_components() - 1,
                pos,
            ) == pos
                || self.input_colorspace == ColorSpace::YCCK
                || self.input_colorspace == ColorSpace::CMYK;
        }

        let mut pixels_written = 0;

        // dequantize and idct have been performed, only color convert.
        for i in 0..mcu_height {
            // All the data is already in the right order, we just need to be able to pass it to
            // the post_process & upsample method. That expects all the data to be stored as one
            // row of MCUs in each component's `raw_coeff`.
            'component: for (position, component) in &mut self.components.iter_mut().enumerate() {
                if !component.needed {
                    continue 'component;
                }

                // step is the number of pixels this iteration wil be handling
                // Given by the number of mcu's height and the length of the component block
                // Since the component block contains the whole channel as raw pixels
                // we this evenly divides the pixels into MCU blocks
                //
                // For interleaved images, this gives us the exact pixels comprising a whole MCU
                // block
                let step = block[position].len() / mcu_height;

                // where we will be reading our pixels from.
                let slice = &block[position][i * step..][..step];
                let temp_channel = &mut component.raw_coeff;
                temp_channel[..step].copy_from_slice(slice);
            }

            // process that whole stripe of MCUs
            self.post_process(
                pixels,
                i,
                mcu_height,
                width,
                padded_width,
                &mut pixels_written,
                &mut upsampler_scratch_space,
            )?;
        }

        return Ok(());
    }

    fn decode_mcu_width<const PROGRESSIVE: bool, B: BitStream>(
        &mut self, context: &mut McuWidthContext<'_, B>,
    ) -> Result<McuContinuation, DecodeErrors> {
        let is_one_by_one = !self.scan_subsampled;

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
        &mut self, context: &mut McuWidthContext<'_, B>,
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
        let progressive: &mut [Vec<i16>; MAX_COMPONENTS] = &mut *context.progressive;

        let z_order = self.z_order;
        let z_scans = &z_order[..usize::from(self.num_scans)];

        // How much of the head of `tmp` was written by the last MCU decoding? We only check for
        // two different cases and not all possible outcomes as this is only used to optimize the
        // bytes written in `fill`. Since the clobber happens in UNZIGZAG order we'd be straddling
        // most cache lines anyways even if we did a partial write with the exact length of the
        // coefficient data which was written into `tmp`.
        let mut clobber_more_than_4x4 = true;

        // For non-interleaved scans (PROGRESSIVE=true), each scan contains a single component
        // and we iterate over that component's actual data unit count, not the interleaved MCU
        // width multiplied by sampling factor.
        let mut scan_du_width = if PROGRESSIVE {
            let k = z_scans[0];
            let comp = &self.components[k];
            // Calculate actual data units for this component: ceil(width / (8 * subsampling_ratio))
            (self.info.width as usize * comp.horizontal_sample).div_ceil(self.h_max * 8)
        } else {
            mcu_width
        };
        // In malformed scans that list multiple components, clamp to the smallest row capacity
        // to avoid writing past the row buffer.
        if PROGRESSIVE && z_scans.len() > 1 {
            let min_du = z_scans
                .iter()
                .map(|&k| self.components[k].width_stride / 8)
                .min()
                .unwrap_or(0);
            scan_du_width = scan_du_width.min(min_du);
        }

        for j in start_col..scan_du_width {
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
                    component.ac_huff_table % MAX_COMPONENTS,
                )?;

                let qt_table = &component.quantization_table;
                let channel = if PROGRESSIVE {
                    let offset = mcu_row
                        .checked_mul(component.width_stride)
                        .and_then(|x| x.checked_mul(8))
                        .ok_or(DecodeErrors::FormatStatic("Overflow"))?;
                    // Small stopgap for https://github.com/etemesi254/zune-image/issues/362
                    if offset >= progressive[k].len() {
                        return Err(DecodeErrors::FormatStatic("Would panic on slice iteration"));
                    }
                    &mut progressive[k][offset..]
                } else {
                    &mut component.raw_coeff
                };

                let component_samples_needed = component.needed;

                // If image is interleaved iterate over scan components,
                // otherwise if it-s non-interleaved, these routines iterate in
                // trivial scanline order(Y,Cb,Cr)
                //
                // Turn the bounds into a compile time constant for a common special case. This
                // allows the compiler to unroll the loop and then do a bunch of interleaving.
                //
                // For PROGRESSIVE (non-interleaved), we iterate data units directly so
                // h_samp/v_samp loops run exactly once.
                let v_step =
                    if SAMPLED && !PROGRESSIVE { 0..component.vertical_sample } else { 0..1 };

                for v_samp in v_step {
                    let h_step =
                        if SAMPLED && !PROGRESSIVE { 0..component.horizontal_sample } else { 0..1 };

                    for h_samp in h_step {
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

                            tmp[..clobber_len].fill(0);

                            stream.decode_mcu_block(
                                &mut self.stream,
                                dc_table,
                                ac_table,
                                qt_table,
                                tmp,
                                &mut component.dc_pred,
                                &mut component.dc_diff,
                            )
                        } else {
                            // We do not touch tmp so there is no need to reset it.
                            stream.discard_mcu_block(
                                &mut self.stream,
                                dc_table,
                                ac_table,
                                &mut component.dc_diff,
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
                            // Always propagate ExhaustedData for incremental
                            // decoding support — the caller can retry with more
                            // data.
                            if err.is_recoverable_eof() {
                                return Err(err);
                            }
                            if self.stream.eof()? {
                                return Err(DecodeErrors::ExhaustedData);
                            }
                            return if self.options.strict_mode() {
                                Err(err)
                            } else {
                                error!("{}", err);
                                Ok(McuContinuation::Terminate)
                            };
                        };

                        if component_samples_needed {
                            // tmp was only written partially, note that len is in ZigZag order.
                            clobber_more_than_4x4 = len > 10;

                            let idct_position = if PROGRESSIVE {
                                // For non-interleaved, j indexes data units directly
                                j * 8
                            } else {
                                // derived from stb and rewritten for my tastes
                                let c2 = v_samp * 8;
                                let c3 = ((j * component.horizontal_sample) + h_samp) * 8;

                                component.width_stride * c2 + c3
                            };

                            let idct_pos = channel.get_mut(idct_position..).unwrap();

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

            self.todo = self.todo.wrapping_sub(1);

            if self.todo == 0 {
                if self.handle_rst_main_with_status(stream)? {
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
                continue;
            }

            if stream.marker().is_some() && stream.bits_left() == 0 {
                break;
            }
        }

        self.check_stream_marker_after_mcu_width(stream)
    }

    fn check_stream_marker_after_mcu_width<B: BitStream>(
        &mut self, stream: &mut B,
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
        if let Some(m) = stream.marker() {
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
                // DNL appears right after the last entropy-coded row. It
                // carries the actual line count for images whose SOF height
                // was 0. Only act on it when we were expecting one; otherwise
                // treat it as an unexpected marker.
                if self.expects_dnl {
                    let _m = stream.marker().take().unwrap();
                    // Consume the DNL segment: 2-byte length (always 4) + 2-byte height.
                    // We read directly from the stream because the bitstream
                    // reader has already drained the entropy data.
                    let _length = self.stream.get_u16_be_err()?;
                    let height = self.stream.get_u16_be_err()?;
                    self.info.set_height(height);
                    self.expects_dnl = false;
                    trace!("DNL marker: actual image height = {height}");
                    return Ok(McuContinuation::DnlFound);
                } else {
                    // Spurious DNL on a normal image — warn and terminate
                    // (parsing it would silently corrupt info.height).\n                    // Swallow the segment body so the stream stays consistent.
                    if self.options.strict_mode() {
                        return Err(DecodeErrors::Format(format!(
                            "Marker {:?} found where not expected", m
                        )));
                    }
                    error!("Unexpected DNL marker in Huffman stream, possibly corrupt jpeg");
                    stream.marker().take();
                    // Skip the DNL body (length-prefixed: read length, skip payload).
                    let length = self.stream.get_u16_be_err()?;
                    let skip = usize::from(length).saturating_sub(2);
                    self.stream.skip(skip)?;
                    stream.reset();
                    B::reset_arith_tables(&mut self.entropy_tables);
                    return Ok(McuContinuation::Terminate);
                }
            } else {
                if self.options.strict_mode() {
                    return Err(DecodeErrors::Format(format!(
                        "Marker {m:?} found where not expected"
                    )));
                }
                error!("Marker `{m:?}` Found within Huffman Stream, possibly corrupt jpeg");

                self.parse_marker_inner(*m)?;
                stream.marker().take();
                stream.reset();
                B::reset_arith_tables(&mut self.entropy_tables);
                return Ok(McuContinuation::Terminate);
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
    ///
    /// # Returns
    /// * `Ok(true)` - Found SOS, ready to continue decoding
    /// * `Ok(false)` - Found EOI, decoding complete
    /// * `Err(_)` - Error (too many markers, unexpected marker in strict mode, etc.)
    fn advance_to_next_sos<B: BitStream>(
        &mut self, first_marker: Marker, stream: &mut B,
    ) -> Result<bool, DecodeErrors> {
        // Limit iterations to prevent DoS from malicious files.
        const MAX_INTER_SCAN_MARKERS: usize = 64;

        let inter_scan_snapshot = if self.scan_checkpoint().is_some() {
            Some((
                HeaderAppendStateSnapshot::capture(self),
                self.capture_scan_header_state(),
            ))
        } else {
            None
        };

        // Keep the previous scan checkpoint valid until the next SOS is fully
        // parsed. If input ends between scans, rollback marker side effects so
        // the retry can resume from that checkpoint and parse the markers again.
        macro_rules! restore_inter_scan_on_eof {
            ($result:expr) => {
                match $result {
                    Ok(value) => value,
                    Err(e) => {
                        if e.is_recoverable_eof() {
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
        restore_inter_scan_on_eof!(self.parse_marker_inner(first_marker));
        stream.reset();
        B::reset_arith_tables(&mut self.entropy_tables);

        for _ in 0..MAX_INTER_SCAN_MARKERS {
            let marker = restore_inter_scan_on_eof!(get_marker(&mut self.stream, stream));

            match marker {
                Marker::SOS => {
                    restore_inter_scan_on_eof!(self.parse_marker_inner(Marker::SOS));
                    self.invalidate_scan_checkpoint();
                    stream.reset();
                    B::reset_arith_tables(&mut self.entropy_tables);
                    trace!("Found SOS marker, continuing decode");
                    return Ok(true);
                }
                Marker::EOI => {
                    *stream.seen_eoi() = true;
                    trace!("Found EOI marker");
                    return Ok(false);
                }
                Marker::DAC | Marker::DHT | Marker::DQT | Marker::DRI | Marker::COM => {
                    trace!("Parsing inter-scan marker {marker:?}");
                    restore_inter_scan_on_eof!(self.parse_marker_inner(marker));
                }
                Marker::APP(_) => {
                    trace!("Parsing inter-scan APP marker {marker:?}");
                    restore_inter_scan_on_eof!(self.parse_marker_inner(marker));
                }
                other => {
                    if self.options.strict_mode() {
                        return Err(DecodeErrors::Format(format!(
                            "Unexpected marker {other:?} while scanning for SOS between scans"
                        )));
                    }
                    // Non-strict: skip unknown marker
                    warn!("Skipping unexpected marker {other:?} between scans");
                    let length = restore_inter_scan_on_eof!(
                        self.stream.get_u16_be_err().map_err(DecodeErrors::IoErrors)
                    );
                    if length >= 2 {
                        restore_inter_scan_on_eof!(
                            self.stream
                                .skip((length - 2) as usize)
                                .map_err(DecodeErrors::IoErrors)
                        );
                    }
                }
            }
        }

        Err(DecodeErrors::FormatStatic(
            "Too many markers between scans (exceeded limit of 64)",
        ))
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
        &mut self, pixels: &mut [u8], i: usize, mcu_height: usize, width: usize,
        padded_width: usize, pixels_written: &mut usize, upsampler_scratch_space: &mut [i16],
    ) -> Result<(), DecodeErrors> {
        let out_colorspace_components = self.options.jpeg_get_out_colorspace().num_components();

        let mut px = *pixels_written;
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
                    .chunks_exact_mut(width * out_colorspace_components)
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
                        output,
                        width,
                        padded_width,
                    )?;
                    px += width * out_colorspace_components;
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
                    is_vertically_sampled,
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
                    if component.sample_ratio != SampleRatios::H {
                        // We don't care about H sampling factors, since it's copied in the workers function

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
    Terminate,
    /// The DNL marker was found and parsed. The scan is complete and
    /// `info.height` has been updated to the actual number of lines.
    DnlFound
}
