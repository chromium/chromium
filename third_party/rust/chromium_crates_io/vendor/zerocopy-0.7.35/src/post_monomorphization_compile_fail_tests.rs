// Copyright 2018 The Fuchsia Authors
//
// Licensed under the 2-Clause BSD License <LICENSE-BSD or
// https://opensource.org/license/bsd-2-clause>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

//! Code that should fail to compile during the post-monomorphization compiler
//! pass.
//!
//! Due to [a limitation with the `trybuild` crate][trybuild-issue], we cannot
//! use our UI testing framework to test compilation failures that are
//! encountered after monomorphization has complated. This module has one item
//! for each such test we would prefer to have as a UI test, with the code in
//! question appearing as a rustdoc example which is marked with `compile_fail`.
//! This has the effect of causing doctests to fail if any of these examples
//! compile successfully.
//!
//! This is very much a hack and not a complete replacement for UI tests - most
//! notably because this only provides a single "compile vs fail" bit of
//! information, but does not allow us to depend upon the specific error that
//! causes compilation to fail.
//!
//! [trybuild-issue]: https://github.com/dtolnay/trybuild/issues/241

// Miri doesn't detect post-monimorphization failures as compile-time failures,
// but instead as runtime failures.
#![cfg(not(miri))]

/// ```compile_fail
/// use core::cell::{Ref, RefCell};
///
/// let refcell = RefCell::new([0u8, 1, 2, 3]);
/// let core_ref = refcell.borrow();
/// let core_ref = Ref::map(core_ref, |bytes| &bytes[..]);
///
/// // `zc_ref` now stores `core_ref` internally.
/// let zc_ref = zerocopy::Ref::<_, u32>::new(core_ref).unwrap();
///
/// // This causes `core_ref` to get dropped and synthesizes a Rust
/// // reference to the memory `core_ref` was pointing at.
/// let rust_ref = zc_ref.into_ref();
///
/// // UB!!! This mutates `rust_ref`'s referent while it's alive.
/// *refcell.borrow_mut() = [0, 0, 0, 0];
///
/// println!("{}", rust_ref);
/// ```
#[allow(unused)]
const REFCELL_REF_INTO_REF: () = ();

/// ```compile_fail
/// use core::cell::{RefCell, RefMut};
///
/// let refcell = RefCell::new([0u8, 1, 2, 3]);
/// let core_ref_mut = refcell.borrow_mut();
/// let core_ref_mut = RefMut::map(core_ref_mut, |bytes| &mut bytes[..]);
///
/// // `zc_ref` now stores `core_ref_mut` internally.
/// let zc_ref = zerocopy::Ref::<_, u32>::new(core_ref_mut).unwrap();
///
/// // This causes `core_ref_mut` to get dropped and synthesizes a Rust
/// // reference to the memory `core_ref` was pointing at.
/// let rust_ref_mut = zc_ref.into_mut();
///
/// // UB!!! This mutates `rust_ref_mut`'s referent while it's alive.
/// *refcell.borrow_mut() = [0, 0, 0, 0];
///
/// println!("{}", rust_ref_mut);
/// ```
#[allow(unused)]
const REFCELL_REFMUT_INTO_MUT: () = ();

/// ```compile_fail
/// use core::cell::{Ref, RefCell};
///
/// let refcell = RefCell::new([0u8, 1, 2, 3]);
/// let core_ref = refcell.borrow();
/// let core_ref = Ref::map(core_ref, |bytes| &bytes[..]);
///
/// // `zc_ref` now stores `core_ref` internally.
/// let zc_ref = zerocopy::Ref::<_, [u16]>::new_slice(core_ref).unwrap();
///
/// // This causes `core_ref` to get dropped and synthesizes a Rust
/// // reference to the memory `core_ref` was pointing at.
/// let rust_ref = zc_ref.into_slice();
///
/// // UB!!! This mutates `rust_ref`'s referent while it's alive.
/// *refcell.borrow_mut() = [0, 0, 0, 0];
///
/// println!("{:?}", rust_ref);
/// ```
#[allow(unused)]
const REFCELL_REFMUT_INTO_SLICE: () = ();

/// ```compile_fail
/// use core::cell::{RefCell, RefMut};
///
/// let refcell = RefCell::new([0u8, 1, 2, 3]);
/// let core_ref_mut = refcell.borrow_mut();
/// let core_ref_mut = RefMut::map(core_ref_mut, |bytes| &mut bytes[..]);
///
/// // `zc_ref` now stores `core_ref_mut` internally.
/// let zc_ref = zerocopy::Ref::<_, [u16]>::new_slice(core_ref_mut).unwrap();
///
/// // This causes `core_ref_mut` to get dropped and synthesizes a Rust
/// // reference to the memory `core_ref` was pointing at.
/// let rust_ref_mut = zc_ref.into_mut_slice();
///
/// // UB!!! This mutates `rust_ref_mut`'s referent while it's alive.
/// *refcell.borrow_mut() = [0, 0, 0, 0];
///
/// println!("{:?}", rust_ref_mut);
/// ```
#[allow(unused)]
const REFCELL_REFMUT_INTO_MUT_SLICE: () = ();
