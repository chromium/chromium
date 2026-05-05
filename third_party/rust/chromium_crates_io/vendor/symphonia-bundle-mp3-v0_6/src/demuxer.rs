// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use symphonia_core::support_format;

use symphonia_core::checksum::Crc16AnsiLe;
use symphonia_core::codecs::CodecParameters;
use symphonia_core::codecs::audio::AudioCodecParameters;
use symphonia_core::codecs::audio::well_known::{CODEC_ID_MP1, CODEC_ID_MP2, CODEC_ID_MP3};
use symphonia_core::errors::{Error, Result, SeekErrorKind, seek_error};
use symphonia_core::formats::prelude::*;
use symphonia_core::formats::probe::{ProbeFormatData, ProbeableFormat, Score, Scoreable};
use symphonia_core::formats::well_known::{FORMAT_ID_MP1, FORMAT_ID_MP2, FORMAT_ID_MP3};
use symphonia_core::io::*;
use symphonia_core::meta::{Metadata, MetadataLog};

use crate::common::{FrameHeader, MpegLayer};
use crate::header::{self, MAX_MPEG_FRAME_SIZE, MPEG_HEADER_LEN};

use std::io::{Seek, SeekFrom};

use log::{debug, info, warn};

const MP1_FORMAT_INFO: FormatInfo =
    FormatInfo { format: FORMAT_ID_MP1, short_name: "mp1", long_name: "MPEG Audio Layer 1 Native" };

const MP2_FORMAT_INFO: FormatInfo =
    FormatInfo { format: FORMAT_ID_MP2, short_name: "mp2", long_name: "MPEG Audio Layer 2 Native" };

const MP3_FORMAT_INFO: FormatInfo =
    FormatInfo { format: FORMAT_ID_MP3, short_name: "mp3", long_name: "MPEG Audio Layer 3 Native" };

/// MPEG1 and MPEG2 audio elementary stream reader.
///
/// `MpaReader` implements a demuxer for the MPEG1 and MPEG2 audio elementary stream.
pub struct MpaReader<'s> {
    reader: MediaSourceStream<'s>,
    tracks: Vec<Track>,
    chapters: Option<ChapterGroup>,
    metadata: MetadataLog,
    first_packet_pos: u64,
    next_packet_ts: Timestamp,
}

impl Scoreable for MpaReader<'_> {
    fn score(mut src: ScopedStream<&mut MediaSourceStream<'_>>) -> Result<Score> {
        // Read the sync word for the first (assumed) MPEG frame and try to parse it into a header.
        let sync1 = header::read_frame_header_word_no_sync(&mut src)?;
        let hdr1 = header::parse_frame_header(sync1)?;

        // Since the first header was parsed successfully, this may be a MPEG audio format. However,
        // if there is enough data left to read the frame body and another frame header, then a
        // higher confidence may be gained. If there is not enough data left, return a partially
        // confident score.
        if src.bytes_available() < (hdr1.frame_size + header::MPEG_HEADER_LEN) as u64 {
            return Ok(Score::Supported(127));
        }

        // Skip the frame body.
        src.ignore_bytes(hdr1.frame_size as u64)?;

        // Read another sync word for the second (assumed) MPEG frame.
        let sync2 = header::read_frame_header_word_no_sync(&mut src)?;

        // The second sync word should look like a sync word.
        if !header::is_frame_header_word_synced(sync2) {
            return Ok(Score::Unsupported);
        }

        // Try to parse the second sync word into a header.
        let _ = header::parse_frame_header(sync2)?;

        Ok(Score::Supported(255))
    }
}

impl ProbeableFormat<'_> for MpaReader<'_> {
    fn try_probe_new(
        mss: MediaSourceStream<'_>,
        opts: FormatOptions,
    ) -> Result<Box<dyn FormatReader + '_>> {
        Ok(Box::new(MpaReader::try_new(mss, opts)?))
    }

    fn probe_data() -> &'static [ProbeFormatData] {
        &[
            // Layer 1
            support_format!(
                MP1_FORMAT_INFO,
                &["mp1"],
                &["audio/mpeg", "audio/mp1"],
                &[
                    &[0xff, 0xfe], // MPEG 1 with CRC
                    &[0xff, 0xff], // MPEG 1
                    &[0xff, 0xf6], // MPEG 2 with CRC
                    &[0xff, 0xf7], // MPEG 2
                    &[0xff, 0xe6], // MPEG 2.5 with CRC
                    &[0xff, 0xe7], // MPEG 2.5
                ]
            ),
            // Layer 2
            support_format!(
                MP2_FORMAT_INFO,
                &["mp2"],
                &["audio/mpeg", "audio/mp2"],
                &[
                    &[0xff, 0xfc], // MPEG 1 with CRC
                    &[0xff, 0xfd], // MPEG 1
                    &[0xff, 0xf4], // MPEG 2 with CRC
                    &[0xff, 0xf5], // MPEG 2
                    &[0xff, 0xe4], // MPEG 2.5 with CRC
                    &[0xff, 0xe5], // MPEG 2.5
                ]
            ),
            // Layer 3
            support_format!(
                MP3_FORMAT_INFO,
                &["mp3"],
                &["audio/mpeg", "audio/mp3"],
                &[
                    &[0xff, 0xfa], // MPEG 1 with CRC
                    &[0xff, 0xfb], // MPEG 1
                    &[0xff, 0xf2], // MPEG 2 with CRC
                    &[0xff, 0xf3], // MPEG 2
                    &[0xff, 0xe2], // MPEG 2.5 with CRC
                    &[0xff, 0xe3], // MPEG 2.5
                ]
            ),
        ]
    }
}

