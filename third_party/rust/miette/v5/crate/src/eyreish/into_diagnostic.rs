use thiserror::Error;

use crate::{Diagnostic, Report};

/// Convenience [`Diagnostic`] that can be used as an "anonymous" wrapper for
/// Errors. This is intended to be paired with [`IntoDiagnostic`].
#[derive(Debug, Error)]
#[error(transparent)]
struct DiagnosticError(Box<dyn std::error::Error + Send + Sync + 'static>);
impl Diagnostic for DiagnosticError {}

/**
Convenience trait that adds a [`.into_diagnostic()`](IntoDiagnostic::into_diagnostic) method that converts a type implementing
[`std::error::Error`] to a [`Result<T, Report>`].

## Warning

Calling this on a type implementing [`Diagnostic`] will reduce it to the common denominator of
[`std::error::Error`]. Meaning all extra information provided by [`Diagnostic`] will be
inaccessible. If you have a type implementing [`Diagnostic`] consider simply returning it or using
[`Into`] or the [`Try`](std::ops::Try) operator (`?`).
*/
pub trait IntoDiagnostic<T, E> {
    /// Converts [`Result`] types that return regular [`std::error::Error`]s
    /// into a [`Result`] that returns a [`Diagnostic`].
    fn into_diagnostic(self) -> Result<T, Report>;
}

impl<T, E: std::error::Error + Send + Sync + 'static> IntoDiagnostic<T, E> for Result<T, E> {
    fn into_diagnostic(self) -> Result<T, Report> {
        self.map_err(|e| DiagnosticError(Box::new(e)).into())
    }
}
