// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

/// `PacketInfo` helps to simulate packetization over a number of blocks of data.
/// In case the codec is blockless the block size equals one full audio frame in bytes.
use std::marker::PhantomData;
use std::num::NonZero;

use symphonia_core::audio::Channels;
use symphonia_core::codecs::audio::well_known::{
    CODEC_ID_ADPCM_IMA_WAV, CODEC_ID_ADPCM_MS, CODEC_ID_PCM_F32BE, CODEC_ID_PCM_F32LE,
    CODEC_ID_PCM_F64BE, CODEC_ID_PCM_F64LE,
};
use symphonia_core::codecs::audio::{AudioCodecId, AudioCodecParameters};
use symphonia_core::errors::{Result, decode_error};
use symphonia_core::formats::prelude::*;
use symphonia_core::io::{MediaSourceStream, ReadBytes};

use log::{debug, info};

pub enum ByteOrder {
    LittleEndian,
    BigEndian,
}

/// The maximum number of frames that will be in a packet.
/// Since there are no real packets in AIFF, this is arbitrary, used same value as MP3.
const MAX_FRAMES_PER_PACKET: NonZero<u64> = NonZero::new(1152).unwrap();

/// `ParseChunkTag` implements `parse_tag` to map between the 4-byte chunk identifier and the
/// enumeration
pub trait ParseChunkTag: Sized {
    fn parse_tag(tag: [u8; 4], len: u32) -> Option<Self>;
}

pub enum NullChunks {}

impl ParseChunkTag for NullChunks {
    fn parse_tag(_tag: [u8; 4], _len: u32) -> Option<Self> {
        None
    }
}

/// `ChunksReader` reads chunks from a `ByteStream`. It is generic across a type, usually an enum,
/// implementing the `ParseChunkTag` trait. When a new chunk is encountered in the stream,
/// `parse_tag` on T is called to return an object capable of parsing/reading that chunk or `None`.
/// This makes reading the actual chunk data lazy in that the  chunk is not read until the object is
/// consumed.
pub struct ChunksReader<T: ParseChunkTag> {
    len: Option<u64>,
    byte_order: ByteOrder,
    consumed: u64,
    phantom: PhantomData<T>,
}

impl<T: ParseChunkTag> ChunksReader<T> {
    pub fn new(len: Option<u32>, byte_order: ByteOrder) -> Self {
        ChunksReader { len: len.map(u64::from), byte_order, consumed: 0, phantom: PhantomData }
    }

    pub fn next<B: ReadBytes>(&mut self, reader: &mut B) -> Result<Option<T>> {
        // Loop until a chunk is recognized and returned, or the end of stream is reached.
        loop {
            // Check if at the end of the parent chunk.
            if let Some(len) = self.len {
                if self.consumed >= len {
                    return Ok(None);
                }
            }

            // Align to the next 2-byte boundary if not currently aligned.
            if self.consumed & 0x1 == 1 {
                reader.read_u8()?;
                self.consumed += 1;
            }

            // Check if there are enough bytes (8) to read a chunk header. If not, there are no more
            // chunks to be read.
            if let Some(len) = self.len {
                if self.consumed + 8 > len {
                    return Ok(None);
                }
            }

            // Read chunk tag and length (the chunk header).
            let tag = reader.read_quad_bytes()?;

            let chunk_len = match self.byte_order {
                ByteOrder::LittleEndian => reader.read_u32()?,
                ByteOrder::BigEndian => reader.read_be_u32()?,
            };

            self.consumed += 8;

            // Check if the ChunkReader has enough unread bytes to fully read the chunk body.
            if let Some(len) = self.len {
                // Warning: The formulation of this conditional is critical because chunk_len is an
                // untrusted input, it may overflow when if added to anything.
                if len - self.consumed < u64::from(chunk_len) {
                    debug!(
                        "chunk length of {} exceeds parent (list) chunk length",
                        String::from_utf8_lossy(&tag)
                    );
                    return decode_error("riff: chunk length exceeds parent (list) chunk length");
                }
            }

            // The length of the chunk has been validated, so "consume" the chunk.
            self.consumed = self.consumed.saturating_add(u64::from(chunk_len));

            match T::parse_tag(tag, chunk_len) {
                Some(chunk) => return Ok(Some(chunk)),
                None => {
                    // As per the RIFF spec, unknown chunks are to be ignored.
                    info!(
                        "ignoring unknown chunk: tag={}, len={}.",
                        String::from_utf8_lossy(&tag),
                        chunk_len
                    );

                    reader.ignore_bytes(u64::from(chunk_len))?
                }
            }
        }
    }
    pub fn finish<B: ReadBytes>(&mut self, reader: &mut B) -> Result<()> {
        // If data is remaining in the parent chunk, skip it.
        if let Some(parent_len) = self.len {
            if self.consumed < parent_len {
                let remaining = parent_len - self.consumed;
                reader.ignore_bytes(remaining)?;
                self.consumed += remaining;
            }

            // Pad the chunk to the next 2-byte boundary.
            if parent_len & 0x1 == 1 {
                reader.read_u8()?;
            }
        }

        Ok(())
    }
}

