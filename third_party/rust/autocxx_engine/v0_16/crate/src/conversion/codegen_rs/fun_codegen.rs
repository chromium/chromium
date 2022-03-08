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
use quote::quote;
use syn::{
    parse::Parser,
    parse_quote,
    punctuated::Punctuated,
    token::{Comma, Unsafe},
    Attribute, FnArg, ForeignItem, Ident, ImplItem, Item, ReturnType,
};

use super::{
    unqualify::{unqualify_params, unqualify_ret_type},
    RsCodegenResult, Use,
};
use crate::{
    conversion::{
        analysis::fun::{
            ArgumentAnalysis, FnAnalysis, FnKind, MethodKind, RustRenameStrategy,
            TraitMethodDetails, UnsafetyNeeded,
        },
        api::ImplBlockDetails,
        codegen_rs::lifetime::add_lifetime_to_all_params,
    },
    types::{Namespace, QualifiedName},
};
use crate::{
    conversion::{api::FuncToConvert, codegen_rs::lifetime::add_explicit_lifetime_if_necessary},
    types::make_ident,
};

impl UnsafetyNeeded {
    fn bridge_token(&self) -> Option<Unsafe> {
        match self {
            UnsafetyNeeded::None => None,
            _ => Some(parse_quote! { unsafe }),
        }
    }

    fn wrapper_token(&self) -> Option<Unsafe> {
        match self {
            UnsafetyNeeded::Always => Some(parse_quote! { unsafe }),
            _ => None,
        }
    }
}

pub(super) fn gen_function(
    ns: &Namespace,
    fun: FuncToConvert,
    analysis: FnAnalysis,
    cpp_call_name: String,
) -> RsCodegenResult {
    if analysis.ignore_reason.is_err() || !analysis.externally_callable {
        return RsCodegenResult::default();
    }
    let cxxbridge_name = analysis.cxxbridge_name;
    let rust_name = analysis.rust_name;
    let ret_type = analysis.ret_type;
    let param_details = analysis.param_details;
    let wrapper_function_needed = analysis.cpp_wrapper.is_some();
    let params = analysis.params;
    let vis = analysis.vis;
    let kind = analysis.kind;
    let doc_attr = fun.doc_attr;

    let mut cpp_name_attr = Vec::new();
    let mut impl_entry = None;
    let mut trait_impl_entry = None;
    let rust_name_attr: Vec<_> = match &analysis.rust_rename_strategy {
        RustRenameStrategy::RenameUsingRustAttr => Attribute::parse_outer
            .parse2(quote!(
                #[rust_name = #rust_name]
            ))
            .unwrap(),
        _ => Vec::new(),
    };
    let wrapper_unsafety = analysis.requires_unsafe.wrapper_token();
    let fn_generator = FnGenerator {
        param_details: &param_details,
        cxxbridge_name: &cxxbridge_name,
        rust_name: &rust_name,
        unsafety: &wrapper_unsafety,
        doc_attr: &doc_attr,
    };
    let mut materialization = match kind {
        FnKind::Method(..) | FnKind::TraitMethod { .. } => None,
        FnKind::Function => match analysis.rust_rename_strategy {
            RustRenameStrategy::RenameInOutputMod(alias) => {
                Some(Use::UsedFromCxxBridgeWithAlias(alias))
            }
            _ => Some(Use::UsedFromCxxBridge),
        },
    };
    let any_param_needs_rust_conversion = param_details
        .iter()
        .any(|pd| pd.conversion.rust_work_needed());
    let rust_wrapper_needed = match kind {
        FnKind::TraitMethod { .. } => true,
        FnKind::Method(..) => any_param_needs_rust_conversion || cxxbridge_name != rust_name,
        _ => any_param_needs_rust_conversion,
    };
    if rust_wrapper_needed {
        match kind {
            FnKind::Method(ref type_name, MethodKind::Constructor) => {
                // Constructor.
                impl_entry = Some(fn_generator.generate_constructor_impl(type_name));
            }
            FnKind::Method(ref type_name, ref method_kind) => {
                // Method, or static method.
                impl_entry = Some(fn_generator.generate_method_impl(
                    matches!(
                        method_kind,
                        MethodKind::MakeUnique | MethodKind::Constructor
                    ),
                    type_name,
                    &ret_type,
                ));
            }
            FnKind::TraitMethod { ref details, .. } => {
                trait_impl_entry = Some(fn_generator.generate_trait_impl(details, &ret_type));
            }
            _ => {
                // Generate plain old function
                materialization = Some(Use::Custom(fn_generator.generate_function_impl(&ret_type)));
            }
        }
    }
    if cxxbridge_name != cpp_call_name && !wrapper_function_needed {
        cpp_name_attr = Attribute::parse_outer
            .parse2(quote!(
                #[cxx_name = #cpp_call_name]
            ))
            .unwrap();
    }
    // In very rare occasions, we might need to give an explicit lifetime.
    let (lifetime_tokens, params, ret_type) =
        add_explicit_lifetime_if_necessary(&param_details, params, &ret_type);

    // Finally - namespace support. All the Types in everything
    // above this point are fully qualified. We need to unqualify them.
    // We need to do that _after_ the above wrapper_function_needed
    // work, because it relies upon spotting fully qualified names like
    // std::unique_ptr. However, after it's done its job, all such
    // well-known types should be unqualified already (e.g. just UniquePtr)
    // and the following code will act to unqualify only those types
    // which the user has declared.
    let params = unqualify_params(params);
    let ret_type = unqualify_ret_type(ret_type.into_owned());
    // And we need to make an attribute for the namespace that the function
    // itself is in.
    let namespace_attr = if ns.is_empty() || wrapper_function_needed {
        Vec::new()
    } else {
        let namespace_string = ns.to_string();
        Attribute::parse_outer
            .parse2(quote!(
                #[namespace = #namespace_string]
            ))
            .unwrap()
    };
    // At last, actually generate the cxx::bridge entry.
    let bridge_unsafety = analysis.requires_unsafe.bridge_token();
    let extern_c_mod_item = ForeignItem::Fn(parse_quote!(
        #(#namespace_attr)*
        #(#rust_name_attr)*
        #(#cpp_name_attr)*
        #doc_attr
        #vis #bridge_unsafety fn #cxxbridge_name #lifetime_tokens ( #params ) #ret_type;
    ));
    RsCodegenResult {
        extern_c_mod_items: vec![extern_c_mod_item],
        bridge_items: Vec::new(),
        global_items: trait_impl_entry.into_iter().collect(),
        bindgen_mod_items: Vec::new(),
        impl_entry,
        materializations: materialization.into_iter().collect(),
        extern_rust_mod_items: Vec::new(),
    }
}

