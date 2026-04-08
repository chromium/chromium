use crate::{
    type_fn::{self, CallFn, InvokeAlias, RevTypeFn, TypeFn, UncallFn}, 
    MakeTypeWitness, TypeWitnessTypeArg,
};

use crate::const_marker::Usize;



use core::{
    cmp::{Ordering, Eq, Ord, PartialEq, PartialOrd},
    default::Default,
    hash::{Hash, Hasher},
    fmt::{self, Debug},
};

#[cfg(feature = "alloc")]
use alloc::boxed::Box;

crate::type_eq_ne_guts::declare_helpers!{
    $
    TypeEq
    TypeFn
    CallFn
}

crate::type_eq_ne_guts::declare_zip_helper!{
    $ TypeEq
}

// Equivalent to `type_eq.zip(other_type_eq).project::<Func>()`,
// defined to ensure that methods which do zip+project have 0 overhead in debug builds.
macro_rules! zip_project {
    // Since `$L0`, `$L1`,`$R0`, and `$R1` are all used only once,
    // it's safe to declare them as `:ty` (safe against malicious type macros).
    (
        $left_type_eq:expr,
        $right_type_eq:expr,
        $F: ty,
        ($L0:ty, $R0:ty),
        ($L1:ty, $R1:ty),
    ) => ({
        __ZipProjectVars::<$F, $L0, $R0, $L1, $R1> {
            left_te: $left_type_eq,
            right_te: $right_type_eq,
            projected_te: {
                // SAFETY: 
                // `TypeEq<$L0, $R0>` and `TypeEq<$L1, $R1>` 
                // implies `TypeEq<($L0, $L1), ($R0, $R1)>`,
                // 
                // Using `$F` only once, as a type argument,
                // to protect against type-position macros that expand to 
                // different types on each use.
                unsafe {
                    TypeEq::new_unchecked()
                }
            }
        }.projected_te
    });
}

struct __ZipProjectVars<F, L0, R0, L1, R1> 
where
    F: TypeFn<(L0, L1)> + TypeFn<(R0, R1)>
{
    #[allow(dead_code)]
    left_te: TypeEq<L0, R0>,

    #[allow(dead_code)]
    right_te: TypeEq<L1, R1>,

    //         (TypeEq<L0, R0>, TypeEq<L1, R1>) 
    // implies TypeEq<(L0, L1), (R0, R1)> 
    // implies TypeEq<CallFn<F, (L0, L1)>, CallFn<F, (R0, R1)>>
    projected_te: TypeEq<CallFn<F, (L0, L1)>, CallFn<F, (R0, R1)>>,
}


/// Constructs a [`TypeEq<T, T>`](TypeEq)
/// 
/// # Example
/// 
/// ```rust
/// use typewit::{MakeTypeWitness, TypeWitnessTypeArg, TypeEq, type_eq};
/// 
/// assert_eq!(ascii_to_upper(b'a'), b'A');
/// assert_eq!(ascii_to_upper(b'f'), b'F');
/// assert_eq!(ascii_to_upper(b'B'), b'B');
/// assert_eq!(ascii_to_upper(b'0'), b'0');
/// 
/// assert_eq!(ascii_to_upper('c'), 'C');
/// assert_eq!(ascii_to_upper('e'), 'E');
/// assert_eq!(ascii_to_upper('H'), 'H');
/// assert_eq!(ascii_to_upper('@'), '@');
/// 
/// const fn ascii_to_upper<T>(c: T) -> T 
/// where
///     Wit<T>: MakeTypeWitness,
/// {
///     match MakeTypeWitness::MAKE {
///         Wit::U8(te) => {
///             // `te` is a `TypeEq<T, u8>`, which allows casting between `T` and `u8`.
///             // `te.to_right(...)` goes from `T` to `u8`
///             // `te.to_left(...)` goes from `u8` to `T`
///             te.to_left(te.to_right(c).to_ascii_uppercase())
///         }
///         Wit::Char(te) => {
///             // `te` is a `TypeEq<T, char>`, which allows casting between `T` and `char`.
///             // `te.to_right(...)` goes from `T` to `char`
///             // `te.to_left(...)` goes from `char` to `T`
///             te.to_left(te.to_right(c).to_ascii_uppercase())
///         }
///     }
/// }
/// 
/// // This is a type witness
/// enum Wit<T> {
///     // this variant requires `T == u8`
///     U8(TypeEq<T, u8>),
/// 
///     // this variant requires `T == char`
///     Char(TypeEq<T, char>),
/// }
/// impl<T> TypeWitnessTypeArg for Wit<T> {
///     type Arg = T;
/// }
/// impl MakeTypeWitness for Wit<u8> {
///     const MAKE: Self = Self::U8(type_eq());
/// }
/// impl MakeTypeWitness for Wit<char> {
///     const MAKE: Self = Self::Char(type_eq());
/// }
/// ```
/// The code above can be written more concisly using 
/// the [`polymatch`](crate::polymatch) and [`simple_type_witness`] macros:
/// ```rust
/// # use typewit::{MakeTypeWitness, TypeWitnessTypeArg, TypeEq, type_eq};
/// # 
/// # assert_eq!(ascii_to_upper(b'a'), b'A');
/// # assert_eq!(ascii_to_upper(b'f'), b'F');
/// # assert_eq!(ascii_to_upper(b'B'), b'B');
/// # assert_eq!(ascii_to_upper(b'0'), b'0');
/// # 
/// # assert_eq!(ascii_to_upper('c'), 'C');
/// # assert_eq!(ascii_to_upper('e'), 'E');
/// # assert_eq!(ascii_to_upper('H'), 'H');
/// # assert_eq!(ascii_to_upper('@'), '@');
/// # 
/// const fn ascii_to_upper<T>(c: T) -> T 
/// where
///     Wit<T>: MakeTypeWitness,
/// {
///     // deduplicating identical match arms using the `polymatch` macro.
///     typewit::polymatch!{MakeTypeWitness::MAKE;
///         Wit::U8(te) | Wit::Char(te) => te.to_left(te.to_right(c).to_ascii_uppercase())
///     }
/// }
///
/// // This macro declares a type witness
/// typewit::simple_type_witness! {
///     // Declares `enum Wit<__Wit>`
///     // The `__Wit` type parameter is implicit and always the last generic parameter.
///     enum Wit {
///         // this variant requires `__Wit == u8`
///         U8 = u8,
///         // this variant requires `__Wit == char`
///         Char = char,
///     }
/// }
/// ```
/// note that [`simple_type_witness`] can't replace enums whose 
/// witnessed type parameter is not the last, 
/// or have variants with anything but one `TypeEq` field each.
/// 
/// [`simple_type_witness`]: crate::simple_type_witness
#[inline(always)]
pub const fn type_eq<T: ?Sized>() -> TypeEq<T, T> {
    TypeEq::NEW
}


