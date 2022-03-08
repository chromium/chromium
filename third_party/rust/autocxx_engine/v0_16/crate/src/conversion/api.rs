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

use std::collections::HashSet;

use crate::types::{make_ident, Namespace, QualifiedName};
use autocxx_parser::RustPath;
use syn::{
    parse::Parse, punctuated::Punctuated, token::Comma, Attribute, FnArg, Ident, ImplItem,
    ItemConst, ItemEnum, ItemStruct, ItemType, ItemUse, LitBool, LitInt, ReturnType, Signature,
    Type, Visibility,
};

use super::{
    analysis::fun::{function_wrapper::CppFunction, ReceiverMutability},
    convert_error::{ConvertErrorWithContext, ErrorContext},
    ConvertError,
};

#[derive(Copy, Clone, Eq, PartialEq)]
pub(crate) enum TypeKind {
    Pod,    // trivial. Can be moved and copied in Rust.
    NonPod, // has destructor or non-trivial move constructors. Can only hold by UniquePtr
    Abstract, // has pure virtual members - can't even generate UniquePtr.
            // It's possible that the type itself isn't pure virtual, but it inherits from
            // some other type which is pure virtual. Alternatively, maybe we just don't
            // know if the base class is pure virtual because it wasn't on the allowlist,
            // in which case we'll err on the side of caution.
}

/// An entry which needs to go into an `impl` block for a given type.
pub(crate) struct ImplBlockDetails {
    pub(crate) item: ImplItem,
    pub(crate) ty: Ident,
}

/// C++ visibility.
#[derive(Debug, Clone, PartialEq, Eq, Copy)]
pub(crate) enum CppVisibility {
    Public,
    Protected,
    Private,
}

/// Details about a C++ struct.
pub(crate) struct StructDetails {
    pub(crate) vis: CppVisibility,
    pub(crate) item: ItemStruct,
    pub(crate) layout: Option<Layout>,
    pub(crate) has_rvalue_reference_fields: bool,
}

/// Layout of a type, equivalent to the same type in ir/layout.rs in bindgen
#[derive(Clone)]
pub(crate) struct Layout {
    /// The size (in bytes) of this layout.
    pub(crate) size: usize,
    /// The alignment (in bytes) of this layout.
    pub(crate) align: usize,
    /// Whether this layout's members are packed or not.
    pub(crate) packed: bool,
}

impl Parse for Layout {
    fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
        let size: LitInt = input.parse()?;
        input.parse::<syn::token::Comma>()?;
        let align: LitInt = input.parse()?;
        input.parse::<syn::token::Comma>()?;
        let packed: LitBool = input.parse()?;
        Ok(Layout {
            size: size.base10_parse().unwrap(),
            align: align.base10_parse().unwrap(),
            packed: packed.value(),
        })
    }
}

#[derive(Clone)]
pub(crate) enum Virtualness {
    None,
    Virtual,
    PureVirtual,
}

#[derive(Clone, Copy)]
pub(crate) enum CastMutability {
    ConstToConst,
    MutToConst,
    MutToMut,
}

/// Indicates that this function didn't exist originally in the C++
/// but we've created it afresh.
#[derive(Clone)]
pub(crate) enum Synthesis {
    MakeUnique,
    SubclassConstructor {
        subclass: SubclassName,
        cpp_impl: Box<CppFunction>,
        is_trivial: bool,
    },
    Cast {
        to_type: QualifiedName,
        mutable: CastMutability,
    },
}

/// Information about references (as opposed to pointers) to be found
/// within the function signature. This is derived from bindgen annotations
/// which is why it's not within `FuncToConvert::inputs`
#[derive(Default, Clone)]
pub(crate) struct References {
    pub(crate) rvalue_ref_params: HashSet<Ident>,
    pub(crate) ref_params: HashSet<Ident>,
    pub(crate) ref_return: bool,
    pub(crate) rvalue_ref_return: bool,
}

impl References {
    pub(crate) fn new_with_this_and_return_as_reference() -> Self {
        Self {
            ref_return: true,
            ref_params: [make_ident("this")].into_iter().collect(),
            ..Default::default()
        }
    }
}

#[derive(Clone, Hash, Eq, PartialEq)]
pub(crate) enum SpecialMemberKind {
    DefaultConstructor,
    CopyConstructor,
    MoveConstructor,
    Destructor,
    AssignmentOperator,
}

