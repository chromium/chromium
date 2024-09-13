// Copyright 2023 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

use core::{
    cmp::Ordering,
    fmt::{self, Debug, Display, Formatter},
    hash::Hash,
    mem::{self, ManuallyDrop},
    ops::{Deref, DerefMut},
    ptr,
};

use super::*;

/// A type with no alignment requirement.
///
/// An `Unalign` wraps a `T`, removing any alignment requirement. `Unalign<T>`
/// has the same size and bit validity as `T`, but not necessarily the same
/// alignment [or ABI]. This is useful if a type with an alignment requirement
/// needs to be read from a chunk of memory which provides no alignment
/// guarantees.
///
/// Since `Unalign` has no alignment requirement, the inner `T` may not be
/// properly aligned in memory. There are five ways to access the inner `T`:
/// - by value, using [`get`] or [`into_inner`]
/// - by reference inside of a callback, using [`update`]
/// - fallibly by reference, using [`try_deref`] or [`try_deref_mut`]; these can
///   fail if the `Unalign` does not satisfy `T`'s alignment requirement at
///   runtime
/// - unsafely by reference, using [`deref_unchecked`] or
///   [`deref_mut_unchecked`]; it is the caller's responsibility to ensure that
///   the `Unalign` satisfies `T`'s alignment requirement
/// - (where `T: Unaligned`) infallibly by reference, using [`Deref::deref`] or
///   [`DerefMut::deref_mut`]
///
/// [or ABI]: https://github.com/google/zerocopy/issues/164
/// [`get`]: Unalign::get
/// [`into_inner`]: Unalign::into_inner
/// [`update`]: Unalign::update
/// [`try_deref`]: Unalign::try_deref
/// [`try_deref_mut`]: Unalign::try_deref_mut
/// [`deref_unchecked`]: Unalign::deref_unchecked
/// [`deref_mut_unchecked`]: Unalign::deref_mut_unchecked
// NOTE: This type is sound to use with types that need to be dropped. The
// reason is that the compiler-generated drop code automatically moves all
// values to aligned memory slots before dropping them in-place. This is not
// well-documented, but it's hinted at in places like [1] and [2]. However, this
// also means that `T` must be `Sized`; unless something changes, we can never
// support unsized `T`. [3]
//
// [1] https://github.com/rust-lang/rust/issues/54148#issuecomment-420529646
// [2] https://github.com/google/zerocopy/pull/126#discussion_r1018512323
// [3] https://github.com/google/zerocopy/issues/209
#[allow(missing_debug_implementations)]
#[derive(Default, Copy)]
#[cfg_attr(
    any(feature = "derive", test),
    derive(KnownLayout, FromZeroes, FromBytes, AsBytes, Unaligned)
)]
#[repr(C, packed)]
pub struct Unalign<T>(T);

#[cfg(not(any(feature = "derive", test)))]
impl_known_layout!(T => Unalign<T>);

safety_comment! {
    /// SAFETY:
    /// - `Unalign<T>` is `repr(packed)`, so it is unaligned regardless of the
    ///   alignment of `T`, and so we don't require that `T: Unaligned`
    /// - `Unalign<T>` has the same bit validity as `T`, and so it is
    ///   `FromZeroes`, `FromBytes`, or `AsBytes` exactly when `T` is as well.
    impl_or_verify!(T => Unaligned for Unalign<T>);
    impl_or_verify!(T: FromZeroes => FromZeroes for Unalign<T>);
    impl_or_verify!(T: FromBytes => FromBytes for Unalign<T>);
    impl_or_verify!(T: AsBytes => AsBytes for Unalign<T>);
}

// Note that `Unalign: Clone` only if `T: Copy`. Since the inner `T` may not be
// aligned, there's no way to safely call `T::clone`, and so a `T: Clone` bound
// is not sufficient to implement `Clone` for `Unalign`.
impl<T: Copy> Clone for Unalign<T> {
    #[inline(always)]
    fn clone(&self) -> Unalign<T> {
        *self
    }
}

impl<T> Unalign<T> {
    /// Constructs a new `Unalign`.
    #[inline(always)]
    pub const fn new(val: T) -> Unalign<T> {
        Unalign(val)
    }

