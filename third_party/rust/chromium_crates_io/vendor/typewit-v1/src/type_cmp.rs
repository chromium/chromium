use crate::{TypeEq, TypeNe};

#[cfg(feature = "rust_1_61")]
use crate::{BaseTypeWitness, MetaBaseTypeWit, SomeTypeArgIsNe};


use core::{
    any::Any,
    cmp::{Ordering, Eq, Ord, PartialEq, PartialOrd},
    hash::{Hash, Hasher},
    fmt::{self, Debug},
};

/// A witness of whether its `L` and `R` type parameters are the same or different types.
/// 
/// # Example
/// 
/// ### Custom array creation
/// 
/// (this example requires Rust 1.63.0, because of [`core::array::from_fn`]).
/// 
#[cfg_attr(not(feature = "rust_1_65"), doc = "```ignore")]
#[cfg_attr(feature = "rust_1_65", doc = "```rust")]
/// use typewit::{const_marker::Usize, TypeCmp, TypeEq, TypeNe};
/// 
/// let empty: [String; 0] = [];
/// assert_eq!(ArrayMaker::<String, 0>::empty().make(), empty);
/// 
/// assert_eq!(ArrayMaker::<u8, 2>::defaulted().make(), [0u8, 0u8]);
/// 
/// assert_eq!(ArrayMaker::with(|i| i.pow(2)).make(), [0usize, 1, 4, 9]);
/// 
/// 
/// enum ArrayMaker<T, const LEN: usize> {
///     NonEmpty(fn(usize) -> T, TypeNe<[T; LEN], [T; 0]>),
///     Empty(TypeEq<[T; LEN], [T; 0]>),
/// }
/// 
/// impl<T, const LEN: usize> ArrayMaker<T, LEN> {
///     pub fn make(self) -> [T; LEN] {
///         match self {
///             ArrayMaker::NonEmpty(func, _) => std::array::from_fn(func),
///             ArrayMaker::Empty(te) => te.to_left([]),
///         }
///     }
/// 
///     pub const fn defaulted() -> Self 
///     where
///         T: Default
///     {
///         Self::with(|_| Default::default())
///     }
/// 
///     pub const fn with(func: fn(usize) -> T) -> Self {
///         match  Usize::<LEN>.equals(Usize::<0>) // : TypeCmp<Usize<LEN>, Usize<0>>
///             .project::<ArrayFn<T>>() // : TypeCmp<[T; LEN], [T; 0]>
///         {
///             TypeCmp::Ne(ne) => ArrayMaker::NonEmpty(func, ne),
///             TypeCmp::Eq(eq) => ArrayMaker::Empty(eq),
///         }
///     }
/// }
/// 
/// impl<T> ArrayMaker<T, 0> {
///     pub const fn empty() -> Self {
///         Self::Empty(TypeEq::NEW)
///     }
/// }
/// 
/// impl<T, const LEN: usize> Copy for ArrayMaker<T, LEN> {}
/// 
/// impl<T, const LEN: usize> Clone for ArrayMaker<T, LEN> {
///     fn clone(&self) -> Self { *self }
/// }
/// 
/// typewit::inj_type_fn! {
///     // Declares `struct ArrayFn`, which implements `InjTypeFn<Usize<LEN>>`:
///     // an injective type-level function from `Usize<LEN>` to `[T; LEN]`
///     struct ArrayFn<T>;
///     impl<const LEN: usize> Usize<LEN> => [T; LEN]
/// }
/// ```
pub enum TypeCmp<L: ?Sized, R: ?Sized>{
    /// proof of `L == R`
    Eq(TypeEq<L, R>),
    /// proof of `L != R`
    Ne(TypeNe<L, R>),
}

