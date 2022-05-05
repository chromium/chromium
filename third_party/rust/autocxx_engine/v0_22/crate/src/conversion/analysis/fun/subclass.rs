// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use indexmap::map::IndexMap as HashMap;

use syn::{parse_quote, FnArg, PatType, Type, TypePtr};

use crate::conversion::analysis::fun::{FnKind, MethodKind, ReceiverMutability};
use crate::conversion::analysis::pod::PodPhase;
use crate::conversion::api::{
    CppVisibility, FuncToConvert, Provenance, RustSubclassFnDetails, SubclassConstructorDetails,
    SubclassName, SuperclassMethod, UnsafetyNeeded, Virtualness,
};
use crate::conversion::apivec::ApiVec;
use crate::{
    conversion::{
        analysis::fun::function_wrapper::{
            CppFunction, CppFunctionBody, CppFunctionKind, TypeConversionPolicy,
        },
        api::{Api, ApiName},
    },
    types::{make_ident, Namespace, QualifiedName},
};

use super::{FnAnalysis, FnPrePhase1};

pub(super) fn subclasses_by_superclass(
    apis: &ApiVec<PodPhase>,
) -> HashMap<QualifiedName, Vec<SubclassName>> {
    let mut subclasses_per_superclass: HashMap<QualifiedName, Vec<SubclassName>> = HashMap::new();

    for api in apis.iter() {
        if let Api::Subclass { name, superclass } = api {
            subclasses_per_superclass
                .entry(superclass.clone())
                .or_default()
                .push(name.clone());
        }
    }
    subclasses_per_superclass
}

pub(super) fn create_subclass_fn_wrapper(
    sub: &SubclassName,
    super_fn_name: &QualifiedName,
    fun: &FuncToConvert,
) -> Box<FuncToConvert> {
    let self_ty = Some(sub.cpp());
    Box::new(FuncToConvert {
        synthesized_this_type: self_ty.clone(),
        self_ty,
        ident: super_fn_name.get_final_ident(),
        doc_attrs: fun.doc_attrs.clone(),
        inputs: fun.inputs.clone(),
        output: fun.output.clone(),
        vis: fun.vis.clone(),
        virtualness: Virtualness::None,
        cpp_vis: CppVisibility::Public,
        special_member: None,
        unused_template_param: fun.unused_template_param,
        original_name: None,
        references: fun.references.clone(),
        add_to_trait: fun.add_to_trait.clone(),
        is_deleted: fun.is_deleted,
        synthetic_cpp: None,
        provenance: Provenance::SynthesizedOther,
        variadic: fun.variadic,
    })
}

pub(super) fn create_subclass_trait_item(
    name: ApiName,
    analysis: &FnAnalysis,
    receiver_mutability: &ReceiverMutability,
    receiver: QualifiedName,
    is_pure_virtual: bool,
) -> Api<FnPrePhase1> {
    let param_names = analysis
        .param_details
        .iter()
        .map(|pd| pd.name.clone())
        .collect();
    Api::SubclassTraitItem {
        name,
        details: SuperclassMethod {
            name: make_ident(&analysis.rust_name),
            params: analysis.params.clone(),
            ret_type: analysis.ret_type.clone(),
            param_names,
            receiver_mutability: receiver_mutability.clone(),
            requires_unsafe: UnsafetyNeeded::from_param_details(&analysis.param_details, false),
            is_pure_virtual,
            receiver,
        },
    }
}

