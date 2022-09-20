#![allow(clippy::manual_assert)]

mod progress;

use self::progress::Progress;
use anyhow::Result;
use flate2::read::GzDecoder;
use std::fs;
use std::path::Path;
use tar::Archive;
use walkdir::DirEntry;

const REVISION: &str = "ee160f2f5e73b6f5954bc33f059c316d9e8582c4";

#[rustfmt::skip]
static EXCLUDE: &[&str] = &[
    // TODO: impl ~const T {}
    // https://github.com/dtolnay/syn/issues/1051
    "src/test/ui/rfc-2632-const-trait-impl/syntax.rs",

    // Compile-fail expr parameter in const generic position: f::<1 + 2>()
    "src/test/ui/const-generics/early/closing-args-token.rs",
    "src/test/ui/const-generics/early/const-expression-parameter.rs",

    // Need at least one trait in impl Trait, no such type as impl 'static
    "src/test/ui/type-alias-impl-trait/generic_type_does_not_live_long_enough.rs",

    // Deprecated anonymous parameter syntax in traits
    "src/test/ui/issues/issue-13105.rs",
    "src/test/ui/issues/issue-13775.rs",
    "src/test/ui/issues/issue-34074.rs",
    "src/test/ui/proc-macro/trait-fn-args-2015.rs",
    "src/tools/rustfmt/tests/source/trait.rs",
    "src/tools/rustfmt/tests/target/trait.rs",

    // Placeholder syntax for "throw expressions"
    "src/test/pretty/yeet-expr.rs",
    "src/test/ui/try-trait/yeet-for-option.rs",
    "src/test/ui/try-trait/yeet-for-result.rs",

    // Excessive nesting
    "src/test/ui/issues/issue-74564-if-expr-stack-overflow.rs",

    // Testing rustfmt on invalid syntax
    "src/tools/rustfmt/tests/coverage/target/comments.rs",
    "src/tools/rustfmt/tests/parser/issue-4126/invalid.rs",
    "src/tools/rustfmt/tests/parser/issue_4418.rs",
    "src/tools/rustfmt/tests/parser/unclosed-delims/issue_4466.rs",
    "src/tools/rustfmt/tests/source/configs/disable_all_formatting/true.rs",
    "src/tools/rustfmt/tests/source/configs/spaces_around_ranges/false.rs",
    "src/tools/rustfmt/tests/source/configs/spaces_around_ranges/true.rs",
    "src/tools/rustfmt/tests/source/type.rs",
    "src/tools/rustfmt/tests/target/configs/spaces_around_ranges/false.rs",
    "src/tools/rustfmt/tests/target/configs/spaces_around_ranges/true.rs",
    "src/tools/rustfmt/tests/target/type.rs",

    // Testing compiler diagnostic localization on invalid syntax
    "src/test/run-make/translation/basic-translation.rs",

    // Clippy lint lists represented as expressions
    "src/tools/clippy/clippy_lints/src/lib.deprecated.rs",
    "src/tools/clippy/clippy_lints/src/lib.register_all.rs",
    "src/tools/clippy/clippy_lints/src/lib.register_cargo.rs",
    "src/tools/clippy/clippy_lints/src/lib.register_complexity.rs",
    "src/tools/clippy/clippy_lints/src/lib.register_correctness.rs",
    "src/tools/clippy/clippy_lints/src/lib.register_internal.rs",
    "src/tools/clippy/clippy_lints/src/lib.register_lints.rs",
    "src/tools/clippy/clippy_lints/src/lib.register_nursery.rs",
    "src/tools/clippy/clippy_lints/src/lib.register_pedantic.rs",
    "src/tools/clippy/clippy_lints/src/lib.register_perf.rs",
    "src/tools/clippy/clippy_lints/src/lib.register_restriction.rs",
    "src/tools/clippy/clippy_lints/src/lib.register_style.rs",
    "src/tools/clippy/clippy_lints/src/lib.register_suspicious.rs",

    // Not actually test cases
    "src/test/rustdoc-ui/test-compile-fail2.rs",
    "src/test/rustdoc-ui/test-compile-fail3.rs",
    "src/test/ui/json-bom-plus-crlf-multifile-aux.rs",
    "src/test/ui/lint/expansion-time-include.rs",
    "src/test/ui/macros/auxiliary/macro-comma-support.rs",
    "src/test/ui/macros/auxiliary/macro-include-items-expr.rs",
    "src/test/ui/macros/include-single-expr-helper.rs",
    "src/test/ui/macros/include-single-expr-helper-1.rs",
    "src/test/ui/parser/issues/auxiliary/issue-21146-inc.rs",
];

pub fn base_dir_filter(entry: &DirEntry) -> bool {
    let path = entry.path();
    if path.is_dir() {
        return true; // otherwise walkdir does not visit the files
    }
    if path.extension().map_or(true, |e| e != "rs") {
        return false;
    }

    let mut path_string = path.to_string_lossy();
    if cfg!(windows) {
        path_string = path_string.replace('\\', "/").into();
    }
    let path = if let Some(path) = path_string.strip_prefix("tests/rust/") {
        path
    } else {
        panic!("unexpected path in Rust dist: {}", path_string);
    };

    if path.starts_with("src/test/compile-fail") || path.starts_with("src/test/rustfix") {
        return false;
    }

    if path.starts_with("src/test/ui") {
        let stderr_path = entry.path().with_extension("stderr");
        if stderr_path.exists() {
            // Expected to fail in some way
            return false;
        }
    }

    !EXCLUDE.contains(&path)
}

#[allow(dead_code)]
pub fn edition(path: &Path) -> &'static str {
    if path.ends_with("dyn-2015-no-warnings-without-lints.rs") {
        "2015"
    } else {
        "2018"
    }
}

pub fn clone_rust() {
    let needs_clone = match fs::read_to_string("tests/rust/COMMIT") {
        Err(_) => true,
        Ok(contents) => contents.trim() != REVISION,
    };
    if needs_clone {
        download_and_unpack().unwrap();
    }
    let mut missing = String::new();
    let test_src = Path::new("tests/rust");
    for exclude in EXCLUDE {
        if !test_src.join(exclude).exists() {
            missing += "\ntests/rust/";
            missing += exclude;
        }
    }
    if !missing.is_empty() {
        panic!("excluded test file does not exist:{}\n", missing);
    }
}

fn download_and_unpack() -> Result<()> {
    let url = format!(
        "https://github.com/rust-lang/rust/archive/{}.tar.gz",
        REVISION
    );
    let response = reqwest::blocking::get(&url)?.error_for_status()?;
    let progress = Progress::new(response);
    let decoder = GzDecoder::new(progress);
    let mut archive = Archive::new(decoder);
    let prefix = format!("rust-{}", REVISION);

    let tests_rust = Path::new("tests/rust");
    if tests_rust.exists() {
        fs::remove_dir_all(tests_rust)?;
    }

    for entry in archive.entries()? {
        let mut entry = entry?;
        let path = entry.path()?;
        if path == Path::new("pax_global_header") {
            continue;
        }
        let relative = path.strip_prefix(&prefix)?;
        let out = tests_rust.join(relative);
        entry.unpack(&out)?;
    }

    fs::write("tests/rust/COMMIT", REVISION)?;
    Ok(())
}
