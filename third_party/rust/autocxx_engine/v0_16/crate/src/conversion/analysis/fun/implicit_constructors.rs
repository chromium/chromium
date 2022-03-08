// Copyright 2022 Google LLC
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

use std::collections::{HashMap, HashSet};

use crate::{
    conversion::{
        analysis::{depth_first::depth_first, pod::PodAnalysis},
        api::{Api, CppVisibility, FuncToConvert, SpecialMemberKind},
    },
    types::QualifiedName,
};

use super::{
    implicit_constructor_rules::{
        determine_implicit_constructors, ExplicitItemsFound, ImplicitConstructorsNeeded,
    },
    FnAnalysis, FnKind, FnPhase, MethodKind, ReceiverMutability, TraitMethodKind,
};

#[derive(Hash, Eq, PartialEq)]
enum ExplicitKind {
    MoveConstructor,
    ConstCopyConstructor,
    NonConstCopyConstructor,
    OtherConstructor,
    Destructor,
    CopyAssignmentOperator,
    MoveAssignmentOperator,
    DeletedOrInaccessibleCopyConstructor,
    DeletedOrInaccessibleDestructor,
}

#[derive(Hash, Eq, PartialEq)]
struct ExplicitFound {
    ty: QualifiedName,
    kind: ExplicitKind,
}

/// If a type has explicit constructors, bindgen will generate corresponding
/// constructor functions, which we'll have already converted to make_unique methods.
/// For types with implicit constructors, we synthesize them here.
/// It is tempting to make this a separate analysis phase, to be run later than
/// the function analysis; but that would make the code much more complex as it
/// would need to output a `FnAnalysisBody`. By running it as part of this phase
/// we can simply generate the sort of thing bindgen generates, then ask
/// the existing code in this phase to figure out what to do with it.
pub(super) fn find_missing_constructors(
    apis: &[Api<FnPhase>],
) -> HashMap<QualifiedName, ImplicitConstructorsNeeded> {
    let explicits = find_explicit_items(apis);
    let mut implicit_constructors_needed = HashMap::new();
    for api in depth_first(apis) {
        if let Api::Struct {
            name,
            analysis: PodAnalysis {
                bases, field_types, ..
            },
            details,
            ..
        } = api
        {
            let name = &name.name;
            let find = |kind: ExplicitKind| -> bool {
                explicits.contains(&ExplicitFound {
                    ty: name.clone(),
                    kind,
                })
            };
            let any_bases_or_fields_lack_const_copy_constructors =
                bases.iter().chain(field_types.iter()).any(|qn| {
                    let has_explicit = explicits.contains(&ExplicitFound {
                        ty: qn.clone(),
                        kind: ExplicitKind::ConstCopyConstructor,
                    });
                    let has_implicit = implicit_constructors_needed
                        .get(qn)
                        .map(|imp: &ImplicitConstructorsNeeded| imp.copy_constructor_taking_const_t)
                        .unwrap_or_default();
                    !has_explicit && !has_implicit
                });
            let any_bases_or_fields_have_deleted_or_inaccessible_copy_constructors =
                bases.iter().chain(field_types.iter()).any(|qn| {
                    explicits.contains(&ExplicitFound {
                        ty: qn.clone(),
                        kind: ExplicitKind::DeletedOrInaccessibleCopyConstructor,
                    })
                });
            let any_bases_have_deleted_or_inaccessible_destructors = bases.iter().any(|qn| {
                explicits.contains(&ExplicitFound {
                    ty: qn.clone(),
                    kind: ExplicitKind::DeletedOrInaccessibleDestructor,
                })
            });
            let explicit_items_found = ExplicitItemsFound {
                move_constructor: find(ExplicitKind::MoveConstructor),
                copy_constructor: find(ExplicitKind::ConstCopyConstructor)
                    || find(ExplicitKind::NonConstCopyConstructor)
                    || find(ExplicitKind::DeletedOrInaccessibleCopyConstructor),
                any_other_constructor: find(ExplicitKind::OtherConstructor),
                any_bases_or_fields_lack_const_copy_constructors,
                any_bases_or_fields_have_deleted_or_inaccessible_copy_constructors,
                any_bases_have_deleted_or_inaccessible_destructors,
                destructor: find(ExplicitKind::Destructor)
                    || find(ExplicitKind::DeletedOrInaccessibleDestructor),
                copy_assignment_operator: find(ExplicitKind::CopyAssignmentOperator),
                move_assignment_operator: find(ExplicitKind::MoveAssignmentOperator),
                has_rvalue_reference_fields: details.has_rvalue_reference_fields,
            };
            let implicits = determine_implicit_constructors(explicit_items_found);
            implicit_constructors_needed.insert(name.clone(), implicits);
        }
    }
    implicit_constructors_needed
}

