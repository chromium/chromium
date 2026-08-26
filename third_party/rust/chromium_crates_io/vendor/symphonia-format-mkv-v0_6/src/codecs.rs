// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use extra_data::{
    VIDEO_EXTRA_DATA_ID_AV1_DECODER_CONFIG, VIDEO_EXTRA_DATA_ID_AVC_DECODER_CONFIG,
    VIDEO_EXTRA_DATA_ID_HEVC_DECODER_CONFIG, VIDEO_EXTRA_DATA_ID_VP9_DECODER_CONFIG,
};
use log::warn;

use symphonia_common::mpeg::video::{
    AVCDecoderConfigurationRecord, HEVCDecoderConfigurationRecord,
};
use symphonia_common::xiph::audio::flac::{MetadataBlockHeader, MetadataBlockType, StreamInfo};
use symphonia_core::audio::Channels;
use symphonia_core::audio::sample::SampleFormat;
use symphonia_core::codecs::audio::AudioCodecParameters;
use symphonia_core::codecs::audio::well_known::{CODEC_ID_FLAC, CODEC_ID_VORBIS};
use symphonia_core::codecs::audio::{AudioCodecId, well_known::*};
use symphonia_core::codecs::subtitle::{SubtitleCodecId, SubtitleCodecParameters, well_known::*};
use symphonia_core::codecs::video::{
    VIDEO_EXTRA_DATA_ID_NULL, VideoCodecId, VideoCodecParameters, VideoExtraData, well_known::*,
};
use symphonia_core::codecs::{CodecId, CodecParameters, CodecProfile};
use symphonia_core::errors::{Error, Result, decode_error};
use symphonia_core::io::{BufReader, ReadBytes};

use crate::lacing::read_xiph_sizes;
use crate::segment::TrackElement;

pub(crate) fn make_track_codec_params(track: TrackElement) -> Result<Option<CodecParameters>> {
    // Get the codec ID for the track.
    let codec_id = get_codec_id(&track);
    let (profile, level) = get_codec_profile_and_level(&track);

    match codec_id {
        Some(CodecId::Audio(id)) => make_audio_codec_params(id, profile, track),
        Some(CodecId::Video(id)) => make_video_codec_params(id, profile, level, track),
        Some(CodecId::Subtitle(id)) => make_subtitle_codec_params(id, track),
        _ => Ok(None),
    }
}

fn make_audio_codec_params(
    id: AudioCodecId,
    profile: Option<CodecProfile>,
    track: TrackElement,
) -> Result<Option<CodecParameters>> {
    // A nested audio track element in expected in the track element.
    let audio = match track.audio {
        Some(audio) => audio,
        None => {
            warn!("expected audio element in track element");
            return Ok(None);
        }
    };

    let mut codec_params = AudioCodecParameters::new();

    codec_params.for_codec(id);

    if let Some(profile) = profile {
        codec_params.with_profile(profile);
    }

    codec_params.with_sample_rate(audio.sampling_frequency.round() as u32);
    codec_params.with_channels(Channels::Discrete(audio.channels.get() as u16));

    let format = audio.bit_depth.and_then(|bits| match bits.get() {
        8 => Some(SampleFormat::S8),
        16 => Some(SampleFormat::S16),
        24 => Some(SampleFormat::S24),
        32 => Some(SampleFormat::S32),
        _ => None,
    });

    if let Some(format) = format {
        codec_params.with_sample_format(format);
    }

    if let Some(bits) = audio.bit_depth {
        codec_params.with_bits_per_sample(bits.get() as u32);
    }

    if let Some(codec_private) = track.codec_private {
        let extra_data = match id {
            CODEC_ID_VORBIS => vorbis_extra_data_from_codec_private(&codec_private)?,
            CODEC_ID_FLAC => flac_extra_data_from_codec_private(&codec_private)?,
            _ => codec_private,
        };

        codec_params.with_extra_data(extra_data);
    }

    Ok(Some(CodecParameters::Audio(codec_params)))
}

