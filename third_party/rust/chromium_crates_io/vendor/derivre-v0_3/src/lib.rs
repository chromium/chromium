//! A derivative-based regular expression engine.
//!
//! The primary entry point is [`Regex`] for simple patterns, or [`RegexBuilder`]
//! for constructing regexes programmatically from [`RegexAst`] trees.
//!
//! **Anchored matching.** The engine only checks whether a regex matches the
//! input from the beginning — there is an implied `\A` anchor. It does not
//! search for matches within the input.
//!
//! **Lookahead.** A single look-ahead at the end of the regex is supported
//! via the `A(?P<stop>B)` syntax. See [`Regex::lookahead_len`] for details.
//!
//! # References
//!
//! - [Regular-expression derivatives reexamined (Owens et al.)](https://www.khoury.northeastern.edu/home/turon/re-deriv.pdf)
//! - [Derivative Based Nonbacktracking Real-World Regex Matching with Backtracking Semantics](https://www.microsoft.com/en-us/research/uploads/prod/2023/04/pldi23main-p249-final.pdf)
//! - [Derivative Based Extended Regular Expression Matching Supporting Intersection, Complement and Lookarounds](https://arxiv.org/abs/2309.14401)

mod deriv;
mod hashcons;
mod nextbyte;

mod ast;
mod bytecompress;
mod mapper;
mod pp;
mod regex;
mod regexbuilder;
mod relevance;
mod simplify;
mod syntax;

pub use ast::{ExprRef, NextByte};
pub use regex::{AlphabetInfo, Regex, StateID};

pub use regexbuilder::{JsonQuoteOptions, RegexAst, RegexBuilder};

pub use mapper::map_ast;

/// The hash random state used throughout the crate.
///
/// When the `ahash` feature is enabled (the default), this is
/// [`ahash::RandomState`]; otherwise it falls back to the standard library's
/// [`std::collections::hash_map::RandomState`].
#[cfg(feature = "ahash")]
pub type RandomState = ahash::RandomState;

/// The hash random state used throughout the crate.
///
/// When the `ahash` feature is enabled (the default), this is
/// [`ahash::RandomState`]; otherwise it falls back to the standard library's
/// [`std::collections::hash_map::RandomState`].
#[cfg(not(feature = "ahash"))]
pub type RandomState = std::collections::hash_map::RandomState;

/// A [`std::collections::HashMap`] using the crate's [`RandomState`] hasher.
pub type HashMap<K, V> = std::collections::HashMap<K, V, RandomState>;
/// A [`std::collections::HashSet`] using the crate's [`RandomState`] hasher.
pub type HashSet<K> = std::collections::HashSet<K, RandomState>;

/// Low-level building blocks for advanced use cases.
///
/// Most users should prefer the high-level [`Regex`] and [`RegexBuilder`] APIs.
/// The types in this module expose the internal expression set, derivative
/// cache, and other structures needed to build custom matching pipelines.
pub mod raw {
    pub use super::ast::ExprSet;
    pub use super::deriv::DerivCache;
    pub use super::hashcons::VecHashCons;
    pub use super::nextbyte::NextByteCache;
    pub use super::relevance::RelevanceCache;
}

pub const VERSION: &str = concat!("derivre@", env!("CARGO_PKG_VERSION"));
