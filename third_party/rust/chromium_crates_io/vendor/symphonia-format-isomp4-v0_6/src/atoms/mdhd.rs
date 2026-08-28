// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::num::NonZero;

use symphonia_core::errors::Error;

use crate::atoms::{Atom, AtomHeader, AtomIterator, ReadAtom, Result, decode_error};

fn parse_language(code: u16) -> String {
    // An ISO language code outside of these bounds is not valid.
    if code < 0x400 || code > 0x7fff {
        String::new()
    }
    else {
        let chars = [
            ((code >> 10) & 0x1f) as u8 + 0x60,
            ((code >> 5) & 0x1f) as u8 + 0x60,
            ((code >> 0) & 0x1f) as u8 + 0x60,
        ];

        String::from_utf8_lossy(&chars).to_string()
    }
}

/// Media header atom.
#[allow(dead_code)]
#[derive(Debug)]
pub struct MdhdAtom {
    /// Creation time.
    pub ctime: u64,
    /// Modification time.
    pub mtime: u64,
    /// Timescale.
    pub timescale: NonZero<u32>,
    /// Duration of the media in timescale units.
    pub duration: u64,
    /// Language.
    pub language: String,
}

impl Atom for MdhdAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        let (version, _) = it.read_extended_header()?;

        let mut mdhd = MdhdAtom {
            ctime: 0,
            mtime: 0,
            timescale: NonZero::new(1).expect("1 is non-zero"),
            duration: 0,
            language: String::new(),
        };

        match version {
            0 => {
                mdhd.ctime = u64::from(it.read_u32()?);
                mdhd.mtime = u64::from(it.read_u32()?);
                mdhd.timescale = NonZero::new(it.read_u32()?)
                    .ok_or(Error::DecodeError("isomp4 (mdhd): timescale is zero"))?;
                // 0xffff_ffff is a special case.
                mdhd.duration = match it.read_u32()? {
                    u32::MAX => u64::MAX,
                    duration => u64::from(duration),
                };
            }
            1 => {
                mdhd.ctime = it.read_u64()?;
                mdhd.mtime = it.read_u64()?;
                mdhd.timescale = NonZero::new(it.read_u32()?)
                    .ok_or(Error::DecodeError("isomp4 (mdhd): timescale is zero"))?;
                mdhd.duration = it.read_u64()?;
            }
            _ => {
                return decode_error("isomp4 (mdhd): invalid mdhd version");
            }
        }

        mdhd.language = parse_language(it.read_u16()?);

        // Quality
        let _ = it.read_u16()?;

        Ok(mdhd)
    }
}
