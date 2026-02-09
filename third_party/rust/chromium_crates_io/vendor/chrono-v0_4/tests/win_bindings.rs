use std::fs;
use windows_bindgen::bindgen;

#[test]
fn gen_bindings() {
    let existing = fs::read_to_string(BINDINGS).unwrap();

    bindgen([
        "--out",
        BINDINGS,
        "--flat",
        "--no-comment",
        "--no-deps",
        "--sys",
        "--filter",
        "GetTimeZoneInformationForYear",
        "SystemTimeToFileTime",
        "SystemTimeToTzSpecificLocalTime",
        "TzSpecificLocalTimeToSystemTime",
    ])
    .unwrap();

    // Check the output is the same as before.
    // Depending on the git configuration the file may have been checked out with `\r\n` newlines or
    // with `\n`. Compare line-by-line to ignore this difference.
    let mut new = fs::read_to_string(BINDINGS).unwrap();
    if existing.contains("\r\n") && !new.contains("\r\n") {
        new = new.replace("\n", "\r\n");
    } else if !existing.contains("\r\n") && new.contains("\r\n") {
        new = new.replace("\r\n", "\n");
    }

    similar_asserts::assert_eq!(existing, new);
    if !new.lines().eq(existing.lines()) {
        panic!("generated file `{BINDINGS}` is changed.");
    }
}

const BINDINGS: &str = "src/offset/local/win_bindings.rs";
