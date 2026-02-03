mod component;

use core::{cmp, fmt, iter};

pub use component::*;

use crate::windows::WindowsComponents;
use crate::{private, Components, Utf8Components, Utf8Encoding, Utf8Path};

/// Represents a Windows-specific [`Components`]
#[derive(Clone)]
pub struct Utf8WindowsComponents<'a> {
    inner: WindowsComponents<'a>,
}

impl<'a> Utf8WindowsComponents<'a> {
    pub(crate) fn new(path: &'a str) -> Self {
        Self {
            inner: WindowsComponents::new(path.as_bytes()),
        }
    }

    /// Extracts a slice corresponding to the portion of the path remaining for iteration.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Path, Utf8WindowsEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let mut components = Utf8Path::<Utf8WindowsEncoding>::new(r"\tmp\foo\bar.txt").components();
    /// components.next();
    /// components.next();
    ///
    /// assert_eq!(Utf8Path::<Utf8WindowsEncoding>::new(r"foo\bar.txt"), components.as_path());
    /// ```
    pub fn as_path<T>(&self) -> &'a Utf8Path<T>
    where
        T: Utf8Encoding,
    {
        Utf8Path::new(self.as_str())
    }
}

impl private::Sealed for Utf8WindowsComponents<'_> {}

impl<'a> Utf8Components<'a> for Utf8WindowsComponents<'a> {
    type Component = Utf8WindowsComponent<'a>;

    fn as_str(&self) -> &'a str {
        // NOTE: We know that the internal byte representation is UTF-8 compliant as we ensure that
        //       the only input provided is UTF-8 and no modifications are made with non-UTF-8 bytes
        unsafe { core::str::from_utf8_unchecked(self.inner.as_bytes()) }
    }

    /// Returns true only if the path represented by the components
    /// has a prefix followed by a root directory
    ///
    /// e.g. `C:\some\path` -> true, `C:some\path` -> false
    fn is_absolute(&self) -> bool {
        self.inner.is_absolute()
    }

    /// Returns true if the `path` has either:
    ///
    /// * physical root, meaning it begins with the separator (e.g. `\my\path` or `C:\`)
    /// * implicit root, meaning it begins with a prefix that is not a drive (e.g. `\\?\pictures`)
    fn has_root(&self) -> bool {
        self.inner.has_root()
    }
}

