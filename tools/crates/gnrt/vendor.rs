// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::config;
use crate::crates;
use crate::inherit::{
    find_inherited_privilege_group, find_inherited_security_critical_flag,
    find_inherited_shipped_flag,
};
use crate::paths;
use crate::readme;
use crate::util::{
    create_dirs_if_needed, init_handlebars, remove_checksums_from_lock, run_cargo_metadata,
    without_cargo_config_toml,
};
use anyhow::{format_err, Context, Result};
use std::collections::{HashMap, HashSet};
use std::path::PathBuf;

pub fn vendor(
    args: &clap::ArgMatches,
    tools: &paths::ToolPaths,
    paths: &paths::ChromiumPaths,
) -> Result<()> {
    // Vendoring needs to work with real crates.io, not with our locally vendored
    // crates.
    without_cargo_config_toml(paths, || vendor_impl(args, tools, paths))?;
    println!("Vendor successful: run gnrt gen to generate GN rules.");
    Ok(())
}

fn vendor_impl(
    args: &clap::ArgMatches,
    tools: &paths::ToolPaths,
    paths: &paths::ChromiumPaths,
) -> Result<()> {
    let config_file_path = paths.third_party_config_file;
    let config_file_contents = std::fs::read_to_string(config_file_path).unwrap();
    let config: config::BuildConfig = toml::de::from_str(&config_file_contents).unwrap();

    let template_path = paths
        .third_party_config_file
        .parent()
        .unwrap()
        .join(&config.gn_config.readme_file_template);
    let handlebars = init_handlebars(&template_path).context("init_handlebars")?;

    println!("Vendoring crates from {}", paths.third_party_cargo_root.display());

    let metadata =
        run_cargo_metadata(paths.third_party_cargo_root.into(), tools, Vec::new(), HashMap::new())
            .context("run_cargo_metadata")?;

    // Running cargo commands against actual crates.io will put checksum into
    // the Cargo.lock file, but we don't generate checksums when we download
    // the crates. This mismatch causes everything else to fail when cargo is
    // using our vendor/ directory. So we remove all the checksums from the
    // lock file.
    remove_checksums_from_lock(paths.third_party_cargo_root)?;

    // Collect the set of third-party crates.
    let packages = {
        let packages: HashMap<_, _> = metadata
            .packages
            .iter()
            .filter(|package| {
                // Remove the root package (our "chromium" crate).
                //
                // We have to keep packages in the `remove_crates` config
                // because they must be downloaded for `cargo metadata` to work.
                metadata.root_package().unwrap().id != package.id
            })
            // Key off the package id.
            .map(|p| (&p.id, p))
            .collect();

        // If there are multiple crates with the same epoch, this is unexpected.
        // Bail out.
        {
            let mut found = HashSet::new();
            for (_, p) in &packages {
                let epoch = crates::Epoch::from_version(&p.version);
                if found.insert((&p.name, epoch)) == false {
                    return Err(format_err!(
                        "Two '{}' crates found with the same {} epoch",
                        p.name,
                        epoch
                    ));
                }
            }
        }

        packages
    };
    let nodes: HashMap<_, _> =
        metadata.resolve.as_ref().unwrap().nodes.iter().map(|node| (&node.id, node)).collect();

    {
        let package_names = packages.values().map(|p| &p.name).collect::<HashSet<_>>();
        for (name, _) in &config.per_crate_config {
            if !package_names.contains(name) {
                return Err(format_err!(
                    "Config found for crate {name}, but it is not a dependency, in {file}",
                    name = name,
                    file = config_file_path.display()
                ));
            }
        }
    }

    // Download missing dirs, remove the rest.
    let vendor_dir = paths.third_party_cargo_root.join("vendor");
    create_dirs_if_needed(&vendor_dir).context("creating vendor dir")?;
    let mut dirs: HashSet<String> = std::fs::read_dir(&vendor_dir)
        .context("reading vendor dir")?
        .filter_map(|dir| {
            if let Ok(entry) = dir {
                if entry.metadata().map(|m| m.is_dir()).unwrap_or(false) {
                    return Some(entry.file_name().to_string_lossy().to_string());
                }
            }
            None
        })
        .collect();
    for (_, p) in &packages {
        if !dirs.contains(&format!("{}-{}", p.name, p.version)) {
            println!("Downloading {}-{}", p.name, p.version);
            download_crate(&p.name, &p.version, paths)?
        }
        dirs.remove(&format!("{}-{}", p.name, p.version));
    }
    for d in &dirs {
        println!("Deleting {}", d);
        std::fs::remove_dir_all(paths.third_party_cargo_root.join("vendor").join(d))
            .with_context(|| format!("removing {}", d))?
    }

    let root = metadata.resolve.as_ref().unwrap().root.as_ref().unwrap();
    let find_group = |id| find_inherited_privilege_group(id, root, &packages, &nodes, &config);
    let find_security_critical =
        |id| find_inherited_security_critical_flag(id, root, &packages, &nodes, &config);
    let find_shipped = |id| find_inherited_shipped_flag(id, root, &packages, &nodes, &config);

    let all_readme_files: HashMap<PathBuf, readme::ReadmeFile> =
        readme::readme_files_from_packages(
            packages
                .values()
                // Don't generate README files for packages skipped by `remove_crates`.
                .filter(|p| config.resolve.remove_crates.iter().all(|r| *r != p.name))
                .copied(),
            paths,
            &config,
            find_group,
            find_security_critical,
            find_shipped,
        )?;

    for (dir, _) in &all_readme_files {
        create_dirs_if_needed(dir).context(format!("dir: {}", dir.display()))?;
    }

    if args.get_flag("dump-template-input") {
        for (dir, readme_file) in &all_readme_files {
            serde_json::to_writer_pretty(
                std::fs::File::create(dir.join("gnrt-template-input.json"))
                    .context("opening dump file")?,
                &readme_file,
            )
            .context("dumping gn information")?;
        }
        return Ok(());
    }

    for (dir, readme_file) in &all_readme_files {
        let readme_str = handlebars.render("template", &readme_file)?;

        let file_path = dir.join("README.chromium");
        let file = std::fs::File::create(&file_path).with_context(|| {
            format!("Could not create README.chromium output file {}", file_path.to_string_lossy())
        })?;
        use std::io::Write;
        write!(std::io::BufWriter::new(file), "{}", readme_str)
            .with_context(|| format!("{}", file_path.to_string_lossy()))?;
    }
    Ok(())
}

