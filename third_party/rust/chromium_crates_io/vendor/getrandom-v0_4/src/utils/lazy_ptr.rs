use core::{
    convert::Infallible,
    ptr::{self, NonNull},
    sync::atomic::{AtomicPtr, Ordering::Relaxed},
};

/// Lazily caches a non-null pointer in an `AtomicPtr`.
///
/// Initialization is intentionally unsynchronized: concurrent callers may race
/// and run `init` more than once. Once a value is produced, it is cached and
/// reused by subsequent calls.
///
/// For fallible initialization (`try_unsync_init`), only successful values are
/// cached; errors are returned to the caller and are not cached.
///
/// Uses `Ordering::Relaxed` because this helper only publishes the cached
/// pointer value. Callers must not rely on this mechanism to synchronize
/// unrelated memory side effects performed by `init`.
pub(crate) struct LazyPtr<T>(AtomicPtr<T>);

impl<T> LazyPtr<T> {
    /// Create new `LazyPtr`.
    pub const fn new() -> Self {
        Self(AtomicPtr::new(ptr::null_mut()))
    }

    /// Call the `init` closure and return the result after caching it in the case of success.
    #[cold]
    fn cold_init<E>(&self, init: impl FnOnce() -> Result<NonNull<T>, E>) -> Result<NonNull<T>, E> {
        let val = init()?;
        self.0.store(val.as_ptr(), Relaxed);
        Ok(val)
    }

    /// Retrieve the cached value if it was already initialized or call the potentially fallible
    /// `init` closure and return the result after caching it in the case of success.
    #[inline]
    pub fn try_unsync_init<E>(
        &self,
        init: impl FnOnce() -> Result<NonNull<T>, E>,
    ) -> Result<NonNull<T>, E> {
        let p = self.0.load(Relaxed);
        match NonNull::new(p) {
            Some(val) => Ok(val),
            None => self.cold_init(init),
        }
    }

    /// Retrieve the cached value if it was already initialized or call the `init` closure
    /// and return the result after caching it.
    #[inline]
    #[allow(dead_code, reason = "Some modules use only `try_unsync_init`")]
    pub fn unsync_init(&self, init: impl FnOnce() -> NonNull<T>) -> NonNull<T> {
        let Ok(p): Result<_, Infallible> = self.try_unsync_init(|| Ok(init()));
        p
    }
}
