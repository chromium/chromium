//! Test subsetter output against expected results.
//!
//! This reads test configs from harfbuzz subset test suite,
//! generate a group of tests to perform, run and then compare the output against the stored expected result
//!
//! To generate the expected output files, pass GEN_EXPECTED_OUTPUTS=1 as an
//! environment variable.

use rayon::prelude::*;
use rstest::rstest;
use skera::{
    parse_unicodes, subset_font, Plan, SubsetFlags, DEFAULT_DROP_TABLES, DEFAULT_LAYOUT_FEATURES,
};
use std::fmt::Write;
use std::fs;
use std::iter::Peekable;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use tempfile::TempDir;
use write_fonts::{
    read::{
        collections::{int_set::Domain, IntSet},
        FontRef,
    },
    types::{GlyphId, NameId, Tag},
};

static TEST_DATA_DIR: &str = "./test-data";
static GEN_EXPECTED_OUTPUTS_VAR: &str = "GEN_EXPECTED_OUTPUTS";

#[derive(Default)]
struct SubsetTestCase {
    /// name of directory that stores expected files
    expected_dir: String,

    /// Path to font files used for testing
    fonts: Vec<String>,

    /// command line args for subsetter
    profiles: Vec<String>,

    /// subset codepoints to retain
    subsets: Vec<String>,

    //command line args for instancer
    //TODO: add support for instancing
    //instances: Vec<String>,
    ///compare against fonttools or not
    fonttool_options: bool,

    ///IUP optimize or not
    iup_optimize: Vec<bool>,
}

#[derive(Default)]
struct TestCaseParser {
    case: SubsetTestCase,
}

/// A helper to iterate over non-empty lines of input
struct LinesIter<'a> {
    iter: Peekable<std::str::Lines<'a>>,
}

impl<'a> LinesIter<'a> {
    fn new(s: &'a str) -> Self {
        let mut this = Self {
            iter: s.lines().peekable(),
        };
        this.skip_empty_lines();
        this
    }

    fn next(&mut self) -> Option<&'a str> {
        let next = self.iter.next();
        self.skip_empty_lines();
        next
    }

    fn skip_empty_lines(&mut self) {
        while let Some(next) = self.iter.peek().copied() {
            let next = next.trim();
            if !(next.starts_with('#') || next.is_empty()) {
                break;
            }
            self.iter.next();
        }
    }

    fn is_end(&mut self) -> bool {
        matches!(
            self.iter.peek().copied(),
            None | Some(
                "FONTS:" | "PROFILES:" | "SUBSETS:" | "INSTANCES:" | "OPTIONS:" | "IUP_OPTIONS:",
            )
        )
    }
}

impl TestCaseParser {
    fn new() -> Self {
        TestCaseParser::default()
    }

    fn parse(mut self, path: &Path) -> SubsetTestCase {
        self.case.expected_dir = String::from(path.file_stem().unwrap().to_str().unwrap());

        let input_text = std::fs::read_to_string(path).unwrap();
        let mut lines = LinesIter::new(&input_text);
        while let Some(line) = lines.next() {
            match line {
                "FONTS:" => self.parse_fonts(&mut lines),
                "PROFILES:" => self.parse_profiles(&mut lines),
                "SUBSETS:" => self.parse_subsets(&mut lines),
                "INSTANCES:" => self.parse_instances(&mut lines),
                "OPTIONS:" => self.parse_fonttools_options(&mut lines),
                "IUP_OPTIONS:" => self.parse_iup_options(&mut lines),
                other => panic!("unexpected heading '{other}'"),
            }
        }
        self.case
    }

    fn parse_fonts(&mut self, lines: &mut LinesIter) {
        while !lines.is_end() {
            if let Some(next) = lines.next() {
                self.case.fonts.push(next.trim().to_owned());
            }
        }
    }

    fn parse_profiles(&mut self, lines: &mut LinesIter) {
        while !lines.is_end() {
            if let Some(next) = lines.next() {
                //hard coded profiles that're not supported yet
                //TODO: remove once we support those options
                let line = next.trim();
                match line {
                    "downgrade-cff2.txt"
                    | "desubroutinize.txt"
                    | "iftb_requirements.txt"
                    | "glyph_map_roboto.txt" => continue,
                    _ => self.case.profiles.push(line.to_owned()),
                }
            }
        }
    }

