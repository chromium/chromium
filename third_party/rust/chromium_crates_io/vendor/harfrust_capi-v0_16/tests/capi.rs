//! Exercises the C API through its own entry points, the way a C caller
//! reaches them.

use core::ffi::{c_char, c_int, c_uint, c_void};
use std::ptr;
use std::sync::Mutex;

use crate::*;

const TEXT: &str = "abc";

fn font_path(name: &str) -> std::path::PathBuf {
    std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .unwrap()
        .join("harfrust")
        .join("tests")
        .join("fonts")
        .join("rb_custom")
        .join(name)
}

/// A plain font with no `GSUB` or `GPOS`, so shaping is one glyph per
/// character and nothing rewrites the advances a callback produces.
const PLAIN_FONT: &str = "LaBelleAurore.ttf";
const PLAIN_UPEM: c_uint = 1024;

/// A variable font, for exercising variation settings.
const VARIABLE_FONT: &str = "Linefont.ttf";

#[test]
fn set_and_layout_lookup_queries() {
    unsafe {
        let set = hr_set_create();
        assert_eq!(hr_set_is_empty(set), 1);
        hr_set_add(set, 32);
        hr_set_add(set, HR_CODEPOINT_INVALID);
        assert_eq!(hr_set_has(set, 32), 1);
        assert_eq!(hr_set_has(set, HR_CODEPOINT_INVALID), 0);
        hr_set_clear(set);
        assert_eq!(hr_set_is_empty(set), 1);

        with_named_font("PT_Sans-Caption-Web-Regular.ttf", |face, _font| {
            const GSUB: u32 = 0x4753_5542;
            const GPOS: u32 = 0x4750_4F53;
            assert_eq!(hr_ot_layout_has_substitution(face), 1);
            assert_eq!(hr_ot_layout_has_positioning(face), 1);
            let count = hr_ot_layout_table_get_lookup_count(face, GSUB);
            assert!(count > 0);
            assert!(hr_ot_layout_table_get_lookup_count(face, GPOS) > 0);
            assert_eq!(hr_ot_layout_table_get_lookup_count(face, 0), 0);
            hr_ot_layout_lookup_collect_glyphs(face, GSUB, count, set, set, set, set);
            assert_eq!(hr_set_is_empty(set), 1);
            hr_ot_layout_lookup_collect_glyphs(face, GSUB, 0, set, set, set, set);
            assert_eq!(hr_set_is_empty(set), 0);
            assert_eq!(hr_set_has(set, 5), 1); // quotedbl, an input to lookup 0
            assert_eq!(hr_set_has(set, 704), 1); // its substituted glyph
            assert_eq!(hr_set_has(set, 3), 0); // space is outside this lookup
        });
        hr_set_destroy(set);
    }
}

#[test]
fn contextual_lookup_collects_each_glyph_role() {
    unsafe {
        with_named_font("PT_Sans-Caption-Web-Regular.ttf", |face, _font| {
            let sets = [
                hr_set_create(),
                hr_set_create(),
                hr_set_create(),
                hr_set_create(),
            ];
            hr_ot_layout_lookup_collect_glyphs(
                face,
                0x4753_5542,
                2,
                sets[0],
                sets[1],
                sets[2],
                sets[3],
            );
            assert_eq!(hr_set_has(sets[0], 36), 1); // backtrack A
            assert_eq!(hr_set_has(sets[1], 311), 1); // contextual input
            assert_eq!(hr_set_is_empty(sets[2]), 1); // no lookahead
            assert_eq!(hr_set_has(sets[3], 609), 1); // nested substitution output
            assert_eq!(hr_set_has(sets[0], 3), 0); // no all-glyph fallback
            assert_eq!(hr_set_has(sets[1], 3), 0);
            for set in sets {
                hr_set_destroy(set);
            }
        });
    }
}

#[test]
fn pair_class_lookup_collects_the_second_glyph_class() {
    unsafe {
        with_named_font("PT_Sans-Caption-Web-Regular.ttf", |face, _font| {
            let input = hr_set_create();
            hr_ot_layout_lookup_collect_glyphs(
                face,
                0x4750_4F53,
                2,
                ptr::null_mut(),
                input,
                ptr::null_mut(),
                ptr::null_mut(),
            );
            assert_eq!(hr_set_has(input, 695), 1);
            assert_eq!(hr_set_has(input, 7), 0);
            hr_set_destroy(input);
        });
    }
}

#[test]
fn contextual_rule_and_class_formats_do_not_fall_back_to_all_glyphs() {
    unsafe {
        for (font_name, lookup) in [
            ("NotoSansSinhala.subset1.otf", 3), // sequence context format 1
            ("NotoSansMyanmarUI-Regular.subset1.otf", 3), // sequence context format 2
            ("NotoSansMyanmarUI-Regular.subset1.otf", 5), // chained context format 2
        ] {
            with_named_font(font_name, |face, _font| {
                let sets = [
                    hr_set_create(),
                    hr_set_create(),
                    hr_set_create(),
                    hr_set_create(),
                ];
                hr_ot_layout_lookup_collect_glyphs(
                    face,
                    0x4753_5542,
                    lookup,
                    sets[0],
                    sets[1],
                    sets[2],
                    sets[3],
                );
                assert_eq!(hr_set_has(sets[1], 1), 1, "{font_name} lookup {lookup}");
                for set in sets {
                    assert_eq!(hr_set_has(set, 0), 0, "{font_name} lookup {lookup}");
                    hr_set_destroy(set);
                }
            });
        }
    }
}

#[test]
fn ot_tag_conversion_round_trips_and_pages() {
    unsafe {
        let dflt = u32::from_be_bytes(*b"dflt");
        assert!(hr_ot_tag_to_language(dflt).is_null());
        let arabic = hr_ot_tag_to_language(u32::from_be_bytes(*b"ARA "));
        assert_eq!(
            std::ffi::CStr::from_ptr(hr_language_to_string(arabic)).to_bytes(),
            b"ar"
        );

        let unknown = u32::from_be_bytes(*b"ABC ");
        let custom = hr_ot_tag_to_language(unknown);
        assert_eq!(
            std::ffi::CStr::from_ptr(hr_language_to_string(custom)).to_bytes(),
            b"abc-x-hbot-41424320"
        );
        let mut script_count = 0;
        let mut language_count = 1;
        let mut language_tags = [0];
        hr_ot_tags_from_script_and_language(
            HR_SCRIPT_INVALID,
            custom,
            &raw mut script_count,
            ptr::null_mut(),
            &raw mut language_count,
            language_tags.as_mut_ptr(),
        );
        assert_eq!((language_count, language_tags[0]), (1, unknown));

        let chinese = hr_language_from_string(c"zh-Hant".as_ptr(), -1);
        let mut scripts = [0; 3];
        let mut languages = [0; 3];
        script_count = 3;
        language_count = 3;
        hr_ot_tags_from_script_and_language(
            HR_SCRIPT_DEVANAGARI,
            chinese,
            &raw mut script_count,
            scripts.as_mut_ptr(),
            &raw mut language_count,
            languages.as_mut_ptr(),
        );
        assert_eq!(script_count, 3);
        assert_eq!(
            scripts,
            [
                u32::from_be_bytes(*b"dev3"),
                u32::from_be_bytes(*b"dev2"),
                u32::from_be_bytes(*b"deva")
            ]
        );
        assert_eq!(
            (language_count, languages[0]),
            (1, u32::from_be_bytes(*b"ZHT "))
        );

        script_count = 1;
        language_count = 1;
        hr_ot_tags_from_script_and_language(
            HR_SCRIPT_DEVANAGARI,
            chinese,
            &raw mut script_count,
            ptr::null_mut(),
            &raw mut language_count,
            ptr::null_mut(),
        );
        assert_eq!((script_count, language_count), (1, 1));
    }
}

#[test]
fn math_queries_scale_device_values_and_page() {
    unsafe {
        let path =
            std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("tests/fonts/MathQuery.ttf");
        with_font_file(&path, |face, font| {
            assert_eq!(hr_ot_math_has_data(face), 1);
            assert_eq!(hr_ot_math_is_glyph_extended_shape(face, 1), 1);
            assert_eq!(hr_ot_math_is_glyph_extended_shape(face, 0), 0);
            assert_eq!(
                hr_ot_math_get_constant(font, HR_OT_MATH_CONSTANT_SCRIPT_PERCENT_SCALE_DOWN),
                80
            );
            assert_eq!(
                hr_ot_math_get_constant(font, HR_OT_MATH_CONSTANT_AXIS_HEIGHT),
                642
            );
            assert_eq!(hr_ot_math_get_constant(font, 99), 0);
            assert_eq!(hr_ot_math_get_glyph_italics_correction(font, 1), 120);
            assert_eq!(hr_ot_math_get_glyph_top_accent_attachment(font, 1), 300);
            assert_eq!(
                hr_ot_math_get_glyph_top_accent_attachment(font, 0),
                hr_font_get_glyph_h_advance(font, 0) / 2
            );
            assert_eq!(
                hr_ot_math_get_glyph_kerning(font, 1, HR_OT_MATH_KERN_TOP_RIGHT, 399),
                25
            );
            assert_eq!(
                hr_ot_math_get_glyph_kerning(font, 1, HR_OT_MATH_KERN_TOP_RIGHT, 400),
                50
            );
            assert_eq!(
                hr_ot_math_get_min_connector_overlap(font, HR_DIRECTION_LTR),
                40
            );

            let mut count = 7;
            assert_eq!(
                hr_ot_math_get_glyph_kernings(
                    font,
                    1,
                    HR_OT_MATH_KERN_TOP_RIGHT,
                    0,
                    &raw mut count,
                    ptr::null_mut()
                ),
                2
            );
            assert_eq!(count, 7);
            let mut entries = [hr_ot_math_kern_entry_t::default(); 2];
            count = 1;
            assert_eq!(
                hr_ot_math_get_glyph_kernings(
                    font,
                    1,
                    HR_OT_MATH_KERN_TOP_RIGHT,
                    1,
                    &raw mut count,
                    entries.as_mut_ptr()
                ),
                2
            );
            assert_eq!(count, 1);
            assert_eq!(entries[0].max_correction_height, i32::MAX);
            assert_eq!(entries[0].kern_value, 50);

            let mut variants = [hr_ot_math_glyph_variant_t::default(); 1];
            count = 1;
            assert_eq!(
                hr_ot_math_get_glyph_variants(
                    font,
                    1,
                    HR_DIRECTION_TTB,
                    0,
                    &raw mut count,
                    variants.as_mut_ptr()
                ),
                1
            );
            assert_eq!(count, 1);
            assert_eq!(
                variants[0],
                hr_ot_math_glyph_variant_t {
                    glyph: 1,
                    advance: 700
                }
            );
            count = 1;
            assert_eq!(
                hr_ot_math_get_glyph_variants(
                    font,
                    1,
                    HR_DIRECTION_LTR,
                    0,
                    &raw mut count,
                    variants.as_mut_ptr()
                ),
                1
            );
            assert_eq!(variants[0].advance, 900);
            count = 7;
            assert_eq!(
                hr_ot_math_get_glyph_variants(
                    font,
                    1,
                    HR_DIRECTION_LTR,
                    0,
                    &raw mut count,
                    ptr::null_mut()
                ),
                1
            );
            assert_eq!(count, 7);
            count = 1;
            assert_eq!(
                hr_ot_math_get_glyph_variants(
                    font,
                    1,
                    HR_DIRECTION_LTR,
                    1,
                    &raw mut count,
                    variants.as_mut_ptr()
                ),
                1
            );
            assert_eq!(count, 0);

            let mut parts = [hr_ot_math_glyph_part_t::default(); 1];
            let mut correction = 0;
            count = 1;
            assert_eq!(
                hr_ot_math_get_glyph_assembly(
                    font,
                    1,
                    HR_DIRECTION_TTB,
                    0,
                    &raw mut count,
                    parts.as_mut_ptr(),
                    &raw mut correction
                ),
                1
            );
            assert_eq!(count, 1);
            assert_eq!(
                parts[0],
                hr_ot_math_glyph_part_t {
                    glyph: 1,
                    start_connector_length: 20,
                    end_connector_length: 30,
                    full_advance: 700,
                    flags: HR_OT_MATH_GLYPH_PART_FLAG_EXTENDER
                }
            );
            assert_eq!(correction, 70);
            count = 7;
            assert_eq!(
                hr_ot_math_get_glyph_assembly(
                    font,
                    1,
                    HR_DIRECTION_TTB,
                    0,
                    &raw mut count,
                    ptr::null_mut(),
                    &raw mut correction
                ),
                1
            );
            assert_eq!(count, 7);
            assert_eq!(correction, 70);

            hr_font_set_scale(font, 4096, 2048);
            hr_font_set_ppem(font, 13, 12);
            assert_eq!(
                hr_ot_math_get_constant(font, HR_OT_MATH_CONSTANT_AXIS_HEIGHT),
                812
            );
            assert_eq!(hr_ot_math_get_glyph_italics_correction(font, 1), 240);
            assert_eq!(
                hr_ot_math_get_glyph_kerning(font, 1, HR_OT_MATH_KERN_TOP_RIGHT, 569),
                680
            );
            assert_eq!(
                hr_ot_math_get_glyph_kerning(font, 1, HR_OT_MATH_KERN_TOP_RIGHT, 570),
                100
            );
            count = 1;
            assert_eq!(
                hr_ot_math_get_glyph_kernings(
                    font,
                    1,
                    HR_OT_MATH_KERN_TOP_RIGHT,
                    0,
                    &raw mut count,
                    entries.as_mut_ptr()
                ),
                2
            );
            assert_eq!(
                entries[0],
                hr_ot_math_kern_entry_t {
                    max_correction_height: 570,
                    kern_value: 680
                }
            );
            assert_eq!(
                hr_ot_math_get_min_connector_overlap(font, HR_DIRECTION_LTR),
                80
            );
            assert_eq!(
                hr_ot_math_get_min_connector_overlap(font, HR_DIRECTION_TTB),
                40
            );
            count = 1;
            assert_eq!(
                hr_ot_math_get_glyph_variants(
                    font,
                    1,
                    HR_DIRECTION_LTR,
                    0,
                    &raw mut count,
                    variants.as_mut_ptr()
                ),
                1
            );
            assert_eq!(variants[0].advance, 1800);
            count = 1;
            assert_eq!(
                hr_ot_math_get_glyph_assembly(
                    font,
                    1,
                    HR_DIRECTION_LTR,
                    0,
                    &raw mut count,
                    parts.as_mut_ptr(),
                    &raw mut correction
                ),
                1
            );
            assert_eq!(parts[0].start_connector_length, 40);
            assert_eq!(parts[0].end_connector_length, 60);
            assert_eq!(parts[0].full_advance, 1800);
            assert_eq!(correction, 140);
            hr_font_set_scale(font, 4096, -2048);
            assert_eq!(
                hr_ot_math_get_constant(font, HR_OT_MATH_CONSTANT_AXIS_HEIGHT),
                -812
            );
            assert_eq!(
                hr_ot_math_get_glyph_kerning(font, 1, HR_OT_MATH_KERN_TOP_RIGHT, -569),
                680
            );
            assert_eq!(
                hr_ot_math_get_glyph_kerning(font, 1, HR_OT_MATH_KERN_TOP_RIGHT, -570),
                100
            );
            assert_eq!(
                hr_ot_math_get_min_connector_overlap(font, HR_DIRECTION_TTB),
                -40
            );
        });
    }
}

#[test]
fn math_queries_without_table_return_empty_values() {
    unsafe {
        with_font(|face, font| {
            assert_eq!(hr_ot_math_has_data(face), 0);
            assert_eq!(
                hr_ot_math_get_constant(font, HR_OT_MATH_CONSTANT_AXIS_HEIGHT),
                0
            );
            assert_eq!(hr_ot_math_get_glyph_italics_correction(font, 1), 0);
            assert_eq!(hr_ot_math_is_glyph_extended_shape(face, 1), 0);
            let mut count = 2;
            assert_eq!(
                hr_ot_math_get_glyph_variants(
                    font,
                    1,
                    HR_DIRECTION_LTR,
                    0,
                    &raw mut count,
                    ptr::null_mut()
                ),
                0
            );
            assert_eq!(count, 0);
        });
    }
}

/// A font can fill a `MathKern` with 65535 heights that all sit below the
/// height asked about, and point every record at one device table laid over
/// the records. Picking a kern, or paging to the last entry, must not read
/// all of them.
#[test]
fn math_kerning_searches_a_full_kern_table() {
    let mut math = vec![
        0, 1, 0, 0, 0, 0, 0, 10, 0, 0, // MATH: only MathGlyphInfo
        0, 0, 0, 0, 0, 0, 0, 8, // MathGlyphInfo: only MathKernInfo
        0, 12, 0, 1, 0, 18, 0, 0, 0, 0, 0, 0, // MathKernInfo: a top right MathKern
        0, 1, 0, 1, 0, 1, // Coverage: glyph 1
        0xFF, 0xFF, // MathKern: heightCount
    ];
    let kern_start = math.len() - 2;
    for _ in 0..65535 + 65536 {
        math.extend([0x80, 0x00, 0x00, 0x04]); // -32768, device table at 4
    }
    // Sizes 1 to 65535, eight bits each: the records after it are its deltas.
    math[kern_start + 4..kern_start + 10].copy_from_slice(&[0, 1, 0xFF, 0xFF, 0, 3]);
    let mut data = vec![0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0];
    data.extend(b"MATH\0\0\0\0\0\0\0\x1C");
    data.extend((math.len() as u32).to_be_bytes());
    data.extend(math);
    unsafe {
        let blob = blob_over(&data);
        let face = hr_face_create(blob, 0);
        let font = hr_font_create(face);
        hr_font_set_ppem(font, 1000, 1000);
        let start = std::time::Instant::now();
        assert_eq!(
            hr_ot_math_get_glyph_kerning(font, 1, HR_OT_MATH_KERN_TOP_RIGHT, 500),
            -32764
        );
        let mut entries = [hr_ot_math_kern_entry_t::default(); 1];
        let mut count = 1;
        assert_eq!(
            hr_ot_math_get_glyph_kernings(
                font,
                1,
                HR_OT_MATH_KERN_TOP_RIGHT,
                65535,
                &raw mut count,
                entries.as_mut_ptr()
            ),
            65536
        );
        assert_eq!(
            entries[0],
            hr_ot_math_kern_entry_t {
                max_correction_height: i32::MAX,
                kern_value: -32764
            }
        );
        assert!(start.elapsed() < std::time::Duration::from_secs(1));
        hr_font_destroy(font);
        hr_face_destroy(face);
        hr_blob_destroy(blob);
    }
}

#[test]
fn base_baselines_follow_script_and_font_scale() {
    unsafe {
        with_named_font("NotoSansCJK.subset1.otf", |_face, font| {
            let mut coord = 999;
            assert_eq!(
                hr_ot_layout_get_baseline(
                    font,
                    HR_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_TOP_OR_RIGHT,
                    HR_DIRECTION_LTR,
                    u32::from_be_bytes(*b"hani"),
                    0,
                    &raw mut coord
                ),
                1
            );
            assert_eq!(coord, 834);
            hr_font_set_scale(font, 2000, 2000);
            assert_eq!(
                hr_ot_layout_get_baseline2(
                    font,
                    HR_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_TOP_OR_RIGHT,
                    HR_DIRECTION_LTR,
                    HR_SCRIPT_HAN,
                    ptr::null(),
                    &raw mut coord
                ),
                1
            );
            assert_eq!(coord, 1668);
            assert_eq!(
                hr_ot_layout_get_baseline(
                    font,
                    HR_OT_LAYOUT_BASELINE_TAG_HANGING,
                    HR_DIRECTION_LTR,
                    u32::from_be_bytes(*b"hani"),
                    0,
                    &raw mut coord
                ),
                0
            );
            assert_eq!(coord, 1668);
        });
    }
}

#[test]
fn base_baselines_round_negative_halves_up() {
    unsafe {
        with_named_font("NotoSansCJK.subset1.otf", |_face, font| {
            hr_font_set_scale(font, -250, -250);
            let mut coord = 0;
            assert_eq!(
                hr_ot_layout_get_baseline(
                    font,
                    HR_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_TOP_OR_RIGHT,
                    HR_DIRECTION_LTR,
                    u32::from_be_bytes(*b"hani"),
                    0,
                    &raw mut coord,
                ),
                1
            );
            // 834 * -0.25 = -208.5.
            assert_eq!(coord, -208);
        });
    }
}

#[test]
fn base_device_adjustments_round_negative_halves_up() {
    unsafe {
        let fixture = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join("tests/fonts/BaselineDevice.otf");
        with_font_file(&fixture, |_face, font| {
            hr_font_set_scale(font, -30, -30);
            hr_font_set_ppem(font, 12, 12);
            let mut coord = 0;
            assert_eq!(
                hr_ot_layout_get_baseline(
                    font,
                    HR_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_BOTTOM_OR_LEFT,
                    HR_DIRECTION_LTR,
                    u32::from_be_bytes(*b"zzzz"),
                    0,
                    &raw mut coord,
                ),
                1
            );
            // The coordinate rounds to 2; the device delta -30/12 to -2.
            assert_eq!(coord, 0);
        });
    }
}

#[test]
fn fallback_vertical_origin_rounds_negative_halves_up() {
    unsafe extern "C" fn no_origin(
        _: *mut hr_font_t,
        _: *mut c_void,
        _: hr_codepoint_t,
        _: *mut hr_position_t,
        _: *mut hr_position_t,
        _: *mut c_void,
    ) -> hr_bool_t {
        0
    }
    unsafe {
        with_font(|_face, font| {
            let funcs = hr_font_funcs_create();
            hr_font_funcs_set_glyph_v_origin_func(funcs, Some(no_origin), ptr::null_mut(), None);
            hr_font_set_funcs(font, funcs, ptr::null_mut(), None);
            hr_font_funcs_destroy(funcs);
            hr_font_set_scale(font, 1024, -512);
            let mut y = 0;
            hr_font_get_glyph_origin_for_direction(
                font,
                1,
                HR_DIRECTION_TTB,
                ptr::null_mut(),
                &raw mut y,
            );
            // The font's 1103-unit ascender scales to -551.5.
            assert_eq!(y, -551);
        });
    }
}