impl<L: ?Sized, R: ?Sized> TypeCmp<L, R> {
    /// Constructs a `TypeCmp<L, R>` by comparing the `L` and `R` types for equality.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::TypeCmp;
    /// 
    /// let eq: TypeCmp<u8, u8> = TypeCmp::with_any();
    /// assert!(matches!(eq, TypeCmp::Eq(_)));
    /// 
    /// let ne = TypeCmp::<u8, i8>::with_any();
    /// assert!(matches!(ne, TypeCmp::Ne(_)));
    /// ```
    #[deprecated = concat!(
        "fallout of `https://github.com/rust-lang/rust/issues/97156`,",
        "`TypeId::of::<L>() != TypeId::of::<R>()` does not imply `L != R`"
    )]
    pub fn with_any() -> Self
    where
        L: Sized + Any,
        R: Sized + Any,
    {
        #[allow(deprecated)]
        if let Some(equal) = TypeEq::with_any() {
            TypeCmp::Eq(equal)
        } else if let Some(unequal) = TypeNe::with_any() {
            TypeCmp::Ne(unequal)
        } else {
            unreachable!()
        }
    }

    /// Swaps the type arguments of this `TypeCmp`
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, type_ne};
    /// 
    /// const TC: TypeCmp<u8, i8> = TypeCmp::Ne(type_ne!(u8, i8));
    /// 
    /// const TK: TypeCmp<i8, u8> = TC.flip();
    /// 
    /// ```
    pub const fn flip(self) -> TypeCmp<R, L> {
        match self {
            TypeCmp::Eq(te) => TypeCmp::Eq(te.flip()),
            TypeCmp::Ne(te) => TypeCmp::Ne(te.flip()),
        }
    }

    /// Joins this `TypeCmp<L, R>` with a `TypeEq<Q, L>`, producing a `TypeCmp<Q, R>`.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, type_ne};
    /// 
    /// const TC: TypeCmp<str, [u8]> = type_ne!(str, [u8]).to_cmp();
    /// 
    /// const fn foo<A: ?Sized>(eq: TypeEq<A, str>) {
    ///     let _tc: TypeCmp<A, [u8]> = TC.join_left(eq);
    /// }
    /// ```
    pub const fn join_left<Q: ?Sized>(self, left: TypeEq<Q, L>) -> TypeCmp<Q, R> {
        match self {
            TypeCmp::Eq(te) => TypeCmp::Eq(left.join(te)),
            TypeCmp::Ne(te) => TypeCmp::Ne(te.join_left(left)),
        }
    }

    /// Joins this `TypeCmp<L, R>` with a `TypeEq<R, Q>`, producing a `TypeCmp<L, Q>`.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, type_ne};
    /// 
    /// const NE: TypeCmp<String, Vec<u8>> = type_ne!(String, Vec<u8>).to_cmp();
    /// 
    /// const fn foo<A>(eq: TypeEq<Vec<u8>, A>) {
    ///     let _ne: TypeCmp<String, A> = NE.join_right(eq);
    /// }
    /// ```
    pub const fn join_right<Q: ?Sized>(self, right: TypeEq<R, Q>) -> TypeCmp<L, Q> {
        match self {
            TypeCmp::Eq(te) => TypeCmp::Eq(te.join(right)),
            TypeCmp::Ne(te) => TypeCmp::Ne(te.join_right(right)),
        }
    }

    /// Converts this `TypeCmp<L, R>` into an `Option<TypeEq<L, R>>`.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, type_ne};
    /// 
    /// let eq: TypeCmp<u8, u8> = TypeCmp::Eq(TypeEq::NEW);
    /// assert!(matches!(eq.eq(), Some(TypeEq::<u8, u8>{..})));
    /// 
    /// let ne = TypeCmp::Ne(type_ne!(u8, i8));
    /// assert!(matches!(ne.eq(), None::<TypeEq<u8, i8>>));
    /// ```
    pub const fn eq(self) -> Option<TypeEq<L, R>> {
        match self {
            TypeCmp::Eq(te) => Some(te),
            TypeCmp::Ne(_) => None,
        }
    }

    /// Converts this `TypeCmp<L, R>` into an `Option<TypeNe<L, R>>`.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, TypeNe, type_ne};
    /// 
    /// let eq: TypeCmp<u8, u8> = TypeCmp::Eq(TypeEq::NEW);
    /// assert!(matches!(eq.ne(), None::<TypeNe<u8, u8>>));
    /// 
    /// let ne = TypeCmp::Ne(type_ne!(u8, i8));
    /// assert!(matches!(ne.ne(), Some(TypeNe::<u8, i8>{..})));
    /// ```
    pub const fn ne(self) -> Option<TypeNe<L, R>> {
        match self {
            TypeCmp::Eq(_) => None,
            TypeCmp::Ne(te) => Some(te),
        }
    }

    /// Returns whether this `TypeCmp` is a `TypeCmp::Eq`.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, TypeNe, type_ne};
    /// 
    /// const EQ: TypeCmp<u8, u8> = TypeEq::NEW.to_cmp();
    /// assert_eq!(EQ.is_eq(), true);
    /// 
    /// const NE: TypeCmp<i8, u8> = type_ne!(i8, u8).to_cmp();
    /// assert_eq!(NE.is_eq(), false);
    /// ```
    pub const fn is_eq(self) -> bool {
        matches!(self, TypeCmp::Eq(_))
    }

    /// Returns whether this `TypeCmp` is a `TypeCmp::Ne`.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, TypeNe, type_ne};
    /// 
    /// const EQ: TypeCmp<u8, u8> = TypeEq::NEW.to_cmp();
    /// assert_eq!(EQ.is_ne(), false);
    /// 
    /// const NE: TypeCmp<i8, u8> = type_ne!(i8, u8).to_cmp();
    /// assert_eq!(NE.is_ne(), true);
    /// ```
    pub const fn is_ne(self) -> bool {
        matches!(self, TypeCmp::Ne(_))
    }

    /// Returns the contained `TypeEq`
    /// 
    /// # Panic
    /// 
    /// Panics if the contained value is a `TypeNe`.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq};
    /// 
    /// let eq: TypeCmp<u8, u8> = TypeCmp::Eq(TypeEq::NEW);
    /// assert!(matches!(eq.unwrap_eq(), TypeEq::<u8, u8>{..}));
    /// ```
    #[track_caller]
    pub const fn unwrap_eq(self) -> TypeEq<L, R> {
        match self {
            TypeCmp::Eq(te) => te,
            TypeCmp::Ne(_) => panic!("called `TypeCmp::unwrap_eq` on a `TypeNe` value"),
        }
    }

    /// Returns the contained `TypeEq`
    /// 
    /// # Panic
    /// 
    /// Panics if the contained value is a `TypeNe`, with `msg` as the panic message.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq};
    /// 
    /// let eq: TypeCmp<u8, u8> = TypeCmp::Eq(TypeEq::NEW);
    /// assert!(matches!(eq.expect_eq("they're the same type!"), TypeEq::<u8, u8>{..}));
    /// ```
    #[track_caller]
    pub const fn expect_eq(self, msg: &str) -> TypeEq<L, R> {
        match self {
            TypeCmp::Eq(te) => te,
            TypeCmp::Ne(_) => panic!("{}", msg),
        }
    }

    /// Returns the contained `TypeNe`
    /// 
    /// # Panic
    /// 
    /// Panics if the contained value is a `TypeEq`.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeNe, type_ne};
    /// 
    /// let ne = TypeCmp::Ne(type_ne!(u8, i8));
    /// assert!(matches!(ne.unwrap_ne(), TypeNe::<u8, i8>{..}));
    /// ```
    #[track_caller]
    pub const fn unwrap_ne(self) -> TypeNe<L, R> {
        match self {
            TypeCmp::Eq(_) => panic!("called `TypeCmp::unwrap_ne` on a `TypeEq` value"),
            TypeCmp::Ne(te) => te,
        }
    }

    /// Returns the contained `TypeNe`
    /// 
    /// # Panic
    /// 
    /// Panics if the contained value is a `TypeEq`, with `msg` as the panic message.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeNe, type_ne};
    /// 
    /// let ne = TypeCmp::Ne(type_ne!(u8, i8));
    /// assert!(matches!(ne.expect_ne("but u8 isn't i8..."), TypeNe::<u8, i8>{..}));
    /// ```
    #[track_caller]
    pub const fn expect_ne(self, msg: &str) -> TypeNe<L, R> {
        match self {
            TypeCmp::Eq(_) => panic!("{}", msg),
            TypeCmp::Ne(te) => te,
        }
    }
}

