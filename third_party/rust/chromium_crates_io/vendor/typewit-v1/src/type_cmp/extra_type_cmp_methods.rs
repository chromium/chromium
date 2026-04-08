use crate::type_fn::{InjTypeFn, InvokeAlias, CallInjFn, UncallFn};

#[cfg(feature = "alloc")]
use alloc::boxed::Box;


/// 
/// # Why `InjTypeFn`
/// 
/// Both [`map`](Self::map) and [`project`](Self::project) 
/// require that the function is [injective]
/// so that `TypeCmp`'s arguments don't change from
/// being equal to unequal or viceversa.
/// 
/// [injective]: mod@crate::type_fn#injective
/// 
impl<L: ?Sized, R: ?Sized> TypeCmp<L, R> {
    /// Maps the type arguments of this `TypeCmp`
    /// by using the `F` [injective type-level function](crate::InjTypeFn).
    /// 
    /// Use this function over [`project`](Self::project) 
    /// if you want the type of the passed in function to be inferred.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, TypeNe, inj_type_fn, type_ne};
    /// 
    /// use std::num::Wrapping;
    /// 
    /// 
    /// const EQ: TypeCmp<u8, u8> = TypeEq::NEW.to_cmp();
    /// assert!(matches!(EQ.map(WrappingFn), TypeCmp::<Wrapping<u8>, Wrapping<u8>>::Eq(_)));
    /// 
    /// const NE: TypeCmp<i8, u8> = type_ne!(i8, u8).to_cmp();
    /// assert!(matches!(NE.map(WrappingFn), TypeCmp::<Wrapping<i8>, Wrapping<u8>>::Ne(_)));
    /// 
    /// 
    /// inj_type_fn!{
    ///     struct WrappingFn;
    /// 
    ///     impl<T> T => Wrapping<T>
    /// }
    /// ```
    pub const fn map<F>(
        self: TypeCmp<L, R>,
        func: F,
    ) -> TypeCmp<CallInjFn<InvokeAlias<F>, L>, CallInjFn<InvokeAlias<F>, R>> 
    where
        InvokeAlias<F>: InjTypeFn<L> + InjTypeFn<R>
    {
        match self {
            TypeCmp::Eq(te) => TypeCmp::Eq(te.map::<F>(func)),
            TypeCmp::Ne(te) => TypeCmp::Ne(te.map::<F>(func)),
        }
    }

    /// Maps the type arguments of this `TypeCmp`
    /// by using the `F` [injective type-level function](crate::InjTypeFn).
    /// 
    /// Use this function over [`map`](Self::map) 
    /// if you want to specify the type of the passed in function explicitly.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, TypeNe, inj_type_fn, type_ne};
    /// 
    /// use std::mem::ManuallyDrop as ManDrop;
    /// 
    /// 
    /// const EQ: TypeCmp<u8, u8> = TypeEq::NEW.to_cmp();
    /// assert!(matches!(EQ.project::<ManDropFn>(), TypeCmp::<ManDrop<u8>, ManDrop<u8>>::Eq(_)));
    /// 
    /// const NE: TypeCmp<i8, u8> = type_ne!(i8, u8).to_cmp();
    /// assert!(matches!(NE.project::<ManDropFn>(), TypeCmp::<ManDrop<i8>, ManDrop<u8>>::Ne(_)));
    /// 
    /// 
    /// inj_type_fn!{
    ///     struct ManDropFn;
    /// 
    ///     impl<T> T => ManDrop<T>
    /// }
    /// ```
    pub const fn project<F>(
        self: TypeCmp<L, R>,
    ) -> TypeCmp<CallInjFn<InvokeAlias<F>, L>, CallInjFn<InvokeAlias<F>, R>> 
    where
        InvokeAlias<F>: InjTypeFn<L> + InjTypeFn<R>
    {
        match self {
            TypeCmp::Eq(te) => TypeCmp::Eq(te.project::<F>()),
            TypeCmp::Ne(te) => TypeCmp::Ne(te.project::<F>()),
        }
    }

