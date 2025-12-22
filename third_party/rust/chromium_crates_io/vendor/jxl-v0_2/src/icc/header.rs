// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{error::Result, util::NewWithCapacity};

use super::{ICC_HEADER_SIZE, IccStream};

fn predict_header(idx: usize, output_size: u32, header: &[u8]) -> u8 {
    match idx {
        0..=3 => output_size.to_be_bytes()[idx],
        8 => 4,
        12..=23 => b"mntrRGB XYZ "[idx - 12],
        36..=39 => b"acsp"[idx - 36],
        // APPL
        41 | 42 if header[40] == b'A' => b'P',
        43 if header[40] == b'A' => b'L',
        // MSFT
        41 if header[40] == b'M' => b'S',
        42 if header[40] == b'M' => b'F',
        43 if header[40] == b'M' => b'T',
        // SGI_
        42 if header[40] == b'S' && header[41] == b'G' => b'I',
        43 if header[40] == b'S' && header[41] == b'G' => b' ',
        // SUNW
        42 if header[40] == b'S' && header[41] == b'U' => b'N',
        43 if header[40] == b'S' && header[41] == b'U' => b'W',
        70 => 246,
        71 => 214,
        73 => 1,
        78 => 211,
        79 => 45,
        80..=83 => header[4 + idx - 80],
        _ => 0,
    }
}

pub(super) fn read_header(data_stream: &mut IccStream, output_size: u64) -> Result<Vec<u8>> {
    let header_size = output_size.min(ICC_HEADER_SIZE);
    let header_data = data_stream.read_to_vec_exact(header_size as usize)?;

    let mut profile = Vec::new_with_capacity(output_size as usize)?;

    for (idx, &e) in header_data.iter().enumerate() {
        let p = predict_header(idx, output_size as u32, &header_data);
        profile.push(p.wrapping_add(e));
    }

    Ok(profile)
}
