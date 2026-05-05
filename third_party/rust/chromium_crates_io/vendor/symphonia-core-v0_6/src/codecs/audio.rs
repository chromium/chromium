// Symphonia
// Copyright (c) 2019-2024 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Audio decoder specific support.

use std::fmt;

use crate::audio::sample::SampleFormat;
use crate::audio::{Channels, GenericAudioBufferRef};
use crate::codecs::{CodecInfo, CodecProfile};
use crate::common::FourCc;
use crate::errors::Result;
use crate::packet::Packet;

/// An `AudioCodecId` is a unique identifier used to identify a specific audio codec.
///
/// # Creating a Codec ID
///
/// Using a [well-known](well_known) codec ID is *highly* recommended to maximize compatibility
/// between components, libraries, and applications. However, if a codec requires custom codec ID,
/// or there is no well-known ID, then the [`FourCc`] for the codec may be converted into a codec
/// ID.
#[repr(transparent)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct AudioCodecId(u32);

/// Null audio codec ID
pub const CODEC_ID_NULL_AUDIO: AudioCodecId = AudioCodecId(0x0);

impl Default for AudioCodecId {
    fn default() -> Self {
        CODEC_ID_NULL_AUDIO
    }
}

impl AudioCodecId {
    /// Create a new audio codec ID from a FourCC.
    pub const fn new(cc: FourCc) -> AudioCodecId {
        // A FourCc always only contains ASCII characters. Therefore, the upper bits are always 0.
        Self(0x8000_0000 | u32::from_be_bytes(cc.get()))
    }
}

impl From<FourCc> for AudioCodecId {
    fn from(value: FourCc) -> Self {
        AudioCodecId::new(value)
    }
}

impl fmt::Display for AudioCodecId {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:#x}", self.0)
    }
}

/// A method and expected value to perform verification on the decoded audio.
#[non_exhaustive]
#[derive(Copy, Clone, Debug)]
pub enum VerificationCheck {
    /// CRC8 of interleaved PCM audio samples.
    Crc8(u8),
    /// CRC16 of interleaved PCM audio samples.
    Crc16([u8; 2]),
    /// CRC32 of interleaved PCM audio samples.
    Crc32([u8; 4]),
    /// MD5 of interleaved PCM audio samples.
    Md5([u8; 16]),
    /// Codec defined, up-to 16-byte code.
    Other([u8; 16]),
}

/// Codec parameters for audio codecs.
#[derive(Clone, Debug, Default)]
pub struct AudioCodecParameters {
    /// The codec ID.
    pub codec: AudioCodecId,
    /// The codec-defined profile.
    pub profile: Option<CodecProfile>,
    /// The sample rate of the audio in Hz.
    pub sample_rate: Option<u32>,
    /// The sample format of an audio sample.
    pub sample_format: Option<SampleFormat>,
    /// The number of bits per one decoded audio sample.
    pub bits_per_sample: Option<u32>,
    /// The number of bits per one encoded audio sample.
    pub bits_per_coded_sample: Option<u32>,
    /// A bitmask of all channels in the stream.
    pub channels: Option<Channels>,
    /// The maximum number of frames a packet will contain.
    pub max_frames_per_packet: Option<u64>,
    /// A method and expected value that may be used to perform verification on the decoded audio.
    pub verification_check: Option<VerificationCheck>,
    /// The number of frames per block, in case packets are seperated in multiple blocks.
    pub frames_per_block: Option<u64>,
    /// Extra data (defined by the codec).
    pub extra_data: Option<Box<[u8]>>,
}

impl AudioCodecParameters {
    pub fn new() -> AudioCodecParameters {
        AudioCodecParameters {
            codec: CODEC_ID_NULL_AUDIO,
            profile: None,
            sample_rate: None,
            sample_format: None,
            bits_per_sample: None,
            bits_per_coded_sample: None,
            channels: None,
            max_frames_per_packet: None,
            verification_check: None,
            frames_per_block: None,
            extra_data: None,
        }
    }

