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

mod byvalue_checker;

use std::collections::{HashMap, HashSet};

use autocxx_parser::IncludeCppConfig;
use byvalue_checker::ByValueChecker;
use syn::{FnArg, ItemEnum, ItemStruct, Type, TypePtr, Visibility};

use crate::{
    conversion::{
        analysis::type_converter::{add_analysis, TypeConversionContext, TypeConverter},
        api::{
            AnalysisPhase, Api, ApiName, CppVisibility, FuncToConvert, SpecialMemberKind,
            StructDetails, TypeKind, UnanalyzedApi,
        },
        convert_error::{ConvertErrorWithContext, ErrorContext},
        error_reporter::convert_apis,
        parse::BindgenSemanticAttributes,
        ConvertError,
    },
    types::{Namespace, QualifiedName},
};

use super::tdef::{TypedefAnalysis, TypedefPhase};

pub(crate) struct PodAnalysis {
    pub(crate) kind: TypeKind,
    pub(crate) bases: HashSet<QualifiedName>,
    /// Base classes for which we should create casts.
    /// That's just those which are on the allowlist,
    /// because otherwise we don't know whether they're
    /// abstract or not.
    pub(crate) castable_bases: HashSet<QualifiedName>,
    pub(crate) field_types: HashSet<QualifiedName>,
    pub(crate) movable: bool,
}

pub(crate) struct PodPhase;

impl AnalysisPhase for PodPhase {
    type TypedefAnalysis = TypedefAnalysis;
    type StructAnalysis = PodAnalysis;
    type FunAnalysis = ();
}

/// In our set of APIs, work out which ones are safe to represent
/// by value in Rust (e.g. they don't have a destructor) and record
/// as such. Return a set of APIs annotated with extra metadata,
/// and an object which can be used to query the POD status of any
/// type whether or not it's one of the [Api]s.
pub(crate) fn analyze_pod_apis(
    apis: Vec<Api<TypedefPhase>>,
    config: &IncludeCppConfig,
) -> Result<Vec<Api<PodPhase>>, ConvertError> {
    // This next line will return an error if any of the 'generate_pod'
    // directives from the user can't be met because, for instance,
    // a type contains a std::string or some other type which can't be
    // held safely by value in Rust.
    let byvalue_checker = ByValueChecker::new_from_apis(&apis, config)?;
    // We'll also note which types have deleted move constructors.
    let deleted_move_constructors = find_deleted_move_and_copy_constructors(&apis);
    let mut extra_apis = Vec::new();
    let mut type_converter = TypeConverter::new(config, &apis);
    let mut results = Vec::new();
    convert_apis(
        apis,
        &mut results,
        Api::fun_unchanged,
        |name, details, _| {
            analyze_struct(
                &byvalue_checker,
                &mut type_converter,
                &mut extra_apis,
                name,
                details,
                config,
                &deleted_move_constructors,
            )
        },
        analyze_enum,
        Api::typedef_unchanged,
    );
    // Conceivably, the process of POD-analysing the first set of APIs could result
    // in us creating new APIs to concretize generic types.
    let extra_apis: Vec<Api<PodPhase>> = extra_apis.into_iter().map(add_analysis).collect();
    let mut more_extra_apis = Vec::new();
    convert_apis(
        extra_apis,
        &mut results,
        Api::fun_unchanged,
        |name, details, _| {
            analyze_struct(
                &byvalue_checker,
                &mut type_converter,
                &mut more_extra_apis,
                name,
                details,
                config,
                &deleted_move_constructors,
            )
        },
        analyze_enum,
        Api::typedef_unchanged,
    );
    assert!(more_extra_apis.is_empty());
    Ok(results)
}

fn analyze_enum(
    name: ApiName,
    mut item: ItemEnum,
) -> Result<Box<dyn Iterator<Item = Api<PodPhase>>>, ConvertErrorWithContext> {
    let metadata = BindgenSemanticAttributes::new_retaining_others(&mut item.attrs);
    metadata.check_for_fatal_attrs(&name.name.get_final_ident())?;
    Ok(Box::new(std::iter::once(Api::Enum { name, item })))
}

