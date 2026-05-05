// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! RIFF-based metadata formats reading.

#[cfg(feature = "riff-info")]
mod info {
    //! RIFF INFO chunk metadata format reading.

    use std::collections::HashMap;
    use std::str;

    use lazy_static::lazy_static;

    use symphonia_core::errors::{Result, decode_error};
    use symphonia_core::meta::{MetadataBuilder, RawTag};

    use crate::utils::std_tag::*;

    lazy_static! {
        static ref RIFF_INFO_MAP: RawTagParserMap = {
            let mut m: RawTagParserMap = HashMap::new();
            m.insert("ages", parse_rating);
            m.insert("cmnt", parse_comment);
            m.insert("comm", parse_comment); // TODO: Same as "cmnt"
            m.insert("dtim", parse_recording_time);
            m.insert("genr", parse_genre);
            m.insert("iart", parse_artist);
            m.insert("icmt", parse_comment); // TODO: Same as "cmnt"?
            m.insert("icnt", parse_release_country);
            m.insert("icop", parse_copyright);
            m.insert("icrd", parse_recording_date);
            m.insert("idit", parse_recording_date); // TODO: Actually date of last edit?
            m.insert("ienc", parse_encoded_by);
            m.insert("ieng", parse_engineer);
            m.insert("ifrm", parse_track_total);
            m.insert("ignr", parse_genre);
            m.insert("ilng", parse_language);
            m.insert("imed", parse_media_format);
            m.insert("imus", parse_composer);
            m.insert("inam", parse_track_title);
            m.insert("iprd", parse_album);
            m.insert("ipro", parse_producer);
            m.insert("iprt", parse_track_number_exclusive);
            m.insert("irtd", parse_rating);
            m.insert("isft", parse_encoder);
            m.insert("isgn", parse_genre);
            m.insert("isrf", parse_media_format);
            m.insert("itch", parse_encoded_by);
            m.insert("itoc", parse_cdtoc);
            m.insert("itrk", parse_track_number_exclusive);
            m.insert("iwri", parse_writer);
            m.insert("lang", parse_language);
            m.insert("prt1", parse_part_number_exclusive);
            m.insert("prt2", parse_part_total);
            m.insert("titl", parse_track_title); // TODO: Same as "inam"?
            m.insert("torg", parse_label);
            m.insert("trck", parse_track_number_exclusive);
            m.insert("tver", parse_version);
            m.insert("year", parse_recording_year);
            m
        };
    }

    /// Parse a RIFF INFO chunk into a `Tag` using the chunk's identifier and a slice containing the
    /// chunk's payload.
    pub fn parse_riff_info_chunk(
        chunk_id: [u8; 4],
        buf: &[u8],
        builder: &mut MetadataBuilder,
    ) -> Result<()> {
        // It is invalid for a chunk ID to contain non-ASCII characters, or ASCII control characters
        // per EA-IFF-85. Since chunk IDs are supposed to be well-defined, and each metadata/info
        // chunk has a standardized ID, this is a hard error.
        if chunk_id.iter().any(|c| !c.is_ascii() || c.is_ascii_control()) {
            return decode_error("meta (riff): chunk ID is invalid");
        }

        // Safety: Key is always ASCII.
        let key = str::from_utf8(&chunk_id).unwrap();
        let value = String::from_utf8_lossy(buf);

        builder.add_mapped_tags(RawTag::new(key, value), &RIFF_INFO_MAP);

        Ok(())
    }
}

#[cfg(feature = "riff-id3")]
mod id3 {
    //! RIFF ID3 chunk metadata format reading.

    use symphonia_core::errors::Result;
    use symphonia_core::io::ReadBytes;
    use symphonia_core::meta::{MetadataBuilder, MetadataRevision, MetadataSideData};

    /// Read a RIFF ID3 chunk payload.
    pub fn read_riff_id3_chunk<B: ReadBytes>(
        reader: &mut B,
        side_data: &mut Vec<MetadataSideData>,
    ) -> Result<MetadataRevision> {
        let mut builder = MetadataBuilder::new(crate::id3v2::ID3V2_METADATA_INFO);
        crate::id3v2::read_id3v2(reader, &mut builder, side_data)?;
        Ok(builder.build())
    }
}

#[cfg(feature = "riff-id3")]
pub use id3::*;
#[cfg(feature = "riff-info")]
pub use info::*;
