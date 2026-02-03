use core::fmt;

use crate::common::{CheckedPathError, StripPrefixError, TryAsRef};
use crate::typed::{
    PathType, Utf8TypedAncestors, Utf8TypedComponents, Utf8TypedIter, Utf8TypedPathBuf,
};
use crate::unix::Utf8UnixPath;
use crate::windows::Utf8WindowsPath;

/// Represents a path with a known type that can be one of:
///
/// * [`Utf8UnixPath`]
/// * [`Utf8WindowsPath`]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub enum Utf8TypedPath<'a> {
    Unix(&'a Utf8UnixPath),
    Windows(&'a Utf8WindowsPath),
}

impl<'a> Utf8TypedPath<'a> {
    /// Creates a new path with the given type as its encoding.
    pub fn new<S: AsRef<str> + ?Sized>(s: &'a S, r#type: PathType) -> Self {
        match r#type {
            PathType::Unix => Self::unix(s),
            PathType::Windows => Self::windows(s),
        }
    }

    /// Creates a new typed Unix path.
    #[inline]
    pub fn unix<S: AsRef<str> + ?Sized>(s: &'a S) -> Self {
        Self::Unix(Utf8UnixPath::new(s))
    }

    /// Creates a new typed Windows path.
    #[inline]
    pub fn windows<S: AsRef<str> + ?Sized>(s: &'a S) -> Self {
        Self::Windows(Utf8WindowsPath::new(s))
    }

    /// Creates a new typed path from a byte slice by determining if the path represents a Windows
    /// or Unix path. This is accomplished by first trying to parse as a Windows path. If the
    /// resulting path contains a prefix such as `C:` or begins with a `\`, it is assumed to be a
    /// [`Utf8WindowsPath`]; otherwise, the slice will be represented as a [`Utf8UnixPath`].
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPath;
    ///
    /// assert!(Utf8TypedPath::derive(r#"C:\some\path\to\file.txt"#).is_windows());
    /// assert!(Utf8TypedPath::derive(r#"\some\path\to\file.txt"#).is_windows());
    /// assert!(Utf8TypedPath::derive(r#"/some/path/to/file.txt"#).is_unix());
    ///
    /// // NOTE: If we don't start with a backslash, it's too difficult to
    /// //       determine and we therefore just assume a Unix/POSIX path.
    /// assert!(Utf8TypedPath::derive(r#"some\path\to\file.txt"#).is_unix());
    /// assert!(Utf8TypedPath::derive("file.txt").is_unix());
    /// assert!(Utf8TypedPath::derive("").is_unix());
    /// ```
    pub fn derive<S: AsRef<str> + ?Sized>(s: &'a S) -> Self {
        let winpath = Utf8WindowsPath::new(s);
        if s.as_ref().starts_with('\\') || winpath.components().has_prefix() {
            Self::Windows(winpath)
        } else {
            Self::Unix(Utf8UnixPath::new(s))
        }
    }

    /// Yields the underlying [`str`] slice.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPath;
    ///
    /// let string = Utf8TypedPath::derive("foo.txt").as_str().to_string();
    /// assert_eq!(string, "foo.txt");
    /// ```
    pub fn as_str(&self) -> &str {
        impl_typed_fn!(self, as_str)
    }

    /// Converts a [`Utf8TypedPath`] into a [`Utf8TypedPathBuf`].
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8TypedPath, Utf8TypedPathBuf};
    ///
    /// let path_buf = Utf8TypedPath::derive("foo.txt").to_path_buf();
    /// assert_eq!(path_buf, Utf8TypedPathBuf::from("foo.txt"));
    /// ```
    pub fn to_path_buf(&self) -> Utf8TypedPathBuf {
        match self {
            Self::Unix(path) => Utf8TypedPathBuf::Unix(path.to_path_buf()),
            Self::Windows(path) => Utf8TypedPathBuf::Windows(path.to_path_buf()),
        }
    }

    /// Returns `true` if the [`Utf8TypedPath`] is absolute, i.e., if it is independent of
    /// the current directory.
    ///
    /// * On Unix ([`UnixPath`]]), a path is absolute if it starts with the root, so
    ///   `is_absolute` and [`has_root`] are equivalent.
    ///
    /// * On Windows ([`WindowsPath`]), a path is absolute if it has a prefix and starts with the
    ///   root: `c:\windows` is absolute, while `c:temp` and `\temp` are not.
    ///
    /// [`UnixPath`]: crate::UnixPath
    /// [`WindowsPath`]: crate::WindowsPath
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPath;
    ///
    /// assert!(!Utf8TypedPath::derive("foo.txt").is_absolute());
    /// ```
    ///
    /// [`has_root`]: Utf8TypedPath::has_root
    pub fn is_absolute(&self) -> bool {
        impl_typed_fn!(self, is_absolute)
    }

    /// Returns `true` if the [`Utf8TypedPath`] is relative, i.e., not absolute.
    ///
    /// See [`is_absolute`]'s documentation for more details.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPath;
    ///
    /// assert!(Utf8TypedPath::derive("foo.txt").is_relative());
    /// ```
    ///
    /// [`is_absolute`]: Utf8TypedPath::is_absolute
    #[inline]
    pub fn is_relative(&self) -> bool {
        impl_typed_fn!(self, is_relative)
    }

    /// Returns `true` if the [`Utf8TypedPath`] has a root.
    ///
    /// * On Unix ([`Utf8UnixPath`]), a path has a root if it begins with `/`.
    ///
    /// * On Windows ([`Utf8WindowsPath`]), a path has a root if it:
    ///     * has no prefix and begins with a separator, e.g., `\windows`
    ///     * has a prefix followed by a separator, e.g., `c:\windows` but not `c:windows`
    ///     * has any non-disk prefix, e.g., `\\server\share`
    ///
    /// [`Utf8UnixPath`]: crate::Utf8UnixPath
    /// [`Utf8WindowsPath`]: crate::Utf8WindowsPath
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPath;
    ///
    /// assert!(Utf8TypedPath::derive("/etc/passwd").has_root());
    /// ```
    #[inline]
    pub fn has_root(&self) -> bool {
        impl_typed_fn!(self, has_root)
    }

    /// Returns the [`Utf8TypedPath`] without its final component, if there is one.
    ///
    /// Returns [`None`] if the path terminates in a root or prefix.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPath;
    ///
    /// let path = Utf8TypedPath::derive("/foo/bar");
    /// let parent = path.parent().unwrap();
    /// assert_eq!(parent, Utf8TypedPath::derive("/foo"));
    ///
    /// let grand_parent = parent.parent().unwrap();
    /// assert_eq!(grand_parent, Utf8TypedPath::derive("/"));
    /// assert_eq!(grand_parent.parent(), None);
    /// ```
    pub fn parent(&self) -> Option<Self> {
        match self {
            Self::Unix(path) => path.parent().map(Self::Unix),
            Self::Windows(path) => path.parent().map(Self::Windows),
        }
    }

    /// Produces an iterator over [`Utf8TypedPath`] and its ancestors.
    ///
    /// The iterator will yield the [`Utf8TypedPath`] that is returned if the [`parent`] method is used
    /// zero or more times. That means, the iterator will yield `&self`, `&self.parent().unwrap()`,
    /// `&self.parent().unwrap().parent().unwrap()` and so on. If the [`parent`] method returns
    /// [`None`], the iterator will do likewise. The iterator will always yield at least one value,
    /// namely `&self`.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPath;
    ///
    /// let mut ancestors = Utf8TypedPath::derive("/foo/bar").ancestors();
    /// assert_eq!(ancestors.next(), Some(Utf8TypedPath::derive("/foo/bar")));
    /// assert_eq!(ancestors.next(), Some(Utf8TypedPath::derive("/foo")));
    /// assert_eq!(ancestors.next(), Some(Utf8TypedPath::derive("/")));
    /// assert_eq!(ancestors.next(), None);
    ///
    /// let mut ancestors = Utf8TypedPath::derive("../foo/bar").ancestors();
    /// assert_eq!(ancestors.next(), Some(Utf8TypedPath::derive("../foo/bar")));
    /// assert_eq!(ancestors.next(), Some(Utf8TypedPath::derive("../foo")));
    /// assert_eq!(ancestors.next(), Some(Utf8TypedPath::derive("..")));
    /// assert_eq!(ancestors.next(), Some(Utf8TypedPath::derive("")));
    /// assert_eq!(ancestors.next(), None);
    /// ```
    ///
    /// [`parent`]: Utf8TypedPath::parent
    #[inline]
    pub fn ancestors(&self) -> Utf8TypedAncestors<'a> {
        match self {
            Self::Unix(p) => Utf8TypedAncestors::Unix(p.ancestors()),
            Self::Windows(p) => Utf8TypedAncestors::Windows(p.ancestors()),
        }
    }

    /// Returns the final component of the [`Utf8TypedPath`], if there is one.
    ///
    /// If the path is a normal file, this is the file name. If it's the path of a directory, this
    /// is the directory name.
    ///
    /// Returns [`None`] if the path terminates in `..`.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPath;
    ///
    /// assert_eq!(Some("bin"), Utf8TypedPath::derive("/usr/bin/").file_name());
    /// assert_eq!(Some("foo.txt"), Utf8TypedPath::derive("tmp/foo.txt").file_name());
    /// assert_eq!(Some("foo.txt"), Utf8TypedPath::derive("foo.txt/.").file_name());
    /// assert_eq!(Some("foo.txt"), Utf8TypedPath::derive("foo.txt/.//").file_name());
    /// assert_eq!(None, Utf8TypedPath::derive("foo.txt/..").file_name());
    /// assert_eq!(None, Utf8TypedPath::derive("/").file_name());
    /// ```
    pub fn file_name(&self) -> Option<&str> {
        impl_typed_fn!(self, file_name)
    }

    /// Returns a path that, when joined onto `base`, yields `self`.
    ///
    /// # Difference from Path
    ///
    /// Unlike [`Path::strip_prefix`], this implementation only supports types that implement
    /// `AsRef<str>` instead of `AsRef<Path>`.
    ///
    /// [`Path::strip_prefix`]: crate::Path::strip_prefix
    ///
    /// # Errors
    ///
    /// If `base` is not a prefix of `self` (i.e., [`starts_with`]
    /// returns `false`), returns [`Err`].
    ///
    /// [`starts_with`]: Utf8TypedPath::starts_with
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8TypedPath, Utf8TypedPathBuf};
    ///
    /// let path = Utf8TypedPath::derive("/test/haha/foo.txt");
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
    /// Unlike [`Path::starts_with`], this implementation only supports types that implement
    /// `AsRef<str>` instead of `AsRef<Path>`.
    ///
    /// [`Path::starts_with`]: crate::Path::starts_with
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPath;
    ///
    /// let path = Utf8TypedPath::derive("/etc/passwd");
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
    /// assert!(!Utf8TypedPath::derive("/etc/foo.rs").starts_with("/etc/foo"));
    /// ```
    pub fn starts_with(&self, base: impl AsRef<str>) -> bool {
        match self {
            Self::Unix(p) => p.starts_with(Utf8UnixPath::new(&base)),
            Self::Windows(p) => p.starts_with(Utf8WindowsPath::new(&base)),
        }
    }

    /// Determines whether `child` is a suffix of `self`.
    ///
    /// Only considers whole path components to match.
    ///
    /// # Difference from Path
    ///
    /// Unlike [`Path::ends_with`], this implementation only supports types that implement
    /// `AsRef<str>` instead of `AsRef<Path>`.
    ///
    /// [`Path::ends_with`]: crate::Path::ends_with
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPath;
    ///
    /// let path = Utf8TypedPath::derive("/etc/resolv.conf");
    ///
    /// assert!(path.ends_with("resolv.conf"));
    /// assert!(path.ends_with("etc/resolv.conf"));
    /// assert!(path.ends_with("/etc/resolv.conf"));
    ///
    /// assert!(!path.ends_with("/resolv.conf"));
    /// assert!(!path.ends_with("conf")); // use .extension() instead
    /// ```
    pub fn ends_with(&self, child: impl AsRef<str>) -> bool {
        match self {
            Self::Unix(p) => p.ends_with(Utf8UnixPath::new(&child)),
            Self::Windows(p) => p.ends_with(Utf8WindowsPath::new(&child)),
        }
    }

    /// Extracts the stem (non-extension) portion of [`self.file_name`].
    ///
    /// [`self.file_name`]: Utf8TypedPath::file_name
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
    /// use typed_path::Utf8TypedPath;
    ///
    /// assert_eq!("foo", Utf8TypedPath::derive("foo.rs").file_stem().unwrap());
    /// assert_eq!("foo.tar", Utf8TypedPath::derive("foo.tar.gz").file_stem().unwrap());
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
    /// [`self.file_name`]: Utf8TypedPath::file_name
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPath;
    ///
    /// assert_eq!("rs", Utf8TypedPath::derive("foo.rs").extension().unwrap());
    /// assert_eq!("gz", Utf8TypedPath::derive("foo.tar.gz").extension().unwrap());
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
    /// use typed_path::{Utf8TypedPath, Utf8TypedPathBuf};
    ///
    /// assert_eq!(
    ///     Utf8TypedPath::derive("foo/bar//baz/./asdf/quux/..").normalize(),
    ///     Utf8TypedPathBuf::from("foo/bar/baz/asdf"),
    /// );
    /// ```
    ///
    /// When starting with a root directory, any `..` segment whose parent is the root directory
    /// will be filtered out:
    ///
    /// ```
    /// use typed_path::{Utf8TypedPath, Utf8TypedPathBuf};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert_eq!(
    ///     Utf8TypedPath::derive("/../foo").normalize(),
    ///     Utf8TypedPathBuf::from("/foo"),
    /// );
    /// ```
    ///
    /// If any `..` is left unresolved as the path is relative and no parent is found, it is
    /// discarded:
    ///
    /// ```
    /// use typed_path::{Utf8TypedPath, Utf8TypedPathBuf};
    ///
    /// assert_eq!(
    ///     Utf8TypedPath::derive("../foo/..").normalize(),
    ///     Utf8TypedPathBuf::from(""),
    /// );
    ///
    /// // Windows prefixes also count this way, but the prefix remains
    /// assert_eq!(
    ///     Utf8TypedPath::derive(r"C:..\foo\..").normalize(),
    ///     Utf8TypedPathBuf::from(r"C:"),
    /// );
    /// ```
    pub fn normalize(&self) -> Utf8TypedPathBuf {
        match self {
            Self::Unix(path) => Utf8TypedPathBuf::Unix(path.normalize()),
            Self::Windows(path) => Utf8TypedPathBuf::Windows(path.normalize()),
        }
    }

    /// Converts a path to an absolute form by [`normalizing`] the path, returning a
    /// [`Utf8TypedPathBuf`].
    ///
    /// In the case that the path is relative, the current working directory is prepended prior to
    /// normalizing.
    ///
    /// [`normalizing`]: Utf8TypedPath::normalize
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{utils, Utf8TypedPath};
    ///
    /// // With an absolute path, it is just normalized
    /// let path = Utf8TypedPath::derive("/a/b/../c/./d");
    /// assert_eq!(path.absolutize().unwrap(), Utf8TypedPath::derive("/a/c/d"));
    ///
    /// // With a relative path, it is first joined with the current working directory
    /// // and then normalized
    /// let cwd = utils::current_dir().unwrap().with_unix_encoding().to_typed_path_buf();
    /// let path = cwd.join("a/b/../c/./d");
    /// assert_eq!(path.absolutize().unwrap(), cwd.join("a/c/d"));
    /// ```
    #[cfg(all(feature = "std", not(target_family = "wasm")))]
    pub fn absolutize(&self) -> std::io::Result<Utf8TypedPathBuf> {
        Ok(match self {
            Self::Unix(path) => Utf8TypedPathBuf::Unix(path.absolutize()?),
            Self::Windows(path) => Utf8TypedPathBuf::Windows(path.absolutize()?),
        })
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
    /// use typed_path::{Utf8TypedPath, Utf8TypedPathBuf};
    ///
    /// assert_eq!(
    ///     Utf8TypedPath::derive("/etc").join("passwd"),
    ///     Utf8TypedPathBuf::from("/etc/passwd"),
    /// );
    /// ```
    pub fn join(&self, path: impl AsRef<str>) -> Utf8TypedPathBuf {
        match self {
            Self::Unix(p) => Utf8TypedPathBuf::Unix(p.join(Utf8UnixPath::new(&path))),
            Self::Windows(p) => Utf8TypedPathBuf::Windows(p.join(Utf8WindowsPath::new(&path))),
        }
    }

    /// Creates an owned [`Utf8TypedPathBuf`] with `path` adjoined to `self`, checking the `path` to
    /// ensure it is safe to join. _When dealing with user-provided paths, this is the preferred
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
    /// use typed_path::{CheckedPathError, Utf8TypedPath, Utf8TypedPathBuf};
    ///
    /// assert_eq!(
    ///     Utf8TypedPath::derive("/etc").join_checked("passwd"),
    ///     Ok(Utf8TypedPathBuf::from("/etc/passwd")),
    /// );
    ///
    /// assert_eq!(
    ///     Utf8TypedPath::derive("/etc").join_checked("/sneaky/path"),
    ///     Err(CheckedPathError::UnexpectedRoot),
    /// );
    /// ```
    pub fn join_checked(
        &self,
        path: impl AsRef<str>,
    ) -> Result<Utf8TypedPathBuf, CheckedPathError> {
        Ok(match self {
            Self::Unix(p) => Utf8TypedPathBuf::Unix(p.join_checked(Utf8UnixPath::new(&path))?),
            Self::Windows(p) => {
                Utf8TypedPathBuf::Windows(p.join_checked(Utf8WindowsPath::new(&path))?)
            }
        })
    }

    /// Creates an owned [`Utf8TypedPathBuf`] like `self` but with the given file name.
    ///
    /// See [`Utf8TypedPathBuf::set_file_name`] for more details.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8TypedPath, Utf8TypedPathBuf};
    ///
    /// let path = Utf8TypedPath::derive("/tmp/foo.txt");
    /// assert_eq!(path.with_file_name("bar.txt"), Utf8TypedPathBuf::from("/tmp/bar.txt"));
    ///
    /// let path = Utf8TypedPath::derive("/tmp");
    /// assert_eq!(path.with_file_name("var"), Utf8TypedPathBuf::from("/var"));
    /// ```
    pub fn with_file_name<S: AsRef<str>>(&self, file_name: S) -> Utf8TypedPathBuf {
        match self {
            Self::Unix(path) => Utf8TypedPathBuf::Unix(path.with_file_name(file_name)),
            Self::Windows(path) => Utf8TypedPathBuf::Windows(path.with_file_name(file_name)),
        }
    }

    /// Creates an owned [`Utf8TypedPathBuf`] like `self` but with the given extension.
    ///
    /// See [`Utf8TypedPathBuf::set_extension`] for more details.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8TypedPath, Utf8TypedPathBuf};
    ///
    /// let path = Utf8TypedPath::derive("foo.rs");
    /// assert_eq!(path.with_extension("txt"), Utf8TypedPathBuf::from("foo.txt"));
    ///
    /// let path = Utf8TypedPath::derive("foo.tar.gz");
    /// assert_eq!(path.with_extension(""), Utf8TypedPathBuf::from("foo.tar"));
    /// assert_eq!(path.with_extension("xz"), Utf8TypedPathBuf::from("foo.tar.xz"));
    /// assert_eq!(path.with_extension("").with_extension("txt"), Utf8TypedPathBuf::from("foo.txt"));
    /// ```
    pub fn with_extension<S: AsRef<str>>(&self, extension: S) -> Utf8TypedPathBuf {
        match self {
            Self::Unix(path) => Utf8TypedPathBuf::Unix(path.with_extension(extension)),
            Self::Windows(path) => Utf8TypedPathBuf::Windows(path.with_extension(extension)),
        }
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
    /// use typed_path::{Utf8TypedPath, Utf8TypedComponent};
    ///
    /// let mut components = Utf8TypedPath::derive("/tmp/foo.txt").components();
    ///
    /// assert!(components.next().unwrap().is_root());
    /// assert_eq!(components.next().unwrap().as_normal_str(), Some("tmp"));
    /// assert_eq!(components.next().unwrap().as_normal_str(), Some("foo.txt"));
    /// assert_eq!(components.next(), None)
    /// ```
    ///
    ///[`Utf8TypedComponent`]: crate::Utf8TypedComponent
    pub fn components(&self) -> Utf8TypedComponents<'a> {
        match self {
            Self::Unix(p) => Utf8TypedComponents::Unix(p.components()),
            Self::Windows(p) => Utf8TypedComponents::Windows(p.components()),
        }
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
    /// use typed_path::Utf8TypedPath;
    ///
    /// let mut it = Utf8TypedPath::derive("/tmp/foo.txt").iter();
    ///
    /// assert_eq!(it.next(), Some(typed_path::constants::unix::SEPARATOR_STR));
    /// assert_eq!(it.next(), Some("tmp"));
    /// assert_eq!(it.next(), Some("foo.txt"));
    /// assert_eq!(it.next(), None)
    /// ```
    #[inline]
    pub fn iter(&self) -> Utf8TypedIter<'a> {
        match self {
            Self::Unix(p) => Utf8TypedIter::Unix(p.iter()),
            Self::Windows(p) => Utf8TypedIter::Windows(p.iter()),
        }
    }

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

    /// Converts this [`Utf8TypedPath`] into the Unix variant of [`Utf8TypedPathBuf`].
    pub fn with_unix_encoding(&self) -> Utf8TypedPathBuf {
        match self {
            Self::Windows(p) => Utf8TypedPathBuf::Unix(p.with_unix_encoding()),
            _ => self.to_path_buf(),
        }
    }

    /// Converts this [`Utf8TypedPath`] into the Unix variant of [`Utf8TypedPathBuf`], ensuring it
    /// is a valid Unix path.
    pub fn with_unix_encoding_checked(&self) -> Result<Utf8TypedPathBuf, CheckedPathError> {
        Ok(match self {
            Self::Unix(p) => Utf8TypedPathBuf::Unix(p.with_unix_encoding_checked()?),
            Self::Windows(p) => Utf8TypedPathBuf::Unix(p.with_unix_encoding_checked()?),
        })
    }

    /// Converts this [`Utf8TypedPath`] into the Windows variant of [`Utf8TypedPathBuf`].
    pub fn with_windows_encoding(&self) -> Utf8TypedPathBuf {
        match self {
            Self::Unix(p) => Utf8TypedPathBuf::Windows(p.with_windows_encoding()),
            _ => self.to_path_buf(),
        }
    }

    /// Converts this [`Utf8TypedPath`] into the Windows variant of [`Utf8TypedPathBuf`], ensuring
    /// it is a valid Windows path.
    pub fn with_windows_encoding_checked(&self) -> Result<Utf8TypedPathBuf, CheckedPathError> {
        Ok(match self {
            Self::Unix(p) => Utf8TypedPathBuf::Windows(p.with_windows_encoding_checked()?),
            Self::Windows(p) => Utf8TypedPathBuf::Windows(p.with_windows_encoding_checked()?),
        })
    }
}

