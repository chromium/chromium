//! Keep-everything presets preserve selections normally excluded by default.
use skera::{subset_font, Plan};
use write_fonts::{
    from_obj::ToOwnedTable,
    read::{types::NameId, FontRef, TableProvider},
    tables::name::{Name, NameRecord},
    types::Tag,
    FontBuilder,
};

fn source() -> Vec<u8> {
    let font = FontRef::new(include_bytes!(
        "../test-data/fonts/RobotoFlex-Variable.ABC.ttf"
    ))
    .unwrap();
    let mut name: Name = font.name().unwrap().to_owned_table();
    name.name_record.extend([
        NameRecord::new(
            1,
            0,
            0,
            NameId::new(500),
            "Legacy family".to_string().into(),
        ),
        NameRecord::new(
            3,
            1,
            0x40c,
            NameId::new(501),
            "Nom français".to_string().into(),
        ),
    ]);
    name.name_record.sort();
    let mut builder = FontBuilder::new();
    for record in font.table_directory().table_records() {
        builder.add_raw(record.tag(), font.data_for_tag(record.tag()).unwrap());
    }
    builder.add_table(&name).unwrap();
    builder
        .add_table(&write_fonts::tables::post::Post::new_v2([
            ".notdef", "custom.A", "custom.B", "custom.C",
        ]))
        .unwrap();
    builder.add_raw(Tag::new(b"TEST"), b"opaque table");
    builder.add_raw(Tag::new(b"DSIG"), &[0, 0, 0, 1, 0, 0, 0, 0]);
    builder.build()
}

fn assert_kept(source: &[u8], output: &[u8]) {
    let source = FontRef::new(source).unwrap();
    let output = FontRef::new(output).unwrap();
    assert_eq!(
        source.maxp().unwrap().num_glyphs(),
        output.maxp().unwrap().num_glyphs()
    );
    for tag in [b"TEST", b"DSIG"] {
        assert_eq!(
            source.data_for_tag(Tag::new(tag)).unwrap().as_bytes(),
            output.data_for_tag(Tag::new(tag)).unwrap().as_bytes()
        );
    }
    for id in [500, 501] {
        assert!(output
            .name()
            .unwrap()
            .name_record()
            .iter()
            .any(|r| r.name_id() == NameId::new(id)));
    }
    let post = output.post().unwrap();
    assert_eq!(
        post.glyph_name(write_fonts::types::GlyphId16::new(1))
            .unwrap()
            .to_string(),
        "custom.A"
    );
    assert_eq!(
        source
            .gsub()
            .unwrap()
            .feature_list()
            .unwrap()
            .feature_count(),
        output
            .gsub()
            .unwrap()
            .feature_list()
            .unwrap()
            .feature_count()
    );
    assert_eq!(
        source
            .gpos()
            .unwrap()
            .feature_list()
            .unwrap()
            .feature_count(),
        output
            .gpos()
            .unwrap()
            .feature_list()
            .unwrap()
            .feature_count()
    );
    assert_eq!(
        source.os2().unwrap().ul_unicode_range_1(),
        output.os2().unwrap().ul_unicode_range_1()
    );
}

#[test]
fn library_preset_retains_names_layout_items_and_opaque_tables() {
    let source = source();
    let font = FontRef::new(&source).unwrap();
    let plan = Plan::keep_everything(&font);
    let output = subset_font(&font, &plan).unwrap();
    assert!(plan.old_to_new_glyph_mapping().all(|(old, new)| old == new));
    assert_kept(&source, &output);
}

#[cfg(feature = "cli")]
#[test]
fn cli_preset_matches_library_and_supports_new_subset_options() {
    use std::process::Command;
    let source = source();
    let temp = tempfile::tempdir().unwrap();
    let input = temp.path().join("source.ttf");
    let output = temp.path().join("subset.ttf");
    std::fs::write(&input, &source).unwrap();
    let run = |options: &[&str]| {
        let result = Command::new(env!("CARGO_BIN_EXE_skera"))
            .arg("--path")
            .arg(&input)
            .arg("--output-file")
            .arg(&output)
            .arg("--keep-everything")
            .args(options)
            .output()
            .unwrap();
        assert!(
            result.status.success(),
            "{}",
            String::from_utf8_lossy(&result.stderr)
        );
        std::fs::read(&output).unwrap()
    };
    let bytes = run(&[]);
    assert_kept(&source, &bytes);
    let font = FontRef::new(&source).unwrap();
    assert_eq!(
        bytes,
        subset_font(&font, &Plan::keep_everything(&font)).unwrap()
    );

    let bytes = run(&[
        "--gids-=*",
        "--unicodes=41",
        "--layout-features=",
        "--name-IDs=1",
        "--name-languages=1033",
        "--drop-tables+=TEST,DSIG",
        "--no-hinting",
    ]);
    let font = FontRef::new(&bytes).unwrap();
    assert_eq!(font.maxp().unwrap().num_glyphs(), 2);
    assert!(font.data_for_tag(Tag::new(b"TEST")).is_none());
    assert!(font.data_for_tag(Tag::new(b"DSIG")).is_none());
    let name = font.name().unwrap();
    assert!(name.name_record().iter().all(|r| r.language_id() == 0x409));
    assert!(name
        .name_record()
        .iter()
        .any(|r| r.name_id() == NameId::new(1)));
    for id in [500, 501] {
        assert!(!name
            .name_record()
            .iter()
            .any(|r| r.name_id() == NameId::new(id)));
    }
    // Names needed by retained axes are still included by name closure.
    for axis in font.fvar().unwrap().axes().unwrap() {
        assert!(name
            .name_record()
            .iter()
            .any(|r| r.name_id() == axis.axis_name_id()));
    }
    assert!(font.data_for_tag(Tag::new(b"fpgm")).is_none());

    // A custom mapping participates in command-line order with the newer list operations.
    let bytes = run(&["--gids=", "--unicodes=", "--gid-map=1:3"]);
    assert_eq!(
        FontRef::new(&bytes).unwrap().maxp().unwrap().num_glyphs(),
        4
    );

    // New boolean subset features remain additive to the preset.
    let bytes = run(&[
        "--retain-gids",
        "--retain-num-glyphs",
        "--iftb-requirements",
        "--optimize",
    ]);
    assert_kept(&source, &bytes);
}
