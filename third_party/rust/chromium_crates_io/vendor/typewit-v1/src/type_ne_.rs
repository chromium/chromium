use core::{
    any::{Any, TypeId},
    cmp::{Ordering, Eq, Ord, PartialEq, PartialOrd},
    hash::{Hash, Hasher},
    fmt::{self, Debug},
};

use crate::TypeEq;

#[cfg(feature = "rust_1_61")]
use crate::{BaseTypeWitness, SomeTypeArgIsNe};


/// Marker type, for constructing `TypeNe` in [`TypeNe::with_fn`] constructor.
pub enum LeftArg {}

/// Marker type, for constructing `TypeNe` in [`TypeNe::with_fn`] constructor.
pub enum RightArg {}



pub use self::type_ne_::TypeNe;

mod type_ne_ {
    use core::marker::PhantomData;

    /// Value-level proof that `L` is a different type to `R`
    /// 
    /// The opposite of [`TypeEq`](crate::TypeEq).
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{const_marker::Usize, TypeNe};
    /// 
    /// assert_eq!(
    ///     array_ref_chunks(&[3, 5, 8, 13, 21, 34, 55], AssertNotZero::V), 
    ///     Chunks {chunks: vec![&[3, 5, 8], &[13, 21, 34]], tail: &[55]}
    /// );
    /// 
    /// 
    /// fn array_ref_chunks<T, const LEN: usize>(
    ///     slice: &[T], 
    ///     _not_zero: TypeNe<Usize<LEN>, Usize<0>>,
    /// ) -> Chunks<'_, T, LEN> {
    ///     let mut chunks = slice.chunks_exact(LEN);
    /// 
    ///     Chunks {
    ///         chunks: chunks.by_ref().map(|c| <&[T; LEN]>::try_from(c).unwrap()).collect(),
    ///         tail: chunks.remainder(),
    ///     }
    /// }
    /// 
    /// #[derive(Debug, PartialEq, Eq)]
    /// struct Chunks<'a, T, const LEN: usize> {
    ///     chunks: Vec<&'a [T; LEN]>,
    ///     tail: &'a [T],
    /// }
    /// 
    /// struct AssertNotZero<const N: usize>;
    /// 
    /// impl<const N: usize> AssertNotZero<N> {
    ///     const V: TypeNe<Usize<N>, Usize<0>> = Usize::<N>.equals(Usize::<0>).unwrap_ne();
    /// }
    /// 
    /// ```
    /// 
    /// If you attempt to pass `0` as the length of the array chunks,
    /// you'll get this compile-time error:
    /// ```text
    /// error[E0080]: evaluation of `main::_doctest_main_src_type_ne_rs_41_0::AssertNotZero::<0>::V` failed
    ///   --> src/type_ne.rs:71:43
    ///    |
    /// 33 |     const V: TypeNe<Usize<N>, Usize<0>> = Usize::<N>.equals(Usize::<0>).unwrap_ne();
    ///    |                                           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ the evaluated program panicked at 'called `TypeCmp::unwrap_ne` on a `TypeEq` value', src/type_ne.rs:33:73
    /// 
    /// error[E0080]: erroneous constant used
    ///  --> src/type_ne.rs:45:50
    ///   |
    /// 7 |     array_ref_chunks(&[3, 5, 8, 13, 21, 34, 55], AssertNotZero::<0>::V), 
    ///   |                                                  ^^^^^^^^^^^^^^^^^^^^^ referenced constant has errors
    /// 
    /// ```
    pub struct TypeNe<L: ?Sized, R: ?Sized>(PhantomData<TypeNeHelper<L, R>>);

    // Declared to work around this error in old Rust versions:
    // > error[E0658]: function pointers cannot appear in constant functions
    struct TypeNeHelper<L: ?Sized, R: ?Sized>(
        #[allow(dead_code)]
        fn(PhantomData<L>) -> PhantomData<L>,
        #[allow(dead_code)]
        fn(PhantomData<R>) -> PhantomData<R>,
    );

    impl<L: ?Sized, R: ?Sized> TypeNe<L, R> {
        /// Constructs a `TypeNe<L, R>`.
        ///
        /// # Safety
        ///
        /// You must ensure that `L != R`.
        ///
        #[inline(always)]
        pub const unsafe fn new_unchecked() -> TypeNe<L, R> {
            TypeNe(PhantomData)
        }
    }


}

