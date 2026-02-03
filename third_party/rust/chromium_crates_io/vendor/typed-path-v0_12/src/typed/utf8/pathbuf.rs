use alloc::collections::TryReserveError;
use core::convert::TryFrom;
use core::fmt;

use crate::common::{CheckedPathError, StripPrefixError};
use crate::no_std_compat::*;
use crate::typed::{
    PathType, Utf8TypedAncestors, Utf8TypedComponents, Utf8TypedIter, Utf8TypedPath,
};
use crate::unix::{Utf8UnixPath, Utf8UnixPathBuf};
use crate::windows::{Utf8WindowsPath, Utf8WindowsPathBuf};

/// Represents a pathbuf with a known type that can be one of:
///
/// * [`Utf8UnixPathBuf`]
/// * [`Utf8WindowsPathBuf`]
#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub enum Utf8TypedPathBuf {
    Unix(Utf8UnixPathBuf),
    Windows(Utf8WindowsPathBuf),
}

impl Utf8TypedPathBuf {
    /// Returns true if this path represents a Unix path.
    #[inline]
    pub fn is_unix(&self) -> bool {
        matches!(self, Self::Unix(_))
    }

    /// Returns true if this path represents a Windows path.
    #[inline]
    pub fn is_windows(&self) -> bool {
        matches!(self, Self::Windows(_))
    }

    /// Converts this [`Utf8TypedPathBuf`] into the Unix variant.
    pub fn with_unix_encoding(&self) -> Utf8TypedPathBuf {
        match self {
            Self::Windows(p) => Utf8TypedPathBuf::Unix(p.with_unix_encoding()),
            _ => self.clone(),
        }
    }

    /// Converts this [`Utf8TypedPathBuf`] into the Unix variant, ensuring it is valid as a Unix
    /// path.
    pub fn with_unix_encoding_checked(&self) -> Result<Utf8TypedPathBuf, CheckedPathError> {
        Ok(match self {
            Self::Unix(p) => Utf8TypedPathBuf::Unix(p.with_unix_encoding_checked()?),
            Self::Windows(p) => Utf8TypedPathBuf::Unix(p.with_unix_encoding_checked()?),
        })
    }

    /// Converts this [`Utf8TypedPathBuf`] into the Windows variant.
    pub fn with_windows_encoding(&self) -> Utf8TypedPathBuf {
        match self {
            Self::Unix(p) => Utf8TypedPathBuf::Windows(p.with_windows_encoding()),
            _ => self.clone(),
        }
    }

    /// Converts this [`Utf8TypedPathBuf`] into the Windows variant, ensuring it is valid as a
    /// Windows path.
    pub fn with_windows_encoding_checked(&self) -> Result<Utf8TypedPathBuf, CheckedPathError> {
        Ok(match self {
            Self::Unix(p) => Utf8TypedPathBuf::Windows(p.with_windows_encoding_checked()?),
            Self::Windows(p) => Utf8TypedPathBuf::Windows(p.with_windows_encoding_checked()?),
        })
    }

