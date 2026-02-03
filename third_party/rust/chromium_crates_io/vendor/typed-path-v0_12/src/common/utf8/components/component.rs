use core::fmt;

use crate::private;

/// Interface representing a component in a [`Utf8Path`]
///
/// [`Utf8Path`]: crate::Utf8Path
pub trait Utf8Component<'a>:
    AsRef<str> + Clone + fmt::Debug + PartialEq + Eq + PartialOrd + Ord + private::Sealed
{
    /// Extracts the underlying [`str`] slice
    fn as_str(&self) -> &'a str;

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
    /// * `Utf8UnixComponent::RootDir` - `is_root() == true`
    /// * `Utf8UnixComponent::ParentDir` - `is_root() == false`
    /// * `Utf8UnixComponent::CurDir` - `is_root() == false`
    /// * `Utf8UnixComponent::Normal("here.txt")` - `is_root() == false`
    fn is_root(&self) -> bool;

    /// Returns true if this component represents a normal part of the path
    ///
    /// # Examples
    ///
    /// `/my/../path/./here.txt` has the components on Unix of
    ///
    /// * `Utf8UnixComponent::RootDir` - `is_normal() == false`
    /// * `Utf8UnixComponent::ParentDir` - `is_normal() == false`
    /// * `Utf8UnixComponent::CurDir` - `is_normal() == false`
    /// * `Utf8UnixComponent::Normal("here.txt")` - `is_normal() == true`
    fn is_normal(&self) -> bool;

    /// Returns true if this component represents a relative representation of a parent directory
    ///
    /// # Examples
    ///
    /// `/my/../path/./here.txt` has the components on Unix of
    ///
    /// * `Utf8UnixComponent::RootDir` - `is_parent() == false`
    /// * `Utf8UnixComponent::ParentDir` - `is_parent() == true`
    /// * `Utf8UnixComponent::CurDir` - `is_parent() == false`
    /// * `Utf8UnixComponent::Normal("here.txt")` - `is_parent() == false`
    fn is_parent(&self) -> bool;

    /// Returns true if this component represents a relative representation of the current
    /// directory
    ///
    /// # Examples
    ///
    /// `/my/../path/./here.txt` has the components on Unix of
    ///
    /// * `Utf8UnixComponent::RootDir` - `is_current() == false`
    /// * `Utf8UnixComponent::ParentDir` - `is_current() == false`
    /// * `Utf8UnixComponent::CurDir` - `is_current() == true`
    /// * `Utf8UnixComponent::Normal("here.txt")` - `is_current() == false`
    fn is_current(&self) -> bool;

    /// Returns true if this component is valid. A component can only be invalid if it represents a
    /// normal component with characters that are disallowed by the encoding.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Utf8Component, Utf8UnixComponent, Utf8WindowsComponent};
    ///
    /// assert!(Utf8UnixComponent::RootDir.is_valid());
    /// assert!(Utf8UnixComponent::ParentDir.is_valid());
    /// assert!(Utf8UnixComponent::CurDir.is_valid());
    /// assert!(Utf8UnixComponent::Normal("abc").is_valid());
    /// assert!(!Utf8UnixComponent::Normal("\0").is_valid());
    ///
    /// assert!(Utf8WindowsComponent::RootDir.is_valid());
    /// assert!(Utf8WindowsComponent::ParentDir.is_valid());
    /// assert!(Utf8WindowsComponent::CurDir.is_valid());
    /// assert!(Utf8WindowsComponent::Normal("abc").is_valid());
    /// assert!(!Utf8WindowsComponent::Normal("|").is_valid());
    /// ```
    fn is_valid(&self) -> bool;

    /// Returns size of component in bytes
    fn len(&self) -> usize;

    /// Returns true if component represents an empty str
    fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Returns a root [`Utf8Component`].
    fn root() -> Self;

    /// Returns a parent directory [`Utf8Component`].
    fn parent() -> Self;

    /// Returns a current directory [`Utf8Component`].
    fn current() -> Self;
}
