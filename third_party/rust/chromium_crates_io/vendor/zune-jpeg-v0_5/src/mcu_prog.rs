/*
 * Copyright (c) 2023.
 *
 * This software is free software;
 *
 * You can redistribute it or modify it under terms of the MIT, Apache License or Zlib license
 */

//!Routines for progressive decoding

use alloc::string::ToString;
use alloc::vec::Vec;
use alloc::{format, vec};
use core::cmp::min;

use zune_core::bytestream::{ZByteReaderTrait, ZReader};
use zune_core::colorspace::ColorSpace;
use zune_core::log::{error, trace, warn};

use crate::bitstream::BitStream;
use crate::components::SampleRatios;
use crate::decoder::{JpegDecoder, MAX_COMPONENTS};
use crate::errors::DecodeErrors;
use crate::headers::parse_sos;
use crate::marker::Marker;
use crate::mcu::DCT_BLOCK;
use crate::misc::{calculate_padded_width, setup_component_params};

impl<T: ZByteReaderTrait> JpegDecoder<T> {
    /// Decode a progressive image
    ///
    /// This routine decodes a progressive image, stopping if it finds any error.
    ///
    /// Completed progressive scans are committed into the decoder-owned
    /// coefficient buffer. When incremental scan preservation is enabled, the
    /// current scan decodes into a scratch copy so a recoverable EOF can expose
    /// the last completed scan without reapplying partially decoded refinement
    /// data on retry.
    #[allow(
        clippy::needless_range_loop,
        clippy::cast_sign_loss,
        clippy::redundant_else,
        clippy::too_many_lines
    )]
    #[inline(never)]
    pub(crate) fn decode_mcu_ycbcr_progressive<B: BitStream>(
        &mut self, pixels: &mut [u8],
    ) -> Result<(), DecodeErrors> {
        // Move the coefficient buffers out so scan helpers can borrow `self`
        // while receiving the buffers separately.
        let mut block = core::mem::take(&mut self.progressive_mcus_buffer);
        let result = self.decode_mcu_ycbcr_progressive_inner::<B>(pixels, &mut block);
        self.progressive_mcus_buffer = block;
        result
    }

    #[allow(
        clippy::needless_range_loop,
        clippy::cast_sign_loss,
        clippy::redundant_else,
        clippy::too_many_lines
    )]
    fn decode_mcu_ycbcr_progressive_inner<B: BitStream>(
        &mut self, pixels: &mut [u8], block: &mut [Vec<i16>; MAX_COMPONENTS]
    ) -> Result<(), DecodeErrors> {
        setup_component_params(self)?;
        let mut mcu_height;
        let mut mcu_width;

        if self.input_colorspace == ColorSpace::Luma && self.is_interleaved {
            warn!("Grayscale image with down-sampled component, resetting component details");
            self.reset_params();
        }

        if self.is_interleaved {
            // this helps us catch component errors.
            self.set_upsampling();
        }
        if self.is_interleaved {
            mcu_width = self.mcu_x;
            mcu_height = self.mcu_y;
        } else {
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

        mcu_width *= 64;

        if self.progressive_scan_checkpoint().is_none() {
            self.progressive_completed_scans = 0;
        }
        for (i, comp) in self.components.iter().enumerate() {
            let len = mcu_width * comp.vertical_sample * comp.horizontal_sample * mcu_height;
            if block[i].len() != len {
                block[i].clear();
                block[i].resize(len, 0);
            } else if self.progressive_completed_scans == 0 {
                block[i].fill(0);
            }
        }

        let mut stream = B::new_progressive(self.succ_low, self.spec_start, self.spec_end);

        let preserve_progressive_scans = self.mcu_checkpoints_enabled;

        if !self.decode_progressive_scan(&mut stream, block, pixels, preserve_progressive_scans)? {
            return self.finish_progressive_decoding(block, pixels);
        }
        if self.progressive_completed_scans > self.options.jpeg_get_max_scans() {
            return Err(DecodeErrors::Format(format!(
                "Too many scans, exceeded limit of {}",
                self.options.jpeg_get_max_scans()
            )));
        }

        let mut marker = match get_marker(&mut self.stream, &mut stream) {
            Ok(marker) => marker,
            Err(e) => {
                return self.handle_progressive_inter_scan_error(
                    e,
                    block,
                    pixels,
                    preserve_progressive_scans
                )
            }
        };

        // If marker is EOI, we are done; otherwise continue scanning.
        'eoi: while marker != Marker::EOI {
            match marker {
                Marker::SOS => {
                    if let Err(e) = parse_sos(self) {
                        return self.handle_progressive_inter_scan_error(
                            e,
                            block,
                            pixels,
                            preserve_progressive_scans
                        );
                    }

                    stream.update_progressive_params(
                        self.succ_high,
                        self.succ_low,
                        self.spec_start,
                        self.spec_end,
                    );
                    if !self.decode_progressive_scan(
                        &mut stream,
                        block,
                        pixels,
                        preserve_progressive_scans
                    )? {
                        break 'eoi;
                    }

                    if self.progressive_completed_scans > self.options.jpeg_get_max_scans() {
                        return Err(DecodeErrors::Format(format!(
                            "Too many scans, exceeded limit of {}",
                            self.options.jpeg_get_max_scans()
                        )));
                    }

                    match get_marker(&mut self.stream, &mut stream) {
                        Ok(marker_n) => {
                            marker = marker_n;
                            stream.reset();
                            B::reset_arith_tables(&mut self.entropy_tables);
                            continue 'eoi;
                        }
                        Err(e) => {
                            return self.handle_progressive_inter_scan_error(
                                e,
                                block,
                                pixels,
                                preserve_progressive_scans
                            )
                        }
                    }
                }
                Marker::RST(_n) => {
                    self.handle_rst(&mut stream)?;
                }
                _ => {
                    if let Err(e) = self.parse_marker_inner(marker) {
                        return self.handle_progressive_inter_scan_error(
                            e,
                            block,
                            pixels,
                            preserve_progressive_scans
                        );
                    }
                }
            }

            match get_marker(&mut self.stream, &mut stream) {
                Ok(marker_n) => {
                    marker = marker_n;
                }
                Err(e) => {
                    return self.handle_progressive_inter_scan_error(
                        e,
                        block,
                        pixels,
                        preserve_progressive_scans
                    )
                }
            }
        }

        self.finish_progressive_decoding(block, pixels)
    }

    fn decode_progressive_scan<B: BitStream>(
        &mut self, stream: &mut B, block: &mut [Vec<i16>; MAX_COMPONENTS], pixels: &mut [u8],
        use_scratch_coefficients: bool
    ) -> Result<bool, DecodeErrors> {
        if !use_scratch_coefficients {
            // The default first decode attempt takes this path and updates the
            // existing progressive buffers directly without cloning them.
            return self.decode_progressive_scan_direct(stream, block);
        }

        // Incremental preservation was explicitly enabled for the first attempt,
        // or this decoder is being retried. Clone only the components touched by
        // the current scan so recoverable EOF can discard partial updates.
        self.checkpoint_progressive_scan(self.progressive_completed_scans)?;
        let mut touched_components = [false; MAX_COMPONENTS];
        for scan_index in 0..usize::from(self.num_scans) {
            let component = self.z_order[scan_index];
            if component < MAX_COMPONENTS {
                touched_components[component] = true;
            }
        }
        let mut scan_block: [Vec<i16>; MAX_COMPONENTS] = core::array::from_fn(|idx| {
            if touched_components[idx] {
                block[idx].clone()
            } else {
                Vec::new()
            }
        });
        let result = self.parse_entropy_coded_data(stream, &mut scan_block);

        if let Err(e) = result {
            // Completed scans are displayable, but a truncated scan is not:
            // refinement passes read-modify-write coefficient data, so the
            // scratch copy is discarded on recoverable EOF.
            if e.is_recoverable_eof() {
                self.finish_progressive_partial(block, pixels)?;
                return Err(e);
            }
            if self.stream.eof()? {
                self.finish_progressive_partial(block, pixels)?;
                return Err(DecodeErrors::ExhaustedData);
            }
            if self.options.strict_mode() {
                return Err(e);
            }
            error!("{e}");
            // Match the direct path's best-effort non-strict output: keep
            // partial corrupt-scan coefficients, but never for recoverable EOF.
            for idx in 0..MAX_COMPONENTS {
                if touched_components[idx] {
                    core::mem::swap(&mut block[idx], &mut scan_block[idx]);
                }
            }
            self.invalidate_progressive_scan_checkpoint();
            return Ok(false);
        }
        if stream.overread_by() > 0 {
            self.finish_progressive_partial(block, pixels)?;
            return Err(DecodeErrors::ExhaustedData);
        }

        for idx in 0..MAX_COMPONENTS {
            if touched_components[idx] {
                core::mem::swap(&mut block[idx], &mut scan_block[idx]);
            }
        }
        self.progressive_completed_scans += 1;
        self.invalidate_progressive_scan_checkpoint();
        Ok(true)
    }

    fn decode_progressive_scan_direct<B: BitStream>(
        &mut self, stream: &mut B, block: &mut [Vec<i16>; MAX_COMPONENTS]
    ) -> Result<bool, DecodeErrors> {
        let result = self.parse_entropy_coded_data(stream, block);

        if let Err(e) = result {
            if e.is_recoverable_eof() {
                self.discard_progressive_partial();
                return Err(e);
            }
            if self.stream.eof()? {
                self.discard_progressive_partial();
                return Err(DecodeErrors::ExhaustedData);
            }
            if self.options.strict_mode() {
                return Err(e);
            }
            error!("{e}");
            return Ok(false);
        }
        if stream.overread_by() > 0 {
            self.discard_progressive_partial();
            return Err(DecodeErrors::ExhaustedData);
        }

        self.progressive_completed_scans += 1;
        Ok(true)
    }

    fn handle_progressive_inter_scan_error(
        &mut self, error: DecodeErrors, block: &[Vec<i16>; MAX_COMPONENTS], pixels: &mut [u8],
        preserve_completed_scans: bool
    ) -> Result<(), DecodeErrors> {
        if error.is_recoverable_eof() {
            if preserve_completed_scans {
                self.finish_progressive_partial(block, pixels)?;
            } else {
                self.discard_progressive_partial();
            }
            return Err(error);
        }
        if self.options.strict_mode() {
            return Err(error);
        }
        error!("{error}");
        self.finish_progressive_decoding(block, pixels)
    }

    fn discard_progressive_partial(&mut self) {
        self.invalidate_progressive_scan_checkpoint();
        self.progressive_completed_scans = 0;
        self.progressive_displayed_scans = 0;
        self.pixels_decoded = 0;
    }

    fn finish_progressive_partial(
        &mut self, block: &[Vec<i16>; MAX_COMPONENTS], pixels: &mut [u8]
    ) -> Result<(), DecodeErrors> {
        if self.progressive_completed_scans == 0 {
            self.pixels_decoded = 0;
            self.progressive_displayed_scans = 0;
            return Ok(());
        }

        if self.progressive_displayed_scans != self.progressive_completed_scans {
            self.finish_progressive_decoding(block, pixels)?;
            self.progressive_displayed_scans = self.progressive_completed_scans;
        }
        self.pixels_decoded = 0;
        Ok(())
    }

    /// Reset progressive parameters
    fn reset_prog_params<B: BitStream>(&mut self, stream: &mut B) {
        stream.reset();
        B::reset_arith_tables(&mut self.entropy_tables);
        self.components.iter_mut().for_each(|x| {
            x.dc_pred = 0;
            x.dc_diff = 0;
        });

        // Also reset JPEG restart intervals
        self.todo = if self.restart_interval != 0 { self.restart_interval } else { usize::MAX };
    }
    /// Parse a progressive scan's entropy-coded data into `buffer`.
    ///
    /// Progressive scans always start at MCU `(0, 0)`. Per-RST checkpoints
    /// are not recorded for progressive: refine passes read-modify-write
    /// `buffer`, so retries restart at the current scan boundary using the
    /// last committed coefficient buffer.
    #[allow(clippy::too_many_lines, clippy::cast_sign_loss)]
    fn parse_entropy_coded_data<B: BitStream>(
        &mut self, stream: &mut B, buffer: &mut [Vec<i16>; MAX_COMPONENTS],
    ) -> Result<(), DecodeErrors> {
        self.reset_prog_params(stream);

        if usize::from(self.num_scans) > self.input_colorspace.num_components() {
            return Err(DecodeErrors::Format(format!(
                "Number of scans {} cannot be greater than number of components, {}",
                self.num_scans,
                self.input_colorspace.num_components()
            )));
        }

        if self.num_scans == 1 {
            // Safety checks
            if self.spec_end != 0 && self.spec_start == 0 {
                return Err(DecodeErrors::FormatStatic(
                    "Can't merge DC and AC corrupt jpeg",
                ));
            }

            let k = self.z_order[0];
            if k >= self.components.len() {
                return Err(DecodeErrors::Format(format!(
                    "Cannot find component {k}, corrupt image"
                )));
            }

            // Dispatch depending on type
            if self.spec_start == 0 {
                if self.succ_high == 0 {
                    self.parse_dc_first_non_interleaved(stream, buffer, k)?;
                } else {
                    self.parse_dc_refine_non_interleaved(stream, buffer, k)?;
                }
            } else if self.succ_high == 0 {
                self.parse_ac_first_non_interleaved(stream, buffer, k)?;
            } else {
                self.parse_ac_refine_non_interleaved(stream, buffer, k)?;
            }
        } else {
            if self.spec_end != 0 {
                return Err(DecodeErrors::HuffmanDecode(
                    "Can't merge dc and AC corrupt jpeg".to_string(),
                ));
            }

            // Verify components and tables for interleaved scans
            for k in 0..self.num_scans {
                let n = self.z_order[k as usize];
                if n >= self.components.len() {
                    return Err(DecodeErrors::Format(format!(
                        "Cannot find component {n}, corrupt image"
                    )));
                }
                let component = &mut self.components[n];
                let _ = B::get_dc_table(&mut self.entropy_tables, component.dc_huff_table)?;
            }

            if self.succ_high == 0 {
                self.parse_dc_first_interleaved(stream, buffer)?;
            } else {
                self.parse_dc_refine_interleaved(stream, buffer)?;
            }
        }
        Ok(())
    }

    #[inline(always)]
    fn get_non_interleaved_dimensions(&self, k: usize) -> (usize, usize) {
        let component = &self.components[k];
        let mcu_width =
            (self.info.width as usize * component.horizontal_sample).div_ceil(self.h_max * 8);
        let mcu_height =
            (self.info.height as usize * component.vertical_sample).div_ceil(self.v_max * 8);
        (mcu_width, mcu_height)
    }

    fn parse_dc_first_non_interleaved<B: BitStream>(
        &mut self, stream: &mut B, buffer: &mut [Vec<i16>; MAX_COMPONENTS], k: usize,
    ) -> Result<(), DecodeErrors> {
        let (mcu_width, mcu_height) = self.get_non_interleaved_dimensions(k);
        let dc_pos = self.components[k].dc_huff_table / MAX_COMPONENTS;
        let width_stride = self.components[k].width_stride / 8;

        let mut cancel = self.cancel_debounced(mcu_width);
        for i in 0..mcu_height {
            if cancel.is_cancelled() {
                return Err(DecodeErrors::Cancelled);
            }
            for j in 0..mcu_width {
                let start = 64 * (j + i * width_stride);
                let dc_pred_opt: Option<&mut i16> = buffer[k].get_mut(start);

                if let Some(dc_pred) = dc_pred_opt {
                    let dc_table = B::get_dc_table(&mut self.entropy_tables, dc_pos)?;
                    let component = &mut self.components[k];

                    stream.decode_prog_dc_first(
                        &mut self.stream,
                        dc_table,
                        dc_pred,
                        &mut component.dc_pred,
                        &mut component.dc_diff,
                    )?;

                    self.todo -= 1;
                    self.handle_rst_main(stream)?;
                }
            }
        }
        Ok(())
    }

    fn parse_dc_refine_non_interleaved<B: BitStream>(
        &mut self, stream: &mut B, buffer: &mut [Vec<i16>; MAX_COMPONENTS], k: usize,
    ) -> Result<(), DecodeErrors> {
        let (mcu_width, mcu_height) = self.get_non_interleaved_dimensions(k);
        let width_stride = self.components[k].width_stride / 8;
        let component_buffer = &mut buffer[k];

        let mut cancel = self.cancel_debounced(mcu_width);
        for i in 0..mcu_height {
            if cancel.is_cancelled() {
                return Err(DecodeErrors::Cancelled);
            }
            for j in 0..mcu_width {
                let start = 64 * (j + i * width_stride);
                let dc_pred_id: Option<&mut i16> = component_buffer.get_mut(start);

                if let Some(dc_pred) = dc_pred_id {
                    stream.decode_prog_dc_refine(&mut self.stream, dc_pred)?;
                    self.todo -= 1;
                    self.handle_rst_main(stream)?;
                }
            }
        }
        Ok(())
    }

    fn parse_ac_first_non_interleaved<B: BitStream>(
        &mut self, stream: &mut B, buffer: &mut [Vec<i16>; MAX_COMPONENTS], k: usize,
    ) -> Result<(), DecodeErrors> {
        let (mcu_width, mcu_height) = self.get_non_interleaved_dimensions(k);
        let ac_pos = self.components[k].ac_huff_table;
        let component_buffer_data = &mut buffer[k];

        let mut cancel = self.cancel_debounced(mcu_width);
        for i in 0..mcu_height {
            if cancel.is_cancelled() {
                return Err(DecodeErrors::Cancelled);
            }
            for j in 0..mcu_width {
                if *stream.eob_run() > 0 {
                    // handle EOB runs here.
                    *stream.eob_run() -= 1;
                } else {
                    let start = 64 * (j + i * (self.components[k].width_stride / 8));

                    let data: &mut [i16; 64] = component_buffer_data
                        .get_mut(start..start + 64)
                        .ok_or(DecodeErrors::FormatStatic("Slice to Small"))?
                        .try_into()
                        .unwrap();

                    let ac_table = B::get_ac_table(&mut self.entropy_tables, ac_pos)?;
                    if !stream.decode_mcu_ac_first(&mut self.stream, ac_table, data)? {
                        // Arithmetic bad-code termination is scan-wide, matching
                        // libjpeg's no-op handling for the remaining MCUs.
                        return Ok(());
                    }
                }

                self.todo -= 1;
                self.handle_rst_main(stream)?;
            }
        }
        Ok(())
    }

    fn parse_ac_refine_non_interleaved<B: BitStream>(
        &mut self, stream: &mut B, buffer: &mut [Vec<i16>; MAX_COMPONENTS], k: usize,
    ) -> Result<(), DecodeErrors> {
        let (mcu_width, mcu_height) = self.get_non_interleaved_dimensions(k);
        let ac_pos = self.components[k].ac_huff_table;
        let component_buffer_data = &mut buffer[k];
        let width_stride = self.components[k].width_stride / 8;

        let mut cancel = self.cancel_debounced(mcu_width);
        for i in 0..mcu_height {
            if cancel.is_cancelled() {
                return Err(DecodeErrors::Cancelled);
            }
            for j in 0..mcu_width {
                let start = 64 * (j + i * width_stride);
                let data: &mut [i16; 64] = component_buffer_data
                    .get_mut(start..start + 64)
                    .ok_or(DecodeErrors::FormatStatic("Slice to Small"))?
                    .try_into()
                    .unwrap();

                let ac_table = B::get_ac_table(&mut self.entropy_tables, ac_pos)?;
                stream.decode_mcu_ac_refine(&mut self.stream, ac_table, data)?;

                self.todo -= 1;
                self.handle_rst_main(stream)?;
            }
        }
        Ok(())
    }

    fn parse_dc_first_interleaved<B: BitStream>(
        &mut self, stream: &mut B, buffer: &mut [Vec<i16>; MAX_COMPONENTS],
    ) -> Result<(), DecodeErrors> {
        let mut cancel = self.cancel_debounced(self.mcu_x);
        for i in 0..self.mcu_y {
            if cancel.is_cancelled() {
                return Err(DecodeErrors::Cancelled);
            }
            for j in 0..self.mcu_x {
                for k in 0..self.num_scans {
                    let n = self.z_order[k as usize];
                    let component = &mut self.components[n];
                    let huff_table =
                        B::get_dc_table(&mut self.entropy_tables, component.dc_huff_table)?;

                    for v_samp in 0..component.vertical_sample {
                        for h_samp in 0..component.horizontal_sample {
                            let x2 = j * component.horizontal_sample + h_samp;
                            let y2 = i * component.vertical_sample + v_samp;
                            let position = 64 * (x2 + y2 * component.width_stride / 8);

                            let Some(data) = buffer[n].get_mut(position) else {
                                return Err(DecodeErrors::FormatStatic("Invalid image"));
                            };

                            stream.decode_prog_dc_first(
                                &mut self.stream,
                                huff_table,
                                data,
                                &mut component.dc_pred,
                                &mut component.dc_diff,
                            )?;
                        }
                    }
                }
                self.todo -= 1;
                self.handle_rst_main(stream)?;
            }
        }
        Ok(())
    }

    fn parse_dc_refine_interleaved<B: BitStream>(
        &mut self, stream: &mut B, buffer: &mut [Vec<i16>; MAX_COMPONENTS],
    ) -> Result<(), DecodeErrors> {
        let mut cancel = self.cancel_debounced(self.mcu_x);
        for i in 0..self.mcu_y {
            if cancel.is_cancelled() {
                return Err(DecodeErrors::Cancelled);
            }
            for j in 0..self.mcu_x {
                for k in 0..self.num_scans {
                    let n = self.z_order[k as usize];
                    let component = &self.components[n];

                    for v_samp in 0..component.vertical_sample {
                        for h_samp in 0..component.horizontal_sample {
                            let x2 = j * component.horizontal_sample + h_samp;
                            let y2 = i * component.vertical_sample + v_samp;
                            let position = 64 * (x2 + y2 * component.width_stride / 8);

                            let Some(data) = buffer[n].get_mut(position) else {
                                return Err(DecodeErrors::FormatStatic("Invalid image"));
                            };

                            stream.decode_prog_dc_refine(&mut self.stream, data)?;
                        }
                    }
                }
                self.todo -= 1;

                // Progressive does not record per-RST checkpoints —
                // refine scans do read-modify-write on `buffer` and
                // mid-scan resume would re-apply partial deltas. On
                // EOF the decoder falls back to scan-start replay.
                self.handle_rst_main(stream)?;
            }
        }
        Ok(())
    }

    /// Handle an RST marker mid-scan, if one is due. Used by the progressive
    /// scan decoder, which does not record per-RST checkpoints and therefore
    /// does not need to know whether a marker was actually consumed.
    ///
    /// The baseline decoder, which does checkpoint, calls
    /// [`Self::handle_rst_main_with_status`] instead.
    #[allow(clippy::used_underscore_binding)]
    pub(crate) fn handle_rst_main<B: BitStream>(
        &mut self, stream: &mut B,
    ) -> Result<(), DecodeErrors> {
        self.handle_rst_main_inner(stream).map(|_| ())
    }

    fn handle_rst_main_inner<B: BitStream>(
        &mut self, stream: &mut B
    ) -> Result<bool, DecodeErrors> {
        if self.todo == 0 {
            stream.refill(&mut self.stream)?;
        }

        let mut handled_restart = false;

        if self.todo == 0
            && self.restart_interval != 0
            && stream.marker().is_none()
            && !*stream.seen_eoi()
        {
            // if no marker and we are to reset RST, look for the marker, this matches
            // libjpeg-turbo behaviour and allows us to decode images in
            // https://github.com/etemesi254/zune-image/issues/261
            let _start = self.stream.position()?;
            // skip bytes until we find marker
            let marker = get_marker(&mut self.stream, stream);

            // In some images, the RST marker on the last section may not be available
            // as it is maybe stopped by an EOI marker, see in the case of https://github.com/etemesi254/zune-image/issues/292
            // what happened was that we would go looking for the RST marker exhausting all the data
            // in the image and this would return an error, so for now
            // translate it to a warning, but return the image decoded up
            // until that point
            match marker {
                Ok(marker) => {
                    let _end = self.stream.position()?;
                    handled_restart = matches!(marker, Marker::RST(_));
                    *stream.marker() = Some(marker);
                    // NB some warnings may be false positives.
                    warn!(
                        "{} Extraneous bytes before marker {:?}",
                        _end - _start,
                        marker
                    );
                }
                Err(ref e) if e.is_recoverable_eof() => {
                    return Err(DecodeErrors::ExhaustedData);
                }
                Err(_) => {
                    warn!("RST marker was not found, where expected, image may be garbled");
                }
            }
        }
        if self.todo == 0 {
            // Lookahead skipped above (marker was already latched by the
            // bitstream refill); inspect the latched marker so the RST
            // status reflects what `handle_rst` is about to consume.
            handled_restart |= matches!(stream.marker(), Some(Marker::RST(_)));
            self.handle_rst(stream)?;
        }
        Ok(handled_restart)
    }

    /// Variant of [`Self::handle_rst_main`] used by the baseline scan decoder
    /// to detect whether the consumed marker was actually an RST. Returns
    /// `true` when `todo` had wrapped to 0 *and* a real RST marker was found
    /// at the entropy-segment boundary, signalling a safe place to take a
    /// resumability checkpoint. Returns `false` otherwise.
    ///
    /// Only baseline calls this; progressive uses the lighter
    /// [`Self::handle_rst_main`] above so its per-MCU codegen stays
    /// bit-identical to the pre-PR shape.
    ///
    /// Equivalent to the pre-refactor logic that inspected the latched
    /// marker slot *after* `handle_rst_main` returned: RST consumption
    /// empties that slot, so "slot is `None` or RST after" matches
    /// "latched marker was RST before" in every reachable state. The
    /// `restart_interval == 0` branch never sets `todo = 0` (the scan loop
    /// uses `0x7fff_ffff` as the sentinel), so the one corner case where
    /// the two formulations could disagree — `todo == 0`, no marker, no
    /// interval — is unreachable.
    pub(crate) fn handle_rst_main_with_status<B: BitStream>(
        &mut self, stream: &mut B,
    ) -> Result<bool, DecodeErrors> {
        let was_due = self.todo == 0;
        let handled_restart = self.handle_rst_main_inner(stream)?;
        Ok(was_due && handled_restart)
    }
    #[allow(clippy::too_many_lines)]
    #[allow(clippy::needless_range_loop, clippy::cast_sign_loss)]
    fn finish_progressive_decoding(
        &mut self, block: &[Vec<i16>; MAX_COMPONENTS], pixels: &mut [u8],
    ) -> Result<(), DecodeErrors> {
        // This function is complicated because we need to replicate
        // the function in mcu.rs
        //
        // The advantage is that we do very little allocation and very lot
        // channel reusing.
        // The trick is to notice that we repeat the same procedure per MCU
        // width.
        //
        // So we can set it up that we only allocate temporary storage large enough
        // to store a single mcu width, then reuse it per invocation.
        //
        // This is advantageous to us.
        //
        // Remember we need to have the whole MCU buffer so we store 3 unprocessed
        // channels in memory, and then we allocate the whole output buffer in memory, both of
        // which are huge.
        //
        //

        let mcu_height = if self.is_interleaved {
            self.mcu_y
        } else {
            // For non-interleaved images( (1*1) subsampling)
            // number of MCU's are the widths (+7 to account for paddings) divided by 8.
            self.info.height.div_ceil(8) as usize
        };

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
        let mut tmp = [0_i32; DCT_BLOCK];

        for (pos, comp) in self.components.iter_mut().enumerate() {
            // Allocate only needed components.
            //
            // For special colorspaces i.e YCCK and CMYK, just allocate all of the needed
            // components.
            if min(
                self.options.jpeg_get_out_colorspace().num_components() - 1,
                pos,
            ) == pos
                || self.input_colorspace == ColorSpace::YCCK
                || self.input_colorspace == ColorSpace::CMYK
            {
                // allocate enough space to hold a whole MCU width
                // this means we should take into account sampling ratios
                // `*8` is because each MCU spans 8 widths.
                let len = comp.width_stride * comp.vertical_sample * 8;

                comp.needed = true;
                comp.raw_coeff = vec![0; len];
            } else {
                comp.needed = false;
            }
        }

        let mut pixels_written = 0;

        // dequantize, idct and color convert.
        let mut cancel = self.cancel_debounced(self.mcu_x);
        for i in 0..mcu_height {
            if cancel.is_cancelled() {
                return Err(DecodeErrors::Cancelled);
            }
            'component: for (position, component) in &mut self.components.iter_mut().enumerate() {
                if !component.needed {
                    continue 'component;
                }
                let qt_table = &component.quantization_table;

                // step is the number of pixels this iteration wil be handling
                // Given by the number of mcu's height and the length of the component block
                // Since the component block contains the whole channel as raw pixels
                // we this evenly divides the pixels into MCU blocks
                //
                // For interleaved images, this gives us the exact pixels comprising a whole MCU
                // block
                let step = block[position].len() / mcu_height;
                // where we will be reading our pixels from.
                let start = i * step;

                let slice = &block[position][start..start + step];

                let temp_channel = &mut component.raw_coeff;

                // The next logical step is to iterate width wise.
                // To figure out how many pixels we iterate by we use effective pixels
                // Given to us by component.x
                // iterate per effective pixels.
                let mcu_x = component.width_stride / 8;

                // iterate per every vertical sample.
                for k in 0..component.vertical_sample {
                    for j in 0..mcu_x {
                        // after writing a single stride, we need to skip 8 rows.
                        // This does the row calculation
                        let width_stride = k * 8 * component.width_stride;
                        let start = j * 64 + width_stride;

                        // See https://github.com/etemesi254/zune-image/issues/262 sample 3.
                        let Some(qt_slice) = slice.get(start..start + 64) else {
                            return Err(DecodeErrors::FormatStatic(
                                "Invalid slice , would panic, invalid image",
                            ));
                        };
                        // dequantize
                        for ((x, out), qt_val) in
                            qt_slice.iter().zip(tmp.iter_mut()).zip(qt_table.iter())
                        {
                            *out = i32::from(*x) * qt_val;
                        }
                        // determine where to write.
                        let sl = &mut temp_channel[component.idct_pos..];

                        component.idct_pos += 8;
                        // tmp now contains a dequantized block so idct it
                        (self.idct_func)(&mut tmp, sl, component.width_stride);
                    }
                    // after every write of 8, skip 7 since idct write stride wise 8 times.
                    //
                    // Remember each MCU is 8x8 block, so each idct will write 8 strides into
                    // sl
                    //
                    // and component.idct_pos is one stride long
                    component.idct_pos += 7 * component.width_stride;
                }
                component.idct_pos = 0;
            }

            // process that width up until it's impossible
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

        trace!("Finished decoding image");

        return Ok(());
    }
    pub(crate) fn reset_params(&mut self) {
        /*
        Apparently, grayscale images which can be down sampled exists, which is weird in the sense
        that it has one component Y, which is not usually down sampled.

        This means some calculations will be wrong, so for that we explicitly reset params
        for such occurrences, warn and reset the image info to appear as if it were
        a non-sampled image to ensure decoding works
        */
        self.h_max = 1;
        self.v_max = 1;
        self.info.sample_ratio = SampleRatios::None;
        self.is_interleaved = false;
        self.components[0].vertical_sample = 1;
        self.components[0].width_stride = (self.info.width as usize).div_ceil(8) * 8;
        self.components[0].horizontal_sample = 1;
    }
}

