// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Helper utilities for implementing media demuxers.

use crate::units::Timestamp;

/// A `SeekPoint` is a mapping between a sample or frame number to byte offset within a media
/// stream.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct SeekPoint {
    /// The frame or sample timestamp of the `SeekPoint`.
    pub frame_ts: Timestamp,
    /// The byte offset of the `SeekPoint`s timestamp relative to a format-specific location.
    pub byte_offset: u64,
    /// The number of frames the `SeekPoint` covers.
    pub n_frames: u32,
}

impl SeekPoint {
    fn new(frame_ts: Timestamp, byte_offset: u64, n_frames: u32) -> Self {
        SeekPoint { frame_ts, byte_offset, n_frames }
    }
}

/// A `SeekIndex` stores `SeekPoint`s (generally a sample or frame number to byte offset) within
/// a media stream and provides methods to efficiently search for the nearest `SeekPoint`(s)
/// given a timestamp.
///
/// A `SeekIndex` does not require complete coverage of the entire media stream. However, the
/// better the coverage, the smaller the manual search range the `SeekIndex` will return.
#[derive(Default)]
pub struct SeekIndex {
    points: Vec<SeekPoint>,
}

/// `SeekSearchResult` is the return value for a search on a `SeekIndex`. It returns a range of
/// `SeekPoint`s a `FormatReader` should search to find the desired timestamp. Ranges are
/// lower-bound inclusive, and upper-bound exclusive.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum SeekSearchResult {
    /// The `SeekIndex` is empty so the desired timestamp could not be found. The entire stream
    /// should be searched for the desired timestamp.
    Stream,
    /// The desired timestamp can be found before, the `SeekPoint`. The stream should be
    /// searched for the desired timestamp from the start of the stream up-to, but not
    /// including, the `SeekPoint`.
    Upper(SeekPoint),
    /// The desired timestamp can be found at, or after, the `SeekPoint`. The stream should be
    /// searched for the desired timestamp starting at the provided `SeekPoint` up-to the end of
    /// the stream.
    Lower(SeekPoint),
    /// The desired timestamp can be found within the range. The stream should be searched for
    /// the desired starting at the first `SeekPoint` up-to, but not-including, the second
    /// `SeekPoint`.
    Range(SeekPoint, SeekPoint),
}

impl SeekIndex {
    /// Create an empty `SeekIndex`
    pub fn new() -> SeekIndex {
        SeekIndex { points: Vec::new() }
    }

    /// Insert a `SeekPoint` into the index.
    pub fn insert(&mut self, ts: Timestamp, byte_offset: u64, n_frames: u32) {
        // Create the seek point.
        let seek_point = SeekPoint::new(ts, byte_offset, n_frames);

        // Get the timestamp of the last entry in the index.
        let (last_ts, last_offset) =
            self.points.last().map_or((Timestamp::MIN, 0), |p| (p.frame_ts, p.byte_offset));

        // If the seek point has a timestamp greater-than and byte offset greater-than or equal to
        // the last entry in the index, then simply append it to the index.
        if ts > last_ts && byte_offset >= last_offset {
            self.points.push(seek_point)
        }
        else if ts < last_ts {
            // If the seek point has a timestamp less-than the last entry in the index, then the
            // insertion point must be found. This case should rarely occur.
            let i =
                self.points.partition_point(|p| ts > p.frame_ts && byte_offset >= p.byte_offset);

            // Insert if the point found or if the points are empty
            if i < self.points.len() || i == 0 {
                self.points.insert(i, seek_point);
            }
        }
    }

