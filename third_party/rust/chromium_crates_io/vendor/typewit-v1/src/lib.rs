#![allow(clippy::needless_doctest_main)]
//! This crate provides abstractions for creating
//! [type witnesses](#what-are-type-witnesses).
//! 
//! The inciting motivation for this crate is emulating trait polymorphism in `const fn`
//! (as of 2025-07-20, it's not possible to call trait methods in const contexts on stable).
//! 
//! # What are type witnesses
//! 
//! Type witnesses are values that prove the equality of a type parameter to a
//! fixed set of possible types.
//! 
//! The simplest type witness is [`TypeEq<L, R>`][`TypeEq`],
//! which only proves equality of its `L` and `R` type parameters,
//! and can be used to coerce between them.
//! 
//! Most type witnesses are enums with [`TypeEq`] fields,
//! which can coerce between a type parameter and as many types as there are variants.
//! 
//! # Examples
//! 
//! <span id="example0"></span>
//! 
//! ### Polymorphic function
//! 
//! This demonstrates how one can write a polymorphic `const fn`
//! (as of 2025-07-20, trait methods can't be called in const fns on stable)
//! 
//! (this example requires Rust 1.61.0, since it uses trait bounds in const)
#![cfg_attr(not(feature = "rust_1_61"), doc = "```ignore")]
#![cfg_attr(feature = "rust_1_61", doc = "```rust")]
//! use typewit::{HasTypeWitness, TypeEq};
//! 
//! const VALS: [&str; 6] = [
//!     message(0),
//!     message(1),
//!     message(2),
//!     message(3),
//!     message("hi"),
//!     message("foo"),
//! ];
//! assert_eq!(VALS, ["A", "B", "C", "A", "hi", "foo"]);
//! 
//! 
//! // A "method" of the `Message` trait (declared below)
//! const fn message<'a, T: Message<'a>>(val: T) -> &'a str {
//!     match HasTypeWitness::WITNESS {
//!         MessageWitness::Usize(te) => {
//!             // `te` (a `TypeEq<T, usize>`) allows coercing between `T` and `usize`,
//!             // because `TypeEq` is a value-level proof that both types are the same.
//!             let index: usize = te.to_right(val);
//!             ["A", "B", "C"][index % 3]
//!         }
//!         MessageWitness::Str(te) => {
//!             // `te` is a `TypeEq<T, &'a str>`
//!             te.to_right(val)
//!         }
//!     }
//! }
//! 
//! // The trait that we use to emulate polymorphic dispatch,
//! // the limitation is that it can only emulate it for a limited set of types known
//! // to the crate that defines the trait, in this case that's `usize` and `&str`.
//! trait Message<'a>: HasTypeWitness<MessageWitness<'a, Self>> { }
//! 
//! // replacing these impls with a blanket impl leads to worse compilation errors
//! impl<'a> Message<'a> for usize {}
//! impl<'a> Message<'a> for &'a str {}
//! 
//! // This macro declares `enum MessageWitness<'a, __Wit>`, a type witness enum,
//! // where each variant requires and then guarantees `__Wit` to be a particular type.
//! // (the `__Wit` type parameter is implicitly added after all generics)
//! typewit::simple_type_witness! {
//!     enum MessageWitness<'a> {
//!         // This variant requires `__Wit == usize`
//!         Usize = usize,
//!    
//!         // This variant requires `__Wit == &'a str`
//!         Str = &'a str,
//!     }
//! }
//! ```
//! 
//! <span id="example-uses-type-fn"></span>
//! ### Indexing polymorphism
//! 
//! This function demonstrates const fn polymorphism
//! and projecting [`TypeEq`] by implementing [`TypeFn`].
//! 
//! (this example requires Rust 1.71.0, because it uses `<[T]>::split_at` in a const context.
#![cfg_attr(not(feature = "rust_stable"), doc = "```ignore")]
#![cfg_attr(feature = "rust_stable", doc = "```rust")]
//! use std::ops::Range;
//! 
//! use typewit::{HasTypeWitness, TypeEq};
//! 
//! fn main() {
//!     let array = [3, 5, 8, 13, 21, 34, 55, 89];
//! 
//!     assert_eq!(index(&array, 0), &3);
//!     assert_eq!(index(&array, 3), &13);
//!     assert_eq!(index(&array, 0..4), [3, 5, 8, 13]);
//!     assert_eq!(index(&array, 3..5), [13, 21]);
//! }
//! 
//! const fn index<T, I>(slice: &[T], idx: I) -> &SliceIndexRet<I, T>
//! where
//!     I: SliceIndex<T>,
//! {
//!     // `I::WITNESS` is `<I as HasTypeWitness<IndexWitness<I>>>::WITNESS`,
//!     match I::WITNESS {
//!         IndexWitness::Usize(arg_te) => {
//!             // `arg_te` (a `TypeEq<I, usize>`) allows coercing between `I` and `usize`,
//!             // because `TypeEq` is a value-level proof that both types are the same.
//!             let idx: usize = arg_te.to_right(idx);
//! 
//!             // using the `TypeFn` impl for `FnSliceIndexRet<T>` to 
//!             // map `TypeEq<I, usize>` 
//!             // to  `TypeEq<SliceIndexRet<I, T>, SliceIndexRet<usize, T>>`
//!             arg_te.project::<FnSliceIndexRet<T>>()
//!                 // converts`TypeEq<SliceIndexRet<I, T>, T>` 
//!                 //      to `TypeEq<&SliceIndexRet<I, T>, &T>`
//!                 .in_ref()
//!                 .to_left(&slice[idx])
//!         }
//!         IndexWitness::Range(arg_te) => {
//!             let range: Range<usize> = arg_te.to_right(idx);
//!             let ret: &[T] = slice_range(slice, range);
//!             arg_te.project::<FnSliceIndexRet<T>>().in_ref().to_left(ret)
//!         }
//!     }
//! }
//! 
//! // This macro declares a type witness enum
//! typewit::simple_type_witness! {
//!     // Declares `enum IndexWitness<__Wit>` 
//!     // (the `__Wit` type parameter is implicitly added after all generics)
//!     enum IndexWitness {
//!         // This variant requires `__Wit == usize`
//!         Usize = usize,
//!    
//!         // This variant requires `__Wit == Range<usize>`
//!         Range = Range<usize>,
//!     }
//! }
//! 
//! /// Trait for all types that can be used as slice indices
//! /// 
//! /// The `HasTypeWitness` supertrait allows getting a `IndexWitness<Self>`
//! /// with its `WITNESS` associated constant.
//! trait SliceIndex<T>: HasTypeWitness<IndexWitness<Self>> + Sized {
//!     type Returns: ?Sized;
//! }
//! impl<T> SliceIndex<T> for usize {
//!     type Returns = T;
//! }
//! impl<T> SliceIndex<T> for Range<usize> {
//!     type Returns = [T];
//! }
//! 
//! type SliceIndexRet<I, T> = <I as SliceIndex<T>>::Returns;
//! 
//! // Declares `struct FnSliceIndexRet<T>`
//! // a type-level function (TypeFn implementor) from `I` to `SliceIndexRet<I, T>`
//! typewit::type_fn! {
//!     struct FnSliceIndexRet<T>;
//!
//!     impl<I: SliceIndex<T>> I => SliceIndexRet<I, T>
//! }
//! 
//! const fn slice_range<T>(slice: &[T], range: Range<usize>) -> &[T] {
//!     let suffix = slice.split_at(range.start).1;
//!     suffix.split_at(range.end - range.start).0
//! }
//! 
//! ```
//! 
//! When the wrong type is passed for the index,
//! the compile-time error is the same as with normal generic functions:
//! ```text
//! error[E0277]: the trait bound `RangeFull: SliceIndex<{integer}>` is not satisfied
//!   --> src/main.rs:43:30
//!    |
//! 13 |     assert_eq!(index(&array, ..), [13, 21]);
//!    |                -----         ^^ the trait `SliceIndex<{integer}>` is not implemented for `RangeFull`
//!    |                |
//!    |                required by a bound introduced by this call
//!    |
//!    = help: the following other types implement trait `SliceIndex<T>`:
//!              std::ops::Range<usize>
//!              usize
//! ```
//! 
//! ### Downcasting const generic type
//! 
//! This example demonstrates "downcasting" from a type with a const parameter to 
//! a concrete instance of that type.
//! 
//! ```rust
//! use typewit::{const_marker::Usize, TypeCmp, TypeEq};
//! 
//! assert_eq!(*mutate(&mut Arr([])), Arr([]));
//! assert_eq!(*mutate(&mut Arr([1])), Arr([1]));
//! assert_eq!(*mutate(&mut Arr([1, 2])), Arr([1, 2]));
//! assert_eq!(*mutate(&mut Arr([1, 2, 3])), Arr([1, 3, 6])); // this is different!
//! assert_eq!(*mutate(&mut Arr([1, 2, 3, 4])), Arr([1, 2, 3, 4])); 
//! 
//! #[derive(Debug, PartialEq)]
//! struct Arr<const N: usize>([u8; N]);
//! 
//! fn mutate<const N: usize>(arr: &mut Arr<N>) -> &mut Arr<N> {
//!     if let TypeCmp::Eq(te) =  Usize::<N>.equals(Usize::<3>) {
//!         let tem = te // `te` is a `TypeEq<Usize<N>, Usize<3>>`
//!             .project::<GArr>() // returns `TypeEq<Arr<N>, Arr<3>>`
//!             .in_mut(); // returns `TypeEq<&mut Arr<N>, &mut Arr<3>>`
//! 
//!         // `tem.to_right(arr)` downcasts `arr` to `&mut Arr<3>`
//!         tetra_sum(tem.to_right(arr));
//!     }
//! 
//!     arr
//! }
//! 
//! fn tetra_sum(arr: &mut Arr<3>) {
//!     arr.0[1] += arr.0[0];
//!     arr.0[2] += arr.0[1];
//! }
//! 
//! // Declares `struct GArr`
//! // a type-level function (TypeFn implementor) from `Usize<N>` to `Arr<N>`
//! typewit::type_fn!{
//!     struct GArr;
//! 
//!     impl<const N: usize> Usize<N> => Arr<N>
//! }
//! ```
//! 
//! ### Builder
//! 
//! Using a type witness to help encode a type-level enum,
//! and to match on that type-level enum inside of a function.
//! 
//! The type-level enum is used to track the initialization of fields in a builder.
//! 
//! This example requires Rust 1.65.0, because it uses Generic Associated Types.
#![cfg_attr(not(feature = "rust_1_65"), doc = "```ignore")]
#![cfg_attr(feature = "rust_1_65", doc = "```rust")]
//! use typewit::HasTypeWitness;
//! 
//! fn main() {
//!     // all default fields
//!     assert_eq!(
//!         StructBuilder::new().build(), 
//!         Struct{foo: "default value".into(), bar: vec![3, 5, 8]},
//!     );
//! 
//!     // defaulted bar field
//!     assert_eq!(
//!         StructBuilder::new().foo("hello").build(), 
//!         Struct{foo: "hello".into(), bar: vec![3, 5, 8]},
//!     );
//! 
//!     // defaulted foo field
//!     assert_eq!(
//!         StructBuilder::new().bar([13, 21, 34]).build(), 
//!         Struct{foo: "default value".into(), bar: vec![13, 21, 34]},
//!     );
//! 
//!     // all initialized fields
//!     assert_eq!(
//!         StructBuilder::new().foo("world").bar([55, 89]).build(), 
//!         Struct{foo: "world".into(), bar: vec![55, 89]},
//!     );
//! }
//! 
//! 
//! #[derive(Debug, PartialEq, Eq)]
//! struct Struct {
//!     foo: String,
//!     bar: Vec<u32>,
//! }
//! 
//! struct StructBuilder<FooInit: InitState, BarInit: InitState> {
//!     // If `FooInit` is `Uninit`, then this field is a `()`
//!     // If `FooInit` is `Init`, then this field is a `String`
//!     foo: BuilderField<FooInit, String>,
//!
//!     // If `BarInit` is `Uninit`, then this field is a `()`
//!     // If `BarInit` is `Init`, then this field is a `Vec<u32>`
//!     bar: BuilderField<BarInit, Vec<u32>>,
//! }
//! 
//! impl StructBuilder<Uninit, Uninit> {
//!     pub const fn new() -> Self {
//!         Self {
//!             foo: (),
//!             bar: (),
//!         }
//!     }
//! }
//! 
//! impl<FooInit: InitState, BarInit: InitState> StructBuilder<FooInit, BarInit> {
//!     /// Sets the `foo` field
//!     pub fn foo(self, foo: impl Into<String>) -> StructBuilder<Init, BarInit> {
//!         StructBuilder {
//!             foo: foo.into(),
//!             bar: self.bar,
//!         }
//!     }
//!
//!     /// Sets the `bar` field
//!     pub fn bar(self, bar: impl Into<Vec<u32>>) -> StructBuilder<FooInit, Init> {
//!         StructBuilder {
//!             foo: self.foo,
//!             bar: bar.into(),
//!         }
//!     }
//! 
//!     /// Builds `Struct`, 
//!     /// providing default values for fields that haven't been set.
//!     pub fn build(self) -> Struct {
//!         Struct {
//!             foo: init_or_else::<FooInit, _, _>(self.foo, || "default value".to_string()),
//!             bar: init_or_else::<BarInit, _, _>(self.bar, || vec![3, 5, 8]),
//!         }
//!     }
//! }
//! 
//! // Emulates a type-level `enum InitState { Init, Uninit }`
//! trait InitState: Sized + HasTypeWitness<InitWit<Self>> {
//!     // How a builder represents an initialized/uninitialized field.
//!     // If `Self` is `Uninit`, then this is `()`.
//!     // If `Self` is `Init`, then this is `T`.
//!     type BuilderField<T>;
//! }
//! 
//! // If `I` is `Uninit`, then this evaluates to `()`
//! // If `I` is `Init`, then this evaluates to `T`
//! type BuilderField<I, T> = <I as InitState>::BuilderField::<T>;
//! 
//! /// Gets `T` out of `maybe_init` if it's actually initialized,
//! /// otherwise returns `else_()`.
//! fn init_or_else<I, T, F>(maybe_init: BuilderField<I, T>, else_: F) -> T
//! where
//!     I: InitState,
//!     F: FnOnce() -> T
//! {
//!     typewit::type_fn! {
//!         // Declares the `HelperFn` type-level function (TypeFn implementor)
//!         // from `I` to `BuilderField<I, T>`
//!         struct HelperFn<T>;
//!         impl<I: InitState> I => BuilderField<I, T>
//!     }
//! 
//!     // matching on the type-level `InitState` enum by using `InitWit`.
//!     // `WITNESS` comes from the `HasTypeWitness` trait
//!     match I::WITNESS {
//!         // `te: TypeEq<FooInit, Init>`
//!         InitWit::InitW(te) => {
//!             te.map(HelperFn::NEW) //: TypeEq<BuilderField<I, T>, T>
//!               .to_right(maybe_init)
//!         }
//!         InitWit::UninitW(_) => else_(),
//!     }
//! }
//! 
//! // Emulates a type-level `InitState::Init` variant.
//! // Marks a field as initialized.
//! enum Init {}
//! 
//! impl InitState for Init {
//!     type BuilderField<T> = T;
//! }
//! 
//! // Emulates a type-level `InitState::Uninit` variant.
//! // Marks a field as uninitialized.
//! enum Uninit {}
//! 
//! impl InitState for Uninit {
//!     type BuilderField<T> = ();
//! }
//! 
//! typewit::simple_type_witness! {
//!     // Declares `enum InitWit<__Wit>`, a type witness.
//!     // (the `__Wit` type parameter is implicitly added after all generics)
//!     enum InitWit {
//!         // This variant requires `__Wit == Init`
//!         InitW = Init,
//!         // This variant requires `__Wit == Uninit`
//!         UninitW = Uninit,
//!     }
//! }
//! ```
//! 
//! ### Generic Const Expressions
//! 
//! This example uses [`Usize`] to coerce an arrays whose length is generic to 
//! another generic, but equal, length.
//! 
//! This example requires the `"generic_const_exprs"` crate feature because it uses the
//! currently-unstable [`generic_const_exprs`] language feature.
#![cfg_attr(not(feature = "generic_const_exprs"), doc = "```ignore")]
#![cfg_attr(feature = "generic_const_exprs", doc = "```rust")]
//! #![feature(generic_const_exprs)]
//! 
//! use typewit::{const_marker::Usize, TypeCmp, TypeEq};
//! 
//! 
//! let mut arrays = Arrays::<1, 3> { a: [3, 5, 8], b: [13, 21, 34] };
//! 
//! arrays.swap_inner();
//! 
//! assert_eq!(arrays.a, [13, 21, 34]);
//! assert_eq!(arrays.b, [3, 5, 8]);
//! 
//! 
//! struct Arrays<const A: usize, const B: usize> 
//! where
//!     [u8; A * B]:, 
//!     [u8; B * A]:,
//! {
//!     a: [u8; A * B],
//!     b: [u8; B * A],
//! }
//! 
//! impl<const A: usize, const B: usize> Arrays<A, B> 
//! where
//!     [u8; A * B]:, 
//!     [u8; B * A]:,
//! {
//!     // Swaps the two array fields
//!     const fn swap_inner(&mut self) {
//!         let a = TypeEq::new::<u8>() // : TypeEq<u8, u8>
//!             .in_array(commutative_proof::<A, B>()) // : TypeEq<[u8; A * B], [u8; B * A]>
//!             .in_mut() // : TypeEq<&mut [u8; A * B], &mut [u8; B * A]>
//!             .to_right(
//!                 &mut self.a // : &mut [u8; A * B] 
//!             ); // : &mut [u8; B * A] 
//!         
//!         core::mem::swap(a, &mut self.b);
//!     }
//! }
//! 
//! const fn commutative_proof<const A: usize, const B: usize>(
//! ) -> TypeEq<Usize<{A * B}>, Usize<{B * A}>>
//! {
//!     // panic-safety: A * B == B * A always holds, so this `unwrap_eq` can never panic
//!     Usize::<{A * B}>.equals(Usize::<{B * A}>).unwrap_eq()
//! }
//! 
//! ```
//! 
//! If you tried to swap the fields directly, you'd get this error:
//! ```text
//! error[E0308]: mismatched types
//!   --> src/lib.rs:437:38
//!    |
//! 42 |         core::mem::swap(&mut self.a, &mut self.b);
//!    |                                      ^^^^^^^^^^^ expected `A * B`, found `B * A`
//!    |
//!    = note: expected constant `A * B`
//!               found constant `B * A`
//! ```
//! 
//! 
//! # Cargo features
//! 
//! These are the features of this crate.
//! 
//! ### Default-features
//! 
//! These features are enabled by default:
//! 
//! - `"proc_macros"`: uses proc macros to improve compile-errors involving 
//! macro-generated impls.
//! 
//! ### Rust-versions and standard crates
//! 
//! These features enable items that have a minimum Rust version:
//! 
//! - `"rust_stable"`: enables all the `"rust_1_*"` features.
//! 
//! - `"rust_1_83"`: turns functions that take mutable references into `const fn`s,
//! enables [`const_marker`] items for comparing [`ConstMarker`]s,
//! and enables the `"rust_1_65"` feature.
//! 
//! - `"rust_1_65"`: enables the [`type_constructors`] module,
//! the [`methods`] module,
//! and the `"rust_1_61"` feature.
//! 
//! - `"rust_1_61"`: enables [`MetaBaseTypeWit`],
//! [`BaseTypeWitness`],
//! and the `{TypeCmp, TypeNe}::{zip*, in_array}` methods.
//!
//! These features enable items that require a non-`core` standard crate:
//! 
//! - `"alloc"`: enable items that use anything from the standard `alloc` crate.
//! 
//! ### Nightly features
//! 
//! These features require the nightly Rust compiler:
//! 
//! - `"adt_const_marker"`:
//! enables the `"rust_stable"` crate feature,
//! and marker types in the [`const_marker`] module that have
//! non-primitive `const` parameters.
//! 
//! - `"generic_const_exprs"`:
//! enables the `"rust_stable"` crate feature,
//! and doc examples that use the [`generic_const_exprs`] unstable language feature.
//! 
//! # No-std support
//! 
//! `typewit` is `#![no_std]`, it can be used anywhere Rust can be used.
//! 
//! You need to enable the `"alloc"` feature to enable items that use anything 
//! from the standard `alloc` crate.
//! 
//! # Minimum Supported Rust Version
//! 
//! `typewit` supports Rust 1.57.0.
//! 
//! Features that require newer versions of Rust, or the nightly compiler,
//! need to be explicitly enabled with crate features.
//! 
//! 
//! 
//! [`TypeCmp`]: crate::TypeCmp
//! [`TypeEq`]: crate::TypeEq
//! [`TypeNe`]: crate::TypeNe
//! [`TypeFn`]: crate::type_fn::TypeFn
//! [`const_marker`]: crate::const_marker
//! [`type_constructors`]: crate::type_constructors
//! [`methods`]: crate::methods
//! [`MetaBaseTypeWit`]: crate::MetaBaseTypeWit
//! [`BaseTypeWitness`]:  crate::BaseTypeWitness
//! [`Usize`]: crate::const_marker::Usize
//! [`typewit::const_marker`]: crate::const_marker
//! [`ConstMarker`]: crate::const_marker::ConstMarker
//! [`generic_const_exprs`]: https://doc.rust-lang.org/unstable-book/language-features/generic-const-exprs.html
#![no_std]
#![cfg_attr(feature = "adt_const_marker", feature(adt_const_params))]
#![cfg_attr(feature = "adt_const_marker", feature(unsized_const_params))]
#![cfg_attr(feature = "adt_const_marker", allow(incomplete_features))]
#![cfg_attr(feature = "docsrs", feature(doc_cfg))]
#![allow(clippy::type_complexity)]
#![deny(missing_docs)]
#![deny(clippy::missing_const_for_fn)]
#![deny(unused_results)]

