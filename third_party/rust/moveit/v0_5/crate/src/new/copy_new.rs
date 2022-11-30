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

use core::mem;
use core::mem::MaybeUninit;
use core::ops::Deref;
use core::pin::Pin;

use crate::new;
use crate::new::New;

/// A copy constructible type: a destination-aware `Clone`.
///
/// # Safety
///
/// After [`CopyNew::copy_new()`] is called:
/// - `this` must have been initialized.
pub unsafe trait CopyNew: Sized {
  /// Copy-construct `src` into `this`, effectively re-pinning it at a new
  /// location.
  ///
  /// # Safety
  ///
  /// The same safety requirements of [`New::new()`] apply.
  unsafe fn copy_new(src: &Self, this: Pin<&mut MaybeUninit<Self>>);
}

/// Returns a new `New` that uses a copy constructor.
#[inline]
pub fn copy<P>(ptr: P) -> impl New<Output = P::Target>
where
  P: Deref,
  P::Target: CopyNew,
{
  unsafe {
    new::by_raw(move |this| {
      CopyNew::copy_new(&*ptr, this);

      // Because `*ptr` is still intact, we can drop it normally.
      mem::drop(ptr)
    })
  }
}
