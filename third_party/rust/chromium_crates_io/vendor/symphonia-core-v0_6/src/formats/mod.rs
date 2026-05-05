// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The `format` module provides the traits and support structures necessary to implement media
//! demuxers.

use std::fmt;

use crate::codecs::{CodecParameters, audio, subtitle, video};
use crate::common::FourCc;
use crate::errors::Result;
use crate::io::MediaSourceStream;
use crate::meta::{ChapterGroup, Metadata, MetadataLog};
use crate::packet::Packet;
use crate::units::{Duration, Time, TimeBase, Timestamp};

use bitflags::bitflags;

pub mod prelude {
    //! The `formats` module prelude for format reader implementers.

    pub use crate::meta::{Chapter, ChapterGroup, ChapterGroupItem};
    pub use crate::packet::{Packet, PacketBuilder};
    pub use crate::units::{Duration, TimeBase, Timestamp};

    pub use super::{
        Attachment, FileAttachment, FormatId, FormatInfo, FormatOptions, FormatReader, SeekMode,
        SeekTo, SeekedTo, Track, VendorDataAttachment,
    };
}

pub mod probe;

/// A `FormatId` is a unique identifier used to identify a specific container format.
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub struct FormatId(u32);

impl FormatId {
    /// Create a new format ID from a FourCC.
    pub const fn new(cc: FourCc) -> FormatId {
        // A FourCc always only contains ASCII characters. Therefore, the upper bits are always 0.
        Self(0x8000_0000 | u32::from_be_bytes(cc.get()))
    }
}

impl From<FourCc> for FormatId {
    fn from(value: FourCc) -> Self {
        FormatId::new(value)
    }
}

impl fmt::Display for FormatId {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:#x}", self.0)
    }
}

/// Null container format
pub const FORMAT_ID_NULL: FormatId = FormatId(0x0);

/// Basic information about a container format.
#[derive(Copy, Clone)]
pub struct FormatInfo {
    /// The `FormatId` identifier.
    pub format: FormatId,
    /// A short ASCII-only string identifying the format.
    pub short_name: &'static str,
    /// A longer, more descriptive, string identifying the format.
    pub long_name: &'static str,
}

/// `SeekTo` specifies a position to seek to.
pub enum SeekTo {
    /// Seek to a `Time` in regular time units.
    Time {
        /// The `Time` to seek to.
        time: Time,
        /// If `Some`, specifies which track's timestamp should be returned after the seek. If
        /// `None`, then the default track's timestamp is returned. If the container does not have
        /// a default track, then the first track's timestamp is returned.
        track_id: Option<u32>,
    },
    /// Seek to a track's `TimeStamp` in that track's timebase units.
    TimeStamp {
        /// The `TimeStamp` to seek to.
        ts: Timestamp,
        /// Specifies which track `ts` is relative to.
        track_id: u32,
    },
}

/// `SeekedTo` is the result of a seek.
#[derive(Copy, Clone, Debug)]
pub struct SeekedTo {
    /// The track the seek was relative to.
    pub track_id: u32,
    /// The `TimeStamp` required for the requested seek.
    pub required_ts: Timestamp,
    /// The `TimeStamp` that was seeked to.
    pub actual_ts: Timestamp,
}

/// `SeekMode` selects the precision of a seek.
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum SeekMode {
    /// Coarse seek mode is a best-effort attempt to seek to the requested position. The actual
    /// position seeked to may be before or after the requested position. Coarse seeking is an
    /// optional performance enhancement. If a `FormatReader` does not support this mode an
    /// accurate seek will be performed instead.
    Coarse,
    /// Accurate (aka sample-accurate) seek mode will be always seek to a position before the
    /// requested position.
    Accurate,
}

