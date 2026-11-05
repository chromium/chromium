use super::*;
use crate::{
    instance::LocationRef,
    outline::{
        pen::{NullPen, SvgPen},
        DrawSettings, Engine, HintingInstance, HintingOptions, SmoothMode, Target as OutlineTarget,
    },
    FontRef, MetadataProvider,
};
use alloc::sync::Arc;
use raw::{types::GlyphId, TableProvider};

fn font(data: &[u8]) -> Font {
    Font::new(data.to_vec(), 0).unwrap()
}

/// Returns a hinting instance that draws `data` with the classic behavior,
/// for comparison.
fn classic_instance(
    data: &[u8],
    size: Size,
    options: HintingOptions,
) -> (FontRef<'_>, HintingInstance) {
    let font = FontRef::new(data).unwrap();
    let outlines = font.outline_glyphs();
    let instance = HintingInstance::new(&outlines, size, LocationRef::default(), options).unwrap();
    (font, instance)
}

/// Draws `glyph` with scratch memory, or fails if the font has no outline.
fn draw(
    scaler: &ScalerFont,
    glyph: GlyphId,
    sink: &mut impl PathSink,
) -> Result<DrawnMetrics, ScaleError> {
    scaler
        .outline(glyph)
        .ok_or(ScaleError::GlyphNotFound(glyph))?
        .draw(DrawMemory::Scratch, sink)
}

fn svg_scaler(scaler: &ScalerFont, glyph: GlyphId) -> Option<(String, DrawnMetrics)> {
    let mut pen = SvgPen::new();
    let metrics = draw(scaler, glyph, &mut pen).ok()?;
    Some((pen.to_string(), metrics))
}

fn svg_instance(font: &FontRef<'_>, instance: &HintingInstance, glyph: GlyphId) -> Option<String> {
    let mut pen = SvgPen::new();
    font.outline_glyphs()
        .get(glyph)?
        .draw(instance, &mut pen)
        .ok()?;
    Some(pen.to_string())
}

/// Asserts that the scaler with the classic behavior matches the hinting
/// instance for every glyph and size.
fn assert_classic_parity(data: &[u8], hinting: HintOptions, options: HintingOptions) {
    let mut compared = 0;
    for ppem in [9.0, 12.0, 16.0, 24.8, 48.0] {
        let (font, instance) = classic_instance(data, Size::new(ppem), options.clone());
        let scaler =
            ScalerFont::hinted(self::font(data), ppem, hinting.with_classic_cleartype(true));
        assert_eq!(scaler.is_hinted(), instance.is_enabled(), "{ppem}");
        for gid in 0..font.maxp().unwrap().num_glyphs() {
            let gid = GlyphId::new(gid as u32);
            let expected = svg_instance(&font, &instance, gid);
            let actual = svg_scaler(&scaler, gid);
            assert_eq!(
                actual.as_ref().map(|(path, _)| path),
                expected.as_ref(),
                "glyph {gid} at {ppem}"
            );
            if let (Some((_, metrics)), Some(glyph)) = (actual, font.outline_glyphs().get(gid)) {
                let theirs = glyph.draw(&instance, &mut NullPen).unwrap();
                assert_eq!(
                    metrics.h_advance, theirs.advance_width,
                    "glyph {gid} at {ppem}"
                );
                assert_eq!(metrics.x_bearing, theirs.lsb, "glyph {gid} at {ppem}");
                assert_eq!(
                    metrics.has_overlaps, theirs.has_overlaps,
                    "glyph {gid} at {ppem}"
                );
            }
            compared += 1;
        }
    }
    assert!(compared > 0);
}

#[test]
fn truetype_interpreter_matches_the_hinting_instance() {
    for data in [
        font_test_data::TINOS_SUBSET,
        font_test_data::COUSINE_HINT_SUBSET,
        font_test_data::TTHINT_SUBSET,
        font_test_data::CVAR,
    ] {
        for (target, outline_target) in [
            (RasterTarget::Normal, OutlineTarget::default()),
            (RasterTarget::Mono, OutlineTarget::Mono),
            (RasterTarget::Lcd, SmoothMode::Lcd.into()),
            (RasterTarget::Light, SmoothMode::Light.into()),
        ] {
            assert_classic_parity(
                data,
                HintOptions::interpreter().with_target(target),
                HintingOptions {
                    engine: Engine::Interpreter,
                    target: outline_target,
                },
            );
        }
    }
}

#[test]
fn preserve_linear_metrics_matches_the_hinting_instance() {
    assert_classic_parity(
        font_test_data::TINOS_SUBSET,
        HintOptions::interpreter().with_preserve_linear_metrics(true),
        HintingOptions {
            engine: Engine::Interpreter,
            target: OutlineTarget::Smooth {
                mode: SmoothMode::Normal,
                symmetric_rendering: true,
                preserve_linear_metrics: true,
            },
        },
    );
}

#[test]
fn autohinter_matches_the_hinting_instance() {
    for data in [
        font_test_data::NOTOSERIF_AUTOHINT_SHAPING,
        font_test_data::VAZIRMATN_VAR,
        font_test_data::CANTARELL_VF_TRIMMED,
    ] {
        for (target, outline_target) in [
            (RasterTarget::Normal, OutlineTarget::default()),
            (RasterTarget::Light, SmoothMode::Light.into()),
            (RasterTarget::Mono, OutlineTarget::Mono),
        ] {
            assert_classic_parity(
                data,
                HintOptions::auto().with_target(target),
                HintingOptions {
                    engine: Engine::Auto(None),
                    target: outline_target,
                },
            );
        }
    }
}

#[test]
fn postscript_hinter_matches_the_hinting_instance() {
    for data in [
        font_test_data::NOTO_SERIF_DISPLAY_TRIMMED,
        font_test_data::CANTARELL_VF_TRIMMED,
        font_test_data::MATERIAL_ICONS_SUBSET_MATRIX,
    ] {
        assert_classic_parity(
            data,
            HintOptions::interpreter(),
            HintingOptions {
                engine: Engine::Interpreter,
                target: OutlineTarget::default(),
            },
        );
    }
}

#[test]
fn auto_fallback_chooses_as_the_hinting_instance_does() {
    // Tinos has programs, so the interpreter; Vazirmatn has none, so the
    // automatic hinter.
    for data in [font_test_data::TINOS_SUBSET, font_test_data::VAZIRMATN_VAR] {
        assert_classic_parity(
            data,
            HintOptions::auto_fallback(),
            HintingOptions::default(),
        );
    }
}

