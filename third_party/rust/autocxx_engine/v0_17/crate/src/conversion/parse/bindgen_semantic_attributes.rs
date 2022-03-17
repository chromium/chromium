// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use proc_macro2::{Ident, TokenStream};
use syn::{
    parenthesized,
    parse::{Parse, Parser},
    Attribute, LitStr,
};

use crate::conversion::{
    api::{CppVisibility, Layout, References, SpecialMemberKind, Virtualness},
    convert_error::{ConvertErrorWithContext, ErrorContext},
    ConvertError,
};

/// The set of all annotations that autocxx_bindgen has added
/// for our benefit.
#[derive(Debug)]
pub(crate) struct BindgenSemanticAttributes(Vec<BindgenSemanticAttribute>);

impl BindgenSemanticAttributes {
    // Remove `bindgen_` attributes. They don't have a corresponding macro defined anywhere,
    // so they will cause compilation errors if we leave them in.
    // We may return an error if one of the bindgen attributes shows that the
    // item can't be processed.
    pub(crate) fn new_retaining_others(attrs: &mut Vec<Attribute>) -> Self {
        let metadata = Self::new(attrs);
        attrs.retain(|a| a.path.segments.last().unwrap().ident != "cpp_semantics");
        metadata
    }

    pub(crate) fn new(attrs: &[Attribute]) -> Self {
        Self(
            attrs
                .iter()
                .filter_map(|attr| {
                    if attr.path.segments.last().unwrap().ident == "cpp_semantics" {
                        let r: Result<BindgenSemanticAttribute, syn::Error> = attr.parse_args();
                        r.ok()
                    } else {
                        None
                    }
                })
                .collect(),
        )
    }

    /// Some attributes indicate we can never handle a given item. Check for those.
    pub(crate) fn check_for_fatal_attrs(
        &self,
        id_for_context: &Ident,
    ) -> Result<(), ConvertErrorWithContext> {
        if self.has_attr("unused_template_param") {
            Err(ConvertErrorWithContext(
                ConvertError::UnusedTemplateParam,
                Some(ErrorContext::new_for_item(id_for_context.clone())),
            ))
        } else {
            Ok(())
        }
    }

    /// Whether the given attribute is present.
    pub(super) fn has_attr(&self, attr_name: &str) -> bool {
        self.0.iter().any(|a| a.is_ident(attr_name))
    }

    /// The C++ visibility of the item.
    pub(super) fn get_cpp_visibility(&self) -> CppVisibility {
        if self.has_attr("visibility_private") {
            CppVisibility::Private
        } else if self.has_attr("visibility_protected") {
            CppVisibility::Protected
        } else {
            CppVisibility::Public
        }
    }

    /// Whether the item is virtual.
    pub(super) fn get_virtualness(&self) -> Virtualness {
        if self.has_attr("pure_virtual") {
            Virtualness::PureVirtual
        } else if self.has_attr("bindgen_virtual") {
            Virtualness::Virtual
        } else {
            Virtualness::None
        }
    }

    fn parse_if_present<T: Parse>(&self, annotation: &str) -> Option<T> {
        self.0
            .iter()
            .find(|a| a.is_ident(annotation))
            .map(|a| a.parse_args().unwrap())
    }

    fn string_if_present(&self, annotation: &str) -> Option<String> {
        let ls: Option<LitStr> = self.parse_if_present(annotation);
        ls.map(|ls| ls.value())
    }

    /// The in-memory layout of the item.
    pub(super) fn get_layout(&self) -> Option<Layout> {
        self.parse_if_present("layout")
    }

    /// The original C++ name, which bindgen may have changed.
    pub(super) fn get_original_name(&self) -> Option<String> {
        self.string_if_present("original_name")
    }

    /// Whether this is a move constructor or other special member.
    pub(super) fn special_member_kind(&self) -> Option<SpecialMemberKind> {
        self.string_if_present("special_member")
            .map(|kind| match kind.as_str() {
                "default_ctor" => SpecialMemberKind::DefaultConstructor,
                "copy_ctor" => SpecialMemberKind::CopyConstructor,
                "move_ctor" => SpecialMemberKind::MoveConstructor,
                "dtor" => SpecialMemberKind::Destructor,
                "assignment_operator" => SpecialMemberKind::AssignmentOperator,
                _ => panic!("unexpected special_member_kind"),
            })
    }

    /// Any reference parameters or return values.
    pub(super) fn get_reference_parameters_and_return(&self) -> References {
        let mut results = References::default();
        for a in &self.0 {
            if a.is_ident("ret_type_reference") {
                results.ref_return = true;
            } else if a.is_ident("ret_type_rvalue_reference") {
                results.rvalue_ref_return = true;
            } else if a.is_ident("arg_type_reference") {
                let r: Result<Ident, syn::Error> = a.parse_args();
                if let Ok(ls) = r {
                    results.ref_params.insert(ls);
                }
            } else if a.is_ident("arg_type_rvalue_reference") {
                let r: Result<Ident, syn::Error> = a.parse_args();
                if let Ok(ls) = r {
                    results.rvalue_ref_params.insert(ls);
                }
            }
        }
        results
    }
}

#[derive(Debug)]
struct BindgenSemanticAttribute {
    annotation_name: Ident,
    body: Option<TokenStream>,
}

impl BindgenSemanticAttribute {
    fn is_ident(&self, name: &str) -> bool {
        self.annotation_name == name
    }

    fn parse_args<T: Parse>(&self) -> Result<T, syn::Error> {
        T::parse.parse2(self.body.as_ref().unwrap().clone())
    }
}

impl Parse for BindgenSemanticAttribute {
    fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
        let annotation_name: Ident = input.parse()?;
        if input.peek(syn::token::Paren) {
            let body_contents;
            parenthesized!(body_contents in input);
            Ok(Self {
                annotation_name,
                body: Some(body_contents.parse()?),
            })
        } else if !input.is_empty() {
            Err(input.error("expected nothing"))
        } else {
            Ok(Self {
                annotation_name,
                body: None,
            })
        }
    }
}
