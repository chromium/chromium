//! This module contains the functionality that can be shared between [`crate::Int`] and [`crate::UInt`]

/// Copies LEN bytes from `from[FROM_OFFSET]` to `to[TO_OFFSET]`.
///
/// Usable in const contexts and inlines for small arrays.
#[inline(always)]
pub(crate) const fn const_byte_copy<
    const LEN: usize,
    const TO_OFFSET: usize,
    const FROM_OFFSET: usize,
>(
    to: &mut [u8],
    from: &[u8],
) {
    let mut i = 0;
    while i < LEN {
        to[TO_OFFSET + i] = from[FROM_OFFSET + i];
        i += 1;
    }
}

// Define type aliases like u1, u63 and u80 (and their signed equivalents) using the smallest possible underlying data type.
// These are for convenience only - UInt<u32, 15> is still legal
macro_rules! type_alias {
    ($ty:ident($storage:ty), $(($name:ident, $bits:expr)),+) => {
        $( pub type $name = crate::$ty<$storage, $bits>; )+
    }
}

pub(crate) use type_alias;

// Conversions
macro_rules! from_arbitrary_int_impl {
    ($ty:ident($from:ty), [$($into:ty),+]) => {
        $(
            impl<const BITS: usize, const BITS_FROM: usize> From<$ty<$from, BITS_FROM>> for $ty<$into, BITS> {
                #[inline]
                fn from(item: $ty<$from, BITS_FROM>) -> Self {
                    const { assert!(BITS_FROM <= BITS, "Can not call from() to convert between the given bit widths.") };
                    Self { value: item.value as $into }
                }
            }
        )+
    };
}

pub(crate) use from_arbitrary_int_impl;

macro_rules! from_native_impl {
    ($ty:ident($from:ty), [$($into:ty),+]) => {
        $(
            impl<const BITS: usize> From<$from> for $ty<$into, BITS> {
                #[inline]
                fn from(from: $from) -> Self {
                    const { assert!(<$from>::BITS as usize <= BITS, "Can not call from() to convert between the given bit widths.") };
                    Self { value: from as $into }
                }
            }

            impl<const BITS: usize> From<$ty<$from, BITS>> for $into {
                #[inline]
                fn from(from: $ty<$from, BITS>) -> Self {
                    const { assert!(BITS <= <$from>::BITS as usize, "Can not call from() to convert between the given bit widths.") };
                    from.value as $into
                }
            }
        )+
    };
}

pub(crate) use from_native_impl;

macro_rules! impl_extract {
    (
        $base_type:ty,
        $example:literal,
        |$value:ident| $constructor:expr,
        $(($bits:literal, $(($type:ty, $extract_fn:ident)),+)),+
    ) => {
        $($(
            #[doc = concat!("Extracts bits from a given value, starting from `start_bit`. This is equivalent to: `", $example, "`.")]
            /// Unlike [`new`](Self::new), this function doesn't perform range-checking and is slightly more efficient.
            ///
            /// # Panics
            ///
            #[doc = concat!(" Panics if `start_bit + Self::BITS` doesn't fit within an ", stringify!($type), ", i.e. it is greater than ", stringify!($bits), ".")]
            #[inline]
            pub const fn $extract_fn(value: $type, start_bit: usize) -> Self {
                // Query MAX to ensure that we get a compiler error if the current definition is bogus (e.g. <u8, 9>)
                let _ = Self::MAX;
                assert!(start_bit + BITS <= $bits);

                let $value = (value >> start_bit) as $base_type;
                Self { value: $constructor }
            }
        )+)+
    };
}

pub(crate) use impl_extract;

