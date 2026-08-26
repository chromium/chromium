// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::{ops::Deref, rc::Rc, sync::Arc};

use symphonia_core::meta::{RawTag, RawTagSubField, RawValue, StandardTag};

use crate::{segment::SimpleTagElement, sub_fields::*};

#[derive(Clone, Debug)]
/// Target type information.
pub struct Target {
    /// The target type value.
    pub value: u64,
    /// The target type name, if explicitly specified.
    pub name: Option<Rc<Box<str>>>,
}

/// Describes the context in-which a tag exists.
#[derive(Clone, Debug)]
pub struct TagContext {
    /// Indicates if the media a video.
    pub is_video: bool,
    /// The current target for the tag, if a target is specified.
    pub target: Option<Target>,
}

pub fn make_raw_tags(tag: SimpleTagElement, ctx: &TagContext, out: &mut Vec<RawTag>) {
    // The nested sub-tags of the following tags are flattened into multiple raw tags:
    //
    //   ORIGINAL  -> <TARGET>@ORIGINAL/<SUB-TAG>
    //   SAMPLE    -> <TARGET>@SAMPLE/<SUB-TAG>
    //   COUNTRY   -> <TARGET>@<SUB-TAG> (with COUNTRY sub-field)
    let mut path = get_target_path(ctx);

    if tag.name.eq_ignore_ascii_case("ORIGINAL") || tag.name.eq_ignore_ascii_case("SAMPLE") {
        // ORIGINAL and SAMPLE tags are parent tags. Flatten the hierarchy.
        path.push_str(&tag.name);
        // Generate tags.
        for tag in tag.sub_tags {
            let path = format!("{}/{}", path, tag.name);
            make_raw_tag(path, tag, out);
        }
    }
    else if tag.name.eq_ignore_ascii_case("COUNTRY") {
        // COUNTRY tag is a parent tag, but a COUNTRY sub-field will be used instead.
        for tag in tag.sub_tags {
            let path = format!("{}/{}", path, tag.name);
            make_raw_tag(path, tag, out);
        }
    }
    else {
        // Non-parent tag.
        path.push_str(&tag.name);
        make_raw_tag(path, tag, out);
    }
}

fn get_target_path(ctx: &TagContext) -> String {
    if let Some(target) = &ctx.target {
        // The tag is associated with a target, generate a raw tag key with the target incorporated
        // into it.
        if let Some(target_name) = &target.name {
            // A target type name is explictly provided.
            format!("{target_name}@")
        }
        else if let Some(target_name) = default_target_name(target.value, ctx.is_video) {
            // A target type name is not provided, but a default target name for this target value
            // is known.
            format!("{target_name}@")
        }
        else {
            // There is no known target type name for the target value provided.
            format!("#{}@", target.value)
        }
    }
    else {
        // No target.
        "".into()
    }
}

pub fn make_raw_tag(path: String, tag: SimpleTagElement, out: &mut Vec<RawTag>) {
    let mut sub_fields = Vec::with_capacity(1);

    // Tag language sub-field.
    if let Some(lang) = tag.lang_bcp47 {
        // BCP47 language code is present, prefer it over the ISO 639-2 chapter
        // language.
        sub_fields.push(RawTagSubField::new(TAG_LANGUAGE_BCP47, lang));
    }
    else if let Some(lang) = tag.lang {
        // ISO 639-2 language code.
        sub_fields.push(RawTagSubField::new(TAG_LANGUAGE, lang));
    }

    // The URL, EMAIL, ADDRESS, FAX, PHONE, INSTRUMENTS, and CHARACTER nested sub-tags are mapped as
    // sub-fields or the main tag. However, the SORT_WITH nested sub-tag must be mapped as an
    // additional tag.
    for sub_tag in tag.sub_tags {
        match sub_tag.name.as_ref() {
            "URL" | "EMAIL" | "ADDRESS" | "FAX" | "PHONE" | "INSTRUMENTS" | "CHARACTER" => {
                if let Some(RawValue::String(value)) = sub_tag.value {
                    sub_fields.push(RawTagSubField::new(sub_tag.name, value));
                }
            }
            "SORT_WITH" => {
                if let Some(RawValue::String(value)) = sub_tag.value {
                    out.push(RawTag::new(format!("{path}/SORT_WITH"), value));
                }
            }
            _ => (),
        }
    }

    out.push(RawTag::new_with_sub_fields(
        path,
        tag.value.unwrap_or(RawValue::Flag),
        sub_fields.into_boxed_slice(),
    ))
}

