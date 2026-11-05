//! Font-wide metrics.

use super::metric;
use crate::model::variation::NormalizedCoord;

use crate::{
    ps::{cff::CffFontRef, type1::Type1Font},
    tables::{mvar::tags, os2::SelectionFlags},
    TableProvider,
};
use types::{BoundingBox, F48Dot16};

/// The two ends of a line, in whatever unit a caller uses.
///
/// Separate from [`LineBox`] because the gap after a line is not part of
/// where it begins and ends, and the metrics built on a line use only the
/// two ends.
#[derive(Copy, Clone, Default, PartialEq, Eq, Debug)]
pub struct LineExtents<T> {
    /// Height of the line above the baseline. Never negative.
    pub ascender: T,
    /// Depth of the line below the baseline, as a negative value. Never
    /// positive.
    pub descender: T,
}

/// Ascender, descender and line gap from one table.
///
/// In font units unless a caller names another unit. The ascender is above
/// the baseline and the descender below it, whatever sign a font stores
/// them with.
#[derive(Copy, Clone, Default, PartialEq, Eq, Debug)]
pub struct LineBox<T = F48Dot16> {
    /// Height of the line above the baseline. Never negative.
    pub ascender: T,
    /// Depth of the line below the baseline, as a negative value. Never
    /// positive.
    pub descender: T,
    /// Extra space between the bottom of one line and the top of the next.
    pub line_gap: T,
}

impl<T: Copy> LineBox<T> {
    /// Returns where this line begins and ends, without the gap after it.
    ///
    /// Text stacks by the two ends; the gap separates one run of stacked
    /// text from the next.
    pub fn extents(&self) -> LineExtents<T> {
        LineExtents {
            ascender: self.ascender,
            descender: self.descender,
        }
    }
}

impl LineBox {
    /// Returns `true` if either end of the line is away from the baseline.
    ///
    /// Some fonts leave both ends at zero, and the line is then not used.
    pub(crate) fn has_nonzero_extent(&self) -> bool {
        self.ascender != F48Dot16::ZERO || self.descender != F48Dot16::ZERO
    }
}

/// Font-wide metrics.
///
/// Read from `head`, `maxp`, `hhea`, `vhea` and `OS/2`, which together are a
/// few hundred bytes.
///
/// A font has three sets of line metrics, and they often differ.
/// [`hhea_line`](Self::hhea_line) is what macOS reads,
/// [`win_line`](Self::win_line) is what Windows reads, and [`typo_line`](Self::typo_line) is what the
/// font asks for when [`use_typo_metrics`](Self::use_typo_metrics) is set.
/// Choosing between them is left to the caller.
///
/// A field missing from the font is `None`, which is distinct from a zero
/// value.
///
/// `MVAR` varies most of these, so a set belongs to the location it was read
/// at. The units per em, the counts, the bounding box, the widest and tallest
/// advance and the average width cannot vary.
///
/// # Units
///
/// Measurements are in font units. They are [`F48Dot16`] because a
/// location can move them off a whole unit, and rounding here would decide
/// for every caller how that fraction is spent.
///
/// The two axes do not scale alike: [`vhea_line`](Self::vhea_line) is
/// measured across the vertical baseline rather than along it, so it scales
/// on x where the horizontal sets scale on y.
#[derive(Copy, Clone, Default, PartialEq, Eq, Debug)]
pub struct Metrics {
    /// Design units per em, which every measurement of the font uses.
    ///
    /// Always between 16 and 16384. A font with a value outside that range,
    /// or with no `head`, uses 1000.
    pub units_per_em: u16,
    /// Design units per em as written in `head`, or zero if the font has no
    /// `head`.
    ///
    /// Differs from [`units_per_em`](Self::units_per_em) only for a font with
    /// an unusable value, which a caller may choose to reject.
    pub raw_units_per_em: u16,
    /// Number of glyphs in the font.
    pub num_glyphs: u32,
    /// Box enclosing every glyph in the font.
    pub bounds: BoundingBox<F48Dot16>,
    /// Line metrics from `hhea`.
    pub hhea_line: Option<LineBox>,
    /// Widest advance of any glyph.
    pub max_h_advance: Option<F48Dot16>,
    /// Line metrics from `vhea`, measured across the vertical baseline.
    pub vhea_line: Option<LineBox>,
    /// Tallest advance of any glyph.
    pub max_v_advance: Option<F48Dot16>,
    /// Typographic line metrics, from `OS/2`.
    pub typo_line: Option<LineBox>,
    /// Whether the font asks for [`typo_line`](Self::typo_line) to be used
    /// in preference to [`hhea_line`](Self::hhea_line).
    pub use_typo_metrics: bool,
    /// The line outside which the font asks not to be clipped.
    ///
    /// `OS/2` gives the descent as a positive number below the baseline.
    /// This negates it, so the pair describes a line like every other pair
    /// here.
    pub win_line: Option<LineExtents<F48Dot16>>,
    /// Height of a lowercase x.
    pub x_height: Option<F48Dot16>,
    /// Height of a capital letter.
    pub cap_height: Option<F48Dot16>,
    /// Average glyph width, as the font reports it.
    pub average_char_width: Option<F48Dot16>,
}

