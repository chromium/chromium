//! the ``StacktraceObserver`` looks up the stacktrace on the execution thread and computes a hash for it for dedupe

#[cfg(feature = "casr")]
use alloc::string::ToString;
use alloc::{borrow::Cow, string::String, vec::Vec};
use core::fmt::Debug;
#[cfg(feature = "casr")]
use core::hash::{Hash, Hasher};
#[cfg(feature = "casr")]
use std::collections::hash_map::DefaultHasher;
use std::{
    fs::{self, File},
    io::Read,
    path::Path,
    process::ChildStderr,
};

use backtrace::Backtrace;
use libafl_bolts::{Named, ownedref::OwnedRefMut};
#[allow(unused_imports)] // expect breaks here for some reason
#[cfg(feature = "casr")]
use libcasr::{
    asan::AsanStacktrace,
    constants::{
        STACK_FRAME_FILEPATH_IGNORE_REGEXES_CPP, STACK_FRAME_FILEPATH_IGNORE_REGEXES_GO,
        STACK_FRAME_FILEPATH_IGNORE_REGEXES_JAVA, STACK_FRAME_FILEPATH_IGNORE_REGEXES_PYTHON,
        STACK_FRAME_FILEPATH_IGNORE_REGEXES_RUST, STACK_FRAME_FUNCTION_IGNORE_REGEXES_CPP,
        STACK_FRAME_FUNCTION_IGNORE_REGEXES_GO, STACK_FRAME_FUNCTION_IGNORE_REGEXES_JAVA,
        STACK_FRAME_FUNCTION_IGNORE_REGEXES_PYTHON, STACK_FRAME_FUNCTION_IGNORE_REGEXES_RUST,
    },
    init_ignored_frames,
    stacktrace::{
        Filter, ParseStacktrace, STACK_FRAME_FILEPATH_IGNORE_REGEXES,
        STACK_FRAME_FUNCTION_IGNORE_REGEXES, Stacktrace, StacktraceEntry,
    },
};
#[cfg(not(feature = "casr"))]
use regex::Regex;
use serde::{Deserialize, Serialize};

use super::ObserverWithHashField;
use crate::{Error, executors::ExitKind, observers::Observer};

#[cfg(not(feature = "casr"))]
/// Collects the backtrace via [`Backtrace`] and [`Debug`]
/// ([`Debug`] is currently used for dev purposes, symbols hash will be used eventually)
#[must_use]
pub fn collect_backtrace() -> u64 {
    let b = Backtrace::new_unresolved();
    if b.frames().is_empty() {
        return 0;
    }
    let mut hash = 0;
    for frame in &b.frames()[1..] {
        hash ^= frame.ip() as u64;
    }
    // will use symbols later
    // let trace = format!("{:?}", b);
    // log::trace!("{}", trace);
    // log::info!(
    //     "backtrace collected with hash={} at pid={}",
    //     hash,
    //     std::process::id()
    // );
    hash
}

#[cfg(feature = "casr")]
/// Collects the backtrace via [`Backtrace`]
#[must_use]
pub fn collect_backtrace() -> u64 {
    let mut b = Backtrace::new_unresolved();
    if b.frames().is_empty() {
        return 0;
    }
    b.resolve();
    let mut strace = Stacktrace::new();
    for frame in &b.frames()[1..] {
        let mut strace_entry = StacktraceEntry::default();
        let symbols = frame.symbols();
        if symbols.len() > 1 {
            let symbol = &symbols[0];
            if let Some(name) = symbol.name() {
                strace_entry.function = name.as_str().map_or_else(String::new, str::to_string);
            }
            if let Some(file) = symbol.filename() {
                strace_entry.debug.file = file.to_string_lossy().to_string();
            }
            strace_entry.debug.line = u64::from(symbol.lineno().unwrap_or(0));
            strace_entry.debug.column = u64::from(symbol.colno().unwrap_or(0));
        }
        strace_entry.address = frame.ip() as u64;
        strace.push(strace_entry);
    }

    strace.filter();
    let mut s = DefaultHasher::new();
    strace.hash(&mut s);
    s.finish()
}

