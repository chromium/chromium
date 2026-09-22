// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::collections::HashMap;
use std::path::Path;

use crate::error::Error;
use crate::image::Image;
use crate::tests::decode::{
    DecodeParams, compare_frames, compute_tile_quartiles, decode, decode_32bit, decode_internal,
    image_size,
};

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

pub fn run(path: &Path, expected_checkpoints: &[(usize, [f32; 4])]) {
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

    let size = image_size(&file).unwrap();

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
        DecodeParams {
            chunk_size,
            do_flush: true,
            flush_callback: Some(&mut cb_16),
            ..Default::default()
        },
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
        DecodeParams {
            chunk_size,
            do_flush: true,
            flush_callback: Some(&mut cb_32),
            disable_16bit_modular_buffers: true,
            ..Default::default()
        },
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
        let q16 = compute_tile_quartiles(buf16, &frames_16[*f16], size);
        let q32 = compute_tile_quartiles(buf32, &frames_32[*f32], size);
        for q_idx in 0..4 {
            let bound = max_mse[q_idx] * 1.02 + 1e-6;
            assert!(
                q16[q_idx] <= bound,
                "16-bit quartile {} ({}) exceeded expected bound {} at {} bytes for {:?}",
                q_idx + 1,
                q16[q_idx],
                bound,
                expected_bytes,
                path
            );
            assert!(
                q32[q_idx] <= bound,
                "32-bit quartile {} ({}) exceeded expected bound {} at {} bytes for {:?}",
                q_idx + 1,
                q32[q_idx],
                bound,
                expected_bytes,
                path
            );
        }
    }
}
