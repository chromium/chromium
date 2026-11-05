//! Color glyphs.
//!
//! A `COLR` glyph is painted through a sequence of [`PaintSink`] callbacks
//! for transforms, clips, fills and layers. Version 0 glyphs are layers of
//! solid-filled outlines. Version 1 adds gradients, transforms and
//! compositing. [`ColorGlyph::paint`] paints both versions in font units at
//! the font's location.
//!
//! # Examples
//!
//! Paint a glyph:
//!
//! ```
//! # use read_fonts::{model::{glyph::color::PaintSink, Error, Font}, types::GlyphId};
//! # fn paint(font: &Font, sink: &mut impl PaintSink, glyph: GlyphId) -> Result<(), Error> {
//! let glyph = font.color_glyphs().get(glyph).expect("a color glyph");
//! glyph.paint(sink)
//! # }
//! ```
//!
//! Measure a glyph. This uses the clip box if the glyph has
//! one, and the bounds of what it paints otherwise:
//!
//! ```
//! # use read_fonts::{model::Font, types::GlyphId};
//! # fn measure(font: &Font, glyph: GlyphId) {
//! if let Some(glyph) = font.color_glyphs().get(glyph) {
//!     let extents = glyph.extents().expect("a well formed paint graph");
//!     println!("{extents:?}");
//! }
//! # }
//! ```

mod bounds;
mod decycler;
mod instance;
mod traversal;

#[cfg(test)]
pub(crate) mod test_glyph_defs;
#[cfg(all(test, feature = "serde", feature = "std"))]
mod traversal_tests;

use crate::{
    model::{font::ColrTables, glyph::outline::OutlineGlyphs, metrics::GlyphExtents, Error, Font},
    tables::{colr, cpal::Cpal},
    types::{BoundingBox, GlyphId, Matrix, NameId, Point},
};
use core::ops::Range;
use instance::{ColrInstance, PaintId};
use traversal::{traverse_v0_range, traverse_with_callbacks, PaintDecycler, TraversalState};

#[doc(inline)]
pub use crate::tables::colr::{CompositeMode, Extend};
#[doc(inline)]
pub use crate::tables::cpal::{ColorRecord as Color, PaletteType};

/// A color stop of a gradient.
///
/// Gradients passed to a [`PaintSink`] always have their stops normalized to
/// the range 0 to 1.
#[derive(Copy, Clone, PartialEq, Debug, Default)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
// `repr(C)` so a C caller can read a slice of these as an array.
#[repr(C)]
pub struct ColorStop {
    /// The position of the stop along the gradient, from 0 to 1.
    pub offset: f32,
    /// The index of the color in a `CPAL` palette.
    pub palette_index: u16,
    /// A multiplier applied to the alpha of the color.
    pub alpha: f32,
}

/// A solid color or gradient used to fill a region.
///
/// Gradients are normalized. A linear gradient is reduced to two points with
/// its stops between them. Other gradients have their stops in the range 0
/// to 1, with their geometry adjusted to match.
#[derive(Debug, Clone, PartialEq)]
pub enum Brush<'a> {
    /// A solid fill with the color at `palette_index`, multiplied by `alpha`.
    Solid { palette_index: u16, alpha: f32 },
    /// A linear gradient from `p0` to `p1`, reduced from the three points in
    /// the table.
    ///
    /// With one stop, the fill is the solid color of that stop.
    LinearGradient {
        p0: Point<f32>,
        p1: Point<f32>,
        color_stops: &'a [ColorStop],
        extend: Extend,
    },
    /// A radial gradient between two circles.
    ///
    /// Normalizing the stops can produce a negative radius. In that case, the
    /// color line is truncated where the interpolated radius reaches zero.
    RadialGradient {
        c0: Point<f32>,
        r0: f32,
        c1: Point<f32>,
        r1: f32,
        color_stops: &'a [ColorStop],
        extend: Extend,
    },
    /// A sweep gradient around `c0`, with angles in degrees clockwise.
    ///
    /// This is the reverse of the direction in the table. Normalizing the
    /// stops can move the angles outside 0 to 360. Only that range is drawn.
    SweepGradient {
        c0: Point<f32>,
        start_angle: f32,
        end_angle: f32,
        color_stops: &'a [ColorStop],
        extend: Extend,
    },
}