    /// Consumes `self`, returning the inner `T`.
    #[inline(always)]
    pub const fn into_inner(self) -> T {
        // Use this instead of `mem::transmute` since the latter can't tell
        // that `Unalign<T>` and `T` have the same size.
        #[repr(C)]
        union Transmute<T> {
            u: ManuallyDrop<Unalign<T>>,
            t: ManuallyDrop<T>,
        }

        // SAFETY: Since `Unalign` is `#[repr(C, packed)]`, it has the same
        // layout as `T`. `ManuallyDrop<U>` is guaranteed to have the same
        // layout as `U`, and so `ManuallyDrop<Unalign<T>>` has the same layout
        // as `ManuallyDrop<T>`. Since `Transmute<T>` is `#[repr(C)]`, its `t`
        // and `u` fields both start at the same offset (namely, 0) within the
        // union.
        //
        // We do this instead of just destructuring in order to prevent
        // `Unalign`'s `Drop::drop` from being run, since dropping is not
        // supported in `const fn`s.
        //
        // TODO(https://github.com/rust-lang/rust/issues/73255): Destructure
        // instead of using unsafe.
        unsafe { ManuallyDrop::into_inner(Transmute { u: ManuallyDrop::new(self) }.t) }
    }

    /// Attempts to return a reference to the wrapped `T`, failing if `self` is
    /// not properly aligned.
    ///
    /// If `self` does not satisfy `mem::align_of::<T>()`, then it is unsound to
    /// return a reference to the wrapped `T`, and `try_deref` returns `None`.
    ///
    /// If `T: Unaligned`, then `Unalign<T>` implements [`Deref`], and callers
    /// may prefer [`Deref::deref`], which is infallible.
    #[inline(always)]
    pub fn try_deref(&self) -> Option<&T> {
        if !util::aligned_to::<_, T>(self) {
            return None;
        }

        // SAFETY: `deref_unchecked`'s safety requirement is that `self` is
        // aligned to `align_of::<T>()`, which we just checked.
        unsafe { Some(self.deref_unchecked()) }
    }

    /// Attempts to return a mutable reference to the wrapped `T`, failing if
    /// `self` is not properly aligned.
    ///
    /// If `self` does not satisfy `mem::align_of::<T>()`, then it is unsound to
    /// return a reference to the wrapped `T`, and `try_deref_mut` returns
    /// `None`.
    ///
    /// If `T: Unaligned`, then `Unalign<T>` implements [`DerefMut`], and
    /// callers may prefer [`DerefMut::deref_mut`], which is infallible.
    #[inline(always)]
    pub fn try_deref_mut(&mut self) -> Option<&mut T> {
        if !util::aligned_to::<_, T>(&*self) {
            return None;
        }

        // SAFETY: `deref_mut_unchecked`'s safety requirement is that `self` is
        // aligned to `align_of::<T>()`, which we just checked.
        unsafe { Some(self.deref_mut_unchecked()) }
    }

    /// Returns a reference to the wrapped `T` without checking alignment.
    ///
    /// If `T: Unaligned`, then `Unalign<T>` implements[ `Deref`], and callers
    /// may prefer [`Deref::deref`], which is safe.
    ///
    /// # Safety
    ///
    /// If `self` does not satisfy `mem::align_of::<T>()`, then
    /// `self.deref_unchecked()` may cause undefined behavior.
    #[inline(always)]
    pub const unsafe fn deref_unchecked(&self) -> &T {
        // SAFETY: `Unalign<T>` is `repr(transparent)`, so there is a valid `T`
        // at the same memory location as `self`. It has no alignment guarantee,
        // but the caller has promised that `self` is properly aligned, so we
        // know that it is sound to create a reference to `T` at this memory
        // location.
        //
        // We use `mem::transmute` instead of `&*self.get_ptr()` because
        // dereferencing pointers is not stable in `const` on our current MSRV
        // (1.56 as of this writing).
        unsafe { mem::transmute(self) }
    }

