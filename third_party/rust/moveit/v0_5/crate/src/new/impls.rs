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

//! Trivial impls for `std` types.

use core::mem;
use core::mem::MaybeUninit;
use core::pin::Pin;

use crate::move_ref::MoveRef;
use crate::new::CopyNew;
use crate::new::MoveNew;
use crate::new::Swap;

macro_rules! trivial_move {
  ($($ty:ty $(where [$($targs:tt)*])?),* $(,)?) => {$(
    unsafe impl<$($($targs)*)?> MoveNew for $ty {
      unsafe fn move_new(
        src: Pin<MoveRef<Self>>,
        this: Pin<&mut MaybeUninit<Self>>,
      ) {
        let src = Pin::into_inner_unchecked(src);
        let this = Pin::into_inner_unchecked(this);
        this.write(MoveRef::into_inner(src));
      }
    }

    impl<$($($targs)*)?> Swap for $ty {
      fn swap_with(self: Pin<&mut Self>, that: Pin<&mut Self>) {
        unsafe {
          let zelf = Pin::into_inner_unchecked(self);
          let that = Pin::into_inner_unchecked(that);
          mem::swap(zelf, that);
        }
      }
    }
  )*}
}

macro_rules! trivial_copy {
  ($($ty:ty $(where [$($targs:tt)*])?),* $(,)?) => {$(
    unsafe impl<$($($targs)*)?> MoveNew for $ty {
      unsafe fn move_new(
        src: Pin<MoveRef<Self>>,
        this: Pin<&mut MaybeUninit<Self>>,
      ) {
        let src = Pin::into_inner_unchecked(src);
        let this = Pin::into_inner_unchecked(this);
        this.write(MoveRef::into_inner(src));
      }
    }

    impl<$($($targs)*)?> Swap for $ty {
      fn swap_with(self: Pin<&mut Self>, that: Pin<&mut Self>) {
        unsafe {
          let zelf = Pin::into_inner_unchecked(self);
          let that = Pin::into_inner_unchecked(that);
          mem::swap(zelf, that);
        }
      }
    }

    unsafe impl<$($($targs)*)?> CopyNew for $ty where Self: Clone {
      unsafe fn copy_new(
        src: &Self,
        this: Pin<&mut MaybeUninit<Self>>,
      ) {
        let this = Pin::into_inner_unchecked(this);
        this.write(src.clone());
      }
    }
  )*}
}

trivial_move! {
  &mut T where [T: ?Sized],

  core::sync::atomic::AtomicI8,
  core::sync::atomic::AtomicI16,
  core::sync::atomic::AtomicI32,
  core::sync::atomic::AtomicI64,
  core::sync::atomic::AtomicIsize,
  core::sync::atomic::AtomicU8,
  core::sync::atomic::AtomicU16,
  core::sync::atomic::AtomicU32,
  core::sync::atomic::AtomicU64,
  core::sync::atomic::AtomicUsize,
  core::sync::atomic::AtomicPtr<T> where [T],
}

trivial_copy! {
  (), char, bool,
  i8, i16, i32, i64, i128, isize,
  u8, u16, u32, u64, u128, usize,

  &T where [T: ?Sized],
  *const T where [T: ?Sized],
  *mut T where [T: ?Sized],

  core::alloc::Layout,

  core::cell::UnsafeCell<T> where [T],
  core::cell::Cell<T> where [T],
  core::cell::RefCell<T> where [T],
  core::cell::Ref<'_, T> where [T],
  core::cell::RefMut<'_, T> where [T],

  core::marker::PhantomData<T> where [T: ?Sized],
  core::marker::PhantomPinned,

  core::mem::Discriminant<T> where [T],
  core::mem::ManuallyDrop<T> where [T],
  core::mem::MaybeUninit<T> where [T],

  core::num::NonZeroI8,
  core::num::NonZeroI16,
  core::num::NonZeroI32,
  core::num::NonZeroI64,
  core::num::NonZeroI128,
  core::num::NonZeroIsize,
  core::num::NonZeroU8,
  core::num::NonZeroU16,
  core::num::NonZeroU32,
  core::num::NonZeroU64,
  core::num::NonZeroU128,
  core::num::NonZeroUsize,
  core::num::Wrapping<T> where [T],

  core::option::Option<T> where [T],

  core::pin::Pin<T> where [T],
  core::ptr::NonNull<T> where [T],

  core::result::Result<T, E> where [T, E],

  core::time::Duration,
}

#[cfg(feature = "alloc")]
trivial_copy! {
  alloc::boxed::Box<T> where [T],

  alloc::collections::binary_heap::BinaryHeap<T> where [T],
  alloc::collections::btree_map::BTreeMap<K, V> where [K, V],
  alloc::collections::btree_set::BTreeSet<T> where [T],
  alloc::collections::linked_list::LinkedList<T> where [T],
  alloc::collections::vec_deque::VecDeque<T> where [T],

  alloc::rc::Rc<T> where [T],
  alloc::rc::Weak<T> where [T],
  alloc::sync::Arc<T> where [T],
  alloc::sync::Weak<T> where [T],

  alloc::string::String,
  alloc::vec::Vec<T> where [T],
}