/// Whether a sink painted a nested color glyph from its own cache.
///
/// Returned from [`PaintSink::paint_cached_color_glyph`] when one color
/// glyph paints another.
pub enum PaintCachedColorGlyph {
    /// The sink painted the glyph itself.
    Ok,
    /// The sink has no cached copy, so the paint graph of the glyph is
    /// traversed.
    Unimplemented,
}

/// Receives the operations that paint a color glyph.
///
/// Each callback is one canvas operation. [`ColorGlyph::paint`] calls them
/// with values in font units. A sink that draws at a specific size scales
/// them by `size / upem`.
pub trait PaintSink {
    /// Concatenates `transform` to the current transformation matrix.
    fn push_transform(&mut self, transform: Matrix<f32>);

    /// Restores the transformation matrix from before the last
    /// [`push_transform`](Self::push_transform).
    fn pop_transform(&mut self);

    /// Clips to the outline of `glyph`.
    fn push_clip_glyph(&mut self, glyph: GlyphId);

    /// Clips to `clip_box`.
    fn push_clip_box(&mut self, clip_box: BoundingBox<f32>);

    /// Restores the clip from before the last [`push_clip_glyph`] or
    /// [`push_clip_box`].
    ///
    /// [`push_clip_glyph`]: Self::push_clip_glyph
    /// [`push_clip_box`]: Self::push_clip_box
    fn pop_clip(&mut self);

    /// Fills the current clip with `brush`.
    fn fill(&mut self, brush: Brush<'_>);

    /// Clips to the outline of `glyph` and fills it with `brush`, with
    /// `brush_transform` applied to the brush alone.
    ///
    /// The default implementation uses the other callbacks. Override it if
    /// the graphics library can fill a path in one call.
    fn fill_glyph(
        &mut self,
        glyph: GlyphId,
        brush_transform: Option<Matrix<f32>>,
        brush: Brush<'_>,
    ) {
        self.push_clip_glyph(glyph);
        if let Some(wrap_in_transform) = brush_transform {
            self.push_transform(wrap_in_transform);
            self.fill(brush);
            self.pop_transform();
        } else {
            self.fill(brush);
        }
        self.pop_clip();
    }

    /// Paints a nested color glyph from the sink's cache, if it has one.
    ///
    /// The glyph is painted unscaled under the current transformation
    /// matrix. The default implementation has no cache, so the paint graph
    /// of the glyph is traversed.
    fn paint_cached_color_glyph(
        &mut self,
        _glyph: GlyphId,
    ) -> Result<PaintCachedColorGlyph, Error> {
        Ok(PaintCachedColorGlyph::Unimplemented)
    }

    /// Pushes a new layer that [`pop_layer`](Self::pop_layer) composites
    /// onto the layer below with `composite_mode`.
    fn push_layer(&mut self, composite_mode: CompositeMode);

    /// Composites the last layer onto the layer below, using the mode passed
    /// to the matching [`push_layer`](Self::push_layer).
    fn pop_layer(&mut self) {}

    /// Same as [`pop_layer`](Self::pop_layer), but also receives the
    /// composite mode.
    ///
    /// Use this for a graphics library that needs the mode when the layer is
    /// popped. Only one of the two methods needs an implementation. The
    /// default calls [`pop_layer`](Self::pop_layer).
    fn pop_layer_with_mode(&mut self, _composite_mode: CompositeMode) {
        self.pop_layer();
    }
}

/// The version of `COLR` that defines a glyph.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum ColorGlyphVersion {
    /// Layered outlines, each filled with a solid color.
    ColrV0,
    /// A paint graph.
    ColrV1,
}

/// Provides access to the color glyphs in a font.
///
/// Glyphs are painted at the location of the font. This type is cheap to
/// create and copy because it borrows tables that the font has already
/// parsed.
#[derive(Clone, Copy)]
pub struct ColorGlyphs<'a> {
    font: &'a Font,
    tables: Option<&'a ColrTables<'a>>,
}

impl<'a> ColorGlyphs<'a> {
    pub(crate) fn new(font: &'a Font) -> Self {
        Self {
            font,
            tables: font.colr_tables(),
        }
    }

