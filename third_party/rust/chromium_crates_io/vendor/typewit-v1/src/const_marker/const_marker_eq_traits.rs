use crate::{Identity, TypeCmp, TypeWitnessTypeArg, MakeTypeWitness};
use crate::const_marker::{ConstMarkerOf, ConstMarker};


/// For types whose values have a [`ConstMarker`] equivalent.
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_83")))]
pub trait HasConstMarker {
    /// The type witness used by the [`ConstMarkerEq<Of = Self>`](ConstMarkerEq) impl of `CM`
    /// 
    /// Implementations that don't need to use a type witness can use
    /// ```rust
    /// # use typewit::const_marker::ConstMarkerOf;
    /// # struct X;
    /// # impl typewit::const_marker::HasConstMarker for X {
    /// type Witness<CM: ConstMarkerOf<Self>> = typewit::TypeEq<CM, CM>;
    /// # }
    /// ```
    type Witness<CM: ConstMarkerOf<Self>>: TypeWitnessTypeArg;
}

/// A [`ConstMarker`] that can be compared to another `ConstMarker` to 
/// produce a [`TypeCmp`] of the comparison.
/// 
/// This trait is mostly designed to support `ConstMarker`s of:
/// - primitive std types
/// - field-less enums
/// 
/// # Examples
/// 
/// - [basic usage of std types](#basic-example)
/// - [defining type-level enums](#enum-example)
/// - [emulating const parameters whose type is generic](#generic-usage-example)
/// 
/// 
/// <div id="basic-example"> </div>
/// 
/// ### Basic usage of std types
/// 
/// ```rust
/// use typewit::TypeCmp;
/// use typewit::const_marker::{ConstMarker, ConstMarkerEqOf, U8};
/// 
/// 
/// const _: () = {
///     // T == U
///     assert!(matches!(typecast::<_, U8<5>>(U8::<5>), Ok(U8::<5>)));
/// 
///     // T != U
///     assert!(matches!(typecast::<_, U8<5>>(U8::<3>), Err(U8::<3>)));
///     assert!(matches!(typecast::<_, U8<5>>(U8::<4>), Err(U8::<4>)));
///     assert!(matches!(typecast::<_, U8<5>>(U8::<6>), Err(U8::<6>)));
///     assert!(matches!(typecast::<_, U8<5>>(U8::<7>), Err(U8::<7>)));
/// };
/// 
/// 
/// // Typecasts `T` to `U` if T == U, returns `Err(val)` if `T != U`.
/// const fn typecast<T, U>(val: T) -> Result<U, T>
/// where
///     T: ConstMarkerEqOf<u8>,
///     U: ConstMarkerEqOf<u8>,
/// {
///     match T::Equals::<U>::VAL {
///         TypeCmp::Eq(te) => Ok(te.to_right(val)),
///         TypeCmp::Ne(_) => Err(val)
///     }
/// }
/// 
/// ```
/// 
/// <div id="enum-example"> </div>
/// 
/// ### Enum
/// 
/// Defining a type-level enum that can be compared for equality with `ConstMarkerEq`
/// 
/// ```rust
/// use typewit::TypeCmp;
/// use typewit::const_marker::{
///     ConstMarker, ConstMarkerOf, ConstMarkerEq, ConstMarkerEqOf, HasConstMarker
/// };
/// 
/// const _: () = {
///     // returns Some if L == R
///     assert!(matches!(typecast_bar::<_, _, First>(&Bar(3, First)), Some(&Bar(3, First))));
///     assert!(matches!(typecast_bar::<_, _, Second>(&Bar(5, Second)), Some(&Bar(5, Second))));
///     assert!(matches!(typecast_bar::<_, _, Third>(&Bar(8, Third)), Some(&Bar(8, Third))));
///     
///     // returns None if L != R
///     assert!(matches!(typecast_bar::<_, _, First>(&Bar(5, Second)), None));
///     assert!(matches!(typecast_bar::<_, _, First>(&Bar(8, Third)), None));
///     
///     assert!(matches!(typecast_bar::<_, _, Second>(&Bar(3, First)), None));
///     assert!(matches!(typecast_bar::<_, _, Second>(&Bar(8, Third)), None));
///     
///     assert!(matches!(typecast_bar::<_, _, Third>(&Bar(3, First)), None));
///     assert!(matches!(typecast_bar::<_, _, Third>(&Bar(5, Second)), None));
/// };
/// 
/// 
/// // Typecasts `&Bar<T, L>` to `&Bar<T, R>` if L == R, returns `None` if `L != R`.
/// const fn typecast_bar<T, L, R>(bar: &Bar<T, L>) -> Option<&Bar<T, R>>
/// where
///     L: ConstMarkerEqOf<Order>,
///     R: ConstMarkerEqOf<Order>,
/// {
///     // type-level function (TypeFn implementor) from `X` to `Bar<T, X>`
///     typewit::type_fn!{
///         struct BarFn<T>;
///         impl<X> X => Bar<T, X>
///     }
///     
///     match L::Equals::<R>::VAL {
///         // te: TypeEq<L, R>, a proof of L == R
///         // .map(BarFn::NEW): TypeEq<Bar<T, L>, Bar<T, R>>
///         // .in_ref(): TypeEq<&Bar<T, L>, &Bar<T, R>>
///         TypeCmp::Eq(te) => Some(te.map(BarFn::NEW).in_ref().to_right(bar)),
///         TypeCmp::Ne(_) => None,
///     }
/// }
/// 
/// 
/// struct Bar<T, L>(T, L);
/// 
/// // Order enum and type-level enum
/// 
/// enum Order {
///     First = 1,
///     Second,
///     Third,
/// }
/// 
/// impl HasConstMarker for Order {
///     // the type witness of the `ConstMarkerEq` implementors below
///     type Witness<T: ConstMarkerOf<Self>> = OrderWit<T>;
/// }
/// 
/// typewit::simple_type_witness! {
///     // Declares `enum OrderWit<__Wit>`
///     // (the `__Wit` type parameter is implicitly added after all generics),
///     // the `Equals` derive adds this inherent method:
///     // ```rust
///     // const fn equals<Wit2>(self, rhs: OrderWit<Wit2>) -> TypeCmp<__Wit, Wit2>
///     // ```
///     derive(Equals)
///     enum OrderWit {
///         First = First,
///         Second = Second,
///         Third = Third,
///     }
/// }
/// 
/// macro_rules! declare_type_level_variant {($variant:ident) => {
///     struct $variant;
/// 
///     impl ConstMarker for $variant {
///         type Of = Order;
///         const VAL: Self::Of = Order::$variant;
///     }
/// 
///     impl ConstMarkerEq for $variant {
///         type Equals<Rhs: ConstMarkerEqOf<Self::Of>> = OrderTypeEquals<Self, Rhs>;
///     }
/// }}
/// 
/// declare_type_level_variant!{First}
/// declare_type_level_variant!{Second}
/// declare_type_level_variant!{Third}
/// 
/// // Emulates a generic constant for `ConstMarkerEq::Equals` above
/// // by implementing `ConstMarker` below.
/// struct OrderTypeEquals<L, R>(core::marker::PhantomData<(L, R)>);
/// 
/// impl<L, R> ConstMarker for OrderTypeEquals<L, R>
/// where
///     L: ConstMarkerEqOf<Order>,
///     R: ConstMarkerEqOf<Order>,
/// {
///     type Of = typewit::TypeCmp<L, R>;
///     const VAL: Self::Of = {
///         // `CM_WITNESS` comes from the `ConstMarkerHasWitness` supertrait of `ConstMarkerEq`
///         let lhs_wit: OrderWit<L> = L::CM_WITNESS;
///         let rhs_wit: OrderWit<R> = R::CM_WITNESS;
///         lhs_wit.equals(rhs_wit)
///     };
/// }
/// ```
/// 
/// <div id="generic-usage-example"> </div>
/// 
/// ### Generic usage
/// 
/// This example demonstrates emulation of const parameters of generic type
/// (as in, it emulates a `struct Pair<Of, const T: Of, const U: Of>;` type)
/// 
/// ```rust
/// use typewit::{Identity, TypeCmp, TypeEq};
/// use typewit::const_marker::{
///     CmEquals, ConstMarker, ConstMarkerOf, ConstMarkerEq, ConstMarkerEqOf, HasConstMarker,
///     I8, I16, U8, U16,
/// };
/// 
/// use core::marker::PhantomData as PD;
/// 
/// const _: () = {
///     // `T == U`, does the typecast
///     assert!(matches!(
///         typecast::<Pair<U8<3>, U8<5>>, Pair<U8<3>, U8<5>>>(Pair(PD)),
///         Ok(Pair::<U8<3>, U8<5>>(PD)),
///     ));
///     assert!(matches!(
///         typecast::<Pair<I8<3>, I8<5>>, Pair<I8<3>, I8<5>>>(Pair(PD)),
///         Ok(Pair::<I8<3>, I8<5>>(PD)),
///     ));
///     
///     // `T != U`, returns error
///     assert!(matches!(
///        typecast::<Pair<U16<3>, U16<5>>, Pair<U16<3>, U16<9>>>(Pair(PD)),
///        Err(Pair::<U16<3>, U16<5>>(PD)),
///     ));
///     assert!(matches!(
///        typecast::<Pair<I16<3>, I16<5>>, Pair<I16<9>, I16<5>>>(Pair(PD)),
///        Err(Pair::<I16<3>, I16<5>>(PD)),
///     ));
/// };
///     
/// // Typecasts `T` to `U` if `T == U`, returns `Err(pair)` if `T != U`.
/// const fn typecast<T: IsPair, U: IsPair<Of = T::Of>>(pair: T) -> Result<U, T> {
///     match T::PairEquals::<U>::VAL {
///         // `te: TypeEq<T, U>`, a proof of `T == U`
///         TypeCmp::Eq(te) => Ok(te.to_right(pair)),
///         TypeCmp::Ne(_) => Err(pair),
///     }
/// }
/// 
/// #[derive(Copy, Clone)]
/// struct Pair<T, U>(PD<(T, U)>);
/// 
/// // Trait for Pair
/// // 
/// // this Identity supertrait emulates a `P == Pair<P::T, P::U>` constraint,
/// // it then allows coercing `P` to `Pair` and vice-versa.
/// trait IsPair: Identity<Type = Pair<Self::T, Self::U>> + Copy {
///     // The type of the constants that the `ConstMarker` parameters emulate.
///     type Of;
///     
///     // The first type parameter of `Pair`
///     type T: ConstMarkerEqOf<Self::Of>;
///     
///     // The second type parameter of `Pair`
///     type U: ConstMarkerEqOf<Self::Of>;
///     
///     // `ConstMarker` that compares `Self` and `Rhs` for equality, 
///     // producing a `TypeCmp<Self, Rhs>` of their (in)equality
///     type PairEquals<Rhs: IsPair<Of = Self::Of>>: ConstMarkerOf<TypeCmp<Self, Rhs>>;
/// }
/// 
/// impl<T, U> IsPair for Pair<T, U> 
/// where
///     T: ConstMarkerEq + Copy,
///     U: ConstMarkerEqOf<T::Of> + Copy,
/// {
///     type Of = T::Of;
///     type T = T;
///     type U = U;
///     type PairEquals<Rhs: IsPair<Of = T::Of>> = PairEquals<Self, Rhs>;
/// }
/// 
/// struct PairEquals<Lhs, Rhs>(core::marker::PhantomData<(Lhs, Rhs)>);
/// 
/// impl<Lhs, Rhs> ConstMarker for PairEquals<Lhs, Rhs> 
/// where
///     Lhs: IsPair, 
///     Rhs: IsPair<Of = Lhs::Of>,
/// {
///     type Of = TypeCmp<Lhs, Rhs>;
///     const VAL: Self::Of = {
///         // type-level function (InjTypeFn implementor) from (T, U) to Pair<T, U>
///         // need `inj_type_fn` to map a `TypeCmp` (`typewit::type_fn` doesn't work)
///         typewit::inj_type_fn! {
///             struct PairFn;
///             impl<T, U> (T, U) => Pair<T, U>
///         }
///         
///         // uses the `Identity` supertrait to get a proof that `Lhs` and `Rhs` are `Pair`s
///         let eq_lhs: TypeEq<Lhs, Pair<Lhs::T, Lhs::U>> = Lhs::TYPE_EQ;
///         let eq_rhs: TypeEq<Rhs, Pair<Rhs::T, Rhs::U>> = Rhs::TYPE_EQ;
///     
///         let cmp_t: TypeCmp<Lhs::T, Rhs::T> = CmEquals::<Lhs::T, Rhs::T>::VAL;
///         let cmp_u: TypeCmp<Lhs::U, Rhs::U> = CmEquals::<Lhs::U, Rhs::U>::VAL;
///     
///         let cmp: TypeCmp<Pair<Lhs::T, Lhs::U>, Pair<Rhs::T, Rhs::U>> =
///             cmp_t.zip(cmp_u).map(PairFn::NEW);
///     
///         cmp.join_left(eq_lhs).join_right(eq_rhs.flip())
///     };
/// }
/// 
/// ```
/// 
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_83")))]
pub trait ConstMarkerEq: ConstMarker + ConstMarkerHasWitness {
    /// [`ConstMarker`] for getting whether `Self` and `Rhs` are the same type.
    /// 
    /// If you need to write `<T as ConstMarkerEq>::Equals::<U>` for diambiguation
    /// (instead of `T::Equals::<U>`),
    /// consider using [`CmEquals`] instead.
    /// 
    /// (ideally this would be a generic associated constant, but those are unstable)
    type Equals<Rhs: ConstMarkerEq<Of = Self::Of>>:
        ConstMarkerOf<TypeCmp<Self, Rhs>>;
}