    fn parse_subsets(&mut self, lines: &mut LinesIter) {
        while !lines.is_end() {
            if let Some(next) = lines.next() {
                match next {
                    "*" => self.case.subsets.push(next.to_owned()),
                    "no-unicodes" => self.case.subsets.push(String::new()),
                    // unicode string
                    next if next.starts_with("U+") => {
                        self.case.subsets.push(strip_unicode_prefix(next))
                    }
                    //convert text string to unicode string
                    _ => self.case.subsets.push(convert_text_to_unicodes(next)),
                }
            }
        }
    }

    fn parse_instances(&mut self, lines: &mut LinesIter) {
        //TODO: add support for instancing
        while !lines.is_end() {
            lines.next();
        }
    }

    fn parse_fonttools_options(&mut self, lines: &mut LinesIter) {
        while !lines.is_end() {
            if let Some(next) = lines.next() {
                match next {
                    "no_fonttools" => {
                        self.case.fonttool_options = true;
                    }
                    _ => {
                        continue;
                    }
                }
            }
        }
    }

    fn parse_iup_options(&mut self, lines: &mut LinesIter) {
        while !lines.is_end() {
            if let Some(next) = lines.next() {
                match next {
                    "Yes" => {
                        self.case.iup_optimize.push(true);
                    }
                    "No" => {
                        self.case.iup_optimize.push(false);
                    }
                    _ => {
                        continue;
                    }
                }
            }
        }
    }
}

#[allow(clippy::too_many_arguments)]
// TODO: support more options:
// --downgrade-cff2,--desubroutinize, --iftb-requirements, --retian-num-glyphs, --gid-map
fn parse_profile_options(
    profile: &str,
    subset_flags: &mut SubsetFlags,
    name_ids: &mut IntSet<NameId>,
    gids: &mut IntSet<GlyphId>,
    unicodes: &mut IntSet<u32>,
    layout_features: &mut IntSet<Tag>,
    layout_scripts: &mut IntSet<Tag>,
    drop_tables: &mut IntSet<Tag>,
    name_languages: &mut IntSet<u16>,
) {
    let file_path = Path::new(TEST_DATA_DIR).join("profiles").join(profile);
    let input = std::fs::read_to_string(file_path).unwrap();

    for line in input.lines() {
        let line = line.trim();
        let split_line = line.split_once("=");
        match split_line {
            None => match line {
                "--desubroutinize" => *subset_flags |= SubsetFlags::SUBSET_FLAGS_DESUBROUTINIZE,
                "--retain-gids" => *subset_flags |= SubsetFlags::SUBSET_FLAGS_RETAIN_GIDS,
                "--no-hinting" => *subset_flags |= SubsetFlags::SUBSET_FLAGS_NO_HINTING,
                "--glyph-names" => *subset_flags |= SubsetFlags::SUBSET_FLAGS_GLYPH_NAMES,
                "--name-legacy" => *subset_flags |= SubsetFlags::SUBSET_FLAGS_NAME_LEGACY,
                "--no-layout-closure" => {
                    *subset_flags |= SubsetFlags::SUBSET_FLAGS_NO_LAYOUT_CLOSURE
                }
                "--no-prune-unicode-ranges" => {
                    *subset_flags |= SubsetFlags::SUBSET_FLAGS_NO_PRUNE_UNICODE_RANGES
                }
                "--notdef-outline" => *subset_flags |= SubsetFlags::SUBSET_FLAGS_NOTDEF_OUTLINE,
                "--no-bidi-closure" => *subset_flags |= SubsetFlags::SUBSET_FLAGS_NO_BIDI_CLOSURE,
                _ => continue,
            },
            Some((option_str, list)) => {
                let (action, option) = if option_str.ends_with("+") {
                    (Some(true), option_str.strip_suffix("+").unwrap())
                } else if option_str.ends_with("-") {
                    (Some(false), option_str.strip_suffix("-").unwrap())
                } else {
                    (None, option_str)
                };
                match option {
                    "--name-IDs" => parse_list(list, name_ids, action, |s| {
                        NameId::from(s.parse::<u16>().unwrap())
                    }),
                    "--gids" => parse_list(list, gids, action, |s| {
                        GlyphId::from(s.parse::<u32>().unwrap())
                    }),
                    "--unicodes" => parse_list(list, unicodes, action, |s| {
                        u32::from_str_radix(s, 16).unwrap()
                    }),
                    "--layout-features" => parse_tag_list(list, layout_features, action),
                    "--layout-scripts" => parse_tag_list(list, layout_scripts, action),
                    "--drop-tables" => parse_tag_list(list, drop_tables, action),
                    "--name-languages" => {
                        parse_list(list, name_languages, action, |s| s.parse::<u16>().unwrap())
                    }
                    _ => continue,
                }
            }
        }
    }
}

