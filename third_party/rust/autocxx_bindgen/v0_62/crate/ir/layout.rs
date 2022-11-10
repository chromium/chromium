//! Intermediate representation for the physical layout of some type.

use super::derive::CanDerive;
use super::ty::{Type, TypeKind, RUST_DERIVE_IN_ARRAY_LIMIT};
use crate::clang;
use crate::ir::context::BindgenContext;
use std::cmp;

/// A type that represents the struct layout of a type.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Layout {
    /// The size (in bytes) of this layout.
    pub size: usize,
    /// The alignment (in bytes) of this layout.
    pub align: usize,
    /// Whether this layout's members are packed or not.
    pub packed: bool,
}

#[test]
fn test_layout_for_size() {
    use std::mem;

    let ptr_size = mem::size_of::<*mut ()>();
    assert_eq!(
        Layout::for_size_internal(ptr_size, ptr_size),
        Layout::new(ptr_size, ptr_size)
    );
    assert_eq!(
        Layout::for_size_internal(ptr_size, 3 * ptr_size),
        Layout::new(3 * ptr_size, ptr_size)
    );
}

impl Layout {
    /// Gets the integer type name for a given known size.
    pub fn known_type_for_size(
        ctx: &BindgenContext,
        size: usize,
    ) -> Option<&'static str> {
        Some(match size {
            16 if ctx.options().rust_features.i128_and_u128 => "u128",
            8 => "u64",
            4 => "u32",
            2 => "u16",
            1 => "u8",
            _ => return None,
        })
    }

    /// Construct a new `Layout` with the given `size` and `align`. It is not
    /// packed.
    pub fn new(size: usize, align: usize) -> Self {
        Layout {
            size,
            align,
            packed: false,
        }
    }

    fn for_size_internal(ptr_size: usize, size: usize) -> Self {
        let mut next_align = 2;
        while size % next_align == 0 && next_align <= ptr_size {
            next_align *= 2;
        }
        Layout {
            size,
            align: next_align / 2,
            packed: false,
        }
    }

    /// Creates a non-packed layout for a given size, trying to use the maximum
    /// alignment possible.
    pub fn for_size(ctx: &BindgenContext, size: usize) -> Self {
        Self::for_size_internal(ctx.target_pointer_size(), size)
    }

    /// Is this a zero-sized layout?
    pub fn is_zero(&self) -> bool {
        self.size == 0 && self.align == 0
    }

    /// Construct a zero-sized layout.
    pub fn zero() -> Self {
        Self::new(0, 0)
    }

    /// Get this layout as an opaque type.
    pub fn opaque(&self) -> Opaque {
        Opaque(*self)
    }
}

/// When we are treating a type as opaque, it is just a blob with a `Layout`.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Opaque(pub Layout);

impl Opaque {
    /// Construct a new opaque type from the given clang type.
    pub fn from_clang_ty(ty: &clang::Type, ctx: &BindgenContext) -> Type {
        let layout = Layout::new(ty.size(ctx), ty.align(ctx));
        let ty_kind = TypeKind::Opaque;
        let is_const = ty.is_const();
        Type::new(None, Some(layout), ty_kind, is_const)
    }

    /// Return the known rust type we should use to create a correctly-aligned
    /// field with this layout.
    pub fn known_rust_type_for_array(
        &self,
        ctx: &BindgenContext,
    ) -> Option<&'static str> {
        Layout::known_type_for_size(ctx, self.0.align)
    }

    /// Return the array size that an opaque type for this layout should have if
    /// we know the correct type for it, or `None` otherwise.
    pub fn array_size(&self, ctx: &BindgenContext) -> Option<usize> {
        if self.known_rust_type_for_array(ctx).is_some() {
            Some(self.0.size / cmp::max(self.0.align, 1))
        } else {
            None
        }
    }

    /// Return `true` if this opaque layout's array size will fit within the
    /// maximum number of array elements that Rust allows deriving traits
    /// with. Return `false` otherwise.
    pub fn array_size_within_derive_limit(
        &self,
        ctx: &BindgenContext,
    ) -> CanDerive {
        if self
            .array_size(ctx)
            .map_or(false, |size| size <= RUST_DERIVE_IN_ARRAY_LIMIT)
        {
            CanDerive::Yes
        } else {
            CanDerive::Manually
        }
    }
}
