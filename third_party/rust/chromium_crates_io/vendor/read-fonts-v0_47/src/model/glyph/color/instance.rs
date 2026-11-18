//! Resolution of `COLR` paints at a location in variation space.

use super::{traversal::ColorStopVec, Brush, Error};
use crate::{
    model::font::ColrTables,
    tables::{
        colr::*,
        variations::{DeltaSetIndex, NO_VARIATION_INDEX},
    },
    types::{BoundingBox, F2Dot14, F48Dot16, FWord, Fixed, GlyphId, GlyphId16, Matrix, Point},
};
use core::ops::{Deref, Range};

#[cfg(feature = "libm")]
#[allow(unused_imports)]
use core_maths::CoreFloat;

/// Unique paint identifier used for detecting cycles in the paint graph.
pub(super) type PaintId = usize;

/// A `COLR` table bound to a location in variation space for resolving
/// paints.
///
/// See [`resolve_paint`], [`ColorStops::resolve`] and [`resolve_clip_box`].
#[derive(Clone, Copy)]
pub(super) struct ColrInstance<'a> {
    tables: &'a ColrTables<'a>,
    coords: &'a [F2Dot14],
    /// The variation store's region scalars at `coords`, where they are
    /// known. Any region past the end is computed for each delta.
    scalars: &'a [Fixed],
}

impl<'a> ColrInstance<'a> {
    /// Creates an instance for `tables` at the normalized variation
    /// coordinates `coords`, with the region `scalars` already computed
    /// there.
    pub(super) fn new(
        tables: &'a ColrTables<'a>,
        coords: &'a [F2Dot14],
        scalars: &'a [Fixed],
    ) -> Self {
        Self {
            tables,
            coords,
            scalars,
        }
    }

    /// Computes a sequence of `N` variation deltas starting at the given
    /// variation index base.
    fn var_deltas<const N: usize>(&self, var_index_base: u32) -> [F48Dot16; N] {
        // FreeType never fails these lookups, so neither does this: an
        // unreadable delta is zero.
        // See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/fc01e7dd/src/sfnt/ttcolr.c#L574>
        let mut deltas = [F48Dot16::ZERO; N];
        let Some(var_store) = self.tables.var_store.as_ref() else {
            return deltas;
        };
        if self.coords.is_empty() || var_index_base == NO_VARIATION_INDEX {
            return deltas;
        }
        // Avoid overflow if var_index_base + N > u32::MAX
        let actual_count = ((u32::MAX - var_index_base) as usize).min(N);
        if let Some(index_map) = self.tables.index_map.as_ref() {
            for (i, delta) in deltas.iter_mut().enumerate().take(actual_count) {
                let var_index = var_index_base + i as u32;
                if let Ok(delta_ix) = index_map.get(var_index) {
                    *delta = var_store
                        .compute_delta_with_scalars(delta_ix, self.coords, self.scalars)
                        .unwrap_or_default();
                }
            }
        } else {
            for (i, delta) in deltas.iter_mut().enumerate().take(actual_count) {
                let var_index = var_index_base + i as u32;
                // Without a var index map, the mapping is implicit: the
                // index is a delta set index with the outer component in the
                // high word and the inner component in the low word.
                let delta_ix = DeltaSetIndex {
                    outer: (var_index >> 16) as u16,
                    inner: var_index as u16,
                };
                *delta = var_store
                    .compute_delta_with_scalars(delta_ix, self.coords, self.scalars)
                    .unwrap_or_default();
            }
        }
        deltas
    }
}

impl<'a> Deref for ColrInstance<'a> {
    type Target = Colr<'a>;

    fn deref(&self) -> &Self::Target {
        &self.tables.colr
    }
}

/// Resolves a clip box, applying variation deltas using the given instance.
pub(super) fn resolve_clip_box(instance: &ColrInstance, clip_box: &ClipBox) -> BoundingBox<f32> {
    match clip_box {
        ClipBox::Format1(clip_box) => BoundingBox {
            x_min: clip_box.x_min().to_i16() as f32,
            y_min: clip_box.y_min().to_i16() as f32,
            x_max: clip_box.x_max().to_i16() as f32,
            y_max: clip_box.y_max().to_i16() as f32,
        },
        ClipBox::Format2(clip_box) => {
            let deltas = instance.var_deltas::<4>(clip_box.var_index_base());
            BoundingBox {
                x_min: clip_box.x_min().apply_delta(deltas[0]),
                y_min: clip_box.y_min().apply_delta(deltas[1]),
                x_max: clip_box.x_max().apply_delta(deltas[2]),
                y_max: clip_box.y_max().apply_delta(deltas[3]),
            }
        }
    }
}

/// Resolves a clip box for measuring glyph extents, as HarfBuzz does.
///
/// Each variation delta is rounded to a whole unit before it is added.
pub(super) fn resolve_clip_box_for_extents(
    instance: &ColrInstance,
    clip_box: &ClipBox,
) -> BoundingBox<f32> {
    match clip_box {
        ClipBox::Format1(_) => resolve_clip_box(instance, clip_box),
        ClipBox::Format2(clip_box) => {
            let deltas = instance.var_deltas::<4>(clip_box.var_index_base());
            let edge =
                |value: FWord, delta: F48Dot16| value.to_i16() as f32 + delta.to_f32().round();
            BoundingBox {
                x_min: edge(clip_box.x_min(), deltas[0]),
                y_min: edge(clip_box.y_min(), deltas[1]),
                x_max: edge(clip_box.x_max(), deltas[2]),
                y_max: edge(clip_box.y_max(), deltas[3]),
            }
        }
    }
}

