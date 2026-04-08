//! Injective type-level functions


use crate::TypeFn;

use core::marker::PhantomData;

/// An  [injective]  type-level function
/// 
/// This trait is implemented automatically when both
/// [`TypeFn`] and [`RevTypeFn`] are implemented, and the function is [injective].
/// `InjTypeFn` cannot be manually implemented.
/// 
/// # Properties
/// 
/// These are properties about `InjTypeFn` that users can rely on.
/// 
/// For any given `F: InjTypeFn<A> + InjTypeFn<B>` these hold:
/// 
/// 1. If `A == B`, then `CallInjFn<F, A> == CallInjFn<F, B>`.
/// 2. If `CallInjFn<F, A> == CallInjFn<F, B>`, then `A == B`.
/// 3. If `A != B`, then `CallInjFn<F, A> != CallInjFn<F, B>`.
/// 4. If `CallInjFn<F, A> != CallInjFn<F, B>`, then `A != B`.
/// 
/// 
/// # Examples
/// 
/// ### Macro-based Implementation
/// 
/// ```rust
/// use typewit::{CallInjFn, UncallFn, inj_type_fn};
/// 
/// let _: CallInjFn<BoxFn, u32> = Box::new(3u32);
/// let _: UncallFn<BoxFn, Box<u32>> = 3u32;
/// 
/// inj_type_fn!{
///     struct BoxFn;
/// 
///     impl<T: ?Sized> T => Box<T>
/// }
/// ```
/// 
/// ### Non-macro Implementation
/// 
/// ```rust
/// use typewit::{CallInjFn, RevTypeFn, TypeFn, UncallFn};
/// 
/// let _: CallInjFn<BoxFn, u32> = Box::new(3u32);
/// let _: UncallFn<BoxFn, Box<u32>> = 3u32;
/// 
/// 
/// struct BoxFn;
///
/// impl<T: ?Sized> TypeFn<T> for BoxFn {
///     type Output = Box<T>;
/// 
///     // Asserts that this impl of `TypeFn` for `BoxFn` is injective.
///     const TYPE_FN_ASSERTS: () = { let _: CallInjFn<Self, T>; };
/// }
/// 
/// impl<T: ?Sized> RevTypeFn<Box<T>> for BoxFn {
///     type Arg = T;
/// }
/// 
/// ```
/// 
/// [injective]: mod@crate::type_fn#injective
#[cfg_attr(feature = "rust_1_83", diagnostic::on_unimplemented(
    message = "{Self} is not an injective type-level function over `{A}`"
))]
pub trait InjTypeFn<A: ?Sized>: TypeFn<A, Output = Self::Ret> + RevTypeFn<Self::Ret, Arg = A> {
    /// Return value of the function
    type Ret: ?Sized;
}

impl<F, A: ?Sized, R: ?Sized> InjTypeFn<A> for F
where
    F: TypeFn<A, Output = R>,
    F: RevTypeFn<R, Arg = A>,
{
    type Ret = R;
}

