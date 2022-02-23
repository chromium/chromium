// Original work Copyright (c) 2014 The Rust Project Developers
// Modified work Copyright (c) 2016-2020 Nikita Pekin and the lazycell contributors
// See the README.md file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(not(test), no_std)]

#![deny(missing_docs)]
#![cfg_attr(feature = "nightly", feature(plugin))]
#![cfg_attr(feature = "clippy", plugin(clippy))]

//! This crate provides a `LazyCell` struct which acts as a lazily filled
//! `Cell`.
//!
//! With a `RefCell`, the inner contents cannot be borrowed for the lifetime of
//! the entire object, but only of the borrows returned. A `LazyCell` is a
//! variation on `RefCell` which allows borrows to be tied to the lifetime of
//! the outer object.
//!
//! # Example
//!
//! The following example shows a quick example of the basic functionality of
//! `LazyCell`.
//!
//! ```
//! use lazycell::LazyCell;
//!
//! let lazycell = LazyCell::new();
//!
//! assert_eq!(lazycell.borrow(), None);
//! assert!(!lazycell.filled());
//! lazycell.fill(1).ok();
//! assert!(lazycell.filled());
//! assert_eq!(lazycell.borrow(), Some(&1));
//! assert_eq!(lazycell.into_inner(), Some(1));
//! ```
//!
//! `AtomicLazyCell` is a variant that uses an atomic variable to manage
//! coordination in a thread-safe fashion. The limitation of an `AtomicLazyCell`
//! is that after it is initialized, it can't be modified.


#[cfg(not(test))]
#[macro_use]
extern crate core as std;
#[cfg(feature = "serde")]
extern crate serde;

#[cfg(feature = "serde")]
mod serde_impl;

use std::cell::UnsafeCell;
use std::mem;
use std::sync::atomic::{AtomicUsize, Ordering};

/// A lazily filled `Cell`, with mutable contents.
///
/// A `LazyCell` is completely frozen once filled, **unless** you have `&mut`
/// access to it, in which case `LazyCell::borrow_mut` may be used to mutate the
/// contents.
#[derive(Debug)]
pub struct LazyCell<T> {
    inner: UnsafeCell<Option<T>>,
}

impl<T> LazyCell<T> {
    /// Creates a new, empty, `LazyCell`.
    pub fn new() -> LazyCell<T> {
        LazyCell { inner: UnsafeCell::new(None) }
    }

    /// Put a value into this cell.
    ///
    /// This function will return `Err(value)` if the cell is already full.
    pub fn fill(&self, value: T) -> Result<(), T> {
        let slot = unsafe { &*self.inner.get() };
        if slot.is_some() {
            return Err(value);
        }
        let slot = unsafe { &mut *self.inner.get() };
        *slot = Some(value);

        Ok(())
    }

    /// Put a value into this cell.
    ///
    /// Note that this function is infallible but requires `&mut self`. By
    /// requiring `&mut self` we're guaranteed that no active borrows to this
    /// cell can exist so we can always fill in the value. This may not always
    /// be usable, however, as `&mut self` may not be possible to borrow.
    ///
    /// # Return value
    ///
    /// This function returns the previous value, if any.
    pub fn replace(&mut self, value: T) -> Option<T> {
        mem::replace(unsafe { &mut *self.inner.get() }, Some(value))
    }

    /// Test whether this cell has been previously filled.
    pub fn filled(&self) -> bool {
        self.borrow().is_some()
    }

    /// Borrows the contents of this lazy cell for the duration of the cell
    /// itself.
    ///
    /// This function will return `Some` if the cell has been previously
    /// initialized, and `None` if it has not yet been initialized.
    pub fn borrow(&self) -> Option<&T> {
        unsafe { &*self.inner.get() }.as_ref()
    }

    /// Borrows the contents of this lazy cell mutably for the duration of the cell
    /// itself.
    ///
    /// This function will return `Some` if the cell has been previously
    /// initialized, and `None` if it has not yet been initialized.
    pub fn borrow_mut(&mut self) -> Option<&mut T> {
        unsafe { &mut *self.inner.get() }.as_mut()
    }

