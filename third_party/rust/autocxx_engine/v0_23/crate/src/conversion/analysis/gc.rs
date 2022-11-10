// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use indexmap::map::IndexMap as HashMap;
use indexmap::set::IndexSet as HashSet;

use autocxx_parser::IncludeCppConfig;

use crate::{
    conversion::{api::Api, apivec::ApiVec},
    types::QualifiedName,
};

use super::{deps::HasDependencies, fun::FnPhase};

/// This is essentially mark-and-sweep garbage collection of the
/// [Api]s that we've discovered. Why do we do this, you might wonder?
/// It seems a bit strange given that we pass an explicit allowlist
/// to bindgen.
/// There are two circumstances under which we want to discard
/// some of the APIs we encounter parsing the bindgen.
/// 1) We simplify some struct to be non-POD. In this case, we'll
///    discard all the fields within it. Those fields can be, and
///    in fact often _are_, stuff which we have trouble converting
///    e.g. std::string or std::string::value_type or
///    my_derived_thing<std::basic_string::value_type> or some
///    other permutation. In such cases, we want to discard those
///    field types with prejudice.
/// 2) block! may be used to ban certain APIs. This often eliminates
///    some methods from a given struct/class. In which case, we
///    don't care about the other parameter types passed into those
///    APIs either.
pub(crate) fn filter_apis_by_following_edges_from_allowlist(
    apis: ApiVec<FnPhase>,
    config: &IncludeCppConfig,
) -> ApiVec<FnPhase> {
    let mut todos: Vec<QualifiedName> = apis
        .iter()
        .filter(|api| {
            let tnforal = api.name_for_allowlist();
            config.is_on_allowlist(&tnforal.to_cpp_name())
        })
        .map(Api::name)
        .cloned()
        .collect();
    let mut by_typename: HashMap<QualifiedName, ApiVec<FnPhase>> = HashMap::new();
    for api in apis.into_iter() {
        let tn = api.name().clone();
        by_typename.entry(tn).or_default().push(api);
    }
    let mut done = HashSet::new();
    let mut output = ApiVec::new();
    while !todos.is_empty() {
        let todo = todos.remove(0);
        if done.contains(&todo) {
            continue;
        }
        if let Some(mut these_apis) = by_typename.remove(&todo) {
            todos.extend(these_apis.iter().flat_map(|api| api.deps().cloned()));
            output.append(&mut these_apis);
        } // otherwise, probably an intrinsic e.g. uint32_t.
        done.insert(todo);
    }
    output
}
