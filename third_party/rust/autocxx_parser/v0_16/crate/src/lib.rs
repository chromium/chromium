// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

mod config;
pub mod file_locations;
mod path;
mod subclass_attrs;

pub use config::{IncludeCppConfig, RustFun, Subclass, UnsafePolicy};
use file_locations::FileLocationStrategy;
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
pub mod directives {
    pub static EXTERN_RUST_TYPE: &str = "extern_rust_type";
    pub static EXTERN_RUST_FUN: &str = "extern_rust_fun";
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
        FileLocationStrategy::new().make_include(&self.config.get_rs_filename())
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