impl Metrics {
    /// Reads the metrics from `tables` at `normalized_coords`.
    ///
    /// A missing or unreadable table leaves the metrics that come from it
    /// unset rather than failing.
    ///
    /// Pass an empty location for the font's defaults. At any other location
    /// `MVAR` is read and its deltas applied.
    pub fn from_sfnt<'a>(
        tables: &impl TableProvider<'a>,
        normalized_coords: &[NormalizedCoord],
    ) -> Self {
        let coords = normalized_coords;
        // Only at a location, so that the common case does not reach for a
        // table it has nothing to ask.
        let mvar = (!coords.is_empty()).then(|| tables.mvar().ok()).flatten();
        let deltas = mvar.as_ref().and_then(|mvar| mvar.at(coords));
        let deltas = deltas.as_ref();
        let mut metrics = Self::default();
        let head = tables.head();
        metrics.raw_units_per_em = head.as_ref().map_or(0, |head| head.units_per_em());
        metrics.units_per_em = units_per_em(metrics.raw_units_per_em);
        if let Ok(head) = head {
            metrics.bounds = BoundingBox {
                x_min: F48Dot16::from_i32(head.x_min() as i32),
                y_min: F48Dot16::from_i32(head.y_min() as i32),
                x_max: F48Dot16::from_i32(head.x_max() as i32),
                y_max: F48Dot16::from_i32(head.y_max() as i32),
            };
        }
        if let Ok(maxp) = tables.selected_maxp() {
            metrics.num_glyphs = maxp.num_glyphs();
        }
        if let Ok(hhea) = tables.selected_hhea() {
            // `MVAR` names one horizontal ascender, descender and line gap
            // between them, so the same delta applies to whichever of these
            // two sets a caller goes on to believe.
            metrics.hhea_line = Some(line_box(
                metric(hhea.ascender().to_i16() as i32, deltas, tags::HASC),
                metric(hhea.descender().to_i16() as i32, deltas, tags::HDSC),
                metric(hhea.line_gap().to_i16() as i32, deltas, tags::HLGP),
            ));
            metrics.max_h_advance =
                Some(F48Dot16::from_i32(hhea.advance_width_max().to_u16() as i32));
        }
        if let Ok(vhea) = tables.selected_vhea() {
            metrics.vhea_line = Some(line_box(
                metric(vhea.ascender().to_i16() as i32, deltas, tags::VASC),
                metric(vhea.descender().to_i16() as i32, deltas, tags::VDSC),
                metric(vhea.line_gap().to_i16() as i32, deltas, tags::VLGP),
            ));
            metrics.max_v_advance =
                Some(F48Dot16::from_i32(vhea.advance_height_max().to_u16() as i32));
        }
        if let Ok(os2) = tables.os2() {
            metrics.typo_line = Some(line_box(
                metric(os2.s_typo_ascender() as i32, deltas, tags::HASC),
                metric(os2.s_typo_descender() as i32, deltas, tags::HDSC),
                metric(os2.s_typo_line_gap() as i32, deltas, tags::HLGP),
            ));
            metrics.win_line = Some(LineExtents {
                ascender: metric(os2.us_win_ascent() as i32, deltas, tags::HCLA),
                descender: -metric(os2.us_win_descent() as i32, deltas, tags::HCLD),
            });
            metrics.use_typo_metrics = os2
                .fs_selection()
                .contains(SelectionFlags::USE_TYPO_METRICS);
            metrics.x_height = os2
                .sx_height()
                .map(|value| metric(value as i32, deltas, tags::XHGT));
            metrics.cap_height = os2
                .s_cap_height()
                .map(|value| metric(value as i32, deltas, tags::CPHT));
            metrics.average_char_width = Some(F48Dot16::from_i32(os2.x_avg_char_width() as i32));
        }
        metrics
    }

    /// Returns the ascender, descender and gap for horizontal text.
    ///
    /// The line is the first of these that applies:
    ///
    /// 1. The typographic line, if `OS/2` sets `USE_TYPO_METRICS`.
    /// 2. The `hhea` line, if either end is nonzero.
    /// 3. The typographic line, if either end is nonzero.
    /// 4. The `OS/2` clipping line, with no gap.
    ///
    /// Returns `None` if the font has no line metrics.
    pub fn h_line(&self) -> Option<LineBox> {
        match (self.hhea_line, self.typo_line) {
            (_, Some(typo)) if self.use_typo_metrics => Some(typo),
            (Some(hhea), _) if hhea.has_nonzero_extent() => Some(hhea),
            (_, Some(typo)) if typo.has_nonzero_extent() => Some(typo),
            // The clipping line has no gap between one line and the next, so
            // there is none to report.
            (hhea, _) => self
                .win_line
                .map(|win| LineBox {
                    ascender: win.ascender,
                    descender: win.descender,
                    line_gap: F48Dot16::ZERO,
                })
                .or(hhea),
        }
    }

    /// Reads the metrics from a Type 1 font.
    ///
    /// Such a font has only a bounding box, a glyph count and a matrix. The
    /// ascender and descender are the top and bottom of that box. Everything
    /// else is unset.
    pub fn from_type1(font: &Type1Font) -> Self {
        let bbox = font.bbox();
        let bounds = BoundingBox {
            x_min: bbox.x_min.to_f48dot16(),
            y_min: bbox.y_min.to_f48dot16(),
            x_max: bbox.x_max.to_f48dot16(),
            y_max: bbox.y_max.to_f48dot16(),
        };
        let raw_units_per_em = font.upem().clamp(0, u16::MAX as i32) as u16;
        Self {
            units_per_em: units_per_em(raw_units_per_em),
            raw_units_per_em,
            num_glyphs: font.num_glyphs(),
            bounds,
            hhea_line: Some(LineBox {
                ascender: bounds.y_max,
                descender: bounds.y_min,
                line_gap: F48Dot16::ZERO,
            }),
            ..Default::default()
        }
    }

    /// Reads the metrics from a standalone CFF font.
    pub fn from_cff(font: &CffFontRef<'_>) -> Self {
        let raw_units_per_em = font.upem().clamp(0, u16::MAX as i32) as u16;
        let mut metrics = Self {
            units_per_em: units_per_em(raw_units_per_em),
            raw_units_per_em,
            num_glyphs: font.num_glyphs(),
            ..Default::default()
        };
        if let Some(metadata) = font.metadata() {
            let bbox = metadata.bbox();
            metrics.bounds = BoundingBox {
                x_min: bbox.x_min.to_f48dot16(),
                y_min: bbox.y_min.to_f48dot16(),
                x_max: bbox.x_max.to_f48dot16(),
                y_max: bbox.y_max.to_f48dot16(),
            };
            metrics.hhea_line = Some(LineBox {
                ascender: metrics.bounds.y_max,
                descender: metrics.bounds.y_min,
                line_gap: F48Dot16::ZERO,
            });
        }
        metrics
    }
}

