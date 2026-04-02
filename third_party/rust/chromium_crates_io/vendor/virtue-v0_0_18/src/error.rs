use crate::{
    generate::{PushParseError, StreamBuilder},
    prelude::*,
};
use std::fmt;

/// Errors that can occur while parsing or generator your derive macro.
#[derive(Debug)]
pub enum Error {
    /// The data type at `Span` is unknown. This will be called when [`Parse::new`] is called on anything that is not a `struct` or `enum`.
    ///
    /// [`Parse::new`]: enum.Parse.html#method.new
    UnknownDataType(Span),

    /// The rust syntax is invalid. This can be returned while parsing the Enum or Struct.
    ///
    /// This error is assumed to not appear as rustc will do syntax checking before virtue gets access to the [`TokenStream`].
    /// However this error could still be returned.
    InvalidRustSyntax {
        /// The span at which the invalid syntax is found
        span: Span,
        /// The expected rust syntax when this parsing occured
        expected: String,
    },

    /// Expected an ident at the given span.
    ExpectedIdent(Span),

    /// Failed to parse the code passed to [`StreamBuilder::push_parsed`].
    ///
    /// [`StreamBuilder::push_parsed`]: struct.StreamBuilder.html#method.push_parsed
    PushParse {
        /// An optional span. Normally this is `None`, unless `.with_span` is called.
        span: Option<Span>,
        /// The internal parse error
        error: PushParseError,
    },

    /// A custom error thrown by the developer
    Custom {
        /// The error message
        error: String,
        /// Optionally the position that the error occurred at
        span: Option<Span>,
    },
}

impl From<PushParseError> for Error {
    fn from(e: PushParseError) -> Self {
        Self::PushParse {
            span: None,
            error: e,
        }
    }
}

impl Error {
    /// Throw a custom error
    pub fn custom(s: impl Into<String>) -> Self {
        Self::Custom {
            error: s.into(),
            span: None,
        }
    }

    /// Throw a custom error at a given location
    pub fn custom_at(s: impl Into<String>, span: Span) -> Self {
        Self::Custom {
            error: s.into(),
            span: Some(span),
        }
    }

    /// Throw a custom error at a given token
    pub fn custom_at_token(s: impl Into<String>, token: TokenTree) -> Self {
        Self::Custom {
            error: s.into(),
            span: Some(token.span()),
        }
    }

    /// Throw a custom error at a given `Option<TokenTree>`
    pub fn custom_at_opt_token(s: impl Into<String>, token: Option<TokenTree>) -> Self {
        Self::Custom {
            error: s.into(),
            span: token.map(|t| t.span()),
        }
    }

    pub(crate) fn wrong_token<T>(token: Option<&TokenTree>, expected: &str) -> Result<T> {
        Err(Self::InvalidRustSyntax {
            span: token.map(|t| t.span()).unwrap_or_else(Span::call_site),
            expected: format!("{}, got {:?}", expected, token),
        })
    }

    /// Return a new error that is located at the given span
    pub fn with_span(mut self, new_span: Span) -> Self {
        match &mut self {
            Error::UnknownDataType(span) => *span = new_span,
            Error::InvalidRustSyntax { span, .. } => *span = new_span,
            Error::ExpectedIdent(span) => *span = new_span,
            Error::PushParse { span, .. } => {
                *span = Some(new_span);
            }
            Error::Custom { span, .. } => *span = Some(new_span),
        }

        self
    }
}

// helper functions for the unit tests
#[cfg(test)]
impl Error {
    pub fn is_unknown_data_type(&self) -> bool {
        matches!(self, Error::UnknownDataType(_))
    }

    pub fn is_invalid_rust_syntax(&self) -> bool {
        matches!(self, Error::InvalidRustSyntax { .. })
    }
}

impl fmt::Display for Error {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Self::UnknownDataType(_) => {
                write!(fmt, "Unknown data type, only enum and struct are supported")
            }
            Self::InvalidRustSyntax { expected, .. } => {
                write!(fmt, "Invalid rust syntax, expected {}", expected)
            }
            Self::ExpectedIdent(_) => write!(fmt, "Expected ident"),
            Self::PushParse { error, .. } => write!(
                fmt,
                "Invalid code passed to `StreamBuilder::push_parsed`: {:?}",
                error
            ),
            Self::Custom { error, .. } => write!(fmt, "{}", error),
        }
    }
}

impl Error {
    /// Turn this error into a [`TokenStream`] so it shows up as a [`compile_error`] for the user.
    pub fn into_token_stream(self) -> TokenStream {
        let maybe_span = match &self {
            Self::UnknownDataType(span)
            | Self::ExpectedIdent(span)
            | Self::InvalidRustSyntax { span, .. } => Some(*span),
            Self::Custom { span, .. } | Self::PushParse { span, .. } => *span,
        };
        self.throw_with_span(maybe_span.unwrap_or_else(Span::call_site))
    }

    /// Turn this error into a [`TokenStream`] so it shows up as a [`compile_error`] for the user. The error will be shown at the given `span`.
    pub fn throw_with_span(self, span: Span) -> TokenStream {
        // compile_error!($message)
        let mut builder = StreamBuilder::new();
        builder.ident_str("compile_error");
        builder.punct('!');
        builder
            .group(Delimiter::Brace, |b| {
                b.lit_str(self.to_string());
                Ok(())
            })
            .unwrap();
        builder.set_span_on_all_tokens(span);
        builder.stream
    }
}
