// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::image::Image;
use crate::tests::decode::{compare_frames, compute_mse, decode, decode_internal};
use std::path::Path;

pub fn run(path: &Path, expected_checkpoints: &[(usize, f32)]) {
    let file = std::fs::read(path).unwrap();
    // One-shot decode
    let (_, one_shot_frames) = decode(&file).unwrap();
    if one_shot_frames.is_empty() {
        return;
    }

    let mut actual_checkpoints = Vec::new();

    let mut flush_callback = |consumed_bytes: usize,
                              f_idx: usize,
                              buffers: &[Image<f32>]|
     -> Result<(), crate::error::Error> {
        let is_checkpoint = expected_checkpoints.iter().any(|&(expected_bytes, _)| {
            consumed_bytes <= expected_bytes && consumed_bytes + 123 > expected_bytes
        });
        if is_checkpoint {
            let mse = compute_mse(buffers, &one_shot_frames[f_idx]);
            actual_checkpoints.push((consumed_bytes, mse));
        }
        Ok(())
    };

    // Incremental decode with progressive callback
    let (_, frames) =
        decode_internal(&file, 123, false, true, None, Some(&mut flush_callback)).unwrap();

    // Record the final state (fully decoded frame has MSE 0.0)
    actual_checkpoints.push((file.len(), 0.0));

    // Validate actual MSE against expected checkpoints
    for &(expected_bytes, max_mse) in expected_checkpoints {
        let latest_flush = actual_checkpoints
            .iter()
            .rfind(|&&(bytes, _)| bytes <= expected_bytes);

        if let Some(&(actual_bytes, actual_mse)) = latest_flush {
            assert!(
                actual_mse <= max_mse * 1.02 + 1e-6,
                "Progressive decoding test failed for {:?}: at {} bytes, expected MSE <= {}, but latest flush at {} bytes achieved MSE {} (actual PSNR: {:.2} dB, expected: {:.2} dB)",
                path,
                expected_bytes,
                max_mse * 1.02 + 1e-6,
                actual_bytes,
                actual_mse,
                if actual_mse > 0.0 {
                    -10.0 * actual_mse.log10()
                } else {
                    99.99
                },
                if (max_mse * 1.02 + 1e-6) > 0.0 {
                    -10.0 * (max_mse * 1.02 + 1e-6).log10()
                } else {
                    99.99
                }
            );
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
