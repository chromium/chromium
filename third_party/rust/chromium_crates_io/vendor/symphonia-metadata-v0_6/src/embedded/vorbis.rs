// Symphonia
// Copyright (c) 2019-2024 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Vorbis Comment reading.

use std::collections::{BTreeMap, HashMap};
use std::sync::Arc;

use lazy_static::lazy_static;
use log::warn;

use symphonia_core::errors::{Error, Result, decode_error};
use symphonia_core::io::{BufReader, ReadBytes};
use symphonia_core::meta::well_known::METADATA_ID_VORBIS_COMMENT;
use symphonia_core::meta::{
    Chapter, ChapterGroup, ChapterGroupItem, MetadataBuilder, MetadataInfo, MetadataSideData,
    RawTag, StandardTag, Tag, Visual,
};
use symphonia_core::units::Time;
use symphonia_core::util::text;

use crate::embedded::flac;
use crate::utils::base64;
use crate::utils::images::try_get_image_info;
use crate::utils::std_tag::*;

pub const VORBIS_COMMENT_METADATA_INFO: MetadataInfo = MetadataInfo {
    metadata: METADATA_ID_VORBIS_COMMENT,
    short_name: "vorbis",
    long_name: "Vorbis Comment",
};

lazy_static! {
    static ref VORBIS_COMMENT_MAP: RawTagParserMap = {
        let mut m: RawTagParserMap = HashMap::new();

        m.insert("accurateripcount"             , parse_accuraterip_count);
        m.insert("accurateripcountalloffsets"   , parse_accuraterip_count_all_offsets);
        m.insert("accurateripcountwithoffset"   , parse_accuraterip_count_with_offset);
        m.insert("accurateripcrc"               , parse_accuraterip_crc);
        m.insert("accurateripdiscid"            , parse_accuraterip_disc_id);
        m.insert("accurateripid"                , parse_accuraterip_id);
        m.insert("accurateripoffset"            , parse_accuraterip_offset);
        m.insert("accurateripresult"            , parse_accuraterip_result);
        m.insert("accurateriptotal"             , parse_accuraterip_total);
        m.insert("acoustid_fingerprint"         , parse_acoustid_fingerprint);
        m.insert("acoustid_id"                  , parse_acoustid_id);
        m.insert("album artist"                 , parse_album_artist);
        m.insert("album"                        , parse_album);
        m.insert("albumartist"                  , parse_album_artist);
        m.insert("albumartistsort"              , parse_sort_album_artist);
        m.insert("albumsort"                    , parse_sort_album);
        m.insert("arranger"                     , parse_arranger);
        m.insert("artist"                       , parse_artist);
        m.insert("artistsort"                   , parse_sort_artist);
        // TODO: Is Author a synonym for Writer?
        m.insert("author"                       , parse_writer);
        m.insert("barcode"                      , parse_ident_barcode);
        m.insert("bpm"                          , parse_bpm);
        m.insert("catalog #"                    , parse_ident_catalog_number);
        m.insert("catalog"                      , parse_ident_catalog_number);
        m.insert("catalognumber"                , parse_ident_catalog_number);
        m.insert("catalogue #"                  , parse_ident_catalog_number);
        m.insert("cdtoc"                        , parse_cdtoc);
        m.insert("comment"                      , parse_comment);
        m.insert("compilation"                  , parse_compilation);
        m.insert("composer"                     , parse_composer);
        m.insert("conductor"                    , parse_conductor);
        m.insert("copyright"                    , parse_copyright);
        m.insert("ctdbdiscconfidence"           , parse_cuetoolsdb_disc_confidence);
        m.insert("ctdbtrackconfidence"          , parse_cuetoolsdb_track_confidence);
        m.insert("date"                         , parse_recording_date);
        m.insert("description"                  , parse_description);
        m.insert("disc"                         , parse_disc_number_exclusive);
        m.insert("discnumber"                   , parse_disc_number);
        m.insert("discsubtitle"                 , parse_disc_subtitle);
        m.insert("disctotal"                    , parse_disc_total);
        m.insert("disk"                         , parse_disc_number_exclusive);
        m.insert("disknumber"                   , parse_disc_number);
        m.insert("disksubtitle"                 , parse_disc_subtitle);
        m.insert("disktotal"                    , parse_disc_total);
        m.insert("djmixer"                      , parse_mix_dj);
        m.insert("ean/upn"                      , parse_ident_ean_upn);
        m.insert("encoded-by"                   , parse_encoded_by);
        m.insert("encodedby"                    , parse_encoded_by);
        m.insert("encoder settings"             , parse_encoder_settings);
        m.insert("encoder"                      , parse_encoder);
        m.insert("encoding"                     , parse_encoder_settings);
        m.insert("engineer"                     , parse_engineer);
        m.insert("ensemble"                     , parse_ensemble);
        m.insert("genre"                        , parse_genre);
        m.insert("grouping"                     , parse_grouping);
        m.insert("isrc"                         , parse_ident_isrc);
        m.insert("language"                     , parse_language);
        m.insert("label"                        , parse_label);
        m.insert("labelno"                      , parse_ident_catalog_number);
        m.insert("license"                      , parse_license);
        m.insert("lyricist"                     , parse_lyricist);
        m.insert("lyrics"                       , parse_lyrics);
        m.insert("media"                        , parse_media_format);
        m.insert("mixer"                        , parse_mix_engineer);
        m.insert("mood"                         , parse_mood);
        m.insert("musicbrainz_albumartistid"    , parse_musicbrainz_album_artist_id);
        m.insert("musicbrainz_albumid"          , parse_musicbrainz_album_id);
        m.insert("musicbrainz_artistid"         , parse_musicbrainz_artist_id);
        m.insert("musicbrainz_discid"           , parse_musicbrainz_disc_id);
        m.insert("musicbrainz_originalalbumid"  , parse_musicbrainz_original_album_id);
        m.insert("musicbrainz_originalartistid" , parse_musicbrainz_original_artist_id);
        m.insert("musicbrainz_recordingid"      , parse_musicbrainz_recording_id);
        m.insert("musicbrainz_releasegroupid"   , parse_musicbrainz_release_group_id);
        m.insert("musicbrainz_releasetrackid"   , parse_musicbrainz_release_track_id);
        m.insert("musicbrainz_trackid"          , parse_musicbrainz_track_id);
        m.insert("musicbrainz_workid"           , parse_musicbrainz_work_id);
        m.insert("opus"                         , parse_opus);
        m.insert("organization"                 , parse_label);
        m.insert("originaldate"                 , parse_original_release_date);
        m.insert("originalyear"                 , parse_original_release_year);
        m.insert("part"                         , parse_part);
        m.insert("partnumber"                   , parse_part_number_exclusive);
        m.insert("performer"                    , parse_performer);
        m.insert("producer"                     , parse_producer);
        m.insert("productnumber"                , parse_ident_pn);
        // TODO: Is Publisher a synonym for Label?
        m.insert("publisher"                    , parse_label);
        m.insert("rating"                       , parse_rating);
        m.insert("releasecountry"               , parse_release_country);
        m.insert("releasestatus"                , parse_musicbrainz_release_status);
        m.insert("releasetype"                  , parse_musicbrainz_release_type);
        m.insert("remixer"                      , parse_remixer);
        m.insert("replaygain_album_gain"        , parse_replaygain_album_gain);
        m.insert("replaygain_album_peak"        , parse_replaygain_album_peak);
        m.insert("replaygain_reference_loudness", parse_replaygain_reference_loudness);
        m.insert("replaygain_track_gain"        , parse_replaygain_track_gain);
        m.insert("replaygain_track_peak"        , parse_replaygain_track_peak);
        m.insert("script"                       , parse_script);
        m.insert("subtitle"                     , parse_track_subtitle);
        m.insert("title"                        , parse_track_title);
        m.insert("titlesort"                    , parse_sort_track_title);
        m.insert("totaldiscs"                   , parse_disc_total);
        m.insert("totaltracks"                  , parse_track_total);
        m.insert("track"                        , parse_track_number_exclusive);
        m.insert("tracknumber"                  , parse_track_number);
        m.insert("tracktotal"                   , parse_track_total);
        m.insert("unsyncedlyrics"               , parse_lyrics);
        m.insert("upc"                          , parse_ident_upc);
        m.insert("version"                      , parse_version);
        m.insert("work"                         , parse_work);
        m.insert("writer"                       , parse_writer);
        m.insert("year"                         , parse_recording_year);
        m
    };
}

