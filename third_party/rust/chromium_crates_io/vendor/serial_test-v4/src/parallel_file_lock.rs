use std::panic;

#[cfg(feature = "async")]
use futures_util::FutureExt;

use crate::file_lock::get_locks;

#[doc(hidden)]
pub fn fs_parallel_core(names: Vec<&str>, path: Option<&str>, function: fn()) {
    get_locks(&names, path)
        .iter_mut()
        .for_each(|lock| lock.start_parallel());
    let res = panic::catch_unwind(|| {
        function();
    });
    get_locks(&names, path)
        .into_iter()
        .for_each(|lock| lock.end_parallel());
    if let Err(err) = res {
        panic::resume_unwind(err);
    }
}

#[doc(hidden)]
pub fn fs_parallel_core_with_return<E>(
    names: Vec<&str>,
    path: Option<&str>,
    function: fn() -> Result<(), E>,
) -> Result<(), E> {
    get_locks(&names, path)
        .iter_mut()
        .for_each(|lock| lock.start_parallel());
    let res = panic::catch_unwind(function);
    get_locks(&names, path)
        .into_iter()
        .for_each(|lock| lock.end_parallel());
    match res {
        Ok(ret) => ret,
        Err(err) => {
            panic::resume_unwind(err);
        }
    }
}

#[doc(hidden)]
#[cfg(feature = "async")]
pub async fn fs_async_parallel_core_with_return<E>(
    names: Vec<&str>,
    path: Option<&str>,
    fut: impl std::future::Future<Output = Result<(), E>> + panic::UnwindSafe,
) -> Result<(), E> {
    get_locks(&names, path)
        .iter_mut()
        .for_each(|lock| lock.start_parallel());
    let res = fut.catch_unwind().await;
    get_locks(&names, path)
        .into_iter()
        .for_each(|lock| lock.end_parallel());
    match res {
        Ok(ret) => ret,
        Err(err) => {
            panic::resume_unwind(err);
        }
    }
}

#[doc(hidden)]
#[cfg(feature = "async")]
pub async fn fs_async_parallel_core(
    names: Vec<&str>,
    path: Option<&str>,
    fut: impl std::future::Future<Output = ()> + panic::UnwindSafe,
) {
    get_locks(&names, path)
        .iter_mut()
        .for_each(|lock| lock.start_parallel());

    let res = fut.catch_unwind().await;
    get_locks(&names, path)
        .into_iter()
        .for_each(|lock| lock.end_parallel());
    if let Err(err) = res {
        panic::resume_unwind(err);
    }
}

#[cfg(test)]
mod tests {
    #[cfg(feature = "async")]
    use crate::{fs_async_parallel_core, fs_async_parallel_core_with_return};

    use crate::{
        file_lock::{path_for_name, Lock},
        fs_parallel_core, fs_parallel_core_with_return,
    };
    use std::{io::Error, panic};

    fn unlock_ok(lock_path: &str) {
        let lock = Lock::new(lock_path);
        assert_eq!(lock.parallel_count, 0);
    }

    #[test]
    fn unlock_on_assert_sync_without_return() {
        let lock_path = path_for_name("parallel_unlock_on_assert_sync_without_return");
        let _ = panic::catch_unwind(|| {
            fs_parallel_core(
                vec!["parallel_unlock_on_assert_sync_without_return"],
                Some(&lock_path),
                || {
                    assert!(false);
                },
            )
        });
        unlock_ok(&lock_path);
    }

    #[test]
    fn unlock_on_assert_sync_with_return() {
        let lock_path = path_for_name("unlock_on_assert_sync_with_return");
        let _ = panic::catch_unwind(|| {
            fs_parallel_core_with_return(
                vec!["unlock_on_assert_sync_with_return"],
                Some(&lock_path),
                || -> Result<(), Error> {
                    assert!(false);
                    Ok(())
                },
            )
        });
        unlock_ok(&lock_path);
    }

    #[test]
    #[cfg(feature = "async")]
    fn unlock_on_assert_async_without_return() {
        let lock_path = path_for_name("unlock_on_assert_async_without_return");
        async fn demo_assert() {
            assert!(false);
        }
        async fn call_serial_test_fn(lock_path: &str) {
            fs_async_parallel_core(
                vec!["unlock_on_assert_async_without_return"],
                Some(&lock_path),
                demo_assert(),
            )
            .await
        }

        let _ = panic::catch_unwind(|| {
            futures_executor::block_on(call_serial_test_fn(&lock_path));
        });
        unlock_ok(&lock_path);
    }

    #[test]
    #[cfg(feature = "async")]
    fn unlock_on_assert_async_with_return() {
        let lock_path = path_for_name("unlock_on_assert_async_with_return");

        async fn demo_assert() -> Result<(), Error> {
            assert!(false);
            Ok(())
        }

        #[allow(unused_must_use)]
        async fn call_serial_test_fn(lock_path: &str) {
            fs_async_parallel_core_with_return(
                vec!["unlock_on_assert_async_with_return"],
                Some(&lock_path),
                demo_assert(),
            )
            .await;
        }

        let _ = panic::catch_unwind(|| {
            futures_executor::block_on(call_serial_test_fn(&lock_path));
        });
        unlock_ok(&lock_path);
    }
}
