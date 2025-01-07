//! This crate is an implementation detail of the `cxx` and `cxx-build` crates,
//! and does not expose any public API.

#![allow(clippy::let_and_return)]

mod r#impl;

#[doc(hidden)]
pub use r#impl::*;
