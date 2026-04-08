#[allow(unused_imports)]
use crate::TypeEq;

use core::marker::PhantomData;


/// Gets a [type witness](crate#what-are-type-witnesses) for `Self`.
/// 
#[doc = explain_type_witness!()]
/// 
/// This trait is a helper to write [`W: MakeTypeWitness<Arg = T>`](MakeTypeWitness) 
/// with the `T` and `W` type parameters flipped,
/// most useful in supertrait bounds.
/// 
/// This trait can't be implemented outside of `typewit`.
/// 
/// # Example 
/// 
/// This example shows how one can make a `const fn` that converts both 
/// `&str` and `&[u8]` to `&str`
/// 
/// (this example requires Rust 1.64.0)
/// 
#[cfg_attr(not(feature = "rust_stable"), doc = "```ignore")]
#[cfg_attr(feature = "rust_stable", doc = "```rust")]
/// use typewit::{HasTypeWitness, TypeWitnessTypeArg, MakeTypeWitness, TypeEq};
/// 
/// fn main() {
///     assert_eq!(str_try_from("hello"), Ok("hello"));
///     
///     assert_eq!(str_try_from(&[b'w', b'o', b'r', b'l', b'd']), Ok("world"));
///     
///     assert_eq!(str_try_from(b"foo bar" as &[_]), Ok("foo bar"));
/// }
/// 
/// pub const fn str_try_from<'a, T, const L: usize>(
///     input: T
/// ) -> Result<&'a str, std::str::Utf8Error>
/// where
///     T: StrTryFrom<'a, L>
/// {
///     // `T::WITNESS` expands to 
///     // `<T as HasTypeWitness<StrTryFromWitness<'a, L, T>>::WITNESS`
///     match T::WITNESS {
///         StrTryFromWitness::Str(te) => {
///             // `te` (a `TypeEq<T, &'a str>`) allows coercing between `T` and `&'a str`.
///             let string: &str = te.to_right(input);
///             Ok(string)
///         }
///         StrTryFromWitness::Bytes(te) => {
///             let bytes: &[u8] = te.to_right(input);
///             std::str::from_utf8(bytes)
///         }
///         StrTryFromWitness::Array(te) => {
///             let slice: &[u8] = te.to_right(input);
///             str_try_from(slice)
///         }
///     }
/// }
/// 
/// 
/// // trait alias pattern
/// pub trait StrTryFrom<'a, const L: usize>: 
///     Copy + HasTypeWitness<StrTryFromWitness<'a, L, Self>> 
/// {}
/// 
/// impl<'a, T, const L: usize> StrTryFrom<'a, L> for T
/// where
///     T: Copy + HasTypeWitness<StrTryFromWitness<'a, L, T>>
/// {}
/// 
/// // This macro declares a type witness enum
/// typewit::simple_type_witness! {
///     // Declares `enum StrTryFromWitness<'a, const L: usize, __Wit>` 
///     // (the `__Wit` type parameter is implicitly added after all generics)
///     // `#[non_exhausitve]` allows adding more supported types.
///     #[non_exhaustive]
///     pub enum StrTryFromWitness<'a, const L: usize> {
///         // This variant requires `__Wit == &'a str`
///         // 
///         // The `<'a, 0>` here changes this macro from generating
///         // `impl<'a, const L: usize> MakeTypeWitness for StrTryFromWitness<'a, L, &'a [u8]>`
///         // to 
///         // `impl<'a> MakeTypeWitness for StrTryFromWitness<'a, 0, &'a [u8]>`
///         // which allows the compiler to infer generic arguments when
///         // using the latter `MakeTypeWitness` impl`
///         Str<'a, 0> = &'a str,
///    
///         // This variant requires `__Wit == &'a [u8]`
///         Bytes<'a, 0> = &'a [u8],
///    
///         // This variant requires `__Wit == &'a [u8; L]`
///         Array = &'a [u8; L],
///     }
/// }
/// ```
pub trait HasTypeWitness<W: TypeWitnessTypeArg<Arg = Self>> {
    /// A constant of the type witness
    const WITNESS: W;

    // prevents dependencies from implementing this trait 
    #[doc(hidden)]
    const __PRIV_KO9Y329U2U: priv_::__Priv<Self, W>;
}

impl<T, W> HasTypeWitness<W> for T
where
    T: ?Sized,
    W: MakeTypeWitness<Arg = T>,
{
    const WITNESS: W = W::MAKE;

    #[doc(hidden)]
    const __PRIV_KO9Y329U2U: priv_::__Priv<Self, W> = priv_::__Priv(PhantomData, PhantomData);
}

mod priv_ {
    use core::marker::PhantomData;

    #[doc(hidden)]
    pub struct __Priv<T: ?Sized, W>(
        pub(super) PhantomData<fn() -> PhantomData<W>>,
        pub(super) PhantomData<fn() -> PhantomData<T>>,
    );
}



////////////////////////////////////////////////

