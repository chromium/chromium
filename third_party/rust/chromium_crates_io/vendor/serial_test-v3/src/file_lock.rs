use fslock::LockFile;
#[cfg(feature = "logging")]
use log::debug;
use std::{
    env,
    fs::{self, File},
    io::{Read, Write},
    path::Path,
    thread,
    time::Duration,
};

pub(crate) struct Lock {
    lockfile: LockFile,
    pub(crate) parallel_count: u32,
    path: String,
}

impl Lock {
    // Can't use the same file as fslock truncates it
    fn gen_count_file(path: &str) -> String {
        format!("{}-count", path)
    }

    fn read_parallel_count(path: &str) -> u32 {
        let parallel_count = match File::open(Lock::gen_count_file(path)) {
            Ok(mut file) => {
                let mut count_buf = [0; 4];
                match file.read_exact(&mut count_buf) {
                    Ok(_) => u32::from_ne_bytes(count_buf),
                    Err(_err) => {
                        #[cfg(feature = "logging")]
                        debug!("Error loading count file: {}", _err);
                        0u32
                    }
                }
            }
            Err(_) => 0,
        };

        #[cfg(feature = "logging")]
        debug!("Parallel count for {:?} is {}", path, parallel_count);
        parallel_count
    }

    fn create_lockfile(path: &str) -> LockFile {
        if !Path::new(path).exists() {
            fs::write(path, "").unwrap_or_else(|_| panic!("Lock file path was {:?}", path))
        }
        LockFile::open(path).unwrap()
    }

    pub(crate) fn new(path: &str) -> Lock {
        #[cfg(feature = "test_logging")]
        let _ = env_logger::builder().try_init();

        let mut lockfile = Self::create_lockfile(path);

        #[cfg(feature = "logging")]
        debug!("Waiting on {:?}", path);

        lockfile.lock().unwrap();

        #[cfg(feature = "logging")]
        debug!("Locked for {:?}", path);

        Lock {
            lockfile,
            parallel_count: Lock::read_parallel_count(path),
            path: String::from(path),
        }
    }

    pub(crate) fn is_locked(path: &str) -> bool {
        let mut lockfile = Self::create_lockfile(path);

        #[cfg(feature = "logging")]
        debug!("Checking lock on {:?}", path);

        if lockfile
            .try_lock()
            .expect("try_lock shouldn't generally fail, please provide a bug report")
        {
            #[cfg(feature = "test_logging")]
            debug!("{:?} wasn't locked", path);
            lockfile
                .unlock()
                .expect("unlock shouldn't generally fail, please provide a bug report");
            false
        } else {
            #[cfg(feature = "test_logging")]
            debug!("{:?} was locked", path);
            true
        }
    }

    pub(crate) fn start_serial(self: &mut Lock) {
        loop {
            if self.parallel_count == 0 {
                return;
            }
            #[cfg(feature = "logging")]
            debug!("Waiting because parallel count is {}", self.parallel_count);
            // unlock here is safe because we re-lock before returning
            self.unlock();
            thread::sleep(Duration::from_millis(50));
            self.lockfile
                .lock()
                .expect("unlock shouldn't generally fail, please provide a bug report");
            #[cfg(feature = "logging")]
            debug!("Locked for {:?}", self.path);
            self.parallel_count = Lock::read_parallel_count(&self.path)
        }
    }

    fn unlock(self: &mut Lock) {
        #[cfg(feature = "logging")]
        debug!("Unlocking {}", self.path);
        self.lockfile.unlock().unwrap();
    }

    pub(crate) fn end_serial(mut self: Lock) {
        self.unlock();
    }

    fn write_parallel(self: &Lock) {
        let mut file = File::create(&Lock::gen_count_file(&self.path)).unwrap();
        file.write_all(&self.parallel_count.to_ne_bytes()).unwrap();
    }

    pub(crate) fn start_parallel(self: &mut Lock) {
        self.parallel_count += 1;
        self.write_parallel();
        self.unlock();
    }

    pub(crate) fn end_parallel(mut self: Lock) {
        assert!(self.parallel_count > 0);
        self.parallel_count -= 1;
        self.write_parallel();
        self.unlock();
    }
}

pub(crate) fn path_for_name(name: &str) -> String {
    let mut pathbuf = env::temp_dir();
    pathbuf.push(format!("serial-test-{}", name));
    pathbuf.into_os_string().into_string().unwrap()
}

fn make_lock_for_name_and_path(name: &str, path_str: Option<&str>) -> Lock {
    if let Some(opt_path) = path_str {
        #[cfg(feature = "logging")]
        {
            let path = Path::new(opt_path);
            if !path.is_absolute() {
                debug!(
                    "Non-absolute path {opt_path} becomes {:?}",
                    path.canonicalize().unwrap_or_default()
                );
            }
        }
        Lock::new(opt_path)
    } else {
        let default_path = path_for_name(name);
        Lock::new(&default_path)
    }
}

