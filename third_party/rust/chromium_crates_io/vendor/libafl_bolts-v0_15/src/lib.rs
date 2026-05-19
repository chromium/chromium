/*!
 * Welcome to `LibAFL_bolts`
 */
#![doc = include_str!("../README.md")]
/*! */
#![cfg_attr(feature = "document-features", doc = document_features::document_features!())]
#![no_std]
#![cfg_attr(not(test), warn(
    missing_debug_implementations,
    missing_docs,
    //trivial_casts,
    trivial_numeric_casts,
    unused_extern_crates,
    unused_import_braces,
    unused_qualifications,
    //unused_results
))]
#![cfg_attr(test, deny(
    missing_debug_implementations,
    missing_docs,
    //trivial_casts,
    trivial_numeric_casts,
    unused_extern_crates,
    unused_import_braces,
    unused_qualifications,
    unused_must_use,
    //unused_results
))]
#![cfg_attr(
    test,
    deny(
        bad_style,
        dead_code,
        improper_ctypes,
        non_shorthand_field_patterns,
        no_mangle_generic_items,
        overflowing_literals,
        path_statements,
        patterns_in_fns_without_body,
        unconditional_recursion,
        unused,
        unused_allocation,
        unused_comparisons,
        unused_parens,
        while_true
    )
)]

/// We need some sort of "[`String`]" for errors in `no_alloc`...
/// We can only support `'static` without allocator, so let's do that.
#[cfg(not(feature = "alloc"))]
type String = &'static str;

/// A simple non-allocating "format" string wrapper for no-std.
///
/// Problem is that we really need a non-allocating format...
/// This one simply returns the `fmt` string.
/// Good enough for simple errors, for anything else, use the `alloc` feature.
#[cfg(not(feature = "alloc"))]
macro_rules! format {
    ($fmt:literal) => {{ $fmt }};
}

#[cfg(feature = "std")]
#[macro_use]
extern crate std;
#[cfg(feature = "alloc")]
#[macro_use]
#[doc(hidden)]
pub extern crate alloc;

#[cfg(feature = "ctor")]
#[doc(hidden)]
pub use ctor;
#[cfg(feature = "alloc")]
pub mod anymap;
#[cfg(feature = "std")]
pub mod build_id;
#[cfg(all(
    any(feature = "cli", feature = "frida_cli", feature = "qemu_cli"),
    feature = "std"
))]
pub mod cli;
#[cfg(feature = "gzip")]
pub mod compress;
#[cfg(feature = "std")]
pub mod core_affinity;
pub mod cpu;
#[cfg(feature = "std")]
pub mod fs;
#[cfg(feature = "alloc")]
pub mod llmp;
pub mod math;
#[cfg(feature = "std")]
pub mod minibsod;
pub mod os;
#[cfg(feature = "alloc")]
pub mod ownedref;
pub mod rands;
#[cfg(feature = "alloc")]
pub mod serdeany;
pub mod shmem;
#[cfg(feature = "std")]
pub mod staterestore;
#[cfg(feature = "alloc")]
pub mod subrange;
// TODO: reenable once ahash works in no-alloc
#[cfg(any(feature = "xxh3", feature = "alloc"))]
pub mod tuples;

#[cfg(all(feature = "std", unix))]
pub mod argparse;
#[cfg(all(feature = "std", unix))]
pub use argparse::*;

#[cfg(feature = "std")]
pub mod target_args;
#[cfg(feature = "std")]
pub use target_args::*;

pub mod simd;

/// The purpose of this module is to alleviate imports of the bolts by adding a glob import.
#[cfg(feature = "prelude")]
pub mod bolts_prelude {
    #[cfg(feature = "std")]
    pub use super::build_id::*;
    #[cfg(all(
        any(feature = "cli", feature = "frida_cli", feature = "qemu_cli"),
        feature = "std"
    ))]
    pub use super::cli::*;
    #[cfg(feature = "gzip")]
    pub use super::compress::*;
    #[cfg(feature = "std")]
    pub use super::core_affinity::*;
    #[cfg(feature = "std")]
    pub use super::fs::*;
    #[cfg(all(feature = "std", unix))]
    pub use super::minibsod::*;
    #[cfg(feature = "std")]
    pub use super::staterestore::*;
    #[cfg(feature = "alloc")]
    pub use super::{anymap::*, llmp::*, ownedref::*, rands::*, serdeany::*, shmem::*, tuples::*};
    pub use super::{cpu::*, os::*};
}

#[cfg(all(unix, feature = "std"))]
use alloc::boxed::Box;
#[cfg(feature = "alloc")]
use alloc::{borrow::Cow, string::ToString, vec::Vec};
#[cfg(all(not(feature = "xxh3"), feature = "alloc"))]
use core::hash::BuildHasher;
#[cfg(any(feature = "xxh3", feature = "alloc"))]
use core::hash::{Hash, Hasher};
#[cfg(all(unix, feature = "std"))]
use core::mem;
#[cfg(feature = "std")]
use std::time::{SystemTime, UNIX_EPOCH};
#[cfg(all(unix, feature = "std"))]
use std::{
    fs::File,
    io::Write,
    os::fd::{FromRawFd, RawFd},
    panic,
};

// There's a bug in ahash that doesn't let it build in `alloc` without once_cell right now.
// TODO: re-enable once <https://github.com/tkaitchuck/aHash/issues/155> is resolved.
#[cfg(all(not(feature = "xxh3"), feature = "alloc"))]
use ahash::RandomState;
use log::SetLoggerError;
use serde::{Deserialize, Serialize};
#[cfg(feature = "xxh3")]
use xxhash_rust::xxh3::xxh3_64;

/// The client ID == the sender id.
#[repr(transparent)]
#[derive(
    Debug, Default, Copy, Clone, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize,
)]
pub struct ClientId(pub u32);

use core::{
    array::TryFromSliceError,
    fmt::{self, Display},
    num::{ParseIntError, TryFromIntError},
    ops::{Deref, DerefMut},
    time,
};
#[cfg(feature = "std")]
use std::{env::VarError, io};

