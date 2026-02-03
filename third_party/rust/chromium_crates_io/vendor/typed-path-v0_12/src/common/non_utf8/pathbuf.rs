use alloc::borrow::Cow;
use alloc::collections::TryReserveError;
use core::borrow::Borrow;
use core::hash::{Hash, Hasher};
use core::iter::{Extend, FromIterator};
use core::marker::PhantomData;
use core::ops::Deref;
use core::str::FromStr;
use core::{cmp, fmt};

use crate::no_std_compat::*;
use crate::{CheckedPathError, Encoding, Iter, Path};

/// An owned, mutable path that mirrors [`std::path::PathBuf`], but operatings using an
/// [`Encoding`] to determine how to parse the underlying bytes.
///
/// This type provides methods like [`push`] and [`set_extension`] that mutate
/// the path in place. It also implements [`Deref`] to [`Path`], meaning that
/// all methods on [`Path`] slices are available on `PathBuf` values as well.
///
/// [`push`]: PathBuf::push
/// [`set_extension`]: PathBuf::set_extension
///
/// # Examples
///
/// You can use [`push`] to build up a `PathBuf` from
/// components:
///
/// ```
/// use typed_path::{PathBuf, WindowsEncoding};
///
/// // NOTE: A pathbuf cannot be created on its own without a defined encoding
/// let mut path = PathBuf::<WindowsEncoding>::new();
///
/// path.push(r"C:\");
/// path.push("windows");
/// path.push("system32");
///
/// path.set_extension("dll");
/// ```
///
/// However, [`push`] is best used for dynamic situations. This is a better way
/// to do this when you know all of the components ahead of time:
///
/// ```
/// use typed_path::{PathBuf, WindowsEncoding};
///
/// let path: PathBuf<WindowsEncoding> = [r"C:\", "windows", "system32.dll"].iter().collect();
/// ```
///
/// We can still do better than this! Since these are all strings, we can use
/// `From::from`:
///
/// ```
/// use typed_path::{PathBuf, WindowsEncoding};
///
/// let path = PathBuf::<WindowsEncoding>::from(br"C:\windows\system32.dll");
/// ```
///
/// Which method works best depends on what kind of situation you're in.
pub struct PathBuf<T>
where
    T: Encoding,
{
    /// Encoding associated with path buf
    pub(crate) _encoding: PhantomData<T>,

    /// Path as an unparsed collection of bytes
    pub(crate) inner: Vec<u8>,
}

