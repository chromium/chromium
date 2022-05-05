// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use indexmap::IndexMap;
use proc_macro2::TokenStream;
use serde::{Deserialize, Serialize};

use crate::IncludeCppConfig;

/// Struct which stores multiple sets of bindings and can be serialized
/// to disk. This is used when our build system uses `autocxx_gen`; that
/// can handle multiple `include_cpp!` macros and therefore generate multiple
/// sets of Rust bindings. We can't simply `include!` those because there's
/// no (easy) way to pass their details from the codegen phase across to
/// the Rust macro phase. Instead, we use this data structure to store
/// several sets of .rs bindings in a single file, and then the macro
/// extracts the correct set of bindings at expansion time.
#[derive(Serialize, Deserialize, Default)]
pub struct MultiBindings(IndexMap<u64, String>);

use thiserror::Error;

#[derive(Error, Debug)]
pub enum MultiBindingsErr {
    #[error("unable to find the desired bindings within the archive of Rust bindings produced by the autocxx code generation phase")]
    MissingBindings,
    #[error("the stored bindings within the JSON file could not be parsed as valid Rust tokens")]
    BindingsNotParseable,
}

impl MultiBindings {
    /// Insert some generated Rust bindings into this data structure.
    pub fn insert(&mut self, config: &IncludeCppConfig, bindings: TokenStream) {
        self.0.insert(config.get_hash(), bindings.to_string());
    }

    /// Retrieves the bindings corresponding to a given [`IncludeCppConfig`].
    pub fn get(&self, config: &IncludeCppConfig) -> Result<TokenStream, MultiBindingsErr> {
        match self.0.get(&(config.get_hash())) {
            None => Err(MultiBindingsErr::MissingBindings),
            Some(bindings) => Ok(bindings
                .parse()
                .map_err(|_| MultiBindingsErr::BindingsNotParseable)?),
        }
    }
}

#[cfg(test)]
mod tests {
    use proc_macro2::Span;
    use quote::quote;
    use syn::parse_quote;

    use crate::IncludeCppConfig;

    use super::MultiBindings;

    #[test]
    fn test_multi_bindings() {
        let hexathorpe = syn::token::Pound(Span::call_site());
        let config1: IncludeCppConfig = parse_quote! {
            #hexathorpe include "a.h"
            generate!("Foo")
        };
        let config2: IncludeCppConfig = parse_quote! {
            #hexathorpe include "b.h"
            generate!("Bar")
        };
        let config3: IncludeCppConfig = parse_quote! {
            #hexathorpe include "c.h"
            generate!("Bar")
        };
        let mut multi_bindings = MultiBindings::default();
        multi_bindings.insert(&config1, quote! { first; });
        multi_bindings.insert(&config2, quote! { second; });
        let json = serde_json::to_string(&multi_bindings).unwrap();
        let multi_bindings2: MultiBindings = serde_json::from_str(&json).unwrap();
        assert_eq!(
            multi_bindings2.get(&config2).unwrap().to_string(),
            "second ;"
        );
        assert_eq!(
            multi_bindings2.get(&config1).unwrap().to_string(),
            "first ;"
        );
        assert!(multi_bindings2.get(&config3).is_err());
    }
}