/// Trait alias for `ConstMarkerEq<Of = Of>`
/// 
/// This trait also shows the `Of` type in unsatisfied trait bound errors
/// (`ConstMarkerEq<Of = ...>` bounds obfuscate this a bit as of 1.89.0) 
#[diagnostic::on_unimplemented(
    message = "`{Self}` is not a comparable type-level constant of type `{Of}`",
)]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_83")))]
pub trait ConstMarkerEqOf<Of>: ConstMarkerEq<Of = Of> {}

impl<C, Of> ConstMarkerEqOf<Of> for C
where
    C: ConstMarkerEq<Of = Of>,
{}


/// For getting the type witness of this `ConstMarker`.
/// 
/// The witness type is determined by the [`Self::Of::Witness`](HasConstMarker::Witness) 
/// associated type, not `Self` directly. 
/// This allows multiple types with the same `Of` associated type to 
/// have a compatible witness.
/// 
/// For std types, this doesn't produce anything useful, 
/// this is mostly intended for user-defined types to be able to use their 
/// type witness in the `ConstMarkerOf<TypeCmp<Lhs, Rhs>>` impl of the
/// [`ConstMarkerEq::Equals`] associated type.
/// 
/// 
/// Note: this trait is blanket implemented and cannot be manually implemented.
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_83")))]
pub trait ConstMarkerHasWitness: ConstMarker<Of: HasConstMarker> {
    /// The type witness of this `ConstMarker`
    /// 
    const CM_WITNESS: <Self::Of as HasConstMarker>::Witness<Self>;

