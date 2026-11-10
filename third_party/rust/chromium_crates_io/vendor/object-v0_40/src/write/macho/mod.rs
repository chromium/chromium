//! Support for writing Mach-O files.
//!
//! Provides [`Encoder`] for low level writing of Mach-O files.
//! This is also used to provide Mach-O support for [`write::Object`](crate::write::Object).

mod encoder;
pub use encoder::*;

mod object;
pub use object::*;
