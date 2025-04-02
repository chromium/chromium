//! Diagnostic reporting support for the codespan crate.

#![forbid(unsafe_code)]
#![no_std]

extern crate alloc;

#[cfg(feature = "std")]
extern crate std;

pub mod diagnostic;
pub mod files;
pub mod term;
