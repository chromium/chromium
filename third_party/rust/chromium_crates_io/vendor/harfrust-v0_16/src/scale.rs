use crate::GlyphExtents;
use read_fonts::types::{BoundingBox, F48Dot16};

// libm used for floor() and ceil()
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
    /// (round to nearest, with halfway cases towards positive infinity).
    #[inline(always)]
    pub(crate) fn scale_x_f(&self, x: f32) -> i32 {
        Self::scale_by_mult_f(x, self.x_multf)
    }

    #[inline(always)]
    pub(crate) fn scale_y_f(&self, y: f32) -> i32 {
        Self::scale_by_mult_f(y, self.y_multf)
    }

    /// Clamps a scaled advance to the sign of its scale. This applies the
    /// unsigned font-unit bound while preserving deliberately negative scales.
    pub(crate) fn clamp_x_advance(&self, advance: i32) -> i32 {
        Self::clamp_advance(advance, self.x_multf)
    }

    pub(crate) fn clamp_y_advance(&self, advance: i32) -> i32 {
        Self::clamp_advance(advance, self.y_multf)
    }

    fn clamp_advance(advance: i32, scale: f32) -> i32 {
        if scale < 0.0 {
            advance.min(0)
        } else {
            advance.max(0)
        }
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

    #[inline(always)]
    fn scale_by_mult_f(value: f32, mult: f32) -> i32 {
        // HarfBuzz's _hb_roundf promotes to double before adding 0.5, so
        // values just below a half and large integers round correctly.
        (f64::from(value * mult) + 0.5).floor() as i32
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
        value >> 1
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

#[cfg(test)]
mod tests {
    use super::*;
    use read_fonts::model::metrics::Scale as MetricsScale;

    #[test]
    fn fractional_scaling_rounds_halves_towards_positive_infinity() {
        for (value, expected) in [
            (-2.5, -2),
            (-1.5, -1),
            (-0.5, 0),
            (0.0, 0),
            (0.5, 1),
            (1.5, 2),
            (2.5, 3),
        ] {
            for factor in [-2_i16, -1, 0, 1, 2] {
                let units = i32::from(factor) * 1000;
                let scale = Scale::new(Some((units, -units)), 1000);
                // Divide the input by the factor so both axes encounter the
                // same halfway cases at positive and negative font scales.
                if factor == 0 {
                    assert_eq!(scale.scale_x_f(value), 0);
                    assert_eq!(scale.scale_y_f(value), 0);
                } else {
                    let input = value / f32::from(factor);
                    assert_eq!(scale.scale_x_f(input), expected);
                    assert_eq!(scale.scale_y_f(-input), expected);
                }
            }
        }
    }

    #[test]
    fn fractional_scaling_preserves_precision_when_rounding() {
        let scale = Scale::default();
        for (value, expected) in [
            (f32::from_bits(0.5_f32.to_bits() - 1), 0),
            (f32::from_bits((-0.5_f32).to_bits() + 1), -1),
            (8_388_609.0, 8_388_609),
            (-8_388_609.0, -8_388_609),
        ] {
            assert_eq!(scale.scale_x_f(value), expected);
            assert_eq!(scale.scale_y_f(value), expected);
        }
    }

    #[test]
    fn font_metrics_and_rectangles_use_the_same_rounding() {
        let scale = Scale::new(Some((500, -500)), 1000);
        assert_eq!(MetricsScale::scale_x(&scale, F48Dot16::from_f64(-5.0)), -2);
        assert_eq!(MetricsScale::scale_y(&scale, F48Dot16::from_f64(5.0)), -2);
        let scale = Scale::default();
        assert_eq!(MetricsScale::scale_x(&scale, F48Dot16::from_f64(-2.5)), -2);
        assert_eq!(MetricsScale::scale_y(&scale, F48Dot16::from_f64(-2.5)), -2);
        assert_eq!(
            scale.scale_rect(BoundingBox {
                x_min: F48Dot16::from_f64(-2.5),
                y_min: F48Dot16::from_f64(-1.5),
                x_max: F48Dot16::from_f64(0.5),
                y_max: F48Dot16::from_f64(2.5),
            }),
            BoundingBox {
                x_min: -2,
                y_min: -1,
                x_max: 1,
                y_max: 3,
            }
        );
    }
}