    /// Provide the `AudioCodecId`.
    pub fn for_codec(&mut self, codec: AudioCodecId) -> &mut Self {
        self.codec = codec;
        self
    }

    /// Provide codec profile.
    pub fn with_profile(&mut self, profile: CodecProfile) -> &mut Self {
        self.profile = Some(profile);
        self
    }

    /// Provide the sample rate in Hz.
    pub fn with_sample_rate(&mut self, sample_rate: u32) -> &mut Self {
        self.sample_rate = Some(sample_rate);
        self
    }

    /// Provide the codec's decoded audio sample format.
    pub fn with_sample_format(&mut self, sample_format: SampleFormat) -> &mut Self {
        self.sample_format = Some(sample_format);
        self
    }

    /// Provide the bit per sample of a decoded audio sample.
    pub fn with_bits_per_sample(&mut self, bits_per_sample: u32) -> &mut Self {
        self.bits_per_sample = Some(bits_per_sample);
        self
    }

    /// Provide the bits per sample of an encoded audio sample.
    pub fn with_bits_per_coded_sample(&mut self, bits_per_coded_sample: u32) -> &mut Self {
        self.bits_per_coded_sample = Some(bits_per_coded_sample);
        self
    }

    /// Provide the channel map.
    pub fn with_channels(&mut self, channels: Channels) -> &mut Self {
        self.channels = Some(channels);
        self
    }

    /// Provide the maximum number of frames per packet.
    pub fn with_max_frames_per_packet(&mut self, len: u64) -> &mut Self {
        self.max_frames_per_packet = Some(len);
        self
    }

    /// Provide the maximum number of frames per packet.
    pub fn with_frames_per_block(&mut self, len: u64) -> &mut Self {
        self.frames_per_block = Some(len);
        self
    }

    /// Provide codec extra data.
    pub fn with_extra_data(&mut self, data: Box<[u8]>) -> &mut Self {
        self.extra_data = Some(data);
        self
    }

    /// Provide a verification code of the final decoded audio.
    pub fn with_verification_code(&mut self, code: VerificationCheck) -> &mut Self {
        self.verification_check = Some(code);
        self
    }
}

/// `FinalizeResult` contains optional information that can only be found, calculated, or
/// determined after decoding is complete.
#[derive(Copy, Clone, Debug, Default)]
pub struct FinalizeResult {
    /// If verification is enabled and supported by the decoder, provides the verification result
    /// if available.
    pub verify_ok: Option<bool>,
}

/// `AudioDecoderOptions` is a common set of options that all audio decoders use.
#[non_exhaustive]
#[derive(Copy, Clone, Debug)]
pub struct AudioDecoderOptions {
    /// Enable support for gapless playback.
    ///
    /// When enabled, the decoder will trim any delay or padding frames.
    ///
    /// Default: `true`.
    pub gapless: bool,
    /// The decoded audio should be verified if possible during the decode process.
    ///
    /// Default: `false`.
    pub verify: bool,
}

impl Default for AudioDecoderOptions {
    fn default() -> Self {
        Self { gapless: true, verify: false }
    }
}

impl AudioDecoderOptions {
    /// Enable or disable support for gapless playback.
    ///
    /// When enabled, the decoder will trim any delay or padding frames.
    ///
    /// Default: `true`.
    pub fn gapless(mut self, enable: bool) -> Self {
        self.gapless = enable;
        self
    }

    /// Enable or disable decoded audio verification if the decoder supports it.
    ///
    /// Default: `false`.
    pub fn verify(mut self, verify: bool) -> Self {
        self.verify = verify;
        self
    }
}

/// An `AudioDecoder` implements an audio codec's decode algorithm. It consumes `Packet`s and
/// produces buffers of PCM audio.
pub trait AudioDecoder: Send + Sync {
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

