#![allow(missing_docs)]

use std::io::{self, Write};
use std::panic::{self, AssertUnwindSafe};
use std::process;

pub fn catch_unwind<F, R>(label: &'static str, foreign_call: F) -> R
where
    F: FnOnce() -> R,
{
    // Regarding the AssertUnwindSafe: we immediately abort on panic so it
    // doesn't matter whether the types involved are unwind-safe. The UnwindSafe
    // bound on catch_unwind is about ensuring nothing is in a broken state if
    // your program plans to continue after the panic.
    match panic::catch_unwind(AssertUnwindSafe(foreign_call)) {
        Ok(ret) => ret,
        Err(_) => abort(label),
    }
}

#[cold]
fn abort(label: &'static str) -> ! {
    let mut stderr = io::stderr();
    let _ = writeln!(stderr, "Error: panic in ffi function {}, aborting.", label);
    process::abort();
}
