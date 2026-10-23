// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// #![warn(missing_docs)]

mod box_parser;
mod codestream_parser;
mod color;
mod data_types;
mod decoder;
mod input;
mod options;
mod signature;
mod xyb_constants;

use std::sync::atomic::{AtomicUsize, Ordering};

pub(crate) use box_parser::BoxParserCheckpoint;
pub use box_parser::{JxlAuxBox, JxlAuxBoxType};
pub use color::*;
pub use data_types::*;
pub use decoder::*;
pub use input::*;
pub use options::*;
pub use signature::*;

use crate::error::Result;
pub use crate::headers::image_metadata::Orientation;
pub use crate::image::JxlOutputBuffer;

/// Events emitted by [`JxlDecoder::process`] as decoding progresses.
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum Event {
    /// More input data is needed to continue decoding.
    /// `size_hint` is an estimate of the number of additional bytes required.
    NeedMoreInput { size_hint: usize },
    /// Basic image information and color profiles are available.
    BasicInfo,
    /// A visible frame's header and TOC have been parsed.
    FrameHeader,
    /// The current visible frame has finished decoding.
    FrameComplete { has_more_frames: bool },
    /// All frames and any trailing container boxes have been processed.
    Complete,
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