    /// Gets a reference to an updated set of `AudioCodecParameters` based on the codec parameters
    /// the decoder was instantiated with.
    fn codec_params(&self) -> &AudioCodecParameters;

    /// Decodes a `Packet` of audio data and returns a generic (untyped) audio buffer reference
    /// containing the decoded audio.
    ///
    /// If a `DecodeError` or `IoError` is returned, the packet is undecodeable and should be
    /// discarded. Decoding may be continued with the next packet. If `ResetRequired` is returned,
    /// consumers of the decoded audio data should expect the duration and audio specification of
    /// the decoded audio buffer to change. All other errors are unrecoverable.
    ///
    /// Implementors of decoders *must* `clear` the internal audio buffer if an error occurs.
    fn decode(&mut self, packet: &Packet) -> Result<GenericAudioBufferRef<'_>>;

    /// Optionally, obtain post-decode information such as the verification status.
    fn finalize(&mut self) -> FinalizeResult;

    /// Allows read access to the internal audio buffer.
    ///
    /// After a successful call to `decode`, this will contain the audio content of the last decoded
    /// `Packet`. If the last call to `decode` resulted in an error, then implementors *must* ensure
    /// the returned audio buffer has zero length.
    fn last_decoded(&self) -> GenericAudioBufferRef<'_>;
}

/// Codec IDs and profiles for well-known audio codecs.
pub mod well_known {
    use super::AudioCodecId;

    // Uncompressed PCM audio codecs
    //------------------------------

