// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

mod bridge_name_tracker;
pub(crate) mod function_wrapper;
mod implicit_constructors;
mod overload_tracker;
mod subclass;

use crate::{
    conversion::{
        analysis::{
            fun::function_wrapper::{CppConversionType, CppFunctionKind},
            type_converter::{self, add_analysis, TypeConversionContext, TypeConverter},
        },
        api::{
            ApiName, CastMutability, CppVisibility, FuncToConvert, NullPhase, Provenance,
            References, SpecialMemberKind, SubclassName, TraitImplSignature, TraitSynthesis,
            UnsafetyNeeded, Virtualness,
        },
        apivec::ApiVec,
        convert_error::ErrorContext,
        convert_error::{ConvertErrorWithContext, ErrorContextType},
        error_reporter::{convert_apis, report_any_error},
    },
    known_types::known_types,
    types::validate_ident_ok_for_rust,
};
use std::collections::{HashMap, HashSet};

use autocxx_parser::{IncludeCppConfig, UnsafePolicy};
use function_wrapper::{CppFunction, CppFunctionBody, TypeConversionPolicy};
use itertools::Itertools;
use proc_macro2::Span;
use quote::quote;
use syn::{
    parse_quote, punctuated::Punctuated, token::Comma, FnArg, Ident, Pat, ReturnType, Type,
    TypePtr, Visibility,
};

use crate::{
    conversion::{
        api::{AnalysisPhase, Api, TypeKind},
        ConvertError,
    },
    types::{make_ident, validate_ident_ok_for_cxx, Namespace, QualifiedName},
};

use self::{
    bridge_name_tracker::BridgeNameTracker,
    function_wrapper::RustConversionType,
    implicit_constructors::{find_constructors_present, ItemsFound},
    overload_tracker::OverloadTracker,
    subclass::{
        create_subclass_constructor, create_subclass_fn_wrapper, create_subclass_function,
        create_subclass_trait_item,
    },
};

use super::{
    doc_label::make_doc_attrs,
    pod::{PodAnalysis, PodPhase},
    tdef::TypedefAnalysis,
    type_converter::Annotated,
};

#[derive(Clone, Debug)]
pub(crate) enum ReceiverMutability {
    Const,
    Mutable,
}

#[derive(Clone, Debug)]
pub(crate) enum MethodKind {
    Normal(ReceiverMutability),
    Constructor { is_default: bool },
    MakeUnique,
    Static,
    Virtual(ReceiverMutability),
    PureVirtual(ReceiverMutability),
}

#[derive(Clone)]
pub(crate) enum TraitMethodKind {
    CopyConstructor,
    MoveConstructor,
    Cast,
    Destructor,
    Alloc,
    Dealloc,
}

#[derive(Clone)]
pub(crate) struct TraitMethodDetails {
    pub(crate) trt: TraitImplSignature,
    pub(crate) avoid_self: bool,
    pub(crate) method_name: Ident,
    /// For traits, where we're trying to implement a specific existing
    /// interface, we may need to reorder the parameters to fit that
    /// interface.
    pub(crate) parameter_reordering: Option<Vec<usize>>,
    /// The function we're calling from the trait requires unsafe even
    /// though the trait and its function aren't.
    pub(crate) trait_call_is_unsafe: bool,
}

#[derive(Clone)]
pub(crate) enum FnKind {
    Function,
    Method {
        method_kind: MethodKind,
        impl_for: QualifiedName,
    },
    TraitMethod {
        kind: TraitMethodKind,
        /// The name of the type T for which we're implementing a trait,
        /// though we may be actually implementing the trait for &mut T or
        /// similar, so we store more details of both the type and the
        /// method in `details`
        impl_for: QualifiedName,
        details: Box<TraitMethodDetails>,
    },
}

/// Strategy for ensuring that the final, callable, Rust name
/// is what the user originally expected.
#[derive(Clone)]

pub(crate) enum RustRenameStrategy {
    /// cxx::bridge name matches user expectations
    None,
    /// Even the #[rust_name] attribute would cause conflicts, and we need
    /// to use a 'use XYZ as ABC'
    RenameInOutputMod(Ident),
    /// This function requires us to generate a Rust function to do
    /// parameter conversion.
    RenameUsingWrapperFunction,
}

#[derive(Clone)]
pub(crate) struct FnAnalysis {
    /// Each entry in the cxx::bridge needs to have a unique name, even if
    /// (from the perspective of Rust and C++) things are in different
    /// namespaces/mods.
    pub(crate) cxxbridge_name: Ident,
    /// ... so record also the name under which we wish to expose it in Rust.
    pub(crate) rust_name: String,
    pub(crate) rust_rename_strategy: RustRenameStrategy,
    pub(crate) params: Punctuated<FnArg, Comma>,
    pub(crate) kind: FnKind,
    pub(crate) ret_type: ReturnType,
    pub(crate) param_details: Vec<ArgumentAnalysis>,
    pub(crate) ret_conversion: Option<TypeConversionPolicy>,
    pub(crate) requires_unsafe: UnsafetyNeeded,
    pub(crate) vis: Visibility,
    pub(crate) cpp_wrapper: Option<CppFunction>,
    pub(crate) deps: HashSet<QualifiedName>,
    /// Some methods still need to be recorded because we want
    /// to (a) generate the ability to call superclasses, (b) create
    /// subclass entries for them. But we do not want to have them
    /// be externally callable.
    pub(crate) ignore_reason: Result<(), ConvertErrorWithContext>,
    /// Whether this can be called by external code. Not so for
    /// protected methods.
    pub(crate) externally_callable: bool,
    /// Whether we need to generate a Rust-side calling function
    pub(crate) rust_wrapper_needed: bool,
}

#[derive(Clone)]
pub(crate) struct ArgumentAnalysis {
    pub(crate) conversion: TypeConversionPolicy,
    pub(crate) name: Pat,
    pub(crate) self_type: Option<(QualifiedName, ReceiverMutability)>,
    pub(crate) was_reference: bool,
    pub(crate) deps: HashSet<QualifiedName>,
    pub(crate) requires_unsafe: UnsafetyNeeded,
}

struct ReturnTypeAnalysis {
    rt: ReturnType,
    conversion: Option<TypeConversionPolicy>,
    was_reference: bool,
    deps: HashSet<QualifiedName>,
}

impl Default for ReturnTypeAnalysis {
    fn default() -> Self {
        Self {
            rt: parse_quote! {},
            conversion: Default::default(),
            was_reference: Default::default(),
            deps: Default::default(),
        }
    }
}

pub(crate) struct PodAndConstructorAnalysis {
    pub(crate) pod: PodAnalysis,
    pub(crate) constructors: PublicConstructors,
}

/// An analysis phase where we've analyzed each function, but
/// haven't yet determined which constructors/etc. belong to each type.
pub(crate) struct FnPrePhase1;

impl AnalysisPhase for FnPrePhase1 {
    type TypedefAnalysis = TypedefAnalysis;
    type StructAnalysis = PodAnalysis;
    type FunAnalysis = FnAnalysis;
}

/// An analysis phase where we've analyzed each function, and identified
/// what implicit constructors/destructors are present in each type.
pub(crate) struct FnPrePhase2;

impl AnalysisPhase for FnPrePhase2 {
    type TypedefAnalysis = TypedefAnalysis;
    type StructAnalysis = PodAndConstructorAnalysis;
    type FunAnalysis = FnAnalysis;
}

pub(crate) struct PodAndDepAnalysis {
    pub(crate) pod: PodAnalysis,
    pub(crate) constructor_and_allocator_deps: Vec<QualifiedName>,
    pub(crate) constructors: PublicConstructors,
}

/// Analysis phase after we've finished analyzing functions and determined
/// which constructors etc. belong to them.
pub(crate) struct FnPhase;

/// Indicates which kinds of public constructors are known to exist for a type.
#[derive(Debug, Default, Copy, Clone)]
pub(crate) struct PublicConstructors {
    pub(crate) move_constructor: bool,
    pub(crate) destructor: bool,
}

impl PublicConstructors {
    fn from_items_found(items_found: &ItemsFound) -> Self {
        Self {
            move_constructor: items_found.move_constructor.callable_any(),
            destructor: items_found.destructor.callable_any(),
        }
    }
}

impl AnalysisPhase for FnPhase {
    type TypedefAnalysis = TypedefAnalysis;
    type StructAnalysis = PodAndDepAnalysis;
    type FunAnalysis = FnAnalysis;
}

/// Whether to allow highly optimized calls because this is a simple Rust->C++ call,
/// or to use a simpler set of policies because this is a subclass call where
/// we may have C++->Rust->C++ etc.
#[derive(Copy, Clone)]
enum TypeConversionSophistication {
    Regular,
    SimpleForSubclasses,
}