/// Common trait implemented for all chunks that are parsed by a `ChunkParser`.
pub trait ParseChunk: Sized {
    /// Parse the chunk.
    fn parse<B: ReadBytes>(reader: &mut B, tag: [u8; 4], len: u32) -> Result<Self>;

    /// Parse the chunk and discard any unread data in the chunk.
    fn parse_and_skip_unread<B: ReadBytes>(reader: &mut B, tag: [u8; 4], len: u32) -> Result<Self> {
        let pos = reader.pos();
        let result = Self::parse(reader, tag, len);

        // Only discard unread data if parsing was successful.
        if result.is_ok() {
            let bytes_read = reader.pos() - pos;
            if bytes_read < u64::from(len) {
                reader.ignore_bytes(u64::from(len) - bytes_read)?;
            }
        }

        result
    }
}

/// `ChunkParser` is a utility struct for unifying the parsing of chunks.
pub struct ChunkParser<P: ParseChunk> {
    tag: [u8; 4],
    pub len: u32,
    phantom: PhantomData<P>,
}

impl<P: ParseChunk> ChunkParser<P> {
    pub fn new(tag: [u8; 4], len: u32) -> Self {
        ChunkParser { tag, len, phantom: PhantomData }
    }

    pub fn parse<B: ReadBytes>(&self, reader: &mut B) -> Result<P> {
        P::parse(reader, self.tag, self.len)
    }

    pub fn parse_and_skip_unread<B: ReadBytes>(&self, reader: &mut B) -> Result<P> {
        P::parse_and_skip_unread(reader, self.tag, self.len)
    }
}

pub enum FormatData {
    Pcm(FormatPcm),
    Adpcm(FormatAdpcm),
    IeeeFloat(FormatIeeeFloat),
    Extensible(FormatExtensible),
    ALaw(FormatALaw),
    MuLaw(FormatMuLaw),
}

pub struct FormatPcm {
    /// The number of bits per sample as written.
    pub bits_per_sample: u16,
    /// The number of bits per sample that are valid. Must be <= `bits_per_sample`.
    pub valid_bits_per_sample: u16,
    /// Channel bitmask.
    pub channels: Channels,
    /// Codec ID.
    pub codec: AudioCodecId,
}

impl FormatPcm {
    pub fn make_packet_info(&self) -> Result<PacketInfo> {
        let num_channels = self.channels.count() as u16;
        let frame_len = (self.bits_per_sample / 8) * num_channels;
        PacketInfo::without_blocks(u32::from(frame_len))
    }
}

pub struct FormatAdpcm {
    pub block_align: u16,
    /// The number of bits per sample. At the moment only 4bit is supported.
    pub bits_per_sample: u16,
    /// Channel bitmask.
    pub channels: Channels,
    /// Codec ID.
    pub codec: AudioCodecId,
}

impl FormatAdpcm {
    pub fn make_packet_info(&self) -> Result<PacketInfo> {
        let num_channels = self.channels.count() as u16;
        let bits_per_frame = (self.bits_per_sample * num_channels) as u64;

        match self.codec {
            CODEC_ID_ADPCM_MS => {
                // Don't allow underflow to occur.
                if 7 * num_channels > self.block_align {
                    return decode_error("riff (adpcm_ms): block align too small");
                }
                let frames_per_block =
                    (8 * u64::from(self.block_align - 7 * num_channels)) / bits_per_frame + 2;
                PacketInfo::with_blocks(self.block_align, frames_per_block)
            }
            CODEC_ID_ADPCM_IMA_WAV => {
                // Don't allow underflow to occur.
                if 4 * num_channels > self.block_align {
                    return decode_error("riff (adpcm_ima_wav): block align too small");
                }
                let frames_per_block =
                    (8 * u64::from(self.block_align - 4 * num_channels)) / bits_per_frame + 1;
                PacketInfo::with_blocks(self.block_align, frames_per_block)
            }
            _ => unimplemented!("incomplete new adpcm codec"),
        }
    }
}

