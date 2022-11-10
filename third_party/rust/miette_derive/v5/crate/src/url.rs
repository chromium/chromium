use proc_macro2::TokenStream;
use quote::quote;
use syn::{
    parenthesized,
    parse::{Parse, ParseStream},
    Fields, Token,
};

use crate::{
    diagnostic::{DiagnosticConcreteArgs, DiagnosticDef},
    utils::{display_pat_members, gen_all_variants_with, gen_unused_pat},
};
use crate::{
    fmt::{self, Display},
    forward::WhichFn,
};

pub enum Url {
    Display(Display),
    DocsRs,
}

impl Parse for Url {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        let ident = input.parse::<syn::Ident>()?;
        if ident == "url" {
            let la = input.lookahead1();
            if la.peek(syn::token::Paren) {
                let content;
                parenthesized!(content in input);
                if content.peek(syn::LitStr) {
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
                    Ok(Url::Display(display))
                } else {
                    let option = content.parse::<syn::Ident>()?;
                    if option == "docsrs" {
                        Ok(Url::DocsRs)
                    } else {
                        Err(syn::Error::new(option.span(), "Invalid argument to url() sub-attribute. It must be either a string or a plain `docsrs` identifier"))
                    }
                }
            } else {
                input.parse::<Token![=]>()?;
                Ok(Url::Display(Display {
                    fmt: input.parse()?,
                    args: TokenStream::new(),
                    has_bonus_display: false,
                }))
            }
        } else {
            Err(syn::Error::new(ident.span(), "not a url"))
        }
    }
}

impl Url {
    pub(crate) fn gen_enum(
        enum_name: &syn::Ident,
        variants: &[DiagnosticDef],
    ) -> Option<TokenStream> {
        gen_all_variants_with(
            variants,
            WhichFn::Url,
            |ident, fields, DiagnosticConcreteArgs { url, .. }| {
                let (pat, fmt, args) = match url.as_ref()? {
                    // fall through to `_ => None` below
                    Url::Display(display) => {
                        let (display_pat, display_members) = display_pat_members(fields);
                        let (fmt, args) = display.expand_shorthand_cloned(&display_members);
                        (display_pat, fmt.value(), args)
                    }
                    Url::DocsRs => {
                        let pat = gen_unused_pat(fields);
                        let fmt =
                            "https://docs.rs/{crate_name}/{crate_version}/{mod_name}/{item_path}"
                                .into();
                        let item_path = format!("enum.{}.html#variant.{}", enum_name, ident);
                        let args = quote! {
                            ,
                            crate_name=env!("CARGO_PKG_NAME"),
                            crate_version=env!("CARGO_PKG_VERSION"),
                            mod_name=env!("CARGO_PKG_NAME").replace('-', "_"),
                            item_path=#item_path
                        };
                        (pat, fmt, args)
                    }
                };
                Some(quote! {
                    Self::#ident #pat => std::option::Option::Some(std::boxed::Box::new(format!(#fmt #args))),
                })
            },
        )
    }

    pub(crate) fn gen_struct(
        &self,
        struct_name: &syn::Ident,
        fields: &Fields,
    ) -> Option<TokenStream> {
        let (pat, fmt, args) = match self {
            Url::Display(display) => {
                let (display_pat, display_members) = display_pat_members(fields);
                let (fmt, args) = display.expand_shorthand_cloned(&display_members);
                (display_pat, fmt.value(), args)
            }
            Url::DocsRs => {
                let pat = gen_unused_pat(fields);
                let fmt =
                    "https://docs.rs/{crate_name}/{crate_version}/{mod_name}/{item_path}".into();
                let item_path = format!("struct.{}.html", struct_name);
                let args = quote! {
                    ,
                    crate_name=env!("CARGO_PKG_NAME"),
                    crate_version=env!("CARGO_PKG_VERSION"),
                    mod_name=env!("CARGO_PKG_NAME").replace('-', "_"),
                    item_path=#item_path
                };
                (pat, fmt, args)
            }
        };
        Some(quote! {
            fn url<'a>(&'a self) -> std::option::Option<std::boxed::Box<dyn std::fmt::Display + 'a>> {
                #[allow(unused_variables, deprecated)]
                let Self #pat = self;
                std::option::Option::Some(std::boxed::Box::new(format!(#fmt #args)))
            }
        })
    }
}
