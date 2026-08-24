// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use symphonia_core::errors::Error;

use crate::atoms::{Atom, AtomHeader, AtomIterator, ReadAtom, Result};

use log::warn;

/// Handler type.
#[derive(Debug, PartialEq, Eq)]
pub enum HandlerType {
    /// Video handler.
    Video,
    /// Audio handler.
    Sound,
    /// Subtitle handler.
    Subtitle,
    /// Metadata handler.
    Metadata,
    /// Text handler.
    Text,
    /// Unknown handler type.
    Other([u8; 4]),
}

/// Handler atom.
#[allow(dead_code)]
#[derive(Debug)]
pub struct HdlrAtom {
    /// Handler type.
    pub handler_type: HandlerType,
    /// Human-readable handler name.
    pub name: String,
}

impl Atom for HdlrAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        /// The maximum size in bytes acceptable for a handler name.
        pub const MAX_HDLR_NAME_BYTES: usize = 4 * 1024;

        let (_, _) = it.read_extended_header()?;

        // Always 0 for MP4, but for Quicktime this contains the component type.
        let _ = it.read_quad_bytes()?;

        let handler_type = match &it.read_quad_bytes()? {
            b"vide" => HandlerType::Video,
            b"soun" => HandlerType::Sound,
            b"meta" => HandlerType::Metadata,
            b"subt" => HandlerType::Subtitle,
            b"text" => HandlerType::Text,
            hdlr => {
                warn!("unknown handler type '{}'", std::str::from_utf8(hdlr).unwrap_or("????"));
                HandlerType::Other(*hdlr)
            }
        };

        // These bytes are reserved for MP4, but for QuickTime they contain the component
        // manufacturer, flags, and flags mask.
        it.ignore_bytes(4 * 3)?;

        // Human readable UTF-8 string of the track type.
        let name = {
            let size = it
                .data_left()?
                .ok_or(Error::DecodeError("isomp4 (hdlr): expected atom size to be known"))?
                .min(MAX_HDLR_NAME_BYTES as u64);

            let buf = it.read_boxed_slice_exact(size as usize)?;
            String::from_utf8_lossy(&buf).to_string()
        };

        Ok(HdlrAtom { handler_type, name })
    }
}
