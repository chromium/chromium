/*
 * Copyright (c) 2026.
 *
 * This software is free software;
 *
 * You can redistribute it or modify it under terms of the MIT, Apache License or Zlib license
 */

//! Predictive decoding for 8-bit grayscale and interleaved YCbCr/RGB lossless frames.

#[cfg(feature = "arith")]
use alloc::boxed::Box;
use alloc::string::ToString;
use alloc::vec::Vec;

use zune_core::bytestream::{ZByteReaderTrait, ZReader};
use zune_core::colorspace::ColorSpace;
use zune_core::log::warn;

use crate::decoder::{EntropyTables, JpegDecoder};
use crate::errors::{DecodeErrors, UnsupportedSchemes};
use crate::huffman::HuffmanTable;
use crate::marker::Marker;

// Unlike the DCT lookahead reader, this reader never synthesizes entropy bits.
struct LosslessBits {
    byte:      u8,
    remaining: u8
}

struct History {
    previous:    Vec<u16>,
    current:     Vec<u16>,
    differences: Vec<i32>
}

enum Entropy {
    Huffman(LosslessBits),
    #[cfg(feature = "arith")]
    Arithmetic(Box<crate::lossless_arith::LosslessArithmetic>)
}

impl Entropy {
    fn marker<T: ZByteReaderTrait>(
        &mut self, reader: &mut ZReader<T>
    ) -> Result<Marker, DecodeErrors> {
        match self {
            Self::Huffman(bits) => bits.marker(reader),
            #[cfg(feature = "arith")]
            Self::Arithmetic(bits) => bits.marker(reader)
        }
    }

    fn restart(&mut self) {
        match self {
            Self::Huffman(_) => {}
            #[cfg(feature = "arith")]
            Self::Arithmetic(bits) => bits.restart()
        }
    }

    fn difference<T: ZByteReaderTrait>(
        &mut self, reader: &mut ZReader<T>, tables: &EntropyTables, selector: usize, left: i32,
        above: i32
    ) -> Result<i32, DecodeErrors> {
        match self {
            Self::Huffman(bits) => {
                let table = tables
                    .dc_huffman
                    .get(selector)
                    .and_then(Option::as_ref)
                    .ok_or(DecodeErrors::FormatStatic(
                        "Missing lossless JPEG Huffman table"
                    ))?;
                let _ = (left, above);
                bits.difference(reader, table)
            }
            #[cfg(feature = "arith")]
            Self::Arithmetic(bits) => {
                let table =
                    tables
                        .dc_arithmetic
                        .get(selector)
                        .ok_or(DecodeErrors::FormatStatic(
                            "Invalid lossless arithmetic conditioning selector"
                        ))?;
                bits.difference(reader, selector, table, left, above)
            }
        }
    }
}

fn row<T: Clone + Default>(width: usize) -> Result<Vec<T>, DecodeErrors> {
    let mut samples = Vec::new();
    samples.try_reserve_exact(width).map_err(|_| {
        DecodeErrors::FormatStatic("Could not allocate lossless JPEG prediction row")
    })?;
    samples.resize(width, T::default());
    Ok(samples)
}

#[cfg(test)]
#[test]
fn prediction_allocation_overflow_is_an_error() {
    assert!(row::<u16>(usize::MAX).is_err());
    assert!(row::<i32>(usize::MAX).is_err());
}

fn predict(history: &History, x: usize, first_row: bool, initial: i32, selector: u8) -> i32 {
    if first_row {
        return if x == 0 { initial } else { i32::from(history.current[x - 1]) };
    }
    let b = i32::from(history.previous[x]);
    if x == 0 {
        return b;
    }
    let a = i32::from(history.current[x - 1]);
    let c = i32::from(history.previous[x - 1]);
    match selector {
        1 => a,
        2 => b,
        3 => c,
        4 => a + b - c,
        5 => a + ((b - c) >> 1),
        6 => b + ((a - c) >> 1),
        7 => (a + b) >> 1,
        _ => unreachable!("SOS validates the predictor")
    }
}

impl LosslessBits {
    fn bit<T: ZByteReaderTrait>(&mut self, reader: &mut ZReader<T>) -> Result<i32, DecodeErrors> {
        if self.remaining == 0 {
            self.byte = reader.read_u8_err()?;
            if self.byte == 0xff && reader.read_u8_err()? != 0 {
                return Err(DecodeErrors::FormatStatic(
                    "Premature marker in lossless JPEG scan"
                ));
            }
            self.remaining = 8;
        }
        self.remaining -= 1;
        Ok(i32::from((self.byte >> self.remaining) & 1))
    }

