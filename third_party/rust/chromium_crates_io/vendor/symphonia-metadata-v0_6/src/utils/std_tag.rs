// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Standard tag parsing and helpers.

// Depending on which features are enabled, some parsers may be unused. Disable this lint as it
// would be too difficult to individually waive the lint.
#![allow(dead_code)]

use std::{collections::HashMap, sync::Arc};

use symphonia_core::meta::{ContentAdvisory, MetadataBuilder, RawTag, RawValue, StandardTag, Tag};

// A pair of standard tags.
pub type StandardTagPair = [Option<StandardTag>; 2];

/// A parser of a raw tag value, yielding 0 to 2 standard tags.
pub type RawTagParser = fn(Arc<String>) -> StandardTagPair;
/// Maps raw tag key to a raw tag parser.
pub type RawTagParserMap = HashMap<&'static str, RawTagParser>;

/// Extension trait for `MetadataBuilder` to parsing and adding raw tags.
pub trait MetadataBuilderExt {
    /// Attempt to map a raw tag into oneor more standard tags after finding a parser in the
    /// provided map of raw tag parsers, and then add them to self. If the raw tag cannot be
    /// mapped, only adds the raw tag to self.
    fn add_mapped_tags(&mut self, raw: RawTag, parsers: &RawTagParserMap) -> &mut Self;

    /// Attempt to map a raw tag into one or more standard tags using the provided raw tag
    /// parser, and then add them to self. If the raw tag cannot be mapped, only adds the raw
    /// tag to self.
    fn add_mapped_tags_with_parser(&mut self, raw: RawTag, parser: RawTagParser) -> &mut Self;
}

impl MetadataBuilderExt for MetadataBuilder {
    fn add_mapped_tags_with_parser(&mut self, raw: RawTag, parser: RawTagParser) -> &mut Self {
        if let RawValue::String(value) = &raw.value {
            // Parse and add standard tags.
            match parser(value.clone()) {
                [Some(std), None] => {
                    // One raw tag yielded one standard tag.
                    return self.add_tag(Tag::new_std(raw, std));
                }
                [None, Some(std)] => {
                    // One raw tag yielded one standard tag.
                    return self.add_tag(Tag::new_std(raw, std));
                }
                [Some(std0), Some(std1)] => {
                    // One raw tag yielded two standards tags.
                    self.add_tag(Tag::new_std(raw.clone(), std0));
                    return self.add_tag(Tag::new_std(raw, std1));
                }
                // The raw value could not be parsed.
                _ => (),
            }
        };

        // Could not parse, add a raw tag.
        self.add_tag(Tag::new(raw))
    }

    fn add_mapped_tags(&mut self, raw: RawTag, parsers: &RawTagParserMap) -> &mut Self {
        // Find a parser based on a case insensitive key map.
        match parsers.get(raw.key.to_ascii_lowercase().as_str()) {
            Some(parser) => self.add_mapped_tags_with_parser(raw, *parser),
            None => self.add_tag(Tag::new(raw)),
        }
    }
}

/// Define a parser function that does nothing except map the input string value to a standard
/// tag.
macro_rules! noop_parser {
    ($name:ident,$tag:path) => {
        pub fn $name(v: Arc<String>) -> StandardTagPair {
            [Some($tag(v)), None]
        }
    };
}

noop_parser!(parse_accuraterip_count, StandardTag::AccurateRipCount);
noop_parser!(parse_accuraterip_count_all_offsets, StandardTag::AccurateRipCountAllOffsets);
noop_parser!(parse_accuraterip_count_with_offset, StandardTag::AccurateRipCountWithOffset);
noop_parser!(parse_accuraterip_crc, StandardTag::AccurateRipCrc);
noop_parser!(parse_accuraterip_disc_id, StandardTag::AccurateRipDiscId);
noop_parser!(parse_accuraterip_id, StandardTag::AccurateRipId);
noop_parser!(parse_accuraterip_offset, StandardTag::AccurateRipOffset);
noop_parser!(parse_accuraterip_result, StandardTag::AccurateRipResult);
noop_parser!(parse_accuraterip_total, StandardTag::AccurateRipTotal);
noop_parser!(parse_acoustid_fingerprint, StandardTag::AcoustIdFingerprint);
noop_parser!(parse_acoustid_id, StandardTag::AcoustIdId);
noop_parser!(parse_album, StandardTag::Album);
noop_parser!(parse_album_artist, StandardTag::AlbumArtist);
noop_parser!(parse_arranger, StandardTag::Arranger);
noop_parser!(parse_artist, StandardTag::Artist);

