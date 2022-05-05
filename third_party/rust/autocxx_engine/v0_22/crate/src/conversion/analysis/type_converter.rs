// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::{
    conversion::{
        api::{AnalysisPhase, Api, ApiName, NullPhase, TypedefKind, UnanalyzedApi},
        apivec::ApiVec,
        codegen_cpp::type_to_cpp::type_to_cpp,
        ConvertError,
    },
    known_types::{known_types, CxxGenericType},
    types::{make_ident, Namespace, QualifiedName},
};
use autocxx_parser::IncludeCppConfig;
use indexmap::map::IndexMap as HashMap;
use indexmap::set::IndexSet as HashSet;
use itertools::Itertools;
use proc_macro2::Ident;
use quote::ToTokens;
use syn::{
    parse_quote, punctuated::Punctuated, token::Comma, GenericArgument, PathArguments, PathSegment,
    Type, TypePath, TypePtr,
};

use super::tdef::TypedefAnalysis;

/// Certain kinds of type may require special handling by callers.
#[derive(Debug)]
pub(crate) enum TypeKind {
    Regular,
    Pointer,
    SubclassHolder(Ident),
    Reference,
    RValueReference,
    MutableReference,
}

/// Results of some type conversion, annotated with a list of every type encountered,
/// and optionally any extra APIs we need in order to use this type.
pub(crate) struct Annotated<T> {
    pub(crate) ty: T,
    pub(crate) types_encountered: HashSet<QualifiedName>,
    pub(crate) extra_apis: ApiVec<NullPhase>,
    pub(crate) kind: TypeKind,
}

impl<T> Annotated<T> {
    fn new(
        ty: T,
        types_encountered: HashSet<QualifiedName>,
        extra_apis: ApiVec<NullPhase>,
        kind: TypeKind,
    ) -> Self {
        Self {
            ty,
            types_encountered,
            extra_apis,
            kind,
        }
    }

    fn map<T2, F: FnOnce(T) -> T2>(self, fun: F) -> Annotated<T2> {
        Annotated {
            ty: fun(self.ty),
            types_encountered: self.types_encountered,
            extra_apis: self.extra_apis,
            kind: self.kind,
        }
    }
}

/// How to interpret a pointer which we encounter during type conversion.
#[derive(Clone, Copy)]
pub(crate) enum PointerTreatment {
    Pointer,
    Reference,
    RValueReference,
}

/// Options when converting a type.
/// It's possible we could add more policies here in future.
/// For example, Rust in general allows type names containing
/// __, whereas cxx doesn't. If we could identify cases where
/// a type will only ever be used in a bindgen context,
/// we could be more liberal. At the moment though, all outputs
/// from [TypeConverter] _might_ be used in the [cxx::bridge].
pub(crate) enum TypeConversionContext {
    WithinReference,
    WithinContainer,
    OuterType { pointer_treatment: PointerTreatment },
}

impl TypeConversionContext {
    fn pointer_treatment(&self) -> PointerTreatment {
        match self {
            Self::WithinReference | Self::WithinContainer => PointerTreatment::Pointer,
            Self::OuterType { pointer_treatment } => *pointer_treatment,
        }
    }
    fn allow_instantiation_of_forward_declaration(&self) -> bool {
        matches!(self, Self::WithinReference)
    }
}

/// A type which can convert from a type encountered in `bindgen`
/// output to the sort of type we should represeent to `cxx`.
/// As a simple example, `std::string` should be replaced
/// with [CxxString]. This also involves keeping track
/// of typedefs, and any instantiated concrete types.
///
/// To do this conversion correctly, this type relies on
/// inspecting the pre-existing list of APIs.
pub(crate) struct TypeConverter<'a> {
    types_found: HashSet<QualifiedName>,
    typedefs: HashMap<QualifiedName, Type>,
    concrete_templates: HashMap<String, QualifiedName>,
    forward_declarations: HashSet<QualifiedName>,
    config: &'a IncludeCppConfig,
}