#[test]
fn unhinted_matches_the_outline_collection() {
    for data in [
        font_test_data::VAZIRMATN_VAR,
        font_test_data::CANTARELL_VF_TRIMMED,
        font_test_data::NOTO_SERIF_DISPLAY_TRIMMED,
        font_test_data::varc::CJK_6868,
    ] {
        let font = FontRef::new(data).unwrap();
        let outlines = font.outline_glyphs();
        for ppem in [None, Some(16.0), Some(100.5)] {
            let size = ppem.map_or_else(Size::unscaled, Size::new);
            let scaler = ScalerFont::unhinted(self::font(data), ppem);
            assert!(!scaler.is_hinted());
            for gid in 0..font.maxp().unwrap().num_glyphs() {
                let gid = GlyphId::new(gid as u32);
                let Some(glyph) = outlines.get(gid) else {
                    continue;
                };
                let mut expected = SvgPen::new();
                let theirs = glyph
                    .draw(
                        DrawSettings::unhinted(size, LocationRef::default()),
                        &mut expected,
                    )
                    .unwrap();
                let (actual, metrics) = svg_scaler(&scaler, gid).unwrap();
                assert_eq!(
                    actual,
                    expected.to_string(),
                    "glyph {gid} at {size:?} in font of {} bytes",
                    data.len()
                );
                assert_eq!(
                    metrics.h_advance,
                    theirs.advance_width,
                    "glyph {gid} at {size:?} in font of {} bytes",
                    data.len()
                );
            }
        }
    }
}

#[test]
fn a_location_is_the_fonts() {
    let data = font_test_data::VAZIRMATN_VAR;
    let font = FontRef::new(data).unwrap();
    let location = font.axes().location([("wght", 700.0)]);
    let model = self::font(data)
        .instance_builder()
        .variations([("wght", 700.0)])
        .build();
    let size = Size::new(20.0);
    let instance = HintingInstance::new(
        &font.outline_glyphs(),
        size,
        &location,
        HintingOptions::default(),
    )
    .unwrap();
    let scaler = ScalerFont::hinted(
        model,
        20.0,
        HintOptions::auto_fallback().with_classic_cleartype(true),
    );
    let mut compared = 0;
    for gid in 1..font.maxp().unwrap().num_glyphs() {
        let gid = GlyphId::new(gid as u32);
        let Some(glyph) = font.outline_glyphs().get(gid) else {
            continue;
        };
        let (actual, _) = svg_scaler(&scaler, gid).unwrap();
        let mut expected = SvgPen::new();
        glyph.draw(&instance, &mut expected).unwrap();
        assert_eq!(actual, expected.to_string(), "glyph {gid}");
        compared += 1;
    }
    assert!(compared > 0);
}

#[test]
fn a_callers_memory_draws_the_same_glyph() {
    for (data, hinting) in [
        (
            font_test_data::TINOS_SUBSET,
            Some(HintOptions::interpreter()),
        ),
        (font_test_data::TINOS_SUBSET, None),
        (font_test_data::varc::CJK_6868, None),
    ] {
        let scaler = match hinting {
            Some(hinting) => ScalerFont::hinted(font(data), 16.0, hinting),
            None => ScalerFont::unhinted(font(data), Some(16.0)),
        };
        for gid in 0..scaler.font().num_glyphs() {
            let gid = GlyphId::new(gid);
            let Some((expected, _)) = svg_scaler(&scaler, gid) else {
                continue;
            };
            let glyph = scaler.outline(gid).unwrap();
            assert_eq!(glyph.glyph(), gid);
            let mut memory = vec![0u8; glyph.required_memory_size()];
            let mut pen = SvgPen::new();
            glyph
                .draw(DrawMemory::Buffer(&mut memory), &mut pen)
                .unwrap();
            assert_eq!(pen.to_string(), expected, "glyph {gid}");
        }
    }
}

#[test]
fn too_little_memory_is_refused() {
    let scaler = ScalerFont::hinted(
        font(font_test_data::TINOS_SUBSET),
        16.0,
        HintOptions::interpreter(),
    );
    let glyph = scaler.outline(GlyphId::new(2)).unwrap();
    let needed = glyph.required_memory_size();
    assert!(needed > 0);
    let mut memory = vec![0u8; needed / 2];
    let result = glyph.draw(DrawMemory::Buffer(&mut memory), &mut NullPen);
    assert_eq!(
        result.unwrap_err(),
        ScaleError::InsufficientMemory {
            required: needed,
            supplied: needed / 2,
        }
    );
}

#[test]
fn a_glyph_the_font_lacks_is_none() {
    let scaler = ScalerFont::unhinted(font(font_test_data::TINOS_SUBSET), Some(16.0));
    let past_the_end = GlyphId::new(scaler.font().num_glyphs());
    assert!(scaler.outline(past_the_end).is_none());
}

#[test]
fn changing_the_size_reconfigures() {
    let hinting = HintOptions::interpreter().with_classic_cleartype(true);
    let mut scaler = ScalerFont::hinted(font(font_test_data::TINOS_SUBSET), 8.0, hinting);
    let gid = GlyphId::new(2);
    for ppem in [12.0, 16.0, 24.0] {
        scaler.reconfigure(scaler.font().clone(), ppem, hinting);
        assert!(scaler.is_hinted());
        let (font, instance) = classic_instance(
            font_test_data::TINOS_SUBSET,
            Size::new(ppem),
            Engine::Interpreter.into(),
        );
        assert_eq!(
            svg_scaler(&scaler, gid).unwrap().0,
            svg_instance(&font, &instance, gid).unwrap()
        );
    }
}

// The automatic hinter holds nothing that depends on the size, so a new size
// keeps the instance and draws as a fresh scaler at that size would.
#[test]
fn changing_the_size_keeps_the_autohinter() {
    let data = font_test_data::NOTOSERIF_AUTOHINT_SHAPING;
    let mut scaler = ScalerFont::hinted(font(data), 12.0, HintOptions::auto());
    for ppem in [16.0, 24.0] {
        scaler.reconfigure(scaler.font().clone(), ppem, HintOptions::auto());
        assert!(scaler.is_hinted());
        let fresh = ScalerFont::hinted(font(data), ppem, HintOptions::auto());
        for gid in 1..10 {
            let gid = GlyphId::new(gid);
            assert_eq!(
                svg_scaler(&scaler, gid),
                svg_scaler(&fresh, gid),
                "glyph {gid}"
            );
        }
    }
}

// A composite states no advance, and the automatic hinter is no exception.
#[test]
fn autohinted_composites_state_no_advance() {
    let scaler = ScalerFont::hinted(
        font(font_test_data::varc::CJK_6868),
        16.0,
        HintOptions::auto(),
    );
    let mut composites = 0;
    for gid in 0..scaler.font().num_glyphs() {
        let Some(glyph) = scaler.outline(GlyphId::new(gid)) else {
            continue;
        };
        if glyph.source() != OutlineSource::Varc {
            continue;
        }
        let metrics = glyph.draw(DrawMemory::Scratch, &mut NullPen).unwrap();
        assert_eq!(metrics.h_advance, None, "glyph {gid}");
        composites += 1;
    }
    assert!(composites > 0);
}

