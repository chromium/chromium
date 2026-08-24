// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::atoms::{Atom, AtomHeader, AtomIterator, ReadAtom, Result, decode_error};

/// Movie extends header atom.
#[allow(dead_code)]
#[derive(Debug)]
pub struct MehdAtom {
    /// Fragment duration.
    pub fragment_duration: u64,
}

impl Atom for MehdAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        let (version, _) = it.read_extended_header()?;

        let fragment_duration = match version {
            0 => u64::from(it.read_u32()?),
            1 => it.read_u64()?,
            _ => return decode_error("isomp4 (mehd): invalid version"),
        };

        Ok(MehdAtom { fragment_duration })
    }
}
