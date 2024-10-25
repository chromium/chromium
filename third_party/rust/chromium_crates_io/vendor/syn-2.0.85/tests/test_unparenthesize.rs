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
use syn::Expr;

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

fn test(path: &Path, failed: &AtomicUsize) {
    let content = fs::read_to_string(path).unwrap();

    match panic::catch_unwind(|| -> syn::Result<()> {
        let mut syntax_tree = syn::parse_file(&content)?;
        FlattenParens.visit_file_mut(&mut syntax_tree);
        syn::parse2::<syn::File>(syntax_tree.to_token_stream())?;
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
