// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::{
    fun::{
        FnAnalysis, FnKind, FnPhase, FnPrePhase2, MethodKind, PodAndConstructorAnalysis,
        TraitMethodKind,
    },
    pod::PodAnalysis,
};
use crate::conversion::{api::Api, apivec::ApiVec};
use crate::conversion::{
    api::TypeKind,
    error_reporter::{convert_apis, convert_item_apis},
    ConvertError,
};
use indexmap::set::IndexSet as HashSet;

/// Spot types with pure virtual functions and mark them abstract.
pub(crate) fn mark_types_abstract(mut apis: ApiVec<FnPrePhase2>) -> ApiVec<FnPrePhase2> {
    let mut abstract_types: HashSet<_> = apis
        .iter()
        .filter_map(|api| match &api {
            Api::Function {
                analysis:
                    FnAnalysis {
                        kind:
                            FnKind::Method {
                                impl_for: self_ty_name,
                                method_kind: MethodKind::PureVirtual(_),
                                ..
                            },
                        ..
                    },
                ..
            } => Some(self_ty_name.clone()),
            _ => None,
        })
        .collect();

    // Spot any derived classes (recursively). Also, any types which have a base
    // class that's not on the allowlist are presumed to be abstract, because we
    // have no way of knowing (as they're not on the allowlist, there will be
    // no methods associated so we won't be able to spot pure virtual methods).
    let mut iterate = true;
    while iterate {
        iterate = false;
        apis = apis
            .into_iter()
            .map(|api| {
                match api {
                    Api::Struct {
                        analysis:
                            PodAndConstructorAnalysis {
                                pod:
                                    PodAnalysis {
                                        bases,
                                        kind: TypeKind::Pod | TypeKind::NonPod,
                                        castable_bases,
                                        field_deps,
                                        field_info,
                                        is_generic,
                                    },
                                constructors,
                            },
                        name,
                        details,
                    } if abstract_types.contains(&name.name)
                        || !abstract_types.is_disjoint(&bases) =>
                    {
                        abstract_types.insert(name.name.clone());
                        // Recurse in case there are further dependent types
                        iterate = true;
                        Api::Struct {
                            analysis: PodAndConstructorAnalysis {
                                pod: PodAnalysis {
                                    bases,
                                    kind: TypeKind::Abstract,
                                    castable_bases,
                                    field_deps,
                                    field_info,
                                    is_generic,
                                },
                                constructors,
                            },
                            name,
                            details,
                        }
                    }
                    _ => api,
                }
            })
            .collect()
    }

    // We also need to remove any constructors belonging to these
    // abstract types.
    apis.retain(|api| {
        !matches!(&api,
        Api::Function {
            analysis:
                FnAnalysis {
                    kind: FnKind::Method{impl_for: self_ty, method_kind: MethodKind::Constructor{..}, ..}
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
    let mut results = ApiVec::new();
    convert_item_apis(apis, &mut results, |api| match api {
        Api::Struct {
            analysis:
                PodAndConstructorAnalysis {
                    pod:
                        PodAnalysis {
                            kind: TypeKind::Abstract,
                            ..
                        },
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

pub(crate) fn discard_ignored_functions(apis: ApiVec<FnPhase>) -> ApiVec<FnPhase> {
    // Some APIs can't be generated, e.g. because they're protected.
    // Now we've finished analyzing abstract types and constructors, we'll
    // convert them to IgnoredItems.
    let mut apis_new = ApiVec::new();
    convert_apis(
        apis,
        &mut apis_new,
        |name, fun, analysis| {
            analysis.ignore_reason.clone()?;
            Ok(Box::new(std::iter::once(Api::Function {
                name,
                fun,
                analysis,
            })))
        },
        Api::struct_unchanged,
        Api::enum_unchanged,
        Api::typedef_unchanged,
    );
    apis_new
}