#[cfg(feature = "libafl_derive")]
pub use libafl_derive::SerdeAny;
#[cfg(feature = "std")]
use log::{Metadata, Record};
#[cfg(feature = "alloc")]
use {
    alloc::string::{FromUtf8Error, String},
    core::cell::{BorrowError, BorrowMutError},
    core::str::Utf8Error,
};

/// Localhost addr, this is used, for example, for LLMP Client, which connects to this address
pub const IP_LOCALHOST: &str = "127.0.0.1";

/// We need fixed names for many parts of this lib.
#[cfg(feature = "alloc")]
pub trait Named {
    /// Provide the name of this element.
    fn name(&self) -> &Cow<'static, str>;
}

#[cfg(feature = "errors_backtrace")]
/// Error Backtrace type when `errors_backtrace` feature is enabled (== [`backtrace::Backtrace`])
pub type ErrorBacktrace = backtrace::Backtrace;

#[cfg(not(feature = "errors_backtrace"))]
#[derive(Debug, Default)]
/// ZST to use when `errors_backtrace` is disabled
pub struct ErrorBacktrace;

#[cfg(not(feature = "errors_backtrace"))]
impl ErrorBacktrace {
    /// Nop
    #[must_use]
    pub fn new() -> Self {
        Self
    }
}

#[cfg(feature = "errors_backtrace")]
fn display_error_backtrace(f: &mut fmt::Formatter, err: &ErrorBacktrace) -> fmt::Result {
    write!(f, "\nBacktrace: {err:?}")
}
#[cfg(not(feature = "errors_backtrace"))]
#[expect(clippy::unnecessary_wraps)]
fn display_error_backtrace(_f: &mut fmt::Formatter, _err: &ErrorBacktrace) -> fmt::Result {
    fmt::Result::Ok(())
}

/// Returns the standard input [`Hasher`]
///
/// Returns the hasher for the input with a given hash, depending on features:
/// [`xxh3_64`](https://docs.rs/xxhash-rust/latest/xxhash_rust/xxh3/fn.xxh3_64.html)
/// if the `xxh3` feature is used, /// else [`ahash`](https://docs.rs/ahash/latest/ahash/).
#[cfg(any(feature = "xxh3", feature = "alloc"))]
#[must_use]
pub fn hasher_std() -> impl Hasher + Clone {
    #[cfg(feature = "xxh3")]
    return xxhash_rust::xxh3::Xxh3::new();
    #[cfg(not(feature = "xxh3"))]
    RandomState::with_seeds(0, 0, 0, 0).build_hasher()
}

/// Hashes the input with a given hash
///
/// Hashes the input with a given hash, depending on features:
/// [`xxh3_64`](https://docs.rs/xxhash-rust/latest/xxhash_rust/xxh3/fn.xxh3_64.html)
/// if the `xxh3` feature is used, /// else [`ahash`](https://docs.rs/ahash/latest/ahash/).
#[cfg(any(feature = "xxh3", feature = "alloc"))]
#[must_use]
pub fn hash_std(input: &[u8]) -> u64 {
    #[cfg(feature = "xxh3")]
    return xxh3_64(input);
    #[cfg(not(feature = "xxh3"))]
    {
        let mut hasher = hasher_std();
        hasher.write(input);
        hasher.finish()
    }
}

/// Fast hash function for 64 bits integers minimizing collisions.
/// Adapted from <https://xorshift.di.unimi.it/splitmix64.c>
#[must_use]
pub fn hash_64_fast(mut x: u64) -> u64 {
    x = (x ^ (x >> 30)).wrapping_mul(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)).wrapping_mul(0x94d049bb133111eb);
    x ^ (x >> 31)
}

/// Hashes the input with a given hash
///
/// Hashes the input with a given hash, depending on features:
/// [`xxh3_64`](https://docs.rs/xxhash-rust/latest/xxhash_rust/xxh3/fn.xxh3_64.html)
/// if the `xxh3` feature is used, /// else [`ahash`](https://docs.rs/ahash/latest/ahash/).
///
/// If you have access to a `&[u8]` directly, [`hash_std`] may provide better performance
#[cfg(any(feature = "xxh3", feature = "alloc"))]
#[must_use]
pub fn generic_hash_std<I: Hash>(input: &I) -> u64 {
    let mut hasher = hasher_std();
    input.hash(&mut hasher);
    hasher.finish()
}

/// Main error struct for `LibAFL`
#[derive(Debug)]
pub enum Error {
    /// Serialization error
    Serialize(String, ErrorBacktrace),
    /// Compression error
    #[cfg(feature = "gzip")]
    Compression(ErrorBacktrace),
    /// Optional val was supposed to be set, but isn't.
    EmptyOptional(String, ErrorBacktrace),
    /// Key not in Map
    KeyNotFound(String, ErrorBacktrace),
    /// Key already exists and should not overwrite
    KeyExists(String, ErrorBacktrace),
    /// No elements in the current item
    Empty(String, ErrorBacktrace),
    /// End of iteration
    IteratorEnd(String, ErrorBacktrace),
    /// This is not supported (yet)
    NotImplemented(String, ErrorBacktrace),
    /// You're holding it wrong
    IllegalState(String, ErrorBacktrace),
    /// The argument passed to this method or function is not valid
    IllegalArgument(String, ErrorBacktrace),
    /// The performed action is not supported on the current platform
    Unsupported(String, ErrorBacktrace),
    /// Shutting down, not really an error.
    ShuttingDown,
    /// OS error, wrapping a [`io::Error`]
    #[cfg(feature = "std")]
    OsError(io::Error, String, ErrorBacktrace),
    /// Something else happened
    Unknown(String, ErrorBacktrace),
    /// Error with the corpora
    InvalidCorpus(String, ErrorBacktrace),
    /// Error specific to a runtime like QEMU or Frida
    Runtime(String, ErrorBacktrace),
    /// The `Input` was invalid.
    InvalidInput(String, ErrorBacktrace),
}

