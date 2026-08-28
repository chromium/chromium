// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::Arc;

use symphonia_core::errors::Error;
use symphonia_core::io::{BufReader, ReadBytes};
use symphonia_core::meta::well_known::METADATA_ID_ISOMP4;
use symphonia_core::meta::{
    ContentAdvisory, MetadataBuilder, MetadataInfo, MetadataRevision, RawTag, StandardTag,
    StandardVisualKey, Tag,
};
use symphonia_core::meta::{RawValue, Visual};
use symphonia_core::util::{bits, text};
use symphonia_metadata::utils::images::try_get_image_info;
use symphonia_metadata::utils::{id3v1, itunes};

use crate::atoms::{Atom, AtomHeader, AtomIterator, AtomType, ReadAtom, Result, decode_error};

use log::{debug, warn};

const ISOMP4_METADATA_INFO: MetadataInfo = MetadataInfo {
    metadata: METADATA_ID_ISOMP4,
    short_name: "isomp4",
    long_name: "ISO Base Media File Format",
};

/// Data type enumeration for metadata value atoms as defined in the QuickTime File Format standard.
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum DataType {
    AffineTransformF64,
    Bmp,
    DimensionsF32,
    Float32,
    Float64,
    Jpeg,
    /// The data type is implicit to the atom.
    NoType,
    Png,
    PointF32,
    QuickTimeMetadata,
    RectF32,
    ShiftJis,
    SignedInt16,
    SignedInt32,
    SignedInt64,
    SignedInt8,
    SignedIntVariable,
    UnsignedInt16,
    UnsignedInt32,
    UnsignedInt64,
    UnsignedInt8,
    UnsignedIntVariable,
    Utf16,
    Utf16Sort,
    Utf8,
    Utf8Sort,
    #[allow(dead_code)]
    Unknown(u32),
}

impl From<u32> for DataType {
    fn from(value: u32) -> Self {
        match value {
            0 => DataType::NoType,
            1 => DataType::Utf8,
            2 => DataType::Utf16,
            3 => DataType::ShiftJis,
            4 => DataType::Utf8Sort,
            5 => DataType::Utf16Sort,
            13 => DataType::Jpeg,
            14 => DataType::Png,
            21 => DataType::SignedIntVariable,
            22 => DataType::UnsignedIntVariable,
            23 => DataType::Float32,
            24 => DataType::Float64,
            27 => DataType::Bmp,
            28 => DataType::QuickTimeMetadata,
            65 => DataType::SignedInt8,
            66 => DataType::SignedInt16,
            67 => DataType::SignedInt32,
            70 => DataType::PointF32,
            71 => DataType::DimensionsF32,
            72 => DataType::RectF32,
            74 => DataType::SignedInt64,
            75 => DataType::UnsignedInt8,
            76 => DataType::UnsignedInt16,
            77 => DataType::UnsignedInt32,
            78 => DataType::UnsignedInt64,
            79 => DataType::AffineTransformF64,
            _ => DataType::Unknown(value),
        }
    }
}

fn parse_no_type(data: &[u8]) -> Option<RawValue> {
    // Return as binary data.
    Some(RawValue::from(data))
}

fn parse_utf8(data: &[u8]) -> Option<RawValue> {
    // UTF8, no null-terminator or count.
    let text = String::from_utf8_lossy(data);
    Some(RawValue::from(text))
}

fn parse_utf16(data: &[u8]) -> Option<RawValue> {
    // UTF16 BE
    let text = text::decode_utf16be_lossy(data).collect::<String>();
    Some(RawValue::from(text))
}

fn parse_signed_int8(data: &[u8]) -> Option<RawValue> {
    match data.len() {
        1 => {
            let s = bits::sign_extend_leq8_to_i8(data[0], 8);
            Some(RawValue::from(s))
        }
        _ => None,
    }
}

fn parse_signed_int16(data: &[u8]) -> Option<RawValue> {
    match data.len() {
        2 => {
            let u = BufReader::new(data).read_be_u16().ok()?;
            let s = bits::sign_extend_leq16_to_i16(u, 16);
            Some(RawValue::from(s))
        }
        _ => None,
    }
}

