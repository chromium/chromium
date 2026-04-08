//! Type-level functions.
//! 
//! Type-level functions come in two flavors: 
//! [injective](#injective), and [non-injective](#non-injective)
//! 
//! 
//! # Injective
//! 
//! An injective function is any function `f` for which `a != b` implies `f(a) != f(b)`.
//! <br>(For both injective and non-injective functions, `f(a) != f(b)` implies `a != b`)
//! 
//! The [`InjTypeFn`] trait encodes injective type-level functions,
//! requiring the type to implement both [`TypeFn`] and [`RevTypeFn`].
//! 
//! 
//! ### Example: injective function
//!
//! ```rust
//! # use typewit::CallInjFn;
//! #
//! typewit::inj_type_fn!{
//!     struct Upcast;
//!     
//!     impl u8 => u16;
//!     impl u16 => u32;
//!     impl u32 => u64;
//!     impl u64 => u128;
//! }
//! let _: CallInjFn<Upcast, u8> = 3u16;
//! let _: CallInjFn<Upcast, u16> = 5u32;
//! ```
//! 
//! Because `Upcast` is injective, 
//! it is possible to query the argument from the returned value:
//! 
//! ```rust
//! # use typewit::UncallFn;
//! #
//! let _: UncallFn<Upcast, u16> = 3u8;
//! let _: UncallFn<Upcast, u128> = 5u64;
//! # 
//! # typewit::inj_type_fn!{
//! #     struct Upcast;
//! #     
//! #     impl u8 => u16;
//! #     impl u16 => u32;
//! #     impl u32 => u64;
//! #     impl u64 => u128;
//! # }
//! ```
//! 
//! # Non-injective
//! 
//! The [`TypeFn`] trait allows implementors to be non-injective.
//!
//! ### Example: non-injective function
//!
//! ```rust
//! typewit::type_fn!{
//!     struct Bar;
//!     
//!     impl<T> Vec<T> => T;
//!     impl<T> Box<T> => T;
//! }
//! ```
//! `Bar` is *non*-injective because it maps both `Vec<T>` and `Box<T>` to `T`.
//! 
//! 
//! [`TypeFn`]: crate::type_fn::TypeFn
//! [`CallFn`]: crate::type_fn::CallFn
//! 

use core::marker::PhantomData;

mod injective;

pub use self::injective::*;

pub(crate) use self::injective::simple_inj_type_fn;

#[doc(no_inline)]
pub use crate::inj_type_fn;

#[doc(no_inline)]
pub use crate::type_fn;


/// A function that operates purely on the level of types.
/// 
/// These can be used in `typewit` to 
/// [map the type arguments of `TypeEq`](crate::TypeEq::project).
/// 
/// Type-level functions can also be declared with the 
/// [`type_fn`](macro@crate::type_fn) macro.
/// 
/// # Properties
/// 
/// These are properties about `TypeFn` implementors that users can rely on.
/// 
/// For any given `F: TypeFn<A> + TypeFn<B>` these hold:
/// 
/// 1. If `A == B`, then `CallFn<F, A> == CallFn<F, B>`.
/// 2. If `CallFn<F, A> != CallFn<F, B>`, then `A != B`. 
/// 
/// # Examples
/// 
/// ### Manual Implementation
/// 
/// ```rust
/// use typewit::{TypeFn, CallFn};
/// 
/// let string: CallFn<AddOutput<String>, &str> = "foo".to_string() +  ", bar";
/// let _: String = string;
/// assert_eq!(string, "foo, bar");
/// 
/// 
/// struct AddOutput<Lhs>(core::marker::PhantomData<Lhs>);
/// 
/// // This part is optional,
/// // only necessary to pass the function as a value, not just as a type.
/// impl<Lhs> AddOutput<Lhs> {
///     const NEW: Self = Self(core::marker::PhantomData);
/// }
/// 
/// impl<Lhs, Rhs> TypeFn<Rhs> for AddOutput<Lhs>
/// where
///     Lhs: core::ops::Add<Rhs>
/// {
///     type Output = Lhs::Output;
/// }
/// ```
/// 
/// ### Macro-based Implementation
/// 
/// This example uses the [`type_fn`](macro@crate::type_fn) macro
/// to declare the type-level function,
/// and is otherwise equivalent to the manual one.
/// 
/// ```rust
/// use typewit::CallFn;
/// 
/// let string: CallFn<AddOutput<String>, &str> = "foo".to_string() +  ", bar";
/// let _: String = string;
/// assert_eq!(string, "foo, bar");
/// 
/// typewit::type_fn! {
///     struct AddOutput<Lhs>;
/// 
///     impl<Rhs> Rhs => Lhs::Output
///     where Lhs: core::ops::Add<Rhs>
/// }
/// ```
/// 
#[cfg_attr(feature = "rust_1_83", diagnostic::on_unimplemented(
    message = "{Self} is not a type-level function over `{T}`",
))]
pub trait TypeFn<T: ?Sized> {
    /// The return value of the function
    type Output: ?Sized;

