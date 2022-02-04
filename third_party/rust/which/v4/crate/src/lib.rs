//! which
//!
//! A Rust equivalent of Unix command `which(1)`.
//! # Example:
//!
//! To find which rustc executable binary is using:
//!
//! ```no_run
//! use which::which;
//! use std::path::PathBuf;
//!
//! let result = which("rustc").unwrap();
//! assert_eq!(result, PathBuf::from("/usr/bin/rustc"));
//!
//! ```

#[cfg(windows)]
#[macro_use]
extern crate lazy_static;

mod checker;
mod error;
mod finder;
#[cfg(windows)]
mod helper;

#[cfg(feature = "regex")]
use regex::Regex;
#[cfg(feature = "regex")]
use std::borrow::Borrow;
use std::env;
use std::fmt;
use std::path;

use std::ffi::OsStr;

use crate::checker::{CompositeChecker, ExecutableChecker, ExistedChecker};
pub use crate::error::*;
use crate::finder::Finder;

/// Find a exectable binary's path by name.
///
/// If given an absolute path, returns it if the file exists and is executable.
///
/// If given a relative path, returns an absolute path to the file if
/// it exists and is executable.
///
/// If given a string without path separators, looks for a file named
/// `binary_name` at each directory in `$PATH` and if it finds an executable
/// file there, returns it.
///
/// # Example
///
/// ```no_run
/// use which::which;
/// use std::path::PathBuf;
///
/// let result = which::which("rustc").unwrap();
/// assert_eq!(result, PathBuf::from("/usr/bin/rustc"));
///
/// ```
pub fn which<T: AsRef<OsStr>>(binary_name: T) -> Result<path::PathBuf> {
    which_all(binary_name).and_then(|mut i| i.next().ok_or(Error::CannotFindBinaryPath))
}

/// Find all binaries with `binary_name` in the path list `paths`, using `cwd` to resolve relative paths.
pub fn which_all<T: AsRef<OsStr>>(binary_name: T) -> Result<impl Iterator<Item = path::PathBuf>> {
    let cwd = env::current_dir().ok();

    let binary_checker = build_binary_checker();

    let finder = Finder::new();

    finder.find(binary_name, env::var_os("PATH"), cwd, binary_checker)
}

/// Find all binaries matching a regular expression in a the system PATH.
///
/// Only available when feature `regex` is enabled.
///
/// # Arguments
///
/// * `regex` - A regular expression to match binaries with
///
/// # Examples
///
/// Find Python executables:
///
/// ```no_run
/// use regex::Regex;
/// use which::which;
/// use std::path::PathBuf;
///
/// let re = Regex::new(r"python\d$").unwrap();
/// let binaries: Vec<PathBuf> = which::which_re(re).unwrap().collect();
/// let python_paths = vec![PathBuf::from("/usr/bin/python2"), PathBuf::from("/usr/bin/python3")];
/// assert_eq!(binaries, python_paths);
/// ```
///
/// Find all cargo subcommand executables on the path:
///
/// ```
/// use which::which_re;
/// use regex::Regex;
///
/// which_re(Regex::new("^cargo-.*").unwrap()).unwrap()
///     .for_each(|pth| println!("{}", pth.to_string_lossy()));
/// ```
#[cfg(feature = "regex")]
pub fn which_re(regex: impl Borrow<Regex>) -> Result<impl Iterator<Item = path::PathBuf>> {
    which_re_in(regex, env::var_os("PATH"))
}

/// Find `binary_name` in the path list `paths`, using `cwd` to resolve relative paths.
pub fn which_in<T, U, V>(binary_name: T, paths: Option<U>, cwd: V) -> Result<path::PathBuf>
where
    T: AsRef<OsStr>,
    U: AsRef<OsStr>,
    V: AsRef<path::Path>,
{
    which_in_all(binary_name, paths, cwd)
        .and_then(|mut i| i.next().ok_or(Error::CannotFindBinaryPath))
}

/// Find all binaries matching a regular expression in a list of paths.
///
/// Only available when feature `regex` is enabled.
///
/// # Arguments
///
/// * `regex` - A regular expression to match binaries with
/// * `paths` - A string containing the paths to search
///             (separated in the same way as the PATH environment variable)
///
/// # Examples
///
/// ```no_run
/// use regex::Regex;
/// use which::which;
/// use std::path::PathBuf;
///
/// let re = Regex::new(r"python\d$").unwrap();
/// let paths = Some("/usr/bin:/usr/local/bin");
/// let binaries: Vec<PathBuf> = which::which_re_in(re, paths).unwrap().collect();
/// let python_paths = vec![PathBuf::from("/usr/bin/python2"), PathBuf::from("/usr/bin/python3")];
/// assert_eq!(binaries, python_paths);
/// ```
#[cfg(feature = "regex")]
pub fn which_re_in<T>(
    regex: impl Borrow<Regex>,
    paths: Option<T>,
) -> Result<impl Iterator<Item = path::PathBuf>>
where
    T: AsRef<OsStr>,
{
    let binary_checker = build_binary_checker();

    let finder = Finder::new();

    finder.find_re(regex, paths, binary_checker)
}

