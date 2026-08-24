// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::collections::{HashMap, VecDeque};

use symphonia_core::errors::{Error, Result, decode_error};
use symphonia_core::io::{BufReader, ReadBytes};

use crate::demuxer::TrackState;
use crate::ebml::{read_signed_vint, read_unsigned_vint};
use crate::segment::{MatroskaTicks, SegmentTicks, SignedTrackTicks, TrackTicks};

enum Lacing {
    None,
    Xiph,
    FixedSize,
    Ebml,
}

fn parse_flags(flags: u8) -> Result<Lacing> {
    match (flags >> 1) & 0b11 {
        0b00 => Ok(Lacing::None),
        0b01 => Ok(Lacing::Xiph),
        0b10 => Ok(Lacing::FixedSize),
        0b11 => Ok(Lacing::Ebml),
        _ => unreachable!(),
    }
}

fn read_ebml_sizes<R: ReadBytes>(mut reader: R, num_frames: usize) -> Result<Vec<u64>> {
    let mut sizes = Vec::with_capacity(num_frames);
    for _ in 0..num_frames {
        if let Some(last_size) = sizes.last().copied() {
            let delta = read_signed_vint(&mut reader)?;
            sizes.push((last_size as i64 + delta) as u64)
        }
        else {
            let size = read_unsigned_vint(&mut reader)?;
            sizes.push(size);
        }
    }

    Ok(sizes)
}

pub(crate) fn read_xiph_sizes<R: ReadBytes>(mut reader: R, num_frames: usize) -> Result<Vec<u64>> {
    let mut sizes = Vec::with_capacity(num_frames);
    let mut prefixes = 0;
    while sizes.len() < num_frames {
        let byte = reader.read_byte()? as u64;
        if byte == 255 {
            prefixes += 1;
        }
        else {
            let size = prefixes * 255 + byte;
            prefixes = 0;
            sizes.push(size);
        }
    }

    Ok(sizes)
}

pub(crate) struct Frame {
    /// The Matroska track number (Symphonia's track ID).
    pub(crate) track_num: u32,
    /// The frame's presentation timestamp.
    pub(crate) pts: SignedTrackTicks,
    /// The frame's duration.
    pub(crate) dur: TrackTicks,
    /// Frame data.
    pub(crate) data: Box<[u8]>,
}

/// Calculate the PTS of a block. This is the PTS of the first frame in the block.
fn calculate_block_pts(
    cluster_ts: SegmentTicks,
    block_rel_ts: SignedTrackTicks,
    track: &TrackState,
) -> Option<SignedTrackTicks> {
    // Convert the cluster timestamp into Track ticks from Segment ticks.
    let cluster_ts = cluster_ts.into_track_ticks(track.track_timestamp_scale).try_into_signed()?;

    cluster_ts.checked_add(block_rel_ts).and_then(|ts| {
        // Codec delay must be converted into Track ticks from Matroska ticks.
        let codec_delay = track.codec_delay.into_track_ticks(track.track_time_base);
        ts.checked_sub_unsigned(codec_delay)
    })
}

/// Iterator-like utility to precisely compute the duration of frames in a block.
struct FrameDurationIter {
    block_dur: TrackTicks,
    num_frames: u64,
    accumulator: u64,
}

impl FrameDurationIter {
    fn new(block_dur: Option<TrackTicks>, track: &TrackState, num_frames: u64) -> Self {
        // If the block duration is known, use it. Otherwise, derive the block duration from the
        // default frame duration if it is known. Otherwise, assume a 0 duration.
        let block_dur = block_dur
            .or_else(|| {
                // Compute the total block duration from the default frame duration. Convert to
                // track ticks after multiplying to maintain as much accuracy as possible. If the
                // multiplication overflows, it won't be possible to calculate the correct duration,
                // so don't.
                track
                    .default_frame_duration
                    .and_then(|frame_dur| {
                        frame_dur.get().checked_mul(num_frames).map(MatroskaTicks::from)
                    })
                    .map(|dur| dur.into_track_ticks(track.track_time_base))
            })
            .unwrap_or_default();
        FrameDurationIter { block_dur, num_frames, accumulator: 0 }
    }

