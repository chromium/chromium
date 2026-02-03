use alloc::borrow::Cow;
use alloc::collections::TryReserveError;
use core::convert::TryFrom;
#[cfg(all(feature = "std", not(target_family = "wasm")))]
use std::io;
#[cfg(feature = "std")]
use std::path::PathBuf;

use crate::common::{CheckedPathError, StripPrefixError};
use crate::no_std_compat::*;
use crate::typed::{PathType, TypedAncestors, TypedComponents, TypedIter, TypedPath};
use crate::unix::{UnixPath, UnixPathBuf};
use crate::windows::{WindowsPath, WindowsPathBuf};

/// Represents a pathbuf with a known type that can be one of:
///
/// * [`UnixPathBuf`]
/// * [`WindowsPathBuf`]
#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub enum TypedPathBuf {
    Unix(UnixPathBuf),
    Windows(WindowsPathBuf),
}

impl TypedPathBuf {
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

    /// Converts this [`TypedPathBuf`] into the Unix variant.
    pub fn with_unix_encoding(&self) -> TypedPathBuf {
        match self {
            Self::Windows(p) => TypedPathBuf::Unix(p.with_unix_encoding()),
            _ => self.clone(),
        }
    }

    /// Converts this [`TypedPathBuf`] into the Unix variant, ensuring it is a valid Unix path.
    pub fn with_unix_encoding_checked(&self) -> Result<TypedPathBuf, CheckedPathError> {
        Ok(match self {
            Self::Unix(p) => TypedPathBuf::Unix(p.with_unix_encoding_checked()?),
            Self::Windows(p) => TypedPathBuf::Unix(p.with_unix_encoding_checked()?),
        })
    }

    /// Converts this [`TypedPathBuf`] into the Windows variant.
    pub fn with_windows_encoding(&self) -> TypedPathBuf {
        match self {
            Self::Unix(p) => TypedPathBuf::Windows(p.with_windows_encoding()),
            _ => self.clone(),
        }
    }

    /// Converts this [`TypedPathBuf`] into the Windows variant, ensuring it is a valid Windows
    /// path.
    pub fn with_windows_encoding_checked(&self) -> Result<TypedPathBuf, CheckedPathError> {
        Ok(match self {
            Self::Unix(p) => TypedPathBuf::Windows(p.with_windows_encoding_checked()?),
            Self::Windows(p) => TypedPathBuf::Windows(p.with_windows_encoding_checked()?),
        })
    }