// The automatic hinter's state is kept for each set of outline sources, since
// drawing with VARC composites and drawing the base outlines alone measure
// different shapes. A varied instance takes its glyph styles from the default
// location's state for the same sources.
#[test]
fn autohinter_state_is_kept_per_outline_source() {
    use raw::model::glyph::GlyphSourceOrder;
    let model = font(font_test_data::varc::CJK_6868);
    let base_order = GlyphSourceOrder::BASE_OUTLINES;
    assert_eq!(model.outline_glyphs().source(), Some(OutlineSource::Varc));
    assert_eq!(
        model.outline_glyphs_with(base_order).source(),
        Some(OutlineSource::Glyf)
    );
    let state = |font: &Font, order| {
        autohint::LocatedState::of_outlines(font, font.outline_glyphs_with(order))
    };
    let with_varc = state(&model, GlyphSourceOrder::DEFAULT);
    let base = state(&model, base_order);
    assert!(Arc::ptr_eq(
        &with_varc,
        &state(&model, GlyphSourceOrder::DEFAULT)
    ));
    assert!(Arc::ptr_eq(&base, &state(&model, base_order)));
    assert!(!Arc::ptr_eq(&with_varc, &base));
    let bold = model
        .instance_builder()
        .variations([("wght", 700.0)])
        .build();
    assert!(state(&bold, base_order).shares_styles_with(&base));
    assert!(state(&bold, GlyphSourceOrder::DEFAULT).shares_styles_with(&with_varc));
}

// A scaler given a source order draws from those sources, and its automatic
// hinter is measured from them.
#[test]
fn a_scaler_draws_from_the_sources_it_is_given() {
    let model = font(font_test_data::varc::CJK_6868);
    let base_only = || ScalerSource::new(model.clone(), GlyphSourceOrder::BASE_OUTLINES);
    let varc = ScalerFont::force_unhinted(model.clone(), Some(16.0));
    let base = ScalerFont::force_unhinted(base_only(), Some(16.0));
    assert_eq!(varc.outline_source(), Some(OutlineSource::Varc));
    assert_eq!(varc.underlying_outline_source(), Some(OutlineSource::Glyf));
    assert_eq!(base.outline_source(), Some(OutlineSource::Glyf));
    assert_eq!(base.underlying_outline_source(), Some(OutlineSource::Glyf));
    let none = ScalerFont::force_unhinted(
        ScalerSource::new(model.clone(), GlyphSourceOrder::COLOR),
        Some(16.0),
    );
    assert_eq!(none.outline_source(), None);
    assert_eq!(none.underlying_outline_source(), None);
    let glyph = (0..model.num_glyphs())
        .map(GlyphId::new)
        .find(|glyph| {
            varc.outline(*glyph)
                .is_some_and(|glyph| glyph.source() == OutlineSource::Varc)
        })
        .expect("a VARC composite");
    assert_eq!(
        base.outline(glyph).map(|glyph| glyph.source()),
        Some(OutlineSource::Glyf)
    );
    let state = |scaler: &ScalerFont| match &scaler.state {
        State::Auto(instance) => instance.state().clone(),
        _ => panic!("expected the automatic hinter"),
    };
    let varc = ScalerFont::hinted(model.clone(), 16.0, HintOptions::auto());
    let base = ScalerFont::hinted(base_only(), 16.0, HintOptions::auto());
    assert!(!Arc::ptr_eq(&state(&varc), &state(&base)));
}

// What the automatic hinter derives from a font is kept with it: every
// scaler of one instance shares it whatever its size or target, and another
// location gets its own.
#[test]
fn scalers_of_one_font_share_the_autohinter_state() {
    let state = |scaler: &ScalerFont| match &scaler.state {
        State::Auto(instance) => instance.state().clone(),
        _ => panic!("expected the automatic hinter"),
    };
    let model = font(font_test_data::VAZIRMATN_VAR);
    let small = ScalerFont::hinted(model.clone(), 12.0, HintOptions::auto());
    let large = ScalerFont::hinted(
        model.clone(),
        40.0,
        HintOptions::auto().with_target(RasterTarget::Light),
    );
    assert!(Arc::ptr_eq(&state(&small), &state(&large)));
    let bold = model
        .instance_builder()
        .variations([("wght", 700.0)])
        .build();
    let bold = ScalerFont::hinted(bold, 12.0, HintOptions::auto());
    assert!(!Arc::ptr_eq(&state(&small), &state(&bold)));
    // The glyph styles hold at every location, so the two share them.
    assert!(state(&small).shares_styles_with(&state(&bold)));
    // A font without variations has one location, so asking for another
    // still shares the one copy.
    let static_font = font(font_test_data::NOTOSERIF_AUTOHINT_SHAPING);
    let plain = ScalerFont::hinted(static_font.clone(), 12.0, HintOptions::auto());
    let asked = static_font
        .instance_builder()
        .variations([("wght", 700.0)])
        .build();
    let asked = ScalerFont::hinted(asked, 30.0, HintOptions::auto());
    assert!(Arc::ptr_eq(&state(&plain), &state(&asked)));
    // A font read again from the same bytes is another font, with its own.
    let again = ScalerFont::hinted(
        font(font_test_data::VAZIRMATN_VAR),
        12.0,
        HintOptions::auto(),
    );
    assert!(!Arc::ptr_eq(&state(&small), &state(&again)));
}

// Tinos asks for asymmetric ClearType smoothing from 11 through 16 ppem in
// its version 1 gasp, which changes what GETINFO reports and so changes the
// hinted outline.
#[test]
fn cleartype_gasp_policy_changes_outlines() {
    let data = font_test_data::TINOS_SUBSET;
    let mut differing = 0;
    let classic = ScalerFont::hinted(
        font(data),
        13.0,
        HintOptions::interpreter().with_classic_cleartype(true),
    );
    let enhanced = ScalerFont::hinted(font(data), 13.0, HintOptions::interpreter());
    assert!(!enhanced.raster_policy().unwrap().symmetric_smoothing);
    assert!(classic.raster_policy().unwrap().symmetric_smoothing);
    for gid in 0..20 {
        let gid = GlyphId::new(gid);
        let (Some(a), Some(b)) = (svg_scaler(&classic, gid), svg_scaler(&enhanced, gid)) else {
            continue;
        };
        if a.0 != b.0 {
            differing += 1;
        }
    }
    assert!(differing > 0, "the gasp policy should reach the outlines");
}

// Above that range the gasp asks for symmetric smoothing, which is what the
// interpreter already assumed.
#[test]
fn cleartype_gasp_policy_agrees_at_large_sizes() {
    let data = font_test_data::TINOS_SUBSET;
    let classic = ScalerFont::hinted(
        font(data),
        24.0,
        HintOptions::interpreter().with_classic_cleartype(true),
    );
    let enhanced = ScalerFont::hinted(font(data), 24.0, HintOptions::interpreter());
    for gid in 0..20 {
        let gid = GlyphId::new(gid);
        assert_eq!(
            svg_scaler(&classic, gid).map(|(path, _)| path),
            svg_scaler(&enhanced, gid).map(|(path, _)| path),
            "glyph {gid}"
        );
    }
}

// A font with no glyph instructions asks for no ClearType grid fitting,
// which every antialiased target honors.
#[test]
fn cleartype_gasp_policy_disables_grid_fitting() {
    for (target, classic, expected) in [
        (RasterTarget::Lcd, true, true),
        (RasterTarget::Lcd, false, false),
        (RasterTarget::Normal, true, true),
        (RasterTarget::Normal, false, false),
        (RasterTarget::Mono, false, true),
    ] {
        let scaler = ScalerFont::hinted(
            font(font_test_data::AHEM),
            12.0,
            HintOptions::interpreter()
                .with_target(target)
                .with_classic_cleartype(classic),
        );
        assert_eq!(scaler.is_hinted(), expected, "{target:?} {classic:?}");
        assert_eq!(scaler.raster_policy().unwrap().grid_fit, classic);
    }
}

