// Symphonia
// Copyright (c) 2019-2024 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The `video` module provides primitives for working with video.

pub struct YuvVideoBuffer {}
pub struct RgbVideoBuffer {}

#[non_exhaustive]
pub enum GenericVideoBufferRef<'a> {
    /// RGB encoded video frame.
    Rgb(&'a RgbVideoBuffer),
    /// YUV encoded video frame.
    Yuv(&'a YuvVideoBuffer),
}
