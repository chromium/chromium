//! Expose an `Executor` based on a `Forkserver` in order to execute AFL/AFL++ binaries

use alloc::{string::ToString, vec::Vec};
use core::{
    fmt::{self, Debug, Formatter},
    marker::PhantomData,
    time::Duration,
};
use std::{
    env,
    ffi::OsString,
    io::{self, ErrorKind, Read, Write},
    os::{
        fd::{AsRawFd, BorrowedFd},
        unix::{io::RawFd, process::CommandExt},
    },
    path::PathBuf,
    process::{Child, Command, Stdio},
};

#[cfg(feature = "regex")]
use libafl_bolts::tuples::{Handle, Handled};
use libafl_bolts::{
    AsSlice, AsSliceMut, InputLocation, StdTargetArgs, StdTargetArgsInner, Truncate,
    core_affinity::CoreId,
    fs::{InputFile, get_unique_std_input_file},
    os::{dup2, last_error_str, pipes::Pipe},
    shmem::{ShMem, ShMemProvider, UnixShMem, UnixShMemProvider},
    tuples::{MatchNameRef, Prepend, RefIndexable},
};
use libc::RLIM_INFINITY;
use nix::{
    sys::{
        select::{FdSet, pselect},
        signal::{SigSet, Signal, kill},
        time::TimeSpec,
        wait::waitpid,
    },
    unistd::Pid,
};

use super::{HasTimeout, StdChildArgs, StdChildArgsInner};
#[cfg(feature = "regex")]
use crate::observers::{
    AsanBacktraceObserver, get_asan_runtime_flags, get_asan_runtime_flags_with_log_path,
};
use crate::{
    Error,
    executors::{Executor, ExitKind, HasObservers, SetTimeout},
    inputs::{Input, ToTargetBytes},
    mutators::Tokens,
    observers::{MapObserver, Observer, ObserversTuple},
    state::HasExecutions,
};

/// Pinned fd number for forkserver communication
pub const FORKSRV_FD: i32 = 198;
#[expect(clippy::cast_possible_wrap)]
const FS_NEW_ERROR: i32 = 0xeffe0000_u32 as i32;

/// Minimum number for new version
pub const FS_NEW_VERSION_MIN: u32 = 1;
/// Maximum number for new version
pub const FS_NEW_VERSION_MAX: u32 = 1;

/// Whether forkserver option customization for old forkserver is enabled
#[expect(clippy::cast_possible_wrap)]
pub const FS_OPT_ENABLED: i32 = 0x80000001_u32 as i32;

/// Set map size option for new forkserver
#[expect(clippy::cast_possible_wrap)]
pub const FS_NEW_OPT_MAPSIZE: i32 = 1_u32 as i32;
/// Set map size option for old forkserver
#[expect(clippy::cast_possible_wrap)]
pub const FS_OPT_MAPSIZE: i32 = 0x40000000_u32 as i32;

/// Enable shared memory fuzzing option for old forkserver
#[expect(clippy::cast_possible_wrap)]
pub const FS_OPT_SHDMEM_FUZZ: i32 = 0x01000000_u32 as i32;
/// Enable shared memory fuzzing option for new forkserver
#[expect(clippy::cast_possible_wrap)]
pub const FS_NEW_OPT_SHDMEM_FUZZ: i32 = 2_u32 as i32;

/// Enable autodict option for new forkserver
#[expect(clippy::cast_possible_wrap)]
pub const FS_NEW_OPT_AUTODTCT: i32 = 0x00000800_u32 as i32;
/// Enable autodict option for old forkserver
#[expect(clippy::cast_possible_wrap)]
pub const FS_OPT_AUTODTCT: i32 = 0x10000000_u32 as i32;

/// Failed to set map size
#[expect(clippy::cast_possible_wrap)]
pub const FS_ERROR_MAP_SIZE: i32 = 1_u32 as i32;
/// Failed to map address
#[expect(clippy::cast_possible_wrap)]
pub const FS_ERROR_MAP_ADDR: i32 = 2_u32 as i32;
/// Failed to open shared memory
#[expect(clippy::cast_possible_wrap)]
pub const FS_ERROR_SHM_OPEN: i32 = 4_u32 as i32;
/// Failed to do `shmat`
#[expect(clippy::cast_possible_wrap)]
pub const FS_ERROR_SHMAT: i32 = 8_u32 as i32;
/// Failed to do `mmap`
#[expect(clippy::cast_possible_wrap)]
pub const FS_ERROR_MMAP: i32 = 16_u32 as i32;
/// Old cmplog error
#[expect(clippy::cast_possible_wrap)]
pub const FS_ERROR_OLD_CMPLOG: i32 = 32_u32 as i32;
/// Old QEMU cmplog error
#[expect(clippy::cast_possible_wrap)]
pub const FS_ERROR_OLD_CMPLOG_QEMU: i32 = 64_u32 as i32;
/// Flag indicating this is an error
#[expect(clippy::cast_possible_wrap)]
pub const FS_OPT_ERROR: i32 = 0xf800008f_u32 as i32;

/// Forkserver message. We'll reuse it in a testcase.
const FAILED_TO_START_FORKSERVER_MSG: &str = "Failed to start forkserver";

fn report_error_and_exit(status: i32) -> Result<(), Error> {
    /* Report on the error received via the forkserver controller and exit */
    match status {
    FS_ERROR_MAP_SIZE =>
        Err(Error::unknown(
            format!(
            "{AFL_MAP_SIZE_ENV_VAR} is not set and fuzzing target reports that the required size is very large. Solution: Run the fuzzing target stand-alone with the environment variable AFL_DEBUG=1 set and set the value for __afl_final_loc in the {AFL_MAP_SIZE_ENV_VAR} environment variable for afl-fuzz."))),
    FS_ERROR_MAP_ADDR =>
        Err(Error::unknown(
            "the fuzzing target reports that hardcoded map address might be the reason the mmap of the shared memory failed. Solution: recompile the target with either afl-clang-lto and do not set AFL_LLVM_MAP_ADDR or recompile with afl-clang-fast.".to_string())),
    FS_ERROR_SHM_OPEN =>
        Err(Error::unknown("the fuzzing target reports that the shm_open() call failed.".to_string())),
    FS_ERROR_SHMAT =>
        Err(Error::unknown("the fuzzing target reports that the shmat() call failed.".to_string())),
    FS_ERROR_MMAP =>
        Err(Error::unknown("the fuzzing target reports that the mmap() call to the shared memory failed.".to_string())),
    FS_ERROR_OLD_CMPLOG =>
        Err(Error::unknown(
            "the -c cmplog target was instrumented with an too old AFL++ version, you need to recompile it.".to_string())),
    FS_ERROR_OLD_CMPLOG_QEMU =>
        Err(Error::unknown("The AFL++ QEMU/FRIDA loaders are from an older version, for -c you need to recompile it.".to_string())),
    _ =>
        Err(Error::unknown(format!("unknown error code {status} from fuzzing target!"))),
    }
}

/// The length of header bytes which tells shmem size
pub const SHMEM_FUZZ_HDR_SIZE: usize = 4;
/// Maximum default length for input
pub const MAX_INPUT_SIZE_DEFAULT: usize = 1024 * 1024;
/// Minimum default length for input
pub const MIN_INPUT_SIZE_DEFAULT: usize = 1;
/// Environment variable key for shared memory id for input and its len
pub const SHM_FUZZ_ENV_VAR: &str = "__AFL_SHM_FUZZ_ID";
/// Environment variable key for the page size (at least/usually `testcase_size_max + sizeof::<u32>()`)
pub const SHM_FUZZ_MAP_SIZE_ENV_VAR: &str = "__AFL_SHM_FUZZ_MAP_SIZE";

