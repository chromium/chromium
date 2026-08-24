// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::atoms::{Atom, AtomHeader, AtomIterator, AtomType, ElstAtom, ReadAtom, Result};

/// Edits atom.
#[allow(dead_code)]
#[derive(Debug)]
pub struct EdtsAtom {
    pub elst: Option<ElstAtom>,
}

impl Atom for EdtsAtom {
    #[allow(clippy::single_match)]
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        let mut elst = None;

        while let Some(header) = it.next_header()? {
            match header.atom_type {
                AtomType::EditList => {
                    elst = Some(it.read_atom::<ElstAtom>()?);
                }
                _ => (),
            }
        }

        Ok(EdtsAtom { elst })
    }
}
