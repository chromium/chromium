// Symphonia
// Copyright (c) 2019-2024 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Video decoder specific support.

use std::fmt;

#[cfg(feature = "exp-video-codecs")]
use crate::codecs::CodecInfo;
use crate::codecs::CodecProfile;
use crate::common::FourCc;

/// An `VideoCodecId` is a unique identifier used to identify a specific video codec.
///
/// # Creating a Codec ID
///
/// Using a [well-known](well_known) codec ID is *highly* recommended to maximize compatibility
/// between components, libraries, and applications. However, if a codec requires custom codec ID,
/// or there is no well-known ID, then the [`FourCc`] for the codec may be converted into a codec
/// ID.
#[repr(transparent)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct VideoCodecId(u32);

/// Null video codec ID
pub const CODEC_ID_NULL_VIDEO: VideoCodecId = VideoCodecId(0x0);

impl Default for VideoCodecId {
    fn default() -> Self {
        CODEC_ID_NULL_VIDEO
    }
}

impl VideoCodecId {
    /// Create a new video codec ID from a FourCC.
    pub const fn new(cc: FourCc) -> VideoCodecId {
        // A FourCc always only contains ASCII characters. Therefore, the upper bits are always 0.
        Self(0x8000_0000 | u32::from_be_bytes(cc.get()))
    }
}

impl From<FourCc> for VideoCodecId {
    fn from(value: FourCc) -> Self {
        VideoCodecId::new(value)
    }
}

impl fmt::Display for VideoCodecId {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:#x}", self.0)
    }
}

/// An `VideoExtraDataId` is a unique identifier used to identify a specific video extra data.
#[repr(transparent)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct VideoExtraDataId(u32);

/// Null video extra data ID.
pub const VIDEO_EXTRA_DATA_ID_NULL: VideoExtraDataId = VideoExtraDataId(0x0);

impl Default for VideoExtraDataId {
    fn default() -> Self {
        VIDEO_EXTRA_DATA_ID_NULL
    }
}

/// Extra data for a video codec.
#[derive(Clone, Debug, Default)]
pub struct VideoExtraData {
    /// The extra data ID.
    pub id: VideoExtraDataId,
    /// Extra data (defined by codec)
    pub data: Box<[u8]>,
}

/// Codec parameters for video codecs.
#[derive(Clone, Debug, Default)]
pub struct VideoCodecParameters {
    /// The codec ID.
    pub codec: VideoCodecId,
    /// The codec-defined profile.
    pub profile: Option<CodecProfile>,
    /// The codec-defined level.
    pub level: Option<u32>,
    /// Video width.
    pub width: Option<u16>,
    /// Video height.
    pub height: Option<u16>,
    /// Extra data (defined by the codec).
    pub extra_data: Vec<VideoExtraData>,
}

impl VideoCodecParameters {
    /// Provide the `VideoCodecId`.
    pub fn for_codec(&mut self, codec: VideoCodecId) -> &mut Self {
        self.codec = codec;
        self
    }

    /// Provide codec profile.
    pub fn with_profile(&mut self, profile: CodecProfile) -> &mut Self {
        self.profile = Some(profile);
        self
    }

    /// Provide codec level.
    pub fn with_level(&mut self, level: u32) -> &mut Self {
        self.level = Some(level);
        self
    }

    /// Provide video width.
    pub fn with_width(&mut self, width: u16) -> &mut Self {
        self.width = Some(width);
        self
    }

    /// Provide video height.
    pub fn with_height(&mut self, height: u16) -> &mut Self {
        self.height = Some(height);
        self
    }

    /// Adds codec's extra data.
    pub fn add_extra_data(&mut self, data: VideoExtraData) -> &mut Self {
        self.extra_data.push(data);
        self
    }
}

/// `VideoDecoderOptions` is a common set of options that all subtitle decoders use.
#[cfg(feature = "exp-video-codecs")]
#[non_exhaustive]
#[derive(Copy, Clone, Debug, Default)]
pub struct VideoDecoderOptions {
    // None yet.
}

/// A `VideoDecoder` implements a video codec's decode algorithm. It consumes `Packet`s and
/// produces video frames.
#[cfg(feature = "exp-video-codecs")]
pub trait VideoDecoder: Send + Sync {
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

    /// Gets a reference to an updated set of `VideoCodecParameters` based on the codec parameters
    /// the decoder was instantiated with.
    fn codec_params(&self) -> &VideoCodecParameters;
}

/// Codec IDs and profiles for well-known video codecs.
pub mod well_known {
    use super::VideoCodecId;

    /// Motion JPEG
    pub const CODEC_ID_MJPEG: VideoCodecId = VideoCodecId(0x100);

    // RAD Games Tools (Epic Games Tools) codecs