fn analyze_struct(
    byvalue_checker: &ByValueChecker,
    type_converter: &mut TypeConverter,
    extra_apis: &mut Vec<UnanalyzedApi>,
    name: ApiName,
    mut details: Box<StructDetails>,
    config: &IncludeCppConfig,
    deleted_move_constructors: &HashSet<QualifiedName>,
) -> Result<Box<dyn Iterator<Item = Api<PodPhase>>>, ConvertErrorWithContext> {
    let movable = !deleted_move_constructors.contains(&name.name);
    let id = name.name.get_final_ident();
    if details.vis != CppVisibility::Public {
        return Err(ConvertErrorWithContext(
            ConvertError::NonPublicNestedType,
            Some(ErrorContext::Item(id)),
        ));
    }
    let metadata = BindgenSemanticAttributes::new_retaining_others(&mut details.item.attrs);
    metadata.check_for_fatal_attrs(&id)?;
    let bases = get_bases(&details.item);
    let mut field_types = HashSet::new();
    let field_conversion_errors = get_struct_field_types(
        type_converter,
        name.name.get_namespace(),
        &details.item,
        &mut field_types,
        extra_apis,
    );
    let type_kind = if byvalue_checker.is_pod(&name.name) {
        // It's POD so any errors encountered parsing its fields are important.
        // Let's not allow anything to be POD if it's got rvalue reference fields.
        if details.has_rvalue_reference_fields {
            return Err(ConvertErrorWithContext(
                ConvertError::RValueReferenceField,
                Some(ErrorContext::Item(id)),
            ));
        }
        if let Some(err) = field_conversion_errors.into_iter().next() {
            return Err(ConvertErrorWithContext(err, Some(ErrorContext::Item(id))));
        }
        TypeKind::Pod
    } else {
        TypeKind::NonPod
    };
    let castable_bases = bases
        .iter()
        .filter(|(_, is_public)| **is_public)
        .map(|(base, _)| base)
        .filter(|base| config.is_on_allowlist(&base.to_cpp_name()))
        .cloned()
        .collect();
    Ok(Box::new(std::iter::once(Api::Struct {
        name,
        details,
        analysis: PodAnalysis {
            kind: type_kind,
            bases: bases.into_keys().collect(),
            castable_bases,
            field_types,
            movable,
        },
    })))
}

fn get_struct_field_types(
    type_converter: &mut TypeConverter,
    ns: &Namespace,
    s: &ItemStruct,
    deps: &mut HashSet<QualifiedName>,
    extra_apis: &mut Vec<UnanalyzedApi>,
) -> Vec<ConvertError> {
    let mut convert_errors = Vec::new();
    for f in &s.fields {
        let annotated =
            type_converter.convert_type(f.ty.clone(), ns, &TypeConversionContext::CxxInnerType);
        match annotated {
            Ok(r) => {
                extra_apis.extend(r.extra_apis);
                deps.extend(r.types_encountered);
            }
            Err(e) => convert_errors.push(e),
        };
    }
    convert_errors
}

/// Map to whether the bases are public.
fn get_bases(item: &ItemStruct) -> HashMap<QualifiedName, bool> {
    item.fields
        .iter()
        .filter_map(|f| {
            let is_public = matches!(f.vis, Visibility::Public(_));
            match &f.ty {
                Type::Path(typ) => f
                    .ident
                    .as_ref()
                    .filter(|id| id.to_string().starts_with("_base"))
                    .map(|_| (QualifiedName::from_type_path(typ), is_public)),
                _ => None,
            }
        })
        .collect()
}

fn find_deleted_move_and_copy_constructors(apis: &[Api<TypedefPhase>]) -> HashSet<QualifiedName> {
    // Remove any deleted move + copy constructors from the API list and list the types
    // that they construct.
    apis.iter().filter_map(|api| match api {
        Api::Function { ref fun, .. } => match &**fun {
            FuncToConvert {
                special_member:
                    Some(SpecialMemberKind::MoveConstructor | SpecialMemberKind::CopyConstructor),
                is_deleted: true,
                inputs,
                ..
             } => match is_a_pointer_arg(inputs.iter().next()) {
                    Some(ty) => Some(ty),
                    _ => panic!("found special constructor member with something other than a pointer first arg"),
                },
            _ => None
        },
        _ => None,
    }).collect()
}

/// Determine if a function argument is a pointer, and if so, to what.
/// It's unfortunate that we need to do this during the POD analysis but
/// for now, it's the best way to identify special constructor members.
fn is_a_pointer_arg(arg: Option<&FnArg>) -> Option<QualifiedName> {
    arg.and_then(|arg| match arg {
        FnArg::Receiver(..) => None,
        FnArg::Typed(pt) => match &*pt.ty {
            Type::Ptr(TypePtr { elem, .. }) => match &**elem {
                Type::Path(typ) => Some(QualifiedName::from_type_path(typ)),
                _ => None,
            },
            _ => None,
        },
    })
}
