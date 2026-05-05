// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Frame body readers.

use std::char;
use std::collections::HashMap;
use std::io;
use std::str;
use std::sync::Arc;

use symphonia_core::errors::{Result, decode_error, unsupported_error};
use symphonia_core::io::{BufReader, FiniteStream, ReadBytes};
use symphonia_core::meta::RawTag;
use symphonia_core::meta::RawTagSubField;
use symphonia_core::meta::{Chapter, RawValue, StandardTag, Tag, Visual};
use symphonia_core::units::Time;
use symphonia_core::util::text;

use lazy_static::lazy_static;
use log::debug;
use smallvec::{SmallVec, smallvec};

use crate::id3v2::frames::{FrameResult, Id3v2Chapter, Id3v2TableOfContents};
use crate::id3v2::sub_fields::*;
use crate::utils::id3v2::get_visual_key_from_picture_type;
use crate::utils::std_tag::*;

use crate::utils::images::try_get_image_info;

/// Function pointer to an ID3v2 frame reader.
pub type FrameReader = fn(BufReader<'_>, &FrameInfo<'_>) -> Result<FrameResult>;

/// Map of 4 character ID3v2 frame IDs to a frame reader and optional raw tag parser pair.
pub type FrameReaderMap = HashMap<&'static [u8; 4], (FrameReader, Option<RawTagParser>)>;

/// Useful information about a frame for a frame reader.
pub struct FrameInfo<'a> {
    /// The original ID of the frame as written in the frame.
    id: &'a str,
    /// The major version of the ID3v2 tag containing the frame.
    major_version: u8,
    /// An optional raw tag parser to be applied if the frame is generic.
    raw_tag_parser: Option<RawTagParser>,
}

impl<'a> FrameInfo<'a> {
    /// Create new frame information from a pre-validated frame ID, tag version and optional
    /// raw tag parser.
    ///
    /// Panics if the frame ID is invalid.
    pub fn new(id: &'a [u8], major_version: u8, raw_tag_parser: Option<RawTagParser>) -> Self {
        FrameInfo {
            id: std::str::from_utf8(id).expect("validated frame id bytes"),
            major_version,
            raw_tag_parser,
        }
    }
}

/// Enumeration of valid encodings for text fields in ID3v2 tags
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
enum Encoding {
    /// ISO-8859-1 (aka Latin-1) characters in the range 0x20-0xFF.
    Iso8859_1,
    /// UTF-16 (or UCS-2) with a byte-order-mark (BOM). If the BOM is missing, big-endian encoding
    /// is assumed.
    Utf16Bom,
    /// UTF-16 big-endian without a byte-order-mark (BOM).
    Utf16Be,
    /// UTF-8.
    Utf8,
}

impl Encoding {
    fn parse(encoding: u8) -> Option<Encoding> {
        match encoding {
            // ISO-8859-1 terminated with 0x00.
            0 => Some(Encoding::Iso8859_1),
            // UTF-16 with byte order marker (BOM), terminated with 0x00 0x00.
            1 => Some(Encoding::Utf16Bom),
            // UTF-16BE without byte order marker (BOM), terminated with 0x00 0x00.
            2 => Some(Encoding::Utf16Be),
            // UTF-8 terminated with 0x00.
            3 => Some(Encoding::Utf8),
            // Invalid encoding.
            _ => None,
        }
    }
}

/// Decodes a slice of bytes containing encoded text into a `String`.
///
/// The ID3v2 specification forbids all control characters other than line-feed on the
/// ISO/IEC 8859-1 text encoding, however, does not state if the same limitation applies to the
/// Unicode encodings. Therefore, this restriction is not applied to other encodings.
fn decode_string_buf(buf: &[u8], encoding: Encoding) -> String {
    match encoding {
        Encoding::Iso8859_1 => {
            // Decode as an ID3v2-specific variant of ISO/IEC 8859-1 that allows the line-feed
            // control character.
            decode_id3v2_iso8859_1(buf).collect()
        }
        Encoding::Utf8 => {
            // Decode as UTF-8.
            String::from_utf8_lossy(buf).into_owned()
        }
        Encoding::Utf16Bom | Encoding::Utf16Be => {
            // Decode as UTF-16. If a byte-order-mark is present, it will be respected. Otherwise,
            // big-endian is assumed.
            text::decode_utf16be_lossy(buf).collect()
        }
    }
}

fn decode_id3v2_iso8859_1(buf: &[u8]) -> impl Iterator<Item = char> + '_ {
    buf.iter().map(|&c| {
        match c {
            // C0 control codes excluding line-feed.
            0x00..=0x09 | 0x0b..=0x1f => char::REPLACEMENT_CHARACTER,
            // C1 control codes.
            0x80..=0x9f => char::REPLACEMENT_CHARACTER,
            // All other non-control characters.
            _ => char::from(c),
        }
    })
}

// Primitive value readers (keep sorted in alphabetical order)
//------------------------------------------------------------

/// Read and validate a date.
fn read_date(reader: &mut BufReader<'_>) -> Result<String> {
    // Read an 8 character unterminated date string in the format "YYYYMMDD".
    let mut date = [0; 8];
    reader.read_buf_exact(&mut date)?;

    // All characters must be digits.
    if date.iter().any(|c| !c.is_ascii_digit()) {
        return decode_error("date format is invalid");
    }

    // Safety: The data array only contains ASCII digits.
    Ok(str::from_utf8(&date).unwrap().to_string())
}

/// Read and validate an encoding indicator.
fn read_encoding(reader: &mut BufReader<'_>) -> Result<Encoding> {
    match Encoding::parse(reader.read_byte()?) {
        Some(encoding) => Ok(encoding),
        _ => decode_error("invalid text encoding"),
    }
}

/// Read and validate a language code.
///
/// Language codes must conform to the ISO-639-2 standard. All codes should be composed of 3
/// alphabetic characters. ID3v2 further specifies a language code of "XXX" indicates an unknown or
/// unspecified language.
fn read_lang_code(reader: &mut BufReader<'_>) -> Result<Option<String>> {
    let code = reader.read_triple_bytes()?;

    if code.iter().any(|&c| !c.is_ascii_alphabetic()) {
        return decode_error("invalid language code");
    }

    let code = if code.eq_ignore_ascii_case(b"XXX") {
        // Unknown language code.
        None
    }
    else {
        // Convert to lowercase string.
        Some(std::str::from_utf8(&code).unwrap().to_ascii_lowercase())
    };

    Ok(code)
}

