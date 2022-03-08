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

use core::convert::TryFrom;
use core::convert::TryInto;
use core::marker::PhantomData;
use core::mem::MaybeUninit;
use core::pin::Pin;

use crate::new::New;
use crate::new::TryNew;

/// Returns a [`New`] that uses the provided closure for construction.
///
/// This is the most primitive [`New`]-creation function, and is almost-always
/// preferred over implementing [`New`] directly.
///
/// # Safety
///
/// `f` must respect the safety requirements of [`New`], since it is used
/// as an implementation basis.
#[inline]
pub unsafe fn by_raw<T, F>(f: F) -> impl New<Output = T>
where
  F: FnOnce(Pin<&mut MaybeUninit<T>>),
{
  struct FnNew<F, T> {
    f: F,
    _ph: PhantomData<fn(T)>,
  }

  unsafe impl<F, T> New for FnNew<F, T>
  where
    F: FnOnce(Pin<&mut MaybeUninit<T>>),
  {
    type Output = T;
    #[inline]
    unsafe fn new(self, this: Pin<&mut MaybeUninit<Self::Output>>) {
      (self.f)(this)
    }
  }

  FnNew::<F, T> {
    f,
    _ph: PhantomData,
  }
}

/// Returns a [`New`] that uses the provided closure for constructing a
/// `T`.
///
/// ```
/// # use moveit::{moveit, new};
/// moveit! {
///   let x = new::by(|| 21 * 2);
/// }
/// assert_eq!(*x, 42);
/// ```
#[inline]
pub fn by<T, F>(f: F) -> impl New<Output = T>
where
  F: FnOnce() -> T,
{
  unsafe { by_raw(|mut this| this.set(MaybeUninit::new(f()))) }
}

/// Returns a [`New`] that uses a [`From`] implementation to generate a `T`.
///
/// ```
/// # use std::pin::Pin;
/// # use moveit::{moveit, new, MoveRef};
/// moveit! {
///   let x: Pin<MoveRef<String>> = new::from("foo");
/// }
/// assert_eq!(*x, "foo");
/// ```
#[inline]
pub fn from<T: From<U>, U>(val: U) -> impl New<Output = T> {
  by(|| val.into())
}

/// Returns a [`New`] that simply returns the given value.
///
/// ```
/// # use std::pin::Pin;
/// # use moveit::{moveit, new};
/// moveit! {
///   let x = new::of(42);
/// }
/// assert_eq!(*x, 42);
/// ```
///
/// In general, you will almost always want [`from()`].
#[inline]
pub fn of<T>(val: T) -> impl New<Output = T> {
  by(|| val)
}

/// Returns a [`New`] calls [`Default`] to generate a `T`.
///
/// ```
/// # use std::pin::Pin;
/// # use moveit::{moveit, new};
/// moveit! {
///   let x = new::default::<i32>();
/// }
/// assert_eq!(*x, 0);
/// ```
#[inline]
pub fn default<T: Default>() -> impl New<Output = T> {
  by(Default::default)
}

/// Returns a [`TryNew`] that uses the provided closure for construction.
///
/// This is the most primitive [`TryNew`]-creation function, and is
/// almost-always preferred over implementing [`TryNew`] directly.
///
/// # Safety
///
/// `f` must respect the safety requirements of [`TryNew`], since it is used
/// as an implementation basis.
#[inline]
pub unsafe fn try_by_raw<T, E, F>(f: F) -> impl TryNew<Output = T, Error = E>
where
  F: FnOnce(Pin<&mut MaybeUninit<T>>) -> Result<(), E>,
{
  struct FnNew<F, T, E> {
    f: F,
    _ph: PhantomData<fn(T) -> E>,
  }

  unsafe impl<F, T, E> TryNew for FnNew<F, T, E>
  where
    F: FnOnce(Pin<&mut MaybeUninit<T>>) -> Result<(), E>,
  {
    type Output = T;
    type Error = E;
    #[inline]
    unsafe fn try_new(
      self,
      this: Pin<&mut MaybeUninit<Self::Output>>,
    ) -> Result<(), E> {
      (self.f)(this)
    }
  }

  FnNew::<F, T, E> {
    f,
    _ph: PhantomData,
  }
}

/// Returns a [`TryNew`] that uses the provided closure for constructing a
/// `T`.
#[inline]
pub fn try_by<T, E, F>(f: F) -> impl TryNew<Output = T, Error = E>
where
  F: FnOnce() -> Result<T, E>,
{
  unsafe {
    try_by_raw(|this| {
      this.get_unchecked_mut().write(f()?);
      Ok(())
    })
  }
}

/// Returns a [`TryNew`] that uses a `TryFrom` implementation to generate a `T`.
#[inline]
pub fn try_from<T: TryFrom<U>, U>(
  val: U,
) -> impl TryNew<Output = T, Error = T::Error> {
  try_by(|| val.try_into())
}
