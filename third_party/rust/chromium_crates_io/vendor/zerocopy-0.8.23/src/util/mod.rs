// Copyright 2023 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

#[macro_use]
mod macros;

#[doc(hidden)]
pub mod macro_util;

use core::{
    cell::UnsafeCell,
    marker::PhantomData,
    mem::{self, ManuallyDrop, MaybeUninit},
    num::{NonZeroUsize, Wrapping},
    ptr::NonNull,
};

use crate::{
    error::AlignmentError,
    pointer::invariant::{self, Invariants},
    Unalign,
};

/// A type which has the same layout as the type it wraps.
///
/// # Safety
///
/// `T: TransparentWrapper` implies that `T` has the same size as [`T::Inner`].
/// Further, `T: TransparentWrapper<I>` implies that:
/// - If `T::UnsafeCellVariance = Covariant`, then `T` has `UnsafeCell`s
///   covering the same byte ranges as `T::Inner`.
/// - If a `T` pointer satisfies the alignment invariant `I::Alignment`, then
///   that same pointer, cast to `T::Inner`, satisfies the alignment invariant
///   `<T::AlignmentVariance as AlignmentVariance<I::Alignment>>::Applied`.
/// - If a `T` pointer satisfies the validity invariant `I::Validity`, then that
///   same pointer, cast to `T::Inner`, satisfies the validity invariant
///   `<T::ValidityVariance as ValidityVariance<I::Validity>>::Applied`.
///
/// [`T::Inner`]: TransparentWrapper::Inner
/// [`UnsafeCell`]: core::cell::UnsafeCell
/// [`T::AlignmentVariance`]: TransparentWrapper::AlignmentVariance
/// [`T::ValidityVariance`]: TransparentWrapper::ValidityVariance
#[doc(hidden)]
pub unsafe trait TransparentWrapper<I: Invariants> {
    type Inner: ?Sized;

    type UnsafeCellVariance;
    type AlignmentVariance: AlignmentVariance<I::Alignment>;
    type ValidityVariance: ValidityVariance<I::Validity>;

    /// Casts a wrapper pointer to an inner pointer.
    ///
    /// # Safety
    ///
    /// The resulting pointer has the same address and provenance as `ptr`, and
    /// addresses the same number of bytes.
    fn cast_into_inner(ptr: *mut Self) -> *mut Self::Inner;

    /// Casts an inner pointer to a wrapper pointer.
    ///
    /// # Safety
    ///
    /// The resulting pointer has the same address and provenance as `ptr`, and
    /// addresses the same number of bytes.
    fn cast_from_inner(ptr: *mut Self::Inner) -> *mut Self;
}

#[allow(unreachable_pub)]
#[doc(hidden)]
pub trait AlignmentVariance<I: invariant::Alignment> {
    type Applied: invariant::Alignment;
}

#[allow(unreachable_pub)]
#[doc(hidden)]
pub trait ValidityVariance<I: invariant::Validity> {
    type Applied: invariant::Validity;
}

#[doc(hidden)]
#[allow(missing_copy_implementations, missing_debug_implementations)]
pub enum Covariant {}

impl<I: invariant::Alignment> AlignmentVariance<I> for Covariant {
    type Applied = I;
}

impl<I: invariant::Validity> ValidityVariance<I> for Covariant {
    type Applied = I;
}

#[doc(hidden)]
#[allow(missing_copy_implementations, missing_debug_implementations)]
pub enum Invariant {}

impl<I: invariant::Alignment> AlignmentVariance<I> for Invariant {
    type Applied = invariant::Unaligned;
}

impl<I: invariant::Validity> ValidityVariance<I> for Invariant {
    type Applied = invariant::Uninit;
}

// SAFETY:
// - Per [1], `MaybeUninit<T>` has the same size as `T`.
// - See inline comments for other safety justifications.
//
// [1] Per https://doc.rust-lang.org/1.81.0/std/mem/union.MaybeUninit.html#layout-1:
//
//   `MaybeUninit<T>` is guaranteed to have the same size, alignment, and ABI as
//   `T`
unsafe impl<T, I: Invariants> TransparentWrapper<I> for MaybeUninit<T> {
    type Inner = T;

    // SAFETY: `MaybeUninit<T>` has `UnsafeCell`s covering the same byte ranges
    // as `Inner = T`. This is not explicitly documented, but it can be
    // inferred. Per [1] in the preceding safety comment, `MaybeUninit<T>` has
    // the same size as `T`. Further, note the signature of
    // `MaybeUninit::assume_init_ref` [2]:
    //
    //   pub unsafe fn assume_init_ref(&self) -> &T
    //
    // If the argument `&MaybeUninit<T>` and the returned `&T` had `UnsafeCell`s
    // at different offsets, this would be unsound. Its existence is proof that
    // this is not the case.
    //
    // [2] https://doc.rust-lang.org/1.81.0/std/mem/union.MaybeUninit.html#method.assume_init_ref
    type UnsafeCellVariance = Covariant;
    // SAFETY: Per [1], `MaybeUninit<T>` has the same layout as `T`, and thus
    // has the same alignment as `T`.
    //
    // [1] Per https://doc.rust-lang.org/std/mem/union.MaybeUninit.html#layout-1:
    //
    //   `MaybeUninit<T>` is guaranteed to have the same size, alignment, and
    //   ABI as `T`.
    type AlignmentVariance = Covariant;
    // SAFETY: `MaybeUninit` has no validity invariants. Thus, a valid
    // `MaybeUninit<T>` is not necessarily a valid `T`.
    type ValidityVariance = Invariant;

