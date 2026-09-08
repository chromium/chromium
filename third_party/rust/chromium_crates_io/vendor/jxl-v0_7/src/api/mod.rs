// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// #![warn(missing_docs)]

mod color;
mod data_types;
mod decoder;
mod inner;
mod input;
mod options;
mod signature;
mod xyb_constants;

use std::sync::atomic::{AtomicUsize, Ordering};

pub use color::*;
pub use data_types::*;
pub use decoder::*;
pub use inner::*;
pub use input::*;
pub use options::*;
pub use signature::*;

use crate::error::Result;
use crate::headers::image_metadata::Orientation;
pub use crate::image::JxlOutputBuffer;

/// This type represents the return value of a function that reads input from a bitstream. The
/// variant `Complete` indicates that the operation was completed successfully, and its return
/// value is available. The variant `NeedsMoreInput` indicates that more input is needed, and the
/// function should be called again. This variant comes with a `size_hint`, representing an
/// estimate of the number of additional bytes needed, and a `fallback`, representing additional
/// information that might be needed to call the function again (i.e. because it takes a decoder
/// object by value).
#[derive(Debug, PartialEq)]
pub enum ProcessingResult<T, U> {
    Complete { result: T },
    NeedsMoreInput { size_hint: usize, fallback: U },
}

impl<T> ProcessingResult<T, ()> {
    fn new(
        result: Result<T, crate::error::Error>,
    ) -> Result<ProcessingResult<T, ()>, crate::error::Error> {
        match result {
            Ok(v) => Ok(ProcessingResult::Complete { result: v }),
            Err(crate::error::Error::OutOfBounds(v)) => Ok(ProcessingResult::NeedsMoreInput {
                size_hint: v,
                fallback: (),
            }),
            Err(e) => Err(e),
        }
    }
}

#[derive(Clone)]
pub struct ToneMapping {
    pub intensity_target: f32,
    pub min_nits: f32,
    pub relative_to_max_display: bool,
    pub linear_below: f32,
}

#[derive(Clone)]
pub struct JxlBasicInfo {
    pub size: (usize, usize),
    pub bit_depth: JxlBitDepth,
    pub orientation: Orientation,
    pub extra_channels: Vec<JxlExtraChannel>,
    pub animation: Option<JxlAnimation>,
    pub uses_original_profile: bool,
    pub tone_mapping: ToneMapping,
    pub preview_size: Option<(usize, usize)>,
}

pub type JxlParallelRunnerFun<'a> = dyn Fn(usize) -> Result<()> + Sync + 'a;

pub trait JxlParallelRunner {
    /// Runs `fun(i)` for each `i` in `0..num`, possibly in parallel.
    ///
    /// The calls *might* happen in parallel or sequentially, and no promises
    /// are made on the order of the calls.
    /// This implies that different invocations of `fun(i)` are not allowed
    /// to wait on each other.
    fn run(&mut self, num: usize, fun: &JxlParallelRunnerFun<'_>) -> Result<()>;

    /// Returns an estimate of the number of parallel threads that this parallel
    /// runner will use.
    ///
    /// Note that this is just an optimization hint.
    fn num_threads(&self) -> usize;

    /// Runs `fun(i)` for each `i` in `0..num`, possibly in parallel.
    ///
    /// Equivalent to `run`, but attempts to start tasks in roughly sequential
    /// order and receives a hint on the number of threads to use.
    /// This is not a hard guarantee, but doing otherwise might have negative
    /// performance implications.
    /// The default implementation uses `run` to start
    /// `min(num_threads, num, max_threads)` tasks, and uses an atomic counter
    /// to ensure each task is executed exactly once and approximately in
    /// order.
    fn run_ordered(
        &mut self,
        num: usize,
        max_threads: Option<usize>,
        fun: &JxlParallelRunnerFun<'_>,
    ) -> Result<()> {
        let max_threads = max_threads
            .unwrap_or(usize::MAX)
            .min(self.num_threads())
            .min(num);
        if max_threads <= 1 {
            for i in 0..num {
                fun(i)?;
            }
            return Ok(());
        }
        let next_index = AtomicUsize::new(0);
        self.run(max_threads, &|_| loop {
            let t = next_index.fetch_add(1, Ordering::Relaxed);
            if t >= num {
                return Ok(());
            }
            fun(t)?;
        })
    }
}
