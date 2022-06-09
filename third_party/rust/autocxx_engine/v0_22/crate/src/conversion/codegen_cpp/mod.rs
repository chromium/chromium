// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

mod function_wrapper_cpp;
mod new_and_delete_prelude;
pub(crate) mod type_to_cpp;

use crate::{
    conversion::analysis::fun::{function_wrapper::CppFunctionKind, FnAnalysis},
    types::{make_ident, QualifiedName},
    CppCodegenOptions, CppFilePair,
};
use autocxx_parser::IncludeCppConfig;
use indexmap::map::IndexMap as HashMap;
use indexmap::set::IndexSet as HashSet;
use itertools::Itertools;
use std::borrow::Cow;
use type_to_cpp::{original_name_map_from_apis, type_to_cpp, CppNameMap};

use self::type_to_cpp::{
    final_ident_using_original_name_map, namespaced_name_using_original_name_map,
};

use super::{
    analysis::{
        fun::{
            function_wrapper::{CppFunction, CppFunctionBody},
            FnPhase, PodAndDepAnalysis,
        },
        pod::PodAnalysis,
    },
    api::{Api, Provenance, SubclassName, TypeKind},
    apivec::ApiVec,
    ConvertError,
};

#[derive(Ord, PartialOrd, Eq, PartialEq, Clone, Hash)]
enum Header {
    System(&'static str),
    CxxH,
    CxxgenH,
    NewDeletePrelude,
}

impl Header {
    fn include_stmt(
        &self,
        cpp_codegen_options: &CppCodegenOptions,
        cxxgen_header_name: &str,
    ) -> String {
        let blank = "".to_string();
        match self {
            Self::System(name) => format!("#include <{}>", name),
            Self::CxxH => {
                let prefix = cpp_codegen_options.path_to_cxx_h.as_ref().unwrap_or(&blank);
                format!("#include \"{}cxx.h\"", prefix)
            }
            Self::CxxgenH => {
                let prefix = cpp_codegen_options
                    .path_to_cxxgen_h
                    .as_ref()
                    .unwrap_or(&blank);
                format!("#include \"{}{}\"", prefix, cxxgen_header_name)
            }
            Header::NewDeletePrelude => new_and_delete_prelude::NEW_AND_DELETE_PRELUDE.to_string(),
        }
    }

    fn is_system(&self) -> bool {
        matches!(self, Header::System(_) | Header::CxxH)
    }
}

enum ConversionDirection {
    RustCallsCpp,
    CppCallsCpp,
    CppCallsRust,
}

/// Some extra snippet of C++ which we (autocxx) need to generate, beyond
/// that which cxx itself generates.
#[derive(Default)]
struct ExtraCpp {
    type_definition: Option<String>, // are output before main declarations
    declaration: Option<String>,
    definition: Option<String>,
    headers: Vec<Header>,
    cpp_headers: Vec<Header>,
}

/// Generates additional C++ glue functions needed by autocxx.
/// In some ways it would be preferable to be able to pass snippets
/// of C++ through to `cxx` for inclusion in the C++ file which it
/// generates, and perhaps we'll explore that in future. But for now,
/// autocxx generates its own _additional_ C++ files which therefore
/// need to be built and included in linking procedures.
pub(crate) struct CppCodeGenerator<'a> {
    additional_functions: Vec<ExtraCpp>,
    inclusions: String,
    original_name_map: CppNameMap,
    config: &'a IncludeCppConfig,
    cpp_codegen_options: &'a CppCodegenOptions<'a>,
    cxxgen_header_name: &'a str,
}

struct SubclassFunction<'a> {
    fun: &'a CppFunction,
    is_pure_virtual: bool,
}

impl<'a> CppCodeGenerator<'a> {
    pub(crate) fn generate_cpp_code(
        inclusions: String,
        apis: &ApiVec<FnPhase>,
        config: &'a IncludeCppConfig,
        cpp_codegen_options: &CppCodegenOptions,
        cxxgen_header_name: &str,
    ) -> Result<Option<CppFilePair>, ConvertError> {
        let mut gen = CppCodeGenerator {
            additional_functions: Vec::new(),
            inclusions,
            original_name_map: original_name_map_from_apis(apis),
            config,
            cpp_codegen_options,
            cxxgen_header_name,
        };
        // The 'filter' on the following line is designed to ensure we don't accidentally
        // end up out of sync with needs_cpp_codegen
        gen.add_needs(apis.iter().filter(|api| api.needs_cpp_codegen()))?;
        Ok(gen.generate())
    }

