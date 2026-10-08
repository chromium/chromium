//! Font-wide metrics.

use super::metric;

use crate::{
    ps::type1::Type1Font,
    tables::{mvar::tags, os2::SelectionFlags},
    TableProvider,
};
use types::{BoundingBox, F2Dot14, F48Dot16};

/// The two ends of a line, in whatever unit a caller uses.
///
/// Separate from [`LineBox`] because the gap after a line is not part of
/// where it begins and ends, and the metrics built on a line use only the
/// two ends.
#[derive(Copy, Clone, Default, PartialEq, Eq, Debug)]
pub struct LineExtents<T> {
    /// Distance from the baseline to the top of the line.
    pub ascender: T,
    /// Distance from the baseline to the bottom of the line. Usually negative.
    pub descender: T,
}

/// Ascender, descender and line gap, as one table states them.
///
/// In design units unless a caller names another unit.
#[derive(Copy, Clone, Default, PartialEq, Eq, Debug)]
pub struct LineBox<T = F48Dot16> {
    /// Distance from the baseline to the top of the line.
    pub ascender: T,
    /// Distance from the baseline to the bottom of the line. Usually negative.
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
    /// A font that leaves both at zero has filled the field in without
    /// saying anything with it, which the choice of line has to account for.
    pub(crate) fn has_nonzero_extent(&self) -> bool {
        self.ascender != F48Dot16::ZERO || self.descender != F48Dot16::ZERO
    }
}

