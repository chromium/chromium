#![deny(missing_docs, warnings, clippy::all, clippy::pedantic)]
#![doc = include_str!("../README.md")]

mod atomic_owned;
pub use atomic_owned::AtomicOwned;

mod atomic_shared;
pub use atomic_shared::AtomicShared;

mod guard;
pub use guard::Guard;

mod epoch;
pub use epoch::Epoch;

mod owned;
pub use owned::Owned;

mod ptr;
pub use ptr::Ptr;

mod shared;
pub use shared::Shared;

mod tag;
pub use tag::Tag;

mod collectible;
mod collector;
mod exit_guard;
mod ref_counted;

/// Suspends the garbage collector of the current thread.
///
/// If returns `false` if there is an active [`Guard`] in the thread. Otherwise, it passes all its
/// retired instances to a free flowing garbage container that can be cleaned up by other threads.
///
/// # Examples
///
/// ```
/// use sdd::{suspend, Guard};
///
/// assert!(suspend());
///
/// {
///     let guard = Guard::new();
///     assert!(!suspend());
/// }
///
/// assert!(suspend());
/// ```
#[inline]
#[must_use]
pub fn suspend() -> bool {
    collector::Collector::pass_garbage()
}

#[cfg(feature = "loom")]
mod maybe_std {
    pub(crate) use loom::sync::atomic::{fence, AtomicPtr};
}

#[cfg(not(feature = "loom"))]
mod maybe_std {
    pub(crate) use std::sync::atomic::{fence, AtomicPtr};
}

#[cfg(test)]
mod tests;
