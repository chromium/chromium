use alloc::borrow::Cow;
use alloc::collections::TryReserveError;
use alloc::string::FromUtf8Error;
use core::borrow::Borrow;
use core::hash::{Hash, Hasher};
use core::iter::{Extend, FromIterator};
use core::marker::PhantomData;
use core::ops::Deref;
use core::str::FromStr;
use core::{cmp, fmt};

use crate::no_std_compat::*;
use crate::{CheckedPathError, Encoding, PathBuf, Utf8Encoding, Utf8Iter, Utf8Path};

/// An owned, mutable path that mirrors [`std::path::PathBuf`], but operatings using a
/// [`Utf8Encoding`] to determine how to parse the underlying str.
///
/// This type provides methods like [`push`] and [`set_extension`] that mutate
/// the path in place. It also implements [`Deref`] to [`Utf8Path`], meaning that
/// all methods on [`Utf8Path`] slices are available on `Utf8PathBuf` values as well.
///
/// [`push`]: Utf8PathBuf::push
/// [`set_extension`]: Utf8PathBuf::set_extension
///
/// # Examples
///
/// You can use [`push`] to build up a `Utf8PathBuf` from
/// components:
///
/// ```
/// use typed_path::{Utf8PathBuf, Utf8WindowsEncoding};
///
/// // NOTE: A pathbuf cannot be created on its own without a defined encoding
/// let mut path = Utf8PathBuf::<Utf8WindowsEncoding>::new();
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
/// use typed_path::{Utf8PathBuf, Utf8WindowsEncoding};
///
/// let path: Utf8PathBuf<Utf8WindowsEncoding> = [
///     r"C:\",
///     "windows",
///     "system32.dll",
/// ].iter().collect();
/// ```
///
/// We can still do better than this! Since these are all strings, we can use
/// `From::from`:
///
/// ```
/// use typed_path::{Utf8PathBuf, Utf8WindowsEncoding};
///
/// let path = Utf8PathBuf::<Utf8WindowsEncoding>::from(r"C:\windows\system32.dll");
/// ```
///
/// Which method works best depends on what kind of situation you're in.
pub struct Utf8PathBuf<T>
where
    T: Utf8Encoding,
{
    /// Encoding associated with path buf
    pub(crate) _encoding: PhantomData<T>,

    /// Path as an unparsed string
    pub(crate) inner: String,
}

