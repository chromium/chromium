use crate::{
    type_constructors::{
        BaseTypeWitnessTc,
        BaseTypeWitnessReparam,
        MapBaseTypeWitness,
        TcToBaseTypeWitness,
        TcTypeCmp, TcTypeEq, TcTypeNe,
    },
    const_marker::Usize,
    BaseTypeWitness,
    MetaBaseTypeWit,
    MetaBaseTypeWit as MBTW,
    HasTypeWitness, SomeTypeArgIsNe,
    TypeCmp, TypeEq, TypeNe
};


//////////////////////////////////////////////////////////////////////

macro_rules! return_type_docs {() => {concat!(
    "# Return type\n",
    "\n",
    "The return type depends on the types of the arguments:\n",
    "- if all arguments are [`TypeEq`], this returns a `TypeEq` \n",
    "- if any argument is a [`TypeNe`], this returns a `TypeNe` \n",
    "- if any argument is a [`TypeCmp`] and no argument is a `TypeNe`,",
    " this returns a `TypeCmp` \n",
)}}

//////////////////////////////////////////////////////////////////////

#[doc(hidden)]
pub trait ZipTc: 'static + Copy {
    type Output: BaseTypeWitnessTc;
}

#[doc(hidden)]
pub type ZipTcOut<TupleOfTc> = <TupleOfTc as ZipTc>::Output;


impl ZipTc for (TcTypeEq, TcTypeEq) {
    type Output = TcTypeEq;
}
impl ZipTc for (TcTypeEq, TcTypeNe) {
    type Output = TcTypeNe;
}
impl ZipTc for (TcTypeEq, TcTypeCmp) {
    type Output = TcTypeCmp;
}

impl<B: BaseTypeWitnessTc> ZipTc for (TcTypeNe, B) {
    type Output = TcTypeNe;
}

impl ZipTc for (TcTypeCmp, TcTypeEq) {
    type Output = TcTypeCmp;
}

impl ZipTc for (TcTypeCmp, TcTypeNe) {
    type Output = TcTypeNe;
}

impl ZipTc for (TcTypeCmp, TcTypeCmp) {
    type Output = TcTypeCmp;
}

impl<A, B, C> ZipTc for (A, B, C)
where
    A: BaseTypeWitnessTc,
    B: BaseTypeWitnessTc,
    C: BaseTypeWitnessTc,
    (A, B): ZipTc,
    (ZipTcOut<(A, B)>, C): ZipTc,
{
    type Output = ZipTcOut<(ZipTcOut<(A, B)>, C)>;
}

impl<A, B, C, D> ZipTc<> for (A, B, C, D)
where
    A: BaseTypeWitnessTc,
    B: BaseTypeWitnessTc,
    C: BaseTypeWitnessTc,
    D: BaseTypeWitnessTc,
    (A, B): ZipTc,
    (C, D): ZipTc,
    (ZipTcOut<(A, B)>, ZipTcOut<(C, D)>): ZipTc,
{
    type Output = ZipTcOut<(ZipTcOut<(A, B)>, ZipTcOut<(C, D)>)>;
}


//////////////////////////////////////////////////////////////////////

