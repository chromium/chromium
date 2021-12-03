use crate::syntax::{derive, Enum, Struct, Trait};
use proc_macro2::{Ident, Span, TokenStream};
use quote::{quote, quote_spanned, ToTokens};

pub use crate::syntax::derive::*;

pub fn expand_struct(strct: &Struct, actual_derives: &mut Option<TokenStream>) -> TokenStream {
    let mut expanded = TokenStream::new();
    let mut traits = Vec::new();

    for derive in &strct.derives {
        let span = derive.span;
        match derive.what {
            Trait::Copy => expanded.extend(struct_copy(strct, span)),
            Trait::Clone => expanded.extend(struct_clone(strct, span)),
            Trait::Debug => expanded.extend(struct_debug(strct, span)),
            Trait::Default => expanded.extend(struct_default(strct, span)),
            Trait::Eq => traits.push(quote_spanned!(span=> ::std::cmp::Eq)),
            Trait::ExternType => unreachable!(),
            Trait::Hash => traits.push(quote_spanned!(span=> ::std::hash::Hash)),
            Trait::Ord => expanded.extend(struct_ord(strct, span)),
            Trait::PartialEq => traits.push(quote_spanned!(span=> ::std::cmp::PartialEq)),
            Trait::PartialOrd => expanded.extend(struct_partial_ord(strct, span)),
            Trait::Serialize => traits.push(quote_spanned!(span=> ::serde::Serialize)),
            Trait::Deserialize => traits.push(quote_spanned!(span=> ::serde::Deserialize)),
        }
    }

    if traits.is_empty() {
        *actual_derives = None;
    } else {
        *actual_derives = Some(quote!(#[derive(#(#traits),*)]));
    }

    expanded
}

pub fn expand_enum(enm: &Enum, actual_derives: &mut Option<TokenStream>) -> TokenStream {
    let mut expanded = TokenStream::new();
    let mut traits = Vec::new();
    let mut has_copy = false;
    let mut has_clone = false;
    let mut has_eq = false;
    let mut has_partial_eq = false;

    for derive in &enm.derives {
        let span = derive.span;
        match derive.what {
            Trait::Copy => {
                expanded.extend(enum_copy(enm, span));
                has_copy = true;
            }
            Trait::Clone => {
                expanded.extend(enum_clone(enm, span));
                has_clone = true;
            }
            Trait::Debug => expanded.extend(enum_debug(enm, span)),
            Trait::Default => unreachable!(),
            Trait::Eq => {
                traits.push(quote_spanned!(span=> ::std::cmp::Eq));
                has_eq = true;
            }
            Trait::ExternType => unreachable!(),
            Trait::Hash => traits.push(quote_spanned!(span=> ::std::hash::Hash)),
            Trait::Ord => expanded.extend(enum_ord(enm, span)),
            Trait::PartialEq => {
                traits.push(quote_spanned!(span=> ::std::cmp::PartialEq));
                has_partial_eq = true;
            }
            Trait::PartialOrd => expanded.extend(enum_partial_ord(enm, span)),
            Trait::Serialize => traits.push(quote_spanned!(span=> ::serde::Serialize)),
            Trait::Deserialize => traits.push(quote_spanned!(span=> ::serde::Deserialize)),
        }
    }

    let span = enm.name.rust.span();
    if !has_copy {
        expanded.extend(enum_copy(enm, span));
    }
    if !has_clone {
        expanded.extend(enum_clone(enm, span));
    }
    if !has_eq {
        // Required to be derived in order for the enum's "variants" to be
        // usable in patterns.
        traits.push(quote!(::std::cmp::Eq));
    }
    if !has_partial_eq {
        traits.push(quote!(::std::cmp::PartialEq));
    }

    *actual_derives = Some(quote!(#[derive(#(#traits),*)]));

    expanded
}

fn struct_copy(strct: &Struct, span: Span) -> TokenStream {
    let ident = &strct.name.rust;
    let generics = &strct.generics;

    quote_spanned! {span=>
        impl #generics ::std::marker::Copy for #ident #generics {}
    }
}

fn struct_clone(strct: &Struct, span: Span) -> TokenStream {
    let ident = &strct.name.rust;
    let generics = &strct.generics;

    let body = if derive::contains(&strct.derives, Trait::Copy) {
        quote!(*self)
    } else {
        let fields = strct.fields.iter().map(|field| &field.name.rust);
        let values = strct.fields.iter().map(|field| {
            let ident = &field.name.rust;
            let ty = field.ty.to_token_stream();
            let span = ty.into_iter().last().unwrap().span();
            quote_spanned!(span=> &self.#ident)
        });
        quote_spanned!(span=> #ident {
            #(#fields: ::std::clone::Clone::clone(#values),)*
        })
    };

    quote_spanned! {span=>
        impl #generics ::std::clone::Clone for #ident #generics {
            fn clone(&self) -> Self {
                #body
            }
        }
    }
}

fn struct_debug(strct: &Struct, span: Span) -> TokenStream {
    let ident = &strct.name.rust;
    let generics = &strct.generics;
    let struct_name = ident.to_string();
    let fields = strct.fields.iter().map(|field| &field.name.rust);
    let field_names = fields.clone().map(Ident::to_string);

    quote_spanned! {span=>
        impl #generics ::std::fmt::Debug for #ident #generics {
            fn fmt(&self, formatter: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
                formatter.debug_struct(#struct_name)
                    #(.field(#field_names, &self.#fields))*
                    .finish()
            }
        }
    }
}

fn struct_default(strct: &Struct, span: Span) -> TokenStream {
    let ident = &strct.name.rust;
    let generics = &strct.generics;
    let fields = strct.fields.iter().map(|field| &field.name.rust);

    quote_spanned! {span=>
        #[allow(clippy::derivable_impls)] // different spans than the derived impl
        impl #generics ::std::default::Default for #ident #generics {
            fn default() -> Self {
                #ident {
                    #(
                        #fields: ::std::default::Default::default(),
                    )*
                }
            }
        }
    }
}

