//! The CXX code generator for constructing and compiling C++ code.
//!
//! This is intended as a mechanism for embedding the `cxx` crate into
//! higher-level code generators. See [dtolnay/cxx#235] and
//! [https://github.com/google/autocxx].
//!
//! [dtolnay/cxx#235]: https://github.com/dtolnay/cxx/issues/235
//! [https://github.com/google/autocxx]: https://github.com/google/autocxx

#![deny(missing_docs)]
#![allow(dead_code)]
#![allow(
    clippy::cast_sign_loss,
    clippy::default_trait_access,
    clippy::enum_glob_use,
    clippy::if_same_then_else,
    clippy::inherent_to_string,
    clippy::items_after_statements,
    clippy::match_bool,
    clippy::match_on_vec_items,
    clippy::match_same_arms,
    clippy::missing_errors_doc,
    clippy::module_name_repetitions,
    clippy::needless_pass_by_value,
    clippy::new_without_default,
    clippy::nonminimal_bool,
    clippy::option_if_let_else,
    clippy::or_fun_call,
    clippy::redundant_else,
    clippy::shadow_unrelated,
    clippy::similar_names,
    clippy::single_match_else,
    clippy::struct_excessive_bools,
    clippy::too_many_arguments,
    clippy::too_many_lines,
    clippy::toplevel_ref_arg,
    // clippy bug: https://github.com/rust-lang/rust-clippy/issues/6983
    clippy::wrong_self_convention
)]

mod error;
mod gen;
mod syntax;

pub use crate::error::Error;
pub use crate::gen::include::{Include, HEADER};
pub use crate::gen::{GeneratedCode, Opt};
pub use crate::syntax::IncludeKind;
use proc_macro2::TokenStream;

/// Generate C++ bindings code from a Rust token stream. This should be a Rust
/// token stream which somewhere contains a `#[cxx::bridge] mod {}`.
pub fn generate_header_and_cc(rust_source: TokenStream, opt: &Opt) -> Result<GeneratedCode, Error> {
    let syntax = syn::parse2(rust_source)
        .map_err(crate::gen::Error::from)
        .map_err(Error::from)?;
    gen::generate(syntax, opt).map_err(Error::from)
}
