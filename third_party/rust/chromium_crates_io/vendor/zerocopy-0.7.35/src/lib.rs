// Copyright 2018 The Fuchsia Authors
//
// Licensed under the 2-Clause BSD License <LICENSE-BSD or
// https://opensource.org/license/bsd-2-clause>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

// After updating the following doc comment, make sure to run the following
// command to update `README.md` based on its contents:
//
//   ./generate-readme.sh > README.md

//! *<span style="font-size: 100%; color:grey;">Want to help improve zerocopy?
//! Fill out our [user survey][user-survey]!</span>*
//!
//! ***<span style="font-size: 140%">Fast, safe, <span
//! style="color:red;">compile error</span>. Pick two.</span>***
//!
//! Zerocopy makes zero-cost memory manipulation effortless. We write `unsafe`
//! so you don't have to.
//!
//! # Overview
//!
//! Zerocopy provides four core marker traits, each of which can be derived
//! (e.g., `#[derive(FromZeroes)]`):
//! - [`FromZeroes`] indicates that a sequence of zero bytes represents a valid
//!   instance of a type
//! - [`FromBytes`] indicates that a type may safely be converted from an
//!   arbitrary byte sequence
//! - [`AsBytes`] indicates that a type may safely be converted *to* a byte
//!   sequence
//! - [`Unaligned`] indicates that a type's alignment requirement is 1
//!
//! Types which implement a subset of these traits can then be converted to/from
//! byte sequences with little to no runtime overhead.
//!
//! Zerocopy also provides byte-order aware integer types that support these
//! conversions; see the [`byteorder`] module. These types are especially useful
//! for network parsing.
//!
//! [user-survey]: https://docs.google.com/forms/d/e/1FAIpQLSdzBNTN9tzwsmtyZxRFNL02K36IWCdHWW2ZBckyQS2xiO3i8Q/viewform?usp=published_options
//!
//! # Cargo Features
//!
//! - **`alloc`**   
//!   By default, `zerocopy` is `no_std`. When the `alloc` feature is enabled,
//!   the `alloc` crate is added as a dependency, and some allocation-related
//!   functionality is added.
//!
//! - **`byteorder`** (enabled by default)   
//!   Adds the [`byteorder`] module and a dependency on the `byteorder` crate.
//!   The `byteorder` module provides byte order-aware equivalents of the
//!   multi-byte primitive numerical types. Unlike their primitive equivalents,
//!   the types in this module have no alignment requirement and support byte
//!   order conversions. This can be useful in handling file formats, network
//!   packet layouts, etc which don't provide alignment guarantees and which may
//!   use a byte order different from that of the execution platform.
//!
//! - **`derive`**   
//!   Provides derives for the core marker traits via the `zerocopy-derive`
//!   crate. These derives are re-exported from `zerocopy`, so it is not
//!   necessary to depend on `zerocopy-derive` directly.   
//!
//!   However, you may experience better compile times if you instead directly
//!   depend on both `zerocopy` and `zerocopy-derive` in your `Cargo.toml`,
//!   since doing so will allow Rust to compile these crates in parallel. To do
//!   so, do *not* enable the `derive` feature, and list both dependencies in
//!   your `Cargo.toml` with the same leading non-zero version number; e.g:
//!
//!   ```toml
//!   [dependencies]
//!   zerocopy = "0.X"
//!   zerocopy-derive = "0.X"
//!   ```
//!
//! - **`simd`**   
//!   When the `simd` feature is enabled, `FromZeroes`, `FromBytes`, and
//!   `AsBytes` impls are emitted for all stable SIMD types which exist on the
//!   target platform. Note that the layout of SIMD types is not yet stabilized,
//!   so these impls may be removed in the future if layout changes make them
//!   invalid. For more information, see the Unsafe Code Guidelines Reference
//!   page on the [layout of packed SIMD vectors][simd-layout].
//!
//! - **`simd-nightly`**   
//!   Enables the `simd` feature and adds support for SIMD types which are only
//!   available on nightly. Since these types are unstable, support for any type
//!   may be removed at any point in the future.
//!
//! [simd-layout]: https://rust-lang.github.io/unsafe-code-guidelines/layout/packed-simd-vectors.html
//!
//! # Security Ethos
//!
//! Zerocopy is expressly designed for use in security-critical contexts. We
//! strive to ensure that that zerocopy code is sound under Rust's current
//! memory model, and *any future memory model*. We ensure this by:
//! - **...not 'guessing' about Rust's semantics.**   
//!   We annotate `unsafe` code with a precise rationale for its soundness that
//!   cites a relevant section of Rust's official documentation. When Rust's
//!   documented semantics are unclear, we work with the Rust Operational
//!   Semantics Team to clarify Rust's documentation.
//! - **...rigorously testing our implementation.**   
//!   We run tests using [Miri], ensuring that zerocopy is sound across a wide
//!   array of supported target platforms of varying endianness and pointer
//!   width, and across both current and experimental memory models of Rust.
//! - **...formally proving the correctness of our implementation.**   
//!   We apply formal verification tools like [Kani][kani] to prove zerocopy's
//!   correctness.
//!
//! For more information, see our full [soundness policy].
//!
//! [Miri]: https://github.com/rust-lang/miri
//! [Kani]: https://github.com/model-checking/kani
//! [soundness policy]: https://github.com/google/zerocopy/blob/main/POLICIES.md#soundness
//!
//! # Relationship to Project Safe Transmute
//!
//! [Project Safe Transmute] is an official initiative of the Rust Project to
//! develop language-level support for safer transmutation. The Project consults
//! with crates like zerocopy to identify aspects of safer transmutation that
//! would benefit from compiler support, and has developed an [experimental,
//! compiler-supported analysis][mcp-transmutability] which determines whether,
//! for a given type, any value of that type may be soundly transmuted into
//! another type. Once this functionality is sufficiently mature, zerocopy
//! intends to replace its internal transmutability analysis (implemented by our
//! custom derives) with the compiler-supported one. This change will likely be
//! an implementation detail that is invisible to zerocopy's users.
//!
//! Project Safe Transmute will not replace the need for most of zerocopy's
//! higher-level abstractions. The experimental compiler analysis is a tool for
//! checking the soundness of `unsafe` code, not a tool to avoid writing
//! `unsafe` code altogether. For the foreseeable future, crates like zerocopy
//! will still be required in order to provide higher-level abstractions on top
//! of the building block provided by Project Safe Transmute.
//!
//! [Project Safe Transmute]: https://rust-lang.github.io/rfcs/2835-project-safe-transmute.html
//! [mcp-transmutability]: https://github.com/rust-lang/compiler-team/issues/411
//!
//! # MSRV
//!
//! See our [MSRV policy].
//!
//! [MSRV policy]: https://github.com/google/zerocopy/blob/main/POLICIES.md#msrv
//!
//! # Changelog
//!
//! Zerocopy uses [GitHub Releases].
//!
//! [GitHub Releases]: https://github.com/google/zerocopy/releases

// Sometimes we want to use lints which were added after our MSRV.
// `unknown_lints` is `warn` by default and we deny warnings in CI, so without
// this attribute, any unknown lint would cause a CI failure when testing with
// our MSRV.
//
// TODO(#1201): Remove `unexpected_cfgs`
#![allow(unknown_lints, non_local_definitions, unexpected_cfgs)]
#![deny(renamed_and_removed_lints)]
#![deny(
    anonymous_parameters,
    deprecated_in_future,
    late_bound_lifetime_arguments,
    missing_copy_implementations,
    missing_debug_implementations,
    missing_docs,
    path_statements,
    patterns_in_fns_without_body,
    rust_2018_idioms,
    trivial_numeric_casts,
    unreachable_pub,
    unsafe_op_in_unsafe_fn,
    unused_extern_crates,
    // We intentionally choose not to deny `unused_qualifications`. When items
    // are added to the prelude (e.g., `core::mem::size_of`), this has the
    // consequence of making some uses trigger this lint on the latest toolchain
    // (e.g., `mem::size_of`), but fixing it (e.g. by replacing with `size_of`)
    // does not work on older toolchains.
    //
    // We tested a more complicated fix in #1413, but ultimately decided that,
    // since this lint is just a minor style lint, the complexity isn't worth it
    // - it's fine to occasionally have unused qualifications slip through,
    // especially since these do not affect our user-facing API in any way.
    variant_size_differences
)]
#![cfg_attr(
    __INTERNAL_USE_ONLY_NIGHLTY_FEATURES_IN_TESTS,
    deny(fuzzy_provenance_casts, lossy_provenance_casts)
)]
#![deny(
    clippy::all,
    clippy::alloc_instead_of_core,
    clippy::arithmetic_side_effects,
    clippy::as_underscore,
    clippy::assertions_on_result_states,
    clippy::as_conversions,
    clippy::correctness,
    clippy::dbg_macro,
    clippy::decimal_literal_representation,
    clippy::get_unwrap,
    clippy::indexing_slicing,
    clippy::missing_inline_in_public_items,
    clippy::missing_safety_doc,
    clippy::obfuscated_if_else,
    clippy::perf,
    clippy::print_stdout,
    clippy::std_instead_of_core,
    clippy::style,
    clippy::suspicious,
    clippy::todo,
    clippy::undocumented_unsafe_blocks,
    clippy::unimplemented,
    clippy::unnested_or_patterns,
    clippy::unwrap_used,
    clippy::use_debug
)]
#![deny(
    rustdoc::bare_urls,
    rustdoc::broken_intra_doc_links,
    rustdoc::invalid_codeblock_attributes,
    rustdoc::invalid_html_tags,
    rustdoc::invalid_rust_codeblocks,
    rustdoc::missing_crate_level_docs,
    rustdoc::private_intra_doc_links
)]
// In test code, it makes sense to weight more heavily towards concise, readable
// code over correct or debuggable code.
#![cfg_attr(any(test, kani), allow(
    // In tests, you get line numbers and have access to source code, so panic
    // messages are less important. You also often unwrap a lot, which would
    // make expect'ing instead very verbose.
    clippy::unwrap_used,
    // In tests, there's no harm to "panic risks" - the worst that can happen is
    // that your test will fail, and you'll fix it. By contrast, panic risks in
    // production code introduce the possibly of code panicking unexpectedly "in
    // the field".
    clippy::arithmetic_side_effects,
    clippy::indexing_slicing,
))]
#![cfg_attr(not(test), no_std)]
#![cfg_attr(
    all(feature = "simd-nightly", any(target_arch = "x86", target_arch = "x86_64")),
    feature(stdarch_x86_avx512)
)]
#![cfg_attr(
    all(feature = "simd-nightly", target_arch = "arm"),
    feature(stdarch_arm_dsp, stdarch_arm_neon_intrinsics)
)]
#![cfg_attr(
    all(feature = "simd-nightly", any(target_arch = "powerpc", target_arch = "powerpc64")),
    feature(stdarch_powerpc)
)]
#![cfg_attr(doc_cfg, feature(doc_cfg))]
#![cfg_attr(
    __INTERNAL_USE_ONLY_NIGHLTY_FEATURES_IN_TESTS,
    feature(layout_for_ptr, strict_provenance)
)]

// This is a hack to allow zerocopy-derive derives to work in this crate. They
// assume that zerocopy is linked as an extern crate, so they access items from
// it as `zerocopy::Xxx`. This makes that still work.
#[cfg(any(feature = "derive", test))]
extern crate self as zerocopy;

#[macro_use]
mod macros;

#[cfg(feature = "byteorder")]
#[cfg_attr(doc_cfg, doc(cfg(feature = "byteorder")))]
pub mod byteorder;
#[doc(hidden)]
pub mod macro_util;
mod post_monomorphization_compile_fail_tests;
mod util;
// TODO(#252): If we make this pub, come up with a better name.
mod wrappers;

#[cfg(feature = "byteorder")]
#[cfg_attr(doc_cfg, doc(cfg(feature = "byteorder")))]
pub use crate::byteorder::*;
pub use crate::wrappers::*;

#[cfg(any(feature = "derive", test))]
#[cfg_attr(doc_cfg, doc(cfg(feature = "derive")))]
pub use zerocopy_derive::Unaligned;

// `pub use` separately here so that we can mark it `#[doc(hidden)]`.
//
// TODO(#29): Remove this or add a doc comment.
#[cfg(any(feature = "derive", test))]
#[cfg_attr(doc_cfg, doc(cfg(feature = "derive")))]
#[doc(hidden)]
pub use zerocopy_derive::KnownLayout;

use core::{
    cell::{self, RefMut},
    cmp::Ordering,
    fmt::{self, Debug, Display, Formatter},
    hash::Hasher,
    marker::PhantomData,
    mem::{self, ManuallyDrop, MaybeUninit},
    num::{
        NonZeroI128, NonZeroI16, NonZeroI32, NonZeroI64, NonZeroI8, NonZeroIsize, NonZeroU128,
        NonZeroU16, NonZeroU32, NonZeroU64, NonZeroU8, NonZeroUsize, Wrapping,
    },
    ops::{Deref, DerefMut},
    ptr::{self, NonNull},
    slice,
};

#[cfg(feature = "alloc")]
extern crate alloc;
#[cfg(feature = "alloc")]
use alloc::{boxed::Box, vec::Vec};

#[cfg(any(feature = "alloc", kani))]
use core::alloc::Layout;

// Used by `TryFromBytes::is_bit_valid`.
#[doc(hidden)]
pub use crate::util::ptr::Ptr;

// For each polyfill, as soon as the corresponding feature is stable, the
// polyfill import will be unused because method/function resolution will prefer
// the inherent method/function over a trait method/function. Thus, we suppress
// the `unused_imports` warning.
//
// See the documentation on `util::polyfills` for more information.
#[allow(unused_imports)]
use crate::util::polyfills::NonNullExt as _;

#[rustversion::nightly]
#[cfg(all(test, not(__INTERNAL_USE_ONLY_NIGHLTY_FEATURES_IN_TESTS)))]
const _: () = {
    #[deprecated = "some tests may be skipped due to missing RUSTFLAGS=\"--cfg __INTERNAL_USE_ONLY_NIGHLTY_FEATURES_IN_TESTS\""]
    const _WARNING: () = ();
    #[warn(deprecated)]
    _WARNING
};

/// The target pointer width, counted in bits.
const POINTER_WIDTH_BITS: usize = mem::size_of::<usize>() * 8;

/// The layout of a type which might be dynamically-sized.
///
/// `DstLayout` describes the layout of sized types, slice types, and "slice
/// DSTs" - ie, those that are known by the type system to have a trailing slice
/// (as distinguished from `dyn Trait` types - such types *might* have a
/// trailing slice type, but the type system isn't aware of it).
///
/// # Safety
///
/// Unlike [`core::alloc::Layout`], `DstLayout` is only used to describe full
/// Rust types - ie, those that satisfy the layout requirements outlined by
/// [the reference]. Callers may assume that an instance of `DstLayout`
/// satisfies any conditions imposed on Rust types by the reference.
///
/// If `layout: DstLayout` describes a type, `T`, then it is guaranteed that:
/// - `layout.align` is equal to `T`'s alignment
/// - If `layout.size_info` is `SizeInfo::Sized { size }`, then `T: Sized` and
///   `size_of::<T>() == size`
/// - If `layout.size_info` is `SizeInfo::SliceDst(slice_layout)`, then
///   - `T` is a slice DST
/// - The `size` of an instance of `T` with `elems` trailing slice elements is
///   equal to `slice_layout.offset + slice_layout.elem_size * elems` rounded up
///   to the nearest multiple of `layout.align`. Any bytes in the range
///   `[slice_layout.offset + slice_layout.elem_size * elems, size)` are padding
///   and must not be assumed to be initialized.
///
/// [the reference]: https://doc.rust-lang.org/reference/type-layout.html
#[doc(hidden)]
#[allow(missing_debug_implementations, missing_copy_implementations)]
#[cfg_attr(any(kani, test), derive(Copy, Clone, Debug, PartialEq, Eq))]
pub struct DstLayout {
    align: NonZeroUsize,
    size_info: SizeInfo,
}

#[cfg_attr(any(kani, test), derive(Copy, Clone, Debug, PartialEq, Eq))]
enum SizeInfo<E = usize> {
    Sized { _size: usize },
    SliceDst(TrailingSliceLayout<E>),
}

#[cfg_attr(any(kani, test), derive(Copy, Clone, Debug, PartialEq, Eq))]
struct TrailingSliceLayout<E = usize> {
    // The offset of the first byte of the trailing slice field. Note that this
    // is NOT the same as the minimum size of the type. For example, consider
    // the following type:
    //
    //   struct Foo {
    //       a: u16,
    //       b: u8,
    //       c: [u8],
    //   }
    //
    // In `Foo`, `c` is at byte offset 3. When `c.len() == 0`, `c` is followed
    // by a padding byte.
    _offset: usize,
    // The size of the element type of the trailing slice field.
    _elem_size: E,
}

impl SizeInfo {
    /// Attempts to create a `SizeInfo` from `Self` in which `elem_size` is a
    /// `NonZeroUsize`. If `elem_size` is 0, returns `None`.
    #[allow(unused)]
    const fn try_to_nonzero_elem_size(&self) -> Option<SizeInfo<NonZeroUsize>> {
        Some(match *self {
            SizeInfo::Sized { _size } => SizeInfo::Sized { _size },
            SizeInfo::SliceDst(TrailingSliceLayout { _offset, _elem_size }) => {
                if let Some(_elem_size) = NonZeroUsize::new(_elem_size) {
                    SizeInfo::SliceDst(TrailingSliceLayout { _offset, _elem_size })
                } else {
                    return None;
                }
            }
        })
    }
}

#[doc(hidden)]
#[derive(Copy, Clone)]
#[cfg_attr(test, derive(Debug))]
#[allow(missing_debug_implementations)]
pub enum _CastType {
    _Prefix,
    _Suffix,
}

impl DstLayout {
    /// The minimum possible alignment of a type.
    const MIN_ALIGN: NonZeroUsize = match NonZeroUsize::new(1) {
        Some(min_align) => min_align,
        None => unreachable!(),
    };

    /// The maximum theoretic possible alignment of a type.
    ///
    /// For compatibility with future Rust versions, this is defined as the
    /// maximum power-of-two that fits into a `usize`. See also
    /// [`DstLayout::CURRENT_MAX_ALIGN`].
    const THEORETICAL_MAX_ALIGN: NonZeroUsize =
        match NonZeroUsize::new(1 << (POINTER_WIDTH_BITS - 1)) {
            Some(max_align) => max_align,
            None => unreachable!(),
        };

    /// The current, documented max alignment of a type \[1\].
    ///
    /// \[1\] Per <https://doc.rust-lang.org/reference/type-layout.html#the-alignment-modifiers>:
    ///
    ///   The alignment value must be a power of two from 1 up to
    ///   2<sup>29</sup>.
    #[cfg(not(kani))]
    const CURRENT_MAX_ALIGN: NonZeroUsize = match NonZeroUsize::new(1 << 28) {
        Some(max_align) => max_align,
        None => unreachable!(),
    };

    /// Constructs a `DstLayout` for a zero-sized type with `repr_align`
    /// alignment (or 1). If `repr_align` is provided, then it must be a power
    /// of two.
    ///
    /// # Panics
    ///
    /// This function panics if the supplied `repr_align` is not a power of two.
    ///
    /// # Safety
    ///
    /// Unsafe code may assume that the contract of this function is satisfied.
    #[doc(hidden)]
    #[inline]
    pub const fn new_zst(repr_align: Option<NonZeroUsize>) -> DstLayout {
        let align = match repr_align {
            Some(align) => align,
            None => Self::MIN_ALIGN,
        };

        assert!(align.is_power_of_two());

        DstLayout { align, size_info: SizeInfo::Sized { _size: 0 } }
    }

    /// Constructs a `DstLayout` which describes `T`.
    ///
    /// # Safety
    ///
    /// Unsafe code may assume that `DstLayout` is the correct layout for `T`.
    #[doc(hidden)]
    #[inline]
    pub const fn for_type<T>() -> DstLayout {
        // SAFETY: `align` is correct by construction. `T: Sized`, and so it is
        // sound to initialize `size_info` to `SizeInfo::Sized { size }`; the
        // `size` field is also correct by construction.
        DstLayout {
            align: match NonZeroUsize::new(mem::align_of::<T>()) {
                Some(align) => align,
                None => unreachable!(),
            },
            size_info: SizeInfo::Sized { _size: mem::size_of::<T>() },
        }
    }

    /// Constructs a `DstLayout` which describes `[T]`.
    ///
    /// # Safety
    ///
    /// Unsafe code may assume that `DstLayout` is the correct layout for `[T]`.
    const fn for_slice<T>() -> DstLayout {
        // SAFETY: The alignment of a slice is equal to the alignment of its
        // element type, and so `align` is initialized correctly.
        //
        // Since this is just a slice type, there is no offset between the
        // beginning of the type and the beginning of the slice, so it is
        // correct to set `offset: 0`. The `elem_size` is correct by
        // construction. Since `[T]` is a (degenerate case of a) slice DST, it
        // is correct to initialize `size_info` to `SizeInfo::SliceDst`.
        DstLayout {
            align: match NonZeroUsize::new(mem::align_of::<T>()) {
                Some(align) => align,
                None => unreachable!(),
            },
            size_info: SizeInfo::SliceDst(TrailingSliceLayout {
                _offset: 0,
                _elem_size: mem::size_of::<T>(),
            }),
        }
    }

    /// Like `Layout::extend`, this creates a layout that describes a record
    /// whose layout consists of `self` followed by `next` that includes the
    /// necessary inter-field padding, but not any trailing padding.
    ///
    /// In order to match the layout of a `#[repr(C)]` struct, this method
    /// should be invoked for each field in declaration order. To add trailing
    /// padding, call `DstLayout::pad_to_align` after extending the layout for
    /// all fields. If `self` corresponds to a type marked with
    /// `repr(packed(N))`, then `repr_packed` should be set to `Some(N)`,
    /// otherwise `None`.
    ///
    /// This method cannot be used to match the layout of a record with the
    /// default representation, as that representation is mostly unspecified.
    ///
    /// # Safety
    ///
    /// If a (potentially hypothetical) valid `repr(C)` Rust type begins with
    /// fields whose layout are `self`, and those fields are immediately
    /// followed by a field whose layout is `field`, then unsafe code may rely
    /// on `self.extend(field, repr_packed)` producing a layout that correctly
    /// encompasses those two components.
    ///
    /// We make no guarantees to the behavior of this method if these fragments
    /// cannot appear in a valid Rust type (e.g., the concatenation of the
    /// layouts would lead to a size larger than `isize::MAX`).
    #[doc(hidden)]
    #[inline]
    pub const fn extend(self, field: DstLayout, repr_packed: Option<NonZeroUsize>) -> Self {
        use util::{core_layout::padding_needed_for, max, min};

        // If `repr_packed` is `None`, there are no alignment constraints, and
        // the value can be defaulted to `THEORETICAL_MAX_ALIGN`.
        let max_align = match repr_packed {
            Some(max_align) => max_align,
            None => Self::THEORETICAL_MAX_ALIGN,
        };

        assert!(max_align.is_power_of_two());

        // We use Kani to prove that this method is robust to future increases
        // in Rust's maximum allowed alignment. However, if such a change ever
        // actually occurs, we'd like to be notified via assertion failures.
        #[cfg(not(kani))]
        {
            debug_assert!(self.align.get() <= DstLayout::CURRENT_MAX_ALIGN.get());
            debug_assert!(field.align.get() <= DstLayout::CURRENT_MAX_ALIGN.get());
            if let Some(repr_packed) = repr_packed {
                debug_assert!(repr_packed.get() <= DstLayout::CURRENT_MAX_ALIGN.get());
            }
        }

        // The field's alignment is clamped by `repr_packed` (i.e., the
        // `repr(packed(N))` attribute, if any) [1].
        //
        // [1] Per https://doc.rust-lang.org/reference/type-layout.html#the-alignment-modifiers:
        //
        //   The alignments of each field, for the purpose of positioning
        //   fields, is the smaller of the specified alignment and the alignment
        //   of the field's type.
        let field_align = min(field.align, max_align);

        // The struct's alignment is the maximum of its previous alignment and
        // `field_align`.
        let align = max(self.align, field_align);

        let size_info = match self.size_info {
            // If the layout is already a DST, we panic; DSTs cannot be extended
            // with additional fields.
            SizeInfo::SliceDst(..) => panic!("Cannot extend a DST with additional fields."),

            SizeInfo::Sized { _size: preceding_size } => {
                // Compute the minimum amount of inter-field padding needed to
                // satisfy the field's alignment, and offset of the trailing
                // field. [1]
                //
                // [1] Per https://doc.rust-lang.org/reference/type-layout.html#the-alignment-modifiers:
                //
                //   Inter-field padding is guaranteed to be the minimum
                //   required in order to satisfy each field's (possibly
                //   altered) alignment.
                let padding = padding_needed_for(preceding_size, field_align);

                // This will not panic (and is proven to not panic, with Kani)
                // if the layout components can correspond to a leading layout
                // fragment of a valid Rust type, but may panic otherwise (e.g.,
                // combining or aligning the components would create a size
                // exceeding `isize::MAX`).
                let offset = match preceding_size.checked_add(padding) {
                    Some(offset) => offset,
                    None => panic!("Adding padding to `self`'s size overflows `usize`."),
                };

                match field.size_info {
                    SizeInfo::Sized { _size: field_size } => {
                        // If the trailing field is sized, the resulting layout
                        // will be sized. Its size will be the sum of the
                        // preceeding layout, the size of the new field, and the
                        // size of inter-field padding between the two.
                        //
                        // This will not panic (and is proven with Kani to not
                        // panic) if the layout components can correspond to a
                        // leading layout fragment of a valid Rust type, but may
                        // panic otherwise (e.g., combining or aligning the
                        // components would create a size exceeding
                        // `usize::MAX`).
                        let size = match offset.checked_add(field_size) {
                            Some(size) => size,
                            None => panic!("`field` cannot be appended without the total size overflowing `usize`"),
                        };
                        SizeInfo::Sized { _size: size }
                    }
                    SizeInfo::SliceDst(TrailingSliceLayout {
                        _offset: trailing_offset,
                        _elem_size,
                    }) => {
                        // If the trailing field is dynamically sized, so too
                        // will the resulting layout. The offset of the trailing
                        // slice component is the sum of the offset of the
                        // trailing field and the trailing slice offset within
                        // that field.
                        //
                        // This will not panic (and is proven with Kani to not
                        // panic) if the layout components can correspond to a
                        // leading layout fragment of a valid Rust type, but may
                        // panic otherwise (e.g., combining or aligning the
                        // components would create a size exceeding
                        // `usize::MAX`).
                        let offset = match offset.checked_add(trailing_offset) {
                            Some(offset) => offset,
                            None => panic!("`field` cannot be appended without the total size overflowing `usize`"),
                        };
                        SizeInfo::SliceDst(TrailingSliceLayout { _offset: offset, _elem_size })
                    }
                }
            }
        };

        DstLayout { align, size_info }
    }

    /// Like `Layout::pad_to_align`, this routine rounds the size of this layout
    /// up to the nearest multiple of this type's alignment or `repr_packed`
    /// (whichever is less). This method leaves DST layouts unchanged, since the
    /// trailing padding of DSTs is computed at runtime.
    ///
    /// In order to match the layout of a `#[repr(C)]` struct, this method
    /// should be invoked after the invocations of [`DstLayout::extend`]. If
    /// `self` corresponds to a type marked with `repr(packed(N))`, then
    /// `repr_packed` should be set to `Some(N)`, otherwise `None`.
    ///
    /// This method cannot be used to match the layout of a record with the
    /// default representation, as that representation is mostly unspecified.
    ///
    /// # Safety
    ///
    /// If a (potentially hypothetical) valid `repr(C)` type begins with fields
    /// whose layout are `self` followed only by zero or more bytes of trailing
    /// padding (not included in `self`), then unsafe code may rely on
    /// `self.pad_to_align(repr_packed)` producing a layout that correctly
    /// encapsulates the layout of that type.
    ///
    /// We make no guarantees to the behavior of this method if `self` cannot
    /// appear in a valid Rust type (e.g., because the addition of trailing
    /// padding would lead to a size larger than `isize::MAX`).
    #[doc(hidden)]
    #[inline]
    pub const fn pad_to_align(self) -> Self {
        use util::core_layout::padding_needed_for;

        let size_info = match self.size_info {
            // For sized layouts, we add the minimum amount of trailing padding
            // needed to satisfy alignment.
            SizeInfo::Sized { _size: unpadded_size } => {
                let padding = padding_needed_for(unpadded_size, self.align);
                let size = match unpadded_size.checked_add(padding) {
                    Some(size) => size,
                    None => panic!("Adding padding caused size to overflow `usize`."),
                };
                SizeInfo::Sized { _size: size }
            }
            // For DST layouts, trailing padding depends on the length of the
            // trailing DST and is computed at runtime. This does not alter the
            // offset or element size of the layout, so we leave `size_info`
            // unchanged.
            size_info @ SizeInfo::SliceDst(_) => size_info,
        };

        DstLayout { align: self.align, size_info }
    }

    /// Validates that a cast is sound from a layout perspective.
    ///
    /// Validates that the size and alignment requirements of a type with the
    /// layout described in `self` would not be violated by performing a
    /// `cast_type` cast from a pointer with address `addr` which refers to a
    /// memory region of size `bytes_len`.
    ///
    /// If the cast is valid, `validate_cast_and_convert_metadata` returns
    /// `(elems, split_at)`. If `self` describes a dynamically-sized type, then
    /// `elems` is the maximum number of trailing slice elements for which a
    /// cast would be valid (for sized types, `elem` is meaningless and should
    /// be ignored). `split_at` is the index at which to split the memory region
    /// in order for the prefix (suffix) to contain the result of the cast, and
    /// in order for the remaining suffix (prefix) to contain the leftover
    /// bytes.
    ///
    /// There are three conditions under which a cast can fail:
    /// - The smallest possible value for the type is larger than the provided
    ///   memory region
    /// - A prefix cast is requested, and `addr` does not satisfy `self`'s
    ///   alignment requirement
    /// - A suffix cast is requested, and `addr + bytes_len` does not satisfy
    ///   `self`'s alignment requirement (as a consequence, since all instances
    ///   of the type are a multiple of its alignment, no size for the type will
    ///   result in a starting address which is properly aligned)
    ///
    /// # Safety
    ///
    /// The caller may assume that this implementation is correct, and may rely
    /// on that assumption for the soundness of their code. In particular, the
    /// caller may assume that, if `validate_cast_and_convert_metadata` returns
    /// `Some((elems, split_at))`, then:
    /// - A pointer to the type (for dynamically sized types, this includes
    ///   `elems` as its pointer metadata) describes an object of size `size <=
    ///   bytes_len`
    /// - If this is a prefix cast:
    ///   - `addr` satisfies `self`'s alignment
    ///   - `size == split_at`
    /// - If this is a suffix cast:
    ///   - `split_at == bytes_len - size`
    ///   - `addr + split_at` satisfies `self`'s alignment
    ///
    /// Note that this method does *not* ensure that a pointer constructed from
    /// its return values will be a valid pointer. In particular, this method
    /// does not reason about `isize` overflow, which is a requirement of many
    /// Rust pointer APIs, and may at some point be determined to be a validity
    /// invariant of pointer types themselves. This should never be a problem so
    /// long as the arguments to this method are derived from a known-valid
    /// pointer (e.g., one derived from a safe Rust reference), but it is
    /// nonetheless the caller's responsibility to justify that pointer
    /// arithmetic will not overflow based on a safety argument *other than* the
    /// mere fact that this method returned successfully.
    ///
    /// # Panics
    ///
    /// `validate_cast_and_convert_metadata` will panic if `self` describes a
    /// DST whose trailing slice element is zero-sized.
    ///
    /// If `addr + bytes_len` overflows `usize`,
    /// `validate_cast_and_convert_metadata` may panic, or it may return
    /// incorrect results. No guarantees are made about when
    /// `validate_cast_and_convert_metadata` will panic. The caller should not
    /// rely on `validate_cast_and_convert_metadata` panicking in any particular
    /// condition, even if `debug_assertions` are enabled.
    #[allow(unused)]
    const fn validate_cast_and_convert_metadata(
        &self,
        addr: usize,
        bytes_len: usize,
        cast_type: _CastType,
    ) -> Option<(usize, usize)> {
        // `debug_assert!`, but with `#[allow(clippy::arithmetic_side_effects)]`.
        macro_rules! __debug_assert {
            ($e:expr $(, $msg:expr)?) => {
                debug_assert!({
                    #[allow(clippy::arithmetic_side_effects)]
                    let e = $e;
                    e
                } $(, $msg)?);
            };
        }

        // Note that, in practice, `self` is always a compile-time constant. We
        // do this check earlier than needed to ensure that we always panic as a
        // result of bugs in the program (such as calling this function on an
        // invalid type) instead of allowing this panic to be hidden if the cast
        // would have failed anyway for runtime reasons (such as a too-small
        // memory region).
        //
        // TODO(#67): Once our MSRV is 1.65, use let-else:
        // https://blog.rust-lang.org/2022/11/03/Rust-1.65.0.html#let-else-statements
        let size_info = match self.size_info.try_to_nonzero_elem_size() {
            Some(size_info) => size_info,
            None => panic!("attempted to cast to slice type with zero-sized element"),
        };

        // Precondition
        __debug_assert!(addr.checked_add(bytes_len).is_some(), "`addr` + `bytes_len` > usize::MAX");

        // Alignment checks go in their own block to avoid introducing variables
        // into the top-level scope.
        {
            // We check alignment for `addr` (for prefix casts) or `addr +
            // bytes_len` (for suffix casts). For a prefix cast, the correctness
            // of this check is trivial - `addr` is the address the object will
            // live at.
            //
            // For a suffix cast, we know that all valid sizes for the type are
            // a multiple of the alignment (and by safety precondition, we know
            // `DstLayout` may only describe valid Rust types). Thus, a
            // validly-sized instance which lives at a validly-aligned address
            // must also end at a validly-aligned address. Thus, if the end
            // address for a suffix cast (`addr + bytes_len`) is not aligned,
            // then no valid start address will be aligned either.
            let offset = match cast_type {
                _CastType::_Prefix => 0,
                _CastType::_Suffix => bytes_len,
            };

            // Addition is guaranteed not to overflow because `offset <=
            // bytes_len`, and `addr + bytes_len <= usize::MAX` is a
            // precondition of this method. Modulus is guaranteed not to divide
            // by 0 because `align` is non-zero.
            #[allow(clippy::arithmetic_side_effects)]
            if (addr + offset) % self.align.get() != 0 {
                return None;
            }
        }

        let (elems, self_bytes) = match size_info {
            SizeInfo::Sized { _size: size } => {
                if size > bytes_len {
                    return None;
                }
                (0, size)
            }
            SizeInfo::SliceDst(TrailingSliceLayout { _offset: offset, _elem_size: elem_size }) => {
                // Calculate the maximum number of bytes that could be consumed
                // - any number of bytes larger than this will either not be a
                // multiple of the alignment, or will be larger than
                // `bytes_len`.
                let max_total_bytes =
                    util::round_down_to_next_multiple_of_alignment(bytes_len, self.align);
                // Calculate the maximum number of bytes that could be consumed
                // by the trailing slice.
                //
                // TODO(#67): Once our MSRV is 1.65, use let-else:
                // https://blog.rust-lang.org/2022/11/03/Rust-1.65.0.html#let-else-statements
                let max_slice_and_padding_bytes = match max_total_bytes.checked_sub(offset) {
                    Some(max) => max,
                    // `bytes_len` too small even for 0 trailing slice elements.
                    None => return None,
                };

                // Calculate the number of elements that fit in
                // `max_slice_and_padding_bytes`; any remaining bytes will be
                // considered padding.
                //
                // Guaranteed not to divide by zero: `elem_size` is non-zero.
                #[allow(clippy::arithmetic_side_effects)]
                let elems = max_slice_and_padding_bytes / elem_size.get();
                // Guaranteed not to overflow on multiplication: `usize::MAX >=
                // max_slice_and_padding_bytes >= (max_slice_and_padding_bytes /
                // elem_size) * elem_size`.
                //
                // Guaranteed not to overflow on addition:
                // - max_slice_and_padding_bytes == max_total_bytes - offset
                // - elems * elem_size <= max_slice_and_padding_bytes == max_total_bytes - offset
                // - elems * elem_size + offset <= max_total_bytes <= usize::MAX
                #[allow(clippy::arithmetic_side_effects)]
                let without_padding = offset + elems * elem_size.get();
                // `self_bytes` is equal to the offset bytes plus the bytes
                // consumed by the trailing slice plus any padding bytes
                // required to satisfy the alignment. Note that we have computed
                // the maximum number of trailing slice elements that could fit
                // in `self_bytes`, so any padding is guaranteed to be less than
                // the size of an extra element.
                //
                // Guaranteed not to overflow:
                // - By previous comment: without_padding == elems * elem_size +
                //   offset <= max_total_bytes
                // - By construction, `max_total_bytes` is a multiple of
                //   `self.align`.
                // - At most, adding padding needed to round `without_padding`
                //   up to the next multiple of the alignment will bring
                //   `self_bytes` up to `max_total_bytes`.
                #[allow(clippy::arithmetic_side_effects)]
                let self_bytes = without_padding
                    + util::core_layout::padding_needed_for(without_padding, self.align);
                (elems, self_bytes)
            }
        };

        __debug_assert!(self_bytes <= bytes_len);

        let split_at = match cast_type {
            _CastType::_Prefix => self_bytes,
            // Guaranteed not to underflow:
            // - In the `Sized` branch, only returns `size` if `size <=
            //   bytes_len`.
            // - In the `SliceDst` branch, calculates `self_bytes <=
            //   max_toatl_bytes`, which is upper-bounded by `bytes_len`.
            #[allow(clippy::arithmetic_side_effects)]
            _CastType::_Suffix => bytes_len - self_bytes,
        };

        Some((elems, split_at))
    }
}

/// A trait which carries information about a type's layout that is used by the
/// internals of this crate.
///
/// This trait is not meant for consumption by code outside of this crate. While
/// the normal semver stability guarantees apply with respect to which types
/// implement this trait and which trait implementations are implied by this
/// trait, no semver stability guarantees are made regarding its internals; they
/// may change at any time, and code which makes use of them may break.
///
/// # Safety
///
/// This trait does not convey any safety guarantees to code outside this crate.
#[doc(hidden)] // TODO: Remove this once KnownLayout is used by other APIs
pub unsafe trait KnownLayout {
    // The `Self: Sized` bound makes it so that `KnownLayout` can still be
    // object safe. It's not currently object safe thanks to `const LAYOUT`, and
    // it likely won't be in the future, but there's no reason not to be
    // forwards-compatible with object safety.
    #[doc(hidden)]
    fn only_derive_is_allowed_to_implement_this_trait()
    where
        Self: Sized;

    #[doc(hidden)]
    const LAYOUT: DstLayout;

    /// SAFETY: The returned pointer has the same address and provenance as
    /// `bytes`. If `Self` is a DST, the returned pointer's referent has `elems`
    /// elements in its trailing slice. If `Self` is sized, `elems` is ignored.
    #[doc(hidden)]
    fn raw_from_ptr_len(bytes: NonNull<u8>, elems: usize) -> NonNull<Self>;
}

// SAFETY: Delegates safety to `DstLayout::for_slice`.
unsafe impl<T: KnownLayout> KnownLayout for [T] {
    #[allow(clippy::missing_inline_in_public_items)]
    fn only_derive_is_allowed_to_implement_this_trait()
    where
        Self: Sized,
    {
    }
    const LAYOUT: DstLayout = DstLayout::for_slice::<T>();

    // SAFETY: `.cast` preserves address and provenance. The returned pointer
    // refers to an object with `elems` elements by construction.
    #[inline(always)]
    fn raw_from_ptr_len(data: NonNull<u8>, elems: usize) -> NonNull<Self> {
        // TODO(#67): Remove this allow. See NonNullExt for more details.
        #[allow(unstable_name_collisions)]
        NonNull::slice_from_raw_parts(data.cast::<T>(), elems)
    }
}

#[rustfmt::skip]
impl_known_layout!(
    (),
    u8, i8, u16, i16, u32, i32, u64, i64, u128, i128, usize, isize, f32, f64,
    bool, char,
    NonZeroU8, NonZeroI8, NonZeroU16, NonZeroI16, NonZeroU32, NonZeroI32,
    NonZeroU64, NonZeroI64, NonZeroU128, NonZeroI128, NonZeroUsize, NonZeroIsize
);
#[rustfmt::skip]
impl_known_layout!(
    T         => Option<T>,
    T: ?Sized => PhantomData<T>,
    T         => Wrapping<T>,
    T         => MaybeUninit<T>,
    T: ?Sized => *const T,
    T: ?Sized => *mut T,
);
impl_known_layout!(const N: usize, T => [T; N]);

safety_comment! {
    /// SAFETY:
    /// `str` and `ManuallyDrop<[T]>` [1] have the same representations as
    /// `[u8]` and `[T]` repsectively. `str` has different bit validity than
    /// `[u8]`, but that doesn't affect the soundness of this impl.
    ///
    /// [1] Per https://doc.rust-lang.org/nightly/core/mem/struct.ManuallyDrop.html:
    ///
    ///   `ManuallyDrop<T>` is guaranteed to have the same layout and bit
    ///   validity as `T`
    ///
    /// TODO(#429):
    /// -  Add quotes from docs.
    /// -  Once [1] (added in
    /// https://github.com/rust-lang/rust/pull/115522) is available on stable,
    /// quote the stable docs instead of the nightly docs.
    unsafe_impl_known_layout!(#[repr([u8])] str);
    unsafe_impl_known_layout!(T: ?Sized + KnownLayout => #[repr(T)] ManuallyDrop<T>);
}

/// Analyzes whether a type is [`FromZeroes`].
///
/// This derive analyzes, at compile time, whether the annotated type satisfies
/// the [safety conditions] of `FromZeroes` and implements `FromZeroes` if it is
/// sound to do so. This derive can be applied to structs, enums, and unions;
/// e.g.:
///
/// ```
/// # use zerocopy_derive::FromZeroes;
/// #[derive(FromZeroes)]
/// struct MyStruct {
/// # /*
///     ...
/// # */
/// }
///
/// #[derive(FromZeroes)]
/// #[repr(u8)]
/// enum MyEnum {
/// #   Variant0,
/// # /*
///     ...
/// # */
/// }
///
/// #[derive(FromZeroes)]
/// union MyUnion {
/// #   variant: u8,
/// # /*
///     ...
/// # */
/// }
/// ```
///
/// [safety conditions]: trait@FromZeroes#safety
///
/// # Analysis
///
/// *This section describes, roughly, the analysis performed by this derive to
/// determine whether it is sound to implement `FromZeroes` for a given type.
/// Unless you are modifying the implementation of this derive, or attempting to
/// manually implement `FromZeroes` for a type yourself, you don't need to read
/// this section.*
///
/// If a type has the following properties, then this derive can implement
/// `FromZeroes` for that type:
///
/// - If the type is a struct, all of its fields must be `FromZeroes`.
/// - If the type is an enum, it must be C-like (meaning that all variants have
///   no fields) and it must have a variant with a discriminant of `0`. See [the
///   reference] for a description of how discriminant values are chosen.
/// - The type must not contain any [`UnsafeCell`]s (this is required in order
///   for it to be sound to construct a `&[u8]` and a `&T` to the same region of
///   memory). The type may contain references or pointers to `UnsafeCell`s so
///   long as those values can themselves be initialized from zeroes
///   (`FromZeroes` is not currently implemented for, e.g.,
///   `Option<&UnsafeCell<_>>`, but it could be one day).
///
/// This analysis is subject to change. Unsafe code may *only* rely on the
/// documented [safety conditions] of `FromZeroes`, and must *not* rely on the
/// implementation details of this derive.
///
/// [the reference]: https://doc.rust-lang.org/reference/items/enumerations.html#custom-discriminant-values-for-fieldless-enumerations
/// [`UnsafeCell`]: core::cell::UnsafeCell
///
/// ## Why isn't an explicit representation required for structs?
///
/// Neither this derive, nor the [safety conditions] of `FromZeroes`, requires
/// that structs are marked with `#[repr(C)]`.
///
/// Per the [Rust reference](reference),
///
/// > The representation of a type can change the padding between fields, but
/// > does not change the layout of the fields themselves.
///
/// [reference]: https://doc.rust-lang.org/reference/type-layout.html#representations
///
/// Since the layout of structs only consists of padding bytes and field bytes,
/// a struct is soundly `FromZeroes` if:
/// 1. its padding is soundly `FromZeroes`, and
/// 2. its fields are soundly `FromZeroes`.
///
/// The answer to the first question is always yes: padding bytes do not have
/// any validity constraints. A [discussion] of this question in the Unsafe Code
/// Guidelines Working Group concluded that it would be virtually unimaginable
/// for future versions of rustc to add validity constraints to padding bytes.
///
/// [discussion]: https://github.com/rust-lang/unsafe-code-guidelines/issues/174
///
/// Whether a struct is soundly `FromZeroes` therefore solely depends on whether
/// its fields are `FromZeroes`.
// TODO(#146): Document why we don't require an enum to have an explicit `repr`
// attribute.
#[cfg(any(feature = "derive", test))]
#[cfg_attr(doc_cfg, doc(cfg(feature = "derive")))]
pub use zerocopy_derive::FromZeroes;

/// Types whose validity can be checked at runtime, allowing them to be
/// conditionally converted from byte slices.
///
/// WARNING: Do not implement this trait yourself! Instead, use
/// `#[derive(TryFromBytes)]`.
///
/// `TryFromBytes` types can safely be deserialized from an untrusted sequence
/// of bytes by performing a runtime check that the byte sequence contains a
/// valid instance of `Self`.
///
/// `TryFromBytes` is ignorant of byte order. For byte order-aware types, see
/// the [`byteorder`] module.
///
/// # What is a "valid instance"?
///
/// In Rust, each type has *bit validity*, which refers to the set of bit
/// patterns which may appear in an instance of that type. It is impossible for
/// safe Rust code to produce values which violate bit validity (ie, values
/// outside of the "valid" set of bit patterns). If `unsafe` code produces an
/// invalid value, this is considered [undefined behavior].
///
/// Rust's bit validity rules are currently being decided, which means that some
/// types have three classes of bit patterns: those which are definitely valid,
/// and whose validity is documented in the language; those which may or may not
/// be considered valid at some point in the future; and those which are
/// definitely invalid.
///
/// Zerocopy takes a conservative approach, and only considers a bit pattern to
/// be valid if its validity is a documenteed guarantee provided by the
/// language.
///
/// For most use cases, Rust's current guarantees align with programmers'
/// intuitions about what ought to be valid. As a result, zerocopy's
/// conservatism should not affect most users. One notable exception is unions,
/// whose bit validity is very up in the air; zerocopy does not permit
/// implementing `TryFromBytes` for any union type.
///
/// If you are negatively affected by lack of support for a particular type,
/// we encourage you to let us know by [filing an issue][github-repo].
///
/// # Safety
///
/// On its own, `T: TryFromBytes` does not make any guarantees about the layout
/// or representation of `T`. It merely provides the ability to perform a
/// validity check at runtime via methods like [`try_from_ref`].
///
/// Currently, it is not possible to stably implement `TryFromBytes` other than
/// by using `#[derive(TryFromBytes)]`. While there are `#[doc(hidden)]` items
/// on this trait that provide well-defined safety invariants, no stability
/// guarantees are made with respect to these items. In particular, future
/// releases of zerocopy may make backwards-breaking changes to these items,
/// including changes that only affect soundness, which may cause code which
/// uses those items to silently become unsound.
///
/// [undefined behavior]: https://raphlinus.github.io/programming/rust/2018/08/17/undefined-behavior.html
/// [github-repo]: https://github.com/google/zerocopy
/// [`try_from_ref`]: TryFromBytes::try_from_ref
// TODO(#5): Update `try_from_ref` doc link once it exists
#[doc(hidden)]
pub unsafe trait TryFromBytes {
    /// Does a given memory range contain a valid instance of `Self`?
    ///
    /// # Safety
    ///
    /// ## Preconditions
    ///
    /// The memory referenced by `candidate` may only be accessed via reads for
    /// the duration of this method call. This prohibits writes through mutable
    /// references and through [`UnsafeCell`]s. There may exist immutable
    /// references to the same memory which contain `UnsafeCell`s so long as:
    /// - Those `UnsafeCell`s exist at the same byte ranges as `UnsafeCell`s in
    ///   `Self`. This is a bidirectional property: `Self` may not contain
    ///   `UnsafeCell`s where other references to the same memory do not, and
    ///   vice-versa.
    /// - Those `UnsafeCell`s are never used to perform mutation for the
    ///   duration of this method call.
    ///
    /// The memory referenced by `candidate` may not be referenced by any
    /// mutable references even if these references are not used to perform
    /// mutation.
    ///
    /// `candidate` is not required to refer to a valid `Self`. However, it must
    /// satisfy the requirement that uninitialized bytes may only be present
    /// where it is possible for them to be present in `Self`. This is a dynamic
    /// property: if, at a particular byte offset, a valid enum discriminant is
    /// set, the subsequent bytes may only have uninitialized bytes as
    /// specificed by the corresponding enum.
    ///
    /// Formally, given `len = size_of_val_raw(candidate)`, at every byte
    /// offset, `b`, in the range `[0, len)`:
    /// - If, in all instances `s: Self` of length `len`, the byte at offset `b`
    ///   in `s` is initialized, then the byte at offset `b` within `*candidate`
    ///   must be initialized.
    /// - Let `c` be the contents of the byte range `[0, b)` in `*candidate`.
    ///   Let `S` be the subset of valid instances of `Self` of length `len`
    ///   which contain `c` in the offset range `[0, b)`. If, for all instances
    ///   of `s: Self` in `S`, the byte at offset `b` in `s` is initialized,
    ///   then the byte at offset `b` in `*candidate` must be initialized.
    ///
    ///   Pragmatically, this means that if `*candidate` is guaranteed to
    ///   contain an enum type at a particular offset, and the enum discriminant
    ///   stored in `*candidate` corresponds to a valid variant of that enum
    ///   type, then it is guaranteed that the appropriate bytes of `*candidate`
    ///   are initialized as defined by that variant's bit validity (although
    ///   note that the variant may contain another enum type, in which case the
    ///   same rules apply depending on the state of its discriminant, and so on
    ///   recursively).
    ///
    /// ## Postconditions
    ///
    /// Unsafe code may assume that, if `is_bit_valid(candidate)` returns true,
    /// `*candidate` contains a valid `Self`.
    ///
    /// # Panics
    ///
    /// `is_bit_valid` may panic. Callers are responsible for ensuring that any
    /// `unsafe` code remains sound even in the face of `is_bit_valid`
    /// panicking. (We support user-defined validation routines; so long as
    /// these routines are not required to be `unsafe`, there is no way to
    /// ensure that these do not generate panics.)
    ///
    /// [`UnsafeCell`]: core::cell::UnsafeCell
    #[doc(hidden)]
    unsafe fn is_bit_valid(candidate: Ptr<'_, Self>) -> bool;

    /// Attempts to interpret a byte slice as a `Self`.
    ///
    /// `try_from_ref` validates that `bytes` contains a valid `Self`, and that
    /// it satisfies `Self`'s alignment requirement. If it does, then `bytes` is
    /// reinterpreted as a `Self`.
    ///
    /// Note that Rust's bit validity rules are still being decided. As such,
    /// there exist types whose bit validity is ambiguous. See the
    /// `TryFromBytes` docs for a discussion of how these cases are handled.
    // TODO(#251): In a future in which we distinguish between `FromBytes` and
    // `RefFromBytes`, this requires `where Self: RefFromBytes` to disallow
    // interior mutability.
    #[inline]
    #[doc(hidden)] // TODO(#5): Finalize name before remove this attribute.
    fn try_from_ref(bytes: &[u8]) -> Option<&Self>
    where
        Self: KnownLayout,
    {
        let maybe_self = Ptr::from(bytes).try_cast_into_no_leftover::<Self>()?;

        // SAFETY:
        // - Since `bytes` is an immutable reference, we know that no mutable
        //   references exist to this memory region.
        // - Since `[u8]` contains no `UnsafeCell`s, we know there are no
        //   `&UnsafeCell` references to this memory region.
        // - Since we don't permit implementing `TryFromBytes` for types which
        //   contain `UnsafeCell`s, there are no `UnsafeCell`s in `Self`, and so
        //   the requirement that all references contain `UnsafeCell`s at the
        //   same offsets is trivially satisfied.
        // - All bytes of `bytes` are initialized.
        //
        // This call may panic. If that happens, it doesn't cause any soundness
        // issues, as we have not generated any invalid state which we need to
        // fix before returning.
        if unsafe { !Self::is_bit_valid(maybe_self) } {
            return None;
        }

        // SAFETY:
        // - Preconditions for `as_ref`:
        //   - `is_bit_valid` guarantees that `*maybe_self` contains a valid
        //     `Self`. Since `&[u8]` does not permit interior mutation, this
        //     cannot be invalidated after this method returns.
        //   - Since the argument and return types are immutable references,
        //     Rust will prevent the caller from producing any mutable
        //     references to the same memory region.
        //   - Since `Self` is not allowed to contain any `UnsafeCell`s and the
        //     same is true of `[u8]`, interior mutation is not possible. Thus,
        //     no mutation is possible. For the same reason, there is no
        //     mismatch between the two types in terms of which byte ranges are
        //     referenced as `UnsafeCell`s.
        // - Since interior mutation isn't possible within `Self`, there's no
        //   way for the returned reference to be used to modify the byte range,
        //   and thus there's no way for the returned reference to be used to
        //   write an invalid `[u8]` which would be observable via the original
        //   `&[u8]`.
        Some(unsafe { maybe_self.as_ref() })
    }
}

/// Types for which a sequence of bytes all set to zero represents a valid
/// instance of the type.
///
/// Any memory region of the appropriate length which is guaranteed to contain
/// only zero bytes can be viewed as any `FromZeroes` type with no runtime
/// overhead. This is useful whenever memory is known to be in a zeroed state,
/// such memory returned from some allocation routines.
///
/// # Implementation
///
/// **Do not implement this trait yourself!** Instead, use
/// [`#[derive(FromZeroes)]`][derive] (requires the `derive` Cargo feature);
/// e.g.:
///
/// ```
/// # use zerocopy_derive::FromZeroes;
/// #[derive(FromZeroes)]
/// struct MyStruct {
/// # /*
///     ...
/// # */
/// }
///
/// #[derive(FromZeroes)]
/// #[repr(u8)]
/// enum MyEnum {
/// #   Variant0,
/// # /*
///     ...
/// # */
/// }
///
/// #[derive(FromZeroes)]
/// union MyUnion {
/// #   variant: u8,
/// # /*
///     ...
/// # */
/// }
/// ```
///
/// This derive performs a sophisticated, compile-time safety analysis to
/// determine whether a type is `FromZeroes`.
///
/// # Safety
///
/// *This section describes what is required in order for `T: FromZeroes`, and
/// what unsafe code may assume of such types. If you don't plan on implementing
/// `FromZeroes` manually, and you don't plan on writing unsafe code that
/// operates on `FromZeroes` types, then you don't need to read this section.*
///
/// If `T: FromZeroes`, then unsafe code may assume that:
/// - It is sound to treat any initialized sequence of zero bytes of length
///   `size_of::<T>()` as a `T`.
/// - Given `b: &[u8]` where `b.len() == size_of::<T>()`, `b` is aligned to
///   `align_of::<T>()`, and `b` contains only zero bytes, it is sound to
///   construct a `t: &T` at the same address as `b`, and it is sound for both
///   `b` and `t` to be live at the same time.
///
/// If a type is marked as `FromZeroes` which violates this contract, it may
/// cause undefined behavior.
///
/// `#[derive(FromZeroes)]` only permits [types which satisfy these
/// requirements][derive-analysis].
///
#[cfg_attr(
    feature = "derive",
    doc = "[derive]: zerocopy_derive::FromZeroes",
    doc = "[derive-analysis]: zerocopy_derive::FromZeroes#analysis"
)]
#[cfg_attr(
    not(feature = "derive"),
    doc = concat!("[derive]: https://docs.rs/zerocopy/", env!("CARGO_PKG_VERSION"), "/zerocopy/derive.FromZeroes.html"),
    doc = concat!("[derive-analysis]: https://docs.rs/zerocopy/", env!("CARGO_PKG_VERSION"), "/zerocopy/derive.FromZeroes.html#analysis"),
)]
pub unsafe trait FromZeroes {
    // The `Self: Sized` bound makes it so that `FromZeroes` is still object
    // safe.
    #[doc(hidden)]
    fn only_derive_is_allowed_to_implement_this_trait()
    where
        Self: Sized;

    /// Overwrites `self` with zeroes.
    ///
    /// Sets every byte in `self` to 0. While this is similar to doing `*self =
    /// Self::new_zeroed()`, it differs in that `zero` does not semantically
    /// drop the current value and replace it with a new one - it simply
    /// modifies the bytes of the existing value.
    ///
    /// # Examples
    ///
    /// ```
    /// # use zerocopy::FromZeroes;
    /// # use zerocopy_derive::*;
    /// #
    /// #[derive(FromZeroes)]
    /// #[repr(C)]
    /// struct PacketHeader {
    ///     src_port: [u8; 2],
    ///     dst_port: [u8; 2],
    ///     length: [u8; 2],
    ///     checksum: [u8; 2],
    /// }
    ///
    /// let mut header = PacketHeader {
    ///     src_port: 100u16.to_be_bytes(),
    ///     dst_port: 200u16.to_be_bytes(),
    ///     length: 300u16.to_be_bytes(),
    ///     checksum: 400u16.to_be_bytes(),
    /// };
    ///
    /// header.zero();
    ///
    /// assert_eq!(header.src_port, [0, 0]);
    /// assert_eq!(header.dst_port, [0, 0]);
    /// assert_eq!(header.length, [0, 0]);
    /// assert_eq!(header.checksum, [0, 0]);
    /// ```
    #[inline(always)]
    fn zero(&mut self) {
        let slf: *mut Self = self;
        let len = mem::size_of_val(self);
        // SAFETY:
        // - `self` is guaranteed by the type system to be valid for writes of
        //   size `size_of_val(self)`.
        // - `u8`'s alignment is 1, and thus `self` is guaranteed to be aligned
        //   as required by `u8`.
        // - Since `Self: FromZeroes`, the all-zeroes instance is a valid
        //   instance of `Self.`
        //
        // TODO(#429): Add references to docs and quotes.
        unsafe { ptr::write_bytes(slf.cast::<u8>(), 0, len) };
    }

    /// Creates an instance of `Self` from zeroed bytes.
    ///
    /// # Examples
    ///
    /// ```
    /// # use zerocopy::FromZeroes;
    /// # use zerocopy_derive::*;
    /// #
    /// #[derive(FromZeroes)]
    /// #[repr(C)]
    /// struct PacketHeader {
    ///     src_port: [u8; 2],
    ///     dst_port: [u8; 2],
    ///     length: [u8; 2],
    ///     checksum: [u8; 2],
    /// }
    ///
    /// let header: PacketHeader = FromZeroes::new_zeroed();
    ///
    /// assert_eq!(header.src_port, [0, 0]);
    /// assert_eq!(header.dst_port, [0, 0]);
    /// assert_eq!(header.length, [0, 0]);
    /// assert_eq!(header.checksum, [0, 0]);
    /// ```
    #[inline(always)]
    fn new_zeroed() -> Self
    where
        Self: Sized,
    {
        // SAFETY: `FromZeroes` says that the all-zeroes bit pattern is legal.
        unsafe { mem::zeroed() }
    }

    /// Creates a `Box<Self>` from zeroed bytes.
    ///
    /// This function is useful for allocating large values on the heap and
    /// zero-initializing them, without ever creating a temporary instance of
    /// `Self` on the stack. For example, `<[u8; 1048576]>::new_box_zeroed()`
    /// will allocate `[u8; 1048576]` directly on the heap; it does not require
    /// storing `[u8; 1048576]` in a temporary variable on the stack.
    ///
    /// On systems that use a heap implementation that supports allocating from
    /// pre-zeroed memory, using `new_box_zeroed` (or related functions) may
    /// have performance benefits.
    ///
    /// Note that `Box<Self>` can be converted to `Arc<Self>` and other
    /// container types without reallocation.
    ///
    /// # Panics
    ///
    /// Panics if allocation of `size_of::<Self>()` bytes fails.
    #[cfg(feature = "alloc")]
    #[cfg_attr(doc_cfg, doc(cfg(feature = "alloc")))]
    #[inline]
    fn new_box_zeroed() -> Box<Self>
    where
        Self: Sized,
    {
        // If `T` is a ZST, then return a proper boxed instance of it. There is
        // no allocation, but `Box` does require a correct dangling pointer.
        let layout = Layout::new::<Self>();
        if layout.size() == 0 {
            return Box::new(Self::new_zeroed());
        }

        // TODO(#429): Add a "SAFETY" comment and remove this `allow`.
        #[allow(clippy::undocumented_unsafe_blocks)]
        let ptr = unsafe { alloc::alloc::alloc_zeroed(layout).cast::<Self>() };
        if ptr.is_null() {
            alloc::alloc::handle_alloc_error(layout);
        }
        // TODO(#429): Add a "SAFETY" comment and remove this `allow`.
        #[allow(clippy::undocumented_unsafe_blocks)]
        unsafe {
            Box::from_raw(ptr)
        }
    }

    /// Creates a `Box<[Self]>` (a boxed slice) from zeroed bytes.
    ///
    /// This function is useful for allocating large values of `[Self]` on the
    /// heap and zero-initializing them, without ever creating a temporary
    /// instance of `[Self; _]` on the stack. For example,
    /// `u8::new_box_slice_zeroed(1048576)` will allocate the slice directly on
    /// the heap; it does not require storing the slice on the stack.
    ///
    /// On systems that use a heap implementation that supports allocating from
    /// pre-zeroed memory, using `new_box_slice_zeroed` may have performance
    /// benefits.
    ///
    /// If `Self` is a zero-sized type, then this function will return a
    /// `Box<[Self]>` that has the correct `len`. Such a box cannot contain any
    /// actual information, but its `len()` property will report the correct
    /// value.
    ///
    /// # Panics
    ///
    /// * Panics if `size_of::<Self>() * len` overflows.
    /// * Panics if allocation of `size_of::<Self>() * len` bytes fails.
    #[cfg(feature = "alloc")]
    #[cfg_attr(doc_cfg, doc(cfg(feature = "alloc")))]
    #[inline]
    fn new_box_slice_zeroed(len: usize) -> Box<[Self]>
    where
        Self: Sized,
    {
        let size = mem::size_of::<Self>()
            .checked_mul(len)
            .expect("mem::size_of::<Self>() * len overflows `usize`");
        let align = mem::align_of::<Self>();
        // On stable Rust versions <= 1.64.0, `Layout::from_size_align` has a
        // bug in which sufficiently-large allocations (those which, when
        // rounded up to the alignment, overflow `isize`) are not rejected,
        // which can cause undefined behavior. See #64 for details.
        //
        // TODO(#67): Once our MSRV is > 1.64.0, remove this assertion.
        #[allow(clippy::as_conversions)]
        let max_alloc = (isize::MAX as usize).saturating_sub(align);
        assert!(size <= max_alloc);
        // TODO(https://github.com/rust-lang/rust/issues/55724): Use
        // `Layout::repeat` once it's stabilized.
        let layout =
            Layout::from_size_align(size, align).expect("total allocation size overflows `isize`");

        let ptr = if layout.size() != 0 {
            // TODO(#429): Add a "SAFETY" comment and remove this `allow`.
            #[allow(clippy::undocumented_unsafe_blocks)]
            let ptr = unsafe { alloc::alloc::alloc_zeroed(layout).cast::<Self>() };
            if ptr.is_null() {
                alloc::alloc::handle_alloc_error(layout);
            }
            ptr
        } else {
            // `Box<[T]>` does not allocate when `T` is zero-sized or when `len`
            // is zero, but it does require a non-null dangling pointer for its
            // allocation.
            NonNull::<Self>::dangling().as_ptr()
        };

        // TODO(#429): Add a "SAFETY" comment and remove this `allow`.
        #[allow(clippy::undocumented_unsafe_blocks)]
        unsafe {
            Box::from_raw(slice::from_raw_parts_mut(ptr, len))
        }
    }

    /// Creates a `Vec<Self>` from zeroed bytes.
    ///
    /// This function is useful for allocating large values of `Vec`s and
    /// zero-initializing them, without ever creating a temporary instance of
    /// `[Self; _]` (or many temporary instances of `Self`) on the stack. For
    /// example, `u8::new_vec_zeroed(1048576)` will allocate directly on the
    /// heap; it does not require storing intermediate values on the stack.
    ///
    /// On systems that use a heap implementation that supports allocating from
    /// pre-zeroed memory, using `new_vec_zeroed` may have performance benefits.
    ///
    /// If `Self` is a zero-sized type, then this function will return a
    /// `Vec<Self>` that has the correct `len`. Such a `Vec` cannot contain any
    /// actual information, but its `len()` property will report the correct
    /// value.
    ///
    /// # Panics
    ///
    /// * Panics if `size_of::<Self>() * len` overflows.
    /// * Panics if allocation of `size_of::<Self>() * len` bytes fails.
    #[cfg(feature = "alloc")]
    #[cfg_attr(doc_cfg, doc(cfg(feature = "new_vec_zeroed")))]
    #[inline(always)]
    fn new_vec_zeroed(len: usize) -> Vec<Self>
    where
        Self: Sized,
    {
        Self::new_box_slice_zeroed(len).into()
    }
}

/// Analyzes whether a type is [`FromBytes`].
///
/// This derive analyzes, at compile time, whether the annotated type satisfies
/// the [safety conditions] of `FromBytes` and implements `FromBytes` if it is
/// sound to do so. This derive can be applied to structs, enums, and unions;
/// e.g.:
///
/// ```
/// # use zerocopy_derive::{FromBytes, FromZeroes};
/// #[derive(FromZeroes, FromBytes)]
/// struct MyStruct {
/// # /*
///     ...
/// # */
/// }
///
/// #[derive(FromZeroes, FromBytes)]
/// #[repr(u8)]
/// enum MyEnum {
/// #   V00, V01, V02, V03, V04, V05, V06, V07, V08, V09, V0A, V0B, V0C, V0D, V0E,
/// #   V0F, V10, V11, V12, V13, V14, V15, V16, V17, V18, V19, V1A, V1B, V1C, V1D,
/// #   V1E, V1F, V20, V21, V22, V23, V24, V25, V26, V27, V28, V29, V2A, V2B, V2C,
/// #   V2D, V2E, V2F, V30, V31, V32, V33, V34, V35, V36, V37, V38, V39, V3A, V3B,
/// #   V3C, V3D, V3E, V3F, V40, V41, V42, V43, V44, V45, V46, V47, V48, V49, V4A,
/// #   V4B, V4C, V4D, V4E, V4F, V50, V51, V52, V53, V54, V55, V56, V57, V58, V59,
/// #   V5A, V5B, V5C, V5D, V5E, V5F, V60, V61, V62, V63, V64, V65, V66, V67, V68,
/// #   V69, V6A, V6B, V6C, V6D, V6E, V6F, V70, V71, V72, V73, V74, V75, V76, V77,
/// #   V78, V79, V7A, V7B, V7C, V7D, V7E, V7F, V80, V81, V82, V83, V84, V85, V86,
/// #   V87, V88, V89, V8A, V8B, V8C, V8D, V8E, V8F, V90, V91, V92, V93, V94, V95,
/// #   V96, V97, V98, V99, V9A, V9B, V9C, V9D, V9E, V9F, VA0, VA1, VA2, VA3, VA4,
/// #   VA5, VA6, VA7, VA8, VA9, VAA, VAB, VAC, VAD, VAE, VAF, VB0, VB1, VB2, VB3,
/// #   VB4, VB5, VB6, VB7, VB8, VB9, VBA, VBB, VBC, VBD, VBE, VBF, VC0, VC1, VC2,
/// #   VC3, VC4, VC5, VC6, VC7, VC8, VC9, VCA, VCB, VCC, VCD, VCE, VCF, VD0, VD1,
/// #   VD2, VD3, VD4, VD5, VD6, VD7, VD8, VD9, VDA, VDB, VDC, VDD, VDE, VDF, VE0,
/// #   VE1, VE2, VE3, VE4, VE5, VE6, VE7, VE8, VE9, VEA, VEB, VEC, VED, VEE, VEF,
/// #   VF0, VF1, VF2, VF3, VF4, VF5, VF6, VF7, VF8, VF9, VFA, VFB, VFC, VFD, VFE,
/// #   VFF,
/// # /*
///     ...
/// # */
/// }
///
/// #[derive(FromZeroes, FromBytes)]
/// union MyUnion {
/// #   variant: u8,
/// # /*
///     ...
/// # */
/// }
/// ```
///
/// [safety conditions]: trait@FromBytes#safety
///
/// # Analysis
///
/// *This section describes, roughly, the analysis performed by this derive to
/// determine whether it is sound to implement `FromBytes` for a given type.
/// Unless you are modifying the implementation of this derive, or attempting to
/// manually implement `FromBytes` for a type yourself, you don't need to read
/// this section.*
///
/// If a type has the following properties, then this derive can implement
/// `FromBytes` for that type:
///
/// - If the type is a struct, all of its fields must be `FromBytes`.
/// - If the type is an enum:
///   - It must be a C-like enum (meaning that all variants have no fields).
///   - It must have a defined representation (`repr`s `C`, `u8`, `u16`, `u32`,
///     `u64`, `usize`, `i8`, `i16`, `i32`, `i64`, or `isize`).
///   - The maximum number of discriminants must be used (so that every possible
///     bit pattern is a valid one). Be very careful when using the `C`,
///     `usize`, or `isize` representations, as their size is
///     platform-dependent.
/// - The type must not contain any [`UnsafeCell`]s (this is required in order
///   for it to be sound to construct a `&[u8]` and a `&T` to the same region of
///   memory). The type may contain references or pointers to `UnsafeCell`s so
///   long as those values can themselves be initialized from zeroes
///   (`FromBytes` is not currently implemented for, e.g., `Option<*const
///   UnsafeCell<_>>`, but it could be one day).
///
/// [`UnsafeCell`]: core::cell::UnsafeCell
///
/// This analysis is subject to change. Unsafe code may *only* rely on the
/// documented [safety conditions] of `FromBytes`, and must *not* rely on the
/// implementation details of this derive.
///
/// ## Why isn't an explicit representation required for structs?
///
/// Neither this derive, nor the [safety conditions] of `FromBytes`, requires
/// that structs are marked with `#[repr(C)]`.
///
/// Per the [Rust reference](reference),
///
/// > The representation of a type can change the padding between fields, but
/// > does not change the layout of the fields themselves.
///
/// [reference]: https://doc.rust-lang.org/reference/type-layout.html#representations
///
/// Since the layout of structs only consists of padding bytes and field bytes,
/// a struct is soundly `FromBytes` if:
/// 1. its padding is soundly `FromBytes`, and
/// 2. its fields are soundly `FromBytes`.
///
/// The answer to the first question is always yes: padding bytes do not have
/// any validity constraints. A [discussion] of this question in the Unsafe Code
/// Guidelines Working Group concluded that it would be virtually unimaginable
/// for future versions of rustc to add validity constraints to padding bytes.
///
/// [discussion]: https://github.com/rust-lang/unsafe-code-guidelines/issues/174
///
/// Whether a struct is soundly `FromBytes` therefore solely depends on whether
/// its fields are `FromBytes`.
// TODO(#146): Document why we don't require an enum to have an explicit `repr`
// attribute.
#[cfg(any(feature = "derive", test))]
#[cfg_attr(doc_cfg, doc(cfg(feature = "derive")))]
pub use zerocopy_derive::FromBytes;

/// Types for which any bit pattern is valid.
///
/// Any memory region of the appropriate length which contains initialized bytes
/// can be viewed as any `FromBytes` type with no runtime overhead. This is
/// useful for efficiently parsing bytes as structured data.
///
/// # Implementation
///
/// **Do not implement this trait yourself!** Instead, use
/// [`#[derive(FromBytes)]`][derive] (requires the `derive` Cargo feature);
/// e.g.:
///
/// ```
/// # use zerocopy_derive::{FromBytes, FromZeroes};
/// #[derive(FromZeroes, FromBytes)]
/// struct MyStruct {
/// # /*
///     ...
/// # */
/// }
///
/// #[derive(FromZeroes, FromBytes)]
/// #[repr(u8)]
/// enum MyEnum {
/// #   V00, V01, V02, V03, V04, V05, V06, V07, V08, V09, V0A, V0B, V0C, V0D, V0E,
/// #   V0F, V10, V11, V12, V13, V14, V15, V16, V17, V18, V19, V1A, V1B, V1C, V1D,
/// #   V1E, V1F, V20, V21, V22, V23, V24, V25, V26, V27, V28, V29, V2A, V2B, V2C,
/// #   V2D, V2E, V2F, V30, V31, V32, V33, V34, V35, V36, V37, V38, V39, V3A, V3B,
/// #   V3C, V3D, V3E, V3F, V40, V41, V42, V43, V44, V45, V46, V47, V48, V49, V4A,
/// #   V4B, V4C, V4D, V4E, V4F, V50, V51, V52, V53, V54, V55, V56, V57, V58, V59,
/// #   V5A, V5B, V5C, V5D, V5E, V5F, V60, V61, V62, V63, V64, V65, V66, V67, V68,
/// #   V69, V6A, V6B, V6C, V6D, V6E, V6F, V70, V71, V72, V73, V74, V75, V76, V77,
/// #   V78, V79, V7A, V7B, V7C, V7D, V7E, V7F, V80, V81, V82, V83, V84, V85, V86,
/// #   V87, V88, V89, V8A, V8B, V8C, V8D, V8E, V8F, V90, V91, V92, V93, V94, V95,
/// #   V96, V97, V98, V99, V9A, V9B, V9C, V9D, V9E, V9F, VA0, VA1, VA2, VA3, VA4,
/// #   VA5, VA6, VA7, VA8, VA9, VAA, VAB, VAC, VAD, VAE, VAF, VB0, VB1, VB2, VB3,
/// #   VB4, VB5, VB6, VB7, VB8, VB9, VBA, VBB, VBC, VBD, VBE, VBF, VC0, VC1, VC2,
/// #   VC3, VC4, VC5, VC6, VC7, VC8, VC9, VCA, VCB, VCC, VCD, VCE, VCF, VD0, VD1,
/// #   VD2, VD3, VD4, VD5, VD6, VD7, VD8, VD9, VDA, VDB, VDC, VDD, VDE, VDF, VE0,
/// #   VE1, VE2, VE3, VE4, VE5, VE6, VE7, VE8, VE9, VEA, VEB, VEC, VED, VEE, VEF,
/// #   VF0, VF1, VF2, VF3, VF4, VF5, VF6, VF7, VF8, VF9, VFA, VFB, VFC, VFD, VFE,
/// #   VFF,
/// # /*
///     ...
/// # */
/// }
///
/// #[derive(FromZeroes, FromBytes)]
/// union MyUnion {
/// #   variant: u8,
/// # /*
///     ...
/// # */
/// }
/// ```
///
/// This derive performs a sophisticated, compile-time safety analysis to
/// determine whether a type is `FromBytes`.
///
/// # Safety
///
/// *This section describes what is required in order for `T: FromBytes`, and
/// what unsafe code may assume of such types. If you don't plan on implementing
/// `FromBytes` manually, and you don't plan on writing unsafe code that
/// operates on `FromBytes` types, then you don't need to read this section.*
///
/// If `T: FromBytes`, then unsafe code may assume that:
/// - It is sound to treat any initialized sequence of bytes of length
///   `size_of::<T>()` as a `T`.
/// - Given `b: &[u8]` where `b.len() == size_of::<T>()`, `b` is aligned to
///   `align_of::<T>()` it is sound to construct a `t: &T` at the same address
///   as `b`, and it is sound for both `b` and `t` to be live at the same time.
///
/// If a type is marked as `FromBytes` which violates this contract, it may
/// cause undefined behavior.
///
/// `#[derive(FromBytes)]` only permits [types which satisfy these
/// requirements][derive-analysis].
///
#[cfg_attr(
    feature = "derive",
    doc = "[derive]: zerocopy_derive::FromBytes",
    doc = "[derive-analysis]: zerocopy_derive::FromBytes#analysis"
)]
#[cfg_attr(
    not(feature = "derive"),
    doc = concat!("[derive]: https://docs.rs/zerocopy/", env!("CARGO_PKG_VERSION"), "/zerocopy/derive.FromBytes.html"),
    doc = concat!("[derive-analysis]: https://docs.rs/zerocopy/", env!("CARGO_PKG_VERSION"), "/zerocopy/derive.FromBytes.html#analysis"),
)]
pub unsafe trait FromBytes: FromZeroes {
    // The `Self: Sized` bound makes it so that `FromBytes` is still object
    // safe.
    #[doc(hidden)]
    fn only_derive_is_allowed_to_implement_this_trait()
    where
        Self: Sized;

    /// Interprets the given `bytes` as a `&Self` without copying.
    ///
    /// If `bytes.len() != size_of::<Self>()` or `bytes` is not aligned to
    /// `align_of::<Self>()`, this returns `None`.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerocopy::FromBytes;
    /// # use zerocopy_derive::*;
    ///
    /// #[derive(FromZeroes, FromBytes)]
    /// #[repr(C)]
    /// struct PacketHeader {
    ///     src_port: [u8; 2],
    ///     dst_port: [u8; 2],
    ///     length: [u8; 2],
    ///     checksum: [u8; 2],
    /// }
    ///
    /// // These bytes encode a `PacketHeader`.
    /// let bytes = [0, 1, 2, 3, 4, 5, 6, 7].as_slice();
    ///
    /// let header = PacketHeader::ref_from(bytes).unwrap();
    ///
    /// assert_eq!(header.src_port, [0, 1]);
    /// assert_eq!(header.dst_port, [2, 3]);
    /// assert_eq!(header.length, [4, 5]);
    /// assert_eq!(header.checksum, [6, 7]);
    /// ```
    #[inline]
    fn ref_from(bytes: &[u8]) -> Option<&Self>
    where
        Self: Sized,
    {
        Ref::<&[u8], Self>::new(bytes).map(Ref::into_ref)
    }

    /// Interprets the prefix of the given `bytes` as a `&Self` without copying.
    ///
    /// `ref_from_prefix` returns a reference to the first `size_of::<Self>()`
    /// bytes of `bytes`. If `bytes.len() < size_of::<Self>()` or `bytes` is not
    /// aligned to `align_of::<Self>()`, this returns `None`.
    ///
    /// To also access the prefix bytes, use [`Ref::new_from_prefix`]. Then, use
    /// [`Ref::into_ref`] to get a `&Self` with the same lifetime.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerocopy::FromBytes;
    /// # use zerocopy_derive::*;
    ///
    /// #[derive(FromZeroes, FromBytes)]
    /// #[repr(C)]
    /// struct PacketHeader {
    ///     src_port: [u8; 2],
    ///     dst_port: [u8; 2],
    ///     length: [u8; 2],
    ///     checksum: [u8; 2],
    /// }
    ///
    /// // These are more bytes than are needed to encode a `PacketHeader`.
    /// let bytes = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9].as_slice();
    ///
    /// let header = PacketHeader::ref_from_prefix(bytes).unwrap();
    ///
    /// assert_eq!(header.src_port, [0, 1]);
    /// assert_eq!(header.dst_port, [2, 3]);
    /// assert_eq!(header.length, [4, 5]);
    /// assert_eq!(header.checksum, [6, 7]);
    /// ```
    #[inline]
    fn ref_from_prefix(bytes: &[u8]) -> Option<&Self>
    where
        Self: Sized,
    {
        Ref::<&[u8], Self>::new_from_prefix(bytes).map(|(r, _)| r.into_ref())
    }

    /// Interprets the suffix of the given `bytes` as a `&Self` without copying.
    ///
    /// `ref_from_suffix` returns a reference to the last `size_of::<Self>()`
    /// bytes of `bytes`. If `bytes.len() < size_of::<Self>()` or the suffix of
    /// `bytes` is not aligned to `align_of::<Self>()`, this returns `None`.
    ///
    /// To also access the suffix bytes, use [`Ref::new_from_suffix`]. Then, use
    /// [`Ref::into_ref`] to get a `&Self` with the same lifetime.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerocopy::FromBytes;
    /// # use zerocopy_derive::*;
    ///
    /// #[derive(FromZeroes, FromBytes)]
    /// #[repr(C)]
    /// struct PacketTrailer {
    ///     frame_check_sequence: [u8; 4],
    /// }
    ///
    /// // These are more bytes than are needed to encode a `PacketTrailer`.
    /// let bytes = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9].as_slice();
    ///
    /// let trailer = PacketTrailer::ref_from_suffix(bytes).unwrap();
    ///
    /// assert_eq!(trailer.frame_check_sequence, [6, 7, 8, 9]);
    /// ```
    #[inline]
    fn ref_from_suffix(bytes: &[u8]) -> Option<&Self>
    where
        Self: Sized,
    {
        Ref::<&[u8], Self>::new_from_suffix(bytes).map(|(_, r)| r.into_ref())
    }

    /// Interprets the given `bytes` as a `&mut Self` without copying.
    ///
    /// If `bytes.len() != size_of::<Self>()` or `bytes` is not aligned to
    /// `align_of::<Self>()`, this returns `None`.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerocopy::FromBytes;
    /// # use zerocopy_derive::*;
    ///
    /// #[derive(AsBytes, FromZeroes, FromBytes)]
    /// #[repr(C)]
    /// struct PacketHeader {
    ///     src_port: [u8; 2],
    ///     dst_port: [u8; 2],
    ///     length: [u8; 2],
    ///     checksum: [u8; 2],
    /// }
    ///
    /// // These bytes encode a `PacketHeader`.
    /// let bytes = &mut [0, 1, 2, 3, 4, 5, 6, 7][..];
    ///
    /// let header = PacketHeader::mut_from(bytes).unwrap();
    ///
    /// assert_eq!(header.src_port, [0, 1]);
    /// assert_eq!(header.dst_port, [2, 3]);
    /// assert_eq!(header.length, [4, 5]);
    /// assert_eq!(header.checksum, [6, 7]);
    ///
    /// header.checksum = [0, 0];
    ///
    /// assert_eq!(bytes, [0, 1, 2, 3, 4, 5, 0, 0]);
    /// ```
    #[inline]
    fn mut_from(bytes: &mut [u8]) -> Option<&mut Self>
    where
        Self: Sized + AsBytes,
    {
        Ref::<&mut [u8], Self>::new(bytes).map(Ref::into_mut)
    }

    /// Interprets the prefix of the given `bytes` as a `&mut Self` without
    /// copying.
    ///
    /// `mut_from_prefix` returns a reference to the first `size_of::<Self>()`
    /// bytes of `bytes`. If `bytes.len() < size_of::<Self>()` or `bytes` is not
    /// aligned to `align_of::<Self>()`, this returns `None`.
    ///
    /// To also access the prefix bytes, use [`Ref::new_from_prefix`]. Then, use
    /// [`Ref::into_mut`] to get a `&mut Self` with the same lifetime.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerocopy::FromBytes;
    /// # use zerocopy_derive::*;
    ///
    /// #[derive(AsBytes, FromZeroes, FromBytes)]
    /// #[repr(C)]
    /// struct PacketHeader {
    ///     src_port: [u8; 2],
    ///     dst_port: [u8; 2],
    ///     length: [u8; 2],
    ///     checksum: [u8; 2],
    /// }
    ///
    /// // These are more bytes than are needed to encode a `PacketHeader`.
    /// let bytes = &mut [0, 1, 2, 3, 4, 5, 6, 7, 8, 9][..];
    ///
    /// let header = PacketHeader::mut_from_prefix(bytes).unwrap();
    ///
    /// assert_eq!(header.src_port, [0, 1]);
    /// assert_eq!(header.dst_port, [2, 3]);
    /// assert_eq!(header.length, [4, 5]);
    /// assert_eq!(header.checksum, [6, 7]);
    ///
    /// header.checksum = [0, 0];
    ///
    /// assert_eq!(bytes, [0, 1, 2, 3, 4, 5, 0, 0, 8, 9]);
    /// ```
    #[inline]
    fn mut_from_prefix(bytes: &mut [u8]) -> Option<&mut Self>
    where
        Self: Sized + AsBytes,
    {
        Ref::<&mut [u8], Self>::new_from_prefix(bytes).map(|(r, _)| r.into_mut())
    }

    /// Interprets the suffix of the given `bytes` as a `&mut Self` without copying.
    ///
    /// `mut_from_suffix` returns a reference to the last `size_of::<Self>()`
    /// bytes of `bytes`. If `bytes.len() < size_of::<Self>()` or the suffix of
    /// `bytes` is not aligned to `align_of::<Self>()`, this returns `None`.
    ///
    /// To also access the suffix bytes, use [`Ref::new_from_suffix`]. Then,
    /// use [`Ref::into_mut`] to get a `&mut Self` with the same lifetime.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerocopy::FromBytes;
    /// # use zerocopy_derive::*;
    ///
    /// #[derive(AsBytes, FromZeroes, FromBytes)]
    /// #[repr(C)]
    /// struct PacketTrailer {
    ///     frame_check_sequence: [u8; 4],
    /// }
    ///
    /// // These are more bytes than are needed to encode a `PacketTrailer`.
    /// let bytes = &mut [0, 1, 2, 3, 4, 5, 6, 7, 8, 9][..];
    ///
    /// let trailer = PacketTrailer::mut_from_suffix(bytes).unwrap();
    ///
    /// assert_eq!(trailer.frame_check_sequence, [6, 7, 8, 9]);
    ///
    /// trailer.frame_check_sequence = [0, 0, 0, 0];
    ///
    /// assert_eq!(bytes, [0, 1, 2, 3, 4, 5, 0, 0, 0, 0]);
    /// ```
    #[inline]
    fn mut_from_suffix(bytes: &mut [u8]) -> Option<&mut Self>
    where
        Self: Sized + AsBytes,
    {
        Ref::<&mut [u8], Self>::new_from_suffix(bytes).map(|(_, r)| r.into_mut())
    }

    /// Interprets the given `bytes` as a `&[Self]` without copying.
    ///
    /// If `bytes.len() % size_of::<Self>() != 0` or `bytes` is not aligned to
    /// `align_of::<Self>()`, this returns `None`.
    ///
    /// If you need to convert a specific number of slice elements, see
    /// [`slice_from_prefix`](FromBytes::slice_from_prefix) or
    /// [`slice_from_suffix`](FromBytes::slice_from_suffix).
    ///
    /// # Panics
    ///
    /// If `Self` is a zero-sized type.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerocopy::FromBytes;
    /// # use zerocopy_derive::*;
    ///
    /// # #[derive(Debug, PartialEq, Eq)]
    /// #[derive(FromZeroes, FromBytes)]
    /// #[repr(C)]
    /// struct Pixel {
    ///     r: u8,
    ///     g: u8,
    ///     b: u8,
    ///     a: u8,
    /// }
    ///
    /// // These bytes encode two `Pixel`s.
    /// let bytes = [0, 1, 2, 3, 4, 5, 6, 7].as_slice();
    ///
    /// let pixels = Pixel::slice_from(bytes).unwrap();
    ///
    /// assert_eq!(pixels, &[
    ///     Pixel { r: 0, g: 1, b: 2, a: 3 },
    ///     Pixel { r: 4, g: 5, b: 6, a: 7 },
    /// ]);
    /// ```
    #[inline]
    fn slice_from(bytes: &[u8]) -> Option<&[Self]>
    where
        Self: Sized,
    {
        Ref::<_, [Self]>::new_slice(bytes).map(|r| r.into_slice())
    }

    /// Interprets the prefix of the given `bytes` as a `&[Self]` with length
    /// equal to `count` without copying.
    ///
    /// This method verifies that `bytes.len() >= size_of::<T>() * count`
    /// and that `bytes` is aligned to `align_of::<T>()`. It consumes the
    /// first `size_of::<T>() * count` bytes from `bytes` to construct a
    /// `&[Self]`, and returns the remaining bytes to the caller. It also
    /// ensures that `sizeof::<T>() * count` does not overflow a `usize`.
    /// If any of the length, alignment, or overflow checks fail, it returns
    /// `None`.
    ///
    /// # Panics
    ///
    /// If `T` is a zero-sized type.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerocopy::FromBytes;
    /// # use zerocopy_derive::*;
    ///
    /// # #[derive(Debug, PartialEq, Eq)]
    /// #[derive(FromZeroes, FromBytes)]
    /// #[repr(C)]
    /// struct Pixel {
    ///     r: u8,
    ///     g: u8,
    ///     b: u8,
    ///     a: u8,
    /// }
    ///
    /// // These are more bytes than are needed to encode two `Pixel`s.
    /// let bytes = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9].as_slice();
    ///
    /// let (pixels, rest) = Pixel::slice_from_prefix(bytes, 2).unwrap();
    ///
    /// assert_eq!(pixels, &[
    ///     Pixel { r: 0, g: 1, b: 2, a: 3 },
    ///     Pixel { r: 4, g: 5, b: 6, a: 7 },
    /// ]);
    ///
    /// assert_eq!(rest, &[8, 9]);
    /// ```
    #[inline]
    fn slice_from_prefix(bytes: &[u8], count: usize) -> Option<(&[Self], &[u8])>
    where
        Self: Sized,
    {
        Ref::<_, [Self]>::new_slice_from_prefix(bytes, count).map(|(r, b)| (r.into_slice(), b))
    }

    /// Interprets the suffix of the given `bytes` as a `&[Self]` with length
    /// equal to `count` without copying.
    ///
    /// This method verifies that `bytes.len() >= size_of::<T>() * count`
    /// and that `bytes` is aligned to `align_of::<T>()`. It consumes the
    /// last `size_of::<T>() * count` bytes from `bytes` to construct a
    /// `&[Self]`, and returns the preceding bytes to the caller. It also
    /// ensures that `sizeof::<T>() * count` does not overflow a `usize`.
    /// If any of the length, alignment, or overflow checks fail, it returns
    /// `None`.
    ///
    /// # Panics
    ///
    /// If `T` is a zero-sized type.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerocopy::FromBytes;
    /// # use zerocopy_derive::*;
    ///
    /// # #[derive(Debug, PartialEq, Eq)]
    /// #[derive(FromZeroes, FromBytes)]
    /// #[repr(C)]
    /// struct Pixel {
    ///     r: u8,
    ///     g: u8,
    ///     b: u8,
    ///     a: u8,
    /// }
    ///
    /// // These are more bytes than are needed to encode two `Pixel`s.
    /// let bytes = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9].as_slice();
    ///
    /// let (rest, pixels) = Pixel::slice_from_suffix(bytes, 2).unwrap();
    ///
    /// assert_eq!(rest, &[0, 1]);
    ///
    /// assert_eq!(pixels, &[
    ///     Pixel { r: 2, g: 3, b: 4, a: 5 },
    ///     Pixel { r: 6, g: 7, b: 8, a: 9 },
    /// ]);
    /// ```
    #[inline]
    fn slice_from_suffix(bytes: &[u8], count: usize) -> Option<(&[u8], &[Self])>
    where
        Self: Sized,
    {
        Ref::<_, [Self]>::new_slice_from_suffix(bytes, count).map(|(b, r)| (b, r.into_slice()))
    }

    /// Interprets the given `bytes` as a `&mut [Self]` without copying.
    ///
    /// If `bytes.len() % size_of::<T>() != 0` or `bytes` is not aligned to
    /// `align_of::<T>()`, this returns `None`.
    ///
    /// If you need to convert a specific number of slice elements, see
    /// [`mut_slice_from_prefix`](FromBytes::mut_slice_from_prefix) or
    /// [`mut_slice_from_suffix`](FromBytes::mut_slice_from_suffix).
    ///
    /// # Panics
    ///
    /// If `T` is a zero-sized type.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerocopy::FromBytes;
    /// # use zerocopy_derive::*;
    ///
    /// # #[derive(Debug, PartialEq, Eq)]
    /// #[derive(AsBytes, FromZeroes, FromBytes)]
    /// #[repr(C)]
    /// struct Pixel {
    ///     r: u8,
    ///     g: u8,
    ///     b: u8,
    ///     a: u8,
    /// }
    ///
    /// // These bytes encode two `Pixel`s.
    /// let bytes = &mut [0, 1, 2, 3, 4, 5, 6, 7][..];
    ///
    /// let pixels = Pixel::mut_slice_from(bytes).unwrap();
    ///
    /// assert_eq!(pixels, &[
    ///     Pixel { r: 0, g: 1, b: 2, a: 3 },
    ///     Pixel { r: 4, g: 5, b: 6, a: 7 },
    /// ]);
    ///
    /// pixels[1] = Pixel { r: 0, g: 0, b: 0, a: 0 };
    ///
    /// assert_eq!(bytes, [0, 1, 2, 3, 0, 0, 0, 0]);
    /// ```
    #[inline]
    fn mut_slice_from(bytes: &mut [u8]) -> Option<&mut [Self]>
    where
        Self: Sized + AsBytes,
    {
        Ref::<_, [Self]>::new_slice(bytes).map(|r| r.into_mut_slice())
    }

    /// Interprets the prefix of the given `bytes` as a `&mut [Self]` with length
    /// equal to `count` without copying.
    ///
    /// This method verifies that `bytes.len() >= size_of::<T>() * count`
    /// and that `bytes` is aligned to `align_of::<T>()`. It consumes the
    /// first `size_of::<T>() * count` bytes from `bytes` to construct a
    /// `&[Self]`, and returns the remaining bytes to the caller. It also
    /// ensures that `sizeof::<T>() * count` does not overflow a `usize`.
    /// If any of the length, alignment, or overflow checks fail, it returns
    /// `None`.
    ///
    /// # Panics
    ///
    /// If `T` is a zero-sized type.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerocopy::FromBytes;
    /// # use zerocopy_derive::*;
    ///
    /// # #[derive(Debug, PartialEq, Eq)]
    /// #[derive(AsBytes, FromZeroes, FromBytes)]
    /// #[repr(C)]
    /// struct Pixel {
    ///     r: u8,
    ///     g: u8,
    ///     b: u8,
    ///     a: u8,
    /// }
    ///
    /// // These are more bytes than are needed to encode two `Pixel`s.
    /// let bytes = &mut [0, 1, 2, 3, 4, 5, 6, 7, 8, 9][..];
    ///
    /// let (pixels, rest) = Pixel::mut_slice_from_prefix(bytes, 2).unwrap();
    ///
    /// assert_eq!(pixels, &[
    ///     Pixel { r: 0, g: 1, b: 2, a: 3 },
    ///     Pixel { r: 4, g: 5, b: 6, a: 7 },
    /// ]);
    ///
    /// assert_eq!(rest, &[8, 9]);
    ///
    /// pixels[1] = Pixel { r: 0, g: 0, b: 0, a: 0 };
    ///
    /// assert_eq!(bytes, [0, 1, 2, 3, 0, 0, 0, 0, 8, 9]);
    /// ```
    #[inline]
    fn mut_slice_from_prefix(bytes: &mut [u8], count: usize) -> Option<(&mut [Self], &mut [u8])>
    where
        Self: Sized + AsBytes,
    {
        Ref::<_, [Self]>::new_slice_from_prefix(bytes, count).map(|(r, b)| (r.into_mut_slice(), b))
    }

    /// Interprets the suffix of the given `bytes` as a `&mut [Self]` with length
    /// equal to `count` without copying.
    ///
    /// This method verifies that `bytes.len() >= size_of::<T>() * count`
    /// and that `bytes` is aligned to `align_of::<T>()`. It consumes the
    /// last `size_of::<T>() * count` bytes from `bytes` to construct a
    /// `&[Self]`, and returns the preceding bytes to the caller. It also
    /// ensures that `sizeof::<T>() * count` does not overflow a `usize`.
    /// If any of the length, alignment, or overflow checks fail, it returns
    /// `None`.
    ///
    /// # Panics
    ///
    /// If `T` is a zero-sized type.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerocopy::FromBytes;
    /// # use zerocopy_derive::*;
    ///
    /// # #[derive(Debug, PartialEq, Eq)]
    /// #[derive(AsBytes, FromZeroes, FromBytes)]
    /// #[repr(C)]
    /// struct Pixel {
    ///     r: u8,
    ///     g: u8,
    ///     b: u8,
    ///     a: u8,
    /// }
    ///
    /// // These are more bytes than are needed to encode two `Pixel`s.
    /// let bytes = &mut [0, 1, 2, 3, 4, 5, 6, 7, 8, 9][..];
    ///
    /// let (rest, pixels) = Pixel::mut_slice_from_suffix(bytes, 2).unwrap();
    ///
    /// assert_eq!(rest, &[0, 1]);
    ///
    /// assert_eq!(pixels, &[
    ///     Pixel { r: 2, g: 3, b: 4, a: 5 },
    ///     Pixel { r: 6, g: 7, b: 8, a: 9 },
    /// ]);
    ///
    /// pixels[1] = Pixel { r: 0, g: 0, b: 0, a: 0 };
    ///
    /// assert_eq!(bytes, [0, 1, 2, 3, 4, 5, 0, 0, 0, 0]);
    /// ```
    #[inline]
    fn mut_slice_from_suffix(bytes: &mut [u8], count: usize) -> Option<(&mut [u8], &mut [Self])>
    where
        Self: Sized + AsBytes,
    {
        Ref::<_, [Self]>::new_slice_from_suffix(bytes, count).map(|(b, r)| (b, r.into_mut_slice()))
    }

    /// Reads a copy of `Self` from `bytes`.
    ///
    /// If `bytes.len() != size_of::<Self>()`, `read_from` returns `None`.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerocopy::FromBytes;
    /// # use zerocopy_derive::*;
    ///
    /// #[derive(FromZeroes, FromBytes)]
    /// #[repr(C)]
    /// struct PacketHeader {
    ///     src_port: [u8; 2],
    ///     dst_port: [u8; 2],
    ///     length: [u8; 2],
    ///     checksum: [u8; 2],
    /// }
    ///
    /// // These bytes encode a `PacketHeader`.
    /// let bytes = [0, 1, 2, 3, 4, 5, 6, 7].as_slice();
    ///
    /// let header = PacketHeader::read_from(bytes).unwrap();
    ///
    /// assert_eq!(header.src_port, [0, 1]);
    /// assert_eq!(header.dst_port, [2, 3]);
    /// assert_eq!(header.length, [4, 5]);
    /// assert_eq!(header.checksum, [6, 7]);
    /// ```
    #[inline]
    fn read_from(bytes: &[u8]) -> Option<Self>
    where
        Self: Sized,
    {
        Ref::<_, Unalign<Self>>::new_unaligned(bytes).map(|r| r.read().into_inner())
    }

    /// Reads a copy of `Self` from the prefix of `bytes`.
    ///
    /// `read_from_prefix` reads a `Self` from the first `size_of::<Self>()`
    /// bytes of `bytes`. If `bytes.len() < size_of::<Self>()`, it returns
    /// `None`.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerocopy::FromBytes;
    /// # use zerocopy_derive::*;
    ///
    /// #[derive(FromZeroes, FromBytes)]
    /// #[repr(C)]
    /// struct PacketHeader {
    ///     src_port: [u8; 2],
    ///     dst_port: [u8; 2],
    ///     length: [u8; 2],
    ///     checksum: [u8; 2],
    /// }
    ///
    /// // These are more bytes than are needed to encode a `PacketHeader`.
    /// let bytes = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9].as_slice();
    ///
    /// let header = PacketHeader::read_from_prefix(bytes).unwrap();
    ///
    /// assert_eq!(header.src_port, [0, 1]);
    /// assert_eq!(header.dst_port, [2, 3]);
    /// assert_eq!(header.length, [4, 5]);
    /// assert_eq!(header.checksum, [6, 7]);
    /// ```
    #[inline]
    fn read_from_prefix(bytes: &[u8]) -> Option<Self>
    where
        Self: Sized,
    {
        Ref::<_, Unalign<Self>>::new_unaligned_from_prefix(bytes)
            .map(|(r, _)| r.read().into_inner())
    }

    /// Reads a copy of `Self` from the suffix of `bytes`.
    ///
    /// `read_from_suffix` reads a `Self` from the last `size_of::<Self>()`
    /// bytes of `bytes`. If `bytes.len() < size_of::<Self>()`, it returns
    /// `None`.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerocopy::FromBytes;
    /// # use zerocopy_derive::*;
    ///
    /// #[derive(FromZeroes, FromBytes)]
    /// #[repr(C)]
    /// struct PacketTrailer {
    ///     frame_check_sequence: [u8; 4],
    /// }
    ///
    /// // These are more bytes than are needed to encode a `PacketTrailer`.
    /// let bytes = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9].as_slice();
    ///
    /// let trailer = PacketTrailer::read_from_suffix(bytes).unwrap();
    ///
    /// assert_eq!(trailer.frame_check_sequence, [6, 7, 8, 9]);
    /// ```
    #[inline]
    fn read_from_suffix(bytes: &[u8]) -> Option<Self>
    where
        Self: Sized,
    {
        Ref::<_, Unalign<Self>>::new_unaligned_from_suffix(bytes)
            .map(|(_, r)| r.read().into_inner())
    }
}

/// Analyzes whether a type is [`AsBytes`].
///
/// This derive analyzes, at compile time, whether the annotated type satisfies
/// the [safety conditions] of `AsBytes` and implements `AsBytes` if it is
/// sound to do so. This derive can be applied to structs, enums, and unions;
/// e.g.:
///
/// ```
/// # use zerocopy_derive::{AsBytes};
/// #[derive(AsBytes)]
/// #[repr(C)]
/// struct MyStruct {
/// # /*
///     ...
/// # */
/// }
///
/// #[derive(AsBytes)]
/// #[repr(u8)]
/// enum MyEnum {
/// #   Variant,
/// # /*
///     ...
/// # */
/// }
///
/// #[derive(AsBytes)]
/// #[repr(C)]
/// union MyUnion {
/// #   variant: u8,
/// # /*
///     ...
/// # */
/// }
/// ```
///
/// [safety conditions]: trait@AsBytes#safety
///
/// # Error Messages
///
/// Due to the way that the custom derive for `AsBytes` is implemented, you may
/// get an error like this:
///
/// ```text
/// error[E0277]: the trait bound `HasPadding<Foo, true>: ShouldBe<false>` is not satisfied
///   --> lib.rs:23:10
///    |
///  1 | #[derive(AsBytes)]
///    |          ^^^^^^^ the trait `ShouldBe<false>` is not implemented for `HasPadding<Foo, true>`
///    |
///    = help: the trait `ShouldBe<VALUE>` is implemented for `HasPadding<T, VALUE>`
/// ```
///
/// This error indicates that the type being annotated has padding bytes, which
/// is illegal for `AsBytes` types. Consider reducing the alignment of some
/// fields by using types in the [`byteorder`] module, adding explicit struct
/// fields where those padding bytes would be, or using `#[repr(packed)]`. See
/// the Rust Reference's page on [type layout] for more information
/// about type layout and padding.
///
/// [type layout]: https://doc.rust-lang.org/reference/type-layout.html
///
/// # Analysis
///
/// *This section describes, roughly, the analysis performed by this derive to
/// determine whether it is sound to implement `AsBytes` for a given type.
/// Unless you are modifying the implementation of this derive, or attempting to
/// manually implement `AsBytes` for a type yourself, you don't need to read
/// this section.*
///
/// If a type has the following properties, then this derive can implement
/// `AsBytes` for that type:
///
/// - If the type is a struct:
///   - It must have a defined representation (`repr(C)`, `repr(transparent)`,
///     or `repr(packed)`).
///   - All of its fields must be `AsBytes`.
///   - Its layout must have no padding. This is always true for
///     `repr(transparent)` and `repr(packed)`. For `repr(C)`, see the layout
///     algorithm described in the [Rust Reference].
/// - If the type is an enum:
///   - It must be a C-like enum (meaning that all variants have no fields).
///   - It must have a defined representation (`repr`s `C`, `u8`, `u16`, `u32`,
///     `u64`, `usize`, `i8`, `i16`, `i32`, `i64`, or `isize`).
/// - The type must not contain any [`UnsafeCell`]s (this is required in order
///   for it to be sound to construct a `&[u8]` and a `&T` to the same region of
///   memory). The type may contain references or pointers to `UnsafeCell`s so
///   long as those values can themselves be initialized from zeroes (`AsBytes`
///   is not currently implemented for, e.g., `Option<&UnsafeCell<_>>`, but it
///   could be one day).
///
/// [`UnsafeCell`]: core::cell::UnsafeCell
///
/// This analysis is subject to change. Unsafe code may *only* rely on the
/// documented [safety conditions] of `FromBytes`, and must *not* rely on the
/// implementation details of this derive.
///
/// [Rust Reference]: https://doc.rust-lang.org/reference/type-layout.html
#[cfg(any(feature = "derive", test))]
#[cfg_attr(doc_cfg, doc(cfg(feature = "derive")))]
pub use zerocopy_derive::AsBytes;

/// Types that can be viewed as an immutable slice of initialized bytes.
///
/// Any `AsBytes` type can be viewed as a slice of initialized bytes of the same
/// size. This is useful for efficiently serializing structured data as raw
/// bytes.
///
/// # Implementation
///
/// **Do not implement this trait yourself!** Instead, use
/// [`#[derive(AsBytes)]`][derive] (requires the `derive` Cargo feature); e.g.:
///
/// ```
/// # use zerocopy_derive::AsBytes;
/// #[derive(AsBytes)]
/// #[repr(C)]
/// struct MyStruct {
/// # /*
///     ...
/// # */
/// }
///
/// #[derive(AsBytes)]
/// #[repr(u8)]
/// enum MyEnum {
/// #   Variant0,
/// # /*
///     ...
/// # */
/// }
///
/// #[derive(AsBytes)]
/// #[repr(C)]
/// union MyUnion {
/// #   variant: u8,
/// # /*
///     ...
/// # */
/// }
/// ```
///
/// This derive performs a sophisticated, compile-time safety analysis to
/// determine whether a type is `AsBytes`. See the [derive
/// documentation][derive] for guidance on how to interpret error messages
/// produced by the derive's analysis.
///
/// # Safety
///
/// *This section describes what is required in order for `T: AsBytes`, and
/// what unsafe code may assume of such types. If you don't plan on implementing
/// `AsBytes` manually, and you don't plan on writing unsafe code that
/// operates on `AsBytes` types, then you don't need to read this section.*
///
/// If `T: AsBytes`, then unsafe code may assume that:
/// - It is sound to treat any `t: T` as an immutable `[u8]` of length
///   `size_of_val(t)`.
/// - Given `t: &T`, it is sound to construct a `b: &[u8]` where `b.len() ==
///   size_of_val(t)` at the same address as `t`, and it is sound for both `b`
///   and `t` to be live at the same time.
///
/// If a type is marked as `AsBytes` which violates this contract, it may cause
/// undefined behavior.
///
/// `#[derive(AsBytes)]` only permits [types which satisfy these
/// requirements][derive-analysis].
///
#[cfg_attr(
    feature = "derive",
    doc = "[derive]: zerocopy_derive::AsBytes",
    doc = "[derive-analysis]: zerocopy_derive::AsBytes#analysis"
)]
#[cfg_attr(
    not(feature = "derive"),
    doc = concat!("[derive]: https://docs.rs/zerocopy/", env!("CARGO_PKG_VERSION"), "/zerocopy/derive.AsBytes.html"),
    doc = concat!("[derive-analysis]: https://docs.rs/zerocopy/", env!("CARGO_PKG_VERSION"), "/zerocopy/derive.AsBytes.html#analysis"),
)]
pub unsafe trait AsBytes {
    // The `Self: Sized` bound makes it so that this function doesn't prevent
    // `AsBytes` from being object safe. Note that other `AsBytes` methods
    // prevent object safety, but those provide a benefit in exchange for object
    // safety. If at some point we remove those methods, change their type
    // signatures, or move them out of this trait so that `AsBytes` is object
    // safe again, it's important that this function not prevent object safety.
    #[doc(hidden)]
    fn only_derive_is_allowed_to_implement_this_trait()
    where
        Self: Sized;

    /// Gets the bytes of this value.
    ///
    /// `as_bytes` provides access to the bytes of this value as an immutable
    /// byte slice.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerocopy::AsBytes;
    /// # use zerocopy_derive::*;
    ///
    /// #[derive(AsBytes)]
    /// #[repr(C)]
    /// struct PacketHeader {
    ///     src_port: [u8; 2],
    ///     dst_port: [u8; 2],
    ///     length: [u8; 2],
    ///     checksum: [u8; 2],
    /// }
    ///
    /// let header = PacketHeader {
    ///     src_port: [0, 1],
    ///     dst_port: [2, 3],
    ///     length: [4, 5],
    ///     checksum: [6, 7],
    /// };
    ///
    /// let bytes = header.as_bytes();
    ///
    /// assert_eq!(bytes, [0, 1, 2, 3, 4, 5, 6, 7]);
    /// ```
    #[inline(always)]
    fn as_bytes(&self) -> &[u8] {
        // Note that this method does not have a `Self: Sized` bound;
        // `size_of_val` works for unsized values too.
        let len = mem::size_of_val(self);
        let slf: *const Self = self;

        // SAFETY:
        // - `slf.cast::<u8>()` is valid for reads for `len *
        //   mem::size_of::<u8>()` many bytes because...
        //   - `slf` is the same pointer as `self`, and `self` is a reference
        //     which points to an object whose size is `len`. Thus...
        //     - The entire region of `len` bytes starting at `slf` is contained
        //       within a single allocation.
        //     - `slf` is non-null.
        //   - `slf` is trivially aligned to `align_of::<u8>() == 1`.
        // - `Self: AsBytes` ensures that all of the bytes of `slf` are
        //   initialized.
        // - Since `slf` is derived from `self`, and `self` is an immutable
        //   reference, the only other references to this memory region that
        //   could exist are other immutable references, and those don't allow
        //   mutation. `AsBytes` prohibits types which contain `UnsafeCell`s,
        //   which are the only types for which this rule wouldn't be sufficient.
        // - The total size of the resulting slice is no larger than
        //   `isize::MAX` because no allocation produced by safe code can be
        //   larger than `isize::MAX`.
        //
        // TODO(#429): Add references to docs and quotes.
        unsafe { slice::from_raw_parts(slf.cast::<u8>(), len) }
    }

    /// Gets the bytes of this value mutably.
    ///
    /// `as_bytes_mut` provides access to the bytes of this value as a mutable
    /// byte slice.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerocopy::AsBytes;
    /// # use zerocopy_derive::*;
    ///
    /// # #[derive(Eq, PartialEq, Debug)]
    /// #[derive(AsBytes, FromZeroes, FromBytes)]
    /// #[repr(C)]
    /// struct PacketHeader {
    ///     src_port: [u8; 2],
    ///     dst_port: [u8; 2],
    ///     length: [u8; 2],
    ///     checksum: [u8; 2],
    /// }
    ///
    /// let mut header = PacketHeader {
    ///     src_port: [0, 1],
    ///     dst_port: [2, 3],
    ///     length: [4, 5],
    ///     checksum: [6, 7],
    /// };
    ///
    /// let bytes = header.as_bytes_mut();
    ///
    /// assert_eq!(bytes, [0, 1, 2, 3, 4, 5, 6, 7]);
    ///
    /// bytes.reverse();
    ///
    /// assert_eq!(header, PacketHeader {
    ///     src_port: [7, 6],
    ///     dst_port: [5, 4],
    ///     length: [3, 2],
    ///     checksum: [1, 0],
    /// });
    /// ```
    #[inline(always)]
    fn as_bytes_mut(&mut self) -> &mut [u8]
    where
        Self: FromBytes,
    {
        // Note that this method does not have a `Self: Sized` bound;
        // `size_of_val` works for unsized values too.
        let len = mem::size_of_val(self);
        let slf: *mut Self = self;

        // SAFETY:
        // - `slf.cast::<u8>()` is valid for reads and writes for `len *
        //   mem::size_of::<u8>()` many bytes because...
        //   - `slf` is the same pointer as `self`, and `self` is a reference
        //     which points to an object whose size is `len`. Thus...
        //     - The entire region of `len` bytes starting at `slf` is contained
        //       within a single allocation.
        //     - `slf` is non-null.
        //   - `slf` is trivially aligned to `align_of::<u8>() == 1`.
        // - `Self: AsBytes` ensures that all of the bytes of `slf` are
        //   initialized.
        // - `Self: FromBytes` ensures that no write to this memory region
        //   could result in it containing an invalid `Self`.
        // - Since `slf` is derived from `self`, and `self` is a mutable
        //   reference, no other references to this memory region can exist.
        // - The total size of the resulting slice is no larger than
        //   `isize::MAX` because no allocation produced by safe code can be
        //   larger than `isize::MAX`.
        //
        // TODO(#429): Add references to docs and quotes.
        unsafe { slice::from_raw_parts_mut(slf.cast::<u8>(), len) }
    }

    /// Writes a copy of `self` to `bytes`.
    ///
    /// If `bytes.len() != size_of_val(self)`, `write_to` returns `None`.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerocopy::AsBytes;
    /// # use zerocopy_derive::*;
    ///
    /// #[derive(AsBytes)]
    /// #[repr(C)]
    /// struct PacketHeader {
    ///     src_port: [u8; 2],
    ///     dst_port: [u8; 2],
    ///     length: [u8; 2],
    ///     checksum: [u8; 2],
    /// }
    ///
    /// let header = PacketHeader {
    ///     src_port: [0, 1],
    ///     dst_port: [2, 3],
    ///     length: [4, 5],
    ///     checksum: [6, 7],
    /// };
    ///
    /// let mut bytes = [0, 0, 0, 0, 0, 0, 0, 0];
    ///
    /// header.write_to(&mut bytes[..]);
    ///
    /// assert_eq!(bytes, [0, 1, 2, 3, 4, 5, 6, 7]);
    /// ```
    ///
    /// If too many or too few target bytes are provided, `write_to` returns
    /// `None` and leaves the target bytes unmodified:
    ///
    /// ```
    /// # use zerocopy::AsBytes;
    /// # let header = u128::MAX;
    /// let mut excessive_bytes = &mut [0u8; 128][..];
    ///
    /// let write_result = header.write_to(excessive_bytes);
    ///
    /// assert!(write_result.is_none());
    /// assert_eq!(excessive_bytes, [0u8; 128]);
    /// ```
    #[inline]
    fn write_to(&self, bytes: &mut [u8]) -> Option<()> {
        if bytes.len() != mem::size_of_val(self) {
            return None;
        }

        bytes.copy_from_slice(self.as_bytes());
        Some(())
    }

    /// Writes a copy of `self` to the prefix of `bytes`.
    ///
    /// `write_to_prefix` writes `self` to the first `size_of_val(self)` bytes
    /// of `bytes`. If `bytes.len() < size_of_val(self)`, it returns `None`.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerocopy::AsBytes;
    /// # use zerocopy_derive::*;
    ///
    /// #[derive(AsBytes)]
    /// #[repr(C)]
    /// struct PacketHeader {
    ///     src_port: [u8; 2],
    ///     dst_port: [u8; 2],
    ///     length: [u8; 2],
    ///     checksum: [u8; 2],
    /// }
    ///
    /// let header = PacketHeader {
    ///     src_port: [0, 1],
    ///     dst_port: [2, 3],
    ///     length: [4, 5],
    ///     checksum: [6, 7],
    /// };
    ///
    /// let mut bytes = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    ///
    /// header.write_to_prefix(&mut bytes[..]);
    ///
    /// assert_eq!(bytes, [0, 1, 2, 3, 4, 5, 6, 7, 0, 0]);
    /// ```
    ///
    /// If insufficient target bytes are provided, `write_to_prefix` returns
    /// `None` and leaves the target bytes unmodified:
    ///
    /// ```
    /// # use zerocopy::AsBytes;
    /// # let header = u128::MAX;
    /// let mut insufficent_bytes = &mut [0, 0][..];
    ///
    /// let write_result = header.write_to_suffix(insufficent_bytes);
    ///
    /// assert!(write_result.is_none());
    /// assert_eq!(insufficent_bytes, [0, 0]);
    /// ```
    #[inline]
    fn write_to_prefix(&self, bytes: &mut [u8]) -> Option<()> {
        let size = mem::size_of_val(self);
        bytes.get_mut(..size)?.copy_from_slice(self.as_bytes());
        Some(())
    }

    /// Writes a copy of `self` to the suffix of `bytes`.
    ///
    /// `write_to_suffix` writes `self` to the last `size_of_val(self)` bytes of
    /// `bytes`. If `bytes.len() < size_of_val(self)`, it returns `None`.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerocopy::AsBytes;
    /// # use zerocopy_derive::*;
    ///
    /// #[derive(AsBytes)]
    /// #[repr(C)]
    /// struct PacketHeader {
    ///     src_port: [u8; 2],
    ///     dst_port: [u8; 2],
    ///     length: [u8; 2],
    ///     checksum: [u8; 2],
    /// }
    ///
    /// let header = PacketHeader {
    ///     src_port: [0, 1],
    ///     dst_port: [2, 3],
    ///     length: [4, 5],
    ///     checksum: [6, 7],
    /// };
    ///
    /// let mut bytes = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    ///
    /// header.write_to_suffix(&mut bytes[..]);
    ///
    /// assert_eq!(bytes, [0, 0, 0, 1, 2, 3, 4, 5, 6, 7]);
    ///
    /// let mut insufficent_bytes = &mut [0, 0][..];
    ///
    /// let write_result = header.write_to_suffix(insufficent_bytes);
    ///
    /// assert!(write_result.is_none());
    /// assert_eq!(insufficent_bytes, [0, 0]);
    /// ```
    ///
    /// If insufficient target bytes are provided, `write_to_suffix` returns
    /// `None` and leaves the target bytes unmodified:
    ///
    /// ```
    /// # use zerocopy::AsBytes;
    /// # let header = u128::MAX;
    /// let mut insufficent_bytes = &mut [0, 0][..];
    ///
    /// let write_result = header.write_to_suffix(insufficent_bytes);
    ///
    /// assert!(write_result.is_none());
    /// assert_eq!(insufficent_bytes, [0, 0]);
    /// ```
    #[inline]
    fn write_to_suffix(&self, bytes: &mut [u8]) -> Option<()> {
        let start = bytes.len().checked_sub(mem::size_of_val(self))?;
        bytes
            .get_mut(start..)
            .expect("`start` should be in-bounds of `bytes`")
            .copy_from_slice(self.as_bytes());
        Some(())
    }
}

/// Types with no alignment requirement.
///
/// WARNING: Do not implement this trait yourself! Instead, use
/// `#[derive(Unaligned)]` (requires the `derive` Cargo feature).
///
/// If `T: Unaligned`, then `align_of::<T>() == 1`.
///
/// # Safety
///
/// *This section describes what is required in order for `T: Unaligned`, and
/// what unsafe code may assume of such types. `#[derive(Unaligned)]` only
/// permits types which satisfy these requirements. If you don't plan on
/// implementing `Unaligned` manually, and you don't plan on writing unsafe code
/// that operates on `Unaligned` types, then you don't need to read this
/// section.*
///
/// If `T: Unaligned`, then unsafe code may assume that it is sound to produce a
/// reference to `T` at any memory location regardless of alignment. If a type
/// is marked as `Unaligned` which violates this contract, it may cause
/// undefined behavior.
pub unsafe trait Unaligned {
    // The `Self: Sized` bound makes it so that `Unaligned` is still object
    // safe.
    #[doc(hidden)]
    fn only_derive_is_allowed_to_implement_this_trait()
    where
        Self: Sized;
}

safety_comment! {
    /// SAFETY:
    /// Per the reference [1], "the unit tuple (`()`) ... is guaranteed as a
    /// zero-sized type to have a size of 0 and an alignment of 1."
    /// - `TryFromBytes` (with no validator), `FromZeroes`, `FromBytes`: There
    ///   is only one possible sequence of 0 bytes, and `()` is inhabited.
    /// - `AsBytes`: Since `()` has size 0, it contains no padding bytes.
    /// - `Unaligned`: `()` has alignment 1.
    ///
    /// [1] https://doc.rust-lang.org/reference/type-layout.html#tuple-layout
    unsafe_impl!((): TryFromBytes, FromZeroes, FromBytes, AsBytes, Unaligned);
    assert_unaligned!(());
}

safety_comment! {
    /// SAFETY:
    /// - `TryFromBytes` (with no validator), `FromZeroes`, `FromBytes`: all bit
    ///   patterns are valid for numeric types [1]
    /// - `AsBytes`: numeric types have no padding bytes [1]
    /// - `Unaligned` (`u8` and `i8` only): The reference [2] specifies the size
    ///   of `u8` and `i8` as 1 byte. We also know that:
    ///   - Alignment is >= 1 [3]
    ///   - Size is an integer multiple of alignment [4]
    ///   - The only value >= 1 for which 1 is an integer multiple is 1
    ///   Therefore, the only possible alignment for `u8` and `i8` is 1.
    ///
    /// [1] Per https://doc.rust-lang.org/beta/reference/types/numeric.html#bit-validity:
    ///
    ///     For every numeric type, `T`, the bit validity of `T` is equivalent to
    ///     the bit validity of `[u8; size_of::<T>()]`. An uninitialized byte is
    ///     not a valid `u8`.
    ///
    /// TODO(https://github.com/rust-lang/reference/pull/1392): Once this text
    /// is available on the Stable docs, cite those instead.
    ///
    /// [2] https://doc.rust-lang.org/reference/type-layout.html#primitive-data-layout
    ///
    /// [3] Per https://doc.rust-lang.org/reference/type-layout.html#size-and-alignment:
    ///
    ///     Alignment is measured in bytes, and must be at least 1.
    ///
    /// [4] Per https://doc.rust-lang.org/reference/type-layout.html#size-and-alignment:
    ///
    ///     The size of a value is always a multiple of its alignment.
    ///
    /// TODO(#278): Once we've updated the trait docs to refer to `u8`s rather
    /// than bits or bytes, update this comment, especially the reference to
    /// [1].
    unsafe_impl!(u8: TryFromBytes, FromZeroes, FromBytes, AsBytes, Unaligned);
    unsafe_impl!(i8: TryFromBytes, FromZeroes, FromBytes, AsBytes, Unaligned);
    assert_unaligned!(u8, i8);
    unsafe_impl!(u16: TryFromBytes, FromZeroes, FromBytes, AsBytes);
    unsafe_impl!(i16: TryFromBytes, FromZeroes, FromBytes, AsBytes);
    unsafe_impl!(u32: TryFromBytes, FromZeroes, FromBytes, AsBytes);
    unsafe_impl!(i32: TryFromBytes, FromZeroes, FromBytes, AsBytes);
    unsafe_impl!(u64: TryFromBytes, FromZeroes, FromBytes, AsBytes);
    unsafe_impl!(i64: TryFromBytes, FromZeroes, FromBytes, AsBytes);
    unsafe_impl!(u128: TryFromBytes, FromZeroes, FromBytes, AsBytes);
    unsafe_impl!(i128: TryFromBytes, FromZeroes, FromBytes, AsBytes);
    unsafe_impl!(usize: TryFromBytes, FromZeroes, FromBytes, AsBytes);
    unsafe_impl!(isize: TryFromBytes, FromZeroes, FromBytes, AsBytes);
    unsafe_impl!(f32: TryFromBytes, FromZeroes, FromBytes, AsBytes);
    unsafe_impl!(f64: TryFromBytes, FromZeroes, FromBytes, AsBytes);
}

safety_comment! {
    /// SAFETY:
    /// - `FromZeroes`: Valid since "[t]he value false has the bit pattern
    ///   0x00" [1].
    /// - `AsBytes`: Since "the boolean type has a size and alignment of 1 each"
    ///   and "The value false has the bit pattern 0x00 and the value true has
    ///   the bit pattern 0x01" [1]. Thus, the only byte of the bool is always
    ///   initialized.
    /// - `Unaligned`: Per the reference [1], "[a]n object with the boolean type
    ///   has a size and alignment of 1 each."
    ///
    /// [1] https://doc.rust-lang.org/reference/types/boolean.html
    unsafe_impl!(bool: FromZeroes, AsBytes, Unaligned);
    assert_unaligned!(bool);
    /// SAFETY:
    /// - The safety requirements for `unsafe_impl!` with an `is_bit_valid`
    ///   closure:
    ///   - Given `t: *mut bool` and `let r = *mut u8`, `r` refers to an object
    ///     of the same size as that referred to by `t`. This is true because
    ///     `bool` and `u8` have the same size (1 byte) [1].
    ///   - Since the closure takes a `&u8` argument, given a `Ptr<'a, bool>`
    ///     which satisfies the preconditions of
    ///     `TryFromBytes::<bool>::is_bit_valid`, it must be guaranteed that the
    ///     memory referenced by that `Ptr` always contains a valid `u8`. Since
    ///     `bool`'s single byte is always initialized, `is_bit_valid`'s
    ///     precondition requires that the same is true of its argument. Since
    ///     `u8`'s only bit validity invariant is that its single byte must be
    ///     initialized, this memory is guaranteed to contain a valid `u8`.
    ///   - The alignment of `bool` is equal to the alignment of `u8`. [1] [2]
    ///   - The impl must only return `true` for its argument if the original
    ///     `Ptr<bool>` refers to a valid `bool`. We only return true if the
    ///     `u8` value is 0 or 1, and both of these are valid values for `bool`.
    ///     [3]
    ///
    /// [1] Per https://doc.rust-lang.org/reference/type-layout.html#primitive-data-layout:
    ///
    ///   The size of most primitives is given in this table.
    ///
    ///   | Type      | `size_of::<Type>() ` |
    ///   |-----------|----------------------|
    ///   | `bool`    | 1                    |
    ///   | `u8`/`i8` | 1                    |
    ///
    /// [2] Per https://doc.rust-lang.org/reference/type-layout.html#size-and-alignment:
    ///
    ///   The size of a value is always a multiple of its alignment.
    ///
    /// [3] Per https://doc.rust-lang.org/reference/types/boolean.html:
    ///
    ///   The value false has the bit pattern 0x00 and the value true has the
    ///   bit pattern 0x01.
    unsafe_impl!(bool: TryFromBytes; |byte: &u8| *byte < 2);
}
safety_comment! {
    /// SAFETY:
    /// - `FromZeroes`: Per reference [1], "[a] value of type char is a Unicode
    ///   scalar value (i.e. a code point that is not a surrogate), represented
    ///   as a 32-bit unsigned word in the 0x0000 to 0xD7FF or 0xE000 to
    ///   0x10FFFF range" which contains 0x0000.
    /// - `AsBytes`: `char` is per reference [1] "represented as a 32-bit
    ///   unsigned word" (`u32`) which is `AsBytes`. Note that unlike `u32`, not
    ///   all bit patterns are valid for `char`.
    ///
    /// [1] https://doc.rust-lang.org/reference/types/textual.html
    unsafe_impl!(char: FromZeroes, AsBytes);
    /// SAFETY:
    /// - The safety requirements for `unsafe_impl!` with an `is_bit_valid`
    ///   closure:
    ///   - Given `t: *mut char` and `let r = *mut u32`, `r` refers to an object
    ///     of the same size as that referred to by `t`. This is true because
    ///     `char` and `u32` have the same size [1].
    ///   - Since the closure takes a `&u32` argument, given a `Ptr<'a, char>`
    ///     which satisfies the preconditions of
    ///     `TryFromBytes::<char>::is_bit_valid`, it must be guaranteed that the
    ///     memory referenced by that `Ptr` always contains a valid `u32`. Since
    ///     `char`'s bytes are always initialized [2], `is_bit_valid`'s
    ///     precondition requires that the same is true of its argument. Since
    ///     `u32`'s only bit validity invariant is that its bytes must be
    ///     initialized, this memory is guaranteed to contain a valid `u32`.
    ///   - The alignment of `char` is equal to the alignment of `u32`. [1]
    ///   - The impl must only return `true` for its argument if the original
    ///     `Ptr<char>` refers to a valid `char`. `char::from_u32` guarantees
    ///     that it returns `None` if its input is not a valid `char`. [3]
    ///
    /// [1] Per https://doc.rust-lang.org/nightly/reference/types/textual.html#layout-and-bit-validity:
    ///
    ///   `char` is guaranteed to have the same size and alignment as `u32` on
    ///   all platforms.
    ///
    /// [2] Per https://doc.rust-lang.org/core/primitive.char.html#method.from_u32:
    ///
    ///   Every byte of a `char` is guaranteed to be initialized.
    ///
    /// [3] Per https://doc.rust-lang.org/core/primitive.char.html#method.from_u32:
    ///
    ///   `from_u32()` will return `None` if the input is not a valid value for
    ///   a `char`.
    unsafe_impl!(char: TryFromBytes; |candidate: &u32| char::from_u32(*candidate).is_some());
}
safety_comment! {
    /// SAFETY:
    /// - `FromZeroes`, `AsBytes`, `Unaligned`: Per the reference [1], `str`
    ///   has the same layout as `[u8]`, and `[u8]` is `FromZeroes`, `AsBytes`,
    ///   and `Unaligned`.
    ///
    /// Note that we don't `assert_unaligned!(str)` because `assert_unaligned!`
    /// uses `align_of`, which only works for `Sized` types.
    ///
    /// TODO(#429): Add quotes from documentation.
    ///
    /// [1] https://doc.rust-lang.org/reference/type-layout.html#str-layout
    unsafe_impl!(str: FromZeroes, AsBytes, Unaligned);
    /// SAFETY:
    /// - The safety requirements for `unsafe_impl!` with an `is_bit_valid`
    ///   closure:
    ///   - Given `t: *mut str` and `let r = *mut [u8]`, `r` refers to an object
    ///     of the same size as that referred to by `t`. This is true because
    ///     `str` and `[u8]` have the same representation. [1]
    ///   - Since the closure takes a `&[u8]` argument, given a `Ptr<'a, str>`
    ///     which satisfies the preconditions of
    ///     `TryFromBytes::<str>::is_bit_valid`, it must be guaranteed that the
    ///     memory referenced by that `Ptr` always contains a valid `[u8]`.
    ///     Since `str`'s bytes are always initialized [1], `is_bit_valid`'s
    ///     precondition requires that the same is true of its argument. Since
    ///     `[u8]`'s only bit validity invariant is that its bytes must be
    ///     initialized, this memory is guaranteed to contain a valid `[u8]`.
    ///   - The alignment of `str` is equal to the alignment of `[u8]`. [1]
    ///   - The impl must only return `true` for its argument if the original
    ///     `Ptr<str>` refers to a valid `str`. `str::from_utf8` guarantees that
    ///     it returns `Err` if its input is not a valid `str`. [2]
    ///
    /// [1] Per https://doc.rust-lang.org/reference/types/textual.html:
    ///
    ///   A value of type `str` is represented the same was as `[u8]`.
    ///
    /// [2] Per https://doc.rust-lang.org/core/str/fn.from_utf8.html#errors:
    ///
    ///   Returns `Err` if the slice is not UTF-8.
    unsafe_impl!(str: TryFromBytes; |candidate: &[u8]| core::str::from_utf8(candidate).is_ok());
}

safety_comment! {
    // `NonZeroXxx` is `AsBytes`, but not `FromZeroes` or `FromBytes`.
    //
    /// SAFETY:
    /// - `AsBytes`: `NonZeroXxx` has the same layout as its associated
    ///    primitive. Since it is the same size, this guarantees it has no
    ///    padding - integers have no padding, and there's no room for padding
    ///    if it can represent all of the same values except 0.
    /// - `Unaligned`: `NonZeroU8` and `NonZeroI8` document that
    ///   `Option<NonZeroU8>` and `Option<NonZeroI8>` both have size 1. [1] [2]
    ///   This is worded in a way that makes it unclear whether it's meant as a
    ///   guarantee, but given the purpose of those types, it's virtually
    ///   unthinkable that that would ever change. `Option` cannot be smaller
    ///   than its contained type, which implies that, and `NonZeroX8` are of
    ///   size 1 or 0. `NonZeroX8` can represent multiple states, so they cannot
    ///   be 0 bytes, which means that they must be 1 byte. The only valid
    ///   alignment for a 1-byte type is 1.
    ///
    /// TODO(#429): Add quotes from documentation.
    ///
    /// [1] https://doc.rust-lang.org/stable/std/num/struct.NonZeroU8.html
    /// [2] https://doc.rust-lang.org/stable/std/num/struct.NonZeroI8.html
    /// TODO(https://github.com/rust-lang/rust/pull/104082): Cite documentation
    /// that layout is the same as primitive layout.
    unsafe_impl!(NonZeroU8: AsBytes, Unaligned);
    unsafe_impl!(NonZeroI8: AsBytes, Unaligned);
    assert_unaligned!(NonZeroU8, NonZeroI8);
    unsafe_impl!(NonZeroU16: AsBytes);
    unsafe_impl!(NonZeroI16: AsBytes);
    unsafe_impl!(NonZeroU32: AsBytes);
    unsafe_impl!(NonZeroI32: AsBytes);
    unsafe_impl!(NonZeroU64: AsBytes);
    unsafe_impl!(NonZeroI64: AsBytes);
    unsafe_impl!(NonZeroU128: AsBytes);
    unsafe_impl!(NonZeroI128: AsBytes);
    unsafe_impl!(NonZeroUsize: AsBytes);
    unsafe_impl!(NonZeroIsize: AsBytes);
    /// SAFETY:
    /// - The safety requirements for `unsafe_impl!` with an `is_bit_valid`
    ///   closure:
    ///   - Given `t: *mut NonZeroXxx` and `let r = *mut xxx`, `r` refers to an
    ///     object of the same size as that referred to by `t`. This is true
    ///     because `NonZeroXxx` and `xxx` have the same size. [1]
    ///   - Since the closure takes a `&xxx` argument, given a `Ptr<'a,
    ///     NonZeroXxx>` which satisfies the preconditions of
    ///     `TryFromBytes::<NonZeroXxx>::is_bit_valid`, it must be guaranteed
    ///     that the memory referenced by that `Ptr` always contains a valid
    ///     `xxx`. Since `NonZeroXxx`'s bytes are always initialized [1],
    ///     `is_bit_valid`'s precondition requires that the same is true of its
    ///     argument. Since `xxx`'s only bit validity invariant is that its
    ///     bytes must be initialized, this memory is guaranteed to contain a
    ///     valid `xxx`.
    ///   - The alignment of `NonZeroXxx` is equal to the alignment of `xxx`.
    ///     [1]
    ///   - The impl must only return `true` for its argument if the original
    ///     `Ptr<NonZeroXxx>` refers to a valid `NonZeroXxx`. The only `xxx`
    ///     which is not also a valid `NonZeroXxx` is 0. [1]
    ///
    /// [1] Per https://doc.rust-lang.org/core/num/struct.NonZeroU16.html:
    ///
    ///   `NonZeroU16` is guaranteed to have the same layout and bit validity as
    ///   `u16` with the exception that `0` is not a valid instance.
    unsafe_impl!(NonZeroU8: TryFromBytes; |n: &u8| *n != 0);
    unsafe_impl!(NonZeroI8: TryFromBytes; |n: &i8| *n != 0);
    unsafe_impl!(NonZeroU16: TryFromBytes; |n: &u16| *n != 0);
    unsafe_impl!(NonZeroI16: TryFromBytes; |n: &i16| *n != 0);
    unsafe_impl!(NonZeroU32: TryFromBytes; |n: &u32| *n != 0);
    unsafe_impl!(NonZeroI32: TryFromBytes; |n: &i32| *n != 0);
    unsafe_impl!(NonZeroU64: TryFromBytes; |n: &u64| *n != 0);
    unsafe_impl!(NonZeroI64: TryFromBytes; |n: &i64| *n != 0);
    unsafe_impl!(NonZeroU128: TryFromBytes; |n: &u128| *n != 0);
    unsafe_impl!(NonZeroI128: TryFromBytes; |n: &i128| *n != 0);
    unsafe_impl!(NonZeroUsize: TryFromBytes; |n: &usize| *n != 0);
    unsafe_impl!(NonZeroIsize: TryFromBytes; |n: &isize| *n != 0);
}
safety_comment! {
    /// SAFETY:
    /// - `TryFromBytes` (with no validator), `FromZeroes`, `FromBytes`,
    ///   `AsBytes`: The Rust compiler reuses `0` value to represent `None`, so
    ///   `size_of::<Option<NonZeroXxx>>() == size_of::<xxx>()`; see
    ///   `NonZeroXxx` documentation.
    /// - `Unaligned`: `NonZeroU8` and `NonZeroI8` document that
    ///   `Option<NonZeroU8>` and `Option<NonZeroI8>` both have size 1. [1] [2]
    ///   This is worded in a way that makes it unclear whether it's meant as a
    ///   guarantee, but given the purpose of those types, it's virtually
    ///   unthinkable that that would ever change. The only valid alignment for
    ///   a 1-byte type is 1.
    ///
    /// TODO(#429): Add quotes from documentation.
    ///
    /// [1] https://doc.rust-lang.org/stable/std/num/struct.NonZeroU8.html
    /// [2] https://doc.rust-lang.org/stable/std/num/struct.NonZeroI8.html
    ///
    /// TODO(https://github.com/rust-lang/rust/pull/104082): Cite documentation
    /// for layout guarantees.
    unsafe_impl!(Option<NonZeroU8>: TryFromBytes, FromZeroes, FromBytes, AsBytes, Unaligned);
    unsafe_impl!(Option<NonZeroI8>: TryFromBytes, FromZeroes, FromBytes, AsBytes, Unaligned);
    assert_unaligned!(Option<NonZeroU8>, Option<NonZeroI8>);
    unsafe_impl!(Option<NonZeroU16>: TryFromBytes, FromZeroes, FromBytes, AsBytes);
    unsafe_impl!(Option<NonZeroI16>: TryFromBytes, FromZeroes, FromBytes, AsBytes);
    unsafe_impl!(Option<NonZeroU32>: TryFromBytes, FromZeroes, FromBytes, AsBytes);
    unsafe_impl!(Option<NonZeroI32>: TryFromBytes, FromZeroes, FromBytes, AsBytes);
    unsafe_impl!(Option<NonZeroU64>: TryFromBytes, FromZeroes, FromBytes, AsBytes);
    unsafe_impl!(Option<NonZeroI64>: TryFromBytes, FromZeroes, FromBytes, AsBytes);
    unsafe_impl!(Option<NonZeroU128>: TryFromBytes, FromZeroes, FromBytes, AsBytes);
    unsafe_impl!(Option<NonZeroI128>: TryFromBytes, FromZeroes, FromBytes, AsBytes);
    unsafe_impl!(Option<NonZeroUsize>: TryFromBytes, FromZeroes, FromBytes, AsBytes);
    unsafe_impl!(Option<NonZeroIsize>: TryFromBytes, FromZeroes, FromBytes, AsBytes);
}

safety_comment! {
    /// SAFETY:
    /// The following types can be transmuted from `[0u8; size_of::<T>()]`. [1]
    /// None of them contain `UnsafeCell`s, and so they all soundly implement
    /// `FromZeroes`.
    ///
    /// [1] Per
    /// https://doc.rust-lang.org/nightly/core/option/index.html#representation:
    ///
    ///   Rust guarantees to optimize the following types `T` such that
    ///   [`Option<T>`] has the same size and alignment as `T`. In some of these
    ///   cases, Rust further guarantees that `transmute::<_, Option<T>>([0u8;
    ///   size_of::<T>()])` is sound and produces `Option::<T>::None`. These
    ///   cases are identified by the second column:
    ///
    ///   | `T`                   | `transmute::<_, Option<T>>([0u8; size_of::<T>()])` sound? |
    ///   |-----------------------|-----------------------------------------------------------|
    ///   | [`Box<U>`]            | when `U: Sized`                                           |
    ///   | `&U`                  | when `U: Sized`                                           |
    ///   | `&mut U`              | when `U: Sized`                                           |
    ///   | [`ptr::NonNull<U>`]   | when `U: Sized`                                           |
    ///   | `fn`, `extern "C" fn` | always                                                    |
    ///
    /// TODO(#429), TODO(https://github.com/rust-lang/rust/pull/115333): Cite
    /// the Stable docs once they're available.
    #[cfg(feature = "alloc")]
    unsafe_impl!(
        #[cfg_attr(doc_cfg, doc(cfg(feature = "alloc")))]
        T => FromZeroes for Option<Box<T>>
    );
    unsafe_impl!(T => FromZeroes for Option<&'_ T>);
    unsafe_impl!(T => FromZeroes for Option<&'_ mut T>);
    unsafe_impl!(T => FromZeroes for Option<NonNull<T>>);
    unsafe_impl_for_power_set!(A, B, C, D, E, F, G, H, I, J, K, L -> M => FromZeroes for opt_fn!(...));
    unsafe_impl_for_power_set!(A, B, C, D, E, F, G, H, I, J, K, L -> M => FromZeroes for opt_extern_c_fn!(...));
}

safety_comment! {
    /// SAFETY:
    /// Per reference [1]:
    /// "For all T, the following are guaranteed:
    /// size_of::<PhantomData<T>>() == 0
    /// align_of::<PhantomData<T>>() == 1".
    /// This gives:
    /// - `TryFromBytes` (with no validator), `FromZeroes`, `FromBytes`: There
    ///   is only one possible sequence of 0 bytes, and `PhantomData` is
    ///   inhabited.
    /// - `AsBytes`: Since `PhantomData` has size 0, it contains no padding
    ///   bytes.
    /// - `Unaligned`: Per the preceding reference, `PhantomData` has alignment
    ///   1.
    ///
    /// [1] https://doc.rust-lang.org/std/marker/struct.PhantomData.html#layout-1
    unsafe_impl!(T: ?Sized => TryFromBytes for PhantomData<T>);
    unsafe_impl!(T: ?Sized => FromZeroes for PhantomData<T>);
    unsafe_impl!(T: ?Sized => FromBytes for PhantomData<T>);
    unsafe_impl!(T: ?Sized => AsBytes for PhantomData<T>);
    unsafe_impl!(T: ?Sized => Unaligned for PhantomData<T>);
    assert_unaligned!(PhantomData<()>, PhantomData<u8>, PhantomData<u64>);
}
safety_comment! {
    /// SAFETY:
    /// `Wrapping<T>` is guaranteed by its docs [1] to have the same layout and
    /// bit validity as `T`. Also, `Wrapping<T>` is `#[repr(transparent)]`, and
    /// has a single field, which is `pub`. Per the reference [2], this means
    /// that the `#[repr(transparent)]` attribute is "considered part of the
    /// public ABI".
    ///
    /// - `TryFromBytes`: The safety requirements for `unsafe_impl!` with an
    ///   `is_bit_valid` closure:
    ///   - Given `t: *mut Wrapping<T>` and `let r = *mut T`, `r` refers to an
    ///     object of the same size as that referred to by `t`. This is true
    ///     because `Wrapping<T>` and `T` have the same layout
    ///   - The alignment of `Wrapping<T>` is equal to the alignment of `T`.
    ///   - The impl must only return `true` for its argument if the original
    ///     `Ptr<Wrapping<T>>` refers to a valid `Wrapping<T>`. Since
    ///     `Wrapping<T>` has the same bit validity as `T`, and since our impl
    ///     just calls `T::is_bit_valid`, our impl returns `true` exactly when
    ///     its argument contains a valid `Wrapping<T>`.
    /// - `FromBytes`: Since `Wrapping<T>` has the same bit validity as `T`, if
    ///   `T: FromBytes`, then all initialized byte sequences are valid
    ///   instances of `Wrapping<T>`. Similarly, if `T: FromBytes`, then
    ///   `Wrapping<T>` doesn't contain any `UnsafeCell`s. Thus, `impl FromBytes
    ///   for Wrapping<T> where T: FromBytes` is a sound impl.
    /// - `AsBytes`: Since `Wrapping<T>` has the same bit validity as `T`, if
    ///   `T: AsBytes`, then all valid instances of `Wrapping<T>` have all of
    ///   their bytes initialized. Similarly, if `T: AsBytes`, then
    ///   `Wrapping<T>` doesn't contain any `UnsafeCell`s. Thus, `impl AsBytes
    ///   for Wrapping<T> where T: AsBytes` is a valid impl.
    /// - `Unaligned`: Since `Wrapping<T>` has the same layout as `T`,
    ///   `Wrapping<T>` has alignment 1 exactly when `T` does.
    ///
    /// [1] Per https://doc.rust-lang.org/core/num/struct.NonZeroU16.html:
    ///
    ///   `NonZeroU16` is guaranteed to have the same layout and bit validity as
    ///   `u16` with the exception that `0` is not a valid instance.
    ///
    /// TODO(#429): Add quotes from documentation.
    ///
    /// [1] TODO(https://doc.rust-lang.org/nightly/core/num/struct.Wrapping.html#layout-1):
    /// Reference this documentation once it's available on stable.
    ///
    /// [2] https://doc.rust-lang.org/nomicon/other-reprs.html#reprtransparent
    unsafe_impl!(T: TryFromBytes => TryFromBytes for Wrapping<T>; |candidate: Ptr<T>| {
        // SAFETY:
        // - Since `T` and `Wrapping<T>` have the same layout and bit validity
        //   and contain the same fields, `T` contains `UnsafeCell`s exactly
        //   where `Wrapping<T>` does. Thus, all memory and `UnsafeCell`
        //   preconditions of `T::is_bit_valid` hold exactly when the same
        //   preconditions for `Wrapping<T>::is_bit_valid` hold.
        // - By the same token, since `candidate` is guaranteed to have its
        //   bytes initialized where there are always initialized bytes in
        //   `Wrapping<T>`, the same is true for `T`.
        unsafe { T::is_bit_valid(candidate) }
    });
    unsafe_impl!(T: FromZeroes => FromZeroes for Wrapping<T>);
    unsafe_impl!(T: FromBytes => FromBytes for Wrapping<T>);
    unsafe_impl!(T: AsBytes => AsBytes for Wrapping<T>);
    unsafe_impl!(T: Unaligned => Unaligned for Wrapping<T>);
    assert_unaligned!(Wrapping<()>, Wrapping<u8>);
}
safety_comment! {
    // `MaybeUninit<T>` is `FromZeroes` and `FromBytes`, but never `AsBytes`
    // since it may contain uninitialized bytes.
    //
    /// SAFETY:
    /// - `TryFromBytes` (with no validator), `FromZeroes`, `FromBytes`:
    ///   `MaybeUninit<T>` has no restrictions on its contents. Unfortunately,
    ///   in addition to bit validity, `TryFromBytes`, `FromZeroes` and
    ///   `FromBytes` also require that implementers contain no `UnsafeCell`s.
    ///   Thus, we require `T: Trait` in order to ensure that `T` - and thus
    ///   `MaybeUninit<T>` - contains to `UnsafeCell`s. Thus, requiring that `T`
    ///   implement each of these traits is sufficient.
    /// - `Unaligned`: "MaybeUninit<T> is guaranteed to have the same size,
    ///    alignment, and ABI as T" [1]
    ///
    /// [1] https://doc.rust-lang.org/stable/core/mem/union.MaybeUninit.html#layout-1
    ///
    /// TODO(https://github.com/google/zerocopy/issues/251): If we split
    /// `FromBytes` and `RefFromBytes`, or if we introduce a separate
    /// `NoCell`/`Freeze` trait, we can relax the trait bounds for `FromZeroes`
    /// and `FromBytes`.
    unsafe_impl!(T: TryFromBytes => TryFromBytes for MaybeUninit<T>);
    unsafe_impl!(T: FromZeroes => FromZeroes for MaybeUninit<T>);
    unsafe_impl!(T: FromBytes => FromBytes for MaybeUninit<T>);
    unsafe_impl!(T: Unaligned => Unaligned for MaybeUninit<T>);
    assert_unaligned!(MaybeUninit<()>, MaybeUninit<u8>);
}
safety_comment! {
    /// SAFETY:
    /// `ManuallyDrop` has the same layout and bit validity as `T` [1], and
    /// accessing the inner value is safe (meaning that it's unsound to leave
    /// the inner value uninitialized while exposing the `ManuallyDrop` to safe
    /// code).
    /// - `FromZeroes`, `FromBytes`: Since it has the same layout as `T`, any
    ///   valid `T` is a valid `ManuallyDrop<T>`. If `T: FromZeroes`, a sequence
    ///   of zero bytes is a valid `T`, and thus a valid `ManuallyDrop<T>`. If
    ///   `T: FromBytes`, any sequence of bytes is a valid `T`, and thus a valid
    ///   `ManuallyDrop<T>`.
    /// - `AsBytes`: Since it has the same layout as `T`, and since it's unsound
    ///   to let safe code access a `ManuallyDrop` whose inner value is
    ///   uninitialized, safe code can only ever access a `ManuallyDrop` whose
    ///   contents are a valid `T`. Since `T: AsBytes`, this means that safe
    ///   code can only ever access a `ManuallyDrop` with all initialized bytes.
    /// - `Unaligned`: `ManuallyDrop` has the same layout (and thus alignment)
    ///   as `T`, and `T: Unaligned` guarantees that that alignment is 1.
    ///
    ///   `ManuallyDrop<T>` is guaranteed to have the same layout and bit
    ///   validity as `T`
    ///
    /// [1] Per https://doc.rust-lang.org/nightly/core/mem/struct.ManuallyDrop.html:
    ///
    /// TODO(#429):
    /// - Add quotes from docs.
    /// - Once [1] (added in
    /// https://github.com/rust-lang/rust/pull/115522) is available on stable,
    /// quote the stable docs instead of the nightly docs.
    unsafe_impl!(T: ?Sized + FromZeroes => FromZeroes for ManuallyDrop<T>);
    unsafe_impl!(T: ?Sized + FromBytes => FromBytes for ManuallyDrop<T>);
    unsafe_impl!(T: ?Sized + AsBytes => AsBytes for ManuallyDrop<T>);
    unsafe_impl!(T: ?Sized + Unaligned => Unaligned for ManuallyDrop<T>);
    assert_unaligned!(ManuallyDrop<()>, ManuallyDrop<u8>);
}
safety_comment! {
    /// SAFETY:
    /// Per the reference [1]:
    ///
    ///   An array of `[T; N]` has a size of `size_of::<T>() * N` and the same
    ///   alignment of `T`. Arrays are laid out so that the zero-based `nth`
    ///   element of the array is offset from the start of the array by `n *
    ///   size_of::<T>()` bytes.
    ///
    ///   ...
    ///
    ///   Slices have the same layout as the section of the array they slice.
    ///
    /// In other words, the layout of a `[T]` or `[T; N]` is a sequence of `T`s
    /// laid out back-to-back with no bytes in between. Therefore, `[T]` or `[T;
    /// N]` are `TryFromBytes`, `FromZeroes`, `FromBytes`, and `AsBytes` if `T`
    /// is (respectively). Furthermore, since an array/slice has "the same
    /// alignment of `T`", `[T]` and `[T; N]` are `Unaligned` if `T` is.
    ///
    /// Note that we don't `assert_unaligned!` for slice types because
    /// `assert_unaligned!` uses `align_of`, which only works for `Sized` types.
    ///
    /// [1] https://doc.rust-lang.org/reference/type-layout.html#array-layout
    unsafe_impl!(const N: usize, T: FromZeroes => FromZeroes for [T; N]);
    unsafe_impl!(const N: usize, T: FromBytes => FromBytes for [T; N]);
    unsafe_impl!(const N: usize, T: AsBytes => AsBytes for [T; N]);
    unsafe_impl!(const N: usize, T: Unaligned => Unaligned for [T; N]);
    assert_unaligned!([(); 0], [(); 1], [u8; 0], [u8; 1]);
    unsafe_impl!(T: TryFromBytes => TryFromBytes for [T]; |c: Ptr<[T]>| {
        // SAFETY: Assuming the preconditions of `is_bit_valid` are satisfied,
        // so too will the postcondition: that, if `is_bit_valid(candidate)`
        // returns true, `*candidate` contains a valid `Self`. Per the reference
        // [1]:
        //
        //   An array of `[T; N]` has a size of `size_of::<T>() * N` and the
        //   same alignment of `T`. Arrays are laid out so that the zero-based
        //   `nth` element of the array is offset from the start of the array by
        //   `n * size_of::<T>()` bytes.
        //
        //   ...
        //
        //   Slices have the same layout as the section of the array they slice.
        //
        // In other words, the layout of a `[T] is a sequence of `T`s laid out
        // back-to-back with no bytes in between. If all elements in `candidate`
        // are `is_bit_valid`, so too is `candidate`.
        //
        // Note that any of the below calls may panic, but it would still be
        // sound even if it did. `is_bit_valid` does not promise that it will
        // not panic (in fact, it explicitly warns that it's a possibility), and
        // we have not violated any safety invariants that we must fix before
        // returning.
        c.iter().all(|elem|
            // SAFETY: We uphold the safety contract of `is_bit_valid(elem)`, by
            // precondition on the surrounding call to `is_bit_valid`. The
            // memory referenced by `elem` is contained entirely within `c`, and
            // satisfies the preconditions satisfied by `c`. By axiom, we assume
            // that `Iterator:all` does not invalidate these preconditions
            // (e.g., by writing to `elem`.) Since `elem` is derived from `c`,
            // it is only possible for uninitialized bytes to occur in `elem` at
            // the same bytes they occur within `c`.
            unsafe { <T as TryFromBytes>::is_bit_valid(elem) }
        )
    });
    unsafe_impl!(T: FromZeroes => FromZeroes for [T]);
    unsafe_impl!(T: FromBytes => FromBytes for [T]);
    unsafe_impl!(T: AsBytes => AsBytes for [T]);
    unsafe_impl!(T: Unaligned => Unaligned for [T]);
}
safety_comment! {
    /// SAFETY:
    /// - `FromZeroes`: For thin pointers (note that `T: Sized`), the zero
    ///   pointer is considered "null". [1] No operations which require
    ///   provenance are legal on null pointers, so this is not a footgun.
    ///
    /// NOTE(#170): Implementing `FromBytes` and `AsBytes` for raw pointers
    /// would be sound, but carries provenance footguns. We want to support
    /// `FromBytes` and `AsBytes` for raw pointers eventually, but we are
    /// holding off until we can figure out how to address those footguns.
    ///
    /// [1] TODO(https://github.com/rust-lang/rust/pull/116988): Cite the
    /// documentation once this PR lands.
    unsafe_impl!(T => FromZeroes for *const T);
    unsafe_impl!(T => FromZeroes for *mut T);
}

// SIMD support
//
// Per the Unsafe Code Guidelines Reference [1]:
//
//   Packed SIMD vector types are `repr(simd)` homogeneous tuple-structs
//   containing `N` elements of type `T` where `N` is a power-of-two and the
//   size and alignment requirements of `T` are equal:
//
//   ```rust
//   #[repr(simd)]
//   struct Vector<T, N>(T_0, ..., T_(N - 1));
//   ```
//
//   ...
//
//   The size of `Vector` is `N * size_of::<T>()` and its alignment is an
//   implementation-defined function of `T` and `N` greater than or equal to
//   `align_of::<T>()`.
//
//   ...
//
//   Vector elements are laid out in source field order, enabling random access
//   to vector elements by reinterpreting the vector as an array:
//
//   ```rust
//   union U {
//      vec: Vector<T, N>,
//      arr: [T; N]
//   }
//
//   assert_eq!(size_of::<Vector<T, N>>(), size_of::<[T; N]>());
//   assert!(align_of::<Vector<T, N>>() >= align_of::<[T; N]>());
//
//   unsafe {
//     let u = U { vec: Vector<T, N>(t_0, ..., t_(N - 1)) };
//
//     assert_eq!(u.vec.0, u.arr[0]);
//     // ...
//     assert_eq!(u.vec.(N - 1), u.arr[N - 1]);
//   }
//   ```
//
// Given this background, we can observe that:
// - The size and bit pattern requirements of a SIMD type are equivalent to the
//   equivalent array type. Thus, for any SIMD type whose primitive `T` is
//   `TryFromBytes`, `FromZeroes`, `FromBytes`, or `AsBytes`, that SIMD type is
//   also `TryFromBytes`, `FromZeroes`, `FromBytes`, or `AsBytes` respectively.
// - Since no upper bound is placed on the alignment, no SIMD type can be
//   guaranteed to be `Unaligned`.
//
// Also per [1]:
//
//   This chapter represents the consensus from issue #38. The statements in
//   here are not (yet) "guaranteed" not to change until an RFC ratifies them.
//
// See issue #38 [2]. While this behavior is not technically guaranteed, the
// likelihood that the behavior will change such that SIMD types are no longer
// `TryFromBytes`, `FromZeroes`, `FromBytes`, or `AsBytes` is next to zero, as
// that would defeat the entire purpose of SIMD types. Nonetheless, we put this
// behavior behind the `simd` Cargo feature, which requires consumers to opt
// into this stability hazard.
//
// [1] https://rust-lang.github.io/unsafe-code-guidelines/layout/packed-simd-vectors.html
// [2] https://github.com/rust-lang/unsafe-code-guidelines/issues/38
#[cfg(feature = "simd")]
#[cfg_attr(doc_cfg, doc(cfg(feature = "simd")))]
mod simd {
    /// Defines a module which implements `TryFromBytes`, `FromZeroes`,
    /// `FromBytes`, and `AsBytes` for a set of types from a module in
    /// `core::arch`.
    ///
    /// `$arch` is both the name of the defined module and the name of the
    /// module in `core::arch`, and `$typ` is the list of items from that module
    /// to implement `FromZeroes`, `FromBytes`, and `AsBytes` for.
    #[allow(unused_macros)] // `allow(unused_macros)` is needed because some
                            // target/feature combinations don't emit any impls
                            // and thus don't use this macro.
    macro_rules! simd_arch_mod {
        (#[cfg $cfg:tt] $arch:ident, $mod:ident, $($typ:ident),*) => {
            #[cfg $cfg]
            #[cfg_attr(doc_cfg, doc(cfg $cfg))]
            mod $mod {
                use core::arch::$arch::{$($typ),*};

                use crate::*;
                impl_known_layout!($($typ),*);
                safety_comment! {
                    /// SAFETY:
                    /// See comment on module definition for justification.
                    $( unsafe_impl!($typ: TryFromBytes, FromZeroes, FromBytes, AsBytes); )*
                }
            }
        };
    }

    #[rustfmt::skip]
    const _: () = {
        simd_arch_mod!(
            #[cfg(target_arch = "x86")]
            x86, x86, __m128, __m128d, __m128i, __m256, __m256d, __m256i
        );
        simd_arch_mod!(
            #[cfg(all(feature = "simd-nightly", target_arch = "x86"))]
            x86, x86_nightly, __m512bh, __m512, __m512d, __m512i
        );
        simd_arch_mod!(
            #[cfg(target_arch = "x86_64")]
            x86_64, x86_64, __m128, __m128d, __m128i, __m256, __m256d, __m256i
        );
        simd_arch_mod!(
            #[cfg(all(feature = "simd-nightly", target_arch = "x86_64"))]
            x86_64, x86_64_nightly, __m512bh, __m512, __m512d, __m512i
        );
        simd_arch_mod!(
            #[cfg(target_arch = "wasm32")]
            wasm32, wasm32, v128
        );
        simd_arch_mod!(
            #[cfg(all(feature = "simd-nightly", target_arch = "powerpc"))]
            powerpc, powerpc, vector_bool_long, vector_double, vector_signed_long, vector_unsigned_long
        );
        simd_arch_mod!(
            #[cfg(all(feature = "simd-nightly", target_arch = "powerpc64"))]
            powerpc64, powerpc64, vector_bool_long, vector_double, vector_signed_long, vector_unsigned_long
        );
        simd_arch_mod!(
            #[cfg(target_arch = "aarch64")]
            aarch64, aarch64, float32x2_t, float32x4_t, float64x1_t, float64x2_t, int8x8_t, int8x8x2_t,
            int8x8x3_t, int8x8x4_t, int8x16_t, int8x16x2_t, int8x16x3_t, int8x16x4_t, int16x4_t,
            int16x8_t, int32x2_t, int32x4_t, int64x1_t, int64x2_t, poly8x8_t, poly8x8x2_t, poly8x8x3_t,
            poly8x8x4_t, poly8x16_t, poly8x16x2_t, poly8x16x3_t, poly8x16x4_t, poly16x4_t, poly16x8_t,
            poly64x1_t, poly64x2_t, uint8x8_t, uint8x8x2_t, uint8x8x3_t, uint8x8x4_t, uint8x16_t,
            uint8x16x2_t, uint8x16x3_t, uint8x16x4_t, uint16x4_t, uint16x8_t, uint32x2_t, uint32x4_t,
            uint64x1_t, uint64x2_t
        );
        simd_arch_mod!(
            #[cfg(all(feature = "simd-nightly", target_arch = "arm"))]
            arm, arm, int8x4_t, uint8x4_t
        );
    };
}

/// Safely transmutes a value of one type to a value of another type of the same
/// size.
///
/// The expression `$e` must have a concrete type, `T`, which implements
/// `AsBytes`. The `transmute!` expression must also have a concrete type, `U`
/// (`U` is inferred from the calling context), and `U` must implement
/// `FromBytes`.
///
/// Note that the `T` produced by the expression `$e` will *not* be dropped.
/// Semantically, its bits will be copied into a new value of type `U`, the
/// original `T` will be forgotten, and the value of type `U` will be returned.
///
/// # Examples
///
/// ```
/// # use zerocopy::transmute;
/// let one_dimensional: [u8; 8] = [0, 1, 2, 3, 4, 5, 6, 7];
///
/// let two_dimensional: [[u8; 4]; 2] = transmute!(one_dimensional);
///
/// assert_eq!(two_dimensional, [[0, 1, 2, 3], [4, 5, 6, 7]]);
/// ```
#[macro_export]
macro_rules! transmute {
    ($e:expr) => {{
        // NOTE: This must be a macro (rather than a function with trait bounds)
        // because there's no way, in a generic context, to enforce that two
        // types have the same size. `core::mem::transmute` uses compiler magic
        // to enforce this so long as the types are concrete.

        let e = $e;
        if false {
            // This branch, though never taken, ensures that the type of `e` is
            // `AsBytes` and that the type of this macro invocation expression
            // is `FromBytes`.

            struct AssertIsAsBytes<T: $crate::AsBytes>(T);
            let _ = AssertIsAsBytes(e);

            struct AssertIsFromBytes<U: $crate::FromBytes>(U);
            #[allow(unused, unreachable_code)]
            let u = AssertIsFromBytes(loop {});
            u.0
        } else {
            // SAFETY: `core::mem::transmute` ensures that the type of `e` and
            // the type of this macro invocation expression have the same size.
            // We know this transmute is safe thanks to the `AsBytes` and
            // `FromBytes` bounds enforced by the `false` branch.
            //
            // We use this reexport of `core::mem::transmute` because we know it
            // will always be available for crates which are using the 2015
            // edition of Rust. By contrast, if we were to use
            // `std::mem::transmute`, this macro would not work for such crates
            // in `no_std` contexts, and if we were to use
            // `core::mem::transmute`, this macro would not work in `std`
            // contexts in which `core` was not manually imported. This is not a
            // problem for 2018 edition crates.
            unsafe {
                // Clippy: It's okay to transmute a type to itself.
                #[allow(clippy::useless_transmute, clippy::missing_transmute_annotations)]
                $crate::macro_util::core_reexport::mem::transmute(e)
            }
        }
    }}
}

/// Safely transmutes a mutable or immutable reference of one type to an
/// immutable reference of another type of the same size.
///
/// The expression `$e` must have a concrete type, `&T` or `&mut T`, where `T:
/// Sized + AsBytes`. The `transmute_ref!` expression must also have a concrete
/// type, `&U` (`U` is inferred from the calling context), where `U: Sized +
/// FromBytes`. It must be the case that `align_of::<T>() >= align_of::<U>()`.
///
/// The lifetime of the input type, `&T` or `&mut T`, must be the same as or
/// outlive the lifetime of the output type, `&U`.
///
/// # Examples
///
/// ```
/// # use zerocopy::transmute_ref;
/// let one_dimensional: [u8; 8] = [0, 1, 2, 3, 4, 5, 6, 7];
///
/// let two_dimensional: &[[u8; 4]; 2] = transmute_ref!(&one_dimensional);
///
/// assert_eq!(two_dimensional, &[[0, 1, 2, 3], [4, 5, 6, 7]]);
/// ```
///
/// # Alignment increase error message
///
/// Because of limitations on macros, the error message generated when
/// `transmute_ref!` is used to transmute from a type of lower alignment to a
/// type of higher alignment is somewhat confusing. For example, the following
/// code:
///
/// ```compile_fail
/// const INCREASE_ALIGNMENT: &u16 = zerocopy::transmute_ref!(&[0u8; 2]);
/// ```
///
/// ...generates the following error:
///
/// ```text
/// error[E0512]: cannot transmute between types of different sizes, or dependently-sized types
///  --> src/lib.rs:1524:34
///   |
/// 5 | const INCREASE_ALIGNMENT: &u16 = zerocopy::transmute_ref!(&[0u8; 2]);
///   |                                  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
///   |
///   = note: source type: `AlignOf<[u8; 2]>` (8 bits)
///   = note: target type: `MaxAlignsOf<[u8; 2], u16>` (16 bits)
///   = note: this error originates in the macro `$crate::assert_align_gt_eq` which comes from the expansion of the macro `transmute_ref` (in Nightly builds, run with -Z macro-backtrace for more info)
/// ```
///
/// This is saying that `max(align_of::<T>(), align_of::<U>()) !=
/// align_of::<T>()`, which is equivalent to `align_of::<T>() <
/// align_of::<U>()`.
#[macro_export]
macro_rules! transmute_ref {
    ($e:expr) => {{
        // NOTE: This must be a macro (rather than a function with trait bounds)
        // because there's no way, in a generic context, to enforce that two
        // types have the same size or alignment.

        // Ensure that the source type is a reference or a mutable reference
        // (note that mutable references are implicitly reborrowed here).
        let e: &_ = $e;

        #[allow(unused, clippy::diverging_sub_expression)]
        if false {
            // This branch, though never taken, ensures that the type of `e` is
            // `&T` where `T: 't + Sized + AsBytes`, that the type of this macro
            // expression is `&U` where `U: 'u + Sized + FromBytes`, and that
            // `'t` outlives `'u`.

            struct AssertIsAsBytes<'a, T: ::core::marker::Sized + $crate::AsBytes>(&'a T);
            let _ = AssertIsAsBytes(e);

            struct AssertIsFromBytes<'a, U: ::core::marker::Sized + $crate::FromBytes>(&'a U);
            #[allow(unused, unreachable_code)]
            let u = AssertIsFromBytes(loop {});
            u.0
        } else if false {
            // This branch, though never taken, ensures that `size_of::<T>() ==
            // size_of::<U>()` and that that `align_of::<T>() >=
            // align_of::<U>()`.

            // `t` is inferred to have type `T` because it's assigned to `e` (of
            // type `&T`) as `&t`.
            let mut t = unreachable!();
            e = &t;

            // `u` is inferred to have type `U` because it's used as `&u` as the
            // value returned from this branch.
            let u;

            $crate::assert_size_eq!(t, u);
            $crate::assert_align_gt_eq!(t, u);

            &u
        } else {
            // SAFETY: For source type `Src` and destination type `Dst`:
            // - We know that `Src: AsBytes` and `Dst: FromBytes` thanks to the
            //   uses of `AssertIsAsBytes` and `AssertIsFromBytes` above.
            // - We know that `size_of::<Src>() == size_of::<Dst>()` thanks to
            //   the use of `assert_size_eq!` above.
            // - We know that `align_of::<Src>() >= align_of::<Dst>()` thanks to
            //   the use of `assert_align_gt_eq!` above.
            unsafe { $crate::macro_util::transmute_ref(e) }
        }
    }}
}

/// Safely transmutes a mutable reference of one type to an mutable reference of
/// another type of the same size.
///
/// The expression `$e` must have a concrete type, `&mut T`, where `T: Sized +
/// AsBytes`. The `transmute_mut!` expression must also have a concrete type,
/// `&mut U` (`U` is inferred from the calling context), where `U: Sized +
/// FromBytes`. It must be the case that `align_of::<T>() >= align_of::<U>()`.
///
/// The lifetime of the input type, `&mut T`, must be the same as or outlive the
/// lifetime of the output type, `&mut U`.
///
/// # Examples
///
/// ```
/// # use zerocopy::transmute_mut;
/// let mut one_dimensional: [u8; 8] = [0, 1, 2, 3, 4, 5, 6, 7];
///
/// let two_dimensional: &mut [[u8; 4]; 2] = transmute_mut!(&mut one_dimensional);
///
/// assert_eq!(two_dimensional, &[[0, 1, 2, 3], [4, 5, 6, 7]]);
///
/// two_dimensional.reverse();
///
/// assert_eq!(one_dimensional, [4, 5, 6, 7, 0, 1, 2, 3]);
/// ```
///
/// # Alignment increase error message
///
/// Because of limitations on macros, the error message generated when
/// `transmute_mut!` is used to transmute from a type of lower alignment to a
/// type of higher alignment is somewhat confusing. For example, the following
/// code:
///
/// ```compile_fail
/// const INCREASE_ALIGNMENT: &mut u16 = zerocopy::transmute_mut!(&mut [0u8; 2]);
/// ```
///
/// ...generates the following error:
///
/// ```text
/// error[E0512]: cannot transmute between types of different sizes, or dependently-sized types
///  --> src/lib.rs:1524:34
///   |
/// 5 | const INCREASE_ALIGNMENT: &mut u16 = zerocopy::transmute_mut!(&mut [0u8; 2]);
///   |                                      ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
///   |
///   = note: source type: `AlignOf<[u8; 2]>` (8 bits)
///   = note: target type: `MaxAlignsOf<[u8; 2], u16>` (16 bits)
///   = note: this error originates in the macro `$crate::assert_align_gt_eq` which comes from the expansion of the macro `transmute_mut` (in Nightly builds, run with -Z macro-backtrace for more info)
/// ```
///
/// This is saying that `max(align_of::<T>(), align_of::<U>()) !=
/// align_of::<T>()`, which is equivalent to `align_of::<T>() <
/// align_of::<U>()`.
#[macro_export]
macro_rules! transmute_mut {
    ($e:expr) => {{
        // NOTE: This must be a macro (rather than a function with trait bounds)
        // because there's no way, in a generic context, to enforce that two
        // types have the same size or alignment.

        // Ensure that the source type is a mutable reference.
        let e: &mut _ = $e;

        #[allow(unused, clippy::diverging_sub_expression)]
        if false {
            // This branch, though never taken, ensures that the type of `e` is
            // `&mut T` where `T: 't + Sized + FromBytes + AsBytes`, that the
            // type of this macro expression is `&mut U` where `U: 'u + Sized +
            // FromBytes + AsBytes`.

            // We use immutable references here rather than mutable so that, if
            // this macro is used in a const context (in which, as of this
            // writing, mutable references are banned), the error message
            // appears to originate in the user's code rather than in the
            // internals of this macro.
            struct AssertSrcIsFromBytes<'a, T: ::core::marker::Sized + $crate::FromBytes>(&'a T);
            struct AssertSrcIsAsBytes<'a, T: ::core::marker::Sized + $crate::AsBytes>(&'a T);
            struct AssertDstIsFromBytes<'a, T: ::core::marker::Sized + $crate::FromBytes>(&'a T);
            struct AssertDstIsAsBytes<'a, T: ::core::marker::Sized + $crate::AsBytes>(&'a T);

            if true {
                let _ = AssertSrcIsFromBytes(&*e);
            } else {
                let _ = AssertSrcIsAsBytes(&*e);
            }

            if true {
                #[allow(unused, unreachable_code)]
                let u = AssertDstIsFromBytes(loop {});
                &mut *u.0
            } else {
                #[allow(unused, unreachable_code)]
                let u = AssertDstIsAsBytes(loop {});
                &mut *u.0
            }
        } else if false {
            // This branch, though never taken, ensures that `size_of::<T>() ==
            // size_of::<U>()` and that that `align_of::<T>() >=
            // align_of::<U>()`.

            // `t` is inferred to have type `T` because it's assigned to `e` (of
            // type `&mut T`) as `&mut t`.
            let mut t = unreachable!();
            e = &mut t;

            // `u` is inferred to have type `U` because it's used as `&mut u` as
            // the value returned from this branch.
            let u;

            $crate::assert_size_eq!(t, u);
            $crate::assert_align_gt_eq!(t, u);

            &mut u
        } else {
            // SAFETY: For source type `Src` and destination type `Dst`:
            // - We know that `Src: FromBytes + AsBytes` and `Dst: FromBytes +
            //   AsBytes` thanks to the uses of `AssertSrcIsFromBytes`,
            //   `AssertSrcIsAsBytes`, `AssertDstIsFromBytes`, and
            //   `AssertDstIsAsBytes` above.
            // - We know that `size_of::<Src>() == size_of::<Dst>()` thanks to
            //   the use of `assert_size_eq!` above.
            // - We know that `align_of::<Src>() >= align_of::<Dst>()` thanks to
            //   the use of `assert_align_gt_eq!` above.
            unsafe { $crate::macro_util::transmute_mut(e) }
        }
    }}
}

/// Includes a file and safely transmutes it to a value of an arbitrary type.
///
/// The file will be included as a byte array, `[u8; N]`, which will be
/// transmuted to another type, `T`. `T` is inferred from the calling context,
/// and must implement [`FromBytes`].
///
/// The file is located relative to the current file (similarly to how modules
/// are found). The provided path is interpreted in a platform-specific way at
/// compile time. So, for instance, an invocation with a Windows path containing
/// backslashes `\` would not compile correctly on Unix.
///
/// `include_value!` is ignorant of byte order. For byte order-aware types, see
/// the [`byteorder`] module.
///
/// # Examples
///
/// Assume there are two files in the same directory with the following
/// contents:
///
/// File `data` (no trailing newline):
///
/// ```text
/// abcd
/// ```
///
/// File `main.rs`:
///
/// ```rust
/// use zerocopy::include_value;
/// # macro_rules! include_value {
/// # ($file:expr) => { zerocopy::include_value!(concat!("../testdata/include_value/", $file)) };
/// # }
///
/// fn main() {
///     let as_u32: u32 = include_value!("data");
///     assert_eq!(as_u32, u32::from_ne_bytes([b'a', b'b', b'c', b'd']));
///     let as_i32: i32 = include_value!("data");
///     assert_eq!(as_i32, i32::from_ne_bytes([b'a', b'b', b'c', b'd']));
/// }
/// ```
#[doc(alias("include_bytes", "include_data", "include_type"))]
#[macro_export]
macro_rules! include_value {
    ($file:expr $(,)?) => {
        $crate::transmute!(*::core::include_bytes!($file))
    };
}

/// A typed reference derived from a byte slice.
///
/// A `Ref<B, T>` is a reference to a `T` which is stored in a byte slice, `B`.
/// Unlike a native reference (`&T` or `&mut T`), `Ref<B, T>` has the same
/// mutability as the byte slice it was constructed from (`B`).
///
/// # Examples
///
/// `Ref` can be used to treat a sequence of bytes as a structured type, and to
/// read and write the fields of that type as if the byte slice reference were
/// simply a reference to that type.
///
/// ```rust
/// # #[cfg(feature = "derive")] { // This example uses derives, and won't compile without them
/// use zerocopy::{AsBytes, ByteSlice, ByteSliceMut, FromBytes, FromZeroes, Ref, Unaligned};
///
/// #[derive(FromZeroes, FromBytes, AsBytes, Unaligned)]
/// #[repr(C)]
/// struct UdpHeader {
///     src_port: [u8; 2],
///     dst_port: [u8; 2],
///     length: [u8; 2],
///     checksum: [u8; 2],
/// }
///
/// struct UdpPacket<B> {
///     header: Ref<B, UdpHeader>,
///     body: B,
/// }
///
/// impl<B: ByteSlice> UdpPacket<B> {
///     pub fn parse(bytes: B) -> Option<UdpPacket<B>> {
///         let (header, body) = Ref::new_unaligned_from_prefix(bytes)?;
///         Some(UdpPacket { header, body })
///     }
///
///     pub fn get_src_port(&self) -> [u8; 2] {
///         self.header.src_port
///     }
/// }
///
/// impl<B: ByteSliceMut> UdpPacket<B> {
///     pub fn set_src_port(&mut self, src_port: [u8; 2]) {
///         self.header.src_port = src_port;
///     }
/// }
/// # }
/// ```
pub struct Ref<B, T: ?Sized>(B, PhantomData<T>);

/// Deprecated: prefer [`Ref`] instead.
#[deprecated(since = "0.7.0", note = "LayoutVerified has been renamed to Ref")]
#[doc(hidden)]
pub type LayoutVerified<B, T> = Ref<B, T>;

impl<B, T> Ref<B, T>
where
    B: ByteSlice,
{
    /// Constructs a new `Ref`.
    ///
    /// `new` verifies that `bytes.len() == size_of::<T>()` and that `bytes` is
    /// aligned to `align_of::<T>()`, and constructs a new `Ref`. If either of
    /// these checks fail, it returns `None`.
    #[inline]
    pub fn new(bytes: B) -> Option<Ref<B, T>> {
        if bytes.len() != mem::size_of::<T>() || !util::aligned_to::<_, T>(bytes.deref()) {
            return None;
        }
        Some(Ref(bytes, PhantomData))
    }

    /// Constructs a new `Ref` from the prefix of a byte slice.
    ///
    /// `new_from_prefix` verifies that `bytes.len() >= size_of::<T>()` and that
    /// `bytes` is aligned to `align_of::<T>()`. It consumes the first
    /// `size_of::<T>()` bytes from `bytes` to construct a `Ref`, and returns
    /// the remaining bytes to the caller. If either the length or alignment
    /// checks fail, it returns `None`.
    #[inline]
    pub fn new_from_prefix(bytes: B) -> Option<(Ref<B, T>, B)> {
        if bytes.len() < mem::size_of::<T>() || !util::aligned_to::<_, T>(bytes.deref()) {
            return None;
        }
        let (bytes, suffix) = bytes.split_at(mem::size_of::<T>());
        Some((Ref(bytes, PhantomData), suffix))
    }

    /// Constructs a new `Ref` from the suffix of a byte slice.
    ///
    /// `new_from_suffix` verifies that `bytes.len() >= size_of::<T>()` and that
    /// the last `size_of::<T>()` bytes of `bytes` are aligned to
    /// `align_of::<T>()`. It consumes the last `size_of::<T>()` bytes from
    /// `bytes` to construct a `Ref`, and returns the preceding bytes to the
    /// caller. If either the length or alignment checks fail, it returns
    /// `None`.
    #[inline]
    pub fn new_from_suffix(bytes: B) -> Option<(B, Ref<B, T>)> {
        let bytes_len = bytes.len();
        let split_at = bytes_len.checked_sub(mem::size_of::<T>())?;
        let (prefix, bytes) = bytes.split_at(split_at);
        if !util::aligned_to::<_, T>(bytes.deref()) {
            return None;
        }
        Some((prefix, Ref(bytes, PhantomData)))
    }
}

impl<B, T> Ref<B, [T]>
where
    B: ByteSlice,
{
    /// Constructs a new `Ref` of a slice type.
    ///
    /// `new_slice` verifies that `bytes.len()` is a multiple of
    /// `size_of::<T>()` and that `bytes` is aligned to `align_of::<T>()`, and
    /// constructs a new `Ref`. If either of these checks fail, it returns
    /// `None`.
    ///
    /// # Panics
    ///
    /// `new_slice` panics if `T` is a zero-sized type.
    #[inline]
    pub fn new_slice(bytes: B) -> Option<Ref<B, [T]>> {
        let remainder = bytes
            .len()
            .checked_rem(mem::size_of::<T>())
            .expect("Ref::new_slice called on a zero-sized type");
        if remainder != 0 || !util::aligned_to::<_, T>(bytes.deref()) {
            return None;
        }
        Some(Ref(bytes, PhantomData))
    }

    /// Constructs a new `Ref` of a slice type from the prefix of a byte slice.
    ///
    /// `new_slice_from_prefix` verifies that `bytes.len() >= size_of::<T>() *
    /// count` and that `bytes` is aligned to `align_of::<T>()`. It consumes the
    /// first `size_of::<T>() * count` bytes from `bytes` to construct a `Ref`,
    /// and returns the remaining bytes to the caller. It also ensures that
    /// `sizeof::<T>() * count` does not overflow a `usize`. If any of the
    /// length, alignment, or overflow checks fail, it returns `None`.
    ///
    /// # Panics
    ///
    /// `new_slice_from_prefix` panics if `T` is a zero-sized type.
    #[inline]
    pub fn new_slice_from_prefix(bytes: B, count: usize) -> Option<(Ref<B, [T]>, B)> {
        let expected_len = match mem::size_of::<T>().checked_mul(count) {
            Some(len) => len,
            None => return None,
        };
        if bytes.len() < expected_len {
            return None;
        }
        let (prefix, bytes) = bytes.split_at(expected_len);
        Self::new_slice(prefix).map(move |l| (l, bytes))
    }

    /// Constructs a new `Ref` of a slice type from the suffix of a byte slice.
    ///
    /// `new_slice_from_suffix` verifies that `bytes.len() >= size_of::<T>() *
    /// count` and that `bytes` is aligned to `align_of::<T>()`. It consumes the
    /// last `size_of::<T>() * count` bytes from `bytes` to construct a `Ref`,
    /// and returns the preceding bytes to the caller. It also ensures that
    /// `sizeof::<T>() * count` does not overflow a `usize`. If any of the
    /// length, alignment, or overflow checks fail, it returns `None`.
    ///
    /// # Panics
    ///
    /// `new_slice_from_suffix` panics if `T` is a zero-sized type.
    #[inline]
    pub fn new_slice_from_suffix(bytes: B, count: usize) -> Option<(B, Ref<B, [T]>)> {
        let expected_len = match mem::size_of::<T>().checked_mul(count) {
            Some(len) => len,
            None => return None,
        };
        let split_at = bytes.len().checked_sub(expected_len)?;
        let (bytes, suffix) = bytes.split_at(split_at);
        Self::new_slice(suffix).map(move |l| (bytes, l))
    }
}

fn map_zeroed<B: ByteSliceMut, T: ?Sized>(opt: Option<Ref<B, T>>) -> Option<Ref<B, T>> {
    match opt {
        Some(mut r) => {
            r.0.fill(0);
            Some(r)
        }
        None => None,
    }
}

fn map_prefix_tuple_zeroed<B: ByteSliceMut, T: ?Sized>(
    opt: Option<(Ref<B, T>, B)>,
) -> Option<(Ref<B, T>, B)> {
    match opt {
        Some((mut r, rest)) => {
            r.0.fill(0);
            Some((r, rest))
        }
        None => None,
    }
}

fn map_suffix_tuple_zeroed<B: ByteSliceMut, T: ?Sized>(
    opt: Option<(B, Ref<B, T>)>,
) -> Option<(B, Ref<B, T>)> {
    map_prefix_tuple_zeroed(opt.map(|(a, b)| (b, a))).map(|(a, b)| (b, a))
}

impl<B, T> Ref<B, T>
where
    B: ByteSliceMut,
{
    /// Constructs a new `Ref` after zeroing the bytes.
    ///
    /// `new_zeroed` verifies that `bytes.len() == size_of::<T>()` and that
    /// `bytes` is aligned to `align_of::<T>()`, and constructs a new `Ref`. If
    /// either of these checks fail, it returns `None`.
    ///
    /// If the checks succeed, then `bytes` will be initialized to zero. This
    /// can be useful when re-using buffers to ensure that sensitive data
    /// previously stored in the buffer is not leaked.
    #[inline(always)]
    pub fn new_zeroed(bytes: B) -> Option<Ref<B, T>> {
        map_zeroed(Self::new(bytes))
    }

    /// Constructs a new `Ref` from the prefix of a byte slice, zeroing the
    /// prefix.
    ///
    /// `new_from_prefix_zeroed` verifies that `bytes.len() >= size_of::<T>()`
    /// and that `bytes` is aligned to `align_of::<T>()`. It consumes the first
    /// `size_of::<T>()` bytes from `bytes` to construct a `Ref`, and returns
    /// the remaining bytes to the caller. If either the length or alignment
    /// checks fail, it returns `None`.
    ///
    /// If the checks succeed, then the prefix which is consumed will be
    /// initialized to zero. This can be useful when re-using buffers to ensure
    /// that sensitive data previously stored in the buffer is not leaked.
    #[inline(always)]
    pub fn new_from_prefix_zeroed(bytes: B) -> Option<(Ref<B, T>, B)> {
        map_prefix_tuple_zeroed(Self::new_from_prefix(bytes))
    }

    /// Constructs a new `Ref` from the suffix of a byte slice, zeroing the
    /// suffix.
    ///
    /// `new_from_suffix_zeroed` verifies that `bytes.len() >= size_of::<T>()`
    /// and that the last `size_of::<T>()` bytes of `bytes` are aligned to
    /// `align_of::<T>()`. It consumes the last `size_of::<T>()` bytes from
    /// `bytes` to construct a `Ref`, and returns the preceding bytes to the
    /// caller. If either the length or alignment checks fail, it returns
    /// `None`.
    ///
    /// If the checks succeed, then the suffix which is consumed will be
    /// initialized to zero. This can be useful when re-using buffers to ensure
    /// that sensitive data previously stored in the buffer is not leaked.
    #[inline(always)]
    pub fn new_from_suffix_zeroed(bytes: B) -> Option<(B, Ref<B, T>)> {
        map_suffix_tuple_zeroed(Self::new_from_suffix(bytes))
    }
}

impl<B, T> Ref<B, [T]>
where
    B: ByteSliceMut,
{
    /// Constructs a new `Ref` of a slice type after zeroing the bytes.
    ///
    /// `new_slice_zeroed` verifies that `bytes.len()` is a multiple of
    /// `size_of::<T>()` and that `bytes` is aligned to `align_of::<T>()`, and
    /// constructs a new `Ref`. If either of these checks fail, it returns
    /// `None`.
    ///
    /// If the checks succeed, then `bytes` will be initialized to zero. This
    /// can be useful when re-using buffers to ensure that sensitive data
    /// previously stored in the buffer is not leaked.
    ///
    /// # Panics
    ///
    /// `new_slice` panics if `T` is a zero-sized type.
    #[inline(always)]
    pub fn new_slice_zeroed(bytes: B) -> Option<Ref<B, [T]>> {
        map_zeroed(Self::new_slice(bytes))
    }

    /// Constructs a new `Ref` of a slice type from the prefix of a byte slice,
    /// after zeroing the bytes.
    ///
    /// `new_slice_from_prefix` verifies that `bytes.len() >= size_of::<T>() *
    /// count` and that `bytes` is aligned to `align_of::<T>()`. It consumes the
    /// first `size_of::<T>() * count` bytes from `bytes` to construct a `Ref`,
    /// and returns the remaining bytes to the caller. It also ensures that
    /// `sizeof::<T>() * count` does not overflow a `usize`. If any of the
    /// length, alignment, or overflow checks fail, it returns `None`.
    ///
    /// If the checks succeed, then the suffix which is consumed will be
    /// initialized to zero. This can be useful when re-using buffers to ensure
    /// that sensitive data previously stored in the buffer is not leaked.
    ///
    /// # Panics
    ///
    /// `new_slice_from_prefix_zeroed` panics if `T` is a zero-sized type.
    #[inline(always)]
    pub fn new_slice_from_prefix_zeroed(bytes: B, count: usize) -> Option<(Ref<B, [T]>, B)> {
        map_prefix_tuple_zeroed(Self::new_slice_from_prefix(bytes, count))
    }

    /// Constructs a new `Ref` of a slice type from the prefix of a byte slice,
    /// after zeroing the bytes.
    ///
    /// `new_slice_from_suffix` verifies that `bytes.len() >= size_of::<T>() *
    /// count` and that `bytes` is aligned to `align_of::<T>()`. It consumes the
    /// last `size_of::<T>() * count` bytes from `bytes` to construct a `Ref`,
    /// and returns the preceding bytes to the caller. It also ensures that
    /// `sizeof::<T>() * count` does not overflow a `usize`. If any of the
    /// length, alignment, or overflow checks fail, it returns `None`.
    ///
    /// If the checks succeed, then the consumed suffix will be initialized to
    /// zero. This can be useful when re-using buffers to ensure that sensitive
    /// data previously stored in the buffer is not leaked.
    ///
    /// # Panics
    ///
    /// `new_slice_from_suffix_zeroed` panics if `T` is a zero-sized type.
    #[inline(always)]
    pub fn new_slice_from_suffix_zeroed(bytes: B, count: usize) -> Option<(B, Ref<B, [T]>)> {
        map_suffix_tuple_zeroed(Self::new_slice_from_suffix(bytes, count))
    }
}

impl<B, T> Ref<B, T>
where
    B: ByteSlice,
    T: Unaligned,
{
    /// Constructs a new `Ref` for a type with no alignment requirement.
    ///
    /// `new_unaligned` verifies that `bytes.len() == size_of::<T>()` and
    /// constructs a new `Ref`. If the check fails, it returns `None`.
    #[inline(always)]
    pub fn new_unaligned(bytes: B) -> Option<Ref<B, T>> {
        Ref::new(bytes)
    }

    /// Constructs a new `Ref` from the prefix of a byte slice for a type with
    /// no alignment requirement.
    ///
    /// `new_unaligned_from_prefix` verifies that `bytes.len() >=
    /// size_of::<T>()`. It consumes the first `size_of::<T>()` bytes from
    /// `bytes` to construct a `Ref`, and returns the remaining bytes to the
    /// caller. If the length check fails, it returns `None`.
    #[inline(always)]
    pub fn new_unaligned_from_prefix(bytes: B) -> Option<(Ref<B, T>, B)> {
        Ref::new_from_prefix(bytes)
    }

    /// Constructs a new `Ref` from the suffix of a byte slice for a type with
    /// no alignment requirement.
    ///
    /// `new_unaligned_from_suffix` verifies that `bytes.len() >=
    /// size_of::<T>()`. It consumes the last `size_of::<T>()` bytes from
    /// `bytes` to construct a `Ref`, and returns the preceding bytes to the
    /// caller. If the length check fails, it returns `None`.
    #[inline(always)]
    pub fn new_unaligned_from_suffix(bytes: B) -> Option<(B, Ref<B, T>)> {
        Ref::new_from_suffix(bytes)
    }
}

impl<B, T> Ref<B, [T]>
where
    B: ByteSlice,
    T: Unaligned,
{
    /// Constructs a new `Ref` of a slice type with no alignment requirement.
    ///
    /// `new_slice_unaligned` verifies that `bytes.len()` is a multiple of
    /// `size_of::<T>()` and constructs a new `Ref`. If the check fails, it
    /// returns `None`.
    ///
    /// # Panics
    ///
    /// `new_slice` panics if `T` is a zero-sized type.
    #[inline(always)]
    pub fn new_slice_unaligned(bytes: B) -> Option<Ref<B, [T]>> {
        Ref::new_slice(bytes)
    }

    /// Constructs a new `Ref` of a slice type with no alignment requirement
    /// from the prefix of a byte slice.
    ///
    /// `new_slice_from_prefix` verifies that `bytes.len() >= size_of::<T>() *
    /// count`. It consumes the first `size_of::<T>() * count` bytes from
    /// `bytes` to construct a `Ref`, and returns the remaining bytes to the
    /// caller. It also ensures that `sizeof::<T>() * count` does not overflow a
    /// `usize`. If either the length, or overflow checks fail, it returns
    /// `None`.
    ///
    /// # Panics
    ///
    /// `new_slice_unaligned_from_prefix` panics if `T` is a zero-sized type.
    #[inline(always)]
    pub fn new_slice_unaligned_from_prefix(bytes: B, count: usize) -> Option<(Ref<B, [T]>, B)> {
        Ref::new_slice_from_prefix(bytes, count)
    }

    /// Constructs a new `Ref` of a slice type with no alignment requirement
    /// from the suffix of a byte slice.
    ///
    /// `new_slice_from_suffix` verifies that `bytes.len() >= size_of::<T>() *
    /// count`. It consumes the last `size_of::<T>() * count` bytes from `bytes`
    /// to construct a `Ref`, and returns the remaining bytes to the caller. It
    /// also ensures that `sizeof::<T>() * count` does not overflow a `usize`.
    /// If either the length, or overflow checks fail, it returns `None`.
    ///
    /// # Panics
    ///
    /// `new_slice_unaligned_from_suffix` panics if `T` is a zero-sized type.
    #[inline(always)]
    pub fn new_slice_unaligned_from_suffix(bytes: B, count: usize) -> Option<(B, Ref<B, [T]>)> {
        Ref::new_slice_from_suffix(bytes, count)
    }
}

impl<B, T> Ref<B, T>
where
    B: ByteSliceMut,
    T: Unaligned,
{
    /// Constructs a new `Ref` for a type with no alignment requirement, zeroing
    /// the bytes.
    ///
    /// `new_unaligned_zeroed` verifies that `bytes.len() == size_of::<T>()` and
    /// constructs a new `Ref`. If the check fails, it returns `None`.
    ///
    /// If the check succeeds, then `bytes` will be initialized to zero. This
    /// can be useful when re-using buffers to ensure that sensitive data
    /// previously stored in the buffer is not leaked.
    #[inline(always)]
    pub fn new_unaligned_zeroed(bytes: B) -> Option<Ref<B, T>> {
        map_zeroed(Self::new_unaligned(bytes))
    }

    /// Constructs a new `Ref` from the prefix of a byte slice for a type with
    /// no alignment requirement, zeroing the prefix.
    ///
    /// `new_unaligned_from_prefix_zeroed` verifies that `bytes.len() >=
    /// size_of::<T>()`. It consumes the first `size_of::<T>()` bytes from
    /// `bytes` to construct a `Ref`, and returns the remaining bytes to the
    /// caller. If the length check fails, it returns `None`.
    ///
    /// If the check succeeds, then the prefix which is consumed will be
    /// initialized to zero. This can be useful when re-using buffers to ensure
    /// that sensitive data previously stored in the buffer is not leaked.
    #[inline(always)]
    pub fn new_unaligned_from_prefix_zeroed(bytes: B) -> Option<(Ref<B, T>, B)> {
        map_prefix_tuple_zeroed(Self::new_unaligned_from_prefix(bytes))
    }

    /// Constructs a new `Ref` from the suffix of a byte slice for a type with
    /// no alignment requirement, zeroing the suffix.
    ///
    /// `new_unaligned_from_suffix_zeroed` verifies that `bytes.len() >=
    /// size_of::<T>()`. It consumes the last `size_of::<T>()` bytes from
    /// `bytes` to construct a `Ref`, and returns the preceding bytes to the
    /// caller. If the length check fails, it returns `None`.
    ///
    /// If the check succeeds, then the suffix which is consumed will be
    /// initialized to zero. This can be useful when re-using buffers to ensure
    /// that sensitive data previously stored in the buffer is not leaked.
    #[inline(always)]
    pub fn new_unaligned_from_suffix_zeroed(bytes: B) -> Option<(B, Ref<B, T>)> {
        map_suffix_tuple_zeroed(Self::new_unaligned_from_suffix(bytes))
    }
}

impl<B, T> Ref<B, [T]>
where
    B: ByteSliceMut,
    T: Unaligned,
{
    /// Constructs a new `Ref` for a slice type with no alignment requirement,
    /// zeroing the bytes.
    ///
    /// `new_slice_unaligned_zeroed` verifies that `bytes.len()` is a multiple
    /// of `size_of::<T>()` and constructs a new `Ref`. If the check fails, it
    /// returns `None`.
    ///
    /// If the check succeeds, then `bytes` will be initialized to zero. This
    /// can be useful when re-using buffers to ensure that sensitive data
    /// previously stored in the buffer is not leaked.
    ///
    /// # Panics
    ///
    /// `new_slice` panics if `T` is a zero-sized type.
    #[inline(always)]
    pub fn new_slice_unaligned_zeroed(bytes: B) -> Option<Ref<B, [T]>> {
        map_zeroed(Self::new_slice_unaligned(bytes))
    }

    /// Constructs a new `Ref` of a slice type with no alignment requirement
    /// from the prefix of a byte slice, after zeroing the bytes.
    ///
    /// `new_slice_from_prefix` verifies that `bytes.len() >= size_of::<T>() *
    /// count`. It consumes the first `size_of::<T>() * count` bytes from
    /// `bytes` to construct a `Ref`, and returns the remaining bytes to the
    /// caller. It also ensures that `sizeof::<T>() * count` does not overflow a
    /// `usize`. If either the length, or overflow checks fail, it returns
    /// `None`.
    ///
    /// If the checks succeed, then the prefix will be initialized to zero. This
    /// can be useful when re-using buffers to ensure that sensitive data
    /// previously stored in the buffer is not leaked.
    ///
    /// # Panics
    ///
    /// `new_slice_unaligned_from_prefix_zeroed` panics if `T` is a zero-sized
    /// type.
    #[inline(always)]
    pub fn new_slice_unaligned_from_prefix_zeroed(
        bytes: B,
        count: usize,
    ) -> Option<(Ref<B, [T]>, B)> {
        map_prefix_tuple_zeroed(Self::new_slice_unaligned_from_prefix(bytes, count))
    }

    /// Constructs a new `Ref` of a slice type with no alignment requirement
    /// from the suffix of a byte slice, after zeroing the bytes.
    ///
    /// `new_slice_from_suffix` verifies that `bytes.len() >= size_of::<T>() *
    /// count`. It consumes the last `size_of::<T>() * count` bytes from `bytes`
    /// to construct a `Ref`, and returns the remaining bytes to the caller. It
    /// also ensures that `sizeof::<T>() * count` does not overflow a `usize`.
    /// If either the length, or overflow checks fail, it returns `None`.
    ///
    /// If the checks succeed, then the suffix will be initialized to zero. This
    /// can be useful when re-using buffers to ensure that sensitive data
    /// previously stored in the buffer is not leaked.
    ///
    /// # Panics
    ///
    /// `new_slice_unaligned_from_suffix_zeroed` panics if `T` is a zero-sized
    /// type.
    #[inline(always)]
    pub fn new_slice_unaligned_from_suffix_zeroed(
        bytes: B,
        count: usize,
    ) -> Option<(B, Ref<B, [T]>)> {
        map_suffix_tuple_zeroed(Self::new_slice_unaligned_from_suffix(bytes, count))
    }
}

impl<'a, B, T> Ref<B, T>
where
    B: 'a + ByteSlice,
    T: FromBytes,
{
    /// Converts this `Ref` into a reference.
    ///
    /// `into_ref` consumes the `Ref`, and returns a reference to `T`.
    #[inline(always)]
    pub fn into_ref(self) -> &'a T {
        assert!(B::INTO_REF_INTO_MUT_ARE_SOUND);

        // SAFETY: According to the safety preconditions on
        // `ByteSlice::INTO_REF_INTO_MUT_ARE_SOUND`, the preceding assert
        // ensures that, given `B: 'a`, it is sound to drop `self` and still
        // access the underlying memory using reads for `'a`.
        unsafe { self.deref_helper() }
    }
}

impl<'a, B, T> Ref<B, T>
where
    B: 'a + ByteSliceMut,
    T: FromBytes + AsBytes,
{
    /// Converts this `Ref` into a mutable reference.
    ///
    /// `into_mut` consumes the `Ref`, and returns a mutable reference to `T`.
    #[inline(always)]
    pub fn into_mut(mut self) -> &'a mut T {
        assert!(B::INTO_REF_INTO_MUT_ARE_SOUND);

        // SAFETY: According to the safety preconditions on
        // `ByteSlice::INTO_REF_INTO_MUT_ARE_SOUND`, the preceding assert
        // ensures that, given `B: 'a + ByteSliceMut`, it is sound to drop
        // `self` and still access the underlying memory using both reads and
        // writes for `'a`.
        unsafe { self.deref_mut_helper() }
    }
}

impl<'a, B, T> Ref<B, [T]>
where
    B: 'a + ByteSlice,
    T: FromBytes,
{
    /// Converts this `Ref` into a slice reference.
    ///
    /// `into_slice` consumes the `Ref`, and returns a reference to `[T]`.
    #[inline(always)]
    pub fn into_slice(self) -> &'a [T] {
        assert!(B::INTO_REF_INTO_MUT_ARE_SOUND);

        // SAFETY: According to the safety preconditions on
        // `ByteSlice::INTO_REF_INTO_MUT_ARE_SOUND`, the preceding assert
        // ensures that, given `B: 'a`, it is sound to drop `self` and still
        // access the underlying memory using reads for `'a`.
        unsafe { self.deref_slice_helper() }
    }
}

impl<'a, B, T> Ref<B, [T]>
where
    B: 'a + ByteSliceMut,
    T: FromBytes + AsBytes,
{
    /// Converts this `Ref` into a mutable slice reference.
    ///
    /// `into_mut_slice` consumes the `Ref`, and returns a mutable reference to
    /// `[T]`.
    #[inline(always)]
    pub fn into_mut_slice(mut self) -> &'a mut [T] {
        assert!(B::INTO_REF_INTO_MUT_ARE_SOUND);

        // SAFETY: According to the safety preconditions on
        // `ByteSlice::INTO_REF_INTO_MUT_ARE_SOUND`, the preceding assert
        // ensures that, given `B: 'a + ByteSliceMut`, it is sound to drop
        // `self` and still access the underlying memory using both reads and
        // writes for `'a`.
        unsafe { self.deref_mut_slice_helper() }
    }
}

impl<B, T> Ref<B, T>
where
    B: ByteSlice,
    T: FromBytes,
{
    /// Creates an immutable reference to `T` with a specific lifetime.
    ///
    /// # Safety
    ///
    /// The type bounds on this method guarantee that it is safe to create an
    /// immutable reference to `T` from `self`. However, since the lifetime `'a`
    /// is not required to be shorter than the lifetime of the reference to
    /// `self`, the caller must guarantee that the lifetime `'a` is valid for
    /// this reference. In particular, the referent must exist for all of `'a`,
    /// and no mutable references to the same memory may be constructed during
    /// `'a`.
    unsafe fn deref_helper<'a>(&self) -> &'a T {
        // TODO(#429): Add a "SAFETY" comment and remove this `allow`.
        #[allow(clippy::undocumented_unsafe_blocks)]
        unsafe {
            &*self.0.as_ptr().cast::<T>()
        }
    }
}

impl<B, T> Ref<B, T>
where
    B: ByteSliceMut,
    T: FromBytes + AsBytes,
{
    /// Creates a mutable reference to `T` with a specific lifetime.
    ///
    /// # Safety
    ///
    /// The type bounds on this method guarantee that it is safe to create a
    /// mutable reference to `T` from `self`. However, since the lifetime `'a`
    /// is not required to be shorter than the lifetime of the reference to
    /// `self`, the caller must guarantee that the lifetime `'a` is valid for
    /// this reference. In particular, the referent must exist for all of `'a`,
    /// and no other references - mutable or immutable - to the same memory may
    /// be constructed during `'a`.
    unsafe fn deref_mut_helper<'a>(&mut self) -> &'a mut T {
        // TODO(#429): Add a "SAFETY" comment and remove this `allow`.
        #[allow(clippy::undocumented_unsafe_blocks)]
        unsafe {
            &mut *self.0.as_mut_ptr().cast::<T>()
        }
    }
}

impl<B, T> Ref<B, [T]>
where
    B: ByteSlice,
    T: FromBytes,
{
    /// Creates an immutable reference to `[T]` with a specific lifetime.
    ///
    /// # Safety
    ///
    /// `deref_slice_helper` has the same safety requirements as `deref_helper`.
    unsafe fn deref_slice_helper<'a>(&self) -> &'a [T] {
        let len = self.0.len();
        let elem_size = mem::size_of::<T>();
        debug_assert_ne!(elem_size, 0);
        // `Ref<_, [T]>` maintains the invariant that `size_of::<T>() > 0`.
        // Thus, neither the mod nor division operations here can panic.
        #[allow(clippy::arithmetic_side_effects)]
        let elems = {
            debug_assert_eq!(len % elem_size, 0);
            len / elem_size
        };
        // TODO(#429): Add a "SAFETY" comment and remove this `allow`.
        #[allow(clippy::undocumented_unsafe_blocks)]
        unsafe {
            slice::from_raw_parts(self.0.as_ptr().cast::<T>(), elems)
        }
    }
}

impl<B, T> Ref<B, [T]>
where
    B: ByteSliceMut,
    T: FromBytes + AsBytes,
{
    /// Creates a mutable reference to `[T]` with a specific lifetime.
    ///
    /// # Safety
    ///
    /// `deref_mut_slice_helper` has the same safety requirements as
    /// `deref_mut_helper`.
    unsafe fn deref_mut_slice_helper<'a>(&mut self) -> &'a mut [T] {
        let len = self.0.len();
        let elem_size = mem::size_of::<T>();
        debug_assert_ne!(elem_size, 0);
        // `Ref<_, [T]>` maintains the invariant that `size_of::<T>() > 0`.
        // Thus, neither the mod nor division operations here can panic.
        #[allow(clippy::arithmetic_side_effects)]
        let elems = {
            debug_assert_eq!(len % elem_size, 0);
            len / elem_size
        };
        // TODO(#429): Add a "SAFETY" comment and remove this `allow`.
        #[allow(clippy::undocumented_unsafe_blocks)]
        unsafe {
            slice::from_raw_parts_mut(self.0.as_mut_ptr().cast::<T>(), elems)
        }
    }
}

impl<B, T> Ref<B, T>
where
    B: ByteSlice,
    T: ?Sized,
{
    /// Gets the underlying bytes.
    #[inline]
    pub fn bytes(&self) -> &[u8] {
        &self.0
    }
}

impl<B, T> Ref<B, T>
where
    B: ByteSliceMut,
    T: ?Sized,
{
    /// Gets the underlying bytes mutably.
    #[inline]
    pub fn bytes_mut(&mut self) -> &mut [u8] {
        &mut self.0
    }
}

impl<B, T> Ref<B, T>
where
    B: ByteSlice,
    T: FromBytes,
{
    /// Reads a copy of `T`.
    #[inline]
    pub fn read(&self) -> T {
        // SAFETY: Because of the invariants on `Ref`, we know that `self.0` is
        // at least `size_of::<T>()` bytes long, and that it is at least as
        // aligned as `align_of::<T>()`. Because `T: FromBytes`, it is sound to
        // interpret these bytes as a `T`.
        unsafe { ptr::read(self.0.as_ptr().cast::<T>()) }
    }
}

impl<B, T> Ref<B, T>
where
    B: ByteSliceMut,
    T: AsBytes,
{
    /// Writes the bytes of `t` and then forgets `t`.
    #[inline]
    pub fn write(&mut self, t: T) {
        // SAFETY: Because of the invariants on `Ref`, we know that `self.0` is
        // at least `size_of::<T>()` bytes long, and that it is at least as
        // aligned as `align_of::<T>()`. Writing `t` to the buffer will allow
        // all of the bytes of `t` to be accessed as a `[u8]`, but because `T:
        // AsBytes`, we know this is sound.
        unsafe { ptr::write(self.0.as_mut_ptr().cast::<T>(), t) }
    }
}

impl<B, T> Deref for Ref<B, T>
where
    B: ByteSlice,
    T: FromBytes,
{
    type Target = T;
    #[inline]
    fn deref(&self) -> &T {
        // SAFETY: This is sound because the lifetime of `self` is the same as
        // the lifetime of the return value, meaning that a) the returned
        // reference cannot outlive `self` and, b) no mutable methods on `self`
        // can be called during the lifetime of the returned reference. See the
        // documentation on `deref_helper` for what invariants we are required
        // to uphold.
        unsafe { self.deref_helper() }
    }
}

impl<B, T> DerefMut for Ref<B, T>
where
    B: ByteSliceMut,
    T: FromBytes + AsBytes,
{
    #[inline]
    fn deref_mut(&mut self) -> &mut T {
        // SAFETY: This is sound because the lifetime of `self` is the same as
        // the lifetime of the return value, meaning that a) the returned
        // reference cannot outlive `self` and, b) no other methods on `self`
        // can be called during the lifetime of the returned reference. See the
        // documentation on `deref_mut_helper` for what invariants we are
        // required to uphold.
        unsafe { self.deref_mut_helper() }
    }
}

impl<B, T> Deref for Ref<B, [T]>
where
    B: ByteSlice,
    T: FromBytes,
{
    type Target = [T];
    #[inline]
    fn deref(&self) -> &[T] {
        // SAFETY: This is sound because the lifetime of `self` is the same as
        // the lifetime of the return value, meaning that a) the returned
        // reference cannot outlive `self` and, b) no mutable methods on `self`
        // can be called during the lifetime of the returned reference. See the
        // documentation on `deref_slice_helper` for what invariants we are
        // required to uphold.
        unsafe { self.deref_slice_helper() }
    }
}

impl<B, T> DerefMut for Ref<B, [T]>
where
    B: ByteSliceMut,
    T: FromBytes + AsBytes,
{
    #[inline]
    fn deref_mut(&mut self) -> &mut [T] {
        // SAFETY: This is sound because the lifetime of `self` is the same as
        // the lifetime of the return value, meaning that a) the returned
        // reference cannot outlive `self` and, b) no other methods on `self`
        // can be called during the lifetime of the returned reference. See the
        // documentation on `deref_mut_slice_helper` for what invariants we are
        // required to uphold.
        unsafe { self.deref_mut_slice_helper() }
    }
}

impl<T, B> Display for Ref<B, T>
where
    B: ByteSlice,
    T: FromBytes + Display,
{
    #[inline]
    fn fmt(&self, fmt: &mut Formatter<'_>) -> fmt::Result {
        let inner: &T = self;
        inner.fmt(fmt)
    }
}

impl<T, B> Display for Ref<B, [T]>
where
    B: ByteSlice,
    T: FromBytes,
    [T]: Display,
{
    #[inline]
    fn fmt(&self, fmt: &mut Formatter<'_>) -> fmt::Result {
        let inner: &[T] = self;
        inner.fmt(fmt)
    }
}

impl<T, B> Debug for Ref<B, T>
where
    B: ByteSlice,
    T: FromBytes + Debug,
{
    #[inline]
    fn fmt(&self, fmt: &mut Formatter<'_>) -> fmt::Result {
        let inner: &T = self;
        fmt.debug_tuple("Ref").field(&inner).finish()
    }
}

impl<T, B> Debug for Ref<B, [T]>
where
    B: ByteSlice,
    T: FromBytes + Debug,
{
    #[inline]
    fn fmt(&self, fmt: &mut Formatter<'_>) -> fmt::Result {
        let inner: &[T] = self;
        fmt.debug_tuple("Ref").field(&inner).finish()
    }
}

impl<T, B> Eq for Ref<B, T>
where
    B: ByteSlice,
    T: FromBytes + Eq,
{
}

impl<T, B> Eq for Ref<B, [T]>
where
    B: ByteSlice,
    T: FromBytes + Eq,
{
}

impl<T, B> PartialEq for Ref<B, T>
where
    B: ByteSlice,
    T: FromBytes + PartialEq,
{
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        self.deref().eq(other.deref())
    }
}

impl<T, B> PartialEq for Ref<B, [T]>
where
    B: ByteSlice,
    T: FromBytes + PartialEq,
{
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        self.deref().eq(other.deref())
    }
}

impl<T, B> Ord for Ref<B, T>
where
    B: ByteSlice,
    T: FromBytes + Ord,
{
    #[inline]
    fn cmp(&self, other: &Self) -> Ordering {
        let inner: &T = self;
        let other_inner: &T = other;
        inner.cmp(other_inner)
    }
}

impl<T, B> Ord for Ref<B, [T]>
where
    B: ByteSlice,
    T: FromBytes + Ord,
{
    #[inline]
    fn cmp(&self, other: &Self) -> Ordering {
        let inner: &[T] = self;
        let other_inner: &[T] = other;
        inner.cmp(other_inner)
    }
}

impl<T, B> PartialOrd for Ref<B, T>
where
    B: ByteSlice,
    T: FromBytes + PartialOrd,
{
    #[inline]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        let inner: &T = self;
        let other_inner: &T = other;
        inner.partial_cmp(other_inner)
    }
}

impl<T, B> PartialOrd for Ref<B, [T]>
where
    B: ByteSlice,
    T: FromBytes + PartialOrd,
{
    #[inline]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        let inner: &[T] = self;
        let other_inner: &[T] = other;
        inner.partial_cmp(other_inner)
    }
}

mod sealed {
    pub trait ByteSliceSealed {}
}

// ByteSlice and ByteSliceMut abstract over [u8] references (&[u8], &mut [u8],
// Ref<[u8]>, RefMut<[u8]>, etc). We rely on various behaviors of these
// references such as that a given reference will never changes its length
// between calls to deref() or deref_mut(), and that split_at() works as
// expected. If ByteSlice or ByteSliceMut were not sealed, consumers could
// implement them in a way that violated these behaviors, and would break our
// unsafe code. Thus, we seal them and implement it only for known-good
// reference types. For the same reason, they're unsafe traits.

#[allow(clippy::missing_safety_doc)] // TODO(fxbug.dev/99068)
/// A mutable or immutable reference to a byte slice.
///
/// `ByteSlice` abstracts over the mutability of a byte slice reference, and is
/// implemented for various special reference types such as `Ref<[u8]>` and
/// `RefMut<[u8]>`.
///
/// Note that, while it would be technically possible, `ByteSlice` is not
/// implemented for [`Vec<u8>`], as the only way to implement the [`split_at`]
/// method would involve reallocation, and `split_at` must be a very cheap
/// operation in order for the utilities in this crate to perform as designed.
///
/// [`split_at`]: crate::ByteSlice::split_at
// It may seem overkill to go to this length to ensure that this doc link never
// breaks. We do this because it simplifies CI - it means that generating docs
// always succeeds, so we don't need special logic to only generate docs under
// certain features.
#[cfg_attr(feature = "alloc", doc = "[`Vec<u8>`]: alloc::vec::Vec")]
#[cfg_attr(
    not(feature = "alloc"),
    doc = "[`Vec<u8>`]: https://doc.rust-lang.org/std/vec/struct.Vec.html"
)]
pub unsafe trait ByteSlice: Deref<Target = [u8]> + Sized + sealed::ByteSliceSealed {
    /// Are the [`Ref::into_ref`] and [`Ref::into_mut`] methods sound when used
    /// with `Self`? If not, evaluating this constant must panic at compile
    /// time.
    ///
    /// This exists to work around #716 on versions of zerocopy prior to 0.8.
    ///
    /// # Safety
    ///
    /// This may only be set to true if the following holds: Given the
    /// following:
    /// - `Self: 'a`
    /// - `bytes: Self`
    /// - `let ptr = bytes.as_ptr()`
    ///
    /// ...then:
    /// - Using `ptr` to read the memory previously addressed by `bytes` is
    ///   sound for `'a` even after `bytes` has been dropped.
    /// - If `Self: ByteSliceMut`, using `ptr` to write the memory previously
    ///   addressed by `bytes` is sound for `'a` even after `bytes` has been
    ///   dropped.
    #[doc(hidden)]
    const INTO_REF_INTO_MUT_ARE_SOUND: bool;

    /// Gets a raw pointer to the first byte in the slice.
    #[inline]
    fn as_ptr(&self) -> *const u8 {
        <[u8]>::as_ptr(self)
    }

    /// Splits the slice at the midpoint.
    ///
    /// `x.split_at(mid)` returns `x[..mid]` and `x[mid..]`.
    ///
    /// # Panics
    ///
    /// `x.split_at(mid)` panics if `mid > x.len()`.
    fn split_at(self, mid: usize) -> (Self, Self);
}

#[allow(clippy::missing_safety_doc)] // TODO(fxbug.dev/99068)
/// A mutable reference to a byte slice.
///
/// `ByteSliceMut` abstracts over various ways of storing a mutable reference to
/// a byte slice, and is implemented for various special reference types such as
/// `RefMut<[u8]>`.
pub unsafe trait ByteSliceMut: ByteSlice + DerefMut {
    /// Gets a mutable raw pointer to the first byte in the slice.
    #[inline]
    fn as_mut_ptr(&mut self) -> *mut u8 {
        <[u8]>::as_mut_ptr(self)
    }
}

impl<'a> sealed::ByteSliceSealed for &'a [u8] {}
// TODO(#429): Add a "SAFETY" comment and remove this `allow`.
#[allow(clippy::undocumented_unsafe_blocks)]
unsafe impl<'a> ByteSlice for &'a [u8] {
    // SAFETY: If `&'b [u8]: 'a`, then the underlying memory is treated as
    // borrowed immutably for `'a` even if the slice itself is dropped.
    const INTO_REF_INTO_MUT_ARE_SOUND: bool = true;

    #[inline]
    fn split_at(self, mid: usize) -> (Self, Self) {
        <[u8]>::split_at(self, mid)
    }
}

impl<'a> sealed::ByteSliceSealed for &'a mut [u8] {}
// TODO(#429): Add a "SAFETY" comment and remove this `allow`.
#[allow(clippy::undocumented_unsafe_blocks)]
unsafe impl<'a> ByteSlice for &'a mut [u8] {
    // SAFETY: If `&'b mut [u8]: 'a`, then the underlying memory is treated as
    // borrowed mutably for `'a` even if the slice itself is dropped.
    const INTO_REF_INTO_MUT_ARE_SOUND: bool = true;

    #[inline]
    fn split_at(self, mid: usize) -> (Self, Self) {
        <[u8]>::split_at_mut(self, mid)
    }
}

impl<'a> sealed::ByteSliceSealed for cell::Ref<'a, [u8]> {}
// TODO(#429): Add a "SAFETY" comment and remove this `allow`.
#[allow(clippy::undocumented_unsafe_blocks)]
unsafe impl<'a> ByteSlice for cell::Ref<'a, [u8]> {
    const INTO_REF_INTO_MUT_ARE_SOUND: bool = if !cfg!(doc) {
        panic!("Ref::into_ref and Ref::into_mut are unsound when used with core::cell::Ref; see https://github.com/google/zerocopy/issues/716")
    } else {
        // When compiling documentation, allow the evaluation of this constant
        // to succeed. This doesn't represent a soundness hole - it just delays
        // any error to runtime. The reason we need this is that, otherwise,
        // `rustdoc` will fail when trying to document this item.
        false
    };

    #[inline]
    fn split_at(self, mid: usize) -> (Self, Self) {
        cell::Ref::map_split(self, |slice| <[u8]>::split_at(slice, mid))
    }
}

impl<'a> sealed::ByteSliceSealed for RefMut<'a, [u8]> {}
// TODO(#429): Add a "SAFETY" comment and remove this `allow`.
#[allow(clippy::undocumented_unsafe_blocks)]
unsafe impl<'a> ByteSlice for RefMut<'a, [u8]> {
    const INTO_REF_INTO_MUT_ARE_SOUND: bool = if !cfg!(doc) {
        panic!("Ref::into_ref and Ref::into_mut are unsound when used with core::cell::RefMut; see https://github.com/google/zerocopy/issues/716")
    } else {
        // When compiling documentation, allow the evaluation of this constant
        // to succeed. This doesn't represent a soundness hole - it just delays
        // any error to runtime. The reason we need this is that, otherwise,
        // `rustdoc` will fail when trying to document this item.
        false
    };

    #[inline]
    fn split_at(self, mid: usize) -> (Self, Self) {
        RefMut::map_split(self, |slice| <[u8]>::split_at_mut(slice, mid))
    }
}

// TODO(#429): Add a "SAFETY" comment and remove this `allow`.
#[allow(clippy::undocumented_unsafe_blocks)]
unsafe impl<'a> ByteSliceMut for &'a mut [u8] {}

// TODO(#429): Add a "SAFETY" comment and remove this `allow`.
#[allow(clippy::undocumented_unsafe_blocks)]
unsafe impl<'a> ByteSliceMut for RefMut<'a, [u8]> {}

#[cfg(feature = "alloc")]
#[cfg_attr(doc_cfg, doc(cfg(feature = "alloc")))]
mod alloc_support {
    use alloc::vec::Vec;

    use super::*;

    /// Extends a `Vec<T>` by pushing `additional` new items onto the end of the
    /// vector. The new items are initialized with zeroes.
    ///
    /// # Panics
    ///
    /// Panics if `Vec::reserve(additional)` fails to reserve enough memory.
    #[inline(always)]
    pub fn extend_vec_zeroed<T: FromZeroes>(v: &mut Vec<T>, additional: usize) {
        insert_vec_zeroed(v, v.len(), additional);
    }

    /// Inserts `additional` new items into `Vec<T>` at `position`.
    /// The new items are initialized with zeroes.
    ///
    /// # Panics
    ///
    /// * Panics if `position > v.len()`.
    /// * Panics if `Vec::reserve(additional)` fails to reserve enough memory.
    #[inline]
    pub fn insert_vec_zeroed<T: FromZeroes>(v: &mut Vec<T>, position: usize, additional: usize) {
        assert!(position <= v.len());
        v.reserve(additional);
        // SAFETY: The `reserve` call guarantees that these cannot overflow:
        // * `ptr.add(position)`
        // * `position + additional`
        // * `v.len() + additional`
        //
        // `v.len() - position` cannot overflow because we asserted that
        // `position <= v.len()`.
        unsafe {
            // This is a potentially overlapping copy.
            let ptr = v.as_mut_ptr();
            #[allow(clippy::arithmetic_side_effects)]
            ptr.add(position).copy_to(ptr.add(position + additional), v.len() - position);
            ptr.add(position).write_bytes(0, additional);
            #[allow(clippy::arithmetic_side_effects)]
            v.set_len(v.len() + additional);
        }
    }

    #[cfg(test)]
    mod tests {
        use core::convert::TryFrom as _;

        use super::*;

        #[test]
        fn test_extend_vec_zeroed() {
            // Test extending when there is an existing allocation.
            let mut v = vec![100u64, 200, 300];
            extend_vec_zeroed(&mut v, 3);
            assert_eq!(v.len(), 6);
            assert_eq!(&*v, &[100, 200, 300, 0, 0, 0]);
            drop(v);

            // Test extending when there is no existing allocation.
            let mut v: Vec<u64> = Vec::new();
            extend_vec_zeroed(&mut v, 3);
            assert_eq!(v.len(), 3);
            assert_eq!(&*v, &[0, 0, 0]);
            drop(v);
        }

        #[test]
        fn test_extend_vec_zeroed_zst() {
            // Test extending when there is an existing (fake) allocation.
            let mut v = vec![(), (), ()];
            extend_vec_zeroed(&mut v, 3);
            assert_eq!(v.len(), 6);
            assert_eq!(&*v, &[(), (), (), (), (), ()]);
            drop(v);

            // Test extending when there is no existing (fake) allocation.
            let mut v: Vec<()> = Vec::new();
            extend_vec_zeroed(&mut v, 3);
            assert_eq!(&*v, &[(), (), ()]);
            drop(v);
        }

        #[test]
        fn test_insert_vec_zeroed() {
            // Insert at start (no existing allocation).
            let mut v: Vec<u64> = Vec::new();
            insert_vec_zeroed(&mut v, 0, 2);
            assert_eq!(v.len(), 2);
            assert_eq!(&*v, &[0, 0]);
            drop(v);

            // Insert at start.
            let mut v = vec![100u64, 200, 300];
            insert_vec_zeroed(&mut v, 0, 2);
            assert_eq!(v.len(), 5);
            assert_eq!(&*v, &[0, 0, 100, 200, 300]);
            drop(v);

            // Insert at middle.
            let mut v = vec![100u64, 200, 300];
            insert_vec_zeroed(&mut v, 1, 1);
            assert_eq!(v.len(), 4);
            assert_eq!(&*v, &[100, 0, 200, 300]);
            drop(v);

            // Insert at end.
            let mut v = vec![100u64, 200, 300];
            insert_vec_zeroed(&mut v, 3, 1);
            assert_eq!(v.len(), 4);
            assert_eq!(&*v, &[100, 200, 300, 0]);
            drop(v);
        }

        #[test]
        fn test_insert_vec_zeroed_zst() {
            // Insert at start (no existing fake allocation).
            let mut v: Vec<()> = Vec::new();
            insert_vec_zeroed(&mut v, 0, 2);
            assert_eq!(v.len(), 2);
            assert_eq!(&*v, &[(), ()]);
            drop(v);

            // Insert at start.
            let mut v = vec![(), (), ()];
            insert_vec_zeroed(&mut v, 0, 2);
            assert_eq!(v.len(), 5);
            assert_eq!(&*v, &[(), (), (), (), ()]);
            drop(v);

            // Insert at middle.
            let mut v = vec![(), (), ()];
            insert_vec_zeroed(&mut v, 1, 1);
            assert_eq!(v.len(), 4);
            assert_eq!(&*v, &[(), (), (), ()]);
            drop(v);

            // Insert at end.
            let mut v = vec![(), (), ()];
            insert_vec_zeroed(&mut v, 3, 1);
            assert_eq!(v.len(), 4);
            assert_eq!(&*v, &[(), (), (), ()]);
            drop(v);
        }

        #[test]
        fn test_new_box_zeroed() {
            assert_eq!(*u64::new_box_zeroed(), 0);
        }

        #[test]
        fn test_new_box_zeroed_array() {
            drop(<[u32; 0x1000]>::new_box_zeroed());
        }

        #[test]
        fn test_new_box_zeroed_zst() {
            // This test exists in order to exercise unsafe code, especially
            // when running under Miri.
            #[allow(clippy::unit_cmp)]
            {
                assert_eq!(*<()>::new_box_zeroed(), ());
            }
        }

        #[test]
        fn test_new_box_slice_zeroed() {
            let mut s: Box<[u64]> = u64::new_box_slice_zeroed(3);
            assert_eq!(s.len(), 3);
            assert_eq!(&*s, &[0, 0, 0]);
            s[1] = 3;
            assert_eq!(&*s, &[0, 3, 0]);
        }

        #[test]
        fn test_new_box_slice_zeroed_empty() {
            let s: Box<[u64]> = u64::new_box_slice_zeroed(0);
            assert_eq!(s.len(), 0);
        }

        #[test]
        fn test_new_box_slice_zeroed_zst() {
            let mut s: Box<[()]> = <()>::new_box_slice_zeroed(3);
            assert_eq!(s.len(), 3);
            assert!(s.get(10).is_none());
            // This test exists in order to exercise unsafe code, especially
            // when running under Miri.
            #[allow(clippy::unit_cmp)]
            {
                assert_eq!(s[1], ());
            }
            s[2] = ();
        }

        #[test]
        fn test_new_box_slice_zeroed_zst_empty() {
            let s: Box<[()]> = <()>::new_box_slice_zeroed(0);
            assert_eq!(s.len(), 0);
        }

        #[test]
        #[should_panic(expected = "mem::size_of::<Self>() * len overflows `usize`")]
        fn test_new_box_slice_zeroed_panics_mul_overflow() {
            let _ = u16::new_box_slice_zeroed(usize::MAX);
        }

        #[test]
        #[should_panic(expected = "assertion failed: size <= max_alloc")]
        fn test_new_box_slice_zeroed_panics_isize_overflow() {
            let max = usize::try_from(isize::MAX).unwrap();
            let _ = u16::new_box_slice_zeroed((max / mem::size_of::<u16>()) + 1);
        }
    }
}

#[cfg(feature = "alloc")]
#[doc(inline)]
pub use alloc_support::*;

#[cfg(test)]
mod tests {
    #![allow(clippy::unreadable_literal)]

    use core::{cell::UnsafeCell, convert::TryInto as _, ops::Deref};

    use static_assertions::assert_impl_all;

    use super::*;
    use crate::util::testutil::*;

    // An unsized type.
    //
    // This is used to test the custom derives of our traits. The `[u8]` type
    // gets a hand-rolled impl, so it doesn't exercise our custom derives.
    #[derive(Debug, Eq, PartialEq, FromZeroes, FromBytes, AsBytes, Unaligned)]
    #[repr(transparent)]
    struct Unsized([u8]);

    impl Unsized {
        fn from_mut_slice(slc: &mut [u8]) -> &mut Unsized {
            // SAFETY: This *probably* sound - since the layouts of `[u8]` and
            // `Unsized` are the same, so are the layouts of `&mut [u8]` and
            // `&mut Unsized`. [1] Even if it turns out that this isn't actually
            // guaranteed by the language spec, we can just change this since
            // it's in test code.
            //
            // [1] https://github.com/rust-lang/unsafe-code-guidelines/issues/375
            unsafe { mem::transmute(slc) }
        }
    }

    /// Tests of when a sized `DstLayout` is extended with a sized field.
    #[allow(clippy::decimal_literal_representation)]
    #[test]
    fn test_dst_layout_extend_sized_with_sized() {
        // This macro constructs a layout corresponding to a `u8` and extends it
        // with a zero-sized trailing field of given alignment `n`. The macro
        // tests that the resulting layout has both size and alignment `min(n,
        // P)` for all valid values of `repr(packed(P))`.
        macro_rules! test_align_is_size {
            ($n:expr) => {
                let base = DstLayout::for_type::<u8>();
                let trailing_field = DstLayout::for_type::<elain::Align<$n>>();

                let packs =
                    core::iter::once(None).chain((0..29).map(|p| NonZeroUsize::new(2usize.pow(p))));

                for pack in packs {
                    let composite = base.extend(trailing_field, pack);
                    let max_align = pack.unwrap_or(DstLayout::CURRENT_MAX_ALIGN);
                    let align = $n.min(max_align.get());
                    assert_eq!(
                        composite,
                        DstLayout {
                            align: NonZeroUsize::new(align).unwrap(),
                            size_info: SizeInfo::Sized { _size: align }
                        }
                    )
                }
            };
        }

        test_align_is_size!(1);
        test_align_is_size!(2);
        test_align_is_size!(4);
        test_align_is_size!(8);
        test_align_is_size!(16);
        test_align_is_size!(32);
        test_align_is_size!(64);
        test_align_is_size!(128);
        test_align_is_size!(256);
        test_align_is_size!(512);
        test_align_is_size!(1024);
        test_align_is_size!(2048);
        test_align_is_size!(4096);
        test_align_is_size!(8192);
        test_align_is_size!(16384);
        test_align_is_size!(32768);
        test_align_is_size!(65536);
        test_align_is_size!(131072);
        test_align_is_size!(262144);
        test_align_is_size!(524288);
        test_align_is_size!(1048576);
        test_align_is_size!(2097152);
        test_align_is_size!(4194304);
        test_align_is_size!(8388608);
        test_align_is_size!(16777216);
        test_align_is_size!(33554432);
        test_align_is_size!(67108864);
        test_align_is_size!(33554432);
        test_align_is_size!(134217728);
        test_align_is_size!(268435456);
    }

    /// Tests of when a sized `DstLayout` is extended with a DST field.
    #[test]
    fn test_dst_layout_extend_sized_with_dst() {
        // Test that for all combinations of real-world alignments and
        // `repr_packed` values, that the extension of a sized `DstLayout`` with
        // a DST field correctly computes the trailing offset in the composite
        // layout.

        let aligns = (0..29).map(|p| NonZeroUsize::new(2usize.pow(p)).unwrap());
        let packs = core::iter::once(None).chain(aligns.clone().map(Some));

        for align in aligns {
            for pack in packs.clone() {
                let base = DstLayout::for_type::<u8>();
                let elem_size = 42;
                let trailing_field_offset = 11;

                let trailing_field = DstLayout {
                    align,
                    size_info: SizeInfo::SliceDst(TrailingSliceLayout {
                        _elem_size: elem_size,
                        _offset: 11,
                    }),
                };

                let composite = base.extend(trailing_field, pack);

                let max_align = pack.unwrap_or(DstLayout::CURRENT_MAX_ALIGN).get();

                let align = align.get().min(max_align);

                assert_eq!(
                    composite,
                    DstLayout {
                        align: NonZeroUsize::new(align).unwrap(),
                        size_info: SizeInfo::SliceDst(TrailingSliceLayout {
                            _elem_size: elem_size,
                            _offset: align + trailing_field_offset,
                        }),
                    }
                )
            }
        }
    }

    /// Tests that calling `pad_to_align` on a sized `DstLayout` adds the
    /// expected amount of trailing padding.
    #[test]
    fn test_dst_layout_pad_to_align_with_sized() {
        // For all valid alignments `align`, construct a one-byte layout aligned
        // to `align`, call `pad_to_align`, and assert that the size of the
        // resulting layout is equal to `align`.
        for align in (0..29).map(|p| NonZeroUsize::new(2usize.pow(p)).unwrap()) {
            let layout = DstLayout { align, size_info: SizeInfo::Sized { _size: 1 } };

            assert_eq!(
                layout.pad_to_align(),
                DstLayout { align, size_info: SizeInfo::Sized { _size: align.get() } }
            );
        }

        // Test explicitly-provided combinations of unpadded and padded
        // counterparts.

        macro_rules! test {
            (unpadded { size: $unpadded_size:expr, align: $unpadded_align:expr }
                => padded { size: $padded_size:expr, align: $padded_align:expr }) => {
                let unpadded = DstLayout {
                    align: NonZeroUsize::new($unpadded_align).unwrap(),
                    size_info: SizeInfo::Sized { _size: $unpadded_size },
                };
                let padded = unpadded.pad_to_align();

                assert_eq!(
                    padded,
                    DstLayout {
                        align: NonZeroUsize::new($padded_align).unwrap(),
                        size_info: SizeInfo::Sized { _size: $padded_size },
                    }
                );
            };
        }

        test!(unpadded { size: 0, align: 4 } => padded { size: 0, align: 4 });
        test!(unpadded { size: 1, align: 4 } => padded { size: 4, align: 4 });
        test!(unpadded { size: 2, align: 4 } => padded { size: 4, align: 4 });
        test!(unpadded { size: 3, align: 4 } => padded { size: 4, align: 4 });
        test!(unpadded { size: 4, align: 4 } => padded { size: 4, align: 4 });
        test!(unpadded { size: 5, align: 4 } => padded { size: 8, align: 4 });
        test!(unpadded { size: 6, align: 4 } => padded { size: 8, align: 4 });
        test!(unpadded { size: 7, align: 4 } => padded { size: 8, align: 4 });
        test!(unpadded { size: 8, align: 4 } => padded { size: 8, align: 4 });

        let current_max_align = DstLayout::CURRENT_MAX_ALIGN.get();

        test!(unpadded { size: 1, align: current_max_align }
            => padded { size: current_max_align, align: current_max_align });

        test!(unpadded { size: current_max_align + 1, align: current_max_align }
            => padded { size: current_max_align * 2, align: current_max_align });
    }

    /// Tests that calling `pad_to_align` on a DST `DstLayout` is a no-op.
    #[test]
    fn test_dst_layout_pad_to_align_with_dst() {
        for align in (0..29).map(|p| NonZeroUsize::new(2usize.pow(p)).unwrap()) {
            for offset in 0..10 {
                for elem_size in 0..10 {
                    let layout = DstLayout {
                        align,
                        size_info: SizeInfo::SliceDst(TrailingSliceLayout {
                            _offset: offset,
                            _elem_size: elem_size,
                        }),
                    };
                    assert_eq!(layout.pad_to_align(), layout);
                }
            }
        }
    }

    // This test takes a long time when running under Miri, so we skip it in
    // that case. This is acceptable because this is a logic test that doesn't
    // attempt to expose UB.
    #[test]
    #[cfg_attr(miri, ignore)]
    fn testvalidate_cast_and_convert_metadata() {
        impl From<usize> for SizeInfo {
            fn from(_size: usize) -> SizeInfo {
                SizeInfo::Sized { _size }
            }
        }

        impl From<(usize, usize)> for SizeInfo {
            fn from((_offset, _elem_size): (usize, usize)) -> SizeInfo {
                SizeInfo::SliceDst(TrailingSliceLayout { _offset, _elem_size })
            }
        }

        fn layout<S: Into<SizeInfo>>(s: S, align: usize) -> DstLayout {
            DstLayout { size_info: s.into(), align: NonZeroUsize::new(align).unwrap() }
        }

        /// This macro accepts arguments in the form of:
        ///
        ///           layout(_, _, _).validate(_, _, _), Ok(Some((_, _)))
        ///                  |  |  |           |  |  |            |  |
        ///    base_size ----+  |  |           |  |  |            |  |
        ///    align -----------+  |           |  |  |            |  |
        ///    trailing_size ------+           |  |  |            |  |
        ///    addr ---------------------------+  |  |            |  |
        ///    bytes_len -------------------------+  |            |  |
        ///    cast_type ----------------------------+            |  |
        ///    elems ---------------------------------------------+  |
        ///    split_at ---------------------------------------------+
        ///
        /// `.validate` is shorthand for `.validate_cast_and_convert_metadata`
        /// for brevity.
        ///
        /// Each argument can either be an iterator or a wildcard. Each
        /// wildcarded variable is implicitly replaced by an iterator over a
        /// representative sample of values for that variable. Each `test!`
        /// invocation iterates over every combination of values provided by
        /// each variable's iterator (ie, the cartesian product) and validates
        /// that the results are expected.
        ///
        /// The final argument uses the same syntax, but it has a different
        /// meaning:
        /// - If it is `Ok(pat)`, then the pattern `pat` is supplied to
        ///   `assert_matches!` to validate the computed result for each
        ///   combination of input values.
        /// - If it is `Err(msg)`, then `test!` validates that the call to
        ///   `validate_cast_and_convert_metadata` panics with the given panic
        ///   message.
        ///
        /// Note that the meta-variables that match these variables have the
        /// `tt` type, and some valid expressions are not valid `tt`s (such as
        /// `a..b`). In this case, wrap the expression in parentheses, and it
        /// will become valid `tt`.
        macro_rules! test {
            ($(:$sizes:expr =>)?
                layout($size:tt, $align:tt)
                .validate($addr:tt, $bytes_len:tt, $cast_type:tt), $expect:pat $(,)?
            ) => {
                itertools::iproduct!(
                    test!(@generate_size $size),
                    test!(@generate_align $align),
                    test!(@generate_usize $addr),
                    test!(@generate_usize $bytes_len),
                    test!(@generate_cast_type $cast_type)
                ).for_each(|(size_info, align, addr, bytes_len, cast_type)| {
                    // Temporarily disable the panic hook installed by the test
                    // harness. If we don't do this, all panic messages will be
                    // kept in an internal log. On its own, this isn't a
                    // problem, but if a non-caught panic ever happens (ie, in
                    // code later in this test not in this macro), all of the
                    // previously-buffered messages will be dumped, hiding the
                    // real culprit.
                    let previous_hook = std::panic::take_hook();
                    // I don't understand why, but this seems to be required in
                    // addition to the previous line.
                    std::panic::set_hook(Box::new(|_| {}));
                    let actual = std::panic::catch_unwind(|| {
                        layout(size_info, align).validate_cast_and_convert_metadata(addr, bytes_len, cast_type)
                    }).map_err(|d| {
                        *d.downcast::<&'static str>().expect("expected string panic message").as_ref()
                    });
                    std::panic::set_hook(previous_hook);

                    assert_matches::assert_matches!(
                        actual, $expect,
                        "layout({size_info:?}, {align}).validate_cast_and_convert_metadata({addr}, {bytes_len}, {cast_type:?})",
                    );
                });
            };
            (@generate_usize _) => { 0..8 };
            // Generate sizes for both Sized and !Sized types.
            (@generate_size _) => {
                test!(@generate_size (_)).chain(test!(@generate_size (_, _)))
            };
            // Generate sizes for both Sized and !Sized types by chaining
            // specified iterators for each.
            (@generate_size ($sized_sizes:tt | $unsized_sizes:tt)) => {
                test!(@generate_size ($sized_sizes)).chain(test!(@generate_size $unsized_sizes))
            };
            // Generate sizes for Sized types.
            (@generate_size (_)) => { test!(@generate_size (0..8)) };
            (@generate_size ($sizes:expr)) => { $sizes.into_iter().map(Into::<SizeInfo>::into) };
            // Generate sizes for !Sized types.
            (@generate_size ($min_sizes:tt, $elem_sizes:tt)) => {
                itertools::iproduct!(
                    test!(@generate_min_size $min_sizes),
                    test!(@generate_elem_size $elem_sizes)
                ).map(Into::<SizeInfo>::into)
            };
            (@generate_fixed_size _) => { (0..8).into_iter().map(Into::<SizeInfo>::into) };
            (@generate_min_size _) => { 0..8 };
            (@generate_elem_size _) => { 1..8 };
            (@generate_align _) => { [1, 2, 4, 8, 16] };
            (@generate_opt_usize _) => { [None].into_iter().chain((0..8).map(Some).into_iter()) };
            (@generate_cast_type _) => { [_CastType::_Prefix, _CastType::_Suffix] };
            (@generate_cast_type $variant:ident) => { [_CastType::$variant] };
            // Some expressions need to be wrapped in parentheses in order to be
            // valid `tt`s (required by the top match pattern). See the comment
            // below for more details. This arm removes these parentheses to
            // avoid generating an `unused_parens` warning.
            (@$_:ident ($vals:expr)) => { $vals };
            (@$_:ident $vals:expr) => { $vals };
        }

        const EVENS: [usize; 8] = [0, 2, 4, 6, 8, 10, 12, 14];
        const ODDS: [usize; 8] = [1, 3, 5, 7, 9, 11, 13, 15];

        // base_size is too big for the memory region.
        test!(layout(((1..8) | ((1..8), (1..8))), _).validate(_, [0], _), Ok(None));
        test!(layout(((2..8) | ((2..8), (2..8))), _).validate(_, [1], _), Ok(None));

        // addr is unaligned for prefix cast
        test!(layout(_, [2]).validate(ODDS, _, _Prefix), Ok(None));
        test!(layout(_, [2]).validate(ODDS, _, _Prefix), Ok(None));

        // addr is aligned, but end of buffer is unaligned for suffix cast
        test!(layout(_, [2]).validate(EVENS, ODDS, _Suffix), Ok(None));
        test!(layout(_, [2]).validate(EVENS, ODDS, _Suffix), Ok(None));

        // Unfortunately, these constants cannot easily be used in the
        // implementation of `validate_cast_and_convert_metadata`, since
        // `panic!` consumes a string literal, not an expression.
        //
        // It's important that these messages be in a separate module. If they
        // were at the function's top level, we'd pass them to `test!` as, e.g.,
        // `Err(TRAILING)`, which would run into a subtle Rust footgun - the
        // `TRAILING` identifier would be treated as a pattern to match rather
        // than a value to check for equality.
        mod msgs {
            pub(super) const TRAILING: &str =
                "attempted to cast to slice type with zero-sized element";
            pub(super) const OVERFLOW: &str = "`addr` + `bytes_len` > usize::MAX";
        }

        // casts with ZST trailing element types are unsupported
        test!(layout((_, [0]), _).validate(_, _, _), Err(msgs::TRAILING),);

        // addr + bytes_len must not overflow usize
        test!(layout(_, _).validate([usize::MAX], (1..100), _), Err(msgs::OVERFLOW));
        test!(layout(_, _).validate((1..100), [usize::MAX], _), Err(msgs::OVERFLOW));
        test!(
            layout(_, _).validate(
                [usize::MAX / 2 + 1, usize::MAX],
                [usize::MAX / 2 + 1, usize::MAX],
                _
            ),
            Err(msgs::OVERFLOW)
        );

        // Validates that `validate_cast_and_convert_metadata` satisfies its own
        // documented safety postconditions, and also a few other properties
        // that aren't documented but we want to guarantee anyway.
        fn validate_behavior(
            (layout, addr, bytes_len, cast_type): (DstLayout, usize, usize, _CastType),
        ) {
            if let Some((elems, split_at)) =
                layout.validate_cast_and_convert_metadata(addr, bytes_len, cast_type)
            {
                let (size_info, align) = (layout.size_info, layout.align);
                let debug_str = format!(
                    "layout({size_info:?}, {align}).validate_cast_and_convert_metadata({addr}, {bytes_len}, {cast_type:?}) => ({elems}, {split_at})",
                );

                // If this is a sized type (no trailing slice), then `elems` is
                // meaningless, but in practice we set it to 0. Callers are not
                // allowed to rely on this, but a lot of math is nicer if
                // they're able to, and some callers might accidentally do that.
                let sized = matches!(layout.size_info, SizeInfo::Sized { .. });
                assert!(!(sized && elems != 0), "{}", debug_str);

                let resulting_size = match layout.size_info {
                    SizeInfo::Sized { _size } => _size,
                    SizeInfo::SliceDst(TrailingSliceLayout {
                        _offset: offset,
                        _elem_size: elem_size,
                    }) => {
                        let padded_size = |elems| {
                            let without_padding = offset + elems * elem_size;
                            without_padding
                                + util::core_layout::padding_needed_for(without_padding, align)
                        };

                        let resulting_size = padded_size(elems);
                        // Test that `validate_cast_and_convert_metadata`
                        // computed the largest possible value that fits in the
                        // given range.
                        assert!(padded_size(elems + 1) > bytes_len, "{}", debug_str);
                        resulting_size
                    }
                };

                // Test safety postconditions guaranteed by
                // `validate_cast_and_convert_metadata`.
                assert!(resulting_size <= bytes_len, "{}", debug_str);
                match cast_type {
                    _CastType::_Prefix => {
                        assert_eq!(addr % align, 0, "{}", debug_str);
                        assert_eq!(resulting_size, split_at, "{}", debug_str);
                    }
                    _CastType::_Suffix => {
                        assert_eq!(split_at, bytes_len - resulting_size, "{}", debug_str);
                        assert_eq!((addr + split_at) % align, 0, "{}", debug_str);
                    }
                }
            } else {
                let min_size = match layout.size_info {
                    SizeInfo::Sized { _size } => _size,
                    SizeInfo::SliceDst(TrailingSliceLayout { _offset, .. }) => {
                        _offset + util::core_layout::padding_needed_for(_offset, layout.align)
                    }
                };

                // If a cast is invalid, it is either because...
                // 1. there are insufficent bytes at the given region for type:
                let insufficient_bytes = bytes_len < min_size;
                // 2. performing the cast would misalign type:
                let base = match cast_type {
                    _CastType::_Prefix => 0,
                    _CastType::_Suffix => bytes_len,
                };
                let misaligned = (base + addr) % layout.align != 0;

                assert!(insufficient_bytes || misaligned);
            }
        }

        let sizes = 0..8;
        let elem_sizes = 1..8;
        let size_infos = sizes
            .clone()
            .map(Into::<SizeInfo>::into)
            .chain(itertools::iproduct!(sizes, elem_sizes).map(Into::<SizeInfo>::into));
        let layouts = itertools::iproduct!(size_infos, [1, 2, 4, 8, 16, 32])
            .filter(|(size_info, align)| !matches!(size_info, SizeInfo::Sized { _size } if _size % align != 0))
            .map(|(size_info, align)| layout(size_info, align));
        itertools::iproduct!(layouts, 0..8, 0..8, [_CastType::_Prefix, _CastType::_Suffix])
            .for_each(validate_behavior);
    }

    #[test]
    #[cfg(__INTERNAL_USE_ONLY_NIGHLTY_FEATURES_IN_TESTS)]
    fn test_validate_rust_layout() {
        use core::ptr::NonNull;

        // This test synthesizes pointers with various metadata and uses Rust's
        // built-in APIs to confirm that Rust makes decisions about type layout
        // which are consistent with what we believe is guaranteed by the
        // language. If this test fails, it doesn't just mean our code is wrong
        // - it means we're misunderstanding the language's guarantees.

        #[derive(Debug)]
        struct MacroArgs {
            offset: usize,
            align: NonZeroUsize,
            elem_size: Option<usize>,
        }

        /// # Safety
        ///
        /// `test` promises to only call `addr_of_slice_field` on a `NonNull<T>`
        /// which points to a valid `T`.
        ///
        /// `with_elems` must produce a pointer which points to a valid `T`.
        fn test<T: ?Sized, W: Fn(usize) -> NonNull<T>>(
            args: MacroArgs,
            with_elems: W,
            addr_of_slice_field: Option<fn(NonNull<T>) -> NonNull<u8>>,
        ) {
            let dst = args.elem_size.is_some();
            let layout = {
                let size_info = match args.elem_size {
                    Some(elem_size) => SizeInfo::SliceDst(TrailingSliceLayout {
                        _offset: args.offset,
                        _elem_size: elem_size,
                    }),
                    None => SizeInfo::Sized {
                        // Rust only supports types whose sizes are a multiple
                        // of their alignment. If the macro created a type like
                        // this:
                        //
                        //   #[repr(C, align(2))]
                        //   struct Foo([u8; 1]);
                        //
                        // ...then Rust will automatically round the type's size
                        // up to 2.
                        _size: args.offset
                            + util::core_layout::padding_needed_for(args.offset, args.align),
                    },
                };
                DstLayout { size_info, align: args.align }
            };

            for elems in 0..128 {
                let ptr = with_elems(elems);

                if let Some(addr_of_slice_field) = addr_of_slice_field {
                    let slc_field_ptr = addr_of_slice_field(ptr).as_ptr();
                    // SAFETY: Both `slc_field_ptr` and `ptr` are pointers to
                    // the same valid Rust object.
                    let offset: usize =
                        unsafe { slc_field_ptr.byte_offset_from(ptr.as_ptr()).try_into().unwrap() };
                    assert_eq!(offset, args.offset);
                }

                // SAFETY: `ptr` points to a valid `T`.
                let (size, align) = unsafe {
                    (mem::size_of_val_raw(ptr.as_ptr()), mem::align_of_val_raw(ptr.as_ptr()))
                };

                // Avoid expensive allocation when running under Miri.
                let assert_msg = if !cfg!(miri) {
                    format!("\n{args:?}\nsize:{size}, align:{align}")
                } else {
                    String::new()
                };

                let without_padding =
                    args.offset + args.elem_size.map(|elem_size| elems * elem_size).unwrap_or(0);
                assert!(size >= without_padding, "{}", assert_msg);
                assert_eq!(align, args.align.get(), "{}", assert_msg);

                // This encodes the most important part of the test: our
                // understanding of how Rust determines the layout of repr(C)
                // types. Sized repr(C) types are trivial, but DST types have
                // some subtlety. Note that:
                // - For sized types, `without_padding` is just the size of the
                //   type that we constructed for `Foo`. Since we may have
                //   requested a larger alignment, `Foo` may actually be larger
                //   than this, hence `padding_needed_for`.
                // - For unsized types, `without_padding` is dynamically
                //   computed from the offset, the element size, and element
                //   count. We expect that the size of the object should be
                //   `offset + elem_size * elems` rounded up to the next
                //   alignment.
                let expected_size = without_padding
                    + util::core_layout::padding_needed_for(without_padding, args.align);
                assert_eq!(expected_size, size, "{}", assert_msg);

                // For zero-sized element types,
                // `validate_cast_and_convert_metadata` just panics, so we skip
                // testing those types.
                if args.elem_size.map(|elem_size| elem_size > 0).unwrap_or(true) {
                    let addr = ptr.addr().get();
                    let (got_elems, got_split_at) = layout
                        .validate_cast_and_convert_metadata(addr, size, _CastType::_Prefix)
                        .unwrap();
                    // Avoid expensive allocation when running under Miri.
                    let assert_msg = if !cfg!(miri) {
                        format!(
                            "{}\nvalidate_cast_and_convert_metadata({addr}, {size})",
                            assert_msg
                        )
                    } else {
                        String::new()
                    };
                    assert_eq!(got_split_at, size, "{}", assert_msg);
                    if dst {
                        assert!(got_elems >= elems, "{}", assert_msg);
                        if got_elems != elems {
                            // If `validate_cast_and_convert_metadata`
                            // returned more elements than `elems`, that
                            // means that `elems` is not the maximum number
                            // of elements that can fit in `size` - in other
                            // words, there is enough padding at the end of
                            // the value to fit at least one more element.
                            // If we use this metadata to synthesize a
                            // pointer, despite having a different element
                            // count, we still expect it to have the same
                            // size.
                            let got_ptr = with_elems(got_elems);
                            // SAFETY: `got_ptr` is a pointer to a valid `T`.
                            let size_of_got_ptr = unsafe { mem::size_of_val_raw(got_ptr.as_ptr()) };
                            assert_eq!(size_of_got_ptr, size, "{}", assert_msg);
                        }
                    } else {
                        // For sized casts, the returned element value is
                        // technically meaningless, and we don't guarantee any
                        // particular value. In practice, it's always zero.
                        assert_eq!(got_elems, 0, "{}", assert_msg)
                    }
                }
            }
        }

        macro_rules! validate_against_rust {
            ($offset:literal, $align:literal $(, $elem_size:literal)?) => {{
                #[repr(C, align($align))]
                struct Foo([u8; $offset]$(, [[u8; $elem_size]])?);

                let args = MacroArgs {
                    offset: $offset,
                    align: $align.try_into().unwrap(),
                    elem_size: {
                        #[allow(unused)]
                        let ret = None::<usize>;
                        $(let ret = Some($elem_size);)?
                        ret
                    }
                };

                #[repr(C, align($align))]
                struct FooAlign;
                // Create an aligned buffer to use in order to synthesize
                // pointers to `Foo`. We don't ever load values from these
                // pointers - we just do arithmetic on them - so having a "real"
                // block of memory as opposed to a validly-aligned-but-dangling
                // pointer is only necessary to make Miri happy since we run it
                // with "strict provenance" checking enabled.
                let aligned_buf = Align::<_, FooAlign>::new([0u8; 1024]);
                let with_elems = |elems| {
                    let slc = NonNull::slice_from_raw_parts(NonNull::from(&aligned_buf.t), elems);
                    #[allow(clippy::as_conversions)]
                    NonNull::new(slc.as_ptr() as *mut Foo).unwrap()
                };
                let addr_of_slice_field = {
                    #[allow(unused)]
                    let f = None::<fn(NonNull<Foo>) -> NonNull<u8>>;
                    $(
                        // SAFETY: `test` promises to only call `f` with a `ptr`
                        // to a valid `Foo`.
                        let f: Option<fn(NonNull<Foo>) -> NonNull<u8>> = Some(|ptr: NonNull<Foo>| unsafe {
                            NonNull::new(ptr::addr_of_mut!((*ptr.as_ptr()).1)).unwrap().cast::<u8>()
                        });
                        let _ = $elem_size;
                    )?
                    f
                };

                test::<Foo, _>(args, with_elems, addr_of_slice_field);
            }};
        }

        // Every permutation of:
        // - offset in [0, 4]
        // - align in [1, 16]
        // - elem_size in [0, 4] (plus no elem_size)
        validate_against_rust!(0, 1);
        validate_against_rust!(0, 1, 0);
        validate_against_rust!(0, 1, 1);
        validate_against_rust!(0, 1, 2);
        validate_against_rust!(0, 1, 3);
        validate_against_rust!(0, 1, 4);
        validate_against_rust!(0, 2);
        validate_against_rust!(0, 2, 0);
        validate_against_rust!(0, 2, 1);
        validate_against_rust!(0, 2, 2);
        validate_against_rust!(0, 2, 3);
        validate_against_rust!(0, 2, 4);
        validate_against_rust!(0, 4);
        validate_against_rust!(0, 4, 0);
        validate_against_rust!(0, 4, 1);
        validate_against_rust!(0, 4, 2);
        validate_against_rust!(0, 4, 3);
        validate_against_rust!(0, 4, 4);
        validate_against_rust!(0, 8);
        validate_against_rust!(0, 8, 0);
        validate_against_rust!(0, 8, 1);
        validate_against_rust!(0, 8, 2);
        validate_against_rust!(0, 8, 3);
        validate_against_rust!(0, 8, 4);
        validate_against_rust!(0, 16);
        validate_against_rust!(0, 16, 0);
        validate_against_rust!(0, 16, 1);
        validate_against_rust!(0, 16, 2);
        validate_against_rust!(0, 16, 3);
        validate_against_rust!(0, 16, 4);
        validate_against_rust!(1, 1);
        validate_against_rust!(1, 1, 0);
        validate_against_rust!(1, 1, 1);
        validate_against_rust!(1, 1, 2);
        validate_against_rust!(1, 1, 3);
        validate_against_rust!(1, 1, 4);
        validate_against_rust!(1, 2);
        validate_against_rust!(1, 2, 0);
        validate_against_rust!(1, 2, 1);
        validate_against_rust!(1, 2, 2);
        validate_against_rust!(1, 2, 3);
        validate_against_rust!(1, 2, 4);
        validate_against_rust!(1, 4);
        validate_against_rust!(1, 4, 0);
        validate_against_rust!(1, 4, 1);
        validate_against_rust!(1, 4, 2);
        validate_against_rust!(1, 4, 3);
        validate_against_rust!(1, 4, 4);
        validate_against_rust!(1, 8);
        validate_against_rust!(1, 8, 0);
        validate_against_rust!(1, 8, 1);
        validate_against_rust!(1, 8, 2);
        validate_against_rust!(1, 8, 3);
        validate_against_rust!(1, 8, 4);
        validate_against_rust!(1, 16);
        validate_against_rust!(1, 16, 0);
        validate_against_rust!(1, 16, 1);
        validate_against_rust!(1, 16, 2);
        validate_against_rust!(1, 16, 3);
        validate_against_rust!(1, 16, 4);
        validate_against_rust!(2, 1);
        validate_against_rust!(2, 1, 0);
        validate_against_rust!(2, 1, 1);
        validate_against_rust!(2, 1, 2);
        validate_against_rust!(2, 1, 3);
        validate_against_rust!(2, 1, 4);
        validate_against_rust!(2, 2);
        validate_against_rust!(2, 2, 0);
        validate_against_rust!(2, 2, 1);
        validate_against_rust!(2, 2, 2);
        validate_against_rust!(2, 2, 3);
        validate_against_rust!(2, 2, 4);
        validate_against_rust!(2, 4);
        validate_against_rust!(2, 4, 0);
        validate_against_rust!(2, 4, 1);
        validate_against_rust!(2, 4, 2);
        validate_against_rust!(2, 4, 3);
        validate_against_rust!(2, 4, 4);
        validate_against_rust!(2, 8);
        validate_against_rust!(2, 8, 0);
        validate_against_rust!(2, 8, 1);
        validate_against_rust!(2, 8, 2);
        validate_against_rust!(2, 8, 3);
        validate_against_rust!(2, 8, 4);
        validate_against_rust!(2, 16);
        validate_against_rust!(2, 16, 0);
        validate_against_rust!(2, 16, 1);
        validate_against_rust!(2, 16, 2);
        validate_against_rust!(2, 16, 3);
        validate_against_rust!(2, 16, 4);
        validate_against_rust!(3, 1);
        validate_against_rust!(3, 1, 0);
        validate_against_rust!(3, 1, 1);
        validate_against_rust!(3, 1, 2);
        validate_against_rust!(3, 1, 3);
        validate_against_rust!(3, 1, 4);
        validate_against_rust!(3, 2);
        validate_against_rust!(3, 2, 0);
        validate_against_rust!(3, 2, 1);
        validate_against_rust!(3, 2, 2);
        validate_against_rust!(3, 2, 3);
        validate_against_rust!(3, 2, 4);
        validate_against_rust!(3, 4);
        validate_against_rust!(3, 4, 0);
        validate_against_rust!(3, 4, 1);
        validate_against_rust!(3, 4, 2);
        validate_against_rust!(3, 4, 3);
        validate_against_rust!(3, 4, 4);
        validate_against_rust!(3, 8);
        validate_against_rust!(3, 8, 0);
        validate_against_rust!(3, 8, 1);
        validate_against_rust!(3, 8, 2);
        validate_against_rust!(3, 8, 3);
        validate_against_rust!(3, 8, 4);
        validate_against_rust!(3, 16);
        validate_against_rust!(3, 16, 0);
        validate_against_rust!(3, 16, 1);
        validate_against_rust!(3, 16, 2);
        validate_against_rust!(3, 16, 3);
        validate_against_rust!(3, 16, 4);
        validate_against_rust!(4, 1);
        validate_against_rust!(4, 1, 0);
        validate_against_rust!(4, 1, 1);
        validate_against_rust!(4, 1, 2);
        validate_against_rust!(4, 1, 3);
        validate_against_rust!(4, 1, 4);
        validate_against_rust!(4, 2);
        validate_against_rust!(4, 2, 0);
        validate_against_rust!(4, 2, 1);
        validate_against_rust!(4, 2, 2);
        validate_against_rust!(4, 2, 3);
        validate_against_rust!(4, 2, 4);
        validate_against_rust!(4, 4);
        validate_against_rust!(4, 4, 0);
        validate_against_rust!(4, 4, 1);
        validate_against_rust!(4, 4, 2);
        validate_against_rust!(4, 4, 3);
        validate_against_rust!(4, 4, 4);
        validate_against_rust!(4, 8);
        validate_against_rust!(4, 8, 0);
        validate_against_rust!(4, 8, 1);
        validate_against_rust!(4, 8, 2);
        validate_against_rust!(4, 8, 3);
        validate_against_rust!(4, 8, 4);
        validate_against_rust!(4, 16);
        validate_against_rust!(4, 16, 0);
        validate_against_rust!(4, 16, 1);
        validate_against_rust!(4, 16, 2);
        validate_against_rust!(4, 16, 3);
        validate_against_rust!(4, 16, 4);
    }

    #[test]
    fn test_known_layout() {
        // Test that `$ty` and `ManuallyDrop<$ty>` have the expected layout.
        // Test that `PhantomData<$ty>` has the same layout as `()` regardless
        // of `$ty`.
        macro_rules! test {
            ($ty:ty, $expect:expr) => {
                let expect = $expect;
                assert_eq!(<$ty as KnownLayout>::LAYOUT, expect);
                assert_eq!(<ManuallyDrop<$ty> as KnownLayout>::LAYOUT, expect);
                assert_eq!(<PhantomData<$ty> as KnownLayout>::LAYOUT, <() as KnownLayout>::LAYOUT);
            };
        }

        let layout = |offset, align, _trailing_slice_elem_size| DstLayout {
            align: NonZeroUsize::new(align).unwrap(),
            size_info: match _trailing_slice_elem_size {
                None => SizeInfo::Sized { _size: offset },
                Some(elem_size) => SizeInfo::SliceDst(TrailingSliceLayout {
                    _offset: offset,
                    _elem_size: elem_size,
                }),
            },
        };

        test!((), layout(0, 1, None));
        test!(u8, layout(1, 1, None));
        // Use `align_of` because `u64` alignment may be smaller than 8 on some
        // platforms.
        test!(u64, layout(8, mem::align_of::<u64>(), None));
        test!(AU64, layout(8, 8, None));

        test!(Option<&'static ()>, usize::LAYOUT);

        test!([()], layout(0, 1, Some(0)));
        test!([u8], layout(0, 1, Some(1)));
        test!(str, layout(0, 1, Some(1)));
    }

    #[cfg(feature = "derive")]
    #[test]
    fn test_known_layout_derive() {
        // In this and other files (`late_compile_pass.rs`,
        // `mid_compile_pass.rs`, and `struct.rs`), we test success and failure
        // modes of `derive(KnownLayout)` for the following combination of
        // properties:
        //
        // +------------+--------------------------------------+-----------+
        // |            |      trailing field properties       |           |
        // | `repr(C)`? | generic? | `KnownLayout`? | `Sized`? | Type Name |
        // |------------+----------+----------------+----------+-----------|
        // |          N |        N |              N |        N |      KL00 |
        // |          N |        N |              N |        Y |      KL01 |
        // |          N |        N |              Y |        N |      KL02 |
        // |          N |        N |              Y |        Y |      KL03 |
        // |          N |        Y |              N |        N |      KL04 |
        // |          N |        Y |              N |        Y |      KL05 |
        // |          N |        Y |              Y |        N |      KL06 |
        // |          N |        Y |              Y |        Y |      KL07 |
        // |          Y |        N |              N |        N |      KL08 |
        // |          Y |        N |              N |        Y |      KL09 |
        // |          Y |        N |              Y |        N |      KL10 |
        // |          Y |        N |              Y |        Y |      KL11 |
        // |          Y |        Y |              N |        N |      KL12 |
        // |          Y |        Y |              N |        Y |      KL13 |
        // |          Y |        Y |              Y |        N |      KL14 |
        // |          Y |        Y |              Y |        Y |      KL15 |
        // +------------+----------+----------------+----------+-----------+

        struct NotKnownLayout<T = ()> {
            _t: T,
        }

        #[derive(KnownLayout)]
        #[repr(C)]
        struct AlignSize<const ALIGN: usize, const SIZE: usize>
        where
            elain::Align<ALIGN>: elain::Alignment,
        {
            _align: elain::Align<ALIGN>,
            _size: [u8; SIZE],
        }

        type AU16 = AlignSize<2, 2>;
        type AU32 = AlignSize<4, 4>;

        fn _assert_kl<T: ?Sized + KnownLayout>(_: &T) {}

        let sized_layout = |align, size| DstLayout {
            align: NonZeroUsize::new(align).unwrap(),
            size_info: SizeInfo::Sized { _size: size },
        };

        let unsized_layout = |align, elem_size, offset| DstLayout {
            align: NonZeroUsize::new(align).unwrap(),
            size_info: SizeInfo::SliceDst(TrailingSliceLayout {
                _offset: offset,
                _elem_size: elem_size,
            }),
        };

        // | `repr(C)`? | generic? | `KnownLayout`? | `Sized`? | Type Name |
        // |          N |        N |              N |        Y |      KL01 |
        #[derive(KnownLayout)]
        #[allow(dead_code)] // fields are never read
        struct KL01(NotKnownLayout<AU32>, NotKnownLayout<AU16>);

        let expected = DstLayout::for_type::<KL01>();

        assert_eq!(<KL01 as KnownLayout>::LAYOUT, expected);
        assert_eq!(<KL01 as KnownLayout>::LAYOUT, sized_layout(4, 8));

        // ...with `align(N)`:
        #[derive(KnownLayout)]
        #[repr(align(64))]
        #[allow(dead_code)] // fields are never read
        struct KL01Align(NotKnownLayout<AU32>, NotKnownLayout<AU16>);

        let expected = DstLayout::for_type::<KL01Align>();

        assert_eq!(<KL01Align as KnownLayout>::LAYOUT, expected);
        assert_eq!(<KL01Align as KnownLayout>::LAYOUT, sized_layout(64, 64));

        // ...with `packed`:
        #[derive(KnownLayout)]
        #[repr(packed)]
        #[allow(dead_code)] // fields are never read
        struct KL01Packed(NotKnownLayout<AU32>, NotKnownLayout<AU16>);

        let expected = DstLayout::for_type::<KL01Packed>();

        assert_eq!(<KL01Packed as KnownLayout>::LAYOUT, expected);
        assert_eq!(<KL01Packed as KnownLayout>::LAYOUT, sized_layout(1, 6));

        // ...with `packed(N)`:
        #[derive(KnownLayout)]
        #[repr(packed(2))]
        #[allow(dead_code)] // fields are never read
        struct KL01PackedN(NotKnownLayout<AU32>, NotKnownLayout<AU16>);

        assert_impl_all!(KL01PackedN: KnownLayout);

        let expected = DstLayout::for_type::<KL01PackedN>();

        assert_eq!(<KL01PackedN as KnownLayout>::LAYOUT, expected);
        assert_eq!(<KL01PackedN as KnownLayout>::LAYOUT, sized_layout(2, 6));

        // | `repr(C)`? | generic? | `KnownLayout`? | `Sized`? | Type Name |
        // |          N |        N |              Y |        Y |      KL03 |
        #[derive(KnownLayout)]
        #[allow(dead_code)] // fields are never read
        struct KL03(NotKnownLayout, u8);

        let expected = DstLayout::for_type::<KL03>();

        assert_eq!(<KL03 as KnownLayout>::LAYOUT, expected);
        assert_eq!(<KL03 as KnownLayout>::LAYOUT, sized_layout(1, 1));

        // ... with `align(N)`
        #[derive(KnownLayout)]
        #[repr(align(64))]
        #[allow(dead_code)] // fields are never read
        struct KL03Align(NotKnownLayout<AU32>, u8);

        let expected = DstLayout::for_type::<KL03Align>();

        assert_eq!(<KL03Align as KnownLayout>::LAYOUT, expected);
        assert_eq!(<KL03Align as KnownLayout>::LAYOUT, sized_layout(64, 64));

        // ... with `packed`:
        #[derive(KnownLayout)]
        #[repr(packed)]
        #[allow(dead_code)] // fields are never read
        struct KL03Packed(NotKnownLayout<AU32>, u8);

        let expected = DstLayout::for_type::<KL03Packed>();

        assert_eq!(<KL03Packed as KnownLayout>::LAYOUT, expected);
        assert_eq!(<KL03Packed as KnownLayout>::LAYOUT, sized_layout(1, 5));

        // ... with `packed(N)`
        #[derive(KnownLayout)]
        #[repr(packed(2))]
        #[allow(dead_code)] // fields are never read
        struct KL03PackedN(NotKnownLayout<AU32>, u8);

        assert_impl_all!(KL03PackedN: KnownLayout);

        let expected = DstLayout::for_type::<KL03PackedN>();

        assert_eq!(<KL03PackedN as KnownLayout>::LAYOUT, expected);
        assert_eq!(<KL03PackedN as KnownLayout>::LAYOUT, sized_layout(2, 6));

        // | `repr(C)`? | generic? | `KnownLayout`? | `Sized`? | Type Name |
        // |          N |        Y |              N |        Y |      KL05 |
        #[derive(KnownLayout)]
        #[allow(dead_code)] // fields are never read
        struct KL05<T>(u8, T);

        fn _test_kl05<T>(t: T) -> impl KnownLayout {
            KL05(0u8, t)
        }

        // | `repr(C)`? | generic? | `KnownLayout`? | `Sized`? | Type Name |
        // |          N |        Y |              Y |        Y |      KL07 |
        #[derive(KnownLayout)]
        #[allow(dead_code)] // fields are never read
        struct KL07<T: KnownLayout>(u8, T);

        fn _test_kl07<T: KnownLayout>(t: T) -> impl KnownLayout {
            let _ = KL07(0u8, t);
        }

        // | `repr(C)`? | generic? | `KnownLayout`? | `Sized`? | Type Name |
        // |          Y |        N |              Y |        N |      KL10 |
        #[derive(KnownLayout)]
        #[repr(C)]
        struct KL10(NotKnownLayout<AU32>, [u8]);

        let expected = DstLayout::new_zst(None)
            .extend(DstLayout::for_type::<NotKnownLayout<AU32>>(), None)
            .extend(<[u8] as KnownLayout>::LAYOUT, None)
            .pad_to_align();

        assert_eq!(<KL10 as KnownLayout>::LAYOUT, expected);
        assert_eq!(<KL10 as KnownLayout>::LAYOUT, unsized_layout(4, 1, 4));

        // ...with `align(N)`:
        #[derive(KnownLayout)]
        #[repr(C, align(64))]
        struct KL10Align(NotKnownLayout<AU32>, [u8]);

        let repr_align = NonZeroUsize::new(64);

        let expected = DstLayout::new_zst(repr_align)
            .extend(DstLayout::for_type::<NotKnownLayout<AU32>>(), None)
            .extend(<[u8] as KnownLayout>::LAYOUT, None)
            .pad_to_align();

        assert_eq!(<KL10Align as KnownLayout>::LAYOUT, expected);
        assert_eq!(<KL10Align as KnownLayout>::LAYOUT, unsized_layout(64, 1, 4));

        // ...with `packed`:
        #[derive(KnownLayout)]
        #[repr(C, packed)]
        struct KL10Packed(NotKnownLayout<AU32>, [u8]);

        let repr_packed = NonZeroUsize::new(1);

        let expected = DstLayout::new_zst(None)
            .extend(DstLayout::for_type::<NotKnownLayout<AU32>>(), repr_packed)
            .extend(<[u8] as KnownLayout>::LAYOUT, repr_packed)
            .pad_to_align();

        assert_eq!(<KL10Packed as KnownLayout>::LAYOUT, expected);
        assert_eq!(<KL10Packed as KnownLayout>::LAYOUT, unsized_layout(1, 1, 4));

        // ...with `packed(N)`:
        #[derive(KnownLayout)]
        #[repr(C, packed(2))]
        struct KL10PackedN(NotKnownLayout<AU32>, [u8]);

        let repr_packed = NonZeroUsize::new(2);

        let expected = DstLayout::new_zst(None)
            .extend(DstLayout::for_type::<NotKnownLayout<AU32>>(), repr_packed)
            .extend(<[u8] as KnownLayout>::LAYOUT, repr_packed)
            .pad_to_align();

        assert_eq!(<KL10PackedN as KnownLayout>::LAYOUT, expected);
        assert_eq!(<KL10PackedN as KnownLayout>::LAYOUT, unsized_layout(2, 1, 4));

        // | `repr(C)`? | generic? | `KnownLayout`? | `Sized`? | Type Name |
        // |          Y |        N |              Y |        Y |      KL11 |
        #[derive(KnownLayout)]
        #[repr(C)]
        struct KL11(NotKnownLayout<AU64>, u8);

        let expected = DstLayout::new_zst(None)
            .extend(DstLayout::for_type::<NotKnownLayout<AU64>>(), None)
            .extend(<u8 as KnownLayout>::LAYOUT, None)
            .pad_to_align();

        assert_eq!(<KL11 as KnownLayout>::LAYOUT, expected);
        assert_eq!(<KL11 as KnownLayout>::LAYOUT, sized_layout(8, 16));

        // ...with `align(N)`:
        #[derive(KnownLayout)]
        #[repr(C, align(64))]
        struct KL11Align(NotKnownLayout<AU64>, u8);

        let repr_align = NonZeroUsize::new(64);

        let expected = DstLayout::new_zst(repr_align)
            .extend(DstLayout::for_type::<NotKnownLayout<AU64>>(), None)
            .extend(<u8 as KnownLayout>::LAYOUT, None)
            .pad_to_align();

        assert_eq!(<KL11Align as KnownLayout>::LAYOUT, expected);
        assert_eq!(<KL11Align as KnownLayout>::LAYOUT, sized_layout(64, 64));

        // ...with `packed`:
        #[derive(KnownLayout)]
        #[repr(C, packed)]
        struct KL11Packed(NotKnownLayout<AU64>, u8);

        let repr_packed = NonZeroUsize::new(1);

        let expected = DstLayout::new_zst(None)
            .extend(DstLayout::for_type::<NotKnownLayout<AU64>>(), repr_packed)
            .extend(<u8 as KnownLayout>::LAYOUT, repr_packed)
            .pad_to_align();

        assert_eq!(<KL11Packed as KnownLayout>::LAYOUT, expected);
        assert_eq!(<KL11Packed as KnownLayout>::LAYOUT, sized_layout(1, 9));

        // ...with `packed(N)`:
        #[derive(KnownLayout)]
        #[repr(C, packed(2))]
        struct KL11PackedN(NotKnownLayout<AU64>, u8);

        let repr_packed = NonZeroUsize::new(2);

        let expected = DstLayout::new_zst(None)
            .extend(DstLayout::for_type::<NotKnownLayout<AU64>>(), repr_packed)
            .extend(<u8 as KnownLayout>::LAYOUT, repr_packed)
            .pad_to_align();

        assert_eq!(<KL11PackedN as KnownLayout>::LAYOUT, expected);
        assert_eq!(<KL11PackedN as KnownLayout>::LAYOUT, sized_layout(2, 10));

        // | `repr(C)`? | generic? | `KnownLayout`? | `Sized`? | Type Name |
        // |          Y |        Y |              Y |        N |      KL14 |
        #[derive(KnownLayout)]
        #[repr(C)]
        struct KL14<T: ?Sized + KnownLayout>(u8, T);

        fn _test_kl14<T: ?Sized + KnownLayout>(kl: &KL14<T>) {
            _assert_kl(kl)
        }

        // | `repr(C)`? | generic? | `KnownLayout`? | `Sized`? | Type Name |
        // |          Y |        Y |              Y |        Y |      KL15 |
        #[derive(KnownLayout)]
        #[repr(C)]
        struct KL15<T: KnownLayout>(u8, T);

        fn _test_kl15<T: KnownLayout>(t: T) -> impl KnownLayout {
            let _ = KL15(0u8, t);
        }

        // Test a variety of combinations of field types:
        //  - ()
        //  - u8
        //  - AU16
        //  - [()]
        //  - [u8]
        //  - [AU16]

        #[allow(clippy::upper_case_acronyms)]
        #[derive(KnownLayout)]
        #[repr(C)]
        struct KLTU<T, U: ?Sized>(T, U);

        assert_eq!(<KLTU<(), ()> as KnownLayout>::LAYOUT, sized_layout(1, 0));

        assert_eq!(<KLTU<(), u8> as KnownLayout>::LAYOUT, sized_layout(1, 1));

        assert_eq!(<KLTU<(), AU16> as KnownLayout>::LAYOUT, sized_layout(2, 2));

        assert_eq!(<KLTU<(), [()]> as KnownLayout>::LAYOUT, unsized_layout(1, 0, 0));

        assert_eq!(<KLTU<(), [u8]> as KnownLayout>::LAYOUT, unsized_layout(1, 1, 0));

        assert_eq!(<KLTU<(), [AU16]> as KnownLayout>::LAYOUT, unsized_layout(2, 2, 0));

        assert_eq!(<KLTU<u8, ()> as KnownLayout>::LAYOUT, sized_layout(1, 1));

        assert_eq!(<KLTU<u8, u8> as KnownLayout>::LAYOUT, sized_layout(1, 2));

        assert_eq!(<KLTU<u8, AU16> as KnownLayout>::LAYOUT, sized_layout(2, 4));

        assert_eq!(<KLTU<u8, [()]> as KnownLayout>::LAYOUT, unsized_layout(1, 0, 1));

        assert_eq!(<KLTU<u8, [u8]> as KnownLayout>::LAYOUT, unsized_layout(1, 1, 1));

        assert_eq!(<KLTU<u8, [AU16]> as KnownLayout>::LAYOUT, unsized_layout(2, 2, 2));

        assert_eq!(<KLTU<AU16, ()> as KnownLayout>::LAYOUT, sized_layout(2, 2));

        assert_eq!(<KLTU<AU16, u8> as KnownLayout>::LAYOUT, sized_layout(2, 4));

        assert_eq!(<KLTU<AU16, AU16> as KnownLayout>::LAYOUT, sized_layout(2, 4));

        assert_eq!(<KLTU<AU16, [()]> as KnownLayout>::LAYOUT, unsized_layout(2, 0, 2));

        assert_eq!(<KLTU<AU16, [u8]> as KnownLayout>::LAYOUT, unsized_layout(2, 1, 2));

        assert_eq!(<KLTU<AU16, [AU16]> as KnownLayout>::LAYOUT, unsized_layout(2, 2, 2));

        // Test a variety of field counts.

        #[derive(KnownLayout)]
        #[repr(C)]
        struct KLF0;

        assert_eq!(<KLF0 as KnownLayout>::LAYOUT, sized_layout(1, 0));

        #[derive(KnownLayout)]
        #[repr(C)]
        struct KLF1([u8]);

        assert_eq!(<KLF1 as KnownLayout>::LAYOUT, unsized_layout(1, 1, 0));

        #[derive(KnownLayout)]
        #[repr(C)]
        struct KLF2(NotKnownLayout<u8>, [u8]);

        assert_eq!(<KLF2 as KnownLayout>::LAYOUT, unsized_layout(1, 1, 1));

        #[derive(KnownLayout)]
        #[repr(C)]
        struct KLF3(NotKnownLayout<u8>, NotKnownLayout<AU16>, [u8]);

        assert_eq!(<KLF3 as KnownLayout>::LAYOUT, unsized_layout(2, 1, 4));

        #[derive(KnownLayout)]
        #[repr(C)]
        struct KLF4(NotKnownLayout<u8>, NotKnownLayout<AU16>, NotKnownLayout<AU32>, [u8]);

        assert_eq!(<KLF4 as KnownLayout>::LAYOUT, unsized_layout(4, 1, 8));
    }

    #[test]
    fn test_object_safety() {
        fn _takes_from_zeroes(_: &dyn FromZeroes) {}
        fn _takes_from_bytes(_: &dyn FromBytes) {}
        fn _takes_unaligned(_: &dyn Unaligned) {}
    }

    #[test]
    fn test_from_zeroes_only() {
        // Test types that implement `FromZeroes` but not `FromBytes`.

        assert!(!bool::new_zeroed());
        assert_eq!(char::new_zeroed(), '\0');

        #[cfg(feature = "alloc")]
        {
            assert_eq!(bool::new_box_zeroed(), Box::new(false));
            assert_eq!(char::new_box_zeroed(), Box::new('\0'));

            assert_eq!(bool::new_box_slice_zeroed(3).as_ref(), [false, false, false]);
            assert_eq!(char::new_box_slice_zeroed(3).as_ref(), ['\0', '\0', '\0']);

            assert_eq!(bool::new_vec_zeroed(3).as_ref(), [false, false, false]);
            assert_eq!(char::new_vec_zeroed(3).as_ref(), ['\0', '\0', '\0']);
        }

        let mut string = "hello".to_string();
        let s: &mut str = string.as_mut();
        assert_eq!(s, "hello");
        s.zero();
        assert_eq!(s, "\0\0\0\0\0");
    }

    #[test]
    fn test_read_write() {
        const VAL: u64 = 0x12345678;
        #[cfg(target_endian = "big")]
        const VAL_BYTES: [u8; 8] = VAL.to_be_bytes();
        #[cfg(target_endian = "little")]
        const VAL_BYTES: [u8; 8] = VAL.to_le_bytes();

        // Test `FromBytes::{read_from, read_from_prefix, read_from_suffix}`.

        assert_eq!(u64::read_from(&VAL_BYTES[..]), Some(VAL));
        // The first 8 bytes are from `VAL_BYTES` and the second 8 bytes are all
        // zeroes.
        let bytes_with_prefix: [u8; 16] = transmute!([VAL_BYTES, [0; 8]]);
        assert_eq!(u64::read_from_prefix(&bytes_with_prefix[..]), Some(VAL));
        assert_eq!(u64::read_from_suffix(&bytes_with_prefix[..]), Some(0));
        // The first 8 bytes are all zeroes and the second 8 bytes are from
        // `VAL_BYTES`
        let bytes_with_suffix: [u8; 16] = transmute!([[0; 8], VAL_BYTES]);
        assert_eq!(u64::read_from_prefix(&bytes_with_suffix[..]), Some(0));
        assert_eq!(u64::read_from_suffix(&bytes_with_suffix[..]), Some(VAL));

        // Test `AsBytes::{write_to, write_to_prefix, write_to_suffix}`.

        let mut bytes = [0u8; 8];
        assert_eq!(VAL.write_to(&mut bytes[..]), Some(()));
        assert_eq!(bytes, VAL_BYTES);
        let mut bytes = [0u8; 16];
        assert_eq!(VAL.write_to_prefix(&mut bytes[..]), Some(()));
        let want: [u8; 16] = transmute!([VAL_BYTES, [0; 8]]);
        assert_eq!(bytes, want);
        let mut bytes = [0u8; 16];
        assert_eq!(VAL.write_to_suffix(&mut bytes[..]), Some(()));
        let want: [u8; 16] = transmute!([[0; 8], VAL_BYTES]);
        assert_eq!(bytes, want);
    }

    #[test]
    fn test_transmute() {
        // Test that memory is transmuted as expected.
        let array_of_u8s = [0u8, 1, 2, 3, 4, 5, 6, 7];
        let array_of_arrays = [[0, 1], [2, 3], [4, 5], [6, 7]];
        let x: [[u8; 2]; 4] = transmute!(array_of_u8s);
        assert_eq!(x, array_of_arrays);
        let x: [u8; 8] = transmute!(array_of_arrays);
        assert_eq!(x, array_of_u8s);

        // Test that the source expression's value is forgotten rather than
        // dropped.
        #[derive(AsBytes)]
        #[repr(transparent)]
        struct PanicOnDrop(());
        impl Drop for PanicOnDrop {
            fn drop(&mut self) {
                panic!("PanicOnDrop::drop");
            }
        }
        #[allow(clippy::let_unit_value)]
        let _: () = transmute!(PanicOnDrop(()));

        // Test that `transmute!` is legal in a const context.
        const ARRAY_OF_U8S: [u8; 8] = [0u8, 1, 2, 3, 4, 5, 6, 7];
        const ARRAY_OF_ARRAYS: [[u8; 2]; 4] = [[0, 1], [2, 3], [4, 5], [6, 7]];
        const X: [[u8; 2]; 4] = transmute!(ARRAY_OF_U8S);
        assert_eq!(X, ARRAY_OF_ARRAYS);
    }

    #[test]
    fn test_transmute_ref() {
        // Test that memory is transmuted as expected.
        let array_of_u8s = [0u8, 1, 2, 3, 4, 5, 6, 7];
        let array_of_arrays = [[0, 1], [2, 3], [4, 5], [6, 7]];
        let x: &[[u8; 2]; 4] = transmute_ref!(&array_of_u8s);
        assert_eq!(*x, array_of_arrays);
        let x: &[u8; 8] = transmute_ref!(&array_of_arrays);
        assert_eq!(*x, array_of_u8s);

        // Test that `transmute_ref!` is legal in a const context.
        const ARRAY_OF_U8S: [u8; 8] = [0u8, 1, 2, 3, 4, 5, 6, 7];
        const ARRAY_OF_ARRAYS: [[u8; 2]; 4] = [[0, 1], [2, 3], [4, 5], [6, 7]];
        #[allow(clippy::redundant_static_lifetimes)]
        const X: &'static [[u8; 2]; 4] = transmute_ref!(&ARRAY_OF_U8S);
        assert_eq!(*X, ARRAY_OF_ARRAYS);

        // Test that it's legal to transmute a reference while shrinking the
        // lifetime (note that `X` has the lifetime `'static`).
        let x: &[u8; 8] = transmute_ref!(X);
        assert_eq!(*x, ARRAY_OF_U8S);

        // Test that `transmute_ref!` supports decreasing alignment.
        let u = AU64(0);
        let array = [0, 0, 0, 0, 0, 0, 0, 0];
        let x: &[u8; 8] = transmute_ref!(&u);
        assert_eq!(*x, array);

        // Test that a mutable reference can be turned into an immutable one.
        let mut x = 0u8;
        #[allow(clippy::useless_transmute)]
        let y: &u8 = transmute_ref!(&mut x);
        assert_eq!(*y, 0);
    }

    #[test]
    fn test_transmute_mut() {
        // Test that memory is transmuted as expected.
        let mut array_of_u8s = [0u8, 1, 2, 3, 4, 5, 6, 7];
        let mut array_of_arrays = [[0, 1], [2, 3], [4, 5], [6, 7]];
        let x: &mut [[u8; 2]; 4] = transmute_mut!(&mut array_of_u8s);
        assert_eq!(*x, array_of_arrays);
        let x: &mut [u8; 8] = transmute_mut!(&mut array_of_arrays);
        assert_eq!(*x, array_of_u8s);

        {
            // Test that it's legal to transmute a reference while shrinking the
            // lifetime.
            let x: &mut [u8; 8] = transmute_mut!(&mut array_of_arrays);
            assert_eq!(*x, array_of_u8s);
        }
        // Test that `transmute_mut!` supports decreasing alignment.
        let mut u = AU64(0);
        let array = [0, 0, 0, 0, 0, 0, 0, 0];
        let x: &[u8; 8] = transmute_mut!(&mut u);
        assert_eq!(*x, array);

        // Test that a mutable reference can be turned into an immutable one.
        let mut x = 0u8;
        #[allow(clippy::useless_transmute)]
        let y: &u8 = transmute_mut!(&mut x);
        assert_eq!(*y, 0);
    }

    #[test]
    fn test_macros_evaluate_args_once() {
        let mut ctr = 0;
        let _: usize = transmute!({
            ctr += 1;
            0usize
        });
        assert_eq!(ctr, 1);

        let mut ctr = 0;
        let _: &usize = transmute_ref!({
            ctr += 1;
            &0usize
        });
        assert_eq!(ctr, 1);
    }

    #[test]
    fn test_include_value() {
        const AS_U32: u32 = include_value!("../testdata/include_value/data");
        assert_eq!(AS_U32, u32::from_ne_bytes([b'a', b'b', b'c', b'd']));
        const AS_I32: i32 = include_value!("../testdata/include_value/data");
        assert_eq!(AS_I32, i32::from_ne_bytes([b'a', b'b', b'c', b'd']));
    }

    #[test]
    fn test_address() {
        // Test that the `Deref` and `DerefMut` implementations return a
        // reference which points to the right region of memory.

        let buf = [0];
        let r = Ref::<_, u8>::new(&buf[..]).unwrap();
        let buf_ptr = buf.as_ptr();
        let deref_ptr: *const u8 = r.deref();
        assert_eq!(buf_ptr, deref_ptr);

        let buf = [0];
        let r = Ref::<_, [u8]>::new_slice(&buf[..]).unwrap();
        let buf_ptr = buf.as_ptr();
        let deref_ptr = r.deref().as_ptr();
        assert_eq!(buf_ptr, deref_ptr);
    }

    // Verify that values written to a `Ref` are properly shared between the
    // typed and untyped representations, that reads via `deref` and `read`
    // behave the same, and that writes via `deref_mut` and `write` behave the
    // same.
    fn test_new_helper(mut r: Ref<&mut [u8], AU64>) {
        // assert that the value starts at 0
        assert_eq!(*r, AU64(0));
        assert_eq!(r.read(), AU64(0));

        // Assert that values written to the typed value are reflected in the
        // byte slice.
        const VAL1: AU64 = AU64(0xFF00FF00FF00FF00);
        *r = VAL1;
        assert_eq!(r.bytes(), &VAL1.to_bytes());
        *r = AU64(0);
        r.write(VAL1);
        assert_eq!(r.bytes(), &VAL1.to_bytes());

        // Assert that values written to the byte slice are reflected in the
        // typed value.
        const VAL2: AU64 = AU64(!VAL1.0); // different from `VAL1`
        r.bytes_mut().copy_from_slice(&VAL2.to_bytes()[..]);
        assert_eq!(*r, VAL2);
        assert_eq!(r.read(), VAL2);
    }

    // Verify that values written to a `Ref` are properly shared between the
    // typed and untyped representations; pass a value with `typed_len` `AU64`s
    // backed by an array of `typed_len * 8` bytes.
    fn test_new_helper_slice(mut r: Ref<&mut [u8], [AU64]>, typed_len: usize) {
        // Assert that the value starts out zeroed.
        assert_eq!(&*r, vec![AU64(0); typed_len].as_slice());

        // Check the backing storage is the exact same slice.
        let untyped_len = typed_len * 8;
        assert_eq!(r.bytes().len(), untyped_len);
        assert_eq!(r.bytes().as_ptr(), r.as_ptr().cast::<u8>());

        // Assert that values written to the typed value are reflected in the
        // byte slice.
        const VAL1: AU64 = AU64(0xFF00FF00FF00FF00);
        for typed in &mut *r {
            *typed = VAL1;
        }
        assert_eq!(r.bytes(), VAL1.0.to_ne_bytes().repeat(typed_len).as_slice());

        // Assert that values written to the byte slice are reflected in the
        // typed value.
        const VAL2: AU64 = AU64(!VAL1.0); // different from VAL1
        r.bytes_mut().copy_from_slice(&VAL2.0.to_ne_bytes().repeat(typed_len));
        assert!(r.iter().copied().all(|x| x == VAL2));
    }

    // Verify that values written to a `Ref` are properly shared between the
    // typed and untyped representations, that reads via `deref` and `read`
    // behave the same, and that writes via `deref_mut` and `write` behave the
    // same.
    fn test_new_helper_unaligned(mut r: Ref<&mut [u8], [u8; 8]>) {
        // assert that the value starts at 0
        assert_eq!(*r, [0; 8]);
        assert_eq!(r.read(), [0; 8]);

        // Assert that values written to the typed value are reflected in the
        // byte slice.
        const VAL1: [u8; 8] = [0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00];
        *r = VAL1;
        assert_eq!(r.bytes(), &VAL1);
        *r = [0; 8];
        r.write(VAL1);
        assert_eq!(r.bytes(), &VAL1);

        // Assert that values written to the byte slice are reflected in the
        // typed value.
        const VAL2: [u8; 8] = [0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF]; // different from VAL1
        r.bytes_mut().copy_from_slice(&VAL2[..]);
        assert_eq!(*r, VAL2);
        assert_eq!(r.read(), VAL2);
    }

    // Verify that values written to a `Ref` are properly shared between the
    // typed and untyped representations; pass a value with `len` `u8`s backed
    // by an array of `len` bytes.
    fn test_new_helper_slice_unaligned(mut r: Ref<&mut [u8], [u8]>, len: usize) {
        // Assert that the value starts out zeroed.
        assert_eq!(&*r, vec![0u8; len].as_slice());

        // Check the backing storage is the exact same slice.
        assert_eq!(r.bytes().len(), len);
        assert_eq!(r.bytes().as_ptr(), r.as_ptr());

        // Assert that values written to the typed value are reflected in the
        // byte slice.
        let mut expected_bytes = [0xFF, 0x00].iter().copied().cycle().take(len).collect::<Vec<_>>();
        r.copy_from_slice(&expected_bytes);
        assert_eq!(r.bytes(), expected_bytes.as_slice());

        // Assert that values written to the byte slice are reflected in the
        // typed value.
        for byte in &mut expected_bytes {
            *byte = !*byte; // different from `expected_len`
        }
        r.bytes_mut().copy_from_slice(&expected_bytes);
        assert_eq!(&*r, expected_bytes.as_slice());
    }

    #[test]
    fn test_new_aligned_sized() {
        // Test that a properly-aligned, properly-sized buffer works for new,
        // new_from_prefix, and new_from_suffix, and that new_from_prefix and
        // new_from_suffix return empty slices. Test that a properly-aligned
        // buffer whose length is a multiple of the element size works for
        // new_slice. Test that xxx_zeroed behaves the same, and zeroes the
        // memory.

        // A buffer with an alignment of 8.
        let mut buf = Align::<[u8; 8], AU64>::default();
        // `buf.t` should be aligned to 8, so this should always succeed.
        test_new_helper(Ref::<_, AU64>::new(&mut buf.t[..]).unwrap());
        let ascending: [u8; 8] = (0..8).collect::<Vec<_>>().try_into().unwrap();
        buf.t = ascending;
        test_new_helper(Ref::<_, AU64>::new_zeroed(&mut buf.t[..]).unwrap());
        {
            // In a block so that `r` and `suffix` don't live too long.
            buf.set_default();
            let (r, suffix) = Ref::<_, AU64>::new_from_prefix(&mut buf.t[..]).unwrap();
            assert!(suffix.is_empty());
            test_new_helper(r);
        }
        {
            buf.t = ascending;
            let (r, suffix) = Ref::<_, AU64>::new_from_prefix_zeroed(&mut buf.t[..]).unwrap();
            assert!(suffix.is_empty());
            test_new_helper(r);
        }
        {
            buf.set_default();
            let (prefix, r) = Ref::<_, AU64>::new_from_suffix(&mut buf.t[..]).unwrap();
            assert!(prefix.is_empty());
            test_new_helper(r);
        }
        {
            buf.t = ascending;
            let (prefix, r) = Ref::<_, AU64>::new_from_suffix_zeroed(&mut buf.t[..]).unwrap();
            assert!(prefix.is_empty());
            test_new_helper(r);
        }

        // A buffer with alignment 8 and length 24. We choose this length very
        // intentionally: if we instead used length 16, then the prefix and
        // suffix lengths would be identical. In the past, we used length 16,
        // which resulted in this test failing to discover the bug uncovered in
        // #506.
        let mut buf = Align::<[u8; 24], AU64>::default();
        // `buf.t` should be aligned to 8 and have a length which is a multiple
        // of `size_of::<AU64>()`, so this should always succeed.
        test_new_helper_slice(Ref::<_, [AU64]>::new_slice(&mut buf.t[..]).unwrap(), 3);
        let ascending: [u8; 24] = (0..24).collect::<Vec<_>>().try_into().unwrap();
        // 16 ascending bytes followed by 8 zeros.
        let mut ascending_prefix = ascending;
        ascending_prefix[16..].copy_from_slice(&[0, 0, 0, 0, 0, 0, 0, 0]);
        // 8 zeros followed by 16 ascending bytes.
        let mut ascending_suffix = ascending;
        ascending_suffix[..8].copy_from_slice(&[0, 0, 0, 0, 0, 0, 0, 0]);
        test_new_helper_slice(Ref::<_, [AU64]>::new_slice_zeroed(&mut buf.t[..]).unwrap(), 3);

        {
            buf.t = ascending_suffix;
            let (r, suffix) = Ref::<_, [AU64]>::new_slice_from_prefix(&mut buf.t[..], 1).unwrap();
            assert_eq!(suffix, &ascending[8..]);
            test_new_helper_slice(r, 1);
        }
        {
            buf.t = ascending_suffix;
            let (r, suffix) =
                Ref::<_, [AU64]>::new_slice_from_prefix_zeroed(&mut buf.t[..], 1).unwrap();
            assert_eq!(suffix, &ascending[8..]);
            test_new_helper_slice(r, 1);
        }
        {
            buf.t = ascending_prefix;
            let (prefix, r) = Ref::<_, [AU64]>::new_slice_from_suffix(&mut buf.t[..], 1).unwrap();
            assert_eq!(prefix, &ascending[..16]);
            test_new_helper_slice(r, 1);
        }
        {
            buf.t = ascending_prefix;
            let (prefix, r) =
                Ref::<_, [AU64]>::new_slice_from_suffix_zeroed(&mut buf.t[..], 1).unwrap();
            assert_eq!(prefix, &ascending[..16]);
            test_new_helper_slice(r, 1);
        }
    }

    #[test]
    fn test_new_unaligned_sized() {
        // Test that an unaligned, properly-sized buffer works for
        // `new_unaligned`, `new_unaligned_from_prefix`, and
        // `new_unaligned_from_suffix`, and that `new_unaligned_from_prefix`
        // `new_unaligned_from_suffix` return empty slices. Test that an
        // unaligned buffer whose length is a multiple of the element size works
        // for `new_slice`. Test that `xxx_zeroed` behaves the same, and zeroes
        // the memory.

        let mut buf = [0u8; 8];
        test_new_helper_unaligned(Ref::<_, [u8; 8]>::new_unaligned(&mut buf[..]).unwrap());
        buf = [0xFFu8; 8];
        test_new_helper_unaligned(Ref::<_, [u8; 8]>::new_unaligned_zeroed(&mut buf[..]).unwrap());
        {
            // In a block so that `r` and `suffix` don't live too long.
            buf = [0u8; 8];
            let (r, suffix) = Ref::<_, [u8; 8]>::new_unaligned_from_prefix(&mut buf[..]).unwrap();
            assert!(suffix.is_empty());
            test_new_helper_unaligned(r);
        }
        {
            buf = [0xFFu8; 8];
            let (r, suffix) =
                Ref::<_, [u8; 8]>::new_unaligned_from_prefix_zeroed(&mut buf[..]).unwrap();
            assert!(suffix.is_empty());
            test_new_helper_unaligned(r);
        }
        {
            buf = [0u8; 8];
            let (prefix, r) = Ref::<_, [u8; 8]>::new_unaligned_from_suffix(&mut buf[..]).unwrap();
            assert!(prefix.is_empty());
            test_new_helper_unaligned(r);
        }
        {
            buf = [0xFFu8; 8];
            let (prefix, r) =
                Ref::<_, [u8; 8]>::new_unaligned_from_suffix_zeroed(&mut buf[..]).unwrap();
            assert!(prefix.is_empty());
            test_new_helper_unaligned(r);
        }

        let mut buf = [0u8; 16];
        // `buf.t` should be aligned to 8 and have a length which is a multiple
        // of `size_of::AU64>()`, so this should always succeed.
        test_new_helper_slice_unaligned(
            Ref::<_, [u8]>::new_slice_unaligned(&mut buf[..]).unwrap(),
            16,
        );
        buf = [0xFFu8; 16];
        test_new_helper_slice_unaligned(
            Ref::<_, [u8]>::new_slice_unaligned_zeroed(&mut buf[..]).unwrap(),
            16,
        );

        {
            buf = [0u8; 16];
            let (r, suffix) =
                Ref::<_, [u8]>::new_slice_unaligned_from_prefix(&mut buf[..], 8).unwrap();
            assert_eq!(suffix, [0; 8]);
            test_new_helper_slice_unaligned(r, 8);
        }
        {
            buf = [0xFFu8; 16];
            let (r, suffix) =
                Ref::<_, [u8]>::new_slice_unaligned_from_prefix_zeroed(&mut buf[..], 8).unwrap();
            assert_eq!(suffix, [0xFF; 8]);
            test_new_helper_slice_unaligned(r, 8);
        }
        {
            buf = [0u8; 16];
            let (prefix, r) =
                Ref::<_, [u8]>::new_slice_unaligned_from_suffix(&mut buf[..], 8).unwrap();
            assert_eq!(prefix, [0; 8]);
            test_new_helper_slice_unaligned(r, 8);
        }
        {
            buf = [0xFFu8; 16];
            let (prefix, r) =
                Ref::<_, [u8]>::new_slice_unaligned_from_suffix_zeroed(&mut buf[..], 8).unwrap();
            assert_eq!(prefix, [0xFF; 8]);
            test_new_helper_slice_unaligned(r, 8);
        }
    }

    #[test]
    fn test_new_oversized() {
        // Test that a properly-aligned, overly-sized buffer works for
        // `new_from_prefix` and `new_from_suffix`, and that they return the
        // remainder and prefix of the slice respectively. Test that
        // `xxx_zeroed` behaves the same, and zeroes the memory.

        let mut buf = Align::<[u8; 16], AU64>::default();
        {
            // In a block so that `r` and `suffix` don't live too long. `buf.t`
            // should be aligned to 8, so this should always succeed.
            let (r, suffix) = Ref::<_, AU64>::new_from_prefix(&mut buf.t[..]).unwrap();
            assert_eq!(suffix.len(), 8);
            test_new_helper(r);
        }
        {
            buf.t = [0xFFu8; 16];
            // `buf.t` should be aligned to 8, so this should always succeed.
            let (r, suffix) = Ref::<_, AU64>::new_from_prefix_zeroed(&mut buf.t[..]).unwrap();
            // Assert that the suffix wasn't zeroed.
            assert_eq!(suffix, &[0xFFu8; 8]);
            test_new_helper(r);
        }
        {
            buf.set_default();
            // `buf.t` should be aligned to 8, so this should always succeed.
            let (prefix, r) = Ref::<_, AU64>::new_from_suffix(&mut buf.t[..]).unwrap();
            assert_eq!(prefix.len(), 8);
            test_new_helper(r);
        }
        {
            buf.t = [0xFFu8; 16];
            // `buf.t` should be aligned to 8, so this should always succeed.
            let (prefix, r) = Ref::<_, AU64>::new_from_suffix_zeroed(&mut buf.t[..]).unwrap();
            // Assert that the prefix wasn't zeroed.
            assert_eq!(prefix, &[0xFFu8; 8]);
            test_new_helper(r);
        }
    }

    #[test]
    fn test_new_unaligned_oversized() {
        // Test than an unaligned, overly-sized buffer works for
        // `new_unaligned_from_prefix` and `new_unaligned_from_suffix`, and that
        // they return the remainder and prefix of the slice respectively. Test
        // that `xxx_zeroed` behaves the same, and zeroes the memory.

        let mut buf = [0u8; 16];
        {
            // In a block so that `r` and `suffix` don't live too long.
            let (r, suffix) = Ref::<_, [u8; 8]>::new_unaligned_from_prefix(&mut buf[..]).unwrap();
            assert_eq!(suffix.len(), 8);
            test_new_helper_unaligned(r);
        }
        {
            buf = [0xFFu8; 16];
            let (r, suffix) =
                Ref::<_, [u8; 8]>::new_unaligned_from_prefix_zeroed(&mut buf[..]).unwrap();
            // Assert that the suffix wasn't zeroed.
            assert_eq!(suffix, &[0xFF; 8]);
            test_new_helper_unaligned(r);
        }
        {
            buf = [0u8; 16];
            let (prefix, r) = Ref::<_, [u8; 8]>::new_unaligned_from_suffix(&mut buf[..]).unwrap();
            assert_eq!(prefix.len(), 8);
            test_new_helper_unaligned(r);
        }
        {
            buf = [0xFFu8; 16];
            let (prefix, r) =
                Ref::<_, [u8; 8]>::new_unaligned_from_suffix_zeroed(&mut buf[..]).unwrap();
            // Assert that the prefix wasn't zeroed.
            assert_eq!(prefix, &[0xFF; 8]);
            test_new_helper_unaligned(r);
        }
    }

    #[test]
    fn test_ref_from_mut_from() {
        // Test `FromBytes::{ref_from, mut_from}{,_prefix,_suffix}` success cases
        // Exhaustive coverage for these methods is covered by the `Ref` tests above,
        // which these helper methods defer to.

        let mut buf =
            Align::<[u8; 16], AU64>::new([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]);

        assert_eq!(
            AU64::ref_from(&buf.t[8..]).unwrap().0.to_ne_bytes(),
            [8, 9, 10, 11, 12, 13, 14, 15]
        );
        let suffix = AU64::mut_from(&mut buf.t[8..]).unwrap();
        suffix.0 = 0x0101010101010101;
        // The `[u8:9]` is a non-half size of the full buffer, which would catch
        // `from_prefix` having the same implementation as `from_suffix` (issues #506, #511).
        assert_eq!(<[u8; 9]>::ref_from_suffix(&buf.t[..]).unwrap(), &[7u8, 1, 1, 1, 1, 1, 1, 1, 1]);
        let suffix = AU64::mut_from_suffix(&mut buf.t[1..]).unwrap();
        suffix.0 = 0x0202020202020202;
        <[u8; 10]>::mut_from_suffix(&mut buf.t[..]).unwrap()[0] = 42;
        assert_eq!(<[u8; 9]>::ref_from_prefix(&buf.t[..]).unwrap(), &[0, 1, 2, 3, 4, 5, 42, 7, 2]);
        <[u8; 2]>::mut_from_prefix(&mut buf.t[..]).unwrap()[1] = 30;
        assert_eq!(buf.t, [0, 30, 2, 3, 4, 5, 42, 7, 2, 2, 2, 2, 2, 2, 2, 2]);
    }

    #[test]
    fn test_ref_from_mut_from_error() {
        // Test `FromBytes::{ref_from, mut_from}{,_prefix,_suffix}` error cases.

        // Fail because the buffer is too large.
        let mut buf = Align::<[u8; 16], AU64>::default();
        // `buf.t` should be aligned to 8, so only the length check should fail.
        assert!(AU64::ref_from(&buf.t[..]).is_none());
        assert!(AU64::mut_from(&mut buf.t[..]).is_none());
        assert!(<[u8; 8]>::ref_from(&buf.t[..]).is_none());
        assert!(<[u8; 8]>::mut_from(&mut buf.t[..]).is_none());

        // Fail because the buffer is too small.
        let mut buf = Align::<[u8; 4], AU64>::default();
        assert!(AU64::ref_from(&buf.t[..]).is_none());
        assert!(AU64::mut_from(&mut buf.t[..]).is_none());
        assert!(<[u8; 8]>::ref_from(&buf.t[..]).is_none());
        assert!(<[u8; 8]>::mut_from(&mut buf.t[..]).is_none());
        assert!(AU64::ref_from_prefix(&buf.t[..]).is_none());
        assert!(AU64::mut_from_prefix(&mut buf.t[..]).is_none());
        assert!(AU64::ref_from_suffix(&buf.t[..]).is_none());
        assert!(AU64::mut_from_suffix(&mut buf.t[..]).is_none());
        assert!(<[u8; 8]>::ref_from_prefix(&buf.t[..]).is_none());
        assert!(<[u8; 8]>::mut_from_prefix(&mut buf.t[..]).is_none());
        assert!(<[u8; 8]>::ref_from_suffix(&buf.t[..]).is_none());
        assert!(<[u8; 8]>::mut_from_suffix(&mut buf.t[..]).is_none());

        // Fail because the alignment is insufficient.
        let mut buf = Align::<[u8; 13], AU64>::default();
        assert!(AU64::ref_from(&buf.t[1..]).is_none());
        assert!(AU64::mut_from(&mut buf.t[1..]).is_none());
        assert!(AU64::ref_from(&buf.t[1..]).is_none());
        assert!(AU64::mut_from(&mut buf.t[1..]).is_none());
        assert!(AU64::ref_from_prefix(&buf.t[1..]).is_none());
        assert!(AU64::mut_from_prefix(&mut buf.t[1..]).is_none());
        assert!(AU64::ref_from_suffix(&buf.t[..]).is_none());
        assert!(AU64::mut_from_suffix(&mut buf.t[..]).is_none());
    }

    #[test]
    #[allow(clippy::cognitive_complexity)]
    fn test_new_error() {
        // Fail because the buffer is too large.

        // A buffer with an alignment of 8.
        let mut buf = Align::<[u8; 16], AU64>::default();
        // `buf.t` should be aligned to 8, so only the length check should fail.
        assert!(Ref::<_, AU64>::new(&buf.t[..]).is_none());
        assert!(Ref::<_, AU64>::new_zeroed(&mut buf.t[..]).is_none());
        assert!(Ref::<_, [u8; 8]>::new_unaligned(&buf.t[..]).is_none());
        assert!(Ref::<_, [u8; 8]>::new_unaligned_zeroed(&mut buf.t[..]).is_none());

        // Fail because the buffer is too small.

        // A buffer with an alignment of 8.
        let mut buf = Align::<[u8; 4], AU64>::default();
        // `buf.t` should be aligned to 8, so only the length check should fail.
        assert!(Ref::<_, AU64>::new(&buf.t[..]).is_none());
        assert!(Ref::<_, AU64>::new_zeroed(&mut buf.t[..]).is_none());
        assert!(Ref::<_, [u8; 8]>::new_unaligned(&buf.t[..]).is_none());
        assert!(Ref::<_, [u8; 8]>::new_unaligned_zeroed(&mut buf.t[..]).is_none());
        assert!(Ref::<_, AU64>::new_from_prefix(&buf.t[..]).is_none());
        assert!(Ref::<_, AU64>::new_from_prefix_zeroed(&mut buf.t[..]).is_none());
        assert!(Ref::<_, AU64>::new_from_suffix(&buf.t[..]).is_none());
        assert!(Ref::<_, AU64>::new_from_suffix_zeroed(&mut buf.t[..]).is_none());
        assert!(Ref::<_, [u8; 8]>::new_unaligned_from_prefix(&buf.t[..]).is_none());
        assert!(Ref::<_, [u8; 8]>::new_unaligned_from_prefix_zeroed(&mut buf.t[..]).is_none());
        assert!(Ref::<_, [u8; 8]>::new_unaligned_from_suffix(&buf.t[..]).is_none());
        assert!(Ref::<_, [u8; 8]>::new_unaligned_from_suffix_zeroed(&mut buf.t[..]).is_none());

        // Fail because the length is not a multiple of the element size.

        let mut buf = Align::<[u8; 12], AU64>::default();
        // `buf.t` has length 12, but element size is 8.
        assert!(Ref::<_, [AU64]>::new_slice(&buf.t[..]).is_none());
        assert!(Ref::<_, [AU64]>::new_slice_zeroed(&mut buf.t[..]).is_none());
        assert!(Ref::<_, [[u8; 8]]>::new_slice_unaligned(&buf.t[..]).is_none());
        assert!(Ref::<_, [[u8; 8]]>::new_slice_unaligned_zeroed(&mut buf.t[..]).is_none());

        // Fail because the buffer is too short.
        let mut buf = Align::<[u8; 12], AU64>::default();
        // `buf.t` has length 12, but the element size is 8 (and we're expecting
        // two of them).
        assert!(Ref::<_, [AU64]>::new_slice_from_prefix(&buf.t[..], 2).is_none());
        assert!(Ref::<_, [AU64]>::new_slice_from_prefix_zeroed(&mut buf.t[..], 2).is_none());
        assert!(Ref::<_, [AU64]>::new_slice_from_suffix(&buf.t[..], 2).is_none());
        assert!(Ref::<_, [AU64]>::new_slice_from_suffix_zeroed(&mut buf.t[..], 2).is_none());
        assert!(Ref::<_, [[u8; 8]]>::new_slice_unaligned_from_prefix(&buf.t[..], 2).is_none());
        assert!(Ref::<_, [[u8; 8]]>::new_slice_unaligned_from_prefix_zeroed(&mut buf.t[..], 2)
            .is_none());
        assert!(Ref::<_, [[u8; 8]]>::new_slice_unaligned_from_suffix(&buf.t[..], 2).is_none());
        assert!(Ref::<_, [[u8; 8]]>::new_slice_unaligned_from_suffix_zeroed(&mut buf.t[..], 2)
            .is_none());

        // Fail because the alignment is insufficient.

        // A buffer with an alignment of 8. An odd buffer size is chosen so that
        // the last byte of the buffer has odd alignment.
        let mut buf = Align::<[u8; 13], AU64>::default();
        // Slicing from 1, we get a buffer with size 12 (so the length check
        // should succeed) but an alignment of only 1, which is insufficient.
        assert!(Ref::<_, AU64>::new(&buf.t[1..]).is_none());
        assert!(Ref::<_, AU64>::new_zeroed(&mut buf.t[1..]).is_none());
        assert!(Ref::<_, AU64>::new_from_prefix(&buf.t[1..]).is_none());
        assert!(Ref::<_, AU64>::new_from_prefix_zeroed(&mut buf.t[1..]).is_none());
        assert!(Ref::<_, [AU64]>::new_slice(&buf.t[1..]).is_none());
        assert!(Ref::<_, [AU64]>::new_slice_zeroed(&mut buf.t[1..]).is_none());
        assert!(Ref::<_, [AU64]>::new_slice_from_prefix(&buf.t[1..], 1).is_none());
        assert!(Ref::<_, [AU64]>::new_slice_from_prefix_zeroed(&mut buf.t[1..], 1).is_none());
        assert!(Ref::<_, [AU64]>::new_slice_from_suffix(&buf.t[1..], 1).is_none());
        assert!(Ref::<_, [AU64]>::new_slice_from_suffix_zeroed(&mut buf.t[1..], 1).is_none());
        // Slicing is unnecessary here because `new_from_suffix[_zeroed]` use
        // the suffix of the slice, which has odd alignment.
        assert!(Ref::<_, AU64>::new_from_suffix(&buf.t[..]).is_none());
        assert!(Ref::<_, AU64>::new_from_suffix_zeroed(&mut buf.t[..]).is_none());

        // Fail due to arithmetic overflow.

        let mut buf = Align::<[u8; 16], AU64>::default();
        let unreasonable_len = usize::MAX / mem::size_of::<AU64>() + 1;
        assert!(Ref::<_, [AU64]>::new_slice_from_prefix(&buf.t[..], unreasonable_len).is_none());
        assert!(Ref::<_, [AU64]>::new_slice_from_prefix_zeroed(&mut buf.t[..], unreasonable_len)
            .is_none());
        assert!(Ref::<_, [AU64]>::new_slice_from_suffix(&buf.t[..], unreasonable_len).is_none());
        assert!(Ref::<_, [AU64]>::new_slice_from_suffix_zeroed(&mut buf.t[..], unreasonable_len)
            .is_none());
        assert!(Ref::<_, [[u8; 8]]>::new_slice_unaligned_from_prefix(&buf.t[..], unreasonable_len)
            .is_none());
        assert!(Ref::<_, [[u8; 8]]>::new_slice_unaligned_from_prefix_zeroed(
            &mut buf.t[..],
            unreasonable_len
        )
        .is_none());
        assert!(Ref::<_, [[u8; 8]]>::new_slice_unaligned_from_suffix(&buf.t[..], unreasonable_len)
            .is_none());
        assert!(Ref::<_, [[u8; 8]]>::new_slice_unaligned_from_suffix_zeroed(
            &mut buf.t[..],
            unreasonable_len
        )
        .is_none());
    }

    // Tests for ensuring that, if a ZST is passed into a slice-like function,
    // we always panic. Since these tests need to be separate per-function, and
    // they tend to take up a lot of space, we generate them using a macro in a
    // submodule instead. The submodule ensures that we can just re-use the name
    // of the function under test for the name of the test itself.
    mod test_zst_panics {
        macro_rules! zst_test {
            ($name:ident($($tt:tt)*), $constructor_in_panic_msg:tt) => {
                #[test]
                #[should_panic = concat!("Ref::", $constructor_in_panic_msg, " called on a zero-sized type")]
                fn $name() {
                    let mut buffer = [0u8];
                    let r = $crate::Ref::<_, [()]>::$name(&mut buffer[..], $($tt)*);
                    unreachable!("should have panicked, got {:?}", r);
                }
            }
        }
        zst_test!(new_slice(), "new_slice");
        zst_test!(new_slice_zeroed(), "new_slice");
        zst_test!(new_slice_from_prefix(1), "new_slice");
        zst_test!(new_slice_from_prefix_zeroed(1), "new_slice");
        zst_test!(new_slice_from_suffix(1), "new_slice");
        zst_test!(new_slice_from_suffix_zeroed(1), "new_slice");
        zst_test!(new_slice_unaligned(), "new_slice_unaligned");
        zst_test!(new_slice_unaligned_zeroed(), "new_slice_unaligned");
        zst_test!(new_slice_unaligned_from_prefix(1), "new_slice_unaligned");
        zst_test!(new_slice_unaligned_from_prefix_zeroed(1), "new_slice_unaligned");
        zst_test!(new_slice_unaligned_from_suffix(1), "new_slice_unaligned");
        zst_test!(new_slice_unaligned_from_suffix_zeroed(1), "new_slice_unaligned");
    }

    #[test]
    fn test_as_bytes_methods() {
        /// Run a series of tests by calling `AsBytes` methods on `t`.
        ///
        /// `bytes` is the expected byte sequence returned from `t.as_bytes()`
        /// before `t` has been modified. `post_mutation` is the expected
        /// sequence returned from `t.as_bytes()` after `t.as_bytes_mut()[0]`
        /// has had its bits flipped (by applying `^= 0xFF`).
        ///
        /// `N` is the size of `t` in bytes.
        fn test<T: FromBytes + AsBytes + Debug + Eq + ?Sized, const N: usize>(
            t: &mut T,
            bytes: &[u8],
            post_mutation: &T,
        ) {
            // Test that we can access the underlying bytes, and that we get the
            // right bytes and the right number of bytes.
            assert_eq!(t.as_bytes(), bytes);

            // Test that changes to the underlying byte slices are reflected in
            // the original object.
            t.as_bytes_mut()[0] ^= 0xFF;
            assert_eq!(t, post_mutation);
            t.as_bytes_mut()[0] ^= 0xFF;

            // `write_to` rejects slices that are too small or too large.
            assert_eq!(t.write_to(&mut vec![0; N - 1][..]), None);
            assert_eq!(t.write_to(&mut vec![0; N + 1][..]), None);

            // `write_to` works as expected.
            let mut bytes = [0; N];
            assert_eq!(t.write_to(&mut bytes[..]), Some(()));
            assert_eq!(bytes, t.as_bytes());

            // `write_to_prefix` rejects slices that are too small.
            assert_eq!(t.write_to_prefix(&mut vec![0; N - 1][..]), None);

            // `write_to_prefix` works with exact-sized slices.
            let mut bytes = [0; N];
            assert_eq!(t.write_to_prefix(&mut bytes[..]), Some(()));
            assert_eq!(bytes, t.as_bytes());

            // `write_to_prefix` works with too-large slices, and any bytes past
            // the prefix aren't modified.
            let mut too_many_bytes = vec![0; N + 1];
            too_many_bytes[N] = 123;
            assert_eq!(t.write_to_prefix(&mut too_many_bytes[..]), Some(()));
            assert_eq!(&too_many_bytes[..N], t.as_bytes());
            assert_eq!(too_many_bytes[N], 123);

            // `write_to_suffix` rejects slices that are too small.
            assert_eq!(t.write_to_suffix(&mut vec![0; N - 1][..]), None);

            // `write_to_suffix` works with exact-sized slices.
            let mut bytes = [0; N];
            assert_eq!(t.write_to_suffix(&mut bytes[..]), Some(()));
            assert_eq!(bytes, t.as_bytes());

            // `write_to_suffix` works with too-large slices, and any bytes
            // before the suffix aren't modified.
            let mut too_many_bytes = vec![0; N + 1];
            too_many_bytes[0] = 123;
            assert_eq!(t.write_to_suffix(&mut too_many_bytes[..]), Some(()));
            assert_eq!(&too_many_bytes[1..], t.as_bytes());
            assert_eq!(too_many_bytes[0], 123);
        }

        #[derive(Debug, Eq, PartialEq, FromZeroes, FromBytes, AsBytes)]
        #[repr(C)]
        struct Foo {
            a: u32,
            b: Wrapping<u32>,
            c: Option<NonZeroU32>,
        }

        let expected_bytes: Vec<u8> = if cfg!(target_endian = "little") {
            vec![1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0]
        } else {
            vec![0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0]
        };
        let post_mutation_expected_a =
            if cfg!(target_endian = "little") { 0x00_00_00_FE } else { 0xFF_00_00_01 };
        test::<_, 12>(
            &mut Foo { a: 1, b: Wrapping(2), c: None },
            expected_bytes.as_bytes(),
            &Foo { a: post_mutation_expected_a, b: Wrapping(2), c: None },
        );
        test::<_, 3>(
            Unsized::from_mut_slice(&mut [1, 2, 3]),
            &[1, 2, 3],
            Unsized::from_mut_slice(&mut [0xFE, 2, 3]),
        );
    }

    #[test]
    fn test_array() {
        #[derive(FromZeroes, FromBytes, AsBytes)]
        #[repr(C)]
        struct Foo {
            a: [u16; 33],
        }

        let foo = Foo { a: [0xFFFF; 33] };
        let expected = [0xFFu8; 66];
        assert_eq!(foo.as_bytes(), &expected[..]);
    }

    #[test]
    fn test_display_debug() {
        let buf = Align::<[u8; 8], u64>::default();
        let r = Ref::<_, u64>::new(&buf.t[..]).unwrap();
        assert_eq!(format!("{}", r), "0");
        assert_eq!(format!("{:?}", r), "Ref(0)");

        let buf = Align::<[u8; 8], u64>::default();
        let r = Ref::<_, [u64]>::new_slice(&buf.t[..]).unwrap();
        assert_eq!(format!("{:?}", r), "Ref([0])");
    }

    #[test]
    fn test_eq() {
        let buf1 = 0_u64;
        let r1 = Ref::<_, u64>::new(buf1.as_bytes()).unwrap();
        let buf2 = 0_u64;
        let r2 = Ref::<_, u64>::new(buf2.as_bytes()).unwrap();
        assert_eq!(r1, r2);
    }

    #[test]
    fn test_ne() {
        let buf1 = 0_u64;
        let r1 = Ref::<_, u64>::new(buf1.as_bytes()).unwrap();
        let buf2 = 1_u64;
        let r2 = Ref::<_, u64>::new(buf2.as_bytes()).unwrap();
        assert_ne!(r1, r2);
    }

    #[test]
    fn test_ord() {
        let buf1 = 0_u64;
        let r1 = Ref::<_, u64>::new(buf1.as_bytes()).unwrap();
        let buf2 = 1_u64;
        let r2 = Ref::<_, u64>::new(buf2.as_bytes()).unwrap();
        assert!(r1 < r2);
    }

    #[test]
    fn test_new_zeroed() {
        assert!(!bool::new_zeroed());
        assert_eq!(u64::new_zeroed(), 0);
        // This test exists in order to exercise unsafe code, especially when
        // running under Miri.
        #[allow(clippy::unit_cmp)]
        {
            assert_eq!(<()>::new_zeroed(), ());
        }
    }

    #[test]
    fn test_transparent_packed_generic_struct() {
        #[derive(AsBytes, FromZeroes, FromBytes, Unaligned)]
        #[repr(transparent)]
        #[allow(dead_code)] // for the unused fields
        struct Foo<T> {
            _t: T,
            _phantom: PhantomData<()>,
        }

        assert_impl_all!(Foo<u32>: FromZeroes, FromBytes, AsBytes);
        assert_impl_all!(Foo<u8>: Unaligned);

        #[derive(AsBytes, FromZeroes, FromBytes, Unaligned)]
        #[repr(packed)]
        #[allow(dead_code)] // for the unused fields
        struct Bar<T, U> {
            _t: T,
            _u: U,
        }

        assert_impl_all!(Bar<u8, AU64>: FromZeroes, FromBytes, AsBytes, Unaligned);
    }

    #[test]
    fn test_impls() {
        use core::borrow::Borrow;

        // A type that can supply test cases for testing
        // `TryFromBytes::is_bit_valid`. All types passed to `assert_impls!`
        // must implement this trait; that macro uses it to generate runtime
        // tests for `TryFromBytes` impls.
        //
        // All `T: FromBytes` types are provided with a blanket impl. Other
        // types must implement `TryFromBytesTestable` directly (ie using
        // `impl_try_from_bytes_testable!`).
        trait TryFromBytesTestable {
            fn with_passing_test_cases<F: Fn(&Self)>(f: F);
            fn with_failing_test_cases<F: Fn(&[u8])>(f: F);
        }

        impl<T: FromBytes> TryFromBytesTestable for T {
            fn with_passing_test_cases<F: Fn(&Self)>(f: F) {
                // Test with a zeroed value.
                f(&Self::new_zeroed());

                let ffs = {
                    let mut t = Self::new_zeroed();
                    let ptr: *mut T = &mut t;
                    // SAFETY: `T: FromBytes`
                    unsafe { ptr::write_bytes(ptr.cast::<u8>(), 0xFF, mem::size_of::<T>()) };
                    t
                };

                // Test with a value initialized with 0xFF.
                f(&ffs);
            }

            fn with_failing_test_cases<F: Fn(&[u8])>(_f: F) {}
        }

        // Implements `TryFromBytesTestable`.
        macro_rules! impl_try_from_bytes_testable {
            // Base case for recursion (when the list of types has run out).
            (=> @success $($success_case:expr),* $(, @failure $($failure_case:expr),*)?) => {};
            // Implements for type(s) with no type parameters.
            ($ty:ty $(,$tys:ty)* => @success $($success_case:expr),* $(, @failure $($failure_case:expr),*)?) => {
                impl TryFromBytesTestable for $ty {
                    impl_try_from_bytes_testable!(
                        @methods     @success $($success_case),*
                                 $(, @failure $($failure_case),*)?
                    );
                }
                impl_try_from_bytes_testable!($($tys),* => @success $($success_case),* $(, @failure $($failure_case),*)?);
            };
            // Implements for multiple types with no type parameters.
            ($($($ty:ty),* => @success $($success_case:expr), * $(, @failure $($failure_case:expr),*)?;)*) => {
                $(
                    impl_try_from_bytes_testable!($($ty),* => @success $($success_case),* $(, @failure $($failure_case),*)*);
                )*
            };
            // Implements only the methods; caller must invoke this from inside
            // an impl block.
            (@methods @success $($success_case:expr),* $(, @failure $($failure_case:expr),*)?) => {
                fn with_passing_test_cases<F: Fn(&Self)>(_f: F) {
                    $(
                        _f($success_case.borrow());
                    )*
                }

                fn with_failing_test_cases<F: Fn(&[u8])>(_f: F) {
                    $($(
                        let case = $failure_case.as_bytes();
                        _f(case.as_bytes());
                    )*)?
                }
            };
        }

        // Note that these impls are only for types which are not `FromBytes`.
        // `FromBytes` types are covered by a preceding blanket impl.
        impl_try_from_bytes_testable!(
            bool => @success true, false,
                    @failure 2u8, 3u8, 0xFFu8;
            char => @success '\u{0}', '\u{D7FF}', '\u{E000}', '\u{10FFFF}',
                    @failure 0xD800u32, 0xDFFFu32, 0x110000u32;
            str  => @success "", "hello", "❤️🧡💛💚💙💜",
                    @failure [0, 159, 146, 150];
            [u8] => @success [], [0, 1, 2];
            NonZeroU8, NonZeroI8, NonZeroU16, NonZeroI16, NonZeroU32,
            NonZeroI32, NonZeroU64, NonZeroI64, NonZeroU128, NonZeroI128,
            NonZeroUsize, NonZeroIsize
                 => @success Self::new(1).unwrap(),
                    // Doing this instead of `0` ensures that we always satisfy
                    // the size and alignment requirements of `Self` (whereas
                    // `0` may be any integer type with a different size or
                    // alignment than some `NonZeroXxx` types).
                    @failure Option::<Self>::None;
            [bool]
                => @success [true, false], [false, true],
                    @failure [2u8], [3u8], [0xFFu8], [0u8, 1u8, 2u8];
        );

        // Asserts that `$ty` implements any `$trait` and doesn't implement any
        // `!$trait`. Note that all `$trait`s must come before any `!$trait`s.
        //
        // For `T: TryFromBytes`, uses `TryFromBytesTestable` to test success
        // and failure cases for `TryFromBytes::is_bit_valid`.
        macro_rules! assert_impls {
            ($ty:ty: TryFromBytes) => {
                <$ty as TryFromBytesTestable>::with_passing_test_cases(|val| {
                    let c = Ptr::from(val);
                    // SAFETY:
                    // - Since `val` is a normal reference, `c` is guranteed to
                    //   be aligned, to point to a single allocation, and to
                    //   have a size which doesn't overflow `isize`.
                    // - Since `val` is a valid `$ty`, `c`'s referent satisfies
                    //   the bit validity constraints of `is_bit_valid`, which
                    //   are a superset of the bit validity constraints of
                    //   `$ty`.
                    let res = unsafe { <$ty as TryFromBytes>::is_bit_valid(c) };
                    assert!(res, "{}::is_bit_valid({:?}): got false, expected true", stringify!($ty), val);

                    // TODO(#5): In addition to testing `is_bit_valid`, test the
                    // methods built on top of it. This would both allow us to
                    // test their implementations and actually convert the bytes
                    // to `$ty`, giving Miri a chance to catch if this is
                    // unsound (ie, if our `is_bit_valid` impl is buggy).
                    //
                    // The following code was tried, but it doesn't work because
                    // a) some types are not `AsBytes` and, b) some types are
                    // not `Sized`.
                    //
                    //   let r = <$ty as TryFromBytes>::try_from_ref(val.as_bytes()).unwrap();
                    //   assert_eq!(r, &val);
                    //   let r = <$ty as TryFromBytes>::try_from_mut(val.as_bytes_mut()).unwrap();
                    //   assert_eq!(r, &mut val);
                    //   let v = <$ty as TryFromBytes>::try_read_from(val.as_bytes()).unwrap();
                    //   assert_eq!(v, val);
                });
                #[allow(clippy::as_conversions)]
                <$ty as TryFromBytesTestable>::with_failing_test_cases(|c| {
                    let res = <$ty as TryFromBytes>::try_from_ref(c);
                    assert!(res.is_none(), "{}::is_bit_valid({:?}): got true, expected false", stringify!($ty), c);
                });

                #[allow(dead_code)]
                const _: () = { static_assertions::assert_impl_all!($ty: TryFromBytes); };
            };
            ($ty:ty: $trait:ident) => {
                #[allow(dead_code)]
                const _: () = { static_assertions::assert_impl_all!($ty: $trait); };
            };
            ($ty:ty: !$trait:ident) => {
                #[allow(dead_code)]
                const _: () = { static_assertions::assert_not_impl_any!($ty: $trait); };
            };
            ($ty:ty: $($trait:ident),* $(,)? $(!$negative_trait:ident),*) => {
                $(
                    assert_impls!($ty: $trait);
                )*

                $(
                    assert_impls!($ty: !$negative_trait);
                )*
            };
        }

        // NOTE: The negative impl assertions here are not necessarily
        // prescriptive. They merely serve as change detectors to make sure
        // we're aware of what trait impls are getting added with a given
        // change. Of course, some impls would be invalid (e.g., `bool:
        // FromBytes`), and so this change detection is very important.

        assert_impls!((): KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, Unaligned);
        assert_impls!(u8: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, Unaligned);
        assert_impls!(i8: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, Unaligned);
        assert_impls!(u16: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);
        assert_impls!(i16: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);
        assert_impls!(u32: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);
        assert_impls!(i32: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);
        assert_impls!(u64: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);
        assert_impls!(i64: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);
        assert_impls!(u128: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);
        assert_impls!(i128: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);
        assert_impls!(usize: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);
        assert_impls!(isize: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);
        assert_impls!(f32: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);
        assert_impls!(f64: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);

        assert_impls!(bool: KnownLayout, TryFromBytes, FromZeroes, AsBytes, Unaligned, !FromBytes);
        assert_impls!(char: KnownLayout, TryFromBytes, FromZeroes, AsBytes, !FromBytes, !Unaligned);
        assert_impls!(str: KnownLayout, TryFromBytes, FromZeroes, AsBytes, Unaligned, !FromBytes);

        assert_impls!(NonZeroU8: KnownLayout, TryFromBytes, AsBytes, Unaligned, !FromZeroes, !FromBytes);
        assert_impls!(NonZeroI8: KnownLayout, TryFromBytes, AsBytes, Unaligned, !FromZeroes, !FromBytes);
        assert_impls!(NonZeroU16: KnownLayout, TryFromBytes, AsBytes, !FromBytes, !Unaligned);
        assert_impls!(NonZeroI16: KnownLayout, TryFromBytes, AsBytes, !FromBytes, !Unaligned);
        assert_impls!(NonZeroU32: KnownLayout, TryFromBytes, AsBytes, !FromBytes, !Unaligned);
        assert_impls!(NonZeroI32: KnownLayout, TryFromBytes, AsBytes, !FromBytes, !Unaligned);
        assert_impls!(NonZeroU64: KnownLayout, TryFromBytes, AsBytes, !FromBytes, !Unaligned);
        assert_impls!(NonZeroI64: KnownLayout, TryFromBytes, AsBytes, !FromBytes, !Unaligned);
        assert_impls!(NonZeroU128: KnownLayout, TryFromBytes, AsBytes, !FromBytes, !Unaligned);
        assert_impls!(NonZeroI128: KnownLayout, TryFromBytes, AsBytes, !FromBytes, !Unaligned);
        assert_impls!(NonZeroUsize: KnownLayout, TryFromBytes, AsBytes, !FromBytes, !Unaligned);
        assert_impls!(NonZeroIsize: KnownLayout, TryFromBytes, AsBytes, !FromBytes, !Unaligned);

        assert_impls!(Option<NonZeroU8>: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, Unaligned);
        assert_impls!(Option<NonZeroI8>: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, Unaligned);
        assert_impls!(Option<NonZeroU16>: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);
        assert_impls!(Option<NonZeroI16>: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);
        assert_impls!(Option<NonZeroU32>: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);
        assert_impls!(Option<NonZeroI32>: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);
        assert_impls!(Option<NonZeroU64>: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);
        assert_impls!(Option<NonZeroI64>: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);
        assert_impls!(Option<NonZeroU128>: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);
        assert_impls!(Option<NonZeroI128>: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);
        assert_impls!(Option<NonZeroUsize>: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);
        assert_impls!(Option<NonZeroIsize>: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned);

        // Implements none of the ZC traits.
        struct NotZerocopy;

        #[rustfmt::skip]
        type FnManyArgs = fn(
            NotZerocopy, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8,
        ) -> (NotZerocopy, NotZerocopy);

        // Allowed, because we're not actually using this type for FFI.
        #[allow(improper_ctypes_definitions)]
        #[rustfmt::skip]
        type ECFnManyArgs = extern "C" fn(
            NotZerocopy, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8,
        ) -> (NotZerocopy, NotZerocopy);

        #[cfg(feature = "alloc")]
        assert_impls!(Option<Box<UnsafeCell<NotZerocopy>>>: KnownLayout, FromZeroes, !TryFromBytes, !FromBytes, !AsBytes, !Unaligned);
        assert_impls!(Option<Box<[UnsafeCell<NotZerocopy>]>>: KnownLayout, !TryFromBytes, !FromZeroes, !FromBytes, !AsBytes, !Unaligned);
        assert_impls!(Option<&'static UnsafeCell<NotZerocopy>>: KnownLayout, FromZeroes, !TryFromBytes, !FromBytes, !AsBytes, !Unaligned);
        assert_impls!(Option<&'static [UnsafeCell<NotZerocopy>]>: KnownLayout, !TryFromBytes, !FromZeroes, !FromBytes, !AsBytes, !Unaligned);
        assert_impls!(Option<&'static mut UnsafeCell<NotZerocopy>>: KnownLayout, FromZeroes, !TryFromBytes, !FromBytes, !AsBytes, !Unaligned);
        assert_impls!(Option<&'static mut [UnsafeCell<NotZerocopy>]>: KnownLayout, !TryFromBytes, !FromZeroes, !FromBytes, !AsBytes, !Unaligned);
        assert_impls!(Option<NonNull<UnsafeCell<NotZerocopy>>>: KnownLayout, FromZeroes, !TryFromBytes, !FromBytes, !AsBytes, !Unaligned);
        assert_impls!(Option<NonNull<[UnsafeCell<NotZerocopy>]>>: KnownLayout, !TryFromBytes, !FromZeroes, !FromBytes, !AsBytes, !Unaligned);
        assert_impls!(Option<fn()>: KnownLayout, FromZeroes, !TryFromBytes, !FromBytes, !AsBytes, !Unaligned);
        assert_impls!(Option<FnManyArgs>: KnownLayout, FromZeroes, !TryFromBytes, !FromBytes, !AsBytes, !Unaligned);
        assert_impls!(Option<extern "C" fn()>: KnownLayout, FromZeroes, !TryFromBytes, !FromBytes, !AsBytes, !Unaligned);
        assert_impls!(Option<ECFnManyArgs>: KnownLayout, FromZeroes, !TryFromBytes, !FromBytes, !AsBytes, !Unaligned);

        assert_impls!(PhantomData<NotZerocopy>: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, Unaligned);
        assert_impls!(PhantomData<[u8]>: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, Unaligned);

        assert_impls!(ManuallyDrop<u8>: KnownLayout, FromZeroes, FromBytes, AsBytes, Unaligned, !TryFromBytes);
        assert_impls!(ManuallyDrop<[u8]>: KnownLayout, FromZeroes, FromBytes, AsBytes, Unaligned, !TryFromBytes);
        assert_impls!(ManuallyDrop<NotZerocopy>: !TryFromBytes, !KnownLayout, !FromZeroes, !FromBytes, !AsBytes, !Unaligned);
        assert_impls!(ManuallyDrop<[NotZerocopy]>: !TryFromBytes, !KnownLayout, !FromZeroes, !FromBytes, !AsBytes, !Unaligned);

        assert_impls!(MaybeUninit<u8>: KnownLayout, TryFromBytes, FromZeroes, FromBytes, Unaligned, !AsBytes);
        assert_impls!(MaybeUninit<NotZerocopy>: KnownLayout, !TryFromBytes, !FromZeroes, !FromBytes, !AsBytes, !Unaligned);

        assert_impls!(Wrapping<u8>: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, Unaligned);
        assert_impls!(Wrapping<NotZerocopy>: KnownLayout, !TryFromBytes, !FromZeroes, !FromBytes, !AsBytes, !Unaligned);

        assert_impls!(Unalign<u8>: KnownLayout, FromZeroes, FromBytes, AsBytes, Unaligned, !TryFromBytes);
        assert_impls!(Unalign<NotZerocopy>: Unaligned, !KnownLayout, !TryFromBytes, !FromZeroes, !FromBytes, !AsBytes);

        assert_impls!([u8]: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, Unaligned);
        assert_impls!([bool]: KnownLayout, TryFromBytes, FromZeroes, AsBytes, Unaligned, !FromBytes);
        assert_impls!([NotZerocopy]: !KnownLayout, !TryFromBytes, !FromZeroes, !FromBytes, !AsBytes, !Unaligned);
        assert_impls!([u8; 0]: KnownLayout, FromZeroes, FromBytes, AsBytes, Unaligned, !TryFromBytes);
        assert_impls!([NotZerocopy; 0]: KnownLayout, !TryFromBytes, !FromZeroes, !FromBytes, !AsBytes, !Unaligned);
        assert_impls!([u8; 1]: KnownLayout, FromZeroes, FromBytes, AsBytes, Unaligned, !TryFromBytes);
        assert_impls!([NotZerocopy; 1]: KnownLayout, !TryFromBytes, !FromZeroes, !FromBytes, !AsBytes, !Unaligned);

        assert_impls!(*const NotZerocopy: KnownLayout, FromZeroes, !TryFromBytes, !FromBytes, !AsBytes, !Unaligned);
        assert_impls!(*mut NotZerocopy: KnownLayout, FromZeroes, !TryFromBytes, !FromBytes, !AsBytes, !Unaligned);
        assert_impls!(*const [NotZerocopy]: KnownLayout, !TryFromBytes, !FromZeroes, !FromBytes, !AsBytes, !Unaligned);
        assert_impls!(*mut [NotZerocopy]: KnownLayout, !TryFromBytes, !FromZeroes, !FromBytes, !AsBytes, !Unaligned);
        assert_impls!(*const dyn Debug: KnownLayout, !TryFromBytes, !FromZeroes, !FromBytes, !AsBytes, !Unaligned);
        assert_impls!(*mut dyn Debug: KnownLayout, !TryFromBytes, !FromZeroes, !FromBytes, !AsBytes, !Unaligned);

        #[cfg(feature = "simd")]
        {
            #[allow(unused_macros)]
            macro_rules! test_simd_arch_mod {
                ($arch:ident, $($typ:ident),*) => {
                    {
                        use core::arch::$arch::{$($typ),*};
                        use crate::*;
                        $( assert_impls!($typ: KnownLayout, TryFromBytes, FromZeroes, FromBytes, AsBytes, !Unaligned); )*
                    }
                };
            }
            #[cfg(target_arch = "x86")]
            test_simd_arch_mod!(x86, __m128, __m128d, __m128i, __m256, __m256d, __m256i);

            #[cfg(all(feature = "simd-nightly", target_arch = "x86"))]
            test_simd_arch_mod!(x86, __m512bh, __m512, __m512d, __m512i);

            #[cfg(target_arch = "x86_64")]
            test_simd_arch_mod!(x86_64, __m128, __m128d, __m128i, __m256, __m256d, __m256i);

            #[cfg(all(feature = "simd-nightly", target_arch = "x86_64"))]
            test_simd_arch_mod!(x86_64, __m512bh, __m512, __m512d, __m512i);

            #[cfg(target_arch = "wasm32")]
            test_simd_arch_mod!(wasm32, v128);

            #[cfg(all(feature = "simd-nightly", target_arch = "powerpc"))]
            test_simd_arch_mod!(
                powerpc,
                vector_bool_long,
                vector_double,
                vector_signed_long,
                vector_unsigned_long
            );

            #[cfg(all(feature = "simd-nightly", target_arch = "powerpc64"))]
            test_simd_arch_mod!(
                powerpc64,
                vector_bool_long,
                vector_double,
                vector_signed_long,
                vector_unsigned_long
            );
            #[cfg(target_arch = "aarch64")]
            #[rustfmt::skip]
            test_simd_arch_mod!(
                aarch64, float32x2_t, float32x4_t, float64x1_t, float64x2_t, int8x8_t, int8x8x2_t,
                int8x8x3_t, int8x8x4_t, int8x16_t, int8x16x2_t, int8x16x3_t, int8x16x4_t, int16x4_t,
                int16x8_t, int32x2_t, int32x4_t, int64x1_t, int64x2_t, poly8x8_t, poly8x8x2_t, poly8x8x3_t,
                poly8x8x4_t, poly8x16_t, poly8x16x2_t, poly8x16x3_t, poly8x16x4_t, poly16x4_t, poly16x8_t,
                poly64x1_t, poly64x2_t, uint8x8_t, uint8x8x2_t, uint8x8x3_t, uint8x8x4_t, uint8x16_t,
                uint8x16x2_t, uint8x16x3_t, uint8x16x4_t, uint16x4_t, uint16x8_t, uint32x2_t, uint32x4_t,
                uint64x1_t, uint64x2_t
            );
            #[cfg(all(feature = "simd-nightly", target_arch = "arm"))]
            #[rustfmt::skip]
            test_simd_arch_mod!(arm, int8x4_t, uint8x4_t);
        }
    }
}

#[cfg(kani)]
mod proofs {
    use super::*;

    impl kani::Arbitrary for DstLayout {
        fn any() -> Self {
            let align: NonZeroUsize = kani::any();
            let size_info: SizeInfo = kani::any();

            kani::assume(align.is_power_of_two());
            kani::assume(align < DstLayout::THEORETICAL_MAX_ALIGN);

            // For testing purposes, we most care about instantiations of
            // `DstLayout` that can correspond to actual Rust types. We use
            // `Layout` to verify that our `DstLayout` satisfies the validity
            // conditions of Rust layouts.
            kani::assume(
                match size_info {
                    SizeInfo::Sized { _size } => Layout::from_size_align(_size, align.get()),
                    SizeInfo::SliceDst(TrailingSliceLayout { _offset, _elem_size }) => {
                        // `SliceDst`` cannot encode an exact size, but we know
                        // it is at least `_offset` bytes.
                        Layout::from_size_align(_offset, align.get())
                    }
                }
                .is_ok(),
            );

            Self { align: align, size_info: size_info }
        }
    }

    impl kani::Arbitrary for SizeInfo {
        fn any() -> Self {
            let is_sized: bool = kani::any();

            match is_sized {
                true => {
                    let size: usize = kani::any();

                    kani::assume(size <= isize::MAX as _);

                    SizeInfo::Sized { _size: size }
                }
                false => SizeInfo::SliceDst(kani::any()),
            }
        }
    }

    impl kani::Arbitrary for TrailingSliceLayout {
        fn any() -> Self {
            let elem_size: usize = kani::any();
            let offset: usize = kani::any();

            kani::assume(elem_size < isize::MAX as _);
            kani::assume(offset < isize::MAX as _);

            TrailingSliceLayout { _elem_size: elem_size, _offset: offset }
        }
    }

    #[kani::proof]
    fn prove_dst_layout_extend() {
        use crate::util::{core_layout::padding_needed_for, max, min};

        let base: DstLayout = kani::any();
        let field: DstLayout = kani::any();
        let packed: Option<NonZeroUsize> = kani::any();

        if let Some(max_align) = packed {
            kani::assume(max_align.is_power_of_two());
            kani::assume(base.align <= max_align);
        }

        // The base can only be extended if it's sized.
        kani::assume(matches!(base.size_info, SizeInfo::Sized { .. }));
        let base_size = if let SizeInfo::Sized { _size: size } = base.size_info {
            size
        } else {
            unreachable!();
        };

        // Under the above conditions, `DstLayout::extend` will not panic.
        let composite = base.extend(field, packed);

        // The field's alignment is clamped by `max_align` (i.e., the
        // `packed` attribute, if any) [1].
        //
        // [1] Per https://doc.rust-lang.org/reference/type-layout.html#the-alignment-modifiers:
        //
        //   The alignments of each field, for the purpose of positioning
        //   fields, is the smaller of the specified alignment and the
        //   alignment of the field's type.
        let field_align = min(field.align, packed.unwrap_or(DstLayout::THEORETICAL_MAX_ALIGN));

        // The struct's alignment is the maximum of its previous alignment and
        // `field_align`.
        assert_eq!(composite.align, max(base.align, field_align));

        // Compute the minimum amount of inter-field padding needed to
        // satisfy the field's alignment, and offset of the trailing field.
        // [1]
        //
        // [1] Per https://doc.rust-lang.org/reference/type-layout.html#the-alignment-modifiers:
        //
        //   Inter-field padding is guaranteed to be the minimum required in
        //   order to satisfy each field's (possibly altered) alignment.
        let padding = padding_needed_for(base_size, field_align);
        let offset = base_size + padding;

        // For testing purposes, we'll also construct `alloc::Layout`
        // stand-ins for `DstLayout`, and show that `extend` behaves
        // comparably on both types.
        let base_analog = Layout::from_size_align(base_size, base.align.get()).unwrap();

        match field.size_info {
            SizeInfo::Sized { _size: field_size } => {
                if let SizeInfo::Sized { _size: composite_size } = composite.size_info {
                    // If the trailing field is sized, the resulting layout
                    // will be sized. Its size will be the sum of the
                    // preceeding layout, the size of the new field, and the
                    // size of inter-field padding between the two.
                    assert_eq!(composite_size, offset + field_size);

                    let field_analog =
                        Layout::from_size_align(field_size, field_align.get()).unwrap();

                    if let Ok((actual_composite, actual_offset)) = base_analog.extend(field_analog)
                    {
                        assert_eq!(actual_offset, offset);
                        assert_eq!(actual_composite.size(), composite_size);
                        assert_eq!(actual_composite.align(), composite.align.get());
                    } else {
                        // An error here reflects that composite of `base`
                        // and `field` cannot correspond to a real Rust type
                        // fragment, because such a fragment would violate
                        // the basic invariants of a valid Rust layout. At
                        // the time of writing, `DstLayout` is a little more
                        // permissive than `Layout`, so we don't assert
                        // anything in this branch (e.g., unreachability).
                    }
                } else {
                    panic!("The composite of two sized layouts must be sized.")
                }
            }
            SizeInfo::SliceDst(TrailingSliceLayout {
                _offset: field_offset,
                _elem_size: field_elem_size,
            }) => {
                if let SizeInfo::SliceDst(TrailingSliceLayout {
                    _offset: composite_offset,
                    _elem_size: composite_elem_size,
                }) = composite.size_info
                {
                    // The offset of the trailing slice component is the sum
                    // of the offset of the trailing field and the trailing
                    // slice offset within that field.
                    assert_eq!(composite_offset, offset + field_offset);
                    // The elem size is unchanged.
                    assert_eq!(composite_elem_size, field_elem_size);

                    let field_analog =
                        Layout::from_size_align(field_offset, field_align.get()).unwrap();

                    if let Ok((actual_composite, actual_offset)) = base_analog.extend(field_analog)
                    {
                        assert_eq!(actual_offset, offset);
                        assert_eq!(actual_composite.size(), composite_offset);
                        assert_eq!(actual_composite.align(), composite.align.get());
                    } else {
                        // An error here reflects that composite of `base`
                        // and `field` cannot correspond to a real Rust type
                        // fragment, because such a fragment would violate
                        // the basic invariants of a valid Rust layout. At
                        // the time of writing, `DstLayout` is a little more
                        // permissive than `Layout`, so we don't assert
                        // anything in this branch (e.g., unreachability).
                    }
                } else {
                    panic!("The extension of a layout with a DST must result in a DST.")
                }
            }
        }
    }

    #[kani::proof]
    #[kani::should_panic]
    fn prove_dst_layout_extend_dst_panics() {
        let base: DstLayout = kani::any();
        let field: DstLayout = kani::any();
        let packed: Option<NonZeroUsize> = kani::any();

        if let Some(max_align) = packed {
            kani::assume(max_align.is_power_of_two());
            kani::assume(base.align <= max_align);
        }

        kani::assume(matches!(base.size_info, SizeInfo::SliceDst(..)));

        let _ = base.extend(field, packed);
    }

    #[kani::proof]
    fn prove_dst_layout_pad_to_align() {
        use crate::util::core_layout::padding_needed_for;

        let layout: DstLayout = kani::any();

        let padded: DstLayout = layout.pad_to_align();

        // Calling `pad_to_align` does not alter the `DstLayout`'s alignment.
        assert_eq!(padded.align, layout.align);

        if let SizeInfo::Sized { _size: unpadded_size } = layout.size_info {
            if let SizeInfo::Sized { _size: padded_size } = padded.size_info {
                // If the layout is sized, it will remain sized after padding is
                // added. Its sum will be its unpadded size and the size of the
                // trailing padding needed to satisfy its alignment
                // requirements.
                let padding = padding_needed_for(unpadded_size, layout.align);
                assert_eq!(padded_size, unpadded_size + padding);

                // Prove that calling `DstLayout::pad_to_align` behaves
                // identically to `Layout::pad_to_align`.
                let layout_analog =
                    Layout::from_size_align(unpadded_size, layout.align.get()).unwrap();
                let padded_analog = layout_analog.pad_to_align();
                assert_eq!(padded_analog.align(), layout.align.get());
                assert_eq!(padded_analog.size(), padded_size);
            } else {
                panic!("The padding of a sized layout must result in a sized layout.")
            }
        } else {
            // If the layout is a DST, padding cannot be statically added.
            assert_eq!(padded.size_info, layout.size_info);
        }
    }
}
