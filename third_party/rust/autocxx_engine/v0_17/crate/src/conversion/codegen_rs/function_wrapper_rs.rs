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

impl TypeConversionPolicy {
    pub(super) fn rust_wrapper_unconverted_type(&self) -> Type {
        match self.rust_conversion {
            RustConversionType::None => self.converted_rust_type(),
            RustConversionType::ToBoxedUpHolder(ref sub) => {
                let id = sub.id();
                parse_quote! { autocxx::subclass::CppSubclassRustPeerHolder<
                    super::super::super:: #id>
                }
            }
            RustConversionType::FromStr => parse_quote! { impl ToCppString },
            RustConversionType::FromPinMaybeUninitToPtr => {
                let ty = match &self.unwrapped_type {
                    Type::Ptr(TypePtr { elem, .. }) => &*elem,
                    _ => panic!("Not a ptr"),
                };
                parse_quote! {
                    ::std::pin::Pin<&mut ::std::mem::MaybeUninit< #ty >>
                }
            }
            RustConversionType::FromPinMoveRefToPtr => {
                let ty = match &self.unwrapped_type {
                    Type::Ptr(TypePtr { elem, .. }) => &*elem,
                    _ => panic!("Not a ptr"),
                };
                parse_quote! {
                    ::std::pin::Pin<autocxx::moveit::MoveRef< '_, #ty >>
                }
            }
            RustConversionType::FromTypeToPtr => {
                let ty = match &self.unwrapped_type {
                    Type::Ptr(TypePtr { elem, .. }) => &*elem,
                    _ => panic!("Not a ptr"),
                };
                parse_quote! { &mut #ty }
            }
            RustConversionType::FromValueParamToPtr => {
                let ty = &self.unwrapped_type;
                parse_quote! { impl autocxx::ValueParam<#ty> }
            }
        }
    }

    pub(super) fn rust_conversion(
        &self,
        var: Pat,
        wrap_in_unsafe: bool,
    ) -> (Option<TokenStream>, TokenStream) {
        match self.rust_conversion {
            RustConversionType::None => (None, quote! { #var }),
            RustConversionType::FromStr => (None, quote! ( #var .into_cpp() )),
            RustConversionType::ToBoxedUpHolder(ref sub) => {
                let holder_type = sub.holder();
                (
                    None,
                    quote! {
                        Box::new(#holder_type(#var))
                    },
                )
            }
            RustConversionType::FromPinMaybeUninitToPtr => (
                None,
                quote! {
                    #var.get_unchecked_mut().as_mut_ptr()
                },
            ),
            RustConversionType::FromPinMoveRefToPtr => (
                None,
                quote! {
                    { let r: &mut _ = ::std::pin::Pin::into_inner_unchecked(#var.as_mut());
                    r
                    }
                },
            ),
            RustConversionType::FromTypeToPtr => (
                None,
                quote! {
                    #var
                },
            ),
            RustConversionType::FromValueParamToPtr => {
                let var_name = if let Pat::Ident(pti) = &var {
                    &pti.ident
                } else {
                    panic!("Unexpected non-ident parameter name");
                };
                let space_var_name = make_ident(format!("{}_space", var_name));
                let call = quote! { #space_var_name.as_mut().populate(#var_name);  };
                let call = if wrap_in_unsafe {
                    quote! {
                        unsafe {
                            #call
                        }
                    }
                } else {
                    call
                };
                // This is the usual trick to put something on the stack, then
                // immediately shadow the variable name so it can't be accessed or moved.
                (
                    Some(quote! {
                        let mut #space_var_name = autocxx::ValueParamHandler::default();
                        let mut #space_var_name = unsafe {
                            std::pin::Pin::new_unchecked(&mut #space_var_name)
                        };
                        #call
                    }),
                    quote! {
                        #space_var_name.get_ptr()
                    },
                )
            }
        }
    }
}
