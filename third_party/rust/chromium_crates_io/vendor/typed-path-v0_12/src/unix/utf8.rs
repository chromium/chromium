mod components;

use core::fmt;
use core::hash::Hasher;

pub use components::*;

use crate::common::CheckedPathError;
use crate::no_std_compat::*;
use crate::typed::{Utf8TypedPath, Utf8TypedPathBuf};
use crate::{private, Encoding, UnixEncoding, Utf8Encoding, Utf8Path, Utf8PathBuf};

/// Represents a Unix-specific [`Utf8Path`]
pub type Utf8UnixPath = Utf8Path<Utf8UnixEncoding>;

/// Represents a Unix-specific [`Utf8PathBuf`]
pub type Utf8UnixPathBuf = Utf8PathBuf<Utf8UnixEncoding>;

/// Represents a Unix-specific [`Utf8Encoding`]
#[derive(Copy, Clone)]
pub struct Utf8UnixEncoding;

impl private::Sealed for Utf8UnixEncoding {}

impl Utf8Encoding for Utf8UnixEncoding {
    type Components<'a> = Utf8UnixComponents<'a>;

    fn label() -> &'static str {
        "unix"
    }

    fn components(path: &str) -> Self::Components<'_> {
        Utf8UnixComponents::new(path)
    }

    fn hash<H: Hasher>(path: &str, h: &mut H) {
        UnixEncoding::hash(path.as_bytes(), h);
    }

    fn push(current_path: &mut String, path: &str) {
        unsafe {
            UnixEncoding::push(current_path.as_mut_vec(), path.as_bytes());
        }
    }

    fn push_checked(current_path: &mut String, path: &str) -> Result<(), CheckedPathError> {
        unsafe { UnixEncoding::push_checked(current_path.as_mut_vec(), path.as_bytes()) }
    }
}

impl fmt::Debug for Utf8UnixEncoding {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Utf8UnixEncoding").finish()
    }
}

impl fmt::Display for Utf8UnixEncoding {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Utf8UnixEncoding")
    }
}

impl<T> Utf8Path<T>
where
    T: Utf8Encoding,
{
    /// Returns true if the encoding for the path is for Unix.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8UnixPath, Utf8WindowsPath};
    ///
    /// assert!(Utf8UnixPath::new("/some/path").has_unix_encoding());
    /// assert!(!Utf8WindowsPath::new(r"\some\path").has_unix_encoding());
    /// ```
    pub fn has_unix_encoding(&self) -> bool {
        T::label() == Utf8UnixEncoding::label()
    }

    /// Creates an owned [`Utf8PathBuf`] like `self` but using [`Utf8UnixEncoding`].
    ///
    /// See [`Utf8Path::with_encoding`] for more information.
    pub fn with_unix_encoding(&self) -> Utf8PathBuf<Utf8UnixEncoding> {
        self.with_encoding()
    }

    /// Creates an owned [`Utf8PathBuf`] like `self` but using [`Utf8UnixEncoding`], ensuring it is
    /// a valid Unix path.
    ///
    /// See [`Utf8Path::with_encoding_checked`] for more information.
    pub fn with_unix_encoding_checked(
        &self,
    ) -> Result<Utf8PathBuf<Utf8UnixEncoding>, CheckedPathError> {
        self.with_encoding_checked()
    }
}