/// A [`ColorStop`] or [`VarColorStop`] with its variation deltas applied.
#[derive(Clone, Debug)]
pub(super) struct ResolvedColorStop {
    pub offset: f32,
    pub palette_index: u16,
    pub alpha: f32,
}

/// Color stops from a [`ColorLine`] or [`VarColorLine`].
// Only one of these fields is used at any given time, but this was chosen
// over the obvious enum for simplicity in generating a single concrete type
// for the `impl Iterator` return type of `resolve`.
#[derive(Clone)]
pub(super) struct ColorStops<'a> {
    stops: &'a [ColorStop],
    var_stops: &'a [VarColorStop],
}

impl<'a> From<ColorLine<'a>> for ColorStops<'a> {
    fn from(value: ColorLine<'a>) -> Self {
        Self {
            stops: value.color_stops(),
            var_stops: &[],
        }
    }
}

impl<'a> From<VarColorLine<'a>> for ColorStops<'a> {
    fn from(value: VarColorLine<'a>) -> Self {
        Self {
            stops: &[],
            var_stops: value.color_stops(),
        }
    }
}

impl<'a> ColorStops<'a> {
    /// Returns an iterator yielding resolved color stops with variation
    /// deltas applied.
    pub(super) fn resolve(
        &self,
        instance: &'a ColrInstance<'a>,
    ) -> impl Iterator<Item = ResolvedColorStop> + 'a {
        self.stops
            .iter()
            .map(|stop| ResolvedColorStop {
                offset: stop.stop_offset().to_f32(),
                palette_index: stop.palette_index(),
                alpha: stop.alpha().to_f32(),
            })
            .chain(self.var_stops.iter().map(|stop| {
                let deltas = instance.var_deltas::<2>(stop.var_index_base());
                ResolvedColorStop {
                    offset: stop.stop_offset().apply_delta(deltas[0]),
                    palette_index: stop.palette_index(),
                    alpha: stop.alpha().apply_delta(deltas[1]),
                }
            }))
    }
}

/// Like `Option<Brush>`, with an extra variant for a valid brush that paints
/// nothing.
pub(super) enum MaybeBrush<'a> {
    Some(Brush<'a>),
    /// Valid brush but produces no rendering
    NonRendering,
    /// Not a brush
    None,
}

/// A `Paint` with its variation deltas applied.
///
/// Created by [`resolve_paint`]. This is roughly equivalent to FreeType's
/// [`FT_COLR_Paint`](https://freetype.org/freetype2/docs/reference/ft2-layer_management.html#ft_colr_paint).
pub(super) enum ResolvedPaint<'a> {
    ColrLayers {
        range: Range<usize>,
    },
    Solid {
        palette_index: u16,
        alpha: f32,
    },
    LinearGradient {
        x0: f32,
        y0: f32,
        x1: f32,
        y1: f32,
        x2: f32,
        y2: f32,
        color_stops: ColorStops<'a>,
        extend: Extend,
    },
    RadialGradient {
        x0: f32,
        y0: f32,
        radius0: f32,
        x1: f32,
        y1: f32,
        radius1: f32,
        color_stops: ColorStops<'a>,
        extend: Extend,
    },
    SweepGradient {
        center_x: f32,
        center_y: f32,
        start_angle: f32,
        end_angle: f32,
        color_stops: ColorStops<'a>,
        extend: Extend,
    },
    Glyph {
        glyph: GlyphId,
        paint: Paint<'a>,
    },
    ColrGlyph {
        glyph: GlyphId16,
    },
    Transform {
        xx: f32,
        yx: f32,
        xy: f32,
        yy: f32,
        dx: f32,
        dy: f32,
        paint: Paint<'a>,
    },
    Translate {
        dx: f32,
        dy: f32,
        paint: Paint<'a>,
    },
    Scale {
        scale_x: f32,
        scale_y: f32,
        around_center: Option<Point<f32>>,
        paint: Paint<'a>,
    },
    Rotate {
        angle: f32,
        around_center: Option<Point<f32>>,
        paint: Paint<'a>,
    },
    Skew {
        x_skew_angle: f32,
        y_skew_angle: f32,
        around_center: Option<Point<f32>>,
        paint: Paint<'a>,
    },
    Composite {
        source_paint: Paint<'a>,
        mode: CompositeMode,
        backdrop_paint: Paint<'a>,
    },
}

