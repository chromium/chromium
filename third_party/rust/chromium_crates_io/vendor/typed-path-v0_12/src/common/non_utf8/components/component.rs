use core::fmt;

use crate::private;

/// Interface representing a component in a [`Path`]
///
/// [`Path`]: crate::Path
pub trait Component<'a>:
    AsRef<[u8]> + Clone + fmt::Debug + PartialEq + Eq + PartialOrd + Ord + private::Sealed
{
    /// Extracts the underlying [`[u8]`] slice
    fn as_bytes(&self) -> &'a [u8];

    /// Returns true if this component is the root component, meaning
    /// there are no more components before this one
    ///
    /// Use cases are for the root dir separator on Windows and Unix as
    /// well as Windows [`std::path::PrefixComponent`]
    ///
    /// # Examples
    ///
    /// `/my/../path/./here.txt` has the components on Unix of
    ///
    /// * `UnixComponent::RootDir` - `is_root() == true`
    /// * `UnixComponent::ParentDir` - `is_root() == false`
    /// * `UnixComponent::CurDir` - `is_root() == false`
    /// * `UnixComponent::Normal(b"here.txt")` - `is_root() == false`
    fn is_root(&self) -> bool;

    /// Returns true if this component represents a normal part of the path
    ///
    /// # Examples
    ///
    /// `/my/../path/./here.txt` has the components on Unix of
    ///
    /// * `UnixComponent::RootDir` - `is_normal() == false`
    /// * `UnixComponent::ParentDir` - `is_normal() == false`
    /// * `UnixComponent::CurDir` - `is_normal() == false`
    /// * `UnixComponent::Normal(b"here.txt")` - `is_normal() == true`
    fn is_normal(&self) -> bool;

    /// Returns true if this component represents a relative representation of a parent directory
    ///
    /// # Examples
    ///
    /// `/my/../path/./here.txt` has the components on Unix of
    ///
    /// * `UnixComponent::RootDir` - `is_parent() == false`
    /// * `UnixComponent::ParentDir` - `is_parent() == true`
    /// * `UnixComponent::CurDir` - `is_parent() == false`
    /// * `UnixComponent::Normal("here.txt")` - `is_parent() == false`
    fn is_parent(&self) -> bool;

    /// Returns true if this component represents a relative representation of the current
    /// directory
    ///
    /// # Examples
    ///
    /// `/my/../path/./here.txt` has the components on Unix of
    ///
    /// * `UnixComponent::RootDir` - `is_current() == false`
    /// * `UnixComponent::ParentDir` - `is_current() == false`
    /// * `UnixComponent::CurDir` - `is_current() == true`
    /// * `UnixComponent::Normal("here.txt")` - `is_current() == false`
    fn is_current(&self) -> bool;

    /// Returns true if this component is valid. A component can only be invalid if it represents a
    /// normal component with bytes that are disallowed by the encoding.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Component, UnixComponent, WindowsComponent};
    ///
    /// assert!(UnixComponent::RootDir.is_valid());
    /// assert!(UnixComponent::ParentDir.is_valid());
    /// assert!(UnixComponent::CurDir.is_valid());
    /// assert!(UnixComponent::Normal(b"abc").is_valid());
    /// assert!(!UnixComponent::Normal(b"\0").is_valid());
    ///
    /// assert!(WindowsComponent::RootDir.is_valid());
    /// assert!(WindowsComponent::ParentDir.is_valid());
    /// assert!(WindowsComponent::CurDir.is_valid());
    /// assert!(WindowsComponent::Normal(b"abc").is_valid());
    /// assert!(!WindowsComponent::Normal(b"|").is_valid());
    /// ```
    fn is_valid(&self) -> bool;

    /// Returns size of component in bytes
    fn len(&self) -> usize;

    /// Returns true if component represents an empty byte slice
    fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Returns a root [`Component`].
    fn root() -> Self;

    /// Returns a parent directory [`Component`].
    fn parent() -> Self;

    /// Returns a current directory [`Component`].
    fn current() -> Self;
}