    /// Maps the type arguments of this `TypeCmp`
    /// by using the [reversed](crate::RevTypeFn) 
    /// version of the `F` type-level function.
    /// 
    /// Use this function over [`unproject`](Self::unproject) 
    /// if you want the type of the passed in function to be inferred.
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, TypeNe, inj_type_fn, type_ne};
    /// 
    /// use std::num::Wrapping;
    /// 
    /// 
    /// const EQ: TypeCmp<Wrapping<u8>, Wrapping<u8>> = TypeEq::NEW.to_cmp();
    /// 
    /// assert!(matches!(EQ.unmap(WrappingFn), TypeCmp::<u8, u8>::Eq(_)));
    /// 
    /// const NE: TypeCmp<Wrapping<i8>, Wrapping<u8>> = 
    ///     type_ne!(Wrapping<i8>, Wrapping<u8>).to_cmp();
    /// 
    /// assert!(matches!(NE.unmap(WrappingFn), TypeCmp::<i8, u8>::Ne(_)));
    /// 
    /// 
    /// inj_type_fn!{
    ///     struct WrappingFn;
    /// 
    ///     impl<T> T => Wrapping<T>
    /// }
    /// ```
    pub const fn unmap<F>(
        self,
        func: F,
    ) -> TypeCmp<UncallFn<InvokeAlias<F>, L>, UncallFn<InvokeAlias<F>, R>>
    where
        InvokeAlias<F>: crate::RevTypeFn<L> + crate::RevTypeFn<R>
    {
        match self {
            TypeCmp::Eq(te) => TypeCmp::Eq(te.unmap::<F>(func)),
            TypeCmp::Ne(te) => TypeCmp::Ne(te.unmap::<F>(func)),
        }
    }
    /// Maps the type arguments of this `TypeCmp`
    /// by using the [reversed](crate::RevTypeFn) 
    /// version of the `F` type-level function.
    /// 
    /// Use this function over [`unmap`](Self::unmap) 
    /// if you want to specify the type of the passed in function explicitly.
    /// 
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, TypeNe, inj_type_fn, type_ne};
    /// 
    /// use std::mem::MaybeUninit as MaybeUn;
    /// 
    /// 
    /// const EQ: TypeCmp<MaybeUn<u8>, MaybeUn<u8>> = TypeEq::NEW.to_cmp();
    /// 
    /// assert!(matches!(EQ.unproject::<MaybeUnFn>(), TypeCmp::<u8, u8>::Eq(_)));
    /// 
    /// const NE: TypeCmp<MaybeUn<i8>, MaybeUn<u8>> = 
    ///     type_ne!(MaybeUn<i8>, MaybeUn<u8>).to_cmp();
    /// 
    /// assert!(matches!(NE.unproject::<MaybeUnFn>(), TypeCmp::<i8, u8>::Ne(_)));
    /// 
    /// 
    /// inj_type_fn!{
    ///     struct MaybeUnFn;
    /// 
    ///     impl<T> T => MaybeUn<T>
    /// }
    /// ```
    pub const fn unproject<F>(
        self,
    ) -> TypeCmp<UncallFn<InvokeAlias<F>, L>, UncallFn<InvokeAlias<F>, R>>
    where
        InvokeAlias<F>: crate::RevTypeFn<L> + crate::RevTypeFn<R>
    {
        match self {
            TypeCmp::Eq(te) => TypeCmp::Eq(te.unproject::<F>()),
            TypeCmp::Ne(te) => TypeCmp::Ne(te.unproject::<F>()),
        }
    }

    /// Converts a `TypeCmp<L, R>` to `TypeCmp<&L, &R>`
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, TypeNe, type_ne};
    /// 
    /// const EQ: TypeCmp<u8, u8> = TypeEq::NEW.to_cmp();
    /// assert!(matches!(EQ.in_ref(), TypeCmp::<&u8, &u8>::Eq(_)));
    /// 
    /// const NE: TypeCmp<i8, u8> = type_ne!(i8, u8).to_cmp();
    /// assert!(matches!(NE.in_ref(), TypeCmp::<&i8, &u8>::Ne(_)));
    /// 
    /// ```
    pub const fn in_ref<'a>(self) -> TypeCmp<&'a L, &'a R> {
        match self {
            TypeCmp::Eq(te) => TypeCmp::Eq(te.in_ref()),
            TypeCmp::Ne(te) => TypeCmp::Ne(te.in_ref()),
        }
    }

    crate::utils::conditionally_const!{
        feature = "rust_1_83";

        /// Converts a `TypeCmp<L, R>` to `TypeCmp<&mut L, &mut R>`
        /// 
        /// # Constness
        /// 
        /// This requires the `"rust_1_83"` crate feature to be a `const fn`.
        /// 
        /// # Example
        /// 
        /// ```rust
        /// use typewit::{TypeCmp, TypeEq, TypeNe, type_ne};
        /// 
        /// const EQ: TypeCmp<u8, u8> = TypeEq::NEW.to_cmp();
        /// assert!(matches!(EQ.in_mut(), TypeCmp::<&mut u8, &mut u8>::Eq(_)));
        /// 
        /// const NE: TypeCmp<i8, u8> = type_ne!(i8, u8).to_cmp();
        /// assert!(matches!(NE.in_mut(), TypeCmp::<&mut i8, &mut u8>::Ne(_)));
        /// 
        /// ```
        pub fn in_mut['a](self) -> TypeCmp<&'a mut L, &'a mut R> {
            match self {
                TypeCmp::Eq(te) => TypeCmp::Eq(te.in_mut()),
                TypeCmp::Ne(te) => TypeCmp::Ne(te.in_mut()),
            }
        }
    }

    /// Converts a `TypeCmp<L, R>` to `TypeCmp<Box<L>, Box<R>>`
    /// 
    /// # Example
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, TypeNe, type_ne};
    /// 
    /// const EQ: TypeCmp<u8, u8> = TypeEq::NEW.to_cmp();
    /// assert!(matches!(EQ.in_box(), TypeCmp::<Box<u8>, Box<u8>>::Eq(_)));
    /// 
    /// const NE: TypeCmp<i8, u8> = type_ne!(i8, u8).to_cmp();
    /// assert!(matches!(NE.in_box(), TypeCmp::<Box<i8>, Box<u8>>::Ne(_)));
    /// 
    /// ```
    #[cfg(feature = "alloc")]
    #[cfg_attr(feature = "docsrs", doc(cfg(feature = "alloc")))]
    pub const fn in_box(self) -> TypeCmp<Box<L>, Box<R>> {
        match self {
            TypeCmp::Eq(te) => TypeCmp::Eq(te.in_box()),
            TypeCmp::Ne(te) => TypeCmp::Ne(te.in_box()),
        }
    }
}



