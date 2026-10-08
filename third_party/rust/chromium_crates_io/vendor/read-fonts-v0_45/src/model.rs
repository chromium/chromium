//! Higher level interface for accessing font data.

pub mod charmap;
pub mod glyph;
pub mod metrics;
pub mod name;

mod font;
mod once;

pub use font::{
    interop as _font_interop, Blob, Font, Format, InstanceBuilder, Kind, NormalizedCoord, Source,
    TableFunction, Tables, Variation,
};
