use crate::expand::display_namespaced;
use crate::syntax::instantiate::NamedImplKey;
use crate::syntax::types::ConditionalImpl;
use crate::syntax::{Lifetimes, Type, Types};
use proc_macro2::TokenStream;
use quote::{quote, ToTokens};
use syn::{Lifetime, Token};

pub(crate) struct ResolvedGenericType<'a> {
    ty: &'a Type,
    explicit_impl: bool,
    types: &'a Types<'a>,
}

/// Gets `(impl_generics, inner_with_generics)` pair that can be used when
/// generating an `impl` for a generic type:
///
/// ```ignore
/// quote! { impl #impl_generics SomeTrait for #inner_with_generics }
/// ```
pub(crate) fn split_for_impl<'a>(
    key: &NamedImplKey<'a>,
    conditional_impl: &ConditionalImpl<'a>,
    types: &'a Types<'a>,
) -> (&'a Lifetimes, ResolvedGenericType<'a>) {
    let impl_generics = if let Some(explicit_impl) = conditional_impl.explicit_impl {
        &explicit_impl.impl_generics
    } else {
        get_impl_generics(key.inner, types)
    };
    let ty_generics = ResolvedGenericType {
        ty: key.inner,
        explicit_impl: conditional_impl.explicit_impl.is_some(),
        types,
    };
    (impl_generics, ty_generics)
}

impl<'a> ToTokens for ResolvedGenericType<'a> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        match self.ty {
            Type::Ident(named_type) => {
                named_type.rust.to_tokens(tokens);
                if self.explicit_impl {
                    named_type.generics.to_tokens(tokens);
                } else {
                    let resolve = self.types.resolve(named_type);
                    if !resolve.generics.lifetimes.is_empty() {
                        let span = named_type.rust.span();
                        named_type
                            .generics
                            .lt_token
                            .unwrap_or_else(|| Token![<](span))
                            .to_tokens(tokens);
                        resolve.generics.lifetimes.to_tokens(tokens);
                        named_type
                            .generics
                            .gt_token
                            .unwrap_or_else(|| Token![>](span))
                            .to_tokens(tokens);
                    }
                }
            }
            Type::RustBox(ty1) => {
                let inner = ResolvedGenericType {
                    ty: &ty1.inner,
                    explicit_impl: self.explicit_impl,
                    types: self.types,
                };
                tokens.extend(quote! {
                    ::cxx::alloc::boxed::Box<#inner>
                });
            }
            _ => unreachable!("syntax/check.rs should reject other types"),
        }
    }
}

fn get_impl_generics<'a>(ty: &Type, types: &Types<'a>) -> &'a Lifetimes {
    match ty {
        Type::Ident(named_type) => types.resolve(named_type).generics,
        Type::RustBox(ty1) => get_impl_generics(&ty1.inner, types),
        _ => unreachable!("syntax/check.rs should reject other types"),
    }
}

pub(crate) fn format_for_prevent_unwind_label(ty: &Type) -> TokenStream {
    match ty {
        Type::Ident(named_type) => {
            let rust_name = named_type.rust.to_string();
            quote! {
                ::cxx::core::concat!(::cxx::core::module_path!(), "::", #rust_name)
            }
        }
        Type::RustBox(ty1) => {
            let inner = format_for_prevent_unwind_label(&ty1.inner);
            quote! {
                ::cxx::core::concat!("Box<", #inner, ">")
            }
        }
        _ => unreachable!("syntax/check.rs should reject other types"),
    }
}

pub(crate) fn concise_rust_name(ty: &Type) -> String {
    match ty {
        Type::Ident(named_type) => named_type.rust.to_string(),
        Type::RustBox(ty1) => {
            let inner = concise_rust_name(&ty1.inner);
            format!("Box<{inner}>")
        }
        _ => unreachable!("syntax/check.rs should reject other types"),
    }
}

pub(crate) fn concise_cxx_name(ty: &Type, types: &Types) -> String {
    match ty {
        Type::Ident(named_type) => {
            let res = types.resolve(&named_type.rust);
            display_namespaced(res.name).to_string()
        }
        Type::RustBox(ty1) => {
            let inner = concise_cxx_name(&ty1.inner, types);
            format!("rust::Box<{inner}>")
        }
        _ => unreachable!("syntax/check.rs should reject other types"),
    }
}

pub(crate) struct UnderscoreLifetimes<'a> {
    generics: &'a Lifetimes,
}

impl Lifetimes {
    pub(crate) fn to_underscore_lifetimes(&self) -> UnderscoreLifetimes {
        UnderscoreLifetimes { generics: self }
    }
}

impl<'a> ToTokens for UnderscoreLifetimes<'a> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let Lifetimes {
            lt_token,
            lifetimes,
            gt_token,
        } = self.generics;
        lt_token.to_tokens(tokens);
        for pair in lifetimes.pairs() {
            let (lifetime, punct) = pair.into_tuple();
            let lifetime = Lifetime::new("'_", lifetime.span());
            lifetime.to_tokens(tokens);
            punct.to_tokens(tokens);
        }
        gt_token.to_tokens(tokens);
    }
}