/// Find all binaries with `binary_name` in the path list `paths`, using `cwd` to resolve relative paths.
pub fn which_in_all<T, U, V>(
    binary_name: T,
    paths: Option<U>,
    cwd: V,
) -> Result<impl Iterator<Item = path::PathBuf>>
where
    T: AsRef<OsStr>,
    U: AsRef<OsStr>,
    V: AsRef<path::Path>,
{
    let binary_checker = build_binary_checker();

    let finder = Finder::new();

    finder.find(binary_name, paths, Some(cwd), binary_checker)
}

fn build_binary_checker() -> CompositeChecker {
    CompositeChecker::new()
        .add_checker(Box::new(ExistedChecker::new()))
        .add_checker(Box::new(ExecutableChecker::new()))
}

/// An owned, immutable wrapper around a `PathBuf` containing the path of an executable.
///
/// The constructed `PathBuf` is the output of `which` or `which_in`, but `which::Path` has the
/// advantage of being a type distinct from `std::path::Path` and `std::path::PathBuf`.
///
/// It can be beneficial to use `which::Path` instead of `std::path::Path` when you want the type
/// system to enforce the need for a path that exists and points to a binary that is executable.
///
/// Since `which::Path` implements `Deref` for `std::path::Path`, all methods on `&std::path::Path`
/// are also available to `&which::Path` values.
#[derive(Clone, PartialEq)]
pub struct Path {
    inner: path::PathBuf,
}

impl Path {
    /// Returns the path of an executable binary by name.
    ///
    /// This calls `which` and maps the result into a `Path`.
    pub fn new<T: AsRef<OsStr>>(binary_name: T) -> Result<Path> {
        which(binary_name).map(|inner| Path { inner })
    }

    /// Returns the paths of all executable binaries by a name.
    ///
    /// this calls `which_all` and maps the results into `Path`s.
    pub fn all<T: AsRef<OsStr>>(binary_name: T) -> Result<impl Iterator<Item = Path>> {
        which_all(binary_name).map(|inner| inner.map(|inner| Path { inner }))
    }

    /// Returns the path of an executable binary by name in the path list `paths` and using the
    /// current working directory `cwd` to resolve relative paths.
    ///
    /// This calls `which_in` and maps the result into a `Path`.
    pub fn new_in<T, U, V>(binary_name: T, paths: Option<U>, cwd: V) -> Result<Path>
    where
        T: AsRef<OsStr>,
        U: AsRef<OsStr>,
        V: AsRef<path::Path>,
    {
        which_in(binary_name, paths, cwd).map(|inner| Path { inner })
    }

    /// Returns all paths of an executable binary by name in the path list `paths` and using the
    /// current working directory `cwd` to resolve relative paths.
    ///
    /// This calls `which_in_all` and maps the results into a `Path`.
    pub fn all_in<T, U, V>(
        binary_name: T,
        paths: Option<U>,
        cwd: V,
    ) -> Result<impl Iterator<Item = Path>>
    where
        T: AsRef<OsStr>,
        U: AsRef<OsStr>,
        V: AsRef<path::Path>,
    {
        which_in_all(binary_name, paths, cwd).map(|inner| inner.map(|inner| Path { inner }))
    }

    /// Returns a reference to a `std::path::Path`.
    pub fn as_path(&self) -> &path::Path {
        self.inner.as_path()
    }

    /// Consumes the `which::Path`, yielding its underlying `std::path::PathBuf`.
    pub fn into_path_buf(self) -> path::PathBuf {
        self.inner
    }
}

impl fmt::Debug for Path {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Debug::fmt(&self.inner, f)
    }
}

impl std::ops::Deref for Path {
    type Target = path::Path;

    fn deref(&self) -> &path::Path {
        self.inner.deref()
    }
}

impl AsRef<path::Path> for Path {
    fn as_ref(&self) -> &path::Path {
        self.as_path()
    }
}

impl AsRef<OsStr> for Path {
    fn as_ref(&self) -> &OsStr {
        self.as_os_str()
    }
}

impl Eq for Path {}

impl PartialEq<path::PathBuf> for Path {
    fn eq(&self, other: &path::PathBuf) -> bool {
        self.inner == *other
    }
}

impl PartialEq<Path> for path::PathBuf {
    fn eq(&self, other: &Path) -> bool {
        *self == other.inner
    }
}