impl<'a> TypeConverter<'a> {
    pub(crate) fn new<A: AnalysisPhase>(config: &'a IncludeCppConfig, apis: &ApiVec<A>) -> Self
    where
        A::TypedefAnalysis: TypedefTarget,
    {
        Self {
            types_found: find_types(apis),
            typedefs: Self::find_typedefs(apis),
            concrete_templates: Self::find_concrete_templates(apis),
            forward_declarations: Self::find_incomplete_types(apis),
            config,
        }
    }

    pub(crate) fn convert_boxed_type(
        &mut self,
        ty: Box<Type>,
        ns: &Namespace,
        ctx: &TypeConversionContext,
    ) -> Result<Annotated<Box<Type>>, ConvertError> {
        Ok(self.convert_type(*ty, ns, ctx)?.map(Box::new))
    }

    pub(crate) fn convert_type(
        &mut self,
        ty: Type,
        ns: &Namespace,
        ctx: &TypeConversionContext,
    ) -> Result<Annotated<Type>, ConvertError> {
        let result = match ty {
            Type::Path(p) => {
                let newp = self.convert_type_path(p, ns)?;
                if let Type::Path(newpp) = &newp.ty {
                    let qn = QualifiedName::from_type_path(newpp);
                    if !ctx.allow_instantiation_of_forward_declaration()
                        && self.forward_declarations.contains(&qn)
                    {
                        return Err(ConvertError::TypeContainingForwardDeclaration(qn));
                    }
                    // Special handling because rust_Str (as emitted by bindgen)
                    // doesn't simply get renamed to a different type _identifier_.
                    // This plain type-by-value (as far as bindgen is concerned)
                    // is actually a &str.
                    if known_types().should_dereference_in_cpp(&qn) {
                        Annotated::new(
                            Type::Reference(parse_quote! {
                                &str
                            }),
                            newp.types_encountered,
                            newp.extra_apis,
                            TypeKind::Reference,
                        )
                    } else {
                        newp
                    }
                } else {
                    newp
                }
            }
            Type::Reference(mut r) => {
                let innerty =
                    self.convert_boxed_type(r.elem, ns, &TypeConversionContext::WithinReference)?;
                r.elem = innerty.ty;
                Annotated::new(
                    Type::Reference(r),
                    innerty.types_encountered,
                    innerty.extra_apis,
                    TypeKind::Reference,
                )
            }
            Type::Array(mut arr) => {
                let innerty =
                    self.convert_type(*arr.elem, ns, &TypeConversionContext::WithinReference)?;
                arr.elem = Box::new(innerty.ty);
                Annotated::new(
                    Type::Array(arr),
                    innerty.types_encountered,
                    innerty.extra_apis,
                    TypeKind::Regular,
                )
            }
            Type::Ptr(ptr) => self.convert_ptr(ptr, ns, ctx.pointer_treatment())?,
            _ => return Err(ConvertError::UnknownType(ty.to_token_stream().to_string())),
        };
        Ok(result)
    }

    fn convert_type_path(
        &mut self,
        mut typ: TypePath,
        ns: &Namespace,
    ) -> Result<Annotated<Type>, ConvertError> {
        // First, qualify any unqualified paths.
        if typ.path.segments.iter().next().unwrap().ident != "root" {
            let ty = QualifiedName::from_type_path(&typ);
            // If the type looks like it is unqualified, check we know it
            // already, and if not, qualify it according to the current
            // namespace. This is a bit of a shortcut compared to having a full
            // resolution pass which can search all known namespaces.
            if !known_types().is_known_type(&ty) {
                let num_segments = typ.path.segments.len();
                if num_segments > 1 {
                    return Err(ConvertError::UnsupportedBuiltInType(ty));
                }
                if !self.types_found.contains(&ty) {
                    typ.path.segments = std::iter::once(&"root".to_string())
                        .chain(ns.iter())
                        .map(|s| {
                            let i = make_ident(s);
                            parse_quote! { #i }
                        })
                        .chain(typ.path.segments.into_iter())
                        .collect();
                }
            }
        }

        let original_tn = QualifiedName::from_type_path(&typ);
        original_tn.validate_ok_for_cxx()?;
        if self.config.is_on_blocklist(&original_tn.to_cpp_name()) {
            return Err(ConvertError::Blocked(original_tn));
        }
        let mut deps = HashSet::new();

        // Now convert this type itself.
        deps.insert(original_tn.clone());
        // First let's see if this is a typedef.
        let (typ, tn) = match self.resolve_typedef(&original_tn)? {
            None => (typ, original_tn),
            Some(Type::Path(resolved_tp)) => {
                let resolved_tn = QualifiedName::from_type_path(resolved_tp);
                deps.insert(resolved_tn.clone());
                (resolved_tp.clone(), resolved_tn)
            }
            Some(Type::Ptr(resolved_tp)) => {
                return Ok(Annotated::new(
                    Type::Ptr(resolved_tp.clone()),
                    deps,
                    ApiVec::new(),
                    TypeKind::Pointer,
                ))
            }
            Some(other) => {
                return Ok(Annotated::new(
                    other.clone(),
                    deps,
                    ApiVec::new(),
                    TypeKind::Regular,
                ))
            }
        };

        // Now let's see if it's a known type.
        // (We may entirely reject some types at this point too.)
        let mut typ = match known_types().consider_substitution(&tn) {
            Some(mut substitute_type) => {
                if let Some(last_seg_args) =
                    typ.path.segments.into_iter().last().map(|ps| ps.arguments)
                {
                    let last_seg = substitute_type.path.segments.last_mut().unwrap();
                    last_seg.arguments = last_seg_args;
                }
                substitute_type
            }
            None => typ,
        };

        let mut extra_apis = ApiVec::new();
        let mut kind = TypeKind::Regular;

        // Finally let's see if it's generic.
        if let Some(last_seg) = Self::get_generic_args(&mut typ) {
            let generic_behavior = known_types().cxx_generic_behavior(&tn);
            let forward_declarations_ok = generic_behavior == CxxGenericType::Rust;
            if generic_behavior != CxxGenericType::Not {
                // this is a type of generic understood by cxx (e.g. CxxVector)
                // so let's convert any generic type arguments. This recurses.
                if let PathArguments::AngleBracketed(ref mut ab) = last_seg.arguments {
                    let mut innerty = self.convert_punctuated(
                        ab.args.clone(),
                        ns,
                        &TypeConversionContext::WithinContainer,
                    )?;
                    ab.args = innerty.ty;
                    kind = self.confirm_inner_type_is_acceptable_generic_payload(
                        &ab.args,
                        &tn,
                        generic_behavior,
                        forward_declarations_ok,
                    )?;
                    deps.extend(innerty.types_encountered.drain(..));
                } else {
                    return Err(ConvertError::TemplatedTypeContainingNonPathArg(tn.clone()));
                }
            } else {
                // Oh poop. It's a generic type which cxx won't be able to handle.
                // We'll have to come up with a concrete type in both the cxx::bridge (in Rust)
                // and a corresponding typedef in C++.
                let (new_tn, api) = self.get_templated_typename(&Type::Path(typ))?;
                extra_apis.extend(api.into_iter());
                deps.remove(&tn);
                typ = new_tn.to_type_path();
                deps.insert(new_tn);
            }
        }
        Ok(Annotated::new(Type::Path(typ), deps, extra_apis, kind))
    }

    fn get_generic_args(typ: &mut TypePath) -> Option<&mut PathSegment> {
        match typ.path.segments.last_mut() {
            Some(s) if !s.arguments.is_empty() => Some(s),
            _ => None,
        }
    }

    fn convert_punctuated<P>(
        &mut self,
        pun: Punctuated<GenericArgument, P>,
        ns: &Namespace,
        ctx: &TypeConversionContext,
    ) -> Result<Annotated<Punctuated<GenericArgument, P>>, ConvertError>
    where
        P: Default,
    {
        let mut new_pun = Punctuated::new();
        let mut types_encountered = HashSet::new();
        let mut extra_apis = ApiVec::new();
        for arg in pun.into_iter() {
            new_pun.push(match arg {
                GenericArgument::Type(t) => {
                    let mut innerty = self.convert_type(t, ns, ctx)?;
                    types_encountered.extend(innerty.types_encountered.drain(..));
                    extra_apis.append(&mut innerty.extra_apis);
                    GenericArgument::Type(innerty.ty)
                }
                _ => arg,
            });
        }
        Ok(Annotated::new(
            new_pun,
            types_encountered,
            extra_apis,
            TypeKind::Regular,
        ))
    }

    fn resolve_typedef<'b>(&'b self, tn: &QualifiedName) -> Result<Option<&'b Type>, ConvertError> {
        let mut encountered = HashSet::new();
        let mut tn = tn.clone();
        let mut previous_typ = None;
        loop {
            let r = self.typedefs.get(&tn);
            match r {
                Some(Type::Path(typ)) => {
                    previous_typ = r;
                    let new_tn = QualifiedName::from_type_path(typ);
                    if encountered.contains(&new_tn) {
                        return Err(ConvertError::InfinitelyRecursiveTypedef(tn.clone()));
                    }
                    encountered.insert(new_tn.clone());
                    tn = new_tn;
                }
                None => return Ok(previous_typ),
                _ => return Ok(r),
            }
        }
    }

