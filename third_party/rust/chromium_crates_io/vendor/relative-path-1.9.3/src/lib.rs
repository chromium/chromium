//! [<img alt="github" src="https://img.shields.io/badge/github-udoprog/relative--path-8da0cb?style=for-the-badge&logo=github" height="20">](https://github.com/udoprog/relative-path)
//! [<img alt="crates.io" src="https://img.shields.io/crates/v/relative-path.svg?style=for-the-badge&color=fc8d62&logo=rust" height="20">](https://crates.io/crates/relative-path)
//! [<img alt="docs.rs" src="https://img.shields.io/badge/docs.rs-relative--path-66c2a5?style=for-the-badge&logoColor=white&logo=data:image/svg+xml;base64,PHN2ZyByb2xlPSJpbWciIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDUxMiA1MTIiPjxwYXRoIGZpbGw9IiNmNWY1ZjUiIGQ9Ik00ODguNiAyNTAuMkwzOTIgMjE0VjEwNS41YzAtMTUtOS4zLTI4LjQtMjMuNC0zMy43bC0xMDAtMzcuNWMtOC4xLTMuMS0xNy4xLTMuMS0yNS4zIDBsLTEwMCAzNy41Yy0xNC4xIDUuMy0yMy40IDE4LjctMjMuNCAzMy43VjIxNGwtOTYuNiAzNi4yQzkuMyAyNTUuNSAwIDI2OC45IDAgMjgzLjlWMzk0YzAgMTMuNiA3LjcgMjYuMSAxOS45IDMyLjJsMTAwIDUwYzEwLjEgNS4xIDIyLjEgNS4xIDMyLjIgMGwxMDMuOS01MiAxMDMuOSA1MmMxMC4xIDUuMSAyMi4xIDUuMSAzMi4yIDBsMTAwLTUwYzEyLjItNi4xIDE5LjktMTguNiAxOS45LTMyLjJWMjgzLjljMC0xNS05LjMtMjguNC0yMy40LTMzLjd6TTM1OCAyMTQuOGwtODUgMzEuOXYtNjguMmw4NS0zN3Y3My4zek0xNTQgMTA0LjFsMTAyLTM4LjIgMTAyIDM4LjJ2LjZsLTEwMiA0MS40LTEwMi00MS40di0uNnptODQgMjkxLjFsLTg1IDQyLjV2LTc5LjFsODUtMzguOHY3NS40em0wLTExMmwtMTAyIDQxLjQtMTAyLTQxLjR2LS42bDEwMi0zOC4yIDEwMiAzOC4ydi42em0yNDAgMTEybC04NSA0Mi41di03OS4xbDg1LTM4Ljh2NzUuNHptMC0xMTJsLTEwMiA0MS40LTEwMi00MS40di0uNmwxMDItMzguMiAxMDIgMzguMnYuNnoiPjwvcGF0aD48L3N2Zz4K" height="20">](https://docs.rs/relative-path)
//!
//! Portable relative UTF-8 paths for Rust.
//!
//! This crate provides a module analogous to [`std::path`], with the following
//! characteristics:
//!
//! * The path separator is set to a fixed character (`/`), regardless of
//!   platform.
//! * Relative paths cannot represent a path in the filesystem without first
//!   specifying *what they are relative to* using functions such as [`to_path`]
//!   and [`to_logical_path`].
//! * Relative paths are always guaranteed to be valid UTF-8 strings.
//!
//! On top of this we support many operations that guarantee the same behavior
//! across platforms.
//!
//! For more utilities to manipulate relative paths, see the
//! [`relative-path-utils` crate].
//!
//! <br>
//!
//! ## Usage
//!
//! Add `relative-path` to your `Cargo.toml`:
//!
//! ```toml
//! relative-path = "1.9.2"
//! ```
//!
//! Start using relative paths:
//!
//! ```
//! use serde::{Serialize, Deserialize};
//! use relative_path::RelativePath;
//!
//! #[derive(Serialize, Deserialize)]
//! struct Manifest<'a> {
//!     #[serde(borrow)]
//!     source: &'a RelativePath,
//! }
//!
//! # Ok::<_, Box<dyn std::error::Error>>(())
//! ```
//!
//! <br>
//!
//! ## Serde Support
//!
//! This library includes serde support that can be enabled with the `serde`
//! feature.
//!
//! <br>
//!
//! ## Why is `std::path` a portability hazard?
//!
//! Path representations differ across platforms.
//!
//! * Windows permits using drive volumes (multiple roots) as a prefix (e.g.
//!   `"c:\"`) and backslash (`\`) as a separator.
//! * Unix references absolute paths from a single root and uses forward slash
//!   (`/`) as a separator.
//!
//! If we use `PathBuf`, Storing paths in a manifest would allow our application
//! to build and run on one platform but potentially not others.
//!
//! Consider the following data model and corresponding toml for a manifest:
//!
//! ```rust
//! use std::path::PathBuf;
//!
//! use serde::{Serialize, Deserialize};
//!
//! #[derive(Serialize, Deserialize)]
//! struct Manifest {
//!     source: PathBuf,
//! }
//! ```
//!
//! ```toml
//! source = "C:\\Users\\udoprog\\repo\\data\\source"
//! ```
//!
//! This will run for you (assuming `source` exists). So you go ahead and check
//! the manifest into git. The next day your Linux colleague calls you and
//! wonders what they have ever done to wrong you?
//!
//! So what went wrong? Well two things. You forgot to make the `source`
//! relative, so anyone at the company which has a different username than you
//! won't be able to use it. So you go ahead and fix that:
//!
//! ```toml
//! source = "data\\source"
//! ```
//!
//! But there is still one problem! A backslash (`\`) is only a legal path
//! separator on Windows. Luckily you learn that forward slashes are supported
//! both on Windows *and* Linux. So you opt for:
//!
//! ```toml
//! source = "data/source"
//! ```
//!
//! Things are working now. So all is well... Right? Sure, but we can do better.
//!
//! This crate provides types that work with *portable relative paths* (hence
//! the name). So by using [`RelativePath`] we can systematically help avoid
//! portability issues like the one above. Avoiding issues at the source is
//! preferably over spending 5 minutes of onboarding time on a theoretical
//! problem, hoping that your new hires will remember what to do if they ever
//! encounter it.
//!
//! Using [`RelativePathBuf`] we can fix our data model like this:
//!
//! ```rust
//! use relative_path::RelativePathBuf;
//! use serde::{Serialize, Deserialize};
//!
//! #[derive(Serialize, Deserialize)]
//! pub struct Manifest {
//!     source: RelativePathBuf,
//! }
//! ```
//!
//! And where it's used:
//!
//! ```rust,no_run
//! # use relative_path::RelativePathBuf;
//! # use serde::{Serialize, Deserialize};
//! # #[derive(Serialize, Deserialize)] pub struct Manifest { source: RelativePathBuf }
//! use std::fs;
//! use std::env::current_dir;
//!
//! let manifest: Manifest = todo!();
//!
//! let root = current_dir()?;
//! let source = manifest.source.to_path(&root);
//! let content = fs::read(&source)?;
//! # Ok::<_, Box<dyn std::error::Error>>(())
//! ```
//!
//! <br>
//!
//! ## Overview
//!
//! Conversion to a platform-specific [`Path`] happens through the [`to_path`]
//! and [`to_logical_path`] functions. Where you are required to specify the
//! path that prefixes the relative path. This can come from a function such as
//! [`std::env::current_dir`].
//!
//! ```rust
//! use std::env::current_dir;
//! use std::path::Path;
//!
//! use relative_path::RelativePath;
//!
//! let root = current_dir()?;
//!
//! # if cfg!(windows) {
//! // to_path unconditionally concatenates a relative path with its base:
//! let relative_path = RelativePath::new("../foo/./bar");
//! let full_path = relative_path.to_path(&root);
//! assert_eq!(full_path, root.join("..\\foo\\.\\bar"));
//!
//! // to_logical_path tries to apply the logical operations that the relative
//! // path corresponds to:
//! let relative_path = RelativePath::new("../foo/./bar");
//! let full_path = relative_path.to_logical_path(&root);
//!
//! // Replicate the operation performed by `to_logical_path`.
//! let mut parent = root.clone();
//! parent.pop();
//! assert_eq!(full_path, parent.join("foo\\bar"));
//! # }
//! # Ok::<_, std::io::Error>(())
//! ```
//!
//! When two relative paths are compared to each other, their exact component
//! makeup determines equality.
//!
//! ```rust
//! use relative_path::RelativePath;
//!
//! assert_ne!(
//!     RelativePath::new("foo/bar/../baz"),
//!     RelativePath::new("foo/baz")
//! );
//! ```
//!
//! Using platform-specific path separators to construct relative paths is not
//! supported.
//!
//! Path separators from other platforms are simply treated as part of a
//! component:
//!
//! ```rust
//! use relative_path::RelativePath;
//!
//! assert_ne!(
//!     RelativePath::new("foo/bar"),
//!     RelativePath::new("foo\\bar")
//! );
//!
//! assert_eq!(1, RelativePath::new("foo\\bar").components().count());
//! assert_eq!(2, RelativePath::new("foo/bar").components().count());
//! ```
//!
//! To see if two relative paths are equivalent you can use [`normalize`]:
//!
//! ```rust
//! use relative_path::RelativePath;
//!
//! assert_eq!(
//!     RelativePath::new("foo/bar/../baz").normalize(),
//!     RelativePath::new("foo/baz").normalize(),
//! );
//! ```
//!
//! <br>
//!
//! ## Additional portability notes
//!
//! While relative paths avoid the most egregious portability issue, that
//! absolute paths will work equally unwell on all platforms. We cannot avoid
//! all. This section tries to document additional portability hazards that we
//! are aware of.
//!
//! [`RelativePath`], similarly to [`Path`], makes no guarantees that its
//! constituent components make up legal file names. While components are
//! strictly separated by slashes, we can still store things in them which may
//! not be used as legal paths on all platforms.
//!
//! * A `NUL` character is not permitted on unix platforms - this is a
//!   terminator in C-based filesystem APIs. Slash (`/`) is also used as a path
//!   separator.
//! * Windows has a number of [reserved characters and names][windows-reserved]
//!   (like `CON`, `PRN`, and `AUX`) which cannot legally be part of a
//!   filesystem component.
//! * Windows paths are [case-insensitive by default][windows-case]. So,
//!   `Foo.txt` and `foo.txt` are the same files on windows. But they are
//!   considered different paths on most unix systems.
//!
//! A relative path that *accidentally* contains a platform-specific components
//! will largely result in a nonsensical paths being generated in the hope that
//! they will fail fast during development and testing.
//!
//! ```rust
//! use relative_path::{RelativePath, PathExt};
//! use std::path::Path;
//!
//! if cfg!(windows) {
//!     assert_eq!(
//!         Path::new("foo\\c:\\bar\\baz"),
//!         RelativePath::new("c:\\bar\\baz").to_path("foo")
//!     );
//! }
//!
//! if cfg!(unix) {
//!     assert_eq!(
//!         Path::new("foo/bar/baz"),
//!         RelativePath::new("/bar/baz").to_path("foo")
//!     );
//! }
//!
//! assert_eq!(
//!     Path::new("foo").relative_to("bar")?,
//!     RelativePath::new("../foo"),
//! );
//! # Ok::<_, Box<dyn std::error::Error>>(())
//! ```
//!
//! [`None`]: https://doc.rust-lang.org/std/option/enum.Option.html
//! [`normalize`]: https://docs.rs/relative-path/1/relative_path/struct.RelativePath.html#method.normalize
//! [`Path`]: https://doc.rust-lang.org/std/path/struct.Path.html
//! [`RelativePath`]: https://docs.rs/relative-path/1/relative_path/struct.RelativePath.html
//! [`RelativePathBuf`]: https://docs.rs/relative-path/1/relative_path/struct.RelativePathBuf.html
//! [`std::env::current_dir`]: https://doc.rust-lang.org/std/env/fn.current_dir.html
//! [`std::path`]: https://doc.rust-lang.org/std/path/index.html
//! [`to_logical_path`]: https://docs.rs/relative-path/1/relative_path/struct.RelativePath.html#method.to_logical_path
//! [`to_path`]: https://docs.rs/relative-path/1/relative_path/struct.RelativePath.html#method.to_path
//! [windows-reserved]: https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx
//! [windows-case]: https://learn.microsoft.com/en-us/windows/wsl/case-sensitivity
//! [`relative-path-utils` crate]: https://docs.rs/relative-path-utils