impl<T> Utf8PathBuf<T>
where
    T: Utf8Encoding,
{
    /// Allocates an empty `Utf8PathBuf`.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8PathBuf, Utf8UnixEncoding};
    ///
    /// // NOTE: A pathbuf cannot be created on its own without a defined encoding
    /// let path = Utf8PathBuf::<Utf8UnixEncoding>::new();
    /// ```
    pub fn new() -> Self {
        Utf8PathBuf {
            _encoding: PhantomData,
            inner: String::new(),
        }
    }

    /// Creates a new `PathBuf` with a given capacity used to create the
    /// internal [`String`]. See [`with_capacity`] defined on [`String`].
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8PathBuf, Utf8UnixEncoding};
    ///
    /// // NOTE: A pathbuf cannot be created on its own without a defined encoding
    /// let mut path = Utf8PathBuf::<Utf8UnixEncoding>::with_capacity(10);
    /// let capacity = path.capacity();
    ///
    /// // This push is done without reallocating
    /// path.push(r"C:\");
    ///
    /// assert_eq!(capacity, path.capacity());
    /// ```
    ///
    /// [`with_capacity`]: String::with_capacity
    #[inline]
    pub fn with_capacity(capacity: usize) -> Self {
        Utf8PathBuf {
            _encoding: PhantomData,
            inner: String::with_capacity(capacity),
        }
    }

    /// Coerces to a [`Utf8Path`] slice.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8PathBuf, Utf8UnixEncoding};
    ///
    /// // NOTE: A pathbuf cannot be created on its own without a defined encoding
    /// let p = Utf8PathBuf::<Utf8UnixEncoding>::from("/test");
    /// assert_eq!(Utf8Path::new("/test"), p.as_path());
    /// ```
    #[inline]
    pub fn as_path(&self) -> &Utf8Path<T> {
        self
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
    /// # Examples
    ///
    /// Pushing a relative path extends the existing path:
    ///
    /// ```
    /// use typed_path::{Utf8PathBuf, Utf8UnixEncoding};
    ///
    /// // NOTE: A pathbuf cannot be created on its own without a defined encoding
    /// let mut path = Utf8PathBuf::<Utf8UnixEncoding>::from("/tmp");
    /// path.push("file.bk");
    /// assert_eq!(path, Utf8PathBuf::from("/tmp/file.bk"));
    /// ```
    ///
    /// Pushing an absolute path replaces the existing path:
    ///
    /// ```
    /// use typed_path::{Utf8PathBuf, Utf8UnixEncoding};
    ///
    /// // NOTE: A pathbuf cannot be created on its own without a defined encoding
    /// let mut path = Utf8PathBuf::<Utf8UnixEncoding>::from("/tmp");
    /// path.push("/etc");
    /// assert_eq!(path, Utf8PathBuf::from("/etc"));
    /// ```
    pub fn push<P: AsRef<Utf8Path<T>>>(&mut self, path: P) {
        T::push(&mut self.inner, path.as_ref().as_str());
    }

    /// Like [`Utf8PathBuf::push`], extends `self` with `path`, but also checks to ensure that
    /// `path` abides by a set of rules.
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
    /// use typed_path::{Utf8PathBuf, Utf8UnixEncoding};
    ///
    /// // NOTE: A pathbuf cannot be created on its own without a defined encoding
    /// let mut path = Utf8PathBuf::<Utf8UnixEncoding>::from("/tmp");
    ///
    /// // Pushing a relative path works like normal
    /// assert!(path.push_checked("file.bk").is_ok());
    /// assert_eq!(path, Utf8PathBuf::from("/tmp/file.bk"));
    /// ```
    ///
    /// Pushing a relative path that contains unresolved parent directory references fails
    /// with an error:
    ///
    /// ```
    /// use typed_path::{CheckedPathError, Utf8PathBuf, Utf8UnixEncoding};
    ///
    /// // NOTE: A pathbuf cannot be created on its own without a defined encoding
    /// let mut path = Utf8PathBuf::<Utf8UnixEncoding>::from("/tmp");
    ///
    /// // Pushing a relative path that contains parent directory references that cannot be
    /// // resolved within the path is considered an error as this is considered a path
    /// // traversal attack!
    /// assert_eq!(path.push_checked(".."), Err(CheckedPathError::PathTraversalAttack));
    /// assert_eq!(path, Utf8PathBuf::from("/tmp"));
    /// ```
    ///
    /// Pushing an absolute path fails with an error:
    ///
    /// ```
    /// use typed_path::{CheckedPathError, Utf8PathBuf, Utf8UnixEncoding};
    ///
    /// // NOTE: A pathbuf cannot be created on its own without a defined encoding
    /// let mut path = Utf8PathBuf::<Utf8UnixEncoding>::from("/tmp");
    ///
    /// // Pushing an absolute path will fail with an error
    /// assert_eq!(path.push_checked("/etc"), Err(CheckedPathError::UnexpectedRoot));
    /// assert_eq!(path, Utf8PathBuf::from("/tmp"));
    /// ```
    pub fn push_checked<P: AsRef<Utf8Path<T>>>(&mut self, path: P) -> Result<(), CheckedPathError> {
        T::push_checked(&mut self.inner, path.as_ref().as_str())
    }

    /// Truncates `self` to [`self.parent`].
    ///
    /// Returns `false` and does nothing if [`self.parent`] is [`None`].
    /// Otherwise, returns `true`.
    ///
    /// [`self.parent`]: Utf8Path::parent
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8PathBuf, Utf8UnixEncoding};
    ///
    /// // NOTE: A pathbuf cannot be created on its own without a defined encoding
    /// let mut p = Utf8PathBuf::<Utf8UnixEncoding>::from("/spirited/away.rs");
    ///
    /// p.pop();
    /// assert_eq!(Utf8Path::new("/spirited"), p);
    /// p.pop();
    /// assert_eq!(Utf8Path::new("/"), p);
    /// ```
    pub fn pop(&mut self) -> bool {
        match self.parent().map(|p| p.as_str().len()) {
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
    /// [`self.file_name`]: Utf8Path::file_name
    /// [`pop`]: Utf8PathBuf::pop
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8PathBuf, Utf8UnixEncoding};
    ///
    /// // NOTE: A pathbuf cannot be created on its own without a defined encoding
    /// let mut buf = Utf8PathBuf::<Utf8UnixEncoding>::from("/");
    /// assert!(buf.file_name() == None);
    /// buf.set_file_name("bar");
    /// assert!(buf == Utf8PathBuf::from("/bar"));
    /// assert!(buf.file_name().is_some());
    /// buf.set_file_name("baz.txt");
    /// assert!(buf == Utf8PathBuf::from("/baz.txt"));
    /// ```
    pub fn set_file_name<S: AsRef<str>>(&mut self, file_name: S) {
        self._set_file_name(file_name.as_ref())
    }

    fn _set_file_name(&mut self, file_name: &str) {
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
    /// [`self.file_name`]: Utf8Path::file_name
    /// [`self.extension`]: Utf8Path::extension
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8PathBuf, Utf8UnixEncoding};
    ///
    /// let mut p = Utf8PathBuf::<Utf8UnixEncoding>::from("/feel/the");
    ///
    /// p.set_extension("force");
    /// assert_eq!(Utf8Path::new("/feel/the.force"), p.as_path());
    ///
    /// p.set_extension("dark_side");
    /// assert_eq!(Utf8Path::new("/feel/the.dark_side"), p.as_path());
    /// ```
    pub fn set_extension<S: AsRef<str>>(&mut self, extension: S) -> bool {
        self._set_extension(extension.as_ref())
    }

    fn _set_extension(&mut self, extension: &str) -> bool {
        if self.file_stem().is_none() {
            return false;
        }

        let old_ext_len = self.extension().map(|ext| ext.len()).unwrap_or(0);

        // Truncate to remove the extension
        if old_ext_len > 0 {
            self.inner.truncate(self.inner.len() - old_ext_len);

            // If we end with a '.' now from the previous extension, remove that too
            if self.inner.ends_with('.') {
                self.inner.pop();
            }
        }

        // Add the new extension if it exists
        if !extension.is_empty() {
            // Add a '.' at the end prior to adding the extension
            if !self.inner.ends_with('.') {
                self.inner.push('.');
            }

            self.inner.push_str(extension);
        }

        true
    }

    /// Consumes the `PathBuf`, yielding its internal [`String`] storage.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8PathBuf, Utf8UnixEncoding};
    ///
    /// let p = Utf8PathBuf::<Utf8UnixEncoding>::from("/the/head");
    /// let s = p.into_string();
    /// ```
    #[inline]
    pub fn into_string(self) -> String {
        self.inner
    }

    /// Converts this [`Utf8PathBuf`] into a [boxed](Box) [`Utf8Path`].
    #[inline]
    pub fn into_boxed_path(self) -> Box<Utf8Path<T>> {
        let rw = Box::into_raw(self.inner.into_boxed_str()) as *mut Utf8Path<T>;
        unsafe { Box::from_raw(rw) }
    }

    /// Invokes [`capacity`] on the underlying instance of [`String`].
    ///
    /// [`capacity`]: String::capacity
    #[inline]
    pub fn capacity(&self) -> usize {
        self.inner.capacity()
    }

    /// Invokes [`clear`] on the underlying instance of [`String`].
    ///
    /// [`clear`]: String::clear
    #[inline]
    pub fn clear(&mut self) {
        self.inner.clear()
    }

    /// Invokes [`reserve`] on the underlying instance of [`String`].
    ///
    /// [`reserve`]: String::reserve
    #[inline]
    pub fn reserve(&mut self, additional: usize) {
        self.inner.reserve(additional)
    }

    /// Invokes [`try_reserve`] on the underlying instance of [`String`].
    ///
    /// [`try_reserve`]: String::try_reserve
    #[inline]
    pub fn try_reserve(&mut self, additional: usize) -> Result<(), TryReserveError> {
        self.inner.try_reserve(additional)
    }

    /// Invokes [`reserve_exact`] on the underlying instance of [`String`].
    ///
    /// [`reserve_exact`]: String::reserve_exact
    #[inline]
    pub fn reserve_exact(&mut self, additional: usize) {
        self.inner.reserve_exact(additional)
    }

    /// Invokes [`try_reserve_exact`] on the underlying instance of [`String`].
    ///
    /// [`try_reserve_exact`]: String::try_reserve_exact
    #[inline]
    pub fn try_reserve_exact(&mut self, additional: usize) -> Result<(), TryReserveError> {
        self.inner.try_reserve_exact(additional)
    }

    /// Invokes [`shrink_to_fit`] on the underlying instance of [`String`].
    ///
    /// [`shrink_to_fit`]: String::shrink_to_fit
    #[inline]
    pub fn shrink_to_fit(&mut self) {
        self.inner.shrink_to_fit()
    }

    /// Invokes [`shrink_to`] on the underlying instance of [`String`].
    ///
    /// [`shrink_to`]: String::shrink_to
    #[inline]
    pub fn shrink_to(&mut self, min_capacity: usize) {
        self.inner.shrink_to(min_capacity)
    }

    /// Consumes [`PathBuf`] and returns a new [`Utf8PathBuf`] by checking that the path contains
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
    /// use typed_path::{PathBuf, Utf8PathBuf, UnixEncoding, Utf8UnixEncoding};
    ///
    /// let path_buf = PathBuf::<UnixEncoding>::from(&[0xf0, 0x9f, 0x92, 0x96]);
    /// let utf8_path_buf = Utf8PathBuf::<Utf8UnixEncoding>::from_bytes_path_buf(path_buf).unwrap();
    /// assert_eq!(utf8_path_buf.as_str(), "💖");
    /// ```
    pub fn from_bytes_path_buf<U>(path_buf: PathBuf<U>) -> Result<Self, FromUtf8Error>
    where
        U: Encoding,
    {
        Ok(Self {
            _encoding: PhantomData,
            inner: String::from_utf8(path_buf.inner)?,
        })
    }

    /// Consumes [`PathBuf`] and returns a new [`Utf8PathBuf`] by checking that the path contains
    /// valid UTF-8.
    ///
    /// # Errors
    ///
    /// Returns `Err` if the path is not UTF-8 with a description as to why the
    /// provided component is not UTF-8.
    ///
    /// # Safety
    ///
    /// The path passed in must be valid UTF-8.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{PathBuf, Utf8PathBuf, UnixEncoding, Utf8UnixEncoding};
    ///
    /// let path_buf = PathBuf::<UnixEncoding>::from(&[0xf0, 0x9f, 0x92, 0x96]);
    /// let utf8_path_buf = unsafe {
    ///     Utf8PathBuf::<Utf8UnixEncoding>::from_bytes_path_buf_unchecked(path_buf)
    /// };
    /// assert_eq!(utf8_path_buf.as_str(), "💖");
    /// ```
    pub unsafe fn from_bytes_path_buf_unchecked<U>(path_buf: PathBuf<U>) -> Self
    where
        U: Encoding,
    {
        Self {
            _encoding: PhantomData,
            inner: String::from_utf8_unchecked(path_buf.inner),
        }
    }

    /// Consumes [`Utf8PathBuf`] and returns a new [`PathBuf`]
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{PathBuf, Utf8PathBuf, UnixEncoding, Utf8UnixEncoding};
    ///
    /// let utf8_path_buf = Utf8PathBuf::<Utf8UnixEncoding>::from("💖");
    /// let path_buf = utf8_path_buf.into_bytes_path_buf::<UnixEncoding>();
    /// assert_eq!(path_buf.as_bytes(), &[0xf0, 0x9f, 0x92, 0x96]);
    /// ```
    pub fn into_bytes_path_buf<U>(self) -> PathBuf<U>
    where
        U: Encoding,
    {
        PathBuf {
            _encoding: PhantomData,
            inner: self.inner.into_bytes(),
        }
    }
}