impl<T> PathBuf<T>
where
    T: Encoding,
{
    /// Allocates an empty `PathBuf`.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{PathBuf, UnixEncoding};
    ///
    /// // NOTE: A pathbuf cannot be created on its own without a defined encoding
    /// let path = PathBuf::<UnixEncoding>::new();
    /// ```
    pub fn new() -> Self {
        PathBuf {
            inner: Vec::new(),
            _encoding: PhantomData,
        }
    }

    /// Creates a new `PathBuf` with a given capacity used to create the
    /// internal [`Vec<u8>`]. See [`with_capacity`] defined on [`Vec`].
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{PathBuf, UnixEncoding};
    ///
    /// // NOTE: A pathbuf cannot be created on its own without a defined encoding
    /// let mut path = PathBuf::<UnixEncoding>::with_capacity(10);
    /// let capacity = path.capacity();
    ///
    /// // This push is done without reallocating
    /// path.push(r"C:\");
    ///
    /// assert_eq!(capacity, path.capacity());
    /// ```
    ///
    /// [`with_capacity`]: Vec::with_capacity
    #[inline]
    pub fn with_capacity(capacity: usize) -> Self {
        PathBuf {
            inner: Vec::with_capacity(capacity),
            _encoding: PhantomData,
        }
    }

    /// Coerces to a [`Path`] slice.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, PathBuf, UnixEncoding};
    ///
    /// // NOTE: A pathbuf cannot be created on its own without a defined encoding
    /// let p = PathBuf::<UnixEncoding>::from("/test");
    /// assert_eq!(Path::new("/test"), p.as_path());
    /// ```
    #[inline]
    pub fn as_path(&self) -> &Path<T> {
        self
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
    /// # Examples
    ///
    /// Pushing a relative path extends the existing path:
    ///
    /// ```
    /// use typed_path::{PathBuf, UnixEncoding};
    ///
    /// // NOTE: A pathbuf cannot be created on its own without a defined encoding
    /// let mut path = PathBuf::<UnixEncoding>::from("/tmp");
    /// path.push("file.bk");
    /// assert_eq!(path, PathBuf::from("/tmp/file.bk"));
    /// ```
    ///
    /// Pushing an absolute path replaces the existing path:
    ///
    /// ```
    /// use typed_path::{PathBuf, UnixEncoding};
    ///
    /// // NOTE: A pathbuf cannot be created on its own without a defined encoding
    /// let mut path = PathBuf::<UnixEncoding>::from("/tmp");
    /// path.push("/etc");
    /// assert_eq!(path, PathBuf::from("/etc"));
    /// ```
    pub fn push<P: AsRef<Path<T>>>(&mut self, path: P) {
        T::push(&mut self.inner, path.as_ref().as_bytes());
    }

    /// Like [`PathBuf::push`], extends `self` with `path`, but also checks to ensure that `path`
    /// abides by a set of rules.
    ///
    /// # Rules
    ///
    /// 1. `path` cannot contain a prefix component.
    /// 2. `path` cannot contain a root component.
    /// 3. `path` cannot contain invalid filename bytes.
    /// 4. `path` cannot contain parent components such that the current path would be escaped.
    ///
    /// # Examples
    ///
    /// Pushing a relative path extends the existing path:
    ///
    /// ```
    /// use typed_path::{PathBuf, UnixEncoding};
    ///
    /// // NOTE: A pathbuf cannot be created on its own without a defined encoding
    /// let mut path = PathBuf::<UnixEncoding>::from("/tmp");
    ///
    /// // Pushing a relative path works like normal
    /// assert!(path.push_checked("file.bk").is_ok());
    /// assert_eq!(path, PathBuf::from("/tmp/file.bk"));
    /// ```
    ///
    /// Pushing a relative path that contains unresolved parent directory references fails
    /// with an error:
    ///
    /// ```
    /// use typed_path::{CheckedPathError, PathBuf, UnixEncoding};
    ///
    /// // NOTE: A pathbuf cannot be created on its own without a defined encoding
    /// let mut path = PathBuf::<UnixEncoding>::from("/tmp");
    ///
    /// // Pushing a relative path that contains parent directory references that cannot be
    /// // resolved within the path is considered an error as this is considered a path
    /// // traversal attack!
    /// assert_eq!(path.push_checked(".."), Err(CheckedPathError::PathTraversalAttack));
    /// assert_eq!(path, PathBuf::from("/tmp"));
    /// ```
    ///
    /// Pushing an absolute path fails with an error:
    ///
    /// ```
    /// use typed_path::{CheckedPathError, PathBuf, UnixEncoding};
    ///
    /// // NOTE: A pathbuf cannot be created on its own without a defined encoding
    /// let mut path = PathBuf::<UnixEncoding>::from("/tmp");
    ///
    /// // Pushing an absolute path will fail with an error
    /// assert_eq!(path.push_checked("/etc"), Err(CheckedPathError::UnexpectedRoot));
    /// assert_eq!(path, PathBuf::from("/tmp"));
    /// ```
    pub fn push_checked<P: AsRef<Path<T>>>(&mut self, path: P) -> Result<(), CheckedPathError> {
        T::push_checked(&mut self.inner, path.as_ref().as_bytes())
    }

    /// Truncates `self` to [`self.parent`].
    ///
    /// Returns `false` and does nothing if [`self.parent`] is [`None`].
    /// Otherwise, returns `true`.
    ///
    /// [`self.parent`]: Path::parent
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, PathBuf, UnixEncoding};
    ///
    /// // NOTE: A pathbuf cannot be created on its own without a defined encoding
    /// let mut p = PathBuf::<UnixEncoding>::from("/spirited/away.rs");
    ///
    /// p.pop();
    /// assert_eq!(Path::new("/spirited"), p);
    /// p.pop();
    /// assert_eq!(Path::new("/"), p);
    /// ```
    pub fn pop(&mut self) -> bool {
        match self.parent().map(|p| p.as_bytes().len()) {
            Some(len) => {
                self.inner.truncate(len);
                true
            }
            None => false,
        }
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
    /// [`self.file_name`]: Path::file_name
    /// [`pop`]: PathBuf::pop
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{PathBuf, UnixEncoding};
    ///
    /// // NOTE: A pathbuf cannot be created on its own without a defined encoding
    /// let mut buf = PathBuf::<UnixEncoding>::from("/");
    /// assert!(buf.file_name() == None);
    /// buf.set_file_name("bar");
    /// assert!(buf == PathBuf::from("/bar"));
    /// assert!(buf.file_name().is_some());
    /// buf.set_file_name("baz.txt");
    /// assert!(buf == PathBuf::from("/baz.txt"));
    /// ```
    pub fn set_file_name<S: AsRef<[u8]>>(&mut self, file_name: S) {
        self._set_file_name(file_name.as_ref())
    }

    fn _set_file_name(&mut self, file_name: &[u8]) {
        if self.file_name().is_some() {
            let popped = self.pop();
            debug_assert!(popped);
        }
        self.push(file_name);
    }

    /// Updates [`self.extension`] to `extension`.
    ///
    /// Returns `false` and does nothing if [`self.file_name`] is [`None`],
    /// returns `true` and updates the extension otherwise.
    ///
    /// If [`self.extension`] is [`None`], the extension is added; otherwise
    /// it is replaced.
    ///
    /// [`self.file_name`]: Path::file_name
    /// [`self.extension`]: Path::extension
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, PathBuf, UnixEncoding};
    ///
    /// let mut p = PathBuf::<UnixEncoding>::from("/feel/the");
    ///
    /// p.set_extension("force");
    /// assert_eq!(Path::new("/feel/the.force"), p.as_path());
    ///
    /// p.set_extension("dark_side");
    /// assert_eq!(Path::new("/feel/the.dark_side"), p.as_path());
    /// ```
    pub fn set_extension<S: AsRef<[u8]>>(&mut self, extension: S) -> bool {
        self._set_extension(extension.as_ref())
    }

    fn _set_extension(&mut self, extension: &[u8]) -> bool {
        if self.file_stem().is_none() {
            return false;
        }

        let old_ext_len = self.extension().map(|ext| ext.len()).unwrap_or(0);

        // Truncate to remove the extension
        if old_ext_len > 0 {
            self.inner.truncate(self.inner.len() - old_ext_len);

            // If we end with a '.' now from the previous extension, remove that too
            if self.inner.last() == Some(&b'.') {
                self.inner.pop();
            }
        }

        // Add the new extension if it exists
        if !extension.is_empty() {
            // Add a '.' at the end prior to adding the extension
            if self.inner.last() != Some(&b'.') {
                self.inner.push(b'.');
            }

            self.inner.extend_from_slice(extension);
        }

        true
    }

    /// Consumes the `PathBuf`, yielding its internal [`Vec<u8>`] storage.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{PathBuf, UnixEncoding};
    ///
    /// let p = PathBuf::<UnixEncoding>::from("/the/head");
    /// let vec = p.into_vec();
    /// ```
    #[inline]
    pub fn into_vec(self) -> Vec<u8> {
        self.inner
    }

    /// Converts this [`PathBuf`] into a [boxed](Box) [`Path`].
    #[inline]
    pub fn into_boxed_path(self) -> Box<Path<T>> {
        let rw = Box::into_raw(self.inner.into_boxed_slice()) as *mut Path<T>;
        unsafe { Box::from_raw(rw) }
    }

    /// Invokes [`capacity`] on the underlying instance of [`Vec`].
    ///
    /// [`capacity`]: Vec::capacity
    #[inline]
    pub fn capacity(&self) -> usize {
        self.inner.capacity()
    }

    /// Invokes [`clear`] on the underlying instance of [`Vec`].
    ///
    /// [`clear`]: Vec::clear
    #[inline]
    pub fn clear(&mut self) {
        self.inner.clear()
    }

    /// Invokes [`reserve`] on the underlying instance of [`Vec`].
    ///
    /// [`reserve`]: Vec::reserve
    #[inline]
    pub fn reserve(&mut self, additional: usize) {
        self.inner.reserve(additional)
    }

    /// Invokes [`try_reserve`] on the underlying instance of [`Vec`].
    ///
    /// [`try_reserve`]: Vec::try_reserve
    #[inline]
    pub fn try_reserve(&mut self, additional: usize) -> Result<(), TryReserveError> {
        self.inner.try_reserve(additional)
    }

    /// Invokes [`reserve_exact`] on the underlying instance of [`Vec`].
    ///
    /// [`reserve_exact`]: Vec::reserve_exact
    #[inline]
    pub fn reserve_exact(&mut self, additional: usize) {
        self.inner.reserve_exact(additional)
    }

    /// Invokes [`try_reserve_exact`] on the underlying instance of [`Vec`].
    ///
    /// [`try_reserve_exact`]: Vec::try_reserve_exact
    #[inline]
    pub fn try_reserve_exact(&mut self, additional: usize) -> Result<(), TryReserveError> {
        self.inner.try_reserve_exact(additional)
    }

    /// Invokes [`shrink_to_fit`] on the underlying instance of [`Vec`].
    ///
    /// [`shrink_to_fit`]: Vec::shrink_to_fit
    #[inline]
    pub fn shrink_to_fit(&mut self) {
        self.inner.shrink_to_fit()
    }

    /// Invokes [`shrink_to`] on the underlying instance of [`Vec`].
    ///
    /// [`shrink_to`]: Vec::shrink_to
    #[inline]
    pub fn shrink_to(&mut self, min_capacity: usize) {
        self.inner.shrink_to(min_capacity)
    }
}

