// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::path::Path;

use crate::image::Image;
use crate::tests::decode::{
    DecodeParams, compare_frames, compute_tile_quartiles, decode, decode_internal, image_size,
};

pub fn run(path: &Path, expected_checkpoints: &[(usize, [f32; 4])]) {
    let file = std::fs::read(path).unwrap();
    // One-shot decode
    let (_, one_shot_frames) = decode(&file).unwrap();
    if one_shot_frames.is_empty() {
        return;
    }

    let size = image_size(&file).unwrap();
    let mut actual_checkpoints = Vec::new();

    let mut flush_callback = |consumed_bytes: usize,
                              f_idx: usize,
                              buffers: &[Image<f32>]|
     -> Result<(), crate::error::Error> {
        let is_checkpoint = expected_checkpoints.iter().any(|&(expected_bytes, _)| {
            consumed_bytes <= expected_bytes && consumed_bytes + 123 > expected_bytes
        });
        if is_checkpoint {
            let quartiles = compute_tile_quartiles(buffers, &one_shot_frames[f_idx], size);
            actual_checkpoints.push((consumed_bytes, quartiles));
        }
        Ok(())
    };

    // Incremental decode with progressive callback
    let (_, frames) = decode_internal(
        &file,
        DecodeParams {
            chunk_size: 123,
            do_flush: true,
            flush_callback: Some(&mut flush_callback),
            ..Default::default()
        },
    )
    .unwrap();

    // Record the final state (fully decoded frame has MSE 0.0)
    actual_checkpoints.push((file.len(), [0.0, 0.0, 0.0, 0.0]));

    for &(expected_bytes, max_quartiles) in expected_checkpoints {
        let latest_flush = actual_checkpoints
            .iter()
            .rfind(|&&(bytes, _)| bytes <= expected_bytes);

        if let Some(&(actual_bytes, actual_quartiles)) = latest_flush {
            for q_idx in 0..4 {
                let actual_q = actual_quartiles[q_idx];
                let expected_q = max_quartiles[q_idx];
                let bound = expected_q * 1.02 + 1e-6;
                assert!(
                    actual_q <= bound,
                    "Progressive decoding test failed for {:?}: at {} bytes (latest flush {} bytes), quartile {} expected <= {}, but achieved {}",
                    path,
                    expected_bytes,
                    actual_bytes,
                    q_idx + 1,
                    bound,
                    actual_q,
                );
            }
        } else {
            panic!(
                "Progressive decoding test failed for {:?}: no flush occurred at or before {} bytes (total file length: {} bytes)",
                path,
                expected_bytes,
                file.len()
            );
        }
    }

    // Compare one_shot_frames and frames
    assert_eq!(one_shot_frames.len(), frames.len());
    for (fc, (f, sf)) in frames.into_iter().zip(one_shot_frames).enumerate() {
        compare_frames(path, fc, &f, &sf);
    }
}
