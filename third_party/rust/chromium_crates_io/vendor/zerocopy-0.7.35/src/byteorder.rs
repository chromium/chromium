// Copyright 2019 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

//! Byte order-aware numeric primitives.
//!
//! This module contains equivalents of the native multi-byte integer types with
//! no alignment requirement and supporting byte order conversions.
//!
//! For each native multi-byte integer type - `u16`, `i16`, `u32`, etc - and
//! floating point type - `f32` and `f64` - an equivalent type is defined by
//! this module - [`U16`], [`I16`], [`U32`], [`F64`], etc. Unlike their native
//! counterparts, these types have alignment 1, and take a type parameter
//! specifying the byte order in which the bytes are stored in memory. Each type
//! implements the [`FromBytes`], [`AsBytes`], and [`Unaligned`] traits.
//!
//! These two properties, taken together, make these types useful for defining
//! data structures whose memory layout matches a wire format such as that of a
//! network protocol or a file format. Such formats often have multi-byte values
//! at offsets that do not respect the alignment requirements of the equivalent
//! native types, and stored in a byte order not necessarily the same as that of
//! the target platform.
//!
//! Type aliases are provided for common byte orders in the [`big_endian`],
//! [`little_endian`], [`network_endian`], and [`native_endian`] submodules.
//!
//! # Example
//!
//! One use of these types is for representing network packet formats, such as
//! UDP:
//!
//! ```rust,edition2021
//! # #[cfg(feature = "derive")] { // This example uses derives, and won't compile without them
//! use zerocopy::{AsBytes, ByteSlice, FromBytes, FromZeroes, Ref, Unaligned};
//! use zerocopy::byteorder::network_endian::U16;
//!
//! #[derive(FromZeroes, FromBytes, AsBytes, Unaligned)]
//! #[repr(C)]
//! struct UdpHeader {
//!     src_port: U16,
//!     dst_port: U16,
//!     length: U16,
//!     checksum: U16,
//! }
//!
//! struct UdpPacket<B: ByteSlice> {
//!     header: Ref<B, UdpHeader>,
//!     body: B,
//! }
//!
//! impl<B: ByteSlice> UdpPacket<B> {
//!     fn parse(bytes: B) -> Option<UdpPacket<B>> {
//!         let (header, body) = Ref::new_from_prefix(bytes)?;
//!         Some(UdpPacket { header, body })
//!     }
//!
//!     fn src_port(&self) -> u16 {
//!         self.header.src_port.get()
//!     }
//!
//!     // more getters...
//! }
//! # }
//! ```

use core::{
    convert::{TryFrom, TryInto},
    fmt::{self, Binary, Debug, Display, Formatter, LowerHex, Octal, UpperHex},
    marker::PhantomData,
    num::TryFromIntError,
};

// We don't reexport `WriteBytesExt` or `ReadBytesExt` because those are only
// available with the `std` feature enabled, and zerocopy is `no_std` by
// default.
pub use ::byteorder::{BigEndian, ByteOrder, LittleEndian, NativeEndian, NetworkEndian, BE, LE};

use super::*;

macro_rules! impl_fmt_trait {
    ($name:ident, $native:ident, $trait:ident) => {
        impl<O: ByteOrder> $trait for $name<O> {
            #[inline(always)]
            fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
                $trait::fmt(&self.get(), f)
            }
        }
    };
}

macro_rules! impl_fmt_traits {
    ($name:ident, $native:ident, "floating point number") => {
        impl_fmt_trait!($name, $native, Display);
    };
    ($name:ident, $native:ident, "unsigned integer") => {
        impl_fmt_traits!($name, $native, @all_types);
    };
    ($name:ident, $native:ident, "signed integer") => {
        impl_fmt_traits!($name, $native, @all_types);
    };
    ($name:ident, $native:ident, @all_types) => {
        impl_fmt_trait!($name, $native, Display);
        impl_fmt_trait!($name, $native, Octal);
        impl_fmt_trait!($name, $native, LowerHex);
        impl_fmt_trait!($name, $native, UpperHex);
        impl_fmt_trait!($name, $native, Binary);
    };
}

