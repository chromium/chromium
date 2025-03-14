// Copyright 2023 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

use core::{
    fmt::{Debug, Formatter},
    marker::PhantomData,
    ptr::NonNull,
};

use super::{inner::PtrInner, invariant::*};
use crate::{
    util::{AlignmentVariance, Covariant, TransparentWrapper, ValidityVariance},
    AlignmentError, CastError, CastType, KnownLayout, SizeError, TryFromBytes, ValidityError,
};

/// Module used to gate access to [`Ptr`]'s fields.
mod def {
    use super::*;

    #[cfg(doc)]
    use super::super::invariant;

    /// A raw pointer with more restrictions.
    ///
    /// `Ptr<T>` is similar to [`NonNull<T>`], but it is more restrictive in the
    /// following ways (note that these requirements only hold of non-zero-sized
    /// referents):
    /// - It must derive from a valid allocation.
    /// - It must reference a byte range which is contained inside the
    ///   allocation from which it derives.
    ///   - As a consequence, the byte range it references must have a size
    ///     which does not overflow `isize`.
    ///
    /// Depending on how `Ptr` is parameterized, it may have additional
    /// invariants:
    /// - `ptr` conforms to the aliasing invariant of
    ///   [`I::Aliasing`](invariant::Aliasing).
    /// - `ptr` conforms to the alignment invariant of
    ///   [`I::Alignment`](invariant::Alignment).
    /// - `ptr` conforms to the validity invariant of
    ///   [`I::Validity`](invariant::Validity).
    ///
    /// `Ptr<'a, T>` is [covariant] in `'a` and invariant in `T`.
    ///
    /// [covariant]: https://doc.rust-lang.org/reference/subtyping.html
    pub struct Ptr<'a, T, I>
    where
        T: ?Sized,
        I: Invariants,
    {
        /// # Invariants
        ///
        /// 0. `ptr` conforms to the aliasing invariant of
        ///    [`I::Aliasing`](invariant::Aliasing).
        /// 1. `ptr` conforms to the alignment invariant of
        ///    [`I::Alignment`](invariant::Alignment).
        /// 2. `ptr` conforms to the validity invariant of
        ///    [`I::Validity`](invariant::Validity).
        // SAFETY: `PtrInner<'a, T>` is covariant in `'a` and invariant in `T`.
        ptr: PtrInner<'a, T>,
        _invariants: PhantomData<I>,
    }

    impl<'a, T, I> Ptr<'a, T, I>
    where
        T: 'a + ?Sized,
        I: Invariants,
    {
        /// Constructs a `Ptr` from a [`NonNull`].
        ///
        /// # Safety
        ///
        /// The caller promises that:
        ///
        /// 0. If `ptr`'s referent is not zero sized, then `ptr` is derived from
        ///    some valid Rust allocation, `A`.
        /// 1. If `ptr`'s referent is not zero sized, then `ptr` has valid
        ///    provenance for `A`.
        /// 2. If `ptr`'s referent is not zero sized, then `ptr` addresses a
        ///    byte range which is entirely contained in `A`.
        /// 3. `ptr` addresses a byte range whose length fits in an `isize`.
        /// 4. `ptr` addresses a byte range which does not wrap around the
        ///    address space.
        /// 5. If `ptr`'s referent is not zero sized, then `A` is guaranteed to
        ///    live for at least `'a`.
        /// 6. `ptr` conforms to the aliasing invariant of
        ///    [`I::Aliasing`](invariant::Aliasing).
        /// 7. `ptr` conforms to the alignment invariant of
        ///    [`I::Alignment`](invariant::Alignment).
        /// 8. `ptr` conforms to the validity invariant of
        ///    [`I::Validity`](invariant::Validity).
        pub(super) unsafe fn new(ptr: NonNull<T>) -> Ptr<'a, T, I> {
            // SAFETY: The caller has promised (in 0 - 5) to satisfy all safety
            // invariants of `PtrInner::new`.
            let ptr = unsafe { PtrInner::new(ptr) };
            // SAFETY: The caller has promised (in 6 - 8) to satisfy all safety
            // invariants of `Ptr`.
            Self { ptr, _invariants: PhantomData }
        }

        /// Constructs a new `Ptr` from a [`PtrInner`].
        ///
        /// # Safety
        ///
        /// The caller promises that:
        ///
        /// 0. `ptr` conforms to the aliasing invariant of
        ///    [`I::Aliasing`](invariant::Aliasing).
        /// 1. `ptr` conforms to the alignment invariant of
        ///    [`I::Alignment`](invariant::Alignment).
        /// 2. `ptr` conforms to the validity invariant of
        ///    [`I::Validity`](invariant::Validity).
        pub(super) unsafe fn from_inner(ptr: PtrInner<'a, T>) -> Ptr<'a, T, I> {
            // SAFETY: The caller has promised to satisfy all safety invariants
            // of `Ptr`.
            Self { ptr, _invariants: PhantomData }
        }

        /// Converts this `Ptr<T>` to a [`PtrInner<T>`].
        ///
        /// Note that this method does not consume `self`. The caller should
        /// watch out for `unsafe` code which uses the returned value in a way
        /// that violates the safety invariants of `self`.
        pub(crate) fn as_inner(&self) -> PtrInner<'a, T> {
            self.ptr
        }
    }
}

#[allow(unreachable_pub)] // This is a false positive on our MSRV toolchain.
pub use def::Ptr;

/// External trait implementations on [`Ptr`].
mod _external {
    use super::*;

    /// SAFETY: Shared pointers are safely `Copy`. `Ptr`'s other invariants
    /// (besides aliasing) are unaffected by the number of references that exist
    /// to `Ptr`'s referent.
    impl<'a, T, I> Copy for Ptr<'a, T, I>
    where
        T: 'a + ?Sized,
        I: Invariants<Aliasing = Shared>,
    {
    }

    /// SAFETY: Shared pointers are safely `Clone`. `Ptr`'s other invariants
    /// (besides aliasing) are unaffected by the number of references that exist
    /// to `Ptr`'s referent.
    impl<'a, T, I> Clone for Ptr<'a, T, I>
    where
        T: 'a + ?Sized,
        I: Invariants<Aliasing = Shared>,
    {
        #[inline]
        fn clone(&self) -> Self {
            *self
        }
    }

    impl<'a, T, I> Debug for Ptr<'a, T, I>
    where
        T: 'a + ?Sized,
        I: Invariants,
    {
        #[inline]
        fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
            self.as_inner().as_non_null().fmt(f)
        }
    }
}

/// Methods for converting to and from `Ptr` and Rust's safe reference types.
mod _conversions {
    use super::*;

    /// `&'a T` → `Ptr<'a, T>`
    impl<'a, T> Ptr<'a, T, (Shared, Aligned, Valid)>
    where
        T: 'a + ?Sized,
    {
        /// Constructs a `Ptr` from a shared reference.
        #[doc(hidden)]
        #[inline]
        pub fn from_ref(ptr: &'a T) -> Self {
            let inner = PtrInner::from_ref(ptr);
            // SAFETY:
            // 0. `ptr`, by invariant on `&'a T`, conforms to the aliasing
            //    invariant of `Shared`.
            // 1. `ptr`, by invariant on `&'a T`, conforms to the alignment
            //    invariant of `Aligned`.
            // 2. `ptr`, by invariant on `&'a T`, conforms to the validity
            //    invariant of `Valid`.
            unsafe { Self::from_inner(inner) }
        }
    }

