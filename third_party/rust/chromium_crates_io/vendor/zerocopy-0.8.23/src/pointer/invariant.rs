// Copyright 2024 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

#![allow(missing_copy_implementations, missing_debug_implementations)]

//! The parameterized invariants of a [`Ptr`][super::Ptr].
//!
//! Invariants are encoded as ([`Aliasing`], [`Alignment`], [`Validity`])
//! triples implementing the [`Invariants`] trait.

/// The invariants of a [`Ptr`][super::Ptr].
pub trait Invariants: Sealed {
    type Aliasing: Aliasing;
    type Alignment: Alignment;
    type Validity: Validity;
}

impl<A: Aliasing, AA: Alignment, V: Validity> Invariants for (A, AA, V) {
    type Aliasing = A;
    type Alignment = AA;
    type Validity = V;
}

/// The aliasing invariant of a [`Ptr`][super::Ptr].
///
/// All aliasing invariants must permit reading from the bytes of a pointer's
/// referent which are not covered by [`UnsafeCell`]s.
///
/// [`UnsafeCell`]: core::cell::UnsafeCell
pub trait Aliasing: Sealed {
    /// Is `Self` [`Exclusive`]?
    #[doc(hidden)]
    const IS_EXCLUSIVE: bool;
}

/// The alignment invariant of a [`Ptr`][super::Ptr].
pub trait Alignment: Sealed {}

/// The validity invariant of a [`Ptr`][super::Ptr].
pub trait Validity: Sealed {}

/// An [`Aliasing`] invariant which is either [`Shared`] or [`Exclusive`].
///
/// # Safety
///
/// Given `A: Reference`, callers may assume that either `A = Shared` or `A =
/// Exclusive`.
pub trait Reference: Aliasing + Sealed {}

/// The `Ptr<'a, T>` adheres to the aliasing rules of a `&'a T`.
///
/// The referent of a shared-aliased `Ptr` may be concurrently referenced by any
/// number of shared-aliased `Ptr` or `&T` references, and may not be
/// concurrently referenced by any exclusively-aliased `Ptr`s or `&mut T`
/// references. The referent must not be mutated, except via [`UnsafeCell`]s.
///
/// [`UnsafeCell`]: core::cell::UnsafeCell
pub enum Shared {}
impl Aliasing for Shared {
    const IS_EXCLUSIVE: bool = false;
}
impl Reference for Shared {}

/// The `Ptr<'a, T>` adheres to the aliasing rules of a `&'a mut T`.
///
/// The referent of an exclusively-aliased `Ptr` may not be concurrently
/// referenced by any other `Ptr`s or references, and may not be accessed (read
/// or written) other than via this `Ptr`.
pub enum Exclusive {}
impl Aliasing for Exclusive {
    const IS_EXCLUSIVE: bool = true;
}
impl Reference for Exclusive {}

/// It is unknown whether the pointer is aligned.
pub enum Unaligned {}

impl Alignment for Unaligned {}

/// The referent is aligned: for `Ptr<T>`, the referent's address is a multiple
/// of the `T`'s alignment.
pub enum Aligned {}
impl Alignment for Aligned {}

/// Any bit pattern is allowed in the `Ptr`'s referent, including uninitialized
/// bytes.
pub enum Uninit {}
impl Validity for Uninit {}