/// Environment variable key for shared memory id for edge map
pub const SHM_ENV_VAR: &str = "__AFL_SHM_ID";
/// Environment variable key for shared memory id for cmplog map
pub const SHM_CMPLOG_ENV_VAR: &str = "__AFL_CMPLOG_SHM_ID";

/// Environment variable key for a custom AFL coverage map size
pub const AFL_MAP_SIZE_ENV_VAR: &str = "AFL_MAP_SIZE";

/// Environment variable keys to skip instrumentation (LLVM variant).
pub const AFL_LLVM_ONLY_FSRV_VAR: &str = "AFL_LLVM_ONLY_FSRV";

/// Environment variable keys to skip instrumentation (GCC variant).
pub const AFL_GCC_ONLY_FSRV_VAR: &str = "AFL_GCC_ONLY_FSRV";

/// The default signal to use to kill child processes
const KILL_SIGNAL_DEFAULT: Signal = Signal::SIGTERM;

/// Configure the target, `limit`, `setsid`, `pipe_stdin`, the code was borrowed from the [`Angora`](https://github.com/AngoraFuzzer/Angora) fuzzer
pub trait ConfigTarget {
    /// Sets the sid
    fn setsid(&mut self) -> &mut Self;

    /// Sets a mem limit
    fn setlimit(&mut self, memlimit: u64) -> &mut Self;

    /// enables core dumps (rlimit = infinity)
    fn set_coredump(&mut self, enable: bool) -> &mut Self;

    /// Sets the AFL forkserver pipes
    ///
    /// # Safety
    /// All pipes must be valid file descriptors. They will be dup2-ed internally.
    unsafe fn setpipe(
        &mut self,
        st_read: RawFd,
        st_write: RawFd,
        ctl_read: RawFd,
        ctl_write: RawFd,
    ) -> &mut Self;

    /// [`dup2`] the specific `fd`, used for `stdio`
    ///
    /// # Safety
    /// The file descriptors must be valid. They will be `dup2-ed`.
    unsafe fn setdup2(&mut self, old_fd: RawFd, new_fd: RawFd) -> &mut Self;

    /// Bind children to a single core
    fn bind(&mut self, core: CoreId) -> &mut Self;
}

impl ConfigTarget for Command {
    fn setsid(&mut self) -> &mut Self {
        let func = move || {
            // # Safety
            // raw libc call without any parameters
            unsafe {
                if libc::setsid() == -1 {
                    log::warn!("Failed to set sid. Error: {:?}", last_error_str());
                }
            };
            Ok(())
        };
        unsafe { self.pre_exec(func) }
    }

    /// # Safety
    /// All pipes must be valid file descriptors. They will be dup2-ed internally.
    unsafe fn setpipe(
        &mut self,
        st_read: RawFd,
        st_write: RawFd,
        ctl_read: RawFd,
        ctl_write: RawFd,
    ) -> &mut Self {
        // # Safety
        // If this was called with correct parameters, we're good.
        unsafe {
            let func = move || {
                match dup2(ctl_read, FORKSRV_FD) {
                    Ok(()) => (),
                    Err(_) => {
                        return Err(io::Error::last_os_error());
                    }
                }

                match dup2(st_write, FORKSRV_FD + 1) {
                    Ok(()) => (),
                    Err(_) => {
                        return Err(io::Error::last_os_error());
                    }
                }
                libc::close(st_read);
                libc::close(st_write);
                libc::close(ctl_read);
                libc::close(ctl_write);
                Ok(())
            };
            self.pre_exec(func)
        }
    }

    // libc::rlim_t is i64 in freebsd and trivial_numeric_casts check will failed
    #[allow(trivial_numeric_casts)] // on 32 bit it does not trigger
    fn setlimit(&mut self, memlimit: u64) -> &mut Self {
        if memlimit == 0 {
            return self;
        }
        // # Safety
        // This method does not do shady pointer foo.
        // It merely call libc functions.
        let func = move || {
            let memlimit: libc::rlim_t = (memlimit as libc::rlim_t) << 20;
            let r = libc::rlimit {
                rlim_cur: memlimit,
                rlim_max: memlimit,
            };
            #[cfg(target_os = "openbsd")]
            let ret = unsafe { libc::setrlimit(libc::RLIMIT_RSS, &r) };
            #[cfg(not(target_os = "openbsd"))]
            let ret = unsafe { libc::setrlimit(libc::RLIMIT_AS, &raw const r) };
            if ret < 0 {
                return Err(io::Error::last_os_error());
            }
            Ok(())
        };
        // # Safety
        // This calls our non-shady function from above.
        unsafe { self.pre_exec(func) }
    }

    fn set_coredump(&mut self, enable: bool) -> &mut Self {
        let func = move || {
            let r0 = libc::rlimit {
                rlim_cur: if enable { RLIM_INFINITY } else { 0 },
                rlim_max: if enable { RLIM_INFINITY } else { 0 },
            };
            let ret = unsafe { libc::setrlimit(libc::RLIMIT_CORE, &raw const r0) };
            if ret < 0 {
                return Err(io::Error::last_os_error());
            }
            Ok(())
        };
        // # Safety
        // This calls our non-shady function from above.
        unsafe { self.pre_exec(func) }
    }

    unsafe fn setdup2(&mut self, old_fd: RawFd, new_fd: RawFd) -> &mut Self {
        let func = move || {
            // # Safety
            // The fd should be valid at this point - depending on parameters.
            let ret = unsafe { libc::dup2(old_fd, new_fd) };
            if ret < 0 {
                return Err(io::Error::last_os_error());
            }
            Ok(())
        };
        // # Safety
        // This calls our non-shady function from above.
        unsafe { self.pre_exec(func) }
    }

    fn bind(&mut self, core: CoreId) -> &mut Self {
        let func = move || {
            if let Err(e) = core.set_affinity_forced() {
                return Err(io::Error::other(e));
            }

            Ok(())
        };
        // # Safety
        // This calls our non-shady function from above.
        unsafe { self.pre_exec(func) }
    }
}

/// The [`Forkserver`] is communication channel with a child process that forks on request of the fuzzer.
/// The communication happens via pipe.
#[derive(Debug)]
pub struct Forkserver {
    /// The "actual" forkserver we spawned in the target
    fsrv_handle: Child,
    /// Status pipe
    st_pipe: Pipe,
    /// Control pipe
    ctl_pipe: Pipe,
    /// Pid of the current forked child (child of the forkserver) during execution
    child_pid: Option<Pid>,
    /// The last status reported to us by the in-target forkserver
    status: i32,
    /// If the last run timed out (in in-target i32)
    last_run_timed_out: i32,
    /// The signal this [`Forkserver`] will use to kill (defaults to [`self.kill_signal`])
    kill_signal: Signal,
}