#[cfg(feature = "alloc")]
extern crate alloc;


// Documentation for concepts not specific to any one item
macro_rules! explain_type_witness {
    () => ("\
        A [type witness](crate#what-are-type-witnesses) is \
        an enum whose variants only have [`TypeEq`](crate::TypeEq) fields.
        Each variant requires the enum's type parameter to be a specific type.
    ")
}

#[macro_use]
pub mod type_fn;

pub mod const_marker;

#[cfg(feature = "adt_const_marker")]
mod all_init_bytes;

mod utils;
mod macros;

#[cfg(feature = "rust_1_61")]
mod base_type_wit;

#[cfg(feature = "rust_1_61")]
pub use crate::base_type_wit::{BaseTypeWitness, MetaBaseTypeWit};


#[cfg(feature = "rust_1_65")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_65")))]
pub mod methods;


#[cfg(feature = "rust_1_61")]
pub(crate) mod some_type_arg_is_ne;

#[cfg(feature = "rust_1_61")]
pub(crate) use self::some_type_arg_is_ne::SomeTypeArgIsNe;


mod type_cmp;
mod type_eq;
mod type_eq_ne_guts;
mod type_identity;

mod type_ne_;

/// [`TypeNe`]-related items
pub mod type_ne {
    pub use crate::type_ne_::{LeftArg, RightArg};
    
    #[doc(no_inline)]
    pub use crate::{TypeNe, type_ne};
}


mod type_witness_traits;

#[cfg(feature = "rust_1_65")]
pub mod type_constructors;


#[doc(inline)]
pub use crate::{
    type_eq::*,
    type_ne_::TypeNe,
    type_witness_traits::*,
    type_identity::Identity,
};


pub use crate::type_cmp::TypeCmp;

#[doc(no_inline)]
pub use crate::type_fn::{CallFn, CallInjFn, InjTypeFn, RevTypeFn, TypeFn, UncallFn};


#[cfg(feature = "proc_macros")]
#[doc(hidden)]
pub use typewit_proc_macros::__impl_with_span;

/// tests doc lints with `cargo doc --features="__test_doc_lints rust_stable"`
#[cfg(all(feature = "__test_doc_lints"))]
pub mod test_doc_lints;

#[doc(hidden)]
pub mod __ {
    pub use core::{
        clone::Clone,
        cmp::{PartialEq, Eq, PartialOrd, Ord, Ordering},
        fmt::{Debug, Formatter, Result as FmtResult},
        hash::{Hash, Hasher},
        marker::{Copy, PhantomData},
        mem::{ManuallyDrop, discriminant},
        option::Option,
        primitive::{bool, usize},
        assert, compile_error, concat, stringify,
    };

    pub use crate::{
        type_identity::Identity,
        macros::{
            generics_parsing::{
                __parse_generic_args_with_defaults,
                __parse_in_generics,
                __parse_ty_bounds,
                __parse_where_clause_for_item_inner,
                __pg_cfg_expansion,
                __pg_parsed_ty_bounds,
            },
            simple_type_witness_macro::__stw_parse_variants,
        },
    };

}



#[cfg(all(doctest, feature = "generic_const_exprs"))]
#[doc = include_str!("../README.md")]
pub struct ReadmeTest;
