// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::collections::HashMap;
use std::num::NonZeroU64;
use std::rc::Rc;
use std::sync::Arc;

use symphonia_core::codecs::video::VideoExtraData;
use symphonia_core::codecs::video::well_known::extra_data::{
    VIDEO_EXTRA_DATA_ID_DOLBY_VISION_CONFIG, VIDEO_EXTRA_DATA_ID_DOLBY_VISION_EL_HEVC,
};
use symphonia_core::formats::{Attachment, FileAttachment, TrackFlags};
use symphonia_core::meta::well_known::METADATA_ID_MATROSKA;
use symphonia_core::meta::{
    Chapter, ChapterGroup, ChapterGroupItem, MetadataBuilder, MetadataInfo, MetadataRevision,
    PerTrackMetadataBuilder, RawTag, RawTagSubField, RawValue, StandardTag, Tag,
};
use symphonia_core::units::{Duration, Time, TimeBase, Timestamp};

use crate::ebml::{EbmlElement, EbmlElementHeader, EbmlError, EbmlIterator, ReadEbml, Result};
use crate::schema::{MkvElement, MkvSchema};
use crate::sub_fields::*;
use crate::tags::{TagContext, Target, make_raw_tags, map_std_tag};

const MKV_METADATA_INFO: MetadataInfo = MetadataInfo {
    metadata: METADATA_ID_MATROSKA,
    short_name: "mkv",
    long_name: "Matroska / WebM",
};

// NOTES ON READING EBML ELEMENTS
// ==============================
//
// EBML elements are classified as mandatory or non-mandatory. Mandatory EBML elements must be
// present if they do not have a schema-defined default value. If a mandatory element is not
// present, then the schema-defined default value should be used instead. On the other hand,
// non-mandatory EBML elements do not need to be present and no default value is assumed in their
// absence.
//
// All non-master EBML elements are defined by the schema to carry one piece of data in one of the
// EBML-defined primitive data-types: unsigned integer, signed integer, float, date, string, or
// binary buffer. However, an EBML element may also be "empty" (i.e., the element is written to the
// EBML document without any data), in which case the schema-defined default value should be used,
// if one was defined, or the default value for the data-type (0 or "").
//
// Therefore, the value yielded for an element depends on:
//
//  1. If the element is defined to be mandatory or non-mandatory.
//  2. If the element is present in the document or not.
//  3. If the element has a schema-defined default value or not.
//
// The table below summarizes the intended behaviour as per the EBML standard.
//
// +---------------+---------------------------------------+---------------------------------------+
// |               | Element is Present                    | Element is not Present                |
// +---------------+---------------------------------------+---------------------------------------+
// | Mandatory     | Not-empty: Use written value.         | Schema default [2], or error [3].     |
// | Element       | Empty:     Schema or type default [1] |                                       |
// +---------------+---------------------------------------+---------------------------------------+
// | Non-Mandatory | Not-empty: Use written value.         | Do nothing. Do not use.               |
// | Element       | Empty:     Schema or type default [1] |                                       |
// +---------------+---------------------------------------+---------------------------------------+
//
// [1] RFC-8794 (Extensible Binary Meta Language), Sect. 6.1
// [2] RFC-8794 (Extensible Binary Meta Language), Sect. 11.1.6.8
// [3] RFC-8794 (Extensible Binary Meta Language), Sect. 11.1.19
//
// EBML ITERATOR
// ~~~~~~~~~~~~~
//
// The EBML iterator provides 3 variants of read functions per data-type to support the scenarios
// above.
//
//  1. `read_TYPE`            - Returns an `Option<T>` where `None` indicates an empty element.
//  2. `read_TYPE_default`    - Takes a schema-defined default value to return when the element is
//                              empty.
//  3. `read_TYPE_no_default` - Returns the type-defined default value when the element is empty.
//                              For use when there is no schema-defined default value.
//
// MODULE CONVENTIONS
// ~~~~~~~~~~~~~~~~~~
//
// The code in this module shall use the following conventions for implementation consistency:
//
//  1. Excluding mandatory elements with schema-defined defaults, element values shall be read with
//     either `read_TYPE_default` or `read_TYPE_no_default`, to always yield a non-empty value, then
//     stored in an `Option<T>`.
//  2. For mandatory elements with schema-defined defaults, element values shall be read with
//     `read_TYPE`. The returned `Option<T>` is stored directly.
//  3. After iterating over all elements, the `Option`s for mandatory elements with schema-defined
//     defaults shall be `unwrap_or`'d with their schema-defined default. Mandatory elements without
//     schema-defined defaults that are `None` shall result in an error.
//  4. The per-element data structures shall be defined such that only non-mandatory element values
//     are wrapped in options.

type MkvEbmlIterator<R> = EbmlIterator<R, MkvSchema>;
type MkvEbmlElementHeader = EbmlElementHeader<MkvSchema>;

/// Matroska ticks.
///
/// Matroska ticks are unsigned timestamps or durations stored in nanoseconds.
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord)]
pub struct MatroskaTicks(u64);

impl MatroskaTicks {
    /// Get the underlying value.
    #[inline]
    pub const fn get(self) -> u64 {
        self.0
    }

    /// Convert `self` into Segment ticks using the segment timebase.
    ///
    /// The Segment timebase is the media timebase.
    #[inline]
    pub fn into_segment_ticks(self, segment_time_base: TimeBase) -> SegmentTicks {
        // The timebase may have been reduced so it is not possible to assume a 1_000_000_000
        // denominator.
        let factor = (1_000_000_000 * u64::from(segment_time_base.numer.get()))
            / u64::from(segment_time_base.denom.get());
        SegmentTicks(self.0 / factor)
    }

    /// Convert `self` into Track ticks using the track timebase.
    #[inline]
    pub fn into_track_ticks(self, track_time_base: TimeBase) -> TrackTicks {
        // The timebase may have been reduced so it is not possible to assume a 1_000_000_000
        // denominator.
        let factor = (1_000_000_000 * u64::from(track_time_base.numer.get()))
            / u64::from(track_time_base.denom.get());
        TrackTicks(self.0 / factor)
    }
}

impl From<u64> for MatroskaTicks {
    #[inline]
    fn from(value: u64) -> Self {
        MatroskaTicks(value)
    }
}

impl std::fmt::Display for MatroskaTicks {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        self.0.fmt(f)
    }
}

/// Non-zero Matroska ticks.
///
/// Matroska ticks are unsigned timestamps or durations stored in nanoseconds.
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct NonZeroMatroskaTicks(NonZeroU64);

impl NonZeroMatroskaTicks {
    /// Get the underlying value.
    #[inline]
    pub const fn get(&self) -> u64 {
        self.0.get()
    }
}

impl From<NonZeroU64> for NonZeroMatroskaTicks {
    #[inline]
    fn from(value: NonZeroU64) -> Self {
        Self(value)
    }
}

/// Signed Matroska ticks.
///
/// Matroska ticks are timestamps or durations stored in nanoseconds.
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord)]
pub struct SignedMatroskaTicks(i64);

impl From<i64> for SignedMatroskaTicks {
    #[inline]
    fn from(value: i64) -> Self {
        SignedMatroskaTicks(value)
    }
}

/// Segment ticks.
///
/// Segment ticks are scaled by `TimestampScale`, the timebase of the Segment.
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord)]
pub struct SegmentTicks(u64);

impl SegmentTicks {
    /// Get the underlying value.
    #[allow(dead_code)]
    #[inline]
    pub const fn get(self) -> u64 {
        self.0
    }

    /// Convert `self` into Track ticks using the track's timestamp scale.
    #[inline]
    pub fn into_track_ticks(self, track_timestamp_scale: f64) -> TrackTicks {
        if track_timestamp_scale == 1.0 {
            return TrackTicks(self.0);
        }
        TrackTicks((self.0 as f64 * track_timestamp_scale).round() as u64)
    }
}

impl From<u64> for SegmentTicks {
    #[inline]
    fn from(value: u64) -> Self {
        Self(value)
    }
}

impl std::fmt::Display for SegmentTicks {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        self.0.fmt(f)
    }
}

/// Floating point segment ticks.
///
/// Segment ticks are scaled by `TimestampScale`, the timebase of the Segment.
#[derive(Copy, Clone, Debug, Default, PartialEq, PartialOrd)]
pub struct FloatingPointSegmentTicks(f64);

impl From<f64> for FloatingPointSegmentTicks {
    #[inline]
    fn from(value: f64) -> Self {
        Self(value)
    }
}

impl FloatingPointSegmentTicks {
    /// Get the underlying value.
    #[inline]
    pub const fn get(self) -> f64 {
        self.0
    }
}

