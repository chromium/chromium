mod component;

use core::{cmp, fmt, iter};

pub use component::*;

use crate::unix::UnixComponents;
use crate::{private, Components, Utf8Components, Utf8Encoding, Utf8Path};

#[derive(Clone)]
pub struct Utf8UnixComponents<'a> {
    inner: UnixComponents<'a>,
}

impl<'a> Utf8UnixComponents<'a> {
    pub(crate) fn new(path: &'a str) -> Self {
        Self {
            inner: UnixComponents::new(path.as_bytes()),
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
    /// let mut components = Utf8Path::<Utf8UnixEncoding>::new("/tmp/foo/bar.txt").components();
    /// components.next();
    /// components.next();
    ///
    /// assert_eq!(Utf8Path::<Utf8UnixEncoding>::new("foo/bar.txt"), components.as_path());
    /// ```
    pub fn as_path<T>(&self) -> &'a Utf8Path<T>
    where
        T: Utf8Encoding,
    {
        Utf8Path::new(self.as_str())
    }
}

impl private::Sealed for Utf8UnixComponents<'_> {}

impl<'a> Utf8Components<'a> for Utf8UnixComponents<'a> {
    type Component = Utf8UnixComponent<'a>;

    fn as_str(&self) -> &'a str {
        // NOTE: We know that the internal byte representation is UTF-8 compliant as we ensure that
        //       the only input provided is UTF-8 and no modifications are made with non-UTF-8 bytes
        unsafe { core::str::from_utf8_unchecked(self.inner.as_bytes()) }
    }

    fn is_absolute(&self) -> bool {
        self.inner.is_absolute()
    }

    fn has_root(&self) -> bool {
        self.inner.has_root()
    }
}

impl AsRef<[u8]> for Utf8UnixComponents<'_> {
    #[inline]
    fn as_ref(&self) -> &[u8] {
        self.as_str().as_bytes()
    }
}

impl AsRef<str> for Utf8UnixComponents<'_> {
    #[inline]
    fn as_ref(&self) -> &str {
        self.as_str()
    }
}

impl<T> AsRef<Utf8Path<T>> for Utf8UnixComponents<'_>
where
    T: Utf8Encoding,
{
    #[inline]
    fn as_ref(&self) -> &Utf8Path<T> {
        Utf8Path::new(self.as_str())
    }
}

impl fmt::Debug for Utf8UnixComponents<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        struct DebugHelper<'a>(Utf8UnixComponents<'a>);

        impl fmt::Debug for DebugHelper<'_> {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                f.debug_list().entries(self.0.clone()).finish()
            }
        }

        f.debug_tuple("Utf8WindowsComponents")
            .field(&DebugHelper(self.clone()))
            .finish()
    }
}

impl<'a> Iterator for Utf8UnixComponents<'a> {
    type Item = <Self as Utf8Components<'a>>::Component;

    fn next(&mut self) -> Option<Self::Item> {
        self.inner
            .next()
            .map(|c| unsafe { Utf8UnixComponent::from_utf8_unchecked(&c) })
    }
}

impl DoubleEndedIterator for Utf8UnixComponents<'_> {
    fn next_back(&mut self) -> Option<Self::Item> {
        self.inner
            .next_back()
            .map(|c| unsafe { Utf8UnixComponent::from_utf8_unchecked(&c) })
    }
}

impl iter::FusedIterator for Utf8UnixComponents<'_> {}

impl cmp::PartialEq for Utf8UnixComponents<'_> {
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        PartialEq::eq(&self.inner, &other.inner)
    }
}

impl cmp::Eq for Utf8UnixComponents<'_> {}

impl cmp::PartialOrd for Utf8UnixComponents<'_> {
    #[inline]
    fn partial_cmp(&self, other: &Self) -> Option<cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl cmp::Ord for Utf8UnixComponents<'_> {
    #[inline]
    fn cmp(&self, other: &Self) -> cmp::Ordering {
        Ord::cmp(&self.inner, &other.inner)
    }
}
