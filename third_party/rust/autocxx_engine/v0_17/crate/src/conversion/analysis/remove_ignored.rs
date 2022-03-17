// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::collections::HashSet;

use super::deps::HasDependencies;
use super::fun::{FnAnalysis, FnKind, FnPhase};
use crate::conversion::apivec::ApiVec;
use crate::conversion::{convert_error::ErrorContext, ConvertError};
use crate::{conversion::api::Api, known_types};

/// Remove any APIs which depend on other items which have been ignored.
/// We also eliminate any APIs that depend on some type that we just don't
/// know about at all. In either case, we don't simply remove the type, but instead
/// replace it with an error marker.
pub(crate) fn filter_apis_by_ignored_dependents(mut apis: ApiVec<FnPhase>) -> ApiVec<FnPhase> {
    let (ignored_items, valid_items): (Vec<&Api<_>>, Vec<&Api<_>>) = apis
        .iter()
        .partition(|api| matches!(api, Api::IgnoredItem { .. }));
    let mut ignored_items: HashSet<_> = ignored_items
        .into_iter()
        .map(|api| api.name().clone())
        .collect();
    let valid_types: HashSet<_> = valid_items
        .into_iter()
        .flat_map(|api| api.valid_types())
        .collect();
    let mut iterate_again = true;
    while iterate_again {
        iterate_again = false;
        apis = apis
            .into_iter()
            .map(|api| {
                let ignored_dependents: HashSet<_> = api
                    .deps()
                    .filter(|dep| ignored_items.contains(dep))
                    .cloned()
                    .collect();
                if !ignored_dependents.is_empty() {
                    iterate_again = true;
                    ignored_items.insert(api.name().clone());
                    create_ignore_item(api, ConvertError::IgnoredDependent(ignored_dependents))
                } else {
                    let mut missing_deps = api.deps().filter(|dep| {
                        !valid_types.contains(dep) && !known_types().is_known_type(dep)
                    });
                    let first = missing_deps.next();
                    std::mem::drop(missing_deps);
                    if let Some(missing_dep) = first.cloned() {
                        create_ignore_item(api, ConvertError::UnknownDependentType(missing_dep))
                    } else {
                        api
                    }
                }
            })
            .collect();
    }
    apis
}

fn create_ignore_item(api: Api<FnPhase>, err: ConvertError) -> Api<FnPhase> {
    let id = api.name().get_final_ident();
    log::info!("Marking as ignored: {} because {}", id.to_string(), err);
    Api::IgnoredItem {
        name: api.name_info().clone(),
        err,
        ctx: match api {
            Api::Function {
                analysis:
                    FnAnalysis {
                        kind: FnKind::TraitMethod { .. },
                        ..
                    },
                ..
            } => None,
            Api::Function {
                analysis:
                    FnAnalysis {
                        kind:
                            FnKind::Method {
                                impl_for: self_ty, ..
                            },
                        ..
                    },
                ..
            } => Some(ErrorContext::new_for_method(self_ty.get_final_ident(), id)),
            _ => Some(ErrorContext::new_for_item(id)),
        },
    }
}
