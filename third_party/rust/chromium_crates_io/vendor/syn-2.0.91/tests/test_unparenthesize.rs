#![cfg(not(miri))]
#![allow(
    clippy::manual_assert,
    clippy::needless_lifetimes,
    clippy::uninlined_format_args
)]

use quote::ToTokens as _;
use std::fs;
use std::mem;
use std::panic;
use std::path::Path;
use std::sync::atomic::{AtomicUsize, Ordering};
use syn::visit_mut::{self, VisitMut};
use syn::{Expr, Generics, LifetimeParam, TypeParam};

#[macro_use]
mod macros;

mod repo;

#[test]
fn test_unparenthesize() {
    repo::rayon_init();
    repo::clone_rust();

    let failed = AtomicUsize::new(0);

    repo::for_each_rust_file(|path| test(path, &failed));

    let failed = failed.into_inner();
    if failed > 0 {
        panic!("{} failures", failed);
    }
}

struct FlattenParens;

impl VisitMut for FlattenParens {
    fn visit_expr_mut(&mut self, e: &mut Expr) {
        while let Expr::Paren(paren) = e {
            *e = mem::replace(&mut *paren.expr, Expr::PLACEHOLDER);
        }
        visit_mut::visit_expr_mut(self, e);
    }
}

struct AsIfPrinted;

impl VisitMut for AsIfPrinted {
    fn visit_generics_mut(&mut self, generics: &mut Generics) {
        if generics.params.is_empty() {
            generics.lt_token = None;
            generics.gt_token = None;
        }
        if let Some(where_clause) = &generics.where_clause {
            if where_clause.predicates.is_empty() {
                generics.where_clause = None;
            }
        }
        visit_mut::visit_generics_mut(self, generics);
    }

    fn visit_lifetime_param_mut(&mut self, param: &mut LifetimeParam) {
        if param.bounds.is_empty() {
            param.colon_token = None;
        }
        visit_mut::visit_lifetime_param_mut(self, param);
    }

    fn visit_type_param_mut(&mut self, param: &mut TypeParam) {
        if param.bounds.is_empty() {
            param.colon_token = None;
        }
        visit_mut::visit_type_param_mut(self, param);
    }
}

fn test(path: &Path, failed: &AtomicUsize) {
    let content = fs::read_to_string(path).unwrap();

    match panic::catch_unwind(|| -> syn::Result<()> {
        let mut before = syn::parse_file(&content)?;
        before.shebang = None;
        FlattenParens.visit_file_mut(&mut before);
        let printed = before.to_token_stream();
        let mut after = syn::parse2::<syn::File>(printed.clone())?;
        FlattenParens.visit_file_mut(&mut after);
        // Normalize features that we expect Syn not to print.
        AsIfPrinted.visit_file_mut(&mut before);
        if before != after {
            errorf!("=== {}\n", path.display());
            if failed.fetch_add(1, Ordering::Relaxed) == 0 {
                errorf!("BEFORE:\n{:#?}\nAFTER:\n{:#?}\n", before, after);
            }
        }
        Ok(())
    }) {
        Err(_) => {
            errorf!("=== {}: syn panic\n", path.display());
            failed.fetch_add(1, Ordering::Relaxed);
        }
        Ok(Err(msg)) => {
            errorf!("=== {}: syn failed to parse\n{:?}\n", path.display(), msg);
            failed.fetch_add(1, Ordering::Relaxed);
        }
        Ok(Ok(())) => {}
    }
}
