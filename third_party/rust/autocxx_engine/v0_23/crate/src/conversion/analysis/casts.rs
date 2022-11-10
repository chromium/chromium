// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use itertools::Itertools;
use quote::quote;
use syn::{parse_quote, FnArg};

use crate::{
    conversion::{
        api::{
            Api, ApiName, CastMutability, DeletedOrDefaulted, Provenance, References,
            TraitSynthesis,
        },
        apivec::ApiVec,
    },
    types::{make_ident, QualifiedName},
};

/// If A is a base of B, we might want to be able to cast from
/// &B to &A, or from Pin<&mut A> to &B, or from Pin<&mut A> to &B.
/// The first is OK; the others turn out to be hard due to all
/// the Pin stuff. For now therefore, we simply don't allow them.
/// But the related code may be useful in future so I'm keeping it around.
const SUPPORT_MUTABLE_CASTS: bool = false;

use super::{
    fun::function_wrapper::{CppFunctionBody, CppFunctionKind},
    pod::{PodAnalysis, PodPhase},
};

pub(crate) fn add_casts(apis: ApiVec<PodPhase>) -> ApiVec<PodPhase> {
    apis.into_iter()
        .flat_map(|api| {
            let mut resultant_apis = match api {
                Api::Struct {
                    ref name,
                    details: _,
                    ref analysis,
                } => create_casts(&name.name, analysis).collect_vec(),
                _ => Vec::new(),
            };
            resultant_apis.push(api);
            resultant_apis.into_iter()
        })
        .collect()
}

fn create_casts<'a>(
    name: &'a QualifiedName,
    analysis: &'a PodAnalysis,
) -> impl Iterator<Item = Api<PodPhase>> + 'a {
    // Create casts only to base classes which are on the allowlist
    // because otherwise we won't know for sure whether they're abstract or not.
    analysis
        .castable_bases
        .iter()
        .flat_map(move |base| cast_types().map(|mutable| create_cast(name, base, mutable)))
}

/// Iterate through the types of cast we should make.
fn cast_types() -> impl Iterator<Item = CastMutability> {
    if SUPPORT_MUTABLE_CASTS {
        vec![
            CastMutability::ConstToConst,
            CastMutability::MutToConst,
            CastMutability::MutToMut,
        ]
        .into_iter()
    } else {
        vec![CastMutability::ConstToConst].into_iter()
    }
}

fn create_cast(from: &QualifiedName, to: &QualifiedName, mutable: CastMutability) -> Api<PodPhase> {
    let name = name_for_cast(from, to, mutable);
    let ident = name.get_final_ident();
    let from_typ = from.to_type_path();
    let to_typ = to.to_type_path();
    let return_mutability = match mutable {
        CastMutability::ConstToConst | CastMutability::MutToConst => quote! { const },
        CastMutability::MutToMut => quote! { mut },
    };
    let param_mutability = match mutable {
        CastMutability::ConstToConst => quote! { const },
        CastMutability::MutToConst | CastMutability::MutToMut => quote! { mut },
    };
    let fnarg: FnArg = parse_quote! {
        this: * #param_mutability #from_typ
    };
    Api::Function {
        name: ApiName::new_from_qualified_name(name),
        fun: Box::new(crate::conversion::api::FuncToConvert {
            ident,
            doc_attrs: Vec::new(),
            inputs: [fnarg].into_iter().collect(),
            output: parse_quote! {
                -> * #return_mutability #to_typ
            },
            vis: parse_quote! { pub },
            virtualness: crate::conversion::api::Virtualness::None,
            cpp_vis: crate::conversion::api::CppVisibility::Public,
            special_member: None,
            unused_template_param: false,
            references: References::new_with_this_and_return_as_reference(),
            original_name: None,
            self_ty: Some(from.clone()),
            synthesized_this_type: None,
            add_to_trait: Some(TraitSynthesis::Cast {
                to_type: to.clone(),
                mutable,
            }),
            synthetic_cpp: Some((CppFunctionBody::Cast, CppFunctionKind::Function)),
            is_deleted: DeletedOrDefaulted::Neither,
            provenance: Provenance::SynthesizedOther,
            variadic: false,
        }),
        analysis: (),
    }
}

fn name_for_cast(
    from: &QualifiedName,
    to: &QualifiedName,
    mutable: CastMutability,
) -> QualifiedName {
    let suffix = match mutable {
        CastMutability::ConstToConst => "",
        CastMutability::MutToConst => "_to_const",
        CastMutability::MutToMut => "_mut",
    };
    let name = format!(
        "cast_{}_to_{}{}",
        from.get_final_item(),
        to.get_final_item(),
        suffix
    );
    let name = make_ident(name);
    QualifiedName::new(from.get_namespace(), name)
}
