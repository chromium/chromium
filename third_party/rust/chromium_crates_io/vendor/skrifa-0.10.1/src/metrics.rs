//! Global font and glyph specific metrics.
//!
//! Metrics are various measurements that define positioning and layout
//! characteristics for a font. They come in two flavors:
//!
//! * Global metrics: these are applicable to all glyphs in a font and generally
//! define values that are used for the layout of a collection of glyphs. For example,
//! the ascent, descent and leading values determine the position of the baseline where
//! a glyph should be rendered as well as the suggested spacing above and below it.
//!
//! * Glyph metrics: these apply to single glyphs. For example, the advance
//! width value describes the distance between two consecutive glyphs on a line.
//!
//! ### Selecting an "instance"
//! Both global and glyph specific metrics accept two additional pieces of information
//! to select the desired instance of a font:
//! * Size: represented by the [Size] type, this determines the scaling factor that is
//! applied to all metrics.
//! * Normalized variation coordinates: respresented by the [LocationRef] type,
//! these define the position in design space for a variable font. For a non-variable
//! font, these coordinates are ignored and you can pass [LocationRef::default()]
//! as an argument for this parameter.
//!

use read_fonts::{
    tables::{
        glyf::{CompositeGlyphFlags, Glyf, Glyph},
        gvar::Gvar,
        hmtx::LongMetric,
        hvar::Hvar,
        loca::Loca,
        os2::SelectionFlags,
    },
    types::{BigEndian, Fixed, GlyphId},
    TableProvider,
};

use super::instance::{LocationRef, NormalizedCoord, Size};

/// Type for a bounding box with single precision floating point coordinates.
pub type BoundingBox = read_fonts::types::BoundingBox<f32>;

/// Metrics for a text decoration.
///
/// This represents the suggested offset and thickness of an underline
/// or strikeout text decoration.
#[derive(Copy, Clone, PartialEq, Default, Debug)]
pub struct Decoration {
    /// Offset to the top of the decoration from the baseline.
    pub offset: f32,
    /// Thickness of the decoration.
    pub thickness: f32,
}

/// Metrics that apply to all glyphs in a font.
///
/// These are retrieved for a specific position in the design space.
///
/// This metrics here are derived from the following tables:
/// * [head](https://learn.microsoft.com/en-us/typography/opentype/spec/head): `units_per_em`, `bounds`
/// * [maxp](https://learn.microsoft.com/en-us/typography/opentype/spec/maxp): `glyph_count`
/// * [post](https://learn.microsoft.com/en-us/typography/opentype/spec/post): `is_monospace`, `italic_angle`, `underline`
/// * [OS/2](https://learn.microsoft.com/en-us/typography/opentype/spec/os2): `average_width`, `cap_height`,
/// `x_height`, `strikeout`, as well as the line metrics: `ascent`, `descent`, `leading` if the `USE_TYPOGRAPHIC_METRICS`
/// flag is set or the `hhea` line metrics are zero (the Windows metrics are used as a last resort).
/// * [hhea](https://learn.microsoft.com/en-us/typography/opentype/spec/hhea): `max_width`, as well as the line metrics:
/// `ascent`, `descent`, `leading` if they are non-zero and the `USE_TYPOGRAHIC_METRICS` flag is not set in the OS/2 table
///
/// For variable fonts, deltas are computed using the  [MVAR](https://learn.microsoft.com/en-us/typography/opentype/spec/MVAR)
/// table.
#[derive(Copy, Clone, PartialEq, Default, Debug)]
pub struct Metrics {
    /// Number of font design units per em unit.
    pub units_per_em: u16,
    /// Number of glyphs in the font.
    pub glyph_count: u16,
    /// True if the font is not proportionally spaced.
    pub is_monospace: bool,
    /// Italic angle in counter-clockwise degrees from the vertical. Zero for upright text,
    /// negative for text that leans to the right.
    pub italic_angle: f32,
    /// Distance from the baseline to the top of the alignment box.
    pub ascent: f32,
    /// Distance from the baseline to the bottom of the alignment box.
    pub descent: f32,
    /// Recommended additional spacing between lines.
    pub leading: f32,
    /// Distance from the baseline to the top of a typical English capital.
    pub cap_height: Option<f32>,
    /// Distance from the baseline to the top of the lowercase "x" or
    /// similar character.
    pub x_height: Option<f32>,
    /// Average width of all non-zero width characters in the font.
    pub average_width: Option<f32>,
    /// Maximum advance width of all characters in the font.
    pub max_width: Option<f32>,
    /// Metrics for an underline decoration.
    pub underline: Option<Decoration>,
    /// Metrics for a strikeout decoration.
    pub strikeout: Option<Decoration>,
    /// Union of minimum and maximum extents for all glyphs in the font.
    pub bounds: Option<BoundingBox>,
}

