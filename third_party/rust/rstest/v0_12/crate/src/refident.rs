/// Provide `RefIdent` and `MaybeIdent` traits that give a shortcut to extract identity reference
/// (`syn::Ident` struct).
use proc_macro2::Ident;
use syn::{FnArg, Pat, PatType, Type};

pub trait RefIdent {
    /// Return the reference to ident if any
    fn ident(&self) -> &Ident;
}

pub trait MaybeIdent {
    /// Return the reference to ident if any
    fn maybe_ident(&self) -> Option<&Ident>;
}

impl<I: RefIdent> MaybeIdent for I {
    fn maybe_ident(&self) -> Option<&Ident> {
        Some(self.ident())
    }
}

impl RefIdent for Ident {
    fn ident(&self) -> &Ident {
        self
    }
}

impl<'a> RefIdent for &'a Ident {
    fn ident(&self) -> &Ident {
        *self
    }
}

impl MaybeIdent for FnArg {
    fn maybe_ident(&self) -> Option<&Ident> {
        match self {
            FnArg::Typed(PatType { pat, .. }) => match pat.as_ref() {
                Pat::Ident(ident) => Some(&ident.ident),
                _ => None,
            },
            _ => None,
        }
    }
}

impl MaybeIdent for Type {
    fn maybe_ident(&self) -> Option<&Ident> {
        match self {
            Type::Path(tp) if tp.qself.is_none() => tp.path.get_ident(),
            _ => None,
        }
    }
}

pub trait MaybeType {
    /// Return the reference to type if any
    fn maybe_type(&self) -> Option<&Type>;
}

impl MaybeType for FnArg {
    fn maybe_type(&self) -> Option<&Type> {
        match self {
            FnArg::Typed(PatType { ty, .. }) => Some(ty.as_ref()),
            _ => None,
        }
    }
}

impl MaybeIdent for syn::GenericParam {
    fn maybe_ident(&self) -> Option<&Ident> {
        match self {
            syn::GenericParam::Type(syn::TypeParam { ident, .. })
            | syn::GenericParam::Const(syn::ConstParam { ident, .. }) => Some(ident),
            syn::GenericParam::Lifetime(syn::LifetimeDef { lifetime, .. }) => Some(&lifetime.ident),
        }
    }
}

impl MaybeIdent for crate::parse::Attribute {
    fn maybe_ident(&self) -> Option<&Ident> {
        use crate::parse::Attribute::*;
        match self {
            Attr(ident) | Tagged(ident, _) | Type(ident, _) => Some(ident),
        }
    }
}
