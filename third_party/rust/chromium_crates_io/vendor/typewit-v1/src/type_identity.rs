use crate::TypeEq;

/// Emulation of `T == U` bounds.
/// 
/// This trait emulates `T == U` bounds with `T: Identity<Type = U>`.
/// 
/// # Projection
/// 
/// Because this trait uses [`TypeEq`] for casting between `Self` and [`Self::Type`],
/// you can transform the arguments of that `TypeEq` to cast any composition of those types,
/// e.g: cast between `Vec<Self>` and `Vec<Self::Type>`
/// 
/// # Example
/// 
/// ### Type Parameter Alias
/// 
/// (this example requires Rust 1.61.0, because it uses trait bounds in a `const fn`)
/// 
#[cfg_attr(not(feature = "rust_1_61"), doc = "```ignore")]
#[cfg_attr(feature = "rust_1_61", doc = "```rust")]
/// use typewit::{Identity, TypeEq};
/// 
/// assert_eq!(foo(3), [3, 3]);
/// 
/// assert_eq!(foo::<&str, 2, _>("hello"), ["hello", "hello"]);
///
///
/// const fn foo<T, const N: usize, R>(val: T) -> R
/// where
///     // emulates a `[T; N] == R` bound
///     [T; N]: Identity<Type = R>,
///     T: Copy,
/// {
///     Identity::TYPE_EQ // returns a `TypeEq<[T; N], R>`
///         .to_right([val; N]) // casts `[T; N]` to `R`
/// }
/// ```
/// 
/// ### Projection
/// 
/// Demonstrating that any projection of `Self` and `Self::Type` can 
/// be casted to each other.
/// 
/// ```rust
/// use typewit::{Identity, TypeEq, type_fn};
/// 
/// assert_eq!(make_vec::<u8>(), vec![3, 5, 8]);
/// 
/// fn make_vec<T>() -> Vec<T> 
/// where
///     T: Identity<Type = u8>
/// {
///     let te: TypeEq<Vec<T>, Vec<u8>> = T::TYPE_EQ.project::<VecFn>();
///     
///     te.to_left(vec![3, 5, 8]) // casts `Vec<u8>` to `Vec<T>`
/// }
/// 
/// type_fn!{
///     // A type-level function (TypeFn implementor) from `T` to `Vec<T>`
///     struct VecFn;
///     impl<T> T => Vec<T>
/// }
/// ```
/// 
pub trait Identity {
    /// The same type as `Self`,
    /// used to emulate type equality bounds (`T == U`)
    /// with associated type equality constraints
    /// (`T: Identity<Type = U>`).
    type Type: ?Sized;
    
    /// Proof that `Self` is the same type as `Self::Type`,
    /// provides methods for casting between `Self` and `Self::Type`.
    const TYPE_EQ: TypeEq<Self, Self::Type>;
}

impl<T: ?Sized> Identity for T {
    type Type = T;

    const TYPE_EQ: TypeEq<Self, Self::Type> = TypeEq::NEW;
}
