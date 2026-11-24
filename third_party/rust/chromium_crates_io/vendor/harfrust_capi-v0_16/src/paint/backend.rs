//! Skrifa painting adapter, based on HarfBuzz src/rust/font.rs.
//! The upstream portions retain the license in COPYING-HARFBUZZ.
// The C drawing/painting ABI uses f32 coordinates, including integer font scales.
#![allow(clippy::cast_precision_loss)]
use super::*;
use crate::common::ot_round;
use read_fonts::types::GlyphId;
use skrifa::color::{Brush, ColorPainter, ColorStop, CompositeMode, Extend, Transform};
use skrifa::metrics::BoundingBox;
use skrifa::raw::tables::cpal::ColorRecord;
use std::cell::Cell;

// A color glyph callback may paint the glyph it is handed, which starts another
// Skrifa traversal with fresh cycle detection, depth limit and node budget. So
// nested traversals are bounded per thread: how deeply they nest, and how many
// one outermost paint may start. At either limit the callback is not offered
// the glyph and Skrifa paints it within the current traversal, under that
// traversal's own limits.
const MAX_PAINT_DEPTH: u32 = 8;
const MAX_NESTED_PAINTS: u32 = 256;
thread_local! {
    static PAINT_DEPTH: Cell<u32> = const { Cell::new(0) };
    static NESTED_PAINTS: Cell<u32> = const { Cell::new(0) };
}

struct HrColorPainter<'a> {
    font: *mut hr_font_t,
    paint_funcs: *mut hr_paint_funcs_t,
    paint_data: *mut c_void,
    color_records: &'a [ColorRecord],
    foreground: hr_color_t,
    glyph_clips: Vec<bool>,
    scopes: Vec<Scope>,
}

#[derive(Clone, Copy)]
enum Scope {
    Transform,
    Clip,
    Group(hr_paint_composite_mode_t),
}

impl Drop for HrColorPainter<'_> {
    fn drop(&mut self) {
        // Malformed paint graphs may fail before Skrifa calls the matching pops.
        while let Some(scope) = self.scopes.pop() {
            unsafe {
                match scope {
                    Scope::Transform => hr_paint_pop_transform(self.paint_funcs, self.paint_data),
                    Scope::Clip => hr_paint_pop_clip(self.paint_funcs, self.paint_data),
                    Scope::Group(mode) => {
                        hr_paint_pop_group(self.paint_funcs, self.paint_data, mode);
                    }
                }
            }
        }
    }
}

impl HrColorPainter<'_> {
    fn lookup_color(&self, color_index: u16, alpha: f32) -> hr_color_t {
        if color_index == 0xFFFF {
            // Apply alpha to foreground color
            return ((self.foreground & 0xFFFF_FF00)
                | (ot_round((self.foreground & 0xFF) as f32 * alpha).clamp(0.0, 255.0) as u32))
                as hr_color_t;
        }

        let mut custom = 0;
        if unsafe {
            hr_paint_custom_palette_color(
                self.paint_funcs,
                self.paint_data,
                u32::from(color_index),
                &raw mut custom,
            )
        } != 0
        {
            return (custom & 0xFFFF_FF00)
                | ot_round((custom & 0xFF) as f32 * alpha).clamp(0.0, 255.0) as u32;
        }
        let c = self.color_records.get(color_index as usize);
        if let Some(c) = c {
            (((c.blue as u32) << 24)
                | ((c.green as u32) << 16)
                | ((c.red as u32) << 8)
                | (ot_round(c.alpha as f32 * alpha).clamp(0.0, 255.0) as u32))
                as hr_color_t
        } else {
            0 as hr_color_t
        }
    }

    fn font_transform(&mut self, inverse: bool) {
        let (mut x, mut y) = unsafe { crate::rendering::scale(self.font) };
        if inverse {
            x = if x == 0.0 { 0.0 } else { x.recip() };
            y = if y == 0.0 { 0.0 } else { y.recip() };
        }
        self.push_transform(Transform {
            xx: x,
            yy: y,
            ..Transform::default()
        });
    }
    fn clip_glyph(&mut self, glyph: GlyphId) {
        unsafe {
            hr_paint_push_clip_glyph(self.paint_funcs, self.paint_data, glyph.to_u32(), self.font);
        }
        self.scopes.push(Scope::Clip);
    }
    fn end_clip(&mut self) {
        self.scopes.pop();
        unsafe {
            hr_paint_pop_clip(self.paint_funcs, self.paint_data);
        }
    }
    fn make_color_line(color_line: &ColorLineData) -> hr_color_line_t {
        let mut cl = unsafe { std::mem::zeroed::<hr_color_line_t>() };
        cl.data = std::ptr::from_ref::<ColorLineData>(color_line) as *mut ::std::os::raw::c_void;
        cl.get_color_stops = Some(skrifa_get_color_stops);
        cl.get_extend = Some(skrifa_get_extend);
        cl
    }
}