// Tinos asks for horizontal-only smoothing from 11 through 16 ppem, which a
// rasterizer needs to know as much as the interpreter does.
#[test]
fn cleartype_policy_is_reachable() {
    for (ppem, symmetric) in [(8.0, true), (13.0, false), (24.0, true)] {
        let scaler = ScalerFont::hinted(
            font(font_test_data::TINOS_SUBSET),
            ppem,
            HintOptions::interpreter(),
        );
        let policy = scaler.raster_policy().unwrap();
        assert_eq!(policy.symmetric_smoothing, symmetric, "at {ppem} ppem");
        assert!(policy.grid_fit, "at {ppem} ppem");
    }
    // Only the TrueType interpreter has a policy to report.
    let scaler = ScalerFont::hinted(
        font(font_test_data::NOTO_SERIF_DISPLAY_TRIMMED),
        12.0,
        HintOptions::interpreter(),
    );
    assert!(scaler.raster_policy().is_none());
    assert!(scaler.is_hinted());
    // And nothing is hinted in font units.
    let scaler = ScalerFont::unhinted(scaler.font().clone(), None);
    assert!(!scaler.is_hinted());
}

// Tinos turns on smart dropout control in its control value program, and a
// caller has to be able to see that to rasterize it to one bit a pixel.
#[test]
fn dropout_control_reaches_the_caller() {
    let hinted = ScalerFont::hinted(
        font(font_test_data::TINOS_SUBSET),
        16.0,
        HintOptions::interpreter(),
    );
    let unhinted = ScalerFont::unhinted(font(font_test_data::TINOS_SUBSET), Some(16.0));
    let mut seen = 0;
    for gid in 0..20 {
        let gid = GlyphId::new(gid);
        let Ok(metrics) = draw(&hinted, gid, &mut NullPen) else {
            continue;
        };
        assert_eq!(
            metrics.dropout_control,
            DropoutControl::Smart,
            "glyph {gid}"
        );
        assert!(metrics.dropout_control.is_enabled());
        assert!(metrics.dropout_control.is_smart());
        assert!(!metrics.dropout_control.includes_stubs());
        // Without a glyph program there is nothing to ask, so the TrueType
        // default stands.
        let metrics = draw(&unhinted, gid, &mut NullPen).unwrap();
        assert_eq!(metrics.dropout_control, DropoutControl::Simple);
        seen += 1;
    }
    assert!(seen > 0);
}

// A font whose control value program never touches scan control gets no
// dropout control at all, which is what FreeType does.
#[test]
fn dropout_control_off_when_unrequested() {
    let scaler = ScalerFont::hinted(font(font_test_data::CVAR), 16.0, HintOptions::interpreter());
    let mut seen = 0;
    for gid in 0..20 {
        let Ok(metrics) = draw(&scaler, GlyphId::new(gid), &mut NullPen) else {
            continue;
        };
        assert_eq!(
            metrics.dropout_control,
            DropoutControl::Ignore,
            "glyph {gid}"
        );
        assert!(!metrics.dropout_control.is_enabled());
        seen += 1;
    }
    assert!(seen > 0);
}

#[test]
fn hinting_defaults() {
    let hinting = HintOptions::auto_fallback();
    assert!(matches!(hinting.engine(), EngineRequest::AutoFallback));
    assert!(matches!(
        HintOptions::default().engine(),
        EngineRequest::AutoFallback
    ));
    assert_eq!(hinting.target(), RasterTarget::Normal);
    assert!(!hinting.classic_cleartype());
    assert!(hinting.symmetric_rendering());
    assert!(!hinting.preserve_linear_metrics());
    assert!(!hinting.pedantic());
}

// FreeType looks a glyph's `hdmx` advance up by the size rounded to a whole
// pixel, so a fractional size still takes the device width.
#[test]
fn hdmx_is_looked_up_by_the_rounded_size() {
    let font_ref = FontRef::new(font_test_data::TINOS_SUBSET).unwrap();
    let hdmx = font_ref.hdmx().unwrap();
    let record = hdmx.records().get(0).unwrap();
    let size = record.pixel_size() as f32;
    // A bilevel target applies the device widths; a smooth one draws in
    // backward compatibility mode, which leaves them alone.
    let scaler = ScalerFont::hinted(
        font(font_test_data::TINOS_SUBSET),
        size - 0.25,
        HintOptions::interpreter()
            .with_target(RasterTarget::Mono)
            .with_classic_cleartype(true),
    );
    assert!(scaler.is_hinted());
    for (gid, width) in record.widths().iter().enumerate() {
        let metrics = draw(&scaler, GlyphId::new(gid as u32), &mut NullPen).unwrap();
        assert_eq!(metrics.h_advance, Some(*width as f32), "gid {gid}");
    }
}

// FreeType takes no advance from `hdmx` for a font whose `post` table says
// it is fixed pitch.
#[test]
fn hdmx_is_ignored_for_a_fixed_pitch_font() {
    use raw::{model::Blob, types::Tag};
    let font_ref = FontRef::new(font_test_data::TINOS_SUBSET).unwrap();
    let record = font_ref.hdmx().unwrap().records().get(0).unwrap();
    let num_widths = record.widths().len();
    // Tinos's device widths are its hinted advances, so each is widened by
    // a pixel to tell them apart.
    let tinos = |fixed_pitch: bool| {
        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(move |tag| {
            let font = FontRef::new(font_test_data::TINOS_SUBSET).ok()?;
            let mut data = font.table_data(tag)?.as_bytes().to_vec();
            if tag == Tag::new(b"hdmx") {
                // The header, then the first record's size and maximum.
                data[10..10 + num_widths]
                    .iter_mut()
                    .for_each(|width| *width += 1);
            }
            if tag == Tag::new(b"post") && fixed_pitch {
                data[12..16].copy_from_slice(&1u32.to_be_bytes());
            }
            Some(Blob::from(data))
        });
        let hinting = HintOptions::interpreter()
            .with_target(RasterTarget::Mono)
            .with_classic_cleartype(true);
        ScalerFont::hinted(
            Font::new(source, 0).unwrap(),
            record.pixel_size() as f32,
            hinting,
        )
    };
    let (proportional, fixed_pitch) = (tinos(false), tinos(true));
    for (gid, width) in record.widths().iter().enumerate() {
        let gid = GlyphId::new(gid as u32);
        let advance = |scaler: &ScalerFont| draw(scaler, gid, &mut NullPen).unwrap().h_advance;
        assert_eq!(
            advance(&proportional),
            Some(*width as f32 + 1.0),
            "gid {gid}"
        );
        assert_eq!(advance(&fixed_pitch), Some(*width as f32), "gid {gid}");
    }
}

