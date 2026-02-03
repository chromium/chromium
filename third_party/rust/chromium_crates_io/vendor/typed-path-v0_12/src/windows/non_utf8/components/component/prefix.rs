use core::cmp;
use core::convert::TryFrom;
use core::hash::{Hash, Hasher};

use crate::windows::WindowsComponents;
use crate::ParseError;

/// A structure wrapping a Windows path prefix as well as its unparsed string
/// representation. Byte slice version of [`std::path::PrefixComponent`].
///
/// In addition to the parsed [`WindowsPrefix`] information returned by [`kind`],
/// `WindowsPrefixComponent` also holds the raw and unparsed [`[u8]`] slice,
/// returned by [`as_bytes`].
///
/// Instances of this `struct` can be obtained by matching against the
/// [`WindowsPrefix` variant] on [`WindowsComponent`].
///
/// This is available for use on all platforms despite being a Windows-specific format.
///
/// [`WindowsComponent`]: crate::WindowsComponent
///
/// # Examples
///
/// ```
/// use typed_path::{WindowsPath, WindowsComponent, WindowsPrefix};
///
/// let path = WindowsPath::new(r"c:\you\later\");
/// match path.components().next().unwrap() {
///     WindowsComponent::Prefix(prefix_component) => {
///         // Notice that the disk kind uses an uppercase letter, but the raw slice
///         // underneath has a lowercase drive letter
///         assert_eq!(WindowsPrefix::Disk(b'C'), prefix_component.kind());
///         assert_eq!(b"c:".as_slice(), prefix_component.as_bytes());
///     }
///     _ => unreachable!(),
/// }
/// ```
///
/// [`as_bytes`]: WindowsPrefixComponent::as_bytes
/// [`kind`]: WindowsPrefixComponent::kind
/// [`WindowsPrefix` variant]: crate::WindowsComponent::Prefix
#[derive(Copy, Clone, Debug, Eq)]
pub struct WindowsPrefixComponent<'a> {
    /// The prefix as an unparsed `[u8]` slice
    pub(crate) raw: &'a [u8],

    /// The parsed prefix data
    pub(crate) parsed: WindowsPrefix<'a>,
}

impl<'a> WindowsPrefixComponent<'a> {
    /// Returns the parsed prefix data
    ///
    /// See [`WindowsPrefix`]'s documentation for more information on the different
    /// kinds of prefixes.
    pub fn kind(&self) -> WindowsPrefix<'a> {
        self.parsed
    }

    /// Returns the size of the prefix in bytes
    #[allow(clippy::len_without_is_empty)]
    pub fn len(&self) -> usize {
        self.raw.len()
    }

    /// Returns the raw [`[u8]`] slice for this prefix
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::WindowsPrefixComponent;
    /// use std::convert::TryFrom;
    ///
    /// // Disk will include the drive letter and :
    /// let component = WindowsPrefixComponent::try_from(b"C:").unwrap();
    /// assert_eq!(component.as_bytes(), b"C:");
    ///
    /// // UNC will include server & share
    /// let component = WindowsPrefixComponent::try_from(br"\\server\share").unwrap();
    /// assert_eq!(component.as_bytes(), br"\\server\share");
    ///
    /// // Device NS will include device
    /// let component = WindowsPrefixComponent::try_from(br"\\.\BrainInterface").unwrap();
    /// assert_eq!(component.as_bytes(), br"\\.\BrainInterface");
    ///
    /// // Verbatim will include component
    /// let component = WindowsPrefixComponent::try_from(br"\\?\pictures").unwrap();
    /// assert_eq!(component.as_bytes(), br"\\?\pictures");
    ///
    /// // Verbatim UNC will include server & share
    /// let component = WindowsPrefixComponent::try_from(br"\\?\UNC\server\share").unwrap();
    /// assert_eq!(component.as_bytes(), br"\\?\UNC\server\share");
    ///
    /// // Verbatim disk will include drive letter and :
    /// let component = WindowsPrefixComponent::try_from(br"\\?\C:").unwrap();
    /// assert_eq!(component.as_bytes(), br"\\?\C:");
    /// ```
    pub fn as_bytes(&self) -> &'a [u8] {
        self.raw
    }
}

