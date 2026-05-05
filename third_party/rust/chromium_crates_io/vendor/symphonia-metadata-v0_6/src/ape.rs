// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! An APEv1 and APEv2 metadata reader.

use core::str;
use std::collections::HashMap;
use std::io::{Seek, SeekFrom};
use std::sync::Arc;

use symphonia_core::errors::{Result, decode_error, unsupported_error};
use symphonia_core::formats::probe::{
    Anchors, ProbeMetadataData, ProbeableMetadata, Score, Scoreable,
};
use symphonia_core::io::{MediaSourceStream, ReadBytes, ScopedStream, SeekBuffered};
use symphonia_core::meta::well_known::{METADATA_ID_APEV1, METADATA_ID_APEV2};
use symphonia_core::meta::{
    MetadataBuffer, MetadataBuilder, MetadataInfo, MetadataOptions, MetadataReader, RawTag,
    RawValue, StandardTag, StandardVisualKey, Tag, Visual,
};
use symphonia_core::support_metadata;

use lazy_static::lazy_static;
use symphonia_core::util::text;

use crate::utils::images::{ImageInfo, try_get_image_info};
use crate::utils::std_tag::*;

lazy_static! {
    static ref APE_TAG_MAP: RawTagParserMap = {
        let mut m: RawTagParserMap = HashMap::new();
        m.insert("accurateripcount"            , parse_accuraterip_count);
        m.insert("accurateripcountalloffsets"  , parse_accuraterip_count_all_offsets);
        m.insert("accurateripcountwithoffset"  , parse_accuraterip_count_with_offset);
        m.insert("accurateripcrc"              , parse_accuraterip_crc);
        m.insert("accurateripdiscid"           , parse_accuraterip_disc_id);
        m.insert("accurateripid"               , parse_accuraterip_id);
        m.insert("accurateripoffset"           , parse_accuraterip_offset);
        m.insert("accurateripresult"           , parse_accuraterip_result);
        m.insert("accurateriptotal"            , parse_accuraterip_total);
        m.insert("acoustid_fingerprint"        , parse_acoustid_fingerprint);
        m.insert("acoustid_id"                 , parse_acoustid_id);
        m.insert("album artist"                , parse_album_artist);
        m.insert("album"                       , parse_album);
        m.insert("albumartistsort"             , parse_sort_album_artist);
        m.insert("albumsort"                   , parse_sort_album);
        m.insert("arranger"                    , parse_arranger);
        m.insert("artist"                      , parse_artist);
        m.insert("artistsort"                  , parse_sort_artist);
        m.insert("asin"                        , parse_ident_asin);
        m.insert("bpm"                         , parse_bpm);
        m.insert("catalog"                     , parse_ident_catalog_number);
        m.insert("catalognumber"               , parse_ident_catalog_number);
        m.insert("comment"                     , parse_comment);
        m.insert("compilation"                 , parse_compilation);
        m.insert("composer"                    , parse_composer);
        m.insert("composersort"                , parse_sort_composer);
        m.insert("conductor"                   , parse_conductor);
        m.insert("copyright"                   , parse_copyright);
        // Disc Number or Disc Number/Total Discs
        m.insert("disc"                        , parse_disc_number);
        m.insert("djmixer"                     , parse_mix_dj);
        // EAN-13/UPC-A
        m.insert("ean/upc"                     , parse_ident_ean_upn);
        m.insert("encodedby"                   , parse_encoded_by);
        m.insert("encoder settings"            , parse_encoder_settings);
        m.insert("encoder"                     , parse_encoder);
        m.insert("engineer"                    , parse_engineer);
        m.insert("file"                        , parse_original_file);
        m.insert("genre"                       , parse_genre);
        m.insert("isbn"                        , parse_ident_isbn);
        m.insert("isrc"                        , parse_ident_isrc);
        m.insert("label"                       , parse_label);
        m.insert("labelcode"                   , parse_label_code);
        m.insert("language"                    , parse_language);
        m.insert("lyricist"                    , parse_lyricist);
        m.insert("lyrics"                      , parse_lyrics);
        m.insert("media"                       , parse_media_format);
        m.insert("mixer"                       , parse_mix_engineer);
        m.insert("mood"                        , parse_mood);
        m.insert("movement"                    , parse_movement_total);
        m.insert("movementname"                , parse_movement_name);
        m.insert("movementtotal"               , parse_mood);
        m.insert("mp3gain_album_minmax"        , parse_mp3gain_album_min_max);
        m.insert("mp3gain_minmax"              , parse_mp3gain_min_max);
        m.insert("mp3gain_undo"                , parse_mp3gain_undo);
        m.insert("musicbrainz_albumartistid"   , parse_musicbrainz_album_artist_id);
        m.insert("musicbrainz_albumid"         , parse_musicbrainz_album_id);
        m.insert("musicbrainz_albumstatus"     , parse_musicbrainz_release_status);
        m.insert("musicbrainz_albumtype"       , parse_musicbrainz_release_type);
        m.insert("musicbrainz_artistid"        , parse_musicbrainz_artist_id);
        m.insert("musicbrainz_discid"          , parse_musicbrainz_disc_id);
        m.insert("musicbrainz_originalalbumid" , parse_musicbrainz_original_album_id);
        m.insert("musicbrainz_originalartistid", parse_musicbrainz_original_artist_id);
        m.insert("musicbrainz_releasegroupid"  , parse_musicbrainz_release_group_id);
        m.insert("musicbrainz_releasetrackid"  , parse_musicbrainz_release_track_id);
        m.insert("musicbrainz_trackid"         , parse_musicbrainz_track_id);
        m.insert("musicbrainz_trmid"           , parse_musicbrainz_trm_id);
        m.insert("musicbrainz_workid"          , parse_musicbrainz_work_id);
        m.insert("original artist"             , parse_original_artist);
        m.insert("originalyear"                , parse_original_release_year);
        m.insert("publisher"                   , parse_label);
        m.insert("record date"                 , parse_recording_date);
        m.insert("record location"             , parse_recording_location);
        m.insert("related"                     , parse_url);
        m.insert("replaygain_album_gain"       , parse_replaygain_album_gain);
        m.insert("replaygain_album_peak"       , parse_replaygain_album_peak);
        m.insert("replaygain_track_gain"       , parse_replaygain_track_gain);
        m.insert("replaygain_track_peak"       , parse_replaygain_track_peak);
        m.insert("subtitle"                    , parse_track_subtitle);
        m.insert("title"                       , parse_track_title);
        m.insert("titlesort"                   , parse_sort_track_title);
        // Track Number or Track Number/Total Tracks
        m.insert("track"                       , parse_track_number);
        m.insert("writer"                      , parse_writer);
        m.insert("year"                        , parse_release_date);
        // TODO: Debut Album
        // TODO: Publicationright
        // TODO: Abstract
        // TODO: Bibliography

        // No mappings for: Index, Introplay, Dummy
        m
    };
}

