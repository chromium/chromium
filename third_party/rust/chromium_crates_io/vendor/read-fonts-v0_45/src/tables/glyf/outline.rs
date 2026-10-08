//! Building outlines from `glyf` data.
//!
//! There are two entry points:
//!
//! - [`Outline`] owns its buffers and grows them as needed. A caller supplies
//!   a glyph and a size.
//! - [`OutlinePlan`] allocates nothing. It walks the composite tree, reports
//!   how much space the result needs through [`buffer_lengths`], and
//!   [`load`s][load] into buffers the caller supplies. Use it to control where
//!   the memory comes from, or to supply a [`Hinter`].
//!
//! Both read through an [`OutlineContext`], which states the tables and the
//! variation location. [`OutlineTables`] implements it by reading a font.
//!
//! A load produces an [`OutlineRef`], which borrows the caller's buffers.
//!
//! [`buffer_lengths`]: OutlinePlan::buffer_lengths
//! [load]: OutlinePlan::load

mod borrowed;
mod buffers;
mod context;
mod error;
mod hinter;
mod load;
mod owned;
mod path;
mod plan;
mod scale;
#[cfg(test)]
mod testing;

pub use borrowed::OutlineRef;
pub use buffers::{BufferLengths, Buffers};
pub use context::{OutlineContext, OutlineTables};
pub use error::OutlineError;
pub use hinter::{GlyphZone, Hinter};
pub use owned::Outline;
pub use path::{contour_to_path, outline_to_path, PathContourStart};
pub use plan::OutlinePlan;
pub use scale::{Scale, Scale26Dot6, ScaleF32, Unscaled};
