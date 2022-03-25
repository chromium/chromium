// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    borrow::Cow,
    collections::{HashMap, HashSet},
};

use proc_macro2::Span;
use quote::ToTokens;
use syn::{
    parse::{Parse, ParseStream},
    Signature, Token,
};
use syn::{Ident, Result as ParseResult};

use crate::{
    directives::{EXTERN_RUST_TYPE, SUBCLASS},
    RustPath,
};

#[cfg(feature = "reproduction_case")]
use quote::quote;

#[derive(PartialEq, Clone, Debug, Hash)]
pub enum UnsafePolicy {
    AllFunctionsSafe,
    AllFunctionsUnsafe,
}

impl Parse for UnsafePolicy {
    fn parse(input: ParseStream) -> ParseResult<Self> {
        if input.parse::<Option<Token![unsafe]>>()?.is_some() {
            return Ok(UnsafePolicy::AllFunctionsSafe);
        }
        let r = match input.parse::<Option<syn::Ident>>()? {
            Some(id) => {
                if id == "unsafe_ffi" {
                    Ok(UnsafePolicy::AllFunctionsSafe)
                } else {
                    Err(syn::Error::new(id.span(), "expected unsafe_ffi"))
                }
            }
            None => Ok(UnsafePolicy::AllFunctionsUnsafe),
        };
        if !input.is_empty() {
            return Err(syn::Error::new(
                Span::call_site(),
                "unexpected tokens within safety directive",
            ));
        }
        r
    }
}

#[cfg(feature = "reproduction_case")]
impl ToTokens for UnsafePolicy {
    fn to_tokens(&self, tokens: &mut proc_macro2::TokenStream) {
        if *self == UnsafePolicy::AllFunctionsSafe {
            tokens.extend(quote! { unsafe })
        }
    }
}

/// An entry in the allowlist.
#[derive(Hash, Debug)]
pub enum AllowlistEntry {
    Item(String),
    Namespace(String),
}

impl AllowlistEntry {
    fn to_bindgen_item(&self) -> String {
        match self {
            AllowlistEntry::Item(i) => i.clone(),
            AllowlistEntry::Namespace(ns) => format!("{}::.*", ns),
        }
    }
}

/// Allowlist configuration.
#[derive(Hash, Debug)]
pub enum Allowlist {
    Unspecified(Vec<AllowlistEntry>),
    All,
    Specific(Vec<AllowlistEntry>),
}

impl Allowlist {
    pub fn push(&mut self, item: AllowlistEntry, span: Span) -> ParseResult<()> {
        match self {
            Allowlist::Unspecified(ref mut uncommitted_list) => {
                let new_list = uncommitted_list
                    .drain(..)
                    .chain(std::iter::once(item))
                    .collect();
                *self = Allowlist::Specific(new_list);
            }
            Allowlist::All => {
                return Err(syn::Error::new(
                    span,
                    "use either generate!/generate_pod!/generate_ns! or generate_all!, not both.",
                ))
            }
            Allowlist::Specific(list) => list.push(item),
        };
        Ok(())
    }

    pub(crate) fn set_all(&mut self, ident: &Ident) -> ParseResult<()> {
        if matches!(self, Allowlist::Specific(..)) {
            return Err(syn::Error::new(
                ident.span(),
                "use either generate!/generate_pod!/generate_ns! or generate_all!, not both.",
            ));
        }
        *self = Allowlist::All;
        Ok(())
    }
}

#[allow(clippy::derivable_impls)] // nightly-only
impl Default for Allowlist {
    fn default() -> Self {
        Allowlist::Unspecified(Vec::new())
    }
}

#[derive(Debug)]
pub struct Subclass {
    pub superclass: String,
    pub subclass: Ident,
}

pub struct RustFun {
    pub path: RustPath,
    pub sig: Signature,
}

impl std::fmt::Debug for RustFun {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("RustFun")
            .field("path", &self.path)
            .field("sig", &self.sig.to_token_stream().to_string())
            .finish()
    }
}