fn parse_list<T: Domain>(
    list: &str,
    set: &mut IntSet<T>,
    action: Option<bool>,
    map_fn: fn(&str) -> T,
) {
    if list == "*" {
        set.clear();
        if action == Some(false) {
            return;
        }
        set.invert();
        return;
    }

    let input = list.split(",").map(map_fn);
    match action {
        Some(true) => set.extend(input),
        Some(false) => set.remove_all(input),
        None => {
            set.clear();
            set.extend(input);
        }
    }
}

#[inline]
fn parse_tag_list(list: &str, tag_set: &mut IntSet<Tag>, action: Option<bool>) {
    parse_list::<Tag>(list, tag_set, action, |s| {
        Tag::new_checked(s.as_bytes()).unwrap()
    });
}

impl SubsetTestCase {
    fn new(path: &Path) -> Self {
        let parser = TestCaseParser::new();
        parser.parse(path)
    }

    fn run(&self) {
        let output_temp_dir = TempDir::new_in(".").unwrap();
        let output_dir = output_temp_dir.path();
        self.fonts.par_iter().for_each(|font| {
            self.profiles.par_iter().for_each(|profile| {
                self.subsets.par_iter().for_each(|subset| {
                    //TODO: add support for instances/iup_options
                    self.run_one_test(font, subset, profile, output_dir);
                })
            })
        })
    }

    fn gen_expected_output(&self) {
        let output_temp_dir = TempDir::new_in(".").unwrap();
        let output_dir = output_temp_dir.path();
        for font in &self.fonts {
            for profile in &self.profiles {
                for subset in &self.subsets {
                    //TODO: add support for instances/iup_options
                    self.gen_expected_output_for_one_test(font, subset, profile, output_dir);
                }
            }
        }
        let expected_dir = Path::new(TEST_DATA_DIR)
            .join("expected")
            .join(&self.expected_dir);
        fs::rename(output_dir, expected_dir).unwrap();
    }

    fn run_one_test(&self, font: &str, subset: &str, profile: &str, output_dir: &Path) {
        let subset_font_name = gen_subset_font_name(font, subset, profile);
        let output_file = output_dir.join(&subset_font_name);
        gen_subset_font_file(font, subset, profile, &output_file);

        let expected_file = Path::new(TEST_DATA_DIR)
            .join("expected")
            .join(&self.expected_dir)
            .join(&subset_font_name);
        compare_with_expected(output_dir, &output_file, &expected_file);
    }

