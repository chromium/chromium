// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use syn::{
    parse_quote, punctuated::Punctuated, FnArg, GenericArgument, PathArguments, PathSegment,
    ReturnType, Token, Type, TypePath,
};

/// Mod to handle stripping paths off the front of types.

fn unqualify_type_path(typ: TypePath) -> TypePath {
    // If we've still got more than one
    // path segment then this is referring to a type within
    // C++ namespaces. Strip them off for now, until cxx supports
    // nested mods within a cxx::bridge.
    // This is 'safe' because earlier code will already have
    // failed with 'DuplicateType' if we had several types called
    // the same thing.
    let last_seg = typ.path.segments.into_iter().last().unwrap();
    let ident = &last_seg.ident;
    let args = match last_seg.arguments {
        PathArguments::AngleBracketed(mut ab) => {
            ab.args = unqualify_punctuated(ab.args);
            PathArguments::AngleBracketed(ab)
        }
        _ => last_seg.arguments.clone(),
    };
    let last_seg: PathSegment = parse_quote!( #ident #args );
    parse_quote!(
        #last_seg
    )
}

fn unqualify_punctuated<P>(pun: Punctuated<GenericArgument, P>) -> Punctuated<GenericArgument, P>
where
    P: Default,
{
    let mut new_pun = Punctuated::new();
    for arg in pun.into_iter() {
        new_pun.push(match arg {
            GenericArgument::Type(t) => GenericArgument::Type(unqualify_type(t)),
            _ => arg,
        });
    }
    new_pun
}

fn unqualify_type(typ: Type) -> Type {
    match typ {
        Type::Path(typ) => Type::Path(unqualify_type_path(typ)),
        Type::Reference(mut typeref) => {
            typeref.elem = unqualify_boxed_type(typeref.elem);
            Type::Reference(typeref)
        }
        Type::Ptr(mut typeptr) => {
            typeptr.elem = unqualify_boxed_type(typeptr.elem);
            Type::Ptr(typeptr)
        }
        _ => typ,
    }
}

fn unqualify_boxed_type(typ: Box<Type>) -> Box<Type> {
    Box::new(unqualify_type(*typ))
}

pub(crate) fn unqualify_ret_type(ret_type: ReturnType) -> ReturnType {
    match ret_type {
        ReturnType::Type(tok, boxed_type) => {
            ReturnType::Type(tok, unqualify_boxed_type(boxed_type))
        }
        _ => ret_type,
    }
}

pub(crate) fn unqualify_params(
    params: Punctuated<FnArg, Token![,]>,
) -> Punctuated<FnArg, Token![,]> {
    params
        .into_iter()
        .map(|p| match p {
            FnArg::Typed(mut pt) => {
                pt.ty = unqualify_boxed_type(pt.ty);
                FnArg::Typed(pt)
            }
            _ => p,
        })
        .collect()
}
