// Copyright 2012-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Ported from the pathdiff crate, which adapted the original rustc's
// path_relative_from
// https://github.com/Manishearth/pathdiff/blob/master/src/lib.rs
// https://github.com/rust-lang/rust/blob/e1d0de82cc40b666b88d4a6d2c9dcbc81d7ed27f/src/librustc_back/rpath.rs#L116-L158

use std::error;
use std::fmt;
use std::path::{Path, PathBuf};

use crate::{Component, RelativePathBuf};

// Prevent downstream implementations, so methods may be added without backwards
// breaking changes.
mod sealed {
    use std::path::{Path, PathBuf};

    pub trait Sealed {}

    impl Sealed for Path {}
    impl Sealed for PathBuf {}
}

/// An error raised when attempting to convert a path using
/// [`PathExt::relative_to`].
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RelativeToError {
    kind: RelativeToErrorKind,
}

/// Error kind for [`RelativeToError`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[non_exhaustive]
enum RelativeToErrorKind {
    /// Non-utf8 component in path.
    NonUtf8,
    /// Mismatching path prefixes.
    PrefixMismatch,
    /// A provided path is ambiguous, in that there is no way to determine which
    /// components should be added from one path to the other to traverse it.
    ///
    /// For example, `.` is ambiguous relative to `../..` because we don't know
    /// the names of the components being traversed.
    AmbiguousTraversal,
    /// This is a catch-all error since we don't control the `std::path` API a
    /// Components iterator might decide (intentionally or not) to produce
    /// components which violates its own contract.
    ///
    /// In particular we rely on only relative components being produced after
    /// the absolute prefix has been consumed.
    IllegalComponent,
}

impl fmt::Display for RelativeToError {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match self.kind {
            RelativeToErrorKind::NonUtf8 => "path contains non-utf8 component".fmt(fmt),
            RelativeToErrorKind::PrefixMismatch => {
                "paths contain different absolute prefixes".fmt(fmt)
            }
            RelativeToErrorKind::AmbiguousTraversal => {
                "path traversal cannot be determined".fmt(fmt)
            }
            RelativeToErrorKind::IllegalComponent => "path contains illegal components".fmt(fmt),
        }
    }
}

impl error::Error for RelativeToError {}

impl From<RelativeToErrorKind> for RelativeToError {
    #[inline]
    fn from(kind: RelativeToErrorKind) -> Self {
        Self { kind }
    }
}

/// Extension methods for [`Path`] and [`PathBuf`] to for building and
/// interacting with [`RelativePath`].
///
/// [`RelativePath`]: crate::RelativePath
pub trait PathExt: sealed::Sealed {
    /// Build a relative path from the provided directory to `self`.
    ///
    /// Producing a relative path like this is a logical operation and does not
    /// guarantee that the constructed path corresponds to what the filesystem
    /// would do. On Linux for example symbolic links could mean that the
    /// logical path doesn't correspond to the filesystem path.
    ///
    /// # Examples
    ///
    /// ```
    /// use std::path::Path;
    /// use relative_path::{RelativePath, PathExt};
    ///
    /// let baz = Path::new("/foo/bar/baz");
    /// let bar = Path::new("/foo/bar");
    /// let qux = Path::new("/foo/bar/qux");
    ///
    /// assert_eq!(bar.relative_to(baz)?, RelativePath::new("../"));
    /// assert_eq!(baz.relative_to(bar)?, RelativePath::new("baz"));
    /// assert_eq!(qux.relative_to(baz)?, RelativePath::new("../qux"));
    /// assert_eq!(baz.relative_to(qux)?, RelativePath::new("../baz"));
    /// assert_eq!(bar.relative_to(qux)?, RelativePath::new("../"));
    /// # Ok::<_, relative_path::RelativeToError>(())
    /// ```
    ///
    /// # Errors
    ///
    /// Errors in case the provided path contains components which cannot be
    /// converted into a relative path as needed, such as non-utf8 data.
    fn relative_to<P>(&self, root: P) -> Result<RelativePathBuf, RelativeToError>
    where
        P: AsRef<Path>;
}

