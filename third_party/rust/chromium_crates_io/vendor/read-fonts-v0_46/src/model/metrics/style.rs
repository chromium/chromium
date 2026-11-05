//! Metrics used to style text and decorations.

use super::metric;
use crate::model::variation::NormalizedCoord;
use crate::{
    ps::{cff::CffFontRef, type1::Type1Font},
    tables::mvar::tags,
    TableProvider,
};
use types::{F48Dot16, Fixed};

/// The size and placement of a subscript or superscript em box.
///
/// Values are in font units unless a caller names another unit.
#[derive(Copy, Clone, Default, PartialEq, Eq, Debug)]
pub struct ScriptMetrics<T = F48Dot16> {
    /// Suggested width of the script em box.
    pub x_size: T,
    /// Suggested height of the script em box.
    pub y_size: T,
    /// Horizontal offset from the base glyph.
    pub x_offset: T,
    /// Vertical offset from the baseline. A positive subscript offset is below
    /// it; a positive superscript offset is above it, as in `OS/2`.
    pub y_offset: T,
}

/// The position and thickness of an underline or strikethrough.
///
/// Position is measured from the baseline to the top of the stroke.
/// Values are in font units unless a caller names another unit.
#[derive(Copy, Clone, Default, PartialEq, Eq, Debug)]
pub struct Decoration<T = F48Dot16> {
    /// Position of the top of the stroke relative to the baseline.
    pub position: T,
    /// Suggested stroke thickness.
    pub thickness: T,
}

/// Font-wide style measurements from `OS/2` and `post`, or Type 1 font info.
///
/// Unlike [`super::Metrics`], reading these may require the `post` table.
/// Each group is `None` if its source table is missing or unreadable. At a
/// non-default location, `MVAR` deltas are applied without rounding. The italic
/// angle remains the value in `post`; normalized coordinates alone do not
/// provide the user-space `slnt` value needed to adjust it.
#[derive(Copy, Clone, Default, PartialEq, Eq, Debug)]
pub struct StyleMetrics {
    /// Design units per em, which the measurements use.
    ///
    /// The same as [`Metrics::units_per_em`](super::Metrics::units_per_em).
    pub units_per_em: u16,
    /// Suggested subscript em box and placement, from `OS/2`.
    pub subscript: Option<ScriptMetrics>,
    /// Suggested superscript em box and placement, from `OS/2`.
    pub superscript: Option<ScriptMetrics>,
    /// Suggested underline, from `post`.
    pub underline: Option<Decoration>,
    /// Suggested strikethrough, from `OS/2`.
    pub strike_through: Option<Decoration>,
    /// Italic angle in degrees, from `post`. This is not a length.
    pub italic_angle: Option<Fixed>,
    /// Whether `post` reports a fixed pitch.
    pub is_fixed_pitch: Option<bool>,
}

