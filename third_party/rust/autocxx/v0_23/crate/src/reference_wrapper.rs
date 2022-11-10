// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{marker::PhantomData, pin::Pin};

use cxx::{memory::UniquePtrTarget, UniquePtr};

/// A C++ const reference. These are different from Rust's `&T` in that
/// these may exist even while the object is mutated elsewhere. See also
/// [`CppMutRef`] for the mutable equivalent.
///
/// The key rule is: we *never* dereference these in Rust. Therefore, any
/// UB here cannot manifest within Rust, but only across in C++, and therefore
/// they are equivalently safe to using C++ references in pure-C++ codebases.
///
/// # Usage
///
/// These types of references are pretty useless in Rust. You can't do
/// field access. But, you can pass them back into C++! And specifically,
/// you can call methods on them (i.e. use this type as a `this`). So
/// the common case here is when C++ gives you a reference to some type,
/// then you want to call methods on that reference.
///
/// # Calling methods
///
/// As noted, one of the main reasons for this type is to call methods.
/// Unfortunately, that depends on unstable Rust features. If you can't
/// call methods on one of these references, check you're using nightly.
///
/// # Lifetimes and cloneability
///
/// Although these references implement C++ aliasing semantics, they
/// do attempt to give you Rust lifetime tracking. This means if a C++ object
/// gives you a reference, you won't be able to use that reference after the
/// C++ object is no longer around.
///
/// This is usually what you need, since a C++ object will typically give
/// you a reference to part of _itself_ or something that it owns. But,
/// if you know that the returned reference lasts longer than its vendor,
/// you can use `lifetime_cast` to get a long-lived version.
///
/// On the other hand, these references do not give you Rust's exclusivity
/// guarantees. These references can be freely cloned, and using [`CppRef::const_cast`]
/// you can even make a mutable reference from an immutable reference.
///
/// # Field access
///
/// Field access would be achieved by adding C++ `get` and/or `set` methods.
///
/// # Implementation notes
///
/// Internally, this is represented as a raw pointer in Rust.
#[repr(transparent)]
pub struct CppRef<'a, T> {
    ptr: *const T,
    phantom: PhantomData<&'a T>,
}

impl<'a, T> CppRef<'a, T> {
    /// Retrieve the underlying C++ pointer.
    pub fn as_ptr(&self) -> *const T {
        self.ptr
    }

    /// Get a regular Rust reference out of this C++ reference.
    ///
    /// # Safety
    ///
    /// Callers must guarantee that the referent is not modified by any other
    /// C++ or Rust code while the returned reference exists. Callers must
    /// also guarantee that no mutable Rust reference is created to the
    /// referent while the returned reference exists.
    pub unsafe fn as_ref(&self) -> &T {
        &*self.as_ptr()
    }

    /// Create a C++ reference from a raw pointer.
    pub fn from_ptr(ptr: *const T) -> Self {
        Self {
            ptr,
            phantom: PhantomData,
        }
    }

    /// Create a mutable version of this reference, roughly equivalent
    /// to C++ `const_cast`.
    ///
    /// The opposite is to use [`AsCppRef::as_cpp_ref`] on a [`CppMutRef`]
    /// to obtain a [`CppRef`].
    ///
    /// # Safety
    ///
    /// Because we never dereference a `CppRef` in Rust, this cannot create
    /// undefined behavior _within Rust_ and is therefore not unsafe. It is
    /// however generally unwise, just as it is in C++. Use sparingly.
    pub fn const_cast(&self) -> CppMutRef<'a, T> {
        CppMutRef {
            ptr: self.ptr as *mut T,
            phantom: self.phantom,
        }
    }

    /// Extend the lifetime of the returned reference beyond normal Rust
    /// borrow checker rules.
    ///
    /// Normally, a reference can't be used beyond the lifetime of the object
    /// which gave it to you, but sometimes C++ APIs can return references
    /// to global or other longer-lived objects. In such a case you should
    /// use this method to get a longer-lived reference.
    ///
    /// # Usage
    ///
    /// When you're given a C++ reference and you know its referent is valid
    /// for a long time, use this method. Store the resulting `PhantomReferent`
    /// somewhere in Rust with an equivalent lifetime.
    /// That object can then vend longer-lived `CppRef`s using
    /// [`AsCppRef::as_cpp_ref`].
    ///
    /// # Safety
    ///
    /// Because `CppRef`s are never dereferenced in Rust, misuse of this API
    /// cannot lead to undefined behavior _in Rust_ and is therefore not
    /// unsafe. Nevertheless this can lead to UB in C++, so use carefully.
    pub fn lifetime_cast(&self) -> PhantomReferent<T> {
        PhantomReferent(self.ptr)
    }
}

