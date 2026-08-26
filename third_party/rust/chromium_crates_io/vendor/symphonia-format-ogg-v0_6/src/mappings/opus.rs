// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::common::SideData;

use super::{MapResult, Mapper, PacketParser};

use symphonia_common::xiph::audio::opus;
use symphonia_core::codecs::CodecParameters;
use symphonia_core::codecs::audio::AudioCodecParameters;
use symphonia_core::codecs::audio::well_known::CODEC_ID_OPUS;
use symphonia_core::errors::Result;
use symphonia_core::formats::Track;
use symphonia_core::io::{BufReader, ReadBytes};
use symphonia_core::meta::MetadataBuilder;

use symphonia_core::units::Duration;
use symphonia_metadata::embedded::vorbis::{self, VORBIS_COMMENT_METADATA_INFO};

use log::warn;

/// The minimum expected size of an Opus identification packet.
const OGG_OPUS_MIN_IDENTIFICATION_PACKET_SIZE: usize = 19;

/// The signature for an Opus metadata packet.
const OGG_OPUS_COMMENT_SIGNATURE: &[u8] = b"OpusTags";

/// The maximum support Opus OGG mapping version.
const OGG_OPUS_MAPPING_VERSION_MAX: u8 = 0x0f;

pub fn detect(serial: u32, buf: &[u8]) -> Result<Option<Box<dyn Mapper>>> {
    // The identification packet for Opus must be a minimum size.
    if buf.len() < OGG_OPUS_MIN_IDENTIFICATION_PACKET_SIZE {
        return Ok(None);
    }

    let mut reader = BufReader::new(buf);

    let Ok(opus_head) = opus::OpusHead::read(&mut reader, OGG_OPUS_MAPPING_VERSION_MAX)
    else {
        return Ok(None);
    };

    // Populate the codec parameters with the information read from identification header.
    let mut codec_params = AudioCodecParameters::new();

    codec_params
        .for_codec(CODEC_ID_OPUS)
        .with_sample_rate(48_000)
        .with_channels(opus_head.channels)
        .with_extra_data(Box::from(buf));

    // Create the track.
    let mut track = Track::new(serial);

    track
        .with_codec_params(CodecParameters::Audio(codec_params))
        .with_delay(u32::from(opus_head.pre_skip));

    // Instantiate the Opus mapper.
    let mapper = Box::new(OpusMapper { track, need_comment: true });

    Ok(Some(mapper))
}

pub struct OpusPacketParser {}

impl PacketParser for OpusPacketParser {
    fn parse_next_packet_dur(&mut self, packet: &[u8]) -> (Duration, Duration) {
        // See https://www.rfc-editor.org/rfc/rfc6716
        // Read TOC (Table Of Contents) byte which is the first byte in the opus data.
        let toc_byte = match packet.first() {
            Some(b) => b,
            None => {
                warn!("opus packet empty");
                return (Duration::ZERO, Duration::ZERO);
            }
        };
        // The configuration number is the 5 most significant bits. Shift out 3 least significant
        // bits.
        let configuration_number = toc_byte >> 3; // max 2^5-1 = 31

        // The configuration number maps to packet length according to this lookup table.
        // See https://www.rfc-editor.org/rfc/rfc6716 top half of page 14.
        // Numbers are in milliseconds in the rfc. Down below they are in TimeBase units, so
        // 10ms = 10*48.
        #[rustfmt::skip]
        const CONFIGURATION_NUMBER_TO_FRAME_DURATION: [u32; 32] = [
            10*48, 20*48, 40*48, 60*48,
            10*48, 20*48, 40*48, 60*48,
            10*48, 20*48, 40*48, 60*48,
            10*48, 20*48,
            10*48, 20*48,
            (2.5*48.0) as u32, 5*48, 10*48, 20*48,
            (2.5*48.0) as u32, 5*48, 10*48, 20*48,
            (2.5*48.0) as u32, 5*48, 10*48, 20*48,
            (2.5*48.0) as u32, 5*48, 10*48, 20*48,
        ];
        // Look up the frame length.
        let frame_duration =
            CONFIGURATION_NUMBER_TO_FRAME_DURATION[configuration_number as usize] as u64;

        // Look up the number of frames in the packet.
        // See https://www.rfc-editor.org/rfc/rfc6716 bottom half of page 14.
        let c = toc_byte & 0b11; // Note: it's actually called "c" in the rfc.
        let num_frames = match c {
            0 => 1,
            1 | 2 => 2,
            3 => match packet.get(1) {
                Some(byte) => {
                    // TOC byte is followed by number of frames. See page 18 section 3.2.5 code 3
                    let m = byte & 0b11111; // Note: it's actually called "M" in the rfc.
                    m as u64
                }
                None => {
                    // What to do here? I'd like to return an error but this is an infalliable
                    // trait.
                    warn!("opus code 3 packet with no following byte containing number of frames");
                    return (Duration::ZERO, Duration::ZERO);
                }
            },
            _ => unreachable!("masked 2 bits"),
        };
        // Look up the packet length and return it.
        (Duration::new(frame_duration * num_frames), Duration::ZERO)
    }
}

struct OpusMapper {
    track: Track,
    need_comment: bool,
}

impl Mapper for OpusMapper {
    fn name(&self) -> &'static str {
        "opus"
    }

    fn reset(&mut self) {
        // Nothing to do.
    }

    fn track(&self) -> &Track {
        &self.track
    }

    fn track_mut(&mut self) -> &mut Track {
        &mut self.track
    }

    fn make_parser(&self) -> Option<Box<dyn super::PacketParser>> {
        Some(Box::new(OpusPacketParser {}))
    }

    fn map_packet(&mut self, packet: &[u8]) -> Result<MapResult> {
        if !self.need_comment {
            let (dur, discard) = OpusPacketParser {}.parse_next_packet_dur(packet);
            Ok(MapResult::StreamData { dur, discard })
        }
        else {
            let mut reader = BufReader::new(packet);

            // Read the header signature.
            let mut sig = [0; 8];
            reader.read_buf_exact(&mut sig)?;

            if sig == *OGG_OPUS_COMMENT_SIGNATURE {
                // This packet should be a metadata packet containing a Vorbis Comment.
                let mut builder = MetadataBuilder::new(VORBIS_COMMENT_METADATA_INFO);
                let mut side_data = Default::default();

                vorbis::read_vorbis_comment(&mut reader, &mut builder, &mut side_data)?;

                let rev = builder.build();

                self.need_comment = false;

                Ok(MapResult::SideData { data: SideData::Metadata { rev, side_data } })
            }
            else {
                warn!("ogg (opus): invalid packet type");
                Ok(MapResult::Unknown)
            }
        }
    }
}