/// Attempt to map a raw tag to a standard tag.
pub fn map_std_tag(raw: &RawTag, lower_ctx: &TagContext) -> Option<StandardTag> {
    if let RawValue::String(value) = &raw.value {
        // String tags.
        let raw_key = raw.key.as_str();

        let (target_name, tag) = raw_key.split_once('@').unwrap_or(("", raw_key));

        let value = value.clone();

        // Attempt the match against the full raw tag key (target & tag name).
        let std = match raw_key {
            // Target level 70.
            //
            // Official target type names for both music & videos: COLLECTION

            // Target level 60.
            //
            // Official target type names for music: EDITION, ISSUE, VOLUME, OPUS
            // Official target type names for video: SEASON, SEQUEL, VOLUME

            // Target level 50.
            //
            // Official target type names for music: ALBUM, OPERA, CONCERT
            // Official target type names for videos: MOVIE, EPISODE, CONCERT
            "ALBUM@ARTIST" => StandardTag::AlbumArtist(value),
            "ALBUM@ARTIST/SORT_WITH" => StandardTag::SortAlbumArtist(value),
            "ALBUM@REPLAYGAIN_GAIN" => StandardTag::ReplayGainAlbumGain(value),
            "ALBUM@REPLAYGAIN_PEAK" => StandardTag::ReplayGainAlbumPeak(value),

            // Target level 40.
            //
            // Official target type names for both music & video: PART, SESSION

            // Target level 30.
            //
            // Official target type names for music: TRACK, SONG
            // Official target type names for video: CHAPTER
            "TRACK@REPLAYGAIN_GAIN" | "SONG@REPLAYGAIN_GAIN" => {
                StandardTag::ReplayGainTrackGain(value)
            }
            "TRACK@REPLAYGAIN_PEAK" | "SONG@REPLAYGAIN_PEAK" => {
                StandardTag::ReplayGainTrackPeak(value)
            }

            // Target level 20.
            //
            // Official target type names for music: SUBTRACK, PART, MOVEMENT
            // Official target type names for video: SCENE

            // Target level 10.
            //
            // Official target type names for video: SHOT

            // Attempt to match only against the tag name.
            _ => match tag {
                // Entities
                "ARTIST" => StandardTag::Artist(value),
                "LEAD_PERFORMER" => StandardTag::Performer(value),
                "ACCOMPANIMENT" => StandardTag::Ensemble(value),
                "COMPOSER" => StandardTag::Composer(value),
                "ARRANGER" => StandardTag::Arranger(value),
                "LYRICS" => StandardTag::Lyrics(value),
                "LYRICIST" => StandardTag::Lyricist(value),
                "CONDUCTOR" => StandardTag::Conductor(value),
                "DIRECTOR" => StandardTag::Director(value),
                "ASSISTANT_DIRECTOR" => StandardTag::AssistantDirector(value),
                "DIRECTOR_OF_PHOTOGRAPHY" => StandardTag::Cinematographer(value),
                "SOUND_ENGINEER" => StandardTag::Engineer(value),
                "ART_DIRECTOR" => StandardTag::ArtDirector(value),
                "PRODUCTION_DESIGNER" => StandardTag::ProductionDesigner(value),
                "CHOREGRAPHER" => StandardTag::Choregrapher(value),
                "COSTUME_DESIGNER" => StandardTag::CostumeDesigner(value),
                "ACTOR" => StandardTag::Actor(value),
                "WRITTEN_BY" => StandardTag::Writer(value),
                "SCREENPLAY_BY" => StandardTag::ScreenplayAuthor(value),
                "EDITED_BY" => StandardTag::EditedBy(value),
                "PRODUCER" => StandardTag::Producer(value),
                "COPRODUCER" => StandardTag::Coproducer(value),
                "EXECUTIVE_PRODUCER" => StandardTag::ExecutiveProducer(value),
                "DISTRIBUTED_BY" => StandardTag::Distributor(value),
                "MASTERED_BY" => StandardTag::Engineer(value),
                "ENCODED_BY" => StandardTag::EncodedBy(value),
                "MIXED_BY" => StandardTag::MixDj(value),
                "REMIXED_BY" => StandardTag::Remixer(value),
                "PRODUCTION_STUDIO" => StandardTag::ProductionStudio(value),
                "THANKS_TO" => StandardTag::Thanks(value),
                "PUBLISHER" => StandardTag::Label(value),
                "LABEL" => StandardTag::Label(value),

                // Search and classification
                "GENRE" => StandardTag::Genre(value),
                "MOOD" => StandardTag::Mood(value),
                "ORIGINAL_MEDIA_TYPE" => StandardTag::MediaFormat(value),
                "CONTENT_TYPE" => StandardTag::ContentType(value),
                "SUBJECT" => StandardTag::Subject(value),
                "DESCRIPTION" => StandardTag::Description(value),
                "KEYWORDS" => StandardTag::Keywords(value),
                "SUMMARY" => StandardTag::Summary(value),
                "SYNOPSIS" => StandardTag::Synopsis(value),
                "INITIAL_KEY" => StandardTag::InitialKey(value),
                "PERIOD" => StandardTag::Period(value),
                "LAW_RATING" => StandardTag::ContentRating(value),

                // Dates
                "DATE_RELEASE" => StandardTag::ReleaseDate(value), // Unofficial.
                "DATE_RELEASED" => StandardTag::ReleaseDate(value),
                "DATE_RECORDED" => StandardTag::RecordingDate(value),
                "DATE_ENCODED" => StandardTag::EncodingDate(value),
                "DATE_TAGGED" => StandardTag::TaggingDate(value),
                "DATE_DIGITIZED" => StandardTag::DigitizedDate(value),
                "DATE_WRITTEN" => StandardTag::WrittenDate(value),
                "DATE_PURCHASED" => StandardTag::PurchaseDate(value),

                // Locations
                "RECORDING_LOCATION" => StandardTag::RecordingLocation(value),
                // "COMPOSITION_LOCATION" => None,
                // "COMPOSER_NATIONALITY" => None,

                // Personal
                "COMMENT" => StandardTag::Comment(value),
                "PLAY_COUNTER" => StandardTag::PlayCounter(parse_number(&value)?),
                "RATING" => parse_rating(&value)?,

                // Technical
                "ENCODER" => StandardTag::Encoder(value),
                "ENCODER_SETTINGS" => StandardTag::EncoderSettings(value),
                // "BPS" => None,
                // "FPS" => None,
                "BPM" => parse_bpm(&value)?,
                "MEASURE" => StandardTag::Measure(value),
                "TUNING" => StandardTag::Tuning(value),

                // Identifiers
                "ISRC" => StandardTag::IdentIsrc(value),
                "ISBN" => StandardTag::IdentIsbn(value),
                "BARCODE" => StandardTag::IdentBarcode(value),
                "CATALOG_NUMBER" => StandardTag::IdentCatalogNumber(value),
                "LABEL_CODE" => StandardTag::LabelCode(value),
                "LCCN" => StandardTag::IdentLccn(value),
                "IMDB" => parse_imdb(&value)?,
                "TMDB" => parse_tmdb(&value)?,
                "TVDB" | "TVDB2" => parse_tvdb(&value)?,

                // Commercial
                // "PURCHASE_ITEM" => None,
                // "PURCHASE_INFO" => None,
                // "PURCHASE_OWNER" => None,
                // "PURCHASE_PRICE" => None,
                // "PURCHASE_CURRENCY" => None,

                // Legal
                "COPYRIGHT" => StandardTag::Copyright(value),
                "PRODUCTION_COPYRIGHT" => StandardTag::ProductionCopyright(value),
                "LICENSE" => StandardTag::License(value),
                "TERMS_OF_USE" => StandardTag::TermsOfUse(value),

                // Organizational
                "TOTAL_PARTS" => map_total_parts(&value, lower_ctx)?,
                "PART_NUMBER" => map_part_number(&value, target_name)?,

                // Titles
                "TITLE" => map_title(value, target_name, Variant::Normal)?,
                "SUBTITLE" => map_subtitle(value, target_name)?,

                // Original
                "ORIGINAL/ARTIST" => StandardTag::OriginalArtist(value),
                "ORIGINAL/LYRICIST" => StandardTag::OriginalLyricist(value),
                "ORIGINAL/TITLE" => map_title(value, target_name, Variant::Original)?,
                "ORIGINAL/WRITTEN_BY" => StandardTag::OriginalWriter(value),

                // Sort order
                "ARTIST/SORT_WITH" => StandardTag::SortArtist(value),
                "COMPOSER/SORT_WITH" => StandardTag::SortComposer(value),
                "TITLE/SORT_WITH" => map_title(value, target_name, Variant::SortOrder)?,

                // Unknown tag.
                _ => return None,
            },
        };

        Some(std)
    }
    // else if let RawValue::Binary(_value) = &raw.value {
    //     // Binary tags.
    //     match raw.key.as_str() {
    //         // TODO: Parse MCDI into CD-TOC string.
    //         // "MCDI" => None,
    //         _ => None,
    //     }
    // }
    else {
        // Unexpected type.
        None
    }
}