// Declaring `TypeEq` in a submodule to prevent "safely" constructing `TypeEq` with
// two different type arguments in the `crate::type_eq` module.
mod type_eq_ {
    use core::{
        any::{Any, TypeId},
        marker::PhantomData,
    };

    /// Value-level proof that `L` is the same type as `R`
    ///
    /// This type can be used to prove that `L` and `R` are the same type,
    /// because it can only be safely constructed with 
    /// [`TypeEq::<L, L>::NEW`](#associatedconstant.NEW)(or [`new`](#method.new)),
    /// where both type arguments are the same type.
    ///
    /// This type is not too useful by itself, it becomes useful 
    /// [when put inside of an enum](#polymorphic-branching).
    ///
    /// 
    /// `TypeEq<L, R>` uses the `L` type parameter as the more generic type by convention
    /// (e.g: `TypeEq<T, char>`).
    /// This only matters if you're using the type witness traits 
    /// ([`HasTypeWitness`](crate::HasTypeWitness), 
    /// [`MakeTypeWitness`](crate::MakeTypeWitness), 
    ///  [`TypeWitnessTypeArg`](crate::TypeWitnessTypeArg)) with `TypeEq`.
    ///
    /// # Soundness
    /// 
    /// `TypeEq<L, R>` requires both type arguments to be the same type so that 
    /// [projecting](Self::project) the type arguments results in the same type for 
    /// both arguments.
    /// 
    /// Unsafely creating a `TypeEq<L, R>` where `L != R` allows
    /// [transmuting between any two types](#arbitrary-transmute)
    /// (that is bad).
    ///
    /// # Examples
    /// 
    /// ### Polymorphic branching
    /// 
    /// This example demonstrates how type witnesses can be used to 
    /// choose between expressions of different types with a constant. 
    /// 
    /// ```rust
    /// use typewit::TypeEq;
    /// 
    /// const fn main() {
    ///     assert!(matches!(choose!(0; b"a string", 2, panic!()), b"a string"));
    /// 
    ///     const UNO: u64 = 1;
    ///     assert!(matches!(choose!(UNO; loop{}, [3, 5], true), [3, 5]));
    /// 
    ///     assert!(matches!(choose!(2 + 3; (), unreachable!(), ['5', '3']), ['5', '3']));
    /// }
    /// 
    /// /// Evaluates the argument at position `$chosen % 3`, other arguments aren't evaluated.
    /// /// 
    /// /// The arguments can all be different types.
    /// /// 
    /// /// `$chosen` must be a `u64` constant.
    /// #[macro_export]
    /// macro_rules! choose {
    ///     ($chosen:expr; $arg_0: expr, $arg_1: expr, $arg_2: expr) => {
    ///         match Choice::<{$chosen % 3}>::VAL {
    ///             // `te` (a `TypeEq<T, X>`) allows us to safely go between 
    ///             // the type that the match returns (its `T` type argument)
    ///             // and the type of `$arg_0` (its `X` type argument).
    ///             Branch3::A(te) => {
    ///                 // `to_left` goes from `X` to `T`
    ///                 te.to_left($arg_0)
    ///             }
    ///             // same as the `A` branch, with a different type for the argument
    ///             Branch3::B(te) => te.to_left($arg_1),
    ///             // same as the `A` branch, with a different type for the argument
    ///             Branch3::C(te) => te.to_left($arg_2),
    ///         }
    ///     }
    /// }
    /// 
    /// // This is a type witness
    /// pub enum Branch3<T, X, Y, Z> {
    ///     // This variant requires `T == X`
    ///     A(TypeEq<T, X>),
    /// 
    ///     // This variant requires `T == Y`
    ///     B(TypeEq<T, Y>),
    /// 
    ///     // This variant requires `T == Z`
    ///     C(TypeEq<T, Z>),
    /// }
    /// 
    /// // Used to get different values of `Branch3` depending on `N`
    /// pub trait Choice<const N: u64> {
    ///     const VAL: Self;
    /// }
    /// 
    /// impl<X, Y, Z> Choice<0> for Branch3<X, X, Y, Z> {
    ///     // Because the first two type arguments of `Branch3` are `X`
    ///     // (as required by the `TypeEq<T, X>` field in Branch3's type definition),
    ///     // we can use `TypeEq::NEW` here.
    ///     const VAL: Self = Self::A(TypeEq::NEW);
    /// }
    /// 
    /// impl<X, Y, Z> Choice<1> for Branch3<Y, X, Y, Z> {
    ///     const VAL: Self = Self::B(TypeEq::NEW);
    /// }
    /// 
    /// impl<X, Y, Z> Choice<2> for Branch3<Z, X, Y, Z> {
    ///     const VAL: Self = Self::C(TypeEq::NEW);
    /// }
    /// 
    /// ```
    /// 
    pub struct TypeEq<L: ?Sized, R: ?Sized>(PhantomData<TypeEqHelper<L, R>>);