macro_rules! declare_zip_items {
    (
        [$first_typa:ident ($($middle_typa:ident)*) $end_typa:ident] 

        $(#[$fn_attr:meta])*
        fn $fn_name:ident($fn_param0:ident $(, $fn_param_rem:ident)*);

        $($rem:tt)*
    ) => {
        __declare_zip_items!{
            [$first_typa ($($middle_typa)*) $end_typa] 
            [$first_typa $($middle_typa)* $end_typa] 

            $(#[$fn_attr])*
            fn $fn_name ($fn_param0 $(, $fn_param_rem)*) ($fn_param0, $(, $fn_param_rem)*);

            $($rem)*
        }
    }
}
macro_rules! __declare_zip_items {
    (
        [$first_typa:ident ($($middle_typa:ident)*) $end_typa:ident] 
        [$($ty_params:ident)*]

        $(#[$fn_attr:meta])*
        fn $fn_name:ident($($fn_param:ident),*) ($fn_param0:ident, $(, $fn_param_rem:ident)*);

        $(#[$type_alias_attr:meta])*
        type $type_alias:ident;

        $(#[$trait_attr:meta])*
        trait $trait:ident;

        enum $ty_wit:ident ($($arg_wit:ident),*);


        zip_method = $zip_method:ident;
        count = $count:tt
    ) => {
        #[doc = concat!(
            "The type returned by zipping ",
            $count,
            " [`BaseTypeWitness`]es.",
        )]
        $(#[$type_alias_attr])*
        #[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_65")))]
        pub type $type_alias<$($ty_params),*> = 
            <$first_typa as $trait<$($middle_typa,)* $end_typa>>::Output;

        #[doc = concat!(
            "Computes the type that the [`",
            stringify!($fn_name),
            "`]  function returns, ",
        )]
        $(#[$trait_attr])*
        #[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_65")))]
        pub trait $trait<
            $($middle_typa: BaseTypeWitness,)*
            $end_typa: BaseTypeWitness,
        >: BaseTypeWitness 
        where
            Self::L: Sized,
            Self::R: Sized,
            $(
                $middle_typa::L: Sized,
                $middle_typa::R: Sized,
            )*
        {
            #[doc = concat!(
                "The the type returned by zipping ",
                $count, " [`BaseTypeWitness`] types"
            )]
            type Output: BaseTypeWitness<
                L = (Self::L, $($middle_typa::L,)* $end_typa::L),
                R = (Self::R, $($middle_typa::R,)* $end_typa::R),
            >;
        }

        impl<$($ty_params,)*> $trait<$($middle_typa,)* $end_typa> for $first_typa
        where
            $($ty_params: BaseTypeWitness,)*
            ($($ty_params::TypeCtor,)*): ZipTc,

            Self::L: Sized,
            Self::R: Sized,
            $(
                $middle_typa::L: Sized,
                $middle_typa::R: Sized,
            )*
            
        {
            type Output = 
                TcToBaseTypeWitness<
                    ZipTcOut<($($ty_params::TypeCtor,)*)>,
                    ($($ty_params::L,)*),
                    ($($ty_params::R,)*),
                >;
        }


        enum $ty_wit<$($ty_params,)* Ret: BaseTypeWitness> 
        where
            $( $ty_params: BaseTypeWitness, )*
            $first_typa::L: Sized,
            $first_typa::R: Sized,
            $(
                $middle_typa::L: Sized,
                $middle_typa::R: Sized,
            )*
        {
            Eq {
                $($arg_wit: TypeEq<$ty_params, TypeEq<$ty_params::L, $ty_params::R>>,)*
                ret: TypeEq<
                    Ret, 
                    TypeEq<($($ty_params::L,)*), ($($ty_params::R,)*)>
                >
            },
            Ne {
                #[allow(dead_code)]
                contains_ne: SomeTypeArgIsNe<$($ty_params,)*>,
                ret: TypeEq<Ret, TypeNe<($($ty_params::L,)*), ($($ty_params::R,)*)>>
            },
            Cmp {
                ret: TypeEq<Ret, TypeCmp<($($ty_params::L,)*), ($($ty_params::R,)*)>>
            },
        }

        impl<$($ty_params,)*> $ty_wit<$($ty_params,)* $type_alias<$($ty_params,)*>> 
        where
            $first_typa: $trait<$($middle_typa,)* $end_typa>,
            $first_typa::L: Sized,
            $first_typa::R: Sized,
            $(
                $middle_typa: BaseTypeWitness,
                $middle_typa::L: Sized,
                $middle_typa::R: Sized,
            )*
            $end_typa: BaseTypeWitness,
        {
            const NEW: Self = 
                match ($($ty_params::WITNESS,)* $type_alias::<$($ty_params,)*>::WITNESS) {
                    ($(MBTW::Eq($arg_wit),)* MBTW::Eq(ret)) => {
                        Self::Eq { $($arg_wit,)* ret }
                    }
                    (.., MBTW::Ne(ret)) => {
                        let contains_ne = SomeTypeArgIsNe::<$($ty_params,)*>::new();
                        Self::Ne { contains_ne, ret }
                    }
                    (.., MBTW::Cmp(ret)) => {
                        Self::Cmp {ret}
                    }
                    _ => panic!("BUG: invalid permutation of $trait"),
                };
        }


        $(#[$fn_attr])*
        #[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_65")))]
        pub const fn $fn_name<$($ty_params,)*>(
            $($fn_param: $ty_params,)*
        ) -> $type_alias<$($ty_params,)*>
        where
            $first_typa: BaseTypeWitness,
            $first_typa::L: Sized,
            $first_typa::R: Sized,
            $(
                $middle_typa: BaseTypeWitness,
                $middle_typa::L: Sized,
                $middle_typa::R: Sized,
            )*
            $end_typa: BaseTypeWitness,
            
            $first_typa: $trait<$($middle_typa,)* $end_typa>
        {
            match $ty_wit::<$($ty_params,)* $type_alias<$($ty_params,)*>>::NEW {
                $ty_wit::Eq {$($arg_wit,)* ret} => 
                    ret.to_left(TypeEq::$zip_method($($arg_wit.to_right($fn_param),)*)),
                $ty_wit::Ne {contains_ne, ret} => 
                    ret.to_left(contains_ne.$fn_name($($fn_param,)*)),
                $ty_wit::Cmp {ret} => 
                    ret.to_left(
                        MetaBaseTypeWit::to_cmp(A::WITNESS, $fn_param0)
                            .$zip_method($($fn_param_rem,)*)
                    ),
            }
        }
    };
}



declare_zip_items!{
    [A () B] 

    /// Zips together two [`BaseTypeWitness`] types.
    ///
    #[doc = return_type_docs!()]
    ///
    /// # Example
    ///
    /// ### Basic
    ///
    /// This example shows all permutations of argument and return types.
    ///
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, TypeNe, type_ne};
    /// use typewit::methods::zip2;
    ///
    /// with::<u8, u8, bool, u16, u32>(
    ///     TypeEq::NEW, 
    ///     type_ne!(u8, bool),
    ///     TypeCmp::Ne(type_ne!(u16, u32)),
    /// );
    ///
    /// const fn with<A, B, C, D, E>(eq: TypeEq<A, B>, ne: TypeNe<B, C>, cmp: TypeCmp<D, E>) {
    ///     let _: TypeEq<(A, B), (B, A)> = zip2(eq, eq.flip());
    ///     let _: TypeNe<(A, B), (B, C)> = zip2(eq, ne);
    ///     let _: TypeCmp<(A, D), (B, E)> = zip2(eq, cmp);
    /// 
    ///     let _: TypeNe<(B, A), (C, B)> = zip2(ne, eq);
    ///     let _: TypeNe<(B, C), (C, B)> = zip2(ne, ne.flip());
    ///     let _: TypeNe<(B, D), (C, E)> = zip2(ne, cmp);
    /// 
    ///     let _: TypeCmp<(D, A), (E, B)> = zip2(cmp, eq);
    ///     let _: TypeNe<(D, B), (E, C)> = zip2(cmp, ne);
    ///     let _: TypeCmp<(D, E), (E, D)> = zip2(cmp, cmp.flip());
    /// 
    /// }
    /// ```
    fn zip2(wit0, wit1);

    type Zip2Out;

    trait Zip2;

    enum Zip2Wit (arg0, arg1);

    zip_method = zip;

    count = "two"
}

//////////////////////////////////////////////////////////////////////

declare_zip_items!{
    [A (B) C] 

    /// Zips together three [`BaseTypeWitness`] types.
    ///
    #[doc = return_type_docs!()]
    ///
    /// # Example
    ///
    /// ### Basic
    ///
    /// This example shows basic usage.
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, TypeNe, type_eq, type_ne};
    /// use typewit::methods::zip3;
    ///
    /// with::<u8, u8, bool, u16, u32>(
    ///     TypeEq::NEW,
    ///     type_ne!(u8, bool),
    ///     TypeCmp::Ne(type_ne!(u16, u32)),
    /// );
    ///
    /// const fn with<A, B, C, D, E>(eq: TypeEq<A, B>, ne: TypeNe<B, C>, cmp: TypeCmp<D, E>) {
    ///     let _: TypeEq<(A, B, i64), (B, A, i64)> = zip3(eq, eq.flip(), type_eq::<i64>());
    ///
    ///     let _: TypeNe<(A, B, B), (B, A, C)> = zip3(eq, eq.flip(), ne);
    ///
    ///     let _: TypeCmp<(A, D, B), (B, E, A)> = zip3(eq, cmp, eq.flip());
    /// }
    /// ```
    fn zip3(wit0, wit1, wit2);

    type Zip3Out;

    trait Zip3;

    enum Zip3Wit (arg0, arg1, arg2);

    zip_method = zip3;

    count = "three"
}