fn parse_signed_int32(data: &[u8]) -> Option<RawValue> {
    match data.len() {
        4 => {
            let u = BufReader::new(data).read_be_u32().ok()?;
            let s = bits::sign_extend_leq32_to_i32(u, 32);
            Some(RawValue::from(s))
        }
        _ => None,
    }
}

fn parse_signed_int64(data: &[u8]) -> Option<RawValue> {
    match data.len() {
        8 => {
            let u = BufReader::new(data).read_be_u64().ok()?;
            let s = bits::sign_extend_leq64_to_i64(u, 64);
            Some(RawValue::from(s))
        }
        _ => None,
    }
}

fn parse_var_signed_int(data: &[u8]) -> Option<RawValue> {
    match data.len() {
        1 => parse_signed_int8(data),
        2 => parse_signed_int16(data),
        4 => parse_signed_int32(data),
        8 => parse_signed_int64(data),
        _ => None,
    }
}

fn parse_unsigned_int8(data: &[u8]) -> Option<RawValue> {
    match data.len() {
        1 => Some(RawValue::from(data[0])),
        _ => None,
    }
}

fn parse_unsigned_int16(data: &[u8]) -> Option<RawValue> {
    match data.len() {
        2 => {
            let u = BufReader::new(data).read_be_u16().ok()?;
            Some(RawValue::from(u))
        }
        _ => None,
    }
}

fn parse_unsigned_int32(data: &[u8]) -> Option<RawValue> {
    match data.len() {
        4 => {
            let u = BufReader::new(data).read_be_u32().ok()?;
            Some(RawValue::from(u))
        }
        _ => None,
    }
}

fn parse_unsigned_int64(data: &[u8]) -> Option<RawValue> {
    match data.len() {
        8 => {
            let u = BufReader::new(data).read_be_u64().ok()?;
            Some(RawValue::from(u))
        }
        _ => None,
    }
}

fn parse_var_unsigned_int(data: &[u8]) -> Option<RawValue> {
    match data.len() {
        1 => parse_unsigned_int8(data),
        2 => parse_unsigned_int16(data),
        4 => parse_unsigned_int32(data),
        8 => parse_unsigned_int64(data),
        _ => None,
    }
}

fn parse_float32(data: &[u8]) -> Option<RawValue> {
    match data.len() {
        4 => {
            let f = BufReader::new(data).read_be_f32().ok()?;
            Some(RawValue::Float(f64::from(f)))
        }
        _ => None,
    }
}

fn parse_float64(data: &[u8]) -> Option<RawValue> {
    match data.len() {
        8 => {
            let f = BufReader::new(data).read_be_f64().ok()?;
            Some(RawValue::Float(f))
        }
        _ => None,
    }
}

fn parse_tag_value(data_type: DataType, data: &[u8]) -> Option<RawValue> {
    match data_type {
        DataType::NoType => parse_no_type(data),
        DataType::Utf8 | DataType::Utf8Sort => parse_utf8(data),
        DataType::Utf16 | DataType::Utf16Sort => parse_utf16(data),
        DataType::UnsignedInt8 => parse_unsigned_int8(data),
        DataType::UnsignedInt16 => parse_unsigned_int16(data),
        DataType::UnsignedInt32 => parse_unsigned_int32(data),
        DataType::UnsignedInt64 => parse_unsigned_int64(data),
        DataType::UnsignedIntVariable => parse_var_unsigned_int(data),
        DataType::SignedInt8 => parse_signed_int8(data),
        DataType::SignedInt16 => parse_signed_int16(data),
        DataType::SignedInt32 => parse_signed_int32(data),
        DataType::SignedInt64 => parse_signed_int64(data),
        DataType::SignedIntVariable => parse_var_signed_int(data),
        DataType::Float32 => parse_float32(data),
        DataType::Float64 => parse_float64(data),
        _ => None,
    }
}

