// Copyright 2022 Google LLC
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

//! Support for `cxx` types.

use std::mem::MaybeUninit;
use std::pin::Pin;

use cxx::memory::UniquePtrTarget;
use cxx::UniquePtr;

use crate::EmplaceUnpinned;
use crate::TryNew;

/// A type which has the ability to create heap storage space
/// for itself in C++, without initializing that storage.
///
/// # Safety
///
/// Implementers must ensure that the pointer returned by
/// `allocate_uninitialized_cpp_storage` is a valid, non-null,
/// pointer to a new but uninitialized storage block, and that
/// such blocks must be freeable using either of these routes:
///
/// * before they're initialized, using `free_uninitialized_cpp_storage`
/// * after they're initialized, via a delete expression like `delete p;`
pub unsafe trait MakeCppStorage: Sized {
  /// Allocates heap space for this type in C++ and return a pointer
  /// to that space, but do not initialize that space (i.e. do not
  /// yet call a constructor).
  ///
  /// # Safety
  ///
  /// To avoid memory leaks, callers must ensure that this space is
  /// freed using `free_uninitialized_cpp_storage`, or is converted into
  /// a [`UniquePtr`] such that it can later be freed by
  /// `std::unique_ptr<T, std::default_delete<T>>`.
  unsafe fn allocate_uninitialized_cpp_storage() -> *mut Self;

  /// Frees a C++ allocation which has not yet
  /// had a constructor called.
  ///
  /// # Safety
  ///
  /// Callers guarantee that the pointer here was allocated by
  /// `allocate_uninitialized_cpp_storage` and has not been
  /// initialized.
  unsafe fn free_uninitialized_cpp_storage(ptr: *mut Self);
}

impl<T: MakeCppStorage + UniquePtrTarget> EmplaceUnpinned<T> for UniquePtr<T> {
  fn try_emplace<N: TryNew<Output = T>>(n: N) -> Result<Self, N::Error> {
    unsafe {
      let uninit_ptr = T::allocate_uninitialized_cpp_storage();
      let uninit =
        Pin::new_unchecked(&mut *(uninit_ptr as *mut MaybeUninit<T>));
      // FIXME - this is not panic safe.
      let result = n.try_new(uninit);
      if let Err(err) = result {
        T::free_uninitialized_cpp_storage(uninit_ptr);
        return Err(err);
      }
      Ok(UniquePtr::from_raw(uninit_ptr))
    }
  }
}