pub fn parse_bpm(v: Arc<String>) -> StandardTagPair {
    [v.parse::<u64>().ok().map(StandardTag::Bpm), None]
}

noop_parser!(parse_cdtoc, StandardTag::CdToc);
noop_parser!(parse_comment, StandardTag::Comment);

pub fn parse_compilation(v: Arc<String>) -> StandardTagPair {
    [parse_bool(v).map(StandardTag::CompilationFlag), None]
}

noop_parser!(parse_composer, StandardTag::Composer);
noop_parser!(parse_conductor, StandardTag::Conductor);
noop_parser!(parse_copyright, StandardTag::Copyright);
noop_parser!(parse_cuetoolsdb_disc_confidence, StandardTag::CueToolsDbDiscConfidence);
noop_parser!(parse_cuetoolsdb_track_confidence, StandardTag::CueToolsDbTrackConfidence);
noop_parser!(parse_description, StandardTag::Description);

pub fn parse_disc_number_exclusive(v: Arc<String>) -> StandardTagPair {
    let disc_number = v.parse::<u64>().ok().map(StandardTag::DiscNumber);
    [disc_number, None]
}

pub fn parse_disc_number(v: Arc<String>) -> StandardTagPair {
    let (num, total) = parse_m_of_n(v);
    [num.map(StandardTag::DiscNumber), total.map(StandardTag::DiscTotal)]
}

noop_parser!(parse_disc_subtitle, StandardTag::DiscSubtitle);

pub fn parse_disc_total(v: Arc<String>) -> StandardTagPair {
    [v.parse::<u64>().ok().map(StandardTag::DiscTotal), None]
}

noop_parser!(parse_encoded_by, StandardTag::EncodedBy);
noop_parser!(parse_encoder, StandardTag::Encoder);
noop_parser!(parse_encoder_settings, StandardTag::EncoderSettings);
noop_parser!(parse_encoding_date, StandardTag::EncodingDate);
noop_parser!(parse_engineer, StandardTag::Engineer);
noop_parser!(parse_ensemble, StandardTag::Ensemble);
noop_parser!(parse_genre, StandardTag::Genre);
noop_parser!(parse_grouping, StandardTag::Grouping);
noop_parser!(parse_ident_asin, StandardTag::IdentAsin);
noop_parser!(parse_ident_barcode, StandardTag::IdentBarcode);
noop_parser!(parse_ident_catalog_number, StandardTag::IdentCatalogNumber);
noop_parser!(parse_ident_ean_upn, StandardTag::IdentEanUpn);
noop_parser!(parse_ident_isbn, StandardTag::IdentIsbn);
noop_parser!(parse_ident_isrc, StandardTag::IdentIsrc);
noop_parser!(parse_ident_pn, StandardTag::IdentPn);
noop_parser!(parse_ident_podcast, StandardTag::IdentPodcast);
noop_parser!(parse_ident_upc, StandardTag::IdentUpc);
noop_parser!(parse_initial_key, StandardTag::InitialKey);
noop_parser!(parse_internet_radio_name, StandardTag::InternetRadioName);
noop_parser!(parse_internet_radio_owner, StandardTag::InternetRadioOwner);
noop_parser!(parse_label, StandardTag::Label);
noop_parser!(parse_label_code, StandardTag::LabelCode);
noop_parser!(parse_language, StandardTag::Language);
noop_parser!(parse_license, StandardTag::License);
noop_parser!(parse_lyricist, StandardTag::Lyricist);
noop_parser!(parse_lyrics, StandardTag::Lyrics);
noop_parser!(parse_media_format, StandardTag::MediaFormat);
noop_parser!(parse_mix_dj, StandardTag::MixDj);
noop_parser!(parse_mix_engineer, StandardTag::MixEngineer);
noop_parser!(parse_mood, StandardTag::Mood);
noop_parser!(parse_movement_name, StandardTag::MovementName);

