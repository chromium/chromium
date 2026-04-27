use crate::Parser;

use core::{
    fmt::{self, Display},
    marker::PhantomData,
};

/// Error returned by all parsing methods that return Result.
///
/// This error type knows [`where`](#method.offset) the error happened,
/// in what [`direction`](#method.error_direction) the bytes were being parsed,
/// and the [`kind`](#method.kind) of error that happened.
#[derive(Debug, PartialEq, Eq, Clone)]
pub struct ParseError<'a> {
    start_offset: u32,
    end_offset: u32,
    direction: ParseDirection,
    kind: ErrorKind,
    // Just in case that it goes back to storing the parser
    _lifetime: PhantomData<&'a [u8]>,
}

impl<'a> ParseError<'a> {
    /// Constructs a `ParseError`.
    #[inline(always)]
    pub const fn new(parser: Parser<'a>, kind: ErrorKind) -> Self {
        Self {
            start_offset: parser.start_offset,
            end_offset: parser.start_offset + parser.bytes.len() as u32,
            direction: parser.parse_direction,
            kind,
            _lifetime: PhantomData,
        }
    }

    /// A const fn equivalent of a clone method.
    pub const fn copy(&self) -> Self {
        Self {
            start_offset: self.start_offset,
            end_offset: self.end_offset,
            direction: self.direction,
            kind: self.kind,
            _lifetime: PhantomData,
        }
    }

    /// Gets the byte offset of this error in the parsed bytes that the
    /// [`Parser`] was constructed from.
    #[inline(always)]
    pub const fn offset(&self) -> usize {
        (match self.direction {
            ParseDirection::FromStart => self.start_offset,
            ParseDirection::FromEnd => self.end_offset,
        }) as usize
    }

    /// The direction that this error happened from,
    /// either from the start or the end.
    pub const fn error_direction(&self) -> ParseDirection {
        self.direction
    }

    /// The kind of parsing error that this is.
    pub const fn kind(&self) -> ErrorKind {
        self.kind
    }

    /// For erroring with an error message,
    /// this is called by the [`unwrap_ctx`] macro.
    ///
    /// [`unwrap_ctx`]: ../result/macro.unwrap_ctx.html
    #[track_caller]
    pub const fn panic(&self) -> ! {
        match self.kind {
            ErrorKind::ParseInteger => match self.direction {
                ParseDirection::FromStart => {
                    [/*integer parsing errored from start offset*/][self.offset()]
                }
                ParseDirection::FromEnd => {
                    [/*integer parsing errored from end offset*/][self.offset()]
                }
            },
            ErrorKind::ParseBool => match self.direction {
                ParseDirection::FromStart => {
                    [/*bool parsing errored from start offset*/][self.offset()]
                }
                ParseDirection::FromEnd => {
                    [/*bool parsing errored from end offset*/][self.offset()]
                }
            },
            ErrorKind::Find => match self.direction {
                ParseDirection::FromStart => {
                    [/*Error finding pattern from start offset*/][self.offset()]
                }
                ParseDirection::FromEnd => {
                    [/*Error finding pattern from end offset*/][self.offset()]
                }
            },
            ErrorKind::Strip => match self.direction {
                ParseDirection::FromStart => [/*Error stripping from start offset*/][self.offset()],
                ParseDirection::FromEnd => [/*Error stripping from end offset*/][self.offset()],
            },
            ErrorKind::SkipByte => [/*Error skipping byte at offset*/][self.offset()],
            ErrorKind::Other => match self.direction {
                ParseDirection::FromStart => [/*parse error from start offset*/][self.offset()],
                ParseDirection::FromEnd => [/*parse error from end offset*/][self.offset()],
            },
        }
    }
}

impl<'a> Display for ParseError<'a> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self.direction {
            ParseDirection::FromStart => "error from the start at the ",
            ParseDirection::FromEnd => "error from the end at the ",
        })?;
        Display::fmt(&self.offset(), f)?;
        f.write_str(" byte offset")?;

        f.write_str(match self.kind {
            ErrorKind::ParseInteger => " while parsing an integer",
            ErrorKind::ParseBool => " while parsing a bool",
            ErrorKind::Find => " while trying to find and skip a pattern",
            ErrorKind::Strip => " while trying to strip a pattern",
            ErrorKind::SkipByte => " while trying to skip a byte",
            ErrorKind::Other => " (a parsing error)",
        })
    }
}

////////////////////////////////////////////////////////////////////////////////

/// The direction that a parser was parsing from when an error happened.
#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub enum ParseDirection {
    /// Parsing was attempted from the start of the string
    FromStart = 0,
    /// Parsing was attempted from the end of the string
    FromEnd = 1,
}

////////////////////////////////////////////////////////////////////////////////

/// What kind of parsing error this is.
#[non_exhaustive]
#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub enum ErrorKind {
    /// Returned from integer parsing methods
    ParseInteger,
    /// Returned from `parse_bool`
    ParseBool,
    /// Returned from `*find*` methods
    Find,
    /// Returned from `strip_*` methods
    Strip,
    /// Returned from `skip_byte`
    SkipByte,
    /// For user-defined types
    Other,
}

////////////////////////////////////////////////////////////////////////////////

/// Result alias for functions that mutate the parser fallibly.
pub type ParserResult<'a, E = ParseError<'a>> = Result<Parser<'a>, E>;

/// Result alias for functions that parse values.
pub type ParseValueResult<'a, T, E = ParseError<'a>> = Result<(T, Parser<'a>), E>;
