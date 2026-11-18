//! CLI text and Unicode selectors share one ordered selection set.
#![cfg(feature = "cli")]

use std::process::Command;

fn cli_subset(options: &[&str]) -> Vec<u8> {
    let directory = tempfile::tempdir().unwrap();
    let output = directory.path().join("subset.ttf");
    let result = Command::new(env!("CARGO_BIN_EXE_skera"))
        .args([
            "--path",
            "test-data/fonts/Roboto-Regular.ttf",
            "--output-file",
        ])
        .arg(&output)
        .args(options)
        .output()
        .unwrap();
    assert!(
        result.status.success(),
        "{}",
        String::from_utf8_lossy(&result.stderr)
    );
    std::fs::read(output).unwrap()
}

#[test]
fn text_selectors_match_unicode_lists_and_follow_command_line_order() {
    for (options, expected) in [
        (vec!["--text=ABé"], vec!["--unicodes=41,42,E9"]),
        (vec!["--unicodes=41,42", "--text=C"], vec!["--unicodes=43"]),
        (
            vec!["--text=A", "--unicodes=42,43"],
            vec!["--unicodes=42,43"],
        ),
        (vec!["--text=A", "--text=B"], vec!["--unicodes=42"]),
        (
            vec!["--text=AB", "--unicodes+=43", "--text-=B"],
            vec!["--unicodes=41,43"],
        ),
        (vec!["--unicodes=41", "--text+=é"], vec!["--unicodes=41,E9"]),
        (
            vec!["--keep-everything", "--text=AB"],
            vec!["--keep-everything", "--unicodes=41,42"],
        ),
        (vec!["--text="], vec!["--unicodes="]),
    ] {
        assert_eq!(cli_subset(&options), cli_subset(&expected));
    }
}