    fn gen_expected_output_for_one_test(
        &self,
        font: &str,
        subset: &str,
        profile: &str,
        output_dir: &Path,
    ) {
        let subset_font_name = gen_subset_font_name(font, subset, profile);
        let output_file = output_dir.join(&subset_font_name);
        gen_subset_font_file(font, subset, profile, &output_file);

        assert_has_ttx_exec();
        let mut expected_file_name = String::from(&subset_font_name);
        expected_file_name.push_str(".expected");
        let expected_file = output_dir.join(expected_file_name);

        let mut args = Vec::new();
        args.push(String::from("subset"));

        let org_font_file = Path::new(TEST_DATA_DIR).join("fonts").join(font);
        args.push(String::from(org_font_file.as_os_str().to_str().unwrap()));

        if !subset.is_empty() {
            let mut unicodes_option = String::from("--unicodes=");
            unicodes_option.push_str(subset);
            args.push(unicodes_option);
        }

        args.push(String::from("--drop-tables+=DSIG,BASE,MATH,CFF,CFF2"));
        args.push(String::from("--no-harfbuzz-repacker"));

        let mut output_option = String::from("--output-file=");
        output_option.push_str(expected_file.to_str().unwrap());
        args.push(output_option);

        let profile_path = Path::new(TEST_DATA_DIR).join("profiles").join(profile);
        let profile_input = std::fs::read_to_string(profile_path).unwrap();
        //TODO: add --desubroutinize back when it's supported
        for line in profile_input.lines() {
            let line = line.trim();
            if line.starts_with("--downgrade-cff2")
                || line.starts_with("--iftb-requirements")
                || line.starts_with("--retian-num-glyphs")
                || line.starts_with("--gid-map")
                || line.starts_with("--desubroutinize")
            {
                continue;
            }
            args.push(String::from(line));
        }

        // TODO: support pruning codepage ranges
        args.push(String::from("--no-prune-codepage-ranges"));

        Command::new("fonttools")
            .args(args.clone())
            .stdout(Stdio::null())
            .status()
            .map(|s| s.success())
            .expect("fonttools failed to subset {org_font_file}");

        let expected_ttx = expected_file.with_extension("ttx");
        Command::new("ttx")
            .arg("-o")
            .arg(&expected_ttx)
            .arg(&expected_file)
            .stdout(Stdio::null())
            .status()
            .map(|s| s.success())
            .expect("ttx failed to parse the expected file {expected_file}");

        let output_ttx = output_file.with_extension("ttx");
        Command::new("ttx")
            .arg("-o")
            .arg(&output_ttx)
            .arg(output_file)
            .stdout(Stdio::null())
            .status()
            .map(|s| s.success())
            .expect("ttx failed to parse the output file {output_file}");

        let diff = diff_ttx(&expected_ttx, &output_ttx);
        if !diff.is_empty() {
            panic!("fonttool args={args:?}, file={expected_file:?}\n{diff}\nError: {expected_file:?} ttx for fonttools and skera does not match.");
        }
        fs::remove_file(expected_file).unwrap();
        fs::remove_file(expected_ttx).unwrap();
        fs::remove_file(output_ttx).unwrap();
    }
}

fn gen_subset_font_file(font_file: &str, subset: &str, profile: &str, output_file: &PathBuf) {
    let org_font_file = PathBuf::from(TEST_DATA_DIR).join("fonts").join(font_file);
    let org_font_bytes = std::fs::read(org_font_file).unwrap();
    let font = FontRef::new(&org_font_bytes).unwrap();

    let mut unicodes = parse_unicodes(subset).unwrap();
    let mut drop_tables = IntSet::empty();
    drop_tables.extend_unsorted(DEFAULT_DROP_TABLES.iter().copied());

    //TODO: remove drop_tables once we support those tables
    drop_tables.insert(Tag::new(b"BASE"));
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
    let mut layout_scripts = IntSet::<Tag>::all();

    let mut subset_flags = SubsetFlags::SUBSET_FLAGS_DEFAULT;
    let mut gids = IntSet::empty();
    parse_profile_options(
        profile,
        &mut subset_flags,
        &mut name_ids,
        &mut gids,
        &mut unicodes,
        &mut layout_features,
        &mut layout_scripts,
        &mut drop_tables,
        &mut name_languages,
    );
    let plan = Plan::new(
        &gids,
        &unicodes,
        &font,
        subset_flags,
        &drop_tables,
        &layout_scripts,
        &layout_features,
        &name_ids,
        &name_languages,
    );

    let subset_output = subset_font(&font, &plan).unwrap();
    std::fs::write(output_file, subset_output).unwrap();
    //TODO: re-enable OTS check
    //assert_has_ots_exec();
    //assert_check_ots(&output_file);
}

