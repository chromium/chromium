/*
 * Copyright (c) 2023.
 *
 * This software is free software;
 *
 * You can redistribute it or modify it under terms of the MIT, Apache License or Zlib license
 */

//! Decode Decoder markers/segments
//!
//! This file deals with decoding header information in a jpeg file
//!
use alloc::format;
use alloc::string::ToString;
use alloc::vec::Vec;
use core::cmp::max;

use zune_core::bytestream::ZByteReaderTrait;
use zune_core::log::{trace, warn};

use crate::components::{Components, SampleRatios};
use crate::decoder::{ExtendedXmpSegment, GainMapInfo, ICCChunk, JpegDecoder, MAX_COMPONENTS};
use crate::errors::DecodeErrors;
use crate::huffman::HuffmanTable;
use crate::misc::{SOFMarkers, UN_ZIGZAG};

/// Wrapper over a marker body that exposes a cursor-style read API.
///
/// All header parsers use this rather than `decoder.stream` directly, so any
/// I/O failure has already happened (and been surfaced to the caller) before
/// the parser starts mutating decoder fields.
pub(crate) struct MarkerBody<'a> {
    body:     &'a [u8],
    position: usize
}

impl<'a> MarkerBody<'a> {
    fn new(body: &'a [u8]) -> Self {
        Self { body, position: 0 }
    }

    pub(crate) fn body(&self) -> &'a [u8] {
        self.body
    }

    fn remaining(&self) -> usize {
        self.body.len() - self.position
    }

    fn read_u8(&mut self) -> Result<u8, DecodeErrors> {
        let byte = *self.body.get(self.position).ok_or(DecodeErrors::FormatStatic(
            "Marker payload shorter than declared length"
        ))?;
        self.position += 1;
        Ok(byte)
    }

    fn read_u16_be(&mut self) -> Result<u16, DecodeErrors> {
        if self.position + 2 > self.body.len() {
            return Err(DecodeErrors::FormatStatic(
                "Marker payload shorter than declared length"
            ));
        }
        let v = u16::from_be_bytes([self.body[self.position], self.body[self.position + 1]]);
        self.position += 2;
        Ok(v)
    }

    fn read_exact(&mut self, dst: &mut [u8]) -> Result<(), DecodeErrors> {
        if self.position + dst.len() > self.body.len() {
            return Err(DecodeErrors::FormatStatic(
                "Marker payload shorter than declared length"
            ));
        }
        dst.copy_from_slice(&self.body[self.position..self.position + dst.len()]);
        self.position += dst.len();
        Ok(())
    }
}

