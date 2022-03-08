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

use autocxx_parser::IncludeCppConfig;
use syn::{parse_quote, Ident, Item};

pub(crate) fn create_impl_items(id: &Ident, movable: bool, config: &IncludeCppConfig) -> Vec<Item> {
    if config.exclude_impls {
        return vec![];
    }
    let mut results = vec![
        Item::Impl(parse_quote! {
            impl UniquePtr<#id> {}
        }),
        Item::Impl(parse_quote! {
            impl SharedPtr<#id> {}
        }),
        Item::Impl(parse_quote! {
            impl WeakPtr<#id> {}
        }),
    ];
    if movable {
        results.push(Item::Impl(parse_quote! {
            impl CxxVector<#id> {}
        }))
    }
    results
}
