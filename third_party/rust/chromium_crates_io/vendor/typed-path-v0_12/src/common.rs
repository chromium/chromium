mod errors;
#[macro_use]
mod non_utf8;
mod utf8;

/// Interface to try to perform a cheap reference-to-reference conversion.
pub trait TryAsRef<T: ?Sized> {
    fn try_as_ref(&self) -> Option<&T>;
}

pub use errors::*;
pub use non_utf8::*;
pub use utf8::*;