    // Declared to work around this error in old Rust versions:
    // > error[E0658]: function pointers cannot appear in constant functions
    struct TypeEqHelper<L: ?Sized, R: ?Sized>(
        #[allow(dead_code)]
        fn(PhantomData<L>) -> PhantomData<L>,
        #[allow(dead_code)]
        fn(PhantomData<R>) -> PhantomData<R>,
    );


    impl<T: ?Sized> TypeEq<T, T> {
        /// Constructs a `TypeEq<T, T>`.
        /// 
        /// # Example
        /// 
        /// ```rust
        /// use typewit::TypeEq;
        /// 
        /// assert_eq!(mutate(5, Wit::U32(TypeEq::NEW)), 25);
        /// 
        /// assert_eq!(mutate(5, Wit::Other(TypeEq::NEW)), 5);
        /// assert_eq!(mutate("hello", Wit::Other(TypeEq::NEW)), "hello");
        /// 
        /// const fn mutate<W>(val: W, wit: Wit<W>) -> W {
        ///     match wit {
        ///         Wit::U32(te) => te.to_left(te.to_right(val) + 20),
        ///         Wit::Other(_) => val,
        ///     }
        /// }
        /// 
        /// // This can't be written using the `simple_type_witness` macro because the 
        /// // type in the `Other` variant overlaps with the other ones.
        /// enum Wit<W> {
        ///     U32(TypeEq<W, u32>),
        ///     Other(TypeEq<W, W>),
        /// }
        /// ```
        /// 
        pub const NEW: Self = TypeEq(PhantomData);
    }

    impl TypeEq<(), ()> {
        /// Constructs a `TypeEq<T, T>`.
        #[inline(always)]
        pub const fn new<T: ?Sized>() -> TypeEq<T, T> {
            TypeEq::<T, T>::NEW
        }
    }

    impl<L: ?Sized, R: ?Sized> TypeEq<L, R> {
        /// Constructs `TypeEq<L, R>` if `L == R`, otherwise returns None.
        ///
        /// # Example
        ///
        /// ```rust
        /// use typewit::TypeEq;
        /// 
        /// use std::any::Any;
        /// 
        /// assert_eq!(sum_u32s(&[3u32, 5, 8]), Some(16));
        /// assert_eq!(sum_u32s(&[3i32, 5, 8]), None);
        /// 
        /// 
        /// fn sum_u32s<T: Clone + Any>(foo: &[T]) -> Option<u32> {
        ///     typecast_slice::<T, u32>(foo)
        ///         .map(|foo: &[u32]| foo.iter().copied().sum())
        /// }
        /// 
        /// fn typecast_slice<T: Any, U: Any>(foo: &[T]) -> Option<&[U]> {
        ///     struct SliceFn;
        ///     impl<T> typewit::TypeFn<T> for SliceFn {
        ///         type Output = [T];
        ///     }
        /// 
        ///     TypeEq::<T, U>::with_any().map(|te: TypeEq<T, U>|{
        ///         te.map(SliceFn) // TypeEq<[T], [U]>
        ///           .in_ref()   // TypeEq<&[T]>, &[U]>
        ///           .to_right(foo) // identity cast from `&[T]` to `&[U]`
        ///     })
        /// }
        /// ```
        pub fn with_any() -> Option<Self>
        where
            L: Sized + Any,
            R: Sized + Any,
        {
            if TypeId::of::<L>() == TypeId::of::<R>() {
                // SAFETY: the two TypeIds compare equal, so L == R
                unsafe { Some(TypeEq::new_unchecked()) }
            } else {
                None
            }
        }

        /// Constructs a `TypeEq<L, R>`.
        ///
        /// # Safety
        ///
        /// You must ensure that `L` is the same type as `R`.
        ///
        /// # Examples
        ///
        /// ### Unsound usage
        /// <span id="arbitrary-transmute"></span>
        ///
        /// This example demonstrates why `L == R` is a strict requirement.
        ///
        /// ```rust
        /// use typewit::{TypeEq, TypeFn};
        ///
        /// // SAFETY: WRONG! UNSOUND!
        /// let te: TypeEq<u8, i8> = unsafe{ TypeEq::new_unchecked() };
        /// 
        /// // Because `TypeEq<u8, i8>` is incorrect,
        /// // we get this absurd `TypeEq` from the `project` method.
        /// let absurd: TypeEq<(), Vec<usize>> = te.project::<Func>();
        /// 
        /// // This casts from `()` to `Vec<usize>` (which is UB).
        /// // Last time I tried uncommenting this, it killed the test runner.
        /// // absurd.to_right(()); 
        /// 
        /// struct Func;
        /// impl TypeFn<u8> for Func { type Output = (); }
        /// impl TypeFn<i8> for Func { type Output = Vec<usize>; }
        ///
        ///
        /// ```
        ///
        #[inline(always)]
        pub const unsafe fn new_unchecked() -> TypeEq<L, R> {
            TypeEq(PhantomData)
        }
    }
}
pub use type_eq_::TypeEq;

impl<L: ?Sized, R: ?Sized> Copy for TypeEq<L, R> {}

