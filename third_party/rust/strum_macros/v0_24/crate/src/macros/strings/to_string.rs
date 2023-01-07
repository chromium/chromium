use proc_macro2::TokenStream;
use quote::quote;
use syn::{Data, DeriveInput, Fields};

use crate::helpers::{non_enum_error, HasStrumVariantProperties, HasTypeProperties};

pub fn to_string_inner(ast: &DeriveInput) -> syn::Result<TokenStream> {
    let name = &ast.ident;
    let (impl_generics, ty_generics, where_clause) = ast.generics.split_for_impl();
    let variants = match &ast.data {
        Data::Enum(v) => &v.variants,
        _ => return Err(non_enum_error()),
    };

    let type_properties = ast.get_type_properties()?;
    let mut arms = Vec::new();
    for variant in variants {
        let ident = &variant.ident;
        let variant_properties = variant.get_variant_properties()?;

        if variant_properties.disabled.is_some() {
            continue;
        }

        // Look at all the serialize attributes.
        let output = variant_properties.get_preferred_name(type_properties.case_style);

        let params = match variant.fields {
            Fields::Unit => quote! {},
            Fields::Unnamed(..) => quote! { (..) },
            Fields::Named(..) => quote! { {..} },
        };

        arms.push(quote! { #name::#ident #params => ::std::string::String::from(#output) });
    }

    if arms.len() < variants.len() {
        arms.push(quote! { _ => panic!("to_string() called on disabled variant.") });
    }

    Ok(quote! {
        #[allow(clippy::use_self)]
        impl #impl_generics ::std::string::ToString for #name #ty_generics #where_clause {
            fn to_string(&self) -> ::std::string::String {
                match *self {
                    #(#arms),*
                }
            }
        }
    })
}
