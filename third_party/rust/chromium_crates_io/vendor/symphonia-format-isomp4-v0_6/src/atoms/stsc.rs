// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::atoms::limits::*;
use crate::atoms::{Atom, AtomHeader, AtomIterator, ReadAtom, Result, decode_error};

#[derive(Debug)]
pub struct StscEntry {
    pub first_chunk: u32,
    pub first_sample: u32,
    pub samples_per_chunk: u32,
    #[allow(dead_code)]
    pub sample_desc_index: u32,
}

/// Sample to Chunk Atom
#[allow(dead_code)]
#[derive(Debug)]
pub struct StscAtom {
    /// Entries.
    pub entries: Vec<StscEntry>,
}

impl StscAtom {
    /// Finds the `StscEntry` for the sample indicated by `sample_num`. Note, `sample_num` is indexed
    /// relative to the `StscAtom`. Complexity is O(log2 N).
    pub fn find_entry_for_sample(&self, sample_num: u32) -> Option<&StscEntry> {
        let mut left = 1;
        let mut right = self.entries.len();

        while left < right {
            let mid = left + (right - left) / 2;

            let entry = self.entries.get(mid).expect("mid is always within entries bounds");

            if entry.first_sample < sample_num {
                left = mid + 1;
            }
            else {
                right = mid;
            }
        }

        // The index found above (left) is the exclusive upper bound of all entries where
        // first_sample < sample_num. Therefore, the entry to return has an index of left-1. The
        // index will never equal 0 so this is safe. If the table were empty, left == 1, thus calling
        // get with an index of 0, and safely returning None.
        self.entries.get(left - 1)
    }
}

impl Atom for StscAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        let (_, _) = it.read_extended_header()?;

        let entry_count = it.read_u32()?;

        // Limit the maximum initial capacity to prevent malicious files from using all the
        // available memory.
        let mut entries = Vec::with_capacity(MAX_TABLE_INITIAL_CAPACITY.min(entry_count as usize));

        for _ in 0..entry_count {
            let first_chunk_raw = it.read_u32()?;

            if first_chunk_raw == 0 {
                return decode_error("isomp4 (stsc): first_chunk must be >= 1");
            }

            entries.push(StscEntry {
                first_chunk: first_chunk_raw - 1,
                first_sample: 0,
                samples_per_chunk: it.read_u32()?,
                sample_desc_index: it.read_u32()?,
            });
        }

        // Post-process entries to check for errors and calculate the file sample.
        if entry_count > 0 {
            for i in 0..entry_count as usize - 1 {
                // Validate that first_chunk is monotonic across all entries.
                if entries[i + 1].first_chunk < entries[i].first_chunk {
                    return decode_error("isomp4 (stsc): entry first chunk not monotonic");
                }

                // Validate that samples per chunk is > 0. Could the entry be ignored?
                if entries[i].samples_per_chunk == 0 {
                    return decode_error("isomp4 (stsc): entry has 0 samples per chunk");
                }

                let n = entries[i + 1].first_chunk - entries[i].first_chunk;

                let Some(chunk_samples) = n
                    .checked_mul(entries[i].samples_per_chunk)
                    .and_then(|v| entries[i].first_sample.checked_add(v))
                else {
                    return decode_error("isomp4 (stsc): sample count overflow");
                };

                entries[i + 1].first_sample = chunk_samples;
            }

            // Validate that samples per chunk is > 0. Could the entry be ignored?
            if entries[entry_count as usize - 1].samples_per_chunk == 0 {
                return decode_error("isomp4 (stsc): entry has 0 samples per chunk");
            }
        }

        Ok(StscAtom { entries })
    }
}
