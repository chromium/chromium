// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use indexmap::map::IndexMap as HashMap;
use indexmap::{map::Entry, set::IndexSet as HashSet};

use syn::{Type, TypeArray};

use crate::conversion::api::DeletedOrDefaulted;
use crate::{
    conversion::{
        analysis::{
            depth_first::fields_and_bases_first, pod::PodAnalysis, type_converter::TypeKind,
        },
        api::{Api, ApiName, CppVisibility, FuncToConvert, SpecialMemberKind},
        apivec::ApiVec,
        convert_error::ConvertErrorWithContext,
        ConvertErrorFromCpp,
    },
    known_types::{known_types, KnownTypeConstructorDetails},
    types::QualifiedName,
};

use super::{FnAnalysis, FnKind, FnPrePhase1, MethodKind, ReceiverMutability, TraitMethodKind};

/// Indicates what we found out about a category of special member function.
///
/// In the end, we only care whether it's public and exists, but we track a bit more information to
/// support determining the information for dependent classes.
#[derive(Debug, Copy, Clone)]
pub(super) enum SpecialMemberFound {
    /// This covers being deleted in any way:
    ///   * Explicitly deleted
    ///   * Implicitly defaulted when that means being deleted
    ///   * Explicitly defaulted when that means being deleted
    ///
    /// It also covers not being either user declared or implicitly defaulted.
    NotPresent,
    /// Implicit special member functions, indicated by this, are always public.
    Implicit,
    /// This covers being explicitly defaulted (when that is not deleted) or being user-defined.
    Explicit(CppVisibility),
}

impl SpecialMemberFound {
    /// Returns whether code outside of subclasses can call this special member function.
    pub fn callable_any(&self) -> bool {
        matches!(self, Self::Explicit(CppVisibility::Public) | Self::Implicit)
    }

    /// Returns whether code in a subclass can call this special member function.
    pub fn callable_subclass(&self) -> bool {
        matches!(
            self,
            Self::Explicit(CppVisibility::Public)
                | Self::Explicit(CppVisibility::Protected)
                | Self::Implicit
        )
    }

    /// Returns whether this exists at all. Note that this will return true even if it's private,
    /// which is generally not very useful, but does come into play for some rules around which
    /// default special member functions are deleted vs don't exist.
    pub fn exists(&self) -> bool {
        matches!(self, Self::Explicit(_) | Self::Implicit)
    }

    pub fn exists_implicit(&self) -> bool {
        matches!(self, Self::Implicit)
    }

    pub fn exists_explicit(&self) -> bool {
        matches!(self, Self::Explicit(_))
    }
}

/// Information about which special member functions exist based on the C++ rules.
///
/// Not all of this information is used directly, but we need to track it to determine the
/// information we do need for classes which are used as members or base classes.
#[derive(Debug, Clone)]
pub(super) struct ItemsFound {
    pub(super) default_constructor: SpecialMemberFound,
    pub(super) destructor: SpecialMemberFound,
    pub(super) const_copy_constructor: SpecialMemberFound,
    /// Remember that [`const_copy_constructor`] may be used in place of this if it exists.
    pub(super) non_const_copy_constructor: SpecialMemberFound,
    pub(super) move_constructor: SpecialMemberFound,

    /// The full name of the type. We identify instances by [`QualifiedName`], because that's
    /// the only thing which [`FnKind::Method`] has to tie it to, and that's unique enough for
    /// identification.  However, when generating functions for implicit special members, we need
    /// the extra information here.
    ///
    /// Will always be `Some` if any of the other fields are [`SpecialMemberFound::Implict`],
    /// otherwise optional.
    pub(super) name: Option<ApiName>,
}

impl ItemsFound {
    /// Returns whether we should generate a default constructor wrapper, because bindgen won't do
    /// one for the implicit default constructor which exists.
    pub(super) fn implicit_default_constructor_needed(&self) -> bool {
        self.default_constructor.exists_implicit()
    }

    /// Returns whether we should generate a copy constructor wrapper, because bindgen won't do one
    /// for the implicit copy constructor which exists.
    pub(super) fn implicit_copy_constructor_needed(&self) -> bool {
        let any_implicit_copy = self.const_copy_constructor.exists_implicit()
            || self.non_const_copy_constructor.exists_implicit();
        let no_explicit_copy = !(self.const_copy_constructor.exists_explicit()
            || self.non_const_copy_constructor.exists_explicit());
        any_implicit_copy && no_explicit_copy
    }

