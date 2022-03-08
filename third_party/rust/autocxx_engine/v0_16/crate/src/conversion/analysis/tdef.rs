// Copyright 2021 Google LLC
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

use autocxx_parser::IncludeCppConfig;
use syn::ItemType;

use crate::{
    conversion::{
        analysis::type_converter::{add_analysis, Annotated, TypeConversionContext, TypeConverter},
        api::{AnalysisPhase, Api, ApiName, TypedefKind, UnanalyzedApi},
        convert_error::{ConvertErrorWithContext, ErrorContext},
        error_reporter::convert_apis,
        parse::BindgenSemanticAttributes,
        ConvertError,
    },
    types::QualifiedName,
};

pub(crate) struct TypedefAnalysis {
    pub(crate) kind: TypedefKind,
    pub(crate) deps: HashSet<QualifiedName>,
}

/// Analysis phase where typedef analysis has been performed but no other
/// analyses just yet.
pub(crate) struct TypedefPhase;

impl AnalysisPhase for TypedefPhase {
    type TypedefAnalysis = TypedefAnalysis;
    type StructAnalysis = ();
    type FunAnalysis = ();
}

#[allow(clippy::needless_collect)] // we need the extra collect because the closure borrows extra_apis
pub(crate) fn convert_typedef_targets(
    config: &IncludeCppConfig,
    apis: Vec<UnanalyzedApi>,
) -> Vec<Api<TypedefPhase>> {
    let mut type_converter = TypeConverter::new(config, &apis);
    let mut extra_apis = Vec::new();
    let mut results = Vec::new();
    convert_apis(
        apis,
        &mut results,
        Api::fun_unchanged,
        Api::struct_unchanged,
        Api::enum_unchanged,
        |name, item, old_tyname, _| {
            Ok(Box::new(std::iter::once(match item {
                TypedefKind::Type(ity) => get_replacement_typedef(
                    name,
                    ity,
                    old_tyname,
                    &mut type_converter,
                    &mut extra_apis,
                )?,
                TypedefKind::Use { .. } => Api::Typedef {
                    name,
                    item: item.clone(),
                    old_tyname,
                    analysis: TypedefAnalysis {
                        kind: item,
                        deps: HashSet::new(),
                    },
                },
            })))
        },
    );
    results.extend(extra_apis.into_iter().map(add_analysis));
    results
}

fn get_replacement_typedef(
    name: ApiName,
    ity: ItemType,
    old_tyname: Option<QualifiedName>,
    type_converter: &mut TypeConverter,
    extra_apis: &mut Vec<UnanalyzedApi>,
) -> Result<Api<TypedefPhase>, ConvertErrorWithContext> {
    let mut converted_type = ity.clone();
    let metadata = BindgenSemanticAttributes::new_retaining_others(&mut converted_type.attrs);
    metadata.check_for_fatal_attrs(&ity.ident)?;
    let type_conversion_results = type_converter.convert_type(
        (*ity.ty).clone(),
        name.name.get_namespace(),
        &TypeConversionContext::CxxInnerType,
    );
    match type_conversion_results {
        Err(err) => Err(ConvertErrorWithContext(
            err,
            Some(ErrorContext::Item(name.name.get_final_ident())),
        )),
        Ok(Annotated {
            ty: syn::Type::Path(ref typ),
            ..
        }) if QualifiedName::from_type_path(typ) == name.name => Err(ConvertErrorWithContext(
            ConvertError::InfinitelyRecursiveTypedef(name.name.clone()),
            Some(ErrorContext::Item(name.name.get_final_ident())),
        )),
        Ok(mut final_type) => {
            converted_type.ty = Box::new(final_type.ty.clone());
            extra_apis.append(&mut final_type.extra_apis);
            Ok(Api::Typedef {
                name,
                item: TypedefKind::Type(ity),
                old_tyname,
                analysis: TypedefAnalysis {
                    kind: TypedefKind::Type(converted_type),
                    deps: final_type.types_encountered,
                },
            })
        }
    }
}