    /// `&'a mut T` → `Ptr<'a, T>`
    impl<'a, T> Ptr<'a, T, (Exclusive, Aligned, Valid)>
    where
        T: 'a + ?Sized,
    {
        /// Constructs a `Ptr` from an exclusive reference.
        #[inline]
        pub(crate) fn from_mut(ptr: &'a mut T) -> Self {
            let inner = PtrInner::from_mut(ptr);
            // SAFETY:
            // 0. `ptr`, by invariant on `&'a mut T`, conforms to the aliasing
            //    invariant of `Exclusive`.
            // 1. `ptr`, by invariant on `&'a mut T`, conforms to the alignment
            //    invariant of `Aligned`.
            // 2. `ptr`, by invariant on `&'a mut T`, conforms to the validity
            //    invariant of `Valid`.
            unsafe { Self::from_inner(inner) }
        }
    }

    /// `Ptr<'a, T>` → `&'a T`
    impl<'a, T, I> Ptr<'a, T, I>
    where
        T: 'a + ?Sized,
        I: Invariants<Alignment = Aligned, Validity = Valid>,
        I::Aliasing: Reference,
    {
        /// Converts `self` to a shared reference.
        // This consumes `self`, not `&self`, because `self` is, logically, a
        // pointer. For `I::Aliasing = invariant::Shared`, `Self: Copy`, and so
        // this doesn't prevent the caller from still using the pointer after
        // calling `as_ref`.
        #[allow(clippy::wrong_self_convention)]
        pub(crate) fn as_ref(self) -> &'a T {
            let raw = self.as_inner().as_non_null();
            // SAFETY: This invocation of `NonNull::as_ref` satisfies its
            // documented safety preconditions:
            //
            // 1. The pointer is properly aligned. This is ensured by-contract
            //    on `Ptr`, because the `I::Alignment` is `Aligned`.
            //
            // 2. If the pointer's referent is not zero-sized, then the pointer
            //    must be “dereferenceable” in the sense defined in the module
            //    documentation; i.e.:
            //
            //    > The memory range of the given size starting at the pointer
            //    > must all be within the bounds of a single allocated object.
            //    > [2]
            //
            //   This is ensured by contract on all `PtrInner`s.
            //
            // 3. The pointer must point to an initialized instance of `T`. This
            //    is ensured by-contract on `Ptr`, because the `I::Validity` is
            //    `Valid`.
            //
            // 4. You must enforce Rust’s aliasing rules. This is ensured by
            //    contract on `Ptr`, because `I::Aliasing: Reference`. Either it
            //    is `Shared` or `Exclusive`. If it is `Shared`, other
            //    references may not mutate the referent outside of
            //    `UnsafeCell`s.
            //
            // [1]: https://doc.rust-lang.org/std/ptr/struct.NonNull.html#method.as_ref
            // [2]: https://doc.rust-lang.org/std/ptr/index.html#safety
            unsafe { raw.as_ref() }
        }
    }

    impl<'a, T, I> Ptr<'a, T, I>
    where
        T: 'a + ?Sized,
        I: Invariants,
        I::Aliasing: Reference,
    {
        /// Reborrows `self`, producing another `Ptr`.
        ///
        /// Since `self` is borrowed immutably, this prevents any mutable
        /// methods from being called on `self` as long as the returned `Ptr`
        /// exists.
        #[doc(hidden)]
        #[inline]
        #[allow(clippy::needless_lifetimes)] // Allows us to name the lifetime in the safety comment below.
        pub fn reborrow<'b>(&'b mut self) -> Ptr<'b, T, I>
        where
            'a: 'b,
        {
            // SAFETY: The following all hold by invariant on `self`, and thus
            // hold of `ptr = self.as_inner()`:
            // 0. SEE BELOW.
            // 1. `ptr` conforms to the alignment invariant of
            //   [`I::Alignment`](invariant::Alignment).
            // 2. `ptr` conforms to the validity invariant of
            //   [`I::Validity`](invariant::Validity).
            //
            // For aliasing (6 above), since `I::Aliasing: Reference`,
            // there are two cases for `I::Aliasing`:
            // - For `invariant::Shared`: `'a` outlives `'b`, and so the
            //   returned `Ptr` does not permit accessing the referent any
            //   longer than is possible via `self`. For shared aliasing, it is
            //   sound for multiple `Ptr`s to exist simultaneously which
            //   reference the same memory, so creating a new one is not
            //   problematic.
            // - For `invariant::Exclusive`: Since `self` is `&'b mut` and we
            //   return a `Ptr` with lifetime `'b`, `self` is inaccessible to
            //   the caller for the lifetime `'b` - in other words, `self` is
            //   inaccessible to the caller as long as the returned `Ptr`
            //   exists. Since `self` is an exclusive `Ptr`, no other live
            //   references or `Ptr`s may exist which refer to the same memory
            //   while `self` is live. Thus, as long as the returned `Ptr`
            //   exists, no other references or `Ptr`s which refer to the same
            //   memory may be live.
            unsafe { Ptr::from_inner(self.as_inner()) }
        }
    }

    /// `Ptr<'a, T>` → `&'a mut T`
    impl<'a, T> Ptr<'a, T, (Exclusive, Aligned, Valid)>
    where
        T: 'a + ?Sized,
    {
        /// Converts `self` to a mutable reference.
        #[allow(clippy::wrong_self_convention)]
        pub(crate) fn as_mut(self) -> &'a mut T {
            let mut raw = self.as_inner().as_non_null();
            // SAFETY: This invocation of `NonNull::as_mut` satisfies its
            // documented safety preconditions:
            //
            // 1. The pointer is properly aligned. This is ensured by-contract
            //    on `Ptr`, because the `ALIGNMENT_INVARIANT` is `Aligned`.
            //
            // 2. If the pointer's referent is not zero-sized, then the pointer
            //    must be “dereferenceable” in the sense defined in the module
            //    documentation; i.e.:
            //
            //    > The memory range of the given size starting at the pointer
            //    > must all be within the bounds of a single allocated object.
            //    > [2]
            //
            //   This is ensured by contract on all `PtrInner`s.
            //
            // 3. The pointer must point to an initialized instance of `T`. This
            //    is ensured by-contract on `Ptr`, because the validity
            //    invariant is `Valid`.
            //
            // 4. You must enforce Rust’s aliasing rules. This is ensured by
            //    contract on `Ptr`, because the `ALIASING_INVARIANT` is
            //    `Exclusive`.
            //
            // [1]: https://doc.rust-lang.org/std/ptr/struct.NonNull.html#method.as_mut
            // [2]: https://doc.rust-lang.org/std/ptr/index.html#safety
            unsafe { raw.as_mut() }
        }
    }

    /// `Ptr<'a, T = Wrapper<U>>` → `Ptr<'a, U>`
    impl<'a, T, I> Ptr<'a, T, I>
    where
        T: 'a + TransparentWrapper<I, UnsafeCellVariance = Covariant> + ?Sized,
        I: Invariants,
    {
        /// Converts `self` to a transparent wrapper type into a `Ptr` to the
        /// wrapped inner type.
        pub(crate) fn transparent_wrapper_into_inner(
            self,
        ) -> Ptr<
            'a,
            T::Inner,
            (
                I::Aliasing,
                <T::AlignmentVariance as AlignmentVariance<I::Alignment>>::Applied,
                <T::ValidityVariance as ValidityVariance<I::Validity>>::Applied,
            ),
        > {
            // SAFETY:
            // - By invariant on `TransparentWrapper::cast_into_inner`:
            //   - This cast preserves address and referent size, and thus the
            //     returned pointer addresses the same bytes as `p`
            //   - This cast preserves provenance
            // - By invariant on `TransparentWrapper<UnsafeCellVariance =
            //   Covariant>`, `T` and `T::Inner` have `UnsafeCell`s at the same
            //   byte ranges. Since `p` and the returned pointer address the
            //   same byte range, they refer to `UnsafeCell`s at the same byte
            //   ranges.
            // - By invariant on `TransparentWrapper`, since `self` satisfies
            //   the validity invariant `I::Validity`, the returned pointer (of
            //   type `T::Inner`) satisfies the given "applied" validity
            //   invariant.
            let ptr = unsafe { self.transmute_unchecked(|p| T::cast_into_inner(p)) };
            // SAFETY: By invariant on `TransparentWrapper`, since `self`
            // satisfies the alignment invariant `I::Alignment`, the returned
            // pointer (of type `T::Inner`) satisfies the given "applied"
            // alignment invariant.
            unsafe { ptr.assume_alignment() }
        }
    }

    /// `Ptr<'a, T>` → `Ptr<'a, U>`
    impl<'a, T: ?Sized, I> Ptr<'a, T, I>
    where
        I: Invariants,
    {
        /// Casts to a different (unsized) target type without checking interior
        /// mutability.
        ///
        /// Callers should prefer [`cast_unsized`] where possible.
        ///
        /// [`cast_unsized`]: Ptr::cast_unsized
        ///
        /// # Safety
        ///
        /// The caller promises that `u = cast(p)` is a pointer cast with the
        /// following properties:
        /// - `u` addresses a subset of the bytes addressed by `p`
        /// - `u` has the same provenance as `p`
        /// - If `I::Aliasing` is [`Shared`], `UnsafeCell`s in `*u` must exist
        ///   at ranges identical to those at which `UnsafeCell`s exist in `*p`
        /// - It is sound to transmute a pointer of type `T` with aliasing
        ///   `I::Aliasing` and validity `I::Validity` to a pointer of type `U`
        ///   with aliasing `I::Aliasing` and validity `V`. This is a subtle
        ///   soundness requirement that is a function of `T`, `U`,
        ///   `I::Aliasing`, `I::Validity`, and `V`, and may depend upon the
        ///   presence, absence, or specific location of `UnsafeCell`s in `T`
        ///   and/or `U`.
        #[doc(hidden)]
        #[inline]
        pub unsafe fn transmute_unchecked<U: ?Sized, V, F>(
            self,
            cast: F,
        ) -> Ptr<'a, U, (I::Aliasing, Unaligned, V)>
        where
            V: Validity,
            F: FnOnce(*mut T) -> *mut U,
        {
            let ptr = cast(self.as_inner().as_non_null().as_ptr());

            // SAFETY: Caller promises that `cast` returns a pointer whose
            // address is in the range of `self.as_inner().as_non_null()`'s referent. By
            // invariant, none of these addresses are null.
            let ptr = unsafe { NonNull::new_unchecked(ptr) };

            // SAFETY:
            //
            // Lemma 1: `ptr` has the same provenance as `self`. The caller
            // promises that `cast` preserves provenance, and we call it with
            // `self.as_inner().as_non_null()`.
            //
            // 0. By invariant,  if `self`'s referent is not zero sized, then
            //    `self` is derived from some valid Rust allocation, `A`. By
            //    Lemma 1, `ptr` has the same provenance as `self`. Thus, `ptr`
            //    is derived from `A`.
            // 1. By invariant, if `self`'s referent is not zero sized, then
            //    `self` has valid provenance for `A`. By Lemma 1, so does
            //    `ptr`.
            // 2. By invariant on `self` and caller precondition, if `ptr`'s
            //    referent is not zero sized, then `ptr` addresses a byte range
            //    which is entirely contained in `A`.
            // 3. By invariant on `self` and caller precondition, `ptr`
            //    addresses a byte range whose length fits in an `isize`.
            // 4. By invariant on `self` and caller precondition, `ptr`
            //    addresses a byte range which does not wrap around the address
            //    space.
            // 5. By invariant on `self`, if `self`'s referent is not zero
            //    sized, then `A` is guaranteed to live for at least `'a`.
            // 6. `ptr` conforms to the aliasing invariant of `I::Aliasing`:
            //    - `Exclusive`: `self` is the only `Ptr` or reference which is
            //      permitted to read or modify the referent for the lifetime
            //      `'a`. Since we consume `self` by value, the returned pointer
            //      remains the only `Ptr` or reference which is permitted to
            //      read or modify the referent for the lifetime `'a`.
            //    - `Shared`: Since `self` has aliasing `Shared`, we know that
            //      no other code may mutate the referent during the lifetime
            //      `'a`, except via `UnsafeCell`s. The caller promises that
            //      `UnsafeCell`s cover the same byte ranges in `*self` and
            //      `*ptr`. For each byte in the referent, there are two cases:
            //      - If the byte is not covered by an `UnsafeCell` in `*ptr`,
            //        then it is not covered in `*self`. By invariant on `self`,
            //        it will not be mutated during `'a`, as required by the
            //        constructed pointer. Similarly, the returned pointer will
            //        not permit any mutations to these locations, as required
            //        by the invariant on `self`.
            //      - If the byte is covered by an `UnsafeCell` in `*ptr`, then
            //        the returned pointer's invariants do not assume that the
            //        byte will not be mutated during `'a`. While the returned
            //        pointer will permit mutation of this byte during `'a`, by
            //        invariant on `self`, no other code assumes that this will
            //        not happen.
            //    - `Inaccessible`: There are no restrictions we need to uphold.
            // 7. `ptr` trivially satisfies the alignment invariant `Unaligned`.
            // 8. The caller promises that `ptr` conforms to the validity
            //    invariant `V` with respect to its referent type, `U`.
            unsafe { Ptr::new(ptr) }
        }
    }

    /// `Ptr<'a, T, (_, _, _)>` → `Ptr<'a, Unalign<T>, (_, Aligned, _)>`
    impl<'a, T, I> Ptr<'a, T, I>
    where
        I: Invariants,
    {
        /// Converts a `Ptr` an unaligned `T` into a `Ptr` to an aligned
        /// `Unalign<T>`.
        pub(crate) fn into_unalign(
            self,
        ) -> Ptr<'a, crate::Unalign<T>, (I::Aliasing, Aligned, I::Validity)> {
            // SAFETY:
            // - This cast preserves provenance.
            // - This cast preserves address. `Unalign<T>` promises to have the
            //   same size as `T`, and so the cast returns a pointer addressing
            //   the same byte range as `p`.
            // - By the same argument, the returned pointer refers to
            //   `UnsafeCell`s at the same locations as `p`.
            // - `Unalign<T>` promises to have the same bit validity as `T`
            let ptr = unsafe {
                #[allow(clippy::as_conversions)]
                self.transmute_unchecked(|p: *mut T| p as *mut crate::Unalign<T>)
            };
            ptr.bikeshed_recall_aligned()
        }
    }
}