impl FormatReader for MpaReader<'_> {
    fn format_info(&self) -> &FormatInfo {
        // Safety: MpaReader only supports/has audio tracks.
        match self.tracks[0].codec_params.as_ref().unwrap().audio().unwrap().codec {
            CODEC_ID_MP1 => &MP1_FORMAT_INFO,
            CODEC_ID_MP2 => &MP2_FORMAT_INFO,
            CODEC_ID_MP3 => &MP3_FORMAT_INFO,
            _ => unreachable!(),
        }
    }

    fn next_packet(&mut self) -> Result<Option<Packet>> {
        let (header, data) = loop {
            // Read the next MPEG frame.
            let (header, data) = match read_mpeg_frame(&mut self.reader) {
                Ok(frame) => frame,
                Err(Error::IoError(err)) if err.kind() == std::io::ErrorKind::UnexpectedEof => {
                    // MPEG streams have no well-defined end, so when no more frames can be read,
                    // consider the stream ended.
                    return Ok(None);
                }
                Err(err) => return Err(err),
            };

            // Check if the packet contains a Xing, Info, or VBRI tag.
            if is_maybe_info_tag(&data, &header) {
                if try_read_info_tag(&data, &header).is_some() {
                    // Discard the packet and tag since it was not at the start of the stream.
                    warn!("found an unexpected xing tag, discarding");
                    continue;
                }
            }
            else if is_maybe_vbri_tag(&data, &header)
                && try_read_vbri_tag(&data, &header).is_some()
            {
                // Discard the packet and tag since it was not at the start of the stream.
                warn!("found an unexpected vbri tag, discarding");
                continue;
            }

            break (header, data);
        };

        // The timestamp and duration for this packet.
        let pts = self.next_packet_ts;
        let dur = header.duration();

        // Advance the next packet timestamp based on this packet's duration. If it saturates, then
        // it is not possible to read further.
        self.next_packet_ts = match self.next_packet_ts.checked_add(dur) {
            Some(ts) => ts,
            None => return Ok(None),
        };

        // Build the packet.
        let packet = PacketBuilder::new()
            .track_id(0)
            .pts(pts)
            .trimmed_dur(
                dur,
                self.tracks[0]
                    .num_frames
                    .map(Duration::from)
                    .and_then(|dur| dur.timestamp_from(Timestamp::ZERO)),
            )
            .data(data)
            .build();

        Ok(Some(packet))
    }

    fn metadata(&mut self) -> Metadata<'_> {
        self.metadata.metadata()
    }

    fn chapters(&self) -> Option<&ChapterGroup> {
        self.chapters.as_ref()
    }

    fn tracks(&self) -> &[Track] {
        &self.tracks
    }

    fn seek(&mut self, mode: SeekMode, to: SeekTo) -> Result<SeekedTo> {
        const MAX_REF_FRAMES: usize = 4;
        const REF_FRAMES_MASK: usize = MAX_REF_FRAMES - 1;

        // Get the timestamp of the desired audio frame.
        let required_ts = match to {
            // Frame timestamp given.
            SeekTo::TimeStamp { ts, .. } => ts,
            // Time value given, calculate frame timestamp using the timebase.
            SeekTo::Time { time, .. } => {
                // The timebase is required to calculate the timestamp.
                let tb =
                    self.tracks[0].time_base.ok_or(Error::SeekError(SeekErrorKind::Unseekable))?;

                // If the timestamp overflows, the seek if out-of-range.
                tb.calc_timestamp(time).ok_or(Error::SeekError(SeekErrorKind::OutOfRange))?
            }
        };

        // Get the total duration of the track (excludes delay and padding).
        let dur_ts = self.tracks[0].num_frames.map(Duration::from);

        // Get the delay and padding values (or 0 if unknown).
        let delay = self.tracks[0].delay.unwrap_or(0);
        let padding = self.tracks[0].padding.unwrap_or(0);

        // Calculate the valid timestamp bounds of the track.
        let min_ts = Timestamp::from(-i64::from(delay));
        let max_ts = dur_ts
            .and_then(|dur| min_ts.checked_add(dur))
            .and_then(|dur| dur.checked_add(Duration::from(delay + padding)));

        // Ensure the seek position is within the bounds of the track.
        if required_ts < min_ts {
            return seek_error(SeekErrorKind::OutOfRange);
        }
        else if let Some(max_ts) = max_ts {
            if required_ts > max_ts {
                return seek_error(SeekErrorKind::OutOfRange);
            }
        }

        let is_seekable = self.reader.is_seekable();

        // If the stream is unseekable and the required timestamp in the past, then return an
        // error, it is not possible to seek to it.
        if !is_seekable && required_ts < self.next_packet_ts {
            return seek_error(SeekErrorKind::ForwardOnly);
        }

        debug!("seeking to ts={required_ts}");

        // Step 1
        //
        // In coarse seek mode, the underlying media source stream will be roughly seeked based on
        // the required timestamp and the total duration of the media. Coarse seek mode requires a
        // seekable stream because the total length in bytes of the stream is required.
        //
        // In accurate seek mode, the underlying media source stream will not be seeked unless the
        // required timestamp is in the past, in which case the stream is seeked back to the start.
        match mode {
            SeekMode::Coarse if is_seekable => self.preseek_coarse(required_ts, min_ts, max_ts)?,
            SeekMode::Accurate => self.preseek_accurate(required_ts, min_ts)?,
            _ => (),
        };

        // Step 2
        //
        // Following the pre-seek operation above, parse MPEG frames (packets) one-by-one from the
        // current position in the stream until the frame containing the desired timestamp is
        // reached. For coarse seeks, this should only parse a few packets. For accurate seeks, the
        // entire stream could potentially be parsed.
        let mut frames: [FramePos; MAX_REF_FRAMES] = Default::default();
        let mut n_parsed = 0;

        loop {
            // Sync to the next frame.
            let sync = match header::sync_frame(&mut self.reader) {
                Ok(sync) => sync,
                Err(Error::IoError(err)) if err.kind() == std::io::ErrorKind::UnexpectedEof => {
                    // MPEG streams have no well-defined end, so if no more frames can be read then
                    // assume the seek position is out-of-range. This would normally only happen if
                    // the duration of the track is unknown, or it is was longer than the track
                    // actually is.
                    return seek_error(SeekErrorKind::OutOfRange);
                }
                Err(err) => return Err(err),
            };

            // Parse the synced frame header.
            let header = header::parse_frame_header(sync)?;

            // Position of the frame header.
            let pos = self.reader.pos() - header::MPEG_HEADER_LEN as u64;

            // Calculate the duration of the frame.
            let frame_dur = header.duration();

            // Add the frame to the frame ring.
            frames[n_parsed & REF_FRAMES_MASK] = FramePos { pos, ts: self.next_packet_ts };
            n_parsed += 1;

            let next_packet_ts = match self.next_packet_ts.checked_add(frame_dur) {
                Some(ts) if ts <= required_ts => ts,
                // The timestamp of the next MPEG frame exceeds the desired timestamp, or it
                // exceeds the representable range. Rewind back to the start of the current frame
                // and end the search.
                _ => {
                    // The main_data_begin offset is a negative offset from the frame's header to
                    // where its main data begins. Therefore, for a decoder to properly decode this
                    // frame, the reader must provide previous (reference) frames up-to and
                    // including the frame that contains the first byte this frame's main_data.
                    let main_data_begin = read_main_data_begin(&mut self.reader, &header)? as u64;

                    debug!(
                        "found frame with ts={} @ pos={} with main_data_begin={}",
                        self.next_packet_ts, pos, main_data_begin
                    );

                    // The number of reference frames is 0 if main_data_begin is also 0. Otherwise,
                    // attempt to find the first (oldest) reference frame, then select 1 frame
                    // before that one to actually seek to.
                    let mut n_ref_frames = 0;
                    let mut ref_frame = &frames[(n_parsed - 1) & REF_FRAMES_MASK];

                    if main_data_begin > 0 {
                        // The maximum number of reference frames is limited to the number of frames
                        // read and the number of previous frames recorded.
                        let max_ref_frames = std::cmp::min(n_parsed, frames.len());

                        while n_ref_frames < max_ref_frames {
                            ref_frame = &frames[(n_parsed - n_ref_frames - 1) & REF_FRAMES_MASK];

                            if pos - ref_frame.pos >= main_data_begin {
                                break;
                            }

                            n_ref_frames += 1;
                        }

                        debug!(
                            "will seek -{} frame(s) to ts={} @ pos={} (-{} bytes)",
                            n_ref_frames,
                            ref_frame.ts,
                            ref_frame.pos,
                            pos - ref_frame.pos
                        );
                    }

                    // Do the actual seek to the reference frame.
                    self.reader.seek_buffered(ref_frame.pos);

                    self.next_packet_ts = ref_frame.ts;
                    break;
                }
            };

            // Otherwise, ignore the frame body.
            self.reader.ignore_bytes(header.frame_size as u64)?;

            // Increment the timestamp for the next packet.
            self.next_packet_ts = next_packet_ts;
        }

        debug!(
            "seeked to ts={} (delta={})",
            self.next_packet_ts,
            self.next_packet_ts.saturating_delta(required_ts),
        );

        Ok(SeekedTo { track_id: 0, required_ts, actual_ts: self.next_packet_ts })
    }

    fn into_inner<'s>(self: Box<Self>) -> MediaSourceStream<'s>
    where
        Self: 's,
    {
        self.reader
    }
}

