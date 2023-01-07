mod dyngen;
mod error;
mod helpers;
mod impl_debug;
mod impl_partialeq;
pub mod struct_layout;

#[cfg(test)]
#[allow(warnings)]
pub(crate) mod bitfield_unit;
#[cfg(all(test, target_endian = "little"))]
mod bitfield_unit_tests;

use self::dyngen::DynamicItems;
use self::helpers::attributes;
use self::struct_layout::StructLayoutTracker;

use super::BindgenOptions;

use crate::ir::analysis::{HasVtable, Sizedness};
use crate::ir::annotations::FieldAccessorKind;
use crate::ir::comment;
use crate::ir::comp::{
    Bitfield, BitfieldUnit, CompInfo, CompKind, Field, FieldData, FieldMethods,
    Method, MethodKind,
};
use crate::ir::context::{BindgenContext, ItemId};
use crate::ir::derive::{
    CanDerive, CanDeriveCopy, CanDeriveDebug, CanDeriveDefault, CanDeriveEq,
    CanDeriveHash, CanDeriveOrd, CanDerivePartialEq, CanDerivePartialOrd,
};
use crate::ir::dot;
use crate::ir::enum_ty::{Enum, EnumVariant, EnumVariantValue};
use crate::ir::function::{Abi, Function, FunctionKind, FunctionSig, Linkage};
use crate::ir::int::IntKind;
use crate::ir::item::{IsOpaque, Item, ItemCanonicalName, ItemCanonicalPath};
use crate::ir::item_kind::ItemKind;
use crate::ir::layout::Layout;
use crate::ir::module::Module;
use crate::ir::objc::{ObjCInterface, ObjCMethod};
use crate::ir::template::{
    AsTemplateParam, TemplateInstantiation, TemplateParameters,
};
use crate::ir::ty::{Type, TypeKind};
use crate::ir::var::Var;

use proc_macro2::{self, Ident, Span};
use quote::TokenStreamExt;

use crate::{Entry, HashMap, HashSet};
use std::borrow::Cow;
use std::cell::Cell;
use std::collections::VecDeque;
use std::fmt::Write;
use std::iter;
use std::ops;
use std::str::FromStr;

// Name of type defined in constified enum module
pub static CONSTIFIED_ENUM_MODULE_REPR_NAME: &str = "Type";

fn top_level_path(
    ctx: &BindgenContext,
    item: &Item,
) -> Vec<proc_macro2::TokenStream> {
    let mut path = vec![quote! { self }];

    if ctx.options().enable_cxx_namespaces {
        for _ in 0..item.codegen_depth(ctx) {
            path.push(quote! { super });
        }
    }

    path
}

fn root_import(
    ctx: &BindgenContext,
    module: &Item,
) -> proc_macro2::TokenStream {
    assert!(ctx.options().enable_cxx_namespaces, "Somebody messed it up");
    assert!(module.is_module());

    let mut path = top_level_path(ctx, module);

    let root = ctx.root_module().canonical_name(ctx);
    let root_ident = ctx.rust_ident(&root);
    path.push(quote! { #root_ident });

    let mut tokens = quote! {};
    tokens.append_separated(path, quote!(::));

    quote! {
        #[allow(unused_imports)]
        use #tokens ;
    }
}

bitflags! {
    struct DerivableTraits: u16 {
        const DEBUG       = 1 << 0;
        const DEFAULT     = 1 << 1;
        const COPY        = 1 << 2;
        const CLONE       = 1 << 3;
        const HASH        = 1 << 4;
        const PARTIAL_ORD = 1 << 5;
        const ORD         = 1 << 6;
        const PARTIAL_EQ  = 1 << 7;
        const EQ          = 1 << 8;
    }
}

fn derives_of_item(
    item: &Item,
    ctx: &BindgenContext,
    packed: bool,
) -> DerivableTraits {
    let mut derivable_traits = DerivableTraits::empty();

    let all_template_params = item.all_template_params(ctx);

    if item.can_derive_copy(ctx) && !item.annotations().disallow_copy() {
        derivable_traits |= DerivableTraits::COPY;

        if ctx.options().rust_features().builtin_clone_impls ||
            !all_template_params.is_empty()
        {
            // FIXME: This requires extra logic if you have a big array in a
            // templated struct. The reason for this is that the magic:
            //     fn clone(&self) -> Self { *self }
            // doesn't work for templates.
            //
            // It's not hard to fix though.
            derivable_traits |= DerivableTraits::CLONE;
        }
    } else if packed {
        // If the struct or union is packed, deriving from Copy is required for
        // deriving from any other trait.
        return derivable_traits;
    }

    if item.can_derive_debug(ctx) && !item.annotations().disallow_debug() {
        derivable_traits |= DerivableTraits::DEBUG;
    }

    if item.can_derive_default(ctx) && !item.annotations().disallow_default() {
        derivable_traits |= DerivableTraits::DEFAULT;
    }

    if item.can_derive_hash(ctx) {
        derivable_traits |= DerivableTraits::HASH;
    }

    if item.can_derive_partialord(ctx) {
        derivable_traits |= DerivableTraits::PARTIAL_ORD;
    }

    if item.can_derive_ord(ctx) {
        derivable_traits |= DerivableTraits::ORD;
    }

    if item.can_derive_partialeq(ctx) {
        derivable_traits |= DerivableTraits::PARTIAL_EQ;
    }

    if item.can_derive_eq(ctx) {
        derivable_traits |= DerivableTraits::EQ;
    }

    derivable_traits
}

impl From<DerivableTraits> for Vec<&'static str> {
    fn from(derivable_traits: DerivableTraits) -> Vec<&'static str> {
        [
            (DerivableTraits::DEBUG, "Debug"),
            (DerivableTraits::DEFAULT, "Default"),
            (DerivableTraits::COPY, "Copy"),
            (DerivableTraits::CLONE, "Clone"),
            (DerivableTraits::HASH, "Hash"),
            (DerivableTraits::PARTIAL_ORD, "PartialOrd"),
            (DerivableTraits::ORD, "Ord"),
            (DerivableTraits::PARTIAL_EQ, "PartialEq"),
            (DerivableTraits::EQ, "Eq"),
        ]
        .iter()
        .filter_map(|&(flag, derive)| {
            Some(derive).filter(|_| derivable_traits.contains(flag))
        })
        .collect()
    }
}

struct CodegenResult<'a> {
    items: Vec<proc_macro2::TokenStream>,
    dynamic_items: DynamicItems,

    /// A monotonic counter used to add stable unique id's to stuff that doesn't
    /// need to be referenced by anything.
    codegen_id: &'a Cell<usize>,

    /// Whether a bindgen union has been generated at least once.
    saw_bindgen_union: bool,

    /// Whether an incomplete array has been generated at least once.
    saw_incomplete_array: bool,

    /// Whether Objective C types have been seen at least once.
    saw_objc: bool,

    /// Whether Apple block types have been seen at least once.
    saw_block: bool,

    /// Whether a bitfield allocation unit has been seen at least once.
    saw_bitfield_unit: bool,

    items_seen: HashSet<ItemId>,
    /// The set of generated function/var names, needed because in C/C++ is
    /// legal to do something like:
    ///
    /// ```c++
    /// extern "C" {
    ///   void foo();
    ///   extern int bar;
    /// }
    ///
    /// extern "C" {
    ///   void foo();
    ///   extern int bar;
    /// }
    /// ```
    ///
    /// Being these two different declarations.
    functions_seen: HashSet<String>,
    vars_seen: HashSet<String>,

    /// Used for making bindings to overloaded functions. Maps from a canonical
    /// function name to the number of overloads we have already codegen'd for
    /// that name. This lets us give each overload a unique suffix.
    overload_counters: HashMap<String, u32>,
}

impl<'a> CodegenResult<'a> {
    fn new(codegen_id: &'a Cell<usize>) -> Self {
        CodegenResult {
            items: vec![],
            dynamic_items: DynamicItems::new(),
            saw_bindgen_union: false,
            saw_incomplete_array: false,
            saw_objc: false,
            saw_block: false,
            saw_bitfield_unit: false,
            codegen_id,
            items_seen: Default::default(),
            functions_seen: Default::default(),
            vars_seen: Default::default(),
            overload_counters: Default::default(),
        }
    }

    fn dynamic_items(&mut self) -> &mut DynamicItems {
        &mut self.dynamic_items
    }

    fn saw_bindgen_union(&mut self) {
        self.saw_bindgen_union = true;
    }

    fn saw_incomplete_array(&mut self) {
        self.saw_incomplete_array = true;
    }

    fn saw_objc(&mut self) {
        self.saw_objc = true;
    }

    fn saw_block(&mut self) {
        self.saw_block = true;
    }

    fn saw_bitfield_unit(&mut self) {
        self.saw_bitfield_unit = true;
    }

    fn seen<Id: Into<ItemId>>(&self, item: Id) -> bool {
        self.items_seen.contains(&item.into())
    }

    fn set_seen<Id: Into<ItemId>>(&mut self, item: Id) {
        self.items_seen.insert(item.into());
    }

    fn seen_function(&self, name: &str) -> bool {
        self.functions_seen.contains(name)
    }

    fn saw_function(&mut self, name: &str) {
        self.functions_seen.insert(name.into());
    }

    /// Get the overload number for the given function name. Increments the
    /// counter internally so the next time we ask for the overload for this
    /// name, we get the incremented value, and so on.
    fn overload_number(&mut self, name: &str) -> u32 {
        let counter = self.overload_counters.entry(name.into()).or_insert(0);
        let number = *counter;
        *counter += 1;
        number
    }

    fn seen_var(&self, name: &str) -> bool {
        self.vars_seen.contains(name)
    }

    fn saw_var(&mut self, name: &str) {
        self.vars_seen.insert(name.into());
    }

    fn inner<F>(&mut self, cb: F) -> Vec<proc_macro2::TokenStream>
    where
        F: FnOnce(&mut Self),
    {
        let mut new = Self::new(self.codegen_id);

        cb(&mut new);

        self.saw_incomplete_array |= new.saw_incomplete_array;
        self.saw_objc |= new.saw_objc;
        self.saw_block |= new.saw_block;
        self.saw_bitfield_unit |= new.saw_bitfield_unit;
        self.saw_bindgen_union |= new.saw_bindgen_union;

        new.items
    }
}

impl<'a> ops::Deref for CodegenResult<'a> {
    type Target = Vec<proc_macro2::TokenStream>;

    fn deref(&self) -> &Self::Target {
        &self.items
    }
}

impl<'a> ops::DerefMut for CodegenResult<'a> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.items
    }
}

/// A trait to convert a rust type into a pointer, optionally const, to the same
/// type.
trait ToPtr {
    fn to_ptr(self, is_const: bool) -> proc_macro2::TokenStream;
}

impl ToPtr for proc_macro2::TokenStream {
    fn to_ptr(self, is_const: bool) -> proc_macro2::TokenStream {
        if is_const {
            quote! { *const #self }
        } else {
            quote! { *mut #self }
        }
    }
}

/// An extension trait for `proc_macro2::TokenStream` that lets us append any implicit
/// template parameters that exist for some type, if necessary.
trait AppendImplicitTemplateParams {
    fn append_implicit_template_params(
        &mut self,
        ctx: &BindgenContext,
        item: &Item,
    );
}

impl AppendImplicitTemplateParams for proc_macro2::TokenStream {
    fn append_implicit_template_params(
        &mut self,
        ctx: &BindgenContext,
        item: &Item,
    ) {
        let item = item.id().into_resolver().through_type_refs().resolve(ctx);

        match *item.expect_type().kind() {
            TypeKind::UnresolvedTypeRef(..) => {
                unreachable!("already resolved unresolved type refs")
            }
            TypeKind::ResolvedTypeRef(..) => {
                unreachable!("we resolved item through type refs")
            }

            // None of these types ever have implicit template parameters.
            TypeKind::Void |
            TypeKind::NullPtr |
            TypeKind::Pointer(..) |
            TypeKind::Reference(..) |
            TypeKind::Int(..) |
            TypeKind::Float(..) |
            TypeKind::Complex(..) |
            TypeKind::Array(..) |
            TypeKind::TypeParam |
            TypeKind::Opaque |
            TypeKind::Function(..) |
            TypeKind::Enum(..) |
            TypeKind::ObjCId |
            TypeKind::ObjCSel |
            TypeKind::TemplateInstantiation(..) => return,
            _ => {}
        }

        let params: Vec<_> = item
            .used_template_params(ctx)
            .iter()
            .map(|p| {
                p.try_to_rust_ty(ctx, &())
                    .expect("template params cannot fail to be a rust type")
            })
            .collect();
        if !params.is_empty() {
            self.append_all(quote! {
                < #( #params ),* >
            });
        }
    }
}

trait CodeGenerator {
    /// Extra information from the caller.
    type Extra;

    /// Extra information returned to the caller.
    type Return;

    fn codegen<'a>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'a>,
        extra: &Self::Extra,
    ) -> Self::Return;
}

impl Item {
    fn process_before_codegen(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult,
    ) -> bool {
        if !self.is_enabled_for_codegen(ctx) {
            return false;
        }

        if self.is_blocklisted(ctx) || result.seen(self.id()) {
            debug!(
                "<Item as CodeGenerator>::process_before_codegen: Ignoring hidden or seen: \
                 self = {:?}",
                self
            );
            return false;
        }

        if !ctx.codegen_items().contains(&self.id()) {
            // TODO(emilio, #453): Figure out what to do when this happens
            // legitimately, we could track the opaque stuff and disable the
            // assertion there I guess.
            warn!("Found non-allowlisted item in code generation: {:?}", self);
        }

        result.set_seen(self.id());
        true
    }
}

impl CodeGenerator for Item {
    type Extra = ();
    type Return = ();

    fn codegen<'a>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'a>,
        _extra: &(),
    ) {
        debug!("<Item as CodeGenerator>::codegen: self = {:?}", self);
        if !self.process_before_codegen(ctx, result) {
            return;
        }

        match *self.kind() {
            ItemKind::Module(ref module) => {
                module.codegen(ctx, result, self);
            }
            ItemKind::Function(ref fun) => {
                fun.codegen(ctx, result, self);
            }
            ItemKind::Var(ref var) => {
                var.codegen(ctx, result, self);
            }
            ItemKind::Type(ref ty) => {
                ty.codegen(ctx, result, self);
            }
        }
    }
}

impl CodeGenerator for Module {
    type Extra = Item;
    type Return = ();

    fn codegen<'a>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'a>,
        item: &Item,
    ) {
        debug!("<Module as CodeGenerator>::codegen: item = {:?}", item);

        let codegen_self = |result: &mut CodegenResult,
                            found_any: &mut bool| {
            for child in self.children() {
                if ctx.codegen_items().contains(child) {
                    *found_any = true;
                    ctx.resolve_item(*child).codegen(ctx, result, &());
                }
            }

            if item.id() == ctx.root_module() {
                if result.saw_block {
                    utils::prepend_block_header(ctx, &mut *result);
                }
                if result.saw_bindgen_union {
                    utils::prepend_union_types(ctx, &mut *result);
                }
                if result.saw_incomplete_array {
                    utils::prepend_incomplete_array_types(ctx, &mut *result);
                }
                if ctx.need_bindgen_complex_type() {
                    utils::prepend_complex_type(&mut *result);
                }
                if result.saw_objc {
                    utils::prepend_objc_header(ctx, &mut *result);
                }
                if result.saw_bitfield_unit {
                    utils::prepend_bitfield_unit_type(ctx, &mut *result);
                }
            }
        };

        if !ctx.options().enable_cxx_namespaces ||
            (self.is_inline() &&
                !ctx.options().conservative_inline_namespaces)
        {
            codegen_self(result, &mut false);
            return;
        }

        let mut found_any = false;
        let inner_items = result.inner(|result| {
            result.push(root_import(ctx, item));

            let path = item.namespace_aware_canonical_path(ctx).join("::");
            if let Some(raw_lines) = ctx.options().module_lines.get(&path) {
                for raw_line in raw_lines {
                    found_any = true;
                    result.push(
                        proc_macro2::TokenStream::from_str(raw_line).unwrap(),
                    );
                }
            }

            codegen_self(result, &mut found_any);
        });

        // Don't bother creating an empty module.
        if !found_any {
            return;
        }

        let name = item.canonical_name(ctx);
        let ident = ctx.rust_ident(name);
        result.push(if item.id() == ctx.root_module() {
            quote! {
                #[allow(non_snake_case, non_camel_case_types, non_upper_case_globals)]
                pub mod #ident {
                    #( #inner_items )*
                }
            }
        } else {
            quote! {
                pub mod #ident {
                    #( #inner_items )*
                }
            }
        });
    }
}

impl CodeGenerator for Var {
    type Extra = Item;
    type Return = ();