/// Read and validate the remainder of the buffer as a variably sized play counter.
fn read_play_counter(reader: &mut BufReader<'_>) -> Result<Option<u64>> {
    let len = reader.bytes_available() as usize;

    // A length of 0 indicates no play counter.
    if len == 0 {
        return Ok(None);
    }

    // A valid play counter must be a minimum of 4 bytes long.
    if len < 4 {
        return decode_error("play counter must be a minimum of 32 bits");
    }

    // However it may be extended by an arbitrary amount of bytes (or so it would seem).
    // Practically, a 4-byte (32-bit) count is way more than enough, but we'll support up-to an
    // 8-byte (64bit) count.
    if len > 8 {
        return unsupported_error("play counter greater-than 64 bits are not supported");
    }

    // The play counter is stored as an N-byte big-endian integer. Read N bytes into an 8-byte
    // buffer, making sure the missing bytes are zeroed, and then reinterpret as a 64-bit integer.
    let mut buf = [0u8; 8];
    reader.read_buf_exact(&mut buf[8 - len..])?;

    Ok(Some(u64::from_be_bytes(buf)))
}

/// Read a null-terminated string of the specified encoding from the stream. If the stream ends
/// before the null-terminator is reached, all the bytes up-to that point are interpreted as the
/// string.
fn read_string(reader: &mut BufReader<'_>, encoding: Encoding) -> io::Result<String> {
    let max_len = reader.bytes_available() as usize;

    let buf = match encoding {
        Encoding::Iso8859_1 | Encoding::Utf8 => {
            // Byte aligned encodings. The null-terminator is 1 byte.
            let buf = reader.scan_bytes_aligned_ref(&[0x00], 1, max_len)?;
            // Trim the trailing null-terminator, if present.
            match buf.last() {
                Some(b'\0') => &buf[..buf.len() - 1],
                _ => buf,
            }
        }
        Encoding::Utf16Bom | Encoding::Utf16Be => {
            // Two-byte aligned encodings. The null-terminator is 2 bytes.
            let buf = reader.scan_bytes_aligned_ref(&[0x00, 0x00], 2, max_len)?;
            // Trim the trailing null-terminator, if present.
            match buf.last_chunk::<2>() {
                Some(b"\0\0") => &buf[..buf.len() - 2],
                _ => buf,
            }
        }
    };

    Ok(decode_string_buf(buf, encoding))
}

/// Same behaviour as `read_string`, but ignores empty strings.
fn read_string_ignore_empty(
    reader: &mut BufReader<'_>,
    encoding: Encoding,
) -> io::Result<Option<String>> {
    Ok(Some(read_string(reader, encoding)?).filter(|text| !text.is_empty()))
}

/// Reads a list of strings where each string is null-terminated.
fn read_string_list(reader: &mut BufReader<'_>, encoding: Encoding) -> Result<RawValue> {
    // Optimize for the single string case.
    let mut items: SmallVec<[String; 1]> = Default::default();

    // Read the first string. If the reader is empty, this will push an empty string.
    items.push(read_string(reader, encoding)?);

    // Read additional strings.
    while reader.bytes_available() > 0 {
        items.push(read_string(reader, encoding)?);
    }

    let value = match items.len() {
        0 => unreachable!(),
        1 => RawValue::from(items.remove(0)),
        _ => RawValue::from(items.into_vec()),
    };

    Ok(value)
}

// Frame Readers (keep sorted in alphabetical order)
//--------------------------------------------------

/// Reads an `AENC` (audio encryption) frame.
pub fn read_aenc_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // The owner identifier string.
    let owner = read_string(&mut reader, Encoding::Iso8859_1)?;
    // Unencypted, "preview", audio start position in frames.
    let preview_start = reader.read_be_u16()?;
    // Unencrypted, "preview", audio length in frames.
    let preview_length = reader.read_be_u16()?;
    // The remainder of the frame is the binary encryption information.
    let encrypt_info = reader.read_buf_bytes_available_ref();

    let sub_fields = vec![
        RawTagSubField::new(AENC_OWNER, owner),
        RawTagSubField::new(AENC_PREVIEW_START, preview_start),
        RawTagSubField::new(AENC_PREVIEW_LENGTH, preview_length),
    ];

    // Create the tag.
    let raw = RawTag::new_with_sub_fields(frame.id, encrypt_info, sub_fields.into_boxed_slice());

    Ok(FrameResult::Tag(Tag::new(raw)))
}

/// Reads an `APIC` (attached picture) frame.
pub fn read_apic_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // The first byte of the frame is the encoding of the text description.
    let encoding = read_encoding(&mut reader)?;

    // Image format/media type
    let media_type = if frame.id == "PIC" {
        // Legacy PIC frames use a 3 character identifier. Only JPG and PNG are well-defined.
        match &reader.read_triple_bytes()? {
            b"JPG" => Some("image/jpeg"),
            b"PNG" => Some("image/png"),
            b"BMP" => Some("image/bmp"),
            b"GIF" => Some("image/gif"),
            _ => None,
        }
        .map(|s| s.to_string())
    }
    else {
        // APIC frames use a null-terminated ASCII media-type string.
        read_string_ignore_empty(&mut reader, Encoding::Iso8859_1)?
    };

    // Image usage.
    let usage = get_visual_key_from_picture_type(u32::from(reader.read_u8()?));

    let mut tags = vec![];

    // Null-teriminated image description in specified encoding.
    if let Some(desc) = read_string_ignore_empty(&mut reader, encoding)? {
        tags.push(Tag::new_from_parts("", "", Some(StandardTag::Description(Arc::from(desc)))));
    }

    // The remainder of the APIC frame is the image data.
    // TODO: Apply a limit.
    let data = Box::from(reader.read_buf_bytes_available_ref());

    // Try to get information about the image.
    let image_info = try_get_image_info(&data);

    let visual = Visual {
        media_type: image_info.as_ref().map(|info| info.media_type.clone()).or(media_type),
        dimensions: image_info.as_ref().map(|info| info.dimensions),
        color_mode: image_info.as_ref().map(|info| info.color_mode),
        usage,
        tags,
        data,
    };

    Ok(FrameResult::Visual(visual))
}

