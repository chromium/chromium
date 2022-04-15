/*!
This module defines the core of the miette protocol: a series of types and
traits that you can implement to get access to miette's (and related library's)
full reporting and such features.
*/
use std::{
    fmt::{self, Display},
    fs,
    panic::Location,
};

use crate::MietteError;

/// Adds rich metadata to your Error that can be used by
/// [`Report`](crate::Report) to print really nice and human-friendly error
/// messages.
pub trait Diagnostic: std::error::Error {
    /// Unique diagnostic code that can be used to look up more information
    /// about this `Diagnostic`. Ideally also globally unique, and documented
    /// in the toplevel crate's documentation for easy searching. Rust path
    /// format (`foo::bar::baz`) is recommended, but more classic codes like
    /// `E0123` or enums will work just fine.
    fn code<'a>(&'a self) -> Option<Box<dyn Display + 'a>> {
        None
    }

    /// Diagnostic severity. This may be used by
    /// [`ReportHandler`](crate::ReportHandler)s to change the display format
    /// of this diagnostic.
    ///
    /// If `None`, reporters should treat this as [`Severity::Error`].
    fn severity(&self) -> Option<Severity> {
        None
    }

    /// Additional help text related to this `Diagnostic`. Do you have any
    /// advice for the poor soul who's just run into this issue?
    fn help<'a>(&'a self) -> Option<Box<dyn Display + 'a>> {
        None
    }

    /// URL to visit for a more detailed explanation/help about this
    /// `Diagnostic`.
    fn url<'a>(&'a self) -> Option<Box<dyn Display + 'a>> {
        None
    }

    /// Source code to apply this `Diagnostic`'s [`Diagnostic::labels`] to.
    fn source_code(&self) -> Option<&dyn SourceCode> {
        None
    }

    /// Labels to apply to this `Diagnostic`'s [`Diagnostic::source_code`]
    fn labels(&self) -> Option<Box<dyn Iterator<Item = LabeledSpan> + '_>> {
        None
    }

    /// Additional related `Diagnostic`s.
    fn related<'a>(&'a self) -> Option<Box<dyn Iterator<Item = &'a dyn Diagnostic> + 'a>> {
        None
    }
}

impl std::error::Error for Box<dyn Diagnostic> {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        (**self).source()
    }

    fn cause(&self) -> Option<&dyn std::error::Error> {
        self.source()
    }
}

impl<T: Diagnostic + Send + Sync + 'static> From<T>
    for Box<dyn Diagnostic + Send + Sync + 'static>
{
    fn from(diag: T) -> Self {
        Box::new(diag)
    }
}

impl<T: Diagnostic + Send + Sync + 'static> From<T> for Box<dyn Diagnostic + Send + 'static> {
    fn from(diag: T) -> Self {
        Box::<dyn Diagnostic + Send + Sync>::from(diag)
    }
}

impl<T: Diagnostic + Send + Sync + 'static> From<T> for Box<dyn Diagnostic + 'static> {
    fn from(diag: T) -> Self {
        Box::<dyn Diagnostic + Send + Sync>::from(diag)
    }
}

impl From<&str> for Box<dyn Diagnostic> {
    fn from(s: &str) -> Self {
        From::from(String::from(s))
    }
}

impl<'a> From<&str> for Box<dyn Diagnostic + Send + Sync + 'a> {
    fn from(s: &str) -> Self {
        From::from(String::from(s))
    }
}

impl From<String> for Box<dyn Diagnostic> {
    fn from(s: String) -> Self {
        let err1: Box<dyn Diagnostic + Send + Sync> = From::from(s);
        let err2: Box<dyn Diagnostic> = err1;
        err2
    }
}

impl From<String> for Box<dyn Diagnostic + Send + Sync> {
    fn from(s: String) -> Self {
        struct StringError(String);

        impl std::error::Error for StringError {}
        impl Diagnostic for StringError {}

        impl Display for StringError {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                Display::fmt(&self.0, f)
            }
        }

        // Purposefully skip printing "StringError(..)"
        impl fmt::Debug for StringError {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                fmt::Debug::fmt(&self.0, f)
            }
        }

        Box::new(StringError(s))
    }
}

impl From<Box<dyn std::error::Error + Send + Sync>> for Box<dyn Diagnostic + Send + Sync> {
    fn from(s: Box<dyn std::error::Error + Send + Sync>) -> Self {
        #[derive(thiserror::Error)]
        #[error(transparent)]
        struct BoxedDiagnostic(Box<dyn std::error::Error + Send + Sync>);
        impl fmt::Debug for BoxedDiagnostic {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                fmt::Debug::fmt(&self.0, f)
            }
        }