pub(super) fn create_subclass_function(
    sub: &SubclassName,
    analysis: &super::FnAnalysis,
    name: &ApiName,
    receiver_mutability: &ReceiverMutability,
    superclass: &QualifiedName,
    dependencies: Vec<QualifiedName>,
) -> Api<FnPrePhase1> {
    let cpp = sub.cpp();
    let holder_name = sub.holder();
    let rust_call_name = make_ident(format!(
        "{}_{}",
        sub.0.name.get_final_item(),
        name.name.get_final_item()
    ));
    let params = std::iter::once(parse_quote! {
        me: & #holder_name
    })
    .chain(analysis.params.iter().skip(1).cloned())
    .collect();
    let kind = if matches!(receiver_mutability, ReceiverMutability::Mutable) {
        CppFunctionKind::Method
    } else {
        CppFunctionKind::ConstMethod
    };
    let argument_conversion = analysis
        .param_details
        .iter()
        .skip(1)
        .map(|p| p.conversion.clone())
        .collect();
    Api::RustSubclassFn {
        name: ApiName::new_in_root_namespace(rust_call_name.clone()),
        subclass: sub.clone(),
        details: Box::new(RustSubclassFnDetails {
            params,
            ret: analysis.ret_type.clone(),
            method_name: make_ident(&analysis.rust_name),
            cpp_impl: CppFunction {
                payload: CppFunctionBody::FunctionCall(Namespace::new(), rust_call_name),
                wrapper_function_name: make_ident(&analysis.rust_name),
                original_cpp_name: name.cpp_name(),
                return_conversion: analysis.ret_conversion.clone(),
                argument_conversion,
                kind,
                pass_obs_field: true,
                qualification: Some(cpp),
            },
            superclass: superclass.clone(),
            receiver_mutability: receiver_mutability.clone(),
            dependencies,
            requires_unsafe: UnsafetyNeeded::from_param_details(&analysis.param_details, false),
            is_pure_virtual: matches!(
                analysis.kind,
                FnKind::Method {
                    method_kind: MethodKind::PureVirtual(..),
                    ..
                }
            ),
        }),
    }
}

pub(super) fn create_subclass_constructor(
    sub: SubclassName,
    analysis: &FnAnalysis,
    sup: &QualifiedName,
    fun: &FuncToConvert,
) -> (Box<FuncToConvert>, ApiName) {
    let holder = sub.holder();
    let cpp = sub.cpp();
    let wrapper_function_name = cpp.get_final_ident();
    let initial_arg = TypeConversionPolicy::new_unconverted(parse_quote! {
        rust::Box< #holder >
    });
    let args = std::iter::once(initial_arg).chain(
        analysis
            .param_details
            .iter()
            .skip(1) // skip placement new destination
            .map(|aa| aa.conversion.clone()),
    );
    let cpp_impl = CppFunction {
        payload: CppFunctionBody::ConstructSuperclass(sup.to_cpp_name()),
        wrapper_function_name,
        return_conversion: None,
        argument_conversion: args.collect(),
        kind: CppFunctionKind::SynthesizedConstructor,
        pass_obs_field: false,
        qualification: Some(cpp.clone()),
        original_cpp_name: cpp.to_cpp_name(),
    };
    let subclass_constructor_details = Box::new(SubclassConstructorDetails {
        subclass: sub.clone(),
        is_trivial: analysis.param_details.len() == 1, // just placement new
        // destination, no other parameters
        cpp_impl,
    });
    let subclass_constructor_name =
        make_ident(format!("{}_{}", cpp.get_final_item(), cpp.get_final_item()));
    let mut existing_params = fun.inputs.clone();
    if let Some(FnArg::Typed(PatType { ty, .. })) = existing_params.first_mut() {
        if let Type::Ptr(TypePtr { elem, .. }) = &mut **ty {
            *elem = Box::new(Type::Path(sub.cpp().to_type_path()));
        } else {
            panic!("Unexpected self type parameter when creating subclass constructor");
        }
    } else {
        panic!("Unexpected self type parameter when creating subclass constructor");
    }
    let mut existing_params = existing_params.into_iter();
    let self_param = existing_params.next();
    let boxed_holder_param: FnArg = parse_quote! {
        peer: rust::Box<#holder>
    };
    let inputs = self_param
        .into_iter()
        .chain(std::iter::once(boxed_holder_param))
        .chain(existing_params)
        .collect();
    let maybe_wrap = Box::new(FuncToConvert {
        ident: subclass_constructor_name.clone(),
        doc_attrs: fun.doc_attrs.clone(),
        inputs,
        output: fun.output.clone(),
        vis: fun.vis.clone(),
        virtualness: Virtualness::None,
        cpp_vis: CppVisibility::Public,
        special_member: fun.special_member.clone(),
        original_name: None,
        unused_template_param: fun.unused_template_param,
        references: fun.references.clone(),
        synthesized_this_type: Some(cpp.clone()),
        self_ty: Some(cpp),
        add_to_trait: None,
        is_deleted: fun.is_deleted,
        synthetic_cpp: None,
        provenance: Provenance::SynthesizedSubclassConstructor(subclass_constructor_details),
        variadic: fun.variadic,
    });
    let subclass_constructor_name = ApiName::new_with_cpp_name(
        &Namespace::new(),
        subclass_constructor_name,
        Some(sub.cpp().get_final_item().to_string()),
    );
    (maybe_wrap, subclass_constructor_name)
}
