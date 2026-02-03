mod components;

use core::fmt;
use core::hash::Hasher;

pub use components::*;

use crate::common::CheckedPathError;
use crate::no_std_compat::*;
use crate::typed::{Utf8TypedPath, Utf8TypedPathBuf};
use crate::{private, Encoding, Utf8Encoding, Utf8Path, Utf8PathBuf, WindowsEncoding};

/// Represents a Windows-specific [`Utf8Path`]
pub type Utf8WindowsPath = Utf8Path<Utf8WindowsEncoding>;

/// Represents a Windows-specific [`Utf8PathBuf`]
pub type Utf8WindowsPathBuf = Utf8PathBuf<Utf8WindowsEncoding>;

/// Represents a Windows-specific [`Utf8Encoding`]
#[derive(Copy, Clone)]
pub struct Utf8WindowsEncoding;

impl private::Sealed for Utf8WindowsEncoding {}

impl Utf8Encoding for Utf8WindowsEncoding {
    type Components<'a> = Utf8WindowsComponents<'a>;

    fn label() -> &'static str {
        "windows"
    }

    fn components(path: &str) -> Self::Components<'_> {
        Utf8WindowsComponents::new(path)
    }

    fn hash<H: Hasher>(path: &str, h: &mut H) {
        WindowsEncoding::hash(path.as_bytes(), h);
    }

    fn push(current_path: &mut String, path: &str) {
        unsafe {
            WindowsEncoding::push(current_path.as_mut_vec(), path.as_bytes());
        }
    }

    fn push_checked(current_path: &mut String, path: &str) -> Result<(), CheckedPathError> {
        unsafe { WindowsEncoding::push_checked(current_path.as_mut_vec(), path.as_bytes()) }
    }
}

impl fmt::Debug for Utf8WindowsEncoding {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Utf8WindowsEncoding").finish()
    }
}

impl fmt::Display for Utf8WindowsEncoding {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Utf8WindowsEncoding")
    }
}

impl<T> Utf8Path<T>
where
    T: Utf8Encoding,
{
    /// Returns true if the encoding for the path is for Windows.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8UnixPath, Utf8WindowsPath};
    ///
    /// assert!(!Utf8UnixPath::new("/some/path").has_windows_encoding());
    /// assert!(Utf8WindowsPath::new(r"\some\path").has_windows_encoding());
    /// ```
    pub fn has_windows_encoding(&self) -> bool {
        T::label() == Utf8WindowsEncoding::label()
    }

    /// Creates an owned [`Utf8PathBuf`] like `self` but using [`Utf8WindowsEncoding`].
    ///
    /// See [`Utf8Path::with_encoding`] for more information.
    pub fn with_windows_encoding(&self) -> Utf8PathBuf<Utf8WindowsEncoding> {
        self.with_encoding()
    }

    /// Creates an owned [`Utf8PathBuf`] like `self` but using [`Utf8WindowsEncoding`], ensuring it
    /// is a valid Windows path.
    ///
    /// See [`Utf8Path::with_encoding_checked`] for more information.
    pub fn with_windows_encoding_checked(
        &self,
    ) -> Result<Utf8PathBuf<Utf8WindowsEncoding>, CheckedPathError> {
        self.with_encoding_checked()
    }
}