impl Error {
    /// Serialization error
    #[must_use]
    pub fn serialize<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::Serialize(arg.into(), ErrorBacktrace::new())
    }

    #[cfg(feature = "gzip")]
    /// Compression error
    #[must_use]
    pub fn compression() -> Self {
        Error::Compression(ErrorBacktrace::new())
    }

    /// Optional val was supposed to be set, but isn't.
    #[must_use]
    pub fn empty_optional<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::EmptyOptional(arg.into(), ErrorBacktrace::new())
    }

    /// The `Input` was invalid
    #[must_use]
    pub fn invalid_input<S>(reason: S) -> Self
    where
        S: Into<String>,
    {
        Error::InvalidInput(reason.into(), ErrorBacktrace::new())
    }

    /// Key not in Map
    #[must_use]
    pub fn key_not_found<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::KeyNotFound(arg.into(), ErrorBacktrace::new())
    }

    /// Key already exists in Map
    #[must_use]
    pub fn key_exists<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::KeyExists(arg.into(), ErrorBacktrace::new())
    }

    /// No elements in the current item
    #[must_use]
    pub fn empty<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::Empty(arg.into(), ErrorBacktrace::new())
    }

    /// End of iteration
    #[must_use]
    pub fn iterator_end<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::IteratorEnd(arg.into(), ErrorBacktrace::new())
    }

    /// This is not supported (yet)
    #[must_use]
    pub fn not_implemented<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::NotImplemented(arg.into(), ErrorBacktrace::new())
    }

    /// You're holding it wrong
    #[must_use]
    pub fn illegal_state<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::IllegalState(arg.into(), ErrorBacktrace::new())
    }

    /// The argument passed to this method or function is not valid
    #[must_use]
    pub fn illegal_argument<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::IllegalArgument(arg.into(), ErrorBacktrace::new())
    }

    /// Shutting down, not really an error.
    #[must_use]
    pub fn shutting_down() -> Self {
        Error::ShuttingDown
    }

    /// This operation is not supported on the current architecture or platform
    #[must_use]
    pub fn unsupported<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::Unsupported(arg.into(), ErrorBacktrace::new())
    }

    /// OS error with additional message
    #[cfg(feature = "std")]
    #[must_use]
    pub fn os_error<S>(err: io::Error, msg: S) -> Self
    where
        S: Into<String>,
    {
        Error::OsError(err, msg.into(), ErrorBacktrace::new())
    }

    /// OS error from [`io::Error::last_os_error`] with additional message
    #[cfg(feature = "std")]
    #[must_use]
    pub fn last_os_error<S>(msg: S) -> Self
    where
        S: Into<String>,
    {
        Error::OsError(
            io::Error::last_os_error(),
            msg.into(),
            ErrorBacktrace::new(),
        )
    }

    /// Something else happened
    #[must_use]
    pub fn unknown<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::Unknown(arg.into(), ErrorBacktrace::new())
    }

    /// Error with corpora
    #[must_use]
    pub fn invalid_corpus<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::InvalidCorpus(arg.into(), ErrorBacktrace::new())
    }

    /// Error specific to some runtime, like QEMU or Frida
    #[must_use]
    pub fn runtime<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::Runtime(arg.into(), ErrorBacktrace::new())
    }
}

impl core::error::Error for Error {
    #[cfg(feature = "std")]
    fn source(&self) -> Option<&(dyn core::error::Error + 'static)> {
        if let Self::OsError(err, _, _) = self {
            Some(err)
        } else {
            None
        }
    }
}

impl Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Self::Serialize(s, b) => {
                write!(f, "Error in Serialization: `{0}`", &s)?;
                display_error_backtrace(f, b)
            }
            #[cfg(feature = "gzip")]
            Self::Compression(b) => {
                write!(f, "Error in decompression")?;
                display_error_backtrace(f, b)
            }
            Self::EmptyOptional(s, b) => {
                write!(f, "Optional value `{0}` was not set", &s)?;
                display_error_backtrace(f, b)
            }
            Self::KeyNotFound(s, b) => {
                write!(f, "Key: `{0}` - not found", &s)?;
                display_error_backtrace(f, b)
            }
            Self::KeyExists(s, b) => {
                write!(f, "Key: `{0}` - already exists", &s)?;
                display_error_backtrace(f, b)
            }
            Self::Empty(s, b) => {
                write!(f, "No items in {0}", &s)?;
                display_error_backtrace(f, b)
            }
            Self::IteratorEnd(s, b) => {
                write!(f, "All elements have been processed in {0} iterator", &s)?;
                display_error_backtrace(f, b)
            }
            Self::NotImplemented(s, b) => {
                write!(f, "Not implemented: {0}", &s)?;
                display_error_backtrace(f, b)
            }
            Self::IllegalState(s, b) => {
                write!(f, "Illegal state: {0}", &s)?;
                display_error_backtrace(f, b)
            }
            Self::IllegalArgument(s, b) => {
                write!(f, "Illegal argument: {0}", &s)?;
                display_error_backtrace(f, b)
            }
            Self::Unsupported(s, b) => {
                write!(
                    f,
                    "The operation is not supported on the current platform: {0}",
                    &s
                )?;
                display_error_backtrace(f, b)
            }
            Self::ShuttingDown => write!(f, "Shutting down!"),
            #[cfg(feature = "std")]
            Self::OsError(err, s, b) => {
                write!(f, "OS error: {0}: {1}", &s, err)?;
                display_error_backtrace(f, b)
            }
            Self::Unknown(s, b) => {
                write!(f, "Unknown error: {0}", &s)?;
                display_error_backtrace(f, b)
            }
            Self::InvalidCorpus(s, b) => {
                write!(f, "Invalid corpus: {0}", &s)?;
                display_error_backtrace(f, b)
            }
            Self::Runtime(s, b) => {
                write!(f, "Runtime error: {0}", &s)?;
                display_error_backtrace(f, b)
            }
            Self::InvalidInput(s, b) => {
                write!(f, "Encountered an invalid input: {0}", &s)?;
                display_error_backtrace(f, b)
            }
        }
    }
}

#[cfg(feature = "alloc")]
impl From<BorrowError> for Error {
    fn from(err: BorrowError) -> Self {
        Self::illegal_state(format!(
            "Couldn't borrow from a RefCell as immutable: {err:?}"
        ))
    }
}