    fn convert_ptr(
        &mut self,
        mut ptr: TypePtr,
        ns: &Namespace,
        pointer_treatment: PointerTreatment,
    ) -> Result<Annotated<Type>, ConvertError> {
        match pointer_treatment {
            PointerTreatment::Pointer => {
                crate::known_types::ensure_pointee_is_valid(&ptr)?;
                let innerty =
                    self.convert_boxed_type(ptr.elem, ns, &TypeConversionContext::WithinReference)?;
                ptr.elem = innerty.ty;
                Ok(Annotated::new(
                    Type::Ptr(ptr),
                    innerty.types_encountered,
                    innerty.extra_apis,
                    TypeKind::Pointer,
                ))
            }
            PointerTreatment::Reference => {
                let mutability = ptr.mutability;
                let elem =
                    self.convert_boxed_type(ptr.elem, ns, &TypeConversionContext::WithinReference)?;
                // TODO - in the future, we should check if this is a rust::Str and throw
                // a wobbler if not. rust::Str should only be seen _by value_ in C++
                // headers; it manifests as &str in Rust but on the C++ side it must
                // be a plain value. We should detect and abort.
                let mut outer = elem.map(|elem| match mutability {
                    Some(_) => Type::Path(parse_quote! {
                        ::std::pin::Pin < & #mutability #elem >
                    }),
                    None => Type::Reference(parse_quote! {
                        & #elem
                    }),
                });
                outer.kind = if mutability.is_some() {
                    TypeKind::MutableReference
                } else {
                    TypeKind::Reference
                };
                Ok(outer)
            }
            PointerTreatment::RValueReference => {
                crate::known_types::ensure_pointee_is_valid(&ptr)?;
                let innerty =
                    self.convert_boxed_type(ptr.elem, ns, &TypeConversionContext::WithinReference)?;
                ptr.elem = innerty.ty;
                Ok(Annotated::new(
                    Type::Ptr(ptr),
                    innerty.types_encountered,
                    innerty.extra_apis,
                    TypeKind::RValueReference,
                ))
            }
        }
    }