impl Utf8WindowsPath {
    pub fn to_typed_path(&self) -> Utf8TypedPath<'_> {
        Utf8TypedPath::windows(self)
    }

    pub fn to_typed_path_buf(&self) -> Utf8TypedPathBuf {
        Utf8TypedPathBuf::from_windows(self)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn push_checked_should_fail_if_providing_an_absolute_path() {
        // Empty current path will fail when pushing an absolute path
        let mut current_path = String::new();
        assert_eq!(
            Utf8WindowsEncoding::push_checked(&mut current_path, r"\abc"),
            Err(CheckedPathError::UnexpectedRoot)
        );
        assert_eq!(current_path, "");

        // Non-empty relative current path will fail when pushing an absolute path
        let mut current_path = String::from(r"some\path");
        assert_eq!(
            Utf8WindowsEncoding::push_checked(&mut current_path, r"\abc"),
            Err(CheckedPathError::UnexpectedRoot)
        );
        assert_eq!(current_path, r"some\path");

        // Non-empty absolute current path will fail when pushing an absolute path
        let mut current_path = String::from(r"\some\path\");
        assert_eq!(
            Utf8WindowsEncoding::push_checked(&mut current_path, r"\abc"),
            Err(CheckedPathError::UnexpectedRoot)
        );
        assert_eq!(current_path, r"\some\path\");
    }

    #[test]
    fn push_checked_should_fail_if_providing_a_path_with_an_embedded_prefix() {
        // Empty current path will fail when pushing a path with a prefix
        let mut current_path = String::new();
        assert_eq!(
            Utf8WindowsEncoding::push_checked(&mut current_path, r"C:abc"),
            Err(CheckedPathError::UnexpectedPrefix)
        );
        assert_eq!(current_path, "");

        // Non-empty relative current path will fail when pushing a path with a prefix
        let mut current_path = String::from(r"some\path");
        assert_eq!(
            Utf8WindowsEncoding::push_checked(&mut current_path, r"C:abc"),
            Err(CheckedPathError::UnexpectedPrefix)
        );
        assert_eq!(current_path, r"some\path");

        // Non-empty absolute current path will fail when pushing a path with a prefix
        let mut current_path = String::from(r"\some\path\");
        assert_eq!(
            Utf8WindowsEncoding::push_checked(&mut current_path, r"C:abc"),
            Err(CheckedPathError::UnexpectedPrefix)
        );
        assert_eq!(current_path, r"\some\path\");
    }

    #[test]
    fn push_checked_should_fail_if_providing_a_path_with_disallowed_filename_bytes() {
        // Empty current path will fail when pushing a path containing disallowed filename bytes
        let mut current_path = String::new();
        assert_eq!(
            Utf8WindowsEncoding::push_checked(&mut current_path, r"some\inva|lid\path"),
            Err(CheckedPathError::InvalidFilename)
        );
        assert_eq!(current_path, "");

        // Non-empty relative current path will fail when pushing a path containing disallowed
        // filename bytes
        let mut current_path = String::from(r"some\path");
        assert_eq!(
            Utf8WindowsEncoding::push_checked(&mut current_path, r"some\inva|lid\path"),
            Err(CheckedPathError::InvalidFilename)
        );
        assert_eq!(current_path, r"some\path");

        // Non-empty absolute current path will fail when pushing a path containing disallowed
        // filename bytes
        let mut current_path = String::from(r"\some\path\");
        assert_eq!(
            Utf8WindowsEncoding::push_checked(&mut current_path, r"some\inva|lid\path"),
            Err(CheckedPathError::InvalidFilename)
        );
        assert_eq!(current_path, r"\some\path\");
    }

    #[test]
    fn push_checked_should_fail_if_providing_a_path_that_would_escape_the_current_path() {
        // Empty current path will fail when pushing a path that would escape
        let mut current_path = String::new();
        assert_eq!(
            Utf8WindowsEncoding::push_checked(&mut current_path, ".."),
            Err(CheckedPathError::PathTraversalAttack)
        );
        assert_eq!(current_path, "");

        // Non-empty relative current path will fail when pushing a path that would escape
        let mut current_path = String::from(r"some\path");
        assert_eq!(
            Utf8WindowsEncoding::push_checked(&mut current_path, ".."),
            Err(CheckedPathError::PathTraversalAttack)
        );
        assert_eq!(current_path, r"some\path");

        // Non-empty absolute current path will fail when pushing a path that would escape
        let mut current_path = String::from(r"\some\path\");
        assert_eq!(
            Utf8WindowsEncoding::push_checked(&mut current_path, ".."),
            Err(CheckedPathError::PathTraversalAttack)
        );
        assert_eq!(current_path, r"\some\path\");
    }

    #[test]
    fn push_checked_should_append_path_to_current_path_with_a_separator_if_does_not_violate_rules()
    {
        // Pushing a path that contains parent dirs, but does not escape the current path,
        // should succeed
        let mut current_path = String::new();
        assert_eq!(
            Utf8WindowsEncoding::push_checked(&mut current_path, r"abc\..\def\."),
            Ok(()),
        );
        assert_eq!(current_path, r"abc\..\def\.");

        let mut current_path = String::from(r"some\path");
        assert_eq!(
            Utf8WindowsEncoding::push_checked(&mut current_path, r"abc\..\def\."),
            Ok(()),
        );
        assert_eq!(current_path, r"some\path\abc\..\def\.");

        let mut current_path = String::from(r"\some\path\");
        assert_eq!(
            Utf8WindowsEncoding::push_checked(&mut current_path, r"abc\..\def\."),
            Ok(()),
        );
        assert_eq!(current_path, r"\some\path\abc\..\def\.");
    }
}