#[cfg(feature = "alloc")]
impl From<BorrowMutError> for Error {
    fn from(err: BorrowMutError) -> Self {
        Self::illegal_state(format!(
            "Couldn't borrow from a RefCell as mutable: {err:?}"
        ))
    }
}

/// Stringify the postcard serializer error
#[cfg(feature = "alloc")]
impl From<postcard::Error> for Error {
    fn from(err: postcard::Error) -> Self {
        Self::serialize(format!("{err:?}"))
    }
}

#[cfg(all(unix, feature = "std"))]
impl From<nix::Error> for Error {
    fn from(err: nix::Error) -> Self {
        Self::unknown(format!("Unix error: {err:?}"))
    }
}

/// Create an AFL Error from io Error
#[cfg(feature = "std")]
impl From<io::Error> for Error {
    fn from(err: io::Error) -> Self {
        Self::os_error(err, "io::Error ocurred")
    }
}

#[cfg(feature = "alloc")]
impl From<FromUtf8Error> for Error {
    fn from(err: FromUtf8Error) -> Self {
        Self::unknown(format!("Could not convert byte / utf-8: {err:?}"))
    }
}

#[cfg(feature = "alloc")]
impl From<Utf8Error> for Error {
    fn from(err: Utf8Error) -> Self {
        Self::unknown(format!("Could not convert byte / utf-8: {err:?}"))
    }
}

#[cfg(feature = "std")]
impl From<VarError> for Error {
    fn from(err: VarError) -> Self {
        Self::empty(format!("Could not get env var: {err:?}"))
    }
}

impl From<ParseIntError> for Error {
    #[allow(unused_variables)] // err is unused without std
    fn from(err: ParseIntError) -> Self {
        Self::unknown(format!("Failed to parse Int: {err:?}"))
    }
}

impl From<TryFromIntError> for Error {
    #[allow(unused_variables)] // err is unused without std
    fn from(err: TryFromIntError) -> Self {
        Self::illegal_state(format!("Expected conversion failed: {err:?}"))
    }
}

impl From<TryFromSliceError> for Error {
    #[allow(unused_variables)] // err is unused without std
    fn from(err: TryFromSliceError) -> Self {
        Self::illegal_argument(format!("Could not convert slice: {err:?}"))
    }
}

impl From<SetLoggerError> for Error {
    #[allow(unused_variables)] // err is unused without std
    fn from(err: SetLoggerError) -> Self {
        Self::illegal_state(format!("Failed to register logger: {err:?}"))
    }
}

#[cfg(windows)]
impl From<windows_result::Error> for Error {
    #[allow(unused_variables)] // err is unused without std
    fn from(err: windows_result::Error) -> Self {
        Self::unknown(format!("Windows API error: {err:?}"))
    }
}

#[cfg(feature = "python")]
impl From<pyo3::PyErr> for Error {
    fn from(err: pyo3::PyErr) -> Self {
        pyo3::Python::attach(|py| {
            if err
                .matches(
                    py,
                    pyo3::types::PyType::new::<pyo3::exceptions::PyKeyboardInterrupt>(py),
                )
                .unwrap()
            {
                Self::shutting_down()
            } else {
                Self::illegal_state(format!("Python exception: {err:?}"))
            }
        })
    }
}

/// The purpose of this module is to alleviate imports of many components by adding a glob import.
#[cfg(feature = "prelude")]
pub mod prelude {
    #![allow(ambiguous_glob_reexports)]

    pub use super::{bolts_prelude::*, *};
}

#[cfg(all(any(doctest, test), not(feature = "std")))]
/// Provide custom time in `no_std` tests.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn external_current_millis() -> u64 {
    // TODO: use "real" time here
    1000
}

/// Trait to convert into an Owned type
pub trait IntoOwned {
    /// Returns if the current type is an owned type.
    #[must_use]
    fn is_owned(&self) -> bool;

    /// Transfer the current type into an owned type.
    #[must_use]
    fn into_owned(self) -> Self;
}

/// Can be converted to a slice
pub trait AsSlice<'a> {
    /// Type of the entries of this slice
    type Entry: 'a;
    /// Type of the reference to this slice
    type SliceRef: Deref<Target = [Self::Entry]>;

    /// Convert to a slice
    fn as_slice(&'a self) -> Self::SliceRef;
}

/// Can be converted to a slice
pub trait AsSizedSlice<'a, const N: usize> {
    /// Type of the entries of this slice
    type Entry: 'a;
    /// Type of the reference to this slice
    type SliceRef: Deref<Target = [Self::Entry; N]>;

    /// Convert to a slice
    fn as_sized_slice(&'a self) -> Self::SliceRef;
}

impl<'a, T, R: ?Sized> AsSlice<'a> for R
where
    T: 'a,
    R: Deref<Target = [T]>,
{
    type Entry = T;
    type SliceRef = &'a [T];

    fn as_slice(&'a self) -> Self::SliceRef {
        self
    }
}

impl<'a, T, const N: usize, R: ?Sized> AsSizedSlice<'a, N> for R
where
    T: 'a,
    R: Deref<Target = [T; N]>,
{
    type Entry = T;
    type SliceRef = &'a [T; N];

    fn as_sized_slice(&'a self) -> Self::SliceRef {
        self
    }
}

/// Can be converted to a mutable slice
pub trait AsSliceMut<'a>: AsSlice<'a> {
    /// Type of the mutable reference to this slice
    type SliceRefMut: DerefMut<Target = [Self::Entry]>;

    /// Convert to a slice
    fn as_slice_mut(&'a mut self) -> Self::SliceRefMut;
}

/// Can be converted to a mutable slice
pub trait AsSizedSliceMut<'a, const N: usize>: AsSizedSlice<'a, N> {
    /// Type of the mutable reference to this slice
    type SliceRefMut: DerefMut<Target = [Self::Entry; N]>;

