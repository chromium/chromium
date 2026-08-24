// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use symphonia_core::util::bits;

use crate::atoms::{Atom, AtomHeader, AtomIterator, ReadAtom, Result};
use crate::atoms::{decode_error, limits::*};

/// Edit list entry.
#[allow(dead_code)]
#[derive(Debug)]
pub struct ElstEntry {
    segment_duration: u64,
    media_time: i64,
    media_rate_int: i16,
    media_rate_frac: i16,
}

/// Edit list atom.
#[allow(dead_code)]
#[derive(Debug)]
pub struct ElstAtom {
    entries: Vec<ElstEntry>,
}

impl Atom for ElstAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        let (version, _) = it.read_extended_header()?;

        if version > 1 {
            return decode_error("isomp4 (elst): invalid tkhd version");
        }

        let entry_count = it.read_u32()?;

        // Limit the maximum initial capacity to prevent malicious files from using all the
        // available memory.
        let mut entries = Vec::with_capacity(MAX_TABLE_INITIAL_CAPACITY.min(entry_count as usize));

        for _ in 0..entry_count {
            let (segment_duration, media_time) = match version {
                0 => (
                    u64::from(it.read_u32()?),
                    i64::from(bits::sign_extend_leq32_to_i32(it.read_u32()?, 32)),
                ),
                1 => (it.read_u64()?, bits::sign_extend_leq64_to_i64(it.read_u64()?, 64)),
                _ => unreachable!(),
            };

            let media_rate_int = bits::sign_extend_leq16_to_i16(it.read_u16()?, 16);
            let media_rate_frac = bits::sign_extend_leq16_to_i16(it.read_u16()?, 16);

            entries.push(ElstEntry {
                segment_duration,
                media_time,
                media_rate_int,
                media_rate_frac,
            });
        }

        Ok(ElstAtom { entries })
    }
}
