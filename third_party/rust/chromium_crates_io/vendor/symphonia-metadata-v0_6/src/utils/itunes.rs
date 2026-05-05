// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Utilties for handling iTunes-style metadata.

use symphonia_core::errors::Result;
use symphonia_core::meta::{MetadataBuilder, RawTag, RawValue};

use std::collections::HashMap;

use lazy_static::lazy_static;

use crate::utils::std_tag::*;

lazy_static! {
    static ref ITUNES_TAG_MAP: RawTagParserMap = {
        let mut m: RawTagParserMap = HashMap::new();
        m.insert("com.apple.itunes:acoustid fingerprint", parse_acoustid_fingerprint);
        m.insert("com.apple.itunes:acoustid id", parse_acoustid_id);
        m.insert("com.apple.itunes:artists", parse_artist);
        m.insert("com.apple.itunes:asin", parse_ident_asin);
        m.insert("com.apple.itunes:barcode", parse_ident_barcode);
        m.insert("com.apple.itunes:catalognumber", parse_ident_catalog_number);
        m.insert("com.apple.itunes:conductor", parse_conductor);
        m.insert("com.apple.itunes:discsubtitle", parse_disc_subtitle);
        m.insert("com.apple.itunes:djmixer", parse_mix_dj);
        m.insert("com.apple.itunes:engineer", parse_engineer);
        m.insert("com.apple.itunes:initialkey", parse_initial_key);
        m.insert("com.apple.itunes:isrc", parse_ident_isrc);
        m.insert("com.apple.itunes:itunes_cddb_1", parse_cdtoc); // TODO: Slightly different format.
        m.insert("com.apple.itunes:label", parse_label);
        m.insert("com.apple.itunes:language", parse_language);
        m.insert("com.apple.itunes:license", parse_license);
        m.insert("com.apple.itunes:lyricist", parse_lyricist);
        m.insert("com.apple.itunes:media", parse_media_format);
        m.insert("com.apple.itunes:mixer", parse_mix_engineer);
        m.insert("com.apple.itunes:mood", parse_mood);
        m.insert("com.apple.itunes:musicbrainz album artist id", parse_musicbrainz_album_artist_id);
        m.insert("com.apple.itunes:musicbrainz album id", parse_musicbrainz_album_id);
        m.insert("com.apple.itunes:musicbrainz album release country", parse_release_country);
        m.insert("com.apple.itunes:musicbrainz album status", parse_musicbrainz_release_status);
        m.insert("com.apple.itunes:musicbrainz album type", parse_musicbrainz_release_type);
        m.insert("com.apple.itunes:musicbrainz artist id", parse_musicbrainz_artist_id);
        m.insert("com.apple.itunes:musicbrainz disc id", parse_musicbrainz_disc_id);
        m.insert("com.apple.itunes:musicbrainz original album id", parse_musicbrainz_album_id);
        m.insert(
            "com.apple.itunes:musicbrainz original artist id",
            parse_musicbrainz_original_artist_id,
        );
        m.insert(
            "com.apple.itunes:musicbrainz release group id",
            parse_musicbrainz_release_group_id,
        );
        m.insert(
            "com.apple.itunes:musicbrainz release track id",
            parse_musicbrainz_release_track_id,
        );
        m.insert("com.apple.itunes:musicbrainz track id", parse_musicbrainz_track_id);
        m.insert("com.apple.itunes:musicbrainz trm id", parse_musicbrainz_trm_id);
        m.insert("com.apple.itunes:musicbrainz work id", parse_musicbrainz_work_id);
        m.insert("com.apple.itunes:originaldate", parse_original_release_date);
        m.insert("com.apple.itunes:producer", parse_producer);
        m.insert("com.apple.itunes:releasedate", parse_release_date);
        m.insert("com.apple.itunes:remixer", parse_remixer);
        m.insert("com.apple.itunes:replaygain_album_gain", parse_replaygain_album_gain);
        m.insert("com.apple.itunes:replaygain_album_peak", parse_replaygain_album_peak);
        m.insert("com.apple.itunes:replaygain_album_range", parse_replaygain_album_range);
        m.insert(
            "com.apple.itunes:replaygain_reference_loudness",
            parse_replaygain_reference_loudness,
        );
        m.insert("com.apple.itunes:replaygain_track_gain", parse_replaygain_track_gain);
        m.insert("com.apple.itunes:replaygain_track_peak", parse_replaygain_track_peak);
        m.insert("com.apple.itunes:replaygain_track_range", parse_replaygain_track_range);
        m.insert("com.apple.itunes:script", parse_script);
        m.insert("com.apple.itunes:subtitle", parse_track_subtitle);
        m
    };
}

/// Try to parse iTunes metadata from a raw tag and add it to the builder.
pub fn parse_itunes_tag(key: String, value: RawValue, builder: &mut MetadataBuilder) -> Result<()> {
    builder.add_mapped_tags(RawTag::new(key, value), &ITUNES_TAG_MAP);
    Ok(())
}
