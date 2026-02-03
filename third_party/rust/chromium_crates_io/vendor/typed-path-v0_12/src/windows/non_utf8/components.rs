mod component;
mod parser;

use core::{cmp, fmt, iter};

pub use component::*;
use parser::Parser;

use crate::{private, Components, Encoding, Path};

/// Represents a Windows-specific [`Components`]
#[derive(Clone)]
pub struct WindowsComponents<'a> {
    parser: Parser<'a>,
}

impl<'a> WindowsComponents<'a> {
    pub(crate) fn new(path: &'a [u8]) -> Self {
        Self {
            parser: Parser::new(path),
        }
    }

    /// Extracts a slice corresponding to the portion of the path remaining for iteration.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Path, WindowsEncoding};
    ///
    /// // NOTE: A path cannot be created on its own without a defined encoding
    /// let mut components = Path::<WindowsEncoding>::new(r"\tmp\foo\bar.txt").components();
    /// components.next();
    /// components.next();
    ///
    /// assert_eq!(Path::<WindowsEncoding>::new(r"foo\bar.txt"), components.as_path());
    /// ```
    pub fn as_path<T>(&self) -> &'a Path<T>
    where
        T: Encoding,
    {
        Path::new(self.parser.remaining())
    }
}

impl private::Sealed for WindowsComponents<'_> {}

impl<'a> Components<'a> for WindowsComponents<'a> {
    type Component = WindowsComponent<'a>;

    fn as_bytes(&self) -> &'a [u8] {
        self.parser.remaining()
    }

    /// Returns true only if the path represented by the components
    /// has a prefix followed by a root directory
    ///
    /// e.g. `C:\some\path` -> true, `C:some\path` -> false
    fn is_absolute(&self) -> bool {
        // Create a copy of our parser so we don't mutate state
        let mut parser = self.parser.clone();

        matches!(
            (parser.next_front(), parser.next_front()),
            (
                Ok(WindowsComponent::Prefix(_)),
                Ok(WindowsComponent::RootDir)
            )
        )
    }

    /// Returns true if the `path` has either:
    ///
    /// * physical root, meaning it begins with the separator (e.g. `\my\path` or `C:\`)
    /// * implicit root, meaning it begins with a prefix that is not a drive (e.g. `\\?\pictures`)
    fn has_root(&self) -> bool {
        // Create a copy of our parser so we don't mutate state
        let mut parser = self.parser.clone();

        match parser.next_front() {
            Ok(WindowsComponent::RootDir) => true,
            Ok(WindowsComponent::Prefix(p)) => match p.kind() {
                WindowsPrefix::Disk(_) | WindowsPrefix::VerbatimDisk(_) => {
                    matches!(parser.next_front(), Ok(WindowsComponent::RootDir))
                }
                _ => true,
            },
            _ => false,
        }
    }
}

impl<'a> WindowsComponents<'a> {
    fn peek_front(&self) -> Option<<Self as Components<'a>>::Component> {
        // Create a clone of our parser so we don't mutate our state
        let mut parser = self.parser.clone();