    /// Allocates an empty [`Utf8TypedPathBuf`] for the specified path type.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{PathType, Utf8TypedPathBuf};
    /// let _unix_path = Utf8TypedPathBuf::new(PathType::Unix);
    /// let _windows_path = Utf8TypedPathBuf::new(PathType::Windows);
    /// ```
    #[inline]
    pub fn new(r#type: PathType) -> Self {
        match r#type {
            PathType::Unix => Self::unix(),
            PathType::Windows => Self::windows(),
        }
    }

    /// Allocates an empty [`Utf8TypedPathBuf`] as a Unix path.
    #[inline]
    pub fn unix() -> Self {
        Self::Unix(Utf8UnixPathBuf::new())
    }

    /// Allocates an empty [`Utf8TypedPathBuf`] as a Windows path.
    #[inline]
    pub fn windows() -> Self {
        Self::Windows(Utf8WindowsPathBuf::new())
    }

    /// Creates a new [`Utf8TypedPathBuf`] from the bytes representing a Unix path.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    /// let path = Utf8TypedPathBuf::from_unix("/tmp");
    /// ```
    pub fn from_unix(s: impl AsRef<str>) -> Self {
        Self::Unix(Utf8UnixPathBuf::from(s.as_ref()))
    }

    /// Creates a new [`Utf8TypedPathBuf`] from the bytes representing a Windows path.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    /// let path = Utf8TypedPathBuf::from_windows(r"C:\tmp");
    /// ```
    pub fn from_windows(s: impl AsRef<str>) -> Self {
        Self::Windows(Utf8WindowsPathBuf::from(s.as_ref()))
    }

    /// Converts into a [`Utf8TypedPath`].
    pub fn to_path(&self) -> Utf8TypedPath<'_> {
        match self {
            Self::Unix(path) => Utf8TypedPath::Unix(path.as_path()),
            Self::Windows(path) => Utf8TypedPath::Windows(path.as_path()),
        }
    }

    /// Extends `self` with `path`.
    ///
    /// If `path` is absolute, it replaces the current path.
    ///
    /// With [`Utf8WindowsPathBuf`]:
    ///
    /// * if `path` has a root but no prefix (e.g., `\windows`), it
    ///   replaces everything except for the prefix (if any) of `self`.
    /// * if `path` has a prefix but no root, it replaces `self`.
    /// * if `self` has a verbatim prefix (e.g. `\\?\C:\windows`)
    ///   and `path` is not empty, the new path is normalized: all references
    ///   to `.` and `..` are removed.
    ///
    /// [`Utf8WindowsPathBuf`]: crate::Utf8WindowsPathBuf
    ///
    /// # Difference from PathBuf
    ///
    /// Unlike [`Utf8PathBuf::push`], this implementation only supports types that implement
    /// `AsRef<str>` instead of `AsRef<Path>`.
    ///
    /// [`Utf8PathBuf::push`]: crate::Utf8PathBuf::push
    ///
    /// # Examples
    ///
    /// Pushing a relative path extends the existing path:
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// let mut path = Utf8TypedPathBuf::from_unix("/tmp");
    /// path.push("file.bk");
    /// assert_eq!(path, Utf8TypedPathBuf::from_unix("/tmp/file.bk"));
    /// ```
    ///
    /// Pushing an absolute path replaces the existing path:
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// let mut path = Utf8TypedPathBuf::from_unix("/tmp");
    /// path.push("/etc");
    /// assert_eq!(path, Utf8TypedPathBuf::from_unix("/etc"));
    /// ```
    pub fn push(&mut self, path: impl AsRef<str>) {
        match self {
            Self::Unix(a) => a.push(Utf8UnixPath::new(&path)),
            Self::Windows(a) => a.push(Utf8WindowsPath::new(&path)),
        }
    }

    /// Like [`Utf8TypedPathBuf::push`], extends `self` with `path`, but also checks to ensure that
    /// `path` abides by a set of rules.
    ///
    /// # Rules
    ///
    /// 1. `path` cannot contain a prefix component.
    /// 2. `path` cannot contain a root component.
    /// 3. `path` cannot contain invalid filename bytes.
    /// 4. `path` cannot contain parent components such that the current path would be escaped.
    ///
    /// # Difference from PathBuf
    ///
    /// Unlike [`PathBuf::push_checked`], this implementation only supports types that implement
    /// `AsRef<str>` instead of `AsRef<Path>`.
    ///
    /// [`PathBuf::push_checked`]: crate::PathBuf::push_checked
    ///
    /// # Examples
    ///
    /// Pushing a relative path extends the existing path:
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// let mut path = Utf8TypedPathBuf::from_unix("/tmp");
    /// assert!(path.push_checked("file.bk").is_ok());
    /// assert_eq!(path, Utf8TypedPathBuf::from_unix("/tmp/file.bk"));
    /// ```
    ///
    /// Pushing a relative path that contains unresolved parent directory references fails
    /// with an error:
    ///
    /// ```
    /// use typed_path::{CheckedPathError, Utf8TypedPathBuf};
    ///
    /// let mut path = Utf8TypedPathBuf::from_unix("/tmp");
    ///
    /// // Pushing a relative path that contains parent directory references that cannot be
    /// // resolved within the path is considered an error as this is considered a path
    /// // traversal attack!
    /// assert_eq!(path.push_checked(".."), Err(CheckedPathError::PathTraversalAttack));
    /// assert_eq!(path, Utf8TypedPathBuf::from("/tmp"));
    /// ```
    ///
    /// Pushing an absolute path fails with an error:
    ///
    /// ```
    /// use typed_path::{CheckedPathError, Utf8TypedPathBuf};
    ///
    /// let mut path = Utf8TypedPathBuf::from_unix("/tmp");
    ///
    /// // Pushing an absolute path will fail with an error
    /// assert_eq!(path.push_checked("/etc"), Err(CheckedPathError::UnexpectedRoot));
    /// assert_eq!(path, Utf8TypedPathBuf::from_unix("/tmp"));
    /// ```
    pub fn push_checked(&mut self, path: impl AsRef<str>) -> Result<(), CheckedPathError> {
        match self {
            Self::Unix(a) => a.push_checked(Utf8UnixPath::new(&path)),
            Self::Windows(a) => a.push_checked(Utf8WindowsPath::new(&path)),
        }
    }

    /// Truncates `self` to [`self.parent`].
    ///
    /// Returns `false` and does nothing if [`self.parent`] is [`None`].
    /// Otherwise, returns `true`.
    ///
    /// [`self.parent`]: Utf8TypedPath::parent
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8TypedPath, Utf8TypedPathBuf};
    ///
    /// let mut p = Utf8TypedPathBuf::from_unix("/spirited/away.rs");
    ///
    /// p.pop();
    /// assert_eq!(Utf8TypedPath::derive("/spirited"), p);
    /// p.pop();
    /// assert_eq!(Utf8TypedPath::derive("/"), p);
    /// ```
    pub fn pop(&mut self) -> bool {
        impl_typed_fn!(self, pop)
    }

    /// Updates [`self.file_name`] to `file_name`.
    ///
    /// If [`self.file_name`] was [`None`], this is equivalent to pushing
    /// `file_name`.
    ///
    /// Otherwise it is equivalent to calling [`pop`] and then pushing
    /// `file_name`. The new path will be a sibling of the original path.
    /// (That is, it will have the same parent.)
    ///
    /// [`self.file_name`]: Utf8TypedPath::file_name
    /// [`pop`]: Utf8TypedPathBuf::pop
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// let mut buf = Utf8TypedPathBuf::from_unix("/");
    /// assert!(buf.file_name() == None);
    /// buf.set_file_name("bar");
    /// assert!(buf == Utf8TypedPathBuf::from_unix("/bar"));
    /// assert!(buf.file_name().is_some());
    /// buf.set_file_name("baz.txt");
    /// assert!(buf == Utf8TypedPathBuf::from_unix("/baz.txt"));
    /// ```
    pub fn set_file_name<S: AsRef<str>>(&mut self, file_name: S) {
        impl_typed_fn!(self, set_file_name, file_name)
    }

    /// Updates [`self.extension`] to `extension`.
    ///
    /// Returns `false` and does nothing if [`self.file_name`] is [`None`],
    /// returns `true` and updates the extension otherwise.
    ///
    /// If [`self.extension`] is [`None`], the extension is added; otherwise
    /// it is replaced.
    ///
    /// [`self.file_name`]: Utf8TypedPath::file_name
    /// [`self.extension`]: Utf8TypedPath::extension
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8TypedPath, Utf8TypedPathBuf};
    ///
    /// let mut p = Utf8TypedPathBuf::from_unix("/feel/the");
    ///
    /// p.set_extension("force");
    /// assert_eq!(Utf8TypedPath::derive("/feel/the.force"), p.to_path());
    ///
    /// p.set_extension("dark_side");
    /// assert_eq!(Utf8TypedPath::derive("/feel/the.dark_side"), p.to_path());
    /// ```
    pub fn set_extension<S: AsRef<str>>(&mut self, extension: S) -> bool {
        impl_typed_fn!(self, set_extension, extension)
    }

    /// Consumes the [`Utf8TypedPathBuf`], yielding its internal [`String`] storage.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// let p = Utf8TypedPathBuf::from_unix("/the/head");
    /// let string = p.into_string();
    /// assert_eq!(string, "/the/head");
    /// ```
    #[inline]
    pub fn into_string(self) -> String {
        impl_typed_fn!(self, into_string)
    }

    /// Invokes [`capacity`] on the underlying instance of [`Vec`].
    ///
    /// [`capacity`]: Vec::capacity
    #[inline]
    pub fn capacity(&self) -> usize {
        impl_typed_fn!(self, capacity)
    }

    /// Invokes [`clear`] on the underlying instance of [`Vec`].
    ///
    /// [`clear`]: Vec::clear
    #[inline]
    pub fn clear(&mut self) {
        impl_typed_fn!(self, clear)
    }

    /// Invokes [`reserve`] on the underlying instance of [`Vec`].
    ///
    /// [`reserve`]: Vec::reserve
    #[inline]
    pub fn reserve(&mut self, additional: usize) {
        impl_typed_fn!(self, reserve, additional)
    }

    /// Invokes [`try_reserve`] on the underlying instance of [`Vec`].
    ///
    /// [`try_reserve`]: Vec::try_reserve
    #[inline]
    pub fn try_reserve(&mut self, additional: usize) -> Result<(), TryReserveError> {
        impl_typed_fn!(self, try_reserve, additional)
    }

    /// Invokes [`reserve_exact`] on the underlying instance of [`Vec`].
    ///
    /// [`reserve_exact`]: Vec::reserve_exact
    #[inline]
    pub fn reserve_exact(&mut self, additional: usize) {
        impl_typed_fn!(self, reserve_exact, additional)
    }

    /// Invokes [`try_reserve_exact`] on the underlying instance of [`Vec`].
    ///
    /// [`try_reserve_exact`]: Vec::try_reserve_exact
    #[inline]
    pub fn try_reserve_exact(&mut self, additional: usize) -> Result<(), TryReserveError> {
        impl_typed_fn!(self, try_reserve_exact, additional)
    }

    /// Invokes [`shrink_to_fit`] on the underlying instance of [`Vec`].
    ///
    /// [`shrink_to_fit`]: Vec::shrink_to_fit
    #[inline]
    pub fn shrink_to_fit(&mut self) {
        impl_typed_fn!(self, shrink_to_fit)
    }

    /// Invokes [`shrink_to`] on the underlying instance of [`Vec`].
    ///
    /// [`shrink_to`]: Vec::shrink_to
    #[inline]
    pub fn shrink_to(&mut self, min_capacity: usize) {
        impl_typed_fn!(self, shrink_to, min_capacity)
    }
}

