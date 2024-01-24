//! Basic representation of an in-memory font resource.

pub use read_fonts::FontRef;

/// Identifier used as a key for internal caches.
///
/// The representation of a font in this crate is designed to be flexible for
/// clients (any type that implements [`TableProvider`](read_fonts::TableProvider)
/// is supported) without imposing any strict constraints on ownership or data layout.
///
/// The price for this flexibility is that an additional mechanism is necessary
/// to enable caching of state for performance sensitive operations. The chosen
/// design stores cached data in separate contexts (see [`scale::Context`][crate::scale::Context]
/// as an example) keyed by this type.
///
/// Thus, to enable caching, a client may provide an instance of this type when
/// making use of a context. For example, [`ScalerBuilder::cache_key`][crate::scale::ScalerBuilder::cache_key] can
/// be used when configuring a glyph scaler.
///
/// # Semantics
/// Currently, the parameters used to construct this type carry no actual semantics.
/// The `index` parameter, for example, is not required to match the index of a font
/// in a collection. Types and names were chosen to accommodate the common representation
/// of a pointer and index pair.
///
/// The only requirement is that the 96 bits be unique for a given font.
///
/// ## Uniqueness and safety
/// Users are responsible for ensuring that a unique identifier is not used for
/// different fonts within a single context. Violating this invariant is guaranteed
/// to be safe, but will very likely lead to incorrect results.
#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash, Debug)]
pub struct UniqueId {
    /// Unique identifier for the data blob containing the content of
    /// a font file.
    data_id: u64,
    /// Index of a font in a font collection file.
    index: u32,
}

impl UniqueId {
    /// Creates a new unique identifier from the given data identifier and font
    /// collection index.
    pub fn new(data_id: u64, index: u32) -> Self {
        Self { data_id, index }
    }
}
