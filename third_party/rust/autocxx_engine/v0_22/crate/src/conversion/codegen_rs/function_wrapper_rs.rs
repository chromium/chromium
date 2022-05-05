// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use proc_macro2::TokenStream;
use syn::{Pat, Type, TypePtr};

use crate::{
    conversion::analysis::fun::function_wrapper::{RustConversionType, TypeConversionPolicy},
    types::make_ident,
};
use quote::quote;
use syn::parse_quote;

use super::MaybeUnsafeStmt;

/// Output Rust snippets for how to deal with a given parameter.
pub(super) enum RustParamConversion {
    Param {
        ty: Type,
        local_variables: Vec<MaybeUnsafeStmt>,
        conversion: TokenStream,
        conversion_requires_unsafe: bool,
    },
    ReturnValue {
        ty: Type,
    },
}

impl TypeConversionPolicy {
    /// If returns `None` then this parameter should be omitted entirely.
    pub(super) fn rust_conversion(&self, var: Pat) -> RustParamConversion {
        match self.rust_conversion {
            RustConversionType::None => RustParamConversion::Param {
                ty: self.converted_rust_type(),
                local_variables: Vec::new(),
                conversion: quote! { #var },
                conversion_requires_unsafe: false,
            },
            RustConversionType::FromStr => RustParamConversion::Param {
                ty: parse_quote! { impl ToCppString },
                local_variables: Vec::new(),
                conversion: quote! ( #var .into_cpp() ),
                conversion_requires_unsafe: false,
            },
            RustConversionType::ToBoxedUpHolder(ref sub) => {
                let holder_type = sub.holder();
                let id = sub.id();
                let ty = parse_quote! { autocxx::subclass::CppSubclassRustPeerHolder<
                    super::super::super:: #id>
                };
                RustParamConversion::Param {
                    ty,
                    local_variables: Vec::new(),
                    conversion: quote! {
                        Box::new(#holder_type(#var))
                    },
                    conversion_requires_unsafe: false,
                }
            }
            RustConversionType::FromPinMaybeUninitToPtr => {
                let ty = match &self.unwrapped_type {
                    Type::Ptr(TypePtr { elem, .. }) => &*elem,
                    _ => panic!("Not a ptr"),
                };
                let ty = parse_quote! {
                    ::std::pin::Pin<&mut ::std::mem::MaybeUninit< #ty >>
                };
                RustParamConversion::Param {
                    ty,
                    local_variables: Vec::new(),
                    conversion: quote! {
                        #var.get_unchecked_mut().as_mut_ptr()
                    },
                    conversion_requires_unsafe: true,
                }
            }
            RustConversionType::FromPinMoveRefToPtr => {
                let ty = match &self.unwrapped_type {
                    Type::Ptr(TypePtr { elem, .. }) => &*elem,
                    _ => panic!("Not a ptr"),
                };
                let ty = parse_quote! {
                    ::std::pin::Pin<autocxx::moveit::MoveRef< '_, #ty >>
                };
                RustParamConversion::Param {
                    ty,
                    local_variables: Vec::new(),
                    conversion: quote! {
                        { let r: &mut _ = ::std::pin::Pin::into_inner_unchecked(#var.as_mut());
                            r
                        }
                    },
                    conversion_requires_unsafe: true,
                }
            }
            RustConversionType::FromTypeToPtr => {
                let ty = match &self.unwrapped_type {
                    Type::Ptr(TypePtr { elem, .. }) => &*elem,
                    _ => panic!("Not a ptr"),
                };
                let ty = parse_quote! { &mut #ty };
                RustParamConversion::Param {
                    ty,
                    local_variables: Vec::new(),
                    conversion: quote! {
                        #var
                    },
                    conversion_requires_unsafe: false,
                }
            }
            RustConversionType::FromValueParamToPtr | RustConversionType::FromRValueParamToPtr => {
                let (handler_type, param_trait) = match self.rust_conversion {
                    RustConversionType::FromValueParamToPtr => ("ValueParamHandler", "ValueParam"),
                    RustConversionType::FromRValueParamToPtr => {
                        ("RValueParamHandler", "RValueParam")
                    }
                    _ => unreachable!(),
                };
                let handler_type = make_ident(handler_type);
                let param_trait = make_ident(param_trait);
                let var_name = if let Pat::Ident(pti) = &var {
                    &pti.ident
                } else {
                    panic!("Unexpected non-ident parameter name");
                };
                let space_var_name = make_ident(format!("{}_space", var_name));
                let ty = &self.unwrapped_type;
                let ty = parse_quote! { impl autocxx::#param_trait<#ty> };
                // This is the usual trick to put something on the stack, then
                // immediately shadow the variable name so it can't be accessed or moved.
                RustParamConversion::Param {
                    ty,
                    local_variables: vec![
                        MaybeUnsafeStmt::new(
                            quote! { let mut #space_var_name = autocxx::#handler_type::default(); },
                        ),
                        MaybeUnsafeStmt::binary(
                            quote! { let mut #space_var_name =
                                unsafe { ::std::pin::Pin::new_unchecked(&mut #space_var_name) };
                            },
                            quote! { let mut #space_var_name =
                                ::std::pin::Pin::new_unchecked(&mut #space_var_name);
                            },
                        ),
                        MaybeUnsafeStmt::needs_unsafe(
                            quote! { #space_var_name.as_mut().populate(#var_name); },
                        ),
                    ],
                    conversion: quote! {
                        #space_var_name.get_ptr()
                    },
                    conversion_requires_unsafe: false,
                }
            }
            // This type of conversion means that this function parameter appears in the cxx::bridge
            // but not in the arguments for the wrapper function, because instead we return an
            // impl New which uses the cxx::bridge function's pointer parameter.
            RustConversionType::FromPlacementParamToNewReturn => {
                let ty = match &self.unwrapped_type {
                    Type::Ptr(TypePtr { elem, .. }) => *(*elem).clone(),
                    _ => panic!("Not a ptr"),
                };
                RustParamConversion::ReturnValue { ty }
            }
        }
    }
}
