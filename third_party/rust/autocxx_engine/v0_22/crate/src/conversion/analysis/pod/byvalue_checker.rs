// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::conversion::apivec::ApiVec;
use crate::{conversion::ConvertError, known_types::known_types};
use crate::{
    conversion::{
        analysis::tdef::TypedefPhase,
        api::{Api, TypedefKind},
    },
    types::{Namespace, QualifiedName},
};
use autocxx_parser::IncludeCppConfig;
use std::collections::HashMap;
use syn::{ItemStruct, Type};

#[derive(Clone)]
enum PodState {
    UnsafeToBePod(String),
    SafeToBePod,
    IsPod,
    IsAlias(QualifiedName),
}

#[derive(Clone)]
struct StructDetails {
    state: PodState,
    dependent_structs: Vec<QualifiedName>,
}

impl StructDetails {
    fn new(state: PodState) -> Self {
        StructDetails {
            state,
            dependent_structs: Vec::new(),
        }
    }
}

/// Type which is able to check whether it's safe to make a type
/// fully representable by cxx. For instance if it is a struct containing
/// a struct containing a std::string, the answer is no, because that
/// std::string contains a self-referential pointer.
/// It is possible that this is duplicative of the information stored
/// elsewhere in the `Api` list and could possibly be removed or simplified.
pub struct ByValueChecker {
    // Mapping from type name to whether it is safe to be POD
    results: HashMap<QualifiedName, StructDetails>,
}

impl ByValueChecker {
    pub fn new() -> Self {
        let mut results = HashMap::new();
        for (tn, by_value_safe) in known_types().get_pod_safe_types() {
            let safety = if by_value_safe {
                PodState::IsPod
            } else {
                PodState::UnsafeToBePod(format!("type {} is not safe for POD", tn))
            };
            results.insert(tn.clone(), StructDetails::new(safety));
        }
        ByValueChecker { results }
    }

    /// Scan APIs to work out which are by-value safe. Constructs a [ByValueChecker]
    /// that others can use to query the results.
    pub(crate) fn new_from_apis(
        apis: &ApiVec<TypedefPhase>,
        config: &IncludeCppConfig,
    ) -> Result<ByValueChecker, ConvertError> {
        let mut byvalue_checker = ByValueChecker::new();
        for blocklisted in config.get_blocklist() {
            let tn = QualifiedName::new_from_cpp_name(blocklisted);
            let safety = PodState::UnsafeToBePod(format!("type {} is on the blocklist", &tn));
            byvalue_checker
                .results
                .insert(tn, StructDetails::new(safety));
        }
        for api in apis.iter() {
            match api {
                Api::Typedef { analysis, .. } => {
                    let name = api.name();
                    let typedef_type = match analysis.kind {
                        TypedefKind::Type(ref type_item) => match type_item.ty.as_ref() {
                            Type::Path(typ) => {
                                let target_tn = QualifiedName::from_type_path(typ);
                                known_types().consider_substitution(&target_tn)
                            }
                            _ => None,
                        },
                        TypedefKind::Use(_, ref ty) => match **ty {
                            Type::Path(ref typ) => {
                                let target_tn = QualifiedName::from_type_path(typ);
                                known_types().consider_substitution(&target_tn)
                            }
                            _ => None,
                        },
                    };
                    match &typedef_type {
                        Some(typ) => {
                            byvalue_checker.results.insert(
                                name.clone(),
                                StructDetails::new(PodState::IsAlias(
                                    QualifiedName::from_type_path(typ),
                                )),
                            );
                        }
                        None => byvalue_checker.ingest_nonpod_type(name.clone()),
                    }
                }
                Api::Struct { details, .. } => {
                    byvalue_checker.ingest_struct(&details.item, api.name().get_namespace())
                }
                Api::Enum { .. } => {
                    byvalue_checker
                        .results
                        .insert(api.name().clone(), StructDetails::new(PodState::IsPod));
                }
                Api::ExternCppType { pod: true, .. } => {
                    byvalue_checker
                        .results
                        .insert(api.name().clone(), StructDetails::new(PodState::IsPod));
                }
                _ => {}
            }
        }
        let pod_requests = config
            .get_pod_requests()
            .iter()
            .map(|ty| QualifiedName::new_from_cpp_name(ty))
            .collect();
        byvalue_checker
            .satisfy_requests(pod_requests)
            .map_err(ConvertError::UnsafePodType)?;
        Ok(byvalue_checker)
    }

    fn ingest_struct(&mut self, def: &ItemStruct, ns: &Namespace) {
        // For this struct, work out whether it _could_ be safe as a POD.
        let tyname = QualifiedName::new(ns, def.ident.clone());
        let mut field_safety_problem = PodState::SafeToBePod;
        let fieldlist = Self::get_field_types(def);
        for ty_id in &fieldlist {
            match self.results.get(ty_id) {
                None => {
                    field_safety_problem = PodState::UnsafeToBePod(format!(
                        "Type {} could not be POD because its dependent type {} isn't known",
                        tyname, ty_id
                    ));
                    break;
                }
                Some(deets) => {
                    if let PodState::UnsafeToBePod(reason) = &deets.state {
                        let new_reason = format!("Type {} could not be POD because its dependent type {} isn't safe to be POD. Because: {}", tyname, ty_id, reason);
                        field_safety_problem = PodState::UnsafeToBePod(new_reason);
                        break;
                    }
                }
            }
        }
        if Self::has_vtable(def) {
            let reason = format!(
                "Type {} could not be POD because it has virtual functions.",
                tyname
            );
            field_safety_problem = PodState::UnsafeToBePod(reason);
        }
        let mut my_details = StructDetails::new(field_safety_problem);
        my_details.dependent_structs = fieldlist;
        self.results.insert(tyname, my_details);
    }