impl fmt::Display for Utf8TypedPath<'_> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Self::Unix(path) => fmt::Display::fmt(path, f),
            Self::Windows(path) => fmt::Display::fmt(path, f),
        }
    }
}

impl<'a> From<&'a str> for Utf8TypedPath<'a> {
    #[inline]
    fn from(s: &'a str) -> Self {
        Utf8TypedPath::derive(s)
    }
}

impl AsRef<str> for Utf8TypedPath<'_> {
    #[inline]
    fn as_ref(&self) -> &str {
        self.as_str()
    }
}

impl TryAsRef<Utf8UnixPath> for Utf8TypedPath<'_> {
    fn try_as_ref(&self) -> Option<&Utf8UnixPath> {
        match self {
            Self::Unix(path) => Some(path),
            _ => None,
        }
    }
}

impl TryAsRef<Utf8WindowsPath> for Utf8TypedPath<'_> {
    fn try_as_ref(&self) -> Option<&Utf8WindowsPath> {
        match self {
            Self::Windows(path) => Some(path),
            _ => None,
        }
    }
}

impl PartialEq<Utf8TypedPathBuf> for Utf8TypedPath<'_> {
    fn eq(&self, path: &Utf8TypedPathBuf) -> bool {
        self.eq(&path.to_path())
    }
}

impl PartialEq<str> for Utf8TypedPath<'_> {
    fn eq(&self, path: &str) -> bool {
        self.as_str() == path
    }
}

impl PartialEq<Utf8TypedPath<'_>> for str {
    fn eq(&self, path: &Utf8TypedPath<'_>) -> bool {
        self == path.as_str()
    }
}

impl<'a> PartialEq<&'a str> for Utf8TypedPath<'_> {
    fn eq(&self, path: &&'a str) -> bool {
        self.as_str() == *path
    }
}

impl PartialEq<Utf8TypedPath<'_>> for &str {
    fn eq(&self, path: &Utf8TypedPath<'_>) -> bool {
        *self == path.as_str()
    }
}