    #[inline(always)]
    fn cast_into_inner(ptr: *mut MaybeUninit<T>) -> *mut T {
        // SAFETY: Per [1] (from comment above), `MaybeUninit<T>` has the same
        // layout as `T`. Thus, this cast preserves size.
        //
        // This cast trivially preserves provenance.
        ptr.cast::<T>()
    }

    #[inline(always)]
    fn cast_from_inner(ptr: *mut T) -> *mut MaybeUninit<T> {
        // SAFETY: Per [1] (from comment above), `MaybeUninit<T>` has the same
        // layout as `T`. Thus, this cast preserves size.
        //
        // This cast trivially preserves provenance.
        ptr.cast::<MaybeUninit<T>>()
    }
}

// SAFETY:
// - Per [1], `ManuallyDrop<T>` has the same size as `T`.
// - See inline comments for other safety justifications.
//
// [1] Per https://doc.rust-lang.org/1.81.0/std/mem/struct.ManuallyDrop.html:
//
//   `ManuallyDrop<T>` is guaranteed to have the same layout and bit validity as
//   `T`
unsafe impl<T: ?Sized, I: Invariants> TransparentWrapper<I> for ManuallyDrop<T> {
    type Inner = T;

    // SAFETY: Per [1], `ManuallyDrop<T>` has `UnsafeCell`s covering the same
    // byte ranges as `Inner = T`.
    //
    // [1] Per https://doc.rust-lang.org/1.81.0/std/mem/struct.ManuallyDrop.html:
    //
    //   `ManuallyDrop<T>` is guaranteed to have the same layout and bit
    //   validity as `T`, and is subject to the same layout optimizations as
    //   `T`. As a consequence, it has no effect on the assumptions that the
    //   compiler makes about its contents.
    type UnsafeCellVariance = Covariant;
    // SAFETY: Per [1], `ManuallyDrop<T>` has the same layout as `T`, and thus
    // has the same alignment as `T`.
    //
    // [1] Per https://doc.rust-lang.org/nightly/core/mem/struct.ManuallyDrop.html:
    //
    //   `ManuallyDrop<T>` is guaranteed to have the same layout and bit
    //   validity as `T`
    type AlignmentVariance = Covariant;

    // SAFETY: Per [1] (from comment above), `ManuallyDrop<T>` has the same bit
    // validity as `T`.
    type ValidityVariance = Covariant;

    #[inline(always)]
    fn cast_into_inner(ptr: *mut ManuallyDrop<T>) -> *mut T {
        // SAFETY: Per [1] (from comment above), `ManuallyDrop<T>` has the same
        // layout as `T`. Thus, this cast preserves size even if `T` is unsized.
        //
        // This cast trivially preserves provenance.
        #[allow(clippy::as_conversions)]
        return ptr as *mut T;
    }

    #[inline(always)]
    fn cast_from_inner(ptr: *mut T) -> *mut ManuallyDrop<T> {
        // SAFETY: Per [1] (from comment above), `ManuallyDrop<T>` has the same
        // layout as `T`. Thus, this cast preserves size even if `T` is unsized.
        //
        // This cast trivially preserves provenance.
        #[allow(clippy::as_conversions)]
        return ptr as *mut ManuallyDrop<T>;
    }
}

// SAFETY:
// - Per [1], `Wrapping<T>` has the same size as `T`.
// - See inline comments for other safety justifications.
//
// [1] Per https://doc.rust-lang.org/1.81.0/std/num/struct.Wrapping.html#layout-1:
//
//   `Wrapping<T>` is guaranteed to have the same layout and ABI as `T`.
unsafe impl<T, I: Invariants> TransparentWrapper<I> for Wrapping<T> {
    type Inner = T;

    // SAFETY: Per [1], `Wrapping<T>` has the same layout as `T`. Since its
    // single field (of type `T`) is public, it would be a breaking change to
    // add or remove fields. Thus, we know that `Wrapping<T>` contains a `T` (as
    // opposed to just having the same size and alignment as `T`) with no pre-
    // or post-padding. Thus, `Wrapping<T>` must have `UnsafeCell`s covering the
    // same byte ranges as `Inner = T`.
    //
    // [1] Per https://doc.rust-lang.org/1.81.0/std/num/struct.Wrapping.html#layout-1:
    //
    //   `Wrapping<T>` is guaranteed to have the same layout and ABI as `T`.
    type UnsafeCellVariance = Covariant;
    // SAFETY: Per [1], `Wrapping<T>` has the same layout as `T`, and thus has
    // the same alignment as `T`.
    //
    // [1] Per https://doc.rust-lang.org/core/num/struct.Wrapping.html#layout-1:
    //
    //   `Wrapping<T>` is guaranteed to have the same layout and ABI as `T`.
    type AlignmentVariance = Covariant;

    // SAFETY: `Wrapping<T>` has only one field, which is `pub` [2]. We are also
    // guaranteed per [1] (from the comment above) that `Wrapping<T>` has the
    // same layout as `T`. The only way for both of these to be true
    // simultaneously is for `Wrapping<T>` to have the same bit validity as `T`.
    // In particular, in order to change the bit validity, one of the following
    // would need to happen:
    // - `Wrapping` could change its `repr`, but this would violate the layout
    //   guarantee.
    // - `Wrapping` could add or change its fields, but this would be a
    //   stability-breaking change.
    //
    // [2] https://doc.rust-lang.org/core/num/struct.Wrapping.html
    type ValidityVariance = Covariant;

