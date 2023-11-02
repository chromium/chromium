use thiserror::Error;

use crate::{Diagnostic, Report};

/// Convenience [`Diagnostic`] that can be used as an "anonymous" wrapper for
/// Errors. This is intended to be paired with [`IntoDiagnostic`].
#[derive(Debug, Error)]
#[error(transparent)]
struct DiagnosticError(Box<dyn std::error::Error + Send + Sync + 'static>);
impl Diagnostic for DiagnosticError {}

/**
Convenience trait that adds a `.into_diagnostic()` method that converts a type
to a `Result<T, Report>`.
*/
pub trait IntoDiagnostic<T, E> {
    /// Converts [`Result`]-like types that return regular errors into a
    /// `Result` that returns a [`Diagnostic`].
    fn into_diagnostic(self) -> Result<T, Report>;
}

impl<T, E: std::error::Error + Send + Sync + 'static> IntoDiagnostic<T, E> for Result<T, E> {
    fn into_diagnostic(self) -> Result<T, Report> {
        self.map_err(|e| DiagnosticError(Box::new(e)).into())
    }
}
