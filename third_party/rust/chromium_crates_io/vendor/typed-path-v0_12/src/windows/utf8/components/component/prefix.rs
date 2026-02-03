use core::cmp;
use core::convert::TryFrom;
use core::hash::{Hash, Hasher};
use core::str::Utf8Error;

use crate::windows::{Utf8WindowsComponents, WindowsPrefix, WindowsPrefixComponent};
use crate::ParseError;

/// A structure wrapping a Windows path prefix as well as its unparsed string
/// representation. [`str`] version of [`std::path::PrefixComponent`].
///
/// In addition to the parsed [`Utf8WindowsPrefix`] information returned by [`kind`],
/// `Utf8WindowsPrefixComponent` also holds the raw and unparsed [`str`] slice,
/// returned by [`as_str`].
///
/// Instances of this `struct` can be obtained by matching against the
/// [`Utf8WindowsPrefix` variant] on [`Utf8WindowsComponent`].
///
/// This is available for use on all platforms despite being a Windows-specific format.
///
/// [`Utf8WindowsComponent`]: crate::Utf8WindowsComponent
///
/// # Examples
///
/// ```
/// use typed_path::{Utf8WindowsPath, Utf8WindowsComponent, Utf8WindowsPrefix};
///
/// let path = Utf8WindowsPath::new(r"c:\you\later\");
/// match path.components().next().unwrap() {
///     Utf8WindowsComponent::Prefix(prefix_component) => {
///         // Notice that the disk kind uses an uppercase letter, but the raw slice
///         // underneath has a lowercase drive letter
///         assert_eq!(Utf8WindowsPrefix::Disk('C'), prefix_component.kind());
///         assert_eq!("c:", prefix_component.as_str());
///     }
///     _ => unreachable!(),
/// }
/// ```
///
/// [`as_str`]: Utf8WindowsPrefixComponent::as_str
/// [`kind`]: Utf8WindowsPrefixComponent::kind
/// [`Utf8WindowsPrefix` variant]: crate::Utf8WindowsComponent::Prefix
#[derive(Copy, Clone, Debug, Eq)]
pub struct Utf8WindowsPrefixComponent<'a> {
    /// The prefix as an unparsed `[u8]` slice
    pub(crate) raw: &'a str,

    /// The parsed prefix data
    pub(crate) parsed: Utf8WindowsPrefix<'a>,
}

