//! Thread-safe lazy cells for cached font data.

#[cfg(feature = "std")]
pub(crate) type Once<T> = std::sync::OnceLock<T>;

#[cfg(not(feature = "std"))]
pub(crate) struct Once<T>(spin::Once<T>);

#[cfg(not(feature = "std"))]
impl<T> Once<T> {
    pub(crate) const fn new() -> Self {
        Self(spin::Once::new())
    }

    pub(crate) fn get_or_init(&self, f: impl FnOnce() -> T) -> &T {
        self.0.call_once(f)
    }
}

#[cfg(not(feature = "std"))]
impl<T: Clone> Clone for Once<T> {
    fn clone(&self) -> Self {
        let copy = Self::new();
        if let Some(value) = self.0.get() {
            copy.0.call_once(|| value.clone());
        }
        copy
    }
}