macro_rules! bytes_operation_impl {
    ($target:ty, $base_data_type:ty) => {
        impl $target {
            /// Reverses the byte order of the integer.
            #[inline]
            pub const fn swap_bytes(self) -> Self {
                // swap_bytes() of the underlying type does most of the work. Then, we just need to shift
                Self {
                    value: self.value.swap_bytes() >> Self::UNUSED_BITS,
                }
            }

            pub const fn to_le_bytes(self) -> [u8; Self::BITS >> 3] {
                let mut result = [0_u8; Self::BITS >> 3];
                let be = self.value.to_le_bytes();
                crate::common::const_byte_copy::<{ Self::BITS >> 3 }, 0, 0>(&mut result, &be);
                result
            }

            pub const fn to_be_bytes(self) -> [u8; Self::BITS >> 3] {
                let mut result = [0_u8; Self::BITS >> 3];
                let be = self.value.to_be_bytes();
                crate::common::const_byte_copy::<{ Self::BITS >> 3 }, 0, { Self::UNUSED_BITS >> 3 }>(
                    &mut result,
                    &be,
                );
                result
            }

            pub const fn from_le_bytes(from: [u8; Self::BITS >> 3]) -> Self {
                let mut bytes = [0_u8; core::mem::size_of::<$base_data_type>()];
                crate::common::const_byte_copy::<{ Self::BITS >> 3 }, { Self::UNUSED_BITS >> 3 }, 0>(
                    &mut bytes, &from,
                );
                Self {
                    value: <$base_data_type>::from_le_bytes(bytes) >> Self::UNUSED_BITS,
                }
            }

            pub const fn from_be_bytes(from: [u8; Self::BITS >> 3]) -> Self {
                let mut bytes = [0_u8; core::mem::size_of::<$base_data_type>()];
                crate::common::const_byte_copy::<{ Self::BITS >> 3 }, 0, 0>(&mut bytes, &from);
                Self {
                    value: <$base_data_type>::from_be_bytes(bytes) >> Self::UNUSED_BITS,
                }
            }

            #[inline]
            pub const fn to_ne_bytes(self) -> [u8; Self::BITS >> 3] {
                #[cfg(target_endian = "little")]
                {
                    self.to_le_bytes()
                }
                #[cfg(target_endian = "big")]
                {
                    self.to_be_bytes()
                }
            }

            #[inline]
            pub const fn from_ne_bytes(bytes: [u8; Self::BITS >> 3]) -> Self {
                #[cfg(target_endian = "little")]
                {
                    Self::from_le_bytes(bytes)
                }
                #[cfg(target_endian = "big")]
                {
                    Self::from_be_bytes(bytes)
                }
            }

            #[inline]
            pub const fn to_le(self) -> Self {
                #[cfg(target_endian = "little")]
                {
                    self
                }
                #[cfg(target_endian = "big")]
                {
                    self.swap_bytes()
                }
            }

            #[inline]
            pub const fn to_be(self) -> Self {
                #[cfg(target_endian = "little")]
                {
                    self.swap_bytes()
                }
                #[cfg(target_endian = "big")]
                {
                    self
                }
            }

            #[inline]
            pub const fn from_le(value: Self) -> Self {
                value.to_le()
            }

            #[inline]
            pub const fn from_be(value: Self) -> Self {
                value.to_be()
            }
        }
    };
}

pub(crate) use bytes_operation_impl;

/// Implements [`core::iter::Sum`] and [`core::iter::Product`] for an integer type.
macro_rules! impl_sum_product {
    ($type:ident, $one:literal, $trait:ident) => {
        // This implements `Sum` for owned values, for example when using an iterator from a fixed-sized array.
        impl<T: BuiltinInteger + $trait, const BITS: usize> core::iter::Sum for $type<T, BITS>
        where
            Self: Integer + Default + core::ops::Add<Output = Self>,
        {
            #[inline]
            fn sum<I: Iterator<Item = Self>>(iter: I) -> Self {
                // Use `default()` to construct a value of zero.
                iter.fold(Self::default(), |lhs, rhs| lhs + rhs)
            }
        }

        // This implements `Sum` for borrowed values, for example when using an iterator from a slice.
        impl<'a, T: BuiltinInteger + $trait, const BITS: usize> core::iter::Sum<&'a Self>
            for $type<T, BITS>
        where
            Self: Integer + Default + core::ops::Add<Output = Self>,
        {
            #[inline]
            fn sum<I: Iterator<Item = &'a Self>>(iter: I) -> Self {
                iter.fold(Self::default(), |lhs, rhs| lhs + *rhs)
            }
        }

        impl<T: BuiltinInteger + $trait, const BITS: usize> core::iter::Product for $type<T, BITS>
        where
            Self: Integer + core::ops::Mul<Output = Self>,
        {
            #[inline]
            fn product<I: Iterator<Item = Self>>(iter: I) -> Self {
                iter.fold(Self::from_($one), |lhs, rhs| lhs * rhs)
            }
        }

        impl<'a, T: BuiltinInteger + $trait, const BITS: usize> core::iter::Product<&'a Self>
            for $type<T, BITS>
        where
            Self: Integer + core::ops::Mul<Output = Self>,
        {
            #[inline]
            fn product<I: Iterator<Item = &'a Self>>(iter: I) -> Self {
                iter.fold(Self::from_($one), |lhs, rhs| lhs * *rhs)
            }
        }
    };
}

