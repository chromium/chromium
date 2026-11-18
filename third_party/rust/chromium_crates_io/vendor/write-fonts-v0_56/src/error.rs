//! Errors that occur during writing

use std::sync::Arc;

use types::Tag;

use crate::{graph::Graph, validate::ValidationReport};

/// An error returned when attempting to add a table to the builder.
///
/// This wraps a compilation error, adding the tag of the table where it was
/// encountered.
#[derive(Clone, Debug)]
#[non_exhaustive]
pub struct BuilderError {
    /// The tag of the root table where the error occurred
    pub tag: Tag,
    /// The underlying error
    pub inner: Error,
}

/// A packing could not be found that satisfied all offsets
///
/// If the "dot2" feature is enabled, you can use the `write_graph_viz` method
/// on this error to dump a text representation of the graph to disk. You can
/// then use graphviz to convert this into an image. For example, to create
/// an SVG, you might use, `dot -v -Tsvg failure.dot2 > failure.svg`, where
/// 'failure.dot2' is the path where you dumped the graph.
#[derive(Clone)]
pub struct PackingError {
    // this is Arc so that we can clone and still be sent between threads.
    pub(crate) graph: Arc<Graph>,
}

/// An error occurred while writing this table
#[derive(Debug, Clone)]
pub enum Error {
    /// The table failed a validation check
    ///
    /// This indicates that the table contained invalid or inconsistent data
    /// (for instance, it had an explicit version set, but contained fields
    /// not present in that version).
    ValidationFailed(ValidationReport),
    /// The table contained overflowing offsets
    ///
    /// This indicates that an ordering could not be found that allowed all
    /// tables to be reachable from their parents. See [`PackingError`] for
    /// more details.
    PackingFailed(PackingError),
    /// Invalid input provided to a builder
    InvalidInput(&'static str),
}

impl PackingError {
    /// Write a graphviz file representing the failed packing to the provided path.
    ///
    /// Has the same semantics as [`std::fs::write`].
    #[cfg(feature = "dot2")]
    pub fn write_graph_viz(&self, path: impl AsRef<std::path::Path>) -> std::io::Result<()> {
        self.graph.write_graph_viz(path)
    }
}

impl std::fmt::Display for BuilderError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "failed to build '{}' table: '{}'", self.tag, self.inner)
    }
}

impl std::error::Error for BuilderError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.inner)
    }
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Error::ValidationFailed(report) => report.fmt(f),
            Error::PackingFailed(error) => error.fmt(f),
            Error::InvalidInput(msg) => write!(f, "Invalid input: {}", msg),
        }
    }
}

impl std::fmt::Display for PackingError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "Table packing failed with {} overflows",
            self.graph.find_overflows().len()
        )
    }
}

impl std::fmt::Debug for PackingError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{self}")
    }
}

impl std::error::Error for PackingError {}
impl std::error::Error for Error {}

impl From<ValidationReport> for Error {
    fn from(value: ValidationReport) -> Self {
        Error::ValidationFailed(value)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Some users, notably fontmake-rs, like Send errors.
    #[test]
    fn assert_compiler_error_is_send() {
        fn send_me_baby<T: Send>() {}
        send_me_baby::<Error>();
    }
}