pub(crate) struct FnAnalyzer<'a> {
    unsafe_policy: UnsafePolicy,
    extra_apis: ApiVec<NullPhase>,
    type_converter: TypeConverter<'a>,
    bridge_name_tracker: BridgeNameTracker,
    pod_safe_types: HashSet<QualifiedName>,
    config: &'a IncludeCppConfig,
    overload_trackers_by_mod: HashMap<Namespace, OverloadTracker>,
    subclasses_by_superclass: HashMap<QualifiedName, Vec<SubclassName>>,
    nested_type_name_map: HashMap<QualifiedName, String>,
    generic_types: HashSet<QualifiedName>,
    existing_superclass_trait_api_names: HashSet<QualifiedName>,
}

impl<'a> FnAnalyzer<'a> {
    pub(crate) fn analyze_functions(
        apis: ApiVec<PodPhase>,
        unsafe_policy: UnsafePolicy,
        config: &'a IncludeCppConfig,
    ) -> ApiVec<FnPrePhase2> {
        let mut me = Self {
            unsafe_policy,
            extra_apis: ApiVec::new(),
            type_converter: TypeConverter::new(config, &apis),
            bridge_name_tracker: BridgeNameTracker::new(),
            config,
            overload_trackers_by_mod: HashMap::new(),
            pod_safe_types: Self::build_pod_safe_type_set(&apis),
            subclasses_by_superclass: subclass::subclasses_by_superclass(&apis),
            nested_type_name_map: Self::build_nested_type_map(&apis),
            generic_types: Self::build_generic_type_set(&apis),
            existing_superclass_trait_api_names: HashSet::new(),
        };
        let mut results = ApiVec::new();
        convert_apis(
            apis,
            &mut results,
            |name, fun, _| me.analyze_foreign_fn_and_subclasses(name, fun),
            Api::struct_unchanged,
            Api::enum_unchanged,
            Api::typedef_unchanged,
        );
        let mut results = me.add_constructors_present(results);
        me.add_make_uniques(&mut results);
        results.extend(me.extra_apis.into_iter().map(add_analysis));
        results
    }

    fn build_pod_safe_type_set(apis: &ApiVec<PodPhase>) -> HashSet<QualifiedName> {
        apis.iter()
            .filter_map(|api| match api {
                Api::Struct {
                    analysis:
                        PodAnalysis {
                            kind: TypeKind::Pod,
                            ..
                        },
                    ..
                } => Some(api.name().clone()),
                Api::Enum { .. } => Some(api.name().clone()),
                _ => None,
            })
            .chain(
                known_types()
                    .get_pod_safe_types()
                    .filter_map(
                        |(tn, is_pod_safe)| {
                            if is_pod_safe {
                                Some(tn)
                            } else {
                                None
                            }
                        },
                    ),
            )
            .collect()
    }

    fn build_generic_type_set(apis: &ApiVec<PodPhase>) -> HashSet<QualifiedName> {
        apis.iter()
            .filter_map(|api| match api {
                Api::Struct {
                    analysis:
                        PodAnalysis {
                            is_generic: true, ..
                        },
                    ..
                } => Some(api.name().clone()),
                _ => None,
            })
            .collect()
    }

    /// Builds a mapping from a qualified type name to the last 'nest'
    /// of its name, if it has multiple elements.
    fn build_nested_type_map(apis: &ApiVec<PodPhase>) -> HashMap<QualifiedName, String> {
        apis.iter()
            .filter_map(|api| match api {
                Api::Struct { name, .. } | Api::Enum { name, .. } => {
                    let cpp_name = name
                        .cpp_name_if_present()
                        .cloned()
                        .unwrap_or_else(|| name.name.get_final_item().to_string());
                    cpp_name
                        .rsplit_once("::")
                        .map(|(_, suffix)| (name.name.clone(), suffix.to_string()))
                }
                _ => None,
            })
            .collect()
    }

    fn convert_boxed_type(
        &mut self,
        ty: Box<Type>,
        ns: &Namespace,
        convert_ptrs_to_references: bool,
    ) -> Result<Annotated<Box<Type>>, ConvertError> {
        let ctx = TypeConversionContext::CxxOuterType {
            convert_ptrs_to_references,
        };
        let mut annotated = self.type_converter.convert_boxed_type(ty, ns, &ctx)?;
        self.extra_apis.append(&mut annotated.extra_apis);
        Ok(annotated)
    }

    fn get_cxx_bridge_name(
        &mut self,
        type_name: Option<&str>,
        found_name: &str,
        ns: &Namespace,
    ) -> String {
        self.bridge_name_tracker
            .get_unique_cxx_bridge_name(type_name, found_name, ns)
    }

    fn is_on_allowlist(&self, type_name: &QualifiedName) -> bool {
        self.config.is_on_allowlist(&type_name.to_cpp_name())
    }

    fn is_generic_type(&self, type_name: &QualifiedName) -> bool {
        self.generic_types.contains(type_name)
    }

    #[allow(clippy::if_same_then_else)] // clippy bug doesn't notice the two
                                        // closures below are different.
    fn should_be_unsafe(
        &self,
        param_details: &[ArgumentAnalysis],
        kind: &FnKind,
    ) -> UnsafetyNeeded {
        let unsafest_non_self_param = UnsafetyNeeded::from_param_details(param_details, true);
        let unsafest_param = UnsafetyNeeded::from_param_details(param_details, false);
        match kind {
            // Trait unsafety must always correspond to the norms for the
            // trait we're implementing.
            FnKind::TraitMethod {
                kind:
                    TraitMethodKind::CopyConstructor
                    | TraitMethodKind::MoveConstructor
                    | TraitMethodKind::Alloc
                    | TraitMethodKind::Dealloc,
                ..
            } => UnsafetyNeeded::Always,
            FnKind::TraitMethod { .. } => match unsafest_param {
                UnsafetyNeeded::Always => UnsafetyNeeded::JustBridge,
                _ => unsafest_param,
            },
            _ if self.unsafe_policy == UnsafePolicy::AllFunctionsUnsafe => UnsafetyNeeded::Always,
            _ => match unsafest_non_self_param {
                UnsafetyNeeded::Always => UnsafetyNeeded::Always,
                UnsafetyNeeded::JustBridge => match unsafest_param {
                    UnsafetyNeeded::Always => UnsafetyNeeded::JustBridge,
                    _ => unsafest_non_self_param,
                },
                UnsafetyNeeded::None => match unsafest_param {
                    UnsafetyNeeded::Always => UnsafetyNeeded::JustBridge,
                    _ => unsafest_param,
                },
            },
        }
    }

    fn add_make_uniques(&mut self, apis: &mut ApiVec<FnPrePhase2>) {
        let mut results = ApiVec::new();

        // Pre-assemble a list of types with known destructors, to avoid having to
        // do a O(n^2) nested loop.
        let types_with_destructors: HashSet<_> = apis
            .iter()
            .filter_map(|api| match api {
                Api::Function {
                    fun,
                    analysis:
                        FnAnalysis {
                            kind: FnKind::TraitMethod { impl_for, .. },
                            ..
                        },
                    ..
                } if matches!(
                    **fun,
                    FuncToConvert {
                        special_member: Some(SpecialMemberKind::Destructor),
                        is_deleted: false,
                        cpp_vis: CppVisibility::Public,
                        ..
                    }
                ) =>
                {
                    Some(impl_for)
                }
                _ => None,
            })
            .cloned()
            .collect();

        for api in apis.iter() {
            if let Api::Function {
                name,
                fun,
                analysis:
                    analysis @ FnAnalysis {
                        kind:
                            FnKind::Method {
                                impl_for: sup,
                                method_kind: MethodKind::Constructor { .. },
                                ..
                            },
                        ..
                    },
                ..
            } = api
            {
                let initial_name = name.clone();
                // If we don't have an accessible destructor, then std::unique_ptr cannot be
                // instantiated for this C++ type.
                if !types_with_destructors.contains(sup) {
                    continue;
                }

                // Create a make_unique too
                self.create_make_unique(fun, initial_name, &mut results);

                for sub in self.subclasses_by_superclass(sup) {
                    // Create a subclass constructor. This is a synthesized function
                    // which didn't exist in the original C++.
                    let (subclass_constructor_func, subclass_constructor_name) =
                        create_subclass_constructor(sub, analysis, sup, fun);
                    self.analyze_and_add(
                        subclass_constructor_name.clone(),
                        subclass_constructor_func.clone(),
                        &mut results,
                        TypeConversionSophistication::Regular,
                    );
                    // and its corresponding make_unique
                    self.create_make_unique(
                        &subclass_constructor_func,
                        subclass_constructor_name,
                        &mut results,
                    );
                }
            }
        }
        apis.extend(results.into_iter());
    }