impl<'s> MpaReader<'s> {
    pub fn try_new(mut mss: MediaSourceStream<'s>, opts: FormatOptions) -> Result<Self> {
        // Try to read the first MPEG frame.
        let (header, packet) = read_mpeg_frame_strict(&mut mss)?;

        // Use the header to populate the codec parameters.
        let mut codec_params = AudioCodecParameters::new();

        codec_params
            .for_codec(header.codec())
            .with_sample_rate(header.sample_rate)
            .with_channels(header.channel_mode.channels());

        // Create the track.
        let mut track = Track::new(0);

        track.with_codec_params(CodecParameters::Audio(codec_params));

        // Check if there is a Xing/Info tag contained in the first frame.
        if let Some(info_tag) = try_read_info_tag(&packet, &header) {
            // The LAME tag contains ReplayGain and padding information.
            if let Some(lame_tag) = info_tag.lame {
                track.with_delay(lame_tag.enc_delay).with_padding(lame_tag.enc_padding);
            }

            // The base Xing/Info tag may contain the number of MPEG frames.
            if let Some(num_mpeg_frames) = info_tag.num_frames {
                info!("using xing header for duration");

                // The total number of audio frames including delay and padding frames.
                let num_frames = u64::from(num_mpeg_frames) * u64::from(header.num_frames());

                // Remove delay and padding frames.
                let discard = track.delay.unwrap_or(0) + track.padding.unwrap_or(0);

                track.with_num_frames(num_frames.saturating_sub(u64::from(discard)));
            }
        }
        else if let Some(vbri_tag) = try_read_vbri_tag(&packet, &header) {
            info!("using vbri header for duration");

            let num_frames = u64::from(vbri_tag.num_mpeg_frames) * u64::from(header.num_frames());

            // Check if there is a VBRI tag.
            track.with_num_frames(num_frames);
        }
        else {
            // The first frame was not a Xing/Info header, rewind back to the start of the frame so
            // that it may be decoded.
            mss.seek_buffered_rev(MPEG_HEADER_LEN + header.frame_size);

            // Likely not a VBR file, so estimate the duration if seekable.
            if mss.is_seekable() {
                info!("estimating duration from bitrate, may be inaccurate for vbr files");

                if let Some(n_mpeg_frames) = estimate_num_mpeg_frames(&mut mss) {
                    track.with_num_frames(n_mpeg_frames * u64::from(header.num_frames()));
                }
            }
        }

        let first_packet_pos = mss.pos();
        let next_packet_ts = Timestamp::from(-i64::from(track.delay.unwrap_or(0)));

        Ok(MpaReader {
            reader: mss,
            tracks: vec![track],
            chapters: opts.external_data.chapters,
            metadata: opts.external_data.metadata.unwrap_or_default(),
            first_packet_pos,
            next_packet_ts,
        })
    }