        impl Diagnostic for BoxedDiagnostic {}

        Box::new(BoxedDiagnostic(s))
    }
}

/**
[`Diagnostic`] severity. Intended to be used by
[`ReportHandler`](crate::ReportHandler)s to change the way different
[`Diagnostic`]s are displayed.
*/
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum Severity {
    /// Critical failure. The program cannot continue.
    Error,
    /// Warning. Please take note.
    Warning,
    /// Just some help. Here's how you could be doing it better.
    Advice,
}

/**
Represents readable source code of some sort.

This trait is able to support simple `SourceCode` types like [`String`]s, as
well as more involved types like indexes into centralized `SourceMap`-like
types, file handles, and even network streams.

If you can read it, you can source it, and it's not necessary to read the
whole thing--meaning you should be able to support `SourceCode`s which are
gigabytes or larger in size.
*/
pub trait SourceCode: Send + Sync {
    /// Read the bytes for a specific span from this SourceCode, keeping a
    /// certain number of lines before and after the span as context.
    fn read_span<'a>(
        &'a self,
        span: &SourceSpan,
        context_lines_before: usize,
        context_lines_after: usize,
    ) -> Result<Box<dyn SpanContents<'a> + 'a>, MietteError>;
}

/// A labeled [`SourceSpan`].
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct LabeledSpan {
    label: Option<String>,
    span: SourceSpan,
}

impl LabeledSpan {
    /// Makes a new labeled span.
    pub fn new(label: Option<String>, offset: ByteOffset, len: ByteOffset) -> Self {
        Self {
            label,
            span: (offset, len).into(),
        }
    }

    /// Makes a new labeled span using an existing span.
    pub fn new_with_span(label: Option<String>, span: impl Into<SourceSpan>) -> Self {
        Self {
            label,
            span: span.into(),
        }
    }

    /// Gets the (optional) label string for this `LabeledSpan`.
    pub fn label(&self) -> Option<&str> {
        self.label.as_deref()
    }

    /// Returns a reference to the inner [`SourceSpan`].
    pub fn inner(&self) -> &SourceSpan {
        &self.span
    }

    /// Returns the 0-based starting byte offset.
    pub fn offset(&self) -> usize {
        self.span.offset()
    }

    /// Returns the number of bytes this `LabeledSpan` spans.
    pub fn len(&self) -> usize {
        self.span.len()
    }

    /// True if this `LabeledSpan` is empty.
    pub fn is_empty(&self) -> bool {
        self.span.is_empty()
    }
}

/**
Contents of a [`SourceCode`] covered by [`SourceSpan`].

Includes line and column information to optimize highlight calculations.
*/
pub trait SpanContents<'a> {
    /// Reference to the data inside the associated span, in bytes.
    fn data(&self) -> &'a [u8];
    /// [`SourceSpan`] representing the span covered by this `SpanContents`.
    fn span(&self) -> &SourceSpan;
    /// An optional (file?) name for the container of this `SpanContents`.
    fn name(&self) -> Option<&str> {
        None
    }
    /// The 0-indexed line in the associated [`SourceCode`] where the data
    /// begins.
    fn line(&self) -> usize;
    /// The 0-indexed column in the associated [`SourceCode`] where the data
    /// begins, relative to `line`.
    fn column(&self) -> usize;
    /// Total number of lines covered by this `SpanContents`.
    fn line_count(&self) -> usize;
}

/**
Basic implementation of the [`SpanContents`] trait, for convenience.
*/
#[derive(Clone, Debug)]
pub struct MietteSpanContents<'a> {
    // Data from a [`SourceCode`], in bytes.
    data: &'a [u8],
    // span actually covered by this SpanContents.
    span: SourceSpan,
    // The 0-indexed line where the associated [`SourceSpan`] _starts_.
    line: usize,
    // The 0-indexed column where the associated [`SourceSpan`] _starts_.
    column: usize,
    // Number of line in this snippet.
    line_count: usize,
    // Optional filename
    name: Option<String>,
}

impl<'a> MietteSpanContents<'a> {
    /// Make a new [`MietteSpanContents`] object.
    pub fn new(
        data: &'a [u8],
        span: SourceSpan,
        line: usize,
        column: usize,
        line_count: usize,
    ) -> MietteSpanContents<'a> {
        MietteSpanContents {
            data,
            span,
            line,
            column,
            line_count,
            name: None,
        }
    }

    /// Make a new [`MietteSpanContents`] object, with a name for its 'file'.
    pub fn new_named(
        name: String,
        data: &'a [u8],
        span: SourceSpan,
        line: usize,
        column: usize,
        line_count: usize,
    ) -> MietteSpanContents<'a> {
        MietteSpanContents {
            data,
            span,
            line,
            column,
            line_count,
            name: Some(name),
        }
    }
}

