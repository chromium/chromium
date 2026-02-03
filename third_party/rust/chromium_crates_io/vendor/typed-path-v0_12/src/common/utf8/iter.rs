use core::fmt;
use core::iter::FusedIterator;
use core::marker::PhantomData;

use crate::{Utf8Component, Utf8Components, Utf8Encoding, Utf8Path};

/// An iterator over the [`Utf8Component`]s of a [`Utf8Path`], as [`str`] slices.
///
/// This `struct` is created by the [`iter`] method on [`Utf8Path`].
/// See its documentation for more.
///
/// [`iter`]: Utf8Path::iter
#[derive(Clone)]
pub struct Utf8Iter<'a, T>
where
    T: Utf8Encoding,
{
    _encoding: PhantomData<T>,
    inner: <T as Utf8Encoding>::Components<'a>,
}

impl<'a, T> Utf8Iter<'a, T>
where
    T: Utf8Encoding + 'a,
{
    pub(crate) fn new(inner: <T as Utf8Encoding>::Components<'a>) -> Self {
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
    /// use typed_path::{Utf8Path, Utf8UnixEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let mut iter = Utf8Path::<Utf8UnixEncoding>::new("/tmp/foo/bar.txt").iter();
    /// iter.next();
    /// iter.next();
    ///
    /// assert_eq!(Utf8Path::<Utf8UnixEncoding>::new("foo/bar.txt"), iter.as_path());
    /// ```
    pub fn as_path(&self) -> &Utf8Path<T> {
        Utf8Path::new(self.inner.as_str())
    }
}

impl<'a, T> fmt::Debug for Utf8Iter<'a, T>
where
    T: Utf8Encoding + 'a,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        struct DebugHelper<'a, T>(&'a Utf8Path<T>)
        where
            T: Utf8Encoding;

        impl<T> fmt::Debug for DebugHelper<'_, T>
        where
            T: Utf8Encoding,
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

impl<'a, T> AsRef<Utf8Path<T>> for Utf8Iter<'a, T>
where
    T: Utf8Encoding + 'a,
{
    #[inline]
    fn as_ref(&self) -> &Utf8Path<T> {
        self.as_path()
    }
}

impl<'a, T> AsRef<[u8]> for Utf8Iter<'a, T>
where
    T: Utf8Encoding + 'a,
{
    #[inline]
    fn as_ref(&self) -> &[u8] {
        self.as_path().as_str().as_bytes()
    }
}

impl<'a, T> AsRef<str> for Utf8Iter<'a, T>
where
    T: Utf8Encoding + 'a,
{
    #[inline]
    fn as_ref(&self) -> &str {
        self.as_path().as_str()
    }
}

impl<'a, T> Iterator for Utf8Iter<'a, T>
where
    T: Utf8Encoding + 'a,
{
    type Item = &'a str;

    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        match self.inner.next() {
            Some(c) => Some(c.as_str()),
            None => None,
        }
    }
}

impl<'a, T> DoubleEndedIterator for Utf8Iter<'a, T>
where
    T: Utf8Encoding + 'a,
{
    #[inline]
    fn next_back(&mut self) -> Option<Self::Item> {
        match self.inner.next_back() {
            Some(c) => Some(c.as_str()),
            None => None,
        }
    }
}

impl<'a, T> FusedIterator for Utf8Iter<'a, T> where T: Utf8Encoding + 'a {}

/// An iterator over [`Utf8Path`] and its ancestors.
///
/// This `struct` is created by the [`ancestors`] method on [`Utf8Path`].
/// See its documentation for more.
///
/// # Examples
///
/// ```
/// use typed_path::{Utf8Path, Utf8UnixEncoding};
///
/// // NOTE: A path cannot be created on its own without a defined encoding
/// let path = Utf8Path::<Utf8UnixEncoding>::new("/foo/bar");
///
/// for ancestor in path.ancestors() {
///     println!("{}", ancestor);
/// }
/// ```
///
/// [`ancestors`]: Utf8Path::ancestors
#[derive(Copy, Clone, Debug)]
pub struct Utf8Ancestors<'a, T>
where
    T: Utf8Encoding,
{
    pub(crate) next: Option<&'a Utf8Path<T>>,
}

impl<'a, T> Iterator for Utf8Ancestors<'a, T>
where
    T: Utf8Encoding,
{
    type Item = &'a Utf8Path<T>;

    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        let next = self.next;
        self.next = next.and_then(Utf8Path::parent);
        next
    }
}

impl<T> FusedIterator for Utf8Ancestors<'_, T> where T: Utf8Encoding {}
