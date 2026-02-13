use alloc::borrow::{Cow, ToOwned};
use alloc::rc::Rc;
#[cfg(target_has_atomic = "ptr")]
use alloc::sync::Arc;
use core::hash::{Hash, Hasher};
use core::marker::PhantomData;
use core::str::Utf8Error;
use core::{cmp, fmt};

use crate::no_std_compat::*;
use crate::{
    CheckedPathError, Encoding, Path, StripPrefixError, Utf8Ancestors, Utf8Component,
    Utf8Components, Utf8Encoding, Utf8Iter, Utf8PathBuf,
};

/// A slice of a path (akin to [`str`]).
///
/// This type supports a number of operations for inspecting a path, including
/// breaking the path into its components (separated by `/` on Unix and by either
/// `/` or `\` on Windows), extracting the file name, determining whether the path
/// is absolute, and so on.
///
/// This is an *unsized* type, meaning that it must always be used behind a
/// pointer like `&` or [`Box`]. For an owned version of this type,
/// see [`Utf8PathBuf`].
///
/// # Examples
///
/// ```
/// use typed_path::{Utf8Path, Utf8UnixEncoding};
///
/// // NOTE: A path cannot be created on its own without a defined encoding,
/// //       but all encodings work on all operating systems, providing the
/// //       ability to parse and operate on paths independently of the
/// //       compiled platform
/// let path = Utf8Path::<Utf8UnixEncoding>::new("./foo/bar.txt");
///
/// let parent = path.parent();
/// assert_eq!(parent, Some(Utf8Path::new("./foo")));
///
/// let file_stem = path.file_stem();
/// assert_eq!(file_stem, Some("bar"));
///
/// let extension = path.extension();
/// assert_eq!(extension, Some("txt"));
/// ```
///
/// In addition to explicitly using [`Utf8Encoding`]s, you can also
/// leverage aliases available from the crate to work with paths:
///
/// ```
/// use typed_path::{Utf8UnixPath, Utf8WindowsPath};
///
/// // Same as Utf8Path<Utf8UnixEncoding>
/// let path = Utf8UnixPath::new("/foo/bar.txt");
///
/// // Same as Utf8Path<Utf8WindowsEncoding>
/// let path = Utf8WindowsPath::new(r"C:\foo\bar.txt");
/// ```
///
/// To mirror the design of Rust's standard library, you can access
/// the path associated with the compiled rust platform using [`Utf8NativePath`],
/// which itself is an alias to one of the other choices:
///
/// ```
/// use typed_path::Utf8NativePath;
///
/// // On Unix, this would be Utf8UnixPath aka Utf8Path<Utf8UnixEncoding>
/// // On Windows, this would be Utf8WindowsPath aka Utf8Path<Utf8WindowsEncoding>
/// let path = Utf8NativePath::new("/foo/bar.txt");
/// ```
///
/// [`Utf8NativePath`]: crate::Utf8NativePath
#[repr(transparent)]
pub struct Utf8Path<T>
where
    T: Utf8Encoding,
{
    /// Encoding associated with path buf
    _encoding: PhantomData<T>,

    /// Path as an unparsed str slice
    pub(crate) inner: str,
}

