mod prefix;
use core::convert::TryFrom;
use core::fmt;
use core::str::Utf8Error;

pub use prefix::{Utf8WindowsPrefix, Utf8WindowsPrefixComponent};

use crate::windows::constants::{
    CURRENT_DIR_STR, DISALLOWED_FILENAME_CHARS, PARENT_DIR_STR, SEPARATOR_STR,
};
use crate::windows::{Utf8WindowsComponents, WindowsComponent};
use crate::{private, ParseError, Utf8Component, Utf8Encoding, Utf8Path};

/// `str` slice version of [`std::path::Component`] that represents a Windows-specific component
#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash, Debug)]
pub enum Utf8WindowsComponent<'a> {
    Prefix(Utf8WindowsPrefixComponent<'a>),
    RootDir,
    CurDir,
    ParentDir,
    Normal(&'a str),
}

impl private::Sealed for Utf8WindowsComponent<'_> {}

impl<'a> Utf8WindowsComponent<'a> {
    /// Returns path representing this specific component
    pub fn as_path<T>(&self) -> &Utf8Path<T>
    where
        T: Utf8Encoding,
    {
        Utf8Path::new(self.as_str())
    }

    /// Converts a non-UTF-8 [`WindowsComponent`] to a UTF-8 [`Utf8WindowsComponent`]  by checking that
    /// the component contains valid UTF-8.
    ///
    /// # Errors
    ///
    /// Returns `Err` if the component is not UTF-8 with a description as to why the
    /// provided component is not UTF-8.
    ///
    /// # Examples
    ///
    /// Basic usage:
    ///
    /// ```
    /// use typed_path::{Utf8Component, WindowsComponent, Utf8WindowsComponent};
    ///
    /// // some bytes, in a vector
    /// let component = WindowsComponent::Normal(&[240, 159, 146, 150]);
    ///
    /// // We know these bytes are valid, so just use `unwrap()`.
    /// let utf8_component = Utf8WindowsComponent::from_utf8(&component).unwrap();
    ///
    /// assert_eq!("💖", utf8_component.as_str());
    /// ```
    ///
    /// Incorrect bytes:
    ///
    /// ```
    /// use typed_path::{WindowsComponent, Utf8WindowsComponent};
    ///
    /// // some invalid bytes, in a vector
    /// let component = WindowsComponent::Normal(&[0, 159, 146, 150]);
    ///
    /// assert!(Utf8WindowsComponent::from_utf8(&component).is_err());
    /// ```
    ///
    /// See the docs for [`Utf8Error`] for more details on the kinds of
    /// errors that can be returned.
    pub fn from_utf8(component: &WindowsComponent<'a>) -> Result<Self, Utf8Error> {
        Ok(match component {
            WindowsComponent::Prefix(x) => Self::Prefix(Utf8WindowsPrefixComponent::from_utf8(x)?),
            WindowsComponent::RootDir => Self::RootDir,
            WindowsComponent::ParentDir => Self::ParentDir,
            WindowsComponent::CurDir => Self::CurDir,
            WindowsComponent::Normal(x) => Self::Normal(core::str::from_utf8(x)?),
        })
    }

    /// Converts a non-UTF-8 [`WindowsComponent`] to a UTF-8 [`Utf8WindowsComponent`] without checking
    /// that the string contains valid UTF-8.
    ///
    /// See the safe version, [`from_utf8`], for more information.
    ///
    /// [`from_utf8`]: Utf8WindowsComponent::from_utf8
    ///
    /// # Safety
    ///
    /// The bytes passed in must be valid UTF-8.
    ///
    /// # Examples
    ///
    /// Basic usage:
    ///
    /// ```
    /// use typed_path::{Utf8Component, WindowsComponent, Utf8WindowsComponent};
    ///
    /// // some bytes, in a vector
    /// let component = WindowsComponent::Normal(&[240, 159, 146, 150]);
    ///
    /// let utf8_component = unsafe {
    ///     Utf8WindowsComponent::from_utf8_unchecked(&component)
    /// };
    ///
    /// assert_eq!("💖", utf8_component.as_str());
    /// ```
    pub unsafe fn from_utf8_unchecked(component: &WindowsComponent<'a>) -> Self {
        match component {
            WindowsComponent::Prefix(x) => {
                Self::Prefix(Utf8WindowsPrefixComponent::from_utf8_unchecked(x))
            }
            WindowsComponent::RootDir => Self::RootDir,
            WindowsComponent::ParentDir => Self::ParentDir,
            WindowsComponent::CurDir => Self::CurDir,
            WindowsComponent::Normal(x) => Self::Normal(core::str::from_utf8_unchecked(x)),
        }
    }

    /// Returns true if represents a prefix
    pub fn is_prefix(&self) -> bool {
        matches!(self, Self::Prefix(_))
    }

    /// Converts from `Utf8WindowsComponent` to [`Option<Utf8WindowsPrefixComponent>`]
    ///
    /// Converts `self` into an [`Option<Utf8WindowsPrefixComponent>`], consuming `self`, and
    /// discarding if not a [`Utf8WindowsPrefixComponent`]
    pub fn prefix(self) -> Option<Utf8WindowsPrefixComponent<'a>> {
        match self {
            Self::Prefix(p) => Some(p),
            _ => None,
        }
    }

    /// Converts from `Utf8WindowsComponent` to [`Option<Utf8WindowsPrefix>`]
    ///
    /// Converts `self` into an [`Option<Utf8WindowsPrefix>`], consuming `self`, and
    /// discarding if not a [`Utf8WindowsPrefixComponent`] whose [`kind`] method we invoke
    ///
    /// [`kind`]: Utf8WindowsPrefixComponent::kind
    pub fn prefix_kind(self) -> Option<Utf8WindowsPrefix<'a>> {
        self.prefix().map(|p| p.kind())
    }
}