    #[doc(hidden)]
    type __Seal__: MakeTypeWitness + Identity<Type = <Self::Of as HasConstMarker>::Witness<Self>>;
}


impl<CM> ConstMarkerHasWitness for CM
where
    CM: ConstMarker,
    CM::Of: HasConstMarker<Witness<CM>: MakeTypeWitness>,
{
    const CM_WITNESS: <Self::Of as HasConstMarker>::Witness<Self> = MakeTypeWitness::MAKE;

    #[doc(hidden)]
    type __Seal__ = <Self::Of as HasConstMarker>::Witness<Self>;
}



////////////////////////////////////////////////////////////////////////////////

/// Type alias for [`<Lhs as ConstMarkerEq>::Equals::<Rhs>`](ConstMarkerEq::Equals),
/// a [`ConstMarker<Of = TypeCmp<Lhs, Rhs>>`](ConstMarker).
///
/// This alias is useful to avoid the verbosity of using
/// `<Lhs as ConstMarkerEq>::Equals::<Rhs>` for diambiguation
/// (whenever `Lhs::Equals::<Rhs>` is ambiguous),
/// 
/// # Example
/// 
/// ```rust
/// use typewit::TypeCmp;
/// use typewit::const_marker::{CmEquals, ConstMarker, U8};
/// 
/// // U8<3> != U8<5>
/// assert!(matches!(CmEquals::<U8<3>, U8<5>>::VAL, TypeCmp::<U8<3>, U8<5>>::Ne(_)));
/// 
/// // U8<3> == U8<5>
/// assert!(matches!(CmEquals::<U8<5>, U8<5>>::VAL, TypeCmp::<U8<5>, U8<5>>::Eq(_)));
/// ```
/// 
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_83")))]
pub type CmEquals<Lhs, Rhs> = 
    <Lhs as ConstMarkerEq>::Equals::<Rhs>;