macro_rules! impl_ops_traits {
    ($name:ident, $native:ident, "floating point number") => {
        impl_ops_traits!($name, $native, @all_types);
        impl_ops_traits!($name, $native, @signed_integer_floating_point);
    };
    ($name:ident, $native:ident, "unsigned integer") => {
        impl_ops_traits!($name, $native, @signed_unsigned_integer);
        impl_ops_traits!($name, $native, @all_types);
    };
    ($name:ident, $native:ident, "signed integer") => {
        impl_ops_traits!($name, $native, @signed_unsigned_integer);
        impl_ops_traits!($name, $native, @signed_integer_floating_point);
        impl_ops_traits!($name, $native, @all_types);
    };
    ($name:ident, $native:ident, @signed_unsigned_integer) => {
        impl_ops_traits!(@without_byteorder_swap $name, $native, BitAnd, bitand, BitAndAssign, bitand_assign);
        impl_ops_traits!(@without_byteorder_swap $name, $native, BitOr, bitor, BitOrAssign, bitor_assign);
        impl_ops_traits!(@without_byteorder_swap $name, $native, BitXor, bitxor, BitXorAssign, bitxor_assign);
        impl_ops_traits!(@with_byteorder_swap $name, $native, Shl, shl, ShlAssign, shl_assign);
        impl_ops_traits!(@with_byteorder_swap $name, $native, Shr, shr, ShrAssign, shr_assign);

        impl<O> core::ops::Not for $name<O> {
            type Output = $name<O>;

            #[inline(always)]
            fn not(self) -> $name<O> {
                 let self_native = $native::from_ne_bytes(self.0);
                 $name((!self_native).to_ne_bytes(), PhantomData)
            }
        }
    };
    ($name:ident, $native:ident, @signed_integer_floating_point) => {
        impl<O: ByteOrder> core::ops::Neg for $name<O> {
            type Output = $name<O>;

            #[inline(always)]
            fn neg(self) -> $name<O> {
                let self_native: $native = self.get();
                #[allow(clippy::arithmetic_side_effects)]
                $name::<O>::new(-self_native)
            }
        }
    };
    ($name:ident, $native:ident, @all_types) => {
        impl_ops_traits!(@with_byteorder_swap $name, $native, Add, add, AddAssign, add_assign);
        impl_ops_traits!(@with_byteorder_swap $name, $native, Div, div, DivAssign, div_assign);
        impl_ops_traits!(@with_byteorder_swap $name, $native, Mul, mul, MulAssign, mul_assign);
        impl_ops_traits!(@with_byteorder_swap $name, $native, Rem, rem, RemAssign, rem_assign);
        impl_ops_traits!(@with_byteorder_swap $name, $native, Sub, sub, SubAssign, sub_assign);
    };
    (@with_byteorder_swap $name:ident, $native:ident, $trait:ident, $method:ident, $trait_assign:ident, $method_assign:ident) => {
        impl<O: ByteOrder> core::ops::$trait for $name<O> {
            type Output = $name<O>;

            #[inline(always)]
            fn $method(self, rhs: $name<O>) -> $name<O> {
                let self_native: $native = self.get();
                let rhs_native: $native = rhs.get();
                let result_native = core::ops::$trait::$method(self_native, rhs_native);
                $name::<O>::new(result_native)
            }
        }

        impl<O: ByteOrder> core::ops::$trait_assign for $name<O> {
            #[inline(always)]
            fn $method_assign(&mut self, rhs: $name<O>) {
                *self = core::ops::$trait::$method(*self, rhs);
            }
        }
    };
    // Implement traits in terms of the same trait on the native type, but
    // without performing a byte order swap. This only works for bitwise
    // operations like `&`, `|`, etc.
    (@without_byteorder_swap $name:ident, $native:ident, $trait:ident, $method:ident, $trait_assign:ident, $method_assign:ident) => {
        impl<O: ByteOrder> core::ops::$trait for $name<O> {
            type Output = $name<O>;

            #[inline(always)]
            fn $method(self, rhs: $name<O>) -> $name<O> {
                let self_native = $native::from_ne_bytes(self.0);
                let rhs_native = $native::from_ne_bytes(rhs.0);
                let result_native = core::ops::$trait::$method(self_native, rhs_native);
                $name(result_native.to_ne_bytes(), PhantomData)
            }
        }

        impl<O: ByteOrder> core::ops::$trait_assign for $name<O> {
            #[inline(always)]
            fn $method_assign(&mut self, rhs: $name<O>) {
                *self = core::ops::$trait::$method(*self, rhs);
            }
        }
    };
}

macro_rules! doc_comment {
    ($x:expr, $($tt:tt)*) => {
        #[doc = $x]
        $($tt)*
    };
}

macro_rules! define_max_value_constant {
    ($name:ident, $bytes:expr, "unsigned integer") => {
        /// The maximum value.
        ///
        /// This constant should be preferred to constructing a new value using
        /// `new`, as `new` may perform an endianness swap depending on the
        /// endianness `O` and the endianness of the platform.
        pub const MAX_VALUE: $name<O> = $name([0xFFu8; $bytes], PhantomData);
    };
    // We don't provide maximum and minimum value constants for signed values
    // and floats because there's no way to do it generically - it would require
    // a different value depending on the value of the `ByteOrder` type
    // parameter. Currently, one workaround would be to provide implementations
    // for concrete implementations of that trait. In the long term, if we are
    // ever able to make the `new` constructor a const fn, we could use that
    // instead.
    ($name:ident, $bytes:expr, "signed integer") => {};
    ($name:ident, $bytes:expr, "floating point number") => {};
}