/// State transitions between invariants.
mod _transitions {
    use super::*;

    impl<'a, T, I> Ptr<'a, T, I>
    where
        T: 'a + ?Sized,
        I: Invariants,
    {
        /// Returns a `Ptr` with [`Exclusive`] aliasing if `self` already has
        /// `Exclusive` aliasing, or generates a compile-time assertion failure.
        ///
        /// This allows code which is generic over aliasing to down-cast to a
        /// concrete aliasing.
        ///
        /// [`Exclusive`]: crate::pointer::invariant::Exclusive
        #[inline]
        pub(crate) fn into_exclusive_or_pme(
            self,
        ) -> Ptr<'a, T, (Exclusive, I::Alignment, I::Validity)> {
            // NOTE(https://github.com/rust-lang/rust/issues/131625): We do this
            // rather than just having `Aliasing::IS_EXCLUSIVE` have the panic
            // behavior because doing it that way causes rustdoc to fail while
            // attempting to document hidden items (since it evaluates the
            // constant - and thus panics).
            trait AliasingExt: Aliasing {
                const IS_EXCL: bool;
            }

            impl<A: Aliasing> AliasingExt for A {
                const IS_EXCL: bool = {
                    const_assert!(Self::IS_EXCLUSIVE);
                    true
                };
            }

            assert!(I::Aliasing::IS_EXCL);

            // SAFETY: We've confirmed that `self` already has the aliasing
            // `Exclusive`. If it didn't, either the preceding assert would fail
            // or evaluating `I::Aliasing::IS_EXCL` would fail. We're *pretty*
            // sure that it's guaranteed to fail const eval, but the `assert!`
            // provides a backstop in case that doesn't work.
            unsafe { self.assume_exclusive() }
        }

