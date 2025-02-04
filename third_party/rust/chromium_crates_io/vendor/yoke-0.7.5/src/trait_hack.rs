// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Workarounds for adding trait bounds to `yoke` objects.
//!
//! # Trait bounds in Yoke
//!
//! [Compiler bug #89196](https://github.com/rust-lang/rust/issues/89196) makes it tricky to add
//! trait bounds involving `yoke` types.
//!
//! For example, you may want to write:
//!
//! `where for<'a> <Y as Yokeable<'a>>::Output: MyTrait`
//!
//! The above trait bound will compile, but at call sites, you get errors such as:
//!
//! > the trait `for<'de> MyTrait` is not implemented for `<Y as Yokeable<'de>>::Output`
//!
//! There are two known workarounds:
//!
//! 1. If the trait is well-defined on references, like `Debug`, bind the trait to a reference:
//!     `where for<'a> &'a <Y as Yokeable<'a>>::Output: MyTrait`
//! 2. If the trait involves `Self`, like `Clone`, use [`YokeTraitHack`]:
//!     `where for<'a> YokeTraitHack<<Y as Yokeable<'a>>::Output>: MyTrait`
//!
//! # Examples
//!
//! Code that does not compile ([playground](https://play.rust-lang.org/?version=beta&mode=debug&edition=2018&gist=ebbda5b15a398d648bdff9e439b27dc0)):
//!
//! ```compile_fail
//! # this compiles in 1.78+, so this text will make it fail
//! use yoke::*;
//!
//! trait MiniDataMarker {
//!     type Yokeable: for<'a> Yokeable<'a>;
//! }
//!
//! struct MiniDataPayload<M>
//! where
//!     M: MiniDataMarker
//! {
//!     pub yoke: Yoke<M::Yokeable, ()>,
//! }
//!
//! impl<M> Clone for MiniDataPayload<M>
//! where
//!     M: MiniDataMarker,
//!     for<'a> <M::Yokeable as Yokeable<'a>>::Output: Clone,
//! {
//!     fn clone(&self) -> Self {
//!         unimplemented!()
//!     }
//! }
//!
//! trait MiniDataProvider<M>
//! where
//!     M: MiniDataMarker
//! {
//!     fn mini_load_data(&self) -> MiniDataPayload<M>;
//! }
//!
//! struct MiniStructProvider<M>
//! where
//!     M: MiniDataMarker,
//! {
//!     pub payload: MiniDataPayload<M>,
//! }
//!
//! impl<M> MiniDataProvider<M> for MiniStructProvider<M>
//! where
//!     M: MiniDataMarker,
//!     for<'a> <M::Yokeable as Yokeable<'a>>::Output: Clone,
//! {
//!     fn mini_load_data(&self) -> MiniDataPayload<M> {
//!         self.payload.clone()
//!     }
//! }
//!
//! #[derive(Clone)]
//! struct SimpleStruct(pub u32);
//!
//! unsafe impl<'a> Yokeable<'a> for SimpleStruct {
//!     // (not shown; see `Yokeable` for examples)
//! #    type Output = SimpleStruct;
//! #    fn transform(&'a self) -> &'a Self::Output {
//! #        self
//! #    }
//! #    fn transform_owned(self) -> Self::Output {
//! #        self
//! #    }
//! #    unsafe fn make(from: Self::Output) -> Self {
//! #        std::mem::transmute(from)
//! #    }
//! #    fn transform_mut<F>(&'a mut self, f: F)
//! #    where
//! #        F: 'static + for<'b> FnOnce(&'b mut Self::Output),
//! #    {
//! #        unsafe {
//! #            f(std::mem::transmute::<&'a mut Self, &'a mut Self::Output>(
//! #                self,
//! #            ))
//! #        }
//! #    }
//! }
//!
//! impl MiniDataMarker for SimpleStruct {
//!     type DataStruct = SimpleStruct;
//! }
//!
//! let provider = MiniStructProvider {
//!     payload: MiniDataPayload {
//!         yoke: Yoke::new_always_owned(SimpleStruct(42))
//!     }
//! };
//!
//! // Broken:
//! // "method cannot be called on `MiniStructProvider<_>` due to unsatisfied trait bounds"
//! let payload: MiniDataPayload<SimpleStruct> = provider.mini_load_data();
//!
//! // Working:
//! let payload = MiniDataProvider::<SimpleStruct>::mini_load_data(&provider);
//!
//! assert_eq!(payload.yoke.get().0, 42);
//! ```
//!
//! Example for binding the trait to a reference:
//!
//! ```
//! use yoke::Yoke;
//! use yoke::Yokeable;
//!
//! // Example trait and struct for illustration purposes:
//! trait MyTrait {
//!     fn demo(&self) -> u32;
//! }
//! struct MyStruct(u32);
//! impl MyTrait for MyStruct {
//!     fn demo(&self) -> u32 {
//!         self.0
//!     }
//! }
//! unsafe impl<'a> Yokeable<'a> for MyStruct {
//!     // (not shown; see `Yokeable` for examples)
//! #    type Output = MyStruct;
//! #    fn transform(&'a self) -> &'a Self::Output {
//! #        self
//! #    }
//! #    fn transform_owned(self) -> Self::Output {
//! #        self
//! #    }
//! #    unsafe fn make(from: Self::Output) -> Self {
//! #        std::mem::transmute(from)
//! #    }
//! #    fn transform_mut<F>(&'a mut self, f: F)
//! #    where
//! #        F: 'static + for<'b> FnOnce(&'b mut Self::Output),
//! #    {
//! #        unsafe {
//! #            f(std::mem::transmute::<&'a mut Self, &'a mut Self::Output>(
//! #                self,
//! #            ))
//! #        }
//! #    }
//! }
//!
//! // The trait needs to be defined on references:
//! impl<'a, T> MyTrait for &'a T
//! where
//!     T: MyTrait,
//! {
//!     fn demo(&self) -> u32 {
//!         self.demo()
//!     }
//! }
//!
//! impl<Y, C> MyTrait for Yoke<Y, C>
//! where
//!     Y: for<'a> Yokeable<'a>,
//!     for<'a> &'a <Y as Yokeable<'a>>::Output: MyTrait,
//! {
//!     fn demo(&self) -> u32 {
//!         self.get().demo()
//!     }
//! }
//!
//! fn example() {
//!     let y = Yoke::<MyStruct, ()>::new_always_owned(MyStruct(42));
//!     let _: &dyn MyTrait = &y;
//! }
//! ```
//!
//! Example for using [`YokeTraitHack`]:
//!
//! ```
//! use std::rc::Rc;
//! use yoke::trait_hack::YokeTraitHack;
//! use yoke::Yoke;
//! use yoke::Yokeable;
//!
//! // Example trait and struct for illustration purposes:
//! trait MyTrait {
//!     fn demo(data: u32) -> Self;
//! }
//! struct MyStruct(u32);
//! impl MyTrait for MyStruct {
//!     fn demo(data: u32) -> Self {
//!         Self(data)
//!     }
//! }
//! unsafe impl<'a> Yokeable<'a> for MyStruct {
//!     // (not shown; see `Yokeable` for examples)
//! #    type Output = MyStruct;
//! #    fn transform(&'a self) -> &'a Self::Output {
//! #        self
//! #    }
//! #    fn transform_owned(self) -> Self::Output {
//! #        self
//! #    }
//! #    unsafe fn make(from: Self::Output) -> Self {
//! #        std::mem::transmute(from)
//! #    }
//! #    fn transform_mut<F>(&'a mut self, f: F)
//! #    where
//! #        F: 'static + for<'b> FnOnce(&'b mut Self::Output),
//! #    {
//! #        unsafe {
//! #            f(std::mem::transmute::<&'a mut Self, &'a mut Self::Output>(
//! #                self,
//! #            ))
//! #        }
//! #    }
//! }
//!
//! // The trait needs to be defined on YokeTraitHack:
//! impl<'a, T> MyTrait for YokeTraitHack<T>
//! where
//!     T: MyTrait,
//! {
//!     fn demo(data: u32) -> Self {
//!         YokeTraitHack(T::demo(data))
//!     }
//! }
//!
//! impl<Y> MyTrait for Yoke<Y, Rc<u32>>
//! where
//!     Y: for<'a> Yokeable<'a>,
//!     for<'a> YokeTraitHack<<Y as Yokeable<'a>>::Output>: MyTrait,
//! {
//!     fn demo(data: u32) -> Self {
//!         let rc_u32: Rc<u32> = Rc::new(data);
//!         Yoke::attach_to_cart(rc_u32, |u| {
//!             YokeTraitHack::<<Y as Yokeable>::Output>::demo(*u).0
//!         })
//!     }
//! }
//!
//! fn example() {
//!     let _ = Yoke::<MyStruct, Rc<u32>>::demo(42);
//! }
//! ```