//////////////////////////////////////////////////////////////////////



declare_zip_items!{
    [A (B C) D] 

    /// Zips together four [`BaseTypeWitness`] types.
    ///
    #[doc = return_type_docs!()]
    ///
    /// # Example
    ///
    /// ### Basic
    ///
    /// This example shows basic usage.
    /// 
    /// ```rust
    /// use typewit::{TypeCmp, TypeEq, TypeNe, type_eq, type_ne};
    /// use typewit::methods::zip4;
    ///
    /// with::<u8, u8, bool, u16, u32>(
    ///     TypeEq::NEW,
    ///     type_ne!(u8, bool),
    ///     TypeCmp::Ne(type_ne!(u16, u32)),
    /// );
    ///
    /// const fn with<A, B, C, D, E>(eq: TypeEq<A, B>, ne: TypeNe<B, C>, cmp: TypeCmp<D, E>) {
    ///     let _: TypeEq<(A, u64, B, i64), (B, u64, A, i64)> = 
    ///         zip4(eq, type_eq(), eq.flip(), type_eq());
    ///
    ///     let _: TypeNe<(A, E, B, B), (B, D, A, C)> = zip4(eq, cmp.flip(), eq.flip(), ne);
    ///
    ///     let _: TypeCmp<(D, A, B, A), (E, B, A, B)> = zip4(cmp, eq, eq.flip(), eq);
    /// }
    /// ```
    fn zip4(wit0, wit1, wit2, wit3);

    type Zip4Out;

    trait Zip4;

    enum Zip4Wit (arg0, arg1, arg2, arg3);

    zip_method = zip4;

    count = "four"
}


