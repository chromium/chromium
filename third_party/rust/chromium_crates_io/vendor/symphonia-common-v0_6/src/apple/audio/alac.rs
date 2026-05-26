// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use symphonia_core::audio::{Channels, layouts};
use symphonia_core::errors::{Result, decode_error, unsupported_error};
use symphonia_core::io::{BufReader, FiniteStream, ReadBytes};

/// Supported ALAC version.
const ALAC_VERSION: u8 = 0;

/// The ALAC "magic cookie" or codec specific configuration.
#[derive(Debug, Clone)]
pub struct MagicCookie {
    pub frame_length: u32,
    pub compatible_version: u8,
    pub bit_depth: u8,
    pub pb: u8,
    pub mb: u8,
    pub kb: u8,
    pub num_channels: u8,
    pub max_run: u16,
    pub max_frame_bytes: u32,
    pub avg_bit_rate: u32,
    pub sample_rate: u32,
    pub channels: Channels,
}

impl MagicCookie {
    /// Read the ALAC magic cookie from the provided buffer.
    pub fn read(mut buf: &[u8]) -> Result<MagicCookie> {
        // The magic cookie must be atleast 24 bytes long.
        if buf.len() < 24 {
            return unsupported_error("common (alac): magic cookie size too small");
        }

        // The magic cookie may be preceeded by a FRMA atom. Skip over the FRMA atom.
        if buf[4..8] == *b"frma" {
            buf = &buf[12..];
        }

        // The magic cookie may be preceeded by an ALAC atom. Skip over the ALAC atom.
        if buf[4..8] == *b"alac" {
            buf = &buf[12..];
        }

        // The magic cookie must be either 24 or 48 bytes long.
        if buf.len() != 24 && buf.len() != 48 {
            return unsupported_error("common (alac): invalid magic cookie size");
        }

        let mut reader = BufReader::new(buf);

        let mut config = MagicCookie {
            frame_length: reader.read_be_u32()?,
            compatible_version: reader.read_u8()?,
            bit_depth: reader.read_u8()?,
            pb: reader.read_u8()?,
            mb: reader.read_u8()?,
            kb: reader.read_u8()?,
            num_channels: reader.read_u8()?,
            max_run: reader.read_be_u16()?,
            max_frame_bytes: reader.read_be_u32()?,
            avg_bit_rate: reader.read_be_u32()?,
            sample_rate: reader.read_be_u32()?,
            channels: Default::default(),
        };

        // Only support up-to the currently implemented ALAC version.
        if config.compatible_version > ALAC_VERSION {
            return unsupported_error("common (alac): not compatible with alac version 0");
        }

        // A bit-depth greater than 32 is not allowed.
        if config.bit_depth > 32 {
            return decode_error("common (alac): invalid bit depth");
        }

        // Only 8 channel layouts exist.
        // TODO: Support discrete/auxiliary channels.
        if config.num_channels < 1 || config.num_channels > 8 {
            return decode_error("common (alac): invalid number of channels");
        }

        // If the magic cookie is 48 bytes, the channel layout is explictly set, otherwise select a
        // channel layout from the number of channels.
        config.channels = if reader.byte_len() == 48 {
            // The first field is the size of the channel layout info. This should always be 24.
            if reader.read_be_u32()? != 24 {
                return decode_error("common (alac): invalid channel layout info size");
            }

            // The channel layout info identifier should be the ascii string "chan".
            if reader.read_quad_bytes()? != *b"chan" {
                return decode_error("common (alac): invalid channel layout info id");
            }

            // The channel layout info version must be 0.
            if reader.read_be_u32()? != 0 {
                return decode_error("common (alac): invalid channel layout info version");
            }

            // Read the channel layout tag. The numerical value of this tag is defined by the Apple
            // CoreAudio API.
            let layout_channels = match reader.read_be_u32()? {
                // 100 << 16
                0x64_0001 => layouts::CHANNEL_LAYOUT_MONO,
                // 101 << 16
                0x65_0002 => layouts::CHANNEL_LAYOUT_STEREO,
                // 113 << 16
                0x71_0003 => layouts::CHANNEL_LAYOUT_MPEG_3P0_B,
                // 116 << 16
                0x74_0004 => layouts::CHANNEL_LAYOUT_MPEG_4P0_B,
                // 120 << 16
                0x78_0005 => layouts::CHANNEL_LAYOUT_MPEG_5P0_D,
                // 124 << 16
                0x7c_0006 => layouts::CHANNEL_LAYOUT_MPEG_5P1_D,
                // 142 << 16
                0x8e_0007 => layouts::CHANNEL_LAYOUT_AAC_6P1,
                // 127 << 16
                0x7f_0008 => layouts::CHANNEL_LAYOUT_MPEG_7P1_B,
                _ => return decode_error("common (alac): invalid channel layout tag"),
            };

            // The number of channels stated in the mandatory part of the magic cookie should match
            // the number of channels implicit to the channel layout.
            if config.num_channels != layout_channels.count() as u8 {
                return decode_error(
                    "common (alac): the number of channels differs from the channel layout",
                );
            }

            // The next two fields are reserved and should be 0.
            if reader.read_be_u32()? != 0 || reader.read_be_u32()? != 0 {
                return decode_error(
                    "common (alac): reserved values in channel layout info are not 0",
                );
            }

            layout_channels
        }
        else {
            // If extra channel information is not provided, use the number of channels to assign
            // a channel layout.
            //
            // TODO: If the number of channels is > 2, then the additional channels are considered
            // discrete and not part of a channel layout. However, Symphonia does not support
            // discrete/auxiliary channels so the standard ALAC channel layouts are used for now.
            match config.num_channels {
                1 => layouts::CHANNEL_LAYOUT_MONO,
                2 => layouts::CHANNEL_LAYOUT_STEREO,
                3 => layouts::CHANNEL_LAYOUT_MPEG_3P0_B,
                4 => layouts::CHANNEL_LAYOUT_MPEG_4P0_B,
                5 => layouts::CHANNEL_LAYOUT_MPEG_5P0_D,
                6 => layouts::CHANNEL_LAYOUT_MPEG_5P1_D,
                7 => layouts::CHANNEL_LAYOUT_AAC_6P1,
                8 => layouts::CHANNEL_LAYOUT_MPEG_7P1_B,
                _ => {
                    return decode_error(
                        "common (alac): unknown channel layout for number of channels",
                    );
                }
            }
        };

        Ok(config)
    }
}