    /// Seeks the media source stream to a byte position roughly where the packet with the required
    /// timestamp should be located.
    fn preseek_coarse(
        &mut self,
        required_ts: Timestamp,
        min_ts: Timestamp,
        max_ts: Option<Timestamp>,
    ) -> Result<()> {
        // Get the length in bytes of the stream's audio data. It is not possible to coarsely seek
        // without knowing this.
        let audio_byte_len = match self.reader.byte_len() {
            Some(byte_len) => u128::from(byte_len - self.first_packet_pos),
            None => return seek_error(SeekErrorKind::Unseekable),
        };

        // It is not possible to coarse seek without knowing both the upper and lower timestamp
        // bounds, and the lower timestamp bound must be <= the upper timestamp bound.
        let max_ts = match max_ts {
            Some(max_ts) if max_ts >= min_ts => max_ts,
            _ => return seek_error(SeekErrorKind::Unseekable),
        };

        // Call invariants.
        debug_assert!(min_ts <= required_ts);
        debug_assert!(required_ts <= max_ts);

        // The total duration of the track.
        let total_dur = u128::from(max_ts.abs_delta(min_ts).get());

        // Calculate, roughly, the byte position of where the MPEG frame containing the required
        // timestamp should be in the media source stream relative to the start of the audio data.
        let seek_pos_rel =
            (u128::from(required_ts.abs_delta(min_ts).get()) * audio_byte_len) / total_dur;

        // It is preferable to return a packet with a timestamp before the requested timestamp.
        // Therefore, subtract the maximum packet size from the position found above to ensure this.
        let seek_pos = (seek_pos_rel + u128::from(self.first_packet_pos))
            .saturating_sub(MAX_MPEG_FRAME_SIZE as u128);

        // Seek the media source stream.
        self.reader.seek(SeekFrom::Start(
            seek_pos.try_into().map_err(|_| Error::SeekError(SeekErrorKind::OutOfRange))?,
        ))?;

        // Resync to the start of the next MPEG frame.
        let (header, _) = read_mpeg_frame_strict(&mut self.reader)?;

        // The byte position of the next MPEG frame relative to the start of the audio data.
        let audio_byte_pos = u128::from(self.reader.pos() - self.first_packet_pos);

        // Calculate, roughly, the duration from the start of the audio of the packet based on the
        // byte position after resync.
        let dur_to_pkt = Duration::from(((audio_byte_pos * total_dur) / audio_byte_len) as u64);

        // Assume the duration of a packet remains constant throughout the stream (not a
        // guarantee, but usually the case).
        let pkt_dur = header.duration();

        // Compute the timestamp of the next packet. First align the estimated duration from the
        // start of the audio to a packet boundary. Then add to the lower-bound timestamp. In
        // theory, this should never exceed the upper-bound timestamp.
        //
        // UNWRAP: The packet duration is always non-zero, and no larger than 1152.
        self.next_packet_ts =
            min_ts.checked_add(dur_to_pkt.align_down(pkt_dur).unwrap()).unwrap_or(max_ts);

        Ok(())
    }