    /// Returns a mutable reference to the wrapped `T` without checking
    /// alignment.
    ///
    /// If `T: Unaligned`, then `Unalign<T>` implements[ `DerefMut`], and
    /// callers may prefer [`DerefMut::deref_mut`], which is safe.
    ///
    /// # Safety
    ///
    /// If `self` does not satisfy `mem::align_of::<T>()`, then
    /// `self.deref_mut_unchecked()` may cause undefined behavior.
    #[inline(always)]
    pub unsafe fn deref_mut_unchecked(&mut self) -> &mut T {
        // SAFETY: `self.get_mut_ptr()` returns a raw pointer to a valid `T` at
        // the same memory location as `self`. It has no alignment guarantee,
        // but the caller has promised that `self` is properly aligned, so we
        // know that the pointer itself is aligned, and thus that it is sound to
        // create a reference to a `T` at this memory location.
        unsafe { &mut *self.get_mut_ptr() }
    }

    /// Gets an unaligned raw pointer to the inner `T`.
    ///
    /// # Safety
    ///
    /// The returned raw pointer is not necessarily aligned to
    /// `align_of::<T>()`. Most functions which operate on raw pointers require
    /// those pointers to be aligned, so calling those functions with the result
    /// of `get_ptr` will be undefined behavior if alignment is not guaranteed
    /// using some out-of-band mechanism. In general, the only functions which
    /// are safe to call with this pointer are those which are explicitly
    /// documented as being sound to use with an unaligned pointer, such as
    /// [`read_unaligned`].
    ///
    /// [`read_unaligned`]: core::ptr::read_unaligned
    #[inline(always)]
    pub const fn get_ptr(&self) -> *const T {
        ptr::addr_of!(self.0)
    }

    /// Gets an unaligned mutable raw pointer to the inner `T`.
    ///
    /// # Safety
    ///
    /// The returned raw pointer is not necessarily aligned to
    /// `align_of::<T>()`. Most functions which operate on raw pointers require
    /// those pointers to be aligned, so calling those functions with the result
    /// of `get_ptr` will be undefined behavior if alignment is not guaranteed
    /// using some out-of-band mechanism. In general, the only functions which
    /// are safe to call with this pointer are those which are explicitly
    /// documented as being sound to use with an unaligned pointer, such as
    /// [`read_unaligned`].
    ///
    /// [`read_unaligned`]: core::ptr::read_unaligned
    // TODO(https://github.com/rust-lang/rust/issues/57349): Make this `const`.
    #[inline(always)]
    pub fn get_mut_ptr(&mut self) -> *mut T {
        ptr::addr_of_mut!(self.0)
    }

    /// Sets the inner `T`, dropping the previous value.
    // TODO(https://github.com/rust-lang/rust/issues/57349): Make this `const`.
    #[inline(always)]
    pub fn set(&mut self, t: T) {
        *self = Unalign::new(t);
    }

    /// Updates the inner `T` by calling a function on it.
    ///
    /// If [`T: Unaligned`], then `Unalign<T>` implements [`DerefMut`], and that
    /// impl should be preferred over this method when performing updates, as it
    /// will usually be faster and more ergonomic.
    ///
    /// For large types, this method may be expensive, as it requires copying
    /// `2 * size_of::<T>()` bytes. \[1\]
    ///
    /// \[1\] Since the inner `T` may not be aligned, it would not be sound to
    /// invoke `f` on it directly. Instead, `update` moves it into a
    /// properly-aligned location in the local stack frame, calls `f` on it, and
    /// then moves it back to its original location in `self`.
    ///
    /// [`T: Unaligned`]: Unaligned
    #[inline]
    pub fn update<O, F: FnOnce(&mut T) -> O>(&mut self, f: F) -> O {
        // On drop, this moves `copy` out of itself and uses `ptr::write` to
        // overwrite `slf`.
        struct WriteBackOnDrop<T> {
            copy: ManuallyDrop<T>,
            slf: *mut Unalign<T>,
        }

        impl<T> Drop for WriteBackOnDrop<T> {
            fn drop(&mut self) {
                // SAFETY: We never use `copy` again as required by
                // `ManuallyDrop::take`.
                let copy = unsafe { ManuallyDrop::take(&mut self.copy) };
                // SAFETY: `slf` is the raw pointer value of `self`. We know it
                // is valid for writes and properly aligned because `self` is a
                // mutable reference, which guarantees both of these properties.
                unsafe { ptr::write(self.slf, Unalign::new(copy)) };
            }
        }

        // SAFETY: We know that `self` is valid for reads, properly aligned, and
        // points to an initialized `Unalign<T>` because it is a mutable
        // reference, which guarantees all of these properties.
        //
        // Since `T: !Copy`, it would be unsound in the general case to allow
        // both the original `Unalign<T>` and the copy to be used by safe code.
        // We guarantee that the copy is used to overwrite the original in the
        // `Drop::drop` impl of `WriteBackOnDrop`. So long as this `drop` is
        // called before any other safe code executes, soundness is upheld.
        // While this method can terminate in two ways (by returning normally or
        // by unwinding due to a panic in `f`), in both cases, `write_back` is
        // dropped - and its `drop` called - before any other safe code can
        // execute.
        let copy = unsafe { ptr::read(self) }.into_inner();
        let mut write_back = WriteBackOnDrop { copy: ManuallyDrop::new(copy), slf: self };

        let ret = f(&mut write_back.copy);

        drop(write_back);
        ret
    }
}