impl<L: ?Sized, R: ?Sized> Clone for TypeEq<L, R> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<L: ?Sized, R: ?Sized> TypeEq<L, R> {
    /// Converts this `TypeEq` into a [`TypeCmp`](crate::TypeCmp)
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq};
    /// 
    /// const TC: TypeCmp<bool, bool> = TypeEq::NEW.to_cmp();
    /// 
    /// assert!(matches!(TC, TypeCmp::Eq(_)));
    /// ```
    #[inline(always)]
    pub const fn to_cmp(self) -> crate::TypeCmp<L, R> {
        crate::TypeCmp::Eq(self)
    }

    /// Swaps the type parameters of this `TypeEq`
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::TypeEq;
    /// 
    /// assert_eq!(flip_bytes([3, 5], TypeEq::NEW), [5, 3]);
    /// 
    /// const fn flip_bytes<T>(val: T, te: TypeEq<T, [u8; 2]>) -> T {
    ///     bar(val, te.flip())
    /// }
    /// const fn bar<T>(val: T, te: TypeEq<[u8; 2], T>) -> T {
    ///     let [l, r] = te.to_left(val);
    ///     te.to_right([r, l])
    /// }
    /// ```
    /// 
    #[inline(always)]
    pub const fn flip(self: TypeEq<L, R>) -> TypeEq<R, L> {
        // SAFETY: L == R implies R == L
        unsafe { TypeEq::new_unchecked() }
    }

    /// Joins this `TypeEq<L, R>` with a `TypeEq<R, O>`, producing a `TypeEq<L, O>`.
    /// 
    /// The returned `TypeEq` can then be used to coerce between `L` and `O`.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::TypeEq;
    /// 
    /// assert_eq!(foo(TypeEq::NEW, TypeEq::NEW, Some(3)), Some(3));
    /// assert_eq!(foo(TypeEq::NEW, TypeEq::NEW, None), None);
    /// 
    /// 
    /// fn foo<L, X>(
    ///     this: TypeEq<L, Option<X>>,
    ///     that: TypeEq<Option<X>, Option<u32>>,
    ///     value: Option<u32>,
    /// ) -> L {
    ///     let te: TypeEq<L, Option<u32>> = this.join(that);
    ///     te.to_left(value)
    /// }
    /// 
    /// ```
    /// 
    #[inline(always)]
    pub const fn join<O: ?Sized>(self: TypeEq<L, R>, _other: TypeEq<R, O>) -> TypeEq<L, O> {
        // SAFETY: (L == R, R == O) implies L == O
        unsafe { TypeEq::new_unchecked() }
    }
}

impl<L0, R0> TypeEq<L0, R0> {
    /// Combines this `TypeEq<L0, R0>` with a `TypeEq<L1, R1>`, 
    /// producing a `TypeEq<(L0, L1), (R0, R1)>`.
    /// 
    /// # Alternative
    /// 
    /// For an alternative which allows zipping `TypeEq` with any
    ///  [`BaseTypeWitness`](crate::BaseTypeWitness),
    /// you can use [`methods::zip2`](crate::methods::zip2)
    /// (requires the `"rust_1_65"` feature)
    /// 
    /// # Example
    /// 
    /// This example demonstrates how one can combine two `TypeEq`s to use
    /// with a multi-parameter type.
    /// 
    /// ```rust
    /// use typewit::{const_marker::Usize, TypeEq, TypeFn};
    /// 
    /// assert_eq!(make_foo(TypeEq::NEW, TypeEq::NEW), Foo("hello", [3, 5, 8]));
    /// 
    /// const fn make_foo<T, const N: usize>(
    ///     te_ty: TypeEq<T, &'static str>,
    ///     te_len: TypeEq<Usize<N>, Usize<3>>,
    /// ) -> Foo<T, N> {
    ///     // the type annotations are just for the reader, they can be inferred.
    ///     let te_pair: TypeEq<(T, Usize<N>), (&str, Usize<3>)> = te_ty.zip(te_len);
    /// 
    ///     let te: TypeEq<Foo<T, N>, Foo<&str, 3>> = te_pair.project::<GFoo>();
    /// 
    ///     // `te.to_left(...)` here goes from `Foo<&str, 3>` to `Foo<T, N>`
    ///     te.to_left(Foo("hello", [3, 5, 8]))
    /// }
    /// 
    /// #[derive(Debug, PartialEq)]
    /// struct Foo<T, const N: usize>(T, [u8; N]);
    /// 
    /// typewit::type_fn!{
    ///     // Type-level function from `(T, Usize<N>)` to `Foo<T, N>`
    ///     struct GFoo;
    ///
    ///     impl<T, const N: usize> (T, Usize<N>) => Foo<T, N>
    /// }
    /// ```
    /// 
    #[inline(always)]
    pub const fn zip<L1: ?Sized, R1: ?Sized>(
        self: TypeEq<L0, R0>,
        other: TypeEq<L1, R1>,
    ) -> TypeEq<(L0, L1), (R0, R1)> {
        zip_impl!{self[L0, R0], other[L1, R1]}
    }