    /// Bink Video
    pub const CODEC_ID_BINK_VIDEO: VideoCodecId = VideoCodecId(0x200);
    /// Smacker Video
    pub const CODEC_ID_SMACKER_VIDEO: VideoCodecId = VideoCodecId(0x201);

    /// Cinepak
    pub const CODEC_ID_CINEPAK: VideoCodecId = VideoCodecId(0x300);

    // Intel codecs

    /// Intel Indeo Video 2
    pub const CODEC_ID_INDEO2: VideoCodecId = VideoCodecId(0x400);
    /// Intel Indeo Video 3
    pub const CODEC_ID_INDEO3: VideoCodecId = VideoCodecId(0x401);
    /// Intel Indeo Video Interactive 4
    pub const CODEC_ID_INDEO4: VideoCodecId = VideoCodecId(0x402);
    /// Intel Indeo Video Interactive 5
    pub const CODEC_ID_INDEO5: VideoCodecId = VideoCodecId(0x403);

    // Sorenson codecs

    /// Sorenson Video 1 (SVQ1)
    pub const CODEC_ID_SVQ1: VideoCodecId = VideoCodecId(0x500);
    /// Sorenson Video 1 (SVQ3)
    pub const CODEC_ID_SVQ3: VideoCodecId = VideoCodecId(0x501);
    /// Flash Video (Sorenson Spark, Sorenson H.263, FLV1)
    pub const CODEC_ID_FLV: VideoCodecId = VideoCodecId(0x502);

    // RealNetworks codecs

    /// RealVideo 1.0 (RV10)
    pub const CODEC_ID_RV10: VideoCodecId = VideoCodecId(0x600);
    /// RealVideo 2.0 (RV20)
    pub const CODEC_ID_RV20: VideoCodecId = VideoCodecId(0x601);
    /// RealVideo 3.0 (RV30)
    pub const CODEC_ID_RV30: VideoCodecId = VideoCodecId(0x602);
    /// RealVideo 4.0 (RV40)
    pub const CODEC_ID_RV40: VideoCodecId = VideoCodecId(0x603);

    // Microsoft codecs

    /// Microsoft MPEG-4 Part 2 version 1 (MPG4)
    pub const CODEC_ID_MSMPEG4V1: VideoCodecId = VideoCodecId(0x700);
    /// Microsoft MPEG-4 Part 2 version 2 (MP42)
    pub const CODEC_ID_MSMPEG4V2: VideoCodecId = VideoCodecId(0x701);
    /// Microsoft MPEG-4 Part 2 version 3 (MP43)
    pub const CODEC_ID_MSMPEG4V3: VideoCodecId = VideoCodecId(0x702);
    /// Windows Media Video 7 (WMV1)
    pub const CODEC_ID_WMV1: VideoCodecId = VideoCodecId(0x703);
    /// Windows Media Video 8 (WMV2)
    pub const CODEC_ID_WMV2: VideoCodecId = VideoCodecId(0x704);
    /// Windows Media Video 9 (WMV3)
    pub const CODEC_ID_WMV3: VideoCodecId = VideoCodecId(0x705);

    // On2 Technologies & Google codecs

    /// On2 TrueMotion VP3 (VP3)
    pub const CODEC_ID_VP3: VideoCodecId = VideoCodecId(0x800);
    /// On2 TrueMotion VP4 (VP4)
    pub const CODEC_ID_VP4: VideoCodecId = VideoCodecId(0x801);
    /// On2 TrueMotion VP5 (VP5)
    pub const CODEC_ID_VP5: VideoCodecId = VideoCodecId(0x802);
    /// On2 TrueMotion VP6 (VP6)
    pub const CODEC_ID_VP6: VideoCodecId = VideoCodecId(0x803);
    /// On2 TrueMotion VP7 (VP7)
    pub const CODEC_ID_VP7: VideoCodecId = VideoCodecId(0x804);
    /// On2 TrueMotion VP8 (VP8)
    pub const CODEC_ID_VP8: VideoCodecId = VideoCodecId(0x805);
    /// On2 TrueMotion VP9 (VP9)
    pub const CODEC_ID_VP9: VideoCodecId = VideoCodecId(0x806);

    // Xiph codecs

    /// Theora
    pub const CODEC_ID_THEORA: VideoCodecId = VideoCodecId(0x900);
    /// AOMedia Video 1 (AV1)
    pub const CODEC_ID_AV1: VideoCodecId = VideoCodecId(0x901);

    // ISO, IEC, MPEG codecs

    /// MPEG-1 Video (MPEG-1 Part 2)
    pub const CODEC_ID_MPEG1: VideoCodecId = VideoCodecId(0xa00);
    /// MPEG-2 Video (MPEG-2 Part 2)
    pub const CODEC_ID_MPEG2: VideoCodecId = VideoCodecId(0xa01);
    /// MPEG-4 Video (MPEG-2 Part 2)
    pub const CODEC_ID_MPEG4: VideoCodecId = VideoCodecId(0xa02);