/// The byte ranges initialized in `T` are also initialized in the referent.
///
/// Formally: uninitialized bytes may only be present in `Ptr<T>`'s referent
/// where they are guaranteed to be present in `T`. This is a dynamic property:
/// if, at a particular byte offset, a valid enum discriminant is set, the
/// subsequent bytes may only have uninitialized bytes as specificed by the
/// corresponding enum.
///
/// Formally, given `len = size_of_val_raw(ptr)`, at every byte offset, `b`, in
/// the range `[0, len)`:
/// - If, in any instance `t: T` of length `len`, the byte at offset `b` in `t`
///   is initialized, then the byte at offset `b` within `*ptr` must be
///   initialized.
/// - Let `c` be the contents of the byte range `[0, b)` in `*ptr`. Let `S` be
///   the subset of valid instances of `T` of length `len` which contain `c` in
///   the offset range `[0, b)`. If, in any instance of `t: T` in `S`, the byte
///   at offset `b` in `t` is initialized, then the byte at offset `b` in `*ptr`
///   must be initialized.
///
///   Pragmatically, this means that if `*ptr` is guaranteed to contain an enum
///   type at a particular offset, and the enum discriminant stored in `*ptr`
///   corresponds to a valid variant of that enum type, then it is guaranteed
///   that the appropriate bytes of `*ptr` are initialized as defined by that
///   variant's bit validity (although note that the variant may contain another
///   enum type, in which case the same rules apply depending on the state of
///   its discriminant, and so on recursively).
pub enum AsInitialized {}
impl Validity for AsInitialized {}

/// The byte ranges in the referent are fully initialized. In other words, if
/// the referent is `N` bytes long, then it contains a bit-valid `[u8; N]`.
pub enum Initialized {}
impl Validity for Initialized {}

/// The referent is bit-valid for `T`.
pub enum Valid {}
impl Validity for Valid {}

/// # Safety
///
/// `DT: CastableFrom<ST, SV, DV>` is sound if `SV = DV = Uninit` or `SV = DV =
/// Initialized`.
pub unsafe trait CastableFrom<ST: ?Sized, SV, DV> {}

// SAFETY: `SV = DV = Uninit`.
unsafe impl<ST: ?Sized, DT: ?Sized> CastableFrom<ST, Uninit, Uninit> for DT {}
// SAFETY: `SV = DV = Initialized`.
unsafe impl<ST: ?Sized, DT: ?Sized> CastableFrom<ST, Initialized, Initialized> for DT {}

/// [`Ptr`](crate::Ptr) referents that permit unsynchronized read operations.
///
/// `T: Read<A, R>` implies that a pointer to `T` with aliasing `A` permits
/// unsynchronized read oeprations. This can be because `A` is [`Exclusive`] or
/// because `T` does not permit interior mutation.
///
/// # Safety
///
/// `T: Read<A, R>` if either of the following conditions holds:
/// - `A` is [`Exclusive`]
/// - `T` implements [`Immutable`](crate::Immutable)
///
/// As a consequence, if `T: Read<A, R>`, then any `Ptr<T, (A, ...)>` is
/// permitted to perform unsynchronized reads from its referent.
pub trait Read<A: Aliasing, R> {}

impl<A: Aliasing, T: ?Sized + crate::Immutable> Read<A, BecauseImmutable> for T {}
impl<T: ?Sized> Read<Exclusive, BecauseExclusive> for T {}

/// Unsynchronized reads are permitted because only one live [`Ptr`](crate::Ptr)
/// or reference may exist to the referent bytes at a time.
#[derive(Copy, Clone, Debug)]
#[doc(hidden)]
pub enum BecauseExclusive {}

/// Unsynchronized reads are permitted because no live [`Ptr`](crate::Ptr)s or
/// references permit interior mutation.
#[derive(Copy, Clone, Debug)]
#[doc(hidden)]
pub enum BecauseImmutable {}

use sealed::Sealed;
mod sealed {
    use super::*;

    pub trait Sealed {}

    impl Sealed for Shared {}
    impl Sealed for Exclusive {}

    impl Sealed for Unaligned {}
    impl Sealed for Aligned {}

    impl Sealed for Uninit {}
    impl Sealed for AsInitialized {}
    impl Sealed for Initialized {}
    impl Sealed for Valid {}

    impl<A: Sealed, AA: Sealed, V: Sealed> Sealed for (A, AA, V) {}

    impl Sealed for BecauseImmutable {}
    impl Sealed for BecauseExclusive {}
}