#[test]
fn a_type1_font_draws_unhinted() {
    let scaler = ScalerFont::hinted(
        font(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA),
        16.0,
        HintOptions::interpreter(),
    );
    assert!(!scaler.is_hinted());
    let mut drawn = 0;
    for gid in 0..scaler.font().num_glyphs() {
        let metrics = draw(&scaler, GlyphId::new(gid), &mut NullPen).unwrap();
        // Hinting was asked for, so the advance is rounded.
        if let Some(advance) = metrics.h_advance {
            assert_eq!(advance, advance.round());
        }
        drawn += 1;
    }
    assert!(drawn > 0);
}

#[test]
fn glyph_styles_can_be_shared_between_scalers() {
    let model = font(font_test_data::NOTOSERIF_AUTOHINT_SHAPING);
    let shared = ScalerFont::hinted(model.clone(), 14.0, HintOptions::auto());
    let own = ScalerFont::hinted(
        font(font_test_data::NOTOSERIF_AUTOHINT_SHAPING),
        14.0,
        HintOptions::auto(),
    );
    for gid in 1..10 {
        let gid = GlyphId::new(gid);
        assert_eq!(
            svg_scaler(&shared, gid).map(|(path, _)| path),
            svg_scaler(&own, gid).map(|(path, _)| path)
        );
    }
}

#[test]
fn hint_tables_match_the_outline_collection() {
    for data in [
        font_test_data::TINOS_SUBSET,
        font_test_data::COUSINE_HINT_SUBSET,
        font_test_data::CVAR,
    ] {
        let font = FontRef::new(data).unwrap();
        let theirs = crate::outline::glyf::Outlines::new(&font).unwrap();
        let theirs = theirs.hint_tables();
        let model = self::font(data);
        let ours = super::truetype::hint_tables(model.tables());
        assert_eq!(ours.fpgm, theirs.fpgm);
        assert_eq!(ours.prep, theirs.prep);
        assert_eq!(ours.cvt, theirs.cvt);
        assert_eq!(ours.cvar.is_some(), theirs.cvar.is_some());
        assert_eq!(ours.max_function_defs, theirs.max_function_defs);
        assert_eq!(ours.max_instruction_defs, theirs.max_instruction_defs);
        assert_eq!(ours.max_twilight_points, theirs.max_twilight_points);
        assert_eq!(ours.max_stack_elements, theirs.max_stack_elements);
        assert_eq!(ours.max_storage, theirs.max_storage);
        assert_eq!(ours.axis_count, theirs.axis_count);
    }
}

// Monochrome rendering rounds a composite's horizontal offset to the pixel
// grid under the newer ClearType rules, and nothing else differs from the
// classic ones for a monochrome target, so only composites can move.
#[test]
fn enhanced_mono_rounds_composite_x_offsets() {
    use raw::tables::glyf::{CompositeGlyphFlags, Glyph};
    let mut moved = 0;
    // Neither font has instructions, so the interpreter runs empty programs
    // and the loader's rounding is all that can differ.
    for data in [
        font_test_data::VAZIRMATN_VAR,
        font_test_data::MPLUS1CODE_VERTICAL_SUBSET,
    ] {
        let font = FontRef::new(data).unwrap();
        let (glyf, loca) = (font.glyf().unwrap(), font.loca(None).unwrap());
        for ppem in [9.0, 11.0, 13.0, 16.0, 24.0, 33.0] {
            let scalers = [true, false].map(|classic| {
                ScalerFont::hinted(
                    self::font(data),
                    ppem,
                    HintOptions::interpreter()
                        .with_target(RasterTarget::Mono)
                        .with_classic_cleartype(classic),
                )
            });
            for gid in 0..font.maxp().unwrap().num_glyphs() {
                let gid = GlyphId::new(gid as u32);
                let rounds_offsets = match loca.get_glyf(gid, &glyf) {
                    Ok(Some(Glyph::Composite(composite))) => composite
                        .component_glyphs_and_flags()
                        .any(|(_, flags)| flags.contains(CompositeGlyphFlags::ROUND_XY_TO_GRID)),
                    _ => false,
                };
                let [classic, enhanced] = scalers.each_ref().map(|scaler| svg_scaler(scaler, gid));
                if classic != enhanced {
                    assert!(rounds_offsets, "glyph {gid} at {ppem} moved without asking");
                    moved += 1;
                }
            }
        }
    }
    assert!(moved > 0, "no composite offset landed on the grid");
}

/// Returns the charstrings of an SFNT CFF font as a standalone CFF font.
fn standalone_cff(data: &[u8]) -> Font {
    let raw = FontRef::new(data)
        .unwrap()
        .cff()
        .unwrap()
        .offset_data()
        .as_bytes()
        .to_vec();
    Font::new(raw, 0).unwrap()
}

// A standalone CFF font scales and hints as the same charstrings do inside
// an SFNT font: the scaler only ever reaches them through the outlines.
#[test]
fn standalone_cff_scales_like_its_table() {
    let data = font_test_data::NOTO_SERIF_DISPLAY_TRIMMED;
    let scalers = |ppem: f32, hinting: Option<HintOptions>| {
        [font(data), standalone_cff(data)].map(|font| match hinting {
            Some(hinting) => ScalerFont::hinted(font, ppem, hinting),
            None => ScalerFont::unhinted(font, Some(ppem)),
        })
    };
    let mut compared = 0;
    for ppem in [9.0, 16.0, 24.8] {
        for hinting in [
            None,
            Some(HintOptions::interpreter()),
            Some(HintOptions::auto_fallback()),
            // The automatic hinter finds its blue zones by character, and a
            // standalone CFF font maps characters through its glyph names.
            #[cfg(feature = "agl")]
            Some(HintOptions::auto()),
        ] {
            let [theirs, ours] = scalers(ppem, hinting);
            assert_eq!(
                ours.is_hinted(),
                theirs.is_hinted(),
                "{hinting:?} at {ppem}"
            );
            for gid in 0..theirs.font().num_glyphs() {
                let gid = GlyphId::new(gid);
                assert_eq!(
                    svg_scaler(&ours, gid),
                    svg_scaler(&theirs, gid),
                    "glyph {gid} with {hinting:?} at {ppem}"
                );
                compared += 1;
            }
        }
    }
    assert!(compared > 0);
    // Without the names the automatic hinter still hints, without blue zones.
    let ours = ScalerFont::hinted(standalone_cff(data), 16.0, HintOptions::auto());
    assert!(ours.is_hinted());
    for gid in 1..ours.font().num_glyphs() {
        let (path, _) = svg_scaler(&ours, GlyphId::new(gid)).unwrap();
        assert!(!path.is_empty(), "glyph {gid}");
    }
}

