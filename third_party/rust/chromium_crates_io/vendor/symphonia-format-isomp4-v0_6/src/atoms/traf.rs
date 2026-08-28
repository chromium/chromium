// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::atoms::{
    Atom, AtomHeader, AtomIterator, AtomType, ReadAtom, Result, TfhdAtom, TrunAtom, decode_error,
};

/// Track fragment atom.
#[allow(dead_code)]
#[derive(Debug)]
pub struct TrafAtom {
    /// Track fragment header.
    pub tfhd: TfhdAtom,
    /// Track fragment sample runs.
    pub truns: Vec<TrunAtom>,
    /// The total number of samples in this track fragment.
    pub total_sample_count: u32,
}

impl Atom for TrafAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        let mut tfhd = None;
        let mut truns = Vec::new();

        let mut total_sample_count = 0;

        while let Some(header) = it.next_header()? {
            match header.atom_type {
                AtomType::TrackFragmentHeader => {
                    tfhd = Some(it.read_atom::<TfhdAtom>()?);
                }
                AtomType::TrackFragmentRun => {
                    let trun = it.read_atom::<TrunAtom>()?;

                    // Increment the total sample count.
                    total_sample_count += trun.sample_count;

                    truns.push(trun);
                }
                _ => (),
            }
        }

        // Tfhd is mandatory.
        let Some(tfhd) = tfhd
        else {
            return decode_error("isomp4 (traf): missing tfhd atom");
        };

        Ok(TrafAtom { tfhd, truns, total_sample_count })
    }
}
