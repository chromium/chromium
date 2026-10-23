// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![deny(unsafe_code)]
pub mod api;
mod bit_reader;
mod color;
mod entropy_coding;
pub mod error;
mod features;
mod frame;
mod headers;
mod icc;
pub mod image;
mod render;
mod util;

#[cfg(test)]
mod tests;

// TODO: Move these to a more appropriate location.
const GROUP_DIM: usize = 256;
const BLOCK_DIM: usize = 8;
const BLOCK_SIZE: usize = BLOCK_DIM * BLOCK_DIM;
#[allow(clippy::excessive_precision)]
const MIN_SIGMA: f32 = -3.90524291751269967465540850526868;