fn make_video_codec_params(
    id: VideoCodecId,
    profile: Option<CodecProfile>,
    level: Option<u32>,
    track: TrackElement,
) -> Result<Option<CodecParameters>> {
    // A nested video track element in expected in the track element.
    let video = match track.video {
        Some(video) => video,
        None => {
            warn!("expected video element in track element");
            return Ok(None);
        }
    };

    let mut codec_params = VideoCodecParameters {
        codec: id,
        width: Some(
            u16::try_from(video.pixel_width.get())
                .map_err(|_| Error::Unsupported("mkv: video width too large"))?,
        ),
        height: Some(
            u16::try_from(video.pixel_height.get())
                .map_err(|_| Error::Unsupported("mkv: video height too large"))?,
        ),
        ..Default::default()
    };

    if let Some(profile) = profile {
        codec_params.with_profile(profile);
    }

    if let Some(level) = level {
        codec_params.with_level(level);
    }

    if let Some(codec_private) = track.codec_private {
        let extra_data_id = match id {
            CODEC_ID_H264 => VIDEO_EXTRA_DATA_ID_AVC_DECODER_CONFIG,
            CODEC_ID_HEVC => VIDEO_EXTRA_DATA_ID_HEVC_DECODER_CONFIG,
            CODEC_ID_VP9 => VIDEO_EXTRA_DATA_ID_VP9_DECODER_CONFIG,
            CODEC_ID_AV1 => VIDEO_EXTRA_DATA_ID_AV1_DECODER_CONFIG,
            _ => VIDEO_EXTRA_DATA_ID_NULL,
        };
        codec_params.add_extra_data(VideoExtraData { id: extra_data_id, data: codec_private });
    }

    for block in track.block_addition_mappings {
        if let Some(extra_data) = block.extra_data {
            codec_params.add_extra_data(extra_data);
        }
    }

    Ok(Some(CodecParameters::Video(codec_params)))
}

fn make_subtitle_codec_params(
    id: SubtitleCodecId,
    track: TrackElement,
) -> Result<Option<CodecParameters>> {
    let mut codec_params = SubtitleCodecParameters::new();

    codec_params.for_codec(id);

    if let Some(codec_private) = track.codec_private {
        codec_params.with_extra_data(codec_private);
    }

    Ok(Some(CodecParameters::Subtitle(codec_params)))
}

// Extra data parsing
fn vorbis_extra_data_from_codec_private(extra: &[u8]) -> Result<Box<[u8]>> {
    const VORBIS_PACKET_TYPE_IDENTIFICATION: u8 = 1;
    const VORBIS_PACKET_TYPE_SETUP: u8 = 5;

    // Private Data for this codec has the following layout:
    // - 1 byte that represents number of packets minus one;
    // - Xiph coded lengths of packets, length of the last packet must be deduced (as in Xiph lacing)
    // - packets in order:
    //    - The Vorbis identification header
    //    - Vorbis comment header
    //    - codec setup header

    let mut reader = BufReader::new(extra);
    let packet_count = reader.read_byte()? as usize;
    let packet_lengths = read_xiph_sizes(&mut reader, packet_count)?;

    let mut packets = Vec::new();
    for length in packet_lengths {
        packets.push(reader.read_boxed_slice_exact(length as usize)?);
    }

    let last_packet_length = extra.len() - reader.pos() as usize;
    packets.push(reader.read_boxed_slice_exact(last_packet_length)?);

    let mut ident_header = None;
    let mut setup_header = None;

    for packet in packets {
        match packet.first().copied() {
            Some(VORBIS_PACKET_TYPE_IDENTIFICATION) => {
                ident_header = Some(packet);
            }
            Some(VORBIS_PACKET_TYPE_SETUP) => {
                setup_header = Some(packet);
            }
            _ => {
                log::debug!("unsupported vorbis packet type");
            }
        }
    }

    // This is layout expected currently by Vorbis codec.
    Ok([
        ident_header.ok_or(Error::DecodeError("mkv: missing vorbis identification packet"))?,
        setup_header.ok_or(Error::DecodeError("mkv: missing vorbis setup packet"))?,
    ]
    .concat()
    .into_boxed_slice())
}

fn flac_extra_data_from_codec_private(codec_private: &[u8]) -> Result<Box<[u8]>> {
    let mut reader = BufReader::new(codec_private);

    let marker = reader.read_quad_bytes()?;
    if marker != *b"fLaC" {
        return decode_error("mkv (flac): missing flac stream marker");
    }

    // The codec private data contains all header/matadata blocks. Extract the stream information
    // block (typically the first).
    loop {
        let header = MetadataBlockHeader::read(&mut reader)?;

        match header.block_type {
            MetadataBlockType::StreamInfo => {
                // Ensure the stream information block is the expected size.
                if !StreamInfo::is_valid_size(u64::from(header.block_len)) {
                    return decode_error("isomp4 (flac): invalid stream info block length");
                }

                break Ok(reader.read_boxed_slice_exact(header.block_len as usize)?);
            }
            _ => reader.ignore_bytes(u64::from(header.block_len))?,
        }
    }
}

