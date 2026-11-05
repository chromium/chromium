//! Traversal of a `COLR` paint graph into a [`PaintSink`].

use super::{
    decycler::{Decycler, DecyclerError},
    instance::{resolve_clip_box, ColrInstance, MaybeBrush, ResolvedColorStop, ResolvedPaint},
    Brush, ColorStop, Error, PaintCachedColorGlyph, PaintSink,
};
use crate::{
    limits::MAX_RECURSION_DEPTH,
    tables::colr::{CompositeMode, Paint},
    types::{BoundingBox, GlyphId},
};
use core::ops::Range;

#[cfg(feature = "libm")]
#[allow(unused_imports)]
use core_maths::CoreFloat;

// Bounds how deep a paint graph is followed, and detects cycles in it.
// HarfBuzz bounds its own paint context the same way; see
// <https://github.com/harfbuzz/harfbuzz/blob/724ef405f3a0a0b2792c7a575d1f3341b41ca950/src/OT/Color/COLR/COLR.hh#L74>.
pub(super) type PaintDecycler = Decycler<usize, MAX_RECURSION_DEPTH>;

// Avoid heap allocations for any gradient with <= 32 color stops. This number
// was chosen to keep stack size < 512 bytes.
//
// The largest gradient in Noto Color Emoji has 13 stops.
//
// Only one ColorStopVec will be created per paint graph traversal.
const MAX_INLINE_COLOR_STOPS: usize = 32;

pub(super) type ColorStopVec = smallvec::SmallVec<[ColorStop; MAX_INLINE_COLOR_STOPS]>;

impl From<DecyclerError> for Error {
    // A cycle never ends, so it exceeds the nesting limit like a graph that
    // is merely too deep.
    fn from(_: DecyclerError) -> Self {
        Self::LimitExceeded
    }
}

/// Maximum number of nodes visited during a single traversal.
///
/// Prevents excessive execution time on graphs with high fan-out. This is the
/// 16384 cap recommended by security analysis. It is above HarfBuzz's limit
/// of 2048
/// (<https://github.com/harfbuzz/harfbuzz/blob/9f2f03173b7fee860cc00d999857d09fa4a362e2/src/hb-limits.hh#L96>)
/// because some glyphs, such as the flags in Noto Color Emoji, visit about
/// 6700 nodes.
const MAX_NODES: u32 = 16384;

pub(super) fn get_clipbox_font_units(
    colr_instance: &ColrInstance,
    glyph: GlyphId,
) -> Option<BoundingBox<f32>> {
    let maybe_clipbox = (*colr_instance).v1_clip_box(glyph)?;
    Some(resolve_clip_box(colr_instance, &maybe_clipbox))
}

impl From<ResolvedColorStop> for ColorStop {
    fn from(resolved_stop: ResolvedColorStop) -> Self {
        ColorStop {
            offset: resolved_stop.offset,
            alpha: resolved_stop.alpha,
            palette_index: resolved_stop.palette_index,
        }
    }
}

pub(super) struct TraversalState<'a> {
    instance: ColrInstance<'a>,
    sink: &'a mut dyn PaintSink,
    stops_buf: ColorStopVec,
    nodes_left: u32,
}

impl<'a> TraversalState<'a> {
    pub(super) fn new(instance: ColrInstance<'a>, sink: &'a mut dyn PaintSink) -> Self {
        Self {
            instance,
            sink,
            stops_buf: ColorStopVec::new(),
            nodes_left: MAX_NODES,
        }
    }