#[derive(Debug)]
pub struct IncludeCppConfig {
    pub inclusions: Vec<String>,
    pub unsafe_policy: UnsafePolicy,
    pub parse_only: bool,
    pub exclude_impls: bool,
    pod_requests: Vec<String>,
    pub allowlist: Allowlist,
    blocklist: Vec<String>,
    constructor_blocklist: Vec<String>,
    exclude_utilities: bool,
    mod_name: Option<Ident>,
    pub rust_types: Vec<RustPath>,
    pub subclasses: Vec<Subclass>,
    pub extern_rust_funs: Vec<RustFun>,
    pub concretes: HashMap<String, Ident>,
}

impl Parse for IncludeCppConfig {
    fn parse(input: ParseStream) -> ParseResult<Self> {
        // Takes as inputs:
        // 1. List of headers to include
        // 2. List of #defines to include
        // 3. Allowlist

        let mut inclusions = Vec::new();
        let mut parse_only = false;
        let mut exclude_impls = false;
        let mut unsafe_policy = UnsafePolicy::AllFunctionsUnsafe;
        let mut allowlist = Allowlist::default();
        let mut blocklist = Vec::new();
        let mut constructor_blocklist = Vec::new();
        let mut pod_requests = Vec::new();
        let mut rust_types = Vec::new();
        let mut exclude_utilities = false;
        let mut mod_name = None;
        let mut subclasses = Vec::new();
        let mut extern_rust_funs = Vec::new();
        let mut concretes = HashMap::new();

        while !input.is_empty() {
            let has_hexathorpe = input.parse::<Option<syn::token::Pound>>()?.is_some();
            let ident: syn::Ident = input.parse()?;
            if has_hexathorpe {
                if ident != "include" {
                    return Err(syn::Error::new(ident.span(), "expected include"));
                }
                let hdr: syn::LitStr = input.parse()?;
                inclusions.push(hdr.value());
            } else {
                input.parse::<Option<syn::token::Bang>>()?;
                if ident == "generate" {
                    let args;
                    syn::parenthesized!(args in input);
                    let generate: syn::LitStr = args.parse()?;
                    allowlist.push(AllowlistEntry::Item(generate.value()), generate.span())?;
                } else if ident == "generate_ns" {
                    let args;
                    syn::parenthesized!(args in input);
                    let generate_ns: syn::LitStr = args.parse()?;
                    allowlist.push(
                        AllowlistEntry::Namespace(generate_ns.value()),
                        generate_ns.span(),
                    )?;
                } else if ident == "generate_pod" {
                    let args;
                    syn::parenthesized!(args in input);
                    let generate_pod: syn::LitStr = args.parse()?;
                    pod_requests.push(generate_pod.value());
                    allowlist.push(
                        AllowlistEntry::Item(generate_pod.value()),
                        generate_pod.span(),
                    )?;
                } else if ident == "pod" {
                    let args;
                    syn::parenthesized!(args in input);
                    let pod: syn::LitStr = args.parse()?;
                    pod_requests.push(pod.value());
                } else if ident == "block" {
                    let args;
                    syn::parenthesized!(args in input);
                    let generate: syn::LitStr = args.parse()?;
                    blocklist.push(generate.value());
                } else if ident == "concrete" {
                    let args;
                    syn::parenthesized!(args in input);
                    let definition: syn::LitStr = args.parse()?;
                    args.parse::<syn::token::Comma>()?;
                    let rust_id: syn::Ident = args.parse()?;
                    concretes.insert(definition.value(), rust_id);
                } else if ident == "block_constructors" {
                    let args;
                    syn::parenthesized!(args in input);
                    let generate: syn::LitStr = args.parse()?;
                    constructor_blocklist.push(generate.value());
                } else if ident == "rust_type" || ident == EXTERN_RUST_TYPE {
                    let args;
                    syn::parenthesized!(args in input);
                    let id: Ident = args.parse()?;
                    rust_types.push(RustPath::new_from_ident(id));
                } else if ident == SUBCLASS {
                    let args;
                    syn::parenthesized!(args in input);
                    let superclass: syn::LitStr = args.parse()?;
                    args.parse::<syn::token::Comma>()?;
                    let subclass: syn::Ident = args.parse()?;
                    subclasses.push(Subclass {
                        superclass: superclass.value(),
                        subclass,
                    });
                } else if ident == "parse_only" {
                    parse_only = true;
                    swallow_parentheses(&input, &ident)?;
                } else if ident == "exclude_impls" {
                    exclude_impls = true;
                    swallow_parentheses(&input, &ident)?;
                } else if ident == "generate_all" {
                    allowlist.set_all(&ident)?;
                    swallow_parentheses(&input, &ident)?;
                } else if ident == "name" {
                    let args;
                    syn::parenthesized!(args in input);
                    let ident: syn::Ident = args.parse()?;
                    mod_name = Some(ident);
                } else if ident == "exclude_utilities" {
                    exclude_utilities = true;
                    swallow_parentheses(&input, &ident)?;
                } else if ident == "safety" {
                    let args;
                    syn::parenthesized!(args in input);
                    unsafe_policy = args.parse()?;
                } else if ident == "extern_rust_fun" {
                    let args;
                    syn::parenthesized!(args in input);
                    let path: RustPath = args.parse()?;
                    args.parse::<syn::token::Comma>()?;
                    let sig: syn::Signature = args.parse()?;
                    extern_rust_funs.push(RustFun { path, sig });
                } else {
                    return Err(syn::Error::new(
                        ident.span(),
                        "expected generate, generate_pod, nested_type, safety or exclude_utilities",
                    ));
                }
            }
            if input.is_empty() {
                break;
            }
        }

        Ok(IncludeCppConfig {
            inclusions,
            unsafe_policy,
            parse_only,
            exclude_impls,
            pod_requests,
            rust_types,
            allowlist,
            blocklist,
            constructor_blocklist,
            exclude_utilities,
            mod_name,
            subclasses,
            extern_rust_funs,
            concretes,
        })
    }
}