    /// PCM signed 32-bit little-endian interleaved
    pub const CODEC_ID_PCM_S32LE: AudioCodecId = AudioCodecId(0x100);
    /// PCM signed 32-bit little-endian planar
    pub const CODEC_ID_PCM_S32LE_PLANAR: AudioCodecId = AudioCodecId(0x101);
    /// PCM signed 32-bit big-endian interleaved
    pub const CODEC_ID_PCM_S32BE: AudioCodecId = AudioCodecId(0x102);
    /// PCM signed 32-bit big-endian planar
    pub const CODEC_ID_PCM_S32BE_PLANAR: AudioCodecId = AudioCodecId(0x103);
    /// PCM signed 24-bit little-endian interleaved
    pub const CODEC_ID_PCM_S24LE: AudioCodecId = AudioCodecId(0x104);
    /// PCM signed 24-bit little-endian planar
    pub const CODEC_ID_PCM_S24LE_PLANAR: AudioCodecId = AudioCodecId(0x105);
    /// PCM signed 24-bit big-endian interleaved
    pub const CODEC_ID_PCM_S24BE: AudioCodecId = AudioCodecId(0x106);
    /// PCM signed 24-bit big-endian planar
    pub const CODEC_ID_PCM_S24BE_PLANAR: AudioCodecId = AudioCodecId(0x107);
    /// PCM signed 16-bit little-endian interleaved
    pub const CODEC_ID_PCM_S16LE: AudioCodecId = AudioCodecId(0x108);
    /// PCM signed 16-bit little-endian planar
    pub const CODEC_ID_PCM_S16LE_PLANAR: AudioCodecId = AudioCodecId(0x109);
    /// PCM signed 16-bit big-endian interleaved
    pub const CODEC_ID_PCM_S16BE: AudioCodecId = AudioCodecId(0x10a);
    /// PCM signed 16-bit big-endian planar
    pub const CODEC_ID_PCM_S16BE_PLANAR: AudioCodecId = AudioCodecId(0x10b);
    /// PCM signed 8-bit interleaved
    pub const CODEC_ID_PCM_S8: AudioCodecId = AudioCodecId(0x10c);
    /// PCM signed 8-bit planar
    pub const CODEC_ID_PCM_S8_PLANAR: AudioCodecId = AudioCodecId(0x10d);
    /// PCM unsigned 32-bit little-endian interleaved
    pub const CODEC_ID_PCM_U32LE: AudioCodecId = AudioCodecId(0x10e);
    /// PCM unsigned 32-bit little-endian planar
    pub const CODEC_ID_PCM_U32LE_PLANAR: AudioCodecId = AudioCodecId(0x10f);
    /// PCM unsigned 32-bit big-endian interleaved
    pub const CODEC_ID_PCM_U32BE: AudioCodecId = AudioCodecId(0x110);
    /// PCM unsigned 32-bit big-endian planar
    pub const CODEC_ID_PCM_U32BE_PLANAR: AudioCodecId = AudioCodecId(0x111);
    /// PCM unsigned 24-bit little-endian interleaved
    pub const CODEC_ID_PCM_U24LE: AudioCodecId = AudioCodecId(0x112);
    /// PCM unsigned 24-bit little-endian planar
    pub const CODEC_ID_PCM_U24LE_PLANAR: AudioCodecId = AudioCodecId(0x113);
    /// PCM unsigned 24-bit big-endian interleaved
    pub const CODEC_ID_PCM_U24BE: AudioCodecId = AudioCodecId(0x114);
    /// PCM unsigned 24-bit big-endian planar
    pub const CODEC_ID_PCM_U24BE_PLANAR: AudioCodecId = AudioCodecId(0x115);
    /// PCM unsigned 16-bit little-endian interleaved
    pub const CODEC_ID_PCM_U16LE: AudioCodecId = AudioCodecId(0x116);
    /// PCM unsigned 16-bit little-endian planar
    pub const CODEC_ID_PCM_U16LE_PLANAR: AudioCodecId = AudioCodecId(0x117);
    /// PCM unsigned 16-bit big-endian interleaved
    pub const CODEC_ID_PCM_U16BE: AudioCodecId = AudioCodecId(0x118);
    /// PCM unsigned 16-bit big-endian planar
    pub const CODEC_ID_PCM_U16BE_PLANAR: AudioCodecId = AudioCodecId(0x119);
    /// PCM unsigned 8-bit interleaved
    pub const CODEC_ID_PCM_U8: AudioCodecId = AudioCodecId(0x11a);
    /// PCM unsigned 8-bit planar
    pub const CODEC_ID_PCM_U8_PLANAR: AudioCodecId = AudioCodecId(0x11b);
    /// PCM 32-bit little-endian floating point interleaved
    pub const CODEC_ID_PCM_F32LE: AudioCodecId = AudioCodecId(0x11c);
    /// PCM 32-bit little-endian floating point planar
    pub const CODEC_ID_PCM_F32LE_PLANAR: AudioCodecId = AudioCodecId(0x11d);
    /// PCM 32-bit big-endian floating point interleaved
    pub const CODEC_ID_PCM_F32BE: AudioCodecId = AudioCodecId(0x11e);
    /// PCM 32-bit big-endian floating point planar
    pub const CODEC_ID_PCM_F32BE_PLANAR: AudioCodecId = AudioCodecId(0x11f);
    /// PCM 64-bit little-endian floating point interleaved
    pub const CODEC_ID_PCM_F64LE: AudioCodecId = AudioCodecId(0x120);
    /// PCM 64-bit little-endian floating point planar
    pub const CODEC_ID_PCM_F64LE_PLANAR: AudioCodecId = AudioCodecId(0x121);
    /// PCM 64-bit big-endian floating point interleaved
    pub const CODEC_ID_PCM_F64BE: AudioCodecId = AudioCodecId(0x122);
    /// PCM 64-bit big-endian floating point planar
    pub const CODEC_ID_PCM_F64BE_PLANAR: AudioCodecId = AudioCodecId(0x123);
    /// PCM A-law (G.711)
    pub const CODEC_ID_PCM_ALAW: AudioCodecId = AudioCodecId(0x124);
    /// PCM Mu-law (G.711)
    pub const CODEC_ID_PCM_MULAW: AudioCodecId = AudioCodecId(0x125);

