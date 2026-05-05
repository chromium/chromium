// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Registry for codecs to support lookup and instantiation of decoders dynamically at runtime.

use std::collections::HashMap;
use std::default::Default;
use std::hash::Hash;

use crate::codecs::CodecInfo;
use crate::codecs::audio::{AudioCodecId, AudioCodecParameters, AudioDecoder, AudioDecoderOptions};
#[cfg(feature = "exp-subtitle-codecs")]
use crate::codecs::subtitle::{
    SubtitleCodecId, SubtitleCodecParameters, SubtitleDecoder, SubtitleDecoderOptions,
};
#[cfg(feature = "exp-video-codecs")]
use crate::codecs::video::{VideoCodecId, VideoCodecParameters, VideoDecoder, VideoDecoderOptions};
use crate::common::Tier;
use crate::errors::{Result, unsupported_error};

/// Description of a supported audio codec.
#[derive(Copy, Clone)]
pub struct SupportedAudioCodec {
    pub id: AudioCodecId,
    pub info: CodecInfo,
}

/// To support registration in a codec registry, an `AudioDecoder` must implement the
/// `RegisterableAudioDecoder` trait.
pub trait RegisterableAudioDecoder: AudioDecoder {
    fn try_registry_new(
        params: &AudioCodecParameters,
        opts: &AudioDecoderOptions,
    ) -> Result<Box<dyn AudioDecoder>>
    where
        Self: Sized;

    /// Get a list of audio codecs supported by this decoder.
    fn supported_codecs() -> &'static [SupportedAudioCodec];
}

/// Description of a supported video codec.
#[cfg(feature = "exp-video-codecs")]
#[derive(Copy, Clone)]
pub struct SupportedVideoCodec {
    pub codec: VideoCodecId,
    pub info: CodecInfo,
}

/// To support registration in a codec registry, a `VideoDecoder` must implement the
/// `RegisterableVideoDecoder` trait.
#[cfg(feature = "exp-video-codecs")]
pub trait RegisterableVideoDecoder: VideoDecoder {
    fn try_registry_new(
        params: &VideoCodecParameters,
        opts: &VideoDecoderOptions,
    ) -> Result<Box<dyn VideoDecoder>>
    where
        Self: Sized;

    /// Get a list of video codecs supported by this decoder.
    fn supported_codecs() -> &'static [SupportedVideoCodec];
}

/// Description of a supported subtitle codec.
#[cfg(feature = "exp-subtitle-codecs")]
#[derive(Copy, Clone)]
pub struct SupportedSubtitleCodec {
    pub codec: SubtitleCodecId,
    pub info: CodecInfo,
}

/// To support registration in a codec registry, a `SubtitleDecoder` must implement the
/// `RegisterableSubtitleDecoder` trait.
#[cfg(feature = "exp-subtitle-codecs")]
pub trait RegisterableSubtitleDecoded: SubtitleDecoder {
    fn try_registry_new(
        params: &SubtitleCodecParameters,
        opts: &SubtitleDecoderOptions,
    ) -> Result<Box<dyn SubtitleDecoder>>
    where
        Self: Sized;

    /// Get a list of subtitle codecs supported by this decoder.
    fn supported_codecs() -> &'static [SupportedSubtitleCodec];
}

/// `AudioDecoder` factory function. Creates a boxed `AudioDecoder`.
pub type AudioDecoderFactoryFn =
    fn(&AudioCodecParameters, &AudioDecoderOptions) -> Result<Box<dyn AudioDecoder>>;

/// `VideoDecoder` factory function. Creates a boxed `VideoDecoder`.
#[cfg(feature = "exp-video-codecs")]
pub type VideoDecoderFactoryFn =
    fn(&VideoCodecParameters, &VideoDecoderOptions) -> Result<Box<dyn VideoDecoder>>;

/// `SubtitleDecoder` factory function. Creates a boxed `SubtitleDecoder`.
#[cfg(feature = "exp-subtitle-codecs")]
pub type SubtitleDecoderFactoryFn =
    fn(&SubtitleCodecParameters, &SubtitleDecoderOptions) -> Result<Box<dyn SubtitleDecoder>>;

/// Registration details of an audio decoder for a particular audio codec.
pub struct RegisteredAudioDecoder {
    /// Audio codec details.
    pub codec: SupportedAudioCodec,
    /// Factory function to instantiate the audio decoder.
    pub factory: AudioDecoderFactoryFn,
}

