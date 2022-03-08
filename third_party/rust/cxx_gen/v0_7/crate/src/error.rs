// We can expose more detail on the error as the need arises, but start with an
// opaque error type for now.

use std::error::Error as StdError;
use std::fmt::{self, Debug, Display};

#[allow(missing_docs)]
pub struct Error {
    pub(crate) err: crate::gen::Error,
}

impl From<crate::gen::Error> for Error {
    fn from(err: crate::gen::Error) -> Self {
        Error { err }
    }
}

impl Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        Display::fmt(&self.err, f)
    }
}

impl Debug for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        Debug::fmt(&self.err, f)
    }
}

impl StdError for Error {
    fn source(&self) -> Option<&(dyn StdError + 'static)> {
        self.err.source()
    }
}
