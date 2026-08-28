// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::atoms::{
    Atom, AtomHeader, AtomIterator, AtomType, MfhdAtom, ReadAtom, Result, TrafAtom, decode_error,
};

/// Movie fragment atom.
#[allow(dead_code)]
#[derive(Debug)]
pub struct MoofAtom {
    /// The position of the first byte of this moof atom. This is used as the anchor point for the
    /// subsequent track atoms.
    pub moof_base_pos: u64,
    /// Movie fragment header.
    pub mfhd: MfhdAtom,
    /// Track fragments.
    pub trafs: Vec<TrafAtom>,
}

impl Atom for MoofAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, header: &AtomHeader) -> Result<Self> {
        let mut mfhd = None;
        let mut trafs = Vec::new();

        while let Some(header) = it.next_header()? {
            match header.atom_type {
                AtomType::MovieFragmentHeader => {
                    mfhd = Some(it.read_atom::<MfhdAtom>()?);
                }
                AtomType::TrackFragment => {
                    let traf = it.read_atom::<TrafAtom>()?;
                    trafs.push(traf);
                }
                _ => (),
            }
        }

        let Some(mfhd) = mfhd
        else {
            return decode_error("isomp4 (moof): missing mfhd atom");
        };

        // The position of the first byte of the entire moof atom.
        let moof_base_pos = header.pos();

        Ok(MoofAtom { moof_base_pos, mfhd, trafs })
    }
}