    pub(super) fn resolve_paint(&mut self, paint: &Paint<'a>) -> Result<ResolvedPaint<'a>, Error> {
        self.nodes_left = self.nodes_left.checked_sub(1).ok_or(Error::LimitExceeded)?;
        super::instance::resolve_paint(&self.instance, paint)
    }
}

pub(super) fn traverse_with_callbacks<'a>(
    paint: &ResolvedPaint<'a>,
    state: &mut TraversalState<'a>,
    decycler: &mut PaintDecycler,
    recurse_depth: usize,
) -> Result<(), Error> {
    if recurse_depth >= MAX_RECURSION_DEPTH {
        return Err(Error::LimitExceeded);
    }
    match paint {
        ResolvedPaint::ColrLayers { range } => {
            for layer_index in range.clone() {
                // Perform cycle detection with paint id here, second part of the tuple.
                let (layer_paint, paint_id) = state
                    .instance
                    .v1_layer(layer_index)
                    .ok_or(Error::Malformed)?;
                let mut cycle_guard = decycler.enter(paint_id)?;
                traverse_with_callbacks(
                    &state.resolve_paint(&layer_paint)?,
                    state,
                    &mut cycle_guard,
                    recurse_depth + 1,
                )?;
            }
            Ok(())
        }
        ResolvedPaint::Solid { .. }
        | ResolvedPaint::LinearGradient { .. }
        | ResolvedPaint::RadialGradient { .. }
        | ResolvedPaint::SweepGradient { .. } => {
            if let MaybeBrush::Some(brush) =
                paint.as_brush(&state.instance, &mut state.stops_buf)?
            {
                state.sink.fill(brush);
            }
            Ok(())
        }
        ResolvedPaint::Glyph { glyph, paint } => {
            let glyph = *glyph;
            // Look for the pattern `(transform)* fill` and optimize it to a
            // single paint call
            let mut next_paint = state.resolve_paint(paint)?;
            // Collect any chain of intermediate transforms
            let mut intermediate_transform = None;
            while let Some((transform, child_paint)) = next_paint.as_transform() {
                intermediate_transform = Some(match intermediate_transform {
                    Some(existing_transform) => existing_transform * transform,
                    None => transform,
                });
                next_paint = state.resolve_paint(&child_paint)?;
            }
            // If the next paint is a brush, we can optimize the traversal to a
            // single fill_glyph call
            match next_paint.as_brush(&state.instance, &mut state.stops_buf)? {
                MaybeBrush::Some(brush) => {
                    state.sink.fill_glyph(glyph, intermediate_transform, brush);
                    return Ok(());
                }
                // Valid brush but doesn't produce any rendering, so we can
                // skip the glyph entirely
                MaybeBrush::NonRendering => {
                    return Ok(());
                }
                // Not a brush, fall through to the unoptimized path
                MaybeBrush::None => {}
            }
            // In case the optimization was not successful, just push a clip,
            // and continue unoptimized traversal.
            state.sink.push_clip_glyph(glyph);
            if let Some(transform) = intermediate_transform {
                state.sink.push_transform(transform);
            }
            let result = traverse_with_callbacks(&next_paint, state, decycler, recurse_depth + 1);
            if intermediate_transform.is_some() {
                state.sink.pop_transform();
            }
            state.sink.pop_clip();
            result
        }
        ResolvedPaint::ColrGlyph { glyph } => {
            let glyph = (*glyph).into();
            match state.instance.v1_base_glyph(glyph) {
                Some((base_glyph, base_glyph_paint_id)) => {
                    let mut cycle_guard = decycler.enter(base_glyph_paint_id)?;
                    let draw_result = state.sink.paint_cached_color_glyph(glyph)?;
                    match draw_result {
                        PaintCachedColorGlyph::Ok => Ok(()),
                        PaintCachedColorGlyph::Unimplemented => {
                            let clipbox = get_clipbox_font_units(&state.instance, glyph);

                            if let Some(rect) = clipbox {
                                state.sink.push_clip_box(rect);
                            }
                            let result = traverse_unresolved_paint(
                                &base_glyph,
                                state,
                                &mut cycle_guard,
                                recurse_depth + 1,
                            );
                            if clipbox.is_some() {
                                state.sink.pop_clip();
                            }
                            result
                        }
                    }
                }
                None => Err(Error::MissingResource),
            }
        }
        ResolvedPaint::Transform {
            paint: next_paint, ..
        }
        | ResolvedPaint::Translate {
            paint: next_paint, ..
        }
        | ResolvedPaint::Scale {
            paint: next_paint, ..
        }
        | ResolvedPaint::Rotate {
            paint: next_paint, ..
        }
        | ResolvedPaint::Skew {
            paint: next_paint, ..
        } => {
            state
                .sink
                .push_transform(paint.as_transform().ok_or(Error::Malformed)?.0);
            let result = traverse_unresolved_paint(next_paint, state, decycler, recurse_depth + 1);
            state.sink.pop_transform();
            result
        }
        ResolvedPaint::Composite {
            source_paint,
            mode,
            backdrop_paint,
        } => {
            state.sink.push_layer(CompositeMode::SrcOver);
            let mut result =
                traverse_unresolved_paint(backdrop_paint, state, decycler, recurse_depth + 1);
            if result.is_err() {
                state.sink.pop_layer_with_mode(CompositeMode::SrcOver);
                return result;
            }
            state.sink.push_layer(*mode);
            result = traverse_unresolved_paint(source_paint, state, decycler, recurse_depth + 1);
            state.sink.pop_layer_with_mode(*mode);
            state.sink.pop_layer_with_mode(CompositeMode::SrcOver);
            result
        }
    }
}