    /// Returns the font that contains these glyphs.
    pub fn font(&self) -> &'a Font {
        self.font
    }

    /// Returns true if the font has no `COLR` table.
    pub fn is_empty(&self) -> bool {
        self.tables.is_none()
    }

    /// Returns the version of the `COLR` table, or `None` if the font has no
    /// `COLR` table.
    pub fn version(&self) -> Option<u16> {
        Some(self.tables?.colr.version())
    }

    /// Returns the color glyph for `glyph` painted with the given `version`
    /// of `COLR`, if the table has one.
    pub fn get_with_version(
        &self,
        glyph: GlyphId,
        version: ColorGlyphVersion,
    ) -> Option<ColorGlyph<'a>> {
        let colr = &self.tables?.colr;
        let root = match version {
            ColorGlyphVersion::ColrV0 => Root::V0(colr.v0_base_glyph(glyph)?),
            ColorGlyphVersion::ColrV1 => {
                let (paint, paint_id) = colr.v1_base_glyph(glyph)?;
                Root::V1(paint, paint_id)
            }
        };
        Some(ColorGlyph {
            glyphs: *self,
            glyph,
            root,
        })
    }

    /// Returns the color glyph for `glyph`.
    ///
    /// Uses version 1 of the table if it has the glyph, and version 0
    /// otherwise.
    pub fn get(&self, glyph: GlyphId) -> Option<ColorGlyph<'a>> {
        self.get_with_version(glyph, ColorGlyphVersion::ColrV1)
            .or_else(|| self.get_with_version(glyph, ColorGlyphVersion::ColrV0))
    }

    /// Returns the ink bounds of `glyph` in font units, measured as HarfBuzz
    /// measures a color glyph.
    ///
    /// Uses the clip box of a version 1 glyph if it has one. Otherwise the
    /// glyph is painted, from either version, and its fills are measured.
    /// Returns `None` if the table has no color glyph for `glyph` or
    /// painting it fails. A fill clipped to an outline measures the outline
    /// in `outlines`.
    pub(crate) fn extents(
        &self,
        glyph: GlyphId,
        outlines: OutlineGlyphs<'a>,
    ) -> Option<GlyphExtents<f32>> {
        self.get(glyph)?.extents_with(outlines).ok()
    }

    fn instance(&self) -> Option<ColrInstance<'a>> {
        Some(ColrInstance::new(
            self.tables?,
            self.font.normalized_coords(),
            self.font.colr_scalars(),
        ))
    }
}

/// A color glyph, ready to paint through a [`PaintSink`].
#[derive(Clone)]
pub struct ColorGlyph<'a> {
    glyphs: ColorGlyphs<'a>,
    glyph: GlyphId,
    root: Root<'a>,
}

#[derive(Clone)]
enum Root<'a> {
    V0(Range<usize>),
    V1(colr::Paint<'a>, PaintId),
}

impl<'a> ColorGlyph<'a> {
    /// Returns the glyph that this color glyph paints.
    pub fn glyph(&self) -> GlyphId {
        self.glyph
    }

    /// Returns the `COLR` version used to paint this glyph.
    pub fn version(&self) -> ColorGlyphVersion {
        match &self.root {
            Root::V0(_) => ColorGlyphVersion::ColrV0,
            Root::V1(..) => ColorGlyphVersion::ColrV1,
        }
    }

    /// Returns the clip box for the glyph, in font units at the font's
    /// location, if the table defines one.
    ///
    /// Only version 1 tables define clip boxes.
    pub fn clip_box(&self) -> Option<BoundingBox<f32>> {
        traversal::get_clipbox_font_units(&self.glyphs.instance()?, self.glyph)
    }

    /// Returns the clip box used by [`extents`](Self::extents), with
    /// variation deltas rounded to whole units as in HarfBuzz.
    fn clip_box_for_extents(&self) -> Option<BoundingBox<f32>> {
        let instance = self.glyphs.instance()?;
        let clip_box = instance.v1_clip_box(self.glyph)?;
        Some(instance::resolve_clip_box_for_extents(&instance, &clip_box))
    }

    /// Paints the glyph into `sink`, in font units at the font's location.
    ///
    /// A version 1 glyph is clipped to its clip box, if it has one, and its
    /// paint graph is traversed from the root. A version 0 glyph fills each
    /// of its layers in order.
    pub fn paint(&self, sink: &mut impl PaintSink) -> Result<(), Error> {
        self.paint_dyn(sink)
    }

