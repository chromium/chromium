// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

//! Parser for the JPEG XL Frame Index box (`jxli`), as specified in
//! the JPEG XL container specification.
//!
//! The frame index box provides a seek table for animated JXL files,
//! listing keyframe byte offsets in the codestream, timestamps, and
//! frame counts.

use std::num::NonZero;

use byteorder::{BigEndian, ReadBytesExt};

use crate::error::{Error, Result};
use crate::icc::read_varint_from_reader;
use crate::util::NewWithCapacity;

/// A single entry in the frame index.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FrameIndexEntry {
    /// Absolute byte offset of this keyframe in the codestream.
    /// (Accumulated from the delta-coded OFFi values.)
    pub codestream_offset: u64,
    /// Duration in ticks from this indexed frame to the next indexed frame
    /// (or end of stream for the last entry). A tick lasts TNUM/TDEN seconds.
    pub duration_ticks: u64,
    /// Number of displayed frames from this indexed frame to the next indexed
    /// frame (or end of stream for the last entry).
    pub frame_count: u64,
}

/// Parsed contents of a Frame Index box (`jxli`).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FrameIndexBox {
    /// Tick numerator. A tick lasts `tnum / tden` seconds.
    pub tnum: u32,
    /// Tick denominator (non-zero per spec).
    pub tden: NonZero<u32>,
    /// Indexed frame entries.
    pub entries: Vec<FrameIndexEntry>,
}

impl FrameIndexBox {
    /// Returns the number of indexed frames.
    pub fn num_frames(&self) -> usize {
        self.entries.len()
    }

    /// Returns the duration of one tick in seconds.
    pub fn tick_duration_secs(&self) -> f64 {
        self.tnum as f64 / self.tden.get() as f64
    }

    /// Finds the index entry for the keyframe at or before the given
    /// codestream byte offset.
    pub fn entry_for_offset(&self, offset: u64) -> Option<&FrameIndexEntry> {
        // Entries are sorted by codestream_offset (monotonically increasing).
        match self
            .entries
            .binary_search_by_key(&offset, |e| e.codestream_offset)
        {
            Ok(i) => Some(&self.entries[i]),
            Err(0) => None,
            Err(i) => Some(&self.entries[i - 1]),
        }
    }

