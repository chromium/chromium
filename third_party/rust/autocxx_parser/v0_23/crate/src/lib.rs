// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![forbid(unsafe_code)]

mod config;
mod directives;
pub mod file_locations;
mod multi_bindings;
mod path;
mod subclass_attrs;

pub use config::{
    AllowlistEntry, ExternCppType, IncludeCppConfig, RustFun, Subclass, UnsafePolicy,
};
use file_locations::FileLocationStrategy;
pub use multi_bindings::{MultiBindings, MultiBindingsErr};
pub use path::RustPath;
use proc_macro2::TokenStream as TokenStream2;
pub use subclass_attrs::SubclassAttrs;
use syn::Result as ParseResult;
use syn::{
    parse::{Parse, ParseStream},
    Macro,
};

#[doc(hidden)]
/// Ensure consistency between the `include_cpp!` parser
/// and the standalone macro discoverer
pub mod directive_names {
    pub static EXTERN_RUST_TYPE: &str = "extern_rust_type";
    pub static EXTERN_RUST_FUN: &str = "extern_rust_function";
    pub static SUBCLASS: &str = "subclass";
}

/// Core of the autocxx engine. See `generate` for most details
/// on how this works.
pub struct IncludeCpp {
    config: IncludeCppConfig,
}

impl Parse for IncludeCpp {
    fn parse(input: ParseStream) -> ParseResult<Self> {
        let config = input.parse::<IncludeCppConfig>()?;
        Ok(Self { config })
    }
}

impl IncludeCpp {
    pub fn new_from_syn(mac: Macro) -> ParseResult<Self> {
        mac.parse_body::<IncludeCpp>()
    }

    /// Generate the Rust bindings.
    pub fn generate_rs(&self) -> TokenStream2 {
        if self.config.parse_only {
            return TokenStream2::new();
        }
        FileLocationStrategy::new().make_include(&self.config)
    }

    pub fn get_config(&self) -> &IncludeCppConfig {
        &self.config
    }
}

#[cfg(test)]
mod parse_tests {
    use crate::IncludeCpp;
    use syn::parse_quote;

    #[test]
    fn test_basic() {
        let _i: IncludeCpp = parse_quote! {
            generate_all!()
        };
    }
}
