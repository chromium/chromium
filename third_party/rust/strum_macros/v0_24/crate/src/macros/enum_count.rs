use proc_macro2::TokenStream;
use quote::quote;
use syn::{Data, DeriveInput};

use crate::helpers::{non_enum_error, HasTypeProperties};

pub(crate) fn enum_count_inner(ast: &DeriveInput) -> syn::Result<TokenStream> {
    let n = match &ast.data {
        Data::Enum(v) => v.variants.len(),
        _ => return Err(non_enum_error()),
    };
    let type_properties = ast.get_type_properties()?;
    let strum_module_path = type_properties.crate_module_path();

    // Used in the quasi-quotation below as `#name`
    let name = &ast.ident;

    // Helper is provided for handling complex generic types correctly and effortlessly
    let (impl_generics, ty_generics, where_clause) = ast.generics.split_for_impl();

    Ok(quote! {
        // Implementation
        impl #impl_generics #strum_module_path::EnumCount for #name #ty_generics #where_clause {
            const COUNT: usize = #n;
        }
    })
}