    /// Seeks the media source stream back to the start of the first packet if the required
    /// timestamp is in the past.
    fn preseek_accurate(&mut self, required_ts: Timestamp, min_ts: Timestamp) -> Result<()> {
        if required_ts < self.next_packet_ts {
            let seeked_pos = self.reader.seek(SeekFrom::Start(self.first_packet_pos))?;

            // Since the elementary stream has no timestamp information, the position seeked
            // to must be exactly as requested.
            if seeked_pos != self.first_packet_pos {
                return seek_error(SeekErrorKind::Unseekable);
            }

            // Successfuly seeked to the start of the stream, reset the next packet timestamp.
            self.next_packet_ts = min_ts;
        }

        Ok(())
    }
}

/// Reads a MPEG frame and returns the header and buffer.
fn read_mpeg_frame(reader: &mut MediaSourceStream<'_>) -> Result<(FrameHeader, Vec<u8>)> {
    let (header, header_word) = loop {
        // Sync to the next frame header.
        let sync = header::sync_frame(reader)?;

        // Parse the frame header fully.
        if let Ok(header) = header::parse_frame_header(sync) {
            break (header, sync);
        }

        warn!("invalid mpeg audio header");
    };

    // Allocate frame buffer.
    let mut packet = vec![0u8; MPEG_HEADER_LEN + header.frame_size];
    packet[0..MPEG_HEADER_LEN].copy_from_slice(&header_word.to_be_bytes());

    // Read the frame body.
    reader.read_buf_exact(&mut packet[MPEG_HEADER_LEN..])?;

    // Return the parsed header and packet body.
    Ok((header, packet))
}

/// Reads a MPEG frame and checks if the next frame begins after the packet.
fn read_mpeg_frame_strict(reader: &mut MediaSourceStream<'_>) -> Result<(FrameHeader, Vec<u8>)> {
    loop {
        // Read the next MPEG frame.
        let (header, packet) = read_mpeg_frame(reader)?;

        // Get the position before trying to read the next header.
        let pos = reader.pos();

        // Read a sync word from the stream. If this read fails then the file may have ended and
        // this check cannot be performed.
        if let Ok(sync) = header::read_frame_header_word_no_sync(reader) {
            // If the stream is not synced to the next frame's sync word, or the next frame header
            // is not parseable or similar to the current frame header, then reject the current
            // packet since the stream likely synced to random data.
            if !header::is_frame_header_word_synced(sync) || !is_frame_header_similar(&header, sync)
            {
                warn!("skipping junk at {} bytes", pos - packet.len() as u64);

                // Seek back to the second byte of the rejected packet to prevent syncing to the
                // same spot again.
                reader.seek_buffered_rev(packet.len() + MPEG_HEADER_LEN - 1);
                continue;
            }
        }

        // Jump back to the position before the next header was read.
        reader.seek_buffered(pos);

        break Ok((header, packet));
    }
}

