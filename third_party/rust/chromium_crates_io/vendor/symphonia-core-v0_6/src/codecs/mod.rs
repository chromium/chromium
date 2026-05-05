// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The `codec` module and its sub-modules provides traits and supporting infrastructure to
//! implement audio, video, and subtitle decoders.
//!
//! # Nomenclature
//!
//! * A codec ID refers to a unique identifier for a specific codec.
//! * A codec type refers to the type of media the codec is encoding: audio, video, subtitles,
//!   etc.
//! * Codec parameters refers to a set of parameters common to a particular codec type (e.g.,
//!   audio sample rate).

use std::hash::Hash;

pub mod audio;
pub mod registry;
pub mod subtitle;
pub mod video;

use crate::codecs::audio::{AudioCodecId, AudioCodecParameters};
use crate::codecs::subtitle::{SubtitleCodecId, SubtitleCodecParameters};
use crate::codecs::video::{VideoCodecId, VideoCodecParameters};

/// A codec-specific identification code for a profile.
///
/// In general, codec profiles are designed to target specific applications, and define a set of
/// minimum capabilities a decoder must implement to successfully decode a bitstream. For an
/// encoder, a profile imposes a set of constraints upon the bitstream it produces.
#[repr(transparent)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct CodecProfile(u32);

impl CodecProfile {
    /// Create a new codec profile from a profile code.
    pub const fn new(code: u32) -> Self {
        Self(code)
    }

    /// Get the profile code.
    pub const fn get(&self) -> u32 {
        self.0
    }
}

impl From<u32> for CodecProfile {
    fn from(value: u32) -> Self {
        Self(value)
    }
}

/// Basic information about a codec profile.
#[derive(Copy, Clone, Debug)]
pub struct CodecProfileInfo {
    /// The codec profile.
    pub profile: CodecProfile,
    /// A short ASCII-only string identifying the codec profile.
    pub short_name: &'static str,
    /// A longer, more descriptive, string identifying the codec profile
    pub long_name: &'static str,
}

/// Basic information about a codec.
#[derive(Copy, Clone, Debug)]
pub struct CodecInfo {
    /// A short ASCII-only string identifying the codec.
    pub short_name: &'static str,
    /// A longer, more descriptive, string identifying the codec.
    pub long_name: &'static str,
    /// A list of codec profiles.
    pub profiles: &'static [CodecProfileInfo],
}

/// Generic wrapper around type-specific codec parameters.
#[non_exhaustive]
#[derive(Clone, Debug)]
pub enum CodecParameters {
    /// Codec parameters for an audio codec.
    Audio(AudioCodecParameters),
    /// Codec parameters for a video codec.
    Video(VideoCodecParameters),
    /// Codec parameters for a subtitle codec.
    Subtitle(SubtitleCodecParameters),
}

impl CodecParameters {
    /// If the codec parameters are for an audio codec, returns an immutable reference to the
    /// contained audio codec parameters. Otherwise, returns `None`.
    pub fn audio(&self) -> Option<&AudioCodecParameters> {
        match self {
            CodecParameters::Audio(params) => Some(params),
            _ => None,
        }
    }

    /// If the codec parameters are for an audio codec, returns a mutable reference to the
    /// contained audio codec parameters. Otherwise, returns `None`.
    pub fn audio_mut(&mut self) -> Option<&mut AudioCodecParameters> {
        match self {
            CodecParameters::Audio(params) => Some(params),
            _ => None,
        }
    }

    /// If the codec parameters are for an video codec, returns an immutable reference to the
    /// contained video codec parameters. Otherwise, returns `None`.
    pub fn video(&self) -> Option<&VideoCodecParameters> {
        match self {
            CodecParameters::Video(params) => Some(params),
            _ => None,
        }
    }

    /// If the codec parameters are for an video codec, returns a mutable reference to the
    /// contained video codec parameters. Otherwise, returns `None`.
    pub fn video_mut(&mut self) -> Option<&mut VideoCodecParameters> {
        match self {
            CodecParameters::Video(params) => Some(params),
            _ => None,
        }
    }

    /// If the codec parameters are for an subtitle codec, returns an immutable reference to the
    /// contained subtitle codec parameters. Otherwise, returns `None`.
    pub fn subtitle(&self) -> Option<&SubtitleCodecParameters> {
        match self {
            CodecParameters::Subtitle(params) => Some(params),
            _ => None,
        }
    }

    /// If the codec parameters are for an subtitle codec, returns a mutable reference to the
    /// contained subtitle codec parameters. Otherwise, returns `None`.
    pub fn subtitle_mut(&mut self) -> Option<&mut SubtitleCodecParameters> {
        match self {
            CodecParameters::Subtitle(params) => Some(params),
            _ => None,
        }
    }
}

impl From<AudioCodecParameters> for CodecParameters {
    fn from(value: AudioCodecParameters) -> Self {
        CodecParameters::Audio(value)
    }
}

impl From<VideoCodecParameters> for CodecParameters {
    fn from(value: VideoCodecParameters) -> Self {
        CodecParameters::Video(value)
    }
}

impl From<SubtitleCodecParameters> for CodecParameters {
    fn from(value: SubtitleCodecParameters) -> Self {
        CodecParameters::Subtitle(value)
    }
}

/// Generic wrapper around type-specific codec IDs.
#[non_exhaustive]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub enum CodecId {
    /// Codec ID for an audio codec.
    Audio(AudioCodecId),
    /// Codec ID for a video codec.
    Video(VideoCodecId),
    /// Codec ID for a subtitle codec.
    Subtitle(SubtitleCodecId),
}

impl From<AudioCodecId> for CodecId {
    fn from(value: AudioCodecId) -> Self {
        CodecId::Audio(value)
    }
}

impl From<VideoCodecId> for CodecId {
    fn from(value: VideoCodecId) -> Self {
        CodecId::Video(value)
    }
}

impl From<SubtitleCodecId> for CodecId {
    fn from(value: SubtitleCodecId) -> Self {
        CodecId::Subtitle(value)
    }
}