pub fn parse_movement_number(v: Arc<String>) -> StandardTagPair {
    let (num, total) = parse_m_of_n(v);
    [num.map(StandardTag::MovementNumber), total.map(StandardTag::MovementTotal)]
}

pub fn parse_movement_total(v: Arc<String>) -> StandardTagPair {
    [v.parse::<u64>().ok().map(StandardTag::MovementTotal), None]
}

noop_parser!(parse_mp3gain_album_min_max, StandardTag::Mp3GainAlbumMinMax);
noop_parser!(parse_mp3gain_min_max, StandardTag::Mp3GainMinMax);
noop_parser!(parse_mp3gain_undo, StandardTag::Mp3GainUndo);
noop_parser!(parse_musicbrainz_album_artist_id, StandardTag::MusicBrainzAlbumArtistId);
noop_parser!(parse_musicbrainz_album_id, StandardTag::MusicBrainzAlbumId);
noop_parser!(parse_musicbrainz_artist_id, StandardTag::MusicBrainzArtistId);
noop_parser!(parse_musicbrainz_disc_id, StandardTag::MusicBrainzDiscId);
// noop_parser!(parse_musicbrainz_genre_id, StandardTag::MusicBrainzGenreId);
// noop_parser!(parse_musicbrainz_label_id, StandardTag::MusicBrainzLabelId);
noop_parser!(parse_musicbrainz_original_album_id, StandardTag::MusicBrainzOriginalAlbumId);
noop_parser!(parse_musicbrainz_original_artist_id, StandardTag::MusicBrainzOriginalArtistId);
noop_parser!(parse_musicbrainz_recording_id, StandardTag::MusicBrainzRecordingId);
noop_parser!(parse_musicbrainz_release_group_id, StandardTag::MusicBrainzReleaseGroupId);
noop_parser!(parse_musicbrainz_release_status, StandardTag::MusicBrainzReleaseStatus);
noop_parser!(parse_musicbrainz_release_track_id, StandardTag::MusicBrainzReleaseTrackId);
noop_parser!(parse_musicbrainz_release_type, StandardTag::MusicBrainzReleaseType);
noop_parser!(parse_musicbrainz_track_id, StandardTag::MusicBrainzTrackId);
noop_parser!(parse_musicbrainz_trm_id, StandardTag::MusicBrainzTrmId);
noop_parser!(parse_musicbrainz_work_id, StandardTag::MusicBrainzWorkId);
noop_parser!(parse_opus, StandardTag::Opus);
noop_parser!(parse_original_album, StandardTag::OriginalAlbum);
noop_parser!(parse_original_artist, StandardTag::OriginalArtist);
noop_parser!(parse_original_recording_date, StandardTag::OriginalRecordingDate);
noop_parser!(parse_original_recording_time, StandardTag::OriginalRecordingTime);

pub fn parse_original_recording_year(v: Arc<String>) -> StandardTagPair {
    let year = v.parse::<u16>().ok().map(StandardTag::OriginalRecordingYear);
    [year, None]
}

noop_parser!(parse_original_release_date, StandardTag::OriginalReleaseDate);
noop_parser!(parse_original_release_time, StandardTag::OriginalReleaseTime);

pub fn parse_original_release_year(v: Arc<String>) -> StandardTagPair {
    let year = v.parse::<u16>().ok().map(StandardTag::OriginalReleaseYear);
    [year, None]
}

