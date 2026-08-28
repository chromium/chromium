// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use core::str;
use std::fmt;
use std::num::NonZero;
use std::sync::Arc;

use log::debug;
use symphonia_core::audio::{Channels, layouts};
use symphonia_core::codecs::audio::well_known::{
    CODEC_ID_PCM_ALAW, CODEC_ID_PCM_F32BE, CODEC_ID_PCM_F64BE, CODEC_ID_PCM_MULAW, CODEC_ID_PCM_S8,
    CODEC_ID_PCM_S16BE, CODEC_ID_PCM_S16LE, CODEC_ID_PCM_S24BE, CODEC_ID_PCM_S32BE,
    CODEC_ID_PCM_S32LE, CODEC_ID_PCM_U8,
};
use symphonia_core::errors::{Result, decode_error, unsupported_error};
use symphonia_core::io::ReadBytes;
use symphonia_core::meta::{MetadataRevision, StandardTag, Tag};
use symphonia_core::util::text;
use symphonia_metadata::embedded::riff;

use crate::common::{
    ChunkParser, FormatALaw, FormatData, FormatIeeeFloat, FormatMuLaw, FormatPcm, PacketInfo,
    ParseChunk, ParseChunkTag,
};

use extended::Extended;

/// `CommonChunk` is a required AIFF chunk, containing metadata.
pub struct CommonChunk {
    /// The number of channels.
    pub num_channels: u16,
    /// The number of audio frames.
    #[allow(dead_code)]
    pub num_sample_frames: u32,
    /// The sample size in bits.
    pub sample_size: u16,
    /// The sample rate in Hz.
    pub sample_rate: NonZero<u32>,
    /// Extra data associated with the format block conditional upon the format tag.
    pub format_data: FormatData,
}

impl CommonChunk {
    fn read_pcm_fmt(valid_bits_per_sample: u16, num_channels: u16) -> Result<FormatData> {
        // Sample sizes that are not a multiple of 8 bits are rounded-up to the nearest byte. The
        // data is left justified. Therefore, these cases are essentially equivalent to if the
        // samples were stored with a sample size that was a multiple of 8 bits to begin with.
        let (codec, bits_per_sample) = match valid_bits_per_sample {
            1..=8 => (CODEC_ID_PCM_S8, 8),
            9..=16 => (CODEC_ID_PCM_S16BE, 16),
            17..=24 => (CODEC_ID_PCM_S24BE, 24),
            25..=32 => (CODEC_ID_PCM_S32BE, 32),
            _ => return decode_error("aiff: bits per sample for pcm must be between 1-32 bits"),
        };

        let channels = map_aiff_channel_count(num_channels)?;
        Ok(FormatData::Pcm(FormatPcm { bits_per_sample, valid_bits_per_sample, channels, codec }))
    }

    fn read_alaw_pcm_fmt(bits_per_sample: u16, num_channels: u16) -> Result<FormatData> {
        if bits_per_sample != 16 {
            debug!("bits per sample not 16 for alaw");
        }
        let channels = map_aiff_channel_count(num_channels)?;
        Ok(FormatData::ALaw(FormatALaw { codec: CODEC_ID_PCM_ALAW, channels }))
    }

    fn read_mulaw_pcm_fmt(bits_per_sample: u16, num_channels: u16) -> Result<FormatData> {
        if bits_per_sample != 16 {
            debug!("bits per sample not 16 for u-law");
        }
        let channels = map_aiff_channel_count(num_channels)?;
        Ok(FormatData::MuLaw(FormatMuLaw { codec: CODEC_ID_PCM_MULAW, channels }))
    }

    fn read_in24_fmt(bits_per_sample: u16, num_channels: u16) -> Result<FormatData> {
        if bits_per_sample != 24 {
            return decode_error("aifc: bits per sample invalid for in14");
        }
        let channels = map_aiff_channel_count(num_channels)?;
        Ok(FormatData::Pcm(FormatPcm {
            bits_per_sample,
            valid_bits_per_sample: bits_per_sample,
            channels,
            codec: CODEC_ID_PCM_S24BE,
        }))
    }

    fn read_in32_fmt(bits_per_sample: u16, num_channels: u16) -> Result<FormatData> {
        if bits_per_sample != 32 {
            return decode_error("aifc: bits per sample invalid for in32");
        }
        let channels = map_aiff_channel_count(num_channels)?;
        Ok(FormatData::Pcm(FormatPcm {
            bits_per_sample,
            valid_bits_per_sample: bits_per_sample,
            channels,
            codec: CODEC_ID_PCM_S32BE,
        }))
    }

