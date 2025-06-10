//! The builtins module contains the main implementation of the Temporal builtins

#[cfg(feature = "compiled_data")]
pub mod compiled;
pub mod core;

pub use core::*;

#[cfg(feature = "compiled_data")]
use crate::tzdb::FsTzdbProvider;
#[cfg(feature = "compiled_data")]
use std::sync::{LazyLock, Mutex};

#[cfg(feature = "compiled_data")]
pub static TZ_PROVIDER: LazyLock<Mutex<FsTzdbProvider>> =
    LazyLock::new(|| Mutex::new(FsTzdbProvider::default()));