    /// Search the index to find a bounded range of bytes wherein the specified frame timestamp
    /// will be contained. If the index is empty, this function simply returns a result
    /// indicating the entire stream should be searched manually.
    pub fn search(&self, frame_ts: Timestamp) -> SeekSearchResult {
        // The index must contain atleast one SeekPoint to return a useful result.
        if !self.points.is_empty() {
            let mut lower = 0;
            let mut upper = self.points.len() - 1;

            // If the desired timestamp is less than the first SeekPoint within the index,
            // indicate that the stream should be searched from the beginning.
            if frame_ts < self.points[lower].frame_ts {
                return SeekSearchResult::Upper(self.points[lower]);
            }
            // If the desired timestamp is greater than or equal to the last SeekPoint within
            // the index, indicate that the stream should be searched from the last SeekPoint.
            else if frame_ts >= self.points[upper].frame_ts {
                return SeekSearchResult::Lower(self.points[upper]);
            }

            // Desired timestamp is between the lower and upper indicies. Perform a binary
            // search to find a range of SeekPoints containing the desired timestamp. The binary
            // search exits when either two adjacent SeekPoints or a single SeekPoint is found.
            while upper - lower > 1 {
                let mid = (lower + upper) / 2;
                let mid_ts = self.points[mid].frame_ts;

                if frame_ts < mid_ts {
                    upper = mid;
                }
                else {
                    lower = mid;
                }
            }

            return SeekSearchResult::Range(self.points[lower], self.points[upper]);
        }

        // The index is empty, the stream must be searched manually.
        SeekSearchResult::Stream
    }
}

#[cfg(test)]
mod tests {
    use crate::units::Timestamp;

    use super::{SeekIndex, SeekPoint, SeekSearchResult};

    #[test]
    fn verify_seek_index_search() {
        let mut index = SeekIndex::new();
        // Normal index insert
        index.insert(Timestamp::new(479232), 706812, 1152);
        index.insert(Timestamp::new(959616), 1421536, 1152);
        index.insert(Timestamp::new(1919232), 2833241, 1152);
        index.insert(Timestamp::new(2399616), 3546987, 1152);
        index.insert(Timestamp::new(2880000), 4259455, 1152);

        // Search for point lower than the first entry
        assert_eq!(
            index.search(Timestamp::new(0)),
            SeekSearchResult::Upper(SeekPoint::new(Timestamp::new(479232), 706812, 1152))
        );

        // Search for point higher than last entry
        assert_eq!(
            index.search(Timestamp::new(3000000)),
            SeekSearchResult::Lower(SeekPoint::new(Timestamp::new(2880000), 4259455, 1152))
        );

        // Search for point that has equal timestamp with some index
        assert_eq!(
            index.search(Timestamp::new(959616)),
            SeekSearchResult::Range(
                SeekPoint::new(Timestamp::new(959616), 1421536, 1152),
                SeekPoint::new(Timestamp::new(1919232), 2833241, 1152)
            )
        );

        // Index insert out of order
        index.insert(Timestamp::new(1440000), 2132419, 1152);
        index.insert(Timestamp::new(-78000), 20, 1152);
        index.insert(Timestamp::new(-50000), 45000, 1152);

        // Search for a negative timestamp.
        assert_eq!(
            index.search(Timestamp::MIN),
            SeekSearchResult::Upper(SeekPoint::new(Timestamp::new(-78000), 20, 1152))
        );

        assert_eq!(
            index.search(Timestamp::new(-69000)),
            SeekSearchResult::Range(
                SeekPoint::new(Timestamp::new(-78000), 20, 1152),
                SeekPoint::new(Timestamp::new(-50000), 45000, 1152)
            )
        );

        // Search for 0 again.
        assert_eq!(
            index.search(Timestamp::new(0)),
            SeekSearchResult::Range(
                SeekPoint::new(Timestamp::new(-50000), 45000, 1152),
                SeekPoint::new(Timestamp::new(479232), 706812, 1152)
            )
        );

        // Search for point that have out of order index when inserting
        assert_eq!(
            index.search(Timestamp::new(1000000)),
            SeekSearchResult::Range(
                SeekPoint::new(Timestamp::new(959616), 1421536, 1152),
                SeekPoint::new(Timestamp::new(1440000), 2132419, 1152)
            )
        );

        // Index insert with byte_offset less than last entry
        index.insert(Timestamp::new(3359232), 0, 0);

        // Search for ignored point because byte_offset less than last entry
        assert_eq!(
            index.search(Timestamp::new(3359232)),
            SeekSearchResult::Lower(SeekPoint::new(Timestamp::new(2880000), 4259455, 1152))
        );
    }
}
