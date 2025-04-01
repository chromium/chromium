// Copyright 2024 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

use core::{marker::PhantomData, ops::Range, ptr::NonNull};

#[allow(unused_imports)]
use crate::util::polyfills::NumExt as _;
use crate::{
    layout::{CastType, DstLayout, MetadataCastError},
    util::AsAddress,
    AlignmentError, CastError, KnownLayout, PointerMetadata, SizeError,
};

pub(crate) use _def::PtrInner;

mod _def {
    use super::*;
    /// The inner pointer stored inside a [`Ptr`][crate::Ptr].
    ///
    /// `PtrInner<'a, T>` is [covariant] in `'a` and invariant in `T`.
    ///
    /// [covariant]: https://doc.rust-lang.org/reference/subtyping.html
    pub(crate) struct PtrInner<'a, T>
    where
        T: ?Sized,
    {
        /// # Invariants
        ///
        /// 0. If `ptr`'s referent is not zero sized, then `ptr` has valid
        ///    provenance for its referent, which is entirely contained in some
        ///    Rust allocation, `A`.
        /// 1. If `ptr`'s referent is not zero sized, `A` is guaranteed to live
        ///    for at least `'a`.
        ///
        /// # Postconditions
        ///
        /// By virtue of these invariants, code may assume the following, which
        /// are logical implications of the invariants:
        /// - `ptr`'s referent is not larger than `isize::MAX` bytes \[1\]
        /// - `ptr`'s referent does not wrap around the address space \[1\]
        ///
        /// \[1\] Per <https://doc.rust-lang.org/1.85.0/std/ptr/index.html#allocated-object>:
        ///
        ///   For any allocated object with `base` address, `size`, and a set of
        ///   `addresses`, the following are guaranteed:
        ///   ...
        ///   - `size <= isize::MAX`
        ///
        ///   As a consequence of these guarantees, given any address `a` within
        ///   the set of addresses of an allocated object:
        ///   ...
        ///   - It is guaranteed that, given `o = a - base` (i.e., the offset of
        ///     `a` within the allocated object), `base + o` will not wrap around
        ///     the address space (in other words, will not overflow `usize`)
        ptr: NonNull<T>,
        // SAFETY: `&'a UnsafeCell<T>` is covariant in `'a` and invariant in `T`
        // [1]. We use this construction rather than the equivalent `&mut T`,
        // because our MSRV of 1.65 prohibits `&mut` types in const contexts.
        //
        // [1] https://doc.rust-lang.org/1.81.0/reference/subtyping.html#variance
        _marker: PhantomData<&'a core::cell::UnsafeCell<T>>,
    }

    impl<'a, T: 'a + ?Sized> Copy for PtrInner<'a, T> {}
    impl<'a, T: 'a + ?Sized> Clone for PtrInner<'a, T> {
        fn clone(&self) -> PtrInner<'a, T> {
            // SAFETY: None of the invariants on `ptr` are affected by having
            // multiple copies of a `PtrInner`.
            *self
        }
    }

    impl<'a, T: 'a + ?Sized> PtrInner<'a, T> {
        /// Constructs a `Ptr` from a [`NonNull`].
        ///
        /// # Safety
        ///
        /// The caller promises that:
        ///
        /// 0. If `ptr`'s referent is not zero sized, then `ptr` has valid
        ///    provenance for its referent, which is entirely contained in some
        ///    Rust allocation, `A`.
        /// 1. If `ptr`'s referent is not zero sized, `A` is guaranteed to live
        ///    for at least `'a`.
        pub(crate) const unsafe fn new(ptr: NonNull<T>) -> PtrInner<'a, T> {
            // SAFETY: The caller has promised to satisfy all safety invariants
            // of `PtrInner`.
            Self { ptr, _marker: PhantomData }
        }

        /// Converts this `PtrInner<T>` to a [`NonNull<T>`].
        ///
        /// Note that this method does not consume `self`. The caller should
        /// watch out for `unsafe` code which uses the returned `NonNull` in a
        /// way that violates the safety invariants of `self`.
        pub(crate) const fn as_non_null(&self) -> NonNull<T> {
            self.ptr
        }
    }
}

