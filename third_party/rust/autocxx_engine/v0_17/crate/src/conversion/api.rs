// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::collections::HashSet;

use crate::types::{make_ident, Namespace, QualifiedName};
use autocxx_parser::RustPath;
use itertools::Itertools;
use quote::ToTokens;
use syn::{
    parse::Parse,
    punctuated::Punctuated,
    token::{Comma, Unsafe},
    Attribute, FnArg, Ident, ItemConst, ItemEnum, ItemStruct, ItemType, ItemUse, LitBool, LitInt,
    Pat, ReturnType, Signature, Type, Visibility,
};

use super::{
    analysis::fun::{
        function_wrapper::{CppFunction, CppFunctionBody, CppFunctionKind},
        ReceiverMutability,
    },
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

/// Indicates that this function (which is synthetic) should
/// be a trait implementation rather than a method or free function.
#[derive(Clone)]
pub(crate) enum TraitSynthesis {
    Cast {
        to_type: QualifiedName,
        mutable: CastMutability,
    },
    AllocUninitialized(QualifiedName),
    FreeUninitialized(QualifiedName),
}

/// Details of a subclass constructor.
/// TODO: zap this; replace with an extra API.
#[derive(Clone)]
pub(crate) struct SubclassConstructorDetails {
    pub(crate) subclass: SubclassName,
    pub(crate) is_trivial: bool,
    /// Implementation of the constructor _itself_ as distinct
    /// from any wrapper function we create to call it.
    pub(crate) cpp_impl: CppFunction,
}

/// Contributions to traits representing C++ superclasses that
/// we may implement as Rust subclasses.
#[derive(Clone)]
pub(crate) struct SuperclassMethod {
    pub(crate) name: Ident,
    pub(crate) receiver: QualifiedName,
    pub(crate) params: Punctuated<FnArg, Comma>,
    pub(crate) param_names: Vec<Pat>,
    pub(crate) ret_type: ReturnType,
    pub(crate) receiver_mutability: ReceiverMutability,
    pub(crate) requires_unsafe: UnsafetyNeeded,
    pub(crate) is_pure_virtual: bool,
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

#[derive(Clone)]
pub(crate) struct TraitImplSignature {
    pub(crate) ty: Type,
    pub(crate) trait_signature: Type,
    /// The trait is 'unsafe' itself
    pub(crate) unsafety: Option<Unsafe>,
}

impl Eq for TraitImplSignature {}

impl PartialEq for TraitImplSignature {
    fn eq(&self, other: &Self) -> bool {
        totokens_equal(&self.unsafety, &other.unsafety)
            && totokens_equal(&self.ty, &other.ty)
            && totokens_equal(&self.trait_signature, &other.trait_signature)
    }
}

fn totokens_to_string<T: ToTokens>(a: &T) -> String {
    a.to_token_stream().to_string()
}

fn totokens_equal<T: ToTokens>(a: &T, b: &T) -> bool {
    totokens_to_string(a) == totokens_to_string(b)
}

fn hash_totokens<T: ToTokens, H: std::hash::Hasher>(a: &T, state: &mut H) {
    use std::hash::Hash;
    totokens_to_string(a).hash(state)
}

impl std::hash::Hash for TraitImplSignature {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        hash_totokens(&self.ty, state);
        hash_totokens(&self.trait_signature, state);
        hash_totokens(&self.unsafety, state);
    }
}

#[derive(Clone, Debug)]
pub(crate) enum SpecialMemberKind {
    DefaultConstructor,
    CopyConstructor,
    MoveConstructor,
    Destructor,
    AssignmentOperator,
}

#[derive(Clone)]
pub(crate) enum Provenance {
    Bindgen,
    SynthesizedOther,
    SynthesizedMakeUnique,
    SynthesizedSubclassConstructor(Box<SubclassConstructorDetails>),
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
    pub(crate) provenance: Provenance,
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
    /// If this function should actually belong to a trait.
    pub(crate) add_to_trait: Option<TraitSynthesis>,
    /// If Some, this function didn't really exist in the original
    /// C++ and instead we're synthesizing it.
    pub(crate) synthetic_cpp: Option<(CppFunctionBody, CppFunctionKind)>,
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

    pub(crate) fn qualified_cpp_name(&self) -> String {
        let cpp_name = self.cpp_name();
        self.name
            .ns_segment_iter()
            .cloned()
            .chain(std::iter::once(cpp_name))
            .join("::")
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
    pub(crate) fn get_trait_api_name(sup: &QualifiedName, method_name: &str) -> QualifiedName {
        QualifiedName::new(
            sup.get_namespace(),
            make_ident(format!(
                "{}_{}_trait_item",
                sup.get_final_item(),
                method_name
            )),
        )
    }
    // TODO this and the following should probably include both class name and method name
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
        ctx: Option<ErrorContext>,
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
    /// Contributions to the traits representing superclass methods that we might
    /// subclass in Rust.
    SubclassTraitItem {
        name: ApiName,
        details: SuperclassMethod,
    },
}

pub(crate) struct RustSubclassFnDetails {
    pub(crate) params: Punctuated<FnArg, Comma>,
    pub(crate) ret: ReturnType,
    pub(crate) cpp_impl: CppFunction,
    pub(crate) method_name: Ident,
    pub(crate) superclass: QualifiedName,
    pub(crate) receiver_mutability: ReceiverMutability,
    pub(crate) dependencies: Vec<QualifiedName>,
    pub(crate) requires_unsafe: UnsafetyNeeded,
    pub(crate) is_pure_virtual: bool,
}

#[derive(Clone, Debug)]
pub(crate) enum UnsafetyNeeded {
    None,
    JustBridge,
    Always,
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
            Api::SubclassTraitItem { name, .. } => name,
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

    /// If this API turns out to have the same QualifiedName as another,
    /// whether it's OK to just discard it?
    pub(crate) fn discard_duplicates(&self) -> bool {
        matches!(self, Api::IgnoredItem { .. })
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
    ) -> Result<Box<dyn Iterator<Item = Api<T>>>, ConvertErrorWithContext>
    where
        T: 'static,
    {
        Ok(Box::new(std::iter::once(Api::Function {
            name,
            fun,
            analysis,
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
