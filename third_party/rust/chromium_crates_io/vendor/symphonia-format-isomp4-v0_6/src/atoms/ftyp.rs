// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use symphonia_core::common::FourCc;
use symphonia_core::errors::Error;

use crate::atoms::{Atom, AtomHeader, AtomIterator, ReadAtom, Result};
use crate::atoms::{decode_error, limits::*};

/// File type atom.
#[allow(dead_code)]
#[derive(Debug)]
pub struct FtypAtom {
    pub major: FourCc,
    pub minor: [u8; 4],
    pub compatible: Vec<FourCc>,
}

impl Atom for FtypAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, header: &AtomHeader) -> Result<Self> {
        // The Ftyp atom must be have a data length that is known, and it must be a multiple of 4
        // since it only stores FourCCs.
        let data_len = header
            .data_size()
            .ok_or(Error::DecodeError("isomp4 (ftyp): expected atom size to be known"))?;

        if data_len < 8 || data_len & 0x3 != 0 {
            return decode_error("isomp4 (ftyp): invalid data length");
        }

        // Major
        let major = FourCc::try_new(it.read_quad_bytes()?)
            .ok_or(Error::DecodeError("isomp4 (ftyp): major brand contains non-ASCII bytes"))?;

        // Minor
        let minor = it.read_quad_bytes()?;

        // The remainder of the Ftyp atom contains the FourCCs of compatible brands.
        let n_brands = (data_len - 8) / 4;

        // Limit the maximum initial capacity to prevent malicious files from using all the
        // available memory.
        let mut compatible = Vec::with_capacity(MAX_TABLE_INITIAL_CAPACITY.min(n_brands as usize));

        for _ in 0..n_brands {
            let brand = it.read_quad_bytes()?;
            if let Some(fourcc) = FourCc::try_new(brand) {
                compatible.push(fourcc);
            }
        }

        Ok(FtypAtom { major, minor, compatible })
    }
}