impl<'a> Utf8WindowsComponents<'a> {
    fn peek_front(&self) -> Option<<Self as Utf8Components<'a>>::Component> {
        self.clone().next()
    }

    /// Returns true if the represented path has a prefix
    #[inline]
    pub fn has_prefix(&self) -> bool {
        self.prefix().is_some()
    }

    /// Returns the prefix of the represented path's components if it has one
    pub fn prefix(&self) -> Option<Utf8WindowsPrefixComponent<'_>> {
        match self.peek_front() {
            Some(Utf8WindowsComponent::Prefix(p)) => Some(p),
            _ => None,
        }
    }

    /// Returns the kind of prefix associated with the represented path if it has one
    #[inline]
    pub fn prefix_kind(&self) -> Option<Utf8WindowsPrefix<'_>> {
        self.prefix().map(|p| p.kind())
    }

    /// Returns true if represented path has a verbatim, verbatim UNC, or verbatim disk prefix
    pub fn has_any_verbatim_prefix(&self) -> bool {
        matches!(
            self.prefix_kind(),
            Some(
                Utf8WindowsPrefix::Verbatim(_)
                    | Utf8WindowsPrefix::UNC(..)
                    | Utf8WindowsPrefix::Disk(_)
            )
        )
    }

    /// Returns true if represented path has a verbatim prefix (e.g. `\\?\pictures)
    pub fn has_verbatim_prefix(&self) -> bool {
        matches!(self.prefix_kind(), Some(Utf8WindowsPrefix::Verbatim(_)))
    }

    /// Returns true if represented path has a verbatim UNC prefix (e.g. `\\?\UNC\server\share`)
    pub fn has_verbatim_unc_prefix(&self) -> bool {
        matches!(self.prefix_kind(), Some(Utf8WindowsPrefix::VerbatimUNC(..)))
    }

    /// Returns true if represented path has a verbatim disk prefix (e.g. `\\?\C:`)
    pub fn has_verbatim_disk_prefix(&self) -> bool {
        matches!(self.prefix_kind(), Some(Utf8WindowsPrefix::VerbatimDisk(_)))
    }

    /// Returns true if represented path has a device NS prefix (e.g. `\\.\BrainInterface`)
    pub fn has_device_ns_prefix(&self) -> bool {
        matches!(self.prefix_kind(), Some(Utf8WindowsPrefix::DeviceNS(_)))
    }

    /// Returns true if represented path has a UNC prefix (e.g. `\\server\share`)
    pub fn has_unc_prefix(&self) -> bool {
        matches!(self.prefix_kind(), Some(Utf8WindowsPrefix::UNC(..)))
    }

    /// Returns true if represented path has a disk prefix (e.g. `C:`)
    pub fn has_disk_prefix(&self) -> bool {
        matches!(self.prefix_kind(), Some(Utf8WindowsPrefix::Disk(_)))
    }

    /// Returns true if there is a separator immediately after the prefix, or separator
    /// starts the components if there is no prefix
    ///
    /// e.g. `C:\` and `\path` would return true whereas `\\?\path` would return false
    pub fn has_physical_root(&self) -> bool {
        self.inner.has_physical_root()
    }

    /// Returns true if there is a root separator without a [`Utf8WindowsComponent::RootDir`]
    /// needing to be present. This is tied to prefixes like verbatim `\\?\` and UNC `\\`.
    ///
    /// Really, it's everything but a disk prefix of `C:` that provide an implicit root
    pub fn has_implicit_root(&self) -> bool {
        match self.prefix().map(|p| p.kind()) {
            Some(Utf8WindowsPrefix::Disk(_)) | None => false,
            Some(_) => true,
        }
    }
}

impl AsRef<[u8]> for Utf8WindowsComponents<'_> {
    #[inline]
    fn as_ref(&self) -> &[u8] {
        self.as_str().as_bytes()
    }
}

impl AsRef<str> for Utf8WindowsComponents<'_> {
    #[inline]
    fn as_ref(&self) -> &str {
        self.as_str()
    }
}

impl<T> AsRef<Utf8Path<T>> for Utf8WindowsComponents<'_>
where
    T: Utf8Encoding,
{
    #[inline]
    fn as_ref(&self) -> &Utf8Path<T> {
        Utf8Path::new(self.as_str())
    }
}

impl fmt::Debug for Utf8WindowsComponents<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        struct DebugHelper<'a>(Utf8WindowsComponents<'a>);

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

impl<'a> Iterator for Utf8WindowsComponents<'a> {
    type Item = <Self as Utf8Components<'a>>::Component;

    fn next(&mut self) -> Option<Self::Item> {
        self.inner
            .next()
            .map(|c| unsafe { Utf8WindowsComponent::from_utf8_unchecked(&c) })
    }
}

impl DoubleEndedIterator for Utf8WindowsComponents<'_> {
    fn next_back(&mut self) -> Option<Self::Item> {
        self.inner
            .next_back()
            .map(|c| unsafe { Utf8WindowsComponent::from_utf8_unchecked(&c) })
    }
}

impl iter::FusedIterator for Utf8WindowsComponents<'_> {}

impl cmp::PartialEq for Utf8WindowsComponents<'_> {
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        PartialEq::eq(&self.inner, &other.inner)
    }
}

impl cmp::Eq for Utf8WindowsComponents<'_> {}

impl cmp::PartialOrd for Utf8WindowsComponents<'_> {
    #[inline]
    fn partial_cmp(&self, other: &Self) -> Option<cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl cmp::Ord for Utf8WindowsComponents<'_> {
    #[inline]
    fn cmp(&self, other: &Self) -> cmp::Ordering {
        Ord::cmp(&self.inner, &other.inner)
    }
}