impl<'a> Utf8WindowsPrefixComponent<'a> {
    /// Returns the parsed prefix data
    ///
    /// See [`Utf8WindowsPrefix`]'s documentation for more information on the different
    /// kinds of prefixes.
    pub fn kind(&self) -> Utf8WindowsPrefix<'a> {
        self.parsed
    }

    /// Returns the size of the prefix in bytes
    #[allow(clippy::len_without_is_empty)]
    pub fn len(&self) -> usize {
        self.raw.len()
    }

    /// Returns the raw [`str`] slice for this prefix
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8WindowsPrefixComponent;
    /// use std::convert::TryFrom;
    ///
    /// // Disk will include the drive letter and :
    /// let component = Utf8WindowsPrefixComponent::try_from("C:").unwrap();
    /// assert_eq!(component.as_str(), "C:");
    ///
    /// // UNC will include server & share
    /// let component = Utf8WindowsPrefixComponent::try_from(r"\\server\share").unwrap();
    /// assert_eq!(component.as_str(), r"\\server\share");
    ///
    /// // Device NS will include device
    /// let component = Utf8WindowsPrefixComponent::try_from(r"\\.\BrainInterface").unwrap();
    /// assert_eq!(component.as_str(), r"\\.\BrainInterface");
    ///
    /// // Verbatim will include component
    /// let component = Utf8WindowsPrefixComponent::try_from(r"\\?\pictures").unwrap();
    /// assert_eq!(component.as_str(), r"\\?\pictures");
    ///
    /// // Verbatim UNC will include server & share
    /// let component = Utf8WindowsPrefixComponent::try_from(r"\\?\UNC\server\share").unwrap();
    /// assert_eq!(component.as_str(), r"\\?\UNC\server\share");
    ///
    /// // Verbatim disk will include drive letter and :
    /// let component = Utf8WindowsPrefixComponent::try_from(r"\\?\C:").unwrap();
    /// assert_eq!(component.as_str(), r"\\?\C:");
    /// ```
    pub fn as_str(&self) -> &'a str {
        self.raw
    }

    /// Converts a non-UTF-8 [`WindowsPrefixComponent`] to a UTF-8 [`Utf8WindowsPrefixComponent`]
    /// by checking that the component contains valid UTF-8.
    ///
    /// # Errors
    ///
    /// Returns `Err` if the prefix component is not UTF-8 with a description as to why the
    /// provided component is not UTF-8.
    ///
    /// See the docs for [`Utf8Error`] for more details on the kinds of
    /// errors that can be returned.
    pub fn from_utf8(component: &WindowsPrefixComponent<'a>) -> Result<Self, Utf8Error> {
        Ok(Self {
            raw: core::str::from_utf8(component.raw)?,
            parsed: Utf8WindowsPrefix::from_utf8(&component.parsed)?,
        })
    }

    /// Converts a non-UTF-8 [`WindowsPrefixComponent`] to a UTF-8 [`Utf8WindowsPrefixComponent`]
    /// without checking that the string contains valid UTF-8.
    ///
    /// See the safe version, [`from_utf8`], for more information.
    ///
    /// # Safety
    ///
    /// The component passed in must be valid UTF-8.
    ///
    /// [`from_utf8`]: Utf8WindowsPrefixComponent::from_utf8
    pub unsafe fn from_utf8_unchecked(component: &WindowsPrefixComponent<'a>) -> Self {
        Self {
            raw: core::str::from_utf8_unchecked(component.raw),
            parsed: Utf8WindowsPrefix::from_utf8_unchecked(&component.parsed),
        }
    }
}