noop_parser!(parse_original_file, StandardTag::OriginalFile);
noop_parser!(parse_original_lyricist, StandardTag::OriginalLyricist);
// noop_parser!(parse_original_writer, StandardTag::OriginalWriter);
noop_parser!(parse_owner, StandardTag::Owner);
noop_parser!(parse_part, StandardTag::Part);

pub fn parse_part_number_exclusive(v: Arc<String>) -> StandardTagPair {
    let part = v.parse::<u64>().ok().map(StandardTag::PartNumber);
    [part, None]
}

pub fn parse_part_total(v: Arc<String>) -> StandardTagPair {
    [v.parse::<u64>().ok().map(StandardTag::PartTotal), None]
}

noop_parser!(parse_performer, StandardTag::Performer);
noop_parser!(parse_podcast_category, StandardTag::PodcastCategory);
noop_parser!(parse_podcast_description, StandardTag::PodcastDescription);

pub fn parse_podcast_flag(v: Arc<String>) -> StandardTagPair {
    [parse_bool(v).map(StandardTag::PodcastFlag), None]
}

noop_parser!(parse_podcast_keywords, StandardTag::PodcastKeywords);
noop_parser!(parse_producer, StandardTag::Producer);
noop_parser!(parse_production_copyright, StandardTag::ProductionCopyright);
// noop_parser!(parse_purchase_date, StandardTag::PurchaseDate);

pub fn parse_rating(v: Arc<String>) -> StandardTagPair {
    // 0 - 100 rating scale to PPM.
    let rating = match v.parse::<u32>() {
        Ok(num) if num <= 100 => Some(StandardTag::Rating(num * 10000)),
        _ => None,
    };
    [rating, None]
}

noop_parser!(parse_recording_date, StandardTag::RecordingDate);
noop_parser!(parse_recording_location, StandardTag::RecordingLocation);
noop_parser!(parse_recording_time, StandardTag::RecordingTime);

pub fn parse_recording_year(v: Arc<String>) -> StandardTagPair {
    let year = v.parse::<u16>().ok().map(StandardTag::RecordingYear);
    [year, None]
}

noop_parser!(parse_release_country, StandardTag::ReleaseCountry);
noop_parser!(parse_release_date, StandardTag::ReleaseDate);
noop_parser!(parse_release_time, StandardTag::ReleaseTime);

pub fn parse_release_year(v: Arc<String>) -> StandardTagPair {
    let year = v.parse::<u16>().ok().map(StandardTag::ReleaseYear);
    [year, None]
}

noop_parser!(parse_remixer, StandardTag::Remixer);

// Parser ReplayGain string into floating point dB values?
noop_parser!(parse_replaygain_album_gain, StandardTag::ReplayGainAlbumGain);
noop_parser!(parse_replaygain_album_peak, StandardTag::ReplayGainAlbumPeak);
noop_parser!(parse_replaygain_album_range, StandardTag::ReplayGainAlbumRange);
noop_parser!(parse_replaygain_reference_loudness, StandardTag::ReplayGainReferenceLoudness);
noop_parser!(parse_replaygain_track_gain, StandardTag::ReplayGainTrackGain);
noop_parser!(parse_replaygain_track_peak, StandardTag::ReplayGainTrackPeak);
noop_parser!(parse_replaygain_track_range, StandardTag::ReplayGainTrackRange);
noop_parser!(parse_script, StandardTag::Script);
noop_parser!(parse_sort_album, StandardTag::SortAlbum);
noop_parser!(parse_sort_album_artist, StandardTag::SortAlbumArtist);
noop_parser!(parse_sort_artist, StandardTag::SortArtist);
noop_parser!(parse_sort_composer, StandardTag::SortComposer);
noop_parser!(parse_sort_track_title, StandardTag::SortTrackTitle);
noop_parser!(parse_tagging_date, StandardTag::TaggingDate);
noop_parser!(parse_terms_of_use, StandardTag::TermsOfUse);

pub fn parse_track_number_exclusive(v: Arc<String>) -> StandardTagPair {
    let track_number = v.parse::<u64>().ok().map(StandardTag::TrackNumber);
    [track_number, None]
}