fn swallow_parentheses(input: &ParseStream, latest_ident: &Ident) -> ParseResult<()> {
    let args;
    syn::parenthesized!(args in input);
    if args.is_empty() {
        Ok(())
    } else {
        Err(syn::Error::new(
            latest_ident.span(),
            "expected no arguments to directive",
        ))
    }
}

impl IncludeCppConfig {
    pub fn get_pod_requests(&self) -> &[String] {
        &self.pod_requests
    }

    pub fn get_mod_name(&self) -> Ident {
        self.mod_name
            .as_ref()
            .cloned()
            .unwrap_or_else(|| Ident::new("ffi", Span::call_site()))
    }

    /// Whether to avoid generating the standard helpful utility
    /// functions which we normally include in every mod.
    pub fn exclude_utilities(&self) -> bool {
        self.exclude_utilities
    }

    /// Items which the user has explicitly asked us to generate;
    /// we should raise an error if we weren't able to do so.
    pub fn must_generate_list(&self) -> Box<dyn Iterator<Item = String> + '_> {
        if let Allowlist::Specific(items) = &self.allowlist {
            Box::new(
                items
                    .iter()
                    .filter_map(|i| match i {
                        AllowlistEntry::Item(i) => Some(i),
                        AllowlistEntry::Namespace(_) => None,
                    })
                    .chain(self.pod_requests.iter())
                    .cloned(),
            )
        } else {
            Box::new(self.pod_requests.iter().cloned())
        }
    }

    /// The allowlist of items to be passed into bindgen, if any.
    pub fn bindgen_allowlist(&self) -> Option<Box<dyn Iterator<Item = String> + '_>> {
        match &self.allowlist {
            Allowlist::All => None,
            Allowlist::Specific(items) => Some(Box::new(
                items
                    .iter()
                    .map(AllowlistEntry::to_bindgen_item)
                    .chain(self.pod_requests.iter().cloned())
                    .chain(self.active_utilities())
                    .chain(self.subclasses.iter().flat_map(|sc| {
                        [
                            format!("{}Cpp", sc.subclass),
                            sc.subclass.to_string(), // TODO may not be necessary
                            sc.superclass.clone(),
                        ]
                    })),
            )),
            Allowlist::Unspecified(_) => unreachable!(),
        }
    }

    fn active_utilities(&self) -> Vec<String> {
        if self.exclude_utilities {
            Vec::new()
        } else {
            vec![self.get_makestring_name().to_string()]
        }
    }

    fn is_subclass_or_superclass(&self, cpp_name: &str) -> bool {
        self.subclasses
            .iter()
            .flat_map(|sc| {
                [
                    Cow::Owned(sc.subclass.to_string()),
                    Cow::Borrowed(&sc.superclass),
                ]
            })
            .any(|item| cpp_name == item.as_str())
    }

    /// Whether this type is on the allowlist specified by the user.
    ///
    /// A note on the allowlist handling in general. It's used in two places:
    /// 1) As directives to bindgen
    /// 2) After bindgen has generated code, to filter the APIs which
    ///    we pass to cxx.
    /// This second pass may seem redundant. But sometimes bindgen generates
    /// unnecessary stuff.
    pub fn is_on_allowlist(&self, cpp_name: &str) -> bool {
        self.active_utilities().iter().any(|item| *item == cpp_name)
            || self.is_subclass_or_superclass(cpp_name)
            || self.is_subclass_holder(cpp_name)
            || self.is_subclass_cpp(cpp_name)
            || self.is_rust_fun(cpp_name)
            || self.is_concrete_type(cpp_name)
            || match &self.allowlist {
                Allowlist::Unspecified(_) => panic!("Eek no allowlist yet"),
                Allowlist::All => true,
                Allowlist::Specific(items) => items.iter().any(|entry| match entry {
                    AllowlistEntry::Item(i) => i == cpp_name,
                    AllowlistEntry::Namespace(ns) => cpp_name.starts_with(ns),
                }),
            }
    }

    pub fn is_on_blocklist(&self, cpp_name: &str) -> bool {
        self.blocklist.contains(&cpp_name.to_string())
    }

    pub fn is_on_constructor_blocklist(&self, cpp_name: &str) -> bool {
        self.constructor_blocklist.contains(&cpp_name.to_string())
    }

    pub fn get_blocklist(&self) -> impl Iterator<Item = &String> {
        self.blocklist.iter()
    }

    fn is_concrete_type(&self, cpp_name: &str) -> bool {
        self.concretes.values().any(|val| *val == cpp_name)
    }

    /// In case there are multiple sets of ffi mods in a single binary,
    /// endeavor to return a name which can be used to make symbols
    /// unique.
    pub fn uniquify_name_per_mod<'a>(&self, name: &'a str) -> Cow<'a, str> {
        match self.mod_name.as_ref() {
            None => Cow::Borrowed(name),
            Some(md) => Cow::Owned(format!("{}_{}", name, md)),
        }
    }

    pub fn get_makestring_name(&self) -> Cow<str> {
        self.uniquify_name_per_mod("autocxx_make_string")
    }

    pub fn is_rust_type(&self, id: &Ident) -> bool {
        self.rust_types
            .iter()
            .any(|rt| rt.get_final_ident() == &id.to_string())
            || self.is_subclass_holder(&id.to_string())
    }

    fn is_rust_fun(&self, possible_fun: &str) -> bool {
        self.extern_rust_funs
            .iter()
            .map(|fun| &fun.sig.ident)
            .any(|id| id == possible_fun)
    }

    pub fn superclasses(&self) -> impl Iterator<Item = &String> {
        let mut uniquified = HashSet::new();
        uniquified.extend(self.subclasses.iter().map(|sc| &sc.superclass));
        uniquified.into_iter()
    }

    pub fn is_subclass_holder(&self, id: &str) -> bool {
        self.subclasses
            .iter()
            .any(|sc| format!("{}Holder", sc.subclass) == id)
    }

    fn is_subclass_cpp(&self, id: &str) -> bool {
        self.subclasses
            .iter()
            .any(|sc| format!("{}Cpp", sc.subclass) == id)
    }

    /// Return the filename to which generated .rs should be written.
    pub fn get_rs_filename(&self) -> String {
        format!(
            "autocxx-{}-gen.rs",
            self.mod_name
                .as_ref()
                .map(|id| id.to_string())
                .unwrap_or_else(|| "ffi-default".into())
        )
    }

    pub fn confirm_complete(&mut self, auto_allowlist: bool) -> ParseResult<()> {
        if matches!(self.allowlist, Allowlist::Unspecified(_)) {
            if auto_allowlist {
                self.allowlist = Allowlist::Specific(Vec::new());
                Ok(())
            } else {
                Err(syn::Error::new(
                    Span::call_site(),
                    "expected either generate!/generate_ns! or generate_all!",
                ))
            }
        } else {
            Ok(())
        }
    }

    /// Used in reduction to substitute all included headers with a single
    /// preprocessed replacement.
    pub fn replace_included_headers(&mut self, replacement: &str) {
        self.inclusions.clear();
        self.inclusions.push(replacement.to_string());
    }
}

