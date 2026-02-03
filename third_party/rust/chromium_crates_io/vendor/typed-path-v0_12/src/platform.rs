pub use self::non_utf8::*;
pub use self::utf8::*;

mod non_utf8 {
    use core::any::TypeId;
    use core::fmt;
    use core::hash::Hasher;

    use crate::common::{CheckedPathError, Encoding, Path, PathBuf};
    use crate::native::NativeEncoding;
    use crate::no_std_compat::*;
    use crate::private;

    /// [`Path`] that has the platform's encoding during compilation.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::PlatformPath;
    ///
    /// // You can create the path like normal, but it is a distinct encoding from Unix/Windows
    /// let path = PlatformPath::new("some/path");
    ///
    /// // The path will still behave like normal and even report its underlying encoding
    /// assert_eq!(path.has_unix_encoding(), cfg!(unix));
    /// assert_eq!(path.has_windows_encoding(), cfg!(windows));
    ///
    /// // It can still be converted into specific platform paths
    /// let unix_path = path.with_unix_encoding();
    /// let win_path = path.with_windows_encoding();
    /// ```
    pub type PlatformPath = Path<PlatformEncoding>;

    /// [`PathBuf`] that has the platform's encoding during compilation.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::PlatformPathBuf;
    ///
    /// // You can create the pathbuf like normal, but it is a distinct encoding from Unix/Windows
    /// let path = PlatformPathBuf::from("some/path");
    ///
    /// // The path will still behave like normal and even report its underlying encoding
    /// assert_eq!(path.has_unix_encoding(), cfg!(unix));
    /// assert_eq!(path.has_windows_encoding(), cfg!(windows));
    ///
    /// // It can still be converted into specific platform paths
    /// let unix_path = path.with_unix_encoding();
    /// let win_path = path.with_windows_encoding();
    /// ```
    pub type PlatformPathBuf = PathBuf<PlatformEncoding>;

    /// Represents an abstraction of [`Encoding`] that represents the current platform encoding.
    ///
    /// This differs from [`NativeEncoding`] in that it is its own struct instead of a type alias
    /// to the platform-specific encoding, and can therefore be used to enforce more strict
    /// compile-time checks of encodings without needing to leverage conditional configs.
    ///
    /// # Examples
    ///
    /// ```
    /// use core::any::TypeId;
    /// use typed_path::{PlatformEncoding, UnixEncoding, WindowsEncoding};
    ///
    /// // The platform encoding is considered a distinct type from Unix/Windows encodings.
    /// assert_ne!(TypeId::of::<PlatformEncoding>(), TypeId::of::<UnixEncoding>());
    /// assert_ne!(TypeId::of::<PlatformEncoding>(), TypeId::of::<WindowsEncoding>());
    /// ```
    #[derive(Copy, Clone)]
    pub struct PlatformEncoding;

    impl private::Sealed for PlatformEncoding {}

    impl Encoding for PlatformEncoding {
        type Components<'a> = <NativeEncoding as Encoding>::Components<'a>;

        fn label() -> &'static str {
            NativeEncoding::label()
        }

        fn components(path: &[u8]) -> Self::Components<'_> {
            <NativeEncoding as Encoding>::components(path)
        }

        fn hash<H: Hasher>(path: &[u8], h: &mut H) {
            <NativeEncoding as Encoding>::hash(path, h)
        }

        fn push(current_path: &mut Vec<u8>, path: &[u8]) {
            <NativeEncoding as Encoding>::push(current_path, path);
        }

        fn push_checked(current_path: &mut Vec<u8>, path: &[u8]) -> Result<(), CheckedPathError> {
            <NativeEncoding as Encoding>::push_checked(current_path, path)
        }
    }

    impl fmt::Debug for PlatformEncoding {
        fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
            f.debug_struct("PlatformEncoding").finish()
        }
    }

    impl fmt::Display for PlatformEncoding {
        fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
            write!(f, "PlatformEncoding")
        }
    }

    impl<T> Path<T>
    where
        T: Encoding,
    {
        /// Returns true if the encoding is the platform abstraction ([`PlatformEncoding`]),
        /// otherwise returns false.
        ///
        /// # Examples
        ///
        /// ```
        /// use typed_path::{PlatformPath, UnixPath, WindowsPath};
        ///
        /// assert!(PlatformPath::new("/some/path").has_platform_encoding());
        /// assert!(!UnixPath::new("/some/path").has_platform_encoding());
        /// assert!(!WindowsPath::new("/some/path").has_platform_encoding());
        /// ```
        pub fn has_platform_encoding(&self) -> bool
        where
            T: 'static,
        {
            TypeId::of::<T>() == TypeId::of::<PlatformEncoding>()
        }

        /// Creates an owned [`PathBuf`] like `self` but using [`PlatformEncoding`].
        ///
        /// See [`Path::with_encoding`] for more information.
        pub fn with_platform_encoding(&self) -> PathBuf<PlatformEncoding> {
            self.with_encoding()
        }

        /// Creates an owned [`PathBuf`] like `self` but using [`PlatformEncoding`], ensuring it is
        /// a valid platform path.
        ///
        /// See [`Path::with_encoding_checked`] for more information.
        pub fn with_platform_encoding_checked(
            &self,
        ) -> Result<PathBuf<PlatformEncoding>, CheckedPathError> {
            self.with_encoding_checked()
        }
    }
}