impl<T> Utf8Path<T>
where
    T: Utf8Encoding,
{
    /// Directly wraps a str slice as a `Utf8Path` slice.
    ///
    /// This is a cost-free conversion.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// Utf8Path::<Utf8UnixEncoding>::new("foo.txt");
    /// ```
    ///
    /// You can create `Utf8Path`s from `String`s, or even other `Utf8Path`s:
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let string = String::from("foo.txt");
    /// let from_string = Utf8Path::<Utf8UnixEncoding>::new(&string);
    /// let from_path = Utf8Path::new(&from_string);
    /// assert_eq!(from_string, from_path);
    /// ```
    ///
    /// There are also handy aliases to the `Utf8Path` with [`Utf8Encoding`]:
    ///
    /// ```
    /// use typed_path::Utf8UnixPath;
    ///
    /// let string = String::from("foo.txt");
    /// let from_string = Utf8UnixPath::new(&string);
    /// let from_path = Utf8UnixPath::new(&from_string);
    /// assert_eq!(from_string, from_path);
    /// ```
    #[inline]
    pub fn new<S: AsRef<str> + ?Sized>(s: &S) -> &Self {
        unsafe { &*(s.as_ref() as *const str as *const Self) }
    }

    /// Yields the underlying [`str`] slice.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let s = Utf8Path::<Utf8UnixEncoding>::new("foo.txt").as_str();
    /// assert_eq!(s, "foo.txt");
    /// ```
    pub fn as_str(&self) -> &str {
        &self.inner
    }

    /// Converts a `Utf8Path` to an owned [`Utf8PathBuf`].
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8PathBuf, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path_buf = Utf8Path::<Utf8UnixEncoding>::new("foo.txt").to_path_buf();
    /// assert_eq!(path_buf, Utf8PathBuf::from("foo.txt"));
    /// ```
    pub fn to_path_buf(&self) -> Utf8PathBuf<T> {
        Utf8PathBuf {
            _encoding: PhantomData,
            inner: self.inner.to_owned(),
        }
    }

    /// Returns `true` if the `Utf8Path` is absolute, i.e., if it is independent of
    /// the current directory.
    ///
    /// * On Unix ([`Utf8UnixPath`]]), a path is absolute if it starts with the root, so
    ///   `is_absolute` and [`has_root`] are equivalent.
    ///
    /// * On Windows ([`Utf8WindowsPath`]), a path is absolute if it has a prefix and starts with
    ///   the root: `c:\windows` is absolute, while `c:temp` and `\temp` are not.
    ///
    /// [`Utf8UnixPath`]: crate::Utf8UnixPath
    /// [`Utf8WindowsPath`]: crate::Utf8WindowsPath
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert!(!Utf8Path::<Utf8UnixEncoding>::new("foo.txt").is_absolute());
    /// ```
    ///
    /// [`has_root`]: Utf8Path::has_root
    pub fn is_absolute(&self) -> bool {
        self.components().is_absolute()
    }

    /// Returns `true` if the `Utf8Path` is relative, i.e., not absolute.
    ///
    /// See [`is_absolute`]'s documentation for more details.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert!(Utf8Path::<Utf8UnixEncoding>::new("foo.txt").is_relative());
    /// ```
    ///
    /// [`is_absolute`]: Utf8Path::is_absolute
    #[inline]
    pub fn is_relative(&self) -> bool {
        !self.is_absolute()
    }

    /// Returns `true` if the path is valid, meaning that all of its components are valid.
    ///
    /// See [`Utf8Component::is_valid`]'s documentation for more details.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert!(Utf8Path::<Utf8UnixEncoding>::new("foo.txt").is_valid());
    /// assert!(!Utf8Path::<Utf8UnixEncoding>::new("foo\0.txt").is_valid());
    /// ```
    ///
    /// [`Utf8Component::is_valid`]: crate::Utf8Component::is_valid
    pub fn is_valid(&self) -> bool {
        self.components().all(|c| c.is_valid())
    }

    /// Returns `true` if the `Utf8Path` has a root.
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
    /// use typed_path::{Utf8Path, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert!(Utf8Path::<Utf8UnixEncoding>::new("/etc/passwd").has_root());
    /// ```
    #[inline]
    pub fn has_root(&self) -> bool {
        self.components().has_root()
    }

    /// Returns the `Utf8Path` without its final component, if there is one.
    ///
    /// Returns [`None`] if the path terminates in a root or prefix.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Utf8Path::<Utf8UnixEncoding>::new("/foo/bar");
    /// let parent = path.parent().unwrap();
    /// assert_eq!(parent, Utf8Path::new("/foo"));
    ///
    /// let grand_parent = parent.parent().unwrap();
    /// assert_eq!(grand_parent, Utf8Path::new("/"));
    /// assert_eq!(grand_parent.parent(), None);
    /// ```
    pub fn parent(&self) -> Option<&Self> {
        let mut comps = self.components();
        let comp = comps.next_back();
        comp.and_then(|p| {
            if !p.is_root() {
                Some(Self::new(comps.as_str()))
            } else {
                None
            }
        })
    }

    /// Produces an iterator over `Utf8Path` and its ancestors.
    ///
    /// The iterator will yield the `Utf8Path` that is returned if the [`parent`] method is used zero
    /// or more times. That means, the iterator will yield `&self`, `&self.parent().unwrap()`,
    /// `&self.parent().unwrap().parent().unwrap()` and so on. If the [`parent`] method returns
    /// [`None`], the iterator will do likewise. The iterator will always yield at least one value,
    /// namely `&self`.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let mut ancestors = Utf8Path::<Utf8UnixEncoding>::new("/foo/bar").ancestors();
    /// assert_eq!(ancestors.next(), Some(Utf8Path::new("/foo/bar")));
    /// assert_eq!(ancestors.next(), Some(Utf8Path::new("/foo")));
    /// assert_eq!(ancestors.next(), Some(Utf8Path::new("/")));
    /// assert_eq!(ancestors.next(), None);
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let mut ancestors = Utf8Path::<Utf8UnixEncoding>::new("../foo/bar").ancestors();
    /// assert_eq!(ancestors.next(), Some(Utf8Path::new("../foo/bar")));
    /// assert_eq!(ancestors.next(), Some(Utf8Path::new("../foo")));
    /// assert_eq!(ancestors.next(), Some(Utf8Path::new("..")));
    /// assert_eq!(ancestors.next(), Some(Utf8Path::new("")));
    /// assert_eq!(ancestors.next(), None);
    /// ```
    ///
    /// [`parent`]: Utf8Path::parent
    #[inline]
    pub fn ancestors(&self) -> Utf8Ancestors<'_, T> {
        Utf8Ancestors { next: Some(self) }
    }

    /// Returns the final component of the `Utf8Path`, if there is one.
    ///
    /// If the path is a normal file, this is the file name. If it's the path of a directory, this
    /// is the directory name.
    ///
    /// Returns [`None`] if the path terminates in `..`.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert_eq!(Some("bin"), Utf8Path::<Utf8UnixEncoding>::new("/usr/bin/").file_name());
    /// assert_eq!(Some("foo.txt"), Utf8Path::<Utf8UnixEncoding>::new("tmp/foo.txt").file_name());
    /// assert_eq!(Some("foo.txt"), Utf8Path::<Utf8UnixEncoding>::new("foo.txt/.").file_name());
    /// assert_eq!(Some("foo.txt"), Utf8Path::<Utf8UnixEncoding>::new("foo.txt/.//").file_name());
    /// assert_eq!(None, Utf8Path::<Utf8UnixEncoding>::new("foo.txt/..").file_name());
    /// assert_eq!(None, Utf8Path::<Utf8UnixEncoding>::new("/").file_name());
    /// ```
    pub fn file_name(&self) -> Option<&str> {
        match self.components().next_back() {
            Some(p) => {
                if p.is_normal() {
                    Some(p.as_str())
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
    /// [`starts_with`]: Utf8Path::starts_with
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8PathBuf, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Utf8Path::<Utf8UnixEncoding>::new("/test/haha/foo.txt");
    ///
    /// assert_eq!(path.strip_prefix("/"), Ok(Utf8Path::new("test/haha/foo.txt")));
    /// assert_eq!(path.strip_prefix("/test"), Ok(Utf8Path::new("haha/foo.txt")));
    /// assert_eq!(path.strip_prefix("/test/"), Ok(Utf8Path::new("haha/foo.txt")));
    /// assert_eq!(path.strip_prefix("/test/haha/foo.txt"), Ok(Utf8Path::new("")));
    /// assert_eq!(path.strip_prefix("/test/haha/foo.txt/"), Ok(Utf8Path::new("")));
    ///
    /// assert!(path.strip_prefix("test").is_err());
    /// assert!(path.strip_prefix("/haha").is_err());
    ///
    /// let prefix = Utf8PathBuf::<Utf8UnixEncoding>::from("/test/");
    /// assert_eq!(path.strip_prefix(prefix), Ok(Utf8Path::new("haha/foo.txt")));
    /// ```
    pub fn strip_prefix<P>(&self, base: P) -> Result<&Utf8Path<T>, StripPrefixError>
    where
        P: AsRef<Utf8Path<T>>,
    {
        self._strip_prefix(base.as_ref())
    }

    fn _strip_prefix(&self, base: &Utf8Path<T>) -> Result<&Utf8Path<T>, StripPrefixError> {
        match helpers::iter_after(self.components(), base.components()) {
            Some(c) => Ok(Utf8Path::new(c.as_str())),
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
    /// use typed_path::{Utf8Path, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Utf8Path::<Utf8UnixEncoding>::new("/etc/passwd");
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
    /// assert!(!Utf8Path::<Utf8UnixEncoding>::new("/etc/foo.rs").starts_with("/etc/foo"));
    /// ```
    pub fn starts_with<P>(&self, base: P) -> bool
    where
        P: AsRef<Utf8Path<T>>,
    {
        self._starts_with(base.as_ref())
    }

    fn _starts_with(&self, base: &Utf8Path<T>) -> bool {
        helpers::iter_after(self.components(), base.components()).is_some()
    }

    /// Determines whether `child` is a suffix of `self`.
    ///
    /// Only considers whole path components to match.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Utf8Path::<Utf8UnixEncoding>::new("/etc/resolv.conf");
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
        P: AsRef<Utf8Path<T>>,
    {
        self._ends_with(child.as_ref())
    }

    fn _ends_with(&self, child: &Utf8Path<T>) -> bool {
        helpers::iter_after(self.components().rev(), child.components().rev()).is_some()
    }

    /// Extracts the stem (non-extension) portion of [`self.file_name`].
    ///
    /// [`self.file_name`]: Utf8Path::file_name
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
    /// use typed_path::{Utf8Path, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert_eq!("foo", Utf8Path::<Utf8UnixEncoding>::new("foo.rs").file_stem().unwrap());
    /// assert_eq!("foo.tar", Utf8Path::<Utf8UnixEncoding>::new("foo.tar.gz").file_stem().unwrap());
    /// ```
    ///
    pub fn file_stem(&self) -> Option<&str> {
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
    /// [`self.file_name`]: Utf8Path::file_name
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert_eq!("rs", Utf8Path::<Utf8UnixEncoding>::new("foo.rs").extension().unwrap());
    /// assert_eq!("gz", Utf8Path::<Utf8UnixEncoding>::new("foo.tar.gz").extension().unwrap());
    /// ```
    pub fn extension(&self) -> Option<&str> {
        self.file_name()
            .map(helpers::rsplit_file_at_dot)
            .and_then(|(before, after)| before.and(after))
    }

    /// Returns an owned [`Utf8PathBuf`] by resolving `..` and `.` segments.
    ///
    /// When multiple, sequential path segment separation characters are found (e.g. `/` for Unix
    /// and either `\` or `/` on Windows), they are replaced by a single instance of the
    /// platform-specific path segment separator (`/` on Unix and `\` on Windows).
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8PathBuf, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert_eq!(
    ///     Utf8Path::<Utf8UnixEncoding>::new("foo/bar//baz/./asdf/quux/..").normalize(),
    ///     Utf8PathBuf::from("foo/bar/baz/asdf"),
    /// );
    /// ```
    ///
    /// When starting with a root directory, any `..` segment whose parent is the root directory
    /// will be filtered out:
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8PathBuf, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert_eq!(
    ///     Utf8Path::<Utf8UnixEncoding>::new("/../foo").normalize(),
    ///     Utf8PathBuf::from("/foo"),
    /// );
    /// ```
    ///
    /// If any `..` is left unresolved as the path is relative and no parent is found, it is
    /// discarded:
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8PathBuf, Utf8UnixEncoding, Utf8WindowsEncoding};
    ///
    /// assert_eq!(
    ///     Utf8Path::<Utf8UnixEncoding>::new("../foo/..").normalize(),
    ///     Utf8PathBuf::from(""),
    /// );
    ///
    /// //Windows prefixes also count this way, but the prefix remains
    /// assert_eq!(
    ///     Utf8Path::<Utf8WindowsEncoding>::new(r"C:..\foo\..").normalize(),
    ///     Utf8PathBuf::from(r"C:"),
    /// );
    /// ```
    pub fn normalize(&self) -> Utf8PathBuf<T> {
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

        let mut path = Utf8PathBuf::<T>::new();

        for component in components {
            path.push(component.as_str());
        }

        path
    }

    /// Converts a path to an absolute form by [`normalizing`] the path, returning a
    /// [`Utf8PathBuf`].
    ///
    /// In the case that the path is relative, the current working directory is prepended prior to
    /// normalizing.
    ///
    /// [`normalizing`]: Utf8Path::normalize
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{utils, Utf8Path, Utf8UnixEncoding};
    ///
    /// // With an absolute path, it is just normalized
    /// let path = Utf8Path::<Utf8UnixEncoding>::new("/a/b/../c/./d");
    /// assert_eq!(path.absolutize().unwrap(), Utf8Path::new("/a/c/d"));
    ///
    /// // With a relative path, it is first joined with the current working directory
    /// // and then normalized
    /// let cwd = utils::utf8_current_dir().unwrap().with_encoding::<Utf8UnixEncoding>();
    /// let path = cwd.join(Utf8Path::new("a/b/../c/./d"));
    /// assert_eq!(path.absolutize().unwrap(), cwd.join(Utf8Path::new("a/c/d")));
    /// ```
    #[cfg(all(feature = "std", not(target_family = "wasm")))]
    pub fn absolutize(&self) -> std::io::Result<Utf8PathBuf<T>> {
        if self.is_absolute() {
            Ok(self.normalize())
        } else {
            // Get the cwd as a platform path and convert to this path's encoding
            let cwd = crate::utils::utf8_current_dir()?.with_encoding();

            Ok(cwd.join(self).normalize())
        }
    }

    /// Creates an owned [`Utf8PathBuf`] with `path` adjoined to `self`.
    ///
    /// See [`Utf8PathBuf::push`] for more details on what it means to adjoin a path.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8PathBuf, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// assert_eq!(
    ///     Utf8Path::<Utf8UnixEncoding>::new("/etc").join("passwd"),
    ///     Utf8PathBuf::from("/etc/passwd"),
    /// );
    /// ```
    pub fn join<P: AsRef<Utf8Path<T>>>(&self, path: P) -> Utf8PathBuf<T> {
        self._join(path.as_ref())
    }

    fn _join(&self, path: &Utf8Path<T>) -> Utf8PathBuf<T> {
        let mut buf = self.to_path_buf();
        buf.push(path);
        buf
    }

    /// Creates an owned [`Utf8PathBuf`] with `path` adjoined to `self`, checking the `path` to
    /// ensure it is safe to join. _When dealing with user-provided paths, this is the preferred
    /// method._
    ///
    /// See [`Utf8PathBuf::push_checked`] for more details on what it means to adjoin a path
    /// safely.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{CheckedPathError, Utf8Path, Utf8PathBuf, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Utf8Path::<Utf8UnixEncoding>::new("/etc");
    ///
    /// // A valid path can be joined onto the existing one
    /// assert_eq!(path.join_checked("passwd"), Ok(Utf8PathBuf::from("/etc/passwd")));
    ///
    /// // An invalid path will result in an error
    /// assert_eq!(path.join_checked("/sneaky/replacement"), Err(CheckedPathError::UnexpectedRoot));
    /// ```
    pub fn join_checked<P: AsRef<Utf8Path<T>>>(
        &self,
        path: P,
    ) -> Result<Utf8PathBuf<T>, CheckedPathError> {
        self._join_checked(path.as_ref())
    }

    fn _join_checked(&self, path: &Utf8Path<T>) -> Result<Utf8PathBuf<T>, CheckedPathError> {
        let mut buf = self.to_path_buf();
        buf.push_checked(path)?;
        Ok(buf)
    }

    /// Creates an owned [`Utf8PathBuf`] like `self` but with the given file name.
    ///
    /// See [`Utf8PathBuf::set_file_name`] for more details.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8PathBuf, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Utf8Path::<Utf8UnixEncoding>::new("/tmp/foo.txt");
    /// assert_eq!(path.with_file_name("bar.txt"), Utf8PathBuf::from("/tmp/bar.txt"));
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Utf8Path::<Utf8UnixEncoding>::new("/tmp");
    /// assert_eq!(path.with_file_name("var"), Utf8PathBuf::from("/var"));
    /// ```
    pub fn with_file_name<S: AsRef<str>>(&self, file_name: S) -> Utf8PathBuf<T> {
        self._with_file_name(file_name.as_ref())
    }

    fn _with_file_name(&self, file_name: &str) -> Utf8PathBuf<T> {
        let mut buf = self.to_path_buf();
        buf.set_file_name(file_name);
        buf
    }

    /// Creates an owned [`Utf8PathBuf`] like `self` but with the given extension.
    ///
    /// See [`Utf8PathBuf::set_extension`] for more details.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8PathBuf, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Utf8Path::<Utf8UnixEncoding>::new("foo.rs");
    /// assert_eq!(path.with_extension("txt"), Utf8PathBuf::from("foo.txt"));
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let path = Utf8Path::<Utf8UnixEncoding>::new("foo.tar.gz");
    /// assert_eq!(path.with_extension(""), Utf8PathBuf::from("foo.tar"));
    /// assert_eq!(path.with_extension("xz"), Utf8PathBuf::from("foo.tar.xz"));
    /// assert_eq!(path.with_extension("").with_extension("txt"), Utf8PathBuf::from("foo.txt"));
    /// ```
    pub fn with_extension<S: AsRef<str>>(&self, extension: S) -> Utf8PathBuf<T> {
        self._with_extension(extension.as_ref())
    }

    fn _with_extension(&self, extension: &str) -> Utf8PathBuf<T> {
        let mut buf = self.to_path_buf();
        buf.set_extension(extension);
        buf
    }

    /// Produces an iterator over the [`Utf8Component`]s of the path.
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
    /// use typed_path::{Utf8Path, Utf8UnixComponent, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let mut components = Utf8Path::<Utf8UnixEncoding>::new("/tmp/foo.txt").components();
    ///
    /// assert_eq!(components.next(), Some(Utf8UnixComponent::RootDir));
    /// assert_eq!(components.next(), Some(Utf8UnixComponent::Normal("tmp")));
    /// assert_eq!(components.next(), Some(Utf8UnixComponent::Normal("foo.txt")));
    /// assert_eq!(components.next(), None)
    /// ```
    ///
    /// [`CurDir`]: crate::unix::UnixComponent::CurDir
    pub fn components(&self) -> <T as Utf8Encoding>::Components<'_> {
        T::components(&self.inner)
    }

    /// Produces an iterator over the path's components viewed as [`str`] slices.
    ///
    /// For more information about the particulars of how the path is separated
    /// into components, see [`components`].
    ///
    /// [`components`]: Utf8Path::components
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let mut it = Utf8Path::<Utf8UnixEncoding>::new("/tmp/foo.txt").iter();
    ///
    /// assert_eq!(it.next(), Some(typed_path::constants::unix::SEPARATOR_STR));
    /// assert_eq!(it.next(), Some("tmp"));
    /// assert_eq!(it.next(), Some("foo.txt"));
    /// assert_eq!(it.next(), None)
    /// ```
    #[inline]
    pub fn iter(&self) -> Utf8Iter<'_, T> {
        Utf8Iter::new(self.components())
    }

    /// Creates an owned [`Utf8PathBuf`] like `self` but with a different encoding.
    ///
    /// # Note
    ///
    /// As part of the process of converting between encodings, the path will need to be rebuilt.
    /// This involves [`pushing`] each component, which may result in differences in the resulting
    /// path such as resolving `.` and `..` early or other unexpected side effects.
    ///
    /// [`pushing`]: Utf8PathBuf::push
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8UnixEncoding, Utf8WindowsEncoding};
    ///
    /// // Convert from Unix to Windows
    /// let unix_path = Utf8Path::<Utf8UnixEncoding>::new("/tmp/foo.txt");
    /// let windows_path = unix_path.with_encoding::<Utf8WindowsEncoding>();
    /// assert_eq!(windows_path, Utf8Path::<Utf8WindowsEncoding>::new(r"\tmp\foo.txt"));
    ///
    /// // Converting from Windows to Unix will drop any prefix
    /// let windows_path = Utf8Path::<Utf8WindowsEncoding>::new(r"C:\tmp\foo.txt");
    /// let unix_path = windows_path.with_encoding::<Utf8UnixEncoding>();
    /// assert_eq!(unix_path, Utf8Path::<Utf8UnixEncoding>::new(r"/tmp/foo.txt"));
    ///
    /// // Converting to itself should retain everything
    /// let path = Utf8Path::<Utf8WindowsEncoding>::new(r"C:\tmp\foo.txt");
    /// assert_eq!(
    ///     path.with_encoding::<Utf8WindowsEncoding>(),
    ///     Utf8Path::<Utf8WindowsEncoding>::new(r"C:\tmp\foo.txt"),
    /// );
    /// ```
    pub fn with_encoding<U>(&self) -> Utf8PathBuf<U>
    where
        U: Utf8Encoding,
    {
        // If we're the same, just return the path buf, which
        // we do with a fancy trick to convert it
        if T::label() == U::label() {
            Utf8Path::new(self.as_str()).to_path_buf()
        } else {
            // Otherwise, we have to rebuild the path from the components
            let mut path = Utf8PathBuf::new();

            // For root, current, and parent we specially handle to convert
            // to the appropriate type, otherwise we pass along as-is
            for component in self.components() {
                if component.is_root() {
                    path.push(<
                        <<U as Utf8Encoding>::Components<'_> as Utf8Components>::Component
                        as Utf8Component
                    >::root().as_str());
                } else if component.is_current() {
                    path.push(<
                        <<U as Utf8Encoding>::Components<'_> as Utf8Components>::Component
                        as Utf8Component
                    >::current().as_str());
                } else if component.is_parent() {
                    path.push(<
                        <<U as Utf8Encoding>::Components<'_> as Utf8Components>::Component
                        as Utf8Component
                    >::parent().as_str());
                } else {
                    path.push(component.as_str());
                }
            }

            path
        }
    }

    /// Like [`with_encoding`], creates an owned [`Utf8PathBuf`] like `self` but with a different
    /// encoding. Additionally, checks to ensure that the produced path will be valid.
    ///
    /// # Note
    ///
    /// As part of the process of converting between encodings, the path will need to be rebuilt.
    /// This involves [`pushing and checking`] each component, which may result in differences in
    /// the resulting path such as resolving `.` and `..` early or other unexpected side effects.
    ///
    /// [`pushing and checking`]: Utf8PathBuf::push_checked
    /// [`with_encoding`]: Utf8Path::with_encoding
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{CheckedPathError, Utf8Path, Utf8UnixEncoding, Utf8WindowsEncoding};
    ///
    /// // Convert from Unix to Windows
    /// let unix_path = Utf8Path::<Utf8UnixEncoding>::new("/tmp/foo.txt");
    /// let windows_path = unix_path.with_encoding_checked::<Utf8WindowsEncoding>().unwrap();
    /// assert_eq!(windows_path, Utf8Path::<Utf8WindowsEncoding>::new(r"\tmp\foo.txt"));
    ///
    /// // Converting from Windows to Unix will drop any prefix
    /// let windows_path = Utf8Path::<Utf8WindowsEncoding>::new(r"C:\tmp\foo.txt");
    /// let unix_path = windows_path.with_encoding_checked::<Utf8UnixEncoding>().unwrap();
    /// assert_eq!(unix_path, Utf8Path::<Utf8UnixEncoding>::new(r"/tmp/foo.txt"));
    ///
    /// // Converting from Unix to Windows with invalid filename characters like `|` should fail
    /// let unix_path = Utf8Path::<Utf8UnixEncoding>::new("/|invalid|/foo.txt");
    /// assert_eq!(
    ///     unix_path.with_encoding_checked::<Utf8WindowsEncoding>(),
    ///     Err(CheckedPathError::InvalidFilename),
    /// );
    ///
    /// // Converting from Unix to Windows with unexpected prefix embedded in path should fail
    /// let unix_path = Utf8Path::<Utf8UnixEncoding>::new("/path/c:/foo.txt");
    /// assert_eq!(
    ///     unix_path.with_encoding_checked::<Utf8WindowsEncoding>(),
    ///     Err(CheckedPathError::UnexpectedPrefix),
    /// );
    /// ```
    pub fn with_encoding_checked<U>(&self) -> Result<Utf8PathBuf<U>, CheckedPathError>
    where
        U: Utf8Encoding,
    {
        let mut path = Utf8PathBuf::new();

        // For root, current, and parent we specially handle to convert to the appropriate type,
        // otherwise we attempt to push using the checked variant, which will ensure that the
        // destination encoding is respected
        for component in self.components() {
            if component.is_root() {
                path.push(<
                        <<U as Utf8Encoding>::Components<'_> as Utf8Components>::Component
                        as Utf8Component
                    >::root().as_str());
            } else if component.is_current() {
                path.push(<
                        <<U as Utf8Encoding>::Components<'_> as Utf8Components>::Component
                        as Utf8Component
                    >::current().as_str());
            } else if component.is_parent() {
                path.push(<
                        <<U as Utf8Encoding>::Components<'_> as Utf8Components>::Component
                        as Utf8Component
                    >::parent().as_str());
            } else {
                path.push_checked(component.as_str())?;
            }
        }

        Ok(path)
    }

    /// Converts a [`Box<Utf8Path>`](Box) into a
    /// [`Utf8PathBuf`] without copying or allocating.
    pub fn into_path_buf(self: Box<Utf8Path<T>>) -> Utf8PathBuf<T> {
        let rw = Box::into_raw(self) as *mut str;
        let inner = unsafe { Box::from_raw(rw) };
        Utf8PathBuf {
            _encoding: PhantomData,
            inner: inner.into_string(),
        }
    }

    /// Converts a non-UTF-8 [`Path`] to a UTF-8 [`Utf8PathBuf`] by checking that the path contains
    /// valid UTF-8.
    ///
    /// # Errors
    ///
    /// Returns `Err` if the path is not UTF-8 with a description as to why the
    /// provided component is not UTF-8.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, Utf8Path, UnixEncoding, Utf8UnixEncoding};
    ///
    /// let path = Path::<UnixEncoding>::new(&[0xf0, 0x9f, 0x92, 0x96]);
    /// let utf8_path = Utf8Path::<Utf8UnixEncoding>::from_bytes_path(&path).unwrap();
    /// assert_eq!(utf8_path.as_str(), "💖");
    /// ```
    pub fn from_bytes_path<U>(path: &Path<U>) -> Result<&Self, Utf8Error>
    where
        U: Encoding,
    {
        Ok(Self::new(core::str::from_utf8(path.as_bytes())?))
    }

    /// Converts a non-UTF-8 [`Path`] to a UTF-8 [`Utf8Path`] without checking that the path
    /// contains valid UTF-8.
    ///
    /// See the safe version, [`from_bytes_path`], for more information.
    ///
    /// [`from_bytes_path`]: Utf8Path::from_bytes_path
    ///
    /// # Safety
    ///
    /// The path passed in must be valid UTF-8.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, Utf8Path, UnixEncoding, Utf8UnixEncoding};
    ///
    /// let path = Path::<UnixEncoding>::new(&[0xf0, 0x9f, 0x92, 0x96]);
    /// let utf8_path = unsafe {
    ///     Utf8Path::<Utf8UnixEncoding>::from_bytes_path_unchecked(&path)
    /// };
    /// assert_eq!(utf8_path.as_str(), "💖");
    /// ```
    pub unsafe fn from_bytes_path_unchecked<U>(path: &Path<U>) -> &Self
    where
        U: Encoding,
    {
        Self::new(core::str::from_utf8_unchecked(path.as_bytes()))
    }

    /// Converts a UTF-8 [`Utf8Path`] to a non-UTF-8 [`Path`].
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, Utf8Path, UnixEncoding, Utf8UnixEncoding};
    ///
    /// let utf8_path = Utf8Path::<Utf8UnixEncoding>::new("💖");
    /// let path = utf8_path.as_bytes_path::<UnixEncoding>();
    /// assert_eq!(path.as_bytes(), &[0xf0, 0x9f, 0x92, 0x96]);
    /// ```
    pub fn as_bytes_path<U>(&self) -> &Path<U>
    where
        U: Encoding,
    {
        Path::new(self.as_str())
    }
}

impl<T> Clone for Box<Utf8Path<T>>
where
    T: Utf8Encoding,
{
    fn clone(&self) -> Self {
        self.to_path_buf().into_boxed_path()
    }
}

impl<T> AsRef<[u8]> for Utf8Path<T>
where
    T: Utf8Encoding,
{
    #[inline]
    fn as_ref(&self) -> &[u8] {
        self.inner.as_bytes()
    }
}

impl<T> AsRef<str> for Utf8Path<T>
where
    T: Utf8Encoding,
{
    #[inline]
    fn as_ref(&self) -> &str {
        &self.inner
    }
}

impl<T> AsRef<Utf8Path<T>> for Utf8Path<T>
where
    T: Utf8Encoding,
{
    #[inline]
    fn as_ref(&self) -> &Utf8Path<T> {
        self
    }
}

impl<T> AsRef<Utf8Path<T>> for str
where
    T: Utf8Encoding,
{
    #[inline]
    fn as_ref(&self) -> &Utf8Path<T> {
        Utf8Path::new(self)
    }
}

impl<T> AsRef<Utf8Path<T>> for String
where
    T: Utf8Encoding,
{
    #[inline]
    fn as_ref(&self) -> &Utf8Path<T> {
        Utf8Path::new(self)
    }
}

impl<T> fmt::Debug for Utf8Path<T>
where
    T: Utf8Encoding,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Utf8Path")
            .field("_encoding", &T::label())
            .field("inner", &&self.inner)
            .finish()
    }
}

impl<T> fmt::Display for Utf8Path<T>
where
    T: Utf8Encoding,
{
    /// Format path into a [`String`] using the underlying [`str`] representation.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let s = Utf8Path::<Utf8UnixEncoding>::new("foo.txt").to_string();
    /// assert_eq!(s, "foo.txt");
    /// ```
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(&self.inner, formatter)
    }
}

