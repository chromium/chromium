// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::atoms::{Atom, AtomHeader, AtomIterator, ReadAtom, Result};

/// Movie fragment header atom.
#[allow(dead_code)]
#[derive(Debug)]
pub struct MfhdAtom {
    /// Sequence number associated with fragment.
    pub sequence_number: u32,
}

impl Atom for MfhdAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        let (_, _) = it.read_extended_header()?;

        let sequence_number = it.read_u32()?;

        Ok(MfhdAtom { sequence_number })
    }
}
