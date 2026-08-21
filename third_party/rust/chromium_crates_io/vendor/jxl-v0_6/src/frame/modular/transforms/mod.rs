// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use num_derive::FromPrimitive;

pub(super) mod apply_local;
pub(super) mod meta_apply;
mod palette;
mod rct;
mod squeeze;
pub(super) mod step;

#[derive(Debug, FromPrimitive, PartialEq, Clone, Copy)]
pub enum RctPermutation {
    Rgb = 0,
    Gbr = 1,
    Brg = 2,
    Rbg = 3,
    Grb = 4,
    Bgr = 5,
}

#[derive(Debug, FromPrimitive, PartialEq, Clone, Copy)]
pub enum RctOp {
    Noop = 0,
    AddFirstToThird = 1,
    AddFirstToSecond = 2,
    AddFirstToSecondAndThird = 3,
    AddAvgToSecond = 4,
    AddFirstToThirdAndAvgToSecond = 5,
    YCoCg = 6,
}