impl<'a, T> core::ops::Deref for CppRef<'a, T> {
    type Target = T;
    #[inline]
    fn deref(&self) -> &T {
        // With `inline_const` we can simplify this to:
        // const { panic!("you shouldn't deref CppRef!") }
        struct C<T>(T);
        impl<T> C<T> {
            const V: core::convert::Infallible = panic!(
                "You cannot directly obtain a Rust reference from a CppRef. Use CppRef::as_ref."
            );
        }
        match C::<T>::V {}
    }
}

impl<'a, T> Clone for CppRef<'a, T> {
    fn clone(&self) -> Self {
        Self {
            ptr: self.ptr,
            phantom: self.phantom,
        }
    }
}

/// A C++ non-const reference. These are different from Rust's `&mut T` in that
/// several C++ references can exist to the same underlying data ("aliasing")
/// and that's not permitted for regular Rust references.
///
/// See [`CppRef`] for details on safety, usage models and implementation.
///
/// You can convert this to a [`CppRef`] using the [`std::convert::Into`] trait.
#[repr(transparent)]
pub struct CppMutRef<'a, T> {
    ptr: *mut T,
    phantom: PhantomData<&'a T>,
}

impl<'a, T> CppMutRef<'a, T> {
    /// Retrieve the underlying C++ pointer.
    pub fn as_mut_ptr(&self) -> *mut T {
        self.ptr
    }

    /// Get a regular Rust mutable reference out of this C++ reference.
    ///
    /// # Safety
    ///
    /// Callers must guarantee that the referent is not modified by any other
    /// C++ or Rust code while the returned reference exists. Callers must
    /// also guarantee that no other Rust reference is created to the referent
    /// while the returned reference exists.
    pub unsafe fn as_mut(&mut self) -> &mut T {
        &mut *self.as_mut_ptr()
    }

    /// Create a C++ reference from a raw pointer.
    pub fn from_ptr(ptr: *mut T) -> Self {
        Self {
            ptr,
            phantom: PhantomData,
        }
    }

    /// Extend the lifetime of the returned reference beyond normal Rust
    /// borrow checker rules. See [`CppRef::lifetime_cast`].
    pub fn lifetime_cast(&mut self) -> PhantomReferentMut<T> {
        PhantomReferentMut(self.ptr)
    }
}

impl<'a, T> core::ops::Deref for CppMutRef<'a, T> {
    type Target = T;
    #[inline]
    fn deref(&self) -> &T {
        // With `inline_const` we can simplify this to:
        // const { panic!("you shouldn't deref CppRef!") }
        struct C<T>(T);
        impl<T> C<T> {
            const V: core::convert::Infallible = panic!("You cannot directly obtain a Rust reference from a CppMutRef. Use CppMutRef::as_mut.");
        }
        match C::<T>::V {}
    }
}

impl<'a, T> Clone for CppMutRef<'a, T> {
    fn clone(&self) -> Self {
        Self {
            ptr: self.ptr,
            phantom: self.phantom,
        }
    }
}

impl<'a, T> From<CppMutRef<'a, T>> for CppRef<'a, T> {
    fn from(mutable: CppMutRef<'a, T>) -> Self {
        Self {
            ptr: mutable.ptr,
            phantom: mutable.phantom,
        }
    }
}

