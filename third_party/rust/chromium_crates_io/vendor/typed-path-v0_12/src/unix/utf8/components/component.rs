use core::fmt;
use core::str::Utf8Error;

use crate::unix::constants::{
    CURRENT_DIR_STR, DISALLOWED_FILENAME_CHARS, PARENT_DIR_STR, SEPARATOR_STR,
};
use crate::unix::{UnixComponent, Utf8UnixComponents};
use crate::{private, ParseError, Utf8Component, Utf8Encoding, Utf8Path};

/// `str` slice version of [`std::path::Component`] that represents a Unix-specific component
#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub enum Utf8UnixComponent<'a> {
    RootDir,
    CurDir,
    ParentDir,
    Normal(&'a str),
}

impl<'a> Utf8UnixComponent<'a> {
    /// Converts a non-UTF-8 [`UnixComponent`] to a UTF-8 [`Utf8UnixComponent`]  by checking that
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
    /// use typed_path::{Utf8Component, UnixComponent, Utf8UnixComponent};
    ///
    /// // some bytes, in a vector
    /// let component = UnixComponent::Normal(&[240, 159, 146, 150]);
    ///
    /// // We know these bytes are valid, so just use `unwrap()`.
    /// let utf8_component = Utf8UnixComponent::from_utf8(&component).unwrap();
    ///
    /// assert_eq!("💖", utf8_component.as_str());
    /// ```
    ///
    /// Incorrect bytes:
    ///
    /// ```
    /// use typed_path::{UnixComponent, Utf8UnixComponent};
    ///
    /// // some invalid bytes, in a vector
    /// let component = UnixComponent::Normal(&[0, 159, 146, 150]);
    ///
    /// assert!(Utf8UnixComponent::from_utf8(&component).is_err());
    /// ```
    ///
    /// See the docs for [`Utf8Error`] for more details on the kinds of
    /// errors that can be returned.
    pub fn from_utf8(component: &UnixComponent<'a>) -> Result<Self, Utf8Error> {
        Ok(match component {
            UnixComponent::RootDir => Self::RootDir,
            UnixComponent::ParentDir => Self::ParentDir,
            UnixComponent::CurDir => Self::CurDir,
            UnixComponent::Normal(x) => Self::Normal(core::str::from_utf8(x)?),
        })
    }

    /// Converts a non-UTF-8 [`UnixComponent`] to a UTF-8 [`Utf8UnixComponent`] without checking
    /// that the string contains valid UTF-8.
    ///
    /// See the safe version, [`from_utf8`], for more information.
    ///
    /// [`from_utf8`]: Utf8UnixComponent::from_utf8
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
    /// use typed_path::{Utf8Component, UnixComponent, Utf8UnixComponent};
    ///
    /// // some bytes, in a vector
    /// let component = UnixComponent::Normal(&[240, 159, 146, 150]);
    ///
    /// let utf8_component = unsafe {
    ///     Utf8UnixComponent::from_utf8_unchecked(&component)
    /// };
    ///
    /// assert_eq!("💖", utf8_component.as_str());
    /// ```
    pub unsafe fn from_utf8_unchecked(component: &UnixComponent<'a>) -> Self {
        match component {
            UnixComponent::RootDir => Self::RootDir,
            UnixComponent::ParentDir => Self::ParentDir,
            UnixComponent::CurDir => Self::CurDir,
            UnixComponent::Normal(x) => Self::Normal(core::str::from_utf8_unchecked(x)),
        }
    }
}

impl private::Sealed for Utf8UnixComponent<'_> {}

impl Utf8UnixComponent<'_> {
    /// Returns path representing this specific component
    pub fn as_path<T>(&self) -> &Utf8Path<T>
    where
        T: Utf8Encoding,
    {
        Utf8Path::new(self.as_str())
    }
}

