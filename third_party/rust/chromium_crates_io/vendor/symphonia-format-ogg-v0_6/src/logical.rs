// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::collections::VecDeque;

use symphonia_core::errors::{Result, decode_error};
use symphonia_core::formats::Track;
use symphonia_core::packet::{Packet, PacketBuilder};
use symphonia_core::units::{Duration, Timestamp};

use super::common::SideData;
use super::mappings::Mapper;
use super::mappings::{MapResult, PacketParser};
use super::page::Page;

use log::{debug, warn};

#[derive(Copy, Clone, Debug)]
struct Bound {
    /// The page sequence number.
    seq: u32,
    /// Indicates if this is the last page.
    is_last_page: bool,
    /// The start or end timestamp (depends on the type of bound).
    ts: Timestamp,
    /// The samples to discard from the start or end (depends on type of bound).
    discard: Duration,
}

#[derive(Copy, Clone, Debug)]
struct PageInfo {
    /// The page sequence number.
    seq: u32,
    /// Indicates if this is the last page.
    is_last_page: bool,
    /// The page's end timestamp derived from the absolute granule position.
    end_ts: Option<Timestamp>,
}

#[derive(Default)]
pub struct InspectState {
    bound: Option<Bound>,
    parser: Option<Box<dyn PacketParser>>,
}

pub struct LogicalStream {
    mapper: Box<dyn Mapper>,
    packets: VecDeque<Packet>,
    part_buf: Vec<u8>,
    part_len: usize,
    prev_page_info: Option<PageInfo>,
    start_bound: Option<Bound>,
    end_bound: Option<Bound>,
}

impl LogicalStream {
    const MAX_PACKET_LEN: usize = 16 * 1024 * 1024;

    pub fn new(mapper: Box<dyn Mapper>) -> Self {
        LogicalStream {
            mapper,
            packets: Default::default(),
            part_buf: Default::default(),
            part_len: 0,
            prev_page_info: None,
            start_bound: None,
            end_bound: None,
        }
    }

    /// Reset the logical stream after a page discontinuity.
    pub fn reset(&mut self) {
        self.part_len = 0;
        self.prev_page_info = None;
        self.packets.clear();
        self.mapper.reset();
    }

    /// Returns true if the stream is ready.
    pub fn is_ready(&self) -> bool {
        self.mapper.is_ready()
    }

    /// Get the maximum duration between two random access points.
    pub fn max_rap_period(&self) -> Duration {
        self.mapper.max_rap_period()
    }

    /// Get the `Track` for the logical stream.
    pub fn track(&self) -> &Track {
        self.mapper.track()
    }

    /// If known, returns whether the logical stream read the last page.
    pub fn has_read_last_page(&self) -> Option<bool> {
        self.prev_page_info.map(|info| info.is_last_page)
    }

    /// Reads a page.
    pub fn read_page(&mut self, page: &Page<'_>) -> Result<Vec<SideData>> {
        self.read_page_init(page, false)
    }

