use proc_macro2::TokenStream;
use quote::quote;
use syn::spanned::Spanned;

use crate::forward::WhichFn;
use crate::{
    diagnostic::{DiagnosticConcreteArgs, DiagnosticDef},
    utils::{display_pat_members, gen_all_variants_with},
};

pub struct DiagnosticSource(syn::Member);

impl DiagnosticSource {
    pub(crate) fn from_fields(fields: &syn::Fields) -> syn::Result<Option<Self>> {
        match fields {
            syn::Fields::Named(named) => Self::from_fields_vec(named.named.iter().collect()),
            syn::Fields::Unnamed(unnamed) => {
                Self::from_fields_vec(unnamed.unnamed.iter().collect())
            }
            syn::Fields::Unit => Ok(None),
        }
    }

    fn from_fields_vec(fields: Vec<&syn::Field>) -> syn::Result<Option<Self>> {
        for (i, field) in fields.iter().enumerate() {
            for attr in &field.attrs {
                if attr.path.is_ident("diagnostic_source") {
                    let diagnostic_source = if let Some(ident) = field.ident.clone() {
                        syn::Member::Named(ident)
                    } else {
                        syn::Member::Unnamed(syn::Index {
                            index: i as u32,
                            span: field.span(),
                        })
                    };
                    return Ok(Some(DiagnosticSource(diagnostic_source)));
                }
            }
        }
        Ok(None)
    }

    pub(crate) fn gen_enum(variants: &[DiagnosticDef]) -> Option<TokenStream> {
        gen_all_variants_with(
            variants,
            WhichFn::DiagnosticSource,
            |ident,
             fields,
             DiagnosticConcreteArgs {
                 diagnostic_source, ..
             }| {
                let (display_pat, _display_members) = display_pat_members(fields);
                diagnostic_source.as_ref().map(|diagnostic_source| {
                    let rel = match &diagnostic_source.0 {
                        syn::Member::Named(ident) => ident.clone(),
                        syn::Member::Unnamed(syn::Index { index, .. }) => {
                            quote::format_ident!("_{}", index)
                        }
                    };
                    quote! {
                        Self::#ident #display_pat => {
                            std::option::Option::Some(std::borrow::Borrow::borrow(#rel))
                        }
                    }
                })
            },
        )
    }

    pub(crate) fn gen_struct(&self) -> Option<TokenStream> {
        let rel = &self.0;
        Some(quote! {
            fn diagnostic_source<'a>(&'a self) -> std::option::Option<&'a dyn miette::Diagnostic> {
                std::option::Option::Some(std::borrow::Borrow::borrow(&self.#rel))
            }
        })
    }
}