impl<'a> TryFrom<&'a [u8]> for WindowsPrefixComponent<'a> {
    type Error = ParseError;

    /// Parses the byte slice into a [`WindowsPrefixComponent`]
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{WindowsPrefix, WindowsPrefixComponent};
    /// use std::convert::TryFrom;
    ///
    /// let component = WindowsPrefixComponent::try_from(b"C:").unwrap();
    /// assert_eq!(component.kind(), WindowsPrefix::Disk(b'C'));
    ///
    /// let component = WindowsPrefixComponent::try_from(br"\\.\BrainInterface").unwrap();
    /// assert_eq!(component.kind(), WindowsPrefix::DeviceNS(b"BrainInterface"));
    ///
    /// let component = WindowsPrefixComponent::try_from(br"\\server\share").unwrap();
    /// assert_eq!(component.kind(), WindowsPrefix::UNC(b"server", b"share"));
    ///
    /// let component = WindowsPrefixComponent::try_from(br"\\?\UNC\server\share").unwrap();
    /// assert_eq!(component.kind(), WindowsPrefix::VerbatimUNC(b"server", b"share"));
    ///
    /// let component = WindowsPrefixComponent::try_from(br"\\?\C:").unwrap();
    /// assert_eq!(component.kind(), WindowsPrefix::VerbatimDisk(b'C'));
    ///
    /// let component = WindowsPrefixComponent::try_from(br"\\?\pictures").unwrap();
    /// assert_eq!(component.kind(), WindowsPrefix::Verbatim(b"pictures"));
    ///
    /// // Parsing something that is not a prefix will fail
    /// assert!(WindowsPrefixComponent::try_from(b"hello").is_err());
    ///
    /// // Parsing more than a prefix will fail
    /// assert!(WindowsPrefixComponent::try_from(br"C:\path").is_err());
    /// ```
    fn try_from(path: &'a [u8]) -> Result<Self, Self::Error> {
        let mut components = WindowsComponents::new(path);

        let prefix = components
            .next()
            .and_then(|c| c.prefix())
            .ok_or("not a prefix")?;

        if components.next().is_some() {
            return Err("contains more than prefix");
        }

        Ok(prefix)
    }
}

impl<'a, const N: usize> TryFrom<&'a [u8; N]> for WindowsPrefixComponent<'a> {
    type Error = ParseError;

    fn try_from(path: &'a [u8; N]) -> Result<Self, Self::Error> {
        Self::try_from(path.as_slice())
    }
}

impl<'a> TryFrom<&'a str> for WindowsPrefixComponent<'a> {
    type Error = ParseError;

    fn try_from(s: &'a str) -> Result<Self, Self::Error> {
        Self::try_from(s.as_bytes())
    }
}

impl<'a> cmp::PartialEq for WindowsPrefixComponent<'a> {
    #[inline]
    fn eq(&self, other: &WindowsPrefixComponent<'a>) -> bool {
        cmp::PartialEq::eq(&self.parsed, &other.parsed)
    }
}

impl<'a> cmp::PartialOrd for WindowsPrefixComponent<'a> {
    #[inline]
    fn partial_cmp(&self, other: &WindowsPrefixComponent<'a>) -> Option<cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl cmp::Ord for WindowsPrefixComponent<'_> {
    #[inline]
    fn cmp(&self, other: &Self) -> cmp::Ordering {
        cmp::Ord::cmp(&self.parsed, &other.parsed)
    }
}

impl Hash for WindowsPrefixComponent<'_> {
    fn hash<H: Hasher>(&self, h: &mut H) {
        self.parsed.hash(h);
    }
}

/// Windows path prefixes, e.g., `C:` or `\\server\share`. This is a byte slice version of
/// [`std::path::Prefix`].
///
/// Windows uses a variety of path prefix styles, including references to drive
/// volumes (like `C:`), network shared folders (like `\\server\share`), and
/// others. In addition, some path prefixes are "verbatim" (i.e., prefixed with
/// `\\?\`), in which case `/` is *not* treated as a separator and essentially
/// no normalization is performed.
///
/// # Examples
///
/// ```
/// use typed_path::{WindowsPath, WindowsComponent, WindowsPrefix};
/// use typed_path::WindowsPrefix::*;
///
/// fn get_path_prefix(s: &str) -> WindowsPrefix {
///     let path = WindowsPath::new(s);
///     match path.components().next().unwrap() {
///         WindowsComponent::Prefix(prefix_component) => prefix_component.kind(),
///         _ => panic!(),
///     }
/// }
///
/// assert_eq!(Verbatim(b"pictures"), get_path_prefix(r"\\?\pictures\kittens"));
/// assert_eq!(VerbatimUNC(b"server", b"share"), get_path_prefix(r"\\?\UNC\server\share"));
/// assert_eq!(VerbatimDisk(b'C'), get_path_prefix(r"\\?\c:\"));
/// assert_eq!(DeviceNS(b"BrainInterface"), get_path_prefix(r"\\.\BrainInterface"));
/// assert_eq!(UNC(b"server", b"share"), get_path_prefix(r"\\server\share"));
/// assert_eq!(Disk(b'C'), get_path_prefix(r"C:\Users\Rust\Pictures\Ferris"));
/// ```
#[derive(Copy, Clone, Debug, Hash, PartialOrd, Ord, PartialEq, Eq)]
pub enum WindowsPrefix<'a> {
    /// Verbatim prefix, e.g., `\\?\cat_pics`.
    ///
    /// Verbatim prefixes consist of `\\?\` immediately followed by the given
    /// component.
    Verbatim(&'a [u8]),

    /// Verbatim prefix using Windows' _**U**niform **N**aming **C**onvention_,
    /// e.g., `\\?\UNC\server\share`.
    ///
    /// Verbatim UNC prefixes consist of `\\?\UNC\` immediately followed by the
    /// server's hostname and a share name.
    VerbatimUNC(&'a [u8], &'a [u8]),

    /// Verbatim disk prefix, e.g., `\\?\C:`.
    ///
    /// Verbatim disk prefixes consist of `\\?\` immediately followed by the
    /// drive letter and `:`.
    VerbatimDisk(u8),

    /// Device namespace prefix, e.g., `\\.\COM42`.
    ///
    /// Device namespace prefixes consist of `\\.\` (possibly using `/`
    /// instead of `\`), immediately followed by the device name.
    DeviceNS(&'a [u8]),

    /// Prefix using Windows' _**U**niform **N**aming **C**onvention_, e.g.
    /// `\\server\share`.
    ///
    /// UNC prefixes consist of the server's hostname and a share name.
    UNC(&'a [u8], &'a [u8]),

    /// Prefix `C:` for the given disk drive.
    Disk(u8),
}

impl<'a> TryFrom<&'a [u8]> for WindowsPrefix<'a> {
    type Error = ParseError;

    fn try_from(path: &'a [u8]) -> Result<Self, Self::Error> {
        Ok(WindowsPrefixComponent::try_from(path)?.kind())
    }
}

impl<'a, const N: usize> TryFrom<&'a [u8; N]> for WindowsPrefix<'a> {
    type Error = ParseError;

