// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::collections::HashMap;
use std::io::{Seek, SeekFrom};
use std::sync::Arc;

use symphonia_core::codecs::CodecParameters;
use symphonia_core::codecs::audio::AudioCodecParameters;
use symphonia_core::errors::{Error, decode_error, seek_error, unsupported_error};
use symphonia_core::errors::{Result, SeekErrorKind};
use symphonia_core::formats::prelude::*;
use symphonia_core::formats::probe::{ProbeFormatData, ProbeableFormat, Score, Scoreable};
use symphonia_core::formats::well_known::FORMAT_ID_AIFF;
use symphonia_core::io::*;
use symphonia_core::meta::well_known::METADATA_ID_AIFF;
use symphonia_core::meta::{
    Metadata, MetadataBuilder, MetadataInfo, MetadataLog, StandardTag, Tag,
};
use symphonia_core::support_format;

use log::debug;

use crate::common::{
    ByteOrder, ChunksReader, PacketInfo, append_data_params, append_format_params, next_packet,
};
mod chunks;
use chunks::*;

/// Aiff is actually a RIFF stream, with a "FORM" ASCII stream marker.
const AIFF_STREAM_MARKER: [u8; 4] = *b"FORM";
/// A possible RIFF form is "aiff".
const AIFF_RIFF_FORM: [u8; 4] = *b"AIFF";
/// A possible RIFF form is "aifc", using compressed data.
const AIFC_RIFF_FORM: [u8; 4] = *b"AIFC";

const AIFF_FORMAT_INFO: FormatInfo = FormatInfo {
    format: FORMAT_ID_AIFF,
    short_name: "aiff",
    long_name: "Audio Interchange File Format",
};

const AIFF_METADATA_INFO: MetadataInfo = MetadataInfo {
    metadata: METADATA_ID_AIFF,
    short_name: "aiff",
    long_name: "Audio Interchange File Format",
};

/// Audio Interchange File Format (AIFF) format reader.
///
/// `AiffReader` implements a demuxer for the AIFF container format.
pub struct AiffReader<'s> {
    reader: MediaSourceStream<'s>,
    media_info: MediaInfo,
    tracks: Vec<Track>,
    attachments: Vec<Attachment>,
    chapters: Option<ChapterGroup>,
    metadata: MetadataLog,
    packet_info: PacketInfo,
    data_start_pos: u64,
    data_end_pos: Option<u64>,
}