/// Check if a sync word parses to a frame header that is similar to the one provided.
fn is_frame_header_similar(header: &FrameHeader, sync: u32) -> bool {
    if let Ok(candidate) = header::parse_frame_header(sync) {
        if header.version == candidate.version
            && header.layer == candidate.layer
            && header.sample_rate == candidate.sample_rate
            && header.n_channels() == candidate.n_channels()
        {
            return true;
        }
    }

    false
}

#[derive(Default)]
struct FramePos {
    ts: Timestamp,
    pos: u64,
}

/// Reads the main_data_begin field from the side information of a MPEG audio frame.
fn read_main_data_begin<B: ReadBytes>(reader: &mut B, header: &FrameHeader) -> Result<u16> {
    // After the head the optional CRC is present.
    if header.has_crc {
        let _crc = reader.read_be_u16()?;
    }

    // For MPEG version 1 the first 9 bits is main_data_begin.
    let main_data_begin = if header.is_mpeg1() {
        reader.read_be_u16()? >> 7
    }
    // For MPEG version 2 the first 8 bits is main_data_begin.
    else {
        u16::from(reader.read_u8()?)
    };

    Ok(main_data_begin)
}

/// Estimates the total number of MPEG frames in the media source stream.
fn estimate_num_mpeg_frames(reader: &mut MediaSourceStream<'_>) -> Option<u64> {
    const MAX_FRAMES: u32 = 16;
    const MAX_LEN: usize = 16 * 1024;

    // Macro to convert a Result to Option, and break a loop on exit.
    macro_rules! break_on_err {
        ($expr:expr) => {
            match $expr {
                Ok(a) => a,
                _ => break None,
            }
        };
    }

    let start_pos = reader.pos();

    let mut total_frame_len = 0;
    let mut total_frames = 0;

    let total_len = match reader.byte_len() {
        Some(len) => len - start_pos,
        _ => return None,
    };

    let num_mpeg_frames = loop {
        // Read the frame header.
        let header_val = break_on_err!(reader.read_be_u32());

        // Parse the frame header.
        let header = break_on_err!(header::parse_frame_header(header_val));

        // Tabulate the size.
        total_frame_len += MPEG_HEADER_LEN + header.frame_size;
        total_frames += 1;

        // Ignore the frame body.
        break_on_err!(reader.ignore_bytes(header.frame_size as u64));

        // Read up-to 16 frames, or 16kB, then calculate the average MPEG frame length, and from
        // that, the total number of MPEG frames.
        if total_frames > MAX_FRAMES || total_frame_len > MAX_LEN {
            let avg_mpeg_frame_len = total_frame_len as f64 / total_frames as f64;
            break Some((total_len as f64 / avg_mpeg_frame_len) as u64);
        }
    };

    // Rewind back to the first frame seen upon entering this function.
    reader.seek_buffered_rev((reader.pos() - start_pos) as usize);

    num_mpeg_frames
}

const XING_TAG_ID: [u8; 4] = *b"Xing";
const INFO_TAG_ID: [u8; 4] = *b"Info";

/// The LAME tag is an extension to the Xing/Info tag.
#[allow(dead_code)]
struct LameTag {
    encoder: String,
    replaygain_peak: Option<f32>,
    replaygain_radio: Option<f32>,
    replaygain_audiophile: Option<f32>,
    enc_delay: u32,
    enc_padding: u32,
}

/// The Xing/Info time additional information for regarding a MP3 file.
#[allow(dead_code)]
struct XingInfoTag {
    num_frames: Option<u32>,
    num_bytes: Option<u32>,
    toc: Option<[u8; 100]>,
    quality: Option<u32>,
    is_cbr: bool,
    lame: Option<LameTag>,
}

/// Try to read a Xing/Info tag from the provided MPEG frame.
fn try_read_info_tag(buf: &[u8], header: &FrameHeader) -> Option<XingInfoTag> {
    // The Info header is a completely optional piece of information. Therefore, flatten an error
    // reading the tag into a None.
    try_read_info_tag_inner(buf, header).ok().flatten()
}

