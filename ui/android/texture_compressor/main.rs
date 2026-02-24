// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//ui/android:texture_compressor";
}

use std::env;
use std::fs;
use std::fs::File;
use std::io::{BufReader, BufWriter};
use std::path::Path;
use std::time::{Duration, Instant};

use bytemuck::cast_slice;

use texture_compressor::{compress_etc1, decompress_etc1};

const ETC1_BLOCK_SIZE: u32 = 8;
const BENCHMARK_RUNS: u32 = 100;

pub struct BenchmarkStats {
    pub average: f64,
    pub std_dev: f64,
}

impl BenchmarkStats {
    fn from_timings(execution_timings: &[Duration]) -> BenchmarkStats {
        let num_runs = execution_timings.len();
        assert_ne!(num_runs, 0);
        let execution_timings_ms =
            execution_timings.iter().map(|timings| timings.as_secs_f64() * 1000.0);

        let total_ms: f64 = execution_timings_ms.clone().sum();
        let average = total_ms / num_runs as f64;

        let sum_squared_diffs: f64 = execution_timings_ms
            .clone()
            .map(|time_ms| {
                let diff = time_ms - average;
                diff * diff
            })
            .sum();
        let variance = if num_runs > 1 { sum_squared_diffs / (num_runs - 1) as f64 } else { 0.0 };
        BenchmarkStats { average: average, std_dev: variance.sqrt() }
    }
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 3 || args.len() > 4 {
        eprintln!("Usage: {} <input.png> <output.png> [etc1_output.etc1]", args[0]);
        eprintln!("\n[etc1_output]: If specified, write intermediate encoder output to this path.");
        std::process::exit(1);
    }

    let input_path = Path::new(&args[1]);
    let output_path = Path::new(&args[2]);
    let etc1_output_path = args.get(3).map(|s| Path::new(s));

    let decoder = png::Decoder::new(BufReader::new(
        File::open(input_path).expect("Failed to open input file"),
    ));
    let mut reader = decoder.read_info().expect("Failed to read PNG info");
    let mut buf = vec![0; reader.output_buffer_size().expect("Output too big")];
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

    let mut decompress = || {
        decompress_etc1(
            &etc1_data,
            &mut output_rgba,
            first_frame.width,
            first_frame.height,
            etc1_data_width,
            first_frame.width,
        )
    };

    println!("Warming up");
    decompress();

    println!("Benchmarking");
    let execution_timing = (0..BENCHMARK_RUNS)
        .map(|_| {
            let start = Instant::now();
            decompress();
            start.elapsed()
        })
        .collect::<Vec<_>>();

    let decode_stats = BenchmarkStats::from_timings(&execution_timing);

    println!("Ran decompress_etc1() {} times.", BENCHMARK_RUNS);
    println!("Average time: {:?} [ms]", decode_stats.average);
    println!("Standard deviation: {:?} [ms]", decode_stats.std_dev);

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
