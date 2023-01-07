// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use indexmap::set::IndexSet as HashSet;

use autocxx_parser::{
    directive_names::{EXTERN_RUST_FUN, EXTERN_RUST_TYPE},
    RustFun, RustPath,
};
use itertools::Itertools;
use proc_macro2::Ident;
use syn::{
    parse_quote, punctuated::Punctuated, Attribute, Binding, Expr, ExprAssign, ExprAssignOp,
    ExprAwait, ExprBinary, ExprBox, ExprBreak, ExprCast, ExprField, ExprGroup, ExprLet, ExprParen,
    ExprReference, ExprTry, ExprType, ExprUnary, ImplItem, Item, ItemEnum, ItemStruct, Pat, PatBox,
    PatReference, PatSlice, PatTuple, Path, Receiver, ReturnType, Signature, Stmt, TraitItem, Type,
    TypeArray, TypeGroup, TypeParamBound, TypeParen, TypePtr, TypeReference, TypeSlice,
};
use thiserror::Error;

#[derive(Default)]
pub(super) struct Discoveries {
    pub(super) cpp_list: HashSet<String>,
    pub(super) extern_rust_funs: Vec<RustFun>,
    pub(super) extern_rust_types: Vec<RustPath>,
}

#[derive(Error, Debug)]
pub enum DiscoveryErr {
    #[error("#[extern_rust_function] was attached to a method in an impl block that was too complex for autocxx. autocxx supports only \"impl X {{...}}\" where X is a single identifier, not a path or more complex type.")]
    FoundExternRustFunOnTypeWithoutClearReceiver,
    #[error("#[extern_rust_function] was attached to a method taking no parameters.")]
    NonReferenceReceiver,
    #[error("#[extern_rust_function] was attached to a method taking a receiver by value.")]
    NoParameterOnMethod,
    #[error("#[extern_rust_function] was in an impl block nested wihtin another block. This is only supported in the outermost mod of a file, alongside the include_cpp!.")]
    FoundExternRustFunWithinMod,
}

impl Discoveries {
    pub(super) fn search_item(
        &mut self,
        item: &Item,
        mod_path: Option<RustPath>,
    ) -> Result<(), DiscoveryErr> {
        let mut this_mod = PerModDiscoveries {
            discoveries: self,
            mod_path,
        };
        this_mod.search_item(item)
    }

    pub(crate) fn found_allowlist(&self) -> bool {
        !self.cpp_list.is_empty()
    }

    pub(crate) fn found_rust(&self) -> bool {
        !self.extern_rust_funs.is_empty() || !self.extern_rust_types.is_empty()
    }

    pub(crate) fn extend(&mut self, other: Self) {
        self.cpp_list.extend(other.cpp_list);
        self.extern_rust_funs.extend(other.extern_rust_funs);
        self.extern_rust_types.extend(other.extern_rust_types);
    }
}

struct PerModDiscoveries<'a> {
    discoveries: &'a mut Discoveries,
    mod_path: Option<RustPath>,
}

impl<'b> PerModDiscoveries<'b> {
    fn deeper_path(&self, id: &Ident) -> RustPath {
        match &self.mod_path {
            None => RustPath::new_from_ident(id.clone()),
            Some(mod_path) => mod_path.append(id.clone()),
        }
    }