pub(crate) use impl_sum_product;

/// Implements support for the `schemars` crate, if the feature is enabled.
macro_rules! impl_schemars {
    ($type:tt, $str_prefix:literal, $trait:ident) => {
        #[cfg(feature = "schemars")]
        impl<T: BuiltinInteger + $trait, const BITS: usize> schemars::JsonSchema for $type<T, BITS>
        where
            Self: Integer,
        {
            fn schema_name() -> alloc::string::String {
                use alloc::string::ToString;
                [$str_prefix, &BITS.to_string()].concat()
            }

            fn json_schema(_gen: &mut schemars::gen::SchemaGenerator) -> schemars::schema::Schema {
                use schemars::schema::{InstanceType, NumberValidation, Schema, SchemaObject};
                let schema_object = SchemaObject {
                    instance_type: Some(InstanceType::Integer.into()),
                    format: Some(Self::schema_name()),
                    number: Some(alloc::boxed::Box::new(NumberValidation {
                        // Can be done with https://github.com/rust-lang/rfcs/pull/2484
                        // minimum: Some(Self::MIN.value().try_into().ok().unwrap()),
                        // maximum: Some(Self::MAX.value().try_into().ok().unwrap()),
                        ..Default::default()
                    })),
                    ..Default::default()
                };
                Schema::Object(schema_object)
            }
        }
    };
}

pub(crate) use impl_schemars;

