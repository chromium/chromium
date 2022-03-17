// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::collections::HashSet;

use autocxx_parser::{
    directives::{EXTERN_RUST_FUN, EXTERN_RUST_TYPE},
    RustFun, RustPath,
};
use itertools::Itertools;
use proc_macro2::Ident;
use syn::{
    punctuated::Punctuated, Attribute, Binding, Expr, ExprAssign, ExprAssignOp, ExprAwait,
    ExprBinary, ExprBox, ExprBreak, ExprCast, ExprField, ExprGroup, ExprLet, ExprParen,
    ExprReference, ExprTry, ExprType, ExprUnary, ImplItem, Item, ItemEnum, ItemStruct, Pat, PatBox,
    PatReference, PatSlice, PatTuple, Path, ReturnType, Stmt, TraitItem, Type, TypeArray,
    TypeGroup, TypeParamBound, TypeParen, TypePtr, TypeReference, TypeSlice,
};

#[derive(Default)]
pub(super) struct Discoveries {
    pub(super) cpp_list: HashSet<String>,
    pub(super) extern_rust_funs: Vec<RustFun>,
    pub(super) extern_rust_types: Vec<RustPath>,
}

impl Discoveries {
    pub(super) fn search_item(&mut self, item: &Item, mod_path: Option<RustPath>) {
        let mut this_mod = PerModDiscoveries {
            discoveries: self,
            mod_path,
        };
        this_mod.search_item(item);
    }