        parser.next_front().ok()
    }

    /// Returns true if the represented path has a prefix
    #[inline]
    pub fn has_prefix(&self) -> bool {
        self.prefix().is_some()
    }

    /// Returns the prefix of the represented path's components if it has one
    pub fn prefix(&self) -> Option<WindowsPrefixComponent<'_>> {
        match self.peek_front() {
            Some(WindowsComponent::Prefix(p)) => Some(p),
            _ => None,
        }
    }

    #[inline]
    pub(crate) fn prefix_len(&self) -> usize {
        self.prefix().map(|p| p.as_bytes().len()).unwrap_or(0)
    }

    /// Returns the kind of prefix associated with the represented path if it has one
    #[inline]
    pub fn prefix_kind(&self) -> Option<WindowsPrefix<'_>> {
        self.prefix().map(|p| p.kind())
    }

    /// Returns true if represented path has a verbatim, verbatim UNC, or verbatim disk prefix
    pub fn has_any_verbatim_prefix(&self) -> bool {
        matches!(
            self.prefix_kind(),
            Some(WindowsPrefix::Verbatim(_) | WindowsPrefix::UNC(..) | WindowsPrefix::Disk(_))
        )
    }

    /// Returns true if represented path has a verbatim prefix (e.g. `\\?\pictures)
    pub fn has_verbatim_prefix(&self) -> bool {
        matches!(self.prefix_kind(), Some(WindowsPrefix::Verbatim(_)))
    }

    /// Returns true if represented path has a verbatim UNC prefix (e.g. `\\?\UNC\server\share`)
    pub fn has_verbatim_unc_prefix(&self) -> bool {
        matches!(self.prefix_kind(), Some(WindowsPrefix::VerbatimUNC(..)))
    }

    /// Returns true if represented path has a verbatim disk prefix (e.g. `\\?\C:`)
    pub fn has_verbatim_disk_prefix(&self) -> bool {
        matches!(self.prefix_kind(), Some(WindowsPrefix::VerbatimDisk(_)))
    }

    /// Returns true if represented path has a device NS prefix (e.g. `\\.\BrainInterface`)
    pub fn has_device_ns_prefix(&self) -> bool {
        matches!(self.prefix_kind(), Some(WindowsPrefix::DeviceNS(_)))
    }

    /// Returns true if represented path has a UNC prefix (e.g. `\\server\share`)
    pub fn has_unc_prefix(&self) -> bool {
        matches!(self.prefix_kind(), Some(WindowsPrefix::UNC(..)))
    }

    /// Returns true if represented path has a disk prefix (e.g. `C:`)
    pub fn has_disk_prefix(&self) -> bool {
        matches!(self.prefix_kind(), Some(WindowsPrefix::Disk(_)))
    }

    /// Returns true if there is a separator immediately after the prefix, or separator
    /// starts the components if there is no prefix
    ///
    /// e.g. `C:\` and `\path` would return true whereas `\\?\path` would return false
    pub fn has_physical_root(&self) -> bool {
        // Create a clone of our parser so we don't mutate our state
        let mut parser = self.parser.clone();

        match parser.next_front() {
            Ok(WindowsComponent::RootDir) => true,
            Ok(WindowsComponent::Prefix(_)) => {
                matches!(parser.next_front(), Ok(WindowsComponent::RootDir))
            }
            _ => false,
        }
    }

    /// Returns true if there is a root separator without a [`WindowsComponent::RootDir`]
    /// needing to be present. This is tied to prefixes like verbatim `\\?\` and UNC `\\`.
    ///
    /// Really, it's everything but a disk prefix of `C:` that provide an implicit root
    pub fn has_implicit_root(&self) -> bool {
        match self.prefix().map(|p| p.kind()) {
            Some(WindowsPrefix::Disk(_)) | None => false,
            Some(_) => true,
        }
    }

    /// Returns true if just a disk, e.g. `C:`
    pub(crate) fn is_only_disk(&self) -> bool {
        self.has_disk_prefix() && {
            // Create a clone of our parser so we don't mutate our state
            let mut parser = self.parser.clone();
            let _ = parser.next_front();
            parser.next_front().is_err()
        }
    }
}

impl AsRef<[u8]> for WindowsComponents<'_> {
    #[inline]
    fn as_ref(&self) -> &[u8] {
        self.as_bytes()
    }
}

impl<T> AsRef<Path<T>> for WindowsComponents<'_>
where
    T: Encoding,
{
    #[inline]
    fn as_ref(&self) -> &Path<T> {
        Path::new(self.as_bytes())
    }
}

impl fmt::Debug for WindowsComponents<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        struct DebugHelper<'a>(WindowsComponents<'a>);

        impl fmt::Debug for DebugHelper<'_> {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                f.debug_list().entries(self.0.clone()).finish()
            }
        }

        f.debug_tuple("WindowsComponents")
            .field(&DebugHelper(self.clone()))
            .finish()
    }
}

impl<'a> Iterator for WindowsComponents<'a> {
    type Item = <Self as Components<'a>>::Component;

    fn next(&mut self) -> Option<Self::Item> {
        self.parser.next_front().ok()
    }
}

impl DoubleEndedIterator for WindowsComponents<'_> {
    fn next_back(&mut self) -> Option<Self::Item> {
        self.parser.next_back().ok()
    }
}

impl iter::FusedIterator for WindowsComponents<'_> {}

impl cmp::PartialEq for WindowsComponents<'_> {
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        let _self = Self::new(self.parser.remaining());
        let _other = Self::new(other.parser.remaining());

        _self.eq(_other)
    }
}

impl cmp::Eq for WindowsComponents<'_> {}

impl cmp::PartialOrd for WindowsComponents<'_> {
    #[inline]
    fn partial_cmp(&self, other: &Self) -> Option<cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl cmp::Ord for WindowsComponents<'_> {
    #[inline]
    fn cmp(&self, other: &Self) -> cmp::Ordering {
        let _self = Self::new(self.parser.remaining());
        let _other = Self::new(other.parser.remaining());

        _self.cmp(_other)
    }
}
