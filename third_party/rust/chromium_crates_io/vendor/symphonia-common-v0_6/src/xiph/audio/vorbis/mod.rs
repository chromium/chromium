// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use symphonia_core::audio::{Channels, Position};
use symphonia_core::errors::{Result, decode_error};

/// Get the mapping 0 channel listing for the given number of channels.
pub fn vorbis_channels_to_channels(num_channels: u8) -> Option<Channels> {
    let positions = match num_channels {
        1 => Position::FRONT_LEFT,
        2 => Position::FRONT_LEFT | Position::FRONT_RIGHT,
        3 => Position::FRONT_LEFT | Position::FRONT_CENTER | Position::FRONT_RIGHT,
        4 => {
            Position::FRONT_LEFT
                | Position::FRONT_RIGHT
                | Position::REAR_LEFT
                | Position::REAR_RIGHT
        }
        5 => {
            Position::FRONT_LEFT
                | Position::FRONT_CENTER
                | Position::FRONT_RIGHT
                | Position::REAR_LEFT
                | Position::REAR_RIGHT
        }
        6 => {
            Position::FRONT_LEFT
                | Position::FRONT_CENTER
                | Position::FRONT_RIGHT
                | Position::REAR_LEFT
                | Position::REAR_RIGHT
                | Position::LFE1
        }
        7 => {
            Position::FRONT_LEFT
                | Position::FRONT_CENTER
                | Position::FRONT_RIGHT
                | Position::SIDE_LEFT
                | Position::SIDE_RIGHT
                | Position::REAR_CENTER
                | Position::LFE1
        }
        8 => {
            Position::FRONT_LEFT
                | Position::FRONT_CENTER
                | Position::FRONT_RIGHT
                | Position::SIDE_LEFT
                | Position::SIDE_RIGHT
                | Position::REAR_LEFT
                | Position::REAR_RIGHT
                | Position::LFE1
        }
        _ => return None,
    };

    Some(Channels::Positioned(positions))
}

// Xiph lacing for three packets starts with `2`, if it's not 2 assume the extradata is
// parseable as a raw Vorbis identification header (which must start with a `1`).
pub const XIPH_LACED_LEADING_HEADER: u8 = 2;

/// Unpack Vorbis extradata packed in the Xiph lacing format (used by WebM/Matroska).
///
/// If the data is not Xiph laced (does not start with 2), it is returned as is.
/// If it is Xiph laced, it extracts the Identification and Setup packets and returns
/// them concatenated.
pub fn unpack_xiph_laced_extradata(extradata: &[u8]) -> Result<(&[u8], &[u8])> {
    let Some((&header_count, lacing_data)) = extradata.split_first()
    else {
        return decode_error("vorbis: extradata is empty");
    };

    if header_count != XIPH_LACED_LEADING_HEADER {
        return decode_error("vorbis: invalid Xiph lacing count");
    }

    let mut iter = lacing_data.iter();
    let mut lengths = [0usize; 2]; // Xiph lacing for 3 packets encodes exactly 2 lengths
    for length in &mut lengths {
        loop {
            let &val = match iter.next() {
                Some(v) => v,
                None => return decode_error("vorbis: truncated length lacing"),
            };
            *length = match length.checked_add(val as usize) {
                Some(l) => l,
                None => return decode_error("vorbis: lacing length overflow"),
            };
            if val < 255 {
                break;
            }
        }
    }
    let [ident_len, comment_len] = lengths;

    let remaining_data = iter.as_slice();
    if remaining_data.is_empty() {
        return decode_error("vorbis: no data remains after reading lacing");
    }

    let id_and_comment_len = match ident_len.checked_add(comment_len) {
        Some(len) if len <= remaining_data.len() => len,
        _ => return decode_error("vorbis: header lengths exceed buffer size"),
    };

    let id_packet = &remaining_data[..ident_len];
    let setup_packet = &remaining_data[id_and_comment_len..];
    Ok((id_packet, setup_packet))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_unpack_xiph_laced_extradata_valid() {
        let id_packet = b"id_packet";
        let comment_packet = b"comment_packet";
        let setup_packet = b"setup_packet";

        let mut extradata = Vec::new();
        extradata.push(XIPH_LACED_LEADING_HEADER);
        extradata.push(id_packet.len() as u8);
        extradata.push(comment_packet.len() as u8);
        extradata.extend_from_slice(id_packet);
        extradata.extend_from_slice(comment_packet);
        extradata.extend_from_slice(setup_packet);

        let result = unpack_xiph_laced_extradata(&extradata).unwrap();

        assert_eq!(result, (id_packet.as_ref(), setup_packet.as_ref()));
    }

    #[test]
    fn test_unpack_xiph_laced_extradata_truncated() {
        let id_packet = b"id";
        let comment_packet = b"comment";

        let mut extradata = Vec::new();
        extradata.push(XIPH_LACED_LEADING_HEADER);
        extradata.push(id_packet.len() as u8);
        extradata.push(comment_packet.len() as u8);
        extradata.extend_from_slice(id_packet);

        let result = unpack_xiph_laced_extradata(&extradata);
        assert!(result.is_err());
    }

    #[test]
    fn test_unpack_xiph_laced_extradata_large_sizes() {
        let setup_packet = b"setup";

        let mut extradata = Vec::new();
        extradata.push(XIPH_LACED_LEADING_HEADER);
        extradata.push(255);
        extradata.push(0);
        extradata.push(0);
        extradata.extend_from_slice(&vec![0; 255]);
        extradata.extend_from_slice(setup_packet);

        let result = unpack_xiph_laced_extradata(&extradata).unwrap();

        let expected_id = vec![0; 255];
        assert_eq!(result, (expected_id.as_slice(), setup_packet.as_ref()));
    }

    #[test]
    fn test_unpack_xiph_laced_extradata_empty() {
        let result = unpack_xiph_laced_extradata(&[]);
        assert!(result.is_err());
    }

    #[test]
    fn test_unpack_xiph_laced_extradata_invalid_count() {
        let extradata = vec![1, 30, 0]; // Raw vorbis identification header starts with 1
        let result = unpack_xiph_laced_extradata(&extradata);
        assert!(result.is_err());
    }

    #[test]
    fn test_unpack_xiph_laced_extradata_truncated_lacing() {
        // Lacing is incomplete (we have a 255 but no next byte to finish the size)
        let extradata = vec![XIPH_LACED_LEADING_HEADER, 255];
        let result = unpack_xiph_laced_extradata(&extradata);
        assert!(result.is_err());
    }

    #[test]
    fn test_unpack_xiph_laced_extradata_no_data_after_lacing() {
        // Lacing completes successfully (lengths 0 and 0) but no actual packet data follows
        let extradata = vec![XIPH_LACED_LEADING_HEADER, 0, 0];
        let result = unpack_xiph_laced_extradata(&extradata);
        assert!(result.is_err());
    }
}