    /// Convert to a slice
    fn as_sized_slice_mut(&'a mut self) -> Self::SliceRefMut;
}

impl<'a, T, R: ?Sized> AsSliceMut<'a> for R
where
    T: 'a,
    R: DerefMut<Target = [T]>,
{
    type SliceRefMut = &'a mut [T];

    fn as_slice_mut(&'a mut self) -> Self::SliceRefMut {
        &mut *self
    }
}

impl<'a, T, const N: usize, R: ?Sized> AsSizedSliceMut<'a, N> for R
where
    T: 'a,
    R: DerefMut<Target = [T; N]>,
{
    type SliceRefMut = &'a mut [T; N];

    fn as_sized_slice_mut(&'a mut self) -> Self::SliceRefMut {
        &mut *self
    }
}

/// Create an `Iterator` from a reference
pub trait AsIter<'it> {
    /// The item type
    type Item: 'it;
    /// The ref type
    type Ref: Deref<Target = Self::Item>;
    /// The iterator type
    type IntoIter: Iterator<Item = Self::Ref>;

    /// Create an iterator from &self
    fn as_iter(&'it self) -> Self::IntoIter;
}

impl<'it, S, T> AsIter<'it> for S
where
    S: AsSlice<'it, Entry = T, SliceRef = &'it [T]>,
    T: 'it,
{
    type Item = S::Entry;
    type Ref = &'it Self::Item;
    type IntoIter = core::slice::Iter<'it, Self::Item>;

    fn as_iter(&'it self) -> Self::IntoIter {
        self.as_slice().iter()
    }
}

/// Create an `Iterator` from a mutable reference
pub trait AsIterMut<'it>: AsIter<'it> {
    /// The ref type
    type RefMut: DerefMut<Target = Self::Item>;
    /// The iterator type
    type IntoIterMut: Iterator<Item = Self::RefMut>;

    /// Create an iterator from &mut self
    fn as_iter_mut(&'it mut self) -> Self::IntoIterMut;
}

impl<'it, S, T> AsIterMut<'it> for S
where
    S: AsSliceMut<'it, Entry = T, SliceRef = &'it [T], SliceRefMut = &'it mut [T]>,
    T: 'it,
{
    type RefMut = &'it mut Self::Item;
    type IntoIterMut = core::slice::IterMut<'it, Self::Item>;

    fn as_iter_mut(&'it mut self) -> Self::IntoIterMut {
        self.as_slice_mut().iter_mut()
    }
}

/// Has a length field
pub trait HasLen {
    /// The length
    fn len(&self) -> usize;

    /// Returns `true` if it has no elements.
    fn is_empty(&self) -> bool {
        self.len() == 0
    }
}

#[cfg(feature = "alloc")]
impl<T> HasLen for Vec<T> {
    #[inline]
    fn len(&self) -> usize {
        Vec::<T>::len(self)
    }
}

impl<T: HasLen> HasLen for &mut T {
    fn len(&self) -> usize {
        self.deref().len()
    }
}

/// Has a ref count
pub trait HasRefCnt {
    /// The ref count
    fn refcnt(&self) -> isize;
    /// The ref count, mutable
    fn refcnt_mut(&mut self) -> &mut isize;
}

/// Trait to truncate slices and maps to a new size
pub trait Truncate {
    /// Reduce the size of the slice
    fn truncate(&mut self, len: usize);
}

/// Current time
#[cfg(feature = "std")]
#[must_use]
#[inline]
pub fn current_time() -> time::Duration {
    SystemTime::now().duration_since(UNIX_EPOCH).unwrap()
}

// external defined function in case of `no_std`
//
// Define your own `external_current_millis()` function via `extern "C"`
// which is linked into the binary and called from here.
#[cfg(all(not(any(doctest, test)), not(feature = "std")))]
unsafe extern "C" {
    //#[unsafe(no_mangle)]
    fn external_current_millis() -> u64;
}

/// Current time (fixed fallback for `no_std`)
#[cfg(not(feature = "std"))]
#[inline]
#[must_use]
pub fn current_time() -> time::Duration {
    let millis = unsafe { external_current_millis() };
    time::Duration::from_millis(millis)
}

/// Gets current nanoseconds since [`UNIX_EPOCH`]
#[must_use]
#[inline]
pub fn current_nanos() -> u64 {
    current_time().as_nanos() as u64
}

/// Gets current milliseconds since [`UNIX_EPOCH`]
#[must_use]
#[inline]
pub fn current_milliseconds() -> u64 {
    current_time().as_millis() as u64
}

/// Format a `Duration` into a HMS string
#[cfg(feature = "alloc")]
#[must_use]
pub fn format_duration(duration: &time::Duration) -> String {
    const MINS_PER_HOUR: u64 = 60;
    const HOURS_PER_DAY: u64 = 24;

    const SECS_PER_MINUTE: u64 = 60;
    const SECS_PER_HOUR: u64 = SECS_PER_MINUTE * MINS_PER_HOUR;
    const SECS_PER_DAY: u64 = SECS_PER_HOUR * HOURS_PER_DAY;

    let total_secs = duration.as_secs();
    let secs = total_secs % SECS_PER_MINUTE;

    if total_secs < SECS_PER_MINUTE {
        format!("{secs}s")
    } else {
        let mins = (total_secs / SECS_PER_MINUTE) % MINS_PER_HOUR;
        if total_secs < SECS_PER_HOUR {
            format!("{mins}m-{secs}s")
        } else {
            let hours = (total_secs / SECS_PER_HOUR) % HOURS_PER_DAY;
            if total_secs < SECS_PER_DAY {
                format!("{hours}h-{mins}m-{secs}s")
            } else {
                let days = total_secs / SECS_PER_DAY;
                format!("{days}days {hours}h-{mins}m-{secs}s")
            }
        }
    }
}

/// Format a number with thousands separators
#[cfg(feature = "alloc")]
#[must_use]
pub fn format_big_number(val: u64) -> String {
    let short = {
        let (num, unit) = match val {
            0..=999 => return format!("{val}"),
            1_000..=999_999 => (1000, "K"),
            1_000_000..=999_999_999 => (1_000_000, "M"),
            1_000_000_000..=999_999_999_999 => (1_000_000_000, "G"),
            _ => (1_000_000_000_000, "T"),
        };
        let main = val / num;
        let frac = (val % num) / (num / 100);
        format!(
            "{}.{}{}",
            main,
            format!("{frac:02}").trim_end_matches('0'),
            unit
        )
    };
    let long = val
        .to_string()
        .chars()
        .rev()
        .enumerate()
        .fold(String::new(), |mut acc, (i, c)| {
            if i > 0 && i % 3 == 0 {
                acc.push(',');
            }
            acc.push(c);
            acc
        })
        .chars()
        .rev()
        .collect::<String>();
    format!("{short} ({long})")
}

/// Stderr logger
#[cfg(feature = "std")]
pub static LIBAFL_STDERR_LOGGER: SimpleStderrLogger = SimpleStderrLogger::new();

/// Stdout logger
#[cfg(feature = "std")]
pub static LIBAFL_STDOUT_LOGGER: SimpleStdoutLogger = SimpleStdoutLogger::new();

/// A logger we can use log to raw fds.
#[cfg(all(unix, feature = "std"))]
static mut LIBAFL_RAWFD_LOGGER: SimpleFdLogger = unsafe { SimpleFdLogger::new(1) };

/// A simple logger struct that logs to stdout when used with [`log::set_logger`].
#[derive(Debug)]
#[cfg(feature = "std")]
pub struct SimpleStdoutLogger {}

#[cfg(feature = "std")]
impl Default for SimpleStdoutLogger {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(feature = "std")]
impl SimpleStdoutLogger {
    /// Create a new [`log::Log`] logger that will write log to stdout
    #[must_use]
    pub const fn new() -> Self {
        Self {}
    }

