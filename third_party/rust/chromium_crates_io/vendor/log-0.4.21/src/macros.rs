// Copyright 2014-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

/// The standard logging macro.
///
/// This macro will generically log with the specified `Level` and `format!`
/// based argument list.
///
/// # Examples
///
/// ```
/// use log::{log, Level};
///
/// # fn main() {
/// let data = (42, "Forty-two");
/// let private_data = "private";
///
/// log!(Level::Error, "Received errors: {}, {}", data.0, data.1);
/// log!(target: "app_events", Level::Warn, "App warning: {}, {}, {}",
///     data.0, data.1, private_data);
/// # }
/// ```
#[macro_export]
macro_rules! log {
    // log!(target: "my_target", Level::Info, key1:? = 42, key2 = true; "a {} event", "log");
    (target: $target:expr, $lvl:expr, $($key:tt $(:$capture:tt)? $(= $value:expr)?),+; $($arg:tt)+) => ({
        let lvl = $lvl;
        if lvl <= $crate::STATIC_MAX_LEVEL && lvl <= $crate::max_level() {
            $crate::__private_api::log::<&_>(
                $crate::__private_api::format_args!($($arg)+),
                lvl,
                &($target, $crate::__private_api::module_path!(), $crate::__private_api::file!()),
                $crate::__private_api::line!(),
                &[$(($crate::__log_key!($key), $crate::__log_value!($key $(:$capture)* = $($value)*))),+]
            );
        }
    });

    // log!(target: "my_target", Level::Info, "a {} event", "log");
    (target: $target:expr, $lvl:expr, $($arg:tt)+) => ({
        let lvl = $lvl;
        if lvl <= $crate::STATIC_MAX_LEVEL && lvl <= $crate::max_level() {
            $crate::__private_api::log(
                $crate::__private_api::format_args!($($arg)+),
                lvl,
                &($target, $crate::__private_api::module_path!(), $crate::__private_api::file!()),
                $crate::__private_api::line!(),
                (),
            );
        }
    });

    // log!(Level::Info, "a log event")
    ($lvl:expr, $($arg:tt)+) => ($crate::log!(target: $crate::__private_api::module_path!(), $lvl, $($arg)+));
}

/// Logs a message at the error level.
///
/// # Examples
///
/// ```
/// use log::error;
///
/// # fn main() {
/// let (err_info, port) = ("No connection", 22);
///
/// error!("Error: {err_info} on port {port}");
/// error!(target: "app_events", "App Error: {err_info}, Port: {port}");
/// # }
/// ```
#[macro_export]
macro_rules! error {
    // error!(target: "my_target", key1 = 42, key2 = true; "a {} event", "log")
    // error!(target: "my_target", "a {} event", "log")
    (target: $target:expr, $($arg:tt)+) => ($crate::log!(target: $target, $crate::Level::Error, $($arg)+));

    // error!("a {} event", "log")
    ($($arg:tt)+) => ($crate::log!($crate::Level::Error, $($arg)+))
}

/// Logs a message at the warn level.
///
/// # Examples
///
/// ```
/// use log::warn;
///
/// # fn main() {
/// let warn_description = "Invalid Input";
///
/// warn!("Warning! {warn_description}!");
/// warn!(target: "input_events", "App received warning: {warn_description}");
/// # }
/// ```
#[macro_export]
macro_rules! warn {
    // warn!(target: "my_target", key1 = 42, key2 = true; "a {} event", "log")
    // warn!(target: "my_target", "a {} event", "log")
    (target: $target:expr, $($arg:tt)+) => ($crate::log!(target: $target, $crate::Level::Warn, $($arg)+));

    // warn!("a {} event", "log")
    ($($arg:tt)+) => ($crate::log!($crate::Level::Warn, $($arg)+))
}