    /// Allocates an empty [`TypedPathBuf`] for the specified path type.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{PathType, TypedPathBuf};
    /// let _unix_path = TypedPathBuf::new(PathType::Unix);
    /// let _windows_path = TypedPathBuf::new(PathType::Windows);
    /// ```
    #[inline]
    pub fn new(r#type: PathType) -> Self {
        match r#type {
            PathType::Unix => Self::unix(),
            PathType::Windows => Self::windows(),
        }
    }

    /// Allocates an empty [`TypedPathBuf`] as a Unix path.
    #[inline]
    pub fn unix() -> Self {
        Self::Unix(UnixPathBuf::new())
    }

    /// Allocates an empty [`TypedPathBuf`] as a Windows path.
    #[inline]
    pub fn windows() -> Self {
        Self::Windows(WindowsPathBuf::new())
    }

    /// Creates a new [`TypedPathBuf`] from the bytes representing a Unix path.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    /// let path = TypedPathBuf::from_unix("/tmp");
    /// ```
    pub fn from_unix(s: impl AsRef<[u8]>) -> Self {
        Self::Unix(UnixPathBuf::from(s.as_ref()))
    }

    /// Creates a new [`TypedPathBuf`] from the bytes representing a Windows path.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    /// let path = TypedPathBuf::from_windows(r"C:\tmp");
    /// ```
    pub fn from_windows(s: impl AsRef<[u8]>) -> Self {
        Self::Windows(WindowsPathBuf::from(s.as_ref()))
    }

    /// Converts into a [`TypedPath`].
    pub fn to_path(&self) -> TypedPath<'_> {
        match self {
            Self::Unix(path) => TypedPath::Unix(path.as_path()),
            Self::Windows(path) => TypedPath::Windows(path.as_path()),
        }
    }

    /// Extends `self` with `path`.
    ///
    /// If `path` is absolute, it replaces the current path.
    ///
    /// With [`WindowsPathBuf`]:
    ///
    /// * if `path` has a root but no prefix (e.g., `\windows`), it
    ///   replaces everything except for the prefix (if any) of `self`.
    /// * if `path` has a prefix but no root, it replaces `self`.
    /// * if `self` has a verbatim prefix (e.g. `\\?\C:\windows`)
    ///   and `path` is not empty, the new path is normalized: all references
    ///   to `.` and `..` are removed.
    ///
    /// [`WindowsPathBuf`]: crate::WindowsPathBuf
    ///
    /// # Difference from PathBuf
    ///
    /// Unlike [`PathBuf::push`], this implementation only supports types that implement
    /// `AsRef<[u8]>` instead of `AsRef<Path>`.
    ///
    /// [`PathBuf::push`]: crate::PathBuf::push
    ///
    /// # Examples
    ///
    /// Pushing a relative path extends the existing path:
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// let mut path = TypedPathBuf::from_unix("/tmp");
    /// path.push("file.bk");
    /// assert_eq!(path, TypedPathBuf::from_unix("/tmp/file.bk"));
    /// ```
    ///
    /// Pushing an absolute path replaces the existing path:
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// let mut path = TypedPathBuf::from_unix("/tmp");
    /// path.push("/etc");
    /// assert_eq!(path, TypedPathBuf::from_unix("/etc"));
    /// ```
    pub fn push(&mut self, path: impl AsRef<[u8]>) {
        match self {
            Self::Unix(a) => a.push(UnixPath::new(&path)),
            Self::Windows(a) => a.push(WindowsPath::new(&path)),
        }
    }

    /// Like [`TypedPathBuf::push`], extends `self` with `path`, but also checks to ensure that
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
    /// `AsRef<[u8]>` instead of `AsRef<Path>`.
    ///
    /// [`PathBuf::push_checked`]: crate::PathBuf::push_checked
    ///
    /// # Examples
    ///
    /// Pushing a relative path extends the existing path:
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// let mut path = TypedPathBuf::from_unix("/tmp");
    /// assert!(path.push_checked("file.bk").is_ok());
    /// assert_eq!(path, TypedPathBuf::from_unix("/tmp/file.bk"));
    /// ```
    ///
    /// Pushing a relative path that contains unresolved parent directory references fails
    /// with an error:
    ///
    /// ```
    /// use typed_path::{CheckedPathError, TypedPathBuf};
    ///
    /// let mut path = TypedPathBuf::from_unix("/tmp");
    ///
    /// // Pushing a relative path that contains parent directory references that cannot be
    /// // resolved within the path is considered an error as this is considered a path
    /// // traversal attack!
    /// assert_eq!(path.push_checked(".."), Err(CheckedPathError::PathTraversalAttack));
    /// assert_eq!(path, TypedPathBuf::from("/tmp"));
    /// ```
    ///
    /// Pushing an absolute path fails with an error:
    ///
    /// ```
    /// use typed_path::{CheckedPathError, TypedPathBuf};
    ///
    /// let mut path = TypedPathBuf::from_unix("/tmp");
    ///
    /// // Pushing an absolute path will fail with an error
    /// assert_eq!(path.push_checked("/etc"), Err(CheckedPathError::UnexpectedRoot));
    /// assert_eq!(path, TypedPathBuf::from_unix("/tmp"));
    /// ```
    pub fn push_checked(&mut self, path: impl AsRef<[u8]>) -> Result<(), CheckedPathError> {
        match self {
            Self::Unix(a) => a.push_checked(UnixPath::new(&path)),
            Self::Windows(a) => a.push_checked(WindowsPath::new(&path)),
        }
    }

    /// Truncates `self` to [`self.parent`].
    ///
    /// Returns `false` and does nothing if [`self.parent`] is [`None`].
    /// Otherwise, returns `true`.
    ///
    /// [`self.parent`]: TypedPath::parent
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{TypedPath, TypedPathBuf};
    ///
    /// let mut p = TypedPathBuf::from_unix("/spirited/away.rs");
    ///
    /// p.pop();
    /// assert_eq!(TypedPath::derive("/spirited"), p);
    /// p.pop();
    /// assert_eq!(TypedPath::derive("/"), p);
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
    /// [`self.file_name`]: TypedPath::file_name
    /// [`pop`]: TypedPathBuf::pop
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// let mut buf = TypedPathBuf::from_unix("/");
    /// assert!(buf.file_name() == None);
    /// buf.set_file_name("bar");
    /// assert!(buf == TypedPathBuf::from_unix("/bar"));
    /// assert!(buf.file_name().is_some());
    /// buf.set_file_name("baz.txt");
    /// assert!(buf == TypedPathBuf::from_unix("/baz.txt"));
    /// ```
    pub fn set_file_name<S: AsRef<[u8]>>(&mut self, file_name: S) {
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
    /// [`self.file_name`]: TypedPath::file_name
    /// [`self.extension`]: TypedPath::extension
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{TypedPath, TypedPathBuf};
    ///
    /// let mut p = TypedPathBuf::from_unix("/feel/the");
    ///
    /// p.set_extension("force");
    /// assert_eq!(TypedPath::derive("/feel/the.force"), p.to_path());
    ///
    /// p.set_extension("dark_side");
    /// assert_eq!(TypedPath::derive("/feel/the.dark_side"), p.to_path());
    /// ```
    pub fn set_extension<S: AsRef<[u8]>>(&mut self, extension: S) -> bool {
        impl_typed_fn!(self, set_extension, extension)
    }

    /// Consumes the [`TypedPathBuf`], yielding its internal [`Vec<u8>`] storage.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// let p = TypedPathBuf::from_unix("/the/head");
    /// let vec = p.into_vec();
    /// assert_eq!(vec, b"/the/head");
    /// ```
    #[inline]
    pub fn into_vec(self) -> Vec<u8> {
        impl_typed_fn!(self, into_vec)
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

/// Reimplementation of [`TypedPath`] methods as we cannot implement [`std::ops::Deref`] directly.
impl TypedPathBuf {
    /// Yields the underlying [`[u8]`] slice.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// let bytes = TypedPathBuf::from("foo.txt").as_bytes().to_vec();
    /// assert_eq!(bytes, b"foo.txt");
    /// ```
    pub fn as_bytes(&self) -> &[u8] {
        impl_typed_fn!(self, as_bytes)
    }

    /// Yields a [`&str`] slice if the [`TypedPathBuf`] is valid unicode.
    ///
    /// This conversion may entail doing a check for UTF-8 validity.
    /// Note that validation is performed because non-UTF-8 strings are
    /// perfectly valid for some OS.
    ///
    /// [`&str`]: str
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// let path = TypedPathBuf::from("foo.txt");
    /// assert_eq!(path.to_str(), Some("foo.txt"));
    /// ```
    #[inline]
    pub fn to_str(&self) -> Option<&str> {
        impl_typed_fn!(self, to_str)
    }

    /// Converts a [`TypedPathBuf`] to a [`Cow<str>`].
    ///
    /// Any non-Unicode sequences are replaced with
    /// [`U+FFFD REPLACEMENT CHARACTER`][U+FFFD].
    ///
    /// [U+FFFD]: std::char::REPLACEMENT_CHARACTER
    ///
    /// # Examples
    ///
    /// Calling `to_string_lossy` on a [`TypedPathBuf`] with valid unicode:
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// let path = TypedPathBuf::from("foo.txt");
    /// assert_eq!(path.to_string_lossy(), "foo.txt");
    /// ```
    ///
    /// Had `path` contained invalid unicode, the `to_string_lossy` call might
    /// have returned `"fo�.txt"`.
    #[inline]
    pub fn to_string_lossy(&self) -> Cow<'_, str> {
        impl_typed_fn!(self, to_string_lossy)
    }

    /// Returns `true` if the [`TypedPathBuf`] is absolute, i.e., if it is independent of
    /// the current directory.
    ///
    /// * On Unix ([`UnixPathBuf`]]), a path is absolute if it starts with the root, so
    ///   `is_absolute` and [`has_root`] are equivalent.
    ///
    /// * On Windows ([`WindowsPathBuf`]), a path is absolute if it has a prefix and starts with
    ///   the root: `c:\windows` is absolute, while `c:temp` and `\temp` are not.
    ///
    /// [`UnixPathBuf`]: crate::UnixPathBuf
    /// [`WindowsPathBuf`]: crate::WindowsPathBuf
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// assert!(!TypedPathBuf::from("foo.txt").is_absolute());
    /// ```
    ///
    /// [`has_root`]: TypedPathBuf::has_root
    pub fn is_absolute(&self) -> bool {
        impl_typed_fn!(self, is_absolute)
    }

    /// Returns `true` if the [`TypedPathBuf`] is relative, i.e., not absolute.
    ///
    /// See [`is_absolute`]'s documentation for more details.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// assert!(TypedPathBuf::from("foo.txt").is_relative());
    /// ```
    ///
    /// [`is_absolute`]: TypedPathBuf::is_absolute
    #[inline]
    pub fn is_relative(&self) -> bool {
        impl_typed_fn!(self, is_relative)
    }

    /// Returns `true` if the [`TypedPathBuf`] has a root.
    ///
    /// * On Unix ([`UnixPathBuf`]), a path has a root if it begins with `/`.
    ///
    /// * On Windows ([`WindowsPathBuf`]), a path has a root if it:
    ///     * has no prefix and begins with a separator, e.g., `\windows`
    ///     * has a prefix followed by a separator, e.g., `c:\windows` but not `c:windows`
    ///     * has any non-disk prefix, e.g., `\\server\share`
    ///
    /// [`UnixPathBuf`]: crate::UnixPathBuf
    /// [`WindowsPathBuf`]: crate::WindowsPathBuf
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// assert!(TypedPathBuf::from("/etc/passwd").has_root());
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
    /// use typed_path::TypedPathBuf;
    ///
    /// let path = TypedPathBuf::from("/foo/bar");
    /// let parent = path.parent().unwrap();
    /// assert_eq!(parent, TypedPathBuf::from("/foo"));
    ///
    /// let grand_parent = parent.parent().unwrap();
    /// assert_eq!(grand_parent, TypedPathBuf::from("/"));
    /// assert_eq!(grand_parent.parent(), None);
    /// ```
    pub fn parent(&self) -> Option<TypedPath<'_>> {
        self.to_path().parent()
    }

    /// Produces an iterator over [`TypedPathBuf`] and its ancestors.
    ///
    /// The iterator will yield the [`TypedPathBuf`] that is returned if the [`parent`] method is
    /// used zero or more times. That means, the iterator will yield `&self`,
    /// `&self.parent().unwrap()`, `&self.parent().unwrap().parent().unwrap()` and so on. If the
    /// [`parent`] method returns [`None`], the iterator will do likewise. The iterator will always
    /// yield at least one value, namely `&self`.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{TypedPath, TypedPathBuf};
    ///
    /// let path = TypedPathBuf::from("/foo/bar");
    /// let mut ancestors = path.ancestors();
    /// assert_eq!(ancestors.next(), Some(TypedPath::derive("/foo/bar")));
    /// assert_eq!(ancestors.next(), Some(TypedPath::derive("/foo")));
    /// assert_eq!(ancestors.next(), Some(TypedPath::derive("/")));
    /// assert_eq!(ancestors.next(), None);
    ///
    /// let path = TypedPathBuf::from("../foo/bar");
    /// let mut ancestors = path.ancestors();
    /// assert_eq!(ancestors.next(), Some(TypedPath::derive("../foo/bar")));
    /// assert_eq!(ancestors.next(), Some(TypedPath::derive("../foo")));
    /// assert_eq!(ancestors.next(), Some(TypedPath::derive("..")));
    /// assert_eq!(ancestors.next(), Some(TypedPath::derive("")));
    /// assert_eq!(ancestors.next(), None);
    /// ```
    ///
    /// [`parent`]: TypedPathBuf::parent
    #[inline]
    pub fn ancestors(&self) -> TypedAncestors<'_> {
        self.to_path().ancestors()
    }

    /// Returns the final component of the [`TypedPathBuf`], if there is one.
    ///
    /// If the path is a normal file, this is the file name. If it's the path of a directory, this
    /// is the directory name.
    ///
    /// Returns [`None`] if the path terminates in `..`.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// assert_eq!(Some(b"bin".as_slice()), TypedPathBuf::from("/usr/bin/").file_name());
    /// assert_eq!(Some(b"foo.txt".as_slice()), TypedPathBuf::from("tmp/foo.txt").file_name());
    /// assert_eq!(Some(b"foo.txt".as_slice()), TypedPathBuf::from("foo.txt/.").file_name());
    /// assert_eq!(Some(b"foo.txt".as_slice()), TypedPathBuf::from("foo.txt/.//").file_name());
    /// assert_eq!(None, TypedPathBuf::from("foo.txt/..").file_name());
    /// assert_eq!(None, TypedPathBuf::from("/").file_name());
    /// ```
    pub fn file_name(&self) -> Option<&[u8]> {
        impl_typed_fn!(self, file_name)
    }

    /// Returns a path that, when joined onto `base`, yields `self`.
    ///
    /// # Errors
    ///
    /// If `base` is not a prefix of `self` (i.e., [`starts_with`]
    /// returns `false`), returns [`Err`].
    ///
    /// [`starts_with`]: TypedPathBuf::starts_with
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{TypedPath, TypedPathBuf};
    ///
    /// let path = TypedPathBuf::from("/test/haha/foo.txt");
    ///
    /// assert_eq!(path.strip_prefix("/"), Ok(TypedPath::derive("test/haha/foo.txt")));
    /// assert_eq!(path.strip_prefix("/test"), Ok(TypedPath::derive("haha/foo.txt")));
    /// assert_eq!(path.strip_prefix("/test/"), Ok(TypedPath::derive("haha/foo.txt")));
    /// assert_eq!(path.strip_prefix("/test/haha/foo.txt"), Ok(TypedPath::derive("")));
    /// assert_eq!(path.strip_prefix("/test/haha/foo.txt/"), Ok(TypedPath::derive("")));
    ///
    /// assert!(path.strip_prefix("test").is_err());
    /// assert!(path.strip_prefix("/haha").is_err());
    ///
    /// let prefix = TypedPathBuf::from("/test/");
    /// assert_eq!(path.strip_prefix(prefix), Ok(TypedPath::derive("haha/foo.txt")));
    /// ```
    pub fn strip_prefix(&self, base: impl AsRef<[u8]>) -> Result<TypedPath<'_>, StripPrefixError> {
        match self {
            Self::Unix(p) => p.strip_prefix(UnixPath::new(&base)).map(TypedPath::Unix),
            Self::Windows(p) => p
                .strip_prefix(WindowsPath::new(&base))
                .map(TypedPath::Windows),
        }
    }

    /// Determines whether `base` is a prefix of `self`.
    ///
    /// Only considers whole path components to match.
    ///
    /// # Difference from Path
    ///
    /// Unlike [`Path::starts_with`], this implementation only supports types that implement
    /// `AsRef<[u8]>` instead of `AsRef<Path>`.
    ///
    /// [`Path::starts_with`]: crate::Path::starts_with
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// let path = TypedPathBuf::from("/etc/passwd");
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
    /// assert!(!TypedPathBuf::from("/etc/foo.rs").starts_with("/etc/foo"));
    /// ```
    pub fn starts_with(&self, base: impl AsRef<[u8]>) -> bool {
        self.to_path().starts_with(base)
    }

    /// Determines whether `child` is a suffix of `self`.
    ///
    /// Only considers whole path components to match.
    ///
    /// # Difference from Path
    ///
    /// Unlike [`Path::ends_with`], this implementation only supports types that implement
    /// `AsRef<[u8]>` instead of `AsRef<Path>`.
    ///
    /// [`Path::ends_with`]: crate::Path::ends_with
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// let path = TypedPathBuf::from("/etc/resolv.conf");
    ///
    /// assert!(path.ends_with("resolv.conf"));
    /// assert!(path.ends_with("etc/resolv.conf"));
    /// assert!(path.ends_with("/etc/resolv.conf"));
    ///
    /// assert!(!path.ends_with("/resolv.conf"));
    /// assert!(!path.ends_with("conf")); // use .extension() instead
    /// ```
    pub fn ends_with(&self, child: impl AsRef<[u8]>) -> bool {
        self.to_path().ends_with(child)
    }

    /// Extracts the stem (non-extension) portion of [`self.file_name`].
    ///
    /// [`self.file_name`]: TypedPathBuf::file_name
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
    /// use typed_path::TypedPathBuf;
    ///
    /// assert_eq!(b"foo", TypedPathBuf::from("foo.rs").file_stem().unwrap());
    /// assert_eq!(b"foo.tar", TypedPathBuf::from("foo.tar.gz").file_stem().unwrap());
    /// ```
    ///
    pub fn file_stem(&self) -> Option<&[u8]> {
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
    /// [`self.file_name`]: TypedPathBuf::file_name
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert_eq!(b"rs", TypedPathBuf::from("foo.rs").extension().unwrap());
    /// assert_eq!(b"gz", TypedPathBuf::from("foo.tar.gz").extension().unwrap());
    /// ```
    pub fn extension(&self) -> Option<&[u8]> {
        impl_typed_fn!(self, extension)
    }

    /// Returns an owned [`TypedPathBuf`] by resolving `..` and `.` segments.
    ///
    /// When multiple, sequential path segment separation characters are found (e.g. `/` for Unix
    /// and either `\` or `/` on Windows), they are replaced by a single instance of the
    /// platform-specific path segment separator (`/` on Unix and `\` on Windows).
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// assert_eq!(
    ///     TypedPathBuf::from("foo/bar//baz/./asdf/quux/..").normalize(),
    ///     TypedPathBuf::from("foo/bar/baz/asdf"),
    /// );
    /// ```
    ///
    /// When starting with a root directory, any `..` segment whose parent is the root directory
    /// will be filtered out:
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// assert_eq!(
    ///     TypedPathBuf::from("/../foo").normalize(),
    ///     TypedPathBuf::from("/foo"),
    /// );
    /// ```
    ///
    /// If any `..` is left unresolved as the path is relative and no parent is found, it is
    /// discarded:
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// assert_eq!(
    ///     TypedPathBuf::from("../foo/..").normalize(),
    ///     TypedPathBuf::from(""),
    /// );
    ///
    /// // Windows prefixes also count this way, but the prefix remains
    /// assert_eq!(
    ///     TypedPathBuf::from(r"C:..\foo\..").normalize(),
    ///     TypedPathBuf::from(r"C:"),
    /// );
    /// ```
    pub fn normalize(&self) -> TypedPathBuf {
        self.to_path().normalize()
    }

    /// Converts a path to an absolute form by [`normalizing`] the path, returning a
    /// [`TypedPathBuf`].
    ///
    /// In the case that the path is relative, the current working directory is prepended prior to
    /// normalizing.
    ///
    /// [`normalizing`]: TypedPathBuf::normalize
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{utils, TypedPathBuf, UnixEncoding};
    ///
    /// // With an absolute path, it is just normalized
    /// let path = TypedPathBuf::from("/a/b/../c/./d");
    /// assert_eq!(path.absolutize().unwrap(), TypedPathBuf::from("/a/c/d"));
    ///
    /// // With a relative path, it is first joined with the current working directory
    /// // and then normalized
    /// let cwd = utils::current_dir().unwrap().with_encoding::<UnixEncoding>().to_typed_path_buf();
    /// let path = cwd.join("a/b/../c/./d");
    /// assert_eq!(path.absolutize().unwrap(), cwd.join("a/c/d"));
    /// ```
    #[cfg(all(feature = "std", not(target_family = "wasm")))]
    pub fn absolutize(&self) -> io::Result<TypedPathBuf> {
        self.to_path().absolutize()
    }

    /// Creates an owned [`TypedPathBuf`] with `path` adjoined to `self`.
    ///
    /// See [`TypedPathBuf::push`] for more details on what it means to adjoin a path.
    ///
    /// # Difference from Path
    ///
    /// Unlike [`Path::join`], this implementation only supports types that implement
    /// `AsRef<[u8]>` instead of `AsRef<Path>`.
    ///
    /// [`Path::join`]: crate::Path::join
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// assert_eq!(
    ///     TypedPathBuf::from("/etc").join("passwd"),
    ///     TypedPathBuf::from("/etc/passwd"),
    /// );
    /// ```
    pub fn join(&self, path: impl AsRef<[u8]>) -> TypedPathBuf {
        self.to_path().join(path)
    }

    /// Creates an owned [`TypedPathBuf`] with `path` adjoined to `self`, checking the `path` to
    /// ensure it is safe to join. _When dealing with user-provided paths, this is the preferred
    /// method._
    ///
    /// See [`TypedPathBuf::push_checked`] for more details on what it means to adjoin a path
    /// safely.
    ///
    /// # Difference from Path
    ///
    /// Unlike [`Path::join_checked`], this implementation only supports types that implement
    /// `AsRef<[u8]>` instead of `AsRef<Path>`.
    ///
    /// [`Path::join_checked`]: crate::Path::join_checked
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{CheckedPathError, TypedPathBuf};
    ///
    /// // Valid path will join successfully
    /// assert_eq!(
    ///     TypedPathBuf::from("/etc").join_checked("passwd"),
    ///     Ok(TypedPathBuf::from("/etc/passwd")),
    /// );
    ///
    /// // Invalid path will fail to join
    /// assert_eq!(
    ///     TypedPathBuf::from("/etc").join_checked("/sneaky/path"),
    ///     Err(CheckedPathError::UnexpectedRoot),
    /// );
    /// ```
    pub fn join_checked(&self, path: impl AsRef<[u8]>) -> Result<TypedPathBuf, CheckedPathError> {
        self.to_path().join_checked(path)
    }

    /// Creates an owned [`TypedPathBuf`] like `self` but with the given file name.
    ///
    /// See [`TypedPathBuf::set_file_name`] for more details.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// let path = TypedPathBuf::from("/tmp/foo.txt");
    /// assert_eq!(path.with_file_name("bar.txt"), TypedPathBuf::from("/tmp/bar.txt"));
    ///
    /// let path = TypedPathBuf::from("/tmp");
    /// assert_eq!(path.with_file_name("var"), TypedPathBuf::from("/var"));
    /// ```
    pub fn with_file_name<S: AsRef<[u8]>>(&self, file_name: S) -> TypedPathBuf {
        self.to_path().with_file_name(file_name)
    }

    /// Creates an owned [`TypedPathBuf`] like `self` but with the given extension.
    ///
    /// See [`TypedPathBuf::set_extension`] for more details.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// let path = TypedPathBuf::from("foo.rs");
    /// assert_eq!(path.with_extension("txt"), TypedPathBuf::from("foo.txt"));
    ///
    /// let path = TypedPathBuf::from("foo.tar.gz");
    /// assert_eq!(path.with_extension(""), TypedPathBuf::from("foo.tar"));
    /// assert_eq!(path.with_extension("xz"), TypedPathBuf::from("foo.tar.xz"));
    /// assert_eq!(path.with_extension("").with_extension("txt"), TypedPathBuf::from("foo.txt"));
    /// ```
    pub fn with_extension<S: AsRef<[u8]>>(&self, extension: S) -> TypedPathBuf {
        self.to_path().with_extension(extension)
    }

    /// Produces an iterator over the [`TypedComponent`]s of the path.
    ///
    /// When parsing the path, there is a small amount of normalization:
    ///
    /// * Repeated separators are ignored, so `a/b` and `a//b` both have
    ///   `a` and `b` as components.
    ///
    /// * Occurrences of `.` are normalized away, except if they are at the
    ///   beginning of the path. For example, `a/./b`, `a/b/`, `a/b/.` and
    ///   `a/b` all have `a` and `b` as components, but `./a/b` starts with
    ///   an additional `CurDir` component.
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
    /// use typed_path::{TypedPathBuf, TypedComponent};
    ///
    /// let path = TypedPathBuf::from("/tmp/foo.txt");
    /// let mut components = path.components();
    ///
    /// assert!(components.next().unwrap().is_root());
    /// assert_eq!(components.next().unwrap().as_normal_bytes(), Some(b"tmp".as_slice()));
    /// assert_eq!(components.next().unwrap().as_normal_bytes(), Some(b"foo.txt".as_slice()));
    /// assert_eq!(components.next(), None)
    /// ```
    ///
    /// [`TypedComponent`]: crate::TypedComponent
    pub fn components(&self) -> TypedComponents<'_> {
        self.to_path().components()
    }

    /// Produces an iterator over the path's components viewed as [`[u8]`] slices.
    ///
    /// For more information about the particulars of how the path is separated
    /// into components, see [`components`].
    ///
    /// [`components`]: TypedPath::components
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// let path = TypedPathBuf::from("/tmp/foo.txt");
    /// let mut it = path.iter();
    ///
    /// assert_eq!(it.next(), Some(typed_path::constants::unix::SEPARATOR_STR.as_bytes()));
    /// assert_eq!(it.next(), Some(b"tmp".as_slice()));
    /// assert_eq!(it.next(), Some(b"foo.txt".as_slice()));
    /// assert_eq!(it.next(), None)
    /// ```
    #[inline]
    pub fn iter(&self) -> TypedIter<'_> {
        self.to_path().iter()
    }
}