/// Any type which can return a C++ reference to its contents.
pub trait AsCppRef<T> {
    /// Returns a reference which obeys C++ reference semantics
    fn as_cpp_ref(&self) -> CppRef<T>;
}

/// Any type which can return a C++ reference to its contents.
pub trait AsCppMutRef<T>: AsCppRef<T> {
    /// Returns a mutable reference which obeys C++ reference semantics
    fn as_cpp_mut_ref(&mut self) -> CppMutRef<T>;
}

impl<'a, T> AsCppRef<T> for CppMutRef<'a, T> {
    fn as_cpp_ref(&self) -> CppRef<T> {
        CppRef::from_ptr(self.ptr)
    }
}

/// Workaround for the inability to use std::ptr::addr_of! on the contents
/// of a box.
#[repr(transparent)]
struct CppPinContents<T>(T);

impl<T> CppPinContents<T> {
    fn addr_of(&self) -> *const T {
        std::ptr::addr_of!(self.0)
    }
    fn addr_of_mut(&mut self) -> *mut T {
        std::ptr::addr_of_mut!(self.0)
    }
}

/// A newtype wrapper which causes the contained object to obey C++ reference
/// semantics rather than Rust reference semantics. That is, multiple aliasing
/// mutable C++ references may exist to the contents.
///
/// C++ references are permitted to alias one another, and commonly do.
/// Rust references must alias according only to the narrow rules of the
/// borrow checker.
///
/// If you need C++ to access your Rust object, first imprison it in one of these
/// objects, then use [`Self::as_cpp_ref`] to obtain C++ references to it.
/// If you need the object back for use in the Rust domain, use [`CppPin::extract`],
/// but be aware of the safety invariants that you - as a human - will need
/// to guarantee.
///
/// # Usage models
///
/// From fairly safe to fairly unsafe:
///
/// * *Configure a thing in Rust then give it to C++*. Take your Rust object,
///   set it up freely using Rust references, methods and data, then imprison
///   it in a `CppPin` and keep it around while you work with it in C++.
///   There is no possibility of _aliasing_ UB in this usage model, but you
///   still need to be careful of use-after-free bugs, just as if you were
///   to create a reference to any data in C++. The Rust borrow checker will
///   help you a little by ensuring that your `CppRef` objects don't outlive
///   the `CppPin`, but once those references pass into C++, it can't help.
/// * *Pass a thing to C++, have it operate on it synchronously, then take
///   it back*. To do this, you'd imprison your Rust object in a `CppPin`,
///   then pass mutable C++ references (using [`AsCppMutRef::as_cpp_mut_ref`])
///   into a C++ function. C++ would duly operate on the object, and thereafter
///   you could reclaim the object with `extract()`. At this point, you (as
///   a human) will need to give a guarantee that no references remain in the
///   C++ domain. If your object was just locally used by a single C++ function,
///   which has now returned, this type of local analysis may well be practical.
/// * *Share a thing between Rust and C++*. This object can vend both C++
///   references and Rust references (via `as_ref` etc.) It may be possible
///   for you to guarantee that C++ does not mutate the object while any Rust
///   reference exists. If you choose this model, you'll need to carefully
///   track exactly what happens to references and pointers on both sides,
///   and document your evidence for why you are sure this is safe.
///   Failure here is bad: Rust makes all sorts of optimization decisions based
///   upon its borrow checker guarantees, so mistakes can lead to undebuggable
///   action-at-a-distance crashes.
///
/// # See also
///
/// See also [`CppUniquePtrPin`], which is equivalent for data which is in
/// a [`cxx::UniquePtr`].
pub struct CppPin<T>(Box<CppPinContents<T>>);

impl<T> CppPin<T> {
    /// Imprison the Rust data within a `CppPin`. This eliminates any remaining
    /// Rust references (since we take the item by value) and this object
    /// subsequently only vends C++ style references, not Rust references,
    /// until or unless `extract` is called.
    pub fn new(item: T) -> Self {
        Self(Box::new(CppPinContents(item)))
    }

