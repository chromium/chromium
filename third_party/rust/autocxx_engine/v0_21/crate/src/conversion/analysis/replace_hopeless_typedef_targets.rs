// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::collections::HashSet;

use crate::{
    conversion::{
        analysis::tdef::TypedefAnalysis, api::Api, apivec::ApiVec,
        convert_error::ConvertErrorWithContext,
    },
    types::QualifiedName,
};

use super::pod::PodPhase;
/// Where we find a typedef pointing at something we can't represent,
/// e.g. because it uses too many template parameters, break the link.
/// Use the typedef as a first-class type.
pub(crate) fn replace_hopeless_typedef_targets(apis: ApiVec<PodPhase>) -> ApiVec<PodPhase> {
    let ignored_types: HashSet<QualifiedName> = apis
        .iter()
        .filter_map(|api| match api {
            Api::IgnoredItem { .. } => Some(api.name()),
            _ => None,
        })
        .cloned()
        .collect();
    let ignored_forward_declarations: HashSet<QualifiedName> = apis
        .iter()
        .filter_map(|api| match api {
            Api::ForwardDeclaration { err: Some(_), .. } => Some(api.name()),
            _ => None,
        })
        .cloned()
        .collect();
    // Convert any Typedefs which depend on these things into OpaqueTypedefs
    // instead.
    // And, after this point we no longer need special knowledge of forward
    // declarations with errors, so just convert them into regular IgnoredItems too.
    apis.into_iter()
        .map(|api| match api {
            Api::Typedef {
                analysis: TypedefAnalysis { ref deps, .. },
                ..
            } if !ignored_types.is_disjoint(deps) => Api::OpaqueTypedef {
                name: api.name_info().clone(),
                forward_declaration: false,
            },
            Api::Typedef {
                analysis: TypedefAnalysis { ref deps, .. },
                ..
            } if !ignored_forward_declarations.is_disjoint(deps) => Api::OpaqueTypedef {
                name: api.name_info().clone(),
                forward_declaration: true,
            },
            Api::ForwardDeclaration {
                name,
                err: Some(ConvertErrorWithContext(err, ctx)),
            } => Api::IgnoredItem { name, err, ctx },
            _ => api,
        })
        .collect()
}