    /// Returns whether we should generate a move constructor wrapper, because bindgen won't do one
    /// for the implicit move constructor which exists.
    pub(super) fn implicit_move_constructor_needed(&self) -> bool {
        self.move_constructor.exists_implicit()
    }

    /// Returns whether we should generate a destructor wrapper, because bindgen won't do one for
    /// the implicit destructor which exists.
    pub(super) fn implicit_destructor_needed(&self) -> bool {
        self.destructor.exists_implicit()
    }
}
#[derive(Hash, Eq, PartialEq)]
enum ExplicitKind {
    DefaultConstructor,
    ConstCopyConstructor,
    NonConstCopyConstructor,
    MoveConstructor,
    OtherConstructor,
    Destructor,
    ConstCopyAssignmentOperator,
    NonConstCopyAssignmentOperator,
    MoveAssignmentOperator,
}

/// Denotes a specific kind of explicit member function that we found.
#[derive(Hash, Eq, PartialEq)]
struct ExplicitType {
    ty: QualifiedName,
    kind: ExplicitKind,
}

/// Includes information about an explicit special member function which was found.
// TODO: Add Defaulted(CppVisibility) for https://github.com/google/autocxx/issues/815.
#[derive(Copy, Clone, Debug)]
enum ExplicitFound {
    UserDefined(CppVisibility),
    /// Note that this always means explicitly deleted, because this enum only represents
    /// explicit declarations.
    Deleted,
    /// Indicates that we found more than one explicit of this kind. This is possible with most of
    /// them, and we just bail and mostly act as if they're deleted. We'd have to decide whether
    /// they're ambiguous to use them, which is really complicated.
    Multiple,
}

