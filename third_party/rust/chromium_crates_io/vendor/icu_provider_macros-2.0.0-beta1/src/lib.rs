// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// https://github.com/unicode-org/icu4x/blob/main/documents/process/boilerplate.md#library-annotations
#![cfg_attr(
    not(test),
    deny(
        clippy::indexing_slicing,
        clippy::unwrap_used,
        clippy::expect_used,
        // Panics are OK in proc macros
        // clippy::panic,
        clippy::exhaustive_structs,
        clippy::exhaustive_enums,
        missing_debug_implementations,
    )
)]
#![warn(missing_docs)]

//! Proc macros for the ICU4X data provider.
//!
//! These macros are re-exported from `icu_provider`.

extern crate proc_macro;
use proc_macro::TokenStream;
use proc_macro2::Span;
use proc_macro2::TokenStream as TokenStream2;
use quote::quote;
use syn::parenthesized;
use syn::parse::{self, Parse, ParseStream};
use syn::parse_macro_input;
use syn::punctuated::Punctuated;
use syn::spanned::Spanned;
use syn::DeriveInput;
use syn::{Ident, LitStr, Path, Token};
#[cfg(test)]
mod tests;

/// The `#[data_struct]` attribute should be applied to all types intended
/// for use in a `DataStruct`.
///
/// It does the following things:
///
/// - `Apply #[derive(Yokeable, ZeroFrom)]`. The `ZeroFrom` derive can
///    be customized with `#[zerofrom(clone)]` on non-ZeroFrom fields.
///
/// In addition, the attribute can be used to implement `DynamicDataMarker` and/or `DataMarker`
/// by adding symbols with optional marker path strings:
///
/// ```
/// # // We DO NOT want to pull in the `icu` crate as a dev-dependency,
/// # // because that will rebuild the whole tree in proc macro mode
/// # // when using cargo test --all-features --all-targets.
/// # pub mod icu {
/// #   pub mod locale {
/// #     pub mod fallback {
/// #       pub use icu_provider::fallback::LocaleFallbackPriority;
/// #     }
/// #   }
/// # }
/// use icu::locale::fallback::*;
/// use icu_provider::prelude::*;
/// use std::borrow::Cow;
///
/// #[icu_provider::data_struct(
///     FooV1Marker,
///     BarV1Marker = "demo/bar@1",
///     marker(BazV1Marker, "demo/baz@1", fallback_by = "region",)
/// )]
/// pub struct FooV1<'data> {
///     message: Cow<'data, str>,
/// };
///
/// // Note: FooV1Marker implements `DynamicDataMarker` but not `DataMarker`.
/// // The other two implement `DataMarker`.
///
/// assert_eq!(BarV1Marker::INFO.path.as_str(), "demo/bar@1");
/// assert_eq!(
///     BarV1Marker::INFO.fallback_config.priority,
///     LocaleFallbackPriority::Language
/// );
///
/// assert_eq!(BazV1Marker::INFO.path.as_str(), "demo/baz@1");
/// assert_eq!(
///     BazV1Marker::INFO.fallback_config.priority,
///     LocaleFallbackPriority::Region
/// );
/// ```
#[proc_macro_attribute]
pub fn data_struct(attr: TokenStream, item: TokenStream) -> TokenStream {
    TokenStream::from(data_struct_impl(
        parse_macro_input!(attr as DataStructArgs),
        parse_macro_input!(item as DeriveInput),
    ))
}

pub(crate) struct DataStructArgs {
    args: Punctuated<DataStructArg, Token![,]>,
}

impl Parse for DataStructArgs {
    fn parse(input: ParseStream<'_>) -> parse::Result<Self> {
        let args = input.parse_terminated(DataStructArg::parse, Token![,])?;
        Ok(Self { args })
    }
}
struct DataStructArg {
    marker_name: Path,
    path_lit: Option<LitStr>,
    fallback_by: Option<LitStr>,
    attributes_domain: Option<LitStr>,
    singleton: bool,
}

impl DataStructArg {
    fn new(marker_name: Path) -> Self {
        Self {
            marker_name,
            path_lit: None,
            fallback_by: None,
            attributes_domain: None,
            singleton: false,
        }
    }
}

impl Parse for DataStructArg {
    fn parse(input: ParseStream<'_>) -> parse::Result<Self> {
        let path: Path = input.parse()?;

        fn at_most_one_option<T>(
            o: &mut Option<T>,
            new: T,
            name: &str,
            span: Span,
        ) -> parse::Result<()> {
            if o.replace(new).is_some() {
                Err(parse::Error::new(
                    span,
                    format!("marker() cannot contain multiple {name}s"),
                ))
            } else {
                Ok(())
            }
        }

        if path.is_ident("marker") {
            let content;
            let paren = parenthesized!(content in input);
            let mut marker_name: Option<Path> = None;
            let mut path_lit: Option<LitStr> = None;
            let mut fallback_by: Option<LitStr> = None;
            let mut attributes_domain: Option<LitStr> = None;
            let mut singleton = false;
            let punct = content.parse_terminated(DataStructMarkerArg::parse, Token![,])?;

            for entry in punct {
                match entry {
                    DataStructMarkerArg::Path(path) => {
                        at_most_one_option(&mut marker_name, path, "marker", input.span())?;
                    }
                    DataStructMarkerArg::NameValue(name, value) => {
                        if name == "fallback_by" {
                            at_most_one_option(
                                &mut fallback_by,
                                value,
                                "fallback_by",
                                paren.span.join(),
                            )?;
                        } else if name == "attributes_domain" {
                            at_most_one_option(
                                &mut attributes_domain,
                                value,
                                "attributes_domain",
                                paren.span.join(),
                            )?;
                        } else {
                            return Err(parse::Error::new(
                                name.span(),
                                format!("unknown option {name} in marker()"),
                            ));
                        }
                    }
                    DataStructMarkerArg::Lit(lit) => {
                        at_most_one_option(&mut path_lit, lit, "literal path", input.span())?;
                    }
                    DataStructMarkerArg::Singleton => {
                        singleton = true;
                    }
                }
            }
            let marker_name = if let Some(marker_name) = marker_name {
                marker_name
            } else {
                return Err(parse::Error::new(
                    input.span(),
                    "marker() must contain a marker!",
                ));
            };

            Ok(Self {
                marker_name,
                path_lit,
                fallback_by,
                attributes_domain,
                singleton,
            })
        } else {
            let mut this = DataStructArg::new(path);
            let lookahead = input.lookahead1();
            if lookahead.peek(Token![=]) {
                let _t: Token![=] = input.parse()?;
                let lit: LitStr = input.parse()?;
                this.path_lit = Some(lit);
                Ok(this)
            } else {
                Ok(this)
            }
        }
    }
}

