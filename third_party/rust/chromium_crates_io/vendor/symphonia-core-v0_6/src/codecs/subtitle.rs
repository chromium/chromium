// Symphonia
// Copyright (c) 2019-2024 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Subtitle decoder specific support.

use std::fmt;

#[cfg(feature = "exp-subtitle-codecs")]
use crate::codecs::CodecInfo;
use crate::common::FourCc;
#[cfg(feature = "exp-subtitle-codecs")]
use crate::errors::Result;
#[cfg(feature = "exp-subtitle-codecs")]
use crate::packet::Packet;
#[cfg(feature = "exp-subtitle-codecs")]
use crate::subtitle::GenericSubtitleBufferRef;

/// An `SubtitleCodecId` is a unique identifier used to identify a specific video codec.
///
/// # Creating a Codec ID
///
/// Using a [well-known](well_known) codec ID is *highly* recommended to maximize compatibility
/// between components, libraries, and applications. However, if a codec requires custom codec ID,
/// or there is no well-known ID, then the [`FourCc`] for the codec may be converted into a codec
/// ID.
#[repr(transparent)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct SubtitleCodecId(u32);

/// Null subtitle codec ID
pub const CODEC_ID_NULL_SUBTITLE: SubtitleCodecId = SubtitleCodecId(0x0);

impl SubtitleCodecId {
    /// Create a new subtitle codec ID from a FourCC.
    pub const fn new(cc: FourCc) -> SubtitleCodecId {
        // A FourCc always only contains ASCII characters. Therefore, the upper bits are always 0.
        Self(0x8000_0000 | u32::from_be_bytes(cc.get()))
    }
}

impl Default for SubtitleCodecId {
    fn default() -> Self {
        CODEC_ID_NULL_SUBTITLE
    }
}

impl From<FourCc> for SubtitleCodecId {
    fn from(value: FourCc) -> Self {
        SubtitleCodecId::new(value)
    }
}

impl fmt::Display for SubtitleCodecId {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:#x}", self.0)
    }
}

/// Codec parameters for subtitle codecs.
#[derive(Clone, Debug, Default)]
pub struct SubtitleCodecParameters {
    /// The codec ID.
    pub codec: SubtitleCodecId,
    /// Extra data (defined by the codec).
    pub extra_data: Option<Box<[u8]>>,
}

impl SubtitleCodecParameters {
    pub fn new() -> SubtitleCodecParameters {
        SubtitleCodecParameters { codec: CODEC_ID_NULL_SUBTITLE, extra_data: None }
    }

    /// Provide the `VideoCodecId`.
    pub fn for_codec(&mut self, codec: SubtitleCodecId) -> &mut Self {
        self.codec = codec;
        self
    }

    /// Provide codec extra data.
    pub fn with_extra_data(&mut self, data: Box<[u8]>) -> &mut Self {
        self.extra_data = Some(data);
        self
    }
}

/// `SubtitleDecoderOptions` is a common set of options that all subtitle decoders use.
#[cfg(feature = "exp-subtitle-codecs")]
#[non_exhaustive]
#[derive(Copy, Clone, Debug, Default)]
pub struct SubtitleDecoderOptions {
    // None yet.
}

/// A `SubtitleDecoder` implements a subtitle codec's decode algorithm. It consumes `Packet`s and
/// produces plain text or rendered subtitles.
#[cfg(feature = "exp-subtitle-codecs")]
pub trait SubtitleDecoder: Send + Sync {
    /// Reset the decoder.
    ///
    /// A decoder must be reset when the next packet is discontinuous with respect to the last
    /// decoded packet. Most notably, this occurs after a seek.
    ///
    /// # For Implementations
    ///
    /// For codecs that do a lot of pre-computation, reset should only reset the absolute minimum
    /// amount of state.
    fn reset(&mut self);

    /// Get basic information about the codec.
    fn codec_info(&self) -> &CodecInfo;

    /// Gets a reference to an updated set of `SubtitleCodecParameters` based on the codec
    /// parameters the decoder was instantiated with.
    fn codec_params(&self) -> &SubtitleCodecParameters;

    /// Decodes a `Packet` of subtitle data and returns a generic (plaintext or rendered) subtitle.
    ///
    /// If a `DecodeError` or `IoError` is returned, the packet is undecodeable and should be
    /// discarded. Decoding may be continued with the next packet.
    ///
    /// Implementors of decoders *must* `clear` the last decoded subtitle if an error occurs.
    fn decode(&mut self, packet: &Packet) -> Result<GenericSubtitleBufferRef<'_>>;

    /// Allows read access to the internal audio buffer.
    ///
    /// After a successful call to `decode`, this will contain the audio content of the last decoded
    /// `Packet`. If the last call to `decode` resulted in an error, then implementors *must* ensure
    /// the returned audio buffer has zero length.
    fn last_decoded(&self) -> GenericSubtitleBufferRef<'_>;
}

/// IDs for well-known subtitle codecs.
pub mod well_known {
    use super::SubtitleCodecId;

    // Text-based subtitle codecs
    //---------------------------

    // Plain text subtitles

    /// UTF8 encoded plain text
    pub const CODEC_ID_TEXT_UTF8: SubtitleCodecId = SubtitleCodecId(0x100);

    // Styled/formatted/rich text/markup-based subtitles

    /// SubStation Alpha
    pub const CODEC_ID_SSA: SubtitleCodecId = SubtitleCodecId(0x200);
    /// Advanced SubStation Alpha
    pub const CODEC_ID_ASS: SubtitleCodecId = SubtitleCodecId(0x201);
    /// Synchronized Accessible Media Interchange
    pub const CODEC_ID_SAMI: SubtitleCodecId = SubtitleCodecId(0x202);
    /// SubRip
    pub const CODEC_ID_SRT: SubtitleCodecId = SubtitleCodecId(0x203);
    /// WebVTT
    pub const CODEC_ID_WEBVTT: SubtitleCodecId = SubtitleCodecId(0x204);
    /// DVB subtitles
    pub const CODEC_ID_DVBSUB: SubtitleCodecId = SubtitleCodecId(0x205);
    /// HDMV text subtitles (HDMV TextST)
    pub const CODEC_ID_HDMV_TEXTST: SubtitleCodecId = SubtitleCodecId(0x206);
    /// 3GPP Timed Text subtitle (MPEG Timed Text)
    pub const CODEC_ID_MOV_TEXT: SubtitleCodecId = SubtitleCodecId(0x207);

    // Image-based subtitle codecs
    //----------------------------

    /// Bitmap Subtitle
    pub const CODEC_ID_BMP: SubtitleCodecId = SubtitleCodecId(0x300);
    /// DVD Subtitle
    pub const CODEC_ID_VOBSUB: SubtitleCodecId = SubtitleCodecId(0x301);
    /// HDMV presentation graphics subtitles (HDMV PGS)
    pub const CODEC_ID_HDMV_PGS: SubtitleCodecId = SubtitleCodecId(0x302);

    // Mixed-mode subtitle codecs
    //---------------------------

    /// OGG Karaoke and Text Encapsulation (OGG KATE)
    pub const CODEC_ID_KATE: SubtitleCodecId = SubtitleCodecId(0x400);
}
