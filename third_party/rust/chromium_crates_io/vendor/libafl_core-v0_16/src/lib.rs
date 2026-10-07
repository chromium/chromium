/*!
 * `LibAFL_core` contains core traits used across all crates, including the [`Error`] enum and various traits.
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
#[macro_export]
#[cfg(not(feature = "alloc"))]
macro_rules! format {
    ($fmt:literal) => {{ $fmt }};
    ($fmt:literal, $($arg:tt)*) => {{
        // make sure the variables don't trigger unused-variable warnings
        let _ = ($($arg)*);
        $fmt
    }};
}
/// Re-export of the "format" macro
#[cfg(feature = "alloc")]
pub use alloc::{borrow::Cow, format};

#[cfg(feature = "std")]
#[macro_use]
extern crate std;
#[cfg(feature = "alloc")]
#[doc(hidden)]
pub extern crate alloc;

#[cfg(feature = "alloc")]
use alloc::vec::Vec;
use core::{
    array::TryFromSliceError,
    fmt::{self, Display},
    num::{ParseIntError, TryFromIntError},
    ops::{Deref, DerefMut},
};
#[cfg(feature = "std")]
use std::{env::VarError, io};

#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};
#[cfg(feature = "alloc")]
use {
    alloc::string::{FromUtf8Error, String},
    core::cell::{BorrowError, BorrowMutError},
    core::str::Utf8Error,
};

/// Localhost addr, this is used, for example, for LLMP Client, which connects to this address
pub const IP_LOCALHOST: &str = "127.0.0.1";

/// The client ID for various use cases across `LibAFL`
#[repr(transparent)]
#[derive(Debug, Default, Copy, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct ClientId(pub u32);

#[cfg(feature = "errors_backtrace")]
/// Error Backtrace type when `errors_backtrace` feature is enabled (== [`Backtrace`](std::backtrace::Backtrace`))
pub type ErrorBacktrace = std::backtrace::Backtrace;

#[cfg(not(feature = "errors_backtrace"))]
#[derive(Debug, Default)]
/// ZST to use when `errors_backtrace` is disabled
pub struct ErrorBacktrace;

#[cfg(not(feature = "errors_backtrace"))]
impl ErrorBacktrace {
    /// Nop
    #[must_use]
    pub fn capture() -> Self {
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

/// Main error struct for `LibAFL`
#[derive(Debug)]
pub enum Error {
    /// Serialization error
    Serialize(String, ErrorBacktrace),
    /// Compression error
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
    /// Raise this from a stage to skip the remaining stages for a given input, not really an error.
    SkipRemainingStages,
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
        Error::Serialize(arg.into(), ErrorBacktrace::capture())
    }

    /// Compression error
    #[must_use]
    pub fn compression() -> Self {
        Error::Compression(ErrorBacktrace::capture())
    }

    /// Optional val was supposed to be set, but isn't.
    #[must_use]
    pub fn empty_optional<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::EmptyOptional(arg.into(), ErrorBacktrace::capture())
    }

    /// The `Input` was invalid
    #[must_use]
    pub fn invalid_input<S>(reason: S) -> Self
    where
        S: Into<String>,
    {
        Error::InvalidInput(reason.into(), ErrorBacktrace::capture())
    }

    /// Key not in Map
    #[must_use]
    pub fn key_not_found<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::KeyNotFound(arg.into(), ErrorBacktrace::capture())
    }

    /// Key already exists in Map
    #[must_use]
    pub fn key_exists<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::KeyExists(arg.into(), ErrorBacktrace::capture())
    }

    /// No elements in the current item
    #[must_use]
    pub fn empty<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::Empty(arg.into(), ErrorBacktrace::capture())
    }

    /// End of iteration
    #[must_use]
    pub fn iterator_end<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::IteratorEnd(arg.into(), ErrorBacktrace::capture())
    }

    /// This is not supported (yet)
    #[must_use]
    pub fn not_implemented<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::NotImplemented(arg.into(), ErrorBacktrace::capture())
    }

    /// You're holding it wrong
    #[must_use]
    pub fn illegal_state<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::IllegalState(arg.into(), ErrorBacktrace::capture())
    }

    /// The argument passed to this method or function is not valid
    #[must_use]
    pub fn illegal_argument<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::IllegalArgument(arg.into(), ErrorBacktrace::capture())
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
        Error::Unsupported(arg.into(), ErrorBacktrace::capture())
    }

    /// OS error with additional message
    #[cfg(feature = "std")]
    #[must_use]
    pub fn os_error<S>(err: io::Error, msg: S) -> Self
    where
        S: Into<String>,
    {
        Error::OsError(err, msg.into(), ErrorBacktrace::capture())
    }

    /// OS error from `io::Error::last_os_error` with additional message
    #[cfg(feature = "std")]
    #[must_use]
    pub fn last_os_error<S>(msg: S) -> Self
    where
        S: Into<String>,
    {
        Error::OsError(
            io::Error::last_os_error(),
            msg.into(),
            ErrorBacktrace::capture(),
        )
    }

    /// Something else happened
    #[must_use]
    pub fn unknown<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::Unknown(arg.into(), ErrorBacktrace::capture())
    }

    /// Error with corpora
    #[must_use]
    pub fn invalid_corpus<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::InvalidCorpus(arg.into(), ErrorBacktrace::capture())
    }

    /// Error specific to some runtime, like QEMU or Frida
    #[must_use]
    pub fn runtime<S>(arg: S) -> Self
    where
        S: Into<String>,
    {
        Error::Runtime(arg.into(), ErrorBacktrace::capture())
    }

    /// Skip the remaining stages for this input
    #[must_use]
    pub fn skip_remaining_stages() -> Self {
        Error::SkipRemainingStages
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
                write!(f, "Error in Serialization: `{s}`")?;
                display_error_backtrace(f, b)
            }
            Self::Compression(b) => {
                write!(f, "Error in decompression")?;
                display_error_backtrace(f, b)
            }
            Self::EmptyOptional(s, b) => {
                write!(f, "Optional value `{s}` was not set")?;
                display_error_backtrace(f, b)
            }
            Self::KeyNotFound(s, b) => {
                write!(f, "Key: `{s}` - not found")?;
                display_error_backtrace(f, b)
            }
            Self::KeyExists(s, b) => {
                write!(f, "Key: `{s}` - already exists")?;
                display_error_backtrace(f, b)
            }
            Self::Empty(s, b) => {
                write!(f, "No items in {s}")?;
                display_error_backtrace(f, b)
            }
            Self::IteratorEnd(s, b) => {
                write!(f, "All elements have been processed in {s} iterator")?;
                display_error_backtrace(f, b)
            }
            Self::NotImplemented(s, b) => {
                write!(f, "Not implemented: {s}")?;
                display_error_backtrace(f, b)
            }
            Self::IllegalState(s, b) => {
                write!(f, "Illegal state: {s}")?;
                display_error_backtrace(f, b)
            }
            Self::IllegalArgument(s, b) => {
                write!(f, "Illegal argument: {s}")?;
                display_error_backtrace(f, b)
            }
            Self::Unsupported(s, b) => {
                write!(
                    f,
                    "The operation is not supported on the current platform: {s}"
                )?;
                display_error_backtrace(f, b)
            }
            Self::ShuttingDown => write!(f, "Shutting down!"),
            #[cfg(feature = "std")]
            Self::OsError(err, s, b) => {
                write!(f, "OS error: {s}: {err}")?;
                display_error_backtrace(f, b)
            }
            Self::Unknown(s, b) => {
                write!(f, "Unknown error: {s}")?;
                display_error_backtrace(f, b)
            }
            Self::InvalidCorpus(s, b) => {
                write!(f, "Invalid corpus: {s}")?;
                display_error_backtrace(f, b)
            }
            Self::Runtime(s, b) => {
                write!(f, "Runtime error: {s}")?;
                display_error_backtrace(f, b)
            }
            Self::InvalidInput(s, b) => {
                write!(f, "Encountered an invalid input: {s}")?;
                display_error_backtrace(f, b)
            }
            Self::SkipRemainingStages => write!(f, "Skip remaining stages"),
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
#[cfg(all(feature = "alloc", feature = "postcard"))]
impl From<postcard::Error> for Error {
    fn from(err: postcard::Error) -> Self {
        Self::serialize(format!("{err:?}"))
    }
}

#[cfg(all(unix, feature = "std", feature = "nix"))]
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
pub trait ToSlice<'a> {
    /// Type of the entries of this slice
    type Entry: 'a;
    /// Type of the reference to this slice
    type SliceRef: Deref<Target = [Self::Entry]>;

    /// Convert to a slice
    fn to_slice(&'a self) -> Self::SliceRef;
}

/// Can be converted to a slice
pub trait ToSizedSlice<'a, const N: usize> {
    /// Type of the entries of this slice
    type Entry: 'a;
    /// Type of the reference to this slice
    type SliceRef: Deref<Target = [Self::Entry; N]>;

    /// Convert to a slice
    fn to_sized_slice(&'a self) -> Self::SliceRef;
}

impl<'a, T, R: ?Sized> ToSlice<'a> for R
where
    T: 'a,
    R: Deref<Target = [T]>,
{
    type Entry = T;
    type SliceRef = &'a [T];

    fn to_slice(&'a self) -> Self::SliceRef {
        self
    }
}

impl<'a, T, const N: usize, R: ?Sized> ToSizedSlice<'a, N> for R
where
    T: 'a,
    R: Deref<Target = [T; N]>,
{
    type Entry = T;
    type SliceRef = &'a [T; N];

    fn to_sized_slice(&'a self) -> Self::SliceRef {
        self
    }
}

/// Can be converted to a mutable slice
pub trait ToSliceMut<'a>: ToSlice<'a> {
    /// Type of the mutable reference to this slice
    type SliceRefMut: DerefMut<Target = [Self::Entry]>;

    /// Convert to a slice
    fn to_slice_mut(&'a mut self) -> Self::SliceRefMut;
}

/// Can be converted to a mutable slice
pub trait ToSizedSliceMut<'a, const N: usize>: ToSizedSlice<'a, N> {
    /// Type of the mutable reference to this slice
    type SliceRefMut: DerefMut<Target = [Self::Entry; N]>;

    /// Convert to a slice
    fn to_sized_slice_mut(&'a mut self) -> Self::SliceRefMut;
}

impl<'a, T, R: ?Sized> ToSliceMut<'a> for R
where
    T: 'a,
    R: DerefMut<Target = [T]>,
{
    type SliceRefMut = &'a mut [T];

    fn to_slice_mut(&'a mut self) -> Self::SliceRefMut {
        &mut *self
    }
}

impl<'a, T, const N: usize, R: ?Sized> ToSizedSliceMut<'a, N> for R
where
    T: 'a,
    R: DerefMut<Target = [T; N]>,
{
    type SliceRefMut = &'a mut [T; N];

    fn to_sized_slice_mut(&'a mut self) -> Self::SliceRefMut {
        &mut *self
    }
}

/// Can be converted to a slice
#[deprecated(note = "Use ToSlice instead")]
#[allow(deprecated)]
pub trait AsSlice<'a>: ToSlice<'a> {
    /// Type of the entries of this slice
    type Entry: 'a;
    /// Type of the reference to this slice
    type SliceRef: Deref<Target = [<Self as ToSlice<'a>>::Entry]>;

    /// Convert to a slice
    fn as_slice(&'a self) -> <Self as ToSlice<'a>>::SliceRef;
}

#[allow(deprecated)]
impl<'a, T: ?Sized + ToSlice<'a>> AsSlice<'a> for T {
    type Entry = T::Entry;
    type SliceRef = T::SliceRef;

    fn as_slice(&'a self) -> <Self as ToSlice<'a>>::SliceRef {
        self.to_slice()
    }
}

/// Can be converted to a slice
#[deprecated(note = "Use ToSizedSlice instead")]
#[allow(deprecated)]
pub trait AsSizedSlice<'a, const N: usize>: ToSizedSlice<'a, N> {
    /// Type of the entries of this slice
    type Entry: 'a;
    /// Type of the reference to this slice
    type SliceRef: Deref<Target = [<Self as ToSizedSlice<'a, N>>::Entry; N]>;

    /// Convert to a slice
    fn as_sized_slice(&'a self) -> <Self as ToSizedSlice<'a, N>>::SliceRef;
}

#[allow(deprecated)]
impl<'a, const N: usize, T: ?Sized + ToSizedSlice<'a, N>> AsSizedSlice<'a, N> for T {
    type Entry = T::Entry;
    type SliceRef = T::SliceRef;

    fn as_sized_slice(&'a self) -> <Self as ToSizedSlice<'a, N>>::SliceRef {
        self.to_sized_slice()
    }
}

/// Can be converted to a mutable slice
#[deprecated(note = "Use ToSliceMut instead")]
#[allow(deprecated)]
pub trait AsSliceMut<'a>: AsSlice<'a> + ToSliceMut<'a> {
    /// Type of the mutable reference to this slice
    type SliceRefMut: DerefMut<Target = [<Self as ToSlice<'a>>::Entry]>;

    /// Convert to a slice
    fn as_slice_mut(&'a mut self) -> <Self as ToSliceMut<'a>>::SliceRefMut;
}

#[allow(deprecated)]
impl<'a, T: ?Sized + ToSliceMut<'a>> AsSliceMut<'a> for T {
    type SliceRefMut = T::SliceRefMut;

    fn as_slice_mut(&'a mut self) -> <Self as ToSliceMut<'a>>::SliceRefMut {
        self.to_slice_mut()
    }
}

/// Can be converted to a mutable slice
#[deprecated(note = "Use ToSizedSliceMut instead")]
#[allow(deprecated)]
pub trait AsSizedSliceMut<'a, const N: usize>:
    AsSizedSlice<'a, N> + ToSizedSliceMut<'a, N>
{
    /// Type of the mutable reference to this slice
    type SliceRefMut: DerefMut<Target = [<Self as ToSizedSlice<'a, N>>::Entry; N]>;

    /// Convert to a slice
    fn as_sized_slice_mut(&'a mut self) -> <Self as ToSizedSliceMut<'a, N>>::SliceRefMut;
}

#[allow(deprecated)]
impl<'a, const N: usize, T: ?Sized + ToSizedSliceMut<'a, N>> AsSizedSliceMut<'a, N> for T {
    type SliceRefMut = T::SliceRefMut;

    fn as_sized_slice_mut(&'a mut self) -> <Self as ToSizedSliceMut<'a, N>>::SliceRefMut {
        self.to_sized_slice_mut()
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
    S: ToSlice<'it, Entry = T, SliceRef = &'it [T]>,
    T: 'it,
{
    type Item = S::Entry;
    type Ref = &'it Self::Item;
    type IntoIter = core::slice::Iter<'it, Self::Item>;

    fn as_iter(&'it self) -> Self::IntoIter {
        self.to_slice().iter()
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
    S: ToSliceMut<'it, Entry = T, SliceRef = &'it [T], SliceRefMut = &'it mut [T]>,
    T: 'it,
{
    type RefMut = &'it mut Self::Item;
    type IntoIterMut = core::slice::IterMut<'it, Self::Item>;

    fn as_iter_mut(&'it mut self) -> Self::IntoIterMut {
        self.to_slice_mut().iter_mut()
    }
}

/// Has a ref count
pub trait HasRefCnt {
    /// The ref count
    fn refcnt(&self) -> isize;
    /// The ref count, mutable
    fn refcnt_mut(&mut self) -> &mut isize;
}

pub use nonzero_macros;
#[doc(inline)]
pub use nonzero_macros::{nonnull_raw_mut, nonzero, try_nonzero};

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

/// We need fixed names for many parts of this lib.
#[cfg(feature = "alloc")]
pub trait Named {
    /// Provide the name of this element.
    fn name(&self) -> &Cow<'static, str>;
}

#[cfg(feature = "alloc")]
impl Named for () {
    #[inline]
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("()");
        &NAME
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

impl<Head, Tail> HasLen for (Head, Tail)
where
    Tail: HasLen,
{
    #[inline]
    fn len(&self) -> usize {
        self.1.len() + 1
    }
}

impl<Tail> HasLen for (Tail,)
where
    Tail: HasLen,
{
    #[inline]
    fn len(&self) -> usize {
        self.0.len()
    }
}

impl HasLen for () {
    #[inline]
    fn len(&self) -> usize {
        0
    }
}

/// Trait to truncate slices and maps to a new size
pub trait Truncate {
    /// Reduce the size of the slice
    fn truncate(&mut self, len: usize);
}

impl<T> Truncate for &[T] {
    fn truncate(&mut self, len: usize) {
        *self = &self[..len];
    }
}

impl<T> Truncate for &mut [T] {
    fn truncate(&mut self, len: usize) {
        let value = core::mem::take(self);
        let len = value.len().min(len);
        let truncated = value
            .get_mut(..len)
            .expect("Truncate with len <= len() should always work");
        let _: &mut [T] = core::mem::replace(self, truncated);
    }
}

#[cfg(test)]
mod tests {
    extern crate alloc;
    use alloc::string::ToString;

    use super::*;

    #[test]
    fn test_error_display() {
        let err = Error::serialize("test_error");
        assert!(
            err.to_string()
                .starts_with("Error in Serialization: `test_error`")
        );

        let err = Error::empty_optional("test_opt");
        assert!(
            err.to_string()
                .starts_with("Optional value `test_opt` was not set")
        );

        let err = Error::key_not_found("test_key");
        assert!(err.to_string().starts_with("Key: `test_key` - not found"));

        let err = Error::illegal_state("bad_state");
        assert!(err.to_string().starts_with("Illegal state: bad_state"));

        let err = Error::illegal_argument("bad_arg");
        assert!(err.to_string().starts_with("Illegal argument: bad_arg"));
    }

    #[test]
    #[allow(deprecated)]
    fn test_deprecated_as_slice_fallbacks() {
        use alloc::vec;
        let mut data = vec![1u8, 2, 3, 4];
        let slice = data.as_slice();
        assert_eq!(slice, &[1, 2, 3, 4]);

        let mut_slice = data.as_slice_mut();
        mut_slice[0] = 10;
        assert_eq!(data[0], 10);
    }
}