/// A C++ function for which we need to generate bindings, but haven't
/// yet analyzed in depth. This is little more than a `ForeignItemFn`
/// broken down into its constituent parts, plus some metadata from the
/// surrounding bindgen parsing context.
///
/// Some parts of the code synthesize additional functions and then
/// pass them through the same pipeline _as if_ they were discovered
/// during normal bindgen parsing. If that happens, they'll create one
/// of these structures, and typically fill in some of the
/// `synthesized_*` members which are not filled in from bindgen.
#[derive(Clone)]
pub(crate) struct FuncToConvert {
    pub(crate) ident: Ident,
    pub(crate) doc_attr: Option<Attribute>,
    pub(crate) inputs: Punctuated<FnArg, Comma>,
    pub(crate) output: ReturnType,
    pub(crate) vis: Visibility,
    pub(crate) virtualness: Virtualness,
    pub(crate) cpp_vis: CppVisibility,
    pub(crate) special_member: Option<SpecialMemberKind>,
    pub(crate) unused_template_param: bool,
    pub(crate) references: References,
    pub(crate) original_name: Option<String>,
    /// Used for static functions only. For all other functons,
    /// this is figured out from the receiver type in the inputs.
    pub(crate) self_ty: Option<QualifiedName>,
    /// If we wish to use a different 'this' type than the original
    /// method receiver, e.g. because we're making a subclass
    /// constructor, fill it in here.
    pub(crate) synthesized_this_type: Option<QualifiedName>,
    /// If Some, this function didn't really exist in the original
    /// C++ and instead we're synthesizing it.
    pub(crate) synthesis: Option<Synthesis>,
    pub(crate) is_deleted: bool,
}

/// Layers of analysis which may be applied to decorate each API.
/// See description of the purpose of this trait within `Api`.
pub(crate) trait AnalysisPhase {
    type TypedefAnalysis;
    type StructAnalysis;
    type FunAnalysis;
}

/// No analysis has been applied to this API.
pub(crate) struct NullPhase;

impl AnalysisPhase for NullPhase {
    type TypedefAnalysis = ();
    type StructAnalysis = ();
    type FunAnalysis = ();
}

#[derive(Clone)]
pub(crate) enum TypedefKind {
    Use(ItemUse),
    Type(ItemType),
}

/// Name information for an API. This includes the name by
/// which we know it in Rust, and its C++ name, which may differ.
#[derive(Clone, Hash, PartialEq, Eq)]
pub(crate) struct ApiName {
    pub(crate) name: QualifiedName,
    cpp_name: Option<String>,
}

impl ApiName {
    pub(crate) fn new(ns: &Namespace, id: Ident) -> Self {
        Self::new_from_qualified_name(QualifiedName::new(ns, id))
    }

    pub(crate) fn new_with_cpp_name(ns: &Namespace, id: Ident, cpp_name: Option<String>) -> Self {
        Self {
            name: QualifiedName::new(ns, id),
            cpp_name,
        }
    }

    pub(crate) fn new_from_qualified_name(name: QualifiedName) -> Self {
        Self {
            name,
            cpp_name: None,
        }
    }

    pub(crate) fn new_in_root_namespace(id: Ident) -> Self {
        Self::new(&Namespace::new(), id)
    }

    pub(crate) fn cpp_name(&self) -> String {
        self.cpp_name
            .as_ref()
            .cloned()
            .unwrap_or_else(|| self.name.get_final_item().to_string())
    }

    pub(crate) fn cpp_name_if_present(&self) -> Option<&String> {
        self.cpp_name.as_ref()
    }
}

impl std::fmt::Debug for ApiName {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)?;
        if let Some(cpp_name) = &self.cpp_name {
            write!(f, " (cpp={})", cpp_name)?;
        }
        Ok(())
    }
}

/// A name representing a subclass.
/// This is a simple newtype wrapper which exists such that
/// we can consistently generate the names of the various subsidiary
/// types which are required both in C++ and Rust codegen.
#[derive(Clone, Hash, PartialEq, Eq, Debug)]
pub(crate) struct SubclassName(pub(crate) ApiName);

