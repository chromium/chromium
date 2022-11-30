// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use autocxx_parser::IncludeCppConfig;
use indexmap::set::IndexSet as HashSet;
use std::borrow::Cow;

use proc_macro2::TokenStream;
use quote::{quote, ToTokens};
use syn::{
    parse::Parser,
    parse_quote,
    punctuated::Punctuated,
    token::{Comma, Unsafe},
    Attribute, FnArg, ForeignItem, Ident, ImplItem, Item, ReturnType,
};

use super::{
    function_wrapper_rs::RustParamConversion,
    maybe_unsafes_to_tokens,
    unqualify::{unqualify_params, unqualify_ret_type},
    ImplBlockDetails, ImplBlockKey, MaybeUnsafeStmt, RsCodegenResult, TraitImplBlockDetails, Use,
};
use crate::{
    conversion::{
        analysis::fun::{
            function_wrapper::TypeConversionPolicy, ArgumentAnalysis, FnAnalysis, FnKind,
            MethodKind, RustRenameStrategy, TraitMethodDetails,
        },
        api::{Pointerness, UnsafetyNeeded},
    },
    types::{Namespace, QualifiedName},
};
use crate::{
    conversion::{api::FuncToConvert, codegen_rs::lifetime::add_explicit_lifetime_if_necessary},
    types::make_ident,
};

impl UnsafetyNeeded {
    pub(crate) fn bridge_token(&self) -> Option<Unsafe> {
        match self {
            UnsafetyNeeded::None => None,
            _ => Some(parse_quote! { unsafe }),
        }
    }

    pub(crate) fn wrapper_token(&self) -> Option<Unsafe> {
        match self {
            UnsafetyNeeded::Always => Some(parse_quote! { unsafe }),
            _ => None,
        }
    }

    pub(crate) fn from_param_details(params: &[ArgumentAnalysis], ignore_placements: bool) -> Self {
        params.iter().fold(UnsafetyNeeded::None, |accumulator, pd| {
            if matches!(accumulator, UnsafetyNeeded::Always) {
                UnsafetyNeeded::Always
            } else if (pd.self_type.is_some() || pd.is_placement_return_destination)
                && ignore_placements
            {
                if matches!(
                    pd.requires_unsafe,
                    UnsafetyNeeded::Always | UnsafetyNeeded::JustBridge
                ) {
                    UnsafetyNeeded::JustBridge
                } else {
                    accumulator
                }
            } else if matches!(pd.requires_unsafe, UnsafetyNeeded::Always) {
                UnsafetyNeeded::Always
            } else if matches!(accumulator, UnsafetyNeeded::JustBridge)
                || matches!(pd.requires_unsafe, UnsafetyNeeded::JustBridge)
            {
                UnsafetyNeeded::JustBridge
            } else {
                UnsafetyNeeded::None
            }
        })
    }
}

