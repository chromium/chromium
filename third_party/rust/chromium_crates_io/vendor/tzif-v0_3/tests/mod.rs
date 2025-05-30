// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use std::path::Path;
use walkdir::WalkDir;

fn parse_tzif_file(path: &Path) -> Result<(), tzif::error::Error> {
    println!("parsing {:?}", path);
    let parsed = tzif::parse_tzif_file(path)?;
    println!("{parsed:#?}");
    Ok(())
}

#[test]
fn parse_tzif_testdata() -> Result<(), tzif::error::Error> {
    for entry in WalkDir::new("testdata").follow_links(true) {
        let entry = entry.unwrap();
        if entry.file_type().is_file() {
            parse_tzif_file(entry.path())?
        }
    }
    Ok(())
}

#[test]
fn parse_posix_tz_string() {
    assert!(tzif::parse_posix_tz_string(b"WGT3WGST,M3.5.0/-2,M10.5.0/-1").is_ok());
}
