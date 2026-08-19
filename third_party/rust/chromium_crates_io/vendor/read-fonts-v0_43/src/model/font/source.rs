//! Font data sources.

use super::{FontBlob, FontTableFunction};
use alloc::sync::Arc;
use types::Tag;

/// Source for font data.
#[derive(Clone)]
pub enum FontSource {
    /// A nice flat buffer.
    Blob(FontBlob),
    /// Lazy loader with per-table data provided by a function.
    TableFunction(FontTableFunction),
}

impl<T: Into<FontBlob>> From<T> for FontSource {
    fn from(value: T) -> Self {
        Self::Blob(value.into())
    }
}

impl From<Arc<dyn Fn(Tag) -> Option<FontBlob> + Send + Sync>> for FontSource {
    fn from(value: Arc<dyn Fn(Tag) -> Option<FontBlob> + Send + Sync>) -> Self {
        Self::TableFunction(FontTableFunction::new(value))
    }
}

impl From<FontTableFunction> for FontSource {
    fn from(value: FontTableFunction) -> Self {
        Self::TableFunction(value)
    }
}