impl<T: Copy> Unalign<T> {
    /// Gets a copy of the inner `T`.
    // TODO(https://github.com/rust-lang/rust/issues/57349): Make this `const`.
    #[inline(always)]
    pub fn get(&self) -> T {
        let Unalign(val) = *self;
        val
    }
}

impl<T: Unaligned> Deref for Unalign<T> {
    type Target = T;

    #[inline(always)]
    fn deref(&self) -> &T {
        // SAFETY: `deref_unchecked`'s safety requirement is that `self` is
        // aligned to `align_of::<T>()`. `T: Unaligned` guarantees that
        // `align_of::<T>() == 1`, and all pointers are one-aligned because all
        // addresses are divisible by 1.
        unsafe { self.deref_unchecked() }
    }
}

impl<T: Unaligned> DerefMut for Unalign<T> {
    #[inline(always)]
    fn deref_mut(&mut self) -> &mut T {
        // SAFETY: `deref_mut_unchecked`'s safety requirement is that `self` is
        // aligned to `align_of::<T>()`. `T: Unaligned` guarantees that
        // `align_of::<T>() == 1`, and all pointers are one-aligned because all
        // addresses are divisible by 1.
        unsafe { self.deref_mut_unchecked() }
    }
}

impl<T: Unaligned + PartialOrd> PartialOrd<Unalign<T>> for Unalign<T> {
    #[inline(always)]
    fn partial_cmp(&self, other: &Unalign<T>) -> Option<Ordering> {
        PartialOrd::partial_cmp(self.deref(), other.deref())
    }
}

impl<T: Unaligned + Ord> Ord for Unalign<T> {
    #[inline(always)]
    fn cmp(&self, other: &Unalign<T>) -> Ordering {
        Ord::cmp(self.deref(), other.deref())
    }
}

impl<T: Unaligned + PartialEq> PartialEq<Unalign<T>> for Unalign<T> {
    #[inline(always)]
    fn eq(&self, other: &Unalign<T>) -> bool {
        PartialEq::eq(self.deref(), other.deref())
    }
}

impl<T: Unaligned + Eq> Eq for Unalign<T> {}

impl<T: Unaligned + Hash> Hash for Unalign<T> {
    #[inline(always)]
    fn hash<H>(&self, state: &mut H)
    where
        H: Hasher,
    {
        self.deref().hash(state);
    }
}

impl<T: Unaligned + Debug> Debug for Unalign<T> {
    #[inline(always)]
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        Debug::fmt(self.deref(), f)
    }
}

impl<T: Unaligned + Display> Display for Unalign<T> {
    #[inline(always)]
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        Display::fmt(self.deref(), f)
    }
}

#[cfg(test)]
mod tests {
    use core::panic::AssertUnwindSafe;

    use super::*;
    use crate::util::testutil::*;

