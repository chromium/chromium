// Symphonia
// Copyright (c) 2019-2024 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! FLAC metadata block reading.

use std::num::NonZeroU8;
use std::sync::Arc;

use log::warn;
use symphonia_core::errors::{Result, decode_error};
use symphonia_core::formats::VendorDataAttachment;
use symphonia_core::formats::util::SeekIndex;
use symphonia_core::io::ReadBytes;
use symphonia_core::meta::well_known::METADATA_ID_FLAC;
use symphonia_core::meta::{
    Chapter, ChapterGroup, ChapterGroupItem, MetadataBuilder, MetadataInfo, Size, StandardTag, Tag,
    Visual,
};
use symphonia_core::units::{TimeBase, Timestamp};

use crate::embedded::vorbis;
use crate::utils::id3v2::get_visual_key_from_picture_type;
use crate::utils::images::try_get_image_info;

pub const FLAC_METADATA_INFO: MetadataInfo = MetadataInfo {
    metadata: METADATA_ID_FLAC,
    short_name: "flac",
    long_name: "Free Lossless Audio Codec",
};

/// Converts a string of bytes to an ASCII string if all characters are within the printable ASCII
/// range. If a null byte is encounted, the string terminates at that point.
fn printable_ascii_to_string(bytes: &[u8]) -> Option<String> {
    let mut result = String::with_capacity(bytes.len());

    for c in bytes {
        match c {
            0x00 => break,
            0x20..=0x7e => result.push(char::from(*c)),
            _ => return None,
        }
    }

    Some(result)
}

/// Read a comment metadata block.
pub fn read_flac_comment_block<B: ReadBytes>(
    reader: &mut B,
    builder: &mut MetadataBuilder,
) -> Result<()> {
    // Discard side data.
    let mut side_data = Default::default();
    vorbis::read_vorbis_comment(reader, builder, &mut side_data)
}

/// Read a picture metadata block.
pub fn read_flac_picture_block<B: ReadBytes>(reader: &mut B) -> Result<Visual> {
    let type_enc = reader.read_be_u32()?;

    // Read the Media Type length in bytes.
    // TODO: Apply a limit.
    let media_type_len = reader.read_be_u32()? as usize;

    // Read the Media Type bytes
    let media_type_buf = reader.read_boxed_slice_exact(media_type_len)?;

    // Convert Media Type bytes to an ASCII string. Non-printable ASCII characters are invalid.
    let media_type = match printable_ascii_to_string(&media_type_buf) {
        Some(s) => {
            // Return None if the media-type string is empty.
            Some(s).filter(|s| !s.is_empty())
        }
        None => return decode_error("meta (flac): picture mime-type contains invalid characters"),
    };

    let mut tags = vec![];

    // Read the description length in bytes.
    // TODO: Apply a limit.
    let desc_len = reader.read_be_u32()? as usize;

    // Read the description bytes.
    let desc_buf = reader.read_boxed_slice_exact(desc_len)?;

    // Convert to a UTF-8 string.
    let desc = String::from_utf8_lossy(&desc_buf);

    if !desc.is_empty() {
        let desc = Arc::new(desc.into_owned());
        let tag =
            Tag::new_from_parts("DESCRIPTION", desc.clone(), Some(StandardTag::Description(desc)));

        tags.push(tag);
    }

    // Read the width, and height of the visual.
    let width = reader.read_be_u32()?;
    let height = reader.read_be_u32()?;

    // If either the width or height is 0, then the size is invalid.
    let dimensions = if width > 0 && height > 0 { Some(Size { width, height }) } else { None };

    // Read bits-per-pixel of the visual.
    let _bits_per_pixel = NonZeroU8::new(reader.read_be_u32()? as u8);

    // Indexed colours is only valid for image formats that use an indexed colour palette. If it is
    // 0, the image does not used indexed colours.
    let _color_mode = reader.read_be_u32()?;

    // Read the image data length in bytes.
    // TODO: Apply a limit.
    let data_len = reader.read_be_u32()? as usize;

    // Read the image data.
    let data = reader.read_boxed_slice_exact(data_len)?;

    // Try to detect the image characteristics from the image data. Detect image characteristics
    // will be preferred over what's been stated in the picture block.
    let image_info = try_get_image_info(&data);

    Ok(Visual {
        media_type: image_info.as_ref().map(|info| info.media_type.clone()).or(media_type),
        dimensions: image_info.as_ref().map(|info| info.dimensions).or(dimensions),
        color_mode: image_info.as_ref().map(|info| info.color_mode),
        usage: get_visual_key_from_picture_type(type_enc),
        tags,
        data,
    })
}