    /// Read a page. Specifying whether this is the initial bitstream page.
    pub fn read_page_init(&mut self, page: &Page<'_>, is_init_page: bool) -> Result<Vec<SideData>> {
        // Side data vector. This will not allocate unless data is pushed to it (normal case).
        let mut side_data = Vec::new();

        // If the last sequence number is available, detect non-monotonicity and discontinuities
        // in the stream. In these cases, clear any partial packet data.
        if let Some(last_ts) = &self.prev_page_info {
            if page.header.sequence < last_ts.seq {
                warn!("detected stream page non-monotonicity");
                self.part_len = 0;
            }
            else if page.header.sequence - last_ts.seq > 1 {
                warn!(
                    "detected stream discontinuity of {} page(s)",
                    page.header.sequence - last_ts.seq
                );
                self.part_len = 0;
            }
        }

        // Keep a copy of the previous page information.
        let prev_page_info = self.prev_page_info.take();

        // Update with new page information.
        self.prev_page_info = Some(PageInfo {
            seq: page.header.sequence,
            is_last_page: page.header.is_last_page,
            // Propagate the last known valid bitstream timestamp. This will be updated later if
            // a bitstream packet ends in this page.
            end_ts: prev_page_info.and_then(|info| info.end_ts),
        });

        // If there is partial packet data buffered, a continuation page is expected.
        if !page.header.is_continuation && self.part_len > 0 {
            warn!("expected a continuation page");

            // Clear partial packet data.
            self.part_len = 0;
        }

        let mut iter = page.packets();

        // If there is no partial packet data buffered, a continuation page is not expected.
        if page.header.is_continuation && self.part_len == 0 {
            // If the continuation page contains packets, drop the first packet since it would
            // require partial packet data to be complete. Otherwise, ignore this page entirely.
            if page.num_packets() > 0 {
                warn!("unexpected continuation page, ignoring incomplete first packet");
                iter.next();
            }
            else {
                warn!("unexpected continuation page, ignoring page");
                return Ok(side_data);
            }
        }

        let mut total_pkt_dur = Duration::ZERO;
        let mut total_pkt_discard = Duration::ZERO;

        let num_prev_packets = self.packets.len();

        for buf in &mut iter {
            // Get a packet with data from the partial packet buffer, the page, or both.
            let data = self.get_packet(buf);

            // Perform packet mapping. If the packet contains stream data, queue it onto the packet
            // queue. If it contains side data, then add it to the side data list. Ignore other
            // types of packet data.
            match self.mapper.map_packet(&data) {
                Ok(MapResult::StreamData { dur, discard }) => {
                    total_pkt_dur = total_pkt_dur.saturating_add(dur);
                    total_pkt_discard = total_pkt_discard.saturating_add(discard);

                    let packet = PacketBuilder::new()
                        .track_id(page.header.serial)
                        .pts(Timestamp::ZERO)
                        .dur(dur)
                        .data(data)
                        .trim_start(discard)
                        .build();

                    self.packets.push_back(packet);
                }
                Ok(MapResult::SideData { data }) => side_data.push(data),
                Err(e) => {
                    warn!("mapping packet failed ({e}), skipping")
                }
                _ => (),
            }
        }

        // If the page contains partial packet data, then save the partial packet data for later
        // as the packet will be completed on a later page.
        if let Some(buf) = iter.partial_packet() {
            self.save_partial_packet(buf)?;
        }

        // If one or more bitstream packets ended in this page, process them.
        if num_prev_packets < self.packets.len() {
            // The page's timestamp is one past the last valid frame in the last completed packet
            // in this page.
            let page_end_ts = self.mapper.absgp_to_ts(page.header.absgp);

            // Update previous page information with the timestamp.
            let prev_page = self
                .prev_page_info
                .as_mut()
                .expect("prev_page_info unconditionally assigned above");
            prev_page.end_ts = Some(page_end_ts);

            // Compute the page's start timestamp.
            let page_start_ts = if let Some(ts) = prev_page_info.and_then(|prev| prev.end_ts) {
                // The previous page is known and it has a valid end timestamp. Use it as this
                // page's start timestamp.
                ts
            }
            else {
                let is_single_page_stream =
                    page.header.is_last_page && (is_init_page || self.is_single_page_stream());

                // The lower-bound for the page's start timestamp.
                let page_start_ts_raw = page_end_ts.saturating_sub(total_pkt_dur);

                if is_single_page_stream {
                    // In a single-page stream that begins at t = 0, the page end timestamp is the
                    // exact length of valid (no delay or padding) frames. Therefore, the page start
                    // timestamp is known to be equal to -delay frames. All additional frames are
                    // padding.
                    //
                    // If the stream does not start at t = 0, then it is not possible to determine
                    // whether frames should be discarded from the start or the end of the stream.
                    // Therefore, we assume t = 0 for the remainder.
                    let total_dur_no_padding =
                        match total_pkt_discard.checked_add((page_end_ts.get() as u64).into()) {
                            Some(v) => v,
                            None => return decode_error("ogg: duration overflow"),
                        };

                    if total_pkt_dur >= total_dur_no_padding {
                        // The total packet duration is >= delay + valid (assuming t = 0) frames,
                        // the extra frames must be padding.
                        //
                        // If the assumption the stream begins at t = 0 is false, then the following
                        // misbehaviour will occur:
                        //
                        // If the encoder set t < 0 to discard additional frames at the start of the
                        // stream, then the additional frames will be discard from the end, not the
                        // beginning.
                        //
                        // If the encoder set t > 0 to indicate the media begins later, then no
                        // padding frames will get discarded.
                        Timestamp::from(-(total_pkt_discard.get() as i64))
                    }
                    else {
                        // Stream starts at t > 0.
                        page_start_ts_raw
                    }
                }
                else {
                    // In a multi-page stream, all pages other than the last have no padding.
                    // Therefore, the naive calculation is always valid because the total packet
                    // duration would only include valid or discarded frames.
                    //
                    // For the last page, this calculation fails because the total packet duration
                    // includes the padding and it is not possible to know how many frames are
                    // valid. If the end bound is known, the correct page start timestamp can be
                    // found.
                    page_start_ts_raw
                }
            };

            let mut next_pkt_pts = page_start_ts;

            for packet in self.packets.iter_mut().skip(num_prev_packets) {
                packet.pts = next_pkt_pts;

                // The packet's duration is populated with the packet's decoded duration (includes
                // delay and padding). Calculate the next packet's PTS. This is also the end PTS of
                // the current packet.
                next_pkt_pts = next_pkt_pts.saturating_add(packet.dur);

                // Remove the start trim from the packet's duration. The start trim was populated
                // earlier.
                packet.dur = packet.dur.saturating_sub(packet.trim_start);

                // If the end of the current packet exceeds the page end, trim the end. Don't trim
                // more frames than available.
                if next_pkt_pts > page_end_ts {
                    packet.trim_end = page_end_ts.abs_delta(next_pkt_pts).min(packet.dur);
                }

                // Remove the end trim from the packet's duration.
                packet.dur = packet.dur.saturating_sub(packet.trim_end);
            }
        }

        Ok(side_data)
    }

