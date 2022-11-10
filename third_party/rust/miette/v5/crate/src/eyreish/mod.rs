#![cfg_attr(doc_cfg, feature(doc_cfg))]
#![allow(
    clippy::needless_doctest_main,
    clippy::new_ret_no_self,
    clippy::wrong_self_convention
)]
use core::fmt::Display;

use std::error::Error as StdError;

use once_cell::sync::OnceCell;

#[allow(unreachable_pub)]
pub use into_diagnostic::*;
#[doc(hidden)]
#[allow(unreachable_pub)]
pub use Report as ErrReport;
/// Compatibility re-export of `Report` for interop with `anyhow`
#[allow(unreachable_pub)]
pub use Report as Error;
#[doc(hidden)]
#[allow(unreachable_pub)]
pub use ReportHandler as EyreContext;
/// Compatibility re-export of `WrapErr` for interop with `anyhow`
#[allow(unreachable_pub)]
pub use WrapErr as Context;

#[cfg(not(feature = "fancy-no-backtrace"))]
use crate::DebugReportHandler;
use crate::Diagnostic;
#[cfg(feature = "fancy-no-backtrace")]
use crate::MietteHandler;

use error::ErrorImpl;

use self::ptr::Own;

mod context;
mod error;
mod fmt;
mod into_diagnostic;
mod kind;
mod macros;
mod ptr;
mod wrapper;

/**
Core Diagnostic wrapper type.

## `eyre` Users

You can just replace `use`s of `eyre::Report` with `miette::Report`.
*/
pub struct Report {
    inner: Own<ErrorImpl<()>>,
}

unsafe impl Sync for Report {}
unsafe impl Send for Report {}

///
pub type ErrorHook =
    Box<dyn Fn(&(dyn Diagnostic + 'static)) -> Box<dyn ReportHandler> + Sync + Send + 'static>;

static HOOK: OnceCell<ErrorHook> = OnceCell::new();

/// Error indicating that [`set_hook()`] was unable to install the provided
/// [`ErrorHook`].
#[derive(Debug)]
pub struct InstallError;

impl core::fmt::Display for InstallError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_str("cannot install provided ErrorHook, a hook has already been installed")
    }
}

impl StdError for InstallError {}
impl Diagnostic for InstallError {}

/**
Set the error hook.
*/
pub fn set_hook(hook: ErrorHook) -> Result<(), InstallError> {
    HOOK.set(hook).map_err(|_| InstallError)
}