macro_rules! define_type {
    ($article:ident,
        $name:ident,
        $native:ident,
        $bits:expr,
        $bytes:expr,
        $read_method:ident,
        $write_method:ident,
        $number_kind:tt,
        [$($larger_native:ty),*],
        [$($larger_native_try:ty),*],
        [$($larger_byteorder:ident),*],
        [$($larger_byteorder_try:ident),*]) => {
        doc_comment! {
            concat!("A ", stringify!($bits), "-bit ", $number_kind,
            " stored in a given byte order.

`", stringify!($name), "` is like the native `", stringify!($native), "` type with
two major differences: First, it has no alignment requirement (its alignment is 1).
Second, the endianness of its memory layout is given by the type parameter `O`,
which can be any type which implements [`ByteOrder`]. In particular, this refers
to [`BigEndian`], [`LittleEndian`], [`NativeEndian`], and [`NetworkEndian`].

", stringify!($article), " `", stringify!($name), "` can be constructed using
the [`new`] method, and its contained value can be obtained as a native
`",stringify!($native), "` using the [`get`] method, or updated in place with
the [`set`] method. In all cases, if the endianness `O` is not the same as the
endianness of the current platform, an endianness swap will be performed in
order to uphold the invariants that a) the layout of `", stringify!($name), "`
has endianness `O` and that, b) the layout of `", stringify!($native), "` has
the platform's native endianness.

`", stringify!($name), "` implements [`FromBytes`], [`AsBytes`], and [`Unaligned`],
making it useful for parsing and serialization. See the module documentation for an
example of how it can be used for parsing UDP packets.

[`new`]: crate::byteorder::", stringify!($name), "::new
[`get`]: crate::byteorder::", stringify!($name), "::get
[`set`]: crate::byteorder::", stringify!($name), "::set
[`FromBytes`]: crate::FromBytes
[`AsBytes`]: crate::AsBytes
[`Unaligned`]: crate::Unaligned"),
            #[derive(Copy, Clone, Eq, PartialEq, Hash)]
            #[cfg_attr(any(feature = "derive", test), derive(KnownLayout, FromZeroes, FromBytes, AsBytes, Unaligned))]
            #[repr(transparent)]
            pub struct $name<O>([u8; $bytes], PhantomData<O>);
        }

        #[cfg(not(any(feature = "derive", test)))]
        impl_known_layout!(O => $name<O>);

        safety_comment! {
            /// SAFETY:
            /// `$name<O>` is `repr(transparent)`, and so it has the same layout
            /// as its only non-zero field, which is a `u8` array. `u8` arrays
            /// are `FromZeroes`, `FromBytes`, `AsBytes`, and `Unaligned`.
            impl_or_verify!(O => FromZeroes for $name<O>);
            impl_or_verify!(O => FromBytes for $name<O>);
            impl_or_verify!(O => AsBytes for $name<O>);
            impl_or_verify!(O => Unaligned for $name<O>);
        }

        impl<O> Default for $name<O> {
            #[inline(always)]
            fn default() -> $name<O> {
                $name::ZERO
            }
        }

        impl<O> $name<O> {
            /// The value zero.
            ///
            /// This constant should be preferred to constructing a new value
            /// using `new`, as `new` may perform an endianness swap depending
            /// on the endianness and platform.
            pub const ZERO: $name<O> = $name([0u8; $bytes], PhantomData);

            define_max_value_constant!($name, $bytes, $number_kind);

            /// Constructs a new value from bytes which are already in the
            /// endianness `O`.
            #[inline(always)]
            pub const fn from_bytes(bytes: [u8; $bytes]) -> $name<O> {
                $name(bytes, PhantomData)
            }
        }

        impl<O: ByteOrder> $name<O> {
            // TODO(joshlf): Make these const fns if the `ByteOrder` methods
            // ever become const fns.

            /// Constructs a new value, possibly performing an endianness swap
            /// to guarantee that the returned value has endianness `O`.
            #[inline(always)]
            pub fn new(n: $native) -> $name<O> {
                let mut out = $name::default();
                O::$write_method(&mut out.0[..], n);
                out
            }

            /// Returns the value as a primitive type, possibly performing an
            /// endianness swap to guarantee that the return value has the
            /// endianness of the native platform.
            #[inline(always)]
            pub fn get(self) -> $native {
                O::$read_method(&self.0[..])
            }

            /// Updates the value in place as a primitive type, possibly
            /// performing an endianness swap to guarantee that the stored value
            /// has the endianness `O`.
            #[inline(always)]
            pub fn set(&mut self, n: $native) {
                O::$write_method(&mut self.0[..], n);
            }
        }

        // The reasoning behind which traits to implement here is to only
        // implement traits which won't cause inference issues. Notably,
        // comparison traits like PartialEq and PartialOrd tend to cause
        // inference issues.

        impl<O: ByteOrder> From<$name<O>> for [u8; $bytes] {
            #[inline(always)]
            fn from(x: $name<O>) -> [u8; $bytes] {
                x.0
            }
        }

        impl<O: ByteOrder> From<[u8; $bytes]> for $name<O> {
            #[inline(always)]
            fn from(bytes: [u8; $bytes]) -> $name<O> {
                $name(bytes, PhantomData)
            }
        }

        impl<O: ByteOrder> From<$name<O>> for $native {
            #[inline(always)]
            fn from(x: $name<O>) -> $native {
                x.get()
            }
        }

        impl<O: ByteOrder> From<$native> for $name<O> {
            #[inline(always)]
            fn from(x: $native) -> $name<O> {
                $name::new(x)
            }
        }

        $(
            impl<O: ByteOrder> From<$name<O>> for $larger_native {
                #[inline(always)]
                fn from(x: $name<O>) -> $larger_native {
                    x.get().into()
                }
            }
        )*

        $(
            impl<O: ByteOrder> TryFrom<$larger_native_try> for $name<O> {
                type Error = TryFromIntError;
                #[inline(always)]
                fn try_from(x: $larger_native_try) -> Result<$name<O>, TryFromIntError> {
                    $native::try_from(x).map($name::new)
                }
            }
        )*

        $(
            impl<O: ByteOrder, P: ByteOrder> From<$name<O>> for $larger_byteorder<P> {
                #[inline(always)]
                fn from(x: $name<O>) -> $larger_byteorder<P> {
                    $larger_byteorder::new(x.get().into())
                }
            }
        )*

        $(
            impl<O: ByteOrder, P: ByteOrder> TryFrom<$larger_byteorder_try<P>> for $name<O> {
                type Error = TryFromIntError;
                #[inline(always)]
                fn try_from(x: $larger_byteorder_try<P>) -> Result<$name<O>, TryFromIntError> {
                    x.get().try_into().map($name::new)
                }
            }
        )*

        impl<O: ByteOrder> AsRef<[u8; $bytes]> for $name<O> {
            #[inline(always)]
            fn as_ref(&self) -> &[u8; $bytes] {
                &self.0
            }
        }

        impl<O: ByteOrder> AsMut<[u8; $bytes]> for $name<O> {
            #[inline(always)]
            fn as_mut(&mut self) -> &mut [u8; $bytes] {
                &mut self.0
            }
        }

        impl<O: ByteOrder> PartialEq<$name<O>> for [u8; $bytes] {
            #[inline(always)]
            fn eq(&self, other: &$name<O>) -> bool {
                self.eq(&other.0)
            }
        }

        impl<O: ByteOrder> PartialEq<[u8; $bytes]> for $name<O> {
            #[inline(always)]
            fn eq(&self, other: &[u8; $bytes]) -> bool {
                self.0.eq(other)
            }
        }

        impl_fmt_traits!($name, $native, $number_kind);
        impl_ops_traits!($name, $native, $number_kind);

        impl<O: ByteOrder> Debug for $name<O> {
            #[inline]
            fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
                // This results in a format like "U16(42)".
                f.debug_tuple(stringify!($name)).field(&self.get()).finish()
            }
        }
    };
}

