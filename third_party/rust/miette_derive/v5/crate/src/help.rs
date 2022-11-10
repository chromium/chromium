use proc_macro2::TokenStream;
use quote::{format_ident, quote};
use syn::{
    parenthesized,
    parse::{Parse, ParseStream},
    spanned::Spanned,
    Fields, Token,
};

use crate::{
    diagnostic::{DiagnosticConcreteArgs, DiagnosticDef},
    utils::{display_pat_members, gen_all_variants_with},
};
use crate::{
    fmt::{self, Display},
    forward::WhichFn,
};

pub enum Help {
    Display(Display),
    Field(syn::Member, Box<syn::Type>),
}

impl Parse for Help {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        let ident = input.parse::<syn::Ident>()?;
        if ident == "help" {
            let la = input.lookahead1();
            if la.peek(syn::token::Paren) {
                let content;
                parenthesized!(content in input);
                let fmt = content.parse()?;
                let args = if content.is_empty() {
                    TokenStream::new()
                } else {
                    fmt::parse_token_expr(&content, false)?
                };
                let display = Display {
                    fmt,
                    args,
                    has_bonus_display: false,
                };
                Ok(Help::Display(display))
            } else {
                input.parse::<Token![=]>()?;
                Ok(Help::Display(Display {
                    fmt: input.parse()?,
                    args: TokenStream::new(),
                    has_bonus_display: false,
                }))
            }
        } else {
            Err(syn::Error::new(ident.span(), "not a help"))
        }
    }
}

impl Help {
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
                if attr.path.is_ident("help") {
                    let help = if let Some(ident) = field.ident.clone() {
                        syn::Member::Named(ident)
                    } else {
                        syn::Member::Unnamed(syn::Index {
                            index: i as u32,
                            span: field.span(),
                        })
                    };
                    return Ok(Some(Help::Field(help, Box::new(field.ty.clone()))));
                }
            }
        }
        Ok(None)
    }
    pub(crate) fn gen_enum(variants: &[DiagnosticDef]) -> Option<TokenStream> {
        gen_all_variants_with(
            variants,
            WhichFn::Help,
            |ident, fields, DiagnosticConcreteArgs { help, .. }| {
                let (display_pat, display_members) = display_pat_members(fields);
                match &help.as_ref()? {
                    Help::Display(display) => {
                        let (fmt, args) = display.expand_shorthand_cloned(&display_members);
                        Some(quote! {
                            Self::#ident #display_pat => std::option::Option::Some(std::boxed::Box::new(format!(#fmt #args))),
                        })
                    }
                    Help::Field(member, ty) => {
                        let help = match &member {
                            syn::Member::Named(ident) => ident.clone(),
                            syn::Member::Unnamed(syn::Index { index, .. }) => {
                                format_ident!("_{}", index)
                            }
                        };
                        let var = quote! { __miette_internal_var };
                        Some(quote! {
                            Self::#ident #display_pat => {
                                use miette::macro_helpers::ToOption;
                                miette::macro_helpers::OptionalWrapper::<#ty>::new().to_option(&#help).as_ref().map(|#var| -> std::boxed::Box<dyn std::fmt::Display + 'a> { std::boxed::Box::new(format!("{}", #var)) })
                            },
                        })
                    }
                }
            },
        )
    }

    pub(crate) fn gen_struct(&self, fields: &Fields) -> Option<TokenStream> {
        let (display_pat, display_members) = display_pat_members(fields);
        match self {
            Help::Display(display) => {
                let (fmt, args) = display.expand_shorthand_cloned(&display_members);
                Some(quote! {
                    fn help<'a>(&'a self) -> std::option::Option<std::boxed::Box<dyn std::fmt::Display + 'a>> {
                        #[allow(unused_variables, deprecated)]
                        let Self #display_pat = self;
                        std::option::Option::Some(std::boxed::Box::new(format!(#fmt #args)))
                    }
                })
            }
            Help::Field(member, ty) => {
                let var = quote! { __miette_internal_var };
                Some(quote! {
                    fn help<'a>(&'a self) -> std::option::Option<std::boxed::Box<dyn std::fmt::Display + 'a>> {
                        #[allow(unused_variables, deprecated)]
                        let Self #display_pat = self;
                        use miette::macro_helpers::ToOption;
                        miette::macro_helpers::OptionalWrapper::<#ty>::new().to_option(&self.#member).as_ref().map(|#var| -> std::boxed::Box<dyn std::fmt::Display + 'a> { std::boxed::Box::new(format!("{}", #var)) })
                    }
                })
            }
        }
    }
}
