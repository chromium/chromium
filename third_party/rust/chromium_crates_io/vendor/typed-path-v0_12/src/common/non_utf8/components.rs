mod component;

use core::{cmp, fmt, iter};

pub use component::*;

use crate::private;

/// Interface of an iterator over a collection of [`Component`]s
pub trait Components<'a>:
    AsRef<[u8]>
    + Clone
    + fmt::Debug
    + cmp::PartialEq
    + cmp::Eq
    + cmp::PartialOrd
    + cmp::Ord
    + iter::Iterator<Item = Self::Component>
    + iter::DoubleEndedIterator<Item = Self::Component>
    + iter::FusedIterator
    + Sized
    + private::Sealed
{
    /// Type of [`Component`] iterated over
    type Component: Component<'a>;

    /// Extracts a slice corresponding to the portion of the path remaining for iteration
    fn as_bytes(&self) -> &'a [u8];

    /// Reports back whether the iterator represents an absolute path
    ///
    /// The definition of an absolute path can vary:
    ///
    /// * On Unix, a path is absolute if it starts with the root, so `is_absolute` and [`has_root`]
    ///   are equivalent.
    ///
    /// * On Windows, a path is absolute if it has a prefix and starts with the root: `c:\windows`
    ///   is absolute, while `c:temp` and `\temp` are not.
    ///
    /// [`has_root`]: Components::has_root
    fn is_absolute(&self) -> bool;

    /// Returns `true` if the iterator represents a path that has a root.
    ///
    /// The definition of what it means for a path to have a root can vary:
    ///
    /// * On Unix, a path has a root if it begins with `/`.
    ///
    /// * On Windows, a path has a root if it:
    ///     * has no prefix and begins with a separator, e.g., `\windows`
    ///     * has a prefix followed by a separator, e.g., `c:\windows` but not `c:windows`
    ///     * has any non-disk prefix, e.g., `\\server\share`
    fn has_root(&self) -> bool;
}