////////////////////////////////////////////////////////////////////////////////


/// Compared any two [`ConstMarkerEq`] types that have constants of the same type,
/// returning a proof of their type (in)equality.
/// 
/// # Example
/// 
/// ```rust
/// use typewit::TypeCmp;
/// use typewit::const_marker::{self, ConstMarkerEq, Char, U8};
/// 
/// assert_eq!(typecast(Char::<' '>, Char::<' '>), Ok([Char::<' '>, Char::<' '>]));
/// assert_eq!(typecast(Char::<'X'>, Char::<'Y'>), Err((Char::<'X'>, Char::<'Y'>)));
/// 
/// assert_eq!(typecast(U8::<3>, U8::<3>), Ok([U8::<3>, U8::<3>]));
/// assert_eq!(typecast(U8::<5>, U8::<8>), Err((U8::<5>, U8::<8>)));
/// 
/// 
/// 
/// const fn typecast<L, R>(lhs: L, rhs: R) -> Result<[L; 2], (L, R)>
/// where
///    L: ConstMarkerEq,
///    R: ConstMarkerEq<Of = L::Of>,
/// {
///     match const_marker::equals(&lhs, &rhs) {
///         TypeCmp::Eq(te) => Ok([lhs, te.to_left(rhs)]),
///         TypeCmp::Ne(_) => Err((lhs, rhs))
///     }
/// }
/// 
/// ```
/// 
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_83")))]
pub const fn equals<L, R>(_lhs: &L, _rhs: &R) -> TypeCmp<L, R>
where
    L: ConstMarkerEq,
    R: ConstMarkerEq<Of = L::Of>,
{
    L::Equals::<R>::VAL
}