#[cfg(feature = "reproduction_case")]
impl ToTokens for IncludeCppConfig {
    fn to_tokens(&self, tokens: &mut proc_macro2::TokenStream) {
        for inc in &self.inclusions {
            let hexathorpe = syn::token::Pound(Span::call_site());
            tokens.extend(quote! {
                #hexathorpe include #inc
            })
        }
        let unsafety = &self.unsafe_policy;
        tokens.extend(quote! {
            safety!(#unsafety)
        });
        if self.exclude_impls {
            tokens.extend(quote! { exclude_impls!() });
        }
        if self.parse_only {
            tokens.extend(quote! { parse_only!() });
        }
        if self.exclude_utilities {
            tokens.extend(quote! { exclude_utilities!() });
        }
        for i in &self.pod_requests {
            tokens.extend(quote! { pod!(#i) });
        }
        for i in &self.blocklist {
            tokens.extend(quote! { block!(#i) });
        }
        for i in &self.constructor_blocklist {
            tokens.extend(quote! { block_constructors!(#i) });
        }
        for path in &self.rust_types {
            tokens.extend(quote! { rust_type!(#path) });
        }
        match &self.allowlist {
            Allowlist::All => tokens.extend(quote! { generate_all!() }),
            Allowlist::Specific(items) => {
                for i in items {
                    match i {
                        AllowlistEntry::Item(i) => tokens.extend(quote! { generate!(#i) }),
                        AllowlistEntry::Namespace(ns) => {
                            tokens.extend(quote! { generate_ns!(#ns) })
                        }
                    }
                }
            }
            Allowlist::Unspecified(_) => panic!("Allowlist mode not yet determined"),
        }
        if let Some(mod_name) = &self.mod_name {
            tokens.extend(quote! { mod_name!(#mod_name) });
        }
        for i in &self.extern_rust_funs {
            let p = &i.path;
            let s = &i.sig;
            tokens.extend(quote! { extern_rust_fun!(#p,#s) });
        }
        for i in &self.subclasses {
            let superclass = &i.superclass;
            let subclass = &i.subclass;
            tokens.extend(quote! { subclass!(#superclass,#subclass) });
        }
    }
}

#[cfg(test)]
mod parse_tests {
    use crate::config::UnsafePolicy;
    use syn::parse_quote;
    #[test]
    fn test_safety_unsafe() {
        let us: UnsafePolicy = parse_quote! {
            unsafe
        };
        assert_eq!(us, UnsafePolicy::AllFunctionsSafe)
    }

    #[test]
    fn test_safety_unsafe_ffi() {
        let us: UnsafePolicy = parse_quote! {
            unsafe_ffi
        };
        assert_eq!(us, UnsafePolicy::AllFunctionsSafe)
    }

    #[test]
    fn test_safety_safe() {
        let us: UnsafePolicy = parse_quote! {};
        assert_eq!(us, UnsafePolicy::AllFunctionsUnsafe)
    }
}