impl<'s> AiffReader<'s> {
    pub fn try_new(mut mss: MediaSourceStream<'s>, opts: FormatOptions) -> Result<Self> {
        // An AIFF file is one large RIFF chunk, with the actual meta and audio data contained in
        // nested chunks. Therefore, the file starts with a RIFF chunk header (chunk ID & size).

        // The top-level chunk has the FORM chunk ID. This is also the file marker.
        let marker = mss.read_quad_bytes()?;

        if marker != AIFF_STREAM_MARKER {
            return unsupported_error("aiff: missing aiff riff stream marker");
        }

        // The length of the top-level FORM chunk. Must be atleast 4 bytes.
        let riff_len = mss.read_be_u32()?;

        if riff_len < 4 {
            return decode_error("aiff: invalid riff length");
        }

        // The form type. Only AIFF and AIFC forms are supported.
        let riff_form = mss.read_quad_bytes()?;

        if riff_form != AIFF_RIFF_FORM && riff_form != AIFC_RIFF_FORM {
            return unsupported_error("aiff: riff form is not aiff or aifc");
        }

        let mut riff_chunks =
            ChunksReader::<RiffAiffChunks>::new(Some(riff_len - 4), ByteOrder::BigEndian);

        // Chunks can be read in any order, so collect them to be processed later.
        let mut comm = None;
        let mut ssnd = None;
        let mut mark = None;
        let mut comt = None;
        let mut id3 = None;

        let is_seekable = mss.is_seekable();

        let mut attachments = Vec::new();
        let mut builder = MetadataBuilder::new(AIFF_METADATA_INFO);

        // Scan over all chunks.
        while let Some(chunk) = riff_chunks.next(&mut mss)? {
            match chunk {
                RiffAiffChunks::Common(chunk) => {
                    // Only one common chunk is allowed.
                    if comm.is_some() {
                        return decode_error("aiff: multiple common chunks");
                    }

                    comm = match riff_form {
                        AIFF_RIFF_FORM => Some(chunk.parse_aiff(&mut mss)?),
                        AIFC_RIFF_FORM => Some(chunk.parse_aifc(&mut mss)?),
                        _ => unreachable!(),
                    };
                }
                RiffAiffChunks::Sound(chunk) => {
                    // Only one sound data chunk is allowed.
                    if ssnd.is_some() {
                        return decode_error("aiff: multiple sound data chunks");
                    }

                    ssnd = Some(chunk.parse(&mut mss)?);

                    // If the media source is not seekable, then it is not possible to scan for
                    // chunks past the sound data chunk.
                    if !is_seekable {
                        break;
                    }

                    // The length of the sound data chunk must also be known.
                    let ssnd_ref = ssnd.as_ref().expect("ssnd assigned a few lines above");
                    if let Some(len) = ssnd_ref.len {
                        mss.ignore_bytes(u64::from(len))?;
                    }
                    else {
                        break;
                    }
                }
                RiffAiffChunks::Marker(chunk) => {
                    // Only one markers chunk is allowed.
                    if mark.is_some() {
                        return decode_error("aiff: multiple markers chunks");
                    }

                    // Saver makers chunk for post-processing.
                    mark = Some(chunk.parse(&mut mss)?)
                }
                RiffAiffChunks::Comments(chunk) => {
                    // Only one comments chunk is allowed.
                    if comt.is_some() {
                        return decode_error("aiff: multiple comments chunks");
                    }

                    // Save comments chunk for post-processing.
                    comt = Some(chunk.parse_and_skip_unread(&mut mss)?);
                }
                RiffAiffChunks::AppSpecific(chunk) => {
                    // Add application-specific data.
                    let appl = chunk.parse_and_skip_unread(&mut mss)?;

                    attachments.push(Attachment::VendorData(VendorDataAttachment {
                        ident: appl.application,
                        data: appl.data,
                    }));
                }
                RiffAiffChunks::Text(chunk) => {
                    // Add tag.
                    let text = chunk.parse_and_skip_unread(&mut mss)?;
                    builder.add_tag(text.tag);
                }
                RiffAiffChunks::Id3(chunk) => id3 = Some(chunk.parse_and_skip_unread(&mut mss)?),
            }
        }

        // The common element is mandatory.
        let comm = comm.ok_or(Error::DecodeError("aiff: missing common element"))?;

        // If the sound data element is not present, then assume an empty one.
        let ssnd = ssnd.unwrap_or_else(|| SoundChunk::empty(mss.pos()));

        // Seek to the start of the sound data.
        if is_seekable {
            mss.seek(SeekFrom::Start(ssnd.data_start_pos))?;
        }

        // Metadata processing.
        let mut metadata = opts.external_data.metadata.unwrap_or_default();

        // Process markers and comments.
        let chapters = process_markers(&comm, mark, comt, &mut builder);

        // Add metadata generated from marker, comment, and text chunks.
        // TODO: Don't add if empty.
        metadata.push(builder.build());

        // Add ID3 metadata.
        if let Some(id3) = id3 {
            metadata.push(id3.metadata);
        }

        // The common chunk contains the block_align field and possible additional information
        // to handle packetization and seeking.
        let packet_info = comm.packet_info()?;

        let mut codec_params = AudioCodecParameters::new();
        codec_params
            .with_max_frames_per_packet(packet_info.max_frames_per_packet.get())
            .with_frames_per_block(packet_info.frames_per_block.get());

        // Append common chunk fields to codec parameters.
        append_format_params(&mut codec_params, comm.format_data, comm.sample_rate.get());

        // Create a new track using the collected codec parameters.
        let mut track = Track::new(0);
        track.with_codec_params(CodecParameters::Audio(codec_params));

        // Append sound data chunk fields to track.
        if let Some(data_len) = ssnd.len {
            append_data_params(&mut track, u64::from(data_len), &packet_info);
        }

        Ok(AiffReader {
            reader: mss,
            media_info: MediaInfo::from_track(&track),
            tracks: vec![track],
            attachments,
            chapters: chapters.or(opts.external_data.chapters),
            metadata,
            packet_info,
            data_start_pos: ssnd.data_start_pos,
            data_end_pos: ssnd.len.map(|data_len| ssnd.data_start_pos + u64::from(data_len)),
        })
    }
}

