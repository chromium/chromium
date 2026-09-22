// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::path::Path;

#[cfg(feature = "shuttle")]
use shuttle::thread;

use crate::error::Error;
use crate::image::Image;
use crate::tests::decode::{DecodeParams, compare_frames, decode_internal};
use crate::tests::parallel_runner::TestParallelRunner;

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

pub fn run_oneshot(path: &Path) {
    let file = std::fs::read(path).unwrap();

    // Oneshot sequential decode
    let (_, seq_frames) = decode_internal(&file, DecodeParams::default()).unwrap();

    if seq_frames.is_empty() {
        return;
    }

    // Oneshot parallel decode
    let mut runner = TestParallelRunner {
        // TEST_MAX_THREADS makes it possible to explore thread-count-dependent
        // interleavings (in particular under shuttle).
        max_threads: std::env::var("TEST_MAX_THREADS")
            .ok()
            .and_then(|x| x.parse().ok())
            .unwrap_or(4),
    };
    let (_, par_frames) = decode_internal(
        &file,
        DecodeParams {
            parallel_runner: Some(&mut runner),
            ..Default::default()
        },
    )
    .unwrap();

    assert_eq!(
        seq_frames.len(),
        par_frames.len(),
        "Parallel and sequential frame counts differ for {:?}",
        path
    );

    for (fc, (seq_f, par_f)) in seq_frames.into_iter().zip(par_frames).enumerate() {
        compare_frames(path, fc, &par_f, &seq_f);
    }
}

pub fn run_progressive(path: &Path) {
    let file = std::fs::read(path).unwrap();

    let chunk_size = (file.len() / 8).max(1024);

    let mut seq_flushes: Vec<(usize, usize, Vec<Image<f32>>)> = Vec::new();
    let mut seq_callback =
        |consumed_bytes: usize, f_idx: usize, buffers: &[Image<f32>]| -> Result<(), Error> {
            seq_flushes.push((consumed_bytes, f_idx, clone_images(buffers)));
            Ok(())
        };

    // Sequential progressive decode
    let _ = decode_internal(
        &file,
        DecodeParams {
            chunk_size,
            do_flush: true,
            flush_callback: Some(&mut seq_callback),
            ..Default::default()
        },
    );

    let mut par_flushes: Vec<(usize, usize, Vec<Image<f32>>)> = Vec::new();
    let mut par_callback =
        |consumed_bytes: usize, f_idx: usize, buffers: &[Image<f32>]| -> Result<(), Error> {
            par_flushes.push((consumed_bytes, f_idx, clone_images(buffers)));
            Ok(())
        };

    // Parallel progressive decode
    let mut runner = TestParallelRunner {
        // TEST_MAX_THREADS makes it possible to explore thread-count-dependent
        // interleavings (in particular under shuttle).
        max_threads: std::env::var("TEST_MAX_THREADS")
            .ok()
            .and_then(|x| x.parse().ok())
            .unwrap_or(4),
    };
    let _ = decode_internal(
        &file,
        DecodeParams {
            chunk_size,
            do_flush: true,
            flush_callback: Some(&mut par_callback),
            parallel_runner: Some(&mut runner),
            ..Default::default()
        },
    );

    assert_eq!(
        seq_flushes.len(),
        par_flushes.len(),
        "Parallel and sequential flush counts differ for {:?}",
        path
    );

    for (idx, ((seq_bytes, seq_f_idx, seq_bufs), (par_bytes, par_f_idx, par_bufs))) in
        seq_flushes.into_iter().zip(par_flushes).enumerate()
    {
        assert_eq!(
            seq_bytes, par_bytes,
            "Flush {} consumed bytes mismatch for {:?}",
            idx, path
        );
        assert_eq!(
            seq_f_idx, par_f_idx,
            "Flush {} frame index mismatch for {:?}",
            idx, path
        );
        compare_frames(path, seq_f_idx, &par_bufs, &seq_bufs);
    }
}

// Runs `f` under the shuttle scheduler selected via environment variables:
// - default: random scheduling, SHUTTLE_ITERATIONS iterations (default 10);
//   set SHUTTLE_RANDOM_SEED to replay a failure reported as "failing seed".
// - SHUTTLE_SCHEDULER=pct: PCT scheduling with SHUTTLE_PCT_DEPTH preemptions
//   (default 3), which finds some bugs random scheduling misses.
// - SHUTTLE_SCHEDULER=replay: replays the schedule stored in the file pointed
//   to by SHUTTLE_REPLAY_FILE (a "failing schedule" printed by a failure).
#[cfg(feature = "shuttle")]
pub fn run_shuttle_test(path: std::path::PathBuf, f: fn(&Path)) {
    let iterations = std::env::var("SHUTTLE_ITERATIONS")
        .ok()
        .and_then(|x| x.parse().ok())
        .unwrap_or(10);

    let mut config = shuttle::Config::default();
    config.max_steps = shuttle::MaxSteps::FailAfter(10_000_000);
    config.stack_size = 1024 * 1024;

    let test = move || {
        // Images small enough to be decoded as a single group never spawn
        // worker threads, and the PCT scheduler treats a test without any
        // concurrency as an error. Spawn a trivial thread (with a yield, so
        // that the scheduler sees at least one point with two runnable
        // tasks) to mask that.
        let t = thread::spawn(|| {});
        thread::yield_now();
        t.join().unwrap();
        f(&path);
    };

    match std::env::var("SHUTTLE_SCHEDULER").as_deref() {
        Ok("replay") => {
            let schedule =
                std::fs::read_to_string(std::env::var("SHUTTLE_REPLAY_FILE").unwrap()).unwrap();
            let scheduler = shuttle::scheduler::ReplayScheduler::new_from_encoded(schedule.trim());
            shuttle::Runner::new(scheduler, config).run(test);
        }
        Ok("pct") => {
            let depth = std::env::var("SHUTTLE_PCT_DEPTH")
                .ok()
                .and_then(|x| x.parse().ok())
                .unwrap_or(3);
            let scheduler = shuttle::scheduler::PctScheduler::new(depth, iterations);
            shuttle::Runner::new(scheduler, config).run(test);
        }
        _ => {
            let scheduler = shuttle::scheduler::RandomScheduler::new(iterations);
            shuttle::Runner::new(scheduler, config).run(test);
        }
    }
}
