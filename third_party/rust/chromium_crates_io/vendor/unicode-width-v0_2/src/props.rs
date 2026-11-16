// Copyright 2012-2026 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::tables::*;
use core::cmp::Ordering;

#[path = "gen/props.rs"]
mod gen;

pub use gen::*;

/// Whether this character is a zero-width character with
/// `Joining_Type=Transparent`. Used by the Alef-Lamed ligatures.
/// See also [`is_ligature_transparent`], a near-subset of this (only ZWJ is excepted)
/// which is transparent for non-Arabic ligatures.
pub(crate) fn is_transparent_zero_width(c: char) -> bool {
    if crate::lookup::lookup_width(c).0 != 0 {
        // Not zero-width
        false
    } else {
        let cp: u32 = c.into();
        NON_TRANSPARENT_ZERO_WIDTHS
            .binary_search_by(|&(lo, hi)| {
                let lo = u32::from_le_bytes([lo[0], lo[1], lo[2], 0]);
                let hi = u32::from_le_bytes([hi[0], hi[1], hi[2], 0]);
                if cp < lo {
                    Ordering::Greater
                } else if cp > hi {
                    Ordering::Less
                } else {
                    Ordering::Equal
                }
            })
            .is_err()
    }
}

/// Whether this character is transparent wrt the effect of
/// U+0338 COMBINING LONG SOLIDUS OVERLAY
/// on its base character.
#[cfg(feature = "cjk")]
pub fn is_solidus_transparent(c: char) -> bool {
    let cp: u32 = c.into();
    is_ligature_transparent(c)
        || SOLIDUS_TRANSPARENT
            .binary_search_by(|&(lo, hi)| {
                let lo = u32::from_le_bytes([lo[0], lo[1], lo[2], 0]);
                let hi = u32::from_le_bytes([hi[0], hi[1], hi[2], 0]);
                if cp < lo {
                    Ordering::Greater
                } else if cp > hi {
                    Ordering::Less
                } else {
                    Ordering::Equal
                }
            })
            .is_ok()
}
