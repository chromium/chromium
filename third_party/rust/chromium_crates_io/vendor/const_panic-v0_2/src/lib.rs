//! For panicking with formatting in const contexts.
//!
//! This library exists because the panic macro was stabilized for use in const contexts
//! in Rust 1.57.0, without formatting support.
//!
//! All of the types that implement the [`PanicFmt`] trait can be formatted in panics.
//!
//! # Examples
//!
//! - [Basic](#basic)
//! - [Custom Types](#custom-types)
//!
//! ### Basic
//!
//! ```compile_fail
//! use const_panic::concat_assert;
//!
//! const FOO: u32 = 10;
//! const BAR: u32 = 0;
//! const _: () = assert_non_zero(FOO, BAR);
//!
//! #[track_caller]
//! const fn assert_non_zero(foo: u32, bar: u32) {
//!     concat_assert!{
//!         foo != 0 && bar != 0,
//!         "\nneither foo nor bar can be zero!\nfoo: ", foo, "\nbar: ", bar
//!     }
//! }
//! ```
//! The above code fails to compile with this error:
//! ```text
//! error[E0080]: evaluation of constant value failed
//!  --> src/lib.rs:20:15
//!   |
//! 8 | const _: () = assert_non_zero(FOO, BAR);
//!   |               ^^^^^^^^^^^^^^^^^^^^^^^^^ the evaluated program panicked at '
//! neither foo nor bar can be zero!
//! foo: 10
//! bar: 0', src/lib.rs:8:15
//! ```
//!
//! When called at runtime
//! ```should_panic
//! use const_panic::concat_assert;
//!
//! assert_non_zero(10, 0);
//!
//! #[track_caller]
//! const fn assert_non_zero(foo: u32, bar: u32) {
//!     concat_assert!{
//!         foo != 0 && bar != 0,
//!         "\nneither foo nor bar can be zero!\nfoo: ", foo, "\nbar: ", bar
//!     }
//! }
//! ```
//! it prints this:
//! ```text
//! thread 'main' panicked at '
//! neither foo nor bar can be zero!
//! foo: 10
//! bar: 0', src/lib.rs:6:1
//! note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace
//!
//! ```
//!
//! ### Custom types
//!
//! Panic formatting for custom types can be done in these ways
//! (in increasing order of verbosity):
//! - Using the [`PanicFmt` derive] macro
//! (requires the opt-in `"derive"` feature)
//! - Using the [`impl_panicfmt`] macro
//! (requires the default-enabled `"non_basic"` feature)
//! - Using the [`flatten_panicvals`] macro
//! (requires the default-enabled `"non_basic"` feature)
//! - Manually implementing the [`PanicFmt`] trait as described in its docs.
//!
//! This example uses the [`PanicFmt` derive] approach.
//!
//! ```compile_fail
//! use const_panic::{PanicFmt, concat_panic};
//!
//! const LAST: u8 = {
//!     Foo{
//!         x: &[],
//!         y: Bar(false, true),
//!         z: Qux::Left(23),
//!     }.pop().1
//! };
//!
//! impl Foo<'_> {
//!     /// Pops the last element
//!     ///
//!     /// # Panics
//!     ///
//!     /// Panics if `self.x` is empty
//!     #[track_caller]
//!     const fn pop(mut self) -> (Self, u8) {
//!         if let [rem @ .., last] = self.x {
//!             self.x = rem;
//!             (self, *last)
//!         } else {
//!             concat_panic!(
//!                 "\nexpected a non-empty Foo, found: \n",
//!                 // uses alternative Debug formatting for `self`,
//!                 // otherwise this would use regular Debug formatting.
//!                 alt_debug: self
//!             )
//!         }
//!     }
//! }
//!
//! #[derive(PanicFmt)]
//! struct Foo<'a> {
//!     x: &'a [u8],
//!     y: Bar,
//!     z: Qux,
//! }
//!
//! #[derive(PanicFmt)]
//! struct Bar(bool, bool);
//!
//! #[derive(PanicFmt)]
//! enum Qux {
//!     Up,
//!     Down { x: u32, y: u32 },
//!     Left(u64),
//! }
//!
//! ```
//! The above code fails to compile with this error:
//! ```text
//! error[E0080]: evaluation of constant value failed
//!   --> src/lib.rs:57:5
//!    |
//! 7  | /     Foo{
//! 8  | |         x: &[],
//! 9  | |         y: Bar(false, true),
//! 10 | |         z: Qux::Left(23),
//! 11 | |     }.pop().1
//!    | |___________^ the evaluated program panicked at '
//! expected a non-empty Foo, found:
//! Foo {
//!     x: [],
//!     y: Bar(
//!         false,
//!         true,
//!     ),
//!     z: Left(
//!         23,
//!     ),
//! }', src/lib.rs:11:7
//!
//!
//! ```
//!
//! # Limitations
#![doc = crate::doc_macros::limitation_docs!()]
//!
//! ### Panic message length
//!
//! The panic message can only be up to [`MAX_PANIC_MSG_LEN`] long,
//! after which it is truncated.
//!
//! # Cargo features
//!
//! - `"non_basic"`(enabled by default):
//! Enables support for formatting structs, enums, and arrays.
//! <br>
//! Without this feature, you can effectively only format primitive types
//! (custom types can manually implement formatting with more difficulty).
//!
//! - `"rust_latest_stable"`(disabled by default):
//! Enables all the `"rust_1_*"` features.
//!
//! - `"rust_1_64"`(disabled by default):
//! Enables formatting of additional items that require Rust 1.64.0 to do so.
//!
//! - `"rust_1_82"`(disabled by default):
//! Enables formatting of additional items that require Rust 1.82.0 to do so.
//!
//! - `"rust_1_88"`(disabled by default):
//! Enables formatting of additional items that require Rust 1.88.0 to do so.
//!
//! - `"derive"`(disabled by default):
//! Enables the [`PanicFmt` derive] macro.
//!
//! # Plans
//!
//! None for now
//!
//! # No-std support
//!
//! `const_panic` is `#![no_std]`, it can be used anywhere Rust can be used.
//!
//! # Minimum Supported Rust Version
//!
//! This requires Rust 1.57.0, because it uses the `panic` macro in a const context.
//!
//!
//! [`PanicFmt` derive]: derive@crate::PanicFmt
//! [`PanicFmt`]: trait@crate::PanicFmt
//! [`impl_panicfmt`]: crate::impl_panicfmt
//! [`flatten_panicvals`]: crate::flatten_panicvals
//! [`MAX_PANIC_MSG_LEN`]: crate::MAX_PANIC_MSG_LEN
#![no_std]
#![cfg_attr(feature = "docsrs", feature(doc_cfg))]
#![warn(missing_docs)]
#![deny(clippy::missing_safety_doc)]
#![deny(clippy::shadow_unrelated)]
#![deny(clippy::wildcard_imports)]