mod utf8 {
    use core::any::TypeId;
    use core::fmt;
    use core::hash::Hasher;
    #[cfg(feature = "std")]
    use std::path::{Path as StdPath, PathBuf as StdPathBuf};

    use crate::common::{CheckedPathError, Utf8Encoding, Utf8Path, Utf8PathBuf};
    use crate::native::Utf8NativeEncoding;
    use crate::no_std_compat::*;
    use crate::private;

    /// [`Utf8Path`] that has the platform's encoding during compilation.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8PlatformPath;
    ///
    /// // You can create the path like normal, but it is a distinct encoding from Unix/Windows
    /// let path = Utf8PlatformPath::new("some/path");
    ///
    /// // The path will still behave like normal and even report its underlying encoding
    /// assert_eq!(path.has_unix_encoding(), cfg!(unix));
    /// assert_eq!(path.has_windows_encoding(), cfg!(windows));
    ///
    /// // It can still be converted into specific platform paths
    /// let unix_path = path.with_unix_encoding();
    /// let win_path = path.with_windows_encoding();
    /// ```
    pub type Utf8PlatformPath = Utf8Path<Utf8PlatformEncoding>;

    /// [`Utf8PathBuf`] that has the platform's encoding during compilation.
    ///
    /// # Examples
    ///
    /// ```
    /// use typed_path::Utf8PlatformPathBuf;
    ///
    /// // You can create the pathbuf like normal, but it is a distinct encoding from Unix/Windows
    /// let path = Utf8PlatformPathBuf::from("some/path");
    ///
    /// // The path will still behave like normal and even report its underlying encoding
    /// assert_eq!(path.has_unix_encoding(), cfg!(unix));
    /// assert_eq!(path.has_windows_encoding(), cfg!(windows));
    ///
    /// // It can still be converted into specific platform paths
    /// let unix_path = path.with_unix_encoding();
    /// let win_path = path.with_windows_encoding();
    /// ```
    pub type Utf8PlatformPathBuf = Utf8PathBuf<Utf8PlatformEncoding>;

    /// Represents an abstraction of [`Utf8Encoding`] that represents the current platform
    /// encoding.
    ///
    /// This differs from [`Utf8NativeEncoding`] in that it is its own struct instead of a type
    /// alias to the platform-specific encoding, and can therefore be used to enforce more strict
    /// compile-time checks of encodings without needing to leverage conditional configs.
    ///
    /// # Examples
    ///
    /// ```
    /// use core::any::TypeId;
    /// use typed_path::{Utf8PlatformEncoding, Utf8UnixEncoding, Utf8WindowsEncoding};
    ///
    /// // The UTF8 platform encoding is considered a distinct type from UTF8 Unix/Windows encodings.
    /// assert_ne!(TypeId::of::<Utf8PlatformEncoding>(), TypeId::of::<Utf8UnixEncoding>());
    /// assert_ne!(TypeId::of::<Utf8PlatformEncoding>(), TypeId::of::<Utf8WindowsEncoding>());
    /// ```
    #[derive(Copy, Clone)]
    pub struct Utf8PlatformEncoding;

    impl private::Sealed for Utf8PlatformEncoding {}

    impl Utf8Encoding for Utf8PlatformEncoding {
        type Components<'a> = <Utf8NativeEncoding as Utf8Encoding>::Components<'a>;

        fn label() -> &'static str {
            Utf8NativeEncoding::label()
        }

