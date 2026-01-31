// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    bit_reader::BitReader,
    error::Error,
    headers::encodings::{self, *},
};
use jxl_macros::UnconditionalCoder;
use num_derive::FromPrimitive;

#[derive(UnconditionalCoder, Copy, Clone, PartialEq, Debug, FromPrimitive, Default)]
enum AspectRatio {
    #[default]
    Unknown = 0,
    Ratio1Over1 = 1,
    Ratio12Over10 = 2,
    Ratio4Over3 = 3,
    Ratio3Over2 = 4,
    Ratio16Over9 = 5,
    Ratio5Over4 = 6,
    Ratio2Over1 = 7,
}

#[derive(UnconditionalCoder, Debug, Clone, Default)]
#[validate]
pub struct Size {
    small: bool,
    #[condition(small)]
    #[coder(Bits(5) + 1)]
    ysize_div8: Option<u32>,
    #[condition(!small)]
    #[coder(1 + u2S(Bits(9), Bits(13), Bits(18), Bits(30)))]
    ysize: Option<u32>,
    #[coder(Bits(3))]
    ratio: AspectRatio,
    #[condition(small && ratio == AspectRatio::Unknown)]
    #[coder(Bits(5) + 1)]
    xsize_div8: Option<u32>,
    #[condition(!small && ratio == AspectRatio::Unknown)]
    #[coder(1 + u2S(Bits(9), Bits(13), Bits(18), Bits(30)))]
    xsize: Option<u32>,
}

#[derive(UnconditionalCoder, Debug, Clone)]
#[validate]
pub struct Preview {
    div8: bool,
    #[condition(div8)]
    #[coder(u2S(16, 32, Bits(5) + 1, Bits(9) + 33))]
    ysize_div8: Option<u32>,
    #[condition(!div8)]
    #[coder(1 + u2S(Bits(6), Bits(8) + 64, Bits(10) + 320, Bits(12) + 1344))]
    ysize: Option<u32>,
    #[coder(Bits(3))]
    ratio: AspectRatio,
    #[condition(div8 && ratio == AspectRatio::Unknown)]
    #[coder(u2S(16, 32, Bits(5) + 1, Bits(9) + 33))]
    xsize_div8: Option<u32>,
    #[condition(!div8 && ratio == AspectRatio::Unknown)]
    #[coder(1 + u2S(Bits(6), Bits(8) + 64, Bits(10) + 320, Bits(12) + 1344))]
    xsize: Option<u32>,
}

fn map_aspect_ratio<T: Fn() -> u64>(ysize: u32, ratio: AspectRatio, fallback: T) -> u64 {
    match ratio {
        AspectRatio::Unknown => fallback(),
        AspectRatio::Ratio1Over1 => ysize as u64,
        AspectRatio::Ratio12Over10 => ysize as u64 * 12 / 10,
        AspectRatio::Ratio4Over3 => ysize as u64 * 4 / 3,
        AspectRatio::Ratio3Over2 => ysize as u64 * 3 / 2,
        AspectRatio::Ratio16Over9 => ysize as u64 * 16 / 9,
        AspectRatio::Ratio5Over4 => ysize as u64 * 5 / 4,
        AspectRatio::Ratio2Over1 => ysize as u64 * 2,
    }
}

impl Size {
    pub fn ysize(&self) -> u32 {
        if self.small {
            self.ysize_div8.unwrap() * 8
        } else {
            self.ysize.unwrap()
        }
    }

    fn compute_xsize(&self) -> Result<u32, Error> {
        let x = map_aspect_ratio(self.ysize(), self.ratio, /* fallback */ || {
            if self.small {
                self.xsize_div8.unwrap() as u64 * 8
            } else {
                self.xsize.unwrap() as u64
            }
        });
        u32::try_from(x).map_err(|_| Error::ImageDimensionTooLarge(x))
    }

    fn check(&self, _: &encodings::Empty) -> Result<(), Error> {
        self.compute_xsize()?;
        Ok(())
    }

    pub fn xsize(&self) -> u32 {
        // Validation happens during struct decoding via #[validate].
        // If we reach here, compute_xsize() is guaranteed to succeed.
        self.compute_xsize().unwrap()
    }
}

impl Preview {
    pub fn ysize(&self) -> u32 {
        if self.div8 {
            self.ysize_div8.unwrap() * 8
        } else {
            self.ysize.unwrap()
        }
    }

    fn compute_xsize(&self) -> Result<u32, Error> {
        let x = map_aspect_ratio(self.ysize(), self.ratio, /* fallback */ || {
            if self.div8 {
                self.xsize_div8.unwrap() as u64 * 8
            } else {
                self.xsize.unwrap() as u64
            }
        });
        u32::try_from(x).map_err(|_| Error::ImageDimensionTooLarge(x))
    }

    fn check(&self, _: &encodings::Empty) -> Result<(), Error> {
        self.compute_xsize()?;
        Ok(())
    }

    pub fn xsize(&self) -> u32 {
        // Validation happens during struct decoding via #[validate].
        // If we reach here, compute_xsize() is guaranteed to succeed.
        self.compute_xsize().unwrap()
    }
}