impl AsRef<[u8]> for TypedPathBuf {
    #[inline]
    fn as_ref(&self) -> &[u8] {
        self.as_bytes()
    }
}

impl<'a, const N: usize> From<&'a [u8; N]> for TypedPathBuf {
    #[inline]
    fn from(s: &'a [u8; N]) -> Self {
        TypedPathBuf::from(s.as_slice())
    }
}

impl<'a> From<&'a [u8]> for TypedPathBuf {
    /// Creates a new typed pathbuf from a byte slice by determining if the path represents a
    /// Windows or Unix path. This is accomplished by first trying to parse as a Windows path. If
    /// the resulting path contains a prefix such as `C:` or begins with a `\`, it is assumed to be
    /// a [`WindowsPathBuf`]; otherwise, the slice will be represented as a [`UnixPathBuf`].
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::TypedPathBuf;
    ///
    /// assert!(TypedPathBuf::from(br#"C:\some\path\to\file.txt"#).is_windows());
    /// assert!(TypedPathBuf::from(br#"\some\path\to\file.txt"#).is_windows());
    /// assert!(TypedPathBuf::from(br#"/some/path/to/file.txt"#).is_unix());
    ///
    /// // NOTE: If we don't start with a backslash, it's too difficult to
    /// //       determine and we therefore just assume a Unix/POSIX path.
    /// assert!(TypedPathBuf::from(br#"some\path\to\file.txt"#).is_unix());
    /// assert!(TypedPathBuf::from(b"file.txt").is_unix());
    /// assert!(TypedPathBuf::from(b"").is_unix());
    /// ```
    #[inline]
    fn from(s: &'a [u8]) -> Self {
        TypedPath::derive(s).to_path_buf()
    }
}