impl Metrics {
    /// Creates new metrics for the given font, size, and location in
    /// normalized variation space.
    pub fn new<'a>(
        font: &impl TableProvider<'a>,
        size: Size,
        location: impl Into<LocationRef<'a>>,
    ) -> Self {
        let head = font.head();
        let mut metrics = Metrics {
            units_per_em: head.map(|head| head.units_per_em()).unwrap_or_default(),
            ..Default::default()
        };
        let coords = location.into().coords();
        let scale = size.linear_scale(metrics.units_per_em);
        if let Ok(head) = font.head() {
            metrics.bounds = Some(BoundingBox {
                x_min: head.x_min() as f32 * scale,
                y_min: head.y_min() as f32 * scale,
                x_max: head.x_max() as f32 * scale,
                y_max: head.y_max() as f32 * scale,
            });
        }
        if let Ok(maxp) = font.maxp() {
            metrics.glyph_count = maxp.num_glyphs();
        }
        if let Ok(post) = font.post() {
            metrics.is_monospace = post.is_fixed_pitch() != 0;
            metrics.italic_angle = post.italic_angle().to_f64() as f32;
            metrics.underline = Some(Decoration {
                offset: post.underline_position().to_i16() as f32 * scale,
                thickness: post.underline_thickness().to_i16() as f32 * scale,
            });
        }
        let hhea = font.hhea();
        if let Ok(hhea) = &hhea {
            metrics.max_width = Some(hhea.advance_width_max().to_u16() as f32 * scale);
        }
        // Choosing proper line metrics is a challenge due to the changing
        // spec, backward compatibility and broken fonts.
        //
        // We use the same strategy as FreeType:
        // 1. Use the OS/2 metrics if the table exists and the USE_TYPO_METRICS
        //    flag is set.
        // 2. Otherwise, use the hhea metrics.
        // 3. If hhea metrics are zero and the OS/2 table exists:
        //    3a. Use the typo metrics if they are non-zero
        //    3b. Otherwise, use the win metrics
        //
        // See: https://github.com/freetype/freetype/blob/5c37b6406258ec0d7ab64b8619c5ea2c19e3c69a/src/sfnt/sfobjs.c#L1311
        let os2 = font.os2().ok();
        let mut used_typo_metrics = false;
        if let Some(os2) = &os2 {
            if os2
                .fs_selection()
                .contains(SelectionFlags::USE_TYPO_METRICS)
            {
                metrics.ascent = os2.s_typo_ascender() as f32 * scale;
                metrics.descent = os2.s_typo_descender() as f32 * scale;
                metrics.leading = os2.s_typo_line_gap() as f32 * scale;
                used_typo_metrics = true;
            }
            metrics.average_width = Some(os2.x_avg_char_width() as f32 * scale);
            metrics.cap_height = os2.s_cap_height().map(|v| v as f32 * scale);
            metrics.x_height = os2.sx_height().map(|v| v as f32 * scale);
            metrics.strikeout = Some(Decoration {
                offset: os2.y_strikeout_position() as f32 * scale,
                thickness: os2.y_strikeout_size() as f32 * scale,
            });
        }
        if !used_typo_metrics {
            if let Ok(hhea) = font.hhea() {
                metrics.ascent = hhea.ascender().to_i16() as f32 * scale;
                metrics.descent = hhea.descender().to_i16() as f32 * scale;
                metrics.leading = hhea.line_gap().to_i16() as f32 * scale;
            }
            if metrics.ascent == 0.0 && metrics.descent == 0.0 {
                if let Some(os2) = &os2 {
                    if os2.s_typo_ascender() != 0 || os2.s_typo_descender() != 0 {
                        metrics.ascent = os2.s_typo_ascender() as f32 * scale;
                        metrics.descent = os2.s_typo_descender() as f32 * scale;
                        metrics.leading = os2.s_typo_line_gap() as f32 * scale;
                    } else {
                        metrics.ascent = os2.us_win_ascent() as f32 * scale;
                        // Win descent is always positive while other descent values are negative. Negate it
                        // to ensure we return consistent metrics.
                        metrics.descent = -(os2.us_win_descent() as f32 * scale);
                    }
                }
            }
        }
        if let (Ok(mvar), true) = (font.mvar(), !coords.is_empty()) {
            use read_fonts::tables::mvar::tags::*;
            let metric_delta =
                |tag| mvar.metric_delta(tag, coords).unwrap_or_default().to_f64() as f32 * scale;
            metrics.ascent += metric_delta(HASC);
            metrics.descent += metric_delta(HDSC);
            metrics.leading += metric_delta(HLGP);
            if let Some(cap_height) = &mut metrics.cap_height {
                *cap_height += metric_delta(CPHT);
            }
            if let Some(x_height) = &mut metrics.x_height {
                *x_height += metric_delta(XHGT);
            }
            if let Some(underline) = &mut metrics.underline {
                underline.offset += metric_delta(UNDO);
                underline.thickness += metric_delta(UNDS);
            }
            if let Some(strikeout) = &mut metrics.strikeout {
                strikeout.offset += metric_delta(STRO);
                strikeout.thickness += metric_delta(STRS);
            }
        }
        metrics
    }
}