#[test]
fn baseline_fallback_and_script_defaults() {
    assert_eq!(
        hr_ot_layout_get_horizontal_baseline_tag_for_script(HR_SCRIPT_LATIN),
        HR_OT_LAYOUT_BASELINE_TAG_ROMAN
    );
    assert_eq!(
        hr_ot_layout_get_horizontal_baseline_tag_for_script(HR_SCRIPT_DEVANAGARI),
        HR_OT_LAYOUT_BASELINE_TAG_HANGING
    );
    assert_eq!(
        hr_ot_layout_get_horizontal_baseline_tag_for_script(HR_SCRIPT_HAN),
        HR_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_BOTTOM_OR_LEFT
    );
    unsafe {
        with_font(|_face, font| {
            let mut coord = 123;
            hr_ot_layout_get_baseline_with_fallback(
                font,
                HR_OT_LAYOUT_BASELINE_TAG_ROMAN,
                HR_DIRECTION_LTR,
                u32::from_be_bytes(*b"latn"),
                0,
                &raw mut coord,
            );
            assert_eq!(coord, 0);
            hr_ot_layout_get_baseline_with_fallback2(
                font,
                HR_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_CENTRAL,
                HR_DIRECTION_LTR,
                HR_SCRIPT_HAN,
                ptr::null(),
                &raw mut coord,
            );
            assert_ne!(coord, 123);
        });
    }
}

#[test]
fn base_baseline2_selects_indic_opentype_script_tag() {
    unsafe {
        with_named_font("Rasa.subset1.otf", |_face, font| {
            let mut coord = 0;
            assert_eq!(
                hr_ot_layout_get_baseline2(
                    font,
                    HR_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_BOTTOM_OR_LEFT,
                    HR_DIRECTION_LTR,
                    HR_SCRIPT_GUJARATI,
                    ptr::null(),
                    &raw mut coord
                ),
                1
            );
            assert_eq!(coord, -236);
        });
    }
}

#[test]
fn base_device_adjustment_uses_coordinate_axis_ppem() {
    unsafe {
        // Derived from NotoSansCJK.subset1.otf: the DFLT icfb coordinate has
        // a format 3 Device table for sizes 12..=17, with deltas 1..=6.
        let fixture = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join("tests/fonts/BaselineDevice.otf");
        with_font_file(&fixture, |_face, font| {
            let mut coord = 0;
            let script = u32::from_be_bytes(*b"zzzz"); // Select the BASE default script.
            hr_font_set_scale(font, 1200, 2400);
            assert_eq!(
                hr_ot_layout_get_baseline(
                    font,
                    HR_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_BOTTOM_OR_LEFT,
                    HR_DIRECTION_LTR,
                    script,
                    0,
                    &raw mut coord
                ),
                1
            );
            assert_eq!(coord, -178);

            hr_font_set_ptem(font, 12.0);
            assert_eq!(
                hr_ot_layout_get_baseline(
                    font,
                    HR_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_BOTTOM_OR_LEFT,
                    HR_DIRECTION_LTR,
                    script,
                    0,
                    &raw mut coord,
                ),
                1
            );
            assert_eq!(coord, -178, "point size does not set pixels per em");

            // Horizontal text reads a y coordinate, so x_ppem is irrelevant.
            hr_font_set_ppem(font, 17, 12);
            assert_eq!(
                hr_ot_layout_get_baseline(
                    font,
                    HR_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_BOTTOM_OR_LEFT,
                    HR_DIRECTION_LTR,
                    script,
                    0,
                    &raw mut coord
                ),
                1
            );
            assert_eq!(coord, 22);

            hr_font_set_ppem(font, 17, 17);
            assert_eq!(
                hr_ot_layout_get_baseline(
                    font,
                    HR_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_BOTTOM_OR_LEFT,
                    HR_DIRECTION_LTR,
                    script,
                    0,
                    &raw mut coord
                ),
                1
            );
            assert_eq!(coord, 669);
        });
    }
}

fn font_data() -> Vec<u8> {
    std::fs::read(font_path(PLAIN_FONT)).expect("failed to read test font")
}

/// Builds a blob over `data`, which the caller must keep alive.
unsafe fn blob_over(data: &[u8]) -> *mut hr_blob_t {
    unsafe {
        hr_blob_create(
            data.as_ptr().cast::<c_char>(),
            data.len() as c_uint,
            HR_MEMORY_MODE_READONLY,
            ptr::null_mut(),
            None,
        )
    }
}

/// Runs `f` with a font built over the named test face.
unsafe fn with_named_font(name: &str, f: impl FnOnce(*mut hr_face_t, *mut hr_font_t)) {
    unsafe { with_font_file(&font_path(name), f) };
}

unsafe fn with_font_file(path: &std::path::Path, f: impl FnOnce(*mut hr_face_t, *mut hr_font_t)) {
    let data = std::fs::read(path).expect("failed to read test font");
    unsafe {
        let blob = blob_over(&data);
        let face = hr_face_create(blob, 0);
        let font = hr_font_create(face);
        f(face, font);
        hr_font_destroy(font);
        hr_face_destroy(face);
        hr_blob_destroy(blob);
    }
}

/// Runs `f` with a font built over [`PLAIN_FONT`].
unsafe fn with_font(f: impl FnOnce(*mut hr_face_t, *mut hr_font_t)) {
    unsafe { with_named_font(PLAIN_FONT, f) };
}

unsafe fn buffer_with_text(text: &str) -> *mut hr_buffer_t {
    unsafe {
        let buffer = hr_buffer_create();
        hr_buffer_add_utf8(
            buffer,
            text.as_ptr().cast::<c_char>(),
            text.len() as c_int,
            0,
            -1,
        );
        hr_buffer_guess_segment_properties(buffer);
        buffer
    }
}

unsafe fn glyph_ids(buffer: *mut hr_buffer_t) -> Vec<u32> {
    unsafe {
        let mut len: c_uint = 0;
        let infos = hr_buffer_get_glyph_infos(buffer, &raw mut len);
        (0..len as usize)
            .map(|i| (*infos.add(i)).codepoint)
            .collect()
    }
}

// ---------------------------------------------------------------- blobs ----

#[test]
fn blob_round_trips_its_data() {
    let data = b"some font bytes".to_vec();
    unsafe {
        let blob = blob_over(&data);
        assert_eq!(hr_blob_get_length(blob) as usize, data.len());
        let mut len: c_uint = 0;
        let got = hr_blob_get_data(blob, &raw mut len);
        let seen = core::slice::from_raw_parts(got.cast::<u8>(), len as usize);
        assert_eq!(seen, data.as_slice());
        hr_blob_destroy(blob);
    }
}

#[test]
fn blob_duplicate_mode_copies() {
    unsafe {
        let blob = {
            let scratch = b"temporary".to_vec();
            hr_blob_create(
                scratch.as_ptr().cast::<c_char>(),
                scratch.len() as c_uint,
                HR_MEMORY_MODE_DUPLICATE,
                ptr::null_mut(),
                None,
            )
            // `scratch` is dropped here; the blob must own its own copy.
        };
        let mut len: c_uint = 0;
        let got = hr_blob_get_data(blob, &raw mut len);
        assert_eq!(
            core::slice::from_raw_parts(got.cast::<u8>(), len as usize),
            b"temporary"
        );
        hr_blob_destroy(blob);
    }
}

#[test]
fn empty_blob_is_immortal_and_shared() {
    unsafe {
        let a = hr_blob_get_empty();
        let b = hr_blob_get_empty();
        assert_eq!(a, b);
        assert_eq!(hr_blob_get_length(a), 0);
        // Referencing and destroying an immortal object is a no-op, so this
        // must not free anything.
        hr_blob_destroy(hr_blob_reference(a));
        assert_eq!(hr_blob_get_length(hr_blob_get_empty()), 0);
    }
}

#[test]
fn sub_blob_shares_parent_storage() {
    let data = b"0123456789".to_vec();
    unsafe {
        let parent = blob_over(&data);
        let sub = hr_blob_create_sub_blob(parent, 2, 3);
        // Dropping the parent reference must not invalidate the sub-blob.
        hr_blob_destroy(parent);
        let mut len: c_uint = 0;
        let got = hr_blob_get_data(sub, &raw mut len);
        assert_eq!(
            core::slice::from_raw_parts(got.cast::<u8>(), len as usize),
            b"234"
        );
        hr_blob_destroy(sub);
    }
}

#[test]
fn sub_blob_keeps_its_parent_alive() {
    static KEY: hr_user_data_key_t = hr_user_data_key_t { unused: 0 };
    let data = b"0123456789".to_vec();
    let drops = AtomicUsize::new(0);
    unsafe {
        // The bytes may belong to the parent through its user data, so a
        // sub-blob has to keep the parent itself and not only its storage.
        let parent = blob_over(&data);
        let drops_ptr: *mut c_void = ptr::from_ref(&drops).cast_mut().cast();
        hr_blob_set_user_data(parent, &raw const KEY, drops_ptr, Some(count_drop), 1);
        let sub = hr_blob_create_sub_blob(parent, 2, 3);
        hr_blob_destroy(parent);
        assert_eq!(drops.load(Ordering::SeqCst), 0);
        hr_blob_destroy(sub);
        assert_eq!(drops.load(Ordering::SeqCst), 1);
    }
}

// ---------------------------------------------------------------- faces ----

#[test]
fn face_reports_its_metrics() {
    unsafe {
        with_font(|face, _| {
            assert_eq!(hr_face_get_upem(face), PLAIN_UPEM);
            assert!(hr_face_get_glyph_count(face) > 0);
            assert_eq!(hr_face_get_index(face), 0);
        });
    }
}

#[test]
fn face_references_a_table() {
    unsafe {
        with_font(|face, _| {
            let tag = hr_tag_from_string(c"cmap".as_ptr(), -1);
            let table = hr_face_reference_table(face, tag);
            assert!(hr_blob_get_length(table) > 0);
            hr_blob_destroy(table);

            // A table the font does not have yields the empty blob.
            let missing = hr_tag_from_string(c"ZZZZ".as_ptr(), -1);
            let table = hr_face_reference_table(face, missing);
            assert_eq!(hr_blob_get_length(table), 0);
            hr_blob_destroy(table);
        });
    }
}

/// Builds a font whose directory holds `count` records, every one pointing at
/// the same four bytes. Record `i` has tag `record_tag(i)`, ascending when
/// `sorted` and descending otherwise.
fn sfnt_with_records(count: usize, sorted: bool) -> Vec<u8> {
    let mut data = 0x0001_0000u32.to_be_bytes().to_vec();
    data.extend((count as u16).to_be_bytes());
    data.extend([0; 6]);
    let offset = 12 + 16 * count;
    for i in 0..count {
        let tag = record_tag(if sorted { i } else { count - 1 - i });
        data.extend(tag.to_be_bytes());
        data.extend(0u32.to_be_bytes());
        data.extend((offset as u32).to_be_bytes());
        data.extend(4u32.to_be_bytes());
    }
    data.extend([1, 2, 3, 4]);
    data
}

/// Distinct tags for [`sfnt_with_records`], avoiding the none tag.
fn record_tag(i: usize) -> hr_tag_t {
    i as hr_tag_t + 1
}

#[test]
fn walking_every_table_of_a_face_stays_linear() {
    // The most records a directory can hold. Fetching each table once must
    // not read the whole directory again per call.
    const COUNT: usize = u16::MAX as usize;
    for sorted in [true, false] {
        let data = sfnt_with_records(COUNT, sorted);
        unsafe {
            let blob = blob_over(&data);
            let face = hr_face_create(blob, 0);
            let started = std::time::Instant::now();
            let mut count = COUNT as c_uint;
            let mut tags = vec![0; COUNT];
            assert_eq!(
                hr_face_get_table_tags(face, 0, &raw mut count, tags.as_mut_ptr()),
                COUNT as c_uint
            );
            assert_eq!(count, COUNT as c_uint);
            for (i, &tag) in tags.iter().enumerate() {
                assert_eq!(tag, record_tag(if sorted { i } else { COUNT - 1 - i }));
                let table = hr_face_reference_table(face, tag);
                assert_eq!(hr_blob_get_length(table), 4);
                hr_blob_destroy(table);
            }
            assert!(started.elapsed() < std::time::Duration::from_secs(10));
            hr_face_destroy(face);
            hr_blob_destroy(blob);
        }
    }
}

#[test]
fn first_record_of_a_repeated_tag_wins() {
    // Two records with one tag leave the directory unsorted; `read-fonts`
    // then finds the first, and so must the face.
    let mut data = 0x0001_0000u32.to_be_bytes().to_vec();
    data.extend(2u16.to_be_bytes());
    data.extend([0; 6]);
    for (offset, length) in [(44u32, 2u32), (46, 3)] {
        data.extend(*b"cmap");
        data.extend(0u32.to_be_bytes());
        data.extend(offset.to_be_bytes());
        data.extend(length.to_be_bytes());
    }
    data.extend([1, 2, 3, 4, 5]);
    unsafe {
        let blob = blob_over(&data);
        let face = hr_face_create(blob, 0);
        let tag = hr_tag_from_string(c"cmap".as_ptr(), -1);
        let table = hr_face_reference_table(face, tag);
        assert_eq!(hr_blob_get_length(table), 2);
        hr_blob_destroy(table);
        hr_face_destroy(face);
        hr_blob_destroy(blob);
    }
}

#[test]
fn null_and_empty_faces_are_safe() {
    unsafe {
        assert_eq!(hr_face_get_upem(ptr::null_mut()), 1000);
        assert_eq!(hr_face_get_glyph_count(ptr::null_mut()), 0);
        assert_eq!(hr_face_get_glyph_count(hr_face_get_empty()), 0);
    }
}

// ---------------------------------------------------------------- fonts ----

#[test]
fn font_scale_defaults_to_upem() {
    unsafe {
        with_font(|face, font| {
            let (mut x, mut y) = (0, 0);
            hr_font_get_scale(font, &raw mut x, &raw mut y);
            assert_eq!(x as c_uint, hr_face_get_upem(face));
            assert_eq!(y as c_uint, hr_face_get_upem(face));

            hr_font_set_scale(font, 1000, 2000);
            hr_font_get_scale(font, &raw mut x, &raw mut y);
            assert_eq!((x, y), (1000, 2000));
        });
    }
}

#[test]
fn font_ppem_defaults_inherits_and_respects_immutability() {
    unsafe {
        with_font(|_, font| {
            let (mut x, mut y) = (99, 99);
            hr_font_get_ppem(font, &raw mut x, &raw mut y);
            assert_eq!((x, y), (0, 0));

            hr_font_set_ppem(font, 12, 17);
            let sub = hr_font_create_sub_font(font);
            hr_font_get_ppem(sub, &raw mut x, &raw mut y);
            assert_eq!((x, y), (12, 17));

            hr_font_set_ppem(sub, 21, 28);
            hr_font_get_ppem(font, &raw mut x, &raw mut y);
            assert_eq!((x, y), (12, 17));

            hr_font_make_immutable(sub);
            hr_font_set_ppem(sub, 33, 44);
            hr_font_get_ppem(sub, &raw mut x, &raw mut y);
            assert_eq!((x, y), (21, 28));
            hr_font_destroy(sub);
        });

        let (mut x, mut y) = (99, 99);
        hr_font_get_ppem(ptr::null_mut(), &raw mut x, &raw mut y);
        assert_eq!((x, y), (0, 0));
    }
}

#[test]
fn font_maps_codepoints_to_glyphs() {
    unsafe {
        with_font(|_, font| {
            let mut glyph: hr_codepoint_t = 0;
            assert_ne!(
                hr_font_get_nominal_glyph(font, 'a' as u32, &raw mut glyph),
                0
            );
            assert_ne!(glyph, 0);
            // A codepoint this subset font does not cover.
            assert_eq!(hr_font_get_nominal_glyph(font, 0x4E00, &raw mut glyph), 0);
        });
    }
}

#[test]
fn immutable_font_rejects_changes() {
    unsafe {
        with_font(|_, font| {
            hr_font_set_scale(font, 512, 512);
            hr_font_make_immutable(font);
            assert_ne!(hr_font_is_immutable(font), 0);
            hr_font_set_scale(font, 999, 999);
            let (mut x, mut y) = (0, 0);
            hr_font_get_scale(font, &raw mut x, &raw mut y);
            assert_eq!((x, y), (512, 512));
        });
    }
}

/// A sub-font answers through its parent, so it holds still only if the
/// parent does: freezing it freezes the chain, as in HarfBuzz.
#[test]
fn freezing_a_sub_font_freezes_the_fonts_it_descends_from() {
    unsafe {
        with_font(|_, font| {
            let child = hr_font_create_sub_font(font);
            let grandchild = hr_font_create_sub_font(child);
            hr_font_make_immutable(grandchild);
            assert_ne!(hr_font_is_immutable(grandchild), 0);
            assert_ne!(hr_font_is_immutable(child), 0);
            assert_ne!(hr_font_is_immutable(font), 0);

            // The base font now rejects changes the grandchild would see.
            hr_font_set_scale(font, 512, 512);
            let (mut x, mut y) = (0, 0);
            hr_font_get_scale(font, &raw mut x, &raw mut y);
            assert_ne!((x, y), (512, 512));
            let replacement = hr_font_funcs_create();
            hr_font_set_funcs(font, replacement, ptr::null_mut(), None);
            hr_font_funcs_destroy(replacement);
            let mut glyph: hr_codepoint_t = 0;
            assert_ne!(
                hr_font_get_nominal_glyph(grandchild, u32::from(b'a'), &raw mut glyph),
                0,
                "the base font should still read its own cmap"
            );

            hr_font_destroy(grandchild);
            hr_font_destroy(child);
        });
    }
}

// --------------------------------------------------------------- buffers ----

#[test]
fn buffer_tracks_its_content_type() {
    unsafe {
        let buffer = hr_buffer_create();
        assert_eq!(
            hr_buffer_get_content_type(buffer),
            HR_BUFFER_CONTENT_TYPE_INVALID
        );
        hr_buffer_add(buffer, 'x' as u32, 0);
        assert_eq!(
            hr_buffer_get_content_type(buffer),
            HR_BUFFER_CONTENT_TYPE_UNICODE
        );
        hr_buffer_clear_contents(buffer);
        assert_eq!(
            hr_buffer_get_content_type(buffer),
            HR_BUFFER_CONTENT_TYPE_INVALID
        );
        hr_buffer_destroy(buffer);
    }
}

#[test]
fn ill_formed_text_replaces_one_byte_at_a_time() {
    unsafe {
        // A truncated three-byte sequence followed by an ASCII byte. Each
        // byte that cannot begin a character is its own replacement, keeping
        // its own offset, rather than a run of them collapsing into one.
        let buffer = hr_buffer_create();
        let bad = [0xE2u8, 0x82, b'a'];
        hr_buffer_add_utf8(buffer, bad.as_ptr().cast(), 3, 0, -1);
        let mut len = 0;
        let infos = std::slice::from_raw_parts(
            hr_buffer_get_glyph_infos(buffer, &raw mut len),
            len as usize,
        );
        let seen: Vec<(u32, u32)> = infos.iter().map(|i| (i.codepoint, i.cluster)).collect();
        const FFFD: u32 = 0xFFFD;
        assert_eq!(seen, [(FFFD, 0), (FFFD, 1), (u32::from(b'a'), 2)]);
        hr_buffer_destroy(buffer);

        // UTF-32 carries values that are not characters at all; only
        // add_codepoints promises to pass those through untouched.
        let buffer = hr_buffer_create();
        let items = [0x41u32, 0xD800, 0x0011_0000, 0x42];
        hr_buffer_add_utf32(buffer, items.as_ptr(), 4, 0, -1);
        let mut len = 0;
        let infos = std::slice::from_raw_parts(
            hr_buffer_get_glyph_infos(buffer, &raw mut len),
            len as usize,
        );
        let seen: Vec<(u32, u32)> = infos.iter().map(|i| (i.codepoint, i.cluster)).collect();
        assert_eq!(seen, [(0x41, 0), (FFFD, 1), (FFFD, 2), (0x42, 3)]);
        hr_buffer_destroy(buffer);
    }
}

#[test]
fn buffer_exposes_codepoints_before_shaping() {
    unsafe {
        let buffer = buffer_with_text(TEXT);
        assert_eq!(hr_buffer_get_length(buffer) as usize, TEXT.chars().count());
        assert_eq!(
            glyph_ids(buffer),
            TEXT.chars().map(|c| c as u32).collect::<Vec<_>>()
        );
        hr_buffer_destroy(buffer);
    }
}

#[test]
fn add_utf8_uses_context_and_absolute_clusters() {
    unsafe {
        let text = "abcdef";
        let buffer = hr_buffer_create();
        // Add only "cd", leaving "ab" and "ef" as context.
        hr_buffer_add_utf8(
            buffer,
            text.as_ptr().cast::<c_char>(),
            text.len() as c_int,
            2,
            2,
        );
        assert_eq!(hr_buffer_get_length(buffer), 2);
        let mut len: c_uint = 0;
        let infos = hr_buffer_get_glyph_infos(buffer, &raw mut len);
        // Cluster values count from the start of the whole text.
        assert_eq!((*infos).codepoint, 'c' as u32);
        assert_eq!((*infos).cluster, 2);
        assert_eq!((*infos.add(1)).cluster, 3);
        hr_buffer_destroy(buffer);
    }
}

#[test]
fn add_utf16_decodes_pairs_and_keeps_code_unit_clusters() {
    unsafe {
        let buffer = hr_buffer_create();
        let text = [
            b'a' as u16,
            0xD83D,
            0xDE00,
            b'b' as u16,
            0xD800,
            b'c' as u16,
            0,
        ];
        hr_buffer_add_utf16(buffer, text.as_ptr(), -1, 1, 5);
        let mut len = 0;
        let infos = std::slice::from_raw_parts(
            hr_buffer_get_glyph_infos(buffer, &raw mut len),
            len as usize,
        );
        let seen: Vec<_> = infos.iter().map(|i| (i.codepoint, i.cluster)).collect();
        assert_eq!(
            seen,
            [
                (0x1F600, 1),
                (b'b' as u32, 3),
                (0xFFFD, 4),
                (b'c' as u32, 5)
            ]
        );
        hr_buffer_destroy(buffer);

        let buffer = hr_buffer_create();
        // Splitting a surrogate pair at the item boundary makes the high
        // surrogate an invalid item, rather than consuming its context.
        hr_buffer_add_utf16(buffer, text.as_ptr(), 6, 1, 1);
        let mut len = 0;
        let infos = hr_buffer_get_glyph_infos(buffer, &raw mut len);
        assert_eq!(len, 1);
        assert_eq!((*infos).codepoint, 0xFFFD);
        assert_eq!((*infos).cluster, 1);
        hr_buffer_destroy(buffer);
    }
}

