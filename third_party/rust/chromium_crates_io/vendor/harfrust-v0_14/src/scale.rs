use crate::GlyphExtents;
use read_fonts::types::{BoundingBox, F48Dot16};

// libm used for f32::floor() and f32::ceil()
#[cfg(not(feature = "std"))]
#[allow(unused_imports)]
use core_maths::CoreFloat as _;

#[derive(Copy, Clone)]
/// Converts font units to the configured font scale.
///
/// The conversion uses the scale set on [`ShaperFont`](crate::ShaperFont)
/// and follows HarfBuzz's rounding rules for glyph metrics and shaping.
#[derive(Debug)]
pub struct Scale {
    x_mult: i64,
    y_mult: i64,
    x_multf: f32,
    y_multf: f32,
}

impl Default for Scale {
    fn default() -> Self {
        Self {
            x_mult: 1 << 16,
            y_mult: 1 << 16,
            x_multf: 1.0,
            y_multf: 1.0,
        }
    }
}

// Various conversions between f32 and i32
#[allow(clippy::cast_precision_loss)]
impl Scale {
    /// Creates a conversion from font units to the requested scale.
    ///
    /// Returns an identity conversion if `scale` is `None` or `upem` is zero.
    pub fn new(scale: Option<(i32, i32)>, upem: i32) -> Self {
        let (Some((x_scale, y_scale)), true) = (scale, upem != 0) else {
            // When scale is not configured, or upem is zero, return results
            // in font units.
            return Self::default();
        };
        let [x_mult, y_mult] = [x_scale, y_scale].map(|s| Self::mult_from_scale(s, upem));
        let upem = upem as f32;
        Self {
            x_mult,
            y_mult,
            x_multf: x_scale as f32 / upem,
            y_multf: y_scale as f32 / upem,
        }
    }

    /// Converts a horizontal distance from font units.
    #[inline(always)]
    pub fn scale_x(&self, x: i32) -> i32 {
        Self::scale_by_mult(x, self.x_mult)
    }

    /// Converts a vertical distance from font units.
    #[inline(always)]
    pub fn scale_y(&self, y: i32) -> i32 {
        Self::scale_by_mult(y, self.y_mult)
    }

    /// Scales a fractional (font-unit) value, matching HarfBuzz's `em_scalef`
    /// (`roundf(v * scale / upem)`).
    #[inline(always)]
    pub(crate) fn scale_x_f(&self, x: f32) -> i32 {
        (x * self.x_multf).round() as i32
    }

    #[inline(always)]
    pub(crate) fn scale_y_f(&self, y: f32) -> i32 {
        (y * self.y_multf).round() as i32
    }

    /// Scales glyph extents using HarfBuzz's corner rounding.
    ///
    /// Floors the near corners and ceils the far corners before calculating
    /// width and height.
    pub fn scale_extents(&self, mut extents: GlyphExtents) -> GlyphExtents {
        let x1 = extents.x_bearing as f32 * self.x_multf;
        let y1 = extents.y_bearing as f32 * self.y_multf;
        let x2 = (i64::from(extents.x_bearing) + i64::from(extents.width)) as f32 * self.x_multf;
        let y2 = (i64::from(extents.y_bearing) + i64::from(extents.height)) as f32 * self.y_multf;
        let rx1 = x1.floor();
        let ry1 = y1.floor();
        let rx2 = x2.ceil();
        let ry2 = y2.ceil();
        extents.x_bearing = rx1 as i32;
        extents.y_bearing = ry1 as i32;
        extents.width = (f64::from(rx2) - f64::from(rx1)) as i32;
        extents.height = (f64::from(ry2) - f64::from(ry1)) as i32;
        extents
    }

    #[inline(always)]
    fn mult_from_scale(scale: i32, upem: i32) -> i64 {
        if scale < 0 {
            -((-(scale as i64)) << 16) / upem as i64
        } else {
            ((scale as i64) << 16) / upem as i64
        }
    }

    #[inline(always)]
    fn scale_by_mult(value: i32, mult: i64) -> i32 {
        ((i64::from(value) * mult + 32768) >> 16) as i32
    }
}

impl read_fonts::model::metrics::Scale for Scale {
    type Value = i32;

    fn add(a: i32, b: i32) -> i32 {
        a.saturating_add(b)
    }

    fn sub(a: i32, b: i32) -> i32 {
        a.saturating_sub(b)
    }

    fn half(value: i32) -> i32 {
        value / 2
    }

    fn scale_x(&self, value: F48Dot16) -> i32 {
        if value.to_bits().trailing_zeros() >= 16 {
            self.scale_x(value.to_i32())
        } else {
            self.scale_x_f(value.to_f32())
        }
    }

    fn scale_y(&self, value: F48Dot16) -> i32 {
        if value.to_bits().trailing_zeros() >= 16 {
            self.scale_y(value.to_i32())
        } else {
            self.scale_y_f(value.to_f32())
        }
    }

    fn scale_glyph_extents(
        &self,
        extents: read_fonts::model::metrics::GlyphExtents<F48Dot16>,
    ) -> read_fonts::model::metrics::GlyphExtents<i32> {
        let left = (extents.x_bearing.to_f32() * self.x_multf).floor();
        let top = (extents.y_bearing.to_f32() * self.y_multf).floor();
        let right = ((extents.x_bearing + extents.width).to_f32() * self.x_multf).ceil();
        let bottom = ((extents.y_bearing - extents.height).to_f32() * self.y_multf).ceil();
        read_fonts::model::metrics::GlyphExtents {
            x_bearing: left as i32,
            y_bearing: top as i32,
            width: (f64::from(right) - f64::from(left)) as i32,
            height: (f64::from(top) - f64::from(bottom)) as i32,
        }
    }

    fn scale_rect(&self, bounds: BoundingBox<F48Dot16>) -> BoundingBox<i32> {
        BoundingBox {
            x_min: self.scale_x_f(bounds.x_min.to_f32()),
            y_min: self.scale_y_f(bounds.y_min.to_f32()),
            x_max: self.scale_x_f(bounds.x_max.to_f32()),
            y_max: self.scale_y_f(bounds.y_max.to_f32()),
        }
    }
}