impl<T> Clone for Utf8PathBuf<T>
where
    T: Utf8Encoding,
{
    #[inline]
    fn clone(&self) -> Self {
        Self {
            _encoding: self._encoding,
            inner: self.inner.clone(),
        }
    }
}

impl<T> fmt::Debug for Utf8PathBuf<T>
where
    T: Utf8Encoding,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Utf8PathBuf")
            .field("_encoding", &T::label())
            .field("inner", &self.inner)
            .finish()
    }
}

impl<T> AsRef<[u8]> for Utf8PathBuf<T>
where
    T: Utf8Encoding,
{
    #[inline]
    fn as_ref(&self) -> &[u8] {
        self.as_str().as_bytes()
    }
}

impl<T> AsRef<str> for Utf8PathBuf<T>
where
    T: Utf8Encoding,
{
    #[inline]
    fn as_ref(&self) -> &str {
        self.as_str()
    }
}

impl<T> AsRef<Utf8Path<T>> for Utf8PathBuf<T>
where
    T: Utf8Encoding,
{
    #[inline]
    fn as_ref(&self) -> &Utf8Path<T> {
        self
    }
}

impl<T> Borrow<Utf8Path<T>> for Utf8PathBuf<T>
where
    T: Utf8Encoding,
{
    #[inline]
    fn borrow(&self) -> &Utf8Path<T> {
        self.deref()
    }
}

