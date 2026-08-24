// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use symphonia_core::codecs::audio::well_known::CODEC_ID_AC3;

use crate::atoms::stsd::AudioSampleEntry;
use crate::atoms::{Atom, AtomHeader, AtomIterator, ReadAtom, Result, decode_error};

#[derive(Debug)]
pub struct Dac3Atom {
    /// AC3SpecificBox
    extra_data: Box<[u8]>,
}

impl Atom for Dac3Atom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, header: &AtomHeader) -> Result<Self> {
        // TODO: Validate.
        const MAX_DAC3_ATOM_SIZE: u64 = 4 * 1024;

        // AC3SpecificBox should have length
        let len = match header.data_size() {
            Some(len) if len <= MAX_DAC3_ATOM_SIZE => len as usize,
            Some(_) => return decode_error("isomp4 (dac3): atom size is greater than 4 kb"),
            None => return decode_error("isomp4 (dac3): expected atom size to be known"),
        };

        let extra_data = it.read_boxed_slice_exact(len)?;

        Ok(Dac3Atom { extra_data })
    }
}

impl Dac3Atom {
    pub fn fill_audio_sample_entry(self, entry: &mut AudioSampleEntry) {
        entry.codec_id = CODEC_ID_AC3;
        entry.extra_data = Some(self.extra_data);
    }
}