impl<T> cmp::PartialEq for Utf8Path<T>
where
    T: Utf8Encoding,
{
    #[inline]
    fn eq(&self, other: &Utf8Path<T>) -> bool {
        self.components() == other.components()
    }
}

impl<T> cmp::Eq for Utf8Path<T> where T: Utf8Encoding {}

impl<T> Hash for Utf8Path<T>
where
    T: Utf8Encoding,
{
    fn hash<H: Hasher>(&self, h: &mut H) {
        T::hash(self.as_str(), h)
    }
}

impl<T> cmp::PartialOrd for Utf8Path<T>
where
    T: Utf8Encoding,
{
    #[inline]
    fn partial_cmp(&self, other: &Utf8Path<T>) -> Option<cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl<T> cmp::Ord for Utf8Path<T>
where
    T: Utf8Encoding,
{
    #[inline]
    fn cmp(&self, other: &Utf8Path<T>) -> cmp::Ordering {
        self.components().cmp(other.components())
    }
}

impl<T> From<&Utf8Path<T>> for Box<Utf8Path<T>>
where
    T: Utf8Encoding,
{
    /// Creates a boxed [`Utf8Path`] from a reference.
    ///
    /// This will allocate and clone `path` to it.
    fn from(path: &Utf8Path<T>) -> Self {
        let boxed: Box<str> = path.inner.into();
        let rw = Box::into_raw(boxed) as *mut Utf8Path<T>;
        unsafe { Box::from_raw(rw) }
    }
}

impl<T> From<Cow<'_, Utf8Path<T>>> for Box<Utf8Path<T>>
where
    T: Utf8Encoding,
{
    /// Creates a boxed [`Utf8Path`] from a clone-on-write pointer.
    ///
    /// Converting from a `Cow::Owned` does not clone or allocate.
    #[inline]
    fn from(cow: Cow<'_, Utf8Path<T>>) -> Box<Utf8Path<T>> {
        match cow {
            Cow::Borrowed(path) => Box::from(path),
            Cow::Owned(path) => Box::from(path),
        }
    }
}

impl<T> From<Utf8PathBuf<T>> for Box<Utf8Path<T>>
where
    T: Utf8Encoding,
{
    /// Converts a [`Utf8PathBuf`] into a <code>[Box]&lt;[Utf8Path]&gt;</code>.
    ///
    /// This conversion currently should not allocate memory,
    /// but this behavior is not guaranteed on all platforms or in all future versions.
    #[inline]
    fn from(p: Utf8PathBuf<T>) -> Box<Utf8Path<T>> {
        p.into_boxed_path()
    }
}

impl<'a, T> From<&'a Utf8Path<T>> for Cow<'a, Utf8Path<T>>
where
    T: Utf8Encoding,
{
    /// Creates a clone-on-write pointer from a reference to
    /// [`Utf8Path`].
    ///
    /// This conversion does not clone or allocate.
    #[inline]
    fn from(s: &'a Utf8Path<T>) -> Self {
        Cow::Borrowed(s)
    }
}

impl<T> From<Utf8PathBuf<T>> for Cow<'_, Utf8Path<T>>
where
    T: Utf8Encoding,
{
    /// Creates a clone-on-write pointer from an owned
    /// instance of [`Utf8PathBuf`].
    ///
    /// This conversion does not clone or allocate.
    #[inline]
    fn from(s: Utf8PathBuf<T>) -> Self {
        Cow::Owned(s)
    }
}

impl<'a, T> From<&'a Utf8PathBuf<T>> for Cow<'a, Utf8Path<T>>
where
    T: Utf8Encoding,
{
    /// Creates a clone-on-write pointer from a reference to
    /// [`Utf8PathBuf`].
    ///
    /// This conversion does not clone or allocate.
    #[inline]
    fn from(p: &'a Utf8PathBuf<T>) -> Self {
        Cow::Borrowed(p.as_path())
    }
}

#[cfg(target_has_atomic = "ptr")]
impl<T> From<Utf8PathBuf<T>> for Arc<Utf8Path<T>>
where
    T: Utf8Encoding,
{
    /// Converts a [`Utf8PathBuf`] into an <code>[Arc]<[Utf8Path]></code> by moving the [`Utf8PathBuf`] data
    /// into a new [`Arc`] buffer.
    #[inline]
    fn from(path_buf: Utf8PathBuf<T>) -> Self {
        let arc: Arc<str> = Arc::from(path_buf.into_string());
        unsafe { Arc::from_raw(Arc::into_raw(arc) as *const Utf8Path<T>) }
    }
}

#[cfg(target_has_atomic = "ptr")]
impl<T> From<&Utf8Path<T>> for Arc<Utf8Path<T>>
where
    T: Utf8Encoding,
{
    /// Converts a [`Utf8Path`] into an [`Arc`] by copying the [`Utf8Path`] data into a new [`Arc`] buffer.
    #[inline]
    fn from(path: &Utf8Path<T>) -> Self {
        let arc: Arc<str> = Arc::from(path.as_str().to_string());
        unsafe { Arc::from_raw(Arc::into_raw(arc) as *const Utf8Path<T>) }
    }
}

impl<T> From<Utf8PathBuf<T>> for Rc<Utf8Path<T>>
where
    T: Utf8Encoding,
{
    /// Converts a [`Utf8PathBuf`] into an <code>[Rc]<[Utf8Path]></code> by moving the [`Utf8PathBuf`] data into
    /// a new [`Rc`] buffer.
    #[inline]
    fn from(path_buf: Utf8PathBuf<T>) -> Self {
        let rc: Rc<str> = Rc::from(path_buf.into_string());
        unsafe { Rc::from_raw(Rc::into_raw(rc) as *const Utf8Path<T>) }
    }
}

impl<T> From<&Utf8Path<T>> for Rc<Utf8Path<T>>
where
    T: Utf8Encoding,
{
    /// Converts a [`Utf8Path`] into an [`Rc`] by copying the [`Utf8Path`] data into a new [`Rc`] buffer.
    #[inline]
    fn from(path: &Utf8Path<T>) -> Self {
        let rc: Rc<str> = Rc::from(path.as_str());
        unsafe { Rc::from_raw(Rc::into_raw(rc) as *const Utf8Path<T>) }
    }
}

impl<'a, T> IntoIterator for &'a Utf8Path<T>
where
    T: Utf8Encoding,
{
    type IntoIter = Utf8Iter<'a, T>;
    type Item = &'a str;

    #[inline]
    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

impl<T> ToOwned for Utf8Path<T>
where
    T: Utf8Encoding,
{
    type Owned = Utf8PathBuf<T>;

    #[inline]
    fn to_owned(&self) -> Self::Owned {
        self.to_path_buf()
    }
}

macro_rules! impl_cmp {
    ($($lt:lifetime),* ; $lhs:ty, $rhs: ty) => {
        impl<$($lt,)* T> PartialEq<$rhs> for $lhs
        where
            T: Utf8Encoding,
        {
            #[inline]
            fn eq(&self, other: &$rhs) -> bool {
                <Utf8Path<T> as PartialEq>::eq(self, other)
            }
        }

        impl<$($lt,)* T> PartialEq<$lhs> for $rhs
        where
            T: Utf8Encoding,
        {
            #[inline]
            fn eq(&self, other: &$lhs) -> bool {
                <Utf8Path<T> as PartialEq>::eq(self, other)
            }
        }

        impl<$($lt,)* T> PartialOrd<$rhs> for $lhs
        where
            T: Utf8Encoding,
        {
            #[inline]
            fn partial_cmp(&self, other: &$rhs) -> Option<cmp::Ordering> {
                <Utf8Path<T> as PartialOrd>::partial_cmp(self, other)
            }
        }

        impl<$($lt,)* T> PartialOrd<$lhs> for $rhs
        where
            T: Utf8Encoding,
        {
            #[inline]
            fn partial_cmp(&self, other: &$lhs) -> Option<cmp::Ordering> {
                <Utf8Path<T> as PartialOrd>::partial_cmp(self, other)
            }
        }
    };
}

impl_cmp!(; Utf8PathBuf<T>, Utf8Path<T>);
impl_cmp!('a; Utf8PathBuf<T>, &'a Utf8Path<T>);
impl_cmp!('a; Cow<'a, Utf8Path<T>>, Utf8Path<T>);
impl_cmp!('a, 'b; Cow<'a, Utf8Path<T>>, &'b Utf8Path<T>);
impl_cmp!('a; Cow<'a, Utf8Path<T>>, Utf8PathBuf<T>);

macro_rules! impl_cmp_bytes {
    ($($lt:lifetime),* ; $lhs:ty, $rhs: ty) => {
        impl<$($lt,)* T> PartialEq<$rhs> for $lhs
        where
            T: Utf8Encoding,
        {
            #[inline]
            fn eq(&self, other: &$rhs) -> bool {
                <Utf8Path<T> as PartialEq>::eq(self, other.as_ref())
            }
        }

        impl<$($lt,)* T> PartialEq<$lhs> for $rhs
        where
            T: Utf8Encoding,
        {
            #[inline]
            fn eq(&self, other: &$lhs) -> bool {
                <Utf8Path<T> as PartialEq>::eq(self.as_ref(), other)
            }
        }

        impl<$($lt,)* T> PartialOrd<$rhs> for $lhs
        where
            T: Utf8Encoding,
        {
            #[inline]
            fn partial_cmp(&self, other: &$rhs) -> Option<cmp::Ordering> {
                <Utf8Path<T> as PartialOrd>::partial_cmp(self, other.as_ref())
            }
        }

        impl<$($lt,)* T> PartialOrd<$lhs> for $rhs
        where
            T: Utf8Encoding,
        {
            #[inline]
            fn partial_cmp(&self, other: &$lhs) -> Option<cmp::Ordering> {
                <Utf8Path<T> as PartialOrd>::partial_cmp(self.as_ref(), other)
            }
        }
    };
}

impl_cmp_bytes!(; Utf8PathBuf<T>, str);
impl_cmp_bytes!('a; Utf8PathBuf<T>, &'a str);
impl_cmp_bytes!(; Utf8PathBuf<T>, String);
impl_cmp_bytes!(; Utf8Path<T>, str);
impl_cmp_bytes!('a; Utf8Path<T>, &'a str);
impl_cmp_bytes!(; Utf8Path<T>, String);
impl_cmp_bytes!('a; &'a Utf8Path<T>, str);
impl_cmp_bytes!('a; &'a Utf8Path<T>, String);

mod helpers {
    use super::*;

    pub fn rsplit_file_at_dot(file: &str) -> (Option<&str>, Option<&str>) {
        if file == ".." {
            return (Some(file), None);
        }

        let mut iter = file.rsplitn(2, '.');
        let after = iter.next();
        let before = iter.next();
        if before == Some("") {
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
        T: Utf8Component<'a>,
        U: Utf8Component<'b>,
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
                (Some(ref x), Some(ref y)) if x.as_str() == y.as_str() => (),
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
    use crate::common::TryAsRef;

    impl<T> TryAsRef<Utf8Path<T>> for OsStr
    where
        T: Utf8Encoding,
    {
        #[inline]
        fn try_as_ref(&self) -> Option<&Utf8Path<T>> {
            std::str::from_utf8(self.as_bytes()).ok().map(Utf8Path::new)
        }
    }

    impl<T> TryAsRef<Utf8Path<T>> for OsString
    where
        T: Utf8Encoding,
    {
        #[inline]
        fn try_as_ref(&self) -> Option<&Utf8Path<T>> {
            std::str::from_utf8(self.as_bytes()).ok().map(Utf8Path::new)
        }
    }

    impl<T> AsRef<OsStr> for Utf8Path<T>
    where
        T: Utf8Encoding,
    {
        #[inline]
        fn as_ref(&self) -> &OsStr {
            OsStrExt::from_bytes(self.as_str().as_bytes())
        }
    }
}
