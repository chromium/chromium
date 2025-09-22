//! Serde is a framework for ***ser***ializing and ***de***serializing Rust data
//! structures efficiently and generically.
//!
//! The `serde_core` crate contains Serde's trait definitions with **no support
//! for #\[derive()\]**.
//!
//! In crates that derive an implementation of `Serialize` or `Deserialize`, you
//! must depend on the [`serde`] crate, not `serde_core`.
//!
//! [`serde`]: https://crates.io/crates/serde
//!
//! In crates that handwrite implementations of Serde traits, or only use them
//! as trait bounds, depending on `serde_core` is permitted. But `serde`
//! re-exports all of these traits and can be used for this use case too. If in
//! doubt, disregard `serde_core` and always use `serde`.
//!
//! Crates that depend on `serde_core` instead of `serde` are able to compile in
//! parallel with `serde_derive` even when `serde`'s "derive" feature is turned on,
//! as shown in the following build timings.
//!
//! <br>
//!
//! <table>
//! <tr><td align="center">When <code>serde_json</code> depends on <code>serde</code></td></tr>
//! <tr><td><img src="https://github.com/user-attachments/assets/78dc179c-6ab1-4059-928c-1474b0d9d0bb"></td></tr>
//! </table>
//!
//! <br>
//!
//! <table>
//! <tr><td align="center">When <code>serde_json</code> depends on <code>serde_core</code></td></tr>
//! <tr><td><img src="https://github.com/user-attachments/assets/6b6cff5e-3e45-4ac7-9db1-d99ee8b9f5f7"></td></tr>
//! </table>

////////////////////////////////////////////////////////////////////////////////

// Serde types in rustdoc of other crates get linked to here.
#![doc(html_root_url = "https://docs.rs/serde_core/1.0.225")]
// Support using Serde without the standard library!
#![cfg_attr(not(feature = "std"), no_std)]
// Show which crate feature enables conditionally compiled APIs in documentation.
#![cfg_attr(docsrs, feature(doc_cfg, rustdoc_internals))]
#![cfg_attr(docsrs, allow(internal_features))]
// Unstable functionality only if the user asks for it. For tracking and
// discussion of these features please refer to this issue:
//
//    https://github.com/serde-rs/serde/issues/812
#![cfg_attr(feature = "unstable", feature(never_type))]
#![allow(unknown_lints, bare_trait_objects, deprecated)]
// Ignored clippy and clippy_pedantic lints
#![allow(
    // clippy bug: https://github.com/rust-lang/rust-clippy/issues/5704
    clippy::unnested_or_patterns,
    // clippy bug: https://github.com/rust-lang/rust-clippy/issues/7768
    clippy::semicolon_if_nothing_returned,
    // not available in our oldest supported compiler
    clippy::empty_enum,
    clippy::type_repetition_in_bounds, // https://github.com/rust-lang/rust-clippy/issues/8772
    // integer and float ser/de requires these sorts of casts
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_precision_loss,
    clippy::cast_sign_loss,
    // things are often more readable this way
    clippy::cast_lossless,
    clippy::module_name_repetitions,
    clippy::single_match_else,
    clippy::type_complexity,
    clippy::use_self,
    clippy::zero_prefixed_literal,
    // correctly used
    clippy::derive_partial_eq_without_eq,
    clippy::enum_glob_use,
    clippy::explicit_auto_deref,
    clippy::incompatible_msrv,
    clippy::let_underscore_untyped,
    clippy::map_err_ignore,
    clippy::new_without_default,
    clippy::result_unit_err,
    clippy::wildcard_imports,
    // not practical
    clippy::needless_pass_by_value,
    clippy::similar_names,
    clippy::too_many_lines,
    // preference
    clippy::doc_markdown,
    clippy::elidable_lifetime_names,
    clippy::needless_lifetimes,
    clippy::unseparated_literal_suffix,
    // false positive
    clippy::needless_doctest_main,
    // noisy
    clippy::missing_errors_doc,
    clippy::must_use_candidate,
)]
// Restrictions
#![deny(clippy::question_mark_used)]
// Rustc lints.
#![deny(missing_docs, unused_imports)]

////////////////////////////////////////////////////////////////////////////////

#[cfg(feature = "alloc")]
extern crate alloc;

/// A facade around all the types we need from the `std`, `core`, and `alloc`
/// crates. This avoids elaborate import wrangling having to happen in every
/// module.
mod lib {
    mod core {
        #[cfg(not(feature = "std"))]
        pub use core::*;
        #[cfg(feature = "std")]
        pub use std::*;
    }

    pub use self::core::{f32, f64};
    pub use self::core::{iter, num, str};

    #[cfg(any(feature = "std", feature = "alloc"))]
    pub use self::core::{cmp, mem};

    pub use self::core::cell::{Cell, RefCell};
    pub use self::core::cmp::Reverse;
    pub use self::core::fmt::{self, Debug, Display, Write as FmtWrite};
    pub use self::core::marker::PhantomData;
    pub use self::core::num::Wrapping;
    pub use self::core::ops::{Bound, Range, RangeFrom, RangeInclusive, RangeTo};
    pub use self::core::result;
    pub use self::core::time::Duration;

    #[cfg(all(feature = "alloc", not(feature = "std")))]
    pub use alloc::borrow::{Cow, ToOwned};
    #[cfg(feature = "std")]
    pub use std::borrow::{Cow, ToOwned};

    #[cfg(all(feature = "alloc", not(feature = "std")))]
    pub use alloc::string::{String, ToString};
    #[cfg(feature = "std")]
    pub use std::string::{String, ToString};