    // ITU-T codecs

    /// H.261
    pub const CODEC_ID_H261: VideoCodecId = VideoCodecId(0xb01);
    /// H.263
    pub const CODEC_ID_H263: VideoCodecId = VideoCodecId(0xb03);
    /// Advanced Video Codec (AVC, MPEG-4 AVC, MPEG-4 Part 10, H.264)
    pub const CODEC_ID_H264: VideoCodecId = VideoCodecId(0xb04);
    /// High Efficiency Video Coding (HEVC, H.265, MPEG-H Part 2)
    pub const CODEC_ID_HEVC: VideoCodecId = VideoCodecId(0xb05);
    /// Versatile Video Coding (VVC, H.266, MPEG-I Part 3)
    pub const CODEC_ID_VVC: VideoCodecId = VideoCodecId(0xb06);

    // SMPTE codecs

    /// SMPTE VC-1
    pub const CODEC_ID_VC1: VideoCodecId = VideoCodecId(0xc00);

    // Audio Video Standard (AVS) codecs

    /// Audio Video Standard (AVS) 1
    pub const CODEC_ID_AVS1: VideoCodecId = VideoCodecId(0xd00);
    /// Audio Video Standard (AVS) 2
    pub const CODEC_ID_AVS2: VideoCodecId = VideoCodecId(0xd01);
    /// Audio Video Standard (AVS) 3
    pub const CODEC_ID_AVS3: VideoCodecId = VideoCodecId(0xd02);

    /// Codec profiles for well-known video codecs.
    pub mod profiles {
        use crate::codecs::CodecProfile;

        // AV1 profiles
        //-------------

        /// AV1 Main Profile
        pub const CODEC_PROFILE_AV1_MAIN: CodecProfile = CodecProfile(0);
        /// AV1 High Profile
        pub const CODEC_PROFILE_AV1_HIGH: CodecProfile = CodecProfile(1);
        /// AV1 Professional Profile
        pub const CODEC_PROFILE_AV1_PROFESSIONAL: CodecProfile = CodecProfile(2);

        // MPEG-2 profiles
        //----------------

        /// MPEG-2 Video Simple Profile (SP)
        pub const CODEC_PROFILE_MPEG2_SIMPLE: CodecProfile = CodecProfile(0);
        /// MPEG-2 Video Main Profile (MP)
        pub const CODEC_PROFILE_MPEG2_MAIN: CodecProfile = CodecProfile(1);
        /// MPEG-2 Video SNR Scalable Profile
        pub const CODEC_PROFILE_MPEG2_SNR_SCALABLE: CodecProfile = CodecProfile(2);
        /// MPEG-2 Video Spatial Scalable Profile
        pub const CODEC_PROFILE_MPEG2_SPATIAL_SCALABLE: CodecProfile = CodecProfile(3);
        /// MPEG-2 Video High Profile (HP)
        pub const CODEC_PROFILE_MPEG2_HIGH: CodecProfile = CodecProfile(4);
        /// MPEG-2 Video 4:2:2 Profile (422)
        pub const CODEC_PROFILE_MPEG2_422: CodecProfile = CodecProfile(5);

        // MPEG-4 profiles
        //---------------

        /// MPEG-4 Video Simple Profile (SP)
        pub const CODEC_PROFILE_MPEG4_SIMPLE: CodecProfile = CodecProfile(0);
        /// MPEG-4 Video Advanced Simple Profile (ASP)
        pub const CODEC_PROFILE_MPEG4_ADVANCED_SIMPLE: CodecProfile = CodecProfile(1);

        // H264 profiles
        //--------------