pub struct FormatIeeeFloat {
    /// Channel bitmask.
    pub channels: Channels,
    /// Codec ID.
    pub codec: AudioCodecId,
}

impl FormatIeeeFloat {
    pub fn make_packet_info(&self) -> Result<PacketInfo> {
        let num_channels = self.channels.count() as u32;
        let bytes_per_sample = match self.codec {
            CODEC_ID_PCM_F32LE | CODEC_ID_PCM_F32BE => 4,
            CODEC_ID_PCM_F64LE | CODEC_ID_PCM_F64BE => 8,
            _ => unreachable!(),
        };
        let frame_len = num_channels * bytes_per_sample;
        PacketInfo::without_blocks(frame_len)
    }
}

pub struct FormatExtensible {
    /// The number of bits per sample as written. This value is always a multiple of 8-bits.
    pub bits_per_sample: u16,
    /// The number of bits per sample that are valid. This number is always less than the number
    /// of bits per sample.
    pub valid_bits_per_sample: u16,
    /// Channel bitmask.
    pub channels: Channels,
    /// Globally unique identifier of the format.
    pub sub_format_guid: [u8; 16],
    /// Codec ID.
    pub codec: AudioCodecId,
}

impl FormatExtensible {
    pub fn make_packet_info(&self) -> Result<PacketInfo> {
        // NOTE: This is only valid for compressed sub-types.
        let num_channels = self.channels.count() as u16;
        let frame_len = (self.bits_per_sample / 8) * num_channels;
        PacketInfo::without_blocks(u32::from(frame_len))
    }
}

pub struct FormatALaw {
    /// Channel bitmask.
    pub channels: Channels,
    /// Codec ID.
    pub codec: AudioCodecId,
}

impl FormatALaw {
    pub fn make_packet_info(&self) -> Result<PacketInfo> {
        let num_channels = self.channels.count() as u32;
        PacketInfo::without_blocks(num_channels)
    }
}

pub struct FormatMuLaw {
    /// Channel bitmask.
    pub channels: Channels,
    /// Codec ID.
    pub codec: AudioCodecId,
}

impl FormatMuLaw {
    pub fn make_packet_info(&self) -> Result<PacketInfo> {
        let num_channels = self.channels.count() as u32;
        PacketInfo::without_blocks(num_channels)
    }
}

pub struct PacketInfo {
    pub block_size: NonZero<u64>,
    pub frames_per_block: NonZero<u64>,
    pub max_blocks_per_packet: NonZero<u64>,
    pub max_frames_per_packet: NonZero<u64>,
}

impl PacketInfo {
    pub fn with_blocks(block_size: u16, frames_per_block: u64) -> Result<Self> {
        // Frames/block must be non-zero.
        let frames_per_block = match NonZero::new(frames_per_block) {
            Some(value) => value,
            _ => return decode_error("riff: frames per block is 0"),
        };

        // Block size must be non-zero.
        let block_size = match NonZero::new(u64::from(block_size)) {
            Some(value) => value,
            None => return decode_error("riff: block size is 0"),
        };

        // Atleast one block must be present per packet.
        let max_blocks_per_packet = match NonZero::new(
            frames_per_block.max(MAX_FRAMES_PER_PACKET).get() / frames_per_block.get(),
        ) {
            Some(value) => value,
            _ => return decode_error("riff: max blocks per packet is 0"),
        };

        // Pre-compute maximum frames per packet.
        let max_frames_per_packet = match max_blocks_per_packet.checked_mul(frames_per_block) {
            Some(value) => value,
            _ => return decode_error("riff: max frames per packet overflow"),
        };

        Ok(Self { block_size, frames_per_block, max_blocks_per_packet, max_frames_per_packet })
    }