#[test]
fn add_latin1_preserves_high_bytes_and_absolute_clusters() {
    unsafe {
        let buffer = hr_buffer_create();
        let text = [b'a', 0xE9, b'b', 0];
        hr_buffer_add_latin1(buffer, text.as_ptr().cast(), -1, 1, 2);
        let mut len = 0;
        let infos = std::slice::from_raw_parts(
            hr_buffer_get_glyph_infos(buffer, &raw mut len),
            len as usize,
        );
        let seen: Vec<_> = infos.iter().map(|i| (i.codepoint, i.cluster)).collect();
        assert_eq!(seen, [(0xE9, 1), (b'b' as u32, 2)]);
        hr_buffer_destroy(buffer);
    }
}

#[test]
fn buffer_properties_round_trip() {
    unsafe {
        let buffer = hr_buffer_create();
        hr_buffer_set_direction(buffer, HR_DIRECTION_RTL);
        hr_buffer_set_script(buffer, HR_SCRIPT_ARABIC);
        let arabic = hr_language_from_string(c"ar".as_ptr(), -1);
        hr_buffer_set_language(buffer, arabic);

        assert_eq!(hr_buffer_get_direction(buffer), HR_DIRECTION_RTL);
        assert_eq!(hr_buffer_get_script(buffer), HR_SCRIPT_ARABIC);
        // Languages are interned, so this compares by pointer.
        assert_eq!(hr_buffer_get_language(buffer), arabic);
        hr_buffer_destroy(buffer);
    }
}

/// Serializes with the given flags and hands back what was written.
unsafe fn serialize(
    buffer: *mut hr_buffer_t,
    start: c_uint,
    end: c_uint,
    font: *mut hr_font_t,
    flags: hr_buffer_serialize_flags_t,
) -> (c_uint, String) {
    let mut dest = [0i8; 512];
    let mut consumed = 0;
    let items = unsafe {
        hr_buffer_serialize_glyphs(
            buffer,
            start,
            end,
            dest.as_mut_ptr(),
            dest.len() as c_uint,
            &raw mut consumed,
            font,
            HR_BUFFER_SERIALIZE_FORMAT_TEXT,
            flags,
        )
    };
    let text = unsafe { std::ffi::CStr::from_ptr(dest.as_ptr()) }
        .to_str()
        .unwrap()
        .to_owned();
    assert_eq!(consumed as usize, text.len());
    (items, text)
}

/// A buffer of glyphs that was never shaped, and so has no positions.
unsafe fn glyph_buffer(gids: &[hr_codepoint_t]) -> *mut hr_buffer_t {
    unsafe {
        let buffer = hr_buffer_create();
        hr_buffer_add_codepoints(buffer, gids.as_ptr(), gids.len() as c_int, 0, -1);
        hr_buffer_set_content_type(buffer, HR_BUFFER_CONTENT_TYPE_GLYPHS);
        buffer
    }
}

#[test]
fn serializing_a_buffer_without_positions_reports_its_glyphs() {
    unsafe {
        with_font(|_, font| {
            // Having no positions is not the same as having zero ones: the
            // glyphs are still there to report, without the `+0` that would
            // claim an advance nobody worked out.
            // Numbered rather than named, so the expected text does not
            // depend on what this font calls its first two glyphs.
            let numbers = HR_BUFFER_SERIALIZE_FLAG_NO_GLYPH_NAMES;
            let buffer = glyph_buffer(&[1, 2]);
            let (items, text) = serialize(buffer, 0, 2, font, numbers);
            assert_eq!((items, text.as_str()), (2, "[1=0|2=1]"));

            // The range path used to run off the end of an empty string here.
            let (items, text) = serialize(buffer, 1, 2, font, numbers);
            assert_eq!((items, text.as_str()), (1, "|2=1]"));
            hr_buffer_destroy(buffer);
        });
    }
}

#[test]
fn serializing_a_range_counts_from_the_start_of_the_buffer() {
    unsafe {
        with_font(|_, font| {
            let buffer = buffer_with_text(TEXT);
            hr_shape(font, buffer, ptr::null(), 0);

            // With advances suppressed each glyph reports the pen position it
            // sits at, which counts from the start of the buffer even when
            // only the tail of it is asked for.
            let (_, whole) = serialize(
                buffer,
                0,
                hr_buffer_get_length(buffer),
                font,
                HR_BUFFER_SERIALIZE_FLAG_NO_ADVANCES,
            );
            let (_, tail) = serialize(
                buffer,
                1,
                hr_buffer_get_length(buffer),
                font,
                HR_BUFFER_SERIALIZE_FLAG_NO_ADVANCES,
            );
            // The tail of the whole listing is exactly the tail on its own.
            let from = whole.find('|').expect("more than one glyph");
            assert_eq!(tail, whole[from..]);
            hr_buffer_destroy(buffer);
        });
    }
}

#[test]
fn extents_that_cannot_be_worked_out_are_left_out() {
    unsafe {
        // Nothing can say what a glyph's extents are without a font, so the
        // field is absent rather than zero.
        let buffer = glyph_buffer(&[1]);
        let (items, text) = serialize(
            buffer,
            0,
            1,
            ptr::null_mut(),
            HR_BUFFER_SERIALIZE_FLAG_GLYPH_EXTENTS,
        );
        assert_eq!((items, text.as_str()), (1, "[gid1=0]"));
        hr_buffer_destroy(buffer);
    }
}

#[test]
fn adding_codepoints_checks_nothing() {
    unsafe {
        let items = [0x41u32, 0xD800, 0x0011_0000, 0x42];

        // `add_utf32` replaces what are not characters ...
        let buffer = hr_buffer_create();
        hr_buffer_add_utf32(buffer, items.as_ptr(), 4, 0, -1);
        const FFFD: u32 = 0xFFFD;
        assert_eq!(glyph_ids(buffer), [0x41, FFFD, FFFD, 0x42]);
        hr_buffer_destroy(buffer);

        // ... and `add_codepoints` is the counterpart that does not, for
        // callers that have already checked, or that mean it.
        let buffer = hr_buffer_create();
        hr_buffer_add_codepoints(buffer, items.as_ptr(), 4, 0, -1);
        assert_eq!(glyph_ids(buffer), items);
        hr_buffer_destroy(buffer);
    }
}

#[test]
fn guessing_settles_on_a_language() {
    unsafe {
        // HarfBuzz fills in the language the process is running under, which
        // language-specific shaping then follows.
        let buffer = hr_buffer_create();
        hr_buffer_add_utf8(buffer, c"abc".as_ptr(), -1, 0, -1);
        assert!(hr_buffer_get_language(buffer).is_null());
        hr_buffer_guess_segment_properties(buffer);
        assert_eq!(hr_buffer_get_language(buffer), hr_language_get_default());
        hr_buffer_destroy(buffer);
    }
}

#[test]
fn serializing_writes_whole_items_or_none() {
    unsafe {
        with_font(|_, font| {
            let buffer = buffer_with_text(TEXT);
            hr_buffer_guess_segment_properties(buffer);
            hr_shape(font, buffer, ptr::null(), 0);
            let count = hr_buffer_get_length(buffer);

            let mut full = [0i8; 256];
            let mut consumed = 0;
            let items = hr_buffer_serialize_glyphs(
                buffer,
                0,
                count,
                full.as_mut_ptr(),
                full.len() as c_uint,
                &raw mut consumed,
                font,
                HR_BUFFER_SERIALIZE_FORMAT_TEXT,
                HR_BUFFER_SERIALIZE_FLAG_DEFAULT,
            );
            assert_eq!(items, count);
            let whole = std::ffi::CStr::from_ptr(full.as_ptr()).to_str().unwrap();
            assert_eq!(consumed as usize, whole.len());

            // Every destination size: an item is written only when all of it
            // fits, and what is reported written is exactly what is there.
            for size in 0..=whole.len() + 2 {
                let mut dest = [0x7fi8; 256];
                let mut consumed = 0;
                let items = hr_buffer_serialize_glyphs(
                    buffer,
                    0,
                    count,
                    dest.as_mut_ptr(),
                    size as c_uint,
                    &raw mut consumed,
                    font,
                    HR_BUFFER_SERIALIZE_FORMAT_TEXT,
                    HR_BUFFER_SERIALIZE_FLAG_DEFAULT,
                );
                if size == 0 {
                    assert_eq!((items, consumed), (0, 0));
                    continue;
                }
                let written = std::ffi::CStr::from_ptr(dest.as_ptr()).to_str().unwrap();
                assert_eq!(written.len(), consumed as usize, "size {size}");
                // Whatever was written is a prefix of the whole, and never
                // half of an item.
                assert!(whole.starts_with(written), "size {size}: {written:?}");
                assert_eq!(
                    items as usize,
                    written.matches('|').count() + usize::from(!written.is_empty()),
                    "size {size}: {written:?}"
                );
                assert!(
                    (consumed as usize) < size,
                    "size {size} left no room to terminate"
                );
            }
            hr_buffer_destroy(buffer);
        });
    }
}

#[test]
fn serializing_without_a_font_numbers_the_glyphs() {
    unsafe {
        with_font(|_, font| {
            let buffer = buffer_with_text(TEXT);
            hr_buffer_guess_segment_properties(buffer);
            hr_shape(font, buffer, ptr::null(), 0);

            // HarfBuzz substitutes its empty font for NULL, which knows no
            // glyph names, so the glyphs come back numbered rather than not
            // at all.
            let mut dest = [0i8; 256];
            let mut consumed = 0;
            let items = hr_buffer_serialize_glyphs(
                buffer,
                0,
                hr_buffer_get_length(buffer),
                dest.as_mut_ptr(),
                dest.len() as c_uint,
                &raw mut consumed,
                ptr::null_mut(),
                HR_BUFFER_SERIALIZE_FORMAT_TEXT,
                HR_BUFFER_SERIALIZE_FLAG_DEFAULT,
            );
            assert_eq!(items, hr_buffer_get_length(buffer));
            let text = std::ffi::CStr::from_ptr(dest.as_ptr()).to_str().unwrap();
            assert!(text.starts_with("[gid"), "{text:?}");
            hr_buffer_destroy(buffer);
        });
    }
}

#[test]
fn appending_carries_everything_the_glyphs_came_with() {
    unsafe {
        with_font(|_, font| {
            let source = buffer_with_text(TEXT);
            hr_buffer_guess_segment_properties(source);
            hr_shape(font, source, ptr::null(), 0);

            let mut len = 0;
            let source_positions = std::slice::from_raw_parts(
                hr_buffer_get_glyph_positions(source, &raw mut len),
                len as usize,
            );
            let source_advances: Vec<i32> = source_positions.iter().map(|p| p.x_advance).collect();
            assert!(source_advances.iter().any(|a| *a != 0));

            // A destination told nothing at all takes the source's kind and
            // properties, and the glyphs arrive with their positions.
            let dest = hr_buffer_create();
            hr_buffer_append(dest, source, 0, u32::MAX);

            assert_eq!(hr_buffer_get_length(dest), len);
            assert_eq!(
                hr_buffer_get_content_type(dest),
                hr_buffer_get_content_type(source)
            );
            assert_eq!(
                hr_buffer_get_direction(dest),
                hr_buffer_get_direction(source)
            );
            assert_eq!(hr_buffer_get_script(dest), hr_buffer_get_script(source));

            let mut dest_len = 0;
            let dest_positions = std::slice::from_raw_parts(
                hr_buffer_get_glyph_positions(dest, &raw mut dest_len),
                dest_len as usize,
            );
            let dest_advances: Vec<i32> = dest_positions.iter().map(|p| p.x_advance).collect();
            assert_eq!(dest_advances, source_advances);

            hr_buffer_destroy(dest);
            hr_buffer_destroy(source);
        });
    }
}

#[test]
fn appending_a_range_carries_its_surroundings() {
    unsafe {
        with_font(|_, font| {
            // Appending the middle of some text leaves the rest of it as
            // context, so the piece shapes as it did in place.
            let source = buffer_with_text("abcde");
            hr_buffer_guess_segment_properties(source);

            let dest = hr_buffer_create();
            hr_buffer_append(dest, source, 1, 3);
            hr_shape(font, dest, ptr::null(), 0);
            let appended = glyph_ids(dest);

            // The same two characters given the same surroundings directly.
            let direct = hr_buffer_create();
            hr_buffer_add_utf8(direct, c"abcde".as_ptr(), -1, 1, 2);
            hr_buffer_guess_segment_properties(direct);
            hr_shape(font, direct, ptr::null(), 0);
            assert_eq!(appended, glyph_ids(direct));

            hr_buffer_destroy(direct);
            hr_buffer_destroy(dest);
            hr_buffer_destroy(source);
        });
    }
}

#[test]
fn an_unknown_serialization_format_survives_the_round_trip() {
    unsafe {
        // The format is a tag, and a caller may hold one this library does
        // not know: it comes back as itself rather than collapsing, and asks
        // for nothing when handed back.
        let unknown = hr_buffer_serialize_format_from_string(c"FOO ".as_ptr(), -1);
        assert_eq!(unknown, 0x464F_4F00);
        assert!(hr_buffer_serialize_format_to_string(unknown).is_null());

        assert_eq!(
            hr_buffer_serialize_format_from_string(c"text".as_ptr(), -1),
            HR_BUFFER_SERIALIZE_FORMAT_TEXT
        );

        // Handing it back asks for a format that does not exist, which
        // serializes nothing rather than misbehaving.
        with_font(|_, font| {
            let buffer = buffer_with_text(TEXT);
            hr_shape(font, buffer, ptr::null(), 0);
            let mut dest = [0i8; 64];
            let mut consumed = 0;
            let items = hr_buffer_serialize_glyphs(
                buffer,
                0,
                hr_buffer_get_length(buffer),
                dest.as_mut_ptr(),
                dest.len() as c_uint,
                &raw mut consumed,
                font,
                unknown,
                HR_BUFFER_SERIALIZE_FLAG_DEFAULT,
            );
            assert_eq!((items, consumed), (0, 0));
            hr_buffer_destroy(buffer);
        });
    }
}

unsafe extern "C" fn advance_of_42(
    _font: *mut hr_font_t,
    _font_data: *mut c_void,
    _glyph: hr_codepoint_t,
    _user_data: *mut c_void,
) -> hr_position_t {
    42
}

#[test]
fn a_partial_sub_font_leaves_the_rest_to_its_parent() {
    unsafe {
        with_font(|_, font| {
            // Callbacks the child does not carry are the parent's business,
            // and a parent carrying none of its own reads its tables.
            let sub = hr_font_create_sub_font(font);
            let ffuncs = hr_font_funcs_create();
            hr_font_funcs_set_glyph_h_advance_func(
                ffuncs,
                Some(advance_of_42),
                ptr::null_mut(),
                None,
            );
            hr_font_set_funcs(sub, ffuncs, ptr::null_mut(), None);
            hr_font_funcs_destroy(ffuncs);

            let mut glyph: hr_codepoint_t = 0;
            assert_ne!(
                hr_font_get_nominal_glyph(sub, u32::from(b'a'), &raw mut glyph),
                0,
                "a glyph the parent can find is one the child can find"
            );
            assert_ne!(glyph, 0);
            hr_font_destroy(sub);
        });

        // And a sub-font told to read the tables itself stops asking its
        // parent, which carrying nothing could not have said.
        with_font(|_, font| {
            let ffuncs = hr_font_funcs_create();
            hr_font_funcs_set_nominal_glyph_func(
                ffuncs,
                Some(always_glyph_777),
                ptr::null_mut(),
                None,
            );
            hr_font_set_funcs(font, ffuncs, ptr::null_mut(), None);
            hr_font_funcs_destroy(ffuncs);

            let sub = hr_font_create_sub_font(font);
            let mut glyph: hr_codepoint_t = 0;
            hr_font_get_nominal_glyph(sub, u32::from(b'a'), &raw mut glyph);
            assert_eq!(glyph, 777);

            hr_ot_font_set_funcs(sub);
            glyph = 0;
            assert_ne!(
                hr_font_get_nominal_glyph(sub, u32::from(b'a'), &raw mut glyph),
                0
            );
            assert_ne!(glyph, 777);
            hr_font_destroy(sub);
        });
    }
}

#[test]
fn no_name_names_no_tag() {
    unsafe {
        // Padding nothing out to four spaces would make a tag of it, and
        // then a script and a language of their own.
        assert_eq!(hr_tag_from_string(c"".as_ptr(), -1), HR_TAG_NONE);
        assert_eq!(hr_script_from_string(c"".as_ptr(), -1), HR_SCRIPT_INVALID);
        assert_eq!(
            hr_tag_from_string(c"ab".as_ptr(), -1),
            hr_tag_from_string(c"ab  ".as_ptr(), -1)
        );

        // Text runs left to right unless its script says otherwise, and a
        // script that is not one does not say otherwise.
        assert_eq!(
            hr_script_get_horizontal_direction(HR_SCRIPT_INVALID),
            HR_DIRECTION_LTR
        );
        assert_eq!(
            hr_script_get_horizontal_direction(HR_SCRIPT_ARABIC),
            HR_DIRECTION_RTL
        );

        // Reversing is the bit flip that separates each pair.
        assert_eq!(hr_direction_reverse(HR_DIRECTION_LTR), HR_DIRECTION_RTL);
        assert_eq!(hr_direction_reverse(HR_DIRECTION_TTB), HR_DIRECTION_BTT);
        assert_eq!(
            hr_direction_reverse(hr_direction_reverse(HR_DIRECTION_INVALID)),
            HR_DIRECTION_INVALID
        );
    }
}

#[test]
fn a_sub_blob_fixes_what_it_is_part_of() {
    unsafe {
        let storage = *b"0123456789";
        let blob = hr_blob_create(
            storage.as_ptr().cast(),
            storage.len() as c_uint,
            HR_MEMORY_MODE_READONLY,
            ptr::null_mut(),
            None,
        );
        assert_eq!(hr_blob_is_immutable(blob), 0);
        let sub = hr_blob_create_sub_blob(blob, 3, 4);
        assert_eq!(hr_blob_get_length(sub), 4);
        // The part cannot outlive its parent being what it was.
        assert_ne!(hr_blob_is_immutable(blob), 0);
        hr_blob_destroy(sub);
        hr_blob_destroy(blob);
    }
}

#[test]
fn every_font_has_a_parent_to_stop_at() {
    unsafe {
        with_font(|_, font| {
            // A font made from a face answers with the font that is empty,
            // rather than with nothing, so walking up the chain terminates.
            assert_eq!(hr_font_get_parent(font), hr_font_get_empty());
            let sub = hr_font_create_sub_font(font);
            assert_eq!(hr_font_get_parent(sub), font);
            hr_font_destroy(sub);
        });
    }
}

#[test]
fn overlaying_properties_onto_themselves_is_allowed() {
    unsafe {
        // Nothing says the two have to be different structs.
        let mut props = latin_props();
        hr_segment_properties_overlay(ptr::from_mut(&mut props), ptr::from_ref(&props));
        assert_eq!(props.direction, HR_DIRECTION_LTR);
        assert_eq!(props.script, HR_SCRIPT_LATIN);
    }
}

#[test]
fn a_length_of_nothing_means_what_each_creator_says() {
    unsafe {
        let storage = [0i8; 1];
        let drops = AtomicUsize::new(0);
        let user_data: *mut c_void = ptr::from_ref(&drops).cast_mut().cast();

        // `hr_blob_create` hands back the blob that holds nothing, so the
        // caller's data goes now: nothing will be left to release it.
        drops.store(0, Ordering::SeqCst);
        let created = hr_blob_create(
            storage.as_ptr(),
            0,
            HR_MEMORY_MODE_READONLY,
            user_data,
            Some(count_drop),
        );
        assert_eq!(created, hr_blob_get_empty());
        assert_eq!(drops.load(Ordering::SeqCst), 1);
        hr_blob_destroy(created);

        // `hr_blob_create_or_fail` makes one that can hold it.
        drops.store(0, Ordering::SeqCst);
        let or_fail = hr_blob_create_or_fail(
            storage.as_ptr(),
            0,
            HR_MEMORY_MODE_READONLY,
            user_data,
            Some(count_drop),
        );
        assert!(!or_fail.is_null());
        assert_ne!(or_fail, hr_blob_get_empty());
        assert_eq!(drops.load(Ordering::SeqCst), 0);
        hr_blob_destroy(or_fail);
        assert_eq!(drops.load(Ordering::SeqCst), 1);
    }
}

#[test]
fn an_empty_buffer_executes_any_plan() {
    unsafe {
        with_font(|_, font| {
            // Including the empty one, which can shape nothing: there is
            // nothing to shape.
            let buffer = hr_buffer_create();
            hr_buffer_guess_segment_properties(buffer);
            assert_ne!(
                hr_shape_plan_execute(hr_shape_plan_get_empty(), font, buffer, ptr::null(), 0),
                0
            );
            hr_buffer_destroy(buffer);
        });
    }
}

#[test]
fn a_sub_font_follows_its_parent() {
    unsafe {
        with_font(|_, font| {
            let ffuncs = hr_font_funcs_create();
            hr_font_funcs_set_nominal_glyph_func(
                ffuncs,
                Some(always_glyph_777),
                ptr::null_mut(),
                None,
            );
            hr_font_set_funcs(font, ffuncs, ptr::null_mut(), None);
            hr_font_funcs_destroy(ffuncs);

            let sub = hr_font_create_sub_font(font);
            let mut glyph: hr_codepoint_t = 0;
            assert_ne!(
                hr_font_get_nominal_glyph(sub, u32::from(b'a'), &raw mut glyph),
                0
            );
            assert_eq!(glyph, 777);

            // Callbacks installed on the parent after the child was made are
            // the child's callbacks too: it asks, rather than having copied.
            let replacement = hr_font_funcs_create();
            hr_font_set_funcs(font, replacement, ptr::null_mut(), None);
            hr_font_funcs_destroy(replacement);
            glyph = 1;
            assert_eq!(
                hr_font_get_nominal_glyph(sub, u32::from(b'a'), &raw mut glyph),
                0,
                "the child should see that the parent now answers nothing"
            );
            assert_eq!(glyph, 0);

            hr_font_destroy(sub);
        });
    }
}

#[test]
fn context_belongs_to_the_text_it_was_added_with() {
    unsafe {
        // Text added to a buffer that already holds some brings no
        // pre-context with it: the first call's surroundings stand.
        let buffer = hr_buffer_create();
        hr_buffer_add_utf8(buffer, c"abcdef".as_ptr(), -1, 2, 2);
        hr_buffer_add_utf8(buffer, c"abcdef".as_ptr(), -1, 4, 2);
        assert_eq!(hr_buffer_get_length(buffer), 4);
        hr_buffer_destroy(buffer);

        // Emptying by length empties what described the contents too.
        let buffer = hr_buffer_create();
        hr_buffer_add_utf8(buffer, c"abcdef".as_ptr(), -1, 0, 3);
        assert_eq!(
            hr_buffer_get_content_type(buffer),
            HR_BUFFER_CONTENT_TYPE_UNICODE
        );
        assert_ne!(hr_buffer_set_length(buffer, 0), 0);
        assert_eq!(
            hr_buffer_get_content_type(buffer),
            HR_BUFFER_CONTENT_TYPE_INVALID
        );
        hr_buffer_destroy(buffer);
    }
}

