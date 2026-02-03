use crate::unix::constants::{CURRENT_DIR, DISALLOWED_FILENAME_BYTES, PARENT_DIR, SEPARATOR_STR};
use crate::unix::UnixComponents;
use crate::{private, Component, Encoding, ParseError, Path};

/// Byte slice version of [`std::path::Component`] that represents a Unix-specific component
#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub enum UnixComponent<'a> {
    RootDir,
    CurDir,
    ParentDir,
    Normal(&'a [u8]),
}

impl private::Sealed for UnixComponent<'_> {}

impl UnixComponent<'_> {
    /// Returns path representing this specific component
    pub fn as_path<T>(&self) -> &Path<T>
    where
        T: Encoding,
    {
        Path::new(self.as_bytes())
    }
}

impl<'a> Component<'a> for UnixComponent<'a> {
    /// Extracts the underlying [`[u8]`] slice
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Component, UnixPath};
    ///
    /// let path = UnixPath::new(b"/tmp/foo/../bar.txt");
    /// let components: Vec<_> = path.components().map(|comp| comp.as_bytes()).collect();
    /// assert_eq!(&components, &[
    ///     b"/".as_slice(),
    ///     b"tmp".as_slice(),
    ///     b"foo".as_slice(),
    ///     b"..".as_slice(),
    ///     b"bar.txt".as_slice(),
    /// ]);
    /// ```
    fn as_bytes(&self) -> &'a [u8] {
        match self {
            Self::RootDir => SEPARATOR_STR.as_bytes(),
            Self::CurDir => CURRENT_DIR,
            Self::ParentDir => PARENT_DIR,
            Self::Normal(path) => path,
        }
    }

    /// Returns true if is the root dir component
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Component, UnixComponent};
    /// use std::convert::TryFrom;
    ///
    /// let root_dir = UnixComponent::try_from(b"/").unwrap();
    /// assert!(root_dir.is_root());
    ///
    /// let normal = UnixComponent::try_from(b"file.txt").unwrap();
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
    /// use typed_path::{Component, UnixComponent};
    /// use std::convert::TryFrom;
    ///
    /// let normal = UnixComponent::try_from(b"file.txt").unwrap();
    /// assert!(normal.is_normal());
    ///
    /// let root_dir = UnixComponent::try_from(b"/").unwrap();
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
    /// use typed_path::{Component, UnixComponent};
    /// use std::convert::TryFrom;
    ///
    /// let parent = UnixComponent::try_from("..").unwrap();
    /// assert!(parent.is_parent());
    ///
    /// let root_dir = UnixComponent::try_from("/").unwrap();
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
    /// use typed_path::{Component, UnixComponent};
    /// use std::convert::TryFrom;
    ///
    /// let current = UnixComponent::try_from(".").unwrap();
    /// assert!(current.is_current());
    ///
    /// let root_dir = UnixComponent::try_from("/").unwrap();
    /// assert!(!root_dir.is_current());
    /// ```
    fn is_current(&self) -> bool {
        matches!(self, Self::CurDir)
    }

    /// Returns true if this component is valid.
    ///
    /// A component can only be invalid if it represents a normal component with bytes that are
    /// disallowed by the encoding.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Component, UnixComponent};
    ///
    /// assert!(UnixComponent::RootDir.is_valid());
    /// assert!(UnixComponent::ParentDir.is_valid());
    /// assert!(UnixComponent::CurDir.is_valid());
    /// assert!(UnixComponent::Normal(b"abc").is_valid());
    /// assert!(!UnixComponent::Normal(b"\0").is_valid());
    /// ```
    fn is_valid(&self) -> bool {
        match self {
            Self::RootDir | Self::ParentDir | Self::CurDir => true,
            Self::Normal(bytes) => !bytes.iter().any(|b| DISALLOWED_FILENAME_BYTES.contains(b)),
        }
    }

    fn len(&self) -> usize {
        self.as_bytes().len()
    }

    /// Returns the root directory component.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Component, UnixComponent};
    ///
    /// assert_eq!(UnixComponent::root(), UnixComponent::RootDir);
    /// ```
    fn root() -> Self {
        Self::RootDir
    }

    /// Returns the parent directory component.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Component, UnixComponent};
    ///
    /// assert_eq!(UnixComponent::parent(), UnixComponent::ParentDir);
    /// ```
    fn parent() -> Self {
        Self::ParentDir
    }

    /// Returns the current directory component.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Component, UnixComponent};
    ///
    /// assert_eq!(UnixComponent::current(), UnixComponent::CurDir);
    /// ```
    fn current() -> Self {
        Self::CurDir
    }
}

impl AsRef<[u8]> for UnixComponent<'_> {
    #[inline]
    fn as_ref(&self) -> &[u8] {
        self.as_bytes()
    }
}

impl<T> AsRef<Path<T>> for UnixComponent<'_>
where
    T: Encoding,
{
    #[inline]
    fn as_ref(&self) -> &Path<T> {
        Path::new(self.as_bytes())
    }
}

