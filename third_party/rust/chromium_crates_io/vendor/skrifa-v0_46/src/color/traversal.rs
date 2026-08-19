use std::ops::Range;

use raw::{tables::colr::Paint, ReadError};
use read_fonts::{
    tables::colr::CompositeMode,
    types::{BoundingBox, GlyphId},
};

use super::{
    instance::{resolve_clip_box, ColrInstance, MaybeBrush, ResolvedColorStop, ResolvedPaint},
    Brush, ColorPainter, ColorStop, PaintCachedColorGlyph, PaintError,
};

use crate::decycler::{Decycler, DecyclerError};

#[cfg(feature = "libm")]
#[allow(unused_imports)]
use core_maths::*;

pub(crate) type PaintDecycler = Decycler<usize, MAX_TRAVERSAL_DEPTH>;

// Avoid heap allocations for any gradient with <= 32 color stops. This number
// was chosen to keep stack size < 512 bytes.
//
// The largest gradient in Noto Color Emoji has 13 stops.
//
// Only one ColorStopVec will be created per paint graph traversal.
//
// Usage of SmallVec as a response to Behdad's wonderful memory usage analysis:
// <https://docs.google.com/document/d/1S47f3E--yqvFdG7lmmufxRoFi_wMzotC03v8UvS_p54/edit?tab=t.0#heading=h.bfj7urloz3oe>
const MAX_INLINE_COLOR_STOPS: usize = 32;

pub(crate) type ColorStopVec = crate::collections::SmallVec<ColorStop, MAX_INLINE_COLOR_STOPS>;

impl From<DecyclerError> for PaintError {
    fn from(value: DecyclerError) -> Self {
        match value {
            DecyclerError::CycleDetected => Self::PaintCycleDetected,
            DecyclerError::DepthLimitExceeded => Self::DepthLimitExceeded,
        }
    }
}

/// Depth at which we will stop traversing and return an error.
///
/// Used to prevent stack overflows. Also allows us to avoid using a HashSet
/// in no_std builds.
///
/// This limit matches the one used in HarfBuzz:
/// HB_MAX_NESTING_LEVEL: <https://github.com/harfbuzz/harfbuzz/blob/c2f8f35a6cfce43b88552b3eb5c05062ac7007b2/src/hb-limits.hh#L53>
/// hb_paint_context_t: <https://github.com/harfbuzz/harfbuzz/blob/c2f8f35a6cfce43b88552b3eb5c05062ac7007b2/src/OT/Color/COLR/COLR.hh#L74>
const MAX_TRAVERSAL_DEPTH: usize = 64;

/// Maximum number of nodes visited during a single traversal.
///
/// Prevents excessive execution time on graphs with high fan-out. Set to 4096
/// as a middle ground between HarfBuzz's limit of 2048 (<https://github.com/harfbuzz/harfbuzz/blob/9f2f03173b7fee860cc00d999857d09fa4a362e2/src/hb-limits.hh#L96>)
/// and the 16384 cap recommended by security analysis.
const MAX_NODES: u32 = 4096;

pub(crate) fn get_clipbox_font_units(
    colr_instance: &ColrInstance,
    glyph_id: GlyphId,
) -> Option<BoundingBox<f32>> {
    let maybe_clipbox = (*colr_instance).v1_clip_box(glyph_id).ok().flatten()?;
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

pub(crate) struct TraversalState<'a, P: ColorPainter> {
    instance: ColrInstance<'a>,
    painter: &'a mut P,
    stops_buf: ColorStopVec,
    nodes_left: u32,
}

impl<'a, P: ColorPainter> TraversalState<'a, P> {
    pub(crate) fn new(instance: ColrInstance<'a>, painter: &'a mut P) -> Self {
        Self {
            instance,
            painter,
            stops_buf: ColorStopVec::new(),
            nodes_left: MAX_NODES,
        }
    }

