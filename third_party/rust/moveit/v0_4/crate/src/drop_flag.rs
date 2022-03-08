// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! Drop flags.
//!
//! The [`Pin<P>`] guarantees state that if we have a `T` allocated somewhere,
//! and we construct a pinned reference to it such as a `Pin<&'a mut T>`, then
//! before that "somewhere" in memory is reused by another Rust object, `T`'s
//! destructor must run.
//!
//! Normally, this isn't a problem for Rust code, since the storage of an object
//! is destroyed immediately after it is destroyed. [`DerefMove`], however,
//! breaks this expectation: it separates the destructors from its storage and
//! contents into two separately destroyed objects: a [`DerefMove::Storage`]
//! and a [`MoveRef`]. If the [`MoveRef`] is [`mem::forget`]'ed, we lose: the
//! storage will potentially be re-used.
//!
//! Therefore, we must somehow detect that [`MoveRef`]s fail to be destroyed
//! when the destructor for the corresponding storage is run, and remediate it,
//! either by leaking heap storage or aborting if we would free stack storage
//! (a panic is insufficient, since that location can be reused if the panic is
//! caught).
//!
//! A [`DropFlag`] allows us to achieve this. It is a generalized, library-level
//! version of the Rust language's drop flags, which it uses to dynamically
//! determine whether to run destructors of stack-allocated values that might
//! have been moved from. Unlike Rust language drop flags, a [`DropFlag`] is
//! actually a counter, rather than a boolean. This allows storage that holds
//! many objects, like a vector, ensure that all contents have been properly
//! destroyed.
//!
//! This module also provides two helper types simplify safe creation and
//! management of drop flags.
//!
//! See the [Rustonomicon entry](https://doc.rust-lang.org/nomicon/drop-flags.html)
//! for the Rust language equivalent.
//!
//! # Safety
//!
//! No function in this module is `unsafe`: instead, functions that construct
//! [`MoveRef`]s out of [`DropFlag`]s are `unsafe`, and their callers are
//! responsible for ensuring that the passed-in [`DropFlag`] helps uphold the
//! relevant invariants.

use core::cell::Cell;
use core::mem;
use core::mem::ManuallyDrop;
use core::ops::Deref;
use core::ops::DerefMut;

#[cfg(doc)]
use {
  crate::move_ref::{DerefMove, MoveRef},
  alloc::boxed::Box,
  core::pin::Pin,
};

/// A drop flag, for tracking successful destruction.
///
/// A `DropFlag` is a reference to a counter somewhere on the stack that lives
/// adjacent to storage for some value. It is just a counter: `unsafe` code is
/// expected to associate semantic meaning to it.
///
/// A flag with a value of zero is usually called "dead", and setting a flag to
/// the dead state is called clearing it.
///
/// See the [module documentation][self] for more information.
#[derive(Clone, Copy)]
pub struct DropFlag<'frame> {
  counter: &'frame Cell<usize>,
}

impl DropFlag<'_> {
  /// Increments the internal counter.
  ///
  /// This function does not provide any overflow protection; `unsafe` code is
  /// responsible for making sure that cannot happen.
  #[inline]
  pub fn inc(self) {
    self.counter.set(self.counter.get() + 1)
  }

  /// Decrements the internal counter and returns true if it became zero.
  ///
  /// This function will return `false` if the counter was already zero.
  #[inline]
  pub fn dec_and_check_if_died(self) -> bool {
    if self.counter.get() == 0 {
      return false;
    }
    self.counter.set(self.counter.get() - 1);
    self.is_dead()
  }

  /// Returns whether the internal counter is zero.
  #[inline]
  pub fn is_dead(self) -> bool {
    self.counter.get() == 0
  }

  /// Lengthens the lifetime of `self`.
  #[inline]
  #[allow(unused)]
  pub(crate) unsafe fn longer_lifetime<'a>(self) -> DropFlag<'a> {
    DropFlag {
      counter: mem::transmute(self.counter),
    }
  }
}

/// A wrapper for managing when a value gets dropped via a [`DropFlag`].
///
/// This type tracks the destruction state of some value relative to another
/// value via its [`DropFlag`]: for example, it might be the storage of a value
/// wrapped up in a [`MoveRef`]. When a `DroppingFlag` is destroyed, it will
/// run the destructor for the wrapped value if and only if the [`DropFlag`]
/// is dead.
///
/// This type can be viewed as using a [`DropFlag`] to "complete" a
/// [`ManuallyDrop<T>`] by explicitly tracking whether it has been dropped. The
/// flag can be used to signal whether to destroy or leak the value, but the
/// destruction occurs lazily rather than immediately when the flag is flipped.
///
/// This is useful as a [`DerefMove::Storage`] type for types where the
/// storage should be leaked if the inner type was somehow not destroyed, such
/// as in the case of heap-allocated storage like [`Box<T>`].
pub struct DroppingFlag<T> {
  value: ManuallyDrop<T>,
  counter: Cell<usize>,
}