    fn try_from(path: &'a [u8; N]) -> Result<Self, Self::Error> {
        Self::try_from(path.as_slice())
    }
}

impl<'a> TryFrom<&'a str> for WindowsPrefix<'a> {
    type Error = ParseError;

    fn try_from(path: &'a str) -> Result<Self, Self::Error> {
        Self::try_from(path.as_bytes())
    }
}

impl WindowsPrefix<'_> {
    /// Calculates the full byte length of the prefix
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::WindowsPrefix::*;
    ///
    /// // \\?\pictures -> 12 bytes
    /// assert_eq!(Verbatim(b"pictures").len(), 12);
    ///
    /// // \\?\UNC\server -> 14 bytes
    /// assert_eq!(VerbatimUNC(b"server", b"").len(), 14);
    ///
    /// // \\?\UNC\server\share -> 20 bytes
    /// assert_eq!(VerbatimUNC(b"server", b"share").len(), 20);
    ///
    /// // \\?\c: -> 6 bytes
    /// assert_eq!(VerbatimDisk(b'C').len(), 6);
    ///
    /// // \\.\BrainInterface -> 18 bytes
    /// assert_eq!(DeviceNS(b"BrainInterface").len(), 18);
    ///
    /// // \\server\share -> 14 bytes
    /// assert_eq!(UNC(b"server", b"share").len(), 14);
    ///
    /// // C: -> 2 bytes
    /// assert_eq!(Disk(b'C').len(), 2);
    /// ```
    #[inline]
    #[allow(clippy::len_without_is_empty)]
    pub fn len(&self) -> usize {
        use self::WindowsPrefix::*;
        match *self {
            Verbatim(x) => 4 + x.len(),
            VerbatimUNC(x, y) => 8 + x.len() + if !y.is_empty() { 1 + y.len() } else { 0 },
            VerbatimDisk(_) => 6,
            UNC(x, y) => 2 + x.len() + if !y.is_empty() { 1 + y.len() } else { 0 },
            DeviceNS(x) => 4 + x.len(),
            Disk(_) => 2,
        }
    }

    /// Determines if the prefix is verbatim, i.e., begins with `\\?\`.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::WindowsPrefix::*;
    ///
    /// assert!(Verbatim(b"pictures").is_verbatim());
    /// assert!(VerbatimUNC(b"server", b"share").is_verbatim());
    /// assert!(VerbatimDisk(b'C').is_verbatim());
    /// assert!(!DeviceNS(b"BrainInterface").is_verbatim());
    /// assert!(!UNC(b"server", b"share").is_verbatim());
    /// assert!(!Disk(b'C').is_verbatim());
    /// ```
    #[inline]
    pub fn is_verbatim(&self) -> bool {
        use self::WindowsPrefix::*;
        matches!(*self, Verbatim(_) | VerbatimDisk(_) | VerbatimUNC(..))
    }
}