    fn codegen<'a>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'a>,
        item: &Item,
    ) {
        use crate::ir::var::VarType;
        debug!("<Var as CodeGenerator>::codegen: item = {:?}", item);
        debug_assert!(item.is_enabled_for_codegen(ctx));

        let canonical_name = item.canonical_name(ctx);

        if result.seen_var(&canonical_name) {
            return;
        }
        result.saw_var(&canonical_name);

        let canonical_ident = ctx.rust_ident(&canonical_name);

        // We can't generate bindings to static variables of templates. The
        // number of actual variables for a single declaration are open ended
        // and we don't know what instantiations do or don't exist.
        if !item.all_template_params(ctx).is_empty() {
            return;
        }

        let mut attrs = vec![];
        if let Some(comment) = item.comment(ctx) {
            attrs.push(attributes::doc(comment));
        }

        let ty = self.ty().to_rust_ty_or_opaque(ctx, &());

        if let Some(val) = self.val() {
            match *val {
                VarType::Bool(val) => {
                    result.push(quote! {
                        #(#attrs)*
                        pub const #canonical_ident : #ty = #val ;
                    });
                }
                VarType::Int(val) => {
                    let int_kind = self
                        .ty()
                        .into_resolver()
                        .through_type_aliases()
                        .through_type_refs()
                        .resolve(ctx)
                        .expect_type()
                        .as_integer()
                        .unwrap();
                    let val = if int_kind.is_signed() {
                        helpers::ast_ty::int_expr(val)
                    } else {
                        helpers::ast_ty::uint_expr(val as _)
                    };
                    result.push(quote! {
                        #(#attrs)*
                        pub const #canonical_ident : #ty = #val ;
                    });
                }
                VarType::String(ref bytes) => {
                    // Account the trailing zero.
                    //
                    // TODO: Here we ignore the type we just made up, probably
                    // we should refactor how the variable type and ty id work.
                    let len = bytes.len() + 1;
                    let ty = quote! {
                        [u8; #len]
                    };

                    match String::from_utf8(bytes.clone()) {
                        Ok(string) => {
                            let cstr = helpers::ast_ty::cstr_expr(string);
                            if ctx
                                .options()
                                .rust_features
                                .static_lifetime_elision
                            {
                                result.push(quote! {
                                    #(#attrs)*
                                    pub const #canonical_ident : &#ty = #cstr ;
                                });
                            } else {
                                result.push(quote! {
                                    #(#attrs)*
                                    pub const #canonical_ident : &'static #ty = #cstr ;
                                });
                            }
                        }
                        Err(..) => {
                            let bytes = helpers::ast_ty::byte_array_expr(bytes);
                            result.push(quote! {
                                #(#attrs)*
                                pub const #canonical_ident : #ty = #bytes ;
                            });
                        }
                    }
                }
                VarType::Float(f) => {
                    if let Ok(expr) = helpers::ast_ty::float_expr(ctx, f) {
                        result.push(quote! {
                            #(#attrs)*
                            pub const #canonical_ident : #ty = #expr ;
                        });
                    }
                }
                VarType::Char(c) => {
                    result.push(quote! {
                        #(#attrs)*
                        pub const #canonical_ident : #ty = #c ;
                    });
                }
            }
        } else {
            // If necessary, apply a `#[link_name]` attribute
            let link_name = self.mangled_name().unwrap_or_else(|| self.name());
            if !utils::names_will_be_identical_after_mangling(
                &canonical_name,
                link_name,
                None,
            ) {
                attrs.push(attributes::link_name(link_name));
            }

            let maybe_mut = if self.is_const() {
                quote! {}
            } else {
                quote! { mut }
            };

            let tokens = quote!(
                extern "C" {
                    #(#attrs)*
                    pub static #maybe_mut #canonical_ident: #ty;
                }
            );

            result.push(tokens);
        }
    }
}

impl CodeGenerator for Type {
    type Extra = Item;
    type Return = ();

    fn codegen<'a>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'a>,
        item: &Item,
    ) {
        debug!("<Type as CodeGenerator>::codegen: item = {:?}", item);
        debug_assert!(item.is_enabled_for_codegen(ctx));

        match *self.kind() {
            TypeKind::Void |
            TypeKind::NullPtr |
            TypeKind::Int(..) |
            TypeKind::Float(..) |
            TypeKind::Complex(..) |
            TypeKind::Array(..) |
            TypeKind::Vector(..) |
            TypeKind::Pointer(..) |
            TypeKind::Reference(..) |
            TypeKind::Function(..) |
            TypeKind::ResolvedTypeRef(..) |
            TypeKind::Opaque |
            TypeKind::TypeParam => {
                // These items don't need code generation, they only need to be
                // converted to rust types in fields, arguments, and such.
                // NOTE(emilio): If you add to this list, make sure to also add
                // it to BindgenContext::compute_allowlisted_and_codegen_items.
            }
            TypeKind::TemplateInstantiation(ref inst) => {
                inst.codegen(ctx, result, item)
            }
            TypeKind::BlockPointer(inner) => {
                if !ctx.options().generate_block {
                    return;
                }

                let inner_item =
                    inner.into_resolver().through_type_refs().resolve(ctx);
                let name = item.canonical_name(ctx);

                let inner_rust_type = {
                    if let TypeKind::Function(fnsig) =
                        inner_item.kind().expect_type().kind()
                    {
                        utils::fnsig_block(ctx, fnsig)
                    } else {
                        panic!("invalid block typedef: {:?}", inner_item)
                    }
                };

                let rust_name = ctx.rust_ident(&name);

                let mut tokens = if let Some(comment) = item.comment(ctx) {
                    attributes::doc(comment)
                } else {
                    quote! {}
                };

                tokens.append_all(quote! {
                    pub type #rust_name = #inner_rust_type ;
                });

                result.push(tokens);
                result.saw_block();
            }
            TypeKind::Comp(ref ci) => ci.codegen(ctx, result, item),
            TypeKind::TemplateAlias(inner, _) | TypeKind::Alias(inner) => {
                let inner_item =
                    inner.into_resolver().through_type_refs().resolve(ctx);
                let name = item.canonical_name(ctx);
                let path = item.canonical_path(ctx);

                {
                    let through_type_aliases = inner
                        .into_resolver()
                        .through_type_refs()
                        .through_type_aliases()
                        .resolve(ctx);

                    // Try to catch the common pattern:
                    //
                    // typedef struct foo { ... } foo;
                    //
                    // here, and also other more complex cases like #946.
                    if through_type_aliases.canonical_path(ctx) == path {
                        return;
                    }
                }

                // If this is a known named type, disallow generating anything
                // for it too.
                let spelling = self.name().expect("Unnamed alias?");
                if utils::type_from_named(ctx, spelling).is_some() {
                    return;
                }

                let mut outer_params = item.used_template_params(ctx);

                let is_opaque = item.is_opaque(ctx, &());
                let inner_rust_type = if is_opaque {
                    outer_params = vec![];
                    self.to_opaque(ctx, item)
                } else {
                    // Its possible that we have better layout information than
                    // the inner type does, so fall back to an opaque blob based
                    // on our layout if converting the inner item fails.
                    let mut inner_ty = inner_item
                        .try_to_rust_ty_or_opaque(ctx, &())
                        .unwrap_or_else(|_| self.to_opaque(ctx, item));
                    inner_ty.append_implicit_template_params(ctx, inner_item);
                    inner_ty
                };

                {
                    // FIXME(emilio): This is a workaround to avoid generating
                    // incorrect type aliases because of types that we haven't
                    // been able to resolve (because, eg, they depend on a
                    // template parameter).
                    //
                    // It's kind of a shame not generating them even when they
                    // could be referenced, but we already do the same for items
                    // with invalid template parameters, and at least this way
                    // they can be replaced, instead of generating plain invalid
                    // code.
                    let inner_canon_type =
                        inner_item.expect_type().canonical_type(ctx);
                    if inner_canon_type.is_invalid_type_param() {
                        warn!(
                            "Item contained invalid named type, skipping: \
                             {:?}, {:?}",
                            item, inner_item
                        );
                        return;
                    }
                }

                let rust_name = ctx.rust_ident(&name);

                let mut tokens = if let Some(comment) = item.comment(ctx) {
                    attributes::doc(comment)
                } else {
                    quote! {}
                };

                let alias_style = if ctx.options().type_alias.matches(&name) {
                    AliasVariation::TypeAlias
                } else if ctx.options().new_type_alias.matches(&name) {
                    AliasVariation::NewType
                } else if ctx.options().new_type_alias_deref.matches(&name) {
                    AliasVariation::NewTypeDeref
                } else {
                    ctx.options().default_alias_style
                };

                // We prefer using `pub use` over `pub type` because of:
                // https://github.com/rust-lang/rust/issues/26264
                // These are the only characters allowed in simple
                // paths, eg `good::dogs::Bront`.
                if inner_rust_type.to_string().chars().all(|c| matches!(c, 'A'..='Z' | 'a'..='z' | '0'..='9' | ':' | '_' | ' ')) && outer_params.is_empty() &&
                    !is_opaque &&
                    alias_style == AliasVariation::TypeAlias &&
                    inner_item.expect_type().canonical_type(ctx).is_enum()
                {
                    tokens.append_all(quote! {
                        pub use
                    });
                    let path = top_level_path(ctx, item);
                    tokens.append_separated(path, quote!(::));
                    tokens.append_all(quote! {
                        :: #inner_rust_type  as #rust_name ;
                    });
                    result.push(tokens);
                    return;
                }

                tokens.append_all(match alias_style {
                    AliasVariation::TypeAlias => quote! {
                        pub type #rust_name
                    },
                    AliasVariation::NewType | AliasVariation::NewTypeDeref => {
                        assert!(
                            ctx.options().rust_features().repr_transparent,
                            "repr_transparent feature is required to use {:?}",
                            alias_style
                        );

                        let mut attributes =
                            vec![attributes::repr("transparent")];
                        let packed = false; // Types can't be packed in Rust.
                        let derivable_traits =
                            derives_of_item(item, ctx, packed);
                        if !derivable_traits.is_empty() {
                            let derives: Vec<_> = derivable_traits.into();
                            attributes.push(attributes::derives(&derives))
                        }

                        quote! {
                            #( #attributes )*
                            pub struct #rust_name
                        }
                    }
                });

                let params: Vec<_> = outer_params
                    .into_iter()
                    .filter_map(|p| p.as_template_param(ctx, &()))
                    .collect();
                if params
                    .iter()
                    .any(|p| ctx.resolve_type(*p).is_invalid_type_param())
                {
                    warn!(
                        "Item contained invalid template \
                         parameter: {:?}",
                        item
                    );
                    return;
                }
                let params: Vec<_> = params
                    .iter()
                    .map(|p| {
                        p.try_to_rust_ty(ctx, &()).expect(
                            "type parameters can always convert to rust ty OK",
                        )
                    })
                    .collect();

                if !params.is_empty() {
                    tokens.append_all(quote! {
                        < #( #params ),* >
                    });
                }

                tokens.append_all(match alias_style {
                    AliasVariation::TypeAlias => quote! {
                        = #inner_rust_type ;
                    },
                    AliasVariation::NewType | AliasVariation::NewTypeDeref => {
                        quote! {
                            (pub #inner_rust_type) ;
                        }
                    }
                });

                if alias_style == AliasVariation::NewTypeDeref {
                    let prefix = ctx.trait_prefix();
                    tokens.append_all(quote! {
                        impl ::#prefix::ops::Deref for #rust_name {
                            type Target = #inner_rust_type;
                            #[inline]
                            fn deref(&self) -> &Self::Target {
                                &self.0
                            }
                        }
                        impl ::#prefix::ops::DerefMut for #rust_name {
                            #[inline]
                            fn deref_mut(&mut self) -> &mut Self::Target {
                                &mut self.0
                            }
                        }
                    });
                }

                result.push(tokens);
            }
            TypeKind::Enum(ref ei) => ei.codegen(ctx, result, item),
            TypeKind::ObjCId | TypeKind::ObjCSel => {
                result.saw_objc();
            }
            TypeKind::ObjCInterface(ref interface) => {
                interface.codegen(ctx, result, item)
            }
            ref u @ TypeKind::UnresolvedTypeRef(..) => {
                unreachable!("Should have been resolved after parsing {:?}!", u)
            }
        }
    }
}

struct Vtable<'a> {
    item_id: ItemId,
    /// A reference to the originating compound object.
    #[allow(dead_code)]
    comp_info: &'a CompInfo,
}

impl<'a> Vtable<'a> {
    fn new(item_id: ItemId, comp_info: &'a CompInfo) -> Self {
        Vtable { item_id, comp_info }
    }
}

impl<'a> CodeGenerator for Vtable<'a> {
    type Extra = Item;
    type Return = ();

    fn codegen<'b>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'b>,
        item: &Item,
    ) {
        assert_eq!(item.id(), self.item_id);
        debug_assert!(item.is_enabled_for_codegen(ctx));
        let name = ctx.rust_ident(&self.canonical_name(ctx));

        // For now, we will only generate vtables for classes that:
        // - do not inherit from others (compilers merge VTable from primary parent class).
        // - do not contain a virtual destructor (requires ordering; platforms generate different vtables).
        if ctx.options().vtable_generation &&
            self.comp_info.base_members().is_empty() &&
            self.comp_info.destructor().is_none()
        {
            let class_ident = ctx.rust_ident(self.item_id.canonical_name(ctx));

            let methods = self
                .comp_info
                .methods()
                .iter()
                .filter_map(|m| {
                    if !m.is_virtual() {
                        return None;
                    }

                    let function_item = ctx.resolve_item(m.signature());
                    let function = function_item.expect_function();
                    let signature_item = ctx.resolve_item(function.signature());
                    let signature = match signature_item.expect_type().kind() {
                        TypeKind::Function(ref sig) => sig,
                        _ => panic!("Function signature type mismatch"),
                    };

                    // FIXME: Is there a canonical name without the class prepended?
                    let function_name = function_item.canonical_name(ctx);

                    // FIXME: Need to account for overloading with times_seen (separately from regular function path).
                    let function_name = ctx.rust_ident(function_name);
                    let mut args = utils::fnsig_arguments(ctx, signature);
                    let ret = utils::fnsig_return_ty(ctx, signature);

                    args[0] = if m.is_const() {
                        quote! { this: *const #class_ident }
                    } else {
                        quote! { this: *mut #class_ident }
                    };

                    Some(quote! {
                        pub #function_name : unsafe extern "C" fn( #( #args ),* ) #ret
                    })
                })
                .collect::<Vec<_>>();

            result.push(quote! {
                #[repr(C)]
                pub struct #name {
                    #( #methods ),*
                }
            })
        } else {
            // For the cases we don't support, simply generate an empty struct.
            let void = helpers::ast_ty::c_void(ctx);

            result.push(quote! {
                #[repr(C)]
                pub struct #name ( #void );
            });
        }
    }
}

impl<'a> ItemCanonicalName for Vtable<'a> {
    fn canonical_name(&self, ctx: &BindgenContext) -> String {
        format!("{}__bindgen_vtable", self.item_id.canonical_name(ctx))
    }
}

impl<'a> TryToRustTy for Vtable<'a> {
    type Extra = ();

    fn try_to_rust_ty(
        &self,
        ctx: &BindgenContext,
        _: &(),
    ) -> error::Result<proc_macro2::TokenStream> {
        let name = ctx.rust_ident(self.canonical_name(ctx));
        Ok(quote! {
            #name
        })
    }
}

impl CodeGenerator for TemplateInstantiation {
    type Extra = Item;
    type Return = ();

