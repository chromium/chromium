// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! It would be highly desirable to share a lot of this code with `value_param.rs`
//! but this proves to be surprisingly fiddly.

use cxx::{memory::UniquePtrTarget, UniquePtr};
use moveit::MoveRef;
use std::{
    marker::{PhantomData, PhantomPinned},
    pin::Pin,
};

/// A trait representing a parameter to a C++ function which is received
/// by rvalue (i.e. by move).
///
/// # Panics
///
/// The implementations of this trait which take a [`cxx::UniquePtr`] will
/// panic if the pointer is NULL.
///
/// # Safety
///
/// Implementers must guarantee that the pointer returned by `get_ptr`
/// is of the correct size and alignment of `T`.
pub unsafe trait RValueParam<T>: Sized {
    /// Retrieve the pointer to the underlying item, to be passed to C++.
    /// Note that on the C++ side this is currently passed to `std::move`
    /// and therefore may be mutated.
    #[doc(hidden)]
    fn get_ptr(stack: Pin<&mut Self>) -> *mut T;
}

unsafe impl<T> RValueParam<T> for UniquePtr<T>
where
    T: UniquePtrTarget,
{
    fn get_ptr(stack: Pin<&mut Self>) -> *mut T {
        // Safety: we won't move/swap the contents of the outer pin, nor of the
        // type stored within the UniquePtr.
        unsafe {
            (Pin::into_inner_unchecked(
                (*Pin::into_inner_unchecked(stack))
                    .as_mut()
                    .expect("Passed a NULL UniquePtr as a C++ rvalue parameter"),
            )) as *mut T
        }
    }
}

unsafe impl<T> RValueParam<T> for Pin<Box<T>> {
    fn get_ptr(stack: Pin<&mut Self>) -> *mut T {
        // Safety: we won't move/swap the contents of the outer pin, nor of the
        // type stored within the UniquePtr.
        unsafe {
            (Pin::into_inner_unchecked((*Pin::into_inner_unchecked(stack)).as_mut())) as *mut T
        }
    }
}

unsafe impl<'a, T> RValueParam<T> for Pin<MoveRef<'a, T>> {
    fn get_ptr(stack: Pin<&mut Self>) -> *mut T {
        // Safety: we won't move/swap the contents of the outer pin, nor of the
        // type stored within the UniquePtr.
        unsafe {
            (Pin::into_inner_unchecked((*Pin::into_inner_unchecked(stack)).as_mut())) as *mut T
        }
    }
}

/// Implementation detail for how we pass rvalue parameters into C++.
/// This type is instantiated by auto-generated autocxx code each time we
/// need to pass a value parameter into C++, and will take responsibility
/// for extracting that value parameter from the [`RValueParam`] and doing
/// any later cleanup.
#[doc(hidden)]
pub struct RValueParamHandler<T, RVP>
where
    RVP: RValueParam<T>,
{
    // We can't populate this on 'new' because the object may move.
    // Hence this is an Option - it's None until populate is called.
    space: Option<RVP>,
    _pinned: PhantomPinned,
    _data: PhantomData<T>,
}

impl<T, RVP: RValueParam<T>> RValueParamHandler<T, RVP> {
    /// Populate this stack space if needs be. Note safety guarantees
    /// on [`get_ptr`].
    ///
    /// # Safety
    ///
    /// Callers must guarantee that this type will not move
    /// in memory between calls to [`populate`] and [`get_ptr`].
    /// Callers must call [`populate`] exactly once prior to calling [`get_ptr`].
    pub unsafe fn populate(self: Pin<&mut Self>, param: RVP) {
        // Structural pinning, as documented in [`std::pin`].
        // Safety: we will not move the contents of the pin.
        *Pin::into_inner_unchecked(self.map_unchecked_mut(|s| &mut s.space)) = Some(param)
    }

    /// Return a pointer to the underlying value which can be passed to C++.
    /// Per the unsafety contract of [`populate`], the object must not have moved
    /// since it was created, and [`populate`] has been called exactly once
    /// prior to this call.
    pub fn get_ptr(self: Pin<&mut Self>) -> *mut T {
        // Structural pinning, as documented in [`std::pin`]. `map_unchecked_mut` doesn't play
        // nicely with `unwrap`, so we have to do it manually.
        unsafe {
            RVP::get_ptr(Pin::new_unchecked(
                self.get_unchecked_mut().space.as_mut().unwrap(),
            ))
        }
    }
}

impl<T, VP: RValueParam<T>> Default for RValueParamHandler<T, VP> {
    fn default() -> Self {
        Self {
            space: None,
            _pinned: PhantomPinned,
            _data: PhantomData,
        }
    }
}
