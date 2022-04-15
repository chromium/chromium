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

//! Explicit stack slots, which can be used for stack emplacement.
//!
//! A [`Slot`] is uninitialized storage on the stack that can be manipulated
//! explicitly. Notionally, a [`Slot<T>`] represents a `let x: T;` in some
//! function's stack.
//!
//! [`Slot`]s mut be created with the [`slot!()`] macro:
//! ```
//! # use moveit::{slot};
//! slot!(storage);
//! let mut x = storage.put(42);
//! *x /= 2;
//! assert_eq!(*x, 21);
//! ```
//! Unfortunately, due to the constrains of Rust today, it is not possible to
//! produce a [`Slot`] as part of a larger expression; since it needs to expand
//! to a `let` to bind the stack location, [`slot!()`] must be a statement, not
//! an expression.
//!
//! [`Slot`]s can also be used to implement a sort of "guaranteed RVO":
//! ```
//! # use moveit::{slot, Slot, move_ref::MoveRef};
//! fn returns_on_the_stack(val: i32, storage: Slot<i32>) -> Option<MoveRef<i32>> {
//!   if val == 0 {
//!     return None
//!   }
//!   Some(storage.put(val))
//! }
//!
//! slot!(storage);
//! let val = returns_on_the_stack(42, storage);
//! assert_eq!(*val.unwrap(), 42);
//! ```
//!
//! [`Slot`]s provide a natural location for emplacing values on the stack.
//! The [`moveit!()`] macro is intended to make this operation
//! straight-forward.
//!
//! # [`DroppingSlot`]
//!
//! [`DroppingSlot`] is a support type similar to [`Slot`] that is used for
//! implementing [`DerefMove`], but which users should otherwise not construct
//! themselves (despite it being otherwise perfectly safe to do so).

use core::mem;
use core::mem::MaybeUninit;
use core::pin::Pin;
use core::ptr;

use crate::drop_flag::DropFlag;
use crate::move_ref::MoveRef;
use crate::new;
use crate::new::New;
use crate::new::TryNew;

#[cfg(doc)]
use {crate::move_ref::DerefMove, alloc::boxed::Box};

/// An empty slot on the stack into which a value could be emplaced.
///
/// The `'frame` lifetime refers to the lifetime of the stack frame this
/// `Slot`'s storage is allocated on.
///
/// See [`slot!()`] and [the module documentation][self].
pub struct Slot<'frame, T> {
  ptr: &'frame mut MaybeUninit<T>,
  drop_flag: DropFlag<'frame>,
}

impl<'frame, T> Slot<'frame, T> {
  /// Creates a new `Slot` with the given pointer as its basis.
  ///
  /// To safely construct a `Slot`, use [`slot!()`].
  ///
  /// # Safety
  ///
  /// `ptr` must not be outlived by any other pointers to its allocation.
  ///
  /// `drop_flag`'s value must be dead, and must be a drop flag governing
  /// the destruction of `ptr`'s storage in an appropriate manner as described
  /// in [`moveit::drop_flag`][crate::drop_flag].
  pub unsafe fn new_unchecked(
    ptr: &'frame mut MaybeUninit<T>,
    drop_flag: DropFlag<'frame>,
  ) -> Self {
    Self { ptr, drop_flag }
  }

  /// Put `val` into this slot, returning a new [`MoveRef`].
  pub fn put(self, val: T) -> MoveRef<'frame, T> {
    unsafe {
      // SAFETY: Pinning is conserved by this operation.
      Pin::into_inner_unchecked(self.pin(val))
    }
  }

  /// Pin `val` into this slot, returning a new, pinned [`MoveRef`].
  pub fn pin(self, val: T) -> Pin<MoveRef<'frame, T>> {
    self.emplace(new::of(val))
  }

  /// Emplace `new` into this slot, returning a new, pinned [`MoveRef`].
  pub fn emplace<N: New<Output = T>>(self, new: N) -> Pin<MoveRef<'frame, T>> {
    match self.try_emplace(new) {
      Ok(x) => x,
      Err(e) => match e {},
    }
  }

  /// Try to emplace `new` into this slot, returning a new, pinned [`MoveRef`].
  pub fn try_emplace<N: TryNew<Output = T>>(
    self,
    new: N,
  ) -> Result<Pin<MoveRef<'frame, T>>, N::Error> {
    unsafe {
      self.drop_flag.inc();
      new.try_new(Pin::new_unchecked(self.ptr))?;
      Ok(MoveRef::into_pin(MoveRef::new_unchecked(
        self.ptr.assume_init_mut(),
        self.drop_flag,
      )))
    }
  }

  /// Converts this into a slot for a pinned `T`.
  ///
  /// This is safe, since this `Slot` owns the referenced data, and
  /// `Pin` is explicitly a `repr(transparent)` type.
  pub fn into_pinned(self) -> Slot<'frame, Pin<T>> {
    unsafe { self.cast() }
  }

  /// Converts this `Slot` from being a slot for a `T` to being a slot for
  /// some other type `U`.
  ///
  /// ```
  /// # use moveit::{Slot, MoveRef};
  /// moveit::slot!(place: u32);
  /// let foo: MoveRef<u16> = unsafe { place.cast::<u16>() }.put(42);
  /// ```
  ///
  /// # Safety
  ///
  /// `T` must have at least the size and alignment as `U`.
  pub unsafe fn cast<U>(self) -> Slot<'frame, U> {
    debug_assert!(mem::size_of::<T>() >= mem::size_of::<U>());
    debug_assert!(mem::align_of::<T>() >= mem::align_of::<U>());
    Slot {
      ptr: &mut *self.ptr.as_mut_ptr().cast(),
      drop_flag: self.drop_flag,
    }
  }
}

