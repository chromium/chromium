// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Reading and parsing of metadata formats that are embedded into other containers.

#[cfg(feature = "flac")]
pub mod flac;
#[cfg(any(feature = "riff-id3", feature = "riff-info"))]
pub mod riff;
#[cfg(feature = "vorbis")]
pub mod vorbis;