pub fn parse_track_number(v: Arc<String>) -> StandardTagPair {
    let (num, total) = parse_m_of_n(v);
    [num.map(StandardTag::TrackNumber), total.map(StandardTag::TrackTotal)]
}

noop_parser!(parse_track_subtitle, StandardTag::TrackSubtitle);
noop_parser!(parse_track_title, StandardTag::TrackTitle);

pub fn parse_track_total(v: Arc<String>) -> StandardTagPair {
    [v.parse::<u64>().ok().map(StandardTag::TrackTotal), None]
}

// pub fn parse_tv_episode(v: Arc<String>) -> StandardTagPair {
//     let tv_episode = v.parse::<u64>().ok().map(StandardTag::TvEpisode);
//     [tv_episode, None]
// }

// noop_parser!(parse_tv_episode_title, StandardTag::TvEpisodeTitle);
// noop_parser!(parse_tv_network, StandardTag::TvNetwork);

// pub fn parse_tv_season(v: Arc<String>) -> StandardTagPair {
//     let tv_season = v.parse::<u64>().ok().map(StandardTag::TvSeason);
//     [tv_season, None]
// }

// noop_parser!(parse_tv_show_title, StandardTag::TvShowTitle);
noop_parser!(parse_url, StandardTag::Url);
noop_parser!(parse_url_artist, StandardTag::UrlArtist);
noop_parser!(parse_url_copyright, StandardTag::UrlCopyright);
noop_parser!(parse_url_internet_radio, StandardTag::UrlInternetRadio);
noop_parser!(parse_url_label, StandardTag::UrlLabel);
noop_parser!(parse_url_official, StandardTag::UrlOfficial);
noop_parser!(parse_url_payment, StandardTag::UrlPayment);
noop_parser!(parse_url_podcast, StandardTag::UrlPodcast);
noop_parser!(parse_url_purchase, StandardTag::UrlPurchase);
noop_parser!(parse_url_source, StandardTag::UrlSource);
noop_parser!(parse_version, StandardTag::Version);
noop_parser!(parse_work, StandardTag::Work);
noop_parser!(parse_writer, StandardTag::Writer);

pub fn parse_itunes_content_advisory(v: Arc<String>) -> StandardTagPair {
    let content_advisory = v
        .parse::<u8>()
        .ok()
        .and_then(|value| match value {
            0 => Some(ContentAdvisory::None),
            1 | 4 => Some(ContentAdvisory::Explicit),
            2 => Some(ContentAdvisory::Censored),
            _ => None,
        })
        .map(StandardTag::ContentAdvisory);

    [content_advisory, None]
}

pub fn parse_id3v2_genre(v: Arc<String>) -> StandardTagPair {
    use regex_lite::Regex;

    use crate::utils::id3v1::get_genre_name;

    // Regex that will match the following strings:
    //
    // "<NUMBER>"
    // "<NAME>"
    // "(<NUMBER>)"
    // "(<NUMBER)<NAME>"
    let re = Regex::new(r"^(?P<num0>[0-9]+)$|(?:\((?P<num1>[0-9]+)\))?(?P<name>.+)?$").unwrap();

    // The regex will always match an empty string, therefore unwrapping is safe.
    let caps = re.captures(v.as_str()).unwrap();

    let name = if let Some(name) = caps.name("name") {
        // A user-defined genre name provided.
        Some(name.as_str().to_owned())
    }
    else if let Some(num) = caps.name("num0").or_else(|| caps.name("num1")) {
        // Only genre number provided. Parse to u8, then lookup the genre name.
        num.as_str().parse::<u8>().ok().and_then(get_genre_name)
    }
    else {
        // Empty string.
        None
    };

    // Fallback to the original value for the genre if one could not be parsed.
    let genre = name
        .map(|name| StandardTag::Genre(Arc::new(name)))
        .unwrap_or_else(|| StandardTag::Genre(v));

    [Some(genre), None]
}

