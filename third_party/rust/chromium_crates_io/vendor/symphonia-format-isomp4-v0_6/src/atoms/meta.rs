// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::fmt::Debug;

use symphonia_core::meta::MetadataRevision;

use crate::atoms::{Atom, AtomHeader, AtomIterator, AtomType, IlstAtom, ReadAtom, Result};

/// User data atom.
#[allow(dead_code)]
pub struct MetaAtom {
    /// Metadata revision.
    pub metadata: Option<MetadataRevision>,
}

impl Debug for MetaAtom {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "(redacted)")
    }
}

impl MetaAtom {
    /// If metadata was read, consumes the metadata and returns it.
    pub fn take_metadata(&mut self) -> Option<MetadataRevision> {
        self.metadata.take()
    }
}

impl Atom for MetaAtom {
    #[allow(clippy::single_match)]
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        let (_, _) = it.read_extended_header()?;

        let mut metadata = None;

        while let Some(header) = it.next_header()? {
            match header.atom_type {
                AtomType::MetaList => {
                    metadata = Some(it.read_atom::<IlstAtom>()?.metadata);
                }
                // TODO: Support country and language lists.
                _ => (),
            }
        }

        Ok(MetaAtom { metadata })
    }
}
