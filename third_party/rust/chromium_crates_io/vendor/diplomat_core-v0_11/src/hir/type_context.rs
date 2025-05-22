//! Store all the types contained in the HIR.

use super::lowering::{ErrorAndContext, ErrorStore, ItemAndInfo};
use super::ty_position::StructPathLike;
use super::{
    AttributeValidator, Attrs, EnumDef, LoweringContext, LoweringError, MaybeStatic, OpaqueDef,
    OutStructDef, StructDef, TraitDef, TypeDef,
};
use crate::ast::attrs::AttrInheritContext;
#[allow(unused_imports)] // use in docs links
use crate::hir;
use crate::{ast, Env};
use core::fmt::{self, Display};
use smallvec::SmallVec;
use std::borrow::Cow;
use std::collections::HashMap;
use std::ops::Index;

/// A context type owning all types exposed to Diplomat.
#[derive(Debug)]
pub struct TypeContext {
    out_structs: Vec<OutStructDef>,
    structs: Vec<StructDef>,
    opaques: Vec<OpaqueDef>,
    enums: Vec<EnumDef>,
    traits: Vec<TraitDef>,
}

/// Additional features/config to support while lowering
#[non_exhaustive]
#[derive(Default, Debug, Copy, Clone)]
pub struct LoweringConfig {
    /// Support references in callback params (unsafe)
    pub unsafe_references_in_callbacks: bool,
}

/// Key used to index into a [`TypeContext`] representing a struct.
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct StructId(usize);

/// Key used to index into a [`TypeContext`] representing an out struct.
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct OutStructId(usize);

/// Key used to index into a [`TypeContext`] representing a opaque.
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct OpaqueId(usize);

/// Key used to index into a [`TypeContext`] representing an enum.
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct EnumId(usize);

/// Key used to index into a [`TypeContext`] representing a trait.
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct TraitId(usize);

#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[non_exhaustive]
pub enum TypeId {
    Struct(StructId),
    OutStruct(OutStructId),
    Opaque(OpaqueId),
    Enum(EnumId),
}

#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[non_exhaustive]
pub enum SymbolId {
    TypeId(TypeId),
    TraitId(TraitId),
}

enum Param<'a> {
    Input(&'a str),
    Return,
}

impl Display for Param<'_> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        if let Param::Input(s) = *self {
            write!(f, "param {s}")
        } else {
            write!(f, "return type")
        }
    }
}

