//! `const fn` equivalents of primitive type methods.

/// `const fn`s for comparing primitive types for equality and ordering.
#[cfg(feature = "cmp")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "cmp")))]
pub mod cmp;

#[cfg(feature = "parsing_no_proc")]
mod parse;

#[cfg(feature = "parsing_no_proc")]
pub use parse::*;