/// Parse a string containing a base64 encoded FLAC picture block into a visual.
fn parse_base64_picture_block(b64: &str) -> Result<ParsedComment> {
    // Decode the Base64 encoded FLAC metadata block.
    let Some(data) = base64::decode(b64)
    else {
        return decode_error("meta(vorbis): the base64 encoding of a picture block is invalid");
    };

    flac::read_flac_picture_block(&mut BufReader::new(&data)).map(ParsedComment::Visual)
}

/// Parse a string containing a base64 encoding image file into a visual.
fn parse_base64_cover_art(b64: &str) -> Result<ParsedComment> {
    // Decode the Base64 encoded image data.
    let Some(data) = base64::decode(b64)
    else {
        return decode_error("meta (vorbis): the base64 encoding of cover art is invalid");
    };

    // Try to get image information.
    let Some(image_info) = try_get_image_info(&data)
    else {
        return decode_error("meta (vorbis): could not detect cover art image format");
    };

    Ok(ParsedComment::Visual(Visual {
        media_type: Some(image_info.media_type),
        dimensions: Some(image_info.dimensions),
        color_mode: Some(image_info.color_mode),
        usage: None,
        tags: vec![],
        data,
    }))
}

/// Parse a chapter timestamp in the HH:MM:SS.SSS format.
fn parse_chapter_timestamp(time: &str) -> Result<Time> {
    const FMT_ERR: Error = Error::DecodeError("malformed chapter timestamp");
    const OOB_ERR: Error = Error::DecodeError("chapter timestamp out-of-bounds");

    /// Parse an unsigned number containing atleast 1 digit from a string slice, and return the
    /// number as an `u32` with the number of digits.
    fn parse_unsigned_num(buf: &str, trim_leading: bool) -> Result<(u32, u8)> {
        // The string slice is empty. This is an error.
        if buf.is_empty() {
            return Err(FMT_ERR);
        }

        // Trim leading or trailing zeros from the string slice. This may result in an empty string
        // slice if all digits are 0.
        let buf = match trim_leading {
            true => buf.trim_start_matches('0'),
            false => buf.trim_end_matches('0'),
        };

        if buf.is_empty() {
            // The string slice after stripping the leading or trailing zeros is empty. Only one
            // zero digit was significant.
            Ok((0, 1))
        }
        else {
            buf.bytes()
                .try_fold(0u32, |num, digit| match digit {
                    b'0'..=b'9' => {
                        num.checked_mul(10).and_then(|num| num.checked_add(u32::from(digit - b'0')))
                    }
                    _ => None,
                })
                .map(|num| (num, buf.len() as u8))
                .ok_or(FMT_ERR)
        }
    }

    // Hours, minutes, and integer seconds are mandatory.
    let Some((h_str, rem)) = time.split_once(':')
    else {
        return Err(FMT_ERR);
    };
    let Some((m_str, rem)) = rem.split_once(':')
    else {
        return Err(FMT_ERR);
    };
    // Fractional seconds are optional.
    let (s_str, frac_s_str) = rem.split_once('.').unwrap_or((rem, "0"));

    let (h, _) = parse_unsigned_num(h_str, true)?;
    let (m, _) = parse_unsigned_num(m_str, true)?;
    let (s, _) = parse_unsigned_num(s_str, true)?;
    // This is the numerator portion of fractional seconds.
    let (frac_s_numer, frac_s_digits) = parse_unsigned_num(frac_s_str, false)?;
    // The number of digits in the numerator indicates the denominator.
    let ns = (1_000_000_000 * u64::from(frac_s_numer)) / 10u64.pow(u32::from(frac_s_digits));

    Time::from_hhmmss(
        h,
        m.try_into().map_err(|_| OOB_ERR)?,
        s.try_into().map_err(|_| OOB_ERR)?,
        ns as u32,
    )
    .ok_or(OOB_ERR)
}