impl<'a> ResolvedPaint<'a> {
    pub(super) fn as_transform(&self) -> Option<(Matrix<f32>, Paint<'a>)> {
        match self {
            ResolvedPaint::Rotate {
                angle,
                around_center,
                paint,
            } => {
                let sin_v = (angle * 180.0).to_radians().sin();
                let cos_v = (angle * 180.0).to_radians().cos();
                let mut out_transform = Matrix {
                    xx: cos_v,
                    xy: -sin_v,
                    yx: sin_v,
                    yy: cos_v,
                    ..Default::default()
                };

                fn scalar_dot_product(a: f32, b: f32, c: f32, d: f32) -> f32 {
                    a * b + c * d
                }

                if let Some(center) = around_center {
                    out_transform.dx = scalar_dot_product(sin_v, center.y, 1.0 - cos_v, center.x);
                    out_transform.dy = scalar_dot_product(-sin_v, center.x, 1.0 - cos_v, center.y);
                }
                Some((out_transform, paint.clone()))
            }
            ResolvedPaint::Scale {
                scale_x,
                scale_y,
                around_center,
                paint,
            } => {
                let mut out_transform = Matrix {
                    xx: *scale_x,
                    yy: *scale_y,
                    ..Matrix::default()
                };

                if let Some(center) = around_center {
                    out_transform.dx = center.x - scale_x * center.x;
                    out_transform.dy = center.y - scale_y * center.y;
                }
                Some((out_transform, paint.clone()))
            }
            ResolvedPaint::Skew {
                x_skew_angle,
                y_skew_angle,
                around_center,
                paint,
            } => {
                let tan_x = (x_skew_angle * 180.0).to_radians().tan();
                let tan_y = (y_skew_angle * 180.0).to_radians().tan();
                let mut out_transform = Matrix {
                    xy: -tan_x,
                    yx: tan_y,
                    ..Matrix::default()
                };

                if let Some(center) = around_center {
                    out_transform.dx = tan_x * center.y;
                    out_transform.dy = -tan_y * center.x;
                }
                Some((out_transform, paint.clone()))
            }
            ResolvedPaint::Transform {
                xx,
                yx,
                xy,
                yy,
                dx,
                dy,
                paint,
            } => Some((
                Matrix {
                    xx: *xx,
                    yx: *yx,
                    xy: *xy,
                    yy: *yy,
                    dx: *dx,
                    dy: *dy,
                },
                paint.clone(),
            )),
            ResolvedPaint::Translate { dx, dy, paint, .. } => Some((
                Matrix {
                    dx: *dx,
                    dy: *dy,
                    ..Default::default()
                },
                paint.clone(),
            )),
            _ => None,
        }
    }