/// Track ticks.
///
/// Track ticks are scaled by `TimescapeScale * TrackTimestampScale`, the timebase of the track.
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord)]
pub struct TrackTicks(u64);

impl TrackTicks {
    /// Get the underlying value.
    #[inline]
    pub const fn get(&self) -> u64 {
        self.0
    }

    /// Try to convert into signed Track ticks.
    #[inline]
    pub fn try_into_signed(self) -> Option<SignedTrackTicks> {
        self.0.try_into().map(SignedTrackTicks).ok()
    }

    /// Convert into a `Duration`.
    #[inline]
    pub fn into_dur(self) -> Duration {
        Duration::new(self.0)
    }
}

impl From<u64> for TrackTicks {
    #[inline]
    fn from(value: u64) -> Self {
        Self(value)
    }
}

impl std::fmt::Display for TrackTicks {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        self.0.fmt(f)
    }
}

/// Signed Track ticks.
///
/// Tracck ticks are scaled by `TimescapeScale * TrackTimestampScale`, the timebase of the track.
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord)]
pub struct SignedTrackTicks(i64);

impl SignedTrackTicks {
    /// Get the underlying value.
    #[allow(dead_code)]
    #[inline]
    pub const fn get(&self) -> i64 {
        self.0
    }

    // Convert into a Timestamp.
    #[inline]
    pub fn into_ts(self) -> Timestamp {
        Timestamp::new(self.0)
    }

    /// Add the provided signed track ticks to `self`, returning `None` if an overflow occurred.
    #[inline]
    pub fn checked_add(self, other: Self) -> Option<Self> {
        self.0.checked_add(other.0).map(SignedTrackTicks)
    }

    /// Add the provided track ticks to `self`, returning `None` if an overflow occurred.
    #[inline]
    pub fn checked_add_unsigned(self, other: TrackTicks) -> Option<Self> {
        self.0.checked_add_unsigned(other.0).map(SignedTrackTicks)
    }

    /// Subtract the provided track ticks to `self`, returning `None` if an overflow occurred.
    #[inline]
    pub fn checked_sub_unsigned(self, other: TrackTicks) -> Option<Self> {
        self.0.checked_sub_unsigned(other.0).map(SignedTrackTicks)
    }

    /// Try to convert Segment ticks to Matroska ticks using the track timebase.
    #[inline]
    pub fn try_into_matroska_ticks(self, track_time_base: TimeBase) -> Option<MatroskaTicks> {
        if self.0.is_negative() {
            return None;
        }
        (self.0 as u64).checked_mul(u64::from(track_time_base.numer.get())).map(MatroskaTicks)
    }
}

impl From<i64> for SignedTrackTicks {
    #[inline]
    fn from(value: i64) -> Self {
        Self(value)
    }
}

impl From<Timestamp> for SignedTrackTicks {
    #[inline]
    fn from(value: Timestamp) -> Self {
        Self(value.get())
    }
}

impl std::fmt::Display for SignedTrackTicks {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        self.0.fmt(f)
    }
}

#[allow(dead_code)]
#[derive(Debug)]
pub(crate) struct TrackElement {
    pub(crate) number: NonZeroU64,
    pub(crate) uid: NonZeroU64,
    pub(crate) lang: String,
    pub(crate) lang_bcp47: Option<String>,
    pub(crate) codec_id: String,
    pub(crate) codec_private: Option<Box<[u8]>>,
    pub(crate) codec_delay: MatroskaTicks,
    pub(crate) block_addition_mappings: Vec<BlockAdditionMappingElement>,
    pub(crate) audio: Option<AudioElement>,
    pub(crate) video: Option<VideoElement>,
    pub(crate) default_duration: Option<NonZeroMatroskaTicks>,
    pub(crate) default_decoded_field_duration: Option<NonZeroMatroskaTicks>,
    pub(crate) track_timestamp_scale: f64,
    pub(crate) seek_pre_roll: MatroskaTicks,
    pub(crate) flags: TrackFlags,
}

impl EbmlElement<MkvSchema> for TrackElement {
    const TYPE: MkvElement = MkvElement::TrackEntry;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut number = None;
        let mut uid = None;
        let mut lang = None;
        let mut lang_bcp47 = None;
        let mut audio = None;
        let mut video = None;
        let mut block_addition_mappings = Vec::new();
        let mut codec_id = None;
        let mut codec_private = None;
        let mut codec_delay = None;
        let mut default_duration = None;
        let mut default_decoded_field_duration = None;
        let mut track_timestamp_scale = None;
        let mut seek_pre_roll = None;
        let mut flags = Default::default();

