use proc_macro2::TokenStream;
use quote::{format_ident, quote};
use syn::{
    parenthesized,
    parse::{Parse, ParseStream},
    spanned::Spanned,
    Token,
};

use crate::{
    diagnostic::{DiagnosticConcreteArgs, DiagnosticDef},
    fmt::{self, Display},
    forward::WhichFn,
    utils::{display_pat_members, gen_all_variants_with},
};

pub struct Labels(Vec<Label>);

struct Label {
    label: Option<Display>,
    span: syn::Member,
}

struct LabelAttr {
    label: Option<Display>,
}

impl Parse for LabelAttr {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        let la = input.lookahead1();
        let label = if la.peek(syn::token::Paren) {
            // #[label("{}", x)]
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
                Some(display)
            } else {
                return Err(syn::Error::new(input.span(), "Invalid argument to label() attribute. The first argument must be a literal string."));
            }
        } else if la.peek(Token![=]) {
            // #[label = "blabla"]
            input.parse::<Token![=]>()?;
            Some(Display {
                fmt: input.parse()?,
                args: TokenStream::new(),
                has_bonus_display: false,
            })
        } else {
            None
        };
        Ok(LabelAttr { label })
    }
}

impl Labels {
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
        let mut labels = Vec::new();
        for (i, field) in fields.iter().enumerate() {
            for attr in &field.attrs {
                if attr.path.is_ident("label") {
                    let span = if let Some(ident) = field.ident.clone() {
                        syn::Member::Named(ident)
                    } else {
                        syn::Member::Unnamed(syn::Index {
                            index: i as u32,
                            span: field.span(),
                        })
                    };
                    let LabelAttr { label } = syn::parse2::<LabelAttr>(attr.tokens.clone())?;
                    labels.push(Label { label, span });
                }
            }
        }
        if labels.is_empty() {
            Ok(None)
        } else {
            Ok(Some(Labels(labels)))
        }
    }

    pub(crate) fn gen_struct(&self, fields: &syn::Fields) -> Option<TokenStream> {
        let (display_pat, display_members) = display_pat_members(fields);
        let labels = self.0.iter().map(|highlight| {
            let Label { span, label } = highlight;
            if let Some(display) = label {
                let (fmt, args) = display.expand_shorthand_cloned(&display_members);
                quote! {
                    miette::LabeledSpan::new_with_span(
                        std::option::Option::Some(format!(#fmt #args)),
                        self.#span.clone(),
                    )
                }
            } else {
                quote! {
                    miette::LabeledSpan::new_with_span(
                        std::option::Option::None,
                        self.#span.clone(),
                    )
                }
            }
        });
        Some(quote! {
            #[allow(unused_variables)]
            fn labels(&self) -> std::option::Option<std::boxed::Box<dyn std::iter::Iterator<Item = miette::LabeledSpan> + '_>> {
                let Self #display_pat = self;
                Some(Box::new(vec![
                    #(#labels),*
                ].into_iter()))
            }
        })
    }

    pub(crate) fn gen_enum(variants: &[DiagnosticDef]) -> Option<TokenStream> {
        gen_all_variants_with(
            variants,
            WhichFn::Labels,
            |ident, fields, DiagnosticConcreteArgs { labels, .. }| {
                let (display_pat, display_members) = display_pat_members(fields);
                labels.as_ref().and_then(|labels| {
                let variant_labels = labels.0.iter().map(|label| {
                    let Label { span, label } = label;
                    let field = match &span {
                        syn::Member::Named(ident) => ident.clone(),
                        syn::Member::Unnamed(syn::Index { index, .. }) => {
                            format_ident!("_{}", index)
                        }
                    };
                    if let Some(display) = label {
                        let (fmt, args) = display.expand_shorthand_cloned(&display_members);
                        quote! {
                            miette::LabeledSpan::new_with_span(
                                std::option::Option::Some(format!(#fmt #args)),
                                #field.clone(),
                            )
                        }
                    } else {
                        quote! {
                            miette::LabeledSpan::new_with_span(
                                std::option::Option::None,
                                #field.clone(),
                            )
                        }
                    }
                });
                let variant_name = ident.clone();
                match &fields {
                    syn::Fields::Unit => None,
                    _ => Some(quote! {
                        Self::#variant_name #display_pat => std::option::Option::Some(std::boxed::Box::new(vec![
                            #(#variant_labels),*
                        ].into_iter())),
                    }),
                }
                })
            },
        )
    }
}