    /// Analyze a given function, and any permutations of that function which
    /// we might additionally generate (e.g. for subclasses.)
    ///
    /// Leaves the [`FnKind::Method::type_constructors`] at its default for [`add_constructors_present`]
    /// to fill out.
    fn analyze_foreign_fn_and_subclasses(
        &mut self,
        name: ApiName,
        fun: Box<FuncToConvert>,
    ) -> Result<Box<dyn Iterator<Item = Api<FnPrePhase1>>>, ConvertErrorWithContext> {
        let (analysis, name) =
            self.analyze_foreign_fn(name, &fun, TypeConversionSophistication::Regular, None);
        let mut results = ApiVec::new();

        // Consider whether we need to synthesize subclass items.
        if let FnKind::Method {
            impl_for: sup,
            method_kind:
                MethodKind::Virtual(receiver_mutability) | MethodKind::PureVirtual(receiver_mutability),
            ..
        } = &analysis.kind
        {
            let (simpler_analysis, _) = self.analyze_foreign_fn(
                name.clone(),
                &fun,
                TypeConversionSophistication::SimpleForSubclasses,
                Some(analysis.rust_name.clone()),
            );
            for sub in self.subclasses_by_superclass(sup) {
                // For each subclass, we need to create a plain-C++ method to call its superclass
                // and a Rust/C++ bridge API to call _that_.
                // What we're generating here is entirely about the subclass, so the
                // superclass's namespace is irrelevant. We generate
                // all subclasses in the root namespace.
                let is_pure_virtual = matches!(
                    &simpler_analysis.kind,
                    FnKind::Method {
                        method_kind: MethodKind::PureVirtual(..),
                        ..
                    }
                );

                let super_fn_call_name =
                    SubclassName::get_super_fn_name(&Namespace::new(), &analysis.rust_name);
                let super_fn_api_name = SubclassName::get_super_fn_name(
                    &Namespace::new(),
                    &analysis.cxxbridge_name.to_string(),
                );
                let trait_api_name = SubclassName::get_trait_api_name(sup, &analysis.rust_name);

                let mut subclass_fn_deps = vec![trait_api_name.clone()];
                if !is_pure_virtual {
                    // Create a C++ API representing the superclass implementation (allowing
                    // calls from Rust->C++)
                    let maybe_wrap = create_subclass_fn_wrapper(&sub, &super_fn_call_name, &fun);
                    let super_fn_name = ApiName::new_from_qualified_name(super_fn_api_name);
                    let super_fn_call_api_name = self.analyze_and_add(
                        super_fn_name,
                        maybe_wrap,
                        &mut results,
                        TypeConversionSophistication::SimpleForSubclasses,
                    );
                    subclass_fn_deps.push(super_fn_call_api_name);
                }

                // Create the Rust API representing the subclass implementation (allowing calls
                // from C++ -> Rust)
                results.push(create_subclass_function(
                    // RustSubclassFn
                    &sub,
                    &simpler_analysis,
                    &name,
                    receiver_mutability,
                    sup,
                    subclass_fn_deps,
                ));

                // Create the trait item for the <superclass>_methods and <superclass>_supers
                // traits. This is required per-superclass, not per-subclass, so don't
                // create it if it already exists.
                if !self
                    .existing_superclass_trait_api_names
                    .contains(&trait_api_name)
                {
                    self.existing_superclass_trait_api_names
                        .insert(trait_api_name.clone());
                    results.push(create_subclass_trait_item(
                        ApiName::new_from_qualified_name(trait_api_name),
                        &simpler_analysis,
                        receiver_mutability,
                        sup.clone(),
                        is_pure_virtual,
                    ));
                }
            }
        }

        results.push(Api::Function {
            fun,
            analysis,
            name,
        });

        Ok(Box::new(results.into_iter()))
    }

    /// Adds an API, usually a synthesized API. Returns the final calculated API name, which can be used
    /// for others to depend on this.
    fn analyze_and_add<P: AnalysisPhase<FunAnalysis = FnAnalysis>>(
        &mut self,
        name: ApiName,
        new_func: Box<FuncToConvert>,
        results: &mut ApiVec<P>,
        sophistication: TypeConversionSophistication,
    ) -> QualifiedName {
        let (analysis, name) = self.analyze_foreign_fn(name, &new_func, sophistication, None);
        results.push(Api::Function {
            fun: new_func,
            analysis,
            name: name.clone(),
        });
        name.name
    }

    /// Take a constructor e.g. pub fn A_A(this: *mut root::A);
    /// and synthesize a make_unique e.g. pub fn make_unique() -> cxx::UniquePtr<A>
    fn create_make_unique(
        &mut self,
        fun: &FuncToConvert,
        initial_name: ApiName,
        results: &mut ApiVec<FnPrePhase2>,
    ) {
        let mut new_fun = fun.clone();
        new_fun.provenance = Provenance::SynthesizedMakeUnique;
        let make_unique_func = Box::new(new_fun);
        self.analyze_and_add(
            initial_name,
            make_unique_func,
            results,
            TypeConversionSophistication::Regular,
        );
    }