impl From<Vec<u8>> for TypedPathBuf {
    #[inline]
    fn from(s: Vec<u8>) -> Self {
        // NOTE: We use the typed path to check the underlying format, and then
        //       create it manually to avoid a clone of the vec itself
        match TypedPath::derive(s.as_slice()) {
            TypedPath::Unix(_) => TypedPathBuf::Unix(UnixPathBuf::from(s)),
            TypedPath::Windows(_) => TypedPathBuf::Windows(WindowsPathBuf::from(s)),
        }
    }
}

impl<'a> From<&'a str> for TypedPathBuf {
    #[inline]
    fn from(s: &'a str) -> Self {
        TypedPathBuf::from(s.as_bytes())
    }
}

impl From<String> for TypedPathBuf {
    #[inline]
    fn from(s: String) -> Self {
        // NOTE: We use the typed path to check the underlying format, and then
        //       create it manually to avoid a clone of the string itself
        match TypedPath::derive(s.as_bytes()) {
            TypedPath::Unix(_) => TypedPathBuf::Unix(UnixPathBuf::from(s)),
            TypedPath::Windows(_) => TypedPathBuf::Windows(WindowsPathBuf::from(s)),
        }
    }
}

impl TryFrom<TypedPathBuf> for UnixPathBuf {
    type Error = TypedPathBuf;

    fn try_from(path: TypedPathBuf) -> Result<Self, Self::Error> {
        match path {
            TypedPathBuf::Unix(path) => Ok(path),
            path => Err(path),
        }
    }
}

impl TryFrom<TypedPathBuf> for WindowsPathBuf {
    type Error = TypedPathBuf;

    fn try_from(path: TypedPathBuf) -> Result<Self, Self::Error> {
        match path {
            TypedPathBuf::Windows(path) => Ok(path),
            path => Err(path),
        }
    }
}

#[cfg(feature = "std")]
impl TryFrom<TypedPathBuf> for PathBuf {
    type Error = TypedPathBuf;

    fn try_from(path: TypedPathBuf) -> Result<Self, Self::Error> {
        match path {
            #[cfg(unix)]
            TypedPathBuf::Unix(path) => PathBuf::try_from(path).map_err(TypedPathBuf::Unix),
            #[cfg(windows)]
            TypedPathBuf::Windows(path) => PathBuf::try_from(path).map_err(TypedPathBuf::Windows),
            path => Err(path),
        }
    }
}

impl PartialEq<TypedPath<'_>> for TypedPathBuf {
    fn eq(&self, path: &TypedPath<'_>) -> bool {
        path.eq(&self.to_path())
    }
}