        /// Assumes that `self` satisfies the invariants `H`.
        ///
        /// # Safety
        ///
        /// The caller promises that `self` satisfies the invariants `H`.
        unsafe fn assume_invariants<H: Invariants>(self) -> Ptr<'a, T, H> {
            // SAFETY: The caller has promised to satisfy all parameterized
            // invariants of `Ptr`. `Ptr`'s other invariants are satisfied
            // by-contract by the source `Ptr`.
            unsafe { Ptr::from_inner(self.as_inner()) }
        }

        /// Helps the type system unify two distinct invariant types which are
        /// actually the same.
        pub(crate) fn unify_invariants<
            H: Invariants<Aliasing = I::Aliasing, Alignment = I::Alignment, Validity = I::Validity>,
        >(
            self,
        ) -> Ptr<'a, T, H> {
            // SAFETY: The associated type bounds on `H` ensure that the
            // invariants are unchanged.
            unsafe { self.assume_invariants::<H>() }
        }

        /// Assumes that `self` satisfies the aliasing requirement of `A`.
        ///
        /// # Safety
        ///
        /// The caller promises that `self` satisfies the aliasing requirement
        /// of `A`.
        #[inline]
        pub(crate) unsafe fn assume_aliasing<A: Aliasing>(
            self,
        ) -> Ptr<'a, T, (A, I::Alignment, I::Validity)> {
            // SAFETY: The caller promises that `self` satisfies the aliasing
            // requirements of `A`.
            unsafe { self.assume_invariants() }
        }

        /// Assumes `self` satisfies the aliasing requirement of [`Exclusive`].
        ///
        /// # Safety
        ///
        /// The caller promises that `self` satisfies the aliasing requirement
        /// of `Exclusive`.
        ///
        /// [`Exclusive`]: crate::pointer::invariant::Exclusive
        #[inline]
        pub(crate) unsafe fn assume_exclusive(
            self,
        ) -> Ptr<'a, T, (Exclusive, I::Alignment, I::Validity)> {
            // SAFETY: The caller promises that `self` satisfies the aliasing
            // requirements of `Exclusive`.
            unsafe { self.assume_aliasing::<Exclusive>() }
        }

        /// Assumes that `self`'s referent is validly-aligned for `T` if
        /// required by `A`.
        ///
        /// # Safety
        ///
        /// The caller promises that `self`'s referent conforms to the alignment
        /// invariant of `T` if required by `A`.
        #[inline]
        pub(crate) unsafe fn assume_alignment<A: Alignment>(
            self,
        ) -> Ptr<'a, T, (I::Aliasing, A, I::Validity)> {
            // SAFETY: The caller promises that `self`'s referent is
            // well-aligned for `T` if required by `A` .
            unsafe { self.assume_invariants() }
        }

        /// Checks the `self`'s alignment at runtime, returning an aligned `Ptr`
        /// on success.
        pub(crate) fn bikeshed_try_into_aligned(
            self,
        ) -> Result<Ptr<'a, T, (I::Aliasing, Aligned, I::Validity)>, AlignmentError<Self, T>>
        where
            T: Sized,
        {
            if let Err(err) =
                crate::util::validate_aligned_to::<_, T>(self.as_inner().as_non_null())
            {
                return Err(err.with_src(self));
            }

            // SAFETY: We just checked the alignment.
            Ok(unsafe { self.assume_alignment::<Aligned>() })
        }

        /// Recalls that `self`'s referent is validly-aligned for `T`.
        #[inline]
        // TODO(#859): Reconsider the name of this method before making it
        // public.
        pub(crate) fn bikeshed_recall_aligned(
            self,
        ) -> Ptr<'a, T, (I::Aliasing, Aligned, I::Validity)>
        where
            T: crate::Unaligned,
        {
            // SAFETY: The bound `T: Unaligned` ensures that `T` has no
            // non-trivial alignment requirement.
            unsafe { self.assume_alignment::<Aligned>() }
        }

        /// Assumes that `self`'s referent conforms to the validity requirement
        /// of `V`.
        ///
        /// # Safety
        ///
        /// The caller promises that `self`'s referent conforms to the validity
        /// requirement of `V`.
        #[doc(hidden)]
        #[must_use]
        #[inline]
        pub unsafe fn assume_validity<V: Validity>(
            self,
        ) -> Ptr<'a, T, (I::Aliasing, I::Alignment, V)> {
            // SAFETY: The caller promises that `self`'s referent conforms to
            // the validity requirement of `V`.
            unsafe { self.assume_invariants() }
        }

        /// A shorthand for `self.assume_validity<invariant::Initialized>()`.
        ///
        /// # Safety
        ///
        /// The caller promises to uphold the safety preconditions of
        /// `self.assume_validity<invariant::Initialized>()`.
        #[doc(hidden)]
        #[must_use]
        #[inline]
        pub unsafe fn assume_initialized(
            self,
        ) -> Ptr<'a, T, (I::Aliasing, I::Alignment, Initialized)> {
            // SAFETY: The caller has promised to uphold the safety
            // preconditions.
            unsafe { self.assume_validity::<Initialized>() }
        }

        /// A shorthand for `self.assume_validity<Valid>()`.
        ///
        /// # Safety
        ///
        /// The caller promises to uphold the safety preconditions of
        /// `self.assume_validity<Valid>()`.
        #[doc(hidden)]
        #[must_use]
        #[inline]
        pub unsafe fn assume_valid(self) -> Ptr<'a, T, (I::Aliasing, I::Alignment, Valid)> {
            // SAFETY: The caller has promised to uphold the safety
            // preconditions.
            unsafe { self.assume_validity::<Valid>() }
        }

        /// Recalls that `self`'s referent is initialized.
        #[doc(hidden)]
        #[must_use]
        #[inline]
        // TODO(#859): Reconsider the name of this method before making it
        // public.
        pub fn bikeshed_recall_initialized_from_bytes(
            self,
        ) -> Ptr<'a, T, (I::Aliasing, I::Alignment, Initialized)>
        where
            T: crate::IntoBytes + crate::FromBytes,
            I: Invariants<Validity = Valid>,
        {
            // SAFETY: The `T: IntoBytes` bound ensures that any bit-valid `T`
            // is entirely initialized. `I: Invariants<Validity = Valid>`
            // ensures that `self`'s referent is a bit-valid `T`. Producing an
            // `Initialized` `Ptr` may permit the caller to write arbitrary
            // initialized bytes to the referent (depending on aliasing mode and
            // presence of `UnsafeCell`s). `T: FromBytes` ensures that any byte
            // sequence written will remain a bit-valid `T`.
            unsafe { self.assume_initialized() }
        }

        /// Recalls that `self`'s referent is initialized.
        #[doc(hidden)]
        #[must_use]
        #[inline]
        // TODO(#859): Reconsider the name of this method before making it
        // public.
        pub fn bikeshed_recall_initialized_immutable(
            self,
        ) -> Ptr<'a, T, (Shared, I::Alignment, Initialized)>
        where
            T: crate::IntoBytes + crate::Immutable,
            I: Invariants<Aliasing = Shared, Validity = Valid>,
        {
            // SAFETY: The `T: IntoBytes` bound ensures that any bit-valid `T`
            // is entirely initialized. `I: Invariants<Validity = Valid>`
            // ensures that `self`'s referent is a bit-valid `T`. Since `T:
            // Immutable` and the aliasing is `Shared`, the resulting `Ptr`
            // cannot be used to modify the referent, and so it's acceptable
            // that going from `Valid` to `Initialized` may increase the set of
            // values allowed in the referent.
            unsafe { self.assume_initialized() }
        }

        /// Recalls that `self`'s referent is bit-valid for `T`.
        #[doc(hidden)]
        #[must_use]
        #[inline]
        // TODO(#859): Reconsider the name of this method before making it
        // public.
        pub fn bikeshed_recall_valid(self) -> Ptr<'a, T, (I::Aliasing, I::Alignment, Valid)>
        where
            T: crate::FromBytes,
            I: Invariants<Validity = Initialized>,
        {
            // SAFETY: The bound `T: FromBytes` ensures that any initialized
            // sequence of bytes is bit-valid for `T`. `I: Invariants<Validity =
            // invariant::Initialized>` ensures that all of the referent bytes
            // are initialized.
            unsafe { self.assume_valid() }
        }

        /// Checks that `self`'s referent is validly initialized for `T`,
        /// returning a `Ptr` with `Valid` on success.
        ///
        /// # Panics
        ///
        /// This method will panic if
        /// [`T::is_bit_valid`][TryFromBytes::is_bit_valid] panics.
        ///
        /// # Safety
        ///
        /// On error, unsafe code may rely on this method's returned
        /// `ValidityError` containing `self`.
        #[inline]
        pub(crate) fn try_into_valid<R>(
            mut self,
        ) -> Result<Ptr<'a, T, (I::Aliasing, I::Alignment, Valid)>, ValidityError<Self, T>>
        where
            T: TryFromBytes + Read<I::Aliasing, R>,
            I::Aliasing: Reference,
            I: Invariants<Validity = Initialized>,
        {
            // This call may panic. If that happens, it doesn't cause any soundness
            // issues, as we have not generated any invalid state which we need to
            // fix before returning.
            if T::is_bit_valid(self.reborrow().forget_aligned()) {
                // SAFETY: If `T::is_bit_valid`, code may assume that `self`
                // contains a bit-valid instance of `Self`.
                Ok(unsafe { self.assume_valid() })
            } else {
                Err(ValidityError::new(self))
            }
        }

        /// Forgets that `self`'s referent is validly-aligned for `T`.
        #[doc(hidden)]
        #[must_use]
        #[inline]
        pub fn forget_aligned(self) -> Ptr<'a, T, (I::Aliasing, Unaligned, I::Validity)> {
            // SAFETY: `Unaligned` is less restrictive than `Aligned`.
            unsafe { self.assume_invariants() }
        }
    }
}

