// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::collections::HashSet;
use std::fmt::Display;

use itertools::Itertools;
use syn::Ident;

use crate::{
    known_types,
    types::{make_ident, Namespace, QualifiedName},
};

#[derive(Debug, Clone)]
pub enum ConvertError {
    NoContent,
    UnsafePodType(String),
    UnexpectedForeignItem,
    UnexpectedOuterItem,
    UnexpectedItemInMod,
    ComplexTypedefTarget(String),
    UnexpectedThisType(Namespace, String),
    UnsupportedBuiltInType(QualifiedName),
    ConflictingTemplatedArgsWithTypedef(QualifiedName),
    UnacceptableParam(String),
    NotOneInputReference(String),
    UnsupportedType(String),
    UnknownType(String),
    StaticData(String),
    InfinitelyRecursiveTypedef(QualifiedName),
    UnexpectedUseStatement(Option<Ident>),
    TemplatedTypeContainingNonPathArg(QualifiedName),
    InvalidPointee,
    DidNotGenerateAnything(String),
    TypeContainingForwardDeclaration(QualifiedName),
    Blocked(QualifiedName),
    UnusedTemplateParam,
    TooManyUnderscores,
    UnknownDependentType(QualifiedName),
    IgnoredDependent(HashSet<QualifiedName>),
    ReservedName(String),
    DuplicateCxxBridgeName,
    UnsupportedReceiver,
    BoxContainingNonRustType(QualifiedName),
    RustTypeWithAPath(QualifiedName),
    AbstractNestedType,
    NonPublicNestedType,
    RValueParam,
    RValueReturn,
    PrivateMethod,
    AssignmentOperator,
    Deleted,
    RValueReferenceField,
    MethodOfNonAllowlistedType,
    MethodOfGenericType,
    DuplicateItemsFoundInParsing,
    ConstructorWithOnlyOneParam,
}

fn format_maybe_identifier(id: &Option<Ident>) -> String {
    match id {
        Some(id) => id.to_string(),
        None => "<unknown>".into(),
    }
}