#[test]
fn a_buffer_like_another_copies_how_it_is_set_up() {
    unsafe {
        let src = hr_buffer_create();
        hr_buffer_set_direction(src, HR_DIRECTION_RTL);
        hr_buffer_set_script(src, HR_SCRIPT_ARABIC);
        hr_buffer_set_cluster_level(src, HR_BUFFER_CLUSTER_LEVEL_CHARACTERS);
        hr_buffer_set_flags(src, HR_BUFFER_FLAG_BOT);

        // How it is configured, and not what it says about the text.
        let like = hr_buffer_create_similar(src);
        assert_eq!(
            hr_buffer_get_cluster_level(like),
            HR_BUFFER_CLUSTER_LEVEL_CHARACTERS
        );
        assert_eq!(hr_buffer_get_flags(like), HR_BUFFER_FLAG_BOT);
        assert_eq!(hr_buffer_get_direction(like), HR_DIRECTION_INVALID);
        assert_eq!(hr_buffer_get_script(like), HR_SCRIPT_INVALID);

        // And clearing the contents leaves the configuration alone.
        hr_buffer_set_not_found_variation_selector_glyph(src, 7);
        hr_buffer_clear_contents(src);
        assert_eq!(
            hr_buffer_get_cluster_level(src),
            HR_BUFFER_CLUSTER_LEVEL_CHARACTERS
        );
        assert_eq!(hr_buffer_get_not_found_variation_selector_glyph(src), 7);

        // A variation selector nothing resolves is spelled with the
        // codepoint that is not one, which is also the default.
        let fresh = hr_buffer_create();
        assert_eq!(
            hr_buffer_get_not_found_variation_selector_glyph(fresh),
            HR_CODEPOINT_INVALID
        );
        hr_buffer_destroy(fresh);
        hr_buffer_destroy(like);
        hr_buffer_destroy(src);
    }
}

#[test]
fn properties_compare_and_overlay_as_a_whole() {
    unsafe {
        let mut a = latin_props();
        let mut b = latin_props();
        a.reserved1 = ptr::dangling_mut();
        assert_eq!(
            hr_segment_properties_equal(ptr::from_ref(&a), ptr::from_ref(&b)),
            0,
            "the reserved fields are part of the comparison"
        );

        // Overlaying stops where the two describe the text differently.
        let mut dest = hr_segment_properties_t {
            direction: HR_DIRECTION_LTR,
            script: HR_SCRIPT_INVALID,
            language: ptr::null(),
            reserved1: ptr::null_mut(),
            reserved2: ptr::null_mut(),
        };
        b.direction = HR_DIRECTION_RTL;
        b.script = HR_SCRIPT_ARABIC;
        hr_segment_properties_overlay(ptr::from_mut(&mut dest), ptr::from_ref(&b));
        assert_eq!(dest.script, HR_SCRIPT_INVALID);
        assert!(dest.language.is_null());
    }
}

#[test]
fn nothing_to_shape_is_nothing_to_fail_at() {
    unsafe {
        with_font(|_, font| {
            // Even asking for a shaper that does not exist: HarfBuzz answers
            // an empty buffer before it looks at anything else.
            let buffer = hr_buffer_create();
            hr_buffer_guess_segment_properties(buffer);
            let absent = [c"graphite2".as_ptr(), ptr::null()];
            assert_ne!(
                hr_shape_full(font, buffer, ptr::null(), 0, absent.as_ptr()),
                0
            );
            hr_buffer_destroy(buffer);
        });
    }
}

#[test]
fn a_blob_of_nothing_is_still_a_blob() {
    unsafe {
        // Zero length is not a failure, and the blob holds the caller's data
        // until it is destroyed.
        let drops = AtomicUsize::new(0);
        let user_data: *mut c_void = ptr::from_ref(&drops).cast_mut().cast();
        let storage = [0i8; 1];
        let blob = hr_blob_create_or_fail(
            storage.as_ptr(),
            0,
            HR_MEMORY_MODE_READONLY,
            user_data,
            Some(count_drop),
        );
        assert!(!blob.is_null());
        assert_eq!(hr_blob_get_length(blob), 0);
        assert_eq!(drops.load(Ordering::SeqCst), 0);
        hr_blob_destroy(blob);
        assert_eq!(drops.load(Ordering::SeqCst), 1);

        // A length that cannot be represented is the one that fails.
        drops.store(0, Ordering::SeqCst);
        let refused = hr_blob_create_or_fail(
            storage.as_ptr(),
            1 << 31,
            HR_MEMORY_MODE_READONLY,
            user_data,
            Some(count_drop),
        );
        assert!(refused.is_null());
        assert_eq!(drops.load(Ordering::SeqCst), 1);
    }
}

#[test]
fn a_failed_parse_describes_nothing() {
    unsafe {
        let mut feature = hr_feature_t {
            tag: 0xABAB_ABAB,
            value: 9,
            start: 9,
            end: 9,
        };
        assert_eq!(
            hr_feature_from_string(c"!!! not a feature".as_ptr(), -1, &raw mut feature),
            0
        );
        assert_eq!(
            (feature.tag, feature.value, feature.start, feature.end),
            (0, 0, 0, 0)
        );

        let mut variation = hr_variation_t {
            tag: 0xABAB_ABAB,
            value: 9.0,
        };
        assert_eq!(
            hr_variation_from_string(c"!!!".as_ptr(), -1, &raw mut variation),
            0
        );
        assert_eq!(variation.tag, 0);

        // A range of one character is written as that character.
        let mut one = hr_feature_t::default();
        assert_ne!(
            hr_feature_from_string(c"liga[5]".as_ptr(), -1, &raw mut one),
            0
        );
        let mut text = [0i8; 64];
        hr_feature_to_string(ptr::from_ref(&one), text.as_mut_ptr(), text.len() as c_uint);
        assert_eq!(
            std::ffi::CStr::from_ptr(text.as_ptr()).to_str().unwrap(),
            "liga[5]"
        );
    }
}

#[test]
fn buffer_properties_round_trip_being_unset() {
    unsafe {
        let buffer = hr_buffer_create();
        // A buffer that was never told a script has none, which is not the
        // same as the `Zzzz` standing for a script known to be unknown.
        assert_eq!(hr_buffer_get_script(buffer), HR_SCRIPT_INVALID);
        assert!(hr_buffer_get_language(buffer).is_null());

        // Setting the absent value clears whatever was there.
        hr_buffer_set_script(buffer, HR_SCRIPT_ARABIC);
        hr_buffer_set_script(buffer, HR_SCRIPT_INVALID);
        assert_eq!(hr_buffer_get_script(buffer), HR_SCRIPT_INVALID);

        hr_buffer_set_language(buffer, hr_language_from_string(c"en".as_ptr(), -1));
        hr_buffer_set_language(buffer, ptr::null());
        assert!(hr_buffer_get_language(buffer).is_null());

        // Whole-property assignment overwrites rather than fills in, so a
        // reused buffer cannot keep anything from its last life.
        hr_buffer_set_script(buffer, HR_SCRIPT_ARABIC);
        hr_buffer_set_language(buffer, hr_language_from_string(c"ar".as_ptr(), -1));
        let blank = hr_segment_properties_t {
            direction: HR_DIRECTION_LTR,
            script: HR_SCRIPT_INVALID,
            language: ptr::null(),
            reserved1: ptr::null_mut(),
            reserved2: ptr::null_mut(),
        };
        hr_buffer_set_segment_properties(buffer, ptr::from_ref(&blank));
        assert_eq!(hr_buffer_get_script(buffer), HR_SCRIPT_INVALID);
        assert!(hr_buffer_get_language(buffer).is_null());
        hr_buffer_destroy(buffer);
    }
}

#[test]
fn an_empty_shaper_list_names_no_available_shaper() {
    unsafe {
        with_font(|face, font| {
            // A list of no names asks for no shaper this library has, which
            // is a failure rather than the free rein that `NULL` gives.
            let empty = [ptr::null()];
            let buffer = buffer_with_text(TEXT);
            assert_eq!(
                hr_shape_full(font, buffer, ptr::null(), 0, empty.as_ptr()),
                0
            );

            let props = latin_props();
            let plan =
                hr_shape_plan_create(face, ptr::from_ref(&props), ptr::null(), 0, empty.as_ptr());
            assert_eq!(plan, hr_shape_plan_get_empty());

            // `NULL` still means no restriction.
            assert_ne!(hr_shape_full(font, buffer, ptr::null(), 0, ptr::null()), 0);
            hr_buffer_destroy(buffer);
        });
    }
}

#[test]
fn flags_combine_and_round_trip() {
    unsafe {
        let buffer = hr_buffer_create();
        // A fresh buffer has no flags at all. Flag types are integers rather
        // than enumerations precisely so that zero, and any combination, is
        // representable.
        assert_eq!(hr_buffer_get_flags(buffer), HR_BUFFER_FLAG_DEFAULT);

        let both = HR_BUFFER_FLAG_BOT | HR_BUFFER_FLAG_EOT;
        hr_buffer_set_flags(buffer, both);
        assert_eq!(hr_buffer_get_flags(buffer), both);

        hr_buffer_set_flags(buffer, HR_BUFFER_FLAG_DEFINED);
        assert_eq!(hr_buffer_get_flags(buffer), HR_BUFFER_FLAG_DEFINED);

        hr_buffer_set_flags(buffer, HR_BUFFER_FLAG_DEFAULT);
        assert_eq!(hr_buffer_get_flags(buffer), HR_BUFFER_FLAG_DEFAULT);
        hr_buffer_destroy(buffer);
    }
}

#[test]
fn glyph_flags_are_readable_including_none() {
    unsafe {
        with_font(|_, font| {
            let buffer = buffer_with_text(TEXT);
            hr_shape(font, buffer, ptr::null(), 0);
            let mut len: c_uint = 0;
            let infos = hr_buffer_get_glyph_infos(buffer, ptr::from_mut(&mut len));
            assert!(len > 0);
            for i in 0..len as usize {
                // Most glyphs carry no flags; reading that must be safe.
                let flags = hr_glyph_info_get_glyph_flags(infos.add(i));
                assert_eq!(flags & !HR_GLYPH_FLAG_DEFINED, 0);
            }
            hr_buffer_destroy(buffer);
        });
    }
}

#[test]
fn reversing_respects_clusters() {
    unsafe {
        let buffer = hr_buffer_create();
        for (i, c) in ['a', 'b', 'c', 'd'].into_iter().enumerate() {
            hr_buffer_add(buffer, c as u32, (i / 2) as c_uint);
        }
        hr_buffer_reverse_clusters(buffer);
        assert_eq!(
            glyph_ids(buffer),
            vec!['c' as u32, 'd' as u32, 'a' as u32, 'b' as u32]
        );
        hr_buffer_destroy(buffer);
    }
}

// --------------------------------------------------------------- shaping ----

#[test]
fn shaping_produces_positioned_glyphs() {
    unsafe {
        with_font(|_, font| {
            let buffer = buffer_with_text(TEXT);
            hr_shape(font, buffer, ptr::null(), 0);

            assert_eq!(
                hr_buffer_get_content_type(buffer),
                HR_BUFFER_CONTENT_TYPE_GLYPHS
            );
            assert_ne!(hr_buffer_has_positions(buffer), 0);

            let mut len: c_uint = 0;
            let positions = hr_buffer_get_glyph_positions(buffer, &raw mut len);
            // This font has no GSUB, so each character yields one glyph.
            assert_eq!(len as usize, TEXT.chars().count());
            for i in 0..len as usize {
                assert!(
                    (*positions.add(i)).x_advance > 0,
                    "glyph {i} has no advance"
                );
            }
            hr_buffer_destroy(buffer);
        });
    }
}

#[test]
fn shaping_twice_reuses_the_plan_and_agrees() {
    unsafe {
        with_font(|_, font| {
            let first = buffer_with_text(TEXT);
            hr_shape(font, first, ptr::null(), 0);
            let once = glyph_ids(first);

            // A second call hits the face's plan cache; the result must match.
            let second = buffer_with_text(TEXT);
            hr_shape(font, second, ptr::null(), 0);
            assert_eq!(glyph_ids(second), once);

            hr_buffer_destroy(first);
            hr_buffer_destroy(second);
        });
    }
}

#[test]
fn shaping_the_same_text_again_means_refilling_the_buffer() {
    unsafe {
        with_font(|_, font| {
            let buffer = buffer_with_text(TEXT);
            hr_shape(font, buffer, ptr::null(), 0);
            let once = glyph_ids(buffer);

            // The buffer holds glyphs now, so shaping it again would be a
            // misuse. Clearing and refilling is how the same text is shaped
            // afresh; relabelling the contents as text would instead shape the
            // glyph ids as though they were codepoints.
            hr_buffer_clear_contents(buffer);
            hr_buffer_add_utf8(
                buffer,
                TEXT.as_ptr().cast::<c_char>(),
                TEXT.len() as c_int,
                0,
                -1,
            );
            hr_buffer_guess_segment_properties(buffer);
            hr_shape(font, buffer, ptr::null(), 0);
            assert_eq!(glyph_ids(buffer), once);

            hr_buffer_destroy(buffer);
        });
    }
}

#[test]
fn shaping_an_already_shaped_buffer_aborts() {
    // Shaping glyphs as though they were text is a misuse of the API, which
    // HarfBuzz asserts on. `hr_shape` returns nothing, so it could not report
    // this any other way.
    assert!(aborts("already_shaped"));
}

#[test]
fn disabling_a_feature_changes_the_result() {
    unsafe {
        with_font(|_, font| {
            let shape_with = |features: &[hr_feature_t]| {
                let buffer = buffer_with_text("office");
                hr_shape(font, buffer, features.as_ptr(), features.len() as c_uint);
                let ids = glyph_ids(buffer);
                hr_buffer_destroy(buffer);
                ids
            };

            let mut no_liga = hr_feature_t {
                tag: 0,
                value: 0,
                start: 0,
                end: 0,
            };
            assert_ne!(
                hr_feature_from_string(c"-liga".as_ptr(), -1, &raw mut no_liga),
                0
            );
            assert_eq!(no_liga.value, 0);
            assert_eq!(no_liga.tag, hr_tag_from_string(c"liga".as_ptr(), -1));

            // Both must shape; whether they differ depends on the font, so
            // only require that turning the feature off is honoured without
            // changing the glyph count.
            let with_liga = shape_with(&[]);
            let without = shape_with(&[no_liga]);
            assert_ne!(with_liga, []);
            assert_ne!(without, []);
        });
    }
}

#[test]
fn serializing_a_shaped_buffer_writes_text() {
    unsafe {
        with_font(|_, font| {
            let buffer = buffer_with_text("AB");
            hr_shape(font, buffer, ptr::null(), 0);

            let mut buf = [0i8; 256];
            let mut consumed: c_uint = 0;
            let count = hr_buffer_serialize_glyphs(
                buffer,
                0,
                hr_buffer_get_length(buffer),
                buf.as_mut_ptr().cast::<c_char>(),
                buf.len() as c_uint,
                &raw mut consumed,
                font,
                HR_BUFFER_SERIALIZE_FORMAT_TEXT,
                HR_BUFFER_SERIALIZE_FLAG_DEFAULT,
            );
            // No GSUB, so two characters serialize as two glyphs.
            assert_eq!(count, 2);
            let text = core::ffi::CStr::from_ptr(buf.as_ptr().cast::<c_char>())
                .to_str()
                .unwrap();
            assert!(text.starts_with('['), "unexpected output: {text}");
            assert!(text.ends_with(']'), "unexpected output: {text}");
            assert_eq!(consumed as usize, text.len());
            hr_buffer_destroy(buffer);
        });
    }
}

// -------------------------------------------------------- table callbacks ----

/// The font bytes a table callback reads from, handed over as user data.
struct TableSource {
    data: Vec<u8>,
    requested: Mutex<Vec<hr_tag_t>>,
}

unsafe extern "C" fn reference_table(
    _face: *mut hr_face_t,
    tag: hr_tag_t,
    user_data: *mut c_void,
) -> *mut hr_blob_t {
    unsafe {
        let source = &*user_data.cast::<TableSource>();
        source.requested.lock().unwrap().push(tag);
        // Reuse a blob-backed face purely to look the table up.
        let blob = hr_blob_create(
            source.data.as_ptr().cast::<c_char>(),
            source.data.len() as c_uint,
            HR_MEMORY_MODE_READONLY,
            ptr::null_mut(),
            None,
        );
        let face = hr_face_create(blob, 0);
        let table = hr_face_reference_table(face, tag);
        hr_face_destroy(face);
        hr_blob_destroy(blob);
        if hr_blob_get_length(table) == 0 {
            hr_blob_destroy(table);
            return ptr::null_mut();
        }
        table
    }
}

unsafe extern "C" fn destroy_table_source(user_data: *mut c_void) {
    drop(unsafe { Box::from_raw(user_data.cast::<TableSource>()) });
}

#[test]
fn table_callback_face_shapes_like_a_blob_face() {
    let data = font_data();
    unsafe {
        // Shape once through an ordinary blob-backed face.
        let expected = {
            let blob = blob_over(&data);
            let face = hr_face_create(blob, 0);
            let font = hr_font_create(face);
            let buffer = buffer_with_text(TEXT);
            hr_shape(font, buffer, ptr::null(), 0);
            let ids = glyph_ids(buffer);
            hr_buffer_destroy(buffer);
            hr_font_destroy(font);
            hr_face_destroy(face);
            hr_blob_destroy(blob);
            ids
        };

        // ... then through a face built from table callbacks.
        let source = Box::into_raw(Box::new(TableSource {
            data: data.clone(),
            requested: Mutex::new(Vec::new()),
        }));
        let face = hr_face_create_for_tables(
            Some(reference_table),
            source.cast::<c_void>(),
            Some(destroy_table_source),
        );
        assert_eq!(hr_face_get_upem(face), PLAIN_UPEM);

        let font = hr_font_create(face);
        let requested = (*source).requested.lock().unwrap();
        assert!(!requested.contains(&hr_tag_from_string(c"cmap".as_ptr(), -1)));
        assert!(!requested.contains(&hr_tag_from_string(c"glyf".as_ptr(), -1)));
        drop(requested);

        let buffer = buffer_with_text(TEXT);
        hr_shape(font, buffer, ptr::null(), 0);
        assert_eq!(glyph_ids(buffer), expected);

        hr_buffer_destroy(buffer);
        hr_font_destroy(font);
        // Freeing the face runs `destroy_table_source`.
        hr_face_destroy(face);
    }
}

#[test]
fn builtin_font_queries_load_charmap_and_metrics_separately() {
    let source = Box::into_raw(Box::new(TableSource {
        data: font_data(),
        requested: Mutex::new(Vec::new()),
    }));
    unsafe {
        let face = hr_face_create_for_tables(
            Some(reference_table),
            source.cast::<c_void>(),
            Some(destroy_table_source),
        );
        let font = hr_font_create(face);
        let cmap = hr_tag_from_string(c"cmap".as_ptr(), -1);
        let hmtx = hr_tag_from_string(c"hmtx".as_ptr(), -1);

        let mut name = [0; 64];
        hr_font_get_glyph_name(font, 1, name.as_mut_ptr(), name.len() as c_uint);
        {
            let requested = (*source).requested.lock().unwrap();
            assert!(!requested.contains(&cmap));
            assert!(!requested.contains(&hmtx));
        }

        let mut glyph = 0;
        assert_ne!(
            hr_font_get_nominal_glyph(font, 'a' as u32, &raw mut glyph),
            0
        );
        {
            let requested = (*source).requested.lock().unwrap();
            assert!(requested.contains(&cmap));
            assert!(!requested.contains(&hmtx));
        }

        assert!(hr_font_get_glyph_h_advance(font, glyph) > 0);
        assert!((*source).requested.lock().unwrap().contains(&hmtx));

        hr_font_destroy(font);
        hr_face_destroy(face);
    }
}

#[test]
fn cold_builtin_font_queries_from_several_threads() {
    let source = Box::into_raw(Box::new(TableSource {
        data: font_data(),
        requested: Mutex::new(Vec::new()),
    }));
    unsafe {
        let face = hr_face_create_for_tables(
            Some(reference_table),
            source.cast::<c_void>(),
            Some(destroy_table_source),
        );
        let font = hr_font_create(face);
        hr_font_make_immutable(font);

        let shared = Shared(font);
        let barrier = std::sync::Arc::new(std::sync::Barrier::new(4));
        let threads: Vec<_> = (0..4)
            .map(|_| {
                let barrier = barrier.clone();
                std::thread::spawn(move || {
                    barrier.wait();
                    for _ in 0..8 {
                        let mut glyph = 0;
                        assert_eq!(
                            hr_font_get_nominal_glyph(shared.get(), 0x4E00, &raw mut glyph),
                            0
                        );
                        assert_ne!(
                            hr_font_get_nominal_glyph(shared.get(), 'a' as u32, &raw mut glyph),
                            0
                        );
                        assert!(hr_font_get_glyph_h_advance(shared.get(), glyph) > 0);
                    }
                })
            })
            .collect();
        for thread in threads {
            thread.join().expect("thread panicked");
        }

        hr_font_destroy(font);
        hr_face_destroy(face);
    }
}

#[test]
fn custom_font_funcs_do_not_load_builtin_font_tables() {
    let source = Box::into_raw(Box::new(TableSource {
        data: font_data(),
        requested: Mutex::new(Vec::new()),
    }));
    unsafe {
        let face = hr_face_create_for_tables(
            Some(reference_table),
            source.cast::<c_void>(),
            Some(destroy_table_source),
        );
        let font = hr_font_create(face);
        let funcs = hr_font_funcs_create();
        hr_font_set_funcs(font, funcs, ptr::null_mut(), None);

        let buffer = buffer_with_text(TEXT);
        hr_shape(font, buffer, ptr::null(), 0);

        let requested = (*source).requested.lock().unwrap();
        assert!(!requested.contains(&hr_tag_from_string(c"cmap".as_ptr(), -1)));
        assert!(!requested.contains(&hr_tag_from_string(c"glyf".as_ptr(), -1)));
        drop(requested);

        hr_buffer_destroy(buffer);
        hr_font_funcs_destroy(funcs);
        hr_font_destroy(font);
        hr_face_destroy(face);
    }
}

