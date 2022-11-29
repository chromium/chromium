// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::fs;
use std::io::{Read, Write};
use std::process::{self, ExitCode};

use crate::crates;
use crate::manifest::CargoManifest;
use crate::paths;

/// Runs the download subcommand, which downloads a crate from crates.io and
/// unpacks it into the Chromium tree.
pub fn download(
    name: &str,
    version: semver::Version,
    security: bool,
    paths: &paths::ChromiumPaths,
) -> ExitCode {
    let vendored_crate = crates::ChromiumVendoredCrate {
        name: name.to_string(),
        epoch: crates::Epoch::from_version(&version),
    };
    let build_path = paths.third_party.join(vendored_crate.build_path());
    let crate_path = paths.third_party.join(vendored_crate.crate_path());

    let url = format!(
        "{dir}/{name}/{name}-{version}.{suffix}",
        dir = CRATES_IO_DOWNLOAD_URL,
        suffix = CRATES_IO_DOWNLOAD_SUFFIX
    );
    let curl_out = process::Command::new("curl")
        .arg("--fail")
        .arg(url.to_string())
        .output()
        .expect("Failed to run curl");
    if !curl_out.status.success() {
        eprintln!("gnrt: {}", String::from_utf8(curl_out.stderr).unwrap());
        return ExitCode::FAILURE;
    }

    // Makes the directory where the build file will go. The crate's source code
    // will go below it. This directory and its parents are allowed to exist
    // already.
    std::fs::create_dir_all(&build_path)
        .expect("Could not make the third-party directory '{build_path}' for the crate");
    // Makes the directory where the source code will be unzipped. It should not
    // exist or we'd be clobbering existing files.
    std::fs::create_dir(&crate_path).expect("Crate directory '{crate_path}' already exists");

    let mut untar = process::Command::new("tar")
        // Extract and unzip from stdin.
        .arg("xzf")
        .arg("-")
        // Drop the first path component, which is the crate's name-version.
        .arg("--strip-components=1")
        // Unzip into the crate's directory in third_party/rust.
        .arg(format!("--directory={}", crate_path.display()))
        // The input is the downloaded file.
        .stdin(process::Stdio::piped())
        .stdout(process::Stdio::piped())
        .stderr(process::Stdio::piped())
        .spawn()
        .expect("Failed to run tar");

    if untar.stdin.take().unwrap().write_all(&curl_out.stdout).is_err() {
        eprintln!("gnrt: Failed to pipe input to tar, it exited early");
    }

    if !untar.wait().expect("Failed to wait for tar").success() {
        let mut stderr_buf = Vec::new();
        untar.stderr.unwrap().read_to_end(&mut stderr_buf).expect("Failed to read stderr from tar");
        eprintln!("gnrt: {}", String::from_utf8(stderr_buf).unwrap());
        return ExitCode::FAILURE;
    }

    let cargo: CargoManifest = {
        let str = std::fs::read_to_string(crate_path.join("Cargo.toml"))
            .expect("Unable to open downloaded Cargo.toml");
        toml::de::from_str(&str).expect("Unable to parse downloaded Cargo.toml")
    };

    let (_, readme_license) = ALLOWED_LICENSES
        .iter()
        .find(|(allowed_license, _)| &cargo.package.license == *allowed_license)
        .expect("License in downloaded Cargo.toml is not in ALLOWED_LICENSES");

    let vcs_path = crate_path.join(".cargo_vcs_info.json");
    let vcs_contents = match fs::read_to_string(&vcs_path) {
        Ok(s) => serde_json::from_str(&s).unwrap(),
        Err(_) => None,
    };
    let githash: Option<&str> = vcs_contents.as_ref().and_then(|v| {
        use serde_json::Value::*;
        match v {
            Object(map) => match map.get("git") {
                Some(Object(map)) => match map.get("sha1") {
                    Some(String(s)) => Some(&s[..]),
                    _ => None,
                },
                _ => None,
            },
            _ => None,
        }
    });

    let readme = gen_readme_chromium_text(&cargo, readme_license, githash, security);
    std::fs::write(build_path.join("README.chromium"), readme)
        .expect("Failed to write README.chromium");

    println!("gnrt: Downloaded {name} {version} to {path}", path = crate_path.display());

    ExitCode::SUCCESS
}

/// Generate the contents of the README.chromium file.
fn gen_readme_chromium_text(
    manifest: &CargoManifest,
    license: &str,
    githash: Option<&str>,
    security: bool,
) -> String {
    let security = if security { "yes" } else { "no" };

    let revision = githash.map_or_else(String::new, |s| format!("Revision: {s}\n"));

    format!(
        "Name: {crate_name}\n\
         URL: {url}\n\
         Description: {description}\n\
         Version: {version}\n\
         Security Critical: {security}\n\
         License: {license}\n\
         {revision}",
        crate_name = manifest.package.name,
        url = format!("{}/{}", CRATES_IO_VIEW_URL, manifest.package.name),
        description = manifest.package.description.as_ref().unwrap_or(&"".to_string()),
        version = manifest.package.version,
    )
}

static CRATES_IO_DOWNLOAD_URL: &str = "https://static.crates.io/crates";
static CRATES_IO_DOWNLOAD_SUFFIX: &str = "crate";
static CRATES_IO_VIEW_URL: &str = "https://crates.io/crates";

// Allowed licenses, in the format they are specified in Cargo.toml files from
// crates.io, and the format to write to README.chromium.
static ALLOWED_LICENSES: [(&str, &str); 15] = [
    // ("Cargo.toml string", "License for README.chromium")
    ("Apache-2.0", "Apache 2.0"),
    ("MIT OR Apache-2.0", "Apache 2.0"),
    ("MIT/Apache-2.0", "Apache 2.0"),
    ("Apache-2.0 / MIT", "Apache 2.0"),
    ("Apache-2.0 OR MIT", "Apache 2.0"),
    ("Apache-2.0/MIT", "Apache 2.0"),
    ("MIT", "MIT"),
    ("Unlicense OR MIT", "MIT"),
    ("Unlicense/MIT", "MIT"),
    ("Apache-2.0 OR BSL-1.0", "Apache 2.0"),
    ("BSD-3-Clause", "BSD 3-Clause"),
    ("ISC", "ISC"),
    ("MIT OR Zlib OR Apache-2.0", "Apache 2.0"),
    ("0BSD OR MIT OR Apache-2.0", "Apache 2.0"),
    ("Unicode-DFS-2016", "Unicode License Agreement - Data Files and Software (2016)"),
];