impl Drop for Forkserver {
    fn drop(&mut self) {
        // Modelled after <https://github.com/AFLplusplus/AFLplusplus/blob/dee76993812fa9b5d8c1b75126129887a10befae/src/afl-forkserver.c#L1429>
        log::debug!("Dropping forkserver",);

        if let Some(pid) = self.child_pid {
            log::debug!("Sending {} to child {pid}", self.kill_signal);
            if let Err(err) = kill(pid, self.kill_signal) {
                log::warn!(
                    "Failed to deliver kill signal to child process {}: {err} ({})",
                    pid,
                    io::Error::last_os_error()
                );
            }
        }

        let forkserver_pid = Pid::from_raw(self.fsrv_handle.id().try_into().unwrap());
        if let Err(err) = kill(forkserver_pid, self.kill_signal) {
            log::warn!(
                "Failed to deliver {} signal to forkserver {}: {err} ({})",
                self.kill_signal,
                forkserver_pid,
                io::Error::last_os_error()
            );
            let _ = kill(forkserver_pid, Signal::SIGKILL);
        } else if let Err(err) = waitpid(forkserver_pid, None) {
            log::warn!(
                "Waitpid on forkserver {} failed: {err} ({})",
                forkserver_pid,
                io::Error::last_os_error()
            );
            let _ = kill(forkserver_pid, Signal::SIGKILL);
        }
    }
}

const fn fs_opt_get_mapsize(x: i32) -> i32 {
    ((x & 0x00fffffe) >> 1) + 1
}

#[expect(clippy::fn_params_excessive_bools)]
#[allow(unstable_name_collisions)]
impl Forkserver {
    /// Create a new [`Forkserver`] that will kill child processes
    /// with the given `kill_signal`.
    /// Using `Forkserver::new(..)` will default to [`Signal::SIGTERM`].
    #[expect(clippy::too_many_arguments)]
    fn new(
        target: OsString,
        args: Vec<OsString>,
        envs: Vec<(OsString, OsString)>,
        input_filefd: RawFd,
        use_stdin: bool,
        memlimit: u64,
        is_persistent: bool,
        is_deferred_frksrv: bool,
        is_fsrv_only: bool,
        dump_asan_logs: bool,
        coverage_map_size: Option<usize>,
        debug_output: bool,
        kill_signal: Signal,
        stdout_memfd: Option<RawFd>,
        stderr_memfd: Option<RawFd>,
        cwd: Option<PathBuf>,
        core: Option<CoreId>,
    ) -> Result<Self, Error> {
        let Some(coverage_map_size) = coverage_map_size else {
            return Err(Error::unknown(
                "Coverage map size unknown. Use coverage_map_size() to tell the forkserver about the map size.",
            ));
        };

        if env::var(SHM_ENV_VAR).is_err() {
            return Err(Error::unknown("__AFL_SHM_ID not set. It is necessary to set this env, otherwise the forkserver cannot communicate with the fuzzer".to_string()));
        }

        let afl_debug = if let Ok(afl_debug) = env::var("AFL_DEBUG") {
            if afl_debug != "1" && afl_debug != "0" {
                return Err(Error::illegal_argument("AFL_DEBUG must be either 1 or 0"));
            }
            afl_debug == "1"
        } else {
            false
        };

        let mut st_pipe = Pipe::new().unwrap();
        let mut ctl_pipe = Pipe::new().unwrap();

        let mut command = Command::new(target);
        // Setup args, stdio
        command.args(args);
        if use_stdin {
            // # Safety
            // We assume the file descriptors will be valid and not closed.
            unsafe {
                command.setdup2(input_filefd, libc::STDIN_FILENO);
            }
        } else {
            command.stdin(Stdio::null());
        }

        if debug_output {
            command.stdout(Stdio::inherit());
        } else if let Some(fd) = &stdout_memfd {
            // # Safety
            // We assume the file descriptors will be valid and not closed.
            unsafe {
                command.setdup2(*fd, libc::STDOUT_FILENO);
            }
            command.stdout(Stdio::null());
        } else {
            command.stdout(Stdio::null());
        }

        if debug_output {
            command.stderr(Stdio::inherit());
        } else if let Some(fd) = &stderr_memfd {
            // # Safety
            // We assume the file descriptors will be valid and not closed.
            unsafe {
                command.setdup2(*fd, libc::STDERR_FILENO);
            }
            command.stderr(Stdio::null());
        } else {
            command.stderr(Stdio::null());
        }

        if let Some(core) = core {
            command.bind(core);
        }

        command.env(AFL_MAP_SIZE_ENV_VAR, format!("{coverage_map_size}"));

        // Persistent, deferred forkserver
        if is_persistent {
            command.env("__AFL_PERSISTENT", "1");
        }

        if is_deferred_frksrv {
            command.env("__AFL_DEFER_FORKSRV", "1");
        }

        if is_fsrv_only {
            command.env(AFL_GCC_ONLY_FSRV_VAR, "1");
            command.env(AFL_LLVM_ONLY_FSRV_VAR, "1");
        }

        #[cfg(feature = "regex")]
        {
            let asan_options = if dump_asan_logs {
                get_asan_runtime_flags_with_log_path()
            } else {
                get_asan_runtime_flags()
            };
            command.env("ASAN_OPTIONS", asan_options);
        }
        #[cfg(not(feature = "regex"))]
        let _ = dump_asan_logs;

        if let Some(cwd) = cwd {
            command.current_dir(cwd);
        }

        // # Saftey
        // The pipe file descriptors used for `setpipe` are valid at this point.
        let fsrv_handle = unsafe {
            match ConfigTarget::setsid(
                command
                    .env("LD_BIND_NOW", "1")
                    .envs(envs)
                    .setlimit(memlimit)
                    .set_coredump(afl_debug),
            )
            .setpipe(
                st_pipe.read_end().unwrap(),
                st_pipe.write_end().unwrap(),
                ctl_pipe.read_end().unwrap(),
                ctl_pipe.write_end().unwrap(),
            )
            .spawn()
            {
                Ok(fsrv_handle) => fsrv_handle,
                Err(err) => {
                    return Err(Error::illegal_state(format!(
                        "Could not spawn the forkserver: {err:#?}"
                    )));
                }
            }
        };

        // Ctl_pipe.read_end and st_pipe.write_end are unnecessary for the parent, so we'll close them
        ctl_pipe.close_read_end();
        st_pipe.close_write_end();

        Ok(Self {
            fsrv_handle,
            st_pipe,
            ctl_pipe,
            child_pid: None,
            status: 0,
            last_run_timed_out: 0,
            kill_signal,
        })
    }

    /// If the last run timed out (as in-target i32)
    #[must_use]
    pub fn last_run_timed_out_raw(&self) -> i32 {
        self.last_run_timed_out
    }

    /// If the last run timed out
    #[must_use]
    pub fn last_run_timed_out(&self) -> bool {
        self.last_run_timed_out_raw() != 0
    }

    /// Sets if the last run timed out (as in-target i32)
    #[inline]
    pub fn set_last_run_timed_out_raw(&mut self, last_run_timed_out: i32) {
        self.last_run_timed_out = last_run_timed_out;
    }

    /// Sets if the last run timed out
    #[inline]
    pub fn set_last_run_timed_out(&mut self, last_run_timed_out: bool) {
        self.last_run_timed_out = i32::from(last_run_timed_out);
    }

    /// The status
    #[must_use]
    pub fn status(&self) -> i32 {
        self.status
    }

    /// Sets the status
    pub fn set_status(&mut self, status: i32) {
        self.status = status;
    }

    /// The child pid
    #[must_use]
    pub fn child_pid(&self) -> Pid {
        self.child_pid.unwrap()
    }