fn try_read_info_tag_inner(buf: &[u8], header: &FrameHeader) -> Result<Option<XingInfoTag>> {
    // Do a quick check that this is a Xing/Info tag.
    if !is_maybe_info_tag(buf, header) {
        return Ok(None);
    }

    // The position of the Xing/Info tag relative to the end of the header. This is equal to the
    // side information length for the frame.
    let offset = header.side_info_len();

    // Start the CRC with the header and side information.
    let mut crc16 = Crc16AnsiLe::new(0);
    crc16.process_buf_bytes(&buf[..offset + MPEG_HEADER_LEN]);

    // Start reading the Xing/Info tag after the side information.
    let mut reader = MonitorStream::new(BufReader::new(&buf[offset + MPEG_HEADER_LEN..]), crc16);

    // Check for Xing/Info header.
    let id = reader.read_quad_bytes()?;

    if id != XING_TAG_ID && id != INFO_TAG_ID {
        return Ok(None);
    }

    // The "Info" id is used for CBR files.
    let is_cbr = id == INFO_TAG_ID;

    // Flags indicates what information is provided in this Xing/Info tag.
    let flags = reader.read_be_u32()?;

    let num_frames = if flags & 0x1 != 0 { Some(reader.read_be_u32()?) } else { None };

    let num_bytes = if flags & 0x2 != 0 { Some(reader.read_be_u32()?) } else { None };

    let toc = if flags & 0x4 != 0 {
        let mut toc = [0; 100];
        reader.read_buf_exact(&mut toc)?;
        Some(toc)
    }
    else {
        None
    };

    let quality = if flags & 0x8 != 0 { Some(reader.read_be_u32()?) } else { None };

    /// The full LAME extension size.
    const LAME_EXT_LEN: u64 = 36;
    /// The minimal LAME extension size up-to the encode delay & padding fields.
    const MIN_LAME_EXT_LEN: u64 = 24;

    // The LAME extension may not always be present, or complete. The important fields in the
    // extension are within the first 24 bytes. Therefore, try to read those if they're available.
    let lame = if reader.inner().bytes_available() >= MIN_LAME_EXT_LEN {
        // Encoder string.
        let mut encoder = [0; 9];
        reader.read_buf_exact(&mut encoder)?;

        // Revision.
        let _revision = reader.read_u8()?;

        // Lowpass filter value.
        let _lowpass = reader.read_u8()?;

        // Replay gain peak in 9.23 (bit) fixed-point format.
        let replaygain_peak = match reader.read_be_u32()? {
            0 => None,
            peak => Some(32767.0 * (peak as f32 / 2.0f32.powi(23))),
        };

        // Radio replay gain.
        let replaygain_radio = parse_lame_tag_replaygain(reader.read_be_u16()?, 1);

        // Audiophile replay gain.
        let replaygain_audiophile = parse_lame_tag_replaygain(reader.read_be_u16()?, 2);

        // Encoding flags & ATH type.
        let _encoding_flags = reader.read_u8()?;

        // Arbitrary bitrate.
        let _abr = reader.read_u8()?;

        let (enc_delay, enc_padding) = {
            let trim = reader.read_be_u24()?;

            if encoder[..4] == *b"LAME" || encoder[..4] == *b"Lavf" || encoder[..4] == *b"Lavc" {
                let delay = 528 + 1 + (trim >> 12);
                let padding = trim & ((1 << 12) - 1);

                (delay, padding.saturating_sub(528 + 1))
            }
            else {
                (0, 0)
            }
        };

        // If possible, attempt to read the extra fields of the extension if they weren't
        // truncated.
        let crc = if reader.inner().bytes_available() >= LAME_EXT_LEN - MIN_LAME_EXT_LEN {
            // Flags.
            let _misc = reader.read_u8()?;

            // MP3 gain.
            let _mp3_gain = reader.read_u8()?;

            // Preset and surround info.
            let _surround_info = reader.read_be_u16()?;

            // Music length.
            let _music_len = reader.read_be_u32()?;

            // Music (audio) CRC.
            let _music_crc = reader.read_be_u16()?;

            // The tag CRC. LAME always includes this CRC regardless of the protection bit, but
            // other encoders may only do so if the protection bit is set.
            if header.has_crc || encoder[..4] == *b"LAME" {
                // Read the CRC using the inner reader to not change the computed CRC.
                Some(reader.inner_mut().read_be_u16()?)
            }
            else {
                // No CRC is present.
                None
            }
        }
        else {
            // The tag is truncated. No CRC will be present.
            info!("xing tag lame extension is truncated");
            None
        };

        // If there is no CRC, then assume the tag is correct. Otherwise, use the CRC.
        let is_tag_ok = crc.is_none_or(|crc| crc == reader.monitor().crc());

        if is_tag_ok {
            // The CRC matched or is not present.
            Some(LameTag {
                encoder: String::from_utf8_lossy(&encoder).into(),
                replaygain_peak,
                replaygain_radio,
                replaygain_audiophile,
                enc_delay,
                enc_padding,
            })
        }
        else {
            // The CRC did not match, this is probably not a LAME tag.
            warn!("xing tag lame extension crc mismatch");
            None
        }
    }
    else {
        // Frame not large enough for a LAME tag.
        info!("xing tag too small for lame extension");
        None
    };

    Ok(Some(XingInfoTag { num_frames, num_bytes, toc, quality, is_cbr, lame }))
}