    fn search_item(&mut self, item: &Item) -> Result<(), DiscoveryErr> {
        match item {
            Item::Fn(fun) => {
                for stmt in &fun.block.stmts {
                    self.search_stmt(stmt)?
                }
                self.search_return_type(&fun.sig.output)?;
                for i in &fun.sig.inputs {
                    match i {
                        syn::FnArg::Receiver(_) => {}
                        syn::FnArg::Typed(pt) => {
                            self.search_pat(&pt.pat)?;
                            self.search_type(&pt.ty)?;
                        }
                    }
                }
                if Self::has_attr(&fun.attrs, EXTERN_RUST_FUN) {
                    self.discoveries.extern_rust_funs.push(RustFun {
                        path: self.deeper_path(&fun.sig.ident),
                        sig: fun.sig.clone(),
                        receiver: None,
                    });
                }
            }
            Item::Impl(imp) => {
                let receiver = match imp.trait_ {
                    // We do not allow 'extern_rust_fun' on trait impls
                    Some(_) => None,
                    None => match &*imp.self_ty {
                        Type::Path(typ) => {
                            let mut segs = typ.path.segments.iter();
                            let id = segs.next();
                            if let Some(seg) = id {
                                if segs.next().is_some() {
                                    None
                                } else {
                                    Some(self.deeper_path(&seg.ident))
                                }
                            } else {
                                None
                            }
                        }
                        _ => None,
                    },
                };
                for item in &imp.items {
                    self.search_impl_item(item, receiver.as_ref())?
                }
            }
            Item::Mod(md) => {
                if let Some((_, items)) = &md.content {
                    let mod_path = Some(self.deeper_path(&md.ident));
                    let mut new_mod = PerModDiscoveries {
                        discoveries: self.discoveries,
                        mod_path,
                    };
                    for item in items {
                        new_mod.search_item(item)?
                    }
                }
            }
            Item::Trait(tr) => {
                for item in &tr.items {
                    self.search_trait_item(item)?
                }
            }
            Item::Struct(ItemStruct { ident, attrs, .. })
            | Item::Enum(ItemEnum { ident, attrs, .. })
                if Self::has_attr(attrs, EXTERN_RUST_TYPE) =>
            {
                self.discoveries
                    .extern_rust_types
                    .push(self.deeper_path(ident));
            }
            _ => {}
        }
        Ok(())
    }

    fn search_path(&mut self, path: &Path) -> Result<(), DiscoveryErr> {
        let mut seg_iter = path.segments.iter();
        if let Some(first_seg) = seg_iter.next() {
            if first_seg.ident == "ffi" {
                self.discoveries
                    .cpp_list
                    .insert(seg_iter.map(|seg| seg.ident.to_string()).join("::"));
            }
        }
        for seg in path.segments.iter() {
            self.search_path_arguments(&seg.arguments)?;
        }
        Ok(())
    }

    fn search_trait_item(&mut self, itm: &TraitItem) -> Result<(), DiscoveryErr> {
        if let TraitItem::Method(itm) = itm {
            if let Some(block) = &itm.default {
                self.search_stmts(block.stmts.iter())?
            }
        }
        Ok(())
    }