/// Logs a message at the info level.
///
/// # Examples
///
/// ```
/// use log::info;
///
/// # fn main() {
/// # struct Connection { port: u32, speed: f32 }
/// let conn_info = Connection { port: 40, speed: 3.20 };
///
/// info!("Connected to port {} at {} Mb/s", conn_info.port, conn_info.speed);
/// info!(target: "connection_events", "Successful connection, port: {}, speed: {}",
///       conn_info.port, conn_info.speed);
/// # }
/// ```
#[macro_export]
macro_rules! info {
    // info!(target: "my_target", key1 = 42, key2 = true; "a {} event", "log")
    // info!(target: "my_target", "a {} event", "log")
    (target: $target:expr, $($arg:tt)+) => ($crate::log!(target: $target, $crate::Level::Info, $($arg)+));

    // info!("a {} event", "log")
    ($($arg:tt)+) => ($crate::log!($crate::Level::Info, $($arg)+))
}

/// Logs a message at the debug level.
///
/// # Examples
///
/// ```
/// use log::debug;
///
/// # fn main() {
/// # struct Position { x: f32, y: f32 }
/// let pos = Position { x: 3.234, y: -1.223 };
///
/// debug!("New position: x: {}, y: {}", pos.x, pos.y);
/// debug!(target: "app_events", "New position: x: {}, y: {}", pos.x, pos.y);
/// # }
/// ```
#[macro_export]
macro_rules! debug {
    // debug!(target: "my_target", key1 = 42, key2 = true; "a {} event", "log")
    // debug!(target: "my_target", "a {} event", "log")
    (target: $target:expr, $($arg:tt)+) => ($crate::log!(target: $target, $crate::Level::Debug, $($arg)+));

    // debug!("a {} event", "log")
    ($($arg:tt)+) => ($crate::log!($crate::Level::Debug, $($arg)+))
}

/// Logs a message at the trace level.
///
/// # Examples
///
/// ```
/// use log::trace;
///
/// # fn main() {
/// # struct Position { x: f32, y: f32 }
/// let pos = Position { x: 3.234, y: -1.223 };
///
/// trace!("Position is: x: {}, y: {}", pos.x, pos.y);
/// trace!(target: "app_events", "x is {} and y is {}",
///        if pos.x >= 0.0 { "positive" } else { "negative" },
///        if pos.y >= 0.0 { "positive" } else { "negative" });
/// # }
/// ```
#[macro_export]
macro_rules! trace {
    // trace!(target: "my_target", key1 = 42, key2 = true; "a {} event", "log")
    // trace!(target: "my_target", "a {} event", "log")
    (target: $target:expr, $($arg:tt)+) => ($crate::log!(target: $target, $crate::Level::Trace, $($arg)+));

    // trace!("a {} event", "log")
    ($($arg:tt)+) => ($crate::log!($crate::Level::Trace, $($arg)+))
}

/// Determines if a message logged at the specified level in that module will
/// be logged.
///
/// This can be used to avoid expensive computation of log message arguments if
/// the message would be ignored anyway.
///
/// # Examples
///
/// ```
/// use log::Level::Debug;
/// use log::{debug, log_enabled};
///
/// # fn foo() {
/// if log_enabled!(Debug) {
///     let data = expensive_call();
///     debug!("expensive debug data: {} {}", data.x, data.y);
/// }
/// if log_enabled!(target: "Global", Debug) {
///    let data = expensive_call();
///    debug!(target: "Global", "expensive debug data: {} {}", data.x, data.y);
/// }
/// # }
/// # struct Data { x: u32, y: u32 }
/// # fn expensive_call() -> Data { Data { x: 0, y: 0 } }
/// # fn main() {}
/// ```
#[macro_export]
macro_rules! log_enabled {
    (target: $target:expr, $lvl:expr) => {{
        let lvl = $lvl;
        lvl <= $crate::STATIC_MAX_LEVEL
            && lvl <= $crate::max_level()
            && $crate::__private_api::enabled(lvl, $target)
    }};
    ($lvl:expr) => {
        $crate::log_enabled!(target: $crate::__private_api::module_path!(), $lvl)
    };
}