impl<'a, T: ?Sized> PtrInner<'a, T> {
    /// Constructs a `PtrInner` from a reference.
    #[inline]
    pub(crate) fn from_ref(ptr: &'a T) -> Self {
        let ptr = NonNull::from(ptr);
        // SAFETY:
        // 0. If `ptr`'s referent is not zero sized, then `ptr`, by invariant on
        //    `&'a T` [1], has valid provenance for its referent, which is
        //    entirely contained in some Rust allocation, `A`.
        // 1. If `ptr`'s referent is not zero sized, then `A`, by invariant on
        //    `&'a T`, is guaranteed to live for at least `'a`.
        //
        // [1] Per https://doc.rust-lang.org/1.85.0/std/primitive.reference.html#safety:
        //
        //   For all types, `T: ?Sized`, and for all `t: &T` or `t: &mut T`,
        //   when such values cross an API boundary, the following invariants
        //   must generally be upheld:
        //   ...
        //   - if `size_of_val(t) > 0`, then `t` is dereferenceable for
        //     `size_of_val(t)` many bytes
        //
        //   If `t` points at address `a`, being “dereferenceable” for N bytes
        //   means that the memory range `[a, a + N)` is all contained within a
        //   single allocated object.
        unsafe { Self::new(ptr) }
    }

    /// Constructs a `PtrInner` from a mutable reference.
    #[inline]
    pub(crate) fn from_mut(ptr: &'a mut T) -> Self {
        let ptr = NonNull::from(ptr);
        // SAFETY:
        // 0. If `ptr`'s referent is not zero sized, then `ptr`, by invariant on
        //    `&'a mut T` [1], has valid provenance for its referent, which is
        //    entirely contained in some Rust allocation, `A`.
        // 1. If `ptr`'s referent is not zero sized, then `A`, by invariant on
        //    `&'a mut T`, is guaranteed to live for at least `'a`.
        //
        // [1] Per https://doc.rust-lang.org/1.85.0/std/primitive.reference.html#safety:
        //
        //   For all types, `T: ?Sized`, and for all `t: &T` or `t: &mut T`,
        //   when such values cross an API boundary, the following invariants
        //   must generally be upheld:
        //   ...
        //   - if `size_of_val(t) > 0`, then `t` is dereferenceable for
        //     `size_of_val(t)` many bytes
        //
        //   If `t` points at address `a`, being “dereferenceable” for N bytes
        //   means that the memory range `[a, a + N)` is all contained within a
        //   single allocated object.
        unsafe { Self::new(ptr) }
    }
}

#[allow(clippy::needless_lifetimes)]
impl<'a, T> PtrInner<'a, [T]> {
    /// Creates a pointer which addresses the given `range` of self.
    ///
    /// # Safety
    ///
    /// `range` is a valid range (`start <= end`) and `end <= self.len()`.
    pub(crate) unsafe fn slice_unchecked(self, range: Range<usize>) -> Self {
        let base = self.as_non_null().cast::<T>().as_ptr();

        // SAFETY: The caller promises that `start <= end <= self.len()`. By
        // invariant, if `self`'s referent is not zero-sized, then `self` refers
        // to a byte range which is contained within a single allocation, which
        // is no more than `isize::MAX` bytes long, and which does not wrap
        // around the address space. Thus, this pointer arithmetic remains
        // in-bounds of the same allocation, and does not wrap around the
        // address space. The offset (in bytes) does not overflow `isize`.
        //
        // If `self`'s referent is zero-sized, then these conditions are
        // trivially satisfied.
        let base = unsafe { base.add(range.start) };

        // SAFETY: The caller promises that `start <= end`, and so this will not
        // underflow.
        #[allow(unstable_name_collisions, clippy::incompatible_msrv)]
        let len = unsafe { range.end.unchecked_sub(range.start) };

        let ptr = core::ptr::slice_from_raw_parts_mut(base, len);

        // SAFETY: By invariant, `self`'s referent is either a ZST or lives
        // entirely in an allocation. `ptr` points inside of or one byte past
        // the end of that referent. Thus, in either case, `ptr` is non-null.
        let ptr = unsafe { NonNull::new_unchecked(ptr) };

        // SAFETY:
        //
        // Lemma 0: `ptr` addresses a subset of the bytes addressed by `self`,
        //          and has the same provenance. Proof: The caller guarantees
        //          that `start <= end <= self.len()`. Thus, `base` is in-bounds
        //          of `self`, and `base + (end - start)` is also in-bounds of
        //          self. Finally, `ptr` is constructed using
        //          provenance-preserving operations.
        //
        // 0. Per Lemma 0 and by invariant on `self`, if `ptr`'s referent is not
        //    zero sized, then `ptr` has valid provenance for its referent,
        //    which is entirely contained in some Rust allocation, `A`.
        // 1. Per Lemma 0 and by invariant on `self`, if `ptr`'s referent is not
        //    zero sized, then `A` is guaranteed to live for at least `'a`.
        unsafe { PtrInner::new(ptr) }
    }

