//! This module implements a simplified, yet safe version of
//! [`scopeguard`](https://crates.io/crates/scopeguard).

use std::ops::{Deref, DerefMut};

/// [`ExitGuard`] captures the environment and invokes the defined closure at the end of the scope.
pub(crate) struct ExitGuard<T, F: FnOnce(T)> {
    drop_callback: Option<(T, F)>,
}

impl<T, F: FnOnce(T)> ExitGuard<T, F> {
    /// Creates a new [`ExitGuard`] with the specified variables captured.
    #[inline]
    pub(crate) fn new(captured: T, drop_callback: F) -> Self {
        Self {
            drop_callback: Some((captured, drop_callback)),
        }
    }
}

impl<T, F: FnOnce(T)> Drop for ExitGuard<T, F> {
    #[inline]
    fn drop(&mut self) {
        if let Some((c, f)) = self.drop_callback.take() {
            f(c);
        }
    }
}

impl<T, F: FnOnce(T)> Deref for ExitGuard<T, F> {
    type Target = T;

    #[inline]
    fn deref(&self) -> &Self::Target {
        unsafe { &self.drop_callback.as_ref().unwrap_unchecked().0 }
    }
}

impl<T, F: FnOnce(T)> DerefMut for ExitGuard<T, F> {
    #[inline]
    fn deref_mut(&mut self) -> &mut Self::Target {
        unsafe { &mut self.drop_callback.as_mut().unwrap_unchecked().0 }
    }
}
