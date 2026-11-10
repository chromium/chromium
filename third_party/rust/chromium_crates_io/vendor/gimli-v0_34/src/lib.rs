//! `gimli` is a library for reading and writing the
//! [DWARF debugging format](https://dwarfstd.org/).
//!
//! See the [read](./read/index.html) and [write](./write/index.html) modules
//! for examples and API documentation.
//!
//! ## Cargo Features
//!
//! Cargo features that can be enabled with `gimli`:
//!
//! * `std`: Enabled by default. Use the `std` library. Disabling this feature allows
//!   using `gimli` in embedded environments that do not have access to `std`.
//!
//! * `read`: Enabled by default. Enables the `read` module. Use of `std` is optional.
//!   Requires the `alloc` crate.
//!
//! * `read-core`: Enabled by default. Enables a subset of the `read` module that does
//!   not require the `alloc` crate.
//!
//! * `endian-reader`: Enabled by default. Enables the [`EndianReader`] trait. This is
//!   a separate feature because it also depends on the `stable_deref_trait` crate.
//!
//! * `write`: Enabled by default. Enables the `write` module. Use of `std` is optional.
//!   Requires the `alloc` crate. Enabling both `read` and `write` will also enable
//!   conversion functionality in the `write` module.
//!
#![deny(missing_docs)]
#![deny(missing_debug_implementations)]
// Selectively enable rust 2018 warnings
#![warn(bare_trait_objects)]
#![warn(unused_extern_crates)]
#![warn(ellipsis_inclusive_range_patterns)]
#![warn(elided_lifetimes_in_paths)]
#![warn(explicit_outlives_requirements)]
// Style.
#![allow(clippy::bool_to_int_with_if)]
#![allow(clippy::collapsible_else_if)]
#![allow(clippy::collapsible_match)]
#![allow(clippy::comparison_chain)]
#![allow(clippy::manual_range_contains)]
#![allow(clippy::needless_late_init)]
#![allow(clippy::too_many_arguments)]
#![allow(clippy::needless_lifetimes)]
// False positives with `fallible_iterator`.
#![allow(clippy::should_implement_trait)]
// False positives.
#![allow(clippy::derive_partial_eq_without_eq)]
#![no_std]

#[allow(unused_imports)]
#[cfg(any(feature = "read", feature = "write"))]
#[macro_use]
extern crate alloc;

#[cfg(feature = "std")]
#[macro_use]
extern crate std;

#[cfg(feature = "endian-reader")]
pub use stable_deref_trait::{CloneStableDeref, StableDeref};

mod common;
pub use crate::common::*;

mod arch;
pub use crate::arch::*;

pub mod constants;
// For backwards compat.
pub use crate::constants::*;

mod endianity;
pub use crate::endianity::*;

pub mod leb128;

mod case_fold;
pub use crate::case_fold::*;

#[cfg(feature = "read-core")]
pub mod read;
// For backwards compat.
#[cfg(feature = "read-core")]
pub use crate::read::*;

#[cfg(feature = "write")]
pub mod write;

#[cfg(test)]
mod test_util;