impl<'a> SpanContents<'a> for MietteSpanContents<'a> {
    fn data(&self) -> &'a [u8] {
        self.data
    }
    fn span(&self) -> &SourceSpan {
        &self.span
    }
    fn line(&self) -> usize {
        self.line
    }
    fn column(&self) -> usize {
        self.column
    }
    fn line_count(&self) -> usize {
        self.line_count
    }
    fn name(&self) -> Option<&str> {
        self.name.as_deref()
    }
}

/**
Span within a [`SourceCode`] with an associated message.
*/
#[derive(Clone, Debug, Eq, PartialEq, Hash)]
pub struct SourceSpan {
    /// The start of the span.
    offset: SourceOffset,
    /// The total length of the span. Think of this as an offset from `start`.
    length: SourceOffset,
}

impl SourceSpan {
    /// Create a new [`SourceSpan`].
    pub fn new(start: SourceOffset, length: SourceOffset) -> Self {
        Self {
            offset: start,
            length,
        }
    }

    /// The absolute offset, in bytes, from the beginning of a [`SourceCode`].
    pub fn offset(&self) -> usize {
        self.offset.offset()
    }

    /// Total length of the [`SourceSpan`], in bytes.
    pub fn len(&self) -> usize {
        self.length.offset()
    }

    /// Whether this [`SourceSpan`] has a length of zero. It may still be useful
    /// to point to a specific point.
    pub fn is_empty(&self) -> bool {
        self.length.offset() == 0
    }
}

impl From<(ByteOffset, ByteOffset)> for SourceSpan {
    fn from((start, len): (ByteOffset, ByteOffset)) -> Self {
        Self {
            offset: start.into(),
            length: len.into(),
        }
    }
}

impl From<(SourceOffset, SourceOffset)> for SourceSpan {
    fn from((start, len): (SourceOffset, SourceOffset)) -> Self {
        Self {
            offset: start,
            length: len,
        }
    }
}

impl From<std::ops::Range<ByteOffset>> for SourceSpan {
    fn from(range: std::ops::Range<ByteOffset>) -> Self {
        Self {
            offset: range.start.into(),
            length: range.len().into(),
        }
    }
}

/**
"Raw" type for the byte offset from the beginning of a [`SourceCode`].
*/
pub type ByteOffset = usize;

/**
Newtype that represents the [`ByteOffset`] from the beginning of a [`SourceCode`]
*/
#[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
pub struct SourceOffset(ByteOffset);

impl SourceOffset {
    /// Actual byte offset.
    pub fn offset(&self) -> ByteOffset {
        self.0
    }

    /// Little utility to help convert line/column locations into
    /// miette-compatible Spans
    ///
    /// This function is infallible: Giving an out-of-range line/column pair
    /// will return the offset of the last byte in the source.
    pub fn from_location(source: impl AsRef<str>, loc_line: usize, loc_col: usize) -> Self {
        let mut line = 0usize;
        let mut col = 0usize;
        let mut offset = 0usize;
        for char in source.as_ref().chars() {
            if char == '\n' {
                col = 0;
                line += 1;
            } else {
                col += 1;
            }
            if line + 1 >= loc_line && col + 1 >= loc_col {
                break;
            }
            offset += char.len_utf8();
        }

        SourceOffset(offset)
    }

    /// Returns an offset for the _file_ location of wherever this function is
    /// called. If you want to get _that_ caller's location, mark this
    /// function's caller with `#[track_caller]` (and so on and so forth).
    ///
    /// Returns both the filename that was given and the offset of the caller
    /// as a [`SourceOffset`].
    ///
    /// Keep in mind that this fill only work if the file your Rust source
    /// file was compiled from is actually available at that location. If
    /// you're shipping binaries for your application, you'll want to ignore
    /// the Err case or otherwise report it.
    #[track_caller]
    pub fn from_current_location() -> Result<(String, Self), MietteError> {
        let loc = Location::caller();
        Ok((
            loc.file().into(),
            fs::read_to_string(loc.file())
                .map(|txt| Self::from_location(&txt, loc.line() as usize, loc.column() as usize))?,
        ))
    }
}

impl From<ByteOffset> for SourceOffset {
    fn from(bytes: ByteOffset) -> Self {
        SourceOffset(bytes)
    }
}