    /// Imprison the boxed Rust data within a `CppPin`. This eliminates any remaining
    /// Rust references (since we take the item by value) and this object
    /// subsequently only vends C++ style references, not Rust references,
    /// until or unless `extract` is called.
    ///
    /// If the item is already in a `Box`, this is slightly more efficient than
    /// `new` because it will avoid moving/reallocating it.
    pub fn from_box(item: Box<T>) -> Self {
        // Safety: CppPinContents<T> is #[repr(transparent)] so
        // this transmute from
        //   Box<T>
        // to
        //   Box<CppPinContents<T>>
        // is safe.
        let contents = unsafe { std::mem::transmute(item) };
        Self(contents)
    }

    /// Get an immutable pointer to the underlying object.
    pub fn as_ptr(&self) -> *const T {
        self.0.addr_of()
    }

    /// Get a mutable pointer to the underlying object.
    pub fn as_mut_ptr(&mut self) -> *mut T {
        self.0.addr_of_mut()
    }

    /// Get a normal Rust reference to the underlying object. This is unsafe.
    ///
    /// # Safety
    ///
    /// You must guarantee that C++ will not mutate the object while the
    /// reference exists.
    pub unsafe fn as_ref(&self) -> &T {
        &*self.as_ptr()
    }

    /// Get a normal Rust mutable reference to the underlying object. This is unsafe.
    ///
    /// # Safety
    ///
    /// You must guarantee that C++ will not mutate the object while the
    /// reference exists.
    pub unsafe fn as_mut(&mut self) -> &mut T {
        &mut *self.as_mut_ptr()
    }

    /// Extract the object from within its prison, for re-use again within
    /// the domain of normal Rust references.
    ///
    /// This returns a `Box<T>`: if you want the underlying `T` you can extract
    /// it using `*`.
    ///
    /// # Safety
    ///
    /// Callers promise that no remaining C++ references exist either
    /// in the form of Rust [`CppRef`]/[`CppMutRef`] or any remaining pointers/
    /// references within C++.
    pub unsafe fn extract(self) -> Box<T> {
        // Safety: CppPinContents<T> is #[repr(transparent)] so
        // this transmute from
        //   Box<CppPinContents<T>>
        // to
        //   Box<T>
        // is safe.
        std::mem::transmute(self.0)
    }
}

impl<T> AsCppRef<T> for CppPin<T> {
    fn as_cpp_ref(&self) -> CppRef<T> {
        CppRef::from_ptr(self.as_ptr())
    }
}

impl<T> AsCppMutRef<T> for CppPin<T> {
    fn as_cpp_mut_ref(&mut self) -> CppMutRef<T> {
        CppMutRef::from_ptr(self.as_mut_ptr())
    }
}

/// Any newtype wrapper which causes the contained [`UniquePtr`] target to obey C++ reference
/// semantics rather than Rust reference semantics. That is, multiple aliasing
/// mutable C++ references may exist to the contents.
///
/// C++ references are permitted to alias one another, and commonly do.
/// Rust references must alias according only to the narrow rules of the
/// borrow checker.
pub struct CppUniquePtrPin<T: UniquePtrTarget>(UniquePtr<T>);

impl<T: UniquePtrTarget> CppUniquePtrPin<T> {
    /// Imprison the type within a `CppPin`. This eliminates any remaining
    /// Rust references (since we take the item by value) and this object
    /// subsequently only vends C++ style references, not Rust references.
    pub fn new(item: UniquePtr<T>) -> Self {
        Self(item)
    }

    /// Get an immutable pointer to the underlying object.
    pub fn as_ptr(&self) -> *const T {
        // TODO - avoid brief reference here
        self.0
            .as_ref()
            .expect("UniquePtr was null; we can't make a C++ reference")
    }
}

impl<T: UniquePtrTarget> AsCppRef<T> for CppUniquePtrPin<T> {
    fn as_cpp_ref(&self) -> CppRef<T> {
        CppRef::from_ptr(self.as_ptr())
    }
}

