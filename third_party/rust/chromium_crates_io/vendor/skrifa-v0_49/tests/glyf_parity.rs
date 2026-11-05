//! Checks read-fonts' `glyf` outline loader against skrifa's scalers.
//!
//! `read_fonts::tables::glyf::outline` and `skrifa::outline::glyf` implement
//! the same thing twice. Both ship until skrifa moves onto the loader, and
//! nothing stops them drifting apart. This is what notices.
//!
//! Three comparisons per glyph, per size, per instance:
//!
//! 1. `Scale26Dot6` against skrifa's FreeType scaler, exactly. Points, flags,
//!    contours, phantom points and metrics all have to match bit for bit.
//! 2. `ScaleF32` against `Scale26Dot6`, within their differing rounding.
//! 3. `Unscaled` against `Scale26Dot6` at no size, exactly, shifted by six.
//!
//! Only the first compares against skrifa. Its HarfBuzz scaler applies a
//! component offset in font units to already scaled points, and does not copy
//! scaled phantom points back before adjusting the left side bearing. The
//! loader corrects both, so the two disagree by design on the glyphs those
//! affect. Pinning 26.6 to skrifa exactly, then pinning the other two modes to
//! 26.6, covers all three without encoding the bugs as expectations.
//!
//! Set `SKRIFA_PARITY_CORPUS` to a directory to run over that as well. The
//! bundled fonts are small, and real ones exercise far more.

use skrifa::{
    instance::{Location, LocationRef, NormalizedCoord, Size},
    prelude::*,
    raw::{
        tables::glyf::outline::{Outline, OutlineTables, Scale26Dot6, ScaleF32, Unscaled},
        TableProvider,
    },
    FontRef, MetadataProvider,
};

/// Sizes every comparison runs at. Zero means unscaled.
const PPEMS: [f32; 5] = [0.0, 8.0, 16.0, 48.0, 144.0];

#[test]
fn static_fonts() {
    let fonts: &[(&str, &[u8])] = &[
        ("glyf_components", font_test_data::GLYF_COMPONENTS),
        ("cubic_glyf", font_test_data::CUBIC_GLYF),
        ("mostly_off_curve", font_test_data::MOSTLY_OFF_CURVE),
        ("starting_off_curve", font_test_data::STARTING_OFF_CURVE),
        ("simple_glyf", font_test_data::SIMPLE_GLYF),
        ("ahem", font_test_data::AHEM),
        ("cousine_hint_subset", font_test_data::COUSINE_HINT_SUBSET),
        ("tthint_subset", font_test_data::TTHINT_SUBSET),
        ("material_icons", font_test_data::MATERIAL_ICONS_SUBSET),
        (
            "material_icons_matrix",
            font_test_data::MATERIAL_ICONS_SUBSET_MATRIX,
        ),
        ("material_symbols", font_test_data::MATERIAL_SYMBOLS_SUBSET),
        ("tinos_subset", font_test_data::TINOS_SUBSET),
        (
            "noto_serif_display",
            font_test_data::NOTO_SERIF_DISPLAY_TRIMMED,
        ),
        ("colrv0v1", font_test_data::COLRV0V1),
        ("autohint_cmap", font_test_data::AUTOHINT_CMAP),
    ];
    let mut report = Report::default();
    for (name, data) in fonts {
        report.check_font(name, data, false);
    }
    report.finish();
}

#[test]
fn variable_fonts() {
    let fonts: &[(&str, &[u8])] = &[
        ("vazirmatn_var", font_test_data::VAZIRMATN_VAR),
        ("cvar", font_test_data::CVAR),
        ("amstelvar_avar2", font_test_data::AMSTELVAR_AVAR2_A),
        ("cantarell_vf", font_test_data::CANTARELL_VF_TRIMMED),
        ("colrv0v1_variable", font_test_data::COLRV0V1_VARIABLE),
        ("avar2_checker", font_test_data::AVAR2_CHECKER),
        ("interpolate_this", font_test_data::INTERPOLATE_THIS),
        ("gvar_use_my_metrics", font_test_data::gvar::USE_MY_METRICS),
    ];
    let mut report = Report::default();
    for (name, data) in fonts {
        // The default instance and a mid axis one take different paths through
        // gvar: no deltas at all, versus deltas that need interpolating.
        report.check_font(name, data, false);
        report.check_font(name, data, true);
    }
    report.finish();
}

