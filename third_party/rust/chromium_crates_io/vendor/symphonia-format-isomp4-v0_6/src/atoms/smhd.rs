// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::atoms::{Atom, AtomHeader, AtomIterator, ReadAtom, Result};
use crate::fp::FpI8;

/// Sound header atom.
#[allow(dead_code)]
#[derive(Debug)]
pub struct SmhdAtom {
    /// Stereo balance.
    pub balance: FpI8,
}

impl Atom for SmhdAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        let (_, _) = it.read_extended_header()?;

        // Stereo balance
        let balance = FpI8::parse_raw(it.read_u16()? as i16);

        // Reserved.
        let _ = it.read_u16()?;

        Ok(SmhdAtom { balance })
    }
}