    /// Determine how to materialize a function.
    ///
    /// The main job here is to determine whether a function can simply be noted
    /// in the [cxx::bridge] mod and passed directly to cxx, or if it needs a Rust-side
    /// wrapper function, or if it needs a C++-side wrapper function, or both.
    /// We aim for the simplest case but, for example:
    /// * We'll need a C++ wrapper for static methods
    /// * We'll need a C++ wrapper for parameters which need to be wrapped and unwrapped
    ///   to [cxx::UniquePtr]
    /// * We'll need a Rust wrapper if we've got a C++ wrapper and it's a method.
    /// * We may need wrappers if names conflict.
    /// etc.
    /// The other major thing we do here is figure out naming for the function.
    /// This depends on overloads, and what other functions are floating around.
    /// The output of this analysis phase is used by both Rust and C++ codegen.
    fn analyze_foreign_fn(
        &mut self,
        name: ApiName,
        fun: &FuncToConvert,
        sophistication: TypeConversionSophistication,
        predetermined_rust_name: Option<String>,
    ) -> (FnAnalysis, ApiName) {
        let mut cpp_name = name.cpp_name_if_present().cloned();
        let ns = name.name.get_namespace();

        // Let's gather some pre-wisdom about the name of the function.
        // We're shortly going to plunge into analyzing the parameters,
        // and it would be nice to have some idea of the function name
        // for diagnostics whilst we do that.
        let initial_rust_name = fun.ident.to_string();
        let diagnostic_display_name = cpp_name.as_ref().unwrap_or(&initial_rust_name);

        // Now let's analyze all the parameters.
        // See if any have annotations which our fork of bindgen has craftily inserted...
        let (param_details, bads): (Vec<_>, Vec<_>) = fun
            .inputs
            .iter()
            .map(|i| {
                self.convert_fn_arg(
                    i,
                    ns,
                    diagnostic_display_name,
                    &fun.synthesized_this_type,
                    &fun.references,
                    true,
                    None,
                    sophistication,
                )
            })
            .partition(Result::is_ok);
        let (mut params, mut param_details): (Punctuated<_, Comma>, Vec<_>) =
            param_details.into_iter().map(Result::unwrap).unzip();

        let params_deps: HashSet<_> = param_details
            .iter()
            .flat_map(|p| p.deps.iter().cloned())
            .collect();
        let self_ty = param_details
            .iter()
            .filter_map(|pd| pd.self_type.as_ref())
            .next()
            .cloned();

        // End of parameter processing.
        // Work out naming, part one.
        // bindgen may have mangled the name either because it's invalid Rust
        // syntax (e.g. a keyword like 'async') or it's an overload.
        // If the former, we respect that mangling. If the latter, we don't,
        // because we'll add our own overload counting mangling later.
        // Cases:
        //   function, IRN=foo,    CN=<none>                    output: foo    case 1
        //   function, IRN=move_,  CN=move   (keyword problem)  output: move_  case 2
        //   function, IRN=foo1,   CN=foo    (overload)         output: foo    case 3
        //   method,   IRN=A_foo,  CN=foo                       output: foo    case 4
        //   method,   IRN=A_move, CN=move   (keyword problem)  output: move_  case 5
        //   method,   IRN=A_foo1, CN=foo    (overload)         output: foo    case 6
        let ideal_rust_name = match &cpp_name {
            None => initial_rust_name, // case 1
            Some(cpp_name) => {
                if initial_rust_name.ends_with('_') {
                    initial_rust_name // case 2
                } else if validate_ident_ok_for_rust(cpp_name).is_err() {
                    format!("{}_", cpp_name) // case 5
                } else {
                    cpp_name.to_string() // cases 3, 4, 6
                }
            }
        };

        // Let's spend some time figuring out the kind of this function (i.e. method,
        // virtual function, etc.)
        // Part one, work out if this is a static method.
        let (is_static_method, self_ty, receiver_mutability) = match self_ty {
            None => {
                // Even if we can't find a 'self' parameter this could conceivably
                // be a static method.
                let self_ty = fun.self_ty.clone();
                (self_ty.is_some(), self_ty, None)
            }
            Some((self_ty, receiver_mutability)) => {
                (false, Some(self_ty), Some(receiver_mutability))
            }
        };

        // Part two, work out if this is a function, or method, or whatever.
        // First determine if this is actually a trait implementation.
        let trait_details = self.trait_creation_details_for_synthetic_function(
            &fun.add_to_trait,
            ns,
            &ideal_rust_name,
            &self_ty,
        );
        let (kind, error_context, rust_name) = if let Some(trait_details) = trait_details {
            trait_details
        } else if let Some(self_ty) = self_ty {
            // Some kind of method or static method.
            let type_ident = self_ty.get_final_item();
            // bindgen generates methods with the name:
            // {class}_{method name}
            // It then generates an impl section for the Rust type
            // with the original name, but we currently discard that impl section.
            // We want to feed cxx methods with just the method name, so let's
            // strip off the class name.
            let mut rust_name = ideal_rust_name;
            let nested_type_ident = self
                .nested_type_name_map
                .get(&self_ty)
                .map(|s| s.as_str())
                .unwrap_or_else(|| self_ty.get_final_item());
            if matches!(
                fun.special_member,
                Some(SpecialMemberKind::CopyConstructor | SpecialMemberKind::MoveConstructor)
            ) {
                let is_move =
                    matches!(fun.special_member, Some(SpecialMemberKind::MoveConstructor));
                if let Some(constructor_suffix) = rust_name.strip_prefix(nested_type_ident) {
                    rust_name = format!("new{}", constructor_suffix);
                }
                rust_name = predetermined_rust_name
                    .unwrap_or_else(|| self.get_overload_name(ns, type_ident, rust_name));
                let error_context = error_context_for_method(&self_ty, &rust_name);

                // If this is 'None', then something weird is going on. We'll check for that
                // later when we have enough context to generate useful errors.
                let arg_is_reference = matches!(
                    param_details
                        .get(1)
                        .map(|param| &param.conversion.unwrapped_type),
                    Some(Type::Reference(_))
                );
                // Some exotic forms of copy constructor have const and/or volatile qualifiers.
                // These are not sufficient to implement CopyNew, so we just treat them as regular
                // constructors. We detect them by their argument being translated to Pin at this
                // point.
                if is_move || arg_is_reference {
                    let (kind, method_name, trait_id) = if is_move {
                        (
                            TraitMethodKind::MoveConstructor,
                            "move_new",
                            quote! { MoveNew },
                        )
                    } else {
                        (
                            TraitMethodKind::CopyConstructor,
                            "copy_new",
                            quote! { CopyNew },
                        )
                    };
                    let ty = Type::Path(self_ty.to_type_path());
                    (
                        FnKind::TraitMethod {
                            kind,
                            impl_for: self_ty,
                            details: Box::new(TraitMethodDetails {
                                trt: TraitImplSignature {
                                    ty,
                                    trait_signature: parse_quote! {
                                        autocxx::moveit::new:: #trait_id
                                    },
                                    unsafety: Some(parse_quote! { unsafe }),
                                },
                                avoid_self: true,
                                method_name: make_ident(method_name),
                                parameter_reordering: Some(vec![1, 0]),
                                trait_call_is_unsafe: false,
                            }),
                        },
                        error_context,
                        rust_name,
                    )
                } else {
                    (
                        FnKind::Method {
                            impl_for: self_ty,
                            method_kind: MethodKind::Constructor { is_default: false },
                        },
                        error_context,
                        rust_name,
                    )
                }
            } else if matches!(fun.special_member, Some(SpecialMemberKind::Destructor)) {
                rust_name = predetermined_rust_name
                    .unwrap_or_else(|| self.get_overload_name(ns, type_ident, rust_name));
                let error_context = error_context_for_method(&self_ty, &rust_name);
                let ty = Type::Path(self_ty.to_type_path());
                (
                    FnKind::TraitMethod {
                        kind: TraitMethodKind::Destructor,
                        impl_for: self_ty,
                        details: Box::new(TraitMethodDetails {
                            trt: TraitImplSignature {
                                ty,
                                trait_signature: parse_quote! {
                                    Drop
                                },
                                unsafety: None,
                            },
                            avoid_self: false,
                            method_name: make_ident("drop"),
                            parameter_reordering: None,
                            trait_call_is_unsafe: true,
                        }),
                    },
                    error_context,
                    rust_name,
                )
            } else {
                let method_kind = if matches!(fun.provenance, Provenance::SynthesizedMakeUnique) {
                    // We're re-running this routine for a function we already analyzed.
                    // Previously we made a placement "new" (MethodKind::Constructor).
                    // This time we've asked ourselves to synthesize a make_unique.
                    let constructor_suffix = rust_name
                        .strip_prefix(nested_type_ident)
                        .or_else(|| rust_name.strip_prefix("new"))
                        .unwrap();
                    rust_name = format!("make_unique{}", constructor_suffix);
                    // Strip off the 'this' arg.
                    params = params.into_iter().skip(1).collect();
                    param_details.remove(0);
                    MethodKind::MakeUnique
                } else if let Some(constructor_suffix) = rust_name.strip_prefix(nested_type_ident) {
                    // It's a constructor. bindgen generates
                    // fn Type(this: *mut Type, ...args)
                    // We want
                    // fn new(this: *mut Type, ...args)
                    // Later code will spot this and re-enter us, and we'll make
                    // a duplicate function in the above 'if' clause like this:
                    // fn make_unique(...args) -> Type
                    // which later code will convert to
                    // fn make_unique(...args) -> UniquePtr<Type>
                    // If there are multiple constructors, bindgen generates
                    // new, new1, new2 etc. and we'll keep those suffixes.
                    rust_name = format!("new{}", constructor_suffix);
                    MethodKind::Constructor {
                        is_default: matches!(
                            fun.special_member,
                            Some(SpecialMemberKind::DefaultConstructor)
                        ),
                    }
                } else if is_static_method {
                    MethodKind::Static
                } else {
                    let receiver_mutability =
                        receiver_mutability.expect("Failed to find receiver details");
                    match fun.virtualness {
                        Virtualness::None => MethodKind::Normal(receiver_mutability),
                        Virtualness::Virtual => MethodKind::Virtual(receiver_mutability),
                        Virtualness::PureVirtual => MethodKind::PureVirtual(receiver_mutability),
                    }
                };
                // Disambiguate overloads.
                let rust_name = predetermined_rust_name
                    .unwrap_or_else(|| self.get_overload_name(ns, type_ident, rust_name));
                let error_context = error_context_for_method(&self_ty, &rust_name);
                (
                    FnKind::Method {
                        impl_for: self_ty,
                        method_kind,
                    },
                    error_context,
                    rust_name,
                )
            }
        } else {
            // Not a method.
            // What shall we call this function? It may be overloaded.
            let rust_name = self.get_function_overload_name(ns, ideal_rust_name);
            (
                FnKind::Function,
                ErrorContext::new_for_item(make_ident(&rust_name)),
                rust_name,
            )
        };

        // If we encounter errors from here on, we can give some context around
        // where the error occurred such that we can put a marker in the output
        // Rust code to indicate that a problem occurred (benefiting people using
        // rust-analyzer or similar). Make a closure to make this easy.
        let mut ignore_reason = Ok(());
        let mut set_ignore_reason =
            |err| ignore_reason = Err(ConvertErrorWithContext(err, Some(error_context.clone())));

        // Now we have figured out the type of function (from its parameters)
        // we might have determined that we have a constructor. If so,
        // annoyingly, we need to go back and fiddle with the parameters in a
        // different way. This is because we want the first parameter to be a
        // pointer not a reference. For copy + move constructors, we also
        // enforce Rust-side conversions to comply with moveit traits.
        match kind {
            FnKind::Method {
                method_kind: MethodKind::Constructor { .. },
                ..
            } => {
                self.reanalyze_parameter(
                    0,
                    fun,
                    ns,
                    &rust_name,
                    &mut params,
                    &mut param_details,
                    None,
                    sophistication,
                )
                .unwrap_or_else(&mut set_ignore_reason);
            }

            FnKind::TraitMethod {
                kind: TraitMethodKind::Destructor,
                ..
            } => {
                self.reanalyze_parameter(
                    0,
                    fun,
                    ns,
                    &rust_name,
                    &mut params,
                    &mut param_details,
                    Some(RustConversionType::FromTypeToPtr),
                    sophistication,
                )
                .unwrap_or_else(&mut set_ignore_reason);
            }
            FnKind::TraitMethod {
                kind: TraitMethodKind::CopyConstructor,
                ..
            } => {
                if param_details.len() < 2 {
                    set_ignore_reason(ConvertError::ConstructorWithOnlyOneParam);
                }
                self.reanalyze_parameter(
                    0,
                    fun,
                    ns,
                    &rust_name,
                    &mut params,
                    &mut param_details,
                    Some(RustConversionType::FromPinMaybeUninitToPtr),
                    sophistication,
                )
                .unwrap_or_else(&mut set_ignore_reason);
            }

            FnKind::TraitMethod {
                kind: TraitMethodKind::MoveConstructor,
                ..
            } => {
                if param_details.len() < 2 {
                    set_ignore_reason(ConvertError::ConstructorWithOnlyOneParam);
                }
                self.reanalyze_parameter(
                    0,
                    fun,
                    ns,
                    &rust_name,
                    &mut params,
                    &mut param_details,
                    Some(RustConversionType::FromPinMaybeUninitToPtr),
                    sophistication,
                )
                .unwrap_or_else(&mut set_ignore_reason);
                self.reanalyze_parameter(
                    1,
                    fun,
                    ns,
                    &rust_name,
                    &mut params,
                    &mut param_details,
                    Some(RustConversionType::FromPinMoveRefToPtr),
                    sophistication,
                )
                .unwrap_or_else(&mut set_ignore_reason);
            }
            _ => {}
        }

        let requires_unsafe = self.should_be_unsafe(&param_details, &kind);

        // Now we can add context to the error, check for a variety of error
        // cases. In each case, we continue to record the API, because it might
        // influence our later decisions to generate synthetic constructors
        // or note whether the type is abstract.
        let externally_callable = match fun.cpp_vis {
            CppVisibility::Private => {
                set_ignore_reason(ConvertError::PrivateMethod);
                false
            }
            CppVisibility::Protected => false,
            CppVisibility::Public => true,
        };
        if let Some(problem) = bads.into_iter().next() {
            match problem {
                Ok(_) => panic!("No error in the error"),
                Err(problem) => set_ignore_reason(problem),
            }
        } else if fun.unused_template_param {
            // This indicates that bindgen essentially flaked out because templates
            // were too complex.
            set_ignore_reason(ConvertError::UnusedTemplateParam)
        } else if matches!(
            fun.special_member,
            Some(SpecialMemberKind::AssignmentOperator)
        ) {
            // Be careful with the order of this if-else tree. Anything above here means we won't
            // treat it as an assignment operator, but anything below we still consider when
            // deciding which other C++ special member functions are implicitly defined.
            set_ignore_reason(ConvertError::AssignmentOperator)
        } else if fun.references.rvalue_ref_return {
            set_ignore_reason(ConvertError::RValueReturn)
        } else if fun.is_deleted {
            set_ignore_reason(ConvertError::Deleted)
        } else if !fun.references.rvalue_ref_params.is_empty()
            && !matches!(
                kind,
                FnKind::TraitMethod {
                    kind: TraitMethodKind::MoveConstructor,
                    ..
                }
            )
        {
            set_ignore_reason(ConvertError::RValueParam)
        } else {
            match kind {
                FnKind::Method {
                    ref impl_for,
                    method_kind:
                        MethodKind::Constructor { .. }
                        | MethodKind::MakeUnique
                        | MethodKind::Normal(..)
                        | MethodKind::PureVirtual(..)
                        | MethodKind::Virtual(..),
                    ..
                } if !known_types().is_cxx_acceptable_receiver(impl_for) => {
                    set_ignore_reason(ConvertError::UnsupportedReceiver);
                }
                FnKind::Method { ref impl_for, .. } if !self.is_on_allowlist(impl_for) => {
                    // Bindgen will output methods for types which have been encountered
                    // virally as arguments on other allowlisted types. But we don't want
                    // to generate methods unless the user has specifically asked us to.
                    // It may, for instance, be a private type.
                    set_ignore_reason(ConvertError::MethodOfNonAllowlistedType);
                }
                FnKind::Method { ref impl_for, .. } | FnKind::TraitMethod { ref impl_for, .. }
                    if self.is_generic_type(impl_for) =>
                {
                    set_ignore_reason(ConvertError::MethodOfGenericType);
                }
                _ => {}
            }
        };

        // The name we use within the cxx::bridge mod may be different
        // from both the C++ name and the Rust name, because it's a flat
        // namespace so we might need to prepend some stuff to make it unique.
        let cxxbridge_name = self.get_cxx_bridge_name(
            match kind {
                FnKind::Method { ref impl_for, .. } => Some(impl_for.get_final_item()),
                FnKind::Function => None,
                FnKind::TraitMethod { ref impl_for, .. } => Some(impl_for.get_final_item()),
            },
            &rust_name,
            ns,
        );
        if cxxbridge_name != rust_name && cpp_name.is_none() {
            cpp_name = Some(rust_name.clone());
        }
        let mut cxxbridge_name = make_ident(&cxxbridge_name);

        // Analyze the return type, just as we previously did for the
        // parameters.
        let mut return_analysis = if let FnKind::Method {
            ref impl_for,
            method_kind: MethodKind::MakeUnique,
            ..
        } = kind
        {
            let constructed_type = impl_for.to_type_path();
            ReturnTypeAnalysis {
                rt: parse_quote! {
                    -> #constructed_type
                },
                conversion: Some(TypeConversionPolicy::new_to_unique_ptr(parse_quote! {
                    #constructed_type
                })),
                was_reference: false,
                deps: std::iter::once(impl_for).cloned().collect(),
            }
        } else {
            self.convert_return_type(&fun.output, ns, &fun.references)
                .unwrap_or_else(|err| {
                    set_ignore_reason(err);
                    ReturnTypeAnalysis::default()
                })
        };
        let mut deps = params_deps;
        deps.extend(return_analysis.deps.drain());

        let num_input_references = param_details.iter().filter(|pd| pd.was_reference).count();
        if num_input_references != 1 && return_analysis.was_reference {
            // cxx only allows functions to return a reference if they take exactly
            // one reference as a parameter. Let's see...
            set_ignore_reason(ConvertError::NotOneInputReference(rust_name.clone()));
        }
        let mut ret_type = return_analysis.rt;
        let ret_type_conversion = return_analysis.conversion;

        // Do we need to convert either parameters or return type?
        let param_conversion_needed = param_details.iter().any(|b| b.conversion.cpp_work_needed());
        let ret_type_conversion_needed = ret_type_conversion
            .as_ref()
            .map_or(false, |x| x.cpp_work_needed());
        // See https://github.com/dtolnay/cxx/issues/878 for the reason for this next line.
        let effective_cpp_name = cpp_name.as_ref().unwrap_or(&rust_name);
        let cpp_name_incompatible_with_cxx =
            validate_ident_ok_for_rust(effective_cpp_name).is_err();
        // If possible, we'll put knowledge of the C++ API directly into the cxx::bridge
        // mod. However, there are various circumstances where cxx can't work with the existing
        // C++ API and we need to create a C++ wrapper function which is more cxx-compliant.
        // That wrapper function is included in the cxx::bridge, and calls through to the
        // original function.
        let wrapper_function_needed = match kind {
            FnKind::Method {
                method_kind:
                    MethodKind::Static
                    | MethodKind::Constructor { .. }
                    | MethodKind::Virtual(_)
                    | MethodKind::PureVirtual(_),
                ..
            }
            | FnKind::TraitMethod {
                kind:
                    TraitMethodKind::CopyConstructor
                    | TraitMethodKind::MoveConstructor
                    | TraitMethodKind::Destructor,
                ..
            } => true,
            FnKind::Method { .. } if cxxbridge_name != rust_name => true,
            _ if param_conversion_needed => true,
            _ if ret_type_conversion_needed => true,
            _ if cpp_name_incompatible_with_cxx => true,
            _ if fun.synthetic_cpp.is_some() => true,
            _ => false,
        };

        let cpp_wrapper = if wrapper_function_needed {
            // Generate a new layer of C++ code to wrap/unwrap parameters
            // and return values into/out of std::unique_ptrs.
            let cpp_construction_ident = make_ident(&effective_cpp_name);
            let joiner = if cxxbridge_name.to_string().ends_with('_') {
                ""
            } else {
                "_"
            };
            cxxbridge_name = make_ident(&format!("{}{}autocxx_wrapper", cxxbridge_name, joiner));
            let (payload, cpp_function_kind) = match fun.synthetic_cpp.as_ref().cloned() {
                Some((payload, cpp_function_kind)) => (payload, cpp_function_kind),
                None => match kind {
                    FnKind::Method {
                        method_kind: MethodKind::MakeUnique,
                        ..
                    } => (CppFunctionBody::MakeUnique, CppFunctionKind::Function),
                    FnKind::Method {
                        ref impl_for,
                        method_kind: MethodKind::Constructor { .. },
                        ..
                    }
                    | FnKind::TraitMethod {
                        kind: TraitMethodKind::CopyConstructor | TraitMethodKind::MoveConstructor,
                        ref impl_for,
                        ..
                    } => (
                        CppFunctionBody::PlacementNew(ns.clone(), impl_for.get_final_ident()),
                        CppFunctionKind::Constructor,
                    ),
                    FnKind::TraitMethod {
                        kind: TraitMethodKind::Destructor,
                        ref impl_for,
                        ..
                    } => (
                        CppFunctionBody::Destructor(ns.clone(), impl_for.get_final_ident()),
                        CppFunctionKind::Function,
                    ),
                    FnKind::Method {
                        ref impl_for,
                        method_kind: MethodKind::Static,
                        ..
                    } => (
                        CppFunctionBody::StaticMethodCall(
                            ns.clone(),
                            impl_for.get_final_ident(),
                            cpp_construction_ident,
                        ),
                        CppFunctionKind::Function,
                    ),
                    FnKind::Method { .. } => (
                        CppFunctionBody::FunctionCall(ns.clone(), cpp_construction_ident),
                        CppFunctionKind::Method,
                    ),
                    _ => (
                        CppFunctionBody::FunctionCall(ns.clone(), cpp_construction_ident),
                        CppFunctionKind::Function,
                    ),
                },
            };
            // Now modify the cxx::bridge entry we're going to make.
            if let Some(ref conversion) = ret_type_conversion {
                let new_ret_type = conversion.unconverted_rust_type();
                ret_type = parse_quote!(
                    -> #new_ret_type
                );
            }

            // Amend parameters for the function which we're asking cxx to generate.
            params.clear();
            for pd in &param_details {
                let type_name = pd.conversion.converted_rust_type();
                let arg_name = if pd.self_type.is_some()
                    && !matches!(
                        kind,
                        FnKind::Method {
                            method_kind: MethodKind::MakeUnique,
                            ..
                        }
                    ) {
                    parse_quote!(autocxx_gen_this)
                } else {
                    pd.name.clone()
                };
                params.push(parse_quote!(
                    #arg_name: #type_name
                ));
            }

            Some(CppFunction {
                payload,
                wrapper_function_name: cxxbridge_name.clone(),
                original_cpp_name: cpp_name
                    .as_ref()
                    .cloned()
                    .unwrap_or_else(|| cxxbridge_name.to_string()),
                return_conversion: ret_type_conversion.clone(),
                argument_conversion: param_details.iter().map(|d| d.conversion.clone()).collect(),
                kind: cpp_function_kind,
                pass_obs_field: false,
                qualification: None,
            })
        } else {
            None
        };

        let vis = fun.vis.clone();

        let any_param_needs_rust_conversion = param_details
            .iter()
            .any(|pd| pd.conversion.rust_work_needed());

        let rust_wrapper_needed = match kind {
            FnKind::TraitMethod { .. } => true,
            FnKind::Method { .. } => any_param_needs_rust_conversion || cxxbridge_name != rust_name,
            _ => any_param_needs_rust_conversion,
        };

        // Naming, part two.
        // Work out our final naming strategy.
        validate_ident_ok_for_cxx(&cxxbridge_name.to_string()).unwrap_or_else(set_ignore_reason);
        let rust_name_ident = make_ident(&rust_name);
        let rust_rename_strategy = match kind {
            _ if rust_wrapper_needed => RustRenameStrategy::RenameUsingWrapperFunction,
            FnKind::Function if cxxbridge_name != rust_name => {
                RustRenameStrategy::RenameInOutputMod(rust_name_ident)
            }
            _ => RustRenameStrategy::None,
        };

        let analysis = FnAnalysis {
            cxxbridge_name: cxxbridge_name.clone(),
            rust_name: rust_name.clone(),
            rust_rename_strategy,
            params,
            ret_conversion: ret_type_conversion,
            kind,
            ret_type,
            param_details,
            requires_unsafe,
            vis,
            cpp_wrapper,
            deps,
            ignore_reason,
            externally_callable,
            rust_wrapper_needed,
        };
        let name = ApiName::new_with_cpp_name(ns, cxxbridge_name, cpp_name);
        (analysis, name)
    }