use core::mem;

/// A wrapper around a type `T`, forwarding trait calls down to the inner type.
///
/// `YokeTraitHack` supports [`Clone`], [`PartialEq`], [`Eq`], and [`serde::Deserialize`] out of
/// the box. Other traits can be implemented by the caller.
///
/// For more information, see the module-level documentation.
///
/// # Example
///
/// Using `YokeTraitHack` as a type bound in a function comparing two `Yoke`s:
///
/// ```
/// use yoke::trait_hack::YokeTraitHack;
/// use yoke::*;
///
/// fn compare_yokes<Y, C1, C2>(y1: Yoke<Y, C1>, y2: Yoke<Y, C2>) -> bool
/// where
///     Y: for<'a> Yokeable<'a>,
///     for<'a> YokeTraitHack<<Y as Yokeable<'a>>::Output>: PartialEq,
/// {
///     YokeTraitHack(y1.get()).into_ref() == YokeTraitHack(y2.get()).into_ref()
/// }
/// ```
#[repr(transparent)]
#[derive(Clone, PartialEq, Eq, Debug)]
#[allow(clippy::exhaustive_structs)] // newtype
pub struct YokeTraitHack<T>(pub T);

impl<'a, T> YokeTraitHack<&'a T> {
    /// Converts from `YokeTraitHack<&T>` to `&YokeTraitHack<T>`.
    ///
    /// This is safe because `YokeTraitHack` is `repr(transparent)`.
    ///
    /// This method is required to implement `Clone` on `Yoke`.
    pub fn into_ref(self) -> &'a YokeTraitHack<T> {
        // Safety: YokeTraitHack is repr(transparent) so it's always safe
        // to transmute YTH<&T> to &YTH<T>
        unsafe { mem::transmute::<YokeTraitHack<&T>, &YokeTraitHack<T>>(self) }
    }
}

// This is implemented manually to avoid the serde derive dependency.
#[cfg(feature = "serde")]
impl<'de, T> serde::de::Deserialize<'de> for YokeTraitHack<T>
where
    T: serde::de::Deserialize<'de>,
{
    #[inline]
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::de::Deserializer<'de>,
    {
        T::deserialize(deserializer).map(YokeTraitHack)
    }
}