/// Glyph specific metrics.
#[derive(Clone)]
pub struct GlyphMetrics<'a> {
    glyph_count: u16,
    scale: f32,
    h_metrics: &'a [LongMetric],
    default_advance_width: u16,
    lsbs: &'a [BigEndian<i16>],
    hvar: Option<Hvar<'a>>,
    gvar: Option<Gvar<'a>>,
    loca_glyf: Option<(Loca<'a>, Glyf<'a>)>,
    coords: &'a [NormalizedCoord],
}

impl<'a> GlyphMetrics<'a> {
    /// Creates new glyph metrics from the given font, size, and location in
    /// normalized variation space.
    pub fn new(
        font: &impl TableProvider<'a>,
        size: Size,
        location: impl Into<LocationRef<'a>>,
    ) -> Self {
        let glyph_count = font
            .maxp()
            .map(|maxp| maxp.num_glyphs())
            .unwrap_or_default();
        let upem = font
            .head()
            .map(|head| head.units_per_em())
            .unwrap_or_default();
        let scale = size.linear_scale(upem);
        let coords = location.into().coords();
        let (h_metrics, default_advance_width, lsbs) = font
            .hmtx()
            .map(|hmtx| {
                let h_metrics = hmtx.h_metrics();
                let default_advance_width = h_metrics.last().map(|m| m.advance.get()).unwrap_or(0);
                let lsbs = hmtx.left_side_bearings();
                (h_metrics, default_advance_width, lsbs)
            })
            .unwrap_or_default();
        let hvar = font.hvar().ok();
        let gvar = font.gvar().ok();
        let loca_glyf = if let (Ok(loca), Ok(glyf)) = (font.loca(None), font.glyf()) {
            Some((loca, glyf))
        } else {
            None
        };
        Self {
            glyph_count,
            scale,
            h_metrics,
            default_advance_width,
            lsbs,
            hvar,
            gvar,
            loca_glyf,
            coords,
        }
    }