#[cfg(feature = "rust_1_61")]
macro_rules! alternative_docs {
    ($func:expr) => {concat!(
        "# Alternative\n",
        "\n",
        "[`methods::", $func,"`](crate::methods::", $func, ") \n",
        "is an alternative to this function. \n",
        "\n",
        "This method always returns `TypeCmp`, \n",
        "while [that function](crate::methods::", $func, ")\n",
        "returns [`TypeNe`] when any argument is a `TypeNe`.\n",
        "\n",
        "# Returned variant\n",
        "\n",
        "This returns either [`TypeCmp::Eq`] or [`TypeCmp::Ne`]",
        " depending on the arguments:\n",
        "- if all arguments (including `self`)",
        " are [`TypeEq`] or [`TypeCmp::Eq`], this returns [`TypeCmp::Eq`] \n",
        "- if any argument (including `self`) ",
        "is a [`TypeNe`] or [`TypeCmp::Ne`], this returns [`TypeCmp::Ne`] \n",
    )};
}

#[cfg(feature = "rust_1_61")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_61")))]
impl<L, R> TypeCmp<L, R> {
    /// Combines this `TypeCmp<L, R>` with a [`BaseTypeWitness`] type to produce a
    /// `TypeCmp<(L, A::L), (R, A::R)>`.
    /// 
    #[doc = alternative_docs!("zip2")]
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, TypeNe, type_ne};
    /// 
    /// 
    /// const NE: TypeNe<u8, i8> = type_ne!(u8, i8);
    /// const EQ: TypeEq<u16, u16> = TypeEq::NEW;
    /// const TC_NE: TypeCmp<u32, u64> = TypeCmp::Ne(type_ne!(u32, u64));
    /// const TC_EQ: TypeCmp<i64, i64> = TypeCmp::Eq(TypeEq::NEW);
    /// 
    /// assert!(matches!(
    ///     TC_EQ.zip(NE),
    ///     TypeCmp::<(i64, u8), (i64, i8)>::Ne(_),
    /// ));
    /// 
    /// assert!(matches!(
    ///     TC_EQ.zip(EQ),
    ///     TypeCmp::<(i64, u16), (i64, u16)>::Eq(_),
    /// ));
    /// 
    /// assert!(matches!(
    ///     TC_EQ.zip(TC_EQ),
    ///     TypeCmp::<(i64, i64), (i64, i64)>::Eq(_),
    /// ));
    /// 
    /// assert!(matches!(
    ///     TC_EQ.zip(TC_NE),
    ///     TypeCmp::<(i64, u32), (i64, u64)>::Ne(_),
    /// ));
    /// ```
    pub const fn zip<A>(self, other: A) -> TypeCmp<(L, A::L), (R, A::R)> 
    where
        A: BaseTypeWitness,
    {
        let other = MetaBaseTypeWit::to_cmp(A::WITNESS, other);

        match (self, other) {
            (TypeCmp::Eq(tel), TypeCmp::Eq(ter)) => {
                TypeCmp::Eq(tel.zip(ter))
            }
            (TypeCmp::Ne(ne), _) => {
                TypeCmp::Ne(SomeTypeArgIsNe::A(TypeEq::NEW).zip2(ne, other))
            }
            (_, TypeCmp::Ne(ne)) => {
                TypeCmp::Ne(SomeTypeArgIsNe::B(TypeEq::NEW).zip2(self, ne))
            }
        }
    }

    /// Combines this `TypeCmp<L, R>` with two [`BaseTypeWitness`] types to produce a
    /// `TypeCmp<(L, A::L, B::L), (R, A::R, B::R)>`.
    /// 
    #[doc = alternative_docs!("zip3")]
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, TypeNe, type_ne};
    /// 
    /// 
    /// const NE: TypeNe<u8, i8> = type_ne!(u8, i8);
    /// const EQ: TypeEq<u16, u16> = TypeEq::NEW;
    /// const TC_NE: TypeCmp<u32, u64> = TypeCmp::Ne(type_ne!(u32, u64));
    /// const TC_EQ: TypeCmp<i64, i64> = TypeCmp::Eq(TypeEq::NEW);
    /// 
    /// assert!(matches!(
    ///     TC_EQ.zip3(EQ, NE),
    ///     TypeCmp::<(i64, u16, u8), (i64, u16, i8)>::Ne(_),
    /// ));
    /// 
    /// assert!(matches!(
    ///     TC_EQ.zip3(EQ, TC_EQ),
    ///     TypeCmp::<(i64, u16, i64), (i64, u16, i64)>::Eq(_),
    /// ));
    /// 
    /// assert!(matches!(
    ///     TC_EQ.zip3(NE, TC_NE),
    ///     TypeCmp::<(i64, u8, u32), (i64, i8, u64)>::Ne(_),
    /// ));
    /// ```
    pub const fn zip3<A, B>(self, arg0: A, arg1: B) -> TypeCmp<(L, A::L, B::L), (R, A::R, B::R)> 
    where
        A: BaseTypeWitness,
        A::L: Sized,
        A::R: Sized,
        B: BaseTypeWitness,
    {
        let arg0 = MetaBaseTypeWit::to_cmp(A::WITNESS, arg0);
        let arg1 = MetaBaseTypeWit::to_cmp(B::WITNESS, arg1);

        match (self, arg0, arg1) {
            (TypeCmp::Eq(te0), TypeCmp::Eq(te1), TypeCmp::Eq(te2)) => {
                TypeCmp::Eq(te0.zip3(te1, te2))
            }
            (TypeCmp::Ne(ne), _, _) => {
                TypeCmp::Ne(SomeTypeArgIsNe::A(TypeEq::NEW).zip3(ne, arg0, arg1))
            }
            (_, TypeCmp::Ne(ne), _) => {
                TypeCmp::Ne(SomeTypeArgIsNe::B(TypeEq::NEW).zip3(self, ne, arg1))
            }
            (_, _, TypeCmp::Ne(ne)) => {
                TypeCmp::Ne(SomeTypeArgIsNe::C(TypeEq::NEW).zip3(self, arg0, ne))
            }
        }
    }

    /// Combines this `TypeCmp<L, R>` with three [`BaseTypeWitness`] types to produce a
    /// `TypeCmp<(L, A::L, B::L, C::L), (R, A::R, B::R, C::R)>`.
    ///
    #[doc = alternative_docs!("zip4")]
    ///
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, TypeNe, type_ne};
    /// 
    /// 
    /// const NE: TypeNe<u8, i8> = type_ne!(u8, i8);
    /// const EQ: TypeEq<u16, u16> = TypeEq::NEW;
    /// const TC_NE: TypeCmp<u32, u64> = TypeCmp::Ne(type_ne!(u32, u64));
    /// const TC_EQ: TypeCmp<i64, i64> = TypeCmp::Eq(TypeEq::NEW);
    /// 
    /// assert!(matches!(
    ///     TC_EQ.zip4(EQ, NE, TC_NE),
    ///     TypeCmp::<(i64, u16, u8, u32), (i64, u16, i8, u64)>::Ne(_),
    /// ));
    /// 
    /// assert!(matches!(
    ///     TC_EQ.zip4(EQ, TC_EQ, EQ),
    ///     TypeCmp::<(i64, u16, i64, u16), (i64, u16, i64, u16)>::Eq(_),
    /// ));
    /// ```
    pub const fn zip4<A, B, C>(
        self, 
        arg0: A, 
        arg1: B, 
        arg2: C,
    ) -> TypeCmp<(L, A::L, B::L, C::L), (R, A::R, B::R, C::R)> 
    where
        A: BaseTypeWitness,
        A::L: Sized,
        A::R: Sized,
        B: BaseTypeWitness,
        B::L: Sized,
        B::R: Sized,
        C: BaseTypeWitness,
    {
        let arg0 = MetaBaseTypeWit::to_cmp(A::WITNESS, arg0);
        let arg1 = MetaBaseTypeWit::to_cmp(B::WITNESS, arg1);
        let arg2 = MetaBaseTypeWit::to_cmp(C::WITNESS, arg2);

        match (self, arg0, arg1, arg2) {
            (TypeCmp::Eq(te0), TypeCmp::Eq(te1), TypeCmp::Eq(te2), TypeCmp::Eq(te3)) => {
                TypeCmp::Eq(te0.zip4(te1, te2, te3))
            }
            (TypeCmp::Ne(ne), _, _, _) => {
                TypeCmp::Ne(SomeTypeArgIsNe::A(TypeEq::NEW).zip4(ne, arg0, arg1, arg2))
            }
            (_, TypeCmp::Ne(ne), _, _) => {
                TypeCmp::Ne(SomeTypeArgIsNe::B(TypeEq::NEW).zip4(self, ne, arg1, arg2))
            }
            (_, _, TypeCmp::Ne(ne), _) => {
                TypeCmp::Ne(SomeTypeArgIsNe::C(TypeEq::NEW).zip4(self, arg0, ne, arg2))
            }
            (_, _, _, TypeCmp::Ne(ne)) => {
                TypeCmp::Ne(SomeTypeArgIsNe::D(TypeEq::NEW).zip4(self, arg0, arg1, ne))
            }
        }
    }
}