impl TypeNe<(), ()> {
    /// Constructs a `TypeNe` by mapping from a 
    /// `TypeNe<`[`LeftArg`]`, `[`RightArg`]`>` 
    /// with an [injective type-level function](crate::InjTypeFn).
    /// 
    /// # Alternative
    /// 
    /// The [`type_ne`](macro@crate::type_ne) macro can be used as syntactic sugar 
    /// for calling this constructor with a one-off type-level function.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::type_ne::{TypeNe, LeftArg, RightArg};
    /// 
    /// const NE: TypeNe<Option<String>, Vec<u16>> = TypeNe::with_fn(MakeNe::NEW);
    /// 
    /// typewit::inj_type_fn! {
    ///     struct MakeNe<T, U>;
    /// 
    ///     impl LeftArg => Option<T>;
    ///     impl RightArg => Vec<U>;
    /// }
    /// ```
    pub const fn with_fn<F>(
        _func: F,
    ) -> TypeNe<CallInjFn<InvokeAlias<F>, LeftArg>, CallInjFn<InvokeAlias<F>, RightArg>>
    where
        InvokeAlias<F>: InjTypeFn<LeftArg> + InjTypeFn<RightArg>
    {
        core::mem::forget(_func);

        // SAFETY: LeftArg isn't RightArg, dummy.
        let this: TypeNe<LeftArg, RightArg> = unsafe { TypeNe::new_unchecked() };

        projected_type_cmp!{this, LeftArg, RightArg, InvokeAlias<F>}
    }
}


impl<L: ?Sized, R: ?Sized> TypeNe<L, R> {
    /// Constructs `TypeNe<L, R>` if `L != R`, otherwise returns None.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::TypeNe;
    /// 
    /// 
    /// let _ne: TypeNe<u8, i8> = TypeNe::with_any().unwrap();
    /// 
    /// assert!(TypeNe::<u8, u8>::with_any().is_none());
    /// 
    /// ```
    #[deprecated = concat!(
        "fallout of `https://github.com/rust-lang/rust/issues/97156`,",
        "`TypeId::of::<L>() != TypeId::of::<R>()` does not imply `L != R`"
    )]
    pub fn with_any() -> Option<Self>
    where
        L: Sized + Any,
        R: Sized + Any,
    {
        if TypeId::of::<L>() != TypeId::of::<R>() {
            // SAFETY: unsound for the deprecated reason
            unsafe { Some(TypeNe::new_unchecked()) }
        } else {
            None
        }
    }

    /// Converts this `TypeNe` into a [`TypeCmp`](crate::TypeCmp)
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeNe, type_ne};
    /// 
    /// const NE: TypeNe<u8, i8> = type_ne!(u8, i8);
    /// const TC: TypeCmp<u8, i8> = NE.to_cmp();
    /// 
    /// assert!(matches!(TC, TypeCmp::Ne(_)));
    /// ```
    #[inline(always)]
    pub const fn to_cmp(self) -> crate::TypeCmp<L, R> {
        crate::TypeCmp::Ne(self)
    }

    /// Swaps the type arguments of this `TypeNe`
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeNe, type_ne};
    /// 
    /// const NE: TypeNe<u8, i8> = type_ne!(u8, i8);
    /// 
    /// const N3: TypeNe<i8, u8> = NE.flip();
    /// 
    /// ```
    pub const fn flip(self: TypeNe<L, R>) -> TypeNe<R, L> {
        // SAFETY: type inequality is commutative
        unsafe { TypeNe::<R, L>::new_unchecked() }
    }

    /// Joins a proof of `L != R` with a proof of `J == L`,
    /// creating a proof of `J != R`.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeEq, TypeNe, type_ne};
    /// 
    /// const NE: TypeNe<str, [u8]> = type_ne!(str, [u8]);
    /// 
    /// const fn foo<A: ?Sized>(eq: TypeEq<A, str>) {
    ///     let _ne: TypeNe<A, [u8]> = NE.join_left(eq);
    /// }
    /// ```
    pub const fn join_left<J: ?Sized>(self: TypeNe<L, R>, _eq: TypeEq<J, L>) -> TypeNe<J, R> {
        // SAFETY: (L != R, J == L) implies J != R
        unsafe { TypeNe::<J, R>::new_unchecked() }
    }

    /// Joins a proof of `L != R` with a proof of `R == J`,
    /// creating a proof of `L != J`.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeEq, TypeNe, type_ne};
    /// 
    /// const NE: TypeNe<String, Vec<u8>> = type_ne!(String, Vec<u8>);
    /// 
    /// const fn foo<A>(eq: TypeEq<Vec<u8>, A>) {
    ///     let _ne: TypeNe<String, A> = NE.join_right(eq);
    /// }
    /// ```
    pub const fn join_right<J: ?Sized>(self: TypeNe<L, R>, _eq: TypeEq<R, J>) -> TypeNe<L, J> {
        // SAFETY: (L != R, R == J) implies L != J
        unsafe { TypeNe::<L, J>::new_unchecked() }
    }
}

#[cfg(feature = "rust_1_61")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_61")))]
impl<L, R> TypeNe<L, R> {
    /// Combines this `TypeNe<L, R>` with an 
    /// [`A: BaseTypeWitness`](BaseTypeWitness) to produce a
    /// `TypeNe<(L, A::L), (R, A::R)>`.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, TypeNe, type_ne};
    /// 
    /// const NE: TypeNe<u8, i8> = type_ne!(u8, i8);
    /// const EQ: TypeEq<u16, u16> = TypeEq::NEW;
    /// const TC: TypeCmp<u32, u64> = TypeCmp::Ne(type_ne!(u32, u64));
    /// 
    /// let _: TypeNe<(u8, i8), (i8, u8)> = NE.zip(NE.flip());
    /// let _: TypeNe<(u8, u16), (i8, u16)> = NE.zip(EQ);
    /// let _: TypeNe<(u8, u32), (i8, u64)> = NE.zip(TC);
    /// 
    /// ```
    pub const fn zip<A>(
        self: TypeNe<L, R>,
        other: A,
    ) -> TypeNe<(L, A::L), (R, A::R)> 
    where
        A: BaseTypeWitness,
    {
        SomeTypeArgIsNe::A(TypeEq::NEW).zip2(self, other)
    }

