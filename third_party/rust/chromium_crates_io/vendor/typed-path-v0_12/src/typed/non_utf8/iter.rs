use core::fmt;
use core::iter::FusedIterator;

use crate::common::{Ancestors, Iter};
use crate::typed::TypedPath;
use crate::unix::UnixEncoding;
use crate::windows::WindowsEncoding;

/// An iterator over the [`TypedComponent`]s of a [`TypedPath`], as [`[u8]`] slices.
///
/// This `struct` is created by the [`iter`] method on [`TypedPath`].
/// See its documentation for more.
///
/// [`iter`]: TypedPath::iter
/// [`TypedComponent`]: crate::TypedComponent
#[derive(Clone)]
pub enum TypedIter<'a> {
    Unix(Iter<'a, UnixEncoding>),
    Windows(Iter<'a, WindowsEncoding>),
}

impl TypedIter<'_> {
    /// Extracts a slice corresponding to the portion of the path remaining for iteration.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::TypedPath;
    ///
    /// let mut iter = TypedPath::derive("/tmp/foo/bar.txt").iter();
    /// iter.next();
    /// iter.next();
    ///
    /// assert_eq!(TypedPath::derive("foo/bar.txt"), iter.to_path());
    /// ```
    pub fn to_path(&self) -> TypedPath<'_> {
        match self {
            Self::Unix(it) => TypedPath::Unix(it.as_path()),
            Self::Windows(it) => TypedPath::Windows(it.as_path()),
        }
    }

    /// Returns reference to the underlying byte slice represented by this iterator.
    pub fn as_bytes(&self) -> &[u8] {
        impl_typed_fn!(self, as_ref)
    }
}

impl fmt::Debug for TypedIter<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        struct DebugHelper<'a>(TypedPath<'a>);

        impl fmt::Debug for DebugHelper<'_> {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                f.debug_list().entries(self.0.iter()).finish()
            }
        }

        f.debug_tuple(stringify!(TypedIter))
            .field(&DebugHelper(self.to_path()))
            .finish()
    }
}

impl AsRef<[u8]> for TypedIter<'_> {
    #[inline]
    fn as_ref(&self) -> &[u8] {
        self.as_bytes()
    }
}

impl<'a> Iterator for TypedIter<'a> {
    type Item = &'a [u8];

    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        match self {
            Self::Unix(it) => it.next(),
            Self::Windows(it) => it.next(),
        }
    }
}

impl DoubleEndedIterator for TypedIter<'_> {
    #[inline]
    fn next_back(&mut self) -> Option<Self::Item> {
        match self {
            Self::Unix(it) => it.next_back(),
            Self::Windows(it) => it.next_back(),
        }
    }
}

impl FusedIterator for TypedIter<'_> {}

/// An iterator over [`TypedPath`] and its ancestors.
///
/// This `struct` is created by the [`ancestors`] method on [`TypedPath`].
/// See its documentation for more.
///
/// # Examples
///
/// ```
/// use typed_path::{TypedPath};
///
/// let path = TypedPath::derive("/foo/bar");
///
/// for ancestor in path.ancestors() {
///     println!("{}", ancestor.display());
/// }
/// ```
///
/// [`ancestors`]: TypedPath::ancestors
#[derive(Copy, Clone, Debug)]
pub enum TypedAncestors<'a> {
    Unix(Ancestors<'a, UnixEncoding>),
    Windows(Ancestors<'a, WindowsEncoding>),
}

impl<'a> Iterator for TypedAncestors<'a> {
    type Item = TypedPath<'a>;

    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        match self {
            Self::Unix(it) => it.next().map(TypedPath::Unix),
            Self::Windows(it) => it.next().map(TypedPath::Windows),
        }
    }
}

impl FusedIterator for TypedAncestors<'_> {}