fn get_codec_id(track: &TrackElement) -> Option<CodecId> {
    let bit_depth = track.audio.as_ref().and_then(|a| a.bit_depth);

    let codec_id = match track.codec_id.as_str() {
        // Audio Codecs
        "A_MPEG/L1" => CodecId::Audio(CODEC_ID_MP1),
        "A_MPEG/L2" => CodecId::Audio(CODEC_ID_MP2),
        "A_MPEG/L3" => CodecId::Audio(CODEC_ID_MP3),
        "A_FLAC" => CodecId::Audio(CODEC_ID_FLAC),
        "A_OPUS" => CodecId::Audio(CODEC_ID_OPUS),
        "A_VORBIS" => CodecId::Audio(CODEC_ID_VORBIS),
        "A_AAC/MPEG2/MAIN" | "A_AAC/MPEG2/LC" | "A_AAC/MPEG2/LC/SBR" | "A_AAC/MPEG2/SSR"
        | "A_AAC/MPEG4/MAIN" | "A_AAC/MPEG4/LC" | "A_AAC/MPEG4/LC/SBR" | "A_AAC/MPEG4/SSR"
        | "A_AAC/MPEG4/LTP" | "A_AAC" => CodecId::Audio(CODEC_ID_AAC),
        "A_MPC" => CodecId::Audio(CODEC_ID_MUSEPACK),
        "A_AC3" | "A_AC3/BSID9" | "A_AC3/BSID10" => CodecId::Audio(CODEC_ID_AC3),
        "A_EAC3" => CodecId::Audio(CODEC_ID_EAC3),
        "A_TRUEHD" => CodecId::Audio(CODEC_ID_TRUEHD),
        "A_ALAC" => CodecId::Audio(CODEC_ID_ALAC),
        "A_DTS" => CodecId::Audio(CODEC_ID_DCA),
        // A_DTS/EXPRESS
        // A_DTS/LOSSLESS
        "A_TTA1" => CodecId::Audio(CODEC_ID_TTA),
        "A_WAVPACK4" => CodecId::Audio(CODEC_ID_WAVPACK),
        "A_ATRAC/AT1" => CodecId::Audio(CODEC_ID_ATRAC1),
        "A_REAL/ATRC" => CodecId::Audio(CODEC_ID_ATRAC3),
        "A_REAL/14_4" => CodecId::Audio(CODEC_ID_RA10),
        "A_REAL/28_8" => CodecId::Audio(CODEC_ID_RA20),
        "A_REAL/COOK" => CodecId::Audio(CODEC_ID_COOK),
        "A_REAL/SIPR" => CodecId::Audio(CODEC_ID_SIPR),
        "A_REAL/RALF" => CodecId::Audio(CODEC_ID_RALF),
        "A_PCM/INT/BIG" => match bit_depth?.get() {
            16 => CodecId::Audio(CODEC_ID_PCM_S16BE),
            24 => CodecId::Audio(CODEC_ID_PCM_S24BE),
            32 => CodecId::Audio(CODEC_ID_PCM_S32BE),
            _ => return None,
        },
        "A_PCM/INT/LIT" => match bit_depth?.get() {
            16 => CodecId::Audio(CODEC_ID_PCM_S16LE),
            24 => CodecId::Audio(CODEC_ID_PCM_S24LE),
            32 => CodecId::Audio(CODEC_ID_PCM_S32LE),
            _ => return None,
        },
        "A_PCM/FLOAT/IEEE" => match bit_depth?.get() {
            32 => CodecId::Audio(CODEC_ID_PCM_F32LE),
            64 => CodecId::Audio(CODEC_ID_PCM_F64LE),
            _ => return None,
        },
        // A_MS/ACM
        // A_QUICKTIME
        // A_QUICKTIME/QDMC
        // A_QUICKTIME/QDM2

        // Video Codecs
        "V_MJPEG" => CodecId::Video(CODEC_ID_MJPEG),
        "V_MPEG4/MS/V3" => CodecId::Video(CODEC_ID_MSMPEG4V3),
        "V_MPEG1" => CodecId::Video(CODEC_ID_MPEG1),
        "V_MPEG2" => CodecId::Video(CODEC_ID_MPEG2),
        "V_MPEG4/ISO/SP" | "V_MPEG4/ISO/ASP" => CodecId::Video(CODEC_ID_MPEG4),
        "V_MPEG4/ISO/AVC" | "V_MPEG4/ISO/AP" => CodecId::Video(CODEC_ID_H264),
        "V_MPEGH/ISO/HEVC" => CodecId::Video(CODEC_ID_HEVC),
        "V_REAL/RV10" => CodecId::Video(CODEC_ID_RV10),
        "V_REAL/RV20" => CodecId::Video(CODEC_ID_RV20),
        "V_REAL/RV30" => CodecId::Video(CODEC_ID_RV30),
        "V_REAL/RV40" => CodecId::Video(CODEC_ID_RV40),
        "V_THEORA" => CodecId::Video(CODEC_ID_THEORA),
        "V_VP8" => CodecId::Video(CODEC_ID_VP8),
        "V_VP9" => CodecId::Video(CODEC_ID_VP9),
        "V_AV1" => CodecId::Video(CODEC_ID_AV1),
        "V_AVS2" => CodecId::Video(CODEC_ID_AVS2),
        "V_AVS3" => CodecId::Video(CODEC_ID_AVS3),
        // V_MS/VFW/FOURCC WVC1 (VC-1)
        // V_UNCOMPRESSED
        // V_QUICKTIME
        // V_PRORES
        // V_FFV1

        // Subtitle Codecs
        "S_TEXT/UTF8" => CodecId::Subtitle(CODEC_ID_TEXT_UTF8),
        "S_TEXT/SSA" => CodecId::Subtitle(CODEC_ID_SSA),
        "S_TEXT/ASS" => CodecId::Subtitle(CODEC_ID_ASS),
        "S_TEXT/WEBVTT" => CodecId::Subtitle(CODEC_ID_WEBVTT),
        "S_IMAGE/BMP" => CodecId::Subtitle(CODEC_ID_BMP),
        "S_VOBSUB" => CodecId::Subtitle(CODEC_ID_VOBSUB),
        "S_DVBSUB" => CodecId::Subtitle(CODEC_ID_DVBSUB),
        "S_HDMV/PGS" => CodecId::Subtitle(CODEC_ID_HDMV_PGS),
        "S_HDMV/TEXTST" => CodecId::Subtitle(CODEC_ID_HDMV_TEXTST),
        "S_KATE" => CodecId::Subtitle(CODEC_ID_KATE),
        // S_ARIBSUB

        // Other Codecs
        _ => {
            log::info!("unknown codec: {}", track.codec_id);
            return None;
        }
    };

    Some(codec_id)
}

