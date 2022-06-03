use crate::syntax::instantiate::NamedImplKey;
use crate::syntax::resolve::Resolution;
use crate::syntax::Impl;
use proc_macro2::TokenStream;
use quote::ToTokens;
use syn::Token;

pub struct ImplGenerics<'a> {
    explicit_impl: Option<&'a Impl>,
    resolve: Resolution<'a>,
}

pub struct TyGenerics<'a> {
    key: NamedImplKey<'a>,
    explicit_impl: Option<&'a Impl>,
    resolve: Resolution<'a>,
}

pub fn split_for_impl<'a>(
    key: NamedImplKey<'a>,
    explicit_impl: Option<&'a Impl>,
    resolve: Resolution<'a>,
) -> (ImplGenerics<'a>, TyGenerics<'a>) {
    let impl_generics = ImplGenerics {
        explicit_impl,
        resolve,
    };
    let ty_generics = TyGenerics {
        key,
        explicit_impl,
        resolve,
    };
    (impl_generics, ty_generics)
}

impl<'a> ToTokens for ImplGenerics<'a> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        if let Some(imp) = self.explicit_impl {
            imp.impl_generics.to_tokens(tokens);
        } else {
            self.resolve.generics.to_tokens(tokens);
        }
    }
}

impl<'a> ToTokens for TyGenerics<'a> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        if let Some(imp) = self.explicit_impl {
            imp.ty_generics.to_tokens(tokens);
        } else if !self.resolve.generics.lifetimes.is_empty() {
            let span = self.key.rust.span();
            self.key
                .lt_token
                .unwrap_or_else(|| Token![<](span))
                .to_tokens(tokens);
            self.resolve.generics.lifetimes.to_tokens(tokens);
            self.key
                .gt_token
                .unwrap_or_else(|| Token![>](span))
                .to_tokens(tokens);
        }
    }
}
