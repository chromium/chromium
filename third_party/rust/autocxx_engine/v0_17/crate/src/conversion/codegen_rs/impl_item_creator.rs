// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use autocxx_parser::IncludeCppConfig;
use syn::{parse_quote, Ident, Item};

pub(crate) fn create_impl_items(
    id: &Ident,
    movable: bool,
    destroyable: bool,
    config: &IncludeCppConfig,
) -> Vec<Item> {
    if config.exclude_impls {
        return vec![];
    }
    let mut results = Vec::new();
    if destroyable {
        results.extend([
            Item::Impl(parse_quote! {
                impl UniquePtr<#id> {}
            }),
            Item::Impl(parse_quote! {
                impl SharedPtr<#id> {}
            }),
            Item::Impl(parse_quote! {
                impl WeakPtr<#id> {}
            }),
        ]);
    }
    if movable {
        results.push(Item::Impl(parse_quote! {
            impl CxxVector<#id> {}
        }))
    }
    results
}