/// Reads an `ATXT` (audio encryption) frame.
pub fn read_atxt_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // The encoding.
    let encoding = read_encoding(&mut reader)?;
    // The audio data mime-type.
    let mime_type = read_string(&mut reader, Encoding::Iso8859_1)?;
    // Single flag indicating if the audio data is scrambled.
    let is_scrambled = reader.read_u8()? & 1 != 0;
    // Text equivalent to what is spoken in the audio data.
    let equivalent_text = read_string(&mut reader, encoding)?;
    // The audio clip.
    let mut audio_data: Box<[u8]> = Box::from(reader.read_buf_bytes_available_ref());

    // Unscramble the audio data if it has been scrambled.
    if is_scrambled {
        let mut prbs = 0xfe;
        for byte in audio_data.iter_mut() {
            *byte ^= prbs;
            // Advance the pseudorandom binary sequence.
            prbs = (((prbs << 1) ^ (prbs << 2)) & 0xfc) | (((prbs >> 6) ^ (prbs >> 4)) & 0x3);
        }
    }

    let sub_fields = vec![
        RawTagSubField::new(ATXT_EQUIVALENT_TEXT, equivalent_text),
        RawTagSubField::new(ATXT_MIME_TYPE, mime_type),
    ];

    // Create the tag.
    let raw = RawTag::new_with_sub_fields(frame.id, audio_data, sub_fields.into_boxed_slice());

    Ok(FrameResult::Tag(Tag::new(raw)))
}

/// Reads a `CHAP` (chapter) frame.
pub fn read_chap_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    use crate::id3v2::frames::{
        min_frame_size, read_id3v2p2_frame, read_id3v2p3_frame, read_id3v2p4_frame,
    };

    // Read the element ID.
    let id = read_string(&mut reader, Encoding::Iso8859_1)?;

    // Start time in ms.
    let start_ms = reader.read_be_u32()?;
    // End time in ms.
    let end_ms = reader.read_be_u32()?;
    // Optional start position in bytes.
    let start_byte = match reader.read_be_u32()? {
        u32::MAX => None,
        start_byte => Some(u64::from(start_byte)),
    };
    // Optional end position in bytes.
    let end_byte = match reader.read_be_u32()? {
        u32::MAX => None,
        end_byte => Some(u64::from(end_byte)),
    };

    // Read supplemental tags.
    let mut tags = vec![];
    let mut visuals = vec![];

    while reader.bytes_available() >= min_frame_size(frame.major_version) {
        let frame = match frame.major_version {
            2 => read_id3v2p2_frame(&mut reader),
            3 => read_id3v2p3_frame(&mut reader),
            4 => read_id3v2p4_frame(&mut reader),
            _ => break,
        }?;

        match frame {
            FrameResult::MultipleTags(tag_list) => tags.extend(tag_list.into_iter()),
            FrameResult::Tag(mut tag) => {
                // The TIT2 (track title) and TIT3 (track subtitle/description) ID3v2 frames are
                // repurposed for chapter title and description, respectively.
                tag.std = match tag.std.take() {
                    Some(StandardTag::TrackTitle(title)) => Some(StandardTag::ChapterTitle(title)),
                    Some(StandardTag::TrackSubtitle(desc)) => Some(StandardTag::Description(desc)),
                    other => other,
                };

                tags.push(tag)
            }
            FrameResult::Visual(visual) => visuals.push(visual),
            _ => {}
        }
    }

    let chapter = Id3v2Chapter {
        id,
        read_order: 0,
        chapter: Chapter {
            start_time: Time::from_millis(i64::from(start_ms)),
            end_time: Some(Time::from_millis(i64::from(end_ms))),
            start_byte,
            end_byte,
            tags,
            visuals,
        },
    };

    Ok(FrameResult::Chapter(chapter))
}

/// Reads a `COMM` (comment) frame.
pub fn read_comm_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // The first byte of the frame is the encoding of the description.
    let encoding = read_encoding(&mut reader)?;

    let mut sub_fields = Vec::new();

    // The next three bytes are the language.
    if let Some(lang) = read_lang_code(&mut reader)? {
        sub_fields.push(RawTagSubField::new(COMM_LANGUAGE, lang));
    }

    // Optional content description.
    if let Some(desc) = read_string_ignore_empty(&mut reader, encoding)? {
        sub_fields.push(RawTagSubField::new(COMM_SHORT_DESCRIPTION, desc));
        // TODO: A description of "iTunNORM" means the comment contains iTunes normalization data.
    }

    // The lyrics.
    let text = read_string(&mut reader, encoding)?;

    // Create the tag.
    let raw = RawTag::new_with_sub_fields(frame.id, text, sub_fields.into_boxed_slice());

    Ok(map_raw_tag(raw, Some(parse_comment)))
}

/// Reads a `COMR` (commercial) frame.
pub fn read_comr_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // The first byte of the frame is the encoding of the terms of use text.
    let encoding = read_encoding(&mut reader)?;

    // The price in the format: "<CURRENCY CODE 1><PRICE 1>[[/<CURRENCY CODE 2><PRICE 2>] ...]"
    let price = read_string(&mut reader, Encoding::Iso8859_1)?;

    let mut sub_fields = vec![
        // Price valid through this date.
        RawTagSubField::new(COMR_VALID_UNTIL, read_date(&mut reader)?),
        // Seller contact information (email address, URL, etc.)
        RawTagSubField::new(COMR_CONTACT_URL, read_string(&mut reader, Encoding::Iso8859_1)?),
        // How the audio was delivered. Takes one of the following values:
        // - 0x0  Other
        // - 0x1  Standard CD album with other songs
        // - 0x2  Compressed audio on CD
        // - 0x3  File over the Internet
        // - 0x4  Stream over the Internet
        // - 0x5  As note sheets
        // - 0x6  As note sheets in a book with other sheets
        // - 0x7  Music on other media
        // - 0x8  Non-musical merchandise
        RawTagSubField::new(COMR_RECEIVED_AS, reader.read_u8()?),
        // The seller name.
        RawTagSubField::new(COMR_SELLER_NAME, read_string(&mut reader, encoding)?),
        // The description of the product.
        RawTagSubField::new(COMR_DESCRIPTION, read_string(&mut reader, encoding)?),
    ];

    // Optional mime-type for the seller logo.
    let mime_type = read_string_ignore_empty(&mut reader, Encoding::Iso8859_1)?;
    // Optional seller logo.
    let logo = reader.read_buf_bytes_available_ref();

    if !logo.is_empty() {
        // Add the mime-type if it was provided.
        if let Some(mime_type) = mime_type {
            sub_fields.push(RawTagSubField::new(COMR_MIME_TYPE, mime_type));
        }
        // Add the seller logo data.
        sub_fields.push(RawTagSubField::new(COMR_SELLER_LOGO, logo));
    }

    // Create the tag.
    let raw = RawTag::new_with_sub_fields(frame.id, price, sub_fields.into_boxed_slice());

    Ok(FrameResult::Tag(Tag::new(raw)))
}

