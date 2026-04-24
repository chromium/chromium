//! Marker types for passing constants as type arguments.
//! 
//! # Example
//! 
//! This example emulates specialization,
//! eliding a `.clone()` call when the created array is only one element long.
//! 
//! ```rust
//! use typewit::{const_marker::Usize, TypeCmp, TypeEq};
//! 
//! let arr = [3u8, 5, 8];
//! 
//! assert_eq!(repeat(3), []);
//! assert_eq!(repeat(3), [3]);
//! assert_eq!(repeat(3), [3, 3]);
//! assert_eq!(repeat(3), [3, 3, 3]);
//! assert_eq!(repeat(3), [3, 3, 3, 3]);
//! 
//! 
//! fn repeat<T: Clone, const OUT: usize>(val: T) -> [T; OUT] {
//!     // `te_len` ìs a `TypeEq<Usize<OUT>, Usize<1>>`
//!     if let TypeCmp::Eq(te_len) = Usize::<OUT>.equals(Usize::<1>) {
//!         // This branch is ran when `OUT == 1`
//!         TypeEq::new::<T>()    // returns `TypeEq<T, T>`
//!             .in_array(te_len) // returns `TypeEq<[T; OUT], [T; 1]>`
//!             .to_left([val])   // goes from `[T; 1]` to `[T; OUT]`
//!     } else {
//!         // This branch is ran when `OUT != 1`
//!         [(); OUT].map(|_| val.clone())
//!     }
//! }
//! ```
//! 
//! 

use crate::{
    TypeEq,
    TypeNe,
};

mod const_marker_trait;

pub use const_marker_trait::*;


#[cfg(feature = "rust_1_83")]
mod const_marker_eq_traits;

#[cfg(feature = "rust_1_83")]
pub use const_marker_eq_traits::*;


#[cfg(feature = "serde")]
mod const_marker_serde_impls;

#[cfg(feature = "serde")]
pub(crate) use const_marker_serde_impls::*;


mod boolwit;

pub use boolwit::*;


#[cfg(feature = "adt_const_marker")]
mod slice_const_markers;

#[cfg(feature = "adt_const_marker")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "adt_const_marker")))]
pub use slice_const_markers::Str;

/// Marker types for `const FOO: &'static [T]` parameters.
#[cfg(feature = "adt_const_marker")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "adt_const_marker")))]
pub mod slice {
    pub use super::slice_const_markers::{
        BoolSlice,
        CharSlice,
        U8Slice,
        U16Slice,
        U32Slice,
        U64Slice,
        U128Slice,
        UsizeSlice,
        I8Slice,
        I16Slice,
        I32Slice,
        I64Slice,
        I128Slice,
        IsizeSlice,
        StrSlice,
    };
}

struct Helper<L, R>(L, R);


macro_rules! __const_eq_with {
    ($L:expr, $R:expr) => {
        $L == $R
    };
    ($L:expr, $R:expr, ($L2:ident, $R2:ident) $cmp:expr) => ({
        let $L2 = $L;
        let $R2 = $R;
        $cmp
    });
} pub(crate) use __const_eq_with;


macro_rules! declare_const_param_type {
    ($($params:tt)*) => {
        $crate::const_marker::__declare_const_param_type!{ $($params)* }

        #[cfg(feature = "rust_1_83")]
        $crate::const_marker::__const_marker_impls!{ $($params)* }
    }
} pub(crate) use declare_const_param_type;