    fn ingest_nonpod_type(&mut self, tyname: QualifiedName) {
        let new_reason = format!("Type {} is a typedef to a complex type", tyname);
        self.results.insert(
            tyname,
            StructDetails::new(PodState::UnsafeToBePod(new_reason)),
        );
    }

    fn satisfy_requests(&mut self, mut requests: Vec<QualifiedName>) -> Result<(), String> {
        while !requests.is_empty() {
            let ty_id = requests.remove(requests.len() - 1);
            let deets = self.results.get_mut(&ty_id);
            let mut alias_to_consider = None;
            match deets {
                None => {
                    return Err(format!(
                        "Unable to make {} POD because we never saw a struct definition",
                        ty_id
                    ))
                }
                Some(deets) => match &deets.state {
                    PodState::UnsafeToBePod(error_msg) => return Err(error_msg.clone()),
                    PodState::IsPod => {}
                    PodState::SafeToBePod => {
                        deets.state = PodState::IsPod;
                        requests.extend_from_slice(&deets.dependent_structs);
                    }
                    PodState::IsAlias(target_type) => {
                        alias_to_consider = Some(target_type.clone());
                    }
                },
            }
            // Do the following outside the match to avoid borrow checker violation.
            if let Some(alias) = alias_to_consider {
                match self.results.get(&alias) {
                    None => requests.extend_from_slice(&[alias, ty_id]), // try again after resolving alias target
                    Some(alias_target_deets) => {
                        self.results.get_mut(&ty_id).unwrap().state =
                            alias_target_deets.state.clone();
                    }
                }
            }
        }
        Ok(())
    }

    /// Return whether a given type is POD (i.e. can be represented by value in Rust) or not.
    /// Unless we've got a definite record that it _is_, we return false.
    /// Some types won't be in our `results` map. For example: (a) AutocxxConcrete types
    /// which we've synthesized; (b) types we couldn't parse but returned ignorable
    /// errors so that we could continue. Assume non-POD for all such cases.
    pub fn is_pod(&self, ty_id: &QualifiedName) -> bool {
        matches!(
            self.results.get(ty_id),
            Some(StructDetails {
                state: PodState::IsPod,
                dependent_structs: _,
            })
        )
    }

    fn get_field_types(def: &ItemStruct) -> Vec<QualifiedName> {
        let mut results = Vec::new();
        for f in &def.fields {
            let fty = &f.ty;
            if let Type::Path(p) = fty {
                results.push(QualifiedName::from_type_path(p));
            }
            // TODO handle anything else which bindgen might spit out, e.g. arrays?
        }
        results
    }

    fn has_vtable(def: &ItemStruct) -> bool {
        for f in &def.fields {
            if f.ident.as_ref().map(|id| id == "vtable_").unwrap_or(false) {
                return true;
            }
        }
        false
    }
}

#[cfg(test)]
mod tests {
    use super::ByValueChecker;
    use crate::types::{Namespace, QualifiedName};
    use syn::{parse_quote, Ident, ItemStruct};

    fn ty_from_ident(id: &Ident) -> QualifiedName {
        QualifiedName::new_from_cpp_name(&id.to_string())
    }

    #[test]
    fn test_primitive_by_itself() {
        let bvc = ByValueChecker::new();
        let t_id = QualifiedName::new_from_cpp_name("u32");
        assert!(bvc.is_pod(&t_id));
    }

    #[test]
    fn test_primitives() {
        let mut bvc = ByValueChecker::new();
        let t: ItemStruct = parse_quote! {
            struct Foo {
                a: i32,
                b: i64,
            }
        };
        let t_id = ty_from_ident(&t.ident);
        bvc.ingest_struct(&t, &Namespace::new());
        bvc.satisfy_requests(vec![t_id.clone()]).unwrap();
        assert!(bvc.is_pod(&t_id));
    }

    #[test]
    fn test_nested_primitives() {
        let mut bvc = ByValueChecker::new();
        let t: ItemStruct = parse_quote! {
            struct Foo {
                a: i32,
                b: i64,
            }
        };
        bvc.ingest_struct(&t, &Namespace::new());
        let t: ItemStruct = parse_quote! {
            struct Bar {
                a: Foo,
                b: i64,
            }
        };
        let t_id = ty_from_ident(&t.ident);
        bvc.ingest_struct(&t, &Namespace::new());
        bvc.satisfy_requests(vec![t_id.clone()]).unwrap();
        assert!(bvc.is_pod(&t_id));
    }

    #[test]
    fn test_with_up() {
        let mut bvc = ByValueChecker::new();
        let t: ItemStruct = parse_quote! {
            struct Bar {
                a: cxx::UniquePtr<CxxString>,
                b: i64,
            }
        };
        let t_id = ty_from_ident(&t.ident);
        bvc.ingest_struct(&t, &Namespace::new());
        bvc.satisfy_requests(vec![t_id.clone()]).unwrap();
        assert!(bvc.is_pod(&t_id));
    }

    #[test]
    fn test_with_cxxstring() {
        let mut bvc = ByValueChecker::new();
        let t: ItemStruct = parse_quote! {
            struct Bar {
                a: CxxString,
                b: i64,
            }
        };
        let t_id = ty_from_ident(&t.ident);
        bvc.ingest_struct(&t, &Namespace::new());
        assert!(bvc.satisfy_requests(vec![t_id]).is_err());
    }
}
