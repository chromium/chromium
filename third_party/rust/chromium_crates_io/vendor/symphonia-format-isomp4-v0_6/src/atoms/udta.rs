// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use symphonia_core::meta::MetadataRevision;

use crate::atoms::{Atom, AtomHeader, AtomIterator, AtomType, MetaAtom, ReadAtom, Result};

/// User data atom.
#[allow(dead_code)]
#[derive(Debug)]
pub struct UdtaAtom {
    /// Metadata atom.
    pub meta: Option<MetaAtom>,
}

impl UdtaAtom {
    /// If metadata was read, consumes the metadata and returns it.
    pub fn take_metadata(&mut self) -> Option<MetadataRevision> {
        self.meta.as_mut().and_then(|meta| meta.take_metadata())
    }
}

impl Atom for UdtaAtom {
    #[allow(clippy::single_match)]
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        let mut meta = None;

        while let Some(header) = it.next_header()? {
            match header.atom_type {
                AtomType::Meta => {
                    meta = Some(it.read_atom::<MetaAtom>()?);
                }
                // TODO: Support older QuickTime-style user data lists. Need sample files.
                _ => (),
            }
        }

        Ok(UdtaAtom { meta })
    }
}