    /// register stdout logger
    pub fn set_logger() -> Result<(), Error> {
        log::set_logger(&LIBAFL_STDOUT_LOGGER)?;
        Ok(())
    }
}

#[cfg(feature = "std")]
#[cfg(target_os = "windows")]
#[allow(clippy::cast_ptr_alignment)]
#[must_use]
/// Return thread ID without using TLS
pub fn get_thread_id() -> u64 {
    use core::arch::asm;
    #[cfg(target_arch = "x86_64")]
    unsafe {
        let teb: *const u8;
        asm!("mov {}, gs:[0x30]", out(reg) teb);
        let thread_id_ptr = teb.add(0x48) as *const u32;
        u64::from(*thread_id_ptr)
    }

    #[cfg(target_arch = "x86")]
    unsafe {
        let teb: *const u8;
        asm!("mov {}, fs:[0x18]", out(reg) teb);
        let thread_id_ptr = teb.add(0x24) as *const u32;
        *thread_id_ptr as u64
    }
}

#[cfg(target_os = "linux")]
#[must_use]
#[allow(clippy::cast_sign_loss)]
/// Return thread ID without using TLS
pub fn get_thread_id() -> u64 {
    use libc::{SYS_gettid, syscall};

    unsafe { syscall(SYS_gettid) as u64 }
}

#[cfg(feature = "std")]
#[cfg(not(any(target_os = "windows", target_os = "linux")))]
#[must_use]
/// Return thread ID using Rust's `std::thread`
pub fn get_thread_id() -> u64 {
    // Fallback for other platforms
    let thread_id = std::thread::current().id();
    unsafe { mem::transmute::<_, u64>(thread_id) }
}

#[cfg(feature = "std")]
#[cfg(target_os = "windows")]
mod windows_logging {
    use core::ptr;

    use once_cell::sync::OnceCell;
    use winapi::um::{
        fileapi::WriteFile, handleapi::INVALID_HANDLE_VALUE, processenv::GetStdHandle,
        winbase::STD_OUTPUT_HANDLE, winnt::HANDLE,
    };

    // Safe wrapper around HANDLE
    struct StdOutHandle(HANDLE);

    // Implement Send and Sync for StdOutHandle, assuming it's safe to share
    unsafe impl Send for StdOutHandle {}
    unsafe impl Sync for StdOutHandle {}

    static H_STDOUT: OnceCell<StdOutHandle> = OnceCell::new();

    fn get_stdout_handle() -> HANDLE {
        H_STDOUT
            .get_or_init(|| {
                let handle = unsafe { GetStdHandle(STD_OUTPUT_HANDLE) };
                StdOutHandle(handle)
            })
            .0
    }
    /// A function that writes directly to stdout using `WinAPI`.
    /// Works much faster than println and does not need TLS
    pub fn direct_log(message: &str) {
        // Get the handle to standard output
        let h_stdout: HANDLE = get_stdout_handle();

        if ptr::addr_eq(h_stdout, INVALID_HANDLE_VALUE) {
            eprintln!("Failed to get standard output handle");
            return;
        }

        let bytes = message.as_bytes();
        let mut bytes_written = 0;

        // Write the message to standard output
        let result = unsafe {
            WriteFile(
                h_stdout,
                bytes.as_ptr() as *const _,
                bytes.len() as u32,
                &raw mut bytes_written,
                ptr::null_mut(),
            )
        };

        if result == 0 {
            eprintln!("Failed to write to standard output");
        }
    }
}

#[cfg(feature = "std")]
impl log::Log for SimpleStdoutLogger {
    #[inline]
    fn enabled(&self, _metadata: &Metadata) -> bool {
        true
    }

    #[cfg(not(target_os = "windows"))]
    fn log(&self, record: &Record) {
        println!(
            "[{:?}, {:?}:{:?}] {}: {}",
            current_time(),
            std::process::id(),
            get_thread_id(),
            record.level(),
            record.args()
        );
    }

    #[cfg(target_os = "windows")]
    fn log(&self, record: &Record) {
        // println is not safe in TLS-less environment
        let msg = format!(
            "[{:?}, {:?}:{:?}] {}: {}\n",
            current_time(),
            std::process::id(),
            get_thread_id(),
            record.level(),
            record.args()
        );
        windows_logging::direct_log(msg.as_str());
    }

    fn flush(&self) {}
}

/// A simple logger struct that logs to stderr when used with [`log::set_logger`].
#[derive(Debug)]
#[cfg(feature = "std")]
pub struct SimpleStderrLogger {}

#[cfg(feature = "std")]
impl Default for SimpleStderrLogger {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(feature = "std")]
impl SimpleStderrLogger {
    /// Create a new [`log::Log`] logger that will write log to stdout
    #[must_use]
    pub const fn new() -> Self {
        Self {}
    }

