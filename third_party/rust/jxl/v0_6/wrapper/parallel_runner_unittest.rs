// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//base/test:task_environment";
}

use rust_gtest_interop::prelude::*;
use std::sync::atomic::{AtomicUsize, Ordering};

#[cxx::bridge(namespace = "blink::jxl_rs")]
mod ffi {
    unsafe extern "C++" {
        include!("third_party/rust/jxl/v0_6/wrapper/parallel_runner.h");

        /// C++ `void` (aliased as `c_void` in parallel_runner.h).
        type c_void;

        /// Re-declaration of the production `RunParallelTasks` from
        /// parallel_runner.h; cxx bridges cannot import declarations from
        /// other bridges, so the one in lib.rs is not usable here. The
        /// Rust-side name differs from lib.rs because cxx derives the
        /// generated shim symbol from it, and both bridges link into the
        /// same binary (duplicate symbols otherwise).
        #[cxx_name = "RunParallelTasks"]
        unsafe fn run_parallel_tasks_for_testing(
            num_tasks: usize,
            context: *mut c_void,
            task: unsafe fn(*mut c_void, usize),
        );
    }
}

struct TaskContext {
    counts: Vec<AtomicUsize>,
}

fn run_task(context: *mut ffi::c_void, task_index: usize) {
    // SAFETY: `run_tasks` keeps `context` alive until the synchronous
    // `run_parallel_tasks` call returns.
    let context = unsafe { &*(context as *const TaskContext) };
    context.counts[task_index].fetch_add(1, Ordering::Relaxed);
}

fn run_tasks(num_tasks: usize) -> Vec<usize> {
    let _task_environment = task_environment::ffi::CreateTaskEnvironment();
    let context = TaskContext { counts: (0..num_tasks).map(|_| AtomicUsize::new(0)).collect() };

    // SAFETY: `context` remains valid until this synchronous call returns, and
    // its atomic counters are safe to update concurrently.
    unsafe {
        ffi::run_parallel_tasks_for_testing(
            num_tasks,
            &context as *const TaskContext as *mut ffi::c_void,
            run_task,
        );
    }

    context.counts.iter().map(|count| count.load(Ordering::Relaxed)).collect()
}

#[gtest(ParallelRunnerTest, RunsNoTasks)]
fn test_runs_no_tasks() {
    expect_true!(run_tasks(0).is_empty());
}

#[gtest(ParallelRunnerTest, RunsOneTask)]
fn test_runs_one_task() {
    let counts = run_tasks(1);
    expect_eq!(counts[0], 1);
}

#[gtest(ParallelRunnerTest, RunsEachTaskExactlyOnce)]
fn test_runs_each_task_exactly_once() {
    let counts = run_tasks(100);
    expect_eq!(counts.len(), 100);
    for count in counts {
        expect_eq!(count, 1);
    }
}