    pub(super) fn as_brush<'s>(
        &self,
        instance: &ColrInstance,
        resolved_stops: &'s mut ColorStopVec,
    ) -> Result<MaybeBrush<'s>, Error> {
        match self {
            ResolvedPaint::Solid {
                palette_index,
                alpha,
            } => Ok(MaybeBrush::Some(Brush::Solid {
                palette_index: *palette_index,
                alpha: *alpha,
            })),
            ResolvedPaint::LinearGradient {
                x0,
                y0,
                x1,
                y1,
                x2,
                y2,
                color_stops,
                extend,
            } => {
                let mut p0 = Point::new(*x0, *y0);
                let p1 = Point::new(*x1, *y1);
                let p2 = Point::new(*x2, *y2);

                let dot_product = |a: Point<f32>, b: Point<f32>| -> f32 { a.x * b.x + a.y * b.y };
                let cross_product = |a: Point<f32>, b: Point<f32>| -> f32 { a.x * b.y - a.y * b.x };
                let project_onto = |vector: Point<f32>, point: Point<f32>| -> Point<f32> {
                    let length = (point.x * point.x + point.y * point.y).sqrt();
                    if length == 0.0 {
                        return Point::default();
                    }
                    let mut point_normalized = point / length;
                    point_normalized *= dot_product(vector, point) / length;
                    point_normalized
                };

                make_sorted_resolved_stops(color_stops, instance, resolved_stops);

                // If p0p1 or p0p2 are degenerate probably nothing should be drawn.
                // If p0p1 and p0p2 are parallel then one side is the first color and the other side is
                // the last color, depending on the direction.
                // For now, just use the first color.
                if p1 == p0 || p2 == p0 || cross_product(p1 - p0, p2 - p0) == 0.0 {
                    if let Some(stop) = resolved_stops.first() {
                        return Ok(MaybeBrush::Some(Brush::Solid {
                            palette_index: stop.palette_index,
                            alpha: stop.alpha,
                        }));
                    };
                    return Ok(MaybeBrush::NonRendering);
                }

                // Follow implementation note in nanoemoji:
                // https://github.com/googlefonts/nanoemoji/blob/0ac6e7bb4d8202db692574d8530a9b643f1b3b3c/src/nanoemoji/svg.py#L188
                // to compute a new gradient end point P3 as the orthogonal
                // projection of the vector from p0 to p1 onto a line perpendicular
                // to line p0p2 and passing through p0.
                let mut perpendicular_to_p2 = p2 - p0;
                perpendicular_to_p2 = Point::new(perpendicular_to_p2.y, -perpendicular_to_p2.x);
                let mut p3 = p0 + project_onto(p1 - p0, perpendicular_to_p2);

                match (
                    resolved_stops.first().cloned(),
                    resolved_stops.last().cloned(),
                ) {
                    (None, _) | (_, None) => {}
                    (Some(first_stop), Some(last_stop)) => {
                        let mut color_stop_range = last_stop.offset - first_stop.offset;

                        // Nothing can be drawn for this situation.
                        if color_stop_range == 0.0 && extend != &Extend::Pad {
                            return Ok(MaybeBrush::NonRendering);
                        }

                        // In the Pad case, for providing normalized stops in the 0 to 1 range to the client,
                        // insert a color stop at the end. Adding this stop will paint the equivalent gradient,
                        // because: All font-specified color stops are in the same spot, mode is pad, so
                        // everything before this spot is painted with the first color, everything after this spot
                        // is painted with the last color. Not adding this stop would skip the projection below along
                        // the p0-p3 axis and result in specifying non-normalized color stops to the shader.

                        if color_stop_range == 0.0 && extend == &Extend::Pad {
                            let mut extra_stop = last_stop;
                            extra_stop.offset += 1.0;
                            resolved_stops.push(extra_stop);

                            color_stop_range = 1.0;
                        }

                        debug_assert!(color_stop_range != 0.0);

                        if color_stop_range != 1.0 || first_stop.offset != 0.0 {
                            let p0_p3 = p3 - p0;
                            let p0_offset = p0_p3 * first_stop.offset;
                            let p3_offset = p0_p3 * last_stop.offset;

                            p3 = p0 + p3_offset;
                            p0 += p0_offset;

                            let scale_factor = 1.0 / color_stop_range;
                            let start_offset = first_stop.offset;

                            for stop in resolved_stops.iter_mut() {
                                stop.offset = (stop.offset - start_offset) * scale_factor;
                            }
                        }

                        return Ok(MaybeBrush::Some(Brush::LinearGradient {
                            p0,
                            p1: p3,
                            color_stops: resolved_stops.as_slice(),
                            extend: *extend,
                        }));
                    }
                }

                Ok(MaybeBrush::NonRendering)
            }
            ResolvedPaint::RadialGradient {
                x0,
                y0,
                radius0,
                x1,
                y1,
                radius1,
                color_stops,
                extend,
            } => {
                let mut c0 = Point::new(*x0, *y0);
                let mut c1 = Point::new(*x1, *y1);
                let mut radius0 = *radius0;
                let mut radius1 = *radius1;

                make_sorted_resolved_stops(color_stops, instance, resolved_stops);

                match (
                    resolved_stops.first().cloned(),
                    resolved_stops.last().cloned(),
                ) {
                    (None, _) | (_, None) => {}
                    (Some(first_stop), Some(last_stop)) => {
                        let mut color_stop_range = last_stop.offset - first_stop.offset;
                        // Nothing can be drawn for this situation.
                        if color_stop_range == 0.0 && extend != &Extend::Pad {
                            return Ok(MaybeBrush::NonRendering);
                        }

                        // In the Pad case, for providing normalized stops in the 0 to 1 range to the client,
                        // insert a color stop at the end. See LinearGradient for more details.

                        if color_stop_range == 0.0 && extend == &Extend::Pad {
                            let mut extra_stop = last_stop;
                            extra_stop.offset += 1.0;
                            resolved_stops.push(extra_stop);
                            color_stop_range = 1.0;
                        }

                        debug_assert!(color_stop_range != 0.0);

                        // If the colorStopRange is 0 at this point, the default behavior of the shader is to
                        // clamp to 1 color stops that are above 1, clamp to 0 for color stops that are below 0,
                        // and repeat the outer color stops at 0 and 1 if the color stops are inside the
                        // range. That will result in the correct rendering.
                        if color_stop_range != 1.0 || first_stop.offset != 0.0 {
                            let c0_to_c1 = c1 - c0;
                            let radius_diff = radius1 - radius0;
                            let scale_factor = 1.0 / color_stop_range;

                            let c0_offset = c0_to_c1 * first_stop.offset;
                            let c1_offset = c0_to_c1 * last_stop.offset;
                            let stops_start_offset = first_stop.offset;

                            // Order of reassignments is important to avoid shadowing variables.
                            c1 = c0 + c1_offset;
                            c0 += c0_offset;
                            radius1 = radius0 + radius_diff * last_stop.offset;
                            radius0 += radius_diff * first_stop.offset;

                            for stop in resolved_stops.iter_mut() {
                                stop.offset = (stop.offset - stops_start_offset) * scale_factor;
                            }
                        }

                        return Ok(MaybeBrush::Some(Brush::RadialGradient {
                            c0,
                            r0: radius0,
                            c1,
                            r1: radius1,
                            color_stops: resolved_stops.as_slice(),
                            extend: *extend,
                        }));
                    }
                }
                Ok(MaybeBrush::NonRendering)
            }
            ResolvedPaint::SweepGradient {
                center_x,
                center_y,
                start_angle,
                end_angle,
                color_stops,
                extend,
            } => {
                // OpenType 1.9.1 adds a shift to the angle to ease specification of a 0 to 360
                // degree sweep.
                let sweep_angle_to_degrees = |angle| angle * 180.0 + 180.0;

                let start_angle = sweep_angle_to_degrees(start_angle);
                let end_angle = sweep_angle_to_degrees(end_angle);

                // Stop normalization for sweep:

                let sector_angle = end_angle - start_angle;

                make_sorted_resolved_stops(color_stops, instance, resolved_stops);
                if resolved_stops.is_empty() {
                    return Ok(MaybeBrush::NonRendering);
                }

                match (
                    resolved_stops.first().cloned(),
                    resolved_stops.last().cloned(),
                ) {
                    (None, _) | (_, None) => {}
                    (Some(first_stop), Some(last_stop)) => {
                        let mut color_stop_range = last_stop.offset - first_stop.offset;

                        let mut start_angle_scaled = start_angle + sector_angle * first_stop.offset;
                        let mut end_angle_scaled = start_angle + sector_angle * last_stop.offset;

                        let start_offset = first_stop.offset;

                        // Nothing can be drawn for this situation.
                        if color_stop_range == 0.0 && extend != &Extend::Pad {
                            return Ok(MaybeBrush::NonRendering);
                        }

                        // In the Pad case, if the color_stop_range is 0 insert a color stop at the end before
                        // normalizing. Adding this stop will paint the equivalent gradient, because: All font
                        // specified color stops are in the same spot, mode is pad, so everything before this
                        // spot is painted with the first color, everything after this spot is painted with
                        // the last color. Not adding this stop will skip the projection and result in
                        // specifying non-normalized color stops to the shader.
                        if color_stop_range == 0.0 && extend == &Extend::Pad {
                            let mut offset_last = last_stop;
                            offset_last.offset += 1.0;
                            resolved_stops.push(offset_last);
                            color_stop_range = 1.0;
                        }

                        debug_assert!(color_stop_range != 0.0);

                        let scale_factor = 1.0 / color_stop_range;

                        for shift_stop in resolved_stops.iter_mut() {
                            shift_stop.offset = (shift_stop.offset - start_offset) * scale_factor;
                        }

                        // /* https://docs.microsoft.com/en-us/typography/opentype/spec/colr#sweep-gradients
                        //  * "The angles are expressed in counter-clockwise degrees from
                        //  * the direction of the positive x-axis on the design
                        //  * grid. [...]  The color line progresses from the start angle
                        //  * to the end angle in the counter-clockwise direction;" -
                        //  * Convert angles and stops from counter-clockwise to clockwise
                        //  * for the shader if the gradient is not already reversed due to
                        //  * start angle being larger than end angle. */
                        start_angle_scaled = 360.0 - start_angle_scaled;
                        end_angle_scaled = 360.0 - end_angle_scaled;

                        if start_angle_scaled >= end_angle_scaled {
                            (start_angle_scaled, end_angle_scaled) =
                                (end_angle_scaled, start_angle_scaled);
                            resolved_stops.reverse();
                            for stop in resolved_stops.iter_mut() {
                                stop.offset = 1.0 - stop.offset;
                            }
                        }

                        // https://learn.microsoft.com/en-us/typography/opentype/spec/colr#sweep-gradients
                        // "If the color line's extend mode is reflect or repeat
                        // and start and end angle are equal, nothing shall be drawn."
                        if start_angle_scaled == end_angle_scaled && extend != &Extend::Pad {
                            return Ok(MaybeBrush::NonRendering);
                        }

                        return Ok(MaybeBrush::Some(Brush::SweepGradient {
                            c0: Point::new(*center_x, *center_y),
                            start_angle: start_angle_scaled,
                            end_angle: end_angle_scaled,
                            color_stops: resolved_stops.as_slice(),
                            extend: *extend,
                        }));
                    }
                }
                Ok(MaybeBrush::NonRendering)
            }
            _ => Ok(MaybeBrush::None),
        }
    }
}

