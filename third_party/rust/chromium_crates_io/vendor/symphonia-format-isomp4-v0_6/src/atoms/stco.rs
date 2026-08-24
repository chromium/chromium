// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::atoms::limits::*;
use crate::atoms::{Atom, AtomHeader, AtomIterator, ReadAtom, Result};

/// Chunk offset atom (32-bit version).
#[allow(dead_code)]
#[derive(Debug)]
pub struct StcoAtom {
    pub chunk_offsets: Vec<u32>,
}

impl Atom for StcoAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        let (_, _) = it.read_extended_header()?;

        let entry_count = it.read_u32()?;

        // Limit the maximum initial capacity to prevent malicious files from using all the
        // available memory.
        let mut chunk_offsets =
            Vec::with_capacity(MAX_TABLE_INITIAL_CAPACITY.min(entry_count as usize));

        for _ in 0..entry_count {
            chunk_offsets.push(it.read_u32()?);
        }

        Ok(StcoAtom { chunk_offsets })
    }
}
