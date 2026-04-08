use crate::{MakeTypeWitness, TypeWitnessTypeArg, TypeEq, TypeNe, TypeCmp};

use core::fmt::{self, Debug};

/// Type witness for 
/// [`TypeEq`](crate::TypeEq)/[`TypeNe`](crate::TypeNe)/[`TypeCmp`](crate::TypeCmp).
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_61")))]
pub enum MetaBaseTypeWit<L: ?Sized, R: ?Sized, W> {
    /// `where W == TypeEq<L, R>`
    Eq(TypeEq<W, TypeEq<L, R>>),
    /// `where W == TypeNe<L, R>`
    Ne(TypeEq<W, TypeNe<L, R>>),
    /// `where W == TypeCmp<L, R>`
    Cmp(TypeEq<W, TypeCmp<L, R>>),
}

impl<L: ?Sized, R: ?Sized, W> MetaBaseTypeWit<L, R, W> {
    /// Converts `W` to `TypeCmp<L, R>`
    pub const fn to_cmp(self, witness: W) -> TypeCmp<L, R> {
        match self {
            Self::Cmp(te) => te.to_right(witness),
            Self::Eq(te) => TypeCmp::Eq(te.to_right(witness)),
            Self::Ne(te) => TypeCmp::Ne(te.to_right(witness)),
        }
    }
}


impl<L: ?Sized, R: ?Sized, W> Copy for  MetaBaseTypeWit<L, R, W> {}

impl<L: ?Sized, R: ?Sized, W> Clone for  MetaBaseTypeWit<L, R, W> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<L: ?Sized, R: ?Sized, W> Debug for MetaBaseTypeWit<L, R, W> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let isa = match self {
            Self::Eq{..} => "TypeEq",
            Self::Ne{..} => "TypeNe",
            Self::Cmp{..} => "TypeCmp",
        };

        f.write_str(isa)
    }
}


impl<L: ?Sized, R: ?Sized, W> TypeWitnessTypeArg for MetaBaseTypeWit<L, R, W> {
    type Arg = W;
}

impl<L: ?Sized, R: ?Sized> MakeTypeWitness for MetaBaseTypeWit<L, R, TypeCmp<L, R>> {
    const MAKE: Self = Self::Cmp(TypeEq::NEW);
}

impl<L: ?Sized, R: ?Sized> MakeTypeWitness for MetaBaseTypeWit<L, R, TypeEq<L, R>> {
    const MAKE: Self = Self::Eq(TypeEq::NEW);
}

impl<L: ?Sized, R: ?Sized> MakeTypeWitness for MetaBaseTypeWit<L, R, TypeNe<L, R>> {
    const MAKE: Self = Self::Ne(TypeEq::NEW);
}