    pub(crate) fn resolve_paint(
        &mut self,
        paint: &Paint<'a>,
    ) -> Result<ResolvedPaint<'a>, PaintError> {
        self.nodes_left = self
            .nodes_left
            .checked_sub(1)
            .ok_or(PaintError::DepthLimitExceeded)?;
        Ok(super::instance::resolve_paint(&self.instance, paint)?)
    }
}

pub(crate) fn traverse_with_callbacks<'a, P: ColorPainter>(
    paint: &ResolvedPaint<'a>,
    state: &mut TraversalState<'a, P>,
    decycler: &mut PaintDecycler,
    recurse_depth: usize,
) -> Result<(), PaintError> {
    if recurse_depth >= MAX_TRAVERSAL_DEPTH {
        return Err(PaintError::DepthLimitExceeded);
    }
    match paint {
        ResolvedPaint::ColrLayers { range } => {
            for layer_index in range.clone() {
                // Perform cycle detection with paint id here, second part of the tuple.
                let (layer_paint, paint_id) = state.instance.v1_layer(layer_index)?;
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
                state.painter.fill(brush);
            }
            Ok(())
        }
        ResolvedPaint::Glyph { glyph_id, paint } => {
            let glyph_id = (*glyph_id).into();
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
                    state
                        .painter
                        .fill_glyph(glyph_id, intermediate_transform, brush);
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
            state.painter.push_clip_glyph(glyph_id);
            if let Some(transform) = intermediate_transform {
                state.painter.push_transform(transform);
            }
            let result = traverse_with_callbacks(&next_paint, state, decycler, recurse_depth + 1);
            if intermediate_transform.is_some() {
                state.painter.pop_transform();
            }
            state.painter.pop_clip();
            result
        }
        ResolvedPaint::ColrGlyph { glyph_id } => {
            let glyph_id = (*glyph_id).into();
            match state.instance.v1_base_glyph(glyph_id)? {
                Some((base_glyph, base_glyph_paint_id)) => {
                    let mut cycle_guard = decycler.enter(base_glyph_paint_id)?;
                    let draw_result = state.painter.paint_cached_color_glyph(glyph_id)?;
                    match draw_result {
                        PaintCachedColorGlyph::Ok => Ok(()),
                        PaintCachedColorGlyph::Unimplemented => {
                            let clipbox = get_clipbox_font_units(&state.instance, glyph_id);

                            if let Some(rect) = clipbox {
                                state.painter.push_clip_box(rect);
                            }
                            let result = traverse_unresolved_paint(
                                &base_glyph,
                                state,
                                &mut cycle_guard,
                                recurse_depth + 1,
                            );
                            if clipbox.is_some() {
                                state.painter.pop_clip();
                            }
                            result
                        }
                    }
                }
                None => Err(PaintError::GlyphNotFound(glyph_id)),
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
            state.painter.push_transform(
                paint
                    .as_transform()
                    .ok_or(ReadError::MalformedData("expected a transform paint"))?
                    .0,
            );
            let result = traverse_unresolved_paint(next_paint, state, decycler, recurse_depth + 1);
            state.painter.pop_transform();
            result
        }
        ResolvedPaint::Composite {
            source_paint,
            mode,
            backdrop_paint,
        } => {
            state.painter.push_layer(CompositeMode::SrcOver);
            let mut result =
                traverse_unresolved_paint(backdrop_paint, state, decycler, recurse_depth + 1);
            if result.is_err() {
                state.painter.pop_layer_with_mode(CompositeMode::SrcOver);
                return result;
            }
            state.painter.push_layer(*mode);
            result = traverse_unresolved_paint(source_paint, state, decycler, recurse_depth + 1);
            state.painter.pop_layer_with_mode(*mode);
            state.painter.pop_layer_with_mode(CompositeMode::SrcOver);
            result
        }
    }
}

fn traverse_unresolved_paint<'a, P: ColorPainter>(
    paint: &Paint<'a>,
    state: &mut TraversalState<'a, P>,
    decycler: &mut PaintDecycler,
    recurse_depth: usize,
) -> Result<(), PaintError> {
    let resolved_paint = state.resolve_paint(paint)?;
    traverse_with_callbacks(&resolved_paint, state, decycler, recurse_depth)
}