//////////////////////////////////////////////////////////////////////

pub use with_const_marker::*;

mod with_const_marker {
    use super::*;

    use crate::type_fn::PairToArrayFn;
    
    use core::marker::PhantomData;


    // maps the type arguments of a BaseTypeWitness with `F`
    struct MapWithFn<F>(PhantomData<F>);

    impl<F, W> crate::TypeFn<W> for MapWithFn<F>
    where
        W: BaseTypeWitness,
        F: crate::TypeFn<W::L> + crate::TypeFn<W::R>,
    {
        type Output = MapBaseTypeWitness<W, F>;
    }

    /// Combines a 
    /// `impl BaseTypeWitness<L = LT, R = RT>`
    /// <br>with an `impl BaseTypeWitness<L = Usize<LN>, R = Usize<RN>>`
    /// <br>returning an `impl BaseTypeWitness<L = [LT; LN], R = [RT; RN]>`.
    /// 
    #[doc = return_type_docs!()]
    /// 
    /// # Example
    /// 
    /// ### Return types
    /// 
    /// ```rust
    /// use typewit::{
    ///     methods::in_array,
    ///     const_marker::Usize,
    ///     TypeCmp, TypeEq, TypeNe,
    ///     type_ne,
    /// };
    ///
    /// let eq_ty: TypeEq<i16, i16> = TypeEq::NEW;
    /// let ne_ty: TypeNe<i16, u16> = type_ne!(i16, u16);
    /// let cmp_ty: TypeCmp<i16, u16> = TypeCmp::Ne(type_ne!(i16, u16));
    /// 
    /// let eq_len: TypeEq<Usize<0>, Usize<0>> = TypeEq::NEW;
    /// let ne_len: TypeNe<Usize<1>, Usize<2>> = Usize.equals(Usize).unwrap_ne();
    /// let cmp_len: TypeCmp<Usize<3>, Usize<3>> = Usize.equals(Usize);
    /// 
    /// 
    /// // if both arguments are TypeEq, this returns a TypeEq
    /// let _: TypeEq<[i16; 0], [i16; 0]> = in_array(eq_ty, eq_len);
    /// 
    /// // if either of the arguments is a TypeNe, this returns a TypeNe
    /// let _: TypeNe<[i16; 0], [u16; 0]> = in_array(ne_ty, eq_len);
    /// let _: TypeNe<[i16; 1], [i16; 2]> = in_array(eq_ty, ne_len);
    /// let _: TypeNe<[i16; 1], [u16; 2]> = in_array(ne_ty, ne_len);
    /// let _: TypeNe<[i16; 1], [u16; 2]> = in_array(cmp_ty, ne_len);
    /// 
    /// // If there are TypeCmp args, and no TypeNe args, this returns a TypeCmp
    /// assert!(matches!(in_array(eq_ty, cmp_len), TypeCmp::<[i16; 3], [i16; 3]>::Eq(_)));
    /// assert!(matches!(in_array(cmp_ty, eq_len), TypeCmp::<[i16; 0], [u16; 0]>::Ne(_)));
    /// assert!(matches!(in_array(cmp_ty, cmp_len), TypeCmp::<[i16; 3], [u16; 3]>::Ne(_)));
    /// ```
    #[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_65")))]
    pub const fn in_array<A, B, LT, RT, const LN: usize, const RN: usize>(
        wit0: A,
        wit1: B,
    ) -> BaseTypeWitnessReparam<Zip2Out<A, B>, [LT; LN], [RT; RN]>
    where
        A: BaseTypeWitness<L = LT, R = RT>,
        B: BaseTypeWitness<L = Usize<LN>, R = Usize<RN>>,
        A: Zip2<B>
    {
        match Zip2Wit::<A, B, Zip2Out<A, B>>::NEW {
            Zip2Wit::Eq {arg0, arg1, ret} => 
                ret.project::<MapWithFn<PairToArrayFn>>()
                    .to_left(TypeEq::in_array(arg0.to_right(wit0), arg1.to_right(wit1))),
            Zip2Wit::Ne {contains_ne, ret} => 
                ret.project::<MapWithFn<PairToArrayFn>>()
                    .to_left(contains_ne.zip2(wit0, wit1).project::<PairToArrayFn>()),
            Zip2Wit::Cmp {ret} => 
                ret.project::<MapWithFn<PairToArrayFn>>().to_left(
                    MetaBaseTypeWit::to_cmp(A::WITNESS, wit0)
                        .in_array(wit1)
                ),
        }
    }
}


