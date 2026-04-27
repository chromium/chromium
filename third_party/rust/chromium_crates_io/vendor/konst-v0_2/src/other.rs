//! `const fn` equivalents of methods from miscelaneous standard library types.

/// `const fn`s for comparing miscelaneous standard library types for equality and ordering.
#[cfg(feature = "cmp")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "cmp")))]
pub mod cmp;
