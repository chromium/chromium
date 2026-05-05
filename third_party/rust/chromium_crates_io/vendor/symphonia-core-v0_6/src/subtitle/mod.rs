// Symphonia
// Copyright (c) 2019-2024 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The `subtitle` module provides primitives for working with text-based and rendered subtitles.

pub struct RawTextSubtitleBuffer {}
pub struct PlainTextSubtitleBuffer {}
pub struct RenderedSubtitleBuffer {}

#[non_exhaustive]
pub enum GenericSubtitleBufferRef<'a> {
    /// Raw, unparsed, subtitle text.
    RawText(&'a RawTextSubtitleBuffer),
    /// Plain text subtitle text.
    PlainText(&'a PlainTextSubtitleBuffer),
    /// Rendered subtitle picture buffer.
    Rendered(&'a RenderedSubtitleBuffer),
}
