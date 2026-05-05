// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The `packet` module defines the packet structure.

use crate::io::BufReader;
use crate::units::{Duration, Timestamp};

/// A `Packet` contains a discrete amount of encoded data for a single codec bitstream. The exact
/// amount of data is bounded, but not defined, and is dependant on the container and/or the
/// encapsulated codec.
///
/// # Timing
///
/// Packets carry various timing information to support the decoding process. Generally, this
/// timing information is read directly from the media container but it may also be synthesized by
/// a format reader if it is not explicitly signalled.
///
/// * **Presentation Timestamp (PTS):** The time relative to the start of the stream that the
///   decoded packet should be presented.
///
/// * **Decode Timestamp (DTS):** The time relative to the start of the stream that the packet
///   should be decoded. The DTS is usually the same as PTS for audio.
///
/// * **Duration:** The duration of all *valid* frames in the packet. Equal to the duration of all
///   *decoded* frames if there is no trimming.
///
/// * **Trim Start/End:** The duration of frames that should be trimmed from the start/end of the
///   decoded packet before presentation. The sum of the duration and trim start/end equals the
///   duration of *decoded* frames.
///
/// Take note of the difference between *valid* and *decoded* frames. Valid frames are frames that
/// should be presented (played back) to the user, while decoded frames include any encoder delay
/// and/or padding frames. The latter are generally discarded by the decoder. The duration of all
/// *decoded* frames is also called the block duration.
///
/// # For Implementers
///
/// When synthesizing PTS, negative PTS should be used for encoder delay frames. However, this is
/// not strictly mandatory. Encoder delay and padding frames are ultimately signalled using the trim
/// fields. Regardless, the `dur` field must only be populated with the duration of *valid* frames,
/// while the sum of `dur`, `trim_start`, and `trim_end` must equal the amount of frames the decoder
/// would produce if no trimming occurs.
#[derive(Clone)]
pub struct Packet {
    /// The track ID.
    track_id: u32,
    /// The presentation timestamp (PTS) of the packet in `TimeBase` units.
    pub pts: Timestamp,
    /// The decode timestamp (DTS) of the packet in `TimeBase` units.
    pub dts: Timestamp,
    /// The duration of all *valid* frames in the packet in `TimeBase` units.
    ///
    /// This duration excludes any delay or padding frames that may be produced by the decoder.
    pub dur: Duration,
    /// The duration of *decoded* frames that should be trimmed from the start of the decoded
    /// buffer to remove encoder delay.
    pub trim_start: Duration,
    /// The duration of *decoded* frames that should be trimmed from the end of the decoded
    /// buffer to remove encoder padding.
    pub trim_end: Duration,
    /// The packet data buffer.
    pub data: Box<[u8]>,
}

impl Packet {
    /// Create a new untrimmed `Packet`.
    pub fn new(track_id: u32, pts: Timestamp, dur: Duration, data: impl Into<Box<[u8]>>) -> Self {
        Packet {
            track_id,
            pts,
            dts: pts,
            dur,
            trim_start: Duration::ZERO,
            trim_end: Duration::ZERO,
            data: data.into(),
        }
    }

    /// The track identifier of the track this packet belongs to.
    #[inline]
    pub const fn track_id(&self) -> u32 {
        self.track_id
    }

    /// Get the presentation timestamp (PTS) of the packet in `TimeBase` units.
    ///
    /// This is the time relative to the start of the media that the decoded packet should be
    /// presented to the user.
    #[inline]
    pub const fn pts(&self) -> Timestamp {
        self.pts
    }

    /// Get the decode timestamp (DTS) of the packet in `TimeBase` units.
    ///
    /// This is the time relative to the start of the media that the packet should be decoded.
    #[inline]
    pub const fn dts(&self) -> Timestamp {
        self.dts
    }

    /// Get the duration of all *valid* frames in the packet in `TimeBase` units.
    ///
    /// This duration excludes any delay or padding frames that may be produced by the decoder.
    /// Generally, delay or padding frames should not be presented to the user.
    #[inline]
    pub const fn dur(&self) -> Duration {
        self.dur
    }

    /// Get the duration of all *decoded* frames in the packet in `TimeBase` units.
    ///
    /// This duration includes any delay or padding frames that may be produced by the decoder. As
    /// such, this is a sum of the duration, start trim, and end trim.
    #[inline]
    pub const fn block_dur(&self) -> Duration {
        self.dur.saturating_add(self.trim_start).saturating_add(self.trim_end)
    }

    /// Get the duration of *decoded* frames that should be trimmed from the start of the decoded
    /// buffer to remove encoder delay.
    #[inline]
    pub const fn trim_start(&self) -> Duration {
        self.trim_start
    }

    /// Get the duration of *decoded* frames that should be trimmed from the end of the decoded
    /// buffer to remove encoder padding.
    #[inline]
    pub const fn trim_end(&self) -> Duration {
        self.trim_end
    }

    /// Get an immutable slice to the packet data buffer.
    #[inline]
    pub const fn buf(&self) -> &[u8] {
        &self.data
    }

    /// Get a `BufReader` to read the packet data buffer sequentially.
    #[inline]
    pub fn as_buf_reader(&self) -> BufReader<'_> {
        BufReader::new(&self.data)
    }
}

mod builder {
    use crate::packet::Packet;
    use crate::units::{Duration, Timestamp};

    pub struct HasTrackId(u32);
    pub struct NoTrackId;

    pub struct HasPts(Timestamp);
    pub struct NoPts;

