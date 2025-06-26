// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use std::fmt;

/// An error enum for all error types.
#[derive(Debug)]
pub enum Error {
    /// A [`std::io::Error`].
    Io(std::io::Error),
    /// A [`combine::stream::read::Error`].
    Read(combine::stream::read::Error),
    /// A [`combine::error::UnexpectedParse`].
    Parse(combine::error::UnexpectedParse),
}

impl From<std::io::Error> for Error {
    fn from(err: std::io::Error) -> Self {
        Error::Io(err)
    }
}

impl From<combine::stream::read::Error> for Error {
    fn from(err: combine::stream::read::Error) -> Self {
        Error::Read(err)
    }
}

impl From<combine::error::UnexpectedParse> for Error {
    fn from(err: combine::error::UnexpectedParse) -> Self {
        Error::Parse(err)
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::Io(err) => write!(f, "{err}"),
            Error::Read(err) => write!(f, "{err}"),
            Error::Parse(err) => write!(f, "{err}"),
        }
    }
}

impl std::error::Error for Error {}
