use core::fmt;

use crate::no_std_compat::*;
use crate::{Encoding, Path};

/// Helper struct for safely printing paths with [`format!`] and `{}`.
///
/// A [`Path`] might contain non-Unicode data. This `struct` implements the
/// [`Display`] trait in a way that mitigates that. It is created by the
/// [`display`](Path::display) method on [`Path`]. This may perform lossy
/// conversion, depending on the platform. If you would like an implementation
/// which escapes the path please use [`Debug`] instead.
///
/// # Examples
///
/// ```
/// use typed_path::{Path, UnixEncoding};
///
/// // NOTE: A path cannot be created on its own without a defined encoding
/// let path = Path::<UnixEncoding>::new("/tmp/foo.rs");
///
/// println!("{}", path.display());
/// ```
///
/// [`Display`]: fmt::Display
/// [`format!`]: std::format
pub struct Display<'a, T>
where
    T: Encoding,
{
    pub(crate) path: &'a Path<T>,
}

impl<T> fmt::Debug for Display<'_, T>
where
    T: Encoding,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Debug::fmt(&self.path, f)
    }
}

impl<T> fmt::Display for Display<'_, T>
where
    T: Encoding,
{
    /// Performs lossy conversion to UTF-8 str
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", String::from_utf8_lossy(&self.path.inner))
    }
}