// This file contains parts that are Copyright 2015 The Rust Project Developers, copied from:
// https://github.com/rust-lang/rust
// cb2a656cdfb6400ac0200c661267f91fabf237e2 src/libstd/path.rs

#![allow(clippy::manual_let_else)]
#![deny(missing_docs)]

mod path_ext;

#[cfg(test)]
mod tests;

pub use path_ext::{PathExt, RelativeToError};

use std::borrow::{Borrow, Cow};
use std::cmp;
use std::error;
use std::fmt;
use std::hash::{Hash, Hasher};
use std::iter::FromIterator;
use std::mem;
use std::ops;
use std::path;
use std::rc::Rc;
use std::str;
use std::sync::Arc;

const STEM_SEP: char = '.';
const CURRENT_STR: &str = ".";
const PARENT_STR: &str = "..";

const SEP: char = '/';

fn split_file_at_dot(input: &str) -> (Option<&str>, Option<&str>) {
    if input == PARENT_STR {
        return (Some(input), None);
    }

    let mut iter = input.rsplitn(2, STEM_SEP);

    let after = iter.next();
    let before = iter.next();

    if before == Some("") {
        (Some(input), None)
    } else {
        (before, after)
    }
}

// Iterate through `iter` while it matches `prefix`; return `None` if `prefix`
// is not a prefix of `iter`, otherwise return `Some(iter_after_prefix)` giving
// `iter` after having exhausted `prefix`.
fn iter_after<'a, 'b, I, J>(mut iter: I, mut prefix: J) -> Option<I>
where
    I: Iterator<Item = Component<'a>> + Clone,
    J: Iterator<Item = Component<'b>>,
{
    loop {
        let mut iter_next = iter.clone();
        match (iter_next.next(), prefix.next()) {
            (Some(x), Some(y)) if x == y => (),
            (Some(_) | None, Some(_)) => return None,
            (Some(_) | None, None) => return Some(iter),
        }
        iter = iter_next;
    }
}

/// A single path component.
///
/// Accessed using the [`RelativePath::components`] iterator.
///
/// # Examples
///
/// ```
/// use relative_path::{Component, RelativePath};
///
/// let path = RelativePath::new("foo/../bar/./baz");
/// let mut it = path.components();
///
/// assert_eq!(Some(Component::Normal("foo")), it.next());
/// assert_eq!(Some(Component::ParentDir), it.next());
/// assert_eq!(Some(Component::Normal("bar")), it.next());
/// assert_eq!(Some(Component::CurDir), it.next());
/// assert_eq!(Some(Component::Normal("baz")), it.next());
/// assert_eq!(None, it.next());
/// ```
#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub enum Component<'a> {
    /// The current directory `.`.
    CurDir,
    /// The parent directory `..`.
    ParentDir,
    /// A normal path component as a string.
    Normal(&'a str),
}

impl<'a> Component<'a> {
    /// Extracts the underlying [`str`] slice.
    ///
    /// [`str`]: prim@str
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::{RelativePath, Component};
    ///
    /// let path = RelativePath::new("./tmp/../foo/bar.txt");
    /// let components: Vec<_> = path.components().map(Component::as_str).collect();
    /// assert_eq!(&components, &[".", "tmp", "..", "foo", "bar.txt"]);
    /// ```
    #[must_use]
    pub fn as_str(self) -> &'a str {
        use self::Component::{CurDir, Normal, ParentDir};

        match self {
            CurDir => CURRENT_STR,
            ParentDir => PARENT_STR,
            Normal(name) => name,
        }
    }
}

/// [`AsRef<RelativePath>`] implementation for [`Component`].
///
/// # Examples
///
/// ```
/// use relative_path::RelativePath;
///
/// let mut it = RelativePath::new("../foo/bar").components();
///
/// let a = it.next().ok_or("a")?;
/// let b = it.next().ok_or("b")?;
/// let c = it.next().ok_or("c")?;
///
/// let a: &RelativePath = a.as_ref();
/// let b: &RelativePath = b.as_ref();
/// let c: &RelativePath = c.as_ref();
///
/// assert_eq!(a, "..");
/// assert_eq!(b, "foo");
/// assert_eq!(c, "bar");
///
/// # Ok::<_, Box<dyn std::error::Error>>(())
/// ```
impl AsRef<RelativePath> for Component<'_> {
    #[inline]
    fn as_ref(&self) -> &RelativePath {
        self.as_str().as_ref()
    }
}

/// Traverse the given components and apply to the provided stack.
///
/// This takes '.', and '..' into account. Where '.' doesn't change the stack, and '..' pops the
/// last item or further adds parent components.
#[inline(always)]
fn relative_traversal<'a, C>(buf: &mut RelativePathBuf, components: C)
where
    C: IntoIterator<Item = Component<'a>>,
{
    use self::Component::{CurDir, Normal, ParentDir};

    for c in components {
        match c {
            CurDir => (),
            ParentDir => match buf.components().next_back() {
                Some(Component::ParentDir) | None => {
                    buf.push(PARENT_STR);
                }
                _ => {
                    buf.pop();
                }
            },
            Normal(name) => {
                buf.push(name);
            }
        }
    }
}

/// Iterator over all the components in a relative path.
#[derive(Clone)]
pub struct Components<'a> {
    source: &'a str,
}

impl<'a> Iterator for Components<'a> {
    type Item = Component<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        self.source = self.source.trim_start_matches(SEP);

        let slice = match self.source.find(SEP) {
            Some(i) => {
                let (slice, rest) = self.source.split_at(i);
                self.source = rest.trim_start_matches(SEP);
                slice
            }
            None => mem::take(&mut self.source),
        };

        match slice {
            "" => None,
            CURRENT_STR => Some(Component::CurDir),
            PARENT_STR => Some(Component::ParentDir),
            slice => Some(Component::Normal(slice)),
        }
    }
}