    /// Returns true if the logical stream has packets buffered.
    pub fn has_packets(&self) -> bool {
        !self.packets.is_empty()
    }

    /// Examine, but do not consume, the next packet.
    pub fn peek_packet(&self) -> Option<&Packet> {
        self.packets.front()
    }

    /// Consumes and returns the next packet.
    pub fn next_packet(&mut self) -> Option<Packet> {
        self.packets.pop_front()
    }

    /// Consumes the next packet.
    pub fn consume_packet(&mut self) {
        self.packets.pop_front();
    }

    /// Examine the first page of the non-setup codec bitstream to obtain the start time and start
    /// delay parameters.
    pub fn inspect_start_page(&mut self, page: &Page<'_>) {
        if self.start_bound.is_some() {
            debug!("start page already found");
            return;
        }

        let mut parser = match self.mapper.make_parser() {
            Some(parser) => parser,
            _ => {
                debug!("failed to make start bound packet parser");
                return;
            }
        };

        // Sum the total duration of all packets, and the amount to discard.
        let mut total_pkt_dur = Duration::ZERO;
        let mut total_pkt_discard = Duration::ZERO;

        for buf in page.packets() {
            let (pkt_dur, pkt_discard) = parser.parse_next_packet_dur(buf);

            // On overflow, it will not be possible to determine the start bound.
            total_pkt_dur = match total_pkt_dur.checked_add(pkt_dur) {
                Some(total) => total,
                _ => return,
            };
            total_pkt_discard = match total_pkt_discard.checked_add(pkt_discard) {
                Some(total) => total,
                _ => return,
            };
        }

        // Map the absolute granule position to a timestamp.
        let page_end_ts = self.mapper.absgp_to_ts(page.header.absgp);

        // For single page streams, the first page is also the last page. If the page is
        // explicitly marked as the last page, then we assume the stream starts and ends on the
        // same page. Therefore, we can only assume it starts at pts=0, and any discarded frames
        // are padding frames, not delay frames.
        let page_start_ts = if !page.header.is_last_page {
            match page_end_ts.checked_sub(total_pkt_dur) {
                Some(ts) => ts,
                _ => return,
            }
        }
        else {
            Timestamp::new(-(total_pkt_discard.get() as i64))
        };

        let bound = Bound {
            seq: page.header.sequence,
            is_last_page: page.header.is_last_page,
            ts: page_start_ts,
            discard: total_pkt_discard,
        };

        // Update codec parameters.
        let track = self.mapper.track_mut();

        track.with_start_ts(bound.ts);

        if bound.discard > Duration::ZERO {
            track.with_delay(bound.discard.get() as u32);
        }

        // Update start bound.
        self.start_bound = Some(bound);
    }