    /// Calls [`paint`](Self::paint) through a trait object, so the traversal
    /// is compiled once instead of once per sink type.
    fn paint_dyn(&self, sink: &mut dyn PaintSink) -> Result<(), Error> {
        let instance = self.glyphs.instance().ok_or(Error::Malformed)?;
        match &self.root {
            Root::V1(paint, paint_id) => {
                let clipbox = traversal::get_clipbox_font_units(&instance, self.glyph);
                if let Some(rect) = clipbox {
                    sink.push_clip_box(rect);
                }
                let mut decycler = PaintDecycler::default();
                let mut cycle_guard = decycler.enter(*paint_id)?;
                let mut state = TraversalState::new(instance, sink);
                let result = state.resolve_paint(paint).and_then(|resolved| {
                    traverse_with_callbacks(&resolved, &mut state, &mut cycle_guard, 0)
                });
                if clipbox.is_some() {
                    sink.pop_clip();
                }
                result
            }
            Root::V0(range) => traverse_v0_range(range, &instance, sink),
        }
    }

    /// Returns the ink bounds of the glyph in font units.
    ///
    /// Uses the clip box if the table defines one. Otherwise the glyph is
    /// painted and the bounds of its fills are returned. A fill clipped to an
    /// outline is measured by the control box of that outline. A glyph that
    /// paints nothing has zero extents.
    pub fn extents(&self) -> Result<GlyphExtents<f32>, Error> {
        self.extents_with(self.glyphs.font.outline_glyphs())
    }

    /// [`extents`](Self::extents), measuring clip glyphs in `outlines`.
    fn extents_with(&self, outlines: OutlineGlyphs<'a>) -> Result<GlyphExtents<f32>, Error> {
        if let Some(clip_box) = self.clip_box_for_extents() {
            return Ok(clip_box.into());
        }
        let mut sink = bounds::BoundsSink::new(outlines);
        self.paint_dyn(&mut sink)?;
        Ok(sink.bounds().into())
    }
}

/// A single color palette.
#[derive(Clone, Copy)]
pub struct Palette<'a> {
    cpal: &'a Cpal<'a>,
    /// The color records for this palette.
    colors: &'a [Color],
    /// The palette index in the table.
    index: u32,
}

impl<'a> Palette<'a> {
    /// Returns the colors in the palette.
    pub fn colors(&self) -> &'a [Color] {
        self.colors
    }

    /// Returns whether the palette is meant for a light or dark background,
    /// if the table provides this.
    pub fn palette_type(&self) -> Option<PaletteType> {
        self.cpal
            .palette_types_array()?
            .ok()?
            .get(self.index as usize)
            .map(|p| p.get())
    }

    /// Returns the name ID for the palette label, if the table has one.
    pub fn label(&self) -> Option<NameId> {
        let name_id = self
            .cpal
            .palette_labels_array()?
            .ok()?
            .get(self.index as usize)?
            .get();
        Some(name_id).filter(|name_id| name_id.to_u16() != 0xFFFF)
    }

    /// Returns the palette index in the table.
    pub fn index(&self) -> u32 {
        self.index
    }
}

/// Provides access to the color palettes in a font.
#[derive(Clone, Copy)]
pub struct Palettes<'a> {
    cpal: Option<&'a Cpal<'a>>,
}

impl<'a> Palettes<'a> {
    pub(crate) fn new(font: &'a Font) -> Self {
        Self { cpal: font.cpal() }
    }

    /// Returns the number of palettes, or zero if the font has no `CPAL`
    /// table.
    pub fn len(&self) -> u32 {
        self.cpal.map_or(0, |cpal| cpal.num_palettes() as u32)
    }

    /// Returns true if there are no palettes.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Returns the palette at `index`. Index zero is the default palette.
    pub fn get(&self, index: u32) -> Option<Palette<'a>> {
        let cpal = self.cpal?;
        let start = usize::from(cpal.color_record_indices().get(index as usize)?.get());
        let count = usize::from(cpal.num_palette_entries());
        let colors = cpal
            .color_records_array()?
            .ok()?
            .get(start..start + count)?;
        Some(Palette {
            cpal,
            colors,
            index,
        })
    }

    /// Returns an iterator over the palettes, in index order.
    pub fn iter(&self) -> impl Iterator<Item = Palette<'a>> + 'a {
        let this = *self;
        (0..this.len()).filter_map(move |index| this.get(index))
    }

    /// Returns the name ID for the label of the color at `color_index`, if
    /// the table has one.
    ///
    /// The label applies to that color in every palette.
    pub fn color_label(&self, color_index: u32) -> Option<NameId> {
        let name_id = self
            .cpal?
            .palette_entry_labels_array()?
            .ok()?
            .get(color_index as usize)?
            .get();
        Some(name_id).filter(|name_id| name_id.to_u16() != 0xFFFF)
    }
}