    fn difference<T: ZByteReaderTrait>(
        &mut self, reader: &mut ZReader<T>, table: &HuffmanTable
    ) -> Result<i32, DecodeErrors> {
        let mut code = 0;
        for length in 1..=16 {
            code = (code << 1) | self.bit(reader)?;
            if table.maxcode[length] >= 0 && (code << (16 - length)) < table.maxcode[length] {
                let index = usize::try_from(code + table.offset[length])
                    .map_err(|_| DecodeErrors::FormatStatic("Invalid lossless Huffman code"))?;
                let category = *table
                    .values
                    .get(index)
                    .ok_or(DecodeErrors::FormatStatic("Invalid lossless Huffman code"))?;
                if category == 16 {
                    return Ok(32768);
                }
                let mut value = 0;
                for _ in 0..category {
                    value = (value << 1) | self.bit(reader)?;
                }
                if category != 0 && value < (1 << (category - 1)) {
                    value -= (1 << category) - 1;
                }
                return Ok(value);
            }
        }
        Err(DecodeErrors::HuffmanDecode(
            "Invalid lossless Huffman code".to_string()
        ))
    }

    fn marker<T: ZByteReaderTrait>(
        &mut self, reader: &mut ZReader<T>
    ) -> Result<Marker, DecodeErrors> {
        self.remaining = 0;
        if reader.read_u8_err()? != 0xff {
            return Err(DecodeErrors::FormatStatic(
                "Expected marker after lossless JPEG entropy"
            ));
        }
        let mut byte = reader.read_u8_err()?;
        while byte == 0xff {
            byte = reader.read_u8_err()?;
        }
        Marker::from_u8(byte)
            .filter(|marker| !matches!(marker, Marker::UNKNOWN(_)))
            .ok_or(DecodeErrors::FormatStatic(
                "Invalid marker after lossless JPEG entropy"
            ))
    }
}

impl<T: ZByteReaderTrait> JpegDecoder<T> {
    pub(crate) fn validate_lossless_encoding(&self) -> Result<(), DecodeErrors> {
        let count = self.components.len();
        if self.info.pixel_density != 8
            || !matches!(count, 1 | 3)
            || usize::from(self.num_scans) != count
            || (count == 3
                && (!matches!(self.input_colorspace, ColorSpace::YCbCr | ColorSpace::RGB)
                    || self
                        .components
                        .iter()
                        .any(|c| c.horizontal_sample != 1 || c.vertical_sample != 1)))
        {
            return Err(DecodeErrors::Unsupported(if self.is_arithmetic {
                UnsupportedSchemes::LosslessArithmetic
            } else {
                UnsupportedSchemes::LosslessHuffman
            }));
        }
        if self.info.height == 0 || self.dnl_image {
            return Err(DecodeErrors::FormatStatic(
                "Lossless JPEG with DNL is not supported"
            ));
        }
        for component in &self.components {
            if component.quantization_table_number != 0 || component.ac_huff_table != 0 {
                return Err(DecodeErrors::FormatStatic(
                    "Lossless JPEG requires zero quantization and AC table selectors"
                ));
            }
            if component.dc_huff_table >= 4 {
                return Err(DecodeErrors::FormatStatic(
                    "Invalid lossless entropy table selector"
                ));
            }
            if !self.is_arithmetic
                && self.entropy_tables.dc_huffman[component.dc_huff_table].is_none()
            {
                return Err(DecodeErrors::FormatStatic(
                    "Missing lossless JPEG Huffman table"
                ));
            }
        }
        if self.restart_interval % usize::from(self.info.width) != 0 {
            return Err(DecodeErrors::FormatStatic(
                "Lossless JPEG restart interval must contain whole rows"
            ));
        }
        if self.input_colorspace == ColorSpace::RGB
            && !matches!(
                self.options.jpeg_get_out_colorspace(),
                ColorSpace::RGB | ColorSpace::BGR | ColorSpace::RGBA | ColorSpace::BGRA
            )
        {
            return Err(DecodeErrors::FormatStatic(
                "Unsupported lossless RGB output colorspace"
            ));
        }
        if !((count == 3 && self.options.jpeg_get_out_colorspace() == ColorSpace::YCbCr)
            || matches!(
                self.options.jpeg_get_out_colorspace(),
                ColorSpace::Luma
                    | ColorSpace::LumaA
                    | ColorSpace::RGB
                    | ColorSpace::BGR
                    | ColorSpace::RGBA
                    | ColorSpace::BGRA
            ))
        {
            return Err(DecodeErrors::FormatStatic(
                "Unsupported lossless JPEG output colorspace"
            ));
        }
        Ok(())
    }

