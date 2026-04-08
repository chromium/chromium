//! Higher Kinded Types for [`BaseTypeWitness`]es
#![cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_65")))]


use crate::{BaseTypeWitness, TypeCmp, TypeEq, TypeNe};

use core::fmt::Debug;


/// The type constructor for a [`BaseTypeWitness`],
/// only implemented for [`TcTypeCmp`]/[`TcTypeEq`]/[`TcTypeNe`].
/// 
/// A type constructor is a "type" which needs to be provided 
/// generic arguments to produce a concrete type.
/// 
/// This crate emulates type constructors for [`BaseTypeWitness`] types by using this trait,
/// concrete types are produced with the [`Type`](Self::Type) associated constant.
/// 
pub trait BaseTypeWitnessTc: 'static + Copy + Debug {
    /// The [`BaseTypeWitness`] type that corresponds to this type constructor.
    /// 
    /// For [`TcTypeCmp`], this is [`TypeCmp`]`<L, R>`
    /// <br>
    /// For [`TcTypeEq`], this is [`TypeEq`]`<L, R>`
    /// <br>
    /// For [`TcTypeNe`], this is [`TypeNe`]`<L, R>`
    /// 
    type Type<L: ?Sized, R: ?Sized>: BaseTypeWitness<L = L, R = R, TypeCtor = Self>;
}


/// Computes a [`BaseTypeWitness`] type from a [`BaseTypeWitnessTc`]
pub type TcToBaseTypeWitness<TC, L, R> = <TC as BaseTypeWitnessTc>::Type::<L, R>;

/// Queries the [`BaseTypeWitnessTc`] of a [`BaseTypeWitness`]
pub type BaseTypeWitnessToTc<W> = <W as BaseTypeWitness>::TypeCtor;

/// Computes `W:`[`BaseTypeWitness`] with its type arguments replaced with `L` and `R`
pub type BaseTypeWitnessReparam<W, L, R> = 
    <BaseTypeWitnessToTc<W> as BaseTypeWitnessTc>::Type::<
        L, 
        R,
    >;

/// The type returned by `W::project::<F>()`,
/// where `W` is a [`BaseTypeWitness`]
pub type MapBaseTypeWitness<W, F> = 
    <BaseTypeWitnessToTc<W> as BaseTypeWitnessTc>::Type::<
        crate::CallFn<F, <W as BaseTypeWitness>::L>,
        crate::CallFn<F, <W as BaseTypeWitness>::R>,
    >;



/////////////////////////////////////////////////////////////////////////////


/// The [*type constructor*](BaseTypeWitnessTc) for [`TypeCmp`].
#[derive(Debug, Copy, Clone)]
pub struct TcTypeCmp;

impl BaseTypeWitnessTc for TcTypeCmp {
    type Type<L: ?Sized, R: ?Sized> = TypeCmp<L, R>;
}


/// The [*type constructor*](BaseTypeWitnessTc) for [`TypeEq`].
#[derive(Debug, Copy, Clone)]
pub struct TcTypeEq;

impl BaseTypeWitnessTc for TcTypeEq {
    type Type<L: ?Sized, R: ?Sized> = TypeEq<L, R>;
}


/// The [*type constructor*](BaseTypeWitnessTc) for [`TypeNe`].
#[derive(Debug, Copy, Clone)]
pub struct TcTypeNe;

impl BaseTypeWitnessTc for TcTypeNe {
    type Type<L: ?Sized, R: ?Sized> = TypeNe<L, R>;
}