    fn read_23ni_fmt(bits_per_sample: u16, num_channels: u16) -> Result<FormatData> {
        if bits_per_sample != 32 {
            return decode_error("aifc: bits per sample invalid for 23ni");
        }
        let channels = map_aiff_channel_count(num_channels)?;
        Ok(FormatData::Pcm(FormatPcm {
            bits_per_sample,
            valid_bits_per_sample: bits_per_sample,
            channels,
            codec: CODEC_ID_PCM_S32LE,
        }))
    }

    fn read_fl32_fmt(bits_per_sample: u16, num_channels: u16) -> Result<FormatData> {
        if bits_per_sample != 32 {
            debug!("bits per sample is not 32 for fl32 format");
        }

        let channels = map_aiff_channel_count(num_channels)?;
        Ok(FormatData::IeeeFloat(FormatIeeeFloat { channels, codec: CODEC_ID_PCM_F32BE }))
    }

    fn read_fl64_fmt(bits_per_sample: u16, num_channels: u16) -> Result<FormatData> {
        if bits_per_sample != 64 {
            debug!("bits per sample is not 64 for fl64 format");
        }

        let channels = map_aiff_channel_count(num_channels)?;
        Ok(FormatData::IeeeFloat(FormatIeeeFloat { channels, codec: CODEC_ID_PCM_F64BE }))
    }

    fn read_sowt_fmt(bits_per_sample: u16, num_channels: u16) -> Result<FormatData> {
        let codec = match bits_per_sample {
            16 => CODEC_ID_PCM_S16LE,
            _ => return decode_error("aiff: bits per sample for sowt must be 16 bits"),
        };

        let channels = map_aiff_channel_count(num_channels)?;
        Ok(FormatData::Pcm(FormatPcm {
            bits_per_sample,
            valid_bits_per_sample: bits_per_sample,
            channels,
            codec,
        }))
    }

    fn read_twos_fmt(bits_per_sample: u16, num_channels: u16) -> Result<FormatData> {
        let codec = match bits_per_sample {
            16 => CODEC_ID_PCM_S16BE,
            _ => return decode_error("aiff: bits per sample for twos must be 16 bits"),
        };

        let channels = map_aiff_channel_count(num_channels)?;
        Ok(FormatData::Pcm(FormatPcm {
            bits_per_sample,
            valid_bits_per_sample: bits_per_sample,
            channels,
            codec,
        }))
    }

    fn read_raw_fmt(bits_per_sample: u16, num_channels: u16) -> Result<FormatData> {
        let codec = match bits_per_sample {
            8 => CODEC_ID_PCM_U8,
            _ => return decode_error("aiff: bits per sample for raw must be 8 bits"),
        };

        let channels = map_aiff_channel_count(num_channels)?;
        Ok(FormatData::Pcm(FormatPcm {
            bits_per_sample,
            valid_bits_per_sample: bits_per_sample,
            channels,
            codec,
        }))
    }

    pub fn packet_info(&self) -> Result<PacketInfo> {
        match &self.format_data {
            FormatData::Pcm(pcm) => pcm.make_packet_info(),
            FormatData::ALaw(alaw) => alaw.make_packet_info(),
            FormatData::MuLaw(mulaw) => mulaw.make_packet_info(),
            FormatData::IeeeFloat(ieee) => ieee.make_packet_info(),
            FormatData::Extensible(_) => {
                unsupported_error("aiff: packet info not implemented for format Extensible")
            }
            FormatData::Adpcm(_) => {
                unsupported_error("aiff: packet info not implemented for format Adpcm")
            }
        }
    }
}

impl ParseChunk for CommonChunk {
    fn parse<B: ReadBytes>(reader: &mut B, _tag: [u8; 4], _: u32) -> Result<CommonChunk> {
        let num_channels = reader.read_be_u16()?;
        let num_sample_frames = reader.read_be_u32()?;
        let sample_size = reader.read_be_u16()?;
        let sample_rate = read_sample_rate(reader)?;
        let format_data = Self::read_pcm_fmt(sample_size, num_channels)?;

        Ok(CommonChunk { num_channels, num_sample_frames, sample_size, sample_rate, format_data })
    }
}