impl<'a> DoubleEndedIterator for Components<'a> {
    fn next_back(&mut self) -> Option<Self::Item> {
        self.source = self.source.trim_end_matches(SEP);

        let slice = match self.source.rfind(SEP) {
            Some(i) => {
                let (rest, slice) = self.source.split_at(i + 1);
                self.source = rest.trim_end_matches(SEP);
                slice
            }
            None => mem::take(&mut self.source),
        };

        match slice {
            "" => None,
            CURRENT_STR => Some(Component::CurDir),
            PARENT_STR => Some(Component::ParentDir),
            slice => Some(Component::Normal(slice)),
        }
    }
}

impl<'a> Components<'a> {
    /// Construct a new component from the given string.
    fn new(source: &'a str) -> Components<'a> {
        Self { source }
    }

    /// Extracts a slice corresponding to the portion of the path remaining for iteration.
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePath;
    ///
    /// let mut components = RelativePath::new("tmp/foo/bar.txt").components();
    /// components.next();
    /// components.next();
    ///
    /// assert_eq!("bar.txt", components.as_relative_path());
    /// ```
    #[must_use]
    #[inline]
    pub fn as_relative_path(&self) -> &'a RelativePath {
        RelativePath::new(self.source)
    }
}

impl<'a> cmp::PartialEq for Components<'a> {
    fn eq(&self, other: &Components<'a>) -> bool {
        Iterator::eq(self.clone(), other.clone())
    }
}

/// An iterator over the [`Component`]s of a [`RelativePath`], as [`str`]
/// slices.
///
/// This `struct` is created by the [`iter`][RelativePath::iter] method.
///
/// [`str`]: prim@str
#[derive(Clone)]
pub struct Iter<'a> {
    inner: Components<'a>,
}

impl<'a> Iterator for Iter<'a> {
    type Item = &'a str;

    fn next(&mut self) -> Option<&'a str> {
        self.inner.next().map(Component::as_str)
    }
}

impl<'a> DoubleEndedIterator for Iter<'a> {
    fn next_back(&mut self) -> Option<&'a str> {
        self.inner.next_back().map(Component::as_str)
    }
}

/// Error kind for [`FromPathError`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[non_exhaustive]
pub enum FromPathErrorKind {
    /// Non-relative component in path.
    NonRelative,
    /// Non-utf8 component in path.
    NonUtf8,
    /// Trying to convert a platform-specific path which uses a platform-specific separator.
    BadSeparator,
}

/// An error raised when attempting to convert a path using
/// [`RelativePathBuf::from_path`].
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FromPathError {
    kind: FromPathErrorKind,
}

impl FromPathError {
    /// Gets the underlying [`FromPathErrorKind`] that provides more details on
    /// what went wrong.
    ///
    /// # Examples
    ///
    /// ```
    /// use std::path::Path;
    /// use relative_path::{FromPathErrorKind, RelativePathBuf};
    ///
    /// let result = RelativePathBuf::from_path(Path::new("/hello/world"));
    /// let e = result.unwrap_err();
    ///
    /// assert_eq!(FromPathErrorKind::NonRelative, e.kind());
    /// ```
    #[must_use]
    pub fn kind(&self) -> FromPathErrorKind {
        self.kind
    }
}

impl From<FromPathErrorKind> for FromPathError {
    fn from(value: FromPathErrorKind) -> Self {
        Self { kind: value }
    }
}

impl fmt::Display for FromPathError {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match self.kind {
            FromPathErrorKind::NonRelative => "path contains non-relative component".fmt(fmt),
            FromPathErrorKind::NonUtf8 => "path contains non-utf8 component".fmt(fmt),
            FromPathErrorKind::BadSeparator => {
                "path contains platform-specific path separator".fmt(fmt)
            }
        }
    }
}

impl error::Error for FromPathError {}

/// An owned, mutable relative path.
///
/// This type provides methods to manipulate relative path objects.
#[derive(Clone)]
pub struct RelativePathBuf {
    inner: String,
}

impl RelativePathBuf {
    /// Create a new relative path buffer.
    #[must_use]
    pub fn new() -> RelativePathBuf {
        RelativePathBuf {
            inner: String::new(),
        }
    }

    /// Internal constructor to allocate a relative path buf with the given capacity.
    fn with_capacity(cap: usize) -> RelativePathBuf {
        RelativePathBuf {
            inner: String::with_capacity(cap),
        }
    }

    /// Try to convert a [`Path`] to a [`RelativePathBuf`].
    ///
    /// [`Path`]: https://doc.rust-lang.org/std/path/struct.Path.html
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::{RelativePath, RelativePathBuf, FromPathErrorKind};
    /// use std::path::Path;
    ///
    /// assert_eq!(
    ///     Ok(RelativePath::new("foo/bar").to_owned()),
    ///     RelativePathBuf::from_path(Path::new("foo/bar"))
    /// );
    /// ```
    ///
    /// # Errors
    ///
    /// This will error in case the provided path is not a relative path, which
    /// is identifier by it having a [`Prefix`] or [`RootDir`] component.
    ///
    /// [`Prefix`]: std::path::Component::Prefix
    /// [`RootDir`]: std::path::Component::RootDir
    pub fn from_path<P: AsRef<path::Path>>(path: P) -> Result<RelativePathBuf, FromPathError> {
        use std::path::Component::{CurDir, Normal, ParentDir, Prefix, RootDir};

        let mut buffer = RelativePathBuf::new();

        for c in path.as_ref().components() {
            match c {
                Prefix(_) | RootDir => return Err(FromPathErrorKind::NonRelative.into()),
                CurDir => continue,
                ParentDir => buffer.push(PARENT_STR),
                Normal(s) => buffer.push(s.to_str().ok_or(FromPathErrorKind::NonUtf8)?),
            }
        }

        Ok(buffer)
    }

    /// Extends `self` with `path`.
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePathBuf;
    ///
    /// let mut path = RelativePathBuf::new();
    /// path.push("foo");
    /// path.push("bar");
    ///
    /// assert_eq!("foo/bar", path);
    ///
    /// let mut path = RelativePathBuf::new();
    /// path.push("foo");
    /// path.push("/bar");
    ///
    /// assert_eq!("foo/bar", path);
    /// ```
    pub fn push<P>(&mut self, path: P)
    where
        P: AsRef<RelativePath>,
    {
        let other = path.as_ref();

        let other = if other.starts_with_sep() {
            &other.inner[1..]
        } else {
            &other.inner[..]
        };

        if !self.inner.is_empty() && !self.ends_with_sep() {
            self.inner.push(SEP);
        }

        self.inner.push_str(other);
    }

    /// Updates [`file_name`] to `file_name`.
    ///
    /// If [`file_name`] was [`None`], this is equivalent to pushing
    /// `file_name`.
    ///
    /// Otherwise it is equivalent to calling [`pop`] and then pushing
    /// `file_name`. The new path will be a sibling of the original path. (That
    /// is, it will have the same parent.)
    ///
    /// [`file_name`]: RelativePath::file_name
    /// [`pop`]: RelativePathBuf::pop
    /// [`None`]: https://doc.rust-lang.org/std/option/enum.Option.html
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePathBuf;
    ///
    /// let mut buf = RelativePathBuf::from("");
    /// assert!(buf.file_name() == None);
    /// buf.set_file_name("bar");
    /// assert_eq!(RelativePathBuf::from("bar"), buf);
    ///
    /// assert!(buf.file_name().is_some());
    /// buf.set_file_name("baz.txt");
    /// assert_eq!(RelativePathBuf::from("baz.txt"), buf);
    ///
    /// buf.push("bar");
    /// assert!(buf.file_name().is_some());
    /// buf.set_file_name("bar.txt");
    /// assert_eq!(RelativePathBuf::from("baz.txt/bar.txt"), buf);
    /// ```
    pub fn set_file_name<S: AsRef<str>>(&mut self, file_name: S) {
        if self.file_name().is_some() {
            let popped = self.pop();
            debug_assert!(popped);
        }

        self.push(file_name.as_ref());
    }

    /// Updates [`extension`] to `extension`.
    ///
    /// Returns `false` and does nothing if
    /// [`file_name`][RelativePath::file_name] is [`None`], returns `true` and
    /// updates the extension otherwise.
    ///
    /// If [`extension`] is [`None`], the extension is added; otherwise it is
    /// replaced.
    ///
    /// [`extension`]: RelativePath::extension
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::{RelativePath, RelativePathBuf};
    ///
    /// let mut p = RelativePathBuf::from("feel/the");
    ///
    /// p.set_extension("force");
    /// assert_eq!(RelativePath::new("feel/the.force"), p);
    ///
    /// p.set_extension("dark_side");
    /// assert_eq!(RelativePath::new("feel/the.dark_side"), p);
    ///
    /// assert!(p.pop());
    /// p.set_extension("nothing");
    /// assert_eq!(RelativePath::new("feel.nothing"), p);
    /// ```
    pub fn set_extension<S: AsRef<str>>(&mut self, extension: S) -> bool {
        let file_stem = match self.file_stem() {
            Some(stem) => stem,
            None => return false,
        };

        let end_file_stem = file_stem[file_stem.len()..].as_ptr() as usize;
        let start = self.inner.as_ptr() as usize;
        self.inner.truncate(end_file_stem.wrapping_sub(start));

        let extension = extension.as_ref();

        if !extension.is_empty() {
            self.inner.push(STEM_SEP);
            self.inner.push_str(extension);
        }

        true
    }