/// Reads and parses a `MetaTagAtom` from the provided iterator and adds it to the `MetadataBuilder`
/// if there are no errors.
fn add_generic_tag<R: ReadAtom>(
    it: &mut AtomIterator<R>,
    builder: &mut MetadataBuilder,
    map: fn(&RawValue) -> Option<StandardTag>,
) -> Result<()> {
    let tag = it.read_atom::<MetaTagAtom>()?;

    let raw_key = get_raw_tag_key(tag.atom_type);

    for value_atom in tag.values.iter() {
        // Parse the value atom data into a raw value of any type, if possible.
        if let Some(raw_value) = parse_tag_value(value_atom.data_type, &value_atom.data) {
            let std_tag = map(&raw_value);
            builder.add_tag(Tag::new_from_parts(raw_key, raw_value, std_tag));
        }
        else {
            warn!("unsupported data type {:?} for {:?} tag", value_atom.data_type, tag.atom_type);
        }
    }

    Ok(())
}

fn add_flag_tag<R: ReadAtom>(
    iter: &mut AtomIterator<R>,
    builder: &mut MetadataBuilder,
    map: fn(&RawValue) -> Option<StandardTag>,
) -> Result<()> {
    let tag = iter.read_atom::<MetaTagAtom>()?;

    // There should be exactly 1 value.
    if let Some(value_atom) = tag.values.first() {
        // Parse the value atom's data to get the raw value of the flag.
        if let Some(raw_value) = parse_tag_value(value_atom.data_type, &value_atom.data) {
            let raw_key = get_raw_tag_key(tag.atom_type);
            let std_tag = map(&raw_value);
            builder.add_tag(Tag::new_from_parts(raw_key, raw_value, std_tag));
        }
    }

    Ok(())
}

fn add_pair_tag<R: ReadAtom>(
    iter: &mut AtomIterator<R>,
    builder: &mut MetadataBuilder,
    map0: fn(&RawValue) -> Option<StandardTag>,
    map1: fn(&RawValue) -> Option<StandardTag>,
) -> Result<()> {
    let tag = iter.read_atom::<MetaTagAtom>()?;

    // There should be exactly 1 value.
    if let Some(value) = tag.values.first() {
        let data_len = value.data.len();

        // The "trkn"/"disk" atoms contain a 8/6 byte value buffer consisting of 4/3-tuple of 16-bit
        // big-endian values. The data type is no-type.
        if data_len == 8 || data_len == 6 {
            let raw_key = get_raw_tag_key(tag.atom_type);

            // The first value is reserved, the second value is the track or disk number, the third
            // value is the track or disk total, and the fourth is reserved (if present).
            let d0 =
                u16::from_be_bytes(value.data[2..4].try_into().expect("slice is exactly 2 bytes"));
            let d1 =
                u16::from_be_bytes(value.data[4..6].try_into().expect("slice is exactly 2 bytes"));

            // Ignore a track/disk number of 0.
            if d0 > 0 {
                let rv0 = RawValue::from(d0);
                builder.add_tag(Tag::new_from_parts(raw_key, rv0.clone(), map0(&rv0)));
            }

            // Ignore a track/disk total of 0.
            if d1 > 0 {
                let rv1 = RawValue::from(d1);
                builder.add_tag(Tag::new_from_parts(raw_key, rv1.clone(), map1(&rv1)));
            }
        }
    }

    Ok(())
}

fn add_visual_tag<R: ReadAtom>(
    iter: &mut AtomIterator<R>,
    builder: &mut MetadataBuilder,
) -> Result<()> {
    let tag = iter.read_atom::<MetaTagAtom>()?;

    // There could be more than one attached image.
    for value in tag.values {
        let image_info = try_get_image_info(&value.data);

        builder.add_visual(Visual {
            media_type: image_info.as_ref().map(|info| info.media_type.clone()),
            dimensions: image_info.as_ref().map(|info| info.dimensions),
            color_mode: image_info.as_ref().map(|info| info.color_mode),
            usage: Some(StandardVisualKey::FrontCover),
            tags: Default::default(),
            data: value.data,
        });
    }

    Ok(())
}