    /// Borrows the contents of this lazy cell for the duration of the cell
    /// itself.
    ///
    /// If the cell has not yet been filled, the cell is first filled using the
    /// function provided.
    ///
    /// # Panics
    ///
    /// Panics if the cell becomes filled as a side effect of `f`.
    pub fn borrow_with<F: FnOnce() -> T>(&self, f: F) -> &T {
        if let Some(value) = self.borrow() {
            return value;
        }
        let value = f();
        if self.fill(value).is_err() {
            panic!("borrow_with: cell was filled by closure")
        }
        self.borrow().unwrap()
    }

    /// Borrows the contents of this `LazyCell` mutably for the duration of the
    /// cell itself.
    ///
    /// If the cell has not yet been filled, the cell is first filled using the
    /// function provided.
    ///
    /// # Panics
    ///
    /// Panics if the cell becomes filled as a side effect of `f`.
    pub fn borrow_mut_with<F: FnOnce() -> T>(&mut self, f: F) -> &mut T {
        if !self.filled() {
            let value = f();
            if self.fill(value).is_err() {
                panic!("borrow_mut_with: cell was filled by closure")
            }
        }

        self.borrow_mut().unwrap()
    }

    /// Same as `borrow_with`, but allows the initializing function to fail.
    ///
    /// # Panics
    ///
    /// Panics if the cell becomes filled as a side effect of `f`.
    pub fn try_borrow_with<E, F>(&self, f: F) -> Result<&T, E>
        where F: FnOnce() -> Result<T, E>
    {
        if let Some(value) = self.borrow() {
            return Ok(value);
        }
        let value = f()?;
        if self.fill(value).is_err() {
            panic!("try_borrow_with: cell was filled by closure")
        }
        Ok(self.borrow().unwrap())
    }

    /// Same as `borrow_mut_with`, but allows the initializing function to fail.
    ///
    /// # Panics
    ///
    /// Panics if the cell becomes filled as a side effect of `f`.
    pub fn try_borrow_mut_with<E, F>(&mut self, f: F) -> Result<&mut T, E>
        where F: FnOnce() -> Result<T, E>
    {
        if self.filled() {
            return Ok(self.borrow_mut().unwrap());
        }
        let value = f()?;
        if self.fill(value).is_err() {
            panic!("try_borrow_mut_with: cell was filled by closure")
        }
        Ok(self.borrow_mut().unwrap())
    }

    /// Consumes this `LazyCell`, returning the underlying value.
    pub fn into_inner(self) -> Option<T> {
        // Rust 1.25 changed UnsafeCell::into_inner() from unsafe to safe
        // function. This unsafe can be removed when supporting Rust older than
        // 1.25 is not needed.
        #[allow(unused_unsafe)]
        unsafe { self.inner.into_inner() }
    }
}

impl<T: Copy> LazyCell<T> {
    /// Returns a copy of the contents of the lazy cell.
    ///
    /// This function will return `Some` if the cell has been previously initialized,
    /// and `None` if it has not yet been initialized.
    pub fn get(&self) -> Option<T> {
        unsafe { *self.inner.get() }
    }
}

impl<T> Default for LazyCell<T> {
    fn default() -> Self {
        Self::new()
    }
}

impl <T: Clone> Clone for LazyCell<T> {
    /// Create a clone of this `LazyCell`
    ///
    /// If self has not been initialized, returns an uninitialized `LazyCell`
    /// otherwise returns a `LazyCell` already initialized with a clone of the
    /// contents of self.
    fn clone(&self) -> LazyCell<T> {
        LazyCell { inner: UnsafeCell::new(self.borrow().map(Clone::clone) ) }
    }
}

// Tracks the AtomicLazyCell inner state
const NONE: usize = 0;
const LOCK: usize = 1;
const SOME: usize = 2;

/// A lazily filled and thread-safe `Cell`, with frozen contents.
#[derive(Debug)]
pub struct AtomicLazyCell<T> {
    inner: UnsafeCell<Option<T>>,
    state: AtomicUsize,
}

impl<T> AtomicLazyCell<T> {
    /// An empty `AtomicLazyCell`.
    pub const NONE: Self = Self {
        inner: UnsafeCell::new(None),
        state: AtomicUsize::new(NONE),
    };

    /// Creates a new, empty, `AtomicLazyCell`.
    pub fn new() -> AtomicLazyCell<T> {
        Self::NONE
    }