lazy_static! {
    static ref APE_VISUAL_TAG_MAP: HashMap<&'static str, StandardVisualKey> = {
        let mut m = HashMap::new();
        m.insert("cover art (other)", StandardVisualKey::Other);
        m.insert("cover art (png icon)", StandardVisualKey::FileIcon);
        m.insert("cover art (icon)", StandardVisualKey::OtherIcon);
        m.insert("cover art (front)", StandardVisualKey::FrontCover);
        m.insert("cover art (back)", StandardVisualKey::BackCover);
        m.insert("cover art (leaflet)", StandardVisualKey::Leaflet);
        m.insert("cover art (media)", StandardVisualKey::Media);
        m.insert("cover art (lead artist)", StandardVisualKey::LeadArtistPerformerSoloist);
        m.insert("cover art (artist)", StandardVisualKey::ArtistPerformer);
        m.insert("cover art (conductor)", StandardVisualKey::Conductor);
        m.insert("cover art (band)", StandardVisualKey::BandOrchestra);
        m.insert("cover art (composer)", StandardVisualKey::Composer);
        m.insert("cover art (lyricist)", StandardVisualKey::Lyricist);
        m.insert("cover art (recording location)", StandardVisualKey::RecordingLocation);
        m.insert("cover art (during recording)", StandardVisualKey::RecordingSession);
        m.insert("cover art (during performance)", StandardVisualKey::Performance);
        m.insert("cover art (video capture)", StandardVisualKey::ScreenCapture);
        m.insert("cover art (fish)", StandardVisualKey::Other);
        m.insert("cover art (illustration)", StandardVisualKey::Illustration);
        m.insert("cover art (band logotype)", StandardVisualKey::BandArtistLogo);
        m.insert("cover art (publisher logotype)", StandardVisualKey::PublisherStudioLogo);

        m
    };
}