#[cfg_attr(track_caller, track_caller)]
#[cfg_attr(not(track_caller), allow(unused_mut))]
fn capture_handler(error: &(dyn Diagnostic + 'static)) -> Box<dyn ReportHandler> {
    let hook = HOOK.get_or_init(|| Box::new(get_default_printer)).as_ref();

    #[cfg(track_caller)]
    {
        let mut handler = hook(error);
        handler.track_caller(std::panic::Location::caller());
        handler
    }
    #[cfg(not(track_caller))]
    {
        hook(error)
    }
}

fn get_default_printer(_err: &(dyn Diagnostic + 'static)) -> Box<dyn ReportHandler + 'static> {
    #[cfg(feature = "fancy-no-backtrace")]
    return Box::new(MietteHandler::new());
    #[cfg(not(feature = "fancy-no-backtrace"))]
    return Box::new(DebugReportHandler::new());
}

impl dyn ReportHandler {
    ///
    pub fn is<T: ReportHandler>(&self) -> bool {
        // Get `TypeId` of the type this function is instantiated with.
        let t = core::any::TypeId::of::<T>();

        // Get `TypeId` of the type in the trait object (`self`).
        let concrete = self.type_id();

        // Compare both `TypeId`s on equality.
        t == concrete
    }

    ///
    pub fn downcast_ref<T: ReportHandler>(&self) -> Option<&T> {
        if self.is::<T>() {
            unsafe { Some(&*(self as *const dyn ReportHandler as *const T)) }
        } else {
            None
        }
    }

    ///
    pub fn downcast_mut<T: ReportHandler>(&mut self) -> Option<&mut T> {
        if self.is::<T>() {
            unsafe { Some(&mut *(self as *mut dyn ReportHandler as *mut T)) }
        } else {
            None
        }
    }
}

/// Error Report Handler trait for customizing `miette::Report`
pub trait ReportHandler: core::any::Any + Send + Sync {
    /// Define the report format
    ///
    /// Used to override the report format of `miette::Report`
    ///
    /// # Example
    ///
    /// ```rust
    /// use indenter::indented;
    /// use miette::{Diagnostic, ReportHandler};
    ///
    /// pub struct Handler;
    ///
    /// impl ReportHandler for Handler {
    ///     fn debug(
    ///         &self,
    ///         error: &dyn Diagnostic,
    ///         f: &mut core::fmt::Formatter<'_>,
    ///     ) -> core::fmt::Result {
    ///         use core::fmt::Write as _;
    ///
    ///         if f.alternate() {
    ///             return core::fmt::Debug::fmt(error, f);
    ///         }
    ///
    ///         write!(f, "{}", error)?;
    ///
    ///         Ok(())
    ///     }
    /// }
    /// ```
    fn debug(
        &self,
        error: &(dyn Diagnostic),
        f: &mut core::fmt::Formatter<'_>,
    ) -> core::fmt::Result;

    /// Override for the `Display` format
    fn display(
        &self,
        error: &(dyn StdError + 'static),
        f: &mut core::fmt::Formatter<'_>,
    ) -> core::fmt::Result {
        write!(f, "{}", error)?;

        if f.alternate() {
            for cause in crate::chain::Chain::new(error).skip(1) {
                write!(f, ": {}", cause)?;
            }
        }

        Ok(())
    }

    /// Store the location of the caller who constructed this error report
    #[allow(unused_variables)]
    fn track_caller(&mut self, location: &'static std::panic::Location<'static>) {}
}

/// type alias for `Result<T, Report>`
///
/// This is a reasonable return type to use throughout your application but also
/// for `main()`. If you do, failures will be printed along with a backtrace if
/// one was captured.
///
/// `miette::Result` may be used with one *or* two type parameters.
///
/// ```rust
/// use miette::Result;
///
/// # const IGNORE: &str = stringify! {
/// fn demo1() -> Result<T> {...}
///            // ^ equivalent to std::result::Result<T, miette::Error>
///
/// fn demo2() -> Result<T, OtherError> {...}
///            // ^ equivalent to std::result::Result<T, OtherError>
/// # };
/// ```
///
/// # Example
///
/// ```
/// # pub trait Deserialize {}
/// #
/// # mod serde_json {
/// #     use super::Deserialize;
/// #     use std::io;
/// #
/// #     pub fn from_str<T: Deserialize>(json: &str) -> io::Result<T> {
/// #         unimplemented!()
/// #     }
/// # }
/// #
/// # #[derive(Debug)]
/// # struct ClusterMap;
/// #
/// # impl Deserialize for ClusterMap {}
/// #
/// use miette::{IntoDiagnostic, Result};
///
/// fn main() -> Result<()> {
///     # return Ok(());
///     let config = std::fs::read_to_string("cluster.json").into_diagnostic()?;
///     let map: ClusterMap = serde_json::from_str(&config).into_diagnostic()?;
///     println!("cluster info: {:#?}", map);
///     Ok(())
/// }
/// ```
///
/// ## `anyhow`/`eyre` Users
///
/// You can just replace `use`s of `anyhow::Result`/`eyre::Result` with
/// `miette::Result`.
pub type Result<T, E = Report> = core::result::Result<T, E>;

/// Provides the [`wrap_err()`](WrapErr::wrap_err) method for [`Result`].
///
/// This trait is sealed and cannot be implemented for types outside of
/// `miette`.
///
/// # Example
///
/// ```
/// use miette::{WrapErr, IntoDiagnostic, Result};
/// use std::{fs, path::PathBuf};
///
/// pub struct ImportantThing {
///     path: PathBuf,
/// }
///
/// impl ImportantThing {
///     # const IGNORE: &'static str = stringify! {
///     pub fn detach(&mut self) -> Result<()> {...}
///     # };
///     # fn detach(&mut self) -> Result<()> {
///     #     unimplemented!()
///     # }
/// }
///
/// pub fn do_it(mut it: ImportantThing) -> Result<Vec<u8>> {
///     it.detach().wrap_err("Failed to detach the important thing")?;
///
///     let path = &it.path;
///     let content = fs::read(path)
///         .into_diagnostic()
///         .wrap_err_with(|| format!(
///             "Failed to read instrs from {}",
///             path.display())
///         )?;
///
///     Ok(content)
/// }
/// ```
///
/// When printed, the outermost error would be printed first and the lower
/// level underlying causes would be enumerated below.
///
/// ```console
/// Error: Failed to read instrs from ./path/to/instrs.json
///
/// Caused by:
///     No such file or directory (os error 2)
/// ```
///
/// # Wrapping Types That Do Not Implement `Error`
///
/// For example `&str` and `Box<dyn Error>`.
///
/// Due to restrictions for coherence `Report` cannot implement `From` for types
/// that don't implement `Error`. Attempts to do so will give `"this type might
/// implement Error in the future"` as an error. As such, `wrap_err()`, which
/// uses `From` under the hood, cannot be used to wrap these types. Instead we
/// encourage you to use the combinators provided for `Result` in `std`/`core`.
///
/// For example, instead of this:
///
/// ```rust,compile_fail
/// use std::error::Error;
/// use miette::{WrapErr, Report};
///
/// fn wrap_example(err: Result<(), Box<dyn Error + Send + Sync + 'static>>)
///     -> Result<(), Report>
/// {
///     err.wrap_err("saw a downstream error")
/// }
/// ```
///
/// We encourage you to write this:
///
/// ```rust
/// use miette::{miette, Report, WrapErr};
/// use std::error::Error;
///
/// fn wrap_example(err: Result<(), Box<dyn Error + Send + Sync + 'static>>) -> Result<(), Report> {
///     err.map_err(|e| miette!(e))
///         .wrap_err("saw a downstream error")
/// }
/// ```
///
/// # Effect on Downcasting
///
/// After attaching a message of type `D` onto an error of type `E`, the
/// resulting `miette::Error` may be downcast to `D` **or** to `E`.
///
/// That is, in codebases that rely on downcasting, `miette`'s `wrap_err()`
/// supports both of the following use cases:
///
///   - **Attaching messages whose type is insignificant onto errors whose type
///     is used in downcasts.**
///
///     In other error libraries whose `wrap_err()` is not designed this way, it
///     can be risky to introduce messages to existing code because new message
///     might break existing working downcasts. In miette, any downcast that
///     worked before adding the message will continue to work after you add a
///     message, so you should freely wrap errors wherever it would be helpful.
///
///     ```
///     # use miette::bail;
///     # use thiserror::Error;
///     #
///     # #[derive(Error, Debug)]
///     # #[error("???")]
///     # struct SuspiciousError;
///     #
///     # fn helper() -> Result<()> {
///     #     bail!(SuspiciousError);
///     # }
///     #
///     use miette::{WrapErr, Result};
///
///     fn do_it() -> Result<()> {
///         helper().wrap_err("Failed to complete the work")?;
///         # const IGNORE: &str = stringify! {
///         ...
///         # };
///         # unreachable!()
///     }
///
///     fn main() {
///         let err = do_it().unwrap_err();
///         if let Some(e) = err.downcast_ref::<SuspiciousError>() {
///             // If helper() returned SuspiciousError, this downcast will
///             // correctly succeed even with the message in between.
///             # return;
///         }
///         # panic!("expected downcast to succeed");
///     }
///     ```
///
///   - **Attaching message whose type is used in downcasts onto errors whose
///     type is insignificant.**
///
///     Some codebases prefer to use machine-readable messages to categorize
///     lower level errors in a way that will be actionable to higher levels of
///     the application.
///
///     ```
///     # use miette::bail;
///     # use thiserror::Error;
///     #
///     # #[derive(Error, Debug)]
///     # #[error("???")]
///     # struct HelperFailed;
///     #
///     # fn helper() -> Result<()> {
///     #     bail!("no such file or directory");
///     # }
///     #
///     use miette::{WrapErr, Result};
///
///     fn do_it() -> Result<()> {
///         helper().wrap_err(HelperFailed)?;
///         # const IGNORE: &str = stringify! {
///         ...
///         # };
///         # unreachable!()
///     }
///
///     fn main() {
///         let err = do_it().unwrap_err();
///         if let Some(e) = err.downcast_ref::<HelperFailed>() {
///             // If helper failed, this downcast will succeed because
///             // HelperFailed is the message that has been attached to
///             // that error.
///             # return;
///         }
///         # panic!("expected downcast to succeed");
///     }
///     ```
pub trait WrapErr<T, E>: context::private::Sealed {
    /// Wrap the error value with a new adhoc error
    #[cfg_attr(track_caller, track_caller)]
    fn wrap_err<D>(self, msg: D) -> Result<T, Report>
    where
        D: Display + Send + Sync + 'static;

    /// Wrap the error value with a new adhoc error that is evaluated lazily
    /// only once an error does occur.
    #[cfg_attr(track_caller, track_caller)]
    fn wrap_err_with<D, F>(self, f: F) -> Result<T, Report>
    where
        D: Display + Send + Sync + 'static,
        F: FnOnce() -> D;

    /// Compatibility re-export of `wrap_err()` for interop with `anyhow`
    #[cfg_attr(track_caller, track_caller)]
    fn context<D>(self, msg: D) -> Result<T, Report>
    where
        D: Display + Send + Sync + 'static;

    /// Compatibility re-export of `wrap_err_with()` for interop with `anyhow`
    #[cfg_attr(track_caller, track_caller)]
    fn with_context<D, F>(self, f: F) -> Result<T, Report>
    where
        D: Display + Send + Sync + 'static,
        F: FnOnce() -> D;
}

// Private API. Referenced by macro-generated code.
#[doc(hidden)]
pub mod private {
    use super::Report;
    use core::fmt::{Debug, Display};

    pub use core::result::Result::Err;

    #[doc(hidden)]
    pub mod kind {
        pub use super::super::kind::{AdhocKind, TraitKind};

        pub use super::super::kind::BoxedKind;
    }

    #[cfg_attr(track_caller, track_caller)]
    pub fn new_adhoc<M>(message: M) -> Report
    where
        M: Display + Debug + Send + Sync + 'static,
    {
        Report::from_adhoc(message)
    }
}