pub(crate) fn traverse_v0_range(
    range: &Range<usize>,
    instance: &ColrInstance,
    painter: &mut impl ColorPainter,
) -> Result<(), PaintError> {
    for layer_index in range.clone() {
        let (layer_glyph, palette_index) = (*instance).v0_layer(layer_index)?;
        painter.fill_glyph(
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
        color::{
            instance::ColrInstance, traversal::get_clipbox_font_units,
            traversal_tests::test_glyph_defs::CLIPBOX, Brush, ColorGlyphFormat, ColorPainter,
            CompositeMode, Transform,
        },
        prelude::LocationRef,
        MetadataProvider,
    };
    use raw::types::GlyphId;
    use read_fonts::{
        types::{BoundingBox, GlyphId16},
        FontRef, TableProvider,
    };

    #[test]
    fn clipbox_test() {
        let colr_font = font_test_data::COLRV0V1_VARIABLE;
        let font = FontRef::new(colr_font).unwrap();
        let test_glyph_id = font.charmap().map(CLIPBOX[0]).unwrap();
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

        for axis_test in test_data_expectations {
            let axis_coordinate = (axis_test.0, axis_test.1);
            let location = font.axes().location([axis_coordinate]);
            let color_instance = ColrInstance::new(font.colr().unwrap(), location.coords());
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

    struct NopPainter;

    impl ColorPainter for NopPainter {
        fn push_transform(&mut self, _transform: Transform) {
            // nop
        }

        fn pop_transform(&mut self) {
            // nop
        }

        fn push_clip_glyph(&mut self, _glyph_id: GlyphId) {
            // nop
        }

        fn push_clip_box(&mut self, _clip_box: BoundingBox<f32>) {
            // nop
        }

        fn pop_clip(&mut self) {
            // nop
        }

        fn fill(&mut self, _brush: Brush<'_>) {
            // nop
        }

        fn push_layer(&mut self, _composite_mode: CompositeMode) {
            // nop
        }

        fn pop_layer(&mut self) {
            // nop
        }
    }

    #[derive(Default)]
    struct StackTrackingPainter {
        transform_pushes: usize,
        transform_pops: usize,
        clip_pushes: usize,
        clip_pops: usize,
        layer_pushes: usize,
        layer_pops: usize,
    }

    impl ColorPainter for StackTrackingPainter {
        fn push_transform(&mut self, _transform: Transform) {
            self.transform_pushes += 1;
        }

        fn pop_transform(&mut self) {
            self.transform_pops += 1;
        }

        fn push_clip_glyph(&mut self, _glyph_id: GlyphId) {
            self.clip_pushes += 1;
        }

        fn push_clip_box(&mut self, _clip_box: BoundingBox<f32>) {
            self.clip_pushes += 1;
        }

        fn pop_clip(&mut self) {
            self.clip_pops += 1;
        }

        fn fill(&mut self, _brush: Brush<'_>) {
            // nop
        }

        fn push_layer(&mut self, _composite_mode: CompositeMode) {
            self.layer_pushes += 1;
        }

        fn pop_layer(&mut self) {
            self.layer_pops += 1;
        }
    }

    #[test]
    fn transform_error_unwinds_transform_stack() {
        let colr_font = font_test_data::COLRV0V1_VARIABLE;
        let font = FontRef::new(colr_font).unwrap();
        let glyph_id = font.charmap().map(CLIPBOX[0]).unwrap();
        let mut painter = StackTrackingPainter::default();
        let instance = ColrInstance::new(font.colr().unwrap(), &[]);
        let inner_paint = instance.v1_base_glyph(glyph_id).unwrap().unwrap().0;
        let mut state = TraversalState::new(instance, &mut painter);
        let mut decycler = PaintDecycler::new();
        state.nodes_left = 0;
        let paint = ResolvedPaint::Transform {
            xx: 1.0,
            yx: 0.0,
            xy: 0.0,
            yy: 1.0,
            dx: 0.0,
            dy: 0.0,
            paint: inner_paint,
        };
        let result = traverse_with_callbacks(&paint, &mut state, &mut decycler, 0);
        assert!(matches!(result, Err(PaintError::DepthLimitExceeded)));
        assert_eq!(painter.transform_pushes, painter.transform_pops);
        assert_ne!(painter.transform_pushes, 0);
    }

    #[test]
    fn composite_error_unwinds_layer_stack() {
        let colr_font = font_test_data::COLRV0V1_VARIABLE;
        let font = FontRef::new(colr_font).unwrap();
        let glyph_id = font.charmap().map(CLIPBOX[0]).unwrap();
        let mut painter = StackTrackingPainter::default();
        let instance = ColrInstance::new(font.colr().unwrap(), &[]);
        let inner_paint = instance.v1_base_glyph(glyph_id).unwrap().unwrap().0;
        let mut state = TraversalState::new(instance, &mut painter);
        let mut decycler = PaintDecycler::new();
        state.nodes_left = 0;
        let paint = ResolvedPaint::Composite {
            source_paint: inner_paint.clone(),
            mode: CompositeMode::SrcOver,
            backdrop_paint: inner_paint,
        };
        let result = traverse_with_callbacks(&paint, &mut state, &mut decycler, 0);
        assert!(matches!(result, Err(PaintError::DepthLimitExceeded)));
        assert_eq!(painter.layer_pushes, painter.layer_pops);
        assert_ne!(painter.layer_pushes, 0);
    }

    #[test]
    fn clipbox_error_unwinds_clip_stack() {
        let colr_font = font_test_data::COLRV0V1_VARIABLE;
        let font = FontRef::new(colr_font).unwrap();
        let glyph_id = font.charmap().map(CLIPBOX[0]).unwrap();
        let mut painter = StackTrackingPainter::default();
        let instance = ColrInstance::new(font.colr().unwrap(), &[]);
        let mut state = TraversalState::new(instance, &mut painter);
        let mut decycler = PaintDecycler::new();
        state.nodes_left = 0;
        let paint = ResolvedPaint::ColrGlyph {
            glyph_id: GlyphId16::new(glyph_id.to_u32() as u16),
        };
        let result = traverse_with_callbacks(&paint, &mut state, &mut decycler, 0);
        assert!(matches!(result, Err(PaintError::DepthLimitExceeded)));
        assert_eq!(painter.clip_pushes, painter.clip_pops);
        assert_ne!(painter.clip_pushes, 0);
    }

    #[test]
    fn no_panic_on_empty_colorline() {
        // Minimized test case from <https://issues.oss-fuzz.com/issues/375768991>.
        let test_case = &[
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

        let color_glyph = font
            .color_glyphs()
            .get_with_format(GlyphId::new(8447), ColorGlyphFormat::ColrV1)
            .unwrap();
        let _ = color_glyph.paint(LocationRef::default(), &mut NopPainter);
    }

    #[test]
    fn visited_nodes_limit() {
        let colr_font = font_test_data::COLRV0V1_VARIABLE;
        let font = FontRef::new(colr_font).unwrap();
        let gid = GlyphId::new(120);
        let mut painter = NopPainter;
        let mut traverse_with_limit = |limit| {
            let instance = ColrInstance::new(font.colr().unwrap(), &[]);
            let mut state = TraversalState::new(instance, &mut painter);
            let mut decycler = PaintDecycler::new();
            state.nodes_left = limit;
            let paint = state
                .resolve_paint(&state.instance.v1_base_glyph(gid).unwrap().unwrap().0)
                .unwrap();
            traverse_with_callbacks(&paint, &mut state, &mut decycler, 0)
                .map(|_| limit - state.nodes_left)
        };
        // Compute the actual number of nodes used by the glyph
        let node_count = traverse_with_limit(MAX_NODES).unwrap();
        // Now run with a reduced limit and verify that we get an error
        let result = traverse_with_limit(node_count - 1);
        assert!(matches!(result, Err(PaintError::DepthLimitExceeded)));
    }
}