/// Reads a `CRM` (encrypted meta) frame.
pub fn read_crm_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    let owner = match read_string_ignore_empty(&mut reader, Encoding::Iso8859_1)? {
        Some(owner) => owner,
        None => {
            // Frame should be ignored if an owner is not specified.
            return Ok(FrameResult::Skipped);
        }
    };

    let desc = read_string_ignore_empty(&mut reader, Encoding::Iso8859_1)?;
    let data = reader.read_buf_bytes_available_ref();

    let mut sub_fields = vec![RawTagSubField::new(CRM_OWNER, owner)];

    if let Some(desc) = desc {
        sub_fields.push(RawTagSubField::new(CRM_DESRIPTION, desc));
    }

    // Create the tag.
    let raw = RawTag::new_with_sub_fields(frame.id, data, sub_fields.into_boxed_slice());

    Ok(FrameResult::Tag(Tag::new(raw)))
}

/// Reads a `CTOC` (table of contents) frame.
pub fn read_ctoc_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    use crate::id3v2::frames::{
        min_frame_size, read_id3v2p2_frame, read_id3v2p3_frame, read_id3v2p4_frame,
    };

    // Read for the element ID.
    let id = read_string(&mut reader, Encoding::Iso8859_1)?;

    // Read the flags.
    // - Bit 0 is the "ordered" bit. Indicates if the items should be played in order, or
    //   individually.
    // - Bit 1 is the "top-level" bit. Indicates if this table of contents is the root.
    let flags = reader.read_u8()?;
    // The number of items in this table of contents
    let entry_count = reader.read_u8()?;

    // Read child item element IDs.
    let mut items = Vec::with_capacity(usize::from(entry_count));

    for _ in 0..entry_count {
        let name = read_string(&mut reader, Encoding::Iso8859_1)?;
        items.push(name);
    }

    // Read supplemental tags.
    let mut tags = Vec::new();
    let mut visuals = Vec::new();

    while reader.bytes_available() >= min_frame_size(frame.major_version) {
        let frame = match frame.major_version {
            2 => read_id3v2p2_frame(&mut reader),
            3 => read_id3v2p3_frame(&mut reader),
            4 => read_id3v2p4_frame(&mut reader),
            _ => break,
        }?;

        match frame {
            FrameResult::MultipleTags(tag_list) => tags.extend(tag_list.into_iter()),
            FrameResult::Tag(tag) => tags.push(tag),
            FrameResult::Visual(visual) => visuals.push(visual),
            _ => {}
        }
    }

    let toc = Id3v2TableOfContents {
        id,
        top_level: flags & 2 != 0,
        ordered: flags & 1 != 0,
        items,
        tags,
        visuals,
    };

    Ok(FrameResult::TableOfContents(toc))
}

/// Reads an ENCR (encryption method registration) frame.
pub fn read_encr_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // The owner identifier string.
    let owner = read_string(&mut reader, Encoding::Iso8859_1)?;
    // The encryption method symbol.
    let method = reader.read_u8()?;
    // The remainder of the frame is encryption method data.
    let encryption_data = reader.read_buf_bytes_available_ref();

    let sub_fields = vec![
        RawTagSubField::new(ENCR_OWNER, owner),
        RawTagSubField::new(ENCR_ENCRYPTION_DATA, encryption_data),
    ];

    // Create the tag.
    let raw = RawTag::new_with_sub_fields(frame.id, method, sub_fields.into_boxed_slice());

    Ok(FrameResult::Tag(Tag::new(raw)))
}

/// Reads a GEOB (general encapsulated object) frame.
pub fn read_geob_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // The first byte of the frame is the encoding.
    let encoding = read_encoding(&mut reader)?;
    // The mime-type. This is mandatory.
    let mime_type = read_string(&mut reader, Encoding::Iso8859_1)?;
    // Optional filename.
    let file_name = read_string_ignore_empty(&mut reader, encoding)?;
    // Optional content description.
    let desc = read_string_ignore_empty(&mut reader, encoding)?;
    // The object data.
    let object = reader.read_buf_bytes_available_ref();

    let mut sub_fields = Vec::new();

    sub_fields.push(RawTagSubField::new(GEOB_MIME_TYPE, mime_type));

    if let Some(file_name) = file_name {
        sub_fields.push(RawTagSubField::new(GEOB_FILE_NAME, file_name));
    }

    if let Some(desc) = desc {
        sub_fields.push(RawTagSubField::new(GEOB_DESCRIPTION, desc));
    }

    // Create the tag.
    let raw = RawTag::new_with_sub_fields(frame.id, object, sub_fields.into_boxed_slice());

    Ok(FrameResult::Tag(Tag::new(raw)))
}

/// Reads a GRID (group ID) frame.
pub fn read_grid_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // The owner identifier string.
    let owner = read_string(&mut reader, Encoding::Iso8859_1)?;
    // The group ID/symbol.
    let group_id = reader.read_u8()?;
    // The remainder of the frame is group data.
    let group_data = reader.read_buf_bytes_available_ref();

    let sub_fields = vec![
        RawTagSubField::new(GRID_OWNER, owner),
        RawTagSubField::new(GRID_GROUP_DATA, group_data),
    ];

    // Create the tag.
    let raw = RawTag::new_with_sub_fields(frame.id, group_id, sub_fields.into_boxed_slice());

    Ok(FrameResult::Tag(Tag::new(raw)))
}

/// Reads a `MCDI` (music CD identifier) frame.
pub fn read_mcdi_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // The entire frame is a binary dump of a CD-DA TOC.
    let buf = reader.read_buf_bytes_ref(reader.byte_len() as usize)?;

    // TODO: Parse binary MCDI into hex-string based format as specified for the StandardTag.

    // Create the tag.
    let tag = Tag::new_from_parts(frame.id, buf, None);

    Ok(FrameResult::Tag(tag))
}