    fn codegen<'a>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'a>,
        item: &Item,
    ) {
        debug_assert!(item.is_enabled_for_codegen(ctx));

        // Although uses of instantiations don't need code generation, and are
        // just converted to rust types in fields, vars, etc, we take this
        // opportunity to generate tests for their layout here. If the
        // instantiation is opaque, then its presumably because we don't
        // properly understand it (maybe because of specializations), and so we
        // shouldn't emit layout tests either.
        if !ctx.options().layout_tests || self.is_opaque(ctx, item) {
            return;
        }

        // If there are any unbound type parameters, then we can't generate a
        // layout test because we aren't dealing with a concrete type with a
        // concrete size and alignment.
        if ctx.uses_any_template_parameters(item.id()) {
            return;
        }

        let layout = item.kind().expect_type().layout(ctx);

        if let Some(layout) = layout {
            let size = layout.size;
            let align = layout.align;

            let name = item.full_disambiguated_name(ctx);
            let mut fn_name =
                format!("__bindgen_test_layout_{}_instantiation", name);
            let times_seen = result.overload_number(&fn_name);
            if times_seen > 0 {
                write!(&mut fn_name, "_{}", times_seen).unwrap();
            }

            let fn_name = ctx.rust_ident_raw(fn_name);

            let prefix = ctx.trait_prefix();
            let ident = item.to_rust_ty_or_opaque(ctx, &());
            let size_of_expr = quote! {
                ::#prefix::mem::size_of::<#ident>()
            };
            let align_of_expr = quote! {
                ::#prefix::mem::align_of::<#ident>()
            };

            let item = quote! {
                #[test]
                fn #fn_name() {
                    assert_eq!(#size_of_expr, #size,
                               concat!("Size of template specialization: ",
                                       stringify!(#ident)));
                    assert_eq!(#align_of_expr, #align,
                               concat!("Alignment of template specialization: ",
                                       stringify!(#ident)));
                }
            };

            result.push(item);
        }
    }
}

/// Trait for implementing the code generation of a struct or union field.
trait FieldCodegen<'a> {
    type Extra;

    fn codegen<F, M>(
        &self,
        ctx: &BindgenContext,
        fields_should_be_private: bool,
        codegen_depth: usize,
        accessor_kind: FieldAccessorKind,
        parent: &CompInfo,
        result: &mut CodegenResult,
        struct_layout: &mut StructLayoutTracker,
        fields: &mut F,
        methods: &mut M,
        extra: Self::Extra,
    ) where
        F: Extend<proc_macro2::TokenStream>,
        M: Extend<proc_macro2::TokenStream>;
}

impl<'a> FieldCodegen<'a> for Field {
    type Extra = ();

    fn codegen<F, M>(
        &self,
        ctx: &BindgenContext,
        fields_should_be_private: bool,
        codegen_depth: usize,
        accessor_kind: FieldAccessorKind,
        parent: &CompInfo,
        result: &mut CodegenResult,
        struct_layout: &mut StructLayoutTracker,
        fields: &mut F,
        methods: &mut M,
        _: (),
    ) where
        F: Extend<proc_macro2::TokenStream>,
        M: Extend<proc_macro2::TokenStream>,
    {
        match *self {
            Field::DataMember(ref data) => {
                data.codegen(
                    ctx,
                    fields_should_be_private,
                    codegen_depth,
                    accessor_kind,
                    parent,
                    result,
                    struct_layout,
                    fields,
                    methods,
                    (),
                );
            }
            Field::Bitfields(ref unit) => {
                unit.codegen(
                    ctx,
                    fields_should_be_private,
                    codegen_depth,
                    accessor_kind,
                    parent,
                    result,
                    struct_layout,
                    fields,
                    methods,
                    (),
                );
            }
        }
    }
}

impl<'a> FieldCodegen<'a> for FieldData {
    type Extra = ();

    fn codegen<F, M>(
        &self,
        ctx: &BindgenContext,
        fields_should_be_private: bool,
        codegen_depth: usize,
        accessor_kind: FieldAccessorKind,
        parent: &CompInfo,
        result: &mut CodegenResult,
        struct_layout: &mut StructLayoutTracker,
        fields: &mut F,
        methods: &mut M,
        _: (),
    ) where
        F: Extend<proc_macro2::TokenStream>,
        M: Extend<proc_macro2::TokenStream>,
    {
        // Bitfields are handled by `FieldCodegen` implementations for
        // `BitfieldUnit` and `Bitfield`.
        assert!(self.bitfield_width().is_none());

        let field_item =
            self.ty().into_resolver().through_type_refs().resolve(ctx);
        let field_ty = field_item.expect_type();
        let mut ty = self.ty().to_rust_ty_or_opaque(ctx, &());
        ty.append_implicit_template_params(ctx, field_item);

        // NB: If supported, we use proper `union` types.
        let ty = if parent.is_union() && !struct_layout.is_rust_union() {
            result.saw_bindgen_union();
            if ctx.options().enable_cxx_namespaces {
                quote! {
                    root::__BindgenUnionField<#ty>
                }
            } else {
                quote! {
                    __BindgenUnionField<#ty>
                }
            }
        } else if let Some(item) = field_ty.is_incomplete_array(ctx) {
            result.saw_incomplete_array();

            let inner = item.to_rust_ty_or_opaque(ctx, &());

            if ctx.options().enable_cxx_namespaces {
                quote! {
                    root::__IncompleteArrayField<#inner>
                }
            } else {
                quote! {
                    __IncompleteArrayField<#inner>
                }
            }
        } else {
            ty
        };

        let mut field = quote! {};
        if ctx.options().generate_comments {
            if let Some(raw_comment) = self.comment() {
                let comment =
                    comment::preprocess(raw_comment, codegen_depth + 1);
                field = attributes::doc(comment);
            }
        }

        let field_name = self
            .name()
            .map(|name| ctx.rust_mangle(name).into_owned())
            .expect("Each field should have a name in codegen!");
        let field_ident = ctx.rust_ident_raw(field_name.as_str());

        if let Some(padding_field) =
            struct_layout.saw_field(&field_name, field_ty, self.offset())
        {
            fields.extend(Some(padding_field));
        }

        let is_private = (!self.is_public() &&
            ctx.options().respect_cxx_access_specs) ||
            self.annotations()
                .private_fields()
                .unwrap_or(fields_should_be_private);

        let accessor_kind =
            self.annotations().accessor_kind().unwrap_or(accessor_kind);

        if is_private {
            field.append_all(quote! {
                #field_ident : #ty ,
            });
        } else {
            field.append_all(quote! {
                pub #field_ident : #ty ,
            });
        }

        fields.extend(Some(field));

        // TODO: Factor the following code out, please!
        if accessor_kind == FieldAccessorKind::None {
            return;
        }

        let getter_name = ctx.rust_ident_raw(format!("get_{}", field_name));
        let mutable_getter_name =
            ctx.rust_ident_raw(format!("get_{}_mut", field_name));
        let field_name = ctx.rust_ident_raw(field_name);

        methods.extend(Some(match accessor_kind {
            FieldAccessorKind::None => unreachable!(),
            FieldAccessorKind::Regular => {
                quote! {
                    #[inline]
                    pub fn #getter_name(&self) -> & #ty {
                        &self.#field_name
                    }

                    #[inline]
                    pub fn #mutable_getter_name(&mut self) -> &mut #ty {
                        &mut self.#field_name
                    }
                }
            }
            FieldAccessorKind::Unsafe => {
                quote! {
                    #[inline]
                    pub unsafe fn #getter_name(&self) -> & #ty {
                        &self.#field_name
                    }

                    #[inline]
                    pub unsafe fn #mutable_getter_name(&mut self) -> &mut #ty {
                        &mut self.#field_name
                    }
                }
            }
            FieldAccessorKind::Immutable => {
                quote! {
                    #[inline]
                    pub fn #getter_name(&self) -> & #ty {
                        &self.#field_name
                    }
                }
            }
        }));
    }
}

impl BitfieldUnit {
    /// Get the constructor name for this bitfield unit.
    fn ctor_name(&self) -> proc_macro2::TokenStream {
        let ctor_name = Ident::new(
            &format!("new_bitfield_{}", self.nth()),
            Span::call_site(),
        );
        quote! {
            #ctor_name
        }
    }
}

impl Bitfield {
    /// Extend an under construction bitfield unit constructor with this
    /// bitfield. This sets the relevant bits on the `__bindgen_bitfield_unit`
    /// variable that's being constructed.
    fn extend_ctor_impl(
        &self,
        ctx: &BindgenContext,
        param_name: proc_macro2::TokenStream,
        mut ctor_impl: proc_macro2::TokenStream,
    ) -> proc_macro2::TokenStream {
        let bitfield_ty = ctx.resolve_type(self.ty());
        let bitfield_ty_layout = bitfield_ty
            .layout(ctx)
            .expect("Bitfield without layout? Gah!");
        let bitfield_int_ty = helpers::integer_type(ctx, bitfield_ty_layout)
            .expect(
                "Should already have verified that the bitfield is \
                 representable as an int",
            );

        let offset = self.offset_into_unit();
        let width = self.width() as u8;
        let prefix = ctx.trait_prefix();

        ctor_impl.append_all(quote! {
            __bindgen_bitfield_unit.set(
                #offset,
                #width,
                {
                    let #param_name: #bitfield_int_ty = unsafe {
                        ::#prefix::mem::transmute(#param_name)
                    };
                    #param_name as u64
                }
            );
        });

        ctor_impl
    }
}

fn access_specifier(
    ctx: &BindgenContext,
    is_pub: bool,
) -> proc_macro2::TokenStream {
    if is_pub || !ctx.options().respect_cxx_access_specs {
        quote! { pub }
    } else {
        quote! {}
    }
}

impl<'a> FieldCodegen<'a> for BitfieldUnit {
    type Extra = ();

    fn codegen<F, M>(
        &self,
        ctx: &BindgenContext,
        fields_should_be_private: bool,
        codegen_depth: usize,
        accessor_kind: FieldAccessorKind,
        parent: &CompInfo,
        result: &mut CodegenResult,
        struct_layout: &mut StructLayoutTracker,
        fields: &mut F,
        methods: &mut M,
        _: (),
    ) where
        F: Extend<proc_macro2::TokenStream>,
        M: Extend<proc_macro2::TokenStream>,
    {
        use crate::ir::ty::RUST_DERIVE_IN_ARRAY_LIMIT;

        result.saw_bitfield_unit();

        let layout = self.layout();
        let unit_field_ty = helpers::bitfield_unit(ctx, layout);
        let field_ty = {
            if parent.is_union() && !struct_layout.is_rust_union() {
                result.saw_bindgen_union();
                if ctx.options().enable_cxx_namespaces {
                    quote! {
                        root::__BindgenUnionField<#unit_field_ty>
                    }
                } else {
                    quote! {
                        __BindgenUnionField<#unit_field_ty>
                    }
                }
            } else {
                unit_field_ty.clone()
            }
        };

        {
            let align_field_name = format!("_bitfield_align_{}", self.nth());
            let align_field_ident = ctx.rust_ident(&align_field_name);
            let align_ty = match self.layout().align {
                n if n >= 8 => quote! { u64 },
                4 => quote! { u32 },
                2 => quote! { u16 },
                _ => quote! { u8  },
            };
            let align_field = quote! {
                pub #align_field_ident: [#align_ty; 0],
            };
            fields.extend(Some(align_field));
        }

        let unit_field_name = format!("_bitfield_{}", self.nth());
        let unit_field_ident = ctx.rust_ident(&unit_field_name);

        let ctor_name = self.ctor_name();
        let mut ctor_params = vec![];
        let mut ctor_impl = quote! {};

        // We cannot generate any constructor if the underlying storage can't
        // implement AsRef<[u8]> / AsMut<[u8]> / etc, or can't derive Default.
        //
        // We don't check `larger_arrays` here because Default does still have
        // the 32 items limitation.
        let mut generate_ctor = layout.size <= RUST_DERIVE_IN_ARRAY_LIMIT;

        let mut access_spec = !fields_should_be_private;
        for bf in self.bitfields() {
            // Codegen not allowed for anonymous bitfields
            if bf.name().is_none() {
                continue;
            }

            if layout.size > RUST_DERIVE_IN_ARRAY_LIMIT &&
                !ctx.options().rust_features().larger_arrays
            {
                continue;
            }

            access_spec &= bf.is_public();
            let mut bitfield_representable_as_int = true;

            bf.codegen(
                ctx,
                fields_should_be_private,
                codegen_depth,
                accessor_kind,
                parent,
                result,
                struct_layout,
                fields,
                methods,
                (&unit_field_name, &mut bitfield_representable_as_int),
            );

            // Generating a constructor requires the bitfield to be representable as an integer.
            if !bitfield_representable_as_int {
                generate_ctor = false;
                continue;
            }

            let param_name = bitfield_getter_name(ctx, bf);
            let bitfield_ty_item = ctx.resolve_item(bf.ty());
            let bitfield_ty = bitfield_ty_item.expect_type();
            let bitfield_ty =
                bitfield_ty.to_rust_ty_or_opaque(ctx, bitfield_ty_item);

            ctor_params.push(quote! {
                #param_name : #bitfield_ty
            });
            ctor_impl = bf.extend_ctor_impl(ctx, param_name, ctor_impl);
        }

        let access_spec = access_specifier(ctx, access_spec);

        let field = quote! {
            #access_spec #unit_field_ident : #field_ty ,
        };
        fields.extend(Some(field));

        if generate_ctor {
            methods.extend(Some(quote! {
                #[inline]
                #access_spec fn #ctor_name ( #( #ctor_params ),* ) -> #unit_field_ty {
                    let mut __bindgen_bitfield_unit: #unit_field_ty = Default::default();
                    #ctor_impl
                    __bindgen_bitfield_unit
                }
            }));
        }

        struct_layout.saw_bitfield_unit(layout);
    }
}

fn bitfield_getter_name(
    ctx: &BindgenContext,
    bitfield: &Bitfield,
) -> proc_macro2::TokenStream {
    let name = bitfield.getter_name();
    let name = ctx.rust_ident_raw(name);
    quote! { #name }
}

fn bitfield_setter_name(
    ctx: &BindgenContext,
    bitfield: &Bitfield,
) -> proc_macro2::TokenStream {
    let setter = bitfield.setter_name();
    let setter = ctx.rust_ident_raw(setter);
    quote! { #setter }
}

impl<'a> FieldCodegen<'a> for Bitfield {
    type Extra = (&'a str, &'a mut bool);

    fn codegen<F, M>(
        &self,
        ctx: &BindgenContext,
        fields_should_be_private: bool,
        _codegen_depth: usize,
        _accessor_kind: FieldAccessorKind,
        parent: &CompInfo,
        _result: &mut CodegenResult,
        struct_layout: &mut StructLayoutTracker,
        _fields: &mut F,
        methods: &mut M,
        (unit_field_name, bitfield_representable_as_int): (&'a str, &mut bool),
    ) where
        F: Extend<proc_macro2::TokenStream>,
        M: Extend<proc_macro2::TokenStream>,
    {
        let prefix = ctx.trait_prefix();
        let getter_name = bitfield_getter_name(ctx, self);
        let setter_name = bitfield_setter_name(ctx, self);
        let unit_field_ident = Ident::new(unit_field_name, Span::call_site());

        let bitfield_ty_item = ctx.resolve_item(self.ty());
        let bitfield_ty = bitfield_ty_item.expect_type();

        let bitfield_ty_layout = bitfield_ty
            .layout(ctx)
            .expect("Bitfield without layout? Gah!");
        let bitfield_int_ty =
            match helpers::integer_type(ctx, bitfield_ty_layout) {
                Some(int_ty) => {
                    *bitfield_representable_as_int = true;
                    int_ty
                }
                None => {
                    *bitfield_representable_as_int = false;
                    return;
                }
            };

        let bitfield_ty =
            bitfield_ty.to_rust_ty_or_opaque(ctx, bitfield_ty_item);

        let offset = self.offset_into_unit();
        let width = self.width() as u8;
        let access_spec = access_specifier(
            ctx,
            self.is_public() && !fields_should_be_private,
        );

        if parent.is_union() && !struct_layout.is_rust_union() {
            methods.extend(Some(quote! {
                #[inline]
                #access_spec fn #getter_name(&self) -> #bitfield_ty {
                    unsafe {
                        ::#prefix::mem::transmute(
                            self.#unit_field_ident.as_ref().get(#offset, #width)
                                as #bitfield_int_ty
                        )
                    }
                }

                #[inline]
                #access_spec fn #setter_name(&mut self, val: #bitfield_ty) {
                    unsafe {
                        let val: #bitfield_int_ty = ::#prefix::mem::transmute(val);
                        self.#unit_field_ident.as_mut().set(
                            #offset,
                            #width,
                            val as u64
                        )
                    }
                }
            }));
        } else {
            methods.extend(Some(quote! {
                #[inline]
                #access_spec fn #getter_name(&self) -> #bitfield_ty {
                    unsafe {
                        ::#prefix::mem::transmute(
                            self.#unit_field_ident.get(#offset, #width)
                                as #bitfield_int_ty
                        )
                    }
                }

                #[inline]
                #access_spec fn #setter_name(&mut self, val: #bitfield_ty) {
                    unsafe {
                        let val: #bitfield_int_ty = ::#prefix::mem::transmute(val);
                        self.#unit_field_ident.set(
                            #offset,
                            #width,
                            val as u64
                        )
                    }
                }
            }));
        }
    }
}

impl CodeGenerator for CompInfo {
    type Extra = Item;
    type Return = ();

