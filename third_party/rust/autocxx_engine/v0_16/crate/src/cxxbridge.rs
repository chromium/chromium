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
        let fp = do_cxx_cpp_generation(self.tokens.clone(), cpp_codegen_options)?;
        Ok(GeneratedCpp(vec![fp]))
    }
}