struct ColorLineData<'a> {
    painter: &'a HrColorPainter<'a>,
    color_stops: &'a [ColorStop],
    extend: Extend,
}
extern "C" fn skrifa_get_color_stops(
    _color_line: *mut hr_color_line_t,
    color_line_data: *mut ::std::os::raw::c_void,
    start: ::std::os::raw::c_uint,
    count_out: *mut ::std::os::raw::c_uint,
    color_stops_out: *mut hr_color_stop_t,
    _user_data: *mut ::std::os::raw::c_void,
) -> ::std::os::raw::c_uint {
    let color_line_data = unsafe { &*(color_line_data as *const ColorLineData) };
    let color_stops = &color_line_data.color_stops;
    if count_out.is_null() {
        return color_stops.len() as u32;
    }
    let available = color_stops.len().saturating_sub(start as usize);
    let count = (unsafe { *count_out } as usize).min(available);
    unsafe { *count_out = count as u32 };
    if !color_stops_out.is_null() {
        for (i, stop) in color_stops
            .iter()
            .skip(start as usize)
            .take(count)
            .enumerate()
        {
            unsafe {
                color_stops_out.add(i).write(hr_color_stop_t {
                    offset: stop.offset,
                    color: color_line_data
                        .painter
                        .lookup_color(stop.palette_index, stop.alpha),
                    is_foreground: i32::from(stop.palette_index == 0xFFFF),
                });
            };
        }
    }
    color_stops.len() as u32
}
extern "C" fn skrifa_get_extend(
    _color_line: *mut hr_color_line_t,
    color_line_data: *mut ::std::os::raw::c_void,
    _user_data: *mut ::std::os::raw::c_void,
) -> hr_paint_extend_t {
    let color_line_data = unsafe { &*(color_line_data as *const ColorLineData) };
    color_line_data.extend as hr_paint_extend_t // They are the same
}

pub fn skrifa_unreduce_anchors(
    x0: f32,
    y0: f32,
    x1: f32,
    y1: f32,
) -> (f32, f32, f32, f32, f32, f32) {
    /* Returns (x0, y0, x1, y1, x2, y2) such that the original
     * `_hr_cairo_reduce_anchors` would produce (xx0, yy0, xx1, yy1)
     * as outputs.
     * The OT spec has the following wording; we just need to
     * invert that operation here:
     *
     * Note: An implementation can derive a single vector, from p₀ to a point p₃, by computing the
     * orthogonal projection of the vector from p₀ to p₁ onto a line perpendicular to line p₀p₂ and
     * passing through p₀ to obtain point p₃. The linear gradient defined using p₀, p₁ and p₂ as
     * described above is functionally equivalent to a linear gradient defined by aligning stop
     * offset 0 to p₀ and aligning stop offset 1.0 to p₃, with each color projecting on either side
     * of that line in a perpendicular direction. This specification uses three points, p₀, p₁ and
     * p₂, as that provides greater flexibility in controlling the placement and rotation of the
     * gradient, as well as variations thereof.
     */

    let dx = x1 - x0;
    let dy = y1 - y0;

    (x0, y0, x1, y1, x0 + dy, y0 - dx)
}