    fn codegen<'a>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'a>,
        item: &Item,
    ) {
        debug!("<CompInfo as CodeGenerator>::codegen: item = {:?}", item);
        debug_assert!(item.is_enabled_for_codegen(ctx));

        // Don't output classes with template parameters that aren't types, and
        // also don't output template specializations, neither total or partial.
        if self.has_non_type_template_params() {
            return;
        }

        let ty = item.expect_type();
        let layout = ty.layout(ctx);
        let mut packed = self.is_packed(ctx, layout.as_ref());

        let canonical_name = item.canonical_name(ctx);
        let canonical_ident = ctx.rust_ident(&canonical_name);

        // Generate the vtable from the method list if appropriate.
        //
        // TODO: I don't know how this could play with virtual methods that are
        // not in the list of methods found by us, we'll see. Also, could the
        // order of the vtable pointers vary?
        //
        // FIXME: Once we generate proper vtables, we need to codegen the
        // vtable, but *not* generate a field for it in the case that
        // HasVtable::has_vtable_ptr is false but HasVtable::has_vtable is true.
        //
        // Also, we need to generate the vtable in such a way it "inherits" from
        // the parent too.
        let is_opaque = item.is_opaque(ctx, &());
        let mut fields = vec![];
        let mut struct_layout =
            StructLayoutTracker::new(ctx, self, ty, &canonical_name);

        if !is_opaque {
            if item.has_vtable_ptr(ctx) {
                let vtable = Vtable::new(item.id(), self);
                vtable.codegen(ctx, result, item);

                let vtable_type = vtable
                    .try_to_rust_ty(ctx, &())
                    .expect("vtable to Rust type conversion is infallible")
                    .to_ptr(true);

                fields.push(quote! {
                    pub vtable_: #vtable_type ,
                });

                struct_layout.saw_vtable();
            }

            for base in self.base_members() {
                if !base.requires_storage(ctx) {
                    continue;
                }

                let inner_item = ctx.resolve_item(base.ty);
                let mut inner = inner_item.to_rust_ty_or_opaque(ctx, &());
                inner.append_implicit_template_params(ctx, inner_item);
                let field_name = ctx.rust_ident(&base.field_name);

                struct_layout.saw_base(inner_item.expect_type());

                let access_spec = access_specifier(ctx, base.is_public());
                fields.push(quote! {
                    #access_spec #field_name: #inner,
                });
            }
        }

        let mut methods = vec![];
        if !is_opaque {
            let codegen_depth = item.codegen_depth(ctx);
            let fields_should_be_private =
                item.annotations().private_fields().unwrap_or(false);
            let struct_accessor_kind = item
                .annotations()
                .accessor_kind()
                .unwrap_or(FieldAccessorKind::None);
            for field in self.fields() {
                field.codegen(
                    ctx,
                    fields_should_be_private,
                    codegen_depth,
                    struct_accessor_kind,
                    self,
                    result,
                    &mut struct_layout,
                    &mut fields,
                    &mut methods,
                    (),
                );
            }
            // Check whether an explicit padding field is needed
            // at the end.
            if let Some(comp_layout) = layout {
                fields.extend(
                    struct_layout
                        .add_tail_padding(&canonical_name, comp_layout),
                );
            }
        }

        if is_opaque {
            // Opaque item should not have generated methods, fields.
            debug_assert!(fields.is_empty());
            debug_assert!(methods.is_empty());
        }

        let is_union = self.kind() == CompKind::Union;
        let layout = item.kind().expect_type().layout(ctx);
        let zero_sized = item.is_zero_sized(ctx);
        let forward_decl = self.is_forward_declaration();

        let mut explicit_align = None;

        // C++ requires every struct to be addressable, so what C++ compilers do
        // is making the struct 1-byte sized.
        //
        // This is apparently not the case for C, see:
        // https://github.com/rust-lang/rust-bindgen/issues/551
        //
        // Just get the layout, and assume C++ if not.
        //
        // NOTE: This check is conveniently here to avoid the dummy fields we
        // may add for unused template parameters.
        if !forward_decl && zero_sized {
            let has_address = if is_opaque {
                // Generate the address field if it's an opaque type and
                // couldn't determine the layout of the blob.
                layout.is_none()
            } else {
                layout.map_or(true, |l| l.size != 0)
            };

            if has_address {
                let layout = Layout::new(1, 1);
                let ty = helpers::blob(ctx, Layout::new(1, 1));
                struct_layout.saw_field_with_layout(
                    "_address",
                    layout,
                    /* offset = */ Some(0),
                );
                fields.push(quote! {
                    pub _address: #ty,
                });
            }
        }

        if is_opaque {
            match layout {
                Some(l) => {
                    explicit_align = Some(l.align);

                    let ty = helpers::blob(ctx, l);
                    fields.push(quote! {
                        pub _bindgen_opaque_blob: #ty ,
                    });
                }
                None => {
                    warn!("Opaque type without layout! Expect dragons!");
                }
            }
        } else if !is_union && !zero_sized {
            if let Some(padding_field) =
                layout.and_then(|layout| struct_layout.pad_struct(layout))
            {
                fields.push(padding_field);
            }

            if let Some(layout) = layout {
                if struct_layout.requires_explicit_align(layout) {
                    if layout.align == 1 {
                        packed = true;
                    } else {
                        explicit_align = Some(layout.align);
                        if !ctx.options().rust_features.repr_align {
                            let ty = helpers::blob(
                                ctx,
                                Layout::new(0, layout.align),
                            );
                            fields.push(quote! {
                                pub __bindgen_align: #ty ,
                            });
                        }
                    }
                }
            }
        } else if is_union && !forward_decl {
            // TODO(emilio): It'd be nice to unify this with the struct path
            // above somehow.
            let layout = layout.expect("Unable to get layout information?");
            if struct_layout.requires_explicit_align(layout) {
                explicit_align = Some(layout.align);
            }

            if !struct_layout.is_rust_union() {
                let ty = helpers::blob(ctx, layout);
                fields.push(quote! {
                    pub bindgen_union_field: #ty ,
                })
            }
        }

        if forward_decl {
            fields.push(quote! {
                _unused: [u8; 0],
            });
        }

        let mut generic_param_names = vec![];

        for (idx, ty) in item.used_template_params(ctx).iter().enumerate() {
            let param = ctx.resolve_type(*ty);
            let name = param.name().unwrap();
            let ident = ctx.rust_ident(name);
            generic_param_names.push(ident.clone());

            let prefix = ctx.trait_prefix();
            let field_name = ctx.rust_ident(format!("_phantom_{}", idx));
            fields.push(quote! {
                pub #field_name : ::#prefix::marker::PhantomData<
                    ::#prefix::cell::UnsafeCell<#ident>
                > ,
            });
        }

        let generics = if !generic_param_names.is_empty() {
            let generic_param_names = generic_param_names.clone();
            quote! {
                < #( #generic_param_names ),* >
            }
        } else {
            quote! {}
        };

        let mut attributes = vec![];
        let mut needs_clone_impl = false;
        let mut needs_default_impl = false;
        let mut needs_debug_impl = false;
        let mut needs_partialeq_impl = false;
        if let Some(comment) = item.comment(ctx) {
            attributes.push(attributes::doc(comment));
        }
        if packed && !is_opaque {
            let n = layout.map_or(1, |l| l.align);
            assert!(ctx.options().rust_features().repr_packed_n || n == 1);
            let packed_repr = if n == 1 {
                "packed".to_string()
            } else {
                format!("packed({})", n)
            };
            attributes.push(attributes::repr_list(&["C", &packed_repr]));
        } else {
            attributes.push(attributes::repr("C"));
        }

        if ctx.options().rust_features().repr_align {
            if let Some(explicit) = explicit_align {
                // Ensure that the struct has the correct alignment even in
                // presence of alignas.
                let explicit = helpers::ast_ty::int_expr(explicit as i64);
                attributes.push(quote! {
                    #[repr(align(#explicit))]
                });
            }
        }

        let derivable_traits = derives_of_item(item, ctx, packed);
        if !derivable_traits.contains(DerivableTraits::DEBUG) {
            needs_debug_impl = ctx.options().derive_debug &&
                ctx.options().impl_debug &&
                !ctx.no_debug_by_name(item) &&
                !item.annotations().disallow_debug();
        }

        if !derivable_traits.contains(DerivableTraits::DEFAULT) {
            needs_default_impl = ctx.options().derive_default &&
                !self.is_forward_declaration() &&
                !ctx.no_default_by_name(item) &&
                !item.annotations().disallow_default();
        }

        let all_template_params = item.all_template_params(ctx);

        if derivable_traits.contains(DerivableTraits::COPY) &&
            !derivable_traits.contains(DerivableTraits::CLONE)
        {
            needs_clone_impl = true;
        }

        if !derivable_traits.contains(DerivableTraits::PARTIAL_EQ) {
            needs_partialeq_impl = ctx.options().derive_partialeq &&
                ctx.options().impl_partialeq &&
                ctx.lookup_can_derive_partialeq_or_partialord(item.id()) ==
                    CanDerive::Manually;
        }

        let mut derives: Vec<_> = derivable_traits.into();
        derives.extend(item.annotations().derives().iter().map(String::as_str));

        // The custom derives callback may return a list of derive attributes;
        // add them to the end of the list.
        let custom_derives;
        if let Some(cb) = &ctx.options().parse_callbacks {
            custom_derives = cb.add_derives(&canonical_name);
            // In most cases this will be a no-op, since custom_derives will be empty.
            derives.extend(custom_derives.iter().map(|s| s.as_str()));
        };

        if !derives.is_empty() {
            attributes.push(attributes::derives(&derives))
        }

        if item.must_use(ctx) {
            attributes.push(attributes::must_use());
        }

        let mut tokens = if is_union && struct_layout.is_rust_union() {
            quote! {
                #( #attributes )*
                pub union #canonical_ident
            }
        } else {
            quote! {
                #( #attributes )*
                pub struct #canonical_ident
            }
        };

        tokens.append_all(quote! {
            #generics {
                #( #fields )*
            }
        });
        result.push(tokens);

        // Generate the inner types and all that stuff.
        //
        // TODO: In the future we might want to be smart, and use nested
        // modules, and whatnot.
        for ty in self.inner_types() {
            let child_item = ctx.resolve_item(*ty);
            // assert_eq!(child_item.parent_id(), item.id());
            child_item.codegen(ctx, result, &());
        }

        // NOTE: Some unexposed attributes (like alignment attributes) may
        // affect layout, so we're bad and pray to the gods for avoid sending
        // all the tests to shit when parsing things like max_align_t.
        if self.found_unknown_attr() {
            warn!(
                "Type {} has an unknown attribute that may affect layout",
                canonical_ident
            );
        }

        if all_template_params.is_empty() {
            if !is_opaque {
                for var in self.inner_vars() {
                    ctx.resolve_item(*var).codegen(ctx, result, &());
                }
            }

            if ctx.options().layout_tests && !self.is_forward_declaration() {
                if let Some(layout) = layout {
                    let fn_name =
                        format!("bindgen_test_layout_{}", canonical_ident);
                    let fn_name = ctx.rust_ident_raw(fn_name);
                    let prefix = ctx.trait_prefix();
                    let size_of_expr = quote! {
                        ::#prefix::mem::size_of::<#canonical_ident>()
                    };
                    let align_of_expr = quote! {
                        ::#prefix::mem::align_of::<#canonical_ident>()
                    };
                    let size = layout.size;
                    let align = layout.align;

                    let check_struct_align = if align >
                        ctx.target_pointer_size() &&
                        !ctx.options().rust_features().repr_align
                    {
                        None
                    } else {
                        Some(quote! {
                            assert_eq!(#align_of_expr,
                                   #align,
                                   concat!("Alignment of ", stringify!(#canonical_ident)));

                        })
                    };

                    // FIXME when [issue #465](https://github.com/rust-lang/rust-bindgen/issues/465) ready
                    let too_many_base_vtables = self
                        .base_members()
                        .iter()
                        .filter(|base| base.ty.has_vtable(ctx))
                        .count() >
                        1;

                    let should_skip_field_offset_checks =
                        is_opaque || too_many_base_vtables;

                    let check_field_offset = if should_skip_field_offset_checks
                    {
                        vec![]
                    } else {
                        self.fields()
                            .iter()
                            .filter_map(|field| match *field {
                                Field::DataMember(ref f) if f.name().is_some() => Some(f),
                                _ => None,
                            })
                            .flat_map(|field| {
                                let name = field.name().unwrap();
                                field.offset().map(|offset| {
                                    let field_offset = offset / 8;
                                    let field_name = ctx.rust_ident(name);
                                    // Put each check in its own function, so
                                    // that rustc with opt-level=0 doesn't take
                                    // too much stack space, see #2218.
                                    let test_fn = Ident::new(&format!("test_field_{}", name), Span::call_site());
                                    quote! {
                                        fn #test_fn() {
                                            assert_eq!(
                                                unsafe {
                                                    let uninit = ::#prefix::mem::MaybeUninit::<#canonical_ident>::uninit();
                                                    let ptr = uninit.as_ptr();
                                                    ::#prefix::ptr::addr_of!((*ptr).#field_name) as usize - ptr as usize
                                                },
                                                #field_offset,
                                                concat!("Offset of field: ", stringify!(#canonical_ident), "::", stringify!(#field_name))
                                            );
                                        }
                                        #test_fn();
                                    }
                                })
                            })
                            .collect::<Vec<proc_macro2::TokenStream>>()
                    };

                    let item = quote! {
                        #[test]
                        fn #fn_name() {
                            assert_eq!(#size_of_expr,
                                       #size,
                                       concat!("Size of: ", stringify!(#canonical_ident)));

                            #check_struct_align
                            #( #check_field_offset )*
                        }
                    };
                    result.push(item);
                }
            }

            let mut method_names = Default::default();
            if ctx.options().codegen_config.methods() {
                for method in self.methods() {
                    assert!(method.kind() != MethodKind::Constructor);
                    method.codegen_method(
                        ctx,
                        &mut methods,
                        &mut method_names,
                        result,
                        self,
                    );
                }
            }

            if ctx.options().codegen_config.constructors() {
                for sig in self.constructors() {
                    Method::new(
                        MethodKind::Constructor,
                        *sig,
                        /* const */
                        false,
                    )
                    .codegen_method(
                        ctx,
                        &mut methods,
                        &mut method_names,
                        result,
                        self,
                    );
                }
            }

            if ctx.options().codegen_config.destructors() {
                if let Some((kind, destructor)) = self.destructor() {
                    debug_assert!(kind.is_destructor());
                    Method::new(kind, destructor, false).codegen_method(
                        ctx,
                        &mut methods,
                        &mut method_names,
                        result,
                        self,
                    );
                }
            }
        }

        // NB: We can't use to_rust_ty here since for opaque types this tries to
        // use the specialization knowledge to generate a blob field.
        let ty_for_impl = quote! {
            #canonical_ident #generics
        };

        if needs_clone_impl {
            result.push(quote! {
                impl #generics Clone for #ty_for_impl {
                    fn clone(&self) -> Self { *self }
                }
            });
        }

        if needs_default_impl {
            let prefix = ctx.trait_prefix();
            let body = if ctx.options().rust_features().maybe_uninit {
                quote! {
                    let mut s = ::#prefix::mem::MaybeUninit::<Self>::uninit();
                    unsafe {
                        ::#prefix::ptr::write_bytes(s.as_mut_ptr(), 0, 1);
                        s.assume_init()
                    }
                }
            } else {
                quote! {
                    unsafe {
                        let mut s: Self = ::#prefix::mem::uninitialized();
                        ::#prefix::ptr::write_bytes(&mut s, 0, 1);
                        s
                    }
                }
            };
            // Note we use `ptr::write_bytes()` instead of `mem::zeroed()` because the latter does
            // not necessarily ensure padding bytes are zeroed. Some C libraries are sensitive to
            // non-zero padding bytes, especially when forwards/backwards compatability is
            // involved.
            result.push(quote! {
                impl #generics Default for #ty_for_impl {
                    fn default() -> Self {
                        #body
                    }
                }
            });
        }

        if needs_debug_impl {
            let impl_ = impl_debug::gen_debug_impl(
                ctx,
                self.fields(),
                item,
                self.kind(),
            );

            let prefix = ctx.trait_prefix();

            result.push(quote! {
                impl #generics ::#prefix::fmt::Debug for #ty_for_impl {
                    #impl_
                }
            });
        }

        if needs_partialeq_impl {
            if let Some(impl_) = impl_partialeq::gen_partialeq_impl(
                ctx,
                self,
                item,
                &ty_for_impl,
            ) {
                let partialeq_bounds = if !generic_param_names.is_empty() {
                    let bounds = generic_param_names.iter().map(|t| {
                        quote! { #t: PartialEq }
                    });
                    quote! { where #( #bounds ),* }
                } else {
                    quote! {}
                };

                let prefix = ctx.trait_prefix();
                result.push(quote! {
                    impl #generics ::#prefix::cmp::PartialEq for #ty_for_impl #partialeq_bounds {
                        #impl_
                    }
                });
            }
        }

        if !methods.is_empty() {
            result.push(quote! {
                impl #generics #ty_for_impl {
                    #( #methods )*
                }
            });
        }
    }
}

trait MethodCodegen {
    fn codegen_method<'a>(
        &self,
        ctx: &BindgenContext,
        methods: &mut Vec<proc_macro2::TokenStream>,
        method_names: &mut HashMap<String, usize>,
        result: &mut CodegenResult<'a>,
        parent: &CompInfo,
    );
}

impl MethodCodegen for Method {
    fn codegen_method<'a>(
        &self,
        ctx: &BindgenContext,
        methods: &mut Vec<proc_macro2::TokenStream>,
        method_names: &mut HashMap<String, usize>,
        result: &mut CodegenResult<'a>,
        _parent: &CompInfo,
    ) {
        assert!({
            let cc = &ctx.options().codegen_config;
            match self.kind() {
                MethodKind::Constructor => cc.constructors(),
                MethodKind::Destructor => cc.destructors(),
                MethodKind::VirtualDestructor { .. } => cc.destructors(),
                MethodKind::Static |
                MethodKind::Normal |
                MethodKind::Virtual { .. } => cc.methods(),
            }
        });

        // TODO(emilio): We could generate final stuff at least.
        if self.is_virtual() {
            return; // FIXME
        }

        // First of all, output the actual function.
        let function_item = ctx.resolve_item(self.signature());
        if !function_item.process_before_codegen(ctx, result) {
            return;
        }
        let function = function_item.expect_function();
        let times_seen = function.codegen(ctx, result, function_item);
        let times_seen = match times_seen {
            Some(seen) => seen,
            None => return,
        };
        let signature_item = ctx.resolve_item(function.signature());
        let mut name = match self.kind() {
            MethodKind::Constructor => "new".into(),
            MethodKind::Destructor => "destruct".into(),
            _ => function.name().to_owned(),
        };

        let signature = match *signature_item.expect_type().kind() {
            TypeKind::Function(ref sig) => sig,
            _ => panic!("How in the world?"),
        };

        let supported_abi = match signature.abi() {
            Abi::ThisCall => ctx.options().rust_features().thiscall_abi,
            Abi::Vectorcall => ctx.options().rust_features().vectorcall_abi,
            _ => true,
        };

        if !supported_abi {
            return;
        }

        // Do not generate variadic methods, since rust does not allow
        // implementing them, and we don't do a good job at it anyway.
        if signature.is_variadic() {
            return;
        }

        let count = {
            let count = method_names.entry(name.clone()).or_insert(0);
            *count += 1;
            *count - 1
        };

        if count != 0 {
            name.push_str(&count.to_string());
        }

        let mut function_name = function_item.canonical_name(ctx);
        if times_seen > 0 {
            write!(&mut function_name, "{}", times_seen).unwrap();
        }
        let function_name = ctx.rust_ident(function_name);
        let mut args = utils::fnsig_arguments(ctx, signature);
        let mut ret = utils::fnsig_return_ty(ctx, signature);

        if !self.is_static() && !self.is_constructor() {
            args[0] = if self.is_const() {
                quote! { &self }
            } else {
                quote! { &mut self }
            };
        }

        // If it's a constructor, we always return `Self`, and we inject the
        // "this" parameter, so there's no need to ask the user for it.
        //
        // Note that constructors in Clang are represented as functions with
        // return-type = void.
        if self.is_constructor() {
            args.remove(0);
            ret = quote! { -> Self };
        }

        let mut exprs =
            helpers::ast_ty::arguments_from_signature(signature, ctx);

        let mut stmts = vec![];

        // If it's a constructor, we need to insert an extra parameter with a
        // variable called `__bindgen_tmp` we're going to create.
        if self.is_constructor() {
            let prefix = ctx.trait_prefix();
            let tmp_variable_decl = if ctx
                .options()
                .rust_features()
                .maybe_uninit
            {
                exprs[0] = quote! {
                    __bindgen_tmp.as_mut_ptr()
                };
                quote! {
                    let mut __bindgen_tmp = ::#prefix::mem::MaybeUninit::uninit()
                }
            } else {
                exprs[0] = quote! {
                    &mut __bindgen_tmp
                };
                quote! {
                    let mut __bindgen_tmp = ::#prefix::mem::uninitialized()
                }
            };
            stmts.push(tmp_variable_decl);
        } else if !self.is_static() {
            assert!(!exprs.is_empty());
            exprs[0] = quote! {
                self
            };
        };

        let call = quote! {
            #function_name (#( #exprs ),* )
        };

        stmts.push(call);

        if self.is_constructor() {
            stmts.push(if ctx.options().rust_features().maybe_uninit {
                quote! {
                    __bindgen_tmp.assume_init()
                }
            } else {
                quote! {
                    __bindgen_tmp
                }
            })
        }

        let block = quote! {
            #( #stmts );*
        };

        let mut attrs = vec![attributes::inline()];

        if signature.must_use() &&
            ctx.options().rust_features().must_use_function
        {
            attrs.push(attributes::must_use());
        }

        let name = ctx.rust_ident(&name);
        methods.push(quote! {
            #(#attrs)*
            pub unsafe fn #name ( #( #args ),* ) #ret {
                #block
            }
        });
    }
}