/// The intent of the chapter information value.
enum ChapterInfoIntent {
    /// Chapter starting timestamp.
    Time,
    /// A chapter-specific tag.
    Tag(String),
}

/// The chapter information key.
struct ChapterInfoKey {
    /// The chapter number this key is associated with.
    num: u32,
    /// The intent of the chapter info associated with this key.
    intent: ChapterInfoIntent,
}

/// Try to parse the key as a chapter information key.
fn try_parse_chapter_info_key(key: &str) -> Option<ChapterInfoKey> {
    let mut iter = key.chars();

    // A chapter key must begin with case-insensitive "CHAPTER" prefix.
    for p in "CHAPTER".chars() {
        match iter.next() {
            Some(c) if c.eq_ignore_ascii_case(&p) => (),
            _ => return None,
        }
    }

    // Then it must be followed by 3 digits indicating the chapter number.
    let mut num = 0;

    for _ in 0..3 {
        match iter.next() {
            Some(c) if c.is_ascii_digit() => {
                num *= 10;
                num += u32::from(c) - u32::from('0');
            }
            _ => return None,
        }
    }

    // The remainder of the key may be an optional suffix containing a key for additional
    // information pertaining to the chapter such as the name. If there is no suffix, then this is a
    // chapter timestamp comment.
    let field = iter.as_str().to_string();

    let intent =
        if field.is_empty() { ChapterInfoIntent::Time } else { ChapterInfoIntent::Tag(field) };

    Some(ChapterInfoKey { num, intent })
}