// These macros use a pattern of #[cfg]s to produce nicer error
// messages when log features aren't available

#[doc(hidden)]
#[macro_export]
#[cfg(feature = "kv")]
macro_rules! __log_key {
    // key1 = 42
    ($($args:ident)*) => {
        $crate::__private_api::stringify!($($args)*)
    };
    // "key1" = 42
    ($($args:expr)*) => {
        $($args)*
    };
}

#[doc(hidden)]
#[macro_export]
#[cfg(not(feature = "kv"))]
macro_rules! __log_key {
    ($($args:tt)*) => {
        compile_error!("key value support requires the `kv` feature of `log`")
    };
}

#[doc(hidden)]
#[macro_export]
#[cfg(feature = "kv")]
macro_rules! __log_value {
    // Entrypoint
    ($key:tt = $args:expr) => {
        $crate::__log_value!(($args):value)
    };
    ($key:tt :$capture:tt = $args:expr) => {
        $crate::__log_value!(($args):$capture)
    };
    ($key:ident =) => {
        $crate::__log_value!(($key):value)
    };
    ($key:ident :$capture:tt =) => {
        $crate::__log_value!(($key):$capture)
    };
    // ToValue
    (($args:expr):value) => {
        $crate::__private_api::capture_to_value(&&$args)
    };
    // Debug
    (($args:expr):?) => {
        $crate::__private_api::capture_debug(&&$args)
    };
    (($args:expr):debug) => {
        $crate::__private_api::capture_debug(&&$args)
    };
    // Display
    (($args:expr):%) => {
        $crate::__private_api::capture_display(&&$args)
    };
    (($args:expr):display) => {
        $crate::__private_api::capture_display(&&$args)
    };
    //Error
    (($args:expr):err) => {
        $crate::__log_value_error!($args)
    };
    // sval::Value
    (($args:expr):sval) => {
        $crate::__log_value_sval!($args)
    };
    // serde::Serialize
    (($args:expr):serde) => {
        $crate::__log_value_serde!($args)
    };
}

#[doc(hidden)]
#[macro_export]
#[cfg(not(feature = "kv"))]
macro_rules! __log_value {
    ($($args:tt)*) => {
        compile_error!("key value support requires the `kv` feature of `log`")
    };
}

#[doc(hidden)]
#[macro_export]
#[cfg(feature = "kv_sval")]
macro_rules! __log_value_sval {
    ($args:expr) => {
        $crate::__private_api::capture_sval(&&$args)
    };
}

#[doc(hidden)]
#[macro_export]
#[cfg(not(feature = "kv_sval"))]
macro_rules! __log_value_sval {
    ($args:expr) => {
        compile_error!("capturing values as `sval::Value` requites the `kv_sval` feature of `log`")
    };
}

#[doc(hidden)]
#[macro_export]
#[cfg(feature = "kv_serde")]
macro_rules! __log_value_serde {
    ($args:expr) => {
        $crate::__private_api::capture_serde(&&$args)
    };
}

#[doc(hidden)]
#[macro_export]
#[cfg(not(feature = "kv_serde"))]
macro_rules! __log_value_serde {
    ($args:expr) => {
        compile_error!(
            "capturing values as `serde::Serialize` requites the `kv_serde` feature of `log`"
        )
    };
}

#[doc(hidden)]
#[macro_export]
#[cfg(feature = "kv_std")]
macro_rules! __log_value_error {
    ($args:expr) => {
        $crate::__private_api::capture_error(&$args)
    };
}

#[doc(hidden)]
#[macro_export]
#[cfg(not(feature = "kv_std"))]
macro_rules! __log_value_error {
    ($args:expr) => {
        compile_error!(
            "capturing values as `std::error::Error` requites the `kv_std` feature of `log`"
        )
    };
}
