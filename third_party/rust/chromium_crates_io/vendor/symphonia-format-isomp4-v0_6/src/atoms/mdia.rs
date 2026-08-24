// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::atoms::{
    Atom, AtomHeader, AtomIterator, AtomType, HdlrAtom, MdhdAtom, MinfAtom, ReadAtom, Result,
    decode_error,
};

#[allow(dead_code)]
#[derive(Debug)]
pub struct MdiaAtom {
    pub mdhd: MdhdAtom,
    pub hdlr: HdlrAtom,
    pub minf: MinfAtom,
}

impl Atom for MdiaAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, _header: &AtomHeader) -> Result<Self> {
        let mut mdhd = None;
        let mut hdlr = None;
        let mut minf = None;

        while let Some(header) = it.next_header()? {
            match header.atom_type {
                AtomType::MediaHeader => {
                    mdhd = Some(it.read_atom::<MdhdAtom>()?);
                }
                AtomType::Handler => {
                    hdlr = Some(it.read_atom::<HdlrAtom>()?);
                }
                AtomType::MediaInfo => {
                    minf = Some(it.read_atom::<MinfAtom>()?);
                }
                _ => (),
            }
        }

        if mdhd.is_none() {
            return decode_error("isomp4 (mdia): missing mdhd atom");
        }

        if hdlr.is_none() {
            return decode_error("isomp4 (mdia): missing hdlr atom");
        }

        if minf.is_none() {
            return decode_error("isomp4 (mdia): missing minf atom");
        }

        Ok(MdiaAtom { mdhd: mdhd.unwrap(), hdlr: hdlr.unwrap(), minf: minf.unwrap() })
    }
}