impl Display for ConvertError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ConvertError::NoContent => write!(f, "The initial run of 'bindgen' did not generate any content. This might be because none of the requested items for generation could be converted.")?,
            ConvertError::UnsafePodType(err) => write!(f, "An item was requested using 'generate_pod' which was not safe to hold by value in Rust. {}", err)?,
            ConvertError::UnexpectedForeignItem => write!(f, "Bindgen generated some unexpected code in a foreign mod section. You may have specified something in a 'generate' directive which is not currently compatible with autocxx.")?,
            ConvertError::UnexpectedOuterItem => write!(f, "Bindgen generated some unexpected code in its outermost mod section. You may have specified something in a 'generate' directive which is not currently compatible with autocxx.")?,
            ConvertError::UnexpectedItemInMod => write!(f, "Bindgen generated some unexpected code in an inner namespace mod. You may have specified something in a 'generate' directive which is not currently compatible with autocxx.")?,
            ConvertError::ComplexTypedefTarget(ty) => write!(f, "autocxx was unable to produce a typdef pointing to the complex type {}.", ty)?,
            ConvertError::UnexpectedThisType(ns, fn_name) => write!(f, "Unexpected type for 'this' in the function {}{}.", fn_name, ns.to_display_suffix())?,
            ConvertError::UnsupportedBuiltInType(ty) => write!(f, "autocxx does not yet know how to support the built-in C++ type {} - please raise an issue on github", ty.to_cpp_name())?,
            ConvertError::ConflictingTemplatedArgsWithTypedef(tn) => write!(f, "Type {} has templated arguments and so does the typedef to which it points", tn)?,
            ConvertError::UnacceptableParam(fn_name) => write!(f, "Function {} has a parameter or return type which is either on the blocklist or a forward declaration", fn_name)?,
            ConvertError::NotOneInputReference(fn_name) => write!(f, "Function {} has a return reference parameter, but 0 or >1 input reference parameters, so the lifetime of the output reference cannot be deduced.", fn_name)?,
            ConvertError::UnsupportedType(ty_desc) => write!(f, "Encountered type not yet supported by autocxx: {}", ty_desc)?,
            ConvertError::UnknownType(ty_desc) => write!(f, "Encountered type not yet known by autocxx: {}", ty_desc)?,
            ConvertError::StaticData(ty_desc) => write!(f, "Encountered mutable static data, not yet supported: {}", ty_desc)?,
            ConvertError::InfinitelyRecursiveTypedef(tn) => write!(f, "Encountered typedef to itself - this is a known bindgen bug: {}", tn.to_cpp_name())?,
            ConvertError::UnexpectedUseStatement(maybe_ident) => write!(f, "Unexpected 'use' statement encountered: {}", format_maybe_identifier(maybe_ident))?,
            ConvertError::TemplatedTypeContainingNonPathArg(tn) => write!(f, "Type {} was parameterized over something complex which we don't yet support", tn)?,
            ConvertError::InvalidPointee => write!(f, "Pointer pointed to something unsupported")?,
            ConvertError::DidNotGenerateAnything(directive) => write!(f, "The 'generate' or 'generate_pod' directive for '{}' did not result in any code being generated. Perhaps this was mis-spelled or you didn't qualify the name with any namespaces? Otherwise please report a bug.", directive)?,
            ConvertError::TypeContainingForwardDeclaration(tn) => write!(f, "Found an attempt at using a forward declaration ({}) inside a templated cxx type such as UniquePtr or CxxVector", tn.to_cpp_name())?,
            ConvertError::Blocked(tn) => write!(f, "Found an attempt at using a type marked as blocked! ({})", tn.to_cpp_name())?,
            ConvertError::UnusedTemplateParam => write!(f, "This function or method uses a type where one of the template parameters was incomprehensible to bindgen/autocxx - probably because it uses template specialization.")?,
            ConvertError::TooManyUnderscores => write!(f, "Names containing __ are reserved by C++ so not acceptable to cxx")?,
            ConvertError::UnknownDependentType(qn) => write!(f, "This item relies on a type not known to autocxx ({})", qn.to_cpp_name())?,
            ConvertError::IgnoredDependent(qns) => write!(f, "This item depends on some other type(s) which autocxx could not generate, some of them are: {}", qns.iter().join(", "))?,
            ConvertError::ReservedName(id) => write!(f, "The item name '{}' is a reserved word in Rust.", id)?,
            ConvertError::DuplicateCxxBridgeName => write!(f, "This item name is used in multiple namespaces. At present, autocxx and cxx allow only one type of a given name. This limitation will be fixed in future.")?,
            ConvertError::UnsupportedReceiver => write!(f, "This is a method on a type which can't be used as the receiver in Rust (i.e. self/this). This is probably because some type involves template specialization.")?,
            ConvertError::BoxContainingNonRustType(ty) => write!(f, "A rust::Box<T> was encountered where T was not known to be a Rust type. Use rust_type!(T): {}", ty.to_cpp_name())?,
            ConvertError::RustTypeWithAPath(ty) => write!(f, "A qualified Rust type was found (i.e. one containing ::): {}. Rust types must always be a simple identifier.", ty.to_cpp_name())?,
            ConvertError::AbstractNestedType => write!(f, "This type is nested within another struct/class, yet is abstract (or is not on the allowlist so we can't be sure). This is not yet supported by autocxx. If you don't believe this type is abstract, add it to the allowlist.")?,
            ConvertError::NonPublicNestedType => write!(f, "This type is nested within another struct/class with protected or private visibility.")?,
            ConvertError::RValueParam => write!(f, "This function takes an rvalue reference parameter (&&) which is not yet supported.")?,
            ConvertError::RValueReturn => write!(f, "This function returns an rvalue reference (&&) which is not yet supported.")?,
            ConvertError::PrivateMethod => write!(f, "This method is private")?,
            ConvertError::AssignmentOperator => write!(f, "autocxx does not know how to generate bindings to operator=")?,
            ConvertError::Deleted => write!(f, "This function was marked =delete")?,
            ConvertError::RValueReferenceField => write!(f, "This structure has an rvalue reference field (&&) which is not yet supported.")?,
            ConvertError::MethodOfNonAllowlistedType => write!(f, "This type was not on the allowlist, so we are not generating methods for it.")?,
            ConvertError::MethodOfGenericType => write!(f, "This type is templated, so we can't generate bindings. We will instead generate bindings for each instantiation.")?,
            ConvertError::DuplicateItemsFoundInParsing => write!(f, "bindgen generated multiple different APIs (functions/types) with this name. autocxx doesn't know how to diambiguate them, so we won't generate bindings for any of them.")?,
            ConvertError::ConstructorWithOnlyOneParam => write!(f, "bindgen generated a move or copy constructor with an unexpected number of parameters.")?,
        }
        Ok(())
    }
}

