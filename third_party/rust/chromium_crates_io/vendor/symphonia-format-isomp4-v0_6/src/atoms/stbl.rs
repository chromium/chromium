// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::atoms::{
    Atom, AtomHeader, AtomIterator, AtomType, Co64Atom, ReadAtom, Result, StcoAtom, StscAtom,
    StsdAtom, StszAtom, SttsAtom, decode_error,
};

use log::{debug, warn};

/// Sample table atom.
#[allow(dead_code)]
#[derive(Debug)]
pub struct StblAtom {
    pub stsd: StsdAtom,
    pub stts: SttsAtom,
    pub stsc: StscAtom,
    pub stsz: StszAtom,
    pub stco: Option<StcoAtom>,
    pub co64: Option<Co64Atom>,
}

impl Atom for StblAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        let mut stsd = None;
        let mut stts = None;
        let mut stsc = None;
        let mut stsz = None;
        let mut stco = None;
        let mut co64 = None;

        while let Some(header) = it.next_header()? {
            match header.atom_type {
                AtomType::SampleDescription => {
                    stsd = Some(it.read_atom::<StsdAtom>()?);
                }
                AtomType::TimeToSample => {
                    stts = Some(it.read_atom::<SttsAtom>()?);
                }
                AtomType::CompositionTimeToSample => {
                    // Composition time to sample atom is only required for video.
                    debug!("ignoring ctts atom.");
                }
                AtomType::SyncSample => {
                    // Sync sample atom is only required for video.
                    debug!("ignoring stss atom.");
                }
                AtomType::SampleToChunk => {
                    stsc = Some(it.read_atom::<StscAtom>()?);
                }
                AtomType::SampleSize => {
                    stsz = Some(it.read_atom::<StszAtom>()?);
                }
                AtomType::ChunkOffset => {
                    stco = Some(it.read_atom::<StcoAtom>()?);
                }
                AtomType::ChunkOffset64 => {
                    co64 = Some(it.read_atom::<Co64Atom>()?);
                }
                _ => (),
            }
        }

        if stsd.is_none() {
            return decode_error("isomp4 (stbl): missing stsd atom");
        }

        if stts.is_none() {
            return decode_error("isomp4 (stbl): missing stts atom");
        }

        if stsc.is_none() {
            return decode_error("isomp4 (stbl): missing stsc atom");
        }

        if stsz.is_none() {
            return decode_error("isomp4 (stbl): missing stsz atom");
        }

        if stco.is_none() && co64.is_none() {
            // This is a spec. violation, but some m4a files appear to lack these atoms.
            warn!("missing stco or co64 atom");
        }

        Ok(StblAtom {
            stsd: stsd.unwrap(),
            stts: stts.unwrap(),
            stsc: stsc.unwrap(),
            stsz: stsz.unwrap(),
            stco,
            co64,
        })
    }
}
