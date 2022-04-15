// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use cxx::{memory::UniquePtrTarget, UniquePtr};
use moveit::{CopyNew, DerefMove, MoveNew, New};
use std::{marker::PhantomPinned, mem::MaybeUninit, ops::Deref, pin::Pin};

/// A trait representing a parameter to a C++ function which is received
/// by value.
///
/// Rust has the concept of receiving parameters by _move_ or by _reference_.
/// C++ has the concept of receiving a parameter by 'value', which means
/// the parameter gets copied.
///
/// To make it easy to pass such parameters from Rust, this trait exists.
/// It is implemented both for references `&T` and for `UniquePtr<T>`,
/// subject to the presence or absence of suitable copy and move constructors.
/// This allows you to pass in parameters by copy (as is ergonomic and normal
/// in C++) retaining the original parameter; or by move semantics thus
/// destroying the object you're passing in. Simply use a reference if you want
/// copy semantics, or the item itself if you want move semantics.
///
/// It is not recommended that you implement this trait, nor that you directly
/// use its methods, which are for use by `autocxx` generated code only.
///
/// # Use of `moveit` traits
///
/// Most of the implementations of this trait require the type to implement
/// [`CopyNew`], which is simply the `autocxx`/`moveit` way of saying that
/// the type has a copy constructor in C++.
///
/// # Being explicit
///
/// If you wish to explicitly force either a move or a copy of some type,
/// use [`as_mov`] or [`as_copy`].
///
/// # Performance
///
/// At present, some additional copying occurs for all implementations of
/// this trait other than that for [`cxx::UniquePtr`]. In the future it's
/// hoped that the implementation for `&T where T: CopyNew` can also avoid
/// this extra copying.
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
pub unsafe trait ValueParam<T> {
    /// Any stack storage required. If, as part of passing to C++,
    /// we need to store a temporary copy of the value, this will be `T`,
    /// otherwise `()`.
    #[doc(hidden)]
    type StackStorage;
    /// Populate the stack storage given as a parameter. Only called if you
    /// return `true` from `needs_stack_space`.
    ///
    /// # Safety
    ///
    /// Callers must guarantee that this object will not move in memory
    /// between this call and any subsequent `get_ptr` call or drop.
    #[doc(hidden)]
    unsafe fn populate_stack_space(self, this: Pin<&mut Option<Self::StackStorage>>);
    /// Retrieve the pointer to the underlying item, to be passed to C++.
    /// Note that on the C++ side this is currently passed to `std::move`
    /// and therefore may be mutated.
    #[doc(hidden)]
    fn get_ptr(stack: Pin<&mut Self::StackStorage>) -> *mut T;
    #[doc(hidden)]
    /// Any special drop steps required for the stack storage. This is not
    /// necessary if the `StackStorage` type is something self-dropping
    /// such as `UniquePtr`; it's only necessary if it's something where
    /// manual management is required such as `MaybeUninit`.
    fn do_drop(_stack: Pin<&mut Self::StackStorage>) {}
}

unsafe impl<T> ValueParam<T> for &T
where
    T: CopyNew,
{
    type StackStorage = MaybeUninit<T>;

    unsafe fn populate_stack_space(self, mut stack: Pin<&mut Option<Self::StackStorage>>) {
        // Safety: we won't move/swap things within the pin.
        let slot = Pin::into_inner_unchecked(stack.as_mut());
        *slot = Some(MaybeUninit::uninit());
        crate::moveit::new::copy(self).new(Pin::new_unchecked(slot.as_mut().unwrap()))
    }
    fn get_ptr(stack: Pin<&mut Self::StackStorage>) -> *mut T {
        // Safety: it's OK to (briefly) create a reference to the T because we
        // populated it within `populate_stack_space`. It's OK to unpack the pin
        // because we're not going to move the contents.
        unsafe { Pin::into_inner_unchecked(stack).assume_init_mut() as *mut T }
    }

    fn do_drop(stack: Pin<&mut Self::StackStorage>) {
        // Switch to MaybeUninit::assume_init_drop when stabilized
        // Safety: per caller guarantees of populate_stack_space, we know this hasn't moved.
        unsafe { std::ptr::drop_in_place(Pin::into_inner_unchecked(stack).assume_init_mut()) };
    }
}