/// Reimplementation of [`Utf8TypedPath`] methods as we cannot implement [`std::ops::Deref`]
/// directly.
impl Utf8TypedPathBuf {
    /// Yields the underlying [`str`] slice.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// let string = Utf8TypedPathBuf::from("foo.txt").as_str().to_string();
    /// assert_eq!(string, "foo.txt");
    /// ```
    pub fn as_str(&self) -> &str {
        impl_typed_fn!(self, as_str)
    }

    /// Returns `true` if the [`Utf8TypedPathBuf`] is absolute, i.e., if it is independent of
    /// the current directory.
    ///
    /// * On Unix ([`Utf8UnixPathBuf`]]), a path is absolute if it starts with the root, so
    ///   `is_absolute` and [`has_root`] are equivalent.
    ///
    /// * On Windows ([`Utf8WindowsPathBuf`]), a path is absolute if it has a prefix and starts with
    ///   the root: `c:\windows` is absolute, while `c:temp` and `\temp` are not.
    ///
    /// [`Utf8UnixPathBuf`]: crate::Utf8UnixPathBuf
    /// [`Utf8WindowsPathBuf`]: crate::Utf8WindowsPathBuf
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// assert!(!Utf8TypedPathBuf::from("foo.txt").is_absolute());
    /// ```
    ///
    /// [`has_root`]: Utf8TypedPathBuf::has_root
    pub fn is_absolute(&self) -> bool {
        impl_typed_fn!(self, is_absolute)
    }

