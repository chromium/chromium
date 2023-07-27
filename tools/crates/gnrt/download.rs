// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::crates::{self, ThirdPartySource};
use crate::manifest::CargoManifest;
use crate::paths;
use crate::util::{check_exit_ok, check_output, check_spawn, check_wait_with_output};

use std::fs;
use std::io::Write;
use std::process;

use anyhow::{Context, Result};

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum SecurityCritical {
    Yes,
    No,
}
impl From<bool> for SecurityCritical {
    fn from(b: bool) -> Self {
        if b { Self::Yes } else { Self::No }
    }
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum Shipped {
    Yes,
    No,
}
impl From<bool> for Shipped {
    fn from(b: bool) -> Self {
        if b { Self::Yes } else { Self::No }
    }
}

/// Runs the download subcommand, which downloads a crate from crates.io and
/// unpacks it into the Chromium tree.
pub fn download(
    name: &str,
    version: semver::Version,
    security: SecurityCritical,
    shipped: Shipped,
    paths: &paths::ChromiumPaths,
) -> Result<()> {
    let vendored_crate = crates::VendoredCrate { name: name.into(), version: version.clone() };
    let build_path = paths.third_party.join(ThirdPartySource::build_path(&vendored_crate));
    let crate_path = paths.third_party.join(ThirdPartySource::crate_path(&vendored_crate));

    let url =
        format!("{CRATES_IO_DOWNLOAD_URL}/{name}/{name}-{version}.{CRATES_IO_DOWNLOAD_SUFFIX}");
    let curl_out = check_output(process::Command::new("curl").arg("--fail").arg(&url), "curl")?;
    check_exit_ok(&curl_out, "curl")?;

    // Makes the directory where the build file will go. The crate's source code
    // will go below it. This directory and its parents are allowed to exist
    // already.
    std::fs::create_dir_all(&build_path)
        .expect("Could not make the third-party directory '{build_path}' for the crate");
    // Makes the directory where the source code will be unzipped. It should not
    // exist or we'd be clobbering existing files.
    std::fs::create_dir(&crate_path).expect("Crate directory '{crate_path}' already exists");

    let mut untar = check_spawn(
        process::Command::new("tar")
            // Extract and unzip from stdin.
            .arg("xzf")
            .arg("-")
            // Drop the first path component, which is the crate's name-version.
            .arg("--strip-components=1")
            // Unzip into the crate's directory in third_party/rust.
            .arg(format!("--directory={}", crate_path.display()))
            // The input is the downloaded file.
            .stdin(process::Stdio::piped())
            .stderr(process::Stdio::piped()),
        "tar",
    )?;

    untar
        .stdin
        .take()
        .unwrap()
        .write_all(&curl_out.stdout)
        .context("Failed to pipe crate archive to tar")?;
    {
        let untar_output = check_wait_with_output(untar, "tar")?;
        check_exit_ok(&untar_output, "tar")?;
    }

    let cargo: CargoManifest = {
        let str = std::fs::read_to_string(crate_path.join("Cargo.toml"))
            .expect("Unable to open downloaded Cargo.toml");
        toml::de::from_str(&str).expect("Unable to parse downloaded Cargo.toml")
    };

    let (_, readme_license) = ALLOWED_LICENSES
        .iter()
        .find(|(allowed_license, _)| cargo.package.license == *allowed_license)
        .expect("License in downloaded Cargo.toml is not in ALLOWED_LICENSES");

    let vcs_path = crate_path.join(".cargo_vcs_info.json");
    let vcs_contents = match fs::read_to_string(vcs_path) {
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

    let readme = gen_readme_chromium_text(&cargo, readme_license, githash, security, shipped);
    std::fs::write(build_path.join("README.chromium"), readme)
        .expect("Failed to write README.chromium");

    println!("gnrt: Downloaded {name} {version} to {path}", path = crate_path.display());

    Ok(())
}

/// Generate the contents of the README.chromium file.
fn gen_readme_chromium_text(
    manifest: &CargoManifest,
    license: &str,
    githash: Option<&str>,
    security: SecurityCritical,
    shipped: Shipped,
) -> String {
    let security = if security == SecurityCritical::Yes { "yes" } else { "no" };
    let shipped = if shipped == Shipped::Yes { "yes" } else { "no" };

    let revision = githash.map_or_else(String::new, |s| format!("Revision: {s}\n"));

    format!(
        "Name: {crate_name}\n\
         URL: {CRATES_IO_VIEW_URL}/{package_name}\n\
         Description: {description}\n\
         Version: {version}\n\
         Security Critical: {security}\n\
         Shipped: {shipped}\n\
         License: {license}\n\
         {revision}",
        crate_name = manifest.package.name,
        package_name = manifest.package.name,
        description = manifest.package.description.as_ref().unwrap_or(&"".to_string()),
        version = manifest.package.version,
    )
}

static CRATES_IO_DOWNLOAD_URL: &str = "https://static.crates.io/crates";
static CRATES_IO_DOWNLOAD_SUFFIX: &str = "crate";
static CRATES_IO_VIEW_URL: &str = "https://crates.io/crates";

// Allowed licenses, in the format they are specified in Cargo.toml files from
// crates.io, and the format to write to README.chromium.
static ALLOWED_LICENSES: [(&str, &str); 16] = [
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
    ("Zlib OR Apache-2.0 OR MIT", "Apache 2.0"),
    ("0BSD OR MIT OR Apache-2.0", "Apache 2.0"),
    (
        "(MIT OR Apache-2.0) AND Unicode-DFS-2016",
        "Apache 2.0 AND Unicode License Agreement - Data Files and Software (2016)",
    ),
];