    fn search_stmts<'a>(
        &mut self,
        stmts: impl Iterator<Item = &'a Stmt>,
    ) -> Result<(), DiscoveryErr> {
        for stmt in stmts {
            self.search_stmt(stmt)?
        }
        Ok(())
    }

    fn search_stmt(&mut self, stmt: &Stmt) -> Result<(), DiscoveryErr> {
        match stmt {
            Stmt::Local(lcl) => {
                if let Some((_, expr)) = &lcl.init {
                    self.search_expr(expr)?
                }
                self.search_pat(&lcl.pat)
            }
            Stmt::Item(itm) => self.search_item(itm),
            Stmt::Expr(exp) | Stmt::Semi(exp, _) => self.search_expr(exp),
        }
    }

    fn search_expr(&mut self, expr: &Expr) -> Result<(), DiscoveryErr> {
        match expr {
            Expr::Path(exp) => {
                self.search_path(&exp.path)?;
            }
            Expr::Macro(_) => {}
            Expr::Array(array) => self.search_exprs(array.elems.iter())?,
            Expr::Assign(ExprAssign { left, right, .. })
            | Expr::AssignOp(ExprAssignOp { left, right, .. })
            | Expr::Binary(ExprBinary { left, right, .. }) => {
                self.search_expr(left)?;
                self.search_expr(right)?;
            }
            Expr::Async(ass) => self.search_stmts(ass.block.stmts.iter())?,
            Expr::Await(ExprAwait { base, .. }) | Expr::Field(ExprField { base, .. }) => {
                self.search_expr(base)?
            }
            Expr::Block(blck) => self.search_stmts(blck.block.stmts.iter())?,
            Expr::Box(ExprBox { expr, .. })
            | Expr::Break(ExprBreak {
                expr: Some(expr), ..
            })
            | Expr::Cast(ExprCast { expr, .. })
            | Expr::Group(ExprGroup { expr, .. })
            | Expr::Paren(ExprParen { expr, .. })
            | Expr::Reference(ExprReference { expr, .. })
            | Expr::Try(ExprTry { expr, .. })
            | Expr::Type(ExprType { expr, .. })
            | Expr::Unary(ExprUnary { expr, .. }) => self.search_expr(expr)?,
            Expr::Call(exc) => {
                self.search_expr(&exc.func)?;
                self.search_exprs(exc.args.iter())?;
            }
            Expr::Closure(cls) => self.search_expr(&cls.body)?,
            Expr::Continue(_)
            | Expr::Lit(_)
            | Expr::Break(ExprBreak { expr: None, .. })
            | Expr::Verbatim(_) => {}
            Expr::ForLoop(fl) => {
                self.search_expr(&fl.expr)?;
                self.search_stmts(fl.body.stmts.iter())?;
            }
            Expr::If(exif) => {
                self.search_expr(&exif.cond)?;
                self.search_stmts(exif.then_branch.stmts.iter())?;
                if let Some((_, else_branch)) = &exif.else_branch {
                    self.search_expr(else_branch)?;
                }
            }
            Expr::Index(exidx) => {
                self.search_expr(&exidx.expr)?;
                self.search_expr(&exidx.index)?;
            }
            Expr::Let(ExprLet { expr, pat, .. }) => {
                self.search_expr(expr)?;
                self.search_pat(pat)?;
            }
            Expr::Loop(exloo) => self.search_stmts(exloo.body.stmts.iter())?,
            Expr::Match(exm) => {
                self.search_expr(&exm.expr)?;
                for a in &exm.arms {
                    self.search_expr(&a.body)?;
                    if let Some((_, guard)) = &a.guard {
                        self.search_expr(guard)?;
                    }
                }
            }
            Expr::MethodCall(mtc) => {
                self.search_expr(&mtc.receiver)?;
                self.search_exprs(mtc.args.iter())?;
            }
            Expr::Range(exr) => {
                self.search_option_expr(&exr.from)?;
                self.search_option_expr(&exr.to)?;
            }
            Expr::Repeat(exr) => {
                self.search_expr(&exr.expr)?;
                self.search_expr(&exr.len)?;
            }
            Expr::Return(exret) => {
                if let Some(expr) = &exret.expr {
                    self.search_expr(expr)?;
                }
            }
            Expr::Struct(exst) => {
                for f in &exst.fields {
                    self.search_expr(&f.expr)?;
                }
                self.search_option_expr(&exst.rest)?;
            }
            Expr::TryBlock(extb) => self.search_stmts(extb.block.stmts.iter())?,
            Expr::Tuple(ext) => self.search_exprs(ext.elems.iter())?,
            Expr::Unsafe(exs) => self.search_stmts(exs.block.stmts.iter())?,
            Expr::While(exw) => {
                self.search_expr(&exw.cond)?;
                self.search_stmts(exw.body.stmts.iter())?;
            }
            Expr::Yield(exy) => self.search_option_expr(&exy.expr)?,
            _ => {}
        }
        Ok(())
    }

    fn search_option_expr(&mut self, expr: &Option<Box<Expr>>) -> Result<(), DiscoveryErr> {
        if let Some(expr) = &expr {
            self.search_expr(expr)?;
        }
        Ok(())
    }

    fn search_exprs<'a>(
        &mut self,
        exprs: impl Iterator<Item = &'a Expr>,
    ) -> Result<(), DiscoveryErr> {
        for e in exprs {
            self.search_expr(e)?;
        }
        Ok(())
    }

    fn search_impl_item(
        &mut self,
        impl_item: &ImplItem,
        receiver: Option<&RustPath>,
    ) -> Result<(), DiscoveryErr> {
        if let ImplItem::Method(itm) = impl_item {
            if Self::has_attr(&itm.attrs, EXTERN_RUST_FUN) {
                if self.mod_path.is_some() {
                    return Err(DiscoveryErr::FoundExternRustFunWithinMod);
                }
                if let Some(receiver) = receiver {
                    // We have a method which we want to put into the cxx::bridge's
                    // "extern Rust" block.
                    let sig = add_receiver(&itm.sig, receiver.get_final_ident())?;
                    assert!(receiver.len() == 1);
                    self.discoveries.extern_rust_funs.push(RustFun {
                        path: self.deeper_path(&itm.sig.ident),
                        sig,
                        receiver: Some(receiver.get_final_ident().clone()),
                    });
                    self.discoveries.extern_rust_types.push(receiver.clone())
                } else {
                    return Err(DiscoveryErr::FoundExternRustFunOnTypeWithoutClearReceiver);
                }
            }
            for stmt in &itm.block.stmts {
                self.search_stmt(stmt)?
            }
        }
        Ok(())
    }

    fn search_pat(&mut self, pat: &Pat) -> Result<(), DiscoveryErr> {
        match pat {
            Pat::Box(PatBox { pat, .. }) | Pat::Reference(PatReference { pat, .. }) => {
                self.search_pat(pat)
            }
            Pat::Ident(_) | Pat::Lit(_) | Pat::Macro(_) | Pat::Range(_) | Pat::Rest(_) => Ok(()),
            Pat::Or(pator) => {
                for case in &pator.cases {
                    self.search_pat(case)?;
                }
                Ok(())
            }
            Pat::Path(pp) => self.search_path(&pp.path),
            Pat::Slice(PatSlice { elems, .. }) | Pat::Tuple(PatTuple { elems, .. }) => {
                for case in elems {
                    self.search_pat(case)?;
                }
                Ok(())
            }
            Pat::Struct(ps) => {
                self.search_path(&ps.path)?;
                for f in &ps.fields {
                    self.search_pat(&f.pat)?;
                }
                Ok(())
            }
            Pat::TupleStruct(tps) => {
                self.search_path(&tps.path)?;
                for f in &tps.pat.elems {
                    self.search_pat(f)?;
                }
                Ok(())
            }
            Pat::Type(pt) => {
                self.search_pat(&pt.pat)?;
                self.search_type(&pt.ty)
            }
            _ => Ok(()),
        }
    }

    fn search_type(&mut self, ty: &Type) -> Result<(), DiscoveryErr> {
        match ty {
            Type::Array(TypeArray { elem, .. })
            | Type::Group(TypeGroup { elem, .. })
            | Type::Paren(TypeParen { elem, .. })
            | Type::Ptr(TypePtr { elem, .. })
            | Type::Reference(TypeReference { elem, .. })
            | Type::Slice(TypeSlice { elem, .. }) => self.search_type(elem)?,
            Type::BareFn(tf) => {
                for input in &tf.inputs {
                    self.search_type(&input.ty)?;
                }
                self.search_return_type(&tf.output)?;
            }
            Type::ImplTrait(tyit) => {
                for b in &tyit.bounds {
                    if let syn::TypeParamBound::Trait(tyt) = b {
                        self.search_path(&tyt.path)?
                    }
                }
            }
            Type::Infer(_) | Type::Macro(_) | Type::Never(_) => {}
            Type::Path(typ) => self.search_path(&typ.path)?,
            Type::TraitObject(tto) => self.search_type_param_bounds(&tto.bounds)?,
            Type::Tuple(tt) => {
                for e in &tt.elems {
                    self.search_type(e)?
                }
            }
            _ => {}
        }
        Ok(())
    }

    fn search_type_param_bounds(
        &mut self,
        bounds: &Punctuated<TypeParamBound, syn::token::Add>,
    ) -> Result<(), DiscoveryErr> {
        for b in bounds {
            if let syn::TypeParamBound::Trait(tpbt) = b {
                self.search_path(&tpbt.path)?
            }
        }
        Ok(())
    }

    fn search_return_type(&mut self, output: &ReturnType) -> Result<(), DiscoveryErr> {
        if let ReturnType::Type(_, ty) = &output {
            self.search_type(ty)
        } else {
            Ok(())
        }
    }

    fn search_path_arguments(
        &mut self,
        arguments: &syn::PathArguments,
    ) -> Result<(), DiscoveryErr> {
        match arguments {
            syn::PathArguments::None => {}
            syn::PathArguments::AngleBracketed(paab) => {
                for arg in &paab.args {
                    match arg {
                        syn::GenericArgument::Lifetime(_) => {}
                        syn::GenericArgument::Type(ty)
                        | syn::GenericArgument::Binding(Binding { ty, .. }) => {
                            self.search_type(ty)?
                        }
                        syn::GenericArgument::Constraint(c) => {
                            self.search_type_param_bounds(&c.bounds)?
                        }
                        syn::GenericArgument::Const(c) => self.search_expr(c)?,
                    }
                }
            }
            syn::PathArguments::Parenthesized(pas) => {
                self.search_return_type(&pas.output)?;
                for t in &pas.inputs {
                    self.search_type(t)?;
                }
            }
        }
        Ok(())
    }

    fn has_attr(attrs: &[Attribute], attr_name: &str) -> bool {
        attrs.iter().any(|attr| {
            attr.path
                .segments
                .last()
                .map(|seg| seg.ident == attr_name)
                .unwrap_or_default()
        })
    }
}

