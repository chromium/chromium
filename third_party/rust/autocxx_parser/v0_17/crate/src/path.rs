// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::ParseResult;
use proc_macro2::Ident;
use quote::{quote, ToTokens, TokenStreamExt};
use syn::parse::{Parse, ParseStream};

/// A little like [`syn::Path`] but simpler - contains only identifiers,
/// no path arguments. Guaranteed to always have at least one identifier.
#[derive(Debug, Clone)]
pub struct RustPath(Vec<Ident>);

impl RustPath {
    pub fn new_from_ident(id: Ident) -> Self {
        Self(vec![id])
    }

    #[must_use]
    pub fn append(&self, id: Ident) -> Self {
        Self(self.0.iter().cloned().chain(std::iter::once(id)).collect())
    }

    pub fn get_final_ident(&self) -> &Ident {
        self.0.last().unwrap()
    }
}

impl ToTokens for RustPath {
    fn to_tokens(&self, tokens: &mut proc_macro2::TokenStream) {
        let mut it = self.0.iter();
        let mut id = it.next();
        while id.is_some() {
            id.unwrap().to_tokens(tokens);
            let next = it.next();
            if next.is_some() {
                tokens.append_all(quote! { :: });
            }
            id = next;
        }
    }
}

impl Parse for RustPath {
    fn parse(input: ParseStream) -> ParseResult<Self> {
        let id: Ident = input.parse()?;
        let mut p = RustPath::new_from_ident(id);
        while input.parse::<Option<syn::token::Colon2>>()?.is_some() {
            let id: Ident = input.parse()?;
            p = p.append(id);
        }
        Ok(p)
    }
}