/// Try to get the default name for a target where only the target type value is specified.
///
/// For audio-only media, assumes target names that are logical for an album. For video media,
/// assumes target names that are logical for a movie.
fn default_target_name(target_value: u64, is_video: bool) -> Option<&'static str> {
    let name = match target_value {
        70 => "COLLECTION",
        60 if is_video => "VOLUME",
        60 => "EDITION",
        50 if is_video => "MOVIE",
        50 => "ALBUM",
        40 => "PART",
        30 if is_video => "CHAPTER",
        30 => "TRACK",
        20 if is_video => "SCENE",
        20 => "SUBTRACK",
        10 if is_video => "SHOT",
        _ => return None,
    };
    Some(name)
}

/// A `TOTAL_PARTS` tag indicates the total count for the next lower target level. Given the next
/// lower target level, map the total parts raw tag to an appropriate standard tag.
///
/// For example, if target level 40 (`ALBUM`) contains a `TOTAL_PARTS` tag with a value of 10, and
/// the next lower target level is 30 (`TRACK`), then there are a total of 10 tracks and the tag
/// will be mapped to `StandardTag::TrackTotal(10)`.
fn map_total_parts(value: &Arc<String>, ctx: &TagContext) -> Option<StandardTag> {
    if let Some(target) = &ctx.target {
        let target_name = target
            .name
            .as_ref()
            .map(|name| name.as_ref().deref())
            .or_else(|| default_target_name(target.value, ctx.is_video));

        if let Some(target_type_name) = target_name {
            let total = parse_number(value)?;

            let std = match target_type_name {
                // "COLLECTION" => None,
                // "EDITION" => None,
                // "ISSUE" => None,
                "VOLUME" => StandardTag::VolumeTotal(total),
                // "OPUS" => None,
                "SEASON" => StandardTag::TvSeasonTotal(total),
                // "SEQUEL" => None,
                // "ALBUM" => None,
                // "OPERA" => None,
                // "CONCERT" => None,
                // "MOVIE" => None,
                "EPISODE" => StandardTag::TvEpisodeTotal(total),
                "PART" | "SESSION" => StandardTag::DiscTotal(total),
                "TRACK" | "SONG" => StandardTag::TrackTotal(total),
                // "CHAPTER" => None,
                // "SUBTRACK" => None,
                "MOVEMENT" => StandardTag::MovementTotal(total),
                // "SCENE" => None,
                // "SHOT" => None,
                _ => return None,
            };

            Some(std)
        }
        else {
            // No explicit or default target name. It is not possible to definitively map to a
            // standard tag.
            None
        }
    }
    else {
        // There is no lower target level, so it is not possible to know what kind of target this
        // total count refers to. The raw tag cannot be mapped.
        None
    }
}