    /// Truncates `self` to [`parent`][RelativePath::parent].
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::{RelativePath, RelativePathBuf};
    ///
    /// let mut p = RelativePathBuf::from("test/test.rs");
    ///
    /// assert_eq!(true, p.pop());
    /// assert_eq!(RelativePath::new("test"), p);
    /// assert_eq!(true, p.pop());
    /// assert_eq!(RelativePath::new(""), p);
    /// assert_eq!(false, p.pop());
    /// assert_eq!(RelativePath::new(""), p);
    /// ```
    pub fn pop(&mut self) -> bool {
        match self.parent().map(|p| p.inner.len()) {
            Some(len) => {
                self.inner.truncate(len);
                true
            }
            None => false,
        }
    }

    /// Coerce to a [`RelativePath`] slice.
    #[must_use]
    pub fn as_relative_path(&self) -> &RelativePath {
        self
    }

    /// Consumes the `RelativePathBuf`, yielding its internal [`String`] storage.
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePathBuf;
    ///
    /// let p = RelativePathBuf::from("/the/head");
    /// let string = p.into_string();
    /// assert_eq!(string, "/the/head".to_owned());
    /// ```
    #[must_use]
    pub fn into_string(self) -> String {
        self.inner
    }

    /// Converts this `RelativePathBuf` into a [boxed][std::boxed::Box]
    /// [`RelativePath`].
    #[must_use]
    pub fn into_boxed_relative_path(self) -> Box<RelativePath> {
        let rw = Box::into_raw(self.inner.into_boxed_str()) as *mut RelativePath;
        unsafe { Box::from_raw(rw) }
    }
}

impl Default for RelativePathBuf {
    fn default() -> Self {
        RelativePathBuf::new()
    }
}

impl<'a> From<&'a RelativePath> for Cow<'a, RelativePath> {
    #[inline]
    fn from(s: &'a RelativePath) -> Cow<'a, RelativePath> {
        Cow::Borrowed(s)
    }
}

impl<'a> From<RelativePathBuf> for Cow<'a, RelativePath> {
    #[inline]
    fn from(s: RelativePathBuf) -> Cow<'a, RelativePath> {
        Cow::Owned(s)
    }
}

impl fmt::Debug for RelativePathBuf {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(fmt, "{:?}", &self.inner)
    }
}

impl AsRef<RelativePath> for RelativePathBuf {
    fn as_ref(&self) -> &RelativePath {
        RelativePath::new(&self.inner)
    }
}

impl AsRef<str> for RelativePath {
    fn as_ref(&self) -> &str {
        &self.inner
    }
}

impl Borrow<RelativePath> for RelativePathBuf {
    #[inline]
    fn borrow(&self) -> &RelativePath {
        self
    }
}

impl<'a, T: ?Sized + AsRef<str>> From<&'a T> for RelativePathBuf {
    fn from(path: &'a T) -> RelativePathBuf {
        RelativePathBuf {
            inner: path.as_ref().to_owned(),
        }
    }
}

impl From<String> for RelativePathBuf {
    fn from(path: String) -> RelativePathBuf {
        RelativePathBuf { inner: path }
    }
}

impl From<RelativePathBuf> for String {
    fn from(path: RelativePathBuf) -> String {
        path.into_string()
    }
}

impl ops::Deref for RelativePathBuf {
    type Target = RelativePath;

    fn deref(&self) -> &RelativePath {
        RelativePath::new(&self.inner)
    }
}

impl cmp::PartialEq for RelativePathBuf {
    fn eq(&self, other: &RelativePathBuf) -> bool {
        self.components() == other.components()
    }
}

impl cmp::Eq for RelativePathBuf {}

impl cmp::PartialOrd for RelativePathBuf {
    #[inline]
    fn partial_cmp(&self, other: &RelativePathBuf) -> Option<cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl cmp::Ord for RelativePathBuf {
    #[inline]
    fn cmp(&self, other: &RelativePathBuf) -> cmp::Ordering {
        self.components().cmp(other.components())
    }
}

impl Hash for RelativePathBuf {
    fn hash<H: Hasher>(&self, h: &mut H) {
        self.as_relative_path().hash(h);
    }
}

impl<P> Extend<P> for RelativePathBuf
where
    P: AsRef<RelativePath>,
{
    #[inline]
    fn extend<I: IntoIterator<Item = P>>(&mut self, iter: I) {
        iter.into_iter().for_each(move |p| self.push(p.as_ref()));
    }
}

impl<P> FromIterator<P> for RelativePathBuf
where
    P: AsRef<RelativePath>,
{
    #[inline]
    fn from_iter<I: IntoIterator<Item = P>>(iter: I) -> RelativePathBuf {
        let mut buf = RelativePathBuf::new();
        buf.extend(iter);
        buf
    }
}

/// A borrowed, immutable relative path.
#[repr(transparent)]
pub struct RelativePath {
    inner: str,
}

/// An error returned from [`strip_prefix`] if the prefix was not found.
///
/// [`strip_prefix`]: RelativePath::strip_prefix
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct StripPrefixError(());

impl RelativePath {
    /// Directly wraps a string slice as a `RelativePath` slice.
    pub fn new<S: AsRef<str> + ?Sized>(s: &S) -> &RelativePath {
        unsafe { &*(s.as_ref() as *const str as *const RelativePath) }
    }

    /// Try to convert a [`Path`] to a [`RelativePath`] without allocating a buffer.
    ///
    /// [`Path`]: std::path::Path
    ///
    /// # Errors
    ///
    /// This requires the path to be a legal, platform-neutral relative path.
    /// Otherwise various forms of [`FromPathError`] will be returned as an
    /// [`Err`].
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::{RelativePath, FromPathErrorKind};
    ///
    /// assert_eq!(
    ///     Ok(RelativePath::new("foo/bar")),
    ///     RelativePath::from_path("foo/bar")
    /// );
    ///
    /// // Note: absolute paths are different depending on platform.
    /// if cfg!(windows) {
    ///     let e = RelativePath::from_path("c:\\foo\\bar").unwrap_err();
    ///     assert_eq!(FromPathErrorKind::NonRelative, e.kind());
    /// }
    ///
    /// if cfg!(unix) {
    ///     let e = RelativePath::from_path("/foo/bar").unwrap_err();
    ///     assert_eq!(FromPathErrorKind::NonRelative, e.kind());
    /// }
    /// ```
    pub fn from_path<P: ?Sized + AsRef<path::Path>>(
        path: &P,
    ) -> Result<&RelativePath, FromPathError> {
        use std::path::Component::{CurDir, Normal, ParentDir, Prefix, RootDir};

        let other = path.as_ref();

        let s = match other.to_str() {
            Some(s) => s,
            None => return Err(FromPathErrorKind::NonUtf8.into()),
        };

        let rel = RelativePath::new(s);

        // check that the component compositions are equal.
        for (a, b) in other.components().zip(rel.components()) {
            match (a, b) {
                (Prefix(_) | RootDir, _) => return Err(FromPathErrorKind::NonRelative.into()),
                (CurDir, Component::CurDir) | (ParentDir, Component::ParentDir) => continue,
                (Normal(a), Component::Normal(b)) if a == b => continue,
                _ => return Err(FromPathErrorKind::BadSeparator.into()),
            }
        }

        Ok(rel)
    }

    /// Yields the underlying [`str`] slice.
    ///
    /// [`str`]: prim@str
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePath;
    ///
    /// assert_eq!(RelativePath::new("foo.txt").as_str(), "foo.txt");
    /// ```
    #[must_use]
    pub fn as_str(&self) -> &str {
        &self.inner
    }

    /// Returns an object that implements [`Display`][std::fmt::Display].
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePath;
    ///
    /// let path = RelativePath::new("tmp/foo.rs");
    ///
    /// println!("{}", path.display());
    /// ```
    #[deprecated(note = "RelativePath implements std::fmt::Display directly")]
    #[must_use]
    pub fn display(&self) -> Display {
        Display { path: self }
    }

    /// Creates an owned [`RelativePathBuf`] with path adjoined to self.
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePath;
    ///
    /// let path = RelativePath::new("foo/bar");
    /// assert_eq!("foo/bar/baz", path.join("baz"));
    /// ```
    pub fn join<P>(&self, path: P) -> RelativePathBuf
    where
        P: AsRef<RelativePath>,
    {
        let mut out = self.to_relative_path_buf();
        out.push(path);
        out
    }

    /// Iterate over all components in this relative path.
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::{Component, RelativePath};
    ///
    /// let path = RelativePath::new("foo/bar/baz");
    /// let mut it = path.components();
    ///
    /// assert_eq!(Some(Component::Normal("foo")), it.next());
    /// assert_eq!(Some(Component::Normal("bar")), it.next());
    /// assert_eq!(Some(Component::Normal("baz")), it.next());
    /// assert_eq!(None, it.next());
    /// ```
    #[must_use]
    pub fn components(&self) -> Components {
        Components::new(&self.inner)
    }