/// `FormatOptions` is a common set of options that all demuxers use.
#[non_exhaustive]
#[derive(Clone, Debug)]
pub struct FormatOptions {
    /// If a `FormatReader` requires a seek index, but the container does not provide one, build the
    /// seek index during instantiation instead of building it progressively.
    ///
    /// Default: `false`.
    pub prebuild_seek_index: bool,
    /// If a seek index needs to be built, this value determines the period, in milliseconds, at
    /// which a new entry is added to the seek index. For example, if set to 500 ms, then two
    /// entries are added to the seek index for every 1 second of media.
    ///
    /// Default: `1000`.
    ///
    /// Note: This is a CPU vs. memory trade-off. A high value will increase the amount of IO
    /// required during a seek, whereas a low value will require more memory. The default chosen is
    /// a good compromise for casual playback of music, podcasts, movies, etc. However, for
    /// highly-interactive applications, this value should be decreased.
    pub seek_index_fill_period_ms: u16,
    /// External, supplementary, data related to the media container read before the start of the
    /// container, or provided through some other side-channel.
    pub external_data: ExternalFormatData,
}

/// `ExternalFormatData` contains supplementary data related to the media container that was read
/// before the start of the container, or provided through some other side-channel.
#[derive(Clone, Debug, Default)]
pub struct ExternalFormatData {
    /// Optional metadata.
    ///
    /// When provided, the `FormatReader` will take the metadata revisions in this log and use them
    /// as them as the first metdata revisions for the container.
    pub metadata: Option<MetadataLog>,
    /// Optional chapter information.
    pub chapters: Option<ChapterGroup>,
}

impl Default for FormatOptions {
    fn default() -> Self {
        FormatOptions {
            prebuild_seek_index: false,
            seek_index_fill_period_ms: 1000,
            external_data: Default::default(),
        }
    }
}

impl FormatOptions {
    /// If a `FormatReader` requires a seek index, but the container does not provide one, build the
    /// seek index during instantiation instead of building it progressively.
    ///
    /// Default: `false`.
    pub fn prebuild_seek_index(mut self, prebuild: bool) -> Self {
        self.prebuild_seek_index = prebuild;
        self
    }

    /// If a seek index needs to be built, this value determines the period, in milliseconds, at
    /// which a new entry is added to the seek index. For example, if set to 500 ms, then two
    /// entries are added to the seek index for every 1 second of media.
    ///
    /// Default: `1000`.
    ///
    /// Note: This is a CPU vs. memory trade-off. A high value will increase the amount of IO
    /// required during a seek, whereas a low value will require more memory. The default chosen is
    /// a good compromise for casual playback of music, podcasts, movies, etc. However, for
    /// highly-interactive applications, this value should be decreased.
    pub fn seek_index_fill_period_ms(mut self, period: u16) -> Self {
        self.seek_index_fill_period_ms = period;
        self
    }
}

bitflags! {
    /// Flags indicating certain attributes about a track.
    #[derive(Copy, Clone, Debug, Default, PartialEq, Eq)]
    pub struct TrackFlags: u32 {
        /// The track is the default track for its track type.
        const DEFAULT           = 1 << 0;
        /// The track should be played even if user or player settings normally wouldn't call for
        /// it.
        ///
        /// For example, the forced flag may be set on an English subtitle track so that it is
        /// always played even if the audio language is also English.
        const FORCED            = 1 << 1;
        /// The track is in the original language.
        const ORIGINAL_LANGUAGE = 1 << 2;
        /// The track contains commentary.
        const COMMENTARY        = 1 << 3;
        /// The track is suitable for the hearing impaired.
        const HEARING_IMPAIRED  = 1 << 4;
        /// The track is suitable for the visually impaired.
        const VISUALLY_IMPAIRED = 1 << 5;
        /// The track contains text descriptions of visual content.
        const TEXT_DESCRIPTIONS = 1 << 6;
    }
}

/// The track type.
#[non_exhaustive]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub enum TrackType {
    /// An audio track.
    Audio,
    /// A video track.
    Video,
    /// A subtitle track.
    Subtitle,
}

