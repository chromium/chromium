use crate::syntax::map::UnorderedMap;
use crate::syntax::resolve::Resolution;
use crate::syntax::types::Types;
use crate::syntax::{mangle, Symbol, Ty1, Type};
use proc_macro2::{Ident, Span};
use std::hash::{Hash, Hasher};

#[derive(PartialEq, Eq, Hash)]
pub(crate) enum ImplKey<'a> {
    RustBox(NamedImplKey<'a>),
    RustVec(NamedImplKey<'a>),
    UniquePtr(NamedImplKey<'a>),
    SharedPtr(NamedImplKey<'a>),
    WeakPtr(NamedImplKey<'a>),
    CxxVector(NamedImplKey<'a>),
}

impl<'a> ImplKey<'a> {
    /// Whether to produce FFI symbols instantiating the given generic type even
    /// when an explicit `impl Foo<T> {}` is not present in the current bridge.
    ///
    /// The main consideration is that the same instantiation must not be
    /// present in two places, which is accomplished using trait impls and the
    /// orphan rule. Every instantiation of a C++ template like `CxxVector<T>`
    /// and Rust generic type like `Vec<T>` requires the implementation of
    /// traits defined by the `cxx` crate for some local type. (TODO: or for a
    /// fundamental type like `Box<LocalType>`)
    pub(crate) fn is_implicit_impl_ok(&self, types: &Types) -> bool {
        // TODO: relax this for Rust generics to allow Vec<Vec<T>> etc.
        types.is_local(self.inner())
    }

    /// Returns the type argument in the generic instantiation described by
    /// `self`. For example, if `self` represents `UniquePtr<u32>` then this
    /// will return `u32`.
    fn inner(&self) -> &'a Type {
        let named_impl_key = match self {
            ImplKey::RustBox(key)
            | ImplKey::RustVec(key)
            | ImplKey::UniquePtr(key)
            | ImplKey::SharedPtr(key)
            | ImplKey::WeakPtr(key)
            | ImplKey::CxxVector(key) => key,
        };
        named_impl_key.inner
    }
}

pub(crate) struct NamedImplKey<'a> {
    #[cfg_attr(not(proc_macro), expect(dead_code))]
    pub begin_span: Span,
    /// Mangled form of the `inner` type.
    pub symbol: Symbol,
    /// Generic type - e.g. `UniquePtr<u8>`.
    #[cfg_attr(proc_macro, expect(dead_code))]
    pub outer: &'a Type,
    /// Generic type argument - e.g. `u8` from `UniquePtr<u8>`.
    pub inner: &'a Type,
    #[cfg_attr(not(proc_macro), expect(dead_code))]
    pub end_span: Span,
}

impl Type {
    pub(crate) fn impl_key(&self, res: &UnorderedMap<&Ident, Resolution>) -> Option<ImplKey> {
        match self {
            Type::RustBox(ty) => Some(ImplKey::RustBox(NamedImplKey::new(self, ty, res)?)),
            Type::RustVec(ty) => Some(ImplKey::RustVec(NamedImplKey::new(self, ty, res)?)),
            Type::UniquePtr(ty) => Some(ImplKey::UniquePtr(NamedImplKey::new(self, ty, res)?)),
            Type::SharedPtr(ty) => Some(ImplKey::SharedPtr(NamedImplKey::new(self, ty, res)?)),
            Type::WeakPtr(ty) => Some(ImplKey::WeakPtr(NamedImplKey::new(self, ty, res)?)),
            Type::CxxVector(ty) => Some(ImplKey::CxxVector(NamedImplKey::new(self, ty, res)?)),
            _ => None,
        }
    }
}

impl<'a> PartialEq for NamedImplKey<'a> {
    fn eq(&self, other: &Self) -> bool {
        PartialEq::eq(&self.symbol, &other.symbol)
    }
}

impl<'a> Eq for NamedImplKey<'a> {}

impl<'a> Hash for NamedImplKey<'a> {
    fn hash<H: Hasher>(&self, hasher: &mut H) {
        self.symbol.hash(hasher);
    }
}

impl<'a> NamedImplKey<'a> {
    fn new(outer: &'a Type, ty1: &'a Ty1, res: &UnorderedMap<&Ident, Resolution>) -> Option<Self> {
        let inner = &ty1.inner;
        Some(NamedImplKey {
            symbol: mangle::typename(inner, res)?,
            begin_span: ty1.name.span(),
            outer,
            inner,
            end_span: ty1.rangle.span,
        })
    }
}
