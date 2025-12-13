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

pub use crate::image::JxlOutputBuffer;
pub use color::*;
pub use data_types::*;
pub use decoder::*;
pub use inner::*;
pub use input::*;
pub use options::*;
pub use signature::*;

use crate::headers::image_metadata::Orientation;

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