impl<'a> TryFrom<&'a str> for Utf8WindowsPrefixComponent<'a> {
    type Error = ParseError;

    /// Parses the str slice into a [`Utf8WindowsPrefixComponent`]
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8WindowsPrefix, Utf8WindowsPrefixComponent};
    /// use std::convert::TryFrom;
    ///
    /// let component = Utf8WindowsPrefixComponent::try_from("C:").unwrap();
    /// assert_eq!(component.kind(), Utf8WindowsPrefix::Disk('C'));
    ///
    /// let component = Utf8WindowsPrefixComponent::try_from(r"\\.\BrainInterface").unwrap();
    /// assert_eq!(component.kind(), Utf8WindowsPrefix::DeviceNS("BrainInterface"));
    ///
    /// let component = Utf8WindowsPrefixComponent::try_from(r"\\server\share").unwrap();
    /// assert_eq!(component.kind(), Utf8WindowsPrefix::UNC("server", "share"));
    ///
    /// let component = Utf8WindowsPrefixComponent::try_from(r"\\?\UNC\server\share").unwrap();
    /// assert_eq!(component.kind(), Utf8WindowsPrefix::VerbatimUNC("server", "share"));
    ///
    /// let component = Utf8WindowsPrefixComponent::try_from(r"\\?\C:").unwrap();
    /// assert_eq!(component.kind(), Utf8WindowsPrefix::VerbatimDisk('C'));
    ///
    /// let component = Utf8WindowsPrefixComponent::try_from(r"\\?\pictures").unwrap();
    /// assert_eq!(component.kind(), Utf8WindowsPrefix::Verbatim("pictures"));
    ///
    /// // Parsing something that is not a prefix will fail
    /// assert!(Utf8WindowsPrefixComponent::try_from("hello").is_err());
    ///
    /// // Parsing more than a prefix will fail
    /// assert!(Utf8WindowsPrefixComponent::try_from(r"C:\path").is_err());
    /// ```
    fn try_from(path: &'a str) -> Result<Self, Self::Error> {
        let mut components = Utf8WindowsComponents::new(path);

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

impl<'a> cmp::PartialEq for Utf8WindowsPrefixComponent<'a> {
    #[inline]
    fn eq(&self, other: &Utf8WindowsPrefixComponent<'a>) -> bool {
        cmp::PartialEq::eq(&self.parsed, &other.parsed)
    }
}

impl<'a> cmp::PartialOrd for Utf8WindowsPrefixComponent<'a> {
    #[inline]
    fn partial_cmp(&self, other: &Utf8WindowsPrefixComponent<'a>) -> Option<cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl cmp::Ord for Utf8WindowsPrefixComponent<'_> {
    #[inline]
    fn cmp(&self, other: &Self) -> cmp::Ordering {
        cmp::Ord::cmp(&self.parsed, &other.parsed)
    }
}

impl Hash for Utf8WindowsPrefixComponent<'_> {
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
/// use typed_path::{Utf8WindowsPath, Utf8WindowsComponent, Utf8WindowsPrefix};
/// use typed_path::Utf8WindowsPrefix::*;
///
/// fn get_path_prefix(s: &str) -> Utf8WindowsPrefix {
///     let path = Utf8WindowsPath::new(s);
///     match path.components().next().unwrap() {
///         Utf8WindowsComponent::Prefix(prefix_component) => prefix_component.kind(),
///         _ => panic!(),
///     }
/// }
///
/// assert_eq!(Verbatim("pictures"), get_path_prefix(r"\\?\pictures\kittens"));
/// assert_eq!(VerbatimUNC("server", "share"), get_path_prefix(r"\\?\UNC\server\share"));
/// assert_eq!(VerbatimDisk('C'), get_path_prefix(r"\\?\c:\"));
/// assert_eq!(DeviceNS("BrainInterface"), get_path_prefix(r"\\.\BrainInterface"));
/// assert_eq!(UNC("server", "share"), get_path_prefix(r"\\server\share"));
/// assert_eq!(Disk('C'), get_path_prefix(r"C:\Users\Rust\Pictures\Ferris"));
/// ```
#[derive(Copy, Clone, Debug, Hash, PartialOrd, Ord, PartialEq, Eq)]
pub enum Utf8WindowsPrefix<'a> {
    /// Verbatim prefix, e.g., `\\?\cat_pics`.
    ///
    /// Verbatim prefixes consist of `\\?\` immediately followed by the given
    /// component.
    Verbatim(&'a str),

    /// Verbatim prefix using Windows' _**U**niform **N**aming **C**onvention_,
    /// e.g., `\\?\UNC\server\share`.
    ///
    /// Verbatim UNC prefixes consist of `\\?\UNC\` immediately followed by the
    /// server's hostname and a share name.
    VerbatimUNC(&'a str, &'a str),

    /// Verbatim disk prefix, e.g., `\\?\C:`.
    ///
    /// Verbatim disk prefixes consist of `\\?\` immediately followed by the
    /// drive letter and `:`.
    VerbatimDisk(char),

    /// Device namespace prefix, e.g., `\\.\COM42`.
    ///
    /// Device namespace prefixes consist of `\\.\` (possibly using `/`
    /// instead of `\`), immediately followed by the device name.
    DeviceNS(&'a str),

    /// Prefix using Windows' _**U**niform **N**aming **C**onvention_, e.g.
    /// `\\server\share`.
    ///
    /// UNC prefixes consist of the server's hostname and a share name.
    UNC(&'a str, &'a str),

    /// Prefix `C:` for the given disk drive.
    Disk(char),
}

impl<'a> TryFrom<&'a str> for Utf8WindowsPrefix<'a> {
    type Error = ParseError;

    fn try_from(path: &'a str) -> Result<Self, Self::Error> {
        Ok(Utf8WindowsPrefixComponent::try_from(path)?.kind())
    }
}

impl<'a> Utf8WindowsPrefix<'a> {
    /// Calculates the full byte length of the prefix
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8WindowsPrefix::*;
    ///
    /// // \\?\pictures -> 12 bytes
    /// assert_eq!(Verbatim("pictures").len(), 12);
    ///
    /// // \\?\UNC\server -> 14 bytes
    /// assert_eq!(VerbatimUNC("server", "").len(), 14);
    ///
    /// // \\?\UNC\server\share -> 20 bytes
    /// assert_eq!(VerbatimUNC("server", "share").len(), 20);
    ///
    /// // \\?\c: -> 6 bytes
    /// assert_eq!(VerbatimDisk('C').len(), 6);
    ///
    /// // \\.\BrainInterface -> 18 bytes
    /// assert_eq!(DeviceNS("BrainInterface").len(), 18);
    ///
    /// // \\server\share -> 14 bytes
    /// assert_eq!(UNC("server", "share").len(), 14);
    ///
    /// // C: -> 2 bytes
    /// assert_eq!(Disk('C').len(), 2);
    /// ```
    #[inline]
    #[allow(clippy::len_without_is_empty)]
    pub fn len(&self) -> usize {
        use self::Utf8WindowsPrefix::*;
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
    /// use typed_path::Utf8WindowsPrefix::*;
    ///
    /// assert!(Verbatim("pictures").is_verbatim());
    /// assert!(VerbatimUNC("server", "share").is_verbatim());
    /// assert!(VerbatimDisk('C').is_verbatim());
    /// assert!(!DeviceNS("BrainInterface").is_verbatim());
    /// assert!(!UNC("server", "share").is_verbatim());
    /// assert!(!Disk('C').is_verbatim());
    /// ```
    #[inline]
    pub fn is_verbatim(&self) -> bool {
        use self::Utf8WindowsPrefix::*;
        matches!(*self, Verbatim(_) | VerbatimDisk(_) | VerbatimUNC(..))
    }

    /// Converts a non-UTF-8 [`WindowsPrefix`] to a UTF-8 [`Utf8WindowsPrefix`]
    /// by checking that the prefix contains valid UTF-8.
    ///
    /// # Errors
    ///
    /// Returns `Err` if the prefix component is not UTF-8 with a description as to why the
    /// provided prefix is not UTF-8.
    ///
    /// See the docs for [`Utf8Error`] for more details on the kinds of
    /// errors that can be returned.
    pub fn from_utf8(prefix: &WindowsPrefix<'a>) -> Result<Self, Utf8Error> {
        Ok(match prefix {
            WindowsPrefix::Verbatim(x) => Self::Verbatim(core::str::from_utf8(x)?),
            WindowsPrefix::VerbatimUNC(x, y) => {
                Self::VerbatimUNC(core::str::from_utf8(x)?, core::str::from_utf8(y)?)
            }
            WindowsPrefix::VerbatimDisk(x) => Self::VerbatimDisk(*x as char),
            WindowsPrefix::UNC(x, y) => {
                Self::UNC(core::str::from_utf8(x)?, core::str::from_utf8(y)?)
            }
            WindowsPrefix::DeviceNS(x) => Self::DeviceNS(core::str::from_utf8(x)?),
            WindowsPrefix::Disk(x) => Self::Disk(*x as char),
        })
    }

    /// Converts a non-UTF-8 [`WindowsPrefix`] to a UTF-8 [`Utf8WindowsPrefix`] without checking
    /// that the string contains valid UTF-8.
    ///
    /// See the safe version, [`from_utf8`], for more information.
    ///
    /// # Safety
    ///
    /// The prefix passed in must be valid UTF-8.
    ///
    /// [`from_utf8`]: Utf8WindowsPrefix::from_utf8
    pub unsafe fn from_utf8_unchecked(prefix: &WindowsPrefix<'a>) -> Self {
        match prefix {
            WindowsPrefix::Verbatim(x) => Self::Verbatim(core::str::from_utf8_unchecked(x)),
            WindowsPrefix::VerbatimUNC(x, y) => Self::VerbatimUNC(
                core::str::from_utf8_unchecked(x),
                core::str::from_utf8_unchecked(y),
            ),
            WindowsPrefix::VerbatimDisk(x) => Self::VerbatimDisk(*x as char),
            WindowsPrefix::UNC(x, y) => Self::UNC(
                core::str::from_utf8_unchecked(x),
                core::str::from_utf8_unchecked(y),
            ),
            WindowsPrefix::DeviceNS(x) => Self::DeviceNS(core::str::from_utf8_unchecked(x)),
            WindowsPrefix::Disk(x) => Self::Disk(*x as char),
        }
    }
}