fn add_advisory_tag<R: ReadAtom>(
    iter: &mut AtomIterator<R>,
    builder: &mut MetadataBuilder,
) -> Result<()> {
    let tag = iter.read_atom::<MetaTagAtom>()?;

    // There should be exactly 1 value.
    if let Some(value_atom) = tag.values.first() {
        // Parse the value atom's data to get the raw value of the advisory tag.
        if let Some(raw_value) = parse_tag_value(value_atom.data_type, &value_atom.data) {
            // The value should be a signed integer.
            let advisory = match raw_value {
                RawValue::SignedInt(value) => {
                    // The value may take 1 of 4 values indicating the explicitness of the media.
                    match value {
                        0 => Some(ContentAdvisory::None),
                        1 | 4 => Some(ContentAdvisory::Explicit),
                        2 => Some(ContentAdvisory::Censored),
                        _ => {
                            warn!("unknown content advisory value {value}");
                            None
                        }
                    }
                }
                _ => {
                    warn!("invalid data type for content advisory tag");
                    None
                }
            };

            if let Some(advisory) = advisory {
                let raw_key = get_raw_tag_key(tag.atom_type);

                builder.add_tag(Tag::new_std(
                    RawTag::new(raw_key, raw_value),
                    StandardTag::ContentAdvisory(advisory),
                ));
            }
        }
        else {
            warn!("unsupported data type {:?} for {:?} tag", value_atom.data_type, tag.atom_type);
        }
    }

    Ok(())
}

fn add_media_type_tag<R: ReadAtom>(
    iter: &mut AtomIterator<R>,
    builder: &mut MetadataBuilder,
) -> Result<()> {
    let tag = iter.read_atom::<MetaTagAtom>()?;

    // There should be exactly 1 value.
    if let Some(value_atom) = tag.values.first() {
        // Parse the value atom's data to get the raw value of the media type tag.
        if let Some(raw_value) = parse_tag_value(value_atom.data_type, &value_atom.data) {
            // The value should be a signed integer.
            let media_type = match raw_value {
                RawValue::SignedInt(value) => {
                    // The value may take 1 of many values indicating the media type.
                    let media_type = match value {
                        0 => "Movie",
                        1 => "Normal",
                        2 => "Audio Book",
                        5 => "Whacked Bookmark",
                        6 => "Music Video",
                        9 => "Short Film",
                        10 => "TV Show",
                        11 => "Booklet",
                        _ => "Unknown",
                    };
                    Some(media_type)
                }
                _ => {
                    warn!("invalid data type for media type tag");
                    None
                }
            };

            if let Some(media_type) = media_type {
                let raw_key = get_raw_tag_key(tag.atom_type);

                builder.add_tag(Tag::new_std(
                    RawTag::new(raw_key, raw_value),
                    StandardTag::MediaFormat(Arc::new(String::from(media_type))),
                ));
            }
        }
        else {
            warn!("unsupported data type {:?} for {:?} tag", value_atom.data_type, tag.atom_type);
        }
    }

    Ok(())
}

fn add_id3v1_genre_tag<R: ReadAtom>(
    iter: &mut AtomIterator<R>,
    builder: &mut MetadataBuilder,
) -> Result<()> {
    let tag = iter.read_atom::<MetaTagAtom>()?;

    // There should be exactly 1 value.
    if let Some(value_atom) = tag.values.first() {
        // The ID3v1 genre number is stored as a unsigned 16-bit big-endian integer. The data type
        // is no-type.
        if value_atom.data.len() == 2 {
            let index = u16::from_be_bytes(
                value_atom.data.as_ref().try_into().expect("data.len() == 2 checked above"),
            );

            // The stored index uses 1-based indexing, but the ID3v1 genre list is 0-based.
            let genre = match index {
                1..=255 => id3v1::get_genre_name((index - 1) as u8),
                _ => None,
            };

            let raw_key = get_raw_tag_key(tag.atom_type);

            builder.add_tag(Tag::new_from_parts(
                raw_key,
                RawValue::UnsignedInt(u64::from(index)),
                genre.map(|genre| StandardTag::Genre(Arc::new(genre))),
            ));
        }
    }

    Ok(())
}

fn add_rating_tag<R: ReadAtom>(
    iter: &mut AtomIterator<R>,
    builder: &mut MetadataBuilder,
) -> Result<()> {
    let tag = iter.read_atom::<MetaTagAtom>()?;

    // There should be exactly 1 value.
    if let Some(value_atom) = tag.values.first() {
        // Parse the value atom's data to get the raw value of the rating.
        if let Some(raw_value) = parse_tag_value(value_atom.data_type, &value_atom.data) {
            // The value should be a signed integer between 0 - 100.
            let rating = match raw_value {
                RawValue::SignedInt(value) if value >= 0 && value <= 100 => Some(value as u32),
                _ => {
                    warn!("invalid data for rating tag");
                    None
                }
            };

            if let Some(rating) = rating {
                let raw_key = get_raw_tag_key(tag.atom_type);

                builder.add_tag(Tag::new_std(
                    RawTag::new(raw_key, raw_value),
                    StandardTag::Rating(10_000 * rating),
                ));
            }
        }
        else {
            warn!("unsupported data type {:?} for {:?} tag", value_atom.data_type, tag.atom_type);
        }
    }

    Ok(())
}