    /// Put a value into this cell.
    ///
    /// This function will return `Err(value)` if the cell is already full.
    pub fn fill(&self, t: T) -> Result<(), T> {
        if NONE != self.state.compare_and_swap(NONE, LOCK, Ordering::Acquire) {
            return Err(t);
        }

        unsafe { *self.inner.get() = Some(t) };

        if LOCK != self.state.compare_and_swap(LOCK, SOME, Ordering::Release) {
            panic!("unable to release lock");
        }

        Ok(())
    }

    /// Put a value into this cell.
    ///
    /// Note that this function is infallible but requires `&mut self`. By
    /// requiring `&mut self` we're guaranteed that no active borrows to this
    /// cell can exist so we can always fill in the value. This may not always
    /// be usable, however, as `&mut self` may not be possible to borrow.
    ///
    /// # Return value
    ///
    /// This function returns the previous value, if any.
    pub fn replace(&mut self, value: T) -> Option<T> {
        match mem::replace(self.state.get_mut(), SOME) {
            NONE | SOME => {}
            _ => panic!("cell in inconsistent state"),
        }
        mem::replace(unsafe { &mut *self.inner.get() }, Some(value))
    }

    /// Test whether this cell has been previously filled.
    pub fn filled(&self) -> bool {
        self.state.load(Ordering::Acquire) == SOME
    }

    /// Borrows the contents of this lazy cell for the duration of the cell
    /// itself.
    ///
    /// This function will return `Some` if the cell has been previously
    /// initialized, and `None` if it has not yet been initialized.
    pub fn borrow(&self) -> Option<&T> {
        match self.state.load(Ordering::Acquire) {
            SOME => unsafe { &*self.inner.get() }.as_ref(),
            _ => None,
        }
    }

    /// Consumes this `LazyCell`, returning the underlying value.
    pub fn into_inner(self) -> Option<T> {
        // Rust 1.25 changed UnsafeCell::into_inner() from unsafe to safe
        // function. This unsafe can be removed when supporting Rust older than
        // 1.25 is not needed.
        #[allow(unused_unsafe)]
        unsafe { self.inner.into_inner() }
    }
}

impl<T: Copy> AtomicLazyCell<T> {
    /// Returns a copy of the contents of the lazy cell.
    ///
    /// This function will return `Some` if the cell has been previously initialized,
    /// and `None` if it has not yet been initialized.
    pub fn get(&self) -> Option<T> {
        match self.state.load(Ordering::Acquire) {
            SOME => unsafe { *self.inner.get() },
            _ => None,
        }
    }
}

impl<T> Default for AtomicLazyCell<T> {
    fn default() -> Self {
        Self::new()
    }
}

impl<T: Clone> Clone for AtomicLazyCell<T> {
    /// Create a clone of this `AtomicLazyCell`
    ///
    /// If self has not been initialized, returns an uninitialized `AtomicLazyCell`
    /// otherwise returns an `AtomicLazyCell` already initialized with a clone of the
    /// contents of self.
    fn clone(&self) -> AtomicLazyCell<T> {
        self.borrow().map_or(
            Self::NONE,
            |v| AtomicLazyCell {
                inner: UnsafeCell::new(Some(v.clone())),
                state: AtomicUsize::new(SOME),
            }
        )
    }
}

unsafe impl<T: Sync + Send> Sync for AtomicLazyCell<T> {}

unsafe impl<T: Send> Send for AtomicLazyCell<T> {}

#[cfg(test)]
mod tests {
    use super::{AtomicLazyCell, LazyCell};

    #[test]
    fn test_borrow_from_empty() {
        let lazycell: LazyCell<usize> = LazyCell::new();

        let value = lazycell.borrow();
        assert_eq!(value, None);

        let value = lazycell.get();
        assert_eq!(value, None);
    }

    #[test]
    fn test_fill_and_borrow() {
        let lazycell = LazyCell::new();

        assert!(!lazycell.filled());
        lazycell.fill(1).unwrap();
        assert!(lazycell.filled());

        let value = lazycell.borrow();
        assert_eq!(value, Some(&1));

        let value = lazycell.get();
        assert_eq!(value, Some(1));
    }

    #[test]
    fn test_borrow_mut() {
        let mut lazycell = LazyCell::new();
        assert!(lazycell.borrow_mut().is_none());

        lazycell.fill(1).unwrap();
        assert_eq!(lazycell.borrow_mut(), Some(&mut 1));

        *lazycell.borrow_mut().unwrap() = 2;
        assert_eq!(lazycell.borrow_mut(), Some(&mut 2));

        // official way to reset the cell
        lazycell = LazyCell::new();
        assert!(lazycell.borrow_mut().is_none());
    }