/// Reads a `OWNE` (terms of use) frame.
pub fn read_owne_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // The first byte of the frame is the encoding of the terms of use text.
    let encoding = read_encoding(&mut reader)?;
    // The price paid in the format: "<CURRENCY CODE><PRICE>".
    let price_paid = read_string(&mut reader, Encoding::Iso8859_1)?;

    let sub_fields = vec![
        // The date of purchase.
        RawTagSubField::new(OWNE_PURCHASE_DATE, read_date(&mut reader)?),
        // The name of the seller.
        RawTagSubField::new(OWNE_SELLER_NAME, read_string(&mut reader, encoding)?),
    ];

    // Create the tag.
    let raw = RawTag::new_with_sub_fields(frame.id, price_paid, sub_fields.into_boxed_slice());

    Ok(FrameResult::Tag(Tag::new(raw)))
}

/// Reads a `PCNT` (total file play count) frame.
pub fn read_pcnt_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // Read the mandatory play counter.
    let play_count = match read_play_counter(&mut reader)? {
        Some(count) => count,
        _ => return decode_error("pcnt: invalid play counter"),
    };

    // Create the tag.
    let tag = Tag::new_std(RawTag::new(frame.id, play_count), StandardTag::PlayCounter(play_count));

    Ok(FrameResult::Tag(tag))
}

/// Reads a `POPM` (popularimeter) frame.
pub fn read_popm_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    let mut sub_fields = vec![
        // The email of the user this frame belongs to.
        RawTagSubField::new(POPM_EMAIL, read_string(&mut reader, Encoding::Iso8859_1)?),
    ];

    // Read the rating.
    let rating = reader.read_u8()?;

    // Read the optional play counter. Add it to the sub-fields of the tag.
    if let Some(play_counter) = read_play_counter(&mut reader)? {
        sub_fields.push(RawTagSubField::new(POPM_PLAY_COUNTER, play_counter));
    }

    // The primary value of this frame is the rating as it is mandatory whereas the play counter is
    // not. Add the user's email and play counter as sub-fields.
    let raw = RawTag::new_with_sub_fields(frame.id, rating, sub_fields.into_boxed_slice());

    // Create the tag.
    let tag = Tag::new(raw);

    Ok(FrameResult::Tag(tag))
}

/// Reads a `POSS` (position synchronisation) frame.
pub fn read_poss_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // The units used for the position.
    let units = reader.read_u8()?;
    // The position.
    let position = reader.read_be_u32()?;

    let sub_fields = vec![RawTagSubField::new(POSS_POSITION_UNITS, units)];

    // Create the tag.
    let raw = RawTag::new_with_sub_fields(frame.id, position, sub_fields.into_boxed_slice());

    Ok(FrameResult::Tag(Tag::new(raw)))
}

/// Reads a `PRIV` (private) frame.
pub fn read_priv_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    let sub_fields = vec![
        // Read the owner identifier.
        RawTagSubField::new(PRIV_OWNER, read_string(&mut reader, Encoding::Iso8859_1)?),
    ];

    // The remainder of the frame is binary data.
    let data = reader.read_buf_bytes_ref(reader.bytes_available() as usize)?;

    // Create the tag.
    let tag = Tag::new(RawTag::new_with_sub_fields(frame.id, data, sub_fields.into_boxed_slice()));

    Ok(FrameResult::Tag(tag))
}

/// Reads the body of a frame into a buffer, and return it as a tag with the frame ID as the key.
pub fn read_raw_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    let data = reader.read_buf_bytes_available_ref();
    Ok(FrameResult::Tag(Tag::new_from_parts(frame.id, data, None)))
}

/// Reads a `SIGN` (signature) frame.
pub fn read_sign_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // The group ID this signature belongs to.
    let group_id = reader.read_u8()?;
    // The remainder of the frame is the signature data.
    let signature = reader.read_buf_bytes_available_ref();

    let sub_fields = vec![RawTagSubField::new(SIGN_GROUP_ID, group_id)];

    // Create the tag.
    let raw = RawTag::new_with_sub_fields(frame.id, signature, sub_fields.into_boxed_slice());

    Ok(FrameResult::Tag(Tag::new(raw)))
}

/// Reads all text frames frame except for `TXXX`.
pub fn read_text_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // The first byte of the frame is the encoding.
    let encoding = read_encoding(&mut reader)?;
    // Read 1 or more strings.
    let text = read_string_list(&mut reader, encoding)?;

    Ok(map_raw_tag(RawTag::new(frame.id, text), frame.raw_tag_parser))
}

/// Reads a `TIPL` (involved people frame).
pub fn read_tipl_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // The first byte of the frame is the encoding.
    let encoding = read_encoding(&mut reader)?;
    // Read 1 or more strings.
    let text = read_string_list(&mut reader, encoding)?;

    // This frame should contain a list of pairs, where the first item in each pair is the function,
    // and the second item is the person (or persons) involved in that function.
    //
    // Parsing this frame into standard tags could be error prone. If any of the following criteria
    // are not met, then the frame will be treated as a text frame with no standard tag(s) assigned:
    //
    //   1. The raw value MUST be a string list.
    //   2. The string list MUST contain an even number of elements.
    //   3. For each pair, the first item MUST map to a raw tag parser.
    //
    // Pre-validate these conditions to prevent doing extra work in the case all conditions do not
    // match.
    let is_parseable = match text {
        RawValue::StringList(ref list) if list.len() % 2 == 0 => {
            list.chunks_exact(2).all(|pair| TIPL_FUNC_PARSERS.contains_key(pair[0].as_str()))
        }
        _ => false,
    };

    let raw = RawTag::new(frame.id, text);

    if is_parseable {
        // Return standard tags per function.
        let mut tags: SmallVec<[Tag; 2]> = Default::default();

        if let RawValue::StringList(list) = &raw.value {
            // Iterate over all pairs, and parse them into standard tags.
            for pair in list.chunks_exact(2) {
                // Safety: Pre-checked above all pairs can be parsed.
                let parser = TIPL_FUNC_PARSERS.get(pair[0].as_str()).unwrap();

                // Parse raw value into standard tag.
                match parser(Arc::new(pair[1].clone())) {
                    [Some(std), None] => tags.push(Tag::new_std(raw.clone(), std)),
                    [None, Some(std)] => tags.push(Tag::new_std(raw.clone(), std)),
                    [Some(std0), Some(std1)] => {
                        tags.push(Tag::new_std(raw.clone(), std0));
                        tags.push(Tag::new_std(raw.clone(), std1));
                    }
                    _ => debug!("tipl frame pair value failed to parse into standard tag"),
                }
            }
        }

        Ok(FrameResult::MultipleTags(tags))
    }
    else {
        // Return as text frame.
        Ok(FrameResult::Tag(Tag::new(raw)))
    }
}

