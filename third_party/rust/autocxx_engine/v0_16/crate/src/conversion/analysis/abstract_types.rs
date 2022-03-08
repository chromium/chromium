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

use super::{
    fun::{FnAnalysis, FnKind, FnPhase, MethodKind, TraitMethodKind},
    pod::PodAnalysis,
};
use crate::conversion::{
    api::TypeKind,
    error_reporter::{convert_apis, convert_item_apis},
    ConvertError,
};
use crate::{conversion::api::Api, types::QualifiedName};
use std::collections::HashSet;

/// Spot types with pure virtual functions and mark them abstract.
pub(crate) fn mark_types_abstract(
    config: &IncludeCppConfig,
    mut apis: Vec<Api<FnPhase>>,
) -> Vec<Api<FnPhase>> {
    let mut abstract_types: HashSet<_> = apis
        .iter()
        .filter_map(|api| match &api {
            Api::Function {
                analysis:
                    FnAnalysis {
                        kind: FnKind::Method(self_ty_name, MethodKind::PureVirtual(_)),
                        ..
                    },
                ..
            } => Some(self_ty_name.clone()),
            _ => None,
        })
        .collect();

    for mut api in apis.iter_mut() {
        match &mut api {
            Api::Struct { analysis, name, .. } if abstract_types.contains(&name.name) => {
                analysis.kind = TypeKind::Abstract;
            }
            _ => {}
        }
    }

    // Spot any derived classes (recursively). Also, any types which have a base
    // class that's not on the allowlist are presumed to be abstract, because we
    // have no way of knowing (as they're not on the allowlist, there will be
    // no methods associated so we won't be able to spot pure virtual methods).
    let mut iterate = true;
    while iterate {
        iterate = false;
        for mut api in apis.iter_mut() {
            match &mut api {
                Api::Struct {
                    analysis: PodAnalysis { bases, kind, .. },
                    ..
                } if *kind != TypeKind::Abstract
                    && (!abstract_types.is_disjoint(bases)
                        || any_missing_from_allowlist(config, bases)) =>
                {
                    *kind = TypeKind::Abstract;
                    abstract_types.insert(api.name().clone());
                    // Recurse in case there are further dependent types
                    iterate = true;
                }
                _ => {}
            }
        }
    }

    // We also need to remove any constructors belonging to these
    // abstract types.
    apis.retain(|api| {
        !matches!(&api,
        Api::Function {
            analysis:
                FnAnalysis {
                    kind: FnKind::Method(self_ty, MethodKind::MakeUnique | MethodKind::Constructor)
                        | FnKind::TraitMethod{ kind: TraitMethodKind::CopyConstructor | TraitMethodKind::MoveConstructor, impl_for: self_ty, ..},
                    ..
                },
                ..
        } if abstract_types.contains(self_ty))
    });

    // Finally, if there are any types which are nested inside other types,
    // they can't be abstract. This is due to two small limitations in cxx.
    // Imagine we have class Foo { class Bar }
    // 1) using "type Foo = super::bindgen::root::Foo_Bar" results
    //    in the creation of std::unique_ptr code which isn't acceptable
    //    for an abtract class
    // 2) using "type Foo;" isn't possible unless Foo is a top-level item
    //    within its namespace. Any outer names will be interpreted as namespace
    //    names and result in cxx generating "namespace Foo { class Bar }"".
    let mut results = Vec::new();
    convert_item_apis(apis, &mut results, |api| match api {
        Api::Struct {
            analysis:
                PodAnalysis {
                    kind: TypeKind::Abstract,
                    ..
                },
            ..
        } if api
            .cpp_name()
            .as_ref()
            .map(|n| n.contains("::"))
            .unwrap_or_default() =>
        {
            Err(ConvertError::AbstractNestedType)
        }
        _ => Ok(Box::new(std::iter::once(api))),
    });
    results
}

fn any_missing_from_allowlist(config: &IncludeCppConfig, bases: &HashSet<QualifiedName>) -> bool {
    bases
        .iter()
        .any(|qn| !config.is_on_allowlist(&qn.to_cpp_name()))
}

pub(crate) fn discard_ignored_functions(apis: Vec<Api<FnPhase>>) -> Vec<Api<FnPhase>> {
    // Some APIs can't be generated, e.g. because they're protected.
    // Now we've finished analyzing abstract types and constructors, we'll
    // convert them to IgnoredI
    let mut apis_new = Vec::new();
    convert_apis(
        apis,
        &mut apis_new,
        |name, fun, analysis, name_for_gc| {
            analysis.ignore_reason.clone()?;
            Ok(Box::new(std::iter::once(Api::Function {
                name,
                fun,
                analysis,
                name_for_gc,
            })))
        },
        Api::struct_unchanged,
        Api::enum_unchanged,
        Api::typedef_unchanged,
    );
    apis_new
}