#[cfg(test)]
mod tests {
    use super::{
        test_glyph_defs::{COLORED_CIRCLES_V0, COLORED_CIRCLES_V1, PAINTCOLRGLYPH_CYCLE},
        *,
    };
    use crate::{model::glyph::outline::ControlBoundsSink, FontRef, TableProvider};
    use alloc::vec::Vec;

    fn font(data: &[u8]) -> Font {
        Font::new(data.to_vec(), 0).unwrap()
    }

    fn map(font: &Font, ch: char) -> GlyphId {
        font.charmap().map_unicode(ch).unwrap()
    }

    #[derive(Default)]
    struct DummySink;

    impl PaintSink for DummySink {
        fn push_transform(&mut self, _transform: Matrix<f32>) {}
        fn pop_transform(&mut self) {}
        fn push_clip_glyph(&mut self, _glyph: GlyphId) {}
        fn push_clip_box(&mut self, _clip_box: BoundingBox<f32>) {}
        fn pop_clip(&mut self) {}
        fn fill(&mut self, _brush: Brush) {}
        fn push_layer(&mut self, _composite_mode: CompositeMode) {}
        fn pop_layer(&mut self) {}
    }

    #[test]
    fn has_colrv1_glyph() {
        let font = font(font_test_data::COLRV0V1_VARIABLE);
        let glyphs = font.color_glyphs();
        assert_eq!(glyphs.version(), Some(1));
        let v1 = |ch| glyphs.get_with_version(map(&font, ch), ColorGlyphVersion::ColrV1);
        assert!(v1(COLORED_CIRCLES_V0[0]).is_none());
        assert!(v1(COLORED_CIRCLES_V1[0]).is_some());
        assert_eq!(
            glyphs
                .get(map(&font, COLORED_CIRCLES_V0[0]))
                .unwrap()
                .version(),
            ColorGlyphVersion::ColrV0
        );
    }

    #[test]
    fn a_font_without_colr_has_no_color_glyphs() {
        let font = font(font_test_data::VAZIRMATN_VAR);
        let glyphs = font.color_glyphs();
        assert!(glyphs.is_empty());
        assert_eq!(glyphs.version(), None);
        assert!(glyphs.get(GlyphId::new(1)).is_none());
        assert!(font.palettes().is_empty());
    }

    #[test]
    fn paintcolrglyph_cycle_fails() {
        let font = font(font_test_data::COLRV0V1_VARIABLE);
        let glyph = font
            .color_glyphs()
            .get_with_version(
                map(&font, PAINTCOLRGLYPH_CYCLE[0]),
                ColorGlyphVersion::ColrV1,
            )
            .unwrap();
        assert_eq!(glyph.paint(&mut DummySink), Err(Error::LimitExceeded));
    }