/// An enum encoding the types of harnesses
#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Eq)]
pub enum HarnessType {
    /// Harness type when the target is in the same process
    InProcess,
    /// Harness type when the target is a child process
    Child,
    /// Harness type with an external component filling the backtrace hash (e.g. `CrashBacktraceCollector` in `libafl_qemu`)
    External,
}

/// An observer looking at the backtrace after the harness crashes
#[derive(Serialize, Deserialize, Debug)]
pub struct BacktraceObserver<'a> {
    observer_name: Cow<'static, str>,
    hash: OwnedRefMut<'a, Option<u64>>,
    harness_type: HarnessType,
}

impl<'a> BacktraceObserver<'a> {
    #[cfg(not(feature = "casr"))]
    /// Creates a new [`BacktraceObserver`] with the given name.
    #[must_use]
    pub fn new<S>(
        observer_name: S,
        backtrace_hash: OwnedRefMut<'a, Option<u64>>,
        harness_type: HarnessType,
    ) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        Self {
            observer_name: observer_name.into(),
            hash: backtrace_hash,
            harness_type,
        }
    }

    #[cfg(feature = "casr")]
    /// Creates a new [`BacktraceObserver`] with the given name.
    #[must_use]
    pub fn new<S>(
        observer_name: S,
        backtrace_hash: OwnedRefMut<'a, Option<u64>>,
        harness_type: HarnessType,
    ) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        init_ignored_frames!("rust", "cpp", "go");
        Self {
            observer_name: observer_name.into(),
            hash: backtrace_hash,
            harness_type,
        }
    }

    /// Creates a new [`BacktraceObserver`] with the given name, owning a new `backtrace_hash` variable.
    #[must_use]
    pub fn owned<S>(observer_name: S, harness_type: HarnessType) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        Self::new(observer_name, OwnedRefMut::owned(None), harness_type)
    }

    /// Updates the hash value of this observer.
    fn update_hash(&mut self, hash: u64) {
        *self.hash.as_mut() = Some(hash);
    }

    /// Clears the current hash value (sets it to `None`)
    fn clear_hash(&mut self) {
        *self.hash.as_mut() = None;
    }

    /// Fill the hash value if the harness type is external
    pub fn fill_external(&mut self, hash: u64, exit_kind: &ExitKind) {
        if self.harness_type == HarnessType::External {
            if *exit_kind == ExitKind::Crash {
                self.update_hash(hash);
            } else {
                self.clear_hash();
            }
        }
    }
}

impl ObserverWithHashField for BacktraceObserver<'_> {
    /// Gets the hash value of this observer.
    fn hash(&self) -> Option<u64> {
        *self.hash.as_ref()
    }
}

impl<I, S> Observer<I, S> for BacktraceObserver<'_> {
    fn post_exec(&mut self, _state: &mut S, _input: &I, exit_kind: &ExitKind) -> Result<(), Error> {
        if self.harness_type == HarnessType::InProcess {
            if *exit_kind == ExitKind::Crash {
                self.update_hash(collect_backtrace());
            } else {
                self.clear_hash();
            }
        }
        Ok(())
    }

    fn post_exec_child(
        &mut self,
        state: &mut S,
        input: &I,
        exit_kind: &ExitKind,
    ) -> Result<(), Error> {
        self.post_exec(state, input, exit_kind)
    }
}

impl Named for BacktraceObserver<'_> {
    fn name(&self) -> &Cow<'static, str> {
        &self.observer_name
    }
}

/// static variable of ASAN log path
pub static ASAN_LOG_PATH: &str = "./asanlog"; // TODO make it unique

/// returns the recommended ASAN runtime flags to capture the backtrace correctly with `log_path` set
#[must_use]
pub fn get_asan_runtime_flags_with_log_path() -> String {
    let mut flags = get_asan_runtime_flags();
    flags.push_str(":log_path=");
    flags.push_str(ASAN_LOG_PATH);
    flags
}