fn add_freeform_tag<R: ReadAtom>(
    iter: &mut AtomIterator<R>,
    builder: &mut MetadataBuilder,
) -> Result<()> {
    let tag = iter.read_atom::<MetaTagAtom>()?;

    // A user-defined tag should only have 1 value.
    for value_atom in tag.values.iter() {
        // Parse the value atom data into a raw value, if possible.
        if let Some(value) = parse_tag_value(value_atom.data_type, &value_atom.data) {
            // Try to map iTunes freeform tags to standard tag keys.
            let _ = itunes::parse_itunes_tag(tag.full_name(), value, builder);
        }
        else {
            warn!("unsupported data type {:?} for freeform tag", value_atom.data_type);
        }
    }

    Ok(())
}

/// Metadata tag data atom.
#[allow(dead_code)]
pub struct MetaTagDataAtom {
    /// Tag data.
    pub data: Box<[u8]>,
    /// The data type contained in buf.
    pub data_type: DataType,
}

impl Atom for MetaTagDataAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        // For the MOV container, the bytes occupied by the version and flags fields is the type
        // indicator.
        //
        // The byte normally occupied by the version number is actually a data type set indicator
        // and must always be 0 (indicating the data type will come from the well-known set).
        // The next 3 bytes, normally occupied by the atom flags, indicate an index into the table
        // of well-known data types.
        //
        // For the ISO/MP4 BMFF container, the version number is always 0, and the flags also
        // indicate an index into a table of well-known data types. Therefore, MOV and the ISO/MP4
        // BMFF are compatible if the version is 0.
        let (version, flags) = it.read_extended_header()?;

        if version != 0 {
            return decode_error("isomp4 (data): invalid data atom version");
        }

        // Lookup the well-known type.
        let data_type = DataType::from(flags);

        // The next 4 bytes form a locale indicator consisting of two 2-byte fields for the country
        // and language codes, respectively.
        //
        // For both fields, a value of 0 indicates default, a value of 1-255 indicate the index of
        // a country/language list stored in the country/language sub-atoms of the parent `meta`
        // atom, and a value > 255 indicate an ISO-3166 country code or a packed ISO-639-2 language
        // code.
        let _country = it.read_u16()?;
        let _language = it.read_u16()?;

        // The data payload is the remainder of the atom.
        let data = {
            // TODO: Apply the metadata limit.
            let size = it
                .data_left()?
                .ok_or(Error::DecodeError("isomp4 (data): expected atom size to be known"))?;

            it.read_boxed_slice_exact(size as usize)?
        };

        Ok(MetaTagDataAtom { data, data_type })
    }
}

/// Metadata tag name and mean atom.
#[allow(dead_code)]
pub struct MetaTagNamespaceAtom {
    /// For 'mean' atoms, this is the key namespace. For 'name' atom, this is the key name.
    pub value: String,
}

impl Atom for MetaTagNamespaceAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        let (_, _) = it.read_extended_header()?;

        let size = it
            .data_left()?
            .ok_or(Error::DecodeError("isomp4 (ilst): expected atom size to be known"))?;

        // TODO: Apply a metadata limit.
        let buf = it.read_boxed_slice_exact(size as usize)?;

        // Do a lossy conversion because metadata should not prevent the demuxer from working.
        let value = String::from_utf8_lossy(&buf).to_string();

        Ok(MetaTagNamespaceAtom { value })
    }
}

/// A generic metadata tag atom.
#[allow(dead_code)]
pub struct MetaTagAtom {
    /// The atom type for the tag.
    pub atom_type: AtomType,
    /// Tag value(s).
    pub values: Vec<MetaTagDataAtom>,
    /// Optional, tag key namespace.
    pub mean: Option<MetaTagNamespaceAtom>,
    /// Optional, tag key name.
    pub name: Option<MetaTagNamespaceAtom>,
}