    #[test]
    fn paint_glyph2_preserves_boundary_glyph_ids() {
        use font_test_data::bebuffer::BeBuffer;

        #[derive(Default)]
        struct Sink {
            glyphs: Vec<GlyphId>,
            fills: Vec<(u16, f32)>,
            clip_depth: usize,
        }
        impl PaintSink for Sink {
            fn push_transform(&mut self, _: Matrix<f32>) {
                panic!("unexpected transform");
            }
            fn pop_transform(&mut self) {
                panic!("unexpected transform");
            }
            fn push_clip_glyph(&mut self, glyph: GlyphId) {
                self.glyphs.push(glyph);
                self.clip_depth += 1;
            }
            fn push_clip_box(&mut self, _: BoundingBox<f32>) {
                panic!("unexpected clip box");
            }
            fn pop_clip(&mut self) {
                self.clip_depth = self.clip_depth.checked_sub(1).unwrap();
            }
            fn fill(&mut self, brush: Brush) {
                assert_eq!(self.clip_depth, 1);
                match brush {
                    Brush::Solid {
                        palette_index,
                        alpha,
                    } => self.fills.push((palette_index, alpha)),
                    _ => panic!("unexpected brush"),
                }
            }
            fn push_layer(&mut self, _: CompositeMode) {
                panic!("unexpected layer");
            }
            fn pop_layer(&mut self) {
                panic!("unexpected layer");
            }
        }

        for gid in [65535u32, 65536, 0xffffff] {
            let mut colr = font_test_data::colr::paint_glyph2_colr().to_vec();
            let paint_offset = 34 + 10;
            colr[paint_offset + 4..paint_offset + 7].copy_from_slice(&gid.to_be_bytes()[1..]);
            let bytes = BeBuffer::new()
                .push(0x00010000u32)
                .push(1u16)
                .push(16u16)
                .push(0u16)
                .push(0u16)
                .extend(*b"COLR")
                .push(0u32)
                .push(28u32)
                .push(colr.len() as u32)
                .extend(colr.iter().copied());
            let font = font(&bytes);
            let colors = font.color_glyphs();
            let glyph = colors
                .get_with_version(GlyphId::new(42), ColorGlyphVersion::ColrV1)
                .unwrap();
            assert!(colors
                .get_with_version(GlyphId::new(43), ColorGlyphVersion::ColrV1)
                .is_none());
            let mut sink = Sink::default();
            for _ in 0..2 {
                glyph.paint(&mut sink).unwrap();
                assert_eq!(sink.clip_depth, 0);
            }
            assert_eq!(sink.glyphs, [GlyphId::new(gid); 2]);
            assert_eq!(sink.fills, [(7, 1.0); 2]);
        }
    }

    #[test]
    fn no_cliplist_paints() {
        let font = font(font_test_data::COLRV1_NO_CLIPLIST);
        let glyph = font
            .color_glyphs()
            .get_with_version(GlyphId::new(1), ColorGlyphVersion::ColrV1)
            .unwrap();
        assert!(glyph.clip_box().is_none());
        assert!(glyph.paint(&mut DummySink).is_ok());
    }

    #[test]
    fn colrv0_glyph_has_no_clip_box() {
        let font = font(font_test_data::COLRV0V1);
        let glyph = font
            .color_glyphs()
            .get_with_version(GlyphId::new(168), ColorGlyphVersion::ColrV0)
            .unwrap();
        assert!(glyph.clip_box().is_none());
    }

    #[test]
    fn extents_are_the_clip_box_where_there_is_one() {
        let font = font(font_test_data::COLRV0V1_VARIABLE);
        let glyphs = font.color_glyphs();
        let mut checked = 0;
        for ch in super::test_glyph_defs::CLIPBOX {
            let glyph = glyphs.get(map(&font, *ch)).unwrap();
            if let Some(clip_box) = glyph.clip_box() {
                assert_eq!(glyph.extents().unwrap(), GlyphExtents::from(clip_box));
                checked += 1;
            }
        }
        assert!(checked > 0);
    }

    #[test]
    fn extents_of_layered_outlines_are_their_union() {
        let font = font(font_test_data::COLRV0V1_VARIABLE);
        let gid = map(&font, COLORED_CIRCLES_V0[0]);
        let glyph = font.color_glyphs().get(gid).unwrap();
        assert_eq!(glyph.version(), ColorGlyphVersion::ColrV0);
        // The same union, taken by hand from the layers' outlines.
        let colr = FontRef::new(font_test_data::COLRV0V1_VARIABLE)
            .unwrap()
            .colr()
            .unwrap();
        let outlines = font.outline_glyphs();
        let mut pen = ControlBoundsSink::new();
        for layer in colr.v0_base_glyph(gid).unwrap() {
            let (layer_glyph, _) = colr.v0_layer(layer).unwrap();
            let outline = outlines.get(layer_glyph.into()).unwrap();
            outline
                .draw(
                    &crate::model::glyph::outline::DrawOptions::default(),
                    crate::model::glyph::outline::DrawMemory::Scratch,
                    &mut pen,
                )
                .unwrap();
        }
        assert_eq!(
            glyph.extents().unwrap(),
            GlyphExtents::from(pen.bounding_box().unwrap())
        );
    }