/// Take a method signature that may be `fn a(&self)`
/// and turn it into `fn a(self: &A)` which is what we will
/// need to specify to cxx.
fn add_receiver(sig: &Signature, receiver: &Ident) -> Result<Signature, DiscoveryErr> {
    let mut sig = sig.clone();
    match sig.inputs.iter_mut().next() {
        Some(first_arg) => match first_arg {
            syn::FnArg::Receiver(Receiver {
                reference: Some(_),
                mutability: Some(_),
                ..
            }) => {
                *first_arg = parse_quote! {
                    self: &mut #receiver
                }
            }
            syn::FnArg::Receiver(Receiver {
                reference: Some(_),
                mutability: None,
                ..
            }) => {
                *first_arg = parse_quote! {
                    self: &#receiver
                }
            }
            syn::FnArg::Receiver(..) => return Err(DiscoveryErr::NonReferenceReceiver),
            syn::FnArg::Typed(_) => {}
        },
        None => return Err(DiscoveryErr::NoParameterOnMethod),
    }
    Ok(sig)
}

#[cfg(test)]
mod tests {
    use quote::{quote, ToTokens};
    use syn::{parse_quote, ImplItemMethod};

    use crate::{ast_discoverer::add_receiver, types::make_ident};

    use super::Discoveries;