    #[inline(always)]
    fn cast_into_inner(ptr: *mut Wrapping<T>) -> *mut T {
        // SAFETY: Per [1] (from comment above), `Wrapping<T>` has the same
        // layout as `T`. Thus, this cast preserves size.
        //
        // This cast trivially preserves provenance.
        ptr.cast::<T>()
    }

    #[inline(always)]
    fn cast_from_inner(ptr: *mut T) -> *mut Wrapping<T> {
        // SAFETY: Per [1] (from comment above), `Wrapping<T>` has the same
        // layout as `T`. Thus, this cast preserves size.
        //
        // This cast trivially preserves provenance.
        ptr.cast::<Wrapping<T>>()
    }
}

// SAFETY:
// - Per [1], `UnsafeCell<T>` has the same size as `T`.
// - See inline comments for other safety justifications.
//
// [1] Per https://doc.rust-lang.org/1.81.0/core/cell/struct.UnsafeCell.html#memory-layout:
//
//   `UnsafeCell<T>` has the same in-memory representation as its inner type
//   `T`.
unsafe impl<T: ?Sized, I: Invariants> TransparentWrapper<I> for UnsafeCell<T> {
    type Inner = T;

    // SAFETY: Since we set this to `Invariant`, we make no safety claims.
    type UnsafeCellVariance = Invariant;

    // SAFETY: Per [1] (from comment on impl), `Unalign<T>` has the same
    // representation as `T`, and thus has the same alignment as `T`.
    type AlignmentVariance = Covariant;

    // SAFETY: Per [1], `Unalign<T>` has the same bit validity as `T`.
    // Technically the term "representation" doesn't guarantee this, but the
    // subsequent sentence in the documentation makes it clear that this is the
    // intention.
    //
    // [1] Per https://doc.rust-lang.org/1.81.0/core/cell/struct.UnsafeCell.html#memory-layout:
    //
    //   `UnsafeCell<T>` has the same in-memory representation as its inner type
    //   `T`. A consequence of this guarantee is that it is possible to convert
    //   between `T` and `UnsafeCell<T>`.
    type ValidityVariance = Covariant;

    #[inline(always)]
    fn cast_into_inner(ptr: *mut UnsafeCell<T>) -> *mut T {
        // SAFETY: Per [1] (from comment above), `UnsafeCell<T>` has the same
        // representation as `T`. Thus, this cast preserves size.
        //
        // This cast trivially preserves provenance.
        #[allow(clippy::as_conversions)]
        return ptr as *mut T;
    }

    #[inline(always)]
    fn cast_from_inner(ptr: *mut T) -> *mut UnsafeCell<T> {
        // SAFETY: Per [1] (from comment above), `UnsafeCell<T>` has the same
        // representation as `T`. Thus, this cast preserves size.
        //
        // This cast trivially preserves provenance.
        #[allow(clippy::as_conversions)]
        return ptr as *mut UnsafeCell<T>;
    }
}

// SAFETY: `Unalign<T>` promises to have the same size as `T`.
//
// See inline comments for other safety justifications.
unsafe impl<T, I: Invariants> TransparentWrapper<I> for Unalign<T> {
    type Inner = T;

    // SAFETY: `Unalign<T>` promises to have `UnsafeCell`s covering the same
    // byte ranges as `Inner = T`.
    type UnsafeCellVariance = Covariant;

    // SAFETY: Since `Unalign<T>` promises to have alignment 1 regardless of
    // `T`'s alignment. Thus, an aligned pointer to `Unalign<T>` is not
    // necessarily an aligned pointer to `T`.
    type AlignmentVariance = Invariant;

    // SAFETY: `Unalign<T>` promises to have the same validity as `T`.
    type ValidityVariance = Covariant;

    #[inline(always)]
    fn cast_into_inner(ptr: *mut Unalign<T>) -> *mut T {
        // SAFETY: Per the safety comment on the impl block, `Unalign<T>` has
        // the size as `T`. Thus, this cast preserves size.
        //
        // This cast trivially preserves provenance.
        ptr.cast::<T>()
    }

    #[inline(always)]
    fn cast_from_inner(ptr: *mut T) -> *mut Unalign<T> {
        // SAFETY: Per the safety comment on the impl block, `Unalign<T>` has
        // the size as `T`. Thus, this cast preserves size.
        //
        // This cast trivially preserves provenance.
        ptr.cast::<Unalign<T>>()
    }
}

