//! Loading, scaling and hinting of glyph outlines.
//!
//! Scaling is the process of decoding an outline, applying variation deltas,
//! and executing [hinting](https://en.wikipedia.org/wiki/Font_hinting)
//! instructions for a glyph of a particular size.
//!
//! ## It all starts with a context
//!
//! The scaling process generally requires dynamic memory allocations to hold
//! intermediate results. In addition, TrueType hinting requires execution
//! of a set of programs to generate state for any instance of a font before
//! applying glyph instructions.
//!
//! To amortize the cost of memory allocations and support caching of hinting
//! state, we use the [`Context`] type. This type is opaque and contains
//! internal buffers and caches that can be reused by subsequent scaling
//! operations.
//!
//! Contexts exist purely as a performance optimization and management of them
//! is up to the user. There are several reasonable strategies of varying
//! complexity:
//!
//! * If performance and heap traffic are not significant concerns, creating
//! a context per glyph (or glyph run) works in a pinch.
//! * When making use of a single shared glyph cache, this is an ideal place to
//! store a context.
//! * Multithreaded code can use thread locals or a global pool of contexts.
//!
//! Regardless of how you manage them, creating a context is trivial:
//! ```
//! use skrifa::scale::Context;
//!
//! let mut context = Context::new();
//! ```
//!
//! For simplicity, the examples below will use a local context.
//!
//! ## Building a scaler
//!
//! Now that we have a [`Context`], we can use the
//! [`new_scaler`](Context::new_scaler) method to generate an instance of the
//! [`ScalerBuilder`] type that allows us to configure and build a [`Scaler`].
//!
//! Assuming you have some `font` (any type that implements
//! [`TableProvider`]), this will build a scaler for
//! a size of 16px:
//!
//! ```
//! # use skrifa::{scale::*, instance::Size};
//! # fn build_scaler(font: read_fonts::FontRef) {
//! let mut context = Context::new();
//! let mut scaler = context.new_scaler()
//!     .size(Size::new(16.0))
//!     .build(&font);
//! # }
//! ```
//!
//! For variable fonts, the
//! [`variation_settings`](ScalerBuilder::variation_settings) method can
//! be used to specify user coordinates for selecting an instance:
//!
//! ```
//! # use skrifa::{scale::*, instance::Size};
//! # fn build_scaler(font: read_fonts::FontRef) {
//! let mut context = Context::new();
//! let mut scaler = context.new_scaler()
//!     .size(Size::new(16.0))
//!     .variation_settings(&[("wght", 720.0), ("wdth", 75.0)])
//!     .build(&font);
//! # }
//! ```
//!
//! If you already have coordinates in normalized design space, you can specify
//! those directly with the
//! [`normalized_coords`](ScalerBuilder::normalized_coords) method.
//!
//! See the [`ScalerBuilder`] type for all available configuration options.
//!
//! ## Getting an outline
//!
//! Once we have a configured scaler, extracting an outline is fairly simple.
//! The [`Scaler::outline`] method uses a callback approach where the user
//! provides an implementation of the [`Pen`] trait and the appropriate methods
//! are invoked for each resulting path element of the scaled outline.
//!
//! Assuming we constructed a scaler as above, let's load a glyph and convert
//! it into an SVG path:
//!
//! ```
//! # use skrifa::{scale::*, GlyphId, instance::Size};
//! # fn build_scaler(font: read_fonts::FontRef) {
//! # let mut context = Context::new();
//! # let mut scaler = context.new_scaler()
//! #    .size(Size::new(16.0))
//! #    .build(&font);
//! // Create a type for holding our SVG path.
//! #[derive(Default)]
//! struct SvgPath(String);
//!
//! // Implement the Pen trait for this type. This emits the appropriate
//! // SVG path commands for each element type.
//! impl Pen for SvgPath {
//!     fn move_to(&mut self, x: f32, y: f32) {
//!         self.0.push_str(&format!("M{x:.1},{y:.1} "));
//!     }
//!
//!     fn line_to(&mut self, x: f32, y: f32) {
//!         self.0.push_str(&format!("L{x:.1},{y:.1} "));
//!     }
//!
//!     fn quad_to(&mut self, cx0: f32, cy0: f32, x: f32, y: f32) {
//!         self.0
//!             .push_str(&format!("Q{cx0:.1},{cy0:.1} {x:.1},{y:.1} "));
//!     }
//!
//!     fn curve_to(&mut self, cx0: f32, cy0: f32, cx1: f32, cy1: f32, x: f32, y: f32) {
//!         self.0.push_str(&format!(
//!             "C{cx0:.1},{cy0:.1} {cx1:.1},{cy1:.1} {x:.1},{y:.1} "
//!         ));
//!     }
//!
//!     fn close(&mut self) {
//!         self.0.push_str("z ");
//!     }
//! }
//!
//! let mut path = SvgPath::default();
//!
//! // Scale an outline for glyph 20 and invoke the appropriate methods
//! // to build an SVG path.
//! scaler.outline(GlyphId::new(20), &mut path);
//!
//! // Print our pretty new path.
//! println!("{}", path.0);
//! # }
//! ```
//!
//! The pen based interface is designed to be flexible. Output can be sent
//! directly to a software rasterizer for scan conversion, converted to an
//! owned path representation (such as a kurbo
//! [`BezPath`](https://docs.rs/kurbo/latest/kurbo/struct.BezPath.html)) for
//! further analysis and transformation, or fed into other crates like
//! [vello](https://github.com/linebender/vello),
//! [lyon](https://github.com/nical/lyon) or
//! [pathfinder](https://github.com/servo/pathfinder) for GPU rendering.