/// A helper type that represents different enum variations.
#[derive(Copy, Clone, PartialEq, Debug)]
pub enum EnumVariation {
    /// The code for this enum will use a Rust enum. Note that creating this in unsafe code
    /// (including FFI) with an invalid value will invoke undefined behaviour, whether or not
    /// its marked as non_exhaustive.
    Rust {
        /// Indicates whether the generated struct should be `#[non_exhaustive]`
        non_exhaustive: bool,
    },
    /// The code for this enum will use a newtype
    NewType {
        /// Indicates whether the newtype will have bitwise operators
        is_bitfield: bool,
    },
    /// The code for this enum will use consts
    Consts,
    /// The code for this enum will use a module containing consts
    ModuleConsts,
}

impl EnumVariation {
    fn is_rust(&self) -> bool {
        matches!(*self, EnumVariation::Rust { .. })
    }

    /// Both the `Const` and `ModuleConsts` variants will cause this to return
    /// true.
    fn is_const(&self) -> bool {
        matches!(*self, EnumVariation::Consts | EnumVariation::ModuleConsts)
    }
}

impl Default for EnumVariation {
    fn default() -> EnumVariation {
        EnumVariation::Consts
    }
}

impl std::str::FromStr for EnumVariation {
    type Err = std::io::Error;

    /// Create a `EnumVariation` from a string.
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "rust" => Ok(EnumVariation::Rust {
                non_exhaustive: false,
            }),
            "rust_non_exhaustive" => Ok(EnumVariation::Rust {
                non_exhaustive: true,
            }),
            "bitfield" => Ok(EnumVariation::NewType { is_bitfield: true }),
            "consts" => Ok(EnumVariation::Consts),
            "moduleconsts" => Ok(EnumVariation::ModuleConsts),
            "newtype" => Ok(EnumVariation::NewType { is_bitfield: false }),
            _ => Err(std::io::Error::new(
                std::io::ErrorKind::InvalidInput,
                concat!(
                    "Got an invalid EnumVariation. Accepted values ",
                    "are 'rust', 'rust_non_exhaustive', 'bitfield', 'consts',",
                    "'moduleconsts', and 'newtype'."
                ),
            )),
        }
    }
}

/// A helper type to construct different enum variations.
enum EnumBuilder<'a> {
    Rust {
        codegen_depth: usize,
        attrs: Vec<proc_macro2::TokenStream>,
        ident: Ident,
        tokens: proc_macro2::TokenStream,
        emitted_any_variants: bool,
    },
    NewType {
        codegen_depth: usize,
        canonical_name: &'a str,
        tokens: proc_macro2::TokenStream,
        is_bitfield: bool,
    },
    Consts {
        variants: Vec<proc_macro2::TokenStream>,
        codegen_depth: usize,
    },
    ModuleConsts {
        codegen_depth: usize,
        module_name: &'a str,
        module_items: Vec<proc_macro2::TokenStream>,
    },
}

impl<'a> EnumBuilder<'a> {
    /// Returns the depth of the code generation for a variant of this enum.
    fn codegen_depth(&self) -> usize {
        match *self {
            EnumBuilder::Rust { codegen_depth, .. } |
            EnumBuilder::NewType { codegen_depth, .. } |
            EnumBuilder::ModuleConsts { codegen_depth, .. } |
            EnumBuilder::Consts { codegen_depth, .. } => codegen_depth,
        }
    }

    /// Returns true if the builder is for a rustified enum.
    fn is_rust_enum(&self) -> bool {
        matches!(*self, EnumBuilder::Rust { .. })
    }

    /// Create a new enum given an item builder, a canonical name, a name for
    /// the representation, and which variation it should be generated as.
    fn new(
        name: &'a str,
        mut attrs: Vec<proc_macro2::TokenStream>,
        repr: proc_macro2::TokenStream,
        enum_variation: EnumVariation,
        enum_codegen_depth: usize,
    ) -> Self {
        let ident = Ident::new(name, Span::call_site());

        match enum_variation {
            EnumVariation::NewType { is_bitfield } => EnumBuilder::NewType {
                codegen_depth: enum_codegen_depth,
                canonical_name: name,
                tokens: quote! {
                    #( #attrs )*
                    pub struct #ident (pub #repr);
                },
                is_bitfield,
            },

            EnumVariation::Rust { .. } => {
                // `repr` is guaranteed to be Rustified in Enum::codegen
                attrs.insert(0, quote! { #[repr( #repr )] });
                let tokens = quote!();
                EnumBuilder::Rust {
                    codegen_depth: enum_codegen_depth + 1,
                    attrs,
                    ident,
                    tokens,
                    emitted_any_variants: false,
                }
            }

            EnumVariation::Consts => {
                let mut variants = Vec::new();

                variants.push(quote! {
                    #( #attrs )*
                    pub type #ident = #repr;
                });

                EnumBuilder::Consts {
                    variants,
                    codegen_depth: enum_codegen_depth,
                }
            }

            EnumVariation::ModuleConsts => {
                let ident = Ident::new(
                    CONSTIFIED_ENUM_MODULE_REPR_NAME,
                    Span::call_site(),
                );
                let type_definition = quote! {
                    #( #attrs )*
                    pub type #ident = #repr;
                };

                EnumBuilder::ModuleConsts {
                    codegen_depth: enum_codegen_depth + 1,
                    module_name: name,
                    module_items: vec![type_definition],
                }
            }
        }
    }

    /// Add a variant to this enum.
    fn with_variant<'b>(
        self,
        ctx: &BindgenContext,
        variant: &EnumVariant,
        mangling_prefix: Option<&str>,
        rust_ty: proc_macro2::TokenStream,
        result: &mut CodegenResult<'b>,
        is_ty_named: bool,
    ) -> Self {
        let variant_name = ctx.rust_mangle(variant.name());
        let is_rust_enum = self.is_rust_enum();
        let expr = match variant.val() {
            EnumVariantValue::Boolean(v) if is_rust_enum => {
                helpers::ast_ty::uint_expr(v as u64)
            }
            EnumVariantValue::Boolean(v) => quote!(#v),
            EnumVariantValue::Signed(v) => helpers::ast_ty::int_expr(v),
            EnumVariantValue::Unsigned(v) => helpers::ast_ty::uint_expr(v),
        };

        let mut doc = quote! {};
        if ctx.options().generate_comments {
            if let Some(raw_comment) = variant.comment() {
                let comment =
                    comment::preprocess(raw_comment, self.codegen_depth());
                doc = attributes::doc(comment);
            }
        }

        match self {
            EnumBuilder::Rust {
                attrs,
                ident,
                tokens,
                emitted_any_variants: _,
                codegen_depth,
            } => {
                let name = ctx.rust_ident(variant_name);
                EnumBuilder::Rust {
                    attrs,
                    ident,
                    codegen_depth,
                    tokens: quote! {
                        #tokens
                        #doc
                        #name = #expr,
                    },
                    emitted_any_variants: true,
                }
            }

            EnumBuilder::NewType { canonical_name, .. } => {
                if ctx.options().rust_features().associated_const && is_ty_named
                {
                    let enum_ident = ctx.rust_ident(canonical_name);
                    let variant_ident = ctx.rust_ident(variant_name);
                    result.push(quote! {
                        impl #enum_ident {
                            #doc
                            pub const #variant_ident : #rust_ty = #rust_ty ( #expr );
                        }
                    });
                } else {
                    let ident = ctx.rust_ident(match mangling_prefix {
                        Some(prefix) => {
                            Cow::Owned(format!("{}_{}", prefix, variant_name))
                        }
                        None => variant_name,
                    });
                    result.push(quote! {
                        #doc
                        pub const #ident : #rust_ty = #rust_ty ( #expr );
                    });
                }

                self
            }

            EnumBuilder::Consts { .. } => {
                let constant_name = match mangling_prefix {
                    Some(prefix) => {
                        Cow::Owned(format!("{}_{}", prefix, variant_name))
                    }
                    None => variant_name,
                };

                let ident = ctx.rust_ident(constant_name);
                result.push(quote! {
                    #doc
                    pub const #ident : #rust_ty = #expr ;
                });

                self
            }
            EnumBuilder::ModuleConsts {
                codegen_depth,
                module_name,
                mut module_items,
            } => {
                let name = ctx.rust_ident(variant_name);
                let ty = ctx.rust_ident(CONSTIFIED_ENUM_MODULE_REPR_NAME);
                module_items.push(quote! {
                    #doc
                    pub const #name : #ty = #expr ;
                });

                EnumBuilder::ModuleConsts {
                    module_name,
                    module_items,
                    codegen_depth,
                }
            }
        }
    }

    fn build<'b>(
        self,
        ctx: &BindgenContext,
        rust_ty: proc_macro2::TokenStream,
        result: &mut CodegenResult<'b>,
    ) -> proc_macro2::TokenStream {
        match self {
            EnumBuilder::Rust {
                attrs,
                ident,
                tokens,
                emitted_any_variants,
                ..
            } => {
                let variants = if !emitted_any_variants {
                    quote!(__bindgen_cannot_repr_c_on_empty_enum = 0)
                } else {
                    tokens
                };

                quote! {
                    #( #attrs )*
                    pub enum #ident {
                        #variants
                    }
                }
            }
            EnumBuilder::NewType {
                canonical_name,
                tokens,
                is_bitfield,
                ..
            } => {
                if !is_bitfield {
                    return tokens;
                }

                let rust_ty_name = ctx.rust_ident_raw(canonical_name);
                let prefix = ctx.trait_prefix();

                result.push(quote! {
                    impl ::#prefix::ops::BitOr<#rust_ty> for #rust_ty {
                        type Output = Self;

                        #[inline]
                        fn bitor(self, other: Self) -> Self {
                            #rust_ty_name(self.0 | other.0)
                        }
                    }
                });

                result.push(quote! {
                    impl ::#prefix::ops::BitOrAssign for #rust_ty {
                        #[inline]
                        fn bitor_assign(&mut self, rhs: #rust_ty) {
                            self.0 |= rhs.0;
                        }
                    }
                });

                result.push(quote! {
                    impl ::#prefix::ops::BitAnd<#rust_ty> for #rust_ty {
                        type Output = Self;

                        #[inline]
                        fn bitand(self, other: Self) -> Self {
                            #rust_ty_name(self.0 & other.0)
                        }
                    }
                });

                result.push(quote! {
                    impl ::#prefix::ops::BitAndAssign for #rust_ty {
                        #[inline]
                        fn bitand_assign(&mut self, rhs: #rust_ty) {
                            self.0 &= rhs.0;
                        }
                    }
                });

                tokens
            }
            EnumBuilder::Consts { variants, .. } => quote! { #( #variants )* },
            EnumBuilder::ModuleConsts {
                module_items,
                module_name,
                ..
            } => {
                let ident = ctx.rust_ident(module_name);
                quote! {
                    pub mod #ident {
                        #( #module_items )*
                    }
                }
            }
        }
    }
}

impl CodeGenerator for Enum {
    type Extra = Item;
    type Return = ();

    fn codegen<'a>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'a>,
        item: &Item,
    ) {
        debug!("<Enum as CodeGenerator>::codegen: item = {:?}", item);
        debug_assert!(item.is_enabled_for_codegen(ctx));

        let name = item.canonical_name(ctx);
        let ident = ctx.rust_ident(&name);
        let enum_ty = item.expect_type();
        let layout = enum_ty.layout(ctx);
        let variation = self.computed_enum_variation(ctx, item);

        let repr_translated;
        let repr = match self.repr().map(|repr| ctx.resolve_type(repr)) {
            Some(repr)
                if !ctx.options().translate_enum_integer_types &&
                    !variation.is_rust() =>
            {
                repr
            }
            repr => {
                // An enum's integer type is translated to a native Rust
                // integer type in 3 cases:
                // * the enum is Rustified and we need a translated type for
                //   the repr attribute
                // * the representation couldn't be determined from the C source
                // * it was explicitly requested as a bindgen option

                let kind = match repr {
                    Some(repr) => match *repr.canonical_type(ctx).kind() {
                        TypeKind::Int(int_kind) => int_kind,
                        _ => panic!("Unexpected type as enum repr"),
                    },
                    None => {
                        warn!(
                            "Guessing type of enum! Forward declarations of enums \
                             shouldn't be legal!"
                        );
                        IntKind::Int
                    }
                };

                let signed = kind.is_signed();
                let size = layout
                    .map(|l| l.size)
                    .or_else(|| kind.known_size())
                    .unwrap_or(0);

                let translated = match (signed, size) {
                    (true, 1) => IntKind::I8,
                    (false, 1) => IntKind::U8,
                    (true, 2) => IntKind::I16,
                    (false, 2) => IntKind::U16,
                    (true, 4) => IntKind::I32,
                    (false, 4) => IntKind::U32,
                    (true, 8) => IntKind::I64,
                    (false, 8) => IntKind::U64,
                    _ => {
                        warn!(
                            "invalid enum decl: signed: {}, size: {}",
                            signed, size
                        );
                        IntKind::I32
                    }
                };

                repr_translated =
                    Type::new(None, None, TypeKind::Int(translated), false);
                &repr_translated
            }
        };

        let mut attrs = vec![];

        // TODO(emilio): Delegate this to the builders?
        match variation {
            EnumVariation::Rust { non_exhaustive } => {
                if non_exhaustive &&
                    ctx.options().rust_features().non_exhaustive
                {
                    attrs.push(attributes::non_exhaustive());
                } else if non_exhaustive &&
                    !ctx.options().rust_features().non_exhaustive
                {
                    panic!("The rust target you're using doesn't seem to support non_exhaustive enums");
                }
            }
            EnumVariation::NewType { .. } => {
                if ctx.options().rust_features.repr_transparent {
                    attrs.push(attributes::repr("transparent"));
                } else {
                    attrs.push(attributes::repr("C"));
                }
            }
            _ => {}
        };

        if let Some(comment) = item.comment(ctx) {
            attrs.push(attributes::doc(comment));
        }

        if item.must_use(ctx) {
            attrs.push(attributes::must_use());
        }

        if !variation.is_const() {
            let packed = false; // Enums can't be packed in Rust.
            let mut derives = derives_of_item(item, ctx, packed);
            // For backwards compat, enums always derive
            // Clone/Eq/PartialEq/Hash, even if we don't generate those by
            // default.
            derives.insert(
                DerivableTraits::CLONE |
                    DerivableTraits::HASH |
                    DerivableTraits::PARTIAL_EQ |
                    DerivableTraits::EQ,
            );
            let mut derives: Vec<_> = derives.into();
            for derive in item.annotations().derives().iter() {
                if !derives.contains(&derive.as_str()) {
                    derives.push(derive);
                }
            }

            // The custom derives callback may return a list of derive attributes;
            // add them to the end of the list.
            let custom_derives;
            if let Some(cb) = &ctx.options().parse_callbacks {
                custom_derives = cb.add_derives(&name);
                // In most cases this will be a no-op, since custom_derives will be empty.
                derives.extend(custom_derives.iter().map(|s| s.as_str()));
            };

            attrs.push(attributes::derives(&derives));
        }

        fn add_constant<'a>(
            ctx: &BindgenContext,
            enum_: &Type,
            // Only to avoid recomputing every time.
            enum_canonical_name: &Ident,
            // May be the same as "variant" if it's because the
            // enum is unnamed and we still haven't seen the
            // value.
            variant_name: &Ident,
            referenced_name: &Ident,
            enum_rust_ty: proc_macro2::TokenStream,
            result: &mut CodegenResult<'a>,
        ) {
            let constant_name = if enum_.name().is_some() {
                if ctx.options().prepend_enum_name {
                    format!("{}_{}", enum_canonical_name, variant_name)
                } else {
                    format!("{}", variant_name)
                }
            } else {
                format!("{}", variant_name)
            };
            let constant_name = ctx.rust_ident(constant_name);

            result.push(quote! {
                pub const #constant_name : #enum_rust_ty =
                    #enum_canonical_name :: #referenced_name ;
            });
        }

        let repr = repr.to_rust_ty_or_opaque(ctx, item);

        let mut builder = EnumBuilder::new(
            &name,
            attrs,
            repr,
            variation,
            item.codegen_depth(ctx),
        );

        // A map where we keep a value -> variant relation.
        let mut seen_values = HashMap::<_, Ident>::default();
        let enum_rust_ty = item.to_rust_ty_or_opaque(ctx, &());
        let is_toplevel = item.is_toplevel(ctx);

        // Used to mangle the constants we generate in the unnamed-enum case.
        let parent_canonical_name = if is_toplevel {
            None
        } else {
            Some(item.parent_id().canonical_name(ctx))
        };

        let constant_mangling_prefix = if ctx.options().prepend_enum_name {
            if enum_ty.name().is_none() {
                parent_canonical_name.as_deref()
            } else {
                Some(&*name)
            }
        } else {
            None
        };

        // NB: We defer the creation of constified variants, in case we find
        // another variant with the same value (which is the common thing to
        // do).
        let mut constified_variants = VecDeque::new();

        let mut iter = self.variants().iter().peekable();
        while let Some(variant) =
            iter.next().or_else(|| constified_variants.pop_front())
        {
            if variant.hidden() {
                continue;
            }

            if variant.force_constification() && iter.peek().is_some() {
                constified_variants.push_back(variant);
                continue;
            }

            match seen_values.entry(variant.val()) {
                Entry::Occupied(ref entry) => {
                    if variation.is_rust() {
                        let variant_name = ctx.rust_mangle(variant.name());
                        let mangled_name =
                            if is_toplevel || enum_ty.name().is_some() {
                                variant_name
                            } else {
                                let parent_name =
                                    parent_canonical_name.as_ref().unwrap();

                                Cow::Owned(format!(
                                    "{}_{}",
                                    parent_name, variant_name
                                ))
                            };

                        let existing_variant_name = entry.get();
                        // Use associated constants for named enums.
                        if enum_ty.name().is_some() &&
                            ctx.options().rust_features().associated_const
                        {
                            let enum_canonical_name = &ident;
                            let variant_name =
                                ctx.rust_ident_raw(&*mangled_name);
                            result.push(quote! {
                                impl #enum_rust_ty {
                                    pub const #variant_name : #enum_rust_ty =
                                        #enum_canonical_name :: #existing_variant_name ;
                                }
                            });
                        } else {
                            add_constant(
                                ctx,
                                enum_ty,
                                &ident,
                                &Ident::new(&*mangled_name, Span::call_site()),
                                existing_variant_name,
                                enum_rust_ty.clone(),
                                result,
                            );
                        }
                    } else {
                        builder = builder.with_variant(
                            ctx,
                            variant,
                            constant_mangling_prefix,
                            enum_rust_ty.clone(),
                            result,
                            enum_ty.name().is_some(),
                        );
                    }
                }
                Entry::Vacant(entry) => {
                    builder = builder.with_variant(
                        ctx,
                        variant,
                        constant_mangling_prefix,
                        enum_rust_ty.clone(),
                        result,
                        enum_ty.name().is_some(),
                    );

                    let variant_name = ctx.rust_ident(variant.name());

                    // If it's an unnamed enum, or constification is enforced,
                    // we also generate a constant so it can be properly
                    // accessed.
                    if (variation.is_rust() && enum_ty.name().is_none()) ||
                        variant.force_constification()
                    {
                        let mangled_name = if is_toplevel {
                            variant_name.clone()
                        } else {
                            let parent_name =
                                parent_canonical_name.as_ref().unwrap();

                            Ident::new(
                                &format!("{}_{}", parent_name, variant_name),
                                Span::call_site(),
                            )
                        };

                        add_constant(
                            ctx,
                            enum_ty,
                            &ident,
                            &mangled_name,
                            &variant_name,
                            enum_rust_ty.clone(),
                            result,
                        );
                    }

                    entry.insert(variant_name);
                }
            }
        }

        let item = builder.build(ctx, enum_rust_ty, result);
        result.push(item);
    }
}

