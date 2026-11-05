//! Drawing glyphs from fonts at a size, with hinting.
//!
//! A [`Font`] is a font at a location in its variation space. A
//! [`ScalerFont`] draws the font's glyph outlines at a size in pixels per em,
//! hinted or not.
//!
//! ```
//! use skrifa::{
//!     font::{glyph::outline::SvgSink, Font},
//!     DrawMemory, HintOptions, RasterTarget, ScalerFont,
//! };
//!
//! # fn wrapper(data: Vec<u8>) -> Result<(), skrifa::ScaleError> {
//! // Load the first font in the file and create a bold instance.
//! let font = Font::new(data, 0).unwrap();
//! let bold = font.instance_builder().variations([("wght", 700.0)]).build();
//!
//! // Find the glyph ID for the character 'A'.
//! let glyph_id = bold.charmap().map_unicode('A').unwrap_or_default();
//!
//! // Configure a scaler font for hinted drawing at 16 pixels per em, using
//! // the interpreter with a horizontal subpixel target.
//! let options = HintOptions::interpreter().with_target(RasterTarget::Lcd);
//! let scaler = ScalerFont::hinted(bold, 16.0, options);
//!
//! if let Some(outline) = scaler.outline(glyph_id) {
//!     let mut path = SvgSink::new();
//!     // Draw into the path and store the resulting metrics.
//!     let metrics = outline.draw(DrawMemory::Scratch, &mut path)?;
//!     println!("{path} advances by {:?}", metrics.h_advance);
//! }
//! # Ok(())
//! # }
//! ```
//!
//! # Drawing
//!
//! [`ScalerFont::outline`] returns a glyph's outline as a [`ScalerOutline`],
//! which draws into any [`PathSink`]. Drawing needs working memory. By
//! default it uses short-lived scratch memory, and a caller drawing many
//! glyphs can supply a reusable buffer instead.
//!
//! A scaler draws from the first outline table the font contains, in
//! [`GlyphSourceOrder::DEFAULT`]: `VARC` composites over `glyf`, `CFF2`,
//! `CFF` or Type 1 charstrings. To choose other tables, pass a
//! [`ScalerSource`] in place of the font. The scaler reports the tables it
//! draws from.
//!
//! A scaler with no size draws in font units.
//!
//! # Hinting
//!
//! Hinting fits an outline to the pixel grid at the scaler's size.
//! [`HintOptions`] configures it:
//!
//! - **Engine.** Run the font's own hints, either TrueType instructions or
//!   PostScript hints; run the automatic hinter, which derives hints from
//!   the outline; or pick based on the font, using its own hints where they
//!   are worth running. A TrueType font that needs its instructions to draw
//!   correct contours always runs them.
//! - **Target.** Monochrome, grayscale, light grayscale, or horizontal or
//!   vertical subpixel rendering. Each target hints to suit the way its
//!   pixels are drawn.
//! - **Linear metrics.** Hint only vertically, keeping the unhinted
//!   horizontal shape and spacing.
//! - **ClearType compatibility.** Choose between the current ClearType
//!   behavior of the TrueType interpreter, which follows the font's `gasp`
//!   table, and the classic behavior, which grid fits every glyph.
//! - **Strictness.** Fail a draw when a glyph's instructions fail, instead of
//!   keeping the outline as they left it.
//!
//! The scaler reports the engine that runs, and the rasterizer settings the
//! font requests at the size: whether to smooth along both axes, and the
//! dropout control a glyph asks of a monochrome rasterizer.
//!
//! # Metrics
//!
//! A scaler returns the font's line metrics and heights, its style metrics
//! such as the underline and italic angle, and the advance and bounds of each
//! glyph, all scaled linearly to its size. Hinting can change a glyph's
//! advance, so hinted glyph metrics come from drawing the outline.
//!
//! # The font model
//!
//! The [`font`] module holds everything else about a font: its character
//! map, names, variation axes and instances, and color and bitmap glyphs.
//!
//! # Need shaping?
//!
//! [HarfRust](https://docs.rs/harfrust) turns text into positioned glyphs.
//! It reads fonts through the same font model, so a [`Font`] can be shaped
//! with HarfRust and drawn with a [`ScalerFont`].

#![cfg_attr(docsrs, feature(doc_cfg))]
// Compiles the README's example.
#![cfg_attr(doctest, doc = include_str!("../README.md"))]
#![forbid(unsafe_code)]
#![cfg_attr(not(any(test, feature = "std")), no_std)]

#[cfg(not(any(feature = "libm", feature = "std")))]
compile_error!("Either feature \"std\" or \"libm\" must be enabled for this crate.");

#[cfg(not(any(test, feature = "std")))]
#[macro_use]
extern crate core as std;

#[macro_use]
extern crate alloc;

/// Expose our "raw" underlying parser crate.
pub extern crate read_fonts as raw;

pub mod font;
mod scaler;

#[doc(inline)]
pub use font::Font;
pub use read_fonts::types::{GlyphId, Tag};
pub use scaler::{
    DrawMemory, DrawnMetrics, DropoutControl, GlyphSource, GlyphSourceOrder, HintEngine,
    HintOptions, OutlineSource, PathSink, RasterPolicy, RasterTarget, ScaleError, ScalerFont,
    ScalerOutline, ScalerSource,
};

// The API that `ScalerFont` and the font model replace. It is kept so that
// existing code builds, and hidden from the documentation.

#[doc(hidden)]
pub mod attribute;
#[doc(hidden)]
pub mod bitmap;
#[doc(hidden)]
pub mod charmap;
#[doc(hidden)]
pub mod color;
#[doc(hidden)]
pub mod instance;
#[doc(hidden)]
pub mod metrics;
#[doc(hidden)]
pub mod outline;
#[doc(hidden)]
pub mod setting;
#[doc(hidden)]
pub mod string;

mod decycler;
mod glyph_name;
mod provider;
mod variation;

#[doc(hidden)]
pub use glyph_name::{GlyphName, GlyphNameSource, GlyphNames};
#[doc(hidden)]
pub use outline::{OutlineGlyph, OutlineGlyphCollection};
#[doc(hidden)]
pub use provider::MetadataProvider;
#[doc(hidden)]
pub use read_fonts::{types::GlyphId16, FontRef};
#[doc(hidden)]
pub use variation::{Axis, AxisCollection, NamedInstance, NamedInstanceCollection};

/// Useful collection of common types suitable for glob importing.
#[doc(hidden)]
pub mod prelude {
    #[doc(no_inline)]
    pub use super::{
        font::FontRef,
        instance::{LocationRef, NormalizedCoord, Size},
        GlyphId, MetadataProvider, Tag,
    };
}

use read_fonts::limits::MAX_OUTLINE_POINTS;