// A cache entry can be handed another font: the scaler draws it as a fresh
// scaler would, whatever engine the previous font used.
#[test]
fn reconfigure_takes_a_new_font() {
    let tinos = font_test_data::TINOS_SUBSET;
    let cousine = font_test_data::COUSINE_HINT_SUBSET;
    let cff = font_test_data::NOTO_SERIF_DISPLAY_TRIMMED;
    let auto = font_test_data::NOTOSERIF_AUTOHINT_SHAPING;
    let mut scaler = ScalerFont::hinted(font(tinos), 16.0, HintOptions::interpreter());
    for (data, ppem, hinting) in [
        (cousine, 16.0, HintOptions::interpreter()),
        (cff, 12.0, HintOptions::interpreter()),
        (auto, 14.0, HintOptions::auto()),
        (tinos, 20.0, HintOptions::interpreter()),
    ] {
        scaler.reconfigure(font(data), ppem, hinting);
        let fresh = ScalerFont::hinted(font(data), ppem, hinting);
        assert_eq!(scaler.is_hinted(), fresh.is_hinted());
        assert_eq!(scaler.raster_policy(), fresh.raster_policy());
        for gid in 0..fresh.font().num_glyphs() {
            let gid = GlyphId::new(gid);
            assert_eq!(
                svg_scaler(&scaler, gid),
                svg_scaler(&fresh, gid),
                "glyph {gid}"
            );
        }
    }
}

// A location can give a glyph a fractional advance through HVAR. FreeType
// rounds it to a whole unit before the autohinter scales it, as the hinting
// instance does, so the scaler must too.
#[test]
fn autohinted_varied_advances_round_like_the_hinting_instance() {
    let data = font_test_data::CANTARELL_VF_TRIMMED;
    let font_ref = FontRef::new(data).unwrap();
    let mut compared = 0;
    for weight in (100..=900).step_by(10) {
        let location = font_ref.axes().location([("wght", weight as f32)]);
        let model = font(data)
            .instance_builder()
            .variations([("wght", weight as f32)])
            .build();
        for ppem in [9.0, 20.0, 64.0, 100.0] {
            let instance = HintingInstance::new(
                &font_ref.outline_glyphs(),
                Size::new(ppem),
                &location,
                HintingOptions {
                    engine: Engine::Auto(None),
                    target: OutlineTarget::default(),
                },
            )
            .unwrap();
            let scaler = ScalerFont::hinted(model.clone(), ppem, HintOptions::auto());
            for gid in 0..font_ref.maxp().unwrap().num_glyphs() {
                let gid = GlyphId::new(gid as u32);
                let Some(glyph) = font_ref.outline_glyphs().get(gid) else {
                    continue;
                };
                let theirs = glyph.draw(&instance, &mut NullPen).unwrap();
                let ours = draw(&scaler, gid, &mut NullPen).unwrap();
                assert_eq!(
                    ours.h_advance, theirs.advance_width,
                    "glyph {gid} at wght {weight}, {ppem} ppem"
                );
                compared += 1;
            }
        }
    }
    assert!(compared > 0);
}

// Where the control value program turns hinting off, FreeType has already
// chosen the interpreter's scale, so a font that asks for whole pixel sizes
// draws at the rounded one. Where the `gasp` table does, FreeType draws at
// the requested size. Either way the advance is rounded. HintingInstance
// draws both at the requested size, and is left as it is.
#[test]
fn hinting_turned_off_draws_at_freetypes_size() {
    let ahem = font(font_test_data::AHEM);
    assert!(ahem
        .tables()
        .head()
        .unwrap()
        .flags()
        .contains(raw::tables::head::Flags::FORCE_INTEGER_PPEM));
    let glyph = ahem.charmap().map_unicode('X').unwrap();
    let unhinted =
        |size| svg_scaler(&ScalerFont::unhinted(ahem.clone(), Some(size)), glyph).unwrap();
    // The control value program: the outline at 13 pixels.
    let classic = HintOptions::interpreter().with_classic_cleartype(true);
    let mut scaler = ScalerFont::hinted(ahem.clone(), 12.5, classic);
    let State::TrueType(hinter) = &mut scaler.state else {
        panic!("Ahem has glyf outlines");
    };
    hinter.simulate_program_disabling_hinting();
    assert!(!scaler.is_hinted());
    let (path, metrics) = svg_scaler(&scaler, glyph).unwrap();
    assert_eq!(path, unhinted(13.0).0);
    assert_eq!(metrics.h_advance, Some(13.0));
    // The gasp table, here through the policy for a font without
    // instructions: the outline at 12.5 pixels.
    let scaler = ScalerFont::hinted(ahem.clone(), 12.5, HintOptions::interpreter());
    assert!(!scaler.is_hinted());
    let (path, metrics) = svg_scaler(&scaler, glyph).unwrap();
    assert_eq!(path, unhinted(12.5).0);
    assert_ne!(path, unhinted(13.0).0);
    assert_eq!(metrics.h_advance, Some(13.0));
}

// A font that needs its instructions to draw correct contours, which
// FreeType calls "tricky", is run through the interpreter whatever engine is
// asked for, and its glyph programs run without backward compatibility, as
// FreeType runs them. On DFKai-SB and DFTT-L5 this draws every glyph as the
// interpreter does for a monochrome target, which is what Skia asks of
// HintingInstance for such a font.
#[test]
fn a_hint_reliant_font_runs_its_instructions_natively() {
    let tricky = hint_reliant_tinos();
    for classic in [false, true] {
        let scaler = ScalerFont::hinted(
            tricky.clone(),
            16.0,
            HintOptions::auto().with_classic_cleartype(classic),
        );
        let State::TrueType(hinter) = &scaler.state else {
            panic!("the interpreter runs a hint reliant font");
        };
        assert!(scaler.is_hinted());
        assert!(!hinter.backward_compatibility(), "{classic:?}");
    }
    // The same font under its own name keeps the engine it asked for.
    let plain = ScalerFont::hinted(
        font(font_test_data::TINOS_SUBSET),
        16.0,
        HintOptions::auto(),
    );
    assert!(matches!(plain.state, State::Auto(_)));
}

/// Returns Tinos named as one of the fonts FreeType knows needs its
/// instructions.
fn hint_reliant_tinos() -> Font {
    use raw::{model::Blob, types::Tag};
    let family: Vec<u8> = "MingLiU"
        .encode_utf16()
        .flat_map(u16::to_be_bytes)
        .collect();
    let mut name = alloc::vec![0, 0, 0, 1, 0, 18];
    for field in [3u16, 1, 0x409, 1, family.len() as u16, 0] {
        name.extend_from_slice(&field.to_be_bytes());
    }
    name.extend_from_slice(&family);
    let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(move |tag| {
        if tag == Tag::new(b"name") {
            return Some(Blob::from(name.clone()));
        }
        let font = FontRef::new(font_test_data::TINOS_SUBSET).ok()?;
        Some(Blob::from(font.table_data(tag)?.as_bytes().to_vec()))
    });
    let tricky = Font::new(source, 0).unwrap();
    assert!(tricky.is_hint_reliant());
    tricky
}