define_type!(
    A,
    U16,
    u16,
    16,
    2,
    read_u16,
    write_u16,
    "unsigned integer",
    [u32, u64, u128, usize],
    [u32, u64, u128, usize],
    [U32, U64, U128],
    [U32, U64, U128]
);
define_type!(
    A,
    U32,
    u32,
    32,
    4,
    read_u32,
    write_u32,
    "unsigned integer",
    [u64, u128],
    [u64, u128],
    [U64, U128],
    [U64, U128]
);
define_type!(
    A,
    U64,
    u64,
    64,
    8,
    read_u64,
    write_u64,
    "unsigned integer",
    [u128],
    [u128],
    [U128],
    [U128]
);
define_type!(A, U128, u128, 128, 16, read_u128, write_u128, "unsigned integer", [], [], [], []);
define_type!(
    An,
    I16,
    i16,
    16,
    2,
    read_i16,
    write_i16,
    "signed integer",
    [i32, i64, i128, isize],
    [i32, i64, i128, isize],
    [I32, I64, I128],
    [I32, I64, I128]
);
define_type!(
    An,
    I32,
    i32,
    32,
    4,
    read_i32,
    write_i32,
    "signed integer",
    [i64, i128],
    [i64, i128],
    [I64, I128],
    [I64, I128]
);
define_type!(
    An,
    I64,
    i64,
    64,
    8,
    read_i64,
    write_i64,
    "signed integer",
    [i128],
    [i128],
    [I128],
    [I128]
);
define_type!(An, I128, i128, 128, 16, read_i128, write_i128, "signed integer", [], [], [], []);
define_type!(
    An,
    F32,
    f32,
    32,
    4,
    read_f32,
    write_f32,
    "floating point number",
    [f64],
    [],
    [F64],
    []
);
define_type!(An, F64, f64, 64, 8, read_f64, write_f64, "floating point number", [], [], [], []);