/// An owned, immutable wrapper around a `PathBuf` containing the _canonical_ path of an
/// executable.
///
/// The constructed `PathBuf` is the result of `which` or `which_in` followed by
/// `Path::canonicalize`, but `CanonicalPath` has the advantage of being a type distinct from
/// `std::path::Path` and `std::path::PathBuf`.
///
/// It can be beneficial to use `CanonicalPath` instead of `std::path::Path` when you want the type
/// system to enforce the need for a path that exists, points to a binary that is executable, is
/// absolute, has all components normalized, and has all symbolic links resolved
///
/// Since `CanonicalPath` implements `Deref` for `std::path::Path`, all methods on
/// `&std::path::Path` are also available to `&CanonicalPath` values.
#[derive(Clone, PartialEq)]
pub struct CanonicalPath {
    inner: path::PathBuf,
}

impl CanonicalPath {
    /// Returns the canonical path of an executable binary by name.
    ///
    /// This calls `which` and `Path::canonicalize` and maps the result into a `CanonicalPath`.
    pub fn new<T: AsRef<OsStr>>(binary_name: T) -> Result<CanonicalPath> {
        which(binary_name)
            .and_then(|p| p.canonicalize().map_err(|_| Error::CannotCanonicalize))
            .map(|inner| CanonicalPath { inner })
    }

    /// Returns the canonical paths of an executable binary by name.
    ///
    /// This calls `which_all` and `Path::canonicalize` and maps the results into `CanonicalPath`s.
    pub fn all<T: AsRef<OsStr>>(
        binary_name: T,
    ) -> Result<impl Iterator<Item = Result<CanonicalPath>>> {
        which_all(binary_name).map(|inner| {
            inner.map(|inner| {
                inner
                    .canonicalize()
                    .map_err(|_| Error::CannotCanonicalize)
                    .map(|inner| CanonicalPath { inner })
            })
        })
    }

    /// Returns the canonical path of an executable binary by name in the path list `paths` and
    /// using the current working directory `cwd` to resolve relative paths.
    ///
    /// This calls `which_in` and `Path::canonicalize` and maps the result into a `CanonicalPath`.
    pub fn new_in<T, U, V>(binary_name: T, paths: Option<U>, cwd: V) -> Result<CanonicalPath>
    where
        T: AsRef<OsStr>,
        U: AsRef<OsStr>,
        V: AsRef<path::Path>,
    {
        which_in(binary_name, paths, cwd)
            .and_then(|p| p.canonicalize().map_err(|_| Error::CannotCanonicalize))
            .map(|inner| CanonicalPath { inner })
    }

    /// Returns all of the canonical paths of an executable binary by name in the path list `paths` and
    /// using the current working directory `cwd` to resolve relative paths.
    ///
    /// This calls `which_in_all` and `Path::canonicalize` and maps the result into a `CanonicalPath`.
    pub fn all_in<T, U, V>(
        binary_name: T,
        paths: Option<U>,
        cwd: V,
    ) -> Result<impl Iterator<Item = Result<CanonicalPath>>>
    where
        T: AsRef<OsStr>,
        U: AsRef<OsStr>,
        V: AsRef<path::Path>,
    {
        which_in_all(binary_name, paths, cwd).map(|inner| {
            inner.map(|inner| {
                inner
                    .canonicalize()
                    .map_err(|_| Error::CannotCanonicalize)
                    .map(|inner| CanonicalPath { inner })
            })
        })
    }

    /// Returns a reference to a `std::path::Path`.
    pub fn as_path(&self) -> &path::Path {
        self.inner.as_path()
    }

    /// Consumes the `which::CanonicalPath`, yielding its underlying `std::path::PathBuf`.
    pub fn into_path_buf(self) -> path::PathBuf {
        self.inner
    }
}

impl fmt::Debug for CanonicalPath {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Debug::fmt(&self.inner, f)
    }
}

impl std::ops::Deref for CanonicalPath {
    type Target = path::Path;

    fn deref(&self) -> &path::Path {
        self.inner.deref()
    }
}

impl AsRef<path::Path> for CanonicalPath {
    fn as_ref(&self) -> &path::Path {
        self.as_path()
    }
}

impl AsRef<OsStr> for CanonicalPath {
    fn as_ref(&self) -> &OsStr {
        self.as_os_str()
    }
}

impl Eq for CanonicalPath {}

impl PartialEq<path::PathBuf> for CanonicalPath {
    fn eq(&self, other: &path::PathBuf) -> bool {
        self.inner == *other
    }
}

impl PartialEq<CanonicalPath> for path::PathBuf {
    fn eq(&self, other: &CanonicalPath) -> bool {
        *self == other.inner
    }
}