const APEV1_METADATA_INFO: MetadataInfo =
    MetadataInfo { metadata: METADATA_ID_APEV1, short_name: "apev1", long_name: "APEv1" };
const APEV2_METADATA_INFO: MetadataInfo =
    MetadataInfo { metadata: METADATA_ID_APEV2, short_name: "apev2", long_name: "APEv2" };

/// The APE tag version.
#[derive(PartialEq, Eq)]
enum ApeVersion {
    /// Version 1, maps to 1000.
    V1,
    /// Version 2, maps to 2000.
    V2,
}

struct ApeHeader {
    version: ApeVersion,
    num_items: u32,
    size: u32,
    is_header: bool,
    has_header: bool,
    has_footer: bool,
}

impl ApeHeader {
    /// Read and verify the APE tag preamble and version.
    fn read_identity<B: ReadBytes>(reader: &mut B) -> Result<ApeVersion> {
        let mut preamble = [0; 8];
        reader.read_buf_exact(&mut preamble)?;

        if preamble != *b"APETAGEX" {
            return decode_error("ape: invalid preamble");
        }

        // Read the version. 1000 for APEv1, 2000 for APEv2, and so on...
        let version = match reader.read_u32()? {
            1000 => ApeVersion::V1,
            2000 => ApeVersion::V2,
            _ => return unsupported_error("ape: unsupported version"),
        };

        Ok(version)
    }

    /// Read an APE tag header.
    fn read<B: ReadBytes>(reader: &mut B) -> Result<ApeHeader> {
        let version = ApeHeader::read_identity(reader)?;

        // The size of the tag excluding any header.
        let size = reader.read_u32()?;
        let num_items = reader.read_u32()?;
        let flags = reader.read_u32()?;
        let _reserved = reader.read_u64()?;

        // Interpret the flags and size based on version.
        let (size, has_footer, has_header, is_header) = match version {
            ApeVersion::V1 => {
                // Flags should be ignored reading an APEv1 tag. However, an APEv1 tag always has a
                // footer.
                (size, true, false, false)
            }
            ApeVersion::V2 => {
                let has_header = flags & 0x8000_0000 != 0;
                let has_footer = flags & 0x4000_0000 != 0;
                let is_header = flags & 0x2000_0000 != 0;

                // The header size is not included in the size written to the tag.
                let real_size = size + if has_header { 32 } else { 0 };

                (real_size, has_footer, has_header, is_header)
            }
        };

        Ok(ApeHeader { version, num_items, size, is_header, has_header, has_footer })
    }
}

/// The value of an APE tag item.
enum ApeItemValue {
    String(String),
    Binary(Box<[u8]>),
    Locator(String),
}

/// An APE tag item.
struct ApeItem {
    key: String,
    value: ApeItemValue,
}

impl ApeItem {
    /// Try to read and return an APE tag item.
    fn read<B: ReadBytes>(reader: &mut B, header: &ApeHeader) -> Result<ApeItem> {
        // The length of the value in bytes.
        let len = reader.read_u32()? as usize;

        // Read flags.
        let flags = match header.version {
            ApeVersion::V1 => {
                // Ignore item flags for APEv1. The value type is always text.
                reader.read_u32()?;
                0
            }
            ApeVersion::V2 => reader.read_u32()?,
        };

        // Read the null-terminated key.
        let key = read_key(reader)?;

        // Read the value.
        let value = match (flags >> 1) & 0x3 {
            // UTF-8
            0 => ApeItemValue::String(read_utf8_value(reader, len)?),
            // Binary
            1 => ApeItemValue::Binary(reader.read_boxed_slice_exact(len)?),
            // Locator
            2 => ApeItemValue::Locator(read_utf8_value(reader, len)?),
            // Reserved
            3 => return decode_error("ape: reserved item value type"),
            _ => unreachable!(),
        };

        Ok(ApeItem { key, value })
    }
}

