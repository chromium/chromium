// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use symphonia_common::mpeg::video::AVCDecoderConfigurationRecord;
use symphonia_core::codecs::CodecProfile;
use symphonia_core::codecs::video::VideoExtraData;
use symphonia_core::codecs::video::well_known::CODEC_ID_H264;
use symphonia_core::codecs::video::well_known::extra_data::VIDEO_EXTRA_DATA_ID_AVC_DECODER_CONFIG;

use crate::atoms::stsd::VisualSampleEntry;
use crate::atoms::{Atom, AtomHeader, AtomIterator, ReadAtom, Result, decode_error};

#[derive(Debug)]
pub struct AvcCAtom {
    /// AVC extra data (AVCDecoderConfigurationRecord).
    extra_data: VideoExtraData,
    profile: CodecProfile,
    level: u32,
}

impl Atom for AvcCAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, header: &AtomHeader) -> Result<Self> {
        // TODO: Validate.
        const MAX_AVCC_ATOM_SIZE: u64 = 1 * 1024;

        // The AVCConfiguration atom payload is a single AVCDecoderConfigurationRecord. This record
        // forms the defacto codec extra data. It should not exceed 1 kB.
        let len = match header.data_size() {
            Some(len) if len <= MAX_AVCC_ATOM_SIZE => len as usize,
            Some(_) => {
                return decode_error("isomp4 (avcC): atom size is greater than 1 kb");
            }
            None => {
                return decode_error("isomp4 (avcC): expected atom size to be known");
            }
        };

        let extra_data = VideoExtraData {
            id: VIDEO_EXTRA_DATA_ID_AVC_DECODER_CONFIG,
            data: it.read_boxed_slice_exact(len)?,
        };

        let avc_config = AVCDecoderConfigurationRecord::read(&extra_data.data)?;

        Ok(Self { extra_data, profile: avc_config.profile, level: avc_config.level })
    }
}

impl AvcCAtom {
    pub fn fill_video_sample_entry(self, entry: &mut VisualSampleEntry) {
        entry.codec_id = CODEC_ID_H264;
        entry.profile = Some(self.profile);
        entry.level = Some(self.level);
        entry.extra_data.push(self.extra_data);
    }
}