/// The inverse of [`TypeFn`], 
/// for getting the argument of a [`TypeFn`](crate::type_fn::TypeFn)
/// from its return value.
/// 
/// # Properties
/// 
/// These are properties about `RevTypeFn` that users can rely on.
/// 
/// For any given `F: RevTypeFn<R> + RevTypeFn<O>` these hold:
/// 
/// 1. If `R == O`, then `UncallFn<F, R> == UncallFn<F, O>`
/// 
/// 2. If `R != O`, then `UncallFn<F, R> != UncallFn<F, O>`
/// 
/// Disclaimer: this trait **does not** by itself ensure that a function is 
/// [injective],
/// since `RevTypeFn<Ret>` can't know if `Self::Arg` is the only argument 
/// that could produce `Ret`.
/// 
/// # Examples
/// 
/// ### Macro-based impl
/// 
/// ```rust
/// use std::ops::Range;
///
/// use typewit::{RevTypeFn, UncallFn};
///
/// let array = [3usize, 5];
///
/// // Getting the argument of `ArrayFn` from its return value
/// let value: UncallFn<ArrayFn<2>, [usize; 2]> = array[0];
///
/// assert_eq!(value, 3usize);
///
/// typewit::inj_type_fn!{
///     struct ArrayFn<const N: usize>;
///     impl<T> T => [T; N]
/// }
/// ```
/// 
/// ### Manual impl
/// 
/// ```rust
/// use std::ops::Range;
///
/// use typewit::{CallInjFn, RevTypeFn, TypeFn, UncallFn};
///
/// let array = [3usize, 5];
///
/// // Getting the argument of `ArrayFn` from its return value
/// let value: UncallFn<ArrayFn<2>, [usize; 2]> = array[0];
///
/// assert_eq!(value, 3usize);
///
/// struct ArrayFn<const N: usize>;
///
/// impl<T, const N: usize> TypeFn<T> for ArrayFn<N> {
///     type Output = [T; N];
///
///     // Ensures that this impl of `TypeFn` for `ArrayFn` is injective.
///     const TYPE_FN_ASSERTS: () = { let _: CallInjFn<Self, T>; };
/// }
/// impl<T, const N: usize> RevTypeFn<[T; N]> for ArrayFn<N> {
///     type Arg = T;
/// }
/// ```
/// 
/// ### Non-injective function
/// 
/// As mentioned above, this trait doesn't make a function [injective].
/// 
/// In the example below, `NonInjective` isn't injective, because it maps different 
/// arguments to the same return value:
/// 
/// ```rust
/// use typewit::{CallFn, RevTypeFn, TypeFn, UncallFn};
/// 
/// let _: CallFn<NonInjective, Vec<u8>> = 3u8;
/// let _: CallFn<NonInjective, String> = 5u8;
/// 
/// let _: UncallFn<NonInjective, u8> = ();
/// 
/// 
/// struct NonInjective;
/// 
/// impl<T> TypeFn<T> for NonInjective {
///     type Output = u8;
/// }
/// 
/// impl RevTypeFn<u8> for NonInjective {
///     type Arg = ();
/// }
/// ```
/// 
/// [injective]: mod@crate::type_fn#injective
#[cfg_attr(feature = "rust_1_83", diagnostic::on_unimplemented(
    message = "{Self} does not have an reverse type-level function from `{Ret}`",
    note = "consider declaring `{Self}` with the `typewit::inj_type_fn` macro",
))]
pub trait RevTypeFn<Ret: ?Sized>: TypeFn<Self::Arg, Output = Ret> {
    /// The argument to this function with `Ret` as the return value.
    type Arg: ?Sized;
}

/// Queries the argument to a `F: `[`TypeFn`] from its return value.
/// 
/// # Example
/// 
/// ```rust
/// use typewit::UncallFn;
/// 
/// let vect = vec![3u32, 5, 8];
/// let value: UncallFn<VecFn, Vec<u32>> = vect[1];
/// assert_eq!(value, 5u32);
/// 
/// typewit::inj_type_fn!{
///     struct VecFn;
///     impl<T> T => Vec<T>
/// }
/// ```
pub type UncallFn<F, Ret> = <F as RevTypeFn<Ret>>::Arg;


