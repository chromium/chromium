// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::marker::PhantomData;
use std::ops::{Deref, DerefMut};
use std::ptr::NonNull;
use std::sync::atomic::{AtomicUsize, Ordering};

use super::AtomicRefCell;

const MUT_BIT: usize = !(usize::MAX >> 1);

impl<T: ?Sized> AtomicRefCell<T> {
    #[inline]
    fn ptr(&self) -> NonNull<T> {
        // SAFETY: Pointer returned by `UnsafeCell::get` is non-null.
        unsafe { NonNull::new_unchecked(self.data.get()) }
    }
}

/// Indicator that a shared reference to the data is successfully acquired.
// Safety invariant: `counter > 0`, and `MUT_BIT` is not set (`counter & MUT_BIT == 0`). The
// invariants imply that no `BorrowTokenMut` exists that points to the same counter.
struct BorrowToken<'a>(&'a AtomicUsize);

impl<'a> BorrowToken<'a> {
    /// Ensures that there's no mutable borrow of the data, and increments the reference counter.
    ///
    /// It is guaranteed that there's no instance of `BorrowTokenMut` that points to the same
    /// counter if this method returned `Some`.
    #[inline]
    fn borrow(counter: &'a AtomicUsize) -> Option<Self> {
        let mut prev_counter = counter.load(Ordering::Relaxed);
        let success = loop {
            if prev_counter & MUT_BIT != 0 {
                // Mutable borrow exists.
                break false;
            }

            // Ensure that the safety invariant holds after incrementing the counter.
            let next_counter = prev_counter + 1;
            if next_counter & MUT_BIT != 0 {
                // Counter overflowed; treat as failure.
                break false;
            }

            // Use compare-exchange to ensure that the counter didn't change since the last time.
            // Acquire ordering synchronizes with Release used in `BorrowToken{,Mut}::drop`.
            // Ordering of other accesses doesn't matter, because the borrow happens only when
            // compare-exchange succeeds.
            match counter.compare_exchange_weak(
                prev_counter,
                next_counter,
                Ordering::Acquire,
                Ordering::Relaxed,
            ) {
                Ok(_) => break true,
                Err(counter) => {
                    // Compare-exchange failed; retry.
                    prev_counter = counter;
                }
            }
        };

        // The safety invariants hold if `success` is true.
        success.then(|| Self(counter))
    }
}

impl Clone for BorrowToken<'_> {
    #[inline]
    fn clone(&self) -> Self {
        Self::borrow(self.0).unwrap()
    }
}

impl Drop for BorrowToken<'_> {
    #[inline]
    fn drop(&mut self) {
        // Decrement the reference counter.
        self.0.fetch_sub(1, Ordering::Release);
    }
}

/// Indicator that a mutable reference to the data is successfully acquired.
// Safety invariant: no other `BorrowTokenMut` that points to the same counter exists, and the
// counter equals `MUT_BIT`. The invariants imply that no `BorrowToken` exists that points to the
// same counter.
struct BorrowTokenMut<'a>(&'a AtomicUsize);

impl<'a> BorrowTokenMut<'a> {
    /// Ensures that there's no active borrow of the data, and marks the reference counter as
    /// mutably borrowed.
    ///
    /// It is guaranteed that there's no instance of `BorrowToken` or `BorrowTokenMut` that points
    /// to the same counter if this method returned `Some`.
    #[inline]
    fn borrow_mut(counter: &'a AtomicUsize) -> Option<Self> {
        // Use compare-exchange to ensure that there's no other reference to the data.
        // Acquire ordering synchronizes with Release used in `BorrowToken{,Mut}::drop`.
        let success = counter
            .compare_exchange(0, MUT_BIT, Ordering::Acquire, Ordering::Relaxed)
            .is_ok();
        // The safety invariants hold if `success` is true, because:
        // - no other `BorrowTokenMut` exists, as the counter was originally 0, and
        // - the counter now equals `MUT_BIT`.
        success.then(|| Self(counter))
    }
}

impl Drop for BorrowTokenMut<'_> {
    #[inline]
    fn drop(&mut self) {
        // Unconditionally set the counter to zero since this is the only reference to the data.
        self.0.store(0, Ordering::Release);
    }
}

// Safety invariant: `ptr` is valid for reads, and while `AtomicRef` is live, `ptr` can be used to
// create a shared reference (there is no live mutable reference). This is ensured by `ptr` only
// borrowing data protected by `AtomicRefCell` that `token` was obtained from.
pub struct AtomicRef<'a, T: ?Sized> {
    ptr: NonNull<T>,
    // Ensures that no other mutable reference exists while this `AtomicRef` is live.
    token: BorrowToken<'a>,
}

// SAFETY: `AtomicRef` acts like a shared reference (see `deref`).
unsafe impl<'a, T: ?Sized> Send for AtomicRef<'a, T> where for<'r> &'r T: Send {}