    fn get_templated_typename(
        &mut self,
        rs_definition: &Type,
    ) -> Result<(QualifiedName, Option<UnanalyzedApi>), ConvertError> {
        let count = self.concrete_templates.len();
        // We just use this as a hash key, essentially.
        // TODO: Once we've completed the TypeConverter refactoring (see #220),
        // pass in an actual original_name_map here.
        let cpp_definition = type_to_cpp(rs_definition, &HashMap::new())?;
        let e = self.concrete_templates.get(&cpp_definition);
        match e {
            Some(tn) => Ok((tn.clone(), None)),
            None => {
                let synthetic_ident = format!(
                    "{}_AutocxxConcrete",
                    cpp_definition.replace(|c: char| !(c.is_ascii_alphanumeric() || c == '_'), "_")
                );
                // Remove runs of multiple _s. Trying to avoid a dependency on
                // regex.
                let synthetic_ident = synthetic_ident
                    .split('_')
                    .filter(|s| !s.is_empty())
                    .join("_");
                // Ensure we're not duplicating some existing concrete template name.
                // If so, we'll invent a name which is guaranteed to be unique.
                let synthetic_ident = match self
                    .concrete_templates
                    .values()
                    .map(|n| n.get_final_item())
                    .find(|s| s == &synthetic_ident)
                {
                    None => synthetic_ident,
                    Some(_) => format!("AutocxxConcrete{}", count),
                };
                let api = UnanalyzedApi::ConcreteType {
                    name: ApiName::new_in_root_namespace(make_ident(&synthetic_ident)),
                    cpp_definition: cpp_definition.clone(),
                    rs_definition: Some(Box::new(rs_definition.clone())),
                };
                self.concrete_templates
                    .insert(cpp_definition, api.name().clone());
                Ok((api.name().clone(), Some(api)))
            }
        }
    }