    /// Splits the slice in two.
    ///
    /// # Safety
    ///
    /// The caller promises that `l_len <= self.len()`.
    ///
    /// Given `let (left, right) = ptr.split_at(l_len)`, it is guaranteed
    /// that `left` and `right` are contiguous and non-overlapping.
    pub(crate) unsafe fn split_at(self, l_len: usize) -> (Self, Self) {
        // SAFETY: The caller promises that `l_len <= self.len()`.
        // Trivially, `0 <= l_len`.
        let left = unsafe { self.slice_unchecked(0..l_len) };

        // SAFETY: The caller promises that `l_len <= self.len() =
        // slf.len()`. Trivially, `slf.len() <= slf.len()`.
        let right = unsafe { self.slice_unchecked(l_len..self.len()) };

        // SAFETY: `left` and `right` are non-overlapping. Proof: `left` is
        // constructed from `slf` with `l_len` as its (exclusive) upper
        // bound, while `right` is constructed from `slf` with `l_len` as
        // its (inclusive) lower bound. Thus, no index is a member of both
        // ranges.
        (left, right)
    }

    /// Iteratively projects the elements `PtrInner<T>` from `PtrInner<[T]>`.
    pub(crate) fn iter(&self) -> impl Iterator<Item = PtrInner<'a, T>> {
        // TODO(#429): Once `NonNull::cast` documents that it preserves
        // provenance, cite those docs.
        let base = self.as_non_null().cast::<T>().as_ptr();
        (0..self.len()).map(move |i| {
            // TODO(https://github.com/rust-lang/rust/issues/74265): Use
            // `NonNull::get_unchecked_mut`.

            // SAFETY: If the following conditions are not satisfied
            // `pointer::cast` may induce Undefined Behavior [1]:
            //
            // > - The computed offset, `count * size_of::<T>()` bytes, must not
            // >   overflow `isize``.
            // > - If the computed offset is non-zero, then `self` must be
            // >   derived from a pointer to some allocated object, and the
            // >   entire memory range between `self` and the result must be in
            // >   bounds of that allocated object. In particular, this range
            // >   must not “wrap around” the edge of the address space.
            //
            // [1] https://doc.rust-lang.org/std/primitive.pointer.html#method.add
            //
            // We satisfy both of these conditions here:
            // - By invariant on `Ptr`, `self` addresses a byte range whose
            //   length fits in an `isize`. Since `elem` is contained in `self`,
            //   the computed offset of `elem` must fit within `isize.`
            // - If the computed offset is non-zero, then this means that the
            //   referent is not zero-sized. In this case, `base` points to an
            //   allocated object (by invariant on `self`). Thus:
            //   - By contract, `self.len()` accurately reflects the number of
            //     elements in the slice. `i` is in bounds of `c.len()` by
            //     construction, and so the result of this addition cannot
            //     overflow past the end of the allocation referred to by `c`.
            //   - By invariant on `Ptr`, `self` addresses a byte range which
            //     does not wrap around the address space. Since `elem` is
            //     contained in `self`, the computed offset of `elem` must wrap
            //     around the address space.
            //
            // TODO(#429): Once `pointer::add` documents that it preserves
            // provenance, cite those docs.
            let elem = unsafe { base.add(i) };

            // SAFETY: `elem` must not be null. `base` is constructed from a
            // `NonNull` pointer, and the addition that produces `elem` must not
            // overflow or wrap around, so `elem >= base > 0`.
            //
            // TODO(#429): Once `NonNull::new_unchecked` documents that it
            // preserves provenance, cite those docs.
            let elem = unsafe { NonNull::new_unchecked(elem) };

            // SAFETY: The safety invariants of `Ptr::new` (see definition) are
            // satisfied:
            // 0. If `elem`'s referent is not zero sized, then `elem` has valid
            //    provenance for its referent, because it derived from `self`
            //    using a series of provenance-preserving operations, and
            //    because `self` has valid provenance for its referent. By the
            //    same argument, `elem`'s referent is entirely contained within
            //    the same allocated object as `self`'s referent.
            // 1. If `elem`'s referent is not zero sized, then the allocation of
            //    `elem` is guaranteed to live for at least `'a`, because `elem`
            //    is entirely contained in `self`, which lives for at least `'a`
            //    by invariant on `Ptr`.
            unsafe { PtrInner::new(elem) }
        })
    }

    /// The number of slice elements in the object referenced by `self`.
    ///
    /// # Safety
    ///
    /// Unsafe code my rely on `len` satisfying the above contract.
    pub(crate) fn len(&self) -> usize {
        self.trailing_slice_len()
    }
}