impl<'a> Utf8Component<'a> for Utf8WindowsComponent<'a> {
    /// Extracts the underlying [`str`] slice
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Component, Utf8WindowsPath};
    ///
    /// let path = Utf8WindowsPath::new(r"C:\tmp\foo\..\bar.txt");
    /// let components: Vec<_> = path.components().map(|comp| comp.as_str()).collect();
    /// assert_eq!(&components, &["C:", r"\", "tmp", "foo", "..", "bar.txt"]);
    /// ```
    fn as_str(&self) -> &'a str {
        match self {
            Self::Prefix(p) => p.as_str(),
            Self::RootDir => SEPARATOR_STR,
            Self::CurDir => CURRENT_DIR_STR,
            Self::ParentDir => PARENT_DIR_STR,
            Self::Normal(path) => path,
        }
    }

    /// Root is one of two situations
    ///
    /// * Is the root separator, e.g. `\windows`
    /// * Is a non-disk prefix, e.g. `\\server\share`
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Component, Utf8WindowsComponent};
    /// use std::convert::TryFrom;
    ///
    /// let root_dir = Utf8WindowsComponent::try_from(r"\").unwrap();
    /// assert!(root_dir.is_root());
    ///
    /// let non_disk_prefix = Utf8WindowsComponent::try_from(r"\\?\pictures").unwrap();
    /// assert!(non_disk_prefix.is_root());
    ///
    /// let disk_prefix = Utf8WindowsComponent::try_from("C:").unwrap();
    /// assert!(!disk_prefix.is_root());
    ///
    /// let normal = Utf8WindowsComponent::try_from("file.txt").unwrap();
    /// assert!(!normal.is_root());
    /// ```
    fn is_root(&self) -> bool {
        match self {
            Self::RootDir => true,
            Self::Prefix(prefix) => !matches!(prefix.kind(), Utf8WindowsPrefix::Disk(_)),
            _ => false,
        }
    }

    /// Returns true if component is normal
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Component, Utf8WindowsComponent};
    /// use std::convert::TryFrom;
    ///
    /// let normal = Utf8WindowsComponent::try_from("file.txt").unwrap();
    /// assert!(normal.is_normal());
    ///
    /// let root_dir = Utf8WindowsComponent::try_from(r"\").unwrap();
    /// assert!(!root_dir.is_normal());
    /// ```
    fn is_normal(&self) -> bool {
        matches!(self, Self::Normal(_))
    }

    /// Returns true if is a parent directory component
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Component, Utf8WindowsComponent};
    /// use std::convert::TryFrom;
    ///
    /// let parent = Utf8WindowsComponent::try_from("..").unwrap();
    /// assert!(parent.is_parent());
    ///
    /// let root_dir = Utf8WindowsComponent::try_from(r"\").unwrap();
    /// assert!(!root_dir.is_parent());
    /// ```
    fn is_parent(&self) -> bool {
        matches!(self, Self::ParentDir)
    }

    /// Returns true if is the current directory component
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Component, Utf8WindowsComponent};
    /// use std::convert::TryFrom;
    ///
    /// let current = Utf8WindowsComponent::try_from(".").unwrap();
    /// assert!(current.is_current());
    ///
    /// let root_dir = Utf8WindowsComponent::try_from(r"\").unwrap();
    /// assert!(!root_dir.is_current());
    /// ```
    fn is_current(&self) -> bool {
        matches!(self, Self::CurDir)
    }

    /// Returns true if this component is valid.
    ///
    /// A component can only be invalid if it represents a normal component with characters that
    /// are disallowed by the encoding.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Component, Utf8WindowsComponent};
    /// use std::convert::TryFrom;
    ///
    /// assert!(Utf8WindowsComponent::try_from("c:").unwrap().is_valid());
    /// assert!(Utf8WindowsComponent::RootDir.is_valid());
    /// assert!(Utf8WindowsComponent::ParentDir.is_valid());
    /// assert!(Utf8WindowsComponent::CurDir.is_valid());
    /// assert!(Utf8WindowsComponent::Normal("abc").is_valid());
    /// assert!(!Utf8WindowsComponent::Normal("|").is_valid());
    /// ```
    fn is_valid(&self) -> bool {
        match self {
            Self::Prefix(_) | Self::RootDir | Self::ParentDir | Self::CurDir => true,
            Self::Normal(s) => !s.chars().any(|c| DISALLOWED_FILENAME_CHARS.contains(&c)),
        }
    }

    fn len(&self) -> usize {
        self.as_str().len()
    }

    /// Returns the root directory component.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Component, Utf8WindowsComponent};
    ///
    /// assert_eq!(Utf8WindowsComponent::root(), Utf8WindowsComponent::RootDir);
    /// ```
    fn root() -> Self {
        Self::RootDir
    }

    /// Returns the parent directory component.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Component, Utf8WindowsComponent};
    ///
    /// assert_eq!(Utf8WindowsComponent::parent(), Utf8WindowsComponent::ParentDir);
    /// ```
    fn parent() -> Self {
        Self::ParentDir
    }

    /// Returns the current directory component.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Component, Utf8WindowsComponent};
    ///
    /// assert_eq!(Utf8WindowsComponent::current(), Utf8WindowsComponent::CurDir);
    /// ```
    fn current() -> Self {
        Self::CurDir
    }
}