    fn confirm_inner_type_is_acceptable_generic_payload(
        &self,
        path_args: &Punctuated<GenericArgument, Comma>,
        desc: &QualifiedName,
        generic_behavior: CxxGenericType,
        forward_declarations_ok: bool,
    ) -> Result<TypeKind, ConvertError> {
        for inner in path_args {
            match inner {
                GenericArgument::Type(Type::Path(typ)) => {
                    let inner_qn = QualifiedName::from_type_path(typ);
                    if !forward_declarations_ok && self.forward_declarations.contains(&inner_qn) {
                        return Err(ConvertError::TypeContainingForwardDeclaration(inner_qn));
                    }
                    match generic_behavior {
                        CxxGenericType::Rust => {
                            if !inner_qn.get_namespace().is_empty() {
                                return Err(ConvertError::RustTypeWithAPath(inner_qn));
                            }
                            if !self.config.is_rust_type(&inner_qn.get_final_ident()) {
                                return Err(ConvertError::BoxContainingNonRustType(inner_qn));
                            }
                            if self
                                .config
                                .is_subclass_holder(&inner_qn.get_final_ident().to_string())
                            {
                                return Ok(TypeKind::SubclassHolder(inner_qn.get_final_ident()));
                            } else {
                                return Ok(TypeKind::Regular);
                            }
                        }
                        CxxGenericType::CppPtr => {
                            if !known_types().permissible_within_unique_ptr(&inner_qn) {
                                return Err(ConvertError::InvalidTypeForCppPtr(inner_qn));
                            }
                        }
                        CxxGenericType::CppVector => {
                            if !known_types().permissible_within_vector(&inner_qn) {
                                return Err(ConvertError::InvalidTypeForCppVector(inner_qn));
                            }
                            if matches!(
                                typ.path.segments.last().map(|ps| &ps.arguments),
                                Some(
                                    PathArguments::Parenthesized(_)
                                        | PathArguments::AngleBracketed(_)
                                )
                            ) {
                                return Err(ConvertError::GenericsWithinVector);
                            }
                        }
                        _ => {}
                    }
                }
                _ => {
                    return Err(ConvertError::TemplatedTypeContainingNonPathArg(
                        desc.clone(),
                    ))
                }
            }
        }
        Ok(TypeKind::Regular)
    }

    fn find_typedefs<A: AnalysisPhase>(apis: &ApiVec<A>) -> HashMap<QualifiedName, Type>
    where
        A::TypedefAnalysis: TypedefTarget,
    {
        apis.iter()
            .filter_map(|api| match &api {
                Api::Typedef { analysis, .. } => analysis
                    .get_target()
                    .cloned()
                    .map(|ty| (api.name().clone(), ty)),
                _ => None,
            })
            .collect()
    }

    fn find_concrete_templates<A: AnalysisPhase>(
        apis: &ApiVec<A>,
    ) -> HashMap<String, QualifiedName> {
        apis.iter()
            .filter_map(|api| match &api {
                Api::ConcreteType { cpp_definition, .. } => {
                    Some((cpp_definition.clone(), api.name().clone()))
                }
                _ => None,
            })
            .collect()
    }

    fn find_incomplete_types<A: AnalysisPhase>(apis: &ApiVec<A>) -> HashSet<QualifiedName> {
        apis.iter()
            .filter_map(|api| match api {
                Api::ForwardDeclaration { .. }
                | Api::OpaqueTypedef {
                    forward_declaration: true,
                    ..
                } => Some(api.name()),
                _ => None,
            })
            .cloned()
            .collect()
    }
}

/// Processing functions sometimes results in new types being materialized.
/// These types haven't been through the analysis phases (chicken and egg
/// problem) but fortunately, don't need to. We need to keep the type
/// system happy by adding an [ApiAnalysis] but in practice, for the sorts
/// of things that get created, it's always blank.
pub(crate) fn add_analysis<A: AnalysisPhase>(api: UnanalyzedApi) -> Api<A> {
    match api {
        Api::ConcreteType {
            name,
            rs_definition,
            cpp_definition,
        } => Api::ConcreteType {
            name,
            rs_definition,
            cpp_definition,
        },
        Api::IgnoredItem { name, err, ctx } => Api::IgnoredItem { name, err, ctx },
        _ => panic!("Function analysis created an unexpected type of extra API"),
    }
}
pub(crate) trait TypedefTarget {
    fn get_target(&self) -> Option<&Type>;
}

impl TypedefTarget for () {
    fn get_target(&self) -> Option<&Type> {
        None
    }
}

impl TypedefTarget for TypedefAnalysis {
    fn get_target(&self) -> Option<&Type> {
        Some(match self.kind {
            TypedefKind::Type(ref ty) => &ty.ty,
            TypedefKind::Use(_, ref ty) => ty,
        })
    }
}

pub(crate) fn find_types<A: AnalysisPhase>(apis: &ApiVec<A>) -> HashSet<QualifiedName> {
    apis.iter()
        .filter_map(|api| match api {
            Api::ForwardDeclaration { .. }
            | Api::OpaqueTypedef { .. }
            | Api::ConcreteType { .. }
            | Api::Typedef { .. }
            | Api::Enum { .. }
            | Api::Struct { .. }
            | Api::Subclass { .. }
            | Api::ExternCppType { .. }
            | Api::RustType { .. } => Some(api.name()),
            Api::StringConstructor { .. }
            | Api::Function { .. }
            | Api::Const { .. }
            | Api::CType { .. }
            | Api::RustSubclassFn { .. }
            | Api::IgnoredItem { .. }
            | Api::SubclassTraitItem { .. }
            | Api::RustFn { .. } => None,
        })
        .cloned()
        .collect()
}
