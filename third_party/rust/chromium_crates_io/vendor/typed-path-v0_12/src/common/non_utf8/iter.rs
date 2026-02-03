use core::fmt;
use core::iter::FusedIterator;
use core::marker::PhantomData;

use crate::{Component, Components, Encoding, Path};

/// An iterator over the [`Component`]s of a [`Path`], as [`[u8]`] slices.
///
/// This `struct` is created by the [`iter`] method on [`Path`].
/// See its documentation for more.
///
/// [`iter`]: Path::iter
#[derive(Clone)]
pub struct Iter<'a, T>
where
    T: Encoding,
{
    _encoding: PhantomData<T>,
    inner: <T as Encoding>::Components<'a>,
}

impl<'a, T> Iter<'a, T>
where
    T: Encoding,
{
    pub(crate) fn new(inner: <T as Encoding>::Components<'a>) -> Self {
        Self {
            _encoding: PhantomData,
            inner,
        }
    }

    /// Extracts a slice corresponding to the portion of the path remaining for iteration.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let mut iter = Path::<UnixEncoding>::new("/tmp/foo/bar.txt").iter();
    /// iter.next();
    /// iter.next();
    ///
    /// assert_eq!(Path::<UnixEncoding>::new("foo/bar.txt"), iter.as_path());
    /// ```
    pub fn as_path(&self) -> &Path<T> {
        Path::new(self.inner.as_bytes())
    }
}

impl<'a, T> fmt::Debug for Iter<'a, T>
where
    T: for<'enc> Encoding + 'a,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        struct DebugHelper<'a, T>(&'a Path<T>)
        where
            T: for<'enc> Encoding;

        impl<T> fmt::Debug for DebugHelper<'_, T>
        where
            T: for<'enc> Encoding,
        {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                f.debug_list().entries(self.0.iter()).finish()
            }
        }

        f.debug_tuple(stringify!(Iter))
            .field(&DebugHelper(self.as_path()))
            .finish()
    }
}

impl<'a, T> AsRef<Path<T>> for Iter<'a, T>
where
    T: for<'enc> Encoding + 'a,
{
    #[inline]
    fn as_ref(&self) -> &Path<T> {
        self.as_path()
    }
}

impl<'a, T> AsRef<[u8]> for Iter<'a, T>
where
    T: for<'enc> Encoding + 'a,
{
    #[inline]
    fn as_ref(&self) -> &[u8] {
        self.as_path().as_bytes()
    }
}

impl<'a, T> Iterator for Iter<'a, T>
where
    T: for<'enc> Encoding + 'a,
{
    type Item = &'a [u8];

    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        match self.inner.next() {
            Some(c) => Some(c.as_bytes()),
            None => None,
        }
    }
}

impl<'a, T> DoubleEndedIterator for Iter<'a, T>
where
    T: for<'enc> Encoding + 'a,
{
    #[inline]
    fn next_back(&mut self) -> Option<Self::Item> {
        match self.inner.next_back() {
            Some(c) => Some(c.as_bytes()),
            None => None,
        }
    }
}

impl<'a, T> FusedIterator for Iter<'a, T> where T: for<'enc> Encoding + 'a {}

/// An iterator over [`Path`] and its ancestors.
///
/// This `struct` is created by the [`ancestors`] method on [`Path`].
/// See its documentation for more.
///
/// # Examples
///
/// ```
/// use typed_path::{Path, UnixEncoding};
///
/// // NOTE: A path cannot be created on its own without a defined encoding
/// let path = Path::<UnixEncoding>::new("/foo/bar");
///
/// for ancestor in path.ancestors() {
///     println!("{}", ancestor.display());
/// }
/// ```
///
/// [`ancestors`]: Path::ancestors
#[derive(Copy, Clone, Debug)]
pub struct Ancestors<'a, T>
where
    T: for<'enc> Encoding,
{
    pub(crate) next: Option<&'a Path<T>>,
}

impl<'a, T> Iterator for Ancestors<'a, T>
where
    T: for<'enc> Encoding,
{
    type Item = &'a Path<T>;

    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        let next = self.next;
        self.next = next.and_then(Path::parent);
        next
    }
}

impl<T> FusedIterator for Ancestors<'_, T> where T: for<'enc> Encoding {}
