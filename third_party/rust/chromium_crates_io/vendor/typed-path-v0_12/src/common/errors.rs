use core::fmt;

/// An error returned if the prefix was not found.
///
/// This `struct` is created by the [`strip_prefix`] method on [`Path`].
/// See its documentation for more.
///
/// [`Path`]: crate::Path
/// [`strip_prefix`]: crate::Path::strip_prefix
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct StripPrefixError(pub(crate) ());

impl fmt::Display for StripPrefixError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "prefix not found")
    }
}

#[cfg(feature = "std")]
impl std::error::Error for StripPrefixError {}

/// An error returned when a path violates checked criteria.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum CheckedPathError {
    /// When a normal component contains invalid characters for the current encoding.
    InvalidFilename,

    /// When a path component that represents a parent directory is provided such that the original
    /// path would be escaped to access arbitrary files.
    PathTraversalAttack,

    /// When a path component that represents a prefix is provided after the start of the path.
    UnexpectedPrefix,

    /// When a path component that represents a root is provided after the start of the path.
    UnexpectedRoot,
}

impl fmt::Display for CheckedPathError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::InvalidFilename => write!(f, "path contains invalid filename"),
            Self::PathTraversalAttack => write!(f, "path attempts to escape original path"),
            Self::UnexpectedPrefix => write!(f, "path contains unexpected prefix"),
            Self::UnexpectedRoot => write!(f, "path contains unexpected root"),
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for CheckedPathError {}
