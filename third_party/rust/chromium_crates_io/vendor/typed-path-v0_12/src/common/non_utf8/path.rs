mod display;

use alloc::borrow::{Cow, ToOwned};
use alloc::rc::Rc;
#[cfg(target_has_atomic = "ptr")]
use alloc::sync::Arc;
use core::hash::{Hash, Hasher};
use core::marker::PhantomData;
use core::{cmp, fmt};

pub use display::Display;

use crate::common::{
    Ancestors, CheckedPathError, Component, Components, Encoding, Iter, PathBuf, StripPrefixError,
};
use crate::no_std_compat::*;

/// A slice of a path (akin to [`str`]).
///
/// This type supports a number of operations for inspecting a path, including
/// breaking the path into its components (separated by `/` on Unix and by either
/// `/` or `\` on Windows), extracting the file name, determining whether the path
/// is absolute, and so on.
///
/// This is an *unsized* type, meaning that it must always be used behind a
/// pointer like `&` or [`Box`]. For an owned version of this type,
/// see [`PathBuf`].
///
/// # Examples
///
/// ```
/// use typed_path::{Path, UnixEncoding};
///
/// // NOTE: A path cannot be created on its own without a defined encoding,
/// //       but all encodings work on all operating systems, providing the
/// //       ability to parse and operate on paths independently of the
/// //       compiled platform
/// let path = Path::<UnixEncoding>::new("./foo/bar.txt");
///
/// let parent = path.parent();
/// assert_eq!(parent, Some(Path::new("./foo")));
///
/// let file_stem = path.file_stem();
/// assert_eq!(file_stem, Some(b"bar".as_slice()));
///
/// let extension = path.extension();
/// assert_eq!(extension, Some(b"txt".as_slice()));
/// ```
///
/// In addition to explicitly using [`Encoding`]s, you can also
/// leverage aliases available from the crate to work with paths:
///
/// ```
/// use typed_path::{UnixPath, WindowsPath};
///
/// // Same as Path<UnixEncoding>
/// let path = UnixPath::new("/foo/bar.txt");
///
/// // Same as Path<WindowsEncoding>
/// let path = WindowsPath::new(r"C:\foo\bar.txt");
/// ```
///
/// To mirror the design of Rust's standard library, you can access
/// the path associated with the compiled rust platform using [`NativePath`],
/// which itself is an alias to one of the other choices:
///
/// ```
/// use typed_path::NativePath;
///
/// // On Unix, this would be UnixPath aka Path<UnixEncoding>
/// // On Windows, this would be WindowsPath aka Path<WindowsEncoding>
/// let path = NativePath::new("/foo/bar.txt");
/// ```
///
/// [`NativePath`]: crate::NativePath
#[repr(transparent)]
pub struct Path<T>
where
    T: Encoding,
{
    /// Encoding associated with path buf
    _encoding: PhantomData<T>,

    /// Path as an unparsed byte slice
    pub(crate) inner: [u8],
}

