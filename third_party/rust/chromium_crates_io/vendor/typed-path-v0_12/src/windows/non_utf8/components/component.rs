mod prefix;
use core::convert::TryFrom;

pub use prefix::{WindowsPrefix, WindowsPrefixComponent};

use crate::windows::constants::{
    CURRENT_DIR, DISALLOWED_FILENAME_BYTES, PARENT_DIR, SEPARATOR_STR,
};
use crate::windows::WindowsComponents;
use crate::{private, Component, Encoding, ParseError, Path};

/// Byte slice version of [`std::path::Component`] that represents a Windows-specific component
#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash, Debug)]
pub enum WindowsComponent<'a> {
    Prefix(WindowsPrefixComponent<'a>),
    RootDir,
    CurDir,
    ParentDir,
    Normal(&'a [u8]),
}

impl private::Sealed for WindowsComponent<'_> {}

impl<'a> WindowsComponent<'a> {
    /// Returns path representing this specific component
    pub fn as_path<T>(&self) -> &Path<T>
    where
        T: Encoding,
    {
        Path::new(self.as_bytes())
    }

    /// Returns true if represents a prefix
    pub fn is_prefix(&self) -> bool {
        matches!(self, Self::Prefix(_))
    }

    /// Converts from `WindowsComponent` to [`Option<WindowsPrefixComponent>`]
    ///
    /// Converts `self` into an [`Option<WindowsPrefixComponent>`], consuming `self`, and
    /// discarding if not a [`WindowsPrefixComponent`]
    pub fn prefix(self) -> Option<WindowsPrefixComponent<'a>> {
        match self {
            Self::Prefix(p) => Some(p),
            _ => None,
        }
    }

    /// Converts from `WindowsComponent` to [`Option<WindowsPrefix>`]
    ///
    /// Converts `self` into an [`Option<WindowsPrefix>`], consuming `self`, and
    /// discarding if not a [`WindowsPrefixComponent`] whose [`kind`] method we invoke
    ///
    /// [`kind`]: WindowsPrefixComponent::kind
    pub fn prefix_kind(self) -> Option<WindowsPrefix<'a>> {
        self.prefix().map(|p| p.kind())
    }
}

impl<'a> Component<'a> for WindowsComponent<'a> {
    /// Extracts the underlying [`[u8]`] slice
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Component, WindowsPath};
    ///
    /// let path = WindowsPath::new(br"C:\tmp\foo\..\bar.txt");
    /// let components: Vec<_> = path.components().map(|comp| comp.as_bytes()).collect();
    /// assert_eq!(&components, &[
    ///     b"C:".as_slice(),
    ///     br"\".as_slice(),
    ///     b"tmp".as_slice(),
    ///     b"foo".as_slice(),
    ///     b"..".as_slice(),
    ///     b"bar.txt".as_slice(),
    /// ]);
    /// ```
    fn as_bytes(&self) -> &'a [u8] {
        match self {
            Self::Prefix(p) => p.as_bytes(),
            Self::RootDir => SEPARATOR_STR.as_bytes(),
            Self::CurDir => CURRENT_DIR,
            Self::ParentDir => PARENT_DIR,
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
    /// use typed_path::{Component, WindowsComponent};
    /// use std::convert::TryFrom;
    ///
    /// let root_dir = WindowsComponent::try_from(br"\").unwrap();
    /// assert!(root_dir.is_root());
    ///
    /// let non_disk_prefix = WindowsComponent::try_from(br"\\?\pictures").unwrap();
    /// assert!(non_disk_prefix.is_root());
    ///
    /// let disk_prefix = WindowsComponent::try_from(b"C:").unwrap();
    /// assert!(!disk_prefix.is_root());
    ///
    /// let normal = WindowsComponent::try_from(b"file.txt").unwrap();
    /// assert!(!normal.is_root());
    /// ```
    fn is_root(&self) -> bool {
        match self {
            Self::RootDir => true,
            Self::Prefix(prefix) => !matches!(prefix.kind(), WindowsPrefix::Disk(_)),
            _ => false,
        }
    }

    /// Returns true if component is normal
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Component, WindowsComponent};
    /// use std::convert::TryFrom;
    ///
    /// let normal = WindowsComponent::try_from(b"file.txt").unwrap();
    /// assert!(normal.is_normal());
    ///
    /// let root_dir = WindowsComponent::try_from(br"\").unwrap();
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
    /// use typed_path::{Component, WindowsComponent};
    /// use std::convert::TryFrom;
    ///
    /// let parent = WindowsComponent::try_from("..").unwrap();
    /// assert!(parent.is_parent());
    ///
    /// let root_dir = WindowsComponent::try_from(r"\").unwrap();
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
    /// use typed_path::{Component, WindowsComponent};
    /// use std::convert::TryFrom;
    ///
    /// let current = WindowsComponent::try_from(".").unwrap();
    /// assert!(current.is_current());
    ///
    /// let root_dir = WindowsComponent::try_from(r"\").unwrap();
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
    /// use typed_path::{Component, WindowsComponent};
    /// use std::convert::TryFrom;
    ///
    /// assert!(WindowsComponent::try_from("c:").unwrap().is_valid());
    /// assert!(WindowsComponent::RootDir.is_valid());
    /// assert!(WindowsComponent::ParentDir.is_valid());
    /// assert!(WindowsComponent::CurDir.is_valid());
    /// assert!(WindowsComponent::Normal(b"abc").is_valid());
    /// assert!(!WindowsComponent::Normal(b"|").is_valid());
    /// ```
    fn is_valid(&self) -> bool {
        match self {
            Self::Prefix(_) | Self::RootDir | Self::ParentDir | Self::CurDir => true,
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
    /// use typed_path::{Component, WindowsComponent};
    ///
    /// assert_eq!(WindowsComponent::root(), WindowsComponent::RootDir);
    /// ```
    fn root() -> Self {
        Self::RootDir
    }

    /// Returns the parent directory component.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Component, WindowsComponent};
    ///
    /// assert_eq!(WindowsComponent::parent(), WindowsComponent::ParentDir);
    /// ```
    fn parent() -> Self {
        Self::ParentDir
    }

    /// Returns the current directory component.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{Component, WindowsComponent};
    ///
    /// assert_eq!(WindowsComponent::current(), WindowsComponent::CurDir);
    /// ```
    fn current() -> Self {
        Self::CurDir
    }
}

impl AsRef<[u8]> for WindowsComponent<'_> {
    #[inline]
    fn as_ref(&self) -> &[u8] {
        self.as_bytes()
    }
}