    #[cfg(all(feature = "alloc", not(feature = "std")))]
    pub use alloc::vec::Vec;
    #[cfg(feature = "std")]
    pub use std::vec::Vec;

    #[cfg(all(feature = "alloc", not(feature = "std")))]
    pub use alloc::boxed::Box;
    #[cfg(feature = "std")]
    pub use std::boxed::Box;

    #[cfg(all(feature = "rc", feature = "alloc", not(feature = "std")))]
    pub use alloc::rc::{Rc, Weak as RcWeak};
    #[cfg(all(feature = "rc", feature = "std"))]
    pub use std::rc::{Rc, Weak as RcWeak};

    #[cfg(all(feature = "rc", feature = "alloc", not(feature = "std")))]
    pub use alloc::sync::{Arc, Weak as ArcWeak};
    #[cfg(all(feature = "rc", feature = "std"))]
    pub use std::sync::{Arc, Weak as ArcWeak};

    #[cfg(all(feature = "alloc", not(feature = "std")))]
    pub use alloc::collections::{BTreeMap, BTreeSet, BinaryHeap, LinkedList, VecDeque};
    #[cfg(feature = "std")]
    pub use std::collections::{BTreeMap, BTreeSet, BinaryHeap, LinkedList, VecDeque};

    #[cfg(all(not(no_core_cstr), not(feature = "std")))]
    pub use self::core::ffi::CStr;
    #[cfg(feature = "std")]
    pub use std::ffi::CStr;

    #[cfg(all(not(no_core_cstr), feature = "alloc", not(feature = "std")))]
    pub use alloc::ffi::CString;
    #[cfg(feature = "std")]
    pub use std::ffi::CString;

    #[cfg(all(not(no_core_net), not(feature = "std")))]
    pub use self::core::net;
    #[cfg(feature = "std")]
    pub use std::net;

    #[cfg(feature = "std")]
    pub use std::error;

    #[cfg(feature = "std")]
    pub use std::collections::{HashMap, HashSet};
    #[cfg(feature = "std")]
    pub use std::ffi::{OsStr, OsString};
    #[cfg(feature = "std")]
    pub use std::hash::{BuildHasher, Hash};
    #[cfg(feature = "std")]
    pub use std::io::Write;
    #[cfg(feature = "std")]
    pub use std::path::{Path, PathBuf};
    #[cfg(feature = "std")]
    pub use std::sync::{Mutex, RwLock};
    #[cfg(feature = "std")]
    pub use std::time::{SystemTime, UNIX_EPOCH};

    #[cfg(all(feature = "std", no_target_has_atomic, not(no_std_atomic)))]
    pub use std::sync::atomic::{
        AtomicBool, AtomicI16, AtomicI32, AtomicI8, AtomicIsize, AtomicU16, AtomicU32, AtomicU8,
        AtomicUsize, Ordering,
    };
    #[cfg(all(feature = "std", no_target_has_atomic, not(no_std_atomic64)))]
    pub use std::sync::atomic::{AtomicI64, AtomicU64};

    #[cfg(all(feature = "std", not(no_target_has_atomic)))]
    pub use std::sync::atomic::Ordering;
    #[cfg(all(feature = "std", not(no_target_has_atomic), target_has_atomic = "8"))]
    pub use std::sync::atomic::{AtomicBool, AtomicI8, AtomicU8};
    #[cfg(all(feature = "std", not(no_target_has_atomic), target_has_atomic = "16"))]
    pub use std::sync::atomic::{AtomicI16, AtomicU16};
    #[cfg(all(feature = "std", not(no_target_has_atomic), target_has_atomic = "32"))]
    pub use std::sync::atomic::{AtomicI32, AtomicU32};
    #[cfg(all(feature = "std", not(no_target_has_atomic), target_has_atomic = "64"))]
    pub use std::sync::atomic::{AtomicI64, AtomicU64};
    #[cfg(all(feature = "std", not(no_target_has_atomic), target_has_atomic = "ptr"))]
    pub use std::sync::atomic::{AtomicIsize, AtomicUsize};

    #[cfg(not(no_core_num_saturating))]
    pub use self::core::num::Saturating;
}

// None of this crate's error handling needs the `From::from` error conversion
// performed implicitly by the `?` operator or the standard library's `try!`
// macro. This simplified macro gives a 5.5% improvement in compile time
// compared to standard `try!`, and 9% improvement compared to `?`.
macro_rules! tri {
    ($expr:expr) => {
        match $expr {
            Ok(val) => val,
            Err(err) => return Err(err),
        }
    };
}

////////////////////////////////////////////////////////////////////////////////

#[macro_use]
mod macros;

pub mod de;
pub mod ser;

mod format;

#[doc(inline)]
pub use crate::de::{Deserialize, Deserializer};
#[doc(inline)]
pub use crate::ser::{Serialize, Serializer};

// Used by generated code. Not public API.
#[doc(hidden)]
mod private;

// Used by declarative macro generated code. Not public API.
#[doc(hidden)]
pub mod __private {
    #[doc(hidden)]
    pub use crate::private::doc;
    #[doc(hidden)]
    pub use core::result::Result;
}

include!(concat!(env!("OUT_DIR"), "/private.rs"));

#[cfg(all(not(feature = "std"), no_core_error))]
mod std_error;

#[macro_export]
#[doc(hidden)]
macro_rules! __require_serde_not_serde_core {
    () => {
        ::core::compile_error!(
            "Serde derive requires a dependency on the serde crate, not serde_core"
        );
    };
}
