//! COLR table instance.

use super::{traversal::ColorStopVec, Brush, PaintError, Transform};
use core::ops::{Deref, Range};
#[cfg(feature = "libm")]
#[allow(unused_imports)]
use core_maths::*;
use read_fonts::{
    tables::{
        colr::*,
        variations::{
            DeltaSetIndex, DeltaSetIndexMap, FloatItemDelta, FloatItemDeltaTarget,
            ItemVariationStore,
        },
    },
    types::{BoundingBox, F2Dot14, GlyphId16, Point},
    ReadError,
};

/// Unique paint identifier used for detecting cycles in the paint graph.
pub type PaintId = usize;

/// Combination of a `COLR` table and a location in variation space for
/// resolving paints.
///
/// See [`resolve_paint`], [`ColorStops::resolve`] and [`resolve_clip_box`].
#[derive(Clone)]
pub struct ColrInstance<'a> {
    colr: Colr<'a>,
    index_map: Option<DeltaSetIndexMap<'a>>,
    var_store: Option<ItemVariationStore<'a>>,
    coords: &'a [F2Dot14],
}

impl<'a> ColrInstance<'a> {
    /// Creates a new instance for the given `COLR` table and normalized variation
    /// coordinates.
    pub fn new(colr: Colr<'a>, coords: &'a [F2Dot14]) -> Self {
        let index_map = colr.var_index_map().and_then(|res| res.ok());
        let var_store = colr.item_variation_store().and_then(|res| res.ok());
        Self {
            colr,
            coords,
            index_map,
            var_store,
        }
    }

    /// Computes a sequence of N variation deltas starting at the given
    /// `var_base` index.
    fn var_deltas<const N: usize>(&self, var_index_base: u32) -> [FloatItemDelta; N] {
        // Magic value that indicates deltas should not be applied.
        const NO_VARIATION_DELTAS: u32 = 0xFFFFFFFF;
        // Note: FreeType never returns an error for these lookups, so
        // we do the same and just `unwrap_or_default` on var store
        // errors.
        // See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/fc01e7dd/src/sfnt/ttcolr.c#L574>
        let mut deltas = [FloatItemDelta::ZERO; N];
        if self.coords.is_empty()
            || self.var_store.is_none()
            || var_index_base == NO_VARIATION_DELTAS
        {
            return deltas;
        }
        // Avoid overflow if var_index_base + N > u32::MAX
        let actual_count = ((u32::MAX - var_index_base) as usize).min(N);
        let var_store = self.var_store.as_ref().unwrap();
        if let Some(index_map) = self.index_map.as_ref() {
            for (i, delta) in deltas.iter_mut().enumerate().take(actual_count) {
                let var_index = var_index_base + i as u32;
                if let Ok(delta_ix) = index_map.get(var_index) {
                    *delta = var_store
                        .compute_float_delta(delta_ix, self.coords)
                        .unwrap_or_default();
                }
            }
        } else {
            for (i, delta) in deltas.iter_mut().enumerate().take(actual_count) {
                let var_index = var_index_base + i as u32;
                // If we don't have a var index map, use our index as the inner
                // component and set the outer to 0.
                let delta_ix = DeltaSetIndex {
                    outer: 0,
                    inner: var_index as u16,
                };
                *delta = var_store
                    .compute_float_delta(delta_ix, self.coords)
                    .unwrap_or_default();
            }
        }
        deltas
    }
}

impl<'a> Deref for ColrInstance<'a> {
    type Target = Colr<'a>;

    fn deref(&self) -> &Self::Target {
        &self.colr
    }
}

/// Resolves a clip box, applying variation deltas using the given
/// instance.
pub fn resolve_clip_box(instance: &ColrInstance, clip_box: &ClipBox) -> BoundingBox<f32> {
    match clip_box {
        ClipBox::Format1(cbox) => BoundingBox {
            x_min: cbox.x_min().to_i16() as f32,
            y_min: cbox.y_min().to_i16() as f32,
            x_max: cbox.x_max().to_i16() as f32,
            y_max: cbox.y_max().to_i16() as f32,
        },
        ClipBox::Format2(cbox) => {
            let deltas = instance.var_deltas::<4>(cbox.var_index_base());
            BoundingBox {
                x_min: cbox.x_min().apply_float_delta(deltas[0]),
                y_min: cbox.y_min().apply_float_delta(deltas[1]),
                x_max: cbox.x_max().apply_float_delta(deltas[2]),
                y_max: cbox.y_max().apply_float_delta(deltas[3]),
            }
        }
    }
}