    pub fn without_blocks(frame_len: u32) -> Result<Self> {
        // Block size must be non-zero.
        let block_size = match NonZero::new(u64::from(frame_len)) {
            Some(value) => value,
            None => return decode_error("riff: block size is 0"),
        };

        Ok(Self {
            block_size,
            frames_per_block: NonZero::new(1).expect("1 is non-zero"),
            max_blocks_per_packet: MAX_FRAMES_PER_PACKET,
            max_frames_per_packet: MAX_FRAMES_PER_PACKET,
        })
    }

    pub fn get_num_frames_at(&self, data_pos: u64) -> u64 {
        (data_pos / self.block_size) * self.frames_per_block.get()
    }

    pub fn get_actual_ts(&self, ts: Timestamp) -> Timestamp {
        let max_frames_per_packet = self.max_frames_per_packet.get();
        ts.align_towards_zero(Duration::from(max_frames_per_packet))
            .expect("max_frames_per_packet is non-zero")
    }
}

pub fn next_packet(
    reader: &mut MediaSourceStream<'_>,
    packet_info: &PacketInfo,
    tracks: &[Track],
    data_start_pos: u64,
    data_end_pos: u64,
) -> Result<Option<Packet>> {
    let pos = reader.pos();
    if tracks.is_empty() {
        return decode_error("riff: no tracks");
    }

    // Determine the number of complete blocks remaining in the data chunk.
    let num_blocks_left =
        if pos < data_end_pos { (data_end_pos - pos) / packet_info.block_size } else { 0 };

    if num_blocks_left == 0 {
        return Ok(None);
    }

    let blocks_per_packet = num_blocks_left.min(packet_info.max_blocks_per_packet.get());

    // The packet timestamp is the position of the first byte of the first frame in the
    // packet relative to the start of the data chunk divided by the length per frame.
    let pts = match packet_info.get_num_frames_at(pos - data_start_pos).try_into() {
        Ok(pts) => pts,
        Err(_) => return Ok(None),
    };

    let dur = Duration::from(blocks_per_packet * packet_info.frames_per_block.get());
    let pkt_len = blocks_per_packet * packet_info.block_size.get();

    // Copy the frames.
    let packet_buf = reader.read_boxed_slice(pkt_len as usize)?;

    Ok(Some(Packet::new(0, pts, dur, packet_buf)))
}

/// TODO: format here refers to format chunk in Wave terminology, but the data being handled here is
/// generic - find a better name, or combine with append_data_params
pub fn append_format_params(
    codec_params: &mut AudioCodecParameters,
    format_data: FormatData,
    sample_rate: u32,
) {
    codec_params.with_sample_rate(sample_rate);

    match format_data {
        FormatData::Pcm(pcm) => {
            codec_params
                .for_codec(pcm.codec)
                .with_bits_per_coded_sample(u32::from(pcm.valid_bits_per_sample))
                .with_bits_per_sample(u32::from(pcm.valid_bits_per_sample))
                .with_channels(pcm.channels);
        }
        FormatData::Adpcm(adpcm) => {
            codec_params.for_codec(adpcm.codec).with_channels(adpcm.channels);
        }
        FormatData::IeeeFloat(ieee) => {
            codec_params.for_codec(ieee.codec).with_channels(ieee.channels);
        }
        FormatData::Extensible(ext) => {
            codec_params
                .for_codec(ext.codec)
                .with_bits_per_coded_sample(u32::from(ext.valid_bits_per_sample))
                .with_bits_per_sample(u32::from(ext.valid_bits_per_sample))
                .with_channels(ext.channels);
        }
        FormatData::ALaw(alaw) => {
            codec_params.for_codec(alaw.codec).with_channels(alaw.channels);
        }
        FormatData::MuLaw(mulaw) => {
            codec_params.for_codec(mulaw.codec).with_channels(mulaw.channels);
        }
    }
}

/// TODO: format here refers to format chunk in Wave terminology, but the data being handled here is
/// generic - find a better name, or combine with append_data_params append_format_params
pub fn append_data_params(track: &mut Track, data_len: u64, packet_info: &PacketInfo) {
    // Prefer the duration in the FACT chunk.
    let num_frames = packet_info.get_num_frames_at(data_len);
    track.with_num_frames(num_frames);
    // Duration equals the number of frames because the timebase is always 1 / sample rate.
    track.with_duration(Duration::from(num_frames));
}