/// Reads a `TXXX` (user defined) text frame.
pub fn read_txxx_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // The first byte of the frame is the encoding.
    let encoding = read_encoding(&mut reader)?;
    // A description of the contents of the frame.
    let desc = read_string(&mut reader, encoding)?;
    // Read 1 or more strings.
    let text = read_string_list(&mut reader, encoding)?;

    // Some TXXX frames may be mapped to standard tags. Try to find a parser based on the
    // description string.
    let raw_tag_parser = TXXX_DESC_PARSERS.get(desc.to_ascii_lowercase().as_str()).copied();

    let sub_fields = vec![RawTagSubField::new(TXXX_DESCRIPTION, desc)];

    let raw = RawTag::new_with_sub_fields(frame.id, text, sub_fields.into_boxed_slice());

    Ok(map_raw_tag(raw, raw_tag_parser))
}

/// Readers an `UFID` (unique file identifier) frame.
pub fn read_ufid_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // The owner identifier.
    let owner = read_string(&mut reader, Encoding::Iso8859_1)?;
    // An up-to 64-byte identifier fills the rest of the frame.
    let id = reader.read_buf_bytes_available_ref();

    // 64-bytes is the limit of the identifier.
    if id.len() > 64 {
        return decode_error("ufid: ufid indentifier exceeds 64 bytes");
    }

    let sub_fields = vec![RawTagSubField::new(UFID_OWNER, owner)];

    // Create the tag.
    let raw = RawTag::new_with_sub_fields(frame.id, id, sub_fields.into_boxed_slice());

    Ok(FrameResult::Tag(Tag::new(raw)))
}

/// Reads all URL frames except for `WXXX`.
pub fn read_url_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // Read an URL string.
    let url = read_string(&mut reader, Encoding::Iso8859_1)?;

    // Create the tag.
    Ok(map_raw_tag(RawTag::new(frame.id, url), frame.raw_tag_parser))
}

/// Reads a USER (terms of use) frame.
pub fn read_user_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // The first byte of the frame is the encoding of the terms of use text.
    let encoding = read_encoding(&mut reader)?;

    let mut sub_fields = Vec::new();

    // The next three bytes are an ISO-639-2 language code.
    if let Some(lang) = read_lang_code(&mut reader)? {
        sub_fields.push(RawTagSubField::new(USER_LANGUAGE, lang));
    }

    // Finally, the terms of use.
    let terms = read_string(&mut reader, encoding)?;

    // Create the tag.
    let raw = RawTag::new_with_sub_fields(frame.id, terms, sub_fields.into_boxed_slice());

    Ok(map_raw_tag(raw, Some(parse_terms_of_use)))
}

/// Reads a `USLT` (unsynchronized comment) frame.
pub fn read_uslt_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // The first byte of the frame is the encoding of the description.
    let encoding = read_encoding(&mut reader)?;

    let mut sub_fields = Vec::new();

    // The language code.
    if let Some(lang) = read_lang_code(&mut reader)? {
        sub_fields.push(RawTagSubField::new(USLT_LANGUAGE, lang));
    }

    // Optional content description.
    if let Some(desc) = read_string_ignore_empty(&mut reader, encoding)? {
        sub_fields.push(RawTagSubField::new(USLT_DESCRIPTION, desc));
    }

    // The lyrics.
    let lyrics = read_string(&mut reader, encoding)?;

    // Create the tag.
    let raw = RawTag::new_with_sub_fields(frame.id, lyrics, sub_fields.into_boxed_slice());

    Ok(map_raw_tag(raw, Some(parse_lyrics)))
}

/// Reads a `WXXX` (user defined) URL frame.
pub fn read_wxxx_frame(mut reader: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    // The first byte of the WXXX frame is the encoding of the description.
    let encoding = read_encoding(&mut reader)?;

    let sub_fields = vec![
        // Read the description string.
        RawTagSubField::new(WXXX_DESCRIPTION, read_string(&mut reader, encoding)?),
    ];

    // Read the ISO-8859-1 URL string.
    let url = read_string(&mut reader, Encoding::Iso8859_1)?;

    // Create the tag.
    let raw = RawTag::new_with_sub_fields(frame.id, url, sub_fields.into_boxed_slice());

    Ok(map_raw_tag(raw, frame.raw_tag_parser))
}

/// Skip the frame.
pub fn skip_frame(_: BufReader<'_>, frame: &FrameInfo<'_>) -> Result<FrameResult> {
    debug!("skipping '{}' frame", frame.id);
    Ok(FrameResult::Skipped)
}

/// Attempt to map the raw tag into one or more standard tags.
fn map_raw_tag(raw: RawTag, parser: Option<RawTagParser>) -> FrameResult {
    if let Some(parser) = parser {
        // A parser was provided.
        if let RawValue::String(value) = &raw.value {
            // Parse and return frame result.
            match parser(value.clone()) {
                [Some(std), None] => {
                    // One raw tag yielded one standard tag.
                    return FrameResult::Tag(Tag::new_std(raw, std));
                }
                [None, Some(std)] => {
                    // One raw tag yielded one standard tag.
                    return FrameResult::Tag(Tag::new_std(raw, std));
                }
                [Some(std0), Some(std1)] => {
                    // One raw tag yielded two standards tags.
                    let tags = smallvec![Tag::new_std(raw.clone(), std0), Tag::new_std(raw, std1)];
                    return FrameResult::MultipleTags(tags);
                }
                // The raw value could not be parsed.
                _ => (),
            }
        }
    }

    // Could not parse, add a raw tag.
    FrameResult::Tag(Tag::new(raw))
}

// Loopup tables.