impl<'a> Utf8Component<'a> for Utf8UnixComponent<'a> {
    /// Extracts the underlying [`str`] slice
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Component, Utf8UnixPath};
    ///
    /// let path = Utf8UnixPath::new("/tmp/foo/../bar.txt");
    /// let components: Vec<_> = path.components().map(|comp| comp.as_str()).collect();
    /// assert_eq!(&components, &["/", "tmp", "foo", "..", "bar.txt"]);
    /// ```
    fn as_str(&self) -> &'a str {
        match self {
            Self::RootDir => SEPARATOR_STR,
            Self::CurDir => CURRENT_DIR_STR,
            Self::ParentDir => PARENT_DIR_STR,
            Self::Normal(path) => path,
        }
    }

    /// Returns true if is the root dir component
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Component, Utf8UnixComponent};
    /// use std::convert::TryFrom;
    ///
    /// let root_dir = Utf8UnixComponent::try_from("/").unwrap();
    /// assert!(root_dir.is_root());
    ///
    /// let normal = Utf8UnixComponent::try_from("file.txt").unwrap();
    /// assert!(!normal.is_root());
    /// ```
    fn is_root(&self) -> bool {
        matches!(self, Self::RootDir)
    }

    /// Returns true if is a normal component
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Component, Utf8UnixComponent};
    /// use std::convert::TryFrom;
    ///
    /// let normal = Utf8UnixComponent::try_from("file.txt").unwrap();
    /// assert!(normal.is_normal());
    ///
    /// let root_dir = Utf8UnixComponent::try_from("/").unwrap();
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
    /// use typed_path::{Utf8Component, Utf8UnixComponent};
    /// use std::convert::TryFrom;
    ///
    /// let parent = Utf8UnixComponent::try_from("..").unwrap();
    /// assert!(parent.is_parent());
    ///
    /// let root_dir = Utf8UnixComponent::try_from("/").unwrap();
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
    /// use typed_path::{Utf8Component, Utf8UnixComponent};
    /// use std::convert::TryFrom;
    ///
    /// let current = Utf8UnixComponent::try_from(".").unwrap();
    /// assert!(current.is_current());
    ///
    /// let root_dir = Utf8UnixComponent::try_from("/").unwrap();
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
    /// use typed_path::{Utf8Component, Utf8UnixComponent};
    ///
    /// assert!(Utf8UnixComponent::RootDir.is_valid());
    /// assert!(Utf8UnixComponent::ParentDir.is_valid());
    /// assert!(Utf8UnixComponent::CurDir.is_valid());
    /// assert!(Utf8UnixComponent::Normal("abc").is_valid());
    /// assert!(!Utf8UnixComponent::Normal("\0").is_valid());
    /// ```
    fn is_valid(&self) -> bool {
        match self {
            Self::RootDir | Self::ParentDir | Self::CurDir => true,
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
    /// use typed_path::{Utf8Component, Utf8UnixComponent};
    ///
    /// assert_eq!(Utf8UnixComponent::root(), Utf8UnixComponent::RootDir);
    /// ```
    fn root() -> Self {
        Self::RootDir
    }

    /// Returns the parent directory component.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Component, Utf8UnixComponent};
    ///
    /// assert_eq!(Utf8UnixComponent::parent(), Utf8UnixComponent::ParentDir);
    /// ```
    fn parent() -> Self {
        Self::ParentDir
    }

    /// Returns the current directory component.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Component, Utf8UnixComponent};
    ///
    /// assert_eq!(Utf8UnixComponent::current(), Utf8UnixComponent::CurDir);
    /// ```
    fn current() -> Self {
        Self::CurDir
    }
}

impl fmt::Display for Utf8UnixComponent<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

impl AsRef<[u8]> for Utf8UnixComponent<'_> {
    #[inline]
    fn as_ref(&self) -> &[u8] {
        self.as_str().as_bytes()
    }
}

impl AsRef<str> for Utf8UnixComponent<'_> {
    #[inline]
    fn as_ref(&self) -> &str {
        self.as_str()
    }
}

impl<T> AsRef<Utf8Path<T>> for Utf8UnixComponent<'_>
where
    T: Utf8Encoding,
{
    #[inline]
    fn as_ref(&self) -> &Utf8Path<T> {
        Utf8Path::new(self.as_str())
    }
}

impl<'a> TryFrom<UnixComponent<'a>> for Utf8UnixComponent<'a> {
    type Error = Utf8Error;

    #[inline]
    fn try_from(component: UnixComponent<'a>) -> Result<Self, Self::Error> {
        Self::from_utf8(&component)
    }
}

impl<'a> TryFrom<&'a str> for Utf8UnixComponent<'a> {
    type Error = ParseError;

    /// Parses the `str` slice into a [`Utf8UnixComponent`]
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8UnixComponent;
    /// use std::convert::TryFrom;
    ///
    /// // Supports parsing standard unix path components
    /// assert_eq!(Utf8UnixComponent::try_from("/"), Ok(Utf8UnixComponent::RootDir));
    /// assert_eq!(Utf8UnixComponent::try_from("."), Ok(Utf8UnixComponent::CurDir));
    /// assert_eq!(Utf8UnixComponent::try_from(".."), Ok(Utf8UnixComponent::ParentDir));
    /// assert_eq!(Utf8UnixComponent::try_from("file.txt"), Ok(Utf8UnixComponent::Normal("file.txt")));
    /// assert_eq!(Utf8UnixComponent::try_from("dir/"), Ok(Utf8UnixComponent::Normal("dir")));
    ///
    /// // Parsing more than one component will fail
    /// assert!(Utf8UnixComponent::try_from("/file").is_err());
    /// ```
    fn try_from(path: &'a str) -> Result<Self, Self::Error> {
        let mut components = Utf8UnixComponents::new(path);

        let component = components.next().ok_or("no component found")?;
        if components.next().is_some() {
            return Err("found more than one component");
        }

        Ok(component)
    }
}