impl<T: UniquePtrTarget> AsCppMutRef<T> for CppUniquePtrPin<T> {
    fn as_cpp_mut_ref(&mut self) -> CppMutRef<T> {
        let pinnned_ref: Pin<&mut T> = self
            .0
            .as_mut()
            .expect("UniquePtr was null; we can't make a C++ reference");
        let ptr = unsafe { Pin::into_inner_unchecked(pinnned_ref) };
        CppMutRef::from_ptr(ptr)
    }
}

/// A structure used to extend the lifetime of a returned C++ reference,
/// to indicate to Rust that it's beyond the normal Rust lifetime rules.
/// See [`CppRef::lifetime_cast`].
#[repr(transparent)]
pub struct PhantomReferent<T>(*const T);

impl<T> AsCppRef<T> for PhantomReferent<T> {
    fn as_cpp_ref(&self) -> CppRef<T> {
        CppRef::from_ptr(self.0)
    }
}

/// A structure used to extend the lifetime of a returned C++ mutable reference,
/// to indicate to Rust that it's beyond the normal Rust lifetime rules.
/// See [`CppRef::lifetime_cast`].
#[repr(transparent)]
pub struct PhantomReferentMut<T>(*mut T);

impl<T> AsCppRef<T> for PhantomReferentMut<T> {
    fn as_cpp_ref(&self) -> CppRef<T> {
        CppRef::from_ptr(self.0)
    }
}

impl<T> AsCppMutRef<T> for PhantomReferentMut<T> {
    fn as_cpp_mut_ref(&mut self) -> CppMutRef<T> {
        CppMutRef::from_ptr(self.0)
    }
}

#[cfg(all(feature = "arbitrary_self_types", test))]
mod tests {
    use super::*;

    struct CppOuter {
        _a: u32,
        inner: CppInner,
        global: *const CppInner,
    }

    impl CppOuter {
        fn get_inner_ref<'a>(self: &CppRef<'a, CppOuter>) -> CppRef<'a, CppInner> {
            // Safety: emulating C++ code for test purposes. This is safe
            // because we know the data isn't modified during the lifetime of
            // the returned reference.
            let self_rust_ref = unsafe { self.as_ref() };
            CppRef::from_ptr(std::ptr::addr_of!(self_rust_ref.inner))
        }
        fn get_global_ref<'a>(self: &CppRef<'a, CppOuter>) -> CppRef<'a, CppInner> {
            // Safety: emulating C++ code for test purposes. This is safe
            // because we know the data isn't modified during the lifetime of
            // the returned reference.
            let self_rust_ref = unsafe { self.as_ref() };
            CppRef::from_ptr(self_rust_ref.global)
        }
    }

    struct CppInner {
        b: u32,
    }

    impl CppInner {
        fn value_is(self: &CppRef<Self>) -> u32 {
            // Safety: emulating C++ code for test purposes. This is safe
            // because we know the data isn't modified during the lifetime of
            // the returned reference.
            let self_rust_ref = unsafe { self.as_ref() };
            self_rust_ref.b
        }
    }

    #[test]
    fn cpp_objects() {
        let mut global = CppInner { b: 7 };
        let global_ref_lifetime_phantom;
        {
            let outer = CppOuter {
                _a: 12,
                inner: CppInner { b: 3 },
                global: &mut global,
            };
            let outer = CppPin::new(outer);
            let inner_ref = outer.as_cpp_ref().get_inner_ref();
            assert_eq!(inner_ref.value_is(), 3);
            global_ref_lifetime_phantom = Some(outer.as_cpp_ref().get_global_ref().lifetime_cast());
        }
        let global_ref = global_ref_lifetime_phantom.unwrap();
        let global_ref = global_ref.as_cpp_ref();
        assert_eq!(global_ref.value_is(), 7);
    }

    #[test]
    fn cpp_pin() {
        let a = RustThing { _a: 4 };
        let a = CppPin::new(a);
        let _ = a.as_cpp_ref();
        let _ = a.as_cpp_ref();
    }
}