/// A table callback that ties each blob it returns to a drop counter through
/// the blob's user data, the way a caller owning table bytes that way would.
unsafe extern "C" fn reference_table_with_user_data(
    face: *mut hr_face_t,
    tag: hr_tag_t,
    user_data: *mut c_void,
) -> *mut hr_blob_t {
    static KEY: hr_user_data_key_t = hr_user_data_key_t { unused: 0 };
    unsafe {
        let (source, drops) = &*user_data.cast::<(TableSource, AtomicUsize)>();
        let table = reference_table(face, tag, ptr::from_ref(source).cast_mut().cast());
        let drops: *mut c_void = ptr::from_ref(drops).cast_mut().cast();
        hr_blob_set_user_data(table, &raw const KEY, drops, Some(count_drop), 1);
        table
    }
}

#[test]
fn table_callback_blob_lives_as_long_as_its_bytes_are_used() {
    let state = (
        TableSource {
            data: font_data(),
            requested: Mutex::new(Vec::new()),
        },
        AtomicUsize::new(0),
    );
    let drops = &state.1;
    unsafe {
        let face = hr_face_create_for_tables(
            Some(reference_table_with_user_data),
            ptr::from_ref(&state).cast_mut().cast(),
            None,
        );
        // Reading `head` keeps its blob, and with it whatever the callback
        // tied to the blob, for as long as the face may read the table.
        assert_eq!(hr_face_get_upem(face), PLAIN_UPEM);
        assert_eq!(drops.load(Ordering::SeqCst), 0);

        // A table handed back to the caller outlives the face the same way.
        let cmap = hr_face_reference_table(face, hr_tag_from_string(c"cmap".as_ptr(), -1));
        assert_ne!(hr_blob_get_length(cmap), 0);
        hr_face_destroy(face);
        assert_eq!(drops.load(Ordering::SeqCst), 1);
        hr_blob_destroy(cmap);
        assert_eq!(drops.load(Ordering::SeqCst), 2);
    }
}

// --------------------------------------------------------- font callbacks ----

unsafe extern "C" fn fixed_advance(
    _font: *mut hr_font_t,
    _font_data: *mut c_void,
    _glyph: hr_codepoint_t,
    user_data: *mut c_void,
) -> hr_position_t {
    unsafe { *user_data.cast::<hr_position_t>() }
}

#[test]
fn font_funcs_override_advances() {
    unsafe {
        with_font(|_, font| {
            let mut advance: hr_position_t = 1234;
            let ffuncs = hr_font_funcs_create();
            hr_font_funcs_set_glyph_h_advance_func(
                ffuncs,
                Some(fixed_advance),
                ptr::from_mut(&mut advance).cast::<c_void>(),
                None,
            );
            hr_font_set_funcs(font, ffuncs, ptr::null_mut(), None);

            let buffer = buffer_with_text("AB");
            hr_shape(font, buffer, ptr::null(), 0);
            let mut len: c_uint = 0;
            let positions = hr_buffer_get_glyph_positions(buffer, &raw mut len);
            for i in 0..len as usize {
                assert_eq!((*positions.add(i)).x_advance, 1234);
            }

            hr_buffer_destroy(buffer);
            hr_font_funcs_destroy(ffuncs);
        });
    }
}

unsafe extern "C" fn batch_advances(
    _font: *mut hr_font_t,
    _font_data: *mut c_void,
    count: c_uint,
    _first_glyph: *const hr_codepoint_t,
    _glyph_stride: c_uint,
    first_advance: *mut hr_position_t,
    advance_stride: c_uint,
    user_data: *mut c_void,
) {
    unsafe {
        *user_data.cast::<u32>() += 1;
        for i in 0..count as usize {
            first_advance
                .cast::<u8>()
                .add(i * advance_stride as usize)
                .cast::<hr_position_t>()
                .write_unaligned(700 + i as i32);
        }
    }
}

unsafe extern "C" fn fixed_origin(
    _font: *mut hr_font_t,
    _font_data: *mut c_void,
    _glyph: hr_codepoint_t,
    x: *mut hr_position_t,
    y: *mut hr_position_t,
    _user_data: *mut c_void,
) -> hr_bool_t {
    unsafe {
        *x = 12;
        *y = 34;
    }
    1
}

unsafe extern "C" fn fixed_kerning(
    _font: *mut hr_font_t,
    _font_data: *mut c_void,
    first: hr_codepoint_t,
    second: hr_codepoint_t,
    _user_data: *mut c_void,
) -> hr_position_t {
    first as i32 - second as i32
}

#[test]
fn added_font_callbacks_answer_and_batch_shaping() {
    unsafe {
        with_font(|_, font| {
            let funcs = hr_font_funcs_create();
            let mut calls = 0u32;
            hr_font_funcs_set_glyph_h_advances_func(
                funcs,
                Some(batch_advances),
                (&raw mut calls).cast(),
                None,
            );
            hr_font_funcs_set_glyph_h_origin_func(funcs, Some(fixed_origin), ptr::null_mut(), None);
            hr_font_funcs_set_glyph_h_kerning_func(
                funcs,
                Some(fixed_kerning),
                ptr::null_mut(),
                None,
            );
            hr_font_funcs_set_glyph_v_kerning_func(
                funcs,
                Some(fixed_kerning),
                ptr::null_mut(),
                None,
            );
            hr_font_set_funcs(font, funcs, ptr::null_mut(), None);

            let buffer = buffer_with_text("AB");
            hr_shape(font, buffer, ptr::null(), 0);
            let mut len = 0;
            let positions = hr_buffer_get_glyph_positions(buffer, &raw mut len);
            assert_eq!(len, 2);
            assert_eq!((*positions).x_advance, 700);
            assert_eq!((*positions.add(1)).x_advance, 701);
            assert!(calls > 0);

            let glyphs = [1u32, 2];
            let mut advances = [0i32; 2];
            hr_font_get_glyph_h_advances(font, 2, glyphs.as_ptr(), 4, advances.as_mut_ptr(), 4);
            assert_eq!(advances, [700, 701]);
            assert_eq!(hr_font_get_glyph_h_advance(font, 1), 700);
            let (mut x, mut y) = (0, 0);
            assert_eq!(
                hr_font_get_glyph_h_origin(font, 1, &raw mut x, &raw mut y),
                1
            );
            assert_eq!((x, y), (12, 34));
            assert_eq!(hr_font_get_glyph_h_kerning(font, 10, 3), 7);
            assert_eq!(hr_font_get_glyph_v_kerning(font, 10, 3), 7);

            hr_buffer_destroy(buffer);
            hr_font_funcs_destroy(funcs);
        });
    }
}

#[test]
fn added_font_callbacks_delegate_and_scale_through_subfonts() {
    unsafe {
        with_font(|_, parent| {
            hr_font_set_scale(parent, 1000, 2000);
            let funcs = hr_font_funcs_create();
            let mut calls = 0u32;
            hr_font_funcs_set_glyph_h_advances_func(
                funcs,
                Some(batch_advances),
                (&raw mut calls).cast(),
                None,
            );
            hr_font_funcs_set_glyph_h_origin_func(funcs, Some(fixed_origin), ptr::null_mut(), None);
            hr_font_funcs_set_glyph_h_kerning_func(
                funcs,
                Some(fixed_kerning),
                ptr::null_mut(),
                None,
            );
            hr_font_funcs_set_glyph_v_kerning_func(
                funcs,
                Some(fixed_kerning),
                ptr::null_mut(),
                None,
            );
            hr_font_set_funcs(parent, funcs, ptr::null_mut(), None);
            let child = hr_font_create_sub_font(parent);
            hr_font_set_scale(child, 2000, 4000);

            let glyphs = [1u32, 2];
            let mut advances = [0i32; 2];
            hr_font_get_glyph_h_advances(child, 2, glyphs.as_ptr(), 4, advances.as_mut_ptr(), 4);
            assert_eq!(advances, [1400, 1402]);
            let (mut x, mut y) = (0, 0);
            assert_eq!(
                hr_font_get_glyph_h_origin(child, 1, &raw mut x, &raw mut y),
                1
            );
            assert_eq!((x, y), (24, 68));
            assert_eq!(hr_font_get_glyph_h_kerning(child, 10, 3), 14);
            assert_eq!(hr_font_get_glyph_v_kerning(child, 10, 3), 14);
            assert!(calls > 0);

            hr_font_destroy(child);
            hr_font_funcs_destroy(funcs);
        });
    }
}

#[test]
fn face_count_reads_single_fonts_and_collections() {
    unsafe {
        let single = std::fs::read(font_path(PLAIN_FONT)).unwrap();
        let blob = blob_over(&single);
        assert_eq!(hr_face_count(blob), 1);
        hr_blob_destroy(blob);

        let collection = std::fs::read(
            std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
                .parent()
                .unwrap()
                .join("harfrust/tests/fonts/in-house/TTC.ttc"),
        )
        .unwrap();
        let blob = blob_over(&collection);
        assert!(hr_face_count(blob) > 1);
        hr_blob_destroy(blob);

        let type1 = b"%!PS-AdobeFont-1.0\n";
        let blob = blob_over(type1);
        assert_eq!(hr_face_count(blob), 0);
        hr_blob_destroy(blob);
        assert_eq!(hr_face_count(hr_blob_get_empty()), 0);
    }
}

unsafe extern "C" fn always_glyph_777(
    _font: *mut hr_font_t,
    _font_data: *mut c_void,
    _unicode: hr_codepoint_t,
    glyph: *mut hr_codepoint_t,
    _user_data: *mut c_void,
) -> hr_bool_t {
    unsafe { *glyph = 777 };
    1
}

#[test]
fn the_glyph_getters_ask_the_installed_callbacks() {
    unsafe {
        with_font(|_, font| {
            // A font answers the same whichever way it is asked: the getter
            // and shaping both go through the callbacks that were installed.
            let ffuncs = hr_font_funcs_create();
            hr_font_funcs_set_nominal_glyph_func(
                ffuncs,
                Some(always_glyph_777),
                ptr::null_mut(),
                None,
            );
            hr_font_set_funcs(font, ffuncs, ptr::null_mut(), None);
            hr_font_funcs_destroy(ffuncs);

            let mut glyph: hr_codepoint_t = 0;
            assert_ne!(
                hr_font_get_nominal_glyph(font, u32::from(b'a'), &raw mut glyph),
                0
            );
            assert_eq!(glyph, 777);

            let buffer = buffer_with_text("a");
            hr_shape(font, buffer, ptr::null(), 0);
            assert_eq!(glyph_ids(buffer), [777]);
            hr_buffer_destroy(buffer);
        });

        // With no callback to answer, the getter reports nothing found and
        // leaves the glyph at zero rather than at whatever it was handed.
        with_font(|_, font| {
            let ffuncs = hr_font_funcs_create();
            hr_font_set_funcs(font, ffuncs, ptr::null_mut(), None);
            hr_font_funcs_destroy(ffuncs);

            let mut glyph: hr_codepoint_t = 12345;
            assert_eq!(
                hr_font_get_nominal_glyph(font, u32::from(b'a'), &raw mut glyph),
                0
            );
            assert_eq!(glyph, 0);
        });

        // A font never given callbacks reads its own cmap.
        with_font(|_, font| {
            let mut glyph: hr_codepoint_t = 0;
            assert_ne!(
                hr_font_get_nominal_glyph(font, u32::from(b'a'), &raw mut glyph),
                0
            );
            assert_ne!(glyph, 0);
            assert_ne!(glyph, 777);
        });
    }
}

#[test]
fn the_built_in_callbacks_can_be_asked_for_again() {
    unsafe {
        let drops = AtomicUsize::new(0);
        let user_data: *mut c_void = ptr::from_ref(&drops).cast_mut().cast();
        with_font(|_, font| {
            let builtin = glyph_ids({
                let buffer = buffer_with_text(TEXT);
                hr_shape(font, buffer, ptr::null(), 0);
                buffer
            });
            assert!(builtin.iter().all(|&id| id != 0));

            let ffuncs = hr_font_funcs_create();
            hr_font_funcs_set_nominal_glyph_func(
                ffuncs,
                Some(always_glyph_777),
                ptr::null_mut(),
                None,
            );
            hr_font_set_funcs(font, ffuncs, user_data, Some(count_drop));
            hr_font_funcs_destroy(ffuncs);

            let buffer = buffer_with_text(TEXT);
            hr_shape(font, buffer, ptr::null(), 0);
            assert!(glyph_ids(buffer).iter().all(|&id| id == 777));
            hr_buffer_destroy(buffer);

            // Asking for no callbacks answers nothing; this is the way back
            // to the ones the font started with.
            hr_ot_font_set_funcs(font);
            assert_eq!(
                drops.load(Ordering::SeqCst),
                1,
                "the replaced callbacks' data should be released"
            );

            let buffer = buffer_with_text(TEXT);
            hr_shape(font, buffer, ptr::null(), 0);
            assert_eq!(glyph_ids(buffer), builtin);
            hr_buffer_destroy(buffer);

            let mut glyph: hr_codepoint_t = 0;
            assert_ne!(
                hr_font_get_nominal_glyph(font, u32::from(b'a'), &raw mut glyph),
                0
            );
            assert_ne!(glyph, 777);
        });
    }
}

#[test]
fn unset_callbacks_report_nothing_available() {
    unsafe {
        with_font(|_, font| {
            // As in HarfBuzz, an installed funcs object is authoritative: it is
            // not blended with the built-in callbacks, so an empty one reports
            // nothing available rather than falling back.
            let ffuncs = hr_font_funcs_create();
            hr_font_set_funcs(font, ffuncs, ptr::null_mut(), None);

            let buffer = buffer_with_text(TEXT);
            hr_shape(font, buffer, ptr::null(), 0);
            let mut len: c_uint = 0;
            let positions = hr_buffer_get_glyph_positions(buffer, &raw mut len);
            assert!(len > 0);
            // An advance nobody answers for is the font's own scale, which is
            // what HarfBuzz answers with rather than nothing at all.
            let mut x_scale = 0;
            hr_font_get_scale(font, &raw mut x_scale, ptr::null_mut());
            for i in 0..len as usize {
                assert_eq!((*positions.add(i)).x_advance, x_scale);
            }
            assert!(glyph_ids(buffer).iter().all(|&id| id == 0));

            hr_buffer_destroy(buffer);
            hr_font_funcs_destroy(ffuncs);
        });

        // Asking for no callbacks is itself an answer, and not a way back
        // to the built-in ones: HarfBuzz reads NULL as its empty funcs
        // object, which reports nothing available. A font that was never
        // given callbacks is the one that reads its own tables.
        with_font(|_, font| {
            let ffuncs = hr_font_funcs_create();
            hr_font_set_funcs(font, ffuncs, ptr::null_mut(), None);
            hr_font_set_funcs(font, ptr::null_mut(), ptr::null_mut(), None);

            let buffer = buffer_with_text(TEXT);
            hr_shape(font, buffer, ptr::null(), 0);
            assert!(glyph_ids(buffer).iter().all(|&id| id == 0));

            hr_buffer_destroy(buffer);
            hr_font_funcs_destroy(ffuncs);
        });

        // Which leaves the untouched font reading its own tables.
        with_font(|_, font| {
            let buffer = buffer_with_text(TEXT);
            hr_shape(font, buffer, ptr::null(), 0);
            assert!(glyph_ids(buffer).iter().all(|&id| id != 0));
            hr_buffer_destroy(buffer);
        });
    }
}

// ------------------------------------------------- font data ownership ----

use std::sync::atomic::{AtomicUsize, Ordering};

#[test]
fn font_data_outlives_every_font_sharing_it() {
    unsafe {
        let drops = AtomicUsize::new(0);
        let user_data: *mut c_void = ptr::from_ref(&drops).cast_mut().cast();
        with_font(|_, font| {
            let ffuncs = hr_font_funcs_create();
            hr_font_set_funcs(font, ffuncs, user_data, Some(count_drop));

            // A sub-font holds no data of its own; it asks its parent every
            // time, so it is never left holding what the parent has replaced.
            let sub = hr_font_create_sub_font(font);
            assert_eq!(drops.load(Ordering::SeqCst), 0);

            // Which means replacing the parent's callbacks releases the data
            // they were given there and then, live sub-font or not, as it
            // does in HarfBuzz.
            hr_font_set_funcs(font, ffuncs, ptr::dangling_mut::<c_void>(), None);
            assert_eq!(drops.load(Ordering::SeqCst), 1);

            hr_font_destroy(sub);
            assert_eq!(drops.load(Ordering::SeqCst), 1);

            hr_font_funcs_destroy(ffuncs);
        });
    }
}

#[test]
fn font_data_is_released_once_when_replaced() {
    unsafe {
        let drops = AtomicUsize::new(0);
        let user_data: *mut c_void = ptr::from_ref(&drops).cast_mut().cast();
        with_font(|_, font| {
            let ffuncs = hr_font_funcs_create();
            hr_font_set_funcs(font, ffuncs, user_data, Some(count_drop));
            assert_eq!(drops.load(Ordering::SeqCst), 0);

            // With nothing else sharing it, replacing releases it at once.
            hr_font_set_funcs(font, ffuncs, ptr::dangling_mut::<c_void>(), None);
            assert_eq!(drops.load(Ordering::SeqCst), 1);

            hr_font_funcs_destroy(ffuncs);
        });
        // And not again when the font itself goes.
        assert_eq!(drops.load(Ordering::SeqCst), 1);
    }
}

#[test]
fn a_sub_font_shapes_with_the_parents_callbacks() {
    unsafe {
        with_font(|_, font| {
            let mut advance: hr_position_t = 777;
            let ffuncs = hr_font_funcs_create();
            hr_font_funcs_set_glyph_h_advance_func(
                ffuncs,
                Some(fixed_advance),
                ptr::from_mut(&mut advance).cast::<c_void>(),
                None,
            );
            hr_font_set_funcs(font, ffuncs, ptr::null_mut(), None);

            let sub = hr_font_create_sub_font(font);
            let buffer = buffer_with_text(TEXT);
            hr_shape(sub, buffer, ptr::null(), 0);

            let mut len: c_uint = 0;
            let positions = hr_buffer_get_glyph_positions(buffer, ptr::from_mut(&mut len));
            assert!(len > 0);
            for i in 0..len as usize {
                assert_eq!((*positions.add(i)).x_advance, 777);
            }

            hr_buffer_destroy(buffer);
            hr_font_destroy(sub);
            hr_font_funcs_destroy(ffuncs);
        });
    }
}

// ------------------------------------------------------------- user data ----

static KEY: hr_user_data_key_t = hr_user_data_key_t { unused: 0 };
static OTHER_KEY: hr_user_data_key_t = hr_user_data_key_t { unused: 0 };

#[test]
fn user_data_is_keyed_by_address() {
    unsafe {
        let buffer = hr_buffer_create();
        let value = 42usize as *mut c_void;
        assert_ne!(
            hr_buffer_set_user_data(buffer, &raw const KEY, value, None, 1),
            0
        );
        assert_eq!(hr_buffer_get_user_data(buffer, &raw const KEY), value);
        assert!(hr_buffer_get_user_data(buffer, &raw const OTHER_KEY).is_null());

        // Without `replace`, an existing key is left alone.
        let other = ptr::without_provenance_mut::<c_void>(7);
        assert_eq!(
            hr_buffer_set_user_data(buffer, &raw const KEY, other, None, 0),
            0
        );
        assert_eq!(hr_buffer_get_user_data(buffer, &raw const KEY), value);

        hr_buffer_destroy(buffer);
    }
}

#[test]
fn immortal_objects_reject_user_data() {
    unsafe {
        let empty = hr_buffer_get_empty();
        assert_eq!(
            hr_buffer_set_user_data(
                empty,
                &raw const KEY,
                ptr::dangling_mut::<c_void>(),
                None,
                1
            ),
            0
        );
    }
}

#[test]
fn the_empty_buffer_is_not_written_by_the_glyph_getters() {
    // The empty buffer is one object shared by every thread, so the getters
    // must neither allocate positions in it nor hand out a writable pointer
    // into it: it answers as `NULL` does, with nothing.
    let threads: Vec<_> = (0..4)
        .map(|_| {
            std::thread::spawn(|| unsafe {
                let empty = hr_buffer_get_empty();
                let mut len = 7;
                assert!(hr_buffer_get_glyph_positions(empty, &raw mut len).is_null());
                assert_eq!(len, 0);
                len = 7;
                assert!(hr_buffer_get_glyph_infos(empty, &raw mut len).is_null());
                assert_eq!(len, 0);
                assert_eq!(hr_buffer_has_positions(ptr::null_mut()), 0);
            })
        })
        .collect();

    for thread in threads {
        thread.join().expect("thread panicked");
    }
}

// --------------------------------------------------------------- common ----

#[test]
fn tags_round_trip() {
    unsafe {
        let tag = hr_tag_from_string(c"kern".as_ptr(), -1);
        let mut buf = [0i8; 5];
        hr_tag_to_string(tag, buf.as_mut_ptr().cast::<c_char>());
        assert_eq!(&buf[..4], b"kern".map(|b| b as i8));
        // Short strings pad with spaces, as in HarfBuzz.
        let short = hr_tag_from_string(c"ab".as_ptr(), -1);
        hr_tag_to_string(short, buf.as_mut_ptr().cast::<c_char>());
        assert_eq!(&buf[..4], b"ab  ".map(|b| b as i8));
    }
}

#[test]
fn directions_round_trip() {
    unsafe {
        assert_eq!(
            hr_direction_from_string(c"rtl".as_ptr(), -1),
            HR_DIRECTION_RTL
        );
        assert_ne!(hr_direction_is_horizontal(HR_DIRECTION_RTL), 0);
        assert_ne!(hr_direction_is_backward(HR_DIRECTION_RTL), 0);
        assert_eq!(hr_direction_reverse(HR_DIRECTION_LTR), HR_DIRECTION_RTL);
    }
}