// using this instead of `mod extra_type_cmp_methods;`
// to document the impls in the submodule below the constructors.
include!{"./type_cmp/extra_type_cmp_methods.rs"}


impl<L: ?Sized, R: ?Sized> Copy for TypeCmp<L, R> {}

impl<L: ?Sized, R: ?Sized> Clone for TypeCmp<L, R> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<L: ?Sized, R: ?Sized> Debug for TypeCmp<L, R> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            TypeCmp::Eq(x) => Debug::fmt(x, f),
            TypeCmp::Ne(x) => Debug::fmt(x, f),
        }
    }
}

impl<L: ?Sized, R: ?Sized> PartialEq for TypeCmp<L, R> {
    fn eq(&self, other: &Self) -> bool {
        self.is_eq() == other.is_eq()
    }
}

impl<L: ?Sized, R: ?Sized> PartialOrd for TypeCmp<L, R> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        self.is_eq().partial_cmp(&other.is_eq())
    }
}

impl<L: ?Sized, R: ?Sized> Ord for TypeCmp<L, R> {
    fn cmp(&self, other: &Self) -> Ordering {
        self.is_eq().cmp(&other.is_eq())
    }
}

impl<L: ?Sized, R: ?Sized> Eq for TypeCmp<L, R> {}

impl<L: ?Sized, R: ?Sized> Hash for TypeCmp<L, R> {
    fn hash<H>(&self, state: &mut H)
    where H: Hasher
    {
        match self {
            TypeCmp::Eq(x) => Hash::hash(x, state),
            TypeCmp::Ne(x) => Hash::hash(x, state),
        }
    }
}