    /// Produces an iterator over the path's components viewed as [`str`]
    /// slices.
    ///
    /// For more information about the particulars of how the path is separated
    /// into components, see [`components`][Self::components].
    ///
    /// [`str`]: prim@str
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePath;
    ///
    /// let mut it = RelativePath::new("/tmp/foo.txt").iter();
    /// assert_eq!(it.next(), Some("tmp"));
    /// assert_eq!(it.next(), Some("foo.txt"));
    /// assert_eq!(it.next(), None)
    /// ```
    #[must_use]
    pub fn iter(&self) -> Iter {
        Iter {
            inner: self.components(),
        }
    }

    /// Convert to an owned [`RelativePathBuf`].
    #[must_use]
    pub fn to_relative_path_buf(&self) -> RelativePathBuf {
        RelativePathBuf::from(self.inner.to_owned())
    }

    /// Build an owned [`PathBuf`] relative to `base` for the current relative
    /// path.
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePath;
    /// use std::path::Path;
    ///
    /// let path = RelativePath::new("foo/bar").to_path(".");
    /// assert_eq!(Path::new("./foo/bar"), path);
    ///
    /// let path = RelativePath::new("foo/bar").to_path("");
    /// assert_eq!(Path::new("foo/bar"), path);
    /// ```
    ///
    /// # Encoding an absolute path
    ///
    /// Absolute paths are, in contrast to when using [`PathBuf::push`] *ignored*
    /// and will be added unchanged to the buffer.
    ///
    /// This is to preserve the probability of a path conversion failing if the
    /// relative path contains platform-specific absolute path components.
    ///
    /// ```
    /// use relative_path::RelativePath;
    /// use std::path::Path;
    ///
    /// if cfg!(windows) {
    ///     let path = RelativePath::new("/bar/baz").to_path("foo");
    ///     assert_eq!(Path::new("foo\\bar\\baz"), path);
    ///
    ///     let path = RelativePath::new("c:\\bar\\baz").to_path("foo");
    ///     assert_eq!(Path::new("foo\\c:\\bar\\baz"), path);
    /// }
    ///
    /// if cfg!(unix) {
    ///     let path = RelativePath::new("/bar/baz").to_path("foo");
    ///     assert_eq!(Path::new("foo/bar/baz"), path);
    ///
    ///     let path = RelativePath::new("c:\\bar\\baz").to_path("foo");
    ///     assert_eq!(Path::new("foo/c:\\bar\\baz"), path);
    /// }
    /// ```
    ///
    /// [`PathBuf`]: std::path::PathBuf
    /// [`PathBuf::push`]: std::path::PathBuf::push
    pub fn to_path<P: AsRef<path::Path>>(&self, base: P) -> path::PathBuf {
        let mut p = base.as_ref().to_path_buf().into_os_string();

        for c in self.components() {
            if !p.is_empty() {
                p.push(path::MAIN_SEPARATOR.encode_utf8(&mut [0u8, 0u8, 0u8, 0u8]));
            }

            p.push(c.as_str());
        }

        path::PathBuf::from(p)
    }

    /// Build an owned [`PathBuf`] relative to `base` for the current relative
    /// path.
    ///
    /// This is similar to [`to_path`] except that it doesn't just
    /// unconditionally append one path to the other, instead it performs the
    /// following operations depending on its own components:
    ///
    /// * [`Component::CurDir`] leaves the `base` unmodified.
    /// * [`Component::ParentDir`] removes a component from `base` using
    ///   [`path::PathBuf::pop`].
    /// * [`Component::Normal`] pushes the given path component onto `base`
    ///   using the same mechanism as [`to_path`].
    ///
    /// [`to_path`]: RelativePath::to_path
    ///
    /// Note that the exact semantics of the path operation is determined by the
    /// corresponding [`PathBuf`] operation. E.g. popping a component off a path
    /// like `.` will result in an empty path.
    ///
    /// ```
    /// use relative_path::RelativePath;
    /// use std::path::Path;
    ///
    /// let path = RelativePath::new("..").to_logical_path(".");
    /// assert_eq!(path, Path::new(""));
    /// ```
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePath;
    /// use std::path::Path;
    ///
    /// let path = RelativePath::new("..").to_logical_path("foo/bar");
    /// assert_eq!(path, Path::new("foo"));
    /// ```
    ///
    /// # Encoding an absolute path
    ///
    /// Behaves the same as [`to_path`][RelativePath::to_path] when encoding
    /// absolute paths.
    ///
    /// Absolute paths are, in contrast to when using [`PathBuf::push`] *ignored*
    /// and will be added unchanged to the buffer.
    ///
    /// This is to preserve the probability of a path conversion failing if the
    /// relative path contains platform-specific absolute path components.
    ///
    /// ```
    /// use relative_path::RelativePath;
    /// use std::path::Path;
    ///
    /// if cfg!(windows) {
    ///     let path = RelativePath::new("/bar/baz").to_logical_path("foo");
    ///     assert_eq!(Path::new("foo\\bar\\baz"), path);
    ///
    ///     let path = RelativePath::new("c:\\bar\\baz").to_logical_path("foo");
    ///     assert_eq!(Path::new("foo\\c:\\bar\\baz"), path);
    ///
    ///     let path = RelativePath::new("foo/bar").to_logical_path("");
    ///     assert_eq!(Path::new("foo\\bar"), path);
    /// }
    ///
    /// if cfg!(unix) {
    ///     let path = RelativePath::new("/bar/baz").to_logical_path("foo");
    ///     assert_eq!(Path::new("foo/bar/baz"), path);
    ///
    ///     let path = RelativePath::new("c:\\bar\\baz").to_logical_path("foo");
    ///     assert_eq!(Path::new("foo/c:\\bar\\baz"), path);
    ///
    ///     let path = RelativePath::new("foo/bar").to_logical_path("");
    ///     assert_eq!(Path::new("foo/bar"), path);
    /// }
    /// ```
    ///
    /// [`PathBuf`]: std::path::PathBuf
    /// [`PathBuf::push`]: std::path::PathBuf::push
    pub fn to_logical_path<P: AsRef<path::Path>>(&self, base: P) -> path::PathBuf {
        use self::Component::{CurDir, Normal, ParentDir};

        let mut p = base.as_ref().to_path_buf().into_os_string();

        for c in self.components() {
            match c {
                CurDir => continue,
                ParentDir => {
                    let mut temp = path::PathBuf::from(std::mem::take(&mut p));
                    temp.pop();
                    p = temp.into_os_string();
                }
                Normal(c) => {
                    if !p.is_empty() {
                        p.push(path::MAIN_SEPARATOR.encode_utf8(&mut [0u8, 0u8, 0u8, 0u8]));
                    }

                    p.push(c);
                }
            }
        }

        path::PathBuf::from(p)
    }

    /// Returns a relative path, without its final [`Component`] if there is one.
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePath;
    ///
    /// assert_eq!(Some(RelativePath::new("foo")), RelativePath::new("foo/bar").parent());
    /// assert_eq!(Some(RelativePath::new("")), RelativePath::new("foo").parent());
    /// assert_eq!(None, RelativePath::new("").parent());
    /// ```
    #[must_use]
    pub fn parent(&self) -> Option<&RelativePath> {
        use self::Component::CurDir;

        if self.inner.is_empty() {
            return None;
        }

        let mut it = self.components();
        while let Some(CurDir) = it.next_back() {}
        Some(it.as_relative_path())
    }

    /// Returns the final component of the `RelativePath`, if there is one.
    ///
    /// If the path is a normal file, this is the file name. If it's the path of
    /// a directory, this is the directory name.
    ///
    /// Returns [`None`] If the path terminates in `..`.
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePath;
    ///
    /// assert_eq!(Some("bin"), RelativePath::new("usr/bin/").file_name());
    /// assert_eq!(Some("foo.txt"), RelativePath::new("tmp/foo.txt").file_name());
    /// assert_eq!(Some("foo.txt"), RelativePath::new("tmp/foo.txt/").file_name());
    /// assert_eq!(Some("foo.txt"), RelativePath::new("foo.txt/.").file_name());
    /// assert_eq!(Some("foo.txt"), RelativePath::new("foo.txt/.//").file_name());
    /// assert_eq!(None, RelativePath::new("foo.txt/..").file_name());
    /// assert_eq!(None, RelativePath::new("/").file_name());
    /// ```
    #[must_use]
    pub fn file_name(&self) -> Option<&str> {
        use self::Component::{CurDir, Normal, ParentDir};

        let mut it = self.components();

        while let Some(c) = it.next_back() {
            return match c {
                CurDir => continue,
                Normal(name) => Some(name),
                ParentDir => None,
            };
        }

        None
    }

    /// Returns a relative path that, when joined onto `base`, yields `self`.
    ///
    /// # Errors
    ///
    /// If `base` is not a prefix of `self` (i.e. [`starts_with`] returns
    /// `false`), returns [`Err`].
    ///
    /// [`starts_with`]: Self::starts_with
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePath;
    ///
    /// let path = RelativePath::new("test/haha/foo.txt");
    ///
    /// assert_eq!(path.strip_prefix("test"), Ok(RelativePath::new("haha/foo.txt")));
    /// assert_eq!(path.strip_prefix("test").is_ok(), true);
    /// assert_eq!(path.strip_prefix("haha").is_ok(), false);
    /// ```
    pub fn strip_prefix<P>(&self, base: P) -> Result<&RelativePath, StripPrefixError>
    where
        P: AsRef<RelativePath>,
    {
        iter_after(self.components(), base.as_ref().components())
            .map(|c| c.as_relative_path())
            .ok_or(StripPrefixError(()))
    }