unsafe impl<T> ValueParam<T> for UniquePtr<T>
where
    T: UniquePtrTarget,
{
    type StackStorage = UniquePtr<T>;

    unsafe fn populate_stack_space(self, mut stack: Pin<&mut Option<Self::StackStorage>>) {
        // Safety: we will not move the contents of the pin.
        *Pin::into_inner_unchecked(stack.as_mut()) = Some(self)
    }

    fn get_ptr(stack: Pin<&mut Self::StackStorage>) -> *mut T {
        // Safety: we won't move/swap the contents of the outer pin, nor of the
        // type stored within the UniquePtr.
        unsafe {
            (Pin::into_inner_unchecked(
                (*Pin::into_inner_unchecked(stack))
                    .as_mut()
                    .expect("Passed a NULL UniquePtr as a C++ value parameter"),
            )) as *mut T
        }
    }
}

unsafe impl<T> ValueParam<T> for Pin<Box<T>> {
    type StackStorage = Pin<Box<T>>;

    unsafe fn populate_stack_space(self, mut stack: Pin<&mut Option<Self::StackStorage>>) {
        // Safety: we will not move the contents of the pin.
        *Pin::into_inner_unchecked(stack.as_mut()) = Some(self)
    }

    fn get_ptr(stack: Pin<&mut Self::StackStorage>) -> *mut T {
        // Safety: we won't move/swap the contents of the outer pin, nor of the
        // type stored within the UniquePtr.
        unsafe {
            (Pin::into_inner_unchecked((*Pin::into_inner_unchecked(stack)).as_mut())) as *mut T
        }
    }
}

unsafe impl<'a, T: 'a> ValueParam<T> for &'a UniquePtr<T>
where
    T: UniquePtrTarget + CopyNew,
{
    type StackStorage = <&'a T as ValueParam<T>>::StackStorage;

    unsafe fn populate_stack_space(self, stack: Pin<&mut Option<Self::StackStorage>>) {
        self.as_ref()
            .expect("Passed a NULL &UniquePtr as a C++ value parameter")
            .populate_stack_space(stack)
    }

    fn get_ptr(stack: Pin<&mut Self::StackStorage>) -> *mut T {
        <&'a T as ValueParam<T>>::get_ptr(stack)
    }

    fn do_drop(stack: Pin<&mut Self::StackStorage>) {
        <&'a T as ValueParam<T>>::do_drop(stack)
    }
}

unsafe impl<'a, T: 'a> ValueParam<T> for &'a Pin<Box<T>>
where
    T: CopyNew,
{
    type StackStorage = <&'a T as ValueParam<T>>::StackStorage;

    unsafe fn populate_stack_space(self, stack: Pin<&mut Option<Self::StackStorage>>) {
        self.as_ref().get_ref().populate_stack_space(stack)
    }

    fn get_ptr(stack: Pin<&mut Self::StackStorage>) -> *mut T {
        <&'a T as ValueParam<T>>::get_ptr(stack)
    }

    fn do_drop(stack: Pin<&mut Self::StackStorage>) {
        <&'a T as ValueParam<T>>::do_drop(stack)
    }
}

/// Explicitly force a value parameter to be taken using any type of [`crate::moveit::new::New`],
/// i.e. a constructor.
pub fn as_new<N: New<Output = T>, T>(constructor: N) -> impl ValueParam<T> {
    ByNew(constructor)
}

/// Explicitly force a value parameter to be taken by copy.
pub fn as_copy<P: Deref<Target = T>, T>(ptr: P) -> impl ValueParam<T>
where
    T: CopyNew,
{
    ByNew(crate::moveit::new::copy(ptr))
}

