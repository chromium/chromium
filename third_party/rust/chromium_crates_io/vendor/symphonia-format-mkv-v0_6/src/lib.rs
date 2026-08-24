// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![warn(rust_2018_idioms)]
#![forbid(unsafe_code)]
// The following lints are allowed in all Symphonia crates. Please see clippy.toml for their
// justification.
#![allow(clippy::comparison_chain)]
#![allow(clippy::excessive_precision)]
#![allow(clippy::identity_op)]
#![allow(clippy::manual_range_contains)]

mod codecs;
mod demuxer;
mod ebml;
mod lacing;
mod schema;
mod segment;
mod tags;

pub use crate::demuxer::MkvReader;

pub mod sub_fields {
    //! Key name constants for sub-fields of MKV tags and chapters.
    //!
    //! For the exact meaning of these fields, and the format of their values, please consult the
    //! official Matroska specification.

    pub const TAG_LANGUAGE: &str = "LANGUAGE";
    pub const TAG_LANGUAGE_BCP47: &str = "LANGUAGE_BCP47";

    pub const CHAPTER_TITLE_COUNTRY: &str = "CHAPTER_TITLE_COUNTRY";
    pub const CHAPTER_TITLE_LANGUAGE: &str = "CHAPTER_TITLE_LANGUAGE";
    pub const CHAPTER_TITLE_LANGUAGE_BCP47: &str = "CHAPTER_TITLE_LANGUAGE_BCP47";

    pub const EDITION_TITLE_LANGUAGE_BCP47: &str = "EDITION_TITLE_LANGUAGE_BCP47";
}