/// Casts of the referent type.
mod _casts {
    use super::*;

    impl<'a, T, I> Ptr<'a, T, I>
    where
        T: 'a + ?Sized,
        I: Invariants,
    {
        /// Casts to a different (unsized) target type without checking interior
        /// mutability.
        ///
        /// Callers should prefer [`cast_unsized`] where possible.
        ///
        /// [`cast_unsized`]: Ptr::cast_unsized
        ///
        /// # Safety
        ///
        /// The caller promises that `u = cast(p)` is a pointer cast with the
        /// following properties:
        /// - `u` addresses a subset of the bytes addressed by `p`
        /// - `u` has the same provenance as `p`
        /// - If `I::Aliasing` is [`Shared`], `UnsafeCell`s in `*u` must exist
        ///   at ranges identical to those at which `UnsafeCell`s exist in `*p`
        #[doc(hidden)]
        #[inline]
        pub unsafe fn cast_unsized_unchecked<U, F: FnOnce(*mut T) -> *mut U>(
            self,
            cast: F,
        ) -> Ptr<'a, U, (I::Aliasing, Unaligned, I::Validity)>
        where
            U: 'a + CastableFrom<T, I::Validity, I::Validity> + ?Sized,
        {
            // SAFETY:
            // - The caller promises that `u = cast(p)` is a pointer which
            //   satisfies:
            //   - `u` addresses a subset of the bytes addressed by `p`
            //   - `u` has the same provenance as `p`
            //   - If `I::Aliasing` is [`Shared`], `UnsafeCell`s in `*u` must
            //     exist at ranges identical to those at which `UnsafeCell`s
            //     exist in `*p`
            // - By `U: CastableFrom<T, I::Validity, I::Validity>`,
            //   `I::Validity` is either `Uninit` or `Initialized`. In both
            //   cases, the bit validity `I::Validity` has the same semantics
            //   regardless of referent type. In other words, the set of allowed
            //   referent values for `Ptr<T, (_, _, I::Validity)>` and `Ptr<U,
            //   (_, _, I::Validity)>` are identical.
            unsafe { self.transmute_unchecked(cast) }
        }

        /// Casts to a different (unsized) target type.
        ///
        /// # Safety
        ///
        /// The caller promises that `u = cast(p)` is a pointer cast with the
        /// following properties:
        /// - `u` addresses a subset of the bytes addressed by `p`
        /// - `u` has the same provenance as `p`
        #[doc(hidden)]
        #[inline]
        pub unsafe fn cast_unsized<U, F, R, S>(
            self,
            cast: F,
        ) -> Ptr<'a, U, (I::Aliasing, Unaligned, I::Validity)>
        where
            T: Read<I::Aliasing, R>,
            U: 'a + ?Sized + Read<I::Aliasing, S> + CastableFrom<T, I::Validity, I::Validity>,
            F: FnOnce(*mut T) -> *mut U,
        {
            // SAFETY: Because `T` and `U` both implement `Read<I::Aliasing, _>`,
            // either:
            // - `I::Aliasing` is `Exclusive`
            // - `T` and `U` are both `Immutable`, in which case they trivially
            //   contain `UnsafeCell`s at identical locations
            //
            // The caller promises all other safety preconditions.
            unsafe { self.cast_unsized_unchecked(cast) }
        }
    }