    /// Determines whether `base` is a prefix of `self`.
    ///
    /// Only considers whole path components to match.
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePath;
    ///
    /// let path = RelativePath::new("etc/passwd");
    ///
    /// assert!(path.starts_with("etc"));
    ///
    /// assert!(!path.starts_with("e"));
    /// ```
    pub fn starts_with<P>(&self, base: P) -> bool
    where
        P: AsRef<RelativePath>,
    {
        iter_after(self.components(), base.as_ref().components()).is_some()
    }

    /// Determines whether `child` is a suffix of `self`.
    ///
    /// Only considers whole path components to match.
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePath;
    ///
    /// let path = RelativePath::new("etc/passwd");
    ///
    /// assert!(path.ends_with("passwd"));
    /// ```
    pub fn ends_with<P>(&self, child: P) -> bool
    where
        P: AsRef<RelativePath>,
    {
        iter_after(self.components().rev(), child.as_ref().components().rev()).is_some()
    }

    /// Determines whether `self` is normalized.
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePath;
    ///
    /// // These are normalized.
    /// assert!(RelativePath::new("").is_normalized());
    /// assert!(RelativePath::new("baz.txt").is_normalized());
    /// assert!(RelativePath::new("foo/bar/baz.txt").is_normalized());
    /// assert!(RelativePath::new("..").is_normalized());
    /// assert!(RelativePath::new("../..").is_normalized());
    /// assert!(RelativePath::new("../../foo/bar/baz.txt").is_normalized());
    ///
    /// // These are not normalized.
    /// assert!(!RelativePath::new(".").is_normalized());
    /// assert!(!RelativePath::new("./baz.txt").is_normalized());
    /// assert!(!RelativePath::new("foo/..").is_normalized());
    /// assert!(!RelativePath::new("foo/../baz.txt").is_normalized());
    /// assert!(!RelativePath::new("foo/.").is_normalized());
    /// assert!(!RelativePath::new("foo/./baz.txt").is_normalized());
    /// assert!(!RelativePath::new("../foo/./bar/../baz.txt").is_normalized());
    /// ```
    #[must_use]
    pub fn is_normalized(&self) -> bool {
        self.components()
            .skip_while(|c| matches!(c, Component::ParentDir))
            .all(|c| matches!(c, Component::Normal(_)))
    }

    /// Creates an owned [`RelativePathBuf`] like `self` but with the given file
    /// name.
    ///
    /// See [`set_file_name`] for more details.
    ///
    /// [`set_file_name`]: RelativePathBuf::set_file_name
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::{RelativePath, RelativePathBuf};
    ///
    /// let path = RelativePath::new("tmp/foo.txt");
    /// assert_eq!(path.with_file_name("bar.txt"), RelativePathBuf::from("tmp/bar.txt"));
    ///
    /// let path = RelativePath::new("tmp");
    /// assert_eq!(path.with_file_name("var"), RelativePathBuf::from("var"));
    /// ```
    pub fn with_file_name<S: AsRef<str>>(&self, file_name: S) -> RelativePathBuf {
        let mut buf = self.to_relative_path_buf();
        buf.set_file_name(file_name);
        buf
    }

    /// Extracts the stem (non-extension) portion of [`file_name`][Self::file_name].
    ///
    /// The stem is:
    ///
    /// * [`None`], if there is no file name;
    /// * The entire file name if there is no embedded `.`;
    /// * The entire file name if the file name begins with `.` and has no other `.`s within;
    /// * Otherwise, the portion of the file name before the final `.`
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePath;
    ///
    /// let path = RelativePath::new("foo.rs");
    ///
    /// assert_eq!("foo", path.file_stem().unwrap());
    /// ```
    pub fn file_stem(&self) -> Option<&str> {
        self.file_name()
            .map(split_file_at_dot)
            .and_then(|(before, after)| before.or(after))
    }

    /// Extracts the extension of [`file_name`][Self::file_name], if possible.
    ///
    /// The extension is:
    ///
    /// * [`None`], if there is no file name;
    /// * [`None`], if there is no embedded `.`;
    /// * [`None`], if the file name begins with `.` and has no other `.`s within;
    /// * Otherwise, the portion of the file name after the final `.`
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePath;
    ///
    /// assert_eq!(Some("rs"), RelativePath::new("foo.rs").extension());
    /// assert_eq!(None, RelativePath::new(".rs").extension());
    /// assert_eq!(Some("rs"), RelativePath::new("foo.rs/.").extension());
    /// ```
    pub fn extension(&self) -> Option<&str> {
        self.file_name()
            .map(split_file_at_dot)
            .and_then(|(before, after)| before.and(after))
    }

    /// Creates an owned [`RelativePathBuf`] like `self` but with the given
    /// extension.
    ///
    /// See [`set_extension`] for more details.
    ///
    /// [`set_extension`]: RelativePathBuf::set_extension
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::{RelativePath, RelativePathBuf};
    ///
    /// let path = RelativePath::new("foo.rs");
    /// assert_eq!(path.with_extension("txt"), RelativePathBuf::from("foo.txt"));
    /// ```
    pub fn with_extension<S: AsRef<str>>(&self, extension: S) -> RelativePathBuf {
        let mut buf = self.to_relative_path_buf();
        buf.set_extension(extension);
        buf
    }

    /// Build an owned [`RelativePathBuf`], joined with the given path and
    /// normalized.
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePath;
    ///
    /// assert_eq!(
    ///     RelativePath::new("foo/baz.txt"),
    ///     RelativePath::new("foo/bar").join_normalized("../baz.txt").as_relative_path()
    /// );
    ///
    /// assert_eq!(
    ///     RelativePath::new("../foo/baz.txt"),
    ///     RelativePath::new("../foo/bar").join_normalized("../baz.txt").as_relative_path()
    /// );
    /// ```
    pub fn join_normalized<P>(&self, path: P) -> RelativePathBuf
    where
        P: AsRef<RelativePath>,
    {
        let mut buf = RelativePathBuf::new();
        relative_traversal(&mut buf, self.components());
        relative_traversal(&mut buf, path.as_ref().components());
        buf
    }

    /// Return an owned [`RelativePathBuf`], with all non-normal components
    /// moved to the beginning of the path.
    ///
    /// This permits for a normalized representation of different relative
    /// components.
    ///
    /// Normalization is a _destructive_ operation if the path references an
    /// actual filesystem path. An example of this is symlinks under unix, a
    /// path like `foo/../bar` might reference a different location other than
    /// `./bar`.
    ///
    /// Normalization is a logical operation and does not guarantee that the
    /// constructed path corresponds to what the filesystem would do. On Linux
    /// for example symbolic links could mean that the logical path doesn't
    /// correspond to the filesystem path.
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePath;
    ///
    /// assert_eq!(
    ///     "../foo/baz.txt",
    ///     RelativePath::new("../foo/./bar/../baz.txt").normalize()
    /// );
    ///
    /// assert_eq!(
    ///     "",
    ///     RelativePath::new(".").normalize()
    /// );
    /// ```
    #[must_use]
    pub fn normalize(&self) -> RelativePathBuf {
        let mut buf = RelativePathBuf::with_capacity(self.inner.len());
        relative_traversal(&mut buf, self.components());
        buf
    }

    /// Constructs a relative path from the current path, to `path`.
    ///
    /// This function will return the empty [`RelativePath`] `""` if this source
    /// contains unnamed components like `..` that would have to be traversed to
    /// reach the destination `path`. This is necessary since we have no way of
    /// knowing what the names of those components are when we're building the
    /// new relative path.
    ///
    /// ```
    /// use relative_path::RelativePath;
    ///
    /// // Here we don't know what directories `../..` refers to, so there's no
    /// // way to construct a path back to `bar` in the current directory from
    /// // `../..`.
    /// let from = RelativePath::new("../../foo/relative-path");
    /// let to = RelativePath::new("bar");
    /// assert_eq!("", from.relative(to));
    /// ```
    ///
    /// One exception to this is when two paths contains a common prefix at
    /// which point there's no need to know what the names of those unnamed
    /// components are.
    ///
    /// ```
    /// use relative_path::RelativePath;
    ///
    /// let from = RelativePath::new("../../foo/bar");
    /// let to = RelativePath::new("../../foo/baz");
    ///
    /// assert_eq!("../baz", from.relative(to));
    ///
    /// let from = RelativePath::new("../a/../../foo/bar");
    /// let to = RelativePath::new("../../foo/baz");
    ///
    /// assert_eq!("../baz", from.relative(to));
    /// ```
    ///
    /// # Examples
    ///
    /// ```
    /// use relative_path::RelativePath;
    ///
    /// assert_eq!(
    ///     "../../e/f",
    ///     RelativePath::new("a/b/c/d").relative(RelativePath::new("a/b/e/f"))
    /// );
    ///
    /// assert_eq!(
    ///     "../bbb",
    ///     RelativePath::new("a/../aaa").relative(RelativePath::new("b/../bbb"))
    /// );
    ///
    /// let a = RelativePath::new("git/relative-path");
    /// let b = RelativePath::new("git");
    /// assert_eq!("relative-path", b.relative(a));
    /// assert_eq!("..", a.relative(b));
    ///
    /// let a = RelativePath::new("foo/bar/bap/foo.h");
    /// let b = RelativePath::new("../arch/foo.h");
    /// assert_eq!("../../../../../arch/foo.h", a.relative(b));
    /// assert_eq!("", b.relative(a));
    /// ```
    pub fn relative<P>(&self, path: P) -> RelativePathBuf
    where
        P: AsRef<RelativePath>,
    {
        let mut from = RelativePathBuf::with_capacity(self.inner.len());
        let mut to = RelativePathBuf::with_capacity(path.as_ref().inner.len());

        relative_traversal(&mut from, self.components());
        relative_traversal(&mut to, path.as_ref().components());

        let mut it_from = from.components();
        let mut it_to = to.components();

        // Strip a common prefixes - if any.
        let (lead_from, lead_to) = loop {
            match (it_from.next(), it_to.next()) {
                (Some(f), Some(t)) if f == t => continue,
                (f, t) => {
                    break (f, t);
                }
            }
        };

        // Special case: The path we are traversing from can't contain unnamed
        // components. A relative path might be any path, like `/`, or
        // `/foo/bar/baz`, and these components cannot be named in the relative
        // traversal.
        //
        // Also note that `relative_traversal` guarantees that all ParentDir
        // components are at the head of the path being built.
        if lead_from == Some(Component::ParentDir) {
            return RelativePathBuf::new();
        }

        let head = lead_from.into_iter().chain(it_from);
        let tail = lead_to.into_iter().chain(it_to);

        let mut buf = RelativePathBuf::with_capacity(usize::max(from.inner.len(), to.inner.len()));

        for c in head.map(|_| Component::ParentDir).chain(tail) {
            buf.push(c.as_str());
        }

        buf
    }

