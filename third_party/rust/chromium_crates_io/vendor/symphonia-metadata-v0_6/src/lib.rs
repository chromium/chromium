// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! This crate implements a collection of metadata readers for standalone metadata formats, and read
//! functions for metadata formats that are embedded into another container.
//!
//! # Standalone Metadata Formats
//!
//! A standalone metadata format is one that exists independent of the media container.
//!
//! This crate implements metadata readers (an object implementing the
//! [`MetadataReader`](symphonia_core::meta::MetadataReader) trait) for these metadata formats.
//! A metadata reader may be registered with a probe for automatic detection. Each major standalone
//! metadata format reader is implemented in a separate module.
//!
//! # Embedded Metadata Formats
//!
//! An embedded metadata format is one that is embedded into the media container. This crate
//! implements reading or parsing functions for these metadata formats in the [`embedded`] module.

#![warn(rust_2018_idioms)]
#![forbid(unsafe_code)]
// The following lints are allowed in all Symphonia crates. Please see clippy.toml for their
// justification.
#![allow(clippy::comparison_chain)]
#![allow(clippy::excessive_precision)]
#![allow(clippy::identity_op)]
#![allow(clippy::manual_range_contains)]

#[cfg(feature = "ape")]
pub mod ape;
#[cfg(feature = "id3v1")]
pub mod id3v1;
#[cfg(feature = "id3v2")]
pub mod id3v2;

pub mod embedded;
pub mod utils;
