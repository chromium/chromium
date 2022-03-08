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

use crate::{
    conversion::{api::Api, AnalysisPhase, ConvertError},
    types::QualifiedName,
};
use itertools::Itertools;
use quote::ToTokens;
use std::collections::HashMap;
use std::iter::once;
use syn::{Token, Type};

/// Map from QualifiedName to original C++ name. Original C++ name does not
/// include the namespace; this can be assumed to be the same as the namespace
/// in the QualifiedName.
pub(crate) type CppNameMap = HashMap<QualifiedName, String>;

pub(crate) fn original_name_map_from_apis<T: AnalysisPhase>(apis: &[Api<T>]) -> CppNameMap {
    apis.iter()
        .filter_map(|api| {
            api.cpp_name()
                .as_ref()
                .map(|cpp_name| (api.name().clone(), cpp_name.clone()))
        })
        .collect()
}

pub(crate) fn namespaced_name_using_original_name_map(
    qual_name: &QualifiedName,
    original_name_map: &CppNameMap,
) -> String {
    if let Some(cpp_name) = original_name_map.get(qual_name) {
        qual_name
            .get_namespace()
            .iter()
            .chain(once(cpp_name))
            .join("::")
    } else {
        qual_name.to_cpp_name()
    }
}

pub(crate) fn identifier_using_original_name_map(
    qual_name: &QualifiedName,
    original_name_map: &CppNameMap,
) -> String {
    original_name_map
        .get(qual_name)
        .cloned()
        .unwrap_or_else(|| qual_name.get_final_cpp_item())
}

pub(crate) fn type_to_cpp(ty: &Type, cpp_name_map: &CppNameMap) -> Result<String, ConvertError> {
    match ty {
        Type::Path(typ) => {
            // If this is a std::unique_ptr we do need to pass
            // its argument through.
            let qual_name = QualifiedName::from_type_path(typ);
            let root = namespaced_name_using_original_name_map(&qual_name, cpp_name_map);
            if root == "Pin" {
                // Strip all Pins from type names when describing them in C++.
                let inner_type = &typ.path.segments.last().unwrap().arguments;
                if let syn::PathArguments::AngleBracketed(ab) = inner_type {
                    let inner_type = ab.args.iter().next().unwrap();
                    if let syn::GenericArgument::Type(gat) = inner_type {
                        return type_to_cpp(gat, cpp_name_map);
                    }
                }
                panic!("Pin<...> didn't contain the inner types we expected");
            }
            let suffix = match &typ.path.segments.last().unwrap().arguments {
                syn::PathArguments::AngleBracketed(ab) => {
                    let results: Result<Vec<_>, _> = ab
                        .args
                        .iter()
                        .map(|x| match x {
                            syn::GenericArgument::Type(gat) => type_to_cpp(gat, cpp_name_map),
                            _ => Ok("".to_string()),
                        })
                        .collect();
                    Some(results?.join(", "))
                }
                syn::PathArguments::None | syn::PathArguments::Parenthesized(_) => None,
            };
            match suffix {
                None => Ok(root),
                Some(suffix) => Ok(format!("{}<{}>", root, suffix)),
            }
        }
        Type::Reference(typr) => Ok(format!(
            "{}{}&",
            get_mut_string(&typr.mutability),
            type_to_cpp(typr.elem.as_ref(), cpp_name_map)?
        )),
        Type::Ptr(typp) => Ok(format!(
            "{}{}*",
            get_mut_string(&typp.mutability),
            type_to_cpp(typp.elem.as_ref(), cpp_name_map)?
        )),
        Type::Array(_)
        | Type::BareFn(_)
        | Type::Group(_)
        | Type::ImplTrait(_)
        | Type::Infer(_)
        | Type::Macro(_)
        | Type::Never(_)
        | Type::Paren(_)
        | Type::Slice(_)
        | Type::TraitObject(_)
        | Type::Tuple(_)
        | Type::Verbatim(_) => Err(ConvertError::UnsupportedType(
            ty.to_token_stream().to_string(),
        )),
        _ => Err(ConvertError::UnknownType(ty.to_token_stream().to_string())),
    }
}

fn get_mut_string(mutability: &Option<Token![mut]>) -> &'static str {
    match mutability {
        None => "const ",
        Some(_) => "",
    }
}