    /// Check if path starts with a path separator.
    #[inline]
    fn starts_with_sep(&self) -> bool {
        self.inner.starts_with(SEP)
    }

    /// Check if path ends with a path separator.
    #[inline]
    fn ends_with_sep(&self) -> bool {
        self.inner.ends_with(SEP)
    }
}

impl<'a> IntoIterator for &'a RelativePath {
    type IntoIter = Iter<'a>;
    type Item = &'a str;

    #[inline]
    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

/// Conversion from a [`Box<str>`] reference to a [`Box<RelativePath>`].
///
/// # Examples
///
/// ```
/// use relative_path::RelativePath;
///
/// let path: Box<RelativePath> = Box::<str>::from("foo/bar").into();
/// assert_eq!(&*path, "foo/bar");
/// ```
impl From<Box<str>> for Box<RelativePath> {
    #[inline]
    fn from(boxed: Box<str>) -> Box<RelativePath> {
        let rw = Box::into_raw(boxed) as *mut RelativePath;
        unsafe { Box::from_raw(rw) }
    }
}

/// Conversion from a [`str`] reference to a [`Box<RelativePath>`].
///
/// [`str`]: prim@str
///
/// # Examples
///
/// ```
/// use relative_path::RelativePath;
///
/// let path: Box<RelativePath> = "foo/bar".into();
/// assert_eq!(&*path, "foo/bar");
///
/// let path: Box<RelativePath> = RelativePath::new("foo/bar").into();
/// assert_eq!(&*path, "foo/bar");
/// ```
impl<T> From<&T> for Box<RelativePath>
where
    T: ?Sized + AsRef<str>,
{
    #[inline]
    fn from(path: &T) -> Box<RelativePath> {
        Box::<RelativePath>::from(Box::<str>::from(path.as_ref()))
    }
}

/// Conversion from [`RelativePathBuf`] to [`Box<RelativePath>`].
///
/// # Examples
///
/// ```
/// use std::sync::Arc;
/// use relative_path::{RelativePath, RelativePathBuf};
///
/// let path = RelativePathBuf::from("foo/bar");
/// let path: Box<RelativePath> = path.into();
/// assert_eq!(&*path, "foo/bar");
/// ```
impl From<RelativePathBuf> for Box<RelativePath> {
    #[inline]
    fn from(path: RelativePathBuf) -> Box<RelativePath> {
        let boxed: Box<str> = path.inner.into();
        let rw = Box::into_raw(boxed) as *mut RelativePath;
        unsafe { Box::from_raw(rw) }
    }
}

/// Clone implementation for [`Box<RelativePath>`].
///
/// # Examples
///
/// ```
/// use relative_path::RelativePath;
///
/// let path: Box<RelativePath> = RelativePath::new("foo/bar").into();
/// let path2 = path.clone();
/// assert_eq!(&*path, &*path2);
/// ```
impl Clone for Box<RelativePath> {
    #[inline]
    fn clone(&self) -> Self {
        self.to_relative_path_buf().into_boxed_relative_path()
    }
}

/// Conversion from [`RelativePath`] to [`Arc<RelativePath>`].
///
/// # Examples
///
/// ```
/// use std::sync::Arc;
/// use relative_path::RelativePath;
///
/// let path: Arc<RelativePath> = RelativePath::new("foo/bar").into();
/// assert_eq!(&*path, "foo/bar");
/// ```
impl From<&RelativePath> for Arc<RelativePath> {
    #[inline]
    fn from(path: &RelativePath) -> Arc<RelativePath> {
        let arc: Arc<str> = path.inner.into();
        let rw = Arc::into_raw(arc) as *const RelativePath;
        unsafe { Arc::from_raw(rw) }
    }
}

/// Conversion from [`RelativePathBuf`] to [`Arc<RelativePath>`].
///
/// # Examples
///
/// ```
/// use std::sync::Arc;
/// use relative_path::{RelativePath, RelativePathBuf};
///
/// let path = RelativePathBuf::from("foo/bar");
/// let path: Arc<RelativePath> = path.into();
/// assert_eq!(&*path, "foo/bar");
/// ```
impl From<RelativePathBuf> for Arc<RelativePath> {
    #[inline]
    fn from(path: RelativePathBuf) -> Arc<RelativePath> {
        let arc: Arc<str> = path.inner.into();
        let rw = Arc::into_raw(arc) as *const RelativePath;
        unsafe { Arc::from_raw(rw) }
    }
}

/// Conversion from [`RelativePathBuf`] to [`Arc<RelativePath>`].
///
/// # Examples
///
/// ```
/// use std::rc::Rc;
/// use relative_path::RelativePath;
///
/// let path: Rc<RelativePath> = RelativePath::new("foo/bar").into();
/// assert_eq!(&*path, "foo/bar");
/// ```
impl From<&RelativePath> for Rc<RelativePath> {
    #[inline]
    fn from(path: &RelativePath) -> Rc<RelativePath> {
        let rc: Rc<str> = path.inner.into();
        let rw = Rc::into_raw(rc) as *const RelativePath;
        unsafe { Rc::from_raw(rw) }
    }
}

/// Conversion from [`RelativePathBuf`] to [`Rc<RelativePath>`].
///
/// # Examples
///
/// ```
/// use std::rc::Rc;
/// use relative_path::{RelativePath, RelativePathBuf};
///
/// let path = RelativePathBuf::from("foo/bar");
/// let path: Rc<RelativePath> = path.into();
/// assert_eq!(&*path, "foo/bar");
/// ```
impl From<RelativePathBuf> for Rc<RelativePath> {
    #[inline]
    fn from(path: RelativePathBuf) -> Rc<RelativePath> {
        let rc: Rc<str> = path.inner.into();
        let rw = Rc::into_raw(rc) as *const RelativePath;
        unsafe { Rc::from_raw(rw) }
    }
}

/// [`ToOwned`] implementation for [`RelativePath`].
///
/// # Examples
///
/// ```
/// use relative_path::RelativePath;
///
/// let path = RelativePath::new("foo/bar").to_owned();
/// assert_eq!(path, "foo/bar");
/// ```
impl ToOwned for RelativePath {
    type Owned = RelativePathBuf;

    #[inline]
    fn to_owned(&self) -> RelativePathBuf {
        self.to_relative_path_buf()
    }
}

impl fmt::Debug for RelativePath {
    #[inline]
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(fmt, "{:?}", &self.inner)
    }
}

/// [`AsRef<str>`] implementation for [`RelativePathBuf`].
///
/// # Examples
///
/// ```
/// use relative_path::RelativePathBuf;
///
/// let path = RelativePathBuf::from("foo/bar");
/// let string: &str = path.as_ref();
/// assert_eq!(string, "foo/bar");
/// ```
impl AsRef<str> for RelativePathBuf {
    #[inline]
    fn as_ref(&self) -> &str {
        &self.inner
    }
}

/// [`AsRef<RelativePath>`] implementation for [String].
///
/// # Examples
///
/// ```
/// use relative_path::RelativePath;
///
/// let path: String = format!("foo/bar");
/// let path: &RelativePath = path.as_ref();
/// assert_eq!(path, "foo/bar");
/// ```
impl AsRef<RelativePath> for String {
    #[inline]
    fn as_ref(&self) -> &RelativePath {
        RelativePath::new(self)
    }
}

/// [`AsRef<RelativePath>`] implementation for [`str`].
///
/// [`str`]: prim@str
///
/// # Examples
///
/// ```
/// use relative_path::RelativePath;
///
/// let path: &RelativePath = "foo/bar".as_ref();
/// assert_eq!(path, RelativePath::new("foo/bar"));
/// ```
impl AsRef<RelativePath> for str {
    #[inline]
    fn as_ref(&self) -> &RelativePath {
        RelativePath::new(self)
    }
}

impl AsRef<RelativePath> for RelativePath {
    #[inline]
    fn as_ref(&self) -> &RelativePath {
        self
    }
}

impl cmp::PartialEq for RelativePath {
    #[inline]
    fn eq(&self, other: &RelativePath) -> bool {
        self.components() == other.components()
    }
}

impl cmp::Eq for RelativePath {}

impl cmp::PartialOrd for RelativePath {
    #[inline]
    fn partial_cmp(&self, other: &RelativePath) -> Option<cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl cmp::Ord for RelativePath {
    #[inline]
    fn cmp(&self, other: &RelativePath) -> cmp::Ordering {
        self.components().cmp(other.components())
    }
}

impl Hash for RelativePath {
    #[inline]
    fn hash<H: Hasher>(&self, h: &mut H) {
        for c in self.components() {
            c.hash(h);
        }
    }
}

impl fmt::Display for RelativePath {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(&self.inner, f)
    }
}

impl fmt::Display for RelativePathBuf {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(&self.inner, f)
    }
}