impl MetaTagAtom {
    pub fn full_name(&self) -> String {
        let mut full_name = String::new();

        if self.mean.is_some() || self.name.is_some() {
            // full_name.push_str("----:");

            if let Some(mean) = &self.mean {
                full_name.push_str(&mean.value);
            }

            full_name.push(':');

            if let Some(name) = &self.name {
                full_name.push_str(&name.value);
            }
        }

        full_name
    }
}

impl Atom for MetaTagAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, header: &AtomHeader) -> Result<Self> {
        let atom_type = header.atom_type();

        let mut mean = None;
        let mut name = None;
        let mut values = Vec::new();

        while let Some(header) = it.next_header()? {
            match header.atom_type {
                AtomType::MetaTagData => {
                    values.push(it.read_atom::<MetaTagDataAtom>()?);
                }
                AtomType::MetaTagName => {
                    name = Some(it.read_atom::<MetaTagNamespaceAtom>()?);
                }
                AtomType::MetaTagMeaning => {
                    mean = Some(it.read_atom::<MetaTagNamespaceAtom>()?);
                }
                _ => (),
            }
        }

        Ok(MetaTagAtom { atom_type, values, mean, name })
    }
}

macro_rules! map_std_str {
    ($std:path) => {
        |value: &RawValue| match value {
            RawValue::String(s) => Some($std(s.clone())),
            _ => None,
        }
    };
}

macro_rules! map_std_bool {
    ($std:path) => {
        |value: &RawValue| match value {
            // A boolean value as-is.
            RawValue::Boolean(b) => Some($std(*b)),
            // A flag is always true.
            RawValue::Flag => Some($std(true)),
            // A signed integer value of 0 is false, otherwise true.
            RawValue::SignedInt(value) => Some($std(*value != 0)),
            // An unsigned integer value of 0 is false, otherwise true.
            RawValue::UnsignedInt(value) => Some($std(*value != 0)),
            _ => None,
        }
    };
}

macro_rules! map_std_uint {
    ($std:path) => {
        |value: &RawValue| match value {
            // Positive signed numbers.
            RawValue::SignedInt(v) => (*v).try_into().ok().map(|v| $std(v)),
            // Any unsigned number.
            RawValue::UnsignedInt(v) => Some($std(*v)),
            _ => None,
        }
    };
}

/// User data atom.
#[allow(dead_code)]
pub struct IlstAtom {
    /// Metadata revision.
    pub metadata: MetadataRevision,
}

