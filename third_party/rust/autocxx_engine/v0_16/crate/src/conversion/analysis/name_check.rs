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

use std::collections::HashMap;

use syn::Ident;

use crate::{
    conversion::{
        api::{Api, SubclassName},
        error_reporter::convert_item_apis,
        ConvertError,
    },
    types::{validate_ident_ok_for_cxx, QualifiedName},
};

use super::fun::FnPhase;

/// Do some final checks that the names we've come up with can be represented
/// within cxx.
pub(crate) fn check_names(apis: Vec<Api<FnPhase>>) -> Vec<Api<FnPhase>> {
    // If any items have names which can't be represented by cxx,
    // abort. This check should ideally be done at the times we fill in the
    // `name` field of each `api` in the first place, at parse time, though
    // as the `name` field of each API may change during various analysis phases,
    // currently it seems better to do it here to ensure we respect
    // the output of any such changes.
    let mut intermediate = Vec::new();
    convert_item_apis(apis, &mut intermediate, |api| match api {
        Api::Typedef { ref name, .. }
        | Api::ForwardDeclaration { ref name, .. }
        | Api::Const { ref name, .. }
        | Api::Enum { ref name, .. }
        | Api::Struct { ref name, .. } => {
            validate_all_segments_ok_for_cxx(name.name.segment_iter())?;
            if let Some(cpp_name) = name.cpp_name_if_present() {
                // The C++ name might itself be outer_type::inner_type and thus may
                // have multiple segments.
                validate_all_segments_ok_for_cxx(
                    QualifiedName::new_from_cpp_name(cpp_name).segment_iter(),
                )?;
            }
            Ok(Box::new(std::iter::once(api)))
        }
        Api::Subclass {
            name: SubclassName(ref name),
            ref superclass,
        } => {
            validate_all_segments_ok_for_cxx(name.name.segment_iter())?;
            validate_all_segments_ok_for_cxx(superclass.segment_iter())?;
            Ok(Box::new(std::iter::once(api)))
        }
        Api::Function { ref name, .. } => {
            // we don't handle function names here because
            // the function analysis does an equivalent check. Instead of just rejecting
            // the function, it creates a wrapper function instead with a more
            // palatable name. That's preferable to rejecting the API entirely.
            validate_all_segments_ok_for_cxx(name.name.segment_iter())?;
            Ok(Box::new(std::iter::once(api)))
        }
        Api::ConcreteType { .. }
        | Api::CType { .. }
        | Api::StringConstructor { .. }
        | Api::RustType { .. }
        | Api::RustSubclassFn { .. }
        | Api::RustFn { .. }
        | Api::IgnoredItem { .. } => Ok(Box::new(std::iter::once(api))),
    });

    // Reject any names which are duplicates within the cxx bridge mod,
    // that has a flat namespace.
    let mut names_found: HashMap<Ident, usize> = HashMap::new();
    for api in &intermediate {
        let my_name = api.cxxbridge_name();
        if let Some(name) = my_name {
            let e = names_found.entry(name).or_default();
            *e += 1usize;
        }
    }
    let mut results = Vec::new();
    convert_item_apis(intermediate, &mut results, |api| {
        let my_name = api.cxxbridge_name();
        if let Some(name) = my_name {
            if *names_found.entry(name).or_default() > 1usize {
                Err(ConvertError::DuplicateCxxBridgeName)
            } else {
                Ok(Box::new(std::iter::once(api)))
            }
        } else {
            Ok(Box::new(std::iter::once(api)))
        }
    });
    results
}

fn validate_all_segments_ok_for_cxx(
    items: impl Iterator<Item = String>,
) -> Result<(), ConvertError> {
    for seg in items {
        validate_ident_ok_for_cxx(&seg)?;
    }
    Ok(())
}