fn map_part_number(value: &Arc<String>, target: &str) -> Option<StandardTag> {
    let number = parse_number(value)?;

    let std = match target {
        // "COLLECTION" => None,
        // "EDITION" => None,
        // "ISSUE" => None,
        "VOLUME" => StandardTag::VolumeNumber(number),
        "OPUS" => StandardTag::OpusNumber(number),
        "SEASON" => StandardTag::TvSeasonNumber(number),
        // "SEQUEL" => None,
        // "ALBUM" => None,
        // "OPERA" => None,
        // "CONCERT" => None,
        // "MOVIE" => None,
        "EPISODE" => StandardTag::TvEpisodeNumber(number),
        "PART" | "SESSION" => StandardTag::DiscNumber(number),
        "TRACK" | "SONG" => StandardTag::TrackNumber(number),
        // "CHAPTER" => None,
        // "SUBTRACK" => None,
        "MOVEMENT" => StandardTag::MovementNumber(number),
        // "SCENE" => None,
        // "SHOT" => None,
        _ => return None,
    };
    Some(std)
}

enum Variant {
    /// A regular tag.
    Normal,
    /// An tag for the original.
    Original,
    /// A sort-order tag.
    SortOrder,
}

fn map_title(value: Arc<String>, target: &str, variant: Variant) -> Option<StandardTag> {
    let std = match target {
        "COLLECTION" => match variant {
            Variant::Normal => StandardTag::CollectionTitle(value),
            Variant::SortOrder => StandardTag::SortCollectionTitle(value),
            _ => return None,
        },
        "EDITION" => match variant {
            Variant::Normal => StandardTag::EditionTitle(value),
            Variant::SortOrder => StandardTag::SortEditionTitle(value),
            _ => return None,
        },
        // "ISSUE" => None,
        "VOLUME" => match variant {
            Variant::Normal => StandardTag::VolumeTitle(value),
            Variant::SortOrder => StandardTag::SortVolumeTitle(value),
            _ => return None,
        },
        "OPUS" => match variant {
            Variant::Normal => StandardTag::Opus(value),
            _ => return None,
        },
        "SEASON" => match variant {
            Variant::Normal => StandardTag::TvSeasonTitle(value),
            Variant::SortOrder => StandardTag::SortTvSeasonTitle(value),
            _ => return None,
        },
        // "SEQUEL" => None,
        "ALBUM" => match variant {
            Variant::Normal => StandardTag::Album(value),
            Variant::Original => StandardTag::OriginalAlbum(value),
            Variant::SortOrder => StandardTag::SortAlbum(value),
        },
        // "OPERA" => None,
        // "CONCERT" => None,
        "MOVIE" => match variant {
            Variant::Normal => StandardTag::MovieTitle(value),
            Variant::SortOrder => StandardTag::SortMovieTitle(value),
            _ => return None,
        },
        "EPISODE" => match variant {
            Variant::Normal => StandardTag::TvEpisodeTitle(value),
            Variant::SortOrder => StandardTag::SortTvEpisodeTitle(value),
            _ => return None,
        },
        "PART" => match variant {
            Variant::Normal => StandardTag::PartTitle(value),
            Variant::SortOrder => StandardTag::SortPartTitle(value),
            _ => return None,
        },
        // "SESSION" => None,
        "TRACK" | "SONG" => match variant {
            Variant::Normal => StandardTag::TrackTitle(value),
            Variant::SortOrder => StandardTag::SortTrackTitle(value),
            _ => return None,
        },
        "CHAPTER" => match variant {
            Variant::Normal => StandardTag::ChapterTitle(value),
            _ => return None,
        },
        // "SUBTRACK" => None,
        "MOVEMENT" => StandardTag::MovementName(value),
        // "SCENE" => None,
        // "SHOT" => None,
        _ => return None,
    };
    Some(std)
}

