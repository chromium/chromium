use proc_macro2::TokenStream;
use quote::{format_ident, quote};
use syn::{
    parenthesized,
    parse::{Parse, ParseStream},
    spanned::Spanned,
};

pub enum Forward {
    Unnamed(usize),
    Named(syn::Ident),
}

impl Parse for Forward {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        let forward = input.parse::<syn::Ident>()?;
        if forward != "forward" {
            return Err(syn::Error::new(forward.span(), "msg"));
        }
        let content;
        parenthesized!(content in input);
        let looky = content.lookahead1();
        if looky.peek(syn::LitInt) {
            let int: syn::LitInt = content.parse()?;
            let index = int.base10_parse()?;
            return Ok(Forward::Unnamed(index));
        }
        Ok(Forward::Named(content.parse()?))
    }
}

#[derive(Copy, Clone)]
pub enum WhichFn {
    Code,
    Help,
    Url,
    Severity,
    Labels,
    SourceCode,
    Related,
}

impl WhichFn {
    pub fn method_call(&self) -> TokenStream {
        match self {
            Self::Code => quote! { code() },
            Self::Help => quote! { help() },
            Self::Url => quote! { url() },
            Self::Severity => quote! { severity() },
            Self::Labels => quote! { labels() },
            Self::SourceCode => quote! { source_code() },
            Self::Related => quote! { related() },
        }
    }

    pub fn signature(&self) -> TokenStream {
        match self {
            Self::Code => quote! {
                fn code<'a>(&'a self) -> std::option::Option<std::boxed::Box<dyn std::fmt::Display + 'a>>
            },
            Self::Help => quote! {
                fn help<'a>(&'a self) -> std::option::Option<std::boxed::Box<dyn std::fmt::Display + 'a>>
            },
            Self::Url => quote! {
                fn url<'a>(&'a self) -> std::option::Option<std::boxed::Box<dyn std::fmt::Display + 'a>>
            },
            Self::Severity => quote! {
                fn severity(&self) -> std::option::Option<miette::Severity>
            },
            Self::Related => quote! {
                fn related(&self) -> std::option::Option<std::boxed::Box<dyn std::iter::Iterator<Item = &dyn miette::Diagnostic> + '_>>
            },
            Self::Labels => quote! {
                fn labels(&self) -> std::option::Option<std::boxed::Box<dyn std::iter::Iterator<Item = miette::LabeledSpan> + '_>>
            },
            Self::SourceCode => quote! {
                fn source_code(&self) -> std::option::Option<&dyn miette::SourceCode>
            },
        }
    }

    pub fn catchall_arm(&self) -> TokenStream {
        quote! { _ => std::option::Option::None }
    }
}

impl Forward {
    pub fn for_transparent_field(fields: &syn::Fields) -> syn::Result<Self> {
        let make_err = || {
            syn::Error::new(
                fields.span(),
                "you can only use #[diagnostic(transparent)] with exactly one field",
            )
        };
        match fields {
            syn::Fields::Named(named) => {
                let mut iter = named.named.iter();
                let field = iter.next().ok_or_else(make_err)?;
                if iter.next().is_some() {
                    return Err(make_err());
                }
                let field_name = field
                    .ident
                    .clone()
                    .unwrap_or_else(|| format_ident!("unnamed"));
                Ok(Self::Named(field_name))
            }
            syn::Fields::Unnamed(unnamed) => {
                if unnamed.unnamed.iter().len() != 1 {
                    return Err(make_err());
                }
                Ok(Self::Unnamed(0))
            }
            _ => Err(syn::Error::new(
                fields.span(),
                "you cannot use #[diagnostic(transparent)] with a unit struct or a unit variant",
            )),
        }
    }

    pub fn gen_struct_method(&self, which_fn: WhichFn) -> TokenStream {
        let signature = which_fn.signature();
        let method_call = which_fn.method_call();

        let field_name = match self {
            Forward::Named(field_name) => quote!(#field_name),
            Forward::Unnamed(index) => {
                let index = syn::Index::from(*index);
                quote!(#index)
            }
        };

        quote! {
            #[inline]
            #signature {
                self.#field_name.#method_call
            }
        }
    }

    pub fn gen_enum_match_arm(&self, variant: &syn::Ident, which_fn: WhichFn) -> TokenStream {
        let method_call = which_fn.method_call();
        match self {
            Forward::Named(field_name) => quote! {
                Self::#variant { #field_name, .. } => #field_name.#method_call,
            },
            Forward::Unnamed(index) => {
                let underscores: Vec<_> = core::iter::repeat(quote! { _, }).take(*index).collect();
                let unnamed = format_ident!("unnamed");
                quote! {
                    Self::#variant ( #(#underscores)* #unnamed, .. ) => #unnamed.#method_call,
                }
            }
        }
    }
}