impl<'a> TryFrom<&'a [u8]> for UnixComponent<'a> {
    type Error = ParseError;

    /// Parses the byte slice into a [`UnixComponent`]
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::UnixComponent;
    /// use std::convert::TryFrom;
    ///
    /// // Supports parsing standard unix path components
    /// assert_eq!(UnixComponent::try_from(b"/"), Ok(UnixComponent::RootDir));
    /// assert_eq!(UnixComponent::try_from(b"."), Ok(UnixComponent::CurDir));
    /// assert_eq!(UnixComponent::try_from(b".."), Ok(UnixComponent::ParentDir));
    /// assert_eq!(UnixComponent::try_from(b"file.txt"), Ok(UnixComponent::Normal(b"file.txt")));
    /// assert_eq!(UnixComponent::try_from(b"dir/"), Ok(UnixComponent::Normal(b"dir")));
    ///
    /// // Parsing more than one component will fail
    /// assert!(UnixComponent::try_from(b"/file").is_err());
    /// ```
    fn try_from(path: &'a [u8]) -> Result<Self, Self::Error> {
        let mut components = UnixComponents::new(path);

        let component = components.next().ok_or("no component found")?;
        if components.next().is_some() {
            return Err("found more than one component");
        }

        Ok(component)
    }
}

impl<'a, const N: usize> TryFrom<&'a [u8; N]> for UnixComponent<'a> {
    type Error = ParseError;

    fn try_from(path: &'a [u8; N]) -> Result<Self, Self::Error> {
        Self::try_from(path.as_slice())
    }
}

impl<'a> TryFrom<&'a str> for UnixComponent<'a> {
    type Error = ParseError;

    fn try_from(path: &'a str) -> Result<Self, Self::Error> {
        Self::try_from(path.as_bytes())
    }
}

#[cfg(feature = "std")]
impl<'a> TryFrom<UnixComponent<'a>> for std::path::Component<'a> {
    type Error = UnixComponent<'a>;

    /// Attempts to convert a [`UnixComponent`] into a [`std::path::Component`], returning a result
    /// containing the new path when successful or the original path when failed
    ///
    /// # Examples
    ///
    /// ```
    /// use std::convert::TryFrom;
    /// use std::ffi::OsStr;
    /// use std::path::Component;
    /// use typed_path::UnixComponent;
    ///
    /// let component = Component::try_from(UnixComponent::RootDir).unwrap();
    /// assert_eq!(component, Component::RootDir);
    ///
    /// let component = Component::try_from(UnixComponent::CurDir).unwrap();
    /// assert_eq!(component, Component::CurDir);
    ///
    /// let component = Component::try_from(UnixComponent::ParentDir).unwrap();
    /// assert_eq!(component, Component::ParentDir);
    ///
    /// let component = Component::try_from(UnixComponent::Normal(b"file.txt")).unwrap();
    /// assert_eq!(component, Component::Normal(OsStr::new("file.txt")));
    /// ```
    fn try_from(component: UnixComponent<'a>) -> Result<Self, Self::Error> {
        match &component {
            UnixComponent::RootDir => Ok(Self::RootDir),
            UnixComponent::CurDir => Ok(Self::CurDir),
            UnixComponent::ParentDir => Ok(Self::ParentDir),
            UnixComponent::Normal(x) => Ok(Self::Normal(std::ffi::OsStr::new(
                std::str::from_utf8(x).map_err(|_| component)?,
            ))),
        }
    }
}

#[cfg(feature = "std")]
impl<'a> TryFrom<std::path::Component<'a>> for UnixComponent<'a> {
    type Error = std::path::Component<'a>;

    /// Attempts to convert a [`std::path::Component`] into a [`UnixComponent`], returning a result
    /// containing the new component when successful or the original component when failed
    ///
    /// # Examples
    ///
    /// ```
    /// use std::convert::TryFrom;
    /// use std::ffi::OsStr;
    /// use std::path::Component;
    /// use typed_path::UnixComponent;
    ///
    /// let component = UnixComponent::try_from(Component::RootDir).unwrap();
    /// assert_eq!(component, UnixComponent::RootDir);
    ///
    /// let component = UnixComponent::try_from(Component::CurDir).unwrap();
    /// assert_eq!(component, UnixComponent::CurDir);
    ///
    /// let component = UnixComponent::try_from(Component::ParentDir).unwrap();
    /// assert_eq!(component, UnixComponent::ParentDir);
    ///
    /// let component = UnixComponent::try_from(Component::Normal(OsStr::new("file.txt"))).unwrap();
    /// assert_eq!(component, UnixComponent::Normal(b"file.txt"));
    /// ```
    fn try_from(component: std::path::Component<'a>) -> Result<Self, Self::Error> {
        match &component {
            std::path::Component::Prefix(_) => Err(component),
            std::path::Component::RootDir => Ok(Self::RootDir),
            std::path::Component::CurDir => Ok(Self::CurDir),
            std::path::Component::ParentDir => Ok(Self::ParentDir),
            std::path::Component::Normal(x) => {
                Ok(Self::Normal(x.to_str().ok_or(component)?.as_bytes()))
            }
        }
    }
}