    /// Applies a specific `force_rust_conversion` to the parameter at index
    /// `param_idx`. Modifies `param_details` and `params` in place.
    #[allow(clippy::too_many_arguments)] // it's true, but sticking with it for now
    fn reanalyze_parameter(
        &mut self,
        param_idx: usize,
        fun: &FuncToConvert,
        ns: &Namespace,
        rust_name: &str,
        params: &mut Punctuated<FnArg, Comma>,
        param_details: &mut [ArgumentAnalysis],
        force_rust_conversion: Option<RustConversionType>,
        sophistication: TypeConversionSophistication,
    ) -> Result<(), ConvertError> {
        self.convert_fn_arg(
            fun.inputs.iter().nth(param_idx).unwrap(),
            ns,
            rust_name,
            &fun.synthesized_this_type,
            &fun.references,
            false,
            force_rust_conversion,
            sophistication,
        )
        .map(|(new_arg, new_analysis)| {
            param_details[param_idx] = new_analysis;
            let mut params_before = params.clone().into_iter();
            let prefix = params_before
                .by_ref()
                .take(param_idx)
                .collect_vec()
                .into_iter();
            let suffix = params_before.skip(1);
            *params = prefix
                .chain(std::iter::once(new_arg))
                .chain(suffix)
                .collect()
        })
    }

