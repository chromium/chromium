// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use symphonia_core::codecs::CodecProfile;
use symphonia_core::errors::{Result, decode_error};
use symphonia_core::io::{BitReaderLtr, ReadBitsLtr};

pub struct AVCDecoderConfigurationRecord {
    pub profile: CodecProfile,
    pub level: u32,
}

impl AVCDecoderConfigurationRecord {
    pub fn read(buf: &[u8]) -> Result<Self> {
        let mut br = BitReaderLtr::new(buf);

        // Parse the AVCDecoderConfigurationRecord to get the profile and level. Defined in
        // ISO/IEC 14496-15 section 5.3.3.1.

        // Configuration version is always 1.
        let configuration_version = br.read_bits_leq32(8)?;

        if configuration_version != 1 {
            return decode_error(
                "common (avc): unexpected avc decoder configuration record version",
            );
        }

        // AVC profile as defined in ISO/IEC 14496-10.
        let avc_profile_indication = br.read_bits_leq32(8)?;
        let _profile_compatibility = br.read_bits_leq32(8)?;
        let avc_level_indication = br.read_bits_leq32(8)?;

        Ok(AVCDecoderConfigurationRecord {
            profile: CodecProfile::new(avc_profile_indication),
            level: avc_level_indication,
        })
    }
}

pub struct HEVCDecoderConfigurationRecord {
    pub profile: CodecProfile,
    pub level: u32,
}

impl HEVCDecoderConfigurationRecord {
    pub fn read(buf: &[u8]) -> Result<Self> {
        let mut br = BitReaderLtr::new(buf);

        // Parse the HEVCDecoderConfigurationRecord to get the profile and level. Defined in
        // ISO/IEC 14496-15 section 8.3.3.1.

        // Configuration version is always 1.
        let configuration_version = br.read_bits_leq32(8)?;

        if configuration_version != 1 {
            return decode_error(
                "common (hevc): unexpected hevc decoder configuration record version",
            );
        }

        let _general_profile_space = br.read_bits_leq32(2)?;
        let _general_tier_flag = br.read_bit()?;
        let general_profile_idc = br.read_bits_leq32(5)?;
        let _general_profile_compatibility_flags = br.read_bits_leq32(32)?;
        let _general_constraint_indicator_flags = br.read_bits_leq64(48)?;
        let general_level_idc = br.read_bits_leq32(8)?;

        Ok(HEVCDecoderConfigurationRecord {
            profile: CodecProfile::new(general_profile_idc),
            level: general_level_idc,
        })
    }
}

#[derive(Debug, Default)]
pub struct DOVIDecoderConfigurationRecord {
    pub dv_version_major: u8,
    pub dv_version_minor: u8,
    pub dv_profile: u8,
    pub dv_level: u8,
    pub rpu_present_flag: bool,
    pub el_present_flag: bool,
    pub bl_present_flag: bool,
    pub dv_bl_signal_compatibility_id: u8,
}

impl DOVIDecoderConfigurationRecord {
    pub fn read(buf: &[u8]) -> Result<Self> {
        let mut br = BitReaderLtr::new(buf);

        // Parse the DOVIDecoderConfigurationRecord, point 3.2 from
        // https://professional.dolby.com/siteassets/content-creation/dolby-vision-for-content-creators/dolby_vision_bitstreams_within_the_iso_base_media_file_format_dec2017.pdf

        let config = DOVIDecoderConfigurationRecord {
            dv_version_major: br.read_bits_leq32(8)? as u8,
            dv_version_minor: br.read_bits_leq32(8)? as u8,
            dv_profile: br.read_bits_leq32(7)? as u8,
            dv_level: br.read_bits_leq32(6)? as u8,
            rpu_present_flag: br.read_bool()?,
            el_present_flag: br.read_bool()?,
            bl_present_flag: br.read_bool()?,
            dv_bl_signal_compatibility_id: br.read_bits_leq32(4)? as u8,
        };

        Ok(config)
    }
}