    /// Returns `true` if the [`Utf8TypedPathBuf`] is relative, i.e., not absolute.
    ///
    /// See [`is_absolute`]'s documentation for more details.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// assert!(Utf8TypedPathBuf::from("foo.txt").is_relative());
    /// ```
    ///
    /// [`is_absolute`]: Utf8TypedPathBuf::is_absolute
    #[inline]
    pub fn is_relative(&self) -> bool {
        impl_typed_fn!(self, is_relative)
    }

    /// Returns `true` if the [`Utf8TypedPathBuf`] has a root.
    ///
    /// * On Unix ([`Utf8UnixPathBuf`]), a path has a root if it begins with `/`.
    ///
    /// * On Windows ([`Utf8WindowsPathBuf`]), a path has a root if it:
    ///     * has no prefix and begins with a separator, e.g., `\windows`
    ///     * has a prefix followed by a separator, e.g., `c:\windows` but not `c:windows`
    ///     * has any non-disk prefix, e.g., `\\server\share`
    ///
    /// [`Utf8UnixPathBuf`]: crate::Utf8UnixPathBuf
    /// [`Utf8WindowsPathBuf`]: crate::Utf8WindowsPathBuf
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// assert!(Utf8TypedPathBuf::from("/etc/passwd").has_root());
    /// ```
    #[inline]
    pub fn has_root(&self) -> bool {
        impl_typed_fn!(self, has_root)
    }