#[allow(clippy::needless_lifetimes)]
impl<'a, T> PtrInner<'a, T>
where
    T: ?Sized + KnownLayout<PointerMetadata = usize>,
{
    /// The number of trailing slice elements in the object referenced by
    /// `self`.
    ///
    /// # Safety
    ///
    /// Unsafe code my rely on `trailing_slice_len` satisfying the above
    /// contract.
    pub(super) fn trailing_slice_len(&self) -> usize {
        T::pointer_to_metadata(self.as_non_null().as_ptr())
    }
}

impl<'a, T, const N: usize> PtrInner<'a, [T; N]> {
    /// Casts this pointer-to-array into a slice.
    ///
    /// # Safety
    ///
    /// Callers may assume that the returned `PtrInner` references the same
    /// address and length as `self`.
    #[allow(clippy::wrong_self_convention)]
    pub(crate) fn as_slice(self) -> PtrInner<'a, [T]> {
        let start = self.as_non_null().cast::<T>().as_ptr();
        let slice = core::ptr::slice_from_raw_parts_mut(start, N);
        // SAFETY: `slice` is not null, because it is derived from `start`
        // which is non-null.
        let slice = unsafe { NonNull::new_unchecked(slice) };
        // SAFETY: Lemma: In the following safety arguments, note that `slice`
        // is derived from `self` in two steps: first, by casting `self: [T; N]`
        // to `start: T`, then by constructing a pointer to a slice starting at
        // `start` of length `N`. As a result, `slice` references exactly the
        // same allocation as `self`, if any.
        //
        // 0. By the above lemma, if `slice`'s referent is not zero sized, then
        //    `slice` has the same referent as `self`. By invariant on `self`,
        //    this referent is entirely contained within some allocation, `A`.
        //    Because `slice` was constructed using provenance-preserving
        //    operations, it has provenance for its entire referent.
        // 1. By the above lemma, if `slice`'s referent is not zero sized, then
        //    `A` is guaranteed to live for at least `'a`, because it is derived
        //    from the same allocation as `self`, which, by invariant on `Ptr`,
        //    lives for at least `'a`.
        unsafe { PtrInner::new(slice) }
    }
}

