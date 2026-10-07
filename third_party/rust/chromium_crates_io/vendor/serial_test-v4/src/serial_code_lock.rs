#![allow(clippy::await_holding_lock)]

use crate::code_lock::{check_new_key, global_locks};

#[doc(hidden)]
macro_rules! core_internal {
    ($names: ident) => {
        let unlocks: Vec<_> = $names
            .into_iter()
            .map(|name| {
                check_new_key(name);
                global_locks()
                    .get(name)
                    .expect("key to be set")
                    .get()
                    .clone()
            })
            .collect();
        let _guards: Vec<_> = unlocks.iter().map(|unlock| unlock.lock()).collect();
    };
}

#[doc(hidden)]
pub fn local_serial_core_with_return<R, E>(
    names: Vec<&str>,
    _path: Option<String>,
    function: fn() -> Result<R, E>,
) -> Result<R, E> {
    core_internal!(names);
    function()
}

#[doc(hidden)]
pub fn local_serial_core(names: Vec<&str>, _path: Option<&str>, function: fn()) {
    core_internal!(names);
    function();
}

#[doc(hidden)]
#[cfg(feature = "async")]
pub async fn local_async_serial_core_with_return<R, E>(
    names: Vec<&str>,
    _path: Option<&str>,
    fut: impl std::future::Future<Output = Result<R, E>> + std::marker::Send,
) -> Result<R, E> {
    core_internal!(names);
    fut.await
}

#[doc(hidden)]
#[cfg(feature = "async")]
pub async fn local_async_serial_core(
    names: Vec<&str>,
    _path: Option<&str>,
    fut: impl std::future::Future<Output = ()>,
) {
    core_internal!(names);
    fut.await;
}

#[cfg(test)]
#[allow(clippy::print_stdout)]
mod tests {
    use super::local_serial_core;
    use crate::code_lock::{check_new_key, global_locks};
    use itertools::Itertools;
    use parking_lot::RwLock;
    use std::{
        sync::{Arc, Barrier},
        thread,
        time::Duration,
    };

    #[test]
    fn test_hammer_check_new_key() {
        let ptrs = Arc::new(RwLock::new(Vec::new()));
        let mut threads = Vec::new();

        let count = 100;
        let barrier = Arc::new(Barrier::new(count));

        for _ in 0..count {
            let local_locks = global_locks();
            let local_ptrs = ptrs.clone();
            let c = barrier.clone();
            threads.push(thread::spawn(move || {
                c.wait();
                check_new_key("foo");
                {
                    let unlock = local_locks.get("foo").expect("read didn't work");
                    let mutex = unlock.get();

                    let mut ptr_guard = local_ptrs
                        .try_write_for(Duration::from_secs(1))
                        .expect("write lock didn't work");
                    ptr_guard.push(mutex.id);
                }

                c.wait();
            }));
        }
        for thread in threads {
            thread.join().expect("thread join worked");
        }
        let ptrs_read_lock = ptrs
            .try_read_recursive_for(Duration::from_secs(1))
            .expect("ptrs read work");
        assert_eq!(ptrs_read_lock.len(), count);
        println!("{:?}", ptrs_read_lock);
        assert_eq!(ptrs_read_lock.iter().unique().count(), 1);
    }

    #[test]
    fn unlock_on_assert() {
        let _ = std::panic::catch_unwind(|| {
            local_serial_core(vec!["assert"], None, || {
                assert!(false);
            })
        });
        assert!(!global_locks().get("assert").unwrap().get().is_locked());
    }
}