    /// Set the child pid
    pub fn set_child_pid(&mut self, child_pid: Pid) {
        self.child_pid = Some(child_pid);
    }

    /// Remove the child pid.
    pub fn reset_child_pid(&mut self) {
        self.child_pid = None;
    }

    /// Read from the st pipe
    pub fn read_st(&mut self) -> Result<i32, Error> {
        let mut buf: [u8; 4] = [0_u8; 4];
        let rlen = self.st_pipe.read(&mut buf)?;
        if rlen == size_of::<i32>() {
            Ok(i32::from_ne_bytes(buf))
        } else {
            // NOTE: The underlying API does not guarantee that the read will return
            //       exactly four bytes, but the chance of this happening is very low.
            //       This is a sacrifice of correctness for performance.
            Err(Error::illegal_state(format!(
                "Could not read from st pipe. Expected {} bytes, got {rlen} bytes",
                size_of::<i32>()
            )))
        }
    }

    /// Read bytes of any length from the st pipe
    pub fn read_st_of_len(&mut self, size: usize) -> Result<Vec<u8>, Error> {
        let mut buf = Vec::with_capacity(size);
        // SAFETY: `buf` will not be returned with `Ok` unless it is filled with `size` bytes.
        //         So it is ok to set the length to `size` such that the length of `&mut buf` is `size`
        //         and the `read_exact` call will try to read `size` bytes.
        #[allow(
            clippy::uninit_vec,
            reason = "The vec will be filled right after setting the length."
        )] // expect for some reason does not work
        unsafe {
            buf.set_len(size);
        }
        self.st_pipe.read_exact(&mut buf)?;
        Ok(buf)
    }

    /// Write to the ctl pipe
    pub fn write_ctl(&mut self, val: i32) -> Result<(), Error> {
        let slen = self.ctl_pipe.write(&val.to_ne_bytes())?;
        if slen == size_of::<i32>() {
            Ok(())
        } else {
            // NOTE: The underlying API does not guarantee that exactly four bytes
            //       are written, but the chance of this happening is very low.
            //       This is a sacrifice of correctness for performance.
            Err(Error::illegal_state(format!(
                "Could not write to ctl pipe. Expected {} bytes, wrote {slen} bytes",
                size_of::<i32>()
            )))
        }
    }

    /// Read a message from the child process.
    pub fn read_st_timed(&mut self, timeout: &TimeSpec) -> Result<Option<i32>, Error> {
        let mut buf: [u8; 4] = [0_u8; 4];
        let Some(st_read) = self.st_pipe.read_end() else {
            return Err(Error::os_error(
                io::Error::new(ErrorKind::BrokenPipe, "Read pipe end was already closed"),
                "read_st_timed failed",
            ));
        };

        // # Safety
        // The FDs are valid as this point in time.
        let st_read = unsafe { BorrowedFd::borrow_raw(st_read) };

        let mut readfds = FdSet::new();
        readfds.insert(st_read);
        // We'll pass a copied timeout to keep the original timeout intact, because select updates timeout to indicate how much time was left. See select(2)
        let sret = pselect(
            Some(readfds.highest().unwrap().as_raw_fd() + 1),
            &mut readfds,
            None,
            None,
            Some(timeout),
            Some(&SigSet::empty()),
        )?;
        if sret > 0 {
            if self.st_pipe.read_exact(&mut buf).is_ok() {
                let val: i32 = i32::from_ne_bytes(buf);
                Ok(Some(val))
            } else {
                Err(Error::unknown(
                    "Unable to communicate with fork server (OOM?)".to_string(),
                ))
            }
        } else {
            Ok(None)
        }
    }
}

/// This [`Executor`] can run binaries compiled for AFL/AFL++ that make use of a forkserver.
///
/// Shared memory feature is also available, but you have to set things up in your code.
/// Please refer to AFL++'s docs. <https://github.com/AFLplusplus/AFLplusplus/blob/stable/instrumentation/README.persistent_mode.md>
pub struct ForkserverExecutor<I, OT, S, SHM> {
    target: OsString,
    args: Vec<OsString>,
    input_file: InputFile,
    uses_shmem_testcase: bool,
    forkserver: Forkserver,
    observers: OT,
    map: Option<SHM>,
    phantom: PhantomData<fn() -> (I, S)>, // For Send/Sync
    map_size: Option<usize>,
    min_input_size: usize,
    max_input_size: usize,
    #[cfg(feature = "regex")]
    asan_obs: Handle<AsanBacktraceObserver>,
    timeout: TimeSpec,
    crash_exitcode: Option<i8>,
}

impl<I, OT, S, SHM> Debug for ForkserverExecutor<I, OT, S, SHM>
where
    OT: Debug,
    SHM: Debug,
{
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        f.debug_struct("ForkserverExecutor")
            .field("target", &self.target)
            .field("args", &self.args)
            .field("input_file", &self.input_file)
            .field("uses_shmem_testcase", &self.uses_shmem_testcase)
            .field("forkserver", &self.forkserver)
            .field("observers", &self.observers)
            .field("map", &self.map)
            .finish_non_exhaustive()
    }
}

impl ForkserverExecutor<(), (), UnixShMem, ()> {
    /// Builder for `ForkserverExecutor`
    #[must_use]
    pub fn builder() -> ForkserverExecutorBuilder<'static, UnixShMemProvider> {
        ForkserverExecutorBuilder::new()
    }
}