// SAFETY: `AtomicRef` acts like a shared reference (see `deref`).
unsafe impl<'a, T: ?Sized> Sync for AtomicRef<'a, T> where for<'r> &'r T: Sync {}

impl<'a, T: ?Sized> AtomicRef<'a, T> {
    #[inline]
    pub(super) fn new(cell: &'a AtomicRefCell<T>) -> Option<Self> {
        let token = BorrowToken::borrow(&cell.counter)?;
        // Safety note: `ptr` and `token` are obtained from the same `AtomicRefCell`.
        Some(Self {
            ptr: cell.ptr(),
            token,
        })
    }

    #[inline]
    pub fn map<U: ?Sized>(orig: Self, f: impl FnOnce(&T) -> &U) -> AtomicRef<'a, U> {
        // Safety note: `f(&*orig)` is derived from `orig.ptr` (via `Deref` impl), therefore `ptr`
        // is derived from the same `AtomicRefCell` that `orig.token` is obtained from.
        AtomicRef {
            ptr: NonNull::from_ref(f(&*orig)),
            token: orig.token,
        }
    }

    #[expect(clippy::should_implement_trait)]
    #[inline]
    pub fn clone(orig: &Self) -> Self {
        // Safety note: The invariants hold trivially, from the invariants of `orig`.
        Self {
            ptr: orig.ptr,
            token: orig.token.clone(),
        }
    }
}

impl<T: ?Sized> Deref for AtomicRef<'_, T> {
    type Target = T;

    #[inline]
    fn deref(&self) -> &T {
        // SAFETY: The requirements of `ptr.as_ref()` is part of the safety invariants of `self`.
        unsafe { self.ptr.as_ref() }
    }
}

impl<T: std::fmt::Debug> std::fmt::Debug for AtomicRef<'_, T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_tuple("AtomicRef").field(&**self).finish()
    }
}

// Safety invariant: `ptr` is valid for reads and writes, and while `AtomicRefMut` is live, `ptr`
// can be used to create a mutable reference (there is no other live reference). This is ensured by
// `ptr` only borrowing data protected by `AtomicRefCell` that `token` was obtained from.
pub struct AtomicRefMut<'a, T: ?Sized> {
    ptr: NonNull<T>,
    // Ensures that no other reference exists while this `AtomicRefMut` is live.
    token: BorrowTokenMut<'a>,
    // Marker to make `AtomicRefMut` invariant over `T`.
    _phantom: PhantomData<&'a mut T>,
}

// SAFETY: `AtomicRefMut` acts like a mutable reference (see `deref_mut`).
unsafe impl<'a, T: ?Sized> Send for AtomicRefMut<'a, T> where for<'r> &'r mut T: Send {}

// SAFETY: `AtomicRefMut` acts like a mutable reference (see `deref_mut`).
unsafe impl<'a, T: ?Sized> Sync for AtomicRefMut<'a, T> where for<'r> &'r mut T: Sync {}

impl<'a, T: ?Sized> AtomicRefMut<'a, T> {
    #[inline]
    pub(super) fn new(cell: &'a AtomicRefCell<T>) -> Option<Self> {
        let token = BorrowTokenMut::borrow_mut(&cell.counter)?;
        // Safety note: `ptr` and `token` are obtained from the same `AtomicRefCell`.
        Some(Self {
            ptr: cell.ptr(),
            token,
            _phantom: PhantomData,
        })
    }

    #[inline]
    pub fn map<U: ?Sized>(mut orig: Self, f: impl FnOnce(&mut T) -> &mut U) -> AtomicRefMut<'a, U> {
        // Safety note: `f(&mut *orig)` is derived from `orig.ptr` (via `DerefMut` impl), therefore
        // `ptr` is derived from the same `AtomicRefCell` that `orig.token` is obtained from.
        AtomicRefMut {
            ptr: NonNull::from_mut(f(&mut *orig)),
            token: orig.token,
            _phantom: PhantomData,
        }
    }
}

impl<T: ?Sized> Deref for AtomicRefMut<'_, T> {
    type Target = T;

    #[inline]
    fn deref(&self) -> &T {
        // SAFETY: The requirements of `ptr.as_ref()` is part of the safety invariants of `self`.
        unsafe { self.ptr.as_ref() }
    }
}

impl<T: ?Sized> DerefMut for AtomicRefMut<'_, T> {
    #[inline]
    fn deref_mut(&mut self) -> &mut T {
        // SAFETY: The requirements of `ptr.as_mut()` is part of the safety invariants of `self`.
        unsafe { self.ptr.as_mut() }
    }
}

impl<T: std::fmt::Debug> std::fmt::Debug for AtomicRefMut<'_, T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_tuple("AtomicRefMut").field(&**self).finish()
    }
}
