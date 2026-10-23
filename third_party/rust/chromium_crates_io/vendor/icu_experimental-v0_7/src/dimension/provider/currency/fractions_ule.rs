// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::fractions::{FractionInfo, Rounding};
use zerovec::{
    maps::ZeroMapKV,
    ule::{AsULE, RawBytesULE},
};

/// Marker value indicating `None` for `cash_digits` and `cash_rounding` fields.
const NONE_MARKER: u8 = 15;

/// ULE type for [`FractionInfo`] - packed into 4 bytes.
///
/// Data layout:
/// - Byte 0, bits 0-3: `digits` (lower nibble)
/// - Byte 0, bits 4-7: `rounding` (upper nibble)
/// - Byte 1, bits 0-3: `cash_digits` (lower nibble, 15 = None)
/// - Byte 1, bits 4-7: `cash_rounding` (upper nibble, 15 = None)
type FractionInfoULE = RawBytesULE<2>;

impl AsULE for FractionInfo {
    type ULE = FractionInfoULE;

    #[inline]
    fn to_unaligned(self) -> Self::ULE {
        debug_assert!(self.digits < 16);

        let cash_digits = self.cash_digits.unwrap_or(NONE_MARKER);
        debug_assert!(cash_digits < 16);

        let rounding = match self.rounding {
            Rounding::R1 => 0,
            Rounding::R5 => 1,
            Rounding::R20 => 2,
            Rounding::R50 => 3,
        };

        let cash_rounding = match self.cash_rounding {
            Some(Rounding::R1) => 0,
            Some(Rounding::R5) => 1,
            Some(Rounding::R20) => 2,
            Some(Rounding::R50) => 3,
            None => 15,
        };

        RawBytesULE([
            (self.digits & 0x0f) | (rounding << 4),
            (cash_digits & 0x0f) | (cash_rounding << 4),
        ])
    }

    #[inline]
    fn from_unaligned(unaligned: Self::ULE) -> Self {
        let [b0, b1] = unaligned.0;

        let digits = b0 & 0x0f;
        let rounding = b0 >> 4;
        let cash_digits = b1 & 0x0f;
        let cash_rounding = b1 >> 4;

        FractionInfo {
            digits,
            rounding: match rounding {
                0 => Rounding::R1,
                1 => Rounding::R5,
                2 => Rounding::R20,
                3 => Rounding::R50,
                _n => {
                    debug_assert!(false, "invalid rounding encoding {_n}");
                    Rounding::R1
                }
            },
            cash_digits: if cash_digits == NONE_MARKER {
                None
            } else {
                Some(cash_digits)
            },
            cash_rounding: match cash_rounding {
                0 => Some(Rounding::R1),
                1 => Some(Rounding::R5),
                2 => Some(Rounding::R20),
                3 => Some(Rounding::R50),
                15 => None,
                _n => {
                    debug_assert!(false, "invalid rounding encoding {_n}");
                    None
                }
            },
        }
    }
}

impl<'a> ZeroMapKV<'a> for FractionInfo {
    type Container = zerovec::ZeroVec<'a, FractionInfo>;
    type Slice = zerovec::ZeroSlice<FractionInfo>;
    type GetType = <FractionInfo as AsULE>::ULE;
    type OwnedType = FractionInfo;
}