pub(super) fn gen_function(
    ns: &Namespace,
    fun: FuncToConvert,
    analysis: FnAnalysis,
    cpp_call_name: String,
    non_pod_types: &HashSet<QualifiedName>,
    config: &IncludeCppConfig,
) -> RsCodegenResult {
    if analysis.ignore_reason.is_err() || !analysis.externally_callable {
        return RsCodegenResult::default();
    }
    let cxxbridge_name = analysis.cxxbridge_name;
    let rust_name = &analysis.rust_name;
    let ret_type = analysis.ret_type;
    let ret_conversion = analysis.ret_conversion;
    let param_details = analysis.param_details;
    let wrapper_function_needed = analysis.cpp_wrapper.is_some();
    let params = analysis.params;
    let vis = analysis.vis;
    let kind = analysis.kind;
    let doc_attrs = fun.doc_attrs;

    let mut cpp_name_attr = Vec::new();
    let mut impl_entry = None;
    let mut trait_impl_entry = None;
    let mut bindgen_mod_items = Vec::new();
    let always_unsafe_due_to_trait_definition = match kind {
        FnKind::TraitMethod { ref details, .. } => details.trait_call_is_unsafe,
        _ => false,
    };
    let fn_generator = FnGenerator {
        param_details: &param_details,
        cxxbridge_name: &cxxbridge_name,
        rust_name,
        unsafety: &analysis.requires_unsafe,
        always_unsafe_due_to_trait_definition,
        doc_attrs: &doc_attrs,
        non_pod_types,
        ret_type: &ret_type,
        ret_conversion: &ret_conversion,
        reference_wrappers: config.unsafe_policy.requires_cpprefs(),
    };
    // In rare occasions, we might need to give an explicit lifetime.
    let (lifetime_tokens, params, ret_type) = add_explicit_lifetime_if_necessary(
        &param_details,
        params,
        Cow::Borrowed(&ret_type),
        non_pod_types,
        true,
    );

    if analysis.rust_wrapper_needed {
        match kind {
            FnKind::Method {
                ref impl_for,
                method_kind: MethodKind::Constructor { .. },
                ..
            } => {
                // Constructor.
                impl_entry = Some(fn_generator.generate_constructor_impl(impl_for));
            }
            FnKind::Method {
                ref impl_for,
                ref method_kind,
                ..
            } => {
                // Method, or static method.
                impl_entry = Some(fn_generator.generate_method_impl(
                    matches!(method_kind, MethodKind::Constructor { .. }),
                    impl_for,
                ));
            }
            FnKind::TraitMethod { ref details, .. } => {
                trait_impl_entry = Some(fn_generator.generate_trait_impl(details));
            }
            _ => {
                // Generate plain old function
                bindgen_mod_items.push(fn_generator.generate_function_impl());
            }
        }
    }

    let materialization = match kind {
        FnKind::Method { .. } | FnKind::TraitMethod { .. } => None,
        FnKind::Function => match analysis.rust_rename_strategy {
            _ if analysis.rust_wrapper_needed => {
                Some(Use::SpecificNameFromBindgen(make_ident(rust_name)))
            }
            RustRenameStrategy::RenameInOutputMod(ref alias) => {
                Some(Use::UsedFromCxxBridgeWithAlias(alias.clone()))
            }
            _ => Some(Use::UsedFromCxxBridge),
        },
    };
    if cxxbridge_name != cpp_call_name && !wrapper_function_needed {
        cpp_name_attr = Attribute::parse_outer
            .parse2(quote!(
                #[cxx_name = #cpp_call_name]
            ))
            .unwrap();
    }

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
        #(#cpp_name_attr)*
        #(#doc_attrs)*
        #vis #bridge_unsafety fn #cxxbridge_name #lifetime_tokens ( #params ) #ret_type;
    ));
    RsCodegenResult {
        extern_c_mod_items: vec![extern_c_mod_item],
        bindgen_mod_items,
        impl_entry,
        trait_impl_entry,
        materializations: materialization.into_iter().collect(),
        ..Default::default()
    }
}

/// Knows how to generate a given function.
#[derive(Clone)]
struct FnGenerator<'a> {
    param_details: &'a [ArgumentAnalysis],
    ret_conversion: &'a Option<TypeConversionPolicy>,
    ret_type: &'a ReturnType,
    cxxbridge_name: &'a Ident,
    rust_name: &'a str,
    unsafety: &'a UnsafetyNeeded,
    always_unsafe_due_to_trait_definition: bool,
    doc_attrs: &'a Vec<Attribute>,
    non_pod_types: &'a HashSet<QualifiedName>,
    reference_wrappers: bool,
}

