/// Return early with an error.
///
/// This macro is equivalent to `return Err(From::from($err))`.
///
/// # Example
///
/// ```
/// # use miette::{bail, Result};
/// #
/// # fn has_permission(user: usize, resource: usize) -> bool {
/// #     true
/// # }
/// #
/// # fn main() -> Result<()> {
/// #     let user = 0;
/// #     let resource = 0;
/// #
/// if !has_permission(user, resource) {
///     bail!("permission denied for accessing {}", resource);
/// }
/// #     Ok(())
/// # }
/// ```
///
/// ```
/// # use miette::{bail, Result};
/// # use thiserror::Error;
/// #
/// # const MAX_DEPTH: usize = 1;
/// #
/// #[derive(Error, Debug)]
/// enum ScienceError {
///     #[error("recursion limit exceeded")]
///     RecursionLimitExceeded,
///     # #[error("...")]
///     # More = (stringify! {
///     ...
///     # }, 1).1,
/// }
///
/// # fn main() -> Result<()> {
/// #     let depth = 0;
/// #     let err: &'static dyn std::error::Error = &ScienceError::RecursionLimitExceeded;
/// #
/// if depth > MAX_DEPTH {
///     bail!(ScienceError::RecursionLimitExceeded);
/// }
/// #     Ok(())
/// # }
/// ```
#[macro_export]
macro_rules! bail {
    ($msg:literal $(,)?) => {
        return $crate::private::Err($crate::miette!($msg));
    };
    ($err:expr $(,)?) => {
        return $crate::private::Err($crate::miette!($err));
    };
    ($fmt:expr, $($arg:tt)*) => {
        return $crate::private::Err($crate::miette!($fmt, $($arg)*));
    };
}

/// Return early with an error if a condition is not satisfied.
///
/// This macro is equivalent to `if !$cond { return Err(From::from($err)); }`.
///
/// Analogously to `assert!`, `ensure!` takes a condition and exits the function
/// if the condition fails. Unlike `assert!`, `ensure!` returns an `Error`
/// rather than panicking.
///
/// # Example
///
/// ```
/// # use miette::{ensure, Result};
/// #
/// # fn main() -> Result<()> {
/// #     let user = 0;
/// #
/// ensure!(user == 0, "only user 0 is allowed");
/// #     Ok(())
/// # }
/// ```
///
/// ```
/// # use miette::{ensure, Result};
/// # use thiserror::Error;
/// #
/// # const MAX_DEPTH: usize = 1;
/// #
/// #[derive(Error, Debug)]
/// enum ScienceError {
///     #[error("recursion limit exceeded")]
///     RecursionLimitExceeded,
///     # #[error("...")]
///     # More = (stringify! {
///     ...
///     # }, 1).1,
/// }
///
/// # fn main() -> Result<()> {
/// #     let depth = 0;
/// #
/// ensure!(depth <= MAX_DEPTH, ScienceError::RecursionLimitExceeded);
/// #     Ok(())
/// # }
/// ```
#[macro_export]
macro_rules! ensure {
    ($cond:expr, $msg:literal $(,)?) => {
        if !$cond {
            return $crate::private::Err($crate::miette!($msg));
        }
    };
    ($cond:expr, $err:expr $(,)?) => {
        if !$cond {
            return $crate::private::Err($crate::miette!($err));
        }
    };
    ($cond:expr, $fmt:expr, $($arg:tt)*) => {
        if !$cond {
            return $crate::private::Err($crate::miette!($fmt, $($arg)*));
        }
    };
}

/// Construct an ad-hoc error from a string.
///
/// This evaluates to an `Error`. It can take either just a string, or a format
/// string with arguments. It also can take any custom type which implements
/// `Debug` and `Display`.
///
/// # Example
///
/// ```
/// # type V = ();
/// #
/// use miette::{miette, Result};
///
/// fn lookup(key: &str) -> Result<V> {
///     if key.len() != 16 {
///         return Err(miette!("key length must be 16 characters, got {:?}", key));
///     }
///
///     // ...
///     # Ok(())
/// }
/// ```
///
/// ## `anyhow`/`eyre` Users
///
/// You can just replace `use`s of the `anyhow!`/`eyre!` macros with `miette!`.
#[macro_export]
macro_rules! miette {
    ($msg:literal $(,)?) => {
        // Handle $:literal as a special case to make cargo-expanded code more
        // concise in the common case.
        $crate::private::new_adhoc($msg)
    };
    ($err:expr $(,)?) => ({
        use $crate::private::kind::*;
        let error = $err;
        (&error).miette_kind().new(error)
    });
    ($fmt:expr, $($arg:tt)*) => {
        $crate::private::new_adhoc(format!($fmt, $($arg)*))
    };
}