    /// Helper constant for adding asserts in the `TypeFn` impl;
    const TYPE_FN_ASSERTS: () = ();
}

/// Calls the `F` [type-level function](TypeFn) with `T` as its argument.
/// 
/// For `F:`[`InjTypeFn<T>`](crate::InjTypeFn), it's better to 
/// use [`CallInjFn`] instead of this type alias.
/// 
/// 
/// # Example
/// 
/// ```rust
/// use typewit::CallFn;
/// use core::ops::Mul;
/// 
/// assert_eq!(mul(3u8, &5u8), 15u8);
/// 
/// fn mul<L, R>(l: L, r: R) -> CallFn<MulOutput<L>, R> 
/// where
///     L: core::ops::Mul<R>
/// {
///     l * r
/// }
/// 
/// // Declares `struct MulOutput<Lhs>`,
/// // a type-level function from `Rhs` to the return type of `Lhs * Rhs`.
/// typewit::type_fn! {
///     struct MulOutput<Lhs>;
///
///     impl<Rhs> Rhs => <Lhs as Mul<Rhs>>::Output
///     where Lhs: core::ops::Mul<Rhs>
/// }
/// ```
/// 
pub type CallFn<F, T> = <F as TypeFn<T>>::Output;

///////////////////////////////////////////////////////

/// Type-level function from `T` to `&'a T`
pub struct GRef<'a>(PhantomData<fn() -> &'a ()>);

impl<'a> GRef<'a> {
    /// Make a value of this type-level function
    pub const NEW: Self = Self(PhantomData);
}

simple_inj_type_fn!{
    impl['a, T: 'a + ?Sized] (T => &'a T) for GRef<'a>
}

////////////////

/// Type-level function from `T` to `&'a mut T`
pub struct GRefMut<'a>(PhantomData<fn() -> &'a mut ()>);

impl<'a> GRefMut<'a> {
    /// Make a value of this type-level function
    pub const NEW: Self = Self(PhantomData);
}

simple_inj_type_fn!{
    impl['a, T: 'a + ?Sized] (T => &'a mut T) for GRefMut<'a>
}

////////////////

/// Type-level function from `T` to `Box<T>`
#[cfg(feature = "alloc")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "alloc")))]
pub struct GBox;

#[cfg(feature = "alloc")]
simple_inj_type_fn!{
    impl[T: ?Sized] (T => alloc::boxed::Box<T>) for GBox
}

////////////////

/// Type-level identity function
pub struct FnIdentity;

simple_inj_type_fn!{
    impl[T: ?Sized] (T => T) for FnIdentity
}

////////////////

/// Type-level function which implements `TypeFn` by delegating to `F` 
/// 
/// This is mostly a workaround to write `F: TypeFn<T>` bounds in Rust 1.57.0
/// (trait bounds in `const fn`s were stabilized in Rust 1.61.0).
///
/// Because `Foo<F>: Trait`-style bounds unintentionally work in 1.57.0,
/// this crate uses `Invoke<F>: TypeFn<T>` 
/// when the `"rust_1_61"` feature is disabled,
/// and `F: TypeFn<T>` when it is enabled.
/// 
pub struct Invoke<F>(PhantomData<fn() -> F>);

impl<F> Copy for Invoke<F> {}

impl<F> Clone for Invoke<F> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<F> Invoke<F> {
    /// Constructs an `Invoke`
    pub const NEW: Self = Self(PhantomData);
}


impl<F, T: ?Sized> TypeFn<T> for Invoke<F> 
where
    F: TypeFn<T>
{
    type Output = CallFn<F, T>;
}

impl<F, R: ?Sized> RevTypeFn<R> for Invoke<F> 
where
    F: RevTypeFn<R>,
{
    type Arg = UncallFn<F, R>;
}


////////////////////////////////////////////////////////////////////////////////

impl<F, T: ?Sized> TypeFn<T> for PhantomData<F> 
where
    F: TypeFn<T>
{
    type Output = CallFn<F, T>;
}

impl<F, R: ?Sized> RevTypeFn<R> for PhantomData<F> 
where
    F: RevTypeFn<R>,
{
    type Arg = UncallFn<F, R>;
}



////////////////////////////////////////////////////////////////////////////////


mod uses_const_marker {
    use crate::const_marker::Usize;

    /// TypeFn from `(T, Usize<N>)` to `[T; N]`
    pub(crate) struct PairToArrayFn;

    super::simple_inj_type_fn!{
        impl[T, const N: usize] ((T, Usize<N>) => [T; N]) for PairToArrayFn
    }
} 

pub(crate) use uses_const_marker::*;



// This type alias makes it so that docs for newer Rust versions don't
// show `Invoke<F>`, keeping the method bounds the same as in 1.0.0.
#[cfg(not(feature = "rust_1_61"))]
pub(crate) type InvokeAlias<F> = Invoke<F>;

#[cfg(feature = "rust_1_61")]
pub(crate) type InvokeAlias<F> = F;