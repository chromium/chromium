// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
use crate::{
    conversion::analysis::fun::{
        function_wrapper::RustConversionType, ArgumentAnalysis, ReceiverMutability,
    },
    types::QualifiedName,
};
use indexmap::set::IndexSet as HashSet;
use proc_macro2::TokenStream;
use quote::{quote, ToTokens};
use std::borrow::Cow;
use syn::{
    parse_quote, punctuated::Punctuated, token::Comma, FnArg, GenericArgument, PatType, Path,
    PathSegment, ReturnType, Type, TypePath, TypeReference,
};

/// Function which can add explicit lifetime parameters to function signatures
/// where necessary, based on analysis of parameters and return types.
/// This is necessary in three cases:
/// 1) where the parameter is a Pin<&mut T>
///    and the return type is some kind of reference - because lifetime elision
///    is not smart enough to see inside a Pin.
/// 2) as a workaround for https://github.com/dtolnay/cxx/issues/1024, where the
///    input parameter is a non-POD type but the output reference is a POD or
///    built-in type
/// 3) Any parameter is any form of reference, and we're returning an `impl New`
///    3a) an 'impl ValueParam' counts as a reference.
pub(crate) fn add_explicit_lifetime_if_necessary<'r>(
    param_details: &[ArgumentAnalysis],
    mut params: Punctuated<FnArg, Comma>,
    ret_type: Cow<'r, ReturnType>,
    non_pod_types: &HashSet<QualifiedName>,
) -> (
    Option<TokenStream>,
    Punctuated<FnArg, Comma>,
    Cow<'r, ReturnType>,
) {
    let has_mutable_receiver = param_details.iter().any(|pd| {
        matches!(pd.self_type, Some((_, ReceiverMutability::Mutable)))
            && !pd.is_placement_return_destination
    });

    let any_param_is_reference = param_details.iter().any(|pd| {
        pd.has_lifetime
            || matches!(
                pd.conversion.rust_conversion,
                RustConversionType::FromValueParamToPtr
            )
    });
    let return_type_is_impl = return_type_is_impl(&ret_type);
    let non_pod_ref_param = reference_parameter_is_non_pod_reference(&params, non_pod_types);
    let ret_type_pod = return_type_is_pod_or_known_type_reference(&ret_type, non_pod_types);
    let returning_impl_with_a_reference_param = return_type_is_impl && any_param_is_reference;
    let hits_1024_bug = non_pod_ref_param && ret_type_pod;
    if !(has_mutable_receiver || hits_1024_bug || returning_impl_with_a_reference_param) {
        return (None, params, ret_type);
    }
    let new_return_type = match ret_type.as_ref() {
        ReturnType::Type(rarrow, boxed_type) => match boxed_type.as_ref() {
            Type::Reference(rtr) => {
                let mut new_rtr = rtr.clone();
                new_rtr.lifetime = Some(parse_quote! { 'a });
                Some(ReturnType::Type(
                    *rarrow,
                    Box::new(Type::Reference(new_rtr)),
                ))
            }
            Type::Path(typ) => {
                let mut new_path = typ.clone();
                add_lifetime_to_pinned_reference(&mut new_path.path.segments)
                    .ok()
                    .map(|_| ReturnType::Type(*rarrow, Box::new(Type::Path(new_path))))
            }
            Type::ImplTrait(tyit) => {
                let old_tyit = tyit.to_token_stream();
                Some(parse_quote! {
                    #rarrow #old_tyit + 'a
                })
            }
            _ => None,
        },
        _ => None,
    };

    match new_return_type {
        None => (None, params, ret_type),
        Some(new_return_type) => {
            for mut param in params.iter_mut() {
                if let FnArg::Typed(PatType { ty, .. }) = &mut param {
                    match ty.as_mut() {
                        Type::Path(TypePath {
                            path: Path { segments, .. },
                            ..
                        }) => add_lifetime_to_pinned_reference(segments).unwrap_or(()),
                        Type::Reference(tyr) => add_lifetime_to_reference(tyr),
                        Type::ImplTrait(tyit) => add_lifetime_to_impl_trait(tyit),
                        _ => {}
                    }
                }
            }

            (Some(quote! { <'a> }), params, Cow::Owned(new_return_type))
        }
    }
}

fn reference_parameter_is_non_pod_reference(
    params: &Punctuated<FnArg, Comma>,
    non_pod_types: &HashSet<QualifiedName>,
) -> bool {
    params.iter().any(|param| match param {
        FnArg::Typed(PatType { ty, .. }) => match ty.as_ref() {
            Type::Reference(TypeReference { elem, .. }) => match elem.as_ref() {
                Type::Path(typ) => {
                    let qn = QualifiedName::from_type_path(typ);
                    non_pod_types.contains(&qn)
                }
                _ => false,
            },
            _ => false,
        },
        _ => false,
    })
}

fn return_type_is_pod_or_known_type_reference(
    ret_type: &ReturnType,
    non_pod_types: &HashSet<QualifiedName>,
) -> bool {
    match ret_type {
        ReturnType::Type(_, boxed_type) => match boxed_type.as_ref() {
            Type::Reference(rtr) => match rtr.elem.as_ref() {
                Type::Path(typ) => {
                    let qn = QualifiedName::from_type_path(typ);
                    !non_pod_types.contains(&qn)
                }
                _ => false,
            },
            _ => false,
        },
        _ => false,
    }
}

fn return_type_is_impl(ret_type: &ReturnType) -> bool {
    matches!(ret_type, ReturnType::Type(_, boxed_type) if matches!(boxed_type.as_ref(), Type::ImplTrait(..)))
}

#[derive(Debug)]
enum AddLifetimeError {
    WasNotPin,
}

fn add_lifetime_to_pinned_reference(
    segments: &mut Punctuated<PathSegment, syn::token::Colon2>,
) -> Result<(), AddLifetimeError> {
    static EXPECTED_SEGMENTS: &[(&str, bool)] = &[
        ("std", false),
        ("pin", false),
        ("Pin", true), // true = act on the arguments of this segment
    ];

    for (seg, (expected_name, act)) in segments.iter_mut().zip(EXPECTED_SEGMENTS.iter()) {
        if seg.ident != expected_name {
            return Err(AddLifetimeError::WasNotPin);
        }
        if *act {
            match &mut seg.arguments {
                syn::PathArguments::AngleBracketed(aba) => match aba.args.iter_mut().next() {
                    Some(GenericArgument::Type(Type::Reference(tyr))) => {
                        add_lifetime_to_reference(tyr);
                    }
                    _ => panic!("Expected generic args with a reference"),
                },
                _ => panic!("Expected angle bracketed args"),
            }
        }
    }
    Ok(())
}

fn add_lifetime_to_reference(tyr: &mut syn::TypeReference) {
    tyr.lifetime = Some(parse_quote! { 'a })
}

fn add_lifetime_to_impl_trait(tyit: &mut syn::TypeImplTrait) {
    tyit.bounds
        .push(syn::TypeParamBound::Lifetime(parse_quote! { 'a }))
}
