// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::atoms::{Atom, AtomHeader, AtomIterator, ReadAtom, Result};

#[allow(dead_code)]
#[derive(Debug)]
pub struct StssAtom {}

impl Atom for StssAtom {
    fn read<R: ReadAtom>(_reader: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        todo!()
    }
}
