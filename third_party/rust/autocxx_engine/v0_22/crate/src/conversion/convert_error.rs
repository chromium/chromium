// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use indexmap::set::IndexSet as HashSet;

use itertools::Itertools;
use syn::Ident;
use thiserror::Error;

use crate::{
    known_types,
    types::{make_ident, Namespace, QualifiedName},
};

#[derive(Debug, Clone, Error)]
pub enum ConvertError {
    #[error("The initial run of 'bindgen' did not generate any content. This might be because none of the requested items for generation could be converted.")]
    NoContent,
    #[error("An item was requested using 'generate_pod' which was not safe to hold by value in Rust. {0}")]
    UnsafePodType(String),
    #[error("Bindgen generated some unexpected code in a foreign mod section. You may have specified something in a 'generate' directive which is not currently compatible with autocxx.")]
    UnexpectedForeignItem,
    #[error("Bindgen generated some unexpected code in its outermost mod section. You may have specified something in a 'generate' directive which is not currently compatible with autocxx.")]
    UnexpectedOuterItem,
    #[error("Bindgen generated some unexpected code in an inner namespace mod. You may have specified something in a 'generate' directive which is not currently compatible with autocxx.")]
    UnexpectedItemInMod,
    #[error("autocxx was unable to produce a typdef pointing to the complex type {0}.")]
    ComplexTypedefTarget(String),
    #[error("Unexpected type for 'this' in the function {}.", .0.to_cpp_name())]
    UnexpectedThisType(QualifiedName),
    #[error("autocxx does not yet know how to support the built-in C++ type {} - please raise an issue on github", .0.to_cpp_name())]
    UnsupportedBuiltInType(QualifiedName),
    #[error("Type {} has templated arguments and so does the typedef to which it points", .0.to_cpp_name())]
    ConflictingTemplatedArgsWithTypedef(QualifiedName),
    #[error("Function {0} has a parameter or return type which is either on the blocklist or a forward declaration")]
    UnacceptableParam(String),
    #[error("Function {0} has a return reference parameter, but 0 or >1 input reference parameters, so the lifetime of the output reference cannot be deduced.")]
    NotOneInputReference(String),
    #[error("Encountered type not yet supported by autocxx: {0}")]
    UnsupportedType(String),
    #[error("Encountered type not yet known by autocxx: {0}")]
    UnknownType(String),
    #[error("Encountered mutable static data, not yet supported: {0}")]
    StaticData(String),
    #[error("Encountered typedef to itself - this is a known bindgen bug: {0}")]
    InfinitelyRecursiveTypedef(QualifiedName),
    #[error("Unexpected 'use' statement encountered: {}", .0.as_ref().map(|s| s.as_str()).unwrap_or("<unknown>"))]
    UnexpectedUseStatement(Option<String>),
    #[error("Type {} was parameterized over something complex which we don't yet support", .0.to_cpp_name())]
    TemplatedTypeContainingNonPathArg(QualifiedName),
    #[error("Pointer pointed to something unsupported")]
    InvalidPointee,
    #[error("The 'generate' or 'generate_pod' directive for '{0}' did not result in any code being generated. Perhaps this was mis-spelled or you didn't qualify the name with any namespaces? Otherwise please report a bug.")]
    DidNotGenerateAnything(String),
    #[error("Found an attempt at using a forward declaration ({}) inside a templated cxx type such as UniquePtr or CxxVector. If the forward declaration is a typedef, perhaps autocxx wasn't sure whether or not it involved a forward declaration. If you're sure it didn't, then you may be able to solve this by using instantiable!.", .0.to_cpp_name())]
    TypeContainingForwardDeclaration(QualifiedName),
    #[error("Found an attempt at using a type marked as blocked! ({})", .0.to_cpp_name())]
    Blocked(QualifiedName),
    #[error("This function or method uses a type where one of the template parameters was incomprehensible to bindgen/autocxx - probably because it uses template specialization.")]
    UnusedTemplateParam,
    #[error("Names containing __ are reserved by C++ so not acceptable to cxx")]
    TooManyUnderscores,
    #[error("This item relies on a type not known to autocxx ({})", .0.to_cpp_name())]
    UnknownDependentType(QualifiedName),
    #[error("This item depends on some other type(s) which autocxx could not generate, some of them are: {}", .0.iter().join(", "))]
    IgnoredDependent(HashSet<QualifiedName>),
    #[error("The item name '{0}' is a reserved word in Rust.")]
    ReservedName(String),
    #[error("This item name is used in multiple namespaces. At present, autocxx and cxx allow only one type of a given name. This limitation will be fixed in future. (Items found with this name: {})", .0.iter().join(", "))]
    DuplicateCxxBridgeName(Vec<String>),
    #[error("This is a method on a type which can't be used as the receiver in Rust (i.e. self/this). This is probably because some type involves template specialization.")]
    UnsupportedReceiver,
    #[error("A rust::Box<T> was encountered where T was not known to be a Rust type. Use rust_type!(T): {}", .0.to_cpp_name())]
    BoxContainingNonRustType(QualifiedName),
    #[error("A qualified Rust type was found (i.e. one containing ::): {}. Rust types must always be a simple identifier.", .0.to_cpp_name())]
    RustTypeWithAPath(QualifiedName),
    #[error("This type is nested within another struct/class, yet is abstract (or is not on the allowlist so we can't be sure). This is not yet supported by autocxx. If you don't believe this type is abstract, add it to the allowlist.")]
    AbstractNestedType,
    #[error("This typedef was nested within another struct/class. autocxx is unable to represent inner types if they might be abstract. Unfortunately, autocxx couldn't prove that this type isn't abstract, so it can't represent it.")]
    NestedOpaqueTypedef,
    #[error(
        "This type is nested within another struct/class with protected or private visibility."
    )]
    NonPublicNestedType,
    #[error("This function returns an rvalue reference (&&) which is not yet supported.")]
    RValueReturn,
    #[error("This method is private")]
    PrivateMethod,
    #[error("autocxx does not know how to generate bindings to operator=")]
    AssignmentOperator,
    #[error("This function was marked =delete")]
    Deleted,
    #[error("This structure has an rvalue reference field (&&) which is not yet supported.")]
    RValueReferenceField,
    #[error("This type was not on the allowlist, so we are not generating methods for it.")]
    MethodOfNonAllowlistedType,
    #[error("This type is templated, so we can't generate bindings. We will instead generate bindings for each instantiation.")]
    MethodOfGenericType,
    #[error("bindgen generated multiple different APIs (functions/types) with this name. autocxx doesn't know how to disambiguate them, so we won't generate bindings for any of them.")]
    DuplicateItemsFoundInParsing,
    #[error(
        "bindgen generated a move or copy constructor with an unexpected number of parameters."
    )]
    ConstructorWithOnlyOneParam,
    #[error("A copy or move constructor was found to take extra parameters. These are likely to be parameters with defaults, which are not yet supported by autocxx, so this constructor has been ignored.")]
    ConstructorWithMultipleParams,
    #[error("A C++ unique_ptr, shared_ptr or weak_ptr was found containing some type that cxx can't accommodate in that position ({})", .0.to_cpp_name())]
    InvalidTypeForCppPtr(QualifiedName),
    #[error("A C++ std::vector was found containing some type that cxx can't accommodate as a vector element ({})", .0.to_cpp_name())]
    InvalidTypeForCppVector(QualifiedName),
    #[error("Variadic functions are not supported by cxx or autocxx.")]
    Variadic,
    #[error("A type had a template inside a std::vector, which is not supported.")]
    GenericsWithinVector,
    #[error("This typedef takes generic parameters, not yet supported by autocxx.")]
    TypedefTakesGenericParameters,
    #[error("This method belonged to an item in an anonymous namespace, not currently supported.")]
    MethodInAnonymousNamespace,
    #[error("We're unable to make a concrete version of this template, because we found an error handling the template.")]
    ConcreteVersionOfIgnoredTemplate,
    #[error("bindgen decided to call this type _bindgen_ty_N because it couldn't deduce the correct name for it. That means we can't generate C++ bindings to it.")]
    BindgenTy,
    #[error("This is a typedef to a type in an anonymous namespace, not currently supported.")]
    TypedefToTypeInAnonymousNamespace,
    #[error("This type refers to a generic type parameter of an outer type, which is not yet supported.")]
    ReferringToGenericTypeParam,
    #[error("This forward declaration was nested within another struct/class. autocxx is unable to represent inner types if they are forward declarations.")]
    ForwardDeclaredNestedType,
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
