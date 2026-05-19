use std::panic;

use crate::file_lock::get_locks;

#[doc(hidden)]
pub fn fs_serial_core(names: Vec<&str>, path: Option<&str>, function: fn()) {
    assert!(names.len() > 0);
    let mut locks = get_locks(&names, path);
    locks.iter_mut().for_each(|lock| lock.start_serial());
    let res = panic::catch_unwind(function);
    locks.into_iter().for_each(|lock| lock.end_serial());
    if let Err(err) = res {
        panic::resume_unwind(err);
    }
}

#[doc(hidden)]
pub fn fs_serial_core_with_return<E>(
    names: Vec<&str>,
    path: Option<&str>,
    function: fn() -> Result<(), E>,
) -> Result<(), E> {
    let mut locks = get_locks(&names, path);
    locks.iter_mut().for_each(|lock| lock.start_serial());
    let res = panic::catch_unwind(function);
    locks.into_iter().for_each(|lock| lock.end_serial());
    match res {
        Ok(ret) => ret,
        Err(err) => {
            panic::resume_unwind(err);
        }
    }
}

#[doc(hidden)]
#[cfg(feature = "async")]
pub async fn fs_async_serial_core_with_return<E>(
    names: Vec<&str>,
    path: Option<&str>,
    fut: impl std::future::Future<Output = Result<(), E>>,
) -> Result<(), E> {
    let mut locks = get_locks(&names, path);
    locks.iter_mut().for_each(|lock| lock.start_serial());
    let ret: Result<(), E> = fut.await;
    locks.into_iter().for_each(|lock| lock.end_serial());
    ret
}

#[doc(hidden)]
#[cfg(feature = "async")]
pub async fn fs_async_serial_core(
    names: Vec<&str>,
    path: Option<&str>,
    fut: impl std::future::Future<Output = ()>,
) {
    let mut locks = get_locks(&names, path);
    locks.iter_mut().for_each(|lock| lock.start_serial());
    fut.await;
    locks.into_iter().for_each(|lock| lock.end_serial());
}

#[cfg(test)]
mod tests {
    use std::panic;

    use fslock::LockFile;

    use super::fs_serial_core;
    use crate::file_lock::path_for_name;

    #[test]
    fn test_serial() {
        fs_serial_core(vec!["test"], None, || {});
    }

    #[test]
    fn unlock_on_assert_sync_without_return() {
        let lock_path = path_for_name("serial_unlock_on_assert_sync_without_return");
        let _ = panic::catch_unwind(|| {
            fs_serial_core(
                vec!["serial_unlock_on_assert_sync_without_return"],
                Some(&lock_path),
                || {
                    assert!(false);
                },
            )
        });
        let mut lockfile = LockFile::open(&lock_path).unwrap();
        assert!(lockfile.try_lock().unwrap());
    }
}
