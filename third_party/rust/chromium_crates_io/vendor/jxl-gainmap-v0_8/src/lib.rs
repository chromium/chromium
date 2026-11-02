// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

//! Helpers for inspecting a JPEG XL `jhgm` gain map payload.

use jxl::api::JxlColorProfile;
use jxl::error::{Error, Result};

/// A view of the fields in a `jhgm` gain map payload.
///
/// The color profile is decoded during parsing and owns any ICC bytes.
#[derive(Clone, Debug, PartialEq)]
pub struct JxlGainMap<'a> {
    /// The `jhgm` version.
    pub version: u8,
    /// ISO 21496-1 binary gain map metadata.
    pub metadata: &'a [u8],
    /// The color profile for the alternate image, or `None` to reuse the baseline.
    pub color_profile: Option<JxlColorProfile>,
    /// The gain map image, encoded as a JPEG XL image.
    pub gain_map: &'a [u8],
}

impl<'a> JxlGainMap<'a> {
    /// Parses a complete, uncompressed `jhgm` payload.
    pub fn parse(mut data: &'a [u8]) -> Result<Self> {
        let version = take_first(&mut data)?;
        if version != 0 {
            return Err(Error::InvalidEnum(
                version as u32,
                "jhgm version".to_owned(),
            ));
        }

        let metadata_length = u16::from_be_bytes(take_const(&mut data)?) as usize;
        let metadata = take(&mut data, metadata_length)?;

        let color_length = take_first(&mut data)? as usize;
        let color_encoding = take(&mut data, color_length)?;

        let icc_length = u32::from_be_bytes(take_const(&mut data)?) as usize;
        let compressed_icc = take(&mut data, icc_length)?;

        let color_profile = if color_encoding.is_empty() && compressed_icc.is_empty() {
            None
        } else {
            Some(JxlColorProfile::from_raw_bytes(
                color_encoding,
                compressed_icc,
            )?)
        };

        Ok(Self {
            version,
            metadata,
            color_profile,
            gain_map: data,
        })
    }
}

#[inline]
fn take<'a>(data: &mut &'a [u8], length: usize) -> Result<&'a [u8]> {
    data.split_off(..length).ok_or(Error::SectionTooShort)
}

#[inline]
fn take_first(data: &mut &[u8]) -> Result<u8> {
    let (b, rem) = data.split_first().ok_or(Error::SectionTooShort)?;
    *data = rem;
    Ok(*b)
}

#[inline]
fn take_const<const N: usize>(data: &mut &[u8]) -> Result<[u8; N]> {
    let (chunk, rem) = data
        .split_first_chunk::<N>()
        .ok_or(Error::SectionTooShort)?;
    *data = rem;
    Ok(*chunk)
}