    // It's important to keep this in sync with Api::needs_cpp_codegen.
    fn add_needs<'b>(
        &mut self,
        apis: impl Iterator<Item = &'a Api<FnPhase>>,
    ) -> Result<(), ConvertError> {
        let mut constructors_by_subclass: HashMap<SubclassName, Vec<&CppFunction>> = HashMap::new();
        let mut methods_by_subclass: HashMap<SubclassName, Vec<SubclassFunction>> = HashMap::new();
        let mut deferred_apis = Vec::new();
        for api in apis {
            match &api {
                Api::StringConstructor { .. } => self.generate_string_constructor(),
                Api::Function {
                    analysis:
                        FnAnalysis {
                            cpp_wrapper: Some(cpp_wrapper),
                            ignore_reason: Ok(_),
                            externally_callable: true,
                            ..
                        },
                    fun,
                    ..
                } => {
                    if let Provenance::SynthesizedSubclassConstructor(details) = &fun.provenance {
                        constructors_by_subclass
                            .entry(details.subclass.clone())
                            .or_default()
                            .push(&details.cpp_impl);
                    }
                    self.generate_cpp_function(cpp_wrapper)?
                }
                Api::ConcreteType {
                    rs_definition,
                    cpp_definition,
                    ..
                } => {
                    let effective_cpp_definition = match rs_definition {
                        Some(rs_definition) => {
                            Cow::Owned(type_to_cpp(rs_definition, &self.original_name_map)?)
                        }
                        None => Cow::Borrowed(cpp_definition),
                    };

                    self.generate_typedef(api.name(), &effective_cpp_definition)
                }
                Api::CType { typename, .. } => self.generate_ctype_typedef(typename),
                Api::Subclass { .. } => deferred_apis.push(api),
                Api::RustSubclassFn {
                    subclass, details, ..
                } => {
                    methods_by_subclass
                        .entry(subclass.clone())
                        .or_default()
                        .push(SubclassFunction {
                            fun: &details.cpp_impl,
                            is_pure_virtual: details.is_pure_virtual,
                        });
                }
                Api::Struct {
                    name,
                    analysis:
                        PodAndDepAnalysis {
                            pod:
                                PodAnalysis {
                                    kind: TypeKind::Pod,
                                    ..
                                },
                            ..
                        },
                    ..
                } => {
                    self.generate_pod_assertion(name.qualified_cpp_name());
                }
                _ => panic!("Should have filtered on needs_cpp_codegen"),
            }
        }

        for api in deferred_apis.into_iter() {
            match api {
                Api::Subclass { name, superclass } => self.generate_subclass(
                    superclass,
                    name,
                    constructors_by_subclass.remove(name).unwrap_or_default(),
                    methods_by_subclass.remove(name).unwrap_or_default(),
                )?,
                _ => panic!("Unexpected deferred API"),
            }
        }
        Ok(())
    }

    fn generate(&self) -> Option<CppFilePair> {
        if self.additional_functions.is_empty() {
            None
        } else {
            let headers = self.collect_headers(|additional_need| &additional_need.headers);
            let cpp_headers = self.collect_headers(|additional_need| &additional_need.cpp_headers);
            let type_definitions = self.concat_additional_items(|x| x.type_definition.as_ref());
            let declarations = self.concat_additional_items(|x| x.declaration.as_ref());
            let declarations = format!(
                "#ifndef __AUTOCXXGEN_H__\n#define __AUTOCXXGEN_H__\n\n{}\n{}\n{}\n{}#endif // __AUTOCXXGEN_H__\n",
                headers, self.inclusions, type_definitions, declarations
            );
            log::info!("Additional C++ decls:\n{}", declarations);
            let header_name = self
                .cpp_codegen_options
                .autocxxgen_header_namer
                .name_header(self.config.get_mod_name().to_string());
            let implementation = if self
                .additional_functions
                .iter()
                .any(|x| x.definition.is_some())
            {
                let definitions = self.concat_additional_items(|x| x.definition.as_ref());
                let definitions = format!(
                    "#include \"{}\"\n{}\n{}",
                    header_name, cpp_headers, definitions
                );
                log::info!("Additional C++ defs:\n{}", definitions);
                Some(definitions.into_bytes())
            } else {
                None
            };
            Some(CppFilePair {
                header: declarations.into_bytes(),
                implementation,
                header_name,
            })
        }
    }

    fn collect_headers<F>(&self, filter: F) -> String
    where
        F: Fn(&ExtraCpp) -> &[Header],
    {
        let cpp_headers: HashSet<_> = self
            .additional_functions
            .iter()
            .flat_map(|x| filter(x).iter())
            .filter(|x| !self.cpp_codegen_options.suppress_system_headers || !x.is_system())
            .collect(); // uniqify
        cpp_headers
            .iter()
            .map(|x| x.include_stmt(self.cpp_codegen_options, self.cxxgen_header_name))
            .join("\n")
    }

    fn concat_additional_items<F>(&self, field_access: F) -> String
    where
        F: FnMut(&ExtraCpp) -> Option<&String>,
    {
        let mut s = self
            .additional_functions
            .iter()
            .flat_map(field_access)
            .join("\n");
        s.push('\n');
        s
    }

    fn generate_pod_assertion(&mut self, name: String) {
        // These assertions are generated by cxx for trivial ExternTypes but
        // *only if* such types are used as trivial types in the cxx::bridge.
        // It's possible for types which we generate to be used even without
        // passing through the cxx::bridge, and as we generate Drop impls, that
        // can result in destructors for nested types being called multiple times
        // if we represent them as trivial types. So generate an extra
        // assertion to make sure.
        let declaration = Some(format!("static_assert(::rust::IsRelocatable<{}>::value, \"type {} should be trivially move constructible and trivially destructible to be used with generate_pod! in autocxx\");", name, name));
        self.additional_functions.push(ExtraCpp {
            declaration,
            headers: vec![Header::CxxH],
            ..Default::default()
        })
    }

    fn generate_string_constructor(&mut self) {
        let makestring_name = self.config.get_makestring_name();
        let declaration = Some(format!("inline std::unique_ptr<std::string> {}(::rust::Str str) {{ return std::make_unique<std::string>(std::string(str)); }}", makestring_name));
        self.additional_functions.push(ExtraCpp {
            declaration,
            headers: vec![
                Header::System("memory"),
                Header::System("string"),
                Header::CxxH,
            ],
            ..Default::default()
        })
    }

    fn generate_cpp_function(&mut self, details: &CppFunction) -> Result<(), ConvertError> {
        self.additional_functions
            .push(self.generate_cpp_function_inner(
                details,
                false,
                ConversionDirection::RustCallsCpp,
                false,
                None,
            )?);
        Ok(())
    }

    fn generate_cpp_function_inner(
        &self,
        details: &CppFunction,
        avoid_this: bool,
        conversion_direction: ConversionDirection,
        requires_rust_declarations: bool,
        force_name: Option<&str>,
    ) -> Result<ExtraCpp, ConvertError> {
        // Even if the original function call is in a namespace,
        // we generate this wrapper in the global namespace.
        // We could easily do this the other way round, and when
        // cxx::bridge comes to support nested namespace mods then
        // we wil wish to do that to avoid name conflicts. However,
        // at the moment this is simpler because it avoids us having
        // to generate namespace blocks in the generated C++.
        let is_a_method = !avoid_this
            && matches!(
                details.kind,
                CppFunctionKind::Method
                    | CppFunctionKind::ConstMethod
                    | CppFunctionKind::Constructor
            );
        let name = match force_name {
            Some(n) => n.to_string(),
            None => details.wrapper_function_name.to_string(),
        };
        let get_arg_name = |counter: usize| -> String {
            if is_a_method && counter == 0 {
                // For method calls that we generate, the first
                // argument name needs to be such that we recognize
                // it as a method in the second invocation of
                // bridge_converter after it's flowed again through
                // bindgen.
                // TODO this may not be the case any longer. We
                // may be able to remove this.
                "autocxx_gen_this".to_string()
            } else {
                format!("arg{}", counter)
            }
        };
        // If this returns a non-POD value, we may instead wish to emplace
        // it into a parameter, let's see.
        let args: Result<Vec<_>, _> = details
            .argument_conversion
            .iter()
            .enumerate()
            .map(|(counter, ty)| {
                Ok(format!(
                    "{} {}",
                    match conversion_direction {
                        ConversionDirection::RustCallsCpp =>
                            ty.unconverted_type(&self.original_name_map)?,
                        ConversionDirection::CppCallsCpp =>
                            ty.converted_type(&self.original_name_map)?,
                        ConversionDirection::CppCallsRust =>
                            ty.inverse().unconverted_type(&self.original_name_map)?,
                    },
                    get_arg_name(counter)
                ))
            })
            .collect();
        let args = args?.join(", ");
        let default_return = match details.kind {
            CppFunctionKind::SynthesizedConstructor => "",
            _ => "void",
        };
        let ret_type = details
            .return_conversion
            .as_ref()
            .and_then(|x| match conversion_direction {
                ConversionDirection::RustCallsCpp => {
                    if x.populate_return_value() {
                        Some(x.converted_type(&self.original_name_map))
                    } else {
                        None
                    }
                }
                ConversionDirection::CppCallsCpp => {
                    Some(x.unconverted_type(&self.original_name_map))
                }
                ConversionDirection::CppCallsRust => {
                    Some(x.inverse().converted_type(&self.original_name_map))
                }
            })
            .unwrap_or_else(|| Ok(default_return.to_string()))?;
        let constness = match details.kind {
            CppFunctionKind::ConstMethod => " const",
            _ => "",
        };
        let declaration = format!("{} {}({}){}", ret_type, name, args, constness);
        let qualification = if let Some(qualification) = &details.qualification {
            format!("{}::", qualification.to_cpp_name())
        } else {
            "".to_string()
        };
        let qualified_declaration = format!(
            "{} {}{}({}){}",
            ret_type, qualification, name, args, constness
        );
        // Whether there's a placement param in which to put the return value
        let placement_param = details
            .argument_conversion
            .iter()
            .enumerate()
            .filter_map(|(counter, conv)| {
                if conv.is_placement_parameter() {
                    Some(get_arg_name(counter))
                } else {
                    None
                }
            })
            .next();
        // Arguments to underlying function call
        let arg_list: Result<Vec<_>, _> = details
            .argument_conversion
            .iter()
            .enumerate()
            .map(|(counter, conv)| match conversion_direction {
                ConversionDirection::RustCallsCpp => {
                    conv.cpp_conversion(&get_arg_name(counter), &self.original_name_map, false)
                }
                ConversionDirection::CppCallsCpp => Ok(Some(get_arg_name(counter))),
                ConversionDirection::CppCallsRust => conv.inverse().cpp_conversion(
                    &get_arg_name(counter),
                    &self.original_name_map,
                    false,
                ),
            })
            .collect();
        let mut arg_list = arg_list?.into_iter().flatten();
        let receiver = if is_a_method { arg_list.next() } else { None };
        if matches!(&details.payload, CppFunctionBody::ConstructSuperclass(_)) {
            arg_list.next();
        }
        let arg_list = if details.pass_obs_field {
            std::iter::once("*obs".to_string())
                .chain(arg_list)
                .join(",")
        } else {
            arg_list.join(", ")
        };
        let (mut underlying_function_call, field_assignments, need_allocators) = match &details
            .payload
        {
            CppFunctionBody::Cast => (arg_list, "".to_string(), false),
            CppFunctionBody::PlacementNew(ns, id) => {
                let ty_id = QualifiedName::new(ns, id.clone());
                let ty_id = self.namespaced_name(&ty_id);
                (
                    format!("new ({}) {}({})", receiver.unwrap(), ty_id, arg_list),
                    "".to_string(),
                    false,
                )
            }
            CppFunctionBody::Destructor(ns, id) => {
                let ty_id = QualifiedName::new(ns, id.clone());
                let ty_id = final_ident_using_original_name_map(&ty_id, &self.original_name_map);
                (format!("{}->~{}()", arg_list, ty_id), "".to_string(), false)
            }
            CppFunctionBody::FunctionCall(ns, id) => match receiver {
                Some(receiver) => (
                    format!("{}.{}({})", receiver, id, arg_list),
                    "".to_string(),
                    false,
                ),
                None => {
                    let underlying_function_call = ns
                        .into_iter()
                        .cloned()
                        .chain(std::iter::once(id.to_string()))
                        .join("::");
                    (
                        format!("{}({})", underlying_function_call, arg_list),
                        "".to_string(),
                        false,
                    )
                }
            },
            CppFunctionBody::StaticMethodCall(ns, ty_id, fn_id) => {
                let underlying_function_call = ns
                    .into_iter()
                    .cloned()
                    .chain([ty_id.to_string(), fn_id.to_string()].iter().cloned())
                    .join("::");
                (
                    format!("{}({})", underlying_function_call, arg_list),
                    "".to_string(),
                    false,
                )
            }
            CppFunctionBody::ConstructSuperclass(_) => ("".to_string(), arg_list, false),
            CppFunctionBody::AllocUninitialized(ty) => {
                let namespaced_ty = self.namespaced_name(ty);
                (
                    format!("new_appropriately<{}>();", namespaced_ty,),
                    "".to_string(),
                    true,
                )
            }
            CppFunctionBody::FreeUninitialized(ty) => (
                format!("delete_appropriately<{}>(arg0);", self.namespaced_name(ty)),
                "".to_string(),
                true,
            ),
        };
        if let Some(ret) = &details.return_conversion {
            let call_itself = match conversion_direction {
                ConversionDirection::RustCallsCpp => {
                    ret.cpp_conversion(&underlying_function_call, &self.original_name_map, true)?
                }
                ConversionDirection::CppCallsCpp => Some(underlying_function_call),
                ConversionDirection::CppCallsRust => ret.inverse().cpp_conversion(
                    &underlying_function_call,
                    &self.original_name_map,
                    true,
                )?,
            }
            .expect(
                "Expected some conversion type for return value which resulted in a parameter name",
            );

            underlying_function_call = match placement_param {
                Some(placement_param) => {
                    let tyname = type_to_cpp(&ret.unwrapped_type, &self.original_name_map)?;
                    format!("new({}) {}({})", placement_param, tyname, call_itself)
                }
                None => format!("return {}", call_itself),
            };
        };
        if !underlying_function_call.is_empty() {
            underlying_function_call = format!("{};", underlying_function_call);
        }
        let field_assignments =
            if let CppFunctionBody::ConstructSuperclass(superclass_name) = &details.payload {
                let superclass_assignments = if field_assignments.is_empty() {
                    "".to_string()
                } else {
                    format!("{}({}), ", superclass_name, field_assignments)
                };
                format!(": {}obs(std::move(arg0))", superclass_assignments)
            } else {
                "".into()
            };
        let definition_after_sig =
            format!("{} {{ {} }}", field_assignments, underlying_function_call,);
        let (declaration, definition) = if requires_rust_declarations {
            (
                Some(format!("{};", declaration)),
                Some(format!(
                    "{} {}",
                    qualified_declaration, definition_after_sig
                )),
            )
        } else {
            (
                Some(format!("inline {} {}", declaration, definition_after_sig)),
                None,
            )
        };
        let mut headers = vec![Header::System("memory")];
        if need_allocators {
            headers.push(Header::System("stddef.h"));
            headers.push(Header::NewDeletePrelude);
        }
        Ok(ExtraCpp {
            declaration,
            definition,
            headers,
            ..Default::default()
        })
    }

    fn namespaced_name(&self, name: &QualifiedName) -> String {
        namespaced_name_using_original_name_map(name, &self.original_name_map)
    }

    fn generate_ctype_typedef(&mut self, tn: &QualifiedName) {
        let cpp_name = tn.to_cpp_name();
        self.generate_typedef(tn, &cpp_name)
    }

    fn generate_typedef(&mut self, tn: &QualifiedName, definition: &str) {
        let our_name = tn.get_final_item();
        self.additional_functions.push(ExtraCpp {
            type_definition: Some(format!("typedef {} {};", definition, our_name)),
            ..Default::default()
        })
    }

    fn generate_subclass(
        &mut self,
        superclass: &QualifiedName,
        subclass: &SubclassName,
        constructors: Vec<&CppFunction>,
        methods: Vec<SubclassFunction>,
    ) -> Result<(), ConvertError> {
        let holder = subclass.holder();
        self.additional_functions.push(ExtraCpp {
            type_definition: Some(format!("struct {};", holder)),
            ..Default::default()
        });
        let mut method_decls = Vec::new();
        for method in methods {
            // First the method which calls from C++ to Rust
            let mut fn_impl = self.generate_cpp_function_inner(
                method.fun,
                true,
                ConversionDirection::CppCallsRust,
                true,
                Some(&method.fun.original_cpp_name),
            )?;
            method_decls.push(fn_impl.declaration.take().unwrap());
            self.additional_functions.push(fn_impl);
            // And now the function to be called from Rust for default implementation (calls superclass in C++)
            if !method.is_pure_virtual {
                let mut super_method = method.fun.clone();
                super_method.pass_obs_field = false;
                super_method.wrapper_function_name = SubclassName::get_super_fn_name(
                    superclass.get_namespace(),
                    &method.fun.wrapper_function_name.to_string(),
                )
                .get_final_ident();
                super_method.payload = CppFunctionBody::StaticMethodCall(
                    superclass.get_namespace().clone(),
                    superclass.get_final_ident(),
                    make_ident(&method.fun.original_cpp_name),
                );
                let mut super_fn_impl = self.generate_cpp_function_inner(
                    &super_method,
                    true,
                    ConversionDirection::CppCallsCpp,
                    false,
                    None,
                )?;
                method_decls.push(super_fn_impl.declaration.take().unwrap());
                self.additional_functions.push(super_fn_impl);
            }
        }
        // In future, for each superclass..
        let super_name = superclass.get_final_item();
        method_decls.push(format!(
            "const {}& As_{}() const {{ return *this; }}",
            super_name, super_name,
        ));
        method_decls.push(format!(
            "{}& As_{}_mut() {{ return *this; }}",
            super_name, super_name
        ));
        self.additional_functions.push(ExtraCpp {
            declaration: Some(format!(
                "inline std::unique_ptr<{}> {}_As_{}_UniquePtr(std::unique_ptr<{}> u) {{ return std::unique_ptr<{}>(u.release()); }}",
                superclass.to_cpp_name(), subclass.cpp(), super_name, subclass.cpp(), superclass.to_cpp_name(),
                )),
                ..Default::default()
        });
        // And now constructors
        let mut constructor_decls: Vec<String> = Vec::new();
        for constructor in constructors {
            let mut fn_impl = self.generate_cpp_function_inner(
                constructor,
                false,
                ConversionDirection::CppCallsCpp,
                false,
                None,
            )?;
            let decl = fn_impl.declaration.take().unwrap();
            constructor_decls.push(decl);
            self.additional_functions.push(fn_impl);
        }
        self.additional_functions.push(ExtraCpp {
            type_definition: Some(format!(
                "class {} : public {}\n{{\npublic:\n{}\n{}\nvoid {}() const;\nprivate:rust::Box<{}> obs;\nvoid really_remove_ownership();\n\n}};",
                subclass.cpp(),
                superclass.to_cpp_name(),
                constructor_decls.join("\n"),
                method_decls.join("\n"),
                subclass.cpp_remove_ownership(),
                holder
            )),
            definition: Some(format!(
                "void {}::{}() const {{\nconst_cast<{}*>(this)->really_remove_ownership();\n}}\nvoid {}::really_remove_ownership() {{\nauto new_obs = {}(std::move(obs));\nobs = std::move(new_obs);\n}}\n",
                subclass.cpp(),
                subclass.cpp_remove_ownership(),
                subclass.cpp(),
                subclass.cpp(),
                subclass.remove_ownership()
            )),
            cpp_headers: vec![Header::CxxgenH],
            ..Default::default()
        });
        Ok(())
    }
}