/// Helper struct for printing relative paths.
///
/// This is not strictly necessary in the same sense as it is for [`Display`],
/// because relative paths are guaranteed to be valid UTF-8. But the behavior is
/// preserved to simplify the transition between [`Path`] and [`RelativePath`].
///
/// [`Path`]: std::path::Path
/// [`Display`]: std::fmt::Display
pub struct Display<'a> {
    path: &'a RelativePath,
}

impl<'a> fmt::Debug for Display<'a> {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Debug::fmt(&self.path, f)
    }
}

impl<'a> fmt::Display for Display<'a> {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(&self.path, f)
    }
}

/// [`serde::ser::Serialize`] implementation for [`RelativePathBuf`].
///
/// ```
/// use serde::Serialize;
/// use relative_path::RelativePathBuf;
///
/// #[derive(Serialize)]
/// struct Document {
///     path: RelativePathBuf,
/// }
/// ```
#[cfg(feature = "serde")]
impl serde::ser::Serialize for RelativePathBuf {
    #[inline]
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::ser::Serializer,
    {
        serializer.serialize_str(&self.inner)
    }
}

/// [`serde::de::Deserialize`] implementation for [`RelativePathBuf`].
///
/// ```
/// use serde::Deserialize;
/// use relative_path::RelativePathBuf;
///
/// #[derive(Deserialize)]
/// struct Document {
///     path: RelativePathBuf,
/// }
/// ```
#[cfg(feature = "serde")]
impl<'de> serde::de::Deserialize<'de> for RelativePathBuf {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::de::Deserializer<'de>,
    {
        struct Visitor;

        impl<'de> serde::de::Visitor<'de> for Visitor {
            type Value = RelativePathBuf;

            #[inline]
            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a relative path")
            }

            #[inline]
            fn visit_string<E>(self, input: String) -> Result<Self::Value, E>
            where
                E: serde::de::Error,
            {
                Ok(RelativePathBuf::from(input))
            }

            #[inline]
            fn visit_str<E>(self, input: &str) -> Result<Self::Value, E>
            where
                E: serde::de::Error,
            {
                Ok(RelativePathBuf::from(input.to_owned()))
            }
        }

        deserializer.deserialize_str(Visitor)
    }
}

/// [`serde::de::Deserialize`] implementation for [`Box<RelativePath>`].
///
/// ```
/// use serde::Deserialize;
/// use relative_path::RelativePath;
///
/// #[derive(Deserialize)]
/// struct Document {
///     path: Box<RelativePath>,
/// }
/// ```
#[cfg(feature = "serde")]
impl<'de> serde::de::Deserialize<'de> for Box<RelativePath> {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::de::Deserializer<'de>,
    {
        struct Visitor;

        impl<'de> serde::de::Visitor<'de> for Visitor {
            type Value = Box<RelativePath>;

            #[inline]
            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a relative path")
            }

            #[inline]
            fn visit_string<E>(self, input: String) -> Result<Self::Value, E>
            where
                E: serde::de::Error,
            {
                Ok(Box::<RelativePath>::from(input.into_boxed_str()))
            }

            #[inline]
            fn visit_str<E>(self, input: &str) -> Result<Self::Value, E>
            where
                E: serde::de::Error,
            {
                Ok(Box::<RelativePath>::from(input))
            }
        }

        deserializer.deserialize_str(Visitor)
    }
}

/// [`serde::de::Deserialize`] implementation for a [`RelativePath`] reference.
///
/// ```
/// use serde::Deserialize;
/// use relative_path::RelativePath;
///
/// #[derive(Deserialize)]
/// struct Document<'a> {
///     #[serde(borrow)]
///     path: &'a RelativePath,
/// }
/// ```
#[cfg(feature = "serde")]
impl<'de: 'a, 'a> serde::de::Deserialize<'de> for &'a RelativePath {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::de::Deserializer<'de>,
    {
        struct Visitor;

        impl<'a> serde::de::Visitor<'a> for Visitor {
            type Value = &'a RelativePath;

            #[inline]
            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a borrowed relative path")
            }

            #[inline]
            fn visit_borrowed_str<E>(self, v: &'a str) -> Result<Self::Value, E>
            where
                E: serde::de::Error,
            {
                Ok(RelativePath::new(v))
            }

            #[inline]
            fn visit_borrowed_bytes<E>(self, v: &'a [u8]) -> Result<Self::Value, E>
            where
                E: serde::de::Error,
            {
                let string = str::from_utf8(v).map_err(|_| {
                    serde::de::Error::invalid_value(serde::de::Unexpected::Bytes(v), &self)
                })?;
                Ok(RelativePath::new(string))
            }
        }

        deserializer.deserialize_str(Visitor)
    }
}

/// [`serde::ser::Serialize`] implementation for [`RelativePath`].
///
/// ```
/// use serde::Serialize;
/// use relative_path::RelativePath;
///
/// #[derive(Serialize)]
/// struct Document<'a> {
///     path: &'a RelativePath,
/// }
/// ```
#[cfg(feature = "serde")]
impl serde::ser::Serialize for RelativePath {
    #[inline]
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::ser::Serializer,
    {
        serializer.serialize_str(&self.inner)
    }
}

macro_rules! impl_cmp {
    ($lhs:ty, $rhs:ty) => {
        impl<'a, 'b> PartialEq<$rhs> for $lhs {
            #[inline]
            fn eq(&self, other: &$rhs) -> bool {
                <RelativePath as PartialEq>::eq(self, other)
            }
        }

        impl<'a, 'b> PartialEq<$lhs> for $rhs {
            #[inline]
            fn eq(&self, other: &$lhs) -> bool {
                <RelativePath as PartialEq>::eq(self, other)
            }
        }

        impl<'a, 'b> PartialOrd<$rhs> for $lhs {
            #[inline]
            fn partial_cmp(&self, other: &$rhs) -> Option<cmp::Ordering> {
                <RelativePath as PartialOrd>::partial_cmp(self, other)
            }
        }

        impl<'a, 'b> PartialOrd<$lhs> for $rhs {
            #[inline]
            fn partial_cmp(&self, other: &$lhs) -> Option<cmp::Ordering> {
                <RelativePath as PartialOrd>::partial_cmp(self, other)
            }
        }
    };
}

impl_cmp!(RelativePathBuf, RelativePath);
impl_cmp!(RelativePathBuf, &'a RelativePath);
impl_cmp!(Cow<'a, RelativePath>, RelativePath);
impl_cmp!(Cow<'a, RelativePath>, &'b RelativePath);
impl_cmp!(Cow<'a, RelativePath>, RelativePathBuf);

macro_rules! impl_cmp_str {
    ($lhs:ty, $rhs:ty) => {
        impl<'a, 'b> PartialEq<$rhs> for $lhs {
            #[inline]
            fn eq(&self, other: &$rhs) -> bool {
                <RelativePath as PartialEq>::eq(self, other.as_ref())
            }
        }

        impl<'a, 'b> PartialEq<$lhs> for $rhs {
            #[inline]
            fn eq(&self, other: &$lhs) -> bool {
                <RelativePath as PartialEq>::eq(self.as_ref(), other)
            }
        }

        impl<'a, 'b> PartialOrd<$rhs> for $lhs {
            #[inline]
            fn partial_cmp(&self, other: &$rhs) -> Option<cmp::Ordering> {
                <RelativePath as PartialOrd>::partial_cmp(self, other.as_ref())
            }
        }

        impl<'a, 'b> PartialOrd<$lhs> for $rhs {
            #[inline]
            fn partial_cmp(&self, other: &$lhs) -> Option<cmp::Ordering> {
                <RelativePath as PartialOrd>::partial_cmp(self.as_ref(), other)
            }
        }
    };
}

impl_cmp_str!(RelativePathBuf, str);
impl_cmp_str!(RelativePathBuf, &'a str);
impl_cmp_str!(RelativePathBuf, String);
impl_cmp_str!(RelativePath, str);
impl_cmp_str!(RelativePath, &'a str);
impl_cmp_str!(RelativePath, String);
impl_cmp_str!(&'a RelativePath, str);
impl_cmp_str!(&'a RelativePath, String);
