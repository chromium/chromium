//! Operating System specific abstractions

#[cfg(any(unix, all(windows, feature = "std")))]
use crate::Error;

#[cfg(all(unix, feature = "std"))]
pub mod unix_shmem_server;

#[cfg(unix)]
pub mod unix_signals;
#[cfg(unix)]
pub use unix_signals::CTRL_C_EXIT;

#[cfg(all(unix, feature = "alloc"))]
pub mod pipes;

#[cfg(all(unix, feature = "std"))]
use alloc::{borrow::Cow, ffi::CString};
#[cfg(all(unix, feature = "std"))]
use core::ffi::CStr;
#[cfg(all(unix, feature = "std"))]
use std::io::{stderr, stdout};
#[cfg(feature = "std")]
use std::{env, process::Command};
#[cfg(all(unix, feature = "std"))]
use std::{
    fs::File,
    os::fd::{AsRawFd, RawFd},
    sync::OnceLock,
};

// Allow a few extra features we need for the whole module
#[cfg(all(windows, feature = "std"))]
#[expect(missing_docs, overflowing_literals)]
pub mod windows_exceptions;
#[cfg(unix)]
use libc::pid_t;
#[cfg(all(windows, feature = "std"))]
pub use windows_exceptions::CTRL_C_EXIT;

/// A file that we keep open, pointing to /dev/null
#[cfg(all(feature = "std", unix))]
static NULL_FILE: OnceLock<File> = OnceLock::new();

/// Child Process Handle
#[cfg(unix)]
#[derive(Debug)]
pub struct ChildHandle {
    /// The process id
    pub pid: pid_t,
}

/// The special exit code when the target signal handler is crashing recursively
pub const SIGNAL_RECURSION_EXIT: i32 = 101;

#[cfg(unix)]
impl ChildHandle {
    /// Block until the child exited and the status code becomes available
    #[must_use]
    pub fn status(&self) -> i32 {
        let mut status = -1;
        unsafe {
            libc::waitpid(self.pid, &raw mut status, 0);
        }
        libc::WEXITSTATUS(status)
    }
}

/// The `ForkResult` (result of a fork)
#[cfg(unix)]
#[derive(Debug)]
pub enum ForkResult {
    /// The fork finished, we are the parent process.
    /// The child has the handle `ChildHandle`.
    Parent(ChildHandle),
    /// The fork finished, we are the child process.
    Child,
}

/// Unix has forks.
/// # Safety
/// A Normal fork. Runs on in two processes. Should be memory safe in general.
#[cfg(unix)]
pub unsafe fn fork() -> Result<ForkResult, Error> {
    unsafe {
        match libc::fork() {
            pid if pid > 0 => Ok(ForkResult::Parent(ChildHandle { pid })),
            pid if pid < 0 => {
                // Getting errno from rust is hard, we'll just let the libc print to stderr for now.
                // In any case, this should usually not happen.
                #[cfg(feature = "std")]
                {
                    let err_str = CString::new("Fork failed").unwrap();
                    libc::perror(err_str.as_ptr());
                }
                Err(Error::unknown(format!("Fork failed ({pid})")))
            }
            _ => Ok(ForkResult::Child),
        }
    }
}

/// Executes the current process from the beginning, as subprocess.
/// use `start_self.status()?` to wait for the child
#[cfg(feature = "std")]
pub fn startable_self() -> Result<Command, Error> {
    let mut startable = Command::new(env::current_exe()?);
    startable
        .current_dir(env::current_dir()?)
        .args(env::args().skip(1));
    Ok(startable)
}

/// "Safe" wrapper around `dup`, duplicating the given file descriptor
///
/// # Safety
/// The fd need to be a legal fd.
#[cfg(all(unix, feature = "std"))]
pub unsafe fn dup(fd: RawFd) -> Result<RawFd, Error> {
    match unsafe { libc::dup(fd) } {
        -1 => Err(Error::last_os_error(format!("Error calling dup({fd})"))),
        new_fd => Ok(new_fd),
    }
}

// Derived from https://github.com/RustPython/RustPython/blob/7996a10116681e9f85eda03413d5011b805e577f/stdlib/src/resource.rs#L113
// LICENSE: MIT https://github.com/RustPython/RustPython/commit/37355d612a451fba7fef8f13a1b9fdd51310b37e
/// Get the peak rss (Resident Set Size) of the all child processes
/// that have terminated and been waited for
#[cfg(all(unix, feature = "std"))]
pub fn peak_rss_mb_child_processes() -> Result<i64, Error> {
    use core::mem;
    use std::io;

    use libc::{RUSAGE_CHILDREN, rusage};

    let rss = unsafe {
        let mut rusage = mem::MaybeUninit::<rusage>::uninit();
        if libc::getrusage(RUSAGE_CHILDREN, rusage.as_mut_ptr()) == -1 {
            Err(io::Error::last_os_error())
        } else {
            Ok(rusage.assume_init())
        }
    }?;
    let result = rss.ru_maxrss >> 10;
    // on 32 bit pointer size, we need to convert the result.
    #[cfg(target_pointer_width = "32")]
    let result = result.into();
    Ok(result)
}

/// "Safe" wrapper around dup2
///
/// # Safety
/// The fds need to be legal fds.
#[cfg(all(unix, feature = "std"))]
pub unsafe fn dup2(fd: RawFd, device: RawFd) -> Result<(), Error> {
    match unsafe { libc::dup2(fd, device) } {
        -1 => Err(Error::last_os_error(format!(
            "Error calling dup2({fd}, {device})"
        ))),
        _ => Ok(()),
    }
}

/// Closes `stdout` and `stderr` and returns a new `stdout` and `stderr`
/// to be used in the fuzzer for further logging.
///
/// # Safety
/// The function in itiself is safe, but it might have undesirable side effects since it closes `stdout` and `stderr`.
#[cfg(all(unix, feature = "std"))]
#[expect(unused_qualifications)]
pub unsafe fn dup_and_mute_outputs() -> Result<(RawFd, RawFd), Error> {
    let old_stdout = stdout().as_raw_fd();
    let old_stderr = stderr().as_raw_fd();
    let null_fd = crate::os::null_fd()?;

    // # Safety
    // Duplicates the corect file descriptors.
    unsafe {
        let new_stdout = crate::os::dup(old_stdout)?;
        let new_stderr = crate::os::dup(old_stderr)?;

        crate::os::dup2(null_fd, old_stdout)?;
        crate::os::dup2(null_fd, old_stderr)?;

        Ok((new_stdout, new_stderr))
    }
}

/// Gets the stringified version of the last `errno`.
/// This is roughly equivalent to `strerror(errno)` in C.
#[cfg(all(unix, feature = "std"))]
#[must_use]
pub fn last_error_str<'a>() -> Option<Cow<'a, str>> {
    std::io::Error::last_os_error().raw_os_error().map(|errno| {
        // # Safety
        //
        // Calling the `strerror` libc functions with the correct `errno`
        unsafe { CStr::from_ptr(libc::strerror(errno)).to_string_lossy() }
    })
}

/// Get a file descriptor ([`RawFd`]) pointing to "/dev/null"
#[cfg(all(unix, feature = "std"))]
pub fn null_fd() -> Result<RawFd, Error> {
    // We don't care about opening the file twice here - races are ok.
    if let Some(file) = NULL_FILE.get() {
        Ok(file.as_raw_fd())
    } else {
        let null_file = File::open("/dev/null")?;
        Ok(NULL_FILE.get_or_init(move || null_file).as_raw_fd())
    }
}