impl Utf8UnixPath {
    pub fn to_typed_path(&self) -> Utf8TypedPath<'_> {
        Utf8TypedPath::unix(self)
    }

    pub fn to_typed_path_buf(&self) -> Utf8TypedPathBuf {
        Utf8TypedPathBuf::from_unix(self)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn push_should_replace_current_path_with_provided_path_if_provided_path_is_absolute() {
        // Empty current path will just become the provided path
        let mut current_path = String::new();
        Utf8UnixEncoding::push(&mut current_path, "/abc");
        assert_eq!(current_path, "/abc");

        // Non-empty relative current path will be replaced with the provided path
        let mut current_path = String::from("some/path");
        Utf8UnixEncoding::push(&mut current_path, "/abc");
        assert_eq!(current_path, "/abc");

        // Non-empty absolute current path will be replaced with the provided path
        let mut current_path = String::from("/some/path/");
        Utf8UnixEncoding::push(&mut current_path, "/abc");
        assert_eq!(current_path, "/abc");
    }

    #[test]
    fn push_should_append_path_to_current_path_with_a_separator_if_provided_path_is_relative() {
        // Empty current path will just become the provided path
        let mut current_path = String::new();
        Utf8UnixEncoding::push(&mut current_path, "abc");
        assert_eq!(current_path, "abc");

        // Non-empty current path will have provided path appended
        let mut current_path = String::from("some/path");
        Utf8UnixEncoding::push(&mut current_path, "abc");
        assert_eq!(current_path, "some/path/abc");

        // Non-empty current path ending in separator will have provided path appended without sep
        let mut current_path = String::from("some/path/");
        Utf8UnixEncoding::push(&mut current_path, "abc");
        assert_eq!(current_path, "some/path/abc");
    }

    #[test]
    fn push_checked_should_fail_if_providing_an_absolute_path() {
        // Empty current path will fail when pushing an absolute path
        let mut current_path = String::new();
        assert_eq!(
            Utf8UnixEncoding::push_checked(&mut current_path, "/abc"),
            Err(CheckedPathError::UnexpectedRoot)
        );
        assert_eq!(current_path, "");

        // Non-empty relative current path will fail when pushing an absolute path
        let mut current_path = String::from("some/path");
        assert_eq!(
            Utf8UnixEncoding::push_checked(&mut current_path, "/abc"),
            Err(CheckedPathError::UnexpectedRoot)
        );
        assert_eq!(current_path, "some/path");

        // Non-empty absolute current path will fail when pushing an absolute path
        let mut current_path = String::from("/some/path/");
        assert_eq!(
            Utf8UnixEncoding::push_checked(&mut current_path, "/abc"),
            Err(CheckedPathError::UnexpectedRoot)
        );
        assert_eq!(current_path, "/some/path/");
    }

    #[test]
    fn push_checked_should_fail_if_providing_a_path_with_disallowed_filename_characters() {
        // Empty current path will fail when pushing a path containing disallowed filename chars
        let mut current_path = String::new();
        assert_eq!(
            Utf8UnixEncoding::push_checked(&mut current_path, "some/inva\0lid/path"),
            Err(CheckedPathError::InvalidFilename)
        );
        assert_eq!(current_path, "");

        // Non-empty relative current path will fail when pushing a path containing disallowed
        // filename bytes
        let mut current_path = String::from("some/path");
        assert_eq!(
            Utf8UnixEncoding::push_checked(&mut current_path, "some/inva\0lid/path"),
            Err(CheckedPathError::InvalidFilename)
        );
        assert_eq!(current_path, "some/path");

        // Non-empty absolute current path will fail when pushing a path containing disallowed
        // filename bytes
        let mut current_path = String::from("/some/path/");
        assert_eq!(
            Utf8UnixEncoding::push_checked(&mut current_path, "some/inva\0lid/path"),
            Err(CheckedPathError::InvalidFilename)
        );
        assert_eq!(current_path, "/some/path/");
    }

    #[test]
    fn push_checked_should_fail_if_providing_a_path_that_would_escape_the_current_path() {
        // Empty current path will fail when pushing a path that would escape
        let mut current_path = String::new();
        assert_eq!(
            Utf8UnixEncoding::push_checked(&mut current_path, ".."),
            Err(CheckedPathError::PathTraversalAttack)
        );
        assert_eq!(current_path, "");

        // Non-empty relative current path will fail when pushing a path that would escape
        let mut current_path = String::from("some/path");
        assert_eq!(
            Utf8UnixEncoding::push_checked(&mut current_path, ".."),
            Err(CheckedPathError::PathTraversalAttack)
        );
        assert_eq!(current_path, "some/path");

        // Non-empty absolute current path will fail when pushing a path that would escape
        let mut current_path = String::from("/some/path/");
        assert_eq!(
            Utf8UnixEncoding::push_checked(&mut current_path, ".."),
            Err(CheckedPathError::PathTraversalAttack)
        );
        assert_eq!(current_path, "/some/path/");
    }

    #[test]
    fn push_checked_should_append_path_to_current_path_with_a_separator_if_does_not_violate_rules()
    {
        // Pushing a path that contains parent dirs, but does not escape the current path,
        // should succeed
        let mut current_path = String::new();
        assert_eq!(
            Utf8UnixEncoding::push_checked(&mut current_path, "abc/../def/."),
            Ok(()),
        );
        assert_eq!(current_path, "abc/../def/.");

        let mut current_path = String::from("some/path");
        assert_eq!(
            Utf8UnixEncoding::push_checked(&mut current_path, "abc/../def/."),
            Ok(()),
        );
        assert_eq!(current_path, "some/path/abc/../def/.");

        let mut current_path = String::from("/some/path/");
        assert_eq!(
            Utf8UnixEncoding::push_checked(&mut current_path, "abc/../def/."),
            Ok(()),
        );
        assert_eq!(current_path, "/some/path/abc/../def/.");
    }
}
