// Copyright 2020 Google LLC
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

use proc_macro2::TokenStream;
use syn::{Pat, Type, TypePtr};

use crate::conversion::analysis::fun::function_wrapper::{
    RustConversionType, TypeConversionPolicy,
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
        }
    }

    pub(super) fn rust_conversion(&self, var: Pat) -> TokenStream {
        match self.rust_conversion {
            RustConversionType::None => quote! { #var },
            RustConversionType::FromStr => quote! ( #var .into_cpp() ),
            RustConversionType::ToBoxedUpHolder(ref sub) => {
                let holder_type = sub.holder();
                quote! {
                    Box::new(#holder_type(#var))
                }
            }
            RustConversionType::FromPinMaybeUninitToPtr => quote! {
                #var.get_unchecked_mut().as_mut_ptr()
            },
            RustConversionType::FromPinMoveRefToPtr => quote! {
                { let r: &mut _ = ::std::pin::Pin::into_inner_unchecked(#var.as_mut());
                r
                }
            },
            RustConversionType::FromTypeToPtr => quote! {
                #var
            },
        }
    }
}