    /// Combines this `TypeNe<L, R>` with 
    /// two [`BaseTypeWitness`](BaseTypeWitness)es to produce a
    /// `TypeNe<(L, A::L, B::L), (R, A::R, B::R)>`.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, TypeNe, type_ne};
    /// 
    /// const NE: TypeNe<u8, i8> = type_ne!(u8, i8);
    /// const EQ: TypeEq<u16, u16> = TypeEq::NEW;
    /// const TC: TypeCmp<u32, u64> = TypeCmp::Ne(type_ne!(u32, u64));
    /// 
    /// let _: TypeNe<(u8, i8, u8), (i8, u8, i8)> = NE.zip3(NE.flip(), NE);
    /// let _: TypeNe<(u8, u16, u16), (i8, u16, u16)> = NE.zip3(EQ, EQ.flip());
    /// let _: TypeNe<(u8, u32, u64), (i8, u64, u32)> = NE.zip3(TC, TC.flip());
    /// 
    /// ```
    pub const fn zip3<A, B>(
        self: TypeNe<L, R>,
        other1: A,
        other2: B,
    ) -> TypeNe<(L, A::L, B::L), (R, A::R, B::R)> 
    where
        A: BaseTypeWitness,
        B: BaseTypeWitness,
        A::L: Sized,
        A::R: Sized,
    {
        SomeTypeArgIsNe::A(TypeEq::NEW).zip3(self, other1, other2)
    }

    /// Combines this `TypeNe<L, R>` with 
    /// three [`BaseTypeWitness`](BaseTypeWitness)es to produce a
    /// `TypeNe<(L, A::L, B::L, C::L), (R, A::R, B::R, C::R)> `.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, TypeNe, type_ne};
    /// 
    /// const NE: TypeNe<u8, i8> = type_ne!(u8, i8);
    /// const EQ: TypeEq<u16, u16> = TypeEq::NEW;
    /// const TC: TypeCmp<u32, u64> = TypeCmp::Ne(type_ne!(u32, u64));
    /// 
    /// let _: TypeNe<(u8, i8, u8, i8), (i8, u8, i8, u8)> = NE.zip4(NE.flip(), NE, NE.flip());
    /// let _: TypeNe<(u8, u16, u16, u16), (i8, u16, u16, u16)> = NE.zip4(EQ, EQ.flip(), EQ);
    /// let _: TypeNe<(u8, u32, u64, u32), (i8, u64, u32, u64)> = NE.zip4(TC, TC.flip(), TC);
    /// 
    /// ```
    pub const fn zip4<A, B, C>(
        self: TypeNe<L, R>,
        other1: A,
        other2: B,
        other3: C,
    ) -> TypeNe<(L, A::L, B::L, C::L), (R, A::R, B::R, C::R)> 
    where
        A: BaseTypeWitness,
        B: BaseTypeWitness,
        C: BaseTypeWitness,
        A::L: Sized,
        A::R: Sized,
        B::L: Sized,
        B::R: Sized,
    {
        SomeTypeArgIsNe::A(TypeEq::NEW).zip4(self, other1, other2, other3)
    }
}



// using this instead of `mod extra_type_ne_methods;`
// to document the impls in the submodule below the constructors.
include!{"./type_ne/extra_type_ne_methods.rs"}



impl<L: ?Sized, R: ?Sized> Copy for TypeNe<L, R> {}

impl<L: ?Sized, R: ?Sized> Clone for TypeNe<L, R> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<L: ?Sized, R: ?Sized> Debug for TypeNe<L, R> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("TypeNe")
    }
}

impl<L: ?Sized, R: ?Sized> PartialEq for TypeNe<L, R> {
    fn eq(&self, _: &Self) -> bool {
        true
    }
}

impl<L: ?Sized, R: ?Sized> PartialOrd for TypeNe<L, R> {
    fn partial_cmp(&self, _: &Self) -> Option<Ordering> {
        Some(Ordering::Equal)
    }
}

impl<L: ?Sized, R: ?Sized> Ord for TypeNe<L, R> {
    fn cmp(&self, _: &Self) -> Ordering {
        Ordering::Equal
    }
}

impl<L: ?Sized, R: ?Sized> Eq for TypeNe<L, R> {}

impl<L: ?Sized, R: ?Sized> Hash for TypeNe<L, R> {
    fn hash<H>(&self, _state: &mut H)
    where H: Hasher
    {}
}



