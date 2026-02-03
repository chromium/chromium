mod components;

use core::fmt;
use core::hash::Hasher;

pub use components::*;

use super::constants::*;
use crate::common::CheckedPathError;
use crate::no_std_compat::*;
use crate::typed::{TypedPath, TypedPathBuf};
use crate::{private, Components, Encoding, Path, PathBuf};

/// Represents a Unix-specific [`Path`]
pub type UnixPath = Path<UnixEncoding>;

/// Represents a Unix-specific [`PathBuf`]
pub type UnixPathBuf = PathBuf<UnixEncoding>;

/// Represents a Unix-specific [`Encoding`]
#[derive(Copy, Clone)]
pub struct UnixEncoding;

impl private::Sealed for UnixEncoding {}

impl Encoding for UnixEncoding {
    type Components<'a> = UnixComponents<'a>;

    fn label() -> &'static str {
        "unix"
    }

    fn components(path: &[u8]) -> Self::Components<'_> {
        UnixComponents::new(path)
    }

    fn hash<H: Hasher>(path: &[u8], h: &mut H) {
        let mut component_start = 0;
        let mut bytes_hashed = 0;

        for i in 0..path.len() {
            let is_sep = path[i] == SEPARATOR as u8;
            if is_sep {
                if i > component_start {
                    let to_hash = &path[component_start..i];
                    h.write(to_hash);
                    bytes_hashed += to_hash.len();
                }

                // skip over separator and optionally a following CurDir item
                // since components() would normalize these away.
                component_start = i + 1;

                let tail = &path[component_start..];

                component_start += match tail {
                    [b'.'] => 1,
                    [b'.', sep, ..] if *sep == SEPARATOR as u8 => 1,
                    _ => 0,
                };
            }
        }

        if component_start < path.len() {
            let to_hash = &path[component_start..];
            h.write(to_hash);
            bytes_hashed += to_hash.len();
        }

        h.write_usize(bytes_hashed);
    }

    fn push(current_path: &mut Vec<u8>, path: &[u8]) {
        if path.is_empty() {
            return;
        }

        // Absolute path will replace entirely, otherwise check if we need to add our separator,
        // and add it if the separator is missing
        //
        // Otherwise, if our current path is not empty, we will append the provided path
        // to the end with a separator inbetween
        if Self::components(path).is_absolute() {
            current_path.clear();
        } else if !current_path.is_empty() && !current_path.ends_with(&[SEPARATOR as u8]) {
            current_path.push(SEPARATOR as u8);
        }

        current_path.extend_from_slice(path);
    }

    fn push_checked(current_path: &mut Vec<u8>, path: &[u8]) -> Result<(), CheckedPathError> {
        // As we scan through path components, we maintain a count of normal components that
        // have not been popped off as a result of a parent component. If we ever reach a
        // parent component without any preceding normal components remaining, this violates
        // pushing onto our path and represents a path traversal attack.
        let mut normal_cnt = 0;
        for component in UnixPath::new(path).components() {
            match component {
                UnixComponent::RootDir => return Err(CheckedPathError::UnexpectedRoot),
                UnixComponent::ParentDir if normal_cnt == 0 => {
                    return Err(CheckedPathError::PathTraversalAttack)
                }
                UnixComponent::ParentDir => normal_cnt -= 1,
                UnixComponent::Normal(bytes) => {
                    for b in bytes {
                        if DISALLOWED_FILENAME_BYTES.contains(b) {
                            return Err(CheckedPathError::InvalidFilename);
                        }
                    }
                    normal_cnt += 1;
                }
                _ => continue,
            }
        }

        Self::push(current_path, path);
        Ok(())
    }
}

impl fmt::Debug for UnixEncoding {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("UnixEncoding").finish()
    }
}

impl fmt::Display for UnixEncoding {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "UnixEncoding")
    }
}

impl<T> Path<T>
where
    T: Encoding,
{
    /// Returns true if the encoding for the path is for Unix.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{UnixPath, WindowsPath};
    ///
    /// assert!(UnixPath::new("/some/path").has_unix_encoding());
    /// assert!(!WindowsPath::new(r"\some\path").has_unix_encoding());
    /// ```
    pub fn has_unix_encoding(&self) -> bool {
        T::label() == UnixEncoding::label()
    }

    /// Creates an owned [`PathBuf`] like `self` but using [`UnixEncoding`].
    ///
    /// See [`Path::with_encoding`] for more information.
    pub fn with_unix_encoding(&self) -> PathBuf<UnixEncoding> {
        self.with_encoding()
    }

    /// Creates an owned [`PathBuf`] like `self` but using [`UnixEncoding`], ensuring it is a valid
    /// Unix path.
    ///
    /// See [`Path::with_encoding_checked`] for more information.
    pub fn with_unix_encoding_checked(&self) -> Result<PathBuf<UnixEncoding>, CheckedPathError> {
        self.with_encoding_checked()
    }
}

