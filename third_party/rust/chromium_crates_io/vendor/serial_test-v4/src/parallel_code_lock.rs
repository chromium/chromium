#![allow(clippy::await_holding_lock)]

use crate::code_lock::{check_new_key, global_locks};
#[cfg(feature = "async")]
use futures_util::FutureExt;
use std::panic;

fn get_locks(names: Vec<&str>) -> Vec<crate::code_lock::UniqueReentrantMutex> {
    names
        .into_iter()
        .map(|name| {
            check_new_key(name);
            global_locks()
                .get(name)
                .expect("key to be set")
                .get()
                .clone()
        })
        .collect::<Vec<_>>()
}

#[doc(hidden)]
pub fn local_parallel_core_with_return<E>(
    names: Vec<&str>,
    _path: Option<&str>,
    function: fn() -> Result<(), E>,
) -> Result<(), E> {
    let locks = get_locks(names);

    locks.iter().for_each(|lock| lock.start_parallel());
    let res = panic::catch_unwind(function);
    locks.iter().for_each(|lock| lock.end_parallel());
    match res {
        Ok(ret) => ret,
        Err(err) => {
            panic::resume_unwind(err);
        }
    }
}

#[doc(hidden)]
pub fn local_parallel_core(names: Vec<&str>, _path: Option<&str>, function: fn()) {
    let locks = get_locks(names);
    locks.iter().for_each(|lock| lock.start_parallel());
    let res = panic::catch_unwind(|| {
        function();
    });
    locks.iter().for_each(|lock| lock.end_parallel());
    if let Err(err) = res {
        panic::resume_unwind(err);
    }
}

#[doc(hidden)]
#[cfg(feature = "async")]
pub async fn local_async_parallel_core_with_return<E>(
    names: Vec<&str>,
    _path: Option<&str>,
    fut: impl std::future::Future<Output = Result<(), E>> + panic::UnwindSafe,
) -> Result<(), E> {
    let locks = get_locks(names);
    locks.iter().for_each(|lock| lock.start_parallel());
    let res = fut.catch_unwind().await;
    locks.iter().for_each(|lock| lock.end_parallel());
    match res {
        Ok(ret) => ret,
        Err(err) => {
            panic::resume_unwind(err);
        }
    }
}

#[doc(hidden)]
#[cfg(feature = "async")]
pub async fn local_async_parallel_core(
    names: Vec<&str>,
    _path: Option<&str>,
    fut: impl std::future::Future<Output = ()> + panic::UnwindSafe,
) {
    let locks = get_locks(names);
    locks.iter().for_each(|lock| lock.start_parallel());
    let res = fut.catch_unwind().await;
    locks.iter().for_each(|lock| lock.end_parallel());
    if let Err(err) = res {
        panic::resume_unwind(err);
    }
}

#[cfg(test)]
mod tests {
    #[cfg(feature = "async")]
    use crate::{local_async_parallel_core, local_async_parallel_core_with_return};

    use crate::{code_lock::global_locks, local_parallel_core, local_parallel_core_with_return};
    use std::{io::Error, panic};

    #[test]
    fn unlock_on_assert_sync_without_return() {
        let _ = panic::catch_unwind(|| {
            local_parallel_core(vec!["unlock_on_assert_sync_without_return"], None, || {
                assert!(false);
            })
        });
        assert_eq!(
            global_locks()
                .get("unlock_on_assert_sync_without_return")
                .unwrap()
                .get()
                .parallel_count(),
            0
        );
    }

    #[test]
    fn unlock_on_assert_sync_with_return() {
        let _ = panic::catch_unwind(|| {
            local_parallel_core_with_return(
                vec!["unlock_on_assert_sync_with_return"],
                None,
                || -> Result<(), Error> {
                    assert!(false);
                    Ok(())
                },
            )
        });
        assert_eq!(
            global_locks()
                .get("unlock_on_assert_sync_with_return")
                .unwrap()
                .get()
                .parallel_count(),
            0
        );
    }

    #[test]
    #[cfg(feature = "async")]
    fn unlock_on_assert_async_without_return() {
        async fn demo_assert() {
            assert!(false);
        }
        async fn call_serial_test_fn() {
            local_async_parallel_core(
                vec!["unlock_on_assert_async_without_return"],
                None,
                demo_assert(),
            )
            .await
        }
        // as per https://stackoverflow.com/a/66529014/320546
        let _ = panic::catch_unwind(|| {
            futures_executor::block_on(call_serial_test_fn());
        });
        assert_eq!(
            global_locks()
                .get("unlock_on_assert_async_without_return")
                .unwrap()
                .get()
                .parallel_count(),
            0
        );
    }

    #[test]
    #[cfg(feature = "async")]
    fn unlock_on_assert_async_with_return() {
        async fn demo_assert() -> Result<(), Error> {
            assert!(false);
            Ok(())
        }

        #[allow(unused_must_use)]
        async fn call_serial_test_fn() {
            local_async_parallel_core_with_return(
                vec!["unlock_on_assert_async_with_return"],
                None,
                demo_assert(),
            )
            .await;
        }

        // as per https://stackoverflow.com/a/66529014/320546
        let _ = panic::catch_unwind(|| {
            futures_executor::block_on(call_serial_test_fn());
        });
        assert_eq!(
            global_locks()
                .get("unlock_on_assert_async_with_return")
                .unwrap()
                .get()
                .parallel_count(),
            0
        );
    }
}