/// A `Track` is an independently coded media bitstream. A media format may contain multiple tracks
/// in one container. Each of those tracks are represented by one `Track`.
#[derive(Clone, Debug)]
pub struct Track {
    /// A unique identifier for the track.
    ///
    /// For most formats this is usually the zero-based index of the track, however, some more
    /// complex formats set this differently.
    pub id: u32,
    /// The codec parameters for the track.
    ///
    /// If `None`, the format reader was unable to determine the codec parameters and the track will
    /// be unplayable.
    pub codec_params: Option<CodecParameters>,
    /// The language of the track. May be unknown or not set.
    pub language: Option<String>,
    /// The timebase of the track.
    ///
    /// The timebase is the length of time in seconds of a single tick of a timestamp or duration.
    /// It can be used to convert any timestamp or duration related to the track into seconds.
    pub time_base: Option<TimeBase>,
    /// The length of the track in number of audio, video, or subtitle frames.
    ///
    /// This count excludes any delay or padding frames. In other words, it is the number of
    /// playable (non-discarded) frames.
    ///
    /// Generally, when presenting a track's duration, the `duration` field should be used instead.
    /// This is because a track's timebase may not always be the reciprocal of the sample or frame
    /// rate, resulting in slight differences between the duration as stated in the container
    /// (in timebase units) vs. when calculated from the number of playable frames.
    pub num_frames: Option<u64>,
    /// The duration of the track in timebase units.
    ///
    /// If a timebase is available, this field can be used to calculate the total duration of the
    /// track in seconds by using [`TimeBase::calc_time`] and passing the duration as the argument.
    pub duration: Option<Duration>,
    /// The timestamp of the first frame.
    pub start_ts: Timestamp,
    /// The number of leading frames inserted by the encoder that should be skipped during playback.
    pub delay: Option<u32>,
    /// The number of trailing frames inserted by the encoder for padding that should be skipped
    /// during playback.
    pub padding: Option<u32>,
    /// Flags indicating track attributes.
    pub flags: TrackFlags,
}

impl Track {
    /// Instantiate a new track with a given ID.
    pub fn new(id: u32) -> Self {
        Track {
            id,
            codec_params: None,
            language: None,
            time_base: None,
            num_frames: None,
            duration: None,
            start_ts: Timestamp::new(0),
            delay: None,
            padding: None,
            flags: TrackFlags::empty(),
        }
    }

    /// Provide the codec parameters.
    ///
    /// Note: If the codec parameters contains a non-zero sample or frame rate that, a default
    /// timebase will be derived.
    pub fn with_codec_params(&mut self, codec_params: CodecParameters) -> &mut Self {
        // Derive a timebase from the sample/frame rate if one is not already set.
        if self.time_base.is_none() {
            self.time_base = match &codec_params {
                CodecParameters::Audio(params) => {
                    params.sample_rate.and_then(TimeBase::try_from_recip)
                }
                _ => None,
            };
        }

        self.codec_params = Some(codec_params);
        self
    }

    /// Provide the track language.
    pub fn with_language(&mut self, language: &str) -> &mut Self {
        self.language = Some(language.to_string());
        self
    }

    /// Provide the `TimeBase`.
    pub fn with_time_base(&mut self, time_base: TimeBase) -> &mut Self {
        self.time_base = Some(time_base);
        self
    }

    /// Provide the total number of frames.
    pub fn with_num_frames(&mut self, num_frames: u64) -> &mut Self {
        self.num_frames = Some(num_frames);
        self
    }

    /// Provide the duration in timebase units.
    pub fn with_duration(&mut self, duration: Duration) -> &mut Self {
        self.duration = Some(duration);
        self
    }

    /// Provide the timestamp of the first frame.
    pub fn with_start_ts(&mut self, start_ts: Timestamp) -> &mut Self {
        self.start_ts = start_ts;
        self
    }

    /// Provide the number of delay frames.
    pub fn with_delay(&mut self, delay: u32) -> &mut Self {
        self.delay = Some(delay);
        self
    }

    /// Provide the number of padding frames.
    pub fn with_padding(&mut self, padding: u32) -> &mut Self {
        self.padding = Some(padding);
        self
    }

    /// Append provided track flags.
    pub fn with_flags(&mut self, flags: TrackFlags) -> &mut Self {
        self.flags |= flags;
        self
    }
}