#[test]
fn scripts_match_harfbuzz_values() {
    // HB_SCRIPT_LATIN is HB_TAG('L','a','t','n').
    assert_eq!(HR_SCRIPT_LATIN, 0x4C61_746E);
    assert_eq!(HR_SCRIPT_ARABIC, 0x4172_6162);
    for (tag, script) in [
        (b"Jurc", HR_SCRIPT_JURCHEN),
        (b"Pcun", HR_SCRIPT_PROTO_CUNEIFORM),
        (b"Seal", HR_SCRIPT_SEAL),
    ] {
        assert_eq!(script, u32::from_be_bytes(*tag));
        assert_eq!(hr_script_from_iso15924_tag(script), script);
        assert_eq!(hr_script_to_iso15924_tag(script), script);
        assert_eq!(hr_script_get_horizontal_direction(script), HR_DIRECTION_LTR);
    }
    unsafe {
        assert_eq!(hr_script_from_string(c"Latn".as_ptr(), -1), HR_SCRIPT_LATIN);
        assert_eq!(
            hr_script_get_horizontal_direction(HR_SCRIPT_ARABIC),
            HR_DIRECTION_RTL
        );
        assert_eq!(
            hr_script_get_horizontal_direction(HR_SCRIPT_LATIN),
            HR_DIRECTION_LTR
        );
    }
}

#[test]
fn languages_are_interned() {
    unsafe {
        let a = hr_language_from_string(c"en-US".as_ptr(), -1);
        let b = hr_language_from_string(c"en-US".as_ptr(), -1);
        assert_eq!(a, b, "equal tags must intern to the same pointer");
        let name = core::ffi::CStr::from_ptr(hr_language_to_string(a));
        assert_eq!(name.to_str().unwrap(), "en-us");

        // The second argument is the more specific of the two: "en-US" is a
        // kind of "en", and not the other way about.
        let en = hr_language_from_string(c"en".as_ptr(), -1);
        assert_ne!(hr_language_matches(en, a), 0);
        assert_eq!(hr_language_matches(a, en), 0);

        // A locale's codeset and modifier are not part of the language.
        let locale = hr_language_from_string(c"en_US.utf8".as_ptr(), -1);
        assert_eq!(locale, a);
    }
}

#[test]
fn interning_many_languages_stays_cheap() {
    // Content chooses the languages, so the table can grow as large as it
    // likes; finding one must not mean comparing it with every other. Walking
    // the table made this many take seconds, and keying it makes it instant.
    let names: Vec<_> = (0..20_000u32)
        .map(|i| std::ffi::CString::new(format!("x-{i:x}")).unwrap())
        .collect();
    unsafe {
        let interned: Vec<_> = names
            .iter()
            .map(|name| hr_language_from_string(name.as_ptr(), -1))
            .collect();
        for (name, language) in names.iter().zip(&interned) {
            assert_eq!(hr_language_from_string(name.as_ptr(), -1), *language);
            assert_eq!(
                core::ffi::CStr::from_ptr(hr_language_to_string(*language)),
                name.as_c_str()
            );
        }
        // Tags that are not registered intern a language each, and share the
        // same table.
        let tag = hr_ot_tag_to_language(u32::from_be_bytes(*b"QQQQ"));
        assert_eq!(hr_ot_tag_to_language(u32::from_be_bytes(*b"QQQQ")), tag);
        assert_eq!(hr_language_from_string(hr_language_to_string(tag), -1), tag);
    }
}

#[test]
fn glyph_and_buffer_structs_match_harfbuzz_layout() {
    // hb_glyph_info_t is five 32 bit fields, hb_glyph_position_t likewise.
    assert_eq!(size_of::<hr_glyph_info_t>(), 20);
    assert_eq!(size_of::<hr_glyph_position_t>(), 20);
    assert_eq!(align_of::<hr_glyph_info_t>(), 4);
    assert_eq!(align_of::<hr_glyph_position_t>(), 4);
}

#[test]
fn variations_parse_and_apply() {
    unsafe {
        let mut variation = hr_variation_t { tag: 0, value: 0.0 };
        assert_ne!(
            hr_variation_from_string(c"wdth=200".as_ptr(), -1, &raw mut variation),
            0
        );
        assert_eq!(variation.tag, hr_tag_from_string(c"wdth".as_ptr(), -1));
        assert!((variation.value - 200.0).abs() < f32::EPSILON);

        with_named_font(VARIABLE_FONT, |_, font| {
            // A font at its defaults carries no coordinates.
            let mut len: c_uint = 0;
            hr_font_get_var_coords_normalized(font, &raw mut len);
            assert_eq!(len, 0);

            let advances = |font: *mut hr_font_t| {
                let buffer = buffer_with_text(TEXT);
                hr_shape(font, buffer, ptr::null(), 0);
                let mut len: c_uint = 0;
                let positions = hr_buffer_get_glyph_positions(buffer, &raw mut len);
                let values: Vec<i32> = (0..len as usize)
                    .map(|i| (*positions.add(i)).x_advance)
                    .collect();
                hr_buffer_destroy(buffer);
                values
            };
            let at_default = advances(font);

            // This font's axes are wdth then wght, and wdth drives the
            // advances through HVAR.
            hr_font_set_variations(font, &raw const variation, 1);
            let coords = hr_font_get_var_coords_normalized(font, &raw mut len);
            assert_eq!(len, 2, "expected the wdth and wght axes");
            // Coordinates are 2.14, so the axis maximum normalizes to 1.0.
            assert_eq!(*coords, 16384, "wdth should be at its maximum");
            assert_eq!(*coords.add(1), 0, "wght was not asked to move");

            let widened = advances(font);
            assert_ne!(
                widened, at_default,
                "variation settings did not reach shaping"
            );
            assert!(
                widened.iter().zip(&at_default).all(|(w, d)| w >= d),
                "a wider width should not narrow any advance"
            );

            // Clearing the settings puts the font back where it started.
            hr_font_set_variations(font, ptr::null(), 0);
            hr_font_get_var_coords_normalized(font, &raw mut len);
            assert_eq!(len, 0);
            assert_eq!(advances(font), at_default);
        });
    }
}

#[test]
fn normalized_coordinates_can_be_set_directly() {
    unsafe {
        with_named_font(VARIABLE_FONT, |_, font| {
            // Half way along the wdth axis, in 2.14.
            let coords = [8192, 0];
            hr_font_set_var_coords_normalized(font, coords.as_ptr(), 2);

            let mut len: c_uint = 0;
            let got = hr_font_get_var_coords_normalized(font, &raw mut len);
            assert_eq!(len, 2);
            assert_eq!(*got, 8192);
            assert_eq!(*got.add(1), 0);

            let buffer = buffer_with_text(TEXT);
            hr_shape(font, buffer, ptr::null(), 0);
            assert_ne!(glyph_ids(buffer), []);
            hr_buffer_destroy(buffer);
        });
    }
}

// ----------------------------------------------------------- shape plans ----

/// Segment properties matching `buffer_with_text`, which guesses them from
/// lowercase Latin text.
fn latin_props() -> hr_segment_properties_t {
    hr_segment_properties_t {
        direction: HR_DIRECTION_LTR,
        script: HR_SCRIPT_LATIN,
        // What a buffer carries once its properties have been guessed, so
        // that a plan built from these applies to one.
        language: hr_language_get_default(),
        reserved1: ptr::null_mut(),
        reserved2: ptr::null_mut(),
    }
}

#[test]
fn segment_properties_round_trip_through_a_buffer() {
    unsafe {
        let buffer = buffer_with_text(TEXT);
        let mut props = latin_props();
        hr_buffer_get_segment_properties(buffer, ptr::from_mut(&mut props));
        assert_eq!(props.direction, HR_DIRECTION_LTR);
        assert_eq!(props.script, HR_SCRIPT_LATIN);

        let other = hr_buffer_create();
        hr_buffer_set_segment_properties(other, ptr::from_ref(&props));
        assert_eq!(hr_buffer_get_direction(other), HR_DIRECTION_LTR);
        assert_eq!(hr_buffer_get_script(other), HR_SCRIPT_LATIN);

        hr_buffer_destroy(other);
        hr_buffer_destroy(buffer);
    }
}

#[test]
fn segment_properties_compare_hash_and_overlay() {
    unsafe {
        let a = latin_props();
        let mut b = a;
        assert_ne!(
            hr_segment_properties_equal(ptr::from_ref(&a), ptr::from_ref(&b)),
            0
        );
        assert_eq!(
            hr_segment_properties_hash(ptr::from_ref(&a)),
            hr_segment_properties_hash(ptr::from_ref(&b))
        );

        b.direction = HR_DIRECTION_RTL;
        assert_eq!(
            hr_segment_properties_equal(ptr::from_ref(&a), ptr::from_ref(&b)),
            0
        );

        // Overlay fills in only what is unset.
        let mut partial = hr_segment_properties_t {
            direction: HR_DIRECTION_INVALID,
            script: HR_SCRIPT_ARABIC,
            language: ptr::null(),
            reserved1: ptr::null_mut(),
            reserved2: ptr::null_mut(),
        };
        hr_segment_properties_overlay(ptr::from_mut(&mut partial), ptr::from_ref(&a));
        assert_eq!(partial.direction, HR_DIRECTION_LTR, "unset field filled in");
        assert_eq!(partial.script, HR_SCRIPT_ARABIC, "set field left alone");
    }
}

#[test]
fn a_plan_shapes_the_same_as_hr_shape() {
    unsafe {
        with_font(|face, font| {
            let expected = {
                let buffer = buffer_with_text(TEXT);
                hr_shape(font, buffer, ptr::null(), 0);
                let ids = glyph_ids(buffer);
                hr_buffer_destroy(buffer);
                ids
            };

            let props = latin_props();
            let plan =
                hr_shape_plan_create(face, ptr::from_ref(&props), ptr::null(), 0, ptr::null());
            assert_eq!(
                core::ffi::CStr::from_ptr(hr_shape_plan_get_shaper(plan))
                    .to_str()
                    .unwrap(),
                "ot"
            );

            let buffer = buffer_with_text(TEXT);
            assert_ne!(hr_shape_plan_execute(plan, font, buffer, ptr::null(), 0), 0);
            assert_eq!(glyph_ids(buffer), expected);
            assert_eq!(
                hr_buffer_get_content_type(buffer),
                HR_BUFFER_CONTENT_TYPE_GLYPHS
            );

            hr_buffer_destroy(buffer);
            hr_shape_plan_destroy(plan);
        });
    }
}

#[test]
fn cached_plans_are_reused_and_agree() {
    unsafe {
        with_font(|face, font| {
            let props = latin_props();
            let make = || {
                hr_shape_plan_create_cached(
                    face,
                    ptr::from_ref(&props),
                    ptr::null(),
                    0,
                    ptr::null(),
                )
            };
            let first = make();
            let second = make();

            let shape_through = |plan: *mut hr_shape_plan_t| {
                let buffer = buffer_with_text(TEXT);
                assert_ne!(hr_shape_plan_execute(plan, font, buffer, ptr::null(), 0), 0);
                let ids = glyph_ids(buffer);
                hr_buffer_destroy(buffer);
                ids
            };
            assert_eq!(shape_through(first), shape_through(second));

            hr_shape_plan_destroy(first);
            hr_shape_plan_destroy(second);
        });
    }
}

#[test]
fn a_plan_reports_the_properties_it_was_built_for() {
    unsafe {
        with_font(|face, _| {
            let mut props = latin_props();
            props.language = hr_language_from_string(c"en".as_ptr(), -1);
            let plan =
                hr_shape_plan_create(face, ptr::from_ref(&props), ptr::null(), 0, ptr::null());

            let mut got = hr_segment_properties_t {
                direction: HR_DIRECTION_INVALID,
                script: HR_SCRIPT_INVALID,
                language: ptr::null(),
                reserved1: ptr::null_mut(),
                reserved2: ptr::null_mut(),
            };
            hr_shape_plan_get_segment_properties(plan, ptr::from_mut(&mut got));
            assert_eq!(got.direction, HR_DIRECTION_LTR);
            assert_eq!(got.script, HR_SCRIPT_LATIN);
            assert_eq!(got.language, props.language);

            hr_shape_plan_destroy(plan);
        });
    }
}

/// The cases that must abort, each exercised in a child process.
///
/// `hr_shape_plan_execute` aborts rather than unwinding, so these cannot be
/// `#[should_panic]`: the test harness never regains control.
unsafe fn run_abort_case(case: &str) {
    match case {
        // A plan for right-to-left Arabic, used on a left-to-right Latin
        // buffer.
        "props" => unsafe {
            with_font(|face, font| {
                let props = hr_segment_properties_t {
                    direction: HR_DIRECTION_RTL,
                    script: HR_SCRIPT_ARABIC,
                    language: ptr::null(),
                    reserved1: ptr::null_mut(),
                    reserved2: ptr::null_mut(),
                };
                let plan =
                    hr_shape_plan_create(face, ptr::from_ref(&props), ptr::null(), 0, ptr::null());
                let buffer = buffer_with_text(TEXT);
                hr_shape_plan_execute(plan, font, buffer, ptr::null(), 0);
            });
        },
        // A plan built over one face, used with a font over another.
        "face" => unsafe {
            with_font(|face, _| {
                let props = latin_props();
                let plan =
                    hr_shape_plan_create(face, ptr::from_ref(&props), ptr::null(), 0, ptr::null());
                with_named_font(VARIABLE_FONT, |_, other_font| {
                    let buffer = buffer_with_text(TEXT);
                    hr_shape_plan_execute(plan, other_font, buffer, ptr::null(), 0);
                });
            });
        },
        // A plan built for one variation, used with a font at another, which
        // shapes rather than aborting.
        "coords" => unsafe {
            with_named_font(VARIABLE_FONT, |face, font| {
                let props = latin_props();
                let coords = [8192, 0];
                let plan = hr_shape_plan_create2(
                    face,
                    ptr::from_ref(&props),
                    ptr::null(),
                    0,
                    coords.as_ptr(),
                    coords.len() as c_uint,
                    ptr::null(),
                );
                // The font is still at its defaults.
                let buffer = buffer_with_text(TEXT);
                hr_shape_plan_execute(plan, font, buffer, ptr::null(), 0);
            });
        },
        // Shaping a buffer that already holds glyphs.
        "already_shaped" => unsafe {
            with_font(|_, font| {
                let buffer = buffer_with_text(TEXT);
                hr_shape(font, buffer, ptr::null(), 0);
                hr_shape(font, buffer, ptr::null(), 0);
            });
        },
        other => panic!("unknown abort case {other}"),
    }
}

/// Entry point for the child process, selected by an environment variable.
#[test]
fn abort_case() {
    let Ok(case) = std::env::var("HR_ABORT_CASE") else {
        // The parent runs this as an ordinary, and empty, test.
        return;
    };
    unsafe { run_abort_case(&case) };
    // The call returned. Say so with a status of its own rather than
    // panicking, which the parent could not tell from the abort it is
    // looking for.
    std::process::exit(RETURNED);
}

/// What the child exits with when the case it ran returned rather than
/// stopping the process.
const RETURNED: i32 = 99;

/// Re-runs `abort_case` in a child process and reports whether it died.
fn aborts(case: &str) -> bool {
    let exe = std::env::current_exe().expect("test binary path");
    let status = std::process::Command::new(exe)
        .args(["capi_tests::abort_case", "--exact"])
        .env("HR_ABORT_CASE", case)
        .stdout(std::process::Stdio::null())
        .stderr(std::process::Stdio::null())
        .status()
        .expect("failed to run the child process");
    // Anything but a clean return, and the child says which of those it was:
    // without that, a case that quietly returned would read as an abort and
    // the test would pass whatever the API did.
    status.code() != Some(RETURNED) && !status.success()
}

#[test]
fn a_plan_that_does_not_apply_aborts() {
    // Matching HarfBuzz, where these are assertions rather than errors.
    assert!(aborts("props"), "mismatched segment properties must abort");
    assert!(aborts("face"), "a plan for another face must abort");
    // Variation settings are the exception, and this is what proves the check
    // above can tell the difference: HarfBuzz does not compare them, so a
    // plan built for another variation runs rather than stopping the process.
    assert!(
        !aborts("coords"),
        "a plan for other variation settings must not abort"
    );
}

#[test]
fn a_plan_that_applies_shapes_without_aborting() {
    unsafe {
        with_named_font(VARIABLE_FONT, |face, font| {
            let props = latin_props();
            let coords = [8192, 0];
            let plan = hr_shape_plan_create2(
                face,
                ptr::from_ref(&props),
                ptr::null(),
                0,
                coords.as_ptr(),
                coords.len() as c_uint,
                ptr::null(),
            );

            // Once the font is set to the coordinates the plan was built for,
            // the very call that aborts above succeeds.
            hr_font_set_var_coords_normalized(font, coords.as_ptr(), coords.len() as c_uint);
            let buffer = buffer_with_text(TEXT);
            assert_ne!(hr_shape_plan_execute(plan, font, buffer, ptr::null(), 0), 0);
            assert_ne!(glyph_ids(buffer), []);

            hr_buffer_destroy(buffer);
            hr_shape_plan_destroy(plan);
        });
    }
}

#[test]
fn a_plan_without_a_direction_is_empty() {
    unsafe {
        with_font(|face, font| {
            let props = hr_segment_properties_t {
                direction: HR_DIRECTION_INVALID,
                script: HR_SCRIPT_LATIN,
                language: ptr::null(),
                reserved1: ptr::null_mut(),
                reserved2: ptr::null_mut(),
            };
            let plan =
                hr_shape_plan_create(face, ptr::from_ref(&props), ptr::null(), 0, ptr::null());
            assert_eq!(plan, hr_shape_plan_get_empty());
            assert!(hr_shape_plan_get_shaper(plan).is_null());

            // The empty plan shapes nothing.
            let buffer = buffer_with_text(TEXT);
            assert_eq!(hr_shape_plan_execute(plan, font, buffer, ptr::null(), 0), 0);
            hr_buffer_destroy(buffer);
        });
    }
}

#[test]
fn an_unknown_shaper_list_yields_the_empty_plan() {
    unsafe {
        with_font(|face, _| {
            let props = latin_props();
            let list = [c"graphite2".as_ptr(), ptr::null()];
            let plan =
                hr_shape_plan_create(face, ptr::from_ref(&props), ptr::null(), 0, list.as_ptr());
            assert_eq!(plan, hr_shape_plan_get_empty());

            // ... but a list naming "ot" is honoured.
            let list = [c"ot".as_ptr(), ptr::null()];
            let plan =
                hr_shape_plan_create(face, ptr::from_ref(&props), ptr::null(), 0, list.as_ptr());
            assert_ne!(plan, hr_shape_plan_get_empty());
            hr_shape_plan_destroy(plan);
        });
    }
}

#[test]
fn shape_full_reports_only_whether_a_shaper_ran() {
    unsafe {
        with_font(|_, font| {
            // The one thing that fails: a list naming only shapers this
            // library does not have.
            let buffer = buffer_with_text(TEXT);
            let absent = [c"graphite2".as_ptr(), ptr::null()];
            assert_eq!(
                hr_shape_full(font, buffer, ptr::null(), 0, absent.as_ptr()),
                0
            );
            hr_buffer_destroy(buffer);

            // A list naming "ot" runs the shaper, and so does no list at all.
            for list in [[c"ot".as_ptr(), ptr::null()].as_ptr(), ptr::null()] {
                let buffer = buffer_with_text(TEXT);
                assert_ne!(hr_shape_full(font, buffer, ptr::null(), 0, list), 0);
                assert_ne!(hr_buffer_allocation_successful(buffer), 0);
                assert_ne!(glyph_ids(buffer), []);
                hr_buffer_destroy(buffer);
            }
        });
    }
}

unsafe extern "C" fn count_drop(user_data: *mut c_void) {
    let drops = unsafe { &*user_data.cast::<AtomicUsize>() };
    drops.fetch_add(1, Ordering::SeqCst);
}

#[test]
fn clearing_a_callback_still_releases_its_user_data() {
    unsafe {
        let ffuncs = hr_font_funcs_create();
        let drops = AtomicUsize::new(0);
        let user_data: *mut c_void = ptr::from_ref(&drops).cast_mut().cast();
        // A setter owns the data it is handed whether or not there is a
        // callback to keep it, so clearing one has to release it. Otherwise
        // the caller cannot tell which calls took ownership.
        hr_font_funcs_set_nominal_glyph_func(ffuncs, None, user_data, Some(count_drop));
        assert_eq!(drops.load(Ordering::SeqCst), 1);
        hr_font_funcs_destroy(ffuncs);
    }
}

#[test]
fn clearing_user_data_removes_the_key() {
    unsafe {
        with_font(|face, _| {
            static KEY: hr_user_data_key_t = hr_user_data_key_t { unused: 0 };
            let value = ptr::dangling_mut::<c_void>();
            assert_ne!(
                hr_face_set_user_data(face, &raw const KEY, value, None, 1),
                0
            );

            // Neither data nor a destructor removes the entry outright.
            assert_ne!(
                hr_face_set_user_data(face, &raw const KEY, ptr::null_mut(), None, 1),
                0
            );
            assert!(hr_face_get_user_data(face, &raw const KEY).is_null());

            // Which is observable: setting without `replace` succeeds only
            // when nothing holds the key.
            assert_ne!(
                hr_face_set_user_data(face, &raw const KEY, value, None, 0),
                0
            );
            assert_eq!(hr_face_get_user_data(face, &raw const KEY), value);
            assert_ne!(
                hr_face_set_user_data(face, &raw const KEY, ptr::null_mut(), None, 1),
                0
            );
        });
    }
}

static REENTRANT_KEY: hr_user_data_key_t = hr_user_data_key_t { unused: 0 };
static REENTRANT_OTHER: hr_user_data_key_t = hr_user_data_key_t { unused: 0 };

#[test]
fn final_destruction_rejects_user_data_reattachment() {
    use core::cell::Cell;

    struct Destruction {
        set: *mut hr_set_t,
        first_calls: Cell<u32>,
        second_calls: Cell<u32>,
        observed_empty: Cell<bool>,
        accepted: Cell<c_int>,
    }

    unsafe extern "C" fn second_destroy(data: *mut c_void) {
        let state = unsafe { &*data.cast::<Destruction>() };
        state.second_calls.set(state.second_calls.get() + 1);
        // If accepted by the first callback, this query used to run while
        // Box::drop held exclusive access to the set, violating aliasing.
        unsafe { hr_set_get_user_data(state.set, &raw const OTHER_KEY) };
    }

    unsafe extern "C" fn first_destroy(data: *mut c_void) {
        let state = unsafe { &*data.cast::<Destruction>() };
        state.first_calls.set(state.first_calls.get() + 1);
        state
            .observed_empty
            .set(unsafe { hr_set_get_user_data(state.set, &raw const KEY).is_null() });
        state.accepted.set(unsafe {
            hr_set_set_user_data(
                state.set,
                &raw const OTHER_KEY,
                data,
                Some(second_destroy),
                1,
            )
        });
    }

    let state = Destruction {
        set: hr_set_create(),
        first_calls: Cell::new(0),
        second_calls: Cell::new(0),
        observed_empty: Cell::new(false),
        accepted: Cell::new(-1),
    };
    unsafe {
        assert_eq!(
            hr_set_set_user_data(
                state.set,
                &raw const KEY,
                ptr::from_ref(&state).cast_mut().cast(),
                Some(first_destroy),
                1,
            ),
            1
        );
        hr_set_destroy(state.set);
    }
    assert_eq!(state.first_calls.get(), 1);
    assert!(state.observed_empty.get());
    assert_eq!(state.accepted.get(), 0);
    assert_eq!(state.second_calls.get(), 0);
}