// An unhinted scaler hints a font that needs its instructions, as FreeType
// does, unless it is forced not to.
#[test]
fn unhinted_hints_a_hint_reliant_font_unless_forced() {
    let tricky = hint_reliant_tinos();
    let scaler = ScalerFont::unhinted(tricky.clone(), Some(16.0));
    assert!(scaler.is_hinted());
    assert_eq!(scaler.hint_options(), Some(HintOptions::interpreter()));
    let forced = ScalerFont::force_unhinted(tricky.clone(), Some(16.0));
    assert!(!forced.is_hinted());
    assert_eq!(forced.hint_options(), None);
    // Without a size there is nothing to hint.
    assert!(!ScalerFont::unhinted(tricky, None).is_hinted());
    let plain = ScalerFont::unhinted(font(font_test_data::TINOS_SUBSET), Some(16.0));
    assert!(!plain.is_hinted());
    assert_eq!(plain.hint_options(), None);
}

// The interpreter hints at a whole pixel size where the font asks for one;
// other engines hint at the size given.
#[test]
fn hinted_ppem_is_the_size_hinting_runs_at() {
    use raw::tables::head::Flags;
    let tinos = font(font_test_data::TINOS_SUBSET);
    let whole = tinos
        .tables()
        .head()
        .unwrap()
        .flags()
        .contains(Flags::FORCE_INTEGER_PPEM);
    assert!(whole, "Tinos asks for whole pixel sizes");
    let scaler = ScalerFont::hinted(tinos.clone(), 12.5, HintOptions::interpreter());
    assert_eq!(scaler.ppem(), Some(12.5));
    assert_eq!(scaler.hinted_ppem(), Some(13.0));
    let auto = ScalerFont::hinted(tinos.clone(), 12.5, HintOptions::auto());
    assert_eq!(auto.hinted_ppem(), Some(12.5));
    assert_eq!(ScalerFont::unhinted(tinos, None).hinted_ppem(), None);
}

/// Returns Tinos with one glyph whose program sets `SCANCTRL` to 255 and
/// `SCANTYPE` to 0, then fails with a truncated `PUSHB` if `fail` is set.
fn dropout_font(fail: bool) -> Font {
    use raw::{model::Blob, types::Tag};
    let mut program = alloc::vec![0xb0, 255, 0x85, 0xb0, 0, 0x8d];
    if fail {
        program.push(0xb0);
    }
    let mut glyf = font_test_data::extended::simple_glyph(&[
        (0, 0, 1),
        (100, 0, 1),
        (100, 100, 1),
        (0, 100, 1),
    ]);
    glyf[12..14].copy_from_slice(&(program.len() as u16).to_be_bytes());
    glyf.splice(14..14, program);
    let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(move |tag| {
        let base = FontRef::new(font_test_data::TINOS_SUBSET).ok()?;
        if tag == Tag::new(b"glyf") {
            return Some(Blob::from(glyf.clone()));
        }
        if tag == Tag::new(b"loca") {
            let mut loca = 0u32.to_be_bytes().to_vec();
            for _ in 0..base.maxp().ok()?.num_glyphs() {
                loca.extend_from_slice(&(glyf.len() as u32).to_be_bytes());
            }
            return Some(Blob::from(loca));
        }
        let mut data = base.table_data(tag)?.as_bytes().to_vec();
        if tag == Tag::new(b"head") {
            data[50..52].copy_from_slice(&1i16.to_be_bytes());
        }
        Some(Blob::from(data))
    });
    Font::new(source, 0).unwrap()
}

// A glyph program that fails after setting the dropout mode keeps that
// mode when hinting is not pedantic, as in FreeType.
#[test]
fn a_tolerated_program_failure_keeps_its_dropout_control() {
    for fail in [false, true] {
        let scaler = ScalerFont::hinted(
            dropout_font(fail),
            16.0,
            HintOptions::interpreter().with_target(RasterTarget::Mono),
        );
        assert!(scaler.hinting_error().is_none());
        let metrics = scaler
            .outline(GlyphId::new(0))
            .unwrap()
            .draw(DrawMemory::Scratch, &mut alloc::vec::Vec::new())
            .unwrap();
        assert_eq!(
            metrics.dropout_control,
            DropoutControl::SimpleWithStubs,
            "fail: {fail}"
        );
    }
}

// The fallback policy grid fits an instructed font whichever maximum profile
// says it has instructions.
#[test]
fn an_extended_maxp_keeps_grid_fitting() {
    use raw::{model::Blob, types::Tag};
    for extended in [false, true] {
        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(move |tag| {
            if tag == Tag::new(b"gasp") {
                return None;
            }
            let base = FontRef::new(font_test_data::TINOS_SUBSET).ok()?;
            if extended && tag == Tag::new(b"maxp") {
                return None;
            }
            if extended && tag == Tag::new(b"MAXP") {
                let maxp = base.table_data(Tag::new(b"maxp"))?;
                // Widen numGlyphs to 24 bits.
                let mut data = maxp.as_bytes()[..4].to_vec();
                data.push(0);
                data.extend_from_slice(&maxp.as_bytes()[4..]);
                return Some(Blob::from(data));
            }
            base.table_data(tag)
                .map(|data| Blob::from(data.as_bytes().to_vec()))
        });
        let scaler = ScalerFont::hinted(
            Font::new(source, 0).unwrap(),
            16.0,
            HintOptions::interpreter(),
        );
        assert!(scaler.is_hinted(), "extended: {extended}");
    }
}

// The autohinter draws outlines from GLYF and LOCA.
#[test]
fn autohinter_draws_uppercase_glyf_outlines() {
    let data = font_test_data::TINOS_SUBSET;
    let lower = ScalerFont::hinted(font(data), 16.0, HintOptions::auto());
    let upper = ScalerFont::hinted(font(&uppercase_glyf(data)), 16.0, HintOptions::auto());
    for gid in 0..lower.font().num_glyphs() {
        let glyph = GlyphId::new(gid);
        let drawn = svg_scaler(&upper, glyph);
        assert!(drawn.is_some(), "glyph {gid}");
        assert_eq!(drawn, svg_scaler(&lower, glyph), "glyph {gid}");
    }
}

/// `data` with its `glyf` and `loca` tables renamed `GLYF` and `LOCA`.
fn uppercase_glyf(data: &[u8]) -> Vec<u8> {
    let count = u16::from_be_bytes([data[4], data[5]]) as usize;
    let mut records: Vec<[u8; 16]> = data[12..12 + 16 * count]
        .chunks_exact(16)
        .map(|record| record.try_into().unwrap())
        .collect();
    for record in &mut records {
        match &record[..4] {
            b"glyf" => record[..4].copy_from_slice(b"GLYF"),
            b"loca" => record[..4].copy_from_slice(b"LOCA"),
            _ => {}
        }
    }
    // The directory stays sorted by tag.
    records.sort();
    let mut font = data.to_vec();
    font[12..12 + 16 * count].copy_from_slice(&records.concat());
    font
}