    #[test]
    fn test_already_filled_error() {
        let lazycell = LazyCell::new();

        lazycell.fill(1).unwrap();
        assert_eq!(lazycell.fill(1), Err(1));
    }

    #[test]
    fn test_borrow_with() {
        let lazycell = LazyCell::new();

        let value = lazycell.borrow_with(|| 1);
        assert_eq!(&1, value);
    }

    #[test]
    fn test_borrow_with_already_filled() {
        let lazycell = LazyCell::new();
        lazycell.fill(1).unwrap();

        let value = lazycell.borrow_with(|| 1);
        assert_eq!(&1, value);
    }

    #[test]
    fn test_borrow_with_not_called_when_filled() {
        let lazycell = LazyCell::new();

        lazycell.fill(1).unwrap();

        let value = lazycell.borrow_with(|| 2);
        assert_eq!(&1, value);
    }

    #[test]
    #[should_panic]
    fn test_borrow_with_sound_with_reentrancy() {
        // Kudos to dbaupp for discovering this issue
        // https://www.reddit.com/r/rust/comments/5vs9rt/lazycell_a_rust_library_providing_a_lazilyfilled/de527xm/
        let lazycell: LazyCell<Box<i32>> = LazyCell::new();

        let mut reference: Option<&i32> = None;

        lazycell.borrow_with(|| {
            let _ = lazycell.fill(Box::new(1));
            reference = lazycell.borrow().map(|r| &**r);
            Box::new(2)
        });
    }

    #[test]
    fn test_borrow_mut_with() {
        let mut lazycell = LazyCell::new();

        {
            let value = lazycell.borrow_mut_with(|| 1);
            assert_eq!(&mut 1, value);
            *value = 2;
        }
        assert_eq!(&2, lazycell.borrow().unwrap());
    }

    #[test]
    fn test_borrow_mut_with_already_filled() {
        let mut lazycell = LazyCell::new();
        lazycell.fill(1).unwrap();

        let value = lazycell.borrow_mut_with(|| 1);
        assert_eq!(&1, value);
    }

    #[test]
    fn test_borrow_mut_with_not_called_when_filled() {
        let mut lazycell = LazyCell::new();

        lazycell.fill(1).unwrap();

        let value = lazycell.borrow_mut_with(|| 2);
        assert_eq!(&1, value);
    }

    #[test]
    fn test_try_borrow_with_ok() {
        let lazycell = LazyCell::new();
        let result = lazycell.try_borrow_with::<(), _>(|| Ok(1));
        assert_eq!(result, Ok(&1));
    }

    #[test]
    fn test_try_borrow_with_err() {
        let lazycell = LazyCell::<()>::new();
        let result = lazycell.try_borrow_with(|| Err(1));
        assert_eq!(result, Err(1));
    }

    #[test]
    fn test_try_borrow_with_already_filled() {
        let lazycell = LazyCell::new();
        lazycell.fill(1).unwrap();
        let result = lazycell.try_borrow_with::<(), _>(|| unreachable!());
        assert_eq!(result, Ok(&1));
    }

    #[test]
    #[should_panic]
    fn test_try_borrow_with_sound_with_reentrancy() {
        let lazycell: LazyCell<Box<i32>> = LazyCell::new();

        let mut reference: Option<&i32> = None;

        let _ = lazycell.try_borrow_with::<(), _>(|| {
            let _ = lazycell.fill(Box::new(1));
            reference = lazycell.borrow().map(|r| &**r);
            Ok(Box::new(2))
        });
    }

    #[test]
    fn test_try_borrow_mut_with_ok() {
        let mut lazycell = LazyCell::new();
        {
            let result = lazycell.try_borrow_mut_with::<(), _>(|| Ok(1));
            assert_eq!(result, Ok(&mut 1));
            *result.unwrap() = 2;
        }
        assert_eq!(&mut 2, lazycell.borrow().unwrap());
    }

    #[test]
    fn test_try_borrow_mut_with_err() {
        let mut lazycell = LazyCell::<()>::new();
        let result = lazycell.try_borrow_mut_with(|| Err(1));
        assert_eq!(result, Err(1));
    }

    #[test]
    fn test_try_borrow_mut_with_already_filled() {
        let mut lazycell = LazyCell::new();
        lazycell.fill(1).unwrap();
        let result = lazycell.try_borrow_mut_with::<(), _>(|| unreachable!());
        assert_eq!(result, Ok(&mut 1));
    }