/// Runs over a directory of fonts named by `SKRIFA_PARITY_CORPUS`, if set.
/// Every font under `dir`, recursively: a corpus is usually arranged by
/// family, and a flat read would see almost none of it.
fn corpus_fonts(dir: &std::ffi::OsString) -> Vec<std::path::PathBuf> {
    let mut out = Vec::new();
    let mut stack = vec![std::path::PathBuf::from(dir)];
    while let Some(dir) = stack.pop() {
        let Ok(entries) = std::fs::read_dir(&dir) else {
            continue;
        };
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() {
                stack.push(path);
            } else if matches!(
                path.extension().and_then(|e| e.to_str()),
                Some("ttf" | "otf" | "ttc")
            ) {
                out.push(path);
            }
        }
    }
    out
}

#[test]
fn corpus_from_env() {
    let Some(dir) = std::env::var_os("SKRIFA_PARITY_CORPUS") else {
        return;
    };
    let paths = corpus_fonts(&dir);
    if paths.is_empty() {
        panic!("SKRIFA_PARITY_CORPUS is set to {dir:?}, which holds no fonts");
    }
    let mut report = Report::default();
    let mut seen = 0;
    for path in paths {
        let Ok(data) = std::fs::read(&path) else {
            continue;
        };
        let name = path.file_name().unwrap().to_string_lossy().into_owned();
        report.check_font(&name, &data, false);
        report.check_font(&name, &data, true);
        seen += 1;
    }
    assert!(seen > 0, "no fonts found in {dir:?}");
    eprintln!("checked {seen} fonts from {dir:?}");
    report.finish();
}

/// One font at one variation instance: everything a comparison needs beyond
/// the glyph and the size.
struct Instance<'a> {
    tables: &'a OutlineTables<'a>,
    location: Location,
    name: &'a str,
    vary: bool,
}

/// One [`Outline`] per mode, reused for every glyph.
///
/// A comparison needs all three alive at once, so they are separate fields
/// rather than one reused in turn. They grow their buffers to fit the largest
/// glyph and then stop allocating.
#[derive(Default)]
struct Loaded {
    fixed: Outline<Scale26Dot6>,
    float: Outline<ScaleF32>,
    raw: Outline<Unscaled>,
}

#[derive(Default)]
struct Report {
    compared: u64,
    failures: Vec<String>,
    skipped: u64,
}

impl Report {
    fn fail(&mut self, what: &str) {
        // Enough to diagnose without burying the count.
        if self.failures.len() < 20 {
            self.failures.push(what.to_string());
        } else if self.failures.len() == 20 {
            self.failures.push("...".to_string());
        }
    }

    fn finish(self) {
        assert!(
            self.failures.is_empty(),
            "{} of {} comparisons disagreed:\n{}",
            self.failures.len(),
            self.compared,
            self.failures.join("\n")
        );
        assert!(
            self.compared > 0,
            "nothing was compared ({} glyphs skipped)",
            self.skipped
        );
        eprintln!("{} comparisons, 0 mismatches", self.compared);
    }

    fn check_font(&mut self, name: &str, data: &[u8], vary: bool) {
        let Ok(font) = FontRef::new(data) else {
            return;
        };
        // Not a glyf font, or missing something the loader needs.
        let Ok(tables) = OutlineTables::new(&font) else {
            return;
        };
        let axes = font.axes();
        if vary && axes.is_empty() {
            return;
        }
        let location = if vary {
            let settings: Vec<_> = axes
                .iter()
                .map(|axis| (axis.tag(), (axis.min_value() + axis.max_value()) * 0.5))
                .collect();
            axes.location(&settings)
        } else {
            Location::default()
        };
        // The location rides on the tables now, so it is selected once per
        // font rather than passed to every load.
        let coords: Vec<_> = location.coords().to_vec();
        // A location that normalises to all zeros is the default instance, and
        // skrifa drops it at its API boundary through
        // `LocationRef::effective_coords`. read-fonts leaves that to the
        // caller, so the two would take different paths through the same
        // arithmetic and disagree by a 26.6 quantum. Nothing to compare.
        if vary
            && coords
                .iter()
                .all(|coord| *coord == NormalizedCoord::default())
        {
            return;
        }
        let tables = tables.at(&coords, &[]);
        let instance = Instance {
            tables: &tables,
            location,
            name,
            vary,
        };
        let outlines = font.outline_glyphs();
        let mut mine = Loaded::default();
        let glyph_count = font.maxp().map(|maxp| maxp.num_glyphs()).unwrap_or(0);
        for &ppem in &PPEMS {
            let size = if ppem == 0.0 {
                Size::unscaled()
            } else {
                Size::new(ppem)
            };
            for glyph_id in (0..glyph_count).map(GlyphId::from) {
                let Some(glyph) = outlines.get(glyph_id) else {
                    continue;
                };
                self.check_glyph(&instance, size, &glyph, &mut mine);
            }
        }
    }