        fn components(path: &str) -> Self::Components<'_> {
            <Utf8NativeEncoding as Utf8Encoding>::components(path)
        }

        fn hash<H: Hasher>(path: &str, h: &mut H) {
            <Utf8NativeEncoding as Utf8Encoding>::hash(path, h)
        }

        fn push(current_path: &mut String, path: &str) {
            <Utf8NativeEncoding as Utf8Encoding>::push(current_path, path);
        }

        fn push_checked(current_path: &mut String, path: &str) -> Result<(), CheckedPathError> {
            <Utf8NativeEncoding as Utf8Encoding>::push_checked(current_path, path)
        }
    }

    impl fmt::Debug for Utf8PlatformEncoding {
        fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
            f.debug_struct("Utf8PlatformEncoding").finish()
        }
    }

    impl fmt::Display for Utf8PlatformEncoding {
        fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
            write!(f, "Utf8PlatformEncoding")
        }
    }

    impl<T> Utf8Path<T>
    where
        T: Utf8Encoding,
    {
        /// Returns true if the encoding is the platform abstraction ([`Utf8PlatformEncoding`]),
        /// otherwise returns false.
        ///
        /// # Examples
        ///
        /// ```
        /// use typed_path::{Utf8PlatformPath, Utf8UnixPath, Utf8WindowsPath};
        ///
        /// assert!(Utf8PlatformPath::new("/some/path").has_platform_encoding());
        /// assert!(!Utf8UnixPath::new("/some/path").has_platform_encoding());
        /// assert!(!Utf8WindowsPath::new("/some/path").has_platform_encoding());
        /// ```
        pub fn has_platform_encoding(&self) -> bool
        where
            T: 'static,
        {
            TypeId::of::<T>() == TypeId::of::<Utf8PlatformEncoding>()
        }

        /// Creates an owned [`Utf8PathBuf`] like `self` but using [`Utf8PlatformEncoding`].
        ///
        /// See [`Utf8Path::with_encoding`] for more information.
        pub fn with_platform_encoding(&self) -> Utf8PathBuf<Utf8PlatformEncoding> {
            self.with_encoding()
        }

        /// Creates an owned [`Utf8PathBuf`] like `self` but using [`Utf8PlatformEncoding`],
        /// ensuring it is a valid platform path.
        ///
        /// See [`Utf8Path::with_encoding_checked`] for more information.
        pub fn with_platform_encoding_checked(
            &self,
        ) -> Result<Utf8PathBuf<Utf8PlatformEncoding>, CheckedPathError> {
            self.with_encoding_checked()
        }
    }

    #[cfg(all(feature = "std", not(target_family = "wasm")))]
    impl AsRef<StdPath> for Utf8PlatformPath {
        /// Converts a platform utf8 path (based on compilation family) into [`std::path::Path`].
        ///
        /// ```
        /// use typed_path::Utf8PlatformPath;
        /// use std::path::Path;
        ///
        /// let platform_path = Utf8PlatformPath::new("some_file.txt");
        /// let std_path: &Path = platform_path.as_ref();
        ///
        /// assert_eq!(std_path, Path::new("some_file.txt"));
        /// ```
        fn as_ref(&self) -> &StdPath {
            StdPath::new(self.as_str())
        }
    }

    #[cfg(all(feature = "std", not(target_family = "wasm")))]
    impl AsRef<StdPath> for Utf8PlatformPathBuf {
        /// Converts a platform utf8 pathbuf (based on compilation family) into [`std::path::Path`].
        ///
        /// ```
        /// use typed_path::Utf8PlatformPathBuf;
        /// use std::path::Path;
        ///
        /// let platform_path_buf = Utf8PlatformPathBuf::from("some_file.txt");
        /// let std_path: &Path = platform_path_buf.as_ref();
        ///
        /// assert_eq!(std_path, Path::new("some_file.txt"));
        /// ```
        fn as_ref(&self) -> &StdPath {
            StdPath::new(self.as_str())
        }
    }

    #[cfg(all(feature = "std", not(target_family = "wasm")))]
    impl From<Utf8PlatformPathBuf> for StdPathBuf {
        /// Converts a platform utf8 pathbuf (based on compilation family) into [`std::path::PathBuf`].
        ///
        /// ```
        /// use typed_path::Utf8PlatformPathBuf;
        /// use std::path::PathBuf;
        ///
        /// let platform_path_buf = Utf8PlatformPathBuf::from("some_file.txt");
        /// let std_path_buf = PathBuf::from(platform_path_buf);
        ///
        /// assert_eq!(std_path_buf, PathBuf::from("some_file.txt"));
        /// ```
        fn from(utf8_platform_path_buf: Utf8PlatformPathBuf) -> StdPathBuf {
            StdPathBuf::from(utf8_platform_path_buf.into_string())
        }
    }
}
