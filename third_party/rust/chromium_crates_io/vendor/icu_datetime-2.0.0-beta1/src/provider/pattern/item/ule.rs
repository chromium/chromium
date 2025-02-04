// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::{GenericPatternItem, PatternItem};
use crate::fields;
use core::convert::TryFrom;
use zerovec::ule::{AsULE, UleError, ULE};

/// `PatternItemULE` is a type optimized for efficient storing and
/// deserialization of `FixedCalendarDateTimeFormatter` `PatternItem` elements using
/// `ZeroVec` model.
///
/// The serialization model packages the pattern item in three bytes.
///
/// The first bit is used to disriminate the item variant. If the bit is
/// set, then the value is the `PatternItem::Field` variant. Otherwise,
/// the `PatternItem::Literal` is used.
///
/// In case the discriminant is set:
///
/// 1) The rest of the first byte remains unused.
/// 2) The second byte encodes `FieldSymbol` encoded as (Type: 4 bits, Symbol: 4 bits).
/// 3) The third byte encodes the field length.
///
/// If the discriminant is not set, the bottom three bits of the first byte,
/// together with the next two bytes, contain all 21 bits required to encode
/// any [`Unicode Code Point`]. By design, the representation of a code point
/// is the same between [`PatternItemULE`] and [`GenericPatternItemULE`].
///
/// # Diagram
///
/// ```text
/// ┌───────────────┬───────────────┬───────────────┐
/// │       u8      │       u8      │       u8      │
/// ├─┬─┬─┬─┬─┬─┬─┬─┼─┬─┬─┬─┬─┬─┬─┬─┼─┬─┬─┬─┬─┬─┬─┬─┤
/// ├─┴─┴─┼─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┤
/// │     │          Unicode Code Point             │ Literal
/// ├─┬───┴─────────┬───────────────┬───────────────┤
/// │X│             │  FieldSymbol  │  FieldLength  │ Field
/// └─┴─────────────┴───────────────┴───────────────┘
///  ▲
///  │
///  Variant Discriminant
/// ```
///
/// # Optimization
///
/// This model is optimized for efficient packaging of the `PatternItem` elements
/// and performant deserialization from the `PatternItemULE` to `PatternItem` type.
///
/// # Constraints
///
/// The model leaves at most 8 `PatternItem` variants, limits the number of possible
/// field types and symbols to 16 each and limits the number of length variants to 256.
///
/// [`Unicode Code Point`]: http://www.unicode.org/versions/latest/
#[derive(Copy, Clone, Debug, PartialEq)]
#[repr(transparent)]
pub struct PatternItemULE([u8; 3]);

impl PatternItemULE {
    /// Given the first byte of the three-byte array that `PatternItemULE` encodes,
    /// the method determines whether the discriminant in
    /// the byte indicates that the array encodes the `PatternItem::Field`
    /// or `PatternItem::Literal` variant of the `PatternItem`.
    ///
    /// Returns true when it is a `PatternItem::Field`.
    #[inline]
    fn determine_field_from_u8(byte: u8) -> bool {
        byte & 0b1000_0000 != 0
    }

    #[inline]
    fn bytes_in_range(value: (&u8, &u8, &u8)) -> bool {
        if Self::determine_field_from_u8(*value.0) {
            // ensure that unused bytes are all zero
            fields::FieldULE::validate_byte_pair((*value.1, *value.2)).is_ok()
                && *value.0 == 0b1000_0000
        } else {
            char::try_from(u32::from_be_bytes([0x00, *value.0, *value.1, *value.2])).is_ok()
        }
    }
}