/// A single argument to `marker()` in `#[data_struct(..., marker(...), ...)]
enum DataStructMarkerArg {
    Path(Path),
    NameValue(Ident, LitStr),
    Lit(LitStr),
    Singleton,
}
impl Parse for DataStructMarkerArg {
    fn parse(input: ParseStream<'_>) -> parse::Result<Self> {
        let lookahead = input.lookahead1();
        if lookahead.peek(LitStr) {
            Ok(DataStructMarkerArg::Lit(input.parse()?))
        } else {
            let path: Path = input.parse()?;
            let lookahead = input.lookahead1();
            if lookahead.peek(Token![=]) {
                let _tok: Token![=] = input.parse()?;
                let ident = path.get_ident().ok_or_else(|| {
                    parse::Error::new(path.span(), "Expected identifier before `=`, found path")
                })?;
                Ok(DataStructMarkerArg::NameValue(
                    ident.clone(),
                    input.parse()?,
                ))
            } else if path.is_ident("singleton") {
                Ok(DataStructMarkerArg::Singleton)
            } else {
                Ok(DataStructMarkerArg::Path(path))
            }
        }
    }
}

fn data_struct_impl(attr: DataStructArgs, input: DeriveInput) -> TokenStream2 {
    if input.generics.type_params().count() > 0 {
        return syn::Error::new(
            input.generics.span(),
            "#[data_struct] does not support type parameters",
        )
        .to_compile_error();
    }
    let lifetimes = input.generics.lifetimes().collect::<Vec<_>>();

    let name = &input.ident;

    let name_with_lt = if !lifetimes.is_empty() {
        quote!(#name<'static>)
    } else {
        quote!(#name)
    };

    if lifetimes.len() > 1 {
        return syn::Error::new(
            input.generics.span(),
            "#[data_struct] does not support more than one lifetime parameter",
        )
        .to_compile_error();
    }

    let mut result = TokenStream2::new();

    for single_attr in attr.args {
        let DataStructArg {
            marker_name,
            path_lit,
            fallback_by,
            attributes_domain,
            singleton,
        } = single_attr;

        let docs = if let Some(ref path_lit) = path_lit {
            let fallback_by_docs_str = match fallback_by {
                Some(ref fallback_by) => fallback_by.value(),
                None => "language (default)".to_string(),
            };
            format!(
                "Marker type for [`{name}`]: \"{}\"\n\n- Fallback priority: {fallback_by_docs_str}",
                path_lit.value()
            )
        } else {
            format!("Marker type for [`{name}`]")
        };

        result.extend(quote!(
            #[doc = #docs]
            pub struct #marker_name;
            impl icu_provider::DynamicDataMarker for #marker_name {
                type DataStruct = #name_with_lt;
            }
        ));

        if let Some(path_lit) = path_lit {
            let path_str = path_lit.value();
            let fallback_by_expr = if let Some(fallback_by_lit) = fallback_by {
                match fallback_by_lit.value().as_str() {
                    "region" => {
                        quote! {icu_provider::fallback::LocaleFallbackPriority::Region}
                    }
                    "script" => {
                        quote! {icu_provider::fallback::LocaleFallbackPriority::Script}
                    }
                    "language" => {
                        quote! {icu_provider::fallback::LocaleFallbackPriority::Language}
                    }
                    _ => panic!("Invalid value for fallback_by"),
                }
            } else {
                quote! {icu_provider::fallback::LocaleFallbackPriority::default()}
            };
            let attributes_domain_setter = if let Some(attributes_domain_lit) = attributes_domain {
                quote! { info.attributes_domain = #attributes_domain_lit; }
            } else {
                quote!()
            };
            result.extend(quote!(
                impl icu_provider::DataMarker for #marker_name {
                    const INFO: icu_provider::DataMarkerInfo = {
                        let mut info = icu_provider::DataMarkerInfo::from_path(icu_provider::marker::data_marker_path!(#path_str));
                        info.is_singleton = #singleton;
                        info.fallback_config.priority = #fallback_by_expr;
                        #attributes_domain_setter
                        info
                    };
                }
            ));
        }
    }

    result.extend(quote!(
        #[derive(icu_provider::prelude::yoke::Yokeable, icu_provider::prelude::zerofrom::ZeroFrom)]
        #input
    ));

    result
}
