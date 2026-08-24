// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::atoms::limits::*;
use crate::atoms::{Atom, AtomHeader, AtomIterator, ReadAtom, Result};

#[derive(Debug)]
pub enum SampleSize {
    Constant(u32),
    Variable(Vec<u32>),
}

/// Sample Size Atom
#[allow(dead_code)]
#[derive(Debug)]
pub struct StszAtom {
    /// The total number of samples.
    pub sample_count: u32,
    /// A vector of `sample_count` sample sizes, or a constant size for all samples.
    pub sample_sizes: SampleSize,
}

impl Atom for StszAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        let (_, _) = it.read_extended_header()?;

        let sample_size = it.read_u32()?;
        let sample_count = it.read_u32()?;

        let sample_sizes = if sample_size == 0 {
            // Limit the maximum initial capacity to prevent malicious files from using all the
            // available memory.
            let mut entries =
                Vec::with_capacity(MAX_TABLE_INITIAL_CAPACITY.min(sample_count as usize));

            for _ in 0..sample_count {
                entries.push(it.read_u32()?);
            }

            SampleSize::Variable(entries)
        }
        else {
            SampleSize::Constant(sample_size)
        };

        Ok(StszAtom { sample_count, sample_sizes })
    }
}