macro_rules! module {
    ($name:ident, $trait:ident, $endianness_str:expr) => {
        /// Numeric primitives stored in
        #[doc = $endianness_str]
        /// byte order.
        pub mod $name {
            use byteorder::$trait;

            module!(@ty U16,  $trait, "16-bit unsigned integer", $endianness_str);
            module!(@ty U32,  $trait, "32-bit unsigned integer", $endianness_str);
            module!(@ty U64,  $trait, "64-bit unsigned integer", $endianness_str);
            module!(@ty U128, $trait, "128-bit unsigned integer", $endianness_str);
            module!(@ty I16,  $trait, "16-bit signed integer", $endianness_str);
            module!(@ty I32,  $trait, "32-bit signed integer", $endianness_str);
            module!(@ty I64,  $trait, "64-bit signed integer", $endianness_str);
            module!(@ty I128, $trait, "128-bit signed integer", $endianness_str);
            module!(@ty F32,  $trait, "32-bit floating point number", $endianness_str);
            module!(@ty F64,  $trait, "64-bit floating point number", $endianness_str);
        }
    };
    (@ty $ty:ident, $trait:ident, $desc_str:expr, $endianness_str:expr) => {
        /// A
        #[doc = $desc_str]
        /// stored in
        #[doc = $endianness_str]
        /// byte order.
        pub type $ty = crate::byteorder::$ty<$trait>;
    };
}

module!(big_endian, BigEndian, "big-endian");
module!(little_endian, LittleEndian, "little-endian");
module!(network_endian, NetworkEndian, "network-endian");
module!(native_endian, NativeEndian, "native-endian");

#[cfg(any(test, kani))]
mod tests {
    use ::byteorder::NativeEndian;

    use {
        super::*,
        crate::{AsBytes, FromBytes, Unaligned},
    };

    #[cfg(not(kani))]
    mod compatibility {
        pub(super) use rand::{
            distributions::{Distribution, Standard},
            rngs::SmallRng,
            Rng, SeedableRng,
        };

        pub(crate) trait Arbitrary {}

        impl<T> Arbitrary for T {}
    }

    #[cfg(kani)]
    mod compatibility {
        pub(crate) use kani::Arbitrary;

        pub(crate) struct SmallRng;

        impl SmallRng {
            pub(crate) fn seed_from_u64(_state: u64) -> Self {
                Self
            }
        }

        pub(crate) trait Rng {
            fn sample<T, D: Distribution<T>>(&mut self, _distr: D) -> T
            where
                T: Arbitrary,
            {
                kani::any()
            }
        }

        impl Rng for SmallRng {}

        pub(crate) trait Distribution<T> {}
        impl<T, U> Distribution<T> for U {}

        pub(crate) struct Standard;
    }

    use compatibility::*;

    // A native integer type (u16, i32, etc).
    #[cfg_attr(kani, allow(dead_code))]
    trait Native: Arbitrary + FromBytes + AsBytes + Copy + PartialEq + Debug {
        const ZERO: Self;
        const MAX_VALUE: Self;

        type Distribution: Distribution<Self>;
        const DIST: Self::Distribution;

        fn rand<R: Rng>(rng: &mut R) -> Self {
            rng.sample(Self::DIST)
        }

        fn checked_add(self, rhs: Self) -> Option<Self>;
        fn checked_div(self, rhs: Self) -> Option<Self>;
        fn checked_mul(self, rhs: Self) -> Option<Self>;
        fn checked_rem(self, rhs: Self) -> Option<Self>;
        fn checked_sub(self, rhs: Self) -> Option<Self>;
        fn checked_shl(self, rhs: Self) -> Option<Self>;
        fn checked_shr(self, rhs: Self) -> Option<Self>;

        fn is_nan(self) -> bool;

        /// For `f32` and `f64`, NaN values are not considered equal to
        /// themselves. This method is like `assert_eq!`, but it treats NaN
        /// values as equal.
        fn assert_eq_or_nan(self, other: Self) {
            let slf = (!self.is_nan()).then(|| self);
            let other = (!other.is_nan()).then(|| other);
            assert_eq!(slf, other);
        }
    }

    trait ByteArray:
        FromBytes + AsBytes + Copy + AsRef<[u8]> + AsMut<[u8]> + Debug + Default + Eq
    {
        /// Invert the order of the bytes in the array.
        fn invert(self) -> Self;
    }

    trait ByteOrderType: FromBytes + AsBytes + Unaligned + Copy + Eq + Debug {
        type Native: Native;
        type ByteArray: ByteArray;

        const ZERO: Self;

        fn new(native: Self::Native) -> Self;
        fn get(self) -> Self::Native;
        fn set(&mut self, native: Self::Native);
        fn from_bytes(bytes: Self::ByteArray) -> Self;
        fn into_bytes(self) -> Self::ByteArray;