/// Analyzes which constructors are present for each type.
///
/// If a type has explicit constructors, bindgen will generate corresponding
/// constructor functions, which we'll have already converted to make_unique methods.
/// For types with implicit constructors, we enumerate them here.
///
/// It is tempting to make this a separate analysis phase, to be run later than
/// the function analysis; but that would make the code much more complex as it
/// would need to output a `FnAnalysisBody`. By running it as part of this phase
/// we can simply generate the sort of thing bindgen generates, then ask
/// the existing code in this phase to figure out what to do with it.
pub(super) fn find_constructors_present(
    apis: &ApiVec<FnPrePhase1>,
) -> HashMap<QualifiedName, ItemsFound> {
    let (explicits, unknown_types) = find_explicit_items(apis);
    let enums: HashSet<QualifiedName> = apis
        .iter()
        .filter_map(|api| match api {
            Api::Enum { name, .. } => Some(name.name.clone()),
            _ => None,
        })
        .collect();

    // These contain all the classes we've seen so far with the relevant properties on their
    // constructors of each kind. We iterate via [`depth_first`], so analyzing later classes
    // just needs to check these.
    //
    // Important only to ask for a depth-first analysis of structs, because
    // when all APIs are considered there may be reference loops and that would
    // panic.
    //
    // These analyses include all bases and members of each class.
    let mut all_items_found: HashMap<QualifiedName, ItemsFound> = HashMap::new();

    for api in fields_and_bases_first(apis.iter()) {
        if let Api::Struct {
            name,
            analysis:
                PodAnalysis {
                    bases,
                    field_info,
                    is_generic: false,
                    in_anonymous_namespace: false,
                    ..
                },
            details,
            ..
        } = api
        {
            let find_explicit = |kind: ExplicitKind| -> Option<&ExplicitFound> {
                explicits.get(&ExplicitType {
                    ty: name.name.clone(),
                    kind,
                })
            };
            let get_items_found = |qn: &QualifiedName| -> Option<ItemsFound> {
                if enums.contains(qn) {
                    Some(ItemsFound {
                        default_constructor: SpecialMemberFound::NotPresent,
                        destructor: SpecialMemberFound::Implicit,
                        const_copy_constructor: SpecialMemberFound::Implicit,
                        non_const_copy_constructor: SpecialMemberFound::NotPresent,
                        move_constructor: SpecialMemberFound::Implicit,
                        name: Some(name.clone()),
                    })
                } else if let Some(constructor_details) = known_types().get_constructor_details(qn)
                {
                    Some(known_type_items_found(constructor_details))
                } else {
                    all_items_found.get(qn).cloned()
                }
            };
            let bases_items_found: Vec<_> = bases.iter().map_while(get_items_found).collect();
            let fields_items_found: Vec<_> = field_info
                .iter()
                .filter_map(|field_info| match field_info.type_kind {
                    TypeKind::Regular | TypeKind::SubclassHolder(_) => match field_info.ty {
                        Type::Path(ref qn) => get_items_found(&QualifiedName::from_type_path(qn)),
                        Type::Array(TypeArray { ref elem, .. }) => match elem.as_ref() {
                            Type::Path(ref qn) => {
                                get_items_found(&QualifiedName::from_type_path(qn))
                            }
                            _ => None,
                        },
                        _ => None,
                    },
                    // TODO: https://github.com/google/autocxx/issues/865 Figure out how to
                    // differentiate between pointers and references coming from C++. Pointers
                    // have a default constructor.
                    TypeKind::Pointer
                    | TypeKind::Reference
                    | TypeKind::MutableReference
                    | TypeKind::RValueReference => Some(ItemsFound {
                        default_constructor: SpecialMemberFound::NotPresent,
                        destructor: SpecialMemberFound::Implicit,
                        const_copy_constructor: SpecialMemberFound::Implicit,
                        non_const_copy_constructor: SpecialMemberFound::NotPresent,
                        move_constructor: SpecialMemberFound::Implicit,
                        name: Some(name.clone()),
                    }),
                })
                .collect();
            let has_rvalue_reference_fields = details.has_rvalue_reference_fields;

            // Check that all the bases and field types are known first. This combined with
            // iterating via [`depth_first`] means we can safely search in `items_found` for all of
            // them.
            //
            // Conservatively, we will not acknowledge the existence of most defaulted or implicit
            // special member functions for any struct/class where we don't fully understand all
            // field types.  However, we can still look for explictly declared versions and use
            // those. See below for destructors.
            //
            // We need to extend our knowledge to understand the constructor behavior of things in
            // known_types.rs, then we'll be able to cope with types which contain strings,
            // unique_ptrs etc.
            let items_found = if bases_items_found.len() != bases.len()
                || fields_items_found.len() != field_info.len()
                || unknown_types.contains(&name.name)
            {
                let is_explicit = |kind: ExplicitKind| -> SpecialMemberFound {
                    // TODO: For https://github.com/google/autocxx/issues/815, map
                    // ExplicitFound::Defaulted(_) to NotPresent.
                    match find_explicit(kind) {
                        None => SpecialMemberFound::NotPresent,
                        Some(ExplicitFound::Deleted | ExplicitFound::Multiple) => {
                            SpecialMemberFound::NotPresent
                        }
                        Some(ExplicitFound::UserDefined(visibility)) => {
                            SpecialMemberFound::Explicit(*visibility)
                        }
                    }
                };
                let items_found = ItemsFound {
                    default_constructor: is_explicit(ExplicitKind::DefaultConstructor),
                    destructor: match find_explicit(ExplicitKind::Destructor) {
                        // Assume that unknown types have destructors. This is common, and allows
                        // use to generate UniquePtr wrappers with them.
                        //
                        // However, this will generate C++ code that doesn't compile if the unknown
                        // type does not have an accessible destructor. Maybe we should have a way
                        // to disable that?
                        //
                        // TODO: For https://github.com/google/autocxx/issues/815, map
                        // ExplicitFound::Defaulted(_) to Explicit.
                        None => SpecialMemberFound::Implicit,
                        // If there are multiple destructors, assume that one of them will be
                        // selected by overload resolution.
                        Some(ExplicitFound::Multiple) => {
                            SpecialMemberFound::Explicit(CppVisibility::Public)
                        }
                        Some(ExplicitFound::Deleted) => SpecialMemberFound::NotPresent,
                        Some(ExplicitFound::UserDefined(visibility)) => {
                            SpecialMemberFound::Explicit(*visibility)
                        }
                    },
                    const_copy_constructor: is_explicit(ExplicitKind::ConstCopyConstructor),
                    non_const_copy_constructor: is_explicit(ExplicitKind::NonConstCopyConstructor),
                    move_constructor: is_explicit(ExplicitKind::MoveConstructor),
                    name: Some(name.clone()),
                };
                log::info!(
                    "Special member functions (explicits only) found for {:?}: {:?}",
                    name,
                    items_found
                );
                items_found
            } else {
                // If no user-declared constructors of any kind are provided for a class type (struct, class, or union),
                // the compiler will always declare a default constructor as an inline public member of its class.
                //
                // The implicitly-declared or defaulted default constructor for class T is defined as deleted if any of the following is true:
                // T has a member of reference type without a default initializer.
                // T has a non-const-default-constructible const member without a default member initializer.
                // T has a member (without a default member initializer) which has a deleted default constructor, or its default constructor is ambiguous or inaccessible from this constructor.
                // T has a direct or virtual base which has a deleted default constructor, or it is ambiguous or inaccessible from this constructor.
                // T has a direct or virtual base or a non-static data member which has a deleted destructor, or a destructor that is inaccessible from this constructor.
                // T is a union with at least one variant member with non-trivial default constructor, and no variant member of T has a default member initializer. // we don't support unions anyway
                // T is a non-union class with a variant member M with a non-trivial default constructor, and no variant member of the anonymous union containing M has a default member initializer.
                // T is a union and all of its variant members are const. // we don't support unions anyway
                //
                // Variant members are the members of anonymous unions.
                let default_constructor = {
                    let explicit = find_explicit(ExplicitKind::DefaultConstructor);
                    // TODO: For https://github.com/google/autocxx/issues/815, replace the first term with:
                    //   explicit.map_or(true, |explicit_found| matches!(explicit_found, ExplicitFound::Defaulted(_)))
                    let have_defaulted = explicit.is_none()
                        && !explicits.iter().any(|(ExplicitType { ty, kind }, _)| {
                            ty == &name.name
                                && match *kind {
                                    ExplicitKind::DefaultConstructor => false,
                                    ExplicitKind::ConstCopyConstructor => true,
                                    ExplicitKind::NonConstCopyConstructor => true,
                                    ExplicitKind::MoveConstructor => true,
                                    ExplicitKind::OtherConstructor => true,
                                    ExplicitKind::Destructor => false,
                                    ExplicitKind::ConstCopyAssignmentOperator => false,
                                    ExplicitKind::NonConstCopyAssignmentOperator => false,
                                    ExplicitKind::MoveAssignmentOperator => false,
                                }
                        });
                    if have_defaulted {
                        let bases_allow = bases_items_found.iter().all(|items_found| {
                            items_found.destructor.callable_subclass()
                                && items_found.default_constructor.callable_subclass()
                        });
                        // TODO: Allow member initializers for
                        // https://github.com/google/autocxx/issues/816.
                        let members_allow = fields_items_found.iter().all(|items_found| {
                            items_found.destructor.callable_any()
                                && items_found.default_constructor.callable_any()
                        });
                        if !has_rvalue_reference_fields && bases_allow && members_allow {
                            // TODO: For https://github.com/google/autocxx/issues/815, grab the
                            // visibility from an explicit default if present.
                            SpecialMemberFound::Implicit
                        } else {
                            SpecialMemberFound::NotPresent
                        }
                    } else if let Some(ExplicitFound::UserDefined(visibility)) = explicit {
                        SpecialMemberFound::Explicit(*visibility)
                    } else {
                        SpecialMemberFound::NotPresent
                    }
                };

                // If no user-declared prospective destructor is provided for a class type (struct, class, or union), the compiler will always declare a destructor as an inline public member of its class.
                //
                // The implicitly-declared or explicitly defaulted destructor for class T is defined as deleted if any of the following is true:
                // T has a non-static data member that cannot be destructed (has deleted or inaccessible destructor)
                // T has direct or virtual base class that cannot be destructed (has deleted or inaccessible destructors)
                // T is a union and has a variant member with non-trivial destructor. // we don't support unions anyway
                // The implicitly-declared destructor is virtual (because the base class has a virtual destructor) and the lookup for the deallocation function (operator delete()) results in a call to ambiguous, deleted, or inaccessible function.
                let destructor = {
                    let explicit = find_explicit(ExplicitKind::Destructor);
                    // TODO: For https://github.com/google/autocxx/issues/815, replace the condition with:
                    //   explicit.map_or(true, |explicit_found| matches!(explicit_found, ExplicitFound::Defaulted(_)))
                    if explicit.is_none() {
                        let bases_allow = bases_items_found
                            .iter()
                            .all(|items_found| items_found.destructor.callable_subclass());
                        let members_allow = fields_items_found
                            .iter()
                            .all(|items_found| items_found.destructor.callable_any());
                        if bases_allow && members_allow {
                            // TODO: For https://github.com/google/autocxx/issues/815, grab the
                            // visibility from an explicit default if present.
                            SpecialMemberFound::Implicit
                        } else {
                            SpecialMemberFound::NotPresent
                        }
                    } else if let Some(ExplicitFound::UserDefined(visibility)) = explicit {
                        SpecialMemberFound::Explicit(*visibility)
                    } else {
                        SpecialMemberFound::NotPresent
                    }
                };

                // If no user-defined copy constructors are provided for a class type (struct, class, or union),
                // the compiler will always declare a copy constructor as a non-explicit inline public member of its class.
                // This implicitly-declared copy constructor has the form T::T(const T&) if all of the following are true:
                //  each direct and virtual base B of T has a copy constructor whose parameters are const B& or const volatile B&;
                //  each non-static data member M of T of class type or array of class type has a copy constructor whose parameters are const M& or const volatile M&.
                //
                // The implicitly-declared or defaulted copy constructor for class T is defined as deleted if any of the following conditions are true:
                // T is a union-like class and has a variant member with non-trivial copy constructor; // we don't support unions anyway
                // T has a user-defined move constructor or move assignment operator (this condition only causes the implicitly-declared, not the defaulted, copy constructor to be deleted).
                // T has non-static data members that cannot be copied (have deleted, inaccessible, or ambiguous copy constructors);
                // T has direct or virtual base class that cannot be copied (has deleted, inaccessible, or ambiguous copy constructors);
                // T has direct or virtual base class or a non-static data member with a deleted or inaccessible destructor;
                // T has a data member of rvalue reference type;
                let (const_copy_constructor, non_const_copy_constructor) = {
                    let explicit_const = find_explicit(ExplicitKind::ConstCopyConstructor);
                    let explicit_non_const = find_explicit(ExplicitKind::NonConstCopyConstructor);
                    let explicit_move = find_explicit(ExplicitKind::MoveConstructor);

                    // TODO: For https://github.com/google/autocxx/issues/815, replace both terms with something like:
                    //   explicit.map_or(true, |explicit_found| matches!(explicit_found, ExplicitFound::Defaulted(_)))
                    let have_defaulted = explicit_const.is_none() && explicit_non_const.is_none();
                    if have_defaulted {
                        // TODO: For https://github.com/google/autocxx/issues/815, ignore this if
                        // the relevant (based on bases_are_const) copy constructor is explicitly defaulted.
                        let class_allows = explicit_move.is_none() && !has_rvalue_reference_fields;
                        let bases_allow = bases_items_found.iter().all(|items_found| {
                            items_found.destructor.callable_subclass()
                                && (items_found.const_copy_constructor.callable_subclass()
                                    || items_found.non_const_copy_constructor.callable_subclass())
                        });
                        let members_allow = fields_items_found.iter().all(|items_found| {
                            items_found.destructor.callable_any()
                                && (items_found.const_copy_constructor.callable_any()
                                    || items_found.non_const_copy_constructor.callable_any())
                        });
                        if class_allows && bases_allow && members_allow {
                            // TODO: For https://github.com/google/autocxx/issues/815, grab the
                            // visibility and existence of const and non-const from an explicit default if present.
                            let dependencies_are_const = bases_items_found
                                .iter()
                                .chain(fields_items_found.iter())
                                .all(|items_found| items_found.const_copy_constructor.exists());
                            if dependencies_are_const {
                                (SpecialMemberFound::Implicit, SpecialMemberFound::NotPresent)
                            } else {
                                (SpecialMemberFound::NotPresent, SpecialMemberFound::Implicit)
                            }
                        } else {
                            (
                                SpecialMemberFound::NotPresent,
                                SpecialMemberFound::NotPresent,
                            )
                        }
                    } else {
                        (
                            if let Some(ExplicitFound::UserDefined(visibility)) = explicit_const {
                                SpecialMemberFound::Explicit(*visibility)
                            } else {
                                SpecialMemberFound::NotPresent
                            },
                            if let Some(ExplicitFound::UserDefined(visibility)) = explicit_non_const
                            {
                                SpecialMemberFound::Explicit(*visibility)
                            } else {
                                SpecialMemberFound::NotPresent
                            },
                        )
                    }
                };

                // If no user-defined move constructors are provided for a class type (struct, class, or union), and all of the following is true:
                // there are no user-declared copy constructors;
                // there are no user-declared copy assignment operators;
                // there are no user-declared move assignment operators;
                // there is no user-declared destructor.
                // then the compiler will declare a move constructor as a non-explicit inline public member of its class with the signature T::T(T&&).
                //
                // A class can have multiple move constructors, e.g. both T::T(const T&&) and T::T(T&&). If some user-defined move constructors are present, the user may still force the generation of the implicitly declared move constructor with the keyword default.
                //
                // The implicitly-declared or defaulted move constructor for class T is defined as deleted if any of the following is true:
                // T has non-static data members that cannot be moved (have deleted, inaccessible, or ambiguous move constructors);
                // T has direct or virtual base class that cannot be moved (has deleted, inaccessible, or ambiguous move constructors);
                // T has direct or virtual base class with a deleted or inaccessible destructor;
                // T is a union-like class and has a variant member with non-trivial move constructor. // we don't support unions anyway
                let move_constructor = {
                    let explicit = find_explicit(ExplicitKind::MoveConstructor);
                    // TODO: For https://github.com/google/autocxx/issues/815, replace relevant terms with something like:
                    //   explicit.map_or(true, |explicit_found| matches!(explicit_found, ExplicitFound::Defaulted(_)))
                    let have_defaulted = !(explicit.is_some()
                        || find_explicit(ExplicitKind::ConstCopyConstructor).is_some()
                        || find_explicit(ExplicitKind::NonConstCopyConstructor).is_some()
                        || find_explicit(ExplicitKind::ConstCopyAssignmentOperator).is_some()
                        || find_explicit(ExplicitKind::NonConstCopyAssignmentOperator).is_some()
                        || find_explicit(ExplicitKind::MoveAssignmentOperator).is_some()
                        || find_explicit(ExplicitKind::Destructor).is_some());
                    if have_defaulted {
                        let bases_allow = bases_items_found.iter().all(|items_found| {
                            items_found.destructor.callable_subclass()
                                && items_found.move_constructor.callable_subclass()
                        });
                        let members_allow = fields_items_found
                            .iter()
                            .all(|items_found| items_found.move_constructor.callable_any());
                        if bases_allow && members_allow {
                            // TODO: For https://github.com/google/autocxx/issues/815, grab the
                            // visibility from an explicit default if present.
                            SpecialMemberFound::Implicit
                        } else {
                            SpecialMemberFound::NotPresent
                        }
                    } else if let Some(ExplicitFound::UserDefined(visibility)) = explicit {
                        SpecialMemberFound::Explicit(*visibility)
                    } else {
                        SpecialMemberFound::NotPresent
                    }
                };

                let items_found = ItemsFound {
                    default_constructor,
                    destructor,
                    const_copy_constructor,
                    non_const_copy_constructor,
                    move_constructor,
                    name: Some(name.clone()),
                };
                log::info!(
                    "Special member items found for {:?}: {:?}",
                    name,
                    items_found
                );
                items_found
            };
            assert!(
                all_items_found
                    .insert(name.name.clone(), items_found)
                    .is_none(),
                "Duplicate struct: {:?}",
                name
            );
        }
    }

    all_items_found
}

