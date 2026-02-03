#![cfg_attr(feature = "std", doc = include_str!("../README.md"))]
#![cfg_attr(not(feature = "std"), no_std)]
#![cfg_attr(all(target_os = "wasi", target_env = "p2"), feature(wasip2))]

#[doc = include_str!("../README.md")]
#[cfg(all(doctest, feature = "std"))]
pub struct ReadmeDoctests;

extern crate alloc;

mod no_std_compat {
    #[allow(unused_imports)]
    pub use alloc::{
        boxed::Box,
        string::{String, ToString},
        vec,
        vec::Vec,
    };
}

#[macro_use]
mod common;
#[cfg(all(not(target_family = "wasm"), any(windows, unix)))]
mod native;
#[cfg(all(not(target_family = "wasm"), any(windows, unix)))]
mod platform;
mod typed;
mod unix;
#[cfg(all(feature = "std", not(target_family = "wasm")))]
pub mod utils;
mod windows;

mod private {
    /// Used to mark traits as sealed to prevent implements from others outside of this crate
    pub trait Sealed {}
}

pub use common::*;
#[cfg(all(not(target_family = "wasm"), any(windows, unix)))]
pub use native::*;
#[cfg(all(not(target_family = "wasm"), any(windows, unix)))]
pub use platform::*;
pub use typed::*;
pub use unix::*;
pub use windows::*;

/// Contains constants associated with different path formats.
pub mod constants {
    use super::unix::constants as unix_constants;
    use super::windows::constants as windows_constants;

    /// Contains constants associated with Unix paths.
    pub mod unix {
        pub use super::unix_constants::*;
    }

    /// Contains constants associated with Windows paths.
    pub mod windows {
        pub use super::windows_constants::*;
    }
}