/// Registration details of a video decoder for a particular video codec.
#[cfg(feature = "exp-video-codecs")]
pub struct RegisteredVideoDecoder {
    /// Video codec details.
    pub codec: SupportedVideoCodec,
    /// Factory function to instantiate the video decoder.
    pub factory: VideoDecoderFactoryFn,
}

/// Registration details of a subtitle decoder for a particular subtitle codec.
#[cfg(feature = "exp-subtitle-codecs")]
pub struct RegisteredSubtitleDecoder {
    /// Subtitle codec details.
    pub codec: SupportedSubtitleCodec,
    /// Factory function to instantiate the subtitle decoder.
    pub factory: SubtitleDecoderFactoryFn,
}

struct InnerCodecRegistry<C, R> {
    preferred: HashMap<C, R>,
    standard: HashMap<C, R>,
    fallback: HashMap<C, R>,
}

impl<C, R> Default for InnerCodecRegistry<C, R> {
    fn default() -> Self {
        Self {
            preferred: Default::default(),
            standard: Default::default(),
            fallback: Default::default(),
        }
    }
}

impl<C, R> InnerCodecRegistry<C, R>
where
    C: Hash + std::cmp::Eq,
{
    fn get(&self, id: &C) -> Option<&R> {
        self.preferred.get(id).or_else(|| self.standard.get(id)).or_else(|| self.fallback.get(id))
    }

    fn get_at_tier(&self, tier: Tier, id: &C) -> Option<&R> {
        match tier {
            Tier::Preferred => self.preferred.get(id),
            Tier::Standard => self.standard.get(id),
            Tier::Fallback => self.fallback.get(id),
        }
    }

    fn register_at_tier(&mut self, tier: Tier, id: C, reg: R) -> Option<R> {
        match tier {
            Tier::Preferred => self.preferred.insert(id, reg),
            Tier::Standard => self.standard.insert(id, reg),
            Tier::Fallback => self.fallback.insert(id, reg),
        }
    }
}

/// A `CodecRegistry` allows the registration of codecs, and provides a method to instantiate a
/// `Decoder` given a `CodecParameters` object.
#[derive(Default)]
pub struct CodecRegistry {
    audio: InnerCodecRegistry<AudioCodecId, RegisteredAudioDecoder>,
    #[cfg(feature = "exp-video-codecs")]
    video: InnerCodecRegistry<VideoCodecId, RegisteredVideoDecoder>,
    #[cfg(feature = "exp-subtitle-codecs")]
    subtitle: InnerCodecRegistry<SubtitleCodecId, RegisteredSubtitleDecoder>,
}

impl CodecRegistry {
    /// Instantiate a new `CodecRegistry`.
    pub fn new() -> Self {
        CodecRegistry {
            audio: Default::default(),
            #[cfg(feature = "exp-video-codecs")]
            video: Default::default(),
            #[cfg(feature = "exp-subtitle-codecs")]
            subtitle: Default::default(),
        }
    }

    /// Get the registration information of the most preferred audio decoder for the specified
    /// audio codec.
    pub fn get_audio_decoder(&self, id: AudioCodecId) -> Option<&RegisteredAudioDecoder> {
        self.audio.get(&id)
    }

    /// Get the registration information of the audio decoder at the specified tier for the
    /// specified audio codec.
    pub fn get_audio_decoder_at_tier(
        &self,
        tier: Tier,
        id: AudioCodecId,
    ) -> Option<&RegisteredAudioDecoder> {
        self.audio.get_at_tier(tier, &id)
    }

    /// Get the registration information of the most preferred video decoder for the specified
    /// video codec.
    #[cfg(feature = "exp-video-codecs")]
    pub fn get_video_decoder(&self, id: VideoCodecId) -> Option<&RegisteredVideoDecoder> {
        self.video.get(&id)
    }

    /// Get the registration information of the video decoder at the specified tier for the
    /// specified video codec.
    #[cfg(feature = "exp-video-codecs")]
    pub fn get_video_decoder_at_tier(
        &self,
        tier: Tier,
        id: VideoCodecId,
    ) -> Option<&RegisteredVideoDecoder> {
        self.video.get_at_tier(tier, &id)
    }