impl<I, OT, S, SHM> ForkserverExecutor<I, OT, S, SHM>
where
    OT: ObserversTuple<I, S>,
    SHM: ShMem,
{
    /// The `target` binary that's going to run.
    pub fn target(&self) -> &OsString {
        &self.target
    }

    /// The `args` used for the binary.
    pub fn args(&self) -> &[OsString] {
        &self.args
    }

    /// Get a reference to the [`Forkserver`] instance.
    pub fn forkserver(&self) -> &Forkserver {
        &self.forkserver
    }

    /// Get a mutable reference to the [`Forkserver`] instance.
    pub fn forkserver_mut(&mut self) -> &mut Forkserver {
        &mut self.forkserver
    }

    /// The [`InputFile`] used by this [`Executor`].
    pub fn input_file(&self) -> &InputFile {
        &self.input_file
    }

    /// The coverage map size if specified by the target
    pub fn coverage_map_size(&self) -> Option<usize> {
        self.map_size
    }

    /// Execute input and increase the execution counter.
    #[inline]
    fn execute_input(&mut self, state: &mut S, input: &[u8]) -> Result<ExitKind, Error>
    where
        S: HasExecutions,
    {
        *state.executions_mut() += 1;

        self.execute_input_uncounted(input)
    }

    fn map_input_to_shmem(&mut self, input: &[u8], input_size: usize) -> Result<(), Error> {
        let input_size_in_bytes = input_size.to_ne_bytes();
        if self.uses_shmem_testcase {
            debug_assert!(
                self.map.is_some(),
                "The uses_shmem_testcase() bool can only exist when a map is set"
            );
            // # Safety
            // Struct can never be created when uses_shmem_testcase is true and map is none.
            let map = unsafe { self.map.as_mut().unwrap_unchecked() };
            // The first four bytes declares the size of the shmem.
            map.as_slice_mut()[..SHMEM_FUZZ_HDR_SIZE]
                .copy_from_slice(&input_size_in_bytes[..SHMEM_FUZZ_HDR_SIZE]);
            map.as_slice_mut()[SHMEM_FUZZ_HDR_SIZE..(SHMEM_FUZZ_HDR_SIZE + input_size)]
                .copy_from_slice(&input[..input_size]);
        } else {
            self.input_file.write_buf(&input[..input_size])?;
        }
        Ok(())
    }

    /// Execute input, but side-step the execution counter.
    #[inline]
    fn execute_input_uncounted(&mut self, input: &[u8]) -> Result<ExitKind, Error> {
        let mut exit_kind = ExitKind::Ok;

        let last_run_timed_out = self.forkserver.last_run_timed_out_raw();

        let mut input_size = input.len();
        if input_size > self.max_input_size {
            // Truncate like AFL++ does
            input_size = self.max_input_size;
            self.map_input_to_shmem(input, input_size)?;
        } else if input_size < self.min_input_size {
            // Extend like AFL++ does
            input_size = self.min_input_size;
            let mut input_bytes_copy = Vec::with_capacity(input_size);
            input_bytes_copy
                .as_slice_mut()
                .copy_from_slice(input.as_slice());
            self.map_input_to_shmem(&input_bytes_copy, input_size)?;
        } else {
            self.map_input_to_shmem(input, input_size)?;
        }

        self.forkserver.set_last_run_timed_out(false);
        if let Err(err) = self.forkserver.write_ctl(last_run_timed_out) {
            return Err(Error::unknown(format!(
                "Unable to request new process from fork server (OOM?): {err:?}"
            )));
        }

        let pid = self.forkserver.read_st().map_err(|err| {
            Error::unknown(format!(
                "Unable to request new process from fork server (OOM?): {err:?}"
            ))
        })?;

        if pid <= 0 {
            return Err(Error::unknown(
                "Fork server is misbehaving (OOM?)".to_string(),
            ));
        }

        self.forkserver.set_child_pid(Pid::from_raw(pid));

        if let Some(status) = self.forkserver.read_st_timed(&self.timeout)? {
            self.forkserver.set_status(status);
            let exitcode_is_crash = if let Some(crash_exitcode) = self.crash_exitcode {
                (libc::WEXITSTATUS(self.forkserver().status()) as i8) == crash_exitcode
            } else {
                false
            };
            if libc::WIFSIGNALED(self.forkserver().status()) || exitcode_is_crash {
                exit_kind = ExitKind::Crash;
                #[cfg(feature = "regex")]
                if let Some(asan_observer) = self.observers.get_mut(&self.asan_obs) {
                    asan_observer.parse_asan_output_from_asan_log_file(pid)?;
                }
            }
        } else {
            self.forkserver.set_last_run_timed_out(true);

            // We need to kill the child in case he has timed out, or we can't get the correct pid in the next call to self.executor.forkserver_mut().read_st()?
            let _ = kill(self.forkserver().child_pid(), self.forkserver.kill_signal);
            if let Err(err) = self.forkserver.read_st() {
                return Err(Error::unknown(format!(
                    "Could not kill timed-out child: {err:?}"
                )));
            }
            exit_kind = ExitKind::Timeout;
        }

        if !libc::WIFSTOPPED(self.forkserver().status()) {
            self.forkserver.reset_child_pid();
        }

        Ok(exit_kind)
    }
}

/// The builder for `ForkserverExecutor`
#[derive(Debug)]
#[expect(clippy::struct_excessive_bools)]
pub struct ForkserverExecutorBuilder<'a, SP> {
    target_inner: StdTargetArgsInner,
    child_env_inner: StdChildArgsInner,
    uses_shmem_testcase: bool,
    is_persistent: bool,
    is_deferred_frksrv: bool,
    is_fsrv_only: bool,
    autotokens: Option<&'a mut Tokens>,
    shmem_provider: Option<&'a mut SP>,
    max_input_size: usize,
    min_input_size: usize,
    map_size: Option<usize>,
    kill_signal: Option<Signal>,
    #[cfg(feature = "regex")]
    asan_obs: Option<Handle<AsanBacktraceObserver>>,
    crash_exitcode: Option<i8>,
}

impl<SP> StdChildArgs for ForkserverExecutorBuilder<'_, SP> {
    fn inner(&self) -> &StdChildArgsInner {
        &self.child_env_inner
    }

    fn inner_mut(&mut self) -> &mut StdChildArgsInner {
        &mut self.child_env_inner
    }
}

impl<SP> StdTargetArgs for ForkserverExecutorBuilder<'_, SP> {
    fn inner(&self) -> &StdTargetArgsInner {
        &self.target_inner
    }

    fn inner_mut(&mut self) -> &mut StdTargetArgsInner {
        &mut self.target_inner
    }

    fn arg_input_arg(self) -> Self {
        panic!("ForkserverExecutor doesn't support mutating arguments")
    }
}