impl<T> AsRef<Path<T>> for WindowsComponent<'_>
where
    T: Encoding,
{
    #[inline]
    fn as_ref(&self) -> &Path<T> {
        Path::new(self.as_bytes())
    }
}

impl<'a> TryFrom<&'a [u8]> for WindowsComponent<'a> {
    type Error = ParseError;

    /// Parses the byte slice into a [`WindowsComponent`]
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::{WindowsComponent, WindowsPrefix};
    /// use std::convert::TryFrom;
    ///
    /// // Supports parsing Windows prefixes
    /// let component = WindowsComponent::try_from(b"c:").unwrap();
    /// assert_eq!(component.prefix_kind(), Some(WindowsPrefix::Disk(b'C')));
    ///
    /// // Supports parsing standard windows path components
    /// assert_eq!(WindowsComponent::try_from(br"\"), Ok(WindowsComponent::RootDir));
    /// assert_eq!(WindowsComponent::try_from(b"."), Ok(WindowsComponent::CurDir));
    /// assert_eq!(WindowsComponent::try_from(b".."), Ok(WindowsComponent::ParentDir));
    /// assert_eq!(WindowsComponent::try_from(br"file.txt"), Ok(WindowsComponent::Normal(b"file.txt")));
    /// assert_eq!(WindowsComponent::try_from(br"dir\"), Ok(WindowsComponent::Normal(b"dir")));
    ///
    /// // Parsing more than one component will fail
    /// assert!(WindowsComponent::try_from(br"\file").is_err());
    /// ```
    fn try_from(path: &'a [u8]) -> Result<Self, Self::Error> {
        let mut components = WindowsComponents::new(path);

        let component = components.next().ok_or("no component found")?;
        if components.next().is_some() {
            return Err("found more than one component");
        }

        Ok(component)
    }
}

impl<'a, const N: usize> TryFrom<&'a [u8; N]> for WindowsComponent<'a> {
    type Error = ParseError;

    fn try_from(path: &'a [u8; N]) -> Result<Self, Self::Error> {
        Self::try_from(path.as_slice())
    }
}

impl<'a> TryFrom<&'a str> for WindowsComponent<'a> {
    type Error = ParseError;

    fn try_from(path: &'a str) -> Result<Self, Self::Error> {
        Self::try_from(path.as_bytes())
    }
}