    #[test]
    fn extents_follow_a_translation() {
        let font = font(font_test_data::COLRV0V1_VARIABLE);
        let glyphs = font.color_glyphs();
        // Each translate test glyph composites one fill over a copy of itself
        // moved by these offsets, so its box is the union of the two.
        let offsets = [
            (0.0, 0.0),
            (0.0, 100.0),
            (0.0, -100.0),
            (100.0, 0.0),
            (-100.0, 0.0),
            (200.0, 200.0),
            (-200.0, -200.0),
        ];
        let extents: Vec<_> = super::test_glyph_defs::PAINT_TRANSLATE
            .iter()
            .map(|ch| glyphs.get(map(&font, *ch)).unwrap().extents().unwrap())
            .collect();
        assert_eq!(extents.len(), offsets.len());
        let base = extents[0];
        for (extents, (dx, dy)) in extents.iter().zip(offsets) {
            let expected = GlyphExtents {
                x_bearing: base.x_bearing.min(base.x_bearing + dx),
                y_bearing: base.y_bearing.max(base.y_bearing + dy),
                width: base.width + dx.abs(),
                height: base.height + dy.abs(),
            };
            assert_eq!(*extents, expected);
        }
    }

    #[test]
    fn version_0_glyphs_have_extents() {
        let font = font(font_test_data::COLRV0V1_VARIABLE);
        let glyphs = font.color_glyphs();
        let outlines = font.outline_glyphs();
        let gid = map(&font, COLORED_CIRCLES_V0[0]);
        assert!(glyphs.extents(gid, outlines).is_some());
        // A glyph the table does not paint has no extents.
        assert!(glyphs.extents(GlyphId::new(0), outlines).is_none());
    }

    #[test]
    fn palettes() {
        let font = font(font_test_data::COLRV0V1);
        let palettes = font.palettes();
        assert_eq!(palettes.len(), 3);
        let indices: Vec<_> = palettes.iter().map(|palette| palette.index()).collect();
        assert_eq!(indices, [0, 1, 2]);

        let first = palettes.get(0).unwrap();
        assert_eq!(first.colors().len(), 14);
        assert_eq!(
            first.colors().first(),
            Some(&Color {
                blue: 0,
                green: 0,
                red: 255,
                alpha: 255
            })
        );
        assert_eq!(first.colors().get(14), None);
        assert_eq!(first.palette_type(), Some(PaletteType::empty()));
        assert_eq!(first.index(), 0);

        let second = palettes.get(1).unwrap();
        assert_eq!(
            second.colors().first(),
            Some(&Color {
                blue: 74,
                green: 41,
                red: 42,
                alpha: 255
            })
        );
        assert_eq!(
            second.palette_type(),
            Some(PaletteType::USABLE_WITH_DARK_BACKGROUND)
        );

        let third = palettes.get(2).unwrap();
        assert_eq!(
            third.colors().first(),
            Some(&Color {
                blue: 24,
                green: 113,
                red: 252,
                alpha: 255
            })
        );
        assert_eq!(
            third.palette_type(),
            Some(PaletteType::USABLE_WITH_LIGHT_BACKGROUND)
        );

        assert!(palettes.get(3).is_none());
    }

    /// The Ecuador and El Salvador flags in Noto Color Emoji have the most
    /// complex paint graphs, visiting about 6700 paints. Painting them checks
    /// that the traversal budget is large enough.
    #[test]
    fn paint_flags_requiring_increased_node_budget() {
        let font = font(font_test_data::NOTO_COLOR_EMOJI_FLAGS);
        let colr = FontRef::new(font_test_data::NOTO_COLOR_EMOJI_FLAGS)
            .unwrap()
            .colr()
            .unwrap();
        let glyph_ids: Vec<_> = colr
            .base_glyph_list()
            .unwrap()
            .unwrap()
            .base_glyph_paint_records()
            .iter()
            .map(|record| GlyphId::from(record.glyph_id()))
            .collect();
        assert!(!glyph_ids.is_empty());
        for gid in glyph_ids {
            font.color_glyphs()
                .get_with_version(gid, ColorGlyphVersion::ColrV1)
                .unwrap_or_else(|| panic!("no COLRv1 glyph for {gid}"))
                .paint(&mut DummySink)
                .unwrap_or_else(|e| panic!("failed to paint {gid}: {e}"));
        }
    }
}
