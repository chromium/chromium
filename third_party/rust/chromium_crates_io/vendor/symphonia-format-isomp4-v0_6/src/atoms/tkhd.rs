// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::atoms::{Atom, AtomHeader, AtomIterator, ReadAtom, Result, decode_error};
use crate::fp::FpU8;

/// Track header atom.
#[allow(dead_code)]
#[derive(Debug)]
pub struct TkhdAtom {
    /// Track header flags.
    pub flags: u32,
    /// Creation time.
    pub ctime: u64,
    /// Modification time.
    pub mtime: u64,
    /// Track identifier.
    pub id: u32,
    /// Track duration in the timescale units specified in the movie header.
    ///
    /// This value is equal to the sum of the durations of all the track's edits. If there are no
    /// edits, then this is the duration of all the track's samples.
    pub duration: u64,
    /// Layer.
    pub layer: u16,
    /// Grouping identifier.
    pub alternate_group: u16,
    /// Preferred volume for track playback.
    pub volume: FpU8,
}

impl Atom for TkhdAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        let (version, flags) = it.read_extended_header()?;

        let mut tkhd = TkhdAtom {
            flags,
            ctime: 0,
            mtime: 0,
            id: 0,
            duration: 0,
            layer: 0,
            alternate_group: 0,
            volume: Default::default(),
        };

        // Version 0 uses 32-bit time values, verion 1 used 64-bit values.
        match version {
            0 => {
                tkhd.ctime = u64::from(it.read_u32()?);
                tkhd.mtime = u64::from(it.read_u32()?);
                tkhd.id = it.read_u32()?;
                let _ = it.read_u32()?; // Reserved
                tkhd.duration = u64::from(it.read_u32()?);
            }
            1 => {
                tkhd.ctime = it.read_u64()?;
                tkhd.mtime = it.read_u64()?;
                tkhd.id = it.read_u32()?;
                let _ = it.read_u32()?; // Reserved
                tkhd.duration = it.read_u64()?;
            }
            _ => return decode_error("isomp4 (tkhd): invalid version"),
        }

        // Reserved
        let _ = it.read_u64()?;

        tkhd.layer = it.read_u16()?;
        tkhd.alternate_group = it.read_u16()?;
        tkhd.volume = FpU8::parse_raw(it.read_u16()?);

        // The remainder of the header is only useful for video tracks.

        Ok(tkhd)
    }
}
