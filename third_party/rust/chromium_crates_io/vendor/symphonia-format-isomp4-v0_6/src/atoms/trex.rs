// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::atoms::{Atom, AtomHeader, AtomIterator, ReadAtom, Result};

/// Track extends atom.
#[allow(dead_code)]
#[derive(Debug)]
pub struct TrexAtom {
    /// Track this atom describes.
    pub track_id: u32,
    /// Default sample description index.
    pub default_sample_desc_idx: u32,
    /// Default sample duration.
    pub default_sample_duration: u32,
    /// Default sample size.
    pub default_sample_size: u32,
    /// Default sample flags.
    pub default_sample_flags: u32,
}

impl Atom for TrexAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        let (_, _) = it.read_extended_header()?;

        Ok(TrexAtom {
            track_id: it.read_u32()?,
            default_sample_desc_idx: it.read_u32()?,
            default_sample_duration: it.read_u32()?,
            default_sample_size: it.read_u32()?,
            default_sample_flags: it.read_u32()?,
        })
    }
}