fn convert_text_to_unicodes(text: &str) -> String {
    let mut out = String::new();
    for c in text.trim().chars() {
        let c = c as u32;
        if out.is_empty() {
            write!(&mut out, "{c:X}").unwrap();
        } else {
            write!(&mut out, ",{c:X}").unwrap();
        }
    }
    out
}

fn strip_unicode_prefix(text: &str) -> String {
    text.replace("U+", "")
}

fn gen_subset_font_name(font: &str, subset: &str, profile: &str) -> String {
    let subset_name = match subset {
        "*" => "all",
        "" => "no-unicodes",
        _ => subset,
    };

    let (font_base_name, font_extension) = font.rsplit_once('.').unwrap();
    //TODO: add instances later
    let (profile_name, _profile_extension) = profile.rsplit_once('.').unwrap();
    let subset_font_name =
        format!("{font_base_name}.{profile_name}.{subset_name}.{font_extension}");
    subset_font_name
}
/// Assert that we can find the `ttx` executable
#[allow(dead_code)]
fn assert_has_ttx_exec() {
    assert!(
        Command::new("ttx")
            .arg("--version")
            .stdout(Stdio::null())
            .status()
            .map(|s| s.success())
            .unwrap_or(false),
        "\nmissing `ttx` executable. Install it with `pip install fonttools`."
    )
}

/// Assert that we can find the `ots-sanitze` executable
#[allow(dead_code)]
fn assert_has_ots_exec() {
    assert!(
        Command::new("ots-sanitize")
            .arg("--version")
            .stdout(Stdio::null())
            .status()
            .map(|s| s.success())
            .unwrap_or(false),
        "\nmissing `ots-sanitize` executable."
    )
}

#[allow(dead_code)]
fn assert_check_ots(file: &Path) {
    let file_name_str = file.to_str().unwrap();
    assert!(
        Command::new("ots-sanitize")
            .arg(file_name_str)
            .stdout(Stdio::null())
            .status()
            .map(|s| s.success())
            .unwrap_or(false),
        "\nOTS failure on {file_name_str}!"
    )
}

fn write_lines(f: &mut impl Write, lines: &[&str], line_num: usize, prefix: char) {
    writeln!(f, "L{line_num}").unwrap();
    for line in lines {
        writeln!(f, "{prefix}  {line}").unwrap();
    }
}

fn diff_ttx(expected_ttx: &Path, output_ttx: &Path) -> String {
    let expected = fs::read_to_string(expected_ttx).unwrap();
    let output = fs::read_to_string(output_ttx).unwrap();
    let lines = diff::lines(&expected, &output);

    let mut result = String::new();
    let mut temp: Vec<&str> = Vec::new();
    let mut left_or_right = None;
    let mut section_start = 0;

    for (i, line) in lines.iter().enumerate() {
        match line {
            diff::Result::Left(line) => {
                if line.contains("checkSumAdjustment value=") {
                    continue;
                }
                if left_or_right == Some('R') {
                    write_lines(&mut result, &temp, section_start, '<');
                    temp.clear();
                } else if left_or_right != Some('L') {
                    section_start = i;
                }
                temp.push(line);
                left_or_right = Some('L');
            }
            diff::Result::Right(line) => {
                if line.contains("checkSumAdjustment value=") {
                    continue;
                }
                if left_or_right == Some('L') {
                    write_lines(&mut result, &temp, section_start, '>');
                    temp.clear();
                } else if left_or_right != Some('R') {
                    section_start = i;
                }
                temp.push(line);
                left_or_right = Some('R');
            }
            diff::Result::Both { .. } => {
                match left_or_right.take() {
                    Some('R') => write_lines(&mut result, &temp, section_start, '<'),
                    Some('L') => write_lines(&mut result, &temp, section_start, '>'),
                    _ => (),
                }
                temp.clear();
            }
        }
    }
    match left_or_right.take() {
        Some('R') => write_lines(&mut result, &temp, section_start, '<'),
        Some('L') => write_lines(&mut result, &temp, section_start, '>'),
        _ => (),
    }
    result
}

