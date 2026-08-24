// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::atoms::{
    Atom, AtomHeader, AtomIterator, AtomType, MehdAtom, ReadAtom, Result, TrexAtom,
};

/// Movie extends atom.
#[allow(dead_code)]
#[derive(Debug)]
pub struct MvexAtom {
    /// Movie extends header, optional.
    pub mehd: Option<MehdAtom>,
    /// Track extends box, one per track.
    pub trexs: Vec<TrexAtom>,
}

impl Atom for MvexAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        let mut mehd = None;
        let mut trexs = Vec::new();

        while let Some(header) = it.next_header()? {
            match header.atom_type {
                AtomType::MovieExtendsHeader => {
                    mehd = Some(it.read_atom::<MehdAtom>()?);
                }
                AtomType::TrackExtends => {
                    let trex = it.read_atom::<TrexAtom>()?;
                    trexs.push(trex);
                }
                _ => (),
            }
        }

        Ok(MvexAtom { mehd, trexs })
    }
}
