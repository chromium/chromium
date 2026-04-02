//! # Virtue, a sinless derive macro helper
//!
//! ## Goals
//!
//! - Zero dependencies, so fast compile times
//! - No other dependencies needed
//! - Declarative code generation
//! - As much typesystem checking as possible
//! - Build for modern rust: 1.57 and up
//! - Build with popular crates in mind:
//!   - [bincode](https://docs.rs/bincode)
//! - Will always respect semver. Minor releases will never have:
//!   - Breaking API changes
//!   - MSRV changes
//!
//! ## Example
//!
//! First, add this to your Cargo.toml:
//! ```toml
//! [lib]
//! proc-macro = true
//! ```
//!
//! Then instantiate your project with:
//!
//! ```ignore
//! use virtue::prelude::*;
//!
//! #[proc_macro_derive(RetHi)] // change this to change your #[derive(...)] name
//! pub fn derive_ret_hi(input: TokenStream) -> TokenStream {
//!     derive_ret_hi_inner(input).unwrap_or_else(|error| error.into_token_stream())
//! }
//!
//! fn derive_ret_hi_inner(input: TokenStream) -> Result<TokenStream> {
//!     let parse = Parse::new(input)?;
//!     let (mut generator, _attributes, _body) = parse.into_generator();
//!     generator
//!         .generate_impl()
//!         .generate_fn("hi")
//!         .with_self_arg(FnSelfArg::RefSelf)
//!         .with_return_type("&'static str")
//!         .body(|body| {
//!             body.lit_str("hi");
//!             Ok(())
//!         })?;
//!     generator.finish()
//! }
//! ```
//!
//! You can invoke this with
//!
//! ```ignore
//! #[derive(RetHi)]
//! struct Foo;
//!
//! fn main() {
//!     println!("{}", Foo.hi());
//! }
//! ```
//!
//! The generated code is:
//!
//! ```ignore
//! impl Foo {
//!     fn hi(&self) -> &'static str {
//!         "hi"
//!     }
//! }
//! ```
#![warn(missing_docs)]

mod error;

pub mod generate;
pub mod parse;
pub mod utils;

/// Result alias for virtue's errors
pub type Result<T = ()> = std::result::Result<T, Error>;

pub use self::error::Error;

/// Useful includes
pub mod prelude {
    pub use crate::generate::{FnSelfArg, Generator, StreamBuilder};
    pub use crate::parse::{
        AttributeAccess, Body, EnumVariant, Fields, FromAttribute, Parse, UnnamedField,
    };
    pub use crate::{Error, Result};

    #[cfg(any(test, feature = "proc-macro2"))]
    pub use proc_macro2::*;

    #[cfg(not(any(test, feature = "proc-macro2")))]
    extern crate proc_macro;
    #[cfg(not(any(test, feature = "proc-macro2")))]
    pub use proc_macro::*;
}

#[cfg(test)]
pub(crate) fn token_stream(
    s: &str,
) -> std::iter::Peekable<impl Iterator<Item = proc_macro2::TokenTree>> {
    use std::str::FromStr;

    let stream = proc_macro2::TokenStream::from_str(s)
        .unwrap_or_else(|e| panic!("Could not parse code: {:?}\n{:?}", s, e));
    stream.into_iter().peekable()
}