    /// Examines one or more of the last pages of the codec bitstream to obtain the end time and
    /// end delay parameters. To obtain the end delay, at a minimum, the last two pages are
    /// required. The state returned by each iteration of this function should be passed into the
    /// subsequent iteration.
    pub fn inspect_end_page(&mut self, state: &mut InspectState, page: &Page<'_>) {
        // Do nothing if the end bound was found.
        if self.end_bound.is_some() {
            debug!("end page already found");
            return;
        }

        // Get and/or create the packet parser.
        let parser = match &mut state.parser {
            Some(parser) => parser,
            None => {
                state.parser = self.mapper.make_parser();

                let Some(parser) = &mut state.parser
                else {
                    debug!("failed to make end bound packet parser");
                    return;
                };

                parser
            }
        };

        // Calculate the page duration. Note that even though only the last page uses this duration,
        // it is important to feed the packet parser so that the first packet of the final page
        // doesn't have a duration of 0 due to lapping on some codecs.
        let mut total_pkt_dur = Duration::ZERO;

        for buf in page.packets() {
            let (pkt_dur, _) = parser.parse_next_packet_dur(buf);

            // On overflow it will be impossible to determine the end bound.
            total_pkt_dur = match total_pkt_dur.checked_add(pkt_dur) {
                Some(total) => total,
                _ => return,
            };
        }

        // Map the absolute granule position to a timestamp.
        let page_end_ts = self.mapper.absgp_to_ts(page.header.absgp);

        // The end delay can only be determined if this is the last page, and the timstamp of the
        // second last page is known, or this is the only page of the stream.
        let end_delay = if page.header.is_last_page {
            if let Some(last_bound) = &state.bound {
                // The actual ending timestamp of the decoded data is the timestamp of the previous
                // page plus the decoded duration of this page. Subtract the stated page end
                // timestamp to determine the padding. On overflow, it'll be impossible to determine
                // the end delay, so force it to be 0.
                last_bound
                    .ts
                    .checked_add(total_pkt_dur)
                    .map(|actual_page_end_ts| actual_page_end_ts.abs_delta(page_end_ts))
            }
            else if self.start_bound.is_some_and(|b| b.seq == page.header.sequence) {
                // The start and end page is the same page. The end delay is the amount of excess
                // page duration after subtracting the page's end timestamp.

                // The amount of start delay, if available.
                let delay = self.start_bound.map(|s| s.discard).unwrap_or(Duration::ZERO);

                // The page end timestamp does not include delay or padding. Therefore, it is also
                // the duration of valid content assuming it is non-negative.
                let valid = page_end_ts.duration_from(Timestamp::ZERO);

                // The amount of valid content plus padding.
                let valid_and_padding = total_pkt_dur.checked_sub(delay);

                // Take the valid duration from the valid and padding duration to yield the end
                // padding.
                valid_and_padding
                    .zip(valid)
                    .and_then(|(valid_and_padding, valid)| valid_and_padding.checked_sub(valid))
            }
            else {
                // Don't have the timestamp of the previous page so it is not possible to
                // calculate the end delay.
                None
            }
        }
        else {
            // Only the last page can have an end delay.
            None
        };

        let bound = Bound {
            seq: page.header.sequence,
            is_last_page: page.header.is_last_page,
            ts: page_end_ts,
            discard: end_delay.unwrap_or(Duration::ZERO),
        };

        // If this is the last page, update the codec parameters.
        if page.header.is_last_page {
            // TODO: What if this is negative?
            // TODO: Bounds are in timestamp units, not number of frames. Fix this up because it
            // only works for audio codecs.
            let num_frames = bound.ts.get() as u64;
            let num_padding_frames = bound.discard.get() as u32;

            let track = self.mapper.track_mut();

            track.with_num_frames(num_frames);
            track.with_duration(Duration::from(num_frames));

            if num_padding_frames > 0 {
                track.with_padding(num_padding_frames);
            }

            self.end_bound = Some(bound);
        }

        // Update the state's bound.
        state.bound = Some(bound);
    }