/// Simplified version of a [`ColorStop`] or [`VarColorStop`] with applied
/// variation deltas.
#[derive(Clone, Debug)]
pub struct ResolvedColorStop {
    pub offset: f32,
    pub palette_index: u16,
    pub alpha: f32,
}

/// Collection of [`ColorStop`] or [`VarColorStop`].
// Note: only one of these fields is used at any given time, but this structure
// was chosen over the obvious enum approach for simplicity in generating a
// single concrete type for the `impl Iterator` return type of the `resolve`
// method.
#[derive(Clone)]
pub struct ColorStops<'a> {
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
    /// Returns an iterator yielding resolved color stops with variation deltas
    /// applied.
    pub fn resolve(
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
                    offset: stop.stop_offset().apply_float_delta(deltas[0]),
                    palette_index: stop.palette_index(),
                    alpha: stop.alpha().apply_float_delta(deltas[1]),
                }
            }))
    }
}

/// Similar to `Option<Brush>` but with an additional variant for a valid brush
/// that produces no rendering.
pub(crate) enum MaybeBrush<'a> {
    Some(Brush<'a>),
    /// Valid brush but produces no rendering
    NonRendering,
    /// Not a brush
    None,
}

/// Simplified version of `Paint` with applied variation deltas.
///
/// These are constructed with the [`resolve_paint`] function.
///
/// This is roughly equivalent to FreeType's
/// [`FT_COLR_Paint`](https://freetype.org/freetype2/docs/reference/ft2-layer_management.html#ft_colr_paint)
/// type.
pub enum ResolvedPaint<'a> {
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
        glyph_id: GlyphId16,
        paint: Paint<'a>,
    },
    ColrGlyph {
        glyph_id: GlyphId16,
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
    pub(crate) fn as_transform(&self) -> Option<(Transform, Paint<'a>)> {
        match self {
            ResolvedPaint::Rotate {
                angle,
                around_center,
                paint,
            } => {
                let sin_v = (angle * 180.0).to_radians().sin();
                let cos_v = (angle * 180.0).to_radians().cos();
                let mut out_transform = Transform {
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
                let mut out_transform = Transform {
                    xx: *scale_x,
                    yy: *scale_y,
                    ..Transform::default()
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
                let mut out_transform = Transform {
                    xy: -tan_x,
                    yx: tan_y,
                    ..Transform::default()
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
                Transform {
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
                Transform {
                    dx: *dx,
                    dy: *dy,
                    ..Default::default()
                },
                paint.clone(),
            )),
            _ => None,
        }
    }

    pub(crate) fn as_brush<'s>(
        &self,
        instance: &ColrInstance,
        resolved_stops: &'s mut ColorStopVec,
    ) -> Result<MaybeBrush<'s>, PaintError> {
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
pub fn resolve_paint<'a>(
    instance: &ColrInstance<'a>,
    paint: &Paint<'a>,
) -> Result<ResolvedPaint<'a>, ReadError> {
    Ok(match paint {
        Paint::ColrLayers(layers) => {
            let start = layers.first_layer_index() as usize;
            ResolvedPaint::ColrLayers {
                range: start..start + layers.num_layers() as usize,
            }
        }
        Paint::Solid(solid) => ResolvedPaint::Solid {
            palette_index: solid.palette_index(),
            alpha: solid.alpha().to_f32(),
        },
        Paint::VarSolid(solid) => {
            let deltas = instance.var_deltas::<1>(solid.var_index_base());
            ResolvedPaint::Solid {
                palette_index: solid.palette_index(),
                alpha: solid.alpha().apply_float_delta(deltas[0]),
            }
        }
        Paint::LinearGradient(gradient) => {
            let color_line = gradient.color_line()?;
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
            let color_line = gradient.color_line()?;
            let extend = color_line.extend();
            let deltas = instance.var_deltas::<6>(gradient.var_index_base());
            ResolvedPaint::LinearGradient {
                x0: gradient.x0().apply_float_delta(deltas[0]),
                y0: gradient.y0().apply_float_delta(deltas[1]),
                x1: gradient.x1().apply_float_delta(deltas[2]),
                y1: gradient.y1().apply_float_delta(deltas[3]),
                x2: gradient.x2().apply_float_delta(deltas[4]),
                y2: gradient.y2().apply_float_delta(deltas[5]),
                color_stops: color_line.into(),
                extend,
            }
        }
        Paint::RadialGradient(gradient) => {
            let color_line = gradient.color_line()?;
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
            let color_line = gradient.color_line()?;
            let extend = color_line.extend();
            let deltas = instance.var_deltas::<6>(gradient.var_index_base());
            ResolvedPaint::RadialGradient {
                x0: gradient.x0().apply_float_delta(deltas[0]),
                y0: gradient.y0().apply_float_delta(deltas[1]),
                radius0: gradient.radius0().apply_float_delta(deltas[2]),
                x1: gradient.x1().apply_float_delta(deltas[3]),
                y1: gradient.y1().apply_float_delta(deltas[4]),
                radius1: gradient.radius1().apply_float_delta(deltas[5]),
                color_stops: color_line.into(),
                extend,
            }
        }
        Paint::SweepGradient(gradient) => {
            let color_line = gradient.color_line()?;
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
            let color_line = gradient.color_line()?;
            let extend = color_line.extend();
            let deltas = instance.var_deltas::<4>(gradient.var_index_base());
            ResolvedPaint::SweepGradient {
                center_x: gradient.center_x().apply_float_delta(deltas[0]),
                center_y: gradient.center_y().apply_float_delta(deltas[1]),
                start_angle: gradient.start_angle().apply_float_delta(deltas[2]),
                end_angle: gradient.end_angle().apply_float_delta(deltas[3]),
                color_stops: color_line.into(),
                extend,
            }
        }
        Paint::Glyph(glyph) => ResolvedPaint::Glyph {
            glyph_id: glyph.glyph_id(),
            paint: glyph.paint()?,
        },
        Paint::ColrGlyph(glyph) => ResolvedPaint::ColrGlyph {
            glyph_id: glyph.glyph_id(),
        },
        Paint::Transform(transform) => {
            let affine = transform.transform()?;
            let paint = transform.paint()?;
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
            let affine = transform.transform()?;
            let paint = transform.paint()?;
            let deltas = instance.var_deltas::<6>(affine.var_index_base());
            ResolvedPaint::Transform {
                xx: affine.xx().apply_float_delta(deltas[0]),
                yx: affine.yx().apply_float_delta(deltas[1]),
                xy: affine.xy().apply_float_delta(deltas[2]),
                yy: affine.yy().apply_float_delta(deltas[3]),
                dx: affine.dx().apply_float_delta(deltas[4]),
                dy: affine.dy().apply_float_delta(deltas[5]),
                paint,
            }
        }
        Paint::Translate(transform) => ResolvedPaint::Translate {
            dx: transform.dx().to_i16() as f32,
            dy: transform.dy().to_i16() as f32,
            paint: transform.paint()?,
        },
        Paint::VarTranslate(transform) => {
            let deltas = instance.var_deltas::<2>(transform.var_index_base());
            ResolvedPaint::Translate {
                dx: transform.dx().apply_float_delta(deltas[0]),
                dy: transform.dy().apply_float_delta(deltas[1]),
                paint: transform.paint()?,
            }
        }
        Paint::Scale(transform) => ResolvedPaint::Scale {
            scale_x: transform.scale_x().to_f32(),
            scale_y: transform.scale_y().to_f32(),
            around_center: None,
            paint: transform.paint()?,
        },
        Paint::VarScale(transform) => {
            let deltas = instance.var_deltas::<2>(transform.var_index_base());
            ResolvedPaint::Scale {
                scale_x: transform.scale_x().apply_float_delta(deltas[0]),
                scale_y: transform.scale_y().apply_float_delta(deltas[1]),
                around_center: None,
                paint: transform.paint()?,
            }
        }
        Paint::ScaleAroundCenter(transform) => ResolvedPaint::Scale {
            scale_x: transform.scale_x().to_f32(),
            scale_y: transform.scale_y().to_f32(),
            around_center: Some(Point::new(
                transform.center_x().to_i16() as f32,
                transform.center_y().to_i16() as f32,
            )),
            paint: transform.paint()?,
        },
        Paint::VarScaleAroundCenter(transform) => {
            let deltas = instance.var_deltas::<4>(transform.var_index_base());
            ResolvedPaint::Scale {
                scale_x: transform.scale_x().apply_float_delta(deltas[0]),
                scale_y: transform.scale_y().apply_float_delta(deltas[1]),
                around_center: Some(Point::new(
                    transform.center_x().apply_float_delta(deltas[2]),
                    transform.center_y().apply_float_delta(deltas[3]),
                )),
                paint: transform.paint()?,
            }
        }
        Paint::ScaleUniform(transform) => {
            let scale = transform.scale().to_f32();
            ResolvedPaint::Scale {
                scale_x: scale,
                scale_y: scale,
                around_center: None,
                paint: transform.paint()?,
            }
        }
        Paint::VarScaleUniform(transform) => {
            let deltas = instance.var_deltas::<1>(transform.var_index_base());
            let scale = transform.scale().apply_float_delta(deltas[0]);
            ResolvedPaint::Scale {
                scale_x: scale,
                scale_y: scale,
                around_center: None,
                paint: transform.paint()?,
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
                paint: transform.paint()?,
            }
        }
        Paint::VarScaleUniformAroundCenter(transform) => {
            let deltas = instance.var_deltas::<3>(transform.var_index_base());
            let scale = transform.scale().apply_float_delta(deltas[0]);
            ResolvedPaint::Scale {
                scale_x: scale,
                scale_y: scale,
                around_center: Some(Point::new(
                    transform.center_x().apply_float_delta(deltas[1]),
                    transform.center_y().apply_float_delta(deltas[2]),
                )),
                paint: transform.paint()?,
            }
        }
        Paint::Rotate(transform) => ResolvedPaint::Rotate {
            angle: transform.angle().to_f32(),
            around_center: None,
            paint: transform.paint()?,
        },
        Paint::VarRotate(transform) => {
            let deltas = instance.var_deltas::<1>(transform.var_index_base());
            ResolvedPaint::Rotate {
                angle: transform.angle().apply_float_delta(deltas[0]),
                around_center: None,
                paint: transform.paint()?,
            }
        }
        Paint::RotateAroundCenter(transform) => ResolvedPaint::Rotate {
            angle: transform.angle().to_f32(),
            around_center: Some(Point::new(
                transform.center_x().to_i16() as f32,
                transform.center_y().to_i16() as f32,
            )),
            paint: transform.paint()?,
        },
        Paint::VarRotateAroundCenter(transform) => {
            let deltas = instance.var_deltas::<3>(transform.var_index_base());
            ResolvedPaint::Rotate {
                angle: transform.angle().apply_float_delta(deltas[0]),
                around_center: Some(Point::new(
                    transform.center_x().apply_float_delta(deltas[1]),
                    transform.center_y().apply_float_delta(deltas[2]),
                )),
                paint: transform.paint()?,
            }
        }
        Paint::Skew(transform) => ResolvedPaint::Skew {
            x_skew_angle: transform.x_skew_angle().to_f32(),
            y_skew_angle: transform.y_skew_angle().to_f32(),
            around_center: None,
            paint: transform.paint()?,
        },
        Paint::VarSkew(transform) => {
            let deltas = instance.var_deltas::<2>(transform.var_index_base());
            ResolvedPaint::Skew {
                x_skew_angle: transform.x_skew_angle().apply_float_delta(deltas[0]),
                y_skew_angle: transform.y_skew_angle().apply_float_delta(deltas[1]),
                around_center: None,
                paint: transform.paint()?,
            }
        }
        Paint::SkewAroundCenter(transform) => ResolvedPaint::Skew {
            x_skew_angle: transform.x_skew_angle().to_f32(),
            y_skew_angle: transform.y_skew_angle().to_f32(),
            around_center: Some(Point::new(
                transform.center_x().to_i16() as f32,
                transform.center_y().to_i16() as f32,
            )),
            paint: transform.paint()?,
        },
        Paint::VarSkewAroundCenter(transform) => {
            let deltas = instance.var_deltas::<4>(transform.var_index_base());
            ResolvedPaint::Skew {
                x_skew_angle: transform.x_skew_angle().apply_float_delta(deltas[0]),
                y_skew_angle: transform.y_skew_angle().apply_float_delta(deltas[1]),
                around_center: Some(Point::new(
                    transform.center_x().apply_float_delta(deltas[2]),
                    transform.center_y().apply_float_delta(deltas[3]),
                )),
                paint: transform.paint()?,
            }
        }
        Paint::Composite(composite) => ResolvedPaint::Composite {
            source_paint: composite.source_paint()?,
            mode: composite.composite_mode(),
            backdrop_paint: composite.backdrop_paint()?,
        },
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use raw::{FontRef, TableProvider};

    /// OSS Fuzz caught add with overflow when computing delta indices.
    /// See <https://oss-fuzz.com/testcase-detail/5180237819478016>
    /// and <https://g-issues.oss-fuzz.com/issues/439498857>
    #[test]
    fn var_delta_index_overflow() {
        let font = FontRef::new(font_test_data::COLRV0V1_VARIABLE).unwrap();
        let coords = &[F2Dot14::from_f32(0.5)];
        let instance = ColrInstance::new(font.colr().unwrap(), coords);
        // Just don't panic with overflow
        let _: [FloatItemDelta; 4] = instance.var_deltas(0xFFFFFFFE);
    }
}