    // ADPCM audio codecs
    //-------------------

    /// G.722 ADPCM
    pub const CODEC_ID_ADPCM_G722: AudioCodecId = AudioCodecId(0x200);
    /// G.726 ADPCM
    pub const CODEC_ID_ADPCM_G726: AudioCodecId = AudioCodecId(0x201);
    /// G.726 ADPCM little-endian
    pub const CODEC_ID_ADPCM_G726LE: AudioCodecId = AudioCodecId(0x202);
    /// Microsoft ADPCM
    pub const CODEC_ID_ADPCM_MS: AudioCodecId = AudioCodecId(0x203);
    /// ADPCM IMA WAV
    pub const CODEC_ID_ADPCM_IMA_WAV: AudioCodecId = AudioCodecId(0x204);
    /// ADPCM IMA QuickTime
    pub const CODEC_ID_ADPCM_IMA_QT: AudioCodecId = AudioCodecId(0x205);

    // Compressed lossy audio codecs
    //------------------------------

    /// Vorbis
    pub const CODEC_ID_VORBIS: AudioCodecId = AudioCodecId(0x1000);
    /// Opus
    pub const CODEC_ID_OPUS: AudioCodecId = AudioCodecId(0x1001);
    /// Speex
    pub const CODEC_ID_SPEEX: AudioCodecId = AudioCodecId(0x1002);
    /// Musepack
    pub const CODEC_ID_MUSEPACK: AudioCodecId = AudioCodecId(0x1003);
    /// MPEG Layer 1 (MP1)
    pub const CODEC_ID_MP1: AudioCodecId = AudioCodecId(0x1004);
    /// MPEG Layer 2 (MP2)
    pub const CODEC_ID_MP2: AudioCodecId = AudioCodecId(0x1005);
    /// MPEG Layer 3 (MP3)
    pub const CODEC_ID_MP3: AudioCodecId = AudioCodecId(0x1006);
    /// Advanced Audio Coding (AAC)
    pub const CODEC_ID_AAC: AudioCodecId = AudioCodecId(0x1007);
    /// AC-3 (Dolby Digital, ATSC A/52A)
    pub const CODEC_ID_AC3: AudioCodecId = AudioCodecId(0x1008);
    /// Enhanced AC-3 (EAC-3, ATSC A/52B)
    pub const CODEC_ID_EAC3: AudioCodecId = AudioCodecId(0x1009);
    /// Dolby AC-4 (ETSI TS 103 190)
    pub const CODEC_ID_AC4: AudioCodecId = AudioCodecId(0x100a);
    /// DTS Coherent Acoustics (DCA/DTS)
    pub const CODEC_ID_DCA: AudioCodecId = AudioCodecId(0x100b);
    /// Adaptive Transform Acoustic Coding (ATRAC1)
    pub const CODEC_ID_ATRAC1: AudioCodecId = AudioCodecId(0x100c);
    /// Adaptive Transform Acoustic Coding 3 (ATRAC3)
    pub const CODEC_ID_ATRAC3: AudioCodecId = AudioCodecId(0x100d);
    /// Adaptive Transform Acoustic Coding 3+ (ATRAC3+)
    pub const CODEC_ID_ATRAC3PLUS: AudioCodecId = AudioCodecId(0x100e);
    /// Adaptive Transform Acoustic Coding 9 (ATRAC9)
    pub const CODEC_ID_ATRAC9: AudioCodecId = AudioCodecId(0x100f);
    /// Windows Media Audio
    pub const CODEC_ID_WMA: AudioCodecId = AudioCodecId(0x1010);
    /// RealAudio 1.0 14.4K (IS-54 VSELP)
    pub const CODEC_ID_RA10: AudioCodecId = AudioCodecId(0x1011);
    /// RealAudio 2.0 28.8K (G.728 LD-CELP)
    pub const CODEC_ID_RA20: AudioCodecId = AudioCodecId(0x1012);
    /// SIPR (ACELP.net, RealAudio 4.0/5.0)
    pub const CODEC_ID_SIPR: AudioCodecId = AudioCodecId(0x1013);
    /// Cook, Cooker, Gecko (RealAudio 6.0/G2)
    pub const CODEC_ID_COOK: AudioCodecId = AudioCodecId(0x1014);
    /// Low-complexity Subband Coding (SBC)
    pub const CODEC_ID_SBC: AudioCodecId = AudioCodecId(0x1015);
    /// aptX
    pub const CODEC_ID_APTX: AudioCodecId = AudioCodecId(0x1016);
    /// aptX HD
    pub const CODEC_ID_APTX_HD: AudioCodecId = AudioCodecId(0x1017);
    /// Lossless Digital Audio Codec (LDAC)
    pub const CODEC_ID_LDAC: AudioCodecId = AudioCodecId(0x1018);
    /// Bink Audio
    pub const CODEC_ID_BINK_AUDIO: AudioCodecId = AudioCodecId(0x1019);
    /// Smacker Audio
    pub const CODEC_ID_SMACKER_AUDIO: AudioCodecId = AudioCodecId(0x1020);