    /// Combines three `TypeEq<L*, R*>` to produce a
    /// `TypeEq<(L0, L1, L2), (R0, R1, R2)>`.
    /// 
    /// # Alternative
    /// 
    /// For an alternative which allows zipping `TypeEq` with two of any
    ///  [`BaseTypeWitness`](crate::BaseTypeWitness),
    /// you can use [`methods::zip3`](crate::methods::zip3)
    /// (requires the `"rust_1_65"` feature)
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeEq, type_eq};
    /// 
    /// use std::cmp::Ordering::{self, Less};
    /// 
    /// assert_eq!(make_tuple(type_eq(), type_eq(), type_eq()), (3, "foo", Less));
    /// 
    /// fn make_tuple<A, B, C>(
    ///     te0: TypeEq<A, u8>,
    ///     te1: TypeEq<B, &str>,
    ///     te2: TypeEq<C, Ordering>,
    /// ) -> (A, B, C) {
    ///     te0.zip3(te1, te2) // returns `TypeEq<(A, B, C), (u8, &str, Ordering)>`
    ///         .to_left((3, "foo", Less))
    /// }
    /// 
    /// ```
    pub const fn zip3<L1, R1, L2: ?Sized, R2: ?Sized>(
        self: TypeEq<L0, R0>,
        other1: TypeEq<L1, R1>,
        other2: TypeEq<L2, R2>,
    ) -> TypeEq<(L0, L1, L2), (R0, R1, R2)> {
        zip_impl!{
            self[L0, R0],
            other1[L1, R1],
            other2[L2, R2],
        }
    }

    /// Combines four `TypeEq<L*, R*>` to produce a
    /// `TypeEq<(L0, L1, L2, L3), (R0, R1, R2, L3)>`.
    /// 
    /// # Alternative
    /// 
    /// For an alternative which allows zipping `TypeEq` with three of any
    ///  [`BaseTypeWitness`](crate::BaseTypeWitness),
    /// you can use [`methods::zip4`](crate::methods::zip4)
    /// (requires the `"rust_1_65"` feature)
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeEq, type_eq};
    /// 
    /// use std::cmp::Ordering::{self, Less};
    /// 
    /// assert_eq!(
    ///     make_tuple(type_eq(), type_eq(), type_eq(), type_eq()), 
    ///     (3, "foo", Less, true),
    /// );
    /// 
    /// fn make_tuple<A, B, C, D>(
    ///     te0: TypeEq<A, u8>,
    ///     te1: TypeEq<B, &str>,
    ///     te2: TypeEq<C, Ordering>,
    ///     te3: TypeEq<D, bool>,
    /// ) -> (A, B, C, D) {
    ///     let te: TypeEq<(A, B, C, D), (u8, &str, Ordering, bool)> = te0.zip4(te1, te2, te3);
    ///     te.to_left((3, "foo", Less, true))
    /// }
    /// 
    /// ```
    pub const fn zip4<L1, R1, L2, R2, L3: ?Sized, R3: ?Sized>(
        self: TypeEq<L0, R0>,
        other1: TypeEq<L1, R1>,
        other2: TypeEq<L2, R2>,
        other3: TypeEq<L3, R3>,
    ) -> TypeEq<(L0, L1, L2, L3), (R0, R1, R2, R3)> {
        zip_impl!{
            self[L0, R0],
            other1[L1, R1],
            other2[L2, R2],
            other3[L3, R3],
        }
    }

}

impl<L, R> TypeEq<L, R> {
    /// Whether `L` is the same type as `R`.
    /// 
    /// False positive equality is fine for this associated constant,
    /// since it's used to optimize out definitely unequal types.
    const ARE_SAME_TYPE: Amb = {
        // hacky way to emulate a lifetime-unaware
        // `TypeId::of<L>() == TypeId::of<R>()`
        let approx_same_type = {
            core::mem::size_of::<L>() == core::mem::size_of::<R>()
                && core::mem::align_of::<L>() == core::mem::align_of::<R>()
                && core::mem::size_of::<Option<L>>() == core::mem::size_of::<Option<R>>()
                && core::mem::align_of::<Option<L>>() == core::mem::align_of::<Option<R>>()
        };

        if approx_same_type {
            Amb::Indefinite
        } else {
            Amb::No
        }
    };


    /// Hints to the compiler that a `TypeEq<L, R>`
    /// can only be constructed if `L == R`.
    ///
    /// This function takes and returns `val` unmodified.
    /// This allows returning some value from an expression
    /// while hinting that `L == R`.
    ///
    #[inline(always)]
    pub const fn reachability_hint<T>(self, val: T) -> T {
        if let Amb::No = Self::ARE_SAME_TYPE {
            // safety: `TypeEq<L, R>` requires `L == R` to be constructed
            unsafe { core::hint::unreachable_unchecked() }
        }

        val
    }

    /// A no-op cast from `L` to `R`.
    /// 
    /// This cast is a no-op because having a `TypeEq<L, R>` value
    /// proves that `L` and `R` are the same type.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeEq, type_eq};
    /// 
    /// use std::cmp::Ordering::{self, *};
    /// 
    /// assert_eq!(mutated(Less, Wit::Ord(type_eq())), Greater);
    /// assert_eq!(mutated(Equal, Wit::Ord(type_eq())), Equal);
    /// assert_eq!(mutated(Greater, Wit::Ord(type_eq())), Less);
    /// 
    /// assert_eq!(mutated(false, Wit::Bool(type_eq())), true);
    /// assert_eq!(mutated(true, Wit::Bool(type_eq())), false);
    /// 
    /// const fn mutated<R>(arg: R, w: Wit<R>) -> R {
    ///     match w {
    ///         Wit::Ord(te) => te.to_left(te.to_right(arg).reverse()),
    ///         Wit::Bool(te) => te.to_left(!te.to_right(arg)),
    ///     }
    /// }
    /// 
    /// enum Wit<R> {
    ///     Ord(TypeEq<R, Ordering>),
    ///     Bool(TypeEq<R, bool>),
    /// }
    /// ```
    /// 
    #[inline(always)]
    pub const fn to_right(self, from: L) -> R {
        self.reachability_hint(());

        // safety: `TypeEq<L, R>` requires `L == R` to be constructed
        unsafe { crate::__priv_transmute!(L, R, from) }
    }
    /// A no-op cast from `R` to `L`.
    /// 
    /// This cast is a no-op because having a `TypeEq<L, R>` value
    /// proves that `L` and `R` are the same type.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeEq, type_eq};
    /// 
    /// assert_eq!(stuff(Wit::OptSlice(type_eq())), Some(&[3, 5, 8][..]));
    /// assert_eq!(stuff(Wit::Bool(type_eq())), true);
    /// 
    /// const fn stuff<R>(te: Wit<R>) -> R {
    ///     match te {
    ///         Wit::OptSlice(te) => te.to_left(Some(&[3, 5, 8])),
    ///         Wit::Bool(te) => te.to_left(true),
    ///     }
    /// }
    /// 
    /// enum Wit<R> {
    ///     OptSlice(TypeEq<R, Option<&'static [u16]>>),
    ///     Bool(TypeEq<R, bool>),
    /// }
    /// ```
    /// 
    #[inline(always)]
    pub const fn to_left(self, from: R) -> L {
        self.reachability_hint(());

        // safety: `TypeEq<L, R>` requires `L == R` to be constructed
        unsafe { crate::__priv_transmute!(R, L, from) }
    }
}


