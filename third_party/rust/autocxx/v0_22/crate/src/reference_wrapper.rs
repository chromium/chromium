// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

/// A C++ const reference. These are different from Rust's `&T` in that
/// these may exist even while the object is mutated elsewhere.
///
/// This is a trait not a struct due to the nuances of Rust's orphan rule
/// - implemntations of this trait are found in each set of generated bindings
/// but they are essentially the same.
pub trait CppRef<'a, T> {
    /// Retrieve the underlying C++ pointer.
    fn as_ptr(&self) -> *const T;

    /// Get a regular Rust reference out of this C++ reference.
    ///
    /// # Safety
    ///
    /// Callers must guarantee that the referent is not modified by any other
    /// C++ or Rust code while the returned reference exists. Callers must
    /// also guarantee that no mutable Rust reference is created to the
    /// referent while the returned reference exists.
    unsafe fn as_ref(&self) -> &T {
        &*self.as_ptr()
    }
}

/// A C++ non-const reference. These are different from Rust's `&mut T` in that
/// several C++ references can exist to the same underlying data ("aliasing")
/// and that's not permitted in Rust.
///
/// This is a trait not a struct due to the nuances of Rust's orphan rule
/// - implemntations of this trait are found in each set of generated bindings
/// but they are essentially the same.
pub trait CppMutRef<'a, T>: CppRef<'a, T> {
    /// Retrieve the underlying C++ pointer.
    fn as_mut_ptr(&self) -> *mut T;

    /// Get a regular Rust mutable reference out of this C++ reference.
    ///
    /// # Safety
    ///
    /// Callers must guarantee that the referent is not modified by any other
    /// C++ or Rust code while the returned reference exists. Callers must
    /// also guarantee that no other Rust reference is created to the referent
    /// while the returned reference exists.
    unsafe fn as_mut(&mut self) -> &mut T {
        &mut *self.as_mut_ptr()
    }
}

/// Any newtype wrapper which causes the contained object to obey C++ reference
/// semantics rather than Rust reference semantics.
///
/// The complex generics here are working around the orphan rule - the only
/// important generic is `T` which is the underlying stored type.
///
/// C++ references are permitted to alias one another, and commonly do.
/// Rust references must alias according only to the narrow rules of the
/// borrow checker.
///
/// If you need C++ to access your Rust object, first imprison it in one of these
/// objects, then use [`Self::as_cpp_ref`] to obtain C++ references to it.
pub trait CppPin<'a, T: 'a> {
    /// The type of C++ reference created to the contained object.
    type CppRef: CppRef<'a, T>;

    /// The type of C++ mutable reference created to the contained object..
    type CppMutRef: CppMutRef<'a, T>;

    /// Get an immutable pointer to the underlying object.
    fn as_ptr(&self) -> *const T;

    /// Get a mutable pointer to the underlying object.
    fn as_mut_ptr(&mut self) -> *mut T;

    /// Returns a reference which obeys C++ reference semantics
    fn as_cpp_ref(&self) -> Self::CppRef;

    /// Returns a mutable reference which obeys C++ reference semantics.
    ///
    /// Note that this requires unique ownership of `self`, but this is
    /// advisory since the resulting reference can be cloned.
    fn as_cpp_mut_ref(&mut self) -> Self::CppMutRef;

    /// Get a normal Rust reference to the underlying object. This is unsafe.
    ///
    /// # Safety
    ///
    /// You must guarantee that C++ will not mutate the object while the
    /// reference exists.
    unsafe fn as_ref(&self) -> &T {
        &*self.as_ptr()
    }

    /// Get a normal Rust mutable reference to the underlying object. This is unsafe.
    ///
    /// # Safety
    ///
    /// You must guarantee that C++ will not mutate the object while the
    /// reference exists.
    unsafe fn as_mut(&mut self) -> &mut T {
        &mut *self.as_mut_ptr()
    }
}
