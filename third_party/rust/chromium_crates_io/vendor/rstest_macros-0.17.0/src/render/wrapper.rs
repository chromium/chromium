use proc_macro2::TokenStream;
use quote::{quote, ToTokens};
use syn::Ident;

pub(crate) trait WrapByModule {
    fn wrap_by_mod(&self, mod_name: &Ident) -> TokenStream;
}

impl<T: ToTokens> WrapByModule for T {
    fn wrap_by_mod(&self, mod_name: &Ident) -> TokenStream {
        quote! {
            mod #mod_name {
                use super::*;

                #self
            }
        }
    }
}