macro_rules! __declare_const_param_type {
    (
        $(#[$struct_docs:meta])*
        $struct:ident($prim:ty) 
        $deser_type:ident,

        $(
            $(#[$eq_docs:meta])*
            fn equals $(($L:ident, $R:ident) $comparator:block)?;
        )?
    ) => {
        #[doc = concat!(
            "Marker type for passing `const VAL: ", stringify!($prim),
            "` as a type parameter."
        )]
        /// # Serde compatibility
        /// 
        /// When the `"serde"` feature is enabled, 
        /// this type is serialized/deserialized as the `VAL` const parameter.
        /// 
        #[cfg_attr(
            all(feature = "serde", feature = "serde_json"),
            doc = $crate::const_marker::__const_marker_deserialize_doc_example!(
                $deser_type
                $struct
                "rust"
            )
        )]
        #[cfg_attr(
            not(all(feature = "serde", feature = "serde_json")),
            doc = $crate::const_marker::__const_marker_deserialize_doc_example!(
                $deser_type
                $struct
                "ignore"
            )
        )]
        /// 
        $(#[$struct_docs])*
        #[derive(Copy, Clone)]
        pub struct $struct<const VAL: $prim>;

        impl<const VAL: $prim> crate::const_marker::ConstMarker for $struct<VAL> {
            const VAL: Self::Of = VAL;
            type Of = $prim;
        }

        impl<const L: $prim, const R: $prim> $crate::const_marker::Helper<$struct<L>, $struct<R>> {
            const EQ: Result<
                TypeEq<$struct<L>, $struct<R>>,
                TypeNe<$struct<L>, $struct<R>>,
            > = if crate::const_marker::__const_eq_with!(
                L,
                R
                $($(, ($L, $R) $comparator)?)?
            ) {
                // SAFETY: `L == R` (both are std types with sensible Eq impls)
                // therefore `$struct<L> == $struct<R>`
                unsafe {
                    Ok(TypeEq::<$struct<L>, $struct<R>>::new_unchecked())
                }
            } else {
                // SAFETY: `L != R` (both are std types with sensible Eq impls)
                // therefore `$struct<L> != $struct<R>`
                unsafe {
                    Err(TypeNe::<$struct<L>, $struct<R>>::new_unchecked())
                }
            };

            const EQUALS: crate::TypeCmp<$struct<L>, $struct<R>> = match Self::EQ {
                Ok(x) => crate::TypeCmp::Eq(x),
                Err(x) => crate::TypeCmp::Ne(x),
            };
        }

        impl<const VAL: $prim> $struct<VAL> {
            /// Compares `self` and `other` for equality.
            ///
            /// Returns:
            /// - `Ok(TypeEq)`: if `VAL == OTHER`
            /// - `Err(TypeNe)`: if `VAL != OTHER`
            ///
            #[inline(always)]
            #[deprecated(note = "superceeded by `equals` method", since = "1.8.0")]
            pub const fn eq<const OTHER: $prim>(
                self, 
                _other: $struct<OTHER>,
            ) -> Result<
                TypeEq<$struct<VAL>, $struct<OTHER>>,
                TypeNe<$struct<VAL>, $struct<OTHER>>,
            > {
                $crate::const_marker::Helper::<$struct<VAL>, $struct<OTHER>>::EQ
            }

            /// Compares `self` and `other` for equality.
            ///
            /// Returns:
            /// - `TypeCmp::Eq(TypeEq)`: if `VAL == OTHER`
            /// - `TypeCmp::Ne(TypeNe)`: if `VAL != OTHER`
            ///
            $($(#[$eq_docs])*)?
            #[inline(always)]
            pub const fn equals<const OTHER: $prim>(
                self, 
                _other: $struct<OTHER>,
            ) -> crate::TypeCmp<$struct<VAL>, $struct<OTHER>> {
                $crate::const_marker::Helper::<$struct<VAL>, $struct<OTHER>>::EQUALS
            }
        }

        /////////

        impl<const VAL: $prim> core::fmt::Debug for $struct<VAL> {
            fn fmt(&self, fmt: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
                core::fmt::Debug::fmt(&VAL, fmt)
            }
        }

        impl<const L: $prim, const R: $prim> core::cmp::PartialEq<$struct<R>> for $struct<L> {
            fn eq(&self, _: &$struct<R>) -> bool {
                L == R
            }
        }

        impl<const VAL: $prim> core::cmp::Eq for $struct<VAL> {}
        
        impl<const VAL: $prim> Default for $struct<VAL> {
            fn default() -> Self {
                Self
            }
        }

        impl<const L: $prim, const R: $prim> core::cmp::PartialOrd<$struct<R>> for $struct<L> {
            fn partial_cmp(&self, _: &$struct<R>) -> Option<core::cmp::Ordering> {
                Some(core::cmp::Ord::cmp(&L, &R))
            }
        }

        impl<const VAL: $prim> core::cmp::Ord for $struct<VAL> {
            fn cmp(&self, _: &Self) -> core::cmp::Ordering {
                core::cmp::Ordering::Equal
            }
        }

        impl<const VAL: $prim> core::hash::Hash for $struct<VAL> {
            fn hash<H>(&self, state: &mut H)
            where
                H: core::hash::Hasher,
            {
                core::hash::Hash::hash(&VAL, state)
            }
        }

        #[cfg(feature = "serde")]
        crate::const_marker::__declare_const_marker_serde_impls!{$struct($prim) $deser_type}


    };
} pub(crate) use __declare_const_param_type;


declare_const_param_type!{
    Bool(bool) primitive,

    /// 
    /// For getting a type witness that
    /// `Bool<B>` is either `Bool<true>` or `Bool<false>`,
    /// you can use [`BoolWit`].


    /// 
    fn equals;
}
declare_const_param_type!{Char(char) primitive,}

declare_const_param_type!{U8(u8) int,}
declare_const_param_type!{U16(u16) int,}
declare_const_param_type!{U32(u32) int,}
declare_const_param_type!{U64(u64) int,}
declare_const_param_type!{U128(u128) int,}

declare_const_param_type!{
    Usize(usize)
    int,

    /// # Examples
    /// 
    /// ### Array
    /// 
    /// This example demonstrates how `Usize` can be used to 
    /// specialize behavior on array length.
    /// 
    /// (this example requires Rust 1.61.0, because it uses trait bounds in const fns)
    #[cfg_attr(not(feature = "rust_1_61"), doc = "```ignore")]
    #[cfg_attr(feature = "rust_1_61", doc = "```rust")]
    /// use typewit::{const_marker::Usize, TypeCmp, TypeEq};
    /// 
    /// assert_eq!(try_from_pair::<_, 0>((3, 5)), Ok([]));
    /// assert_eq!(try_from_pair::<_, 1>((3, 5)), Ok([3]));
    /// assert_eq!(try_from_pair::<_, 2>((3, 5)), Ok([3, 5]));
    /// assert_eq!(try_from_pair::<_, 3>((3, 5)), Err((3, 5)));
    /// 
    /// 
    /// const fn try_from_pair<T: Copy, const LEN: usize>(pair: (T, T)) -> Result<[T; LEN], (T, T)> {
    ///     if let TypeCmp::Eq(te_len) = Usize::<LEN>.equals(Usize::<0>) {
    ///         // this branch is ran on `LEN == 0`
    ///         // `te_len` is a `TypeEq<Usize<LEN>, Usize<0>>`
    ///         Ok(
    ///             TypeEq::new::<T>()    // `TypeEq<T, T>`
    ///                 .in_array(te_len) // `TypeEq<[T; LEN], [T; 0]>`
    ///                 .to_left([])      // Goes from `[T; 0]` to `[T; LEN]`
    ///         )
    ///     } else if let TypeCmp::Eq(te_len) = Usize.equals(Usize) {
    ///         // this branch is ran on `LEN == 1`
    ///         // `te_len` is inferred to be `TypeEq<Usize<LEN>, Usize<1>>`
    ///         Ok(TypeEq::NEW.in_array(te_len).to_left([pair.0]))
    ///     } else if let TypeCmp::Eq(te_len) = Usize.equals(Usize) {
    ///         // this branch is ran on `LEN == 2`
    ///         // `te_len` is inferred to be `TypeEq<Usize<LEN>, Usize<2>>`
    ///         Ok(TypeEq::NEW.in_array(te_len).to_left([pair.0, pair.1]))
    ///     } else {
    ///         Err(pair)
    ///     }
    /// }
    /// 
    /// ```
    /// 
    /// ### Struct
    /// 
    /// This example demonstrates how `Usize` can be used to pass a 
    /// const-generic struct to a function expecting a concrete type of that struct.
    /// 
    /// ```rust
    /// use typewit::{const_marker::Usize, TypeCmp};
    /// 
    /// assert_eq!(mutate(Array([])), Array([]));
    /// assert_eq!(mutate(Array([3])), Array([3]));
    /// assert_eq!(mutate(Array([3, 5])), Array([3, 5]));
    /// assert_eq!(mutate(Array([3, 5, 8])), Array([8, 5, 3])); // reversed!
    /// assert_eq!(mutate(Array([3, 5, 8, 13])), Array([3, 5, 8, 13]));
    /// 
    /// 
    /// #[derive(Debug, PartialEq)]
    /// struct Array<const CAP: usize>([u32; CAP]);
    /// 
    /// const fn mutate<const LEN: usize>(arr: Array<LEN>) -> Array<LEN> {
    ///     match Usize::<LEN>.equals(Usize::<3>) {
    ///         // `te_len` is a `TypeEq<Usize<LEN>, Usize<3>>`
    ///         // this branch is ran on `LEN == 3`
    ///         TypeCmp::Eq(te_len) => {
    ///             // `te` is a `TypeEq<Array<LEN>, Array<3>>`
    ///             let te = te_len.project::<GArray>();
    /// 
    ///             // `te.to_right(...)` here goes from `Array<LEN>` to `Array<3>`
    ///             let ret = reverse3(te.to_right(arr));
    /// 
    ///             // `te.to_left(...)` here goes from `Array<3>` to `Array<LEN>`
    ///             te.to_left(ret)
    ///         }
    ///         TypeCmp::Ne(_) => arr,
    ///     }
    /// }
    /// 
    /// const fn reverse3(Array([a, b, c]): Array<3>) -> Array<3> {
    ///     Array([c, b, a])
    /// }
    /// 
    /// typewit::type_fn!{
    ///     // Type-level function from `Usize<LEN>` to `Array<LEN>`
    ///     struct GArray;
    /// 
    ///     impl<const LEN: usize> Usize<LEN> => Array<LEN>
    /// }
    /// ```
    fn equals;
}

declare_const_param_type!{I8(i8) int,}
declare_const_param_type!{I16(i16) int,}
declare_const_param_type!{I32(i32) int,}
declare_const_param_type!{I64(i64) int,}
declare_const_param_type!{I128(i128) int,}
declare_const_param_type!{Isize(isize) int,}












macro_rules! __const_marker_deserialize_doc_example {
    (int $struct:ident $example_ty:literal) => {concat!(
        "```", $example_ty, "\n",
        "use typewit::const_marker::", stringify!($struct), "; \n",
        " \n",
        "assert_eq!(serde_json::from_str::<",
        stringify!($struct), 
        "<1>>(\"1\").unwrap(), ",
        stringify!($struct),
        "::<1>); \n",
        " \n",
        "// trying to deserialize `", stringify!($struct),
        "<1>` from any value other than `1` produces an error \n",
        "assert!(serde_json::from_str::<", stringify!($struct), "<1>>(\"0\").is_err()); \n",
        " \n",
        " \n",
        "assert_eq!(serde_json::to_string(&", stringify!($struct), "::<1>).unwrap(), \"1\"); \n",
        " \n",
        "assert_eq!(serde_json::to_string(&", stringify!($struct), "::<92>).unwrap(), \"92\"); \n",
        " \n",
        "```"
    )};
    ($other:ident $struct:ident $example_ty:tt) => { "" };
}

pub(crate) use __const_marker_deserialize_doc_example;