////////////////////////////////////////////////////////////////////////////////


/// Dummy type whose only purpose is to make sure that 
/// the ConstMarker impl for StdTypeEquality is sound.
#[doc(hidden)]
pub struct __StdIdentityTypeEq<T>(pub(crate) core::marker::PhantomData<T>);

impl<T> crate::TypeWitnessTypeArg for __StdIdentityTypeEq<T> {
    type Arg = T;
}

/// [`ConstMarker`] for getting whether `L` and `R` (two `ConstMarkerOf<StdType>`)
/// are the same type or not.
///
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_83")))]
pub struct StdTypeEquality<StdType, L, R>(pub(crate) core::marker::PhantomData<(StdType, L, R)>);


macro_rules! __const_marker_impls {
    (
        $(#[$struct_docs:meta])*
        $struct:ident($prim:ty)

        $(
            $(#[$eq_docs:meta])*
            fn equals $(($L:ident, $R:ident) $comparator:block)?;
        )?
    ) => {

        // keep this specific to `$struct<T>`,
        // making this generic violates the safety assumptions of StdTypeEquality::VAL.
        impl<const T: $prim> crate::MakeTypeWitness
        for crate::const_marker::__StdIdentityTypeEq<$struct<T>>
        {
            const MAKE: Self = crate::const_marker::__StdIdentityTypeEq{
                0: core::marker::PhantomData
            };
        }

        impl crate::const_marker::HasConstMarker for $prim {
            type Witness<CM: crate::const_marker::ConstMarkerOf<Self>> =
                crate::const_marker::__StdIdentityTypeEq<CM>;
        }

        impl<const VAL: $prim> crate::const_marker::ConstMarkerEq for $struct<VAL> {
            type Equals<Rhs: crate::const_marker::ConstMarkerEqOf<Self::Of>> = 
                crate::const_marker::StdTypeEquality<$prim, Self, Rhs>;
        }

        impl<L, R> crate::const_marker::ConstMarker
        for crate::const_marker::StdTypeEquality<$prim, L, R>
        where
            L: crate::const_marker::ConstMarkerEqOf<$prim>,
            R: crate::const_marker::ConstMarkerEqOf<$prim>,
        {
            const VAL: Self::Of = 
                if crate::const_marker::__const_eq_with!(
                    L::VAL,
                    R::VAL
                    $($(, ($L, $R) $comparator)?)?
                ) {
                    // SAFETY:  (boy, this is complicated)
                    // - both `L` and `R` impl `ConstMarkerOf<$prim>`,
                    // - `L::VAL == R::VAL` (both are std types with sensible Eq impls)
                    // - `$prim` requires the witness type for `L` and `R` to be 
                    //   `__StdIdentityTypeEq`
                    // - `ConstMarkerHasWitness` requires that witness to impl `MakeTypeWitness`,
                    // - `__StdIdentityTypeEq` only impls `MakeTypeWitness` for marker types 
                    //   of std types defined in typewit
                    // - there's <= 1 `ConstMarkerEqOf<X>` impls in typewit 
                    //   where `X` is some type from std
                    //  
                    // therefore: `L == R`
                    unsafe {
                        crate::TypeEq::<L, R>::new_unchecked().to_cmp()
                    }
                } else {
                    // SAFETY: `L != R` (both are std types with sensible Eq impls)
                    // therefore `L != R`
                    unsafe {
                        crate::TypeNe::<L, R>::new_unchecked().to_cmp()
                    }
                };

            type Of = crate::TypeCmp<L, R>;
        }
    }

} pub(crate) use __const_marker_impls;