fn find_explicit_items(apis: &[Api<FnPhase>]) -> HashSet<ExplicitFound> {
    apis.iter()
        .filter_map(|api| match api {
            Api::Function {
                analysis:
                    FnAnalysis {
                        kind: FnKind::Method(self_ty, MethodKind::Constructor),
                        ..
                    },
                ..
            } => Some(ExplicitFound {
                ty: self_ty.clone(),
                kind: ExplicitKind::OtherConstructor,
            }),
            Api::Function {
                analysis:
                    FnAnalysis {
                        kind:
                            FnKind::TraitMethod {
                                kind: TraitMethodKind::MoveConstructor,
                                impl_for,
                                ..
                            },
                        ..
                    },
                ..
            } => Some(ExplicitFound {
                ty: impl_for.clone(),
                kind: ExplicitKind::MoveConstructor,
            }),
            Api::Function {
                analysis:
                    FnAnalysis {
                        kind:
                            FnKind::TraitMethod {
                                kind: TraitMethodKind::Destructor,
                                impl_for,
                                ..
                            },
                        ..
                    },
                fun,
                ..
            } if is_deleted_or_inaccessible(fun) => Some(ExplicitFound {
                ty: impl_for.clone(),
                kind: ExplicitKind::DeletedOrInaccessibleDestructor,
            }),
            Api::Function {
                analysis:
                    FnAnalysis {
                        kind:
                            FnKind::TraitMethod {
                                kind: TraitMethodKind::Destructor,
                                impl_for,
                                ..
                            },
                        ..
                    },
                ..
            } => Some(ExplicitFound {
                ty: impl_for.clone(),
                kind: ExplicitKind::Destructor,
            }),
            Api::Function {
                analysis:
                    FnAnalysis {
                        kind:
                            FnKind::TraitMethod {
                                kind: TraitMethodKind::CopyConstructor,
                                impl_for,
                                ..
                            },
                        ..
                    },
                fun,
                ..
            } if is_deleted_or_inaccessible(fun) => Some(ExplicitFound {
                ty: impl_for.clone(),
                kind: ExplicitKind::DeletedOrInaccessibleCopyConstructor,
            }),
            Api::Function {
                analysis:
                    FnAnalysis {
                        kind:
                            FnKind::TraitMethod {
                                kind: TraitMethodKind::CopyConstructor,
                                impl_for,
                                ..
                            },
                        param_details,
                        ..
                    },
                ..
            } => {
                let receiver_mutability = &param_details
                    .iter()
                    .next()
                    .unwrap()
                    .self_type
                    .as_ref()
                    .unwrap()
                    .1;
                let kind = match receiver_mutability {
                    ReceiverMutability::Const => ExplicitKind::ConstCopyConstructor,
                    ReceiverMutability::Mutable => ExplicitKind::NonConstCopyConstructor,
                };
                Some(ExplicitFound {
                    ty: impl_for.clone(),
                    kind,
                })
            }
            Api::Function {
                fun,
                analysis:
                    FnAnalysis {
                        kind: FnKind::Method(self_ty, ..),
                        ..
                    },
                ..
            } if matches!(
                fun.special_member,
                Some(SpecialMemberKind::AssignmentOperator)
            ) =>
            {
                let is_move_assignment_operator = !fun.references.rvalue_ref_params.is_empty();
                Some(ExplicitFound {
                    ty: self_ty.clone(),
                    kind: if is_move_assignment_operator {
                        ExplicitKind::MoveAssignmentOperator
                    } else {
                        ExplicitKind::CopyAssignmentOperator
                    },
                })
            }
            _ => None,
        })
        .collect()
}

fn is_deleted_or_inaccessible(fun: &FuncToConvert) -> bool {
    fun.cpp_vis == CppVisibility::Private || fun.is_deleted
}
