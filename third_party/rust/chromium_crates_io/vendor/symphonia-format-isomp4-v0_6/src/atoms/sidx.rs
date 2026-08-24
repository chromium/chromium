// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::num::NonZero;

use symphonia_core::errors::Error;

use crate::atoms::{Atom, AtomHeader, AtomIterator, ReadAtom, Result, decode_error};

#[derive(Debug)]
pub enum ReferenceType {
    Segment,
    Media,
}

#[allow(dead_code)]
#[derive(Debug)]
pub struct SidxReference {
    pub reference_type: ReferenceType,
    pub reference_size: u32,
    pub subsegment_duration: u32,
    // pub starts_with_sap: bool,
    // pub sap_type: u8,
    // pub sap_delta_time: u32,
}

/// Segment index atom.
#[allow(dead_code)]
#[derive(Debug)]
pub struct SidxAtom {
    pub reference_id: u32,
    pub timescale: NonZero<u32>,
    pub earliest_pts: u64,
    pub first_offset: u64,
    pub references: Vec<SidxReference>,
    pub total_duration: u64,
}

impl Atom for SidxAtom {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, header: &AtomHeader) -> Result<Self> {
        let (version, _) = it.read_extended_header()?;

        let reference_id = it.read_u32()?;
        let timescale = NonZero::new(it.read_u32()?)
            .ok_or(Error::DecodeError("isomp4 (sidx): timescale is zero"))?;

        // The anchor point for segment offsets is the first byte after this atom.
        let anchor = header
            .size()
            .map(|atom_len| header.pos() + atom_len.get())
            .ok_or(Error::DecodeError("isomp4 (sidx): expected atom size to be known"))?;

        let (earliest_pts, first_offset) = match version {
            0 => (u64::from(it.read_u32()?), anchor + u64::from(it.read_u32()?)),
            1 => (it.read_u64()?, anchor + it.read_u64()?),
            _ => return decode_error("isomp4 (sidx): invalid version"),
        };

        let _reserved = it.read_u16()?;
        let reference_count = it.read_u16()?;

        let mut references = Vec::new();
        let mut total_duration = 0u64;

        for _ in 0..reference_count {
            let reference = it.read_u32()?;
            let subsegment_duration = it.read_u32()?;

            total_duration = total_duration
                .checked_add(u64::from(subsegment_duration))
                .ok_or(Error::DecodeError("isomp4 (sidx): total duration overflow"))?;

            let reference_type = match (reference & 0x8000_0000) != 0 {
                false => ReferenceType::Media,
                true => ReferenceType::Segment,
            };

            let reference_size = reference & !0x8000_0000;

            // Ignore SAP
            let _ = it.read_u32()?;

            references.push(SidxReference { reference_type, reference_size, subsegment_duration });
        }

        Ok(SidxAtom {
            reference_id,
            timescale,
            earliest_pts,
            first_offset,
            references,
            total_duration,
        })
    }
}
