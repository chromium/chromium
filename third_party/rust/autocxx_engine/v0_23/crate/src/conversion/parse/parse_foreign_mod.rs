// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::conversion::api::{ApiName, NullPhase, Provenance};
use crate::conversion::apivec::ApiVec;
use crate::conversion::doc_attr::get_doc_attrs;
use crate::conversion::error_reporter::report_any_error;
use crate::conversion::{
    api::{FuncToConvert, UnanalyzedApi},
    convert_error::ConvertErrorWithContext,
    convert_error::ErrorContext,
};
use crate::{
    conversion::ConvertErrorFromCpp,
    types::{Namespace, QualifiedName},
};
use std::collections::HashMap;
use syn::{Block, Expr, ExprCall, ForeignItem, Ident, ImplItem, ItemImpl, Stmt, Type};

use super::bindgen_semantic_attributes::BindgenSemanticAttributes;

/// Parses a given bindgen-generated 'mod' into suitable
/// [Api]s. In bindgen output, a given mod concerns
/// a specific C++ namespace.
pub(crate) struct ParseForeignMod {
    ns: Namespace,
    // We mostly act upon the functions we see within the 'extern "C"'
    // block of bindgen output, but we can't actually do this until
    // we've seen the (possibly subsequent) 'impl' blocks so we can
    // deduce which functions are actually static methods. Hence
    // store them.
    funcs_to_convert: Vec<FuncToConvert>,
    // Evidence from 'impl' blocks about which of these items
    // may actually be methods (static or otherwise). Mapping from
    // function name to type name.
    method_receivers: HashMap<Ident, QualifiedName>,
    ignored_apis: ApiVec<NullPhase>,
}

impl ParseForeignMod {
    pub(crate) fn new(ns: Namespace) -> Self {
        Self {
            ns,
            funcs_to_convert: Vec::new(),
            method_receivers: HashMap::new(),
            ignored_apis: ApiVec::new(),
        }
    }

    /// Record information from foreign mod items encountered
    /// in bindgen output.
    pub(crate) fn convert_foreign_mod_items(&mut self, foreign_mod_items: Vec<ForeignItem>) {
        let mut extra_apis = ApiVec::new();
        for i in foreign_mod_items {
            report_any_error(&self.ns.clone(), &mut extra_apis, || {
                self.parse_foreign_item(i)
            });
        }
        self.ignored_apis.append(&mut extra_apis);
    }

    fn parse_foreign_item(&mut self, i: ForeignItem) -> Result<(), ConvertErrorWithContext> {
        match i {
            ForeignItem::Fn(item) => {
                let annotations = BindgenSemanticAttributes::new(&item.attrs);
                let doc_attrs = get_doc_attrs(&item.attrs);
                self.funcs_to_convert.push(FuncToConvert {
                    provenance: Provenance::Bindgen,
                    self_ty: None,
                    ident: item.sig.ident,
                    doc_attrs,
                    inputs: item.sig.inputs,
                    output: item.sig.output,
                    vis: item.vis,
                    virtualness: annotations.get_virtualness(),
                    cpp_vis: annotations.get_cpp_visibility(),
                    special_member: annotations.special_member_kind(),
                    unused_template_param: annotations
                        .has_attr("incomprehensible_param_in_arg_or_return"),
                    references: annotations.get_reference_parameters_and_return(),
                    original_name: annotations.get_original_name(),
                    synthesized_this_type: None,
                    add_to_trait: None,
                    is_deleted: annotations.get_deleted_or_defaulted(),
                    synthetic_cpp: None,
                    variadic: item.sig.variadic.is_some(),
                });
                Ok(())
            }
            ForeignItem::Static(item) => Err(ConvertErrorWithContext(
                ConvertErrorFromCpp::StaticData(item.ident.to_string()),
                Some(ErrorContext::new_for_item(item.ident)),
            )),
            _ => Err(ConvertErrorWithContext(
                ConvertErrorFromCpp::UnexpectedForeignItem,
                None,
            )),
        }
    }

    /// Record information from impl blocks encountered in bindgen
    /// output.
    pub(crate) fn convert_impl_items(&mut self, imp: ItemImpl) {
        let ty_id = match *imp.self_ty {
            Type::Path(typ) => typ.path.segments.last().unwrap().ident.clone(),
            _ => return,
        };
        for i in imp.items {
            if let ImplItem::Method(itm) = i {
                let effective_fun_name = match get_called_function(&itm.block) {
                    Some(id) => id.clone(),
                    None => itm.sig.ident,
                };
                self.method_receivers.insert(
                    effective_fun_name,
                    QualifiedName::new(&self.ns, ty_id.clone()),
                );
            }
        }
    }

    /// Indicate that all foreign mods and all impl blocks have been
    /// fed into us, and we should process that information to generate
    /// the resulting APIs.
    pub(crate) fn finished(mut self, apis: &mut ApiVec<NullPhase>) {
        apis.append(&mut self.ignored_apis);
        while !self.funcs_to_convert.is_empty() {
            let mut fun = self.funcs_to_convert.remove(0);
            fun.self_ty = self.method_receivers.get(&fun.ident).cloned();
            apis.push(UnanalyzedApi::Function {
                name: ApiName::new_with_cpp_name(
                    &self.ns,
                    fun.ident.clone(),
                    fun.original_name.clone(),
                ),
                fun: Box::new(fun),
                analysis: (),
            })
        }
    }
}

/// bindgen sometimes generates an impl fn called a which calls
/// a function called a1(), if it's dealing with conflicting names.
/// We actually care about the name a1, so we have to parse the
/// name of the actual function call inside the block's body.
fn get_called_function(block: &Block) -> Option<&Ident> {
    match block.stmts.first() {
        Some(Stmt::Expr(Expr::Call(ExprCall { func, .. }))) => match **func {
            Expr::Path(ref exp) => exp.path.segments.first().map(|ps| &ps.ident),
            _ => None,
        },
        _ => None,
    }
}

#[cfg(test)]
mod test {
    use super::get_called_function;
    use syn::parse_quote;
    use syn::Block;

    #[test]
    fn test_get_called_function() {
        let b: Block = parse_quote! {
            {
                call_foo()
            }
        };
        assert_eq!(get_called_function(&b).unwrap().to_string(), "call_foo");
    }
}