impl Atom for IlstAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        let mut mb = MetadataBuilder::new(ISOMP4_METADATA_INFO);

        while let Some(header) = it.next_header()? {
            // Ignore standard atoms, check if other is a metadata atom.
            match &header.atom_type {
                AtomType::AdvisoryTag => add_advisory_tag(it, &mut mb)?,
                AtomType::AlbumArtistTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::AlbumArtist))?
                }
                AtomType::AlbumTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Album))?
                }
                AtomType::ArrangerTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Arranger))?
                }
                AtomType::ArtistTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Artist))?
                }
                AtomType::AuthorTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Author))?
                }
                AtomType::PodcastCategoryTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::PodcastCategory))?
                }
                AtomType::CommentTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Comment))?
                }
                AtomType::CompilationTag => {
                    add_flag_tag(it, &mut mb, map_std_bool!(StandardTag::CompilationFlag))?
                }
                AtomType::ComposerTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Composer))?
                }
                AtomType::ConductorTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Conductor))?
                }
                AtomType::CopyrightTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Copyright))?
                }
                AtomType::CoverTag => add_visual_tag(it, &mut mb)?,
                AtomType::CustomGenreTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Genre))?
                }
                AtomType::DateTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::ReleaseDate))?
                }
                AtomType::DescriptionTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Description))?
                }
                AtomType::DiskNumberTag => add_pair_tag(
                    it,
                    &mut mb,
                    map_std_uint!(StandardTag::DiscNumber),
                    map_std_uint!(StandardTag::DiscTotal),
                )?,
                AtomType::EncodedByTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::EncodedBy))?
                }
                AtomType::EncoderTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Encoder))?
                }
                AtomType::GaplessPlaybackTag => add_flag_tag(it, &mut mb, |_| None)?,
                AtomType::GenreTag => add_id3v1_genre_tag(it, &mut mb)?,
                AtomType::GroupingTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Grouping))?
                }
                AtomType::HdVideoTag => (),
                AtomType::IdentPodcastTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::IdentPodcast))?
                }
                AtomType::IsrcTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::IdentIsrc))?
                }
                AtomType::LabelTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Label))?
                }
                AtomType::LabelUrlTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::UrlLabel))?
                }
                AtomType::PodcastKeywordsTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::PodcastKeywords))?
                }
                AtomType::LongDescriptionTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Description))?
                }
                AtomType::LyricsTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Lyrics))?
                }
                AtomType::MediaTypeTag => add_media_type_tag(it, &mut mb)?,
                AtomType::MovementCountTag => {
                    add_generic_tag(it, &mut mb, map_std_uint!(StandardTag::MovementTotal))?
                }
                AtomType::MovementIndexTag => {
                    add_generic_tag(it, &mut mb, map_std_uint!(StandardTag::MovementNumber))?
                }
                AtomType::MovementTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::MovementName))?
                }
                AtomType::NarratorTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Narrator))?
                }
                AtomType::OriginalArtistTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::OriginalArtist))?
                }
                AtomType::OwnerTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Owner))?
                }
                AtomType::PodcastTag => {
                    add_flag_tag(it, &mut mb, map_std_bool!(StandardTag::PodcastFlag))?
                }
                AtomType::PurchaseDateTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::PurchaseDate))?
                }
                AtomType::ProducerTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Producer))?
                }
                AtomType::PublisherTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Label))?
                }
                AtomType::RatingTag => add_rating_tag(it, &mut mb)?,
                AtomType::RecordingCopyrightTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::ProductionCopyright))?
                }
                AtomType::SortAlbumArtistTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::SortAlbumArtist))?
                }
                AtomType::SortAlbumTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::SortAlbum))?
                }
                AtomType::SortArtistTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::SortArtist))?
                }
                AtomType::SortComposerTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::SortComposer))?
                }
                AtomType::SortNameTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::SortTrackTitle))?
                }
                AtomType::SortShowNameTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::SortTvSeriesTitle))?
                }
                AtomType::TempoTag => {
                    add_generic_tag(it, &mut mb, map_std_uint!(StandardTag::Bpm))?
                }
                AtomType::TrackNumberTag => add_pair_tag(
                    it,
                    &mut mb,
                    map_std_uint!(StandardTag::TrackNumber),
                    map_std_uint!(StandardTag::TrackTotal),
                )?,
                AtomType::TrackTitleTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::TrackTitle))?
                }
                AtomType::TvEpisodeNameTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::TvEpisodeTitle))?
                }
                AtomType::TvEpisodeNumberTag => {
                    add_generic_tag(it, &mut mb, map_std_uint!(StandardTag::TvEpisodeNumber))?
                }
                AtomType::TvNetworkNameTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::TvNetwork))?
                }
                AtomType::TvSeasonNumberTag => {
                    add_generic_tag(it, &mut mb, map_std_uint!(StandardTag::TvSeasonNumber))?
                }
                AtomType::TvShowNameTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::TvSeriesTitle))?
                }
                AtomType::UrlPodcastTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::UrlPodcast))?
                }
                AtomType::ShowMovementTag => {
                    add_flag_tag(it, &mut mb, |_| None)?;
                }
                AtomType::SoloistTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Soloist))?
                }
                AtomType::TrackArtistUrl => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::UrlArtist))?
                }
                AtomType::WorkTag => add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Work))?,
                AtomType::WriterTag => {
                    add_generic_tag(it, &mut mb, map_std_str!(StandardTag::Writer))?
                }
                // Free-form tag atom.
                AtomType::FreeFormTag => add_freeform_tag(it, &mut mb)?,
                // Completely unknown tag atom.
                AtomType::Other(atom_type) => {
                    debug!("unknown metadata sub-atom {atom_type:x?}");
                }
                // Known tag atom, but has no standard tag or special handling.
                _ => {
                    add_generic_tag(it, &mut mb, |_| None)?;
                }
            }
        }

        Ok(IlstAtom { metadata: mb.build() })
    }
}