impl<T> Clone for PathBuf<T>
where
    T: Encoding,
{
    #[inline]
    fn clone(&self) -> Self {
        Self {
            _encoding: self._encoding,
            inner: self.inner.clone(),
        }
    }
}

impl<T> fmt::Debug for PathBuf<T>
where
    T: Encoding,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("PathBuf")
            .field("_encoding", &T::label())
            .field("inner", &self.inner)
            .finish()
    }
}

impl<T> AsRef<[u8]> for PathBuf<T>
where
    T: Encoding,
{
    #[inline]
    fn as_ref(&self) -> &[u8] {
        self.as_bytes()
    }
}

impl<T> AsRef<Path<T>> for PathBuf<T>
where
    T: Encoding,
{
    #[inline]
    fn as_ref(&self) -> &Path<T> {
        self
    }
}

impl<T> Borrow<Path<T>> for PathBuf<T>
where
    T: Encoding,
{
    #[inline]
    fn borrow(&self) -> &Path<T> {
        self.deref()
    }
}

impl<T> Default for PathBuf<T>
where
    T: Encoding,
{
    #[inline]
    fn default() -> PathBuf<T> {
        PathBuf::new()
    }
}

impl<T> Deref for PathBuf<T>
where
    T: Encoding,
{
    type Target = Path<T>;

    #[inline]
    fn deref(&self) -> &Path<T> {
        Path::new(&self.inner)
    }
}