/// Read a seek table metadata block as a seek index.
pub fn read_flac_seektable_block<B: ReadBytes>(
    reader: &mut B,
    block_length: u32,
) -> Result<SeekIndex> {
    // The number of seek table entries is always the metadata block length divided by the length of
    // a single entry, 18 bytes.
    let count = block_length / 18;

    let mut index = SeekIndex::new();

    // Read each entry in the seektable.
    for _ in 0..count {
        // The sample number (timestamp) of the first sample in the target frame.
        let sample = reader.read_be_u64()?;

        // A sample number of 0xFFFFFFFFFFFFFFFF is designated as a placeholder and indicates the
        // entry should be ignored by decoders. However, since timestamps > 0x7FFFFFFFFFFFFFFF are
        // unrepresentable in Symphonia, all of these entries will also be ignored.
        if let Ok(sample) = sample.try_into() {
            // The byte offset of the target frame.
            let offset = reader.read_be_u64()?;
            // The number of samples in the target frame.
            let num_samples = reader.read_be_u16()?;
            index.insert(sample, offset, u32::from(num_samples));
        }
        else {
            reader.ignore_bytes(10)?
        }
    }

    Ok(index)
}

/// Read a vendor-specific application metadata block as vendor data.
pub fn read_flac_application_block<B: ReadBytes>(
    reader: &mut B,
    block_length: u32,
) -> Result<VendorDataAttachment> {
    // Read the application identifier. Usually this is just 4 ASCII characters, but it is not
    // limited to that. Non-printable ASCII characters must be escaped to create a valid UTF8
    // string.
    let ident = escape_identifier(&reader.read_quad_bytes()?);
    let data = reader.read_boxed_slice_exact(block_length as usize - 4)?;
    Ok(VendorDataAttachment { ident, data })
}

/// Read a cuesheet metadata block as a chapter group.
pub fn read_flac_cuesheet_block<B: ReadBytes>(
    reader: &mut B,
    tb: TimeBase,
) -> Result<ChapterGroup> {
    // Read cuesheet catalog number. The catalog number only allows printable ASCII characters.
    let mut catalog_number_buf = vec![0u8; 128];
    reader.read_buf_exact(&mut catalog_number_buf)?;

    // Read the catalog number, and store it in a Tag to be attached to the chapter group.
    let catalog_number = match printable_ascii_to_string(&catalog_number_buf) {
        Some(num) => {
            let num = Arc::new(num);
            Tag::new_from_parts("CATALOG", num.clone(), Some(StandardTag::IdentCatalogNumber(num)))
        }
        None => return decode_error("flac: cuesheet catalog number contains invalid characters"),
    };

    // Number of lead-in samples.
    let n_lead_in_samples = reader.read_be_u64()?;

    // Next bit is set for CD-DA cuesheets.
    let is_cdda = (reader.read_u8()? & 0x80) == 0x80;

    // Lead-in should be non-zero only for CD-DA cuesheets.
    if !is_cdda && n_lead_in_samples > 0 {
        return decode_error("flac: cuesheet lead-in samples should be zero if not CD-DA");
    }

    // Next 258 bytes (read as 129 u16's) must be zero.
    for _ in 0..129 {
        if reader.read_be_u16()? != 0 {
            return decode_error("flac: cuesheet reserved bits should be zero");
        }
    }

    let n_tracks = reader.read_u8()?;

    // There should be at-least one track in the cuesheet.
    if n_tracks == 0 {
        return decode_error("flac: cuesheet must have at-least one track");
    }

    // CD-DA cuesheets must have no more than 100 tracks (99 audio tracks + lead-out track)
    if is_cdda && n_tracks > 100 {
        return decode_error("flac: cuesheets for CD-DA must not have more than 100 tracks");
    }

    let mut group = ChapterGroup {
        items: Vec::with_capacity(usize::from(n_tracks)),
        tags: vec![catalog_number],
        visuals: Vec::new(),
    };

    for _ in 0..n_tracks {
        group.items.push(read_flac_cuesheet_track(reader, is_cdda, tb)?);
    }

    Ok(group)
}