// Mapping TXXX descriptions to raw tag parsers.
lazy_static! {
    static ref TXXX_DESC_PARSERS: RawTagParserMap = {
        let mut m: RawTagParserMap = HashMap::new();
        m.insert("acoustid fingerprint", parse_acoustid_fingerprint);
        m.insert("acoustid id", parse_acoustid_id);
        m.insert("albumartistsort", parse_sort_album_artist);
        m.insert("asin", parse_ident_asin);
        m.insert("barcode", parse_ident_barcode);
        m.insert("catalognumber", parse_ident_catalog_number);
        m.insert("composersort", parse_sort_composer);
        m.insert("itunesadvistory", parse_itunes_content_advisory);
        m.insert("license", parse_license);
        m.insert("musicbrainz album artist id", parse_musicbrainz_album_artist_id);
        m.insert("musicbrainz album id", parse_musicbrainz_album_id);
        m.insert("musicbrainz album release country", parse_release_country);
        m.insert("musicbrainz album status", parse_musicbrainz_release_status);
        m.insert("musicbrainz album type", parse_musicbrainz_release_type);
        m.insert("musicbrainz artist id", parse_musicbrainz_artist_id);
        m.insert("musicbrainz disc id", parse_musicbrainz_disc_id);
        m.insert("musicbrainz original album id", parse_musicbrainz_original_album_id);
        m.insert("musicbrainz original artist id", parse_musicbrainz_original_artist_id);
        m.insert("musicbrainz release group id", parse_musicbrainz_release_group_id);
        m.insert("musicbrainz release track id", parse_musicbrainz_release_track_id);
        m.insert("musicbrainz trm id", parse_musicbrainz_trm_id);
        m.insert("musicbrainz work id", parse_musicbrainz_work_id);
        m.insert("releasedate", parse_release_date);
        m.insert("replaygain_album_gain", parse_replaygain_album_gain);
        m.insert("replaygain_album_peak", parse_replaygain_album_peak);
        m.insert("replaygain_album_range", parse_replaygain_album_range);
        m.insert("replaygain_reference_loudness", parse_replaygain_reference_loudness);
        m.insert("replaygain_track_gain", parse_replaygain_track_gain);
        m.insert("replaygain_track_peak", parse_replaygain_track_peak);
        m.insert("replaygain_track_range", parse_replaygain_track_range);
        m.insert("script", parse_script);
        m.insert("work", parse_work);
        m.insert("writer", parse_writer);
        m
    };
}

// Mapping TIPL "functions" to raw tag parsers.
lazy_static! {
    static ref TIPL_FUNC_PARSERS: RawTagParserMap = {
        let mut m: RawTagParserMap = HashMap::new();
        m.insert("arranger", parse_arranger);
        m.insert("engineer", parse_engineer);
        m.insert("dj-mix", parse_mix_dj);
        m.insert("mix", parse_mix_engineer);
        m.insert("producer", parse_producer);
        m
    };
}

#[cfg(test)]
mod tests {
    use super::Encoding;

    use symphonia_core::io::BufReader;

    #[test]
    fn verify_read_date() {
        use super::read_date;

        // Empty buffer.
        assert!(read_date(&mut BufReader::new(&[])).is_err());
        // Too few characters in buffer.
        assert!(read_date(&mut BufReader::new(b"0")).is_err());
        assert!(read_date(&mut BufReader::new(b"01")).is_err());
        assert!(read_date(&mut BufReader::new(b"012")).is_err());
        assert!(read_date(&mut BufReader::new(b"0123")).is_err());
        assert!(read_date(&mut BufReader::new(b"01234")).is_err());
        assert!(read_date(&mut BufReader::new(b"012345")).is_err());
        assert!(read_date(&mut BufReader::new(b"0123456")).is_err());
        // Non-digit characters.
        assert!(read_date(&mut BufReader::new(b"0123456a")).is_err());
        assert!(read_date(&mut BufReader::new(b"abcdefgh")).is_err());
        // Exact number of digits.
        assert_eq!(read_date(&mut BufReader::new(b"20000101")).unwrap(), "20000101");
        // Read only 8 digits.
        assert_eq!(read_date(&mut BufReader::new(b"0123456789abcdef")).unwrap(), "01234567");
    }

    #[test]
    fn verify_read_encoding() {
        use super::read_encoding;

        // Empty buffer.
        assert!(read_encoding(&mut BufReader::new(&[])).is_err());
        // Various valid encodings.
        assert_eq!(read_encoding(&mut BufReader::new(&[0])).unwrap(), Encoding::Iso8859_1);
        assert_eq!(read_encoding(&mut BufReader::new(&[1])).unwrap(), Encoding::Utf16Bom);
        assert_eq!(read_encoding(&mut BufReader::new(&[2])).unwrap(), Encoding::Utf16Be);
        assert_eq!(read_encoding(&mut BufReader::new(&[3])).unwrap(), Encoding::Utf8);
        // Invalid encodings.
        assert!(read_encoding(&mut BufReader::new(&[4])).is_err());
        assert!(read_encoding(&mut BufReader::new(&[5])).is_err());
    }

    #[test]
    fn verify_read_lang_code() {
        use super::read_lang_code;

        // Empty buffer.
        assert!(read_lang_code(&mut BufReader::new(&[])).is_err());
        // Too few characters in buffer.
        assert!(read_lang_code(&mut BufReader::new(b"")).is_err());
        assert!(read_lang_code(&mut BufReader::new(b"e")).is_err());
        assert!(read_lang_code(&mut BufReader::new(b"en")).is_err());
        // Non-alphabetic.
        assert!(read_lang_code(&mut BufReader::new(b"0")).is_err());
        assert!(read_lang_code(&mut BufReader::new(b"01")).is_err());
        assert!(read_lang_code(&mut BufReader::new(b"012")).is_err());
        assert!(read_lang_code(&mut BufReader::new(b"en1")).is_err());
        assert!(read_lang_code(&mut BufReader::new(b"en!")).is_err());
        assert!(read_lang_code(&mut BufReader::new(b"   ")).is_err());
        assert!(read_lang_code(&mut BufReader::new(b"---")).is_err());
        // Valid language codes.
        assert_eq!(read_lang_code(&mut BufReader::new(b"enu")).unwrap().as_deref(), Some("enu"));
        assert_eq!(read_lang_code(&mut BufReader::new(b"jpn")).unwrap().as_deref(), Some("jpn"));
        // Valid language codes, mixed case.
        assert_eq!(read_lang_code(&mut BufReader::new(b"cHi")).unwrap().as_deref(), Some("chi"));
        assert_eq!(read_lang_code(&mut BufReader::new(b"IND")).unwrap().as_deref(), Some("ind"));
        // Unknown language codes.
        assert_eq!(read_lang_code(&mut BufReader::new(b"xxx")).unwrap(), None);
        assert_eq!(read_lang_code(&mut BufReader::new(b"xxX")).unwrap(), None);
        assert_eq!(read_lang_code(&mut BufReader::new(b"xXx")).unwrap(), None);
        assert_eq!(read_lang_code(&mut BufReader::new(b"xXX")).unwrap(), None);
        assert_eq!(read_lang_code(&mut BufReader::new(b"Xxx")).unwrap(), None);
        assert_eq!(read_lang_code(&mut BufReader::new(b"XxX")).unwrap(), None);
        assert_eq!(read_lang_code(&mut BufReader::new(b"XXx")).unwrap(), None);
        assert_eq!(read_lang_code(&mut BufReader::new(b"XXX")).unwrap(), None);
    }