impl<L: ?Sized, R: ?Sized> TypeWitnessTypeArg for TypeEq<L, R> {
    type Arg = L;
}

impl<T: ?Sized> MakeTypeWitness for TypeEq<T, T> {
    const MAKE: Self = Self::NEW;
}


impl<L: ?Sized, R: ?Sized> TypeEq<L, R> {
    /// Maps the type arguments of this `TypeEq`
    /// by using the `F` [type-level function](crate::type_fn::TypeFn).
    /// 
    /// Use this function over [`project`](Self::project) 
    /// if you want the type of the passed in function to be inferred.
    ///
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeEq, TypeFn};
    /// 
    /// assert_eq!(foo(TypeEq::NEW), (false, 5));
    /// 
    /// const fn foo<'a, T>(te: TypeEq<u32, T>) -> (bool, T) {
    ///     // `GPair<bool>` maps `u32` to `(bool, u32)`
    ///     //           and maps `T`   to `(bool, T)`
    ///     let map_te: TypeEq<(bool, u32), (bool, T)> = te.map(GPair::<bool>::NEW); 
    /// 
    ///     // same as the above, but inferring `GPair`'s generic arguments.
    ///     let _: TypeEq<(bool, u32), (bool, T)> = te.map(GPair::NEW); 
    /// 
    ///     map_te.to_right((false, 5u32))
    /// }
    /// 
    /// // Declares `struct GPair<A>`, a type-level function from `B` to `(A, B)` 
    /// typewit::type_fn! {
    ///      struct GPair<A>;
    /// 
    ///      impl<B> B => (A, B)
    /// } 
    /// ```
    /// 
    // #[cfg(feature = "project")]
    // #[cfg_attr(feature = "docsrs", doc(cfg(feature = "project")))]
    pub const fn map<F>(
        self,
        func: F,
    ) -> TypeEq<CallFn<InvokeAlias<F>, L>, CallFn<InvokeAlias<F>, R>>
    where
        InvokeAlias<F>: crate::TypeFn<L> + crate::TypeFn<R>
    {
        core::mem::forget(func);
        projected_type_cmp!{self, L, R, F}
    }

    /// Maps the type arguments of this `TypeEq`
    /// by using the `F` [type-level function](crate::type_fn::TypeFn).
    /// 
    /// Use this function over [`map`](Self::map) 
    /// if you want to specify the type of the passed in function explicitly.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeEq, TypeFn};
    /// 
    /// assert_eq!(foo(TypeEq::NEW), vec![3u32, 5, 8]);
    /// 
    /// fn foo<T>(te: TypeEq<u32, T>) -> Vec<T> {
    ///     let vec_te: TypeEq<Vec<u32>, Vec<T>> = te.project::<GVec>();
    ///     vec_te.to_right(vec![3, 5, 8])
    /// }
    /// 
    /// // Declares `GVec`, a type-level function from `T` to `Vec<T>`
    /// typewit::type_fn!{
    ///     struct GVec;
    /// 
    ///     impl<T> T => Vec<T>
    /// }
    /// 
    /// ```
    /// 
    pub const fn project<F>(self) -> TypeEq<CallFn<InvokeAlias<F>, L>, CallFn<InvokeAlias<F>, R>>
    where
        InvokeAlias<F>: crate::TypeFn<L> + crate::TypeFn<R>
    {
        projected_type_cmp!{self, L, R, F}
    }
}

impl<L: ?Sized, R: ?Sized> TypeEq<L, R> {
    /// Maps the type arguments of this `TypeEq`
    /// by using the [reversed](crate::RevTypeFn) 
    /// version of the `F` type-level function.
    /// 
    /// Use this function over [`unproject`](Self::unproject) 
    /// if you want the type of the passed in function to be inferred.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeEq, UncallFn};
    /// 
    /// assert_eq!(first_int(&[3, 5, 8, 13], TypeEq::NEW), 3);
    /// 
    /// const fn first_int<T, const N: usize>(
    ///     array: &[T; N],
    ///     te_slice: TypeEq<[T], [u8]>,
    /// ) -> u8 {
    ///     let te: TypeEq<T, u8> = te_slice.unmap(SliceFn);
    ///
    ///     let te_ref: TypeEq<&T, &u8> = te.in_ref();
    ///
    ///     *te_ref.to_right(&array[0])
    /// }
    ///
    /// typewit::inj_type_fn! {
    ///     struct SliceFn;
    /// 
    ///     impl<T> T => [T]
    /// }
    /// ```
    pub const fn unmap<F>(
        self,
        func: F,
    ) -> TypeEq<UncallFn<InvokeAlias<F>, L>, UncallFn<InvokeAlias<F>, R>>
    where
        InvokeAlias<F>: RevTypeFn<L> + RevTypeFn<R>
    {
        core::mem::forget(func);
        
        unprojected_type_cmp!{self, L, R, F}
    }