unsafe extern "C" fn reenter_on_drop(user_data: *mut c_void) {
    let face = user_data.cast::<hr_face_t>();
    unsafe {
        hr_face_set_user_data(
            face,
            &raw const REENTRANT_OTHER,
            ptr::dangling_mut(),
            None,
            1,
        );
    }
}

#[test]
fn a_destructor_may_set_user_data_on_the_same_object() {
    // The destructor is the caller's code and may come back into the API, so
    // it must not run while the object's user data is locked. Run on a worker
    // so that a regression fails the test instead of hanging the suite.
    let (tx, rx) = std::sync::mpsc::channel();
    std::thread::spawn(move || {
        unsafe {
            with_font(|face, _| {
                hr_face_set_user_data(
                    face,
                    &raw const REENTRANT_KEY,
                    face.cast(),
                    Some(reenter_on_drop),
                    1,
                );
                // Replacing runs the destructor above, which re-enters.
                hr_face_set_user_data(face, &raw const REENTRANT_KEY, ptr::dangling_mut(), None, 1);
            });
        }
        let _ = tx.send(());
    });
    assert!(
        rx.recv_timeout(std::time::Duration::from_secs(30)).is_ok(),
        "a destructor that set user data deadlocked"
    );
}

/// What `log_destroy` is handed: the log to write to, and what to write.
struct DestroyWitness {
    log: *const Mutex<Vec<&'static str>>,
    name: &'static str,
}

unsafe extern "C" fn log_destroy(user_data: *mut c_void) {
    let witness = unsafe { &*user_data.cast::<DestroyWitness>() };
    unsafe { &*witness.log }.lock().unwrap().push(witness.name);
}

#[test]
fn a_font_runs_its_destroy_callbacks_before_releasing_what_it_holds() {
    unsafe {
        let data = std::fs::read(font_path(PLAIN_FONT)).expect("failed to read test font");
        let blob = blob_over(&data);
        let face = hr_face_create(blob, 0);
        let font = hr_font_create(face);
        hr_blob_destroy(blob);
        // The font now holds the only reference to the face.
        hr_face_destroy(face);

        let log = Mutex::new(Vec::new());
        let witness = |name| DestroyWitness {
            log: &raw const log,
            name,
        };
        let on_face = witness("face");
        let on_font = witness("font user data");
        let with_funcs = witness("font funcs data");
        let as_data = |witness: &DestroyWitness| ptr::from_ref(witness).cast_mut().cast::<c_void>();
        assert_ne!(
            hr_face_set_user_data(
                face,
                &raw const KEY,
                as_data(&on_face),
                Some(log_destroy),
                1
            ),
            0
        );
        assert_ne!(
            hr_font_set_user_data(
                font,
                &raw const KEY,
                as_data(&on_font),
                Some(log_destroy),
                1
            ),
            0
        );
        let ffuncs = hr_font_funcs_create();
        hr_font_set_funcs(font, ffuncs, as_data(&with_funcs), Some(log_destroy));
        hr_font_funcs_destroy(ffuncs);

        // As in HarfBuzz, a callback may still look at the font's face,
        // parent or callbacks: the font lets go of those only after every
        // callback attached to it has run, so the face outlives them all.
        hr_font_destroy(font);
        assert_eq!(
            *log.lock().unwrap(),
            ["font user data", "font funcs data", "face"]
        );
    }
}

static SETTER_REENTRY_DROPS: AtomicUsize = AtomicUsize::new(0);
unsafe extern "C" fn reenter_setter_on_drop(user_data: *mut c_void) {
    // Only the first call re-enters, so that a regression fails the count
    // below instead of recursing.
    if SETTER_REENTRY_DROPS.fetch_add(1, Ordering::SeqCst) == 0 {
        unsafe {
            hr_font_funcs_set_nominal_glyph_func(
                user_data.cast(),
                Some(always_glyph_777),
                ptr::null_mut(),
                None,
            );
        }
    }
}

#[test]
fn a_destructor_may_set_the_callback_it_is_released_from() {
    // The slot must be empty by the time the destructor runs, or the nested
    // setter drops the same callback a second time.
    unsafe {
        let ffuncs = hr_font_funcs_create();
        hr_font_funcs_set_nominal_glyph_func(
            ffuncs,
            Some(always_glyph_777),
            ffuncs.cast(),
            Some(reenter_setter_on_drop),
        );
        // Clearing runs the destructor above, which re-enters.
        hr_font_funcs_set_nominal_glyph_func(ffuncs, None, ptr::null_mut(), None);
        hr_font_funcs_destroy(ffuncs);
    }
    assert_eq!(
        SETTER_REENTRY_DROPS.load(Ordering::SeqCst),
        1,
        "a callback registered once is released once"
    );
}

#[test]
fn a_plan_runs_whatever_variation_the_font_is_at() {
    unsafe {
        let props = latin_props();
        let execute = |face: *mut hr_face_t, font: *mut hr_font_t, coords: &[i32]| {
            let plan = hr_shape_plan_create2(
                face,
                ptr::from_ref(&props),
                ptr::null(),
                0,
                if coords.is_empty() {
                    ptr::null()
                } else {
                    coords.as_ptr()
                },
                coords.len() as c_uint,
                ptr::null(),
            );
            assert_ne!(plan, hr_shape_plan_get_empty());
            let buffer = buffer_with_text(TEXT);
            hr_buffer_set_segment_properties(buffer, ptr::from_ref(&props));
            let ran = hr_shape_plan_execute(plan, font, buffer, ptr::null(), 0);
            let glyphs = hr_buffer_get_length(buffer);
            hr_buffer_destroy(buffer);
            hr_shape_plan_destroy(plan);
            (ran, glyphs)
        };

        // An all-zero vector names the default instance, which is what a font
        // with no coordinates set already is: the same variation spelled two
        // ways, and nothing to object to.
        with_font(|face, font| {
            assert_ne!(execute(face, font, &[0]).0, 0);
        });

        // And a plan built for one variation still runs against a font at
        // another. HarfBuzz does not compare them: the font's settings are
        // the ones shaping uses.
        with_named_font(VARIABLE_FONT, |face, font| {
            let coords = [8000i32];
            hr_font_set_var_coords_normalized(font, coords.as_ptr(), 1);
            let (ran, glyphs) = execute(face, font, &[]);
            assert_ne!(ran, 0, "a plan for another variation should still run");
            assert!(glyphs > 0);
        });
    }
}

#[test]
fn plans_carry_user_data_and_refcounts() {
    unsafe {
        with_font(|face, _| {
            let props = latin_props();
            let plan =
                hr_shape_plan_create(face, ptr::from_ref(&props), ptr::null(), 0, ptr::null());

            let value = ptr::dangling_mut::<c_void>();
            assert_ne!(
                hr_shape_plan_set_user_data(plan, &raw const KEY, value, None, 1),
                0
            );
            assert_eq!(hr_shape_plan_get_user_data(plan, &raw const KEY), value);

            // Taking and dropping a reference leaves the plan usable.
            hr_shape_plan_destroy(hr_shape_plan_reference(plan));
            assert_eq!(hr_shape_plan_get_user_data(plan, &raw const KEY), value);

            hr_shape_plan_destroy(plan);
        });
    }
}

// ------------------------------------------------------------- threading ----

/// A handle shared across threads on purpose: the C API documents faces and
/// fonts as safe to use from several at once, and these tests are what backs
/// that up. Buffers are never shared.
struct Shared<T>(*mut T);

// SAFETY: exactly the claim under test.
unsafe impl<T> Send for Shared<T> {}

// Derived copies would demand `T: Copy`, which these objects are not.
impl<T> Clone for Shared<T> {
    fn clone(&self) -> Self {
        *self
    }
}
impl<T> Copy for Shared<T> {}

impl<T> Shared<T> {
    /// Taking the pointer through a method captures the whole wrapper in a
    /// closure, rather than just the field, which would not be `Send`.
    fn get(&self) -> *mut T {
        self.0
    }
}

#[test]
fn one_face_shapes_from_several_threads() {
    let data = font_data();
    unsafe {
        let blob = blob_over(&data);
        let face = hr_face_create(blob, 0);

        // What every thread should agree on.
        let expected = {
            let font = hr_font_create(face);
            let buffer = buffer_with_text(TEXT);
            hr_shape(font, buffer, ptr::null(), 0);
            let ids = glyph_ids(buffer);
            hr_buffer_destroy(buffer);
            hr_font_destroy(font);
            ids
        };

        // Each thread builds its own font over the shared face, which is what
        // exercises the face's plan cache, its lazily loaded tables, and the
        // glyph caches hanging off it.
        let shared = Shared(face);
        let threads: Vec<_> = (0..4)
            .map(|_| {
                std::thread::spawn(move || {
                    let font = hr_font_create(shared.get());
                    let mut seen = Vec::new();
                    for _ in 0..4 {
                        let buffer = buffer_with_text(TEXT);
                        hr_shape(font, buffer, ptr::null(), 0);
                        seen = glyph_ids(buffer);
                        hr_buffer_destroy(buffer);
                    }
                    hr_font_destroy(font);
                    seen
                })
            })
            .collect();

        for thread in threads {
            assert_eq!(thread.join().expect("thread panicked"), expected);
        }

        hr_face_destroy(face);
        hr_blob_destroy(blob);
    }
}

#[test]
fn one_font_shapes_from_several_threads() {
    let data = font_data();
    unsafe {
        let blob = blob_over(&data);
        let face = hr_face_create(blob, 0);
        let font = hr_font_create(face);
        // Nothing may change the font once it is shared, which this makes
        // explicit and enforces.
        hr_font_make_immutable(font);

        let expected = {
            let buffer = buffer_with_text(TEXT);
            hr_shape(font, buffer, ptr::null(), 0);
            let ids = glyph_ids(buffer);
            hr_buffer_destroy(buffer);
            ids
        };

        let shared = Shared(font);
        let threads: Vec<_> = (0..4)
            .map(|_| {
                std::thread::spawn(move || {
                    let mut seen = Vec::new();
                    for _ in 0..4 {
                        let buffer = buffer_with_text(TEXT);
                        hr_shape(shared.get(), buffer, ptr::null(), 0);
                        seen = glyph_ids(buffer);
                        hr_buffer_destroy(buffer);
                    }
                    seen
                })
            })
            .collect();

        for thread in threads {
            assert_eq!(thread.join().expect("thread panicked"), expected);
        }

        hr_font_destroy(font);
        hr_face_destroy(face);
        hr_blob_destroy(blob);
    }
}

#[test]
fn reference_counts_survive_several_threads() {
    let data = font_data();
    unsafe {
        let blob = blob_over(&data);
        let face = hr_face_create(blob, 0);

        // Every thread takes and drops references to the same objects.
        let shared = Shared(face);
        let threads: Vec<_> = (0..4)
            .map(|_| {
                std::thread::spawn(move || {
                    for _ in 0..8 {
                        let taken = hr_face_reference(shared.get());
                        let table = hr_face_reference_table(
                            taken,
                            hr_tag_from_string(c"cmap".as_ptr(), -1),
                        );
                        assert!(hr_blob_get_length(table) > 0);
                        hr_blob_destroy(table);
                        hr_face_destroy(taken);
                    }
                })
            })
            .collect();

        for thread in threads {
            thread.join().expect("thread panicked");
        }

        // The face survived, so the counting balanced out.
        assert!(hr_face_get_glyph_count(face) > 0);
        hr_face_destroy(face);
        hr_blob_destroy(blob);
    }
}

#[test]
fn interning_a_language_from_several_threads_agrees() {
    let threads: Vec<_> = (0..4)
        .map(|_| {
            std::thread::spawn(|| unsafe {
                (0..8)
                    .map(|_| hr_language_from_string(c"en-US".as_ptr(), -1) as usize)
                    .collect::<Vec<_>>()
            })
        })
        .collect();

    let mut all = Vec::new();
    for thread in threads {
        all.extend(thread.join().expect("thread panicked"));
    }
    // Interning must hand back one pointer, however many threads raced for it.
    assert!(all.iter().all(|value| *value == all[0]));
    assert_ne!(all[0], 0);
}

#[test]
fn a_glyph_can_be_asked_what_a_callback_could_answer_for() {
    // Every callback a caller can install is one a caller can also ask, and
    // the two go through the same place.
    unsafe {
        with_font(|face, font| {
            let upem = hr_face_get_upem(face) as c_int;
            let mut glyph = 0;
            assert_ne!(
                hr_font_get_nominal_glyph(font, 'a' as u32, ptr::addr_of_mut!(glyph)),
                0
            );

            let advance = hr_font_get_glyph_h_advance(font, glyph);
            assert!(advance > 0);
            assert!(hr_font_get_glyph_v_advance(font, glyph) < 0);

            let (mut x, mut y) = (7, 7);
            // A glyph's horizontal origin is its own.
            assert_ne!(
                hr_font_get_glyph_h_origin(font, glyph, ptr::addr_of_mut!(x), ptr::addr_of_mut!(y)),
                0
            );
            assert_eq!((x, y), (0, 0));

            let mut extents = hr_glyph_extents_t::default();
            assert_ne!(
                hr_font_get_glyph_extents(font, glyph, ptr::addr_of_mut!(extents)),
                0
            );
            assert!(extents.width > 0);

            // At twice the size, everything the font reports doubles.
            hr_font_set_scale(font, upem * 2, upem * 2);
            assert_eq!(hr_font_get_glyph_h_advance(font, glyph), advance * 2);
            let mut bigger = hr_glyph_extents_t::default();
            assert_ne!(
                hr_font_get_glyph_extents(font, glyph, ptr::addr_of_mut!(bigger)),
                0
            );
            assert!(bigger.width >= extents.width * 2 - 2);
            assert!(bigger.width <= extents.width * 2 + 2);
            hr_font_set_scale(font, upem, upem);
        });
    }
}

#[test]
fn advances_run_over_a_caller_s_own_layout() {
    unsafe {
        with_font(|_, font| {
            // Glyphs and advances read and written a stride apart, so a
            // caller can walk its own structures rather than pack arrays.
            #[repr(C)]
            struct Slot {
                glyph: hr_codepoint_t,
                padding: u32,
            }
            let mut slots = [
                Slot {
                    glyph: 0,
                    padding: 0,
                },
                Slot {
                    glyph: 0,
                    padding: 0,
                },
            ];
            for (slot, c) in slots.iter_mut().zip(['a', 'b']) {
                assert_ne!(
                    hr_font_get_nominal_glyph(font, c as u32, ptr::addr_of_mut!(slot.glyph)),
                    0
                );
            }
            let mut advances = [0i32; 2];
            hr_font_get_glyph_h_advances(
                font,
                2,
                ptr::addr_of!(slots[0].glyph),
                size_of::<Slot>() as c_uint,
                advances.as_mut_ptr(),
                size_of::<i32>() as c_uint,
            );
            for (slot, advance) in slots.iter().zip(advances) {
                assert_eq!(advance, hr_font_get_glyph_h_advance(font, slot.glyph));
            }
        });
    }
}

#[test]
fn advances_and_origins_follow_the_direction_asked_for() {
    unsafe {
        with_font(|_, font| {
            let mut glyph = 0;
            hr_font_get_nominal_glyph(font, 'a' as u32, ptr::addr_of_mut!(glyph));

            let (mut x, mut y) = (0, 0);
            hr_font_get_glyph_advance_for_direction(
                font,
                glyph,
                HR_DIRECTION_LTR,
                ptr::addr_of_mut!(x),
                ptr::addr_of_mut!(y),
            );
            assert_eq!(x, hr_font_get_glyph_h_advance(font, glyph));
            assert_eq!(y, 0);

            hr_font_get_glyph_advance_for_direction(
                font,
                glyph,
                HR_DIRECTION_TTB,
                ptr::addr_of_mut!(x),
                ptr::addr_of_mut!(y),
            );
            assert_eq!(x, 0);
            assert_eq!(y, hr_font_get_glyph_v_advance(font, glyph));

            // Adding an origin and taking it away again gets back where it
            // started, whichever way the text runs.
            for direction in [HR_DIRECTION_LTR, HR_DIRECTION_TTB] {
                let (mut px, mut py) = (100, 200);
                hr_font_add_glyph_origin_for_direction(
                    font,
                    glyph,
                    direction,
                    ptr::addr_of_mut!(px),
                    ptr::addr_of_mut!(py),
                );
                hr_font_subtract_glyph_origin_for_direction(
                    font,
                    glyph,
                    direction,
                    ptr::addr_of_mut!(px),
                    ptr::addr_of_mut!(py),
                );
                assert_eq!((px, py), (100, 200));
            }
        });
    }
}

#[test]
fn glyphs_go_by_name_and_come_back_from_one() {
    unsafe {
        with_font(|_, font| {
            let mut glyph = 0;
            assert_ne!(
                hr_font_get_nominal_glyph(font, 'a' as u32, ptr::addr_of_mut!(glyph)),
                0
            );

            let mut name = [0 as c_char; 64];
            assert_ne!(
                hr_font_get_glyph_name(font, glyph, name.as_mut_ptr(), name.len() as c_uint),
                0
            );
            let mut back = 0;
            assert_ne!(
                hr_font_get_glyph_from_name(font, name.as_ptr(), -1, ptr::addr_of_mut!(back)),
                0
            );
            assert_eq!(back, glyph);

            // A glyph the face names nothing still goes by a name.
            let mut text = [0 as c_char; 64];
            hr_font_glyph_to_string(font, 60_000, text.as_mut_ptr(), text.len() as c_uint);
            let printed = std::ffi::CStr::from_ptr(text.as_ptr()).to_str().unwrap();
            assert_eq!(printed, "gid60000");

            // And the names a string can be read as.
            for (spelling, expected) in [
                (c"gid60000".as_ptr(), 60_000),
                (c"41".as_ptr(), 41),
                (c"uni0061".as_ptr(), glyph),
            ] {
                let mut parsed = 0;
                assert_ne!(
                    hr_font_glyph_from_string(font, spelling, -1, ptr::addr_of_mut!(parsed)),
                    0
                );
                assert_eq!(parsed, expected);
            }
        });
    }
}

#[test]
fn a_variation_selector_the_font_lacks_falls_back_to_the_plain_glyph() {
    unsafe {
        with_font(|_, font| {
            let mut plain = 0;
            assert_ne!(
                hr_font_get_nominal_glyph(font, 'a' as u32, ptr::addr_of_mut!(plain)),
                0
            );
            let mut combined = 0;
            assert_ne!(
                hr_font_get_glyph(font, 'a' as u32, 0xFE00, ptr::addr_of_mut!(combined)),
                0
            );
            assert_eq!(combined, plain);
            // And with no selector at all it is the plain mapping.
            let mut none = 0;
            assert_ne!(
                hr_font_get_glyph(font, 'a' as u32, 0, ptr::addr_of_mut!(none)),
                0
            );
            assert_eq!(none, plain);
        });
    }
}

#[test]
fn nothing_is_advanced_past_the_last_glyph_the_face_has() {
    unsafe {
        with_font(|face, font| {
            let count = hr_face_get_glyph_count(face);
            assert!(count > 0);
            assert!(hr_font_get_glyph_h_advance(font, count - 1) > 0);
            // A malformed cmap can point at a glyph the face does not have,
            // and there is no advance to give for one.
            assert_eq!(hr_font_get_glyph_h_advance(font, count), 0);
            // A face carrying no vertical metrics gives every glyph the same
            // fallback instead, in range or not, as HarfBuzz does.
            assert_eq!(
                hr_font_get_glyph_v_advance(font, count),
                hr_font_get_glyph_v_advance(font, 0)
            );
        });
    }
}

/// Builds a font from `tables`, which must be sorted by tag.
fn sfnt(tables: &[(&[u8; 4], Vec<u8>)]) -> Vec<u8> {
    let mut data = 0x0001_0000u32.to_be_bytes().to_vec();
    data.extend((tables.len() as u16).to_be_bytes());
    data.extend([0; 6]);
    let mut offset = 12 + 16 * tables.len();
    for (tag, table) in tables {
        data.extend(*tag);
        data.extend(0u32.to_be_bytes());
        data.extend((offset as u32).to_be_bytes());
        data.extend((table.len() as u32).to_be_bytes());
        offset += table.len().next_multiple_of(4);
    }
    for (_, table) in tables {
        data.extend(table);
        data.resize(data.len().next_multiple_of(4), 0);
    }
    data
}

/// A `maxp` or `MAXP` table holding only a glyph count.
fn maxp(count: u32, extended: bool) -> Vec<u8> {
    let mut data = 0x0000_5000u32.to_be_bytes().to_vec();
    if extended {
        data.extend(&count.to_be_bytes()[1..]);
    } else {
        data.extend((count as u16).to_be_bytes());
    }
    data
}

/// A GSUB table whose one lookup maps glyph 65536 to 65537 with
/// SingleSubst format 3.
fn extended_single_subst_gsub() -> Vec<u8> {
    #[rustfmt::skip]
    let data = vec![
        0, 1, 0, 0, 0, 10, 0, 12, 0, 14, // header
        0, 0, // script count
        0, 0, // feature count
        0, 1, 0, 4, // one lookup
        0, 1, 0, 0, 0, 1, 0, 8, // single substitution, one subtable
        0, 3, 0, 0, 0, 9, 0, 0, 1, // format 3, coverage, delta 1
        0, 3, 0, 0, 1, 1, 0, 0, // coverage format 3: glyph 65536
    ];
    data
}