    /// Returns a reference to the path without its final component, if there is one.
    ///
    /// Returns [`None`] if the path terminates in a root or prefix.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// let path = Utf8TypedPathBuf::from("/foo/bar");
    /// let parent = path.parent().unwrap();
    /// assert_eq!(parent, Utf8TypedPathBuf::from("/foo"));
    ///
    /// let grand_parent = parent.parent().unwrap();
    /// assert_eq!(grand_parent, Utf8TypedPathBuf::from("/"));
    /// assert_eq!(grand_parent.parent(), None);
    /// ```
    pub fn parent(&self) -> Option<Utf8TypedPath<'_>> {
        self.to_path().parent()
    }

    /// Produces an iterator over [`Utf8TypedPathBuf`] and its ancestors.
    ///
    /// The iterator will yield the [`Utf8TypedPathBuf`] that is returned if the [`parent`] method
    /// is used zero or more times. That means, the iterator will yield `&self`,
    /// `&self.parent().unwrap()`, `&self.parent().unwrap().parent().unwrap()` and so on. If the
    /// [`parent`] method returns [`None`], the iterator will do likewise. The iterator will always
    /// yield at least one value, namely `&self`.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8TypedPath, Utf8TypedPathBuf};
    ///
    /// let path = Utf8TypedPathBuf::from("/foo/bar");
    /// let mut ancestors = path.ancestors();
    /// assert_eq!(ancestors.next(), Some(Utf8TypedPath::derive("/foo/bar")));
    /// assert_eq!(ancestors.next(), Some(Utf8TypedPath::derive("/foo")));
    /// assert_eq!(ancestors.next(), Some(Utf8TypedPath::derive("/")));
    /// assert_eq!(ancestors.next(), None);
    ///
    /// let path = Utf8TypedPathBuf::from("../foo/bar");
    /// let mut ancestors = path.ancestors();
    /// assert_eq!(ancestors.next(), Some(Utf8TypedPath::derive("../foo/bar")));
    /// assert_eq!(ancestors.next(), Some(Utf8TypedPath::derive("../foo")));
    /// assert_eq!(ancestors.next(), Some(Utf8TypedPath::derive("..")));
    /// assert_eq!(ancestors.next(), Some(Utf8TypedPath::derive("")));
    /// assert_eq!(ancestors.next(), None);
    /// ```
    ///
    /// [`parent`]: Utf8TypedPathBuf::parent
    #[inline]
    pub fn ancestors(&self) -> Utf8TypedAncestors<'_> {
        self.to_path().ancestors()
    }

    /// Returns the final component of the [`Utf8TypedPathBuf`], if there is one.
    ///
    /// If the path is a normal file, this is the file name. If it's the path of a directory, this
    /// is the directory name.
    ///
    /// Returns [`None`] if the path terminates in `..`.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// assert_eq!(Some("bin"), Utf8TypedPathBuf::from("/usr/bin/").file_name());
    /// assert_eq!(Some("foo.txt"), Utf8TypedPathBuf::from("tmp/foo.txt").file_name());
    /// assert_eq!(Some("foo.txt"), Utf8TypedPathBuf::from("foo.txt/.").file_name());
    /// assert_eq!(Some("foo.txt"), Utf8TypedPathBuf::from("foo.txt/.//").file_name());
    /// assert_eq!(None, Utf8TypedPathBuf::from("foo.txt/..").file_name());
    /// assert_eq!(None, Utf8TypedPathBuf::from("/").file_name());
    /// ```
    pub fn file_name(&self) -> Option<&str> {
        impl_typed_fn!(self, file_name)
    }

    /// Returns a path that, when joined onto `base`, yields `self`.
    ///
    /// # Errors
    ///
    /// If `base` is not a prefix of `self` (i.e., [`starts_with`]
    /// returns `false`), returns [`Err`].
    ///
    /// [`starts_with`]: Utf8TypedPathBuf::starts_with
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8TypedPath, Utf8TypedPathBuf};
    ///
    /// let path = Utf8TypedPathBuf::from("/test/haha/foo.txt");
    ///
    /// assert_eq!(path.strip_prefix("/"), Ok(Utf8TypedPath::derive("test/haha/foo.txt")));
    /// assert_eq!(path.strip_prefix("/test"), Ok(Utf8TypedPath::derive("haha/foo.txt")));
    /// assert_eq!(path.strip_prefix("/test/"), Ok(Utf8TypedPath::derive("haha/foo.txt")));
    /// assert_eq!(path.strip_prefix("/test/haha/foo.txt"), Ok(Utf8TypedPath::derive("")));
    /// assert_eq!(path.strip_prefix("/test/haha/foo.txt/"), Ok(Utf8TypedPath::derive("")));
    ///
    /// assert!(path.strip_prefix("test").is_err());
    /// assert!(path.strip_prefix("/haha").is_err());
    ///
    /// let prefix = Utf8TypedPathBuf::from("/test/");
    /// assert_eq!(path.strip_prefix(prefix), Ok(Utf8TypedPath::derive("haha/foo.txt")));
    /// ```
    pub fn strip_prefix(
        &self,
        base: impl AsRef<str>,
    ) -> Result<Utf8TypedPath<'_>, StripPrefixError> {
        match self {
            Self::Unix(p) => p
                .strip_prefix(Utf8UnixPath::new(&base))
                .map(Utf8TypedPath::Unix),
            Self::Windows(p) => p
                .strip_prefix(Utf8WindowsPath::new(&base))
                .map(Utf8TypedPath::Windows),
        }
    }

    /// Determines whether `base` is a prefix of `self`.
    ///
    /// Only considers whole path components to match.
    ///
    /// # Difference from Path
    ///
    /// Unlike [`Utf8Path::starts_with`], this implementation only supports types that implement
    /// `AsRef<str>` instead of `AsRef<Path>`.
    ///
    /// [`Utf8Path::starts_with`]: crate::Utf8Path::starts_with
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// let path = Utf8TypedPathBuf::from("/etc/passwd");
    ///
    /// assert!(path.starts_with("/etc"));
    /// assert!(path.starts_with("/etc/"));
    /// assert!(path.starts_with("/etc/passwd"));
    /// assert!(path.starts_with("/etc/passwd/")); // extra slash is okay
    /// assert!(path.starts_with("/etc/passwd///")); // multiple extra slashes are okay
    ///
    /// assert!(!path.starts_with("/e"));
    /// assert!(!path.starts_with("/etc/passwd.txt"));
    ///
    /// assert!(!Utf8TypedPathBuf::from("/etc/foo.rs").starts_with("/etc/foo"));
    /// ```
    pub fn starts_with(&self, base: impl AsRef<str>) -> bool {
        self.to_path().starts_with(base)
    }

    /// Determines whether `child` is a suffix of `self`.
    ///
    /// Only considers whole path components to match.
    ///
    /// # Difference from Path
    ///
    /// Unlike [`Utf8Path::ends_with`], this implementation only supports types that implement
    /// `AsRef<str>` instead of `AsRef<Path>`.
    ///
    /// [`Utf8Path::ends_with`]: crate::Utf8Path::ends_with
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// let path = Utf8TypedPathBuf::from("/etc/resolv.conf");
    ///
    /// assert!(path.ends_with("resolv.conf"));
    /// assert!(path.ends_with("etc/resolv.conf"));
    /// assert!(path.ends_with("/etc/resolv.conf"));
    ///
    /// assert!(!path.ends_with("/resolv.conf"));
    /// assert!(!path.ends_with("conf")); // use .extension() instead
    /// ```
    pub fn ends_with(&self, child: impl AsRef<str>) -> bool {
        self.to_path().ends_with(child)
    }

    /// Extracts the stem (non-extension) portion of [`self.file_name`].
    ///
    /// [`self.file_name`]: Utf8TypedPathBuf::file_name
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
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// assert_eq!("foo", Utf8TypedPathBuf::from("foo.rs").file_stem().unwrap());
    /// assert_eq!("foo.tar", Utf8TypedPathBuf::from("foo.tar.gz").file_stem().unwrap());
    /// ```
    ///
    pub fn file_stem(&self) -> Option<&str> {
        impl_typed_fn!(self, file_stem)
    }

    /// Extracts the extension of [`self.file_name`], if possible.
    ///
    /// The extension is:
    ///
    /// * [`None`], if there is no file name;
    /// * [`None`], if there is no embedded `.`;
    /// * [`None`], if the file name begins with `.` and has no other `.`s within;
    /// * Otherwise, the portion of the file name after the final `.`
    ///
    /// [`self.file_name`]: Utf8TypedPathBuf::file_name
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// assert_eq!("rs", Utf8TypedPathBuf::from("foo.rs").extension().unwrap());
    /// assert_eq!("gz", Utf8TypedPathBuf::from("foo.tar.gz").extension().unwrap());
    /// ```
    pub fn extension(&self) -> Option<&str> {
        impl_typed_fn!(self, extension)
    }

    /// Returns an owned [`Utf8TypedPathBuf`] by resolving `..` and `.` segments.
    ///
    /// When multiple, sequential path segment separation characters are found (e.g. `/` for Unix
    /// and either `\` or `/` on Windows), they are replaced by a single instance of the
    /// platform-specific path segment separator (`/` on Unix and `\` on Windows).
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// assert_eq!(
    ///     Utf8TypedPathBuf::from("foo/bar//baz/./asdf/quux/..").normalize(),
    ///     Utf8TypedPathBuf::from("foo/bar/baz/asdf"),
    /// );
    /// ```
    ///
    /// When starting with a root directory, any `..` segment whose parent is the root directory
    /// will be filtered out:
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// assert_eq!(
    ///     Utf8TypedPathBuf::from("/../foo").normalize(),
    ///     Utf8TypedPathBuf::from("/foo"),
    /// );
    /// ```
    ///
    /// If any `..` is left unresolved as the path is relative and no parent is found, it is
    /// discarded:
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// assert_eq!(
    ///     Utf8TypedPathBuf::from("../foo/..").normalize(),
    ///     Utf8TypedPathBuf::from(""),
    /// );
    ///
    /// // Windows prefixes also count this way, but the prefix remains
    /// assert_eq!(
    ///     Utf8TypedPathBuf::from(r"C:..\foo\..").normalize(),
    ///     Utf8TypedPathBuf::from(r"C:"),
    /// );
    /// ```
    pub fn normalize(&self) -> Utf8TypedPathBuf {
        self.to_path().normalize()
    }

    /// Converts a path to an absolute form by [`normalizing`] the path, returning a
    /// [`Utf8TypedPathBuf`].
    ///
    /// In the case that the path is relative, the current working directory is prepended prior to
    /// normalizing.
    ///
    /// [`normalizing`]: Utf8TypedPathBuf::normalize
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{utils, Utf8TypedPathBuf, Utf8UnixEncoding};
    ///
    /// // With an absolute path, it is just normalized
    /// let path = Utf8TypedPathBuf::from("/a/b/../c/./d");
    /// assert_eq!(path.absolutize().unwrap(), Utf8TypedPathBuf::from("/a/c/d"));
    ///
    /// // With a relative path, it is first joined with the current working directory
    /// // and then normalized
    /// let cwd = utils::utf8_current_dir().unwrap()
    ///     .with_encoding::<Utf8UnixEncoding>().to_typed_path_buf();
    ///
    /// let path = cwd.join("a/b/../c/./d");
    /// assert_eq!(path.absolutize().unwrap(), cwd.join("a/c/d"));
    /// ```
    #[cfg(all(feature = "std", not(target_family = "wasm")))]
    pub fn absolutize(&self) -> std::io::Result<Utf8TypedPathBuf> {
        self.to_path().absolutize()
    }

    /// Creates an owned [`Utf8TypedPathBuf`] with `path` adjoined to `self`.
    ///
    /// See [`Utf8TypedPathBuf::push`] for more details on what it means to adjoin a path.
    ///
    /// # Difference from Path
    ///
    /// Unlike [`Utf8Path::join`], this implementation only supports types that implement
    /// `AsRef<str>` instead of `AsRef<Path>`.
    ///
    /// [`Utf8Path::join`]: crate::Utf8Path::join
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// assert_eq!(
    ///     Utf8TypedPathBuf::from("/etc").join("passwd"),
    ///     Utf8TypedPathBuf::from("/etc/passwd"),
    /// );
    /// ```
    pub fn join(&self, path: impl AsRef<str>) -> Utf8TypedPathBuf {
        self.to_path().join(path)
    }

    /// Creates an owned [`Utf8TypedPathBuf`] with `path` adjoined to `self`, checking the `path`
    /// to ensure it is safe to join. _When dealing with user-provided paths, this is the preferred
    /// method._
    ///
    /// See [`Utf8TypedPathBuf::push_checked`] for more details on what it means to adjoin a path
    /// safely.
    ///
    /// # Difference from Path
    ///
    /// Unlike [`Utf8Path::join_checked`], this implementation only supports types that implement
    /// `AsRef<str>` instead of `AsRef<Path>`.
    ///
    /// [`Utf8Path::join_checked`]: crate::Utf8Path::join_checked
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{CheckedPathError, Utf8TypedPathBuf};
    ///
    /// // Valid path will join successfully
    /// assert_eq!(
    ///     Utf8TypedPathBuf::from("/etc").join_checked("passwd"),
    ///     Ok(Utf8TypedPathBuf::from("/etc/passwd")),
    /// );
    ///
    /// // Invalid path will fail to join
    /// assert_eq!(
    ///     Utf8TypedPathBuf::from("/etc").join_checked("/sneaky/path"),
    ///     Err(CheckedPathError::UnexpectedRoot),
    /// );
    /// ```
    pub fn join_checked(
        &self,
        path: impl AsRef<str>,
    ) -> Result<Utf8TypedPathBuf, CheckedPathError> {
        self.to_path().join_checked(path)
    }

    /// Creates an owned [`Utf8TypedPathBuf`] like `self` but with the given file name.
    ///
    /// See [`Utf8TypedPathBuf::set_file_name`] for more details.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// let path = Utf8TypedPathBuf::from("/tmp/foo.txt");
    /// assert_eq!(path.with_file_name("bar.txt"), Utf8TypedPathBuf::from("/tmp/bar.txt"));
    ///
    /// let path = Utf8TypedPathBuf::from("/tmp");
    /// assert_eq!(path.with_file_name("var"), Utf8TypedPathBuf::from("/var"));
    /// ```
    pub fn with_file_name<S: AsRef<str>>(&self, file_name: S) -> Utf8TypedPathBuf {
        self.to_path().with_file_name(file_name)
    }

    /// Creates an owned [`Utf8TypedPathBuf`] like `self` but with the given extension.
    ///
    /// See [`Utf8TypedPathBuf::set_extension`] for more details.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// let path = Utf8TypedPathBuf::from("foo.rs");
    /// assert_eq!(path.with_extension("txt"), Utf8TypedPathBuf::from("foo.txt"));
    ///
    /// let path = Utf8TypedPathBuf::from("foo.tar.gz");
    /// assert_eq!(path.with_extension(""), Utf8TypedPathBuf::from("foo.tar"));
    /// assert_eq!(path.with_extension("xz"), Utf8TypedPathBuf::from("foo.tar.xz"));
    /// assert_eq!(path.with_extension("").with_extension("txt"), Utf8TypedPathBuf::from("foo.txt"));
    /// ```
    pub fn with_extension<S: AsRef<str>>(&self, extension: S) -> Utf8TypedPathBuf {
        self.to_path().with_extension(extension)
    }

    /// Produces an iterator over the [`Utf8TypedComponent`]s of the path.
    ///
    /// When parsing the path, there is a small amount of normalization:
    ///
    /// * Repeated separators are ignored, so `a/b` and `a//b` both have
    ///   `a` and `b` as components.
    ///
    /// * Occurrences of `.` are normalized away, except if they are at the
    ///   beginning of the path. For example, `a/./b`, `a/b/`, `a/b/.` and
    ///   `a/b` all have `a` and `b` as components, but `./a/b` starts with
    ///   an additional CurDir component.
    ///
    /// * A trailing slash is normalized away, `/a/b` and `/a/b/` are equivalent.
    ///
    /// Note that no other normalization takes place; in particular, `a/c`
    /// and `a/b/../c` are distinct, to account for the possibility that `b`
    /// is a symbolic link (so its parent isn't `a`).
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8TypedPathBuf, Utf8TypedComponent};
    ///
    /// let path = Utf8TypedPathBuf::from("/tmp/foo.txt");
    /// let mut components = path.components();
    ///
    /// assert!(components.next().unwrap().is_root());
    /// assert_eq!(components.next().unwrap().as_normal_str(), Some("tmp"));
    /// assert_eq!(components.next().unwrap().as_normal_str(), Some("foo.txt"));
    /// assert_eq!(components.next(), None)
    /// ```
    ///
    /// [`Utf8TypedComponent`]: crate::Utf8TypedComponent
    pub fn components(&self) -> Utf8TypedComponents<'_> {
        self.to_path().components()
    }

    /// Produces an iterator over the path's components viewed as [`str`] slices.
    ///
    /// For more information about the particulars of how the path is separated
    /// into components, see [`components`].
    ///
    /// [`components`]: Utf8TypedPath::components
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// let path = Utf8TypedPathBuf::from("/tmp/foo.txt");
    /// let mut it = path.iter();
    ///
    /// assert_eq!(it.next(), Some(typed_path::constants::unix::SEPARATOR_STR));
    /// assert_eq!(it.next(), Some("tmp"));
    /// assert_eq!(it.next(), Some("foo.txt"));
    /// assert_eq!(it.next(), None)
    /// ```
    #[inline]
    pub fn iter(&self) -> Utf8TypedIter<'_> {
        self.to_path().iter()
    }
}

