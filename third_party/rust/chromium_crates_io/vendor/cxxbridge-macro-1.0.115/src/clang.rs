use serde_derive::{Deserialize, Serialize};

pub(crate) type Node = clang_ast::Node<Clang>;

#[derive(Deserialize, Serialize)]
pub(crate) enum Clang {
    NamespaceDecl(NamespaceDecl),
    EnumDecl(EnumDecl),
    EnumConstantDecl(EnumConstantDecl),
    ImplicitCastExpr,
    ConstantExpr(ConstantExpr),
    Unknown,
}

#[derive(Deserialize, Serialize)]
pub(crate) struct NamespaceDecl {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub name: Option<Box<str>>,
}

#[derive(Deserialize, Serialize)]
pub(crate) struct EnumDecl {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub name: Option<Box<str>>,
    #[serde(
        rename = "fixedUnderlyingType",
        skip_serializing_if = "Option::is_none"
    )]
    pub fixed_underlying_type: Option<Type>,
}

#[derive(Deserialize, Serialize)]
pub(crate) struct EnumConstantDecl {
    pub name: Box<str>,
}

#[derive(Deserialize, Serialize)]
pub(crate) struct ConstantExpr {
    pub value: Box<str>,
}

#[derive(Deserialize, Serialize)]
pub(crate) struct Type {
    #[serde(rename = "qualType")]
    pub qual_type: Box<str>,
    #[serde(rename = "desugaredQualType", skip_serializing_if = "Option::is_none")]
    pub desugared_qual_type: Option<Box<str>>,
}

#[cfg(all(test, target_pointer_width = "64"))]
const _: [(); core::mem::size_of::<Node>()] = [(); 88];
