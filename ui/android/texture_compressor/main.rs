// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::env;
use std::fs;
use std::fs::File;
use std::io::{BufReader, BufWriter};
use std::path::Path;

use bytemuck::cast_slice;
use ui_sandroid_ctexture_ucompressor::{compress_etc1, decompress_etc1};

const ETC1_BLOCK_SIZE: u32 = 8;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 3 || args.len() > 4 {
        eprintln!("Usage: {} <input.png> <output.png> [etc1_output.etc1]", args[0]);
        std::process::exit(1);
    }

    let input_path = Path::new(&args[1]);
    let output_path = Path::new(&args[2]);
    let etc1_output_path = args.get(3).map(|s| Path::new(s));

    let decoder = png::Decoder::new(BufReader::new(
        File::open(input_path).expect("Failed to open input file"),
    ));
    let mut reader = decoder.read_info().expect("Failed to read PNG info");
    let mut buf = vec![0; reader.output_buffer_size()];
    let first_frame = reader.next_frame(&mut buf).expect("Failed to read PNG frame");
    let input_rgba = &buf[..first_frame.buffer_size()];
    let etc1_data_width = first_frame.width.div_ceil(4);
    let etc1_data_height = first_frame.height.div_ceil(4);
    let etc1_data_len = etc1_data_height
        .checked_mul(etc1_data_width)
        .and_then(|blocks| blocks.checked_mul(ETC1_BLOCK_SIZE))
        .expect("Input image is too big");

    let mut etc1_data = vec![0u8; etc1_data_len as usize];

    // `input_rgba` is in the order of RGBARGBA...
    // After casting, R becomes the lowermost byte and A becomes the uppermost byte.
    // Note that this program only supports little-endian machines. (See
    // interleave_etc1)
    compress_etc1(
        cast_slice(input_rgba),
        &mut etc1_data,
        first_frame.width,
        first_frame.height,
        (first_frame.line_size / 4) as u32,
        etc1_data_width,
    );

    if let Some(etc1_output_path) = etc1_output_path {
        println!("ETC1 output will be saved to: {}", etc1_output_path.display());
        fs::write(etc1_output_path, &etc1_data).expect("Failed to save intermediate ETC1 output");
    }

    let mut output_rgba = vec![0u32; (first_frame.height * first_frame.width) as usize];

    decompress_etc1(
        &etc1_data,
        &mut output_rgba,
        first_frame.width,
        first_frame.height,
        etc1_data_width,
        first_frame.width,
    );

    let mut encoder = png::Encoder::new(
        BufWriter::new(File::create(output_path).expect("Failed to create output file")),
        first_frame.width,
        first_frame.height,
    );
    encoder.set_color(first_frame.color_type);
    encoder.set_depth(first_frame.bit_depth);
    let mut writer = encoder.write_header().expect("Failed to write PNG header");

    // See above for the layout of `input_rgba` and `output_rgba`
    writer.write_image_data(cast_slice(&output_rgba)).expect("Failed to write PNG data");
}