    /// register stderr logger
    pub fn set_logger() -> Result<(), Error> {
        log::set_logger(&LIBAFL_STDERR_LOGGER)?;
        Ok(())
    }
}

#[cfg(feature = "std")]
impl log::Log for SimpleStderrLogger {
    #[inline]
    fn enabled(&self, _metadata: &Metadata) -> bool {
        true
    }

    fn log(&self, record: &Record) {
        eprintln!(
            "[{:?}, {:?}] {}: {}",
            current_time(),
            std::process::id(),
            record.level(),
            record.args()
        );
    }

    fn flush(&self) {}
}

/// A simple logger struct that logs to a `RawFd` when used with [`log::set_logger`].
#[derive(Debug)]
#[cfg(all(feature = "std", unix))]
pub struct SimpleFdLogger {
    fd: RawFd,
}

#[cfg(all(feature = "std", unix))]
impl SimpleFdLogger {
    /// Create a new [`log::Log`] logger that will write the log to the given `fd`
    ///
    /// # Safety
    /// Needs a valid raw file descriptor opened for writing.
    #[must_use]
    pub const unsafe fn new(fd: RawFd) -> Self {
        Self { fd }
    }

    /// Sets the `fd` this logger will write to
    ///
    /// # Safety
    /// Needs a valid raw file descriptor opened for writing.
    pub unsafe fn set_fd(&mut self, fd: RawFd) {
        self.fd = fd;
    }

    /// Register this logger, logging to the given `fd`
    ///
    /// # Safety
    /// This function may not be called multiple times concurrently.
    /// The passed-in `fd` has to be a legal file descriptor to log to.
    pub unsafe fn set_logger(log_fd: RawFd) -> Result<(), Error> {
        // # Safety
        // The passed-in `fd` has to be a legal file descriptor to log to.
        // We also access a shared variable here.
        let logger = &raw mut LIBAFL_RAWFD_LOGGER;
        unsafe {
            let logger = &mut *logger;
            logger.set_fd(log_fd);
            log::set_logger(logger)?;
        }
        Ok(())
    }
}

#[cfg(all(feature = "std", unix))]
impl log::Log for SimpleFdLogger {
    #[inline]
    fn enabled(&self, _metadata: &Metadata) -> bool {
        true
    }

    fn log(&self, record: &Record) {
        let mut f = unsafe { File::from_raw_fd(self.fd) };
        writeln!(
            f,
            "[{:?}, {:#?}] {}: {}",
            current_time(),
            std::process::id(),
            record.level(),
            record.args()
        )
        .unwrap_or_else(|err| println!("Failed to log to fd {}: {err}", self.fd));
        mem::forget(f);
    }

    fn flush(&self) {}
}

/// Set up an error print hook that will
///
/// # Safety
/// Will fail if `new_stderr` is not a valid file descriptor.
/// May not be called multiple times concurrently.
#[cfg(all(unix, feature = "std"))]
pub unsafe fn set_error_print_panic_hook(new_stderr: RawFd) {
    // Make sure potential errors get printed to the correct (non-closed) stderr
    panic::set_hook(Box::new(move |panic_info| {
        let mut f = unsafe { File::from_raw_fd(new_stderr) };
        writeln!(f, "{panic_info}",)
            .unwrap_or_else(|err| println!("Failed to log to fd {new_stderr}: {err}"));
        mem::forget(f);
    }));
}

#[cfg(feature = "std")]
#[cfg(target_os = "windows")]
#[repr(C)]
#[allow(clippy::upper_case_acronyms)]
struct TEB {
    reserved1: [u8; 0x58],
    tls_pointer: *mut *mut u8,
    reserved2: [u8; 0xC0],
}

#[cfg(feature = "std")]
#[cfg(target_arch = "x86_64")]
#[inline(always)]
#[cfg(target_os = "windows")]
fn nt_current_teb() -> *mut TEB {
    use core::arch::asm;
    let teb: *mut TEB;
    unsafe {
        asm!("mov {}, gs:0x30", out(reg) teb);
    }
    teb
}

/// Some of our hooks can be invoked from threads that do not have TLS yet.
/// Many Rust and Frida functions require TLS to be set up, so we need to check if we have TLS.
/// This was observed on Windows, so for now for other platforms we assume that we have TLS.
#[cfg(feature = "std")]
#[inline]
#[allow(unreachable_code)]
#[must_use]
pub fn has_tls() -> bool {
    #[cfg(target_os = "windows")]
    unsafe {
        let teb = nt_current_teb();
        if teb.is_null() {
            return false;
        }

        let tls_array = (*teb).tls_pointer;
        if tls_array.is_null() {
            return false;
        }
        return true;
    }
    #[cfg(target_arch = "aarch64")]
    unsafe {
        let mut tid: u64;
        std::arch::asm!(
            "mrs {tid}, TPIDRRO_EL0",
            tid = out(reg) tid,
        );
        tid &= 0xffff_ffff_ffff_fff8;
        let tlsptr = tid as *const u64;
        return tlsptr.add(0x102).read() != 0u64;
    }
    // Default
    true
}

/// Zero-cost way to construct [`core::num::NonZeroUsize`] at compile-time.
#[macro_export]
macro_rules! nonzero {
    // TODO: Further simplify with `unwrap`/`expect` once MSRV includes
    // https://github.com/rust-lang/rust/issues/67441
    ($val:expr) => {
        const {
            match core::num::NonZero::new($val) {
                Some(x) => x,
                None => panic!("Value passed to `nonzero!` was zero"),
            }
        }
    };
}

/// Get a [`core::ptr::NonNull`] to a global static mut (or similar).
///
/// The same as [`core::ptr::addr_of_mut`] or `&raw mut`, but wrapped in said [`NonNull`](core::ptr::NonNull).
#[macro_export]
macro_rules! nonnull_raw_mut {
    ($val:expr) => {
        // # Safety
        // The pointer to a value will never be null (unless we're on an archaic OS in a CTF challenge).
        unsafe { core::ptr::NonNull::new(&raw mut $val).unwrap_unchecked() }
    };
}

#[cfg(feature = "python")]
#[allow(missing_docs)] // expect somehow breaks here
pub mod pybind {