// Safety (based on the safety checklist on the ULE trait):
//  1. PatternItemULE does not include any uninitialized or padding bytes.
//     (achieved by `#[repr(transparent)]` on a ULE type)
//  2. PatternItemULE is aligned to 1 byte.
//     (achieved by `#[repr(transparent)]` on a ULE type)
//  3. The impl of validate_bytes() returns an error if any byte is not valid.
//  4. The impl of validate_bytes() returns an error if there are extra bytes.
//  5. The other ULE methods use the default impl.
//  6. PatternItemULE byte equality is semantic equality.
unsafe impl ULE for PatternItemULE {
    fn validate_bytes(bytes: &[u8]) -> Result<(), UleError> {
        if bytes.len() % 3 != 0 {
            return Err(UleError::length::<Self>(bytes.len()));
        }

        #[allow(clippy::indexing_slicing)] // chunks
        if !bytes
            .chunks(3)
            .all(|c| Self::bytes_in_range((&c[0], &c[1], &c[2])))
        {
            return Err(UleError::parse::<Self>());
        }
        Ok(())
    }
}

impl AsULE for PatternItem {
    type ULE = PatternItemULE;

    #[inline]
    fn to_unaligned(self) -> Self::ULE {
        match self {
            Self::Field(field) => {
                PatternItemULE([0b1000_0000, field.symbol.idx(), field.length.idx()])
            }
            Self::Literal(ch) => {
                let u = ch as u32;
                let bytes = u.to_be_bytes();
                PatternItemULE([bytes[1], bytes[2], bytes[3]])
            }
        }
    }

    #[inline]
    fn from_unaligned(unaligned: Self::ULE) -> Self {
        let value = unaligned.0;
        #[allow(clippy::unwrap_used)] // validated
        if PatternItemULE::determine_field_from_u8(value[0]) {
            let symbol = fields::FieldSymbol::from_idx(value[1]).unwrap();
            let length = fields::FieldLength::from_idx(value[2]).unwrap();
            PatternItem::Field(fields::Field { symbol, length })
        } else {
            // validated
            PatternItem::Literal(unsafe {
                char::from_u32_unchecked(u32::from_be_bytes([0x00, value[0], value[1], value[2]]))
            })
        }
    }
}

/// `GenericPatternItemULE` is a type optimized for efficient storing and
/// deserialization of `FixedCalendarDateTimeFormatter` `GenericPatternItem` elements using
/// the `ZeroVec` model.
///
/// The serialization model packages the pattern item in three bytes.
///
/// The first bit is used to disriminate the item variant. If the bit is
/// set, then the value is the `GenericPatternItem::Placeholder` variant. Otherwise,
/// the `GenericPatternItem::Literal` is used.
///
/// In case the discriminant is set:
///
/// 1) The rest of the first byte remains unused.
/// 2) The second byte is unused.
/// 3) The third byte encodes the placeholder index.
///
/// If the discriminant is not set, the bottom three bits of the first byte,
/// together with the next two bytes, contain all 21 bits required to encode
/// any [`Unicode Code Point`]. By design, the representation of a code point
/// is the same between [`PatternItemULE`] and [`GenericPatternItemULE`].
///
/// # Diagram
///
/// ```text
/// ┌───────────────┬───────────────┬───────────────┐
/// │       u8      │       u8      │       u8      │
/// ├─┬─┬─┬─┬─┬─┬─┬─┼─┬─┬─┬─┬─┬─┬─┬─┼─┬─┬─┬─┬─┬─┬─┬─┤
/// ├─┴─┴─┼─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┤
/// │     │          Unicode Code Point             │ Literal
/// ├─┬───┴─────────────────────────┬───────────────┤
/// │X│                             │  Placeholder  │ Placeholder
/// └─┴─────────────────────────────┴───────────────┘
///  ▲
///  │
///  Variant Discriminant
/// ```
///
/// # Optimization
///
/// This model is optimized for efficient packaging of the `GenericPatternItem` elements
/// and performant deserialization from the `GernericPatternItemULE` to `GenericPatternItem` type.
///
/// # Constraints
///
/// The model leaves at most 8 `PatternItem` variants, and limits the placeholder
/// to a single u8.
///
/// [`Unicode Code Point`]: http://www.unicode.org/versions/latest/
#[derive(Copy, Clone, Debug, PartialEq)]
#[repr(transparent)]
pub struct GenericPatternItemULE([u8; 3]);