    fn next(&mut self) -> TrackTicks {
        // Accumulate the remainder after each computation since integer division rounds down.
        self.accumulator = self.accumulator.saturating_add(self.block_dur.get());
        let dur = TrackTicks::from(self.accumulator / self.num_frames);
        self.accumulator %= self.num_frames;
        dur
    }
}

pub(crate) fn extract_frames(
    block: &[u8],
    block_duration: Option<TrackTicks>,
    cluster_ts: SegmentTicks,
    tracks: &HashMap<u32, TrackState>,
    frames: &mut VecDeque<Frame>,
) -> Result<bool> {
    let mut reader = BufReader::new(block);
    let track_num = read_unsigned_vint(&mut reader)? as u32;
    let block_rel_ts = SignedTrackTicks::from((reader.read_be_u16()? as i16) as i64);
    let flags = reader.read_byte()?;
    let lacing = parse_flags(flags)?;

    // Get the track associated with the block. It's an error if the track doesn't exist.
    let track =
        tracks.get(&track_num).ok_or(Error::DecodeError("mkv: unvalid track number for block"))?;

    let mut pts = match calculate_block_pts(cluster_ts, block_rel_ts, track) {
        Some(pts) => pts,
        _ => return Ok(false),
    };

    match lacing {
        Lacing::None => {
            let data = reader.read_boxed_slice_exact(block.len() - reader.pos() as usize)?;
            let dur = FrameDurationIter::new(block_duration, track, 1).next();
            frames.push_back(Frame { track_num, pts, data, dur });
        }
        Lacing::Xiph | Lacing::Ebml => {
            // Read number of stored sizes which is actually `number of frames` - 1
            // since size of the last frame is deduced from block size.
            let num_frames = reader.read_byte()? as usize;
            let sizes = match lacing {
                Lacing::Xiph => read_xiph_sizes(&mut reader, num_frames)?,
                Lacing::Ebml => read_ebml_sizes(&mut reader, num_frames)?,
                _ => unreachable!(),
            };

            // The total of all decoded frame sizes should not exceed the block they will be read
            // from.
            let total_laced_size = sizes.iter().try_fold(0u64, |acc, &size| acc.checked_add(size));

            match total_laced_size {
                Some(size) if size <= block.len() as u64 => (),
                _ => return decode_error("mkv: total of laced frame sizes exceeds block"),
            }

            let mut dur_it = FrameDurationIter::new(block_duration, track, num_frames as u64 + 1);

            for frame_size in sizes {
                let data = reader.read_boxed_slice_exact(frame_size as usize)?;
                let dur = dur_it.next();

                frames.push_back(Frame { track_num, pts, data, dur });

                // If PTS overflows, end the stream.
                pts = match pts.checked_add_unsigned(dur) {
                    Some(pts) => pts,
                    None => return Ok(false),
                };
            }

            // Size of last frame is not provided so we read to the end of the block.
            let size = block.len() - reader.pos() as usize;
            let data = reader.read_boxed_slice_exact(size)?;
            frames.push_back(Frame { track_num, pts, data, dur: dur_it.next() });
        }
        Lacing::FixedSize => {
            let num_frames = reader.read_byte()? as usize + 1;
            let total_size = block.len() - reader.pos() as usize;
            if total_size % num_frames != 0 {
                return decode_error("mkv: invalid block size");
            }

            let mut dur_it = FrameDurationIter::new(block_duration, track, num_frames as u64);

            let frame_size = total_size / num_frames;
            for _ in 0..num_frames {
                let data = reader.read_boxed_slice_exact(frame_size)?;
                let dur = dur_it.next();

                frames.push_back(Frame { track_num, pts, data, dur });

                // If PTS overflows, end the stream.
                pts = match pts.checked_add_unsigned(dur) {
                    Some(pts) => pts,
                    None => return Ok(false),
                };
            }
        }
    }

    Ok(true)
}