        /// For `f32` and `f64`, NaN values are not considered equal to
        /// themselves. This method is like `assert_eq!`, but it treats NaN
        /// values as equal.
        fn assert_eq_or_nan(self, other: Self) {
            let slf = (!self.get().is_nan()).then(|| self);
            let other = (!other.get().is_nan()).then(|| other);
            assert_eq!(slf, other);
        }
    }

    trait ByteOrderTypeUnsigned: ByteOrderType {
        const MAX_VALUE: Self;
    }

    macro_rules! impl_byte_array {
        ($bytes:expr) => {
            impl ByteArray for [u8; $bytes] {
                fn invert(mut self) -> [u8; $bytes] {
                    self.reverse();
                    self
                }
            }
        };
    }

    impl_byte_array!(2);
    impl_byte_array!(4);
    impl_byte_array!(8);
    impl_byte_array!(16);

    macro_rules! impl_byte_order_type_unsigned {
        ($name:ident, unsigned) => {
            impl<O: ByteOrder> ByteOrderTypeUnsigned for $name<O> {
                const MAX_VALUE: $name<O> = $name::MAX_VALUE;
            }
        };
        ($name:ident, signed) => {};
    }

    macro_rules! impl_traits {
        ($name:ident, $native:ident, $bytes:expr, $sign:ident $(, @$float:ident)?) => {
            impl Native for $native {
                // For some types, `0 as $native` is required (for example, when
                // `$native` is a floating-point type; `0` is an integer), but
                // for other types, it's a trivial cast. In all cases, Clippy
                // thinks it's dangerous.
                #[allow(trivial_numeric_casts, clippy::as_conversions)]
                const ZERO: $native = 0 as $native;
                const MAX_VALUE: $native = $native::MAX;

                type Distribution = Standard;
                const DIST: Standard = Standard;

                impl_traits!(@float_dependent_methods $(@$float)?);
            }

            impl<O: ByteOrder> ByteOrderType for $name<O> {
                type Native = $native;
                type ByteArray = [u8; $bytes];

                const ZERO: $name<O> = $name::ZERO;

                fn new(native: $native) -> $name<O> {
                    $name::new(native)
                }

                fn get(self) -> $native {
                    $name::get(self)
                }

                fn set(&mut self, native: $native) {
                    $name::set(self, native)
                }

                fn from_bytes(bytes: [u8; $bytes]) -> $name<O> {
                    $name::from(bytes)
                }

                fn into_bytes(self) -> [u8; $bytes] {
                    <[u8; $bytes]>::from(self)
                }
            }

            impl_byte_order_type_unsigned!($name, $sign);
        };
        (@float_dependent_methods) => {
            fn checked_add(self, rhs: Self) -> Option<Self> { self.checked_add(rhs) }
            fn checked_div(self, rhs: Self) -> Option<Self> { self.checked_div(rhs) }
            fn checked_mul(self, rhs: Self) -> Option<Self> { self.checked_mul(rhs) }
            fn checked_rem(self, rhs: Self) -> Option<Self> { self.checked_rem(rhs) }
            fn checked_sub(self, rhs: Self) -> Option<Self> { self.checked_sub(rhs) }
            fn checked_shl(self, rhs: Self) -> Option<Self> { self.checked_shl(rhs.try_into().unwrap_or(u32::MAX)) }
            fn checked_shr(self, rhs: Self) -> Option<Self> { self.checked_shr(rhs.try_into().unwrap_or(u32::MAX)) }
            fn is_nan(self) -> bool { false }
        };
        (@float_dependent_methods @float) => {
            fn checked_add(self, rhs: Self) -> Option<Self> { Some(self + rhs) }
            fn checked_div(self, rhs: Self) -> Option<Self> { Some(self / rhs) }
            fn checked_mul(self, rhs: Self) -> Option<Self> { Some(self * rhs) }
            fn checked_rem(self, rhs: Self) -> Option<Self> { Some(self % rhs) }
            fn checked_sub(self, rhs: Self) -> Option<Self> { Some(self - rhs) }
            fn checked_shl(self, _rhs: Self) -> Option<Self> { unimplemented!() }
            fn checked_shr(self, _rhs: Self) -> Option<Self> { unimplemented!() }
            fn is_nan(self) -> bool { self.is_nan() }
        };
    }

    impl_traits!(U16, u16, 2, unsigned);
    impl_traits!(U32, u32, 4, unsigned);
    impl_traits!(U64, u64, 8, unsigned);
    impl_traits!(U128, u128, 16, unsigned);
    impl_traits!(I16, i16, 2, signed);
    impl_traits!(I32, i32, 4, signed);
    impl_traits!(I64, i64, 8, signed);
    impl_traits!(I128, i128, 16, signed);
    impl_traits!(F32, f32, 4, signed, @float);
    impl_traits!(F64, f64, 8, signed, @float);