        /// H.264 Baseline Profile (BP)
        pub const CODEC_PROFILE_H264_BASELINE: CodecProfile = CodecProfile(66);
        /// H.264 Contrained Baseline Profile (CBP)
        pub const CODEC_PROFILE_H264_CONSTRAINED_BASELINE: CodecProfile =
            CodecProfile(66 | 1 << (8 + 1)); // Constraint set 1
        /// H.264 Main Profile (MP)
        pub const CODEC_PROFILE_H264_MAIN: CodecProfile = CodecProfile(77);
        /// H.264 Extended Profile (XP)
        pub const CODEC_PROFILE_H264_EXTENDED: CodecProfile = CodecProfile(88);
        /// H.264 High Profile (HiP)
        pub const CODEC_PROFILE_H264_HIGH: CodecProfile = CodecProfile(100);
        /// H.264 Progressive High Profile (PHiP)
        pub const CODEC_PROFILE_H264_PROGRESSIVE_HIGH: CodecProfile =
            CodecProfile(100 | 1 << (8 + 4)); // Constraint set 4
        /// H.264 Constrained High profile
        pub const CODEC_PROFILE_H264_CONSTRAINED_HIGH: CodecProfile =
            CodecProfile(100 | 1 << (8 + 4) | 1 << (8 + 5)); // Constraint set 4 & 5
        /// H.264 High 10 Profile (Hi10P)
        pub const CODEC_PROFILE_H264_HIGH_10: CodecProfile = CodecProfile(110);
        /// H.264 High 10 Intra Profile
        pub const CODEC_PROFILE_H264_HIGH_10_INTRA: CodecProfile = CodecProfile(110 | 1 << (8 + 3)); // Constraint set 3
        /// H.264 High 4:2:2 Profile (Hi422P)
        pub const CODEC_PROFILE_H264_HIGH_422: CodecProfile = CodecProfile(122);
        /// H.264 High 4:2:2 Intra Profile
        pub const CODEC_PROFILE_H264_HIGH_422_INTRA: CodecProfile =
            CodecProfile(122 | 1 << (8 + 3)); // Constraint set 3
        /// H.264 High 4:4:4 Profile (Hi444P)
        pub const CODEC_PROFILE_H264_HIGH_444: CodecProfile = CodecProfile(144);
        /// H.264 High 4:4:4 Predictive Profile (Hi444PP)
        pub const CODEC_PROFILE_H264_HIGH_444_PREDICTIVE: CodecProfile = CodecProfile(244);
        /// H.264 High 4:4:4 Intra Profile
        pub const CODEC_PROFILE_H264_HIGH_444_INTRA: CodecProfile =
            CodecProfile(244 | 1 << (8 + 3)); // Constraint set 3
        /// H.264 CAVLC 4:4:4 Profile
        pub const CODEC_PROFILE_H264_CAVLC_444: CodecProfile = CodecProfile(44);

        // HEVC profiles
        //--------------

        /// HEVC Main Profile
        pub const CODEC_PROFILE_HEVC_MAIN: CodecProfile = CodecProfile(1);
        /// HEVC Main 10 Profile
        pub const CODEC_PROFILE_HEVC_MAIN_10: CodecProfile = CodecProfile(2);
        /// HEVC Main Still Picture Profile
        pub const CODEC_PROFILE_HEVC_MAIN_STILL_PICTURE: CodecProfile = CodecProfile(3);

        // VP9 profiles
        //-------------

        /// VP9 Profile 0
        pub const CODEC_PROFILE_VP9_0: CodecProfile = CodecProfile(0);
        /// VP9 Profile 1
        pub const CODEC_PROFILE_VP9_1: CodecProfile = CodecProfile(1);
        /// VP9 Profile 2
        pub const CODEC_PROFILE_VP9_2: CodecProfile = CodecProfile(2);
        /// VP9 Profile 3
        pub const CODEC_PROFILE_VP9_3: CodecProfile = CodecProfile(3);

        // VC1 profiles
        //-------------

        /// VC-1 Simple Profile
        pub const CODEC_PROFILE_VC1_SIMPLE: CodecProfile = CodecProfile(0);
        /// VC-1 Main Profile
        pub const CODEC_PROFILE_VC1_MAIN: CodecProfile = CodecProfile(1);
        /// VC-1 Advanced Profile
        pub const CODEC_PROFILE_VC1_ADVANCED: CodecProfile = CodecProfile(2);
    }

    pub mod extra_data {
        use crate::codecs::video::VideoExtraDataId;

        /// AVCDecoderConfigurationRecord
        pub const VIDEO_EXTRA_DATA_ID_AVC_DECODER_CONFIG: VideoExtraDataId = VideoExtraDataId(1);

        /// HEVCDecoderConfigurationRecord
        pub const VIDEO_EXTRA_DATA_ID_HEVC_DECODER_CONFIG: VideoExtraDataId = VideoExtraDataId(2);

        /// VP9DecoderConfiguration
        pub const VIDEO_EXTRA_DATA_ID_VP9_DECODER_CONFIG: VideoExtraDataId = VideoExtraDataId(3);

        /// AV1DecoderConfiguration
        pub const VIDEO_EXTRA_DATA_ID_AV1_DECODER_CONFIG: VideoExtraDataId = VideoExtraDataId(4);

        /// DolbyVisionConfiguration
        pub const VIDEO_EXTRA_DATA_ID_DOLBY_VISION_CONFIG: VideoExtraDataId = VideoExtraDataId(5);

        /// DolbyVision EL HEVC
        pub const VIDEO_EXTRA_DATA_ID_DOLBY_VISION_EL_HEVC: VideoExtraDataId = VideoExtraDataId(6);
    }
}