    use pyo3::{Bound, PyResult, pymodule, types::PyModule};

    #[macro_export]
    macro_rules! unwrap_me_body {
        ($wrapper:expr, $name:ident, $body:block, $wrapper_type:ident, { $($wrapper_option:tt),* }) => {
            match &$wrapper {
                $(
                    $wrapper_type::$wrapper_option(py_wrapper) => {
                        Python::with_gil(|py| -> PyResult<_> {
                            let borrowed = py_wrapper.borrow(py);
                            let $name = &borrowed.inner;
                            Ok($body)
                        })
                        .unwrap()
                    }
                )*
            }
        };
        ($wrapper:expr, $name:ident, $body:block, $wrapper_type:ident, { $($wrapper_option:tt),* }, { $($wrapper_optional:tt($pw:ident) => $code_block:block)* }) => {
            match &$wrapper {
                $(
                    $wrapper_type::$wrapper_option(py_wrapper) => {
                        Python::with_gil(|py| -> PyResult<_> {
                            let borrowed = py_wrapper.borrow(py);
                            let $name = &borrowed.inner;
                            Ok($body)
                        })
                        .unwrap()
                    }
                )*
                $($wrapper_type::$wrapper_optional($pw) => { $code_block })*
            }
        };
    }

    #[macro_export]
    macro_rules! unwrap_me_mut_body {
        ($wrapper:expr, $name:ident, $body:block, $wrapper_type:ident, { $($wrapper_option:tt),*}) => {
            match &mut $wrapper {
                $(
                    $wrapper_type::$wrapper_option(py_wrapper) => {
                        Python::attach(|py| -> PyResult<_> {
                            let mut borrowed = py_wrapper.borrow_mut(py);
                            let $name = &mut borrowed.inner;
                            Ok($body)
                        })
                        .unwrap()
                    }
                )*
            }
        };
        ($wrapper:expr, $name:ident, $body:block, $wrapper_type:ident, { $($wrapper_option:tt),*}, { $($wrapper_optional:tt($pw:ident) => $code_block:block)* }) => {
            match &mut $wrapper {
                $(
                    $wrapper_type::$wrapper_option(py_wrapper) => {
                        Python::with_gil(|py| -> PyResult<_> {
                            let mut borrowed = py_wrapper.borrow_mut(py);
                            let $name = &mut borrowed.inner;
                            Ok($body)
                        })
                        .unwrap()
                    }
                )*
                $($wrapper_type::$wrapper_optional($pw) => { $code_block })*
            }
        };
    }

    #[macro_export]
    macro_rules! impl_serde_pyobjectwrapper {
        ($struct_name:ident, $inner:tt) => {
            const _: () = {
                use alloc::vec::Vec;

                use pyo3::prelude::*;
                use serde::{Deserialize, Deserializer, Serialize, Serializer};

                impl Serialize for $struct_name {
                    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
                    where
                        S: Serializer,
                    {
                        let buf = Python::with_gil(|py| -> PyResult<Vec<u8>> {
                            let pickle = PyModule::import(py, "pickle")?;
                            let buf: Vec<u8> =
                                pickle.getattr("dumps")?.call1((&self.$inner,))?.extract()?;
                            Ok(buf)
                        })
                        .unwrap();
                        serializer.serialize_bytes(&buf)
                    }
                }

                struct PyObjectVisitor;

                impl<'de> serde::de::Visitor<'de> for PyObjectVisitor {
                    type Value = $struct_name;

                    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
                        formatter
                            .write_str("Expecting some bytes to deserialize from the Python side")
                    }

                    fn visit_byte_buf<E>(self, v: Vec<u8>) -> Result<Self::Value, E>
                    where
                        E: serde::de::Error,
                    {
                        let obj = Python::with_gil(|py| -> PyResult<PyObject> {
                            let pickle = PyModule::import(py, "pickle")?;
                            let obj = pickle.getattr("loads")?.call1((v,))?.to_object(py);
                            Ok(obj)
                        })
                        .unwrap();
                        Ok($struct_name::new(obj))
                    }
                }

                impl<'de> Deserialize<'de> for $struct_name {
                    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
                    where
                        D: Deserializer<'de>,
                    {
                        deserializer.deserialize_byte_buf(PyObjectVisitor)
                    }
                }
            };
        };
    }

    #[pymodule]
    #[pyo3(name = "libafl_bolts")]
    /// Register the classes to the python module
    pub fn python_module(m: &Bound<'_, PyModule>) -> PyResult<()> {
        crate::rands::pybind::register(m)?;
        Ok(())
    }
}

/// Create a [`Vec`] of the given type with `nb_elts` elements, initialized in place.
/// The closure must initialize [`Vec`] (of size `nb_elts` * `sizeo_of::<T>()`).
///
/// # Safety
///
/// The input closure should fully initialize the new [`Vec`], not leaving any uninitialized bytes.
// TODO: Use MaybeUninit API at some point.
#[cfg(feature = "alloc")]
#[expect(clippy::uninit_vec)]
pub unsafe fn vec_init<E, F, T>(nb_elts: usize, init_fn: F) -> Result<Vec<T>, E>
where
    F: FnOnce(&mut Vec<T>) -> Result<(), E>,
{
    unsafe {
        let mut new_vec: Vec<T> = Vec::with_capacity(nb_elts);
        new_vec.set_len(nb_elts);

        init_fn(&mut new_vec)?;

        Ok(new_vec)
    }
}

#[cfg(test)]
mod tests {

    #[cfg(all(feature = "std", unix))]
    use crate::LIBAFL_RAWFD_LOGGER;

    #[test]
    #[cfg(all(unix, feature = "std"))]
    fn test_logger() {
        use std::{io::stdout, os::fd::AsRawFd};

        unsafe { LIBAFL_RAWFD_LOGGER.fd = stdout().as_raw_fd() };

        let libafl_rawfd_logger_fd = &raw const LIBAFL_RAWFD_LOGGER;
        unsafe {
            log::set_logger(&*libafl_rawfd_logger_fd).unwrap();
        }
        log::set_max_level(log::LevelFilter::Debug);
        log::info!("Test");
    }
}