    /// Maps the type arguments of this `TypeEq`
    /// by using the [reversed](crate::RevTypeFn) 
    /// version of the `F` type-level function.
    /// 
    /// Use this function over [`unmap`](Self::unmap) 
    /// if you want to specify the type of the passed in function explicitly.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::TypeEq;
    /// use std::ops::{Range, RangeInclusive as RangeInc};
    /// 
    /// assert_eq!(usize_bounds(3..=5, TypeEq::NEW), (3, 5));
    /// 
    /// const fn usize_bounds<T>(
    ///     range: RangeInc<T>,
    ///     te_range: TypeEq<Range<T>, Range<usize>>,
    /// ) -> (usize, usize) {
    ///     let te: TypeEq<T, usize> = te_range.unproject::<RangeFn>();
    ///     
    ///     let te_range_inc: TypeEq<RangeInc<T>, RangeInc<usize>> = te.project::<RangeIncFn>();
    ///     
    ///     let range: RangeInc<usize> = te_range_inc.to_right(range);
    ///     
    ///     (*range.start(), *range.end())
    /// }
    /// 
    /// typewit::inj_type_fn! {
    ///     struct RangeFn;
    /// 
    ///     impl<T> T => Range<T>
    /// }
    /// typewit::inj_type_fn! {
    ///     struct RangeIncFn;
    /// 
    ///     impl<T> T => RangeInc<T>
    /// }
    /// ```
    pub const fn unproject<F>(
        self,
    ) -> TypeEq<UncallFn<InvokeAlias<F>, L>, UncallFn<InvokeAlias<F>, R>>
    where
        InvokeAlias<F>: crate::RevTypeFn<L> + crate::RevTypeFn<R>
    {
        unprojected_type_cmp!{self, L, R, F}
    }
}

impl<L: ?Sized, R: ?Sized> TypeEq<L, R> {
    /// Converts a `TypeEq<L, R>` to `TypeEq<&L, &R>`
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{MakeTypeWitness, TypeEq};
    /// 
    /// assert_eq!(get::<u8>(), &3);
    /// assert_eq!(get::<str>(), "hello");
    /// 
    /// 
    /// const fn get<R: ?Sized>() -> &'static R 
    /// where
    ///     Returned<R>: MakeTypeWitness
    /// {
    ///     match MakeTypeWitness::MAKE {
    ///         // `te` is a `TypeEq<R, u8>`
    ///         Returned::U8(te) => te.in_ref().to_left(&3),
    ///
    ///         // `te` is a `TypeEq<R, str>`
    ///         Returned::Str(te) => te.in_ref().to_left("hello"),
    ///     }
    /// }
    /// 
    /// typewit::simple_type_witness! {
    ///     // declares the `enum Returned<R> {` type witness
    ///     enum Returned {
    ///         // this variant requires `R == u8`
    ///         U8 = u8,
    ///         // this variant requires `R == str`
    ///         Str = str,
    ///     }
    /// }
    /// ```
    /// 
    pub const fn in_ref<'a>(self) -> TypeEq<&'a L, &'a R> {
        projected_type_cmp!{self, L, R, type_fn::GRef<'a>}
    }