#[cfg(feature = "rust_1_61")]
use crate::const_marker::Usize;

#[cfg(feature = "rust_1_61")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_61")))]
impl<L, R> TypeCmp<L, R> {
    /// Combines `TypeCmp<L, R>` and a
    /// `O:`[`BaseTypeWitness`]`<L = Usize<UL>, R = Usize<UR>>`
    /// into `TypeCmp<[L; UL], [R; UR]>`
    /// 
    /// [`BaseTypeWitness`]: crate::BaseTypeWitness
    /// 
    #[doc = alternative_docs!("in_array")]
    /// 
    /// # Example
    /// 
    /// ### Basic
    /// 
    /// ```rust
    /// use typewit::{
    ///     const_marker::Usize,
    ///     TypeCmp, TypeEq, TypeNe,
    ///     type_ne,
    /// };
    /// 
    /// let cmp_eq_ty: TypeCmp<i32, i32> = TypeCmp::Eq(TypeEq::NEW);
    /// let cmp_ne_ty: TypeCmp<i64, u64> = TypeCmp::Ne(type_ne!(i64, u64));
    /// 
    /// let eq_len: TypeEq<Usize<0>, Usize<0>> = TypeEq::NEW;
    /// let ne_len: TypeNe<Usize<1>, Usize<2>> = Usize.equals(Usize).unwrap_ne();
    /// let cmp_eq_len: TypeCmp<Usize<3>, Usize<3>> = Usize.equals(Usize);
    /// let cmp_ne_len: TypeCmp<Usize<5>, Usize<8>> = Usize.equals(Usize);
    /// 
    /// assert!(matches!(cmp_eq_ty.in_array(eq_len), TypeCmp::<[i32; 0], [i32; 0]>::Eq(_)));
    /// assert!(matches!(cmp_eq_ty.in_array(ne_len), TypeCmp::<[i32; 1], [i32; 2]>::Ne(_)));
    /// assert!(matches!(cmp_eq_ty.in_array(cmp_eq_len), TypeCmp::<[i32; 3], [i32; 3]>::Eq(_)));
    /// assert!(matches!(cmp_eq_ty.in_array(cmp_ne_len), TypeCmp::<[i32; 5], [i32; 8]>::Ne(_)));
    /// 
    /// assert!(matches!(cmp_ne_ty.in_array(eq_len), TypeCmp::<[i64; 0], [u64; 0]>::Ne(_)));
    /// assert!(matches!(cmp_ne_ty.in_array(ne_len), TypeCmp::<[i64; 1], [u64; 2]>::Ne(_)));
    /// assert!(matches!(cmp_ne_ty.in_array(cmp_eq_len), TypeCmp::<[i64; 3], [u64; 3]>::Ne(_)));
    /// assert!(matches!(cmp_ne_ty.in_array(cmp_ne_len), TypeCmp::<[i64; 5], [u64; 8]>::Ne(_)));
    /// ```
    pub const fn in_array<O, const UL: usize, const UR: usize>(
        self,
        other: O,
    ) -> TypeCmp<[L; UL], [R; UR]> 
    where
        O: BaseTypeWitness<L = Usize<UL>, R = Usize<UR>>
    {
        use crate::type_fn::PairToArrayFn as PTAF;

        let other = MetaBaseTypeWit::to_cmp(O::WITNESS, other);

        match (self, other) {
            (TypeCmp::Eq(tel), TypeCmp::Eq(ter)) => {
                TypeCmp::Eq(tel.in_array(ter))
            }
            (TypeCmp::Ne(ne), _) => {
                TypeCmp::Ne(SomeTypeArgIsNe::A(TypeEq::NEW).zip2(ne, other).project::<PTAF>())
            }
            (_, TypeCmp::Ne(ne)) => {
                TypeCmp::Ne(SomeTypeArgIsNe::B(TypeEq::NEW).zip2(self, ne).project::<PTAF>())
            }
        }
    }
}