fn process_markers(
    comm: &CommonChunk,
    mark: Option<MarkerChunk>,
    comt: Option<CommentsChunk>,
    builder: &mut MetadataBuilder,
) -> Option<ChapterGroup> {
    let mut chapters = Vec::new();
    let mut marker_index = HashMap::new();

    // Process markers chunk.
    if let Some(mark) = mark {
        let tb = TimeBase::from_recip(comm.sample_rate);

        // Create a chapter for each marker.
        chapters.reserve(mark.markers.len());

        for marker in mark.markers {
            // Record the index of the chapter in the chapters vector for the given marker id.
            // Only non-zero positive marker IDs are valid. There should also only be one marker
            // per marker ID.
            if marker.id > 0 && !marker_index.contains_key(&marker.id) {
                marker_index.insert(marker.id, chapters.len());
            }

            // Add the chapter.
            // marker.ts is u32; the result is seconds as i64, so overflow is impossible.
            let start_time = tb
                .calc_time(Timestamp::from(marker.ts))
                .expect("aiff: chapter timestamp overflow is impossible");

            chapters.push(Chapter {
                start_time,
                end_time: None,
                start_byte: None,
                end_byte: None,
                tags: vec![Tag::new_from_parts("NAME", marker.name, None)],
                visuals: vec![],
            });
        }
    }

    // Process comments cjunk.
    if let Some(comt) = comt {
        for comment in comt.comments {
            let value = Arc::new(comment.text);

            let tag =
                Tag::new_from_parts("COMMMENT", value.clone(), Some(StandardTag::Comment(value)));

            if comment.marker_id == 0 {
                // Invalid/unset marker ID, this is a general comment.
                builder.add_tag(tag);
            }
            else if comment.marker_id > 0 {
                // Marker ID is set, this comment belongs to a marker/chapter. Try to get the
                // index of the chapter associated with this marker ID.
                if let Some(idx) = marker_index.get(&comment.marker_id) {
                    // Add tag to chapter.
                    chapters[*idx].tags.push(tag);
                }
            }
        }
    }

    if !chapters.is_empty() {
        Some(ChapterGroup {
            items: chapters.into_iter().map(ChapterGroupItem::Chapter).collect(),
            tags: vec![],
            visuals: vec![],
        })
    }
    else {
        None
    }
}

impl Scoreable for AiffReader<'_> {
    fn score(mut src: ScopedStream<&mut MediaSourceStream<'_>>) -> Result<Score> {
        // Perform simple scoring by testing that the RIFF stream marker and RIFF form are both
        // valid for AIFF.
        let riff_marker = src.read_quad_bytes()?;
        src.ignore_bytes(4)?;
        let riff_form = src.read_quad_bytes()?;

        if riff_marker != AIFF_STREAM_MARKER {
            return Ok(Score::Unsupported);
        }

        if riff_form != AIFF_RIFF_FORM && riff_form != AIFC_RIFF_FORM {
            return Ok(Score::Unsupported);
        }

        Ok(Score::Supported(255))
    }
}

impl ProbeableFormat<'_> for AiffReader<'_> {
    fn try_probe_new(
        mss: MediaSourceStream<'_>,
        opts: FormatOptions,
    ) -> Result<Box<dyn FormatReader + '_>> {
        Ok(Box::new(AiffReader::try_new(mss, opts)?))
    }

    fn probe_data() -> &'static [ProbeFormatData] {
        &[
            // AIFF RIFF form
            support_format!(
                AIFF_FORMAT_INFO,
                &["aiff", "aif", "aifc"],
                &["audio/aiff", "audio/x-aiff", " sound/aiff", "audio/x-pn-aiff"],
                &[b"FORM"]
            ),
        ]
    }
}

