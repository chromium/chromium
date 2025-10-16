// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::env;
use std::fs::File;
use std::io::{BufReader, BufWriter};
use std::path::Path;

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
    let info = reader.next_frame(&mut buf).expect("Failed to read PNG frame");
    let bytes = &buf[..info.buffer_size()];

    // TODO: Encode & decode through the ETC1 codec before converting back to PNG.
    if let Some(etc1_output_path) = etc1_output_path {
        // TODO: Save the ETC1 encoded blob to etc1_output_path for inspection.
        println!("ETC1 output will be saved to: {}", etc1_output_path.display());
    }

    let mut encoder = png::Encoder::new(
        BufWriter::new(File::create(output_path).expect("Failed to create output file")),
        info.width,
        info.height,
    );
    encoder.set_color(info.color_type);
    encoder.set_depth(info.bit_depth);
    let mut writer = encoder.write_header().expect("Failed to write PNG header");
    writer.write_image_data(bytes).expect("Failed to write PNG data");
}