fn map_subtitle(value: Arc<String>, target: &str) -> Option<StandardTag> {
    let std = match target {
        "PART" | "SESSION" => StandardTag::DiscSubtitle(value),
        "TRACK" => StandardTag::TrackSubtitle(value),
        _ => return None,
    };
    Some(std)
}

fn parse_bpm(value: &Arc<String>) -> Option<StandardTag> {
    match value.parse::<f64>() {
        Ok(bpm) if bpm.is_finite() => Some(StandardTag::Bpm(bpm as u64)),
        _ => None,
    }
}

fn parse_rating(value: &Arc<String>) -> Option<StandardTag> {
    // 0.0 - 5.0 rating scale to PPM.
    match value.parse::<f32>() {
        Ok(num) if num <= 5.0 => {
            let ppm = (200_000.0 * num).round() as u32;
            Some(StandardTag::Rating(ppm))
        }
        _ => None,
    }
}

fn parse_imdb(value: &Arc<String>) -> Option<StandardTag> {
    if let Some(id) = value.strip_prefix("tt") {
        // The ID must only contain digits.
        if id.chars().any(|c| !c.is_ascii_digit()) {
            return None;
        }

        // The ID must contain atleast 7 digits.
        if id.len() < 7 {
            return None;
        }

        Some(StandardTag::ImdbTitleId(Arc::new(id.to_string())))
    }
    else {
        // Invalid format.
        None
    }
}