fn struct_ord(strct: &Struct, span: Span) -> TokenStream {
    let ident = &strct.name.rust;
    let generics = &strct.generics;
    let fields = strct.fields.iter().map(|field| &field.name.rust);

    quote_spanned! {span=>
        impl #generics ::std::cmp::Ord for #ident #generics {
            fn cmp(&self, other: &Self) -> ::std::cmp::Ordering {
                #(
                    match ::std::cmp::Ord::cmp(&self.#fields, &other.#fields) {
                        ::std::cmp::Ordering::Equal => {}
                        ordering => return ordering,
                    }
                )*
                ::std::cmp::Ordering::Equal
            }
        }
    }
}

fn struct_partial_ord(strct: &Struct, span: Span) -> TokenStream {
    let ident = &strct.name.rust;
    let generics = &strct.generics;

    let body = if derive::contains(&strct.derives, Trait::Ord) {
        quote! {
            ::std::option::Option::Some(::std::cmp::Ord::cmp(self, other))
        }
    } else {
        let fields = strct.fields.iter().map(|field| &field.name.rust);
        quote! {
            #(
                match ::std::cmp::PartialOrd::partial_cmp(&self.#fields, &other.#fields) {
                    ::std::option::Option::Some(::std::cmp::Ordering::Equal) => {}
                    ordering => return ordering,
                }
            )*
            ::std::option::Option::Some(::std::cmp::Ordering::Equal)
        }
    };

    quote_spanned! {span=>
        impl #generics ::std::cmp::PartialOrd for #ident #generics {
            fn partial_cmp(&self, other: &Self) -> ::std::option::Option<::std::cmp::Ordering> {
                #body
            }
        }
    }
}

fn enum_copy(enm: &Enum, span: Span) -> TokenStream {
    let ident = &enm.name.rust;

    quote_spanned! {span=>
        impl ::std::marker::Copy for #ident {}
    }
}

fn enum_clone(enm: &Enum, span: Span) -> TokenStream {
    let ident = &enm.name.rust;

    quote_spanned! {span=>
        impl ::std::clone::Clone for #ident {
            fn clone(&self) -> Self {
                *self
            }
        }
    }
}

fn enum_debug(enm: &Enum, span: Span) -> TokenStream {
    let ident = &enm.name.rust;
    let variants = enm.variants.iter().map(|variant| {
        let variant = &variant.name.rust;
        let name = variant.to_string();
        quote_spanned! {span=>
            #ident::#variant => formatter.write_str(#name),
        }
    });
    let fallback = format!("{}({{}})", ident);

    quote_spanned! {span=>
        impl ::std::fmt::Debug for #ident {
            fn fmt(&self, formatter: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
                match *self {
                    #(#variants)*
                    _ => ::std::write!(formatter, #fallback, self.repr),
                }
            }
        }
    }
}

fn enum_ord(enm: &Enum, span: Span) -> TokenStream {
    let ident = &enm.name.rust;

    quote_spanned! {span=>
        impl ::std::cmp::Ord for #ident {
            fn cmp(&self, other: &Self) -> ::std::cmp::Ordering {
                ::std::cmp::Ord::cmp(&self.repr, &other.repr)
            }
        }
    }
}

fn enum_partial_ord(enm: &Enum, span: Span) -> TokenStream {
    let ident = &enm.name.rust;

    quote_spanned! {span=>
        impl ::std::cmp::PartialOrd for #ident {
            fn partial_cmp(&self, other: &Self) -> ::std::option::Option<::std::cmp::Ordering> {
                ::std::cmp::PartialOrd::partial_cmp(&self.repr, &other.repr)
            }
        }
    }
}