pub use super::outline::{AdjustedMetrics as ScalerMetrics, DrawError as Error};
pub use read_fonts::types::Pen;

use core::borrow::Borrow;

use read_fonts::{
    types::{Fixed, GlyphId},
    TableProvider,
};

use super::{
    font::UniqueId,
    instance::{NormalizedCoord, Size},
    outline,
    setting::VariationSetting,
};

/// Result type for errors that may occur when loading glyphs.
pub type Result<T> = core::result::Result<T, Error>;

/// Modes for hinting.
///
/// Only the `glyf` source supports all hinting modes.
#[derive(Copy, Clone, PartialEq, Eq, Default, Debug)]
pub enum Hinting {
    /// Hinting is disabled.
    #[default]
    None,
    /// "Full" hinting mode. May generate rough outlines and poor horizontal
    /// spacing.
    Full,
    /// Light hinting mode. This prevents most movement in the horizontal
    /// direction with the exception of a per-font backward compatibility
    /// opt in.
    Light,
    /// Same as light, but with additional support for RGB subpixel rendering.
    LightSubpixel,
    /// Same as light subpixel, but always prevents adjustment in the
    /// horizontal direction. This is the default mode.
    VerticalSubpixel,
}

/// Context for scaling glyphs.
///
/// This type contains temporary memory buffers and various internal caches to
/// accelerate the glyph scaling process.
///
/// See the [module level documentation](crate::scale#it-all-starts-with-a-context)
/// for more detail.
#[derive(Clone, Default, Debug)]
pub struct Context {
    /// Storage for normalized variation coordinates.
    coords: Vec<NormalizedCoord>,
    /// Storage for variation settings.
    variations: Vec<VariationSetting>,
}

impl Context {
    /// Creates a new glyph scaling context.
    pub fn new() -> Self {
        Self::default()
    }

    /// Returns a builder for configuring a glyph scaler.
    pub fn new_scaler(&mut self) -> ScalerBuilder {
        ScalerBuilder::new(self)
    }
}

/// Builder for configuring a glyph scaler.
///
/// See the [module level documentation](crate::scale#building-a-scaler)
/// for more detail.
pub struct ScalerBuilder<'a> {
    context: &'a mut Context,
    cache_key: Option<UniqueId>,
    size: Size,
    hint: Option<Hinting>,
}

impl<'a> ScalerBuilder<'a> {
    /// Creates a new builder for configuring a scaler with the given context.
    pub fn new(context: &'a mut Context) -> Self {
        context.coords.clear();
        context.variations.clear();
        Self {
            context,
            cache_key: None,
            size: Size::unscaled(),
            hint: None,
        }
    }