impl PathExt for Path {
    fn relative_to<P>(&self, root: P) -> Result<RelativePathBuf, RelativeToError>
    where
        P: AsRef<Path>,
    {
        use std::path::Component::{CurDir, Normal, ParentDir, Prefix, RootDir};

        // Helper function to convert from a std::path::Component to a
        // relative_path::Component.
        fn std_to_c(c: std::path::Component<'_>) -> Result<Component<'_>, RelativeToError> {
            Ok(match c {
                CurDir => Component::CurDir,
                ParentDir => Component::ParentDir,
                Normal(n) => Component::Normal(n.to_str().ok_or(RelativeToErrorKind::NonUtf8)?),
                _ => return Err(RelativeToErrorKind::IllegalComponent.into()),
            })
        }

        let root = root.as_ref();
        let mut a_it = self.components();
        let mut b_it = root.components();

        // Ensure that the two paths are both either relative, or have the same
        // prefix. Strips any common prefix the two paths do have. Prefixes are
        // platform dependent, but different prefixes would for example indicate
        // paths for different drives on Windows.
        let (a_head, b_head) = loop {
            match (a_it.next(), b_it.next()) {
                (Some(RootDir), Some(RootDir)) => (),
                (Some(Prefix(a)), Some(Prefix(b))) if a == b => (),
                (Some(Prefix(_) | RootDir), _) | (_, Some(Prefix(_) | RootDir)) => {
                    return Err(RelativeToErrorKind::PrefixMismatch.into());
                }
                (None, None) => break (None, None),
                (a, b) if a != b => break (a, b),
                _ => (),
            }
        };

        let mut a_it = a_head.into_iter().chain(a_it);
        let mut b_it = b_head.into_iter().chain(b_it);
        let mut buf = RelativePathBuf::new();

        loop {
            let a = if let Some(a) = a_it.next() {
                a
            } else {
                for _ in b_it {
                    buf.push(Component::ParentDir);
                }

                break;
            };

            match b_it.next() {
                Some(CurDir) => buf.push(std_to_c(a)?),
                Some(ParentDir) => {
                    return Err(RelativeToErrorKind::AmbiguousTraversal.into());
                }
                root => {
                    if root.is_some() {
                        buf.push(Component::ParentDir);
                    }

                    for comp in b_it {
                        match comp {
                            ParentDir => {
                                if !buf.pop() {
                                    return Err(RelativeToErrorKind::AmbiguousTraversal.into());
                                }
                            }
                            CurDir => (),
                            _ => buf.push(Component::ParentDir),
                        }
                    }

                    buf.push(std_to_c(a)?);

                    for c in a_it {
                        buf.push(std_to_c(c)?);
                    }

                    break;
                }
            }
        }

        Ok(buf)
    }
}

impl PathExt for PathBuf {
    #[inline]
    fn relative_to<P>(&self, root: P) -> Result<RelativePathBuf, RelativeToError>
    where
        P: AsRef<Path>,
    {
        self.as_path().relative_to(root)
    }
}

#[cfg(test)]
mod tests {
    use std::path::Path;

    use super::{PathExt, RelativeToErrorKind};
    use crate::{RelativePathBuf, RelativeToError};

    macro_rules! assert_relative_to {
        ($path:expr, $base:expr, Ok($expected:expr) $(,)?) => {
            assert_eq!(
                Path::new($path).relative_to($base),
                Ok(RelativePathBuf::from($expected))
            );
        };

        ($path:expr, $base:expr, Err($expected:ident) $(,)?) => {
            assert_eq!(
                Path::new($path).relative_to($base),
                Err(RelativeToError::from(RelativeToErrorKind::$expected))
            );
        };
    }

    #[cfg(windows)]
    macro_rules! abs {
        ($path:expr) => {
            Path::new(concat!("C:\\", $path))
        };
    }

    #[cfg(not(windows))]
    macro_rules! abs {
        ($path:expr) => {
            Path::new(concat!("/", $path))
        };
    }

