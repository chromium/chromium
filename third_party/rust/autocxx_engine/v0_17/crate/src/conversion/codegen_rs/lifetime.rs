// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
use crate::{
    conversion::analysis::fun::{ArgumentAnalysis, ReceiverMutability},
    types::QualifiedName,
};
use proc_macro2::TokenStream;
use quote::quote;
use std::{borrow::Cow, collections::HashSet};
use syn::{
    parse_quote, punctuated::Punctuated, token::Comma, FnArg, GenericArgument, PatType, Path,
    PathSegment, ReturnType, Type, TypePath, TypeReference,
};

/// Function which can add explicit lifetime parameters to function signatures
/// where necessary, based on analysis of parameters and return types.
/// This is necessary only in two cases:
/// 1) where the parameter is a Pin<&mut T>
///    and the return type is some kind of reference - because lifetime elision
///    is not smart enough to see inside a Pin.
/// 2) as a workaround for https://github.com/dtolnay/cxx/issues/1024, where the
///    input parameter is a non-POD type but the output reference is a POD or
///    built-in type
pub(crate) fn add_explicit_lifetime_if_necessary<'r>(
    param_details: &[ArgumentAnalysis],
    mut params: Punctuated<FnArg, Comma>,
    ret_type: &'r ReturnType,
    non_pod_types: &HashSet<QualifiedName>,
) -> (
    Option<TokenStream>,
    Punctuated<FnArg, Comma>,
    Cow<'r, ReturnType>,
) {
    let has_mutable_receiver = param_details
        .iter()
        .any(|pd| matches!(pd.self_type, Some((_, ReceiverMutability::Mutable))));

    let non_pod_ref_param = reference_parameter_is_non_pod_reference(&params, non_pod_types);
    let ret_type_pod = return_type_is_pod_or_known_type_reference(ret_type, non_pod_types);
    let hits_1024_bug = non_pod_ref_param && ret_type_pod;
    if !(has_mutable_receiver || hits_1024_bug) {
        return (None, params, Cow::Borrowed(ret_type));
    }
    let new_return_type = match ret_type {
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
            _ => None,
        },
        _ => None,
    };

    match new_return_type {
        None => (None, params, Cow::Borrowed(ret_type)),
        Some(new_return_type) => {
            for mut param in params.iter_mut() {
                match &mut param {
                    FnArg::Typed(PatType { ty, .. }) => match ty.as_mut() {
                        Type::Path(TypePath {
                            path: Path { segments, .. },
                            ..
                        }) => add_lifetime_to_pinned_reference(segments).unwrap(),
                        Type::Reference(tyr) => add_lifetime_to_reference(tyr),
                        _ => panic!("Expected Pin<&mut T> or &T"),
                    },
                    _ => panic!("Unexpected fnarg"),
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

pub(crate) fn add_lifetime_to_all_reference_params(params: &mut Punctuated<FnArg, Comma>) {
    for mut param in params.iter_mut() {
        match &mut param {
            FnArg::Typed(PatType { ty, .. }) => match ty.as_mut() {
                Type::Path(TypePath {
                    path: Path { segments, .. },
                    ..
                }) => {
                    // This function will check whether this path is a pinned reference and if
                    // so, add a lifetime to it. Otherwise, it will return an error - which
                    // we ignore.
                    add_lifetime_to_pinned_reference(segments).unwrap_or_default()
                }
                Type::Reference(tyr) => add_lifetime_to_reference(tyr),
                _ => {}
            },
            _ => panic!("Unexpected fnarg"),
        }
    }
}