    /// Get the registration information of the most preferred subtitle decoder for the specified
    /// subtitle codec.
    #[cfg(feature = "exp-subtitle-codecs")]
    pub fn get_subtitle_decoder(&self, id: SubtitleCodecId) -> Option<&RegisteredSubtitleDecoder> {
        self.subtitle.get(&id)
    }

    /// Get the registration information of the subtitle decoder at the specified tier for the
    /// specified subtitle codec.
    #[cfg(feature = "exp-subtitle-codecs")]
    pub fn get_subtitle_decoder_at_tier(
        &self,
        tier: Tier,
        id: SubtitleCodecId,
    ) -> Option<&RegisteredSubtitleDecoder> {
        self.subtitle.get_at_tier(tier, &id)
    }

    /// Registers all audio codecs supported by the audio decoder at the standard tier.
    ///
    /// If a supported audio codec was previously registered by another audio decoder at the same
    /// tier, it will be replaced within the registry.
    pub fn register_audio_decoder<C: RegisterableAudioDecoder>(&mut self) {
        self.register_audio_decoder_at_tier::<C>(Tier::Standard);
    }

    /// Registers all audio codecs supported by the audio decoder at a specific tier.
    ///
    /// If a supported codec was previously registered by another audio decoder at the same tier, it
    /// will be replaced within the registry.
    pub fn register_audio_decoder_at_tier<C: RegisterableAudioDecoder>(&mut self, tier: Tier) {
        for codec in C::supported_codecs() {
            let reg = RegisteredAudioDecoder {
                codec: *codec,
                factory: |params, opts| C::try_registry_new(params, opts),
            };

            self.audio.register_at_tier(tier, codec.id, reg);
        }
    }

    /// Registers all video codecs supported by the video decoder at the standard tier.
    ///
    /// If a supported video codec was previously registered by another video decoder at the same
    /// tier, it will be replaced within the registry.
    #[cfg(feature = "exp-video-codecs")]
    pub fn register_video_decoder<C: RegisterableVideoDecoder>(&mut self) {
        self.register_video_decoder_at_tier::<C>(Tier::Standard);
    }

    /// Registers all video codecs supported by the video decoder at a specific tier.
    ///
    /// If a supported codec was previously registered by another video decoder at the same tier, it
    /// will be replaced within the registry.
    #[cfg(feature = "exp-video-codecs")]
    pub fn register_video_decoder_at_tier<C: RegisterableVideoDecoder>(&mut self, tier: Tier) {
        for codec in C::supported_codecs() {
            let reg = RegisteredVideoDecoder {
                codec: *codec,
                factory: |params, opts| C::try_registry_new(params, opts),
            };

            self.video.register_at_tier(tier, codec.codec, reg);
        }
    }

    /// Registers all subtitle codecs supported by the subtitle decoder at the standard tier.
    ///
    /// If a supported subtitle codec was previously registered by another subtitle decoder at the
    /// same tier, it will be replaced within the registry.
    #[cfg(feature = "exp-subtitle-codecs")]
    pub fn register_subtitle_decoder<C: RegisterableSubtitleDecoded>(&mut self) {
        self.register_subtitle_decoder_at_tier::<C>(Tier::Standard);
    }

    /// Registers all subtitle codecs supported by the subtitle decoder at a specific tier.
    ///
    /// If a supported codec was previously registered by another subtitle decoder at the same tier,
    /// it will be replaced within the registry.
    #[cfg(feature = "exp-subtitle-codecs")]
    pub fn register_subtitle_decoder_at_tier<C: RegisterableSubtitleDecoded>(
        &mut self,
        tier: Tier,
    ) {
        for codec in C::supported_codecs() {
            let reg = RegisteredSubtitleDecoder {
                codec: *codec,
                factory: |params, opts| C::try_registry_new(params, opts),
            };

            self.subtitle.register_at_tier(tier, codec.codec, reg);
        }
    }

    /// Instantiate an audio decoder for the specified audio codec parameters.
    ///
    /// This function searches the registry for an audio decoder that supports the codec. If one is
    /// found, it will be instantiated with the provided audio codec parameters and audio decoder
    /// options. If a suitable decoder could not be found, or the decoder could not be instantiated,
    /// an error will be returned.
    pub fn make_audio_decoder(
        &self,
        params: &AudioCodecParameters,
        opts: &AudioDecoderOptions,
    ) -> Result<Box<dyn AudioDecoder>> {
        if let Some(codec) = self.get_audio_decoder(params.codec) {
            Ok((codec.factory)(params, opts)?)
        }
        else {
            unsupported_error("core (codec): unsupported audio codec")
        }
    }

