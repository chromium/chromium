// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::dimension::provider::currency::compact::CompactCount;
use icu_plurals::PluralCategory;
use zerovec::ule::{AsULE, UleError, ULE};

/// [`CompactCountULE`] is a type optimized for efficient storing and
/// deserialization of [`CompactCount`] using the `ZeroVec` model.
///
/// The serialization model packages the pattern item in one byte.
///
/// The first bit (b7) is used to determine count_type.
/// If the bit is `0`, then, then the type is `Standard`.
/// If the bit is `1`, then, then the type is `AlphaNextToNumber`.
///
/// The last three bits (b2, b1 & b0), are used to determine the count:
///     000 -> Count::Zero
///     001 -> Count::One
///     010 -> Count::Two
///     011 -> Count::Few
///     100 -> Count::Many
///     101 -> Count::Other
///
/// The other bits (b6,b5,b4,b3) must always be zeros.
#[derive(Copy, Clone, Debug, PartialEq)]
#[repr(transparent)]
pub struct CompactCountULE(u8);

// Safety (based on the safety checklist on the ULE trait):
//  1. CompactCountULE does not include any uninitialized or padding bytes.
//     (achieved by `#[repr(transparent)]` on a ULE type)
//  2. CompactCountULE is aligned to 1 byte.
//     (achieved by `#[repr(transparent)]` on a ULE type)
//  3. The impl of validate_bytes() returns an error if any byte is not valid.
//  4. The impl of validate_bytes() returns an error if there are extra bytes.
//  5. The other ULE methods use the default impl.
//  6. CompactCountULE byte equality is semantic equality.
unsafe impl ULE for CompactCountULE {
    fn validate_bytes(bytes: &[u8]) -> Result<(), UleError> {
        for byte in bytes {
            if byte & 0b0111_1000 != 0 {
                return Err(UleError::parse::<Self>());
            }

            if byte & 0b0000_0111 > 5 {
                return Err(UleError::parse::<Self>());
            }
        }

        Ok(())
    }
}

impl AsULE for CompactCount {
    type ULE = CompactCountULE;
    fn to_unaligned(self) -> Self::ULE {
        CompactCountULE(match self {
            CompactCount::Standard(count) => count as u8,
            CompactCount::AlphaNextToNumber(count) => (count as u8) | 0b1000_0000,
        })
    }

    #[inline]
    fn from_unaligned(unaligned: Self::ULE) -> Self {
        let count = match unaligned.0 & 0b0000_0111 {
            0 => PluralCategory::Zero,
            1 => PluralCategory::One,
            2 => PluralCategory::Two,
            3 => PluralCategory::Few,
            4 => PluralCategory::Many,
            5 => PluralCategory::Other,
            _ => unreachable!(),
        };
        match unaligned.0 & 0b1000_0000 {
            0 => CompactCount::Standard(count),
            0b1000_0000 => CompactCount::AlphaNextToNumber(count),
            _ => unreachable!(),
        }
    }
}