/// [`CallFn`](crate::CallFn) with an additional `F:`[`InjTypeFn<A>`] requirement,
/// which helps with type inference.
///
/// # Example
///
/// ```rust
/// use typewit::{InjTypeFn, CallInjFn};
/// 
/// // inferred return type
/// let inferred_ret = upcast(3u8);
/// assert_eq!(inferred_ret, 3);
/// 
/// // inferred argument type
/// let inferred_arg: u32 = upcast(5);
/// assert_eq!(inferred_arg, 5);
/// 
/// // Because the return type is `CallInjFn<_, I>`,
/// // this can infer `I` from the return type,
/// fn upcast<I>(int: I) -> CallInjFn<Upcast, I>
/// where
///     Upcast: InjTypeFn<I>,
///     CallInjFn<Upcast, I>: From<I>,
/// {
///     int.into()
/// }
/// 
/// 
/// typewit::inj_type_fn!{
///     struct Upcast;
///     
///     impl u8 => u16;
///     impl u16 => u32;
///     impl u32 => u64;
///     impl u64 => u128;
/// }
/// ```
/// 
/// As of October 2023, replacing `CallInjFn` with `CallFn` can cause type inference errors:
/// 
/// ```text
/// error[E0277]: the trait bound `Upcast: TypeFn<{integer}>` is not satisfied
///   --> src/type_fn/injective.rs:132:32
///    |
/// 11 | let inferred_arg: u32 = upcast(5);
///    |                         ------ ^ the trait `TypeFn<{integer}>` is not implemented for `Upcast`
///    |                         |
///    |                         required by a bound introduced by this call
///    |
///    = help: the following other types implement trait `TypeFn<T>`:
///              <Upcast as TypeFn<u16>>
///              <Upcast as TypeFn<u32>>
///              <Upcast as TypeFn<u64>>
///              <Upcast as TypeFn<u8>>
/// ```
pub type CallInjFn<F, A> = <F as InjTypeFn<A>>::Ret;


macro_rules! simple_inj_type_fn {
    (
        impl[$($impl:tt)*] ($arg:ty => $ret:ty) for $func:ty
        $(where[$($where:tt)*])?
    ) => {
        impl<$($impl)*> crate::type_fn::TypeFn<$arg> for $func
        $(where $($where)*)?
        {
            type Output = $ret;
        }

        impl<$($impl)*> crate::type_fn::RevTypeFn<$ret> for $func
        $(where $($where)*)?
        {
            type Arg = $arg;
        }
    };
} pub(crate) use simple_inj_type_fn;


////////////////////////////////////////////////////////////////////////////////

/// Reverses an [`InjTypeFn`], its arguments become return values, 
/// and its return values become arguments.
/// 
/// # Examples
///
/// ### Permutations
///
/// The different ways this function can be combined with [`CallFn`] and 
/// [`UncallFn`] 
///
/// ```rust
/// use typewit::type_fn::{CallFn, FnRev, UncallFn};
/// 
/// let _: CallFn<FnRev<Swap>, Right> = Left;
/// let _: UncallFn<    Swap,  Right> = Left;
/// 
/// let _: CallFn<        Swap,  Up> = Down;
/// let _: UncallFn<FnRev<Swap>, Up> = Down;
/// 
/// typewit::inj_type_fn!{
///     struct Swap;
/// 
///     impl Left => Right;
///     impl Up   => Down;
/// }
/// 
/// struct Left;
/// struct Right;
/// struct Up;
/// struct Down;
/// ```
/// 
/// [`CallFn`]: crate::CallFn
pub struct FnRev<F: ?Sized>(PhantomData<fn() -> F>);


impl<F: ?Sized> FnRev<F> {
    /// Constructs a `FnRev`.
    pub const NEW: Self = Self(PhantomData);
}

impl<F> FnRev<F> {
    /// Constructs a `FnRev` from `&F`
    pub const fn from_ref(_f: &F) -> Self {
        Self::NEW
    }
}

impl<F, A: ?Sized> TypeFn<A> for FnRev<F>
where
    F: RevTypeFn<A>
{
    type Output = UncallFn<F, A>;
}

impl<F, R: ?Sized> RevTypeFn<R> for FnRev<F>
where
    F: InjTypeFn<R>
{
    type Arg = CallInjFn<F, R>;
}

#[test]
fn test_fnrev_equivalence(){
    fn _foo<A, F: InjTypeFn<A>>() {
        let _ = crate::TypeEq::<CallInjFn<FnRev<F>, F::Ret>, UncallFn<F, F::Ret>>::NEW;
        
        let _ = crate::TypeEq::<UncallFn<FnRev<F>, A>, CallInjFn<F, A>>::NEW;
    }
}