    /// Sets a unique font identifier for hint state caching. Specifying `None` will
    /// disable caching.
    pub fn cache_key(mut self, key: Option<UniqueId>) -> Self {
        self.cache_key = key;
        self
    }

    /// Sets the requested font size.
    ///
    /// The default value is `Size::unscaled()` and outlines will be generated
    /// in font units.
    pub fn size(mut self, size: Size) -> Self {
        self.size = size;
        self
    }

    /// Sets the hinting mode.
    ///
    /// Passing `None` will disable hinting.
    pub fn hint(mut self, hint: Option<Hinting>) -> Self {
        self.hint = hint;
        self
    }

    /// Specifies a variation with a set of normalized coordinates.
    ///
    /// This will clear any variations specified with the variations method.
    pub fn normalized_coords<I>(self, coords: I) -> Self
    where
        I: IntoIterator,
        I::Item: Borrow<NormalizedCoord>,
    {
        self.context.variations.clear();
        self.context.coords.clear();
        self.context
            .coords
            .extend(coords.into_iter().map(|v| *v.borrow()));
        self
    }

    /// Appends the given sequence of variation settings. This will clear any
    /// variations specified as normalized coordinates.
    ///
    /// This methods accepts any type which can be converted into an iterator
    /// that yields a sequence of values that are convertible to
    /// [`VariationSetting`]. Various conversions from tuples are provided.
    ///
    /// The following are all equivalent:
    ///
    /// ```
    /// # use skrifa::{scale::*, setting::VariationSetting, Tag};
    /// # let mut context = Context::new();
    /// # let builder = context.new_scaler();
    /// // slice of VariationSetting
    /// builder.variation_settings(&[
    ///     VariationSetting::new(Tag::new(b"wgth"), 720.0),
    ///     VariationSetting::new(Tag::new(b"wdth"), 50.0),
    /// ])
    /// # ; let builder = context.new_scaler();
    /// // slice of (Tag, f32)
    /// builder.variation_settings(&[(Tag::new(b"wght"), 720.0), (Tag::new(b"wdth"), 50.0)])
    /// # ; let builder = context.new_scaler();
    /// // slice of (&str, f32)
    /// builder.variation_settings(&[("wght", 720.0), ("wdth", 50.0)])
    /// # ;
    ///
    /// ```
    ///
    /// Iterators that yield the above types are also accepted.
    pub fn variation_settings<I>(self, settings: I) -> Self
    where
        I: IntoIterator,
        I::Item: Into<VariationSetting>,
    {
        self.context.coords.clear();
        self.context
            .variations
            .extend(settings.into_iter().map(|v| v.into()));
        self
    }

    /// Builds a scaler using the currently configured settings
    /// and the specified font.
    pub fn build(mut self, font: &impl TableProvider<'a>) -> Scaler<'a> {
        self.resolve_variations(font);
        let coords = &self.context.coords[..];
        let outlines = outline::OutlineGlyphCollection::new(font);
        Scaler {
            outlines,
            size: self.size,
            coords,
        }
    }

    fn resolve_variations(&mut self, font: &impl TableProvider<'a>) {
        if self.context.variations.is_empty() {
            return; // nop
        }
        let Ok(fvar) = font.fvar() else {
            return; // nop
        };
        let Ok(axes) = fvar.axes() else {
            return; // nop
        };
        let avar_mappings = font.avar().ok().map(|avar| avar.axis_segment_maps());
        let axis_count = fvar.axis_count() as usize;
        self.context.coords.clear();
        self.context
            .coords
            .resize(axis_count, NormalizedCoord::default());
        for variation in &self.context.variations {
            // To permit non-linear interpolation, iterate over all axes to ensure we match
            // multiple axes with the same tag:
            // https://github.com/PeterConstable/OT_Drafts/blob/master/NLI/UnderstandingNLI.md
            // We accept quadratic behavior here to avoid dynamic allocation and with the assumption
            // that fonts contain a relatively small number of axes.
            for (i, axis) in axes
                .iter()
                .enumerate()
                .filter(|(_, axis)| axis.axis_tag() == variation.selector)
            {
                let coord = axis.normalize(Fixed::from_f64(variation.value as f64));
                let coord = avar_mappings
                    .as_ref()
                    .and_then(|mappings| mappings.get(i).transpose().ok())
                    .flatten()
                    .map(|mapping| mapping.apply(coord))
                    .unwrap_or(coord);
                self.context.coords[i] = coord.to_f2dot14();
            }
        }
    }
}

/// Glyph scaler for a specific font and configuration.
///
/// See the [module level documentation](crate::scale#getting-an-outline)
/// for more detail.
pub struct Scaler<'a> {
    outlines: outline::OutlineGlyphCollection<'a>,
    size: Size,
    coords: &'a [NormalizedCoord],
}

