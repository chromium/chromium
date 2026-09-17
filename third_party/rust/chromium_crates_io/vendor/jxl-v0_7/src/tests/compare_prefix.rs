// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::path::Path;

use crate::tests::decode::{
    DecodeParams, compare_frames_close, decode_internal, has_decoded_pixels,
};
#[cfg(not(any(target_family = "wasm", target_arch = "wasm32")))]
use crate::tests::parallel_runner::TestParallelRunner;

pub fn run(path: &Path) {
    let file = std::fs::read(path).unwrap();
    if file.is_empty() {
        return;
    }

    arbtest::arbtest(|u| {
        let prefix_len = u.int_in_range(1..=file.len())?;
        let prefix_data = &file[..prefix_len];

        // Sequential one-shot decode of the prefix
        let oneshot_frames = match decode_internal(
            prefix_data,
            DecodeParams {
                allow_partial: true,
                ..Default::default()
            },
        ) {
            Ok((_, frames)) => frames,
            Err(_) => vec![],
        };

        // Sequential chunked decode of the prefix
        let chunk_size = u.int_in_range(1..=4096)?;
        let chunked_seq_frames = match decode_internal(
            prefix_data,
            DecodeParams {
                chunk_size,
                allow_partial: true,
                ..Default::default()
            },
        ) {
            Ok((_, frames)) => frames,
            Err(_) => vec![],
        };

        // Parallel chunked decode of the prefix
        #[cfg(not(any(target_family = "wasm", target_arch = "wasm32")))]
        let chunked_par_frames = {
            let mut runner = TestParallelRunner {
                max_threads: u.int_in_range(2..=4)?,
            };
            match decode_internal(
                prefix_data,
                DecodeParams {
                    chunk_size,
                    parallel_runner: Some(&mut runner),
                    allow_partial: true,
                    ..Default::default()
                },
            ) {
                Ok((_, frames)) => frames,
                Err(_) => vec![],
            }
        };

        let check_match = |candidate_frames: &[Vec<crate::image::Image<f32>>],
                           candidate_desc: &str| {
            match (oneshot_frames.is_empty(), candidate_frames.is_empty()) {
                (true, true) => {}
                (false, false) => {
                    assert_eq!(
                        oneshot_frames.len(),
                        candidate_frames.len(),
                        "Frame count mismatch between sequential one-shot and {} at prefix {} bytes for {:?}",
                        candidate_desc,
                        prefix_len,
                        path
                    );
                    for (f_idx, (f_oneshot, f_cand)) in oneshot_frames
                        .iter()
                        .zip(candidate_frames.iter())
                        .enumerate()
                    {
                        compare_frames_close(path, f_idx, f_oneshot, f_cand, 5e-3);
                    }
                }
                (false, true) => {
                    if has_decoded_pixels(&oneshot_frames) {
                        panic!(
                            "Prefix equivalence failure for {:?}: sequential one-shot decoded pixels, but {} did not at prefix {} bytes",
                            path, candidate_desc, prefix_len
                        );
                    }
                }
                (true, false) => {
                    if has_decoded_pixels(candidate_frames) {
                        panic!(
                            "Prefix equivalence failure for {:?}: {} decoded pixels, but sequential one-shot did not at prefix {} bytes",
                            path, candidate_desc, prefix_len
                        );
                    }
                }
            }
        };

        check_match(&chunked_seq_frames, "sequential chunked decode");
        #[cfg(not(any(target_family = "wasm", target_arch = "wasm32")))]
        check_match(&chunked_par_frames, "parallel chunked decode");

        Ok(())
    });
}
