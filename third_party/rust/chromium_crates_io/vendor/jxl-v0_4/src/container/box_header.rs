// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
//
// Originally written for jxl-oxide.

use crate::error::Error;

/// Box header used in JPEG XL containers.
#[derive(Debug, Clone)]
pub struct ContainerBoxHeader {
    ty: ContainerBoxType,
    box_size: Option<u64>,
    is_last: bool,
}

pub enum HeaderParseResult {
    Done {
        header: ContainerBoxHeader,
        header_size: usize,
    },
    NeedMoreData,
}

impl ContainerBoxHeader {
    pub(super) fn parse(buf: &[u8]) -> Result<HeaderParseResult, Error> {
        let (tbox, box_size, header_size) = match *buf {
            [
                0,
                0,
                0,
                1,
                t0,
                t1,
                t2,
                t3,
                s0,
                s1,
                s2,
                s3,
                s4,
                s5,
                s6,
                s7,
                ..,
            ] => {
                let xlbox = u64::from_be_bytes([s0, s1, s2, s3, s4, s5, s6, s7]);
                let tbox = ContainerBoxType([t0, t1, t2, t3]);
                let xlbox = xlbox.checked_sub(16).ok_or(Error::InvalidBox)?;
                (tbox, Some(xlbox), 16)
            }
            [s0, s1, s2, s3, t0, t1, t2, t3, ..] => {
                let sbox = u32::from_be_bytes([s0, s1, s2, s3]);
                let tbox = ContainerBoxType([t0, t1, t2, t3]);
                let sbox = if sbox == 0 {
                    None
                } else if let Some(sbox) = sbox.checked_sub(8) {
                    Some(sbox as u64)
                } else {
                    return Err(Error::InvalidBox);
                };
                (tbox, sbox, 8)
            }
            _ => return Ok(HeaderParseResult::NeedMoreData),
        };
        let is_last = box_size.is_none();

        let header = Self {
            ty: tbox,
            box_size,
            is_last,
        };
        Ok(HeaderParseResult::Done {
            header,
            header_size,
        })
    }
}

impl ContainerBoxHeader {
    #[inline]
    pub fn box_type(&self) -> ContainerBoxType {
        self.ty
    }

    #[inline]
    pub fn box_size(&self) -> Option<u64> {
        self.box_size
    }

    #[inline]
    pub fn is_last(&self) -> bool {
        self.is_last
    }
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub struct ContainerBoxType(pub [u8; 4]);

impl ContainerBoxType {
    pub const JXL: Self = Self(*b"JXL ");
    pub const FILE_TYPE: Self = Self(*b"ftyp");
    pub const JXL_LEVEL: Self = Self(*b"jxll");
    pub const JUMBF: Self = Self(*b"jumb");
    pub const EXIF: Self = Self(*b"Exif");
    pub const XML: Self = Self(*b"xml ");
    pub const BROTLI_COMPRESSED: Self = Self(*b"brob");
    pub const FRAME_INDEX: Self = Self(*b"jxli");
    pub const CODESTREAM: Self = Self(*b"jxlc");
    pub const PARTIAL_CODESTREAM: Self = Self(*b"jxlp");
    pub const JPEG_RECONSTRUCTION: Self = Self(*b"jbrd");
}