    impl<'a, T, I> Ptr<'a, T, I>
    where
        T: 'a + KnownLayout + ?Sized,
        I: Invariants<Validity = Initialized>,
    {
        /// Casts this pointer-to-initialized into a pointer-to-bytes.
        #[allow(clippy::wrong_self_convention)]
        pub(crate) fn as_bytes<R>(self) -> Ptr<'a, [u8], (I::Aliasing, Aligned, Valid)>
        where
            T: Read<I::Aliasing, R>,
            I::Aliasing: Reference,
        {
            let bytes = match T::size_of_val_raw(self.as_inner().as_non_null()) {
                Some(bytes) => bytes,
                // SAFETY: `KnownLayout::size_of_val_raw` promises to always
                // return `Some` so long as the resulting size fits in a
                // `usize`. By invariant on `Ptr`, `self` refers to a range of
                // bytes whose size fits in an `isize`, which implies that it
                // also fits in a `usize`.
                None => unsafe { core::hint::unreachable_unchecked() },
            };

            // SAFETY:
            // - `slice_from_raw_parts_mut` and `.cast` both preserve the
            //   pointer's address, and `bytes` is the length of `p`, so the
            //   returned pointer addresses the same bytes as `p`
            // - `slice_from_raw_parts_mut` and `.cast` both preserve provenance
            let ptr: Ptr<'a, [u8], _> = unsafe {
                self.cast_unsized(|p: *mut T| {
                    #[allow(clippy::as_conversions)]
                    core::ptr::slice_from_raw_parts_mut(p.cast::<u8>(), bytes)
                })
            };

            let ptr = ptr.bikeshed_recall_aligned();

            // SAFETY: `ptr`'s referent begins as `Initialized`, denoting that
            // all bytes of the referent are initialized bytes. The referent
            // type is then casted to `[u8]`, whose only validity invariant is
            // that its bytes are initialized. This validity invariant is
            // satisfied by the `Initialized` invariant on the starting `ptr`.
            unsafe { ptr.assume_validity::<Valid>() }
        }
    }

    impl<'a, T, I, const N: usize> Ptr<'a, [T; N], I>
    where
        T: 'a,
        I: Invariants,
    {
        /// Casts this pointer-to-array into a slice.
        #[allow(clippy::wrong_self_convention)]
        pub(crate) fn as_slice(self) -> Ptr<'a, [T], I> {
            let slice = self.as_inner().as_slice();
            // SAFETY: Note that, by post-condition on `PtrInner::as_slice`,
            // `slice` refers to the same byte range as `self.as_inner()`.
            //
            // 6. Thus, `slice` conforms to the aliasing invariant of
            //    `I::Aliasing` because `self` does.
            // 7. By the above lemma, `slice` conforms to the alignment
            //    invariant of `I::Alignment` because `self` does.
            // 8. By the above lemma, `slice` conforms to the validity invariant
            //    of `I::Validity` because `self` does.
            unsafe { Ptr::from_inner(slice) }
        }
    }

    /// For caller convenience, these methods are generic over alignment
    /// invariant. In practice, the referent is always well-aligned, because the
    /// alignment of `[u8]` is 1.
    impl<'a, I> Ptr<'a, [u8], I>
    where
        I: Invariants<Validity = Valid>,
    {
        /// Attempts to cast `self` to a `U` using the given cast type.
        ///
        /// If `U` is a slice DST and pointer metadata (`meta`) is provided,
        /// then the cast will only succeed if it would produce an object with
        /// the given metadata.
        ///
        /// Returns `None` if the resulting `U` would be invalidly-aligned, if
        /// no `U` can fit in `self`, or if the provided pointer metadata
        /// describes an invalid instance of `U`. On success, returns a pointer
        /// to the largest-possible `U` which fits in `self`.
        ///
        /// # Safety
        ///
        /// The caller may assume that this implementation is correct, and may
        /// rely on that assumption for the soundness of their code. In
        /// particular, the caller may assume that, if `try_cast_into` returns
        /// `Some((ptr, remainder))`, then `ptr` and `remainder` refer to
        /// non-overlapping byte ranges within `self`, and that `ptr` and
        /// `remainder` entirely cover `self`. Finally:
        /// - If this is a prefix cast, `ptr` has the same address as `self`.
        /// - If this is a suffix cast, `remainder` has the same address as
        ///   `self`.
        #[inline(always)]
        pub(crate) fn try_cast_into<U, R>(
            self,
            cast_type: CastType,
            meta: Option<U::PointerMetadata>,
        ) -> Result<
            (Ptr<'a, U, (I::Aliasing, Aligned, Initialized)>, Ptr<'a, [u8], I>),
            CastError<Self, U>,
        >
        where
            I::Aliasing: Reference,
            U: 'a + ?Sized + KnownLayout + Read<I::Aliasing, R>,
        {
            let (inner, remainder) =
                self.as_inner().try_cast_into(cast_type, meta).map_err(|err| {
                    err.map_src(|inner|
                    // SAFETY: `PtrInner::try_cast_into` promises to return its
                    // original argument on error, which was originally produced
                    // by `self.as_inner()`, which is guaranteed to satisfy
                    // `Ptr`'s invariants.
                    unsafe { Ptr::from_inner(inner) })
                })?;

            // SAFETY:
            // 0. Since `U: Read<I::Aliasing, _>`, either:
            //    - `I::Aliasing` is `Exclusive`, in which case both `src` and
            //      `ptr` conform to `Exclusive`
            //    - `I::Aliasing` is `Shared` and `U` is `Immutable` (we already
            //      know that `[u8]: Immutable`). In this case, neither `U` nor
            //      `[u8]` permit mutation, and so `Shared` aliasing is
            //      satisfied.
            // 1. `ptr` conforms to the alignment invariant of `Aligned` because
            //    it is derived from `try_cast_into`, which promises that the
            //    object described by `target` is validly aligned for `U`.
            // 2. By trait bound, `self` - and thus `target` - is a bit-valid
            //    `[u8]`. All bit-valid `[u8]`s have all of their bytes
            //    initialized, so `ptr` conforms to the validity invariant of
            //    `Initialized`.
            let res = unsafe { Ptr::from_inner(inner) };

            // SAFETY:
            // 0. `self` and `remainder` both have the type `[u8]`. Thus, they
            //    have `UnsafeCell`s at the same locations. Type casting does
            //    not affect aliasing.
            // 1. `[u8]` has no alignment requirement.
            // 2. `self` has validity `Valid` and has type `[u8]`. Since
            //    `remainder` references a subset of `self`'s referent, it is
            //    also bit-valid.
            let remainder = unsafe { Ptr::from_inner(remainder) };

            Ok((res, remainder))
        }

        /// Attempts to cast `self` into a `U`, failing if all of the bytes of
        /// `self` cannot be treated as a `U`.
        ///
        /// In particular, this method fails if `self` is not validly-aligned
        /// for `U` or if `self`'s size is not a valid size for `U`.
        ///
        /// # Safety
        ///
        /// On success, the caller may assume that the returned pointer
        /// references the same byte range as `self`.
        #[allow(unused)]
        #[inline(always)]
        pub(crate) fn try_cast_into_no_leftover<U, R>(
            self,
            meta: Option<U::PointerMetadata>,
        ) -> Result<Ptr<'a, U, (I::Aliasing, Aligned, Initialized)>, CastError<Self, U>>
        where
            I::Aliasing: Reference,
            U: 'a + ?Sized + KnownLayout + Read<I::Aliasing, R>,
        {
            // TODO(#67): Remove this allow. See NonNulSlicelExt for more
            // details.
            #[allow(unstable_name_collisions)]
            match self.try_cast_into(CastType::Prefix, meta) {
                Ok((slf, remainder)) => {
                    if remainder.len() == 0 {
                        Ok(slf)
                    } else {
                        // Undo the cast so we can return the original bytes.
                        let slf = slf.as_bytes();
                        // Restore the initial alignment invariant of `self`.
                        //
                        // SAFETY: The referent type of `slf` is now equal to
                        // that of `self`, but the alignment invariants
                        // nominally differ. Since `slf` and `self` refer to the
                        // same memory and no actions have been taken that would
                        // violate the original invariants on `self`, it is
                        // sound to apply the alignment invariant of `self` onto
                        // `slf`.
                        let slf = unsafe { slf.assume_alignment::<I::Alignment>() };
                        let slf = slf.unify_invariants();
                        Err(CastError::Size(SizeError::<_, U>::new(slf)))
                    }
                }
                Err(err) => Err(err),
            }
        }
    }

    impl<'a, T, I> Ptr<'a, core::cell::UnsafeCell<T>, I>
    where
        T: 'a + ?Sized,
        I: Invariants<Aliasing = Exclusive>,
    {
        /// Converts this `Ptr` into a pointer to the underlying data.
        ///
        /// This call borrows the `UnsafeCell` mutably (at compile-time) which
        /// guarantees that we possess the only reference.
        ///
        /// This is like [`UnsafeCell::get_mut`], but for `Ptr`.
        ///
        /// [`UnsafeCell::get_mut`]: core::cell::UnsafeCell::get_mut
        #[must_use]
        #[inline(always)]
        pub fn get_mut(self) -> Ptr<'a, T, I> {
            // SAFETY:
            // - The closure uses an `as` cast, which preserves address
            //   range and provenance.
            // - Aliasing is `Exclusive`, and so we are not required to promise
            //   anything about the locations of `UnsafeCell`s.
            // - `UnsafeCell<T>` has the same bit validity as `T` [1], and so if
            //   `self` has a particular validity invariant, then the same holds
            //   of the returned `Ptr`. Technically the term "representation"
            //   doesn't guarantee this, but the subsequent sentence in the
            //   documentation makes it clear that this is the intention.
            //
            // [1] Per https://doc.rust-lang.org/1.81.0/core/cell/struct.UnsafeCell.html#memory-layout:
            //
            //   `UnsafeCell<T>` has the same in-memory representation as its
            //   inner type `T`. A consequence of this guarantee is that it is
            //   possible to convert between `T` and `UnsafeCell<T>`.
            #[allow(clippy::as_conversions)]
            let ptr = unsafe { self.transmute_unchecked(|p| p as *mut T) };

            // SAFETY: `UnsafeCell<T>` has the same alignment as `T` [1],
            // and so if `self` is guaranteed to be aligned, then so is the
            // returned `Ptr`.
            //
            // [1] Per https://doc.rust-lang.org/1.81.0/core/cell/struct.UnsafeCell.html#memory-layout:
            //
            //   `UnsafeCell<T>` has the same in-memory representation as
            //   its inner type `T`. A consequence of this guarantee is that
            //   it is possible to convert between `T` and `UnsafeCell<T>`.
            let ptr = unsafe { ptr.assume_alignment::<I::Alignment>() };
            ptr.unify_invariants()
        }
    }
}