impl SubclassName {
    pub(crate) fn new(id: Ident) -> Self {
        Self(ApiName::new_in_root_namespace(id))
    }
    pub(crate) fn from_holder_name(id: &Ident) -> Self {
        Self::new(make_ident(id.to_string().strip_suffix("Holder").unwrap()))
    }
    pub(crate) fn id(&self) -> Ident {
        self.0.name.get_final_ident()
    }
    /// Generate the name for the 'Holder' type
    pub(crate) fn holder(&self) -> Ident {
        self.with_suffix("Holder")
    }
    /// Generate the name for the 'Cpp' type
    pub(crate) fn cpp(&self) -> QualifiedName {
        let id = self.with_suffix("Cpp");
        QualifiedName::new(self.0.name.get_namespace(), id)
    }
    pub(crate) fn cpp_remove_ownership(&self) -> Ident {
        self.with_suffix("Cpp_remove_ownership")
    }
    pub(crate) fn remove_ownership(&self) -> Ident {
        self.with_suffix("_remove_ownership")
    }
    fn with_suffix(&self, suffix: &str) -> Ident {
        make_ident(format!("{}{}", self.0.name.get_final_item(), suffix))
    }
    pub(crate) fn get_super_fn_name(superclass_namespace: &Namespace, id: &str) -> QualifiedName {
        let id = make_ident(format!("{}_super", id));
        QualifiedName::new(superclass_namespace, id)
    }
    pub(crate) fn get_methods_trait_name(superclass_name: &QualifiedName) -> QualifiedName {
        Self::with_qualified_name_suffix(superclass_name, "methods")
    }
    pub(crate) fn get_supers_trait_name(superclass_name: &QualifiedName) -> QualifiedName {
        Self::with_qualified_name_suffix(superclass_name, "supers")
    }

    fn with_qualified_name_suffix(name: &QualifiedName, suffix: &str) -> QualifiedName {
        let id = make_ident(format!("{}_{}", name.get_final_item(), suffix));
        QualifiedName::new(name.get_namespace(), id)
    }
}

#[derive(strum_macros::Display)]
/// Different types of API we might encounter.
///
/// This type is parameterized over an `ApiAnalysis`. This is any additional
/// information which we wish to apply to our knowledge of our APIs later
/// during analysis phases.
///
/// This is not as high-level as the equivalent types in `cxx` or `bindgen`,
/// because sometimes we pass on the `bindgen` output directly in the
/// Rust codegen output.
///
/// This derives from [strum_macros::Display] because we want to be
/// able to debug-print the enum discriminant without worrying about
/// the fact that their payloads may not be `Debug` or `Display`.
/// (Specifically, allowing `syn` Types to be `Debug` requires
/// enabling syn's `extra-traits` feature which increases compile time.)
pub(crate) enum Api<T: AnalysisPhase> {
    /// A forward declared type for which no definition is available.
    ForwardDeclaration { name: ApiName },
    /// A synthetic type we've manufactured in order to
    /// concretize some templated C++ type.
    ConcreteType {
        name: ApiName,
        rs_definition: Box<Type>,
        cpp_definition: String,
    },
    /// A simple note that we want to make a constructor for
    /// a `std::string` on the heap.
    StringConstructor { name: ApiName },
    /// A function. May include some analysis.
    Function {
        name: ApiName,
        name_for_gc: Option<QualifiedName>,
        fun: Box<FuncToConvert>,
        analysis: T::FunAnalysis,
    },
    /// A constant.
    Const {
        name: ApiName,
        const_item: ItemConst,
    },
    /// A typedef found in the bindgen output which we wish
    /// to pass on in our output
    Typedef {
        name: ApiName,
        item: TypedefKind,
        old_tyname: Option<QualifiedName>,
        analysis: T::TypedefAnalysis,
    },
    /// An enum encountered in the
    /// `bindgen` output.
    Enum { name: ApiName, item: ItemEnum },
    /// A struct encountered in the
    /// `bindgen` output.
    Struct {
        name: ApiName,
        details: Box<StructDetails>,
        analysis: T::StructAnalysis,
    },
    /// A variable-length C integer type (e.g. int, unsigned long).
    CType {
        name: ApiName,
        typename: QualifiedName,
    },
    /// Some item which couldn't be processed by autocxx for some reason.
    /// We will have emitted a warning message about this, but we want
    /// to mark that it's ignored so that we don't attempt to process
    /// dependent items.
    IgnoredItem {
        name: ApiName,
        err: ConvertError,
        ctx: ErrorContext,
    },
    /// A Rust type which is not a C++ type.
    RustType { name: ApiName, path: RustPath },
    /// A function for the 'extern Rust' block which is not a C++ type.
    RustFn {
        name: ApiName,
        sig: Signature,
        path: RustPath,
    },
    /// Some function for the extern "Rust" block.
    RustSubclassFn {
        name: ApiName,
        subclass: SubclassName,
        details: Box<RustSubclassFnDetails>,
    },
    /// A Rust subclass of a C++ class.
    Subclass {
        name: SubclassName,
        superclass: QualifiedName,
    },
}

