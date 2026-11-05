//! Checks read-fonts' path conversion against skrifa's.
//!
//! `read_fonts::tables::glyf::outline::outline_to_path` is a rewrite of
//! `skrifa::outline::path`, not a transcription: the contour walk that was two
//! functions with separate control flow is now one loop over a rotation. This
//! is what says the rewrite kept its meaning.
//!
//! Every glyph of every font given, in both contour start directions, drawn
//! through the same pen and compared as text.
//!
//! Set `SKRIFA_PARITY_CORPUS` to a directory to run over that as well.

use skrifa::{
    instance::{LocationRef, Size},
    outline::{pen::PathStyle, DrawSettings},
    prelude::*,
    raw::{
        tables::glyf::outline::{Outline, OutlineTables, PathContourStart, Scale26Dot6, ScaleF32},
        TableProvider,
    },
    FontRef, MetadataProvider,
};

/// Collects path commands as text, so two implementations can be diffed.
#[derive(Default, PartialEq)]
struct Recorder(String);

impl skrifa::outline::pen::OutlinePen for Recorder {
    fn move_to(&mut self, x: f32, y: f32) {
        self.0.push_str(&format!("M{x} {y} "));
    }
    fn line_to(&mut self, x: f32, y: f32) {
        self.0.push_str(&format!("L{x} {y} "));
    }
    fn quad_to(&mut self, a: f32, b: f32, x: f32, y: f32) {
        self.0.push_str(&format!("Q{a} {b} {x} {y} "));
    }
    fn curve_to(&mut self, a: f32, b: f32, c: f32, d: f32, x: f32, y: f32) {
        self.0.push_str(&format!("C{a} {b} {c} {d} {x} {y} "));
    }
    fn close(&mut self) {
        self.0.push_str("Z ");
    }
}

const PPEMS: [f32; 4] = [0.0, 16.0, 48.0, 144.0];

#[test]
fn bundled_fonts() {
    let fonts: &[(&str, &[u8])] = &[
        ("glyf_components", font_test_data::GLYF_COMPONENTS),
        ("cubic_glyf", font_test_data::CUBIC_GLYF),
        ("mostly_off_curve", font_test_data::MOSTLY_OFF_CURVE),
        ("starting_off_curve", font_test_data::STARTING_OFF_CURVE),
        ("interpolate_this", font_test_data::INTERPOLATE_THIS),
        ("vazirmatn_var", font_test_data::VAZIRMATN_VAR),
        ("colrv0v1", font_test_data::COLRV0V1),
        ("colrv0v1_variable", font_test_data::COLRV0V1_VARIABLE),
        (
            "noto_serif_display",
            font_test_data::NOTO_SERIF_DISPLAY_TRIMMED,
        ),
        ("material_icons", font_test_data::MATERIAL_ICONS_SUBSET),
        ("tinos_subset", font_test_data::TINOS_SUBSET),
        ("cousine_hint_subset", font_test_data::COUSINE_HINT_SUBSET),
    ];
    let mut compared = 0;
    for (name, data) in fonts {
        compared += check_font(name, data);
    }
    assert!(compared > 0);
    eprintln!("{compared} comparisons, 0 mismatches");
}

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
    let mut compared = 0;
    let mut seen = 0;
    for path in corpus_fonts(&dir) {
        let Ok(data) = std::fs::read(&path) else {
            continue;
        };
        let name = path.file_name().unwrap().to_string_lossy().into_owned();
        compared += check_font(&name, &data);
        seen += 1;
    }
    assert!(seen > 0, "no fonts found in {dir:?}");
    eprintln!("checked {seen} fonts, {compared} comparisons, 0 mismatches");
}

/// Draws every glyph both ways and compares. Returns how many it compared.
fn check_font(name: &str, data: &[u8]) -> u64 {
    let Ok(font) = FontRef::new(data) else {
        return 0;
    };
    let Ok(tables) = OutlineTables::new(&font) else {
        return 0;
    };
    let glyphs = font.outline_glyphs();
    let glyph_count = font.maxp().map(|maxp| maxp.num_glyphs()).unwrap_or(0);
    // skrifa's path style also picks its scaler, so each direction is paired
    // with the mode that matches: FreeType with 26.6, HarfBuzz with f32.
    let mut fixed = Outline::<Scale26Dot6>::new();
    let mut float = Outline::<ScaleF32>::new();
    let mut compared = 0;
    for &ppem in &PPEMS {
        let size = if ppem == 0.0 {
            Size::unscaled()
        } else {
            Size::new(ppem)
        };
        for glyph_id in (0..glyph_count).map(GlyphId::from) {
            let Some(glyph) = glyphs.get(glyph_id) else {
                continue;
            };
            for (start, style) in [
                (PathContourStart::ScanBackward, PathStyle::FreeType),
                (PathContourStart::ScanForward, PathStyle::HarfBuzz),
            ] {
                let forward = start == PathContourStart::ScanForward;
                let mut theirs = Recorder::default();
                let settings =
                    DrawSettings::unhinted(size, LocationRef::default()).with_path_style(style);
                if glyph.draw(settings, &mut theirs).is_err() {
                    continue;
                }
                let mut mine = Recorder::default();
                let drawn = if forward {
                    float
                        .load(&tables, glyph_id, size.ppem())
                        .map(|o| o.to_path(start, &mut mine))
                } else {
                    fixed
                        .load(&tables, glyph_id, size.ppem())
                        .map(|o| o.to_path(start, &mut mine))
                };
                let at = format!("{name} gid {glyph_id} ppem {ppem} {start:?}");
                // skrifa drew this glyph, so declining to draw it is a
                // disagreement rather than something to pass over.
                match drawn {
                    Ok(true) => {}
                    Ok(false) => panic!("{at}: our points do not describe a path"),
                    Err(err) => panic!("{at}: loading failed ({err:?})"),
                }
                assert_eq!(mine.0, theirs.0, "{at}");
                compared += 1;
            }
        }
    }
    compared
}