extern crate self as const_panic;

#[macro_use]
mod doc_macros;

#[macro_use]
mod macros;

mod concat_panic_;

mod debug_str_fmt;

mod int_formatting;

pub mod fmt;

#[cfg(all(doctest, feature = "non_basic"))]
pub mod doctests;

mod panic_val;

#[cfg(feature = "non_basic")]
mod const_default;

pub mod utils;

/// tests doc lints with `cargo doc --features="test rust_latest_stable derive"`
#[cfg(all(feature = "test", feature = "non_basic"))]
pub mod test_doc_lints;

#[cfg(all(test, not(feature = "test")))]
compile_error! {r##"please use cargo test --features "test""##}

#[cfg(feature = "non_basic")]
mod slice_stuff;

#[cfg(feature = "non_basic")]
mod array_string;

#[cfg(feature = "non_basic")]
pub use crate::array_string::ArrayString;

mod wrapper;

mod fmt_impls {
    #[macro_use]
    pub(crate) mod basic_fmt_impls;

    #[cfg(feature = "rust_1_64")]
    mod rust_1_64_fmt_impls;

    #[cfg(feature = "rust_1_82")]
    mod rust_1_82_fmt_impls;

    #[macro_use]
    #[cfg(feature = "non_basic")]
    mod option_fmt_impls;

    #[cfg(feature = "non_basic")]
    mod nonzero_impls;

    #[cfg(feature = "non_basic")]
    mod other_impls;

    #[cfg(feature = "non_basic")]
    mod fmt_range;

    #[cfg(all(feature = "non_basic", feature = "rust_1_88"))]
    mod rust_1_88_nonbasic_fmt_impls;
}

pub use crate::{
    concat_panic_::{concat_panic, MAX_PANIC_MSG_LEN},
    panic_val::PanicVal,
    wrapper::StdWrapper,
};

#[doc(no_inline)]
pub use crate::fmt::{FmtArg, IsCustomType, PanicFmt};

#[cfg(feature = "non_basic")]
#[doc(no_inline)]
pub use crate::fmt::{ComputePvCount, TypeDelim};

#[doc(hidden)]
pub mod __ {
    pub use core::{
        assert, compile_error, concat,
        option::Option::{None, Some},
        primitive::usize,
        result::Result::{Err, Ok},
        stringify,
    };

    pub use crate::*;

    #[cfg(feature = "non_basic")]
    pub use crate::reexported_non_basic::*;
}

#[cfg(feature = "non_basic")]
#[doc(hidden)]
mod reexported_non_basic {
    pub use core::{mem::forget, option::Option, primitive::str, unreachable};

    pub use typewit::MakeTypeWitness;

    pub use crate::{
        concat_panic_::{compute_length, make_panic_string_unwrapped},
        const_default::ConstDefault,
        macros::concat_macro::ConcatCmd,
        utils::{assert_flatten_panicvals_length, flatten_panicvals, panicvals_id},
    };

    pub const EPV: crate::PanicVal<'_> = crate::PanicVal::EMPTY;
}

#[cfg(feature = "derive")]
include! {"./proc_macro_reexports/panicfmt_derive.rs"}

#[doc(hidden)]
#[cfg(feature = "test")]
pub mod test_utils;

#[doc(hidden)]
#[cfg(feature = "test")]
pub mod for_tests {
    pub use crate::concat_panic_::{format_panic_message, NotEnoughSpace};
}

#[cfg(all(doctest))]
#[doc = include_str!("../README.md")]
pub struct ReadmeTest;