    /// A `T` which is guaranteed not to satisfy `align_of::<A>()`.
    ///
    /// It must be the case that `align_of::<T>() < align_of::<A>()` in order
    /// fot this type to work properly.
    #[repr(C)]
    struct ForceUnalign<T, A> {
        // The outer struct is aligned to `A`, and, thanks to `repr(C)`, `t` is
        // placed at the minimum offset that guarantees its alignment. If
        // `align_of::<T>() < align_of::<A>()`, then that offset will be
        // guaranteed *not* to satisfy `align_of::<A>()`.
        _u: u8,
        t: T,
        _a: [A; 0],
    }

    impl<T, A> ForceUnalign<T, A> {
        const fn new(t: T) -> ForceUnalign<T, A> {
            ForceUnalign { _u: 0, t, _a: [] }
        }
    }

    #[test]
    fn test_unalign() {
        // Test methods that don't depend on alignment.
        let mut u = Unalign::new(AU64(123));
        assert_eq!(u.get(), AU64(123));
        assert_eq!(u.into_inner(), AU64(123));
        assert_eq!(u.get_ptr(), <*const _>::cast::<AU64>(&u));
        assert_eq!(u.get_mut_ptr(), <*mut _>::cast::<AU64>(&mut u));
        u.set(AU64(321));
        assert_eq!(u.get(), AU64(321));

        // Test methods that depend on alignment (when alignment is satisfied).
        let mut u: Align<_, AU64> = Align::new(Unalign::new(AU64(123)));
        assert_eq!(u.t.try_deref(), Some(&AU64(123)));
        assert_eq!(u.t.try_deref_mut(), Some(&mut AU64(123)));
        // SAFETY: The `Align<_, AU64>` guarantees proper alignment.
        assert_eq!(unsafe { u.t.deref_unchecked() }, &AU64(123));
        // SAFETY: The `Align<_, AU64>` guarantees proper alignment.
        assert_eq!(unsafe { u.t.deref_mut_unchecked() }, &mut AU64(123));
        *u.t.try_deref_mut().unwrap() = AU64(321);
        assert_eq!(u.t.get(), AU64(321));

        // Test methods that depend on alignment (when alignment is not
        // satisfied).
        let mut u: ForceUnalign<_, AU64> = ForceUnalign::new(Unalign::new(AU64(123)));
        assert_eq!(u.t.try_deref(), None);
        assert_eq!(u.t.try_deref_mut(), None);

        // Test methods that depend on `T: Unaligned`.
        let mut u = Unalign::new(123u8);
        assert_eq!(u.try_deref(), Some(&123));
        assert_eq!(u.try_deref_mut(), Some(&mut 123));
        assert_eq!(u.deref(), &123);
        assert_eq!(u.deref_mut(), &mut 123);
        *u = 21;
        assert_eq!(u.get(), 21);

        // Test that some `Unalign` functions and methods are `const`.
        const _UNALIGN: Unalign<u64> = Unalign::new(0);
        const _UNALIGN_PTR: *const u64 = _UNALIGN.get_ptr();
        const _U64: u64 = _UNALIGN.into_inner();
        // Make sure all code is considered "used".
        //
        // TODO(https://github.com/rust-lang/rust/issues/104084): Remove this
        // attribute.
        #[allow(dead_code)]
        const _: () = {
            let x: Align<_, AU64> = Align::new(Unalign::new(AU64(123)));
            // Make sure that `deref_unchecked` is `const`.
            //
            // SAFETY: The `Align<_, AU64>` guarantees proper alignment.
            let au64 = unsafe { x.t.deref_unchecked() };
            match au64 {
                AU64(123) => {}
                _ => unreachable!(),
            }
        };
    }

    #[test]
    fn test_unalign_update() {
        let mut u = Unalign::new(AU64(123));
        u.update(|a| a.0 += 1);
        assert_eq!(u.get(), AU64(124));

        // Test that, even if the callback panics, the original is still
        // correctly overwritten. Use a `Box` so that Miri is more likely to
        // catch any unsoundness (which would likely result in two `Box`es for
        // the same heap object, which is the sort of thing that Miri would
        // probably catch).
        let mut u = Unalign::new(Box::new(AU64(123)));
        let res = std::panic::catch_unwind(AssertUnwindSafe(|| {
            u.update(|a| {
                a.0 += 1;
                panic!();
            })
        }));
        assert!(res.is_err());
        assert_eq!(u.into_inner(), Box::new(AU64(124)));
    }
}