impl<T> Default for Utf8PathBuf<T>
where
    T: Utf8Encoding,
{
    #[inline]
    fn default() -> Utf8PathBuf<T> {
        Utf8PathBuf::new()
    }
}

impl<T> fmt::Display for Utf8PathBuf<T>
where
    T: Utf8Encoding,
{
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(&self.inner, f)
    }
}

impl<T> Deref for Utf8PathBuf<T>
where
    T: Utf8Encoding,
{
    type Target = Utf8Path<T>;

    #[inline]
    fn deref(&self) -> &Utf8Path<T> {
        Utf8Path::new(&self.inner)
    }
}

impl<T> Eq for Utf8PathBuf<T> where T: Utf8Encoding {}

impl<T> PartialEq for Utf8PathBuf<T>
where
    T: Utf8Encoding,
{
    fn eq(&self, other: &Self) -> bool {
        self.components() == other.components()
    }
}

impl<T, P> Extend<P> for Utf8PathBuf<T>
where
    T: Utf8Encoding,
    P: AsRef<Utf8Path<T>>,
{
    fn extend<I: IntoIterator<Item = P>>(&mut self, iter: I) {
        iter.into_iter().for_each(move |p| self.push(p.as_ref()));
    }
}

impl<T> From<Box<Utf8Path<T>>> for Utf8PathBuf<T>
where
    T: Utf8Encoding,
{
    fn from(boxed: Box<Utf8Path<T>>) -> Self {
        boxed.into_path_buf()
    }
}