/// Metrics that describe a font rather than any glyph in it.
///
/// Read from `head`, `maxp`, `hhea`, `vhea` and `OS/2`, which together are a
/// few hundred bytes.
///
/// A font states its line metrics three times and the three disagree.
/// [`hhea_line`](Self::hhea_line) is what macOS reads,
/// [`win_line`](Self::win_line) is what Windows reads, and [`typo_line`](Self::typo_line) is what the
/// font asks for when [`use_typo_metrics`](Self::use_typo_metrics) is set.
/// Choosing between them is left to the caller.
///
/// A field the font does not state is `None`, which is distinct from a stated
/// zero.
///
/// `MVAR` varies most of these, so a set belongs to the location it was read
/// at. The units per em, the counts, the bounding box, the widest and tallest
/// advance and the average width cannot vary.
///
/// # Units
///
/// Measurements are unscaled design units. They are [`F48Dot16`] because a
/// location can move them off a whole unit, and rounding here would decide
/// for every caller how that fraction is spent.
///
/// The two axes do not scale alike: [`vhea_line`](Self::vhea_line) is
/// measured across the vertical baseline rather than along it, so it scales
/// on x where the horizontal sets scale on y.
#[derive(Copy, Clone, Default, PartialEq, Eq, Debug)]
pub struct Metrics {
    /// Design units per em.
    pub units_per_em: u16,
    /// Number of glyphs in the font.
    pub num_glyphs: u32,
    /// Box enclosing every glyph in the font.
    pub bounds: BoundingBox<F48Dot16>,
    /// Line metrics from `hhea`.
    pub hhea_line: Option<LineBox>,
    /// Widest advance of any glyph.
    pub max_advance_width: Option<F48Dot16>,
    /// Line metrics from `vhea`, measured across the vertical baseline.
    pub vhea_line: Option<LineBox>,
    /// Tallest advance of any glyph.
    pub max_advance_height: Option<F48Dot16>,
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
    /// Reads the metrics from `tables` at the given location.
    ///
    /// A missing or unreadable table leaves the metrics that come from it
    /// unset rather than failing.
    ///
    /// Pass an empty location for the font's defaults. At any other location
    /// `MVAR` is read and its deltas applied.
    pub fn from_sfnt<'a>(tables: &impl TableProvider<'a>, coords: &[F2Dot14]) -> Self {
        // Only at a location, so that the common case does not reach for a
        // table it has nothing to ask.
        let mvar = (!coords.is_empty()).then(|| tables.mvar().ok()).flatten();
        let deltas = mvar.as_ref().and_then(|mvar| mvar.at(coords));
        let deltas = deltas.as_ref();
        let mut metrics = Self::default();
        if let Ok(head) = tables.head() {
            metrics.units_per_em = head.units_per_em();
            metrics.bounds = BoundingBox {
                x_min: F48Dot16::from_i32(head.x_min() as i32),
                y_min: F48Dot16::from_i32(head.y_min() as i32),
                x_max: F48Dot16::from_i32(head.x_max() as i32),
                y_max: F48Dot16::from_i32(head.y_max() as i32),
            };
        }
        if let Ok(maxp) = tables.maxp() {
            metrics.num_glyphs = maxp.num_glyphs() as u32;
        }
        if let Ok(hhea) = tables.hhea() {
            // `MVAR` names one horizontal ascender, descender and line gap
            // between them, so the same delta applies to whichever of these
            // two sets a caller goes on to believe.
            metrics.hhea_line = Some(LineBox {
                ascender: metric(hhea.ascender().to_i16() as i32, deltas, tags::HASC),
                descender: metric(hhea.descender().to_i16() as i32, deltas, tags::HDSC),
                line_gap: metric(hhea.line_gap().to_i16() as i32, deltas, tags::HLGP),
            });
            metrics.max_advance_width =
                Some(F48Dot16::from_i32(hhea.advance_width_max().to_u16() as i32));
        }
        if let Ok(vhea) = tables.vhea() {
            metrics.vhea_line = Some(LineBox {
                ascender: metric(vhea.ascender().to_i16() as i32, deltas, tags::VASC),
                descender: metric(vhea.descender().to_i16() as i32, deltas, tags::VDSC),
                line_gap: metric(vhea.line_gap().to_i16() as i32, deltas, tags::VLGP),
            });
            metrics.max_advance_height =
                Some(F48Dot16::from_i32(vhea.advance_height_max().to_u16() as i32));
        }
        if let Ok(os2) = tables.os2() {
            metrics.typo_line = Some(LineBox {
                ascender: metric(os2.s_typo_ascender() as i32, deltas, tags::HASC),
                descender: metric(os2.s_typo_descender() as i32, deltas, tags::HDSC),
                line_gap: metric(os2.s_typo_line_gap() as i32, deltas, tags::HLGP),
            });
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
    /// A font states this line up to three times and the three disagree, so
    /// the choice follows FreeType:
    ///
    /// 1. The typographic line, if `OS/2` asks for it through
    ///    `USE_TYPO_METRICS`.
    /// 2. Otherwise `hhea`.
    /// 3. If `hhea` is zero, the typographic line if it is not, and the
    ///    clipping line otherwise.
    ///
    /// HarfBuzz stops at the second. The third matters because Arial Narrow
    /// Bold zeroes its typographic metrics where its siblings do not, and
    /// stopping early would lay it out differently from the rest of the
    /// family.
    ///
    /// Returns `None` if the font states no line at all.
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

    /// Reads the metrics a Type1 font states.
    ///
    /// Such a font has only a bounding box, a glyph count and a matrix. The
    /// ascender and descender are the top and bottom of that box, as FreeType
    /// takes them, since the font states none of its own. Everything else is
    /// unset.
    pub fn from_type1(font: &Type1Font) -> Self {
        let bbox = font.bbox();
        let bounds = BoundingBox {
            x_min: bbox.x_min.to_f48dot16(),
            y_min: bbox.y_min.to_f48dot16(),
            x_max: bbox.x_max.to_f48dot16(),
            y_max: bbox.y_max.to_f48dot16(),
        };
        Self {
            units_per_em: font.upem().clamp(0, u16::MAX as i32) as u16,
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
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::FontRef;

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
    fn far() -> [F2Dot14; 12] {
        [F2Dot14::from_f32(1.0); 12]
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
        assert!(metrics.max_advance_height.is_none());
        // And one that has it.
        let metrics = Metrics::from_sfnt(&vertical(), &[]);
        assert!(metrics.vhea_line.is_some());
        assert!(metrics.max_advance_height.unwrap() > F48Dot16::ZERO);
    }

    #[test]
    fn a_font_with_nothing_to_read_gives_up_rather_than_failing() {
        let metrics = Metrics::from_sfnt(&FontRef::new(font_test_data::NAMES_ONLY).unwrap(), &[]);
        assert_eq!(metrics, Metrics::default());
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
        let far = Metrics::from_sfnt(&variable(), &[F2Dot14::from_f32(0.4); 12]);
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
        assert_eq!(default.max_advance_width, far.max_advance_width);
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