impl<'frame, T> Slot<'frame, Pin<T>> {
  /// Converts this into a slot for an unpinned `T`.
  ///
  /// This is safe, since this `Slot` owns the referenced data, and
  /// `Pin` is explicitly a `repr(transparent)` type.
  ///
  /// Moreover, no actual unpinning is occurring: the referenced data must
  /// be uninitialized, so it cannot have a pinned referent.
  pub fn into_unpinned(self) -> Slot<'frame, T> {
    unsafe { self.cast() }
  }
}

/// Similar to a [`Slot`], but able to drop its contents.
///
/// A `DroppingSlot` wraps a [`Slot`], and will drop its contents if the
/// [`Slot`]'s drop flag is dead at the time of the `DroppingSlot`'s
/// destruction.
///
/// This type has an API similar to [`Slot`]'s, but rather than returning
/// `MoveRef`s, which would own the contents of this slot, we return a `&mut T`
/// and a [`DropFlag`], which the caller can assemble into an
/// appropriately-shaped `MoveRef`. The drop flag will be one decrement away
/// from being dead; callers should make sure to decremement it to trigger
/// destruction.
///
/// `DroppingSlot` is intended to be used with [`DerefMove::deref_move()`],
/// and will usually not be created by `moveit`'s users. However, [`slot!()`]
/// provides `DroppingSlot` support, too. These slots will silently forget their
/// contents if the drop flag is left untouched, rather than crash.
pub struct DroppingSlot<'frame, T> {
  ptr: &'frame mut MaybeUninit<T>,
  drop_flag: DropFlag<'frame>,
}

impl<'frame, T> DroppingSlot<'frame, T> {
  /// Creates a new `DroppingSlot` with the given pointer as its basis.
  ///
  /// To safely construct a `DroppingSlot`, use [`slot!()`].
  ///
  /// # Safety
  ///
  /// `ptr` must not be outlived by any other pointers to its allocation.
  ///
  /// `drop_flag`'s value must be dead, and must be a drop flag governing
  /// the destruction of `ptr`'s storage in an appropriate manner as described
  /// in [`moveit::drop_flag`][crate::drop_flag].
  pub unsafe fn new_unchecked(
    ptr: &'frame mut MaybeUninit<T>,
    drop_flag: DropFlag<'frame>,
  ) -> Self {
    drop_flag.inc();
    Self { ptr, drop_flag }
  }

  /// Put `val` into this slot, returning a reference to it.
  pub fn put(self, val: T) -> (&'frame mut T, DropFlag<'frame>) {
    ({ self.ptr }.write(val), self.drop_flag)
  }

  /// Pin `val` into this slot, returning a reference to it.
  ///
  /// # Safety
  ///
  /// This function pins the memory this slot wraps, but does not guarantee its
  /// destructor is run; that is the caller's responsibility, by decrementing
  /// the given [`DropFlag`].
  pub unsafe fn pin(self, val: T) -> (Pin<&'frame mut T>, DropFlag<'frame>) {
    self.emplace(new::of(val))
  }

  /// Emplace `new` into this slot, returning a reference to it.
  ///
  /// # Safety
  ///
  /// This function pins the memory this slot wraps, but does not guarantee its
  /// destructor is run; that is the caller's responsibility, by decrementing
  /// the given [`DropFlag`].
  pub unsafe fn emplace<N: New<Output = T>>(
    self,
    new: N,
  ) -> (Pin<&'frame mut T>, DropFlag<'frame>) {
    match self.try_emplace(new) {
      Ok((x, d)) => (x, d),
      Err(e) => match e {},
    }
  }

