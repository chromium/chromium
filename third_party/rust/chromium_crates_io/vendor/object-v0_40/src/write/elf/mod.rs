//! Support for writing ELF files.
//!
//! Provides [`Writer`] for low level writing of ELF files.
//! This is also used to provide ELF support for [`write::Object`](crate::write::Object).

mod object;

mod encoder;
pub use encoder::*;

mod writer;
pub use writer::*;
