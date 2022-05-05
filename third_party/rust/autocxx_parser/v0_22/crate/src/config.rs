// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use indexmap::map::IndexMap as HashMap;
use indexmap::set::IndexSet as HashSet;
use std::borrow::Cow;
use std::collections::hash_map::DefaultHasher;
use std::hash::{Hash, Hasher};

use itertools::Itertools;
use proc_macro2::Span;
use quote::ToTokens;

#[cfg(feature = "reproduction_case")]
use quote::format_ident;
use syn::{
    parse::{Parse, ParseStream},
    Signature, Token, TypePath,
};
use syn::{Ident, Result as ParseResult};
use thiserror::Error;

use crate::{directives::get_directives, RustPath};

use quote::quote;

#[derive(PartialEq, Clone, Debug, Hash)]
pub enum UnsafePolicy {
    AllFunctionsSafe,
    AllFunctionsUnsafe,
}

impl Default for UnsafePolicy {
    fn default() -> Self {
        Self::AllFunctionsUnsafe
    }
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

/// Errors that may be encountered while adding allowlist entries.
#[derive(Error, Debug)]
pub enum AllowlistErr {
    #[error("Conflict between generate/generate_ns! and generate_all! - use one not both")]
    ConflictingGenerateAndGenerateAll,
}

impl Allowlist {
    pub fn push(&mut self, item: AllowlistEntry) -> Result<(), AllowlistErr> {
        match self {
            Allowlist::Unspecified(ref mut uncommitted_list) => {
                let new_list = uncommitted_list
                    .drain(..)
                    .chain(std::iter::once(item))
                    .collect();
                *self = Allowlist::Specific(new_list);
            }
            Allowlist::All => {
                return Err(AllowlistErr::ConflictingGenerateAndGenerateAll);
            }
            Allowlist::Specific(list) => list.push(item),
        };
        Ok(())
    }

    pub(crate) fn set_all(&mut self) -> Result<(), AllowlistErr> {
        if matches!(self, Allowlist::Specific(..)) {
            return Err(AllowlistErr::ConflictingGenerateAndGenerateAll);
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

#[derive(Debug, Hash)]
pub struct Subclass {
    pub superclass: String,
    pub subclass: Ident,
}

#[derive(Clone, Hash)]
pub struct RustFun {
    pub path: RustPath,
    pub sig: Signature,
    pub receiver: Option<Ident>,
}

impl std::fmt::Debug for RustFun {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("RustFun")
            .field("path", &self.path)
            .field("sig", &self.sig.to_token_stream().to_string())
            .finish()
    }
}

#[derive(Debug, Clone, Hash)]
pub struct ExternCppType {
    pub rust_path: TypePath,
    pub opaque: bool,
}

/// Newtype wrapper so we can implement Hash.
#[derive(Debug, Default)]
pub struct ExternCppTypeMap(pub HashMap<String, ExternCppType>);

impl std::hash::Hash for ExternCppTypeMap {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        for (k, v) in &self.0 {
            k.hash(state);
            v.hash(state);
        }
    }
}

/// Newtype wrapper so we can implement Hash.
#[derive(Debug, Default)]
pub struct ConcretesMap(pub HashMap<String, Ident>);

impl std::hash::Hash for ConcretesMap {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        for (k, v) in &self.0 {
            k.hash(state);
            v.hash(state);
        }
    }
}

#[derive(Debug, Default, Hash)]
pub struct IncludeCppConfig {
    pub inclusions: Vec<String>,
    pub unsafe_policy: UnsafePolicy,
    pub parse_only: bool,
    pub exclude_impls: bool,
    pub(crate) pod_requests: Vec<String>,
    pub allowlist: Allowlist,
    pub(crate) blocklist: Vec<String>,
    pub(crate) constructor_blocklist: Vec<String>,
    pub instantiable: Vec<String>,
    pub(crate) exclude_utilities: bool,
    pub(crate) mod_name: Option<Ident>,
    pub rust_types: Vec<RustPath>,
    pub subclasses: Vec<Subclass>,
    pub extern_rust_funs: Vec<RustFun>,
    pub concretes: ConcretesMap,
    pub externs: ExternCppTypeMap,
}

impl Parse for IncludeCppConfig {
    fn parse(input: ParseStream) -> ParseResult<Self> {
        let mut config = IncludeCppConfig::default();

        while !input.is_empty() {
            let has_hexathorpe = input.parse::<Option<syn::token::Pound>>()?.is_some();
            let ident: syn::Ident = input.parse()?;
            let args;
            let (possible_directives, to_parse, parse_completely) = if has_hexathorpe {
                (&get_directives().need_hexathorpe, input, false)
            } else {
                input.parse::<Option<syn::token::Bang>>()?;
                syn::parenthesized!(args in input);
                (&get_directives().need_exclamation, &args, true)
            };
            let all_possible = possible_directives.keys().join(", ");
            let ident_str = ident.to_string();
            match possible_directives.get(&ident_str) {
                None => {
                    return Err(syn::Error::new(
                        ident.span(),
                        format!("expected {}", all_possible),
                    ));
                }
                Some(directive) => directive.parse(to_parse, &mut config, &ident.span())?,
            }
            if parse_completely && !to_parse.is_empty() {
                return Err(syn::Error::new(
                    ident.span(),
                    format!("found unexpected input within the directive {}", ident_str),
                ));
            }
            if input.is_empty() {
                break;
            }
        }
        Ok(config)
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
            vec![self.get_makestring_name()]
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
        self.concretes.0.values().any(|val| *val == cpp_name)
    }

    /// Get a hash of the contents of this `include_cpp!` block.
    pub fn get_hash(&self) -> u64 {
        let mut s = DefaultHasher::new();
        self.hash(&mut s);
        s.finish()
    }

    /// In case there are multiple sets of ffi mods in a single binary,
    /// endeavor to return a name which can be used to make symbols
    /// unique.
    pub fn uniquify_name_per_mod(&self, name: &str) -> String {
        format!("{}_{:#x}", name, self.get_hash())
    }

    pub fn get_makestring_name(&self) -> String {
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

    pub fn confirm_complete(&mut self) {
        if matches!(self.allowlist, Allowlist::Unspecified(_)) {
            self.allowlist = Allowlist::Specific(Vec::new());
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
        let directives = get_directives();
        let hexathorpe = syn::token::Pound(Span::call_site());
        for (id, directive) in &directives.need_hexathorpe {
            let id = format_ident!("{}", id);
            for output in directive.output(self) {
                tokens.extend(quote! {
                    #hexathorpe #id #output
                })
            }
        }
        for (id, directive) in &directives.need_exclamation {
            let id = format_ident!("{}", id);
            for output in directive.output(self) {
                tokens.extend(quote! {
                    #id ! (#output)
                })
            }
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
