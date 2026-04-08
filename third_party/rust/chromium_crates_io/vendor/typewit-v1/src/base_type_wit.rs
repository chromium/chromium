//! abstractions over
//! [`TypeEq`](crate::TypeEq)/[`TypeNe`](crate::TypeNe)/[`TypeCmp`](crate::TypeCmp).


mod meta_base_type_wit;

pub use meta_base_type_wit::MetaBaseTypeWit;


/// Marker trait for 
/// [`TypeCmp`](crate::TypeCmp)/[`TypeEq`](crate::TypeEq)/[`TypeNe`](crate::TypeNe).
/// 
/// [`TypeEq`]: crate::TypeEq
/// [`TypeNe`]: crate::TypeNe
/// [`TypeCmp`]: crate::TypeCmp
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_61")))]
pub trait BaseTypeWitness: 
    core::fmt::Debug + 
    Copy + 
    crate::HasTypeWitness<MetaBaseTypeWit<Self::L, Self::R, Self>>
{
    /// The `L` type parameter of `TypeEq`/`TypeNe`/`TypeCmp` types.
    type L: ?Sized;
    /// The `R` type parameter of `TypeEq`/`TypeNe`/`TypeCmp` types.
    type R: ?Sized;
    
    /// The [type constructor] corresponding to this type.
    ///
    /// [type constructor]: crate::type_constructors::BaseTypeWitnessTc
    #[cfg(feature = "rust_1_65")]
    #[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_65")))]
    type TypeCtor: crate::type_constructors::BaseTypeWitnessTc<Type<Self::L, Self::R> = Self>;
}


impl<L: ?Sized, R: ?Sized> BaseTypeWitness for crate::TypeEq<L, R> {
    type L = L;
    type R = R;

    #[cfg(feature = "rust_1_65")]
    type TypeCtor = crate::type_constructors::TcTypeEq;

}

impl<L: ?Sized, R: ?Sized> BaseTypeWitness for crate::TypeNe<L, R> {
    type L = L;
    type R = R;

    #[cfg(feature = "rust_1_65")]
    type TypeCtor = crate::type_constructors::TcTypeNe;
}

impl<L: ?Sized, R: ?Sized> BaseTypeWitness for crate::TypeCmp<L, R> {
    type L = L;
    type R = R;

    #[cfg(feature = "rust_1_65")]
    type TypeCtor = crate::type_constructors::TcTypeCmp;
}