/// Projections through the referent.
mod _project {
    use super::*;

    impl<'a, T, I> Ptr<'a, [T], I>
    where
        T: 'a,
        I: Invariants,
    {
        /// The number of slice elements in the object referenced by `self`.
        ///
        /// # Safety
        ///
        /// Unsafe code my rely on `len` satisfying the above contract.
        pub(crate) fn len(&self) -> usize {
            self.as_inner().len()
        }
    }

    impl<'a, T, I> Ptr<'a, [T], I>
    where
        T: 'a,
        I: Invariants,
        I::Aliasing: Reference,
    {
        /// Iteratively projects the elements `Ptr<T>` from `Ptr<[T]>`.
        pub(crate) fn iter(&self) -> impl Iterator<Item = Ptr<'a, T, I>> {
            // SAFETY:
            // 0. `elem` conforms to the aliasing invariant of `I::Aliasing`
            //    because projection does not impact the aliasing invariant.
            // 1. `elem`, conditionally, conforms to the validity invariant of
            //    `I::Alignment`. If `elem` is projected from data well-aligned
            //    for `[T]`, `elem` will be valid for `T`.
            // 2. `elem`, conditionally, conforms to the validity invariant of
            //    `I::Validity`. If `elem` is projected from data valid for
            //    `[T]`, `elem` will be valid for `T`.
            self.as_inner().iter().map(|elem| unsafe { Ptr::from_inner(elem) })
        }
    }
}

#[cfg(test)]
mod tests {
    use core::mem::{self, MaybeUninit};

    use super::*;
    #[allow(unused)] // Needed on our MSRV, but considered unused on later toolchains.
    use crate::util::AsAddress;
    use crate::{pointer::BecauseImmutable, util::testutil::AU64, FromBytes, Immutable};

    mod test_ptr_try_cast_into_soundness {
        use super::*;

        // This test is designed so that if `Ptr::try_cast_into_xxx` are
        // buggy, it will manifest as unsoundness that Miri can detect.