/// Ensures that error contexts are always created using the constructors in this
/// mod, therefore undergoing identifier sanitation.
#[derive(Clone)]
struct PhantomSanitized;

/// The context of an error, e.g. whether it applies to a function or a method.
/// This is used to generate suitable rustdoc in the output codegen so that
/// the errors can be revealed in rust-analyzer-based IDEs, etc.
#[derive(Clone)]
pub(crate) struct ErrorContext(ErrorContextType, PhantomSanitized);

/// All idents in this structure are guaranteed to be something we can safely codegen for.
#[derive(Clone)]
pub(crate) enum ErrorContextType {
    Item(Ident),
    SanitizedItem(Ident),
    Method { self_ty: Ident, method: Ident },
}

impl ErrorContext {
    pub(crate) fn new_for_item(id: Ident) -> Self {
        match Self::sanitize_error_ident(&id) {
            None => Self(ErrorContextType::Item(id), PhantomSanitized),
            Some(sanitized) => Self(ErrorContextType::SanitizedItem(sanitized), PhantomSanitized),
        }
    }

    pub(crate) fn new_for_method(self_ty: Ident, method: Ident) -> Self {
        // If this IgnoredItem relates to a method on a self_ty which we can't represent,
        // e.g. u8, then forget about trying to attach this error text to something within
        // an impl block.
        match Self::sanitize_error_ident(&self_ty) {
            None => Self(
                ErrorContextType::Method {
                    self_ty,
                    method: Self::sanitize_error_ident(&method).unwrap_or(method),
                },
                PhantomSanitized,
            ),
            Some(_) => Self(
                ErrorContextType::SanitizedItem(make_ident(format!("{}_{}", self_ty, method))),
                PhantomSanitized,
            ),
        }
    }

    /// Because errors may be generated for invalid types or identifiers,
    /// we may need to scrub the name
    fn sanitize_error_ident(id: &Ident) -> Option<Ident> {
        let qn = QualifiedName::new(&Namespace::new(), id.clone());
        if known_types().conflicts_with_built_in_type(&qn) {
            Some(make_ident(format!("{}_autocxx_error", qn.get_final_item())))
        } else {
            None
        }
    }

    pub(crate) fn get_type(&self) -> &ErrorContextType {
        &self.0
    }

    pub(crate) fn into_type(self) -> ErrorContextType {
        self.0
    }
}

impl std::fmt::Display for ErrorContext {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match &self.0 {
            ErrorContextType::Item(id) | ErrorContextType::SanitizedItem(id) => write!(f, "{}", id),
            ErrorContextType::Method { self_ty, method } => write!(f, "{}::{}", self_ty, method),
        }
    }
}

#[derive(Clone)]
pub(crate) struct ConvertErrorWithContext(pub(crate) ConvertError, pub(crate) Option<ErrorContext>);

impl std::fmt::Debug for ConvertErrorWithContext {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl std::fmt::Display for ConvertErrorWithContext {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}