    #[test]
    fn test_into_inner() {
        let lazycell = LazyCell::new();

        lazycell.fill(1).unwrap();
        let value = lazycell.into_inner();
        assert_eq!(value, Some(1));
    }

    #[test]
    fn test_atomic_borrow_from_empty() {
        let lazycell: AtomicLazyCell<usize> = AtomicLazyCell::new();

        let value = lazycell.borrow();
        assert_eq!(value, None);

        let value = lazycell.get();
        assert_eq!(value, None);
    }

    #[test]
    fn test_atomic_fill_and_borrow() {
        let lazycell = AtomicLazyCell::new();

        assert!(!lazycell.filled());
        lazycell.fill(1).unwrap();
        assert!(lazycell.filled());

        let value = lazycell.borrow();
        assert_eq!(value, Some(&1));

        let value = lazycell.get();
        assert_eq!(value, Some(1));
    }

    #[test]
    fn test_atomic_already_filled_panic() {
        let lazycell = AtomicLazyCell::new();

        lazycell.fill(1).unwrap();
        assert_eq!(1, lazycell.fill(1).unwrap_err());
    }

    #[test]
    fn test_atomic_into_inner() {
        let lazycell = AtomicLazyCell::new();

        lazycell.fill(1).unwrap();
        let value = lazycell.into_inner();
        assert_eq!(value, Some(1));
    }

    #[test]
    fn normal_replace() {
        let mut cell = LazyCell::new();
        assert_eq!(cell.fill(1), Ok(()));
        assert_eq!(cell.replace(2), Some(1));
        assert_eq!(cell.replace(3), Some(2));
        assert_eq!(cell.borrow(), Some(&3));

        let mut cell = LazyCell::new();
        assert_eq!(cell.replace(2), None);
    }

    #[test]
    fn atomic_replace() {
        let mut cell = AtomicLazyCell::new();
        assert_eq!(cell.fill(1), Ok(()));
        assert_eq!(cell.replace(2), Some(1));
        assert_eq!(cell.replace(3), Some(2));
        assert_eq!(cell.borrow(), Some(&3));
    }

    #[test]
    fn clone() {
        let mut cell = LazyCell::new();
        let clone1 = cell.clone();
        assert_eq!(clone1.borrow(), None);
        assert_eq!(cell.fill(1), Ok(()));
        let mut clone2 = cell.clone();
        assert_eq!(clone1.borrow(), None);
        assert_eq!(clone2.borrow(), Some(&1));
        assert_eq!(cell.replace(2), Some(1));
        assert_eq!(clone1.borrow(), None);
        assert_eq!(clone2.borrow(), Some(&1));
        assert_eq!(clone1.fill(3), Ok(()));
        assert_eq!(clone2.replace(4), Some(1));
        assert_eq!(clone1.borrow(), Some(&3));
        assert_eq!(clone2.borrow(), Some(&4));
        assert_eq!(cell.borrow(), Some(&2));
    }

    #[test]
    fn clone_atomic() {
        let mut cell = AtomicLazyCell::new();
        let clone1 = cell.clone();
        assert_eq!(clone1.borrow(), None);
        assert_eq!(cell.fill(1), Ok(()));
        let mut clone2 = cell.clone();
        assert_eq!(clone1.borrow(), None);
        assert_eq!(clone2.borrow(), Some(&1));
        assert_eq!(cell.replace(2), Some(1));
        assert_eq!(clone1.borrow(), None);
        assert_eq!(clone2.borrow(), Some(&1));
        assert_eq!(clone1.fill(3), Ok(()));
        assert_eq!(clone2.replace(4), Some(1));
        assert_eq!(clone1.borrow(), Some(&3));
        assert_eq!(clone2.borrow(), Some(&4));
        assert_eq!(cell.borrow(), Some(&2));
    }

    #[test]
    fn default() {
        #[derive(Default)]
        struct Defaultable;
        struct NonDefaultable;

        let _: LazyCell<Defaultable> = LazyCell::default();
        let _: LazyCell<NonDefaultable> = LazyCell::default();

        let _: AtomicLazyCell<Defaultable> = AtomicLazyCell::default();
        let _: AtomicLazyCell<NonDefaultable> = AtomicLazyCell::default();
    }
}
