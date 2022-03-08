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
use crate::conversion::analysis::fun::{ArgumentAnalysis, ReceiverMutability};
use proc_macro2::TokenStream;
use quote::quote;
use std::borrow::Cow;
use syn::{
    parse_quote, punctuated::Punctuated, token::Comma, FnArg, GenericArgument, PatType, Path,
    PathSegment, ReturnType, Type, TypePath,
};

/// Function which can add explicit lifetime parameters to function signatures
/// where necessary, based on analysis of parameters and return types.
/// This is necessary only in one case - where the parameter is a Pin<&mut T>
/// and the return type is some kind of reference - because lifetime elision
/// is not smart enough to see inside a Pin.
pub(crate) fn add_explicit_lifetime_if_necessary<'r>(
    param_details: &[ArgumentAnalysis],
    mut params: Punctuated<FnArg, Comma>,
    ret_type: &'r ReturnType,
) -> (
    Option<TokenStream>,
    Punctuated<FnArg, Comma>,
    Cow<'r, ReturnType>,
) {
    let has_mutable_receiver = param_details
        .iter()
        .any(|pd| matches!(pd.self_type, Some((_, ReceiverMutability::Mutable))));
    if !has_mutable_receiver {
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
                        _ => panic!("Expected Pin<T>"),
                    },
                    _ => panic!("Unexpected fnarg"),
                }
            }

            (Some(quote! { <'a> }), params, Cow::Owned(new_return_type))
        }
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

pub(crate) fn add_lifetime_to_all_params(params: &mut Punctuated<FnArg, Comma>) {
    for mut param in params.iter_mut() {
        match &mut param {
            FnArg::Typed(PatType { ty, .. }) => match ty.as_mut() {
                Type::Path(TypePath {
                    path: Path { segments, .. },
                    ..
                }) => add_lifetime_to_pinned_reference(segments).unwrap(),
                Type::Reference(tyr) => add_lifetime_to_reference(tyr),
                _ => {}
            },
            _ => panic!("Unexpected fnarg"),
        }
    }
}