impl TypeContext {
    pub fn all_types<'tcx>(&'tcx self) -> impl Iterator<Item = (TypeId, TypeDef<'tcx>)> {
        self.structs
            .iter()
            .enumerate()
            .map(|(i, ty)| (TypeId::Struct(StructId(i)), TypeDef::Struct(ty)))
            .chain(
                self.out_structs
                    .iter()
                    .enumerate()
                    .map(|(i, ty)| (TypeId::OutStruct(OutStructId(i)), TypeDef::OutStruct(ty))),
            )
            .chain(
                self.opaques
                    .iter()
                    .enumerate()
                    .map(|(i, ty)| (TypeId::Opaque(OpaqueId(i)), TypeDef::Opaque(ty))),
            )
            .chain(
                self.enums
                    .iter()
                    .enumerate()
                    .map(|(i, ty)| (TypeId::Enum(EnumId(i)), TypeDef::Enum(ty))),
            )
    }

    pub fn all_traits<'tcx>(&'tcx self) -> impl Iterator<Item = (TraitId, &'tcx TraitDef)> {
        self.traits
            .iter()
            .enumerate()
            .map(|(i, trt)| (TraitId(i), trt))
    }

    pub fn out_structs(&self) -> &[OutStructDef] {
        &self.out_structs
    }

    pub fn structs(&self) -> &[StructDef] {
        &self.structs
    }

    pub fn opaques(&self) -> &[OpaqueDef] {
        &self.opaques
    }

    pub fn enums(&self) -> &[EnumDef] {
        &self.enums
    }

    pub fn traits(&self) -> &[TraitDef] {
        &self.traits
    }

    pub fn resolve_type<'tcx>(&'tcx self, id: TypeId) -> TypeDef<'tcx> {
        match id {
            TypeId::Struct(i) => TypeDef::Struct(self.resolve_struct(i)),
            TypeId::OutStruct(i) => TypeDef::OutStruct(self.resolve_out_struct(i)),
            TypeId::Opaque(i) => TypeDef::Opaque(self.resolve_opaque(i)),
            TypeId::Enum(i) => TypeDef::Enum(self.resolve_enum(i)),
        }
    }

    /// Helper methods for resolving different IDs.
    ///
    /// Prefer using `resolve_type()` for simplicity.
    pub fn resolve_out_struct(&self, id: OutStructId) -> &OutStructDef {
        self.out_structs.index(id.0)
    }

    /// Helper methods for resolving different IDs.
    ///
    /// Prefer using `resolve_type()` for simplicity.
    pub fn resolve_struct(&self, id: StructId) -> &StructDef {
        self.structs.index(id.0)
    }

    /// Helper methods for resolving different IDs.
    ///
    /// Prefer using `resolve_type()` for simplicity.
    pub fn resolve_opaque(&self, id: OpaqueId) -> &OpaqueDef {
        self.opaques.index(id.0)
    }

    /// Helper methods for resolving different IDs.
    ///
    /// Prefer using `resolve_type()` for simplicity.
    pub fn resolve_enum(&self, id: EnumId) -> &EnumDef {
        self.enums.index(id.0)
    }

    pub fn resolve_trait(&self, id: TraitId) -> &TraitDef {
        self.traits.index(id.0)
    }

    /// Resolve and format a named type for use in diagnostics
    /// (don't apply rename rules and such)
    pub fn fmt_type_name_diagnostics(&self, id: TypeId) -> Cow<str> {
        self.resolve_type(id).name().as_str().into()
    }

    pub fn fmt_symbol_name_diagnostics(&self, id: SymbolId) -> Cow<str> {
        match id {
            SymbolId::TypeId(id) => self.fmt_type_name_diagnostics(id),
            SymbolId::TraitId(id) => self.resolve_trait(id).name.as_str().into(),
        }
    }

    /// Lower the AST to the HIR while simultaneously performing validation.
    pub fn from_syn<'ast>(
        s: &'ast syn::File,
        cfg: LoweringConfig,
        attr_validator: impl AttributeValidator + 'static,
    ) -> Result<Self, Vec<ErrorAndContext>> {
        let types = ast::File::from(s).all_types();
        let (mut ctx, hir) = Self::from_ast_without_validation(&types, cfg, attr_validator)?;
        ctx.errors.set_item("(validation)");
        hir.validate(&mut ctx.errors);
        if !ctx.errors.is_empty() {
            return Err(ctx.errors.take_errors());
        }
        Ok(hir)
    }

    /// Lower the AST to the HIR, without validation. For testing
    pub(super) fn from_ast_without_validation<'ast>(
        env: &'ast Env,
        cfg: LoweringConfig,
        attr_validator: impl AttributeValidator + 'static,
    ) -> Result<(LoweringContext<'ast>, Self), Vec<ErrorAndContext>> {
        let mut ast_out_structs = SmallVec::<[_; 16]>::new();
        let mut ast_structs = SmallVec::<[_; 16]>::new();
        let mut ast_opaques = SmallVec::<[_; 16]>::new();
        let mut ast_enums = SmallVec::<[_; 16]>::new();
        let mut ast_traits = SmallVec::<[_; 16]>::new();

        let mut errors = ErrorStore::default();

        for (path, mod_env) in env.iter_modules() {
            errors.set_item(
                path.elements
                    .last()
                    .map(|m| m.as_str())
                    .unwrap_or("root module"),
            );
            let mod_attrs = Attrs::from_ast(
                &mod_env.attrs,
                &attr_validator,
                &Default::default(),
                &mut errors,
            );
            let ty_attrs = mod_attrs.for_inheritance(AttrInheritContext::Type);
            let method_attrs =
                mod_attrs.for_inheritance(AttrInheritContext::MethodOrImplFromModule);

            for sym in mod_env.items() {
                match sym {
                    ast::ModSymbol::CustomType(custom_type) => match custom_type {
                        ast::CustomType::Struct(strct) => {
                            let id = if strct.output_only {
                                TypeId::OutStruct(OutStructId(ast_out_structs.len()))
                            } else {
                                TypeId::Struct(StructId(ast_structs.len()))
                            };
                            let item = ItemAndInfo {
                                item: strct,
                                in_path: path,
                                ty_parent_attrs: ty_attrs.clone(),
                                method_parent_attrs: method_attrs.clone(),
                                id: id.into(),
                            };
                            if strct.output_only {
                                ast_out_structs.push(item);
                            } else {
                                ast_structs.push(item);
                            }
                        }
                        ast::CustomType::Opaque(opaque) => {
                            let item = ItemAndInfo {
                                item: opaque,
                                in_path: path,
                                ty_parent_attrs: ty_attrs.clone(),
                                method_parent_attrs: method_attrs.clone(),
                                id: TypeId::Opaque(OpaqueId(ast_opaques.len())).into(),
                            };
                            ast_opaques.push(item)
                        }
                        ast::CustomType::Enum(enm) => {
                            let item = ItemAndInfo {
                                item: enm,
                                in_path: path,
                                ty_parent_attrs: ty_attrs.clone(),
                                method_parent_attrs: method_attrs.clone(),
                                id: TypeId::Enum(EnumId(ast_enums.len())).into(),
                            };
                            ast_enums.push(item)
                        }
                    },
                    ast::ModSymbol::Trait(trt) => {
                        let item = ItemAndInfo {
                            item: trt,
                            in_path: path,
                            ty_parent_attrs: ty_attrs.clone(),
                            method_parent_attrs: method_attrs.clone(),
                            id: TraitId(ast_traits.len()).into(),
                        };
                        ast_traits.push(item)
                    }
                    _ => {}
                }
            }
        }

        let lookup_id = LookupId::new(
            &ast_out_structs[..],
            &ast_structs[..],
            &ast_opaques[..],
            &ast_enums[..],
            &ast_traits[..],
        );
        let attr_validator = Box::new(attr_validator);

        let mut ctx = LoweringContext {
            lookup_id,
            env,
            errors,
            attr_validator,
            cfg,
        };

        let out_structs = ctx.lower_all_out_structs(ast_out_structs.into_iter());
        let structs = ctx.lower_all_structs(ast_structs.into_iter());
        let opaques = ctx.lower_all_opaques(ast_opaques.into_iter());
        let enums = ctx.lower_all_enums(ast_enums.into_iter());
        let traits = ctx.lower_all_traits(ast_traits.into_iter()).unwrap();

        match (out_structs, structs, opaques, enums) {
            (Ok(out_structs), Ok(structs), Ok(opaques), Ok(enums)) => {
                let res = Self {
                    out_structs,
                    structs,
                    opaques,
                    enums,
                    traits,
                };

                if !ctx.errors.is_empty() {
                    return Err(ctx.errors.take_errors());
                }
                Ok((ctx, res))
            }
            _ => {
                assert!(
                    !ctx.errors.is_empty(),
                    "Lowering failed without error messages"
                );
                Err(ctx.errors.take_errors())
            }
        }
    }

    /// Run validation phase
    ///
    /// Currently validates that methods are not inheriting any transitive bounds from parameters
    ///    Todo: Automatically insert these bounds during HIR construction in a second phase
    fn validate<'hir>(&'hir self, errors: &mut ErrorStore<'hir>) {
        // Lifetime validity check
        for (_id, ty) in self.all_types() {
            errors.set_item(ty.name().as_str());
            for method in ty.methods() {
                errors.set_subitem(method.name.as_str());

                // This check must occur before validate_ty_in_method is called
                // since validate_ty_in_method calls link_lifetimes which does not
                // work for structs with elision
                let mut failed = false;
                method.output.with_contained_types(|out_ty| {
                    for lt in out_ty.lifetimes() {
                        if let MaybeStatic::NonStatic(lt) = lt {
                            if method.lifetime_env.get_bounds(lt).is_none() {
                                errors.push(LoweringError::Other(
                                    "Found elided lifetime in return type, please explicitly specify".into(),
                                ));

                                failed = true;
                                break;
                            }
                        }
                    }
                });

                if failed {
                    // link_lifetimes will fail if elision exists
                    continue;
                }

                for param in &method.params {
                    self.validate_ty_in_method(
                        errors,
                        Param::Input(param.name.as_str()),
                        &param.ty,
                        method,
                    )
                }

                method.output.with_contained_types(|out_ty| {
                    self.validate_ty_in_method(errors, Param::Return, out_ty, method);
                })
            }
        }
    }

    /// Ensure that a given method's input our output type does not implicitly introduce bounds that are not
    /// already specified on the method
    fn validate_ty_in_method<P: hir::TyPosition>(
        &self,
        errors: &mut ErrorStore,
        param: Param,
        param_ty: &hir::Type<P>,
        method: &hir::Method,
    ) {
        let linked = match &param_ty {
            hir::Type::Opaque(p) => p.link_lifetimes(self),
            hir::Type::Struct(p) => p.link_lifetimes(self),
            _ => return,
        };

        for (use_lt, def_lt) in linked.lifetimes_all() {
            let MaybeStatic::NonStatic(use_lt) = use_lt else {
                continue;
            };
            let Some(use_bounds) = &method.lifetime_env.get_bounds(use_lt) else {
                continue;
            };
            let use_longer_lifetimes = &use_bounds.longer;
            let anchor;
            let def_longer_lifetimes = if let Some(def_lt) = def_lt {
                let Some(def_bounds) = &linked.def_env().get_bounds(def_lt) else {
                    continue;
                };
                &def_bounds.longer
            } else {
                anchor = linked.def_env().all_lifetimes().collect();
                &anchor
            };

            for def_longer in def_longer_lifetimes {
                let MaybeStatic::NonStatic(corresponding_use) = linked.def_to_use(*def_longer)
                else {
                    continue;
                };

                // In the case of stuff like <'a, 'a> passed to Foo<'x, 'y: 'x> the bound
                // is trivially fulfilled
                if corresponding_use == use_lt {
                    continue;
                }

                if !use_longer_lifetimes.contains(&corresponding_use) {
                    let use_name = method.lifetime_env.fmt_lifetime(use_lt);
                    let use_longer_name = method.lifetime_env.fmt_lifetime(corresponding_use);
                    let def_cause = if let Some(def_lt) = def_lt {
                        let def_name = linked.def_env().fmt_lifetime(def_lt);
                        let def_longer_name = linked.def_env().fmt_lifetime(def_longer);
                        format!("comes from source type's '{def_longer_name}: '{def_name}")
                    } else {
                        // This case is technically already handled in the lifetime lowerer, we're being careful
                        "comes from &-ref's lifetime in parameter".into()
                    };
                    errors.push(LoweringError::Other(format!("Method should explicitly include this \
                                        lifetime bound from {param}: '{use_longer_name}: '{use_name} ({def_cause})")))
                }
            }
        }
    }
}

