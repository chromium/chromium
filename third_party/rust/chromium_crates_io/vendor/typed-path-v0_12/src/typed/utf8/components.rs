mod component;
use core::{cmp, fmt, iter};

pub use component::*;

use crate::typed::Utf8TypedPath;
use crate::unix::Utf8UnixComponents;
use crate::windows::Utf8WindowsComponents;
use crate::{private, Utf8Components};

#[derive(Clone)]
pub enum Utf8TypedComponents<'a> {
    Unix(Utf8UnixComponents<'a>),
    Windows(Utf8WindowsComponents<'a>),
}

impl<'a> Utf8TypedComponents<'a> {
    /// Extracts a slice corresponding to the portion of the path remaining for iteration.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8TypedPath;
    ///
    /// let mut components = Utf8TypedPath::derive("/tmp/foo/bar.txt").components();
    /// components.next();
    /// components.next();
    ///
    /// assert_eq!(Utf8TypedPath::derive("foo/bar.txt"), components.to_path());
    /// ```
    pub fn to_path(&self) -> Utf8TypedPath<'a> {
        match self {
            Self::Unix(components) => Utf8TypedPath::Unix(components.as_path()),
            Self::Windows(components) => Utf8TypedPath::Windows(components.as_path()),
        }
    }

    /// Extracts a slice corresponding to the portion of the path remaining for iteration.
    pub fn as_str(&self) -> &'a str {
        impl_typed_fn!(self, as_str)
    }

    /// Reports back whether the iterator represents an absolute path
    ///
    /// The definition of an absolute path can vary:
    ///
    /// * On Unix, a path is absolute if it starts with the root, so `is_absolute` and [`has_root`]
    ///   are equivalent.
    ///
    /// * On Windows, a path is absolute if it has a prefix and starts with the root: `c:\windows`
    ///   is absolute, while `c:temp` and `\temp` are not.
    ///
    /// [`has_root`]: Utf8TypedComponents::has_root
    pub fn is_absolute(&self) -> bool {
        impl_typed_fn!(self, is_absolute)
    }

    /// Returns `true` if the iterator represents a path that has a root.
    ///
    /// The definition of what it means for a path to have a root can vary:
    ///
    /// * On Unix, a path has a root if it begins with `/`.
    ///
    /// * On Windows, a path has a root if it:
    ///     * has no prefix and begins with a separator, e.g., `\windows`
    ///     * has a prefix followed by a separator, e.g., `c:\windows` but not `c:windows`
    ///     * has any non-disk prefix, e.g., `\\server\share`
    pub fn has_root(&self) -> bool {
        impl_typed_fn!(self, has_root)
    }
}

impl private::Sealed for Utf8TypedComponents<'_> {}

impl AsRef<[u8]> for Utf8TypedComponents<'_> {
    #[inline]
    fn as_ref(&self) -> &[u8] {
        impl_typed_fn!(self, as_ref)
    }
}

impl AsRef<str> for Utf8TypedComponents<'_> {
    #[inline]
    fn as_ref(&self) -> &str {
        impl_typed_fn!(self, as_ref)
    }
}

impl fmt::Debug for Utf8TypedComponents<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        struct DebugHelper<'a>(Utf8TypedComponents<'a>);

        impl fmt::Debug for DebugHelper<'_> {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                f.debug_list().entries(self.0.clone()).finish()
            }
        }

        f.debug_tuple("Utf8TypedComponents")
            .field(&DebugHelper(self.clone()))
            .finish()
    }
}

impl<'a> Iterator for Utf8TypedComponents<'a> {
    type Item = Utf8TypedComponent<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        match self {
            Self::Unix(it) => it.next().map(Utf8TypedComponent::Unix),
            Self::Windows(it) => it.next().map(Utf8TypedComponent::Windows),
        }
    }
}

impl DoubleEndedIterator for Utf8TypedComponents<'_> {
    fn next_back(&mut self) -> Option<Self::Item> {
        match self {
            Self::Unix(it) => it.next_back().map(Utf8TypedComponent::Unix),
            Self::Windows(it) => it.next_back().map(Utf8TypedComponent::Windows),
        }
    }
}

impl iter::FusedIterator for Utf8TypedComponents<'_> {}

impl cmp::PartialEq for Utf8TypedComponents<'_> {
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        match (self, other) {
            (Self::Unix(a), Self::Unix(b)) => a.eq(b),
            (Self::Windows(a), Self::Windows(b)) => a.eq(b),
            _ => false,
        }
    }
}

impl cmp::Eq for Utf8TypedComponents<'_> {}

impl cmp::PartialOrd for Utf8TypedComponents<'_> {
    #[inline]
    fn partial_cmp(&self, other: &Self) -> Option<cmp::Ordering> {
        match (self, other) {
            (Self::Unix(a), Self::Unix(b)) => a.partial_cmp(b),
            (Self::Windows(a), Self::Windows(b)) => a.partial_cmp(b),
            _ => None,
        }
    }
}