impl fmt::Display for Utf8TypedPathBuf {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Self::Unix(path) => fmt::Display::fmt(path, f),
            Self::Windows(path) => fmt::Display::fmt(path, f),
        }
    }
}

impl AsRef<[u8]> for Utf8TypedPathBuf {
    #[inline]
    fn as_ref(&self) -> &[u8] {
        self.as_str().as_bytes()
    }
}

impl AsRef<str> for Utf8TypedPathBuf {
    #[inline]
    fn as_ref(&self) -> &str {
        self.as_str()
    }
}

impl<'a> From<&'a str> for Utf8TypedPathBuf {
    /// Creates a new typed pathbuf from a byte slice by determining if the path represents a
    /// Windows or Unix path. This is accomplished by first trying to parse as a Windows path. If
    /// the resulting path contains a prefix such as `C:` or begins with a `\`, it is assumed to be
    /// a [`Utf8WindowsPathBuf`]; otherwise, the slice will be represented as a [`Utf8UnixPathBuf`].
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPathBuf;
    ///
    /// assert!(Utf8TypedPathBuf::from(r#"C:\some\path\to\file.txt"#).is_windows());
    /// assert!(Utf8TypedPathBuf::from(r#"\some\path\to\file.txt"#).is_windows());
    /// assert!(Utf8TypedPathBuf::from(r#"/some/path/to/file.txt"#).is_unix());
    ///
    /// // NOTE: If we don't start with a backslash, it's too difficult to
    /// //       determine and we therefore just assume a Unix/POSIX path.
    /// assert!(Utf8TypedPathBuf::from(r#"some\path\to\file.txt"#).is_unix());
    /// assert!(Utf8TypedPathBuf::from("file.txt").is_unix());
    /// assert!(Utf8TypedPathBuf::from("").is_unix());
    /// ```
    #[inline]
    fn from(s: &'a str) -> Self {
        Utf8TypedPath::derive(s).to_path_buf()
    }
}