/// Implements `TransparentWrapper` for an atomic type.
///
/// # Safety
///
/// The caller promises that `$atomic` is an atomic type whose natie equivalent
/// is `$native`.
#[cfg(all(
    zerocopy_target_has_atomics_1_60_0,
    any(
        target_has_atomic = "8",
        target_has_atomic = "16",
        target_has_atomic = "32",
        target_has_atomic = "64",
        target_has_atomic = "ptr"
    )
))]
macro_rules! unsafe_impl_transparent_wrapper_for_atomic {
    ($(#[$attr:meta])* $(,)?) => {};
    ($(#[$attr:meta])* $atomic:ty [$native:ty], $($atomics:ty [$natives:ty]),* $(,)?) => {
        $(#[$attr])*
        // SAFETY: See safety comment in next match arm.
        unsafe impl<I: crate::invariant::Invariants> crate::util::TransparentWrapper<I> for $atomic {
            unsafe_impl_transparent_wrapper_for_atomic!(@inner $atomic [$native]);
        }
        unsafe_impl_transparent_wrapper_for_atomic!($(#[$attr])* $($atomics [$natives],)*);
    };
    ($(#[$attr:meta])* $tyvar:ident => $atomic:ty [$native:ty]) => {
        // We implement for `$atomic` and set `Inner = $native`. The caller has
        // promised that `$atomic` and `$native` are an atomic type and its
        // native counterpart, respectively. Per [1], `$atomic` and `$native`
        // have the same size.
        //
        // [1] Per (for example) https://doc.rust-lang.org/1.81.0/std/sync/atomic/struct.AtomicU64.html:
        //
        //   This type has the same size and bit validity as the underlying
        //   integer type
        $(#[$attr])*
        unsafe impl<$tyvar, I: crate::invariant::Invariants> crate::util::TransparentWrapper<I> for $atomic {
            unsafe_impl_transparent_wrapper_for_atomic!(@inner $atomic [$native]);
        }
    };
    (@inner $atomic:ty [$native:ty]) => {
        type Inner = UnsafeCell<$native>;

        // SAFETY: It is "obvious" that each atomic type contains a single
        // `UnsafeCell` that covers all bytes of the type, but we can also prove
        // it:
        // - Since `$atomic` provides an API which permits loading and storing
        //   values of type `$native` via a `&self` (shared) reference, *some*
        //   interior mutation must be happening, and interior mutation can only
        //   happen via `UnsafeCell`. Further, there must be enough bytes in
        //   `$atomic` covered by an `UnsafeCell` to hold every possible value
        //   of `$native`.
        // - Per [1], `$atomic` has the same size as `$native`. This on its own
        //   isn't enough: it would still be possible for `$atomic` to store
        //   `$native` using a compact representation (for `$native` types for
        //   which some bit patterns are illegal). However, this is ruled out by
        //   the fact that `$atomic` has the same bit validity as `$native` [1].
        //   Thus, we can conclude that every byte of `$atomic` must be covered
        //   by an `UnsafeCell`.
        //
        // Thus, every byte of `$atomic` is covered by an `UnsafeCell`, and we
        // set `type Inner = UnsafeCell<$native>`. Thus, `Self` and
        // `Self::Inner` have `UnsafeCell`s covering the same byte ranges.
        //
        // [1] Per (for example) https://doc.rust-lang.org/1.81.0/std/sync/atomic/struct.AtomicU64.html:
        //
        //   This type has the same size and bit validity as the underlying
        //   integer type
        type UnsafeCellVariance = crate::util::Covariant;

        // SAFETY: No safety justification is required for an invariant
        // variance.
        type AlignmentVariance = crate::util::Invariant;

        // SAFETY: Per [1], all atomic types have the same bit validity as their
        // native counterparts. The caller has promised that `$atomic` and
        // `$native` are an atomic type and its native counterpart,
        // respectively.
        //
        // [1] Per (for example) https://doc.rust-lang.org/1.81.0/std/sync/atomic/struct.AtomicU64.html:
        //
        //   This type has the same size and bit validity as the underlying
        //   integer type
        type ValidityVariance = crate::util::Covariant;

        #[inline(always)]
        fn cast_into_inner(ptr: *mut $atomic) -> *mut UnsafeCell<$native> {
            // SAFETY: Per [1] (from comment on impl block), `$atomic` has the
            // same size as `$native`. Thus, this cast preserves size.
            //
            // This cast trivially preserves provenance.
            ptr.cast::<UnsafeCell<$native>>()
        }

        #[inline(always)]
        fn cast_from_inner(ptr: *mut UnsafeCell<$native>) -> *mut $atomic {
            // SAFETY: Per [1] (from comment on impl block), `$atomic` has the
            // same size as `$native`. Thus, this cast preserves size.
            //
            // This cast trivially preserves provenance.
            ptr.cast::<$atomic>()
        }
    };
}

/// Like [`PhantomData`], but [`Send`] and [`Sync`] regardless of whether the
/// wrapped `T` is.
pub(crate) struct SendSyncPhantomData<T: ?Sized>(PhantomData<T>);

// SAFETY: `SendSyncPhantomData` does not enable any behavior which isn't sound
// to be called from multiple threads.
unsafe impl<T: ?Sized> Send for SendSyncPhantomData<T> {}
// SAFETY: `SendSyncPhantomData` does not enable any behavior which isn't sound
// to be called from multiple threads.
unsafe impl<T: ?Sized> Sync for SendSyncPhantomData<T> {}

impl<T: ?Sized> Default for SendSyncPhantomData<T> {
    fn default() -> SendSyncPhantomData<T> {
        SendSyncPhantomData(PhantomData)
    }
}

impl<T: ?Sized> PartialEq for SendSyncPhantomData<T> {
    fn eq(&self, other: &Self) -> bool {
        self.0.eq(&other.0)
    }
}

impl<T: ?Sized> Eq for SendSyncPhantomData<T> {}

pub(crate) trait AsAddress {
    fn addr(self) -> usize;
}

impl<T: ?Sized> AsAddress for &T {
    #[inline(always)]
    fn addr(self) -> usize {
        let ptr: *const T = self;
        AsAddress::addr(ptr)
    }
}

impl<T: ?Sized> AsAddress for &mut T {
    #[inline(always)]
    fn addr(self) -> usize {
        let ptr: *const T = self;
        AsAddress::addr(ptr)
    }
}

impl<T: ?Sized> AsAddress for NonNull<T> {
    #[inline(always)]
    fn addr(self) -> usize {
        AsAddress::addr(self.as_ptr())
    }
}

impl<T: ?Sized> AsAddress for *const T {
    #[inline(always)]
    fn addr(self) -> usize {
        // TODO(#181), TODO(https://github.com/rust-lang/rust/issues/95228): Use
        // `.addr()` instead of `as usize` once it's stable, and get rid of this
        // `allow`. Currently, `as usize` is the only way to accomplish this.
        #[allow(clippy::as_conversions)]
        #[cfg_attr(
            __ZEROCOPY_INTERNAL_USE_ONLY_NIGHTLY_FEATURES_IN_TESTS,
            allow(lossy_provenance_casts)
        )]
        return self.cast::<()>() as usize;
    }
}

impl<T: ?Sized> AsAddress for *mut T {
    #[inline(always)]
    fn addr(self) -> usize {
        let ptr: *const T = self;
        AsAddress::addr(ptr)
    }
}

/// Validates that `t` is aligned to `align_of::<U>()`.
#[inline(always)]
pub(crate) fn validate_aligned_to<T: AsAddress, U>(t: T) -> Result<(), AlignmentError<(), U>> {
    // `mem::align_of::<U>()` is guaranteed to return a non-zero value, which in
    // turn guarantees that this mod operation will not panic.
    #[allow(clippy::arithmetic_side_effects)]
    let remainder = t.addr() % mem::align_of::<U>();
    if remainder == 0 {
        Ok(())
    } else {
        // SAFETY: We just confirmed that `t.addr() % align_of::<U>() != 0`.
        // That's only possible if `align_of::<U>() > 1`.
        Err(unsafe { AlignmentError::new_unchecked(()) })
    }
}

/// Returns the bytes needed to pad `len` to the next multiple of `align`.
///
/// This function assumes that align is a power of two; there are no guarantees
/// on the answer it gives if this is not the case.
pub(crate) const fn padding_needed_for(len: usize, align: NonZeroUsize) -> usize {
    // Abstractly, we want to compute:
    //   align - (len % align).
    // Handling the case where len%align is 0.
    // Because align is a power of two, len % align = len & (align-1).
    // Guaranteed not to underflow as align is nonzero.
    #[allow(clippy::arithmetic_side_effects)]
    let mask = align.get() - 1;

    // To efficiently subtract this value from align, we can use the bitwise complement.
    // Note that ((!len) & (align-1)) gives us a number that with (len &
    // (align-1)) sums to align-1. So subtracting 1 from x before taking the
    // complement subtracts `len` from `align`. Some quick inspection of
    // cases shows that this also handles the case where `len % align = 0`
    // correctly too: len-1 % align then equals align-1, so the complement mod
    // align will be 0, as desired.
    //
    // The following reasoning can be verified quickly by an SMT solver
    // supporting the theory of bitvectors:
    // ```smtlib
    // ; Naive implementation of padding
    // (define-fun padding1 (
    //     (len (_ BitVec 32))
    //     (align (_ BitVec 32))) (_ BitVec 32)
    //    (ite
    //      (= (_ bv0 32) (bvand len (bvsub align (_ bv1 32))))
    //      (_ bv0 32)
    //      (bvsub align (bvand len (bvsub align (_ bv1 32))))))
    //
    // ; The implementation below
    // (define-fun padding2 (
    //     (len (_ BitVec 32))
    //     (align (_ BitVec 32))) (_ BitVec 32)
    // (bvand (bvnot (bvsub len (_ bv1 32))) (bvsub align (_ bv1 32))))
    //
    // (define-fun is-power-of-two ((x (_ BitVec 32))) Bool
    //   (= (_ bv0 32) (bvand x (bvsub x (_ bv1 32)))))
    //
    // (declare-const len (_ BitVec 32))
    // (declare-const align (_ BitVec 32))
    // ; Search for a case where align is a power of two and padding2 disagrees with padding1
    // (assert (and (is-power-of-two align)
    //              (not (= (padding1 len align) (padding2 len align)))))
    // (simplify (padding1 (_ bv300 32) (_ bv32 32))) ; 20
    // (simplify (padding2 (_ bv300 32) (_ bv32 32))) ; 20
    // (simplify (padding1 (_ bv322 32) (_ bv32 32))) ; 30
    // (simplify (padding2 (_ bv322 32) (_ bv32 32))) ; 30
    // (simplify (padding1 (_ bv8 32) (_ bv8 32)))    ; 0
    // (simplify (padding2 (_ bv8 32) (_ bv8 32)))    ; 0
    // (check-sat) ; unsat, also works for 64-bit bitvectors
    // ```
    !(len.wrapping_sub(1)) & mask
}

/// Rounds `n` down to the largest value `m` such that `m <= n` and `m % align
/// == 0`.
///
/// # Panics
///
/// May panic if `align` is not a power of two. Even if it doesn't panic in this
/// case, it will produce nonsense results.
#[inline(always)]
pub(crate) const fn round_down_to_next_multiple_of_alignment(
    n: usize,
    align: NonZeroUsize,
) -> usize {
    let align = align.get();
    #[cfg(zerocopy_panic_in_const_and_vec_try_reserve_1_57_0)]
    debug_assert!(align.is_power_of_two());

    // Subtraction can't underflow because `align.get() >= 1`.
    #[allow(clippy::arithmetic_side_effects)]
    let mask = !(align - 1);
    n & mask
}

pub(crate) const fn max(a: NonZeroUsize, b: NonZeroUsize) -> NonZeroUsize {
    if a.get() < b.get() {
        b
    } else {
        a
    }
}

pub(crate) const fn min(a: NonZeroUsize, b: NonZeroUsize) -> NonZeroUsize {
    if a.get() > b.get() {
        b
    } else {
        a
    }
}

/// Copies `src` into the prefix of `dst`.
///
/// # Safety
///
/// The caller guarantees that `src.len() <= dst.len()`.
#[inline(always)]
pub(crate) unsafe fn copy_unchecked(src: &[u8], dst: &mut [u8]) {
    debug_assert!(src.len() <= dst.len());
    // SAFETY: This invocation satisfies the safety contract of
    // copy_nonoverlapping [1]:
    // - `src.as_ptr()` is trivially valid for reads of `src.len()` bytes
    // - `dst.as_ptr()` is valid for writes of `src.len()` bytes, because the
    //   caller has promised that `src.len() <= dst.len()`
    // - `src` and `dst` are, trivially, properly aligned
    // - the region of memory beginning at `src` with a size of `src.len()`
    //   bytes does not overlap with the region of memory beginning at `dst`
    //   with the same size, because `dst` is derived from an exclusive
    //   reference.
    unsafe {
        core::ptr::copy_nonoverlapping(src.as_ptr(), dst.as_mut_ptr(), src.len());
    };
}

/// Unsafely transmutes the given `src` into a type `Dst`.
///
/// # Safety
///
/// The value `src` must be a valid instance of `Dst`.
#[inline(always)]
pub(crate) const unsafe fn transmute_unchecked<Src, Dst>(src: Src) -> Dst {
    static_assert!(Src, Dst => core::mem::size_of::<Src>() == core::mem::size_of::<Dst>());

    #[repr(C)]
    union Transmute<Src, Dst> {
        src: ManuallyDrop<Src>,
        dst: ManuallyDrop<Dst>,
    }

    // SAFETY: Since `Transmute<Src, Dst>` is `#[repr(C)]`, its `src` and `dst`
    // fields both start at the same offset and the types of those fields are
    // transparent wrappers around `Src` and `Dst` [1]. Consequently,
    // initializng `Transmute` with with `src` and then reading out `dst` is
    // equivalent to transmuting from `Src` to `Dst` [2]. Transmuting from `src`
    // to `Dst` is valid because — by contract on the caller — `src` is a valid
    // instance of `Dst`.
    //
    // [1] Per https://doc.rust-lang.org/1.82.0/std/mem/struct.ManuallyDrop.html:
    //
    //     `ManuallyDrop<T>` is guaranteed to have the same layout and bit
    //     validity as `T`, and is subject to the same layout optimizations as
    //     `T`.
    //
    // [2] Per https://doc.rust-lang.org/1.82.0/reference/items/unions.html#reading-and-writing-union-fields:
    //
    //     Effectively, writing to and then reading from a union with the C
    //     representation is analogous to a transmute from the type used for
    //     writing to the type used for reading.
    unsafe { ManuallyDrop::into_inner(Transmute { src: ManuallyDrop::new(src) }.dst) }
}

/// Uses `allocate` to create a `Box<T>`.
///
/// # Errors
///
/// Returns an error on allocation failure. Allocation failure is guaranteed
/// never to cause a panic or an abort.
///
/// # Safety
///
/// `allocate` must be either `alloc::alloc::alloc` or
/// `alloc::alloc::alloc_zeroed`. The referent of the box returned by `new_box`
/// has the same bit-validity as the referent of the pointer returned by the
/// given `allocate` and sufficient size to store `T` with `meta`.
#[must_use = "has no side effects (other than allocation)"]
#[cfg(feature = "alloc")]
#[inline]
pub(crate) unsafe fn new_box<T>(
    meta: T::PointerMetadata,
    allocate: unsafe fn(core::alloc::Layout) -> *mut u8,
) -> Result<alloc::boxed::Box<T>, crate::error::AllocError>
where
    T: ?Sized + crate::KnownLayout,
{
    use crate::error::AllocError;
    use crate::PointerMetadata;
    use core::alloc::Layout;

    let size = match meta.size_for_metadata(T::LAYOUT) {
        Some(size) => size,
        None => return Err(AllocError),
    };

    let align = T::LAYOUT.align.get();
    // On stable Rust versions <= 1.64.0, `Layout::from_size_align` has a bug in
    // which sufficiently-large allocations (those which, when rounded up to the
    // alignment, overflow `isize`) are not rejected, which can cause undefined
    // behavior. See #64 for details.
    //
    // TODO(#67): Once our MSRV is > 1.64.0, remove this assertion.
    #[allow(clippy::as_conversions)]
    let max_alloc = (isize::MAX as usize).saturating_sub(align);
    if size > max_alloc {
        return Err(AllocError);
    }

    // TODO(https://github.com/rust-lang/rust/issues/55724): Use
    // `Layout::repeat` once it's stabilized.
    let layout = Layout::from_size_align(size, align).or(Err(AllocError))?;

    let ptr = if layout.size() != 0 {
        // SAFETY: By contract on the caller, `allocate` is either
        // `alloc::alloc::alloc` or `alloc::alloc::alloc_zeroed`. The above
        // check ensures their shared safety precondition: that the supplied
        // layout is not zero-sized type [1].
        //
        // [1] Per https://doc.rust-lang.org/stable/std/alloc/trait.GlobalAlloc.html#tymethod.alloc:
        //
        //     This function is unsafe because undefined behavior can result if
        //     the caller does not ensure that layout has non-zero size.
        let ptr = unsafe { allocate(layout) };
        match NonNull::new(ptr) {
            Some(ptr) => ptr,
            None => return Err(AllocError),
        }
    } else {
        let align = T::LAYOUT.align.get();
        // We use `transmute` instead of an `as` cast since Miri (with strict
        // provenance enabled) notices and complains that an `as` cast creates a
        // pointer with no provenance. Miri isn't smart enough to realize that
        // we're only executing this branch when we're constructing a zero-sized
        // `Box`, which doesn't require provenance.
        //
        // SAFETY: any initialized bit sequence is a bit-valid `*mut u8`. All
        // bits of a `usize` are initialized.
        #[allow(clippy::useless_transmute)]
        let dangling = unsafe { mem::transmute::<usize, *mut u8>(align) };
        // SAFETY: `dangling` is constructed from `T::LAYOUT.align`, which is a
        // `NonZeroUsize`, which is guaranteed to be non-zero.
        //
        // `Box<[T]>` does not allocate when `T` is zero-sized or when `len` is
        // zero, but it does require a non-null dangling pointer for its
        // allocation.
        //
        // TODO(https://github.com/rust-lang/rust/issues/95228): Use
        // `std::ptr::without_provenance` once it's stable. That may optimize
        // better. As written, Rust may assume that this consumes "exposed"
        // provenance, and thus Rust may have to assume that this may consume
        // provenance from any pointer whose provenance has been exposed.
        unsafe { NonNull::new_unchecked(dangling) }
    };

    let ptr = T::raw_from_ptr_len(ptr, meta);

    // TODO(#429): Add a "SAFETY" comment and remove this `allow`. Make sure to
    // include a justification that `ptr.as_ptr()` is validly-aligned in the ZST
    // case (in which we manually construct a dangling pointer) and to justify
    // why `Box` is safe to drop (it's because `allocate` uses the system
    // allocator).
    #[allow(clippy::undocumented_unsafe_blocks)]
    Ok(unsafe { alloc::boxed::Box::from_raw(ptr.as_ptr()) })
}

/// Since we support multiple versions of Rust, there are often features which
/// have been stabilized in the most recent stable release which do not yet
/// exist (stably) on our MSRV. This module provides polyfills for those
/// features so that we can write more "modern" code, and just remove the
/// polyfill once our MSRV supports the corresponding feature. Without this,
/// we'd have to write worse/more verbose code and leave TODO comments sprinkled
/// throughout the codebase to update to the new pattern once it's stabilized.
///
/// Each trait is imported as `_` at the crate root; each polyfill should "just
/// work" at usage sites.
pub(crate) mod polyfills {
    use core::ptr::{self, NonNull};

    // A polyfill for `NonNull::slice_from_raw_parts` that we can use before our
    // MSRV is 1.70, when that function was stabilized.
    //
    // The `#[allow(unused)]` is necessary because, on sufficiently recent
    // toolchain versions, `ptr.slice_from_raw_parts()` resolves to the inherent
    // method rather than to this trait, and so this trait is considered unused.
    //
    // TODO(#67): Once our MSRV is 1.70, remove this.
    #[allow(unused)]
    pub(crate) trait NonNullExt<T> {
        fn slice_from_raw_parts(data: Self, len: usize) -> NonNull<[T]>;
    }

    impl<T> NonNullExt<T> for NonNull<T> {
        // NOTE on coverage: this will never be tested in nightly since it's a
        // polyfill for a feature which has been stabilized on our nightly
        // toolchain.
        #[cfg_attr(
            all(coverage_nightly, __ZEROCOPY_INTERNAL_USE_ONLY_NIGHTLY_FEATURES_IN_TESTS),
            coverage(off)
        )]
        #[inline(always)]
        fn slice_from_raw_parts(data: Self, len: usize) -> NonNull<[T]> {
            let ptr = ptr::slice_from_raw_parts_mut(data.as_ptr(), len);
            // SAFETY: `ptr` is converted from `data`, which is non-null.
            unsafe { NonNull::new_unchecked(ptr) }
        }
    }

    // A polyfill for `Self::unchecked_sub` that we can use until methods like
    // `usize::unchecked_sub` is stabilized.
    //
    // The `#[allow(unused)]` is necessary because, on sufficiently recent
    // toolchain versions, `ptr.slice_from_raw_parts()` resolves to the inherent
    // method rather than to this trait, and so this trait is considered unused.
    //
    // TODO(#67): Once our MSRV is high enough, remove this.
    #[allow(unused)]
    pub(crate) trait NumExt {
        /// Subtract without checking for underflow.
        ///
        /// # Safety
        ///
        /// The caller promises that the subtraction will not underflow.
        unsafe fn unchecked_sub(self, rhs: Self) -> Self;
    }

    impl NumExt for usize {
        // NOTE on coverage: this will never be tested in nightly since it's a
        // polyfill for a feature which has been stabilized on our nightly
        // toolchain.
        #[cfg_attr(
            all(coverage_nightly, __ZEROCOPY_INTERNAL_USE_ONLY_NIGHTLY_FEATURES_IN_TESTS),
            coverage(off)
        )]
        #[inline(always)]
        unsafe fn unchecked_sub(self, rhs: usize) -> usize {
            match self.checked_sub(rhs) {
                Some(x) => x,
                None => {
                    // SAFETY: The caller promises that the subtraction will not
                    // underflow.
                    unsafe { core::hint::unreachable_unchecked() }
                }
            }
        }
    }
}

#[cfg(test)]
pub(crate) mod testutil {
    use crate::*;

    /// A `T` which is aligned to at least `align_of::<A>()`.
    #[derive(Default)]
    pub(crate) struct Align<T, A> {
        pub(crate) t: T,
        _a: [A; 0],
    }

    impl<T: Default, A> Align<T, A> {
        pub(crate) fn set_default(&mut self) {
            self.t = T::default();
        }
    }

    impl<T, A> Align<T, A> {
        pub(crate) const fn new(t: T) -> Align<T, A> {
            Align { t, _a: [] }
        }
    }

    /// A `T` which is guaranteed not to satisfy `align_of::<A>()`.
    ///
    /// It must be the case that `align_of::<T>() < align_of::<A>()` in order
    /// fot this type to work properly.
    #[repr(C)]
    pub(crate) struct ForceUnalign<T: Unaligned, A> {
        // The outer struct is aligned to `A`, and, thanks to `repr(C)`, `t` is
        // placed at the minimum offset that guarantees its alignment. If
        // `align_of::<T>() < align_of::<A>()`, then that offset will be
        // guaranteed *not* to satisfy `align_of::<A>()`.
        //
        // Note that we need `T: Unaligned` in order to guarantee that there is
        // no padding between `_u` and `t`.
        _u: u8,
        pub(crate) t: T,
        _a: [A; 0],
    }

    impl<T: Unaligned, A> ForceUnalign<T, A> {
        pub(crate) fn new(t: T) -> ForceUnalign<T, A> {
            ForceUnalign { _u: 0, t, _a: [] }
        }
    }
    // A `u64` with alignment 8.
    //
    // Though `u64` has alignment 8 on some platforms, it's not guaranteed. By
    // contrast, `AU64` is guaranteed to have alignment 8 on all platforms.
    #[derive(
        KnownLayout,
        Immutable,
        FromBytes,
        IntoBytes,
        Eq,
        PartialEq,
        Ord,
        PartialOrd,
        Default,
        Debug,
        Copy,
        Clone,
    )]
    #[repr(C, align(8))]
    pub(crate) struct AU64(pub(crate) u64);

    impl AU64 {
        // Converts this `AU64` to bytes using this platform's endianness.
        pub(crate) fn to_bytes(self) -> [u8; 8] {
            crate::transmute!(self)
        }
    }

    impl Display for AU64 {
        #[cfg_attr(
            all(coverage_nightly, __ZEROCOPY_INTERNAL_USE_ONLY_NIGHTLY_FEATURES_IN_TESTS),
            coverage(off)
        )]
        fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
            Display::fmt(&self.0, f)
        }
    }

    #[derive(Immutable, FromBytes, Eq, PartialEq, Ord, PartialOrd, Default, Debug, Copy, Clone)]
    #[repr(C)]
    pub(crate) struct Nested<T, U: ?Sized> {
        _t: T,
        _u: U,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_round_down_to_next_multiple_of_alignment() {
        fn alt_impl(n: usize, align: NonZeroUsize) -> usize {
            let mul = n / align.get();
            mul * align.get()
        }

        for align in [1, 2, 4, 8, 16] {
            for n in 0..256 {
                let align = NonZeroUsize::new(align).unwrap();
                let want = alt_impl(n, align);
                let got = round_down_to_next_multiple_of_alignment(n, align);
                assert_eq!(got, want, "round_down_to_next_multiple_of_alignment({}, {})", n, align);
            }
        }
    }

    #[rustversion::since(1.57.0)]
    #[test]
    #[should_panic]
    fn test_round_down_to_next_multiple_of_alignment_zerocopy_panic_in_const_and_vec_try_reserve() {
        round_down_to_next_multiple_of_alignment(0, NonZeroUsize::new(3).unwrap());
    }
}

