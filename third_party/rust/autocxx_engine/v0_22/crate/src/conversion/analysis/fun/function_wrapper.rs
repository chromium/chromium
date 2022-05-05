// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::{
    conversion::api::SubclassName,
    types::{Namespace, QualifiedName},
};
use syn::{parse_quote, Ident, Type};

#[derive(Clone, Debug)]
pub(crate) enum CppConversionType {
    None,
    Move,
    FromUniquePtrToValue,
    FromPtrToValue,
    FromValueToUniquePtr,
    FromPtrToMove,
    /// Ignored in the sense that it isn't passed into the C++ function.
    IgnoredPlacementPtrParameter,
    FromReturnValueToPlacementPtr,
}

impl CppConversionType {
    /// If we've found a function which does X to its parameter, what
    /// is the opposite of X? This is used for subclasses where calls
    /// from Rust to C++ might also involve calls from C++ to Rust.
    fn inverse(&self) -> Self {
        match self {
            CppConversionType::None => CppConversionType::None,
            CppConversionType::FromUniquePtrToValue | CppConversionType::FromPtrToValue => {
                CppConversionType::FromValueToUniquePtr
            }
            CppConversionType::FromValueToUniquePtr => CppConversionType::FromUniquePtrToValue,
            _ => panic!("Did not expect to have to invert this conversion"),
        }
    }
}

#[derive(Clone, Debug)]
pub(crate) enum RustConversionType {
    None,
    FromStr,
    ToBoxedUpHolder(SubclassName),
    FromPinMaybeUninitToPtr,
    FromPinMoveRefToPtr,
    FromTypeToPtr,
    FromValueParamToPtr,
    FromPlacementParamToNewReturn,
    FromRValueParamToPtr,
}

impl RustConversionType {
    pub(crate) fn requires_mutability(&self) -> Option<syn::token::Mut> {
        match self {
            Self::FromPinMoveRefToPtr => Some(parse_quote! { mut }),
            _ => None,
        }
    }
}

/// A policy for converting types. Conversion may occur on both the Rust and
/// C++ side. The most complex example is a C++ function which takes
/// std::string by value, which might do this:
/// * Client Rust code: `&str`
/// * Rust wrapper function: converts `&str` to `UniquePtr<CxxString>`
/// * cxx::bridge mod: refers to `UniquePtr<CxxString>`
/// * C++ wrapper function converts `std::unique_ptr<std::string>` to just
///   `std::string`
/// * Finally, the actual C++ API receives a `std::string` by value.
/// The implementation here is distributed across this file, and
/// `function_wrapper_rs` and `function_wrapper_cpp`.
#[derive(Clone)]
pub(crate) struct TypeConversionPolicy {
    pub(crate) unwrapped_type: Type,
    pub(crate) cpp_conversion: CppConversionType,
    pub(crate) rust_conversion: RustConversionType,
}

impl TypeConversionPolicy {
    pub(crate) fn new_unconverted(ty: Type) -> Self {
        TypeConversionPolicy {
            unwrapped_type: ty,
            cpp_conversion: CppConversionType::None,
            rust_conversion: RustConversionType::None,
        }
    }

    pub(crate) fn new_to_unique_ptr(ty: Type) -> Self {
        TypeConversionPolicy {
            unwrapped_type: ty,
            cpp_conversion: CppConversionType::FromValueToUniquePtr,
            rust_conversion: RustConversionType::None,
        }
    }

    pub(crate) fn new_for_placement_return(ty: Type) -> Self {
        TypeConversionPolicy {
            unwrapped_type: ty,
            cpp_conversion: CppConversionType::FromReturnValueToPlacementPtr,
            // Rust conversion is marked as none here, since this policy
            // will be applied to the return value, and the Rust-side
            // shenanigans applies to the placement new *parameter*
            rust_conversion: RustConversionType::None,
        }
    }

    pub(crate) fn cpp_work_needed(&self) -> bool {
        !matches!(self.cpp_conversion, CppConversionType::None)
    }

    pub(crate) fn unconverted_rust_type(&self) -> Type {
        match self.cpp_conversion {
            CppConversionType::FromValueToUniquePtr => self.make_unique_ptr_type(),
            _ => self.unwrapped_type.clone(),
        }
    }

    pub(crate) fn converted_rust_type(&self) -> Type {
        match self.cpp_conversion {
            CppConversionType::FromUniquePtrToValue => self.make_unique_ptr_type(),
            CppConversionType::FromPtrToValue => {
                let innerty = &self.unwrapped_type;
                parse_quote! {
                    *mut #innerty
                }
            }
            _ => self.unwrapped_type.clone(),
        }
    }

    fn make_unique_ptr_type(&self) -> Type {
        let innerty = &self.unwrapped_type;
        parse_quote! {
            cxx::UniquePtr < #innerty >
        }
    }

    pub(crate) fn rust_work_needed(&self) -> bool {
        !matches!(self.rust_conversion, RustConversionType::None)
    }

    /// Subclass support involves calls from Rust -> C++, but
    /// also from C++ -> Rust. Work out the correct argument conversion
    /// type for the latter call, when given the former.
    pub(crate) fn inverse(&self) -> Self {
        Self {
            unwrapped_type: self.unwrapped_type.clone(),
            cpp_conversion: self.cpp_conversion.inverse(),
            rust_conversion: self.rust_conversion.clone(),
        }
    }

    pub(crate) fn bridge_unsafe_needed(&self) -> bool {
        matches!(
            self.rust_conversion,
            RustConversionType::FromValueParamToPtr
                | RustConversionType::FromRValueParamToPtr
                | RustConversionType::FromPlacementParamToNewReturn
        )
    }

    pub(crate) fn is_placement_parameter(&self) -> bool {
        matches!(
            self.cpp_conversion,
            CppConversionType::IgnoredPlacementPtrParameter
        )
    }

    pub(crate) fn populate_return_value(&self) -> bool {
        !matches!(
            self.cpp_conversion,
            CppConversionType::FromReturnValueToPlacementPtr
        )
    }
}

#[derive(Clone)]
pub(crate) enum CppFunctionBody {
    FunctionCall(Namespace, Ident),
    StaticMethodCall(Namespace, Ident, Ident),
    PlacementNew(Namespace, Ident),
    ConstructSuperclass(String),
    Cast,
    Destructor(Namespace, Ident),
    AllocUninitialized(QualifiedName),
    FreeUninitialized(QualifiedName),
}

#[derive(Clone)]
pub(crate) enum CppFunctionKind {
    Function,
    Method,
    Constructor,
    ConstMethod,
    SynthesizedConstructor,
}

#[derive(Clone)]
pub(crate) struct CppFunction {
    pub(crate) payload: CppFunctionBody,
    pub(crate) wrapper_function_name: Ident,
    pub(crate) original_cpp_name: String,
    pub(crate) return_conversion: Option<TypeConversionPolicy>,
    pub(crate) argument_conversion: Vec<TypeConversionPolicy>,
    pub(crate) kind: CppFunctionKind,
    pub(crate) pass_obs_field: bool,
    pub(crate) qualification: Option<QualifiedName>,
}