impl fmt::Display for CommonChunk {
    //TODO: perhaps place this in riff.rs to share with wave etc
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "CommonChunk {{")?;
        writeln!(f, "\tnum_channels: {},", self.num_channels)?;
        writeln!(f, "\tsample_rate: {} Hz,", self.sample_rate)?;
        writeln!(f, "\tsample_size: {},", self.sample_size)?;

        match self.format_data {
            FormatData::Pcm(ref pcm) => {
                writeln!(f, "\tformat_data: Pcm {{")?;
                writeln!(f, "\t\tbits_per_sample: {},", pcm.bits_per_sample)?;
                writeln!(f, "\t\tvalid_bits_per_sample: {},", pcm.valid_bits_per_sample)?;
                writeln!(f, "\t\tchannels: {},", pcm.channels)?;
                writeln!(f, "\t\tcodec: {},", pcm.codec)?;
            }
            FormatData::ALaw(ref alaw) => {
                writeln!(f, "\tformat_data: MuLaw {{")?;
                writeln!(f, "\t\tchannels: {},", alaw.channels)?;
                writeln!(f, "\t\tcodec: {},", alaw.codec)?;
            }
            FormatData::MuLaw(ref mulaw) => {
                writeln!(f, "\tformat_data: MuLaw {{")?;
                writeln!(f, "\t\tchannels: {},", mulaw.channels)?;
                writeln!(f, "\t\tcodec: {},", mulaw.codec)?;
            }
            FormatData::IeeeFloat(ref ieee) => {
                writeln!(f, "\tformat_data: IeeeFloat {{")?;
                writeln!(f, "\t\tchannels: {},", ieee.channels)?;
                writeln!(f, "\t\tcodec: {},", ieee.codec)?;
            }
            FormatData::Extensible(_) => {
                writeln!(f, "\tformat_data: Extensible DISPLAY UNSUPPORTED {{")?;
            }
            FormatData::Adpcm(_) => {
                writeln!(f, "\tformat_data: Adpcm DISPLAY UNSUPPORTED {{")?;
            }
        };

        writeln!(f, "\t}}")?;
        writeln!(f, "}}")
    }
}

pub trait CommonChunkParser {
    fn parse_aiff<B: ReadBytes>(self, reader: &mut B) -> Result<CommonChunk>;
    fn parse_aifc<B: ReadBytes>(self, reader: &mut B) -> Result<CommonChunk>;
}

impl CommonChunkParser for ChunkParser<CommonChunk> {
    fn parse_aiff<B: ReadBytes>(self, reader: &mut B) -> Result<CommonChunk> {
        self.parse(reader)
    }

    fn parse_aifc<B: ReadBytes>(self, reader: &mut B) -> Result<CommonChunk> {
        let num_channels = reader.read_be_u16()?;
        let num_sample_frames = reader.read_be_u32()?;
        let sample_size = reader.read_be_u16()?;
        let sample_rate = read_sample_rate(reader)?;
        let compression_type = reader.read_quad_bytes()?;

        // Ignore the compression_name pascal string.
        ignore_pascal_string(reader)?;

        let format_data = match &compression_type {
            b"none" | b"NONE" => CommonChunk::read_pcm_fmt(sample_size, num_channels),
            b"alaw" | b"ALAW" => CommonChunk::read_alaw_pcm_fmt(sample_size, num_channels),
            b"ulaw" | b"ULAW" => CommonChunk::read_mulaw_pcm_fmt(sample_size, num_channels),
            b"in24" | b"IN24" => CommonChunk::read_in24_fmt(sample_size, num_channels),
            b"in32" | b"IN32" => CommonChunk::read_in32_fmt(sample_size, num_channels),
            b"23ni" | b"23NI" => CommonChunk::read_23ni_fmt(sample_size, num_channels),
            b"fl32" | b"FL32" => CommonChunk::read_fl32_fmt(sample_size, num_channels),
            b"fl64" | b"FL64" => CommonChunk::read_fl64_fmt(sample_size, num_channels),
            b"sowt" | b"SOWT" => CommonChunk::read_sowt_fmt(sample_size, num_channels),
            b"twos" | b"TWOS" => CommonChunk::read_twos_fmt(sample_size, num_channels),
            b"raw " | b"RAW " => CommonChunk::read_raw_fmt(sample_size, num_channels),
            _ => return unsupported_error("aifc: compression type not supported"),
        }?;

        Ok(CommonChunk { num_channels, num_sample_frames, sample_size, sample_rate, format_data })
    }
}

/// `SoundChunk` is a required AIFF chunk, containing the audio data.
pub struct SoundChunk {
    pub len: Option<u32>,
    #[allow(dead_code)]
    pub offset: u32,
    #[allow(dead_code)]
    pub block_size: u32,
    pub data_start_pos: u64,
}