    /// Examine a page in isolation and return the start and end timestamps as a tuple.
    pub fn inspect_page(&mut self, page: &Page<'_>) -> (Timestamp, Timestamp) {
        // Get the cumulative duration of all packets within this page.
        let mut total_pkt_dur = Duration::ZERO;
        let mut total_pkt_discard = Duration::ZERO;

        if let Some(mut parser) = self.mapper.make_parser() {
            for buf in page.packets() {
                let (pkt_dur, pkt_discard) = parser.parse_next_packet_dur(buf);

                total_pkt_dur = total_pkt_dur.saturating_add(pkt_dur);
                total_pkt_discard = total_pkt_discard.saturating_add(pkt_discard);
            }
        }

        let page_end_ts = self.mapper.absgp_to_ts(page.header.absgp);

        // Map the absolute granule position to a timestamp.
        let page_start_ts_raw = page_end_ts.saturating_sub(total_pkt_dur);

        let page_start_ts = match self.start_bound {
            // Start bound is known, and this is the first bitstream page.
            Some(b) if b.seq == page.header.sequence => b.ts,
            // Start bound is known, but this is not the first bitstream page.
            Some(_) => page_start_ts_raw,
            // Start bound is not known. Assume this is the first bitstream page.
            None if page_start_ts_raw.is_negative() => {
                Timestamp::new(-(total_pkt_discard.get() as i64))
            }
            // Start bound is not known, but the page start timestamp is positive. The
            // stream likely started with a positive timestamp.
            None => page_start_ts_raw,
        };

        (page_start_ts, page_end_ts)
    }

    /// Returns true if the stream is only a single page.
    fn is_single_page_stream(&self) -> bool {
        match self.start_bound {
            // The starting page is known, and it is marked as the last page. This is a single-page
            // stream.
            Some(start) if start.is_last_page => true,
            // The starting page is known, and it is not marked as the last page. This can be
            // assumed to be a multi-page stream. NOTE: This assumption relies on the starting page
            // being the page with first completed packet.
            Some(start) => {
                // However, if the starting and ending pages are known and have mismatched sequence
                // numbers (this shouldn't happen), then the previous assumption was false.
                match self.end_bound {
                    Some(end) => start.seq == end.seq,
                    _ => true,
                }
            }
            // The starting page is not known, cannot assume this is a single-page stream.
            _ => false,
        }
    }

    fn get_packet(&mut self, packet_buf: &[u8]) -> Box<[u8]> {
        if self.part_len == 0 {
            Box::from(packet_buf)
        }
        else {
            let mut buf = vec![0u8; self.part_len + packet_buf.len()];

            // Split packet buffer into two portions: saved and new.
            let (vec0, vec1) = buf.split_at_mut(self.part_len);

            // Copy and consume the saved partial packet.
            vec0.copy_from_slice(&self.part_buf[..self.part_len]);
            self.part_len = 0;

            // Read the remainder of the partial packet from the page.
            vec1.copy_from_slice(packet_buf);

            buf.into_boxed_slice()
        }
    }

    fn save_partial_packet(&mut self, buf: &[u8]) -> Result<()> {
        let new_part_len = self.part_len + buf.len();

        if new_part_len > self.part_buf.len() {
            // Do not exceed an a certain limit to prevent unbounded memory growth.
            if new_part_len > LogicalStream::MAX_PACKET_LEN {
                return decode_error("ogg: packet buffer would exceed max size");
            }

            // New partial packet buffer size, rounded up to the nearest 8K block.
            let new_buf_len = (new_part_len + (8 * 1024 - 1)) & !(8 * 1024 - 1);
            debug!("grow packet buffer to {new_buf_len} bytes");

            self.part_buf.resize(new_buf_len, Default::default());
        }

        self.part_buf[self.part_len..new_part_len].copy_from_slice(buf);
        self.part_len = new_part_len;

        Ok(())
    }
}