///Get a marker from the bit-stream.
///
/// This reads until it gets a marker or end of file is encountered
pub fn get_marker<T, B: BitStream>(
    reader: &mut ZReader<T>, stream: &mut B,
) -> Result<Marker, DecodeErrors>
where
    T: ZByteReaderTrait,
{
    if let Some(marker) = stream.marker().take() {
        return Ok(marker);
    }

    // read until we get a marker

    while !reader.eof()? {
        let marker = reader.read_u8_err()?;

        if marker == 255 {
            let mut r = reader.read_u8_err()?;
            // 0xFF 0XFF(some images may be like that)
            while r == 0xFF {
                r = reader.read_u8_err()?;
            }

            if r != 0 {
                return Marker::from_u8(r)
                    .ok_or_else(|| DecodeErrors::Format(format!("Unknown marker 0xFF{r:X}")));
            }
        }
    }
    return Err(DecodeErrors::ExhaustedData);
}

#[cfg(test)]
mod tests {
    use crate::JpegDecoder;
    use zune_core::bytestream::ZCursor;

    #[test]
    fn test_progressive_dri_420_color() {
        // 16x16 progressive 4:2:0 JPEG with DRI (restart markers)
        // Original: R=x*16, G=y*16, B=128
        // Bug: all three conditions (progressive + DRI + 4:2:0) produced grayscale
        const JPEG: &[u8] = &[
            0xff, 0xd8, 0xff, 0xdb, 0x00, 0xc5, 0x00, 0x03, 0x04, 0x04, 0x06, 0x04, 0x06, 0x06,
            0x06, 0x06, 0x06, 0x07, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
            0x08, 0x07, 0x08, 0x07, 0x08, 0x07, 0x08, 0x08, 0x09, 0x08, 0x09, 0x09, 0x08, 0x09,
            0x08, 0x09, 0x08, 0x0a, 0x0a, 0x0a, 0x08, 0x09, 0x09, 0x0a, 0x0a, 0x0a, 0x0a, 0x09,
            0x0a, 0x0c, 0x0c, 0x0c, 0x0a, 0x0c, 0x0b, 0x0b, 0x0c, 0x0d, 0x0c, 0x0d, 0x0b, 0x0b,
            0x09, 0x01, 0x02, 0x04, 0x04, 0x07, 0x06, 0x07, 0x08, 0x07, 0x07, 0x08, 0x07, 0x08,
            0x08, 0x08, 0x07, 0x0a, 0x0b, 0x0d, 0x0d, 0x0b, 0x0a, 0x0d, 0x0b, 0x0b, 0x0c, 0x0b,
            0x0b, 0x0d, 0x15, 0x18, 0x13, 0x0c, 0x0c, 0x13, 0x18, 0x15, 0x10, 0x49, 0x12, 0x0d,
            0x12, 0x49, 0x10, 0x12, 0x15, 0x0b, 0x0b, 0x15, 0x12, 0x1d, 0x0b, 0x15, 0x0b, 0x1d,
            0x2a, 0x20, 0x20, 0x2a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x50, 0x02, 0x03, 0x03, 0x03,
            0x05, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x06, 0x04, 0x05, 0x04, 0x06, 0x05, 0x05,
            0x05, 0x05, 0x05, 0x05, 0x06, 0x05, 0x05, 0x04, 0x05, 0x05, 0x06, 0x07, 0x06, 0x05,
            0x06, 0x06, 0x05, 0x06, 0x07, 0x06, 0x06, 0x06, 0x04, 0x06, 0x06, 0x06, 0x06, 0x06,
            0x07, 0x07, 0x06, 0x06, 0x07, 0x05, 0x06, 0x05, 0x07, 0x07, 0x07, 0x07, 0x07, 0x0a,
            0x0b, 0x0a, 0x0a, 0x0a, 0x52, 0xff, 0xc2, 0x00, 0x11, 0x08, 0x00, 0x10, 0x00, 0x10,
            0x03, 0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x02, 0xff, 0xc4, 0x00, 0x17,
            0x00, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x07, 0x02, 0x05, 0x08, 0xff, 0xdd, 0x00, 0x04, 0x00, 0x04, 0xff,
            0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0xca, 0xac, 0xcc, 0x4c, 0xdf,
            0xff, 0xda, 0x00, 0x08, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0xb0, 0xff, 0xda, 0x00,
            0x08, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x8f, 0xff, 0xc4, 0x00, 0x1f, 0x10, 0x00,
            0x01, 0x04, 0x02, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x05, 0x06, 0x21, 0x31, 0x07, 0xf1, 0x20, 0x61, 0xa1, 0xc1, 0xe1, 0xff,
            0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x02, 0x00, 0x49, 0x6a, 0x24, 0xb5, 0x12,
            0x5a, 0x89, 0x2d, 0x4f, 0xff, 0xda, 0x00, 0x08, 0x01, 0x02, 0x00, 0x01, 0x02, 0x00,
            0x59, 0x78, 0x7f, 0xff, 0xda, 0x00, 0x08, 0x01, 0x03, 0x00, 0x01, 0x02, 0x00, 0xc3,
            0xd9, 0x47, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x03, 0x3f, 0x02, 0xe1, 0xff,
            0xda, 0x00, 0x08, 0x01, 0x02, 0x00, 0x03, 0x3f, 0x02, 0x3f, 0xff, 0xda, 0x00, 0x08,
            0x01, 0x03, 0x00, 0x03, 0x3f, 0x02, 0xa9, 0x3f, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01,
            0x00, 0x03, 0x3f, 0x21, 0xa8, 0x3a, 0x2a, 0x0a, 0x83, 0xff, 0xda, 0x00, 0x08, 0x01,
            0x02, 0x00, 0x03, 0x3f, 0x21, 0xec, 0xff, 0xda, 0x00, 0x08, 0x01, 0x03, 0x00, 0x03,
            0x3f, 0x21, 0xf1, 0x1f, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x03, 0x3f, 0x10,
            0xd2, 0x57, 0xa9, 0xa4, 0xd2, 0x7f, 0xff, 0xda, 0x00, 0x08, 0x01, 0x02, 0x00, 0x03,
            0x3f, 0x10, 0xb3, 0xff, 0xda, 0x00, 0x08, 0x01, 0x03, 0x00, 0x03, 0x3f, 0x10, 0xfa,
            0x8f, 0xff, 0xd9,
        ];

        let mut decoder = JpegDecoder::new(ZCursor::new(JPEG));
        let pixels = decoder.decode().unwrap();
        let (w, h) = decoder.dimensions().unwrap();
        assert_eq!((w, h), (16, 16));

        // The image has color — R varies 0-240, G varies 0-240, B≈128
        // If the decoder produces R==G==B for every pixel, it's outputting grayscale
        let all_gray = pixels.chunks(3).all(|p| p[0] == p[1] && p[1] == p[2]);
        assert!(
            !all_gray,
            "Output is grayscale (R==G==B for all pixels). \
             Expected color output for progressive 4:2:0 JPEG with DRI."
        );

        // Verify first pixel is close to expected (R≈0, G≈0, B≈128)
        assert!(
            pixels[2] > 100,
            "Blue channel should be around 128, got {}",
            pixels[2]
        );
    }

