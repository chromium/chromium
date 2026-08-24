// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use symphonia_core::{
    audio::{Channels, Position},
    errors::{Result, decode_error, unsupported_error},
    io::ReadBytes,
};

const OPUS_MAGIC_SIGNATURE: &[u8] = b"OpusHead";

#[derive(Debug, Default)]
pub struct OpusHead {
    pub version: u8,
    pub channels: Channels,
    pub original_sample_rate: u32,
    pub gain: i16,
    pub pre_skip: u16,
}

impl OpusHead {
    pub fn read<B: ReadBytes>(reader: &mut B, max_version: u8) -> Result<Self> {
        // The first 8 bytes are the magic signature ASCII bytes.
        let mut magic = [0; 8];
        reader.read_buf_exact(&mut magic)?;

        if magic != *OPUS_MAGIC_SIGNATURE {
            return unsupported_error("common (opus): invalid magic signature");
        }

        // The next byte is the encapsulation version. The max version is specified by the caller
        // since it depends on the container format used.
        let version = reader.read_byte()?;
        if version > max_version {
            return decode_error("common (opus): invalid version");
        }

        // The next byte is the number of channels and must not be 0.
        let channel_count = reader.read_byte()?;

        if channel_count == 0 {
            return decode_error("common (opus): invalid channel count");
        }

        // The next 16-bit integer is the pre-skip padding (# of samples at 48kHz to subtract from
        // the OGG granule position to obtain the PCM sample position).
        let pre_skip = reader.read_u16()?;

        // The next 32-bit integer is the sample rate of the original audio.
        let original_sample_rate = reader.read_u32()?;

        // Next, the 16-bit gain value.
        let gain = reader.read_i16()?;

        // The next byte indicates the channel mapping. Most of these values are reserved.
        let channel_mapping = reader.read_byte()?;

        let positions = match channel_mapping {
            // RTP Mapping
            0 if channel_count == 1 => Position::FRONT_LEFT,
            0 if channel_count == 2 => Position::FRONT_LEFT | Position::FRONT_RIGHT,
            // Vorbis Mapping
            1 => match channel_count {
                1 => Position::FRONT_LEFT,
                2 => Position::FRONT_LEFT | Position::FRONT_RIGHT,
                3 => Position::FRONT_LEFT | Position::FRONT_CENTER | Position::FRONT_RIGHT,
                4 => {
                    Position::FRONT_LEFT
                        | Position::FRONT_RIGHT
                        | Position::REAR_LEFT
                        | Position::REAR_RIGHT
                }
                5 => {
                    Position::FRONT_LEFT
                        | Position::FRONT_CENTER
                        | Position::FRONT_RIGHT
                        | Position::REAR_LEFT
                        | Position::REAR_RIGHT
                }
                6 => {
                    Position::FRONT_LEFT
                        | Position::FRONT_CENTER
                        | Position::FRONT_RIGHT
                        | Position::REAR_LEFT
                        | Position::REAR_RIGHT
                        | Position::LFE1
                }
                7 => {
                    Position::FRONT_LEFT
                        | Position::FRONT_CENTER
                        | Position::FRONT_RIGHT
                        | Position::SIDE_LEFT
                        | Position::SIDE_RIGHT
                        | Position::REAR_CENTER
                        | Position::LFE1
                }
                8 => {
                    Position::FRONT_LEFT
                        | Position::FRONT_CENTER
                        | Position::FRONT_RIGHT
                        | Position::SIDE_LEFT
                        | Position::SIDE_RIGHT
                        | Position::REAR_LEFT
                        | Position::REAR_RIGHT
                        | Position::LFE1
                }
                _ => return decode_error("common (opus): invalid vorbis channel mapping"),
            },
            // Reserved, and should NOT be supported for playback.
            _ => return unsupported_error("common (opus): unsupported channel mapping family"),
        };

        Ok(Self {
            version,
            channels: Channels::Positioned(positions),
            gain,
            original_sample_rate,
            pre_skip,
        })
    }
}