fn parse_lame_tag_replaygain(value: u16, expected_name: u8) -> Option<f32> {
    // The 3 most-significant bits are the name code.
    let name = ((value & 0xe000) >> 13) as u8;

    if name == expected_name {
        let gain = (value & 0x01ff) as f32 / 10.0;
        Some(if value & 0x200 != 0 { -gain } else { gain })
    }
    else {
        None
    }
}

/// Perform a fast check to see if the packet contains a Xing/Info tag. If this returns true, the
/// packet should be parsed fully to ensure it is in fact a tag.
fn is_maybe_info_tag(buf: &[u8], header: &FrameHeader) -> bool {
    const MIN_XING_TAG_LEN: usize = 8;

    // Only supported with layer 3 packets.
    if header.layer != MpegLayer::Layer3 {
        return false;
    }

    // The position of the Xing/Info tag relative to the start of the packet. This is equal to the
    // side information length for the frame.
    let offset = header.side_info_len() + MPEG_HEADER_LEN;

    // The packet must be big enough to contain a tag.
    if buf.len() < offset + MIN_XING_TAG_LEN {
        return false;
    }

    // The tag ID must be present and correct.
    let id = &buf[offset..offset + 4];

    if id != XING_TAG_ID && id != INFO_TAG_ID {
        return false;
    }

    // The side information should be zeroed.
    !buf[MPEG_HEADER_LEN..offset].iter().any(|&b| b != 0)
}

const VBRI_TAG_ID: [u8; 4] = *b"VBRI";

/// The contents of a VBRI tag.
#[allow(dead_code)]
struct VbriTag {
    num_bytes: u32,
    num_mpeg_frames: u32,
}

/// Try to read a VBRI tag from the provided MPEG frame.
fn try_read_vbri_tag(buf: &[u8], header: &FrameHeader) -> Option<VbriTag> {
    // The VBRI header is a completely optional piece of information. Therefore, flatten an error
    // reading the tag into a None.
    try_read_vbri_tag_inner(buf, header).ok().flatten()
}

fn try_read_vbri_tag_inner(buf: &[u8], header: &FrameHeader) -> Result<Option<VbriTag>> {
    // Do a quick check that this is a VBRI tag.
    if !is_maybe_vbri_tag(buf, header) {
        return Ok(None);
    }

    let mut reader = BufReader::new(buf);

    // The VBRI tag is always 32 bytes after the header.
    reader.ignore_bytes(MPEG_HEADER_LEN as u64 + 32)?;

    // Check for the VBRI signature.
    let id = reader.read_quad_bytes()?;

    if id != VBRI_TAG_ID {
        return Ok(None);
    }

    // The version is always 1.
    let version = reader.read_be_u16()?;

    if version != 1 {
        return Ok(None);
    }

    // Delay is a 2-byte big-endiann floating point value?
    let _delay = reader.read_be_u16()?;
    let _quality = reader.read_be_u16()?;

    let num_bytes = reader.read_be_u32()?;
    let num_mpeg_frames = reader.read_be_u32()?;

    Ok(Some(VbriTag { num_bytes, num_mpeg_frames }))
}

/// Perform a fast check to see if the packet contains a VBRI tag. If this returns true, the
/// packet should be parsed fully to ensure it is in fact a tag.
fn is_maybe_vbri_tag(buf: &[u8], header: &FrameHeader) -> bool {
    const MIN_VBRI_TAG_LEN: usize = 26;
    const VBRI_TAG_OFFSET: usize = 36;

    // Only supported with layer 3 packets.
    if header.layer != MpegLayer::Layer3 {
        return false;
    }

    // The packet must be big enough to contain a tag.
    if buf.len() < VBRI_TAG_OFFSET + MIN_VBRI_TAG_LEN {
        return false;
    }

    // The tag ID must be present and correct.
    let id = &buf[VBRI_TAG_OFFSET..VBRI_TAG_OFFSET + 4];

    if id != VBRI_TAG_ID {
        return false;
    }

    // The bytes preceeding the VBRI tag (mostly the side information) should be all 0.
    !buf[MPEG_HEADER_LEN..VBRI_TAG_OFFSET].iter().any(|&b| b != 0)
}