impl FormatReader for AiffReader<'_> {
    fn format_info(&self) -> &FormatInfo {
        &AIFF_FORMAT_INFO
    }

    fn media_info(&self) -> &MediaInfo {
        &self.media_info
    }

    fn next_packet(&mut self) -> Result<Option<Packet>> {
        next_packet(
            &mut self.reader,
            &self.packet_info,
            &self.tracks,
            self.data_start_pos,
            self.data_end_pos.unwrap_or(u64::MAX),
        )
    }

    fn metadata(&mut self) -> Metadata<'_> {
        self.metadata.metadata()
    }

    fn attachments(&self) -> &[Attachment] {
        &self.attachments
    }

    fn chapters(&self) -> Option<&ChapterGroup> {
        self.chapters.as_ref()
    }

    fn tracks(&self) -> &[Track] {
        &self.tracks
    }

    fn seek(&mut self, _mode: SeekMode, to: SeekTo) -> Result<SeekedTo> {
        if self.tracks.is_empty() {
            return seek_error(SeekErrorKind::Unseekable);
        }

        let track = &self.tracks[0];

        let required_ts = match to {
            // Frame timestamp given.
            SeekTo::Timestamp { ts, .. } => ts,
            // Time value given, calculate frame timestamp using the timebase.
            SeekTo::Time { time, .. } => {
                // The timebase is required to calculate the timestamp.
                let tb = track.time_base.ok_or(Error::SeekError(SeekErrorKind::Unseekable))?;

                // If the timestamp overflows, the seek if out-of-range.
                tb.calc_timestamp(time).ok_or(Error::SeekError(SeekErrorKind::OutOfRange))?
            }
        };

        // Negative timestamps are not allowed.
        if required_ts.is_negative() {
            return seek_error(SeekErrorKind::OutOfRange);
        }

        // If the total number of frames in the track is known, verify the desired frame timestamp
        // does not exceed it.
        if let Some(num_frames) = track.num_frames {
            if required_ts.get() as u64 > num_frames {
                return seek_error(SeekErrorKind::OutOfRange);
            }
        }

        debug!("seeking to frame_ts={required_ts}");

        // RIFF is not internally packetized for PCM codecs. Packetization is simulated by trying to
        // read a constant number of samples or blocks every call to next_packet. Therefore, a
        // packet begins wherever the data stream is currently positioned. Since timestamps on
        // packets should be determinstic, instead of seeking to the exact timestamp requested and
        // starting the next packet there, seek to a packet boundary. In this way, packets will have
        // have the same timestamps regardless if the stream was seeked or not.
        let actual_ts = self.packet_info.get_actual_ts(required_ts);

        // Calculate the absolute byte offset of the desired audio frame.
        let seek_pos =
            self.data_start_pos + (actual_ts.get() as u64 * self.packet_info.block_size.get());

        // If the reader supports seeking we can seek directly to the frame's offset wherever it may
        // be.
        if self.reader.is_seekable() {
            self.reader.seek(SeekFrom::Start(seek_pos))?;
        }
        // If the reader does not support seeking, we can only emulate forward seeks by consuming
        // bytes. If the reader has to seek backwards, return an error.
        else {
            let current_pos = self.reader.pos();
            if seek_pos >= current_pos {
                self.reader.ignore_bytes(seek_pos - current_pos)?;
            }
            else {
                return seek_error(SeekErrorKind::ForwardOnly);
            }
        }

        debug!(
            "seeked to packet_ts={} (delta={})",
            actual_ts,
            actual_ts.saturating_delta(required_ts)
        );

        Ok(SeekedTo { track_id: 0, actual_ts, required_ts })
    }

    fn into_inner<'s>(self: Box<Self>) -> MediaSourceStream<'s>
    where
        Self: 's,
    {
        self.reader
    }
}