    // Compressed lossless audio codecs
    //---------------------------------

    /// Free Lossless Audio Codec (FLAC)
    pub const CODEC_ID_FLAC: AudioCodecId = AudioCodecId(0x2000);
    /// WavPack
    pub const CODEC_ID_WAVPACK: AudioCodecId = AudioCodecId(0x2001);
    /// Monkey's Audio (APE)
    pub const CODEC_ID_MONKEYS_AUDIO: AudioCodecId = AudioCodecId(0x2002);
    /// Apple Lossless Audio Codec (ALAC)
    pub const CODEC_ID_ALAC: AudioCodecId = AudioCodecId(0x2003);
    /// True Audio (TTA)
    pub const CODEC_ID_TTA: AudioCodecId = AudioCodecId(0x2004);
    /// RealAudio Lossless Format (RALF)
    pub const CODEC_ID_RALF: AudioCodecId = AudioCodecId(0x2005);
    /// Dolby TrueHD Lossless codec
    pub const CODEC_ID_TRUEHD: AudioCodecId = AudioCodecId(0x2006);

    /// Codec profiles for well-known audio codecs.
    pub mod profiles {
        use crate::codecs::CodecProfile;

        // AAC Profiles
        //-------------

        // AAC profiles are defined to be the MPEG-4 Audio Object Type minus 1 (as per ADTS).

        /// AAC Main Profile
        pub const CODEC_PROFILE_AAC_MAIN: CodecProfile = CodecProfile(0);
        /// AAC Low Complexity (LC) Profile
        pub const CODEC_PROFILE_AAC_LC: CodecProfile = CodecProfile(1);
        /// AAC Scalable Sample Rate (SSR) Profile
        pub const CODEC_PROFILE_AAC_SSR: CodecProfile = CodecProfile(2);
        /// AAC Long Term Prediction (LTP) Profile
        pub const CODEC_PROFILE_AAC_LTP: CodecProfile = CodecProfile(3);
        /// High Efficiency AAC (HE-AAC) Profile (using Spectral Band Replication)
        pub const CODEC_PROFILE_AAC_HE: CodecProfile = CodecProfile(4);
        /// High Efficiency AAC v2 (HE-AACv2) Profile (using Parametric Stereo)
        pub const CODEC_PROFILE_AAC_HE_V2: CodecProfile = CodecProfile(28);
        /// Extended HE-AAC (xHE-AAC) Profile (using Unified Speech and Audio Coding)
        pub const CODEC_PROFILE_AAC_USAC: CodecProfile = CodecProfile(41);
    }
}