    #[test]
    fn make_test() {
        let data = ZCursor::new([
            255, 216, 255, 224, 0, 16, 74, 70, 73, 70, 0, 1, 0, 2, 0, 28, 0, 28, 0, 0, 255, 219, 0,
            67, 0, 40, 28, 30, 20, 30, 25, 40, 35, 33, 35, 45, 43, 40, 48, 60, 100, 65, 60, 55, 55,
            60, 123, 88, 93, 65, 100, 145, 128, 153, 150, 143, 128, 140, 138, 160, 180, 230, 195,
            160, 170, 218, 173, 138, 140, 200, 255, 203, 218, 255, 238, 245, 255, 101, 0, 62, 8,
            255, 255, 250, 255, 230, 253, 255, 17, 255, 219, 0, 67, 1, 43, 45, 45, 42, 60, 48, 60,
            118, 65, 65, 118, 248, 165, 140, 165, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248,
            248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248,
            241, 255, 255, 255, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248,
            248, 248, 248, 248, 248, 248, 255, 192, 0, 17, 8, 0, 32, 0, 32, 3, 2, 17, 0, 1, 34, 1,
            3, 17, 1, 255, 196, 0, 24, 0, 1, 1, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 3, 0,
            1, 4, 255, 196, 0, 37, 16, 0, 2, 2, 1, 4, 1, 3, 5, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 17,
            0, 4, 18, 33, 48, 34, 65, 81, 113, 19, 20, 51, 97, 161, 255, 196, 0, 22, 1, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 255, 196, 0, 26, 17, 1, 0, 2, 3, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 17, 18, 38, 65, 255, 218, 0, 12, 3, 1, 0, 2, 17, 3,
            17, 0, 63, 0, 175, 119, 49, 197, 184, 2, 0, 0, 0, 16, 13, 129, 103, 161, 102, 178, 115,
            125, 202, 68, 236, 173, 25, 42, 164, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0,
            38, 0, 0, 0, 0, 250, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 67, 1, 43, 45, 45, 60, 48, 60,
            118, 65, 65, 118, 248, 165, 140, 165, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248,
            248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248,
            241, 255, 255, 255, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248,
            248, 248, 248, 248, 248, 248, 255, 192, 0, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 248, 248,
            248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248,
            248, 248, 248, 248, 248, 248, 248, 255, 192, 0, 17, 8, 0, 32, 0, 32, 3, 1, 34, 0, 2,
            17, 1, 3, 17, 1, 255, 196, 0, 24, 0, 1, 1, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            2, 0, 126, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 198,
        ]);
        let mut decoder = JpegDecoder::new(data);
        decoder.decode().unwrap();
    }
}