impl<T> Path<T>
where
    T: Encoding,
{
    /// Directly wraps a byte slice as a `Path` slice.
    ///
    /// This is a cost-free conversion.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// Path::<UnixEncoding>::new("foo.txt");
    /// ```
    ///
    /// You can create `Path`s from `String`s, or even other `Path`s:
    ///
    /// ```
    /// use typed_path::{Path, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let string = String::from("foo.txt");
    /// let from_string = Path::<UnixEncoding>::new(&string);
    /// let from_path = Path::new(&from_string);
    /// assert_eq!(from_string, from_path);
    /// ```
    ///
    /// There are also handy aliases to the `Path` with [`Encoding`]:
    ///
    /// ```
    /// use typed_path::UnixPath;
    ///
    /// let string = String::from("foo.txt");
    /// let from_string = UnixPath::new(&string);
    /// let from_path = UnixPath::new(&from_string);
    /// assert_eq!(from_string, from_path);
    /// ```
    #[inline]
    pub fn new<S: AsRef<[u8]> + ?Sized>(s: &S) -> &Self {
        unsafe { &*(s.as_ref() as *const [u8] as *const Self) }
    }

    /// Yields the underlying [`[u8]`] slice.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let bytes = Path::<UnixEncoding>::new("foo.txt").as_bytes();
    /// assert_eq!(bytes, b"foo.txt");
    /// ```
    pub fn as_bytes(&self) -> &[u8] {
        &self.inner
    }

    /// Yields a [`&str`] slice if the `Path` is valid unicode.
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
    /// use typed_path::{Path, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Path::<UnixEncoding>::new("foo.txt");
    /// assert_eq!(path.to_str(), Some("foo.txt"));
    /// ```
    #[inline]
    pub fn to_str(&self) -> Option<&str> {
        core::str::from_utf8(&self.inner).ok()
    }

    /// Converts a `Path` to a [`Cow<str>`].
    ///
    /// Any non-Unicode sequences are replaced with
    /// [`U+FFFD REPLACEMENT CHARACTER`][U+FFFD].
    ///
    /// [U+FFFD]: std::char::REPLACEMENT_CHARACTER
    ///
    /// # Examples
    ///
    /// Calling `to_string_lossy` on a `Path` with valid unicode:
    ///
    /// ```
    /// use typed_path::{Path, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Path::<UnixEncoding>::new("foo.txt");
    /// assert_eq!(path.to_string_lossy(), "foo.txt");
    /// ```
    ///
    /// Had `path` contained invalid unicode, the `to_string_lossy` call might
    /// have returned `"fo�.txt"`.
    #[inline]
    pub fn to_string_lossy(&self) -> Cow<'_, str> {
        String::from_utf8_lossy(&self.inner)
    }

    /// Converts a `Path` to an owned [`PathBuf`].
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, PathBuf, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path_buf = Path::<UnixEncoding>::new("foo.txt").to_path_buf();
    /// assert_eq!(path_buf, PathBuf::from("foo.txt"));
    /// ```
    pub fn to_path_buf(&self) -> PathBuf<T> {
        PathBuf {
            inner: self.inner.to_owned(),
            _encoding: PhantomData,
        }
    }

    /// Returns `true` if the `Path` is absolute, i.e., if it is independent of
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
    /// use typed_path::{Path, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert!(!Path::<UnixEncoding>::new("foo.txt").is_absolute());
    /// ```
    ///
    /// [`has_root`]: Path::has_root
    pub fn is_absolute(&self) -> bool {
        self.components().is_absolute()
    }

    /// Returns `true` if the `Path` is relative, i.e., not absolute.
    ///
    /// See [`is_absolute`]'s documentation for more details.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert!(Path::<UnixEncoding>::new("foo.txt").is_relative());
    /// ```
    ///
    /// [`is_absolute`]: Path::is_absolute
    #[inline]
    pub fn is_relative(&self) -> bool {
        !self.is_absolute()
    }

    /// Returns `true` if the path is valid, meaning that all of its components are valid.
    ///
    /// See [`Component::is_valid`]'s documentation for more details.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert!(Path::<UnixEncoding>::new("foo.txt").is_valid());
    /// assert!(!Path::<UnixEncoding>::new("foo\0.txt").is_valid());
    /// ```
    ///
    /// [`Component::is_valid`]: crate::Component::is_valid
    pub fn is_valid(&self) -> bool {
        self.components().all(|c| c.is_valid())
    }

    /// Returns `true` if the `Path` has a root.
    ///
    /// * On Unix ([`UnixPath`]), a path has a root if it begins with `/`.
    ///
    /// * On Windows ([`WindowsPath`]), a path has a root if it:
    ///     * has no prefix and begins with a separator, e.g., `\windows`
    ///     * has a prefix followed by a separator, e.g., `c:\windows` but not `c:windows`
    ///     * has any non-disk prefix, e.g., `\\server\share`
    ///
    /// [`UnixPath`]: crate::UnixPath
    /// [`WindowsPath`]: crate::WindowsPath
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert!(Path::<UnixEncoding>::new("/etc/passwd").has_root());
    /// ```
    #[inline]
    pub fn has_root(&self) -> bool {
        self.components().has_root()
    }

    /// Returns the `Path` without its final component, if there is one.
    ///
    /// Returns [`None`] if the path terminates in a root or prefix.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Path::<UnixEncoding>::new("/foo/bar");
    /// let parent = path.parent().unwrap();
    /// assert_eq!(parent, Path::new("/foo"));
    ///
    /// let grand_parent = parent.parent().unwrap();
    /// assert_eq!(grand_parent, Path::new("/"));
    /// assert_eq!(grand_parent.parent(), None);
    /// ```
    pub fn parent(&self) -> Option<&Self> {
        let mut comps = self.components();
        let comp = comps.next_back();
        comp.and_then(|p| {
            if !p.is_root() {
                Some(Self::new(comps.as_bytes()))
            } else {
                None
            }
        })
    }

    /// Produces an iterator over `Path` and its ancestors.
    ///
    /// The iterator will yield the `Path` that is returned if the [`parent`] method is used zero
    /// or more times. That means, the iterator will yield `&self`, `&self.parent().unwrap()`,
    /// `&self.parent().unwrap().parent().unwrap()` and so on. If the [`parent`] method returns
    /// [`None`], the iterator will do likewise. The iterator will always yield at least one value,
    /// namely `&self`.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let mut ancestors = Path::<UnixEncoding>::new("/foo/bar").ancestors();
    /// assert_eq!(ancestors.next(), Some(Path::new("/foo/bar")));
    /// assert_eq!(ancestors.next(), Some(Path::new("/foo")));
    /// assert_eq!(ancestors.next(), Some(Path::new("/")));
    /// assert_eq!(ancestors.next(), None);
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let mut ancestors = Path::<UnixEncoding>::new("../foo/bar").ancestors();
    /// assert_eq!(ancestors.next(), Some(Path::new("../foo/bar")));
    /// assert_eq!(ancestors.next(), Some(Path::new("../foo")));
    /// assert_eq!(ancestors.next(), Some(Path::new("..")));
    /// assert_eq!(ancestors.next(), Some(Path::new("")));
    /// assert_eq!(ancestors.next(), None);
    /// ```
    ///
    /// [`parent`]: Path::parent
    #[inline]
    pub fn ancestors(&self) -> Ancestors<'_, T> {
        Ancestors { next: Some(self) }
    }

    /// Returns the final component of the `Path`, if there is one.
    ///
    /// If the path is a normal file, this is the file name. If it's the path of a directory, this
    /// is the directory name.
    ///
    /// Returns [`None`] if the path terminates in `..`.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert_eq!(Some(b"bin".as_slice()), Path::<UnixEncoding>::new("/usr/bin/").file_name());
    /// assert_eq!(Some(b"foo.txt".as_slice()), Path::<UnixEncoding>::new("tmp/foo.txt").file_name());
    /// assert_eq!(Some(b"foo.txt".as_slice()), Path::<UnixEncoding>::new("foo.txt/.").file_name());
    /// assert_eq!(Some(b"foo.txt".as_slice()), Path::<UnixEncoding>::new("foo.txt/.//").file_name());
    /// assert_eq!(None, Path::<UnixEncoding>::new("foo.txt/..").file_name());
    /// assert_eq!(None, Path::<UnixEncoding>::new("/").file_name());
    /// ```
    pub fn file_name(&self) -> Option<&[u8]> {
        match self.components().next_back() {
            Some(p) => {
                if p.is_normal() {
                    Some(p.as_bytes())
                } else {
                    None
                }
            }
            None => None,
        }
    }

    /// Returns a path that, when joined onto `base`, yields `self`.
    ///
    /// # Errors
    ///
    /// If `base` is not a prefix of `self` (i.e., [`starts_with`]
    /// returns `false`), returns [`Err`].
    ///
    /// [`starts_with`]: Path::starts_with
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, PathBuf, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Path::<UnixEncoding>::new("/test/haha/foo.txt");
    ///
    /// assert_eq!(path.strip_prefix("/"), Ok(Path::new("test/haha/foo.txt")));
    /// assert_eq!(path.strip_prefix("/test"), Ok(Path::new("haha/foo.txt")));
    /// assert_eq!(path.strip_prefix("/test/"), Ok(Path::new("haha/foo.txt")));
    /// assert_eq!(path.strip_prefix("/test/haha/foo.txt"), Ok(Path::new("")));
    /// assert_eq!(path.strip_prefix("/test/haha/foo.txt/"), Ok(Path::new("")));
    ///
    /// assert!(path.strip_prefix("test").is_err());
    /// assert!(path.strip_prefix("/haha").is_err());
    ///
    /// let prefix = PathBuf::<UnixEncoding>::from("/test/");
    /// assert_eq!(path.strip_prefix(prefix), Ok(Path::new("haha/foo.txt")));
    /// ```
    pub fn strip_prefix<P>(&self, base: P) -> Result<&Path<T>, StripPrefixError>
    where
        P: AsRef<Path<T>>,
    {
        self._strip_prefix(base.as_ref())
    }

    fn _strip_prefix(&self, base: &Path<T>) -> Result<&Path<T>, StripPrefixError> {
        match helpers::iter_after(self.components(), base.components()) {
            Some(c) => Ok(Path::new(c.as_bytes())),
            None => Err(StripPrefixError(())),
        }
    }

    /// Determines whether `base` is a prefix of `self`.
    ///
    /// Only considers whole path components to match.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Path::<UnixEncoding>::new("/etc/passwd");
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
    /// assert!(!Path::<UnixEncoding>::new("/etc/foo.rs").starts_with("/etc/foo"));
    /// ```
    pub fn starts_with<P>(&self, base: P) -> bool
    where
        P: AsRef<Path<T>>,
    {
        self._starts_with(base.as_ref())
    }

    fn _starts_with(&self, base: &Path<T>) -> bool {
        helpers::iter_after(self.components(), base.components()).is_some()
    }

    /// Determines whether `child` is a suffix of `self`.
    ///
    /// Only considers whole path components to match.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Path::<UnixEncoding>::new("/etc/resolv.conf");
    ///
    /// assert!(path.ends_with("resolv.conf"));
    /// assert!(path.ends_with("etc/resolv.conf"));
    /// assert!(path.ends_with("/etc/resolv.conf"));
    ///
    /// assert!(!path.ends_with("/resolv.conf"));
    /// assert!(!path.ends_with("conf")); // use .extension() instead
    /// ```
    pub fn ends_with<P>(&self, child: P) -> bool
    where
        P: AsRef<Path<T>>,
    {
        self._ends_with(child.as_ref())
    }

    fn _ends_with(&self, child: &Path<T>) -> bool {
        helpers::iter_after(self.components().rev(), child.components().rev()).is_some()
    }

    /// Extracts the stem (non-extension) portion of [`self.file_name`].
    ///
    /// [`self.file_name`]: Path::file_name
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
    /// use typed_path::{Path, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert_eq!(b"foo", Path::<UnixEncoding>::new("foo.rs").file_stem().unwrap());
    /// assert_eq!(b"foo.tar", Path::<UnixEncoding>::new("foo.tar.gz").file_stem().unwrap());
    /// ```
    ///
    pub fn file_stem(&self) -> Option<&[u8]> {
        self.file_name()
            .map(helpers::rsplit_file_at_dot)
            .and_then(|(before, after)| before.or(after))
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
    /// [`self.file_name`]: Path::file_name
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert_eq!(b"rs", Path::<UnixEncoding>::new("foo.rs").extension().unwrap());
    /// assert_eq!(b"gz", Path::<UnixEncoding>::new("foo.tar.gz").extension().unwrap());
    /// ```
    pub fn extension(&self) -> Option<&[u8]> {
        self.file_name()
            .map(helpers::rsplit_file_at_dot)
            .and_then(|(before, after)| before.and(after))
    }

    /// Returns an owned [`PathBuf`] by resolving `..` and `.` segments.
    ///
    /// When multiple, sequential path segment separation characters are found (e.g. `/` for Unix
    /// and either `\` or `/` on Windows), they are replaced by a single instance of the
    /// platform-specific path segment separator (`/` on Unix and `\` on Windows).
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, PathBuf, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert_eq!(
    ///     Path::<UnixEncoding>::new("foo/bar//baz/./asdf/quux/..").normalize(),
    ///     PathBuf::from("foo/bar/baz/asdf"),
    /// );
    /// ```
    ///
    /// When starting with a root directory, any `..` segment whose parent is the root directory
    /// will be filtered out:
    ///
    /// ```
    /// use typed_path::{Path, PathBuf, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert_eq!(
    ///     Path::<UnixEncoding>::new("/../foo").normalize(),
    ///     PathBuf::from("/foo"),
    /// );
    /// ```
    ///
    /// If any `..` is left unresolved as the path is relative and no parent is found, it is
    /// discarded:
    ///
    /// ```
    /// use typed_path::{Path, PathBuf, UnixEncoding, WindowsEncoding};
    ///
    /// assert_eq!(
    ///     Path::<UnixEncoding>::new("../foo/..").normalize(),
    ///     PathBuf::from(""),
    /// );
    ///
    /// // Windows prefixes also count this way, but the prefix remains
    /// assert_eq!(
    ///     Path::<WindowsEncoding>::new(r"C:..\foo\..").normalize(),
    ///     PathBuf::from(r"C:"),
    /// );
    /// ```
    pub fn normalize(&self) -> PathBuf<T> {
        let mut components = Vec::new();
        for component in self.components() {
            if !component.is_current() && !component.is_parent() {
                components.push(component);
            } else if component.is_parent() {
                if let Some(last) = components.last() {
                    if last.is_normal() {
                        components.pop();
                    }
                }
            }
        }

        let mut path = PathBuf::<T>::new();

        for component in components {
            path.push(component.as_bytes());
        }

        path
    }

    /// Converts a path to an absolute form by [`normalizing`] the path, returning a
    /// [`PathBuf`].
    ///
    /// In the case that the path is relative, the current working directory is prepended prior to
    /// normalizing.
    ///
    /// [`normalizing`]: Path::normalize
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{utils, Path, UnixEncoding};
    ///
    /// // With an absolute path, it is just normalized
    /// let path = Path::<UnixEncoding>::new("/a/b/../c/./d");
    /// assert_eq!(path.absolutize().unwrap(), Path::new("/a/c/d"));
    ///
    /// // With a relative path, it is first joined with the current working directory
    /// // and then normalized
    /// let cwd = utils::current_dir().unwrap().with_encoding::<UnixEncoding>();
    /// let path = cwd.join(Path::new("a/b/../c/./d"));
    /// assert_eq!(path.absolutize().unwrap(), cwd.join(Path::new("a/c/d")));
    /// ```
    #[cfg(all(feature = "std", not(target_family = "wasm")))]
    pub fn absolutize(&self) -> std::io::Result<PathBuf<T>> {
        if self.is_absolute() {
            Ok(self.normalize())
        } else {
            // Get the cwd as a platform path and convert to this path's encoding
            let cwd = crate::utils::current_dir()?.with_encoding();

            Ok(cwd.join(self).normalize())
        }
    }

    /// Creates an owned [`PathBuf`] with `path` adjoined to `self`.
    ///
    /// See [`PathBuf::push`] for more details on what it means to adjoin a path.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, PathBuf, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert_eq!(
    ///     Path::<UnixEncoding>::new("/etc").join("passwd"),
    ///     PathBuf::from("/etc/passwd"),
    /// );
    /// ```
    pub fn join<P: AsRef<Path<T>>>(&self, path: P) -> PathBuf<T> {
        self._join(path.as_ref())
    }

    fn _join(&self, path: &Path<T>) -> PathBuf<T> {
        let mut buf = self.to_path_buf();
        buf.push(path);
        buf
    }

    /// Creates an owned [`PathBuf`] with `path` adjoined to `self`, checking the `path` to ensure
    /// it is safe to join. _When dealing with user-provided paths, this is the preferred method._
    ///
    /// See [`PathBuf::push_checked`] for more details on what it means to adjoin a path safely.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{CheckedPathError, Path, PathBuf, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Path::<UnixEncoding>::new("/etc");
    ///
    /// // A valid path can be joined onto the existing one
    /// assert_eq!(path.join_checked("passwd"), Ok(PathBuf::from("/etc/passwd")));
    ///
    /// // An invalid path will result in an error
    /// assert_eq!(path.join_checked("/sneaky/replacement"), Err(CheckedPathError::UnexpectedRoot));
    /// ```
    pub fn join_checked<P: AsRef<Path<T>>>(&self, path: P) -> Result<PathBuf<T>, CheckedPathError> {
        self._join_checked(path.as_ref())
    }

    fn _join_checked(&self, path: &Path<T>) -> Result<PathBuf<T>, CheckedPathError> {
        let mut buf = self.to_path_buf();
        buf.push_checked(path)?;
        Ok(buf)
    }

    /// Creates an owned [`PathBuf`] like `self` but with the given file name.
    ///
    /// See [`PathBuf::set_file_name`] for more details.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, PathBuf, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Path::<UnixEncoding>::new("/tmp/foo.txt");
    /// assert_eq!(path.with_file_name("bar.txt"), PathBuf::from("/tmp/bar.txt"));
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Path::<UnixEncoding>::new("/tmp");
    /// assert_eq!(path.with_file_name("var"), PathBuf::from("/var"));
    /// ```
    pub fn with_file_name<S: AsRef<[u8]>>(&self, file_name: S) -> PathBuf<T> {
        self._with_file_name(file_name.as_ref())
    }

    fn _with_file_name(&self, file_name: &[u8]) -> PathBuf<T> {
        let mut buf = self.to_path_buf();
        buf.set_file_name(file_name);
        buf
    }

    /// Creates an owned [`PathBuf`] like `self` but with the given extension.
    ///
    /// See [`PathBuf::set_extension`] for more details.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, PathBuf, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Path::<UnixEncoding>::new("foo.rs");
    /// assert_eq!(path.with_extension("txt"), PathBuf::from("foo.txt"));
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Path::<UnixEncoding>::new("foo.tar.gz");
    /// assert_eq!(path.with_extension(""), PathBuf::from("foo.tar"));
    /// assert_eq!(path.with_extension("xz"), PathBuf::from("foo.tar.xz"));
    /// assert_eq!(path.with_extension("").with_extension("txt"), PathBuf::from("foo.txt"));
    /// ```
    pub fn with_extension<S: AsRef<[u8]>>(&self, extension: S) -> PathBuf<T> {
        self._with_extension(extension.as_ref())
    }

    fn _with_extension(&self, extension: &[u8]) -> PathBuf<T> {
        let mut buf = self.to_path_buf();
        buf.set_extension(extension);
        buf
    }

    /// Produces an iterator over the [`Component`]s of the path.
    ///
    /// When parsing the path, there is a small amount of normalization:
    ///
    /// * Repeated separators are ignored, so `a/b` and `a//b` both have
    ///   `a` and `b` as components.
    ///
    /// * Occurrences of `.` are normalized away, except if they are at the
    ///   beginning of the path. For example, `a/./b`, `a/b/`, `a/b/.` and
    ///   `a/b` all have `a` and `b` as components, but `./a/b` starts with
    ///   an additional [`CurDir`] component.
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
    /// use typed_path::{Path, UnixComponent, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let mut components = Path::<UnixEncoding>::new("/tmp/foo.txt").components();
    ///
    /// assert_eq!(components.next(), Some(UnixComponent::RootDir));
    /// assert_eq!(components.next(), Some(UnixComponent::Normal(b"tmp")));
    /// assert_eq!(components.next(), Some(UnixComponent::Normal(b"foo.txt")));
    /// assert_eq!(components.next(), None)
    /// ```
    ///
    /// [`CurDir`]: crate::unix::UnixComponent::CurDir
    pub fn components(&self) -> <T as Encoding>::Components<'_> {
        T::components(&self.inner)
    }

    /// Produces an iterator over the path's components viewed as [`[u8]`] slices.
    ///
    /// For more information about the particulars of how the path is separated
    /// into components, see [`components`].
    ///
    /// [`components`]: Path::components
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let mut it = Path::<UnixEncoding>::new("/tmp/foo.txt").iter();
    ///
    /// assert_eq!(it.next(), Some(typed_path::constants::unix::SEPARATOR_STR.as_bytes()));
    /// assert_eq!(it.next(), Some(b"tmp".as_slice()));
    /// assert_eq!(it.next(), Some(b"foo.txt".as_slice()));
    /// assert_eq!(it.next(), None)
    /// ```
    #[inline]
    pub fn iter(&self) -> Iter<'_, T> {
        Iter::new(self.components())
    }

    /// Returns an object that implements [`Display`] for safely printing paths
    /// that may contain non-Unicode data. This may perform lossy conversion,
    /// depending on the platform.  If you would like an implementation which
    /// escapes the path please use [`Debug`] instead.
    ///
    /// [`Debug`]: fmt::Debug
    /// [`Display`]: fmt::Display
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Path::<UnixEncoding>::new("/tmp/foo.rs");
    ///
    /// println!("{}", path.display());
    /// ```
    #[inline]
    pub fn display(&self) -> Display<'_, T> {
        Display { path: self }
    }

    /// Creates an owned [`PathBuf`] like `self` but with a different encoding.
    ///
    /// # Note
    ///
    /// As part of the process of converting between encodings, the path will need to be rebuilt.
    /// This involves [`pushing`] each component, which may result in differences in the resulting
    /// path such as resolving `.` and `..` early or other unexpected side effects.
    ///
    /// [`pushing`]: PathBuf::push
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, UnixEncoding, WindowsEncoding};
    ///
    /// // Convert from Unix to Windows
    /// let unix_path = Path::<UnixEncoding>::new("/tmp/foo.txt");
    /// let windows_path = unix_path.with_encoding::<WindowsEncoding>();
    /// assert_eq!(windows_path, Path::<WindowsEncoding>::new(r"\tmp\foo.txt"));
    ///
    /// // Converting from Windows to Unix will drop any prefix
    /// let windows_path = Path::<WindowsEncoding>::new(r"C:\tmp\foo.txt");
    /// let unix_path = windows_path.with_encoding::<UnixEncoding>();
    /// assert_eq!(unix_path, Path::<UnixEncoding>::new(r"/tmp/foo.txt"));
    ///
    /// // Converting to itself should retain everything
    /// let path = Path::<WindowsEncoding>::new(r"C:\tmp\foo.txt");
    /// assert_eq!(
    ///     path.with_encoding::<WindowsEncoding>(),
    ///     Path::<WindowsEncoding>::new(r"C:\tmp\foo.txt"),
    /// );
    /// ```
    pub fn with_encoding<U>(&self) -> PathBuf<U>
    where
        U: Encoding,
    {
        // If we're the same, just return the path buf, which
        // we do with a fancy trick to convert it
        if T::label() == U::label() {
            Path::new(self.as_bytes()).to_path_buf()
        } else {
            // Otherwise, we have to rebuild the path from the components
            let mut path = PathBuf::new();

            // For root, current, and parent we specially handle to convert
            // to the appropriate type, otherwise we pass along as-is
            for component in self.components() {
                if component.is_root() {
                    path.push(<
                        <<U as Encoding>::Components<'_> as Components>::Component
                        as Component
                    >::root().as_bytes());
                } else if component.is_current() {
                    path.push(<
                        <<U as Encoding>::Components<'_> as Components>::Component
                        as Component
                    >::current().as_bytes());
                } else if component.is_parent() {
                    path.push(<
                        <<U as Encoding>::Components<'_> as Components>::Component
                        as Component
                    >::parent().as_bytes());
                } else {
                    path.push(component.as_bytes());
                }
            }

            path
        }
    }

    /// Like [`with_encoding`], creates an owned [`PathBuf`] like `self` but with a different
    /// encoding. Additionally, checks to ensure that the produced path will be valid.
    ///
    /// # Note
    ///
    /// As part of the process of converting between encodings, the path will need to be rebuilt.
    /// This involves [`pushing and checking`] each component, which may result in differences in
    /// the resulting path such as resolving `.` and `..` early or other unexpected side effects.
    ///
    /// [`pushing and checking`]: PathBuf::push_checked
    /// [`with_encoding`]: Path::with_encoding
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{CheckedPathError, Path, UnixEncoding, WindowsEncoding};
    ///
    /// // Convert from Unix to Windows
    /// let unix_path = Path::<UnixEncoding>::new("/tmp/foo.txt");
    /// let windows_path = unix_path.with_encoding_checked::<WindowsEncoding>().unwrap();
    /// assert_eq!(windows_path, Path::<WindowsEncoding>::new(r"\tmp\foo.txt"));
    ///
    /// // Converting from Windows to Unix will drop any prefix
    /// let windows_path = Path::<WindowsEncoding>::new(r"C:\tmp\foo.txt");
    /// let unix_path = windows_path.with_encoding_checked::<UnixEncoding>().unwrap();
    /// assert_eq!(unix_path, Path::<UnixEncoding>::new(r"/tmp/foo.txt"));
    ///
    /// // Converting from Unix to Windows with invalid filename characters like `:` should fail
    /// let unix_path = Path::<UnixEncoding>::new("/|invalid|/foo.txt");
    /// assert_eq!(
    ///     unix_path.with_encoding_checked::<WindowsEncoding>(),
    ///     Err(CheckedPathError::InvalidFilename),
    /// );
    ///
    /// // Converting from Unix to Windows with unexpected prefix embedded in path should fail
    /// let unix_path = Path::<UnixEncoding>::new("/path/c:/foo.txt");
    /// assert_eq!(
    ///     unix_path.with_encoding_checked::<WindowsEncoding>(),
    ///     Err(CheckedPathError::UnexpectedPrefix),
    /// );
    /// ```
    pub fn with_encoding_checked<U>(&self) -> Result<PathBuf<U>, CheckedPathError>
    where
        U: Encoding,
    {
        let mut path = PathBuf::new();

        // For root, current, and parent we specially handle to convert to the appropriate type,
        // otherwise we attempt to push using the checked variant, which will ensure that the
        // destination encoding is respected
        for component in self.components() {
            if component.is_root() {
                path.push(
                    <<<U as Encoding>::Components<'_> as Components>::Component as Component>::root()
                        .as_bytes(),
                );
            } else if component.is_current() {
                path.push(
                    <<<U as Encoding>::Components<'_> as Components>::Component as Component>::current(
                    )
                    .as_bytes(),
                );
            } else if component.is_parent() {
                path.push(
                    <<<U as Encoding>::Components<'_> as Components>::Component as Component>::parent()
                        .as_bytes(),
                );
            } else {
                path.push_checked(component.as_bytes())?;
            }
        }

        Ok(path)
    }

    /// Converts a [`Box<Path>`](Box) into a
    /// [`PathBuf`] without copying or allocating.
    pub fn into_path_buf(self: Box<Path<T>>) -> PathBuf<T> {
        let rw = Box::into_raw(self) as *mut [u8];
        let inner = unsafe { Box::from_raw(rw) };
        PathBuf {
            _encoding: PhantomData,
            inner: inner.into_vec(),
        }
    }
}