/// Enum for the default type of macro constants.
#[derive(Copy, Clone, PartialEq, Debug)]
pub enum MacroTypeVariation {
    /// Use i32 or i64
    Signed,
    /// Use u32 or u64
    Unsigned,
}

impl MacroTypeVariation {
    /// Convert a `MacroTypeVariation` to its str representation.
    pub fn as_str(&self) -> &str {
        match self {
            MacroTypeVariation::Signed => "signed",
            MacroTypeVariation::Unsigned => "unsigned",
        }
    }
}

impl Default for MacroTypeVariation {
    fn default() -> MacroTypeVariation {
        MacroTypeVariation::Unsigned
    }
}

impl std::str::FromStr for MacroTypeVariation {
    type Err = std::io::Error;

    /// Create a `MacroTypeVariation` from a string.
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "signed" => Ok(MacroTypeVariation::Signed),
            "unsigned" => Ok(MacroTypeVariation::Unsigned),
            _ => Err(std::io::Error::new(
                std::io::ErrorKind::InvalidInput,
                concat!(
                    "Got an invalid MacroTypeVariation. Accepted values ",
                    "are 'signed' and 'unsigned'"
                ),
            )),
        }
    }
}

/// Enum for how aliases should be translated.
#[derive(Copy, Clone, PartialEq, Debug)]
pub enum AliasVariation {
    /// Convert to regular Rust alias
    TypeAlias,
    /// Create a new type by wrapping the old type in a struct and using #[repr(transparent)]
    NewType,
    /// Same as NewStruct but also impl Deref to be able to use the methods of the wrapped type
    NewTypeDeref,
}

impl AliasVariation {
    /// Convert an `AliasVariation` to its str representation.
    pub fn as_str(&self) -> &str {
        match self {
            AliasVariation::TypeAlias => "type_alias",
            AliasVariation::NewType => "new_type",
            AliasVariation::NewTypeDeref => "new_type_deref",
        }
    }
}

impl Default for AliasVariation {
    fn default() -> AliasVariation {
        AliasVariation::TypeAlias
    }
}

impl std::str::FromStr for AliasVariation {
    type Err = std::io::Error;

    /// Create an `AliasVariation` from a string.
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "type_alias" => Ok(AliasVariation::TypeAlias),
            "new_type" => Ok(AliasVariation::NewType),
            "new_type_deref" => Ok(AliasVariation::NewTypeDeref),
            _ => Err(std::io::Error::new(
                std::io::ErrorKind::InvalidInput,
                concat!(
                    "Got an invalid AliasVariation. Accepted values ",
                    "are 'type_alias', 'new_type', and 'new_type_deref'"
                ),
            )),
        }
    }
}

/// Fallible conversion to an opaque blob.
///
/// Implementors of this trait should provide the `try_get_layout` method to
/// fallibly get this thing's layout, which the provided `try_to_opaque` trait
/// method will use to convert the `Layout` into an opaque blob Rust type.
trait TryToOpaque {
    type Extra;

    /// Get the layout for this thing, if one is available.
    fn try_get_layout(
        &self,
        ctx: &BindgenContext,
        extra: &Self::Extra,
    ) -> error::Result<Layout>;

    /// Do not override this provided trait method.
    fn try_to_opaque(
        &self,
        ctx: &BindgenContext,
        extra: &Self::Extra,
    ) -> error::Result<proc_macro2::TokenStream> {
        self.try_get_layout(ctx, extra)
            .map(|layout| helpers::blob(ctx, layout))
    }
}

/// Infallible conversion of an IR thing to an opaque blob.
///
/// The resulting layout is best effort, and is unfortunately not guaranteed to
/// be correct. When all else fails, we fall back to a single byte layout as a
/// last resort, because C++ does not permit zero-sized types. See the note in
/// the `ToRustTyOrOpaque` doc comment about fallible versus infallible traits
/// and when each is appropriate.
///
/// Don't implement this directly. Instead implement `TryToOpaque`, and then
/// leverage the blanket impl for this trait.
trait ToOpaque: TryToOpaque {
    fn get_layout(&self, ctx: &BindgenContext, extra: &Self::Extra) -> Layout {
        self.try_get_layout(ctx, extra)
            .unwrap_or_else(|_| Layout::for_size(ctx, 1))
    }

    fn to_opaque(
        &self,
        ctx: &BindgenContext,
        extra: &Self::Extra,
    ) -> proc_macro2::TokenStream {
        let layout = self.get_layout(ctx, extra);
        helpers::blob(ctx, layout)
    }
}

impl<T> ToOpaque for T where T: TryToOpaque {}

/// Fallible conversion from an IR thing to an *equivalent* Rust type.
///
/// If the C/C++ construct represented by the IR thing cannot (currently) be
/// represented in Rust (for example, instantiations of templates with
/// const-value generic parameters) then the impl should return an `Err`. It
/// should *not* attempt to return an opaque blob with the correct size and
/// alignment. That is the responsibility of the `TryToOpaque` trait.
trait TryToRustTy {
    type Extra;

    fn try_to_rust_ty(
        &self,
        ctx: &BindgenContext,
        extra: &Self::Extra,
    ) -> error::Result<proc_macro2::TokenStream>;
}

/// Fallible conversion to a Rust type or an opaque blob with the correct size
/// and alignment.
///
/// Don't implement this directly. Instead implement `TryToRustTy` and
/// `TryToOpaque`, and then leverage the blanket impl for this trait below.
trait TryToRustTyOrOpaque: TryToRustTy + TryToOpaque {
    type Extra;

    fn try_to_rust_ty_or_opaque(
        &self,
        ctx: &BindgenContext,
        extra: &<Self as TryToRustTyOrOpaque>::Extra,
    ) -> error::Result<proc_macro2::TokenStream>;
}

impl<E, T> TryToRustTyOrOpaque for T
where
    T: TryToRustTy<Extra = E> + TryToOpaque<Extra = E>,
{
    type Extra = E;

    fn try_to_rust_ty_or_opaque(
        &self,
        ctx: &BindgenContext,
        extra: &E,
    ) -> error::Result<proc_macro2::TokenStream> {
        self.try_to_rust_ty(ctx, extra).or_else(|_| {
            if let Ok(layout) = self.try_get_layout(ctx, extra) {
                Ok(helpers::blob(ctx, layout))
            } else {
                Err(error::Error::NoLayoutForOpaqueBlob)
            }
        })
    }
}

/// Infallible conversion to a Rust type, or an opaque blob with a best effort
/// of correct size and alignment.
///
/// Don't implement this directly. Instead implement `TryToRustTy` and
/// `TryToOpaque`, and then leverage the blanket impl for this trait below.
///
/// ### Fallible vs. Infallible Conversions to Rust Types
///
/// When should one use this infallible `ToRustTyOrOpaque` trait versus the
/// fallible `TryTo{RustTy, Opaque, RustTyOrOpaque}` triats? All fallible trait
/// implementations that need to convert another thing into a Rust type or
/// opaque blob in a nested manner should also use fallible trait methods and
/// propagate failure up the stack. Only infallible functions and methods like
/// CodeGenerator implementations should use the infallible
/// `ToRustTyOrOpaque`. The further out we push error recovery, the more likely
/// we are to get a usable `Layout` even if we can't generate an equivalent Rust
/// type for a C++ construct.
trait ToRustTyOrOpaque: TryToRustTy + ToOpaque {
    type Extra;

    fn to_rust_ty_or_opaque(
        &self,
        ctx: &BindgenContext,
        extra: &<Self as ToRustTyOrOpaque>::Extra,
    ) -> proc_macro2::TokenStream;
}

impl<E, T> ToRustTyOrOpaque for T
where
    T: TryToRustTy<Extra = E> + ToOpaque<Extra = E>,
{
    type Extra = E;

    fn to_rust_ty_or_opaque(
        &self,
        ctx: &BindgenContext,
        extra: &E,
    ) -> proc_macro2::TokenStream {
        self.try_to_rust_ty(ctx, extra)
            .unwrap_or_else(|_| self.to_opaque(ctx, extra))
    }
}

impl<T> TryToOpaque for T
where
    T: Copy + Into<ItemId>,
{
    type Extra = ();

    fn try_get_layout(
        &self,
        ctx: &BindgenContext,
        _: &(),
    ) -> error::Result<Layout> {
        ctx.resolve_item((*self).into()).try_get_layout(ctx, &())
    }
}

impl<T> TryToRustTy for T
where
    T: Copy + Into<ItemId>,
{
    type Extra = ();

    fn try_to_rust_ty(
        &self,
        ctx: &BindgenContext,
        _: &(),
    ) -> error::Result<proc_macro2::TokenStream> {
        ctx.resolve_item((*self).into()).try_to_rust_ty(ctx, &())
    }
}

impl TryToOpaque for Item {
    type Extra = ();

    fn try_get_layout(
        &self,
        ctx: &BindgenContext,
        _: &(),
    ) -> error::Result<Layout> {
        self.kind().expect_type().try_get_layout(ctx, self)
    }
}

impl TryToRustTy for Item {
    type Extra = ();

    fn try_to_rust_ty(
        &self,
        ctx: &BindgenContext,
        _: &(),
    ) -> error::Result<proc_macro2::TokenStream> {
        self.kind().expect_type().try_to_rust_ty(ctx, self)
    }
}

impl TryToOpaque for Type {
    type Extra = Item;

    fn try_get_layout(
        &self,
        ctx: &BindgenContext,
        _: &Item,
    ) -> error::Result<Layout> {
        self.layout(ctx).ok_or(error::Error::NoLayoutForOpaqueBlob)
    }
}

impl TryToRustTy for Type {
    type Extra = Item;

