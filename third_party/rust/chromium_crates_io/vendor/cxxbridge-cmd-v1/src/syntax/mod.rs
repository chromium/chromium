// Functionality that is shared between the cxxbridge macro and the cmd.

pub(crate) mod atom;
pub(crate) mod attrs;
pub(crate) mod cfg;
pub(crate) mod check;
pub(crate) mod derive;
mod discriminant;
mod doc;
pub(crate) mod error;
pub(crate) mod file;
pub(crate) mod ident;
mod impls;
mod improper;
pub(crate) mod instantiate;
pub(crate) mod mangle;
pub(crate) mod map;
mod names;
pub(crate) mod namespace;
mod parse;
mod pod;
pub(crate) mod qualified;
pub(crate) mod report;
pub(crate) mod resolve;
pub(crate) mod set;
pub(crate) mod symbol;
mod tokens;
mod toposort;
pub(crate) mod trivial;
pub(crate) mod types;
mod visit;

use self::attrs::OtherAttrs;
use self::cfg::CfgExpr;
use self::namespace::Namespace;
use self::parse::kw;
use self::symbol::Symbol;
use proc_macro2::{Ident, Span};
use syn::punctuated::Punctuated;
use syn::token::{Brace, Bracket, Paren};
use syn::{Attribute, Expr, Generics, Lifetime, LitInt, Token, Type as RustType};

pub(crate) use self::atom::Atom;
pub(crate) use self::derive::{Derive, Trait};
pub(crate) use self::discriminant::Discriminant;
pub(crate) use self::doc::Doc;
pub(crate) use self::names::ForeignName;
pub(crate) use self::parse::parse_items;
pub(crate) use self::types::Types;

pub(crate) enum Api {
    #[allow(dead_code)] // only used by cxx-build, not cxxbridge-macro
    Include(Include),
    Struct(Struct),
    Enum(Enum),
    CxxType(ExternType),
    CxxFunction(ExternFn),
    RustType(ExternType),
    RustFunction(ExternFn),
    TypeAlias(TypeAlias),
    Impl(Impl),
}

pub(crate) struct Include {
    pub cfg: CfgExpr,
    pub path: String,
    pub kind: IncludeKind,
    #[allow(dead_code)] // only used by cxx-build, not cxxbridge-macro
    pub begin_span: Span,
    #[allow(dead_code)] // only used by cxx-build, not cxxbridge-macro
    pub end_span: Span,
}

/// Whether to emit `#include "path"` or `#include <path>`.
#[derive(Copy, Clone, PartialEq, Debug)]
pub enum IncludeKind {
    /// `#include "quoted/path/to"`
    Quoted,
    /// `#include <bracketed/path/to>`
    Bracketed,
}

pub(crate) struct ExternType {
    #[allow(dead_code)] // only used by cxx-build, not cxxbridge-macro
    pub cfg: CfgExpr,
    pub lang: Lang,
    pub doc: Doc,
    pub derives: Vec<Derive>,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub attrs: OtherAttrs,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub visibility: Token![pub],
    pub type_token: Token![type],
    pub name: Pair,
    pub generics: Lifetimes,
    #[allow(dead_code)]
    pub colon_token: Option<Token![:]>,
    pub bounds: Vec<Derive>,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub semi_token: Token![;],
    pub trusted: bool,
}

pub(crate) struct Struct {
    #[allow(dead_code)] // only used by cxx-build, not cxxbridge-macro
    pub cfg: CfgExpr,
    pub doc: Doc,
    pub derives: Vec<Derive>,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub attrs: OtherAttrs,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub visibility: Token![pub],
    pub struct_token: Token![struct],
    pub name: Pair,
    pub generics: Lifetimes,
    pub brace_token: Brace,
    pub fields: Vec<Var>,
}

pub(crate) struct Enum {
    #[allow(dead_code)] // only used by cxx-build, not cxxbridge-macro
    pub cfg: CfgExpr,
    pub doc: Doc,
    pub derives: Vec<Derive>,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub attrs: OtherAttrs,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub visibility: Token![pub],
    pub enum_token: Token![enum],
    pub name: Pair,
    pub generics: Lifetimes,
    pub brace_token: Brace,
    pub variants: Vec<Variant>,
    pub variants_from_header: bool,
    #[allow(dead_code)]
    pub variants_from_header_attr: Option<Attribute>,
    pub repr: EnumRepr,
    pub explicit_repr: bool,
}

pub(crate) enum EnumRepr {
    Native {
        atom: Atom,
        repr_type: Type,
    },
    #[cfg(feature = "experimental-enum-variants-from-header")]
    Foreign {
        rust_type: syn::Path,
    },
}

pub(crate) struct ExternFn {
    #[allow(dead_code)] // only used by cxx-build, not cxxbridge-macro
    pub cfg: CfgExpr,
    pub lang: Lang,
    pub doc: Doc,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub attrs: OtherAttrs,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub visibility: Token![pub],
    pub name: Pair,
    pub sig: Signature,
    pub semi_token: Token![;],
    pub trusted: bool,
}