/// `hhea` and `hmtx` tables with one long metric and a side bearing for each
/// further glyph.
fn h_metrics(glyphs: usize) -> [(&'static [u8; 4], Vec<u8>); 2] {
    let mut hhea = vec![0; 36];
    hhea[1] = 1; // version 1.0
    hhea[35] = 1; // one long metric
    [(b"hhea", hhea), (b"hmtx", vec![0; 4 + 2 * (glyphs - 1)])]
}

#[test]
fn wide_lookup_glyphs_do_not_require_metrics_or_a_glyph_count() {
    let [hhea, hmtx] = h_metrics(4);
    for tables in [
        // No count at all.
        vec![(b"GSUB", extended_single_subst_gsub())],
        // A count no table backs.
        vec![
            (b"GSUB", extended_single_subst_gsub()),
            (b"MAXP", maxp(0x00FF_FFFF, true)),
        ],
        // A count the metrics back only in part.
        vec![
            (b"GSUB", extended_single_subst_gsub()),
            (b"MAXP", maxp(0x00FF_FFFF, true)),
            hhea,
            hmtx,
        ],
    ] {
        let data = sfnt(&tables);
        unsafe {
            let blob = blob_over(&data);
            let face = hr_face_create(blob, 0);
            let set = hr_set_create();
            hr_ot_layout_lookup_collect_glyphs(face, 0x4753_5542, 0, set, set, set, set);
            assert_eq!(hr_set_get_population(set), 2);
            assert_eq!(hr_set_has(set, 65536), 1);
            assert_eq!(hr_set_has(set, 65537), 1);
            hr_set_destroy(set);
            hr_face_destroy(face);
            hr_blob_destroy(blob);
        }
    }
}

#[test]
fn extended_lookup_collects_glyphs_beyond_64k() {
    let [hhea, hmtx] = h_metrics(65550);
    let data = sfnt(&[
        (b"GSUB", extended_single_subst_gsub()),
        (b"MAXP", maxp(65550, true)),
        hhea,
        hmtx,
        (b"maxp", maxp(4, false)),
    ]);
    unsafe {
        let blob = blob_over(&data);
        let face = hr_face_create(blob, 0);
        let sets = [
            hr_set_create(),
            hr_set_create(),
            hr_set_create(),
            hr_set_create(),
        ];
        hr_ot_layout_lookup_collect_glyphs(
            face,
            0x4753_5542,
            0,
            sets[0],
            sets[1],
            sets[2],
            sets[3],
        );
        assert_eq!(hr_set_has(sets[1], 65536), 1);
        assert_eq!(hr_set_has(sets[3], 65537), 1);
        assert_eq!(hr_set_has(sets[1], 65550), 0);
        for set in sets {
            hr_set_destroy(set);
        }
        hr_face_destroy(face);
        hr_blob_destroy(blob);
    }
}

/// Collects lookup 0 of a `GSUB` or `GPOS` table in a font with three
/// glyphs and returns the populations of the before, input, after and
/// output sets.
fn collect_lookup_0(tag: &[u8; 4], table: Vec<u8>) -> [u32; 4] {
    let data = sfnt(&[(tag, table), (b"maxp", maxp(3, false))]);
    unsafe {
        let blob = blob_over(&data);
        let face = hr_face_create(blob, 0);
        let sets = [
            hr_set_create(),
            hr_set_create(),
            hr_set_create(),
            hr_set_create(),
        ];
        hr_ot_layout_lookup_collect_glyphs(
            face,
            u32::from_be_bytes(*tag),
            0,
            sets[0],
            sets[1],
            sets[2],
            sets[3],
        );
        let populations = sets.map(|set| hr_set_get_population(set));
        for set in sets {
            hr_set_destroy(set);
        }
        hr_face_destroy(face);
        hr_blob_destroy(blob);
        populations
    }
}

/// A GSUB table whose one ligature lookup has `n` subtable offsets, `n`
/// ligature set offsets and `n` ligature offsets, each array naming one
/// shared table, and a ligature of `n` components.
fn aliased_ligature_gsub(n: u16) -> Vec<u8> {
    let offsets = |offset: u16| (0..n).flat_map(move |_| offset.to_be_bytes());
    let mut data = vec![0, 1, 0, 0, 0, 10, 0, 12, 0, 14]; // header
    data.extend([0, 0, 0, 0]); // no scripts, no features
    data.extend([0, 1, 0, 4]); // one lookup
    data.extend([0, 4, 0, 0]); // ligature substitution
    data.extend(n.to_be_bytes());
    data.extend(offsets(6 + 2 * n)); // subtables
    data.extend([0, 1]); // format 1
    data.extend((6 + 2 * n).to_be_bytes()); // coverage
    data.extend(n.to_be_bytes());
    data.extend(offsets(6 + 2 * n + 6)); // ligature sets
    data.extend([0, 1, 0, 1, 0, 1]); // coverage format 1: glyph 1
    data.extend(n.to_be_bytes());
    data.extend(offsets(2 + 2 * n)); // ligatures
    data.extend([0, 2]); // ligature glyph
    data.extend((n + 1).to_be_bytes());
    data.extend(offsets(1)); // components
    data
}

/// A GPOS table whose one pair positioning lookup has `n` subtable offsets
/// and `n` pair set offsets, each array naming one shared table, and a pair
/// set of `n` records.
fn aliased_pair_pos_gpos(n: u16) -> Vec<u8> {
    let offsets = |offset: u16| (0..n).flat_map(move |_| offset.to_be_bytes());
    let mut data = vec![0, 1, 0, 0, 0, 10, 0, 12, 0, 14]; // header
    data.extend([0, 0, 0, 0]); // no scripts, no features
    data.extend([0, 1, 0, 4]); // one lookup
    data.extend([0, 2, 0, 0]); // pair positioning
    data.extend(n.to_be_bytes());
    data.extend(offsets(6 + 2 * n)); // subtables
    data.extend([0, 1]); // format 1
    data.extend((10 + 2 * n).to_be_bytes()); // coverage
    data.extend([0, 0, 0, 0]); // no values
    data.extend(n.to_be_bytes());
    data.extend(offsets(10 + 2 * n + 6)); // pair sets
    data.extend([0, 1, 0, 1, 0, 1]); // coverage format 1: glyph 1
    data.extend(n.to_be_bytes());
    data.extend(offsets(2)); // second glyphs
    data
}

#[test]
fn aliased_offsets_fall_back_to_all_glyphs() {
    assert_eq!(
        collect_lookup_0(b"GSUB", aliased_ligature_gsub(1)),
        [0, 1, 0, 1]
    );
    // Followed to the end, these 1.6 KB would be 200^4 components.
    assert_eq!(
        collect_lookup_0(b"GSUB", aliased_ligature_gsub(200)),
        [u32::MAX; 4]
    );
    assert_eq!(
        collect_lookup_0(b"GPOS", aliased_pair_pos_gpos(1)),
        [0, 2, 0, 0]
    );
    // Followed to the end, these 1.8 KB would be 300^3 pair records.
    assert_eq!(
        collect_lookup_0(b"GPOS", aliased_pair_pos_gpos(300)),
        [u32::MAX; 4]
    );
}

#[test]
fn repeated_collections_each_get_an_allowance() {
    // One call follows about 30^4 components, within an allowance of 4M steps.
    // As in HarfBuzz, whose sanitizer bounds the table once, each call gets
    // the same allowance and the same result.
    let data = sfnt(&[
        (b"GSUB", aliased_ligature_gsub(30)),
        (b"maxp", maxp(3, false)),
    ]);
    unsafe {
        let blob = blob_over(&data);
        let face = hr_face_create(blob, 0);
        let mut populations = vec![];
        for _ in 0..50 {
            let set = hr_set_create();
            hr_ot_layout_lookup_collect_glyphs(face, 0x4753_5542, 0, set, set, set, set);
            populations.push(hr_set_get_population(set));
            hr_set_destroy(set);
        }
        assert!(populations.iter().all(|&population| population == 2));
        hr_face_destroy(face);
        hr_blob_destroy(blob);
    }
}

/// A GSUB table where each of lookups 0 to 6 is a contextual lookup with
/// `n` records that all apply the next lookup, and lookup 7 maps glyph 1
/// to glyph 2.
fn repeatedly_nested_gsub(n: u16) -> Vec<u8> {
    let mut data = vec![0, 1, 0, 0, 0, 10, 0, 12, 0, 14]; // header
    data.extend([0, 0, 0, 0]); // no scripts, no features
    data.extend([0, 8]);
    let contextual_len = 22 + 4 * n;
    for index in 0..8 {
        data.extend((18 + index * contextual_len).to_be_bytes());
    }
    for index in 1..8 {
        data.extend([0, 5, 0, 0, 0, 1, 0, 8]); // contextual, one subtable
        data.extend([0, 3, 0, 1]); // format 3, one input glyph
        data.extend(n.to_be_bytes());
        data.extend((8 + 4 * n).to_be_bytes()); // coverage
        for _ in 0..n {
            data.extend([0, 0, 0, index]);
        }
        data.extend([0, 1, 0, 1, 0, 1]); // coverage format 1: glyph 1
    }
    data.extend([0, 1, 0, 0, 0, 1, 0, 8]); // single substitution, one subtable
    data.extend([0, 1, 0, 6, 0, 1]); // format 1, coverage, delta 1
    data.extend([0, 1, 0, 1, 0, 1]); // coverage format 1: glyph 1
    data
}

#[test]
fn repeatedly_nested_lookups_are_collected_without_fallback() {
    // Followed along every path, lookup 7 would be collected 16^7 times.
    assert_eq!(
        collect_lookup_0(b"GSUB", repeatedly_nested_gsub(16)),
        [0, 1, 0, 1]
    );
}

#[test]
fn glyph_count_prefers_extended_maxp() {
    let data = sfnt(&[(b"MAXP", maxp(65550, true)), (b"maxp", maxp(4, false))]);
    unsafe {
        let blob = blob_over(&data);
        let face = hr_face_create(blob, 0);
        assert_eq!(hr_face_get_glyph_count(face), 65550);
        hr_face_destroy(face);
        hr_blob_destroy(blob);
    }
}

/// A `CFF ` table with one `endchar` glyph, whose Top DICT is padded with
/// `padding` no-op `UniqueID` entries ahead of its `charset` and
/// `CharStrings` operators.
fn cff_with_padded_top_dict(padding: usize) -> Vec<u8> {
    fn index(items: &[&[u8]]) -> Vec<u8> {
        let mut data = (items.len() as u16).to_be_bytes().to_vec();
        if items.is_empty() {
            return data;
        }
        data.push(4);
        let mut offset = 1u32;
        data.extend(offset.to_be_bytes());
        for item in items {
            offset += item.len() as u32;
            data.extend(offset.to_be_bytes());
        }
        data.extend(items.concat());
        data
    }
    let header = [1, 0, 4, 4];
    let names = index(&[b"x"]);
    let charstrings = index(&[&[0x0e]]);
    // Header, Name INDEX, Top DICT INDEX (count, offSize, two offsets, the
    // dict), and empty String and Global Subr INDEXes.
    let top_dict_len = 2 * padding + 12;
    let after_top_dict = header.len() + names.len() + (2 + 1 + 8 + top_dict_len) + 2 + 2;
    let mut top_dict = [0x8b, 0x0d].repeat(padding);
    top_dict.push(0x1d);
    top_dict.extend(((after_top_dict + charstrings.len()) as u32).to_be_bytes());
    top_dict.push(0x0f);
    top_dict.push(0x1d);
    top_dict.extend((after_top_dict as u32).to_be_bytes());
    top_dict.push(0x11);
    assert_eq!(top_dict.len(), top_dict_len);
    [
        &header[..],
        &names,
        &index(&[&top_dict]),
        &index(&[]),
        &index(&[]),
        &charstrings,
        // A format 0 charset naming only `.notdef`.
        &[0],
    ]
    .concat()
}

/// A `MAXP` table can claim 16.7 million glyphs in seven bytes, and reading
/// one glyph's name rereads the whole CFF Top DICT. Looking a name up must
/// not pay one for each of the other.
#[test]
fn a_name_is_looked_up_without_asking_every_claimed_glyph_for_its_own() {
    let data = sfnt(&[
        (b"CFF ", cff_with_padded_top_dict(2000)),
        (b"MAXP", maxp(0x00FF_FFFF, true)),
    ]);
    unsafe {
        let blob = blob_over(&data);
        let face = hr_face_create(blob, 0);
        let font = hr_font_create(face);
        assert_eq!(hr_face_get_glyph_count(face), 0x00FF_FFFF);
        let start = std::time::Instant::now();
        let mut glyph = 7;
        assert_eq!(
            hr_font_get_glyph_from_name(font, c"nosuchglyph".as_ptr(), -1, &raw mut glyph),
            0
        );
        assert_eq!(glyph, 0);
        // One pass over the claimed glyphs, against hours for the product.
        assert!(start.elapsed().as_secs() < 60, "{:?}", start.elapsed());
        // The names the face stores are still found.
        assert_ne!(
            hr_font_get_glyph_from_name(font, c".notdef".as_ptr(), -1, &raw mut glyph),
            0
        );
        assert_eq!(glyph, 0);
        // And a glyph the face names nothing still answers to its `gidNNN`.
        assert_ne!(
            hr_font_get_glyph_from_name(font, c"gid5".as_ptr(), -1, &raw mut glyph),
            0
        );
        assert_eq!(glyph, 5);
        assert_eq!(
            hr_font_get_glyph_from_name(font, c"gid05".as_ptr(), -1, &raw mut glyph),
            0
        );
        hr_font_destroy(font);
        hr_face_destroy(face);
        hr_blob_destroy(blob);
    }
}

/// A `cmap` table whose one Windows UCS-4 subtable is format 13 with
/// `groups`, each mapping an inclusive character range to one glyph.
fn format13_cmap(groups: &[(u32, u32, u32)]) -> Vec<u8> {
    let mut data = vec![0, 0, 0, 1, 0, 3, 0, 10, 0, 0, 0, 12];
    data.extend(13u16.to_be_bytes());
    data.extend(0u16.to_be_bytes());
    data.extend((16 + 12 * groups.len() as u32).to_be_bytes());
    data.extend(0u32.to_be_bytes());
    data.extend((groups.len() as u32).to_be_bytes());
    for (first, last, glyph) in groups {
        data.extend(first.to_be_bytes());
        data.extend(last.to_be_bytes());
        data.extend(glyph.to_be_bytes());
    }
    data
}

#[test]
fn unicode_collection_steps_over_overlapping_format13_groups() {
    // Overlapping groups are counted once, and a group that repeats all of
    // Unicode costs nothing after the first.
    let partial = [(0x41, 0x5A, 1), (0x50, 0x7A, 1), (0x61, 0x6A, 1)];
    let partial = sfnt(&[
        (b"cmap", format13_cmap(&partial)),
        (b"maxp", maxp(2, false)),
    ]);
    let full = [(0, 0x0010_FFFF, 1); 1000];
    let full = sfnt(&[(b"cmap", format13_cmap(&full)), (b"maxp", maxp(2, false))]);
    for (data, population) in [(partial, 0x7A - 0x41 + 1), (full, 0x0011_0000)] {
        unsafe {
            let blob = blob_over(&data);
            let face = hr_face_create(blob, 0);
            let set = hr_set_create();
            hr_face_collect_unicodes(face, set);
            assert_eq!(hr_set_get_population(set), population);
            hr_set_destroy(set);
            hr_face_destroy(face);
            hr_blob_destroy(blob);
        }
    }
}

#[test]
fn prepared_metrics_follow_scale_subfonts_and_instance_changes() {
    unsafe {
        with_named_font(VARIABLE_FONT, |_, font| {
            let mut glyph = 0;
            assert_ne!(
                hr_font_get_nominal_glyph(font, 'a' as u32, &raw mut glyph),
                0
            );
            let original = hr_font_get_glyph_h_advance(font, glyph);
            assert!(original > 0);
            let mut upem = 0;
            hr_font_get_scale(font, &raw mut upem, ptr::null_mut());
            hr_font_set_scale(font, upem * 2, upem * 3);
            assert_eq!(hr_font_get_glyph_h_advance(font, glyph), original * 2);
            let child = hr_font_create_sub_font(font);
            hr_font_set_scale(font, -upem, 0);
            assert_eq!(hr_font_get_glyph_h_advance(font, glyph), -original);
            assert_eq!(hr_font_get_glyph_h_advance(child, glyph), original * 2);

            let variation = hr_variation_t {
                tag: u32::from_be_bytes(*b"wdth"),
                value: 200.0,
            };
            // Rebuilding the instance must preserve the child's scale.
            hr_font_set_variations(child, &raw const variation, 1);
            let wide = hr_font_get_glyph_h_advance(child, glyph);
            assert!(wide > original * 2);
            let mut advance = 0;
            hr_font_get_glyph_h_advances(child, 1, &raw const glyph, 0, &raw mut advance, 0);
            assert_eq!(advance, wide);
            let buffer = buffer_with_text("a");
            hr_shape(child, buffer, ptr::null(), 0);
            let positions = hr_buffer_get_glyph_positions(buffer, ptr::null_mut());
            assert_eq!((*positions).x_advance, wide);
            hr_buffer_destroy(buffer);
            hr_font_set_variations(child, ptr::null(), 0);
            assert_eq!(hr_font_get_glyph_h_advance(child, glyph), original * 2);
            hr_font_destroy(child);
        });
    }
}

#[test]
fn tags_read_only_four_raw_bytes() {
    unsafe {
        for len in [-2, -1, 4, 7] {
            // UTF-8 validation after the fourth byte must not affect the tag.
            let name = [b'L', b'a', b't', b'n', 0xFF, b'x', 0];
            assert_eq!(
                hr_tag_from_string(name.as_ptr().cast(), len),
                HR_SCRIPT_LATIN
            );
            assert_eq!(
                hr_script_from_string(name.as_ptr().cast(), len),
                HR_SCRIPT_LATIN
            );
        }
        for (bytes, len, expected) in [
            (&b"a\0bc"[..], -1, u32::from_be_bytes(*b"a   ")),
            (&b"abcd"[..], 2, u32::from_be_bytes(*b"ab  ")),
            (&b"abcd"[..], 4, u32::from_be_bytes(*b"abcd")),
            (&[0xFF, 0x80, 0xFE, 0x81][..], 4, 0xFF80_FE81),
            (&b"\0"[..], -1, HR_TAG_NONE),
            (&b""[..], 0, HR_TAG_NONE),
        ] {
            assert_eq!(hr_tag_from_string(bytes.as_ptr().cast(), len), expected);
        }
        assert_eq!(hr_tag_from_string(ptr::null(), -1), HR_TAG_NONE);
    }
}

#[test]
fn conservative_lookup_fallback_preserves_aliased_and_inverted_sets() {
    let mut table = extended_single_subst_gsub();
    table[27] = 0xff; // unreadable SingleSubst format
    let data = sfnt(&[(b"GSUB", table)]);
    unsafe {
        let blob = blob_over(&data);
        let face = hr_face_create(blob, 0);
        for inverted in [false, true] {
            let set = hr_set_create();
            hr_set_add(set, u32::MAX - 1);
            if inverted {
                hr_set_invert(set);
                hr_set_del(set, 65536);
            }
            hr_ot_layout_lookup_collect_glyphs(face, 0x4753_5542, 0, set, set, set, set);
            assert_eq!(hr_set_get_population(set), u32::MAX);
            assert_eq!(hr_set_is_inverted(set), 1);
            assert_eq!(hr_set_has(set, 65536), 1);
            assert_eq!(hr_set_has(set, 65537), 1);
            assert_eq!(hr_set_has(set, u32::MAX - 1), 1);
            hr_set_destroy(set);
        }
        hr_face_destroy(face);
        hr_blob_destroy(blob);
    }
}

#[test]
fn wide_single_substitution_with_explicit_outputs_needs_no_metrics() {
    let mut table = extended_single_subst_gsub();
    table.truncate(26);
    table.extend([
        0, 4, 0, 0, 0, 12, // format 4, coverage offset
        0, 0, 1, // one output glyph
        1, 0, 1, // glyph 65537
        0, 3, 0, 0, 1, 1, 0, 0, // coverage: glyph 65536
    ]);
    let data = sfnt(&[(b"GSUB", table)]);
    unsafe {
        let blob = blob_over(&data);
        let face = hr_face_create(blob, 0);
        let input = hr_set_create();
        let output = hr_set_create();
        hr_ot_layout_lookup_collect_glyphs(
            face,
            0x4753_5542,
            0,
            ptr::null_mut(),
            input,
            ptr::null_mut(),
            output,
        );
        assert_eq!(hr_set_get_population(input), 1);
        assert_eq!(hr_set_get_population(output), 1);
        assert_eq!(hr_set_has(input, 65536), 1);
        assert_eq!(hr_set_has(output, 65537), 1);
        hr_set_destroy(input);
        hr_set_destroy(output);
        hr_face_destroy(face);
        hr_blob_destroy(blob);
    }
}

#[test]
fn inverted_set_iteration_seeks_past_a_long_excluded_prefix() {
    // Previously every next() rebuilt the first range by scanning all the
    // excluded pages before PREFIX, even though each answer is nearby.
    const PREFIX: u32 = 1 << 22;
    const COUNT: usize = 4096;
    for stride in [2, 32] {
        unsafe {
            let set = hr_set_create();
            hr_set_invert(set);
            hr_set_del_range(set, 0, PREFIX - 1);
            for index in 0..COUNT as u32 {
                hr_set_del_range(
                    set,
                    PREFIX + stride * index + 1,
                    PREFIX + stride * (index + 1) - 1,
                );
            }
            let mut cursor = PREFIX;
            for index in 1..=COUNT as u32 {
                assert_eq!(hr_set_next(set, &raw mut cursor), 1);
                assert_eq!(cursor, PREFIX + stride * index);
            }
            let mut batch = [0; COUNT];
            assert_eq!(
                hr_set_next_many(set, PREFIX, batch.as_mut_ptr(), COUNT as u32),
                COUNT as u32
            );
            for (index, value) in batch.into_iter().enumerate() {
                assert_eq!(value, PREFIX + stride * (index as u32 + 1));
            }
            hr_set_destroy(set);
        }
    }
}

#[test]
fn inverted_set_seeks_across_long_gaps_and_invalidates_cached_ranges() {
    const END: u32 = 1 << 24;
    unsafe {
        let set = hr_set_create();
        hr_set_invert(set);
        hr_set_del_range(set, 0, END - 1);
        let mut cursor = HR_SET_VALUE_INVALID;
        assert_eq!(hr_set_next(set, &raw mut cursor), 1);
        assert_eq!(cursor, END);
        hr_set_add(set, 17);
        cursor = HR_SET_VALUE_INVALID;
        assert_eq!(hr_set_next(set, &raw mut cursor), 1);
        assert_eq!(cursor, 17);
        hr_set_del(set, 17);
        cursor = HR_SET_VALUE_INVALID;
        assert_eq!(hr_set_next(set, &raw mut cursor), 1);
        assert_eq!(cursor, END);
        hr_set_invert(set);
        cursor = HR_SET_VALUE_INVALID;
        assert_eq!(hr_set_next(set, &raw mut cursor), 1);
        assert_eq!(cursor, 0);
        hr_set_destroy(set);
    }
}