impl<T> DroppingFlag<T> {
  /// Wraps a new value to have its drop state managed by a `DropFlag`.
  ///
  /// The drop flag will start out dead and needs to be manually incremented.
  pub fn new(value: T) -> Self {
    Self {
      value: ManuallyDrop::new(value),
      counter: Cell::new(0),
    }
  }

  /// Gets a reference to the drop flag.
  ///
  /// This function is safe; the returned reference to the drop flag cannot be
  /// used to make a previously dropped value live again.
  pub fn flag(slot: &Self) -> DropFlag {
    DropFlag {
      counter: &slot.counter,
    }
  }

  /// Splits this slot into a reference to the wrapped value plus a reference to
  /// the drop flag.
  ///
  /// This function is safe; the returned reference to the drop flag cannot be
  /// used to make a previously dropped value live again, since the value is
  /// not destroyed before the wrapper is.
  pub fn as_parts(slot: &Self) -> (&T, DropFlag) {
    (
      &slot.value,
      DropFlag {
        counter: &slot.counter,
      },
    )
  }

  /// Splits this slot into a reference to the wrapped value plus a reference to
  /// the drop flag.
  ///
  /// This function is safe; the returned reference to the drop flag cannot be
  /// used to make a previously dropped value live again, since the value is
  /// not destroyed before the wrapper is.
  pub fn as_parts_mut(slot: &mut Self) -> (&mut T, DropFlag) {
    (
      &mut slot.value,
      DropFlag {
        counter: &slot.counter,
      },
    )
  }
}

impl<T> Deref for DroppingFlag<T> {
  type Target = T;
  #[inline]
  fn deref(&self) -> &T {
    &self.value
  }
}

impl<T> DerefMut for DroppingFlag<T> {
  #[inline]
  fn deref_mut(&mut self) -> &mut T {
    &mut self.value
  }
}

impl<T> Drop for DroppingFlag<T> {
  fn drop(&mut self) {
    if Self::flag(self).is_dead() {
      unsafe {
        ManuallyDrop::drop(&mut self.value);
      }
    }
  }
}

/// An RAII trap that ensures a drop flag is correctly cleared.
///
/// This type is *similar* to a [`DroppingFlag`], except that it does not wrap
/// a value and rather than leaking memory aborts the program if its flag is
/// not cleared.
///
/// This type is useful for safely constructing [`MoveRef`]s.
pub struct TrappedFlag {
  counter: Cell<usize>,

  // In debug mode, we capture the location the trap is created at, to help
  // connect an eventual failure to the matching storage.
  #[cfg(debug_assertions)]
  location: &'static core::panic::Location<'static>,
}

impl TrappedFlag {
  /// Creates a new trap with a dead flag.
  #[cfg(debug_assertions)]
  #[track_caller]
  pub fn new() -> Self {
    Self {
      counter: Cell::new(0),
      location: core::panic::Location::caller(),
    }
  }

  /// Creates a new trap with a dead flag.
  #[cfg(not(debug_assertions))]
  pub fn new() -> Self {
    Self {
      counter: Cell::new(0),
    }
  }

  /// Returns a reference to the [`DropFlag`].
  pub fn flag(&self) -> DropFlag {
    DropFlag {
      counter: &self.counter,
    }
  }

  /// Preemptively checks that this flag has been cleared.
  ///
  /// Aborts (rather than panicking!) if the assertion fails.
  pub fn assert_cleared(&self) {
    if self.flag().is_dead() {
      return;
    }

    // We can force an abort by triggering a panic mid-unwind.
    // This is the only way to force an LLVM abort from inside of `core`.
    struct DoublePanic;
    impl Drop for DoublePanic {
      fn drop(&mut self) {
        // In tests, we don't double-panic so that we can observe the
        // failure correctly.
        if cfg!(not(test)) {
          panic!()
        }
      }
    }

    let _dp = DoublePanic;

    #[cfg(debug_assertions)]
    panic!("a critical drop flag at {} was not cleared!", self.location);

    #[cfg(not(debug_assertions))]
    panic!("a critical drop flag was not cleared!");
  }
}

impl Default for TrappedFlag {
  fn default() -> Self {
    Self::new()
  }
}

impl Drop for TrappedFlag {
  fn drop(&mut self) {
    self.assert_cleared();
  }
}

/// A [`DropFlag`] source that doesn't do anything with it.
///
/// This is similar to `TrappedFlag`, but where it does not abort the program
/// if used incorrectly. This type is generally only useful when some separate
/// mechanism is ensuring that invariants are not violated.
pub struct QuietFlag {
  counter: Cell<usize>,
}

impl QuietFlag {
  /// Creates a new dead flag.
  pub fn new() -> Self {
    Self {
      counter: Cell::new(0),
    }
  }

  /// Returns a reference to the [`DropFlag`].
  pub fn flag(&self) -> DropFlag {
    DropFlag {
      counter: &self.counter,
    }
  }
}

impl Default for QuietFlag {
  fn default() -> Self {
    Self::new()
  }
}
