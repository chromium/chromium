//! `const fn` equivalents of `NonZero*` methods.

/// `const fn`s for comparing `NonZero*` types for equality and ordering.
#[cfg(feature = "cmp")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "cmp")))]
pub mod cmp;
