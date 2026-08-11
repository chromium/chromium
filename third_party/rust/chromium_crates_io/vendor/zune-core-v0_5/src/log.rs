/*
 * Copyright (c) 2023.
 *
 * This software is free software;
 *
 * You can redistribute it or modify it under terms of the MIT, Apache License or Zlib license
 */

// Re-export macros under nicer names
pub use crate::{
    __debug as debug, __error as error, __info as info, __log_enabled as log_enabled,
    __trace as trace, __warn as warn
};

#[repr(usize)]
#[derive(Copy, Clone, Eq, PartialEq, Debug, Hash)]
pub enum Level {
    Error = 1,
    Warn,
    Info,
    Debug,
    Trace
}

//
// log_enabled (unchanged)
//
#[doc(hidden)]
#[macro_export]
macro_rules! __log_enabled {
    ($lvl:expr) => {{
        let _ = $lvl;
        false
    }};
}

//
// ERROR
//
#[cfg(feature = "std")]
#[doc(hidden)]
#[macro_export]
macro_rules! __error {
    ($($arg:tt)+) => {{
        // Expand to a block so the macro is usable in expression position.
        // `{}` alone expands to *nothing*, which is a hard error for callers
        // that use the macro as a trailing expression.
        let _ = ::core::format_args!($($arg)+);
    }};
}

#[cfg(not(feature = "std"))]
#[doc(hidden)]
#[macro_export]
macro_rules! __error {
    ($($arg:tt)+) => {{
        // Expand to a block so the macro is usable in expression position.
        // `{}` alone expands to *nothing*, which is a hard error for callers
        // that use the macro as a trailing expression.
        let _ = ::core::format_args!($($arg)+);
    }};
}

//
// WARN
//
#[cfg(feature = "std")]
#[doc(hidden)]
#[macro_export]
macro_rules! __warn {
    ($($arg:tt)+) => {{
        // Expand to a block so the macro is usable in expression position.
        // `{}` alone expands to *nothing*, which is a hard error for callers
        // that use the macro as a trailing expression.
        let _ = ::core::format_args!($($arg)+);
    }};
}

#[cfg(not(feature = "std"))]
#[doc(hidden)]
#[macro_export]
macro_rules! __warn {
    ($($arg:tt)+) => {{
        // Expand to a block so the macro is usable in expression position.
        // `{}` alone expands to *nothing*, which is a hard error for callers
        // that use the macro as a trailing expression.
        let _ = ::core::format_args!($($arg)+);
    }};
}

//
// INFO
//
#[cfg(feature = "std")]
#[doc(hidden)]
#[macro_export]
macro_rules! __info {
    ($($arg:tt)+) => {{
        // Expand to a block so the macro is usable in expression position.
        // `{}` alone expands to *nothing*, which is a hard error for callers
        // that use the macro as a trailing expression.
        let _ = ::core::format_args!($($arg)+);
    }};
}

#[cfg(not(feature = "std"))]
#[doc(hidden)]
#[macro_export]
macro_rules! __info {
    ($($arg:tt)+) => {{
        // Expand to a block so the macro is usable in expression position.
        // `{}` alone expands to *nothing*, which is a hard error for callers
        // that use the macro as a trailing expression.
        let _ = ::core::format_args!($($arg)+);
    }};
}

//
// DEBUG
//
#[cfg(feature = "std")]
#[doc(hidden)]
#[macro_export]
macro_rules! __debug {
    ($($arg:tt)+) => {{
        // Expand to a block so the macro is usable in expression position.
        // `{}` alone expands to *nothing*, which is a hard error for callers
        // that use the macro as a trailing expression.
        let _ = ::core::format_args!($($arg)+);
    }};
}

#[cfg(not(feature = "std"))]
#[doc(hidden)]
#[macro_export]
macro_rules! __debug {
    ($($arg:tt)+) => {{
        // Expand to a block so the macro is usable in expression position.
        // `{}` alone expands to *nothing*, which is a hard error for callers
        // that use the macro as a trailing expression.
        let _ = ::core::format_args!($($arg)+);
    }};
}

//
// TRACE
//
#[cfg(feature = "std")]
#[doc(hidden)]
#[macro_export]
macro_rules! __trace {
    ($($arg:tt)+) => {{
        // Expand to a block so the macro is usable in expression position.
        // `{}` alone expands to *nothing*, which is a hard error for callers
        // that use the macro as a trailing expression.
        let _ = ::core::format_args!($($arg)+);
    }};
}

#[cfg(not(feature = "std"))]
#[doc(hidden)]
#[macro_export]
macro_rules! __trace {
    ($($arg:tt)+) => {{
        // Expand to a block so the macro is usable in expression position.
        // `{}` alone expands to *nothing*, which is a hard error for callers
        // that use the macro as a trailing expression.
        let _ = ::core::format_args!($($arg)+);
    }};
}

#[cfg(test)]
mod tests {
    use crate::log::{debug, error, info, trace, warn};

    /// The no-op logging macros must expand to a valid expression of type `()`,
    /// not to nothing at all.
    ///
    /// Downstream crates call these as the trailing expression of a block, e.g.
    /// `else { warn!("..") }`. A macro with an empty transcriber still works as
    /// a statement, so this crate keeps compiling standalone and the breakage
    /// only shows up in dependents.
    #[test]
    fn macros_expand_in_expression_position() {
        let value = 42;

        let _: () = { error!("error {}", value) };
        let _: () = { warn!("warn {}", value) };
        let _: () = { info!("info {}", value) };
        let _: () = { debug!("debug {}", value) };
        let _: () = { trace!("trace {}", value) };
    }

    /// Statement position must keep working too.
    #[test]
    fn macros_expand_in_statement_position() {
        warn!("no args");
        warn!("with args {} {:?}", 1, "two");
    }
}