fn find_explicit_items(
    apis: &ApiVec<FnPrePhase1>,
) -> (HashMap<ExplicitType, ExplicitFound>, HashSet<QualifiedName>) {
    let mut result = HashMap::new();
    let mut merge_fun = |ty: QualifiedName, kind: ExplicitKind, fun: &FuncToConvert| match result
        .entry(ExplicitType { ty, kind })
    {
        Entry::Vacant(entry) => {
            entry.insert(if matches!(fun.is_deleted, DeletedOrDefaulted::Deleted) {
                ExplicitFound::Deleted
            } else {
                ExplicitFound::UserDefined(fun.cpp_vis)
            });
        }
        Entry::Occupied(mut entry) => {
            entry.insert(ExplicitFound::Multiple);
        }
    };
    let mut unknown_types = HashSet::new();
    for api in apis.iter() {
        match api {
            Api::Function {
                analysis:
                    FnAnalysis {
                        kind: FnKind::Method { impl_for, .. },
                        param_details,
                        ignore_reason:
                            Ok(())
                            | Err(ConvertErrorWithContext(ConvertErrorFromCpp::AssignmentOperator, _)),
                        ..
                    },
                fun,
                ..
            } if matches!(
                fun.special_member,
                Some(SpecialMemberKind::AssignmentOperator)
            ) =>
            {
                let is_move_assignment_operator = !fun.references.rvalue_ref_params.is_empty();
                merge_fun(
                    impl_for.clone(),
                    if is_move_assignment_operator {
                        ExplicitKind::MoveAssignmentOperator
                    } else {
                        let receiver_mutability = &param_details
                            .iter()
                            .next()
                            .unwrap()
                            .self_type
                            .as_ref()
                            .unwrap()
                            .1;
                        match receiver_mutability {
                            ReceiverMutability::Const => ExplicitKind::ConstCopyAssignmentOperator,
                            ReceiverMutability::Mutable => {
                                ExplicitKind::NonConstCopyAssignmentOperator
                            }
                        }
                    },
                    fun,
                )
            }
            Api::Function {
                analysis:
                    FnAnalysis {
                        kind: FnKind::Method { impl_for, .. },
                        ..
                    },
                fun,
                ..
            } if matches!(
                fun.special_member,
                Some(SpecialMemberKind::AssignmentOperator)
            ) =>
            {
                unknown_types.insert(impl_for.clone());
            }
            Api::Function {
                analysis:
                    FnAnalysis {
                        kind:
                            FnKind::Method {
                                impl_for,
                                method_kind,
                                ..
                            },
                        ..
                    },
                fun,
                ..
            } => match method_kind {
                MethodKind::Constructor { is_default: true } => {
                    Some(ExplicitKind::DefaultConstructor)
                }
                MethodKind::Constructor { is_default: false } => {
                    Some(ExplicitKind::OtherConstructor)
                }
                _ => None,
            }
            .map_or((), |explicit_kind| {
                merge_fun(impl_for.clone(), explicit_kind, fun)
            }),
            Api::Function {
                analysis:
                    FnAnalysis {
                        kind: FnKind::TraitMethod { impl_for, kind, .. },
                        ..
                    },
                fun,
                ..
            } => match kind {
                TraitMethodKind::Destructor => Some(ExplicitKind::Destructor),
                // In `analyze_foreign_fn` we mark non-const copy constructors as not being copy
                // constructors for now, so we don't have to worry about them.
                TraitMethodKind::CopyConstructor => Some(ExplicitKind::ConstCopyConstructor),
                TraitMethodKind::MoveConstructor => Some(ExplicitKind::MoveConstructor),
                _ => None,
            }
            .map_or((), |explicit_kind| {
                merge_fun(impl_for.clone(), explicit_kind, fun)
            }),
            _ => (),
        }
    }
    (result, unknown_types)
}

/// Returns the information for a given known type.
fn known_type_items_found(constructor_details: KnownTypeConstructorDetails) -> ItemsFound {
    let exists_public = SpecialMemberFound::Explicit(CppVisibility::Public);
    let exists_public_if = |exists| {
        if exists {
            exists_public
        } else {
            SpecialMemberFound::NotPresent
        }
    };
    ItemsFound {
        default_constructor: exists_public,
        destructor: exists_public,
        const_copy_constructor: exists_public_if(constructor_details.has_const_copy_constructor),
        non_const_copy_constructor: SpecialMemberFound::NotPresent,
        move_constructor: exists_public_if(constructor_details.has_move_constructor),
        name: None,
    }
}