/// Gets the type argument that this [type witness](crate#what-are-type-witnesses) witnesses.
/// 
/// [**example shared with `MakeTypeWitness`**](MakeTypeWitness#example)
/// 
#[doc = explain_type_witness!()]
/// 
/// This trait should be implemented generically, 
/// as generic as the type definition of the implementor,
/// doing so will help type inference.
/// 
pub trait TypeWitnessTypeArg {
    /// The type parameter used for type witnesses.
    ///
    /// Usually, enums that implement this trait have
    /// variants with [`TypeEq`]`<`[`Self::Arg`]`, SomeType>` fields.
    type Arg: ?Sized;
}

/// Constructs this [type witness](crate#what-are-type-witnesses).
/// 
#[doc = explain_type_witness!()]
/// 
/// This trait can be automatically implemented for simple type witnesses
/// by declaring the type witness with the [`simple_type_witness`] macro.
/// 
/// # Example
/// 
/// (this example requires Rust 1.61.0)
#[cfg_attr(not(feature = "rust_1_61"), doc = "```ignore")]
#[cfg_attr(feature = "rust_1_61", doc = "```rust")]
/// use typewit::{TypeWitnessTypeArg, MakeTypeWitness, TypeEq};
/// 
/// const fn default<'a, T, const L: usize>() -> T 
/// where
///     Defaultable<'a, L, T>: MakeTypeWitness
/// {
///     match MakeTypeWitness::MAKE {
///         // `te` is a `TypeEq<T, i32>`, which allows coercing between `T` and `i32`.
///         // `te.to_left(...)` goes from `i32` to `T`.
///         Defaultable::I32(te) => te.to_left(3),
///
///         // `te` is a `TypeEq<T, bool>`
///         Defaultable::Bool(te) => te.to_left(true),
///
///         // `te` is a `TypeEq<T, &'a str>`
///         Defaultable::Str(te) => te.to_left("empty"),
///
///         // `te` is a `TypeEq<T, [u32; L]>`
///         Defaultable::Array(te) => te.to_left([5; L]),
///     }
/// }
/// 
/// let number: i32 = default();
/// assert_eq!(number, 3);
/// 
/// let boolean: bool = default();
/// assert_eq!(boolean, true);
/// 
/// let string: &str = default();
/// assert_eq!(string, "empty");
///
/// let array: [u32; 3] = default();
/// assert_eq!(array, [5, 5, 5]);
/// 
/// 
/// // This enum is a type witness (documented in the root module)
/// #[non_exhaustive]
/// enum Defaultable<'a, const L: usize, T> {
///     // This variant requires `T == i32`
///     I32(TypeEq<T, i32>),
///
///     // This variant requires `T == bool`
///     Bool(TypeEq<T, bool>),
///
///     // This variant requires `T == &'a str`
///     Str(TypeEq<T, &'a str>),
///
///     // This variant requires `T == [u32; L]`
///     Array(TypeEq<T, [u32; L]>),
/// }
/// 
/// impl<T, const L: usize> TypeWitnessTypeArg for Defaultable<'_, L, T> {
///     // this aids type inference for what type parameter is witnessed 
///     type Arg = T;
/// }
/// 
/// // Specifying dummy values for the generics that the `I32` variant doesn't use,
/// // so that they don't have to be specified when this impl is used.
/// impl MakeTypeWitness for Defaultable<'_, 0, i32> {
///     // The `TypeEq<T, i32>` field can be constructed because `T == i32` here.
///     const MAKE: Self = Self::I32(TypeEq::NEW);
/// }
/// 
/// impl MakeTypeWitness for Defaultable<'_, 0, bool> {
///     const MAKE: Self = Self::Bool(TypeEq::NEW);
/// }
/// 
/// impl<'a> MakeTypeWitness for Defaultable<'a, 0, &'a str> {
///     const MAKE: Self = Self::Str(TypeEq::NEW);
/// }
/// 
/// impl<const L: usize> MakeTypeWitness for Defaultable<'_, L, [u32; L]> {
///     const MAKE: Self = Self::Array(TypeEq::NEW);
/// }
/// 
/// ```
/// 
/// The `Defaultable` type definition and its impls can also be written using 
/// the [`simple_type_witness`] macro:
#[cfg_attr(not(feature = "rust_1_61"), doc = "```ignore")]
#[cfg_attr(feature = "rust_1_61", doc = "```rust")]
/// typewit::simple_type_witness!{
///     // Declares `enum Defaultable<'a, const L: usize, __Wit>`
///     // The `__Wit` type parameter is implicit and always the last generic parameter.
///     #[non_exhaustive]
///     enum Defaultable<'a, const L: usize> {
///         // `<'a, 0>` is necessary to have 
///         // `impl MakeTypeWitness for Defaultable<'_, 0, i32>` instead of 
///         // `impl<'a, const L: u32> MakeTypeWitness for Defaultable<'a, L, i32>`,
///         // which allows the generic arguments to be inferred.
///         I32<'a, 0> = i32,
///
///         Bool<'a, 0> = bool,
///
///         Str<'a, 0> = &'a str,
///
///         Array = [u32; L],
///     }
/// }
/// ```
/// note that [`simple_type_witness`] can't replace enums whose 
/// witnessed type parameter is not the last, 
/// or have variants with anything but one [`TypeEq`] field each.
/// 
/// 
/// [`simple_type_witness`]: crate::simple_type_witness
pub trait MakeTypeWitness: TypeWitnessTypeArg {
    /// A constant with the type witness
    const MAKE: Self;
}
