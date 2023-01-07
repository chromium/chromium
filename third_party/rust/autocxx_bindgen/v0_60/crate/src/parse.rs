//! Common traits and types related to parsing our IR from Clang cursors.

use crate::clang;
use crate::ir::context::{BindgenContext, ItemId, TypeId};
use crate::ir::ty::TypeKind;

/// Not so much an error in the traditional sense, but a control flow message
/// when walking over Clang's AST with a cursor.
#[derive(Debug)]
pub enum ParseError {
    /// Recurse down the current AST node's children.
    Recurse,
    /// Continue on to the next sibling AST node, or back up to the parent's
    /// siblings if we've exhausted all of this node's siblings (and so on).
    Continue,
}

/// The result of parsing a Clang AST node.
#[derive(Debug)]
pub enum ParseResult<T> {
    /// We've already resolved this item before, here is the extant `ItemId` for
    /// it.
    AlreadyResolved(ItemId),

    /// This is a newly parsed item. If the cursor is `Some`, it points to the
    /// AST node where the new `T` was declared.
    New(T, Option<clang::Cursor>),
}

/// An intermediate representation "sub-item" (i.e. one of the types contained
/// inside an `ItemKind` variant) that can be parsed from a Clang cursor.
pub trait ClangSubItemParser: Sized {
    /// Attempt to parse this type from the given cursor.
    ///
    /// The fact that is a reference guarantees it's held by the context, and
    /// allow returning already existing types.
    fn parse(
        cursor: clang::Cursor,
        context: &mut BindgenContext,
    ) -> Result<ParseResult<Self>, ParseError>;
}

/// An intermediate representation item that can be parsed from a Clang cursor.
pub trait ClangItemParser: Sized {
    /// Parse this item from the given Clang cursor.
    fn parse(
        cursor: clang::Cursor,
        parent: Option<ItemId>,
        context: &mut BindgenContext,
    ) -> Result<ItemId, ParseError>;

    /// Parse this item from the given Clang type.
    fn from_ty(
        ty: &clang::Type,
        location: clang::Cursor,
        parent: Option<ItemId>,
        ctx: &mut BindgenContext,
    ) -> Result<TypeId, ParseError>;

    /// Identical to `from_ty`, but use the given `id` as the `ItemId` for the
    /// newly parsed item.
    fn from_ty_with_id(
        id: ItemId,
        ty: &clang::Type,
        location: clang::Cursor,
        parent: Option<ItemId>,
        ctx: &mut BindgenContext,
    ) -> Result<TypeId, ParseError>;

    /// Parse this item from the given Clang type, or if we haven't resolved all
    /// the other items this one depends on, an unresolved reference.
    fn from_ty_or_ref(
        ty: clang::Type,
        location: clang::Cursor,
        parent_id: Option<ItemId>,
        context: &mut BindgenContext,
    ) -> TypeId;

    /// Identical to `from_ty_or_ref`, but use the given `potential_id` as the
    /// `ItemId` for the newly parsed item.
    fn from_ty_or_ref_with_id(
        potential_id: ItemId,
        ty: clang::Type,
        location: clang::Cursor,
        parent_id: Option<ItemId>,
        context: &mut BindgenContext,
    ) -> TypeId;

    /// Create a named template type.
    fn type_param(
        with_id: Option<ItemId>,
        location: clang::Cursor,
        ctx: &mut BindgenContext,
    ) -> Option<TypeId>;

    /// Create a builtin type.
    fn builtin_type(
        kind: TypeKind,
        is_const: bool,
        context: &mut BindgenContext,
    ) -> TypeId;
}
