// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! General helpers

/// Given a `Result<_, E>` expression where `E: Display`, log the `Err(e)` case
/// and pass the result through.
#[macro_export]
macro_rules! log_err {
    ($res_expr:expr, $ctx:expr $(, $name:ident = $arg:expr)*) => {
        {
            let res = $res_expr;
            if let Err(e) = &res {
                ::log::error!(concat!($ctx, ": {__error}"), __error = e $(, $name = $arg)*);
            }
            res
        }
    };

    ($res_expr:expr) => {
        {
            let res = $res_expr;
            if let Err(e) = &res {
                ::log::error!("`{expr_str}`: {e}", expr_str = stringify!($res_expr));
            }
            res
        }
    }
}

/// Wrap a value `T: Debug` to give it a `Display` instance.
pub struct AsDebug<T>(pub T);

impl<T: std::fmt::Debug> std::fmt::Display for AsDebug<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        <T as std::fmt::Debug>::fmt(&self.0, f)
    }
}