pub(crate) struct TypeAlias {
    #[allow(dead_code)] // only used by cxx-build, not cxxbridge-macro
    pub cfg: CfgExpr,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub doc: Doc,
    pub derives: Vec<Derive>,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub attrs: OtherAttrs,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub visibility: Token![pub],
    pub type_token: Token![type],
    pub name: Pair,
    pub generics: Lifetimes,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub eq_token: Token![=],
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub ty: RustType,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub semi_token: Token![;],
}

pub(crate) struct Impl {
    #[allow(dead_code)] // only used by cxx-build, not cxxbridge-macro
    pub cfg: CfgExpr,
    pub impl_token: Token![impl],
    pub impl_generics: Lifetimes,
    #[allow(dead_code)]
    pub negative: bool,
    pub ty: Type,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub ty_generics: Lifetimes,
    pub brace_token: Brace,
    pub negative_token: Option<Token![!]>,
}

#[derive(Clone, Default)]
pub(crate) struct Lifetimes {
    pub lt_token: Option<Token![<]>,
    pub lifetimes: Punctuated<Lifetime, Token![,]>,
    pub gt_token: Option<Token![>]>,
}

pub(crate) struct Signature {
    pub asyncness: Option<Token![async]>,
    pub unsafety: Option<Token![unsafe]>,
    pub fn_token: Token![fn],
    pub generics: Generics,
    pub receiver: Option<Receiver>,
    pub args: Punctuated<Var, Token![,]>,
    pub ret: Option<Type>,
    pub throws: bool,
    pub paren_token: Paren,
    pub throws_tokens: Option<(kw::Result, Token![<], Token![>])>,
}

pub(crate) struct Var {
    #[allow(dead_code)] // only used by cxx-build, not cxxbridge-macro
    pub cfg: CfgExpr,
    pub doc: Doc,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub attrs: OtherAttrs,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub visibility: Token![pub],
    pub name: Pair,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub colon_token: Token![:],
    pub ty: Type,
}

pub(crate) struct Receiver {
    pub pinned: bool,
    pub ampersand: Token![&],
    pub lifetime: Option<Lifetime>,
    pub mutable: bool,
    pub var: Token![self],
    pub ty: NamedType,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub colon_token: Token![:],
    pub shorthand: bool,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub pin_tokens: Option<(kw::Pin, Token![<], Token![>])>,
    pub mutability: Option<Token![mut]>,
}

pub(crate) struct Variant {
    #[allow(dead_code)] // only used by cxx-build, not cxxbridge-macro
    pub cfg: CfgExpr,
    pub doc: Doc,
    #[allow(dead_code)] // only used by cxxbridge-macro, not cxx-build
    pub attrs: OtherAttrs,
    pub name: Pair,
    pub discriminant: Discriminant,
    #[allow(dead_code)]
    pub expr: Option<Expr>,
}

pub(crate) enum Type {
    Ident(NamedType),
    RustBox(Box<Ty1>),
    RustVec(Box<Ty1>),
    UniquePtr(Box<Ty1>),
    SharedPtr(Box<Ty1>),
    WeakPtr(Box<Ty1>),
    Ref(Box<Ref>),
    Ptr(Box<Ptr>),
    Str(Box<Ref>),
    CxxVector(Box<Ty1>),
    Fn(Box<Signature>),
    Void(Span),
    SliceRef(Box<SliceRef>),
    Array(Box<Array>),
}

pub(crate) struct Ty1 {
    pub name: Ident,
    pub langle: Token![<],
    pub inner: Type,
    pub rangle: Token![>],
}

pub(crate) struct Ref {
    pub pinned: bool,
    pub ampersand: Token![&],
    pub lifetime: Option<Lifetime>,
    pub mutable: bool,
    pub inner: Type,
    pub pin_tokens: Option<(kw::Pin, Token![<], Token![>])>,
    pub mutability: Option<Token![mut]>,
}

pub(crate) struct Ptr {
    pub star: Token![*],
    pub mutable: bool,
    pub inner: Type,
    pub mutability: Option<Token![mut]>,
    pub constness: Option<Token![const]>,
}

pub(crate) struct SliceRef {
    pub ampersand: Token![&],
    pub lifetime: Option<Lifetime>,
    pub mutable: bool,
    pub bracket: Bracket,
    pub inner: Type,
    pub mutability: Option<Token![mut]>,
}

pub(crate) struct Array {
    pub bracket: Bracket,
    pub inner: Type,
    pub semi_token: Token![;],
    pub len: usize,
    pub len_token: LitInt,
}

#[derive(Copy, Clone, PartialEq)]
pub(crate) enum Lang {
    Cxx,
    Rust,
}

// An association of a defined Rust name with a fully resolved, namespace
// qualified C++ name.
#[derive(Clone)]
pub(crate) struct Pair {
    pub namespace: Namespace,
    pub cxx: ForeignName,
    pub rust: Ident,
}

// Wrapper for a type which needs to be resolved before it can be printed in
// C++.
#[derive(PartialEq, Eq, Hash)]
pub(crate) struct NamedType {
    pub rust: Ident,
    pub generics: Lifetimes,
}