        while let Some(child) = it.next_header()? {
            match child.element_type() {
                MkvElement::TrackNumber => {
                    // Mandatory element. May not be 0. No schema-defined default.
                    let val = NonZeroU64::new(it.read_u64_no_default()?)
                        .ok_or(EbmlError::ElementError("mkv: invalid (0) track number"))?;

                    number = Some(val);
                }
                MkvElement::TrackUid => {
                    // Mandatory element. May not be 0. No schema-defined default.
                    let val = NonZeroU64::new(it.read_u64_no_default()?)
                        .ok_or(EbmlError::ElementError("mkv: invalid (0) track uid"))?;

                    uid = Some(val);
                }
                MkvElement::Language => {
                    // Mandatory element. Schema-defined default is "eng".
                    lang = it.read_string()?;
                }
                MkvElement::LanguageBcp47 => {
                    // Non-mandatory element. No schema-defined default.
                    lang_bcp47 = Some(it.read_string_no_default()?);
                }
                MkvElement::CodecId => {
                    // Mandatory element. No schema-defined default.
                    codec_id = Some(it.read_string_no_default()?);
                }
                MkvElement::CodecPrivate => {
                    // Non-mandatory element.
                    codec_private = Some(it.read_binary()?);
                }
                MkvElement::CodecDelay => {
                    // Mandatory element. Schema-defined default is 0.
                    codec_delay = it.read_u64()?.map(MatroskaTicks);
                }
                MkvElement::Audio => {
                    // Non-mandatory element.
                    audio = Some(it.read_master_element()?);
                }
                MkvElement::Video => {
                    // Non-mandatory element.
                    video = Some(it.read_master_element()?);
                }
                MkvElement::BlockAdditionMapping => {
                    // Non-mandatory element.
                    block_addition_mappings.push(it.read_master_element()?);
                }
                MkvElement::DefaultDuration => {
                    // Non-mandatory. May not be 0. No schema-defined default.
                    let val = NonZeroU64::new(it.read_u64_no_default()?).ok_or(
                        EbmlError::ElementError("mkv: invalid (0) track default duration"),
                    )?;

                    default_duration = Some(NonZeroMatroskaTicks::from(val));
                }
                MkvElement::DefaultDecodedFieldDuration => {
                    // Non-mandatory. May not be 0. No schema-defined default.
                    let val = NonZeroU64::new(it.read_u64_no_default()?).ok_or(
                        EbmlError::ElementError(
                            "mkv: invalid (0) track default decoded field duration",
                        ),
                    )?;

                    default_decoded_field_duration = Some(NonZeroMatroskaTicks::from(val));
                }
                MkvElement::TrackTimestampScale => {
                    // Mandatory element. Schema-defined default is 1.0. Deprecated v3.
                    track_timestamp_scale = Some(it.read_f64_default(1.0)?);
                }
                MkvElement::SeekPreRoll => {
                    // Mandatory element. Schema-defined default is 0.
                    seek_pre_roll = it.read_u64()?.map(MatroskaTicks);
                }
                MkvElement::FlagDefault => {
                    // Mandatory element. Schema-defined default is 1 (set).
                    if it.read_u64_default(1)? == 1 {
                        flags |= TrackFlags::DEFAULT;
                    }
                }
                MkvElement::FlagForced => {
                    // Mandatory element. Schema-defined default is 0 (unset).
                    if it.read_u64_default(0)? == 1 {
                        flags |= TrackFlags::FORCED;
                    }
                }
                MkvElement::FlagHearingImpaired => {
                    // Non-mandatory element. Schema-defined default is 0 (unset).
                    if it.read_u64_default(0)? == 1 {
                        flags |= TrackFlags::HEARING_IMPAIRED;
                    }
                }
                MkvElement::FlagVisualImpaired => {
                    // Non-mandatory element. Schema-defined default is 0 (unset).
                    if it.read_u64_default(0)? == 1 {
                        flags |= TrackFlags::VISUALLY_IMPAIRED;
                    }
                }
                MkvElement::FlagTextDescriptions => {
                    // Non-mandatory element. Schema-defined default is 0 (unset).
                    if it.read_u64_default(0)? == 1 {
                        flags |= TrackFlags::TEXT_DESCRIPTIONS;
                    }
                }
                MkvElement::FlagOriginal => {
                    // Non-mandatory element. Schema-defined default is 0 (unset).
                    if it.read_u64_default(0)? == 1 {
                        flags |= TrackFlags::ORIGINAL_LANGUAGE;
                    }
                }
                MkvElement::FlagCommentary => {
                    // Non-mandatory element. Schema-defined default is 0 (unset).
                    if it.read_u64_default(0)? == 1 {
                        flags |= TrackFlags::COMMENTARY;
                    }
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        // Populate missing or empty mandatory elements that have default values.
        let lang = lang.unwrap_or_else(|| "eng".into());
        let codec_delay = codec_delay.unwrap_or(MatroskaTicks(0));
        let track_timestamp_scale = track_timestamp_scale.unwrap_or(1.0);
        let seek_pre_roll = seek_pre_roll.unwrap_or(MatroskaTicks(0));

        Ok(Self {
            number: number.ok_or(EbmlError::ElementError("mkv: missing track number"))?,
            uid: uid.ok_or(EbmlError::ElementError("mkv: missing track uid"))?,
            lang,
            lang_bcp47,
            codec_id: codec_id.ok_or(EbmlError::ElementError("mkv: missing codec id"))?,
            codec_private,
            codec_delay,
            block_addition_mappings,
            audio,
            video,
            default_duration,
            default_decoded_field_duration,
            track_timestamp_scale,
            seek_pre_roll,
            flags,
        })
    }
}

#[allow(dead_code)]
#[derive(Debug)]
pub(crate) struct AudioElement {
    pub(crate) sampling_frequency: f64,
    pub(crate) output_sampling_frequency: Option<f64>,
    pub(crate) channels: NonZeroU64,
    pub(crate) bit_depth: Option<NonZeroU64>,
}

impl EbmlElement<MkvSchema> for AudioElement {
    const TYPE: MkvElement = MkvElement::Audio;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut sampling_frequency = None;
        let mut output_sampling_frequency = None;
        let mut channels = None;
        let mut bit_depth = None;

        while let Some(child) = it.next_header()? {
            match child.element_type() {
                MkvElement::SamplingFrequency => {
                    // Mandatory element. Must be > 0.0. Schema-defined default is 8000.
                    sampling_frequency = match it.read_f64()? {
                        Some(freq) if freq > 0.0 => Some(freq),
                        Some(_) => {
                            return Err(EbmlError::ElementError(
                                "mkv: invalid (<= 0.0) audio sampling frequency",
                            ));
                        }
                        _ => None,
                    };
                }
                MkvElement::OutputSamplingFrequency => {
                    // Non-mandatory element. Must be > 0.0. Schema-defined default is equal to
                    // `sampling_frequency`.
                    output_sampling_frequency = match it.read_f64()? {
                        None => Some(None),
                        Some(freq) if freq > 0.0 => Some(Some(freq)),
                        Some(_) => {
                            return Err(EbmlError::ElementError(
                                "mkv: invalid (<= 0.0) audio output sampling frequency",
                            ));
                        }
                    };
                }
                MkvElement::Channels => {
                    // Mandatory element. Must not be 0. Schema-defined default is 1.
                    channels = it
                        .read_u64()?
                        .map(|s| {
                            NonZeroU64::new(s)
                                .ok_or(EbmlError::ElementError("mkv: invalid (0) audio channels"))
                        })
                        .transpose()?;
                }
                MkvElement::BitDepth => {
                    // Non-mandatory element. Must not be 0. No schema-defined default.
                    let val = NonZeroU64::new(it.read_u64_no_default()?)
                        .ok_or(EbmlError::ElementError("mkv: invalid (0) track number"))?;

                    bit_depth = Some(val);
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        // Populate missing to empty mandatory element defaults.
        let sampling_frequency = sampling_frequency.unwrap_or(8000.0);
        let channels = channels.unwrap_or(NonZeroU64::new(1).expect("1 is non-zero"));

        // The output sampling frequency is a non-mandatory element. If it was present and not
        // empty, then use the contained value. If it was empty, then it defaults to the value of
        // the sampling frequency element.
        let output_sampling_frequency =
            output_sampling_frequency.map(|freq| freq.unwrap_or(sampling_frequency));

        Ok(Self { sampling_frequency, output_sampling_frequency, channels, bit_depth })
    }
}

#[allow(dead_code)]
#[derive(Debug)]
pub(crate) struct VideoElement {
    pub(crate) pixel_width: NonZeroU64,
    pub(crate) pixel_height: NonZeroU64,
}

impl EbmlElement<MkvSchema> for VideoElement {
    const TYPE: MkvElement = MkvElement::Video;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut pixel_width = None;
        let mut pixel_height = None;

        while let Some(child) = it.next_header()? {
            match child.element_type() {
                MkvElement::PixelWidth => {
                    // Mandatory element. No schema-defined default.
                    let val = NonZeroU64::new(it.read_u64_no_default()?)
                        .ok_or(EbmlError::ElementError("mkv: invalid (0) video width"))?;

                    pixel_width = Some(val);
                }
                MkvElement::PixelHeight => {
                    // Mandatory element. No schema-defined default.
                    let val = NonZeroU64::new(it.read_u64_no_default()?)
                        .ok_or(EbmlError::ElementError("mkv: invalid (0) video height"))?;

                    pixel_height = Some(val);
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        Ok(Self {
            pixel_width: pixel_width.ok_or(EbmlError::ElementError("mkv: missing video width"))?,
            pixel_height: pixel_height
                .ok_or(EbmlError::ElementError("mkv: missing video height"))?,
        })
    }
}

#[derive(Debug)]
pub(crate) struct BlockAdditionMappingElement {
    pub(crate) extra_data: Option<VideoExtraData>,
}

impl EbmlElement<MkvSchema> for BlockAdditionMappingElement {
    const TYPE: MkvElement = MkvElement::BlockAdditionMapping;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        // There can be many BlockAdditionMapping elements with DolbyVisionConfiguration in a single
        // track BlockAddIdType FourCC string allows to determine the type of
        // DolbyVisionConfiguration extra data
        let mut extra_data = None;
        let mut block_add_id_type = None;

        while let Some(child) = it.next_header()? {
            match child.element_type() {
                MkvElement::BlockAddIdType => {
                    // Mandatory element. Default is 0.
                    block_add_id_type = it.read_u64()?;
                }
                MkvElement::BlockAddIdExtraData => {
                    // Non-manadatory element. Interpret block addition type ID as FourCC.
                    match &u32::to_be_bytes(block_add_id_type.unwrap_or(0) as u32) {
                        b"dvcC" | b"dvvC" => {
                            extra_data = Some(VideoExtraData {
                                id: VIDEO_EXTRA_DATA_ID_DOLBY_VISION_CONFIG,
                                data: it.read_binary()?,
                            });
                        }
                        b"hvcE" => {
                            extra_data = Some(VideoExtraData {
                                id: VIDEO_EXTRA_DATA_ID_DOLBY_VISION_EL_HEVC,
                                data: it.read_binary()?,
                            });
                        }
                        _ => {}
                    }
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        Ok(Self { extra_data })
    }
}

#[derive(Debug)]
pub(crate) struct SeekHeadElement {
    pub(crate) seeks: Box<[SeekElement]>,
}

impl EbmlElement<MkvSchema> for SeekHeadElement {
    const TYPE: MkvElement = MkvElement::SeekHead;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut seeks = Vec::new();

        while let Some(child) = it.next_header()? {
            match child.element_type() {
                MkvElement::Seek => {
                    // Mandatory element.
                    seeks.push(it.read_master_element()?);
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        Ok(Self { seeks: seeks.into_boxed_slice() })
    }
}

#[derive(Debug)]
pub(crate) struct SeekElement {
    pub(crate) id: u64,
    pub(crate) position: u64,
}

impl EbmlElement<MkvSchema> for SeekElement {
    const TYPE: MkvElement = MkvElement::Seek;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut seek_id = None;
        let mut seek_position = None;

        while let Some(child) = it.next_header()? {
            match child.element_type() {
                MkvElement::SeekId => {
                    // Read
                    let mut raw = [0u8; 8];

                    seek_id = match it.read_binary_into(&mut raw)? {
                        len @ 1..=8 => {
                            let mut buf = [0u8; 8];
                            buf[8 - len..].copy_from_slice(&raw[..len]);
                            Some(u64::from_be_bytes(buf))
                        }
                        _ => return Err(EbmlError::ElementError("mkv: invalid seek element id")),
                    };
                    // TODO: This is actually an EBML element ID. Read it properly.
                }
                MkvElement::SeekPosition => {
                    // Mandatory element. Must not be 0. No schema-defined default.
                    seek_position = Some(it.read_u64_no_default()?);
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        Ok(Self {
            id: seek_id.ok_or(EbmlError::ElementError("mkv: missing seek track id"))?,
            position: seek_position
                .ok_or(EbmlError::ElementError("mkv: missing seek track position"))?,
        })
    }
}

#[derive(Debug)]
pub(crate) struct TracksElement {
    pub(crate) tracks: Box<[TrackElement]>,
}

impl EbmlElement<MkvSchema> for TracksElement {
    const TYPE: MkvElement = MkvElement::Tracks;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut tracks = vec![];

        while let Some(child) = it.next_header()? {
            match child.element_type() {
                MkvElement::TrackEntry => {
                    // Mandatory element.
                    tracks.push(it.read_master_element()?);
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        Ok(Self { tracks: tracks.into_boxed_slice() })
    }
}

impl TracksElement {
    pub(crate) fn get_target_uids(&self, target_tags: &mut TargetTagsMap) {
        self.tracks.iter().for_each(|track| {
            target_tags.insert(TargetUid::Track(track.uid.get()), Default::default());
        })
    }
}

#[allow(dead_code)]
#[derive(Debug)]
pub(crate) struct EbmlHeaderElement {
    pub(crate) version: NonZeroU64,
    pub(crate) read_version: NonZeroU64,
    pub(crate) max_id_length: u64,
    pub(crate) max_size_length: u64,
    pub(crate) doc_type: String,
    pub(crate) doc_type_version: NonZeroU64,
    pub(crate) doc_type_read_version: NonZeroU64,
}

impl EbmlElement<MkvSchema> for EbmlHeaderElement {
    const TYPE: MkvElement = MkvElement::Ebml;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut version = None;
        let mut read_version = None;
        let mut max_id_length = None;
        let mut max_size_length = None;
        let mut doc_type = None;
        let mut doc_type_version = None;
        let mut doc_type_read_version = None;

        while let Some(child) = it.next_header()? {
            match child.element_type() {
                MkvElement::EbmlVersion => {
                    // Mandatory element. Must not be 0. Schema-defined default is 1.
                    version = it
                        .read_u64()?
                        .map(|s| {
                            NonZeroU64::new(s)
                                .ok_or(EbmlError::ElementError("mkv: invalid (0) ebml version"))
                        })
                        .transpose()?;
                }
                MkvElement::EbmlReadVersion => {
                    // Mandatory element. Must note be 0. Schema-defined default is 1.
                    read_version = it
                        .read_u64()?
                        .map(|s| {
                            NonZeroU64::new(s).ok_or(EbmlError::ElementError(
                                "mkv: invalid (0) ebml read version",
                            ))
                        })
                        .transpose()?;
                }
                MkvElement::EbmlMaxIdLength => {
                    // Mandatory element. Must be >= 4. Schema-defined default is 4.
                    max_id_length = match it.read_u64()? {
                        Some(len) if len < 4 => {
                            return Err(EbmlError::ElementError(
                                "mkv: invalid ebml maximum id length",
                            ));
                        }
                        len => len,
                    }
                }
                MkvElement::EbmlMaxSizeLength => {
                    // Mandatory element. Must not be 0. Schema-defined default is 8.
                    max_size_length = match it.read_u64()? {
                        Some(0) => {
                            return Err(EbmlError::ElementError(
                                "mkv: invalid ebml maximum size length",
                            ));
                        }
                        len => len,
                    }
                }
                MkvElement::DocType => {
                    // Mandatory element. No schema-defined default.
                    doc_type = it.read_string()?;
                }
                MkvElement::DocTypeVersion => {
                    // Mandatory element. Not not be 0. Schema-defined default is 1.
                    doc_type_version = it
                        .read_u64()?
                        .map(|s| {
                            NonZeroU64::new(s).ok_or(EbmlError::ElementError(
                                "mkv: invalid (0) ebml document type version",
                            ))
                        })
                        .transpose()?;
                }
                MkvElement::DocTypeReadVersion => {
                    // Mandatory element. Must not be 0. Schema-defined default is 1.
                    doc_type_read_version = it
                        .read_u64()?
                        .map(|s| {
                            NonZeroU64::new(s).ok_or(EbmlError::ElementError(
                                "mkv: invalid (0) ebml document type read version",
                            ))
                        })
                        .transpose()?;
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        let version = version.unwrap_or(NonZeroU64::new(1).expect("1 is non-zero"));
        let read_version = read_version.unwrap_or(NonZeroU64::new(1).expect("1 is non-zero"));
        let doc_type_version =
            doc_type_version.unwrap_or(NonZeroU64::new(1).expect("1 is non-zero"));
        let doc_type_read_version =
            doc_type_read_version.unwrap_or(NonZeroU64::new(1).expect("1 is non-zero"));

        // EbmlReadVersion must be <= EbmlVersion.
        if read_version > version {
            return Err(EbmlError::ElementError(
                "mkv: ebml minimum reader version must be <= ebml version",
            ));
        }

        // DocTypeReadVersion must be <= DocTypeVersion.
        if doc_type_read_version > doc_type_version {
            return Err(EbmlError::ElementError(
                "mkv: ebml minimum document type reader version must be <= document type version",
            ));
        }

        Ok(Self {
            version,
            read_version,
            max_id_length: max_id_length.unwrap_or(4),
            max_size_length: max_size_length.unwrap_or(8),
            doc_type: doc_type.ok_or(EbmlError::ElementError("mkv: missing ebml doc type"))?,
            doc_type_version,
            doc_type_read_version,
        })
    }
}

#[allow(dead_code)]
#[derive(Debug)]
pub(crate) struct InfoElement {
    pub(crate) timestamp_scale: NonZeroU64,
    pub(crate) duration: Option<FloatingPointSegmentTicks>,
    title: Option<Box<str>>,
    muxing_app: Box<str>,
    writing_app: Box<str>,
}

impl EbmlElement<MkvSchema> for InfoElement {
    const TYPE: MkvElement = MkvElement::Info;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut duration = None;
        let mut timestamp_scale = None;
        let mut title = None;
        let mut muxing_app = None;
        let mut writing_app = None;

        while let Some(child) = it.next_header()? {
            match child.element_type() {
                MkvElement::TimestampScale => {
                    // Mandatory element. Must not be 0. Schema-defined default is 1'000'000.
                    timestamp_scale = it
                        .read_u64()?
                        .map(|s| {
                            NonZeroU64::new(s).ok_or(EbmlError::ElementError(
                                "mkv: invalid (0) info timestamp scale",
                            ))
                        })
                        .transpose()?;
                }
                MkvElement::Duration => {
                    // Non-mandatory element. No schema-defined default.
                    // TODO: Must not be > 0.0.
                    duration = Some(FloatingPointSegmentTicks::from(it.read_f64_no_default()?));
                }
                MkvElement::Title => {
                    // Non-mandatory element. No schema-defined default.
                    title = Some(it.read_string_no_default()?);
                }
                MkvElement::MuxingApp => {
                    // Mandatory element. No schema-defined default.
                    muxing_app = Some(it.read_string_no_default()?);
                }
                MkvElement::WritingApp => {
                    // Mandatory element. No schema-defined default.
                    writing_app = Some(it.read_string_no_default()?);
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        // Populate missing or empty mandatory elements with defaults.
        let timestamp_scale =
            timestamp_scale.unwrap_or(NonZeroU64::new(1_000_000).expect("1_000_000 is non-zero"));

        Ok(Self {
            timestamp_scale,
            duration,
            title: title.map(|it| it.into_boxed_str()),
            muxing_app: muxing_app
                .ok_or(EbmlError::ElementError("mkv: missing info muxing app"))?
                .into_boxed_str(),
            writing_app: writing_app
                .ok_or(EbmlError::ElementError("mkv: missing info writing app"))?
                .into_boxed_str(),
        })
    }
}

#[allow(dead_code)]
#[derive(Debug)]
pub(crate) struct CuesElement {
    pub(crate) points: Box<[CuePointElement]>,
}

impl EbmlElement<MkvSchema> for CuesElement {
    const TYPE: MkvElement = MkvElement::Cues;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut points = vec![];

        while let Some(child) = it.next_header()? {
            match child.element_type() {
                MkvElement::CuePoint => {
                    // Mandatory element.
                    points.push(it.read_master_element()?);
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        Ok(Self { points: points.into_boxed_slice() })
    }
}

#[allow(dead_code)]
#[derive(Debug)]
pub(crate) struct CuePointElement {
    /// Time in segment ticks.
    pub(crate) time: MatroskaTicks,
    pub(crate) positions: CueTrackPositionsElement,
}

impl EbmlElement<MkvSchema> for CuePointElement {
    const TYPE: MkvElement = MkvElement::CuePoint;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut time = None;
        let mut positions = None;

        while let Some(child) = it.next_header()? {
            match child.element_type() {
                MkvElement::CueTime => {
                    // Mandatory element. No schema-defined default.
                    time = Some(MatroskaTicks(it.read_u64_no_default()?));
                }
                MkvElement::CueTrackPositions => {
                    // Mandatory element.
                    positions = Some(it.read_master_element()?);
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        Ok(Self {
            time: time.ok_or(EbmlError::ElementError("mkv: missing time in cue"))?,
            positions: positions.ok_or(EbmlError::ElementError("mkv: missing positions in cue"))?,
        })
    }
}

#[allow(dead_code)]
#[derive(Debug)]
pub(crate) struct CueTrackPositionsElement {
    pub(crate) track: NonZeroU64,
    pub(crate) cluster_pos: u64,
    pub(crate) cluster_rel_pos: Option<u64>,
}

impl EbmlElement<MkvSchema> for CueTrackPositionsElement {
    const TYPE: MkvElement = MkvElement::CueTrackPositions;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut track = None;
        let mut cluster_pos = None;
        let mut cluster_rel_pos = None;

        while let Some(child) = it.next_header()? {
            match child.element_type() {
                MkvElement::CueTrack => {
                    // Mandatory element. May not be 0. No schema-defined default.
                    let val = NonZeroU64::new(it.read_u64_no_default()?).ok_or(
                        EbmlError::ElementError("mkv: invalid (0) track for cue track positions"),
                    )?;

                    track = Some(val);
                }
                MkvElement::CueClusterPosition => {
                    // Mandatory element. No schema-defined default.
                    cluster_pos = Some(it.read_u64_no_default()?);
                }
                MkvElement::CueRelativePosition => {
                    // Non-mandatory element. No schema-defined default.
                    cluster_rel_pos = Some(it.read_u64_no_default()?);
                }
                MkvElement::CueDuration => {
                    // Cue duration is not required but are so numerous we don't want to log them
                    // as unexpected elements, or when they're skipped. Explictly skip to silence.
                    // logs.
                    it.skip_data()?;
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }
        Ok(Self {
            track: track
                .ok_or(EbmlError::ElementError("mkv: missing track in cue track positions"))?,
            cluster_pos: cluster_pos
                .ok_or(EbmlError::ElementError("mkv: missing position in cue track positions"))?,
            cluster_rel_pos,
        })
    }
}

#[allow(dead_code)]
#[derive(Debug)]
pub(crate) struct BlockGroupElement {
    pub(crate) data: Box<[u8]>,
    pub(crate) duration: Option<TrackTicks>,
    pub(crate) reference_block: Option<i64>,
    pub(crate) discard_padding: Option<SignedMatroskaTicks>,
}

impl EbmlElement<MkvSchema> for BlockGroupElement {
    const TYPE: MkvElement = MkvElement::BlockGroup;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut data = None;
        let mut block_duration = None;
        let mut reference_block = None;
        let mut discard_padding = None;

        while let Some(child) = it.next_header()? {
            match child.element_type() {
                MkvElement::DiscardPadding => {
                    // Non-mandatory element. No schema-defined default.
                    discard_padding = Some(SignedMatroskaTicks::from(it.read_i64_no_default()?));
                }
                MkvElement::Block => {
                    // Mandatory element.
                    data = Some(it.read_binary()?);
                }
                MkvElement::BlockDuration => {
                    // Non-mandatory element. Schema-defined default is TBD.
                    block_duration = it.read_u64()?.map(TrackTicks);
                }
                MkvElement::ReferenceBlock => {
                    // Non-mandatory element. No schema-defined default.
                    reference_block = Some(it.read_i64_no_default()?);
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        Ok(Self {
            data: data.ok_or(EbmlError::ElementError("mkv: missing block inside block group"))?,
            duration: block_duration,
            reference_block,
            discard_padding,
        })
    }
}

#[derive(Debug)]
pub(crate) struct TagsElement {
    pub(crate) tags: Box<[TagElement]>,
}

impl EbmlElement<MkvSchema> for TagsElement {
    const TYPE: MkvElement = MkvElement::Tags;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut tags = Vec::new();

        while let Some(header) = it.next_header()? {
            match header.element_type() {
                MkvElement::Tag => {
                    tags.push(it.read_master_element::<TagElement>()?);
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        Ok(Self { tags: tags.into_boxed_slice() })
    }
}

/// Map of a target-specific UID to a vector of tags.
pub type TargetTagsMap = HashMap<TargetUid, Vec<Tag>>;

impl TagsElement {
    pub(crate) fn into_metadata(
        mut self,
        target_tags: &mut TargetTagsMap,
        is_video: bool,
    ) -> MetadataRevision {
        /// UID -> (Last tag context, Tag vector)
        type UidMap = HashMap<u64, (TagContext, Vec<Tag>)>;

        /// Append tags to the item in the UID map for the given UID.
        fn append_to_map(map: &mut UidMap, uid: u64, raw_tags: &Vec<RawTag>, target: &Target) {
            if let Some(item) = map.get_mut(&uid) {
                // Attempt to generate a standard tag for each raw tag using the last context for
                // the current item, and push a new tag.
                for raw in raw_tags {
                    item.1.push(Tag { raw: raw.clone(), std: map_std_tag(raw, &item.0) });
                }
                // Update the last target of the context for the given item. Note, this is a cheap
                // clone (internal RC).
                item.0.target = Some(target.clone());
            }
        }

        /// Append tags to all items in the UID map.
        fn append_to_map_all(map: &mut UidMap, raw_tags: &Vec<RawTag>, target: &Target) {
            for item in map.values_mut() {
                // Attempt to generate a standard tag for each raw tag using the last context for
                // the current item, and push a new tag.
                for raw in raw_tags {
                    item.1.push(Tag { raw: raw.clone(), std: map_std_tag(raw, &item.0) });
                }
                // Update the last target of the context for the given item. Note, this is a cheap
                // clone (internal RC).
                item.0.target = Some(target.clone());
            }
        }

        /// Append tags to media.
        fn append_to_media(
            builder: &mut MetadataBuilder,
            raw_tags: &Vec<RawTag>,
            target: &Option<Target>,
            ctx: &mut TagContext,
        ) {
            // Attempt to generate a standard tag for each raw tag using the last media context, and
            // push a new tag.
            for raw in raw_tags {
                builder.add_tag(Tag { raw: raw.clone(), std: map_std_tag(raw, ctx) });
            }
            // Update the last target of the media tag context. Note, this is a cheap clone
            // (internal RC).
            ctx.target = target.clone();
        }

        let mut builder = MetadataBuilder::new(MKV_METADATA_INFO);
        let mut media_target = TagContext { is_video, target: None };

        let mut tracks: UidMap = Default::default();
        let mut editions: UidMap = Default::default();
        let mut chapters: UidMap = Default::default();
        let mut attachments: UidMap = Default::default();

        // Pre-populate maps using known track, edition, chapter, and attachment UIDs.
        for uid in target_tags.keys() {
            let default = (media_target.clone(), Vec::new());
            match uid {
                TargetUid::Track(uid) => tracks.insert(*uid, default),
                TargetUid::Edition(uid) => editions.insert(*uid, default),
                TargetUid::Chapter(uid) => chapters.insert(*uid, default),
                TargetUid::Attachment(uid) => attachments.insert(*uid, default),
            };
        }

        // Sort all tag elements in order of ascending target level. Tag elements without a target
        // or target level should be last. This sort is stable so tag elements at the same target
        // level will be in the same relative position as they were read.
        self.tags.sort_by_key(|tag| {
            tag.targets.as_ref().map(|targets| targets.target_type_value).unwrap_or(u64::MAX)
        });

        for tag in self.tags {
            // Tag context for the current tag element.
            let ctx = TagContext {
                is_video,
                target: tag.targets.as_ref().map(|t| Target {
                    value: t.target_type_value,
                    name: t.target_type.clone().map(Rc::new),
                }),
            };

            // Generate a vector of raw tags from simple tag elements. This vector will be appended
            // to the appropriate targets or media.
            let mut raw_tags = Vec::with_capacity(tag.simple_tags.len());

            for tag in tag.simple_tags {
                make_raw_tags(tag, &ctx, &mut raw_tags);
            }

            // Append tags to targets.
            if let Some(targets) = tag.targets {
                if !targets.uids.is_empty() {
                    let target = ctx.target.expect("ctx.target is Some when tag.targets is Some");

                    // Append tags to specific to tracks, editions, chapters, or attachments.
                    for uid in targets.uids {
                        match uid {
                            TargetUid::Track(uid) if !targets.all_tracks => {
                                append_to_map(&mut tracks, uid, &raw_tags, &target);
                            }
                            TargetUid::Edition(uid) if !targets.all_editions => {
                                append_to_map(&mut editions, uid, &raw_tags, &target);
                            }
                            TargetUid::Chapter(uid) if !targets.all_chapters => {
                                append_to_map(&mut chapters, uid, &raw_tags, &target);
                            }
                            TargetUid::Attachment(uid) if !targets.all_attachments => {
                                append_to_map(&mut attachments, uid, &raw_tags, &target);
                            }
                            _ => (),
                        }
                    }

                    // Append tags to all tracks.
                    if targets.all_tracks {
                        append_to_map_all(&mut tracks, &raw_tags, &target);
                    }
                    // Append tags to all editions.
                    if targets.all_editions {
                        append_to_map_all(&mut editions, &raw_tags, &target);
                    }
                    // Append tags to all chapters.
                    if targets.all_chapters {
                        append_to_map_all(&mut chapters, &raw_tags, &target);
                    }
                    // Append tags to all attachments.
                    if targets.all_attachments {
                        append_to_map_all(&mut attachments, &raw_tags, &target);
                    }
                }
                else {
                    // No target UID(s). Append tags to the entire media.
                    append_to_media(&mut builder, &raw_tags, &ctx.target, &mut media_target);
                }
            }
            else {
                // No targets. Append tags to the entire media.
                append_to_media(&mut builder, &raw_tags, &None, &mut media_target);
            }
        }

        // SAFETY: There needs to be sane limits (e.g., 1024 each category).

        // Return track target tags.
        for (uid, tags) in tracks {
            let mut track_builder = PerTrackMetadataBuilder::new(uid);
            for tag in tags.1 {
                track_builder.add_tag(tag);
            }
            builder.add_track(track_builder.build());
        }
        // Return edition target tags.
        for (uid, mut value) in editions {
            let tags = target_tags.entry(TargetUid::Edition(uid)).or_default();
            tags.append(&mut value.1);
        }
        // Return chapter target tags.
        for (uid, mut value) in chapters {
            let tags = target_tags.entry(TargetUid::Chapter(uid)).or_default();
            tags.append(&mut value.1);
        }
        // Return attachment target tags.
        for (uid, mut value) in attachments {
            let tags = target_tags.entry(TargetUid::Attachment(uid)).or_default();
            tags.append(&mut value.1);
        }

        // Return media and track-level metadata.
        builder.build()
    }
}

#[derive(Debug)]
pub(crate) struct TagElement {
    pub(crate) simple_tags: Box<[SimpleTagElement]>,
    pub(crate) targets: Option<TargetsElement>,
}

impl EbmlElement<MkvSchema> for TagElement {
    const TYPE: MkvElement = MkvElement::Tag;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut simple_tags = Vec::new();
        let mut targets = None;

        while let Some(header) = it.next_header()? {
            match header.element_type() {
                MkvElement::Targets => {
                    targets = Some(it.read_master_element::<TargetsElement>()?);
                }
                MkvElement::SimpleTag => {
                    simple_tags.push(it.read_master_element::<SimpleTagElement>()?);
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        Ok(Self { simple_tags: simple_tags.into_boxed_slice(), targets })
    }
}

#[derive(Debug, Hash, PartialEq, Eq)]
pub(crate) enum TargetUid {
    Track(u64),
    Edition(u64),
    Chapter(u64),
    Attachment(u64),
}

#[derive(Debug)]
pub(crate) struct TargetsElement {
    pub(crate) target_type_value: u64,
    pub(crate) target_type: Option<Box<str>>,
    pub(crate) uids: Vec<TargetUid>,
    pub(crate) all_tracks: bool,
    pub(crate) all_editions: bool,
    pub(crate) all_chapters: bool,
    pub(crate) all_attachments: bool,
}

impl EbmlElement<MkvSchema> for TargetsElement {
    const TYPE: MkvElement = MkvElement::Targets;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut target_type_value = None;
        let mut target_type = None;
        let mut uids = Vec::new();
        let mut all_tracks = false;
        let mut all_editions = false;
        let mut all_chapters = false;
        let mut all_attachments = false;

        while let Some(header) = it.next_header()? {
            match header.element_type() {
                MkvElement::TargetTypeValue => {
                    // Mandatory element. Schema-defined default is 50.
                    target_type_value = it.read_u64()?;
                }
                MkvElement::TargetType => {
                    // Non-mandatory element. No schema-defined default.
                    target_type = Some(it.read_string_no_default()?);
                }
                MkvElement::TagTrackUid => {
                    // Non-mandatory element. Schema-defined default is 0.
                    let uid = it.read_u64_default(0)?;
                    uids.push(TargetUid::Track(uid));
                    // If the UID is 0, then all tracks are targets.
                    if uid == 0 {
                        all_tracks = true;
                    }
                }
                MkvElement::TagEditionUid => {
                    // Non-mandatory element. Schema-defined default is 0.
                    let uid = it.read_u64_default(0)?;
                    uids.push(TargetUid::Edition(uid));
                    // If the UID is 0, then all editions are targets.
                    if uid == 0 {
                        all_editions = true;
                    }
                }
                MkvElement::TagChapterUid => {
                    // Non-mandatory element. Schema-defined default is 0.
                    let uid = it.read_u64_default(0)?;
                    uids.push(TargetUid::Chapter(uid));
                    // If the UID is 0, then all chapters are targets.
                    if uid == 0 {
                        all_chapters = true;
                    }
                }
                MkvElement::TagAttachmentUid => {
                    // Non-mandatory element. Schema-defined default is 0.
                    let uid = it.read_u64_default(0)?;
                    uids.push(TargetUid::Attachment(uid));
                    // If the UID is 0, then all attachments are targets.
                    if uid == 0 {
                        all_attachments = true;
                    }
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        // Populate missing or empty mandatory elements with defaults.
        let target_type_value = target_type_value.unwrap_or(50);

        Ok(Self {
            target_type_value,
            target_type: target_type.map(|t| t.into_boxed_str()),
            uids,
            all_tracks,
            all_editions,
            all_chapters,
            all_attachments,
        })
    }
}

#[derive(Debug)]
pub(crate) struct SimpleTagElement {
    pub(crate) name: Box<str>,
    pub(crate) value: Option<RawValue>,
    #[allow(dead_code)]
    pub(crate) is_default: bool,
    pub(crate) lang: Option<String>,
    pub(crate) lang_bcp47: Option<String>,
    pub(crate) sub_tags: Vec<SimpleTagElement>,
}

impl EbmlElement<MkvSchema> for SimpleTagElement {
    const TYPE: MkvElement = MkvElement::SimpleTag;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut name = None;
        let mut value = None;
        let mut lang = None;
        let mut lang_bcp47 = None;
        let mut is_default = true;
        let mut sub_tags = Vec::new();

        while let Some(header) = it.next_header()? {
            match header.element_type() {
                MkvElement::TagName => {
                    // Mandatory element. No schema-defined default.
                    name = Some(it.read_string_no_default()?);
                }
                MkvElement::TagLanguage => {
                    // Mandatory element. Schema-defined default is "und", however, treat it as
                    // optional.
                    lang = it.read_string()?;
                }
                MkvElement::TagString => {
                    // Non-mandatory element. No schema-defined default.
                    value = Some(RawValue::String(Arc::new(it.read_string_no_default()?)));
                }
                MkvElement::TagBinary => {
                    // Non-mandatory element. No schema-defined default.
                    value = Some(RawValue::Binary(Arc::new(it.read_binary()?)))
                }
                MkvElement::TagLanguageBcp47 => {
                    // Non-mandatory element. No schema-defined default.
                    lang_bcp47 = Some(it.read_string_no_default()?);
                }
                MkvElement::TagDefault => {
                    // Mandatory element. Schema-defined default value is set.
                    is_default = it.read_u64_default(1)? == 1;
                }
                MkvElement::SimpleTag => {
                    // Simple tag elements exist at a depth >= 3. Only support 3 levels of nesting
                    // as this is enough to support Matroska's standardized tagging scheme.
                    if hdr.depth() < 6 {
                        sub_tags.push(it.read_master_element::<SimpleTagElement>()?);
                    }
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        Ok(Self {
            name: name.ok_or(EbmlError::ElementError("mkv: missing tag name"))?.into_boxed_str(),
            value,
            lang,
            lang_bcp47,
            is_default,
            sub_tags,
        })
    }
}

#[derive(Debug)]
pub(crate) struct AttachedFileElement {
    pub(crate) uid: NonZeroU64,
    pub(crate) name: String,
    pub(crate) desc: Option<String>,
    pub(crate) media_type: String,
    pub(crate) data: Box<[u8]>,
}

impl EbmlElement<MkvSchema> for AttachedFileElement {
    const TYPE: MkvElement = MkvElement::AttachedFile;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut uid = None;
        let mut name = None;
        let mut desc = None;
        let mut media_type = None;
        let mut data = None;

        while let Some(header) = it.next_header()? {
            match header.element_type() {
                MkvElement::FileDescription => {
                    // Non-mandatory element. No schema-defined default.
                    desc = Some(it.read_string_no_default()?);
                }
                MkvElement::FileName => {
                    // Mandatory element. No schema-defined default.
                    name = Some(it.read_string_no_default()?);
                }
                MkvElement::FileMediaType => {
                    // Mandatory element. No schema-defined default.
                    media_type = Some(it.read_string_no_default()?);
                }
                MkvElement::FileData => {
                    // Mandatory element. No schema-defined default.
                    data = Some(it.read_binary()?);
                }
                MkvElement::FileUid => {
                    // Mandatory element. May not be 0. No schema-defined default.
                    let val = NonZeroU64::new(it.read_u64_no_default()?)
                        .ok_or(EbmlError::ElementError("mkv: invalid file uid"))?;

                    uid = Some(val);
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        Ok(Self {
            uid: uid.ok_or(EbmlError::ElementError("mkv: missing attached file uid"))?,
            name: name.ok_or(EbmlError::ElementError("mkv: missing attached file name"))?,
            desc,
            media_type: media_type
                .ok_or(EbmlError::ElementError("mkv: missing attached file media-type"))?,
            data: data.ok_or(EbmlError::ElementError("mkv: missing attached file data"))?,
        })
    }
}

#[derive(Debug)]
pub(crate) struct AttachmentsElement {
    pub(crate) attached_files: Box<[AttachedFileElement]>,
}

impl EbmlElement<MkvSchema> for AttachmentsElement {
    const TYPE: MkvElement = MkvElement::Attachments;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut attached_files = Vec::new();

        while let Some(header) = it.next_header()? {
            match header.element_type() {
                MkvElement::AttachedFile => {
                    attached_files.push(it.read_master_element::<AttachedFileElement>()?);
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        Ok(Self { attached_files: attached_files.into_boxed_slice() })
    }
}

impl AttachmentsElement {
    pub(crate) fn get_target_uids(&self, target_tags: &mut TargetTagsMap) {
        self.attached_files.iter().for_each(|file| {
            target_tags.insert(TargetUid::Attachment(file.uid.get()), Default::default());
        })
    }

    pub(crate) fn into_attachments(self, _target_tags: &mut TargetTagsMap) -> Vec<Attachment> {
        self.attached_files
            .into_vec()
            .into_iter()
            .map(|file| {
                Attachment::File(FileAttachment {
                    name: file.name,
                    description: file.desc,
                    media_type: Some(file.media_type),
                    data: file.data,
                })
            })
            .collect()
    }
}

#[derive(Debug)]
pub(crate) struct ChaptersElement {
    pub(crate) editions: Box<[EditionEntryElement]>,
}

impl EbmlElement<MkvSchema> for ChaptersElement {
    const TYPE: MkvElement = MkvElement::Chapters;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut editions = Vec::new();

        while let Some(header) = it.next_header()? {
            match header.element_type() {
                MkvElement::EditionEntry => {
                    editions.push(it.read_master_element::<EditionEntryElement>()?);
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        Ok(Self { editions: editions.into_boxed_slice() })
    }
}

impl ChaptersElement {
    pub(crate) fn get_target_uids(&self, target_tags: &mut TargetTagsMap) {
        self.editions.iter().for_each(|edition| edition.get_target_uids(target_tags));
    }

    /// Builds a chapter group containing the chapters of the first non-hidden default edition, or,
    /// if one doesn't exist, the first non-hidden edition.
    pub(crate) fn into_chapter_group(
        self,
        target_tags: &mut TargetTagsMap,
    ) -> Option<ChapterGroup> {
        // Find the first non-hidden default edition. Or, the first non-hidden edition.
        let index = self
            .editions
            .iter()
            .position(|e| !e.is_hidden && e.is_default)
            .or_else(|| self.editions.iter().position(|e| !e.is_hidden));

        // Convert to chapter group.
        index.map(|index| {
            self.editions.into_vec().swap_remove(index).into_chapter_group(target_tags)
        })
    }
}

#[derive(Debug)]
pub(crate) struct EditionEntryElement {
    pub(crate) uid: NonZeroU64,
    pub(crate) is_hidden: bool,
    pub(crate) is_default: bool,
    #[allow(dead_code)]
    pub(crate) is_ordered: bool,
    pub(crate) display: Box<[EditionDisplayElement]>,
    pub(crate) chapters: Box<[ChapterAtomElement]>,
}

impl EbmlElement<MkvSchema> for EditionEntryElement {
    const TYPE: MkvElement = MkvElement::EditionEntry;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut uid = None;
        let mut is_hidden = false;
        let mut is_default = false;
        let mut is_ordered = false;
        let mut display = Vec::new();
        let mut chapters = Vec::new();

        while let Some(header) = it.next_header()? {
            match header.element_type() {
                MkvElement::EditionUid => {
                    // Mandatory element. Must not be 0.
                    let val = NonZeroU64::new(it.read_u64_no_default()?)
                        .ok_or(EbmlError::ElementError("mkv: invalid (0) edition uid"))?;

                    uid = Some(val);
                }
                MkvElement::EditionFlagHidden => {
                    is_hidden = it.read_u64_default(0)? == 1;
                }
                MkvElement::EditionFlagDefault => {
                    is_default = it.read_u64_default(0)? == 1;
                }
                MkvElement::EditionFlagOrdered => {
                    is_ordered = it.read_u64_default(0)? == 1;
                }
                MkvElement::EditionDisplay => {
                    display.push(it.read_master_element::<EditionDisplayElement>()?)
                }
                MkvElement::ChapterAtom => {
                    chapters.push(it.read_master_element::<ChapterAtomElement>()?)
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        Ok(Self {
            uid: uid.ok_or(EbmlError::ElementError("mkv: missing edition uid"))?,
            is_hidden,
            is_default,
            is_ordered,
            display: display.into_boxed_slice(),
            chapters: chapters.into_boxed_slice(),
        })
    }
}

impl EditionEntryElement {
    pub(crate) fn get_target_uids(&self, target_tags: &mut TargetTagsMap) {
        // Append edition UID.
        target_tags.insert(TargetUid::Edition(self.uid.get()), Default::default());
        // Append chapter UIDs.
        self.chapters.iter().for_each(|chapter| chapter.get_target_uids(target_tags));
    }

    pub(crate) fn into_chapter_group(self, target_tags: &mut TargetTagsMap) -> ChapterGroup {
        // Take the vector of tags associated with this edition, if any.
        let mut tags = target_tags
            .remove(&TargetUid::Edition(self.uid.get()))
            .unwrap_or_else(|| Vec::with_capacity(self.display.len()));

        // Edition title tags.
        for display in self.display {
            let mut sub_fields = Vec::with_capacity(1);

            // Edition language sub-field.
            if let Some(lang) = display.lang_bcp47 {
                // BCP47 language code is present.
                sub_fields.push(RawTagSubField::new(EDITION_TITLE_LANGUAGE_BCP47, lang));
            }

            let title = Arc::new(display.name);

            let raw = RawTag::new_with_sub_fields(
                "ChapString",
                title.clone(),
                sub_fields.into_boxed_slice(),
            );

            tags.push(Tag::new_std(raw, StandardTag::ChapterTitle(title)));
        }

        // Edition chapters.
        let mut items = Vec::with_capacity(self.chapters.len());

        for chapter in self.chapters {
            if !chapter.is_hidden {
                items.push(chapter.into_chapter_group_item(target_tags));
            }
        }

        ChapterGroup { items, tags, visuals: vec![] }
    }
}

#[derive(Debug)]
pub(crate) struct EditionDisplayElement {
    pub(crate) name: String,
    pub(crate) lang_bcp47: Option<String>,
}

impl EbmlElement<MkvSchema> for EditionDisplayElement {
    const TYPE: MkvElement = MkvElement::EditionDisplay;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut name = None;
        let mut lang_bcp47 = None;

        while let Some(header) = it.next_header()? {
            match header.element_type() {
                MkvElement::EditionString => {
                    // Mandatory element. No schema-defined default.
                    name = Some(it.read_string_no_default()?);
                }
                MkvElement::EditionLanguageIetf => {
                    // Non-mandatory element. No schema-defined default.
                    lang_bcp47 = Some(it.read_string_no_default()?);
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        Ok(Self {
            name: name.ok_or(EbmlError::ElementError("mkv: missing edition display name"))?,
            lang_bcp47,
        })
    }
}

#[derive(Debug)]
pub(crate) struct ChapterAtomElement {
    pub(crate) uid: NonZeroU64,
    #[allow(dead_code)]
    pub(crate) is_enabled: bool,
    pub(crate) is_hidden: bool,
    pub(crate) time_start: MatroskaTicks,
    pub(crate) time_end: Option<MatroskaTicks>,
    pub(crate) skip_type: Option<u8>,
    pub(crate) display: Box<[ChapterDisplayElement]>,
    pub(crate) chapters: Box<[ChapterAtomElement]>,
}

impl EbmlElement<MkvSchema> for ChapterAtomElement {
    const TYPE: MkvElement = MkvElement::ChapterAtom;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut uid = None;
        let mut is_enabled = false;
        let mut is_hidden = false;
        let mut time_start = None;
        let mut time_end = None;
        let mut skip_type = None;
        let mut display = Vec::new();
        let mut chapters = Vec::new();

        while let Some(header) = it.next_header()? {
            match header.element_type() {
                MkvElement::ChapterUid => {
                    // Non-mandatory element. Must not be 0. No schema-defined default.
                    let val = NonZeroU64::new(it.read_u64_no_default()?)
                        .ok_or(EbmlError::ElementError("mkv: invalid chapter uid"))?;

                    uid = Some(val);
                }
                MkvElement::ChapterStringUid => {}
                MkvElement::ChapterTimeStart => {
                    // Mandatory element. No schema-defined default.
                    time_start = Some(MatroskaTicks(it.read_u64_no_default()?));
                }
                MkvElement::ChapterTimeEnd => {
                    // Non-mandatory element. No schema-defined default.
                    time_end = Some(MatroskaTicks(it.read_u64_no_default()?));
                }
                MkvElement::ChapterFlagEnabled => {
                    // Mandatory element. Schema-defined default is 1 (set).
                    is_enabled = it.read_u64_default(1)? == 1;
                }
                MkvElement::ChapterFlagHidden => {
                    // Mandatory element. Schema-defined default is 0 (unset).
                    is_hidden = it.read_u64_default(0)? == 1;
                }
                MkvElement::ChapterDisplay => {
                    // Non-mandatory element.
                    display.push(it.read_master_element::<ChapterDisplayElement>()?);
                }
                MkvElement::ChapterSkipType => {
                    // Non-mandatory element. No schema-defined default.
                    skip_type = match it.read_u64_no_default()? {
                        value if value <= 6 => Some(value as u8),
                        _ => return Err(EbmlError::ElementError("mkv: invalid chapter skip type")),
                    };
                }
                MkvElement::ChapterAtom => {
                    chapters.push(it.read_master_element::<ChapterAtomElement>()?);
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        Ok(Self {
            uid: uid.ok_or(EbmlError::ElementError("mkv: missing chapter uid"))?,
            is_enabled,
            is_hidden,
            time_start: time_start
                .ok_or(EbmlError::ElementError("mkv: missing chapter atom time start"))?,
            time_end,
            skip_type,
            display: display.into_boxed_slice(),
            chapters: chapters.into_boxed_slice(),
        })
    }
}

impl ChapterAtomElement {
    pub(crate) fn get_target_uids(&self, target_tags: &mut TargetTagsMap) {
        // Append chapter UID.
        target_tags.insert(TargetUid::Chapter(self.uid.get()), Default::default());
        // Append chapter UIDs.
        self.chapters.iter().for_each(|chapter| chapter.get_target_uids(target_tags));
    }

    pub(crate) fn into_chapter_group_item(
        self,
        target_tags: &mut TargetTagsMap,
    ) -> ChapterGroupItem {
        // Take the vector of any tags associated with this chapter, if any.
        let mut tags = target_tags
            .remove(&TargetUid::Chapter(self.uid.get()))
            .unwrap_or_else(|| Vec::with_capacity(self.display.len()));

        // Chapter title tags.
        for display in self.display {
            let mut sub_fields = Vec::with_capacity(if display.country.is_some() { 2 } else { 1 });

            // Chapter language sub-field.
            if let Some(lang) = display.lang_bcp47 {
                // BCP47 language code is present, prefer it over the ISO 639-2 chapter language
                // and county elements.
                sub_fields.push(RawTagSubField::new(CHAPTER_TITLE_LANGUAGE_BCP47, lang));
            }
            else {
                // ISO 639-2 language code.
                sub_fields.push(RawTagSubField::new(CHAPTER_TITLE_LANGUAGE, display.lang));

                // Chapter country sub-field.
                if let Some(country) = display.country {
                    sub_fields.push(RawTagSubField::new(CHAPTER_TITLE_COUNTRY, country));
                }
            }

            let title = Arc::new(display.name);

            let raw = RawTag::new_with_sub_fields(
                "ChapString",
                title.clone(),
                sub_fields.into_boxed_slice(),
            );

            tags.push(Tag::new_std(raw, StandardTag::ChapterTitle(title)));
        }

        // Chapter skip-type tag.
        if let Some(skip_type) = self.skip_type {
            tags.push(Tag::new_from_parts("CHAPTER_SKIP_TYPE", skip_type, None));
        }

        let chapter = Chapter {
            start_time: Time::from_nanos_u64(self.time_start.get()),
            end_time: self.time_end.map(|t| Time::from_nanos_u64(t.get())),
            start_byte: None,
            end_byte: None,
            tags,
            visuals: vec![],
        };

        if self.chapters.is_empty() {
            // This chapter atom does not have nested chapters. Return a chapter item.
            ChapterGroupItem::Chapter(chapter)
        }
        else {
            // This chapter atom has nested chapters. Return a group containing 1 chapter
            // (this one), and a group of nested chapters.
            let mut items = Vec::with_capacity(self.chapters.len());

            for chapter in self.chapters {
                if !chapter.is_hidden {
                    items.push(chapter.into_chapter_group_item(target_tags));
                }
            }

            ChapterGroupItem::Group(
                // The parent chapter group.
                ChapterGroup {
                    items: vec![
                        // The parent chapter as the first item in the chapter group.
                        ChapterGroupItem::Chapter(chapter),
                        // The nested chapters as a chapter group as the second item.
                        ChapterGroupItem::Group(ChapterGroup {
                            items,
                            tags: vec![],
                            visuals: vec![],
                        }),
                    ],
                    tags: vec![],
                    visuals: vec![],
                },
            )
        }
    }
}

#[derive(Debug)]
pub(crate) struct ChapterDisplayElement {
    pub(crate) name: String,
    pub(crate) lang: String,
    pub(crate) lang_bcp47: Option<String>,
    pub(crate) country: Option<String>,
}

impl EbmlElement<MkvSchema> for ChapterDisplayElement {
    const TYPE: MkvElement = MkvElement::ChapterDisplay;

    fn read<R: ReadEbml>(it: &mut MkvEbmlIterator<R>, hdr: &MkvEbmlElementHeader) -> Result<Self> {
        let mut name = None;
        let mut lang = None;
        let mut lang_bcp47 = None;
        let mut country = None;

        while let Some(header) = it.next_header()? {
            match header.element_type() {
                MkvElement::ChapString => {
                    // Mandatory element. No schema-defined default.
                    name = Some(it.read_string_no_default()?);
                }
                MkvElement::ChapLanguage => {
                    // Mandatory element. Schema-defined default is "eng".
                    lang = it.read_string()?;
                }
                MkvElement::ChapLanguageBcp47 => {
                    // Non-mandatory element. No schema-defined default.
                    lang_bcp47 = Some(it.read_string_no_default()?);
                }
                MkvElement::ChapCountry => {
                    // Non-mandatory element. No schema-defined default.
                    country = Some(it.read_string_no_default()?);
                }
                other => {
                    // Unexpected child element.
                    log::debug!("ignored {:?} child {:?}", hdr.element_type(), other);
                }
            }
        }

        // Populate missing or empty mandatory elements with defaults.
        let lang = lang.unwrap_or_else(|| "eng".into());

        Ok(Self {
            name: name.ok_or(EbmlError::ElementError("mkv: missing chapter display name"))?,
            lang,
            lang_bcp47,
            country,
        })
    }
}