    pub struct HasDur(Duration);
    pub struct NoDur;

    pub struct HasBuf(Box<[u8]>);
    pub struct NoBuf;

    /// A builder for creating packets.
    ///
    /// See [`Packet`] for a detailed description of all packet fields.
    ///
    /// The track ID, PTS, duration, and data fields are mandatory and must be provided before a
    /// packet can be built.
    pub struct PacketBuilder<T, P, D, B> {
        track_id: T,
        pts: P,
        dur: D,
        buf: B,
        dts: Option<Timestamp>,
        trim_start: Duration,
        trim_end: Duration,
    }

    impl Default for PacketBuilder<NoTrackId, NoPts, NoDur, NoBuf> {
        fn default() -> Self {
            Self::new()
        }
    }

    impl PacketBuilder<NoTrackId, NoPts, NoDur, NoBuf> {
        /// Create the packet builder.
        pub fn new() -> Self {
            Self {
                track_id: NoTrackId,
                pts: NoPts,
                dur: NoDur,
                buf: NoBuf,
                dts: None,
                trim_start: Duration::ZERO,
                trim_end: Duration::ZERO,
            }
        }
    }

    impl PacketBuilder<HasTrackId, HasPts, HasDur, HasBuf> {
        /// Build the packet.
        pub fn build(self) -> Packet {
            Packet {
                track_id: self.track_id.0,
                pts: self.pts.0,
                dts: self.dts.unwrap_or(self.pts.0),
                dur: self.dur.0,
                trim_start: self.trim_start,
                trim_end: self.trim_end,
                data: self.buf.0,
            }
        }
    }

    impl<T, B> PacketBuilder<T, HasPts, NoDur, B> {
        /// Provide the packet's duration and calculate the trim fields.
        ///
        /// Given the provided duration including delay and padding frames, the stream end timestamp
        /// (optional), and the previously provided PTS, this function calculates the packet's
        /// duration, trim start, and trim end.
        ///
        /// This helper assumes that all frames with a PTS < 0 are to be trimmed from the start.
        /// Frames whose PTS exceeds the end timestamp of the stream are trimmed from the end. The
        /// trim end will only be calculated if the stream's end timestamp is provided.
        pub fn trimmed_dur(
            self,
            block_dur: Duration,
            end_pts: Option<Timestamp>,
        ) -> PacketBuilder<T, HasPts, HasDur, B> {
            let Self { track_id, pts, buf, dts, .. } = self;

            // All frames with a negative PTS must be trimmed first. This duration may exceed the
            // number of decoded frames.
            let negative = pts.0.duration_to(Timestamp::ZERO).unwrap_or(Duration::ZERO);

            // Cap to the number of decoded frames.
            let trim_start = negative.min(block_dur);
            let mut trim_end = Duration::ZERO;

            // It is only possible to trim the end of a packet if the end PTS is known.
            if let Some(end_pts) = end_pts {
                if let Some(pkt_end_pts) = pts.0.checked_add(block_dur) {
                    trim_end = pkt_end_pts.duration_from(end_pts).unwrap_or(Duration::ZERO);
                }
            }

            let dur = block_dur.saturating_sub(self.trim_start).saturating_sub(self.trim_end);

            PacketBuilder { track_id, pts, dur: HasDur(dur), buf, dts, trim_start, trim_end }
        }
    }

    impl<T, P, B> PacketBuilder<T, P, NoDur, B> {
        /// Provide the packet's duration including delay and padding frames.
        pub fn dur(self, dur: Duration) -> PacketBuilder<T, P, HasDur, B> {
            let Self { track_id, pts, buf, dts, trim_start, trim_end, .. } = self;
            PacketBuilder { track_id, pts, dur: HasDur(dur), buf, dts, trim_start, trim_end }
        }
    }

    impl<T, P, D, B> PacketBuilder<T, P, D, B> {
        /// Provide the track ID.
        pub fn track_id(self, track_id: u32) -> PacketBuilder<HasTrackId, P, D, B> {
            let Self { pts, dur, buf, dts, trim_start, trim_end, .. } = self;
            PacketBuilder {
                track_id: HasTrackId(track_id),
                pts,
                dur,
                buf,
                dts,
                trim_start,
                trim_end,
            }
        }

        /// Provide the presentation timestamp (PTS).
        pub fn pts(self, pts: Timestamp) -> PacketBuilder<T, HasPts, D, B> {
            let Self { track_id, dur, buf, dts, trim_start, trim_end, .. } = self;
            PacketBuilder { track_id, pts: HasPts(pts), dur, buf, dts, trim_start, trim_end }
        }

        /// Provide the packet's data buffer.
        pub fn data(self, buf: impl Into<Box<[u8]>>) -> PacketBuilder<T, P, D, HasBuf> {
            let Self { track_id, pts, dur, dts, trim_start, trim_end, .. } = self;
            PacketBuilder { track_id, pts, dur, buf: HasBuf(buf.into()), dts, trim_start, trim_end }
        }

        /// Provide the decode timestamp (DTS).
        pub fn dts(mut self, dts: Timestamp) -> Self {
            self.dts = Some(dts);
            self
        }

        /// Provide the trim start duration.
        pub fn trim_start(mut self, trim_start: Duration) -> Self {
            self.trim_start = trim_start;
            self
        }

        /// Provide the trim end duration.
        pub fn trim_end(mut self, trim_end: Duration) -> Self {
            self.trim_end = trim_end;
            self
        }
    }
}

pub use builder::PacketBuilder;