    macro_rules! call_for_unsigned_types {
        ($fn:ident, $byteorder:ident) => {
            $fn::<U16<$byteorder>>();
            $fn::<U32<$byteorder>>();
            $fn::<U64<$byteorder>>();
            $fn::<U128<$byteorder>>();
        };
    }

    macro_rules! call_for_signed_types {
        ($fn:ident, $byteorder:ident) => {
            $fn::<I16<$byteorder>>();
            $fn::<I32<$byteorder>>();
            $fn::<I64<$byteorder>>();
            $fn::<I128<$byteorder>>();
        };
    }

    macro_rules! call_for_float_types {
        ($fn:ident, $byteorder:ident) => {
            $fn::<F32<$byteorder>>();
            $fn::<F64<$byteorder>>();
        };
    }

    macro_rules! call_for_all_types {
        ($fn:ident, $byteorder:ident) => {
            call_for_unsigned_types!($fn, $byteorder);
            call_for_signed_types!($fn, $byteorder);
            call_for_float_types!($fn, $byteorder);
        };
    }

    #[cfg(target_endian = "big")]
    type NonNativeEndian = LittleEndian;
    #[cfg(target_endian = "little")]
    type NonNativeEndian = BigEndian;

    // We use a `u64` seed so that we can use `SeedableRng::seed_from_u64`.
    // `SmallRng`'s `SeedableRng::Seed` differs by platform, so if we wanted to
    // call `SeedableRng::from_seed`, which takes a `Seed`, we would need
    // conditional compilation by `target_pointer_width`.
    const RNG_SEED: u64 = 0x7A03CAE2F32B5B8F;

    const RAND_ITERS: usize = if cfg!(any(miri, kani)) {
        // The tests below which use this constant used to take a very long time
        // on Miri, which slows down local development and CI jobs. We're not
        // using Miri to check for the correctness of our code, but rather its
        // soundness, and at least in the context of these particular tests, a
        // single loop iteration is just as good for surfacing UB as multiple
        // iterations are.
        //
        // As of the writing of this comment, here's one set of measurements:
        //
        //   $ # RAND_ITERS == 1
        //   $ cargo miri test -- -Z unstable-options --report-time endian
        //   test byteorder::tests::test_native_endian ... ok <0.049s>
        //   test byteorder::tests::test_non_native_endian ... ok <0.061s>
        //
        //   $ # RAND_ITERS == 1024
        //   $ cargo miri test -- -Z unstable-options --report-time endian
        //   test byteorder::tests::test_native_endian ... ok <25.716s>
        //   test byteorder::tests::test_non_native_endian ... ok <38.127s>
        1
    } else {
        1024
    };

    #[cfg_attr(test, test)]
    #[cfg_attr(kani, kani::proof)]
    fn test_zero() {
        fn test_zero<T: ByteOrderType>() {
            assert_eq!(T::ZERO.get(), T::Native::ZERO);
        }

        call_for_all_types!(test_zero, NativeEndian);
        call_for_all_types!(test_zero, NonNativeEndian);
    }

    #[cfg_attr(test, test)]
    #[cfg_attr(kani, kani::proof)]
    fn test_max_value() {
        fn test_max_value<T: ByteOrderTypeUnsigned>() {
            assert_eq!(T::MAX_VALUE.get(), T::Native::MAX_VALUE);
        }

        call_for_unsigned_types!(test_max_value, NativeEndian);
        call_for_unsigned_types!(test_max_value, NonNativeEndian);
    }

    #[cfg_attr(test, test)]
    #[cfg_attr(kani, kani::proof)]
    fn test_endian() {
        fn test<T: ByteOrderType>(invert: bool) {
            let mut r = SmallRng::seed_from_u64(RNG_SEED);
            for _ in 0..RAND_ITERS {
                let native = T::Native::rand(&mut r);
                let mut bytes = T::ByteArray::default();
                bytes.as_bytes_mut().copy_from_slice(native.as_bytes());
                if invert {
                    bytes = bytes.invert();
                }
                let mut from_native = T::new(native);
                let from_bytes = T::from_bytes(bytes);

                from_native.assert_eq_or_nan(from_bytes);
                from_native.get().assert_eq_or_nan(native);
                from_bytes.get().assert_eq_or_nan(native);

                assert_eq!(from_native.into_bytes(), bytes);
                assert_eq!(from_bytes.into_bytes(), bytes);

                let updated = T::Native::rand(&mut r);
                from_native.set(updated);
                from_native.get().assert_eq_or_nan(updated);
            }
        }

        fn test_native<T: ByteOrderType>() {
            test::<T>(false);
        }

        fn test_non_native<T: ByteOrderType>() {
            test::<T>(true);
        }

        call_for_all_types!(test_native, NativeEndian);
        call_for_all_types!(test_non_native, NonNativeEndian);
    }

