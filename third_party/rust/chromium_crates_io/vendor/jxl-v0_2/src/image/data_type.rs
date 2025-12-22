// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::fmt::Debug;

mod private {
    pub trait Sealed {}
}

#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum DataTypeTag {
    U8,
    U16,
    U32,
    F32,
    I8,
    I16,
    I32,
    F16,
    F64,
}

impl DataTypeTag {
    // Note that the const block below asserts that the implementation of this method is correct
    // (as in, matches the size of the types that the tag corresponds to).
    pub const fn size(&self) -> usize {
        match self {
            DataTypeTag::U8 | DataTypeTag::I8 => 1,
            DataTypeTag::U16 | DataTypeTag::F16 | DataTypeTag::I16 => 2,
            DataTypeTag::U32 | DataTypeTag::F32 | DataTypeTag::I32 => 4,
            DataTypeTag::F64 => 8,
        }
    }
}

const _: () = {
    assert!(std::mem::size_of::<i8>() == DataTypeTag::I8.size());
    assert!(std::mem::size_of::<u8>() == DataTypeTag::U8.size());
    assert!(std::mem::size_of::<i16>() == DataTypeTag::I16.size());
    assert!(std::mem::size_of::<u16>() == DataTypeTag::U16.size());
    assert!(std::mem::size_of::<crate::util::f16>() == DataTypeTag::F16.size());
    assert!(std::mem::size_of::<i32>() == DataTypeTag::I32.size());
    assert!(std::mem::size_of::<u32>() == DataTypeTag::U32.size());
    assert!(std::mem::size_of::<f32>() == DataTypeTag::F32.size());
    assert!(std::mem::size_of::<f64>() == DataTypeTag::F64.size());
};

/// # Safety
/// Any type implementing this trait must be a "bag-of-bits" type with no padding.
/// Moreover, Self::DATA_TYPE_ID.size() must match the size of `Self`.
pub unsafe trait ImageDataType:
    private::Sealed + Copy + Default + 'static + Debug + PartialEq + Send + Sync
{
    /// ID of this data type. Different types *must* have different values (this is not a safety
    /// requirement).
    const DATA_TYPE_ID: DataTypeTag;

    fn from_f64(f: f64) -> Self;
    fn to_f64(self) -> f64;
    #[cfg(test)]
    fn random<R: rand::Rng>(rng: &mut R) -> Self;
}

#[cfg(test)]
macro_rules! type_min {
    (f32) => {
        0.0f32
    };
    (f64) => {
        0.0f64
    };
    ($ty: ty) => {
        <$ty>::MIN
    };
}

#[cfg(test)]
macro_rules! type_max {
    (f32) => {
        1.0f32
    };
    (f64) => {
        1.0f64
    };
    ($ty: ty) => {
        <$ty>::MAX
    };
}

macro_rules! impl_image_data_type {
    ($ty: ident, $id: ident) => {
        impl private::Sealed for $ty {}
        // SAFETY: primitive integer/float types are bag-of-bits types.
        unsafe impl ImageDataType for $ty {
            const DATA_TYPE_ID: DataTypeTag = DataTypeTag::$id;
            fn from_f64(f: f64) -> $ty {
                f as $ty
            }
            fn to_f64(self) -> f64 {
                self as f64
            }
            #[cfg(test)]
            fn random<R: rand::Rng>(rng: &mut R) -> Self {
                use rand::distr::{Distribution, Uniform};
                let min = type_min!($ty);
                let max = type_max!($ty);
                Uniform::new_inclusive(min, max).unwrap().sample(rng)
            }
        }
    };
}

impl_image_data_type!(u8, U8);
impl_image_data_type!(u16, U16);
impl_image_data_type!(u32, U32);
impl_image_data_type!(f32, F32);
impl_image_data_type!(i8, I8);
impl_image_data_type!(i16, I16);
impl_image_data_type!(i32, I32);

// Meant to be used by the simple render pipeline and in general for
// testing purposes.
impl_image_data_type!(f64, F64);

impl private::Sealed for crate::util::f16 {}
// SAFETY: f16 is a bag-of-bits type (transparent wrapper around u16).
unsafe impl ImageDataType for crate::util::f16 {
    const DATA_TYPE_ID: DataTypeTag = DataTypeTag::F16;
    fn from_f64(f: f64) -> crate::util::f16 {
        crate::util::f16::from_f64(f)
    }
    fn to_f64(self) -> f64 {
        crate::util::f16::to_f64(self)
    }
    #[cfg(test)]
    fn random<R: rand::Rng>(rng: &mut R) -> Self {
        use rand::distr::{Distribution, Uniform};
        Self::from_f64(Uniform::new(0.0f32, 1.0f32).unwrap().sample(rng) as f64)
    }
}