impl<T> Clone for Box<Path<T>>
where
    T: Encoding,
{
    fn clone(&self) -> Self {
        self.to_path_buf().into_boxed_path()
    }
}

impl<T> AsRef<[u8]> for Path<T>
where
    T: Encoding,
{
    #[inline]
    fn as_ref(&self) -> &[u8] {
        &self.inner
    }
}

impl<T> AsRef<Path<T>> for Path<T>
where
    T: Encoding,
{
    #[inline]
    fn as_ref(&self) -> &Path<T> {
        self
    }
}

impl<T> AsRef<Path<T>> for [u8]
where
    T: Encoding,
{
    #[inline]
    fn as_ref(&self) -> &Path<T> {
        Path::new(self)
    }
}

impl<T> AsRef<Path<T>> for Vec<u8>
where
    T: Encoding,
{
    #[inline]
    fn as_ref(&self) -> &Path<T> {
        Path::new(self)
    }
}

impl<T> AsRef<Path<T>> for str
where
    T: Encoding,
{
    #[inline]
    fn as_ref(&self) -> &Path<T> {
        Path::new(self)
    }
}

impl<T> AsRef<Path<T>> for String
where
    T: Encoding,
{
    #[inline]
    fn as_ref(&self) -> &Path<T> {
        Path::new(self)
    }
}

