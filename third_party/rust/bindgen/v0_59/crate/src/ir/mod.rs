//! The ir module defines bindgen's intermediate representation.
//!
//! Parsing C/C++ generates the IR, while code generation outputs Rust code from
//! the IR.

pub mod analysis;
pub mod annotations;
pub mod comment;
pub mod comp;
pub mod context;
pub mod derive;
pub mod dot;
pub mod enum_ty;
pub mod function;
pub mod int;
pub mod item;
pub mod item_kind;
pub mod layout;
pub mod module;
pub mod objc;
pub mod template;
pub mod traversal;
pub mod ty;
pub mod var;