/// Implement support for the `borsh` crate (if the feature is enabled)
macro_rules! impl_borsh {
    ($type:ident, $declaration_prefix:literal, $trait:ident) => {
        #[cfg(feature = "borsh")]
        impl<T: BuiltinInteger + $trait, const BITS: usize> borsh::BorshSerialize for $type<T, BITS>
        where
            Self: Integer,
            <<Self as Integer>::UnsignedInteger as Integer>::UnderlyingType: borsh::BorshSerialize,
        {
            fn serialize<W: borsh::io::Write>(&self, writer: &mut W) -> borsh::io::Result<()> {
                // Ideally, we'd want a buffer of size `BITS >> 3` or `size_of::<T>`, but that's not possible
                // with arrays at present (`feature(generic_const_exprs)`, once stable, will allow this).
                const BUFFER_SIZE: usize = core::mem::size_of::<u128>();
                let mut buffer = [0_u8; BUFFER_SIZE];
                const {
                    // This causes a compiler error if the buffer isn't big enough. That isn't possible with any
                    // of the types provided by this crate, but it can't hurt to double check.
                    assert!(core::mem::size_of::<T>() <= BUFFER_SIZE);
                }

                let serialized_byte_count = (BITS + 7) >> 3;
                self.to_unsigned().value().serialize(&mut &mut buffer[..])?;
                writer.write_all(&buffer[..serialized_byte_count])
            }
        }

        #[cfg(feature = "borsh")]
        impl<T: BuiltinInteger + $trait + borsh::BorshDeserialize, const BITS: usize>
            borsh::BorshDeserialize for $type<T, BITS>
        where
            Self: Integer,
            <<Self as Integer>::UnsignedInteger as Integer>::UnderlyingType:
                borsh::BorshDeserialize,
        {
            fn deserialize_reader<R: borsh::io::Read>(reader: &mut R) -> borsh::io::Result<Self> {
                // Ideally, we'd want a buffer of size `BITS >> 3` or `size_of::<T>`, but that's not possible
                // with arrays at present (`feature(generic_const_exprs)`, once stable, will allow this).
                const BUFFER_SIZE: usize = core::mem::size_of::<u128>();
                let mut buffer = [0_u8; BUFFER_SIZE];
                const {
                    // This causes a compiler error if the buffer isn't big enough. That isn't possible with any
                    // of the types provided by this crate, but it can't hurt to double check.
                    assert!(core::mem::size_of::<T>() <= BUFFER_SIZE);
                }
                let serialized_byte_count = (BITS + 7) >> 3;
                let underlying_byte_count = core::mem::size_of::<T>();

                // Read from the source, advancing cursor by the exact right number of bytes
                reader.read_exact(&mut buffer[..serialized_byte_count])?;

                // Deserialize the underlying type into an unsigned underlying type. We have to pass
                // in the correct number of bytes of the underlying type (or more, but let's be
                // precise). The unused bytes are all still zero.
                let value =
                    <<Self as Integer>::UnsignedInteger as Integer>::UnderlyingType::deserialize(
                        &mut &buffer[..underlying_byte_count],
                    )?;

                // We can use try_new to range check this to the correct number of bits (e.g. from
                // u16 to u13). We're still unsigned, but that range-check is also correct for
                // signed numbers as the upper bits are all zero.
                if let Ok(value) = <<Self as Integer>::UnsignedInteger>::try_new(value) {
                    Ok(Self::from_unsigned(value))
                } else {
                    Err(borsh::io::Error::new(
                        borsh::io::ErrorKind::InvalidData,
                        "Value out of range",
                    ))
                }
            }
        }

        #[cfg(feature = "borsh")]
        impl<T: BuiltinInteger + $trait, const BITS: usize> borsh::BorshSchema for $type<T, BITS> {
            fn add_definitions_recursively(
                definitions: &mut alloc::collections::btree_map::BTreeMap<
                    borsh::schema::Declaration,
                    borsh::schema::Definition,
                >,
            ) {
                let byte_count = ((BITS + 7) >> 3) as u8;
                let def = borsh::schema::Definition::Primitive(byte_count);
                definitions.insert(Self::declaration(), def);
            }

            fn declaration() -> borsh::schema::Declaration {
                use alloc::string::ToString;
                [$declaration_prefix, &BITS.to_string()].concat()
            }
        }
    };
}

pub(crate) use impl_borsh;

macro_rules! impl_step {
    ($type:tt,  $trait:ident) => {
        #[cfg(feature = "step_trait")]
        impl<T: BuiltinInteger + $trait, const BITS: usize> core::iter::Step for $type<T, BITS>
        where
            Self: Integer<UnderlyingType = T>,
            T: Copy + core::iter::Step,
        {
            #[inline]
            fn steps_between(start: &Self, end: &Self) -> (usize, Option<usize>) {
                core::iter::Step::steps_between(&start.value(), &end.value())
            }

            #[inline]
            fn forward_checked(start: Self, count: usize) -> Option<Self> {
                if let Some(res) = core::iter::Step::forward_checked(start.value(), count) {
                    Self::try_new(res).ok()
                } else {
                    None
                }
            }

            #[inline]
            fn backward_checked(start: Self, count: usize) -> Option<Self> {
                if let Some(res) = core::iter::Step::backward_checked(start.value(), count) {
                    Self::try_new(res).ok()
                } else {
                    None
                }
            }
        }
    };
}

pub(crate) use impl_step;

