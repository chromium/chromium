mod components;
mod iter;
mod path;
mod pathbuf;

use core::hash::Hasher;

pub use components::*;
pub use iter::*;
pub use path::*;
pub use pathbuf::*;

use crate::common::errors::CheckedPathError;
use crate::no_std_compat::*;
use crate::private;

/// Interface to provide meaning to a byte slice such that paths can be derived
pub trait Utf8Encoding: private::Sealed {
    /// Represents the type of component that will be derived by this encoding
    type Components<'a>: Utf8Components<'a>;

    /// Static label representing encoding type
    fn label() -> &'static str;

    /// Produces an iterator of [`Utf8Component`]s over the given the byte slice (`path`)
    fn components(path: &str) -> Self::Components<'_>;

    /// Hashes a utf8 str (`path`)
    fn hash<H: Hasher>(path: &str, h: &mut H);

    /// Pushes a utf8 str (`path`) onto the an existing path (`current_path`)
    fn push(current_path: &mut String, path: &str);

    /// Like [`Utf8Encoding::push`], but enforces several new rules:
    ///
    /// 1. `path` cannot contain a prefix component.
    /// 2. `path` cannot contain a root component.
    /// 3. `path` cannot contain invalid filename characters.
    /// 4. `path` cannot contain parent components such that the current path would be escaped.
    fn push_checked(current_path: &mut String, path: &str) -> Result<(), CheckedPathError>;
}