impl ColorPainter for HrColorPainter<'_> {
    fn push_transform(&mut self, transform: Transform) {
        self.scopes.push(Scope::Transform);
        unsafe {
            hr_paint_push_transform(
                self.paint_funcs,
                self.paint_data,
                transform.xx,
                transform.yx,
                transform.xy,
                transform.yy,
                transform.dx,
                transform.dy,
            );
        }
    }
    fn pop_transform(&mut self) {
        self.scopes.pop();
        unsafe {
            hr_paint_pop_transform(self.paint_funcs, self.paint_data);
        }
    }
    fn fill_glyph(
        &mut self,
        glyph_id: GlyphId,
        brush_transform: Option<Transform>,
        brush: Brush<'_>,
    ) {
        self.font_transform(true);

        if brush_transform.is_none() {
            if let Brush::Solid {
                palette_index: color_index,
                alpha,
            } = brush
            {
                let is_foreground = color_index == 0xFFFF;
                let color = self.lookup_color(color_index, alpha);
                unsafe {
                    hr_paint_fill_glyph(
                        self.paint_funcs,
                        self.paint_data,
                        glyph_id.to_u32() as hr_codepoint_t,
                        self.font,
                        is_foreground as hr_bool_t,
                        color,
                    );
                }
                self.pop_transform();
                return;
            }
        }

        self.clip_glyph(glyph_id);
        self.font_transform(false);
        if let Some(wrap_in_transform) = brush_transform {
            self.push_transform(wrap_in_transform);
            self.fill(brush);
            self.pop_transform();
        } else {
            self.fill(brush);
        }
        self.pop_transform();
        self.end_clip();
        self.pop_transform();
    }
    fn push_clip_glyph(&mut self, glyph_id: GlyphId) {
        self.glyph_clips.push(true);
        self.font_transform(true);
        self.clip_glyph(glyph_id);
        self.font_transform(false);
    }
    fn push_clip_box(&mut self, bbox: BoundingBox) {
        self.glyph_clips.push(false);
        self.scopes.push(Scope::Clip);
        unsafe {
            hr_paint_push_clip_rectangle(
                self.paint_funcs,
                self.paint_data,
                bbox.x_min,
                bbox.y_min,
                bbox.x_max,
                bbox.y_max,
            );
        }
    }
    fn pop_clip(&mut self) {
        let Some(is_glyph_clip) = self.glyph_clips.pop() else {
            return;
        };
        if is_glyph_clip {
            self.pop_transform();
        }
        self.end_clip();
        if is_glyph_clip {
            self.pop_transform();
        }
    }
    fn fill(&mut self, brush: Brush) {
        match brush {
            Brush::Solid {
                palette_index: color_index,
                alpha,
            } => {
                let is_foreground = color_index == 0xFFFF;
                unsafe {
                    hr_paint_color(
                        self.paint_funcs,
                        self.paint_data,
                        is_foreground as hr_bool_t,
                        self.lookup_color(color_index, alpha),
                    );
                }
            }
            Brush::LinearGradient {
                color_stops,
                extend,
                p0,
                p1,
            } => {
                let color_stops = ColorLineData {
                    painter: self,
                    color_stops,
                    extend,
                };
                let mut color_line = Self::make_color_line(&color_stops);

                let (x0, y0, x1, y1, x2, y2) = skrifa_unreduce_anchors(p0.x, p0.y, p1.x, p1.y);

                unsafe {
                    hr_paint_linear_gradient(
                        self.paint_funcs,
                        self.paint_data,
                        &raw mut color_line,
                        x0,
                        y0,
                        x1,
                        y1,
                        x2,
                        y2,
                    );
                }
            }
            Brush::RadialGradient {
                color_stops,
                extend,
                c0,
                r0,
                c1,
                r1,
            } => {
                let color_stops = ColorLineData {
                    painter: self,
                    color_stops,
                    extend,
                };
                let mut color_line = Self::make_color_line(&color_stops);
                unsafe {
                    hr_paint_radial_gradient(
                        self.paint_funcs,
                        self.paint_data,
                        &raw mut color_line,
                        c0.x,
                        c0.y,
                        r0,
                        c1.x,
                        c1.y,
                        r1,
                    );
                }
            }
            Brush::SweepGradient {
                color_stops,
                extend,
                c0,
                start_angle,
                end_angle,
            } => {
                let color_stops = ColorLineData {
                    painter: self,
                    color_stops,
                    extend,
                };
                let mut color_line = Self::make_color_line(&color_stops);
                // Skrifa has this gem, so we swap end_angle and start_angle
                // when passing to our API:
                //
                //  * Convert angles and stops from counter-clockwise to clockwise
                //  * for the shader if the gradient is not already reversed due to
                //  * start angle being larger than end angle.
                //
                //  Undo that.
                let (start_angle, end_angle) = (360. - start_angle, 360. - end_angle);
                let start_angle = start_angle.to_radians();
                let end_angle = end_angle.to_radians();
                unsafe {
                    hr_paint_sweep_gradient(
                        self.paint_funcs,
                        self.paint_data,
                        &raw mut color_line,
                        c0.x,
                        c0.y,
                        start_angle,
                        end_angle,
                    );
                }
            }
        }
    }
    fn paint_cached_color_glyph(
        &mut self,
        glyph: GlyphId,
    ) -> Result<skrifa::color::PaintCachedColorGlyph, skrifa::color::PaintError> {
        if PAINT_DEPTH.get() >= MAX_PAINT_DEPTH || NESTED_PAINTS.get() >= MAX_NESTED_PAINTS {
            return Ok(skrifa::color::PaintCachedColorGlyph::Unimplemented);
        }
        self.font_transform(true);
        let painted = unsafe {
            hr_paint_color_glyph(self.paint_funcs, self.paint_data, glyph.to_u32(), self.font)
        };
        self.pop_transform();
        Ok(if painted != 0 {
            skrifa::color::PaintCachedColorGlyph::Ok
        } else {
            skrifa::color::PaintCachedColorGlyph::Unimplemented
        })
    }
    fn push_layer(&mut self, mode: CompositeMode) {
        let mode = mode as hr_paint_composite_mode_t;
        self.scopes.push(Scope::Group(mode));
        unsafe {
            hr_paint_push_group_for(self.paint_funcs, self.paint_data, mode);
        }
    }
    fn pop_layer_with_mode(&mut self, mode: CompositeMode) {
        self.scopes.pop();
        let mode = mode as hr_paint_composite_mode_t; // They are the same
        unsafe {
            hr_paint_pop_group(self.paint_funcs, self.paint_data, mode);
        }
    }
}

