//! Implementation detail of the `konst` crate.
#![no_std]
#![cfg_attr(feature = "nightly_mut_refs", feature(const_mut_refs))]

#[cfg(feature = "rust_1_56")]
#[macro_use]
mod array_macros;

#[macro_use]
mod internal_macros;

pub mod into_iter;
#[doc(hidden)]
pub mod iter;

#[macro_use]
mod option_macros_;

#[macro_use]
mod result_macros_;

#[macro_use]
#[doc(hidden)]
pub mod slice_;

#[macro_use]
#[doc(hidden)]
pub mod string;

#[doc(hidden)]
pub mod utils;

#[doc(hidden)]
#[cfg(feature = "rust_1_56")]
mod type_eq;

#[doc(hidden)]
#[cfg(feature = "rust_1_56")]
pub mod collect_const;

#[doc(hidden)]
#[cfg(feature = "rust_1_56")]
pub mod utils_1_56;

#[doc(hidden)]
pub mod __ {
    pub use core::panic;

    #[cfg(feature = "rust_1_56")]
    pub use crate::{
        array_macros::{assert_array, uninit_array, AssumInitCopyArray},
        collect_const::{CollectorCmd, ComputedLength},
        type_eq::TypeEq,
    };

    pub use crate::into_iter::{EmptyIter, IntoIterWrapper, IsIntoIterKind};

    pub use core::{
        cmp::Ordering::{self, Equal, Greater, Less},
        compile_error, concat,
        marker::PhantomData,
        matches,
        mem::{transmute, ManuallyDrop, MaybeUninit},
        ops::Range,
        option::Option::{self, None, Some},
        primitive::{bool, str, u8, usize},
        result::Result::{self, Err, Ok},
        str::from_utf8_unchecked,
        stringify,
    };
}