/// Struct that just wraps the mapping from AST custom types to their IDs that
/// will show up in the final [`TypeContext`].
///
/// The entire point of this type is to reduce the number of arguments in helper
/// functions which need to look up IDs for structs. It does nothing fancy and
/// is only ever used when constructing a [`TypeContext`].
pub(super) struct LookupId<'ast> {
    out_struct_map: HashMap<&'ast ast::Struct, OutStructId>,
    struct_map: HashMap<&'ast ast::Struct, StructId>,
    opaque_map: HashMap<&'ast ast::OpaqueType, OpaqueId>,
    enum_map: HashMap<&'ast ast::Enum, EnumId>,
    trait_map: HashMap<&'ast ast::Trait, TraitId>,
}

impl<'ast> LookupId<'ast> {
    /// Returns a new [`LookupId`].
    fn new(
        out_structs: &[ItemAndInfo<'ast, ast::Struct>],
        structs: &[ItemAndInfo<'ast, ast::Struct>],
        opaques: &[ItemAndInfo<'ast, ast::OpaqueType>],
        enums: &[ItemAndInfo<'ast, ast::Enum>],
        traits: &[ItemAndInfo<'ast, ast::Trait>],
    ) -> Self {
        Self {
            out_struct_map: out_structs
                .iter()
                .enumerate()
                .map(|(index, item)| (item.item, OutStructId(index)))
                .collect(),
            struct_map: structs
                .iter()
                .enumerate()
                .map(|(index, item)| (item.item, StructId(index)))
                .collect(),
            opaque_map: opaques
                .iter()
                .enumerate()
                .map(|(index, item)| (item.item, OpaqueId(index)))
                .collect(),
            enum_map: enums
                .iter()
                .enumerate()
                .map(|(index, item)| (item.item, EnumId(index)))
                .collect(),
            trait_map: traits
                .iter()
                .enumerate()
                .map(|(index, item)| (item.item, TraitId(index)))
                .collect(),
        }
    }

    pub(super) fn resolve_out_struct(&self, strct: &ast::Struct) -> Option<OutStructId> {
        self.out_struct_map.get(strct).copied()
    }

    pub(super) fn resolve_struct(&self, strct: &ast::Struct) -> Option<StructId> {
        self.struct_map.get(strct).copied()
    }

    pub(super) fn resolve_opaque(&self, opaque: &ast::OpaqueType) -> Option<OpaqueId> {
        self.opaque_map.get(opaque).copied()
    }

    pub(super) fn resolve_enum(&self, enm: &ast::Enum) -> Option<EnumId> {
        self.enum_map.get(enm).copied()
    }

    pub(super) fn resolve_trait(&self, trt: &ast::Trait) -> Option<TraitId> {
        self.trait_map.get(trt).copied()
    }
}

impl From<StructId> for TypeId {
    fn from(x: StructId) -> Self {
        TypeId::Struct(x)
    }
}

impl From<OutStructId> for TypeId {
    fn from(x: OutStructId) -> Self {
        TypeId::OutStruct(x)
    }
}

impl From<OpaqueId> for TypeId {
    fn from(x: OpaqueId) -> Self {
        TypeId::Opaque(x)
    }
}

impl From<EnumId> for TypeId {
    fn from(x: EnumId) -> Self {
        TypeId::Enum(x)
    }
}

impl From<TypeId> for SymbolId {
    fn from(x: TypeId) -> Self {
        SymbolId::TypeId(x)
    }
}

impl From<TraitId> for SymbolId {
    fn from(x: TraitId) -> Self {
        SymbolId::TraitId(x)
    }
}

impl TryInto<TypeId> for SymbolId {
    type Error = ();
    fn try_into(self) -> Result<TypeId, Self::Error> {
        match self {
            SymbolId::TypeId(id) => Ok(id),
            _ => Err(()),
        }
    }
}

impl TryInto<TraitId> for SymbolId {
    type Error = ();
    fn try_into(self) -> Result<TraitId, Self::Error> {
        match self {
            SymbolId::TraitId(id) => Ok(id),
            _ => Err(()),
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::hir;
    use std::fmt::Write;

    macro_rules! uitest_lowering {
        ($($file:tt)*) => {
            let parsed: syn::File = syn::parse_quote! { $($file)* };

            let mut output = String::new();

            let mut attr_validator = hir::BasicAttributeValidator::new("tests");
            attr_validator.support.option = true;
            match hir::TypeContext::from_syn(&parsed, Default::default(), attr_validator) {
                Ok(_context) => (),
                Err(e) => {
                    for (ctx, err) in e {
                        writeln!(&mut output, "Lowering error in {ctx}: {err}").unwrap();
                    }
                }
            };
            insta::with_settings!({}, {
                insta::assert_snapshot!(output)
            });
        }
    }

    #[test]
    fn test_required_implied_bounds() {
        uitest_lowering! {
            #[diplomat::bridge]
            mod ffi {
                #[diplomat::opaque]
                struct Foo<'a, 'b: 'a, 'c: 'b> (&'a u8, &'b u8, &'c u8);

                #[diplomat::opaque]
                struct Opaque;


                #[diplomat::opaque]
                struct OneLifetime<'a>(&'a u8);

                impl Opaque {
                    pub fn use_foo<'x, 'y, 'z>(&self, foo: &Foo<'x, 'y, 'z>) {}
                    pub fn return_foo<'x, 'y, 'z>(&'x self) -> Box<Foo<'x, 'y, 'z>> {}
                    pub fn return_result_foo<'x, 'y, 'z>(&'x self) -> Result<Box<Foo<'x, 'y, 'z>>, ()> {}
                    // This doesn't actually error since the lowerer inserts the implicit bound
                    pub fn implied_ref_bound<'a, 'b>(&self, one_lt: &'a OneLifetime<'b>) {}
                }
            }
        }
    }

    /// This is a buch of tests put together
    #[test]
    fn test_basic_lowering() {
        uitest_lowering! {
            #[diplomat::bridge]
            mod other_ffi {

                struct Foo {
                    field: u8
                }

                #[diplomat::out]
                struct OutStruct {
                    field: Box<OtherOpaque>,
                }

                #[diplomat::opaque]
                struct OtherOpaque;
            }
            #[diplomat::bridge]
            mod ffi {
                use crate::other_ffi::{Foo, OutStruct, OtherOpaque};

                #[diplomat::opaque]
                struct Opaque;

                struct EmptyStruct;

                enum EmptyEnum {}

                struct InStructWithOutField {
                    field: Box<OtherOpaque>,
                    out_struct: OutStruct,
                }

                struct BadStructFields {
                    field1: Option<u8>,
                    field2: Result<u8, u8>,
                }

                impl Opaque {
                    pub fn use_foo_ref(&self, foo: &Foo) {}
                    pub fn return_foo_box(&self) -> Box<Foo> {}
                    pub fn use_self(self) {}
                    pub fn return_self(self) -> Self {}
                    pub fn use_opaque_owned(&self, opaque: OtherOpaque) {}
                    pub fn return_opaque_owned(&self) -> OtherOpaque {}
                    pub fn use_out_as_in(&self, out: OutStruct) {}
                }

            }
        }
    }

    #[test]
    fn test_opaque_ffi() {
        uitest_lowering! {
            #[diplomat::bridge]
            mod ffi {
                #[diplomat::opaque]
                struct MyOpaqueStruct(UnknownType);

                impl MyOpaqueStruct {
                    pub fn new() -> Box<MyOpaqueStruct> {}
                    pub fn new_broken() -> MyOpaqueStruct {}
                    pub fn do_thing(&self) {}
                    pub fn do_thing_broken(self) {}
                    pub fn broken_differently(&self, x: &MyOpaqueStruct) {}
                }

                #[diplomat::opaque]
                enum MyOpaqueEnum {
                    A(UnknownType),
                    B,
                    C(i32, i32, UnknownType2),
                }

                impl MyOpaqueEnum {
                    pub fn new() -> Box<MyOpaqueEnum> {}
                    pub fn new_broken() -> MyOpaqueEnum {}
                    pub fn do_thing(&self) {}
                    pub fn do_thing_broken(self) {}
                    pub fn broken_differently(&self, x: &MyOpaqueEnum) {}
                }
            }
        }
    }

    #[test]
    fn opaque_checks_with_safe_use() {
        uitest_lowering! {
            #[diplomat::bridge]
            mod ffi {
                struct NonOpaqueStruct {}

                impl NonOpaqueStruct {
                    pub fn new(x: i32) -> NonOpaqueStruct {
                        unimplemented!();
                    }
                }

                #[diplomat::opaque]
                struct OpaqueStruct {}

                impl OpaqueStruct {
                    pub fn new() -> Box<OpaqueStruct> {
                        unimplemented!();
                    }

                    pub fn get_i32(&self) -> i32 {
                        unimplemented!()
                    }
                }
            }
        };
    }

    #[test]
    fn opaque_checks_with_error() {
        uitest_lowering! {
            #[diplomat::bridge]
            mod ffi {
                #[diplomat::opaque]
                struct OpaqueStruct {}

                impl OpaqueStruct {
                    pub fn new() -> OpaqueStruct {
                        unimplemented!();
                    }

                    pub fn get_i32(self) -> i32 {
                        unimplemented!()
                    }
                }
            }
        };
    }

    #[test]
    fn zst_non_opaque() {
        uitest_lowering! {
            #[diplomat::bridge]
            mod ffi {
                struct NonOpaque;

                impl NonOpaque {
                    pub fn foo(self) {}
                }
            }
        };
    }

    #[test]
    fn option_invalid() {
        uitest_lowering! {
            #[diplomat::bridge]
            mod ffi {
                use diplomat_runtime::DiplomatResult;
                struct Foo {
                    field: Option<u8>,
                }

                impl Foo {
                    pub fn do_thing(opt: Option<Option<u16>>) {

                    }

                    pub fn do_thing2(opt: DiplomatResult<Option<DiplomatChar>, u8>) {

                    }
                    pub fn do_thing2(opt: Option<u16>) {

                    }

                    pub fn do_thing3() -> Option<u16> {

                    }
                }
            }
        };
    }

    #[test]
    fn option_valid() {
        uitest_lowering! {
            #[diplomat::bridge]
            mod ffi {
                struct Foo {
                    field: Option<Box<u8>>,
                }

                impl Foo {
                    pub fn do_thing(opt: Option<Box<u32>>) {

                    }
                    pub fn do_thing2(opt: Option<&u32>) {

                    }
                }
            }
        };
    }

    #[test]
    fn non_opaque_move() {
        uitest_lowering! {
            #[diplomat::bridge]
            mod ffi {
                struct NonOpaque {
                    num: u8,
                }

                impl NonOpaque {
                    pub fn foo(&self) {}
                }

                #[diplomat::opaque]
                struct Opaque;

                impl Opaque {
                    pub fn bar<'a>(&'a self) -> &'a NonOpaque {}
                    pub fn baz<'a>(&'a self, x: &'a NonOpaque) {}
                    pub fn quux(&self) -> Box<NonOpaque> {}
                }
            }
        };
    }

    #[test]
    fn test_lifetime_in_return() {
        uitest_lowering! {
            #[diplomat::bridge]
            mod ffi {
                #[diplomat::opaque]
                struct Opaque;

                struct Foo<'a> {
                    x: &'a Opaque,
                }

                impl Opaque {
                    pub fn returns_self(&self) -> &Self {}
                    pub fn returns_foo(&self) -> Foo {}
                }
            }
        };
    }
    #[test]
    fn test_struct_forbidden() {
        uitest_lowering! {
            #[diplomat::bridge]
            mod ffi {
                struct Crimes<'a> {
                    slice1: &'a str,
                    slice1: &'a DiplomatStr,
                    slice2: &'a [u8],
                    slice3: Box<str>,
                    slice3: Box<DiplomatStr>,
                    slice4: Box<[u8]>,
                }
            }
        };
    }

    #[test]
    fn test_option() {
        uitest_lowering! {
            #[diplomat::bridge]
                mod ffi {
                use diplomat_runtime::DiplomatOption;
                #[diplomat::opaque]
                struct Foo {}
                struct CustomStruct {
                    num: u8,
                    b: bool,
                    diplo_option: DiplomatOption<u8>,
                }

                struct BrokenStruct {
                    regular_option: Option<u8>,
                    regular_option: Option<CustomStruct>,
                }
                impl Foo {
                    pub fn diplo_option_u8(x: DiplomatOption<u8>) -> DiplomatOption<u8> {
                        x
                    }
                    pub fn diplo_option_ref(x: DiplomatOption<&Foo>) -> DiplomatOption<&Foo> {
                        x
                    }
                    pub fn diplo_option_box() -> DiplomatOption<Box<Foo>> {
                        x
                    }
                    pub fn diplo_option_struct(x: DiplomatOption<CustomStruct>) -> DiplomatOption<CustomStruct> {
                        x
                    }
                    pub fn option_u8(x: Option<u8>) -> Option<u8> {
                        x
                    }
                    pub fn option_ref(x: Option<&Foo>) -> Option<&Foo> {
                        x
                    }
                    pub fn option_box() -> Option<Box<Foo>> {
                        x
                    }
                    pub fn option_struct(x: Option<CustomStruct>) -> Option<CustomStruct> {
                        x
                    }
                }
            }
        };
    }
}