    #[test]
    fn test_ops_impls() {
        // Test implementations of traits in `core::ops`. Some of these are
        // fairly banal, but some are optimized to perform the operation without
        // swapping byte order (namely, bit-wise operations which are identical
        // regardless of byte order). These are important to test, and while
        // we're testing those anyway, it's trivial to test all of the impls.

        fn test<T, F, G, H>(op: F, op_native: G, op_native_checked: Option<H>)
        where
            T: ByteOrderType,
            F: Fn(T, T) -> T,
            G: Fn(T::Native, T::Native) -> T::Native,
            H: Fn(T::Native, T::Native) -> Option<T::Native>,
        {
            let mut r = SmallRng::seed_from_u64(RNG_SEED);
            for _ in 0..RAND_ITERS {
                let n0 = T::Native::rand(&mut r);
                let n1 = T::Native::rand(&mut r);
                let t0 = T::new(n0);
                let t1 = T::new(n1);

                // If this operation would overflow/underflow, skip it rather
                // than attempt to catch and recover from panics.
                if matches!(&op_native_checked, Some(checked) if checked(n0, n1).is_none()) {
                    continue;
                }

                let n_res = op_native(n0, n1);
                let t_res = op(t0, t1);

                // For `f32` and `f64`, NaN values are not considered equal to
                // themselves. We store `Option<f32>`/`Option<f64>` and store
                // NaN as `None` so they can still be compared.
                let n_res = (!T::Native::is_nan(n_res)).then(|| n_res);
                let t_res = (!T::Native::is_nan(t_res.get())).then(|| t_res.get());
                assert_eq!(n_res, t_res);
            }
        }

        macro_rules! test {
            (@binary $trait:ident, $method:ident $([$checked_method:ident])?, $($call_for_macros:ident),*) => {{
                test!(
                    @inner $trait,
                    core::ops::$trait::$method,
                    core::ops::$trait::$method,
                    {
                        #[allow(unused_mut, unused_assignments)]
                        let mut op_native_checked = None::<fn(T::Native, T::Native) -> Option<T::Native>>;
                        $(
                            op_native_checked = Some(T::Native::$checked_method);
                        )?
                        op_native_checked
                    },
                    $($call_for_macros),*
                );
            }};
            (@unary $trait:ident, $method:ident $([$checked_method:ident])?, $($call_for_macros:ident),*) => {{
                test!(
                    @inner $trait,
                    |slf, _rhs| core::ops::$trait::$method(slf),
                    |slf, _rhs| core::ops::$trait::$method(slf),
                    {
                        #[allow(unused_mut, unused_assignments)]
                        let mut op_native_checked = None::<fn(T::Native, T::Native) -> Option<T::Native>>;
                        $(
                            op_native_checked = Some(|slf, _rhs| T::Native::$checked_method(slf));
                        )?
                        op_native_checked
                    },
                    $($call_for_macros),*
                );
            }};
            (@inner $trait:ident, $op:expr, $op_native:expr, $op_native_checked:expr, $($call_for_macros:ident),*) => {{
                fn t<T: ByteOrderType + core::ops::$trait<Output = T>>()
                where
                    T::Native: core::ops::$trait<Output = T::Native>,
                {
                    test::<T, _, _, _>(
                        $op,
                        $op_native,
                        $op_native_checked,
                    );
                }

                $(
                    $call_for_macros!(t, NativeEndian);
                    $call_for_macros!(t, NonNativeEndian);
                )*
            }};
        }

        test!(@binary Add, add[checked_add], call_for_all_types);
        test!(@binary Div, div[checked_div], call_for_all_types);
        test!(@binary Mul, mul[checked_mul], call_for_all_types);
        test!(@binary Rem, rem[checked_rem], call_for_all_types);
        test!(@binary Sub, sub[checked_sub], call_for_all_types);

        test!(@binary BitAnd, bitand, call_for_unsigned_types, call_for_signed_types);
        test!(@binary BitOr, bitor, call_for_unsigned_types, call_for_signed_types);
        test!(@binary BitXor, bitxor, call_for_unsigned_types, call_for_signed_types);
        test!(@binary Shl, shl[checked_shl], call_for_unsigned_types, call_for_signed_types);
        test!(@binary Shr, shr[checked_shr], call_for_unsigned_types, call_for_signed_types);

        test!(@unary Not, not, call_for_signed_types, call_for_unsigned_types);
        test!(@unary Neg, neg, call_for_signed_types, call_for_float_types);
    }

    #[test]
    fn test_debug_impl() {
        // Ensure that Debug applies format options to the inner value.
        let val = U16::<LE>::new(10);
        assert_eq!(format!("{:?}", val), "U16(10)");
        assert_eq!(format!("{:03?}", val), "U16(010)");
        assert_eq!(format!("{:x?}", val), "U16(a)");
    }
}