    fn get_overload_name(&mut self, ns: &Namespace, type_ident: &str, rust_name: String) -> String {
        let overload_tracker = self.overload_trackers_by_mod.entry(ns.clone()).or_default();
        overload_tracker.get_method_real_name(type_ident, rust_name)
    }

    /// Determine if this synthetic function should actually result in the implementation
    /// of a trait, rather than a function/method.
    fn trait_creation_details_for_synthetic_function(
        &mut self,
        synthesis: &Option<TraitSynthesis>,
        ns: &Namespace,
        ideal_rust_name: &str,
        self_ty: &Option<QualifiedName>,
    ) -> Option<(FnKind, ErrorContext, String)> {
        synthesis.as_ref().and_then(|synthesis| match synthesis {
            TraitSynthesis::Cast { to_type, mutable } => {
                let rust_name = self.get_function_overload_name(ns, ideal_rust_name.to_string());
                let from_type = self_ty.as_ref().unwrap();
                let from_type_path = from_type.to_type_path();
                let to_type = to_type.to_type_path();
                let (trait_signature, ty, method_name) = match *mutable {
                    CastMutability::ConstToConst => (
                        parse_quote! {
                            AsRef < #to_type >
                        },
                        Type::Path(from_type_path),
                        "as_ref",
                    ),
                    CastMutability::MutToConst => (
                        parse_quote! {
                            AsRef < #to_type >
                        },
                        parse_quote! {
                            &'a mut ::std::pin::Pin < &'a mut #from_type_path >
                        },
                        "as_ref",
                    ),
                    CastMutability::MutToMut => (
                        parse_quote! {
                            autocxx::PinMut < #to_type >
                        },
                        parse_quote! {
                            ::std::pin::Pin < &'a mut #from_type_path >
                        },
                        "pin_mut",
                    ),
                };
                let method_name = make_ident(method_name);
                Some((
                    FnKind::TraitMethod {
                        kind: TraitMethodKind::Cast,
                        impl_for: from_type.clone(),
                        details: Box::new(TraitMethodDetails {
                            trt: TraitImplSignature {
                                ty,
                                trait_signature,
                                unsafety: None,
                            },
                            avoid_self: false,
                            method_name,
                            parameter_reordering: None,
                            trait_call_is_unsafe: false,
                        }),
                    },
                    ErrorContext::new_for_item(make_ident(&rust_name)),
                    rust_name,
                ))
            }
            TraitSynthesis::AllocUninitialized(ty) => self.generate_alloc_or_deallocate(
                ideal_rust_name,
                ty,
                "allocate_uninitialized_cpp_storage",
                TraitMethodKind::Alloc,
            ),
            TraitSynthesis::FreeUninitialized(ty) => self.generate_alloc_or_deallocate(
                ideal_rust_name,
                ty,
                "free_uninitialized_cpp_storage",
                TraitMethodKind::Dealloc,
            ),
        })
    }