    #[test]
    #[cfg(windows)]
    fn test_different_prefixes() {
        assert_relative_to!("C:\\repo", "D:\\repo", Err(PrefixMismatch),);
        assert_relative_to!("C:\\repo", "C:\\repo", Ok(""));
        assert_relative_to!(
            "\\\\server\\share\\repo",
            "\\\\server2\\share\\repo",
            Err(PrefixMismatch),
        );
    }

    #[test]
    fn test_absolute() {
        assert_relative_to!(abs!("foo"), abs!("bar"), Ok("../foo"));
        assert_relative_to!("foo", "bar", Ok("../foo"));
        assert_relative_to!(abs!("foo"), "bar", Err(PrefixMismatch));
        assert_relative_to!("foo", abs!("bar"), Err(PrefixMismatch));
    }

    #[test]
    fn test_identity() {
        assert_relative_to!(".", ".", Ok(""));
        assert_relative_to!("../foo", "../foo", Ok(""));
        assert_relative_to!("./foo", "./foo", Ok(""));
        assert_relative_to!("/foo", "/foo", Ok(""));
        assert_relative_to!("foo", "foo", Ok(""));

        assert_relative_to!("../foo/bar/baz", "../foo/bar/baz", Ok(""));
        assert_relative_to!("foo/bar/baz", "foo/bar/baz", Ok(""));
    }

    #[test]
    fn test_subset() {
        assert_relative_to!("foo", "fo", Ok("../foo"));
        assert_relative_to!("fo", "foo", Ok("../fo"));
    }

    #[test]
    fn test_empty() {
        assert_relative_to!("", "", Ok(""));
        assert_relative_to!("foo", "", Ok("foo"));
        assert_relative_to!("", "foo", Ok(".."));
    }

    #[test]
    fn test_relative() {
        assert_relative_to!("../foo", "../bar", Ok("../foo"));
        assert_relative_to!("../foo", "../foo/bar/baz", Ok("../.."));
        assert_relative_to!("../foo/bar/baz", "../foo", Ok("bar/baz"));

        assert_relative_to!("foo/bar/baz", "foo", Ok("bar/baz"));
        assert_relative_to!("foo/bar/baz", "foo/bar", Ok("baz"));
        assert_relative_to!("foo/bar/baz", "foo/bar/baz", Ok(""));
        assert_relative_to!("foo/bar/baz", "foo/bar/baz/", Ok(""));

        assert_relative_to!("foo/bar/baz/", "foo", Ok("bar/baz"));
        assert_relative_to!("foo/bar/baz/", "foo/bar", Ok("baz"));
        assert_relative_to!("foo/bar/baz/", "foo/bar/baz", Ok(""));
        assert_relative_to!("foo/bar/baz/", "foo/bar/baz/", Ok(""));

        assert_relative_to!("foo/bar/baz", "foo/", Ok("bar/baz"));
        assert_relative_to!("foo/bar/baz", "foo/bar/", Ok("baz"));
        assert_relative_to!("foo/bar/baz", "foo/bar/baz", Ok(""));
    }

    #[test]
    fn test_current_directory() {
        assert_relative_to!(".", "foo", Ok("../."));
        assert_relative_to!("foo", ".", Ok("foo"));
        assert_relative_to!("/foo", "/.", Ok("foo"));
    }

    #[test]
    fn assert_does_not_skip_parents() {
        assert_relative_to!("some/path", "some/foo/baz/path", Ok("../../../path"));
        assert_relative_to!("some/path", "some/foo/bar/../baz/path", Ok("../../../path"));
    }

    #[test]
    fn test_ambiguous_paths() {
        // Parent directory name is unknown, so trying to make current directory
        // relative to it is impossible.
        assert_relative_to!(".", "../..", Err(AmbiguousTraversal));
        assert_relative_to!(".", "a/../..", Err(AmbiguousTraversal));
        // Common prefixes are ok.
        assert_relative_to!("../a/..", "../a/../b", Ok(".."));
        assert_relative_to!("../a/../b", "../a/..", Ok("b"));
    }
}