    crate::utils::conditionally_const!{
        feature = "rust_1_83";

        /// Converts a `TypeEq<L, R>` to `TypeEq<&mut L, &mut R>`
        /// 
        /// # Constness
        /// 
        /// This requires the `"rust_1_83"` feature to be a `const fn`.
        /// 
        /// # Example
        /// 
        /// Because this example calls `in_mut` inside a `const fn`,
        /// it requires the `"rust_1_83"` crate feature.
        #[cfg_attr(not(feature = "rust_1_83"), doc = "```ignore")]
        #[cfg_attr(feature = "rust_1_83", doc = "```rust")]
        /// 
        /// use typewit::{TypeEq, type_eq};
        /// 
        /// let foo = &mut Foo { bar: 10, baz: ['W', 'H', 'O'] };
        /// 
        /// *get_mut(foo, Field::Bar(type_eq())) *= 2;
        /// assert_eq!(foo.bar, 20);
        /// 
        /// assert_eq!(*get_mut(foo, Field::Baz(type_eq())), ['W', 'H', 'O']);
        /// 
        /// 
        /// const fn get_mut<R>(foo: &mut Foo, te: Field<R>) -> &mut R {
        ///     match te {
        ///         Field::Bar(te) => te.in_mut().to_left(&mut foo.bar),
        ///         Field::Baz(te) => te.in_mut().to_left(&mut foo.baz),
        ///     }
        /// }
        /// 
        /// struct Foo {
        ///     bar: u8,
        ///     baz: [char; 3],
        /// }
        /// 
        /// enum Field<R: ?Sized> {
        ///     Bar(TypeEq<R, u8>),
        ///     Baz(TypeEq<R, [char; 3]>),
        /// }
        /// ```
        /// 
        pub fn in_mut['a](self) -> TypeEq<&'a mut L, &'a mut R> {
            projected_type_cmp!{self, L, R, type_fn::GRefMut<'a>}
        }
    }

    /// Converts a `TypeEq<L, R>` to `TypeEq<Box<L>, Box<R>>`
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{MakeTypeWitness, TypeEq, type_eq};
    /// 
    /// use std::any::Any;
    /// use std::fmt::Display;
    /// 
    /// assert_eq!(factory::<dyn Any>().downcast::<u16>().unwrap(), Box::new(1337));
    /// assert_eq!(factory::<dyn Display>().to_string(), "hello bob");
    /// 
    /// fn factory<R: ?Sized>() -> Box<R> 
    /// where
    ///     Dyn<R>: MakeTypeWitness
    /// {
    ///     match MakeTypeWitness::MAKE {
    ///         // `te` is a `TypeEq<R, dyn Any>`
    ///         Dyn::Any(te) => te.in_box().to_left(Box::new(1337u16)),
    ///
    ///         // `te` is a `TypeEq<R, dyn Display>`
    ///         Dyn::Display(te) => te.in_box().to_left(Box::new("hello bob")),
    ///     }
    /// }
    /// 
    /// typewit::simple_type_witness! {
    ///     // declares the `enum Dyn<R> {` type witness
    ///     enum Dyn {
    ///         // this variant requires `R == dyn Any`
    ///         Any = dyn Any,
    ///         // this variant requires `R == dyn Display`
    ///         Display = dyn Display,
    ///     }
    /// }
    /// 
    /// ```
    ///
    #[cfg(feature = "alloc")]
    #[cfg_attr(feature = "docsrs", doc(cfg(feature = "alloc")))]
    pub const fn in_box(self) -> TypeEq<Box<L>, Box<R>> {
        projected_type_cmp!{self, L, R, type_fn::GBox}
    }
}

impl<L: Sized, R: Sized> TypeEq<L, R> {
    /// Combines `TypeEq<L, R>` and `TypeEq<Usize<UL>, Usize<UR>>`
    /// into `TypeEq<[L; UL], [R; UR]>`
    /// 
    /// # Alternative
    /// 
    /// For an alternative which allows passing any
    ///  [`BaseTypeWitness`](crate::BaseTypeWitness) for the length,
    /// you can use [`methods::in_array`](crate::methods::in_array)
    /// (requires the `"rust_1_65"` feature)
    /// 
    /// 
    /// # Example
    /// 
    /// <details>
    /// <summary><b>motivation</b></summary>
    /// <p>
    /// The safe way to map an array in const fns(on stable Rust in 2023)
    /// is to create an array of the returned type with some dummy value,
    /// and then fill it in with the desired values.
    /// 
    /// Because the function in this example takes a `[T; LEN]` where the `T` is generic,
    /// it copies the first element of the input array to initialize the returned array,
    /// so we must handle empty arrays, 
    /// but trying to return an empty array the naive way
    /// ```compile_fail
    /// # use std::num::Wrapping;
    /// # const fn map_wrapping<T: Copy, const LEN: usize>(arr: [T; LEN]) -> [Wrapping<T>; LEN] {
    ///     if LEN == 0 {
    ///         return [];
    ///     }
    /// #   unimplemented!()
    /// # }
    /// ```
    /// does not work
    /// ```text
    /// error[E0308]: mismatched types
    ///  --> src/type_eq.rs:827:16
    ///   |
    /// 4 | const fn map_wrapping<T: Copy, const LEN: usize>(arr: [T; LEN]) -> [Wrapping<T>; LEN] {
    ///   |                                                                    ------------------ expected `[Wrapping<T>; LEN]` because of return type
    /// 5 |     if LEN == 0 {
    /// 6 |         return [];
    ///   |                ^^ expected `LEN`, found `0`
    ///   |
    ///   = note: expected array `[Wrapping<T>; LEN]`
    ///              found array `[_; 0]`
    /// 
    /// ```
    /// </p>
    /// </details>
    /// 
    /// This example demonstrates how `in_array` allows one to return an empty array:
    /// (this example requires Rust 1.61.0, because it uses trait bounds in const fns)
    #[cfg_attr(not(feature = "rust_1_61"), doc = "```ignore")]
    #[cfg_attr(feature = "rust_1_61", doc = "```rust")]
    /// use typewit::{const_marker::Usize, TypeCmp, TypeEq};
    /// 
    /// use std::num::Wrapping;
    /// 
    /// assert_eq!(map_wrapping([""; 0]), []);
    /// assert_eq!(map_wrapping([3, 5, 8]), [Wrapping(3), Wrapping(5), Wrapping(8)]);
    /// 
    /// const fn map_wrapping<T: Copy, const LEN: usize>(arr: [T; LEN]) -> [Wrapping<T>; LEN] {
    ///     // `teq` is a `TypeEq<Usize<LEN>, Usize<0>>`
    ///     if let TypeCmp::Eq(teq) = Usize::<LEN>.equals(Usize::<0>) {
    ///         return TypeEq::new::<Wrapping<T>>()
    ///             .in_array(teq) // `TypeEq<[Wrapping<T>; LEN], [Wrapping<T>; 0]>`
    ///             .to_left([]);
    ///     }
    ///     
    ///     let mut ret = [Wrapping(arr[0]); LEN];
    ///     let mut i = 1;
    ///     
    ///     while i < LEN {
    ///         ret[i] = Wrapping(arr[i]);
    ///         i += 1;
    ///     }
    ///     
    ///     ret
    /// }
    /// ```
    #[inline(always)]
    pub const fn in_array<const UL: usize, const UR: usize>(
        self,
        other: TypeEq<Usize<UL>, Usize<UR>>,
    ) -> TypeEq<[L; UL], [R; UR]> {
        zip_project!{
            self,
            other,
            crate::type_fn::PairToArrayFn,
            (L, R),
            (Usize<UL>, Usize<UR>),
        }
    }
}

enum Amb {
    // indefinitely false/true
    Indefinite,
    // definitely false
    No,
}



impl<T: ?Sized> Default for TypeEq<T, T> {
    fn default() -> Self {
        Self::NEW
    }
}

impl<L: ?Sized, R: ?Sized> Debug for TypeEq<L, R> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("TypeEq")
    }
}

impl<L: ?Sized, R: ?Sized> PartialEq for TypeEq<L, R> {
    fn eq(&self, _: &Self) -> bool {
        true
    }
}

impl<L: ?Sized, R: ?Sized> PartialOrd for TypeEq<L, R> {
    fn partial_cmp(&self, _: &Self) -> Option<Ordering> {
        Some(Ordering::Equal)
    }
}

impl<L: ?Sized, R: ?Sized> Ord for TypeEq<L, R> {
    fn cmp(&self, _: &Self) -> Ordering {
        Ordering::Equal
    }
}

impl<L: ?Sized, R: ?Sized> Eq for TypeEq<L, R> {}


impl<L: ?Sized, R: ?Sized> Hash for TypeEq<L, R> {
    fn hash<H>(&self, _state: &mut H)
    where H: Hasher
    {}
}

