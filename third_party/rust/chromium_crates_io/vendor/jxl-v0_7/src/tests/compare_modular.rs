// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::collections::HashMap;
use std::path::Path;

use crate::error::Error;
use crate::image::Image;
use crate::tests::decode::{compare_frames, compute_mse, decode, decode_32bit, decode_internal};

fn clone_images(imgs: &[Image<f32>]) -> Vec<Image<f32>> {
    imgs.iter()
        .map(|img| {
            let mut copy = Image::new(img.size()).unwrap();
            for y in 0..img.size().1 {
                copy.row_mut(y).copy_from_slice(img.row(y));
            }
            copy
        })
        .collect()
}

pub fn run(path: &Path, expected_checkpoints: &[(usize, f32)]) {
    let file = std::fs::read(path).unwrap();

    // 1. One-shot decode in 16-bit (normal) and 32-bit mode
    let (_, frames_16) = decode(&file).unwrap();
    if frames_16.is_empty() {
        return;
    }

    let (_, frames_32) = decode_32bit(&file).unwrap();

    assert_eq!(
        frames_16.len(),
        frames_32.len(),
        "Frame count mismatch between 16-bit and 32-bit decode for {:?}",
        path
    );
    for (fc, (f16, f32)) in frames_16.iter().zip(frames_32.iter()).enumerate() {
        compare_frames(path, fc, f16, f32);
    }

    // If expected_checkpoints is empty, we have verified the final render.
    if expected_checkpoints.is_empty() {
        return;
    }

    // 2. Incremental progressive decode with chunk_size = 123
    let chunk_size = 123;

    let mut latest_cp_16: HashMap<usize, (usize, usize, Vec<Image<f32>>)> = HashMap::new();
    let mut cb_16 =
        |consumed_bytes: usize, f_idx: usize, buffers: &[Image<f32>]| -> Result<(), Error> {
            for (cp_idx, &(cp_bytes, _)) in expected_checkpoints.iter().enumerate() {
                if consumed_bytes <= cp_bytes && consumed_bytes + chunk_size > cp_bytes {
                    latest_cp_16.insert(cp_idx, (consumed_bytes, f_idx, clone_images(buffers)));
                }
            }
            Ok(())
        };
    let _ = decode_internal(
        &file,
        chunk_size,
        false,
        true,
        None,
        Some(&mut cb_16),
        None,
        false,
    );

    let mut latest_cp_32: HashMap<usize, (usize, usize, Vec<Image<f32>>)> = HashMap::new();
    let mut cb_32 =
        |consumed_bytes: usize, f_idx: usize, buffers: &[Image<f32>]| -> Result<(), Error> {
            for (cp_idx, &(cp_bytes, _)) in expected_checkpoints.iter().enumerate() {
                if consumed_bytes <= cp_bytes && consumed_bytes + chunk_size > cp_bytes {
                    latest_cp_32.insert(cp_idx, (consumed_bytes, f_idx, clone_images(buffers)));
                }
            }
            Ok(())
        };
    let _ = decode_internal(
        &file,
        chunk_size,
        false,
        true,
        None,
        Some(&mut cb_32),
        None,
        true,
    );

    // 3. Validate exact equality and target MSE bounds at checkpoints
    for (cp_idx, &(expected_bytes, max_mse)) in expected_checkpoints.iter().enumerate() {
        let (b16, f16, buf16) = latest_cp_16.get(&cp_idx).unwrap_or_else(|| {
            panic!(
                "Progressive decoding test failed for {:?}: no 16-bit flush occurred at or before {} bytes (total file length: {} bytes)",
                path, expected_bytes, file.len()
            )
        });
        let (b32, f32, buf32) = latest_cp_32.get(&cp_idx).unwrap_or_else(|| {
            panic!(
                "Progressive decoding test failed for {:?}: no 32-bit flush occurred at or before {} bytes (total file length: {} bytes)",
                path, expected_bytes, file.len()
            )
        });

        assert_eq!(
            b16, b32,
            "Checkpoint {} consumed bytes mismatch for {:?}",
            cp_idx, path
        );
        assert_eq!(
            f16, f32,
            "Checkpoint {} frame index mismatch for {:?}",
            cp_idx, path
        );
        compare_frames(path, *f16, buf16, buf32);
        let mse_16 = compute_mse(buf16, &frames_16[*f16]);
        let mse_32 = compute_mse(buf32, &frames_32[*f32]);
        assert!(
            mse_16 <= max_mse * 1.02 + 1e-6,
            "16-bit MSE {} exceeded max_mse {} at {} bytes for {:?}",
            mse_16,
            max_mse,
            expected_bytes,
            path
        );
        assert!(
            mse_32 <= max_mse * 1.02 + 1e-6,
            "32-bit MSE {} exceeded max_mse {} at {} bytes for {:?}",
            mse_32,
            max_mse,
            expected_bytes,
            path
        );
    }
}