fn read_flac_cuesheet_track<B: ReadBytes>(
    reader: &mut B,
    is_cdda: bool,
    tb: TimeBase,
) -> Result<ChapterGroupItem> {
    let n_offset_samples = reader.read_be_u64()?;

    // For a CD-DA cuesheet, the track sample offset is the same as the first index (INDEX 00 or
    // INDEX 01) on the CD. Therefore, the offset must be a multiple of 588 samples
    // (588 samples = 44100 samples/sec * 1/75th of a sec).
    if is_cdda && n_offset_samples % 588 != 0 {
        return decode_error(
            "flac: cuesheet track sample offset is not a multiple of 588 for CD-DA",
        );
    }

    let number = reader.read_u8()?;

    // A track number of 0 is disallowed in all cases. For CD-DA cuesheets, track 0 is reserved for
    // lead-in.
    if number == 0 {
        return decode_error("flac: cuesheet track number of 0 not allowed");
    }

    // For CD-DA cuesheets, only track numbers 1-99 are allowed for regular tracks and 170 for
    // lead-out.
    if is_cdda && number > 99 && number != 170 {
        return decode_error(
            "flac: cuesheet track numbers greater than 99 are not allowed for CD-DA",
        );
    }

    let mut isrc_buf = vec![0u8; 12];
    reader.read_buf_exact(&mut isrc_buf)?;

    let isrc = match printable_ascii_to_string(&isrc_buf) {
        Some(num) => {
            let num = Arc::new(num);
            Tag::new_from_parts("ISRC", num.clone(), Some(StandardTag::IdentIsrc(num)))
        }
        None => return decode_error("flac: cuesheet track ISRC contains invalid characters"),
    };

    // Next 14 bytes are reserved. However, the first two bits are flags. Consume the reserved bytes
    // in u16 chunks a minor performance improvement.
    let flags = reader.read_be_u16()?;

    // These values are contained in the Cuesheet but have no analogue in Symphonia.
    let _is_audio = (flags & 0x8000) == 0x0000;
    let _use_pre_emphasis = (flags & 0x4000) == 0x4000;

    if flags & 0x3fff != 0 {
        return decode_error("flac: cuesheet track reserved bits should be zero");
    }

    // Consume the remaining 12 bytes read in 3 u32 chunks.
    for _ in 0..3 {
        if reader.read_be_u32()? != 0 {
            return decode_error("flac: cuesheet track reserved bits should be zero");
        }
    }

    let n_indicies = reader.read_u8()? as usize;

    // For CD-DA cuesheets, the track index cannot exceed 100 indicies.
    if is_cdda && n_indicies > 100 {
        return decode_error("flac: cuesheet track indicies cannot exceed 100 for CD-DA");
    }

    // If the track contains indicies, then one chapter will be created per index, and a chapter
    // group returned.
    if n_indicies > 0 {
        let mut group = ChapterGroup {
            items: Vec::with_capacity(n_indicies),
            tags: vec![isrc],
            visuals: vec![],
        };

        // Add each index as a chapter.
        for _ in 0..n_indicies {
            let index = read_flac_cuesheet_track_index(reader, is_cdda, n_offset_samples, tb)?;

            group.items.push(ChapterGroupItem::Chapter(index));
        }

        Ok(ChapterGroupItem::Group(group))
    }
    else {
        let start_ts = n_offset_samples.try_into().unwrap_or_else(|_| {
            warn!("cuesheet track index offset too large, clamping to maximum");
            i64::MAX
        });

        // If the track has no indicies, return a single chapter.
        let chapter = Chapter {
            start_time: tb.calc_time_saturating(Timestamp::from(start_ts)),
            end_time: None,
            start_byte: None,
            end_byte: None,
            tags: vec![isrc],
            visuals: vec![],
        };

        Ok(ChapterGroupItem::Chapter(chapter))
    }
}

fn read_flac_cuesheet_track_index<B: ReadBytes>(
    reader: &mut B,
    is_cdda: bool,
    track_ts: u64,
    tb: TimeBase,
) -> Result<Chapter> {
    let n_offset_samples = reader.read_be_u64()?;
    let idx_number_raw = reader.read_be_u32()?;

    // CD-DA track index points must have a sample offset that is a multiple of 588 samples
    // (588 samples = 44100 samples/sec * 1/75th of a sec).
    if is_cdda && n_offset_samples % 588 != 0 {
        return decode_error(
            "flac: cuesheet track index point sample offset is not a multiple of 588 for CD-DA",
        );
    }

    // Lower 24 bits are reserved and should be 0.
    if idx_number_raw & 0x00ff_ffff != 0 {
        return decode_error("flac: cuesheet track index reserved bits should be 0");
    }

    // Upper 8 bits is the index number.
    // TODO: Should be 0 or 1 for the first index for CD-DA. It should also not exceed 100 for
    //       CD-DA.
    let idx_number = ((idx_number_raw & 0xff00_0000) >> 24) as u8;

    // Calculate the track index's starting timestamp. Clamp it to the maximum
    let start_ts = track_ts
        .checked_add(n_offset_samples)
        .and_then(|ts| ts.try_into().ok())
        .unwrap_or_else(|| {
            warn!("cuesheet track index offset too large, clamping to maximum");
            i64::MAX
        });

    Ok(Chapter {
        start_time: tb.calc_time_saturating(Timestamp::from(start_ts)),
        end_time: None,
        start_byte: None,
        end_byte: None,
        tags: vec![Tag::new_from_parts(
            "INDEX",
            idx_number,
            Some(StandardTag::CdTrackIndex(idx_number)),
        )],
        visuals: Vec::new(),
    })
}

/// Convert a buffer slice containing most ASCII into a string identifier.
///
/// For each byte in the buffer, either convert and append it as a character if it is printable
/// ASCII character, or append it as an escaped hex value.
fn escape_identifier(buf: &[u8]) -> String {
    let mut ident = String::with_capacity(buf.len());

    for &byte in buf {
        if byte.is_ascii() && !byte.is_ascii_control() {
            ident.push(char::from(byte));
        }
        else {
            let u = (byte & 0xf0) >> 4;
            let l = byte & 0x0f;
            ident.push_str("\\0x");
            ident.push(if u < 10 { (b'0' + u) as char } else { (b'a' + u - 10) as char });
            ident.push(if l < 10 { (b'0' + l) as char } else { (b'a' + l - 10) as char });
        }
    }

    ident
}