    fn assert_cpp_found(discoveries: &Discoveries) {
        assert!(!discoveries.cpp_list.is_empty());
        assert!(discoveries.cpp_list.iter().next().unwrap() == "xxx");
    }

    #[test]
    fn test_mod_plain_call() {
        let mut discoveries = Discoveries::default();
        let itm = parse_quote! {
            mod foo {
                fn bar() {
                    ffi::xxx()
                }
            }
        };
        discoveries.search_item(&itm, None).unwrap();
        assert_cpp_found(&discoveries);
    }

    #[test]
    fn test_plain_call() {
        let mut discoveries = Discoveries::default();
        let itm = parse_quote! {
            fn bar() {
                ffi::xxx()
            }
        };
        discoveries.search_item(&itm, None).unwrap();
        assert_cpp_found(&discoveries);
    }

    #[test]
    fn test_plain_call_with_semi() {
        let mut discoveries = Discoveries::default();
        let itm = parse_quote! {
            fn bar() {
                ffi::xxx();
            }
        };
        discoveries.search_item(&itm, None).unwrap();
        assert_cpp_found(&discoveries);
    }

    #[test]
    fn test_in_ns() {
        let mut discoveries = Discoveries::default();
        let itm = parse_quote! {
            fn bar() {
                ffi::a::b::xxx();
            }
        };
        discoveries.search_item(&itm, None).unwrap();
        assert!(!discoveries.cpp_list.is_empty());
        assert!(discoveries.cpp_list.iter().next().unwrap() == "a::b::xxx");
    }

