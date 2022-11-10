// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use proc_macro2::TokenStream;
use quote::{ToTokens, TokenStreamExt};
use syn::ItemMod;

use crate::{do_cxx_cpp_generation, parse_file::CppBuildable, CppCodegenOptions, GeneratedCpp};

/// A struct to represent a cxx::bridge (i.e. some manual bindings)
/// found in a file. autocxx knows about them so that we can generate C++
/// for both manual and automatic bindings using the same tooling.
pub struct CxxBridge {
    tokens: TokenStream,
}

impl From<ItemMod> for CxxBridge {
    fn from(itm: ItemMod) -> Self {
        Self {
            tokens: itm.to_token_stream(),
        }
    }
}

impl ToTokens for CxxBridge {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        tokens.append_all(self.tokens.clone());
    }
}

impl CppBuildable for CxxBridge {
    fn generate_h_and_cxx(
        &self,
        cpp_codegen_options: &CppCodegenOptions,
    ) -> Result<GeneratedCpp, cxx_gen::Error> {
        let header_name = cpp_codegen_options.cxxgen_header_namer.name_header();
        let fp = do_cxx_cpp_generation(self.tokens.clone(), cpp_codegen_options, header_name)?;
        Ok(GeneratedCpp(vec![fp]))
    }
}