#[cfg(feature = "std")]
impl<'a> TryFrom<WindowsComponent<'a>> for std::path::Component<'a> {
    type Error = WindowsComponent<'a>;

    /// Attempts to convert a [`WindowsComponent`] into a [`std::path::Component`], returning a
    /// result containing the new path when successful or the original path when failed
    ///
    /// # Examples
    ///
    /// ```
    /// use std::convert::TryFrom;
    /// use std::ffi::OsStr;
    /// use std::path::Component;
    /// use typed_path::WindowsComponent;
    ///
    /// let component = Component::try_from(WindowsComponent::RootDir).unwrap();
    /// assert_eq!(component, Component::RootDir);
    ///
    /// let component = Component::try_from(WindowsComponent::CurDir).unwrap();
    /// assert_eq!(component, Component::CurDir);
    ///
    /// let component = Component::try_from(WindowsComponent::ParentDir).unwrap();
    /// assert_eq!(component, Component::ParentDir);
    ///
    /// let component = Component::try_from(WindowsComponent::Normal(b"file.txt")).unwrap();
    /// assert_eq!(component, Component::Normal(OsStr::new("file.txt")));
    /// ```
    ///
    /// Alongside the traditional path components, the [`Component::Prefix`] variant is also
    /// supported, but only when compiling on Windows. When on a non-Windows platform, the
    /// conversion will always fail.
    ///
    /// [`Component::Prefix`]: std::path::Component::Prefix
    ///
    fn try_from(component: WindowsComponent<'a>) -> Result<Self, Self::Error> {
        match &component {
            // NOTE: Standard library provides no way to construct a PrefixComponent, so, we have
            //       to build a new path with just the prefix and then get the component
            //
            //       Because the prefix is not empty when being supplied to the path, we should get
            //       back at least one component and can therefore return the unwrapped result
            WindowsComponent::Prefix(x) => {
                if cfg!(windows) {
                    Ok(std::path::Path::new(
                        std::str::from_utf8(x.as_bytes()).map_err(|_| component)?,
                    )
                    .components()
                    .next()
                    .expect("Impossible: non-empty std path had no components"))
                } else {
                    Err(component)
                }
            }
            WindowsComponent::RootDir => Ok(Self::RootDir),
            WindowsComponent::CurDir => Ok(Self::CurDir),
            WindowsComponent::ParentDir => Ok(Self::ParentDir),
            WindowsComponent::Normal(x) => Ok(Self::Normal(std::ffi::OsStr::new(
                std::str::from_utf8(x).map_err(|_| component)?,
            ))),
        }
    }
}

#[cfg(feature = "std")]
impl<'a> TryFrom<std::path::Component<'a>> for WindowsComponent<'a> {
    type Error = std::path::Component<'a>;

    /// Attempts to convert a [`std::path::Component`] into a [`WindowsComponent`], returning a
    /// result containing the new component when successful or the original component when failed
    ///
    /// # Examples
    ///
    /// ```
    /// use std::convert::TryFrom;
    /// use std::ffi::OsStr;
    /// use std::path::Component;
    /// use typed_path::WindowsComponent;
    ///
    /// let component = WindowsComponent::try_from(Component::RootDir).unwrap();
    /// assert_eq!(component, WindowsComponent::RootDir);
    ///
    /// let component = WindowsComponent::try_from(Component::CurDir).unwrap();
    /// assert_eq!(component, WindowsComponent::CurDir);
    ///
    /// let component = WindowsComponent::try_from(Component::ParentDir).unwrap();
    /// assert_eq!(component, WindowsComponent::ParentDir);
    ///
    /// let component = WindowsComponent::try_from(Component::Normal(OsStr::new("file.txt"))).unwrap();
    /// assert_eq!(component, WindowsComponent::Normal(b"file.txt"));
    /// ```
    ///
    /// Alongside the traditional path components, the [`Component::Prefix`] variant is also
    /// supported, but only when compiling on Windows. When on a non-Windows platform, the
    /// conversion will always fail.
    ///
    /// [`Component::Prefix`]: std::path::Component::Prefix
    ///
    fn try_from(component: std::path::Component<'a>) -> Result<Self, Self::Error> {
        match &component {
            std::path::Component::Prefix(x) => Ok(WindowsComponent::Prefix(
                WindowsPrefixComponent::try_from(x.as_os_str().to_str().ok_or(component)?)
                    .map_err(|_| component)?,
            )),
            std::path::Component::RootDir => Ok(Self::RootDir),
            std::path::Component::CurDir => Ok(Self::CurDir),
            std::path::Component::ParentDir => Ok(Self::ParentDir),
            std::path::Component::Normal(x) => {
                Ok(Self::Normal(x.to_str().ok_or(component)?.as_bytes()))
            }
        }
    }
}