/// Knows how to generate a given function.
#[derive(Clone)]
struct FnGenerator<'a> {
    param_details: &'a [ArgumentAnalysis],
    cxxbridge_name: &'a Ident,
    rust_name: &'a str,
    unsafety: &'a Option<Unsafe>,
    doc_attr: &'a Option<Attribute>,
}

impl<'a> FnGenerator<'a> {
    fn generate_arg_lists(&self, avoid_self: bool) -> (Punctuated<FnArg, Comma>, Vec<TokenStream>) {
        let mut wrapper_params: Punctuated<FnArg, Comma> = Punctuated::new();
        let mut arg_list = Vec::new();

        for pd in self.param_details {
            let type_name = pd.conversion.rust_wrapper_unconverted_type();
            let wrapper_arg_name = if pd.self_type.is_some() && !avoid_self {
                parse_quote!(self)
            } else {
                pd.name.clone()
            };
            let param_mutability = pd.conversion.rust_conversion.requires_mutability();
            wrapper_params.push(parse_quote!(
                #param_mutability #wrapper_arg_name: #type_name
            ));
            arg_list.push(pd.conversion.rust_conversion(wrapper_arg_name));
        }
        (wrapper_params, arg_list)
    }

    /// Generate an 'impl Type { methods-go-here }' item
    fn generate_method_impl(
        &self,
        avoid_self: bool,
        impl_block_type_name: &QualifiedName,
        ret_type: &ReturnType,
    ) -> Box<ImplBlockDetails> {
        let (wrapper_params, arg_list) = self.generate_arg_lists(avoid_self);
        let (lifetime_tokens, wrapper_params, ret_type) =
            add_explicit_lifetime_if_necessary(self.param_details, wrapper_params, ret_type);
        let rust_name = make_ident(self.rust_name);
        let unsafety = self.unsafety;
        let doc_attr = self.doc_attr;
        let cxxbridge_name = self.cxxbridge_name;
        Box::new(ImplBlockDetails {
            item: ImplItem::Method(parse_quote! {
                #doc_attr
                pub #unsafety fn #rust_name #lifetime_tokens ( #wrapper_params ) #ret_type {
                    cxxbridge::#cxxbridge_name ( #(#arg_list),* )
                }
            }),
            ty: impl_block_type_name.get_final_ident(),
        })
    }

    /// Generate an 'impl Trait for Type { methods-go-here }' in its entrety.
    fn generate_trait_impl(&self, details: &TraitMethodDetails, ret_type: &ReturnType) -> Item {
        let (mut wrapper_params, arg_list) = self.generate_arg_lists(details.avoid_self);
        if let Some(parameter_reordering) = &details.parameter_reordering {
            wrapper_params = Self::reorder_parameters(wrapper_params, parameter_reordering);
        }
        let (lifetime_tokens, wrapper_params, ret_type) =
            add_explicit_lifetime_if_necessary(self.param_details, wrapper_params, ret_type);
        let doc_attr = self.doc_attr;
        let unsafety = self.unsafety;
        let cxxbridge_name = self.cxxbridge_name;
        let trait_signature = &details.trait_signature;
        let trait_unsafety = &details.trait_unsafety;
        let impl_for_specifics = &details.impl_for_specifics;
        let method_name = &details.method_name;
        let call_body = quote! {
            cxxbridge::#cxxbridge_name ( #(#arg_list),* )
        };
        let call_body = if details.trait_call_is_unsafe {
            quote! {
                unsafe {
                    #call_body
                }
            }
        } else {
            call_body
        };
        parse_quote! {
            #trait_unsafety impl #lifetime_tokens #trait_signature for #impl_for_specifics {
                #doc_attr
                #unsafety fn #method_name ( #wrapper_params ) #ret_type {
                    #call_body
                }
            }
        }
    }

    /// Generate a 'impl Type { methods-go-here }' item which is a constructor
    /// for use with moveit traits.
    fn generate_constructor_impl(
        &self,
        impl_block_type_name: &QualifiedName,
    ) -> Box<ImplBlockDetails> {
        let (wrapper_params, arg_list) = self.generate_arg_lists(true);
        let mut wrapper_params: Punctuated<FnArg, Comma> =
            wrapper_params.into_iter().skip(1).collect();
        let ptr_arg_name = &arg_list[0];
        let rust_name = make_ident(&self.rust_name);
        let any_references = self.param_details.iter().any(|pd| pd.was_reference);
        let (lifetime_param, lifetime_addition) = if any_references {
            add_lifetime_to_all_params(&mut wrapper_params);
            (quote! { <'a> }, quote! { + 'a })
        } else {
            (quote! {}, quote! {})
        };
        let cxxbridge_name = self.cxxbridge_name;
        let body = quote! {
            autocxx::moveit::new::by_raw(move |#ptr_arg_name| {
                let #ptr_arg_name = #ptr_arg_name.get_unchecked_mut().as_mut_ptr();
                cxxbridge::#cxxbridge_name(#(#arg_list),* )
            })
        };
        let body = if self.unsafety.is_some() {
            // No need for `unsafe` inside the function
            body
        } else {
            quote! {
                unsafe { #body }
            }
        };
        let doc_attr = self.doc_attr;
        let unsafety = self.unsafety;
        Box::new(ImplBlockDetails {
            item: ImplItem::Method(parse_quote! {
                #doc_attr
                pub #unsafety fn #rust_name #lifetime_param ( #wrapper_params ) -> impl autocxx::moveit::new::New<Output=Self> #lifetime_addition {
                    #body
                }
            }),
            ty: impl_block_type_name.get_final_ident(),
        })
    }

    /// Generate a function call wrapper
    fn generate_function_impl(&self, ret_type: &ReturnType) -> Box<Item> {
        let (wrapper_params, arg_list) = self.generate_arg_lists(false);
        let rust_name = make_ident(self.rust_name);
        let doc_attr = self.doc_attr;
        let unsafety = self.unsafety;
        Box::new(Item::Fn(parse_quote! {
            #doc_attr
            pub #unsafety fn #rust_name ( #wrapper_params ) #ret_type {
                cxxbridge::#rust_name ( #(#arg_list),* )
            }
        }))
    }

    fn reorder_parameters(
        params: Punctuated<FnArg, Comma>,
        parameter_ordering: &[usize],
    ) -> Punctuated<FnArg, Comma> {
        let old_params = params.into_iter().collect::<Vec<_>>();
        parameter_ordering
            .iter()
            .map(|n| old_params.get(*n).unwrap().clone())
            .collect()
    }
}
