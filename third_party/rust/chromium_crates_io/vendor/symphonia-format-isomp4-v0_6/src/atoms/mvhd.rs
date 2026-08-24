// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::num::NonZero;

use symphonia_core::errors::Error;

use crate::atoms::{Atom, AtomHeader, AtomIterator, ReadAtom, Result, decode_error};
use crate::fp::FpU8;

/// Movie header atom.
#[allow(dead_code)]
#[derive(Debug)]
pub struct MvhdAtom {
    /// The creation time.
    pub ctime: u64,
    /// The modification time.
    pub mtime: u64,
    /// Timescale for the movie expressed as the number of units per second.
    pub timescale: NonZero<u32>,
    /// The duration of the movie in timescale units.
    ///
    /// This value is equal to the sum of the durations of all the longest track's edits. If there
    /// are no edits, then this is the duration of all the longest track's samples.
    pub duration: u64,
    /// The preferred volume to play the movie.
    pub volume: FpU8,
}

impl Atom for MvhdAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        let (version, _) = it.read_extended_header()?;

        let mut mvhd = MvhdAtom {
            ctime: 0,
            mtime: 0,
            timescale: NonZero::new(1).unwrap(),
            duration: 0,
            volume: Default::default(),
        };

        // Version 0 uses 32-bit time values, verion 1 used 64-bit values.
        match version {
            0 => {
                mvhd.ctime = u64::from(it.read_u32()?);
                mvhd.mtime = u64::from(it.read_u32()?);
                mvhd.timescale = NonZero::new(it.read_u32()?)
                    .ok_or(Error::DecodeError("isomp4 (mvhd): timescale is zero"))?;
                // 0xffff_ffff is a special case.
                mvhd.duration = match it.read_u32()? {
                    u32::MAX => u64::MAX,
                    duration => u64::from(duration),
                };
            }
            1 => {
                mvhd.ctime = it.read_u64()?;
                mvhd.mtime = it.read_u64()?;
                mvhd.timescale = NonZero::new(it.read_u32()?)
                    .ok_or(Error::DecodeError("isomp4 (mvhd): timescale is zero"))?;
                mvhd.duration = it.read_u64()?;
            }
            _ => return decode_error("isomp4 (mvhd): invalid version"),
        }

        // Ignore the preferred playback rate.
        let _ = it.read_u32()?;

        // Preferred volume.
        mvhd.volume = FpU8::parse_raw(it.read_u16()?);

        // Remaining fields are ignored.

        Ok(mvhd)
    }
}