impl<'a> FnGenerator<'a> {
    fn common_parts<'b>(
        &'b self,
        avoid_self: bool,
        parameter_reordering: &Option<Vec<usize>>,
        ret_type: Option<ReturnType>,
    ) -> (
        Option<TokenStream>,
        Punctuated<FnArg, Comma>,
        std::borrow::Cow<'b, ReturnType>,
        TokenStream,
    ) {
        let mut wrapper_params: Punctuated<FnArg, Comma> = Punctuated::new();
        let mut local_variables = Vec::new();
        let mut arg_list = Vec::new();
        let mut ptr_arg_name = None;
        let mut ret_type: Cow<'a, _> = ret_type
            .map(Cow::Owned)
            .unwrap_or(Cow::Borrowed(self.ret_type));
        let mut any_conversion_requires_unsafe = false;
        let mut variable_counter = 0usize;
        for pd in self.param_details {
            let wrapper_arg_name = if pd.self_type.is_some() && !avoid_self {
                parse_quote!(self)
            } else {
                pd.name.clone()
            };
            let rust_for_param = pd
                .conversion
                .rust_conversion(parse_quote! { #wrapper_arg_name }, &mut variable_counter);
            match rust_for_param {
                RustParamConversion::Param {
                    ty,
                    conversion,
                    local_variables: mut these_local_variables,
                    conversion_requires_unsafe,
                } => {
                    arg_list.push(conversion.clone());
                    local_variables.append(&mut these_local_variables);
                    if pd.is_placement_return_destination {
                        ptr_arg_name = Some(conversion);
                    } else {
                        let param_mutability = pd.conversion.rust_conversion.requires_mutability();
                        wrapper_params.push(parse_quote!(
                            #param_mutability #wrapper_arg_name: #ty
                        ));
                    }
                    any_conversion_requires_unsafe =
                        conversion_requires_unsafe || any_conversion_requires_unsafe;
                }
                RustParamConversion::ReturnValue { ty } => {
                    ptr_arg_name = Some(pd.name.to_token_stream());
                    ret_type = Cow::Owned(parse_quote! {
                        -> impl autocxx::moveit::new::New<Output = #ty>
                    });
                    arg_list.push(pd.name.to_token_stream());
                }
            }
        }
        if let Some(parameter_reordering) = &parameter_reordering {
            wrapper_params = Self::reorder_parameters(wrapper_params, parameter_reordering);
        }
        let (lifetime_tokens, wrapper_params, ret_type) = add_explicit_lifetime_if_necessary(
            self.param_details,
            wrapper_params,
            ret_type,
            self.non_pod_types,
            false,
        );

        let cxxbridge_name = self.cxxbridge_name;
        let call_body = MaybeUnsafeStmt::maybe_unsafe(
            quote! {
                cxxbridge::#cxxbridge_name ( #(#arg_list),* )
            },
            any_conversion_requires_unsafe || matches!(self.unsafety, UnsafetyNeeded::JustBridge),
        );
        let context_is_unsafe = matches!(self.unsafety, UnsafetyNeeded::Always)
            || self.always_unsafe_due_to_trait_definition;
        let (call_body, ret_type) = match self.ret_conversion {
            Some(ret_conversion) if ret_conversion.rust_work_needed() => {
                let expr = maybe_unsafes_to_tokens(vec![call_body], context_is_unsafe);
                let conv =
                    ret_conversion.rust_conversion(parse_quote! { #expr }, &mut variable_counter);
                let (conversion, requires_unsafe, ty) = match conv {
                    RustParamConversion::Param {
                        local_variables, ..
                    } if !local_variables.is_empty() => panic!("return type required variables"),
                    RustParamConversion::Param {
                        conversion,
                        conversion_requires_unsafe,
                        ty,
                        ..
                    } => (conversion, conversion_requires_unsafe, ty),
                    _ => panic!(
                        "Unexpected - return type is supposed to be converted to a return type"
                    ),
                };
                (
                    if requires_unsafe {
                        MaybeUnsafeStmt::NeedsUnsafe(conversion)
                    } else {
                        MaybeUnsafeStmt::Normal(conversion)
                    },
                    Cow::Owned(parse_quote! { -> #ty }),
                )
            }
            _ => (call_body, ret_type),
        };

        let call_stmts = if let Some(ptr_arg_name) = ptr_arg_name {
            let mut closure_stmts = local_variables;
            closure_stmts.push(MaybeUnsafeStmt::binary(
                quote! { let #ptr_arg_name = unsafe { #ptr_arg_name.get_unchecked_mut().as_mut_ptr() };},
                quote! { let #ptr_arg_name = #ptr_arg_name.get_unchecked_mut().as_mut_ptr();},
            ));
            closure_stmts.push(call_body);
            let closure_stmts = maybe_unsafes_to_tokens(closure_stmts, true);
            vec![MaybeUnsafeStmt::needs_unsafe(parse_quote! {
                autocxx::moveit::new::by_raw(move |#ptr_arg_name| {
                    #closure_stmts
                })
            })]
        } else {
            let mut call_stmts = local_variables;
            call_stmts.push(call_body);
            call_stmts
        };
        let call_body = maybe_unsafes_to_tokens(call_stmts, context_is_unsafe);
        (lifetime_tokens, wrapper_params, ret_type, call_body)
    }

    /// Generate an 'impl Type { methods-go-here }' item
    fn generate_method_impl(
        &self,
        avoid_self: bool,
        impl_block_type_name: &QualifiedName,
    ) -> Box<ImplBlockDetails> {
        let (lifetime_tokens, wrapper_params, ret_type, call_body) =
            self.common_parts(avoid_self, &None, None);
        let rust_name = make_ident(self.rust_name);
        let unsafety = self.unsafety.wrapper_token();
        let doc_attrs = self.doc_attrs;
        let receiver_pointerness = self
            .param_details
            .iter()
            .next()
            .map(|pd| pd.conversion.is_a_pointer())
            .unwrap_or(Pointerness::Not);
        let ty = impl_block_type_name.get_final_ident();
        let ty = if self.reference_wrappers {
            match receiver_pointerness {
                Pointerness::MutPtr => ImplBlockKey {
                    ty: parse_quote! {
                        CppMutRef< 'a, #ty>
                    },
                    lifetime: Some(parse_quote! { 'a }),
                },
                Pointerness::ConstPtr => ImplBlockKey {
                    ty: parse_quote! {
                        CppRef< 'a, #ty>
                    },
                    lifetime: Some(parse_quote! { 'a }),
                },
                Pointerness::Not => ImplBlockKey {
                    ty: parse_quote! { # ty },
                    lifetime: None,
                },
            }
        } else {
            ImplBlockKey {
                ty: parse_quote! { # ty },
                lifetime: None,
            }
        };
        Box::new(ImplBlockDetails {
            item: ImplItem::Method(parse_quote! {
                #(#doc_attrs)*
                pub #unsafety fn #rust_name #lifetime_tokens ( #wrapper_params ) #ret_type {
                    #call_body
                }
            }),
            ty,
        })
    }

    /// Generate an 'impl Trait for Type { methods-go-here }' in its entrety.
    fn generate_trait_impl(&self, details: &TraitMethodDetails) -> Box<TraitImplBlockDetails> {
        let (lifetime_tokens, wrapper_params, ret_type, call_body) =
            self.common_parts(details.avoid_self, &details.parameter_reordering, None);
        let doc_attrs = self.doc_attrs;
        let unsafety = self.unsafety.wrapper_token();
        let key = details.trt.clone();
        let method_name = &details.method_name;
        let item = parse_quote! {
            #(#doc_attrs)*
            #unsafety fn #method_name #lifetime_tokens ( #wrapper_params ) #ret_type {
                #call_body
            }
        };
        Box::new(TraitImplBlockDetails { item, key })
    }

    /// Generate a 'impl Type { methods-go-here }' item which is a constructor
    /// for use with moveit traits.
    fn generate_constructor_impl(
        &self,
        impl_block_type_name: &QualifiedName,
    ) -> Box<ImplBlockDetails> {
        let ret_type: ReturnType = parse_quote! { -> impl autocxx::moveit::new::New<Output=Self> };
        let (lifetime_tokens, wrapper_params, ret_type, call_body) =
            self.common_parts(true, &None, Some(ret_type));
        let rust_name = make_ident(&self.rust_name);
        let doc_attrs = self.doc_attrs;
        let unsafety = self.unsafety.wrapper_token();
        let ty = impl_block_type_name.get_final_ident();
        let ty = parse_quote! { #ty };
        let stuff = quote! {
                #(#doc_attrs)*
                pub #unsafety fn #rust_name #lifetime_tokens ( #wrapper_params ) #ret_type {
                    #call_body
                }
        };
        Box::new(ImplBlockDetails {
            item: ImplItem::Method(parse_quote! { #stuff }),
            ty: ImplBlockKey { ty, lifetime: None },
        })
    }

    /// Generate a function call wrapper
    fn generate_function_impl(&self) -> Item {
        let (lifetime_tokens, wrapper_params, ret_type, call_body) =
            self.common_parts(false, &None, None);
        let rust_name = make_ident(self.rust_name);
        let doc_attrs = self.doc_attrs;
        let unsafety = self.unsafety.wrapper_token();
        Item::Fn(parse_quote! {
            #(#doc_attrs)*
            pub #unsafety fn #rust_name #lifetime_tokens ( #wrapper_params ) #ret_type {
                #call_body
            }
        })
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