#[cfg(test)]
#[cfg(feature = "std")]
mod tests {
    use std::convert::TryFrom;
    use std::path::Component;

    use super::*;

    fn make_windows_prefix_component(s: &str) -> WindowsComponent<'_> {
        let component = WindowsComponent::try_from(s).unwrap();
        assert!(component.is_prefix());
        component
    }

    #[test]
    #[cfg(windows)]
    fn try_from_windows_component_to_std_component_should_keep_prefix_on_windows() {
        use std::ffi::OsStr;
        use std::path::Prefix;
        fn get_prefix(component: Component) -> Prefix {
            match component {
                Component::Prefix(prefix) => prefix.kind(),
                x => panic!("Wrong component: {x:?}"),
            }
        }

        let component = Component::try_from(make_windows_prefix_component("C:")).unwrap();
        assert_eq!(get_prefix(component), Prefix::Disk(b'C'));

        let component =
            Component::try_from(make_windows_prefix_component(r"\\server\share")).unwrap();
        assert_eq!(
            get_prefix(component),
            Prefix::UNC(OsStr::new("server"), OsStr::new("share"))
        );

        let component = Component::try_from(make_windows_prefix_component(r"\\.\COM42")).unwrap();
        assert_eq!(get_prefix(component), Prefix::DeviceNS(OsStr::new("COM42")));

        let component = Component::try_from(make_windows_prefix_component(r"\\?\C:")).unwrap();
        assert_eq!(get_prefix(component), Prefix::VerbatimDisk(b'C'));

        let component =
            Component::try_from(make_windows_prefix_component(r"\\?\UNC\server\share")).unwrap();
        assert_eq!(
            get_prefix(component),
            Prefix::VerbatimUNC(OsStr::new("server"), OsStr::new("share"))
        );

        let component =
            Component::try_from(make_windows_prefix_component(r"\\?\pictures")).unwrap();
        assert_eq!(
            get_prefix(component),
            Prefix::Verbatim(OsStr::new("pictures"))
        );
    }

    #[test]
    #[cfg(not(windows))]
    fn try_from_windows_component_to_std_component_should_fail_for_prefix_on_non_windows() {
        Component::try_from(make_windows_prefix_component("C:")).unwrap_err();
        Component::try_from(make_windows_prefix_component(r"\\server\share")).unwrap_err();
        Component::try_from(make_windows_prefix_component(r"\\.\COM42")).unwrap_err();
        Component::try_from(make_windows_prefix_component(r"\\?\C:")).unwrap_err();
        Component::try_from(make_windows_prefix_component(r"\\?\UNC\server\share")).unwrap_err();
        Component::try_from(make_windows_prefix_component(r"\\?\pictures")).unwrap_err();
    }

    #[test]
    #[cfg(windows)]
    fn try_from_std_component_to_windows_component_should_keep_prefix_on_windows() {
        use std::path::Path;

        use crate::windows::WindowsPrefix;

        fn make_component(s: &str) -> Component<'_> {
            let component = Path::new(s).components().next();
            assert!(
                matches!(component, Some(Component::Prefix(_))),
                "std component not a prefix"
            );
            component.unwrap()
        }

        fn get_prefix(component: WindowsComponent) -> WindowsPrefix {
            match component {
                WindowsComponent::Prefix(prefix) => prefix.kind(),
                x => panic!("Wrong component: {x:?}"),
            }
        }

        let component = WindowsComponent::try_from(make_component("C:")).unwrap();
        assert_eq!(get_prefix(component), WindowsPrefix::Disk(b'C'));

        let component = WindowsComponent::try_from(make_component(r"\\server\share")).unwrap();
        assert_eq!(
            get_prefix(component),
            WindowsPrefix::UNC(b"server", b"share")
        );

        let component = WindowsComponent::try_from(make_component(r"\\.\COM42")).unwrap();
        assert_eq!(get_prefix(component), WindowsPrefix::DeviceNS(b"COM42"));

        let component = WindowsComponent::try_from(make_component(r"\\?\C:")).unwrap();
        assert_eq!(get_prefix(component), WindowsPrefix::VerbatimDisk(b'C'));

        let component =
            WindowsComponent::try_from(make_component(r"\\?\UNC\server\share")).unwrap();
        assert_eq!(
            get_prefix(component),
            WindowsPrefix::VerbatimUNC(b"server", b"share")
        );

        let component = WindowsComponent::try_from(make_component(r"\\?\pictures")).unwrap();
        assert_eq!(get_prefix(component), WindowsPrefix::Verbatim(b"pictures"));
    }
}