fn compare_with_expected(output_dir: &Path, output_file: &Path, expected_file: &Path) {
    let expected = fs::read(expected_file).unwrap();
    let output = fs::read(output_file).unwrap();
    if expected != output {
        // uncomment to overwrite expected file with output for updating integration tests
        // fs::write(expected_file, &output).unwrap(); return;
        assert_has_ttx_exec();
        let expected_file_prefix = expected_file.file_stem().unwrap().to_str().unwrap();
        let expected_ttx = format!("{expected_file_prefix}.expected.ttx");
        let expected_ttx = output_dir.join(expected_ttx);
        Command::new("ttx")
            .arg("-o")
            .arg(&expected_ttx)
            .arg(expected_file)
            .stdout(Stdio::null())
            .status()
            .map(|s| s.success())
            .expect("ttx failed to parse the expected file {expected_file}");

        let output_ttx = output_file.with_extension("ttx");
        Command::new("ttx")
            .arg("-o")
            .arg(&output_ttx)
            .arg(output_file)
            .stdout(Stdio::null())
            .status()
            .map(|s| s.success())
            .expect("ttx failed to parse the output file {output_file}");

        let ttx_diff = diff_ttx(&expected_ttx, &output_ttx);
        //TODO: print more info about the test state
        panic!(
            "failed on {expected_file:?}\n{ttx_diff}\nError: ttx for expected and actual does not match."
        );
    }
}

const LAYOUT_NOTONASTALIQURDU_TEST: &str = "tests/layout.notonastaliqurdu.tests";

#[rstest]
fn test_subset_case(#[files("test-data/tests/*.tests")] path: PathBuf) {
    // Tested in test_subset_layout_notonastaliqurdu.
    if path.ends_with(LAYOUT_NOTONASTALIQURDU_TEST) {
        return;
    }
    let test = SubsetTestCase::new(&path);
    match std::env::var(GEN_EXPECTED_OUTPUTS_VAR) {
        Ok(_val) => test.gen_expected_output(),
        Err(_e) => test.run(),
    }
}

#[test]
#[ignore = r#"Slow integration test, see https://github.com/googlefonts/fontations/issues/1910.
To run manually: cargo test -p skera -- --ignored"#]
fn test_subset_layout_notonastaliqurdu() {
    let path = Path::new(TEST_DATA_DIR).join(LAYOUT_NOTONASTALIQURDU_TEST);
    let test = SubsetTestCase::new(&path);
    match std::env::var(GEN_EXPECTED_OUTPUTS_VAR) {
        Ok(_val) => test.gen_expected_output(),
        Err(_e) => test.run(),
    }
}