    fn generate_alloc_or_deallocate(
        &mut self,
        ideal_rust_name: &str,
        ty: &QualifiedName,
        method_name: &str,
        kind: TraitMethodKind,
    ) -> Option<(FnKind, ErrorContext, String)> {
        let rust_name =
            self.get_function_overload_name(ty.get_namespace(), ideal_rust_name.to_string());
        let typ = ty.to_type_path();
        Some((
            FnKind::TraitMethod {
                impl_for: ty.clone(),
                details: Box::new(TraitMethodDetails {
                    trt: TraitImplSignature {
                        ty: Type::Path(typ),
                        trait_signature: parse_quote! { autocxx::moveit::MakeCppStorage },
                        unsafety: Some(parse_quote! { unsafe }),
                    },
                    avoid_self: false,
                    method_name: make_ident(method_name),
                    parameter_reordering: None,
                    trait_call_is_unsafe: false,
                }),
                kind,
            },
            ErrorContext::new_for_item(make_ident(&rust_name)),
            rust_name,
        ))
    }

    fn get_function_overload_name(&mut self, ns: &Namespace, ideal_rust_name: String) -> String {
        let overload_tracker = self.overload_trackers_by_mod.entry(ns.clone()).or_default();
        overload_tracker.get_function_real_name(ideal_rust_name)
    }

    fn subclasses_by_superclass(&self, sup: &QualifiedName) -> impl Iterator<Item = SubclassName> {
        match self.subclasses_by_superclass.get(sup) {
            Some(subs) => subs.clone().into_iter(),
            None => Vec::new().into_iter(),
        }
    }

    #[allow(clippy::too_many_arguments)] // currently reasonably clear
    fn convert_fn_arg(
        &mut self,
        arg: &FnArg,
        ns: &Namespace,
        fn_name: &str,
        virtual_this: &Option<QualifiedName>,
        references: &References,
        treat_this_as_reference: bool,
        force_rust_conversion: Option<RustConversionType>,
        sophistication: TypeConversionSophistication,
    ) -> Result<(FnArg, ArgumentAnalysis), ConvertError> {
        Ok(match arg {
            FnArg::Typed(pt) => {
                let mut pt = pt.clone();
                let mut self_type = None;
                let old_pat = *pt.pat;
                let mut treat_as_reference = false;
                let mut treat_as_rvalue_reference = false;
                let new_pat = match old_pat {
                    syn::Pat::Ident(mut pp) if pp.ident == "this" => {
                        let this_type = match pt.ty.as_ref() {
                            Type::Ptr(TypePtr {
                                elem, mutability, ..
                            }) => match elem.as_ref() {
                                Type::Path(typ) => {
                                    let receiver_mutability = if mutability.is_some() {
                                        ReceiverMutability::Mutable
                                    } else {
                                        ReceiverMutability::Const
                                    };

                                    let this_type = if let Some(virtual_this) = virtual_this {
                                        let this_type_path = virtual_this.to_type_path();
                                        let const_token = if mutability.is_some() {
                                            None
                                        } else {
                                            Some(syn::Token![const](Span::call_site()))
                                        };
                                        pt.ty = Box::new(parse_quote! {
                                            * #mutability #const_token #this_type_path
                                        });
                                        virtual_this.clone()
                                    } else {
                                        QualifiedName::from_type_path(typ)
                                    };
                                    Ok((this_type, receiver_mutability))
                                }
                                _ => Err(ConvertError::UnexpectedThisType(
                                    ns.clone(),
                                    fn_name.into(),
                                )),
                            },
                            _ => Err(ConvertError::UnexpectedThisType(ns.clone(), fn_name.into())),
                        }?;
                        self_type = Some(this_type);
                        if treat_this_as_reference {
                            pp.ident = Ident::new("self", pp.ident.span());
                            treat_as_reference = true;
                        }
                        syn::Pat::Ident(pp)
                    }
                    syn::Pat::Ident(pp) => {
                        validate_ident_ok_for_cxx(&pp.ident.to_string())?;
                        treat_as_reference = references.ref_params.contains(&pp.ident);
                        treat_as_rvalue_reference =
                            references.rvalue_ref_params.contains(&pp.ident);
                        syn::Pat::Ident(pp)
                    }
                    _ => old_pat,
                };
                let annotated_type = self.convert_boxed_type(pt.ty, ns, treat_as_reference)?;
                let new_ty = annotated_type.ty;
                let subclass_holder = match &annotated_type.kind {
                    type_converter::TypeKind::SubclassHolder(holder) => Some(holder),
                    _ => None,
                };
                let conversion = self.argument_conversion_details(
                    &new_ty,
                    &subclass_holder.cloned(),
                    treat_as_rvalue_reference,
                    force_rust_conversion,
                    sophistication,
                );
                pt.pat = Box::new(new_pat.clone());
                pt.ty = new_ty;
                let requires_unsafe =
                    if matches!(annotated_type.kind, type_converter::TypeKind::Pointer) {
                        UnsafetyNeeded::Always
                    } else if conversion.bridge_unsafe_needed() {
                        UnsafetyNeeded::JustBridge
                    } else {
                        UnsafetyNeeded::None
                    };
                (
                    FnArg::Typed(pt),
                    ArgumentAnalysis {
                        self_type,
                        name: new_pat,
                        conversion,
                        was_reference: matches!(
                            annotated_type.kind,
                            type_converter::TypeKind::Reference
                                | type_converter::TypeKind::MutableReference
                        ),
                        deps: annotated_type.types_encountered,
                        requires_unsafe,
                    },
                )
            }
            _ => panic!("Did not expect FnArg::Receiver to be generated by bindgen"),
        })
    }

    fn argument_conversion_details(
        &self,
        ty: &Type,
        is_subclass_holder: &Option<Ident>,
        is_rvalue_ref: bool,
        force_rust_conversion: Option<RustConversionType>,
        sophistication: TypeConversionSophistication,
    ) -> TypeConversionPolicy {
        if let Some(holder_id) = is_subclass_holder {
            let subclass = SubclassName::from_holder_name(holder_id);
            return {
                let ty = parse_quote! {
                    rust::Box<#holder_id>
                };
                TypeConversionPolicy {
                    unwrapped_type: ty,
                    cpp_conversion: CppConversionType::Move,
                    rust_conversion: RustConversionType::ToBoxedUpHolder(subclass),
                }
            };
        }
        match ty {
            Type::Path(p) => {
                let ty = ty.clone();
                let tn = QualifiedName::from_type_path(p);
                if self.pod_safe_types.contains(&tn) {
                    if known_types().lacks_copy_constructor(&tn) {
                        TypeConversionPolicy {
                            unwrapped_type: ty,
                            cpp_conversion: CppConversionType::Move,
                            rust_conversion: RustConversionType::None,
                        }
                    } else {
                        TypeConversionPolicy::new_unconverted(ty)
                    }
                } else if known_types().convertible_from_strs(&tn)
                    && !self.config.exclude_utilities()
                {
                    TypeConversionPolicy {
                        unwrapped_type: ty,
                        cpp_conversion: CppConversionType::FromUniquePtrToValue,
                        rust_conversion: RustConversionType::FromStr,
                    }
                } else if matches!(
                    sophistication,
                    TypeConversionSophistication::SimpleForSubclasses
                ) {
                    TypeConversionPolicy {
                        unwrapped_type: ty,
                        cpp_conversion: CppConversionType::FromUniquePtrToValue,
                        rust_conversion: RustConversionType::None,
                    }
                } else {
                    TypeConversionPolicy {
                        unwrapped_type: ty,
                        cpp_conversion: CppConversionType::FromPtrToValue,
                        rust_conversion: RustConversionType::FromValueParamToPtr,
                    }
                }
            }
            _ => {
                let cpp_conversion = if is_rvalue_ref {
                    CppConversionType::FromPtrToMove
                } else {
                    CppConversionType::None
                };
                let rust_conversion = force_rust_conversion.unwrap_or(RustConversionType::None);
                TypeConversionPolicy {
                    unwrapped_type: ty.clone(),
                    cpp_conversion,
                    rust_conversion,
                }
            }
        }
    }

    fn return_type_conversion_details(&self, ty: &Type) -> TypeConversionPolicy {
        match ty {
            Type::Path(p) => {
                let tn = QualifiedName::from_type_path(p);
                if self.pod_safe_types.contains(&tn) {
                    TypeConversionPolicy::new_unconverted(ty.clone())
                } else {
                    TypeConversionPolicy::new_to_unique_ptr(ty.clone())
                }
            }
            _ => TypeConversionPolicy::new_unconverted(ty.clone()),
        }
    }

    fn convert_return_type(
        &mut self,
        rt: &ReturnType,
        ns: &Namespace,
        references: &References,
    ) -> Result<ReturnTypeAnalysis, ConvertError> {
        let result = match rt {
            ReturnType::Default => ReturnTypeAnalysis {
                rt: ReturnType::Default,
                was_reference: false,
                conversion: None,
                deps: HashSet::new(),
            },
            ReturnType::Type(rarrow, boxed_type) => {
                // TODO remove the below clone
                let annotated_type =
                    self.convert_boxed_type(boxed_type.clone(), ns, references.ref_return)?;
                let boxed_type = annotated_type.ty;
                let was_reference = matches!(boxed_type.as_ref(), Type::Reference(_));
                let conversion = self.return_type_conversion_details(boxed_type.as_ref());
                ReturnTypeAnalysis {
                    rt: ReturnType::Type(*rarrow, boxed_type),
                    conversion: Some(conversion),
                    was_reference,
                    deps: annotated_type.types_encountered,
                }
            }
        };
        Ok(result)
    }

