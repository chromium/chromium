// Copyright 2012-2026 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) struct WidthInfo(u16);

#[path = "gen/width_info.rs"]
mod gen;

const LIGATURE_TRANSPARENT_MASK: u16 = 0b0010_0000_0000_0000;

impl WidthInfo {
    // Additional constants are in the `gen` module.

    /// Whether this width mode is ligature_transparent
    /// (has 5th MSB set.)
    pub(crate) fn is_ligature_transparent(self) -> bool {
        (self.0 & 0b0000_1000_0000_0000) == 0b0000_1000_0000_0000
    }

    /// Sets 6th MSB.
    pub(crate) fn set_zwj_bit(self) -> Self {
        Self(self.0 | 0b0000_0100_0000_0000)
    }

    /// Has top bit set
    pub(crate) fn is_emoji_presentation(self) -> bool {
        (self.0 & WidthInfo::VARIATION_SELECTOR_16.0) == WidthInfo::VARIATION_SELECTOR_16.0
    }

    pub(crate) fn is_zwj_emoji_presentation(self) -> bool {
        (self.0 & 0b1011_0000_0000_0000) == 0b1001_0000_0000_0000
    }

    /// Set top bit
    pub(crate) fn set_emoji_presentation(self) -> Self {
        if (self.0 & LIGATURE_TRANSPARENT_MASK) == LIGATURE_TRANSPARENT_MASK
            || (self.0 & 0b1001_0000_0000_0000) == 0b0001_0000_0000_0000
        {
            Self(
                self.0
                    | WidthInfo::VARIATION_SELECTOR_16.0
                        & !WidthInfo::VARIATION_SELECTOR_15.0
                        & !WidthInfo::VARIATION_SELECTOR_1_2_OR_3.0,
            )
        } else {
            Self::VARIATION_SELECTOR_16
        }
    }

    /// Clear top bit
    pub(crate) fn unset_emoji_presentation(self) -> Self {
        if (self.0 & LIGATURE_TRANSPARENT_MASK) == LIGATURE_TRANSPARENT_MASK {
            Self(self.0 & !WidthInfo::VARIATION_SELECTOR_16.0)
        } else {
            Self::DEFAULT
        }
    }

    /// Has 2nd bit set
    pub(crate) fn is_text_presentation(self) -> bool {
        (self.0 & WidthInfo::VARIATION_SELECTOR_15.0) == WidthInfo::VARIATION_SELECTOR_15.0
    }

    /// Set 2nd bit
    pub(crate) fn set_text_presentation(self) -> Self {
        if (self.0 & LIGATURE_TRANSPARENT_MASK) == LIGATURE_TRANSPARENT_MASK {
            Self(
                self.0
                    | WidthInfo::VARIATION_SELECTOR_15.0
                        & !WidthInfo::VARIATION_SELECTOR_16.0
                        & !WidthInfo::VARIATION_SELECTOR_1_2_OR_3.0,
            )
        } else {
            Self(WidthInfo::VARIATION_SELECTOR_15.0)
        }
    }

    /// Clear 2nd bit
    pub(crate) fn unset_text_presentation(self) -> Self {
        Self(self.0 & !WidthInfo::VARIATION_SELECTOR_15.0)
    }

    /// Has 7th bit set
    pub(crate) fn is_vs1_2_3(self) -> bool {
        (self.0 & WidthInfo::VARIATION_SELECTOR_1_2_OR_3.0)
            == WidthInfo::VARIATION_SELECTOR_1_2_OR_3.0
    }

    /// Set 7th bit
    pub(crate) fn set_vs1_2_3(self) -> Self {
        if (self.0 & LIGATURE_TRANSPARENT_MASK) == LIGATURE_TRANSPARENT_MASK {
            Self(
                self.0
                    | WidthInfo::VARIATION_SELECTOR_1_2_OR_3.0
                        & !WidthInfo::VARIATION_SELECTOR_15.0
                        & !WidthInfo::VARIATION_SELECTOR_16.0,
            )
        } else {
            Self(WidthInfo::VARIATION_SELECTOR_1_2_OR_3.0)
        }
    }

    /// Clear 7th bit
    pub(crate) fn unset_vs1_2_3(self) -> Self {
        Self(self.0 & !WidthInfo::VARIATION_SELECTOR_1_2_OR_3.0)
    }
}