impl GenericPatternItemULE {
    /// Given the first byte of the three-byte array that `GenericPatternItemULE` encodes,
    /// the method determines whether the discriminant in
    /// the byte indicates that the array encodes the `GenericPatternItem::Field`
    /// or `GenericPatternItem::Literal` variant of the `GenericPatternItem`.
    ///
    /// Returns true when it is a `GenericPatternItem::Field`.
    #[inline]
    fn determine_field_from_u8(byte: u8) -> bool {
        byte & 0b1000_0000 != 0
    }

    #[inline]
    fn bytes_in_range(value: (&u8, &u8, &u8)) -> bool {
        if Self::determine_field_from_u8(*value.0) {
            // ensure that unused bytes are all zero
            *value.0 == 0b1000_0000 && *value.1 == 0 && *value.2 < 10
        } else {
            let u = u32::from_be_bytes([0x00, *value.0, *value.1, *value.2]);
            char::try_from(u).is_ok()
        }
    }

    /// Converts this [`GenericPatternItemULE`] to a [`PatternItemULE`]
    /// (if a Literal) or returns the placeholder value.
    #[inline]
    pub(crate) fn as_pattern_item_ule(&self) -> Result<&PatternItemULE, u8> {
        if Self::determine_field_from_u8(self.0[0]) {
            Err(self.0[2])
        } else {
            if cfg!(debug_assertions) {
                let GenericPatternItem::Literal(c) = GenericPatternItem::from_unaligned(*self)
                else {
                    unreachable!("expected a literal!")
                };
                let pattern_item_ule = PatternItem::Literal(c).to_unaligned();
                debug_assert_eq!(self.0, pattern_item_ule.0);
            }
            // Safety: when a Literal, the two ULEs have the same repr,
            // as shown in the above assertion (and the class docs).
            Ok(unsafe { core::mem::transmute::<&GenericPatternItemULE, &PatternItemULE>(self) })
        }
    }
}

// Safety (based on the safety checklist on the ULE trait):
//  1. GenericPatternItemULE does not include any uninitialized or padding bytes.
//     (achieved by `#[repr(transparent)]` on a type that satisfies this invariant)
//  2. GenericPatternItemULE is aligned to 1 byte.
//     (achieved by `#[repr(transparent)]` on a type that satisfies this invariant)
//  3. The impl of validate_bytes() returns an error if any byte is not valid.
//  4. The impl of validate_bytes() returns an error if there are extra bytes.
//  5. The other ULE methods use the default impl.
//  6. GenericPatternItemULE byte equality is semantic equality.
unsafe impl ULE for GenericPatternItemULE {
    fn validate_bytes(bytes: &[u8]) -> Result<(), UleError> {
        if bytes.len() % 3 != 0 {
            return Err(UleError::length::<Self>(bytes.len()));
        }
        #[allow(clippy::indexing_slicing)] // chunks
        if !bytes
            .chunks_exact(3)
            .all(|c| Self::bytes_in_range((&c[0], &c[1], &c[2])))
        {
            return Err(UleError::parse::<Self>());
        }
        Ok(())
    }
}

impl GenericPatternItem {
    #[inline]
    pub(crate) const fn to_unaligned_const(self) -> <Self as AsULE>::ULE {
        match self {
            Self::Placeholder(idx) => GenericPatternItemULE([0b1000_0000, 0x00, idx]),
            Self::Literal(ch) => {
                let u = ch as u32;
                let bytes = u.to_be_bytes();
                GenericPatternItemULE([bytes[1], bytes[2], bytes[3]])
            }
        }
    }
}

impl AsULE for GenericPatternItem {
    type ULE = GenericPatternItemULE;

    #[inline]
    fn to_unaligned(self) -> Self::ULE {
        self.to_unaligned_const()
    }