impl<T> Eq for PathBuf<T> where T: Encoding {}

impl<T> PartialEq for PathBuf<T>
where
    T: Encoding,
{
    fn eq(&self, other: &Self) -> bool {
        self.components() == other.components()
    }
}

impl<T, P> Extend<P> for PathBuf<T>
where
    T: Encoding,
    P: AsRef<Path<T>>,
{
    fn extend<I: IntoIterator<Item = P>>(&mut self, iter: I) {
        iter.into_iter().for_each(move |p| self.push(p.as_ref()));
    }
}

impl<T> From<Box<Path<T>>> for PathBuf<T>
where
    T: Encoding,
{
    fn from(boxed: Box<Path<T>>) -> Self {
        boxed.into_path_buf()
    }
}

impl<T, V> From<&V> for PathBuf<T>
where
    T: Encoding,
    V: ?Sized + AsRef<[u8]>,
{
    /// Converts a borrowed [`[u8]`] to a [`PathBuf`].
    ///
    /// Allocates a [`PathBuf`] and copies the data into it.
    #[inline]
    fn from(s: &V) -> Self {
        PathBuf::from(s.as_ref().to_vec())
    }
}

impl<T> From<Vec<u8>> for PathBuf<T>
where
    T: Encoding,
{
    /// Converts a [`Vec<u8>`] into a [`PathBuf`]
    ///
    /// This conversion does not allocate or copy memory.
    #[inline]
    fn from(inner: Vec<u8>) -> Self {
        PathBuf {
            _encoding: PhantomData,
            inner,
        }
    }
}

impl<T> From<PathBuf<T>> for Vec<u8>
where
    T: Encoding,
{
    /// Converts a [`PathBuf`] into a [`Vec<u8>`]
    ///
    /// This conversion does not allocate or copy memory.
    #[inline]
    fn from(path_buf: PathBuf<T>) -> Self {
        path_buf.inner
    }
}