    #[test]
    fn verify_read_play_counter() {
        use super::read_play_counter;

        // None.
        assert_eq!(read_play_counter(&mut BufReader::new(&[])).unwrap(), None);
        // Too small buffer.
        assert!(read_play_counter(&mut BufReader::new(&[0])).is_err());
        assert!(read_play_counter(&mut BufReader::new(&[0, 0])).is_err());
        assert!(read_play_counter(&mut BufReader::new(&[0, 0, 0])).is_err());
        // Valid.
        assert_eq!(read_play_counter(&mut BufReader::new(&[0, 0, 0, 0])).unwrap(), Some(0));
        assert_eq!(read_play_counter(&mut BufReader::new(&[0, 0, 0, 0, 0])).unwrap(), Some(0));
        assert_eq!(read_play_counter(&mut BufReader::new(&[0, 0, 0, 0, 0, 0])).unwrap(), Some(0));
        assert_eq!(
            read_play_counter(&mut BufReader::new(&[0, 0, 0, 0, 0, 0, 0])).unwrap(),
            Some(0)
        );
        assert_eq!(
            read_play_counter(&mut BufReader::new(&[0, 0, 0, 0, 0, 0, 0, 0])).unwrap(),
            Some(0)
        );
        // Too large.
        assert!(read_play_counter(&mut BufReader::new(&[0, 0, 0, 0, 0, 0, 0, 0, 0])).is_err());
        // Valid values.
        assert_eq!(
            read_play_counter(&mut BufReader::new(&u32::MAX.to_be_bytes())).unwrap(),
            Some(u64::from(u32::MAX))
        );
        assert_eq!(
            read_play_counter(&mut BufReader::new(&u64::MAX.to_be_bytes())).unwrap(),
            Some(u64::MAX)
        );
        assert_eq!(
            read_play_counter(&mut BufReader::new(&[0x11, 0x22, 0x33, 0x44, 0x55, 0x66])).unwrap(),
            Some(18_838_586_676_582)
        );
    }

    #[test]
    fn verify_read_string() {
        use super::read_string;

        // Empty strings.
        assert_eq!(read_string(&mut BufReader::new(&[]), Encoding::Utf8).unwrap(), "");
        // Null-terminated empty string.
        assert_eq!(read_string(&mut BufReader::new(b"\0"), Encoding::Utf8).unwrap(), "");
        // Non-terminated string.
        assert_eq!(
            read_string(&mut BufReader::new(b"Hello! 123!"), Encoding::Utf8).unwrap(),
            "Hello! 123!"
        );
        // Null-terminated string.
        assert_eq!(
            read_string(&mut BufReader::new(b"Null-terminated.\0"), Encoding::Utf8).unwrap(),
            "Null-terminated."
        );
        assert_eq!(
            read_string(&mut BufReader::new(b"Null-terminated.\0\0\0\0"), Encoding::Utf8).unwrap(),
            "Null-terminated."
        );
        // Two null-terminated strings.
        assert_eq!(
            read_string(&mut BufReader::new(b"Part 1\0Part 2\0"), Encoding::Utf8).unwrap(),
            "Part 1"
        );
    }

    #[test]
    fn verify_read_string_ignore_empty() {
        use super::read_string_ignore_empty;

        // Empty strings.
        assert_eq!(
            read_string_ignore_empty(&mut BufReader::new(&[]), Encoding::Utf8).unwrap(),
            None
        );
        // Null-terminated empty string.
        assert_eq!(
            read_string_ignore_empty(&mut BufReader::new(b"\0"), Encoding::Utf8).unwrap(),
            None
        );
    }

    #[test]
    fn verify_read_string_list() {
        use super::read_string_list;

        use std::sync::Arc;

        use symphonia_core::meta::RawValue;

        // Single item.
        assert_eq!(
            read_string_list(&mut BufReader::new(b""), Encoding::Utf8).unwrap(),
            RawValue::String(Arc::new("".into()))
        );
        assert_eq!(
            read_string_list(&mut BufReader::new(b"\0"), Encoding::Utf8).unwrap(),
            RawValue::String(Arc::new("".into()))
        );
        assert_eq!(
            read_string_list(&mut BufReader::new(b"Hello"), Encoding::Utf8).unwrap(),
            RawValue::String(Arc::new("Hello".into()))
        );
        assert_eq!(
            read_string_list(&mut BufReader::new(b"Hello\0"), Encoding::Utf8).unwrap(),
            RawValue::String(Arc::new("Hello".into()))
        );
        // Multiple items.
        assert_eq!(
            read_string_list(&mut BufReader::new(b"#1\0#2"), Encoding::Utf8).unwrap(),
            RawValue::StringList(Arc::new(vec!["#1".to_string(), "#2".to_string()]))
        );
        assert_eq!(
            read_string_list(&mut BufReader::new(b"#1\0#2\0"), Encoding::Utf8).unwrap(),
            RawValue::StringList(Arc::new(vec!["#1".to_string(), "#2".to_string()]))
        );
        // Empty items.
        assert_eq!(
            read_string_list(&mut BufReader::new(b"#1\0#2\0\0"), Encoding::Utf8).unwrap(),
            RawValue::StringList(Arc::new(vec![
                "#1".to_string(),
                "#2".to_string(),
                "".to_string()
            ]))
        );
        assert_eq!(
            read_string_list(&mut BufReader::new(b"#1\0#2\0\0\0"), Encoding::Utf8).unwrap(),
            RawValue::StringList(Arc::new(vec![
                "#1".to_string(),
                "#2".to_string(),
                "".to_string(),
                "".to_string()
            ]))
        );
        // All empty.
        assert_eq!(
            read_string_list(&mut BufReader::new(b"\0\0"), Encoding::Utf8).unwrap(),
            RawValue::StringList(Arc::new(vec!["".to_string(), "".to_string()]))
        );
    }
}