/// Explicitly force a value parameter to be taken usign C++ move semantics.
pub fn as_mov<P: DerefMove + Deref<Target = T>, T>(ptr: P) -> impl ValueParam<T>
where
    P: DerefMove,
    P::Target: MoveNew,
{
    ByNew(crate::moveit::new::mov(ptr))
}

#[doc(hidden)]
pub struct ByNew<N: New>(N);

unsafe impl<N, T> ValueParam<T> for ByNew<N>
where
    N: New<Output = T>,
{
    type StackStorage = MaybeUninit<T>;

    unsafe fn populate_stack_space(self, mut stack: Pin<&mut Option<Self::StackStorage>>) {
        // Safety: we won't move/swap things within the pin.
        let slot = Pin::into_inner_unchecked(stack.as_mut());
        *slot = Some(MaybeUninit::uninit());
        self.0.new(Pin::new_unchecked(slot.as_mut().unwrap()))
    }
    fn get_ptr(stack: Pin<&mut Self::StackStorage>) -> *mut T {
        // Safety: it's OK to (briefly) create a reference to the T because we
        // populated it within `populate_stack_space`. It's OK to unpack the pin
        // because we're not going to move the contents.
        unsafe { Pin::into_inner_unchecked(stack).assume_init_mut() as *mut T }
    }

    fn do_drop(stack: Pin<&mut Self::StackStorage>) {
        // Switch to MaybeUninit::assume_init_drop when stabilized
        // Safety: per caller guarantees of populate_stack_space, we know this hasn't moved.
        unsafe { std::ptr::drop_in_place(Pin::into_inner_unchecked(stack).assume_init_mut()) };
    }
}

/// Implementation detail for how we pass value parameters into C++.
/// This type is instantiated by auto-generated autocxx code each time we
/// need to pass a value parameter into C++, and will take responsibility
/// for extracting that value parameter from the [`ValueParam`] and doing
/// any later cleanup.
#[doc(hidden)]
pub struct ValueParamHandler<T, VP: ValueParam<T>> {
    // We can't populate this on 'new' because the object may move.
    // Hence this is an Option - it's None until populate is called.
    space: Option<VP::StackStorage>,
    _pinned: PhantomPinned,
}

impl<T, VP: ValueParam<T>> ValueParamHandler<T, VP> {
    /// Populate this stack space if needs be. Note safety guarantees
    /// on [`get_ptr`].
    ///
    /// # Safety
    ///
    /// Callers must call [`populate`] exactly once prior to calling [`get_ptr`].
    pub unsafe fn populate(self: Pin<&mut Self>, param: VP) {
        // Structural pinning, as documented in [`std::pin`].
        param.populate_stack_space(self.map_unchecked_mut(|s| &mut s.space))
    }

    /// Return a pointer to the underlying value which can be passed to C++.
    ///
    /// Per the unsafety contract of [`populate`], [`populate`] has been called exactly once
    /// prior to this call.
    pub fn get_ptr(self: Pin<&mut Self>) -> *mut T {
        // Structural pinning, as documented in [`std::pin`]. `map_unchecked_mut` doesn't play
        // nicely with `unwrap`, so we have to do it manually.
        unsafe {
            VP::get_ptr(Pin::new_unchecked(
                self.get_unchecked_mut().space.as_mut().unwrap(),
            ))
        }
    }
}

impl<T, VP: ValueParam<T>> Default for ValueParamHandler<T, VP> {
    fn default() -> Self {
        Self {
            space: None,
            _pinned: PhantomPinned,
        }
    }
}

impl<T, VP: ValueParam<T>> Drop for ValueParamHandler<T, VP> {
    fn drop(&mut self) {
        if let Some(space) = self.space.as_mut() {
            unsafe { VP::do_drop(Pin::new_unchecked(space)) }
        }
    }
}
