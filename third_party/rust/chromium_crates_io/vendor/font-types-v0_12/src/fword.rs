//! 16-bit signed and unsigned font-units

use super::{F48Dot16, Fixed};

/// 16-bit signed quantity in font design units.
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[cfg_attr(feature = "bytemuck", derive(bytemuck::AnyBitPattern))]
#[repr(transparent)]
pub struct FWord(i16);

/// 16-bit unsigned quantity in font design units.
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[cfg_attr(feature = "bytemuck", derive(bytemuck::AnyBitPattern))]
#[repr(transparent)]
pub struct UfWord(u16);

impl FWord {
    pub const fn new(raw: i16) -> Self {
        Self(raw)
    }

    pub const fn to_i16(self) -> i16 {
        self.0
    }

    /// Converts this number to a 16.16 fixed point value.
    pub const fn to_fixed(self) -> Fixed {
        Fixed::from_i32(self.0 as i32)
    }

    /// Applies an item variation delta, returning the varied value as a
    /// single precision floating point number.
    ///
    /// A delta for a font unit valued target is already in font units, so
    /// the accumulated 48.16 delta applies unscaled. The result is
    /// intentionally not rounded back to an integer.
    #[inline(always)]
    pub fn apply_delta(self, delta: F48Dot16) -> f32 {
        self.0 as f32 + delta.to_f64() as f32
    }

    /// The representation of this number as a big-endian byte array.
    pub const fn to_be_bytes(self) -> [u8; 2] {
        self.0.to_be_bytes()
    }
}

impl UfWord {
    pub const fn new(raw: u16) -> Self {
        Self(raw)
    }

    pub const fn to_u16(self) -> u16 {
        self.0
    }

    /// Converts this number to a 16.16 fixed point value.
    pub const fn to_fixed(self) -> Fixed {
        Fixed::from_i32(self.0 as i32)
    }

    /// Applies an item variation delta, returning the varied value as a
    /// single precision floating point number.
    ///
    /// A delta for a font unit valued target is already in font units, so
    /// the accumulated 48.16 delta applies unscaled. The result is
    /// intentionally not rounded back to an integer.
    #[inline(always)]
    pub fn apply_delta(self, delta: F48Dot16) -> f32 {
        self.0 as f32 + delta.to_f64() as f32
    }

    /// The representation of this number as a big-endian byte array.
    pub const fn to_be_bytes(self) -> [u8; 2] {
        self.0.to_be_bytes()
    }
}

impl std::fmt::Display for FWord {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        self.0.fmt(f)
    }
}

impl std::fmt::Display for UfWord {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        self.0.fmt(f)
    }
}

impl From<u16> for UfWord {
    fn from(src: u16) -> Self {
        UfWord(src)
    }
}

impl From<i16> for FWord {
    fn from(src: i16) -> Self {
        FWord(src)
    }
}

impl From<FWord> for i16 {
    fn from(src: FWord) -> Self {
        src.0
    }
}

impl From<UfWord> for u16 {
    fn from(src: UfWord) -> Self {
        src.0
    }
}

crate::newtype_scalar!(FWord, [u8; 2]);
crate::newtype_scalar!(UfWord, [u8; 2]);
//TODO: we can add addition/etc as needed

#[cfg(test)]
mod tests {
    use super::*;

    /// Font unit targets take the accumulated delta unscaled, matching the
    /// former `FloatItemDeltaTarget` impls in read-fonts these replaced.
    #[test]
    fn apply_delta() {
        assert_eq!(FWord::new(100).apply_delta(F48Dot16::from_f64(2.5)), 102.5);
        assert_eq!(
            FWord::new(-100).apply_delta(F48Dot16::from_f64(-0.25)),
            -100.25
        );
        assert_eq!(
            UfWord::new(1000).apply_delta(F48Dot16::from_f64(-1.5)),
            998.5
        );
        assert_eq!(UfWord::new(0).apply_delta(F48Dot16::ZERO), 0.0);
    }
}