    fn try_to_rust_ty(
        &self,
        ctx: &BindgenContext,
        item: &Item,
    ) -> error::Result<proc_macro2::TokenStream> {
        use self::helpers::ast_ty::*;

        match *self.kind() {
            TypeKind::Void => Ok(c_void(ctx)),
            // TODO: we should do something smart with nullptr, or maybe *const
            // c_void is enough?
            TypeKind::NullPtr => Ok(c_void(ctx).to_ptr(true)),
            TypeKind::Int(ik) => {
                match ik {
                    IntKind::Bool => Ok(quote! { bool }),
                    IntKind::Char { .. } => Ok(raw_type(ctx, "c_char")),
                    IntKind::SChar => Ok(raw_type(ctx, "c_schar")),
                    IntKind::UChar => Ok(raw_type(ctx, "c_uchar")),
                    IntKind::Short => Ok(raw_type(ctx, "c_short")),
                    IntKind::UShort => Ok(raw_type(ctx, "c_ushort")),
                    IntKind::Int => Ok(raw_type(ctx, "c_int")),
                    IntKind::UInt => Ok(raw_type(ctx, "c_uint")),
                    IntKind::Long => Ok(raw_type(ctx, "c_long")),
                    IntKind::ULong => Ok(raw_type(ctx, "c_ulong")),
                    IntKind::LongLong => Ok(raw_type(ctx, "c_longlong")),
                    IntKind::ULongLong => Ok(raw_type(ctx, "c_ulonglong")),
                    IntKind::WChar => {
                        let layout = self
                            .layout(ctx)
                            .expect("Couldn't compute wchar_t's layout?");
                        let ty = Layout::known_type_for_size(ctx, layout.size)
                            .expect("Non-representable wchar_t?");
                        let ident = ctx.rust_ident_raw(ty);
                        Ok(quote! { #ident })
                    }

                    IntKind::I8 => Ok(quote! { i8 }),
                    IntKind::U8 => Ok(quote! { u8 }),
                    IntKind::I16 => Ok(quote! { i16 }),
                    IntKind::U16 => Ok(quote! { u16 }),
                    IntKind::I32 => Ok(quote! { i32 }),
                    IntKind::U32 => Ok(quote! { u32 }),
                    IntKind::I64 => Ok(quote! { i64 }),
                    IntKind::U64 => Ok(quote! { u64 }),
                    IntKind::Custom { name, .. } => {
                        Ok(proc_macro2::TokenStream::from_str(name).unwrap())
                    }
                    IntKind::U128 => {
                        Ok(if ctx.options().rust_features.i128_and_u128 {
                            quote! { u128 }
                        } else {
                            // Best effort thing, but wrong alignment
                            // unfortunately.
                            quote! { [u64; 2] }
                        })
                    }
                    IntKind::I128 => {
                        Ok(if ctx.options().rust_features.i128_and_u128 {
                            quote! { i128 }
                        } else {
                            quote! { [u64; 2] }
                        })
                    }
                }
            }
            TypeKind::Float(fk) => {
                Ok(float_kind_rust_type(ctx, fk, self.layout(ctx)))
            }
            TypeKind::Complex(fk) => {
                let float_path =
                    float_kind_rust_type(ctx, fk, self.layout(ctx));

                ctx.generated_bindgen_complex();
                Ok(if ctx.options().enable_cxx_namespaces {
                    quote! {
                        root::__BindgenComplex<#float_path>
                    }
                } else {
                    quote! {
                        __BindgenComplex<#float_path>
                    }
                })
            }
            TypeKind::Function(ref fs) => {
                // We can't rely on the sizeof(Option<NonZero<_>>) ==
                // sizeof(NonZero<_>) optimization with opaque blobs (because
                // they aren't NonZero), so don't *ever* use an or_opaque
                // variant here.
                let ty = fs.try_to_rust_ty(ctx, &())?;

                let prefix = ctx.trait_prefix();
                Ok(quote! {
                    ::#prefix::option::Option<#ty>
                })
            }
            TypeKind::Array(item, len) | TypeKind::Vector(item, len) => {
                let ty = item.try_to_rust_ty(ctx, &())?;
                Ok(quote! {
                    [ #ty ; #len ]
                })
            }
            TypeKind::Enum(..) => {
                let path = item.namespace_aware_canonical_path(ctx);
                let path = proc_macro2::TokenStream::from_str(&path.join("::"))
                    .unwrap();
                Ok(quote!(#path))
            }
            TypeKind::TemplateInstantiation(ref inst) => {
                inst.try_to_rust_ty(ctx, item)
            }
            TypeKind::ResolvedTypeRef(inner) => inner.try_to_rust_ty(ctx, &()),
            TypeKind::TemplateAlias(..) |
            TypeKind::Alias(..) |
            TypeKind::BlockPointer(..) => {
                if self.is_block_pointer() && !ctx.options().generate_block {
                    let void = c_void(ctx);
                    return Ok(void.to_ptr(/* is_const = */ false));
                }

                if item.is_opaque(ctx, &()) &&
                    item.used_template_params(ctx)
                        .into_iter()
                        .any(|param| param.is_template_param(ctx, &()))
                {
                    self.try_to_opaque(ctx, item)
                } else if let Some(ty) = self
                    .name()
                    .and_then(|name| utils::type_from_named(ctx, name))
                {
                    Ok(ty)
                } else {
                    utils::build_path(item, ctx)
                }
            }
            TypeKind::Comp(ref info) => {
                let template_params = item.all_template_params(ctx);
                if info.has_non_type_template_params() ||
                    (item.is_opaque(ctx, &()) && !template_params.is_empty())
                {
                    return self.try_to_opaque(ctx, item);
                }

                utils::build_path(item, ctx)
            }
            TypeKind::Opaque => self.try_to_opaque(ctx, item),
            TypeKind::Pointer(inner) | TypeKind::Reference(inner) => {
                let is_const = ctx.resolve_type(inner).is_const();

                let inner =
                    inner.into_resolver().through_type_refs().resolve(ctx);
                let inner_ty = inner.expect_type();

                let is_objc_pointer =
                    matches!(inner_ty.kind(), TypeKind::ObjCInterface(..));

                // Regardless if we can properly represent the inner type, we
                // should always generate a proper pointer here, so use
                // infallible conversion of the inner type.
                let mut ty = inner.to_rust_ty_or_opaque(ctx, &());
                ty.append_implicit_template_params(ctx, inner);

                // Avoid the first function pointer level, since it's already
                // represented in Rust.
                if inner_ty.canonical_type(ctx).is_function() || is_objc_pointer
                {
                    Ok(ty)
                } else {
                    Ok(ty.to_ptr(is_const))
                }
            }
            TypeKind::TypeParam => {
                let name = item.canonical_name(ctx);
                let ident = ctx.rust_ident(&name);
                Ok(quote! {
                    #ident
                })
            }
            TypeKind::ObjCSel => Ok(quote! {
                objc::runtime::Sel
            }),
            TypeKind::ObjCId => Ok(quote! {
                id
            }),
            TypeKind::ObjCInterface(ref interface) => {
                let name = ctx.rust_ident(interface.name());
                Ok(quote! {
                    #name
                })
            }
            ref u @ TypeKind::UnresolvedTypeRef(..) => {
                unreachable!("Should have been resolved after parsing {:?}!", u)
            }
        }
    }
}

impl TryToOpaque for TemplateInstantiation {
    type Extra = Item;

    fn try_get_layout(
        &self,
        ctx: &BindgenContext,
        item: &Item,
    ) -> error::Result<Layout> {
        item.expect_type()
            .layout(ctx)
            .ok_or(error::Error::NoLayoutForOpaqueBlob)
    }
}

impl TryToRustTy for TemplateInstantiation {
    type Extra = Item;

    fn try_to_rust_ty(
        &self,
        ctx: &BindgenContext,
        item: &Item,
    ) -> error::Result<proc_macro2::TokenStream> {
        if self.is_opaque(ctx, item) {
            return Err(error::Error::InstantiationOfOpaqueType);
        }

        let def = self
            .template_definition()
            .into_resolver()
            .through_type_refs()
            .resolve(ctx);

        let mut ty = quote! {};
        let def_path = def.namespace_aware_canonical_path(ctx);
        ty.append_separated(
            def_path.into_iter().map(|p| ctx.rust_ident(p)),
            quote!(::),
        );

        let def_params = def.self_template_params(ctx);
        if def_params.is_empty() {
            // This can happen if we generated an opaque type for a partial
            // template specialization, and we've hit an instantiation of
            // that partial specialization.
            extra_assert!(def.is_opaque(ctx, &()));
            return Err(error::Error::InstantiationOfOpaqueType);
        }

        // TODO: If the definition type is a template class/struct
        // definition's member template definition, it could rely on
        // generic template parameters from its outer template
        // class/struct. When we emit bindings for it, it could require
        // *more* type arguments than we have here, and we will need to
        // reconstruct them somehow. We don't have any means of doing
        // that reconstruction at this time.

        let template_args = self
            .template_arguments()
            .iter()
            .zip(def_params.iter())
            // Only pass type arguments for the type parameters that
            // the def uses.
            .filter(|&(_, param)| ctx.uses_template_parameter(def.id(), *param))
            .map(|(arg, _)| {
                let arg = arg.into_resolver().through_type_refs().resolve(ctx);
                let mut ty = arg.try_to_rust_ty(ctx, &())?;
                ty.append_implicit_template_params(ctx, arg);
                Ok(ty)
            })
            .collect::<error::Result<Vec<_>>>()?;

        if template_args.is_empty() {
            return Ok(ty);
        }

        Ok(quote! {
            #ty < #( #template_args ),* >
        })
    }
}

impl TryToRustTy for FunctionSig {
    type Extra = ();

    fn try_to_rust_ty(
        &self,
        ctx: &BindgenContext,
        _: &(),
    ) -> error::Result<proc_macro2::TokenStream> {
        // TODO: we might want to consider ignoring the reference return value.
        let ret = utils::fnsig_return_ty(ctx, self);
        let arguments = utils::fnsig_arguments(ctx, self);
        let abi = self.abi();

        match abi {
            Abi::ThisCall if !ctx.options().rust_features().thiscall_abi => {
                warn!("Skipping function with thiscall ABI that isn't supported by the configured Rust target");
                Ok(proc_macro2::TokenStream::new())
            }
            Abi::Vectorcall
                if !ctx.options().rust_features().vectorcall_abi =>
            {
                warn!("Skipping function with vectorcall ABI that isn't supported by the configured Rust target");
                Ok(proc_macro2::TokenStream::new())
            }
            _ => Ok(quote! {
                unsafe extern #abi fn ( #( #arguments ),* ) #ret
            }),
        }
    }
}

impl CodeGenerator for Function {
    type Extra = Item;

    /// If we've actually generated the symbol, the number of times we've seen
    /// it.
    type Return = Option<u32>;

    fn codegen<'a>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'a>,
        item: &Item,
    ) -> Self::Return {
        debug!("<Function as CodeGenerator>::codegen: item = {:?}", item);
        debug_assert!(item.is_enabled_for_codegen(ctx));

        // We can't currently do anything with Internal functions so just
        // avoid generating anything for them.
        match self.linkage() {
            Linkage::Internal => return None,
            Linkage::External => {}
        }

        // Pure virtual methods have no actual symbol, so we can't generate
        // something meaningful for them.
        let is_dynamic_function = match self.kind() {
            FunctionKind::Method(ref method_kind)
                if method_kind.is_pure_virtual() =>
            {
                return None;
            }
            FunctionKind::Function => {
                ctx.options().dynamic_library_name.is_some()
            }
            _ => false,
        };

        // Similar to static member variables in a class template, we can't
        // generate bindings to template functions, because the set of
        // instantiations is open ended and we have no way of knowing which
        // monomorphizations actually exist.
        if !item.all_template_params(ctx).is_empty() {
            return None;
        }

        let name = self.name();
        let mut canonical_name = item.canonical_name(ctx);
        let mangled_name = self.mangled_name();

        {
            let seen_symbol_name = mangled_name.unwrap_or(&canonical_name);

            // TODO: Maybe warn here if there's a type/argument mismatch, or
            // something?
            if result.seen_function(seen_symbol_name) {
                return None;
            }
            result.saw_function(seen_symbol_name);
        }

        let signature_item = ctx.resolve_item(self.signature());
        let signature = signature_item.kind().expect_type().canonical_type(ctx);
        let signature = match *signature.kind() {
            TypeKind::Function(ref sig) => sig,
            _ => panic!("Signature kind is not a Function: {:?}", signature),
        };

        let args = utils::fnsig_arguments(ctx, signature);
        let ret = utils::fnsig_return_ty(ctx, signature);

        let mut attributes = vec![];

        if ctx.options().rust_features().must_use_function {
            let must_use = signature.must_use() || {
                let ret_ty = signature
                    .return_type()
                    .into_resolver()
                    .through_type_refs()
                    .resolve(ctx);
                ret_ty.must_use(ctx)
            };

            if must_use {
                attributes.push(attributes::must_use());
            }
        }

        if let Some(comment) = item.comment(ctx) {
            attributes.push(attributes::doc(comment));
        }

        let abi = match signature.abi() {
            Abi::ThisCall if !ctx.options().rust_features().thiscall_abi => {
                warn!("Skipping function with thiscall ABI that isn't supported by the configured Rust target");
                return None;
            }
            Abi::Vectorcall
                if !ctx.options().rust_features().vectorcall_abi =>
            {
                warn!("Skipping function with vectorcall ABI that isn't supported by the configured Rust target");
                return None;
            }
            Abi::Win64 if signature.is_variadic() => {
                warn!("Skipping variadic function with Win64 ABI that isn't supported");
                return None;
            }
            Abi::Unknown(unknown_abi) => {
                panic!(
                    "Invalid or unknown abi {:?} for function {:?} ({:?})",
                    unknown_abi, canonical_name, self
                );
            }
            abi => abi,
        };

        // Handle overloaded functions by giving each overload its own unique
        // suffix.
        let times_seen = result.overload_number(&canonical_name);
        if times_seen > 0 {
            write!(&mut canonical_name, "{}", times_seen).unwrap();
        }

        let link_name = mangled_name.unwrap_or(name);
        if !is_dynamic_function &&
            !utils::names_will_be_identical_after_mangling(
                &canonical_name,
                link_name,
                Some(abi),
            )
        {
            attributes.push(attributes::link_name(link_name));
        }

        // Unfortunately this can't piggyback on the `attributes` list because
        // the #[link(wasm_import_module)] needs to happen before the `extern
        // "C"` block. It doesn't get picked up properly otherwise
        let wasm_link_attribute =
            ctx.options().wasm_import_module_name.as_ref().map(|name| {
                quote! { #[link(wasm_import_module = #name)] }
            });

        let ident = ctx.rust_ident(canonical_name);
        let tokens = quote! {
            #wasm_link_attribute
            extern #abi {
                #(#attributes)*
                pub fn #ident ( #( #args ),* ) #ret;
            }
        };

        // If we're doing dynamic binding generation, add to the dynamic items.
        if is_dynamic_function {
            let args_identifiers =
                utils::fnsig_argument_identifiers(ctx, signature);
            let return_item = ctx.resolve_item(signature.return_type());
            let ret_ty = match *return_item.kind().expect_type().kind() {
                TypeKind::Void => quote! {()},
                _ => return_item.to_rust_ty_or_opaque(ctx, &()),
            };
            result.dynamic_items().push(
                ident,
                abi,
                signature.is_variadic(),
                ctx.options().dynamic_link_require_all,
                args,
                args_identifiers,
                ret,
                ret_ty,
                attributes,
            );
        } else {
            result.push(tokens);
        }
        Some(times_seen)
    }
}

fn objc_method_codegen(
    ctx: &BindgenContext,
    method: &ObjCMethod,
    class_name: Option<&str>,
    prefix: &str,
) -> proc_macro2::TokenStream {
    let signature = method.signature();
    let fn_args = utils::fnsig_arguments(ctx, signature);
    let fn_ret = utils::fnsig_return_ty(ctx, signature);

    let sig = if method.is_class_method() {
        let fn_args = fn_args.clone();
        quote! {
            ( #( #fn_args ),* ) #fn_ret
        }
    } else {
        let fn_args = fn_args.clone();
        let args = iter::once(quote! { &self }).chain(fn_args.into_iter());
        quote! {
            ( #( #args ),* ) #fn_ret
        }
    };

    let methods_and_args = method.format_method_call(&fn_args);

    let body = if method.is_class_method() {
        let class_name = ctx.rust_ident(
            class_name.expect("Generating a class method without class name?"),
        );
        quote! {
            msg_send!(class!(#class_name), #methods_and_args)
        }
    } else {
        quote! {
            msg_send!(*self, #methods_and_args)
        }
    };

    let method_name =
        ctx.rust_ident(format!("{}{}", prefix, method.rust_name()));

    quote! {
        unsafe fn #method_name #sig where <Self as std::ops::Deref>::Target: objc::Message + Sized {
            #body
        }
    }
}

impl CodeGenerator for ObjCInterface {
    type Extra = Item;
    type Return = ();

    fn codegen<'a>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'a>,
        item: &Item,
    ) {
        debug_assert!(item.is_enabled_for_codegen(ctx));

        let mut impl_items = vec![];

        for method in self.methods() {
            let impl_item = objc_method_codegen(ctx, method, None, "");
            impl_items.push(impl_item);
        }

        for class_method in self.class_methods() {
            let ambiquity = self
                .methods()
                .iter()
                .map(|m| m.rust_name())
                .any(|x| x == class_method.rust_name());
            let prefix = if ambiquity { "class_" } else { "" };
            let impl_item = objc_method_codegen(
                ctx,
                class_method,
                Some(self.name()),
                prefix,
            );
            impl_items.push(impl_item);
        }

        let trait_name = ctx.rust_ident(self.rust_name());
        let trait_constraints = quote! {
            Sized + std::ops::Deref
        };
        let trait_block = if self.is_template() {
            let template_names: Vec<Ident> = self
                .template_names
                .iter()
                .map(|g| ctx.rust_ident(g))
                .collect();

            quote! {
                pub trait #trait_name <#(#template_names:'static),*> : #trait_constraints {
                    #( #impl_items )*
                }
            }
        } else {
            quote! {
                pub trait #trait_name : #trait_constraints {
                    #( #impl_items )*
                }
            }
        };

        let class_name = ctx.rust_ident(self.name());
        if !self.is_category() && !self.is_protocol() {
            let struct_block = quote! {
                #[repr(transparent)]
                #[derive(Debug, Copy, Clone)]
                pub struct #class_name(pub id);
                impl std::ops::Deref for #class_name {
                    type Target = objc::runtime::Object;
                    fn deref(&self) -> &Self::Target {
                        unsafe {
                            &*self.0
                        }
                    }
                }
                unsafe impl objc::Message for #class_name { }
                impl #class_name {
                    pub fn alloc() -> Self {
                        Self(unsafe {
                            msg_send!(class!(#class_name), alloc)
                        })
                    }
                }
            };
            result.push(struct_block);
            let mut protocol_set: HashSet<ItemId> = Default::default();
            for protocol_id in self.conforms_to.iter() {
                protocol_set.insert(*protocol_id);
                let protocol_name = ctx.rust_ident(
                    ctx.resolve_type(protocol_id.expect_type_id(ctx))
                        .name()
                        .unwrap(),
                );
                let impl_trait = quote! {
                    impl #protocol_name for #class_name { }
                };
                result.push(impl_trait);
            }
            let mut parent_class = self.parent_class;
            while let Some(parent_id) = parent_class {
                let parent = parent_id
                    .expect_type_id(ctx)
                    .into_resolver()
                    .through_type_refs()
                    .resolve(ctx)
                    .expect_type()
                    .kind();

                let parent = match parent {
                    TypeKind::ObjCInterface(ref parent) => parent,
                    _ => break,
                };
                parent_class = parent.parent_class;

                let parent_name = ctx.rust_ident(parent.rust_name());
                let impl_trait = if parent.is_template() {
                    let template_names: Vec<Ident> = parent
                        .template_names
                        .iter()
                        .map(|g| ctx.rust_ident(g))
                        .collect();
                    quote! {
                        impl <#(#template_names :'static),*> #parent_name <#(#template_names),*> for #class_name {
                        }
                    }
                } else {
                    quote! {
                        impl #parent_name for #class_name { }
                    }
                };
                result.push(impl_trait);
                for protocol_id in parent.conforms_to.iter() {
                    if protocol_set.insert(*protocol_id) {
                        let protocol_name = ctx.rust_ident(
                            ctx.resolve_type(protocol_id.expect_type_id(ctx))
                                .name()
                                .unwrap(),
                        );
                        let impl_trait = quote! {
                            impl #protocol_name for #class_name { }
                        };
                        result.push(impl_trait);
                    }
                }
                if !parent.is_template() {
                    let parent_struct_name = parent.name();
                    let child_struct_name = self.name();
                    let parent_struct = ctx.rust_ident(parent_struct_name);
                    let from_block = quote! {
                        impl From<#class_name> for #parent_struct {
                            fn from(child: #class_name) -> #parent_struct {
                                #parent_struct(child.0)
                            }
                        }
                    };
                    result.push(from_block);

                    let error_msg = format!(
                        "This {} cannot be downcasted to {}",
                        parent_struct_name, child_struct_name
                    );
                    let try_into_block = quote! {
                        impl std::convert::TryFrom<#parent_struct> for #class_name {
                            type Error = &'static str;
                            fn try_from(parent: #parent_struct) -> Result<#class_name, Self::Error> {
                                let is_kind_of : bool = unsafe { msg_send!(parent, isKindOfClass:class!(#class_name))};
                                if is_kind_of {
                                    Ok(#class_name(parent.0))
                                } else {
                                    Err(#error_msg)
                                }
                            }
                        }
                    };
                    result.push(try_into_block);
                }
            }
        }

        if !self.is_protocol() {
            let impl_block = if self.is_template() {
                let template_names: Vec<Ident> = self
                    .template_names
                    .iter()
                    .map(|g| ctx.rust_ident(g))
                    .collect();
                quote! {
                    impl <#(#template_names :'static),*> #trait_name <#(#template_names),*> for #class_name {
                    }
                }
            } else {
                quote! {
                    impl #trait_name for #class_name {
                    }
                }
            };
            result.push(impl_block);
        }

        result.push(trait_block);
        result.saw_objc();
    }
}

pub(crate) fn codegen(
    context: BindgenContext,
) -> (Vec<proc_macro2::TokenStream>, BindgenOptions) {
    context.gen(|context| {
        let _t = context.timer("codegen");
        let counter = Cell::new(0);
        let mut result = CodegenResult::new(&counter);

        debug!("codegen: {:?}", context.options());

        if context.options().emit_ir {
            let codegen_items = context.codegen_items();
            for (id, item) in context.items() {
                if codegen_items.contains(&id) {
                    println!("ir: {:?} = {:#?}", id, item);
                }
            }
        }

        if let Some(path) = context.options().emit_ir_graphviz.as_ref() {
            match dot::write_dot_file(context, path) {
                Ok(()) => info!(
                    "Your dot file was generated successfully into: {}",
                    path
                ),
                Err(e) => warn!("{}", e),
            }
        }

        if let Some(spec) = context.options().depfile.as_ref() {
            match spec.write(context.deps()) {
                Ok(()) => info!(
                    "Your depfile was generated successfully into: {}",
                    spec.depfile_path.display()
                ),
                Err(e) => warn!("{}", e),
            }
        }

        context.resolve_item(context.root_module()).codegen(
            context,
            &mut result,
            &(),
        );

        if let Some(ref lib_name) = context.options().dynamic_library_name {
            let lib_ident = context.rust_ident(lib_name);
            let dynamic_items_tokens =
                result.dynamic_items().get_tokens(lib_ident);
            result.push(dynamic_items_tokens);
        }

        result.items
    })
}

pub mod utils {
    use super::{error, ToRustTyOrOpaque};
    use crate::ir::context::BindgenContext;
    use crate::ir::function::{Abi, FunctionSig};
    use crate::ir::item::{Item, ItemCanonicalPath};
    use crate::ir::ty::TypeKind;
    use proc_macro2;
    use std::borrow::Cow;
    use std::mem;
    use std::str::FromStr;

    pub fn prepend_bitfield_unit_type(
        ctx: &BindgenContext,
        result: &mut Vec<proc_macro2::TokenStream>,
    ) {
        let bitfield_unit_src = include_str!("./bitfield_unit.rs");
        let bitfield_unit_src = if ctx.options().rust_features().min_const_fn {
            Cow::Borrowed(bitfield_unit_src)
        } else {
            Cow::Owned(bitfield_unit_src.replace("const fn ", "fn "))
        };
        let bitfield_unit_type =
            proc_macro2::TokenStream::from_str(&bitfield_unit_src).unwrap();
        let bitfield_unit_type = quote!(#bitfield_unit_type);

        let items = vec![bitfield_unit_type];
        let old_items = mem::replace(result, items);
        result.extend(old_items);
    }

    pub fn prepend_objc_header(
        ctx: &BindgenContext,
        result: &mut Vec<proc_macro2::TokenStream>,
    ) {
        let use_objc = if ctx.options().objc_extern_crate {
            quote! {
                #[macro_use]
                extern crate objc;
            }
        } else {
            quote! {
                use objc::{self, msg_send, sel, sel_impl, class};
            }
        };

        let id_type = quote! {
            #[allow(non_camel_case_types)]
            pub type id = *mut objc::runtime::Object;
        };

        let items = vec![use_objc, id_type];
        let old_items = mem::replace(result, items);
        result.extend(old_items.into_iter());
    }

    pub fn prepend_block_header(
        ctx: &BindgenContext,
        result: &mut Vec<proc_macro2::TokenStream>,
    ) {
        let use_block = if ctx.options().block_extern_crate {
            quote! {
                extern crate block;
            }
        } else {
            quote! {
                use block;
            }
        };

        let items = vec![use_block];
        let old_items = mem::replace(result, items);
        result.extend(old_items.into_iter());
    }

    pub fn prepend_union_types(
        ctx: &BindgenContext,
        result: &mut Vec<proc_macro2::TokenStream>,
    ) {
        let prefix = ctx.trait_prefix();

        // If the target supports `const fn`, declare eligible functions
        // as `const fn` else just `fn`.
        let const_fn = if ctx.options().rust_features().min_const_fn {
            quote! { const fn }
        } else {
            quote! { fn }
        };

        // TODO(emilio): The fmt::Debug impl could be way nicer with
        // std::intrinsics::type_name, but...
        let union_field_decl = quote! {
            #[repr(C)]
            pub struct __BindgenUnionField<T>(::#prefix::marker::PhantomData<T>);
        };

        let union_field_impl = quote! {
            impl<T> __BindgenUnionField<T> {
                #[inline]
                pub #const_fn new() -> Self {
                    __BindgenUnionField(::#prefix::marker::PhantomData)
                }

                #[inline]
                pub unsafe fn as_ref(&self) -> &T {
                    ::#prefix::mem::transmute(self)
                }

                #[inline]
                pub unsafe fn as_mut(&mut self) -> &mut T {
                    ::#prefix::mem::transmute(self)
                }
            }
        };

        let union_field_default_impl = quote! {
            impl<T> ::#prefix::default::Default for __BindgenUnionField<T> {
                #[inline]
                fn default() -> Self {
                    Self::new()
                }
            }
        };

        let union_field_clone_impl = quote! {
            impl<T> ::#prefix::clone::Clone for __BindgenUnionField<T> {
                #[inline]
                fn clone(&self) -> Self {
                    Self::new()
                }
            }
        };

        let union_field_copy_impl = quote! {
            impl<T> ::#prefix::marker::Copy for __BindgenUnionField<T> {}
        };

        let union_field_debug_impl = quote! {
            impl<T> ::#prefix::fmt::Debug for __BindgenUnionField<T> {
                fn fmt(&self, fmt: &mut ::#prefix::fmt::Formatter<'_>)
                       -> ::#prefix::fmt::Result {
                    fmt.write_str("__BindgenUnionField")
                }
            }
        };

        // The actual memory of the filed will be hashed, so that's why these
        // field doesn't do anything with the hash.
        let union_field_hash_impl = quote! {
            impl<T> ::#prefix::hash::Hash for __BindgenUnionField<T> {
                fn hash<H: ::#prefix::hash::Hasher>(&self, _state: &mut H) {
                }
            }
        };

        let union_field_partialeq_impl = quote! {
            impl<T> ::#prefix::cmp::PartialEq for __BindgenUnionField<T> {
               fn eq(&self, _other: &__BindgenUnionField<T>) -> bool {
                   true
               }
           }
        };

        let union_field_eq_impl = quote! {
           impl<T> ::#prefix::cmp::Eq for __BindgenUnionField<T> {
           }
        };

        let items = vec![
            union_field_decl,
            union_field_impl,
            union_field_default_impl,
            union_field_clone_impl,
            union_field_copy_impl,
            union_field_debug_impl,
            union_field_hash_impl,
            union_field_partialeq_impl,
            union_field_eq_impl,
        ];

        let old_items = mem::replace(result, items);
        result.extend(old_items.into_iter());
    }

