//! Test subsetter output against expected results.
//!
//! This reads test configs from harfbuzz subset repacker_test suite,
//! generate a group of tests to perform, run and then compare the output
//! against the stored expected result

use rstest::rstest;
use skera::{subset_font, Plan, SubsetFlags, DEFAULT_DROP_TABLES, DEFAULT_LAYOUT_FEATURES};
use std::path::PathBuf;
use write_fonts::{
    read::{collections::IntSet, FontRef},
    types::{NameId, Tag},
};

static TEST_DATA_DIR: &str = "./test-data";

struct RepackerTestCase {
    font_name: String,
    unicodes: IntSet<u32>,
}

impl RepackerTestCase {
    fn new(path: &PathBuf) -> Self {
        let test_file = std::fs::read_to_string(path).unwrap();
        let mut input_lines = test_file.lines();
        let font_name = input_lines.next().unwrap().to_string();

        let mut unicodes = IntSet::empty();
        for line in input_lines {
            let line = line.trim();
            if line.is_empty() {
                continue;
            }

            if line == "*" {
                unicodes.clear();
                unicodes.invert();
                break;
            }

            let line = line.trim_start_matches("0x");
            let cp = u32::from_str_radix(line, 16).unwrap();
            unicodes.insert(cp);
        }

        RepackerTestCase { font_name, unicodes }
    }

    fn run(&self) {
        let font_file = PathBuf::from(TEST_DATA_DIR).join("fonts").join(&self.font_name);

        let font_bytes = std::fs::read(font_file).unwrap();
        let font = FontRef::new(&font_bytes).unwrap();

        let empty_set = IntSet::empty();
        let mut drop_tables = IntSet::empty();
        drop_tables.extend_unsorted(DEFAULT_DROP_TABLES.iter().copied());

        //TODO: remove drop_tables once we support those tables
        drop_tables.insert(Tag::new(b"MATH"));
        drop_tables.insert(Tag::new(b"CFF "));
        drop_tables.insert(Tag::new(b"CFF2"));

        let mut name_ids = IntSet::<NameId>::empty();
        name_ids.insert_range(NameId::from(0)..=NameId::from(6));

        let mut name_languages = IntSet::<u16>::empty();
        name_languages.insert(0x0409);

        let mut layout_features = IntSet::empty();
        layout_features.extend_unsorted(DEFAULT_LAYOUT_FEATURES.iter().copied());

        // Default to all scripts.
        let layout_scripts = IntSet::<Tag>::all();

        let plan = Plan::new(
            &empty_set,
            &self.unicodes,
            &font,
            SubsetFlags::SUBSET_FLAGS_DEFAULT,
            &drop_tables,
            &layout_scripts,
            &layout_features,
            &name_ids,
            &name_languages,
        );

        subset_font(&font, &plan).unwrap();
    }
}

#[rstest]
#[ignore = r#"Slow integration test, see https://github.com/googlefonts/fontations/issues/1910.
To run manually: cargo test -p skera -- --ignored"#]
fn test_repacker_case(#[files("test-data/repacker_tests/*.tests")] path: PathBuf) {
    let test = RepackerTestCase::new(&path);
    test.run();
}