  /// Try to emplace `new` into this slot, returning a reference to it.
  ///
  /// # Safety
  ///
  /// This function pins the memory this slot wraps, but does not guarantee its
  /// destructor is run; that is the caller's responsibility, by decrementing
  /// the given [`DropFlag`].
  pub unsafe fn try_emplace<N: TryNew<Output = T>>(
    self,
    new: N,
  ) -> Result<(Pin<&'frame mut T>, DropFlag<'frame>), N::Error> {
    self.drop_flag.inc();
    new.try_new(Pin::new_unchecked(self.ptr))?;
    Ok((
      Pin::new_unchecked(self.ptr.assume_init_mut()),
      self.drop_flag,
    ))
  }
}

#[doc(hidden)]
#[allow(missing_docs)]
pub mod __macro {
  use super::*;
  use crate::drop_flag::QuietFlag;
  pub use core;

  pub struct SlotDropper<T> {
    val: MaybeUninit<T>,
    drop_flag: QuietFlag,
  }

  impl<T> SlotDropper<T> {
    #[allow(clippy::new_without_default)]
    pub fn new() -> Self {
      Self {
        val: MaybeUninit::uninit(),
        drop_flag: QuietFlag::new(),
      }
    }

    // Workaround for `unsafe {}` unhygine wrt to lints.
    //
    // This function is still `unsafe`.
    pub fn new_unchecked_hygine_hack(&mut self) -> DroppingSlot<T> {
      unsafe {
        DroppingSlot::new_unchecked(&mut self.val, self.drop_flag.flag())
      }
    }
  }

  impl<T> Drop for SlotDropper<T> {
    fn drop(&mut self) {
      if self.drop_flag.flag().is_dead() {
        unsafe { ptr::drop_in_place(self.val.assume_init_mut()) }
      }
    }
  }

  // Workaround for `unsafe {}` unhygine wrt to lints.
  //
  // This function is still `unsafe`.
  pub fn new_unchecked_hygine_hack<'frame, T>(
    ptr: &'frame mut MaybeUninit<T>,
    drop_flag: DropFlag<'frame>,
  ) -> Slot<'frame, T> {
    unsafe { Slot::new_unchecked(ptr, drop_flag) }
  }
}

/// Constructs a new [`Slot`].
///
/// Because [`Slot`]s need to own data on the stack, but that data cannot
/// move with the [`Slot`], it must be constructed using this macro. For
/// example:
/// ```
/// moveit::slot!(x, y: bool);
/// let x = x.put(5);
/// let y = y.put(false);
/// ```
///
/// This macro is especially useful for passing data into functions that want to
/// emplace a value into the caller.
///
/// The `slot!(#[dropping] x)` syntax can be used to create a [`DroppingSlot`]
/// instead. This should be a comparatively rare operation.
///
/// This macro can also be used without arguments to create a *temporary*
/// [`Slot`]. Such types cannot be assigned to variables but can be used as
/// part of a larger expression:
///
/// ```compile_fail
/// # use moveit::Slot;
/// let bad: Slot<i32> = moveit::slot!();
/// bad.put(4);  // Borrow check error.
/// ```
///
/// ```
/// # use moveit::Slot;
/// fn do_thing(x: Slot<i32>) { /* ... */ }
/// do_thing(moveit::slot!())
/// ```
#[macro_export]
macro_rules! slot {
  () => {
    $crate::slot::__macro::new_unchecked_hygine_hack(
      &mut $crate::slot::__macro::core::mem::MaybeUninit::uninit(),
      $crate::drop_flag::TrappedFlag::new().flag(),
    )
  };
  (#[dropping]) => {
    $crate::slot::__macro::SlotDropper::new().new_unchecked_hygine_hack()
  };
  ($($name:ident $(: $ty:ty)?),* $(,)*) => {$(
    let mut uninit = $crate::slot::__macro::core::mem::MaybeUninit::<
      $crate::slot!(@tyof $($ty)?)
    >::uninit();let trap = $crate::drop_flag::TrappedFlag::new();
    let $name = $crate::slot::__macro::new_unchecked_hygine_hack(
      &mut uninit,
      trap.flag()
    );
  )*};
  (#[dropping] $($name:ident $(: $ty:ty)?),* $(,)*) => {$(
    let mut uninit = $crate::slot::__macro::SlotDropper::<
      $crate::slot!(@tyof $($ty)?)
    >::new();
    #[allow(unsafe_code, unused_unsafe)]
    let $name = uninit.new_unchecked_hygine_hack();
  )*};
  (@tyof) => {_};
  (@tyof $ty:ty) => {$ty};
}