pub(super) unsafe fn paint(
    font: *mut hr_font_t,
    glyph: u32,
    funcs: *mut hr_paint_funcs_t,
    data: *mut c_void,
    palette: u32,
    foreground: hr_color_t,
) -> hr_bool_t {
    use read_fonts::TableProvider;
    let Some(render_face) = (unsafe { crate::hr_font_get_face(font).as_ref() }) else {
        return 0;
    };
    let Some(face) = (unsafe { crate::rendering::font_ref(render_face) }) else {
        return 0;
    };
    let glyph = GlyphId::new(glyph);
    let coords = unsafe { crate::rendering::coords(font) };
    if let Some(color) = skrifa::color::ColorGlyphCollection::new(&face).get(glyph) {
        let cpal = face.cpal().ok();
        let records = cpal
            .as_ref()
            .and_then(|cpal| cpal.color_records_array())
            .and_then(Result::ok);
        let colors = cpal
            .as_ref()
            .and_then(|cpal| {
                let start = cpal
                    .color_record_indices()
                    .get(palette as usize)
                    .or_else(|| cpal.color_record_indices().first())?
                    .get() as usize;
                let end = start.checked_add(cpal.num_palette_entries() as usize)?;
                records.as_ref()?.get(start..end)
            })
            .unwrap_or(&[]);
        let mut painter = HrColorPainter {
            font,
            paint_funcs: funcs,
            paint_data: data,
            color_records: colors,
            foreground,
            glyph_clips: Vec::new(),
            scopes: Vec::new(),
        };
        let depth = PAINT_DEPTH.get();
        NESTED_PAINTS.set(if depth == 0 {
            0
        } else {
            NESTED_PAINTS.get().saturating_add(1)
        });
        PAINT_DEPTH.set(depth + 1);
        unsafe { hr_paint_push_font_transform(funcs, data, font) };
        let result = color.paint(coords.as_slice(), &mut painter);
        drop(painter);
        unsafe { hr_paint_pop_transform(funcs, data) };
        PAINT_DEPTH.set(depth);
        return i32::from(result.is_ok());
    }
    if let Some(svg) = face.svg().ok().and_then(|svg| svg.glyph_data(glyph)) {
        if unsafe {
            image(
                render_face,
                funcs,
                data,
                svg,
                0,
                0,
                HR_PAINT_IMAGE_FORMAT_SVG,
                core::ptr::null_mut(),
            )
        } != 0
        {
            return 1;
        }
    }
    use skrifa::bitmap::{BitmapData, BitmapFormat, BitmapStrikes, Origin};
    use skrifa::instance::Size;
    let mut x_ppem = 0;
    let mut y_ppem = 0;
    unsafe { crate::hr_font_get_ppem(font, &raw mut x_ppem, &raw mut y_ppem) };
    let ppem = x_ppem.max(y_ppem);
    let size = if ppem == 0 {
        Size::unscaled()
    } else {
        Size::new(ppem as f32)
    };
    for format in [BitmapFormat::Cbdt, BitmapFormat::Sbix] {
        let Some(bitmap) = BitmapStrikes::with_format(&face, format)
            .and_then(|strikes| strikes.glyph_for_size(size, glyph))
        else {
            continue;
        };
        let (bytes, format) = match bitmap.data {
            BitmapData::Png(bytes) => (bytes, HR_PAINT_IMAGE_FORMAT_PNG),
            BitmapData::Bgra(bytes) => (bytes, HR_PAINT_IMAGE_FORMAT_BGRA),
            BitmapData::Mask(_) => continue,
        };
        if !bitmap.ppem_x.is_finite()
            || !bitmap.ppem_y.is_finite()
            || bitmap.ppem_x <= 0.0
            || bitmap.ppem_y <= 0.0
            || bitmap.width >= 65536
            || bitmap.height >= 65536
        {
            continue;
        }
        let upem = unsafe { crate::hr_face_get_upem(crate::hr_font_get_face(font)) } as f32;
        let (sx, sy) = unsafe { crate::rendering::scale(font) };
        let mx = upem / bitmap.ppem_x;
        let my = upem / bitmap.ppem_y;
        let bx = ot_round(bitmap.bearing_x + bitmap.inner_bearing_x * mx) as f32;
        let inner = bitmap.inner_bearing_y
            + if bitmap.placement_origin == Origin::BottomLeft {
                bitmap.height as f32
            } else {
                0.0
            };
        let by = ot_round(bitmap.bearing_y + inner * my) as f32;
        let w = ot_round(bitmap.width as f32 * mx) as f32;
        let h = ot_round(-(bitmap.height as f32 * my)) as f32;
        let mut extents = hr_glyph_extents_t {
            x_bearing: (bx * sx).floor() as i32,
            y_bearing: (by * sy).floor() as i32,
            width: ((bx + w) * sx).ceil() as i32,
            height: ((by + h) * sy).ceil() as i32,
        };
        extents.width = extents.width.saturating_sub(extents.x_bearing);
        extents.height = extents.height.saturating_sub(extents.y_bearing);
        if unsafe {
            image(
                render_face,
                funcs,
                data,
                bytes,
                bitmap.width,
                bitmap.height,
                format,
                &raw mut extents,
            )
        } != 0
        {
            return 1;
        }
    }
    0
}
#[allow(clippy::too_many_arguments)]
unsafe fn image(
    face: &crate::hr_face_t,
    funcs: *mut hr_paint_funcs_t,
    data: *mut c_void,
    bytes: &[u8],
    width: u32,
    height: u32,
    format: u32,
    extents: *mut hr_glyph_extents_t,
) -> hr_bool_t {
    if bytes.is_empty() {
        return 0;
    }
    // `bytes` is a slice of the face's rendering data, which never changes
    // once built. Consumers may retain the blob beyond the callback, so hand
    // out a sub-blob sharing that storage, as HarfBuzz does with its table
    // blob, rather than a copy: one SVG document may cover every glyph of a
    // font, and copying it per paint costs glyphs x document bytes.
    let Some(render) = (unsafe { crate::rendering::render_blob(face) }) else {
        return 0;
    };
    let Some(start) = (bytes.as_ptr() as usize).checked_sub(render.as_ptr() as usize) else {
        return 0;
    };
    let Some(end) = start
        .checked_add(bytes.len())
        .filter(|end| *end <= render.len())
    else {
        return 0;
    };
    let blob = hr_blob_t::sub_shared(render, start, end);
    let result = unsafe { hr_paint_image(funcs, data, blob, width, height, format, 0.0, extents) };
    unsafe { crate::hr_blob_destroy(blob) };
    result
}