impl<T> From<String> for PathBuf<T>
where
    T: Encoding,
{
    /// Converts a [`String`] into a [`PathBuf`]
    ///
    /// This conversion does not allocate or copy memory.
    #[inline]
    fn from(s: String) -> Self {
        PathBuf::from(s.into_bytes())
    }
}

impl<T> FromStr for PathBuf<T>
where
    T: Encoding,
{
    type Err = core::convert::Infallible;

    #[inline]
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Ok(PathBuf::from(s))
    }
}

impl<'a, T> From<Cow<'a, Path<T>>> for PathBuf<T>
where
    T: Encoding,
{
    /// Converts a clone-on-write pointer to an owned path.
    ///
    /// Converting from a `Cow::Owned` does not clone or allocate.
    #[inline]
    fn from(p: Cow<'a, Path<T>>) -> Self {
        p.into_owned()
    }
}

impl<T, P> FromIterator<P> for PathBuf<T>
where
    T: Encoding,
    P: AsRef<Path<T>>,
{
    fn from_iter<I: IntoIterator<Item = P>>(iter: I) -> Self {
        let mut buf = PathBuf::new();
        buf.extend(iter);
        buf
    }
}

impl<T> Hash for PathBuf<T>
where
    T: Encoding,
{
    fn hash<H: Hasher>(&self, h: &mut H) {
        self.as_path().hash(h)
    }
}

impl<'a, T> IntoIterator for &'a PathBuf<T>
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

impl<T> cmp::PartialOrd for PathBuf<T>
where
    T: Encoding,
{
    #[inline]
    fn partial_cmp(&self, other: &Self) -> Option<cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl<T> cmp::Ord for PathBuf<T>
where
    T: Encoding,
{
    #[inline]
    fn cmp(&self, other: &Self) -> cmp::Ordering {
        self.components().cmp(other.components())
    }
}

#[cfg(feature = "std")]
impl<T> TryFrom<PathBuf<T>> for std::path::PathBuf
where
    T: Encoding,
{
    type Error = PathBuf<T>;

    /// Attempts to convert a [`PathBuf`] into a [`std::path::PathBuf`], returning a result
    /// containing the new path when successful or the original path when failed
    ///
    /// # Examples
    ///
    /// ```
    /// use std::convert::TryFrom;
    /// use std::path::PathBuf;
    /// use typed_path::UnixPathBuf;
    ///
    /// let unix_path_buf = UnixPathBuf::from("/path/to/file.txt");
    /// let std_path_buf: PathBuf = TryFrom::try_from(unix_path_buf).unwrap();
    /// ```
    fn try_from(path: PathBuf<T>) -> Result<Self, Self::Error> {
        match std::str::from_utf8(path.as_bytes()) {
            Ok(s) => Ok(std::path::PathBuf::from(s)),
            Err(_) => Err(path),
        }
    }
}

#[cfg(feature = "std")]
impl<T> TryFrom<std::path::PathBuf> for PathBuf<T>
where
    T: Encoding,
{
    type Error = std::path::PathBuf;

    /// Attempts to convert a [`std::path::PathBuf`] into a [`PathBuf`], returning a result
    /// containing the new path when successful or the original path when failed
    ///
    /// # Examples
    ///
    /// ```
    /// use std::convert::TryFrom;
    /// use std::path::PathBuf;
    /// use typed_path::UnixPathBuf;
    ///
    /// let std_path_buf = PathBuf::from("/path/to/file.txt");
    /// let unix_path_buf: UnixPathBuf = TryFrom::try_from(std_path_buf).unwrap();
    /// ```
    fn try_from(path: std::path::PathBuf) -> Result<Self, Self::Error> {
        match path.to_str() {
            Some(s) => Ok(PathBuf::from(s)),
            None => Err(path),
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

    use os::ffi::{OsStrExt, OsStringExt};

    use super::*;

    impl<T> From<PathBuf<T>> for OsString
    where
        T: Encoding,
    {
        #[inline]
        fn from(path_buf: PathBuf<T>) -> Self {
            OsStringExt::from_vec(path_buf.into_vec())
        }
    }

    impl<T> AsRef<OsStr> for PathBuf<T>
    where
        T: Encoding,
    {
        #[inline]
        fn as_ref(&self) -> &OsStr {
            OsStrExt::from_bytes(self.as_bytes())
        }
    }
}