/// Implements support for the `num-traits` crate, if the feature is enabled.
macro_rules! impl_num_traits {
    ($type:ident, $trait:ident, $primitive_8:ty, |$result:ident| ($limit_result:expr, $clamp_result:expr)) => {
        #[cfg(feature = "num-traits")]
        impl<T: BuiltinInteger + $trait, const BITS: usize> num_traits::WrappingAdd
            for $type<T, BITS>
        where
            Self: Integer,
            T: PartialEq
                + Eq
                + Copy
                + Add<T, Output = T>
                + Sub<T, Output = T>
                + BitAnd<T, Output = T>
                + Not<Output = T>
                + Shr<usize, Output = T>
                + Shl<usize, Output = T>
                + From<$primitive_8>,
            core::num::Wrapping<T>: Add<core::num::Wrapping<T>, Output = core::num::Wrapping<T>>,
        {
            #[inline]
            fn wrapping_add(&self, rhs: &Self) -> Self {
                let $result =
                    (core::num::Wrapping(self.value()) + core::num::Wrapping(rhs.value())).0;
                Self {
                    value: $limit_result,
                }
            }
        }

        #[cfg(feature = "num-traits")]
        impl<T: BuiltinInteger + $trait, const BITS: usize> num_traits::WrappingSub
            for $type<T, BITS>
        where
            Self: Integer,
            T: PartialEq
                + Eq
                + Copy
                + Add<T, Output = T>
                + Sub<T, Output = T>
                + BitAnd<T, Output = T>
                + Not<Output = T>
                + Shr<usize, Output = T>
                + Shl<usize, Output = T>
                + From<$primitive_8>,
            core::num::Wrapping<T>: Sub<core::num::Wrapping<T>, Output = core::num::Wrapping<T>>,
        {
            #[inline]
            fn wrapping_sub(&self, rhs: &Self) -> Self {
                let $result =
                    (core::num::Wrapping(self.value()) - core::num::Wrapping(rhs.value())).0;
                Self {
                    value: $limit_result,
                }
            }
        }

        #[cfg(feature = "num-traits")]
        impl<T: BuiltinInteger + $trait, const BITS: usize> num_traits::SaturatingAdd
            for $type<T, BITS>
        where
            Self: Integer,
            T: PartialEq
                + Eq
                + Copy
                + Add<T, Output = T>
                + Sub<T, Output = T>
                + BitAnd<T, Output = T>
                + Not<Output = T>
                + Shr<usize, Output = T>
                + Shl<usize, Output = T>
                + From<$primitive_8>,
            core::num::Saturating<T>:
                Add<core::num::Saturating<T>, Output = core::num::Saturating<T>>,
        {
            #[inline]
            fn saturating_add(&self, rhs: &Self) -> Self {
                let $result =
                    (core::num::Saturating(self.value()) + core::num::Saturating(rhs.value())).0;
                Self {
                    value: $clamp_result,
                }
            }
        }

        #[cfg(feature = "num-traits")]
        impl<T: BuiltinInteger + $trait, const BITS: usize> num_traits::SaturatingSub
            for $type<T, BITS>
        where
            Self: Integer,
            T: PartialEq
                + Eq
                + Copy
                + Add<T, Output = T>
                + Sub<T, Output = T>
                + BitAnd<T, Output = T>
                + Not<Output = T>
                + Shr<usize, Output = T>
                + Shl<usize, Output = T>
                + From<$primitive_8>,
            core::num::Saturating<T>:
                Sub<core::num::Saturating<T>, Output = core::num::Saturating<T>>,
        {
            #[inline]
            fn saturating_sub(&self, rhs: &Self) -> Self {
                let $result =
                    (core::num::Saturating(self.value()) - core::num::Saturating(rhs.value())).0;
                Self {
                    value: $clamp_result,
                }
            }
        }

        #[cfg(feature = "num-traits")]
        impl<T: BuiltinInteger + $trait, const BITS: usize> num_traits::bounds::Bounded
            for $type<T, BITS>
        where
            Self: Integer,
        {
            #[inline]
            fn min_value() -> Self {
                Self::MIN
            }

            #[inline]
            fn max_value() -> Self {
                Self::MAX
            }
        }
    };
}

pub(crate) use impl_num_traits;