    #[test]
    fn test_deep_nested_thingy() {
        let mut discoveries = Discoveries::default();
        let itm = parse_quote! {
            fn bar() {
                a + 3 * foo(ffi::xxx());
            }
        };
        discoveries.search_item(&itm, None).unwrap();
        assert_cpp_found(&discoveries);
    }

    #[test]
    fn test_ty_in_let() {
        let mut discoveries = Discoveries::default();
        let itm = parse_quote! {
            fn bar() {
                let foo: ffi::xxx = bar();
            }
        };
        discoveries.search_item(&itm, None).unwrap();
        assert_cpp_found(&discoveries);
    }

    #[test]
    fn test_ty_in_fn() {
        let mut discoveries = Discoveries::default();
        let itm = parse_quote! {
            fn bar(a: &mut ffi::xxx) {
            }
        };
        discoveries.search_item(&itm, None).unwrap();
        assert_cpp_found(&discoveries);
    }

    #[test]
    fn test_ty_in_fn_up() {
        let mut discoveries = Discoveries::default();
        let itm = parse_quote! {
            fn bar(a: cxx::UniquePtr<ffi::xxx>) {
            }
        };
        discoveries.search_item(&itm, None).unwrap();
        assert_cpp_found(&discoveries);
    }

    #[test]
    fn test_extern_rust_fun() {
        let mut discoveries = Discoveries::default();
        let itm = parse_quote! {
            #[autocxx::extern_rust::extern_rust_function]
            fn bar(a: cxx::UniquePtr<ffi::xxx>) {
            }
        };
        discoveries.search_item(&itm, None).unwrap();
        assert!(discoveries.extern_rust_funs.get(0).unwrap().sig.ident == "bar");
    }

    #[test]
    fn test_extern_rust_method() {
        let mut discoveries = Discoveries::default();
        let itm = parse_quote! {
            impl A {
                #[autocxx::extern_rust::extern_rust_function]
                fn bar(&self) {
                }
            }
        };
        discoveries.search_item(&itm, None).unwrap();
        assert!(discoveries.extern_rust_funs.get(0).unwrap().sig.ident == "bar");
    }

    #[test]
    fn test_extern_rust_ty() {
        let mut discoveries = Discoveries::default();
        let itm = parse_quote! {
            #[autocxx::extern_rust::extern_rust_type]
            struct Bar {

            }
        };
        discoveries.search_item(&itm, None).unwrap();
        assert!(
            discoveries
                .extern_rust_types
                .get(0)
                .unwrap()
                .get_final_ident()
                == "Bar"
        );
    }

    #[test]
    fn test_add_receiver() {
        let meth: ImplItemMethod = parse_quote! {
            fn a(&self) {}
        };
        let a = make_ident("A");
        assert_eq!(
            add_receiver(&meth.sig, &a)
                .unwrap()
                .to_token_stream()
                .to_string(),
            quote! { fn a(self: &A) }.to_string()
        );

        let meth: ImplItemMethod = parse_quote! {
            fn a(&mut self, b: u32) -> Foo {}
        };
        assert_eq!(
            add_receiver(&meth.sig, &a)
                .unwrap()
                .to_token_stream()
                .to_string(),
            quote! { fn a(self: &mut A, b: u32) -> Foo }.to_string()
        );

        let meth: ImplItemMethod = parse_quote! {
            fn a(self) {}
        };
        assert!(add_receiver(&meth.sig, &a).is_err());

        let meth: ImplItemMethod = parse_quote! {
            fn a() {}
        };
        assert!(add_receiver(&meth.sig, &a).is_err());
    }
}