impl<'a> Scaler<'a> {
    /// Returns the current set of normalized coordinates in use by the scaler.
    pub fn normalized_coords(&self) -> &[NormalizedCoord] {
        self.coords
    }

    /// Returns true if the scaler has a source for simple outlines.
    pub fn has_outlines(&self) -> bool {
        self.outlines.format().is_some()
    }

    /// Loads a simple outline for the specified glyph identifier and invokes the functions
    /// in the given pen for the sequence of path commands that define the outline.
    pub fn outline(&mut self, glyph_id: GlyphId, pen: &mut impl Pen) -> Result<ScalerMetrics> {
        let outline = self
            .outlines
            .get(glyph_id)
            .ok_or(Error::GlyphNotFound(glyph_id))?;
        outline.draw((self.size, self.coords), pen)
    }
}

#[cfg(test)]
mod tests {
    use super::{Context, Size};
    use read_fonts::{scaler_test, types::GlyphId, FontRef, TableProvider};

    #[test]
    fn vazirmatin_var() {
        compare_glyphs(
            font_test_data::VAZIRMATN_VAR,
            font_test_data::VAZIRMATN_VAR_GLYPHS,
        );
    }

    #[test]
    fn cantarell_vf() {
        compare_glyphs(
            font_test_data::CANTARELL_VF_TRIMMED,
            font_test_data::CANTARELL_VF_TRIMMED_GLYPHS,
        );
    }

    #[test]
    fn noto_serif_display() {
        compare_glyphs(
            font_test_data::NOTO_SERIF_DISPLAY_TRIMMED,
            font_test_data::NOTO_SERIF_DISPLAY_TRIMMED_GLYPHS,
        );
    }

    #[test]
    fn overlap_flags() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let mut cx = Context::new();
        let mut path = scaler_test::Path::default();
        let mut scaler = cx.new_scaler().build(&font);
        let glyph_count = font.maxp().unwrap().num_glyphs();
        // GID 2 is a composite glyph with the overlap bit on a component
        // GID 3 is a simple glyph with the overlap bit on the first flag
        let expected_gids_with_overlap = vec![2, 3];
        assert_eq!(
            expected_gids_with_overlap,
            (0..glyph_count)
                .filter(|gid| scaler
                    .outline(GlyphId::new(*gid), &mut path)
                    .unwrap()
                    .has_overlaps)
                .collect::<Vec<_>>()
        );
    }

    fn compare_glyphs(font_data: &[u8], expected_outlines: &str) {
        let font = FontRef::new(font_data).unwrap();
        let outlines = scaler_test::parse_glyph_outlines(expected_outlines);
        let mut cx = Context::new();
        let mut path = scaler_test::Path::default();
        for expected_outline in &outlines {
            if expected_outline.size == 0.0 && !expected_outline.coords.is_empty() {
                continue;
            }
            path.elements.clear();
            let size = if expected_outline.size != 0.0 {
                Size::new(expected_outline.size)
            } else {
                Size::unscaled()
            };
            let mut scaler = cx
                .new_scaler()
                .size(size)
                .normalized_coords(&expected_outline.coords)
                .build(&font);
            scaler
                .outline(expected_outline.glyph_id, &mut path)
                .unwrap();
            if path.elements != expected_outline.path {
                panic!(
                    "mismatch in glyph path for id {} (size: {}, coords: {:?}): path: {:?} expected_path: {:?}",
                    expected_outline.glyph_id,
                    expected_outline.size,
                    expected_outline.coords,
                    &path.elements,
                    &expected_outline.path
                );
            }
        }
    }
}