    /// If a type has explicit constructors, bindgen will generate corresponding
    /// constructor functions, which we'll have already converted to make_unique methods.
    /// C++ mandates the synthesis of certain implicit constructors, to which we
    /// need to create bindings too. We do that here.
    /// It is tempting to make this a separate analysis phase, to be run later than
    /// the function analysis; but that would make the code much more complex as it
    /// would need to output a `FnAnalysisBody`. By running it as part of this phase
    /// we can simply generate the sort of thing bindgen generates, then ask
    /// the existing code in this phase to figure out what to do with it.
    ///
    /// Also fills out the [`PodAndConstructorAnalysis::constructors`] fields with information useful
    /// for further analysis phases.
    fn add_constructors_present(&mut self, mut apis: ApiVec<FnPrePhase1>) -> ApiVec<FnPrePhase2> {
        let all_items_found = find_constructors_present(&apis);
        for (self_ty, items_found) in all_items_found.iter() {
            if self.config.exclude_impls {
                // Remember that `find_constructors_present` mutates `apis`, so we always have to
                // call that, even if we don't do anything with the return value. This is kind of
                // messy, see the comment on this function for why.
                continue;
            }
            if self
                .config
                .is_on_constructor_blocklist(&self_ty.to_cpp_name())
            {
                continue;
            }
            let path = self_ty.to_type_path();
            if items_found.implicit_default_constructor_needed() {
                self.synthesize_special_member(
                    items_found,
                    None,
                    &mut apis,
                    SpecialMemberKind::DefaultConstructor,
                    parse_quote! { this: *mut #path },
                    References::default(),
                );
            }
            if items_found.implicit_move_constructor_needed() {
                self.synthesize_special_member(
                    items_found,
                    Some("move"),
                    &mut apis,
                    SpecialMemberKind::MoveConstructor,
                    parse_quote! { this: *mut #path, other: *mut #path },
                    References {
                        rvalue_ref_params: [make_ident("other")].into_iter().collect(),
                        ..Default::default()
                    },
                )
            }
            if items_found.implicit_copy_constructor_needed() {
                self.synthesize_special_member(
                    items_found,
                    Some("const_copy"),
                    &mut apis,
                    SpecialMemberKind::CopyConstructor,
                    parse_quote! { this: *mut #path, other: *const #path },
                    References {
                        ref_params: [make_ident("other")].into_iter().collect(),
                        ..Default::default()
                    },
                )
            }
            if items_found.implicit_destructor_needed() {
                self.synthesize_special_member(
                    items_found,
                    None,
                    &mut apis,
                    SpecialMemberKind::Destructor,
                    parse_quote! { this: *mut #path },
                    References::default(),
                );
            }
        }

        // Also, annotate each type with the constructors we found.
        let mut results = ApiVec::new();
        convert_apis(
            apis,
            &mut results,
            Api::fun_unchanged,
            |name, details, analysis| {
                let items_found = all_items_found.get(&name.name);
                Ok(Box::new(std::iter::once(Api::Struct {
                    name,
                    details,
                    analysis: PodAndConstructorAnalysis {
                        pod: analysis,
                        constructors: if let Some(items_found) = items_found {
                            PublicConstructors::from_items_found(items_found)
                        } else {
                            PublicConstructors::default()
                        },
                    },
                })))
            },
            Api::enum_unchanged,
            Api::typedef_unchanged,
        );
        results
    }

    #[allow(clippy::too_many_arguments)] // it's true, but sticking with it for now
    fn synthesize_special_member(
        &mut self,
        items_found: &ItemsFound,
        label: Option<&str>,
        apis: &mut ApiVec<FnPrePhase1>,
        special_member: SpecialMemberKind,
        inputs: Punctuated<FnArg, Comma>,
        references: References,
    ) {
        let self_ty = items_found.name.as_ref().unwrap();
        let ident = match label {
            Some(label) => make_ident(self.config.uniquify_name_per_mod(&format!(
                "{}_synthetic_{}_ctor",
                self_ty.name.get_final_item(),
                label
            ))),
            None => self_ty.name.get_final_ident(),
        };
        let cpp_name = if matches!(special_member, SpecialMemberKind::DefaultConstructor) {
            // Constructors (other than move or copy) are identified in `analyze_foreign_fn` by
            // being suffixed with the cpp_name, so we have to produce that.
            self.nested_type_name_map
                .get(&self_ty.name)
                .cloned()
                .or_else(|| Some(self_ty.name.get_final_item().to_string()))
        } else {
            None
        };
        let fake_api_name =
            ApiName::new_with_cpp_name(self_ty.name.get_namespace(), ident.clone(), cpp_name);
        let self_ty = &self_ty.name;
        let ns = self_ty.get_namespace().clone();
        let mut any_errors = ApiVec::new();
        apis.extend(
            report_any_error(&ns, &mut any_errors, || {
                self.analyze_foreign_fn_and_subclasses(
                    fake_api_name,
                    Box::new(FuncToConvert {
                        self_ty: Some(self_ty.clone()),
                        ident,
                        doc_attrs: make_doc_attrs(format!("Synthesized {}.", special_member)),
                        inputs,
                        output: ReturnType::Default,
                        vis: parse_quote! { pub },
                        virtualness: Virtualness::None,
                        cpp_vis: CppVisibility::Public,
                        special_member: Some(special_member),
                        unused_template_param: false,
                        references,
                        original_name: None,
                        synthesized_this_type: None,
                        is_deleted: false,
                        add_to_trait: None,
                        synthetic_cpp: None,
                        provenance: Provenance::SynthesizedOther,
                    }),
                )
            })
            .into_iter()
            .flatten(),
        );
        apis.append(&mut any_errors);
    }
}

fn error_context_for_method(self_ty: &QualifiedName, rust_name: &str) -> ErrorContext {
    ErrorContext::new_for_method(self_ty.get_final_ident(), make_ident(rust_name))
}

impl Api<FnPhase> {
    pub(crate) fn name_for_allowlist(&self) -> QualifiedName {
        match &self {
            Api::Function { analysis, .. } => match analysis.kind {
                FnKind::Method { ref impl_for, .. } => impl_for.clone(),
                FnKind::TraitMethod { ref impl_for, .. } => impl_for.clone(),
                FnKind::Function => {
                    QualifiedName::new(self.name().get_namespace(), make_ident(&analysis.rust_name))
                }
            },
            Api::RustSubclassFn { subclass, .. } => subclass.0.name.clone(),
            Api::IgnoredItem {
                name,
                ctx: Some(ctx),
                ..
            } => match ctx.get_type() {
                ErrorContextType::Method { self_ty, .. } => {
                    QualifiedName::new(name.name.get_namespace(), self_ty.clone())
                }
                ErrorContextType::Item(id) => {
                    QualifiedName::new(name.name.get_namespace(), id.clone())
                }
                _ => name.name.clone(),
            },
            _ => self.name().clone(),
        }
    }

    /// Whether this API requires generation of additional C++.
    /// This seems an odd place for this function (as opposed to in the [codegen_cpp]
    /// module) but, as it happens, even our Rust codegen phase needs to know if
    /// more C++ is needed (so it can add #includes in the cxx mod).
    /// And we can't answer the question _prior_ to this function analysis phase.
    pub(crate) fn needs_cpp_codegen(&self) -> bool {
        matches!(
            &self,
            Api::Function {
                analysis: FnAnalysis {
                    cpp_wrapper: Some(..),
                    ignore_reason: Ok(_),
                    externally_callable: true,
                    ..
                },
                ..
            } | Api::StringConstructor { .. }
                | Api::ConcreteType { .. }
                | Api::CType { .. }
                | Api::RustSubclassFn { .. }
                | Api::Subclass { .. }
                | Api::Struct {
                    analysis: PodAndDepAnalysis {
                        pod: PodAnalysis {
                            kind: TypeKind::Pod,
                            ..
                        },
                        ..
                    },
                    ..
                }
        )
    }

    pub(crate) fn cxxbridge_name(&self) -> Option<Ident> {
        match self {
            Api::Function { ref analysis, .. } => Some(analysis.cxxbridge_name.clone()),
            Api::StringConstructor { .. }
            | Api::Const { .. }
            | Api::IgnoredItem { .. }
            | Api::RustSubclassFn { .. } => None,
            _ => Some(self.name().get_final_ident()),
        }
    }
}