/// Read a length-prefixed marker body using the decoder's reusable scratch
/// buffer, then parse it inside a lexical scope.
///
/// The length field and payload are consumed before the closure runs. If the
/// input ends mid-marker, the decoder state is untouched and the scratch
/// allocation is returned before the recoverable EOF is reported. Keeping the
/// body scoped to the closure also makes it impossible for safe code to hold
/// a marker body after the decoder has been dropped.
pub(crate) fn with_marker_body<T, R, F>(
    decoder: &mut JpegDecoder<T>, parse: F
) -> Result<R, DecodeErrors>
where
    T: ZByteReaderTrait,
    F: FnOnce(&mut JpegDecoder<T>, MarkerBody<'_>) -> Result<R, DecodeErrors>
{
    let length = decoder.stream.get_u16_be_err()?;
    let body_len = usize::from(length)
        .checked_sub(2)
        .ok_or(DecodeErrors::FormatStatic("Marker length < 2"))?;

    let mut bytes = core::mem::take(&mut decoder.marker_body_scratch);
    bytes.clear();
    bytes.resize(body_len, 0);

    if let Err(e) = decoder.stream.read_exact_bytes(&mut bytes) {
        bytes.clear();
        decoder.marker_body_scratch = bytes;
        return Err(e.into());
    }

    let result = parse(decoder, MarkerBody::new(&bytes));
    bytes.clear();
    decoder.marker_body_scratch = bytes;
    result
}

///**B.2.4.2 Huffman table-specification syntax**
#[allow(clippy::similar_names, clippy::cast_sign_loss)]
pub(crate) fn parse_huffman<T: ZByteReaderTrait>(
    decoder: &mut JpegDecoder<T>
) -> Result<(), DecodeErrors>
where
{
    with_marker_body(decoder, |decoder, mut cursor| {
        let is_progressive = decoder.is_progressive;
        // Parse the body in two passes so we never mutate `decoder` if the body
        // turns out to be malformed: first build all tables into a local Vec,
        // then commit them in a tight loop.
        let mut new_tables: Vec<(u8, usize, HuffmanTable)> = Vec::new();
        while cursor.remaining() > 16 {
            // HT information
            let ht_info = cursor.read_u8()?;
            // third bit indicates whether the huffman encoding is DC or AC type
            let dc_or_ac = (ht_info >> 4) & 0xF;
            // Indicate the position of this table, should be less than 4;
            let index = (ht_info & 0xF) as usize;
            if index >= MAX_COMPONENTS {
                return Err(DecodeErrors::HuffmanDecode(format!(
                    "Invalid DHT index {index}, expected between 0 and 3"
                )));
            }
            if dc_or_ac > 1 {
                return Err(DecodeErrors::HuffmanDecode(format!(
                    "Invalid DHT position {dc_or_ac}, should be 0 or 1"
                )));
            }
            // read the code-length histogram
            let mut num_symbols: [u8; 17] = [0; 17];
            cursor.read_exact(&mut num_symbols[1..17])?;
            let symbols_sum: i32 = num_symbols.iter().map(|f| i32::from(*f)).sum();
            if symbols_sum > 256 {
                return Err(DecodeErrors::FormatStatic(
                    "Encountered Huffman table with excessive length in DHT"
                ));
            }
            if symbols_sum as usize > cursor.remaining() {
                return Err(DecodeErrors::HuffmanDecode(format!(
                    "Excessive Huffman table of length {symbols_sum} found when remaining bytes is {}",
                    cursor.remaining()
                )));
            }
            // symbols in increasing code length
            let mut symbols = [0; 256];
            cursor.read_exact(&mut symbols[0..(symbols_sum as usize)])?;
            let table = HuffmanTable::new(&num_symbols, symbols, dc_or_ac == 0, is_progressive)?;
            new_tables.push((dc_or_ac, index, table));
        }
        if cursor.remaining() > 0 {
            return Err(DecodeErrors::FormatStatic("Bogus Huffman table definition"));
        }
        // Commit phase: only after the entire body parsed cleanly.
        for (dc_or_ac, index, table) in new_tables {
            if dc_or_ac == 0 {
                decoder.entropy_tables.dc_huffman[index] = Some(table);
            } else {
                decoder.entropy_tables.ac_huffman[index] = Some(table);
            }
        }
        Ok(())
    })
}

///**B.2.4.3 Arithmetic conditioning table-specification syntax**
#[cfg(feature = "arith")]
#[allow(clippy::similar_names, clippy::cast_sign_loss)]
pub(crate) fn parse_dac<T: ZByteReaderTrait>(
    decoder: &mut JpegDecoder<T>
) -> Result<(), DecodeErrors>
where
{
    with_marker_body(decoder, |decoder, mut cursor| {
        if cursor.body().len() % 2 != 0 {
            return Err(DecodeErrors::FormatStatic(
                "Bogus (odd) Arithmetic-coding conditioning segment length"
            ));
        }
        // Validate everything before committing: collect new entries into a
        // local Vec and apply them only when the whole body parses cleanly.
        enum DacEntry {
            Dc { index: usize, l: u8, u: u8 },
            Ac { index: usize, kx: u8 }
        }
        let mut entries: Vec<DacEntry> = Vec::with_capacity(cursor.body().len() / 2);
        while cursor.remaining() >= 2 {
            let ht_info = cursor.read_u8()?;
            let cs_value = cursor.read_u8()?;
            let dc_or_ac = (ht_info >> 4) & 0xF;
            let index = (ht_info & 0xF) as usize;

            if index >= MAX_COMPONENTS {
                return Err(DecodeErrors::ArithmeticDecode(format!(
                    "Invalid DAC index {index}, expected between 0 and 3"
                )));
            }

            // todo: value 1 is also forbidden in lossless mode
            match dc_or_ac {
                0 => {
                    /* DC */
                    let (u, l) = (cs_value >> 4, cs_value & 0xF);
                    if l > u {
                        return Err(DecodeErrors::ArithmeticDecode(format!(
                            "Invalid conditioning table value {cs_value:x} for AC table, lower nibble should not exceed upper"
                        )));
                    }
                    entries.push(DacEntry::Dc { index, l, u });
                }
                1 => {
                    /* AC */
                    if cs_value == 0 || cs_value >= 64 {
                        return Err(DecodeErrors::ArithmeticDecode(format!(
                            "Invalid conditioning table value {cs_value} for AC table, should be in [1,63]"
                        )));
                    }
                    entries.push(DacEntry::Ac { index, kx: cs_value });
                }
                _ => {
                    return Err(DecodeErrors::ArithmeticDecode(format!(
                        "Invalid DHT position {dc_or_ac}, should be 0 or 1"
                    )));
                }
            }
        }
        // Commit phase.
        for entry in entries {
            match entry {
                DacEntry::Dc { index, l, u } => {
                    let t = &mut decoder.entropy_tables.dc_arithmetic[index];
                    t.l = l;
                    t.u = u;
                }
                DacEntry::Ac { index, kx } => {
                    decoder.entropy_tables.ac_arithmetic[index].kx = kx;
                }
            }
        }
        Ok(())
    })
}

///**B.2.4.1 Quantization table-specification syntax**
#[allow(clippy::cast_possible_truncation, clippy::needless_range_loop)]
pub(crate) fn parse_dqt<T: ZByteReaderTrait>(img: &mut JpegDecoder<T>) -> Result<(), DecodeErrors> {
    with_marker_body(img, |img, mut cursor| {
        // Build the new tables into a local Vec so the body either commits
        // entirely or commits not at all.
        let mut new_tables: Vec<(usize, [i32; 64])> = Vec::new();
        while cursor.remaining() > 0 {
            let qt_info = cursor.read_u8()?;
            // 0 = 8 bit otherwise 16 bit dqt
            let precision = (qt_info >> 4) as usize;
            // last 4 bits give us position
            let table_position = (qt_info & 0x0f) as usize;
            let precision_value = 64 * (precision + 1);

            if precision_value > cursor.remaining() {
                return Err(DecodeErrors::DqtError(format!(
                    "Invalid QT table bytes left: {}. Too small to construct a valid qt table which should be {} long",
                    cursor.remaining(),
                    precision_value
                )));
            }

            if table_position >= MAX_COMPONENTS {
                return Err(DecodeErrors::DqtError(format!(
                    "Too large table position for QT :{table_position}, expected between 0 and 3"
                )));
            }

            let dct_table = match precision {
                0 => {
                    let mut qt_values = [0u8; 64];
                    cursor.read_exact(&mut qt_values)?;
                    un_zig_zag(&qt_values)
                }
                1 => {
                    // 16 bit quantization tables
                    let mut qt_values = [0u16; 64];
                    for i in 0..64 {
                        qt_values[i] = cursor.read_u16_be()?;
                    }
                    un_zig_zag(&qt_values)
                }
                _ => {
                    return Err(DecodeErrors::DqtError(format!(
                        "Expected QT precision value of either 0 or 1, found {precision:?}"
                    )));
                }
            };

            trace!("Assigning qt table {table_position} with precision {precision}");
            new_tables.push((table_position, dct_table));
        }
        // Commit phase.
        for (table_position, dct_table) in new_tables {
            img.qt_tables[table_position] = Some(dct_table);
        }
        Ok(())
    })
}

/// Section:`B.2.2 Frame header syntax`
pub(crate) fn parse_start_of_frame<T: ZByteReaderTrait>(
    sof: SOFMarkers, img: &mut JpegDecoder<T>
) -> Result<(), DecodeErrors> {
    if img.seen_sof {
        return Err(DecodeErrors::SofError(
            "Two Start of Frame Markers".to_string()
        ));
    }
    with_marker_body(img, |img, mut cursor| {
        // Body length came from a u16 length field minus 2; +2 round-trips it.
        #[allow(clippy::cast_possible_truncation)]
        let length = (cursor.body().len() + 2) as u16;
        // usually 8, but can be 12 and 16, we currently support only 8
        // so sorry about that 12 bit images
        let dt_precision = cursor.read_u8()?;

        if dt_precision != 8 {
            return Err(DecodeErrors::SofError(format!(
                "The library can only parse 8-bit images, the image has {dt_precision} bits of precision"
            )));
        }

        // read the image height and width.
        let img_height = cursor.read_u16_be()?;
        let img_width = cursor.read_u16_be()?;

        trace!("Image width  :{img_width}");
        trace!("Image height :{img_height}");

        if usize::from(img_width) > img.options.max_width() {
            return Err(DecodeErrors::Format(format!("Image width {} greater than width limit {}. If use `set_limits` if you want to support huge images", img_width, img.options.max_width())));
        }

        if usize::from(img_height) > img.options.max_height() {
            return Err(DecodeErrors::Format(format!("Image height {} greater than height limit {}. If use `set_limits` if you want to support huge images", img_height, img.options.max_height())));
        }

        // Check image width is zero (height may legitimately be 0 for DNL images
        // where the actual number of lines is defined by a later DNL marker)
        if img_width == 0 {
            return Err(DecodeErrors::ZeroError);
        }

        // Number of components for the image.
        let num_components = cursor.read_u8()?;

        if num_components == 0 {
            return Err(DecodeErrors::SofError(
                "Number of components cannot be zero.".to_string()
            ));
        }

        let expected = 8 + 3 * u16::from(num_components);
        // length should be equal to num components
        if length != expected {
            return Err(DecodeErrors::SofError(format!(
                "Length of start of frame differs from expected {expected},value is {length}"
            )));
        }

        trace!("Image components : {num_components}");

        // Build components list locally; commit only when the whole body parses.
        let mut components = Vec::with_capacity(num_components as usize);
        let mut temp = [0; 3];
        for pos in 0..num_components {
            cursor.read_exact(&mut temp)?;
            let component = Components::from(temp, pos)?;
            components.push(component);
        }

        let mut h_max = 1;
        let mut v_max = 1;
        for comp in &components {
            h_max = max(h_max, comp.horizontal_sample);
            v_max = max(v_max, comp.vertical_sample);
        }
        let sample_ratio = match (h_max, v_max) {
            (1, 1) => SampleRatios::None,
            (1, 2) => SampleRatios::V,
            (2, 1) => SampleRatios::H,
            (2, 2) => SampleRatios::HV,
            (hs, vs) => SampleRatios::Generic(hs, vs)
        };

        // Commit phase: all reads succeeded, mutate the decoder.
        img.info.set_density(dt_precision);
        img.info.set_height(img_height);
        img.info.set_width(img_width);
        // A height of 0 means the encoder used a DNL marker to define the
        // actual line count. Signal this so the MCU decode loop knows to
        // intercept the DNL marker rather than stopping at row 0.
        if img_height == 0 {
            img.expects_dnl = true;
        }
        img.info.components = num_components;
        img.components = components;
        img.seen_sof = true;
        img.info.set_sof_marker(sof);
        img.info.sample_ratio = sample_ratio;

        Ok(())
    })
}

/// Parse a start of scan data
pub(crate) fn parse_sos<T: ZByteReaderTrait>(
    image: &mut JpegDecoder<T>
) -> Result<(), DecodeErrors> {
    with_marker_body(image, |image, mut cursor| {
        let ls = cursor.body().len() + 2; // total scan header length including the length field
        // Number of image components in scan
        let ns = cursor.read_u8()?;

        let mut seen: [_; 5] = [-1; { MAX_COMPONENTS + 1 }];

        let smallest_size = 6 + 2 * usize::from(ns);
        if ls != smallest_size {
            return Err(DecodeErrors::SosError(format!(
                "Bad SOS length {ls},corrupt jpeg"
            )));
        }

        // Check number of components.
        if !(1..5).contains(&ns) {
            return Err(DecodeErrors::SosError(format!(
                "Invalid number of components in start of scan {ns}, expected in range 1..5"
            )));
        }

        if image.info.components == 0 {
            return Err(DecodeErrors::FormatStatic(
                "Error decoding SOF Marker, Number of components cannot be zero."
            ));
        }

        // Validate inputs first; only mutate component table assignments at the
        // end so a malformed body cannot leave the decoder half-updated.
        let mut z_order = image.z_order;
        let mut huff_assignments: Vec<(usize, u8)> = Vec::with_capacity(ns as usize);
        let mut scan_subsampled = false;
        for i in 0..ns {
            let id = cursor.read_u8()?;

            if seen.contains(&i32::from(id)) {
                return Err(DecodeErrors::SofError(format!(
                    "Duplicate ID {id} seen twice in the same component"
                )));
            }

            seen[usize::from(i)] = i32::from(id);
            // DC and AC huffman table position
            // top 4 bits contain dc huffman destination table
            // lower four bits contain ac huffman destination table
            let y = cursor.read_u8()?;

            let mut j = 0;
            while j < image.info.components {
                if image.components[j as usize].id == id {
                    break;
                }
                j += 1;
            }

            if j == image.info.components {
                return Err(DecodeErrors::SofError(format!(
                    "Invalid component id {}, expected one one of {:?}",
                    id,
                    image.components.iter().map(|c| c.id).collect::<Vec<_>>()
                )));
            }

            huff_assignments.push((usize::from(j), y));
            z_order[i as usize] = j as usize;

            let component = &image.components[usize::from(j)];
            if component.vertical_sample != 1 || component.horizontal_sample != 1 {
                scan_subsampled = true;
            }
        }

        // Spectral parameters.
        let spec_start = cursor.read_u8()?;
        let spec_end = cursor.read_u8()?;
        let bit_approx = cursor.read_u8()?;
        let succ_high = bit_approx >> 4;
        let succ_low = bit_approx & 0xF;

        if spec_end > 63 {
            return Err(DecodeErrors::SosError(format!(
                "Invalid Se parameter {spec_end}, range should be 0-63"
            )));
        }
        if spec_start > 63 {
            return Err(DecodeErrors::SosError(format!(
                "Invalid Ss parameter {spec_start}, range should be 0-63"
            )));
        }
        if succ_high > 13 {
            return Err(DecodeErrors::SosError(format!(
                "Invalid Ah parameter {succ_high}, range should be 0-13"
            )));
        }
        if succ_low > 13 {
            return Err(DecodeErrors::SosError(format!(
                "Invalid Al parameter {succ_low}, range should be 0-13"
            )));
        }

        // Commit phase: all reads and validations succeeded.
        image.num_scans = ns;
        image.scan_subsampled = scan_subsampled;
        image.z_order = z_order;
        for (j, y) in huff_assignments {
            let component = &mut image.components[j];
            component.dc_huff_table = usize::from((y >> 4) & 0xF);
            component.ac_huff_table = usize::from(y & 0xF);
            trace!(
                "Assigned huffman tables {}/{} to component {j}, id={}",
                component.dc_huff_table,
                component.ac_huff_table,
                component.id,
            );
        }
        image.spec_start = spec_start;
        image.spec_end = spec_end;
        image.succ_high = succ_high;
        image.succ_low = succ_low;

        trace!("Ss={spec_start}, Se={spec_end} Ah={succ_high} Al={succ_low}");

        Ok(())
    })
}

/// Parse the APP13 (IPTC) segment.
pub(crate) fn parse_app13<T: ZByteReaderTrait>(
    decoder: &mut JpegDecoder<T>
) -> Result<(), DecodeErrors> {
    const IPTC_PREFIX: &[u8] = b"Photoshop 3.0\0";
    with_marker_body(decoder, |decoder, cursor| {
        let body = cursor.body();

        let new_iptc = if body.len() > IPTC_PREFIX.len() && &body[..IPTC_PREFIX.len()] == IPTC_PREFIX {
            Some(body[IPTC_PREFIX.len()..].to_vec())
        } else {
            None
        };

        // Commit phase.
        if let Some(iptc_bytes) = new_iptc {
            decoder.info.iptc_data = Some(iptc_bytes);
        }
        Ok(())
    })
}

/// Parse Adobe App14 segment
pub(crate) fn parse_app14<T: ZByteReaderTrait>(
    decoder: &mut JpegDecoder<T>
) -> Result<(), DecodeErrors> {
    with_marker_body(decoder, |decoder, cursor| {
        let body = cursor.body();

        // Validate, decide, then commit. No partial mutation on error.
        let transform = if body.len() >= 5 && &body[..5] == b"Adobe" {
            // Adobe segment must be at least 12 bytes of body (6 id + 5 ver/flags + 1 transform).
            if body.len() < 12 {
                return Err(DecodeErrors::FormatStatic(
                    "Too short of a length for App14 segment"
                ));
            }
            // adobe id = 6, version/flags = 5, transform = 1
            let transform = body[11];
            // https://exiftool.org/TagNames/JPEG.html#Adobe
            match transform {
                0..=2 => Some(transform),
                _ => {
                    return Err(DecodeErrors::Format(format!(
                        "Unknown Adobe colorspace {transform}"
                    )))
                }
            }
        } else {
            warn!("Not a valid Adobe APP14 Segment, skipping {} bytes", body.len());
            None
        };

        // Commit phase.
        if let Some(transform) = transform {
            decoder.adobe_transform = Some(transform);
        }
        Ok(())
    })
}

/// Parse the APP1 segment
///
/// This contains the exif tag
pub(crate) fn parse_app1<T: ZByteReaderTrait>(
    decoder: &mut JpegDecoder<T>
) -> Result<(), DecodeErrors> {
    const XMP_NAMESPACE_PREFIX: &[u8] = b"http://ns.adobe.com/xap/1.0/\0";
    const EXTENDED_XMP_NAMESPACE_PREFIX: &[u8] = b"http://ns.adobe.com/xmp/extension/\0";
    const EXTENDED_XMP_GUID_SIZE: usize = 32;
    const EXTENDED_XMP_TOTAL_SIZE_SIZE: usize = 4;
    const EXTENDED_XMP_OFFSET_SIZE: usize = 4;
    const EXTENDED_XMP_HEADER_SIZE: usize =
        EXTENDED_XMP_GUID_SIZE + EXTENDED_XMP_TOTAL_SIZE_SIZE + EXTENDED_XMP_OFFSET_SIZE;

    enum App1Payload {
        Exif(Vec<u8>),
        Xmp(Vec<u8>),
        ExtendedXmp(ExtendedXmpSegment),
        Unknown
    }

    with_marker_body(decoder, |decoder, cursor| {
        let body = cursor.body();

        let payload = if body.len() > 6 && &body[..6] == b"Exif\x00\x00" {
            trace!("Exif segment present");
            App1Payload::Exif(body[6..].to_vec())
        } else if body.len() > XMP_NAMESPACE_PREFIX.len()
            && &body[..XMP_NAMESPACE_PREFIX.len()] == XMP_NAMESPACE_PREFIX
        {
            trace!("XMP Data Present");
            App1Payload::Xmp(body[XMP_NAMESPACE_PREFIX.len()..].to_vec())
        } else if body.len() > EXTENDED_XMP_NAMESPACE_PREFIX.len()
            && &body[..EXTENDED_XMP_NAMESPACE_PREFIX.len()] == EXTENDED_XMP_NAMESPACE_PREFIX
        {
            trace!("Extended XMP Data Present");
            let rest = &body[EXTENDED_XMP_NAMESPACE_PREFIX.len()..];
            if rest.len() < EXTENDED_XMP_HEADER_SIZE {
                return Err(DecodeErrors::FormatStatic("Too small Extended XMP segment"));
            }
            let guid = rest[0..EXTENDED_XMP_GUID_SIZE].to_vec();
            let total_size_start = EXTENDED_XMP_GUID_SIZE;
            let total_size_end = total_size_start + EXTENDED_XMP_TOTAL_SIZE_SIZE;
            let total_size =
                u32::from_be_bytes(rest[total_size_start..total_size_end].try_into().unwrap());
            let offset_start = total_size_end;
            let offset_end = offset_start + EXTENDED_XMP_OFFSET_SIZE;
            let offset = u32::from_be_bytes(rest[offset_start..offset_end].try_into().unwrap());
            let data = rest[EXTENDED_XMP_HEADER_SIZE..].to_vec();
            App1Payload::ExtendedXmp(ExtendedXmpSegment { offset, total_size, guid, data })
        } else {
            warn!("Unknown format for APP1 tag, skipping");
            App1Payload::Unknown
        };

        // Commit phase.
        match payload {
            App1Payload::Exif(data) => decoder.info.exif_data = Some(data),
            App1Payload::Xmp(data) => decoder.info.xmp_data = Some(data),
            App1Payload::ExtendedXmp(seg) => decoder.extended_xmp_segments.push(seg),
            App1Payload::Unknown => {}
        }
        Ok(())
    })
}

pub(crate) fn parse_app2<T: ZByteReaderTrait>(
    decoder: &mut JpegDecoder<T>
) -> Result<(), DecodeErrors> {
    static HDR_META: &[u8] = b"urn:iso:std:iso:ts:21496:-1\0";
    static MPF_DATA: &[u8] = b"MPF\0";
    static ICC_PROFILE: &[u8] = b"ICC_PROFILE\0";

    enum App2Payload {
        IccChunk(ICCChunk),
        GainMap(GainMapInfo),
        Mpf { offset: u64, data: Vec<u8> },
        Unknown
    }

    with_marker_body(decoder, |decoder, cursor| {
        let body = cursor.body();

        let payload = if body.len() > ICC_PROFILE.len() + 2 && &body[..ICC_PROFILE.len()] == ICC_PROFILE
        {
            trace!("ICC Profile present");
            let rest = &body[ICC_PROFILE.len()..];
            let seq_no = rest[0];
            let num_markers = rest[1];

            // Mirrors libjpeg-turbo jdicc.c: reject num_markers == 0
            // and seq_no out of [1, num_markers]. No artificial cap on
            // num_markers — the field is a u8 so 255 is the natural limit.
            if num_markers == 0 {
                return Err(DecodeErrors::Format(format!(
                    "ICC profile claims {num_markers} chunks (must be >= 1)"
                )));
            }
            if seq_no == 0 || seq_no > num_markers {
                return Err(DecodeErrors::Format(format!(
                    "ICC chunk seq_no {seq_no} out of range for {num_markers} chunks"
                )));
            }

            let data = rest[2..].to_vec();
            App2Payload::IccChunk(ICCChunk {
                seq_no,
                num_markers,
                data
            })
        } else if body.len() > HDR_META.len() && &body[..HDR_META.len()] == HDR_META {
            trace!("Gain Map metadata found");
            let rest = &body[HDR_META.len()..];
            match rest.len() {
                4 => {
                    // If gain map metadata length == 4 the body is just version words.
                    App2Payload::GainMap(GainMapInfo { data: Vec::new() })
                }
                n if n > 4 => App2Payload::GainMap(GainMapInfo {
                    data: rest.to_vec()
                }),
                _ => App2Payload::Unknown
            }
        } else if body.len() > MPF_DATA.len() && &body[..MPF_DATA.len()] == MPF_DATA {
            trace!("MPF Signature present");
            // Compute body start position on-demand (only MPF needs it).
            // After with_marker_body read the payload, the stream sits at
            // body_start + body.len(); subtract to recover the origin.
            let body_start_pos = decoder.stream.position()? - body.len() as u64;
            let mpf_offset = body_start_pos + MPF_DATA.len() as u64;
            let data = body[MPF_DATA.len()..].to_vec();
            App2Payload::Mpf { offset: mpf_offset, data }
        } else {
            App2Payload::Unknown
        };

        // Commit phase.
        match payload {
            App2Payload::IccChunk(chunk) => decoder.icc_data.push(chunk),
            App2Payload::GainMap(info) => decoder.info.gain_map_info.push(info),
            App2Payload::Mpf { offset, data } => {
                decoder.info.multi_picture_information_offset = Some(offset);
                decoder.info.multi_picture_information = Some(data);
            }
            App2Payload::Unknown => {}
        }

        Ok(())
    })
}

/// Small utility function to print Un-zig-zagged quantization tables
fn un_zig_zag<T>(a: &[T]) -> [i32; 64]
where
    T: Default + Copy,
    i32: core::convert::From<T>
{
    let mut output = [i32::default(); 64];

    for i in 0..64 {
        output[UN_ZIGZAG[i]] = i32::from(a[i]);
    }

    output
}