    pub(crate) fn is_empty(&self) -> bool {
        self.cpp_list.is_empty()
            && self.extern_rust_funs.is_empty()
            && self.extern_rust_types.is_empty()
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

    fn search_item(&mut self, item: &Item) {
        match item {
            Item::Fn(fun) => {
                for stmt in &fun.block.stmts {
                    self.search_stmt(stmt)
                }
                self.search_return_type(&fun.sig.output);
                for i in &fun.sig.inputs {
                    match i {
                        syn::FnArg::Receiver(_) => {}
                        syn::FnArg::Typed(pt) => {
                            self.search_pat(&pt.pat);
                            self.search_type(&pt.ty);
                        }
                    }
                }
                if Self::has_attr(&fun.attrs, EXTERN_RUST_FUN) {
                    self.discoveries.extern_rust_funs.push(RustFun {
                        path: self.deeper_path(&fun.sig.ident),
                        sig: fun.sig.clone(),
                    });
                }
            }
            Item::Impl(imp) => {
                for item in &imp.items {
                    self.search_impl_item(item)
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
                        new_mod.search_item(item)
                    }
                }
            }
            Item::Trait(tr) => {
                for item in &tr.items {
                    self.search_trait_item(item)
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
    }

    fn search_path(&mut self, path: &Path) {
        let mut seg_iter = path.segments.iter();
        if let Some(first_seg) = seg_iter.next() {
            if first_seg.ident == "ffi" {
                self.discoveries
                    .cpp_list
                    .insert(seg_iter.map(|seg| seg.ident.to_string()).join("::"));
            }
        }
        for seg in path.segments.iter() {
            self.search_path_arguments(&seg.arguments);
        }
    }

    fn search_trait_item(&mut self, itm: &TraitItem) {
        if let TraitItem::Method(itm) = itm {
            if let Some(block) = &itm.default {
                self.search_stmts(block.stmts.iter())
            }
        }
    }

    fn search_stmts<'a>(&mut self, stmts: impl Iterator<Item = &'a Stmt>) {
        for stmt in stmts {
            self.search_stmt(stmt)
        }
    }

    fn search_stmt(&mut self, stmt: &Stmt) {
        match stmt {
            Stmt::Local(lcl) => {
                if let Some((_, expr)) = &lcl.init {
                    self.search_expr(expr)
                }
                self.search_pat(&lcl.pat);
            }
            Stmt::Item(itm) => self.search_item(itm),
            Stmt::Expr(exp) | Stmt::Semi(exp, _) => self.search_expr(exp),
        }
    }

    fn search_expr(&mut self, expr: &Expr) {
        match expr {
            Expr::Path(exp) => {
                self.search_path(&exp.path);
            }
            Expr::Macro(_) => {}
            Expr::Array(array) => self.search_exprs(array.elems.iter()),
            Expr::Assign(ExprAssign { left, right, .. })
            | Expr::AssignOp(ExprAssignOp { left, right, .. })
            | Expr::Binary(ExprBinary { left, right, .. }) => {
                self.search_expr(left);
                self.search_expr(right);
            }
            Expr::Async(ass) => self.search_stmts(ass.block.stmts.iter()),
            Expr::Await(ExprAwait { base, .. }) | Expr::Field(ExprField { base, .. }) => {
                self.search_expr(base)
            }
            Expr::Block(blck) => self.search_stmts(blck.block.stmts.iter()),
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
            | Expr::Unary(ExprUnary { expr, .. }) => self.search_expr(expr),
            Expr::Call(exc) => {
                self.search_expr(&exc.func);
                self.search_exprs(exc.args.iter());
            }
            Expr::Closure(cls) => self.search_expr(&cls.body),
            Expr::Continue(_)
            | Expr::Lit(_)
            | Expr::Break(ExprBreak { expr: None, .. })
            | Expr::Verbatim(_) => {}
            Expr::ForLoop(fl) => {
                self.search_expr(&fl.expr);
                self.search_stmts(fl.body.stmts.iter());
            }
            Expr::If(exif) => {
                self.search_expr(&exif.cond);
                self.search_stmts(exif.then_branch.stmts.iter());
                if let Some((_, else_branch)) = &exif.else_branch {
                    self.search_expr(else_branch);
                }
            }
            Expr::Index(exidx) => {
                self.search_expr(&exidx.expr);
                self.search_expr(&exidx.index);
            }
            Expr::Let(ExprLet { expr, pat, .. }) => {
                self.search_expr(expr);
                self.search_pat(pat);
            }
            Expr::Loop(exloo) => self.search_stmts(exloo.body.stmts.iter()),
            Expr::Match(exm) => {
                self.search_expr(&exm.expr);
                for a in &exm.arms {
                    self.search_expr(&a.body);
                    if let Some((_, guard)) = &a.guard {
                        self.search_expr(guard);
                    }
                }
            }
            Expr::MethodCall(mtc) => {
                self.search_expr(&mtc.receiver);
                self.search_exprs(mtc.args.iter());
            }
            Expr::Range(exr) => {
                self.search_option_expr(&exr.from);
                self.search_option_expr(&exr.to);
            }
            Expr::Repeat(exr) => {
                self.search_expr(&exr.expr);
                self.search_expr(&exr.len);
            }
            Expr::Return(exret) => {
                if let Some(expr) = &exret.expr {
                    self.search_expr(expr);
                }
            }
            Expr::Struct(exst) => {
                for f in &exst.fields {
                    self.search_expr(&f.expr);
                }
                self.search_option_expr(&exst.rest);
            }
            Expr::TryBlock(extb) => self.search_stmts(extb.block.stmts.iter()),
            Expr::Tuple(ext) => self.search_exprs(ext.elems.iter()),
            Expr::Unsafe(exs) => self.search_stmts(exs.block.stmts.iter()),
            Expr::While(exw) => {
                self.search_expr(&exw.cond);
                self.search_stmts(exw.body.stmts.iter());
            }
            Expr::Yield(exy) => self.search_option_expr(&exy.expr),
            Expr::__TestExhaustive(_) => {}
        }
    }

    fn search_option_expr(&mut self, expr: &Option<Box<Expr>>) {
        if let Some(expr) = &expr {
            self.search_expr(expr);
        }
    }

    fn search_exprs<'a>(&mut self, exprs: impl Iterator<Item = &'a Expr>) {
        for e in exprs {
            self.search_expr(e);
        }
    }

    fn search_impl_item(&mut self, impl_item: &ImplItem) {
        if let ImplItem::Method(itm) = impl_item {
            for stmt in &itm.block.stmts {
                self.search_stmt(stmt)
            }
        }
    }

    fn search_pat(&mut self, pat: &Pat) {
        match pat {
            Pat::Box(PatBox { pat, .. }) | Pat::Reference(PatReference { pat, .. }) => {
                self.search_pat(pat)
            }
            Pat::Ident(_) | Pat::Lit(_) | Pat::Macro(_) | Pat::Range(_) | Pat::Rest(_) => {}
            Pat::Or(pator) => {
                for case in &pator.cases {
                    self.search_pat(case);
                }
            }
            Pat::Path(pp) => self.search_path(&pp.path),
            Pat::Slice(PatSlice { elems, .. }) | Pat::Tuple(PatTuple { elems, .. }) => {
                for case in elems {
                    self.search_pat(case);
                }
            }
            Pat::Struct(ps) => {
                self.search_path(&ps.path);
                for f in &ps.fields {
                    self.search_pat(&f.pat);
                }
            }
            Pat::TupleStruct(tps) => {
                self.search_path(&tps.path);
                for f in &tps.pat.elems {
                    self.search_pat(f);
                }
            }
            Pat::Type(pt) => {
                self.search_pat(&pt.pat);
                self.search_type(&pt.ty);
            }
            _ => {}
        }
    }

    fn search_type(&mut self, ty: &Type) {
        match ty {
            Type::Array(TypeArray { elem, .. })
            | Type::Group(TypeGroup { elem, .. })
            | Type::Paren(TypeParen { elem, .. })
            | Type::Ptr(TypePtr { elem, .. })
            | Type::Reference(TypeReference { elem, .. })
            | Type::Slice(TypeSlice { elem, .. }) => self.search_type(elem),
            Type::BareFn(tf) => {
                for input in &tf.inputs {
                    self.search_type(&input.ty);
                }
                self.search_return_type(&tf.output);
            }
            Type::ImplTrait(tyit) => {
                for b in &tyit.bounds {
                    if let syn::TypeParamBound::Trait(tyt) = b {
                        self.search_path(&tyt.path)
                    }
                }
            }
            Type::Infer(_) | Type::Macro(_) | Type::Never(_) => {}
            Type::Path(typ) => self.search_path(&typ.path),
            Type::TraitObject(tto) => self.search_type_param_bounds(&tto.bounds),
            Type::Tuple(tt) => {
                for e in &tt.elems {
                    self.search_type(e)
                }
            }
            _ => {}
        }
    }

    fn search_type_param_bounds(&mut self, bounds: &Punctuated<TypeParamBound, syn::token::Add>) {
        for b in bounds {
            if let syn::TypeParamBound::Trait(tpbt) = b {
                self.search_path(&tpbt.path)
            }
        }
    }

    fn search_return_type(&mut self, output: &ReturnType) {
        if let ReturnType::Type(_, ty) = &output {
            self.search_type(ty)
        }
    }

    fn search_path_arguments(&mut self, arguments: &syn::PathArguments) {
        match arguments {
            syn::PathArguments::None => {}
            syn::PathArguments::AngleBracketed(paab) => {
                for arg in &paab.args {
                    match arg {
                        syn::GenericArgument::Lifetime(_) => {}
                        syn::GenericArgument::Type(ty)
                        | syn::GenericArgument::Binding(Binding { ty, .. }) => self.search_type(ty),
                        syn::GenericArgument::Constraint(c) => {
                            self.search_type_param_bounds(&c.bounds)
                        }
                        syn::GenericArgument::Const(c) => self.search_expr(c),
                    }
                }
            }
            syn::PathArguments::Parenthesized(pas) => {
                self.search_return_type(&pas.output);
                for t in &pas.inputs {
                    self.search_type(t);
                }
            }
        }
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

#[cfg(test)]
mod tests {
    use syn::parse_quote;

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
        discoveries.search_item(&itm, None);
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
        discoveries.search_item(&itm, None);
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
        discoveries.search_item(&itm, None);
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
        discoveries.search_item(&itm, None);
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
        discoveries.search_item(&itm, None);
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
        discoveries.search_item(&itm, None);
        assert_cpp_found(&discoveries);
    }

    #[test]
    fn test_ty_in_fn() {
        let mut discoveries = Discoveries::default();
        let itm = parse_quote! {
            fn bar(a: &mut ffi::xxx) {
            }
        };
        discoveries.search_item(&itm, None);
        assert_cpp_found(&discoveries);
    }

    #[test]
    fn test_ty_in_fn_up() {
        let mut discoveries = Discoveries::default();
        let itm = parse_quote! {
            fn bar(a: cxx::UniquePtr<ffi::xxx>) {
            }
        };
        discoveries.search_item(&itm, None);
        assert_cpp_found(&discoveries);
    }

    #[test]
    fn test_extern_rust_fun() {
        let mut discoveries = Discoveries::default();
        let itm = parse_quote! {
            #[autocxx::extern_rust::extern_rust_fun]
            fn bar(a: cxx::UniquePtr<ffi::xxx>) {
            }
        };
        discoveries.search_item(&itm, None);
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
        discoveries.search_item(&itm, None);
        assert!(
            discoveries
                .extern_rust_types
                .get(0)
                .unwrap()
                .get_final_ident()
                == "Bar"
        );
    }
}
