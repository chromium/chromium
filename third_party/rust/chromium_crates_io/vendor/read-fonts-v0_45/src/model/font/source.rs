//! Font data sources.

use super::{Blob, TableFunction};
use alloc::sync::Arc;
use types::Tag;

/// Source for font data.
#[derive(Clone)]
pub enum Source {
    /// A nice flat buffer.
    Blob(Blob),
    /// Lazy loader with per-table data provided by a function.
    TableFunction(TableFunction),
}

impl<T: Into<Blob>> From<T> for Source {
    fn from(value: T) -> Self {
        Self::Blob(value.into())
    }
}

impl From<Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync>> for Source {
    fn from(value: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync>) -> Self {
        Self::TableFunction(TableFunction::new(value))
    }
}

impl From<TableFunction> for Source {
    fn from(value: TableFunction) -> Self {
        Self::TableFunction(value)
    }
}