fn make_sorted_resolved_stops(
    stops: &ColorStops,
    instance: &ColrInstance,
    out_stops: &mut ColorStopVec,
) {
    let color_stop_iter = stops.resolve(instance).map(|stop| stop.into());
    out_stops.clear();
    for stop in color_stop_iter {
        out_stops.push(stop);
    }
    out_stops.sort_by(|a, b| {
        a.offset
            .partial_cmp(&b.offset)
            .unwrap_or(core::cmp::Ordering::Equal)
    });
}

/// Resolves this paint with the given instance.
///
/// Resolving means that all numeric values are converted to 32-bit floating
/// point, variation deltas are applied (also computed fully in floating
/// point), and the various transform paints are collapsed into a single value
/// for their category (transform, translate, scale, rotate and skew).
///
/// This provides a simpler type for consumers that are more interested
/// in extracting the semantics of the graph rather than working with the
/// raw encoded structures.
pub(super) fn resolve_paint<'a>(
    instance: &ColrInstance<'a>,
    paint: &Paint<'a>,
) -> Result<ResolvedPaint<'a>, Error> {
    Ok(match paint {
        Paint::ColrLayers(layers) => {
            // A range that overflows, as it can on a 32-bit target, names no
            // layer the table could hold.
            let start = layers.first_layer_index() as usize;
            let end = start
                .checked_add(layers.num_layers() as usize)
                .ok_or(Error::Malformed)?;
            ResolvedPaint::ColrLayers { range: start..end }
        }
        Paint::Solid(solid) => ResolvedPaint::Solid {
            palette_index: solid.palette_index(),
            alpha: solid.alpha().to_f32(),
        },
        Paint::VarSolid(solid) => {
            let deltas = instance.var_deltas::<1>(solid.var_index_base());
            ResolvedPaint::Solid {
                palette_index: solid.palette_index(),
                alpha: solid.alpha().apply_delta(deltas[0]),
            }
        }
        Paint::LinearGradient(gradient) => {
            let color_line = gradient.color_line().map_err(|_| Error::Malformed)?;
            let extend = color_line.extend();
            ResolvedPaint::LinearGradient {
                x0: gradient.x0().to_i16() as f32,
                y0: gradient.y0().to_i16() as f32,
                x1: gradient.x1().to_i16() as f32,
                y1: gradient.y1().to_i16() as f32,
                x2: gradient.x2().to_i16() as f32,
                y2: gradient.y2().to_i16() as f32,
                color_stops: color_line.into(),
                extend,
            }
        }
        Paint::VarLinearGradient(gradient) => {
            let color_line = gradient.color_line().map_err(|_| Error::Malformed)?;
            let extend = color_line.extend();
            let deltas = instance.var_deltas::<6>(gradient.var_index_base());
            ResolvedPaint::LinearGradient {
                x0: gradient.x0().apply_delta(deltas[0]),
                y0: gradient.y0().apply_delta(deltas[1]),
                x1: gradient.x1().apply_delta(deltas[2]),
                y1: gradient.y1().apply_delta(deltas[3]),
                x2: gradient.x2().apply_delta(deltas[4]),
                y2: gradient.y2().apply_delta(deltas[5]),
                color_stops: color_line.into(),
                extend,
            }
        }
        Paint::RadialGradient(gradient) => {
            let color_line = gradient.color_line().map_err(|_| Error::Malformed)?;
            let extend = color_line.extend();
            ResolvedPaint::RadialGradient {
                x0: gradient.x0().to_i16() as f32,
                y0: gradient.y0().to_i16() as f32,
                radius0: gradient.radius0().to_u16() as f32,
                x1: gradient.x1().to_i16() as f32,
                y1: gradient.y1().to_i16() as f32,
                radius1: gradient.radius1().to_u16() as f32,
                color_stops: color_line.into(),
                extend,
            }
        }
        Paint::VarRadialGradient(gradient) => {
            let color_line = gradient.color_line().map_err(|_| Error::Malformed)?;
            let extend = color_line.extend();
            let deltas = instance.var_deltas::<6>(gradient.var_index_base());
            ResolvedPaint::RadialGradient {
                x0: gradient.x0().apply_delta(deltas[0]),
                y0: gradient.y0().apply_delta(deltas[1]),
                radius0: gradient.radius0().apply_delta(deltas[2]),
                x1: gradient.x1().apply_delta(deltas[3]),
                y1: gradient.y1().apply_delta(deltas[4]),
                radius1: gradient.radius1().apply_delta(deltas[5]),
                color_stops: color_line.into(),
                extend,
            }
        }
        Paint::SweepGradient(gradient) => {
            let color_line = gradient.color_line().map_err(|_| Error::Malformed)?;
            let extend = color_line.extend();
            ResolvedPaint::SweepGradient {
                center_x: gradient.center_x().to_i16() as f32,
                center_y: gradient.center_y().to_i16() as f32,
                start_angle: gradient.start_angle().to_f32(),
                end_angle: gradient.end_angle().to_f32(),
                color_stops: color_line.into(),
                extend,
            }
        }
        Paint::VarSweepGradient(gradient) => {
            let color_line = gradient.color_line().map_err(|_| Error::Malformed)?;
            let extend = color_line.extend();
            let deltas = instance.var_deltas::<4>(gradient.var_index_base());
            ResolvedPaint::SweepGradient {
                center_x: gradient.center_x().apply_delta(deltas[0]),
                center_y: gradient.center_y().apply_delta(deltas[1]),
                start_angle: gradient.start_angle().apply_delta(deltas[2]),
                end_angle: gradient.end_angle().apply_delta(deltas[3]),
                color_stops: color_line.into(),
                extend,
            }
        }
        Paint::Glyph(glyph) => ResolvedPaint::Glyph {
            glyph: glyph.glyph_id().into(),
            paint: glyph.paint().map_err(|_| Error::Malformed)?,
        },
        Paint::Glyph2(glyph) => ResolvedPaint::Glyph {
            glyph: glyph.glyph_id().into(),
            paint: glyph.paint().map_err(|_| Error::Malformed)?,
        },
        Paint::ColrGlyph(glyph) => ResolvedPaint::ColrGlyph {
            glyph: glyph.glyph_id(),
        },
        Paint::Transform(transform) => {
            let affine = transform.transform().map_err(|_| Error::Malformed)?;
            let paint = transform.paint().map_err(|_| Error::Malformed)?;
            ResolvedPaint::Transform {
                xx: affine.xx().to_f32(),
                yx: affine.yx().to_f32(),
                xy: affine.xy().to_f32(),
                yy: affine.yy().to_f32(),
                dx: affine.dx().to_f32(),
                dy: affine.dy().to_f32(),
                paint,
            }
        }
        Paint::VarTransform(transform) => {
            let affine = transform.transform().map_err(|_| Error::Malformed)?;
            let paint = transform.paint().map_err(|_| Error::Malformed)?;
            let deltas = instance.var_deltas::<6>(affine.var_index_base());
            ResolvedPaint::Transform {
                xx: affine.xx().apply_delta(deltas[0]),
                yx: affine.yx().apply_delta(deltas[1]),
                xy: affine.xy().apply_delta(deltas[2]),
                yy: affine.yy().apply_delta(deltas[3]),
                dx: affine.dx().apply_delta(deltas[4]),
                dy: affine.dy().apply_delta(deltas[5]),
                paint,
            }
        }
        Paint::Translate(transform) => ResolvedPaint::Translate {
            dx: transform.dx().to_i16() as f32,
            dy: transform.dy().to_i16() as f32,
            paint: transform.paint().map_err(|_| Error::Malformed)?,
        },
        Paint::VarTranslate(transform) => {
            let deltas = instance.var_deltas::<2>(transform.var_index_base());
            ResolvedPaint::Translate {
                dx: transform.dx().apply_delta(deltas[0]),
                dy: transform.dy().apply_delta(deltas[1]),
                paint: transform.paint().map_err(|_| Error::Malformed)?,
            }
        }
        Paint::Scale(transform) => ResolvedPaint::Scale {
            scale_x: transform.scale_x().to_f32(),
            scale_y: transform.scale_y().to_f32(),
            around_center: None,
            paint: transform.paint().map_err(|_| Error::Malformed)?,
        },
        Paint::VarScale(transform) => {
            let deltas = instance.var_deltas::<2>(transform.var_index_base());
            ResolvedPaint::Scale {
                scale_x: transform.scale_x().apply_delta(deltas[0]),
                scale_y: transform.scale_y().apply_delta(deltas[1]),
                around_center: None,
                paint: transform.paint().map_err(|_| Error::Malformed)?,
            }
        }
        Paint::ScaleAroundCenter(transform) => ResolvedPaint::Scale {
            scale_x: transform.scale_x().to_f32(),
            scale_y: transform.scale_y().to_f32(),
            around_center: Some(Point::new(
                transform.center_x().to_i16() as f32,
                transform.center_y().to_i16() as f32,
            )),
            paint: transform.paint().map_err(|_| Error::Malformed)?,
        },
        Paint::VarScaleAroundCenter(transform) => {
            let deltas = instance.var_deltas::<4>(transform.var_index_base());
            ResolvedPaint::Scale {
                scale_x: transform.scale_x().apply_delta(deltas[0]),
                scale_y: transform.scale_y().apply_delta(deltas[1]),
                around_center: Some(Point::new(
                    transform.center_x().apply_delta(deltas[2]),
                    transform.center_y().apply_delta(deltas[3]),
                )),
                paint: transform.paint().map_err(|_| Error::Malformed)?,
            }
        }
        Paint::ScaleUniform(transform) => {
            let scale = transform.scale().to_f32();
            ResolvedPaint::Scale {
                scale_x: scale,
                scale_y: scale,
                around_center: None,
                paint: transform.paint().map_err(|_| Error::Malformed)?,
            }
        }
        Paint::VarScaleUniform(transform) => {
            let deltas = instance.var_deltas::<1>(transform.var_index_base());
            let scale = transform.scale().apply_delta(deltas[0]);
            ResolvedPaint::Scale {
                scale_x: scale,
                scale_y: scale,
                around_center: None,
                paint: transform.paint().map_err(|_| Error::Malformed)?,
            }
        }
        Paint::ScaleUniformAroundCenter(transform) => {
            let scale = transform.scale().to_f32();
            ResolvedPaint::Scale {
                scale_x: scale,
                scale_y: scale,
                around_center: Some(Point::new(
                    transform.center_x().to_i16() as f32,
                    transform.center_y().to_i16() as f32,
                )),
                paint: transform.paint().map_err(|_| Error::Malformed)?,
            }
        }
        Paint::VarScaleUniformAroundCenter(transform) => {
            let deltas = instance.var_deltas::<3>(transform.var_index_base());
            let scale = transform.scale().apply_delta(deltas[0]);
            ResolvedPaint::Scale {
                scale_x: scale,
                scale_y: scale,
                around_center: Some(Point::new(
                    transform.center_x().apply_delta(deltas[1]),
                    transform.center_y().apply_delta(deltas[2]),
                )),
                paint: transform.paint().map_err(|_| Error::Malformed)?,
            }
        }
        Paint::Rotate(transform) => ResolvedPaint::Rotate {
            angle: transform.angle().to_f32(),
            around_center: None,
            paint: transform.paint().map_err(|_| Error::Malformed)?,
        },
        Paint::VarRotate(transform) => {
            let deltas = instance.var_deltas::<1>(transform.var_index_base());
            ResolvedPaint::Rotate {
                angle: transform.angle().apply_delta(deltas[0]),
                around_center: None,
                paint: transform.paint().map_err(|_| Error::Malformed)?,
            }
        }
        Paint::RotateAroundCenter(transform) => ResolvedPaint::Rotate {
            angle: transform.angle().to_f32(),
            around_center: Some(Point::new(
                transform.center_x().to_i16() as f32,
                transform.center_y().to_i16() as f32,
            )),
            paint: transform.paint().map_err(|_| Error::Malformed)?,
        },
        Paint::VarRotateAroundCenter(transform) => {
            let deltas = instance.var_deltas::<3>(transform.var_index_base());
            ResolvedPaint::Rotate {
                angle: transform.angle().apply_delta(deltas[0]),
                around_center: Some(Point::new(
                    transform.center_x().apply_delta(deltas[1]),
                    transform.center_y().apply_delta(deltas[2]),
                )),
                paint: transform.paint().map_err(|_| Error::Malformed)?,
            }
        }
        Paint::Skew(transform) => ResolvedPaint::Skew {
            x_skew_angle: transform.x_skew_angle().to_f32(),
            y_skew_angle: transform.y_skew_angle().to_f32(),
            around_center: None,
            paint: transform.paint().map_err(|_| Error::Malformed)?,
        },
        Paint::VarSkew(transform) => {
            let deltas = instance.var_deltas::<2>(transform.var_index_base());
            ResolvedPaint::Skew {
                x_skew_angle: transform.x_skew_angle().apply_delta(deltas[0]),
                y_skew_angle: transform.y_skew_angle().apply_delta(deltas[1]),
                around_center: None,
                paint: transform.paint().map_err(|_| Error::Malformed)?,
            }
        }
        Paint::SkewAroundCenter(transform) => ResolvedPaint::Skew {
            x_skew_angle: transform.x_skew_angle().to_f32(),
            y_skew_angle: transform.y_skew_angle().to_f32(),
            around_center: Some(Point::new(
                transform.center_x().to_i16() as f32,
                transform.center_y().to_i16() as f32,
            )),
            paint: transform.paint().map_err(|_| Error::Malformed)?,
        },
        Paint::VarSkewAroundCenter(transform) => {
            let deltas = instance.var_deltas::<4>(transform.var_index_base());
            ResolvedPaint::Skew {
                x_skew_angle: transform.x_skew_angle().apply_delta(deltas[0]),
                y_skew_angle: transform.y_skew_angle().apply_delta(deltas[1]),
                around_center: Some(Point::new(
                    transform.center_x().apply_delta(deltas[2]),
                    transform.center_y().apply_delta(deltas[3]),
                )),
                paint: transform.paint().map_err(|_| Error::Malformed)?,
            }
        }
        Paint::Composite(composite) => ResolvedPaint::Composite {
            source_paint: composite.source_paint().map_err(|_| Error::Malformed)?,
            mode: composite.composite_mode(),
            backdrop_paint: composite.backdrop_paint().map_err(|_| Error::Malformed)?,
        },
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{FontRef, TableProvider};

    /// OSS Fuzz caught add with overflow when computing delta indices.
    /// See <https://oss-fuzz.com/testcase-detail/5180237819478016>
    /// and <https://g-issues.oss-fuzz.com/issues/439498857>
    #[test]
    fn var_delta_index_overflow() {
        let font = FontRef::new(font_test_data::COLRV0V1_VARIABLE).unwrap();
        let coords = &[F2Dot14::from_f32(0.5)];
        let tables = ColrTables::new(font.colr().unwrap());
        let instance = ColrInstance::new(&tables, coords, &[]);
        // Just don't panic with overflow
        let _: [F48Dot16; 4] = instance.var_deltas(0xFFFFFFFE);
    }

    /// Without a `VarIndexMap`, a variation index is a delta-set index with
    /// the outer index in the high word and the inner index in the low word.
    /// See <https://learn.microsoft.com/en-us/typography/opentype/spec/colr#colr-table-and-opentype-font-variations>
    // Deltas from the scalars a varied font caches match deltas that compute
    // every region.
    #[test]
    fn cached_scalars_give_the_same_deltas() {
        use crate::model::{Font, NormalizedCoord};
        let font = Font::new(font_test_data::COLRV0V1_VARIABLE, 0).unwrap();
        let axes = font.axes().len() as usize;
        for value in [-1.0, -0.3, 0.6, 1.0] {
            let varied = font
                .instance_builder()
                .normalized_coords(core::iter::repeat_n(NormalizedCoord::from_f32(value), axes))
                .build();
            let tables = varied.colr_tables().unwrap();
            let scalars = varied.colr_scalars();
            assert!(!scalars.is_empty());
            let cached = ColrInstance::new(tables, varied.normalized_coords(), scalars);
            let computed = ColrInstance::new(tables, varied.normalized_coords(), &[]);
            for base in 0..512 {
                assert_eq!(
                    cached.var_deltas::<4>(base),
                    computed.var_deltas::<4>(base),
                    "{value} {base}"
                );
            }
        }
        assert!(font.colr_scalars().is_empty());
    }

    #[test]
    fn var_deltas_implicit_mapping_uses_outer_index() {
        use crate::FontRead;
        use font_test_data::bebuffer::BeBuffer;

        let buf = BeBuffer::new()
            // COLR v1 header with only an item variation store
            .push(1u16) // version
            .push(0u16) // numBaseGlyphRecords
            .push(0u32) // baseGlyphRecordsOffset
            .push(0u32) // layerRecordsOffset
            .push(0u16) // numLayerRecords
            .push(0u32) // baseGlyphListOffset
            .push(0u32) // layerListOffset
            .push(0u32) // clipListOffset
            .push(0u32) // varIndexMapOffset (NULL)
            .push(34u32) // itemVariationStoreOffset
            // ItemVariationStore
            .push(1u16) // format
            .push(16u32) // variationRegionListOffset
            .push(2u16) // itemVariationDataCount
            .extend([26u32, 36]) // itemVariationDataOffsets
            // VariationRegionList: one axis, one region
            .extend([1u16, 1])
            .extend([F2Dot14::ZERO, F2Dot14::ONE, F2Dot14::ONE])
            // ItemVariationData 0: a single item with a delta of 100
            .extend([1u16, 1, 1, 0])
            .push(100i16)
            // ItemVariationData 1: a single item with a delta of 200
            .extend([1u16, 1, 1, 0])
            .push(200i16);
        let colr = Colr::read(buf.data().into()).unwrap();
        let tables = ColrTables::new(colr);
        let coords = &[F2Dot14::ONE];
        let instance = ColrInstance::new(&tables, coords, &[]);
        assert!(tables.index_map.is_none());
        assert_eq!(
            instance.var_deltas::<1>(0x0000_0000),
            [F48Dot16::from_i32(100)]
        );
        assert_eq!(
            instance.var_deltas::<1>(0x0001_0000),
            [F48Dot16::from_i32(200)]
        );
    }
}