pub(crate) fn get_locks(names: &Vec<&str>, path: Option<&str>) -> Vec<Lock> {
    #[cfg(feature = "test_logging")]
    let _ = env_logger::builder().try_init();

    if names.len() > 1 && path.is_some() {
        panic!("Can't do file_serial/parallel with both more than one name _and_ a specific path");
    }
    names
        .iter()
        .map(|name| make_lock_for_name_and_path(name, path))
        .collect::<Vec<_>>()
}

/// Check if the current thread is holding a `file_serial` lock
///
/// Can be used to assert that a piece of code can only be called
/// from a test marked `#[file_serial]`.
///
/// Example, with `#[file_serial]`:
///
/// ```no_run
/// use serial_test::{is_locked_file_serially, file_serial};
///
/// fn do_something_in_need_of_serialization() {
///     assert!(is_locked_file_serially(None, None));
///
///     // ...
/// }
///
/// #[test]
/// # fn unused() {}
/// #[file_serial]
/// fn main() {
///     do_something_in_need_of_serialization();
/// }
/// ```
///
/// Example, missing `#[file_serial]`:
///
/// ```no_run
/// use serial_test::{is_locked_file_serially, file_serial};
///
/// #[test]
/// # fn unused() {}
/// // #[file_serial] // <-- missing
/// fn main() {
///     assert!(is_locked_file_serially(None, None));
/// }
/// ```
///
/// Example, `#[test(some_key)]`:
///
/// ```no_run
/// use serial_test::{is_locked_file_serially, file_serial};
///
/// #[test]
/// # fn unused() {}
/// #[file_serial(some_key)]
/// fn main() {
///     assert!(is_locked_file_serially(Some("some_key"), None));
///     assert!(!is_locked_file_serially(None, None));
/// }
/// ```
pub fn is_locked_file_serially(name: Option<&str>, path: Option<&str>) -> bool {
    if let Some(opt_path) = path {
        Lock::is_locked(opt_path)
    } else {
        let default_path = path_for_name(name.unwrap_or_default());
        Lock::is_locked(&default_path)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{fs_parallel_core, fs_serial_core};

    fn init() {
        #[cfg(feature = "test_logging")]
        let _ = env_logger::builder().is_test(false).try_init();
    }

    #[test]
    fn assert_serially_locked_without_name() {
        init();
        fs_serial_core(vec![""], None, || {
            assert!(is_locked_file_serially(None, None));
            assert!(!is_locked_file_serially(
                Some("no_such_name_assert_serially_locked_without_name"),
                None
            ));
        });
    }

    #[test]
    fn assert_serially_locked_with_multiple_names() {
        const NAME1: &str = "assert_serially_locked_with_multiple_names-NAME1";
        const NAME2: &str = "assert_serially_locked_with_multiple_names-NAME2";
        init();

        fs_serial_core(vec![NAME1, NAME2], None, || {
            assert!(is_locked_file_serially(Some(NAME1), None));
            assert!(is_locked_file_serially(Some(NAME2), None));
            assert!(!is_locked_file_serially(
                Some("no_such_name_assert_serially_locked_with_multiple_names"),
                None
            ));
        });
    }

    #[test]
    fn assert_serially_locked_when_actually_locked_parallel() {
        const NAME1: &str = "assert_serially_locked_when_actually_locked_parallel-NAME1";
        const NAME2: &str = "assert_serially_locked_when_actually_locked_parallel-NAME2";
        init();

        fs_parallel_core(vec![NAME1, NAME2], None, || {
            assert!(!is_locked_file_serially(Some(NAME1), None));
            assert!(!is_locked_file_serially(Some(NAME2), None));
            assert!(!is_locked_file_serially(
                Some("no_such_name_assert_serially_locked_when_actually_locked_parallel"),
                None
            ));
        });
    }

    #[test]
    fn assert_serially_locked_outside_serial_lock() {
        const NAME1: &str = "assert_serially_locked_outside_serial_lock-NAME1";
        const NAME2: &str = "assert_serially_locked_outside_serial_lock-NAME2";
        init();

        assert!(!is_locked_file_serially(Some(NAME1), None));
        assert!(!is_locked_file_serially(Some(NAME2), None));

        fs_serial_core(vec![NAME1], None, || {
            // ...
        });

        assert!(!is_locked_file_serially(Some(NAME1), None));
        assert!(!is_locked_file_serially(Some(NAME2), None));
    }

    #[test]
    fn assert_serially_locked_in_different_thread() {
        const NAME1: &str = "assert_serially_locked_in_different_thread-NAME1";
        const NAME2: &str = "assert_serially_locked_in_different_thread-NAME2";

        init();
        fs_serial_core(vec![NAME1, NAME2], None, || {
            std::thread::spawn(|| {
                assert!(is_locked_file_serially(Some(NAME2), None));
            })
            .join()
            .unwrap();
        });
    }
}