fn parse_tvdb(value: &Arc<String>) -> Option<StandardTag> {
    let (category, id) = value.split_once('/').unwrap_or_else(|| ("series", value.as_str()));

    // The ID must only contain digits.
    if id.chars().any(|c| !c.is_ascii_digit()) {
        return None;
    }

    if category.eq_ignore_ascii_case("series") {
        // TVDB series ID.
        Some(StandardTag::TvdbSeriesId(Arc::new(id.to_string())))
    }
    else if category.eq_ignore_ascii_case("episodes") {
        // TVDB episode ID.
        Some(StandardTag::TvdbEpisodeId(Arc::new(id.to_string())))
    }
    else if category.eq_ignore_ascii_case("movies") {
        // TVDB movie ID.
        Some(StandardTag::TvdbMovieId(Arc::new(id.to_string())))
    }
    else {
        // Unknown category.
        None
    }
}

fn parse_tmdb(value: &Arc<String>) -> Option<StandardTag> {
    if let Some((category, id)) = value.split_once('/') {
        // The ID must only contain digits.
        if id.chars().any(|c| !c.is_ascii_digit()) {
            return None;
        }

        if category.eq_ignore_ascii_case("movie") {
            // TMDB movie ID.
            Some(StandardTag::TmdbMovieId(Arc::new(id.to_string())))
        }
        else if category.eq_ignore_ascii_case("tv") {
            // TMDB series ID
            Some(StandardTag::TmdbSeriesId(Arc::new(id.to_string())))
        }
        else {
            // Unknown category.
            None
        }
    }
    else {
        // Invalid format.
        None
    }
}

fn parse_number(value: &Arc<String>) -> Option<u64> {
    value.parse::<u64>().ok()
}