    /// Returns the number of available glyphs in the font.
    pub fn glyph_count(&self) -> u16 {
        self.glyph_count
    }

    /// Returns the advance width for the specified glyph.
    ///
    /// If normalized coordinates were provided when constructing glyph metrics and
    /// an `HVAR` table is present, applies the appropriate delta.
    pub fn advance_width(&self, glyph_id: GlyphId) -> Option<f32> {
        if glyph_id.to_u16() >= self.glyph_count {
            return None;
        }
        let mut advance = self
            .h_metrics
            .get(glyph_id.to_u16() as usize)
            .map(|metric| metric.advance())
            .unwrap_or(self.default_advance_width) as i32;
        if let Some(hvar) = &self.hvar {
            advance += hvar
                .advance_width_delta(glyph_id, self.coords)
                // FreeType truncates metric deltas...
                // https://github.com/freetype/freetype/blob/7838c78f53f206ac5b8e9cefde548aa81cb00cf4/src/truetype/ttgxvar.c#L1027
                .map(|delta| delta.to_f64() as i32)
                .unwrap_or(0);
        } else if self.gvar.is_some() {
            advance += self.metric_deltas_from_gvar(glyph_id)[1];
        }
        Some(advance as f32 * self.scale)
    }

    /// Returns the left side bearing for the specified glyph.
    ///
    /// If normalized coordinates were provided when constructing glyph metrics and
    /// an `HVAR` table is present, applies the appropriate delta.
    pub fn left_side_bearing(&self, glyph_id: GlyphId) -> Option<f32> {
        if glyph_id.to_u16() >= self.glyph_count {
            return None;
        }
        let gid_index = glyph_id.to_u16() as usize;
        let mut lsb = self
            .h_metrics
            .get(gid_index)
            .map(|metric| metric.side_bearing())
            .unwrap_or_else(|| {
                self.lsbs
                    .get(gid_index.saturating_sub(self.h_metrics.len()))
                    .map(|lsb| lsb.get())
                    .unwrap_or_default()
            }) as i32;
        if let Some(hvar) = &self.hvar {
            lsb += hvar
                .lsb_delta(glyph_id, self.coords)
                // FreeType truncates metric deltas...
                // https://github.com/freetype/freetype/blob/7838c78f53f206ac5b8e9cefde548aa81cb00cf4/src/truetype/ttgxvar.c#L1027
                .map(|delta| delta.to_f64() as i32)
                .unwrap_or(0);
        } else if self.gvar.is_some() {
            lsb += self.metric_deltas_from_gvar(glyph_id)[0];
        }
        Some(lsb as f32 * self.scale)
    }

    /// Returns the bounding box for the specified glyph.
    ///
    /// Note that variations are not reflected in the bounding box returned by
    /// this method.
    pub fn bounds(&self, glyph_id: GlyphId) -> Option<BoundingBox> {
        let (loca, glyf) = self.loca_glyf.as_ref()?;
        Some(match loca.get_glyf(glyph_id, glyf).ok()? {
            Some(glyph) => BoundingBox {
                x_min: glyph.x_min() as f32 * self.scale,
                y_min: glyph.y_min() as f32 * self.scale,
                x_max: glyph.x_max() as f32 * self.scale,
                y_max: glyph.y_max() as f32 * self.scale,
            },
            // Empty glyphs have an empty bounding box
            None => BoundingBox::default(),
        })
    }
}

impl<'a> GlyphMetrics<'a> {
    fn metric_deltas_from_gvar(&self, glyph_id: GlyphId) -> [i32; 2] {
        GvarMetricDeltas::new(self)
            .and_then(|metric_deltas| metric_deltas.compute_deltas(glyph_id))
            .unwrap_or_default()
    }
}

