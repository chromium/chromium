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

pub use mapper::map_ast; // utility function

#[cfg(feature = "ahash")]
pub type RandomState = ahash::RandomState;

#[cfg(not(feature = "ahash"))]
pub type RandomState = std::collections::hash_map::RandomState;

pub type HashMap<K, V> = std::collections::HashMap<K, V, RandomState>;
pub type HashSet<K> = std::collections::HashSet<K, RandomState>;

pub mod raw {
    pub use super::ast::ExprSet;
    pub use super::deriv::DerivCache;
    pub use super::hashcons::VecHashCons;
    pub use super::nextbyte::NextByteCache;
    pub use super::relevance::RelevanceCache;
}
