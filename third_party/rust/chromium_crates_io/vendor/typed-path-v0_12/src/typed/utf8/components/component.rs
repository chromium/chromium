use core::fmt;

use crate::typed::Utf8TypedPath;
use crate::unix::Utf8UnixComponent;
use crate::windows::Utf8WindowsComponent;
use crate::{private, Utf8Component};

/// Str slice version of [`std::path::Component`] that represents either a Unix or Windows path
/// component.
#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub enum Utf8TypedComponent<'a> {
    Unix(Utf8UnixComponent<'a>),
    Windows(Utf8WindowsComponent<'a>),
}

impl private::Sealed for Utf8TypedComponent<'_> {}

impl<'a> Utf8TypedComponent<'a> {
    /// Returns path representing this specific component.
    pub fn to_path(&self) -> Utf8TypedPath<'a> {
        Utf8TypedPath::derive(self.as_str())
    }

    /// Extracts the underlying [`str`] slice.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8TypedComponent, Utf8TypedPath};
    ///
    /// let path = Utf8TypedPath::derive("/tmp/foo/../bar.txt");
    /// let components: Vec<_> = path.components().map(|comp| comp.as_str()).collect();
    /// assert_eq!(&components, &[
    ///     "/",
    ///     "tmp",
    ///     "foo",
    ///     "..",
    ///     "bar.txt",
    /// ]);
    /// ```
    pub fn as_str(&self) -> &'a str {
        impl_typed_fn!(self, as_str)
    }

    /// Returns true if is the root dir component.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Component, Utf8UnixComponent, Utf8TypedComponent};
    /// use std::convert::TryFrom;
    ///
    /// let root_dir = Utf8TypedComponent::Unix(Utf8UnixComponent::try_from("/").unwrap());
    /// assert!(root_dir.is_root());
    ///
    /// let normal = Utf8TypedComponent::Unix(Utf8UnixComponent::try_from("file.txt").unwrap());
    /// assert!(!normal.is_root());
    /// ```
    pub fn is_root(&self) -> bool {
        impl_typed_fn!(self, is_root)
    }

    /// Returns true if is a normal component.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Component, Utf8UnixComponent, Utf8TypedComponent};
    /// use std::convert::TryFrom;
    ///
    /// let normal = Utf8TypedComponent::Unix(Utf8UnixComponent::try_from("file.txt").unwrap());
    /// assert!(normal.is_normal());
    ///
    /// let root_dir = Utf8TypedComponent::Unix(Utf8UnixComponent::try_from("/").unwrap());
    /// assert!(!root_dir.is_normal());
    /// ```
    pub fn is_normal(&self) -> bool {
        impl_typed_fn!(self, is_normal)
    }

    /// Returns str if is a normal component.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Component, Utf8UnixComponent, Utf8TypedComponent};
    /// use std::convert::TryFrom;
    ///
    /// let normal = Utf8TypedComponent::Unix(Utf8UnixComponent::try_from("file.txt").unwrap());
    /// assert_eq!(normal.as_normal_str(), Some("file.txt"));
    ///
    /// let root_dir = Utf8TypedComponent::Unix(Utf8UnixComponent::try_from("/").unwrap());
    /// assert_eq!(root_dir.as_normal_str(), None);
    /// ```
    pub fn as_normal_str(&self) -> Option<&str> {
        match self {
            Self::Unix(Utf8UnixComponent::Normal(s)) => Some(s),
            Self::Windows(Utf8WindowsComponent::Normal(s)) => Some(s),
            _ => None,
        }
    }

    /// Returns true if is a parent directory component.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Component, Utf8UnixComponent, Utf8TypedComponent};
    /// use std::convert::TryFrom;
    ///
    /// let parent = Utf8TypedComponent::Unix(Utf8UnixComponent::try_from("..").unwrap());
    /// assert!(parent.is_parent());
    ///
    /// let root_dir = Utf8TypedComponent::Unix(Utf8UnixComponent::try_from("/").unwrap());
    /// assert!(!root_dir.is_parent());
    /// ```
    pub fn is_parent(&self) -> bool {
        impl_typed_fn!(self, is_parent)
    }

    /// Returns true if is the current directory component.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Component, Utf8UnixComponent, Utf8TypedComponent};
    /// use std::convert::TryFrom;
    ///
    /// let current = Utf8TypedComponent::Unix(Utf8UnixComponent::try_from(".").unwrap());
    /// assert!(current.is_current());
    ///
    /// let root_dir = Utf8TypedComponent::Unix(Utf8UnixComponent::try_from("/").unwrap());
    /// assert!(!root_dir.is_current());
    /// ```
    pub fn is_current(&self) -> bool {
        impl_typed_fn!(self, is_current)
    }

    /// Returns str length of component.
    pub fn len(&self) -> usize {
        impl_typed_fn!(self, len)
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }
}

impl fmt::Display for Utf8TypedComponent<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

impl AsRef<[u8]> for Utf8TypedComponent<'_> {
    #[inline]
    fn as_ref(&self) -> &[u8] {
        impl_typed_fn!(self, as_ref)
    }
}

impl AsRef<str> for Utf8TypedComponent<'_> {
    #[inline]
    fn as_ref(&self) -> &str {
        impl_typed_fn!(self, as_ref)
    }
}
