// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(unsafe_code)]

use std::cell::UnsafeCell;
use std::sync::atomic::AtomicUsize;

mod internal;

pub use internal::{AtomicRef, AtomicRefMut};

pub struct AtomicRefCell<T: ?Sized> {
    counter: AtomicUsize,
    data: UnsafeCell<T>,
}

impl<T> AtomicRefCell<T> {
    #[inline]
    pub const fn new(value: T) -> Self {
        Self {
            counter: AtomicUsize::new(0),
            data: UnsafeCell::new(value),
        }
    }
}

impl<T: ?Sized> AtomicRefCell<T> {
    #[inline]
    pub fn borrow(&self) -> AtomicRef<'_, T> {
        AtomicRef::new(self).unwrap()
    }

    #[inline]
    pub fn try_borrow(&self) -> Option<AtomicRef<'_, T>> {
        AtomicRef::new(self)
    }

    #[inline]
    pub fn borrow_mut(&self) -> AtomicRefMut<'_, T> {
        AtomicRefMut::new(self).unwrap()
    }

    #[inline]
    pub fn try_borrow_mut(&self) -> Option<AtomicRefMut<'_, T>> {
        AtomicRefMut::new(self)
    }
}

// SAFETY: Accesses to the inner data are synchronized by the atomic reference counter.
unsafe impl<T: ?Sized + Send> Send for AtomicRefCell<T> {}

// SAFETY: Accesses to the inner data are synchronized by the atomic reference counter. Additional
// `Send` bound is needed because `AtomicRefCell` provides mutable access behind a shared reference.
unsafe impl<T: ?Sized + Send + Sync> Sync for AtomicRefCell<T> {}

impl<T: std::fmt::Debug> std::fmt::Debug for AtomicRefCell<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let val = self.try_borrow();
        if let Some(val) = val {
            f.debug_tuple("AtomicRefCell").field(&*val).finish()
        } else {
            f.debug_tuple("AtomicRefCell")
                .field(&format_args!("[borrowed]"))
                .finish()
        }
    }
}