impl<'a, SHM, SP> ForkserverExecutorBuilder<'a, SP>
where
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    /// Builds `ForkserverExecutor`.
    /// This Forkserver will attempt to provide inputs over shared mem when `shmem_provider` is given.
    /// Else this forkserver will pass the input to the target via `stdin`
    /// in case no input file is specified.
    /// If `debug_child` is set, the child will print to `stdout`/`stderr`.
    #[expect(clippy::pedantic)]
    pub fn build<I, OT, S>(
        mut self,
        observers: OT,
    ) -> Result<ForkserverExecutor<I, OT, S, SHM>, Error>
    where
        OT: ObserversTuple<I, S>,
    {
        let (forkserver, input_file, map) = self.build_helper(&observers)?;

        let target = self.target_inner.program.take().unwrap();
        log::info!(
            "ForkserverExecutor: program: {:?}, arguments: {:?}, use_stdin: {:?}",
            target,
            self.target_inner.arguments.clone(),
            self.use_stdin()
        );

        if self.uses_shmem_testcase && map.is_none() {
            return Err(Error::illegal_state(
                "Map must always be set for `uses_shmem_testcase`",
            ));
        }

        let timeout: TimeSpec = self.child_env_inner.timeout.into();
        if self.min_input_size > self.max_input_size {
            return Err(Error::illegal_argument(
                format!(
                    "Minimum input size ({}) must not exceed maximum input size ({})",
                    self.min_input_size, self.max_input_size
                )
                .as_str(),
            ));
        }

        Ok(ForkserverExecutor {
            target,
            args: self.target_inner.arguments.clone(),
            input_file,
            uses_shmem_testcase: self.uses_shmem_testcase,
            forkserver,
            observers,
            map,
            phantom: PhantomData,
            map_size: self.map_size,
            min_input_size: self.min_input_size,
            max_input_size: self.max_input_size,
            timeout,
            #[cfg(feature = "regex")]
            asan_obs: self
                .asan_obs
                .clone()
                .unwrap_or(AsanBacktraceObserver::default().handle()),
            crash_exitcode: self.crash_exitcode,
        })
    }

    /// Builds `ForkserverExecutor` downsizing the coverage map to fit exaclty the AFL++ map size.
    #[expect(clippy::pedantic)]
    pub fn build_dynamic_map<A, MO, OT, I, S>(
        mut self,
        mut map_observer: A,
        other_observers: OT,
    ) -> Result<ForkserverExecutor<I, (A, OT), S, SHM>, Error>
    where
        A: Observer<I, S> + AsMut<MO>,
        I: Input,
        MO: MapObserver + Truncate, // TODO maybe enforce Entry = u8 for the cov map
        OT: ObserversTuple<I, S> + Prepend<MO>,
    {
        let (forkserver, input_file, map) = self.build_helper(&other_observers)?;

        let target = self.target_inner.program.take().unwrap();
        log::info!(
            "ForkserverExecutor: program: {:?}, arguments: {:?}, use_stdin: {:?}, map_size: {:?}",
            target,
            self.target_inner.arguments.clone(),
            self.use_stdin(),
            self.map_size
        );

        if let Some(dynamic_map_size) = self.map_size {
            map_observer.as_mut().truncate(dynamic_map_size);
        }

        let observers = (map_observer, other_observers);

        if self.uses_shmem_testcase && map.is_none() {
            return Err(Error::illegal_state(
                "Map must always be set for `uses_shmem_testcase`",
            ));
        }

        let timeout: TimeSpec = self.child_env_inner.timeout.into();

        Ok(ForkserverExecutor {
            target,
            args: self.target_inner.arguments.clone(),
            input_file,
            uses_shmem_testcase: self.uses_shmem_testcase,
            forkserver,
            observers,
            map,
            phantom: PhantomData,
            map_size: self.map_size,
            min_input_size: self.min_input_size,
            max_input_size: self.max_input_size,
            timeout,
            #[cfg(feature = "regex")]
            asan_obs: self
                .asan_obs
                .clone()
                .unwrap_or(AsanBacktraceObserver::default().handle()),
            crash_exitcode: self.crash_exitcode,
        })
    }

    #[expect(clippy::pedantic)]
    fn build_helper<I, OT, S>(
        &mut self,
        obs: &OT,
    ) -> Result<(Forkserver, InputFile, Option<SHM>), Error>
    where
        OT: ObserversTuple<I, S>,
    {
        let input_file = match &self.target_inner.input_location {
            InputLocation::StdIn {
                input_file: out_file,
            } => match out_file {
                Some(out_file) => out_file.clone(),
                None => InputFile::create(OsString::from(get_unique_std_input_file()))?,
            },
            InputLocation::Arg { argnum: _ } => {
                return Err(Error::illegal_argument(
                    "forkserver doesn't support argument mutation",
                ));
            }
            InputLocation::File { out_file } => out_file.clone(),
        };

        let map = match &mut self.shmem_provider {
            None => None,
            Some(provider) => {
                // setup shared memory
                let mut shmem = provider.new_shmem(self.max_input_size + SHMEM_FUZZ_HDR_SIZE)?;
                // # Safety
                // This is likely single threade here, we're likely fine if it's not.
                unsafe {
                    shmem.write_to_env(SHM_FUZZ_ENV_VAR)?;
                    env::set_var(SHM_FUZZ_MAP_SIZE_ENV_VAR, format!("{}", shmem.len()));
                }

                let size_in_bytes = (self.max_input_size + SHMEM_FUZZ_HDR_SIZE).to_ne_bytes();
                shmem.as_slice_mut()[..4].clone_from_slice(&size_in_bytes[..4]);
                Some(shmem)
            }
        };

        let mut forkserver = match &self.target_inner.program {
            Some(t) => Forkserver::new(
                t.clone(),
                self.target_inner.arguments.clone(),
                self.target_inner.envs.clone(),
                input_file.as_raw_fd(),
                self.use_stdin(),
                0,
                self.is_persistent,
                self.is_deferred_frksrv,
                self.is_fsrv_only,
                self.has_asan_obs(),
                self.map_size,
                self.child_env_inner.debug_child,
                self.kill_signal.unwrap_or(KILL_SIGNAL_DEFAULT),
                self.child_env_inner.stdout_observer.as_ref().map(|t| {
                    obs.get(t)
                        .as_ref()
                        .expect("stdout observer not passed in the builder")
                        .as_raw_fd()
                        .expect("only memory fd backend is allowed for forkserver executor")
                }),
                self.child_env_inner.stderr_observer.as_ref().map(|t| {
                    obs.get(t)
                        .as_ref()
                        .expect("stderr observer not passed in the builder")
                        .as_raw_fd()
                        .expect("only memory fd backend is allowed for forkserver executor")
                }),
                self.child_env_inner.current_directory.clone(),
                self.child_env_inner.core,
            )?,
            None => {
                return Err(Error::illegal_argument(
                    "ForkserverExecutorBuilder::build: target file not found".to_string(),
                ));
            }
        };

        // Initial handshake, read 4-bytes hello message from the forkserver.
        let version_status = forkserver.read_st().map_err(|err| {
            Error::illegal_state(format!("{FAILED_TO_START_FORKSERVER_MSG}: {err:?}"))
        })?;

        if (version_status & FS_NEW_ERROR) == FS_NEW_ERROR {
            report_error_and_exit(version_status & 0x0000ffff)?;
        }

        if Self::is_old_forkserver(version_status) {
            log::info!("Old fork server model is used by the target, this still works though.");
            self.initialize_old_forkserver(version_status, map.as_ref(), &mut forkserver)?;
        } else {
            self.initialize_forkserver(version_status, map.as_ref(), &mut forkserver)?;
        }
        Ok((forkserver, input_file, map))
    }

    fn is_old_forkserver(version_status: i32) -> bool {
        !(0x41464c00..0x41464cff).contains(&version_status)
    }

    /// Intialize forkserver > v4.20c
    #[expect(clippy::cast_possible_wrap)]
    #[expect(clippy::cast_sign_loss)]
    fn initialize_forkserver(
        &mut self,
        status: i32,
        map: Option<&SHM>,
        forkserver: &mut Forkserver,
    ) -> Result<(), Error> {
        let keep = status;
        let version: u32 = status as u32 - 0x41464c00_u32;
        match version {
            0 => {
                return Err(Error::illegal_state(
                    "Fork server version is not assigned, this should not happen. Recompile target.",
                ));
            }
            FS_NEW_VERSION_MIN..=FS_NEW_VERSION_MAX => {
                // good, do nothing
            }
            _ => {
                return Err(Error::illegal_state(
                    "Fork server version is not supported. Recompile the target.",
                ));
            }
        }

        let xored_status = (status as u32 ^ 0xffffffff) as i32;

        if let Err(err) = forkserver.write_ctl(xored_status) {
            return Err(Error::illegal_state(format!(
                "Writing to forkserver failed: {err:?}"
            )));
        }

        log::info!("All right - new fork server model version {version} is up");

        let status = forkserver.read_st().map_err(|err| {
            Error::illegal_state(format!("Reading from forkserver failed: {err:?}"))
        })?;

        if status & FS_NEW_OPT_MAPSIZE == FS_NEW_OPT_MAPSIZE {
            let fsrv_map_size = forkserver.read_st().map_err(|err| {
                Error::illegal_state(format!("Failed to read map size from forkserver: {err:?}"))
            })?;
            self.set_map_size(fsrv_map_size)?;
        }

        if status & FS_NEW_OPT_SHDMEM_FUZZ != 0 {
            if map.is_some() {
                log::info!("Using SHARED MEMORY FUZZING feature.");
                self.uses_shmem_testcase = true;
            } else {
                return Err(Error::illegal_state(
                    "Target requested sharedmem fuzzing, but you didn't prepare shmem",
                ));
            }
        }

        if status & FS_NEW_OPT_AUTODTCT != 0 {
            // Here unlike shmem input fuzzing, we are forced to read things
            // hence no self.autotokens.is_some() to check if we proceed
            let autotokens_size = forkserver.read_st().map_err(|err| {
                Error::illegal_state(format!(
                    "Failed to read autotokens size from forkserver: {err:?}",
                ))
            })?;

            let tokens_size_max = 0xffffff;

            if !(2..=tokens_size_max).contains(&autotokens_size) {
                return Err(Error::illegal_state(format!(
                    "Autotokens size is incorrect, expected 2 to {tokens_size_max} (inclusive), but got {autotokens_size}. Make sure your afl-cc verison is up to date."
                )));
            }
            log::info!("Autotokens size {autotokens_size:x}");
            let buf = forkserver
                .read_st_of_len(autotokens_size as usize)
                .map_err(|err| {
                    Error::illegal_state(format!("Failed to load autotokens: {err:?}"))
                })?;
            if let Some(t) = &mut self.autotokens {
                t.parse_autodict(&buf, autotokens_size as usize);
            }
        }

        let aflx = forkserver.read_st().map_err(|err| {
            Error::illegal_state(format!("Reading from forkserver failed: {err:?}"))
        })?;

        if aflx != keep {
            return Err(Error::unknown(format!(
                "Error in forkserver communication ({aflx:?}=>{keep:?})",
            )));
        }
        Ok(())
    }

    /// Intialize old forkserver. < v4.20c
    #[expect(clippy::cast_sign_loss)]
    fn initialize_old_forkserver(
        &mut self,
        status: i32,
        map: Option<&SHM>,
        forkserver: &mut Forkserver,
    ) -> Result<(), Error> {
        if status & FS_OPT_ENABLED == FS_OPT_ENABLED && status & FS_OPT_MAPSIZE == FS_OPT_MAPSIZE {
            let fsrv_map_size = fs_opt_get_mapsize(status);
            self.set_map_size(fsrv_map_size)?;
        }

        // Only with SHMEM or AUTODTCT we can send send_status back or it breaks!
        // If forkserver is responding, we then check if there's any option enabled.
        // We'll send 4-bytes message back to the forkserver to tell which features to use
        // The forkserver is listening to our response if either shmem fuzzing is enabled or auto dict is enabled
        // <https://github.com/AFLplusplus/AFLplusplus/blob/147654f8715d237fe45c1657c87b2fe36c4db22a/instrumentation/afl-compiler-rt.o.c#L1026>
        if status & FS_OPT_ENABLED == FS_OPT_ENABLED
            && (status & FS_OPT_SHDMEM_FUZZ == FS_OPT_SHDMEM_FUZZ
                || status & FS_OPT_AUTODTCT == FS_OPT_AUTODTCT)
        {
            let mut send_status = FS_OPT_ENABLED;

            if (status & FS_OPT_SHDMEM_FUZZ == FS_OPT_SHDMEM_FUZZ) && map.is_some() {
                log::info!("Using SHARED MEMORY FUZZING feature.");
                send_status |= FS_OPT_SHDMEM_FUZZ;
                self.uses_shmem_testcase = true;
            }

            if (status & FS_OPT_AUTODTCT == FS_OPT_AUTODTCT) && self.autotokens.is_some() {
                log::info!("Using AUTODTCT feature");
                send_status |= FS_OPT_AUTODTCT;
            }

            if send_status != FS_OPT_ENABLED {
                // if send_status is not changed (Options are available but we didn't use any), then don't send the next write_ctl message.
                // This is important

                if let Err(err) = forkserver.write_ctl(send_status) {
                    return Err(Error::illegal_state(format!(
                        "Writing to forkserver failed: {err:?}"
                    )));
                }

                if (send_status & FS_OPT_AUTODTCT) == FS_OPT_AUTODTCT {
                    let dict_size = forkserver.read_st().map_err(|err| {
                        Error::illegal_state(format!("Reading from forkserver failed: {err:?}"))
                    })?;

                    if !(2..=0xffffff).contains(&dict_size) {
                        return Err(Error::illegal_state(
                            "Dictionary has an illegal size".to_string(),
                        ));
                    }

                    log::info!("Autodict size {dict_size:x}");

                    let buf = forkserver
                        .read_st_of_len(dict_size as usize)
                        .map_err(|err| {
                            Error::unknown(format!("Failed to load autodictionary: {err:?}"))
                        })?;
                    if let Some(t) = &mut self.autotokens {
                        t.parse_autodict(&buf, dict_size as usize);
                    }
                }
            }
        } else {
            log::warn!("Forkserver Options are not available.");
        }

        Ok(())
    }

    #[expect(clippy::cast_sign_loss)]
    fn set_map_size(&mut self, fsrv_map_size: i32) -> Result<usize, Error> {
        // When 0, we assume that map_size was filled by the user or const
        /* TODO autofill map size from the observer

        if fsrv_map_size > 0 {
            self.map_size = Some(fsrv_map_size as usize);
        }
        */
        let mut actual_map_size = fsrv_map_size;
        if actual_map_size % 64 != 0 {
            actual_map_size = ((actual_map_size + 63) >> 6) << 6;
        }

        // TODO set AFL_MAP_SIZE
        if let Some(max_size) = self.map_size {
            if actual_map_size as usize > max_size {
                return Err(Error::illegal_state(format!(
                    "The target map size is {actual_map_size} but the allocated map size is {max_size}. \
                    Increase the initial size of the forkserver map to at least that size using the forkserver builder's `coverage_map_size`."
                )));
            }
        } else {
            return Err(Error::illegal_state(format!(
                "The target map size is {actual_map_size} but we did not create a coverage map before launching the target! \
                Set an initial forkserver map to at least that size using the forkserver builder's `coverage_map_size`."
            )));
        }

        // we'll use this later when we truncate the observer
        self.map_size = Some(actual_map_size as usize);

        Ok(actual_map_size as usize)
    }

    #[must_use]
    /// If set to true, we will only spin up a forkserver without any coverage collected. This is useful for several
    /// scenario like slave executors of SAND or cmplog executors.
    pub fn fsrv_only(mut self, fsrv_only: bool) -> Self {
        self.is_fsrv_only = fsrv_only;
        self
    }

    /// Use autodict?
    #[must_use]
    pub fn autotokens(mut self, tokens: &'a mut Tokens) -> Self {
        self.autotokens = Some(tokens);
        self
    }

    /// Set the max input size
    #[must_use]
    pub fn max_input_size(mut self, size: usize) -> Self {
        self.max_input_size = size;
        self
    }

    /// Set the min input size
    #[must_use]
    pub fn min_input_size(mut self, size: usize) -> Self {
        self.min_input_size = size;
        self
    }

    /// Call this if you want to run it under persistent mode; default is false
    #[must_use]
    pub fn is_persistent(mut self, is_persistent: bool) -> Self {
        self.is_persistent = is_persistent;
        self
    }

    /// Treats an execution as a crash if the provided exitcode is returned
    #[must_use]
    pub fn crash_exitcode(mut self, exitcode: i8) -> Self {
        self.crash_exitcode = Some(exitcode);
        self
    }

    /// Call this if the harness uses deferred forkserver mode; default is false
    #[must_use]
    pub fn is_deferred_frksrv(mut self, is_deferred_frksrv: bool) -> Self {
        self.is_deferred_frksrv = is_deferred_frksrv;
        self
    }

    /// Call this to set a defauult const coverage map size
    #[must_use]
    pub fn coverage_map_size(mut self, size: usize) -> Self {
        self.map_size = Some(size);
        self
    }

    /// Call this to set a signal to be used to kill child processes after executions
    #[must_use]
    pub fn kill_signal(mut self, kill_signal: Signal) -> Self {
        self.kill_signal = Some(kill_signal);
        self
    }

    /// Determine if the asan observer is present (always false if feature "regex" is disabled)
    #[cfg(feature = "regex")]
    #[must_use]
    pub fn has_asan_obs(&self) -> bool {
        self.asan_obs.is_some()
    }

    /// Determine if the asan observer is present (always false if feature "regex" is disabled)
    #[cfg(not(feature = "regex"))]
    #[must_use]
    pub fn has_asan_obs(&self) -> bool {
        false
    }
}