/// Chapter information.
struct ChapterInfo {
    /// The chapter information key.
    key: ChapterInfoKey,
    /// The value.
    value: String,
}

/// A parsed Vorbis comment.
enum ParsedComment {
    /// The comment yielded a tag.
    Tag(RawTag),
    /// The comment yielded a visual.
    Visual(Visual),
    /// The comment yielded chapter information.
    ChapterInfo(ChapterInfo),
}

/// Parse the given Vorbis Comment string into a `Tag`.
fn parse_vorbis_comment(buf: &[u8]) -> Result<ParsedComment> {
    // Vorbis Comments are stored as <Key>=<Value> pairs where <Key> is a reduced ASCII-only
    // identifier and <Value> is a UTF-8 string value.
    //
    // Convert the entire comment into a UTF-8 string.
    let comment = String::from_utf8_lossy(buf);

    // Split the comment into key and value at the first '=' character.
    if let Some((key, value)) = comment.split_once('=') {
        // The key should only contain ASCII 0x20 through 0x7e (officially 0x7d, but this probably a
        // typo), with 0x3d ('=') excluded.
        let key = key.chars().filter(text::filter::ascii_text).collect::<String>();

        if let Some(key) = try_parse_chapter_info_key(&key) {
            // A comment with a key starting with "CHAPTERXXX" is a chapter information comment.
            Ok(ParsedComment::ChapterInfo(ChapterInfo { key, value: value.to_string() }))
        }
        else if key.eq_ignore_ascii_case("metadata_block_picture") {
            // A comment with a key "METADATA_BLOCK_PICTURE" is a FLAC picture block encoded in
            // base64. Attempt to decode it as such.
            parse_base64_picture_block(value)
        }
        else if key.eq_ignore_ascii_case("coverart") {
            // A comment with a key "COVERART" is a base64 encoded image. Attempt to decode it as
            // such.
            parse_base64_cover_art(value)
        }
        else {
            // Add a tag created from the key-value pair, while also attempting to map it to a
            // standard tag.
            Ok(ParsedComment::Tag(RawTag::new(key, value)))
        }
    }
    else {
        decode_error("meta (vorbis): malformed comment")
    }
}