impl UnixPath {
    pub fn to_typed_path(&self) -> TypedPath<'_> {
        TypedPath::unix(self)
    }

    pub fn to_typed_path_buf(&self) -> TypedPathBuf {
        TypedPathBuf::from_unix(self)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn push_should_replace_current_path_with_provided_path_if_provided_path_is_absolute() {
        // Empty current path will just become the provided path
        let mut current_path = vec![];
        UnixEncoding::push(&mut current_path, b"/abc");
        assert_eq!(current_path, b"/abc");

        // Non-empty relative current path will be replaced with the provided path
        let mut current_path = b"some/path".to_vec();
        UnixEncoding::push(&mut current_path, b"/abc");
        assert_eq!(current_path, b"/abc");

        // Non-empty absolute current path will be replaced with the provided path
        let mut current_path = b"/some/path/".to_vec();
        UnixEncoding::push(&mut current_path, b"/abc");
        assert_eq!(current_path, b"/abc");
    }

    #[test]
    fn push_should_append_path_to_current_path_with_a_separator_if_provided_path_is_relative() {
        // Empty current path will just become the provided path
        let mut current_path = vec![];
        UnixEncoding::push(&mut current_path, b"abc");
        assert_eq!(current_path, b"abc");

        // Non-empty current path will have provided path appended
        let mut current_path = b"some/path".to_vec();
        UnixEncoding::push(&mut current_path, b"abc");
        assert_eq!(current_path, b"some/path/abc");

        // Non-empty current path ending in separator will have provided path appended without sep
        let mut current_path = b"some/path/".to_vec();
        UnixEncoding::push(&mut current_path, b"abc");
        assert_eq!(current_path, b"some/path/abc");
    }

    #[test]
    fn push_checked_should_fail_if_providing_an_absolute_path() {
        // Empty current path will fail when pushing an absolute path
        let mut current_path = vec![];
        assert_eq!(
            UnixEncoding::push_checked(&mut current_path, b"/abc"),
            Err(CheckedPathError::UnexpectedRoot)
        );
        assert_eq!(current_path, b"");

        // Non-empty relative current path will fail when pushing an absolute path
        let mut current_path = b"some/path".to_vec();
        assert_eq!(
            UnixEncoding::push_checked(&mut current_path, b"/abc"),
            Err(CheckedPathError::UnexpectedRoot)
        );
        assert_eq!(current_path, b"some/path");

        // Non-empty absolute current path will fail when pushing an absolute path
        let mut current_path = b"/some/path/".to_vec();
        assert_eq!(
            UnixEncoding::push_checked(&mut current_path, b"/abc"),
            Err(CheckedPathError::UnexpectedRoot)
        );
        assert_eq!(current_path, b"/some/path/");
    }

    #[test]
    fn push_checked_should_fail_if_providing_a_path_with_disallowed_filename_bytes() {
        // Empty current path will fail when pushing a path containing disallowed filename bytes
        let mut current_path = vec![];
        assert_eq!(
            UnixEncoding::push_checked(&mut current_path, b"some/inva\0lid/path"),
            Err(CheckedPathError::InvalidFilename)
        );
        assert_eq!(current_path, b"");

        // Non-empty relative current path will fail when pushing a path containing disallowed
        // filename bytes
        let mut current_path = b"some/path".to_vec();
        assert_eq!(
            UnixEncoding::push_checked(&mut current_path, b"some/inva\0lid/path"),
            Err(CheckedPathError::InvalidFilename)
        );
        assert_eq!(current_path, b"some/path");

        // Non-empty absolute current path will fail when pushing a path containing disallowed
        // filename bytes
        let mut current_path = b"/some/path/".to_vec();
        assert_eq!(
            UnixEncoding::push_checked(&mut current_path, b"some/inva\0lid/path"),
            Err(CheckedPathError::InvalidFilename)
        );
        assert_eq!(current_path, b"/some/path/");
    }

    #[test]
    fn push_checked_should_fail_if_providing_a_path_that_would_escape_the_current_path() {
        // Empty current path will fail when pushing a path that would escape
        let mut current_path = vec![];
        assert_eq!(
            UnixEncoding::push_checked(&mut current_path, b".."),
            Err(CheckedPathError::PathTraversalAttack)
        );
        assert_eq!(current_path, b"");

        // Non-empty relative current path will fail when pushing a path that would escape
        let mut current_path = b"some/path".to_vec();
        assert_eq!(
            UnixEncoding::push_checked(&mut current_path, b".."),
            Err(CheckedPathError::PathTraversalAttack)
        );
        assert_eq!(current_path, b"some/path");

        // Non-empty absolute current path will fail when pushing a path that would escape
        let mut current_path = b"/some/path/".to_vec();
        assert_eq!(
            UnixEncoding::push_checked(&mut current_path, b".."),
            Err(CheckedPathError::PathTraversalAttack)
        );
        assert_eq!(current_path, b"/some/path/");
    }

    #[test]
    fn push_checked_should_append_path_to_current_path_with_a_separator_if_does_not_violate_rules()
    {
        // Pushing a path that contains parent dirs, but does not escape the current path,
        // should succeed
        let mut current_path = vec![];
        assert_eq!(
            UnixEncoding::push_checked(&mut current_path, b"abc/../def/."),
            Ok(()),
        );
        assert_eq!(current_path, b"abc/../def/.");

        let mut current_path = b"some/path".to_vec();
        assert_eq!(
            UnixEncoding::push_checked(&mut current_path, b"abc/../def/."),
            Ok(()),
        );
        assert_eq!(current_path, b"some/path/abc/../def/.");

        let mut current_path = b"/some/path/".to_vec();
        assert_eq!(
            UnixEncoding::push_checked(&mut current_path, b"abc/../def/."),
            Ok(()),
        );
        assert_eq!(current_path, b"/some/path/abc/../def/.");
    }
}