impl StyleMetrics {
    /// Reads style measurements from `tables` at `normalized_coords`.
    ///
    /// Pass an empty location for the font's defaults. Missing or unreadable
    /// tables leave their measurements unset.
    pub fn from_sfnt<'a>(
        tables: &impl TableProvider<'a>,
        normalized_coords: &[NormalizedCoord],
    ) -> Self {
        let coords = normalized_coords;
        let mvar = (!coords.is_empty()).then(|| tables.mvar().ok()).flatten();
        let deltas = mvar.as_ref().and_then(|mvar| mvar.at(coords));
        let deltas = deltas.as_ref();
        let mut metrics = Self {
            units_per_em: super::units_per_em(tables.head().map_or(0, |head| head.units_per_em())),
            ..Self::default()
        };
        if let Ok(os2) = tables.os2() {
            metrics.subscript = Some(ScriptMetrics {
                x_size: metric(os2.y_subscript_x_size() as i32, deltas, tags::SBXS),
                y_size: metric(os2.y_subscript_y_size() as i32, deltas, tags::SBYS),
                x_offset: metric(os2.y_subscript_x_offset() as i32, deltas, tags::SBXO),
                y_offset: metric(os2.y_subscript_y_offset() as i32, deltas, tags::SBYO),
            });
            metrics.superscript = Some(ScriptMetrics {
                x_size: metric(os2.y_superscript_x_size() as i32, deltas, tags::SPXS),
                y_size: metric(os2.y_superscript_y_size() as i32, deltas, tags::SPYS),
                x_offset: metric(os2.y_superscript_x_offset() as i32, deltas, tags::SPXO),
                y_offset: metric(os2.y_superscript_y_offset() as i32, deltas, tags::SPYO),
            });
            metrics.strike_through = Some(Decoration {
                position: metric(os2.y_strikeout_position() as i32, deltas, tags::STRO),
                thickness: metric(os2.y_strikeout_size() as i32, deltas, tags::STRS),
            });
        }
        if let Ok(post) = tables.post() {
            metrics.underline = Some(Decoration {
                position: metric(
                    post.underline_position().to_i16() as i32,
                    deltas,
                    tags::UNDO,
                ),
                thickness: metric(
                    post.underline_thickness().to_i16() as i32,
                    deltas,
                    tags::UNDS,
                ),
            });
            metrics.italic_angle = Some(post.italic_angle());
            metrics.is_fixed_pitch = Some(post.is_fixed_pitch() != 0);
        }
        metrics
    }

    /// Reads the style measurements from a Type 1 font.
    ///
    /// Its underline center is converted to the top edge used by
    /// [`Decoration`]. Type 1 has no subscript, superscript or strikethrough.
    pub fn from_type1(font: &Type1Font) -> Self {
        // Type 1 locates the underline by its center, whereas `post` locates
        // its top edge. Keep the public decoration convention consistent.
        let underline_top = F48Dot16::from_i32(font.underline_position())
            + F48Dot16::from_bits((font.underline_thickness() as i64) << 15);
        Self {
            units_per_em: super::units_per_em(font.upem().clamp(0, u16::MAX as i32) as u16),
            underline: Some(Decoration {
                position: underline_top,
                thickness: F48Dot16::from_i32(font.underline_thickness()),
            }),
            italic_angle: Some(Fixed::from_i32(font.italic_angle())),
            is_fixed_pitch: Some(font.is_fixed_pitch()),
            ..Default::default()
        }
    }

    /// Reads style measurements from a standalone CFF font.
    pub fn from_cff(font: &CffFontRef<'_>) -> Self {
        let units_per_em = super::units_per_em(font.upem().clamp(0, u16::MAX as i32) as u16);
        let Some(metadata) = font.metadata() else {
            return Self {
                units_per_em,
                ..Self::default()
            };
        };
        let thickness = metadata.underline_thickness().to_f48dot16();
        Self {
            units_per_em,
            underline: Some(Decoration {
                position: metadata.underline_position().to_f48dot16()
                    + F48Dot16::from_bits(thickness.to_bits() >> 1),
                thickness,
            }),
            italic_angle: Some(metadata.italic_angle()),
            is_fixed_pitch: Some(metadata.is_fixed_pitch()),
            ..Default::default()
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::FontRef;

    #[test]
    fn reads_os2_and_post_without_losing_stated_zeroes() {
        let font = FontRef::new(font_test_data::TINOS_SUBSET).unwrap();
        let os2 = font.os2().unwrap();
        let post = font.post().unwrap();
        let metrics = StyleMetrics::from_sfnt(&font, &[]);
        let subscript = metrics.subscript.unwrap();
        assert_eq!(
            subscript.x_size,
            F48Dot16::from_i32(os2.y_subscript_x_size() as i32)
        );
        assert_eq!(
            subscript.y_size,
            F48Dot16::from_i32(os2.y_subscript_y_size() as i32)
        );
        assert_eq!(
            subscript.x_offset,
            F48Dot16::from_i32(os2.y_subscript_x_offset() as i32)
        );
        assert_eq!(
            subscript.y_offset,
            F48Dot16::from_i32(os2.y_subscript_y_offset() as i32)
        );
        let superscript = metrics.superscript.unwrap();
        assert_eq!(
            superscript.x_size,
            F48Dot16::from_i32(os2.y_superscript_x_size() as i32)
        );
        assert_eq!(
            superscript.y_size,
            F48Dot16::from_i32(os2.y_superscript_y_size() as i32)
        );
        assert_eq!(
            superscript.x_offset,
            F48Dot16::from_i32(os2.y_superscript_x_offset() as i32)
        );
        assert_eq!(
            superscript.y_offset,
            F48Dot16::from_i32(os2.y_superscript_y_offset() as i32)
        );
        assert_eq!(
            metrics.strike_through.unwrap().position,
            F48Dot16::from_i32(os2.y_strikeout_position() as i32)
        );
        assert_eq!(
            metrics.strike_through.unwrap().thickness,
            F48Dot16::from_i32(os2.y_strikeout_size() as i32)
        );
        assert_eq!(
            metrics.underline.unwrap().position,
            F48Dot16::from_i32(post.underline_position().to_i16() as i32)
        );
        assert_eq!(
            metrics.underline.unwrap().thickness,
            F48Dot16::from_i32(post.underline_thickness().to_i16() as i32)
        );
        assert_eq!(metrics.italic_angle, Some(post.italic_angle()));
        assert_eq!(metrics.is_fixed_pitch, Some(post.is_fixed_pitch() != 0));
    }

    #[test]
    fn absent_tables_leave_style_metrics_unset() {
        let font = FontRef::new(font_test_data::NAMES_ONLY).unwrap();
        // Without `head`, the measurements are in a 1000 unit em.
        let expected = StyleMetrics {
            units_per_em: 1000,
            ..StyleMetrics::default()
        };
        assert_eq!(StyleMetrics::from_sfnt(&font, &[]), expected);
    }

    // Scaling to a size uses the font's own units per em.
    #[test]
    fn scaling_to_a_size_uses_the_units_per_em() {
        use crate::model::{
            metrics::{Scale26Dot6, ScaleF32},
            Font,
        };
        use types::GlyphId;
        let font = Font::new(font_test_data::TINOS_SUBSET, 0).unwrap();
        let upem = font.units_per_em();
        let glyph = GlyphId::new(1);
        let (f32_scale, fixed_scale) = (
            ScaleF32::from_ppem(16.0, upem),
            Scale26Dot6::from_ppem(16.0, upem),
        );
        let metrics = font.metrics();
        assert_eq!(
            metrics.scaled_f32(16.0).h_line(),
            metrics.scaled(f32_scale).h_line()
        );
        assert_eq!(
            metrics.scaled_26dot6(16.0).h_line(),
            metrics.scaled(fixed_scale).h_line()
        );
        let style = font.style_metrics();
        assert_eq!(style.units_per_em, upem);
        assert_eq!(
            style.scaled_f32(16.0).underline(),
            style.scaled(f32_scale).underline()
        );
        assert_eq!(
            style.scaled_26dot6(16.0).underline(),
            style.scaled(fixed_scale).underline()
        );
        let glyphs = font.glyph_metrics();
        assert_eq!(
            glyphs.scaled_f32(16.0).h_advance(glyph),
            glyphs.scaled(f32_scale).h_advance(glyph)
        );
        assert_eq!(
            glyphs.scaled_26dot6(16.0).h_advance(glyph),
            glyphs.scaled(fixed_scale).h_advance(glyph)
        );
    }

    #[test]
    fn mvar_moves_the_strikethrough_position() {
        let font = FontRef::new(font_test_data::AMSTELVAR_AVAR2_A).unwrap();
        let coords = [NormalizedCoord::from_f32(1.0); 12];
        let default = StyleMetrics::from_sfnt(&font, &[]);
        let varied = StyleMetrics::from_sfnt(&font, &coords);
        let delta = font
            .mvar()
            .unwrap()
            .at(&coords)
            .unwrap()
            .get(tags::STRO)
            .unwrap();
        assert_ne!(delta, F48Dot16::ZERO);
        assert_eq!(
            varied.strike_through.unwrap().position - default.strike_through.unwrap().position,
            delta
        );
        assert_eq!(varied.italic_angle, default.italic_angle);
    }

    #[test]
    fn type1_exposes_its_font_info() {
        let font = Type1Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFB).unwrap();
        let metrics = StyleMetrics::from_type1(&font);
        assert_eq!(
            metrics.underline.unwrap().position,
            F48Dot16::from_i32(font.underline_position())
                + F48Dot16::from_bits((font.underline_thickness() as i64) << 15)
        );
        assert_eq!(
            metrics.underline.unwrap().thickness,
            F48Dot16::from_i32(font.underline_thickness())
        );
        assert_eq!(
            metrics.italic_angle,
            Some(Fixed::from_i32(font.italic_angle()))
        );
        assert_eq!(metrics.is_fixed_pitch, Some(font.is_fixed_pitch()));
        assert!(metrics.subscript.is_none());
    }

    #[test]
    #[cfg(feature = "experimental_font_api")]
    fn scales_lengths_on_their_own_axes_but_not_the_angle() {
        use crate::model::metrics::{ScaleF32, ScaledStyleMetrics};
        let metrics = StyleMetrics {
            subscript: Some(ScriptMetrics {
                x_size: F48Dot16::from_i32(100),
                y_size: F48Dot16::from_i32(200),
                x_offset: F48Dot16::from_i32(30),
                y_offset: F48Dot16::from_i32(-40),
            }),
            underline: Some(Decoration {
                position: F48Dot16::from_i32(-50),
                thickness: F48Dot16::from_i32(20),
            }),
            italic_angle: Some(Fixed::from_i32(-12)),
            ..Default::default()
        };
        let scaled: ScaledStyleMetrics<'_, _> = metrics.scaled(ScaleF32::new(100.0, 200.0, 1000));
        let subscript = scaled.subscript().unwrap();
        assert_eq!((subscript.x_size, subscript.y_size), (10.0, 40.0));
        assert_eq!((subscript.x_offset, subscript.y_offset), (3.0, -8.0));
        assert_eq!(scaled.underline().unwrap().position, -10.0);
        assert_eq!(scaled.underline().unwrap().thickness, 4.0);
        assert_eq!(scaled.italic_angle(), metrics.italic_angle);
    }
}