    /// Instantiate a video decoder for the specified audio codec parameters.
    ///
    /// This function searches the registry for a video decoder that supports the codec. If one is
    /// found, it will be instantiated with the provided video codec parameters and video decoder
    /// options. If a suitable decoder could not be found, or the decoder could not be instantiated,
    /// an error will be returned.
    #[cfg(feature = "exp-video-codecs")]
    pub fn make_video_decoder(
        &self,
        params: &VideoCodecParameters,
        opts: &VideoDecoderOptions,
    ) -> Result<Box<dyn VideoDecoder>> {
        if let Some(codec) = self.get_video_decoder(params.codec) {
            Ok((codec.factory)(params, opts)?)
        }
        else {
            unsupported_error("core (codec): unsupported video codec")
        }
    }

    /// Instantiate a subtitle decoder for the specified audio codec parameters.
    ///
    /// This function searches the registry for a subtitle decoder that supports the codec. If one
    /// is found, it will be instantiated with the provided subtitle codec parameters and subtitle
    /// decoder options. If a suitable decoder could not be found, or the decoder could not be
    /// instantiated, an error will be returned.
    #[cfg(feature = "exp-subtitle-codecs")]
    pub fn make_subtitle_decoder(
        &self,
        params: &SubtitleCodecParameters,
        opts: &SubtitleDecoderOptions,
    ) -> Result<Box<dyn SubtitleDecoder>> {
        if let Some(codec) = self.get_subtitle_decoder(params.codec) {
            Ok((codec.factory)(params, opts)?)
        }
        else {
            unsupported_error("core (codec): unsupported subtitle codec")
        }
    }
}

/// Convience macro for declaring `CodecProfileInfo`.
#[macro_export]
macro_rules! codec_profile {
    ($id:expr, $short_name:expr, $long_name:expr) => {
        symphonia_core::codecs::CodecProfileInfo {
            profile: $id,
            short_name: $short_name,
            long_name: $long_name,
        }
    };
}

/// Convenience macro for declaring a `SupportedAudioCodec`.
#[macro_export]
macro_rules! support_audio_codec {
    ($id:expr, $short_name:expr, $long_name:expr) => {
        symphonia_core::codecs::registry::SupportedAudioCodec {
            id: $id,
            info: symphonia_core::codecs::CodecInfo {
                short_name: $short_name,
                long_name: $long_name,
                profiles: &[],
            },
        }
    };
    ($id:expr, $short_name:expr, $long_name:expr, $profiles:expr) => {
        symphonia_core::codecs::registry::SupportedAudioCodec {
            id: $id,
            info: symphonia_core::codecs::CodecInfo {
                short_name: $short_name,
                long_name: $long_name,
                profiles: $profiles,
            },
        }
    };
}

/// Convenience macro for declaring a `SupportedVideoCodec`.
#[macro_export]
macro_rules! support_video_codec {
    ($id:expr, $short_name:expr, $long_name:expr) => {
        symphonia_core::codecs::registry::SupportedVideoCodec {
            id: $id,
            info: symphonia_core::codecs::CodecInfo {
                short_name: $short_name,
                long_name: $long_name,
                profiles: &[],
            },
        }
    };
    ($id:expr, $short_name:expr, $long_name:expr, $profiles:expr) => {
        symphonia_core::codecs::registry::SupportedVideoCodec {
            id: $id,
            info: symphonia_core::codecs::CodecInfo {
                short_name: $short_name,
                long_name: $long_name,
                profiles: $profiles,
            },
        }
    };
}

/// Convenience macro for declaring a `SupportedSubtitleCodec`.
#[macro_export]
macro_rules! support_subtitle_codec {
    ($id:expr, $short_name:expr, $long_name:expr) => {
        symphonia_core::codecs::registry::SupportedSubtitleCodec {
            id: $id,
            info: symphonia_core::codecs::CodecInfo {
                short_name: $short_name,
                long_name: $long_name,
                profiles: &[],
            },
        }
    };
}