/// Get a raw tag key for a given metadata atom type.
fn get_raw_tag_key(atom_type: AtomType) -> &'static str {
    match atom_type {
        // Freeform tag.
        AtomType::FreeFormTag => "----",
        // Well-defined tags.
        AtomType::AdvisoryTag => "rtng",
        AtomType::AlbumArtistTag => "aART",
        AtomType::AlbumTag => "\u{a9}alb",
        AtomType::ArrangerTag => "\u{a9}arg",
        AtomType::ArtistTag => "\u{a9}ART",
        AtomType::AuthorTag => "\u{a9}aut",
        AtomType::CommentTag => "\u{a9}cmt",
        AtomType::CompilationTag => "cpil",
        AtomType::ComposerTag => "\u{a9}com",
        AtomType::ConductorTag => "\u{a9}con",
        AtomType::CopyrightTag => "cprt",
        AtomType::CoverTag => "covr",
        AtomType::CustomGenreTag => "\u{a9}gen",
        AtomType::DateTag => "\u{a9}day",
        AtomType::DescriptionTag => "desc",
        AtomType::DiskNumberTag => "disk",
        AtomType::EncodedByTag => "\u{a9}enc",
        AtomType::EncoderTag => "\u{a9}too",
        AtomType::FileCreatorUrlTag => "\u{a9}mal",
        AtomType::GaplessPlaybackTag => "pgap",
        AtomType::GenreTag => "gnre",
        AtomType::GroupingTag => "\u{a9}grp",
        AtomType::HdVideoTag => "hdvd",
        AtomType::IdentPodcastTag => "egid",
        AtomType::IsrcTag => "\u{a9}isr",
        AtomType::ItunesAccountIdTag => "apID",
        AtomType::ItunesAccountTypeIdTag => "akID",
        AtomType::ItunesArtistIdTag => "atID",
        AtomType::ItunesComposerIdTag => "cmID",
        AtomType::ItunesContentIdTag => "cnID",
        AtomType::ItunesCountryIdTag => "sfID",
        AtomType::ItunesGenreIdTag => "geID",
        AtomType::ItunesPlaylistIdTag => "plID",
        AtomType::LabelTag => "\u{a9}lab",
        AtomType::LabelUrlTag => "\u{a9}lal",
        AtomType::LongDescriptionTag => "ldes",
        AtomType::LyricsTag => "\u{a9}lyr",
        AtomType::MediaTypeTag => "stik",
        AtomType::NarratorTag => "\u{a9}nrt",
        AtomType::OriginalArtistTag => "\u{a9}ope",
        AtomType::OwnerTag => "ownr",
        AtomType::PodcastCategoryTag => "catg",
        AtomType::PodcastKeywordsTag => "keyw",
        AtomType::PodcastTag => "pcst",
        AtomType::ProducerTag => "\u{a9}prd",
        AtomType::PublisherTag => "\u{a9}pub",
        AtomType::PurchaseDateTag => "purd",
        AtomType::RatingTag => "rate",
        AtomType::RecordingCopyrightTag => "\u{a9}phg",
        AtomType::SoloistTag => "\u{a9}sol",
        AtomType::SortAlbumArtistTag => "soaa",
        AtomType::SortAlbumTag => "soal",
        AtomType::SortArtistTag => "soar",
        AtomType::SortComposerTag => "soco",
        AtomType::SortNameTag => "sonm",
        AtomType::TempoTag => "tmpo",
        AtomType::TrackArtistUrl => "\u{a9}prl",
        AtomType::TrackNumberTag => "trkn",
        AtomType::TrackTitleTag => "\u{a9}nam",
        AtomType::TvEpisodeNameTag => "tven",
        AtomType::TvEpisodeNumberTag => "tves",
        AtomType::TvNetworkNameTag => "tvnn",
        AtomType::TvSeasonNumberTag => "tvsn",
        AtomType::TvShowNameTag => "tvsh",
        AtomType::UrlPodcastTag => "purl",
        AtomType::WorkTag => "\u{a9}wrk",
        AtomType::WriterTag => "\u{a9}wrt",
        AtomType::XidTag => "xid ",
        _ => "",
    }
}
