use core::fmt::Display;
use core::ops::{Add, BitAnd, BitOr, BitOrAssign, BitXorAssign, Div, Mul, Shl, Shr, Sub};

pub trait Float: Copy {
    const MANTISSA_DIGITS: u32;
    const MIN_10_EXP: i32;
    const MAX_10_EXP: i32;
    const MAX_DIGITS10: u32;
}

impl Float for f32 {
    const MANTISSA_DIGITS: u32 = Self::MANTISSA_DIGITS;
    const MIN_10_EXP: i32 = Self::MIN_10_EXP;
    const MAX_10_EXP: i32 = Self::MAX_10_EXP;
    const MAX_DIGITS10: u32 = 9;
}

impl Float for f64 {
    const MANTISSA_DIGITS: u32 = Self::MANTISSA_DIGITS;
    const MIN_10_EXP: i32 = Self::MIN_10_EXP;
    const MAX_10_EXP: i32 = Self::MAX_10_EXP;
    const MAX_DIGITS10: u32 = 17;
}

pub trait UInt:
    Copy
    + From<u8>
    + From<bool>
    + Add<Output = Self>
    + Sub<Output = Self>
    + Mul<Output = Self>
    + Div<Output = Self>
    + BitAnd<Output = Self>
    + BitOr<Output = Self>
    + Shl<u8, Output = Self>
    + Shl<i32, Output = Self>
    + Shl<u32, Output = Self>
    + Shr<i32, Output = Self>
    + Shr<u32, Output = Self>
    + BitOrAssign
    + BitXorAssign
    + PartialOrd
    + Into<u64>
    + Display
{
    type Signed: Ord;
}

impl UInt for u32 {
    type Signed = i32;
}

impl UInt for u64 {
    type Signed = i64;
}