fn parse_bool(v: Arc<String>) -> Option<bool> {
    match v.to_ascii_lowercase().as_str() {
        "1" | "true" | "yes" | "y " => Some(true),
        "0" | "false" | "no" | "n" => Some(false),
        _ => None,
    }
}

/// Parse a string in the format "NUM/TOTAL" or "NUM" into a pair of optional integers.
fn parse_m_of_n(v: Arc<String>) -> (Option<u64>, Option<u64>) {
    use regex_lite::Regex;

    let re = Regex::new(r"^(?P<m>[0-9]+)(/(?P<n>[0-9]+))?$").unwrap();

    let mut opt_m = None;
    let mut opt_n = None;

    if let Some(caps) = re.captures(v.as_str()) {
        opt_m = caps.name("m").and_then(|m| m.as_str().parse::<u64>().ok());
        opt_n = caps.name("n").and_then(|n| n.as_str().parse::<u64>().ok());
    }

    (opt_m, opt_n)
}

#[cfg(test)]
mod tests {
    use std::sync::Arc;

    use symphonia_core::meta::StandardTag;

    use super::*;

    fn arc_string(value: &str) -> Arc<String> {
        Arc::new(String::from(value))
    }

    #[test]
    fn verify_parse_m_of_n() {
        assert_eq!(parse_m_of_n(arc_string("1")), (Some(1), None));
        assert_eq!(parse_m_of_n(arc_string("")), (None, None));
        assert_eq!(parse_m_of_n(arc_string("a")), (None, None));
        assert_eq!(parse_m_of_n(arc_string("ab")), (None, None));
        assert_eq!(parse_m_of_n(arc_string("1a")), (None, None));
        assert_eq!(parse_m_of_n(arc_string("1/2")), (Some(1), Some(2)));
        assert_eq!(parse_m_of_n(arc_string("1a/2")), (None, None));
        assert_eq!(parse_m_of_n(arc_string("1/2a")), (None, None));
        assert_eq!(parse_m_of_n(arc_string("/2")), (None, None));
        assert_eq!(parse_m_of_n(arc_string("1/2/")), (None, None));
        assert_eq!(parse_m_of_n(arc_string("1/2/2")), (None, None));
        assert_eq!(parse_m_of_n(arc_string("1/99999999999999999999999999999999")), (Some(1), None));
    }

    #[test]
    fn verify_parse_id3v2_genre() {
        assert_eq!(parse_id3v2_genre(arc_string(""))[0], Some(StandardTag::Genre(arc_string(""))));
        assert_eq!(
            parse_id3v2_genre(arc_string("-"))[0],
            Some(StandardTag::Genre(arc_string("-")))
        );
        assert_eq!(
            parse_id3v2_genre(arc_string("145"))[0],
            Some(StandardTag::Genre(arc_string("Anime")))
        );
        assert_eq!(
            parse_id3v2_genre(arc_string("(145)"))[0],
            Some(StandardTag::Genre(arc_string("Anime")))
        );
        assert_eq!(
            parse_id3v2_genre(arc_string("(145)Ani"))[0],
            Some(StandardTag::Genre(arc_string("Ani")))
        );
        assert_eq!(
            parse_id3v2_genre(arc_string("Anime"))[0],
            Some(StandardTag::Genre(arc_string("Anime")))
        );
        assert_eq!(
            parse_id3v2_genre(arc_string("145333"))[0],
            Some(StandardTag::Genre(arc_string("145333")))
        );
        assert_eq!(
            parse_id3v2_genre(arc_string("255"))[0],
            Some(StandardTag::Genre(arc_string("255")))
        );
        assert_eq!(
            parse_id3v2_genre(arc_string("()Hello"))[0],
            Some(StandardTag::Genre(arc_string("()Hello")))
        );
        assert_eq!(
            parse_id3v2_genre(arc_string("(abc)Hello"))[0],
            Some(StandardTag::Genre(arc_string("(abc)Hello")))
        );
    }
}