/// An attachment is additional data that is carried along with the container format.
pub enum Attachment {
    /// A file.
    File(FileAttachment),
    /// Application or vendor-specific data.
    VendorData(VendorDataAttachment),
}

/// A file attachment.
pub struct FileAttachment {
    /// The file name.
    pub name: String,
    /// An optional description of the file.
    pub description: Option<String>,
    /// An optional media-type describing the file data.
    pub media_type: Option<String>,
    /// The file data.
    pub data: Box<[u8]>,
}

/// Application or vendor-specific proprietary binary data attachment.
#[derive(Clone, Debug)]
pub struct VendorDataAttachment {
    /// A text representation of the vendor's application identifier.
    pub ident: String,
    /// The vendor data.
    pub data: Box<[u8]>,
}

/// A `FormatReader` is a container demuxer. It provides methods to probe a media container for
/// information and access the tracks encapsulated in the container.
///
/// Most, if not all, media containers contain metadata, then a number of packetized, and
/// interleaved codec bitstreams. These bitstreams are usually referred to as tracks. Generally,
/// the encapsulated bitstreams are independently encoded using some codec. The allowed codecs for a
/// container are defined in the specification of the container format.
///
/// While demuxing, packets are read one-by-one and may be discarded or decoded at the choice of
/// the caller. The contents of a packet is undefined: it may be a frame of video, a millisecond
/// of audio, or a subtitle, but a packet will never contain data from two different bitstreams.
/// Therefore the caller can be selective in what tracks(s) should be decoded and consumed.
///
/// `FormatReader` provides an Iterator-like interface over packets for easy consumption and
/// filtering. Seeking will invalidate the state of any `Decoder` processing packets from the
/// `FormatReader` and should be reset after a successful seek operation.
pub trait FormatReader: Send + Sync {
    /// Get basic information about the container format.
    fn format_info(&self) -> &FormatInfo;

    /// Get a list of all attachments.
    ///
    /// # For Implementations
    ///
    /// The default implementation returns an empty slice.
    fn attachments(&self) -> &[Attachment] {
        &[]
    }

    /// Get media chapters, if available.
    ///
    /// # For Implementations
    ///
    /// The default implementation returns `None`.
    fn chapters(&self) -> Option<&ChapterGroup> {
        None
    }

    /// Gets the metadata revision log.
    fn metadata(&mut self) -> Metadata<'_>;

    /// Seek, as precisely as possible depending on the mode, to the `Time` or track `TimeStamp`
    /// requested. Returns the requested and actual `TimeStamps` seeked to, as well as the `Track`.
    ///
    /// After a seek, all `Decoder`s consuming packets from this reader should be reset.
    ///
    /// Note: The `FormatReader` by itself cannot seek to an exact audio frame, it is only capable
    /// of seeking to the nearest `Packet`. Therefore, to seek to an exact frame, a `Decoder` must
    /// decode packets until the requested position is reached. When using the accurate `SeekMode`,
    /// the seeked position will always be at or before the requested position. If the coarse
    /// `SeekMode` is used, then the seek position may be after the requested position. Coarse
    /// seeking is an optional performance enhancement a reader may implement, therefore, a coarse
    /// seek may sometimes be an accurate seek.
    fn seek(&mut self, mode: SeekMode, to: SeekTo) -> Result<SeekedTo>;

    /// Gets a list of tracks in the container.
    fn tracks(&self) -> &[Track];

    /// Get the first track of a certain track type.
    fn first_track(&self, track_type: TrackType) -> Option<&Track> {
        // Find the first track matching the desired track type.
        self.tracks().iter().find(|track| matches_track_type(track, track_type))
    }

    /// Get the first track of a certain track type with a known (non-null) codec.
    fn first_track_known_codec(&self, track_type: TrackType) -> Option<&Track> {
        // Find the first track matching the desired track type with a known codec.
        self.tracks().iter().find(|track| match &track.codec_params {
            Some(CodecParameters::Audio(params)) if track_type == TrackType::Audio => {
                params.codec != audio::CODEC_ID_NULL_AUDIO
            }
            Some(CodecParameters::Video(params)) if track_type == TrackType::Video => {
                params.codec != video::CODEC_ID_NULL_VIDEO
            }
            Some(CodecParameters::Subtitle(params)) if track_type == TrackType::Subtitle => {
                params.codec != subtitle::CODEC_ID_NULL_SUBTITLE
            }
            _ => false,
        })
    }