/// Returns the charstrings of a CID-keyed SFNT CFF font as a standalone
/// font, with the private dict of font dict `damaged` made unreadable.
fn cid_cff_with_a_bad_private_dict(data: &[u8], damaged: usize) -> Vec<u8> {
    use raw::ps::cff::{
        dict::{entries, Entry},
        index::Index,
    };
    let cff = FontRef::new(data).unwrap().cff().unwrap();
    let mut raw = cff.offset_data().as_bytes().to_vec();
    let top = cff.top_dicts().get(0).unwrap();
    let fd_array = entries(top, None)
        .find_map(|entry| match entry {
            Ok(Entry::FdArrayOffset(offset)) => Some(offset),
            _ => None,
        })
        .unwrap();
    let font_dicts = Index::new(&raw[fd_array..], false).unwrap();
    let private = entries(font_dicts.get(damaged).unwrap(), None)
        .find_map(|entry| match entry {
            Ok(Entry::PrivateDictRange(range)) => Some(range),
            _ => None,
        })
        .unwrap();
    // A Subrs offset of -1, which no private dict can resolve.
    let damage = [29u8, 0xff, 0xff, 0xff, 0xff, 19];
    assert!(private.len() >= damage.len());
    raw[private.start..private.start + damage.len()].copy_from_slice(&damage);
    raw
}

// A subfont that cannot be read fails only its own glyphs; the others hint
// as they would in the undamaged font.
#[test]
fn a_bad_subfont_fails_only_its_own_glyphs() {
    use raw::ps::cff::CffFontRef;
    let data = font_test_data::NOTO_SANS_JP_CFF;
    let damaged = 1;
    let raw = cid_cff_with_a_bad_private_dict(data, damaged);
    let cff = CffFontRef::new_cff(&raw, 0, None).unwrap();
    assert!(cff.num_subfonts() as usize > damaged);
    let theirs = ScalerFont::hinted(standalone_cff(data), 16.0, HintOptions::interpreter());
    let ours = ScalerFont::hinted(
        Font::new(raw.clone(), 0).unwrap(),
        16.0,
        HintOptions::interpreter(),
    );
    assert!(ours.is_hinted());
    let mut drawn = 0;
    for gid in 0..ours.font().num_glyphs() {
        let glyph = GlyphId::new(gid);
        if cff.subfont_index(glyph) == Some(damaged as u16) {
            assert!(svg_scaler(&ours, glyph).is_none(), "glyph {gid}");
            continue;
        }
        assert_eq!(
            svg_scaler(&ours, glyph),
            svg_scaler(&theirs, glyph),
            "glyph {gid}"
        );
        drawn += 1;
    }
    assert!(drawn > 0);
}

// A scaler's metrics are the font's, scaled linearly to its size, or in
// font units without one.
#[test]
fn metrics_are_scaled_to_the_size() {
    let font = font(font_test_data::TINOS_SUBSET);
    let upem = font.units_per_em() as f32;
    for scaler in [
        ScalerFont::hinted(font.clone(), 16.0, HintOptions::interpreter()),
        ScalerFont::unhinted(font.clone(), Some(16.0)),
    ] {
        assert_eq!(
            scaler.metrics().h_line(),
            font.metrics().scaled_f32(16.0).h_line()
        );
        assert_eq!(
            scaler.style_metrics().underline(),
            font.style_metrics().scaled_f32(16.0).underline()
        );
    }
    let unscaled = ScalerFont::unhinted(font.clone(), None);
    assert_eq!(
        unscaled.metrics().h_line(),
        font.metrics().scaled_f32(upem).h_line()
    );
    let line = font.metrics().h_line().unwrap();
    assert_eq!(
        unscaled.metrics().h_line().unwrap().ascender,
        line.ascender.to_f32()
    );
}

// Linear glyph metrics are the font's, scaled to the size, with extents
// measured from the sources the scaler draws from.
#[test]
fn linear_glyph_metrics_follow_the_scaler() {
    let font = font(font_test_data::TINOS_SUBSET);
    let scaler = ScalerFont::hinted(font.clone(), 16.0, HintOptions::interpreter());
    let linear = scaler.linear_glyph_metrics();
    let expected = font.glyph_metrics().scaled_f32(16.0);
    let mut differs = false;
    for gid in 0..font.num_glyphs() {
        let glyph = GlyphId::new(gid);
        assert_eq!(linear.h_advance(glyph), expected.h_advance(glyph));
        assert_eq!(linear.extents(glyph), expected.extents(glyph));
        // Hinting rounds the drawn advance, which the linear one is not.
        let drawn = draw(&scaler, glyph, &mut NullPen).unwrap();
        differs |= drawn.h_advance != Some(linear.h_advance(glyph));
    }
    assert!(differs);
    // A scaler drawing only from `COLR` measures nothing in a font without
    // it.
    let color = ScalerFont::unhinted(
        ScalerSource::new(font.clone(), GlyphSourceOrder::COLOR),
        Some(16.0),
    );
    assert!(expected.extents(GlyphId::new(1)).is_some());
    assert!(color
        .linear_glyph_metrics()
        .extents(GlyphId::new(1))
        .is_none());
}

// The engine a scaler runs follows the options and the font.
#[test]
fn hint_engine_reports_what_runs() {
    let tinos = font(font_test_data::TINOS_SUBSET);
    let cff = font(font_test_data::NOTO_SERIF_DISPLAY_TRIMMED);
    let engine =
        |font: &Font, options| ScalerFont::hinted(font.clone(), 16.0, options).hint_engine();
    assert_eq!(
        engine(&tinos, HintOptions::interpreter()),
        Some(HintEngine::TrueType)
    );
    assert_eq!(
        engine(&tinos, HintOptions::auto_fallback()),
        Some(HintEngine::TrueType)
    );
    assert_eq!(engine(&tinos, HintOptions::auto()), Some(HintEngine::Auto));
    assert_eq!(
        engine(&cff, HintOptions::interpreter()),
        Some(HintEngine::PostScript)
    );
    // A PostScript font always runs its own hints when they are allowed.
    assert_eq!(
        engine(&cff, HintOptions::auto_fallback()),
        Some(HintEngine::PostScript)
    );
    assert_eq!(engine(&cff, HintOptions::auto()), Some(HintEngine::Auto));
    // Unhinted, except for a font that needs its instructions.
    assert_eq!(
        ScalerFont::unhinted(tinos.clone(), Some(16.0)).hint_engine(),
        None
    );
    assert_eq!(
        ScalerFont::unhinted(hint_reliant_tinos(), Some(16.0)).hint_engine(),
        Some(HintEngine::TrueType)
    );
    assert_eq!(
        ScalerFont::unhinted(hint_reliant_tinos(), None).hint_engine(),
        None
    );
}

// Debug output names the scaler's configuration without dumping the font.
#[test]
fn scalers_debug_print_their_configuration() {
    let font = font(font_test_data::TINOS_SUBSET);
    let scaler = ScalerFont::hinted(font.clone(), 16.0, HintOptions::interpreter());
    let printed = format!("{scaler:?}");
    assert!(printed.starts_with("ScalerFont { outline_source: Some(Glyf), ppem: Some(16.0)"));
    assert!(printed.contains("hint_engine: Some(TrueType)"), "{printed}");
    let outline = scaler.outline(GlyphId::new(1)).unwrap();
    assert_eq!(
        format!("{outline:?}"),
        "ScalerOutline { glyph: GlyphId(1), source: Glyf }"
    );
    let source = ScalerSource::from(font);
    assert!(format!("{source:?}").starts_with("ScalerSource { order: [Sbix"));
}
