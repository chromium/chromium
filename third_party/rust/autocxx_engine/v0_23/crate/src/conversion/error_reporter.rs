// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use syn::ItemEnum;

use super::{
    api::{AnalysisPhase, Api, ApiName, FuncToConvert, StructDetails, TypedefKind},
    apivec::ApiVec,
    convert_error::{ConvertErrorWithContext, ErrorContext},
    ConvertErrorFromCpp,
};
use crate::{
    conversion::convert_error::ErrorContextType,
    types::{Namespace, QualifiedName},
};

/// Run some code which may generate a ConvertError.
/// If it does, try to note the problem in our output APIs
/// such that users will see documentation of the error.
pub(crate) fn report_any_error<F, T>(
    ns: &Namespace,
    apis: &mut ApiVec<impl AnalysisPhase>,
    fun: F,
) -> Option<T>
where
    F: FnOnce() -> Result<T, ConvertErrorWithContext>,
{
    match fun() {
        Ok(result) => Some(result),
        Err(ConvertErrorWithContext(err, None)) => {
            eprintln!("Ignored item: {}", err);
            None
        }
        Err(ConvertErrorWithContext(err, Some(ctx))) => {
            let id = match ctx.get_type() {
                ErrorContextType::Item(id) | ErrorContextType::SanitizedItem(id) => id,
                ErrorContextType::Method { self_ty, .. } => self_ty,
            };
            let name = ApiName::new_from_qualified_name(QualifiedName::new(ns, id.clone()));
            apis.push(Api::IgnoredItem {
                name,
                err,
                ctx: Some(ctx),
            });
            None
        }
    }
}

/// Run some code which generates an API. Add that API, or if
/// anything goes wrong, instead add a note of the problem in our
/// output API such that users will see documentation for the problem.
pub(crate) fn convert_apis<FF, SF, EF, TF, A, B: 'static>(
    in_apis: ApiVec<A>,
    out_apis: &mut ApiVec<B>,
    mut func_conversion: FF,
    mut struct_conversion: SF,
    mut enum_conversion: EF,
    mut typedef_conversion: TF,
) where
    A: AnalysisPhase,
    B: AnalysisPhase,
    FF: FnMut(
        ApiName,
        Box<FuncToConvert>,
        A::FunAnalysis,
    ) -> Result<Box<dyn Iterator<Item = Api<B>>>, ConvertErrorWithContext>,
    SF: FnMut(
        ApiName,
        Box<StructDetails>,
        A::StructAnalysis,
    ) -> Result<Box<dyn Iterator<Item = Api<B>>>, ConvertErrorWithContext>,
    EF: FnMut(
        ApiName,
        ItemEnum,
    ) -> Result<Box<dyn Iterator<Item = Api<B>>>, ConvertErrorWithContext>,
    TF: FnMut(
        ApiName,
        TypedefKind,
        Option<QualifiedName>,
        A::TypedefAnalysis,
    ) -> Result<Box<dyn Iterator<Item = Api<B>>>, ConvertErrorWithContext>,
{
    out_apis.extend(in_apis.into_iter().flat_map(|api| {
        let fullname = api.name_info().clone();
        let result: Result<Box<dyn Iterator<Item = Api<B>>>, ConvertErrorWithContext> = match api {
            // No changes to any of these...
            Api::ConcreteType {
                name,
                rs_definition,
                cpp_definition,
            } => Ok(Box::new(std::iter::once(Api::ConcreteType {
                name,
                rs_definition,
                cpp_definition,
            }))),
            Api::ForwardDeclaration { name, err } => {
                Ok(Box::new(std::iter::once(Api::ForwardDeclaration {
                    name,
                    err,
                })))
            }
            Api::OpaqueTypedef {
                name,
                forward_declaration,
            } => Ok(Box::new(std::iter::once(Api::OpaqueTypedef {
                name,
                forward_declaration,
            }))),
            Api::StringConstructor { name } => {
                Ok(Box::new(std::iter::once(Api::StringConstructor { name })))
            }
            Api::Const { name, const_item } => {
                Ok(Box::new(std::iter::once(Api::Const { name, const_item })))
            }
            Api::CType { name, typename } => {
                Ok(Box::new(std::iter::once(Api::CType { name, typename })))
            }
            Api::RustType { name, path } => {
                Ok(Box::new(std::iter::once(Api::RustType { name, path })))
            }
            Api::RustFn {
                name,
                details,
                deps,
            } => Ok(Box::new(std::iter::once(Api::RustFn {
                name,
                details,
                deps,
            }))),
            Api::RustSubclassFn {
                name,
                subclass,
                details,
            } => Ok(Box::new(std::iter::once(Api::RustSubclassFn {
                name,
                subclass,
                details,
            }))),
            Api::Subclass { name, superclass } => Ok(Box::new(std::iter::once(Api::Subclass {
                name,
                superclass,
            }))),
            Api::IgnoredItem { name, err, ctx } => {
                Ok(Box::new(std::iter::once(Api::IgnoredItem {
                    name,
                    err,
                    ctx,
                })))
            }
            Api::SubclassTraitItem { name, details } => {
                Ok(Box::new(std::iter::once(Api::SubclassTraitItem {
                    name,
                    details,
                })))
            }
            Api::ExternCppType { name, details, pod } => {
                Ok(Box::new(std::iter::once(Api::ExternCppType {
                    name,
                    details,
                    pod,
                })))
            }
            // Apply a mapping to the following
            Api::Enum { name, item } => enum_conversion(name, item),
            Api::Typedef {
                name,
                item,
                old_tyname,
                analysis,
            } => typedef_conversion(name, item, old_tyname, analysis),
            Api::Function {
                name,
                fun,
                analysis,
            } => func_conversion(name, fun, analysis),
            Api::Struct {
                name,
                details,
                analysis,
            } => struct_conversion(name, details, analysis),
        };
        api_or_error(fullname, result)
    }))
}

fn api_or_error<T: AnalysisPhase + 'static>(
    name: ApiName,
    api_or_error: Result<Box<dyn Iterator<Item = Api<T>>>, ConvertErrorWithContext>,
) -> Box<dyn Iterator<Item = Api<T>>> {
    match api_or_error {
        Ok(opt) => opt,
        Err(ConvertErrorWithContext(err, ctx)) => {
            Box::new(std::iter::once(Api::IgnoredItem { name, err, ctx }))
        }
    }
}

/// Run some code which generates an API for an item (as opposed to
/// a method). Add that API, or if
/// anything goes wrong, instead add a note of the problem in our
/// output API such that users will see documentation for the problem.
pub(crate) fn convert_item_apis<F, A, B: 'static>(
    in_apis: ApiVec<A>,
    out_apis: &mut ApiVec<B>,
    mut fun: F,
) where
    F: FnMut(Api<A>) -> Result<Box<dyn Iterator<Item = Api<B>>>, ConvertErrorFromCpp>,
    A: AnalysisPhase,
    B: AnalysisPhase,
{
    out_apis.extend(in_apis.into_iter().flat_map(|api| {
        let fullname = api.name_info().clone();
        let tn = api.name().clone();
        let result = fun(api).map_err(|e| {
            ConvertErrorWithContext(e, Some(ErrorContext::new_for_item(tn.get_final_ident())))
        });
        api_or_error(fullname, result)
    }))
}
