#[cfg(feature = "rust_1_65")]
use crate::base_type_wit::MetaBaseTypeWit as MBTW;

use crate::{
    base_type_wit::BaseTypeWitness as BTW,
    TypeEq, TypeNe
};



// A `TypeEq` that's used as padding for the trailing type arguments of SomeTypeArgIsNe.
type PadTyEq = TypeEq<(), ()>;

// The first TypeNe in the 4 `BaseTypeWitness` type parameters
pub(crate) enum SomeTypeArgIsNe<A: BTW,  B: BTW, C: BTW = PadTyEq, D: BTW = PadTyEq> {
    A(TypeEq<A, TypeNe<A::L, A::R>>),
    B(TypeEq<B, TypeNe<B::L, B::R>>),
    C(TypeEq<C, TypeNe<C::L, C::R>>),
    D(TypeEq<D, TypeNe<D::L, D::R>>),
}

#[cfg(feature = "rust_1_65")]
impl<A: BTW, B: BTW, C: BTW, D: BTW> SomeTypeArgIsNe<A, B, C, D> {
    pub(crate) const TRY_NEW: Option<Self> = {
        match (A::WITNESS, B::WITNESS, C::WITNESS, D::WITNESS) {
            (MBTW::Ne(ne), _, _, _) => Some(Self::A(ne)),
            (_, MBTW::Ne(ne), _, _) => Some(Self::B(ne)),
            (_, _, MBTW::Ne(ne), _) => Some(Self::C(ne)),
            (_, _, _, MBTW::Ne(ne)) => Some(Self::D(ne)),
            _ => None,
        }
    };

    pub(crate) const fn new() -> Self {
        match Self::TRY_NEW {
            Some(x) => x,
            None => panic!("expected at least one type argument to be TypeNe"),
        }
    }
}

impl<A: BTW, B: BTW> SomeTypeArgIsNe<A, B, PadTyEq, PadTyEq> 
where
    A::L: Sized,
    A::R: Sized,
{
    #[inline(always)]
    pub(crate) const fn zip2(self, _: A, _: B) -> TypeNe<(A::L, B::L), (A::R, B::R)> {
        // SAFETY: either `A` or `B` is a TypeNe (PadTyEq isn't a TypeNe),
        //         therefore: `(A::L, B::L) != (A::R, B::R)`.
        //         (the function parameters are needed for soundness,
        //          since `TypeNe` guarantees type inequality at the value level)
        unsafe { TypeNe::new_unchecked() }
    }
}
impl<A: BTW, B: BTW, C: BTW> SomeTypeArgIsNe<A, B, C, PadTyEq> 
where
    A::L: Sized,
    A::R: Sized,
    B::L: Sized,
    B::R: Sized,
{
    #[inline(always)]
    pub(crate) const fn zip3(
        self,
        _: A,
        _: B,
        _: C,
    ) -> TypeNe<(A::L, B::L, C::L), (A::R, B::R, C::R)> {
        // SAFETY: either `A`, `B`, or `C is a TypeNe (PadTyEq isn't a TypeNe),
        //         therefore: `(A::L, B::L, C::L) != (A::R, B::R, C::R)`.
        //         (the function parameters are needed for soundness,
        //          since `TypeNe` guarantees type inequality at the value level)
        unsafe { TypeNe::new_unchecked() }
    }
}
impl<A: BTW, B: BTW, C: BTW, D: BTW> SomeTypeArgIsNe<A, B, C, D> 
where
    A::L: Sized,
    A::R: Sized,
    B::L: Sized,
    B::R: Sized,
    C::L: Sized,
    C::R: Sized,
{
    #[inline(always)]
    pub(crate) const fn zip4(
        self,
        _: A,
        _: B,
        _: C,
        _: D,
    ) -> TypeNe<(A::L, B::L, C::L, D::L), (A::R, B::R, C::R, D::R)> {
        // SAFETY: either `A`, `B`, `C`, or `D` is a TypeNe,
        //         therefore: `(A::L, B::L, C::L, D::L) != (A::R, B::R, C::R, D::R)`.
        //         (the function parameters are needed for soundness,
        //          since `TypeNe` guarantees type inequality at the value level)
        unsafe { TypeNe::new_unchecked() }
    }
}