/// APEv1 and APEv2 tag reader.
pub struct ApeReader<'s> {
    reader: MediaSourceStream<'s>,
    version: ApeVersion,
}

impl<'s> ApeReader<'s> {
    pub fn try_new(mut mss: MediaSourceStream<'s>, _opts: MetadataOptions) -> Result<Self> {
        // Read and verify the APE tag preamble and version.
        let version = ApeHeader::read_identity(&mut mss)?;
        mss.seek_buffered_rel(-12);

        Ok(Self { reader: mss, version })
    }
}

impl Scoreable for ApeReader<'_> {
    fn score(_src: ScopedStream<&mut MediaSourceStream<'_>>) -> Result<Score> {
        Ok(Score::Supported(255))
    }
}

impl ProbeableMetadata<'_> for ApeReader<'_> {
    fn try_probe_new(
        mss: MediaSourceStream<'_>,
        opts: MetadataOptions,
    ) -> Result<Box<dyn MetadataReader + '_>>
    where
        Self: Sized,
    {
        Ok(Box::new(ApeReader::try_new(mss, opts)?))
    }

    fn probe_data() -> &'static [ProbeMetadataData] {
        &[
            // APEv1
            support_metadata!(
                APEV1_METADATA_INFO,
                &[],
                &[],
                &[b"APETAGEX\xe8\x03\x00\x00"],
                // APEv1 tags are only appended to the end of the stream.
                Anchors::Exclusive(&[
                    32,  // APE tag at end of stream.
                    160  // APE tag before ID3v1 tag.
                ])
            ),
            // APEv2
            support_metadata!(
                APEV2_METADATA_INFO,
                &[],
                &[],
                &[b"APETAGEX\xd0\x07\x00\x00"],
                // APEv2 tags can be appended to the end of the stream, or be at the start.
                Anchors::Supplemental(&[
                    32,  // APE tag at end of stream.
                    160  // APE tag before ID3v1 tag.
                ])
            ),
        ]
    }
}

impl MetadataReader for ApeReader<'_> {
    fn metadata_info(&self) -> &MetadataInfo {
        match self.version {
            ApeVersion::V1 => &APEV1_METADATA_INFO,
            ApeVersion::V2 => &APEV2_METADATA_INFO,
        }
    }

    fn read_all(&mut self) -> Result<MetadataBuffer> {
        let mut builder = MetadataBuilder::new(*self.metadata_info());

        // Read the tag header. This may actually be the header OR the footer.
        let header = ApeHeader::read(&mut self.reader)?;

        // If the header was actually a footer. Seek to the start of the APE tag.
        if !header.is_header {
            // The current position is the first byte after the APE footer. After the seek, the
            // reader will be at the header (if the tag contains one), or the first item.
            self.reader.seek(SeekFrom::Current(-(i64::from(header.size))))?;

            // If the APE tag contains a header, read it and do some verification checks. All header
            // and footer fields should match other than the `is_header` flag.
            if header.has_header {
                let real_header = ApeHeader::read(&mut self.reader)?;

                if header.has_footer != real_header.has_footer
                    || header.has_header != real_header.has_header
                    || header.is_header == real_header.is_header
                    || header.num_items != real_header.num_items
                    || header.size != real_header.size
                    || header.version != real_header.version
                {
                    return decode_error("ape: header and footer mismatch");
                }
            }
        }

        // Read APE tag items.
        for _ in 0..header.num_items {
            let item = ApeItem::read(&mut self.reader, &header)?;

            let key_lower = item.key.to_ascii_lowercase();

            // If the APE tag key can be mapped to a standard visual key, and the value is binary
            // data, then consider the tag to be a visual.
            if let Some(std_key) = APE_VISUAL_TAG_MAP.get(key_lower.as_str()).copied() {
                if let ApeItemValue::Binary(data) = item.value {
                    let mut tags = vec![];

                    // Try to parse the image data to obtain information about the image. This may
                    // alter the image buffer if extra information was attached to it.
                    let (data, image_info) = try_parse_image_data(data, &mut tags);

                    builder.add_visual(Visual {
                        media_type: image_info.as_ref().map(|info| info.media_type.clone()),
                        dimensions: image_info.as_ref().map(|info| info.dimensions),
                        color_mode: image_info.as_ref().map(|info| info.color_mode),
                        usage: Some(std_key),
                        tags,
                        data,
                    });

                    continue;
                }
            }

            // Map APE tag item values to raw values.
            let value = match item.value {
                ApeItemValue::String(value) | ApeItemValue::Locator(value) => {
                    // If the value contains a null-terminator, then the value is actually a list.
                    if value.contains('\0') {
                        let items = value.split_terminator('\0').map(|s| s.to_string()).collect();
                        RawValue::StringList(Arc::new(items))
                    }
                    else {
                        RawValue::from(value)
                    }
                }
                ApeItemValue::Binary(value) => RawValue::from(value),
            };

            builder.add_mapped_tags(RawTag::new(item.key, value), &APE_TAG_MAP);
        }

        // Read the footer.
        let footer = ApeHeader::read(&mut self.reader)?;

        // If the initial header was the actual header, then this checks the entire APE tag was
        // read, and the footer matches the header. If the initial header was actually the footer,
        // or there was no header, then this only checks the entire tag APE tag was read. However,
        // if there was a header, then the header and footer was checked to match earlier above.
        if header.has_footer != footer.has_footer
            || header.has_header != footer.has_header
            || header.num_items != footer.num_items
            || header.size != footer.size
            || header.version != footer.version
        {
            return decode_error("ape: header and footer mismatch");
        }

        Ok(MetadataBuffer { revision: builder.build(), side_data: Vec::new() })
    }

    fn into_inner<'s>(self: Box<Self>) -> MediaSourceStream<'s>
    where
        Self: 's,
    {
        self.reader
    }
}