fn get_codec_profile_and_level(track: &TrackElement) -> (Option<CodecProfile>, Option<u32>) {
    use symphonia_core::codecs::audio::well_known::profiles::{
        CODEC_PROFILE_AAC_HE, CODEC_PROFILE_AAC_LC, CODEC_PROFILE_AAC_LTP, CODEC_PROFILE_AAC_MAIN,
        CODEC_PROFILE_AAC_SSR,
    };
    use symphonia_core::codecs::video::well_known::profiles::{
        CODEC_PROFILE_MPEG4_ADVANCED_SIMPLE, CODEC_PROFILE_MPEG4_SIMPLE,
    };

    match track.codec_id.as_str() {
        // Audio Codecs
        "A_AAC" | "A_AAC/MPEG2/LC" | "A_AAC/MPEG4/LC" => (Some(CODEC_PROFILE_AAC_LC), None),
        "A_AAC/MPEG2/MAIN" | "A_AAC/MPEG4/MAIN" => (Some(CODEC_PROFILE_AAC_MAIN), None),
        "A_AAC/MPEG2/LC/SBR" | "A_AAC/MPEG4/LC/SBR" => (Some(CODEC_PROFILE_AAC_HE), None),
        "A_AAC/MPEG2/SSR" | "A_AAC/MPEG4/SSR" => (Some(CODEC_PROFILE_AAC_SSR), None),
        "A_AAC/MPEG4/LTP" => (Some(CODEC_PROFILE_AAC_LTP), None),

        // Video Codecs
        "V_MPEG4/ISO/SP" => (Some(CODEC_PROFILE_MPEG4_SIMPLE), None),
        "V_MPEG4/ISO/ASP" => (Some(CODEC_PROFILE_MPEG4_ADVANCED_SIMPLE), None),
        "V_MPEG4/ISO/AVC" | "V_MPEG4/ISO/AP" => {
            // Parse AVCDecoderConfigurationRecord extra data to acquire the profile and level.
            track
                .codec_private
                .as_ref()
                .and_then(|buf| AVCDecoderConfigurationRecord::read(buf).ok())
                .map(|cfg| (Some(cfg.profile), Some(cfg.level)))
                .unwrap_or_else(|| (None, None))
        }
        "V_MPEGH/ISO/HEVC" => {
            // Parse HevcDecoderConfigurationRecord extra data to acquire the profile and level.
            track
                .codec_private
                .as_ref()
                .and_then(|buf| HEVCDecoderConfigurationRecord::read(buf).ok())
                .map(|cfg| (Some(cfg.profile), Some(cfg.level)))
                .unwrap_or_else(|| (None, None))
        }

        // Other Codecs
        _ => (None, None),
    }
}
