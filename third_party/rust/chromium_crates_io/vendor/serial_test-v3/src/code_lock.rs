use crate::rwlock::{Locks, MutexGuardWrapper};
use once_cell::sync::OnceCell;
use std::{
    collections::HashMap,
    sync::{atomic::AtomicU32, Mutex},
};

pub(crate) struct ValueRef(UniqueReentrantMutex);

impl ValueRef {
    pub fn get(&self) -> &UniqueReentrantMutex {
        &self.0
    }
}

pub(crate) struct LockMap {
    inner: Mutex<HashMap<String, UniqueReentrantMutex>>,
}

impl LockMap {
    fn new() -> Self {
        LockMap {
            inner: Mutex::new(HashMap::new()),
        }
    }

    pub fn get(&self, key: &str) -> Option<ValueRef> {
        self.inner.lock().unwrap().get(key).cloned().map(ValueRef)
    }

    fn get_or_insert(
        &self,
        key: &str,
        f: impl FnOnce() -> UniqueReentrantMutex,
    ) -> UniqueReentrantMutex {
        let mut map = self.inner.lock().unwrap();
        map.entry(key.to_owned()).or_insert_with(f).clone()
    }
}

#[derive(Clone)]
pub(crate) struct UniqueReentrantMutex {
    locks: Locks,

    // Only actually used for tests
    #[allow(dead_code)]
    pub(crate) id: u32,
}

impl UniqueReentrantMutex {
    pub(crate) fn lock(&self) -> MutexGuardWrapper<'_> {
        self.locks.serial()
    }

    pub(crate) fn start_parallel(&self) {
        self.locks.start_parallel();
    }

    pub(crate) fn end_parallel(&self) {
        self.locks.end_parallel();
    }

    #[cfg(test)]
    pub fn parallel_count(&self) -> u32 {
        self.locks.parallel_count()
    }

    #[cfg(test)]
    pub fn is_locked(&self) -> bool {
        self.locks.is_locked()
    }

    pub fn is_locked_by_current_thread(&self) -> bool {
        self.locks.is_locked_by_current_thread()
    }
}

#[inline]
pub(crate) fn global_locks() -> &'static LockMap {
    #[cfg(feature = "test_logging")]
    let _ = env_logger::builder().try_init();
    static LOCKS: OnceCell<LockMap> = OnceCell::new();
    LOCKS.get_or_init(LockMap::new)
}

/// Check if the current thread is holding a serial lock
///
/// Can be used to assert that a piece of code can only be called
/// from a test marked `#[serial]`.
///
/// Example, with `#[serial]`:
///
/// ```no_run
/// use serial_test::{is_locked_serially, serial};
///
/// fn do_something_in_need_of_serialization() {
///     assert!(is_locked_serially(None));
///
///     // ...
/// }
///
/// #[test]
/// # fn unused() {}
/// #[serial]
/// fn main() {
///     do_something_in_need_of_serialization();
/// }
/// ```
///
/// Example, missing `#[serial]`:
///
/// ```should_panic,no_run
/// use serial_test::{is_locked_serially, serial};
///
/// #[test]
/// # fn unused() {}
/// // #[serial] // <-- missing
/// fn main() {
///     assert!(is_locked_serially(None));
/// }
/// ```
///
/// Example, `#[test(some_key)]`:
///
/// ```no_run
/// use serial_test::{is_locked_serially, serial};
///
/// #[test]
/// # fn unused() {}
/// #[serial(some_key)]
/// fn main() {
///     assert!(is_locked_serially(Some("some_key")));
///     assert!(!is_locked_serially(None));
/// }
/// ```
pub fn is_locked_serially(name: Option<&str>) -> bool {
    global_locks()
        .get(name.unwrap_or_default())
        .map(|lock| lock.get().is_locked_by_current_thread())
        .unwrap_or_default()
}

static MUTEX_ID: AtomicU32 = AtomicU32::new(1);

impl UniqueReentrantMutex {
    fn new_mutex(name: &str) -> Self {
        Self {
            locks: Locks::new(name),
            id: MUTEX_ID.fetch_add(1, std::sync::atomic::Ordering::SeqCst),
        }
    }
}

pub(crate) fn check_new_key(name: &str) {
    global_locks().get_or_insert(name, || UniqueReentrantMutex::new_mutex(name));
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{local_parallel_core, local_serial_core};

    #[test]
    fn assert_serially_locked_without_name() {
        local_serial_core(vec![""], None, || {
            assert!(is_locked_serially(None));
            assert!(!is_locked_serially(Some(
                "no_such_name_assert_serially_locked_without_name"
            )));
        });
    }

    #[test]
    fn assert_serially_locked_with_multiple_names() {
        const NAME1: &str = "assert_serially_locked_with_multiple_names-NAME1";
        const NAME2: &str = "assert_serially_locked_with_multiple_names-NAME2";
        local_serial_core(vec![NAME1, NAME2], None, || {
            assert!(is_locked_serially(Some(NAME1)));
            assert!(is_locked_serially(Some(NAME2)));
            assert!(!is_locked_serially(Some(
                "no_such_name_assert_serially_locked_with_multiple_names"
            )));
        });
    }

    #[test]
    fn assert_serially_locked_when_actually_locked_parallel() {
        const NAME1: &str = "assert_serially_locked_when_actually_locked_parallel-NAME1";
        const NAME2: &str = "assert_serially_locked_when_actually_locked_parallel-NAME2";
        local_parallel_core(vec![NAME1, NAME2], None, || {
            assert!(!is_locked_serially(Some(NAME1)));
            assert!(!is_locked_serially(Some(NAME2)));
            assert!(!is_locked_serially(Some(
                "no_such_name_assert_serially_locked_when_actually_locked_parallel"
            )));
        });
    }

    #[test]
    fn assert_serially_locked_outside_serial_lock() {
        const NAME1: &str = "assert_serially_locked_outside_serial_lock-NAME1";
        const NAME2: &str = "assert_serially_locked_outside_serial_lock-NAME2";
        assert!(!is_locked_serially(Some(NAME1)));
        assert!(!is_locked_serially(Some(NAME2)));

        local_serial_core(vec![NAME1], None, || {
            // ...
        });

        assert!(!is_locked_serially(Some(NAME1)));
        assert!(!is_locked_serially(Some(NAME2)));
    }

    #[test]
    fn assert_serially_locked_in_different_thread() {
        const NAME1: &str = "assert_serially_locked_in_different_thread-NAME1";
        const NAME2: &str = "assert_serially_locked_in_different_thread-NAME2";
        local_serial_core(vec![NAME1, NAME2], None, || {
            std::thread::spawn(|| {
                assert!(!is_locked_serially(Some(NAME2)));
            })
            .join()
            .unwrap();
        });
    }
}