struct GvarMetricDeltas<'a> {
    loca: Loca<'a>,
    glyf: Glyf<'a>,
    gvar: Gvar<'a>,
    coords: &'a [NormalizedCoord],
}

impl<'a> GvarMetricDeltas<'a> {
    fn new(metrics: &GlyphMetrics<'a>) -> Option<Self> {
        let (loca, glyf) = metrics.loca_glyf.clone()?;
        let gvar = metrics.gvar.clone()?;
        Some(Self {
            loca,
            glyf,
            gvar,
            coords: metrics.coords,
        })
    }

    /// Returns [lsb_delta, advance_delta]
    fn compute_deltas(&self, glyph_id: GlyphId) -> Option<[i32; 2]> {
        // For any given glyph, there's only one outline that contributes to
        // metrics deltas (via "phantom points"). For simple glyphs, that is
        // the glyph itself. For composite glyphs, it is the first component
        // in the tree that has the USE_MY_METRICS flag set or, if there are
        // none, the composite glyph itself.
        //
        // This searches for the glyph that meets that criteria and also
        // returns the point count (for composites, this is the component
        // count), so that we know where the deltas for phantom points start
        // in the variation data.
        let (glyph_id, point_count) = self.find_glyph_and_point_count(glyph_id, 0)?;
        // [left_extent_delta, right_extent_delta]
        let mut metric_deltas = [Fixed::ZERO; 2];
        let phantom_range = point_count..point_count + 2;
        let var_data = self.gvar.glyph_variation_data(glyph_id).ok()?;
        // Note that phantom points can never belong to a contour so we don't have
        // to handle the IUP case here.
        for (tuple, scalar) in var_data.active_tuples_at(self.coords) {
            for tuple_delta in tuple.deltas() {
                let ix = tuple_delta.position as usize;
                if phantom_range.contains(&ix) {
                    metric_deltas[ix - phantom_range.start] += tuple_delta.apply_scalar(scalar).x;
                }
            }
        }
        metric_deltas[1] -= metric_deltas[0];
        Some(metric_deltas.map(|x| x.to_i32()))
    }