impl fmt::Display for Utf8WindowsComponent<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

impl AsRef<[u8]> for Utf8WindowsComponent<'_> {
    #[inline]
    fn as_ref(&self) -> &[u8] {
        self.as_str().as_bytes()
    }
}

impl AsRef<str> for Utf8WindowsComponent<'_> {
    #[inline]
    fn as_ref(&self) -> &str {
        self.as_str()
    }
}

impl<T> AsRef<Utf8Path<T>> for Utf8WindowsComponent<'_>
where
    T: Utf8Encoding,
{
    #[inline]
    fn as_ref(&self) -> &Utf8Path<T> {
        Utf8Path::new(self.as_str())
    }
}

impl<'a> TryFrom<&'a str> for Utf8WindowsComponent<'a> {
    type Error = ParseError;

    /// Parses the byte slice into a [`Utf8WindowsComponent`]
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8WindowsComponent, Utf8WindowsPrefix};
    /// use std::convert::TryFrom;
    ///
    /// // Supports parsing Windows prefixes
    /// let component = Utf8WindowsComponent::try_from("c:").unwrap();
    /// assert_eq!(component.prefix_kind(), Some(Utf8WindowsPrefix::Disk('C')));
    ///
    /// // Supports parsing standard windows path components
    /// assert_eq!(Utf8WindowsComponent::try_from(r"\"), Ok(Utf8WindowsComponent::RootDir));
    /// assert_eq!(Utf8WindowsComponent::try_from("."), Ok(Utf8WindowsComponent::CurDir));
    /// assert_eq!(Utf8WindowsComponent::try_from(".."), Ok(Utf8WindowsComponent::ParentDir));
    /// assert_eq!(Utf8WindowsComponent::try_from(r"file.txt"), Ok(Utf8WindowsComponent::Normal("file.txt")));
    /// assert_eq!(Utf8WindowsComponent::try_from(r"dir\"), Ok(Utf8WindowsComponent::Normal("dir")));
    ///
    /// // Parsing more than one component will fail
    /// assert!(Utf8WindowsComponent::try_from(r"\file").is_err());
    /// ```
    fn try_from(path: &'a str) -> Result<Self, Self::Error> {
        let mut components = Utf8WindowsComponents::new(path);

        let component = components.next().ok_or("no component found")?;
        if components.next().is_some() {
            return Err("found more than one component");
        }

        Ok(component)
    }
}
