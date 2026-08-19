//! CFF and CFF2 fonts.

pub mod blend;
pub mod charset;
pub mod dict;
pub mod encoding;
pub mod fd_select;
pub mod index;
pub mod stack;
pub mod v1;
pub mod v2;

mod font;

pub use font::{CffFontRef, Encoding, Metadata, Subfont};
