use std::error;
use std::fmt;

/// Errors that can occur during code generation.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Error {
    /// Tried to generate an opaque blob for a type that did not have a layout.
    NoLayoutForOpaqueBlob,

    /// Tried to instantiate an opaque template definition, or a template
    /// definition that is too difficult for us to understand (like a partial
    /// template specialization).
    InstantiationOfOpaqueType,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(match *self {
            Error::NoLayoutForOpaqueBlob => {
                "Tried to generate an opaque blob, but had no layout"
            }
            Error::InstantiationOfOpaqueType => {
                "Instantiation of opaque template type or partial template \
                 specialization"
            }
        })
    }
}

impl error::Error for Error {}

/// A `Result` of `T` or an error of `bindgen::codegen::error::Error`.
pub type Result<T> = ::std::result::Result<T, Error>;