impl<'a> ForkserverExecutorBuilder<'a, UnixShMemProvider> {
    /// Creates a new `AFL`-style [`ForkserverExecutor`] with the given target, arguments and observers.
    /// This is the builder for `ForkserverExecutor`
    /// This Forkserver will attempt to provide inputs over shared mem when `shmem_provider` is given.
    /// Else this forkserver will pass the input to the target via `stdin`
    /// in case no input file is specified.
    /// If `debug_child` is set, the child will print to `stdout`/`stderr`.
    #[must_use]
    pub fn new() -> ForkserverExecutorBuilder<'a, UnixShMemProvider> {
        ForkserverExecutorBuilder {
            target_inner: StdTargetArgsInner::default(),
            child_env_inner: StdChildArgsInner::default(),
            uses_shmem_testcase: false,
            is_persistent: false,
            is_deferred_frksrv: false,
            is_fsrv_only: false,
            autotokens: None,
            shmem_provider: None,
            map_size: None,
            max_input_size: MAX_INPUT_SIZE_DEFAULT,
            min_input_size: MIN_INPUT_SIZE_DEFAULT,
            kill_signal: None,
            #[cfg(feature = "regex")]
            asan_obs: None,
            crash_exitcode: None,
        }
    }
}

impl<'a> ForkserverExecutorBuilder<'a, UnixShMemProvider> {
    /// Shmem provider for forkserver's shared memory testcase feature.
    pub fn shmem_provider<SP>(
        self,
        shmem_provider: &'a mut SP,
    ) -> ForkserverExecutorBuilder<'a, SP> {
        ForkserverExecutorBuilder {
            // Set the new provider
            shmem_provider: Some(shmem_provider),
            // Copy all other values from the old Builder
            target_inner: self.target_inner,
            child_env_inner: self.child_env_inner,
            uses_shmem_testcase: self.uses_shmem_testcase,
            is_persistent: self.is_persistent,
            is_deferred_frksrv: self.is_deferred_frksrv,
            is_fsrv_only: self.is_fsrv_only,
            autotokens: self.autotokens,
            map_size: self.map_size,
            max_input_size: self.max_input_size,
            min_input_size: self.min_input_size,
            kill_signal: self.kill_signal,
            #[cfg(feature = "regex")]
            asan_obs: self.asan_obs,
            crash_exitcode: self.crash_exitcode,
        }
    }
}