impl From<String> for Utf8TypedPathBuf {
    #[inline]
    fn from(s: String) -> Self {
        // NOTE: We use the typed path to check the underlying format, and then
        //       create it manually to avoid a clone of the vec itself
        match Utf8TypedPath::derive(s.as_str()) {
            Utf8TypedPath::Unix(_) => Utf8TypedPathBuf::Unix(Utf8UnixPathBuf::from(s)),
            Utf8TypedPath::Windows(_) => Utf8TypedPathBuf::Windows(Utf8WindowsPathBuf::from(s)),
        }
    }
}

impl TryFrom<Utf8TypedPathBuf> for Utf8UnixPathBuf {
    type Error = Utf8TypedPathBuf;

    fn try_from(path: Utf8TypedPathBuf) -> Result<Self, Self::Error> {
        match path {
            Utf8TypedPathBuf::Unix(path) => Ok(path),
            path => Err(path),
        }
    }
}

impl TryFrom<Utf8TypedPathBuf> for Utf8WindowsPathBuf {
    type Error = Utf8TypedPathBuf;

    fn try_from(path: Utf8TypedPathBuf) -> Result<Self, Self::Error> {
        match path {
            Utf8TypedPathBuf::Windows(path) => Ok(path),
            path => Err(path),
        }
    }
}

impl PartialEq<Utf8TypedPath<'_>> for Utf8TypedPathBuf {
    fn eq(&self, path: &Utf8TypedPath<'_>) -> bool {
        path.eq(&self.to_path())
    }
}

impl PartialEq<str> for Utf8TypedPathBuf {
    fn eq(&self, path: &str) -> bool {
        self.as_str() == path
    }
}

impl PartialEq<Utf8TypedPathBuf> for str {
    fn eq(&self, path: &Utf8TypedPathBuf) -> bool {
        self == path.as_str()
    }
}

impl<'a> PartialEq<&'a str> for Utf8TypedPathBuf {
    fn eq(&self, path: &&'a str) -> bool {
        self.as_str() == *path
    }
}

impl PartialEq<Utf8TypedPathBuf> for &str {
    fn eq(&self, path: &Utf8TypedPathBuf) -> bool {
        *self == path.as_str()
    }
}