pub fn read_vorbis_comment<B: ReadBytes>(
    reader: &mut B,
    builder: &mut MetadataBuilder,
    side_data: &mut Vec<MetadataSideData>,
) -> Result<()> {
    // Read the vendor string length in bytes.
    let vendor_len = reader.read_u32()?;

    // Ignore the vendor string.
    reader.ignore_bytes(u64::from(vendor_len))?;

    // Map of chapter number to a vector of chapter information.
    let mut chapters: BTreeMap<u32, Vec<ChapterInfo>> = Default::default();

    // Read the number of comments.
    let num_comments = reader.read_u32()? as usize;

    // Read each comment.
    for _ in 0..num_comments {
        // Read the comment string length in bytes.
        let comment_length = reader.read_u32()?;

        // TODO: Apply a limit.

        // Read the comment string.
        let mut comment_data = vec![0; comment_length as usize];
        reader.read_buf_exact(&mut comment_data)?;

        // Parse the Vorbis comment and handle the parsed output.
        match parse_vorbis_comment(&comment_data) {
            Ok(parsed) => match parsed {
                ParsedComment::Tag(raw) => {
                    // Comment was a tag.
                    builder.add_mapped_tags(raw, &VORBIS_COMMENT_MAP);
                }
                ParsedComment::Visual(visual) => {
                    // Comment was a picture.
                    builder.add_visual(visual);
                }
                ParsedComment::ChapterInfo(info) => {
                    // Comment was chapter information. Collect chapter information to build a
                    // chapter group later.
                    chapters.entry(info.key.num).or_default().push(info);
                }
            },
            Err(err) => warn!("{err}"),
        }
    }

    // If chapter information is present, try to build a chapter group.
    if !chapters.is_empty() {
        let items = chapters
            .into_iter()
            .filter_map(|(_, infos)| {
                let mut time = None;
                let mut tags = Vec::new();

                for info in infos {
                    match info.key.intent {
                        ChapterInfoIntent::Time => {
                            // Value is the chapter's start time.
                            time = parse_chapter_timestamp(&info.value).ok();
                        }
                        ChapterInfoIntent::Tag(key) => {
                            let value = Arc::new(info.value);

                            // "NAME" and "URL" are the only standardized keys for chapters.
                            let std_tag = if key.eq_ignore_ascii_case("name") {
                                Some(StandardTag::ChapterTitle(value.clone()))
                            }
                            else if key.eq_ignore_ascii_case("url") {
                                Some(StandardTag::Url(value.clone()))
                            }
                            else {
                                None
                            };

                            // Chapter-specific comment.
                            tags.push(Tag::new_from_parts(key, value, std_tag));
                        }
                    }
                }

                // A chapter can only be created if the chapter's start time is known.
                time.map(|time| {
                    ChapterGroupItem::Chapter(Chapter {
                        start_time: time,
                        end_time: None,
                        start_byte: None,
                        end_byte: None,
                        tags,
                        visuals: vec![],
                    })
                })
            })
            .collect::<Vec<ChapterGroupItem>>();

        side_data.push(MetadataSideData::Chapters(ChapterGroup {
            items,
            tags: vec![],
            visuals: vec![],
        }));
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use symphonia_core::units::Time;

    #[test]
    fn verify_parse_chapter_timestamp() {
        use super::parse_chapter_timestamp;

        // Empty buffer.
        assert!(parse_chapter_timestamp("").is_err());
        // Various invalid constructions.
        assert!(parse_chapter_timestamp("0").is_err());
        assert!(parse_chapter_timestamp("00").is_err());
        assert!(parse_chapter_timestamp("00:").is_err());
        assert!(parse_chapter_timestamp("00:0").is_err());
        assert!(parse_chapter_timestamp("00:00").is_err());
        assert!(parse_chapter_timestamp("00:00:").is_err());
        assert!(parse_chapter_timestamp("00:00:0.").is_err());
        // Invalid radix.
        assert!(parse_chapter_timestamp("0x0:00:00.000").is_err());
        assert!(parse_chapter_timestamp("00:0x0:00.000").is_err());
        assert!(parse_chapter_timestamp("00:00:0x0.000").is_err());
        assert!(parse_chapter_timestamp("00:00:00.0x000").is_err());
        // No negative or positive signs.
        assert!(parse_chapter_timestamp("-:00:00.000").is_err());
        assert!(parse_chapter_timestamp("+:00:00.000").is_err());
        assert!(parse_chapter_timestamp("-01:00:00.000").is_err());
        assert!(parse_chapter_timestamp("00:-01:00.000").is_err());
        assert!(parse_chapter_timestamp("00:00:-01.000").is_err());
        assert!(parse_chapter_timestamp("+01:00:00.000").is_err());

        // Various valid constructions.
        assert_eq!(
            parse_chapter_timestamp("00:00:0").unwrap(),
            Time::from_hhmmss(0, 0, 0, 0).unwrap()
        );
        assert_eq!(
            parse_chapter_timestamp("00:00:00").unwrap(),
            Time::from_hhmmss(0, 0, 0, 0).unwrap()
        );
        assert_eq!(
            parse_chapter_timestamp("0:0:0.0").unwrap(),
            Time::from_hhmmss(0, 0, 0, 0).unwrap()
        );
        assert_eq!(
            parse_chapter_timestamp("00:0:0.0").unwrap(),
            Time::from_hhmmss(0, 0, 0, 0).unwrap()
        );
        assert_eq!(
            parse_chapter_timestamp("00:00:0.0").unwrap(),
            Time::from_hhmmss(0, 0, 0, 0).unwrap()
        );
        assert_eq!(
            parse_chapter_timestamp("00:00:00.0").unwrap(),
            Time::from_hhmmss(0, 0, 0, 0).unwrap()
        );
        assert_eq!(
            parse_chapter_timestamp("00:00:00.00").unwrap(),
            Time::from_hhmmss(0, 0, 0, 0).unwrap()
        );
        assert_eq!(
            parse_chapter_timestamp("00:00:00.000").unwrap(),
            Time::from_hhmmss(0, 0, 0, 0).unwrap()
        );
        assert_eq!(
            parse_chapter_timestamp("000000000:00:00.00").unwrap(),
            Time::from_hhmmss(0, 0, 0, 0).unwrap()
        );
        assert_eq!(
            parse_chapter_timestamp("00:00:00.1000").unwrap(),
            Time::from_hhmmss(0, 0, 0, 100_000_000).unwrap()
        );

        // Maximum valid.
        assert_eq!(
            parse_chapter_timestamp("999999999:59:59.999999999").unwrap(),
            Time::from_hhmmss(999_999_999, 59, 59, 999_999_999).unwrap()
        );
        // Maximum valid with insignificant digits.
        assert_eq!(
            parse_chapter_timestamp("0999999999:059:059.9999999990").unwrap(),
            Time::from_hhmmss(999_999_999, 59, 59, 999_999_999).unwrap()
        );
        assert_eq!(
            parse_chapter_timestamp("000000999999999:000059:000059.999999999000000").unwrap(),
            Time::from_hhmmss(999_999_999, 59, 59, 999_999_999).unwrap()
        );

        // Hours invalid (> u32::MAX).
        assert!(parse_chapter_timestamp("4294967296:00:00.0").is_err());
        // Minutes invalid (> 59).
        assert!(parse_chapter_timestamp("00:60:00.000").is_err());
        assert!(parse_chapter_timestamp("00:256:00.000").is_err());
        // Seconds invalid (> 59).
        assert!(parse_chapter_timestamp("00:00:60.000").is_err());
        assert!(parse_chapter_timestamp("00:00:256.000").is_err());
    }
}