    #[allow(clippy::cast_possible_truncation)]
    fn write_lossless_row(&self, histories: &[History], output: &mut [u8]) {
        let color = self.options.jpeg_get_out_colorspace();
        let channels = color.num_components();
        let sample =
            |component: usize, x: usize| (histories[component].current[x] << self.succ_low) as u8;
        if self.input_colorspace == ColorSpace::RGB {
            let order = if matches!(color, ColorSpace::BGR | ColorSpace::BGRA) {
                [2, 1, 0]
            } else {
                [0, 1, 2]
            };
            for (x, pixel) in output.chunks_exact_mut(channels).enumerate() {
                for (value, component) in pixel.iter_mut().zip(order) {
                    *value = sample(component, x);
                }
                if color.has_alpha() {
                    pixel[3] = 255;
                }
            }
        } else if histories.len() == 1
            || matches!(
                color,
                ColorSpace::Luma | ColorSpace::LumaA | ColorSpace::YCbCr
            )
        {
            for (x, pixel) in output.chunks_exact_mut(channels).enumerate() {
                if color == ColorSpace::YCbCr {
                    for (component, value) in pixel.iter_mut().enumerate() {
                        *value = sample(component, x);
                    }
                } else {
                    pixel.fill(sample(0, x));
                    if color.has_alpha() {
                        pixel[channels - 1] = 255;
                    }
                }
            }
        } else {
            for (block, destination) in output.chunks_mut(16 * channels).enumerate() {
                let count = destination.len() / channels;
                let mut planes = [[0i16; 16]; 3];
                for (component, plane) in planes.iter_mut().enumerate() {
                    for (x, value) in plane.iter_mut().take(count).enumerate() {
                        *value = i16::from(sample(component, block * 16 + x));
                    }
                }
                let mut converted = [0; 64];
                let mut position = 0;
                (self.color_convert_16.block)(
                    &planes[0],
                    &planes[1],
                    &planes[2],
                    &mut converted,
                    &mut position
                );
                destination.copy_from_slice(&converted[..destination.len()]);
            }
        }
    }

    #[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
    pub(crate) fn decode_lossless(&mut self, output: &mut [u8]) -> Result<(), DecodeErrors> {
        let width = usize::from(self.info.width);
        let height = usize::from(self.info.height);
        let channels = self.options.jpeg_get_out_colorspace().num_components();
        let mut histories = self
            .components
            .iter()
            .map(|_| {
                let differences = if self.is_arithmetic { row(width)? } else { Vec::new() };
                Ok(History {
                    previous: row(width)?,
                    current: row(width)?,
                    differences
                })
            })
            .collect::<Result<Vec<_>, DecodeErrors>>()?;
        let mut bits = Entropy::Huffman(LosslessBits {
            byte:      0,
            remaining: 0
        });
        #[cfg(feature = "arith")]
        if self.is_arithmetic {
            bits = Entropy::Arithmetic(Box::new(crate::lossless_arith::LosslessArithmetic::new()));
        }

        let mut next_restart = 0;
        let mut interval_start = 0;
        let initial = 1 << (7 - self.succ_low);
        let mut cancel = self.cancel_debounced(width);
        for y in 0..height {
            if cancel.is_cancelled() {
                return Err(DecodeErrors::Cancelled);
            }
            let row_start = y * width;
            if self.restart_interval != 0
                && row_start != 0
                && row_start % self.restart_interval == 0
            {
                if bits.marker(&mut self.stream)? != Marker::RST(next_restart) {
                    return Err(DecodeErrors::FormatStatic(
                        "Incorrect lossless JPEG restart sequence"
                    ));
                }
                next_restart = (next_restart + 1) & 7;
                interval_start = y;
                bits.restart();
                for history in &mut histories {
                    history.differences.fill(0);
                }
            }
            let mut left = [0; 3];
            for x in 0..width {
                for &component in &self.z_order[..self.components.len()] {
                    let history = &mut histories[component];
                    let prediction =
                        predict(history, x, y == interval_start, initial, self.spec_start);
                    let above = if self.is_arithmetic { history.differences[x] } else { 0 };
                    let difference = bits.difference(
                        &mut self.stream,
                        &self.entropy_tables,
                        self.components[component].dc_huff_table,
                        left[component],
                        above
                    )?;
                    if self.is_arithmetic {
                        history.differences[x] = difference;
                    }
                    left[component] = difference;
                    // T.81 H.2 reconstructs modulo 2^16, before the point transform.
                    history.current[x] = ((prediction + difference) & 0xffff) as u16;
                }
            }
            self.write_lossless_row(
                &histories,
                &mut output[row_start * channels..(row_start + width) * channels]
            );
            self.pixels_decoded = (row_start + width) * channels;
            for history in &mut histories {
                core::mem::swap(&mut history.previous, &mut history.current);
            }
        }
        loop {
            match bits.marker(&mut self.stream) {
                Ok(Marker::EOI) => return Ok(()),
                Ok(marker @ (Marker::APP(_) | Marker::COM)) => self.parse_marker_inner(marker)?,
                Ok(_) => {
                    return Err(DecodeErrors::FormatStatic(
                        "Unexpected marker after lossless JPEG scan"
                    ))
                }
                Err(error) if error.is_recoverable_eof() && !self.scan_eof_is_error() => {
                    warn!(
                        "Lossless JPEG samples are complete but the end marker is missing: {error}"
                    );
                    return Ok(());
                }
                Err(error) => return Err(error)
            }
        }
    }
}