    /// Get the default track of a certain track type.
    ///
    /// # For Implementations
    ///
    /// The default implementation of this function will return the first track of the desired track
    /// type with the default flag set, or if there is no track with the default flag set, the first
    /// track of the desired track type with a non-null codec ID. If no tracks are present then
    /// `None` is returned.
    ///
    /// Most format reader implementations should not override the default implementation and
    /// instead set the default track flag appropriately.
    fn default_track(&self, track_type: TrackType) -> Option<&Track> {
        // Find a track with the default flag set that matches the desired track type.
        self.tracks()
            .iter()
            .filter(|track| track.flags.contains(TrackFlags::DEFAULT))
            .find(|track| matches_track_type(track, track_type))
            .or_else(|| self.first_track_known_codec(track_type))
    }

    /// Reader the next packet from the container.
    ///
    /// If `Ok(None)` is returned, the media has ended and no more packets will be produced until
    /// the reader is seeked to a new position.
    ///
    /// If `Err(ResetRequired)` is returned, then the track list must be re-examined and all
    /// `Decoder`s re-created. All other errors are unrecoverable.
    fn next_packet(&mut self) -> Result<Option<Packet>>;

    /// Consumes the `FormatReader` and returns the underlying media source stream
    fn into_inner<'s>(self: Box<Self>) -> MediaSourceStream<'s>
    where
        Self: 's;
}

/// Returns true, if `track` is of the specific track type.
fn matches_track_type(track: &Track, track_type: TrackType) -> bool {
    match track.codec_params {
        Some(CodecParameters::Audio(_)) if track_type == TrackType::Audio => true,
        Some(CodecParameters::Video(_)) if track_type == TrackType::Video => true,
        Some(CodecParameters::Subtitle(_)) if track_type == TrackType::Subtitle => true,
        _ => false,
    }
}

pub mod util {
    //! Helper utilities for implementing `FormatReader`s.

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
                let i = self
                    .points
                    .partition_point(|p| ts > p.frame_ts && byte_offset >= p.byte_offset);

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
}

/// IDs for well-known container formats.
pub mod well_known {
    use super::FormatId;

    /// Waveform Audio File Format
    pub const FORMAT_ID_WAVE: FormatId = FormatId(0x100);
    /// Audio Interchange File Format
    pub const FORMAT_ID_AIFF: FormatId = FormatId(0x101);
    /// Audio Video Interleave
    pub const FORMAT_ID_AVI: FormatId = FormatId(0x102);
    /// Core Audio Format
    pub const FORMAT_ID_CAF: FormatId = FormatId(0x103);
    /// MPEG Audio Layer 1 Native
    pub const FORMAT_ID_MP1: FormatId = FormatId(0x104);
    /// MPEG Audio Layer 2 Native
    pub const FORMAT_ID_MP2: FormatId = FormatId(0x105);
    /// MPEG Audio Layer 3 Native
    pub const FORMAT_ID_MP3: FormatId = FormatId(0x106);
    /// Audio Data Transport Stream
    pub const FORMAT_ID_ADTS: FormatId = FormatId(0x107);
    /// Ogg
    pub const FORMAT_ID_OGG: FormatId = FormatId(0x108);
    /// Free Lossless Audio Codec Native
    pub const FORMAT_ID_FLAC: FormatId = FormatId(0x109);
    /// WavPack
    pub const FORMAT_ID_WAVPACK: FormatId = FormatId(0x10a);
    /// ISO Base Media File Format
    pub const FORMAT_ID_ISOMP4: FormatId = FormatId(0x10b);
    /// Matroska/WebM
    pub const FORMAT_ID_MKV: FormatId = FormatId(0x10c);
    /// Flash Video
    pub const FORMAT_ID_FLV: FormatId = FormatId(0x10d);
}