fn read_key<B: ReadBytes>(reader: &mut B) -> Result<String> {
    // A key is recommended to be between 2-16 characters.
    let mut key = String::with_capacity(16);

    loop {
        let c = char::from(reader.read_u8()?);

        // Break at the null-terminator. Do not add it to the string buffer.
        if text::filter::null(&c) {
            break;
        }

        // TODO: The maximum allowed key length is 255 characters. Drop characters or error out?

        // A key may only contain ASCII characters from 0x20 ' ' up to 0x7e '~'.
        if text::filter::ascii_text(&c) {
            key.push(c);
        }
    }

    Ok(key)
}

fn read_utf8_value<B: ReadBytes>(reader: &mut B, len: usize) -> Result<String> {
    match String::from_utf8(reader.read_boxed_slice_exact(len)?.into_vec()) {
        Ok(value) => Ok(value),
        Err(_) => decode_error("ape: item value is valid not utf-8"),
    }
}

fn try_parse_image_data(buf: Box<[u8]>, tags: &mut Vec<Tag>) -> (Box<[u8]>, Option<ImageInfo>) {
    // It appears that the buffer stored by some binary tag items start with a null-terminated
    // filename of unspecified encoding (though UTF-8 seems likely). This is not documented
    // anywhere. Try to get this filename and strip it from the binary data.

    // Try to detect an image at the start of the data buffer.
    if let Some(info) = try_get_image_info(&buf) {
        // Image detected, return the original buffer back with the image information.
        return (buf, Some(info));
    }

    // Image information could not be detected. The data buffer may start with a null-terminated
    // filename. Try to find a null-terminator.
    if let Some(pos) = buf.iter().position(|&d| d == b'\0') {
        // Split at the null-terminator.
        let (left, right) = buf.split_at(pos);
        // Drop the null-terminator.
        let right = right.split_first().unwrap().1;

        // Try to detect an image after the null-terminator.
        if let Some(info) = try_get_image_info(right) {
            // Try to interpret the bytes preceeding the null-terminator as a UTF-8 encoded filename
            // and add it to the visual's tags if successful.
            if let Ok(name) = str::from_utf8(left) {
                if !name.is_empty() {
                    let name = Arc::new(name.to_string());

                    let tag = Tag::new_from_parts(
                        "FILE",
                        name.clone(),
                        Some(StandardTag::OriginalFile(name)),
                    );

                    tags.push(tag);
                }
            }

            // Image detected, return the cropped buffer with the image information.
            return (right.into(), Some(info));
        }
    }

    // An image could not be detected. The image format may be unsupported, or the buffer contains
    // something else. Return the original buffer.
    (buf, None)
}
