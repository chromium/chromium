use crate::typed::TypedPath;
use crate::unix::UnixComponent;
use crate::windows::WindowsComponent;
use crate::{private, Component};

/// Byte slice version of [`std::path::Component`] that represents either a Unix or Windows path
/// component.
#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub enum TypedComponent<'a> {
    Unix(UnixComponent<'a>),
    Windows(WindowsComponent<'a>),
}

impl private::Sealed for TypedComponent<'_> {}

impl<'a> TypedComponent<'a> {
    /// Returns path representing this specific component.
    pub fn to_path(&self) -> TypedPath<'a> {
        TypedPath::derive(self.as_bytes())
    }

    /// Extracts the underlying [`[u8]`] slice.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{TypedComponent, TypedPath};
    ///
    /// let path = TypedPath::derive(b"/tmp/foo/../bar.txt");
    /// let components: Vec<_> = path.components().map(|comp| comp.as_bytes()).collect();
    /// assert_eq!(&components, &[
    ///     b"/".as_slice(),
    ///     b"tmp".as_slice(),
    ///     b"foo".as_slice(),
    ///     b"..".as_slice(),
    ///     b"bar.txt".as_slice(),
    /// ]);
    /// ```
    pub fn as_bytes(&self) -> &'a [u8] {
        impl_typed_fn!(self, as_bytes)
    }

    /// Returns true if is the root dir component.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Component, UnixComponent, TypedComponent};
    /// use std::convert::TryFrom;
    ///
    /// let root_dir = TypedComponent::Unix(UnixComponent::try_from(b"/").unwrap());
    /// assert!(root_dir.is_root());
    ///
    /// let normal = TypedComponent::Unix(UnixComponent::try_from(b"file.txt").unwrap());
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
    /// use typed_path::{Component, UnixComponent, TypedComponent};
    /// use std::convert::TryFrom;
    ///
    /// let normal = TypedComponent::Unix(UnixComponent::try_from(b"file.txt").unwrap());
    /// assert!(normal.is_normal());
    ///
    /// let root_dir = TypedComponent::Unix(UnixComponent::try_from(b"/").unwrap());
    /// assert!(!root_dir.is_normal());
    /// ```
    pub fn is_normal(&self) -> bool {
        impl_typed_fn!(self, is_normal)
    }

    /// Returns bytes if is a normal component.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Component, UnixComponent, TypedComponent};
    /// use std::convert::TryFrom;
    ///
    /// let normal = TypedComponent::Unix(UnixComponent::try_from(b"file.txt").unwrap());
    /// assert_eq!(normal.as_normal_bytes(), Some(b"file.txt".as_slice()));
    ///
    /// let root_dir = TypedComponent::Unix(UnixComponent::try_from(b"/").unwrap());
    /// assert_eq!(root_dir.as_normal_bytes(), None);
    /// ```
    pub fn as_normal_bytes(&self) -> Option<&[u8]> {
        match self {
            Self::Unix(UnixComponent::Normal(bytes)) => Some(bytes),
            Self::Windows(WindowsComponent::Normal(bytes)) => Some(bytes),
            _ => None,
        }
    }

    /// Returns true if is a parent directory component.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Component, UnixComponent, TypedComponent};
    /// use std::convert::TryFrom;
    ///
    /// let parent = TypedComponent::Unix(UnixComponent::try_from("..").unwrap());
    /// assert!(parent.is_parent());
    ///
    /// let root_dir = TypedComponent::Unix(UnixComponent::try_from("/").unwrap());
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
    /// use typed_path::{Component, UnixComponent, TypedComponent};
    /// use std::convert::TryFrom;
    ///
    /// let current = TypedComponent::Unix(UnixComponent::try_from(".").unwrap());
    /// assert!(current.is_current());
    ///
    /// let root_dir = TypedComponent::Unix(UnixComponent::try_from("/").unwrap());
    /// assert!(!root_dir.is_current());
    /// ```
    pub fn is_current(&self) -> bool {
        impl_typed_fn!(self, is_current)
    }

    /// Returns byte length of component.
    pub fn len(&self) -> usize {
        impl_typed_fn!(self, len)
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }
}

impl AsRef<[u8]> for TypedComponent<'_> {
    #[inline]
    fn as_ref(&self) -> &[u8] {
        impl_typed_fn!(self, as_ref)
    }
}
