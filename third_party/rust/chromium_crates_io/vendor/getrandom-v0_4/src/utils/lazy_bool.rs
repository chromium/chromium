use core::sync::atomic::{AtomicU8, Ordering::Relaxed};

/// Lazily caches a `bool` in an `AtomicU8`.
///
/// Initialization is intentionally unsynchronized: concurrent callers may race
/// and run `init` more than once. Once a value is produced, it is cached and
/// reused by subsequent calls.
///
/// Uses `Relaxed` ordering because this helper only publishes the cached
/// value itself.
pub(crate) struct LazyBool(AtomicU8);

impl LazyBool {
    const UNINIT: u8 = u8::MAX;

    /// Create new `LazyBool`.
    pub const fn new() -> Self {
        Self(AtomicU8::new(Self::UNINIT))
    }

    /// Call the `init` closure and return the result after caching it.
    #[cold]
    fn cold_init(&self, init: impl FnOnce() -> bool) -> bool {
        let val = u8::from(init());
        self.0.store(val, Relaxed);
        val != 0
    }

    /// Retrieve the cached value if it was already initialized or call the `init` closure
    /// and return the result after caching it.
    #[inline]
    pub fn unsync_init(&self, init: impl FnOnce() -> bool) -> bool {
        let val = self.0.load(Relaxed);
        if val == Self::UNINIT {
            return self.cold_init(init);
        }
        val != 0
    }
}