impl SoundChunk {
    /// Create an empty sound chunk starting at the specified data start position.
    pub fn empty(data_start_pos: u64) -> Self {
        SoundChunk { len: Some(0), offset: 0, block_size: 0, data_start_pos }
    }
}

impl ParseChunk for SoundChunk {
    fn parse<B: ReadBytes>(reader: &mut B, _: [u8; 4], len: u32) -> Result<SoundChunk> {
        // Validate minimum size.
        if len < 8 {
            return decode_error("aiff: invalid chunk size for sound chunk");
        }

        let offset = reader.read_be_u32()?;
        let block_size = reader.read_be_u32()?;

        if block_size != 0 {
            return unsupported_error("aiff: no support for aiff block-aligned data");
        }

        if offset > len - 8 {
            return decode_error("aiff: sound data offset too large");
        }

        reader.ignore_bytes(u64::from(offset))?;

        let data_start_pos = reader.pos();

        // TODO: FFmpeg seems to set the chunk length to 0 when streaming. This, however, doesn't
        // appear to be well supported, event by FFmpeg.
        Ok(SoundChunk { len: Some(len - offset - 8), offset, block_size, data_start_pos })
    }
}

pub struct MarkerChunk {
    pub markers: Vec<Marker>,
}

pub struct Marker {
    pub id: i16,
    pub ts: u32,
    pub name: String,
}

impl ParseChunk for MarkerChunk {
    fn parse<B: ReadBytes>(reader: &mut B, _tag: [u8; 4], _len: u32) -> Result<Self> {
        let num_markers = reader.read_be_u16()?;

        let mut markers = Vec::with_capacity(usize::from(num_markers));

        for _ in 0..num_markers {
            let id = reader.read_be_i16()?;
            let ts = reader.read_be_u32()?;
            let name = read_pascal_string(reader)?;

            markers.push(Marker { id, ts, name });
        }

        Ok(MarkerChunk { markers })
    }
}

pub struct AppSpecificChunk {
    pub application: String,
    pub data: Box<[u8]>,
}

impl ParseChunk for AppSpecificChunk {
    fn parse<B: ReadBytes>(reader: &mut B, _tag: [u8; 4], len: u32) -> Result<Self> {
        let start_pos = reader.pos();

        // The application signature.
        let signature = reader.read_quad_bytes()?;

        // If the signature is "pdos", an application name is present before the app-specific data.
        let application = match &signature {
            b"pdos" => read_pascal_string(reader)?,
            _ => format!("{:x}", u32::from_be_bytes(signature)),
        };

        // The remainder of the chunk is the app-specific data.
        let consumed = (reader.pos() - start_pos) as u32;

        if consumed > len {
            return decode_error("aiff: malformed application-specific chunk");
        }

        let data = reader.read_boxed_slice_exact((len - consumed) as usize)?;

        Ok(AppSpecificChunk { application, data })
    }
}

pub struct CommentsChunk {
    pub comments: Vec<Comment>,
}

pub struct Comment {
    /// Comment creation timestamp.
    #[allow(dead_code)]
    pub timestamp: u32,
    pub marker_id: i16,
    pub text: String,
}

impl ParseChunk for CommentsChunk {
    fn parse<B: ReadBytes>(reader: &mut B, _tag: [u8; 4], _len: u32) -> Result<Self> {
        let num_comments = reader.read_be_u16()?;

        let mut comments = Vec::with_capacity(usize::from(num_comments));

        for _ in 0..num_comments {
            let timestamp = reader.read_be_u32()?;
            let marker_id = reader.read_be_i16()?;
            let len = reader.read_be_u16()?;
            let buf = reader.read_boxed_slice_exact(usize::from(len))?;

            comments.push(Comment { timestamp, marker_id, text: decode_string(&buf) });
        }

        Ok(CommentsChunk { comments })
    }
}

pub struct TextChunk {
    pub tag: Tag,
}

impl ParseChunk for TextChunk {
    fn parse<B: ReadBytes>(reader: &mut B, tag: [u8; 4], len: u32) -> Result<Self> {
        let text = reader.read_boxed_slice_exact(len as usize)?;

        let value = Arc::new(decode_string(&text));

        let std_tag = match &tag {
            b"NAME" => StandardTag::TrackTitle(value.clone()),
            b"AUTH" => StandardTag::Encoder(value.clone()),
            b"(c) " => StandardTag::Copyright(value.clone()),
            b"ANNO" => StandardTag::Comment(value.clone()),
            _ => unreachable!(),
        };

        let tag_key = match str::from_utf8(&tag) {
            Ok(s) => s,
            Err(_) => return decode_error("aiff: tag key is not valid UTF-8"),
        };
        let tag = Tag::new_from_parts(tag_key, value, Some(std_tag));

        Ok(TextChunk { tag })
    }
}

