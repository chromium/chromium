// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::atoms::{Atom, AtomHeader, AtomIterator, ReadAtom, Result};

/// Composition time atom.
#[allow(dead_code)]
#[derive(Debug)]
pub struct CttsAtom {}

impl Atom for CttsAtom {
    fn read<R: ReadAtom>(_it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        todo!()
    }
}