macro_rules! impl_bytemuck_basic {
    ($type:ident, $trait:ident $({
        $(#[$zeroable_attr:meta])*
        impl Zeroable for ... {}
        $(#[$no_uninit_attr:meta])*
        impl NoUninit for ... {}
        $(#[$checked_bit_pattern_attr:meta])*
        impl CheckedBitPattern for ... {}
    })?) => {
        #[cfg(feature = "bytemuck")]
        $($(#[$zeroable_attr])*)*
        unsafe impl<T: BuiltinInteger + bytemuck::Zeroable + $trait, const BITS: usize>
            bytemuck::Zeroable for $type<T, BITS>
        {
        }
        // There are never any uninitialized bytes, since a UInt/Int is a #[repr(transparent)] wrapper around T
        #[cfg(feature = "bytemuck")]
        $($(#[$no_uninit_attr])*)*
        unsafe impl<T: BuiltinInteger + bytemuck::NoUninit + $trait, const BITS: usize>
            bytemuck::NoUninit for $type<T, BITS>
        {
        }
        #[cfg(feature = "bytemuck")]
        $($(#[$checked_bit_pattern_attr])*)*
        unsafe impl<T: BuiltinInteger + bytemuck::AnyBitPattern + $trait, const BITS: usize>
            bytemuck::CheckedBitPattern for $type<T, BITS>
            where Self: Integer<UnderlyingType = T>
        {
            type Bits = T;

            #[inline]
            fn is_valid_bit_pattern(bits: &Self::Bits) -> bool {
                // this works for both signed & unsigned ints
                <Self as Integer>::try_new(*bits).is_ok()
            }
        }
    };
}
macro_rules! impl_bytemuck_full {
    ($type:ident, $trait:ident $({
        $(#[$contig_attr:meta])*
        impl Contiguous for ... {}
        $($extra:tt)*
    })?) => {
        $crate::common::impl_bytemuck_basic!($type, $trait $({
            $($extra)*
        })*);
        #[cfg(feature = "bytemuck")]
        $($(#[$contig_attr])*)*
        unsafe impl<T: BuiltinInteger + Copy + Ord + $trait + 'static, const BITS: usize>
            bytemuck::Contiguous for $type<T, BITS>
        where
            Self: Integer,
        {
            type Int = T;
            const MAX_VALUE: Self::Int = <Self as Integer>::MAX.value();
            const MIN_VALUE: Self::Int = <Self as Integer>::MIN.value();
        }
    };
}

pub(crate) use impl_bytemuck_basic;
pub(crate) use impl_bytemuck_full;

// [`bin_proto::Bits`] has a [`u32`] constant, whereas `$type<T, $bits>` has a [`usize`] constant.
// These cannot be cast in a const context, meaning that we cannot implement the traits for a
// generic bit width.
macro_rules! impl_bin_proto {
    ($type:tt, $trait:ident) => {
        impl_bin_proto!($type, $trait, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128);
    };
    ($type:tt, $trait:ident, $($bits:literal),+) => {
        $(
            #[cfg(feature = "bin-proto")]
            impl<T: BuiltinInteger + $trait, Ctx> bin_proto::BitEncode<Ctx> for $type<T, $bits>
            where
                Self: Integer,
                <Self as Integer>::UnderlyingType: bin_proto::BitEncode<Ctx, bin_proto::Bits<$bits>>,
            {
                fn encode<W, E>(&self, write: &mut W, ctx: &mut Ctx, (): ()) -> bin_proto::Result<()>
                where
                    W: bin_proto::BitWrite,
                    E: bin_proto::Endianness,
                {
                    bin_proto::BitEncode::encode::<_, E>(
                        &Integer::value(*self),
                        write,
                        ctx,
                        bin_proto::Bits::<$bits>,
                    )
                }
            }

            #[cfg(feature = "bin-proto")]
            impl<T: BuiltinInteger + $trait, Ctx> bin_proto::BitDecode<Ctx>
                for $type<T, $bits>
            where
                Self: Integer,
                <Self as Integer>::UnderlyingType: bin_proto::BitDecode<Ctx, bin_proto::Bits<$bits>>,
            {
                fn decode<R, E>(read: &mut R, ctx: &mut Ctx, (): ()) -> bin_proto::Result<Self>
                where
                    R: bin_proto::BitRead,
                    E: bin_proto::Endianness,
                {
                    Ok(Self::new(
                        <<Self as Integer>::UnderlyingType as bin_proto::BitDecode<_, _>>::decode::<
                            _,
                            E,
                        >(read, ctx, bin_proto::Bits::<$bits>)?,
                    ))
                }
            }
        )+
    };
}

pub(crate) use impl_bin_proto;
