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

use std::collections::HashSet;

use super::fun::{FnAnalysis, FnKind, FnPhase};
use crate::conversion::{convert_error::ErrorContext, ConvertError};
use crate::{conversion::api::Api, known_types};

/// Remove any APIs which depend on other items which have been ignored.
/// We also eliminate any APIs that depend on some type that we just don't
/// know about at all. In either case, we don't simply remove the type, but instead
/// replace it with an error marker.
pub(crate) fn filter_apis_by_ignored_dependents(mut apis: Vec<Api<FnPhase>>) -> Vec<Api<FnPhase>> {
    let (ignored_items, valid_items): (Vec<&Api<_>>, Vec<&Api<_>>) = apis.iter().partition(|api| {
        matches!(
            api,
            Api::IgnoredItem {
                ctx: ErrorContext::Item(..),
                ..
            }
        )
    });
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
                if api.deps().any(|dep| ignored_items.contains(&dep)) {
                    iterate_again = true;
                    ignored_items.insert(api.name().clone());
                    create_ignore_item(api, ConvertError::IgnoredDependent)
                } else {
                    let mut missing_deps = api.deps().filter(|dep| {
                        !valid_types.contains(dep) && !known_types().is_known_type(dep)
                    });
                    let first = missing_deps.next();
                    std::mem::drop(missing_deps);
                    if let Some(missing_dep) = first {
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
                        kind: FnKind::Method(self_ty, _),
                        ..
                    },
                ..
            } => ErrorContext::Method {
                self_ty: self_ty.get_final_ident(),
                method: id,
            },
            _ => ErrorContext::Item(id),
        },
    }
}