fn traverse_unresolved_paint<'a>(
    paint: &Paint<'a>,
    state: &mut TraversalState<'a>,
    decycler: &mut PaintDecycler,
    recurse_depth: usize,
) -> Result<(), Error> {
    let resolved_paint = state.resolve_paint(paint)?;
    traverse_with_callbacks(&resolved_paint, state, decycler, recurse_depth)
}

pub(super) fn traverse_v0_range(
    range: &Range<usize>,
    instance: &ColrInstance,
    sink: &mut dyn PaintSink,
) -> Result<(), Error> {
    for layer_index in range.clone() {
        let (layer_glyph, palette_index) =
            (*instance).v0_layer(layer_index).ok_or(Error::Malformed)?;
        sink.fill_glyph(
            layer_glyph.into(),
            None,
            Brush::Solid {
                palette_index,
                alpha: 1.0,
            },
        );
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{
        model::{
            font::ColrTables,
            glyph::color::{
                test_glyph_defs::CLIPBOX, Brush, ColorGlyphVersion, CompositeMode, Font, PaintSink,
            },
        },
        types::{GlyphId16, Matrix},
        FontData, FontRead, FontRef, TableProvider,
    };
    use alloc::vec::Vec;

    #[test]
    fn clipbox_test() {
        let font = FontRef::new(font_test_data::COLRV0V1_VARIABLE).unwrap();
        let model = Font::new(font_test_data::COLRV0V1_VARIABLE.to_vec(), 0).unwrap();
        let test_glyph_id = model.charmap().map_unicode(CLIPBOX[0]).unwrap();
        let upem = font.head().unwrap().units_per_em();

        let base_bounding_box = BoundingBox {
            x_min: 0.0,
            x_max: upem as f32 / 2.0,
            y_min: upem as f32 / 2.0,
            y_max: upem as f32,
        };
        // Fractional value needed to match variation scaling of clipbox.
        const CLIPBOX_SHIFT: f32 = 200.0122;

        macro_rules! test_entry {
            ($axis:literal, $shift:expr, $field:ident) => {
                (
                    $axis,
                    $shift,
                    BoundingBox {
                        $field: base_bounding_box.$field + ($shift),
                        ..base_bounding_box
                    },
                )
            };
        }

        let test_data_expectations = [
            ("", 0.0, base_bounding_box),
            test_entry!("CLXI", CLIPBOX_SHIFT, x_min),
            test_entry!("CLXA", -CLIPBOX_SHIFT, x_max),
            test_entry!("CLYI", CLIPBOX_SHIFT, y_min),
            test_entry!("CLYA", -CLIPBOX_SHIFT, y_max),
        ];

        let tables = ColrTables::new(font.colr().unwrap());
        for axis_test in test_data_expectations {
            let instance = model
                .instance_builder()
                .variations([(axis_test.0, axis_test.1)])
                .build();
            let color_instance = ColrInstance::new(&tables, instance.normalized_coords(), &[]);
            let clip_box = get_clipbox_font_units(&color_instance, test_glyph_id);
            assert!(clip_box.is_some());
            assert!(
                clip_box.unwrap() == axis_test.2,
                "Clip boxes do not match. Actual: {:?}, expected: {:?}",
                clip_box.unwrap(),
                axis_test.2
            );
        }
    }

    struct NopSink;

    impl PaintSink for NopSink {
        fn push_transform(&mut self, _transform: Matrix<f32>) {}

        fn pop_transform(&mut self) {}

        fn push_clip_glyph(&mut self, _glyph_id: GlyphId) {}

        fn push_clip_box(&mut self, _clip_box: BoundingBox<f32>) {}

        fn pop_clip(&mut self) {}

        fn fill(&mut self, _brush: Brush<'_>) {}

        fn push_layer(&mut self, _composite_mode: CompositeMode) {}

        fn pop_layer(&mut self) {}
    }

    #[derive(Default)]
    struct StackTrackingSink {
        transform_pushes: usize,
        transform_pops: usize,
        clip_pushes: usize,
        clip_pops: usize,
        layer_pushes: usize,
        layer_pops: usize,
        clip_glyphs: Vec<GlyphId>,
        filled_glyphs: Vec<GlyphId>,
        fills: usize,
    }

    impl PaintSink for StackTrackingSink {
        fn push_transform(&mut self, _transform: Matrix<f32>) {
            self.transform_pushes += 1;
        }

        fn pop_transform(&mut self) {
            self.transform_pops += 1;
        }

        fn push_clip_glyph(&mut self, glyph: GlyphId) {
            self.clip_pushes += 1;
            self.clip_glyphs.push(glyph);
        }

        fn push_clip_box(&mut self, _clip_box: BoundingBox<f32>) {
            self.clip_pushes += 1;
        }

        fn pop_clip(&mut self) {
            self.clip_pops += 1;
        }

        fn fill(&mut self, _brush: Brush<'_>) {
            self.fills += 1;
        }

        fn fill_glyph(
            &mut self,
            glyph: GlyphId,
            _brush_transform: Option<Matrix<f32>>,
            _brush: Brush<'_>,
        ) {
            self.filled_glyphs.push(glyph);
            self.fills += 1;
        }

        fn push_layer(&mut self, _composite_mode: CompositeMode) {
            self.layer_pushes += 1;
        }

        fn pop_layer(&mut self) {
            self.layer_pops += 1;
        }
    }

    fn colr_tables(font: &FontRef<'static>) -> ColrTables<'static> {
        ColrTables::new(font.colr().unwrap())
    }

    #[test]
    fn transform_error_unwinds_transform_stack() {
        let font = FontRef::new(font_test_data::COLRV0V1_VARIABLE).unwrap();
        let tables = colr_tables(&font);
        let instance = ColrInstance::new(&tables, &[], &[]);
        let model = Font::new(font_test_data::COLRV0V1_VARIABLE.to_vec(), 0).unwrap();
        let glyph = model.charmap().map_unicode(CLIPBOX[0]).unwrap();
        let (paint, _) = instance.v1_base_glyph(glyph).unwrap();
        let mut sink = StackTrackingSink::default();
        let mut state = TraversalState::new(instance, &mut sink);
        let mut decycler = PaintDecycler::new();
        // Exhaust the node budget so the traversal fails inside the transform.
        state.nodes_left = 1;
        let paint = ResolvedPaint::Translate {
            dx: 0.0,
            dy: 0.0,
            paint,
        };
        let result = traverse_with_callbacks(&paint, &mut state, &mut decycler, 0);
        assert!(matches!(result, Err(Error::LimitExceeded)));
        assert_eq!(sink.transform_pushes, sink.transform_pops);
        assert_ne!(sink.transform_pushes, 0);
    }

    #[test]
    fn composite_error_unwinds_layer_stack() {
        let font = FontRef::new(font_test_data::COLRV0V1_VARIABLE).unwrap();
        let tables = colr_tables(&font);
        let instance = ColrInstance::new(&tables, &[], &[]);
        let model = Font::new(font_test_data::COLRV0V1_VARIABLE.to_vec(), 0).unwrap();
        let glyph = model.charmap().map_unicode(CLIPBOX[0]).unwrap();
        let (inner_paint, _) = instance.v1_base_glyph(glyph).unwrap();
        let mut sink = StackTrackingSink::default();
        let mut state = TraversalState::new(instance, &mut sink);
        let mut decycler = PaintDecycler::new();
        state.nodes_left = 0;
        let paint = ResolvedPaint::Composite {
            source_paint: inner_paint.clone(),
            mode: CompositeMode::SrcOver,
            backdrop_paint: inner_paint,
        };
        let result = traverse_with_callbacks(&paint, &mut state, &mut decycler, 0);
        assert!(matches!(result, Err(Error::LimitExceeded)));
        assert_eq!(sink.layer_pushes, sink.layer_pops);
        assert_ne!(sink.layer_pushes, 0);
    }

    #[test]
    fn clipbox_error_unwinds_clip_stack() {
        let font = FontRef::new(font_test_data::COLRV0V1_VARIABLE).unwrap();
        let tables = colr_tables(&font);
        let instance = ColrInstance::new(&tables, &[], &[]);
        let model = Font::new(font_test_data::COLRV0V1_VARIABLE.to_vec(), 0).unwrap();
        let glyph = model.charmap().map_unicode(CLIPBOX[0]).unwrap();
        let mut sink = StackTrackingSink::default();
        let mut state = TraversalState::new(instance, &mut sink);
        let mut decycler = PaintDecycler::new();
        state.nodes_left = 0;
        let paint = ResolvedPaint::ColrGlyph {
            glyph: GlyphId16::new(glyph.to_u32() as u16),
        };
        let result = traverse_with_callbacks(&paint, &mut state, &mut decycler, 0);
        assert!(matches!(result, Err(Error::LimitExceeded)));
        assert_eq!(sink.clip_pushes, sink.clip_pops);
        assert_ne!(sink.clip_pushes, 0);
    }

    #[test]
    fn no_panic_on_empty_colorline() {
        // Minimized test case from <https://issues.oss-fuzz.com/issues/375768991>.
        let test_case: &[u8] = &[
            0, 1, 0, 0, 0, 3, 32, 32, 32, 32, 32, 32, 0, 32, 32, 32, 32, 32, 32, 32, 255, 32, 32,
            32, 32, 32, 32, 32, 67, 79, 76, 82, 32, 32, 32, 32, 0, 0, 0, 229, 0, 0, 0, 178, 99,
            109, 97, 112, 32, 32, 32, 32, 0, 0, 0, 10, 0, 0, 1, 32, 32, 32, 32, 255, 32, 32, 32, 0,
            4, 32, 255, 32, 32, 0, 32, 32, 32, 32, 32, 32, 32, 255, 32, 32, 32, 32, 32, 32, 32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 255, 32, 0, 0,
            32, 32, 0, 0, 0, 57, 32, 32, 32, 32, 32, 32, 32, 255, 32, 32, 32, 32, 32, 32, 32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
            32, 0, 0, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 32, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            255, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 0, 0, 0, 4, 32, 32, 32, 32, 32, 32, 32,
            32, 32, 0, 0, 0, 1, 32, 32, 32, 32, 32, 32, 255, 0, 0, 0, 40, 32, 32, 32, 32, 32, 32,
            32, 255, 255, 32, 32, 32, 4, 0, 0, 32, 32, 32, 32, 32, 0, 0, 0, 0, 0, 0, 0, 0, 32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 0, 0, 32, 32, 32, 255, 255,
            255, 255, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            255, 255, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
            255, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
        ];

        let font = FontRef::new(test_case).unwrap();
        font.cmap().unwrap();
        font.colr().unwrap();

        let model = Font::new(test_case.to_vec(), 0).unwrap();
        let color_glyph = model
            .color_glyphs()
            .get_with_version(GlyphId::new(8447), ColorGlyphVersion::ColrV1)
            .unwrap();
        let _ = color_glyph.paint(&mut NopSink);
    }

    #[test]
    fn visited_nodes_limit() {
        let font = FontRef::new(font_test_data::COLRV0V1_VARIABLE).unwrap();
        let tables = colr_tables(&font);
        let gid = GlyphId::new(120);
        let mut sink = NopSink;
        let mut traverse_with_limit = |limit| {
            let instance = ColrInstance::new(&tables, &[], &[]);
            let mut state = TraversalState::new(instance, &mut sink);
            let mut decycler = PaintDecycler::new();
            state.nodes_left = limit;
            let paint = state
                .resolve_paint(&state.instance.v1_base_glyph(gid).unwrap().0)
                .unwrap();
            traverse_with_callbacks(&paint, &mut state, &mut decycler, 0)
                .map(|_| limit - state.nodes_left)
        };
        // Compute the actual number of nodes used by the glyph
        let node_count = traverse_with_limit(MAX_NODES).unwrap();
        // Now run with a reduced limit and verify that we get an error
        let result = traverse_with_limit(node_count - 1);
        assert!(matches!(result, Err(Error::LimitExceeded)));
    }

    #[test]
    fn a_minimal_colr_reads_without_variation_tables() {
        // A version 0 table with no records.
        let bytes = [0u8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
        let colr = crate::tables::colr::Colr::read(FontData::new(&bytes)).unwrap();
        let tables = ColrTables::new(colr);
        assert!(tables.index_map.is_none());
        assert!(tables.var_store.is_none());
    }
}
