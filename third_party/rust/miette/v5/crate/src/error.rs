use std::io;

use thiserror::Error;

use crate::{self as miette, Diagnostic};

/**
Error enum for miette. Used by certain operations in the protocol.
*/
#[derive(Debug, Diagnostic, Error)]
pub enum MietteError {
    /// Wrapper around [`std::io::Error`]. This is returned when something went
    /// wrong while reading a [`SourceCode`](crate::SourceCode).
    #[error(transparent)]
    #[diagnostic(code(miette::io_error), url(docsrs))]
    IoError(#[from] io::Error),

    /// Returned when a [`SourceSpan`](crate::SourceSpan) extends beyond the
    /// bounds of a given [`SourceCode`](crate::SourceCode).
    #[error("The given offset is outside the bounds of its Source")]
    #[diagnostic(
        code(miette::span_out_of_bounds),
        help("Double-check your spans. Do you have an off-by-one error?"),
        url(docsrs)
    )]
    OutOfBounds,
}
