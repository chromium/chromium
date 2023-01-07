use proc_macro2::TokenStream;
use quote::{format_ident, quote};
use syn::spanned::Spanned;

use crate::{
    diagnostic::{DiagnosticConcreteArgs, DiagnosticDef},
    forward::WhichFn,
    utils::{display_pat_members, gen_all_variants_with},
};

pub struct SourceCode {
    source_code: syn::Member,
}

impl SourceCode {
    pub fn from_fields(fields: &syn::Fields) -> syn::Result<Option<Self>> {
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
                if attr.path.is_ident("source_code") {
                    let source_code = if let Some(ident) = field.ident.clone() {
                        syn::Member::Named(ident)
                    } else {
                        syn::Member::Unnamed(syn::Index {
                            index: i as u32,
                            span: field.span(),
                        })
                    };
                    return Ok(Some(SourceCode { source_code }));
                }
            }
        }
        Ok(None)
    }

    pub(crate) fn gen_struct(&self, fields: &syn::Fields) -> Option<TokenStream> {
        let (display_pat, _display_members) = display_pat_members(fields);
        let src = &self.source_code;
        Some(quote! {
            #[allow(unused_variables)]
            fn source_code(&self) -> std::option::Option<&dyn miette::SourceCode> {
                let Self #display_pat = self;
                Some(&self.#src)
            }
        })
    }

    pub(crate) fn gen_enum(variants: &[DiagnosticDef]) -> Option<TokenStream> {
        gen_all_variants_with(
            variants,
            WhichFn::SourceCode,
            |ident, fields, DiagnosticConcreteArgs { source_code, .. }| {
                let (display_pat, _display_members) = display_pat_members(fields);
                source_code.as_ref().and_then(|source_code| {
                    let field = match &source_code.source_code {
                        syn::Member::Named(ident) => ident.clone(),
                        syn::Member::Unnamed(syn::Index { index, .. }) => {
                            format_ident!("_{}", index)
                        }
                    };
                    let variant_name = ident.clone();
                    match &fields {
                        syn::Fields::Unit => None,
                        _ => Some(quote! {
                            Self::#variant_name #display_pat => std::option::Option::Some(#field),
                        }),
                    }
                })
            },
        )
    }
}