fn download_crate(
    name: &str,
    version: &semver::Version,
    paths: &paths::ChromiumPaths,
) -> Result<()> {
    let mut response = reqwest::blocking::get(format!(
        "https://crates.io/api/v1/crates/{name}/{version}/download",
    ))?;
    if response.status() != 200 {
        return Err(format_err!("Failed to download crate {}: {}", name, response.status()));
    }
    let num_bytes = {
        let header = response.headers().get(reqwest::header::CONTENT_LENGTH);
        if let Some(value) = header { value.to_str()?.parse::<usize>()? } else { 0 }
    };
    let mut bytes = Vec::with_capacity(num_bytes);
    {
        use std::io::Read;
        response
            .read_to_end(&mut bytes)
            .with_context(|| format!("reading http response for crate {}", name))?;
    }
    let unzipped = flate2::read::GzDecoder::new(bytes.as_slice());
    let mut archive = tar::Archive::new(unzipped);

    let vendor_dir = paths.third_party_cargo_root.join("vendor");
    let crate_dir = vendor_dir.join(format!("{}-{}", name, version));

    if let Err(e) = archive.unpack(vendor_dir) {
        std::fs::remove_dir_all(crate_dir)
            .with_context(|| format!("Deleting failed unpack of crate {}-{}", name, version))?;
        return Err(e)
            .with_context(|| format!("Failed to unpack crate {}-{}", name, version))
            .into();
    }

    std::fs::write(crate_dir.join(".cargo-checksum.json"), "{\"files\":{}}\n")
        .with_context(|| format!("writing .cargo-checksum.json for crate {}", name))?;

    Ok(())
}