        // - If `size_of::<T>() == 0`, `N == 4`
        // - Else, `N == 4 * size_of::<T>()`
        //
        // Each test will be run for each metadata in `metas`.
        fn test<T, I, const N: usize>(metas: I)
        where
            T: ?Sized + KnownLayout + Immutable + FromBytes,
            I: IntoIterator<Item = Option<T::PointerMetadata>> + Clone,
        {
            let mut bytes = [MaybeUninit::<u8>::uninit(); N];
            let initialized = [MaybeUninit::new(0u8); N];
            for start in 0..=bytes.len() {
                for end in start..=bytes.len() {
                    // Set all bytes to uninitialized other than those in
                    // the range we're going to pass to `try_cast_from`.
                    // This allows Miri to detect out-of-bounds reads
                    // because they read uninitialized memory. Without this,
                    // some out-of-bounds reads would still be in-bounds of
                    // `bytes`, and so might spuriously be accepted.
                    bytes = [MaybeUninit::<u8>::uninit(); N];
                    let bytes = &mut bytes[start..end];
                    // Initialize only the byte range we're going to pass to
                    // `try_cast_from`.
                    bytes.copy_from_slice(&initialized[start..end]);

                    let bytes = {
                        let bytes: *const [MaybeUninit<u8>] = bytes;
                        #[allow(clippy::as_conversions)]
                        let bytes = bytes as *const [u8];
                        // SAFETY: We just initialized these bytes to valid
                        // `u8`s.
                        unsafe { &*bytes }
                    };

                    // SAFETY: The bytes in `slf` must be initialized.
                    unsafe fn validate_and_get_len<T: ?Sized + KnownLayout + FromBytes>(
                        slf: Ptr<'_, T, (Shared, Aligned, Initialized)>,
                    ) -> usize {
                        let t = slf.bikeshed_recall_valid().as_ref();

                        let bytes = {
                            let len = mem::size_of_val(t);
                            let t: *const T = t;
                            // SAFETY:
                            // - We know `t`'s bytes are all initialized
                            //   because we just read it from `slf`, which
                            //   points to an initialized range of bytes. If
                            //   there's a bug and this doesn't hold, then
                            //   that's exactly what we're hoping Miri will
                            //   catch!
                            // - Since `T: FromBytes`, `T` doesn't contain
                            //   any `UnsafeCell`s, so it's okay for `t: T`
                            //   and a `&[u8]` to the same memory to be
                            //   alive concurrently.
                            unsafe { core::slice::from_raw_parts(t.cast::<u8>(), len) }
                        };

                        // This assertion ensures that `t`'s bytes are read
                        // and compared to another value, which in turn
                        // ensures that Miri gets a chance to notice if any
                        // of `t`'s bytes are uninitialized, which they
                        // shouldn't be (see the comment above).
                        assert_eq!(bytes, vec![0u8; bytes.len()]);

                        mem::size_of_val(t)
                    }

                    for meta in metas.clone().into_iter() {
                        for cast_type in [CastType::Prefix, CastType::Suffix] {
                            if let Ok((slf, remaining)) = Ptr::from_ref(bytes)
                                .try_cast_into::<T, BecauseImmutable>(cast_type, meta)
                            {
                                // SAFETY: All bytes in `bytes` have been
                                // initialized.
                                let len = unsafe { validate_and_get_len(slf) };
                                assert_eq!(remaining.len(), bytes.len() - len);
                                #[allow(unstable_name_collisions)]
                                let bytes_addr = bytes.as_ptr().addr();
                                #[allow(unstable_name_collisions)]
                                let remaining_addr =
                                    remaining.as_inner().as_non_null().as_ptr().addr();
                                match cast_type {
                                    CastType::Prefix => {
                                        assert_eq!(remaining_addr, bytes_addr + len)
                                    }
                                    CastType::Suffix => assert_eq!(remaining_addr, bytes_addr),
                                }

                                if let Some(want) = meta {
                                    let got = KnownLayout::pointer_to_metadata(
                                        slf.as_inner().as_non_null().as_ptr(),
                                    );
                                    assert_eq!(got, want);
                                }
                            }
                        }

                        if let Ok(slf) = Ptr::from_ref(bytes)
                            .try_cast_into_no_leftover::<T, BecauseImmutable>(meta)
                        {
                            // SAFETY: All bytes in `bytes` have been
                            // initialized.
                            let len = unsafe { validate_and_get_len(slf) };
                            assert_eq!(len, bytes.len());

                            if let Some(want) = meta {
                                let got = KnownLayout::pointer_to_metadata(
                                    slf.as_inner().as_non_null().as_ptr(),
                                );
                                assert_eq!(got, want);
                            }
                        }
                    }
                }
            }
        }

        #[derive(FromBytes, KnownLayout, Immutable)]
        #[repr(C)]
        struct SliceDst<T> {
            a: u8,
            trailing: [T],
        }

        // Each test case becomes its own `#[test]` function. We do this because
        // this test in particular takes far, far longer to execute under Miri
        // than all of our other tests combined. Previously, we had these
        // execute sequentially in a single test function. We run Miri tests in
        // parallel in CI, but this test being sequential meant that most of
        // that parallelism was wasted, as all other tests would finish in a
        // fraction of the total execution time, leaving this test to execute on
        // a single thread for the remainder of the test. By putting each test
        // case in its own function, we permit better use of available
        // parallelism.
        macro_rules! test {
            ($test_name:ident: $ty:ty) => {
                #[test]
                #[allow(non_snake_case)]
                fn $test_name() {
                    const S: usize = core::mem::size_of::<$ty>();
                    const N: usize = if S == 0 { 4 } else { S * 4 };
                    test::<$ty, _, N>([None]);

                    // If `$ty` is a ZST, then we can't pass `None` as the
                    // pointer metadata, or else computing the correct trailing
                    // slice length will panic.
                    if S == 0 {
                        test::<[$ty], _, N>([Some(0), Some(1), Some(2), Some(3)]);
                        test::<SliceDst<$ty>, _, N>([Some(0), Some(1), Some(2), Some(3)]);
                    } else {
                        test::<[$ty], _, N>([None, Some(0), Some(1), Some(2), Some(3)]);
                        test::<SliceDst<$ty>, _, N>([None, Some(0), Some(1), Some(2), Some(3)]);
                    }
                }
            };
            ($ty:ident) => {
                test!($ty: $ty);
            };
            ($($ty:ident),*) => { $(test!($ty);)* }
        }

        test!(empty_tuple: ());
        test!(u8, u16, u32, u64, u128, usize, AU64);
        test!(i8, i16, i32, i64, i128, isize);
        test!(f32, f64);
    }

    #[test]
    fn test_try_cast_into_explicit_count() {
        macro_rules! test {
            ($ty:ty, $bytes:expr, $elems:expr, $expect:expr) => {{
                let bytes = [0u8; $bytes];
                let ptr = Ptr::from_ref(&bytes[..]);
                let res =
                    ptr.try_cast_into::<$ty, BecauseImmutable>(CastType::Prefix, Some($elems));
                if let Some(expect) = $expect {
                    let (ptr, _) = res.unwrap();
                    assert_eq!(
                        KnownLayout::pointer_to_metadata(ptr.as_inner().as_non_null().as_ptr()),
                        expect
                    );
                } else {
                    let _ = res.unwrap_err();
                }
            }};
        }

        #[derive(KnownLayout, Immutable)]
        #[repr(C)]
        struct ZstDst {
            u: [u8; 8],
            slc: [()],
        }

        test!(ZstDst, 8, 0, Some(0));
        test!(ZstDst, 7, 0, None);

        test!(ZstDst, 8, usize::MAX, Some(usize::MAX));
        test!(ZstDst, 7, usize::MAX, None);

        #[derive(KnownLayout, Immutable)]
        #[repr(C)]
        struct Dst {
            u: [u8; 8],
            slc: [u8],
        }

        test!(Dst, 8, 0, Some(0));
        test!(Dst, 7, 0, None);

        test!(Dst, 9, 1, Some(1));
        test!(Dst, 8, 1, None);

        // If we didn't properly check for overflow, this would cause the
        // metadata to overflow to 0, and thus the cast would spuriously
        // succeed.
        test!(Dst, 8, usize::MAX - 8 + 1, None);
    }
}