pub(crate) struct RustSubclassFnDetails {
    pub(crate) params: Punctuated<FnArg, Comma>,
    pub(crate) ret: ReturnType,
    pub(crate) cpp_impl: CppFunction,
    pub(crate) method_name: Ident,
    pub(crate) superclass: QualifiedName,
    pub(crate) receiver_mutability: ReceiverMutability,
    pub(crate) dependency: Option<QualifiedName>,
    pub(crate) requires_unsafe: bool,
    pub(crate) is_pure_virtual: bool,
}

impl<T: AnalysisPhase> Api<T> {
    pub(crate) fn name_info(&self) -> &ApiName {
        match self {
            Api::ForwardDeclaration { name } => name,
            Api::ConcreteType { name, .. } => name,
            Api::StringConstructor { name } => name,
            Api::Function { name, .. } => name,
            Api::Const { name, .. } => name,
            Api::Typedef { name, .. } => name,
            Api::Enum { name, .. } => name,
            Api::Struct { name, .. } => name,
            Api::CType { name, .. } => name,
            Api::IgnoredItem { name, .. } => name,
            Api::RustType { name, .. } => name,
            Api::RustFn { name, .. } => name,
            Api::RustSubclassFn { name, .. } => name,
            Api::Subclass { name, .. } => &name.0,
        }
    }

    /// The name of this API as used in Rust code.
    /// For types, it's important that this never changes, since
    /// functions or other types may refer to this.
    /// Yet for functions, this may not actually be the name
    /// used in the [cxx::bridge] mod -  see
    /// [Api<FnAnalysis>::cxxbridge_name]
    pub(crate) fn name(&self) -> &QualifiedName {
        &self.name_info().name
    }

    /// The name recorded for use in C++, if and only if
    /// it differs from Rust.
    pub(crate) fn cpp_name(&self) -> &Option<String> {
        &self.name_info().cpp_name
    }

    /// The name for use in C++, whether or not it differs
    /// from Rust.
    pub(crate) fn effective_cpp_name(&self) -> &str {
        self.cpp_name()
            .as_deref()
            .unwrap_or_else(|| self.name().get_final_item())
    }

    pub(crate) fn valid_types(&self) -> Box<dyn Iterator<Item = QualifiedName>> {
        match self {
            Api::Subclass { name, .. } => Box::new(
                vec![
                    self.name().clone(),
                    QualifiedName::new(&Namespace::new(), name.holder()),
                    name.cpp(),
                ]
                .into_iter(),
            ),
            _ => Box::new(std::iter::once(self.name().clone())),
        }
    }
}

impl<T: AnalysisPhase> std::fmt::Debug for Api<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{:?} (kind={})", self.name_info(), self)
    }
}

pub(crate) type UnanalyzedApi = Api<NullPhase>;

impl<T: AnalysisPhase> Api<T> {
    pub(crate) fn typedef_unchanged(
        name: ApiName,
        item: TypedefKind,
        old_tyname: Option<QualifiedName>,
        analysis: T::TypedefAnalysis,
    ) -> Result<Box<dyn Iterator<Item = Api<T>>>, ConvertErrorWithContext>
    where
        T: 'static,
    {
        Ok(Box::new(std::iter::once(Api::Typedef {
            name,
            item,
            old_tyname,
            analysis,
        })))
    }

    pub(crate) fn struct_unchanged(
        name: ApiName,
        details: Box<StructDetails>,
        analysis: T::StructAnalysis,
    ) -> Result<Box<dyn Iterator<Item = Api<T>>>, ConvertErrorWithContext>
    where
        T: 'static,
    {
        Ok(Box::new(std::iter::once(Api::Struct {
            name,
            details,
            analysis,
        })))
    }

    pub(crate) fn fun_unchanged(
        name: ApiName,
        fun: Box<FuncToConvert>,
        analysis: T::FunAnalysis,
        name_for_gc: Option<QualifiedName>,
    ) -> Result<Box<dyn Iterator<Item = Api<T>>>, ConvertErrorWithContext>
    where
        T: 'static,
    {
        Ok(Box::new(std::iter::once(Api::Function {
            name,
            fun,
            analysis,
            name_for_gc,
        })))
    }

    pub(crate) fn enum_unchanged(
        name: ApiName,
        item: ItemEnum,
    ) -> Result<Box<dyn Iterator<Item = Api<T>>>, ConvertErrorWithContext>
    where
        T: 'static,
    {
        Ok(Box::new(std::iter::once(Api::Enum { name, item })))
    }
}