    /// Returns the glyph id and associated point count that determines the
    /// metrics for the requested glyph.
    fn find_glyph_and_point_count(
        &self,
        glyph_id: GlyphId,
        recurse_depth: usize,
    ) -> Option<(GlyphId, usize)> {
        if recurse_depth > crate::GLYF_COMPOSITE_RECURSION_LIMIT {
            return None;
        }
        let glyph = self.loca.get_glyf(glyph_id, &self.glyf).ok()??;
        match glyph {
            Glyph::Simple(simple) => {
                // Simple glyphs always use their own metrics
                Some((glyph_id, simple.num_points()))
            }
            Glyph::Composite(composite) => {
                // For composite glyphs, if one of the components has the
                // USE_MY_METRICS flag set, recurse into the glyph referenced
                // by that component. Otherwise, return the composite glyph
                // itself and the number of components as the point count.
                let mut count = 0;
                for component in composite.components() {
                    count += 1;
                    if component
                        .flags
                        .contains(CompositeGlyphFlags::USE_MY_METRICS)
                    {
                        return self.find_glyph_and_point_count(component.glyph, recurse_depth + 1);
                    }
                }
                Some((glyph_id, count))
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::MetadataProvider as _;
    use font_test_data::{SIMPLE_GLYF, VAZIRMATN_VAR};
    use read_fonts::FontRef;

    #[test]
    fn metrics() {
        let font = FontRef::new(SIMPLE_GLYF).unwrap();
        let metrics = font.metrics(Size::unscaled(), LocationRef::default());
        let expected = Metrics {
            units_per_em: 1024,
            glyph_count: 3,
            bounds: Some(BoundingBox {
                x_min: 51.0,
                y_min: -250.0,
                x_max: 998.0,
                y_max: 950.0,
            }),
            average_width: Some(1275.0),
            max_width: None,
            x_height: Some(512.0),
            cap_height: Some(717.0),
            is_monospace: false,
            italic_angle: 0.0,
            ascent: 950.0,
            descent: -250.0,
            leading: 0.0,
            underline: None,
            strikeout: Some(Decoration {
                offset: 307.0,
                thickness: 51.0,
            }),
        };
        assert_eq!(metrics, expected);
    }

    #[test]
    fn metrics_missing_os2() {
        let font = FontRef::new(VAZIRMATN_VAR).unwrap();
        let metrics = font.metrics(Size::unscaled(), LocationRef::default());
        let expected = Metrics {
            units_per_em: 2048,
            glyph_count: 4,
            bounds: Some(BoundingBox {
                x_min: 29.0,
                y_min: 0.0,
                x_max: 1310.0,
                y_max: 1847.0,
            }),
            average_width: None,
            max_width: Some(1336.0),
            x_height: None,
            cap_height: None,
            is_monospace: false,
            italic_angle: 0.0,
            ascent: 2100.0,
            descent: -1100.0,
            leading: 0.0,
            underline: None,
            strikeout: None,
        };
        assert_eq!(metrics, expected);
    }

    #[test]
    fn glyph_metrics() {
        let font = FontRef::new(VAZIRMATN_VAR).unwrap();
        let glyph_metrics = font.glyph_metrics(Size::unscaled(), LocationRef::default());
        // (advance_width, lsb) in glyph order
        let expected = &[
            (908.0, 100.0),
            (1336.0, 29.0),
            (1336.0, 29.0),
            (633.0, 57.0),
        ];
        let result = (0..4)
            .map(|i| {
                let gid = GlyphId::new(i as u16);
                let advance_width = glyph_metrics.advance_width(gid).unwrap();
                let lsb = glyph_metrics.left_side_bearing(gid).unwrap();
                (advance_width, lsb)
            })
            .collect::<Vec<_>>();
        assert_eq!(expected, &result[..]);
    }

    #[test]
    fn glyph_metrics_var() {
        let font = FontRef::new(VAZIRMATN_VAR).unwrap();
        let coords = &[NormalizedCoord::from_f32(-0.8)];
        let glyph_metrics = font.glyph_metrics(Size::unscaled(), LocationRef::new(coords));
        // (advance_width, lsb) in glyph order
        let expected = &[
            (908.0, 100.0),
            (1246.0, 29.0),
            (1246.0, 29.0),
            (556.0, 57.0),
        ];
        let result = (0..4)
            .map(|i| {
                let gid = GlyphId::new(i as u16);
                let advance_width = glyph_metrics.advance_width(gid).unwrap();
                let lsb = glyph_metrics.left_side_bearing(gid).unwrap();
                (advance_width, lsb)
            })
            .collect::<Vec<_>>();
        assert_eq!(expected, &result[..]);
    }

    #[test]
    fn glyph_metrics_missing_hvar() {
        let font = FontRef::new(VAZIRMATN_VAR).unwrap();
        let glyph_count = font.maxp().unwrap().num_glyphs();
        // Test a few different locations in variation space
        for coord in [-1.0, -0.8, 0.0, 0.75, 1.0] {
            let coords = &[NormalizedCoord::from_f32(coord)];
            let location = LocationRef::new(coords);
            let glyph_metrics = font.glyph_metrics(Size::unscaled(), location);
            let mut glyph_metrics_no_hvar = glyph_metrics.clone();
            // Setting hvar to None forces use of gvar for metric deltas
            glyph_metrics_no_hvar.hvar = None;
            for gid in 0..glyph_count {
                let gid = GlyphId::new(gid);
                assert_eq!(
                    glyph_metrics.advance_width(gid),
                    glyph_metrics_no_hvar.advance_width(gid)
                );
                assert_eq!(
                    glyph_metrics.left_side_bearing(gid),
                    glyph_metrics_no_hvar.left_side_bearing(gid)
                );
            }
        }
    }
}