    #[inline]
    fn from_unaligned(unaligned: Self::ULE) -> Self {
        let value = unaligned.0;
        if GenericPatternItemULE::determine_field_from_u8(value[0]) {
            Self::Placeholder(value[2])
        } else {
            #[allow(clippy::unwrap_used)] // validated
            Self::Literal(
                char::try_from(u32::from_be_bytes([0x00, value[0], value[1], value[2]])).unwrap(),
            )
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::fields::{FieldLength, FieldSymbol, Second, Year};
    use zerovec::ule::{AsULE, ULE};

    #[test]
    fn test_pattern_item_as_ule() {
        let samples = [
            (
                PatternItem::from((FieldSymbol::Minute, FieldLength::Two)),
                [0x80, FieldSymbol::Minute.idx(), FieldLength::Two.idx()],
            ),
            (
                PatternItem::from((FieldSymbol::Year(Year::Calendar), FieldLength::Four)),
                [
                    0x80,
                    FieldSymbol::Year(Year::Calendar).idx(),
                    FieldLength::Four.idx(),
                ],
            ),
            (
                PatternItem::from((FieldSymbol::Year(Year::Cyclic), FieldLength::Four)),
                [
                    0x80,
                    FieldSymbol::Year(Year::Cyclic).idx(),
                    FieldLength::Four.idx(),
                ],
            ),
            (
                PatternItem::from((FieldSymbol::Second(Second::MillisInDay), FieldLength::One)),
                [
                    0x80,
                    FieldSymbol::Second(Second::MillisInDay).idx(),
                    FieldLength::One.idx(),
                ],
            ),
            (PatternItem::from('z'), [0x00, 0x00, 0x7a]),
        ];

        for (ref_pattern, ref_bytes) in samples {
            let ule = ref_pattern.to_unaligned();
            assert_eq!(ULE::slice_as_bytes(&[ule]), ref_bytes);
            let pattern = PatternItem::from_unaligned(ule);
            assert_eq!(pattern, ref_pattern);
        }
    }

    #[test]
    fn test_pattern_item_ule() {
        let samples = [(
            [
                PatternItem::from((FieldSymbol::Year(Year::Calendar), FieldLength::Four)),
                PatternItem::from('z'),
                PatternItem::from((FieldSymbol::Second(Second::MillisInDay), FieldLength::One)),
            ],
            [
                [
                    0x80,
                    FieldSymbol::Year(Year::Calendar).idx(),
                    FieldLength::Four.idx(),
                ],
                [0x00, 0x00, 0x7a],
                [
                    0x80,
                    FieldSymbol::Second(Second::MillisInDay).idx(),
                    FieldLength::One.idx(),
                ],
            ],
        )];

        for (ref_pattern, ref_bytes) in samples {
            let mut bytes: Vec<u8> = vec![];
            for item in ref_pattern.iter() {
                let ule = item.to_unaligned();
                bytes.extend(ULE::slice_as_bytes(&[ule]));
            }

            let mut bytes2: Vec<u8> = vec![];
            for seq in ref_bytes.iter() {
                bytes2.extend_from_slice(seq);
            }

            assert!(PatternItemULE::validate_bytes(&bytes).is_ok());
            assert_eq!(bytes, bytes2);
        }
    }

    #[test]
    fn test_generic_pattern_item_as_ule() {
        let samples = [
            (GenericPatternItem::Placeholder(4), [0x80, 0x00, 4]),
            (GenericPatternItem::Placeholder(0), [0x80, 0x00, 0]),
            (GenericPatternItem::from('z'), [0x00, 0x00, 0x7a]),
        ];

        for (ref_pattern, ref_bytes) in samples {
            let ule = ref_pattern.to_unaligned();
            assert_eq!(ULE::slice_as_bytes(&[ule]), ref_bytes);
            let pattern = GenericPatternItem::from_unaligned(ule);
            assert_eq!(pattern, ref_pattern);
        }
    }
}