    /// Parse a frame index box from its raw content bytes (after the box header).
    pub fn parse(data: &[u8]) -> Result<Self> {
        let mut reader = data;

        let nf = read_varint_from_reader(&mut reader)?;
        if nf > u32::MAX as u64 {
            return Err(Error::InvalidBox);
        }
        let nf = nf as usize;

        let tnum = reader
            .read_u32::<BigEndian>()
            .map_err(|_| Error::InvalidBox)?;
        let tden = NonZero::new(
            reader
                .read_u32::<BigEndian>()
                .map_err(|_| Error::InvalidBox)?,
        )
        .ok_or(Error::InvalidBox)?;

        // Each entry requires at least 3 bytes (three varints, min 1 byte each).
        // Cap the pre-allocation to avoid OOM from a crafted NF value.
        // Use new_with_capacity to return Err on allocation failure instead of aborting.
        let mut entries = Vec::new_with_capacity(nf.min(reader.len() / 3))?;
        let mut absolute_offset: u64 = 0;

        for _ in 0..nf {
            let off_delta = read_varint_from_reader(&mut reader)?;
            let duration_ticks = read_varint_from_reader(&mut reader)?;
            let frame_count = read_varint_from_reader(&mut reader)?;

            absolute_offset = absolute_offset
                .checked_add(off_delta)
                .ok_or(Error::InvalidBox)?;

            entries.push(FrameIndexEntry {
                codestream_offset: absolute_offset,
                duration_ticks,
                frame_count,
            });
        }

        Ok(FrameIndexBox {
            tnum,
            tden,
            entries,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::util::test::{build_frame_index_content, encode_varint};

    fn build_frame_index(tnum: u32, tden: u32, entries: &[(u64, u64, u64)]) -> Vec<u8> {
        build_frame_index_content(tnum, tden, entries)
    }

    #[test]
    fn test_parse_empty_index() {
        let data = build_frame_index(1, 1000, &[]);
        let index = FrameIndexBox::parse(&data).unwrap();
        assert_eq!(index.num_frames(), 0);
        assert_eq!(index.tnum, 1);
        assert_eq!(index.tden.get(), 1000);
    }

    #[test]
    fn test_parse_single_entry() {
        // One frame at offset 0, duration 100 ticks, 1 frame
        let data = build_frame_index(1, 1000, &[(0, 100, 1)]);
        let index = FrameIndexBox::parse(&data).unwrap();
        assert_eq!(index.num_frames(), 1);
        assert_eq!(
            index.entries[0],
            FrameIndexEntry {
                codestream_offset: 0,
                duration_ticks: 100,
                frame_count: 1,
            }
        );
    }

    #[test]
    fn test_parse_multiple_entries_delta_coding() {
        // Three frames with delta-coded offsets:
        //   OFF0=100 (absolute: 100), T0=50, F0=2
        //   OFF1=200 (absolute: 300), T1=50, F1=2
        //   OFF2=150 (absolute: 450), T2=30, F2=1
        let data = build_frame_index(1, 1000, &[(100, 50, 2), (200, 50, 2), (150, 30, 1)]);
        let index = FrameIndexBox::parse(&data).unwrap();
        assert_eq!(index.num_frames(), 3);
        assert_eq!(index.entries[0].codestream_offset, 100);
        assert_eq!(index.entries[1].codestream_offset, 300);
        assert_eq!(index.entries[2].codestream_offset, 450);
        assert_eq!(index.entries[0].duration_ticks, 50);
        assert_eq!(index.entries[1].duration_ticks, 50);
        assert_eq!(index.entries[2].duration_ticks, 30);
    }

    #[test]
    fn test_parse_large_varint() {
        // Test with a value that requires multiple varint bytes
        let mut data = Vec::new();
        data.extend(encode_varint(1)); // NF = 1
        data.extend(1u32.to_be_bytes()); // TNUM
        data.extend(1000u32.to_be_bytes()); // TDEN
        data.extend(encode_varint(0x1234_5678_9ABC)); // large offset
        data.extend(encode_varint(42));
        data.extend(encode_varint(1));
        let index = FrameIndexBox::parse(&data).unwrap();
        assert_eq!(index.entries[0].codestream_offset, 0x1234_5678_9ABC);
    }

    #[test]
    fn test_entry_for_offset() {
        let data = build_frame_index(1, 1000, &[(100, 50, 2), (200, 50, 2), (150, 30, 1)]);
        let index = FrameIndexBox::parse(&data).unwrap();
        // Absolute offsets: 100, 300, 450

        // Before first entry
        assert!(index.entry_for_offset(50).is_none());
        // Exact match
        assert_eq!(index.entry_for_offset(100).unwrap().codestream_offset, 100);
        // Between entries
        assert_eq!(index.entry_for_offset(200).unwrap().codestream_offset, 100);
        assert_eq!(index.entry_for_offset(350).unwrap().codestream_offset, 300);
        // Exact match on last
        assert_eq!(index.entry_for_offset(450).unwrap().codestream_offset, 450);
        // Past last
        assert_eq!(index.entry_for_offset(999).unwrap().codestream_offset, 450);
    }

    #[test]
    fn test_zero_tden_rejected() {
        let data = build_frame_index(1, 0, &[]);
        assert!(FrameIndexBox::parse(&data).is_err());
    }

    #[test]
    fn test_truncated_data() {
        // Just NF=1, no TNUM/TDEN
        let data = encode_varint(1);
        assert!(FrameIndexBox::parse(&data).is_err());
    }

    #[test]
    fn test_huge_nf_no_oom() {
        // Crafted input: NF claims billions of entries but the data is tiny.
        // This must not OOM -- Vec::with_capacity should be bounded by data length.
        let mut data = Vec::new();
        data.extend(encode_varint(u32::MAX as u64)); // NF = 4 billion
        data.extend(1u32.to_be_bytes()); // TNUM
        data.extend(1000u32.to_be_bytes()); // TDEN
        // No actual entry data -- parse should fail gracefully, not OOM.
        assert!(FrameIndexBox::parse(&data).is_err());
    }

    #[test]
    fn test_tick_duration() {
        let data = build_frame_index(1, 1000, &[]);
        let index = FrameIndexBox::parse(&data).unwrap();
        assert!((index.tick_duration_secs() - 0.001).abs() < 1e-9);
    }
}