/// returns the recommended ASAN runtime flags to capture the backtrace correctly
#[must_use]
pub fn get_asan_runtime_flags() -> String {
    let flags = [
        "exitcode=0",
        "abort_on_error=1",
        "handle_abort=1",
        "handle_segv=1",
        "handle_sigbus=1",
        "handle_sigill=1",
        "handle_sigfpe=1",
    ];

    flags.join(":")
}

/// An observer looking at the backtrace of target command using ASAN output. This observer is only compatible with a `ForkserverExecutor`.
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct AsanBacktraceObserver {
    observer_name: Cow<'static, str>,
    hash: Option<u64>,
}

impl AsanBacktraceObserver {
    #[cfg(not(feature = "casr"))]
    /// Creates a new [`BacktraceObserver`] with the given name.
    #[must_use]
    pub fn new<S>(observer_name: S) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        Self {
            observer_name: observer_name.into(),
            hash: None,
        }
    }

    #[cfg(feature = "casr")]
    /// Creates a new [`BacktraceObserver`] with the given name.
    #[must_use]
    pub fn new<S>(observer_name: S) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        init_ignored_frames!("rust", "cpp", "go");
        Self {
            observer_name: observer_name.into(),
            hash: None,
        }
    }

    /// read ASAN output from the child stderr and parse it.
    pub fn parse_asan_output_from_childstderr(
        &mut self,
        stderr: &mut ChildStderr,
    ) -> Result<(), Error> {
        let mut buf = Vec::new();
        stderr.read_to_end(&mut buf)?;
        self.parse_asan_output(&String::from_utf8_lossy(&buf));
        Ok(())
    }

    /// read ASAN output from the log file and parse it.
    pub fn parse_asan_output_from_asan_log_file(&mut self, pid: i32) -> Result<(), Error> {
        let log_path = format!("{ASAN_LOG_PATH}.{pid}");
        let mut asan_output = File::open(Path::new(&log_path))?;

        let mut buf = String::new();
        asan_output.read_to_string(&mut buf)?;
        fs::remove_file(&log_path)?;

        self.parse_asan_output(&buf);
        Ok(())
    }

    #[cfg(not(feature = "casr"))]
    /// parse ASAN error output emited by the target command and compute the hash
    pub fn parse_asan_output(&mut self, output: &str) {
        let mut hash = 0;
        let matcher = Regex::new("\\s*#[0-9]*\\s0x([0-9a-f]*)\\s.*").unwrap();
        matcher.captures_iter(output).for_each(|m| {
            let g = m.get(1).unwrap();
            hash ^= u64::from_str_radix(g.as_str(), 16).unwrap();
        });
        self.update_hash(hash);
    }

    #[cfg(feature = "casr")]
    /// parse ASAN error output emited by the target command and compute the hash
    pub fn parse_asan_output(&mut self, output: &str) {
        let mut hash = 0;
        if let Ok(st_vec) = AsanStacktrace::extract_stacktrace(output) {
            if let Ok(mut stacktrace) = AsanStacktrace::parse_stacktrace(&st_vec) {
                stacktrace.filter();
                let mut s = DefaultHasher::new();
                stacktrace.hash(&mut s);
                hash = s.finish();
            }
        }
        self.update_hash(hash);
    }

    /// Updates the hash value of this observer.
    fn update_hash(&mut self, hash: u64) {
        self.hash = Some(hash);
    }
}

impl ObserverWithHashField for AsanBacktraceObserver {
    /// Gets the hash value of this observer.
    fn hash(&self) -> Option<u64> {
        self.hash
    }
}

impl Default for AsanBacktraceObserver {
    fn default() -> Self {
        Self::new("AsanBacktraceObserver")
    }
}

impl<I, S> Observer<I, S> for AsanBacktraceObserver {}

impl Named for AsanBacktraceObserver {
    fn name(&self) -> &Cow<'static, str> {
        &self.observer_name
    }
}