    pub fn prepend_incomplete_array_types(
        ctx: &BindgenContext,
        result: &mut Vec<proc_macro2::TokenStream>,
    ) {
        let prefix = ctx.trait_prefix();

        // If the target supports `const fn`, declare eligible functions
        // as `const fn` else just `fn`.
        let const_fn = if ctx.options().rust_features().min_const_fn {
            quote! { const fn }
        } else {
            quote! { fn }
        };

        let incomplete_array_decl = quote! {
            #[repr(C)]
            #[derive(Default)]
            pub struct __IncompleteArrayField<T>(
                ::#prefix::marker::PhantomData<T>, [T; 0]);
        };

        let incomplete_array_impl = quote! {
            impl<T> __IncompleteArrayField<T> {
                #[inline]
                pub #const_fn new() -> Self {
                    __IncompleteArrayField(::#prefix::marker::PhantomData, [])
                }

                #[inline]
                pub fn as_ptr(&self) -> *const T {
                    self as *const _ as *const T
                }

                #[inline]
                pub fn as_mut_ptr(&mut self) -> *mut T {
                    self as *mut _ as *mut T
                }

                #[inline]
                pub unsafe fn as_slice(&self, len: usize) -> &[T] {
                    ::#prefix::slice::from_raw_parts(self.as_ptr(), len)
                }

                #[inline]
                pub unsafe fn as_mut_slice(&mut self, len: usize) -> &mut [T] {
                    ::#prefix::slice::from_raw_parts_mut(self.as_mut_ptr(), len)
                }
            }
        };

        let incomplete_array_debug_impl = quote! {
            impl<T> ::#prefix::fmt::Debug for __IncompleteArrayField<T> {
                fn fmt(&self, fmt: &mut ::#prefix::fmt::Formatter<'_>)
                       -> ::#prefix::fmt::Result {
                    fmt.write_str("__IncompleteArrayField")
                }
            }
        };

        let items = vec![
            incomplete_array_decl,
            incomplete_array_impl,
            incomplete_array_debug_impl,
        ];

        let old_items = mem::replace(result, items);
        result.extend(old_items.into_iter());
    }

    pub fn prepend_complex_type(result: &mut Vec<proc_macro2::TokenStream>) {
        let complex_type = quote! {
            #[derive(PartialEq, Copy, Clone, Hash, Debug, Default)]
            #[repr(C)]
            pub struct __BindgenComplex<T> {
                pub re: T,
                pub im: T
            }
        };

        let items = vec![complex_type];
        let old_items = mem::replace(result, items);
        result.extend(old_items.into_iter());
    }

    pub fn build_path(
        item: &Item,
        ctx: &BindgenContext,
    ) -> error::Result<proc_macro2::TokenStream> {
        let path = item.namespace_aware_canonical_path(ctx);
        let tokens =
            proc_macro2::TokenStream::from_str(&path.join("::")).unwrap();

        Ok(tokens)
    }

    fn primitive_ty(
        ctx: &BindgenContext,
        name: &str,
    ) -> proc_macro2::TokenStream {
        let ident = ctx.rust_ident_raw(name);
        quote! {
            #ident
        }
    }

    pub fn type_from_named(
        ctx: &BindgenContext,
        name: &str,
    ) -> Option<proc_macro2::TokenStream> {
        // FIXME: We could use the inner item to check this is really a
        // primitive type but, who the heck overrides these anyway?
        Some(match name {
            "int8_t" => primitive_ty(ctx, "i8"),
            "uint8_t" => primitive_ty(ctx, "u8"),
            "int16_t" => primitive_ty(ctx, "i16"),
            "uint16_t" => primitive_ty(ctx, "u16"),
            "int32_t" => primitive_ty(ctx, "i32"),
            "uint32_t" => primitive_ty(ctx, "u32"),
            "int64_t" => primitive_ty(ctx, "i64"),
            "uint64_t" => primitive_ty(ctx, "u64"),

            "size_t" if ctx.options().size_t_is_usize => {
                primitive_ty(ctx, "usize")
            }
            "uintptr_t" => primitive_ty(ctx, "usize"),

            "ssize_t" if ctx.options().size_t_is_usize => {
                primitive_ty(ctx, "isize")
            }
            "intptr_t" | "ptrdiff_t" => primitive_ty(ctx, "isize"),
            _ => return None,
        })
    }

    pub fn fnsig_return_ty(
        ctx: &BindgenContext,
        sig: &FunctionSig,
    ) -> proc_macro2::TokenStream {
        let return_item = ctx.resolve_item(sig.return_type());
        if let TypeKind::Void = *return_item.kind().expect_type().kind() {
            quote! {}
        } else {
            let ret_ty = return_item.to_rust_ty_or_opaque(ctx, &());
            quote! {
                -> #ret_ty
            }
        }
    }

    pub fn fnsig_arguments(
        ctx: &BindgenContext,
        sig: &FunctionSig,
    ) -> Vec<proc_macro2::TokenStream> {
        use super::ToPtr;

        let mut unnamed_arguments = 0;
        let mut args = sig
            .argument_types()
            .iter()
            .map(|&(ref name, ty)| {
                let arg_item = ctx.resolve_item(ty);
                let arg_ty = arg_item.kind().expect_type();

                // From the C90 standard[1]:
                //
                //     A declaration of a parameter as "array of type" shall be
                //     adjusted to "qualified pointer to type", where the type
                //     qualifiers (if any) are those specified within the [ and ] of
                //     the array type derivation.
                //
                // [1]: http://c0x.coding-guidelines.com/6.7.5.3.html
                let arg_ty = match *arg_ty.canonical_type(ctx).kind() {
                    TypeKind::Array(t, _) => {
                        let stream =
                            if ctx.options().array_pointers_in_arguments {
                                arg_ty.to_rust_ty_or_opaque(ctx, arg_item)
                            } else {
                                t.to_rust_ty_or_opaque(ctx, &())
                            };
                        stream.to_ptr(ctx.resolve_type(t).is_const())
                    }
                    TypeKind::Pointer(inner) => {
                        let inner = ctx.resolve_item(inner);
                        let inner_ty = inner.expect_type();
                        if let TypeKind::ObjCInterface(ref interface) =
                            *inner_ty.canonical_type(ctx).kind()
                        {
                            let name = ctx.rust_ident(interface.name());
                            quote! {
                                #name
                            }
                        } else {
                            arg_item.to_rust_ty_or_opaque(ctx, &())
                        }
                    }
                    _ => arg_item.to_rust_ty_or_opaque(ctx, &()),
                };

                let arg_name = match *name {
                    Some(ref name) => ctx.rust_mangle(name).into_owned(),
                    None => {
                        unnamed_arguments += 1;
                        format!("arg{}", unnamed_arguments)
                    }
                };

                assert!(!arg_name.is_empty());
                let arg_name = ctx.rust_ident(arg_name);

                quote! {
                    #arg_name : #arg_ty
                }
            })
            .collect::<Vec<_>>();

        if sig.is_variadic() {
            args.push(quote! { ... })
        }

        args
    }

    pub fn fnsig_argument_identifiers(
        ctx: &BindgenContext,
        sig: &FunctionSig,
    ) -> Vec<proc_macro2::TokenStream> {
        let mut unnamed_arguments = 0;
        let args = sig
            .argument_types()
            .iter()
            .map(|&(ref name, _ty)| {
                let arg_name = match *name {
                    Some(ref name) => ctx.rust_mangle(name).into_owned(),
                    None => {
                        unnamed_arguments += 1;
                        format!("arg{}", unnamed_arguments)
                    }
                };

                assert!(!arg_name.is_empty());
                let arg_name = ctx.rust_ident(arg_name);

                quote! {
                    #arg_name
                }
            })
            .collect::<Vec<_>>();

        args
    }

    pub fn fnsig_block(
        ctx: &BindgenContext,
        sig: &FunctionSig,
    ) -> proc_macro2::TokenStream {
        let args = sig.argument_types().iter().map(|&(_, ty)| {
            let arg_item = ctx.resolve_item(ty);

            arg_item.to_rust_ty_or_opaque(ctx, &())
        });

        let return_item = ctx.resolve_item(sig.return_type());
        let ret_ty =
            if let TypeKind::Void = *return_item.kind().expect_type().kind() {
                quote! { () }
            } else {
                return_item.to_rust_ty_or_opaque(ctx, &())
            };

        quote! {
            *const ::block::Block<(#(#args,)*), #ret_ty>
        }
    }

    // Returns true if `canonical_name` will end up as `mangled_name` at the
    // machine code level, i.e. after LLVM has applied any target specific
    // mangling.
    pub fn names_will_be_identical_after_mangling(
        canonical_name: &str,
        mangled_name: &str,
        call_conv: Option<Abi>,
    ) -> bool {
        // If the mangled name and the canonical name are the same then no
        // mangling can have happened between the two versions.
        if canonical_name == mangled_name {
            return true;
        }

        // Working with &[u8] makes indexing simpler than with &str
        let canonical_name = canonical_name.as_bytes();
        let mangled_name = mangled_name.as_bytes();

        let (mangling_prefix, expect_suffix) = match call_conv {
            Some(Abi::C) |
            // None is the case for global variables
            None => {
                (b'_', false)
            }
            Some(Abi::Stdcall) => (b'_', true),
            Some(Abi::Fastcall) => (b'@', true),

            // This is something we don't recognize, stay on the safe side
            // by emitting the `#[link_name]` attribute
            Some(_) => return false,
        };

        // Check that the mangled name is long enough to at least contain the
        // canonical name plus the expected prefix.
        if mangled_name.len() < canonical_name.len() + 1 {
            return false;
        }

        // Return if the mangled name does not start with the prefix expected
        // for the given calling convention.
        if mangled_name[0] != mangling_prefix {
            return false;
        }

        // Check that the mangled name contains the canonical name after the
        // prefix
        if &mangled_name[1..canonical_name.len() + 1] != canonical_name {
            return false;
        }

        // If the given calling convention also prescribes a suffix, check that
        // it exists too
        if expect_suffix {
            let suffix = &mangled_name[canonical_name.len() + 1..];

            // The shortest suffix is "@0"
            if suffix.len() < 2 {
                return false;
            }

            // Check that the suffix starts with '@' and is all ASCII decimals
            // after that.
            if suffix[0] != b'@' || !suffix[1..].iter().all(u8::is_ascii_digit)
            {
                return false;
            }
        } else if mangled_name.len() != canonical_name.len() + 1 {
            // If we don't expect a prefix but there is one, we need the
            // #[link_name] attribute
            return false;
        }

        true
    }
}