pub struct Id3Chunk {
    pub metadata: MetadataRevision,
}

impl ParseChunk for Id3Chunk {
    fn parse<B: ReadBytes>(reader: &mut B, _tag: [u8; 4], _len: u32) -> Result<Self> {
        let mut side_data = Vec::new();
        let metadata = riff::read_riff_id3_chunk(reader, &mut side_data)?;
        Ok(Id3Chunk { metadata })
    }
}

pub enum RiffAiffChunks {
    Common(ChunkParser<CommonChunk>),
    Sound(ChunkParser<SoundChunk>),
    Marker(ChunkParser<MarkerChunk>),
    AppSpecific(ChunkParser<AppSpecificChunk>),
    Comments(ChunkParser<CommentsChunk>),
    Text(ChunkParser<TextChunk>),
    Id3(ChunkParser<Id3Chunk>),
}

macro_rules! parser {
    ($class:expr, $result:ty, $tag:expr, $len:expr) => {
        Some($class(ChunkParser::<$result>::new($tag, $len)))
    };
}

impl ParseChunkTag for RiffAiffChunks {
    fn parse_tag(tag: [u8; 4], len: u32) -> Option<Self> {
        match &tag {
            b"COMM" => parser!(RiffAiffChunks::Common, CommonChunk, tag, len),
            b"SSND" => parser!(RiffAiffChunks::Sound, SoundChunk, tag, len),
            b"MARK" => parser!(RiffAiffChunks::Marker, MarkerChunk, tag, len),
            b"APPL" => parser!(RiffAiffChunks::AppSpecific, AppSpecificChunk, tag, len),
            b"COMT" => parser!(RiffAiffChunks::Comments, CommentsChunk, tag, len),
            b"NAME" | b"AUTH" | b"(c) " | b"ANNO" => {
                parser!(RiffAiffChunks::Text, TextChunk, tag, len)
            }
            b"ID3 " => parser!(RiffAiffChunks::Id3, Id3Chunk, tag, len),
            _ => None,
        }
    }
}

fn read_sample_rate<B: ReadBytes>(reader: &mut B) -> Result<NonZero<u32>> {
    let mut buf: [u8; 10] = [0; 10];
    reader.read_buf_exact(&mut buf)?;

    let sample_rate_f64 = Extended::from_be_bytes(buf).to_f64();

    // Do not allow infinite or NaN sample rates.
    if sample_rate_f64.is_infinite() || sample_rate_f64.is_nan() {
        return decode_error("aiff: sample rate is not a real number");
    }

    // Do not allow a 0 Hz sample rates.
    let sample_rate = match NonZero::new(sample_rate_f64 as u32) {
        Some(sample_rate) => sample_rate,
        _ => return decode_error("aiff: sample rate cannot be 0"),
    };

    Ok(sample_rate)
}

fn ignore_pascal_string<B: ReadBytes>(reader: &mut B) -> Result<()> {
    let mut len = u64::from(reader.read_byte()?);
    if len & 1 == 0 {
        len += 1;
    }
    Ok(reader.ignore_bytes(len)?)
}

fn read_pascal_string<B: ReadBytes>(reader: &mut B) -> Result<String> {
    let len = reader.read_byte()?;
    let value = reader.read_boxed_slice_exact(usize::from(len))?;

    // If the length of the string data is even, then the total length of the pascal string would be
    // odd with the length byte. Read an additional padding byte such that the pascal string is an
    // even length in total.
    if len & 1 == 0 {
        let _ = reader.read_byte()?;
    }

    Ok(decode_string(&value))
}

fn decode_string(data: &[u8]) -> String {
    // Stop at a null-terminator. Control character usage is undefined in AIFF, so preserve them.
    text::decode_iso8859_1_lossy(data).take_while(text::filter::not_null).collect()
}

fn map_aiff_channel_count(count: u16) -> Result<Channels> {
    let channels = match count {
        0 => return decode_error("aiff: invalid channel count"),
        1 => layouts::CHANNEL_LAYOUT_MONO,
        2 => layouts::CHANNEL_LAYOUT_STEREO,
        3 => layouts::CHANNEL_LAYOUT_3P0,
        // Channel layouts consisting of more than 3 channels are poorly defined, or have
        // conflicting definitions. Treat these cases as discrete channels.
        _ => Channels::Discrete(count),
    };
    Ok(channels)
}