/// Returns the design units per em to use for a font whose `head` holds
/// `stated`.
///
/// This is `stated` if it is between 16 and 16384, and 1000 otherwise, which
/// matches HarfBuzz.
/// Returns a line with its ascender above the baseline and its descender
/// below it.
///
/// The `hhea`, `vhea` and `OS/2` descenders are negative by the
/// specification, but some fonts store them positive, which would put the
/// bottom of the line above the baseline. Either end is read by its size
/// alone. The gap is kept as stored.
fn line_box(ascender: F48Dot16, descender: F48Dot16, line_gap: F48Dot16) -> LineBox {
    LineBox {
        ascender: ascender.abs(),
        descender: -descender.abs(),
        line_gap,
    }
}

pub(crate) fn units_per_em(stated: u16) -> u16 {
    if (16..=16384).contains(&stated) {
        stated
    } else {
        1000
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::FontRef;

    /// Returns Tinos with the sign of its `hhea` descender flipped, as some
    /// fonts store it.
    fn positive_descender() -> Vec<u8> {
        let mut data = font_test_data::TINOS_SUBSET.to_vec();
        let font = FontRef::new(font_test_data::TINOS_SUBSET).unwrap();
        let record = font
            .table_directory()
            .table_records()
            .iter()
            .find(|record| record.tag() == types::Tag::new(b"hhea"))
            .unwrap();
        // The descender follows the version and the ascender.
        let at = record.offset() as usize + 6;
        let descender = i16::from_be_bytes([data[at], data[at + 1]]);
        assert!(descender < 0);
        data[at..at + 2].copy_from_slice(&(-descender).to_be_bytes());
        data
    }

    // A descender stored positive is read below the baseline, as is the
    // vertical advance of a font without `vmtx`, which spans the line.
    #[test]
    fn descenders_stored_positive_are_below_the_baseline() {
        use crate::model::Font;
        use alloc::vec::Vec;
        use types::GlyphId;
        let flipped = positive_descender();
        let stored = FontRef::new(&flipped).unwrap().hhea().unwrap().descender();
        assert!(stored.to_i16() > 0);
        let original =
            Metrics::from_sfnt(&FontRef::new(font_test_data::TINOS_SUBSET).unwrap(), &[]);
        let metrics = Metrics::from_sfnt(&FontRef::new(&flipped).unwrap(), &[]);
        assert_eq!(metrics.hhea_line, original.hhea_line);
        assert_eq!(metrics.h_line(), original.h_line());
        assert!(metrics.h_line().unwrap().descender < F48Dot16::ZERO);
        let [original, flipped] = [font_test_data::TINOS_SUBSET.to_vec(), flipped]
            .map(|data| Font::new(data, 0).unwrap());
        assert!(flipped.tables().vmtx().is_err());
        let advances = |font: &Font| -> Vec<_> {
            (0..font.num_glyphs())
                .map(|gid| font.glyph_metrics().v_advance_exact(GlyphId::new(gid)))
                .collect()
        };
        assert_eq!(advances(&flipped), advances(&original));
    }

    fn line(ascender: i32, descender: i32) -> LineBox {
        LineBox {
            ascender: F48Dot16::from_i32(ascender),
            descender: F48Dot16::from_i32(descender),
            line_gap: F48Dot16::ZERO,
        }
    }

    #[test]
    fn the_typographic_line_is_read_when_the_font_asks() {
        let metrics = Metrics {
            hhea_line: Some(line(800, -200)),
            typo_line: Some(line(750, -250)),
            use_typo_metrics: true,
            ..Default::default()
        };
        assert_eq!(metrics.h_line(), Some(line(750, -250)));
    }

    #[test]
    fn hhea_is_read_when_it_does_not() {
        let metrics = Metrics {
            hhea_line: Some(line(800, -200)),
            typo_line: Some(line(750, -250)),
            use_typo_metrics: false,
            ..Default::default()
        };
        assert_eq!(metrics.h_line(), Some(line(800, -200)));
    }

    #[test]
    fn a_silent_hhea_gives_way_to_the_typographic_line() {
        let metrics = Metrics {
            hhea_line: Some(line(0, 0)),
            typo_line: Some(line(750, -250)),
            use_typo_metrics: false,
            ..Default::default()
        };
        assert_eq!(metrics.h_line(), Some(line(750, -250)));
    }

    #[test]
    fn a_font_silent_twice_over_falls_back_to_the_clipping_line() {
        // Arial Narrow Bold is the case: zeroed typographic metrics beside
        // usable clipping ones, where its siblings state all three.
        let metrics = Metrics {
            hhea_line: Some(line(0, 0)),
            typo_line: Some(line(0, 0)),
            win_line: Some(LineExtents {
                ascender: F48Dot16::from_i32(905),
                descender: F48Dot16::from_i32(-212),
            }),
            use_typo_metrics: false,
            ..Default::default()
        };
        // The clipping metrics state a descent below the baseline as a
        // positive number, so it is negated to describe the same line.
        assert_eq!(metrics.h_line(), Some(line(905, -212)));
    }

    #[test]
    fn a_font_stating_no_line_reports_none() {
        assert_eq!(Metrics::default().h_line(), None);
    }

    /// A horizontal font with `OS/2` and `post`, but no `vhea`.
    fn horizontal() -> FontRef<'static> {
        FontRef::new(font_test_data::TINOS_SUBSET).unwrap()
    }

    /// A font set vertically as well, so it has `vhea`.
    fn vertical() -> FontRef<'static> {
        FontRef::new(font_test_data::VORG).unwrap()
    }

    /// A variable font whose `MVAR` varies its line metrics, among others.
    fn variable() -> FontRef<'static> {
        FontRef::new(font_test_data::AMSTELVAR_AVAR2_A).unwrap()
    }

    /// The far end of every one of that font's twelve axes.
    fn far() -> [NormalizedCoord; 12] {
        [NormalizedCoord::from_f32(1.0); 12]
    }

    #[test]
    fn the_three_sets_of_line_metrics_are_kept_apart() {
        let metrics = Metrics::from_sfnt(&horizontal(), &[]);
        assert_eq!(metrics.units_per_em, 2048);
        let hhea = metrics.hhea_line.unwrap();
        assert!(hhea.ascender > F48Dot16::ZERO);
        assert!(hhea.descender < F48Dot16::ZERO);
        // The typographic set descends the same way `hhea` does.
        let typo = metrics.typo_line.unwrap();
        assert!(typo.ascender > F48Dot16::ZERO);
        assert!(typo.descender < F48Dot16::ZERO);
        // The Windows pair does not: both are stated as positive extents.
        let win = metrics.win_line.unwrap();
        assert!(win.ascender > F48Dot16::ZERO);
        assert!(win.descender < F48Dot16::ZERO);
    }

    #[test]
    fn a_font_without_a_table_leaves_its_measurements_unset() {
        // A font with no `vhea`, which most horizontal fonts are.
        let metrics = Metrics::from_sfnt(&horizontal(), &[]);
        assert!(metrics.vhea_line.is_none());
        assert!(metrics.max_v_advance.is_none());
        // And one that has it.
        let metrics = Metrics::from_sfnt(&vertical(), &[]);
        assert!(metrics.vhea_line.is_some());
        assert!(metrics.max_v_advance.unwrap() > F48Dot16::ZERO);
    }

    #[test]
    fn a_font_with_nothing_to_read_gives_up_rather_than_failing() {
        let metrics = Metrics::from_sfnt(&FontRef::new(font_test_data::NAMES_ONLY).unwrap(), &[]);
        // The font has no `head`, so only the em is set.
        assert_eq!(
            metrics,
            Metrics {
                units_per_em: 1000,
                ..Default::default()
            }
        );
    }

    #[test]
    fn a_font_stating_an_unusable_em_is_measured_on_a_thousand_units() {
        let data = font_test_data::TINOS_SUBSET;
        let reference = FontRef::new(data).unwrap();
        let head = reference
            .table_directory()
            .table_records()
            .iter()
            .find(|record| record.tag() == types::Tag::new(b"head"))
            .unwrap()
            .offset() as usize;
        assert_eq!(Metrics::from_sfnt(&reference, &[]).units_per_em, 2048);
        for stated in [0u16, 15, 16385, u16::MAX] {
            let mut data = data.to_vec();
            // The em follows the version, revision, checksum, magic and flags.
            data[head + 18..head + 20].copy_from_slice(&stated.to_be_bytes());
            let font = FontRef::new(&data).unwrap();
            let metrics = Metrics::from_sfnt(&font, &[]);
            assert_eq!(metrics.units_per_em, 1000, "stated {stated}");
            assert_eq!(metrics.raw_units_per_em, stated, "stated {stated}");
        }
        for stated in [16u16, 16384] {
            assert_eq!(units_per_em(stated), stated);
        }
    }

    #[test]
    fn a_default_location_leaves_every_measurement_on_a_whole_unit() {
        // Nothing has moved them off one, so each is exactly what its table
        // states.
        let metrics = Metrics::from_sfnt(&horizontal(), &[]);
        let ascender = metrics.hhea_line.unwrap().ascender;
        assert_eq!(ascender, F48Dot16::from_i32(ascender.to_i32()));
    }

    #[test]
    fn a_location_moves_what_mvar_varies() {
        let font = variable();
        let default = Metrics::from_sfnt(&font, &[]);
        let far = Metrics::from_sfnt(&font, &far());
        assert_ne!(default, far);
        // This font varies its clipping extents and both of its heights.
        assert_ne!(default.win_line, far.win_line);
        assert_ne!(default.x_height, far.x_height);
        assert_ne!(default.cap_height, far.cap_height);
    }

    #[test]
    fn the_two_horizontal_line_sets_move_together() {
        // `MVAR` names one horizontal ascender between `hhea` and the
        // typographic set, so a location moves both by the same amount.
        let font = variable();
        let default = Metrics::from_sfnt(&font, &[]);
        let far = Metrics::from_sfnt(&font, &far());
        let shift = far.hhea_line.unwrap().ascender - default.hhea_line.unwrap().ascender;
        assert_ne!(shift, F48Dot16::ZERO);
        assert_eq!(
            far.typo_line.unwrap().ascender - default.typo_line.unwrap().ascender,
            shift
        );
    }

    #[test]
    fn a_delta_keeps_the_fraction_the_variation_store_computed() {
        // The whole reason these are not integers: some of this font's
        // metrics land off a whole design unit, and rounding them here would
        // decide for every caller how that fraction is spent.
        // Part way along the axes, where the region scalars are fractions;
        // at the far end of every one they are exactly one and the deltas
        // land on whole units by luck rather than by design.
        let far = Metrics::from_sfnt(&variable(), &[NormalizedCoord::from_f32(0.4); 12]);
        let moved = [
            far.hhea_line.unwrap().ascender,
            far.hhea_line.unwrap().descender,
            far.win_line.unwrap().ascender,
            far.win_line.unwrap().descender,
            far.x_height.unwrap(),
            far.cap_height.unwrap(),
        ];
        assert!(moved
            .iter()
            .any(|value| *value != F48Dot16::from_i32(value.to_i32())));
    }

    #[test]
    fn what_mvar_cannot_say_does_not_move() {
        let font = variable();
        let default = Metrics::from_sfnt(&font, &[]);
        let far = Metrics::from_sfnt(&font, &far());
        assert_eq!(default.units_per_em, far.units_per_em);
        assert_eq!(default.num_glyphs, far.num_glyphs);
        assert_eq!(default.bounds, far.bounds);
        assert_eq!(default.max_h_advance, far.max_h_advance);
        assert_eq!(default.average_char_width, far.average_char_width);
    }

    #[test]
    fn a_type1_font_states_what_little_it_can() {
        let font =
            crate::ps::type1::Type1Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA)
                .unwrap();
        let metrics = Metrics::from_type1(&font);
        assert_eq!(metrics.units_per_em, 1000);
        assert!(metrics.num_glyphs > 0);
        assert!(metrics.bounds.y_max > F48Dot16::ZERO);
        // No table states these, so the bounding box stands in for them.
        assert_eq!(metrics.hhea_line.unwrap().ascender, metrics.bounds.y_max);
        assert_eq!(metrics.hhea_line.unwrap().descender, metrics.bounds.y_min);
        // And what such a font cannot express stays unset.
        assert!(metrics.vhea_line.is_none());
        assert!(metrics.typo_line.is_none());
        assert!(metrics.win_line.is_none());
        assert!(metrics.cap_height.is_none());
    }

    #[test]
    fn a_static_font_reads_the_same_at_every_location() {
        // Coordinates a font has no way to use change nothing about it.
        let font = horizontal();
        assert_eq!(
            Metrics::from_sfnt(&font, &[]),
            Metrics::from_sfnt(&font, &far())
        );
    }
}