#[test]
fn parse_test() {
    let test_data_dir = Path::new(TEST_DATA_DIR);
    assert!(test_data_dir.exists());
    let test_file = test_data_dir.join("tests/basics.tests");
    let subset_test = SubsetTestCase::new(&test_file);

    assert_eq!(subset_test.fonts.len(), 2);
    assert_eq!(subset_test.fonts[0], "Roboto-Regular.abc.ttf");
    assert_eq!(subset_test.profiles.len(), 14);
    assert_eq!(subset_test.profiles[0], String::from("default.txt"));

    // parse default.txt: all empty
    let (
        mut subset_flag,
        mut name_ids,
        mut gids,
        mut unicodes,
        mut layout_features,
        mut layout_scripts,
        mut drop_tables,
        mut name_languages,
    ) = (
        SubsetFlags::SUBSET_FLAGS_DEFAULT,
        IntSet::empty(),
        IntSet::empty(),
        IntSet::empty(),
        IntSet::empty(),
        IntSet::empty(),
        IntSet::empty(),
        IntSet::empty(),
    );

    parse_profile_options(
        &subset_test.profiles[0],
        &mut subset_flag,
        &mut name_ids,
        &mut gids,
        &mut unicodes,
        &mut layout_features,
        &mut layout_scripts,
        &mut drop_tables,
        &mut name_languages,
    );
    assert_eq!(subset_flag, SubsetFlags::SUBSET_FLAGS_DEFAULT);
    assert!(name_ids.is_empty());
    assert!(unicodes.is_empty());
    assert!(name_languages.is_empty());
    assert!(gids.is_empty());
    assert!(layout_features.is_empty());
    assert!(layout_scripts.is_empty());
    assert!(drop_tables.is_empty());

    // parse drop-hints
    assert_eq!(subset_test.profiles[1], String::from("drop-hints.txt"));
    parse_profile_options(
        &subset_test.profiles[1],
        &mut subset_flag,
        &mut name_ids,
        &mut gids,
        &mut unicodes,
        &mut layout_features,
        &mut layout_scripts,
        &mut drop_tables,
        &mut name_languages,
    );
    assert_eq!(subset_flag, SubsetFlags::SUBSET_FLAGS_NO_HINTING);
    assert!(name_ids.is_empty());
    assert!(unicodes.is_empty());
    assert!(name_languages.is_empty());
    assert!(gids.is_empty());
    assert!(layout_features.is_empty());
    assert!(layout_scripts.is_empty());
    assert!(drop_tables.is_empty());

    // parse drop-hints-retain-gids
    assert_eq!(
        subset_test.profiles[2],
        String::from("drop-hints-retain-gids.txt")
    );

    subset_flag = SubsetFlags::SUBSET_FLAGS_DEFAULT;

    parse_profile_options(
        &subset_test.profiles[2],
        &mut subset_flag,
        &mut name_ids,
        &mut gids,
        &mut unicodes,
        &mut layout_features,
        &mut layout_scripts,
        &mut drop_tables,
        &mut name_languages,
    );
    assert_eq!(
        subset_flag,
        SubsetFlags::SUBSET_FLAGS_NO_HINTING | SubsetFlags::SUBSET_FLAGS_RETAIN_GIDS
    );
    assert!(name_ids.is_empty());
    assert!(unicodes.is_empty());
    assert!(name_languages.is_empty());
    assert!(gids.is_empty());
    assert!(layout_features.is_empty());
    assert!(layout_scripts.is_empty());
    assert!(drop_tables.is_empty());

    // parse notdef-outline.txt
    assert_eq!(subset_test.profiles[4], String::from("notdef-outline.txt"));
    subset_flag = SubsetFlags::SUBSET_FLAGS_DEFAULT;

    parse_profile_options(
        &subset_test.profiles[4],
        &mut subset_flag,
        &mut name_ids,
        &mut gids,
        &mut unicodes,
        &mut layout_features,
        &mut layout_scripts,
        &mut drop_tables,
        &mut name_languages,
    );
    assert_eq!(subset_flag, SubsetFlags::SUBSET_FLAGS_NOTDEF_OUTLINE);

    // parse name-ids.txt
    assert_eq!(subset_test.profiles[5], String::from("name-ids.txt"));
    subset_flag = SubsetFlags::SUBSET_FLAGS_DEFAULT;

    parse_profile_options(
        &subset_test.profiles[5],
        &mut subset_flag,
        &mut name_ids,
        &mut gids,
        &mut unicodes,
        &mut layout_features,
        &mut layout_scripts,
        &mut drop_tables,
        &mut name_languages,
    );
    assert_eq!(subset_flag, SubsetFlags::SUBSET_FLAGS_DEFAULT);
    assert_eq!(name_ids.len(), 3);
    assert!(name_ids.contains(NameId::new(0)));
    assert!(name_ids.contains(NameId::new(1)));
    assert!(name_ids.contains(NameId::new(2)));
    assert!(unicodes.is_empty());
    assert!(name_languages.is_empty());
    assert!(gids.is_empty());
    assert!(layout_features.is_empty());
    assert!(layout_scripts.is_empty());
    assert!(drop_tables.is_empty());

    // parse name-languages.txt
    assert_eq!(subset_test.profiles[6], String::from("name-languages.txt"));
    subset_flag = SubsetFlags::SUBSET_FLAGS_DEFAULT;
    name_ids.clear();

    parse_profile_options(
        &subset_test.profiles[6],
        &mut subset_flag,
        &mut name_ids,
        &mut gids,
        &mut unicodes,
        &mut layout_features,
        &mut layout_scripts,
        &mut drop_tables,
        &mut name_languages,
    );
    assert_eq!(subset_flag, SubsetFlags::SUBSET_FLAGS_DEFAULT);
    assert!(name_languages.contains(1));
    assert!(name_languages.contains(2));
    assert!(name_languages.contains(3));
    assert!(name_ids.is_empty());
    assert!(unicodes.is_empty());
    assert!(gids.is_empty());
    assert!(layout_features.is_empty());
    assert!(layout_scripts.is_empty());
    assert!(drop_tables.is_empty());

    // parse name-legacy
    assert_eq!(subset_test.profiles[7], String::from("name-legacy.txt"));
    subset_flag = SubsetFlags::SUBSET_FLAGS_DEFAULT;
    name_languages.clear();

    parse_profile_options(
        &subset_test.profiles[7],
        &mut subset_flag,
        &mut name_ids,
        &mut gids,
        &mut unicodes,
        &mut layout_features,
        &mut layout_scripts,
        &mut drop_tables,
        &mut name_languages,
    );
    assert_eq!(subset_flag, SubsetFlags::SUBSET_FLAGS_NAME_LEGACY);
    assert!(name_ids.is_empty());
    assert!(unicodes.is_empty());
    assert!(gids.is_empty());
    assert!(layout_features.is_empty());
    assert!(layout_scripts.is_empty());
    assert!(drop_tables.is_empty());
    assert!(name_languages.is_empty());

    // parse gids.txt
    assert_eq!(subset_test.profiles[8], String::from("gids.txt"));
    subset_flag = SubsetFlags::SUBSET_FLAGS_DEFAULT;

    parse_profile_options(
        &subset_test.profiles[8],
        &mut subset_flag,
        &mut name_ids,
        &mut gids,
        &mut unicodes,
        &mut layout_features,
        &mut layout_scripts,
        &mut drop_tables,
        &mut name_languages,
    );
    assert_eq!(subset_flag, SubsetFlags::SUBSET_FLAGS_DEFAULT);
    assert_eq!(gids.len(), 3);
    assert!(gids.contains(GlyphId::new(1)));
    assert!(gids.contains(GlyphId::new(2)));
    assert!(gids.contains(GlyphId::new(3)));

    // parse layout-features.txt
    assert_eq!(subset_test.profiles[9], String::from("layout-features.txt"));
    subset_flag = SubsetFlags::SUBSET_FLAGS_DEFAULT;
    gids.clear();

    parse_profile_options(
        &subset_test.profiles[9],
        &mut subset_flag,
        &mut name_ids,
        &mut gids,
        &mut unicodes,
        &mut layout_features,
        &mut layout_scripts,
        &mut drop_tables,
        &mut name_languages,
    );
    assert_eq!(layout_features.len(), 3);
    assert!(layout_features.contains(Tag::new(b"kern")));
    assert!(layout_features.contains(Tag::new(b"mark")));
    assert!(layout_features.contains(Tag::new(b"liga")));

    // parse keep-all-layout-features.txt
    assert_eq!(
        subset_test.profiles[10],
        String::from("keep-all-layout-features.txt")
    );
    layout_features.clear();

    parse_profile_options(
        &subset_test.profiles[10],
        &mut subset_flag,
        &mut name_ids,
        &mut gids,
        &mut unicodes,
        &mut layout_features,
        &mut layout_scripts,
        &mut drop_tables,
        &mut name_languages,
    );
    assert!(layout_features.is_inverted());

    // parse retain-gids-glyph-names
    assert_eq!(
        subset_test.profiles[13],
        String::from("retain-gids-glyph-names.txt")
    );
    subset_flag = SubsetFlags::SUBSET_FLAGS_DEFAULT;

    parse_profile_options(
        &subset_test.profiles[13],
        &mut subset_flag,
        &mut name_ids,
        &mut gids,
        &mut unicodes,
        &mut layout_features,
        &mut layout_scripts,
        &mut drop_tables,
        &mut name_languages,
    );
    assert_eq!(
        subset_flag,
        SubsetFlags::SUBSET_FLAGS_RETAIN_GIDS | SubsetFlags::SUBSET_FLAGS_GLYPH_NAMES
    );
    assert_eq!(subset_test.subsets.len(), 3);
    assert_eq!(subset_test.subsets[1], "61,62,63");
}
