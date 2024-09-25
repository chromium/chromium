/// Provide `RefIdent` and `MaybeIdent` traits that give a shortcut to extract identity reference
/// (`syn::Ident` struct).
use proc_macro2::Ident;
use syn::{FnArg, Pat, PatIdent, PatType, Type};

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
        self
    }
}

impl MaybeIdent for FnArg {
    fn maybe_ident(&self) -> Option<&Ident> {
        match self {
            FnArg::Typed(pat) => pat.maybe_ident(),
            _ => None,
        }
    }
}

impl MaybeIdent for PatType {
    fn maybe_ident(&self) -> Option<&Ident> {
        self.pat.maybe_ident()
    }
}

impl MaybeIdent for Pat {
    fn maybe_ident(&self) -> Option<&Ident> {
        match self {
            Pat::Ident(ident) => Some(&ident.ident),
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
            syn::GenericParam::Lifetime(syn::LifetimeParam { lifetime, .. }) => {
                Some(&lifetime.ident)
            }
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

pub trait MaybeIntoPath {
    fn maybe_into_path(self) -> Option<syn::Path>;
}

impl MaybeIntoPath for PatIdent {
    fn maybe_into_path(self) -> Option<syn::Path> {
        Some(self.ident.into())
    }
}

impl MaybeIntoPath for Pat {
    fn maybe_into_path(self) -> Option<syn::Path> {
        match self {
            Pat::Ident(pi) => pi.maybe_into_path(),
            _ => None,
        }
    }
}

pub trait RefPat {
    /// Return the reference to ident if any
    fn pat(&self) -> &Pat;
}

pub trait MaybePatIdent {
    fn maybe_patident(&self) -> Option<&syn::PatIdent>;
}

impl MaybePatIdent for FnArg {
    fn maybe_patident(&self) -> Option<&syn::PatIdent> {
        match self {
            FnArg::Typed(PatType { pat, .. }) => match pat.as_ref() {
                Pat::Ident(ident) => Some(ident),
                _ => None,
            },
            _ => None,
        }
    }
}

impl MaybePatIdent for Pat {
    fn maybe_patident(&self) -> Option<&syn::PatIdent> {
        match self {
            Pat::Ident(ident) => Some(ident),
            _ => None,
        }
    }
}

pub trait MaybePatType {
    fn maybe_pat_type(&self) -> Option<&syn::PatType>;
}

impl MaybePatType for FnArg {
    fn maybe_pat_type(&self) -> Option<&syn::PatType> {
        match self {
            FnArg::Typed(pt) => Some(pt),
            _ => None,
        }
    }
}

pub trait MaybePatTypeMut {
    fn maybe_pat_type_mut(&mut self) -> Option<&mut syn::PatType>;
}

impl MaybePatTypeMut for FnArg {
    fn maybe_pat_type_mut(&mut self) -> Option<&mut syn::PatType> {
        match self {
            FnArg::Typed(pt) => Some(pt),
            _ => None,
        }
    }
}

pub trait MaybePat {
    fn maybe_pat(&self) -> Option<&syn::Pat>;
}

impl MaybePat for FnArg {
    fn maybe_pat(&self) -> Option<&syn::Pat> {
        match self {
            FnArg::Typed(PatType { pat, .. }) => Some(pat.as_ref()),
            _ => None,
        }
    }
}

pub trait RemoveMutability {
    fn remove_mutability(&mut self);
}

impl RemoveMutability for FnArg {
    fn remove_mutability(&mut self) {
        if let FnArg::Typed(PatType { pat, .. }) = self {
            if let Pat::Ident(ident) = pat.as_mut() {
                ident.mutability = None
            }
        };
    }
}

pub trait IntoPat {
    fn into_pat(self) -> Pat;
}

impl IntoPat for Ident {
    fn into_pat(self) -> Pat {
        Pat::Ident(syn::PatIdent {
            attrs: vec![],
            by_ref: None,
            mutability: None,
            ident: self,
            subpat: None,
        })
    }
}