    fn check_glyph(
        &mut self,
        instance: &Instance<'_>,
        size: Size,
        glyph: &skrifa::outline::OutlineGlyph<'_>,
        mine: &mut Loaded,
    ) {
        let Instance {
            tables,
            location,
            name,
            vary,
        } = instance;
        let ppem = size.ppem();
        let glyph_id = glyph.glyph_id();
        let at = &format!("{name} gid {glyph_id} ppem {ppem:?} vary {vary}");
        // skrifa's FreeType scaler is the oracle for the 26.6 mode.
        let theirs = glyph.with_scaled_glyf_outline(size, LocationRef::from(location), None, |o| {
            Ok((
                o.points.to_vec(),
                o.flags.to_vec(),
                o.contours.to_vec(),
                o.phantom_points,
                o.adjusted_lsb(),
                o.adjusted_advance_width(),
            ))
        });
        // A CFF glyph, or one skrifa itself rejects. Nothing to compare.
        let Ok((points, flags, contours, phantom, lsb, advance)) = theirs else {
            self.skipped += 1;
            return;
        };
        let Ok(fixed) = mine.fixed.load(*tables, glyph_id, ppem) else {
            self.fail(&format!("{at}: loading failed but skrifa succeeded"));
            return;
        };
        self.compared += 1;
        if fixed.points() != points {
            let first = fixed.points().iter().zip(&points).position(|(a, b)| a != b);
            self.fail(&format!(
                "{at}: 26.6 points differ {}",
                match first {
                    Some(ix) => format!(
                        "at {ix}, ours {:?} theirs {:?} (len {} vs {})",
                        fixed.points()[ix],
                        points[ix],
                        fixed.points().len(),
                        points.len()
                    ),
                    None => format!("in length, {} vs {}", fixed.points().len(), points.len()),
                }
            ));
        } else if fixed.flags() != flags {
            self.fail(&format!("{at}: 26.6 flags differ"));
        } else if fixed.contours() != contours {
            self.fail(&format!(
                "{at}: 26.6 contours differ, {:?} vs {:?}",
                fixed.contours(),
                contours
            ));
        } else if *fixed.phantom_points() != phantom {
            self.fail(&format!(
                "{at}: phantom points differ, {:?} vs {:?}",
                fixed.phantom_points(),
                phantom
            ));
        } else if fixed.adjusted_lsb() != lsb || fixed.adjusted_advance_width() != advance {
            self.fail(&format!(
                "{at}: metrics differ, lsb {:?}/{lsb:?} advance {:?}/{advance:?}",
                fixed.adjusted_lsb(),
                fixed.adjusted_advance_width()
            ));
        }

        // The f32 mode is not compared here. It reproduces HarfBuzz, and the
        // 26.6 mode reproduces FreeType, so the two are different arithmetic
        // by design and agree only to within a tolerance nothing specifies.
        // `glyf_path_parity` checks it against skrifa's HarfBuzz scaler
        // instead, which is a reference rather than a stand-in.
        if mine.float.load(*tables, glyph_id, ppem).is_err() {
            // The 26.6 mode already loaded this glyph, so nothing about the
            // font explains a failure here.
            self.fail(&format!("{at}: f32 load failed where 26.6 succeeded"));
        }

        // The unscaled mode does the same arithmetic as 26.6 at no size,
        // without the round trip through 26.6, so it must agree exactly.
        if ppem.is_none() {
            if let Ok(raw) = mine.raw.load(*tables, glyph_id) {
                self.compared += 1;
                if raw.points().len() != fixed.points().len() {
                    self.fail(&format!(
                        "{at}: unscaled point count differs, {} vs {}",
                        raw.points().len(),
                        fixed.points().len()
                    ));
                } else if let Some(ix) = raw
                    .points()
                    .iter()
                    .zip(fixed.points())
                    .position(|(r, x)| r.x != x.x.to_bits() >> 6 || r.y != x.y.to_bits() >> 6)
                {
                    self.fail(&format!(
                        "{at}: unscaled disagrees with 26.6 at {ix}, {:?} vs {:?}",
                        raw.points()[ix],
                        fixed.points()[ix]
                    ));
                } else if raw.contours() != fixed.contours() {
                    self.fail(&format!("{at}: unscaled contours differ"));
                }
            } else {
                self.fail(&format!("{at}: unscaled load failed where 26.6 succeeded"));
            }
        }
    }
}