#[cfg(kani)]
mod proofs {
    use super::*;

    #[kani::proof]
    fn prove_round_down_to_next_multiple_of_alignment() {
        fn model_impl(n: usize, align: NonZeroUsize) -> usize {
            assert!(align.get().is_power_of_two());
            let mul = n / align.get();
            mul * align.get()
        }

        let align: NonZeroUsize = kani::any();
        kani::assume(align.get().is_power_of_two());
        let n: usize = kani::any();

        let expected = model_impl(n, align);
        let actual = round_down_to_next_multiple_of_alignment(n, align);
        assert_eq!(expected, actual, "round_down_to_next_multiple_of_alignment({}, {})", n, align);
    }

    // Restricted to nightly since we use the unstable `usize::next_multiple_of`
    // in our model implementation.
    #[cfg(__ZEROCOPY_INTERNAL_USE_ONLY_NIGHTLY_FEATURES_IN_TESTS)]
    #[kani::proof]
    fn prove_padding_needed_for() {
        fn model_impl(len: usize, align: NonZeroUsize) -> usize {
            let padded = len.next_multiple_of(align.get());
            let padding = padded - len;
            padding
        }

        let align: NonZeroUsize = kani::any();
        kani::assume(align.get().is_power_of_two());
        let len: usize = kani::any();
        // Constrain `len` to valid Rust lengths, since our model implementation
        // isn't robust to overflow.
        kani::assume(len <= isize::MAX as usize);
        kani::assume(align.get() < 1 << 29);

        let expected = model_impl(len, align);
        let actual = padding_needed_for(len, align);
        assert_eq!(expected, actual, "padding_needed_for({}, {})", len, align);

        let padded_len = actual + len;
        assert_eq!(padded_len % align, 0);
        assert!(padded_len / align >= len / align);
    }
}