impl<T, V> From<&V> for Utf8PathBuf<T>
where
    T: Utf8Encoding,
    V: ?Sized + AsRef<str>,
{
    /// Converts a borrowed [`str`] to a [`Utf8PathBuf`].
    ///
    /// Allocates a [`Utf8PathBuf`] and copies the data into it.
    #[inline]
    fn from(s: &V) -> Self {
        Utf8PathBuf::from(s.as_ref().to_string())
    }
}

impl<T> From<String> for Utf8PathBuf<T>
where
    T: Utf8Encoding,
{
    /// Converts a [`String`] into a [`Utf8PathBuf`]
    ///
    /// This conversion does not allocate or copy memory.
    #[inline]
    fn from(inner: String) -> Self {
        Utf8PathBuf {
            _encoding: PhantomData,
            inner,
        }
    }
}

impl<T> From<Utf8PathBuf<T>> for String
where
    T: Utf8Encoding,
{
    /// Converts a [`Utf8PathBuf`] into a [`String`]
    ///
    /// This conversion does not allocate or copy memory.
    #[inline]
    fn from(path_buf: Utf8PathBuf<T>) -> Self {
        path_buf.inner
    }
}

impl<T> FromStr for Utf8PathBuf<T>
where
    T: Utf8Encoding,
{
    type Err = core::convert::Infallible;

    #[inline]
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Ok(Utf8PathBuf::from(s))
    }
}

impl<'a, T> From<Cow<'a, Utf8Path<T>>> for Utf8PathBuf<T>
where
    T: Utf8Encoding,
{
    /// Converts a clone-on-write pointer to an owned path.
    ///
    /// Converting from a `Cow::Owned` does not clone or allocate.
    #[inline]
    fn from(p: Cow<'a, Utf8Path<T>>) -> Self {
        p.into_owned()
    }
}

impl<T, P> FromIterator<P> for Utf8PathBuf<T>
where
    T: Utf8Encoding,
    P: AsRef<Utf8Path<T>>,
{
    fn from_iter<I: IntoIterator<Item = P>>(iter: I) -> Self {
        let mut buf = Utf8PathBuf::new();
        buf.extend(iter);
        buf
    }
}

impl<T> Hash for Utf8PathBuf<T>
where
    T: Utf8Encoding,
{
    fn hash<H: Hasher>(&self, h: &mut H) {
        self.as_path().hash(h)
    }
}

impl<'a, T> IntoIterator for &'a Utf8PathBuf<T>
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

impl<T> cmp::PartialOrd for Utf8PathBuf<T>
where
    T: Utf8Encoding,
{
    #[inline]
    fn partial_cmp(&self, other: &Self) -> Option<cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl<T> cmp::Ord for Utf8PathBuf<T>
where
    T: Utf8Encoding,
{
    #[inline]
    fn cmp(&self, other: &Self) -> cmp::Ordering {
        self.components().cmp(other.components())
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

    impl<T> From<Utf8PathBuf<T>> for OsString
    where
        T: Utf8Encoding,
    {
        #[inline]
        fn from(path_buf: Utf8PathBuf<T>) -> Self {
            OsStringExt::from_vec(path_buf.into_string().into_bytes())
        }
    }

    impl<T> AsRef<OsStr> for Utf8PathBuf<T>
    where
        T: Utf8Encoding,
    {
        #[inline]
        fn as_ref(&self) -> &OsStr {
            OsStrExt::from_bytes(self.as_str().as_bytes())
        }
    }
}