impl<T> fmt::Debug for Path<T>
where
    T: Encoding,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Path")
            .field("_encoding", &T::label())
            .field("inner", &&self.inner)
            .finish()
    }
}

impl<T> fmt::Display for Path<T>
where
    T: Encoding,
{
    /// Format path by converting bytes to a [`String`]. This may perform lossy conversion,
    /// depending on the platform.  If you would like an implementation which escapes the path
    /// please use [`Debug`] instead.
    ///
    /// [`Debug`]: fmt::Debug
    /// [`Display`]: fmt::Display
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Path::<UnixEncoding>::new("/tmp/foo.rs");
    ///
    /// assert_eq!(path.to_string(), "/tmp/foo.rs");
    /// ```
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.display())
    }
}

impl<T> cmp::PartialEq for Path<T>
where
    T: Encoding,
{
    #[inline]
    fn eq(&self, other: &Path<T>) -> bool {
        self.components() == other.components()
    }
}

impl<T> cmp::Eq for Path<T> where T: Encoding {}

impl<T> Hash for Path<T>
where
    T: Encoding,
{
    fn hash<H: Hasher>(&self, h: &mut H) {
        T::hash(self.as_bytes(), h)
    }
}

impl<T> cmp::PartialOrd for Path<T>
where
    T: Encoding,
{
    #[inline]
    fn partial_cmp(&self, other: &Path<T>) -> Option<cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl<T> cmp::Ord for Path<T>
where
    T: Encoding,
{
    #[inline]
    fn cmp(&self, other: &Path<T>) -> cmp::Ordering {
        self.components().cmp(other.components())
    }
}

impl<T> From<&Path<T>> for Box<Path<T>>
where
    T: Encoding,
{
    /// Creates a boxed [`Path`] from a reference.
    ///
    /// This will allocate and clone `path` to it.
    fn from(path: &Path<T>) -> Self {
        let boxed: Box<[u8]> = path.inner.into();
        let rw = Box::into_raw(boxed) as *mut Path<T>;
        unsafe { Box::from_raw(rw) }
    }
}

impl<T> From<Cow<'_, Path<T>>> for Box<Path<T>>
where
    T: Encoding,
{
    /// Creates a boxed [`Path`] from a clone-on-write pointer.
    ///
    /// Converting from a `Cow::Owned` does not clone or allocate.
    #[inline]
    fn from(cow: Cow<'_, Path<T>>) -> Box<Path<T>> {
        match cow {
            Cow::Borrowed(path) => Box::from(path),
            Cow::Owned(path) => Box::from(path),
        }
    }
}

impl<T> From<PathBuf<T>> for Box<Path<T>>
where
    T: Encoding,
{
    /// Converts a [`PathBuf`] into a <code>[Box]&lt;[Path]&gt;</code>.
    ///
    /// This conversion currently should not allocate memory,
    /// but this behavior is not guaranteed on all platforms or in all future versions.
    #[inline]
    fn from(p: PathBuf<T>) -> Box<Path<T>> {
        p.into_boxed_path()
    }
}

impl<'a, T> From<&'a Path<T>> for Cow<'a, Path<T>>
where
    T: Encoding,
{
    /// Creates a clone-on-write pointer from a reference to
    /// [`Path`].
    ///
    /// This conversion does not clone or allocate.
    #[inline]
    fn from(s: &'a Path<T>) -> Self {
        Cow::Borrowed(s)
    }
}

impl<T> From<PathBuf<T>> for Cow<'_, Path<T>>
where
    T: Encoding,
{
    /// Creates a clone-on-write pointer from an owned
    /// instance of [`PathBuf`].
    ///
    /// This conversion does not clone or allocate.
    #[inline]
    fn from(s: PathBuf<T>) -> Self {
        Cow::Owned(s)
    }
}

impl<'a, T> From<&'a PathBuf<T>> for Cow<'a, Path<T>>
where
    T: Encoding,
{
    /// Creates a clone-on-write pointer from a reference to
    /// [`PathBuf`].
    ///
    /// This conversion does not clone or allocate.
    #[inline]
    fn from(p: &'a PathBuf<T>) -> Self {
        Cow::Borrowed(p.as_path())
    }
}

#[cfg(target_has_atomic = "ptr")]
impl<T> From<PathBuf<T>> for Arc<Path<T>>
where
    T: Encoding,
{
    /// Converts a [`PathBuf`] into an <code>[Arc]<[Path]></code> by moving the [`PathBuf`] data
    /// into a new [`Arc`] buffer.
    #[inline]
    fn from(path_buf: PathBuf<T>) -> Self {
        let arc: Arc<[u8]> = Arc::from(path_buf.into_vec());
        unsafe { Arc::from_raw(Arc::into_raw(arc) as *const Path<T>) }
    }
}

#[cfg(target_has_atomic = "ptr")]
impl<T> From<&Path<T>> for Arc<Path<T>>
where
    T: Encoding,
{
    /// Converts a [`Path`] into an [`Arc`] by copying the [`Path`] data into a new [`Arc`] buffer.
    #[inline]
    fn from(path: &Path<T>) -> Self {
        let arc: Arc<[u8]> = Arc::from(path.as_bytes().to_vec());
        unsafe { Arc::from_raw(Arc::into_raw(arc) as *const Path<T>) }
    }
}

impl<T> From<PathBuf<T>> for Rc<Path<T>>
where
    T: Encoding,
{
    /// Converts a [`PathBuf`] into an <code>[Rc]<[Path]></code> by moving the [`PathBuf`] data into
    /// a new [`Rc`] buffer.
    #[inline]
    fn from(path_buf: PathBuf<T>) -> Self {
        let rc: Rc<[u8]> = Rc::from(path_buf.into_vec());
        unsafe { Rc::from_raw(Rc::into_raw(rc) as *const Path<T>) }
    }
}

impl<T> From<&Path<T>> for Rc<Path<T>>
where
    T: Encoding,
{
    /// Converts a [`Path`] into an [`Rc`] by copying the [`Path`] data into a new [`Rc`] buffer.
    #[inline]
    fn from(path: &Path<T>) -> Self {
        let rc: Rc<[u8]> = Rc::from(path.as_bytes());
        unsafe { Rc::from_raw(Rc::into_raw(rc) as *const Path<T>) }
    }
}

impl<'a, T> IntoIterator for &'a Path<T>
where
    T: Encoding,
{
    type IntoIter = Iter<'a, T>;
    type Item = &'a [u8];

    #[inline]
    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

impl<T> ToOwned for Path<T>
where
    T: Encoding,
{
    type Owned = PathBuf<T>;

    #[inline]
    fn to_owned(&self) -> Self::Owned {
        self.to_path_buf()
    }
}

macro_rules! impl_cmp {
    ($($lt:lifetime),* ; $lhs:ty, $rhs: ty) => {
        impl<$($lt,)* T> PartialEq<$rhs> for $lhs
        where
            T:  Encoding,
        {
            #[inline]
            fn eq(&self, other: &$rhs) -> bool {
                <Path<T> as PartialEq>::eq(self, other)
            }
        }

        impl<$($lt,)* T> PartialEq<$lhs> for $rhs
        where
            T:  Encoding,
        {
            #[inline]
            fn eq(&self, other: &$lhs) -> bool {
                <Path<T> as PartialEq>::eq(self, other)
            }
        }

        impl<$($lt,)* T> PartialOrd<$rhs> for $lhs
        where
            T:  Encoding,
        {
            #[inline]
            fn partial_cmp(&self, other: &$rhs) -> Option<cmp::Ordering> {
                <Path<T> as PartialOrd>::partial_cmp(self, other)
            }
        }

        impl<$($lt,)* T> PartialOrd<$lhs> for $rhs
        where
            T:  Encoding,
        {
            #[inline]
            fn partial_cmp(&self, other: &$lhs) -> Option<cmp::Ordering> {
                <Path<T> as PartialOrd>::partial_cmp(self, other)
            }
        }
    };
}

impl_cmp!(; PathBuf<T>, Path<T>);
impl_cmp!('a; PathBuf<T>, &'a Path<T>);
impl_cmp!('a; Cow<'a, Path<T>>, Path<T>);
impl_cmp!('a, 'b; Cow<'a, Path<T>>, &'b Path<T>);
impl_cmp!('a; Cow<'a, Path<T>>, PathBuf<T>);

macro_rules! impl_cmp_bytes {
    ($($lt:lifetime),* ; $lhs:ty, $rhs: ty) => {
        impl<$($lt,)* T> PartialEq<$rhs> for $lhs
        where
            T:  Encoding,
        {
            #[inline]
            fn eq(&self, other: &$rhs) -> bool {
                <Path<T> as PartialEq>::eq(self, other.as_ref())
            }
        }

        impl<$($lt,)* T> PartialEq<$lhs> for $rhs
        where
            T:  Encoding,
        {
            #[inline]
            fn eq(&self, other: &$lhs) -> bool {
                <Path<T> as PartialEq>::eq(self.as_ref(), other)
            }
        }

        impl<$($lt,)* T> PartialOrd<$rhs> for $lhs
        where
            T:  Encoding,
        {
            #[inline]
            fn partial_cmp(&self, other: &$rhs) -> Option<cmp::Ordering> {
                <Path<T> as PartialOrd>::partial_cmp(self, other.as_ref())
            }
        }

        impl<$($lt,)* T> PartialOrd<$lhs> for $rhs
        where
            T:  Encoding,
        {
            #[inline]
            fn partial_cmp(&self, other: &$lhs) -> Option<cmp::Ordering> {
                <Path<T> as PartialOrd>::partial_cmp(self.as_ref(), other)
            }
        }
    };
}

impl_cmp_bytes!(; PathBuf<T>, [u8]);
impl_cmp_bytes!('a; PathBuf<T>, &'a [u8]);
impl_cmp_bytes!(; PathBuf<T>, Vec<u8>);
impl_cmp_bytes!(; Path<T>, [u8]);
impl_cmp_bytes!('a; Path<T>, &'a [u8]);
impl_cmp_bytes!(; Path<T>, Vec<u8>);
impl_cmp_bytes!('a; &'a Path<T>, [u8]);
impl_cmp_bytes!('a; &'a Path<T>, Vec<u8>);

mod helpers {
    use super::*;

    pub fn rsplit_file_at_dot(file: &[u8]) -> (Option<&[u8]>, Option<&[u8]>) {
        if file == b".." {
            return (Some(file), None);
        }

        let mut iter = file.rsplitn(2, |b| *b == b'.');
        let after = iter.next();
        let before = iter.next();
        if before == Some(b"") {
            (Some(file), None)
        } else {
            (before, after)
        }
    }

    // Iterate through `iter` while it matches `prefix`; return `None` if `prefix`
    // is not a prefix of `iter`, otherwise return `Some(iter_after_prefix)` giving
    // `iter` after having exhausted `prefix`.
    pub fn iter_after<'a, 'b, T, U, I, J>(mut iter: I, mut prefix: J) -> Option<I>
    where
        T: Component<'a>,
        U: Component<'b>,
        I: Iterator<Item = T> + Clone,
        J: Iterator<Item = U>,
    {
        loop {
            let mut iter_next = iter.clone();
            match (iter_next.next(), prefix.next()) {
                // TODO: Because there is not a `Component` struct, there is no direct comparison
                //       between T and U since they aren't the same type due to different
                //       lifetimes. We get around this with an equality check by converting these
                //       components to bytes, which should work for the Unix and Windows component
                //       implementations, but is error-prone as any new implementation could
                //       deviate in a way that breaks this subtlely. Instead, need to figure out
                //       either how to bring back equality of x == y WITHOUT needing to have
                //       T: PartialEq<U> because that causes lifetime problems for `strip_prefix`
                (Some(ref x), Some(ref y)) if x.as_bytes() == y.as_bytes() => (),
                (Some(_), Some(_)) => return None,
                (Some(_), None) => return Some(iter),
                (None, None) => return Some(iter),
                (None, Some(_)) => return None,
            }
            iter = iter_next;
        }
    }
}

#[cfg(any(
    unix,
    all(target_vendor = "fortanix", target_env = "sgx"),
    target_os = "solid_asp3",
    target_os = "hermit",
    target_os = "wasi"
))]
#[cfg(feature = "std")]
mod std_conversions {
    use std::ffi::{OsStr, OsString};
    #[cfg(all(target_vendor = "fortanix", target_env = "sgx"))]
    use std::os::fortanix_sgx as os;
    #[cfg(target_os = "solid_asp3")]
    use std::os::solid as os;
    #[cfg(any(target_os = "hermit", unix))]
    use std::os::unix as os;
    #[cfg(target_os = "wasi")]
    use std::os::wasi as os;

    use os::ffi::OsStrExt;

    use super::*;

    impl<T> AsRef<Path<T>> for OsStr
    where
        T: Encoding,
    {
        #[inline]
        fn as_ref(&self) -> &Path<T> {
            Path::new(self.as_bytes())
        }
    }

    impl<T> AsRef<Path<T>> for OsString
    where
        T: Encoding,
    {
        #[inline]
        fn as_ref(&self) -> &Path<T> {
            Path::new(self.as_bytes())
        }
    }

    impl<T> AsRef<OsStr> for Path<T>
    where
        T: Encoding,
    {
        #[inline]
        fn as_ref(&self) -> &OsStr {
            OsStrExt::from_bytes(self.as_bytes())
        }
    }
}