impl Default for ForkserverExecutorBuilder<'_, UnixShMemProvider> {
    fn default() -> Self {
        Self::new()
    }
}

impl<EM, I, OT, S, SHM, Z> Executor<EM, I, S, Z> for ForkserverExecutor<I, OT, S, SHM>
where
    OT: ObserversTuple<I, S>,
    S: HasExecutions,
    SHM: ShMem,
    Z: ToTargetBytes<I>,
{
    #[inline]
    fn run_target(
        &mut self,
        fuzzer: &mut Z,
        state: &mut S,
        _mgr: &mut EM,
        input: &I,
    ) -> Result<ExitKind, Error> {
        let bytes = fuzzer.to_target_bytes(input);
        self.observers_mut().pre_exec_child_all(state, input)?;
        let exit = self.execute_input(state, bytes.as_slice())?;
        self.observers_mut()
            .post_exec_child_all(state, input, &exit)?;
        Ok(exit)
    }
}

impl<I, OT, S, SHM> HasTimeout for ForkserverExecutor<I, OT, S, SHM> {
    #[inline]
    fn timeout(&self) -> Duration {
        self.timeout.into()
    }
}

impl<I, OT, S, SHM> SetTimeout for ForkserverExecutor<I, OT, S, SHM> {
    #[inline]
    fn set_timeout(&mut self, timeout: Duration) {
        self.timeout = TimeSpec::from_duration(timeout);
    }
}

impl<I, OT, S, SHM> HasObservers for ForkserverExecutor<I, OT, S, SHM>
where
    OT: ObserversTuple<I, S>,
{
    type Observers = OT;

    #[inline]
    fn observers(&self) -> RefIndexable<&Self::Observers, Self::Observers> {
        RefIndexable::from(&self.observers)
    }

    #[inline]
    fn observers_mut(&mut self) -> RefIndexable<&mut Self::Observers, Self::Observers> {
        RefIndexable::from(&mut self.observers)
    }
}

#[cfg(test)]
mod tests {
    use std::ffi::OsString;

    use libafl_bolts::{
        AsSliceMut, StdTargetArgs,
        shmem::{ShMem, ShMemProvider, UnixShMemProvider},
        tuples::tuple_list,
    };
    use serial_test::serial;

    use crate::{
        Error,
        corpus::NopCorpus,
        executors::{
            StdChildArgs,
            forkserver::{FAILED_TO_START_FORKSERVER_MSG, ForkserverExecutor},
        },
        inputs::BytesInput,
        observers::{ConstMapObserver, HitcountsMapObserver},
    };

    #[test]
    #[serial]
    #[cfg_attr(miri, ignore)]
    #[cfg_attr(target_pointer_width = "32", ignore)] // TODO: Why does this fail?
    fn test_forkserver() {
        const MAP_SIZE: usize = 65536;
        let bin = OsString::from("echo");
        let args = vec![OsString::from("@@")];

        let mut shmem_provider = UnixShMemProvider::new().unwrap();

        let mut shmem = shmem_provider.new_shmem(MAP_SIZE).unwrap();
        // # Safety
        // There's a slight chance this is racey but very unlikely in the normal use case
        unsafe {
            shmem.write_to_env("__AFL_SHM_ID").unwrap();
        }
        let shmem_buf: &mut [u8; MAP_SIZE] = shmem.as_slice_mut().try_into().unwrap();

        let edges_observer = HitcountsMapObserver::new(ConstMapObserver::<_, MAP_SIZE>::new(
            "shared_mem",
            shmem_buf,
        ));

        let executor = ForkserverExecutor::builder()
            .program(bin)
            .args(args)
            .coverage_map_size(MAP_SIZE)
            .debug_child(false)
            .shmem_provider(&mut shmem_provider)
            .build::<BytesInput, _, NopCorpus<BytesInput>>(tuple_list!(edges_observer));

        // Since /usr/bin/echo is not a instrumented binary file, the test will just check if the forkserver has failed at the initial handshake
        let result = match executor {
            Ok(_) => true,
            Err(e) => {
                println!("Error: {e:?}");
                match e {
                    Error::IllegalState(s, _) => s.contains(FAILED_TO_START_FORKSERVER_MSG),
                    _ => false,
                }
            }
        };
        assert!(result);
    }
}