impl<'a> PtrInner<'a, [u8]> {
    /// Attempts to cast `self` to a `U` using the given cast type.
    ///
    /// If `U` is a slice DST and pointer metadata (`meta`) is provided, then
    /// the cast will only succeed if it would produce an object with the given
    /// metadata.
    ///
    /// Returns `None` if the resulting `U` would be invalidly-aligned, if no
    /// `U` can fit in `self`, or if the provided pointer metadata describes an
    /// invalid instance of `U`. On success, returns a pointer to the
    /// largest-possible `U` which fits in `self`.
    ///
    /// # Safety
    ///
    /// The caller may assume that this implementation is correct, and may rely
    /// on that assumption for the soundness of their code. In particular, the
    /// caller may assume that, if `try_cast_into` returns `Some((ptr,
    /// remainder))`, then `ptr` and `remainder` refer to non-overlapping byte
    /// ranges within `self`, and that `ptr` and `remainder` entirely cover
    /// `self`. Finally:
    /// - If this is a prefix cast, `ptr` has the same address as `self`.
    /// - If this is a suffix cast, `remainder` has the same address as `self`.
    #[inline]
    pub(crate) fn try_cast_into<U>(
        self,
        cast_type: CastType,
        meta: Option<U::PointerMetadata>,
    ) -> Result<(PtrInner<'a, U>, PtrInner<'a, [u8]>), CastError<Self, U>>
    where
        U: 'a + ?Sized + KnownLayout,
    {
        let layout = match meta {
            None => U::LAYOUT,
            // This can return `None` if the metadata describes an object
            // which can't fit in an `isize`.
            Some(meta) => {
                let size = match meta.size_for_metadata(U::LAYOUT) {
                    Some(size) => size,
                    None => return Err(CastError::Size(SizeError::new(self))),
                };
                DstLayout { align: U::LAYOUT.align, size_info: crate::SizeInfo::Sized { size } }
            }
        };
        // PANICS: By invariant, the byte range addressed by
        // `self.as_non_null()` does not wrap around the address space. This
        // implies that the sum of the address (represented as a `usize`) and
        // length do not overflow `usize`, as required by
        // `validate_cast_and_convert_metadata`. Thus, this call to
        // `validate_cast_and_convert_metadata` will only panic if `U` is a DST
        // whose trailing slice element is zero-sized.
        let maybe_metadata = layout.validate_cast_and_convert_metadata(
            AsAddress::addr(self.as_non_null().as_ptr()),
            self.len(),
            cast_type,
        );

        let (elems, split_at) = match maybe_metadata {
            Ok((elems, split_at)) => (elems, split_at),
            Err(MetadataCastError::Alignment) => {
                // SAFETY: Since `validate_cast_and_convert_metadata` returned
                // an alignment error, `U` must have an alignment requirement
                // greater than one.
                let err = unsafe { AlignmentError::<_, U>::new_unchecked(self) };
                return Err(CastError::Alignment(err));
            }
            Err(MetadataCastError::Size) => return Err(CastError::Size(SizeError::new(self))),
        };

        // SAFETY: `validate_cast_and_convert_metadata` promises to return
        // `split_at <= self.len()`.
        let (l_slice, r_slice) = unsafe { self.split_at(split_at) };

        let (target, remainder) = match cast_type {
            CastType::Prefix => (l_slice, r_slice),
            CastType::Suffix => (r_slice, l_slice),
        };

        let base = target.as_non_null().cast::<u8>();

        let elems = <U as KnownLayout>::PointerMetadata::from_elem_count(elems);
        // For a slice DST type, if `meta` is `Some(elems)`, then we synthesize
        // `layout` to describe a sized type whose size is equal to the size of
        // the instance that we are asked to cast. For sized types,
        // `validate_cast_and_convert_metadata` returns `elems == 0`. Thus, in
        // this case, we need to use the `elems` passed by the caller, not the
        // one returned by `validate_cast_and_convert_metadata`.
        let elems = meta.unwrap_or(elems);

        let ptr = U::raw_from_ptr_len(base, elems);

        // SAFETY:
        // 0. By invariant, if `target`'s referent is not zero sized, then
        //    `target` has provenance valid for some Rust allocation, `A`.
        //    Because `ptr` is derived from `target` via provenance-preserving
        //    operations, `ptr` will also have provenance valid for its entire
        //    referent.
        // 1. `validate_cast_and_convert_metadata` promises that the object
        //    described by `elems` and `split_at` lives at a byte range which is
        //    a subset of the input byte range. Thus, by invariant, if
        //    `target`'s referent is not zero sized, then `target` refers to an
        //    allocation which is guaranteed to live for at least `'a`, and thus
        //    so does `ptr`.
        Ok((unsafe { PtrInner::new(ptr) }, remainder))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_split_at() {
        const N: usize = 16;
        let arr = [1; N];
        let ptr = PtrInner::from_ref(&arr).as_slice();
        for i in 0..=N {
            assert_eq!(ptr.len(), N);
            // SAFETY: `i` is in bounds by construction.
            let (l, r) = unsafe { ptr.split_at(i) };
            // SAFETY: Points to a valid value by construction.
            #[allow(clippy::undocumented_unsafe_blocks)] // Clippy false positive
            let l_sum: usize = l
                .iter()
                .map(|ptr| unsafe { core::ptr::read_unaligned(ptr.as_non_null().as_ptr()) })
                .sum();
            // SAFETY: Points to a valid value by construction.
            #[allow(clippy::undocumented_unsafe_blocks)] // Clippy false positive
            let r_sum: usize = r
                .iter()
                .map(|ptr| unsafe { core::ptr::read_unaligned(ptr.as_non_null().as_ptr()) })
                .sum();
            assert_eq!(l_sum, i);
            assert_eq!(r_sum, N - i);
            assert_eq!(l_sum + r_sum, N);
        }
    }
}
